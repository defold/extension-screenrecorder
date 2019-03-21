# Screenrecorder extension

The extension consists of three inner implementations - one for iOS, one for Android and one for desktop platforms.

## macOS, Linux, Windows, HTML5

On desktop platforms `libvpx` and `libwebm` are used for encoding and writing to WEBM files.

`libvpx` version 1.7.0 is used. It's compiled on Linux, macOS and Windows.

Configuration command for Linux:
```
../libvpx/configure --target=x86_64-linux-gcc --enable-vp8 --enable-onthefly-bitpacking --enable-runtime-cpu-detect --disable-webm-io --disable-libyuv --disable-examples --disable-vp9-encoder --disable-vp9-decoder --disable-vp8-decoder --disable-unit-tests --disable-tools --disable-docs
```

Configuration command for macOS:
```
../libvpx/configure --target=x86_64-darwin16-gcc --enable-vp8 --enable-onthefly-bitpacking --enable-runtime-cpu-detect --disable-webm-io --disable-libyuv --disable-examples --disable-vp9-encoder --disable-vp9-decoder --disable-vp8-decoder --disable-unit-tests --disable-tools --disable-docs
```

Configuration command for Windows:
```
../libvpx/configure --target=x86_64-win64-vs15 --enable-vp8 --enable-onthefly-bitpacking --enable-runtime-cpu-detect --disable-webm-io --disable-libyuv --disable-examples --disable-vp9-encoder --disable-vp9-decoder --disable-vp8-decoder --disable-unit-tests --disable-tools --disable-docs
```

Replace `x86_64-win64-vs15` with `x86-win32-vs15` for 32 bit build.

Configuration command for HTML5:
```
emconfigure ../libvpx/configure --target=generic-gnu --enable-vp8 --disable-runtime-cpu-detect --disable-webm-io --disable-libyuv --disable-examples --disable-vp9-encoder --disable-vp9-decoder --disable-vp8-decoder --disable-unit-tests --disable-tools --disable-docs
```

Follow compilation instructions in the `libvpx` repository. https://github.com/webmproject/libvpx

On Windows the `make` command generates a Visual Studio project, which is then has to be compiled manually. However an additional step is required. In the project settings set `Optimizations -> Whole Program Optimization` to `NO`. Otherwise Defold will not accept the library.

When compiling for HTML5, edit `libs-generic-gnu.mk` makefile and remove the dash before the `crs` option in the `ARFLAGS` variable. After that you can invoke `emmake make`.

`libwebm` is compiled similary to `libvpx`, however `cmake` is used instead of plain `make`.

Command to generate makefiles for Linux:
```
cmake path/to/libwebm -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=MinSizeRel
```

Command to generate Xcode project for macOS:
```
cmake path/to/libwebm -G Xcode
```

Command to generate Visual Studio project for Windows 64bit:
```
cmake path/to/libwebm -G "Visual Studio 15 Win64" 
```

Command to generate Visual Studio project for Windows 32bit:
```
cmake path/to/libwebm -G "Visual Studio 15" 
```

Command to generate makefiles for HTML5:
```
emconfigure cmake path/to/libwebm -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=MinSizeRel
```

The `MinSizeRel` target is recommended to use.

Follow build instructions in the `libwebm` repository https://github.com/webmproject/libwebm

For HTML5 builds it's recommended to use the same emscripten version as Defold uses on the build server.