/* Keyboard, mouse and joystick emulation with deterministic record/replay.
 *
 * Every input change is applied at the start of a sys_idle() call, stamped with the
 * emulated clock. A recording therefore replays bit-exactly (given the same RNG seed). */
#include "alleg.h"
#include "internal.h"
#include "input.h"
#include "../platform/sys.h"
#include "../platform/net.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

volatile char key[128];
volatile int key_shifts;
volatile int mouse_x, mouse_y, mouse_b;
int joy_type;
JOYSTICK_STATE joy[MAX_PADS + 1];
int num_joysticks;

#define KEY_BUFFER_SIZE 64
static int key_buffer[KEY_BUFFER_SIZE];
static volatile int key_buffer_start, key_buffer_end;

static FILE *rec_file;
static FILE *rep_file;
static int replaying;
static uint32_t rep_seed;
static int rep_has_seed;

/* pending replay event */
static struct {
   int valid;
   unsigned long long clock;
   char kind;
   int a, b, c;
} rep_ev;

/* live gamepad state (sampled each idle), copied to joy[] on poll_joystick() */
static JOYSTICK_STATE live_pad[MAX_PADS];
static SDL_Gamepad *pads[MAX_PADS];

static void read_next_event(void);
static void net_queue(int kind, int a, int b, int c);

static float mouse_fx, mouse_fy;
static int mouse_range_w = 320, mouse_range_h = 200;

/* ---------------------------------------------------------------- recording helpers */

static void rec(const char *fmt, ...)
{
   va_list ap;
   if (!rec_file)
      return;
   fprintf(rec_file, "%llu ", (unsigned long long)sys_clock());
   va_start(ap, fmt);
   vfprintf(rec_file, fmt, ap);
   va_end(ap);
   fputc('\n', rec_file);
}

/* ---------------------------------------------------------------- keyboard */

static void push_key(int v)
{
   int c = key_buffer_end + 1;
   if (c >= KEY_BUFFER_SIZE)
      c = 0;
   if (c != key_buffer_start) {
      key_buffer[key_buffer_end] = v;
      key_buffer_end = c;
   }
}

static void apply_key(int sc, int down, int ascii)
{
   if (sc <= 0 || sc >= 128)
      return;
   key[sc] = down ? TRUE : FALSE;
   if (sc == KEY_LSHIFT || sc == KEY_RSHIFT) {
      if (down) key_shifts |= 1; else key_shifts &= ~1;
   }
   if (down)
      push_key((sc << 8) | (ascii & 0xFF));
}

/* A press and release arriving in the same event batch would never be seen by the
 * game's polling loops (on DOS the busy loop catches even short taps), so releases of
 * keys pressed in the current batch are deferred to the next pump. */
static unsigned char pressed_in_batch[128], deferred_release[128];

void _input_begin_pump(void)
{
   int i;
   for (i = 0; i < 128; i++) {
      if (deferred_release[i]) {
         deferred_release[i] = 0;
         rec("k %d 0 0", i);
         apply_key(i, 0, 0);
      }
      pressed_in_batch[i] = 0;
   }
}

void _keyboard_event(int scancode, int down, int ascii, int repeat)
{
   (void)repeat;
   if (scancode <= 0 || scancode >= 128)
      return;
   if (net_session) {
      net_queue('k', scancode, down, ascii);
      return;
   }
   if (!down && pressed_in_batch[scancode]) {
      deferred_release[scancode] = 1;
      return;
   }
   if (down) {
      pressed_in_batch[scancode] = 1;
      deferred_release[scancode] = 0;
   }
   rec("k %d %d %d", scancode, down, ascii);
   apply_key(scancode, down, ascii);
}

int install_keyboard(void)
{
   return 0;
}

int keypressed(void)
{
   return key_buffer_start != key_buffer_end;
}

/* Gamepad text entry while the game waits in readkey() (high score names): up/down pick a
 * character, A (or right) types it, B (or left) deletes, Y = Enter, Start = space.
 * The picker is drawn by the platform layer on top of the frame (sys_overlay), not into
 * the game's screen, and only produces ordinary key presses. */
static const char vk_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-. ";
static int vk_pos, vk_prev, vk_repeat;

static int vk_scancode(char c)
{
   static const char row1[] = "QWERTYUIOP", row2[] = "ASDFGHJKL", row3[] = "ZXCVBNM";
   const char *q;
   if ((q = strchr(row1, c)) && c) return KEY_Q + (int)(q - row1);
   if ((q = strchr(row2, c)) && c) return KEY_A + (int)(q - row2);
   if ((q = strchr(row3, c)) && c) return KEY_Z + (int)(q - row3);
   if (c >= '1' && c <= '9') return KEY_1 + (c - '1');
   if (c == '0') return KEY_0;
   if (c == '-') return KEY_MINUS;
   if (c == '.') return KEY_STOP;
   return KEY_SPACE;
}

static void vk_update(void)
{
   int i, bits = 0, edge, n = (int)sizeof(vk_chars) - 1;
   for (i = 0; i < MAX_PADS; i++)
      bits |= live_pad[i].bits;
   edge = bits & ~vk_prev;
   if (bits & (PAD_UP | PAD_DOWN)) {
      if (edge & (PAD_UP | PAD_DOWN))
         vk_repeat = 12;                 /* first repeat after 12 idles (~170 ms) */
      else if (--vk_repeat <= 0) {
         vk_repeat = 4;
         edge |= bits & (PAD_UP | PAD_DOWN);
      }
   }
   vk_prev = bits;
   if (edge & PAD_UP) vk_pos = (vk_pos + n - 1) % n;
   if (edge & PAD_DOWN) vk_pos = (vk_pos + 1) % n;
   if (edge & (PAD_B1 | PAD_RIGHT)) {
      char c = vk_chars[vk_pos];
      apply_key(vk_scancode(c), 1, c);
      apply_key(vk_scancode(c), 0, 0);
   }
   if (edge & (PAD_B2 | PAD_LEFT)) {
      apply_key(KEY_BACKSPACE, 1, 8);
      apply_key(KEY_BACKSPACE, 0, 0);
   }
   if (num_joysticks > 0)
      snprintf(sys_overlay, sizeof(sys_overlay), "< %c >  A:type B:delete Y:done",
               vk_chars[vk_pos] == ' ' ? '_' : vk_chars[vk_pos]);
}

int readkey(void)
{
   int r;
   sys_sync_enter('K');
   vk_prev = ~0;                         /* ignore buttons already held */
   while (key_buffer_start == key_buffer_end) {
      sys_idle();
      vk_update();
   }
   sys_overlay[0] = 0;
   r = key_buffer[key_buffer_start];
   key_buffer_start++;
   if (key_buffer_start >= KEY_BUFFER_SIZE)
      key_buffer_start = 0;
   return sys_sync_leave(r);
}

void clear_keybuf(void)
{
   int c;
   key_buffer_start = 0;
   key_buffer_end = 0;
   for (c = 0; c < 128; c++)
      key[c] = FALSE;
}

void simulate_keypress(int k)
{
   push_key(k);
}

/* ---------------------------------------------------------------- mouse */

int install_mouse(void)
{
   mouse_x = mouse_range_w / 2;
   mouse_y = mouse_range_h / 2;
   return 2;
}

static void apply_mouse_move(int dx, int dy)
{
   int x = mouse_x + dx, y = mouse_y + dy;
   mouse_x = MID(0, x, mouse_range_w - 1);
   mouse_y = MID(0, y, mouse_range_h - 1);
}

void _mouse_motion(float dx, float dy)
{
   int ix, iy;
   mouse_fx += dx * 0.5f;
   mouse_fy += dy * 0.5f;
   ix = (int)mouse_fx;
   iy = (int)mouse_fy;
   mouse_fx -= (float)ix;
   mouse_fy -= (float)iy;
   if ((ix || iy) && net_session)
      net_queue('m', ix, iy, 0);
   else if (ix || iy) {
      rec("m %d %d", ix, iy);
      apply_mouse_move(ix, iy);
   }
}

void _mouse_button(int button, int down)
{
   int bit = button == 1 ? 1 : button == 3 ? 2 : button == 2 ? 4 : 0;
   int nb;
   if (!bit)
      return;
   if (net_session) {
      static int local_b;
      nb = down ? (local_b | bit) : (local_b & ~bit);
      if (nb != local_b)
         net_queue('b', local_b = nb, 0, 0);
      return;
   }
   nb = down ? (mouse_b | bit) : (mouse_b & ~bit);
   if (nb != mouse_b) {
      rec("b %d", nb);
      mouse_b = nb;
   }
}

/* ---------------------------------------------------------------- joystick */

void _gamepad_hotplug(void)
{
   SDL_JoystickID *ids;
   int n, i, old = num_joysticks;
   if (replaying && !net_session) {
      /* the pad count comes from the replay ('n' events, recorded right here) */
      while (rep_ev.valid && rep_ev.kind == 'n' && rep_ev.clock <= sys_clock()) {
         num_joysticks = MID(0, rep_ev.a, MAX_PADS);
         read_next_event();
      }
      return;
   }
   for (i = 0; i < MAX_PADS; i++) {
      if (pads[i] && !SDL_GamepadConnected(pads[i])) {
         SDL_CloseGamepad(pads[i]);
         pads[i] = NULL;
      }
   }
   ids = SDL_GetGamepads(&n);
   for (i = 0; ids && i < n; i++) {
      int j, have = 0, slot = -1;
      for (j = 0; j < MAX_PADS; j++) {
         if (pads[j] && SDL_GetGamepadID(pads[j]) == ids[i]) have = 1;
         if (!pads[j] && slot < 0) slot = j;
      }
      if (!have && slot >= 0)
         pads[slot] = SDL_OpenGamepad(ids[i]);
   }
   SDL_free(ids);
   if (net_session) {
      num_joysticks = MAX_PADS - 1;      /* fixed in a session: as recorded ("n 3" at 0) */
      return;
   }
   num_joysticks = 0;
   for (i = 0; i < MAX_PADS; i++)
      if (pads[i])
         num_joysticks = i + 1;
   if (num_joysticks != old)
      rec("n %d", num_joysticks);
}

static int pad_bits(SDL_Gamepad *g)
{
   int bits = 0;
   const int dead = 12000;
   int ax, ay;
   if (!g)
      return 0;
   ax = SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFTX);
   ay = SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFTY);
   if (ax < -dead || SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_DPAD_LEFT)) bits |= PAD_LEFT;
   if (ax > dead || SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) bits |= PAD_RIGHT;
   if (ay < -dead || SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_DPAD_UP)) bits |= PAD_UP;
   if (ay > dead || SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_DPAD_DOWN)) bits |= PAD_DOWN;
   /* button layout: b1 = fire (south), b2 = east, b3 = west, b4 = north */
   if (SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_SOUTH)) bits |= PAD_B1;
   if (SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_EAST)) bits |= PAD_B2;
   if (SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_WEST)) bits |= PAD_B3;
   if (SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_NORTH)) bits |= PAD_B4;
   if (SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) bits |= PAD_B5;
   if (SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)) bits |= PAD_B6;
   if (SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_START)) bits |= PAD_START;
   if (SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_BACK)) bits |= PAD_BACK;
   return bits;
}

static void apply_key(int sc, int down, int ascii);

/* pad buttons that act as keys (for pad-only play; the game never reads these buttons):
 * Start = Pause + Space (pause in game, confirm in menus / setup screen), Back = Esc,
 * Y (north) = Enter (e.g. "YES (enter)"). Driven by the OR of all pads. */
static int pad_key_bits;

static void pad_keys(void)
{
   int i, any = 0, changed;
   for (i = 0; i < MAX_PADS; i++)
      any |= live_pad[i].bits;
   any &= PAD_START | PAD_BACK | PAD_B4;
   changed = pad_key_bits ^ any;
   pad_key_bits = any;
   if (changed & PAD_START) {
      apply_key(KEY_PAUSE, (any & PAD_START) != 0, 0);
      apply_key(KEY_SPACE, (any & PAD_START) != 0, (any & PAD_START) ? ' ' : 0);
   }
   if (changed & PAD_BACK)
      apply_key(KEY_ESC, (any & PAD_BACK) != 0, (any & PAD_BACK) ? 27 : 0);
   if (changed & PAD_B4)
      apply_key(KEY_ENTER, (any & PAD_B4) != 0, (any & PAD_B4) ? 13 : 0);
}

static void set_pad_state(JOYSTICK_STATE *s, int bits)
{
   s->bits = bits;
   s->left = (bits & PAD_LEFT) != 0;
   s->right = (bits & PAD_RIGHT) != 0;
   s->up = (bits & PAD_UP) != 0;
   s->down = (bits & PAD_DOWN) != 0;
   s->b[0] = (bits & PAD_B1) != 0;
   s->b[1] = (bits & PAD_B2) != 0;
   s->b[2] = (bits & PAD_B3) != 0;
   s->b[3] = (bits & PAD_B4) != 0;
   s->b[4] = (bits & PAD_B5) != 0;
   s->b[5] = (bits & PAD_B6) != 0;
   s->b[6] = (bits & PAD_START) != 0;
   s->b[7] = (bits & PAD_BACK) != 0;
   if (s >= live_pad && s < live_pad + MAX_PADS)
      pad_keys();
}

int install_joystick(int type)
{
   joy_type = type;
   sys_sync_enter('J');
   _gamepad_hotplug();
   return sys_sync_leave(num_joysticks > 0 ? 0 : -1);
}

int poll_joystick(void)
{
   int i, any = 0;
   for (i = 0; i < MAX_PADS; i++) {
      joy[i] = live_pad[i];
      any |= live_pad[i].bits;
   }
   set_pad_state(&joy[JOY_ANY], any);
   return 0;
}

/* ---------------------------------------------------------------- replay */

static void read_next_event(void)
{
   char line[256];
   rep_ev.valid = 0;
   while (rep_file && fgets(line, sizeof(line), rep_file)) {
      unsigned long long clk;
      char kind;
      int a = 0, b = 0, c = 0;
      if (line[0] == '#' || line[0] == '\n')
         continue;
      if (sscanf(line, "%llu %c %d %d %d", &clk, &kind, &a, &b, &c) >= 2) {
         rep_ev.valid = 1;
         rep_ev.clock = clk;
         rep_ev.kind = kind;
         rep_ev.a = a;
         rep_ev.b = b;
         rep_ev.c = c;
         return;
      }
   }
}

int _input_replaying(void)
{
   return replaying;
}

static void net_tick(void);

void _input_tick_hook(void)
{
   int i;
   if (net_session) {
      net_tick();
      return;
   }
   if (replaying) {
      while (rep_ev.valid && rep_ev.clock <= sys_clock()) {
         switch (rep_ev.kind) {
            case 'k': apply_key(rep_ev.a, rep_ev.b, rep_ev.c); break;
            case 'm': apply_mouse_move(rep_ev.a, rep_ev.b); break;
            case 'b': mouse_b = rep_ev.a; break;
            case 'j':
               if (rep_ev.a >= 0 && rep_ev.a < MAX_PADS)
                  set_pad_state(&live_pad[rep_ev.a], rep_ev.b);
               break;
            case 'n': num_joysticks = MID(0, rep_ev.a, MAX_PADS); break;
            case 'q': sys_quit_requested = 1; break;
            default: break;
         }
         read_next_event();
      }
      if (!rep_ev.valid && rep_file) {
         fclose(rep_file);
         rep_file = NULL;
         replaying = 0;
         sys_log("replay finished at clock %llu", (unsigned long long)sys_clock());
      }
      return;
   }
   /* on changes of the physical pad only, like keys: after a replay ends, what it
      held stays held (as in the session it was recorded from) until the pad moves */
   for (i = 0; i < MAX_PADS; i++) {
      static int phys_bits[MAX_PADS];
      int bits = pad_bits(pads[i]);
      if (bits != phys_bits[i]) {
         phys_bits[i] = bits;
         if (bits != live_pad[i].bits) {
            rec("j %d %d", i, bits);
            set_pad_state(&live_pad[i], bits);
         }
      }
   }
}

/* ---------------------------------------------------------------- netplay (net.h)
 * In a net session nothing local touches the game's input state directly: this
 * machine's events (or a --replay file, for tests) are queued per frame and sent, and
 * net_tick() applies both players' frames at the first pump of each frame, host's events
 * first. Per-source key/button/pad state is OR-ed, so one side releasing does not cancel
 * what the other holds. Applied events go to --record: the merged stream is a replay. */

int (*input_net_map_key)(int sc);      /* sender side: this player's scancode, <= 0 = drop */
int (*input_net_pad_slot)(void);       /* sender side: virtual pad for this player's pads */
uint32_t (*input_net_hash)(void);      /* complete game state (sync_state) */

#define NET_HASH_EVERY 35              /* frames: twice a second */
#define NET_RING 64
static NetEvent pend[NET_MAX_EVENTS], carry[NET_MAX_EVENTS];
static int npend, ncarry;
static unsigned char pressed_in_frame[128];
static NetEvent local_ring[NET_RING][NET_MAX_EVENTS];
static int local_ring_n[NET_RING];
static uint32_t next_frame;
static unsigned char src_key[2][128];
static int src_mb[2], src_pad[2][MAX_PADS];
static int local_pad_bits, local_pad_slot, script_pad[MAX_PADS];

static void net_push(NetEvent *list, int *n, int kind, int a, int b, int c)
{
   if (*n >= NET_MAX_EVENTS)
      return;
   list[*n].kind = (uint8_t)kind;
   list[*n].a = (int16_t)a;
   list[*n].b = (int16_t)b;
   list[*n].c = (uint8_t)c;
   (*n)++;
}

static void net_queue(int kind, int a, int b, int c)
{
   if (kind == 'k') {
      if (input_net_map_key)
         a = input_net_map_key(a);
      if (a <= 0 || a >= 128)
         return;
      /* a tap within one frame: release in the next one, or the game never sees it */
      if (!b && pressed_in_frame[a]) {
         net_push(carry, &ncarry, 'k', a, 0, 0);
         return;
      }
      if (b)
         pressed_in_frame[a] = 1;
   }
   if (kind == 'm' && npend && pend[npend - 1].kind == 'm' &&
       abs(pend[npend - 1].a + a) < 30000 && abs(pend[npend - 1].b + b) < 30000) {
      pend[npend - 1].a = (int16_t)(pend[npend - 1].a + a);
      pend[npend - 1].b = (int16_t)(pend[npend - 1].b + b);
      return;
   }
   net_push(pend, &npend, kind, a, b, c);
}

static void net_apply(int s, const NetEvent *e)
{
   int v;
   switch (e->kind) {
      case 'k':
         if (e->a <= 0 || e->a >= 128)
            break;
         src_key[s][e->a] = e->b != 0;
         if (e->b) {
            rec("k %d 1 %d", e->a, e->c);
            apply_key(e->a, 1, e->c);
         }
         else if (!src_key[s ^ 1][e->a]) {
            rec("k %d 0 0", e->a);
            apply_key(e->a, 0, 0);
         }
         break;
      case 'm':
         rec("m %d %d", e->a, e->b);
         apply_mouse_move(e->a, e->b);
         break;
      case 'b':
         src_mb[s] = e->a & 7;
         v = src_mb[0] | src_mb[1];
         if (v != mouse_b) {
            rec("b %d", v);
            mouse_b = v;
         }
         break;
      case 'j':
         if (e->a < 0 || e->a >= MAX_PADS)
            break;
         src_pad[s][e->a] = e->b & 0xffff;
         v = src_pad[0][e->a] | src_pad[1][e->a];
         if (v != live_pad[e->a].bits) {
            rec("j %d %d", e->a, v);
            set_pad_state(&live_pad[e->a], v);
         }
         break;
      case 'q':
         rec("q");
         sys_quit_requested = 1;
         break;
      default:
         break;
   }
}

static void net_tick(void)
{
   uint32_t cur = (uint32_t)(sys_clock() / NET_FRAME_PIT);
   int i, bits = 0, slot;
   num_joysticks = MAX_PADS - 1;
   if (replaying) {
      /* a replay file is this player's input (tests) */
      while (rep_ev.valid && rep_ev.clock <= sys_clock()) {
         switch (rep_ev.kind) {
            case 'k': net_queue('k', rep_ev.a, rep_ev.b, rep_ev.c); break;
            case 'm': net_queue('m', rep_ev.a, rep_ev.b, 0); break;
            case 'b': net_queue('b', rep_ev.a, 0, 0); break;
            case 'j':
               if (rep_ev.a >= 0 && rep_ev.a < MAX_PADS)
                  script_pad[rep_ev.a] = rep_ev.b;
               break;
            case 'q': net_queue('q', 0, 0, 0); break;
            default: break;
         }
         read_next_event();
      }
      for (i = 0; i < MAX_PADS; i++)
         bits |= script_pad[i];
      if (!rep_ev.valid && rep_file) {
         fclose(rep_file);
         rep_file = NULL;
         replaying = 0;
      }
   }
   else
      for (i = 0; i < MAX_PADS; i++)
         bits |= pad_bits(pads[i]);
   slot = input_net_pad_slot ? input_net_pad_slot() : 0;
   if (slot != local_pad_slot) {
      if (local_pad_bits)
         net_queue('j', local_pad_slot, 0, 0);
      local_pad_slot = slot;
      local_pad_bits = 0;
   }
   if (bits != local_pad_bits) {
      net_queue('j', slot, bits, 0);
      local_pad_bits = bits;
   }
   while (next_frame <= cur) {
      static NetEvent remote[NET_MAX_EVENTS];
      uint32_t f = next_frame++, out = f + (uint32_t)net_delay, hf = 0xffffffffu, h = 0;
      int nr = 0, nl, r;
      if (f % NET_HASH_EVERY == 0 && input_net_hash) {
         hf = f;
         h = input_net_hash();
         net_local_hash(f, h);
      }
      /* close this machine's frame f + delay and send it */
      memcpy(local_ring[out % NET_RING], pend, sizeof(NetEvent) * (size_t)npend);
      local_ring_n[out % NET_RING] = npend;
      net_send_frame(out, pend, npend, hf, h);
      memcpy(pend, carry, sizeof(NetEvent) * (size_t)ncarry);
      npend = ncarry;
      ncarry = 0;
      memset(pressed_in_frame, 0, sizeof(pressed_in_frame));
      for (i = 0; i < npend; i++)
         if (pend[i].kind == 'k' && pend[i].b)
            pressed_in_frame[pend[i].a] = 1;
      /* frame f: both sides' events (frames below the delay are empty everywhere) */
      nl = f < (uint32_t)net_delay ? 0 : local_ring_n[f % NET_RING];
      if (f >= (uint32_t)net_delay) {
         r = net_get_frame(f, remote, NET_MAX_EVENTS);
         nr = r < 0 ? 0 : r;
      }
      for (r = 0; r < 2; r++) {
         int from_local = (r == net_me);
         const NetEvent *ev = from_local ? local_ring[f % NET_RING] : remote;
         int n = from_local ? nl : nr;
         for (i = 0; i < n; i++)
            net_apply(r, &ev[i]);
      }
   }
}

uint32_t input_random_seed(uint32_t live_seed)
{
   sys_sync_enter('B');
   return (uint32_t)sys_sync_leave((int)(rep_has_seed ? rep_seed : live_seed));
}

/* sync log (re/refemu): pending key buffer entries and live pad state */
int _input_keybuf(int *out, int max)
{
   int n = 0, i = key_buffer_start;
   while (i != key_buffer_end && n < max) {
      out[n++] = key_buffer[i];
      if (++i >= KEY_BUFFER_SIZE)
         i = 0;
   }
   return n;
}

int _input_pad_bits(int pad)
{
   return live_pad[pad].bits;
}

void input_init_recording(void)
{
   if (sys_cfg.replay_path) {
      char line[256];
      rep_file = fopen(sys_cfg.replay_path, "rb")   /* binary: text-mode ftell/fseek break on LF files (Windows) */;
      if (!rep_file) {
         sys_log("cannot open replay %s", sys_cfg.replay_path);
      }
      else {
         long pos = ftell(rep_file);
         while (fgets(line, sizeof(line), rep_file)) {
            unsigned s;
            if (sscanf(line, "seed %u", &s) == 1) {
               if (!net_session) {           /* a net session's seed comes from the host */
                  rep_seed = s;
                  rep_has_seed = 1;
               }
               pos = ftell(rep_file);
               continue;
            }
            if (line[0] == '#') {
               pos = ftell(rep_file);
               continue;
            }
            break;
         }
         fseek(rep_file, pos, SEEK_SET);
         replaying = 1;
         read_next_event();
      }
   }
   if (sys_cfg.record_path) {
      rec_file = fopen(sys_cfg.record_path, "wb");
      if (!rep_has_seed) {
         rep_seed = (uint32_t)(SDL_GetTicksNS() ^ (uint64_t)time(NULL));
         rep_has_seed = 1;
      }
      if (rec_file)
         fprintf(rec_file, "# Spacehog input recording v1\nseed %u\n", rep_seed);
   }
   if (net_session)   /* fixed pad count: both players may pick a pad, wherever it is */
      rec("n %d", MAX_PADS - 1);
}

void input_net_start(uint32_t seed)
{
   rep_seed = seed;
   rep_has_seed = 1;
}
