/* Gameplay globals (DOS address in the comment). */
#ifndef SH_GGLOBALS_H
#define SH_GGLOBALS_H
extern unsigned char bg_tile;     /* 0x73ae0 index into bg_tiles[] for the scrolling background */
extern int gun_vmax[5];           /* 0x73b64 gun shot target speed per stream */
extern int gun_vy[5];             /* 0x73b78 gun shot vertical speed per stream */
extern int cheat_latch;           /* 0x73c0a keypad +/- cheat held */
extern int min_x, min_y;          /* 0x73c34 player movement bounds */
extern int max_x, max_y;          /* 0x73c3c */
extern unsigned char anim_phase;  /* 0x73c4e +0x10 per tick */
extern unsigned char anim_tick;   /* 0x73c4f g_anim[] advance on odd ticks */
extern unsigned char frame_parity; /* 0x73c50 */
extern int particle_color;        /* 0x73c5c ending: rocket colour */
extern int firework_colors[5];    /* 0x73c60 ending: firework colours */
extern int launch_tick;           /* 0xa5344 ticks in the current launch keyframe */
extern int view_y;                /* 0xa5f04 vertical offset of the play field (screen shake) */
extern int mothership_x;          /* 0xa9430 launch sequence */
extern unsigned char mouse_used;  /* 0xaa07c a player uses the mouse: draw the cursor */
extern int step_ticks;            /* 0xaa084 ticks until the next level step (0x50) */
extern int level_step;            /* 0xae338 level position; events fire when it matches */
extern int bg_scroll;             /* 0xae4f8 background scroll position 0..0x27f */
extern int gameover_delay;        /* 0xae5bc ticks after both players died */
extern int boss_pip_hp;           /* 0xae6c0 boss hp per energy bar pip */
extern int launch_step;           /* 0xc03b4 current g_launch_path keyframe */
extern int bosses_alive;          /* 0xc052c */
extern int level_done;            /* 0xc0530 end-of-level event reached (F8 cheat) */
extern int engine_sound;          /* 0xc0534 mothership engine sample playing */
extern int boss_enemy;            /* 0xc0538 g_enemy index of the boss */
extern unsigned char pad_used;    /* 0xc8540 a player uses a gamepad */
extern int boss_pips;             /* 0xc8868 boss energy bar pips shown */
extern int mothership_y;          /* 0xca6f0 */
extern int next_event;            /* 0xcce04 first g_event not fired yet */
extern int boss_hp;               /* 0xcce0c boss hp at spawn */
#endif
