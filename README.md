
# libc7zip

A wrapper over lib7zip so it can be used from C without callbacks.

This project exists to enable calling 7-zip from Go via [sevenzip-go](https://github.com/itchio/sevenzip-go). These shared libraries are packed as sidecar files with [butler](https://github.com/itchio/butler) to facilitate extraction of archives. Butler dynamically loads these libraries at runtime for all archive extraction, as 7-zip supports a wide range of archive formats (zip, tar, gzip, rar, 7z, and many others).

lib7zip is vendored in `vendor/lib7zip/`, originally based on <https://github.com/stonewell/lib7zip>.

## Building

The library is built using CMake. A convenience Makefile wraps the CI build scripts for local development:

```bash
make build          # Build for current OS/architecture
make test           # Run test suite (after build)
make clean          # Remove build artifacts
```

The Makefile auto-detects your OS and architecture. To cross-compile or override:

```bash
make build OS=linux ARCH=arm64
make build OS=darwin ARCH=amd64
```

If all goes well, `libc7zip.{dll,so,dylib}` will be in the `build/` folder.

### CI Build Process

The full CI build is orchestrated by `release/ci-compile.js` and builds for multiple platforms:

- **Linux**: amd64, arm64
- **macOS**: amd64 (Intel), arm64 (Apple Silicon)
- **Windows**: 32-bit (386), 64-bit (amd64), ARM64

#### Linux and macOS

On Linux and macOS, the build compiles from official [7-Zip](https://7-zip.org/) source (version 25.01). The build produces:

- `libc7zip.so` / `libc7zip.dylib` - The wrapper library
- `7z.so` - The 7-zip engine (built from 7-zip source)

#### Windows

Windows builds work differently. Instead of compiling 7-zip from source, the build downloads official pre-built 7-zip installers from <https://7-zip.org/a/> (version 25.01) and extracts the `7z.dll` from them. The wrapper library `c7zip.dll` is still compiled from source using Visual Studio.

The Windows build produces:

- `c7zip.dll` - The wrapper library (built from source)
- `7z.dll` - The 7-zip engine (extracted from official MSI)

### Deployment

The built libraries are deployed to <https://itchio.itch.io/libc7zip> where butler can fetch them as needed.

## License

  * libc7zip itself is distributed under the MIT license (see the `LICENSE` file)
    * except for the utf conversion code, which is LGPL 2.1 (from 7-zip)
  * lib7zip is distributed under the MPL 2.0 license (see `vendor/lib7zip/COPYING`)
  * 7-zip is LGPL 2.1 + some other terms, depending on which build you use: <http://7-zip.org/faq.html>

