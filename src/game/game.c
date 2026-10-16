/* The game sequence: ship selection, launch, levels, dialogs, pause, tally, ending. */
#include "sh.h"
#include "play.h"

/* FUN_00010df0 @ 0x10df0: text box with a portrait. mode 0: wait for fire, 1: ESC/ENTER
 * question (returns 0/1), 2: pause note. typewriter: print slowly. */

int dialog(const char *text,RLE_SPRITE *face,int mode,int typewriter)

{
  /* the DOS code word-wraps the caller's string in place; work on a copy instead */
  static char textbuf[1024];
  char *buf = textbuf;
  char cVar1;
  int iVar2;
  unsigned int uVar3;
  char local_21a [514];
  int local_14;
  int local_10;
  unsigned int p;
  int local_8;
  unsigned int local_4;
  
  snprintf(textbuf, sizeof(textbuf), "%s", text);
  local_14 = 0;
  local_21a[0] = ' ';
  local_21a[1] = 0;
  color_map = &light_white;
  drawing_mode(5,(BITMAP *)0x0,0,0);
  rect(screen,10,0x6e,0xdd,0xa0,0x3c);
  rectfill(screen,0xc,0x70,0xdb,0x9e,0x3c);
  rectfill(screen,0xdf,0x46,0x13f,0xa5,0xb4);
  draw_rle_sprite(screen,face,0xdf,0x46);
  solid_mode();
  p = 0;
  local_4 = 0;
  local_8 = 0;
  local_10 = 0;
  do {
    local_21a[0] = buf[local_4];
    iVar2 = text_length(dat_spacehog[SH_FONT_ARIAL].dat,local_21a);
    local_10 += iVar2;
    local_21a[local_8 + 2] = local_21a[0];
    local_21a[local_8 + 3] = 0;
    if (local_21a[0] == ' ') {
      p = local_4;
    }
    if (local_21a[0] == '@') {
      buf[local_4] = 0;
    }
    if (local_21a[0] == -0x6c) {
      local_21a[2] = 0x20;
      local_21a[3] = 0;
      local_8 = -1;
      local_10 = 0;
    }
    if (199 < local_10) {
      buf[p] = -0x6c;
      local_4 = p;
      local_21a[2] = 0x20;
      local_21a[3] = 0;
      local_10 = 0;
      local_8 = -1;
    }
    local_4++;
    local_8++;
    uVar3 = strlen(buf);
  } while (local_4 < uVar3);
  local_4 = 0x10;
  local_8 = 0x72;
  p = 0;
  do {
    local_21a[0] = buf[p];
    if (local_21a[0] == -0x6c) {
      local_4 = 0x10;
      local_8 += 10;
    }
    else {
      textout(screen,dat_spacehog[SH_FONT_ARIAL].dat,local_21a,local_4,local_8,1);
      iVar2 = text_length(dat_spacehog[SH_FONT_ARIAL].dat,local_21a);
      local_4 += iVar2;
      if ((typewriter != 0) && (cVar1 = key[0xf], cVar1 == 0)) {
        rest(0xf);
      }
    }
    p++;
    uVar3 = strlen(buf);
  } while (p < uVar3);
  if (mode == 0) {
    while (local_14 == 0) {
      wait_tick();
      cVar1 = key[0xf];
      if ((cVar1 != 0) && (cVar1 = key[0x1c], cVar1 != 0)) {
        console();
      }
      if (joy_present != 0) {
        poll_joystick();
      }
      for (p = 0; (int)p <= (int)(unsigned int)two_players; p++) {
        if ((g_player[p].control == CTRL_KEYBOARD) && (pkey(p, PK_FIRE) != 0)) {
          local_14 = 1;
        }
        if ((g_player[p].control == CTRL_PAD) && (iVar2 = pjoy(p)->b[0], iVar2 != 0)) {
          local_14 = 1;
        }
        if ((g_player[p].control == '\x02') && (uVar3 = mouse_b, (uVar3 & 1) != 0)) {
          local_14 = 1;
        }
      }
    }
    local_14 = 0;
  }
  else if (mode == 1) {
    while (key[1])
      wait_tick();
    local_14 = 0;
    while (local_14 == 0) {
      wait_tick();
      cVar1 = key[0xf];
      if ((cVar1 != 0) && (cVar1 = key[0x1c], cVar1 != 0)) {
        console();
      }
      cVar1 = key[1];
      if (cVar1 != 0) {
        local_14 = 1;
      }
      cVar1 = key[0x1c];
      if (cVar1 != 0) {
        local_14 = 2;
      }
    }
    while (key[1])
      wait_tick();
    local_14--;
  }
  else {
    textout_centre(screen,dat_spacehog[SH_FONT_SMALL].dat,"GAME PAUSED, PRESS PAUSE TO CONTINUE",0x73,0x96,1)
    ;
    local_14 = 0;
  }
  return local_14;
}


/* FUN_00011438 @ 0x11438 */


int launch_sequence(void)

{
  int iVar1;
  char cVar2;
  int bVar3;
  int pan;
  int local_14;
  int local_10;
  int local_8;
  int local_4;
  
  local_14 = 0;
  bVar3 = false;
  mothership_x = 400;
  mothership_y = 0;
  engine_sound = 1;
  launch_step = 0;
  launch_tick = 0;
  if (snd_on != 0) {
    play_sample(dat_shsnd[SN_SND_ENGINE].dat,0,0x80,500,-1);
  }
  do {
    cVar2 = key[0xf];
    if ((cVar2 != 0) && (cVar2 = key[0x1c], cVar2 != 0)) {
      console();
    }
    if (!bVar3) {
      g_tick = 0;
      render_frame();
      iVar1 = g_tick;
      for (local_10 = 0; local_10 <= iVar1; local_10++) {
        anim_phase += '\x10';
        anim_tick++;
        if ((anim_tick & 1) != 0) {
          for (local_4 = 1; local_4 < 4; local_4++) {
            g_anim[local_4]++;
            if ((unsigned int)g_anim[local_4] == local_4 + 1U) {
              g_anim[local_4] = 0;
            }
          }
          bg_scroll++;
          if (0x27f < bg_scroll) {
            bg_scroll = 0;
          }
          if ((view_y != 0) && (g_anim[2] == 0)) {
            view_y--;
          }
        }
        if (opts[3] != 0) {
          update_stars();
        }
        if (snd_on != 0) {
          pan = mothership_x + 0x50;
          if (0xff < pan) {
            pan = 0xff;
          }
          adjust_sample(dat_shsnd[SN_SND_ENGINE].dat,(mothership_y * 0x14 + 4000) / mothership_x,pan,500,-1);
        }
      }
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
    if ((level_step < 2) && (0x78 < g_title_y)) {
      g_title_y--;
    }
    if ((1 < level_step) && (g_title_y < 0xf1)) {
      g_title_y += 2;
    }
    if (!bVar3) {
      while (!g_tick)
        wait_tick();
    }
    cVar2 = key[0xf];
    if (cVar2 != 0) {
      bVar3 = true;
    }
    cVar2 = key[1];
    if (cVar2 != 0) {
      local_14 = dialog("What kind of wimp would leave now?\x94""\x94""    NOT I! (esc)    MY KIND! (enter)",dat_shbgs[BG_HEAD_G].dat,1,0);
    }
  } while ((launch_step < 1) && (local_14 == 0));
  if (local_14 == 0) {
    for (local_4 = 0; local_4 <= (int)(unsigned int)two_players; local_4++) {
      if (g_player[local_4].dead == 0) {
        g_player[local_4].invuln = 0;
        g_player[local_4].warp_in = 0;
        g_player[local_4].x = mothership_x + 0x58;
        g_player[local_4].y = mothership_y + (local_4 + -1) * 10 + 0x50;
      }
    }
    render_frame();
    if (custom_level == 0) {
      dialog(dat_shlev[g_level + 0x15].dat,dat_shbgs[BG_HEAD_G].dat,0,1);
    }
    else {
      dialog("What\'s this? A homemade level? Impressive! I hope you know what you are doing."
                   ,dat_shbgs[BG_HEAD_G].dat,0,1);
    }
    local_8 = 0x28;
    do {
      cVar2 = key[0xf];
      if ((cVar2 != 0) && (cVar2 = key[0x1c], cVar2 != 0)) {
        console();
      }
      g_tick = 0;
      render_frame();
      iVar1 = g_tick;
      for (local_10 = 0; local_10 <= iVar1; local_10++) {
        anim_phase += '\x10';
        anim_tick++;
        if ((anim_tick & 1) != 0) {
          for (local_4 = 1; local_4 < 4; local_4++) {
            g_anim[local_4]++;
            if ((unsigned int)g_anim[local_4] == local_4 + 1U) {
              g_anim[local_4] = 0;
            }
          }
          bg_scroll++;
          if (0x27f < bg_scroll) {
            bg_scroll = 0;
          }
          if ((view_y != 0) && (g_anim[2] == 0)) {
            view_y--;
          }
        }
        update_objects();
        for (local_4 = 0; local_4 <= (int)(unsigned int)two_players; local_4++) {
          if (g_player[local_4].flame != 0) {
            g_player[local_4].flame++;
          }
          if (3 < g_player[local_4].flame) {
            g_player[local_4].flame = 1;
          }
          if ((g_player[local_4].dead == 0) && (local_8 != 0)) {
            g_player[local_4].y += local_4 + 1;
          }
        }
        if (local_8 != 0) {
          local_8--;
        }
      }
      while (!g_tick)
        wait_tick();
      cVar2 = key[1];
      if (cVar2 != 0) {
        local_14 = dialog("Where do you think you're going?\x94""\x94""      Back to the game! (esc)\x94""      Outta here! (enter)",dat_shbgs[BG_HEAD_G].dat,1,0);
      }
    } while ((launch_step < 2) && (local_14 == 0));
  }
  return local_14;
}


/* FUN_00011bd0 @ 0x11bd0 */


void next_level(void)

{
  void *buf;
  int iVar1;
  int p;
  
  if (custom_level == 0) {
    g_level++;
  }
  buf = dat_shlev[g_level + 3].dat;
  if (custom_level == 0) {
    level_load_mem(buf, (size_t)dat_shlev[g_level + 3].size);
  }
  else {
    iVar1 = level_load_file(level_file);
    if (iVar1 == 0) {
      level_load_file("empty.lev");
    }
  }
  clear(bmp_world);
  draw_rle_sprite(bmp_world,dat_spacehog[SH_BKG_STARS1].dat,0,0);
  draw_rle_sprite(bmp_world,dat_spacehog[SH_BKG_STARS1].dat,0,200);
  draw_rle_sprite(bmp_world,dat_spacehog[SH_BKG_STARS2].dat,0x140,0);
  draw_rle_sprite(bmp_world,dat_spacehog[SH_BKG_STARS2].dat,0x140,200);
  draw_rle_sprite(bmp_world,dat_spacehog[SH_BKG_STARS1].dat,0x280,0);
  draw_rle_sprite(bmp_world,dat_spacehog[SH_BKG_STARS1].dat,0x280,200);
  step_ticks = 0;
  level_step = 0;
  next_event = 0;
  level_done = 0;
  bg_scroll = 0;
  view_y = 0x3e;
  engine_sound = 0;
  g_title_y = 0xf0;
  bosses_alive = 0;
  init_stars();
  for (p = 0; p <= (int)(unsigned int)two_players; p++) {
    g_player[p].warp_in = 100;
    g_player[p].charge_tick = 0;
    g_player[p].charge = 0;
    g_player[p].blast = 0;
    draw_status_bar(p);
  }
  if (music_on != 0) {
    if (custom_music == 0) {
      play_midi(dat_shsnd[SN_MIDI_LEVEL].dat,-1);
    }
    else {
      destroy_midi(custom_midi);
      custom_midi = load_midi(midi_file);
      play_midi(custom_midi,1);
    }
    music_playing = 1;
  }
  clear(screen);
  render_frame();
  fade_in(dat_spacehog[SH_PAL].dat,5);
  return;
}


/* FUN_00012170 @ 0x12170 */

int ship_select(int p)

{
  unsigned int uVar1;
  char cVar2;
  int iVar3;
  int local_250;
  int local_24c [6];
  int local_234 [134];
  unsigned int local_1c;
  unsigned int local_18;
  unsigned int local_14;
  unsigned int local_10;
  unsigned int local_c;
  int local_8;
  int local_4;
  
  local_4 = 0;
  local_8 = 0;
  local_234[3] = 0x44;
  local_234[4] = 0x44;
  local_234[5] = 0x44;
  local_234[0] = 6;
  local_234[1] = 0xfffffffa;
  local_234[2] = 0;
  { static const int pilot_ofs[6] = { 0, 0, -16, -8, 0, 12 }; /* 0x11f58 */
    memcpy(local_24c, pilot_ofs, sizeof(pilot_ofs)); }
  /* local_234[6 + scancode]: "still held" flags. The original never initializes them; on
   * DOS they hold what Allegro's fade left on the stack (non-zero), so a button still held
   * from the previous screen must be released first. */
  for (local_c = 6; local_c < 134; local_c++)
    local_234[local_c] = 1;

  if (p != 0) {
    local_234[g_player[0].shipno + 3] = 0x25;
  }
  clear(bmp_world);
  draw_rle_sprite(bmp_world,dat_spacehog[SH_HANDCPU].dat,0xc,4);
  drawing_mode(2,dat_spacehog[SH_GRID].dat,0,0);
  floodfill(bmp_world,100,100,1);
  solid_mode();
  blit(bmp_world,screen,0,0,0,0,0x140,200);
  blit(bmp_world,bmp_buf,0,0,0,0,0x140,200);
  blit(bmp_buf,screen,0,0,0,0,0x140,200);
  fade_in(dat_spacehog[SH_PAL].dat,5);
  do {
    cVar2 = key[0xf];
    if ((cVar2 != 0) && (cVar2 = key[0x1c], cVar2 != 0)) {
      console();
    }
    cVar2 = key[0x54];
    if (cVar2 != 0) {
      save_screenshot();
    }
    if (joy_present != 0) {
      poll_joystick();
    }
    local_c = 0;
    local_18 = 0;
    local_14 = 0;
    local_1c = 0;
    if ((g_player[p].control == CTRL_PAD) && (joy_present != 0)) {
      iVar3 = pjoy(p)->up;
      local_c = (unsigned int)(iVar3 != 0);
      iVar3 = pjoy(p)->down;
      local_10 = (unsigned int)(iVar3 != 0);
      iVar3 = pjoy(p)->left;
      local_18 = (unsigned int)(iVar3 != 0);
      iVar3 = pjoy(p)->right;
      local_14 = (unsigned int)(iVar3 != 0);
      iVar3 = pjoy(p)->b[0];
      local_1c = (unsigned int)(iVar3 != 0);
    }
    else if (g_player[p].control == CTRL_KEYBOARD) {
      local_c = (unsigned int)(pkey(p, PK_UP) != 0);
      local_10 = (unsigned int)(pkey(p, PK_DOWN) != 0);
      local_18 = (unsigned int)(pkey(p, PK_LEFT) != 0);
      local_14 = (unsigned int)(pkey(p, PK_RIGHT) != 0);
      local_1c = (unsigned int)(pkey(p, PK_FIRE) != 0);
    }
    else {
      uVar1 = mouse_b;
      local_10 = (unsigned int)((uVar1 & 2) != 0);
      uVar1 = mouse_b;
      if ((uVar1 & 1) != 0) {
        local_1c = 1;
      }
    }
    cVar2 = key[0x1c];
    if (cVar2 != 0) {
      local_1c = 1;
    }
    if ((((pkey(p, PK_FIRE) == 0) && (cVar2 = key[0x1c], cVar2 == 0)) &&
        (iVar3 = pjoy(p)->b[0], iVar3 == 0)) && (uVar1 = mouse_b, (uVar1 & 1) == 0)) {
      local_234[g_keys[p * 7 + PK_FIRE] + 6] = 0;
    }
    if ((pkey(p, PK_UP) == 0) && (iVar3 = pjoy(p)->up, iVar3 == 0)) {
      local_234[g_keys[p * 7 + PK_UP] + 6] = 0;
    }
    if (((pkey(p, PK_DOWN) == 0) && (iVar3 = pjoy(p)->down, iVar3 == 0)) &&
       (uVar1 = mouse_b, (uVar1 & 2) == 0)) {
      local_234[g_keys[p * 7 + PK_DOWN] + 6] = 0;
    }
    if ((pkey(p, PK_LEFT) == 0) && (iVar3 = pjoy(p)->up, iVar3 == 0)) {
      local_234[g_keys[p * 7 + PK_LEFT] + 6] = 0;
    }
    if ((pkey(p, PK_RIGHT) == 0) && (iVar3 = pjoy(p)->down, iVar3 == 0)) {
      local_234[g_keys[p * 7 + PK_RIGHT] + 6] = 0;
    }
    blit(bmp_world,bmp_buf,0,0,0,0,0x140,200);
    draw_rle_sprite(bmp_buf,dat_spacehog[local_8 + 0xf].dat,0x2a,local_234[local_8] + 0x3f);
    set_clip(bmp_buf,0x1d,0x13,0xf2,0xa5);
    draw_rle_sprite(bmp_buf,dat_shbgs[local_8 + 0x18].dat,local_24c[local_8 * 2] + 0x74,
                    local_24c[local_8 * 2 + 1] + 0x12);
    set_clip(bmp_buf,0,0,0x13f,0xef);
    textprintf(bmp_buf,dat_spacehog[SH_FONT].dat,0xb4,0x10,0x4a,"Player %d",p + 1);
    local_250 = 0x48;
    if ((p != 0) && (g_player[0].shipno == local_8)) {
      textout(bmp_buf,dat_spacehog[SH_FONT].dat,"U  N  A  V  A  I  L  A  B  L  E",0x2a,0x50,0x28);
      local_250 = 0x26;
    }
    textout(bmp_buf,dat_spacehog[SH_FONT_SMALL].dat,"PILOT:",0x28,0x17,local_250 + 2);
    if (local_8 == 0) {
      textout(bmp_buf,dat_spacehog[SH_FONT_SMALL].dat,"Fast and agile but limited",0x28,0x80,local_250);
      textout(bmp_buf,dat_spacehog[SH_FONT_SMALL].dat,"firepower. Equipped with",0x28,0x86,local_250);
      textout(bmp_buf,dat_spacehog[SH_FONT_SMALL].dat,"Berserk charges.",0x28,0x8c,local_250);
      textout(bmp_buf,dat_spacehog[SH_FONT_ARIAL].dat,"Billy 68",0x42,0x14,local_250);
      textout(bmp_buf,dat_spacehog[SH_FONT].dat,"Viper",0x3c,0x91,local_250 + 2);
      textout(bmp_buf,dat_spacehog[SH_FONT_SMALL].dat,"Unreliable and totally",0x28,0x24,local_250);
      textout(bmp_buf,dat_spacehog[SH_FONT_SMALL].dat,"insane. This character",0x28,0x2a,local_250);
      textout(bmp_buf,dat_spacehog[SH_FONT_SMALL].dat,"is safer off-Earth.",0x28,0x30,local_250);
    }
    if (local_8 == 1) {
      textout(bmp_buf,dat_spacehog[SH_FONT_SMALL].dat,"An average ship for general",0x28,0x80,local_250);
      textout(bmp_buf,dat_spacehog[SH_FONT_SMALL].dat,"purposes. Carries the feared",0x28,0x86,local_250);
      textout(bmp_buf,dat_spacehog[SH_FONT_SMALL].dat,"Hellrider missiles.",0x28,0x8c,local_250);
      textout(bmp_buf,dat_spacehog[SH_FONT_ARIAL].dat,"Sheila Longhart",0x42,0x14,local_250);
      textout(bmp_buf,dat_spacehog[SH_FONT].dat,"Albatross",0x3c,0x91,local_250 + 2);
      textout(bmp_buf,dat_spacehog[SH_FONT_SMALL].dat,"English Lady with a taste",0x28,0x24,local_250);
      textout(bmp_buf,dat_spacehog[SH_FONT_SMALL].dat,"for action. Beautiful and",0x28,0x2a,local_250);
      textout(bmp_buf,dat_spacehog[SH_FONT_SMALL].dat,"sexy but cold to the bone.",0x28,0x30,local_250);
    }
    if (local_8 == 2) {
      textout(bmp_buf,dat_spacehog[SH_FONT_SMALL].dat,"Powerfull and heavy but a",0x28,0x80,local_250);
      textout(bmp_buf,dat_spacehog[SH_FONT_SMALL].dat,"bit slow. Onboard is the",0x28,0x86,local_250);
      textout(bmp_buf,dat_spacehog[SH_FONT_SMALL].dat,"Fireworks weapon.",0x28,0x8c,local_250);
      textout(bmp_buf,dat_spacehog[SH_FONT_ARIAL].dat,"Maximillian Drake",0x42,0x14,local_250);
      textout(bmp_buf,dat_spacehog[SH_FONT].dat,"Bulldog",0x3c,0x91,local_250 + 2);
      textout(bmp_buf,dat_spacehog[SH_FONT_SMALL].dat,"Threefold winner of the",0x28,0x24,local_250);
      textout(bmp_buf,dat_spacehog[SH_FONT_SMALL].dat,"Mr Universe contest. Cocky",0x28,0x2a,local_250);
      textout(bmp_buf,dat_spacehog[SH_FONT_SMALL].dat,"and selfconfident American.",0x28,0x30,local_250);
    }
    if (local_10 != 0) {
      draw_rle_sprite(screen,dat_spacehog[SH_ARROWD2].dat,0x112,0x96);
    }
    if (local_c != 0) {
      draw_rle_sprite(screen,dat_spacehog[SH_ARROWU2].dat,0x112,0x7a);
    }
    if (local_14 != 0) {
      draw_rle_sprite(screen,dat_spacehog[SH_ARROWR2].dat,0x120,0x88);
    }
    if (local_18 != 0) {
      draw_rle_sprite(screen,dat_spacehog[SH_ARROWL2].dat,0x104,0x88);
    }
    vsync();
    blit(bmp_buf,screen,0,0,0,0,0x140,200);
    if ((local_10 != 0) && (local_234[g_keys[p * 7 + PK_DOWN] + 6] == 0)) {
      local_8++;
      if (local_8 == 3) {
        local_8 = 0;
      }
      local_234[g_keys[p * 7 + PK_DOWN] + 6] = 1;
    }
    if ((local_c != 0) && (local_234[g_keys[p * 7 + PK_UP] + 6] == 0)) {
      local_8--;
      if (local_8 == -1) {
        local_8 = 2;
      }
      local_234[g_keys[p * 7 + PK_UP] + 6] = 1;
    }
    if ((local_18 != 0) && (local_234[g_keys[p * 7 + PK_LEFT] + 6] == 0)) {
      local_234[g_keys[p * 7 + PK_LEFT] + 6] = 1;
    }
    if ((local_14 != 0) && (local_234[g_keys[p * 7 + PK_RIGHT] + 6] == 0)) {
      local_234[g_keys[p * 7 + PK_RIGHT] + 6] = 1;
    }
    cVar2 = key[1];
    if (cVar2 != 0) {
      local_4 = 1;
    }
    if (((local_1c != 0) && (local_234[g_keys[p * 7 + PK_FIRE] + 6] == 0)) &&
       ((local_234[g_keys[p * 7 + PK_FIRE] + 6] = 1, p == 0 || (g_player[0].shipno != local_8)))
       ) {
      local_4 = 2;
      draw_rle_sprite(screen,dat_spacehog[SH_BUTTONOK2].dat,0x10b,0xb0);
    }
  } while (local_4 == 0);
  if (snd_on != 0) {
    play_sample(dat_shsnd[SN_SND_SELECT].dat,200,0x80,1000,0);
  }
  g_player[p].shipno = local_8;
  fade_out(2);
  return local_4 != 1;
}


/* FUN_000131bc @ 0x131bc */


int prepare_game(void)

{
  int iVar1;
  int local_4;
  
  stop_midi();
  music_playing = 0;
  g_level = g_ae6c4;
  g_player[0].dead = 1;
  g_player[1].dead = 1;
  g_player[0].lives = 0;
  g_player[1].lives = 0;
  g_player[0].warp_in = 100;
  g_player[1].warp_in = 100;
  pad_used = 0;
  mouse_used = 0;
  init_ship_types();
  init_enemy_templates();
  init_ebullet_templates();
  local_4 = 0;
  while( true ) {
    if ((int)(unsigned int)two_players < local_4) {
      gameover_delay = 100;
      return 1;
    }
    respawn_player(local_4);
    iVar1 = ship_select(local_4);
    if (iVar1 == 0) break;
    init_player(local_4);
    local_4++;
  }
  return 0;
}


/* FUN_0001ccd8 @ 0x1ccd8 */

void pause_game(void)

{
  int iVar2;
  int iVar3;
  
  midi_pause();
  iVar2 = rand();
  iVar3 = iVar2;
  if (iVar2 < 0) {
    iVar3 = iVar2 + 3;
  }
  iVar2 = iVar2 + (iVar3 >> 2) * -4;
  if (iVar2 == 1) {
    dialog("Hey! Get back here, there\'s still lots of alien butt to kick!",
                 dat_shbgs[BG_HEAD_G].dat,2,0);
  }
  else {
    if (iVar2 < 2) {
      if (iVar2 == 0) {
        dialog("What? Coffeebreak?!",dat_shbgs[BG_HEAD_G].dat,2,0);
        goto cb1cd94;
      }
    }
    else if (iVar2 == 2) {
      dialog("Hey! What\'s happening? Everything just froze!",dat_shbgs[BG_HEAD_G].dat,2,0);
      goto cb1cd94;
    }
    dialog("Where do you think you\'re going?",dat_shbgs[BG_HEAD_G].dat,2,0);
  }
cb1cd94:
  while (!key[0x7b])
    wait_tick();
  while (key[0x7b])
    wait_tick();
  midi_resume();
  return;
}


/* FUN_0001ce08 @ 0x1ce08 */


void level_end_tally(void)

{
  int iVar1;
  char cVar2;
  int bVar3;
  int local_8;
  int p;
  
  bVar3 = false;
  clear(bmp_world);
  draw_rle_sprite(bmp_world,dat_spacehog[SH_HANDCPU].dat,0xc,4);
  drawing_mode(2,dat_spacehog[SH_GRID].dat,0,0);
  floodfill(bmp_world,100,100,1);
  solid_mode();
  textout_centre(bmp_world,dat_spacehog[SH_FONT_ARIAL].dat,"LEVEL COMPLETED",0x88,0x16,0x4a);
  textout_centre(bmp_world,dat_spacehog[SH_FONT_ARIAL].dat,"Player 1",0x78,0x32,0x48);
  if (two_players != 0) {
    textout_centre(bmp_world,dat_spacehog[SH_FONT_ARIAL].dat,"Player 2",0xb4,0x32,0x48);
  }
  textout(bmp_world,dat_spacehog[SH_FONT_ARIAL].dat,"Score:",0x28,0x46,0x48);
  textout(bmp_world,dat_spacehog[SH_FONT_ARIAL].dat,"Shipbonus:",0x28,0x54,0x48);
  textout(bmp_world,dat_spacehog[SH_FONT_ARIAL].dat,"Levelbonus:",0x28,0x62,0x48);
  line(bmp_world,0x26,0x6e,0xe6,0x6e,0x48);
  textout(bmp_world,dat_spacehog[SH_FONT_ARIAL].dat,"Total:",0x28,0x70,0x4a);
  blit(bmp_world,screen,0,0,0,0,0x140,200);
  blit(bmp_world,bmp_buf,0,0,0,0,0x140,200);
  blit(bmp_buf,screen,0,0,0,0,0x140,200);
  fade_in(dat_spacehog[SH_PAL].dat,5);
  g_tick = 0;
  while (iVar1 = g_tick, iVar1 < 10) {
    wait_tick();
    cVar2 = key[0xf];
    if ((cVar2 != 0) && (cVar2 = key[0x1c], cVar2 != 0)) {
      console();
    }
  }
  for (p = 0; p <= (int)(unsigned int)two_players; p++) {
    textprintf_centre(screen,dat_spacehog[SH_FONT_ARIAL].dat,p * 0x3c + 0x78,0x46,0x4a,"%ld",
                      g_player[p].score);
  }
  if (snd_on != 0) {
    play_sample(dat_shsnd[SN_SND_FIRE1].dat,200,0x80,1000,0);
  }
  g_tick = 0;
  while (iVar1 = g_tick, iVar1 < 10) {
    wait_tick();
    cVar2 = key[0xf];
    if ((cVar2 != 0) && (cVar2 = key[0x1c], cVar2 != 0)) {
      console();
    }
  }
  for (p = 0; p <= (int)(unsigned int)two_players; p++) {
    if (g_player[p].dead == 0) {
      textprintf_centre(screen,dat_spacehog[SH_FONT_ARIAL].dat,p * 0x3c + 0x78,0x54,0x4a,"%ld",
                        (unsigned int)g_player[p].lives * 1000);
      g_player[p].score = g_player[p].score + (unsigned int)g_player[p].lives * 1000;
    }
  }
  if (snd_on != 0) {
    play_sample(dat_shsnd[SN_SND_FIRE1].dat,200,0x80,1000,0);
  }
  g_tick = 0;
  while (iVar1 = g_tick, iVar1 < 10) {
    wait_tick();
    cVar2 = key[0xf];
    if ((cVar2 != 0) && (cVar2 = key[0x1c], cVar2 != 0)) {
      console();
    }
  }
  for (p = 0; p <= (int)(unsigned int)two_players; p++) {
    if (g_player[p].dead == 0) {
      textprintf_centre(screen,dat_spacehog[SH_FONT_ARIAL].dat,p * 0x3c + 0x78,0x62,0x4a,"%ld",
                        difficulty * g_level * 100);
      g_player[p].score = difficulty * g_level * 100 + g_player[p].score;
    }
  }
  if (snd_on != 0) {
    play_sample(dat_shsnd[SN_SND_FIRE1].dat,200,0x80,1000,0);
  }
  g_tick = 0;
  while (iVar1 = g_tick, iVar1 < 10) {
    wait_tick();
    cVar2 = key[0xf];
    if ((cVar2 != 0) && (cVar2 = key[0x1c], cVar2 != 0)) {
      console();
    }
  }
  for (p = 0; p <= (int)(unsigned int)two_players; p++) {
    textprintf_centre(screen,dat_spacehog[SH_FONT_ARIAL].dat,p * 0x3c + 0x78,0x70,0x4c,"%ld",
                      g_player[p].score);
  }
  if (snd_on != 0) {
    play_sample(dat_shsnd[SN_SND_BUILD].dat,200,0x80,1000,0);
  }
  while (!bVar3) {
    wait_tick();
    cVar2 = key[0xf];
    if ((cVar2 != 0) && (cVar2 = key[0x1c], cVar2 != 0)) {
      console();
    }
    if (joy_present != 0) {
      poll_joystick();
    }
    for (p = 0; p <= (int)(unsigned int)two_players; p++) {
      if ((g_player[p].control == CTRL_KEYBOARD) && (pkey(p, PK_FIRE) != 0)) {
        bVar3 = true;
      }
      if ((g_player[p].control == CTRL_PAD) && (iVar1 = pjoy(p)->b[0], iVar1 != 0)) {
        bVar3 = true;
      }
      if ((g_player[p].control == '\x02') && (iVar1 = mouse_b, iVar1 != 0)) {
        bVar3 = true;
      }
    }
  }
  draw_rle_sprite(screen,dat_spacehog[SH_BUTTONOK2].dat,0x10b,0xb0);
  if (snd_on != 0) {
    play_sample(dat_shsnd[SN_SND_SELECT].dat,200,0x80,1000,0);
  }
  fade_out(5);
  if (difficulty < 3) {
    for (local_8 = 0; local_8 < 9; local_8++) {
      for (p = 0; p <= (int)(unsigned int)two_players; p++) {
        g_player[p].ship.lvl[local_8] = g_player[p].wlvl[local_8];
      }
    }
  }
  return;
}


/* 0x1de4c / 0x1e2a0: end-of-level pilot comments, [6 + ship * 6 + level - 1] */
static const char *const pilot_line_a[24] = {
   "Easy start, wasn't it? Those pesky aliens doesn't seem to be that tough.",
   "Still going strong? Goodie. Now get back here, I have some fresh repairs for you.",
   "Good work. Hungry aliens fight bad, and their ships wont fly without fuel.",
   "I guess our alien enemies have learned how to assemble their big guns. Better get some shield.",
   "Nice flyin' soldier. I'm impressed. Those rocks aren't easy to fly through.",
   "I don't have any line here.",
   "Blam blam! I'm going down! Aaaargh!. Just kidding, that was fun. Those scumbags are dead.\x94""D. E. A. D.",
   "Fchiou! Ratatata! You're dead. Haha! I like these aliens! They die easily. Heh heh.",
   "BOOOOM! KA-BLAM! Mwahahaa! Lightning strikes the skies! No more food for the slimy aliens!",
   "Oooh! They have big guns! I like guns! Har har! Maybe I switch side soon. Heh.",
   "Big rocks! Big rocks! Blast 'em! Har har har.",
   "I don't have any line here.",
   "That wasn't so bad. I'd like some more resistance though, almost too easy.",
   "Ah, finally. I'm getting tired from this. Let go back to the mothership.",
   "Not much left, eh? Large craft are easier too hit, aren't they?",
   "How come they are so many and large and we are so few and small? Still they have difficulty getting us.",
   "I hope we don't have go through those again.",
   "I don't have any line here.",
   "Sigh, this is boring. I need something more challening. Why don't we warp into the heart of the enemy or something?",
   "I am the greatest, I am the greatest. What can stop us now?",
   "Woohaa! Damn I'm good. Their supplies are gone, away, lost, whatever.",
   "Somebody stop me! Ha ha ha. I'm invincible! Their guns are no match for me. Hah!",
   "Hah! Did you see that? Disintegration! I rule!",
   "I don't have any line here."
};
static const char *const pilot_line_b[24] = {
   "I don't have any line here.",
   "I don't have any line here.",
   "I don't have any line here.",
   "I don't have any line here.",
   "I don't have any line here.",
   "I don't have any line here.",
   "Easy targets... Heh. Can I do it again? Please?\x94""Bleargh!",
   "Still alive! Ahahahaaaa... I eat aliens for breakfast! Every night! Har har.",
   "Food... Guns... Fuel... All blasted into the void.\x94""Bbbbblll.",
   "Evade, evade! Mwahaaha... Hmmm... I guess those flashing dots aren't powerups, eh?",
   "Next time I'll blast the big one too!\x94""Mwhahahaha!",
   "I don't have any line here.",
   "I didn't know their fighters were so weak, this i like vacation.",
   "Is this the best they can do? This will be an easy ride then.",
   "I'm dying for a sandwich myself... Let's head back and get some coffee.",
   "Who cares? The idea is to not stand in their line of fire. Then hit them where it hurts.",
   "Not so amusing, I can imagine a lot of things I'd rather do.",
   "I don't have any line here.",
   "They don't stand a chance against us. So far they're not more than bugs.",
   "Back to glory and fame! I really get hungry from all this shooting.",
   "You're right about that, resistance might be weaker in the nearest future.",
   "Hah! They couldn't hit a barn with their blasters even if they in it. Loosers.",
   "Piece of cake. I'd do it in my sleep.",
   "I don't have any line here."
};

/* FUN_0001e300 @ 0x1e300 */

void pilot_comments(int level)

{
  char local_208 [256];
  char local_108 [256];
  int local_8;
  int local_4;
  
  /* the original leaves the texts uninitialized when both players are dead (the level can
   * end during the game-over delay) and shows stack garbage; here: empty text boxes */
  local_108[0] = 0;
  local_208[0] = 0;
  local_4 = g_player[0].shipno;
  local_8 = g_player[1].shipno;
  level--;
  if ((g_player[0].dead == 0) && (g_player[1].dead == 0)) {
    if (g_player[0].score < g_player[1].score) {
      strcpy(local_108,(char *)pilot_line_a[6 + local_8 * 6 + level]);
      strcpy(local_208,(char *)pilot_line_b[6 + local_4 * 6 + level]);
    }
    else {
      strcpy(local_108,(char *)pilot_line_a[6 + local_4 * 6 + level]);
      strcpy(local_208,(char *)pilot_line_b[6 + local_8 * 6 + level]);
    }
  }
  if ((g_player[0].dead == 0) && (g_player[1].dead != 0)) {
    strcpy(local_108,pilot_line_a[level]);
    strcpy(local_208,(char *)pilot_line_b[6 + local_4 * 6 + level]);
    local_8 = local_4;
    local_4 = 3;
  }
  if ((g_player[0].dead != 0) && (g_player[1].dead == 0)) {
    strcpy(local_108,pilot_line_a[level]);
    strcpy(local_208,(char *)pilot_line_b[6 + local_8 * 6 + level]);
    local_4 = 3;
  }
  dialog(local_108,dat_shbgs[local_4 + 0x13].dat,0,1);
  render_frame();
  dialog(local_208,dat_shbgs[local_8 + 0x13].dat,0,1);
  render_frame();
  return;
}


/* FUN_0002063c @ 0x2063c */


int ending(void)

{
  char cVar1;
  int iVar2;
  int uVar3;
  int y;
  int x;
  fixed fVar4;
  int local_44c;
  PALETTE local_424;
  int local_24;
  int local_20;
  int local_1c;
  int local_18;
  int pt;
  int local_10;
  int local_c;
  int local_8;
  unsigned char local_1;
  
  for (pt = 0; pt < 100; pt++) {
    g_particle[pt].active = 0;
  }
  clear(screen);
  set_pallete(dat_spacehog[SH_PAL].dat);
  get_pallete(local_424);
  for (pt = 1; pt < 0x100; pt++) {
    local_424[pt].r = '?';
    local_424[pt].g = '?';
    local_424[pt].b = '?';
  }
  set_pallete((RGB *)&black_pallete);
  rest(1000);
  clear(screen);
  draw_rle_sprite(screen,dat_spacehog[SH_BKG_STARS1].dat,0,0);
  draw_rle_sprite(screen,dat_spacehog[SH_BKG_STARS1].dat,0,200);
  rectfill(screen,0,0,0x13f,0x13,0);
  rectfill(screen,0,0xb4,0x13f,199,0);
  fade_in(dat_spacehog[SH_PAL].dat,2);
  iVar2 = intro_wait(0x28);
  if (iVar2 == 0) {
    blit(screen,bmp_world,0,0,0,0,0x140,200);
    dialog("The aliens are all scattered, fleeing back towards where ever they came from. The earth is saved for now. Mankind can continue to live on our planet in peace."
                 ,dat_shbgs[BG_HEAD_G].dat,0,1);
    if (opt_vsync != 0) {
      vsync();
    }
    blit(bmp_world,screen,0,0,0,0,0x140,200);
    iVar2 = intro_wait(0x14);
    if (iVar2 == 0) {
      dialog("Congratulations! Operation Spacehog is completed. A success thanks to you.",
                   dat_shbgs[BG_HEAD_G].dat,0,1);
      if (music_on != 0) {
        play_midi(dat_shsnd[SN_MIDI_TITLE].dat,1);
        music_playing = 1;
      }
      for (local_1c = 0; local_1c < 400; local_1c++) {
        g_tick = 0;
        clear(bmp_buf);
        draw_rle_sprite(bmp_buf,dat_spacehog[SH_BKG_STARS1].dat,0,-local_1c >> 1);
        draw_rle_sprite(bmp_buf,dat_spacehog[SH_BKG_STARS1].dat,0,200 - (local_1c >> 1));
        draw_rle_sprite(bmp_buf,dat_shbgs[BG_EARTH].dat,0,0x208 - local_1c);
        if ((opt_vsync != 0) && (cVar1 = key[0xf], cVar1 == 0)) {
          vsync();
        }
        blit(bmp_buf,screen,0,0x14,0,0x14,0x140,0xa0);
        do {
          iVar2 = g_tick;
          if (iVar2 != 0) break;
          wait_tick();
          cVar1 = key[0xf];
        } while (cVar1 == 0);
        cVar1 = key[1];
        if (cVar1 != 0) {
          return 0;
        }
      }
      blit(screen,bmp_world,0,0,0,0,0x140,200);
      local_24 = 0x14;
      local_1 = 0;
      g_tick = 0;
      do {
        blit(bmp_world,bmp_buf,0,0,0,0,0x140,200);
        iVar2 = rand();
        if (iVar2 % local_24 == 0) {
          local_24--;
          if (local_24 < 1) {
            iVar2 = rand();
            local_24 = iVar2 % 0x14 + 0x14;
          }
          iVar2 = rand();
          local_8 = itofix(iVar2 % 200 + 0x3c);
          iVar2 = rand();
          local_c = itofix(0xa0 - iVar2 % 0x32);
          local_18 = 0;
          while( true ) {
            iVar2 = rand();
            local_44c = iVar2;
            if (iVar2 < 0) {
              local_44c = iVar2 + 3;
            }
            if (iVar2 + (local_44c >> 2) * -4 <= local_18) break;
            for (pt = 0; (g_particle[pt].active != 0 && (pt < 100));
                pt++) {
            }
            g_particle[pt].x = local_8;
            g_particle[pt].y = local_c;
            iVar2 = rand();
            local_1 = (unsigned char)((char)(iVar2 % 0x28) + 0xa2);
            iVar2 = rand();
            fVar4 = itofix(iVar2 % 6 + 1);
            local_10 = fVar4 / 2;
            g_particle[pt].active = 1;
            g_particle[pt].color = particle_color;
            fVar4 = itofix((unsigned int)local_1);
            fVar4 = fcos(fVar4);
            fVar4 = fmul(local_10,fVar4);
            g_particle[pt].vx = fVar4;
            fVar4 = itofix((unsigned int)local_1);
            fVar4 = fsin(fVar4);
            fVar4 = fmul(local_10,fVar4);
            g_particle[pt].vy = fVar4;
            g_particle[pt].age = 0;
            local_18++;
          }
        }
        for (pt = 0; pt < 100; pt++) {
          if (g_particle[pt].active != 0) {
            iVar2 = g_particle[pt].color;
            y = fixtoi(g_particle[pt].y);
            x = fixtoi(g_particle[pt].x);
            putpixel(bmp_buf,x,y,iVar2);
            g_particle[pt].x += g_particle[pt].vx;
            g_particle[pt].y += g_particle[pt].vy;
            g_particle[pt].color--;
            g_particle[pt].age++;
            if ((g_particle[pt].age == 0x10) &&
               (g_particle[pt].active = 0, g_particle[pt].color < 0x14)) {
              iVar2 = rand();
              local_20 = firework_colors[iVar2 % 5];
              iVar2 = rand();
              local_1c = (iVar2 % 0x1e + 0xc) / 2;
              iVar2 = rand();
              local_44c = iVar2;
              if (iVar2 < 0) {
                local_44c = iVar2 + 3;
              }
              fVar4 = itofix(iVar2 + (local_44c >> 2) * -4 + 1);
              local_10 = fVar4 / 6;
              for (local_18 = 0; local_18 < local_1c; local_18++) {
                spawn_particle(g_particle[pt].x,g_particle[pt].y,local_10,
                             (int)(0x100 / (long long)local_1c) * local_18 & 0xff,local_20);
              }
            }
          }
        }
        if ((opt_vsync != 0) && (cVar1 = key[0xf], cVar1 == 0)) {
          vsync();
        }
        blit(bmp_buf,screen,0,0x14,0,0x14,0x140,0xa0);
        do {
          iVar2 = g_tick;
          if (iVar2 != 0) break;
          wait_tick();
          cVar1 = key[0xf];
        } while (cVar1 == 0);
        cVar1 = key[1];
        if (cVar1 != 0) {
          return 0;
        }
        iVar2 = g_tick;
      } while ((iVar2 < 300) && (cVar1 = key[0x39], cVar1 == 0));
      blit(bmp_world,screen,0,0,0,0,0x140,200);
      iVar2 = intro_type("T H E   E N D",0x6e,0x50,0x1f);
      if (iVar2 == 0) {
        iVar2 = intro_wait(0x3c);
        if (iVar2 == 0) {
          blit(bmp_world,screen,0,0,0,0,0x140,200);
          iVar2 = intro_wait(0x14);
          if (iVar2 == 0) {
            fade_out(2);
            clear(screen);
            set_pallete(dat_spacehog[SH_PAL].dat);
            stop_midi();
            music_playing = 0;
            uVar3 = 1;
          }
          else {
            uVar3 = 0;
          }
        }
        else {
          uVar3 = 0;
        }
      }
      else {
        uVar3 = 0;
      }
    }
    else {
      uVar3 = 0;
    }
  }
  else {
    uVar3 = 0;
  }
  return uVar3;
}


/* FUN_00021a7c @ 0x21a7c */


void play_game(void)

{
  int iVar1;
  int iVar2;
  char cVar3;
  int iVar4;
  unsigned int uVar5;
  int iVar6;
  int local_3c;
  int local_28;
  int local_14;
  int p;
  unsigned int local_4;
  
  if (music_on != 0) {
    play_midi((MIDI *)0x0,0);
    music_playing = 0;
  }
  do {
    next_level();
    local_28 = launch_sequence();
    while (((local_28 == 0 && (gameover_delay != 0)) && (level_done == 0))) {
      g_tick = 0;
      render_frame();
      if (two_players == 0) {
        local_14 = g_player[0].y;
      }
      else if ((g_player[0].dead == 0) && (g_player[1].dead != 0)) {
        local_14 = g_player[0].y;
      }
      else if ((g_player[0].dead == 0) || (g_player[1].dead != 0)) {
        local_14 = (g_player[0].y + g_player[1].y) >> 1;
      }
      else {
        local_14 = g_player[1].y;
      }
      iVar4 = (local_14 * 0x3e) / 0xf0;
      if (view_y < iVar4) {
        view_y++;
      }
      else if (iVar4 < view_y) {
        view_y--;
      }
      min_y = view_y + 0xb;
      max_y = view_y + 0xa7;
      anim_phase += '\x10';
      anim_tick++;
      if ((anim_tick & 1) != 0) {
        for (local_4 = 1; (int)local_4 < 4; local_4++) {
          g_anim[local_4]++;
          if ((unsigned int)g_anim[local_4] == local_4 + 1) {
            g_anim[local_4] = 0;
          }
        }
        if (opts[2] != 0) {
          scroll_bgobjs(1);
        }
        frame_parity++;
        if ((frame_parity & 1) != 0) {
          if (opts[2] != 0) {
            scroll_bgobjs(0);
          }
          bg_scroll++;
          if (0x27f < bg_scroll) {
            bg_scroll = 0;
          }
        }
      }
      if ((bosses_alive == 0) && ((g_player[0].warp_in == 0 || (g_player[1].warp_in == 0)))) {
        step_ticks++;
      }
      if (step_ticks == 0x50) {
        level_step++;
        step_ticks = 0;
        run_events();
      }
      spawn_waves();
      update_objects();
      collisions();
      if (((g_player[0].dead != 0) && (g_player[1].dead != 0)) && (gameover_delay != 0)) {
        gameover_delay--;
      }
      if (pad_used != 0) {
        poll_joystick();
      }
      for (p = 0; p <= (int)(unsigned int)two_players; p++) {
        if (g_player[p].flame != 0) {
          g_player[p].flame++;
        }
        if (3 < g_player[p].flame) {
          g_player[p].flame = 1;
        }
        if (g_player[p].invuln != 0) {
          g_player[p].invuln--;
        }
        if (g_player[p].warp_in == 1) {
          respawn_player(p);
        }
        if (g_player[p].warp_in != 0) {
          g_player[p].warp_in--;
        }
        if (g_player[p].special_time != 0) {
          g_player[p].special_time--;
        }
        if ((g_player[p].special_time == 1) && (g_player[p].shipno == 0)) {
          g_player[p].plasma = (unsigned char)g_player[p].saved_plasma;
        }
        local_4 = (unsigned int)(p == 0);
        if (((g_player[p].dead != 0) && (1 < g_player[local_4].lives)) &&
           (((g_player[p].control == CTRL_PAD && (iVar6 = pjoy(p)->b[2], iVar6 != 0)) ||
            (pkey(p, PK_SPECIAL) != 0)))) {
          g_player[local_4].lives--;
          g_player[p].lives++;
          g_player[p].dead = 0;
          g_player[p].warp_in = 0x32;
        }
        if (((g_player[p].dead == 0) && (g_player[p].warp_in == 0)) &&
           (g_player[p].blast == 0)) {
          if (g_player[p].control == CTRL_PAD) {
            iVar6 = pjoy(p)->up;
            if (iVar6 != 0) {
              g_player[p].vy--;
            }
            iVar6 = pjoy(p)->down;
            if (iVar6 != 0) {
              g_player[p].vy++;
            }
            iVar6 = pjoy(p)->left;
            if (iVar6 != 0) {
              g_player[p].vx--;
            }
            iVar6 = pjoy(p)->right;
            if (iVar6 != 0) {
              g_player[p].vx++;
            }
            if ((0 < g_player[p].vx) && (iVar6 = pjoy(p)->right, iVar6 == 0)) {
              g_player[p].vx--;
            }
            if ((g_player[p].vx < 0) && (iVar6 = pjoy(p)->left, iVar6 == 0)) {
              g_player[p].vx++;
            }
            if ((0 < g_player[p].vy) && (iVar6 = pjoy(p)->down, iVar6 == 0)) {
              g_player[p].vy--;
            }
            if ((g_player[p].vy < 0) && (iVar6 = pjoy(p)->up, iVar6 == 0)) {
              g_player[p].vy++;
            }
            iVar6 = pjoy(p)->b[1];
            if (((iVar6 != 0) && (-1 < g_player[p].powerup)) && (g_player[p].select_held == 0)) {
              buy_powerup(p);
            }
            iVar6 = pjoy(p)->b[1];
            if (iVar6 == 0) {
              g_player[p].select_held = 0;
            }
            iVar6 = pjoy(p)->b[0];
            if ((iVar6 == 0) && (g_player[p].charge != 0)) {
              player_fire(p);
            }
            iVar6 = pjoy(p)->b[0];
            if (iVar6 == 0) {
              g_player[p].fire_wait = 0;
            }
            iVar6 = pjoy(p)->b[0];
            if ((iVar6 != 0) && (g_player[p].buildup != 0)) {
              g_player[p].charge_tick = (unsigned int)g_player[p].buildup + g_player[p].charge_tick;
            }
            iVar6 = pjoy(p)->b[0];
            if ((iVar6 != 0) && (g_player[p].fire_wait == 0)) {
              player_fire(p);
            }
            iVar6 = pjoy(p)->b[2];
            if (((iVar6 != 0) && (g_player[p].special_time == 0)) && (g_player[p].specials != 0)) {
              use_special(p);
            }
            if (((pkey(p, PK_SPECIAL) != 0) && (g_player[p].special_time == 0)) &&
               (g_player[p].specials != 0)) {
              use_special(p);
            }
          }
          else if (g_player[p].control == '\x02') {
            iVar6 = mouse_x;
            iVar1 = mouse_y;
            iVar2 = mouse_b;
            if (iVar4 + iVar1 < g_player[p].y) {
              g_player[p].vy--;
            }
            if (g_player[p].y < iVar4 + iVar1) {
              g_player[p].vy++;
            }
            if (iVar4 + iVar6 < g_player[p].x) {
              g_player[p].vx--;
            }
            if (g_player[p].x < iVar4 + iVar6) {
              g_player[p].vx++;
            }
            if (((iVar2 == 2) && (-1 < g_player[p].powerup)) && (g_player[p].select_held == 0)) {
              buy_powerup(p);
            }
            if (iVar2 != 2) {
              g_player[p].select_held = 0;
            }
            if ((iVar2 != 1) && (g_player[p].charge != 0)) {
              player_fire(p);
            }
            if (iVar2 == 1) {
              if (g_player[p].buildup != 0) {
                g_player[p].charge_tick = (unsigned int)g_player[p].buildup + g_player[p].charge_tick;
              }
            }
            else {
              g_player[p].fire_wait = 0;
            }
            if ((iVar2 == 1) && (g_player[p].fire_wait == 0)) {
              player_fire(p);
            }
            if (((iVar2 == 3) && (g_player[p].special_time == 0)) && (g_player[p].specials != 0)) {
              use_special(p);
            }
          }
          else {
            if (pkey(p, PK_UP) != 0) {
              g_player[p].vy--;
            }
            if (pkey(p, PK_DOWN) != 0) {
              g_player[p].vy++;
            }
            if (pkey(p, PK_LEFT) != 0) {
              g_player[p].vx--;
            }
            if (pkey(p, PK_RIGHT) != 0) {
              g_player[p].vx++;
            }
            if ((0 < g_player[p].vx) && (pkey(p, PK_RIGHT) == 0)) {
              g_player[p].vx--;
            }
            if ((g_player[p].vx < 0) && (pkey(p, PK_LEFT) == 0)) {
              g_player[p].vx++;
            }
            if ((0 < g_player[p].vy) && (pkey(p, PK_DOWN) == 0)) {
              g_player[p].vy--;
            }
            if ((g_player[p].vy < 0) && (pkey(p, PK_UP) == 0)) {
              g_player[p].vy++;
            }
            if (((pkey(p, PK_SELECT) != 0) && (-1 < g_player[p].powerup)) &&
               (g_player[p].select_held == 0)) {
              buy_powerup(p);
            }
            if (pkey(p, PK_SELECT) == 0) {
              g_player[p].select_held = 0;
            }
            if ((pkey(p, PK_FIRE) == 0) && (g_player[p].charge != 0)) {
              player_fire(p);
            }
            if (pkey(p, PK_FIRE) == 0) {
              g_player[p].fire_wait = 0;
            }
            if ((pkey(p, PK_FIRE) != 0) && (g_player[p].buildup != 0)) {
              g_player[p].charge_tick = (unsigned int)g_player[p].buildup + g_player[p].charge_tick;
            }
            if ((pkey(p, PK_FIRE) != 0) && (g_player[p].fire_wait == 0)) {
              player_fire(p);
            }
            if (((pkey(p, PK_SPECIAL) != 0) && (g_player[p].special_time == 0)) &&
               (g_player[p].specials != 0)) {
              use_special(p);
            }
          }
          uVar5 = g_player[p].vx;
          if ((int)(unsigned int)g_player[p].speed < g_player[p].vx) {
            uVar5 = (unsigned int)g_player[p].speed;
          }
          if ((int)uVar5 < (int)-(unsigned int)g_player[p].speed) {
            uVar5 = -(unsigned int)g_player[p].speed;
          }
          g_player[p].vx = uVar5;
          uVar5 = g_player[p].vy;
          if ((int)(unsigned int)g_player[p].speed < g_player[p].vy) {
            uVar5 = (unsigned int)g_player[p].speed;
          }
          if ((int)uVar5 < (int)-(unsigned int)g_player[p].speed) {
            uVar5 = -(unsigned int)g_player[p].speed;
          }
          g_player[p].vy = uVar5;
          local_3c = g_player[p].x + g_player[p].vx;
          if (local_3c < min_x) {
            local_3c = min_x;
          }
          if (max_x < local_3c) {
            local_3c = max_x;
          }
          g_player[p].x = local_3c;
          local_3c = g_player[p].y + g_player[p].vy;
          if (local_3c < min_y) {
            local_3c = min_y;
          }
          if (max_y < local_3c) {
            local_3c = max_y;
          }
          g_player[p].y = local_3c;
          for (local_4 = 0; (int)local_4 < (int)(unsigned int)g_player[p].option; local_4++)
          {
            g_player[p].orbit[local_4].angle += '\x04'
            ;
          }
          if (0x14 < g_player[p].charge_tick) {
            iVar6 = g_player[p].charge + 1;
            if (g_player[p].charge_max < iVar6) {
              iVar6 = g_player[p].charge_max;
            }
            g_player[p].charge = iVar6;
            g_player[p].charge_tick = 10;
          }
          if ((g_player[p].fire_wait != 0) && (g_player[p].buildup == 0)) {
            g_player[p].fire_wait--;
          }
        }
      }
      do {
        iVar4 = g_tick;
        if (iVar4 != 0) break;
        wait_tick();
        cVar3 = key[0xf];
      } while (cVar3 == 0);
      cVar3 = key[0x54];
      if (cVar3 != 0) {
        save_screenshot();
      }
      cVar3 = key[0x7b];
      if (cVar3 != 0) {
        pause_game();
      }
      cVar3 = key[0xf];
      if ((cVar3 != 0) && (cVar3 = key[0x1c], cVar3 != 0)) {
        console();
      }
      cVar3 = key[0x4e];
      if (((cVar3 != 0) && (opt_cheats != 0)) && (cheat_latch == 0)) {
        g_player[0].powerup++;
        if (8 < g_player[0].powerup) {
          g_player[0].powerup = 8;
        }
        g_player[0].bar_dirty = 1;
      }
      cVar3 = key[0x4a];
      if (((cVar3 != 0) && (opt_cheats != 0)) && (cheat_latch == 0)) {
        g_player[0].powerup--;
        if (g_player[0].powerup < 0) {
          g_player[0].powerup = 0;
        }
        g_player[0].bar_dirty = 1;
      }
      cVar3 = key[0x4e];
      if (((cVar3 != 0) && (opt_cheats != 0)) && (cheat_latch == 0)) {
        g_player[1].powerup++;
        if (8 < g_player[1].powerup) {
          g_player[1].powerup = 8;
        }
        g_player[1].bar_dirty = 1;
      }
      cVar3 = key[0x4a];
      if (((cVar3 != 0) && (opt_cheats != 0)) && (cheat_latch == 0)) {
        g_player[1].powerup--;
        if (g_player[1].powerup < 0) {
          g_player[1].powerup = 0;
        }
        g_player[1].bar_dirty = 1;
      }
      cVar3 = key[0x3b];
      if ((cVar3 != 0) && (opt_cheats != 0)) {
        g_player[0].lives++;
        g_player[1].lives++;
      }
      cVar3 = key[0x3c];
      if ((cVar3 != 0) && (opt_cheats != 0)) {
        g_player[0].specials++;
        g_player[1].specials++;
      }
      cVar3 = key[0x42];
      if ((cVar3 != 0) && (opt_cheats != 0)) {
        level_done = 1;
      }
      cVar3 = key[0x40];
      if (((cVar3 != 0) && (cVar3 = key[0x41], cVar3 != 0)) && (opt_cheats != 0)) {
        level_done = 1;
        g_level = 6;
      }
      cVar3 = key[0x4a];
      if ((cVar3 != 0) || (cVar3 = key[0x4e], cVar3 != 0)) {
        cheat_latch = 1;
      }
      cVar3 = key[0x4e];
      if ((cVar3 == 0) && (cVar3 = key[0x4a], cVar3 == 0)) {
        cheat_latch = 0;
      }
      if (g_player[0].bar_dirty != 0) {
        draw_status_bar(0);
      }
      if (g_player[1].bar_dirty != 0) {
        draw_status_bar(1);
      }
      cVar3 = key[0x3e];
      if ((cVar3 != 0) && (opt_cheats != 0)) {
        dbg_enemyinfo = 1;
      }
      cVar3 = key[0x3e];
      if ((cVar3 != 0) && (cVar3 = key[0xf], cVar3 != 0)) {
        dbg_enemyinfo = 0;
      }
      cVar3 = key[0x3f];
      if ((cVar3 != 0) && (opt_cheats != 0)) {
        player_mortal = 0;
      }
      cVar3 = key[0x3f];
      if (((cVar3 != 0) && (cVar3 = key[0xf], cVar3 != 0)) && (opt_cheats != 0)) {
        player_mortal = 1;
      }
      cVar3 = key[0x43];
      if (cVar3 != 0) {
        opt_vsync = 1;
      }
      cVar3 = key[0x43];
      if ((cVar3 != 0) && (cVar3 = key[0xf], cVar3 != 0)) {
        opt_vsync = 0;
      }
      cVar3 = key[0x44];
      if ((cVar3 != 0) && (opt_cheats != 0)) {
        dbg_fps = 1;
      }
      cVar3 = key[0x44];
      if ((cVar3 != 0) && (cVar3 = key[0xf], cVar3 != 0)) {
        dbg_fps = 0;
      }
      cVar3 = key[1];
      if (cVar3 != 0) {
        local_28 = dialog("Are you sure you want to quit now?\x94""\x94""      NO! (esc)    YES! (enter)",dat_shbgs[BG_HEAD_G].dat,1,0);
      }
    }
    if ((level_done != 0) && (g_level != 6)) {
      pilot_comments(g_level);
    }
    fade_out(4);
    if (snd_on != 0) {
      stop_sample(dat_shsnd[SN_SND_ENGINE].dat);
    }
    if (music_on != 0) {
      play_midi((MIDI *)0x0,0);
      music_playing = 0;
    }
    if (((level_done != 0) && (level_end_tally(), g_level == 3)) && (difficulty < 3)) {
      g_level = 6;
    }
    remove_missiles();
    if ((level_done == 0) || (g_level == 6)) {
      if (((g_player[0].dead == 0) || (g_player[1].dead == 0)) &&
         (((local_28 == 0 && (g_level == 6)) && (2 < difficulty)))) {
        ending();
        credits();
      }
      if (local_28 == 0) {
        game_over(difficulty);
      }
      if (music_on != 0) {
        play_midi(dat_shsnd[SN_MIDI_TITLE].dat,-1);
        music_playing = 1;
      }
      return;
    }
  } while( true );
}
