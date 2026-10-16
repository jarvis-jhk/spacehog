/* Title menu (0x23b5c). Returns 4 = play, 3 = options, 2 = hall of fame, 1 = credits, 0 = exit. */
#include "sh.h"

int g_ae6c4;                      /* 0xae6c4 */

int title_menu(void)
{
   int sel = 4, prev, i;
   char scroll_ch[2] = { ' ', 0 };  /* 0x23b1c */
   int text_pos = 0, scroll_wait = 0, logo_x = -50, players_prev, anim = 0;
   unsigned char col[5 + 128];
   BITMAP *title = SPR(SH_TITLE);
   BITMAP *ticker;

   col[0] = col[1] = col[2] = col[3] = col[4] = 0x40;
   players_prev = two_players;
   if (music_on && !music_playing) {
      play_midi(MUS(SN_MIDI_TITLE), TRUE);
      music_playing = 1;
   }
   g_ae6c4 = 0;
   for (i = 0; i < 128; i++)
      col[i + 5] = 0;
   ticker = create_bitmap(340, 20);
   clear(ticker);
   set_pallete(black_pallete);
   clear(screen);
   draw_rle_sprite(screen, RLE(SH_BGMOON), 215, 10);
   draw_rle_sprite(screen, RLE(SH_TITLE), 160 - (title->w >> 1), 30);
   fade_in(dat_spacehog[SH_PAL].dat, 5);
   if (joy_present)
      poll_joystick();

   while (!joy[JOY_ANY].b[0] && !key[KEY_SPACE] && !key[KEY_ENTER] && (!key[KEY_ESC] || sel != 0) && !sys_quit_requested) {
      prev = sel;
      if (joy_present)
         poll_joystick();
      if ((joy[JOY_ANY].down || key[KEY_DOWN]) && !col[5 + KEY_DOWN]) {
         if (--sel < 0) sel = 0;
         col[5 + KEY_DOWN] = 1;
      }
      if ((joy[JOY_ANY].up || key[KEY_UP]) && !col[5 + KEY_UP]) {
         if (++sel > 4) sel = 4;
         col[5 + KEY_UP] = 1;
      }
      players_prev = two_players;
      if ((joy[JOY_ANY].left || key[KEY_LEFT]) && sel == 4)
         two_players = 0;
      if ((joy[JOY_ANY].right || key[KEY_RIGHT]) && sel == 4)
         two_players = 1;
      if (!joy[JOY_ANY].down && !key[KEY_DOWN])
         col[5 + KEY_DOWN] = 0;
      if (!joy[JOY_ANY].up && !key[KEY_UP])
         col[5 + KEY_UP] = 0;
      if (key[KEY_ESC]) {
         sel = 0;
         prev = 0;
      }
      if (joy[JOY_ANY].b[1])
         sel = 0;
      if (snd_on && sel != prev)
         play_sample(SND(SN_SND_NA), 200, 128, 1000, 0);
      if (players_prev != two_players)
         textout_centre(screen, FNT(SH_FONT), players_prev == 0 ? "1 PLAYER GAME" : "2 PLAYER GAME", 159, 70, 0);
      textout_centre(screen, FNT(SH_FONT), two_players == 0 ? "1 PLAYER GAME" : "2 PLAYER GAME", 159, 70, col[4]);
      textout_centre(screen, FNT(SH_FONT), "OPTIONS", 159, 88, col[3]);
      textout_centre(screen, FNT(SH_FONT), "HALL OF FAME", 159, 106, col[2]);
      textout_centre(screen, FNT(SH_FONT), "CREDITS", 159, 124, col[1]);
      textout_centre(screen, FNT(SH_FONT), "EXIT", 159, 142, col[0]);
      if (scroll_wait == 0) {
         scroll_ch[0] = ((char *)dat_spacehog[SH_TITLE_TEXT].dat)[text_pos];
         textout(ticker, FNT(SH_FONT), scroll_ch, 320, 0, 0x6b);
         scroll_wait = text_length(FNT(SH_FONT), scroll_ch);
         if (++text_pos >= dat_spacehog[SH_TITLE_TEXT].size)
            text_pos = 0;
      }
      if (g_tick) {
         g_frames++;
         anim++;
         g_tick = 0;
         scroll_wait--;
         blit(ticker, ticker, 1, 0, 0, 0, 339, 20);
         if (opt_vsync)
            vsync();
         blit(ticker, screen, 0, 0, 0, 179, 338, 20);
         if (logo_x < 100) {
            draw_character(screen, SPR(SH_OPERATION), logo_x, 20, 0);
            logo_x += 2;
            draw_sprite(screen, SPR(SH_OPERATION), logo_x, 20);
         }
      }
      if (anim > 1) {
         anim = 0;
         for (i = 0; i < 5; i++) {
            if (i == sel && col[i] < 0x4d)
               col[i]++;
            else {
               if (col[i] < 0x44) col[i]++;
               if (col[i] > 0x44) col[i]--;
            }
         }
      }
      if (key[KEY_ENTER] && key[KEY_TAB])
         console();
      wait_tick();
   }
   if (snd_on)
      play_sample(SND(SN_SND_SELECT), 200, 128, 1000, 0);
   destroy_bitmap(ticker);
   fade_out(4);
   if (sys_quit_requested)
      return 0;
   return sel;
}
