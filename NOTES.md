# Maintainer notes (JARVIS)

Working log for things Jan does not need to see. Newest first.

## 2026-10-20 — libwarpzone 0.1.11, online co-op on macOS (t_mvg5dyxh10cwfq, warpzone lane)

- Adopted libwarpzone **0.1.11** on all three systems (`scripts/fetch-warpzone.sh linux|
  windows|macos`; checksum with `shasum -a 256`, which both runners have). Header is
  byte-identical to 0.1.10, no source changes. macOS links `libwarpzone.a -liconv -lm`
  (NATIVE_LIBS.txt; `-lSystem -lc` are implicit). nettest/reporttest server pinned to 0.1.11.
- Linux build-w reconfigured on 0.1.11: regress.sh green, ci-replay-check ok, nettest
  seeds 61, 62 --pad, 63 --realtime green, reporttest green.
- macOS: only CI-built (link, both slices, replays); netplay itself not run on a Mac.
  Descriptor (warpzone repo + live) already has the `macos` asset/exec matching pack.sh.
- First macOS CI run with HAVE_WARPZONE failed (t_mvg5os1q14s9oo): report.c's `<ucontext.h>`
  needs _XOPEN_SOURCE on macOS; now `<sys/ucontext.h>` there. CI green (all 3, both slices).
- **v0.3.1 released** (prerelease) from CI: 4 assets OK, SHA256SUMS OK, layout unchanged
  (`spacehog/spacehog`), released Linux binary passes ci-replay-check, `--version` 0.3.1.
  Combined roadmap `spacehog-macos-online-co-op`: not marked done from this worker (no
  roadmap API access here) — mark it done when possible.

## 2026-10-20 — macOS, fault reports, libwarpzone 0.1.10 (request t_mvg3i3e7twx87)

- GitHub: no open issues/PRs. The only event was a failed CI run on my own `macos-ci`
  branch (t_mvg40kb0xw9hm): otool's "(architecture …)" header lines tripped the linkage
  check; fixed in e065c3a, CI green.
- **Upstream check (2026-10-20)**: libwarpzone v0.1.2 → **v0.1.10** adopted (header
  only gained `wz_adopt_identity` and `wz_report`/`wz_report_problem`; regress.sh,
  nettest seeds 41–45, 51–53 incl. --pad/--realtime green; nettest server 0.1.10).
  Not adopted: `wz_adopt_identity` (Android launcher only). Already current: SDL
  release-3.4.18, llvm-mingw 20261006, Nuked-OPL3 765ec962e4 (2026-08-24) and
  TinySoundFont 853a0a1717 (2026-07-19) — vendored files byte-identical to upstream HEAD.
  No macOS libwarpzone yet (0.1.10: Linux x86_64/aarch64, Windows gnu/msvc).
- **macOS** (combined roadmap `spacehog-macos-support`): universal binary from
  `macos-14` (`scripts/ci-build.sh macos`): static SDL3, minos 11.0 both slices,
  system-only linkage, ad hoc signed after strip. No online co-op (no libwarpzone):
  asked the warpzone lane (t_mvg3xqsgvasri) for a macOS libwarpzone + `macos` platform
  in descriptors. Not verified: a real Mac (window/HiDPI, pads, Gatekeeper flow) — no
  hardware; the CI runner only runs it headless.
- **CI replay check** (`scripts/ci-replay-check.sh`, `re/refemu/replays/sync.sha256`):
  every Linux and macOS build (arm64 and x86_64 via Rosetta) must replay `short.rec` and
  `fullgame_judge.rec` to the sync-log hashes of the regress-tested build-w. Game data:
  `https://image.dosgamesarchive.com/games/sh_dos.zip` (sha256 4e3b15c2…; its inner
  spacehog.zip = orig/dist/spacehog.zip), downloaded in CI, never packed/uploaded. When
  game behaviour changes on purpose: `REPLAY_CHECK_UPDATE=1 scripts/ci-replay-check.sh
  build-w/spacehog` after regress.sh.
- **Fault reports** (ROADMAP 8) via Warpzone's feedback API, not the JARVIS report URL
  (that one is secret and this repo is public; Warpzone's server forwards fault reports).
  `re/net/reporttest.sh` checks data/SAV/crash reports land in a local server's
  `feedback` table without paths. Desync and "session never starts" reports are wired
  but not provoked by a test.
- Found by nettest seed 42 --pad (pre-existing, also with 0.1.2): an offline replay of a
  session recording differed in the last record — when a replay runs out, live pads
  (none) released what the replay held. Live pads now apply on physical changes only.
- regress.sh caps parallel runs at the cgroup CPU quota (8 here): 2m25s, no false stalls.
- **v0.3.0 released** (prerelease) from CI on green main: 4 assets (linux, windows,
  macos-universal, SHA256SUMS) HTTP 200 with the expected sizes, SHA256SUMS OK, the
  released Linux binary passes `scripts/ci-replay-check.sh`. Combined roadmap:
  `spacehog-macos-support` marked done; added `spacehog-macos-online-co-op`.

## 2026-10-19 — reminder t_mveys7wp0v1et (Johan Peitz source request)

- Scheduled reminder from 2026-10-12 (t_mv5fenaodpf90): ask Jan whether Johan Peitz answered
  his email about the Operation Spacehog source. Obsolete as a "start the project?" question:
  the lane/repo exist and the port is at v0.2.0 (online co-op). Not sent to Jan (lane is
  releases-only; schedule-originated requests may not message him).
- If Johan ever sends the source, it is useful as a cross-check for the decompilation
  (re/refemu), not a restart. Jan can mention it in a Signal message to the lane.

## 2026-10-17 — Warpzone fault reports (request t_mvc4357ndehku, from the warpzone lane)

- Valid and wanted (= roadmap item 8), but blocked: needs libwarpzone 0.1.3 (wz_report /
  wz_report_problem), not released yet (latest 0.1.2). Plan written into ROADMAP item 8.
  Asked the warpzone lane to file a request here once 0.1.3 is released.
- regress.sh in this worker (cgroup quota 8 CPUs, nproc 12): all 12 runs at once → 8 false
  failures ("stalled … no sync for 10s"); rerun in batches of 4: all 11 pass. If it recurs,
  cap regress.sh's parallelism to the cgroup quota.

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
- v0.2.0 released (prerelease) from CI: 3 assets HTTP 200 with expected sizes, SHA256SUMS OK,
  the released Linux binary passes `re/net/nettest.sh` (seed 31). Item closed. Asked the
  warpzone lane (t_mvbq58qs35wt4z) to merge/deploy descriptor PR jarvis-jhk/warpzone#13.

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
