#!/bin/bash
# Fuzz campaign: random replays (fuzzrec.py) in several configurations, run in parallel.
# usage: re/refemu/fuzz.sh FIRST_SEED LAST_SEED [MINUTES]
# Results: scratch/fuzz/<config>-<seed>/result.txt; failures are listed at the end.
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
source "$ROOT/tools/env.sh"
A=$1; B=$2; MIN=${3:-6}
F=$ROOT/scratch/fuzz
mkdir -p "$F"
python3 "$ROOT/re/refemu/mksav.py" "$F/kb2.sav" --two 1
python3 "$ROOT/re/refemu/mksav.py" "$F/pad1.sav" --p1 1
python3 "$ROOT/re/refemu/mksav.py" "$F/pad2.sav" --two 1 --p1 1
python3 "$ROOT/re/refemu/mksav.py" "$F/easy.sav" --two 1 --difficulty 1
jobs=()
for s in $(seq "$A" "$B"); do
   jobs+=("kb2 $s --players 2" "kb1 $s --players 1" "pad1 $s --players 1 --pad" "pad2 $s --players 2 --pad" "easy $s --players 2")
done
run() {
   set -- $1
   cfg=$1; seed=$2; shift 2
   d=$F/$cfg-$seed; rm -rf "$d"; mkdir -p "$d"
   python3 "$ROOT/re/refemu/fuzzrec.py" "$d/in.rec" --seed "$seed" --minutes "$MIN" "$@"
   sav=; case $cfg in kb2|pad1|pad2|easy) sav="--sav $F/$cfg.sav" ;; esac
   DIFFTEST_OUT=$d/run "$ROOT/re/refemu/difftest.sh" "$d/in.rec" $sav --stall 20 $REFEMU_OPTS > "$d/result.txt" 2>&1
   echo "$cfg-$seed: $(grep '^\[refemu\]' "$d/result.txt" | head -1)"
}
export -f run; export F ROOT MIN REFEMU_OPTS
printf '%s\n' "${jobs[@]}" | xargs -P "$(nproc)" -I{} bash -c 'run "{}"'
