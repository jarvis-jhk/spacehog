/* FLOS console (TAB+ENTER), 0x7180. */
#include "sh.h"

char midi_file[80];               /* 0xc03b8 */
MIDI *custom_midi;                /* 0xcce00 */
char player_mortal = 1;           /* 0x73c08: 0 = cheat shield on */

/* 0x6abc: scroll a screen region by (dx,dy), n times */
static void scroll_region(BITMAP *b, int x1, int y1, int x2, int y2, int dx, int dy, int n)
{
   int i;
   for (i = 0; i < n; i++)
      blit(b, b, x1, y1, x1 + dx, y1 + dy, x2 - x1 + 1, y2 - y1 + 1);
}

static void con_scroll(int n)
{
   scroll_region(screen, 0x1f, 0x15, 0xef, 0x95, 0, -1, n);
}

static void con_line(const char *s)
{
   textout(screen, FNT(SH_FONT_SMALL), s, 0x26, 0x8c, 0x4b);
}

void console(void)
{
   BITMAP *saved = create_bitmap(320, 200);
   FONT *f = FNT(SH_FONT_SMALL);
   char cmd[96], history[96], prev[96], squeezed[96];
   int quit = 0, n, k, sc, i, j;
   FILE *fp;

   blit(screen, saved, 0, 0, 0, 0, 320, 200);
   fade_out(16);
   clear(screen);
   draw_rle_sprite(screen, RLE(SH_HANDCPU), 12, 4);
   textout(screen, f, "Free Lunch Design, FLOS, Version 1.0", 0x26, 0x6e, 0x4a);
   textout(screen, f, "Type exit to continue...", 0x26, 0x7a, 0x4a);
   fade_in(dat_spacehog[SH_PAL].dat, 16);
   clear_keybuf();
   history[0] = 0;
   prev[0] = 0;
   do {
      n = 0;
      textout(screen, f, "%", 0x26, 0x8c, 0x4a);
      cmd[0] = '_';
      cmd[1] = 0;
      textout(screen, f, cmd, 0x2c, 0x8c, 0x4a);
      do {
         k = readkey();
         cmd[n] = (char)k;
         cmd[n + 1] = '_';
         cmd[n + 2] = 0;
         sc = k >> 8;
         n++;
         if (sc == KEY_UP) {
            strcpy(cmd, history);
            n = (int)strlen(cmd);
            cmd[n] = '_';
            cmd[n + 1] = 0;
         }
         if (sc == KEY_DOWN) {
            cmd[0] = '_';
            cmd[1] = 0;
         }
         if (sc == 0x54) {
            save_screenshot();
            cmd[n] = 0;
            cmd[n - 1] = '_';
            n--;
         }
         if ((sc == KEY_BACKSPACE || sc == KEY_DEL) && n == 1) {
            cmd[1] = 0;
            n = 0;
            cmd[0] = '_';
         }
         if ((sc == KEY_BACKSPACE || sc == KEY_DEL) && n > 1) {
            cmd[n - 1] = 0;
            cmd[n - 2] = '_';
            n -= 2;
         }
         if (sc == KEY_ESC) {
            quit = 1;
            strcpy(cmd, "exit");
         }
         if (sc == KEY_ENTER)
            cmd[n - 1] = 0;
         if (text_length(f, cmd) > 190) {
            cmd[n] = 0;
            cmd[n - 1] = '_';
            n--;
         }
         rectfill(screen, 0x2c, 0x8c, 0xef, 0x94, 0x40);
         textout(screen, f, cmd, 0x2c, 0x8c, 0x48);
      } while (sc != KEY_ENTER && !quit);
      strcpy(history, cmd);
      /* remove spaces ("reset levels" -> "resetlevels") */
      for (i = 0; i < n; i++) {
         if (cmd[i] == ' ') {
            for (j = i; j < n; j++)
               cmd[j] = cmd[j + 1];
            i--;
         }
      }
      strcpy(squeezed, cmd);
      con_scroll(6);
      n = 0;
      if (!strcasecmp(cmd, "exit") || !strcasecmp(cmd, "quit")) {
         quit = 1;
         n = 2;
         strcpy(cmd, "bye...");
      }
      if (!strcasecmp(cmd, "password")) {
         n = 1;
         strcpy(cmd, "What's the password?");
      }
      if (!strcasecmp(prev, "password") && !strcasecmp(cmd, "rock")) {
         opt_cheats = 1;
         n = 2;
         strcpy(cmd, "Cheat is now enabled. 'Reset' to turn off.");
      }
      if (!strcasecmp(cmd, "resetKeys")) {
         reset_keys();
         n = 2;
         strcpy(cmd, "Keys restored to default.");
      }
      if (!strcasecmp(cmd, "resetScores")) {
         reset_scores();
         n = 2;
         strcpy(cmd, "Highscores reseted.");
      }
      if (!strcasecmp(cmd, "vSync") || !strcasecmp(cmd, "vSyncOn")) {
         opt_vsync = 1;
         n = 2;
         strcpy(cmd, "Vsync is now ON.");
      }
      if (!strcasecmp(cmd, "vSyncOff")) {
         opt_vsync = 0;
         n = 2;
         strcpy(cmd, "Vsync is now OFF.");
      }
      if (!strcasecmp(cmd, "cheat")) {
         n = 2;
         strcpy(cmd, "Too easy. Try harder.");
      }
      if (!strcasecmp(cmd, "cheater")) {
         n = 2;
         strcpy(cmd, "What kind of fool are you? Try harder.");
      }
      if (!strcasecmp(cmd, "johanPeitz")) { n = 2; strcpy(cmd, "Design, programming and gfx."); }
      if (!strcasecmp(cmd, "andreasQassim")) { n = 2; strcpy(cmd, "Character design."); }
      if (!strcasecmp(cmd, "shawnHargreaves")) { n = 2; strcpy(cmd, "Creator of Allegro."); }
      if (!strcasecmp(cmd, "emilRolandsson")) { n = 2; strcpy(cmd, "First gametester."); }
      if (!strcasecmp(cmd, "ivanBaldo")) { n = 2; strcpy(cmd, "Pixelperfect collision routines."); }
      if (!strcasecmp(cmd, "spacehog")) { n = 2; strcpy(cmd, "That's the name of the game."); }
      if (!strcasecmp(cmd, "viper") || !strcasecmp(cmd, "bulldog") || !strcasecmp(cmd, "albatross")) {
         n = 2;
         strcpy(cmd, "A great spaceship!");
      }
      if (!strcasecmp(cmd, "maximillianDrake")) { n = 2; strcpy(cmd, "Piloting the Bulldog."); }
      if (!strcasecmp(cmd, "sheilaLonghart")) { n = 2; strcpy(cmd, "Whatch out for those Hellfires!."); }
      if (!strcasecmp(cmd, "billy68")) { n = 2; strcpy(cmd, "Where's your strait-jacket?"); }
      if (!strcasecmp(cmd, "brockVixel")) { n = 2; strcpy(cmd, "Sir. Yes Sir. Right away, sir!"); }
      if (!strcasecmp(cmd, "reset") || !strcasecmp(cmd, "restore")) {
         opt_vsync = 1;
         opt_cheats = 0;
         custom_level = 0;
         player_mortal = 1;
         custom_music = 0;
         n = 2;
         strcpy(cmd, "Settings returned to normal. (info)");
      }
      if (!strcasecmp(cmd, "resetlevels")) {
         custom_level = 0;
         n = 2;
         strcpy(cmd, "Levels returned to normal.");
      }
      if (!strcasecmp(cmd, "loadlevel")) {
         n = 1;
         strcpy(cmd, "Enter filename for custom level: ");
      }
      if (!strcasecmp(cmd, "loadmidi")) {
         n = 1;
         strcpy(cmd, "Enter filename for level midi: ");
      }
      if (!strcasecmp(cmd, "resetmidi")) {
         custom_music = 0;
         n = 2;
         strcpy(cmd, "Midi returned to normal.");
      }
      if (!strcasecmp(cmd, "level")) {
         if (custom_level == 0)
            strcpy(cmd, "Normal levels.");
         else {
            strcpy(cmd, "Next level is: ");
            strcat(cmd, level_file);
         }
         n = 2;
      }
      if (!strcasecmp(prev, "loadlevel")) {
         n = 2;
         snprintf(level_file, sizeof(level_file), "%s", cmd);
         fp = sys_fopen_save(cmd, "rb");
         if (!fp) {
            strcpy(cmd, "File not found. Levels set to normal.");
            custom_level = 0;
         }
         else {
            fclose(fp);
            snprintf(cmd, sizeof(cmd), "Next level is: %s", level_file);
            con_line(cmd);
            con_scroll(6);
            strcpy(cmd, "Type 'reset levels' to cancel.");
            custom_level = 1;
         }
      }
      if (!strcasecmp(prev, "loadmidi")) {
         n = 2;
         snprintf(midi_file, sizeof(midi_file), "%s", cmd);
         fp = sys_fopen_save(cmd, "rb");
         if (!fp)
            strcpy(cmd, "File not found. Midi not changed");
         else {
            char path[1024];
            fclose(fp);
            sys_save_path(midi_file, path, sizeof(path));
            custom_midi = load_midi(path);
            snprintf(cmd, sizeof(cmd), "Level midi is now: %s", midi_file);
            con_line(cmd);
            con_scroll(6);
            strcpy(cmd, "Type 'reset midi' to cancel.");
            custom_music = 1;
         }
      }
      if (!strcasecmp(cmd, "screenshot") || !strcasecmp(cmd, "takeScreenshot")) {
         blit(screen, bmp_buf, 0, 0, 0, 0, 320, 200);
         blit(saved, screen, 0, 0, 0, 0, 320, 200);
         save_screenshot();
         blit(bmp_buf, screen, 0, 0, 0, 0, 320, 200);
         n = 2;
         strcpy(cmd, "Screenshot taken.");
      }
      if (!strcasecmp(cmd, "info")) {
         if (custom_level == 0)
            con_line("Using normal levels.");
         else {
            snprintf(cmd, sizeof(cmd), "Using level: %s", level_file);
            con_line(cmd);
         }
         con_scroll(6);
         if (custom_music == 0)
            con_line("Using original music.");
         else {
            snprintf(cmd, sizeof(cmd), "Now playing: %s", midi_file);
            con_line(cmd);
         }
         con_scroll(6);
         con_line(opt_vsync ? "Vsync is ON." : "Vsync is OFF.");
         con_scroll(6);
         if (!opt_cheats)
            con_line("Cheat is OFF.");
         else {
            con_line("Cheat is ON.");
            con_scroll(6);
            con_line(player_mortal ? "Shield is OFF." : "Shield is ON.");
            con_scroll(6);
         }
         con_scroll(12);
      }
      if (!strcasecmp(cmd, "help")) {
         con_line("Welcome to OPERATION SPACEHOG.");
         con_scroll(6);
         con_line("This is the console mode (Tab+ENTER).");
         con_scroll(6);
         con_line("From here you can alter settings and");
         con_scroll(6);
         con_line("enter cheats (if you know the codes!).");
         con_scroll(6);
         con_line("See README.TXT.txt for further info.");
         con_scroll(12);
      }
      if (n == 9) {
         strcpy(cmd, "You must first enter the cheat-activator.");
         n = 2;
      }
      if (n != 0) {
         con_line(cmd);
         con_scroll(12);
      }
      strcpy(prev, squeezed);
   } while (!quit);
   fade_out(16);
   blit(saved, screen, 0, 0, 0, 0, 320, 200);
   fade_in(dat_spacehog[SH_PAL].dat, 16);
   destroy_bitmap(saved);
}
