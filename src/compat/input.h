/* Joystick state as seen by the game (replaces Allegro's JOYSTICK_INFO). */
#ifndef SH_INPUT_H
#define SH_INPUT_H

#define MAX_PADS 4

enum {
   PAD_LEFT = 1 << 0, PAD_RIGHT = 1 << 1, PAD_UP = 1 << 2, PAD_DOWN = 1 << 3,
   PAD_B1 = 1 << 4, PAD_B2 = 1 << 5, PAD_B3 = 1 << 6, PAD_B4 = 1 << 7,
   PAD_B5 = 1 << 8, PAD_B6 = 1 << 9, PAD_START = 1 << 10, PAD_BACK = 1 << 11
};

typedef struct JOYSTICK_STATE {
   int bits;
   int left, right, up, down;
   int b[8];
} JOYSTICK_STATE;

/* joy[0..MAX_PADS-1] = individual pads, joy[JOY_ANY] = all pads combined */
#define JOY_ANY MAX_PADS
extern JOYSTICK_STATE joy[MAX_PADS + 1];
extern int num_joysticks;

#endif
