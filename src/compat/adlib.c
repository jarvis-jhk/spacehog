/* OPL2/OPL3 FM MIDI driver: transcription of Allegro 3.11 src/djgpp/adlib.c (giftware),
 * writing to an emulated OPL3 (Nuked OPL3) instead of I/O ports. */
#include "alleg.h"
#include "sound_int.h"
#include <string.h>
#include <stdio.h>

static int fm_load_patches(char *patches, char *drums);
static void fm_key_on(int inst, int note, int bend, int vol, int pan);
static void fm_key_off(int voice);
static void fm_set_volume(int voice, int vol);
static void fm_set_pitch(int voice, int note, int bend);


typedef struct FM_INSTRUMENT
{
   unsigned char characteristic1;
   unsigned char characteristic2;
   unsigned char level1;
   unsigned char level2;
   unsigned char attackdecay1;
   unsigned char attackdecay2;
   unsigned char sustainrelease1;
   unsigned char sustainrelease2;
   unsigned char wave1;
   unsigned char wave2;
   unsigned char feedback;
   unsigned char freq;
   unsigned char key;
   unsigned char type;
} FM_INSTRUMENT;


#define FM_HH     1
#define FM_CY     2
#define FM_TT     4
#define FM_SD     8
#define FM_BD     16


/* include the GM patch set (static data) */
#include "fm_instr.h"


/* is the OPL in percussion mode? */
static int fm_drum_mode = FALSE;

/* delays when writing to OPL registers */

/* register offsets for each voice */
static int fm_offset[18] = {
   0x000, 0x001, 0x002, 0x008, 0x009, 0x00A, 0x010, 0x011, 0x012, 
   0x100, 0x101, 0x102, 0x108, 0x109, 0x10A, 0x110, 0x111, 0x112
};

/* for converting midi note numbers to FM frequencies */
static int fm_freq[13] = {
   0x157, 0x16B, 0x181, 0x198, 0x1B0, 0x1CA,
   0x1E5, 0x202, 0x220, 0x241, 0x263, 0x287, 0x2AE
};

/* logarithmic relationship between midi and FM volumes */
static int fm_vol_table[128] = {
   0,  11, 16, 19, 22, 25, 27, 29, 32, 33, 35, 37, 39, 40, 42, 43,
   45, 46, 48, 49, 50, 51, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62,
   64, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 75, 76, 77,
   78, 79, 80, 80, 81, 82, 83, 83, 84, 85, 86, 86, 87, 88, 89, 89,
   90, 91, 91, 92, 93, 93, 94, 95, 96, 96, 97, 97, 98, 99, 99, 100,
   101, 101, 102, 103, 103, 104, 104, 105, 106, 106, 107, 107, 108,
   109, 109, 110, 110, 111, 112, 112, 113, 113, 114, 114, 115, 115,
   116, 117, 117, 118, 118, 119, 119, 120, 120, 121, 121, 122, 122,
   123, 123, 124, 124, 125, 125, 126, 126, 127
};

/* drum channel tables:          BD       SD       TT       CY       HH    */
static int fm_drum_channel[] = { 6,       7,       8,       8,       7     };
static int fm_drum_op1[] =     { TRUE,    FALSE,   TRUE,    FALSE,   TRUE  };
static int fm_drum_op2[] =     { TRUE,    TRUE,    FALSE,   TRUE,    FALSE };
static int fm_drum_pitch[] __attribute__((unused)) =   { TRUE,    TRUE,    TRUE,    FALSE,   FALSE };

/* cached information about the state of the drum channels */
static FM_INSTRUMENT *fm_drum_cached_inst1[5];
static FM_INSTRUMENT *fm_drum_cached_inst2[5];
static int fm_drum_cached_vol1[5];
static int fm_drum_cached_vol2[5];
static long fm_drum_cached_time[5];

/* various bits of information about the current state of the FM chip */
static unsigned char fm_drum_mask;
static unsigned char fm_key[18];
static unsigned char fm_keyscale[18];
static unsigned char fm_feedback[18];
static int fm_level[18];
static int fm_patch[18];

#define VOICE_OFFSET(x)     ((x < 9) ? x : 0x100+x-9)



/* fm_write:
 *  Writes a byte to the specified register on the FM chip.
 */
static void fm_write(int reg, unsigned char data)
{
   OPL3_WriteReg(&_fm_chip, (uint16_t)reg, data);
}

/* fm_reset:
 *  Resets the FM chip. If enable is set, puts OPL3 cards into OPL3 mode,
 *  otherwise puts them into OPL2 emulation mode.
 */
static void fm_reset(int enable)
{
   int i;

   for (i=0xF5; i>0; i--)
      fm_write(i, 0);

   if (midi_card == MIDI_OPL3) {          /* if we have an OPL3... */
      fm_write(0x105, 1);                 /* enable OPL3 mode */

      for (i=0x1F5; i>0x105; i--)
	 fm_write(i, 0);

      for (i=0x104; i>0x100; i--)
	 fm_write(i, 0);

      if (!enable)
	 fm_write(0x105, 0);              /* turn OPL3 mode off again */
   }
   else {
      if (midi_card == MIDI_2XOPL2) {     /* if we have a second OPL2... */
	 for (i=0x1F5; i>0x100; i--)
	    fm_write(i, 0);

	 fm_write(0x101, 0x20); 
	 fm_write(0x1BD, 0xC0); 
      }
   }

   for (i=0; i<midi_fm.voices; i++) {
      fm_key[i] = 0;
      fm_keyscale[i] = 0;
      fm_feedback[i] = 0;
      fm_level[i] = 0;
      fm_patch[i] = -1;
      fm_write(0x40+fm_offset[i], 63);
      fm_write(0x43+fm_offset[i], 63);
   }

   for (i=0; i<5; i++) {
      fm_drum_cached_inst1[i] = NULL;
      fm_drum_cached_inst2[i] = NULL;
      fm_drum_cached_vol1[i] = -1;
      fm_drum_cached_vol2[i] = -1;
      fm_drum_cached_time[i] = 0;
   }

   fm_write(0x01, 0x20);                  /* turn on wave form control */

   fm_drum_mode = FALSE;
   fm_drum_mask = 0xC0;
   fm_write(0xBD, fm_drum_mask);          /* set AM and vibrato to high */

   midi_fm.xmin = -1;
   midi_fm.xmax = -1;
}

/* fm_set_drum_mode:
 *  Switches the OPL synth between normal and percussion modes.
 */
static void fm_set_drum_mode(int usedrums)
{
   int i;

   fm_drum_mode = usedrums;
   fm_drum_mask = usedrums ? 0xE0 : 0xC0;

   midi_fm.xmin = usedrums ? 6 : -1;
   midi_fm.xmax = usedrums ? 8 : -1;

   for (i=6; i<9; i++)
      if (midi_card == MIDI_OPL3)
	 fm_write(0xC0+VOICE_OFFSET(i), 0x30);
      else
	 fm_write(0xC0+VOICE_OFFSET(i), 0);

   fm_write(0xBD, fm_drum_mask);
}

/* fm_set_voice:
 *  Sets the sound to be used for the specified voice, from a structure
 *  containing eleven bytes of FM operator data. Note that it doesn't
 *  actually set the volume: it just stores volume data in the fm_level
 *  arrays for fm_set_volume() to use.
 */
static inline void fm_set_voice(int voice, FM_INSTRUMENT *inst)
{
   /* store some info */
   fm_keyscale[voice] = inst->level2 & 0xC0;
   fm_level[voice] = 63 - (inst->level2 & 63);
   fm_feedback[voice] = inst->feedback;

   /* write the new data */
   fm_write(0x20+fm_offset[voice], inst->characteristic1);
   fm_write(0x23+fm_offset[voice], inst->characteristic2);
   fm_write(0x60+fm_offset[voice], inst->attackdecay1);
   fm_write(0x63+fm_offset[voice], inst->attackdecay2);
   fm_write(0x80+fm_offset[voice], inst->sustainrelease1);
   fm_write(0x83+fm_offset[voice], inst->sustainrelease2);
   fm_write(0xE0+fm_offset[voice], inst->wave1);
   fm_write(0xE3+fm_offset[voice], inst->wave2);

   /* don't set operator1 level for additive synthesis sounds */
   if (!(inst->feedback & 1))
      fm_write(0x40+fm_offset[voice], inst->level1);

   /* on OPL3, 0xC0 contains pan info, so don't set it until fm_key_on() */
   if (midi_card != MIDI_OPL3)
      fm_write(0xC0+VOICE_OFFSET(voice), inst->feedback);
}



/* fm_set_drum_op1:
 *  Sets the sound for operator #1 of a drum channel.
 */
static inline void fm_set_drum_op1(int voice, FM_INSTRUMENT *inst)
{
   fm_write(0x20+fm_offset[voice], inst->characteristic1);
   fm_write(0x60+fm_offset[voice], inst->attackdecay1);
   fm_write(0x80+fm_offset[voice], inst->sustainrelease1);
   fm_write(0xE0+fm_offset[voice], inst->wave1);
}



/* fm_set_drum_op2:
 *  Sets the sound for operator #2 of a drum channel.
 */
static inline void fm_set_drum_op2(int voice, FM_INSTRUMENT *inst)
{
   fm_write(0x23+fm_offset[voice], inst->characteristic2);
   fm_write(0x63+fm_offset[voice], inst->attackdecay2);
   fm_write(0x83+fm_offset[voice], inst->sustainrelease2);
   fm_write(0xE3+fm_offset[voice], inst->wave2);
}



/* fm_set_drum_vol_op1:
 *  Sets the volume for operator #1 of a drum channel.
 */
static inline void fm_set_drum_vol_op1(int voice, int vol)
{
   vol = 63 * fm_vol_table[vol] / 128;
   fm_write(0x40+fm_offset[voice], (63-vol));
}



/* fm_set_drum_vol_op2:
 *  Sets the volume for operator #2 of a drum channel.
 */
static inline void fm_set_drum_vol_op2(int voice, int vol)
{
   vol = 63 * fm_vol_table[vol] / 128;
   fm_write(0x43+fm_offset[voice], (63-vol));
}



/* fm_set_drum_pitch:
 *  Sets the pitch of a drum channel.
 */
static inline void fm_set_drum_pitch(int voice, FM_INSTRUMENT *drum)
{
   fm_write(0xA0+VOICE_OFFSET(voice), drum->freq);
   fm_write(0xB0+VOICE_OFFSET(voice), drum->key & 0x1F);
}



/* fm_trigger_drum:
 *  Triggers a note on a drum channel.
 */
static inline void fm_trigger_drum(int inst, int vol)
{
   FM_INSTRUMENT *drum = fm_drum+inst;
   int d;

   if (!fm_drum_mode)
      fm_set_drum_mode(TRUE);

   if (drum->type == FM_BD)
      d = 0;
   else if (drum->type == FM_SD)
      d = 1;
   else if (drum->type == FM_TT)
      d = 2;
   else if (drum->type == FM_CY)
      d = 3;
   else
      d = 4;

   /* don't let drum sounds come too close together */
   if (fm_drum_cached_time[d] == _midi_tick)
      return;

   fm_drum_cached_time[d] = _midi_tick;

   fm_drum_mask &= (~drum->type);
   fm_write(0xBD, fm_drum_mask);

   vol = vol*3/4;

   if (fm_drum_op1[d]) {
      if (fm_drum_cached_inst1[d] != drum) {
	 fm_drum_cached_inst1[d] = drum;
	 fm_set_drum_op1(fm_drum_channel[d], drum);
      }

      if (fm_drum_cached_vol1[d] != vol) {
	 fm_drum_cached_vol1[d] = vol;
	 fm_set_drum_vol_op1(fm_drum_channel[d], vol);
      }
   }

   if (fm_drum_op2[d]) {
      if (fm_drum_cached_inst2[d] != drum) {
	 fm_drum_cached_inst2[d] = drum;
	 fm_set_drum_op2(fm_drum_channel[d], drum);
      }

      if (fm_drum_cached_vol2[d] != vol) {
	 fm_drum_cached_vol2[d] = vol;
	 fm_set_drum_vol_op2(fm_drum_channel[d], vol);
      }
   }

   fm_set_drum_pitch(fm_drum_channel[d], drum);

   fm_drum_mask |= drum->type;
   fm_write(0xBD, fm_drum_mask);
}



/* fm_key_on:
 *  Triggers the specified voice. The instrument is specified as a GM
 *  patch number, pitch as a midi note number, and volume from 0-127.
 *  The bend parameter is _not_ expressed as a midi pitch bend value.
 *  It ranges from 0 (no pitch change) to 0xFFF (almost a semitone sharp).
 *  Drum sounds are indicated by passing an instrument number greater than
 *  128, in which case the sound is GM percussion key #(inst-128).
 */
static void fm_key_on(int inst, int note, int bend, int vol, int pan)
{
   int voice;

   if (inst > 127) {                               /* drum sound? */
      inst -= 163;
      if (inst < 0)
	 inst = 0;
      else if (inst > 46)
	 inst = 46;

      fm_trigger_drum(inst, vol);
   }
   else {                                          /* regular instrument */
      if (midi_card == MIDI_2XOPL2) {
	 /* the SB Pro-1 has fixed pan positions per voice... */
	 if (pan < 64)
	    voice = _midi_allocate_voice(0, 5);
	 else
	    voice = _midi_allocate_voice(9, midi_driver->voices-1);
      }
      else
	 /* on other cards we can use any voices */
	 voice = _midi_allocate_voice(-1, -1);

      if (voice < 0)
	 return;

      /* make sure the voice isn't sounding */
      fm_write(0x43+fm_offset[voice], 63);
      if (fm_feedback[voice] & 1)
	 fm_write(0x40+fm_offset[voice], 63);

      /* make sure the voice is set up with the right sound */
      if (inst != fm_patch[voice]) {
	 fm_set_voice(voice, fm_instrument+inst);
	 fm_patch[voice] = inst;
      }

      /* set pan position */
      if (midi_card == MIDI_OPL3) {
	 if (pan < 48)
	    pan = 0x10;
	 else if (pan >= 80)
	    pan = 0x20;
	 else
	    pan = 0x30;

	 fm_write(0xC0+VOICE_OFFSET(voice), pan | fm_feedback[voice]);
      }

      /* and play the note */
      fm_set_pitch(voice, note, bend);
      fm_set_volume(voice, vol);
   }
}

/* fm_key_off:
 *  Hey, guess what this does :-)
 */
static void fm_key_off(int voice)
{
   fm_write(0xB0+VOICE_OFFSET(voice), fm_key[voice] & 0xDF);
}

/* fm_set_volume:
 *  Sets the volume of the specified voice (vol range 0-127).
 */
static void fm_set_volume(int voice, int vol)
{
   vol = fm_level[voice] * fm_vol_table[vol] / 128;
   fm_write(0x43+fm_offset[voice], (63-vol) | fm_keyscale[voice]);
   if (fm_feedback[voice] & 1)
      fm_write(0x40+fm_offset[voice], (63-vol) | fm_keyscale[voice]);
}

/* fm_set_pitch:
 *  Sets the pitch of the specified voice.
 */
static void fm_set_pitch(int voice, int note, int bend)
{
   int oct = 1;
   int freq;

   note -= 24;
   while (note >= 12) {
      note -= 12;
      oct++;
   }

   freq = fm_freq[note];
   if (bend)
      freq += (fm_freq[note+1] - fm_freq[note]) * bend / 0x1000;

   fm_key[voice] = (oct<<2) | (freq >> 8);

   fm_write(0xA0+VOICE_OFFSET(voice), freq & 0xFF); 
   fm_write(0xB0+VOICE_OFFSET(voice), fm_key[voice] | 0x20);
}

/* fm_load_patches:
 *  Called before starting to play a MIDI file, to check if we need to be
 *  in rhythm mode or not.
 */
static int fm_load_patches(char *patches, char *drums)
{
   int i;
   int usedrums = FALSE;

   for (i=6; i<9; i++) {
      fm_key[i] = 0;
      fm_keyscale[i] = 0;
      fm_feedback[i] = 0;
      fm_level[i] = 0;
      fm_patch[i] = -1;
      fm_write(0x40+fm_offset[i], 63);
      fm_write(0x43+fm_offset[i], 63);
   }

   for (i=0; i<5; i++) {
      fm_drum_cached_inst1[i] = NULL;
      fm_drum_cached_inst2[i] = NULL;
      fm_drum_cached_vol1[i] = -1;
      fm_drum_cached_vol2[i] = -1;
      fm_drum_cached_time[i] = 0;
   }

   for (i=0; i<128; i++) {
      if (drums[i]) {
	 usedrums = TRUE;
	 break;
      }
   }

   fm_set_drum_mode(usedrums);

   return 0;
}

/* driver description used by midi_player.c: OPL3, 18 melodic voices */
static int fm_init_driver(void)
{
   fm_reset(1);
   return 0;
}

MIDI_DRIVER midi_fm = {
   18, 0, -1, -1,
   fm_init_driver,
   fm_load_patches,
   fm_key_on,
   fm_key_off,
   fm_set_volume,
   fm_set_pitch,
   NULL,
};
