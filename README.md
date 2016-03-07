vgmtag
======

A tool to change GD3 tags for the VGM/VGZ files

Usage
-------------

`vgmtag --help` provides instructions how to use the program.

Build instruction (Unix-like systems)
-------------------------------------

Here, `${basedir}` denotes the root directory of the vgmtag codebase.

1. install the build tool [`ninja`](https://github.com/martine/ninja)
2. install GCC g++ 4.7+
3. install the libraries (including development versions; use your package manager for this):
    * `zlib`
4. build the static version of the library [`libafc`](https://github.com/dzidzitop/libafc) and copy it to `${basedir}/lib`
5. copy headers of the library `libafc` to `${basedir}/include`
6. execute `ninja` from `${basedir}`. The binary `vgmtag` will be created in `${basedir}/build`

System requirements
-------------------

* GCC g++ 4.7+
* zlib 1.2.8+
* ninja 1.3.3+
