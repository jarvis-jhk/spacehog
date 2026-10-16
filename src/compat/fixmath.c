/* Fixed point maths, bit-exact with Allegro 3.11 (DJGPP build). */
#include "alleg.h"

int allegro_errno;
extern const unsigned short _sqrt_table[256];

fixed fmul(fixed x, fixed y)
{
   int64_t r = (int64_t)x * (int64_t)y;
   uint16_t top = (uint16_t)((uint64_t)r >> 48);
   /* same (loose) overflow test as the DJGPP build: only bits 48..63 are checked */
   if (top != 0 && top != 0xFFFF) {
      /* overflow, same test and result as the asm version */
      fixed v = 0x7FFFFFFF;
      allegro_errno = 2;
      if (x < 0) v = -0x7FFFFFFF;
      if (y < 0) v = -v;
      return v;
   }
   return (fixed)(uint32_t)((uint64_t)r >> 16);
}

fixed fdiv(fixed x, fixed y)
{
   uint32_t ux, uy;
   int neg = 0;
   int32_t q;
   if (x < 0) { ux = (uint32_t)-x; neg = !neg; } else ux = (uint32_t)x;
   if (y < 0) { uy = (uint32_t)-y; neg = !neg; } else uy = (uint32_t)y;
   if ((ux >> 16) >= uy || (q = (int32_t)((((uint64_t)(ux >> 16) << 32) | (uint32_t)(ux << 16)) / uy)) < 0) {
      allegro_errno = 2;
      return neg ? -0x7FFFFFFF : 0x7FFFFFFF;
   }
   return neg ? -q : q;
}

fixed fatan(fixed x)
{
   int a, b, c = 0;
   fixed d;
   if (x >= 0) { a = 0; b = 127; }
   else { a = 128; b = 255; }
   do {
      c = (a + b) >> 1;
      d = x - _tan_tbl[c];
      if (d > 0)
         a = c + 1;
      else if (d < 0)
         b = c - 1;
   } while ((a <= b) && (d));
   if (x >= 0)
      return ((fixed)c) << 15;
   return (-0x00800000 + (((fixed)c) << 15));
}

fixed fatan2(fixed y, fixed x)
{
   fixed r;
   if (x == 0) {
      if (y == 0)
         return 0;
      return (y < 0) ? -0x00400000 : 0x00400000;
   }
   allegro_errno = 0;
   r = fdiv(y, x);
   if (allegro_errno) {
      allegro_errno = 0;
      return (y < 0) ? -0x00400000 : 0x00400000;
   }
   r = fatan(r);
   if (x >= 0)
      return r;
   if (y >= 0)
      return 0x00800000 + r;
   return r - 0x00800000;
}

/* C transcription of the i386 asm fsqrt (table lookup, not libm) */
fixed fsqrt(fixed x)
{
   uint32_t v;
   int cl = 0, dl;
   if (x <= 0)
      return 0;
   v = (uint32_t)x;
   if (v >= 0x10000) { v >>= 16; cl += 16; }
   if (v >= 0x100) { v >>= 8; cl += 8; }
   if (v >= 0x10) { v >>= 4; cl += 4; }
   if (v >= 0x4) { v >>= 2; cl += 2; }
   if (v >= 2) cl++;
   v = (uint32_t)x;
   cl = (int8_t)(cl - 15);
   if (cl & 1) cl = (int8_t)(cl + 1);
   dl = cl;
   cl = (int8_t)(cl + 4);
   if (cl < 0)
      v <<= -cl;
   else
      v >>= cl;
   dl >>= 1;
   v >>= 4;
   v = _sqrt_table[v & 0xFF] | (v & 0xFFFF0000u);
   if (dl >= 0)
      v <<= dl;
   else
      v >>= -dl;
   return (fixed)v;
}
