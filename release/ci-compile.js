#!/usr/bin/env node

const { run, log, inDir, getOutput, writeFile } = require("./common");
// explicitly ask for the posix version of path manipulation
// routines, because we run our windows worker in MSYS2
const posixPath = require("path").posix;

let config = undefined

async function ci_compile(args) {
  const [os, arch] = args;

  if (!os) { throw new Error(`missing os`); }
  if (["linux", "windows", "darwin"].indexOf(os) === -1) { throw new Error(`unknown os '${os}'`); }

  if (!arch) { throw new Error(`missing arch`); }
  if (["386", "amd64", "arm64"].indexOf(arch) === -1) { throw new Error(`unknown arch '${arch}'`); }

  const osarch = `${os}-${arch}`;
  log(`compiling libc7zip for ${osarch}`);

  let binDir = `./broth/${osarch}`;
  await run(`mkdir -p ${binDir}`);

  config = { os, arch, osarch, binDir, artifacts: [] };

  await buildLib();
  await buildUpstream();

  for (const artifact of config.artifacts) {
    await run(`cp -f ${artifact} ${binDir}/`);
  }

  let artifactNames = config.artifacts.map((v) => posixPath.basename(v));

  log(`artifacts for ${osarch}: `);
  for (const artifactName of artifactNames) {
    log(` - ${artifactName}: ${getOutput(`file ${binDir}/${artifactName}`)}`);
  }
}

function libname() {
  switch (config.os) {
    case "linux":
      return "libc7zip.so";
    case "darwin":
      return "libc7zip.dylib";
    case "windows":
      return "c7zip.dll";
  }
  throw new Error(`unknown os ${config.os}`);
}

async function buildLib() {
  let buildDir = `./build/${config.osarch}`;
  await run(`rm -rf ${buildDir}`);
  await run(`mkdir -p ${buildDir}`);

  let extraCMakeFlags = ""
  if (config.os === "windows") {
    // Let cmake pick the newest installed Visual Studio (runner images move
    // between VS versions); only the target architecture is pinned
    const archMap = { "386": "Win32", "amd64": "x64", "arm64": "ARM64" };
    const arch = archMap[config.arch];
    extraCMakeFlags = `-A ${arch}`;
  }

  await inDir(buildDir, async () => {
    await run(`cmake ${extraCMakeFlags} -DCMAKE_BUILD_TYPE=Release ../..`);
    await run(`cmake --build . --config Release`);
  });

  let prefix = buildDir;
  if (config.os === "windows") {
    prefix += "/Release";
  }
  config.artifacts.push(`${prefix}/${libname()}`);
}

async function buildUpstream() {
  if (config.os === "windows") {
    const urlPrefix = "https://7-zip.org/a";
    const installerSpecs = {
      "386": {
        name: "7z2603.msi",
        isExe: false,
        hashes: {
          sha1: `7b80ca4b583aa9ba4f99089541c9ba0f6cc91c46 *7z2603.msi`,
          sha256: `23b5a8843b09db629b6f6a63364bae5430f66d6ecd4b38e11e1661bfaff5d2c6 *7z2603.msi`
        }
      },
      "amd64": {
        name: "7z2603-x64.msi",
        isExe: false,
        hashes: {
          sha1: `f09d573e0c79e744dbdb3c37f79dec45b375c947 *7z2603-x64.msi`,
          sha256: `c0680064d698a62dd4a5a47f403db356a6531a5473e4c4b1d090ea2590513926 *7z2603-x64.msi`
        }
      },
      "arm64": {
        name: "7z2603-arm64.exe",
        isExe: true,
        hashes: {
          sha1: `c0674e36b8596565c4c0f6d2e5e18907ee58a261 *7z2603-arm64.exe`,
          sha256: `e22ce71c11dcf503c448fe51e56f41830eb4e1344fa5c7731ae63bce533a8e8e *7z2603-arm64.exe`
        }
      }
    }
    const spec = installerSpecs[config.arch];
    await run(`curl -L -o ${spec.name} ${urlPrefix}/${spec.name}`);
    await checkHashes(spec.hashes);

    if (spec.isExe) {
      // EXE installer: 7z.dll is at root, no rename needed
      await run(`7z x -y -oexe ${spec.name}`);
      config.artifacts.push("exe/7z.dll");
    } else {
      // MSI installer: 7z.dll is prefixed with underscore
      await run(`7z x -y -omsi ${spec.name}`);
      await run(`mv msi/_7z.dll msi/7z.dll`);
      config.artifacts.push("msi/7z.dll");
    }
  } else {
    // Official 7-zip source (replaces unmaintained p7zip)
    const sourceUrl = `https://7-zip.org/a/7z2603-src.tar.xz`;
    const sha1 = `a48b1d61b3704fcd70db2ea6d3c85ffc8a09707a *source.tar.xz`;
    const sha256 = `9cbde5099c6deb73691b0579063da5827522ccbbcba3f0020fd04e8c8c16c0d4 *source.tar.xz`;

    await run(`curl -L ${sourceUrl} > source.tar.xz`);
    checkHashes({sha1, sha256});

    await run(`rm -rf source`);
    await run(`mkdir source`);
    await run(`tar -x -J -C source < source.tar.xz`);

    // Select the appropriate makefile and output directory based on OS and arch
    let makefile, outputDir;
    if (config.os === "darwin") {
      // macOS uses clang-based makefiles with architecture-specific flags
      if (config.arch === "arm64") {
        makefile = "cmpl_mac_arm64.mak";
        outputDir = "b/m_arm64";
      } else {
        makefile = "cmpl_mac_x64.mak";
        outputDir = "b/m_x64";
      }

      // Patch warn_clang_mac.mak to disable -Wswitch-default warning
      // The upstream 7zip code has many switch statements without default labels
      // which is intentional but triggers warnings with -Weverything
      const warnFile = "source/CPP/7zip/warn_clang_mac.mak";
      const fs = require("fs");
      // Make file writable (extracted tarball has read-only files)
      fs.chmodSync(warnFile, 0o644);
      let content = fs.readFileSync(warnFile, "utf8");
      content = content.replace(
        "CFLAGS_WARN = -Weverything -Wfatal-errors -Wno-poison-system-directories",
        "CFLAGS_WARN = -Weverything -Wfatal-errors -Wno-poison-system-directories -Wno-switch-default"
      );
      fs.writeFileSync(warnFile, content);
      log(`patched ${warnFile} to disable -Wswitch-default`);
    } else {
      // Linux uses GCC makefile
      makefile = "cmpl_gcc.mak";
      outputDir = "b/g";
    }

    await inDir("source/CPP/7zip/Bundles/Format7zF", async function() {
      await run(`make -j -f ../../${makefile}`);
    });
    // Output is called `7z.so` on both Linux and macOS
    config.artifacts.push(`source/CPP/7zip/Bundles/Format7zF/${outputDir}/7z.so`);
  }
}

async function checkHashes(hashes) {
  for (const k of Object.keys(hashes)) {
    log(`checking ${k} hash`);
    const sumFile = `${k}.txt`;
    const sum = hashes[k];
    writeFile(sumFile, sum);
    await run(`${k}sum -c ${sumFile}`);
  }
}

ci_compile(process.argv.slice(2));
