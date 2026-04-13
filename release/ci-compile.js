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
    // VS 2022 uses -A flag for architecture instead of generator suffix
    const archMap = { "386": "Win32", "amd64": "x64", "arm64": "ARM64" };
    const arch = archMap[config.arch];
    extraCMakeFlags = `-G "Visual Studio 17 2022" -A ${arch}`;
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
        name: "7z2600.msi",
        isExe: false,
        hashes: {
          sha1: `22d12f5292440c7644f70697f20f294d206241da *7z2600.msi`,
          sha256: `53b4f99a2471678020a326fd1d5c888616f5d6c84b00d5db7da30357755c74c3 *7z2600.msi`
        }
      },
      "amd64": {
        name: "7z2600-x64.msi",
        isExe: false,
        hashes: {
          sha1: `41e7990b056ebaf0d427f2a00bf1aa12b6010975 *7z2600-x64.msi`,
          sha256: `c388d0444871ca11b21237001af158cfddad7e137851795e5b65cee69b518495 *7z2600-x64.msi`
        }
      },
      "arm64": {
        name: "7z2600-arm64.exe",
        isExe: true,
        hashes: {
          sha1: `f8c2aa3c8f98a11215cddb11340a12c166d67468 *7z2600-arm64.exe`,
          sha256: `92fac666911336f3bbf3d99fdc48ec36fe20ac7a4200556936e61a8076ae6493 *7z2600-arm64.exe`
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
    const sourceUrl = `https://7-zip.org/a/7z2600-src.tar.xz`;
    const sha1 = `32f1646a6281bb55a547941576660dc7addfff62 *source.tar.xz`;
    const sha256 = `3e596155744af055a77fc433c703d54e3ea9212246287b5b1436a6beac060f16 *source.tar.xz`;

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
