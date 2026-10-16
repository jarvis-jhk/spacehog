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
pack() {   # staging dir, binary
   local d=$DIST/$1/spacehog
   mkdir -p "$d"
   cp "$2" "$d/"
   cp "$ROOT/README.md" "$ROOT/CHANGELOG.md" "$ROOT/LICENSE" "$ROOT/THIRD_PARTY.txt" "$d/"
   echo "$VER" > "$d/VERSION.txt"
   cp "$ROOT/src/third_party/nuked_opl3/LICENSE" "$d/LICENSE-NukedOPL3.txt"
   printf 'Copy SPACEHOG.DAT, SHLEV.DAT, SHBGS.DAT and SHSND.DAT from the original\nOperation Spacehog (freeware) into this directory, then start the game.\n' > "$d/PUT_GAME_FILES_HERE.txt"
}
L=spacehog-linux-x86_64
pack "$L" "$ROOT/build-rel/spacehog"
strip "$DIST/$L/spacehog/spacehog"
tar -C "$DIST/$L" -czf "$DIST/$L.tar.gz" spacehog
W=spacehog-windows-x86_64
pack "$W" "$ROOT/build-rel-win/spacehog.exe"
llvm-strip "$DIST/$W/spacehog/spacehog.exe"
(cd "$DIST/$W" && python3 -c "import shutil; shutil.make_archive('../$W', 'zip', '.', 'spacehog')")
(cd "$DIST" && sha256sum "$L.tar.gz" "$W.zip" > SHA256SUMS)
ls -la "$DIST"/*.tar.gz "$DIST"/*.zip "$DIST"/SHA256SUMS
