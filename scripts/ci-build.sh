#!/bin/bash
# CI build (GitHub Actions, ubuntu-22.04): static SDL3 + the game, packed into dist/.
#   linux:   SDL3 with X11 + Wayland (system libraries are loaded at run time),
#            glibc of the build base (2.35) is the floor.
#   windows: cross-built with llvm-mingw.
# usage: scripts/ci-build.sh linux|windows VERSION
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OS=$1 VER=$2
SDL_VER=3.4.18
LLVM_MINGW=llvm-mingw-20261006-ucrt-ubuntu-22.04-x86_64
DL=$ROOT/ci-cache; mkdir -p "$DL"
cd "$DL"
[ -d SDL3-$SDL_VER ] || curl -fsSL "https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VER/SDL3-$SDL_VER.tar.gz" | tar xz
SDLOPT=(-G Ninja -DCMAKE_BUILD_TYPE=Release -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF)
if [ "$OS" = windows ]; then
   [ -d $LLVM_MINGW ] || curl -fsSL "https://github.com/mstorsjo/llvm-mingw/releases/download/20261006/$LLVM_MINGW.tar.xz" | tar xJ
   export PATH=$DL/$LLVM_MINGW/bin:$PATH
   SDLOPT+=(-DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/mingw-w64-x86_64.cmake")
   BIN=spacehog.exe
else
   SDLOPT+=(-DSDL_X11=ON -DSDL_WAYLAND=ON -DSDL_PULSEAUDIO=ON -DSDL_PIPEWIRE=ON -DSDL_ALSA=ON)
   BIN=spacehog
fi
PREFIX=$DL/sdl3-$OS
if [ ! -f "$PREFIX/.done-$SDL_VER" ]; then
   rm -rf "$PREFIX" "$DL/build-sdl3-$OS"
   cmake -S SDL3-$SDL_VER -B "$DL/build-sdl3-$OS" "${SDLOPT[@]}" -DCMAKE_INSTALL_PREFIX="$PREFIX"
   cmake --build "$DL/build-sdl3-$OS" -j"$(nproc)"
   cmake --install "$DL/build-sdl3-$OS"
   touch "$PREFIX/.done-$SDL_VER"
fi
WZ=$("$ROOT/scripts/fetch-warpzone.sh" "$OS" "$DL/warpzone")
GAMEOPT=(-G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$PREFIX" -DWARPZONE_DIR="$WZ" -DSPACEHOG_VERSION="${VER#v}")
[ "$OS" = windows ] && GAMEOPT+=(-DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/mingw-w64-x86_64.cmake")
cmake -S "$ROOT" -B "$ROOT/build-ci-$OS" "${GAMEOPT[@]}"
cmake --build "$ROOT/build-ci-$OS"
if [ "$OS" = linux ]; then
   # the release must start on older distributions and the Steam Deck
   max=$(objdump -T "$ROOT/build-ci-$OS/$BIN" | grep -o 'GLIBC_[0-9.]*' | sort -uV | tail -1)
   echo "newest glibc symbol: $max"
   [ "$(printf '%s\n' "$max" GLIBC_2.35 | sort -V | tail -1)" = GLIBC_2.35 ] || { echo "needs $max > 2.35" >&2; exit 1; }
   "$ROOT/build-ci-$OS/$BIN" --help >/dev/null
fi
WARPZONE_DIR=$WZ "$ROOT/scripts/pack.sh" "$OS" "$ROOT/build-ci-$OS/$BIN" "$ROOT/dist" "$VER"
