#!/bin/bash
# Pack one release archive (without the game data): a spacehog/ folder holding the
# binary, docs, licences and VERSION.txt. Used by package.sh and by CI.
# usage: scripts/pack.sh linux|windows|macos BINARY OUTDIR VERSION
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OS=$1 BIN=$2 OUT=$3 VER=$4
NAME=spacehog-$OS-x86_64
[ "$OS" = macos ] && NAME=spacehog-macos-universal
mkdir -p "$OUT"; OUT=$(cd "$OUT" && pwd)
STAGE=$OUT/$NAME; rm -rf "$STAGE"
d=$STAGE/spacehog
mkdir -p "$d"
cp "$BIN" "$d/"
cp "$ROOT/README.md" "$ROOT/CHANGELOG.md" "$ROOT/LICENSE" "$ROOT/THIRD_PARTY.txt" "$d/"
echo "$VER" > "$d/VERSION.txt"
cp "$ROOT/src/third_party/nuked_opl3/LICENSE" "$d/LICENSE-NukedOPL3.txt"
if [ -n "$WARPZONE_DIR" ]; then
   cp "$WARPZONE_DIR/LICENSE-MIT" "$d/LICENSE-libwarpzone.txt"
   cp "$WARPZONE_DIR/THIRD-PARTY-NOTICES.html" "$d/LICENSE-libwarpzone-third-party.html"
fi
printf 'Copy SPACEHOG.DAT, SHLEV.DAT, SHBGS.DAT and SHSND.DAT from the original\nOperation Spacehog (freeware) into this directory, then start the game.\n' > "$d/PUT_GAME_FILES_HERE.txt"
if [ "$OS" = macos ]; then
   strip "$d/spacehog"
   codesign --force -s - "$d/spacehog"   # arm64 runs only signed code (ad hoc is enough)
   # read me first: Gatekeeper blocks unsigned downloads (README "macOS")
   printf 'This build is not signed by Apple. If macOS says it cannot be opened, run once:\n  xattr -dr com.apple.quarantine /path/to/this/spacehog/folder\nor Control-click spacehog, choose Open and confirm.\n' > "$d/MACOS_FIRST_START.txt"
   tar -C "$STAGE" -czf "$OUT/$NAME.tar.gz" spacehog
elif [ "$OS" = linux ]; then
   strip "$d/spacehog"
   tar -C "$STAGE" -czf "$OUT/$NAME.tar.gz" spacehog
else
   ${STRIP:-llvm-strip} "$d/spacehog.exe"
   (cd "$STAGE" && python3 -c "import shutil, sys; shutil.make_archive(sys.argv[1], 'zip', '.', 'spacehog')" "$OUT/$NAME")
fi
