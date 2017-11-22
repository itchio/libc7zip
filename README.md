
# libc7zip

![](https://img.shields.io/badge/maintained%3F-no!-red.svg)

A wrapper over lib7zip so it can be used from C without callbacks.

This uses this particular lib7zip fork:

  * <https://github.com/itchio/lib7zip>

### Building

The library is built using CMake, and downloads lib7zip automatically, all you need to do
is the usual steps to build a CMake projet, for example:

```bash
mkdir build
cd build
cmake ..
make
```

If all goes well, `libc7zip.{dll,so,dylib}` will be in the `build/` folder. It's statically
linked with lib7zip, so you don't need to worry about it.

This library was made for internal purposes, to expose the 7-zip API to
<https://github.com/itchio/butler>.

As a result, I probably won't be accepting issues/PRs on this repo. Cheers!

### License

  * libc7zip itself is distributed under the MIT license (see the `LICENSE` file)
    * except for the utf conversion code, which is LGPL 2.1 (from 7-zip)
  * lib7zip is distributed under the MPL 2.0 license: <https://github.com/itchio/lib7zip>
  * 7-zip is LGPL 2.1 + some other terms, depending on which build you use: <http://7-zip.org/faq.html>

