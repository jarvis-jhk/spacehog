#!/bin/bash
# Download and unpack the pinned libwarpzone release package (online co-op).
# usage: scripts/fetch-warpzone.sh linux|windows|macos DESTDIR   -> prints the package dir
set -e
OS=$1 DEST=$2
VER=0.1.11
case $OS in
   linux)   F=libwarpzone-$VER-x86_64-linux-gnu.tar.gz SUM=c8d20a02b4f63143e4d33179ecec25741789b3488098deb44feaaf748c7d32a1 ;;
   windows) F=libwarpzone-$VER-x86_64-windows-gnu.zip SUM=8bccfc2d2e5072c9898f2f13585248af577da689a21c639c0f621c1dc4a7b8bf ;;
   macos)   F=libwarpzone-$VER-universal-apple-darwin.tar.gz SUM=0c28b0df943ff1430b808745dfaf09372ea28fd6d254bf020abc312aa0795a93 ;;
   *) echo "usage: $0 linux|windows|macos DESTDIR" >&2; exit 2 ;;
esac
mkdir -p "$DEST"; cd "$DEST"
D=${F%.tar.gz}; D=${D%.zip}
if [ ! -f "$D/.ok" ]; then
   [ -f "$F" ] || curl -fsSL -o "$F" "https://github.com/jarvis-jhk/warpzone/releases/download/v$VER/$F"
   echo "$SUM  $F" | shasum -a 256 -c - >&2
   rm -rf "$D"
   case $F in
      *.zip) python3 -c 'import sys, zipfile; zipfile.ZipFile(sys.argv[1]).extractall(".")' "$F" ;;
      *) tar xzf "$F" ;;
   esac
   touch "$D/.ok"
fi
echo "$DEST/$D"
