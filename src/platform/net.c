/* Online co-op over Warpzone rooms (see net.h): session setup and frame transport. */
#include "net.h"
#include "sys.h"
#include "report.h"
#include "../compat/internal.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

int net_active, net_session, net_me, net_delay;   /* delay 0 = from the RTT */
NetConfig net_cfg;

#ifndef SPACEHOG_VERSION
#define SPACEHOG_VERSION "dev"
#endif

#ifndef HAVE_WARPZONE

int net_start(int *argc, char ***argv, uint32_t *seed)
{
   (void)argc; (void)argv; (void)seed;
   if (!net_cfg.requested)
      return 0;
   sys_error_box("This build of Operation Spacehog has no Warpzone support.");
   return -1;
}
void net_send_frame(uint32_t frame, const NetEvent *ev, int n, uint32_t hash_frame, uint32_t hash)
{
   (void)frame; (void)ev; (void)n; (void)hash_frame; (void)hash;
}
int net_get_frame(uint32_t frame, NetEvent *ev, int max)
{
   (void)frame; (void)ev; (void)max;
   return -1;
}
void net_local_hash(uint32_t frame, uint32_t hash)
{
   (void)frame; (void)hash;
}
void net_shutdown(void)
{
}
int net_report(const char *kind, const char *title, const char *detail)
{
   (void)kind; (void)title; (void)detail;
   return 0;
}

#else
#include "warpzone.h"

#define NET_PROTO 1
#define CH 1                          /* everything goes reliable on one channel, in order */
#define RING 64
#define HRING 16
#define HASH_EVERY 35                 /* = input.c NET_HASH_EVERY */

enum { M_HELLO = 'H', M_START = 'S', M_SAVE = 'D', M_END = 'E', M_READY = 'R',
       M_REFUSE = 'X', M_BYE = 'B', M_FRAME = 'F' };

static wz_client *wz;
static struct {
   uint32_t frame;
   int valid, n;
   NetEvent ev[NET_MAX_EVENTS];
} rframes[RING];
static struct { uint32_t frame, hash; int valid; } lhash[HRING], rhash[HRING];
static int desynced;
static char partner[64];
static wz_peer partner_peer;
static int have_peer;

/* handshake state */
static int logged_in, joined, hosted, got_hello, got_ready, start_done, refused;
static uint32_t hello_proto, hello_data, start_seed;
static int start_delay;
static uint8_t *sav_buf;
static size_t sav_len, sav_want;
static char start_args[1024];
static size_t start_args_len;
static char status_line[2][80];

static uint32_t rd32(const uint8_t *p)
{
   return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static void wr32(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

/* value of "key": "..." in a JSON text (enough for the flat fields used here) */
static int json_str(const char *json, const char *key, char *out, size_t cap)
{
   char pat[64];
   const char *p;
   size_t n = 0;
   snprintf(pat, sizeof(pat), "\"%s\"", key);
   p = json ? strstr(json, pat) : NULL;
   if (!p)
      return 0;
   p += strlen(pat);
   while (*p == ' ' || *p == ':')
      p++;
   if (*p != '"')
      return 0;
   p++;
   while (*p && *p != '"' && n + 1 < cap) {
      if (*p == '\\' && p[1])
         p++;
      out[n++] = *p++;
   }
   out[n] = 0;
   return 1;
}

/* the original data must be the same on both sides */
static uint32_t data_hash(void)
{
   static const char *const files[] = { "SPACEHOG.DAT", "SHLEV.DAT", "SHBGS.DAT", "SHSND.DAT" };
   uint32_t h = 2166136261u;
   size_t i, k, sz;
   for (i = 0; i < 4; i++) {
      uint8_t *b = sys_read_file(files[i], &sz);
      if (!b)
         continue;
      for (k = 0; k < sz; k++)
         h = (h ^ b[k]) * 16777619u;
      free(b);
   }
   return h;
}

static void status(int line, const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   vsnprintf(status_line[line], sizeof(status_line[line]), fmt, ap);
   va_end(ap);
   sys_log("warpzone: %s", status_line[line]);
}

static void draw_status(void)
{
   int i;
   const char *s;
   sys_set_textmode(1);
   sys_textmode_color(7, 0);
   sys_textmode_clrscr();
   sys_textmode_gotoxy(3, 3);
   sys_textmode_color(15, 1);
   for (s = " Operation Spacehog - online co-op over Warpzone "; *s; s++)
      sys_textmode_putc(*s);
   sys_textmode_color(7, 0);
   for (i = 0; i < 2; i++) {
      sys_textmode_gotoxy(3, 6 + 2 * i);
      for (s = status_line[i]; *s; s++)
         sys_textmode_putc(*s);
   }
   sys_textmode_gotoxy(3, 23);
   sys_textmode_color(8, 0);
   for (s = "Close the window to cancel."; *s; s++)
      sys_textmode_putc(*s);
   sys_present();
}

static void send_msg(const uint8_t *b, size_t n)
{
   wz_broadcast(wz, CH, b, n, 1);
}

/* a message on top of the game for a few seconds */
static char shown[64];
static uint64_t shown_at;

static void show(const char *msg)
{
   snprintf(sys_overlay, sizeof(sys_overlay), "%s", msg);
   snprintf(shown, sizeof(shown), "%s", sys_overlay);
   shown_at = SDL_GetTicks();
}

static void unshow(int force)
{
   if (shown[0] && (force || SDL_GetTicks() - shown_at > 6000)) {
      if (!strcmp(sys_overlay, shown))
         sys_overlay[0] = 0;
      shown[0] = 0;
   }
}

static void partner_gone(const char *why)
{
   if (!net_active)
      return;
   net_active = 0;
   sys_log("netplay: partner gone (%s); playing on alone", why);
   {
      char m[64];
      snprintf(m, sizeof(m), "%.40s left the game", partner[0] ? partner : "partner");
      show(m);
   }
}

static void on_data(const uint8_t *d, size_t n)
{
   if (!n)
      return;
   switch (d[0]) {
      case M_HELLO:
         if (n >= 9) {
            hello_proto = rd32(d + 1);
            hello_data = rd32(d + 5);
            got_hello = 1;
         }
         break;
      case M_START:
         if (n >= 14 && rd32(d + 1) == NET_PROTO) {
            start_seed = rd32(d + 5);
            start_delay = d[9];
            sav_want = rd32(d + 10);
            start_args_len = MIN(n - 14, sizeof(start_args) - 1);
            memcpy(start_args, d + 14, start_args_len);
            start_args[start_args_len] = 0;
            free(sav_buf);
            sav_buf = malloc(sav_want + 1);
            sav_len = 0;
         }
         break;
      case M_SAVE:
         if (sav_buf && sav_len + n - 1 <= sav_want) {
            memcpy(sav_buf + sav_len, d + 1, n - 1);
            sav_len += n - 1;
         }
         break;
      case M_END:
         start_done = sav_buf != NULL && sav_len == sav_want;
         break;
      case M_READY:
         got_ready = 1;
         break;
      case M_REFUSE:
         refused = 1;
         status(1, "The host refused: %.*s", (int)MIN(n - 1, 60), (const char *)d + 1);
         break;
      case M_BYE:
         partner_gone("quit");
         break;
      case M_FRAME:
         if (n >= 14) {
            uint32_t f = rd32(d + 1), hf = rd32(d + 5), h = rd32(d + 9);
            int k, cnt = d[13];
            if ((size_t)14 + (size_t)cnt * 6 > n)
               break;
            rframes[f % RING].frame = f;
            rframes[f % RING].n = cnt;
            for (k = 0; k < cnt; k++) {
               const uint8_t *e = d + 14 + k * 6;
               NetEvent *o = &rframes[f % RING].ev[k];
               o->kind = e[0];
               o->c = e[1];
               o->a = (int16_t)(e[2] | e[3] << 8);
               o->b = (int16_t)(e[4] | e[5] << 8);
            }
            rframes[f % RING].valid = 1;
            if (hf != 0xffffffffu) {
               int i = (int)(hf / HASH_EVERY % HRING);
               rhash[i].frame = hf;
               rhash[i].hash = h;
               rhash[i].valid = 1;
               net_local_hash(0xffffffffu, 0);
            }
         }
         break;
      default:
         break;
   }
}

static void pump_wz(void)
{
   wz_event e;
   if (!wz)
      return;
   wz_poll(wz);
   while (wz_next_event(wz, &e)) {
      char buf[256];
      switch (e.type) {
         case WZ_EVENT_LOGGED_IN:
            if (strstr(e.json, "\"account\":null") || strstr(e.json, "\"account\": null")) {
               if (net_cfg.name) {
                  status(0, "Registering the Warpzone name %s ...", net_cfg.name);
                  wz_register(wz, net_cfg.name);
               }
               else {
                  status(0, "No Warpzone name yet: start the Warpzone launcher once,");
                  status(1, "or start the game with --warpzone-name NAME.");
                  refused = 1;
               }
            }
            else
               logged_in = 1;
            break;
         case WZ_EVENT_REGISTERED:
            logged_in = 1;
            break;
         case WZ_EVENT_ROOM_HOSTED:
            hosted = 1;
            if (json_str(e.json, "code", buf, sizeof(buf)))
               status(0, "Room open. Code: %s", buf);
            else
               status(0, "Room open in the Warpzone room list.");
            if (json_str(e.json, "invite", buf, sizeof(buf)))
               sys_log("warpzone: invite %s", buf);
            status(1, "Waiting for player 2 ...");
            break;
         case WZ_EVENT_ROOM_JOINED:
            joined = 1;
            if (!hosted)
               status(0, "Joined the room, connecting to the host ...");
            break;
         case WZ_EVENT_KNOCKING:
            status(0, "Waiting for the host to let you in ...");
            break;
         case WZ_EVENT_KNOCK_DENIED:
            status(1, "The host did not let you in.");
            refused = 1;
            break;
         case WZ_EVENT_PEER_JOINED:
            if (json_str(e.json, "username", buf, sizeof(buf)))
               snprintf(partner, sizeof(partner), "%.40s", buf);
            status(1, "%s joined, connecting ...", partner[0] ? partner : "A player");
            break;
         case WZ_EVENT_PEER_CONNECTED:
            status(1, "Connected to %s (%s).", partner[0] ? partner : "partner",
                   strstr(e.json, "true") ? "relayed" : "direct");
            if (!partner[0])
               wz_peer_name(wz, e.peer, partner, sizeof(partner));
            partner_peer = e.peer;
            have_peer = 1;
            break;
         case WZ_EVENT_PEER_DATA:
            if (e.channel == CH)
               on_data(e.data, e.len);
            break;
         case WZ_EVENT_PEER_LEFT:
            partner_gone("left");
            if (!net_session)
               status(1, "Player 2 left. Waiting for player 2 ...");
            break;
         case WZ_EVENT_ROOM_CLOSED:
         case WZ_EVENT_KICKED:
            partner_gone("room closed");
            if (!net_session) {
               status(1, "The room was closed.");
               refused = 1;
            }
            break;
         case WZ_EVENT_ERROR:
            if (json_str(e.json, "message", buf, sizeof(buf))) {
               char op[32] = "";
               json_str(e.json, "op", op, sizeof(op));
               status(1, "Warpzone: %s", buf);
               if (strcmp(op, "login") != 0 && !net_session)
                  refused = 1;
            }
            break;
         default:
            break;
      }
   }
}

/* wait (keeping the window alive) until *flag is set; 0 = ok, -1 = failed/cancelled */
static int wait_for(int *flag, int timeout_s)
{
   uint64_t start = SDL_GetTicks(), last_draw = 0;
   while (!*flag) {
      pump_wz();
      sys_wait_events();
      if (refused || sys_quit_requested)
         break;
      if (timeout_s && SDL_GetTicks() - start > (uint64_t)timeout_s * 1000) {
         status(1, "No answer from Warpzone.");
         break;
      }
      if (SDL_GetTicks() - last_draw > 100) {
         draw_status();
         last_draw = SDL_GetTicks();
      }
      SDL_Delay(5);
   }
   draw_status();
   return *flag ? 0 : -1;
}

static void fail_pause(void)
{
   uint64_t start = SDL_GetTicks();
   if (sys_quit_requested || sys_cfg.headless)
      return;
   draw_status();
   while (SDL_GetTicks() - start < 6000 && !sys_quit_requested) {
      sys_wait_events();
      SDL_Delay(20);
   }
}

static int host_session(int argc, char **argv, uint32_t *seed)
{
   uint8_t msg[WZ_MAX_DATAGRAM];
   size_t n, off;
   int i;
   char opts[256];
   FILE *f;
   uint8_t *sav = NULL;
   long savn = 0;
   snprintf(opts, sizeof(opts),
            "{\"max_players\": 2, \"visibility\": \"%s\", \"entry\": \"open\", \"preset\": \"coop\","
            " \"metadata\": {\"level\": \"1\"}}", net_cfg.private_room ? "unlisted" : "public");
   wz_host(wz, opts);
   status(0, "Opening a room ...");
   if (wait_for(&hosted, 30) != 0)
      return -1;
   for (;;) {
      got_hello = 0;
      if (wait_for(&got_hello, 0) != 0)
         return -1;
      if (hello_proto == NET_PROTO && hello_data == data_hash())
         break;
      n = (size_t)snprintf((char *)msg + 1, sizeof(msg) - 1, "%s",
                           hello_proto != NET_PROTO ? "different game version" : "different game data") + 1;
      msg[0] = M_REFUSE;
      send_msg(msg, n);
      status(1, "Refused %s: %s. Waiting for player 2 ...", partner, (char *)msg + 1);
   }
   wz_set_closed(wz, 1);
   /* the host's SPACEHOG.SAV and game arguments are the session's */
   f = sys_fopen_save("SPACEHOG.SAV", "rb");
   if (f) {
      fseek(f, 0, SEEK_END);
      savn = ftell(f);
      fseek(f, 0, SEEK_SET);
      sav = malloc((size_t)savn + 1);
      savn = (long)fread(sav, 1, (size_t)savn, f);
      fclose(f);
   }
   if (!net_delay) {
      /* the guest's frame must arrive before it is due: one-way time plus some slack */
      int rtt = have_peer ? wz_peer_rtt_ms(wz, partner_peer) : -1;
      net_delay = rtt < 0 ? 6 : (rtt / 2 + 25) * 70 / 1000 + 1;
      if (net_delay < 3) net_delay = 3;
      if (net_delay > 15) net_delay = 15;
      sys_log("netplay: rtt %d ms, input delay %d frames", rtt, net_delay);
   }
   *seed = (uint32_t)(SDL_GetTicksNS() ^ (uint64_t)time(NULL) * 2654435761u);
   msg[0] = M_START;
   wr32(msg + 1, NET_PROTO);
   wr32(msg + 5, *seed);
   msg[9] = (uint8_t)net_delay;
   wr32(msg + 10, (uint32_t)savn);
   n = 14;
   for (i = 1; i < argc; i++) {
      size_t l = strlen(argv[i]) + 1;
      if (n + l > sizeof(msg))
         break;
      memcpy(msg + n, argv[i], l);
      n += l;
   }
   send_msg(msg, n);
   for (off = 0; off < (size_t)savn; off += WZ_MAX_DATAGRAM - 1) {
      size_t k = MIN((size_t)savn - off, (size_t)WZ_MAX_DATAGRAM - 1);
      msg[0] = M_SAVE;
      memcpy(msg + 1, sav + off, k);
      send_msg(msg, k + 1);
   }
   free(sav);
   msg[0] = M_END;
   send_msg(msg, 1);
   status(1, "Starting with %s ...", partner);
   if (wait_for(&got_ready, 30) != 0) {
      if (!refused && !sys_quit_requested && have_peer)
         report_fault("netplay: the guest never got ready", "host, input delay %d, save %ld bytes\n",
                      net_delay, savn);
      return -1;
   }
   return 0;
}

static int guest_session(int *argc, char ***argv, uint32_t *seed)
{
   static char *args[34];
   static char dir[1100];
   uint8_t msg[16];
   char *p;
   int n = 0;
   if (wait_for(&joined, 60) != 0)
      return -1;
   msg[0] = M_HELLO;
   wr32(msg + 1, NET_PROTO);
   wr32(msg + 5, data_hash());
   send_msg(msg, 9);
   if (wait_for(&start_done, 60) != 0) {
      if (!refused && !sys_quit_requested)
         report_fault(have_peer ? "netplay: no start from the host" : "netplay: no connection to the host",
                      "guest, joined the room, %s\n", have_peer ? "connected" : "peer never connected");
      return -1;
   }
   *seed = start_seed;
   net_delay = start_delay;
   /* play on the host's SPACEHOG.SAV, kept apart from this player's own */
   snprintf(dir, sizeof(dir), "%s/net-guest", sys_get_save_dir());
   SDL_CreateDirectory(dir);
   sys_set_save_dir(dir);
   if (sav_len) {
      FILE *f = sys_fopen_save("SPACEHOG.SAV", "wb");
      if (f) {
         fwrite(sav_buf, 1, sav_len, f);
         fclose(f);
      }
   }
   else {
      char path[1200];
      if (sys_save_path("SPACEHOG.SAV", path, sizeof(path)) == 0)
         remove(path);
   }
   args[n++] = (*argv)[0];
   for (p = start_args; p < start_args + start_args_len && n < 33; p += strlen(p) + 1)
      args[n++] = p;
   args[n] = NULL;
   *argc = n;
   *argv = args;
   msg[0] = M_READY;
   send_msg(msg, 1);
   return 0;
}

int net_start(int *argc, char ***argv, uint32_t *seed)
{
   wz_config cfg;
   const char *largv[4];
   int largc = 0, r;
   if (!net_cfg.requested)
      return 0;
   memset(&cfg, 0, sizeof(cfg));
   cfg.abi = WZ_ABI_VERSION;
   cfg.server = net_cfg.server;
   cfg.game = "spacehog";
   cfg.game_version = SPACEHOG_VERSION;
   wz = wz_create(&cfg);
   if (!wz) {
      sys_error_box("Warpzone: %s", wz_last_error());
      return -1;
   }
   if (net_delay < 0) net_delay = 0;
   if (net_delay > 20) net_delay = 20;
   status(0, "Connecting to Warpzone ...");
   status(1, "");
   if (wait_for(&logged_in, 60) != 0) {
      fail_pause();
      return -1;
   }
   largv[largc++] = "spacehog";
   if (net_cfg.host)
      largv[largc++] = "--warpzone-host";
   else if (net_cfg.join) {
      largv[largc++] = "--warpzone-join";
      largv[largc++] = net_cfg.join;
   }
   else if (net_cfg.invite)
      largv[largc++] = net_cfg.invite;
   largv[largc] = NULL;
   if (net_cfg.code) {
      wz_join_code(wz, net_cfg.code);
      r = 1;
   }
   else
      r = wz_handle_launch(wz, largc, largv);
   if (r == 2 || (r == 0 && net_cfg.host)) {
      net_me = 0;
      r = host_session(*argc, *argv, seed);
   }
   else if (r == 1) {
      net_me = 1;
      status(0, "Joining the room ...");
      r = guest_session(argc, argv, seed);
   }
   else {
      status(1, "Nothing to join.");
      r = -1;
   }
   if (r != 0) {
      fail_pause();
      return -1;
   }
   sys_log("netplay: started as %s with %s, seed %u, input delay %d frames",
           net_me ? "guest (player 2)" : "host (player 1)", partner, *seed, net_delay);
   sys_textmode_clrscr();
   sys_set_textmode(0);
   net_session = net_active = 1;
   atexit(net_shutdown);
   return 0;
}

void net_send_frame(uint32_t frame, const NetEvent *ev, int n, uint32_t hash_frame, uint32_t hash)
{
   uint8_t msg[14 + NET_MAX_EVENTS * 6];
   int k;
   unshow(0);
   if (!net_active)
      return;
   msg[0] = M_FRAME;
   wr32(msg + 1, frame);
   wr32(msg + 5, hash_frame);
   wr32(msg + 9, hash);
   msg[13] = (uint8_t)n;
   for (k = 0; k < n; k++) {
      uint8_t *e = msg + 14 + k * 6;
      e[0] = ev[k].kind;
      e[1] = ev[k].c;
      e[2] = (uint8_t)ev[k].a; e[3] = (uint8_t)((uint16_t)ev[k].a >> 8);
      e[4] = (uint8_t)ev[k].b; e[5] = (uint8_t)((uint16_t)ev[k].b >> 8);
   }
   send_msg(msg, (size_t)(14 + n * 6));
   pump_wz();
}

int net_get_frame(uint32_t frame, NetEvent *ev, int max)
{
   uint64_t start = 0;
   int slot = (int)(frame % RING), n;
   while (net_active && !(rframes[slot].valid && rframes[slot].frame == frame)) {
      if (!start)
         start = SDL_GetTicksNS();
      pump_wz();
      sys_wait_events();
      if (sys_quit_requested)
         return -1;
      if (SDL_GetTicksNS() - start > 2000000000ull && !shown[0] && !sys_overlay[0]) {
         char m[64];
         snprintf(m, sizeof(m), "waiting for %.40s ...", partner[0] ? partner : "partner");
         show(m);
         sys_present();
      }
      SDL_DelayNS(500000);
   }
   if (start) {
      if (!strncmp(shown, "waiting for", 11)) {
         unshow(1);
         sys_present();
      }
      if (!sys_cfg.fast)
         sys_clock_pause(SDL_GetTicksNS() - start);
   }
   /* frames that arrived before the partner left still count */
   if (!(rframes[slot].valid && rframes[slot].frame == frame))
      return -1;
   n = MIN(rframes[slot].n, max);
   memcpy(ev, rframes[slot].ev, sizeof(NetEvent) * (size_t)n);
   rframes[slot].valid = 0;
   return n;
}

/* store this side's hash (frame != ~0) and compare with the partner's where both exist */
void net_local_hash(uint32_t frame, uint32_t hash)
{
   int i;
   if (frame != 0xffffffffu) {
      i = (int)(frame / HASH_EVERY % HRING);
      lhash[i].frame = frame;
      lhash[i].hash = hash;
      lhash[i].valid = 1;
   }
   for (i = 0; i < HRING; i++)
      if (lhash[i].valid && rhash[i].valid && lhash[i].frame == rhash[i].frame) {
         if (lhash[i].hash != rhash[i].hash && !desynced) {
            desynced = 1;
            sys_log("netplay: DESYNC at frame %u (state %08x here, %08x at the partner)",
                    lhash[i].frame, lhash[i].hash, rhash[i].hash);
            report_fault("netplay: out of sync",
                         "%s, frame %u, state %08x here, %08x at the partner, input delay %d, protocol %d\n",
                         net_me ? "guest" : "host", lhash[i].frame, lhash[i].hash, rhash[i].hash,
                         net_delay, NET_PROTO);
            {
               char m[64];
               snprintf(m, sizeof(m), "out of sync with %.40s", partner[0] ? partner : "partner");
               show(m);
            }
         }
         else if (lhash[i].frame % (HASH_EVERY * 70) == 0)
            sys_log("netplay: in sync at frame %u", lhash[i].frame);
         lhash[i].valid = rhash[i].valid = 0;
      }
}

void net_shutdown(void)
{
   static int done;
   if (done || !wz)
      return;
   done = 1;
   if (net_active) {
      uint8_t b = M_BYE;
      uint64_t t = SDL_GetTicks();
      send_msg(&b, 1);
      while (SDL_GetTicks() - t < 300) {   /* let the reliable message go out */
         wz_poll(wz);
         SDL_Delay(10);
      }
   }
   wz_destroy(wz);
   wz = NULL;
   net_active = 0;
}

int net_report(const char *kind, const char *title, const char *detail)
{
   if (!wz)
      return 0;
   wz_report(wz, kind, title, detail);
   return 1;
}

#endif
