#!/bin/bash
# Start (or reuse) this project's own Xvfb. Never reuses a display we did not start.
# usage: xvfb.sh [display]  -> prints the display string, e.g. ":93"
source "$(dirname "$0")/../../tools/env.sh"
D=${1:-93}
PIDF="$SH_ROOT/scratch/xvfb$D.pid"
if [ -f "$PIDF" ] && kill -0 "$(cat "$PIDF")" 2>/dev/null && grep -q Xvfb /proc/"$(cat "$PIDF")"/cmdline 2>/dev/null; then
  echo ":$D"; exit 0
fi
if [ -e /tmp/.X11-unix/X$D ]; then
  echo "display :$D is in use by someone else; pick another" >&2; exit 1
fi
Xvfb :$D -screen 0 1280x960x24 -nolisten tcp >"$SH_ROOT/scratch/xvfb$D.log" 2>&1 &
echo $! > "$PIDF"
for i in $(seq 50); do [ -e /tmp/.X11-unix/X$D ] && break; sleep 0.1; done
echo ":$D"
