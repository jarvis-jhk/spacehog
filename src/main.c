/* Operation Spacehog native port - entry point. */
#include "compat/alleg.h"
#include <SDL3/SDL_main.h>   /* WinMain on Windows */
#include "compat/internal.h"
#include "platform/sys.h"
#include "game/game.h"
#include "game/synclog.h"
#include "game/sh.h"
#include "platform/net.h"
#include "platform/report.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef SPACEHOG_VERSION
#define SPACEHOG_VERSION "dev"
#endif

static void usage(void)
{
   printf("Operation Spacehog (native port)\n"
          "usage: spacehog [options] [/nosound] [/r_scores] [/r_keys]\n"
          "  --version         print the version and exit\n"
          "  --data DIR        directory with SPACEHOG.DAT etc.\n"
          "  --save-dir DIR    where SPACEHOG.SAV is kept (default: data dir)\n"
          "  --scale N         initial window scale (default 3)\n"
          "  --fullscreen      start fullscreen (Alt+Enter toggles)\n"
          "  --square-pixels   no 4:3 aspect correction\n"
          "  --record FILE     record input (deterministic replay)\n"
          "  --replay FILE     replay recorded input\n"
          "  --fast            run in virtual time as fast as possible\n"
          "  --headless        no window/audio (tests)\n"
          "  --shots DIR       screenshot directory (also SPACEHOG_SHOT_EVERY_MS)\n"
          "  --max-seconds N   quit after N seconds of game time\n"
          "  --music fm|gm     fm: original FM music (default); gm: General MIDI via a SoundFont\n"
          "  --soundfont FILE  .sf2 for --music gm (default: first *.sf2 in the data/save dir)\n"
          "  (both can also be set in spacehog.ini in the save dir: music = gm, soundfont = FILE)\n"
          "  --warpzone-host   host an online co-op game (Warpzone room; you are player 1)\n"
          "  --warpzone-join ROOM / --warpzone-code CODE / warpzone://join/...\n"
          "                    join one (you are player 2; the host's settings are used)\n"
          "  --warpzone-private  host an unlisted room (join by code or invite link)\n"
          "  --warpzone-name NAME  Warpzone name to register if this device has none\n"
          "  --warpzone-server URL  Warpzone server (default: WARPZONE_SERVER or the public one)\n"
          "  --net-delay N     online input delay in frames of 1/70 s (host; default: from the ping)\n"
          "  --input-latch     read player input once per tick (lockstep; not the original's timing)\n"
          "  --sync-log FILE   write the differential-test sync log (re/refemu)\n"
          "  --sync-dump A[-B] include full canonical state for records A..B\n");
}

/* --render-midi INDEX FILE.wav SECONDS: render a SHSND.DAT tune offline (sound tests) */
static int render_midi(int idx, const char *path, int secs)
{
   DATAFILE *snd;
   FILE *f;
   int frames = 44100 * secs, i;
   int16_t *buf;
   packfile_password("skidmarks");
   snd = load_datafile("shsnd.dat");
   if (!snd) { sys_log("cannot load shsnd.dat"); return 1; }
   install_sound(DIGI_AUTODETECT, MIDI_AUTODETECT, NULL);
   set_volume(255, 200);
   play_midi(snd[idx].dat, FALSE);
   buf = malloc(sizeof(int16_t) * 2 * (size_t)frames);
   sound_render(buf, frames);
   f = fopen(path, "wb");
   if (!f) return 1;
   {
      uint32_t data = (uint32_t)frames * 4, v;
      fwrite("RIFF", 1, 4, f); v = 36 + data; fwrite(&v, 4, 1, f);
      fwrite("WAVEfmt ", 1, 8, f); v = 16; fwrite(&v, 4, 1, f);
      { uint16_t h[2] = { 1, 2 }; fwrite(h, 2, 2, f); }
      v = 44100; fwrite(&v, 4, 1, f); v = 44100 * 4; fwrite(&v, 4, 1, f);
      { uint16_t h[2] = { 4, 16 }; fwrite(h, 2, 2, f); }
      fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);
   }
   for (i = 0; i < frames * 2; i++)
      fwrite(&buf[i], 2, 1, f);
   fclose(f);
   free(buf);
   return 0;
}

int main(int argc, char **argv)
{
   char *game_argv[32];
   const char *sync_log = NULL, *sync_dump = NULL;
   int game_argc = 0, i, ret;
   game_argv[game_argc++] = argv[0];
   for (i = 1; i < argc; i++) {
      const char *a = argv[i];
      const char *next = (i + 1 < argc) ? argv[i + 1] : NULL;
      if (!strcmp(a, "--data") && next) { sys_cfg.data_dir = next; i++; }
      else if (!strcmp(a, "--save-dir") && next) { sys_cfg.save_dir = next; i++; }
      else if (!strcmp(a, "--scale") && next) { sys_cfg.scale = atoi(next); i++; }
      else if (!strcmp(a, "--fullscreen")) sys_cfg.fullscreen = 1;
      else if (!strcmp(a, "--square-pixels")) sys_cfg.square_pixels = 1;
      else if (!strcmp(a, "--record") && next) { sys_cfg.record_path = next; i++; }
      else if (!strcmp(a, "--replay") && next) { sys_cfg.replay_path = next; i++; }
      else if (!strcmp(a, "--fast")) sys_cfg.fast = 1;
      else if (!strcmp(a, "--headless")) sys_cfg.headless = 1;
      else if (!strcmp(a, "--shots") && next) { sys_cfg.shot_dir = next; i++; }
      else if (!strcmp(a, "--max-seconds") && next) { sys_cfg.exit_after_ticks = atol(next); i++; }
      else if (!strcmp(a, "--soundfont") && next) { sys_cfg.soundfont = next; sys_cfg.music_gm = 1; i++; }
      else if (!strcmp(a, "--music") && next) { sys_cfg.music_gm = !strcmp(next, "gm"); i++; }
      else if (!strcmp(a, "--input-latch")) player_input_latched = 1;
      else if (!strcmp(a, "--warpzone-host")) net_cfg.requested = net_cfg.host = 1;
      else if (!strcmp(a, "--warpzone-join") && next) { net_cfg.requested = 1; net_cfg.join = next; i++; }
      else if (!strcmp(a, "--warpzone-code") && next) { net_cfg.requested = 1; net_cfg.code = next; i++; }
      else if (!strncmp(a, "warpzone://", 11)) { net_cfg.requested = 1; net_cfg.invite = a; }
      else if (!strcmp(a, "--warpzone-private")) net_cfg.private_room = 1;
      else if (!strcmp(a, "--warpzone-name") && next) { net_cfg.name = next; i++; }
      else if (!strcmp(a, "--warpzone-server") && next) { net_cfg.server = next; i++; }
      else if (!strcmp(a, "--net-delay") && next) { net_delay = atoi(next); i++; }
      else if (!strcmp(a, "--sync-log") && next) { sync_log = next; i++; }
      else if (!strcmp(a, "--sync-dump") && next) { sync_dump = next; i++; }
      else if (!strcmp(a, "--help") || !strcmp(a, "-h")) { usage(); return 0; }
      else if (!strcmp(a, "--version")) { printf("spacehog %s\n", SPACEHOG_VERSION); return 0; }
      else if (!strcmp(a, "--render-midi") && i + 3 < argc) {
         sys_cfg.headless = 1;
         sys_init();
         return render_midi(atoi(argv[i + 1]), argv[i + 2], atoi(argv[i + 3]));
      }
      else if (game_argc < 31) game_argv[game_argc++] = argv[i];
   }
   game_argv[game_argc] = NULL;
   if (getenv("WARPZONE_JOIN") && *getenv("WARPZONE_JOIN")) {
      net_cfg.requested = 1;
      net_cfg.join = getenv("WARPZONE_JOIN");
   }
   if (getenv("WARPZONE_HOST") && !strcmp(getenv("WARPZONE_HOST"), "1"))
      net_cfg.requested = net_cfg.host = 1;
   if (sys_init() != 0)
      return 1;
   report_init();
   if (net_cfg.requested) {
      uint32_t seed;
      int nargc = game_argc;
      char **nargv = game_argv;
      if (net_start(&nargc, &nargv, &seed) != 0) {
         sys_shutdown();
         return 1;
      }
      game_argc = nargc;
      if (nargv != game_argv) {
         for (i = 0; i <= nargc && i < 32; i++)
            game_argv[i] = nargv[i];
      }
      input_net_start(seed);
      input_net_map_key = pinput_net_map_key;
      input_net_pad_slot = pinput_net_pad_slot;
      input_net_hash = sync_state_hash;
   }
   input_init_recording();
   if (sync_log && synclog_open(sync_log, sync_dump) != 0)
      sys_log("cannot open sync log %s", sync_log);
   allegro_init();
   ret = game_main(game_argc, game_argv);
   sys_shutdown();
   return ret;
}
