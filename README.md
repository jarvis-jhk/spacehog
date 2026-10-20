# Operation Spacehog — native port

A native C port of **Operation Spacehog 1.01** (Free Lunch Design, 2000, DOS freeware) for
Linux, Windows and macOS, rebuilt from a decompilation of the original `SH.EXE`. It plays like
the original — same logic, timing, graphics and sound — with modern extras: a resizable
window, fullscreen, and gamepads for both players.

The game data is **not** included. You need the original freeware release
(`SPACEHOG.DAT`, `SHLEV.DAT`, `SHBGS.DAT`, `SHSND.DAT`), for example from the
[DOS Games Archive](https://www.dosgamesarchive.com/download/operation-spacehog):
unpack its zip anywhere.

What comes next (4 players, mods, leaderboards): [ROADMAP.md](ROADMAP.md).

## Download

- **Warpzone launcher** ([warpzone.virtualtabletop.io](https://warpzone.virtualtabletop.io)):
  installs and updates the game, finds the original data in your Downloads folder and
  hands it to the game, and adds it to Steam (Steam Deck included).
- Or by hand from the [releases](https://github.com/jarvis-jhk/spacehog/releases):
  `spacehog-linux-x86_64.tar.gz` (also for the Steam Deck),
  `spacehog-windows-x86_64.zip` or `spacehog-macos-universal.tar.gz` (macOS 11 or newer,
  Apple Silicon and Intel). Copy the four `.DAT` files into the `spacehog` folder next to
  the program, or point it at them with `--data`.

### macOS

The macOS build is not signed by Apple, so Gatekeeper refuses it at first ("cannot be
opened" / "is damaged"). Unpack the archive (double-click it in Finder, or
`tar xzf spacehog-macos-universal.tar.gz`), copy the `.DAT` files into the `spacehog`
folder, then either

- remove the download quarantine once in Terminal:
  `xattr -dr com.apple.quarantine ~/Downloads/spacehog` (the folder where you unpacked it), or
- Control-click (right-click) `spacehog` in Finder, choose **Open** and confirm **Open**
  (on macOS 15 and newer: try to open it once, then *System Settings → Privacy & Security →
  Open Anyway*).

Then double-click `spacehog` (it opens a Terminal window next to the game) or start it in
Terminal with `./spacehog`. Settings and high scores go to `SPACEHOG.SAV` next to the data,
or to `~/Library/Application Support/FreeLunchDesign/Spacehog/` if that folder is not
writable. Gamepads (Xbox, PlayStation, MFi) work as on the other systems. Online co-op is
not in the macOS build yet (the Warpzone client library has no macOS release so far).

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

## Online co-op (Warpzone)

Two players on two machines play together over [Warpzone](https://warpzone.virtualtabletop.io)
rooms (direct connection with relay fallback, encrypted):

    spacehog --warpzone-host                 # player 1: opens a room, shows its code
    spacehog --warpzone-code K7QM3X          # player 2: joins with that code
    spacehog --warpzone-join ROOM            # ... or by room id (the launcher's "Join")
    spacehog warpzone://join/...             # ... or with an invite link

The host is player 1 and runs the menus (choose 2 players as usual); the guest plays
player 2 with its usual player-1 keys or its gamepad. The host's settings, controls and
high scores are used (the guest's own `SPACEHOG.SAV` stays untouched). `--warpzone-private`
opens an unlisted room (code or invite link only), `--net-delay N` sets the input delay
(frames of 1/70 s; by default it follows the ping). The Warpzone identity is the launcher's; on a machine without
one, add `--warpzone-name NAME` once.

Both machines run the same deterministic game in lockstep: every input is applied on both
at the same emulated time, so nothing but the input crosses the network. A checksum of the
whole game state is compared twice a second ("out of sync" would show if they ever
differed). If the partner leaves, the game goes on with the remaining player.
`--record FILE` on either side writes the session as an ordinary replay.

## Building

Needs CMake ≥ 3.20, a C11 compiler and SDL 3 (linked statically).

    cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/path/to/sdl3
    cmake --build build

Online co-op needs the Warpzone client library: `scripts/fetch-warpzone.sh linux DIR`
downloads the pinned libwarpzone release (checksum-verified) and prints its folder; pass
it as `-DWARPZONE_DIR=...`. Without it the game builds without online play.

Release archives are built by CI (`scripts/ci-build.sh`, on ubuntu-22.04 so the Linux
binary runs with glibc 2.35 or newer, and a universal macOS 11+ binary on a macOS runner);
every CI build replays the recordings in `re/refemu/replays` and checks that it writes the
same sync logs as the build verified against the original (`scripts/ci-replay-check.sh`;
it downloads the freeware game data for that and never packs it). On macOS that runs for
both the arm64 slice and the x86_64 slice (Rosetta). `scripts/package.sh` builds them locally into `dist/`.
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
- Player input: the game reads every player's keys, pad and mouse only through
  `player_input(p)` (`src/game/pinput.c`); `--input-latch` samples it once per tick at the
  tick wait instead of at the original's read points.
- Online co-op (`src/platform/net.c`, the netplay section of `src/compat/input.c`):
  lockstep on the raw input stream. `re/net/nettest.sh` runs host and guest against a local
  Warpzone server and checks that both write identical sync logs and recordings, and that
  the merged recording matches the emulated original.

## Licensing

- Operation Spacehog and its data © Free Lunch Design (freeware; not distributed here).
- The port's own code: MIT, see `LICENSE`.
- `src/compat/midi_player.c`, `adlib.c`, `fm_instr.h` are derived from Allegro 3.11
  (giftware).
- `src/third_party/tsf`: TinySoundFont by Bernhard Schelling, MIT.
- `src/third_party/nuked_opl3`: Nuked-OPL3 by nukeykt, LGPL-2.1 (binaries must allow
  relinking against a modified version; the full source here satisfies that).
- SDL 3: zlib license.
- libwarpzone (online play): MIT OR Apache-2.0; its own dependencies are listed in
  `LICENSE-libwarpzone-third-party.html` in the release archives. Full notices: `THIRD_PARTY.txt`.
