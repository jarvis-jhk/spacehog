/* Operation Spacehog - shared declarations for the translated game code.
 *
 * Names: functions/globals keep their original address as a suffix or comment
 * (e.g. 0x582c) so they can be cross-referenced with the Ghidra project.
 */
#ifndef SH_SH_H
#define SH_SH_H

#include "../compat/alleg.h"
#include "../compat/dos.h"
#include "../compat/input.h"
#include "../platform/sys.h"
#include "structs.h"
#include "dat.h"
#include "game.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#define rand() dos_rand()
#define srand(x) dos_srand(x)

/* --------------------------------------------------------------- datafiles */
extern DATAFILE *dat_spacehog;    /* 0xcc854 */
extern DATAFILE *dat_shlev;       /* 0xccdfc */
extern DATAFILE *dat_shbgs;       /* 0xc8fcc */
extern DATAFILE *dat_shsnd;       /* 0xc8594 */
#define SPR(i) ((BITMAP *)dat_spacehog[i].dat)
#define RLE(i) ((RLE_SPRITE *)dat_spacehog[i].dat)
#define FNT(i) ((FONT *)dat_spacehog[i].dat)
#define SND(i) ((SAMPLE *)dat_shsnd[i].dat)
#define MUS(i) ((MIDI *)dat_shsnd[i].dat)

/* --------------------------------------------------------------- flags & options */
extern char opt_nosound;          /* 0x73c04 /nosound */
extern char opt_cheats;           /* 0x73c07 /judge or /betatest */
extern char music_playing;        /* 0x73c0c */
extern unsigned char snd_on;      /* 0xc053c */
extern unsigned char music_on;    /* 0xa942c */
extern unsigned char digi_vol;    /* 0xa5f00 */
extern unsigned char midi_vol;    /* 0xaa080 */
extern int difficulty;            /* 0x73acc */
extern unsigned char two_players; /* 0xa9434 (0 = one player, 1 = two) */
extern unsigned char opts[10];    /* 0xc0520 */
extern const unsigned char opts_default[10]; /* 0x73c44 */
extern char mouse_present;        /* 0xcce14 */
/* 0xae33c: set once from the joystick detection in the original; the port follows
 * hot-plugging (identical as long as the pad count does not change) */
#define joy_present (num_joysticks > 0)
extern int screenshot_no;         /* 0x73c54 */

/* --------------------------------------------------------------- timing */
extern volatile int g_tick;       /* 0xa5f08, +1 every 30 ms */
extern volatile int g_fps;        /* 0xcce08 */
extern volatile int g_frames;     /* 0xcce10 */
void wait_tick(void);             /* busy-wait helper (sys_idle) */

/* --------------------------------------------------------------- persistent data */
extern HISCORE g_hiscore[10];     /* 0xc0408 */
extern int g_keys[14];            /* 0xc8f94: per player up,down,right,left,fire,select,special */
enum { PK_UP, PK_DOWN, PK_RIGHT, PK_LEFT, PK_FIRE, PK_SELECT, PK_SPECIAL };

/* --------------------------------------------------------------- graphics */
extern BITMAP *bmp_world;         /* 0xccc58 960x240 */
extern BITMAP *bmp_buf;           /* 0xb03b0 320x240 */
extern RGB_MAP rgb_table;         /* 0xc0540 */
extern COLOR_MAP light_white;     /* 0xb03b4 */
extern COLOR_MAP light_green;     /* 0x95344 */
extern int g_cos128[256];         /* 0xcc858 */
extern int g_sin128[256];         /* 0xca6f4 */
extern RLE_SPRITE *expl_small[20];  /* 0xc8544 */
extern RLE_SPRITE *expl_medium[42]; /* 0xae290 */
extern RLE_SPRITE *expl_big[50];    /* 0xa5e38 */
extern RLE_SPRITE *missile_rot[16]; /* 0xae250 */
extern BITMAP *bigmissile_rot[48]; /* 0xae4fc [dir*3 + frame] */
extern char level_file[80];       /* 0xaa02c */

/* --------------------------------------------------------------- game objects */
extern PLAYER g_player[2];        /* 0xae088 */
extern ENEMY g_enemy[50];         /* 0xae6c8 */
extern ENEMY g_enemy_tmpl[40];    /* 0xc8fd0 */
extern SHOT g_shot[200];          /* 0xa5f0c */
extern LEVELEVENT g_event[256];        /* 0xaa088 */
/* --------------------------------------------------------------- player input (pinput.c)
 * Everything the game reads from a player's devices, as one snapshot per player: the
 * player's 7 key bindings, its pad (pjoy), the mouse and Enter. Game code reads player
 * input only through these accessors. Default: sampled live at every read, so the read
 * points stay those of the original (difftest-pinned). player_input_latched = 1 (lockstep,
 * --input-latch): sampled once per tick at the tick wait and served from that snapshot. */
enum {
   PI_PAD_UP = 1 << 8, PI_PAD_DOWN = 1 << 9, PI_PAD_LEFT = 1 << 10, PI_PAD_RIGHT = 1 << 11,
   PI_PAD_B1 = 1 << 12, PI_PAD_B2 = 1 << 13, PI_PAD_B3 = 1 << 14,   /* pjoy(p)->b[0..2] */
   PI_ENTER = 1 << 15,
   PI_MOUSE_SHIFT = 16                                               /* mouse_b, 3 bits */
};
typedef struct PLAYER_INPUT {
   unsigned int bits;     /* 1 << PK_x = key binding x down, PI_* */
   int mouse_x, mouse_y;
} PLAYER_INPUT;
PLAYER_INPUT player_input(int p);
PLAYER_INPUT player_input_sample(int p);   /* always live */
void player_input_latch(void);            /* tick wait; no-op unless latched */
int pinput_net_map_key(int sc);           /* netplay hooks (input.c) */
int pinput_net_pad_slot(void);
static inline int pin_key(int p, int k) { return (player_input(p).bits >> k) & 1; }
static inline int pin_pad(int p, int bit) { return (player_input(p).bits & bit) != 0; }
static inline int pin_enter(int p) { return (player_input(p).bits & PI_ENTER) != 0; }
static inline int pin_mouse_b(int p) { return (player_input(p).bits >> PI_MOUSE_SHIFT) & 7; }
static inline int pin_mouse_x(int p) { return player_input(p).mouse_x; }
static inline int pin_mouse_y(int p) { return player_input(p).mouse_y; }
extern PICKUP g_pickup[10];         /* 0xae340 */
extern EXPLOSION g_expl[50];         /* 0xc8954 explosions */
extern EBULLET g_ebullet[100];        /* 0xcac34 enemy bullets */
extern EBULLET g_ebullet_tmpl[5];   /* 0xc8598 */
extern PARTICLE g_particle[100];        /* 0xa5348 */
extern WAVE g_wave[10];           /* 0xccc68 */
extern BGOBJ g_bgobj[10];         /* 0xcaaf4 */
extern STAR g_star[255];          /* 0xa9438 */
extern SHIPTYPE g_shiptype[3];    /* 0xc886c */
extern LEVELHDR g_levelhdr;       /* 0xc88fc */
extern unsigned char bg_tiles[256];
extern int g_title_y;             /* 0xccc64 */
extern int g_level;               /* 0xccdf8 */
extern BITMAP *bmp_bar[2];        /* 0xccc5c */
extern int g_ship_specials[3];    /* 0x73ad4 */
extern int g_dir_vx[16], g_dir_vy[16];
extern int g_launch_path[20];     /* 0x73bb4 */
extern int g_lamp_x[9];           /* 0x73c10 */
extern unsigned char g_anim[4];   /* 0x73c58 */
extern char dbg_fps, dbg_enemyinfo;
extern int g_ae6c4;               /* 0xae6c4 */

/* --------------------------------------------------------------- not yet understood */
extern int custom_level;          /* 0xc8950 console "loadlevel": play level_file next */
extern int custom_music;          /* 0xcce18 console "loadmidi": play midi_file in levels */

/* --------------------------------------------------------------- functions */
/* setup.c */
void sh_setup(void);              /* 0x582c */
void load_datafiles(void);        /* 0x4930 */
void load_saved(void);            /* 0x5234 */
void write_saved(void);           /* 0x6120 */
void sh_shutdown(void);           /* 0x62ec */
void reset_scores(void);          /* 0x4b84 */
void reset_keys(void);            /* 0x4d10 */
void setup_sound(void);           /* 0x5448 */
void calc_trig(void);             /* 0x5154 */
void progress_dot(int pos);       /* 0x40f8 */
void gen_explosions(int size);    /* 0x4228 */
void gen_missiles(void);          /* 0x46a4 */
int save_screenshot(void);        /* 0x4130 */
void show_help(void);             /* 0x27ce8 */

/* level.c */
void run_events(void);            /* 0x1c688 */
unsigned char spawn_path_enemy(int x, int y, int tmpl, unsigned char flags); /* 0x3ae8 */

/* not yet translated */
void intro(void);                 /* 0x1ee3c */
void frame_wait(void);
int intro_wait(int n);             /* 0x1eb4c */
int intro_type(const char *s, int x, int y, int color); /* 0x1eb90 */
extern char opt_vsync;            /* 0x73c05 */
void console(void);               /* 0x7180 (TAB+ENTER) */
int title_menu(void);             /* 0x23b5c */
int prepare_game(void);           /* 0x131bc ship selection etc., 0 = back to menu */
void play_game(void);             /* 0x21a7c the game until game over */
void credits(void);               /* 0x1e614 */
void hall_of_fame(void);          /* 0x1b088 */
void options_menu(void);          /* 0x27308 */
void game_over(int dummy);        /* 0x1b89c */
void get_key_name(int sc);        /* 0x24438 */
extern char key_name[12];         /* 0x95338 */
extern char midi_file[80];        /* 0xc03b8 */
extern MIDI *custom_midi;         /* 0xcce00 */
extern char player_mortal;        /* 0x73c08 */

#include "calllog.h"

#endif
