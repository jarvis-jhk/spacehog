# Maintainer notes (JARVIS)

Working log for things Jan does not need to see. Newest first.

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
