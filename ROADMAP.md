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
3. **Online co-op over Warpzone rooms** — deterministic lockstep with input delay,
   2 players first; per-tick `sync_state` hash to detect desyncs; `--warpzone-host`,
   `--warpzone-join ROOM`, then raise the descriptor's tier and add `host`/`join`.
4. **Leaderboards** `score-1p` / `score-2p` (seasonal): submitted at game over, never
   from `/judge` or modded runs, with the run's input recording so a score can be checked.
5. **4 players** — `num_players` instead of `two_players`, compact HUD; the 2-player
   layout stays bit-identical. Then 4-player online.
6. **Mods** — virtual file layer: `mods/NAME/` overrides datafile objects by name (PNG,
   WAV, MID, `.lev`), later the ship/enemy tables as text. Warpzone archive categories:
   level, graphics, ship, sound, music, campaign. No code in mods.
7. **Steam Deck** — fullscreen and pad defaults when `SteamDeck=1`.
8. **Fault reports** — crashes reported through a public channel into GitHub issues,
   no personal data.

## Later

- Clean-up of the translation (local variable names, Ghidra-isms) — `PLAN.md` item 9.
- macOS build (needs a Mac or an SDK that may legally be used).
- Rollback netplay on top of the state snapshot.

## Not planned

- Shipping the original game data: it is Free Lunch Design's and only passed on in its
  original form; get it from the [DOS Games Archive](https://www.dosgamesarchive.com/download/operation-spacehog).
