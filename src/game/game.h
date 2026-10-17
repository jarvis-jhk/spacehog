/* Operation Spacehog game code (translated from the DOS executable). */
#ifndef SH_GAME_H
#define SH_GAME_H

int game_main(int argc, char **argv);
extern int player_input_latched;   /* --input-latch: player input once per tick (pinput.c) */

#endif
