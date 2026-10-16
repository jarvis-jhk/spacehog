/* Palette handling, fades and colour tables (Allegro 3.11 algorithms, giftware). */
#include "alleg.h"
#include "../platform/sys.h"
#include <string.h>
#include <limits.h>

PALETTE black_pallete;
PALETTE _current_pallete;

static int col_diff[3 * 128];

static void bestfit_init(void)
{
   int i;
   for (i = 1; i < 64; i++) {
      int k = i * i;
      col_diff[0 + i] = col_diff[0 + 128 - i] = k * (59 * 59);
      col_diff[128 + i] = col_diff[128 + 128 - i] = k * (30 * 30);
      col_diff[256 + i] = col_diff[256 + 128 - i] = k * (11 * 11);
   }
}

int bestfit_color(const RGB *pal, int r, int g, int b)
{
   int i, coldiff, lowest, bestfit;
   if (col_diff[1] == 0)
      bestfit_init();
   bestfit = 0;
   lowest = INT_MAX;
   if ((r == 63) && (g == 0) && (b == 63))
      i = 0;
   else
      i = 1;
   while (i < PAL_SIZE) {
      const RGB *rgb = &pal[i];
      coldiff = (col_diff + 0)[(rgb->g - g) & 0x7F];
      if (coldiff < lowest) {
         coldiff += (col_diff + 128)[(rgb->r - r) & 0x7F];
         if (coldiff < lowest) {
            coldiff += (col_diff + 256)[(rgb->b - b) & 0x7F];
            if (coldiff < lowest) {
               bestfit = i;
               if (coldiff == 0)
                  return bestfit;
               lowest = coldiff;
            }
         }
      }
      i++;
   }
   return bestfit;
}

void set_color(int index, const RGB *p)
{
   set_pallete_range(p - index, index, index, FALSE);
}

void set_pallete(const RGB *p)
{
   set_pallete_range(p, 0, PAL_SIZE - 1, TRUE);
}

void set_pallete_range(const RGB *p, int from, int to, int vs)
{
   int c;
   if (vs)
      vsync();
   for (c = from; c <= to; c++)
      _current_pallete[c] = p[c];
}

void get_pallete(RGB *p)
{
   get_pallete_range(p, 0, PAL_SIZE - 1);
}

void get_pallete_range(RGB *p, int from, int to)
{
   int c;
   for (c = from; c <= to; c++)
      p[c] = _current_pallete[c];
}

void fade_interpolate(const RGB *source, const RGB *dest, RGB *output, int pos, int from, int to)
{
   int c;
   for (c = from; c <= to; c++) {
      output[c].r = (uint8_t)(((int)source[c].r * (63 - pos) + (int)dest[c].r * pos) / 64);
      output[c].g = (uint8_t)(((int)source[c].g * (63 - pos) + (int)dest[c].g * pos) / 64);
      output[c].b = (uint8_t)(((int)source[c].b * (63 - pos) + (int)dest[c].b * pos) / 64);
   }
}

void fade_from_range(const RGB *source, const RGB *dest, int speed, int from, int to)
{
   PALETTE temp;
   int c;
   for (c = 0; c < PAL_SIZE; c++)
      temp[c] = source[c];
   sys_sync_enter('F');
   for (c = 0; c < 64; c += speed) {
      fade_interpolate(source, dest, temp, c, from, to);
      set_pallete_range(temp, from, to, TRUE);
      set_pallete_range(temp, from, to, TRUE);
   }
   set_pallete_range(dest, from, to, TRUE);
   sys_sync_leave(0);
}

void fade_from(const RGB *source, const RGB *dest, int speed)
{
   fade_from_range(source, dest, speed, 0, PAL_SIZE - 1);
}

void fade_in(const RGB *p, int speed)
{
   fade_from_range(black_pallete, p, speed, 0, PAL_SIZE - 1);
}

void fade_out(int speed)
{
   PALETTE temp;
   get_pallete(temp);
   fade_from_range(temp, black_pallete, speed, 0, PAL_SIZE - 1);
}

void create_rgb_table(RGB_MAP *table, const RGB *pal, void (*callback)(int pos))
{
#define UNUSED 65535
#define LAST 65532
#define add(i) (next[(i)] == UNUSED ? (next[(i)] = LAST, \
                (first != LAST ? (next[last] = (unsigned short)(i)) : (first = (i))), \
                (last = (i))) : 0)
#define add1(i) (next[(i)] == UNUSED ? (next[(i)] = LAST, \
                 next[last] = (unsigned short)(i), \
                 (last = (i))) : 0)
#define dist(a1, a2, a3, b1, b2, b3) \
   (col_diff[((a2) - (b2)) & 0x7F] + \
    (col_diff + 128)[((a1) - (b1)) & 0x7F] + \
    (col_diff + 256)[((a3) - (b3)) & 0x7F])
#define pos(r, g, b) (((r) / 2) * 32 * 32 + ((g) / 2) * 32 + ((b) / 2))
#define depos(pal, r, g, b) \
   ((b) = ((pal) & 31) * 2, \
    (g) = (((pal) >> 5) & 31) * 2, \
    (r) = (((pal) >> 10) & 31) * 2)
#define better(r1, g1, b1, pal1) \
   (((int)dist((r1), (g1), (b1), (pal1).r, (pal1).g, (pal1).b)) > (int)dist2)
#define dopos(rp, gp, bp, ts) \
   if ((rp > -1 || r > 0) && (rp < 1 || r < 61) && \
       (gp > -1 || g > 0) && (gp < 1 || g < 61) && \
       (bp > -1 || b > 0) && (bp < 1 || b < 61)) { \
      i = first + rp * 32 * 32 + gp * 32 + bp; \
      if (ts ? data[i] != val : !data[i]) { \
         dist2 = (rp ? (col_diff + 128)[(r + 2 * rp - pal[val].r) & 0x7F] : r2) + \
                 (gp ? (col_diff)[(g + 2 * gp - pal[val].g) & 0x7F] : g2) + \
                 (bp ? (col_diff + 256)[(b + 2 * bp - pal[val].b) & 0x7F] : b2); \
         if (better((r + 2 * rp), (g + 2 * gp), (b + 2 * bp), pal[data[i]])) { \
            data[i] = (unsigned char)val; \
            add1(i); \
         } \
      } \
   }

   int i, curr, r, g, b, val, r2, g2, b2, dist2;
   static unsigned short next[32 * 32 * 32];
   unsigned char *data;
   int first = LAST;
   int last = LAST;
   int count = 0;
   int cbcount = 0;
#define AVERAGE_COUNT 18000

   if (col_diff[1] == 0)
      bestfit_init();
   memset(next, 255, sizeof(next));
   memset(table->data, 0, sizeof(char) * 32 * 32 * 32);
   data = (unsigned char *)table->data;

   for (i = 1; i < 256; i++) {
      curr = pos(pal[i].r, pal[i].g, pal[i].b);
      if (next[curr] == UNUSED) {
         data[curr] = (unsigned char)i;
         add(curr);
      }
   }

   while (first != LAST) {
      depos(first, r, g, b);
      val = data[first];
      r2 = (col_diff + 128)[((pal[val].r) - (r)) & 0x7F];
      g2 = (col_diff)[((pal[val].g) - (g)) & 0x7F];
      b2 = (col_diff + 256)[((pal[val].b) - (b)) & 0x7F];
      dopos(0, 0, 1, 1);
      dopos(0, 0, -1, 1);
      dopos(1, 0, 0, 1);
      dopos(-1, 0, 0, 1);
      dopos(0, 1, 0, 1);
      dopos(0, -1, 0, 1);
      if ((b > 0) && (data[first - 1] == val)) {
         b -= 2;
         first--;
         b2 = (col_diff + 256)[((pal[val].b) - (b)) & 0x7F];
         dopos(-1, 0, 0, 0);
         dopos(1, 0, 0, 0);
         dopos(0, -1, 0, 0);
         dopos(0, 1, 0, 0);
         first++;
      }
      i = first;
      first = next[first];
      next[i] = UNUSED;
      if (first != LAST) {
         depos(first, r, g, b);
         val = data[first];
         r2 = (col_diff + 128)[((pal[val].r) - (r)) & 0x7F];
         g2 = (col_diff)[((pal[val].g) - (g)) & 0x7F];
         b2 = (col_diff + 256)[((pal[val].b) - (b)) & 0x7F];
         dopos(0, 0, 1, 1);
         dopos(0, 0, -1, 1);
         dopos(1, 0, 0, 1);
         dopos(-1, 0, 0, 1);
         dopos(0, 1, 0, 1);
         dopos(0, -1, 0, 1);
         if ((b < 61) && (data[first + 1] == val)) {
            b += 2;
            first++;
            b2 = (col_diff + 256)[((pal[val].b) - (b)) & 0x7f];
            dopos(-1, 0, 0, 0);
            dopos(1, 0, 0, 0);
            dopos(0, -1, 0, 0);
            dopos(0, 1, 0, 0);
            first--;
         }
         i = first;
         first = next[first];
         next[i] = UNUSED;
      }
      count++;
      if (count == (cbcount + 1) * AVERAGE_COUNT / 256) {
         if (cbcount < 256) {
            if (callback)
               callback(cbcount);
            cbcount++;
         }
      }
   }
   if (callback)
      while (cbcount < 256)
         callback(cbcount++);
#undef UNUSED
#undef LAST
#undef add
#undef add1
#undef dist
#undef pos
#undef depos
#undef better
#undef dopos
}

void create_light_table(COLOR_MAP *table, const RGB *pal, int r, int g, int b, void (*callback)(int pos))
{
   int r1, g1, b1, x, y;
   unsigned int tmp;
   RGB c;
   for (x = 0; x < PAL_SIZE; x++) {
      tmp = (unsigned)(255 - x) * 65793u;
      r1 = (int)(((unsigned int)r * tmp) >> 24);
      g1 = (int)(((unsigned int)g * tmp) >> 24);
      b1 = (int)(((unsigned int)b * tmp) >> 24);
      tmp = 255u * 65793u - tmp;
      for (y = 0; y < PAL_SIZE; y++) {
         c.r = (uint8_t)(r1 + (int)(((unsigned int)pal[y].r * tmp) >> 24));
         c.g = (uint8_t)(g1 + (int)(((unsigned int)pal[y].g * tmp) >> 24));
         c.b = (uint8_t)(b1 + (int)(((unsigned int)pal[y].b * tmp) >> 24));
         if (rgb_map)
            table->data[x][y] = rgb_map->data[c.r >> 1][c.g >> 1][c.b >> 1];
         else
            table->data[x][y] = (uint8_t)bestfit_color(pal, c.r, c.g, c.b);
      }
      if (callback)
         (*callback)(x);
   }
}

void create_trans_table(COLOR_MAP *table, const RGB *pal, int r, int g, int b, void (*callback)(int pos))
{
   int tmp[768], *q;
   int x, y, i, j, k;
   unsigned char *p;
   RGB c;
   for (x = 0; x < 256; x++) {
      tmp[x * 3] = pal[x].r * (255 - r) / 255;
      tmp[x * 3 + 1] = pal[x].g * (255 - g) / 255;
      tmp[x * 3 + 2] = pal[x].b * (255 - b) / 255;
   }
   for (y = 0; y < PAL_SIZE; y++)
      table->data[0][y] = (uint8_t)y;
   if (callback)
      (*callback)(0);
   for (x = 1; x < 256; x++) {
      i = pal[x].r * r / 255;
      j = pal[x].g * g / 255;
      k = pal[x].b * b / 255;
      p = table->data[x];
      q = tmp;
      for (y = 0; y < 256; y++) {
         c.r = (uint8_t)(i + *(q++));
         c.g = (uint8_t)(j + *(q++));
         c.b = (uint8_t)(k + *(q++));
         if (rgb_map)
            p[y] = rgb_map->data[c.r >> 1][c.g >> 1][c.b >> 1];
         else
            p[y] = (uint8_t)bestfit_color(pal, c.r, c.g, c.b);
      }
      if (callback)
         (*callback)(x);
   }
}
