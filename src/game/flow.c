/* Program flow: command line, menu loop (0x27dbc). */
#include "sh.h"

/* 0x27dbc: main() of the DOS program */
int game_main(int argc, char **argv)
{
   int i, choice;
   for (i = 1; i < argc; i++) {
      if (!strcasecmp(argv[i], "/help")) show_help();
      if (!strcasecmp(argv[i], "/nosound")) opt_nosound = 1;
      if (!strcasecmp(argv[i], "/r_scores")) reset_scores();
      if (!strcasecmp(argv[i], "/r_keys")) reset_keys();
      if (!strcasecmp(argv[i], "/judge")) opt_cheats = 1;
      if (!strcasecmp(argv[i], "/betatest")) opt_cheats = 1;
   }
   sh_setup();
   for (i = 1; i < argc; i++) {
      if (!strcasecmp(argv[i], "/r_scores")) reset_scores();
      if (!strcasecmp(argv[i], "/r_keys")) reset_keys();
   }
   intro();
   stop_midi();
   music_playing = 0;
   choice = 1;
   while (choice != 0 && !sys_quit_requested) {
      choice = title_menu();
      if (choice == 4 && prepare_game())
         play_game();
      if (choice == 3)
         options_menu();
      if (choice == 2)
         hall_of_fame();
      if (choice == 1)
         credits();
   }
   if (music_playing)
      stop_midi();
   sh_shutdown();
   return 0;
}
