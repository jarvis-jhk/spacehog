/* Internal glue between the Allegro compat layer and the platform layer. */
#ifndef SH_COMPAT_INTERNAL_H
#define SH_COMPAT_INTERNAL_H

#include <stdint.h>

/* timer.c */
uint64_t _timer_next_event(uint64_t now);
void _timer_fire(uint64_t now);

/* input.c */
void _keyboard_event(int scancode, int down, int ascii, int repeat);
void _mouse_motion(float dx, float dy);
void _mouse_button(int button, int down);
void _gamepad_hotplug(void);
void _input_tick_hook(void);
void _input_begin_pump(void);
int _input_replaying(void);
void input_init_recording(void);
uint32_t input_random_seed(uint32_t live_seed);
void input_net_start(uint32_t seed);
extern int (*input_net_map_key)(int sc);
extern int (*input_net_pad_slot)(void);
extern uint32_t (*input_net_hash)(void);
int _input_keybuf(int *out, int max);
int _input_pad_bits(int pad);

/* misc.c */
void dos_rand_state(uint32_t st[31], int *fp, int *rp);

/* misc.c */
void _screen_capture_hook(void);

/* textmode.c */
int _textmode_active(void);
void _textmode_render(const uint8_t **pixels, int *w, int *h, uint32_t *pal32);

/* sound.c */
void _sound_init_mixer(void);

#endif
