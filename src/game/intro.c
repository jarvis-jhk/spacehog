/* Intro sequence (0x1ee3c) and its helpers. ESC aborts, TAB fast-forwards. */
#include "sh.h"

char opt_vsync = 1;               /* 0x73c05 */

/* end of an animation frame: wait for the next 30 ms tick (TAB skips the wait) */
void frame_wait(void)
{
   while (!g_tick && !key[KEY_TAB])
      wait_tick();
   if (key[KEY_TAB])
      sys_poll();
}

static void frame_vsync(void)
{
   if (opt_vsync && !key[KEY_TAB])
      vsync();
}

/* 0x1eb4c: wait n ticks; returns 1 if ESC was pressed (TAB skips) */
int intro_wait(int n)
{
   g_tick = 0;
   for (;;) {
      if (key[KEY_ESC])
         return 1;
      if (!(g_tick < n && !key[KEY_TAB]))
         return 0;
      wait_tick();
   }
}

/* 0x1eb90: typewriter text with a colour 1 outline, one character per tick */
int intro_type(const char *s, int x, int y, int color)
{
   char c[2] = { ' ', 0 };
   int len = (int)strlen(s), i, ofs = 0;
   FONT *f = FNT(SH_FONT);
   for (i = 0; i < len; i++) {
      g_tick = 0;
      c[0] = s[i];
      textout(screen, f, c, x + ofs - 1, y, 1);
      textout(screen, f, c, x + ofs, y - 1, 1);
      textout(screen, f, c, x + ofs + 1, y, 1);
      textout(screen, f, c, x + ofs, y + 1, 1);
      textout(screen, f, c, x + ofs, y, color);
      ofs += text_length(f, c);
      if (key[KEY_ESC])
         return 1;
      frame_wait();
   }
   return 0;
}

/* common "present frame and wait" tail of the intro animation loops */
static int intro_frame(int y, int h)
{
   frame_vsync();
   blit(bmp_buf, screen, 0, y, 0, y, 320, h);
   frame_wait();
   return key[KEY_ESC] != 0;
}

/* 0x1ee3c */
int intro_run(void)
{
   static const int hero_bg[3] = { 0x24, 0x43, 99 };
   PALETTE white;
   int i, r, a, b, c;
   RGB *pal = dat_spacehog[SH_PAL].dat;

   clear(screen);
   set_pallete(pal);
   get_pallete(white);
   for (i = 1; i < 256; i++)
      white[i].r = white[i].g = white[i].b = 63;
   set_pallete(black_pallete);
   rest(1000);
   draw_rle_sprite(screen, RLE(SH_BKG_STARS1), 0, 0);
   draw_rle_sprite(screen, RLE(SH_BKG_STARS1), 0, 200);
   textout_centre(screen, FNT(SH_FONT), "In a near and quite", 160, 80, 0x6c);
   textout_centre(screen, FNT(SH_FONT), "possible future...", 160, 98, 0x6c);
   rectfill(screen, 0, 0, 319, 19, 0);
   rectfill(screen, 0, 180, 319, 199, 0);
   fade_in(pal, 2);
   if (intro_wait(40)) return 0;

   /* scroll down to earth */
   for (i = 0; i < 400; i++) {
      g_tick = 0;
      clear(bmp_buf);
      draw_rle_sprite(bmp_buf, RLE(SH_BKG_STARS1), 0, -i >> 1);
      draw_rle_sprite(bmp_buf, RLE(SH_BKG_STARS1), 0, 200 - (i >> 1));
      textout_centre(bmp_buf, FNT(SH_FONT), "In a near and quite", 160, 80 - i, 0x6c);
      textout_centre(bmp_buf, FNT(SH_FONT), "possible future...", 160, 98 - i, 0x6c);
      draw_rle_sprite(bmp_buf, dat_shbgs[BG_EARTH].dat, 0, 520 - i);
      if (intro_frame(20, 160)) return 0;
   }
   if (music_on) {
      play_midi(MUS(SN_MIDI_INTRO), 0);
      music_playing = 1;
   }
   blit(screen, bmp_world, 0, 20, 0, 20, 320, 160);
   if (intro_wait(80)) return 0;

   /* a light appears and grows into the alien ship */
   color_map = &light_white;
   drawing_mode(DRAW_MODE_TRANS, NULL, 0, 0);
   for (i = 255; i >= 0; i -= 3) {
      g_tick = 0;
      blit(bmp_world, bmp_buf, 0, 20, 0, 20, 320, 160);
      r = rand();
      circlefill(bmp_buf, 60, 30, (i >> 3) + r % 3, i);
      if (intro_frame(20, 160)) return 0;
   }
   for (i = 1; i < 152; i++) {
      g_tick = 0;
      blit(bmp_world, bmp_buf, 0, 20, 0, 20, 320, 160);
      if (i < 80)
         a = rand() % 30 + 30;
      else {
         a = 152 - i;
         if (a < 0) a = 0;
      }
      r = rand();
      circlefill(bmp_buf, 60, 30, (a >> 3) + r % 3, a);
      stretch_sprite(bmp_buf, dat_shbgs[BG_BIGALIENSHIP].dat, (i >> 1) + 60, (i >> 2) + 30, i, i >> 1);
      if (intro_frame(20, 160)) return 0;
   }
   solid_mode();
   blit(bmp_world, bmp_buf, 0, 20, 0, 20, 320, 160);
   draw_sprite(bmp_buf, dat_shbgs[BG_BIGALIENSHIP].dat, 136, 68);
   frame_vsync();
   blit(bmp_buf, screen, 0, 20, 0, 20, 320, 160);
   blit(screen, bmp_world, 0, 0, 0, 0, 320, 200);
   if (intro_wait(80)) return 0;

   /* OCTO slides in */
   for (i = 320; i >= 0; i -= 4) {
      g_tick = 0;
      blit(bmp_world, bmp_buf, 0, 0, 0, 0, 320, 200);
      draw_rle_sprite(bmp_buf, dat_shbgs[BG_OCTO].dat, i, 0);
      if (intro_frame(0, 200)) return 0;
   }
   if (intro_wait(20)) return 0;
   if (intro_type("Earthlings! Your time is over,", 40, 120, 0x2c)) return 0;
   if (intro_type("resistance is futile.", 40, 140, 0x2c)) return 0;
   if (intro_type("Prepare to die...", 40, 160, 0x2c)) return 0;
   if (intro_wait(80)) return 0;

   /* the general answers */
   fade_out(8);
   blit(bmp_world, screen, 0, 0, 0, 0, 320, 200);
   fade_in(pal, 8);
   for (i = 330; i != 150; i -= 4) {
      g_tick = 0;
      blit(bmp_world, bmp_buf, 0, 0, 0, 0, 320, 200);
      draw_rle_sprite(bmp_buf, dat_shbgs[BG_GENERAL].dat, i, 0);
      if (intro_frame(0, 200)) return 0;
   }
   if (intro_wait(20)) return 0;
   if (intro_type("Keep your distance weirdo!", 20, 60, 0x6b)) return 0;
   if (intro_type("We will never surrender!", 20, 80, 0x6b)) return 0;
   if (intro_wait(80)) return 0;

   fade_out(8);
   clear(screen);
   textout_centre(screen, FNT(SH_FONT), "E N T E R   T H E   H E R O E S", 160, 80, 0x1f);
   fade_in(pal, 16);
   if (intro_wait(20)) return 0;
   for (i = 0; i < 3; i++) {
      play_sample(SND(SN_SND_BUILD), 200, 128, 1000, 0);
      fade_in(white, 32);
      clear(screen);
      rectfill(screen, 0, 20, 319, 179, hero_bg[i]);
      draw_rle_sprite(screen, dat_shbgs[BG_CLOSE_1 + i].dat, 0, 0);
      fade_in(pal, 16);
      if (intro_wait(20)) return 0;
   }
   fade_out(8);
   clear(screen);
   fade_in(pal, 16);

   /* the three ships fly in (no tick reset in this loop: runs at vsync speed) */
   a = -1000; b = -320; c = -620;
   for (i = 0; i < 300; i++) {
      clear(bmp_buf);
      r = rand();
      rectfill(bmp_buf, 0, 20, 319, 179, r % 16 + 0xa0);
      draw_rle_sprite(bmp_buf, dat_shbgs[BG_SHIP_2].dat, b, 10);
      draw_rle_sprite(bmp_buf, dat_shbgs[BG_SHIP_3].dat, c, 50);
      draw_rle_sprite(bmp_buf, dat_shbgs[BG_SHIP_1].dat, a, 90);
      if (a < -30) a += 4;
      if (b < 80) b += 2;
      if (c < 50) c += 3;
      if (intro_frame(0, 200)) return 0;
   }
   fade_in(white, 32);
   clear(screen);
   fade_in(pal, 16);

   /* the pilots */
   a = 320; b = 900; c = -540;
   for (i = 0; i < 250; i++) {
      clear(bmp_buf);
      r = rand();
      rectfill(bmp_buf, 0, 20, 319, 179, r % 24 + 100);
      draw_rle_sprite(bmp_buf, dat_shbgs[BG_PILOT3].dat, c, 10);
      draw_rle_sprite(bmp_buf, dat_shbgs[BG_PILOT1].dat, a, 10);
      draw_rle_sprite(bmp_buf, dat_shbgs[BG_PILOT2].dat, b, 10);
      if (a > 0) a -= 2;
      if (b > 80) b -= 5;
      if (c < 180) c += 4;
      if (intro_frame(0, 200)) return 0;
   }
   {
      int rad = 16, col = 0x60;
      for (; i < 340; i++) {
         r = rand();
         clear_to_color(bmp_buf, r % 24 + 100);
         circlefill(bmp_buf, 160, 99, rad, col);
         if (++col > 0x6f) col = 0x6f;
         rad += 2;
         if (rad > 190) rad = 190;
         rectfill(bmp_buf, 0, 0, 319, 19, 0);
         rectfill(bmp_buf, 0, 180, 319, 199, 0);
         draw_rle_sprite(bmp_buf, dat_shbgs[BG_PILOT3].dat, c, 10);
         draw_rle_sprite(bmp_buf, dat_shbgs[BG_PILOT1].dat, a, 10);
         draw_rle_sprite(bmp_buf, dat_shbgs[BG_PILOT2].dat, b, 10);
         if (intro_frame(0, 200)) return 0;
      }
   }
   fade_from(pal, white, 4);
   if (intro_wait(10)) return 0;

   fade_out(8);
   clear(screen);
   blit(bmp_world, screen, 0, 0, 0, 0, 320, 200);
   fade_in(pal, 4);
   if (intro_type("an adventure is about to start", 60, 80, 0x1f)) return 0;
   if (intro_wait(40)) return 0;
   blit(bmp_world, screen, 0, 0, 0, 0, 320, 200);
   if (intro_wait(20)) return 0;
   for (a = 1; a < 285; a++) {
      blit(bmp_world, bmp_buf, 0, 20, 0, 20, 320, 160);
      stretch_sprite(bmp_buf, dat_shbgs[BG_FLD].dat, 160 - (a >> 1), 60, a, 38);
      if (intro_frame(20, 160)) return 0;
   }
   if (intro_wait(20)) return 0;
   if (intro_type("p r e s e n t s", 110, 120, 0x1f)) return 0;
   if (intro_wait(40)) return 0;
   fade_out(2);
   clear(screen);
   set_pallete(pal);
   stop_midi();
   music_playing = 0;
   return 1;
}

void intro(void)
{
   intro_run();
}
