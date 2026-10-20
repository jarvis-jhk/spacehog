#!/bin/bash
# CI build (GitHub Actions, ubuntu-22.04): static SDL3 + the game, packed into dist/.
#   linux:   SDL3 with X11 + Wayland (system libraries are loaded at run time),
#            glibc of the build base (2.35) is the floor.
#   windows: cross-built with llvm-mingw.
#   macos:   universal binary (arm64 + x86_64), macOS 11+, on a GitHub macos runner;
#            no online co-op yet (there is no macOS libwarpzone).
# usage: scripts/ci-build.sh linux|windows|macos VERSION
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
elif [ "$OS" = macos ]; then
   MACOPT=(-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0)
   SDLOPT+=("${MACOPT[@]}")
   BIN=spacehog
else
   SDLOPT+=(-DSDL_X11=ON -DSDL_WAYLAND=ON -DSDL_PULSEAUDIO=ON -DSDL_PIPEWIRE=ON -DSDL_ALSA=ON)
   BIN=spacehog
fi
PREFIX=$DL/sdl3-$OS
if [ ! -f "$PREFIX/.done-$SDL_VER" ]; then
   rm -rf "$PREFIX" "$DL/build-sdl3-$OS"
   cmake -S SDL3-$SDL_VER -B "$DL/build-sdl3-$OS" "${SDLOPT[@]}" -DCMAKE_INSTALL_PREFIX="$PREFIX"
   cmake --build "$DL/build-sdl3-$OS" -j"$(getconf _NPROCESSORS_ONLN)"
   cmake --install "$DL/build-sdl3-$OS"
   touch "$PREFIX/.done-$SDL_VER"
fi
WZ=
[ "$OS" = macos ] || WZ=$("$ROOT/scripts/fetch-warpzone.sh" "$OS" "$DL/warpzone")
GAMEOPT=(-G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$PREFIX" -DWARPZONE_DIR="$WZ" -DSPACEHOG_VERSION="${VER#v}")
[ "$OS" = windows ] && GAMEOPT+=(-DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/mingw-w64-x86_64.cmake")
[ "$OS" = macos ] && GAMEOPT+=("${MACOPT[@]}")
cmake -S "$ROOT" -B "$ROOT/build-ci-$OS" "${GAMEOPT[@]}"
cmake --build "$ROOT/build-ci-$OS"
if [ "$OS" = linux ]; then
   # the release must start on older distributions and the Steam Deck
   max=$(objdump -T "$ROOT/build-ci-$OS/$BIN" | grep -o 'GLIBC_[0-9.]*' | sort -uV | tail -1)
   echo "newest glibc symbol: $max"
   [ "$(printf '%s\n' "$max" GLIBC_2.35 | sort -V | tail -1)" = GLIBC_2.35 ] || { echo "needs $max > 2.35" >&2; exit 1; }
   "$ROOT/build-ci-$OS/$BIN" --version
fi
if [ "$OS" = macos ]; then
   B=$ROOT/build-ci-$OS/$BIN
   archs=$(lipo -archs "$B"); echo "architectures: $archs"
   case " $archs " in *" arm64 "*) ;; *) echo "no arm64 slice" >&2; exit 1 ;; esac
   case " $archs " in *" x86_64 "*) ;; *) echo "no x86_64 slice" >&2; exit 1 ;; esac
   # only system libraries and frameworks (SDL3 is static)
   otool -L "$B"
   if otool -L "$B" | tail -n +2 | grep -v -e '^[[:space:]]*/usr/lib/' -e '^[[:space:]]*/System/Library/'; then
      echo "links a non-system library" >&2; exit 1
   fi
   for a in arm64 x86_64; do
      minos=$(otool -arch $a -l "$B" | grep -A3 LC_BUILD_VERSION | grep minos | head -1)
      echo "$a: $minos"
      [ "$(echo "$minos" | tr -s ' ' | cut -d' ' -f3)" = 11.0 ] || { echo "$a: minimum macOS is not 11.0" >&2; exit 1; }
   done
   "$B" --version
   arch -x86_64 "$B" --version
fi
WARPZONE_DIR=$WZ "$ROOT/scripts/pack.sh" "$OS" "$ROOT/build-ci-$OS/$BIN" "$ROOT/dist" "$VER"
