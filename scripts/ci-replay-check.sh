#!/bin/bash
# Cross-platform determinism in CI: a built binary replays the recordings in
# re/refemu/replays to sync logs (state, draw calls, frames) with exactly the hashes in
# re/refemu/replays/sync.sha256, which come from the build that passed re/refemu/regress.sh
# against the original. The game data is the freeware release from the DOS Games
# Archive, downloaded here (pinned by checksum) and never packed or uploaded.
# When the game's behaviour changes on purpose: run regress.sh, then
#   REPLAY_CHECK_UPDATE=1 scripts/ci-replay-check.sh build-w/spacehog   (rewrites the hashes)
# usage: scripts/ci-replay-check.sh BINARY [WRAPPER...]   (e.g. arch -x86_64)
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BIN=$1; shift
ZIP_URL=https://image.dosgamesarchive.com/games/sh_dos.zip
ZIP_SUM=4e3b15c2ca24f2706cb9f805e44b2a6427ac84c60faef068904bc1e98d0a34f3
sha() { if command -v sha256sum >/dev/null; then sha256sum "$@"; else shasum -a 256 "$@"; fi; }
DL=$ROOT/ci-cache/gamedata; mkdir -p "$DL"
if [ ! -f "$DL/game/SPACEHOG.DAT" ]; then
   [ -f "$DL/sh_dos.zip" ] || curl -fsSL --retry 3 -o "$DL/sh_dos.zip" "$ZIP_URL"
   [ "$(sha "$DL/sh_dos.zip" | cut -d' ' -f1)" = $ZIP_SUM ] || { echo "sh_dos.zip: checksum mismatch" >&2; exit 1; }
   python3 - "$DL" <<'EOF'
import io, sys, zipfile
d = sys.argv[1]
inner = zipfile.ZipFile(d + '/sh_dos.zip').read('spacehog.zip')
zipfile.ZipFile(io.BytesIO(inner)).extractall(d + '/game')
EOF
fi
OUT=$ROOT/ci-cache/replay-check; rm -rf "$OUT"; mkdir -p "$OUT"
fail=0 new=
while read -r want name args; do
   mkdir -p "$OUT/$name"
   # shellcheck disable=SC2086
   "$@" "$BIN" $args --data "$DL/game" --save-dir "$OUT/$name" --headless --fast \
      --replay "$ROOT/re/refemu/replays/$name.rec" --sync-log "$OUT/$name.sync" > "$OUT/$name.log" 2>&1 || true
   got=$(sha "$OUT/$name.sync" 2>/dev/null | cut -d' ' -f1)
   new+="$got $name${args:+ $args}"$'\n'
   if [ -n "$REPLAY_CHECK_UPDATE" ]; then echo "$name: $got"
   elif [ "$got" = "$want" ]; then echo "ok   $name"; else echo "FAIL $name: sync log $got, expected $want"; tail -5 "$OUT/$name.log"; fail=1; fi
done < "$ROOT/re/refemu/replays/sync.sha256"
[ -n "$REPLAY_CHECK_UPDATE" ] && { printf '%s' "$new" > "$ROOT/re/refemu/replays/sync.sha256"; exit 0; }
exit $fail
