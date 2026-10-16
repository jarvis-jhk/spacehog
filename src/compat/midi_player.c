/* MIDI player: transcription of Allegro 3.11 src/midi.c (giftware, Shawn Hargreaves).
 * The interrupt timer is replaced by a PIT-tick countdown advanced by the audio thread. */
#include "alleg.h"
#include "sound_int.h"
#include "../platform/sys.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#define MIDI_VOICES 64
#define MIDI_TRACKS 32

volatile long midi_pos = -1;
static long midi_pos_counter;
int _midi_volume = -1;
volatile long _midi_tick;
MIDI_DRIVER *midi_driver = &midi_fm;

/* replacement for Allegro's timer: midi_player() wants to be called again after `speed` PIT ticks */
static int midi_timer_on;
static int midi_timer_left;
static void midi_set_timer(int speed)
{
   midi_timer_on = 1;
   midi_timer_left = speed;
}
static int midi_seek(int target) { (void)target; return -1; }


/* maximum number of layers in a single voice */
#define MIDI_LAYERS  4


typedef struct MIDI_TRACK_STATE                       /* a track in the MIDI file */
{
   unsigned char *pos;                          /* position in track data */
   int timer;                                  /* time until next event */
   unsigned char running_status;                /* last MIDI event */
} MIDI_TRACK_STATE;


typedef struct MIDI_CHANNEL                     /* a MIDI channel */
{
   int patch;                                   /* current sound */
   int volume;                                  /* volume controller */
   int pan;                                     /* pan position */
   int pitch_bend;                              /* pitch bend position */
   int new_volume;                              /* cached volume change */
   int new_pitch_bend;                          /* cached pitch bend */
   int note[128][MIDI_LAYERS];                  /* status of each note */
} MIDI_CHANNEL;


typedef struct MIDI_VOICE                       /* a voice on the soundcard */
{
   int channel;                                 /* MIDI channel */
   int note;                                    /* note (-1 = off) */
   int volume;                                  /* note velocity */
   int time;                                   /* when note was triggered */
} MIDI_VOICE;


typedef struct WAITING_NOTE                     /* a stored note-on request */
{
   int channel;
   int note;
   int volume;
} WAITING_NOTE;


typedef struct PATCH_TABLE                      /* GM -> external synth */
{
   int bank1;                                   /* controller #0 */
   int bank2;                                   /* controller #32 */
   int prog;                                    /* program change */
   int pitch;                                   /* pitch shift */
} PATCH_TABLE;


static void midi_player();                      /* core MIDI player routine */
static void prepare_to_play(MIDI *midi);

static MIDI *midifile = NULL;                   /* the file that is playing */

static int midi_loop = 0;                       /* repeat at eof? */

int midi_loop_start = -1;                      /* where to loop back to */
int midi_loop_end = -1;                        /* loop at this position */

static int midi_semaphore = 0;                  /* reentrancy flag */
static int midi_loaded_patches = FALSE;         /* loaded entire patch set? */

static int midi_timer_speed;                    /* midi_player's timer speed */
static int midi_pos_speed;                      /* MIDI delta -> midi_pos */
static int midi_speed;                          /* MIDI delta -> timer */
static int midi_new_speed;                      /* for tempo change events */

static int old_midi_volume = -1;                /* stored global volume */

static int midi_alloc_channel;                  /* so _midi_allocate_voice */
static int midi_alloc_note;                     /* knows which note the */
static int midi_alloc_vol;                      /* sound is associated with */

static MIDI_TRACK_STATE midi_track[MIDI_TRACKS];      /* the active tracks */
static MIDI_VOICE midi_voice[MIDI_VOICES];      /* synth voice status */
static MIDI_CHANNEL midi_channel[16];           /* MIDI channel info */
static WAITING_NOTE midi_waiting[MIDI_VOICES];  /* notes still to be played */
static PATCH_TABLE patch_table[128];            /* GM -> external synth */

static int midi_seeking;                        /* set during seeks */
static int midi_looping;                        /* set during loops */

/* hook functions */
void (*midi_msg_callback)(int msg, int byte1, int byte2) = NULL;
void (*midi_meta_callback)(int type, unsigned char *data, int length) = NULL;
void (*midi_sysex_callback)(unsigned char *data, int length) = NULL;



static unsigned int parse_var_len(unsigned char **data)
{
   unsigned int val = **data & 0x7F;

   while (**data & 0x80) {
      (*data)++;
      val <<= 7;
      val += (**data & 0x7F);
   }

   (*data)++;
   return val;
}

/* global_volume_fix:
 *  Converts a note volume, adjusting it according to the global 
 *  _midi_volume variable.
 */
static inline int global_volume_fix(int vol)
{
   if (_midi_volume >= 0)
      return (vol * _midi_volume) / 256;

   return vol;
}



/* sort_out_volume:
 *  Converts a note volume, adjusting it according to the channel volume
 *  and the global _midi_volume variable.
 */
static inline int sort_out_volume(int c, int vol)
{
   return global_volume_fix((vol * midi_channel[c].volume) / 128);
}



/* raw_program_change:
 *  Sends a program change message to a device capable of handling raw
 *  MIDI data, using patch mapping tables. Assumes that midi_driver->raw_midi
 *  isn't NULL, so check before calling it!
 */
static void raw_program_change(int channel, int patch)
{
   if (channel != 9) {
      /* bank change #1 */
      if (patch_table[patch].bank1 >= 0) {
	 midi_driver->raw_midi(0xB0+channel);
	 midi_driver->raw_midi(0);
	 midi_driver->raw_midi(patch_table[patch].bank1);
      }

      /* bank change #2 */
      if (patch_table[patch].bank2 >= 0) {
	 midi_driver->raw_midi(0xB0+channel);
	 midi_driver->raw_midi(32);
	 midi_driver->raw_midi(patch_table[patch].bank2);
      }

      /* program change */
      midi_driver->raw_midi(0xC0+channel);
      midi_driver->raw_midi(patch_table[patch].prog);

      /* update volume */
      midi_driver->raw_midi(0xB0+channel);
      midi_driver->raw_midi(7);
      midi_driver->raw_midi(global_volume_fix(midi_channel[channel].volume-1));
   }
}

/* midi_note_off:
 *  Processes a MIDI note-off event.
 */
static void midi_note_off(int channel, int note)
{
   int done = FALSE;
   int voice, layer;
   int c;

   /* can we send raw MIDI data? */
   if (midi_driver->raw_midi) {
      if (channel != 9)
	 note += patch_table[midi_channel[channel].patch].pitch;

      midi_driver->raw_midi(0x80+channel);
      midi_driver->raw_midi(note);
      midi_driver->raw_midi(0);
      return;
   }

   /* oh well, have to do it the int way... */
   for (layer=0; layer<MIDI_LAYERS; layer++) {
      voice = midi_channel[channel].note[note][layer];
      if (voice >= 0) {
	 midi_driver->key_off(voice + midi_driver->basevoice);
	 midi_voice[voice].note = -1;
	 midi_voice[voice].time = _midi_tick;
	 midi_channel[channel].note[note][layer] = -1; 
	 done = TRUE;
      }
   }

   /* if the note isn't playing, it must still be in the waiting room */
   if (!done) {
      for (c=0; c<MIDI_VOICES; c++) {
	 if ((midi_waiting[c].channel == channel) && 
	     (midi_waiting[c].note == note)) {
	    midi_waiting[c].note = -1;
	    break;
	 }
      }
   }
}

/* sort_out_pitch_bend:
 *  MIDI pitch bend range is + or - two semitones. The low-level soundcard
 *  drivers can only handle bends up to +1 semitone. This routine converts
 *  pitch bends from MIDI format to our own format.
 */
static inline void sort_out_pitch_bend(int *bend, int *note)
{
   if (*bend == 0x2000) {
      *bend = 0;
      return;
   }

   (*bend) -= 0x2000;

   while (*bend < 0) {
      (*note)--;
      (*bend) += 0x1000;
   }

   while (*bend >= 0x1000) {
      (*note)++;
      (*bend) -= 0x1000;
   }
}



/* _midi_allocate_voice:
 *  Allocates a MIDI voice in the range min-max (inclusive). This is 
 *  intended to be called by the key_on() handlers in the MIDI driver, 
 *  and shouldn't be used by any other code.
 */
int _midi_allocate_voice(int min, int max)
{
   int c;
   int layer;
   int voice = -1;
   int best_time = INT_MAX;

   if (min < 0)
      min = 0;

   if (max < 0)
      max = midi_driver->voices-1;

   /* which layer can we use? */
   for (layer=0; layer<MIDI_LAYERS; layer++)
      if (midi_channel[midi_alloc_channel].note[midi_alloc_note][layer] < 0)
	 break; 

   if (layer >= MIDI_LAYERS)
      return -1;

   /* find a free voice */
   for (c=min; c<=max; c++) {
      if ((midi_voice[c].note < 0) && 
	  (midi_voice[c].time < best_time) && 
	  ((c < midi_driver->xmin) || (c > midi_driver->xmax))) {
	 voice = c;
	 best_time = midi_voice[c].time;
      }
   }

   /* if there are no free voices, kill a note to make room */
   if (voice < 0) {
      voice = -1;
      best_time = INT_MAX;
      for (c=min; c<=max; c++) {
	 if ((midi_voice[c].time < best_time) && 
	     ((c < midi_driver->xmin) || (c > midi_driver->xmax))) {
	    voice = c;
	    best_time = midi_voice[c].time;
	 }
      }
      if (voice >= 0)
	 midi_note_off(midi_voice[voice].channel, midi_voice[voice].note);
      else
	 return -1;
   }

   /* ok, we got it... */
   midi_voice[voice].channel = midi_alloc_channel;
   midi_voice[voice].note = midi_alloc_note;
   midi_voice[voice].volume = midi_alloc_vol;
   midi_voice[voice].time = _midi_tick;
   midi_channel[midi_alloc_channel].note[midi_alloc_note][layer] = voice; 

   return voice + midi_driver->basevoice;
}

/* midi_note_on:
 *  Processes a MIDI note-on event. Tries to find a free soundcard voice,
 *  and if it can't either cuts off an existing note, or if 'polite' is
 *  set, just stores the channel, note and volume in the waiting list.
 */
static void midi_note_on(int channel, int note, int vol, int polite)
{
   int c, layer, inst, bend, corrected_note;

   /* it's easy if the driver can handle raw MIDI data */
   if (midi_driver->raw_midi) {
      if (channel != 9)
	 note += patch_table[midi_channel[channel].patch].pitch;

      midi_driver->raw_midi(0x90+channel);
      midi_driver->raw_midi(note);
      midi_driver->raw_midi(vol);
      return;
   }

   /* if the note is already on, turn it off */
   for (layer=0; layer<MIDI_LAYERS; layer++) {
      if (midi_channel[channel].note[note][layer] >= 0) {
	 midi_note_off(channel, note);
	 return;
      }
   }

   /* if zero volume and the note isn't playing, we can just ignore it */
   if (vol == 0)
      return;

   if (channel != 9) {
      /* are there any free voices? */
      for (c=0; c<midi_driver->voices; c++)
	 if ((midi_voice[c].note < 0) && 
	     ((c < midi_driver->xmin) || (c > midi_driver->xmax)))
	    break;

      /* if there are no free voices, remember the note for later */
      if ((c >= midi_driver->voices) && (polite)) {
	 for (c=0; c<MIDI_VOICES; c++) {
	    if (midi_waiting[c].note < 0) {
	       midi_waiting[c].channel = channel;
	       midi_waiting[c].note = note;
	       midi_waiting[c].volume = vol;
	       break;
	    }
	 }
	 return;
      }
   }

   /* drum sound? */
   if (channel == 9) {
      inst = 128+note;
      corrected_note = 60;
      bend = 0;
   }
   else {
      inst = midi_channel[channel].patch;
      corrected_note = note;
      bend = midi_channel[channel].pitch_bend;
      sort_out_pitch_bend(&bend, &corrected_note);
   }

   /* play the note */
   midi_alloc_channel = channel;
   midi_alloc_note = note;
   midi_alloc_vol = vol;

   midi_driver->key_on(inst, corrected_note, bend, 
		       sort_out_volume(channel, vol), 
		       midi_channel[channel].pan);
}

/* all_notes_off:
 *  Turns off all active notes.
 */
static void all_notes_off(int channel)
{
   if (midi_driver->raw_midi) {
      midi_driver->raw_midi(0xB0+channel);
      midi_driver->raw_midi(123);
      midi_driver->raw_midi(0);
      return;
   }
   else {
      int note, layer;

      for (note=0; note<128; note++)
	 for (layer=0; layer<MIDI_LAYERS; layer++)
	    if (midi_channel[channel].note[note][layer] >= 0)
	       midi_note_off(channel, note);
   }
}

/* reset_controllers:
 *  Resets volume, pan, pitch bend, etc, to default positions.
 */
static void reset_controllers(int channel)
{
   midi_channel[channel].new_volume = 128;
   midi_channel[channel].new_pitch_bend = 0x2000;

   switch (channel % 3) {
      case 0:  midi_channel[channel].pan = ((channel/3) & 1) ? 60 : 68; break;
      case 1:  midi_channel[channel].pan = 104; break;
      case 2:  midi_channel[channel].pan = 24; break;
   }

   if (midi_driver->raw_midi) {
      midi_driver->raw_midi(0xB0+channel);
      midi_driver->raw_midi(10);
      midi_driver->raw_midi(midi_channel[channel].pan);
   }
}

/* update_controllers:
 *  Checks cached controller information and updates active voices.
 */
static void update_controllers()
{
   int c, c2;

   for (c=0; c<16; c++) {
      /* check for volume controller change */
      if ((midi_channel[c].volume != midi_channel[c].new_volume) ||
	  (old_midi_volume != _midi_volume)) {
	 midi_channel[c].volume = midi_channel[c].new_volume;
	 if (midi_driver->raw_midi) {
	    midi_driver->raw_midi(0xB0+c);
	    midi_driver->raw_midi(7);
	    midi_driver->raw_midi(global_volume_fix(midi_channel[c].volume-1));
	 }
	 else {
	    for (c2=0; c2<MIDI_VOICES; c2++) {
	       if ((midi_voice[c2].channel == c) &&
		  (midi_voice[c2].note >= 0)) {
		  int vol = sort_out_volume(c, midi_voice[c2].volume);
		  midi_driver->set_volume(c2 + midi_driver->basevoice, vol);
	       }
	    }
	 }
      }

      /* check for pitch bend change */
      if (midi_channel[c].pitch_bend != midi_channel[c].new_pitch_bend) {
	 midi_channel[c].pitch_bend = midi_channel[c].new_pitch_bend;
	 if (midi_driver->raw_midi) {
	    midi_driver->raw_midi(0xE0+c);
	    midi_driver->raw_midi(midi_channel[c].pitch_bend & 0x7F);
	    midi_driver->raw_midi(midi_channel[c].pitch_bend >> 7);
	 }
	 else {
	    for (c2=0; c2<MIDI_VOICES; c2++) {
	       if ((midi_voice[c2].channel == c) &&
		  (midi_voice[c2].note >= 0)) {
		  int bend = midi_channel[c].pitch_bend;
		  int note = midi_voice[c2].note;
		  sort_out_pitch_bend(&bend, &note);
		  midi_driver->set_pitch(c2 + midi_driver->basevoice, note, bend);
	       }
	    }
	 }
      }
   }

   old_midi_volume = _midi_volume;
}

/* process_controller:
 *  Deals with a MIDI controller message on the specified channel.
 */
static void process_controller(int channel, int ctrl, int data)
{
   switch (ctrl) {

      case 7:                                   /* main volume */
	 midi_channel[channel].new_volume = data+1;
	 break;

      case 10:                                  /* pan */
	 midi_channel[channel].pan = data;
	 if (midi_driver->raw_midi) {
	    midi_driver->raw_midi(0xB0+channel);
	    midi_driver->raw_midi(10);
	    midi_driver->raw_midi(data);
	 }
	 break;

      case 121:                                 /* reset all controllers */
	 reset_controllers(channel);
	 break;

      case 123:                                 /* all notes off */
      case 124:                                 /* omni mode off */
      case 125:                                 /* omni mode on */
      case 126:                                 /* poly mode off */
      case 127:                                 /* poly mode on */
	 all_notes_off(channel);
	 break;
   }
}

/* process_meta_event:
 *  Processes the next meta-event on the specified track.
 */
static void process_meta_event(unsigned char **pos, int *timer)
{
   unsigned char metatype = *((*pos)++);
   int length = parse_var_len(pos);
   int tempo;

   if (midi_meta_callback)
      midi_meta_callback(metatype, *pos, length);

   if (metatype == 0x2F) {                      /* end of track */
      *pos = NULL;
      *timer = INT_MAX;
      return;
   }

   if (metatype == 0x51) {                      /* tempo change */
      tempo = (*pos)[0] * 0x10000L + (*pos)[1] * 0x100 + (*pos)[2];
      midi_new_speed = (tempo/1000) * (TIMERS_PER_SECOND/1000);
      midi_new_speed /= midifile->divisions;
   }

   (*pos) += length;
}

/* process_midi_event:
 *  Processes the next MIDI event on the specified track.
 */
static void process_midi_event(unsigned char **pos, unsigned char *running_status, int *timer)
{
   unsigned char byte1, byte2; 
   int channel;
   unsigned char event;
   int l;

   event = *((*pos)++); 

   if (event & 0x80) {                          /* regular message */
      /* no running status for sysex and meta-events! */
      if ((event != 0xF0) && (event != 0xF7) && (event != 0xFF))
	 *running_status = event;
      byte1 = (*pos)[0];
      byte2 = (*pos)[1];
   }
   else {                                       /* use running status */
      byte1 = event; 
      byte2 = (*pos)[0];
      event = *running_status; 
      (*pos)--;
   }

   /* program callback? */
   if ((midi_msg_callback) && 
       (event != 0xF0) && (event != 0xF7) && (event != 0xFF))
      midi_msg_callback(event, byte1, byte2);

   channel = event & 0x0F;

   switch (event>>4) {

      case 0x08:                                /* note off */
	 midi_note_off(channel, byte1);
	 (*pos) += 2;
	 break;

      case 0x09:                                /* note on */
	 midi_note_on(channel, byte1, byte2, 1);
	 (*pos) += 2;
	 break;

      case 0x0A:                                /* note aftertouch */
	 (*pos) += 2;
	 break;

      case 0x0B:                                /* control change */
	 process_controller(channel, byte1, byte2);
	 (*pos) += 2;
	 break;

      case 0x0C:                                /* program change */
	 midi_channel[channel].patch = byte1;
	 if (midi_driver->raw_midi)
	    raw_program_change(channel, byte1);
	 (*pos) += 1;
	 break;

      case 0x0D:                                /* channel aftertouch */
	 (*pos) += 1;
	 break;

      case 0x0E:                                /* pitch bend */
	 midi_channel[channel].new_pitch_bend = byte1 + (byte2<<7);
	 (*pos) += 2;
	 break;

      case 0x0F:                                /* special event */
	 switch (event) {
	    case 0xF0:                          /* sysex */
	    case 0xF7: 
	       l = parse_var_len(pos);
	       if (midi_sysex_callback)
		  midi_sysex_callback(*pos, l);
	       (*pos) += l;
	       break;

	    case 0xF2:                          /* song position */
	       (*pos) += 2;
	       break;

	    case 0xF3:                          /* song select */
	       (*pos)++;
	       break;

	    case 0xFF:                          /* meta-event */
	       process_meta_event(pos, timer);
	       break;

	    default:
	       /* the other special events don't have any data bytes,
		  so we don't need to bother skipping past them */
	       break;
	 }
	 break;

      default:
	 /* something has gone badly wrong if we ever get to here */
	 break;
   }
}

static void midi_player()
{
   int c;
   int l;
   int active;

   if (!midifile)
      return;

   if (midi_semaphore) {
      midi_timer_speed += BPS_TO_TIMER(40);
      midi_set_timer(BPS_TO_TIMER(40));
      return;
   }

   midi_semaphore = TRUE;
   _midi_tick++;

   do_it_all_again:

   for (c=0; c<MIDI_VOICES; c++)
      midi_waiting[c].note = -1;

   /* deal with each track in turn... */
   for (c=0; c<MIDI_TRACKS; c++) { 
      if (midi_track[c].pos) {
	 midi_track[c].timer -= midi_timer_speed;

	 /* while events are waiting, process them */
	 while (midi_track[c].timer <= 0) { 
	    process_midi_event(&midi_track[c].pos, 
			       &midi_track[c].running_status,
			       &midi_track[c].timer); 

	    /* read next time offset */
	    if (midi_track[c].pos) { 
	       l = parse_var_len(&midi_track[c].pos);
	       l *= midi_speed;
	       midi_track[c].timer += l;
	    }
	 }
      }
   }

   /* update global position value */
   midi_pos_counter -= midi_timer_speed;
   while (midi_pos_counter <= 0) {
      midi_pos_counter += midi_pos_speed;
      midi_pos++;
   }

   /* tempo change? */
   if (midi_new_speed > 0) {
      for (c=0; c<MIDI_TRACKS; c++) {
	 if (midi_track[c].pos) {
	    midi_track[c].timer /= midi_speed;
	    midi_track[c].timer *= midi_new_speed;
	 }
      }
      midi_pos_counter /= midi_speed;
      midi_pos_counter *= midi_new_speed;

      midi_speed = midi_new_speed;
      midi_pos_speed = midi_new_speed * midifile->divisions;
      midi_new_speed = -1;
   }

   /* figure out how int until we need to be called again */
   active = 0;
   midi_timer_speed = INT_MAX;
   for (c=0; c<MIDI_TRACKS; c++) {
      if (midi_track[c].pos) {
	 active = 1;
	 if (midi_track[c].timer < midi_timer_speed)
	    midi_timer_speed = midi_track[c].timer;
      }
   }

   /* end of the music? */
   if ((!active) || ((midi_loop_end > 0) && (midi_pos >= midi_loop_end))) {
      if ((midi_loop) && (!midi_looping)) {
	 if (midi_loop_start > 0) {
	    midi_timer_on = 0;
	    midi_semaphore = FALSE;
	    midi_looping = TRUE;
	    if (midi_seek(midi_loop_start) != 0) {
	       midi_looping = FALSE;
	       stop_midi(); 
	       return;
	    }
	    midi_looping = FALSE;
	    midi_semaphore = TRUE;
	    goto do_it_all_again;
	 }
	 else {
	    for (c=0; c<16; c++)
	       all_notes_off(c);
	    prepare_to_play(midifile);
	    goto do_it_all_again;
	 }
      }
      else {
	 stop_midi(); 
	 midi_semaphore = FALSE;
	 return;
      }
   }

   /* reprogram the timer */
   if (midi_timer_speed < BPS_TO_TIMER(40))
      midi_timer_speed = BPS_TO_TIMER(40);

   if (!midi_seeking) 
      midi_set_timer(midi_timer_speed);

   /* controller changes are cached and only processed here, so we can 
      condense streams of controller data into just a few voice updates */ 
   update_controllers();

   /* and deal with any notes that are still waiting to be played */
   for (c=0; c<MIDI_VOICES; c++)
      if (midi_waiting[c].note >= 0)
	 midi_note_on(midi_waiting[c].channel, midi_waiting[c].note,
		      midi_waiting[c].volume, 0);

   midi_semaphore = FALSE;
}

/* midi_init:
 *  Sets up the MIDI player ready for use. Returns non-zero on failure.
 */
static int midi_init()
{
   int c, c2, c3;

   midi_loaded_patches = FALSE;

   for (c=0; c<16; c++) {
      midi_channel[c].volume = 128;
      midi_channel[c].pitch_bend = 0x2000;

      for (c2=0; c2<128; c2++)
	 for (c3=0; c3<MIDI_LAYERS; c3++)
	    midi_channel[c].note[c2][c3] = -1;
   }

   for (c=0; c<MIDI_VOICES; c++) {
      midi_voice[c].note = -1;
      midi_voice[c].time = 0;
   }

   for (c=0; c<128; c++) {
      patch_table[c].bank1 = -1;
      patch_table[c].bank2 = -1;
      patch_table[c].prog = c;
      patch_table[c].pitch = 0;
   }

   return 0;
}



/* midi_exit:
 *  Turns off all active notes and removes the timer handler.
 */



/* load_patches: scan the MIDI data for used instruments and drums */
static int load_patches(MIDI *midi)
{
   char patches[128], drums[128];
   unsigned char *p, *end;
   unsigned char running_status, event;
   int l;
   int c;

   for (c=0; c<128; c++)                        /* initialise to unused */
      patches[c] = drums[c] = FALSE;

   patches[0] = TRUE;                           /* always load the piano */

   for (c=0; c<MIDI_TRACKS; c++) {              /* for each track... */
      p = midi->track[c].data;
      end = p + midi->track[c].len;
      running_status = 0;

      while (p < end) {                         /* work through data stream */
	 event = *p; 
	 if (event & 0x80) {                    /* regular message */
	    p++;
	    if ((event != 0xF0) && (event != 0xF7) && (event != 0xFF))
	       running_status = event;
	 }
	 else                                   /* use running status */
	    event = running_status; 

	 switch (event>>4) {

	    case 0x0C:                          /* program change! */
	       patches[*p] = TRUE;
	       p++;
	       break;

	    case 0x09:                          /* note on, is it a drum? */
	       if ((event & 0x0F) == 9)
		  drums[*p] = TRUE;
	       p += 2;
	       break;

	    case 0x08:                          /* note off */
	    case 0x0A:                          /* note aftertouch */
	    case 0x0B:                          /* control change */
	    case 0x0E:                          /* pitch bend */
	       p += 2;
	       break;

	    case 0x0D:                          /* channel aftertouch */
	       p += 1;
	       break;

	    case 0x0F:                          /* special event */
	       switch (event) {
		  case 0xF0:                    /* sysex */
		  case 0xF7: 
		     l = parse_var_len(&p);
		     p += l;
		     break;

		  case 0xF2:                    /* song position */
		     p += 2;
		     break;

		  case 0xF3:                    /* song select */
		     p++;
		     break;

		  case 0xFF:                    /* meta-event */
		     p++;
		     l = parse_var_len(&p);
		     p += l;
		     break;

		  default:
		     /* the other special events don't have any data bytes,
			so we don't need to bother skipping past them */
		     break;
	       }
	       break;

	    default:
	       /* something has gone badly wrong if we ever get to here */
	       break;
	 }

	 if (p < end)                           /* skip time offset */
	    parse_var_len(&p);
      }
   }

   /* tell the driver to do its stuff */ 
   return midi_driver->load_patches(patches, drums);
}



/* prepare_to_play:
 *  Sets up all the global variables needed to play the specified file.
 */
static void prepare_to_play(MIDI *midi)
{
   int c;

   for (c=0; c<16; c++)
      reset_controllers(c);

   update_controllers();

   midifile = midi;
   midi_pos = 0;
   midi_pos_counter = 0;
   midi_speed = TIMERS_PER_SECOND / 2 / midifile->divisions;   /* 120 bpm */
   midi_new_speed = -1;
   midi_pos_speed = midi_speed * midifile->divisions;
   midi_timer_speed = 0;
   midi_seeking = 0;
   midi_looping = 0;

   for (c=0; c<16; c++) {
      midi_channel[c].patch = 0;
      if (midi_driver->raw_midi)
	 raw_program_change(c, 0);
   }

   for (c=0; c<MIDI_TRACKS; c++) {
      if (midi->track[c].data) {
	 midi_track[c].pos = midi->track[c].data;
	 midi_track[c].timer = parse_var_len(&midi_track[c].pos);
	 midi_track[c].timer *= midi_speed;
      }
      else {
	 midi_track[c].pos = NULL;
	 midi_track[c].timer = INT_MAX;
      }
      midi_track[c].running_status = 0;
   }
}

/* play_midi:
 *  Starts playing the specified MIDI file. If loop is set, the MIDI file 
 *  will be repeated until replaced with something else, otherwise it will 
 *  stop at the end of the file. Passing a NULL MIDI file will stop whatever 
 *  music is currently playing: allegro.h defines the macro stop_midi() to 
 *  be play_midi(NULL, FALSE); Returns non-zero if an error occurs (this
 *  may happen if a patch-caching wavetable driver is unable to load the
 *  required samples).
 */
static int locked_play_midi(MIDI *midi, int loop) {
   int c;

   midi_timer_on = 0;

   for (c=0; c<16; c++)
      all_notes_off(c);

   if (midi) {
      if (!midi_loaded_patches)
	 if (load_patches(midi) != 0)
	    return -1;

      midi_loop = loop;
      midi_loop_start = -1;
      midi_loop_end = -1;

      prepare_to_play(midi);

      /* arbitrary speed, midi_player() will adjust it */
      midi_set_timer(MSEC_TO_TIMER(20));
   }
   else {
      midifile = NULL;

      if (midi_pos > 0)
	 midi_pos = -midi_pos;
      else if (midi_pos == 0)
	 midi_pos = -1;
   }

   return 0;
}

int play_midi(MIDI *midi, int loop)
{
   int r;
   sys_audio_lock();
   r = locked_play_midi(midi, loop);
   sys_audio_unlock();
   return r;
}

/* play_looped_midi:
 *  Like play_midi(), but the file loops from the specified end position
 *  back to the specified start position (the end position can be -1 to 
 *  indicate the end of the file).
 */
int play_looped_midi(MIDI *midi, int loop_start, int loop_end)
{
   if (play_midi(midi, TRUE) != 0)
      return -1;

   midi_loop_start = loop_start;
   midi_loop_end = loop_end;

   return 0;
}



/* stop_midi:
 *  Stops whatever MIDI file is currently playing.
 */
void stop_midi()
{
   play_midi(NULL, FALSE);
}

/* midi_pause:
 *  Pauses the currently playing MIDI file.
 */
static void locked_midi_pause(void) {
   int c;

   if (!midifile)
      return;

   midi_timer_on = 0;

   for (c=0; c<16; c++)
      all_notes_off(c);
}

void midi_pause(void)
{
   sys_audio_lock();
   locked_midi_pause();
   sys_audio_unlock();
}

/* midi_resume:
 *  Resumes playing a paused MIDI file.
 */
static void locked_midi_resume(void) {
   if (!midifile)
      return;

   midi_set_timer(midi_timer_speed);
}

void midi_resume(void)
{
   sys_audio_lock();
   locked_midi_resume();
   sys_audio_unlock();
}

/* called by the mixer: advance the MIDI clock by `ticks` PIT ticks, returns ticks until the
 * next player call (or LONG_MAX if idle) */
long _midi_advance(long ticks)
{
   if (!midi_timer_on || !midifile)
      return LONG_MAX;
   midi_timer_left -= ticks;
   while (midi_timer_on && midifile && midi_timer_left <= 0) {
      long over = midi_timer_left;
      midi_timer_on = 0;
      midi_player();            /* re-arms the timer via midi_set_timer() */
      if (midi_timer_on)
         midi_timer_left += over;
   }
   return (midi_timer_on && midifile) ? midi_timer_left : LONG_MAX;
}

int _midi_init(void)
{
   return midi_init();
}
