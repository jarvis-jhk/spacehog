#!/bin/bash
# Regression set for refactoring: full game (with /judge cheats) + a few fuzz replays,
# all against the emulated original, in parallel. Prints one line per run.
# usage: [REFEMU_OPTS=--pixels] re/refemu/regress.sh [MORE_SEEDS...]
# Exits non-zero if any run diverges.
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
source "$ROOT/tools/env.sh"
OUT=$ROOT/scratch/regress
rm -rf "$OUT"; mkdir -p "$OUT"
python3 "$ROOT/re/refemu/mksav.py" "$OUT/kb2.sav" --two 1
python3 "$ROOT/re/refemu/mksav.py" "$OUT/pad2.sav" --two 1 --p1 1
python3 "$ROOT/re/refemu/mksav.py" "$OUT/easy.sav" --two 1 --difficulty 1
python3 "$ROOT/re/refemu/mksav.py" "$OUT/low.sav" --two 1 --p1 1 --low-scores
python3 "$ROOT/re/refemu/mksav.py" "$OUT/mouse.sav" --two 1 --p2 2
# at most one run per CPU of the quota (oversubscribed runs stall: "no sync for 10s")
J=${REGRESS_JOBS:-$(nproc)}
if read -r q per < /sys/fs/cgroup/cpu.max 2>/dev/null && [ "$q" != max ]; then
   [ $((q / per)) -ge 1 ] && [ $((q / per)) -lt "$J" ] && J=$((q / per))
fi
run() {
   name=$1; rec=$2; shift 2
   DIFFTEST_OUT=$OUT/$name "$ROOT/re/refemu/difftest.sh" "$rec" $REFEMU_OPTS "$@" > "$OUT/$name.txt" 2>&1
   echo "$name: $(grep '^\[refemu\]' "$OUT/$name.txt" | head -1)"
}
bg() {
   while [ "$(jobs -rp | wc -l)" -ge "$J" ]; do wait -n; done
   run "$@" &
}
bg fullgame "$ROOT/re/refemu/replays/fullgame_judge.rec" -- /judge
for s in 1 3 5 "$@"; do
   python3 "$ROOT/re/refemu/fuzzrec.py" "$OUT/kb2-$s.rec" --seed "$s" --minutes 6 --players 2
   python3 "$ROOT/re/refemu/fuzzrec.py" "$OUT/pad2-$s.rec" --seed "$s" --minutes 6 --players 2 --pad
   bg kb2-$s "$OUT/kb2-$s.rec" --sav "$OUT/kb2.sav"
   bg pad2-$s "$OUT/pad2-$s.rec" --sav "$OUT/pad2.sav"
done
# seed 24, 8 min: a level ends while both players are dead (uninitialized pilot texts)
python3 "$ROOT/re/refemu/fuzzrec.py" "$OUT/pad2-24.rec" --seed 24 --minutes 8 --players 2 --pad
bg pad2-24 "$OUT/pad2-24.rec" --sav "$OUT/pad2.sav"
# zero high scores: name entry (keyboard + gamepad letter picker)
python3 "$ROOT/re/refemu/fuzzrec.py" "$OUT/names.rec" --seed 31 --minutes 6 --players 2 --pad
bg names "$OUT/names.rec" --sav "$OUT/low.sav"
# player 2 on mouse control (player_input's mouse fields)
python3 "$ROOT/re/refemu/fuzzrec.py" "$OUT/mouse.rec" --seed 1 --minutes 6 --players 2
bg mouse "$OUT/mouse.rec" --sav "$OUT/mouse.sav"
python3 "$ROOT/re/refemu/fuzzrec.py" "$OUT/easy.rec" --seed 7 --minutes 6 --players 2
bg easy "$OUT/easy.rec" --sav "$OUT/easy.sav"
wait
# exit status: 0 only if every run reached its end without a divergence
fail=0
for f in "$OUT"/*.txt; do grep -q '^\[refemu\].*no divergence' "$f" || { echo "FAILED: $(basename "$f" .txt)"; fail=1; }; done
exit $fail
