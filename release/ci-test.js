#!/usr/bin/env node

const { run, log, inDir } = require("./common");

let config = undefined;

async function ci_test(args) {
  const [os, arch] = args;

  if (!os) { throw new Error(`missing os`); }
  if (["linux", "windows", "darwin"].indexOf(os) === -1) { throw new Error(`unknown os '${os}'`); }

  if (!arch) { throw new Error(`missing arch`); }
  if (["386", "amd64", "arm64"].indexOf(arch) === -1) { throw new Error(`unknown arch '${arch}'`); }

  const osarch = `${os}-${arch}`;
  log(`running tests for libc7zip on ${osarch}`);

  config = { os, arch, osarch };

  await runTests();
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

function backendLibName() {
  switch (config.os) {
    case "linux":
    case "darwin":
      return "7z.so";
    case "windows":
      return "7z.dll";
  }
  throw new Error(`unknown os ${config.os}`);
}

async function runTests() {
  let buildDir = `./build/${config.osarch}-test`;
  await run(`rm -rf ${buildDir}`);
  await run(`mkdir -p ${buildDir}`);

  let extraCMakeFlags = "";
  if (config.os === "windows") {
    const arch = config.arch === "386" ? "Win32" : "x64";
    extraCMakeFlags = `-G "Visual Studio 17 2022" -A ${arch}`;
  }

  await inDir(buildDir, async () => {
    // Configure with tests enabled
    await run(`cmake ${extraCMakeFlags} -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON ../..`);

    // Build
    await run(`cmake --build . --config Release`);

    // Determine paths based on OS
    let testDir, libDir;
    if (config.os === "windows") {
      testDir = "test/Release";
      libDir = "Release";
    } else {
      testDir = "test";
      libDir = ".";
    }

    // Copy 7z backend library to test directory so it can be found at runtime
    // We're inside build/${osarch}-test, so we need ../../ to get back to repo root
    const brothDir = `../../broth/${config.osarch}`;
    const backendSrc = `${brothDir}/${backendLibName()}`;
    await run(`cp -f ${backendSrc} ${testDir}/`);
    log(`copied ${backendLibName()} to ${testDir}/`)

    // Copy libc7zip to test directory
    const libSrc = `${libDir}/${libname()}`;
    await run(`cp -f ${libSrc} ${testDir}/`);
    log(`copied ${libname()} to ${testDir}/`);

    // Run tests with ctest
    // Use "cd test &&" instead of "--test-dir test" for compatibility with older CMake (<3.20)
    let testEnv = "";
    if (config.os === "linux") {
      testEnv = `LD_LIBRARY_PATH=.:$LD_LIBRARY_PATH`;
    } else if (config.os === "darwin") {
      testEnv = `DYLD_LIBRARY_PATH=.:$DYLD_LIBRARY_PATH`;
    }

    const ctestConfig = config.os === "windows" ? "-C Release" : "";
    await run(`cd test && ${testEnv} ctest ${ctestConfig} --output-on-failure`);
  });

  log(`tests passed for ${config.osarch}`);
}

ci_test(process.argv.slice(2));
