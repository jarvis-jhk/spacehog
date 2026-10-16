/* Allegro init/shutdown, graphics mode, PCX export, DJGPP libc bits. */
#include "alleg.h"
#include "internal.h"
#include "dos.h"
#include "../platform/sys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

BITMAP *screen;
static FONT default_font;
FONT *font = &default_font;
extern const uint8_t font8x8_data[224][8];

int allegro_init(void)
{
   default_font.height = 8;
   default_font.fixed_dat = (uint8_t *)font8x8_data;
   return 0;
}

void allegro_exit(void)
{
   remove_sound();
}

int set_gfx_mode(int card, int w, int h, int v_w, int v_h)
{
   (void)v_w;
   (void)v_h;
   if (card == GFX_TEXT) {
      sys_set_textmode(1);
      return 0;
   }
   if (!screen || screen->w != w || screen->h != h) {
      destroy_bitmap(screen);
      screen = create_bitmap(w, h);
   }
   clear(screen);
   sys_set_textmode(0);
   return 0;
}

/* ---------------------------------------------------------------- PCX */

int save_pcx(const char *filename, BITMAP *bmp, const RGB *pal)
{
   FILE *f = sys_fopen_save(filename, "wb");
   uint8_t hdr[128];
   int x, y, i;
   if (!f)
      return -1;
   memset(hdr, 0, sizeof(hdr));
   hdr[0] = 10; hdr[1] = 5; hdr[2] = 1; hdr[3] = 8;
   hdr[8] = (uint8_t)((bmp->w - 1) & 0xFF); hdr[9] = (uint8_t)((bmp->w - 1) >> 8);
   hdr[10] = (uint8_t)((bmp->h - 1) & 0xFF); hdr[11] = (uint8_t)((bmp->h - 1) >> 8);
   hdr[12] = 200; hdr[14] = 200;
   hdr[65] = 1;
   hdr[66] = (uint8_t)(bmp->w & 0xFF); hdr[67] = (uint8_t)(bmp->w >> 8);
   hdr[68] = 1;
   fwrite(hdr, 1, 128, f);
   for (y = 0; y < bmp->h; y++) {
      x = 0;
      while (x < bmp->w) {
         uint8_t c = bmp->line[y][x];
         int run = 1;
         while (x + run < bmp->w && run < 63 && bmp->line[y][x + run] == c)
            run++;
         if (run > 1 || c >= 0xC0)
            fputc(0xC0 | run, f);
         fputc(c, f);
         x += run;
      }
   }
   fputc(12, f);
   for (i = 0; i < 256; i++) {
      fputc(((pal[i].r & 63) * 255 + 31) / 63, f);
      fputc(((pal[i].g & 63) * 255 + 31) / 63, f);
      fputc(((pal[i].b & 63) * 255 + 31) / 63, f);
   }
   fclose(f);
   return 0;
}

/* ---------------------------------------------------------------- test screenshots */

static int shot_counter;
static long shot_every;
static unsigned long long next_shot;

/* writes the current screen + palette as binary PPM (lossless, no deps) */
static void write_ppm(const char *path)
{
   FILE *f = fopen(path, "wb");
   int x, y;
   if (!f || !screen)
      return;
   fprintf(f, "P6\n%d %d\n255\n", screen->w, screen->h);
   for (y = 0; y < screen->h; y++)
      for (x = 0; x < screen->w; x++) {
         const RGB *c = &_current_pallete[screen->line[y][x]];
         fputc(((c->r & 63) * 255 + 31) / 63, f);
         fputc(((c->g & 63) * 255 + 31) / 63, f);
         fputc(((c->b & 63) * 255 + 31) / 63, f);
      }
   fclose(f);
}

void sh_screenshot(const char *tag)
{
   char path[1024];
   if (!sys_cfg.shot_dir || shot_counter >= 5000)  /* hard cap: never fill the disk */
      return;
   snprintf(path, sizeof(path), "%s/%s_%04d_%llu.ppm", sys_cfg.shot_dir, tag ? tag : "shot", shot_counter++,
            (unsigned long long)sys_clock());
   write_ppm(path);
}

/* SPACEHOG_FRAME_HASHES=file: log an FNV-1a hash of every presented RGB frame (for
 * frame-exact comparison with DOSBox video captures). SPACEHOG_DUMP_FRAMES=dir: save them. */
static void frame_hash_hook(void)
{
   static FILE *hf;
   static int init, dumped;
   static const char *dumpdir;
   static uint32_t last;
   uint32_t h = 2166136261u;
   int x, y;
   if (!init) {
      const char *p = getenv("SPACEHOG_FRAME_HASHES");
      init = 1;
      if (p) hf = fopen(p, "w");
      dumpdir = getenv("SPACEHOG_DUMP_FRAMES");
   }
   if (!hf || !screen)
      return;
   for (y = 0; y < screen->h; y++)
      for (x = 0; x < screen->w; x++) {
         const RGB *c = &_current_pallete[screen->line[y][x]];
         h = (h ^ (uint32_t)(((c->r & 63) * 255 + 31) / 63)) * 16777619u;
         h = (h ^ (uint32_t)(((c->g & 63) * 255 + 31) / 63)) * 16777619u;
         h = (h ^ (uint32_t)(((c->b & 63) * 255 + 31) / 63)) * 16777619u;
      }
   if (h == last)
      return;
   last = h;
   fprintf(hf, "%llu %08x\n", (unsigned long long)sys_clock(), h);
   if (dumpdir && dumped < 20000) {
      char path[1024];
      snprintf(path, sizeof(path), "%s/f_%08x.ppm", dumpdir, h);
      {
         FILE *f = fopen(path, "wb");
         if (f) {
            fprintf(f, "P6\n%d %d\n255\n", screen->w, screen->h);
            for (y = 0; y < screen->h; y++)
               for (x = 0; x < screen->w; x++) {
                  const RGB *c = &_current_pallete[screen->line[y][x]];
                  fputc(((c->r & 63) * 255 + 31) / 63, f);
                  fputc(((c->g & 63) * 255 + 31) / 63, f);
                  fputc(((c->b & 63) * 255 + 31) / 63, f);
               }
            fclose(f);
         }
      }
      dumped++;
   }
}

void _screen_capture_hook(void)
{
   frame_hash_hook();
   if (!sys_cfg.shot_dir)
      return;
   if (!shot_every) {
      const char *e = getenv("SPACEHOG_SHOT_EVERY_MS");
      shot_every = e ? atol(e) : -1;
      next_shot = 0;
   }
   if (shot_every > 0 && sys_clock() >= next_shot) {
      sh_screenshot("auto");
      next_shot = sys_clock() + (unsigned long long)MSEC_TO_TIMER(shot_every);
   }
}

/* ---------------------------------------------------------------- DJGPP libc */

/* DJGPP's rand() is BSD random() with TYPE_3 (x**31 + x**3 + 1 additive feedback). */
#define RAND_DEG 31
#define RAND_SEP 3
static uint32_t rand_state[RAND_DEG] = {
   0x9a319039, 0x32d9c024, 0x9b663182, 0x5da1f342, 0xde3b81e0, 0xdf0a6fb5, 0xf103bc02, 0x48f340fb,
   0x7449e56b, 0xbeb1dbb0, 0xab5c5918, 0x946554fd, 0x8c2e680f, 0xeb3d799f, 0xb11ee0b7, 0x2d436b86,
   0xda672e2a, 0x1588ca88, 0xe369735d, 0x904f35f7, 0xd7158fd6, 0x6fa6f051, 0x616e6b96, 0xac94efdc,
   0x36413f93, 0xc622c298, 0xf5a42ab8, 0x8a88d77b, 0xf5ad9d0e, 0x8999220b, 0x27fb47b9
};
static int rand_fp = RAND_SEP, rand_rp = 0;

int dos_rand(void)
{
   uint32_t v;
   rand_state[rand_fp] += rand_state[rand_rp];
   v = rand_state[rand_fp] >> 1;
   if (++rand_fp >= RAND_DEG) {
      rand_fp = 0;
      ++rand_rp;
   }
   else if (++rand_rp >= RAND_DEG)
      rand_rp = 0;
   return (int)v;
}

void dos_rand_state(uint32_t st[31], int *fp, int *rp)
{
   memcpy(st, rand_state, sizeof(rand_state));
   *fp = rand_fp;
   *rp = rand_rp;
}

void dos_srand(unsigned int seed)
{
   int i;
   rand_state[0] = seed;
   for (i = 1; i < RAND_DEG; i++)
      rand_state[i] = rand_state[i - 1] * 1103515245u + 12345u;
   rand_fp = RAND_SEP;
   rand_rp = 0;
   for (i = 0; i < RAND_DEG * 10; i++)
      dos_rand();
}
