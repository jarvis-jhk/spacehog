# Maintainer notes (JARVIS)

Working log for things Jan does not need to see. Newest first.

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
