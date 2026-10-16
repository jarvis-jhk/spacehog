#!/bin/bash
# Differential test: run the port with a replay, record the sync log, then run the
# original game code in the emulator against it.
# usage: re/refemu/difftest.sh REPLAY [--dump A-B] [--sav FILE] [--keep-going] [-- game args]
#   --sav FILE: initial SPACEHOG.SAV for both sides (re/refemu/mksav.py)
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
source "$ROOT/tools/env.sh"
REPLAY=$1; shift
DUMP=; SAV=; EMUOPT=()
while [ $# -gt 0 ]; do
   case $1 in
      --dump) DUMP=$2; shift 2 ;;
      --sav) SAV=$2; shift 2 ;;
      --) shift; break ;;
      *) EMUOPT+=("$1"); shift ;;
   esac
done
OUT=${DIFFTEST_OUT:-$ROOT/scratch/difftest}
rm -rf "$OUT"; mkdir -p "$OUT/port_save" "$OUT/emu_save"
[ -n "$SAV" ] && cp "$SAV" "$OUT/port_save/SPACEHOG.SAV" && cp "$SAV" "$OUT/emu_save/SPACEHOG.SAV"
"$ROOT/build/spacehog" --data "$ROOT/orig/dist/game" --save-dir "$OUT/port_save" --headless --fast \
   --replay "$REPLAY" --sync-log "$OUT/port.sync" ${DUMP:+--sync-dump $DUMP} "$@" >"$OUT/port.out" 2>&1 || true
GAMEARGS=(); for a in "$@"; do GAMEARGS+=(--arg "$a"); done
python3 "$ROOT/re/refemu/refemu.py" "$OUT/port.sync" --save-dir "$OUT/emu_save" "${EMUOPT[@]}" "${GAMEARGS[@]}"
