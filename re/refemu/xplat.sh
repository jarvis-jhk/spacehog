#!/bin/bash
# Cross-platform determinism: the Linux build and the Windows build (under Wine) must write
# byte-identical sync logs (state, draw calls, frames) for the same replay.
# usage: re/refemu/xplat.sh [REPLAY] [-- game args]   (default: full game with /judge)
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
source "$ROOT/tools/env.sh"
REPLAY=${1:-$ROOT/re/refemu/replays/fullgame_judge.rec}; shift
[ "$1" = "--" ] && shift
[ $# -eq 0 ] && [ -z "$2" ] && set -- /judge
OUT=$ROOT/scratch/xplat; rm -rf "$OUT"; mkdir -p "$OUT/lin" "$OUT/win"
w() { echo "Z:$(realpath "$1" | sed 's|/|\\|g')"; }
"$SH_BUILD/spacehog" "$@" --data "$ROOT/orig/dist/game" --save-dir "$OUT/lin" --headless --fast \
   --replay "$REPLAY" --sync-log "$OUT/lin.sync" >/dev/null 2>&1
WINEDLLOVERRIDES="mscoree,mshtml=" wine "$ROOT/build-win/spacehog.exe" "$@" --data "$(w "$ROOT/orig/dist/game")" \
   --save-dir "$(w "$OUT/win")" --headless --fast --replay "$(w "$REPLAY")" --sync-log "$(w "$OUT")\\win.sync" >/dev/null 2>&1
if cmp -s "$OUT/lin.sync" "$OUT/win.sync"; then echo "identical ($(stat -c %s "$OUT/lin.sync") bytes)"; else echo "DIFFERENT"; cmp "$OUT/lin.sync" "$OUT/win.sync"; exit 1; fi
