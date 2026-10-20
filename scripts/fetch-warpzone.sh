#!/bin/bash
# Download and unpack the pinned libwarpzone release package (online co-op).
# usage: scripts/fetch-warpzone.sh linux|windows DESTDIR   -> prints the package dir
set -e
OS=$1 DEST=$2
VER=0.1.10
case $OS in
   linux)   F=libwarpzone-$VER-x86_64-linux-gnu.tar.gz SUM=41e39c51657cb1de2b43252c9e6400afae1d2329502ddfa542e005f6b6bb9189 ;;
   windows) F=libwarpzone-$VER-x86_64-windows-gnu.zip SUM=15ce3fc96578a1ba720f5bebb8ebd227fe9d9416286aa226feea76766d05f8b4 ;;
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
