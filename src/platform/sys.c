/* SDL3 platform layer. */
#include "sys.h"
#include "net.h"
#include "../compat/alleg.h"
#include "../compat/internal.h"
#include "../compat/dos.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

SysConfig sys_cfg = { .music_gm = -1 };
volatile int sys_quit_requested;

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static int tex_w, tex_h;
static uint32_t *argb;
static SDL_AudioStream *audio_stream;
static sys_audio_cb audio_cb;
static void *audio_user;
static int audio_rate;

/* ---------------------------------------------------------------- clock */

#define PIT_HZ 1193181ULL
#define RETRACE_PIT 17025ULL /* 1193181 / 70.086 Hz */

static uint64_t virt_clock;     /* emulated time in PIT ticks */
static uint64_t real_base_ns;   /* SDL ns at virt_clock == 0 (real-time mode) */
static uint64_t next_retrace;

uint64_t sys_clock(void)
{
   return virt_clock;
}

static uint64_t real_now_pit(void)
{
   uint64_t ns = SDL_GetTicksNS() - real_base_ns;
   return ns * PIT_HZ / 1000000000ULL;
}

/* advance the emulated clock to t, firing timers along the way */
/* SPACEHOG_AUDIO_DUMP=file.wav (headless runs): render the game's audio offline along
 * the emulated clock, for audio tests and comparisons with DOSBox captures */
static FILE *audio_dump;
static uint64_t audio_dump_frames;

static void audio_dump_wav_header(uint32_t frames)
{
   uint32_t v, data = frames * 4;
   uint16_t h[2];
   fseek(audio_dump, 0, SEEK_SET);
   fwrite("RIFF", 1, 4, audio_dump); v = 36 + data; fwrite(&v, 4, 1, audio_dump);
   fwrite("WAVEfmt ", 1, 8, audio_dump); v = 16; fwrite(&v, 4, 1, audio_dump);
   h[0] = 1; h[1] = 2; fwrite(h, 2, 2, audio_dump);
   v = 44100; fwrite(&v, 4, 1, audio_dump); v = 44100 * 4; fwrite(&v, 4, 1, audio_dump);
   h[0] = 4; h[1] = 16; fwrite(h, 2, 2, audio_dump);
   fwrite("data", 1, 4, audio_dump); fwrite(&data, 4, 1, audio_dump);
   fseek(audio_dump, 0, SEEK_END);
}

static void audio_dump_advance(void)
{
   static int16_t buf[4096 * 2];
   uint64_t due = virt_clock * 44100 / PIT_HZ;
   while (audio_dump_frames < due) {
      int n = (int)(due - audio_dump_frames > 4096 ? 4096 : due - audio_dump_frames);
      sound_render(buf, n);
      fwrite(buf, 4, (size_t)n, audio_dump);
      audio_dump_frames += (uint64_t)n;
   }
}

static void advance_to(uint64_t t)
{
   while (virt_clock < t) {
      uint64_t step = _timer_next_event(virt_clock);
      if (step > t)
         step = t;
      if (step <= virt_clock)
         step = virt_clock + 1;
      virt_clock = step;
      _timer_fire(virt_clock);
   }
   if (audio_dump)
      audio_dump_advance();
}

/* ---------------------------------------------------------------- keyboard map */

static int sdl_to_pc(SDL_Scancode s)
{
   switch (s) {
      case SDL_SCANCODE_ESCAPE: return KEY_ESC;
      case SDL_SCANCODE_1: return KEY_1;
      case SDL_SCANCODE_2: return KEY_2;
      case SDL_SCANCODE_3: return KEY_3;
      case SDL_SCANCODE_4: return KEY_4;
      case SDL_SCANCODE_5: return KEY_5;
      case SDL_SCANCODE_6: return KEY_6;
      case SDL_SCANCODE_7: return KEY_7;
      case SDL_SCANCODE_8: return KEY_8;
      case SDL_SCANCODE_9: return KEY_9;
      case SDL_SCANCODE_0: return KEY_0;
      case SDL_SCANCODE_MINUS: return KEY_MINUS;
      case SDL_SCANCODE_EQUALS: return KEY_EQUALS;
      case SDL_SCANCODE_BACKSPACE: return KEY_BACKSPACE;
      case SDL_SCANCODE_TAB: return KEY_TAB;
      case SDL_SCANCODE_Q: return KEY_Q;
      case SDL_SCANCODE_W: return KEY_W;
      case SDL_SCANCODE_E: return KEY_E;
      case SDL_SCANCODE_R: return KEY_R;
      case SDL_SCANCODE_T: return KEY_T;
      case SDL_SCANCODE_Y: return KEY_Y;
      case SDL_SCANCODE_U: return KEY_U;
      case SDL_SCANCODE_I: return KEY_I;
      case SDL_SCANCODE_O: return KEY_O;
      case SDL_SCANCODE_P: return KEY_P;
      case SDL_SCANCODE_LEFTBRACKET: return KEY_OPENBRACE;
      case SDL_SCANCODE_RIGHTBRACKET: return KEY_CLOSEBRACE;
      case SDL_SCANCODE_RETURN: return KEY_ENTER;
      case SDL_SCANCODE_KP_ENTER: return KEY_ENTER;
      case SDL_SCANCODE_LCTRL: return KEY_CONTROL;
      case SDL_SCANCODE_RCTRL: return KEY_RCONTROL;
      case SDL_SCANCODE_A: return KEY_A;
      case SDL_SCANCODE_S: return KEY_S;
      case SDL_SCANCODE_D: return KEY_D;
      case SDL_SCANCODE_F: return KEY_F;
      case SDL_SCANCODE_G: return KEY_G;
      case SDL_SCANCODE_H: return KEY_H;
      case SDL_SCANCODE_J: return KEY_J;
      case SDL_SCANCODE_K: return KEY_K;
      case SDL_SCANCODE_L: return KEY_L;
      case SDL_SCANCODE_SEMICOLON: return KEY_COLON;
      case SDL_SCANCODE_APOSTROPHE: return KEY_QUOTE;
      case SDL_SCANCODE_GRAVE: return KEY_TILDE;
      case SDL_SCANCODE_LSHIFT: return KEY_LSHIFT;
      case SDL_SCANCODE_BACKSLASH: return KEY_BACKSLASH;
      case SDL_SCANCODE_Z: return KEY_Z;
      case SDL_SCANCODE_X: return KEY_X;
      case SDL_SCANCODE_C: return KEY_C;
      case SDL_SCANCODE_V: return KEY_V;
      case SDL_SCANCODE_B: return KEY_B;
      case SDL_SCANCODE_N: return KEY_N;
      case SDL_SCANCODE_M: return KEY_M;
      case SDL_SCANCODE_COMMA: return KEY_COMMA;
      case SDL_SCANCODE_PERIOD: return KEY_STOP;
      case SDL_SCANCODE_SLASH: return KEY_SLASH;
      case SDL_SCANCODE_RSHIFT: return KEY_RSHIFT;
      case SDL_SCANCODE_KP_MULTIPLY: return KEY_ASTERISK;
      case SDL_SCANCODE_LALT: return KEY_ALT;
      case SDL_SCANCODE_RALT: return KEY_ALTGR;
      case SDL_SCANCODE_SPACE: return KEY_SPACE;
      case SDL_SCANCODE_CAPSLOCK: return KEY_CAPSLOCK;
      case SDL_SCANCODE_F1: return KEY_F1;
      case SDL_SCANCODE_F2: return KEY_F2;
      case SDL_SCANCODE_F3: return KEY_F3;
      case SDL_SCANCODE_F4: return KEY_F4;
      case SDL_SCANCODE_F5: return KEY_F5;
      case SDL_SCANCODE_F6: return KEY_F6;
      case SDL_SCANCODE_F7: return KEY_F7;
      case SDL_SCANCODE_F8: return KEY_F8;
      case SDL_SCANCODE_F9: return KEY_F9;
      case SDL_SCANCODE_F10: return KEY_F10;
      case SDL_SCANCODE_NUMLOCKCLEAR: return KEY_NUMLOCK;
      case SDL_SCANCODE_SCROLLLOCK: return KEY_SCRLOCK;
      case SDL_SCANCODE_HOME: case SDL_SCANCODE_KP_7: return KEY_HOME;
      case SDL_SCANCODE_UP: case SDL_SCANCODE_KP_8: return KEY_UP;
      case SDL_SCANCODE_PAGEUP: case SDL_SCANCODE_KP_9: return KEY_PGUP;
      case SDL_SCANCODE_KP_MINUS: return KEY_MINUS_PAD;
      case SDL_SCANCODE_LEFT: case SDL_SCANCODE_KP_4: return KEY_LEFT;
      case SDL_SCANCODE_KP_5: return KEY_5_PAD;
      case SDL_SCANCODE_RIGHT: case SDL_SCANCODE_KP_6: return KEY_RIGHT;
      case SDL_SCANCODE_KP_PLUS: return KEY_PLUS_PAD;
      case SDL_SCANCODE_END: case SDL_SCANCODE_KP_1: return KEY_END;
      case SDL_SCANCODE_DOWN: case SDL_SCANCODE_KP_2: return KEY_DOWN;
      case SDL_SCANCODE_PAGEDOWN: case SDL_SCANCODE_KP_3: return KEY_PGDN;
      case SDL_SCANCODE_INSERT: case SDL_SCANCODE_KP_0: return KEY_INSERT;
      case SDL_SCANCODE_DELETE: case SDL_SCANCODE_KP_PERIOD: return KEY_DEL;
      case SDL_SCANCODE_F11: return KEY_F11;
      case SDL_SCANCODE_F12: return KEY_F12;
      case SDL_SCANCODE_PAUSE: return KEY_PAUSE;
      case SDL_SCANCODE_KP_DIVIDE: return KEY_SLASH2;
      case SDL_SCANCODE_PRINTSCREEN: return KEY_PRTSCR;
      case SDL_SCANCODE_LGUI: return KEY_LWIN;
      case SDL_SCANCODE_RGUI: return KEY_RWIN;
      case SDL_SCANCODE_APPLICATION: return KEY_MENU;
      default: return 0;
   }
}

/* ascii for readkey(), US layout like the DOS keyboard driver */
static int pc_ascii(int sc, int shift)
{
   static const char lo[] = "\0\0331234567890-=\b\tqwertyuiop[]\r\0asdfghjkl;'`\0\\zxcvbnm,./\0*\0 ";
   static const char hi[] = "\0\033!@#$%^&*()_+\b\tQWERTYUIOP{}\r\0ASDFGHJKL:\"~\0|ZXCVBNM<>?\0*\0 ";
   if (sc > 0 && sc < (int)sizeof(lo) - 1)
      return (unsigned char)(shift ? hi[sc] : lo[sc]);
   return 0;
}

/* ---------------------------------------------------------------- presentation */

static int present_dirty = 1;

static void palette_to_argb(uint32_t *pal32)
{
   int i;
   for (i = 0; i < 256; i++) {
      const RGB *c = &_current_pallete[i];
      /* VGA DAC 6-bit -> 8-bit, rounded like DOSBox */
      uint32_t r = (uint32_t)(((c->r & 63) * 255 + 31) / 63);
      uint32_t g = (uint32_t)(((c->g & 63) * 255 + 31) / 63);
      uint32_t b = (uint32_t)(((c->b & 63) * 255 + 31) / 63);
      pal32[i] = 0xFF000000u | r << 16 | g << 8 | b;
   }
}

char sys_overlay[64];
extern const uint8_t font8x8_data[224][8];

/* sys_overlay as white 8x8 text on a dark box at the bottom of the frame */
static void draw_overlay(uint32_t *buf, int w, int h)
{
   int len = (int)strlen(sys_overlay), tw = len * 8, x0 = (w - tw) / 2, y0 = h - 14, x, y, i;
   for (y = y0 - 2; y < y0 + 10 && y < h; y++)
      for (x = x0 - 3; x < x0 + tw + 3; x++)
         if (x >= 0 && x < w && y >= 0)
            buf[(size_t)y * w + x] = 0xff202040;
   for (i = 0; i < len; i++) {
      unsigned char c = (unsigned char)sys_overlay[i];
      const uint8_t *g = c >= 32 ? font8x8_data[c - 32] : font8x8_data[0];
      for (y = 0; y < 8; y++)
         for (x = 0; x < 8; x++)
            if ((g[y] & (0x80 >> x)) && x0 + i * 8 + x >= 0 && x0 + i * 8 + x < w && y0 + y < h)
               buf[(size_t)(y0 + y) * w + x0 + i * 8 + x] = 0xffffffff;
   }
}

void sys_present(void)
{
   uint32_t pal32[256];
   const uint8_t *src_pixels;
   int x, y, w, h;
   SDL_FRect dst;
   int ww, wh;
   if (!screen && !_textmode_active())
      return;
   if (screen)
      _screen_capture_hook();
   if (sys_cfg.headless)
      return;
   w = screen ? screen->w : 0;
   h = screen ? screen->h : 0;
   if (_textmode_active()) {
      _textmode_render(&src_pixels, &w, &h, pal32);
   }
   else {
      palette_to_argb(pal32);
      src_pixels = NULL;
   }
   if (!texture || tex_w != w || tex_h != h) {
      if (texture)
         SDL_DestroyTexture(texture);
      texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
      SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_PIXELART);
      tex_w = w;
      tex_h = h;
      free(argb);
      argb = malloc(sizeof(uint32_t) * (size_t)w * h);
   }
   for (y = 0; y < h; y++) {
      const uint8_t *s = src_pixels ? src_pixels + (size_t)y * w : screen->line[y];
      uint32_t *d = argb + (size_t)y * w;
      for (x = 0; x < w; x++)
         d[x] = pal32[s[x]];
   }
   if (sys_overlay[0] && !_textmode_active())
      draw_overlay(argb, w, h);
   SDL_UpdateTexture(texture, NULL, argb, w * 4);
   SDL_GetRenderOutputSize(renderer, &ww, &wh);
   {
      /* 4:3 display aspect for 320x200 (and 640x400 text mode) unless square pixels */
      float aspect = sys_cfg.square_pixels ? (float)w / (float)h : 4.0f / 3.0f;
      float sw = (float)ww, sh = (float)ww / aspect;
      if (sh > (float)wh) {
         sh = (float)wh;
         sw = sh * aspect;
      }
      dst.w = sw;
      dst.h = sh;
      dst.x = ((float)ww - sw) / 2;
      dst.y = ((float)wh - sh) / 2;
   }
   SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
   SDL_RenderClear(renderer);
   SDL_RenderTexture(renderer, texture, NULL, &dst);
   SDL_RenderPresent(renderer);
   present_dirty = 0;
}

/* ---------------------------------------------------------------- events */

static void handle_events(void)
{
   SDL_Event ev;
   while (SDL_PollEvent(&ev)) {
      switch (ev.type) {
         case SDL_EVENT_QUIT:
            sys_quit_requested = 1;
            break;
         case SDL_EVENT_KEY_DOWN:
         case SDL_EVENT_KEY_UP: {
            int sc = sdl_to_pc(ev.key.scancode);
            int down = ev.type == SDL_EVENT_KEY_DOWN;
            if (getenv("SPACEHOG_DEBUG_INPUT"))
               sys_log("key %s sdl=%d pc=%d", down ? "down" : "up", (int)ev.key.scancode, sc);
            if (down && ev.key.scancode == SDL_SCANCODE_F5 && (ev.key.mod & SDL_KMOD_CTRL)) {
               sh_screenshot("key");   /* raw 320x200 screenshot, like DOSBox Ctrl+F5 */
               break;
            }
            if (down && ev.key.scancode == SDL_SCANCODE_RETURN && (ev.key.mod & SDL_KMOD_ALT)) {
               sys_cfg.fullscreen = !sys_cfg.fullscreen;
               SDL_SetWindowFullscreen(window, sys_cfg.fullscreen);
               break;
            }
            if (sc) {
               if (_input_replaying())
                  break;
               _keyboard_event(sc, down, down ? pc_ascii(sc, (ev.key.mod & SDL_KMOD_SHIFT) != 0) : 0, ev.key.repeat);
            }
            break;
         }
         case SDL_EVENT_MOUSE_MOTION:
            _mouse_motion(ev.motion.xrel, ev.motion.yrel);
            break;
         case SDL_EVENT_MOUSE_BUTTON_DOWN:
         case SDL_EVENT_MOUSE_BUTTON_UP:
            _mouse_button(ev.button.button, ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
            break;
         case SDL_EVENT_GAMEPAD_ADDED:
         case SDL_EVENT_GAMEPAD_REMOVED:
            _gamepad_hotplug();
            break;
         default:
            break;
      }
   }
}

/* ---------------------------------------------------------------- idle / retrace */

static uint64_t last_present_pit;

static void wait_until(uint64_t t)
{
   if (sys_cfg.fast) {
      advance_to(t);
      return;
   }
   for (;;) {
      uint64_t now = real_now_pit();
      if (now >= t)
         break;
      {
         uint64_t ns = (t - now) * 1000000000ULL / PIT_HZ;
         if (ns > 2000000)
            SDL_DelayNS(ns - 1000000);
         else if (ns > 200000)
            SDL_DelayNS(100000);
      }
   }
   advance_to(t);
}

void (*sys_quit_handler)(void);

/* exit cleanly once a quit was requested (window closed, signal, replay end, time limit) */
static void check_quit(void)
{
   static int quitting;
   if (sys_cfg.exit_after_ticks && virt_clock > (uint64_t)sys_cfg.exit_after_ticks * PIT_HZ)
      sys_quit_requested = 1;
   if (!sys_quit_requested || quitting)
      return;
   quitting = 1;
   if (sys_quit_handler)
      sys_quit_handler();
   sys_shutdown();
   exit(0);
}

static void pump(void)
{
   check_quit();
   _input_begin_pump();
   handle_events();   /* also in headless mode: delivers SIGINT/SIGTERM as quit */
   _input_tick_hook();
}

void (*sys_sync_hook)(int leave, int kind, int ret);
static int sync_depth, sync_kind;

void sys_sync_enter(int kind)
{
   if (sync_depth++ == 0) {
      sync_kind = kind;
      if (sys_sync_hook)
         sys_sync_hook(0, kind, 0);
   }
}

int sys_sync_leave(int ret)
{
   if (--sync_depth == 0 && sys_sync_hook)
      sys_sync_hook(1, sync_kind, ret);
   return ret;
}

void sys_idle(void)
{
   uint64_t t;
   sys_sync_enter('i');
   pump();
   /* advance to the next interesting event (timer tick or retrace) */
   t = _timer_next_event(virt_clock);
   if (t > next_retrace)
      t = next_retrace;
   if (!sys_cfg.fast) {
      uint64_t now = real_now_pit();
      /* don't sleep past "now" if we are behind; never run ahead of real time */
      if (now > t && now - t > PIT_HZ / 4) {
         /* we fell far behind (debugger, window drag): resync instead of spinning */
         real_base_ns += (now - t) * 1000000000ULL / PIT_HZ;
      }
   }
   wait_until(t);
   if (virt_clock >= next_retrace) {
      retrace_count++;
      next_retrace += RETRACE_PIT;
      if (virt_clock - last_present_pit >= RETRACE_PIT || present_dirty) {
         sys_present();
         last_present_pit = virt_clock;
      }
   }
   sys_sync_leave(0);
}

/* Non-blocking: pump events and let emulated time catch up (models CPU time spent in
 * loops that do not wait for a timer, e.g. TAB fast-forward). */
void sys_poll(void)
{
   uint64_t t;
   sys_sync_enter('p');
   pump();
   if (sys_cfg.fast)
      t = virt_clock + PIT_HZ / 1000;
   else if (net_session) {
      /* lockstep: emulated time must not depend on the real clock */
      t = virt_clock + PIT_HZ / 1000;
      if (real_now_pit() < t) {
         sys_sync_leave(0);
         return;
      }
   }
   else {
      t = real_now_pit();
      if (t <= virt_clock) {
         sys_sync_leave(0);
         return;
      }
   }
   advance_to(t);
   if (virt_clock >= next_retrace) {
      while (next_retrace <= virt_clock) {
         retrace_count++;
         next_retrace += RETRACE_PIT;
      }
      sys_present();
      last_present_pit = virt_clock;
   }
   sys_sync_leave(0);
}

/* netplay: real time spent waiting for the partner does not count as emulated time */
void sys_clock_pause(uint64_t ns)
{
   real_base_ns += ns;
}

/* netplay wait loops: keep the window alive and notice a close request */
void sys_wait_events(void)
{
   SDL_Event ev;
   SDL_PumpEvents();
   while (SDL_PeepEvents(&ev, 1, SDL_GETEVENT, SDL_EVENT_QUIT, SDL_EVENT_QUIT) > 0)
      sys_quit_requested = 1;
}

void sys_wait_retrace(void)
{
   int start = retrace_count;
   sys_sync_enter('V');
   while (retrace_count == start)
      sys_idle();
   sys_sync_leave(0);
}

/* ---------------------------------------------------------------- files */

static char data_dir[1024];
static char save_dir[1024];

static int file_exists_ci(const char *dir, const char *name, char *out, size_t outsz)
{
   char **list;
   int n, i, found = 0;
   snprintf(out, outsz, "%s/%s", dir, name);
   if (SDL_GetPathInfo(out, NULL))
      return 1;
   list = SDL_GlobDirectory(dir, NULL, 0, &n);
   if (!list)
      return 0;
   for (i = 0; i < n; i++) {
      if (SDL_strcasecmp(list[i], name) == 0) {
         snprintf(out, outsz, "%s/%s", dir, list[i]);
         found = 1;
         break;
      }
   }
   SDL_free(list);
   return found;
}

/* set when the data came from the Warpzone launcher (WARPZONE_DATA_GAME): the
   folder it found is the player's download or install, so saves go to the
   per-user directory instead */
static int data_from_launcher;

static void find_data_dir(void)
{
   const char *cands[6];
   char base[1024];
   char tmp[1200];
   int i, n = 0;
   const char *b = SDL_GetBasePath();
   snprintf(base, sizeof(base), "%s", b ? b : "./");
   if (sys_cfg.data_dir) cands[n++] = sys_cfg.data_dir;
   if (getenv("SPACEHOG_DATA")) cands[n++] = getenv("SPACEHOG_DATA");
   if (getenv("WARPZONE_DATA_GAME")) cands[n++] = getenv("WARPZONE_DATA_GAME");
   cands[n++] = ".";
   cands[n++] = "data";
   cands[n++] = base;
   for (i = 0; i < n; i++) {
      if (file_exists_ci(cands[i], "SPACEHOG.DAT", tmp, sizeof(tmp))) {
         snprintf(data_dir, sizeof(data_dir), "%s", cands[i]);
         data_from_launcher = cands[i] == getenv("WARPZONE_DATA_GAME");
         return;
      }
   }
   snprintf(data_dir, sizeof(data_dir), ".");
}

int sys_find_soundfont(char *out, size_t outsz)
{
   const char *dirs[2];
   int i;
   if (sys_cfg.music_gm != 1)
      return 0;
   if (sys_cfg.soundfont) {
      if (!file_exists_ci(save_dir, sys_cfg.soundfont, out, outsz) &&
          !file_exists_ci(data_dir, sys_cfg.soundfont, out, outsz))
         snprintf(out, outsz, "%s", sys_cfg.soundfont);
      return 1;
   }
   dirs[0] = data_dir;
   dirs[1] = save_dir;
   for (i = 0; i < 2; i++) {
      int n;
      char **list = SDL_GlobDirectory(dirs[i], "*.sf2", SDL_GLOB_CASEINSENSITIVE, &n);
      if (list && n > 0) {
         snprintf(out, outsz, "%s/%s", dirs[i], list[0]);
         SDL_free(list);
         return 1;
      }
      SDL_free(list);
   }
   sys_log("music=gm, but no SoundFont (*.sf2) found; using FM");
   return 0;
}

/* spacehog.ini in the save dir: optional "key = value" settings for things the original
 * options menu does not have. Command line options take precedence. */
static void load_ini(void)
{
   char path[1200], line[512], key[64], val[400];
   FILE *f;
   if (!file_exists_ci(save_dir, "spacehog.ini", path, sizeof(path)) || !(f = fopen(path, "r")))
      return;
   while (fgets(line, sizeof(line), f)) {
      if (line[0] == '#' || line[0] == ';' || sscanf(line, " %63[^= ] = %399[^\r\n]", key, val) != 2)
         continue;
      if (!SDL_strcasecmp(key, "music") && sys_cfg.music_gm < 0)
         sys_cfg.music_gm = !SDL_strcasecmp(val, "gm") || !SDL_strcasecmp(val, "soundfont");
      else if (!SDL_strcasecmp(key, "soundfont") && !sys_cfg.soundfont)
         sys_cfg.soundfont = SDL_strdup(val);
   }
   fclose(f);
}

uint8_t *sys_read_file(const char *name, size_t *size)
{
   char path[1200];
   FILE *f;
   uint8_t *buf;
   long n;
   if (!file_exists_ci(data_dir, name, path, sizeof(path)))
      return NULL;
   f = fopen(path, "rb");
   if (!f)
      return NULL;
   fseek(f, 0, SEEK_END);
   n = ftell(f);
   fseek(f, 0, SEEK_SET);
   buf = malloc((size_t)(n > 0 ? n : 1));
   if (fread(buf, 1, (size_t)n, f) != (size_t)n) {
      free(buf);
      fclose(f);
      return NULL;
   }
   fclose(f);
   *size = (size_t)n;
   return buf;
}

void sys_set_save_dir(const char *dir)
{
   snprintf(save_dir, sizeof(save_dir), "%s", dir);
}

const char *sys_get_save_dir(void)
{
   return save_dir;
}

int sys_save_path(const char *name, char *out, size_t outsz)
{
   if (!file_exists_ci(save_dir, name, out, outsz))
      snprintf(out, outsz, "%s/%s", save_dir, name);
   return 0;
}

void *sys_fopen_save(const char *name, const char *mode)
{
   char path[1200];
   sys_save_path(name, path, sizeof(path));
   return fopen(path, mode);
}

/* ---------------------------------------------------------------- audio */

static void SDLCALL audio_callback(void *userdata, SDL_AudioStream *stream, int additional, int total)
{
   int16_t buf[2048 * 2];
   (void)userdata;
   (void)total;
   while (additional > 0) {
      int frames = additional / 4;
      if (frames > 2048) frames = 2048;
      if (frames <= 0) frames = 1;
      memset(buf, 0, sizeof(int16_t) * 2 * (size_t)frames);
      if (audio_cb)
         audio_cb(buf, frames, audio_user);
      SDL_PutAudioStreamData(stream, buf, frames * 4);
      additional -= frames * 4;
   }
}

int sys_audio_open(int rate, sys_audio_cb cb, void *user)
{
   SDL_AudioSpec spec;
   if (sys_cfg.headless)
      return -1;
   spec.format = SDL_AUDIO_S16;
   spec.channels = 2;
   spec.freq = rate;
   audio_cb = cb;
   audio_user = user;
   audio_rate = rate;
   audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, audio_callback, NULL);
   if (!audio_stream) {
      sys_log("audio: %s", SDL_GetError());
      return -1;
   }
   SDL_ResumeAudioStreamDevice(audio_stream);
   return 0;
}

void sys_audio_lock(void)
{
   if (audio_stream)
      SDL_LockAudioStream(audio_stream);
}

void sys_audio_unlock(void)
{
   if (audio_stream)
      SDL_UnlockAudioStream(audio_stream);
}

/* ---------------------------------------------------------------- init */

void sys_log(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   vfprintf(stderr, fmt, ap);
   va_end(ap);
   fputc('\n', stderr);
}

/* fatal startup error: log it and, with a window, show it in a message box too */
void sys_error_box(const char *fmt, ...)
{
   char msg[512];
   va_list ap;
   va_start(ap, fmt);
   vsnprintf(msg, sizeof(msg), fmt, ap);
   va_end(ap);
   fprintf(stderr, "%s\n", msg);
   if (!sys_cfg.headless)
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Operation Spacehog", msg, window);
}

int sys_init(void)
{
   SDL_InitFlags flags = SDL_INIT_GAMEPAD | SDL_INIT_JOYSTICK | SDL_INIT_EVENTS;
   if (!sys_cfg.headless)
      flags |= SDL_INIT_VIDEO | SDL_INIT_AUDIO;
   SDL_SetAppMetadata("Operation Spacehog", "1.01-port", "org.freelunchdesign.spacehog");
   if (!SDL_Init(flags)) {
      sys_log("SDL_Init: %s", SDL_GetError());
      return -1;
   }
   find_data_dir();
   if (sys_cfg.headless && getenv("SPACEHOG_AUDIO_DUMP")) {
      audio_dump = fopen(getenv("SPACEHOG_AUDIO_DUMP"), "wb");
      if (audio_dump)
         audio_dump_wav_header(0);
   }
   if (sys_cfg.save_dir)
      snprintf(save_dir, sizeof(save_dir), "%s", sys_cfg.save_dir);
   else {
      /* like DOS: next to the data, unless that directory is read-only or the
         launcher found it */
      char probe[1200];
      SDL_IOStream *io;
      snprintf(save_dir, sizeof(save_dir), "%s", data_dir);
      snprintf(probe, sizeof(probe), "%s/.spacehog_write_test", data_dir);
      io = data_from_launcher ? NULL : SDL_IOFromFile(probe, "wb");
      if (io) {
         SDL_CloseIO(io);
         SDL_RemovePath(probe);
      }
      else {
         char *pref = SDL_GetPrefPath("FreeLunchDesign", "Spacehog");
         if (pref) {
            snprintf(save_dir, sizeof(save_dir), "%s", pref);
            SDL_free(pref);
         }
      }
   }
   load_ini();
   if (!sys_cfg.headless) {
      int scale = sys_cfg.scale > 0 ? sys_cfg.scale : 3;
      window = SDL_CreateWindow("Operation Spacehog", 320 * scale, 240 * scale,
                                SDL_WINDOW_RESIZABLE | (sys_cfg.fullscreen ? SDL_WINDOW_FULLSCREEN : 0));
      if (!window) {
         sys_log("SDL_CreateWindow: %s", SDL_GetError());
         return -1;
      }
      renderer = SDL_CreateRenderer(window, NULL);
      if (!renderer) {
         sys_log("SDL_CreateRenderer: %s", SDL_GetError());
         return -1;
      }
      SDL_SetRenderVSync(renderer, 0);
      SDL_HideCursor();
   }
   real_base_ns = SDL_GetTicksNS();
   virt_clock = 0;
   next_retrace = RETRACE_PIT;
   return 0;
}

void sys_shutdown(void)
{
   if (audio_dump) {
      audio_dump_wav_header((uint32_t)audio_dump_frames);
      fclose(audio_dump);
      audio_dump = NULL;
   }
   if (audio_stream)
      SDL_DestroyAudioStream(audio_stream);
   audio_stream = NULL;
   if (texture) SDL_DestroyTexture(texture);
   if (renderer) SDL_DestroyRenderer(renderer);
   if (window) SDL_DestroyWindow(window);
   free(argb);
   SDL_Quit();
}
