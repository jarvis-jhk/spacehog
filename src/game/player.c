/* Player ships: spawning, firing, power-ups, damage, special weapons. */
#include "sh.h"
#include "play.h"

/* FUN_0000c354 @ 0xc354 */

void lose_weapons(int p)

{
  int iVar1;
  int local_c;
  int local_8;
  int local_4;
  
  if (g_player[p].lock == 0) {
    local_c = 8;
    local_4 = 2;
    if (g_player[p].ship.lvl[0] < g_player[p].speed) {
      g_player[p].speed--;
    }
    while ((0 < local_c && (0 < local_4))) {
      iVar1 = rand();
      iVar1 = iVar1 % 6;
      if (g_player[p].ship.lvl[iVar1 + 1] < g_player[p].wlvl[iVar1 + 1]) {
        g_player[p].wlvl[iVar1 + 1]--;
        local_4--;
      }
      else {
        local_c--;
      }
    }
    local_c = 3;
    local_8 = 6;
    while ((0 < local_4 && (0 < local_c))) {
      if (g_player[p].ship.lvl[local_8] < g_player[p].wlvl[local_8]) {
        g_player[p].wlvl[local_8]--;
        local_4--;
      }
      local_8--;
      if (local_8 < 0) {
        local_8 = 6;
        local_c--;
      }
    }
  }
  g_player[p].option = 0;
  g_player[p].powerup = -1;
  g_player[p].lock = 0;
  draw_status_bar(p);
  return;
}


/* FUN_0000c628 @ 0xc628 */

void respawn_player(int p)

{
  g_player[p].invuln = 0x50;
  g_player[p].x = g_player[p].start_x;
  g_player[p].y = g_player[p].start_y;
  g_player[p].bar_dirty = 1;
  g_player[p].powerup = 0;
  g_player[p].vx = 0;
  g_player[p].vy = 0;
  g_player[p].charge = 0;
  g_player[p].charge_tick = 0;
  g_player[p].blast = 0;
  g_player[p].select_held = 0;
  g_player[p].special_time = 0;
  g_player[p].flame = 1;
  g_player[p].lock = 0;
  if (g_player[p].control == '\x02') {
    mouse_used = 1;
  }
  if (g_player[p].control == CTRL_PAD) {
    pad_used = 1;
  }
  draw_status_bar(p);
  return;
}


/* FUN_0000c8e8 @ 0xc8e8 */

void init_player(int p)

{
  int *piVar3;
  
  g_player[p].specials = g_ship_specials[g_player[p].shipno];
  g_player[p].ship = g_shiptype[g_player[p].shipno];
  g_player[p].sprite = g_player[p].ship.sprite;
  piVar3 = dat_spacehog[g_player[p].sprite].dat;
  g_player[p].hw = *piVar3 / 2;
  g_player[p].hh = piVar3[1] / 2;
  g_player[p].w = *piVar3;
  g_player[p].h = piVar3[1];
  g_player[p].start_y = p * 0x50 + 0x50;
  g_player[p].start_x = 0x28;
  g_player[p].score = 0;
  g_player[p].fire_delay = '\n';
  g_player[p].fire_wait = 0;
  g_player[p].lives = '\x03';
  g_player[p].warp_in = 100;
  g_player[p].dead = 0;
  g_player[p].missiles = 0;
  g_player[p].speed = g_player[p].ship.lvl[0];
  g_player[p].gun = g_player[p].ship.lvl[1];
  g_player[p].side = g_player[p].ship.lvl[2];
  g_player[p].buildup = g_player[p].ship.lvl[3];
  g_player[p].missile = g_player[p].ship.lvl[4];
  g_player[p].plasma = g_player[p].ship.lvl[5];
  g_player[p].magma = g_player[p].ship.lvl[6];
  g_player[p].option = g_player[p].ship.lvl[7];
  g_player[p].addon = g_player[p].ship.lvl[8];
  return;
}


/* FUN_00013528 @ 0x13528 */

int find_free_shot(void)

{
  int local_4;
  
  for (local_4 = 0; (g_shot[local_4].active != 0 && (local_4 < 200)); local_4++) {
  }
  if (199 < local_4) {
    local_4 = -1;
  }
  return local_4;
}


/* FUN_00013590 @ 0x13590 */

void player_fire(int p)

{
  int *piVar1;
  int iVar2;
  int s;
  unsigned int local_8;
  unsigned int local_4;
  
  if (g_player[p].charge == 0) {
    g_player[p].muzzle = 1;
    if (snd_on != 0) {
      if (g_player[p].magma == 0) {
        play_sample(dat_shsnd[SN_SND_FIRE2].dat,200,(g_player[p].x * 0xff) / 0x140,1000,0);
      }
      else {
        play_sample(dat_shsnd[SN_SND_FIRE1].dat,200,(g_player[p].x * 0xff) / 0x140,1000,0);
      }
    }
    local_8 = (unsigned int)(g_player[p].magma != 0);
    for (local_4 = local_8; (int)local_4 <= (int)(unsigned int)g_player[p].gun; local_4 = local_4 + 1
        ) {
      iVar2 = find_free_shot();
      if (-1 < iVar2) {
        g_shot[iVar2].kind = SHOT_GUN;
        g_shot[iVar2].owner = (unsigned char)p;
        g_shot[iVar2].x = g_player[p].x + g_player[p].ship.gun_x;
        g_shot[iVar2].y = g_player[p].y + g_player[p].ship.gun_y + 1;
        g_shot[iVar2].vmax = gun_vmax[local_4];
        g_shot[iVar2].vx = 2;
        g_shot[iVar2].vy = gun_vy[local_4];
        g_shot[iVar2].damage = g_player[p].plasma + 1;
        g_shot[iVar2].sprite = g_player[p].plasma + 0x8c;
        piVar1 = dat_spacehog[g_shot[iVar2].sprite].dat;
        g_shot[iVar2].active = 1;
        g_shot[iVar2].bounced = 0;
        g_shot[iVar2].hw = *piVar1 >> 1;
        g_shot[iVar2].hh = piVar1[1] >> 1;
        g_shot[iVar2].w = *piVar1;
        g_shot[iVar2].h = piVar1[1];
      }
    }
    for (local_8 = 0; (int)local_8 < (int)(unsigned int)g_player[p].option; local_8++) {
      iVar2 = find_free_shot();
      if (-1 < iVar2) {
        g_shot[iVar2].kind = SHOT_GUN;
        g_shot[iVar2].owner = (unsigned char)p;
        g_shot[iVar2].x =
             (g_player[p].x + g_player[p].orbit[local_8].x +
             (g_cos128[g_player[p].orbit[local_8].angle] >> 3) + 5) -
             g_player[p].orbit[local_8].x;
        g_shot[iVar2].y =
             (g_player[p].y + g_player[p].orbit[local_8].y +
             (g_sin128[g_player[p].orbit[local_8].angle] >> 3)) -
             g_player[p].orbit[local_8].y;
        g_shot[iVar2].vmax = gun_vmax[0];
        g_shot[iVar2].vx = 2;
        g_shot[iVar2].vy = gun_vy[0];
        g_shot[iVar2].damage = g_player[p].plasma + 1;
        g_shot[iVar2].sprite = g_player[p].plasma + 0x8c;
        piVar1 = dat_spacehog[g_shot[iVar2].sprite].dat;
        g_shot[iVar2].active = 1;
        g_shot[iVar2].bounced = 0;
        g_shot[iVar2].hw = *piVar1 >> 1;
        g_shot[iVar2].hh = piVar1[1] >> 1;
        g_shot[iVar2].w = *piVar1;
        g_shot[iVar2].h = piVar1[1];
      }
    }
    s = -1;
    if (g_player[p].missiles < g_player[p].missile) {
      s = find_free_shot();
    }
    if (-1 < s) {
      g_shot[s].kind = SHOT_MISSILE;
      g_shot[s].owner = (unsigned char)p;
      g_shot[s].x = g_player[p].x;
      g_shot[s].y = g_player[p].y + 4;
      g_shot[s].vx = 4;
      g_shot[s].vy = 0;
      g_shot[s].damage = '\x02';
      g_shot[s].sprite = 0x5a;
      g_shot[s].active = 1;
      g_shot[s].bounced = 0;
      g_shot[s].hw = 4;
      g_shot[s].hh = 4;
      g_shot[s].w = 8;
      g_shot[s].h = 8;
      g_shot[s].target = -1;
      g_shot[s].dir = 0;
      g_player[p].missiles++;
    }
    s = -1;
    if (g_player[p].side != 0) {
      s = find_free_shot();
    }
    if (-1 < s) {
      g_shot[s].kind = SHOT_SIDE;
      g_shot[s].owner = (unsigned char)p;
      g_shot[s].x = g_player[p].x;
      g_shot[s].y = g_player[p].y + 10;
      g_shot[s].vx = 0;
      g_shot[s].vy = 6;
      g_shot[s].damage = g_player[p].side;
      g_shot[s].sprite = g_player[p].side + 0x92;
      g_shot[s].active = 1;
      g_shot[s].bounced = 0;
      piVar1 = dat_spacehog[g_shot[s].sprite].dat;
      g_shot[s].hw = *piVar1 >> 1;
      g_shot[s].hh = piVar1[1] >> 1;
      g_shot[s].w = *piVar1;
      g_shot[s].h = piVar1[1];
    }
    s = -1;
    if (g_player[p].side != 0) {
      s = find_free_shot();
    }
    if (-1 < s) {
      g_shot[s].kind = SHOT_SIDE;
      g_shot[s].owner = (unsigned char)p;
      g_shot[s].x = g_player[p].x;
      g_shot[s].y = g_player[p].y + -10;
      g_shot[s].vx = 0;
      g_shot[s].vy = -6;
      g_shot[s].damage = g_player[p].side;
      g_shot[s].sprite = g_player[p].side + 0x97;
      g_shot[s].active = 1;
      g_shot[s].bounced = 0;
      piVar1 = dat_spacehog[g_shot[s].sprite].dat;
      g_shot[s].hw = *piVar1 >> 1;
      g_shot[s].hh = piVar1[1] >> 1;
      g_shot[s].w = *piVar1;
      g_shot[s].h = piVar1[1];
    }
    s = -1;
    if (g_player[p].magma != 0) {
      s = find_free_shot();
    }
    if (-1 < s) {
      g_shot[s].kind = SHOT_MAGMA;
      g_shot[s].owner = (unsigned char)p;
      g_shot[s].x = g_player[p].ship.gun_x + 6 + g_player[p].x;
      g_shot[s].y = g_player[p].y + g_player[p].ship.gun_y + 1;
      g_shot[s].vx = 8;
      g_shot[s].vy = 0;
      g_shot[s].damage = g_player[p].magma * '\x03';
      g_shot[s].sprite = g_player[p].magma + 0x4b;
      g_shot[s].active = 1;
      g_shot[s].bounced = 0;
      piVar1 = dat_spacehog[g_shot[s].sprite].dat;
      g_shot[s].hw = *piVar1 >> 1;
      g_shot[s].hh = piVar1[1] >> 1;
      g_shot[s].w = *piVar1;
      g_shot[s].h = piVar1[1];
    }
  }
  else {
    g_player[p].blast = g_player[p].charge;
    if (snd_on != 0) {
      play_sample(dat_shsnd[SN_SND_BUILD].dat,200,(g_player[p].x * 0xff) / 0x140,1000,0);
    }
  }
  g_player[p].charge_tick = 0;
  g_player[p].charge = 0;
  g_player[p].fire_wait = g_player[p].fire_delay;
  return;
}


/* FUN_00015d3c @ 0x15d3c */

void buy_powerup(int p)

{
  unsigned int uVar1;
  unsigned char bVar2;
  char local_14;
  int local_4;
  
  g_player[p].select_held = 1;
  if (g_player[p].wlvl[g_player[p].powerup] <
      g_player[p].ship.maxlvl[g_player[p].powerup]) {
    if (g_player[p].powerup == 8) {
      for (local_4 = 0; local_4 < 9; local_4++) {
        uVar1 = g_player[p].ship.maxlvl[local_4] + 1;
        bVar2 = g_player[p].ship.addon_max[local_4];
        if (uVar1 < bVar2) {
          bVar2 = (unsigned char)uVar1;
        }
        g_player[p].ship.maxlvl[local_4] = bVar2;
      }
    }
    if (g_player[p].powerup == 7) {
      if (g_player[p].option == 0) {
        g_player[p].orbit[g_player[p].wlvl[g_player[p].powerup]].angle = 0;
      }
      else {
        for (local_4 = 1; local_4 < 4; local_4++) {
          local_14 = (char)(0x100 / (g_player[p].option + 1));
          g_player[p].orbit[local_4].angle =
               local_14 + g_player[p].orbit[local_4 - 1].angle;
        }
      }
    }
    g_player[p].wlvl[g_player[p].powerup] =
         g_player[p].wlvl[g_player[p].powerup] + 1;
    g_player[p].powerup = -1;
    draw_status_bar(p);
    if (snd_on != 0) {
      play_sample(dat_shsnd[SN_SND_SELECT].dat,200,0x80,1000,0);
    }
  }
  else if (snd_on != 0) {
    play_sample(dat_shsnd[SN_SND_NA].dat,200,0x80,1000,0);
  }
  return;
}




/* FUN_0001661c @ 0x1661c */

void player_hit(int p)

{
  if (g_player[p].option == 0) {
    spawn_explosion(g_player[p].x,g_player[p].y,0,0,1);
    g_player[p].warp_in = 100;
    if (g_player[p].lives != 0) {
      g_player[p].lives--;
    }
    if (g_player[p].lives == 0) {
      g_player[p].dead = 1;
    }
    lose_weapons(p);
    if (g_player[p].control == '\x02') {
      mouse_used = 0;
    }
    if (g_player[p].control == CTRL_PAD) {
      pad_used = 0;
    }
  }
  else {
    spawn_explosion((g_player[p].x -
                 g_player[p].orbit[g_player[p].option - 1].x) +
                 (g_cos128[g_player[p].orbit[(unsigned int)g_player[p].option - 1].angle] >> 3),
                 (g_player[p].y -
                 g_player[p].orbit[g_player[p].option - 1].y) +
                 (g_sin128[g_player[p].orbit[(unsigned int)g_player[p].option - 1].angle] >> 3),
                 0,0,0);
    g_player[p].option--;
  }
  return;
}


/* FUN_0001bbb0 @ 0x1bbb0 */

void use_special(int p)

{
  int iVar1;
  int iVar2;
  int iVar3;
  int uVar4;
  int uVar5;
  int local_4;
  
  g_player[p].specials--;
  if (g_player[p].shipno == 0) {
    g_player[p].special_time = 0x78;
    g_player[p].saved_plasma = (unsigned int)g_player[p].plasma;
    g_player[p].plasma += '\x03';
  }
  if ((g_player[p].shipno == 1) && (iVar1 = find_free_shot(), -1 < iVar1)) {
    g_player[p].special_time = 10;
    g_shot[iVar1].kind = SHOT_MISSILE;
    g_shot[iVar1].owner = (unsigned char)p;
    g_shot[iVar1].x = g_player[p].x;
    g_shot[iVar1].y = g_player[p].y + 3;
    g_shot[iVar1].vx = 4;
    g_shot[iVar1].vy = 0;
    g_shot[iVar1].damage = '(';
    g_shot[iVar1].sprite = 10;
    g_shot[iVar1].active = 1;
    g_shot[iVar1].bounced = 0;
    g_shot[iVar1].hw = 0xb;
    g_shot[iVar1].hh = 0xb;
    g_shot[iVar1].w = 0x17;
    g_shot[iVar1].h = 0x17;
    g_shot[iVar1].target = -1;
    g_shot[iVar1].dir = 0;
    g_player[p].missiles++;
  }
  if (g_player[p].shipno == 2) {
    g_player[p].special_time = 0x32;
    for (local_4 = 0; local_4 < 0x32; local_4++) {
      if ((g_enemy[local_4].active != 0) && ((g_enemy[local_4].immune & 0x10) == 0)) {
        g_enemy[local_4].hit_flash = 1;
        g_enemy[local_4].hp -= 0x14;
        iVar1 = g_enemy[local_4].hp;
        if (iVar1 < 0) {
          iVar1 = 0;
        }
        g_enemy[local_4].hp = iVar1;
        if (g_enemy[local_4].hp == 0) {
          kill_enemy(local_4,p);
        }
      }
    }
    for (local_4 = 0; local_4 < 0xf; local_4++) {
      iVar2 = rand();
      iVar2 = iVar2 % 3;
      uVar5 = 0;
      uVar4 = 0;
      iVar1 = rand();
      iVar1 = iVar1 % 200 + 0x14;
      iVar3 = rand();
      spawn_explosion(iVar3 % 0x118 + 0x14,iVar1,uVar4,uVar5,iVar2);
    }
    for (local_4 = 0; local_4 < 100; local_4++) {
      g_ebullet[local_4].active = 0;
    }
  }
  draw_status_bar(p);
  return;
}




/* FUN_0001d764 @ 0x1d764 */

void remove_missiles(void)

{
  int s;
  
  for (s = 0; s < 200; s++) {
    if ((g_shot[s].active != 0) && ((g_shot[s].kind & SHOT_MISSILE) != 0)) {
      g_player[g_shot[s].owner].missiles = g_player[g_shot[s].owner].missiles + 0xff;
      g_shot[s].active = 0;
    }
  }
  return;
}
