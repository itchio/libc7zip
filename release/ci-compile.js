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
    await sign(artifact);
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

async function sign(target) {
  if (!process.env.CODESIGN_ENABLED) {
    log(`skipping code signing (CODESIGN_ENABLED not set)`);
    return;
  }

  if (config.os === "windows") {
    const signKey = "itch corp.";
    // const signUrl = "http://timestamp.comodoca.com/";
    // whoops, comodo won't talk to us now (April 16, 2018)
    const signUrl = "http://timestamp.globalsign.com/scripts/timestamp.dll"

    await run(`./vendor/signtool.exe sign //v //s MY //n "${signKey}" //fd sha256 //tr "${signUrl}" //td sha256 ${target}`);
  }
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
        name: "7z1900.msi",
        isExe: false,
        hashes: {
          sha1: `887ccdf0e9bab497a39a66506bd6ba641c30ff53 *7z1900.msi`,
          sha256: `b49d55a52bc0eab14947c8982c413d9be141c337da1368a24aa0484cbb5e89cd *7z1900.msi`
        }
      },
      "amd64": {
        name: "7z1900-x64.msi",
        isExe: false,
        hashes: {
          sha1: `d0dc016df5f9f9bf1a57b57db0e9e82f097b02b6 *7z1900-x64.msi`,
          sha256: `a7803233eedb6a4b59b3024ccf9292a6fffb94507dc998aa67c5b745d197a5dc *7z1900-x64.msi`
        }
      },
      "arm64": {
        name: "7z2501-arm64.exe",
        isExe: true,
        hashes: {
          sha1: `17fe72d57ef65d49a8734e11e084150bb75bf152 *7z2501-arm64.exe`,
          sha256: `6365c7c44e217b9c1009e065daf9f9aa37454e64315b4aaa263f7f8f060755dc *7z2501-arm64.exe`
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
    const sourceUrl = `https://downloads.sourceforge.net/project/p7zip/p7zip/16.02/p7zip_16.02_src_all.tar.bz2`
    const sha1 = `e8819907132811aa1afe5ef296181d3a15cc8f22 *source.tar.bz2`;
    const sha256 = `5eb20ac0e2944f6cb9c2d51dd6c4518941c185347d4089ea89087ffdd6e2341f *source.tar.bz2`;

    await run(`curl -L ${sourceUrl} > source.tar.bz2`);
    checkHashes({sha1, sha256});

    await run(`rm -rf source`);
    await run(`mkdir source`);
    await run(`tar -x -j --strip-components=1 -C source < source.tar.bz2`);
    await inDir("source", async function() {
      let makefileName = "";
      if (config.os === "linux") {
        if (config.arch === "amd64") {
          makefileName = "makefile.linux_amd64";
        } else {
          makefileName = "makefile.linux_any_cpu";
        }
      } else {
        makefileName = "makefile.macosx_llvm_64bits"
      }
      await run(`cp -f ${makefileName} makefile.machine`);
      // -Wno-narrowing needed for modern GCC/clang with old p7zip code
      // -std=c++14 needed because p7zip uses bool++ which is forbidden in C++17
      // Use ALLFLAGS_CPP (not ALLFLAGS) since these are C++ specific and break C compilation
      await run(`echo "ALLFLAGS_CPP += -Wno-narrowing -std=c++14" >> makefile.machine`);
      await run(`make all3`);
    });
    // sic. - it's also called `7z.so` on macOS
    config.artifacts.push("source/bin/7z.so");
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
