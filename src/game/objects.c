/* Enemies, bullets, pickups, explosions and particles; collisions; waves. */
#include "sh.h"
#include "play.h"

/* FUN_0000d07c @ 0xd07c */

int find_free_ebullet(void)

{
  int local_4;
  
  for (local_4 = 0; (g_ebullet[local_4].active != 0 && (local_4 < 100)); local_4++) {
  }
  if (99 < local_4) {
    local_4 = -1;
  }
  return local_4;
}


/* FUN_0000d0e0 @ 0xd0e0 */

void enemy_fire(int e)

{
  int *piVar1;
  int iVar2;
  unsigned int uVar3;
  fixed fVar4;
  int iVar5;
  int y;
  unsigned char local_15;
  int p;
  
  iVar2 = find_free_ebullet();
  if (-1 < iVar2) {
    g_ebullet[iVar2] = g_ebullet_tmpl[g_enemy[e].bullet];
    g_ebullet[iVar2].x = g_enemy[e].x + g_enemy[e].gun_x;
    g_ebullet[iVar2].y = g_enemy[e].y + g_enemy[e].gun_y;
    g_ebullet[iVar2].active = 1;
    piVar1 = dat_spacehog[g_ebullet[iVar2].sprite].dat;
    g_ebullet[iVar2].hw = *piVar1 >> 1;
    g_ebullet[iVar2].hh = piVar1[1] >> 1;
    g_ebullet[iVar2].w = *piVar1;
    g_ebullet[iVar2].h = piVar1[1];
    if (g_ebullet[iVar2].aim == 0) {
      uVar3 = rand();
      g_ebullet[iVar2].vx = g_cos128[uVar3 & 0xff] >> 6;
      g_ebullet[iVar2].vy = g_sin128[uVar3 & 0xff] >> 6;
    }
    else if (g_ebullet[iVar2].aim == 5) {
      p = 0;
      if (two_players != 0) {
        p = rand();
        p = p % 2;
      }
      y = g_player[p].x - g_enemy[e].x;
      fVar4 = fdiv(g_player[p].y - g_enemy[e].y,y);
      fVar4 = fatan(fVar4);
      iVar5 = fixtoi(fVar4);
      local_15 = (unsigned char)iVar5;
      if (y < 0) {
        local_15 += 0x80;
      }
      g_ebullet[iVar2].vx = g_cos128[local_15] >> 5;
      g_ebullet[iVar2].vy = g_sin128[local_15] >> 5;
    }
  }
  return;
}


/* FUN_0000d618 @ 0xd618 */

void scroll_bgobjs(unsigned char layer)

{
  int o;
  
  for (o = 0; o < 10; o++) {
    if (((g_bgobj[o].active != 0) && (layer == g_bgobj[o].layer)) &&
       (g_bgobj[o].x = g_bgobj[o].x + -1,
       g_bgobj[o].x + g_bgobj[o].hw < 0)) {
      g_bgobj[o].active = 0;
    }
  }
  return;
}


/* FUN_0000d6d4 @ 0xd6d4 */

void spawn_pickup(int x,int y,int type)

{
  int iVar1;
  int pk;
  
  for (pk = 0; (g_pickup[pk].active != 0 && (pk < 10)); pk++) {
  }
  if (pk < 10) {
    g_pickup[pk].active = 1;
    g_pickup[pk].frame = 0;
    g_pickup[pk].x = x;
    iVar1 = y;
    if (y < 0x14) {
      iVar1 = 0x14;
    }
    if (iVar1 < 0xdd) {
      if (y < 0x14) {
        y = 0x14;
      }
    }
    else {
      y = 0xdc;
    }
    g_pickup[pk].y = y;
    if (type == 1) {
      g_pickup[pk].sprite = 0x6e;
      g_pickup[pk].type = 1;
      g_pickup[pk].f_10 = 0x12;
      g_pickup[pk].hw = 9;
      g_pickup[pk].hh = 8;
      g_pickup[pk].w = 0x12;
      g_pickup[pk].h = 0x10;
    }
    else if (type == 2) {
      g_pickup[pk].sprite = 0x9e;
      g_pickup[pk].f_10 = 3;
      g_pickup[pk].type = 2;
      g_pickup[pk].hw = 6;
      g_pickup[pk].hh = 6;
      g_pickup[pk].w = 0xb;
      g_pickup[pk].h = 0xb;
    }
    else if (type == 3) {
      g_pickup[pk].sprite = 100;
      g_pickup[pk].f_10 = 3;
      g_pickup[pk].type = 3;
      g_pickup[pk].hw = 6;
      g_pickup[pk].hh = 6;
      g_pickup[pk].w = 0xb;
      g_pickup[pk].h = 0xb;
    }
  }
  return;
}


/* FUN_0000daec @ 0xdaec */

int spawn_explosion(int x,int y,int vx,int vy,int size)

{
  int ex;
  
  ex = 0;
  if (size < 0) {
    ex = -1;
  }
  else {
    for (; (g_expl[ex].active != 0 && (ex < 0x32)); ex++) {
    }
    if (ex < 0x32) {
      g_expl[ex].kind = (unsigned char)size;
      if (size == 0) {
        g_expl[ex].size = '\x14';
        g_expl[ex].frames = '\x14';
        if (snd_on != 0) {
          play_sample(dat_shsnd[SN_SND_EXP3].dat,200,(x * 0xff) / 0x140,1000,0);
        }
      }
      else if (size == 1) {
        g_expl[ex].size = '0';
        g_expl[ex].frames = '*';
        if (snd_on != 0) {
          play_sample(dat_shsnd[SN_SND_EXP2].dat,200,(x * 0xff) / 0x140,1000,0);
        }
      }
      else {
        g_expl[ex].size = 'P';
        g_expl[ex].frames = '2';
        if (snd_on != 0) {
          play_sample(dat_shsnd[SN_SND_EXP1].dat,200,(x * 0xff) / 0x140,1000,0);
        }
      }
      g_expl[ex].hw = (unsigned int)(g_expl[ex].size >> 1);
      g_expl[ex].hh = g_expl[ex].hw;
      g_expl[ex].active = 1;
      g_expl[ex].frame = 0;
      g_expl[ex].x = x;
      g_expl[ex].y = y;
      g_expl[ex].vx = vx;
      g_expl[ex].vy = vy;
    }
  }
  return ex;
}


/* FUN_0000dfd0 @ 0xdfd0 */


void kill_enemy(int e,int killer)

{
  int iVar1;
  int iVar2;
  int iVar3;
  int iVar4;
  int iVar5;
  int iVar6;
  int iVar7;
  
  if (killer != -1) {
    g_player[killer].score += g_enemy[e].points;
  }
  g_enemy[e].active = 0;
  if (g_enemy[e].path_pos != -1) {
    g_enemy[e].vx = g_enemy[e].x - g_enemy[e].vx;
    g_enemy[e].vy = g_enemy[e].y - g_enemy[e].vy;
    g_wave[g_enemy[e].wave].alive = g_wave[g_enemy[e].wave].alive + 0xff;
    if ((g_wave[g_enemy[e].wave].alive == 0) &&
       (g_wave[g_enemy[e].wave].active = 0, g_wave[g_enemy[e].wave].bonus != 0)) {
      spawn_pickup(g_enemy[e].x,g_enemy[e].y,1);
    }
  }
  if ((g_enemy[e].drop == 'c') && (bosses_alive = bosses_alive + -1, music_on != 0)) {
    play_midi(dat_shsnd[SN_MIDI_LEVEL].dat,-1);
    music_playing = 1;
  }
  if ((g_enemy[e].drop != 0) && (g_enemy[e].drop < 10)) {
    spawn_pickup(g_enemy[e].x,g_enemy[e].y,g_enemy[e].drop);
  }
  if (g_enemy[e].move == 6) {
    g_enemy[e].vx = 0;
    g_enemy[e].vy = 0;
  }
  spawn_explosion(g_enemy[e].x,g_enemy[e].y,g_enemy[e].vx,
               g_enemy[e].vy,g_enemy[e].expl_size);
  while (g_enemy[e].debris != 0) {
    iVar5 = rand();
    iVar1 = g_enemy[e].w;
    iVar2 = g_enemy[e].hw;
    iVar6 = rand();
    iVar3 = g_enemy[e].h;
    iVar4 = g_enemy[e].hh;
    iVar7 = rand();
    spawn_explosion(g_enemy[e].x + (iVar5 % iVar1 - iVar2),
                 g_enemy[e].y + (iVar6 % iVar3 - iVar4),g_enemy[e].vx,
                 g_enemy[e].vy,iVar7 % 2 + 1);
    g_enemy[e].debris--;
  }
  return;
}


/* FUN_0000e718 @ 0xe718 */

void update_objects(void)

{
  int iVar1;
  fixed fVar2;
  int y;
  int p;
  int local_4;
  
  if (opts[3] != 0) {
    update_stars();
  }
  if (engine_sound != 0) {
    if (snd_on != 0) {
      iVar1 = mothership_x + 0x50;
      if (0xff < iVar1) {
        iVar1 = 0xff;
      }
      adjust_sample(dat_shsnd[SN_SND_ENGINE].dat,(mothership_y * 0x14 + 4000) / mothership_x,iVar1,500,-1);
    }
    mothership_x += g_launch_path[launch_step * 5 + 1];
    mothership_y += g_launch_path[launch_step * 5 + 2];
    launch_tick++;
    if (g_launch_path[launch_step * 5] == launch_tick) {
      launch_tick = 0;
      launch_step++;
      if ((g_launch_path[launch_step * 5] == 0) && (engine_sound = 0, snd_on != 0)) {
        stop_sample(dat_shsnd[SN_SND_ENGINE].dat);
      }
    }
  }
  if ((level_step < 2) && (0x78 < g_title_y)) {
    g_title_y--;
  }
  if ((1 < level_step) && (g_title_y < 0xf1)) {
    g_title_y += 2;
  }
  for (local_4 = 0; local_4 < 0x32; local_4++) {
    if (g_enemy[local_4].active != 0) {
      if (g_enemy[local_4].path_pos == -1) {
        if (g_enemy[local_4].parent == 0xff) {
          g_enemy[local_4].x += g_enemy[local_4].vx;
          g_enemy[local_4].y += g_enemy[local_4].vy;
        }
        else {
          g_enemy[local_4].x = g_enemy[(signed char)g_enemy[local_4].parent].x + g_enemy[local_4].rel_x;
          g_enemy[local_4].y = g_enemy[(signed char)g_enemy[local_4].parent].y + g_enemy[local_4].rel_y;
        }
        if (g_enemy[local_4].move == 2) {
          g_enemy[local_4].x =
               g_enemy[local_4].x -
               (g_cos128[g_enemy[local_4].angle] >> ((unsigned char)g_enemy[local_4].radius & 0x1f));
          g_enemy[local_4].y =
               g_enemy[local_4].y -
               (g_sin128[g_enemy[local_4].angle] >> ((unsigned char)g_enemy[local_4].radius & 0x1f));
          g_enemy[local_4].angle = g_enemy[local_4].angle + (char)g_enemy[local_4].spin;
          g_enemy[local_4].x =
               g_enemy[local_4].x +
               (g_cos128[g_enemy[local_4].angle] >> ((unsigned char)g_enemy[local_4].radius & 0x1f));
          g_enemy[local_4].y =
               g_enemy[local_4].y +
               (g_sin128[g_enemy[local_4].angle] >> ((unsigned char)g_enemy[local_4].radius & 0x1f));
        }
        if (((g_enemy[local_4].move == 3) && (g_enemy[local_4].x < g_enemy[local_4].radius)) &&
           (g_enemy[local_4].rel_y != 0)) {
          if (g_enemy[local_4].spin == 0) {
            g_enemy[local_4].spin = g_enemy[local_4].vx;
          }
          g_enemy[local_4].vx = 0;
          if (g_enemy[local_4].rel_y == 1) {
            g_enemy[local_4].vx = g_enemy[local_4].spin;
          }
          g_enemy[local_4].rel_y--;
        }
        if (g_enemy[local_4].move == 4) {
          if (0 < g_enemy[local_4].vy) {
            g_enemy[local_4].vy--;
          }
          if (g_enemy[local_4].vy < 0) {
            g_enemy[local_4].vy++;
          }
          for (p = 0; p <= (int)(unsigned int)two_players; p++) {
            if (g_player[p].dead == 0) {
              if (((g_enemy[local_4].y - g_player[p].y < 10) &&
                  (-1 < g_enemy[local_4].y - g_player[p].y)) &&
                 (g_enemy[local_4].y < 0xb4)) {
                g_enemy[local_4].vy = 8;
              }
              if (((g_player[p].y - g_enemy[local_4].y < 10) &&
                  (-1 < g_player[p].y - g_enemy[local_4].y)) &&
                 (0x3c < g_enemy[local_4].y)) {
                g_enemy[local_4].vy = -8;
              }
            }
          }
        }
        if ((g_enemy[local_4].parent == 0xff) &&
           ((g_enemy[local_4].x < -g_enemy[local_4].hw || (g_enemy[local_4].y < -200)))) {
          g_enemy[local_4].active = 0;
        }
      }
      else if (g_enemy[local_4].move == 6) {
        g_enemy[local_4].x = g_enemy[local_4].path[g_enemy[local_4].path_pos];
        g_enemy[local_4].y =
             g_enemy[local_4].path[g_enemy[local_4].path_pos + 1] + g_enemy[local_4].rel_y;
        g_enemy[local_4].path_pos = g_enemy[local_4].vx * 2 + g_enemy[local_4].path_pos;
        if (g_enemy[local_4].path[g_enemy[local_4].path_pos] == -1000) {
          g_enemy[local_4].path_pos = 0;
        }
      }
      else {
        g_enemy[local_4].vx = g_enemy[local_4].x;
        g_enemy[local_4].vy = g_enemy[local_4].y;
        g_enemy[local_4].x = g_wave[g_enemy[local_4].wave].path[g_enemy[local_4].path_pos];
        g_enemy[local_4].y =
             g_wave[g_enemy[local_4].wave].path[g_enemy[local_4].path_pos + 1] +
             (unsigned int)g_wave[g_enemy[local_4].wave].yofs;
        g_enemy[local_4].path_pos = g_enemy[local_4].path_pos + (unsigned int)g_wave[g_enemy[local_4].wave].step * 2
        ;
        if (g_wave[g_enemy[local_4].wave].path[g_enemy[local_4].path_pos] == -1000) {
          g_enemy[local_4].active = 0;
          g_wave[g_enemy[local_4].wave].alive = g_wave[g_enemy[local_4].wave].alive + 0xff;
          g_wave[g_enemy[local_4].wave].bonus = 0;
          if (g_wave[g_enemy[local_4].wave].alive == 0) {
            g_wave[g_enemy[local_4].wave].active = 0;
          }
        }
      }
      if (g_enemy[local_4].fire_count < g_enemy[local_4].fire_rate) {
        g_enemy[local_4].fire_count++;
        if (g_enemy[local_4].fire_rate <= g_enemy[local_4].fire_count) {
          enemy_fire(local_4);
        }
      }
      else {
        g_enemy[local_4].fire_count = 0;
      }
    }
  }
  for (local_4 = 0; local_4 < 100; local_4++) {
    if (g_ebullet[local_4].active != 0) {
      g_ebullet[local_4].x += g_ebullet[local_4].vx;
      g_ebullet[local_4].y += g_ebullet[local_4].vy;
      if ((((0x140 < g_ebullet[local_4].x - g_ebullet[local_4].hw) ||
           (g_ebullet[local_4].x + g_ebullet[local_4].hw < 0)) ||
          (0xf0 < g_ebullet[local_4].y - g_ebullet[local_4].hh)) ||
         (g_ebullet[local_4].y + g_ebullet[local_4].hh < 0)) {
        g_ebullet[local_4].active = 0;
      }
    }
  }
  for (local_4 = 0; local_4 < 200; local_4++) {
    if (g_shot[local_4].active != 0) {
      if ((g_shot[local_4].kind & SHOT_MISSILE) != 0) {
        if (g_shot[local_4].target == -1) {
          if (g_shot[local_4].sprite == 0x5a) {
            iVar1 = pick_missile_target();
            g_shot[local_4].target = iVar1;
          }
          else {
            iVar1 = pick_strongest_target();
            g_shot[local_4].target = iVar1;
          }
          if (g_shot[local_4].target == -1) {
            g_shot[local_4].vx = g_dir_vx[g_shot[local_4].dir];
            g_shot[local_4].vy = g_dir_vy[g_shot[local_4].dir];
          }
        }
        else if (g_enemy[g_shot[local_4].target].active == 0) {
          g_shot[local_4].target = -1;
        }
        else {
          y = (g_enemy[g_shot[local_4].target].y - g_shot[local_4].y) >> 1;
          fVar2 = fdiv((g_enemy[g_shot[local_4].target].x - g_shot[local_4].x) >> 1,y);
          fVar2 = fatan(fVar2);
          iVar1 = fixtoi(fVar2);
          g_shot[local_4].dir = (char)(iVar1 >> 4) + '\x04';
          if (-1 < y) {
            g_shot[local_4].dir += '\b';
          }
          if (0xf < g_shot[local_4].dir) {
            g_shot[local_4].dir += 0xf0;
          }
          if (g_shot[local_4].vx < g_dir_vx[g_shot[local_4].dir]) {
            g_shot[local_4].vx++;
          }
          if (g_dir_vx[g_shot[local_4].dir] < g_shot[local_4].vx) {
            g_shot[local_4].vx--;
          }
          if (g_shot[local_4].vy < g_dir_vy[g_shot[local_4].dir]) {
            g_shot[local_4].vy++;
          }
          if (g_dir_vy[g_shot[local_4].dir] < g_shot[local_4].vy) {
            g_shot[local_4].vy--;
          }
        }
      }
      g_shot[local_4].x += g_shot[local_4].vx;
      g_shot[local_4].y += g_shot[local_4].vy;
      if (g_shot[local_4].kind == SHOT_GUN) {
        if (g_shot[local_4].vx < g_shot[local_4].vmax) {
          g_shot[local_4].vx++;
        }
        else {
          g_shot[local_4].vy = 0;
        }
      }
      if ((((0x140 < g_shot[local_4].x - g_shot[local_4].hw) ||
           (g_shot[local_4].x + g_shot[local_4].hw < 0)) ||
          ((0xf0 < g_shot[local_4].y - g_shot[local_4].hh ||
           (g_shot[local_4].y + g_shot[local_4].hh < 0)))) &&
         (g_shot[local_4].active = 0, (g_shot[local_4].kind & SHOT_MISSILE) != 0)) {
        if (-1 < g_shot[local_4].target) {
          g_enemy[g_shot[local_4].target].targeted = 0;
        }
        g_player[g_shot[local_4].owner].missiles = g_player[g_shot[local_4].owner].missiles + 0xff;
      }
    }
  }
  for (local_4 = 0; local_4 < 0x32; local_4++) {
    if (g_expl[local_4].active != 0) {
      g_expl[local_4].x += g_expl[local_4].vx;
      g_expl[local_4].y += g_expl[local_4].vy;
      g_expl[local_4].frame++;
      if (g_expl[local_4].frames == g_expl[local_4].frame) {
        g_expl[local_4].active = 0;
      }
    }
  }
  for (local_4 = 0; local_4 < 10; local_4++) {
    if (g_pickup[local_4].active != 0) {
      g_pickup[local_4].x--;
      if (g_pickup[local_4].x < -0x14) {
        g_pickup[local_4].active = 0;
      }
      if ((g_pickup[local_4].sprite == 0x6e) &&
         (g_pickup[local_4].frame = g_pickup[local_4].frame + 1, g_pickup[local_4].frame == 0x12)) {
        g_pickup[local_4].frame = 0;
      }
    }
  }
  return;
}


/* FUN_00013298 @ 0x13298 */

int pick_missile_target(void)

{
  int e;
  
  for (e = 0;
      (((g_enemy[e].active == 0 || (g_enemy[e].targeted != 0)) ||
       ((g_enemy[e].immune & NO_HOMING) != 0)) && (e < 0x32)); e++) {
  }
  if (e < 0x32) {
    g_enemy[e].targeted = 1;
  }
  else {
    for (e = 0;
        ((g_enemy[e].active == 0 || ((g_enemy[e].immune & NO_HOMING) != 0)) &&
        (e < 0x32)); e++) {
    }
    if (e < 0x32) {
      g_enemy[e].targeted = 1;
    }
    else {
      e = -1;
    }
  }
  return e;
}


/* FUN_00013418 @ 0x13418 */

int pick_strongest_target(void)

{
  int local_c;
  int local_8;
  int e;
  
  local_8 = 0;
  local_c = 0x32;
  for (e = 0; e < 0x32; e++) {
    if (((g_enemy[e].active != 0) && ((g_enemy[e].immune & NO_HOMING) == 0)) &&
       (local_8 < g_enemy[e].hp)) {
      local_c = e;
      local_8 = g_enemy[e].hp;
    }
  }
  if (local_c < 0x32) {
    g_enemy[local_c].targeted = 1;
  }
  else {
    local_c = -1;
  }
  return local_c;
}


/* 0x169cc: PPCOL-style pixel perfect collision between two 8-bit bitmaps (mask colour 0) */
int pixel_collide(BITMAP *a, BITMAP *b, int ax, int ay, int bx, int by)
{
   int a_x, a_y, b_x, b_y, w, h, x, y;
   if (!(ax < bx + b->w && bx < ax + a->w && ay < by + b->h && by < ay + a->h))
      return 0;
   if (bx < ax) { a_x = 0; b_x = ax - bx; } else { a_x = bx - ax; b_x = 0; }
   if (by < ay) { a_y = 0; b_y = ay - by; } else { a_y = by - ay; b_y = 0; }
   w = MIN(b->w - b_x, a->w - a_x);
   h = MIN(b->h - b_y, a->h - a_y);
   for (y = h - 1; y >= 0; y--)
      for (x = w - 1; x >= 0; x--)
         if (a->line[a_y + y][a_x + x] && b->line[b_y + y][b_x + x])
            return 1;
   return 0;
}


/* FUN_00016bf8 @ 0x16bf8 */

void collisions(void)

{
  int iVar1;
  int p;
  int local_8;
  int local_4;
  
  for (local_4 = 0; local_4 < 0x32; local_4++) {
    if (((g_enemy[local_4].active != 0) && (g_enemy[local_4].parent != 0xff)) &&
       (g_enemy[(signed char)g_enemy[local_4].parent].active == 0)) {
      kill_enemy(local_4,0xffffffff);
    }
  }
  for (local_4 = 0; local_4 < 200; local_4++) {
    for (local_8 = 0; local_8 < 0x32; local_8++) {
      if (((g_enemy[local_8].active != 0) && (g_shot[local_4].active != 0)) &&
         (g_enemy[local_8].invulnerable == 0)) {
        if (g_enemy[local_8].masked == 0) {
          if (g_shot[local_4].x - g_enemy[local_8].x < 0) {
            iVar1 = g_enemy[local_8].x - g_shot[local_4].x;
          }
          else {
            iVar1 = g_shot[local_4].x - g_enemy[local_8].x;
          }
          if (iVar1 < g_shot[local_4].hw + g_enemy[local_8].hw) {
            if (g_shot[local_4].y - g_enemy[local_8].y < 0) {
              iVar1 = g_enemy[local_8].y - g_shot[local_4].y;
            }
            else {
              iVar1 = g_shot[local_4].y - g_enemy[local_8].y;
            }
            if (iVar1 < g_shot[local_4].hh + g_enemy[local_8].hh) {
              g_shot[local_4].active = 0;
              if ((g_enemy[local_8].immune & SHOT_GUN & g_shot[local_4].kind) != 0) {
                g_shot[local_4].bounced = 1;
                g_shot[local_4].active = 1;
              }
              if ((g_enemy[local_8].immune & SHOT_SIDE & g_shot[local_4].kind) != 0) {
                g_shot[local_4].bounced = 1;
                g_shot[local_4].active = 1;
              }
              if ((g_enemy[local_8].immune & SHOT_MAGMA & g_shot[local_4].kind) != 0) {
                g_shot[local_4].bounced = 1;
                g_shot[local_4].active = 1;
              }
              if ((g_enemy[local_8].immune & g_shot[local_4].kind) == 0) {
                g_enemy[local_8].hit_flash = 1;
                g_enemy[local_8].hp = g_enemy[local_8].hp - (unsigned int)g_shot[local_4].damage;
                iVar1 = g_enemy[local_8].hp;
                if (iVar1 < 0) {
                  iVar1 = 0;
                }
                g_enemy[local_8].hp = iVar1;
                if (g_enemy[local_8].hp == 0) {
                  kill_enemy(local_8,g_shot[local_4].owner);
                }
              }
              if ((g_shot[local_4].kind & SHOT_MISSILE) != 0) {
                g_player[g_shot[local_4].owner].missiles = g_player[g_shot[local_4].owner].missiles + 0xff;
                g_enemy[local_8].targeted = 0;
                if (g_enemy[local_8].active != 0) {
                  if (g_enemy[local_8].path_pos == -1) {
                    spawn_explosion(g_shot[local_4].x,g_shot[local_4].y,g_enemy[local_8].vx,
                                 g_enemy[local_8].vy,0);
                  }
                  else {
                    spawn_explosion(g_shot[local_4].x,g_shot[local_4].y,
                                 g_dir_vx[g_shot[local_4].dir],g_dir_vy[g_shot[local_4].dir],0);
                  }
                }
              }
            }
          }
        }
        else {
          iVar1 = pixel_collide(dat_spacehog[g_shot[local_4].sprite].dat,
                               dat_spacehog[g_enemy[local_8].sprite].dat,
                               g_shot[local_4].x - g_shot[local_4].hw,
                               g_shot[local_4].y - g_shot[local_4].hh,
                               g_enemy[local_8].x - g_enemy[local_8].hw,
                               g_enemy[local_8].y - g_enemy[local_8].hh);
          if (iVar1 != 0) {
            g_shot[local_4].active = 0;
            if ((g_enemy[local_8].immune & SHOT_GUN & g_shot[local_4].kind) != 0) {
              g_shot[local_4].bounced = 1;
              g_shot[local_4].active = 1;
            }
            if ((g_enemy[local_8].immune & SHOT_SIDE & g_shot[local_4].kind) != 0) {
              g_shot[local_4].bounced = 1;
              g_shot[local_4].active = 1;
            }
            if ((g_enemy[local_8].immune & SHOT_MAGMA & g_shot[local_4].kind) != 0) {
              g_shot[local_4].bounced = 1;
              g_shot[local_4].active = 1;
            }
            if ((g_enemy[local_8].immune & g_shot[local_4].kind) == 0) {
              g_enemy[local_8].hit_flash = 1;
              g_enemy[local_8].hp = g_enemy[local_8].hp - (unsigned int)g_shot[local_4].damage;
              iVar1 = g_enemy[local_8].hp;
              if (iVar1 < 0) {
                iVar1 = 0;
              }
              g_enemy[local_8].hp = iVar1;
              if (g_enemy[local_8].hp == 0) {
                kill_enemy(local_8,g_shot[local_4].owner);
              }
            }
            if ((g_shot[local_4].kind & SHOT_MISSILE) != 0) {
              g_player[g_shot[local_4].owner].missiles = g_player[g_shot[local_4].owner].missiles + 0xff;
              g_enemy[local_8].targeted = 0;
              if (g_enemy[local_8].active != 0) {
                if (g_enemy[local_8].path_pos == -1) {
                  spawn_explosion(g_shot[local_4].x,g_shot[local_4].y,g_enemy[local_8].vx,
                               g_enemy[local_8].vy,0);
                }
                else {
                  spawn_explosion(g_shot[local_4].x,g_shot[local_4].y,
                               g_dir_vx[g_shot[local_4].dir],g_dir_vy[g_shot[local_4].dir],0);
                }
              }
            }
          }
        }
      }
    }
  }
  for (p = 0; p <= (int)(unsigned int)two_players; p++) {
    if (g_player[p].blast != 0) {
      for (local_8 = 0; local_8 < 0x32; local_8++) {
        if (((g_enemy[local_8].active != 0) && ((g_enemy[local_8].immune & 1) == 0)) &&
           (g_player[p].x < g_enemy[local_8].x)) {
          if (g_player[p].y - g_enemy[local_8].y < 0) {
            iVar1 = g_enemy[local_8].y - g_player[p].y;
          }
          else {
            iVar1 = g_player[p].y - g_enemy[local_8].y;
          }
          if (iVar1 < g_player[p].blast + g_enemy[local_8].hh) {
            g_enemy[local_8].hit_flash = 1;
            g_enemy[local_8].hp -= 3;
            iVar1 = g_enemy[local_8].hp;
            if (iVar1 < 0) {
              iVar1 = 0;
            }
            g_enemy[local_8].hp = iVar1;
            if (g_enemy[local_8].hp == 0) {
              kill_enemy(local_8,p);
            }
          }
        }
      }
    }
  }
  for (local_4 = 0; local_4 < 10; local_4++) {
    for (local_8 = 0; local_8 <= (int)(unsigned int)two_players; local_8++) {
      if (((g_pickup[local_4].active != 0) && (g_player[local_8].dead == 0)) &&
         (g_player[local_8].warp_in == 0)) {
        if (g_player[local_8].x - g_pickup[local_4].x < 0) {
          iVar1 = g_pickup[local_4].x - g_player[local_8].x;
        }
        else {
          iVar1 = g_player[local_8].x - g_pickup[local_4].x;
        }
        if (iVar1 < g_player[local_8].hw + g_pickup[local_4].hw) {
          if (g_player[local_8].y - g_pickup[local_4].y < 0) {
            iVar1 = g_pickup[local_4].y - g_player[local_8].y;
          }
          else {
            iVar1 = g_player[local_8].y - g_pickup[local_4].y;
          }
          if (iVar1 < g_player[local_8].hh + g_pickup[local_4].hh) {
            g_pickup[local_4].active = 0;
            if (g_pickup[local_4].type == 1) {
              g_player[local_8].powerup++;
            }
            if (g_pickup[local_4].type == 2) {
              g_player[local_8].specials =
                   g_player[local_8].specials + g_ship_specials[g_player[local_8].shipno];
            }
            if (g_pickup[local_4].type == 3) {
              g_player[local_8].lives++;
            }
            draw_status_bar(local_8);
            if (snd_on != 0) {
              play_sample(dat_shsnd[SN_SND_PICKUP].dat,200,(g_player[local_8].x * 0xff) / 0x140,1000,0);
            }
          }
        }
      }
    }
  }
  if (player_mortal != 0) {
    for (local_4 = 0; local_4 <= (int)(unsigned int)two_players; local_4++) {
      if (((g_player[local_4].dead == 0) && (g_player[local_4].warp_in == 0)) &&
         (g_player[local_4].invuln == 0)) {
        for (local_8 = 0; local_8 < 0x32; local_8++) {
          if (g_enemy[local_8].active != 0) {
            if (g_enemy[local_8].masked == 0) {
              if (g_player[local_4].x - g_enemy[local_8].x < 0) {
                iVar1 = g_enemy[local_8].x - g_player[local_4].x;
              }
              else {
                iVar1 = g_player[local_4].x - g_enemy[local_8].x;
              }
              if (iVar1 < g_player[local_4].hw + g_enemy[local_8].hw) {
                if (g_player[local_4].y - g_enemy[local_8].y < 0) {
                  iVar1 = g_enemy[local_8].y - g_player[local_4].y;
                }
                else {
                  iVar1 = g_player[local_4].y - g_enemy[local_8].y;
                }
                if (iVar1 < g_player[local_4].hh + g_enemy[local_8].hh) {
                  player_hit(local_4);
                }
              }
            }
            else {
              iVar1 = pixel_collide(dat_spacehog[g_player[local_4].sprite].dat,
                                   dat_spacehog[g_enemy[local_8].sprite].dat,
                                   g_player[local_4].x - g_player[local_4].hw,
                                   g_player[local_4].y - g_player[local_4].hh,
                                   g_enemy[local_8].x - g_enemy[local_8].hw,
                                   g_enemy[local_8].y - g_enemy[local_8].hh);
              if (iVar1 != 0) {
                player_hit(local_4);
              }
            }
          }
        }
      }
    }
  }
  if (player_mortal != 0) {
    for (local_4 = 0; local_4 <= (int)(unsigned int)two_players; local_4++) {
      if (((g_player[local_4].dead == 0) && (g_player[local_4].warp_in == 0)) &&
         (g_player[local_4].invuln == 0)) {
        for (local_8 = 0; local_8 < 100; local_8++) {
          if (g_ebullet[local_8].active != 0) {
            if (g_player[local_4].x - g_ebullet[local_8].x < 0) {
              iVar1 = g_ebullet[local_8].x - g_player[local_4].x;
            }
            else {
              iVar1 = g_player[local_4].x - g_ebullet[local_8].x;
            }
            if (iVar1 < g_player[local_4].hw + g_ebullet[local_8].hw) {
              if (g_player[local_4].y - g_ebullet[local_8].y < 0) {
                iVar1 = g_ebullet[local_8].y - g_player[local_4].y;
              }
              else {
                iVar1 = g_player[local_4].y - g_ebullet[local_8].y;
              }
              if (iVar1 < g_player[local_4].hh + g_ebullet[local_8].hh) {
                g_ebullet[local_8].active = 0;
                player_hit(local_4);
              }
            }
          }
        }
      }
    }
  }
  return;
}


/* FUN_0001c3c0 @ 0x1c3c0 */

void spawn_waves(void)

{
  char uVar1;
  int w;
  
  for (w = 0; w < 10; w++) {
    if ((g_wave[w].active != 0) && (g_wave[w].remaining != 0)) {
      if (g_wave[w].interval == g_wave[w].timer) {
        uVar1 = spawn_path_enemy(*g_wave[w].path,
                                 g_wave[w].path[1] + (unsigned int)g_wave[w].yofs,
                                 g_wave[w].tmpl, 0);
        g_wave[w].idx[g_wave[w].remaining - 1] =
             uVar1;
        if (g_wave[w].idx[g_wave[w].remaining - 1] != '2'
           ) {
          g_enemy[g_wave[w].idx[g_wave[w].remaining - 1]].path_pos = 0;
          g_enemy[g_wave[w].idx[g_wave[w].remaining - 1]].wave = w;
          g_wave[w].remaining--;
        }
        g_wave[w].timer = 0;
      }
      else {
        g_wave[w].timer++;
      }
    }
  }
  return;
}


/* FUN_000203b4 @ 0x203b4 */

void spawn_particle(int x,int y,fixed speed,unsigned int angle,int color)

{
  fixed fVar1;
  int pt;
  
  for (pt = 0; (g_particle[pt].active != 0 && (pt < 99)); pt++) {
  }
  g_particle[pt].active = 1;
  g_particle[pt].color = color;
  g_particle[pt].x = x;
  g_particle[pt].y = y;
  fVar1 = itofix(angle & 0xff);
  fVar1 = fcos(fVar1);
  fVar1 = fmul(speed,fVar1);
  g_particle[pt].vx = fVar1;
  fVar1 = itofix(angle & 0xff);
  fVar1 = fsin(fVar1);
  fVar1 = fmul(speed,fVar1);
  g_particle[pt].vy = fVar1;
  g_particle[pt].age = 0;
  return;
}
