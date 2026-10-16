#!/bin/bash
# Build release packages (without the game data) into dist/:
#   spacehog-linux-x86_64.tar.gz, spacehog-windows-x86_64.zip (each holding a
#   spacehog/ folder with VERSION.txt) and SHA256SUMS. The names carry no version,
#   so the Warpzone launcher's descriptor matches every release.
# usage: scripts/package.sh
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
source "$ROOT/tools/env.sh"
VER=$(git -C "$ROOT" describe --tags --always --dirty)
DIST=$ROOT/dist
rm -rf "$DIST"; mkdir -p "$DIST"
cmake -S "$ROOT" -B "$ROOT/build-rel" -G Ninja -DCMAKE_BUILD_TYPE=Release \
   -DCMAKE_PREFIX_PATH="$ROOT/tools/sdl3-linux" >/dev/null
cmake --build "$ROOT/build-rel"
cmake -S "$ROOT" -B "$ROOT/build-rel-win" -G Ninja -DCMAKE_BUILD_TYPE=Release \
   -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/mingw-w64-x86_64.cmake" \
   -DCMAKE_PREFIX_PATH="$ROOT/tools/sdl3-win" >/dev/null
cmake --build "$ROOT/build-rel-win"
"$ROOT/scripts/pack.sh" linux "$ROOT/build-rel/spacehog" "$DIST" "$VER"
"$ROOT/scripts/pack.sh" windows "$ROOT/build-rel-win/spacehog.exe" "$DIST" "$VER"
L=spacehog-linux-x86_64 W=spacehog-windows-x86_64
(cd "$DIST" && sha256sum "$L.tar.gz" "$W.zip" > SHA256SUMS)
ls -la "$DIST"/*.tar.gz "$DIST"/*.zip "$DIST"/SHA256SUMS
