/* 8-bit software renderer following Allegro 3.11 semantics.
 * Algorithms for lines, circles, flood fill, rotation, stretching and RLE encoding are
 * transcribed from Allegro 3.11 (giftware); asm-only routines are reimplemented in C with
 * the same clipping and drawing-mode behaviour.
 */
#include "alleg.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>

COLOR_MAP *color_map;
RGB_MAP *rgb_map;

static int _drawing_mode = DRAW_MODE_SOLID;
static BITMAP *_drawing_pattern;
static int _drawing_x_anchor, _drawing_y_anchor;
static unsigned _drawing_x_mask, _drawing_y_mask;
static int _textmode;

/* ------------------------------------------------------------------ bitmaps */

static BITMAP *alloc_bitmap_struct(int w, int h)
{
   BITMAP *b = calloc(1, sizeof(BITMAP));
   if (!b)
      return NULL;
   b->line = calloc(h > 0 ? h : 1, sizeof(uint8_t *));
   b->w = w;
   b->h = h;
   b->clip = TRUE;
   b->cl = b->ct = 0;
   b->cr = w;
   b->cb = h;
   return b;
}

BITMAP *create_bitmap(int w, int h)
{
   BITMAP *b = alloc_bitmap_struct(w, h);
   int y;
   if (!b)
      return NULL;
   b->dat = calloc((size_t)(w > 0 ? w : 1) * (h > 0 ? h : 1), 1);
   b->pitch = w;
   for (y = 0; y < h; y++)
      b->line[y] = b->dat + (size_t)y * w;
   return b;
}

BITMAP *create_sub_bitmap(BITMAP *parent, int x, int y, int w, int h)
{
   BITMAP *b;
   int i;
   if (!parent)
      return NULL;
   if (x < 0) x = 0;
   if (y < 0) y = 0;
   if (x + w > parent->w) w = parent->w - x;
   if (y + h > parent->h) h = parent->h - y;
   b = alloc_bitmap_struct(w, h);
   if (!b)
      return NULL;
   b->parent = parent;
   b->x_ofs = x + parent->x_ofs;
   b->y_ofs = y + parent->y_ofs;
   b->pitch = parent->pitch;
   for (i = 0; i < h; i++)
      b->line[i] = parent->line[y + i] + x;
   return b;
}

void destroy_bitmap(BITMAP *bmp)
{
   if (!bmp)
      return;
   free(bmp->dat);
   free(bmp->line);
   free(bmp);
}

void set_clip(BITMAP *bmp, int x1, int y1, int x2, int y2)
{
   int t;
   if ((x1 == 0) && (y1 == 0) && (x2 == 0) && (y2 == 0)) {
      bmp->clip = FALSE;
      bmp->cl = bmp->ct = 0;
      bmp->cr = bmp->w;
      bmp->cb = bmp->h;
      return;
   }
   if (x2 < x1) { t = x1; x1 = x2; x2 = t; }
   if (y2 < y1) { t = y1; y1 = y2; y2 = t; }
   x2++;
   y2++;
   bmp->clip = TRUE;
   bmp->cl = MID(0, x1, bmp->w - 1);
   bmp->ct = MID(0, y1, bmp->h - 1);
   bmp->cr = MID(0, x2, bmp->w);
   bmp->cb = MID(0, y2, bmp->h);
}

/* ------------------------------------------------------------------ drawing modes */

void drawing_mode(int mode, BITMAP *pattern, int x_anchor, int y_anchor)
{
   _drawing_mode = mode;
   _drawing_pattern = pattern;
   _drawing_x_anchor = x_anchor;
   _drawing_y_anchor = y_anchor;
   if (pattern) {
      _drawing_x_mask = 1;
      while (_drawing_x_mask < (unsigned)pattern->w)
         _drawing_x_mask <<= 1;
      if (_drawing_x_mask > (unsigned)pattern->w)
         _drawing_x_mask >>= 1;
      _drawing_x_mask--;
      _drawing_y_mask = 1;
      while (_drawing_y_mask < (unsigned)pattern->h)
         _drawing_y_mask <<= 1;
      if (_drawing_y_mask > (unsigned)pattern->h)
         _drawing_y_mask >>= 1;
      _drawing_y_mask--;
   }
   else
      _drawing_x_mask = _drawing_y_mask = 0;
}

void solid_mode(void)
{
   drawing_mode(DRAW_MODE_SOLID, NULL, 0, 0);
}

/* write one pixel honouring the drawing mode (no clipping) */
static inline void mode_pixel(BITMAP *bmp, int x, int y, int color)
{
   uint8_t *d = bmp->line[y] + x;
   uint8_t p;
   switch (_drawing_mode) {
      case DRAW_MODE_SOLID:
         *d = (uint8_t)color;
         return;
      case DRAW_MODE_XOR:
         *d ^= (uint8_t)color;
         return;
      case DRAW_MODE_TRANS:
         *d = color_map->data[color & 0xFF][*d];
         return;
      default:
         p = _drawing_pattern->line[(unsigned)(y - _drawing_y_anchor) & _drawing_y_mask]
                                   [(unsigned)(x - _drawing_x_anchor) & _drawing_x_mask];
         if (_drawing_mode == DRAW_MODE_COPY_PATTERN)
            *d = p;
         else if (p)
            *d = (uint8_t)color;
         else if (_drawing_mode == DRAW_MODE_SOLID_PATTERN)
            *d = 0;
         return;
   }
}

void putpixel(BITMAP *bmp, int x, int y, int color)
{
   if (bmp->clip && ((x < bmp->cl) || (x >= bmp->cr) || (y < bmp->ct) || (y >= bmp->cb)))
      return;
   mode_pixel(bmp, x, y, color);
}

int getpixel(BITMAP *bmp, int x, int y)
{
   if ((x < 0) || (x >= bmp->w) || (y < 0) || (y >= bmp->h))
      return -1;
   return bmp->line[y][x];
}

void hline(BITMAP *bmp, int x1, int y, int x2, int color)
{
   int x, t;
   if (x1 > x2) { t = x1; x1 = x2; x2 = t; }
   if (bmp->clip) {
      if ((y < bmp->ct) || (y >= bmp->cb))
         return;
      if (x1 < bmp->cl) x1 = bmp->cl;
      if (x2 >= bmp->cr) x2 = bmp->cr - 1;
      if (x2 < x1)
         return;
   }
   if (_drawing_mode == DRAW_MODE_SOLID) {
      memset(bmp->line[y] + x1, color, (size_t)(x2 - x1 + 1));
      return;
   }
   for (x = x1; x <= x2; x++)
      mode_pixel(bmp, x, y, color);
}

void vline(BITMAP *bmp, int x, int y1, int y2, int color)
{
   int y, t;
   if (y1 > y2) { t = y1; y1 = y2; y2 = t; }
   if (bmp->clip) {
      if ((x < bmp->cl) || (x >= bmp->cr))
         return;
      if (y1 < bmp->ct) y1 = bmp->ct;
      if (y2 >= bmp->cb) y2 = bmp->cb - 1;
      if (y2 < y1)
         return;
   }
   for (y = y1; y <= y2; y++)
      mode_pixel(bmp, x, y, color);
}

void clear_to_color(BITMAP *bmp, int color)
{
   int y;
   for (y = bmp->ct; y < bmp->cb; y++)
      memset(bmp->line[y] + bmp->cl, color, (size_t)(bmp->cr - bmp->cl));
}

static void do_line(BITMAP *bmp, int x1, int y1, int x2, int y2, int d, void (*proc)(BITMAP *, int, int, int))
{
   int dx = x2 - x1;
   int dy = y2 - y1;
   int i1, i2;
   int x, y;
   int dd;

#define DO_LINE(pri_sign, pri_c, pri_cond, sec_sign, sec_c, sec_cond) \
   { \
      if (d##pri_c == 0) { \
         proc(bmp, x1, y1, d); \
         return; \
      } \
      i1 = 2 * d##sec_c; \
      dd = i1 - (sec_sign(pri_sign d##pri_c)); \
      i2 = dd - (sec_sign(pri_sign d##pri_c)); \
      x = x1; \
      y = y1; \
      while (pri_c pri_cond pri_c##2) { \
         proc(bmp, x, y, d); \
         if (dd sec_cond 0) { \
            sec_c sec_sign## = 1; \
            dd += i2; \
         } \
         else \
            dd += i1; \
         pri_c pri_sign## = 1; \
      } \
   }

   if (dx >= 0) {
      if (dy >= 0) {
         if (dx >= dy) { DO_LINE(+, x, <=, +, y, >=); }
         else { DO_LINE(+, y, <=, +, x, >=); }
      }
      else {
         if (dx >= -dy) { DO_LINE(+, x, <=, -, y, <=); }
         else { DO_LINE(-, y, >=, +, x, >=); }
      }
   }
   else {
      if (dy >= 0) {
         if (-dx >= dy) { DO_LINE(-, x, >=, +, y, >=); }
         else { DO_LINE(+, y, <=, -, x, <=); }
      }
      else {
         if (-dx >= -dy) { DO_LINE(-, x, >=, -, y, <=); }
         else { DO_LINE(-, y, >=, -, x, <=); }
      }
   }
#undef DO_LINE
}

void line(BITMAP *bmp, int x1, int y1, int x2, int y2, int color)
{
   int sx, sy, dx, dy, t;
   if (x1 == x2) {
      vline(bmp, x1, y1, y2, color);
      return;
   }
   if (y1 == y2) {
      hline(bmp, x1, y1, x2, color);
      return;
   }
   if (bmp->clip) {
      sx = x1; sy = y1; dx = x2; dy = y2;
      if (sx > dx) { t = sx; sx = dx; dx = t; }
      if (sy > dy) { t = sy; sy = dy; dy = t; }
      if ((sx >= bmp->cr) || (sy >= bmp->cb) || (dx < bmp->cl) || (dy < bmp->ct))
         return;
      if ((sx >= bmp->cl) && (sy >= bmp->ct) && (dx < bmp->cr) && (dy < bmp->cb))
         bmp->clip = FALSE;
      t = TRUE;
   }
   else
      t = FALSE;
   do_line(bmp, x1, y1, x2, y2, color, putpixel);
   bmp->clip = t;
}

void rect(BITMAP *bmp, int x1, int y1, int x2, int y2, int color)
{
   if (x2 < x1) { x1 ^= x2; x2 ^= x1; x1 ^= x2; }
   if (y2 < y1) { y1 ^= y2; y2 ^= y1; y1 ^= y2; }
   hline(bmp, x1, y1, x2, color);
   if (y2 > y1)
      hline(bmp, x1, y2, x2, color);
   if (y2 - 1 >= y1 + 1) {
      vline(bmp, x1, y1 + 1, y2 - 1, color);
      if (x2 > x1)
         vline(bmp, x2, y1 + 1, y2 - 1, color);
   }
}

void rectfill(BITMAP *bmp, int x1, int y1, int x2, int y2, int color)
{
   int t;
   if (y1 > y2) { t = y1; y1 = y2; y2 = t; }
   if (bmp->clip) {
      if (x1 > x2) { t = x1; x1 = x2; x2 = t; }
      if (x1 < bmp->cl) x1 = bmp->cl;
      if (x2 >= bmp->cr) x2 = bmp->cr - 1;
      if (x2 < x1) return;
      if (y1 < bmp->ct) y1 = bmp->ct;
      if (y2 >= bmp->cb) y2 = bmp->cb - 1;
      if (y2 < y1) return;
      bmp->clip = FALSE;
      t = TRUE;
   }
   else
      t = FALSE;
   while (y1 <= y2) {
      hline(bmp, x1, y1, x2, color);
      y1++;
   }
   bmp->clip = t;
}

static void do_circle(BITMAP *bmp, int x, int y, int radius, int d, void (*proc)(BITMAP *, int, int, int))
{
   int cx = 0;
   int cy = radius;
   int df = 1 - radius;
   int d_e = 3;
   int d_se = -2 * radius + 5;
   do {
      proc(bmp, x + cx, y + cy, d);
      if (cx) proc(bmp, x - cx, y + cy, d);
      if (cy) proc(bmp, x + cx, y - cy, d);
      if ((cx) && (cy)) proc(bmp, x - cx, y - cy, d);
      if (cx != cy) {
         proc(bmp, x + cy, y + cx, d);
         if (cx) proc(bmp, x + cy, y - cx, d);
         if (cy) proc(bmp, x - cy, y + cx, d);
         if (cx && cy) proc(bmp, x - cy, y - cx, d);
      }
      if (df < 0) {
         df += d_e;
         d_e += 2;
         d_se += 2;
      }
      else {
         df += d_se;
         d_e += 2;
         d_se += 4;
         cy--;
      }
      cx++;
   } while (cx <= cy);
}

void circle(BITMAP *bmp, int x, int y, int radius, int color)
{
   int clip, sx, sy, dx, dy;
   if (bmp->clip) {
      sx = x - radius - 1; sy = y - radius - 1;
      dx = x + radius + 1; dy = y + radius + 1;
      if ((sx >= bmp->cr) || (sy >= bmp->cb) || (dx < bmp->cl) || (dy < bmp->ct))
         return;
      if ((sx >= bmp->cl) && (sy >= bmp->ct) && (dx < bmp->cr) && (dy < bmp->cb))
         bmp->clip = FALSE;
      clip = TRUE;
   }
   else
      clip = FALSE;
   do_circle(bmp, x, y, radius, color, putpixel);
   bmp->clip = clip;
}

void circlefill(BITMAP *bmp, int x, int y, int radius, int color)
{
   int cx = 0;
   int cy = radius;
   int df = 1 - radius;
   int d_e = 3;
   int d_se = -2 * radius + 5;
   int clip, sx, sy, dx, dy;
   if (bmp->clip) {
      sx = x - radius - 1; sy = y - radius - 1;
      dx = x + radius + 1; dy = y + radius + 1;
      if ((sx >= bmp->cr) || (sy >= bmp->cb) || (dx < bmp->cl) || (dy < bmp->ct))
         return;
      if ((sx >= bmp->cl) && (sy >= bmp->ct) && (dx < bmp->cr) && (dy < bmp->cb))
         bmp->clip = FALSE;
      clip = TRUE;
   }
   else
      clip = FALSE;
   do {
      hline(bmp, x - cy, y - cx, x + cy, color);
      if (cx)
         hline(bmp, x - cy, y + cx, x + cy, color);
      if (df < 0) {
         df += d_e;
         d_e += 2;
         d_se += 2;
      }
      else {
         if (cx != cy) {
            hline(bmp, x - cx, y - cy, x + cx, color);
            if (cy)
               hline(bmp, x - cx, y + cy, x + cx, color);
         }
         df += d_se;
         d_e += 2;
         d_se += 4;
         cy--;
      }
      cx++;
   } while (cx <= cy);
   bmp->clip = clip;
}

/* ------------------------------------------------------------------ flood fill */

typedef struct FLOODED_LINE {
   short flags;
   short lpos, rpos;
   short y;
   short next;
} FLOODED_LINE;

#define FLOOD_IN_USE     1
#define FLOOD_TODO_ABOVE 2
#define FLOOD_TODO_BELOW 4

static FLOODED_LINE *flood_lines;
static int flood_count, flood_alloc;

static FLOODED_LINE *flood_line(int c)
{
   return flood_lines + c;
}

static void flood_grow(int n)
{
   if (n > flood_alloc) {
      flood_alloc = n + 64;
      flood_lines = realloc(flood_lines, sizeof(FLOODED_LINE) * (size_t)flood_alloc);
   }
}

static int flooder(BITMAP *bmp, int x, int y, int src_color, int dest_color)
{
   int c, left, right;
   FLOODED_LINE *p;
   uint8_t *row = bmp->line[y];
   if (row[x] != src_color)
      return x + 1;
   for (left = x - 1; left >= bmp->cl; left--)
      if (row[left] != src_color)
         break;
   for (right = x + 1; right < bmp->cr; right++)
      if (row[right] != src_color)
         break;
   left++;
   right--;
   hline(bmp, left, y, right, dest_color);
   c = y;
   p = flood_line(c);
   if (p->flags) {
      while (p->next) {
         c = p->next;
         p = flood_line(c);
      }
      p->next = (short)(c = flood_count++);
      flood_grow(flood_count);
      p = flood_line(c);
   }
   p->flags = FLOOD_IN_USE;
   p->lpos = (short)left;
   p->rpos = (short)right;
   p->y = (short)y;
   p->next = 0;
   if (y > bmp->ct)
      p->flags |= FLOOD_TODO_ABOVE;
   if (y + 1 < bmp->cb)
      p->flags |= FLOOD_TODO_BELOW;
   return right + 2;
}

static int check_flood_line(BITMAP *bmp, int y, int left, int right, int src_color, int dest_color)
{
   int c;
   FLOODED_LINE *p;
   int ret = FALSE;
   while (left <= right) {
      c = y;
      do {
         p = flood_line(c);
         if ((left >= p->lpos) && (left <= p->rpos)) {
            left = p->rpos + 2;
            goto no_flood;
         }
         c = p->next;
      } while (c);
      left = flooder(bmp, left, y, src_color, dest_color);
      ret = TRUE;
   no_flood:;
   }
   return ret;
}

void floodfill(BITMAP *bmp, int x, int y, int color)
{
   int src_color, c, done;
   FLOODED_LINE *p;
   if ((x < bmp->cl) || (x >= bmp->cr) || (y < bmp->ct) || (y >= bmp->cb))
      return;
   src_color = getpixel(bmp, x, y);
   if (src_color == color)
      return;
   flood_grow(bmp->cb);
   flood_count = bmp->cb;
   for (c = 0; c < flood_count; c++) {
      flood_lines[c].flags = 0;
      flood_lines[c].lpos = SHRT_MAX;
      flood_lines[c].rpos = SHRT_MIN;
      flood_lines[c].y = (short)y;
      flood_lines[c].next = 0;
   }
   flooder(bmp, x, y, src_color, color);
   do {
      done = TRUE;
      for (c = 0; c < flood_count; c++) {
         p = flood_line(c);
         if (p->flags & FLOOD_TODO_BELOW) {
            p->flags &= ~FLOOD_TODO_BELOW;
            if (check_flood_line(bmp, p->y + 1, p->lpos, p->rpos, src_color, color)) {
               done = FALSE;
               p = flood_line(c);
            }
         }
         if (p->flags & FLOOD_TODO_ABOVE) {
            p->flags &= ~FLOOD_TODO_ABOVE;
            if (check_flood_line(bmp, p->y - 1, p->lpos, p->rpos, src_color, color)) {
               done = FALSE;
               if ((c < bmp->cb) && (c > 0))
                  c -= 2;
            }
         }
      }
   } while (!done);
}

/* ------------------------------------------------------------------ blitting */

void blit(BITMAP *src, BITMAP *dest, int s_x, int s_y, int d_x, int d_y, int w, int h)
{
   int y;
   if ((s_x >= src->w) || (s_y >= src->h) || (d_x >= dest->cr) || (d_y >= dest->cb))
      return;
   if (s_x < 0) { w += s_x; d_x -= s_x; s_x = 0; }
   if (s_y < 0) { h += s_y; d_y -= s_y; s_y = 0; }
   if (s_x + w > src->w) w = src->w - s_x;
   if (s_y + h > src->h) h = src->h - s_y;
   if (d_x < dest->cl) { d_x -= dest->cl; w += d_x; s_x -= d_x; d_x = dest->cl; }
   if (d_y < dest->ct) { d_y -= dest->ct; h += d_y; s_y -= d_y; d_y = dest->ct; }
   if (d_x + w > dest->cr) w = dest->cr - d_x;
   if (d_y + h > dest->cb) h = dest->cb - d_y;
   if ((w <= 0) || (h <= 0))
      return;
   /* memmove handles overlap within a row; pick row order for overlapping blits */
   if (src->line[s_y] + s_x < dest->line[d_y] + d_x) {
      for (y = h - 1; y >= 0; y--)
         memmove(dest->line[d_y + y] + d_x, src->line[s_y + y] + s_x, (size_t)w);
   }
   else {
      for (y = 0; y < h; y++)
         memmove(dest->line[d_y + y] + d_x, src->line[s_y + y] + s_x, (size_t)w);
   }
}

void masked_blit(BITMAP *src, BITMAP *dest, int s_x, int s_y, int d_x, int d_y, int w, int h)
{
   int x, y;
   if ((s_x >= src->w) || (s_y >= src->h) || (d_x >= dest->cr) || (d_y >= dest->cb))
      return;
   if (s_x < 0) { w += s_x; d_x -= s_x; s_x = 0; }
   if (s_y < 0) { h += s_y; d_y -= s_y; s_y = 0; }
   if (s_x + w > src->w) w = src->w - s_x;
   if (s_y + h > src->h) h = src->h - s_y;
   if (d_x < dest->cl) { d_x -= dest->cl; w += d_x; s_x -= d_x; d_x = dest->cl; }
   if (d_y < dest->ct) { d_y -= dest->ct; h += d_y; s_y -= d_y; d_y = dest->ct; }
   if (d_x + w > dest->cr) w = dest->cr - d_x;
   if (d_y + h > dest->cb) h = dest->cb - d_y;
   for (y = 0; y < h; y++) {
      const uint8_t *s = src->line[s_y + y] + s_x;
      uint8_t *d = dest->line[d_y + y] + d_x;
      for (x = 0; x < w; x++)
         if (s[x])
            d[x] = s[x];
   }
}

/* ------------------------------------------------------------------ sprites */

/* Computes the clipped region of a w*h sprite drawn at x,y. Returns 0 if nothing visible. */
static int sprite_clip(BITMAP *bmp, int x, int y, int w, int h, int *lgap, int *tgap, int *cw, int *ch)
{
   if (bmp->clip) {
      int t = bmp->ct - y, l = bmp->cl - x;
      if (t < 0) t = 0;
      if (l < 0) l = 0;
      *tgap = t;
      *lgap = l;
      *ch = MIN(h, bmp->cb - y) - t;
      *cw = MIN(w, bmp->cr - x) - l;
      if (*ch <= 0 || *cw <= 0)
         return 0;
   }
   else {
      *tgap = *lgap = 0;
      *cw = w;
      *ch = h;
   }
   return 1;
}

enum { SPR_NORMAL, SPR_TRANS, SPR_LIT };

static void sprite_generic(BITMAP *bmp, BITMAP *sprite, int x, int y, int hflip, int vflip, int kind, int color)
{
   int lgap, tgap, w, h, i, j;
   if (!sprite_clip(bmp, x, y, sprite->w, sprite->h, &lgap, &tgap, &w, &h))
      return;
   for (j = 0; j < h; j++) {
      int sy = tgap + j;
      const uint8_t *s;
      uint8_t *d = bmp->line[y + sy] + x + lgap;
      if (vflip)
         sy = sprite->h - 1 - sy;
      s = sprite->line[sy];
      for (i = 0; i < w; i++) {
         int sx = lgap + i;
         uint8_t c = s[hflip ? sprite->w - 1 - sx : sx];
         if (!c)
            continue;
         if (kind == SPR_NORMAL)
            d[i] = c;
         else if (kind == SPR_TRANS)
            d[i] = color_map->data[c][d[i]];
         else
            d[i] = color_map->data[color & 0xFF][c];
      }
   }
}

void draw_sprite(BITMAP *bmp, BITMAP *sprite, int x, int y) { sprite_generic(bmp, sprite, x, y, 0, 0, SPR_NORMAL, 0); }
void draw_sprite_v_flip(BITMAP *bmp, BITMAP *sprite, int x, int y) { sprite_generic(bmp, sprite, x, y, 0, 1, SPR_NORMAL, 0); }
void draw_sprite_h_flip(BITMAP *bmp, BITMAP *sprite, int x, int y) { sprite_generic(bmp, sprite, x, y, 1, 0, SPR_NORMAL, 0); }
void draw_sprite_vh_flip(BITMAP *bmp, BITMAP *sprite, int x, int y) { sprite_generic(bmp, sprite, x, y, 1, 1, SPR_NORMAL, 0); }
void draw_trans_sprite(BITMAP *bmp, BITMAP *sprite, int x, int y) { sprite_generic(bmp, sprite, x, y, 0, 0, SPR_TRANS, 0); }
void draw_lit_sprite(BITMAP *bmp, BITMAP *sprite, int x, int y, int color) { sprite_generic(bmp, sprite, x, y, 0, 0, SPR_LIT, color); }

/* glyph renderer: set pixels get color, clear pixels get the text mode background (if >= 0) */
void draw_character(BITMAP *bmp, BITMAP *sprite, int x, int y, int color)
{
   int lgap, tgap, w, h, i, j;
   if (!sprite_clip(bmp, x, y, sprite->w, sprite->h, &lgap, &tgap, &w, &h))
      return;
   for (j = 0; j < h; j++) {
      const uint8_t *s = sprite->line[tgap + j] + lgap;
      uint8_t *d = bmp->line[y + tgap + j] + x + lgap;
      for (i = 0; i < w; i++) {
         if (s[i])
            d[i] = (uint8_t)color;
         else if (_textmode >= 0)
            d[i] = (uint8_t)_textmode;
      }
   }
}

/* ------------------------------------------------------------------ RLE sprites */

RLE_SPRITE *get_rle_sprite(BITMAP *bitmap)
{
   int x, y, run, c = 0, cap = 256;
   int8_t *p8 = malloc((size_t)cap);
   RLE_SPRITE *s;
#define WRITE8(v) do { if (c + 1 > cap) { cap *= 2; p8 = realloc(p8, (size_t)cap); } p8[c++] = (int8_t)(v); } while (0)
   for (y = 0; y < bitmap->h; y++) {
      run = -1;
      for (x = 0; x < bitmap->w; x++) {
         int px = bitmap->line[y][x];
         if (px) {
            if ((run >= 0) && (p8[run] > 0) && (p8[run] < 127))
               p8[run]++;
            else {
               run = c;
               WRITE8(1);
            }
            WRITE8(px);
         }
         else {
            if ((run >= 0) && (p8[run] < 0) && (p8[run] > -128))
               p8[run]--;
            else {
               run = c;
               WRITE8(-1);
            }
         }
      }
      WRITE8(0);
   }
#undef WRITE8
   s = malloc(sizeof(RLE_SPRITE));
   s->w = bitmap->w;
   s->h = bitmap->h;
   s->size = c;
   s->dat = p8;
   return s;
}

void destroy_rle_sprite(RLE_SPRITE *sprite)
{
   if (sprite) {
      free(sprite->dat);
      free(sprite);
   }
}

static void rle_generic(BITMAP *bmp, RLE_SPRITE *spr, int x, int y, int kind, int color)
{
   int lgap, tgap, w, h, j;
   const int8_t *p = spr->dat;
   if (!sprite_clip(bmp, x, y, spr->w, spr->h, &lgap, &tgap, &w, &h))
      return;
   /* skip clipped top rows */
   for (j = 0; j < tgap; j++) {
      for (;;) {
         int c = *p++;
         if (c == 0) break;
         if (c > 0) p += c;
      }
   }
   for (j = 0; j < h; j++) {
      uint8_t *d = bmp->line[y + tgap + j] + x;
      int sx = 0;
      for (;;) {
         int c = *p++;
         if (c == 0)
            break;
         if (c > 0) {
            int k;
            for (k = 0; k < c; k++, sx++) {
               uint8_t px = (uint8_t)p[k];
               if (sx < lgap || sx >= lgap + w)
                  continue;
               if (kind == SPR_NORMAL)
                  d[sx] = px;
               else if (kind == SPR_TRANS)
                  d[sx] = color_map->data[px][d[sx]];
               else
                  d[sx] = color_map->data[color & 0xFF][px];
            }
            p += c;
         }
         else
            sx -= c;
      }
   }
}

void draw_rle_sprite(BITMAP *bmp, RLE_SPRITE *sprite, int x, int y) { rle_generic(bmp, sprite, x, y, SPR_NORMAL, 0); }
void draw_trans_rle_sprite(BITMAP *bmp, RLE_SPRITE *sprite, int x, int y) { rle_generic(bmp, sprite, x, y, SPR_TRANS, 0); }
void draw_lit_rle_sprite(BITMAP *bmp, RLE_SPRITE *sprite, int x, int y, int color) { rle_generic(bmp, sprite, x, y, SPR_LIT, color); }

/* ------------------------------------------------------------------ rotation */

void rotate_sprite(BITMAP *bmp, BITMAP *sprite, int x, int y, fixed angle)
{
   rotate_scaled_sprite(bmp, sprite, x, y, angle, itofix(1));
}

void rotate_scaled_sprite(BITMAP *bmp, BITMAP *sprite, int x, int y, fixed angle, fixed scale)
{
   fixed f1x, f1y, f1xd, f1yd;
   fixed f2x, f2y, f2xd, f2yd;
   fixed w, h;
   fixed dir, dir2, dist;
   int x1, y1, x2, y2;
   int dx, dy;
   int sx, sy;
   int wgap = sprite->w;
   int hgap = sprite->h;
   uint8_t *addr;

   w = itofix(wgap / 2);
   h = itofix(hgap / 2);
   dir = fatan2(h, w);
   dir2 = fatan2(h, -w);
   dist = fsqrt((wgap * wgap + hgap * hgap) << 6) << 4;
   f1x = w - fmul(dist, fcos(dir - angle));
   f1y = h - fmul(dist, fsin(dir - angle));
   f1xd = fcos(itofix(64) - angle);
   f1yd = fsin(itofix(64) - angle);
   f2xd = fcos(-angle);
   f2yd = fsin(-angle);
   if (scale != itofix(1)) {
      dist = fmul(dist, scale);
      wgap = fixtoi(fmul(itofix(wgap), scale));
      hgap = fixtoi(fmul(itofix(hgap), scale));
      scale = fdiv(itofix(1), scale);
      f1xd = fmul(f1xd, scale);
      f1yd = fmul(f1yd, scale);
      f2xd = fmul(f2xd, scale);
      f2yd = fmul(f2yd, scale);
   }
   x1 = fixtoi(fmul(dist, fcos(dir + angle)));
   y1 = fixtoi(fmul(dist, fsin(dir + angle)));
   x2 = fixtoi(fmul(dist, fcos(dir2 + angle)));
   y2 = fixtoi(fmul(dist, fsin(dir2 + angle)));
   x1 = MAX(ABS(x1), ABS(x2)) - wgap / 2;
   y1 = MAX(ABS(y1), ABS(y2)) - hgap / 2;
   x -= x1;
   wgap += x1 * 2;
   f1x -= f2xd * x1;
   f1y -= f2yd * x1;
   y -= y1;
   hgap += y1 * 2;
   f1x -= f1xd * y1;
   f1y -= f1yd * y1;
   if (bmp->clip) {
      while (x < bmp->cl) { x++; wgap--; f1x += f2xd; f1y += f2yd; }
      while (y < bmp->ct) { y++; hgap--; f1x += f1xd; f1y += f1yd; }
      while (x + wgap > bmp->cr) wgap--;
      while (y + hgap > bmp->cb) hgap--;
      if ((wgap <= 0) || (hgap <= 0))
         return;
   }
   for (dy = hgap - 1; dy >= 0; dy--) {
      f2x = f1x;
      f2y = f1y;
      addr = bmp->line[y + hgap - dy - 1] + x;
      for (dx = wgap - 1; dx >= 0; dx--) {
         sx = fixtoi(f2x);
         if ((unsigned)sx < (unsigned)sprite->w) {
            sy = fixtoi(f2y);
            if ((unsigned)sy < (unsigned)sprite->h) {
               if (sprite->line[sy][sx])
                  *addr = sprite->line[sy][sx];
            }
         }
         addr++;
         f2x += f2xd;
         f2y += f2yd;
      }
      f1x += f1xd;
      f1y += f1yd;
   }
}

/* ------------------------------------------------------------------ stretching */

/* One row of the runtime-generated 8-bit stretcher from stretch.c, interpreted. */
static void stretch_row(const uint8_t *src, uint8_t *dst, fixed sx, fixed sxd, int dest_width, int masked)
{
   int x, x2;
   if (sxd == itofix(1)) {
      for (x = 0; x < dest_width; x++)
         if (!masked || src[x])
            dst[x] = src[x];
   }
   else if (sxd > itofix(1)) {
      for (x = 0; x < dest_width; x++) {
         uint8_t c = *src++;
         if (!masked || c)
            dst[x] = c;
         x2 = (sx >> 16) + 1;
         sx += sxd;
         x2 = (sx >> 16) - x2;
         if (x2 > 0)
            src += x2;
      }
   }
   else {
      uint8_t c;
      x2 = sx >> 16;
      c = *src++;
      for (x = 0; x < dest_width; x++) {
         if (!masked || c)
            dst[x] = c;
         sx += sxd;
         if ((sx >> 16) > x2) {
            c = *src++;
            x2++;
         }
      }
   }
}

static void do_stretch_blit(BITMAP *source, BITMAP *dest, int source_x, int source_y, int source_width, int source_height,
                            int dest_x, int dest_y, int dest_width, int dest_height, int masked)
{
   fixed sx, sy, sxd, syd;
   int y;
   if ((source_width <= 0) || (source_height <= 0) || (dest_width <= 0) || (dest_height <= 0))
      return;
   sx = itofix(source_x);
   sy = itofix(source_y);
   sxd = itofix(source_width) / dest_width;
   syd = itofix(source_height) / dest_height;
   if (dest->clip) {
      while (dest_x < dest->cl) { dest_x++; dest_width--; sx += sxd; }
      while (dest_y < dest->ct) { dest_y++; dest_height--; sy += syd; }
      if (dest_x + dest_width > dest->cr) dest_width = dest->cr - dest_x;
      if (dest_y + dest_height > dest->cb) dest_height = dest->cb - dest_y;
      if ((dest_width <= 0) || (dest_height <= 0))
         return;
   }
   /* _do_stretch (misc.s): for each dest line, run the stretcher on source line sy>>16 */
   for (y = 0; y < dest_height; y++) {
      stretch_row(source->line[sy >> 16] + (sx >> 16), dest->line[dest_y + y] + dest_x, sx, sxd, dest_width, masked);
      sy += syd;
   }
}

void stretch_blit(BITMAP *s, BITMAP *d, int s_x, int s_y, int s_w, int s_h, int d_x, int d_y, int d_w, int d_h)
{
   do_stretch_blit(s, d, s_x, s_y, s_w, s_h, d_x, d_y, d_w, d_h, 0);
}

void stretch_sprite(BITMAP *bmp, BITMAP *sprite, int x, int y, int w, int h)
{
   do_stretch_blit(sprite, bmp, 0, 0, sprite->w, sprite->h, x, y, w, h, 1);
}

/* ------------------------------------------------------------------ text */

void text_mode(int mode)
{
   _textmode = (mode < 0) ? -1 : mode;
}

static void blit_character(BITMAP *bmp, BITMAP *sprite, int x, int y)
{
   blit(sprite, bmp, 0, 0, x, y, sprite->w, sprite->h);
}

static void textout_fixed(BITMAP *bmp, FONT *f, const unsigned char *str, int x, int y, int color)
{
   int h = f->height;
   while (*str) {
      int c = (int)*str - ' ', i, j;
      if ((c < 0) || (c >= FONT_SIZE))
         c = 0;
      for (j = 0; j < h; j++) {
         int bits = f->fixed_dat[c * h + j];
         for (i = 0; i < 8; i++) {
            if (bits & (0x80 >> i)) {
               if (color >= 0) putpixel(bmp, x + i, y + j, color);
            }
            else if (_textmode >= 0)
               putpixel(bmp, x + i, y + j, _textmode);
         }
      }
      x += 8;
      str++;
   }
}

void textout(BITMAP *bmp, FONT *f, const char *s, int x, int y, int color)
{
   const unsigned char *str = (const unsigned char *)s;
   BITMAP *b;
   int c;
   if (f->height > 0) {
      textout_fixed(bmp, f, str, x, y, color);
      return;
   }
   while (*str) {
      c = (int)*str - ' ';
      if ((c < 0) || (c >= FONT_SIZE))
         c = 0;
      b = f->glyph[c];
      if (b) {
         if (color >= 0)
            draw_character(bmp, b, x, y, color);
         else if (_textmode < 0)
            draw_sprite(bmp, b, x, y);
         else
            blit_character(bmp, b, x, y);
         x += b->w;
         if (x >= bmp->cr)
            return;
      }
      str++;
   }
}

int text_length(FONT *f, const char *s)
{
   const unsigned char *str = (const unsigned char *)s;
   int c, len = 0;
   if (f->height > 0)
      return (int)strlen(s) * 8;
   while (*str) {
      c = (int)*str - ' ';
      if ((c < 0) || (c >= FONT_SIZE))
         c = 0;
      if (f->glyph[c])
         len += f->glyph[c]->w;
      str++;
   }
   return len;
}

void textout_centre(BITMAP *bmp, FONT *f, const char *str, int x, int y, int color)
{
   int len = text_length(f, str);
   textout(bmp, f, str, x - len / 2, y, color);
}

void textprintf(BITMAP *bmp, FONT *f, int x, int y, int color, const char *fmt, ...)
{
   char buf[256];
   va_list ap;
   va_start(ap, fmt);
   vsnprintf(buf, sizeof(buf), fmt, ap);
   va_end(ap);
   textout(bmp, f, buf, x, y, color);
}

void textprintf_centre(BITMAP *bmp, FONT *f, int x, int y, int color, const char *fmt, ...)
{
   char buf[256];
   va_list ap;
   va_start(ap, fmt);
   vsnprintf(buf, sizeof(buf), fmt, ap);
   va_end(ap);
   textout_centre(bmp, f, buf, x, y, color);
}
