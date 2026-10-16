/* Options menu (0x27308), key configuration (0x257dc), graphics options (0x260cc). */
#include "sh.h"

char key_name[12];                /* 0x95338 */

/* Allegro's capitals key table (0x7499c), indexed by scancode */
static const char key_caps[128] =
   "\0\x1b" "1234567890-=\b\tQWERTYUIOP[]\r\0ASDFGHJKL;'`\0\\ZXCVBNM,./\0*\0 \0\3\3\3\3\b\3\3\3\3\3\0\0\0\0\0-\0\0\0+"
   "\0\0\0\0\x7f\0\0\\\3\3\0\0\0\0\0\0\0\r\0/\0\0\0\0\0\0\0\0\0\0\0\0\x7f\0\0\0\0\0\0\0\0\0\0/\0\0\0\0";

/* 0x248e0: help line at the bottom */
static void help_line(const char *s)
{
   int w = text_length(FNT(SH_FONT_SMALL), s);
   textout(bmp_buf, FNT(SH_FONT_SMALL), s, 160 - (w >> 1), 190, 0x96);
}

/* 0x24438: printable name of a scancode into key_name */
void get_key_name(int sc)
{
   static const struct { int sc; const char *name; } names[] = {
      { 0, " " }, { 0x4b, "Left" }, { 0x4d, "Right" }, { 0x48, "Up" }, { 0x50, "Down" },
      { 0x2a, "L Shift" }, { 0x36, "R Shift" }, { 0x38, "Alt" }, { 0x79, "Alt Gr" }, { 0x1d, "L Ctrl" },
      { 0x78, "R Ctrl" }, { 0x1c, "Enter" }, { 0x39, "Space" }, { 0x0f, "Tab" }, { 0x3a, "Caps Lock" },
      { 0x3b, "F1" }, { 0x3c, "F2" }, { 0x3d, "F3" }, { 0x3e, "F4" }, { 0x3f, "F5" }, { 0x40, "F6" },
      { 0x41, "F7" }, { 0x42, "F8" }, { 0x43, "F9" }, { 0x44, "F10" }, { 0x57, "F11" }, { 0x58, "F12" },
      { 0x52, "Insert" }, { 0x53, "Delete" }, { 0x47, "Home" }, { 0x4f, "End" }, { 0x49, "Page Up" },
      { 0x51, "Page Down" }, { 0x4c, "Pad 5" }, { 0x4e, "Pad +" }, { 0x4a, "Pad -" }, { 0x45, "NumLock" },
      { 0x46, "ScrLock" }, { 0x5b, "L Win" }, { 0x5c, "R Win" }, { 0x5d, "Menu" }, { 0x0e, "Backspace" },
      { 0x7a, "PAD /" }, { 100, "PAD" }, { 0x37, "PAD *" },
   };
   unsigned i;
   memset(key_name, 0, 10);
   key_name[0] = (sc >= 0 && sc < 128) ? key_caps[sc] : 0;
   for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
      if (names[i].sc == sc)
         strcpy(key_name, names[i].name);
}

/* 0x271bc: cycle a player's control device (0 keyboard, 1 gamepad, 2 mouse) */
static int next_control(int p, int dir)
{
   int other = (p != 1);
   int c = dir + g_player[p].control;
   c = MID(0, c, 2);
   if (c == 1 && !joy_present)
      c = MID(0, dir + 1, 2);
   /* a device can't be shared; port: with two or more pads both players may pick one */
   if (c == g_player[other].control && c != 0 && !(c == 1 && num_joysticks >= 2))
      c = MID(0, c + dir, 2);
   if (c == g_player[other].control && c != 0 && !(c == 1 && num_joysticks >= 2))
      c = g_player[p].control;
   return c;
}

/* 0x2673c */
static void draw_options(int sel)
{
   static const int col_init[8] = { 0x26, 0x26, 0x26, 0x26, 0x26, 0x26, 0x26, 0x28 };
   static const int ys[8] = { 0x50, 0x64, 0x78, 0x8c, 0x50, 0x64, 0x78, 0xa0 };
   static const char *ctrl_names[3] = { "Keyboard", "Gamepad", "Mouse" };
   static const char *levels[7] = { "INSANE", "PRACTICE", "EASY", "NORMAL", "TOUGH", "HARD", "INSANE" };
   int col[8], xs[8], i;
   const int x1 = 30, x2 = 180, dx = 60, dx2 = 60;
   char help[80];
   RLE_SPRITE *logo = RLE(SH_OPTIONS);
   FONT *f = FNT(SH_FONT);

   memcpy(col, col_init, sizeof(col));
   for (i = 0; i < 4; i++) { xs[i] = x1; xs[i + 4] = x2; }
   clear(bmp_buf);
   draw_rle_sprite(bmp_buf, RLE(SH_BGMOON), 215, 10);
   draw_rle_sprite(bmp_buf, logo, 160 - (logo->w >> 1), 30);
   col[sel] += 0x20;
   textout(bmp_buf, f, "Players:", x1, ys[0], col[0]);
   textprintf(bmp_buf, f, x1 + dx, ys[0], col[0] + 2, "%d", two_players + 1);
   textout(bmp_buf, f, "Level:", x1, ys[1], col[1]);
   textout(bmp_buf, f, (difficulty >= 1 && difficulty <= 5) ? levels[difficulty] : "INSANE", x1 + dx, ys[1], col[1] + 2);
   textout(bmp_buf, f, "Ply 1:", x1, ys[2], col[2]);
   textout(bmp_buf, f, ctrl_names[g_player[0].control <= 1 ? g_player[0].control : 2], x1 + dx2, ys[2], col[2] + 2);
   textout(bmp_buf, f, "Ply 2:", x1, ys[3], col[3]);
   textout(bmp_buf, f, ctrl_names[g_player[1].control <= 1 ? g_player[1].control : 2], x1 + dx2, ys[3], col[3] + 2);
   textout(bmp_buf, f, "Set Graphics", x2, ys[4], col[4]);
   textout(bmp_buf, f, "Music vol:", x2, ys[5], col[5]);
   if (!music_on)
      textprintf(bmp_buf, f, x2 + dx, ys[5], col[5] + 2, "%8s", "OFF");
   else {
      rectfill(bmp_buf, x2 + 80, ys[5] + 6, x2 + 131, ys[5] + 16, 0x24);
      rectfill(bmp_buf, x2 + 80, ys[5] + 6, midi_vol / 5 + 80 + x2, ys[5] + 16, 0x4b);
   }
   textout(bmp_buf, f, "SFX vol:", x2, ys[6], col[6]);
   if (!snd_on)
      textprintf(bmp_buf, f, x2 + dx, ys[6], col[6] + 2, "%8s", "OFF");
   else {
      rectfill(bmp_buf, x2 + 80, ys[6] + 6, x2 + 131, ys[6] + 16, 0x24);
      rectfill(bmp_buf, x2 + 80, ys[6] + 6, digi_vol / 5 + 80 + x2, ys[6] + 16, 0x4b);
   }
   textout(bmp_buf, f, "Main Menu", x2, ys[7], col[7]);
   draw_rle_sprite(bmp_buf, RLE(SH_OPTION), xs[sel] - 14, ys[sel] + 6);
   switch (sel) {
      case 0: strcpy(help, "< and > to select number of players"); break;
      case 1: strcpy(help, "< and > to select level of difficulty"); break;
      case 2: strcpy(help, g_player[0].control == CTRL_KEYBOARD ? "ENTER to customize, < and > to change" : "< and > to change"); break;
      case 3: strcpy(help, g_player[1].control == CTRL_KEYBOARD ? "ENTER to customize, < and > to change" : "< and > to change"); break;
      case 4: strcpy(help, "ENTER to custom graphics"); break;
      case 5: strcpy(help, !opt_nosound ? "< and > to raise/lower music volume, ENTER toggles on/off" : "music not available"); break;
      case 6: strcpy(help, !opt_nosound ? "< and > to raise/lower sfx volume, ENTER toggles on/off" : "sfx not available"); break;
      default: strcpy(help, "ENTER to return to main menu"); break;
   }
   help_line(help);
   vsync();
   blit(bmp_buf, screen, 0, 0, 0, 0, 320, 200);
}

/* 0x24a20: key configuration screen. sel = highlighted row, editing = row being
 * redefined (its current key is hidden), blink = show '#' cursor */
static void draw_keys(int p, int sel, int editing, int blink)
{
   static const int col_init[9] = { 0x26, 0x26, 0x26, 0x26, 0x26, 0x26, 0x26, 0x26, 0x28 };
   static const int xs[9] = { 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0xb4, 0xb4 };
   static const int ys[9] = { 0x46, 0x56, 0x66, 0x76, 0x86, 0x96, 0xa6, 0x96, 0xa6 };
   /* row -> index in g_keys (rows are up, down, left, right, fire, select, special) */
   static const int key_idx[7] = { 0, 1, 3, 2, 4, 5, 6 };
   static const char *labels[7] = { "UP", "DOWN", "LEFT", "RIGHT", "FIRE", "SELECT", "SPECIAL" };
   int col[9], i;
   char help[80];
   RLE_SPRITE *logo = RLE(SH_OPTIONS);
   FONT *f = FNT(SH_FONT);

   memcpy(col, col_init, sizeof(col));
   col[sel] += 0x20;
   clear(bmp_buf);
   draw_rle_sprite(bmp_buf, RLE(SH_BGMOON), 215, 10);
   draw_rle_sprite(bmp_buf, logo, 160 - (logo->w >> 1), 30);
   for (i = 0; i < 7; i++) {
      textout(bmp_buf, f, labels[i], 0x32, ys[i], col[i]);
      get_key_name(g_keys[p * 7 + key_idx[i]]);
      if (editing != i)
         textprintf(bmp_buf, f, 100, ys[i], col[i], "%s", key_name);
   }
   textout(bmp_buf, f, "CUSTOM", 0xb4, 0x96, col[7]);
   textout(bmp_buf, f, "BACK", 0xb4, 0xa6, col[8]);
   draw_rle_sprite(bmp_buf, RLE(SH_OPTION), xs[sel] - 14, ys[sel] + 6);
   if (blink)
      textout(bmp_buf, f, "#", 100, editing * 16 + 0x46, col[editing]);
   if (sel <= 6)
      strcpy(help, "ENTER to change key");
   else if (sel == 7)
      strcpy(help, "ENTER to change all keys");
   else
      strcpy(help, "ENTER to return to option menu");
   help_line(help);
   vsync();
   blit(bmp_buf, screen, 0, 0, 0, 0, 320, 200);
}

/* 0x25184: wait for any key, returns its scancode */
static int read_new_key(int p, int row)
{
   int blink = 0, k = 0, i;
   clear_keybuf();
   rest(125);
   do {
      if (++blink == 2)
         blink = 0;
      draw_keys(p, row, row, blink);
      for (i = 0; i < 128; i++)
         if (key[i])
            k = i;
      if (k == 0x7b || k == 0x54)
         k = 0;
   } while (k == 0);
   rest(125);
   clear_keybuf();
   return k;
}

/* 0x25254: replace scancode `from` by `to` in all bindings */
static void swap_key(int from, int to)
{
   int p, i;
   for (p = 0; p < 2; p++)
      for (i = 0; i < 7; i++)
         if (g_keys[p * 7 + i] == from)
            g_keys[p * 7 + i] = to;
}

/* 0x2546c: redefine one row */
static void redefine_key(int p, int row)
{
   static const int key_idx[7] = { 0, 1, 3, 2, 4, 5, 6 };
   int *slot = &g_keys[p * 7 + key_idx[row]];
   int old = *slot, k;
   *slot = 0;
   k = read_new_key(p, row);
   if (k == KEY_ESC)
      k = old;
   else
      swap_key(k, old);
   *slot = k;
}

/* 0x257dc */
static void key_config(int p)
{
   int sel = 8, prev, done = 0, i;
   /* the original never initializes its "still held" latches; on DOS they hold non-zero
    * stack garbage, so a key or button held from the previous screen must be released */
   int up_latch = 1, down_latch = 1, enter_latch = 1;
   if (snd_on)
      play_sample(SND(SN_SND_SELECT), 200, 128, 1000, 0);
   fade_out(4);
   draw_keys(p, 8, 9, 0);
   fade_in(dat_spacehog[SH_PAL].dat, 4);
   clear_keybuf();
   while (!key[KEY_ESC] && !done) {
      if (key[KEY_TAB] && key[KEY_ENTER])
         console();
      if (joy_present)
         poll_joystick();
      if (!key[KEY_UP] && !joy[JOY_ANY].up) up_latch = 0;
      if (!key[KEY_DOWN] && !joy[JOY_ANY].down) down_latch = 0;
      if (!key[KEY_ENTER] && !joy[JOY_ANY].b[0]) enter_latch = 0;
      prev = sel;
      if ((key[KEY_UP] || joy[JOY_ANY].up) && !up_latch) {
         if (--sel < 0) sel = 0;
         up_latch = 1;
      }
      if ((key[KEY_DOWN] || joy[JOY_ANY].down) && !down_latch) {
         if (++sel > 8) sel = 8;
         down_latch = 1;
      }
      if ((key[KEY_ENTER] || joy[JOY_ANY].b[0]) && !enter_latch) {
         if (sel >= 0 && sel < 7)
            redefine_key(p, sel);
         if (sel == 7)
            for (i = 0; i < 7; i++)
               redefine_key(p, i);
         if (sel == 8)
            done = 1;
         enter_latch = 1;
      }
      if (key[KEY_ESC]) {
         sel = 8;
         prev = 8;
      }
      if (joy[JOY_ANY].b[1])
         sel = 8;
      if (prev != sel && snd_on)
         play_sample(SND(SN_SND_NA), 200, 128, 1000, 0);
      draw_keys(p, sel, 9, 0);
   }
   if (snd_on)
      play_sample(SND(SN_SND_SELECT), 200, 128, 1000, 0);
   fade_out(4);
}

/* 0x25bc4 */
static void draw_gfx_options(int sel)
{
   int ys[5] = { 0x56, 0x66, 0x76, 0x86, 0xa0 };
   int col[5] = { 0x26, 0x26, 0x26, 0x26, 0x28 };
   static const char *labels[3] = { "Lighting Effects:", "Background Image:", "Backgrond Objects:" };
   char help[80];
   RLE_SPRITE *logo = RLE(SH_OPTIONS);
   FONT *f = FNT(SH_FONT);
   int i;
   col[sel] += 0x20;
   clear(bmp_buf);
   draw_rle_sprite(bmp_buf, RLE(SH_BGMOON), 215, 10);
   draw_rle_sprite(bmp_buf, logo, 160 - (logo->w >> 1), 30);
   for (i = 0; i < 3; i++) {
      textout(bmp_buf, f, labels[i], 0x32, ys[i], col[i]);
      textout(bmp_buf, f, opts[i] ? "ON" : "OFF", 200, ys[i], col[i]);
   }
   textout(bmp_buf, f, "Stars:", 0x32, ys[3], col[3]);
   if (!opts[3])
      textout(bmp_buf, f, "OFF", 200, ys[3], col[3]);
   else {
      rectfill(bmp_buf, 200, ys[3] + 6, 251, ys[3] + 16, 0x24);
      rectfill(bmp_buf, 200, ys[3] + 6, opts[3] / 5 + 200, ys[3] + 16, 0x4b);
   }
   textout(bmp_buf, f, "BACK", 0x32, ys[4], col[4]);
   draw_rle_sprite(bmp_buf, RLE(SH_OPTION), 0x32 - 14, ys[sel] + 6);
   switch (sel) {
      case 0: strcpy(help, "ENTER to toggles on/off"); break;
      case 1:
      case 2: strcpy(help, "ENTER toggles on/off"); break;
      case 3: strcpy(help, "< and > to alter number of stars"); break;
      default: strcpy(help, "ENTER to return to option menu"); break;
   }
   help_line(help);
   vsync();
   blit(bmp_buf, screen, 0, 0, 0, 0, 320, 200);
}

/* 0x260cc */
static void gfx_options(void)
{
   int sel = 4, prev, done = 0, v;
   int up_l = 1, down_l = 1, left_l = 1, right_l = 1, enter_l = 1;   /* see key_config() */
   if (snd_on)
      play_sample(SND(SN_SND_SELECT), 200, 128, 1000, 0);
   fade_out(4);
   draw_gfx_options(4);
   fade_in(dat_spacehog[SH_PAL].dat, 4);
   clear_keybuf();
   while (!key[KEY_ESC] && !done) {
      if (key[KEY_TAB] && key[KEY_ENTER])
         console();
      if (joy_present)
         poll_joystick();
      prev = sel;
      if (joy[JOY_ANY].b[1])
         sel = 4;
      if ((key[KEY_UP] || joy[JOY_ANY].up) && !up_l) {
         if (--sel < 0) sel = 0;
         up_l = 1;
      }
      if ((key[KEY_DOWN] || joy[JOY_ANY].down) && !down_l) {
         if (++sel > 4) sel = 4;
         down_l = 1;
      }
      if ((key[KEY_RIGHT] || joy[JOY_ANY].right) && !right_l) {
         if (sel == 3) {
            v = opts[3] + 10;
            opts[3] = (unsigned char)(v > 255 ? 255 : v);
         }
         if (sel >= 0 && sel < 3) {
            right_l = 1;
            opts[sel] = 1;
         }
      }
      if ((key[KEY_LEFT] || joy[JOY_ANY].left) && !left_l) {
         if (sel == 3) {
            v = opts[3] - 10;
            opts[3] = (unsigned char)(v < 0 ? 0 : v);
         }
         if (sel >= 0 && sel < 3) {
            left_l = 1;
            opts[sel] = 0;
         }
      }
      if ((key[KEY_ENTER] || key[KEY_SPACE] || joy[JOY_ANY].b[0]) && !enter_l) {
         enter_l = 1;
         if (sel >= 0 && sel < 3 && ++opts[sel] == 2)
            opts[sel] = 0;
         if (sel == 4)
            done = 1;
      }
      if (key[KEY_ESC]) {
         sel = 4;
         prev = 4;
      }
      if (!key[KEY_UP] && !joy[JOY_ANY].up) up_l = 0;
      if (!key[KEY_DOWN] && !joy[JOY_ANY].down) down_l = 0;
      if (!key[KEY_LEFT] && !joy[JOY_ANY].left) left_l = 0;
      if (!key[KEY_RIGHT] && !joy[JOY_ANY].right) right_l = 0;
      if (!key[KEY_SPACE] && !joy[JOY_ANY].b[0] && !key[KEY_ENTER]) enter_l = 0;
      if (prev != sel && snd_on)
         play_sample(SND(SN_SND_NA), 200, 128, 1000, 0);
      draw_gfx_options(sel);
   }
   if (snd_on)
      play_sample(SND(SN_SND_SELECT), 200, 128, 1000, 0);
   fade_out(4);
}

/* 0x27308 */
void options_menu(void)
{
   int sel = 0, prev, done = 0, volume_changed, refade, v;
   int up_l = 1, down_l = 1, left_l = 1, right_l = 1, enter_l = 1;   /* see key_config() */
   draw_options(0);
   fade_in(dat_spacehog[SH_PAL].dat, 4);
   clear_keybuf();
   for (;;) {
      refade = 0;
      if (key[KEY_ESC] || done)
         break;
      if (key[KEY_TAB] && key[KEY_ENTER])
         console();
      volume_changed = 0;
      if (joy_present)
         poll_joystick();
      prev = sel;
      if (joy[JOY_ANY].b[1])
         sel = 7;
      if ((joy[JOY_ANY].up || key[KEY_UP]) && !up_l) {
         if (--sel < 0) sel = 0;
         up_l = 1;
      }
      if ((joy[JOY_ANY].down || key[KEY_DOWN]) && !down_l) {
         if (++sel > 7) sel = 7;
         down_l = 1;
      }
      if ((joy[JOY_ANY].right || key[KEY_RIGHT]) && !right_l) {
         right_l = 1;
         if (sel == 0)
            two_players = (unsigned char)MIN(two_players + 1, 1);
         if (sel == 1 && ++difficulty > 6)
            difficulty = 6;
         if (sel == 2)
            g_player[0].control = (unsigned char)next_control(0, 1);
         if (sel == 3)
            g_player[1].control = (unsigned char)next_control(1, 1);
         if (sel == 5 && !opt_nosound && music_on) {
            v = midi_vol + 10;
            midi_vol = (unsigned char)(v > 255 ? 255 : v);
            music_on = 1;
            volume_changed = 1;
            right_l = 0;
         }
         if (sel == 6 && !opt_nosound && snd_on) {
            v = digi_vol + 10;
            digi_vol = (unsigned char)(v > 255 ? 255 : v);
            snd_on = 1;
            volume_changed = 1;
            right_l = 0;
         }
         if (music_on && !music_playing) {
            play_midi(MUS(SN_MIDI_TITLE), TRUE);
            music_playing = 1;
         }
      }
      if ((joy[JOY_ANY].left || key[KEY_LEFT]) && !left_l) {
         left_l = 1;
         if (sel == 0)
            two_players = (unsigned char)MAX(two_players - 1, 0);
         if (sel == 1 && --difficulty < 1)
            difficulty = 1;
         if (sel == 2)
            g_player[0].control = (unsigned char)next_control(0, -1);
         if (sel == 3)
            g_player[1].control = (unsigned char)next_control(1, -1);
         if (sel == 5 && !opt_nosound && music_on) {
            v = midi_vol - 10;
            midi_vol = (unsigned char)(v < 0 ? 0 : v);
            volume_changed = 1;
            left_l = 0;
         }
         if (sel == 6 && !opt_nosound && snd_on) {
            v = digi_vol - 10;
            digi_vol = (unsigned char)(v < 0 ? 0 : v);
            volume_changed = 1;
            left_l = 0;
         }
         if (midi_vol == 0) {
            music_on = 0;
            volume_changed = 1;
            stop_midi();
            music_playing = 0;
         }
         if (digi_vol == 0) {
            snd_on = 0;
            volume_changed = 1;
         }
      }
      if ((joy[JOY_ANY].b[0] || key[KEY_ENTER] || key[KEY_SPACE]) && !enter_l) {
         if (sel == 0 && two_players == 0)
            two_players = 1;
         else if (sel == 0 && two_players == 1)
            two_players = 0;
         if (sel == 2 && g_player[0].control == CTRL_KEYBOARD) {
            key_config(0);
            refade = 1;
         }
         if (sel == 3 && g_player[1].control == CTRL_KEYBOARD) {
            key_config(1);
            refade = 1;
         }
         if (sel == 4) {
            gfx_options();
            refade = 1;
         }
         if (!opt_nosound) {
            if (sel == 5 && music_on) {
               music_on = 0;
               volume_changed = 1;
            }
            else if (sel == 5 && !music_on) {
               music_on = 1;
               volume_changed = 1;
               if (midi_vol == 0)
                  midi_vol = 200;
            }
         }
         if (!opt_nosound) {
            if (sel == 6 && snd_on) {
               snd_on = 0;
               volume_changed = 1;
            }
            else if (sel == 6 && !snd_on) {
               snd_on = 1;
               volume_changed = 1;
               if (digi_vol == 0)
                  digi_vol = 200;
            }
         }
         if (midi_vol == 0) {
            music_on = 0;
            volume_changed = 1;
         }
         if (digi_vol == 0) {
            snd_on = 0;
            volume_changed = 1;
         }
         if (sel == 7)
            done = 1;
         enter_l = 1;
      }
      if (key[KEY_ESC]) {
         sel = 7;
         prev = 7;
      }
      if (!key[KEY_UP] && !joy[JOY_ANY].up) up_l = 0;
      if (!key[KEY_DOWN] && !joy[JOY_ANY].down) down_l = 0;
      if (!key[KEY_LEFT] && !joy[JOY_ANY].left) left_l = 0;
      if (!key[KEY_RIGHT] && !joy[JOY_ANY].right) right_l = 0;
      if (!key[KEY_SPACE] && !joy[JOY_ANY].b[0] && !key[KEY_ENTER]) enter_l = 0;
      if (prev != sel && snd_on)
         play_sample(SND(SN_SND_NA), 200, 128, 1000, 0);
      draw_options(sel);
      if (volume_changed && snd_on)
         set_volume(digi_vol, -1);
      if (volume_changed && music_on)
         set_volume(-1, midi_vol);
      if (refade)
         fade_in(dat_spacehog[SH_PAL].dat, 4);
   }
   if (snd_on)
      play_sample(SND(SN_SND_SELECT), 200, 128, 1000, 0);
   fade_out(4);
}
