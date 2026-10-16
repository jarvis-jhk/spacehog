# Operation Spacehog — native port

A native C port of **Operation Spacehog 1.01** (Free Lunch Design, 2000, DOS freeware) for
Linux and Windows, rebuilt from a decompilation of the original `SH.EXE`. It plays like
the original — same logic, timing, graphics and sound — with modern extras: a resizable
window, fullscreen, and gamepads for both players.

The game data is **not** included. You need the original freeware release
(`SPACEHOG.DAT`, `SHLEV.DAT`, `SHBGS.DAT`, `SHSND.DAT`), for example from the
[DOS Games Archive](https://www.dosgamesarchive.com/download/operation-spacehog):
unpack its zip anywhere.

What comes next (online co-op, 4 players, mods, leaderboards): [ROADMAP.md](ROADMAP.md).

## Download

- **Warpzone launcher** ([warpzone.virtualtabletop.io](https://warpzone.virtualtabletop.io)):
  installs and updates the game, finds the original data in your Downloads folder and
  hands it to the game, and adds it to Steam (Steam Deck included).
- Or by hand from the [releases](https://github.com/jarvis-jhk/spacehog/releases):
  `spacehog-linux-x86_64.tar.gz` (also for the Steam Deck) or
  `spacehog-windows-x86_64.zip`. Copy the four `.DAT` files into the `spacehog` folder
  next to the program, or point it at them with `--data`.

## Playing

    spacehog --data /path/to/spacehog

Options: `--fullscreen` (Alt+Enter toggles), `--scale N`, `--square-pixels` (no 4:3
correction), `--save-dir DIR` (where `SPACEHOG.SAV` goes; default: the data directory, or the per-user app data directory if that is read-only or the data came from the launcher), `--help` for the rest. The data directory is,
in this order: `--data`, `$SPACEHOG_DATA`, `$WARPZONE_DATA_GAME` (set by the Warpzone
launcher), the current directory, `data/`, the program's own directory. The
original command line switches (`/nosound`, `/r_scores`, `/r_keys`, ...) still work.

Music: the original FM music (Allegro's Adlib/OPL driver on an emulated OPL3, as with a
Sound Blaster) is the default. Optionally the music can be played through a General MIDI
SoundFont instead, as with a wavetable card: `--music gm` (uses the first `*.sf2` next to
the game data, e.g. the free GeneralUser GS) or `--soundfont FILE`. The same can be set
permanently in `spacehog.ini` in the save directory:

    music = gm
    soundfont = GeneralUser-GS.sf2

Controls are the original ones (configurable in the options menu). Gamepads: choose
"Gamepad" for a player in the options. With two pads connected both players can use one;
pads can be plugged in at any time. A = fire / select, B = buy power-up, X = special,
Y = Enter (e.g. confirm quitting), Start = pause (and Space in menus), Back = Esc.
High score names can be entered with the pad: up/down pick a letter, A types it,
B deletes, Y finishes. Everything works without a keyboard.

## Building

Needs CMake ≥ 3.20, a C11 compiler and SDL 3 (linked statically).

    cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/path/to/sdl3
    cmake --build build

Release archives are built by CI (`scripts/ci-build.sh`, on ubuntu-22.04 so the Linux
binary runs with glibc 2.35 or newer); `scripts/package.sh` builds them locally into `dist/`.
`tools/fetch.sh` and `tools/build-sdl3*.sh` set up a self-contained toolchain in `tools/`
(static SDL3 for Linux, llvm-mingw + SDL3 for Windows; toolchain file
`cmake/mingw-w64-x86_64.cmake`). Always `source tools/env.sh` first.

## How it was made / verification

- `re/`: reverse-engineering pipeline (UPX 0.40 unpacker, datafile extractor, Ghidra
  scripts with recovered symbols and types). See `PLAN.md` for the full story.
- `src/compat/`: the subset of Allegro 3.11 the game uses, on SDL3 (graphics, palette,
  timers on a virtual 1193181 Hz clock, keyboard/gamepads, sample mixer, Allegro's MIDI
  player driving an emulated OPL3).
- `src/game/`: the translated game code.
- `re/refemu/`: a differential tester. It runs the original DOS code in an emulator next to
  the port and compares the complete game state and every drawing/sound call at every game
  tick. Full playthroughs and randomized 1/2-player sessions match exactly.
- Input recording/replay (`--record`, `--replay`) is deterministic.

## Licensing

- Operation Spacehog and its data © Free Lunch Design (freeware; not distributed here).
- The port's own code: MIT, see `LICENSE`.
- `src/compat/midi_player.c`, `adlib.c`, `fm_instr.h` are derived from Allegro 3.11
  (giftware).
- `src/third_party/tsf`: TinySoundFont by Bernhard Schelling, MIT.
- `src/third_party/nuked_opl3`: Nuked-OPL3 by nukeykt, LGPL-2.1 (binaries must allow
  relinking against a modified version; the full source here satisfies that).
- SDL 3: zlib license. Full notices: `THIRD_PARTY.txt`.
