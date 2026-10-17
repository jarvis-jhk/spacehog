# Maintainer notes (JARVIS)

Working log for things Jan does not need to see. Newest first.

## 2026-10-17 — Warpzone lockstep co-op (roadmap `spacehog-warpzone-lockstep-co-op`)

- Design: lockstep on the raw input stream (keys/mouse/pads per 70 Hz frame of the emulated
  clock), not on player_input — the menus read key[] directly, and this keeps every screen
  deterministic with no game-code changes. Details in PLAN.md "Netplay".
- libwarpzone 0.1.2 pinned in `scripts/fetch-warpzone.sh` (sha256); CMake `-DWARPZONE_DIR`;
  CI fetches it. Worker: `build-w` is now configured with it (tools/warpzone/), so
  regress.sh runs on the release configuration.
- `re/net/nettest.sh [--seed N] [--pad] [--realtime] [--minutes M] [--delay N]`: own local
  warpzone-server (pinned), host + guest headless, each with half of a fuzz session.
  Passed: seeds 11 (3 min), 12 --pad, 13 (6 min, delay 2), 21 --realtime (auto delay);
  also by hand: join by room id, invite link, guest SIGTERM (host plays on at once) and
  SIGKILL (host waits ~15 s for Warpzone's peer timeout, then plays on).
  Every run: identical sync logs + recordings on both sides, merged recording = original
  in refemu, offline replay byte-identical to the session.
- Server quirks: one identity may host only one room at a time; registrations are
  rate-limited (nettest registers fresh names on its own fresh server).
- Not verified here: the Windows build at run time (links with llvm-mingw; wine does not
  run in the worker), real networks/relay, the window UI of the connect screen.
- Open test for Jan (not a block): two machines, one `--warpzone-host` (or Host in the
  launcher once the descriptor PR is merged), the other joins; play a level together.
- Warpzone descriptor: tier 2 with join/host, max_players 2, mods/leaderboards sections
  removed until implemented — PR to jarvis-jhk/warpzone (🌀 lane merges).
- Later: in-game room list and username prompt (now: launcher identity or
  `--warpzone-name`), 4 players, fault reports for desyncs.

## 2026-10-17 — per-tick player input (roadmap `spacehog-per-tick-player-input`)

- `src/game/pinput.c`: every per-player device read in game.c/render.c (keys via g_keys,
  pjoy, mouse_b/x/y, Enter in ship select) goes through `player_input(p)`. Default = live
  sample at each read → same read points as the original; regress.sh bit-exact (now 11
  runs: added `mouse`, player 2 on mouse control).
- `--input-latch`: latched once per tick in `wait_tick()`; deterministic (two replays →
  identical sync logs), intentionally not original-timed (not difftestable).
- Not in the layer (system keys): Esc, pause, screenshot, console, cheats, menus — for
  netplay (next item) pause/quit must become shared tick events.
- Open test for Jan (not a block): play 0.1.2 with keyboard + pad as usual.
- v0.1.2 released (prerelease) from CI: 3 assets HTTP 200 with expected sizes, SHA256SUMS OK,
  released Linux binary writes the same sync log as the regress-tested build. Item closed.
  (Creating the release via the API with `tag_name` + `target_commitish` makes the tag and
  triggers release.yml; no separate tag push needed.)

## 2026-10-16 — takeover

- Lane `spacehog` taken over at laptop HEAD ce8554c. Worker toolchain: `tools/venv-worker`
  (Python 3.11), `tools/sdl3-worker` (headless SDL3: dummy/offscreen only), `build-w/`.
  `source tools/env.sh` picks them automatically (Python version ≠ laptop venv's).
- `re/refemu/regress.sh` passes in the worker (11 runs, ~3 min with 12 cores) and now
  exits non-zero on a divergence.
- CI added (`ci.yml`, `release.yml`, `scripts/ci-build.sh`); release binaries come from CI.
  Release flow: run regress.sh in a worker → create the GitHub release with notes
  ("regression set passed") → push the `v*` tag → release.yml attaches the files.
- v0.1.1 released from CI (first CI release; assets checked: checksums, Linux binary runs headless).
- Open: Warpzone integration (netplay, leaderboards, mods), Deck defaults, fault reports,
  lane schedules/JARVIS.md entry (asked the `self` lane, t_mvb3f9ed0ut3y).
