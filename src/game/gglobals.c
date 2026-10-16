/* Gameplay globals, see gglobals.h (initial values from SH.EXE .data). */
#include "sh.h"
#include "play.h"

unsigned char bg_tile;
int gun_vmax[5] = { 10, 9, 9, 8, 8 };
int gun_vy[5] = { 0, 1, -1, 2, -2 };
int cheat_latch;
int min_x = 20;
int min_y;
int max_x = 300;
int max_y = 240;
unsigned char anim_phase;
unsigned char anim_tick;
unsigned char frame_parity;
int particle_color = 31;
int firework_colors[5] = { 47, 79, 111, 143, 159 };
int launch_tick;
int view_y;
int mothership_x;
unsigned char mouse_used;
int step_ticks;
int level_step;
int bg_scroll;
int gameover_delay;
int boss_pip_hp;
int launch_step;
int bosses_alive;
int level_done;
int engine_sound;
int boss_enemy;
unsigned char pad_used;
int boss_pips;
int mothership_y;
int next_event;
int boss_hp;
