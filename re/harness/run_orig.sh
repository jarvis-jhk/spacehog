#!/bin/bash
# Run the original game in DOSBox Staging on a headless Xvfb display.
# usage: run_orig.sh [display-number]   (default 93). Game copy lives in orig/run (writable).
set -e
source "$(dirname "$0")/../../tools/env.sh"
D=${1:-93}
mkdir -p "$SH_ROOT/orig/run"
[ -f "$SH_ROOT/orig/run/SH.EXE" ] || { cp "$SH_ROOT"/orig/dist/game/* "$SH_ROOT/orig/run/"; cp "$SH_ROOT/orig/dist/SPACEHOG.SAV" "$SH_ROOT/orig/run/"; }
"$SH_ROOT/re/harness/xvfb.sh" $D >/dev/null || exit 1
cd "$SH_ROOT/orig/run"
export DISPLAY=:$D SDL_AUDIODRIVER=${SDL_AUDIODRIVER:-dummy}
exec "$SH_ROOT/tools/dosbox-staging-linux-x86_64-0.83.0-7b400/dosbox" --noprimaryconf --nolocalconf -conf "$SH_ROOT/re/harness/orig.conf"
