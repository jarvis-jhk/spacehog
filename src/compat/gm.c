/* General MIDI music through a SoundFont (TinySoundFont), as an alternative to the FM
 * driver. Allegro's MIDI player treats it like an MPU-401: every message arrives as raw
 * bytes (raw_midi). Called from the audio thread or with the audio lock held. */
#include "alleg.h"
#include "sound_int.h"
#include <stdlib.h>
#include <string.h>

#define TSF_IMPLEMENTATION
#define TSF_STATIC
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "../third_party/tsf/tsf.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

static tsf *synth;
static unsigned char status, data[2];
static int have, need;

static int msg_len(unsigned char st)
{
   switch (st >> 4) {
      case 0xC: case 0xD: return 1;
      case 0x8: case 0x9: case 0xA: case 0xB: case 0xE: return 2;
      default: return 0;
   }
}

static void dispatch(void)
{
   int ch = status & 0x0F;
   switch (status >> 4) {
      case 0x8:
         tsf_channel_note_off(synth, ch, data[0]);
         break;
      case 0x9:
         if (data[1])
            tsf_channel_note_on(synth, ch, data[0], data[1] / 127.0f);
         else
            tsf_channel_note_off(synth, ch, data[0]);
         break;
      case 0xB:
         tsf_channel_midi_control(synth, ch, data[0], data[1]);
         break;
      case 0xC:
         tsf_channel_set_presetnumber(synth, ch, data[0], ch == 9);
         break;
      case 0xE:
         tsf_channel_set_pitchwheel(synth, ch, data[0] | (data[1] << 7));
         break;
      default:
         break;
   }
}

static void gm_raw_midi(unsigned char b)
{
   if (!synth)
      return;
   if (b & 0x80) {
      if (b >= 0xF0)          /* system messages: not sent by the player, ignore */
         return;
      status = b;
      have = 0;
      need = msg_len(b);
      return;
   }
   if (!need)
      return;
   data[have++] = b;
   if (have == need) {
      dispatch();
      have = 0;                /* running status */
   }
}

static int gm_init(void)
{
   return synth ? 0 : -1;
}

static int gm_load_patches(char *patches, char *drums)
{
   (void)patches; (void)drums;
   return 0;
}

MIDI_DRIVER midi_gm = {
   0, 0, -1, -1,
   gm_init, gm_load_patches, NULL, NULL, NULL, NULL,
   gm_raw_midi,
};

/* load a SoundFont; 0 on success */
int _gm_open(const char *sf2_path, int rate)
{
   int ch;
   synth = tsf_load_filename(sf2_path);
   if (!synth)
      return -1;
   tsf_set_output(synth, TSF_STEREO_INTERLEAVED, rate, -20.0f);   /* about the loudness of the FM music */
   tsf_set_max_voices(synth, 96);
   for (ch = 0; ch < 16; ch++)
      tsf_channel_set_bank_preset(synth, ch, ch == 9 ? 128 : 0, 0);
   return 0;
}

void _gm_render(int16_t *out, int frames)
{
   if (synth)
      tsf_render_short(synth, out, frames, 0);
   else
      memset(out, 0, sizeof(int16_t) * 2 * (size_t)frames);
}

int _gm_active(void)
{
   return synth != NULL && midi_driver == &midi_gm;
}
