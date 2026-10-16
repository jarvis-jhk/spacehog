/* Level script: event dispatch and spawners (0x1c688, 0x1c220, 0x2a0c, 0x2fe4, 0x3ae8, 0x164a4).
 * The DOS code passes the 64-byte event record by value; here it is passed by pointer. */
#include "sh.h"
#include "play.h"

static int free_enemy_slot(void)
{
   int i;
   for (i = 0; i < 50 && g_enemy[i].active; i++)
      ;
   return i;
}

/* common part of the enemy spawners: copy the template and size from its sprite */
static void init_from_template(int i, int tmpl)
{
   int *spr;
   g_enemy[i] = g_enemy_tmpl[tmpl];
   g_enemy[i].tmpl = (unsigned char)tmpl;
   spr = dat_spacehog[g_enemy[i].sprite].dat;   /* BITMAP / RLE_SPRITE: w, h first */
   g_enemy[i].hw = spr[0] >> 1;
   g_enemy[i].hh = spr[1] >> 1;
   g_enemy[i].w = spr[0];
   g_enemy[i].h = spr[1];
}

/* 0x2a0c: enemy moving on a circle around a drifting centre (event kind 2) */
static unsigned char spawn_orbiter(const LEVELEVENT *ev)
{
   int i = free_enemy_slot();
   if (i < 50) {
      ENEMY *e = &g_enemy[i];
      init_from_template(i, ev->tmpl);
      e->angle = (unsigned char)ev->p28;
      e->spin = ev->p2c;
      e->radius = ev->p24;
      e->x = e->hw + 320 + ev->x + (g_cos128[e->angle] >> (e->radius & 0x1f));
      e->y = (g_sin128[e->angle] >> (e->radius & 0x1f)) + ev->y;
      e->vx = ev->vx;
      e->vy = ev->vy;
      e->move = ev->kind;
      e->active = 1;
      e->hp = e->hp * difficulty;
      e->points = e->points * difficulty >> 1;
      e->targeted = 0;
      e->hit_flash = 0;
      e->drop = ev->flags;
      e->path_pos = -1;
      e->parent = 0xff;
   }
   return (unsigned char)i;
}

/* 0x2fe4: single enemy (event kinds 0, 3, 4, 6); parent = last enemy that started a group */
static unsigned char spawn_enemy(const LEVELEVENT *ev, unsigned char parent)
{
   int i = free_enemy_slot();
   if (i < 50) {
      ENEMY *e = &g_enemy[i];
      init_from_template(i, ev->tmpl);
      e->rel_x = ev->relx;
      e->rel_y = ev->rely;
      if (ev->attach == -2) {
         e->parent = parent;
         e->x = g_enemy[(signed char)e->parent].x + e->rel_x;
         e->y = g_enemy[(signed char)e->parent].y + e->rel_y;
      }
      else {
         e->parent = 0xff;
         e->x = ev->x + e->hw + 320;
         e->y = ev->y;
         e->vx = ev->vx;
         e->vy = ev->vy;
      }
      e->move = ev->kind;
      e->active = 1;
      e->hp = e->hp * difficulty;
      e->points = e->points * difficulty >> 1;
      e->targeted = 0;
      e->hit_flash = 0;
      e->fire_count = rand() % (e->fire_rate + 1);
      e->drop = ev->flags;
      e->path_pos = -1;
      if (e->move == 3) {
         e->rel_y = e->rel_y * 25;
         e->radius = ev->p24;
         e->spin = 0;
      }
      if (e->move == 6) {
         e->path_pos = 0;
         e->rel_y = ev->y;
         e->path = dat_shlev[ev->path].dat;
      }
   }
   return (unsigned char)i;
}

/* 0x3ae8: enemy following a wave path */
unsigned char spawn_path_enemy(int x, int y, int tmpl, unsigned char flags)
{
   int i = free_enemy_slot();
   if (i < 50) {
      ENEMY *e = &g_enemy[i];
      init_from_template(i, tmpl);
      e->x = x + e->hw;
      e->y = y;
      e->active = 1;
      e->hp = e->hp * difficulty;
      e->points = e->points * difficulty >> 1;
      e->fire_count = rand() % (e->fire_rate + 1);
      e->targeted = 0;
      e->hit_flash = 0;
      e->drop = flags;
      e->path_pos = -1;
      e->parent = 0xff;
   }
   return (unsigned char)i;
}

/* 0x164a4: background decoration (event kind 5) */
static void spawn_bgobj(const LEVELEVENT *ev)
{
   int i;
   int *spr;
   for (i = 0; i < 10 && g_bgobj[i].active; i++)
      ;
   if (i < 10) {
      g_bgobj[i].active = 1;
      g_bgobj[i].sprite = ev->tmpl;
      spr = dat_shbgs[g_bgobj[i].sprite].dat;
      g_bgobj[i].hw = spr[0] >> 1;
      g_bgobj[i].hh = spr[1] >> 1;
      g_bgobj[i].w = spr[0];
      g_bgobj[i].h = spr[1];
      g_bgobj[i].x = ev->x + g_bgobj[i].hw + 320;
      g_bgobj[i].y = ev->y;
      g_bgobj[i].layer = (unsigned char)ev->vy;
   }
}

/* 0x1c220: start a wave of enemies following a path */
static void start_wave(unsigned char tmpl, int path, unsigned char count, unsigned char step,
                       unsigned char interval, unsigned char yofs, signed char bonus)
{
   int i;
   for (i = 0; i < 10 && g_wave[i].active; i++)
      ;
   if (i != 10) {
      g_wave[i].path = dat_shlev[path + 10].dat;
      g_wave[i].tmpl = tmpl;
      g_wave[i].active = 1;
      g_wave[i].timer = 0;
      g_wave[i].step = step;
      g_wave[i].yofs = yofs;
      g_wave[i].bonus = (unsigned char)bonus;
      g_wave[i].alive = count;
      g_wave[i].remaining = count;
      g_wave[i].interval = interval;
   }
}

/* 0x1c688: fire all events scheduled for the current level step (level_step) */
void run_events(void)
{
   int i = next_event;
   unsigned char spawned = 0, parent = 0;
   while (g_event[i].time == level_step) {
      LEVELEVENT *ev = &g_event[i];
      switch (ev->kind) {
         case 0:
         case 3:
         case 4:
         case 6:
            spawned = spawn_enemy(ev, parent);
            if (ev->attach == 0)
               parent = spawned;
            break;
         case 1:
            start_wave(ev->tmpl, ev->path, (unsigned char)ev->count, (unsigned char)ev->vx,
                       (unsigned char)ev->interval, (unsigned char)ev->y, (signed char)ev->flags);
            break;
         case 2:
            spawned = spawn_orbiter(ev);
            break;
         case 5:
            spawn_bgobj(ev);
            break;
         case 9:
            level_done = 1;
            break;
         default:
            break;
      }
      if (ev->flags == 'c') {
         int pips;
         bosses_alive++;
         boss_enemy = spawned;
         boss_hp = g_enemy[spawned].hp;
         boss_pips = 0;
         pips = boss_hp / 30;
         if (pips < 1)
            pips = 1;
         boss_pip_hp = pips;
         if (music_on) {
            play_midi(MUS(SN_MIDI_BOSS), TRUE);
            music_playing = 1;
         }
      }
      i++;
      next_event++;
   }
}

/* 0x1740: load a level (.lev = 0x54 byte header + events). The DOS version wrote the
 * embedded level to a swap file "level.$$$" and read it back; here it is read from memory. */
_Static_assert(sizeof(LEVELEVENT) == 64 && sizeof(LEVELHDR) == 84, "level structs must match the file format");

int level_load_mem(const unsigned char *buf, size_t size)
{
   size_t n;
   if (size < sizeof(LEVELHDR))
      return 0;
   memcpy(&g_levelhdr, buf, sizeof(LEVELHDR));
   n = (size_t)g_levelhdr.last_event + 1;
   if (n > 256)
      n = 256;
   if (sizeof(LEVELHDR) + n * sizeof(LEVELEVENT) > size)
      n = (size - sizeof(LEVELHDR)) / sizeof(LEVELEVENT);
   memcpy(g_event, buf + sizeof(LEVELHDR), n * sizeof(LEVELEVENT));
   return 1;
}

int level_load_file(const char *name)
{
   FILE *f = sys_fopen_save(name, "rb");
   unsigned char *buf;
   long n;
   int ok;
   if (!f) {
      size_t sz;
      buf = sys_read_file(name, &sz);   /* also look next to the data files */
      if (!buf)
         return 0;
      ok = level_load_mem(buf, sz);
      free(buf);
      return ok;
   }
   fseek(f, 0, SEEK_END);
   n = ftell(f);
   fseek(f, 0, SEEK_SET);
   buf = malloc((size_t)(n > 0 ? n : 1));
   n = (long)fread(buf, 1, (size_t)n, f);
   fclose(f);
   ok = level_load_mem(buf, (size_t)n);
   free(buf);
   return ok;
}
