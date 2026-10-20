#!/bin/bash
# Online co-op test: a local Warpzone server, host + guest as two headless processes, each
# fed its half of a randomized 2-player session (fuzzrec.py + splitrec.py). Passes if both
# machines write identical sync logs and recordings, no desync is reported, and the merged
# recording matches the emulated original (refemu) and replays offline to the same log.
# usage: re/net/nettest.sh [--seed N] [--minutes M] [--pad] [--realtime] [--delay N]
# Needs a build with Warpzone ($SH_BUILD configured with -DWARPZONE_DIR; README "Building").
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
source "$ROOT/tools/env.sh"
SEED=1 MIN=2 PAD= FAST=--fast DELAY=0
while [ $# -gt 0 ]; do
   case $1 in
      --seed) SEED=$2; shift 2 ;;
      --minutes) MIN=$2; shift 2 ;;
      --pad) PAD=--pad; shift ;;
      --realtime) FAST=; shift ;;
      --delay) DELAY=$2; shift 2 ;;
      *) echo "unknown option $1" >&2; exit 2 ;;
   esac
done
BIN=${NETTEST_BIN:-$SH_BUILD/spacehog}
DATA=$ROOT/orig/dist/game
OUT=$ROOT/scratch/nettest/run-$SEED$PAD; rm -rf "$OUT"; mkdir -p "$OUT/hs" "$OUT/gs"
BASE=$ROOT/scratch/nettest

# pinned Warpzone server (release asset, checksum from its SHA256SUMS)
SRV=$ROOT/tools/warpzone/warpzone-server-0.1.11-x86_64-linux-musl
if [ ! -x "$SRV" ]; then
   mkdir -p "$(dirname "$SRV")"
   curl -fsSL -o "$SRV.$$" https://github.com/jarvis-jhk/warpzone/releases/download/v0.1.11/warpzone-server-0.1.11-x86_64-linux-musl
   echo "8b1d8498bae81573d2ae68b5571ac1c63ef028952b48a63c3b36a42a55532f66  $SRV.$$" | sha256sum -c - >/dev/null
   chmod +x "$SRV.$$"; mv "$SRV.$$" "$SRV"
fi
PORT=$((3100 + SEED % 400)) UPORT=$((3600 + SEED % 400))
S=$OUT/srv; mkdir -p "$S/games" "$S/data"; touch "$S/staff.toml"
cat > "$S/server.toml" <<EOF
name = "spacehog nettest"
public_url = "http://127.0.0.1:$PORT"
http_listen = "127.0.0.1:$PORT"
udp_listen = "127.0.0.1:$UPORT"
data_dir = "data"
games_dir = "games"
staff_file = "staff.toml"
EOF
cat > "$S/games/spacehog.toml" <<'EOF'
id = "spacehog"
name = "Operation Spacehog"
tagline = "test"
tier = 1
[install]
github = "jarvis-jhk/spacehog"
assets = { linux = "spacehog-linux-x86_64.tar.gz" }
[launch]
exec = { linux = "spacehog/spacehog" }
join = ["--warpzone-join", "{room}"]
host = ["--warpzone-host"]
[rooms]
max_players = 2
presets = [{ id = "coop", label = "co-op" }]
EOF
(cd "$S" && exec "$SRV" --config server.toml > server.log 2>&1) &
SRVPID=$!
trap 'kill $SRVPID 2>/dev/null || true' EXIT
export WARPZONE_SERVER=http://127.0.0.1:$PORT
for i in $(seq 1 50); do curl -fs -o /dev/null "$WARPZONE_SERVER/api/v1/games" && break; sleep 0.1; done

# inputs: one 2-player session split into the two machines' halves
python3 "$ROOT/re/refemu/fuzzrec.py" "$OUT/all.rec" --seed "$SEED" --minutes "$MIN" --players 2 $PAD
python3 "$ROOT/re/net/splitrec.py" "$OUT/all.rec" "$OUT/host.rec" "$OUT/guest.rec" --seed "$SEED"
python3 "$ROOT/re/refemu/mksav.py" "$OUT/start.sav" --two 1 ${PAD:+--p1 1}
cp "$OUT/start.sav" "$OUT/hs/SPACEHOG.SAV"

# each server has its own accounts: fresh identities per run
run() {  # who home args...
   local who=$1 home=$2; shift 2
   WARPZONE_HOME=$home timeout 900 "$BIN" --data "$DATA" --save-dir "$OUT/${who:0:1}s" --headless $FAST \
      --warpzone-name "nt${who:0:1}$SEED$RANDOM" --replay "$OUT/$who.rec" --record "$OUT/$who-merged.rec" \
      --sync-log "$OUT/$who.sync" "$@" > "$OUT/$who.log" 2>&1
}
run host "$OUT/home-h" --warpzone-host --warpzone-private --net-delay "$DELAY" &
HOSTPID=$!
for i in $(seq 1 200); do grep -q "Code:" "$OUT/host.log" 2>/dev/null && break; sleep 0.1; done
CODE=$(grep -o "Code: [A-Z0-9]*" "$OUT/host.log" | head -1 | cut -d' ' -f2)
[ -n "$CODE" ] || { echo "FAIL: host opened no room"; cat "$OUT/host.log"; exit 1; }
run guest "$OUT/home-g" --warpzone-code "$CODE" || true
wait $HOSTPID || true

fail=0
check() { if eval "$2"; then echo "ok   $1"; else echo "FAIL $1"; fail=1; fi; }
check "both machines in the session" "grep -q 'netplay: started' '$OUT/host.log' && grep -q 'netplay: started' '$OUT/guest.log'"
check "no desync reported" "! grep -q DESYNC '$OUT/host.log' '$OUT/guest.log'"
check "identical sync logs" "cmp -s '$OUT/host.sync' '$OUT/guest.sync'"
check "identical recordings" "cmp -s '$OUT/host-merged.rec' '$OUT/guest-merged.rec'"
check "player 2 got input" "grep -q ' k 17 1\| k 31 1\| k 35 1' '$OUT/host-merged.rec'"
DIFFTEST_OUT=$OUT/difftest SH_BUILD=$(dirname "$BIN") "$ROOT/re/refemu/difftest.sh" "$OUT/host-merged.rec" --sav "$OUT/start.sav" > "$OUT/difftest.txt" 2>&1 || true
check "merged recording = original (refemu)" "grep -q '^\[refemu\].*no divergence' '$OUT/difftest.txt'"
check "offline replay = session" "cmp -s '$OUT/difftest/port.sync' '$OUT/host.sync'"
echo "$(grep -c . "$OUT/host-merged.rec") events, $(grep '^\[refemu\]' "$OUT/difftest.txt" | head -1)"
exit $fail
