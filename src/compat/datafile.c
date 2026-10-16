/* Allegro 3.11 datafile loader (packfiles with optional password-XORed LZSS). */
#include "alleg.h"
#include "../platform/sys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define F_PACK_MAGIC   0x736C6821u
#define F_NOPACK_MAGIC 0x736C682Eu
#define DAT_MAGIC      DAT_ID('A','L','L','.')
#define DAT_PROPERTY   DAT_ID('p','r','o','p')
#define LZ_N 4096
#define LZ_F 18
#define LZ_THRESHOLD 2

static char the_password[256];

void packfile_password(const char *password)
{
   if (password) {
      strncpy(the_password, password, 255);
      the_password[255] = 0;
   }
   else
      the_password[0] = 0;
}

static uint32_t encrypt_id(uint32_t x)
{
   uint32_t mask = 0;
   int i;
   for (i = 0; the_password[i]; i++)
      mask ^= ((uint32_t)(unsigned char)the_password[i] << ((i & 3) * 8));
   return x ^ mask;
}

/* Allegro pack_read(): LZSS, flag bytes XORed with the cycling password. */
static uint8_t *lzss_unpack(const uint8_t *src, size_t n, size_t *out_len)
{
   size_t cap = n * 4 + 1024, len = 0, p = 0;
   uint8_t *out = malloc(cap);
   uint8_t text[LZ_N];
   unsigned flags = 0;
   int r = LZ_N - LZ_F, pw = 0;
   memset(text, 0, sizeof(text));
   for (;;) {
      int c;
      if (((flags >>= 1) & 256) == 0) {
         if (p >= n) break;
         c = src[p++];
         if (the_password[0]) {
            c ^= (unsigned char)the_password[pw++];
            if (!the_password[pw]) pw = 0;
         }
         flags = (unsigned)c | 0xFF00;
      }
      if (flags & 1) {
         if (p >= n) break;
         c = src[p++];
         text[r++] = (uint8_t)c;
         r &= (LZ_N - 1);
         if (len + 1 > cap) { cap *= 2; out = realloc(out, cap); }
         out[len++] = (uint8_t)c;
      }
      else {
         int i, j, k;
         if (p + 1 >= n) break;
         i = src[p];
         j = src[p + 1];
         p += 2;
         i |= ((j & 0xF0) << 4);
         j = (j & 0x0F) + LZ_THRESHOLD;
         if (len + (size_t)j + 1 > cap) { cap = cap * 2 + (size_t)j; out = realloc(out, cap); }
         for (k = 0; k <= j; k++) {
            c = text[(i + k) & (LZ_N - 1)];
            text[r++] = (uint8_t)c;
            r &= (LZ_N - 1);
            out[len++] = (uint8_t)c;
         }
      }
   }
   *out_len = len;
   return out;
}

typedef struct { const uint8_t *b; size_t n, p; } RD;

static uint32_t mgetl(RD *r)
{
   uint32_t v;
   if (r->p + 4 > r->n) { r->p = r->n; return 0; }
   v = ((uint32_t)r->b[r->p] << 24) | ((uint32_t)r->b[r->p + 1] << 16) | ((uint32_t)r->b[r->p + 2] << 8) | r->b[r->p + 3];
   r->p += 4;
   return v;
}

static int mgetw(RD *r)
{
   int v;
   if (r->p + 2 > r->n) { r->p = r->n; return 0; }
   v = (int16_t)((r->b[r->p] << 8) | r->b[r->p + 1]);
   r->p += 2;
   return v;
}

static const uint8_t *take(RD *r, size_t n)
{
   const uint8_t *p = r->b + r->p;
   if (r->p + n > r->n) { r->p = r->n; return NULL; }
   r->p += n;
   return p;
}

static BITMAP *read_bitmap8(RD *r)
{
   int w = mgetw(r), h = mgetw(r), y;
   const uint8_t *px = take(r, (size_t)w * h);
   BITMAP *b = create_bitmap(w, h);
   if (px)
      for (y = 0; y < h; y++)
         memcpy(b->line[y], px + (size_t)y * w, (size_t)w);
   return b;
}

static void *load_object(uint32_t type, const uint8_t *data, size_t size);

static DATAFILE *load_file_object(const uint8_t *data, size_t size)
{
   RD r = { data, size, 0 };
   int count = (int)mgetl(&r), c = 0;
   DATAFILE *dat = calloc((size_t)count + 1, sizeof(DATAFILE));
   char name[32] = "";
   while (c < count && r.p < r.n) {
      uint32_t type = mgetl(&r);
      if (type == DAT_PROPERTY) {
         uint32_t ptype = mgetl(&r);
         uint32_t len = mgetl(&r);
         const uint8_t *pd = take(&r, len);
         if (ptype == DAT_NAME && pd) {
            size_t l = len < sizeof(name) - 1 ? len : sizeof(name) - 1;
            memcpy(name, pd, l);
            name[l] = 0;
         }
         continue;
      }
      {
         int32_t fsize = (int32_t)mgetl(&r);
         int32_t dsize = (int32_t)mgetl(&r);
         const uint8_t *raw = take(&r, (size_t)(fsize < 0 ? -fsize : fsize));
         uint8_t *unpacked = NULL;
         size_t ulen = 0;
         const uint8_t *obj = raw;
         size_t olen = (size_t)(fsize < 0 ? -fsize : fsize);
         if (!raw) break;
         if (dsize < 0) {
            unpacked = lzss_unpack(raw, olen, &ulen);
            obj = unpacked;
            olen = (size_t)-dsize < ulen ? (size_t)-dsize : ulen;
         }
         else if ((size_t)dsize < olen)
            olen = (size_t)dsize;
         dat[c].type = type;
         dat[c].size = (long)olen;
         dat[c].dat = load_object(type, obj, olen);
         strcpy(dat[c].name, name);
         free(unpacked);
         name[0] = 0;
         c++;
      }
   }
   dat[c].type = DAT_END;
   return dat;
}

static void *load_object(uint32_t type, const uint8_t *data, size_t size)
{
   RD r = { data, size, 0 };
   if (type == DAT_FILE)
      return load_file_object(data, size);
   if (type == DAT_BITMAP) {
      int bits = mgetw(&r);
      if (bits != 8) return NULL;
      return read_bitmap8(&r);
   }
   if (type == DAT_RLE_SPRITE) {
      int bits = mgetw(&r);
      RLE_SPRITE *s;
      int w, h, sz;
      if (bits != 8) return NULL;
      w = mgetw(&r);
      h = mgetw(&r);
      sz = (int)mgetl(&r);
      s = calloc(1, sizeof(RLE_SPRITE));
      s->w = w;
      s->h = h;
      s->size = sz;
      s->dat = calloc((size_t)(sz > 0 ? sz : 1), 1);
      {
         const uint8_t *p = take(&r, (size_t)sz);
         if (p)
            memcpy(s->dat, p, (size_t)sz);
      }
      return s;
   }
   if (type == DAT_FONT) {
      FONT *f = calloc(1, sizeof(FONT));
      int height = mgetw(&r), c;
      f->height = height;
      if (height > 0) {
         const uint8_t *p = take(&r, (size_t)FONT_SIZE * height);
         f->fixed_dat = calloc((size_t)FONT_SIZE * height, 1);
         if (p) memcpy(f->fixed_dat, p, (size_t)FONT_SIZE * height);
      }
      else {
         f->height = -1;
         for (c = 0; c < FONT_SIZE; c++) {
            if (r.p >= r.n)
               break;
            f->glyph[c] = read_bitmap8(&r);
         }
         for (; c < FONT_SIZE; c++)
            f->glyph[c] = create_bitmap(8, f->glyph[0] ? f->glyph[0]->h : 8);
      }
      return f;
   }
   if (type == DAT_SAMPLE) {
      SAMPLE *s = calloc(1, sizeof(SAMPLE));
      int bits = mgetw(&r);
      size_t n;
      s->stereo = bits < 0;
      s->bits = bits < 0 ? -bits : bits;
      s->freq = mgetw(&r) & 0xFFFF;
      s->len = mgetl(&r);
      s->priority = 128;
      s->loop_start = 0;
      s->loop_end = s->len;
      n = s->len * (s->stereo ? 2 : 1) * (s->bits == 8 ? 1 : 2);
      s->data = calloc(n ? n : 1, 1);
      if (s->bits == 8) {
         const uint8_t *p = take(&r, n);
         if (p) memcpy(s->data, p, n);
      }
      else {
         size_t i;
         uint16_t *d = s->data;
         for (i = 0; i < n / 2 && r.p + 2 <= r.n; i++, r.p += 2)
            d[i] = (uint16_t)(r.b[r.p] | (r.b[r.p + 1] << 8));
      }
      return s;
   }
   if (type == DAT_MIDI) {
      MIDI *m = calloc(1, sizeof(MIDI));
      int c;
      m->divisions = mgetw(&r);
      for (c = 0; c < 32; c++) {
         int len = (int)mgetl(&r);
         m->track[c].len = len;
         if (len > 0) {
            const uint8_t *p = take(&r, (size_t)len);
            m->track[c].data = malloc((size_t)len);
            if (p) memcpy(m->track[c].data, p, (size_t)len);
         }
      }
      return m;
   }
   /* DATA, PAL and anything else: raw bytes (plus a NUL so text objects are safe strings) */
   {
      char *p = malloc(size + 1);
      memcpy(p, data, size);
      p[size] = 0;
      return p;
   }
}

DATAFILE *load_datafile(const char *filename)
{
   size_t n, len;
   uint8_t *raw = sys_read_file(filename, &n), *data;
   uint32_t magic;
   DATAFILE *dat;
   if (!raw || n < 8) {
      free(raw);
      return NULL;
   }
   magic = ((uint32_t)raw[0] << 24) | ((uint32_t)raw[1] << 16) | ((uint32_t)raw[2] << 8) | raw[3];
   if (magic == encrypt_id(F_PACK_MAGIC))
      data = lzss_unpack(raw + 4, n - 4, &len);
   else if (magic == encrypt_id(F_NOPACK_MAGIC)) {
      len = n - 4;
      data = malloc(len);
      memcpy(data, raw + 4, len);
   }
   else {
      free(raw);
      return NULL;
   }
   free(raw);
   if (len < 4 || (((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | ((uint32_t)data[2] << 8) | data[3]) != DAT_MAGIC) {
      free(data);
      return NULL;
   }
   dat = load_file_object(data + 4, len - 4);
   free(data);
   return dat;
}

static void unload_object(DATAFILE *d)
{
   int i;
   if (!d->dat)
      return;
   switch (d->type) {
      case DAT_FILE: unload_datafile(d->dat); return;
      case DAT_BITMAP: destroy_bitmap(d->dat); return;
      case DAT_RLE_SPRITE: destroy_rle_sprite(d->dat); return;
      case DAT_FONT: {
         FONT *f = d->dat;
         for (i = 0; i < FONT_SIZE; i++)
            destroy_bitmap(f->glyph[i]);
         free(f->fixed_dat);
         free(f);
         return;
      }
      case DAT_SAMPLE: free(((SAMPLE *)d->dat)->data); free(d->dat); return;
      case DAT_MIDI: destroy_midi(d->dat); return;
      default: free(d->dat); return;
   }
}

void unload_datafile(DATAFILE *dat)
{
   int i;
   if (!dat)
      return;
   for (i = 0; dat[i].type != DAT_END; i++)
      unload_object(&dat[i]);
   free(dat);
}

DATAFILE *find_datafile_object(DATAFILE *dat, const char *name)
{
   int i;
   for (i = 0; dat[i].type != DAT_END; i++)
      if (strcmp(dat[i].name, name) == 0)
         return &dat[i];
   return NULL;
}
