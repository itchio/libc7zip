#!/usr/bin/env node

const $ = require("./common");

async function ci_compile(args) {
  const [os, arch] = args;

  if (!os) { throw new Error(`missing os`); }
  if (["linux", "windows", "darwin"].indexOf(os) === -1) { throw new Error(`unknown os '${os}'`); }

  if (!arch) { throw new Error(`missing arch`); }
  if (["386", "amd64"].indexOf(arch) === -1) { throw new Error(`unknown os '${arch}'`); }

  const osarch = `${os}-${arch}`;
  $.say(`compiling libc7zip for ${osarch}`);

  let buildDir = `./build/${osarch}`;
  $(await $.sh(`rm -rf ${buildDir}`));
  $(await $.sh(`mkdir -p ${buildDir}`));

  let extraCMakeFlags = ""
  if (os === "windows") {
    if (arch === "386") {
      extraCMakeFlags = `-G "Visual Studio 14 2015"`;
    } else {
      extraCMakeFlags = `-G "Visual Studio 14 2015 Win64"`;
    }
  }

  await $.cd(buildDir, async () => {
    $(await $.sh(`cmake ${extraCMakeFlags} -DCMAKE_BUILD_TYPE=Release ../..`))
    $(await $.sh(`cmake --build . --config Release`))
  });

  let ciVersion = process.env.CI_BUILD_REF_NAME;
  let artifactsDir = `./compile-artifacts/${osarch}`;
  let binDir = `${artifactsDir}/${ciVersion}`;
  $(await $.sh(`mkdir -p ${binDir}`));

  let artifacts = [];
  switch (os) {
    case "linux":
      artifacts.push(libname(os));
      break;
    case "darwin":
      artifacts.push(libname(os));
      break;
    case "windows":
      artifacts.push(`Release/${libname(os)}`);
      break;
  }

  for (const artifact of artifacts) {
    const fullArtifactPath = `${buildDir}/${artifact}`;
    if (os === "windows") {
      const signKey = "itch corp.";
      const signUrl = "http://timestamp.comodoca.com/";
      
      $(await $.sh(`./vendor/signtool.exe sign //v //s MY //n "${signKey}" //fd sha256 //tr "${signUrl}?td=sha256" //td sha256 ${fullArtifactPath}`));
    }
    $(await $.sh(`cp -f ${fullArtifactPath} ${binDir}/`));
  }

  await $.cd(binDir, async () => {
    // set up a file hierarchy that ibrew can consume, ie:
    //
    // - dl.itch.ovh
    //   - libc7zip
    //     - windows-amd64
    //       - LATEST
    //       - v1.1.0
    //         - libc7zip.zip
    //         - c7zip.dll
    //         - SHA1SUMS
    //         - SHA256SUMS

    $(await $.sh(`zip -9 -r libc7zip.zip ${artifacts.join(" ")}`));
    $(await $.sh(`sha1sum * > SHA1SUMS`));
    $(await $.sh(`sha256sum * > SHA256SUMS`));
  });

  if (process.env.CI_BUILD_TAG) {
    await $.writeFile(`${artifactsDir}/LATEST`, `${process.env.CI_BUILD_TAG}\n`);
  }
}

function libname(os) {
  switch (os) {
    case "linux":
      return "libc7zip.so";
    case "darwin":
      return "libc7zip.dylib";
    case "windows":
      return "c7zip.dll";
  }
  throw new Error(``)
}


ci_compile(process.argv.slice(2));