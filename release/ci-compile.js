#!/usr/bin/env node

const $ = require("./common");
// explicitly ask for the posix version of path manipulation
// routines, because we run our windows worker in MSYS2
const posixPath = require("path").posix;

let config = undefined

async function ci_compile(args) {
  const [os, arch] = args;

  if (!os) { throw new Error(`missing os`); }
  if (["linux", "windows", "darwin"].indexOf(os) === -1) { throw new Error(`unknown os '${os}'`); }

  if (!arch) { throw new Error(`missing arch`); }
  if (["386", "amd64"].indexOf(arch) === -1) { throw new Error(`unknown os '${arch}'`); }

  const osarch = `${os}-${arch}`;
  $.say(`compiling libc7zip for ${osarch}`);

  let ciVersion = process.env.CI_BUILD_REF_NAME;
  let artifactsDir = `./compile-artifacts/${osarch}`;
  let binDir = `${artifactsDir}/${ciVersion}`;
  $(await $.sh(`mkdir -p ${binDir}`));

  config = { os, arch, osarch, binDir, artifacts: [] };

  await buildLib();
  await buildUpstream();
  
  for (const artifact of config.artifacts) {
    await sign(artifact);
    $(await $.sh(`cp -f ${artifact} ${binDir}/`));
  }

  let artifactNames = config.artifacts.map((v) => posixPath.basename(v));

  $.say(`artifacts for ${osarch}: `);
  for (const artifactName of artifactNames) {
    $.say(` - ${artifactName}: ${await $.getOutput(`file ${binDir}/${artifactName}`)}`);
  }

  await $.cd(binDir, async () => {
    // ibrew file hierarchy reminder:
    //
    // - dl.itch.ovh
    //   - project
    //     - os-arch
    //       - LATEST
    //       - v1.1.0
    //         - naked-artifact
    //         - project.7z
    //         - project.zip
    //         - SHA1SUMS
    //         - SHA256SUMS

    $(await $.sh(`7za a libc7zip.zip ${artifactNames.join(" ")}`));
    $(await $.sh(`7za a libc7zip.7z ${artifactNames.join(" ")}`));
    $(await $.sh(`sha1sum * > SHA1SUMS`));
    $(await $.sh(`sha256sum * > SHA256SUMS`));
  });

  if (process.env.CI_BUILD_TAG) {
    await $.writeFile(`${artifactsDir}/LATEST`, `${process.env.CI_BUILD_TAG}\n`);
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
  if (config.os === "windows") {
    const signKey = "itch corp.";
    const signUrl = "http://timestamp.comodoca.com/";
    
    $(await $.sh(`./vendor/signtool.exe sign //v //s MY //n "${signKey}" //fd sha256 //tr "${signUrl}?td=sha256" //td sha256 ${target}`));
  }
}

async function buildLib() {
  let buildDir = `./build/${config.osarch}`;
  $(await $.sh(`rm -rf ${buildDir}`));
  $(await $.sh(`mkdir -p ${buildDir}`));

  let extraCMakeFlags = ""
  if (config.os === "windows") {
    if (config.arch === "386") {
      extraCMakeFlags = `-G "Visual Studio 14 2015"`;
    } else {
      extraCMakeFlags = `-G "Visual Studio 14 2015 Win64"`;
    }
  }

  await $.cd(buildDir, async () => {
    $(await $.sh(`cmake ${extraCMakeFlags} -DCMAKE_BUILD_TYPE=Release ../..`))
    $(await $.sh(`cmake --build . --config Release`))
  });

  let prefix = buildDir;
  if (config.os === "windows") {
    prefix += "/Release";
  }
  config.artifacts.push(`${prefix}/${libname()}`);
}

async function buildUpstream() {
  if (config.os === "windows") {
    const urlPrefix = "http://7-zip.org/a";
    const msiSpecs = {
      "386": {
        name: "7z1604.msi",
        hashes: {
          sha1: `e1ee28c92d74c7961da7e4d4e4420e242c2951b2 *7z1604.msi`,
          sha256: `d9b62c0ed0eb48d2df86d8b83394048414a2a4e1d64a50adb9abcff643471d20 *7z1604.msi`
        }
      },
      "amd64": {
        name: "7z1604-x64.msi",
        hashes: {
          sha1: `bae316e5148d3b42efa1d3f272afc10d3ffa6f4b *7z1604-x64.msi`,
          sha256: `b3885b2f090f1e9b5cf2b9f802b07fe88e472d70d60732db9f830209ac296067 *7z1604-x64.msi`
        }
      }
    }
    const spec = msiSpecs[config.arch];
    $(await $.sh(`wget ${urlPrefix}/${spec.name}`))
    await checkHashes(spec.hashes);

    // assume yes, output in the `msi` folder
    $(await $.sh(`7z x -y -omsi ${spec.name}`))

    $(await $.sh(`mv msi/_7z.dll msi/7z.dll`))  
    config.artifacts.push("msi/7z.dll");
  } else {
    const sourceUrl = `https://downloads.sourceforge.net/project/p7zip/p7zip/16.02/p7zip_16.02_src_all.tar.bz2`
    const sha1 = `e8819907132811aa1afe5ef296181d3a15cc8f22 *source.tar.bz2`;
    const sha256 = `5eb20ac0e2944f6cb9c2d51dd6c4518941c185347d4089ea89087ffdd6e2341f *source.tar.bz2`;

    $(await $.sh(`curl -L ${sourceUrl} > source.tar.bz2`));
    checkHashes({sha1, sha256});

    $(await $.sh(`mkdir source`))
    $(await $.sh(`tar -x -j --strip-components=1 -C source < source.tar.bz2`));
    await $.cd("source", async function() {
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
      $(await $.sh(`cp -f ${makefileName} makefile.machine`))
      $(await $.sh(`make all3`))
    })
    // sic. - it's also called `7z.so` on macOS
    config.artifacts.push("source/bin/7z.so");
  }
}

async function checkHashes(hashes) {
  for (const k of Object.keys(hashes)) {
    $.say(`checking ${k} hash`);
    const sumFile = `${k}.txt`;
    const sum = hashes[k];
    await $.writeFile(sumFile, sum);
    $(await $.sh(`${k}sum -c ${sumFile}`));
  }
}

ci_compile(process.argv.slice(2));
