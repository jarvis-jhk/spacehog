/* Startup: text-mode setup screen, data loading, precalculation, save file.
 * Translated from SH.EXE 0x4130-0x6400 and 0x27ce8. */
#include "sh.h"
#include "../compat/internal.h"
#include "../platform/report.h"
#include "../platform/sys.h"
#include <stdlib.h>
#include <time.h>

DATAFILE *dat_spacehog, *dat_shlev, *dat_shbgs, *dat_shsnd;

char opt_nosound, opt_cheats, music_playing;
unsigned char snd_on, music_on, digi_vol, midi_vol, two_players;
int difficulty = 3;
unsigned char opts[10];
const unsigned char opts_default[10] = { 1, 1, 1, 50, 0, 0, 0, 0, 0, 0 };
char mouse_present;
int screenshot_no;

volatile int g_tick, g_fps, g_frames;

HISCORE g_hiscore[10];
int g_keys[14];

BITMAP *bmp_world, *bmp_buf;
RGB_MAP rgb_table;
COLOR_MAP light_white, light_green;
int g_cos128[256], g_sin128[256];
RLE_SPRITE *expl_small[20], *expl_medium[42], *expl_big[50];
RLE_SPRITE *missile_rot[16];
BITMAP *bigmissile_rot[48];
char level_file[80];

/* 0x40c4: 1 Hz timer, latches the frame counter */
static void fps_timer(void)
{
   g_fps = g_frames;
   g_frames = 0;
}

/* 0x40e0: 30 ms game timer */
static void game_timer(void)
{
   g_tick++;
}

void wait_tick(void)
{
   sys_idle();
   player_input_latch();
}

static void con_flush(void)
{
   sys_present();
}

/* 0x40f8: progress callback for the long precalculations */
void progress_dot(int pos)
{
   if ((pos & 3) == 3) {
      tprintf("\xfe");
      con_flush();
   }
}

/* 0x4130: F12 screenshot to shog_NNN.pcx */
int save_screenshot(void)
{
   BITMAP *b;
   PALETTE pal;
   char name[32];
   int r;
   if (screenshot_no >= 1000)
      return 0;
   b = create_sub_bitmap(screen, 0, 0, screen->w, screen->h);
   if (!b)
      return 0;
   get_pallete(pal);
   snprintf(name, sizeof(name), "shog_%03d.pcx", screenshot_no);
   screenshot_no++;
   r = save_pcx(name, b, pal);
   destroy_bitmap(b);
   return r != 0;
}

/* 0x4228: render explosion animation frames (soft blobs, blurred over time) */
void gen_explosions(int size)
{
   BITMAP *bmp;
   int w, frames, f, x, y;
   if (size == 0) { w = 20; frames = 20; }
   else if (size == 1) { w = 48; frames = 42; }
   else { w = 80; frames = 50; }
   bmp = create_bitmap(w, w);
   clear_to_color(bmp, 0xa0);
   for (f = 0; f < frames; f++) {
      progress_dot(f);
      if (f < frames / 3) {
         int cx = (w >> 2) + rand() % ((w - 2) >> 1);
         int cy = (w >> 2) + rand() % ((w - 2) >> 1);
         int r = ((w - 4) >> 2) - rand() % ((w - 4) >> 4);
         circlefill(bmp, cx, cy, r, 0xbf - rand() % 5);
         circle(bmp, cx, cy, r, 0xa3);
      }
      /* in-place 3x3 box blur (reads already blurred neighbours, as the original) */
      for (x = 1; x < w - 1; x++)
         for (y = 1; y < w - 1; y++) {
            int s = _getpixel(bmp, x + 1, y + 1) + _getpixel(bmp, x + 1, y) + _getpixel(bmp, x + 1, y - 1) +
                    _getpixel(bmp, x, y + 1) + _getpixel(bmp, x, y) + _getpixel(bmp, x, y - 1) +
                    _getpixel(bmp, x - 1, y + 1) + _getpixel(bmp, x - 1, y) + _getpixel(bmp, x - 1, y - 1);
            /* order of the sum in the original: (x-1,y-1)+(x-1,y)+(x-1,y+1)+(x,y-1)+... (commutative) */
            _putpixel(bmp, x, y, (unsigned char)(s / 9));
         }
      for (x = 0; x < w; x++)
         for (y = 0; y < w; y++)
            if (_getpixel(bmp, x, y) == 0xa0)
               _putpixel(bmp, x, y, 0);
      if (size == 0)
         expl_small[f] = get_rle_sprite(bmp);
      else if (size == 1)
         expl_medium[f] = get_rle_sprite(bmp);
      else
         expl_big[f] = get_rle_sprite(bmp);
      for (x = 0; x < w; x++)
         for (y = 0; y < w; y++)
            if (_getpixel(bmp, x, y) == 0)
               _putpixel(bmp, x, y, 0xa0);
   }
   destroy_bitmap(bmp);
}

/* 0x46a4: pre-rotated missile sprites (16 directions) */
void gen_missiles(void)
{
   BITMAP *bmp = create_bitmap(8, 8);
   int i, j;
   for (i = 0; i < 16; i++) {
      clear(bmp);
      rotate_sprite(bmp, SPR(SH_MISSILE), 0, 0, itofix(256 - i * 16));
      missile_rot[i] = get_rle_sprite(bmp);
      for (j = 0; j < 3; j++) {
         bigmissile_rot[i * 3 + j] = create_bitmap(23, 23);
         clear(bigmissile_rot[i * 3 + j]);
         rotate_sprite(bigmissile_rot[i * 3 + j], SPR(SH_BIGMISSILE1 + j), 0, 0, itofix(256 - i * 16));
      }
   }
   destroy_bitmap(bmp);
}

/* a data file that is there but cannot be read is a fault worth reporting (another
   release of the game, a damaged download); a missing one is not */
static void report_bad_datafile(const char *name)
{
   size_t n = 0;
   uint8_t *raw = sys_read_file(name, &n);
   if (raw)
      report_fault("data file not readable", "%s, %lu bytes, starts %02x %02x %02x %02x\n", name,
                   (unsigned long)n, n > 0 ? raw[0] : 0, n > 1 ? raw[1] : 0, n > 2 ? raw[2] : 0,
                   n > 3 ? raw[3] : 0);
   free(raw);
}

/* 0x4930 */
void load_datafiles(void)
{
   tprintf("%s ", "spacehog.dat");
   con_flush();
   dat_spacehog = load_datafile("spacehog.dat");
   if (!dat_spacehog) {
      report_bad_datafile("SPACEHOG.DAT");
      allegro_exit();
      sys_error_box("Cannot load SPACEHOG.DAT.\n\nPut the original game files (SPACEHOG.DAT, SHLEV.DAT, SHBGS.DAT, SHSND.DAT) next to the program or in the current directory, or start it with --data DIR.");
      exit(1);
   }
   tprintf("%s ", "shlev.dat");
   con_flush();
   dat_shlev = load_datafile("shlev.dat");
   if (!dat_shlev) {
      report_bad_datafile("SHLEV.DAT");
      allegro_exit();
      sys_error_box("Cannot load SHLEV.DAT. Please copy all original game files.");
      exit(1);
   }
   tprintf("%s ", "shbgs.dat");
   con_flush();
   dat_shbgs = load_datafile("shbgs.dat");
   if (!dat_shbgs) {
      report_bad_datafile("SHBGS.DAT");
      allegro_exit();
      sys_error_box("Cannot load SHBGS.DAT. Please copy all original game files.");
      exit(1);
   }
   tprintf("%s ", "shsnd.dat");
   con_flush();
   dat_shsnd = load_datafile("shsnd.dat");
   if (!dat_shsnd) {
      report_bad_datafile("SHSND.DAT");
      allegro_exit();
      sys_error_box("Cannot load SHSND.DAT. Please copy all original game files.");
      exit(1);
   }
   tprintf("\n");
}

/* 0x4b84 */
void reset_scores(void)
{
   int i, ship = 0;
   for (i = 0; i < 10; i++) {
      strcpy(g_hiscore[i].name, "Free Lunch Design");
      g_hiscore[i].score = 50000 - i * 5000;
      g_hiscore[i].ship = ship;
      if (++ship == 3)
         ship = 0;
   }
   strcpy(g_hiscore[1].name, "Maximillian Drake");
   strcpy(g_hiscore[2].name, "Sheila Longhart");
   strcpy(g_hiscore[3].name, "Billy 68");
   strcpy(g_hiscore[4].name, "Kraven Drax");
   strcpy(g_hiscore[5].name, "Shay Prumble");
   strcpy(g_hiscore[6].name, "Brock Vixel");
   strcpy(g_hiscore[7].name, "Maxim Friggot");
   strcpy(g_hiscore[8].name, "Leloo Takenoo");
   strcpy(g_hiscore[9].name, "Hantor Capole");
}

/* 0x4d10 */
void reset_keys(void)
{
   static const int def[14] = {
      KEY_UP, KEY_DOWN, KEY_RIGHT, KEY_LEFT, KEY_INSERT, KEY_DEL, KEY_ENTER,
      KEY_W, KEY_S, KEY_D, KEY_A, KEY_H, KEY_J, KEY_K
   };
   memcpy(g_keys, def, sizeof(def));
}

/* ---------------------------------------------------------------- SPACEHOG.SAV
 * Layout (354 bytes, little endian, identical to the DOS version):
 *   10 x { char name[20]; int32 score; int32 ship; }   hall of fame
 *   14 x int32                                         key bindings
 *   10 x byte options, digi vol, midi vol, sound, music, difficulty (low byte),
 *   two players, player 1 control, player 2 control
 */
static void put32(unsigned char *p, int v)
{
   p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24);
}

static int get32(const unsigned char *p)
{
   return (int)((unsigned)p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16 | (unsigned)p[3] << 24);
}

/* 0x5234 */
void load_saved(void)
{
   unsigned char b[354];
   FILE *f;
   int i;
   tprintf("spacehog.sav");
   con_flush();
   f = sys_fopen_save("SPACEHOG.SAV", "rb");
   if (!f) {
      tprintf(" not found, restoring defaults...\n");
      con_flush();
      reset_scores();
      reset_keys();
      difficulty = 3;
      two_players = 0;
      snd_on = 1;
      music_on = 1;
      digi_vol = 200;
      midi_vol = 200;
      g_player[0].control = CTRL_KEYBOARD;
      g_player[1].control = CTRL_KEYBOARD;
      for (i = 0; i < 10; i++)
         opts[i] = opts_default[i];
      return;
   }
   memset(b, 0, sizeof(b));
   if ((i = (int)fread(b, 1, sizeof(b), f)) != (int)sizeof(b)) {
      sys_log("SPACEHOG.SAV is short; missing values are zero");
      report_fault("SPACEHOG.SAV is short", "%d of %d bytes\n", i, (int)sizeof(b));
   }
   fclose(f);
   for (i = 0; i < 10; i++) {
      memcpy(g_hiscore[i].name, b + i * 28, 20);
      g_hiscore[i].name[19] = 0;
      g_hiscore[i].score = get32(b + i * 28 + 20);
      g_hiscore[i].ship = get32(b + i * 28 + 24);
   }
   for (i = 0; i < 14; i++)
      g_keys[i] = get32(b + 280 + i * 4);
   memcpy(opts, b + 336, 10);
   digi_vol = b[346];
   midi_vol = b[347];
   snd_on = b[348];
   music_on = b[349];
   difficulty = (difficulty & ~0xFF) | b[350]; /* the DOS code freads one byte into the int */
   two_players = b[351];
   g_player[0].control = b[352];
   g_player[1].control = b[353];
}

/* 0x6120 */
void write_saved(void)
{
   unsigned char b[354];
   FILE *f;
   int i;
   memset(b, 0, sizeof(b));
   for (i = 0; i < 10; i++) {
      memcpy(b + i * 28, g_hiscore[i].name, 20);
      put32(b + i * 28 + 20, g_hiscore[i].score);
      put32(b + i * 28 + 24, g_hiscore[i].ship);
   }
   for (i = 0; i < 14; i++)
      put32(b + 280 + i * 4, g_keys[i]);
   memcpy(b + 336, opts, 10);
   b[346] = digi_vol;
   b[347] = midi_vol;
   b[348] = snd_on;
   b[349] = music_on;
   b[350] = (unsigned char)difficulty;
   b[351] = two_players;
   b[352] = g_player[0].control;
   b[353] = g_player[1].control;
   f = sys_fopen_save("SPACEHOG.SAV", "wb");
   if (!f)
      return;
   fwrite(b, 1, sizeof(b), f);
   fclose(f);
}

/* 0x5448 */
void setup_sound(void)
{
   if (install_sound(DIGI_AUTODETECT, MIDI_AUTODETECT, NULL) == 0) {
      tprintf("A%X I%i D%i \n", 0x220, 7, 1);
      con_flush();
      set_volume(digi_vol, midi_vol);
   }
   else {
      tprintf("Failed to detect sound hardware. \n");
      con_flush();
      opt_nosound = 1;
      music_on = 0;
      snd_on = 0;
   }
}

/* 0x5154 */
void calc_trig(void)
{
   int i;
   for (i = 0; i < 256; i++) {
      progress_dot(i);
      g_cos128[i] = fixtoi(fcos(itofix(i)) << 7);
      g_sin128[i] = fixtoi(fsin(itofix(i)) << 7);
   }
}

static const char dots64[] =
   "\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9"
   "\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9";

static void progress_bar(int y)
{
   gotoxy(1, y);
   tprintf("[%s]\r[", dots64);
   con_flush();
}

static unsigned long bios_ticks(void)
{
   time_t t = time(NULL);
   struct tm *lt = localtime(&t);
   long secs = lt ? lt->tm_hour * 3600L + lt->tm_min * 60L + lt->tm_sec : 0;
   return (unsigned long)(secs * 1193180L / 65536L);
}

/* 0x582c */
void sh_setup(void)
{
   int i, ok;
   packfile_password("skidmarks");
   set_gfx_mode(GFX_TEXT, 0, 0, 0, 0);
   clrscr();
   textcolor(0);
   textbackground(1);
   cprintf("                                                                            \xb0\xb1\xb2\xdb");
   textcolor(15);
   textbackground(1);
   gotoxy(1, 1);
   cprintf("\xdb\xb2\xb1\xb0  Operation Spacehog                   Copyright 1998-2000 Johan Peitz");
   textcolor(7);
   gotoxy(1, 3);
   tprintf(" Release version %s\n", "1.01");
   tprintf(" Compiled: %s\n", "00 06 02, 13:49");
   tprintf(" Designed by Johan Peitz\n");
   tprintf(" Allegro %s, by Shawn Hargreaves, %s\n", "3.11", "1999");
   tprintf(" PPCOL, by Ivan Baldo\n");
   tprintf(" Native port, 2026\n");
   con_flush();
   srand(input_random_seed((unsigned)bios_ticks()));
   gotoxy(1, 9);
   textcolor(1);
   textbackground(0);
   cprintf("\xcd\xcd\xcd\xcd[SETTING UP]\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd"
             "\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd"
             "\xcd\xcd\xcd\xcd \xcd\xcd\xcd \xcd\xcd  \xcd   \xcd\n");
   textcolor(9);
   gotoxy(6, 9);
   cprintf("SETTING UP");
   textcolor(7);
   gotoxy(1, 10);
   clreol();
   tprintf(" Installing Mouse: ");
   con_flush();
   ok = install_mouse();
   tprintf(ok != -1 ? "OK!\n" : "Not found!\n");
   mouse_present = ok != -1;
   gotoxy(1, 11);
   clreol();
   tprintf(" Installing Timers\n");
   con_flush();
   install_timer();
   install_int(fps_timer, 1000);
   g_fps = 0;
   g_frames = 0;
   install_int(game_timer, 30);
   g_tick = 0;
   gotoxy(1, 12);
   clreol();
   tprintf(" Loading Saved Data: ");
   con_flush();
   load_saved();
   gotoxy(1, 13);
   clreol();
   tprintf(" Loading Data: ");
   con_flush();
   load_datafiles();
   gotoxy(1, 14);
   clreol();
   tprintf(" Installing Sound: ");
   con_flush();
   if (!opt_nosound)
      setup_sound();
   else {
      tprintf("nosound selected\n");
      con_flush();
      snd_on = 0;
      music_on = 0;
   }
   gotoxy(1, 15);
   clreol();
   tprintf(" Installing Keyboard \n");
   con_flush();
   install_keyboard();
   gotoxy(1, 16);
   clreol();
   tprintf(" Detecting Joystick:");
   con_flush();
   /* joy_present follows the number of connected pads (hot-plug), see sh.h */
   if (install_joystick(0) == 0)
      tprintf(" Found!\n");
   else
      tprintf(" Not found!\n");
   con_flush();
   gotoxy(1, 17);
   clreol();
   tprintf(" Reserving Memory\n");
   con_flush();
   bmp_world = create_bitmap(960, 240);
   bmp_buf = create_bitmap(320, 240);
   bmp_bar[0] = create_bitmap(320, 11);
   bmp_bar[1] = create_bitmap(320, 11);
   gotoxy(1, 18);
   clreol();
   tprintf(" Precalculating Trigonometry\n");
   progress_bar(19);
   calc_trig();
   gotoxy(1, 19);
   clreol();
   tprintf(" Precalculating LightningFX\n");
   progress_bar(20);
   create_rgb_table(&rgb_table, dat_spacehog[SH_PAL].dat, progress_dot);
   rgb_map = &rgb_table;
   gotoxy(1, 20);
   clreol();
   progress_bar(20);
   create_light_table(&light_white, dat_spacehog[SH_PAL].dat, 63, 63, 63, progress_dot);
   gotoxy(1, 20);
   clreol();
   progress_bar(20);
   create_light_table(&light_green, dat_spacehog[SH_PAL].dat, 31, 63, 31, progress_dot);
   gotoxy(1, 20);
   clreol();
   tprintf(" Generating Explosions\n");
   con_flush();
   gotoxy(1, 21);
   tprintf("[");
   for (i = 0; i < 12; i++) tprintf("\xf9");
   tprintf("]\r[");
   con_flush();
   gen_explosions(2);
   gotoxy(1, 21);
   clreol();
   gotoxy(1, 21);
   tprintf("[");
   for (i = 0; i < 10; i++) tprintf("\xf9");
   tprintf("]\r[");
   con_flush();
   gen_explosions(1);
   gotoxy(1, 21);
   clreol();
   gotoxy(1, 21);
   tprintf("[");
   for (i = 0; i < 5; i++) tprintf("\xf9");
   tprintf("]\r[");
   con_flush();
   gen_explosions(0);
   gotoxy(1, 21);
   clreol();
   tprintf(" Generating Missile\n");
   con_flush();
   gen_missiles();
   gotoxy(1, 23);
   clreol();
   textcolor(15);
   cprintf(" Press SPACEBAR to continue...");
   con_flush();
   while (!key[KEY_SPACE] && !sys_quit_requested)
      wait_tick();
   fade_out(5);
   set_gfx_mode(GFX_AUTODETECT, 320, 200, 0, 0);
   strcpy(level_file, "empty.lev");
   custom_level = 0;
   custom_music = 0;
   text_mode(-1);
}

/* 0x62ec */
void sh_shutdown(void)
{
   char path[1024];
   write_saved();
   sys_save_path("level.$$$", path, sizeof(path));
   remove(path);
   destroy_bitmap(bmp_buf);
   destroy_bitmap(bmp_world);
   destroy_bitmap(bmp_bar[0]);
   destroy_bitmap(bmp_bar[1]);
   unload_datafile(dat_spacehog);
   unload_datafile(dat_shbgs);
   unload_datafile(dat_shlev);
   unload_datafile(dat_shsnd);
   allegro_exit();
}

/* 0x27ce8 */
void show_help(void)
{
   printf(" Operation Spacehog, Copyright 1998-2000 Johan Peitz \n");
   printf(" =================================================== \n");
   printf("                                      Version %s\n", "1.01");
   printf(" Commandline options:\n");
   printf("   /help     this text\n");
   printf("   /nosound  turn of all sound\n");
   printf("   /r_scores reset highscore table\n");
   printf("   /r_keys   reset keys to default\n");
   printf("   /?????    secret option to enable cheats\n");
   printf("\n");
   printf(" Contact Free Lunch Design at: d98peitz@dtek.chalmers.se\n\n");
   exit(1);
}
