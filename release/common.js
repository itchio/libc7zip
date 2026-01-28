// base functions useful throughout CI scripts
// Self-contained utility module using only Node.js built-ins

const { exec, execSync } = require("child_process");
const fs = require("fs");

async function run(cmd) {
  console.log(`\x1b[34m$ ${cmd}\x1b[0m`);
  return new Promise((resolve, reject) => {
    exec(cmd, { maxBuffer: 50 * 1024 * 1024 }, (err, stdout, stderr) => {
      if (stdout) process.stdout.write(stdout);
      if (stderr) process.stderr.write(stderr);
      if (err) reject(new Error(`Command failed: ${cmd}`));
      else resolve();
    });
  });
}

function log(msg) {
  console.log(`\x1b[32m>> ${msg}\x1b[0m`);
}

async function inDir(dir, fn) {
  const prev = process.cwd();
  process.chdir(dir);
  try {
    await fn();
  } finally {
    process.chdir(prev);
  }
}

function getOutput(cmd) {
  return execSync(cmd, { encoding: "utf8" }).trim();
}

function writeFile(path, data) {
  fs.writeFileSync(path, data);
}

module.exports = { run, log, inDir, getOutput, writeFile };
