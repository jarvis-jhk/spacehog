/* Credits (0x1e614), hall of fame (0x1b088), name entry (0x1b4bc), game over (0x1b89c). */
#include "sh.h"

static void wait_for_tick(void)
{
   while (!g_tick)
      wait_tick();
   g_tick = 0;
}

static int joy_cancel(void)
{
   if (joy_present) {
      poll_joystick();
      if (joy[JOY_ANY].b[0] || joy[JOY_ANY].b[1])
         return 1;
   }
   return 0;
}

/* 0x1e614: credits roll, text with inline codes from CREDIT_TEXT */
void credits(void)
{
   RLE_SPRITE *logo = RLE(SH_CREDITS);
   BITMAP *line = create_bitmap(200, 20);
   const char *text = dat_spacehog[SH_CREDIT_TEXT].dat;
   long text_len = dat_spacehog[SH_CREDIT_TEXT].size;
   int end = 0, quit = 0, pos = 0, h = 0, y, newline, x, fnt, color;
   char ch[2] = { 0, 0 };

   clear(screen);
   draw_rle_sprite(screen, RLE(SH_BGMOON), 215, 10);
   draw_rle_sprite(screen, logo, 160 - (logo->w >> 1), 30);
   fade_in(dat_spacehog[SH_PAL].dat, 5);
   clear(line);
   do {
      /* scroll the previously rendered line in, one pixel per tick */
      for (y = h; y >= 0 && !quit; y--) {
         if (key[KEY_TAB] && key[KEY_ENTER])
            console();
         wait_for_tick();
         vsync();
         blit(line, screen, 0, 0, 60, y + 199 - h, 200, h - y);
         blit(screen, screen, 60, 101, 60, 100, 180, 99);
         if (key[KEY_ESC])
            quit = 1;
         if (joy_cancel())
            quit = 1;
      }
      clear(line);
      newline = 0;
      x = 0;
      fnt = SH_FONT_ARIAL;
      h = 12;
      color = 0xad;
      do {
         ch[0] = pos < text_len ? text[pos] : 0;
         if (ch[0] == '\\') {
            pos++;
            ch[0] = pos < text_len ? text[pos] : 0;
            switch (ch[0]) {
               case '1': color = 0x48; break;
               case '2': color = 0x28; break;
               case '3': color = 0x68; break;
               case '4': color = 0xad; break;
               case '5': color = 0xaa; break;
               case '6': color = 0x96; break;
               case '7': color = 0x1f; break;
               case 'e': end = 1; break;
               case 'l': fnt = SH_FONT; h = 20; break;
               case 'm': fnt = SH_FONT_ARIAL; h = 12; break;
               case 'n': newline = 1; break;
               case 's': fnt = SH_FONT_SMALL; h = 8; break;
               default: break;
            }
         }
         else {
            textout(line, FNT(fnt), ch, x, 0, color);
            x += text_length(FNT(fnt), ch);
         }
         pos++;
         if (key[KEY_ESC])
            quit = 1;
         if (joy_cancel())
            quit = 1;
      } while (!newline && !end && !quit);
   } while (!end && !quit);
   if (snd_on)
      play_sample(SND(SN_SND_SELECT), 200, 128, 1000, 0);
   destroy_bitmap(line);
   fade_out(4);
}

/* 0x1b088 */
void hall_of_fame(void)
{
   BITMAP *list = create_bitmap(320, 160);
   RLE_SPRITE *logo = RLE(SH_HALLOFFAME);
   int i, done = 0, scroll = 0, dir = 1, pause = 100;

   clear(screen);
   clear(list);
   draw_rle_sprite(screen, RLE(SH_BGMOON), 215, 10);
   draw_rle_sprite(screen, logo, 160 - (logo->w >> 1), 30);
   textprintf(screen, FNT(SH_FONT), 50, 90, 0x4e, "%s", g_hiscore[0].name);
   draw_sprite(screen, SPR(SH_SHIP1 + g_hiscore[0].ship), 178, 94);
   textprintf(screen, FNT(SH_FONT), 210, 90, 0x6e, "%8d", g_hiscore[0].score);
   for (i = 1; i < 10; i++) {
      textprintf(list, FNT(SH_FONT), 50, (i - 1) * 17, 0x4e - i, "%s", g_hiscore[i].name);
      draw_sprite(list, SPR(SH_SHIP1 + g_hiscore[i].ship), 178, (i - 1) * 17 + 4);
      textprintf(list, FNT(SH_FONT), 210, (i - 1) * 17, 0x6e - i, "%8d", g_hiscore[i].score);
   }
   blit(list, screen, 0, 0, 0, 111, 320, 89);
   fade_in(dat_spacehog[SH_PAL].dat, 5);
   do {
      if (key[KEY_TAB] && key[KEY_ENTER])
         console();
      if (key[KEY_TAB] && key[KEY_R]) {
         reset_scores();
         done = 1;
      }
      if (key[KEY_ENTER] || key[KEY_SPACE] || key[KEY_ESC])
         done = 1;
      if (joy_cancel())
         done = 1;
      vsync();
      blit(list, screen, 0, scroll, 0, 111, 320, 89);
      wait_for_tick();
      if (pause == 0) {
         scroll += dir;
         if (scroll == 89 || scroll == 0) {
            dir = -dir;
            pause = 50;
         }
      }
      else
         pause--;
   } while (!done);
   if (snd_on)
      play_sample(SND(SN_SND_SELECT), 200, 128, 1000, 0);
   fade_out(4);
   destroy_bitmap(list);
}

/* 0x1b4bc: enter a name for hall of fame slot `slot` */
static void enter_name(int player, int slot)
{
   RLE_SPRITE *logo = RLE(SH_HALLOFFAME);
   HISCORE *h = &g_hiscore[slot];
   int pos = 0, k, sc;

   clear(screen);
   draw_rle_sprite(screen, RLE(SH_BGMOON), 215, 10);
   draw_rle_sprite(screen, logo, 160 - (logo->w >> 1), 30);
   textprintf_centre(screen, FNT(SH_FONT), 160, 90, 0x49, "Player %d, enter Your name.", player + 1);
   fade_in(dat_spacehog[SH_PAL].dat, 5);
   strcpy(h->name, "#");
   h->score = g_player[player].score;
   h->ship = g_player[player].shipno;
   clear_keybuf();
   do {
      k = readkey();
      h->name[pos] = (char)k;
      sc = k >> 8;
      if (sc == KEY_BACKSPACE) {
         h->name[pos] = 0;
         pos--;
      }
      else
         pos++;
      if (pos < 19 && pos < 0)
         pos = 0;
      else if (pos > 18)
         pos = 18;
      h->name[pos] = '#';
      h->name[pos + 1] = 0;
      vsync();
      rectfill(screen, 0, 130, 319, 150, 0);
      if (sc == KEY_ENTER)
         h->name[pos] = 0;
      textprintf_centre(screen, FNT(SH_FONT), 160, 130, 0x29, "%s", h->name);
   } while (sc != KEY_ENTER);
   h->name[pos] = 0;
   fade_out(4);
}

/* 0x1b89c: game over screen, then hall of fame entry */
void game_over(int dummy)
{
   RLE_SPRITE *logo = RLE(SH_GAMEOVER);
   int p, i, slot;
   (void)dummy;
   set_pallete(black_pallete);
   clear(screen);
   draw_rle_sprite(screen, RLE(SH_BGMOON), 215, 10);
   draw_rle_sprite(screen, logo, 160 - (logo->w >> 1), 30);
   textprintf(screen, FNT(SH_FONT), 110, 90, 0x6b, "PLAYER 1 %8d", g_player[0].score);
   if (two_players)
      textprintf(screen, FNT(SH_FONT), 110, 110, 0x6b, "PLAYER 2 %8d", g_player[1].score);
   if (difficulty < 3) {
      textout_centre(screen, FNT(SH_FONT_ARIAL), "Practice and easy mode hold only 3 levels.", 160, 130, 0x67);
      textout_centre(screen, FNT(SH_FONT_ARIAL), "To play the entire game, increase the", 160, 140, 0x67);
      textout_centre(screen, FNT(SH_FONT_ARIAL), "difficulty level in the optionsmenu.", 160, 150, 0x67);
   }
   fade_in(dat_spacehog[SH_PAL].dat, 5);
   do {
      if (key[KEY_TAB] && key[KEY_ENTER])
         console();
      if (joy_present)
         poll_joystick();
      wait_tick();
   } while (!key[KEY_ENTER] && !key[KEY_ESC] && !joy[JOY_ANY].b[0]);
   fade_out(4);
   for (p = 0; p <= two_players; p++) {
      slot = -1;
      for (i = 9; i >= 0; i--) {
         if (g_hiscore[i].score <= g_player[p].score && i < 9) {
            g_hiscore[i + 1] = g_hiscore[i];
            slot = i;
         }
      }
      if (slot >= 0)
         enter_name(p, slot);
   }
   hall_of_fame();
}
