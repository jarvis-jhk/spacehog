#!/bin/bash
# Send key presses to the first window matching NAME on DISPLAY.
# usage: DISPLAY=:93 keys.sh NAME key[:hold_ms] ... ; "sleep:ms" pauses, "shot:file.png" grabs the screen
NAME=$1; shift
case "$NAME" in class:*) W=$(xdotool search --class "${NAME#class:}" | head -1);; *) W=$(xdotool search --name "$NAME" | head -1);; esac
[ -n "$W" ] || { echo "no window $NAME" >&2; exit 1; }
xdotool windowfocus --sync "$W" 2>/dev/null
[ -n "$CLICK_FOCUS" ] && { xdotool mousemove --window "$W" 300 300 click 1; sleep 0.5; }
for k in "$@"; do
  case "$k" in
    sleep:*) sleep "$(echo "scale=3; ${k#sleep:}/1000" | bc)";;
    shot:*) python3 -c "
from PIL import ImageGrab; import os
ImageGrab.grab(xdisplay=os.environ['DISPLAY']).save('${k#shot:}')" 2>/dev/null || import -display "$DISPLAY" -window root "${k#shot:}";;
    *:*) xdotool keydown "${k%%:*}"; sleep "$(echo "scale=3; ${k##*:}/1000" | bc)"; xdotool keyup "${k%%:*}";;
    *) xdotool keydown "$k"; sleep 0.08; xdotool keyup "$k";;
  esac
done
