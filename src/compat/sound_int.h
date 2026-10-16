/* Internal sound interfaces (mixer <-> MIDI player <-> FM driver). */
#ifndef SH_SOUND_INT_H
#define SH_SOUND_INT_H

#include <stdint.h>
#include "../third_party/nuked_opl3/opl3.h"

#define MIDI_OPL2   1
#define MIDI_2XOPL2 2
#define MIDI_OPL3   3
#define midi_card   MIDI_OPL3

typedef struct MIDI_DRIVER {
   int voices;
   int basevoice;
   int xmin, xmax;
   int (*init)(void);
   int (*load_patches)(char *patches, char *drums);
   void (*key_on)(int inst, int note, int bend, int vol, int pan);
   void (*key_off)(int voice);
   void (*set_volume)(int voice, int vol);
   void (*set_pitch)(int voice, int note, int bend);
   void (*raw_midi)(unsigned char data);   /* always NULL: FM only */
} MIDI_DRIVER;

extern MIDI_DRIVER midi_fm;
extern MIDI_DRIVER midi_gm;          /* SoundFont (gm.c) */
int _gm_open(const char *sf2_path, int rate);
void _gm_render(int16_t *out, int frames);
int _gm_active(void);
extern MIDI_DRIVER *midi_driver;
extern opl3_chip _fm_chip;
extern int _midi_volume;
extern volatile long _midi_tick;

int _midi_init(void);
int _midi_allocate_voice(int min, int max);
long _midi_advance(long pit_ticks);

#endif
