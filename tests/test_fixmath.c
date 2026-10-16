#include "../src/compat/alleg.h"
#include <stdio.h>
#include <math.h>
int main(void)
{
   fixed xs[] = { 46400, 65536, 4 * 65536, 100 * 65536, 12345, 1, 0x7fffffff, 3 };
   unsigned i;
   for (i = 0; i < sizeof(xs) / sizeof(xs[0]); i++)
      printf("fsqrt(%d) = %d  (float %f)\n", xs[i], fsqrt(xs[i]), sqrt(xs[i] / 65536.0) * 65536.0);
   printf("fatan2(5,12)=%d fmul(3.5,2)=%d fdiv(1,3)=%d\n", fatan2(itofix(5), itofix(12)), fmul(229376, 131072), fdiv(65536, 196608));
   return 0;
}
