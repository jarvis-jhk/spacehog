/* Sync log writer, see synclog.h and re/refemu/README.md for the format. */
#include "sh.h"
#include "synclog.h"
#include "../compat/internal.h"
#include "../platform/sys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define MAX_SECTIONS 32

static FILE *sf;
static long rec_no, dump_from = -1, dump_to = -1;
static uint32_t cur_hash, hashes[MAX_SECTIONS];
static unsigned char *dbuf;
static size_t dlen, dcap;
static int capturing, capturing_frame;
static unsigned char frame[320 * 240 + 768];   /* screen + palette for dumps */
static size_t frame_len;

/* ---- game-level drawing/sound call log (calllog.h) */
int sl_calls_on;
static uint32_t calls_crc = 0xffffffffu;
static char *ctext;                 /* text of the calls since the last record (dumps) */
static size_t ctext_len, ctext_cap;
static const void *created[4096];
static int ncreated;

/* CRC-32 (zlib polynomial), so the Python side can use zlib.crc32 */
static uint32_t crc_table[256];

static void crc_init(void)
{
   uint32_t c;
   int i, k;
   for (i = 0; i < 256; i++) {
      c = (uint32_t)i;
      for (k = 0; k < 8; k++)
         c = c & 1 ? 0xedb88320u ^ (c >> 1) : c >> 1;
      crc_table[i] = c;
   }
}

void sync_w_raw(const void *p, int n)
{
   const unsigned char *b = p;
   int i;
   for (i = 0; i < n; i++)
      cur_hash = crc_table[(cur_hash ^ b[i]) & 0xff] ^ (cur_hash >> 8);
   if (capturing) {
      if (dlen + (size_t)n > dcap) {
         dcap = (dlen + (size_t)n) * 2;
         dbuf = realloc(dbuf, dcap);
      }
      memcpy(dbuf + dlen, b, (size_t)n);
      dlen += (size_t)n;
   }
}

/* pointer -> (datafile object id, offset) */
void sync_w_ptr(const void *p)
{
   DATAFILE *files[4] = { dat_spacehog, dat_shlev, dat_shbgs, dat_shsnd };
   uint32_t v[2] = { 0, 0 };
   int f, k;
   if (p) {
      v[0] = 0xffffffffu;
      for (f = 0; f < 4 && v[0] == 0xffffffffu; f++)
         for (k = 0; files[f] && files[f][k].type != DAT_END; k++) {
            const char *d = files[f][k].dat;
            if ((const char *)p >= d && (const char *)p <= d + files[f][k].size) {
               v[0] = (uint32_t)(((f + 1) << 16) | (k + 1));
               v[1] = (uint32_t)((const char *)p - d);
               break;
            }
         }
   }
   sync_w_raw(v, 8);
}

void sync_w_rng(void)
{
   uint32_t st[31], idx[2];
   int fp, rp;
   dos_rand_state(st, &fp, &rp);
   idx[0] = (uint32_t)fp;
   idx[1] = (uint32_t)rp;
   sync_w_raw(st, sizeof(st));
   sync_w_raw(idx, sizeof(idx));
}

/* CRC of the complete game state (netplay desync check); independent of the sync log */
uint32_t sync_state_hash(void)
{
   uint32_t saved = cur_hash, h;
   int i, cap = capturing;
   if (!crc_table[1])
      crc_init();
   capturing = 0;
   cur_hash = 0xffffffffu;
   for (i = 0; i < sync_num_sections; i++)
      sync_state_section(i);
   h = ~cur_hash;
   cur_hash = saved;
   capturing = cap;
   return h;
}

static uint32_t crc_buf(uint32_t c, const void *p, size_t n)
{
   const unsigned char *b = p;
   while (n--)
      c = crc_table[(c ^ *b++) & 0xff] ^ (c >> 8);
   return c;
}

static void ref_name(const void *p, char *out, size_t sz)
{
   DATAFILE *files[4] = { dat_spacehog, dat_shlev, dat_shbgs, dat_shsnd };
   int f, k;
   if (!p) { snprintf(out, sz, "#0"); return; }
   if (p == screen) { snprintf(out, sz, "#scr"); return; }
   for (k = ncreated - 1; k >= 0; k--)       /* latest wins: memory gets reused */
      if (created[k] == p) { snprintf(out, sz, "#c%d", k); return; }
   for (f = 0; f < 4; f++)
      for (k = 0; files[f] && files[f][k].type != DAT_END; k++)
         if (files[f][k].dat == p) { snprintf(out, sz, "#d%d.%d", f + 1, k); return; }
   snprintf(out, sz, "#?");
}

static void calls_add(const char *line)
{
   size_t n = strlen(line);
   calls_crc = crc_buf(calls_crc, line, n);
   calls_crc = crc_buf(calls_crc, "\n", 1);
   if (rec_no >= dump_from && rec_no <= dump_to) {
      if (ctext_len + n + 2 > ctext_cap) {
         ctext_cap = (ctext_len + n + 2) * 2;
         ctext = realloc(ctext, ctext_cap);
      }
      memcpy(ctext + ctext_len, line, n);
      ctext[ctext_len + n] = '\n';
      ctext_len += n + 1;
   }
}

void sl_call(const char *name, const char *spec, ...)
{
   char line[512], tmp[64];
   size_t len;
   va_list ap;
   va_start(ap, spec);
   len = (size_t)snprintf(line, sizeof(line), "%s", name);
   for (; *spec; spec++) {
      if (*spec == 'R')
         ref_name(va_arg(ap, const void *), tmp, sizeof(tmp));
      else if (*spec == 'i')
         snprintf(tmp, sizeof(tmp), "%d", va_arg(ap, int));
      else if (*spec == 'P') {
         const RGB *pal = va_arg(ap, const RGB *);
         uint32_t c = 0xffffffffu;
         int i;
         for (i = 0; i < 256; i++) {
            unsigned char rgb[3] = { pal[i].r, pal[i].g, pal[i].b };
            c = crc_buf(c, rgb, 3);
         }
         snprintf(tmp, sizeof(tmp), "pal:%08x", ~c);
      }
      else if (*spec == 's') {
         len += (size_t)snprintf(line + len, sizeof(line) - len, " \"%s\"", va_arg(ap, const char *));
         continue;
      }
      len += (size_t)snprintf(line + len, sizeof(line) - len, " %s", tmp);
      if (len >= sizeof(line))
         len = sizeof(line) - 1;
   }
   va_end(ap);
   calls_add(line);
}

void sl_textprintf(int centre, BITMAP *bmp, FONT *f, int x, int y, int color, const char *fmt, ...)
{
   char buf[512];
   va_list ap;
   va_start(ap, fmt);
   vsnprintf(buf, sizeof(buf), fmt, ap);
   va_end(ap);
   if (centre) {
      if (sl_calls_on)
         sl_call("textout_centre", "RRsiii", bmp, f, buf, x, y, color);
      (textout_centre)(bmp, f, buf, x, y, color);
   }
   else {
      if (sl_calls_on)
         sl_call("textout", "RRsiii", bmp, f, buf, x, y, color);
      (textout)(bmp, f, buf, x, y, color);
   }
}

BITMAP *sl_created(BITMAP *b)
{
   if (sl_calls_on && ncreated < 4096)
      created[ncreated++] = b;
   return b;
}

RLE_SPRITE *sl_created_rle(RLE_SPRITE *r)
{
   if (sl_calls_on && ncreated < 4096)
      created[ncreated++] = r;
   return r;
}

static void w32(int32_t v)
{
   fwrite(&v, 4, 1, sf);
}

static void hook(int leave, int kind, int ret)
{
   int i, kb[64], nkb;
   unsigned char keys[128];
   if (!leave) {
      capturing = rec_no >= dump_from && rec_no <= dump_to;
      capturing_frame = capturing;
      dlen = 0;
      for (i = 0; i < sync_num_sections; i++) {
         cur_hash = 0xffffffffu;
         sync_state_section(i);
         hashes[i] = ~cur_hash;
      }
      hashes[sync_num_sections] = ~calls_crc;
      calls_crc = 0xffffffffu;
      /* screen pixels and palette (r, g, b) */
      {
         uint32_t c = 0xffffffffu;
         int y;
         if (screen)
            for (y = 0; y < screen->h; y++)
               c = crc_buf(c, screen->line[y], (size_t)screen->w);
         hashes[sync_num_sections + 1] = screen ? ~c : 0;
         c = 0xffffffffu;
         for (y = 0; y < 256; y++) {
            unsigned char rgb[3] = { _current_pallete[y].r, _current_pallete[y].g, _current_pallete[y].b };
            c = crc_buf(c, rgb, 3);
         }
         hashes[sync_num_sections + 2] = ~c;
         if (capturing_frame) {
            frame_len = 0;
            if (screen)
               for (y = 0; y < screen->h && frame_len + (size_t)screen->w <= sizeof(frame); y++) {
                  memcpy(frame + frame_len, screen->line[y], (size_t)screen->w);
                  frame_len += (size_t)screen->w;
               }
            for (y = 0; y < 256; y++) {
               frame[frame_len++] = _current_pallete[y].r;
               frame[frame_len++] = _current_pallete[y].g;
               frame[frame_len++] = _current_pallete[y].b;
            }
         }
      }
      capturing = 0;
      return;
   }
   fputc(kind, sf);
   fputc(rec_no >= dump_from && rec_no <= dump_to, sf);
   fputc(sync_num_sections + 3, sf);
   fputc(0, sf);
   w32(ret);
   fwrite(hashes, 4, (size_t)sync_num_sections + 3, sf);
   for (i = 0; i < 128; i++)
      keys[i] = (unsigned char)key[i];
   fwrite(keys, 1, 128, sf);
   nkb = _input_keybuf(kb, 64);
   fputc(nkb, sf);
   fwrite(kb, 4, (size_t)nkb, sf);
   w32(_input_pad_bits(0));
   w32(num_joysticks);
   w32(mouse_x);
   w32(mouse_y);
   w32(mouse_b);
   w32(g_tick);
   w32(g_fps);
   w32(g_frames);
   if (rec_no >= dump_from && rec_no <= dump_to) {
      w32((int32_t)dlen);
      fwrite(dbuf, 1, dlen, sf);
      w32((int32_t)ctext_len);
      fwrite(ctext, 1, ctext_len, sf);
      ctext_len = 0;
      w32((int32_t)frame_len);
      fwrite(frame, 1, frame_len, sf);
   }
   rec_no++;
}

int synclog_open(const char *path, const char *dump_range)
{
   sf = fopen(path, "wb");
   if (!sf)
      return -1;
   setvbuf(sf, NULL, _IOFBF, 1 << 20);
   fwrite("SHSYNC1\n", 1, 8, sf);
   if (dump_range) {
      char *e;
      dump_from = strtol(dump_range, &e, 10);
      dump_to = *e == '-' ? strtol(e + 1, NULL, 10) : dump_from;
   }
   crc_init();
   sl_calls_on = 1;
   sys_sync_hook = hook;
   return 0;
}
