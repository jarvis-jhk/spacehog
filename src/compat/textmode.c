/* 80x25 colour text mode emulation for the DOS conio calls of the setup screen. */
#include "alleg.h"
#include "internal.h"
#include "dos.h"
#include "../platform/sys.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define TW 80
#define TH 25
#define CW 8
#define CH 16

extern const uint8_t font8x8_data[224][8];

static uint16_t cells[TH][TW];
static int cur_x, cur_y; /* 0-based */
static uint8_t attr = 0x07;
static int active;
static int keep_attr;
static uint8_t pixels[TW * CW * TH * CH];

static const uint32_t ega[16] = {
   0x000000, 0x0000AA, 0x00AA00, 0x00AAAA, 0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
   0x555555, 0x5555FF, 0x55FF55, 0x55FFFF, 0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF
};

void sys_set_textmode(int on)
{
   active = on;
}

int _textmode_active(void)
{
   return active;
}

static void scroll(void)
{
   int x;
   memmove(cells[0], cells[1], sizeof(cells[0]) * (TH - 1));
   for (x = 0; x < TW; x++)
      cells[TH - 1][x] = (uint16_t)(attr << 8 | ' ');
   cur_y = TH - 1;
}

void sys_textmode_putc(int c)
{
   if (c == '\n') {
      cur_x = 0;
      if (++cur_y >= TH) scroll();
      return;
   }
   if (c == '\r') {
      cur_x = 0;
      return;
   }
   if (keep_attr)
      cells[cur_y][cur_x] = (uint16_t)((cells[cur_y][cur_x] & 0xFF00) | (c & 0xFF));
   else
      cells[cur_y][cur_x] = (uint16_t)(attr << 8 | (c & 0xFF));
   if (++cur_x >= TW) {
      cur_x = 0;
      if (++cur_y >= TH) scroll();
   }
}

void sys_textmode_gotoxy(int x, int y)
{
   cur_x = MID(0, x - 1, TW - 1);
   cur_y = MID(0, y - 1, TH - 1);
}

void sys_textmode_color(int fg, int bg)
{
   if (fg >= 0) attr = (uint8_t)((attr & 0x70) | (fg & 0x8F));
   if (bg >= 0) attr = (uint8_t)((attr & 0x8F) | ((bg & 7) << 4));
}

void sys_textmode_clrscr(void)
{
   int x, y;
   for (y = 0; y < TH; y++)
      for (x = 0; x < TW; x++)
         cells[y][x] = (uint16_t)(attr << 8 | ' ');
   cur_x = cur_y = 0;
}

void sys_textmode_clreol(void)
{
   int x;
   for (x = cur_x; x < TW; x++)
      cells[cur_y][x] = (uint16_t)(attr << 8 | ' ');
}

/* The few CP437 graphics characters the setup screen uses, as 8x16 bitmaps (drawn by hand
 * to look like the VGA ROM font); everything else uses the doubled 8x8 Allegro font. */
static const uint8_t *cp437_glyph(int ch)
{
   static uint8_t g[16];
   int y;
   switch (ch) {
      case 0xB0: for (y = 0; y < 16; y++) g[y] = (y & 1) ? 0x88 : 0x22; return g;   /* light shade */
      case 0xB1: for (y = 0; y < 16; y++) g[y] = (y & 1) ? 0xAA : 0x55; return g;   /* medium shade */
      case 0xB2: for (y = 0; y < 16; y++) g[y] = (y & 1) ? 0xEE : 0xBB; return g;   /* dark shade */
      case 0xDB: memset(g, 0xFF, 16); return g;                                     /* full block */
      case 0xCD: memset(g, 0, 16); g[6] = g[8] = 0xFF; return g;                    /* double horizontal line */
      case 0xF9: memset(g, 0, 16); g[7] = g[8] = 0x18; return g;                    /* bullet */
      case 0xFE: memset(g, 0, 16); for (y = 5; y < 11; y++) g[y] = 0x7C; return g;  /* small square */
      default: return NULL;
   }
}

void _textmode_render(const uint8_t **px, int *w, int *h, uint32_t *pal32)
{
   int cx, cy, x, y, i;
   for (i = 0; i < 16; i++)
      pal32[i] = 0xFF000000u | ega[i];
   for (cy = 0; cy < TH; cy++)
      for (cx = 0; cx < TW; cx++) {
         int ch = cells[cy][cx] & 0xFF, a = cells[cy][cx] >> 8;
         int fg = a & 15, bg = (a >> 4) & 7;
         const uint8_t *g16 = cp437_glyph(ch);
         const uint8_t *g = (ch >= 32) ? font8x8_data[ch - 32] : NULL;
         for (y = 0; y < CH; y++) {
            int bits = g16 ? g16[y] : g ? g[y / 2] : 0;
            uint8_t *d = pixels + (size_t)(cy * CH + y) * TW * CW + cx * CW;
            for (x = 0; x < CW; x++)
               d[x] = (uint8_t)((bits & (0x80 >> x)) ? fg : bg);
         }
      }
   *px = pixels;
   *w = TW * CW;
   *h = TH * CH;
}

/* ---------------------------------------------------------------- conio API */

static void out_str(const char *s)
{
   while (*s)
      sys_textmode_putc((unsigned char)*s++);
}

void cprintf(const char *fmt, ...)
{
   char buf[1024];
   va_list ap;
   va_start(ap, fmt);
   vsnprintf(buf, sizeof(buf), fmt, ap);
   va_end(ap);
   out_str(buf);
}

void tprintf(const char *fmt, ...)
{
   char buf[1024];
   va_list ap;
   va_start(ap, fmt);
   vsnprintf(buf, sizeof(buf), fmt, ap);
   va_end(ap);
   keep_attr = 1;
   out_str(buf);
   keep_attr = 0;
}

void gotoxy(int x, int y) { sys_textmode_gotoxy(x, y); }
void textcolor(int c) { sys_textmode_color(c, -1); }
void textbackground(int c) { sys_textmode_color(-1, c); }
void clreol(void) { sys_textmode_clreol(); }
void clrscr(void) { sys_textmode_clrscr(); }
