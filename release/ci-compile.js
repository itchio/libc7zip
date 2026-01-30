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
        name: "7z2501.msi",
        isExe: false,
        hashes: {
          sha1: `eb90279bcd894f432b36ae8ac0e73c7be28023ca *7z2501.msi`,
          sha256: `dce9e456ace76b969fe0fe4d228bf096662c11d2376d99a9210f6364428a94c4 *7z2501.msi`
        }
      },
      "amd64": {
        name: "7z2501-x64.msi",
        isExe: false,
        hashes: {
          sha1: `15e3c8accdd5f7631a460be8283a53740dd94de6 *7z2501-x64.msi`,
          sha256: `e7eb0b7ed5efa4e087b7b17f191797f7af5b7f442d1290c66f3a21777005ef57 *7z2501-x64.msi`
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
