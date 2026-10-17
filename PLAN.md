# Operation Spacehog — decompilation & native port

Living status document. Keep it current; a resumed session starts here.

## Goal
Native C port of Operation Spacehog 1.01 (DOS, Johan Peitz / Free Lunch Design, 2000) for
Linux + Windows (macOS later). Behaviour and visuals must match the original very closely
(not byte-exact). Then: proper controller support (2 players first), later online play,
4 players, modding.

## Ground rules
- No changes outside this folder. All tools live in `tools/` (`tools/fetch.sh` recreates
  them). Always `source tools/env.sh` first (redirects HOME/XDG/TMPDIR/pip/java/wine state).
- Original game files live in `orig/` (gitignored, never committed — repo will be public).
  The port loads the original `.DAT` files at runtime.
- Scratch files go to `scratch/` (gitignored), never `/tmp`.

## Facts established
- `SH.EXE`: DJGPP COFF, UPX 0.40 packed → unpacked by `re/tools/unupx_djgpp.py`
  (unicorn emulation of the stub) into `orig/unpacked/SH.EXE` / `SH.coff`.
- Built with **Allegro 3.11** (source in `re/allegro311/`, giftware) + **PPCOL** (Ivan Baldo)
  + DJGPP libc. Game compiled **without optimisation** (calls out-of-line inline.c copies).
- Datafiles are Allegro packfiles, password **`skidmarks`** (only LZSS flag bytes are XORed).
  `re/tools/datx.py` extracts everything (PNG/WAV/MID) into `orig/extracted/`.
  PAL objects are raw 4-byte RGB structs, 6-bit values.
- Assets: SPACEHOG.DAT (sprites, fonts, palette, credits text), SHLEV.DAT (levels LEV_xx,
  patterns PAT_xx/GPAT_xx, texts TXT_xx), SHBGS.DAT (background deco), SHSND.DAT
  (4 MIDIs: BOSS/INTRO/LEVEL/TITLE + 10 samples).
- Video: VGA mode 13h 320x200 @ 70 Hz. Work bitmaps: 960x240, 320x240, 2× 320x11.
- Timers: `install_int(cb_40c4, 1000)` (seconds), `install_int(cb_40e0, 30)` → game tick
  every 30 ms (33.33 Hz). Logic is presumably driven by this counter.
- RNG: DJGPP `rand()` = BSD `random()` TYPE_3 (31 words, sep 3), seeded `srand(biostime(0,0))`.
- Code layout: game code 0x10a8–0x28684 (~106 funcs), Allegro 0x28684–~0x67800, libc after.
- `main` = 0x27dbc: args (/nosound /r_scores /r_keys /judge /betatest /help) → `sh_setup`
  (0x582c) → intro (0x1ee3c) → menu loop `0x23b5c` returns 4=play (0x131bc check,
  0x21a7c game), 3=options (0x27308), 2=hall of fame (0x1b088), 1=credits (0x1e614), 0=exit.

## RE workflow
- `re/ghidra/reanalyze.sh` re-applies `re/symbols.tsv` (names/prototypes) and
  `re/ghidra/types.h` (struct types) and exports `re/ghidra/out/{decomp.c,functions.tsv}`.
- `re/tools/fn.py ADDR|NAME [-n N]` prints a function's decompilation.
- Reference runs: `re/harness/run_orig.sh [display]` starts Xvfb + DOSBox Staging with the
  original; drive with `xdotool`, Ctrl+F5 = raw screenshot into `scratch/capture`.

## Plan / status
1. [x] Toolchain, unpacking, asset extraction, DOSBox harness, Ghidra pipeline.
2. [x] Identify Allegro/libc API surface used by game code (in `re/symbols.tsv`).
3. [~] Name game globals/functions, recover structs (types in `re/ghidra/types.h`; many
       fields still `f_XX`, many functions still `fXXXXX`).
4. [x] Port skeleton: CMake + SDL3; `src/compat/` Allegro-3.11 subset; deterministic
       virtual clock; input record/replay.
5. [x] Game code translated. Hand-written modules: setup, flow, intro, menu, screens,
       options, console, level. Gameplay (from the Ghidra translation, hand-maintained):
       `game.c` (sequence), `player.c`, `objects.c`, `render.c`, `tables.c`; declarations
       in `play.h`, `gglobals.h`, `structs.h`.
6. [x] Verification:
       - Visual: hall of fame/options pixel-identical, intro frame-identical vs. DOSBox.
       - Logic: differential tester `re/refemu` (original code in unicorn vs. port, game
         state + every draw/sound call compared at every sync point/tick). Full 6-level
         run + fuzz campaigns (`re/refemu/fuzz.sh`, 1/2 players, keyboard/gamepad) match.
       - Pixels: `--pixels` runs Allegro 3.11's real drawing code in the emulator; every
         frame and palette of the regression set is identical to the port's.
       - Cross-platform: `re/refemu/xplat.sh` — Linux (gcc) and Windows (clang/mingw,
         under Wine) builds write byte-identical sync logs for the full game.
       - Valgrind memcheck (`tools/valgrind`): no uninitialized reads in the replays.
7. [x] Sound: 8-voice sample mixer + Allegro MIDI player + adlib FM driver on Nuked-OPL3;
       optional General MIDI via a SoundFont (TinySoundFont): `--music gm`, `--soundfont FILE`
       or `music = gm` in spacehog.ini. FM is the default (owner's decision, 2026-10-16).
       Balance: with an SB16, Allegro applies the game's volumes (200/200) via the hardware
       mixer equally to DAC and FM, so the relative level is Allegro's software mixer vs.
       the OPL output; the port matches both (FM RMS ~200 like DOSBox; effects follow
       Allegro's mixer math). Quiet FM music is authentic. DOSBox Staging 0.83 played no
       digital effects for this game (only FM), so it cannot serve as effects reference.
       `SPACEHOG_AUDIO_DUMP=file.wav` (headless) renders the game audio offline.
8. [x] Controllers (SDL gamepads, per-player pads via `pjoy()`, Start=Pause, Back=Esc,
       hot-plug); Windows build (llvm-mingw, static SDL3, tested under Wine).
9. [~] Clean-up: structs, fields, functions and globals named; play.c split. Still open:
       local variable names (param_N/local_N), Ghidra-isms (`+ 0xff` for -1 on bytes,
       `(unsigned int)` casts). Run `re/refemu/regress.sh` after every refactoring step:
       the difftest catches any behaviour change.
10. [x] Packaging (`scripts/package.sh`), README, THIRD_PARTY.txt. [ ] macOS build
       (needs a Mac or an SDK we may legally use).
    [x] Gamepad-only play: Y = Enter, Start = Pause + Space, letter picker overlay for names.
11. [ ] Later: 4 players, netplay (lockstep/rollback on per-tick input), modding (override dir).
       Waiting for the owner's priorities (see design notes below).

## Intentional differences to the original (keep difftests valid!)
Each is identical to the original under the original's conditions (<= 1 pad, no hot-plug):
- `joy_present` = pads connected (hot-plug) instead of one-time detection.
- Two players may both pick "gamepad" only with 2+ pads; each then uses its own pad.
- Uninitialized "still held" latches (ship select, option screens) start as "held"; in DOS
  they contain leftovers of earlier frames (e.g. the title menu's locals). refemu forces the
  same value (`UB_INIT`), and scribbles non-zero bytes below the stack under HLE calls.
- End-of-level pilot comments with both players dead: the original shows uninitialized
  stack text; the port shows empty boxes (refemu `UB_INIT` pins the original to that).
- Busy-wait loops call `wait_tick()` (virtual time); placement mirrors the original loop
  (see `re/refemu/sites.tsv`).

## Testing
- Build: `source tools/env.sh; cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=$SH_ROOT/tools/sdl3-linux; cmake --build build`
  (ASan variant: `build-asan`, flags `-fsanitize=address,undefined`).
- Scripted runs: `python3 re/harness/mkscript.py OUT.rec 'sec:key[:hold]' ...` then
  `./build-asan/spacehog /judge --data orig/dist/game --save-dir scratch/save --headless --fast
   --replay OUT.rec --max-seconds N --shots scratch/shots` (SPACEHOG_SHOT_EVERY_MS=ms).
  Cheats with /judge: F5 shield, F8 finish level, F1 lives, F2 specials.
- `re/tools/spinloops.py FILE` finds polling loops that never yield (must call wait_tick()).
- Interactive: `re/harness/xvfb.sh` (display :93) + `re/harness/keys.sh`.
- Differential test (see `re/refemu/README.md`): `re/refemu/difftest.sh REPLAY [-- /judge]`
  (port log + emulated original; first difference with field names via `--dump N`).
  Fuzzing: `re/refemu/fuzz.sh FIRST LAST [MINUTES]` (parallel, results in `scratch/fuzz/`).
  Regression set: `re/refemu/regress.sh` (full game with /judge from
  `re/refemu/replays/`, plus fuzz replays; ~3 minutes in parallel).

## Pitfalls learned
- Ghidra must see interrupt-written globals as volatile (`re/volatile.tsv`), otherwise busy
  waits get constant-folded.
- Functions taking the 64-byte level event by value lose their arguments in Ghidra; read
  the disassembly (`re/tools/disasm.py`).
- Negative indexes on `pad_XX` arrays = neighbouring fields; fix with `re/tools/fixidx.py`.
- The game writes into its string arguments (dialog word wrap) -> copy literals first.
- Never `pkill -f` with patterns that occur in the shell command itself.
- unicorn: set real GDT descriptors for SS/DS/ES/FS/GS, otherwise loading FS silently makes
  the stack 16-bit. Never resume `emu_start` at an address with a hook (it fires again):
  run once, stop via a watchdog thread.
- Original reads uninitialized locals in a few places (see "Intentional differences").
## Design notes for later goals
Ground rule: the original-compatible path (1-2 players, original rules) must keep passing
`re/refemu/regress.sh`; new behaviour is opt-in (mode flag / more players / mod loaded).

- **Tick structure** (play_game): one logic step per 30 ms tick; inside a tick the game
  reads `key[]`/`pjoy()` at several places, and `render_frame()` calls `vsync()`, which
  pumps input mid-tick. Lockstep/rollback needs input to change only at the tick wait:
  in net mode, latch all player inputs at the tick wait (end of `play_game`'s loop) and
  serve `key[]`/`joy[]` from the latched snapshot for the whole tick.
- **Per-player input** — DONE (0.1.2, `src/game/pinput.c`): `PLAYER_INPUT player_input(p)`
  = key-binding bits, pjoy bits, Enter, mouse buttons + position; live unless
  `player_input_latched` (`--input-latch`, latched in `wait_tick()`). Still read directly
  (system keys, not per player): Esc/pause/F12 screenshot, TAB+Enter console, cheats,
  dialog yes/no, menus/options. Netplay must share those too (pause/quit as a tick event).
  Original note: replace direct `key[g_keys[p*7+k]]` / `pjoy(p)` / mouse reads in
  player code by one `player_input(p)` bitmask (up/down/left/right/fire/select/special),
  filled per tick from the player's device (keyboard, pad, mouse, network). Verified
  equivalent by the difftest as long as sampling points stay the same.
- **State snapshot**: `src/game/sync_state.c` (generated from `re/ghidra/types.h`) already
  enumerates the complete game state; extend it into save/restore (pointer fields are
  datafile-relative) for rollback netplay and save states. RNG state is `dos_rand_state`.
- **Netplay**: deterministic lockstep with input delay first (2 players over UDP, inputs
  per tick + periodic state hash from sync_state to detect desyncs), rollback later.
- **4 players**: `g_player[2]`, `bmp_bar[2]`, `two_players` (45 uses), `rand() % 2` target
  choice, spawn positions, HUD (bars at top and bottom take 11 px each). Needs new HUD
  layout (compact bars) and a `num_players` replacing `two_players`; keep the 2-player
  layout bit-identical.
- **Modding**: virtual file layer in `sys_read_file`/`load_datafile`: an override dir
  (`mods/NAME/`) checked first; per-object overrides by datafile object name (PNG for
  bitmaps/RLE, WAV, MID, .lev). Enemy/ship/bullet tables (`tables.c`) could be loaded
  from a text file with the built-in values as default.
