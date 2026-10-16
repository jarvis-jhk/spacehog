/* Platform layer (SDL3): window, presentation, input, timing, audio, files. */
#ifndef SH_SYS_H
#define SH_SYS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SysConfig {
   const char *data_dir;     /* where the original .DAT files are (NULL = search) */
   const char *save_dir;     /* where SPACEHOG.SAV lives (NULL = data dir) */
   int fast;                 /* virtual time: never sleep, run as fast as possible */
   int headless;             /* no window (tests) */
   int scale;                /* initial window scale */
   int fullscreen;
   int square_pixels;        /* 1 = no 4:3 aspect correction */
   const char *record_path;  /* input recording */
   const char *replay_path;  /* input replay */
   const char *shot_dir;     /* screenshot output (F12 / scripted) */
   long exit_after_ticks;    /* stop after N game timer ticks (0 = never) */
   const char *script;       /* scripted key presses "tick:key[,tick:key...]" */
   const char *soundfont;    /* .sf2 for General MIDI music (NULL = first *.sf2 in data/save dir) */
   int music_gm;             /* 1 = General MIDI via SoundFont, 0 = original FM (default), -1 = unset */
} SysConfig;

extern SysConfig sys_cfg;

int sys_init(void);
void sys_shutdown(void);

/* Call from any busy-wait loop: pumps events, fires timers, presents, sleeps a bit. */
void sys_idle(void);
/* Non-blocking variant of sys_idle() for loops that do not wait. */
void sys_poll(void);
/* Wait for the next emulated vertical retrace (70.086 Hz), present the frame. */
void sys_wait_retrace(void);
/* Text shown on top of the presented frame (outside the game's screen bitmap); "" = none. */
extern char sys_overlay[64];
/* Present the current screen + palette if anything changed. */
void sys_present(void);
/* Request to quit (window closed). The game polls this through key[KEY_ESC] etc. */
extern volatile int sys_quit_requested;

/* Emulated clock in PIT ticks (1193181 Hz). */
uint64_t sys_clock(void);

/* Files: resolve a data file name (case-insensitive) to a full path. */
uint8_t *sys_read_file(const char *name, size_t *size);
/* SoundFont for General MIDI music if that is selected (--music gm, --soundfont or
 * music=gm in spacehog.ini): sys_cfg.soundfont or the first *.sf2 in the data/save dir. */
int sys_find_soundfont(char *out, size_t outsz);
/* Open a file in the save directory. */
void *sys_fopen_save(const char *name, const char *mode);
int sys_save_path(const char *name, char *out, size_t outsz);

/* Audio: the mixer callback is called from the audio thread. */
typedef void (*sys_audio_cb)(int16_t *out, int frames, void *user);
int sys_audio_open(int rate, sys_audio_cb cb, void *user);
void sys_audio_lock(void);
void sys_audio_unlock(void);

/* Text-mode console used during the setup screen (cprintf etc.). */
void sys_textmode_putc(int c);
void sys_textmode_gotoxy(int x, int y);
void sys_textmode_color(int fg, int bg);
void sys_textmode_clrscr(void);
void sys_textmode_clreol(void);
void sys_set_textmode(int on);

void sys_log(const char *fmt, ...);
/* Fatal error message (stderr + message box when there is a window). */
void sys_error_box(const char *fmt, ...);

/* Differential-test hooks (re/refemu). Every call that can change the environment
 * (input, timers, seed) is bracketed by enter/leave; the hook sees only the outermost
 * bracket: leave = 0 on entry, 1 on exit (ret = the call's result). */
extern void (*sys_sync_hook)(int leave, int kind, int ret);
void sys_sync_enter(int kind);
int sys_sync_leave(int ret);

#ifdef __cplusplus
}
#endif
#endif
