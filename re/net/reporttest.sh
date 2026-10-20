#!/bin/bash
# Fault reports (src/platform/report.c) against a local Warpzone server: a data file that
# cannot be read, a short SPACEHOG.SAV, and a crash (SIGSEGV) that the next start sends.
# Passes if the server's feedback table holds exactly those reports, with stable titles
# and no paths, and the crash file is gone after it was sent.
# usage: re/net/reporttest.sh   (needs $SH_BUILD with Warpzone, like nettest.sh)
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
source "$ROOT/tools/env.sh"
BIN=${NETTEST_BIN:-$SH_BUILD/spacehog}
DATA=$ROOT/orig/dist/game
OUT=$ROOT/scratch/reporttest; rm -rf "$OUT"; mkdir -p "$OUT"
SRV=$ROOT/tools/warpzone/warpzone-server-0.1.10-x86_64-linux-musl
[ -x "$SRV" ] || { echo "run re/net/nettest.sh once (fetches the server)"; exit 2; }
PORT=3517 UPORT=3518
S=$OUT/srv; mkdir -p "$S/games" "$S/data"; touch "$S/staff.toml"
cat > "$S/server.toml" <<EOF
name = "spacehog reporttest"
public_url = "http://127.0.0.1:$PORT"
http_listen = "127.0.0.1:$PORT"
udp_listen = "127.0.0.1:$UPORT"
data_dir = "data"
games_dir = "games"
staff_file = "staff.toml"
EOF
printf 'id = "spacehog"\nname = "Operation Spacehog"\ntagline = "test"\ntier = 1\n' > "$S/games/spacehog.toml"
(cd "$S" && exec "$SRV" --config server.toml > server.log 2>&1) &
SRVPID=$!
trap 'kill $SRVPID 2>/dev/null || true' EXIT
export WARPZONE_SERVER=http://127.0.0.1:$PORT SPACEHOG_REPORTS=1
for i in $(seq 1 50); do curl -fs -o /dev/null "$WARPZONE_SERVER/api/v1/games" && break; sleep 0.1; done
ulimit -c 0

# 1. a damaged SHLEV.DAT (present, wrong contents): reported; then the game exits
mkdir -p "$OUT/bad" "$OUT/s1"
cp "$DATA"/*.DAT "$OUT/bad/"
head -c 5000 /dev/zero > "$OUT/bad/SHLEV.DAT"
"$BIN" --data "$OUT/bad" --save-dir "$OUT/s1" --headless --fast --max-seconds 5 > "$OUT/bad.log" 2>&1 || true
# 2. a short SPACEHOG.SAV
mkdir -p "$OUT/s2"; head -c 100 /dev/zero > "$OUT/s2/SPACEHOG.SAV"
"$BIN" --data "$DATA" --save-dir "$OUT/s2" --headless --fast --max-seconds 2 > "$OUT/sav.log" 2>&1 || true
# 3. a crash: the handler writes crash-report.txt; the next start sends and deletes it
mkdir -p "$OUT/s3"
"$BIN" --data "$DATA" --save-dir "$OUT/s3" --headless --max-seconds 30 > "$OUT/crash.log" 2>&1 &
P=$!; sleep 1.5; kill -SEGV $P; wait $P 2>/dev/null || true
cp "$OUT/s3/crash-report.txt" "$OUT/crash-file.txt" 2>/dev/null || true
"$BIN" --data "$DATA" --save-dir "$OUT/s3" --headless --fast --max-seconds 2 > "$OUT/after.log" 2>&1 || true
# 4. a normal run reports nothing
mkdir -p "$OUT/s4"
"$BIN" --data "$DATA" --save-dir "$OUT/s4" --headless --fast --max-seconds 2 > "$OUT/clean.log" 2>&1 || true
sleep 0.5

python3 - "$S/data/warpzone.db" > "$OUT/feedback.txt" <<'EOF'
import sqlite3, sys
for r in sqlite3.connect(sys.argv[1]).execute('SELECT game, kind, title, version, count, detail FROM feedback ORDER BY id'):
    print('\t'.join(str(x).replace('\n', '|') for x in r))
EOF
cat "$OUT/feedback.txt"
fail=0
check() { if eval "$2"; then echo "ok   $1"; else echo "FAIL $1"; fail=1; fi; }
check "data file report" "grep -q \$'^spacehog\tbug\tdata file not readable\t.*SHLEV.DAT, 5000 bytes' '$OUT/feedback.txt'"
check "short save report" "grep -q \$'^spacehog\tbug\tSPACEHOG.SAV is short\t.*100 of 354 bytes' '$OUT/feedback.txt'"
check "crash file written" "grep -q '^spacehog-crash 1' '$OUT/crash-file.txt'"
check "crash report" "grep -q \$'^spacehog\tcrash\tcrash: SIGSEGV at ' '$OUT/feedback.txt'"
check "crash file deleted after sending" "[ ! -e '$OUT/s3/crash-report.txt' ]"
check "nothing else reported" "[ \$(wc -l < '$OUT/feedback.txt') -eq 3 ]"
check "no paths in reports" "! grep -q '/' '$OUT/feedback.txt'"
exit $fail
