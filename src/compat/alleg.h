/* Minimal Allegro 3.11 compatible API (8-bit only) used by the Spacehog port.
 *
 * Semantics follow Allegro 3.11 (giftware, Shawn Hargreaves et al.) closely so that the
 * translated game code renders pixel-identical results. Only what the game uses exists.
 */
#ifndef SH_ALLEG_H
#define SH_ALLEG_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef TRUE
#define TRUE  -1
#define FALSE 0
#endif

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MID(x, y, z) MAX((x), MIN((y), (z)))
#define ABS(x) (((x) >= 0) ? (x) : (-(x)))
#define SGN(x) (((x) >= 0) ? 1 : -1)

/* ---------------------------------------------------------------- fixed point */
typedef int32_t fixed;

extern const fixed _cos_tbl[512];
extern const fixed _tan_tbl[256];
extern const fixed _acos_tbl[513];
extern int allegro_errno;

static inline fixed itofix(int x) { return (fixed)((uint32_t)x << 16); }
static inline int fixtoi(fixed x) { return (x >> 16) + ((x & 0x8000) >> 15); }
static inline fixed fcos(fixed x) { return _cos_tbl[((x + 0x4000) >> 15) & 0x1FF]; }
static inline fixed fsin(fixed x) { return _cos_tbl[((x - 0x400000 + 0x4000) >> 15) & 0x1FF]; }
fixed fmul(fixed x, fixed y);
fixed fdiv(fixed x, fixed y);
fixed fatan(fixed x);
fixed fatan2(fixed y, fixed x);
fixed fsqrt(fixed x);

/* ---------------------------------------------------------------- bitmaps */
typedef struct BITMAP {
   int w, h;                 /* size in pixels */
   int clip;                 /* clipping enabled? */
   int cl, cr, ct, cb;       /* clip rectangle (cr/cb exclusive) */
   uint8_t *dat;             /* owned pixel memory (NULL for sub-bitmaps) */
   struct BITMAP *parent;    /* parent for sub-bitmaps */
   int x_ofs, y_ofs;
   int pitch;
   uint8_t **line;           /* row pointers */
} BITMAP;

typedef struct RLE_SPRITE {
   int w, h;
   int size;
   int8_t *dat;
} RLE_SPRITE;

typedef struct RGB { uint8_t r, g, b, filler; } RGB;
#define PAL_SIZE 256
typedef RGB PALETTE[PAL_SIZE];
typedef RGB PALLETE[PAL_SIZE];

typedef struct COLOR_MAP { uint8_t data[PAL_SIZE][PAL_SIZE]; } COLOR_MAP;
typedef struct RGB_MAP { uint8_t data[32][32][32]; } RGB_MAP;

#define FONT_SIZE 224
typedef struct FONT {
   int height;               /* 8 or 16 = fixed, -1 = proportional */
   uint8_t *fixed_dat;       /* FONT_SIZE * height bytes for fixed fonts */
   BITMAP *glyph[FONT_SIZE]; /* proportional glyphs */
} FONT;

extern BITMAP *screen;
extern FONT *font;
extern COLOR_MAP *color_map;
extern RGB_MAP *rgb_map;
extern PALETTE black_pallete;
extern PALETTE _current_pallete;

BITMAP *create_bitmap(int w, int h);
BITMAP *create_sub_bitmap(BITMAP *parent, int x, int y, int w, int h);
void destroy_bitmap(BITMAP *bmp);
void set_clip(BITMAP *bmp, int x1, int y1, int x2, int y2);
static inline int bitmap_color_depth(BITMAP *bmp) { (void)bmp; return 8; }
static inline int bitmap_mask_color(BITMAP *bmp) { (void)bmp; return 0; }

#define DRAW_MODE_SOLID          0
#define DRAW_MODE_XOR            1
#define DRAW_MODE_COPY_PATTERN   2
#define DRAW_MODE_SOLID_PATTERN  3
#define DRAW_MODE_MASKED_PATTERN 4
#define DRAW_MODE_TRANS          5
void drawing_mode(int mode, BITMAP *pattern, int x_anchor, int y_anchor);
void solid_mode(void);

void clear_to_color(BITMAP *bmp, int color);
static inline void clear(BITMAP *bmp) { clear_to_color(bmp, 0); }
void putpixel(BITMAP *bmp, int x, int y, int color);
int getpixel(BITMAP *bmp, int x, int y);
static inline void _putpixel(BITMAP *bmp, int x, int y, int color) { bmp->line[y][x] = (uint8_t)color; }
static inline int _getpixel(BITMAP *bmp, int x, int y) { return bmp->line[y][x]; }
void hline(BITMAP *bmp, int x1, int y, int x2, int color);
void vline(BITMAP *bmp, int x, int y1, int y2, int color);
void line(BITMAP *bmp, int x1, int y1, int x2, int y2, int color);
void rect(BITMAP *bmp, int x1, int y1, int x2, int y2, int color);
void rectfill(BITMAP *bmp, int x1, int y1, int x2, int y2, int color);
void circle(BITMAP *bmp, int x, int y, int radius, int color);
void circlefill(BITMAP *bmp, int x, int y, int radius, int color);
void floodfill(BITMAP *bmp, int x, int y, int color);

void blit(BITMAP *src, BITMAP *dst, int sx, int sy, int dx, int dy, int w, int h);
void masked_blit(BITMAP *src, BITMAP *dst, int sx, int sy, int dx, int dy, int w, int h);
void draw_sprite(BITMAP *bmp, BITMAP *sprite, int x, int y);
void draw_sprite_v_flip(BITMAP *bmp, BITMAP *sprite, int x, int y);
void draw_sprite_h_flip(BITMAP *bmp, BITMAP *sprite, int x, int y);
void draw_sprite_vh_flip(BITMAP *bmp, BITMAP *sprite, int x, int y);
void draw_trans_sprite(BITMAP *bmp, BITMAP *sprite, int x, int y);
void draw_lit_sprite(BITMAP *bmp, BITMAP *sprite, int x, int y, int color);
void draw_character(BITMAP *bmp, BITMAP *sprite, int x, int y, int color);
void rotate_sprite(BITMAP *bmp, BITMAP *sprite, int x, int y, fixed angle);
void rotate_scaled_sprite(BITMAP *bmp, BITMAP *sprite, int x, int y, fixed angle, fixed scale);
void stretch_sprite(BITMAP *bmp, BITMAP *sprite, int x, int y, int w, int h);
void stretch_blit(BITMAP *s, BITMAP *d, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh);

RLE_SPRITE *get_rle_sprite(BITMAP *bitmap);
void destroy_rle_sprite(RLE_SPRITE *sprite);
void draw_rle_sprite(BITMAP *bmp, RLE_SPRITE *sprite, int x, int y);
void draw_trans_rle_sprite(BITMAP *bmp, RLE_SPRITE *sprite, int x, int y);
void draw_lit_rle_sprite(BITMAP *bmp, RLE_SPRITE *sprite, int x, int y, int color);

/* ---------------------------------------------------------------- text */
void text_mode(int mode);
void textout(BITMAP *bmp, FONT *f, const char *str, int x, int y, int color);
void textout_centre(BITMAP *bmp, FONT *f, const char *str, int x, int y, int color);
void textprintf(BITMAP *bmp, FONT *f, int x, int y, int color, const char *fmt, ...);
void textprintf_centre(BITMAP *bmp, FONT *f, int x, int y, int color, const char *fmt, ...);
int text_length(FONT *f, const char *str);

/* ---------------------------------------------------------------- palette */
void set_color(int index, const RGB *p);
void set_pallete(const RGB *p);
void set_pallete_range(const RGB *p, int from, int to, int vsync);
void get_pallete(RGB *p);
void get_pallete_range(RGB *p, int from, int to);
void fade_interpolate(const RGB *source, const RGB *dest, RGB *output, int pos, int from, int to);
void fade_from_range(const RGB *source, const RGB *dest, int speed, int from, int to);
void fade_from(const RGB *source, const RGB *dest, int speed);
void fade_in(const RGB *p, int speed);
void fade_out(int speed);
int bestfit_color(const RGB *pal, int r, int g, int b);
void create_rgb_table(RGB_MAP *table, const RGB *pal, void (*callback)(int pos));
void create_light_table(COLOR_MAP *table, const RGB *pal, int r, int g, int b, void (*callback)(int pos));
void create_trans_table(COLOR_MAP *table, const RGB *pal, int r, int g, int b, void (*callback)(int pos));

/* ---------------------------------------------------------------- datafiles */
#define DAT_ID(a, b, c, d) (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(c) << 8) | (uint32_t)(d))
#define DAT_FILE        DAT_ID('F','I','L','E')
#define DAT_DATA        DAT_ID('D','A','T','A')
#define DAT_FONT        DAT_ID('F','O','N','T')
#define DAT_SAMPLE      DAT_ID('S','A','M','P')
#define DAT_MIDI        DAT_ID('M','I','D','I')
#define DAT_BITMAP      DAT_ID('B','M','P',' ')
#define DAT_RLE_SPRITE  DAT_ID('R','L','E',' ')
#define DAT_PALLETE     DAT_ID('P','A','L',' ')
#define DAT_NAME        DAT_ID('N','A','M','E')
#define DAT_END         0xFFFFFFFFu

typedef struct DATAFILE {
   void *dat;
   uint32_t type;
   long size;
   char name[32];
} DATAFILE;

void packfile_password(const char *password);
DATAFILE *load_datafile(const char *filename);
void unload_datafile(DATAFILE *dat);
DATAFILE *find_datafile_object(DATAFILE *dat, const char *name);

/* ---------------------------------------------------------------- sound */
typedef struct SAMPLE {
   int bits;                 /* 8 or 16 */
   int stereo;
   int freq;
   int priority;
   unsigned long len;
   unsigned long loop_start, loop_end;
   void *data;               /* 8-bit unsigned or 16-bit unsigned */
} SAMPLE;

typedef struct MIDI {
   int divisions;
   struct { uint8_t *data; int len; } track[32];
} MIDI;

#define DIGI_AUTODETECT -1
#define MIDI_AUTODETECT -1
#define DIGI_NONE 0
#define MIDI_NONE 0
int install_sound(int digi, int midi, const char *cfg_path);
void remove_sound(void);
void set_volume(int digi_volume, int midi_volume);
int play_sample(const SAMPLE *spl, int vol, int pan, int freq, int loop);
void adjust_sample(const SAMPLE *spl, int vol, int pan, int freq, int loop);
void stop_sample(const SAMPLE *spl);
int play_midi(MIDI *midi, int loop);
void stop_midi(void);
void midi_pause(void);
void midi_resume(void);
MIDI *load_midi(const char *filename);
void destroy_midi(MIDI *midi);
void sound_render(int16_t *out, int frames);   /* offline mixing (tests) */
extern volatile long midi_pos;

/* ---------------------------------------------------------------- timers */
#define TIMERS_PER_SECOND 1193181L
#define SECS_TO_TIMER(x) ((long)(x) * TIMERS_PER_SECOND)
#define MSEC_TO_TIMER(x) ((long)(x) * (TIMERS_PER_SECOND / 1000))
#define BPS_TO_TIMER(x)  (TIMERS_PER_SECOND / (long)(x))
int install_timer(void);
int install_int_ex(void (*proc)(void), long speed);
int install_int(void (*proc)(void), long speed);
void remove_int(void (*proc)(void));
void rest(long time);
void vsync(void);
extern volatile int retrace_count;

/* ---------------------------------------------------------------- keyboard */
extern volatile char key[128];
extern volatile int key_shifts;
int install_keyboard(void);
int keypressed(void);
int readkey(void);
void clear_keybuf(void);
void simulate_keypress(int key);

/* Allegro 3.11 scancodes (PC set 1) */
enum {
   KEY_ESC = 1, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9, KEY_0,
   KEY_MINUS, KEY_EQUALS, KEY_BACKSPACE, KEY_TAB,
   KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T, KEY_Y, KEY_U, KEY_I, KEY_O, KEY_P,
   KEY_OPENBRACE, KEY_CLOSEBRACE, KEY_ENTER, KEY_CONTROL,
   KEY_A, KEY_S, KEY_D, KEY_F, KEY_G, KEY_H, KEY_J, KEY_K, KEY_L,
   KEY_COLON, KEY_QUOTE, KEY_TILDE, KEY_LSHIFT, KEY_BACKSLASH,
   KEY_Z, KEY_X, KEY_C, KEY_V, KEY_B, KEY_N, KEY_M,
   KEY_COMMA, KEY_STOP, KEY_SLASH, KEY_RSHIFT, KEY_ASTERISK, KEY_ALT, KEY_SPACE, KEY_CAPSLOCK,
   KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10,
   KEY_NUMLOCK, KEY_SCRLOCK, KEY_HOME, KEY_UP, KEY_PGUP, KEY_MINUS_PAD, KEY_LEFT, KEY_5_PAD,
   KEY_RIGHT, KEY_PLUS_PAD, KEY_END, KEY_DOWN, KEY_PGDN, KEY_INSERT, KEY_DEL,
   KEY_PRTSCR, KEY_ALT_SYSRQ_UNUSED, KEY_BACKSLASH2, KEY_F11, KEY_F12,
   KEY_LWIN = 91, KEY_RWIN, KEY_MENU, KEY_PAD = 100,
   KEY_RCONTROL = 120, KEY_ALTGR = 121, KEY_SLASH2 = 122, KEY_PAUSE = 123
};

/* ---------------------------------------------------------------- joystick / mouse */
extern int joy_type;
int install_joystick(int type);
int poll_joystick(void);
extern volatile int mouse_x, mouse_y, mouse_b;
int install_mouse(void);

/* ---------------------------------------------------------------- misc */
#define GFX_AUTODETECT 0
#define GFX_TEXT -1
int allegro_init(void);
void allegro_exit(void);
int set_gfx_mode(int card, int w, int h, int v_w, int v_h);
int save_pcx(const char *filename, BITMAP *bmp, const RGB *pal);

#ifdef __cplusplus
}
#endif
#endif
