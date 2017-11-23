#!/usr/bin/env node

const $ = require("./common");

async function ci_compile(args) {
  const [os, arch] = args;

  $.say(`compiling libc7zip for os ${os}, arch ${arch}`);
  $.say(`process.versions: ${JSON.stringify(process.versions, null, 2)}`);

  let buildDir = `./build`;
  $(await $.sh(`rm -rf ${buildDir}`));
  $(await $.sh(`mkdir -p ${buildDir}`));

  await $.cd(buildDir, async () => {
    $(await $.sh(`cmake -DCMAKE_BUILD_TYPE=Release ..`))
    $(await $.sh(`cmake --build .`))
  });

  const osarch = `${os}-${arch}`;
  let binDir = `./compile-artifacts/${osarch}`;
  $(await $.sh(`mkdir -p ${binDir}`));

  $(await $.sh(`cp -f ${buildDir}/${libname(os)} ${binDir}/`));
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