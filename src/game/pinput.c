/* Per-player input layer: see "player input" in sh.h. */
#include "sh.h"

int player_input_latched;
static PLAYER_INPUT latched[2];
static int have_latch;

/* the original's pad choice: own pad only if both players use one, else pad 0 */
static JOYSTICK_STATE *pjoy(int p)
{
   return &joy[(g_player[0].control == CTRL_PAD && g_player[1].control == CTRL_PAD) ? p : 0];
}

PLAYER_INPUT player_input_sample(int p)
{
   PLAYER_INPUT in;
   JOYSTICK_STATE *j = pjoy(p);
   int k;
   in.bits = 0;
   for (k = PK_UP; k <= PK_SPECIAL; k++)
      if (key[g_keys[p * 7 + k]])
         in.bits |= 1u << k;
   if (j->up) in.bits |= PI_PAD_UP;
   if (j->down) in.bits |= PI_PAD_DOWN;
   if (j->left) in.bits |= PI_PAD_LEFT;
   if (j->right) in.bits |= PI_PAD_RIGHT;
   if (j->b[0]) in.bits |= PI_PAD_B1;
   if (j->b[1]) in.bits |= PI_PAD_B2;
   if (j->b[2]) in.bits |= PI_PAD_B3;
   if (key[0x1c]) in.bits |= PI_ENTER;
   in.bits |= (unsigned int)(mouse_b & 7) << PI_MOUSE_SHIFT;
   in.mouse_x = mouse_x;
   in.mouse_y = mouse_y;
   return in;
}

void player_input_latch(void)
{
   if (!player_input_latched)
      return;
   latched[0] = player_input_sample(0);
   latched[1] = player_input_sample(1);
   have_latch = 1;
}

PLAYER_INPUT player_input(int p)
{
   if (!player_input_latched)
      return player_input_sample(p);
   if (!have_latch)
      player_input_latch();
   return latched[p];
}
