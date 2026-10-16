#!/bin/bash
# Re-create the local toolchain in tools/ (nothing is installed system-wide).
set -e
cd "$(dirname "$0")"
mkdir -p dl bin
cd dl
xargs -P 6 -n 1 curl -sSLOC - < ../urls.txt
curl -sSLO https://liballeg.org/files/alleg311.zip
cd ..
for f in dl/*.tar.*; do tar xf "$f"; done
unzip -qo dl/ghidra_*.zip
mkdir -p SDL3-win && unzip -qo dl/SDL3-*-win32-x64.zip -d SDL3-win
unzip -qo dl/ninja-linux.zip -d bin
ln -sf ../upx-5.2.1-amd64_linux/upx bin/upx
mkdir -p ../re/allegro311 && unzip -qo dl/alleg311.zip -d ../re/allegro311
# Xvfb from the distro package, extracted locally (needs xserver-common, xkb utils from the host)
mkdir -p debs && (cd debs && apt-get download xvfb) && dpkg -x debs/xvfb_*.deb sysroot && ln -sf ../sysroot/usr/bin/Xvfb bin/Xvfb
mkdir -p debs && (cd debs && apt-get download valgrind) && mkdir -p valgrind && dpkg -x debs/valgrind_*.deb valgrind   # uninitialized-read checks: VALGRIND_LIB=tools/valgrind/usr/libexec/valgrind
python3 -m venv --without-pip venv
PIP_CACHE_DIR="$PWD/home/.cache/pip" pip --python venv/bin/python install -q unicorn capstone pillow numpy
