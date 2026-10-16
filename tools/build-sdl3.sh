#!/bin/bash
# Build static SDL3 for Linux into tools/sdl3-linux using -dev headers extracted to tools/sysroot.
set -e
source "$(dirname "$0")/env.sh"
T=$SH_ROOT/tools; S=$T/sysroot
mkdir -p $T/debs && cd $T/debs
apt-get download libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxfixes-dev libxss-dev \
  libxtst-dev x11proto-dev libxrender-dev libxkbcommon-dev libpulse-dev libasound2-dev libpipewire-0.3-dev \
  libspa-0.2-dev libudev-dev libdbus-1-dev libegl-dev libgl-dev libgles-dev libdrm-dev libgbm-dev \
  libxcb1-dev libxau-dev libxdmcp-dev libglx-dev libffi-dev
for f in *.deb; do dpkg -x "$f" $S; done
# point dangling -dev .so symlinks at the host's runtime libraries
for f in $S/usr/lib/x86_64-linux-gnu/*.so; do
  if [ -L "$f" ] && [ ! -e "$f" ]; then b=$(basename "$(readlink "$f")"); [ -e /usr/lib/x86_64-linux-gnu/$b ] && ln -sf /usr/lib/x86_64-linux-gnu/$b "$f"; fi
done
export PKG_CONFIG_SYSROOT_DIR=$S PKG_CONFIG_LIBDIR=$S/usr/lib/x86_64-linux-gnu/pkgconfig:$S/usr/share/pkgconfig
cmake -S $T/SDL3-3.4.18 -B $T/build-sdl3 -G Ninja -DCMAKE_BUILD_TYPE=Release -DSDL_SHARED=OFF -DSDL_STATIC=ON \
  -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF -DSDL_WAYLAND=OFF -DCMAKE_INSTALL_PREFIX=$T/sdl3-linux -DCMAKE_PREFIX_PATH=$S/usr \
  -DCMAKE_C_FLAGS="-I$S/usr/include -I$S/usr/lib/x86_64-linux-gnu/dbus-1.0/include"
cmake --build $T/build-sdl3 -j"$(nproc)"
cmake --install $T/build-sdl3
