/* Online co-op over Warpzone: deterministic lockstep on the raw input stream.
 *
 * Both machines run the whole program (setup screen, menus, game) from the same seed and
 * SPACEHOG.SAV. Each side's input events (keys, mouse, pads) are collected per frame of
 * the emulated clock (one 70 Hz retrace), sent with an input delay of net_delay frames,
 * and applied on both sides at the first input pump of that frame: host first, then
 * guest. The game therefore sees the same input at the same emulated time on both
 * machines, exactly like a replay; --record writes that merged stream (an ordinary
 * replay). A CRC of the complete game state (sync_state) is compared twice a second. */
#ifndef SH_NET_H
#define SH_NET_H

#include <stdint.h>

#define NET_FRAME_PIT 17025u      /* = one retrace */
#define NET_MAX_EVENTS 160

typedef struct NetEvent {
   uint8_t kind;                  /* 'k' key, 'm' mouse move, 'b' mouse buttons, 'j' pad, 'q' quit */
   uint8_t c;
   int16_t a, b;
} NetEvent;

typedef struct NetConfig {
   int requested;                 /* any Warpzone option given */
   int host;                      /* --warpzone-host */
   const char *join;              /* --warpzone-join ROOM */
   const char *code;              /* --warpzone-code CODE */
   const char *invite;            /* warpzone://join/... */
   const char *name;              /* --warpzone-name: register this name if there is none */
   const char *server;            /* --warpzone-server URL (else WARPZONE_SERVER / public) */
   int private_room;              /* --warpzone-private: unlisted room, join by code/link */
} NetConfig;
extern NetConfig net_cfg;

extern int net_active;            /* a lockstep session is running (partner still there) */
extern int net_session;           /* started as a net session (stays set after the partner left) */
extern int net_me;                /* 0 = host (player 1), 1 = guest (player 2) */
extern int net_delay;             /* input delay in frames */

/* Parse the Warpzone options, connect and agree on seed, SAV and game arguments.
 * Returns 0 (no net session requested, or connected: net_session set) or -1 (failed or
 * cancelled; the program should exit). On success *seed is the shared RNG seed and
 * argc and argv may be replaced by the host's game arguments. */
int net_start(int *argc, char ***argv, uint32_t *seed);
/* both called by the input layer (input.c), once per frame */
void net_send_frame(uint32_t frame, const NetEvent *ev, int n, uint32_t hash_frame, uint32_t hash);
/* blocks until the partner's frame is there; -1 = partner gone (net_active cleared) */
int net_get_frame(uint32_t frame, NetEvent *ev, int max);
void net_local_hash(uint32_t frame, uint32_t hash);
void net_shutdown(void);
/* send a fault report with the session's Warpzone client (report.c); 0 = no client */
int net_report(const char *kind, const char *title, const char *detail);

#endif
