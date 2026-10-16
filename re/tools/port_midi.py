"""Produce src/compat/midi_player.c and src/compat/adlib.c from Allegro 3.11 sources
(giftware) with the DOS specifics replaced. Run once; output is then maintained by hand."""
import re
root = __file__.rsplit('/', 3)[0]
A = root + '/re/allegro311/allegro/src/'
midi = open(A + 'midi.c').read().split('\n')
def lines(a, b): return '\n'.join(midi[a - 1:b])
body = lines(28, 78) + '\n' + lines(84, 127) + '\n' + lines(247, 830) + '\n' + lines(837, 1035) + '\n' + lines(1037, 1298)
body = body.replace('MIDI_TRACK ', 'MIDI_TRACK_STATE ').replace('} MIDI_TRACK;', '} MIDI_TRACK_STATE;').replace('MIDI_TRACK_STATE midi_track', 'MIDI_TRACK_STATE midi_track')
body = re.sub(r'^\s*(static )?END_OF_FUNCTION\([^)]*\);\s*$', '', body, flags=re.M)
body = re.sub(r'^\s*midi_lock_mem\(\);\s*$', '', body, flags=re.M)
body = body.replace('static void midi_lock_mem();\n', '')
body = body.replace('install_int_ex(midi_player, ', 'midi_set_timer(').replace('install_int(midi_player, 20);', 'midi_set_timer(MSEC_TO_TIMER(20));')
body = body.replace('remove_int(midi_player);', 'midi_timer_on = 0;')
body = re.sub(r'   for \(c=0; c<128; c\+\+\) \{\n      sprintf\(buf, "p%d", c\+1\);.*?\n   \}\n\n   register_datafile_object[^\n]*\n',
              '   for (c=0; c<128; c++) {\n      patch_table[c].bank1 = -1;\n      patch_table[c].bank2 = -1;\n      patch_table[c].prog = c;\n      patch_table[c].pitch = 0;\n   }\n', body, flags=re.S)
hdr = '''/* MIDI player: transcription of Allegro 3.11 src/midi.c (giftware, Shawn Hargreaves).
 * The interrupt timer is replaced by a PIT-tick countdown advanced by the audio thread. */
#include "alleg.h"
#include "sound_int.h"
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
static long midi_timer_left;
static void midi_set_timer(long speed)
{
   midi_timer_on = 1;
   midi_timer_left = speed;
}
static int midi_seek(int target) { (void)target; return -1; }

'''
open(root + '/src/compat/midi_player.c', 'w').write(hdr + body + '''

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
''')
ad = open(A + 'djgpp/adlib.c').read().split('\n')
def al(a, b): return '\n'.join(ad[a - 1:b])
fm = al(60, 155) + '\n' + al(156, 640) 
fm = re.sub(r'^\s*(static )?END_OF_FUNCTION\([^)]*\);\s*$', '', fm, flags=re.M)
open(root + '/src/compat/adlib.c', 'w').write('''/* OPL2/OPL3 FM MIDI driver: transcription of Allegro 3.11 src/djgpp/adlib.c (giftware),
 * writing to an emulated OPL3 (Nuked OPL3) instead of I/O ports. */
#include "alleg.h"
#include "sound_int.h"
#include <string.h>

''' + fm)
print('ok')
