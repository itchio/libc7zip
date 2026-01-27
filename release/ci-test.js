#!/usr/bin/env node

const $ = require("./common");
const posixPath = require("path").posix;
const path = require("path");

let config = undefined;

async function ci_test(args) {
  const [os, arch] = args;

  if (!os) { throw new Error(`missing os`); }
  if (["linux", "windows", "darwin"].indexOf(os) === -1) { throw new Error(`unknown os '${os}'`); }

  if (!arch) { throw new Error(`missing arch`); }
  if (["386", "amd64", "arm64"].indexOf(arch) === -1) { throw new Error(`unknown arch '${arch}'`); }

  const osarch = `${os}-${arch}`;
  $.say(`running tests for libc7zip on ${osarch}`);

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
  $(await $.sh(`rm -rf ${buildDir}`));
  $(await $.sh(`mkdir -p ${buildDir}`));

  let extraCMakeFlags = "";
  if (config.os === "windows") {
    const arch = config.arch === "386" ? "Win32" : "x64";
    extraCMakeFlags = `-G "Visual Studio 17 2022" -A ${arch}`;
  }

  await $.cd(buildDir, async () => {
    // Configure with tests enabled
    $(await $.sh(`cmake ${extraCMakeFlags} -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON ../..`));

    // Build
    $(await $.sh(`cmake --build . --config Release`));

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
    const mainBuildDir = `../../build/${config.osarch}`;
    let backendSrc;
    if (config.os === "windows") {
      backendSrc = `${mainBuildDir}/msi/${backendLibName()}`;
    } else {
      backendSrc = `${mainBuildDir}/source/bin/${backendLibName()}`;
    }

    // Also check if backend lib is in broth directory (from CI compile step)
    const brothDir = `../../broth/${config.osarch}`;
    const brothBackend = `${brothDir}/${backendLibName()}`;

    // Try to find the backend library
    // $.sh() throws on non-zero exit, so if test -f succeeds the file exists
    let foundBackend = false;
    for (const src of [backendSrc, brothBackend]) {
      try {
        await $.sh(`test -f ${src}`);
        // If we get here without throwing, the file exists
        $(await $.sh(`cp -f ${src} ${testDir}/`));
        $.say(`copied ${backendLibName()} from ${src} to ${testDir}/`);
        foundBackend = true;
        break;
      } catch (e) {
        // File doesn't exist, try next source
      }
    }

    if (!foundBackend) {
      $.say(`warning: could not find ${backendLibName()}, tests may fail`);
    }

    // Copy libc7zip to test directory
    const libSrc = `${libDir}/${libname()}`;
    $(await $.sh(`cp -f ${libSrc} ${testDir}/`));
    $.say(`copied ${libname()} to ${testDir}/`);

    // Run tests with ctest
    // Use "cd test &&" instead of "--test-dir test" for compatibility with older CMake (<3.20)
    let testEnv = "";
    if (config.os === "linux") {
      testEnv = `LD_LIBRARY_PATH=.:$LD_LIBRARY_PATH`;
    } else if (config.os === "darwin") {
      testEnv = `DYLD_LIBRARY_PATH=.:$DYLD_LIBRARY_PATH`;
    }

    const ctestConfig = config.os === "windows" ? "-C Release" : "";
    $(await $.sh(`cd test && ${testEnv} ctest ${ctestConfig} --output-on-failure`));
  });

  $.say(`tests passed for ${config.osarch}`);
}

ci_test(process.argv.slice(2));
