/* Gameplay functions (split from the original play.c translation). DOS addresses are in
 * the comment above each definition. */
#ifndef SH_PLAY_H
#define SH_PLAY_H
#include "gglobals.h"

int level_load_mem(const unsigned char *buf, size_t size);
int level_load_file(const char *name);
void init_ebullet_templates(void);
void init_enemy_templates(void);
void init_ship_types(void);
void init_stars(void);
void update_stars(void);
void draw_stars(void);
void draw_shots(void);
void draw_ebullets(void);
void draw_bgobjs(void);
void draw_enemies(void);
void draw_explosions(void);
void draw_pickups(void);
void draw_status_bar(int p);
void render_frame(void);
void lose_weapons(int p);
void respawn_player(int p);
void init_player(int p);
int find_free_ebullet(void);
void enemy_fire(int e);
void scroll_bgobjs(unsigned char layer);
void spawn_pickup(int x,int y,int type);
int spawn_explosion(int x,int y,int vx,int vy,int size);
void kill_enemy(int e,int killer);
void update_objects(void);
int dialog(const char *text,RLE_SPRITE *face,int mode,int typewriter);
int launch_sequence(void);
void next_level(void);
int ship_select(int p);
int prepare_game(void);
int pick_missile_target(void);
int pick_strongest_target(void);
int find_free_shot(void);
void player_fire(int p);
void buy_powerup(int p);
void player_hit(int p);
void collisions(void);
void use_special(int p);
void spawn_waves(void);
void pause_game(void);
void level_end_tally(void);
void remove_missiles(void);
void pilot_comments(int level);
void spawn_particle(int x,int y,fixed speed,unsigned int angle,int color);
int ending(void);
void play_game(void);

#endif
