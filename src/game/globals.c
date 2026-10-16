/* Gameplay globals with recovered names (initial values from SH.EXE .data). */
#include "sh.h"

PLAYER g_player[2];
ENEMY g_enemy[50];
ENEMY g_enemy_tmpl[40];
SHOT g_shot[200];
LEVELEVENT g_event[256];
PICKUP g_pickup[10];
EXPLOSION g_expl[50];
EBULLET g_ebullet[100];
EBULLET g_ebullet_tmpl[5];
PARTICLE g_particle[100];
WAVE g_wave[10];
BGOBJ g_bgobj[10];
STAR g_star[255];
SHIPTYPE g_shiptype[3];
LEVELHDR g_levelhdr;
unsigned char bg_tiles[256];
int g_title_y;
int g_level;
BITMAP *bmp_bar[2];
int custom_level, custom_music;   /* 0xc8950, 0xcce18 */

/* 0x73ad4 */
int g_ship_specials[3] = { 1, 2, 1 };
/* 0x73ae4 / 0x73b24: velocity per direction (16 directions) */
int g_dir_vx[16] = { 6, 5, 4, 2, 0, -2, -4, -5, -6, -5, -4, -2, 0, 2, 4, 5 };
int g_dir_vy[16] = { 0, -2, -4, -5, -6, -5, -4, -2, 0, 2, 4, 5, 6, 5, 4, 2 };
/* 0x73bb4: mothership launch keyframes {frames, dx, dy, flame h, flame v} */
int g_launch_path[20] = { 380, -1, 0, 35, 5, 60, 0, 0, 15, 10, 200, 1, -1, 35, 35, 0, 0, 0, 0, 0 };
/* 0x73c10: x positions of the weapon lamps in the status bar */
int g_lamp_x[9] = { 0x47, 0x5d, 0x70, 0x8a, 0xaa, 0xc9, 0xe8, 0x105, 0x122 };
/* 0x73c58: animation phase counters (index = animation speed class) */
unsigned char g_anim[4];
char dbg_fps;                     /* 0x73c06 */
char dbg_enemyinfo;               /* 0x73c09 */
