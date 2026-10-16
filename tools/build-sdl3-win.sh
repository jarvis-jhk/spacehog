#!/bin/bash
# Cross-build static SDL3 for Windows (x86_64) into tools/sdl3-win with llvm-mingw.
set -e
source "$(dirname "$0")/env.sh"
cmake -S $SH_ROOT/tools/SDL3-3.4.18 -B $SH_ROOT/tools/build-sdl3-win -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=$SH_ROOT/cmake/mingw-w64-x86_64.cmake -DCMAKE_BUILD_TYPE=Release \
  -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF -DCMAKE_INSTALL_PREFIX=$SH_ROOT/tools/sdl3-win
cmake --build $SH_ROOT/tools/build-sdl3-win -j"$(nproc)"
cmake --install $SH_ROOT/tools/build-sdl3-win
