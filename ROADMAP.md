# Roadmap

Operation Spacehog is maintained by JARVIS. The goal from the start: play the original
with controllers (done), then online, with up to 4 players, and with mods — all through
the [Warpzone launcher](https://warpzone.virtualtabletop.io).

**Binding rule:** `re/refemu/regress.sh` passes at every commit. The original-compatible
path (1–2 players, original rules, no mod) stays identical to the original DOS game; new
behaviour is opt-in (a mode flag, more players, a mod loaded). Intentional differences are
listed in `PLAN.md` and pinned by the differential tests.

## Current priorities

1. **CI and releases** — Linux x86_64 on an old base (ubuntu-22.04, glibc ≥ 2.35, static
   SDL3 with X11 + Wayland) and Windows x86_64 on every push; `v*` tags publish
   `spacehog-linux-x86_64.tar.gz`, `spacehog-windows-x86_64.zip` and `SHA256SUMS`.
   0.1.1 is the first release built this way.
2. ~~**Per-tick input layer**~~ — done in 0.1.2: `player_input(p)` (`src/game/pinput.c`),
   live by default (difftest-pinned), `--input-latch` = once per tick at the tick wait.
3. ~~**Online co-op over Warpzone rooms**~~ — done in 0.2.0 (2 players): lockstep on
   the raw input stream per 70 Hz frame with input delay from the ping, `sync_state`
   CRC compared twice a second, `--warpzone-host` / `--warpzone-join ROOM` /
   `--warpzone-code` / invite links; tested by `re/net/nettest.sh`. Descriptor tier 2
   with `host`/`join`: warpzone PR. Later: in-game room list and name prompt.
4. **Leaderboards** `score-1p` / `score-2p` (seasonal): submitted at game over, never
   from `/judge` or modded runs, with the run's input recording so a score can be checked.
5. **4 players** — `num_players` instead of `two_players`, compact HUD; the 2-player
   layout stays bit-identical. Then 4-player online.
6. **Mods** — virtual file layer: `mods/NAME/` overrides datafile objects by name (PNG,
   WAV, MID, `.lev`), later the ship/enemy tables as text. Warpzone archive categories:
   level, graphics, ship, sound, music, campaign. No code in mods.
7. **Steam Deck** — fullscreen and pad defaults when `SteamDeck=1`.
8. **Fault reports** through Warpzone's feedback API (libwarpzone ≥ 0.1.3, game id
   `spacehog`): caught faults (failed data load, corrupt save/recording, net desync,
   failed join) via `wz_report(wz, "bug", stable title, detail)`; crashes: the signal
   handler / `SetUnhandledExceptionFilter` writes title + backtrace to a file in the save
   dir, the next start sends it with `wz_report_problem(NULL, "spacehog", VERSION,
   "crash", …)` and deletes it. Stable titles, no personal data. Waiting for the 0.1.3
   release (launcher-started crashes are already reported by Warpzone itself).
9. **macOS support** (`spacehog-macos-support`) — a macOS build, universal (arm64 +
   x86_64, `CMAKE_OSX_ARCHITECTURES`) or one per arch, built and smoke-tested on a GitHub
   `macos-*` runner in `ci.yml` (headless start, `--version`, a short replay/demo run),
   and on `v*` tags `spacehog-macos-universal.tar.gz` (or `.zip`/`.app`) in the release
   and `SHA256SUMS`. Install/start notes for macOS in the player docs (README). Unsigned
   is fine: document the Gatekeeper workaround (right-click → Open, or
   `xattr -dr com.apple.quarantine`). Known risks:
   - **Graphics**: we only use `SDL_Renderer` (no OpenGL), so SDL3 picks Metal; check
     the integer scaling / palette upload and HiDPI (window vs pixel size) on Retina.
   - **SDL3**: static build for macOS with Cocoa + Metal (+ IOKit/GameController
     frameworks), minimum deployment target set explicitly (e.g. 11.0).
   - **Controllers**: macOS pads go through GameController.framework/IOKit HID; check
     mapping and hot-plug, the Xbox/PS pad button labels and that `player_input(p)`
     stays deterministic.
   - **File paths**: `SDL_GetBasePath` points into `Contents/Resources` in an `.app`;
     game data and saves must go to `SDL_GetPrefPath` (`~/Library/Application Support`),
     never next to the binary (read-only / App Translocation when unsigned).
   - **Warpzone client**: there is no macOS libwarpzone yet (0.1.2: Linux, Windows only)
     — needs a macOS (universal) release from the warpzone lane, and the launcher must
     know a macOS artifact; until then a macOS build without online co-op.
   - **Crash reports** (item 8): signal handler + backtrace work as on Linux, but
     `backtrace()` symbols need the dSYM / `atos`.
   - Differential tests (`re/refemu`) stay on Linux; the macOS job only checks that the
     same recording replays to the same `sync_state` CRC as on Linux.

## Later

- Clean-up of the translation (local variable names, Ghidra-isms) — `PLAN.md` item 9.
- Rollback netplay on top of the state snapshot.

## Not planned

- Shipping the original game data: it is Free Lunch Design's and only passed on in its
  original form; get it from the [DOS Games Archive](https://www.dosgamesarchive.com/download/operation-spacehog).
