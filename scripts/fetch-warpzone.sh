#!/bin/bash
# Download and unpack the pinned libwarpzone release package (online co-op).
# usage: scripts/fetch-warpzone.sh linux|windows DESTDIR   -> prints the package dir
set -e
OS=$1 DEST=$2
VER=0.1.2
case $OS in
   linux)   F=libwarpzone-$VER-x86_64-linux-gnu.tar.gz SUM=597cdee6a7ac22d37bf1bec7234f7709673fc807de1dce235e8165e6ef28e5c0 ;;
   windows) F=libwarpzone-$VER-x86_64-windows-gnu.zip SUM=af53fb4551ea498615963e8d604627a612dd3f3f22b38bdf089db3ef658dc24f ;;
   *) echo "usage: $0 linux|windows DESTDIR" >&2; exit 2 ;;
esac
mkdir -p "$DEST"; cd "$DEST"
D=${F%.tar.gz}; D=${D%.zip}
if [ ! -f "$D/.ok" ]; then
   [ -f "$F" ] || curl -fsSL -o "$F" "https://github.com/jarvis-jhk/warpzone/releases/download/v$VER/$F"
   echo "$SUM  $F" | sha256sum -c - >&2
   rm -rf "$D"
   case $F in
      *.zip) python3 -c 'import sys, zipfile; zipfile.ZipFile(sys.argv[1]).extractall(".")' "$F" ;;
      *) tar xzf "$F" ;;
   esac
   touch "$D/.ok"
fi
echo "$DEST/$D"
