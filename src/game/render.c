/* Drawing of the play field, objects and the status bars; star field. */
#include "sh.h"
#include "play.h"

/* FUN_000063f8 @ 0x63f8 */

void init_stars(void)

{
  int *piVar1;
  int iVar2;
  int local_8;
  int local_4;
  
  for (local_4 = 0; local_4 < 0xff; local_4++) {
    iVar2 = rand();
    g_star[local_4].x = iVar2 % 0x140;
    iVar2 = rand();
    g_star[local_4].y = iVar2 % 0xf0;
    iVar2 = rand();
    g_star[local_4].speed = (char)(iVar2 % 3) + 1;
    g_star[local_4].color = g_star[local_4].speed * '\a' + '\x02';
    g_star[local_4].alive = 1;
  }
  for (local_4 = 0; local_4 < 0x100; local_4++) {
    iVar2 = rand();
    bg_tiles[local_4] = (unsigned char)(iVar2 % 3);
  }
  for (local_4 = 0; local_4 < 200; local_4++) {
    g_shot[local_4].active = 0;
  }
  for (local_4 = 0; local_4 < 10; local_4++) {
    g_bgobj[local_4].active = 0;
  }
  for (local_4 = 0; local_4 < 100; local_4++) {
    g_ebullet[local_4].active = 0;
  }
  for (local_4 = 0; local_4 < 10; local_4++) {
    g_wave[local_4].active = 0;
  }
  for (local_4 = 0; local_4 < 0x32; local_4++) {
    g_enemy[local_4].targeted = 0;
    g_enemy[local_4].active = 0;
  }
  for (local_4 = 0; local_4 < 0x32; local_4++) {
    g_expl[local_4].active = 0;
  }
  for (local_4 = 0; local_4 < 10; local_4++) {
    piVar1 = dat_spacehog[SH_PUP000].dat;
    g_pickup[local_4].active = 0;
    g_pickup[local_4].hw = *piVar1 / 2;
    g_pickup[local_4].hh = piVar1[1] / 2;
    g_pickup[local_4].w = *piVar1;
    g_pickup[local_4].h = piVar1[1];
  }
  piVar1 = dat_spacehog[SH_OPTION].dat;
  for (local_4 = 0; local_4 < 2; local_4++) {
    for (local_8 = 0; local_8 < 4; local_8++) {
      g_player[local_4].orbit[local_8].x = *piVar1 / 2;
      g_player[local_4].orbit[local_8].y = piVar1[1] / 2;
    }
    clear_to_color(bmp_bar[local_4],0xd0);
    draw_rle_sprite(bmp_bar[local_4],dat_spacehog[SH_TOOLBAR].dat,0,0);
    if (local_4 != 0) {
      draw_rle_sprite(bmp_bar[local_4],dat_spacehog[SH_TB2].dat,8,0);
    }
  }
  return;
}


/* FUN_00008784 @ 0x8784 */

void update_stars(void)

{
  int iVar1;
  int st;
  
  for (st = 0; st < (int)(unsigned int)opts[3]; st++) {
    if (g_star[st].alive == 0) {
      iVar1 = rand();
      if (iVar1 % 0x32 == 1) {
        g_star[st].x = 0x13f;
        iVar1 = rand();
        g_star[st].y = iVar1 % 0xf0;
        iVar1 = rand();
        g_star[st].speed = (char)(iVar1 % 3) + 1;
        g_star[st].color = g_star[st].speed * '\a' + '\x02';
        g_star[st].alive = 1;
      }
    }
    else {
      g_star[st].x = g_star[st].x - (unsigned int)g_star[st].speed;
      if (g_star[st].x < 0) {
        g_star[st].alive = 0;
      }
    }
  }
  return;
}


/* FUN_000085f0 @ 0x85f0 */

void draw_stars(void)

{
  int st;
  
  if (opts[0] == 0) {
    for (st = 0; st < (int)(unsigned int)opts[3]; st++) {
      if (g_star[st].alive != 0) {
        _putpixel(bmp_buf,g_star[st].x,g_star[st].y,g_star[st].color);
      }
    }
  }
  else {
    color_map = &light_white;
    drawing_mode(5,(BITMAP *)0x0,0,0);
    for (st = 0; st < (int)(unsigned int)opts[3]; st++) {
      if (g_star[st].alive != 0) {
        putpixel(bmp_buf,g_star[st].x,g_star[st].y,(unsigned int)g_star[st].color * -8 + 0xff)
        ;
      }
    }
    solid_mode();
  }
  return;
}


/* FUN_00008a0c @ 0x8a0c */

void draw_shots(void)

{
  int local_4;
  
  for (local_4 = 0; local_4 < 200; local_4++) {
    if (g_shot[local_4].active != 0) {
      if (g_shot[local_4].bounced == 0) {
        if ((g_shot[local_4].kind & SHOT_MISSILE) == 0) {
          draw_sprite(bmp_buf,dat_spacehog[g_shot[local_4].sprite].dat,
                      g_shot[local_4].x - g_shot[local_4].hw,
                      g_shot[local_4].y - g_shot[local_4].hh);
        }
        else if (g_shot[local_4].sprite == 0x5a) {
          draw_rle_sprite(bmp_buf,missile_rot[g_shot[local_4].dir],
                          g_shot[local_4].x - g_shot[local_4].hw,
                          g_shot[local_4].y - g_shot[local_4].hh);
        }
        else {
          draw_sprite(bmp_buf,bigmissile_rot[(unsigned int)g_shot[local_4].dir * 3 + (unsigned int)g_anim[2]],
                      g_shot[local_4].x - g_shot[local_4].hw,
                      g_shot[local_4].y - g_shot[local_4].hh);
        }
      }
      else {
        if ((g_shot[local_4].kind & SHOT_GUN) == 0) {
          if ((g_shot[local_4].kind & SHOT_SIDE) == 0) {
            if ((g_shot[local_4].kind & SHOT_MAGMA) != 0) {
              draw_rle_sprite(bmp_buf,dat_spacehog[bg_tiles[bg_tile] + 0x57].dat,
                              g_shot[local_4].x - ((unsigned int)bg_tiles[bg_tile] * 2 + 9),
                              g_shot[local_4].y - ((unsigned int)bg_tiles[bg_tile] * 2 + 9));
            }
          }
          else {
            draw_rle_sprite(bmp_buf,dat_spacehog[bg_tiles[bg_tile] + 0x3d].dat,
                            g_shot[local_4].x - (bg_tiles[bg_tile] + 6),
                            g_shot[local_4].y - (bg_tiles[bg_tile] + 6));
          }
        }
        else {
          draw_rle_sprite(bmp_buf,dat_spacehog[bg_tiles[bg_tile] + 0x69].dat,
                          g_shot[local_4].x - (bg_tiles[bg_tile] + 6),
                          g_shot[local_4].y - (bg_tiles[bg_tile] + 6));
        }
        g_shot[local_4].active = 0;
        bg_tile++;
      }
    }
  }
  for (local_4 = 0; local_4 <= (int)(unsigned int)two_players; local_4++) {
    if (g_player[local_4].blast != 0) {
      if (opts[0] != 0) {
        color_map = &light_green;
        drawing_mode(5,(BITMAP *)0x0,0,0);
        circlefill(bmp_buf,g_player[local_4].x,g_player[local_4].y,
                   g_player[local_4].blast + (g_player[local_4].blast >> 2),g_player[local_4].blast * 8
                  );
        solid_mode();
      }
      stretch_sprite(bmp_buf,dat_spacehog[SH_BUILDUP].dat,g_player[local_4].x,
                     g_player[local_4].y - g_player[local_4].blast,0x140,
                     g_player[local_4].blast * 2);
      g_player[local_4].blast--;
    }
  }
  return;
}


/* FUN_00009194 @ 0x9194 */

void draw_ebullets(void)

{
  int b;
  
  for (b = 0; b < 100; b++) {
    if (g_ebullet[b].active != 0) {
      draw_rle_sprite(bmp_buf,dat_spacehog
                              [g_ebullet[b].sprite + (unsigned int)g_anim[g_ebullet[b].anim]
                              ].dat,g_ebullet[b].x - g_ebullet[b].hw,
                      g_ebullet[b].y - g_ebullet[b].hh);
    }
  }
  return;
}


/* FUN_000092b4 @ 0x92b4 */

void draw_bgobjs(void)

{
  int o;
  
  for (o = 0; o < 10; o++) {
    if ((g_bgobj[o].active != 0) && (g_bgobj[o].layer == 0)) {
      draw_rle_sprite(bmp_buf,dat_shbgs[g_bgobj[o].sprite].dat,
                      g_bgobj[o].x - g_bgobj[o].hw,
                      g_bgobj[o].y - g_bgobj[o].hh);
    }
  }
  for (o = 0; o < 10; o++) {
    if ((g_bgobj[o].active != 0) && (g_bgobj[o].layer != 0)) {
      draw_rle_sprite(bmp_buf,dat_shbgs[g_bgobj[o].sprite].dat,
                      g_bgobj[o].x - g_bgobj[o].hw,
                      g_bgobj[o].y - g_bgobj[o].hh);
    }
  }
  return;
}


/* FUN_00009478 @ 0x9478 */

void draw_enemies(void)

{
  int iVar1;
  int e;
  
  for (e = 0; e < 0x32; e++) {
    if (g_enemy[e].active != 0) {
      if (g_enemy[e].hit_flash == 0) {
        if (g_enemy[e].masked == 0) {
          draw_rle_sprite(bmp_buf,dat_spacehog
                                  [g_enemy[e].sprite + (unsigned int)g_anim[g_enemy[e].anim]].dat,
                          g_enemy[e].x - g_enemy[e].hw,
                          g_enemy[e].y - g_enemy[e].hh);
        }
        else if (g_enemy[e].tmpl == '\x1a') {
          iVar1 = rand();
          stretch_sprite(bmp_buf,dat_spacehog[g_enemy[e].sprite].dat,
                         (g_enemy[e].x - g_enemy[e].hw) - iVar1 % 0x14,
                         g_enemy[e].y - g_enemy[e].hh,iVar1 % 0x14 + 0x25,0x1a);
        }
        else if (g_enemy[e].tmpl == '\x1c') {
          iVar1 = rand();
          stretch_sprite(bmp_buf,dat_spacehog[g_enemy[e].sprite].dat,
                         g_enemy[e].x - g_enemy[e].hw,
                         g_enemy[e].y - g_enemy[e].hh,0x26 - iVar1 % 0x14,0x1a);
        }
        else if (g_enemy[e].tmpl == '\x1d') {
          iVar1 = rand();
          stretch_sprite(bmp_buf,dat_spacehog[g_enemy[e].sprite].dat,
                         g_enemy[e].x - g_enemy[e].hw,
                         g_enemy[e].y - g_enemy[e].hh,0x1a,0x26 - iVar1 % 0x14);
        }
        else {
          draw_sprite(bmp_buf,dat_spacehog
                              [g_enemy[e].sprite + (unsigned int)g_anim[g_enemy[e].anim]].dat,
                      g_enemy[e].x - g_enemy[e].hw,
                      g_enemy[e].y - g_enemy[e].hh);
        }
      }
      else {
        color_map = &light_white;
        if (g_enemy[e].masked == 0) {
          draw_lit_rle_sprite(bmp_buf,dat_spacehog[g_enemy[e].sprite].dat,
                              g_enemy[e].x - g_enemy[e].hw,
                              g_enemy[e].y - g_enemy[e].hh,0x3c);
        }
        else {
          draw_lit_sprite(bmp_buf,dat_spacehog[g_enemy[e].sprite].dat,
                          g_enemy[e].x - g_enemy[e].hw,
                          g_enemy[e].y - g_enemy[e].hh,0x3c);
        }
        g_enemy[e].hit_flash = 0;
      }
      if (dbg_enemyinfo != 0) {
        textprintf(bmp_buf,dat_spacehog[SH_FONT_SMALL].dat,g_enemy[e].x,g_enemy[e].y,0x1f,
                   "%d",g_enemy[e].hp);
        textprintf(bmp_buf,dat_spacehog[SH_FONT_SMALL].dat,g_enemy[e].x,g_enemy[e].y + 6,
                   0x1f,"%d",(unsigned int)g_enemy[e].drop);
        if (g_enemy[e].parent != 0xff) {
          line(bmp_buf,g_enemy[e].x,g_enemy[e].y,
               g_enemy[(signed char)g_enemy[e].parent].x,g_enemy[(signed char)g_enemy[e].parent].y,
               0xff);
        }
      }
    }
  }
  return;
}


/* FUN_00009e14 @ 0x9e14 */

void draw_explosions(void)

{
  int ex;
  
  for (ex = 0; ex < 0x32; ex++) {
    if (g_expl[ex].active != 0) {
      if (g_expl[ex].kind == 0) {
        draw_rle_sprite(bmp_buf,expl_small[g_expl[ex].frame],
                        g_expl[ex].x - g_expl[ex].hw,
                        g_expl[ex].y - g_expl[ex].hh);
      }
      else if (g_expl[ex].kind == 1) {
        draw_rle_sprite(bmp_buf,expl_medium[g_expl[ex].frame],
                        g_expl[ex].x - g_expl[ex].hw,
                        g_expl[ex].y - g_expl[ex].hh);
      }
      else {
        draw_rle_sprite(bmp_buf,expl_big[g_expl[ex].frame],
                        g_expl[ex].x - g_expl[ex].hw,
                        g_expl[ex].y - g_expl[ex].hh);
      }
      if (opts[0] != 0) {
        color_map = &light_white;
        drawing_mode(5,(BITMAP *)0x0,0,0);
        circlefill(bmp_buf,g_expl[ex].x,g_expl[ex].y,
                   (unsigned int)g_expl[ex].frame * 2,
                   ((unsigned int)g_expl[ex].frame << 8) / ((unsigned int)g_expl[ex].frames * 2) + 0x7f);
        solid_mode();
      }
    }
  }
  return;
}


/* FUN_0000a0d0 @ 0xa0d0 */

void draw_pickups(void)

{
  int pk;
  
  for (pk = 0; pk < 10; pk++) {
    if (g_pickup[pk].active != 0) {
      draw_rle_sprite(bmp_buf,dat_spacehog[g_pickup[pk].sprite + g_pickup[pk].frame].dat,
                      g_pickup[pk].x - g_pickup[pk].hw,
                      g_pickup[pk].y - g_pickup[pk].hh);
    }
  }
  return;
}


/* FUN_0000a204 @ 0xa204 */

void draw_status_bar(int p)

{
  unsigned int uVar1;
  int iVar2;
  int local_4;
  
  if (8 < g_player[p].powerup) {
    g_player[p].powerup = -1;
    if (g_player[p].lock == 0) {
      g_player[p].lock = 1;
    }
    else {
      g_player[p].specials += g_ship_specials[g_player[p].shipno];
    }
  }
  g_player[p].charge_max =
       ((unsigned int)g_player[p].plasma + (unsigned int)g_player[p].gun + g_player[p].side + 2) *
       2;
  for (local_4 = 0; local_4 < 9; local_4++) {
    if (g_player[p].wlvl[local_4] < g_player[p].ship.maxlvl[local_4]) {
      draw_rle_sprite(bmp_bar[p],dat_spacehog[SH_LAMP1].dat,g_lamp_x[local_4],7);
    }
    if ((g_player[p].wlvl[local_4] < g_player[p].ship.maxlvl[local_4]) &&
       (local_4 == g_player[p].powerup)) {
      draw_rle_sprite(bmp_bar[p],dat_spacehog[SH_LAMP2].dat,g_lamp_x[local_4],7);
    }
    if (g_player[p].ship.maxlvl[local_4] == g_player[p].wlvl[local_4]) {
      draw_rle_sprite(bmp_bar[p],dat_spacehog[SH_LAMP0].dat,g_lamp_x[local_4],7);
    }
    if ((g_player[p].ship.maxlvl[local_4] == g_player[p].wlvl[local_4]) &&
       (local_4 == g_player[p].powerup)) {
      draw_rle_sprite(bmp_bar[p],dat_spacehog[SH_LAMP3].dat,g_lamp_x[local_4],7);
    }
    if (dbg_enemyinfo != 0) {
      rectfill(bmp_bar[p],g_lamp_x[local_4],0,g_lamp_x[local_4] + 4,7,1);
      textprintf(bmp_bar[p],dat_spacehog[SH_FONT_SMALL].dat,g_lamp_x[local_4] + 1,1,0x1f,"%d",
                 (unsigned int)g_player[p].wlvl[local_4]);
    }
  }
  rectfill(bmp_bar[p],0x24,3,0x26,7,1);
  rectfill(bmp_bar[p],0x33,3,0x39,7,1);
  if (g_player[p].lives < 9) {
    uVar1 = (unsigned int)g_player[p].lives;
  }
  else {
    uVar1 = 9;
  }
  textprintf(bmp_bar[p],dat_spacehog[SH_FONT_SMALL].dat,0x24,3,0x6c,"%d",uVar1);
  iVar2 = g_player[p].specials;
  if (99 < iVar2) {
    iVar2 = 99;
  }
  textprintf(bmp_bar[p],dat_spacehog[SH_FONT_SMALL].dat,0x33,3,0x6c,"%2d",iVar2);
  if (g_player[p].lock == 0) {
    draw_rle_sprite(bmp_bar[p],dat_spacehog[SH_LOCKOFF].dat,0x135,0);
  }
  else {
    draw_rle_sprite(bmp_bar[p],dat_spacehog[SH_LOCKON].dat,0x135,0);
  }
  g_player[p].bar_dirty = 1;
  return;
}


/* FUN_0000a994 @ 0xa994 */

void render_frame(void)

{
  int uVar1;
  char cVar2;
  int iVar3;
  int iVar4;
  int local_8;
  int local_4;
  
  iVar3 = g_frames;
  g_frames = iVar3 + 1;
  uVar1 = g_frames;
  if (opts[1] == 0) {
    clear_to_color(bmp_buf,0xd0);
  }
  else {
    blit(bmp_world,bmp_buf,bg_scroll,0,0,0,bg_scroll + 0x140,0xf0);
  }
  if ((engine_sound != 0) && (launch_step == 0)) {
    textout_centre(bmp_buf,dat_spacehog[SH_FONT_SMALL].dat,"Press TAB to skip",0xa0,view_y + 0xaa,0x92)
    ;
  }
  if ((g_title_y < 0xf0) &&
     (textout_centre(bmp_buf,dat_spacehog[SH_FONT].dat,g_levelhdr.name,0xa0,g_title_y + view_y,
                     0x93), custom_level != 0)) {
    textout_centre(bmp_buf,dat_spacehog[SH_FONT_SMALL].dat,"CUSTOM LEVEL",0xa0,g_title_y + view_y + -2,
                   0x91);
  }
  if (opts[2] != 0) {
    draw_bgobjs();
  }
  if (opts[3] != 0) {
    draw_stars();
  }
  draw_pickups();
  draw_explosions();
  draw_enemies();
  draw_shots();
  draw_ebullets();
  for (local_4 = 0; local_4 <= (int)(unsigned int)two_players; local_4++) {
    if (g_player[local_4].dead == 0) {
      if (((opts[0] != 0) && (g_player[local_4].warp_in != 0)) &&
         (g_player[local_4].warp_in < 0x32)) {
        color_map = &light_white;
        drawing_mode(5,(BITMAP *)0x0,0,0);
        circlefill(bmp_buf,g_player[local_4].start_x - (g_player[local_4].warp_in >> 2),
                   g_player[local_4].start_y,g_player[local_4].warp_in * 2,g_player[local_4].warp_in * 5);
        solid_mode();
      }
      if (g_player[local_4].warp_in == 0) {
        iVar3 = local_4 * 6 + 0x24;
        if (((g_player[local_4].control == CTRL_PAD) && (iVar4 = pjoy(local_4)->right, iVar4 != 0)) ||
           (pkey(local_4, PK_RIGHT) != 0)) {
          draw_rle_sprite(bmp_buf,dat_spacehog[iVar3 + (unsigned int)g_player[local_4].flame * 2].dat,
                          g_player[local_4].x + g_player[local_4].ship.flame_x,
                          g_player[local_4].y + g_player[local_4].ship.flame_y);
        }
        else {
          draw_rle_sprite(bmp_buf,dat_spacehog[(unsigned int)g_player[local_4].flame + iVar3].dat,
                          g_player[local_4].x + g_player[local_4].ship.flame_x,
                          g_player[local_4].y + g_player[local_4].ship.flame_y);
        }
        if (g_player[local_4].charge == 0) {
          if (g_player[local_4].invuln == 0) {
            if ((g_player[local_4].special_time == 0) || (g_player[local_4].shipno != 0)) {
              draw_sprite(bmp_buf,dat_spacehog[g_player[local_4].sprite].dat,
                          g_player[local_4].x - g_player[local_4].hw,
                          g_player[local_4].y - g_player[local_4].hh);
            }
            else {
              color_map = &light_green;
              draw_lit_sprite(bmp_buf,dat_spacehog[g_player[local_4].sprite].dat,
                              g_player[local_4].x - g_player[local_4].hw,
                              g_player[local_4].y - g_player[local_4].hh,(unsigned int)anim_phase);
            }
          }
          else {
            color_map = &light_white;
            draw_lit_sprite(bmp_buf,dat_spacehog[g_player[local_4].sprite].dat,
                            g_player[local_4].x - g_player[local_4].hw,
                            g_player[local_4].y - g_player[local_4].hh,(unsigned int)anim_phase);
          }
        }
        else {
          color_map = &light_green;
          drawing_mode(5,(BITMAP *)0x0,0,0);
          circlefill(bmp_buf,g_player[local_4].x,g_player[local_4].y,g_player[local_4].charge,
                     g_player[local_4].charge * -4 + 0xff);
          circlefill(bmp_buf,g_player[local_4].x,g_player[local_4].y,
                     g_player[local_4].charge - (g_player[local_4].charge >> 2),(unsigned int)anim_phase);
          draw_lit_sprite(bmp_buf,dat_spacehog[g_player[local_4].sprite].dat,
                          g_player[local_4].x - g_player[local_4].hw,
                          g_player[local_4].y - g_player[local_4].hh,
                          0xff - (unsigned int)(anim_phase >> 1));
          solid_mode();
        }
        if (g_player[local_4].muzzle != 0) {
          g_player[local_4].muzzle = 0;
          draw_rle_sprite(bmp_buf,dat_spacehog[SH_WFLAME].dat,
                          g_player[local_4].x + g_player[local_4].ship.gun_x,
                          g_player[local_4].y + g_player[local_4].ship.gun_y);
        }
        for (local_8 = 0; local_8 < (int)(unsigned int)g_player[local_4].option; local_8++) {
          draw_rle_sprite(bmp_buf,dat_spacehog[SH_OPTION].dat,
                          (g_player[local_4].x - g_player[local_4].orbit[local_8].x) +
                          (g_cos128[g_player[local_4].orbit[local_8].angle] >> 3),
                          (g_player[local_4].y - g_player[local_4].orbit[local_8].y) +
                          (g_sin128[g_player[local_4].orbit[local_8].angle] >> 3));
        }
      }
    }
  }
  if (engine_sound != 0) {
    iVar3 = rand();
    iVar4 = g_launch_path[launch_step * 5 + 4] + iVar3 % 0x14;
    iVar3 = rand();
    iVar3 = g_launch_path[launch_step * 5 + 3] + iVar3 % 0x14;
    draw_rle_sprite(bmp_buf,dat_spacehog[SH_MOTHERSHIP].dat,mothership_x,mothership_y);
    stretch_sprite(bmp_buf,dat_spacehog[SH_MFLAMEH].dat,(mothership_x - iVar3) + 4,mothership_y + 0x18,
                   iVar3,0x1a);
    stretch_sprite(bmp_buf,dat_spacehog[SH_MFLAMEH].dat,(mothership_x - iVar3) + 4,mothership_y + 0x3a,
                   iVar3,0x1a);
    stretch_sprite(bmp_buf,dat_spacehog[SH_MFLAMEV].dat,mothership_x + 0xeb,mothership_y + 0x57,0x1a,iVar4
                  );
    stretch_sprite(bmp_buf,dat_spacehog[SH_MFLAMEV].dat,mothership_x + 0x119,mothership_y + 0x57,0x1a,
                   iVar4);
  }
  if (bosses_alive != 0) {
    iVar3 = g_enemy[boss_enemy].hp / boss_pip_hp;
    if (boss_pips < iVar3) {
      boss_pips++;
      iVar3 = boss_pips;
    }
    boss_pips = iVar3;
    for (local_4 = 0; local_4 <= boss_pips; local_4++) {
      draw_rle_sprite(bmp_buf,dat_spacehog[SH_G_ENERGY].dat,0x131,view_y + 0xaa + local_4 * -2);
    }
  }
  if (mouse_used != 0) {
    iVar4 = mouse_y;
    iVar3 = mouse_x;
    draw_rle_sprite(bmp_buf,dat_spacehog[g_anim[3] + 0x5c].dat,iVar3 + -2,view_y + -2 + iVar4)
    ;
  }
  if (dbg_fps != 0) {
    uVar1 = g_fps;
    textprintf(bmp_buf,dat_spacehog[SH_FONT_SMALL].dat,0,view_y + 7,0x1f,"fps   :%ld",uVar1);
    textprintf(bmp_buf,dat_spacehog[SH_FONT_SMALL].dat,0,view_y + 0x1f,0x1f,"lCnt  :%ld",level_step);
    textprintf(bmp_buf,dat_spacehog[SH_FONT_SMALL].dat,0,view_y + 0x25,0x1f,"lCpos :%ld",step_ticks);
    textprintf(bmp_buf,dat_spacehog[SH_FONT_SMALL].dat,0,view_y + 0x2b,0x1f,"boss  :%ld",bosses_alive);
  }
  if (dbg_enemyinfo != 0) {
    for (local_4 = 0; local_4 < 10; local_4++) {
      if (g_wave[local_4].active != 0) {
        textprintf(bmp_buf,dat_spacehog[SH_FONT_SMALL].dat,0,local_4 * 6 + view_y + 0x3c,0x1f,
                   "W:%d M:%d L:%d",local_4,(unsigned int)g_wave[local_4].alive,
                   (unsigned int)g_wave[local_4].remaining);
      }
    }
  }
  draw_rle_sprite(bmp_buf,dat_spacehog[SH_SCORES2].dat,0x15,view_y);
  textprintf(bmp_buf,dat_spacehog[SH_FONT_SMALL].dat,0x1b,view_y + 1,0x1f,"%7ld",g_player[0].score);
  if (two_players != 0) {
    draw_rle_sprite(bmp_buf,dat_spacehog[SH_SCORES1].dat,0x15,view_y + 0xab);
    textprintf(bmp_buf,dat_spacehog[SH_FONT_SMALL].dat,0x1b,view_y + 0xac,0x1f,"%7ld",g_player[1].score)
    ;
  }
  if ((opt_vsync != 0) && (cVar2 = key[0xf], cVar2 == 0)) {
    vsync();
  }
  blit(bmp_buf,screen,0,view_y,0,0xb,0x140,0xb2);
  if (g_player[0].bar_dirty != 0) {
    blit(bmp_bar[0],screen,0,0,0,0,0x140,0xb);
  }
  if ((g_player[1].bar_dirty != 0) && (two_players == 1)) {
    blit(bmp_bar[1],screen,0,0,0,0xbd,0x140,0xb);
  }
  return;
}
