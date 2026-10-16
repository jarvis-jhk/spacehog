/* Digital sample mixer (Allegro 3.11 semantics: 8 voices, priority/LRU stealing) and the
 * glue that renders OPL3 FM music. Everything below the API runs in the audio thread. */
#include "alleg.h"
#include "internal.h"
#include "sound_int.h"
#include "../platform/sys.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIX_RATE 44100
#define NUM_VOICES 8   /* MIXER_DEF_SFX: what the game got on a Sound Blaster */

typedef struct VOICE {
   const SAMPLE *spl;
   int playing;
   int loop;
   int autokill;
   int priority;
   int time;
   int vol, pan;          /* 0..255 */
   uint64_t pos;          /* 32.32 fixed position in samples */
   uint64_t step;
} VOICE;

static VOICE voices[NUM_VOICES];
static int sound_installed;   /* audio device open */
static int sound_ready;       /* mixer, OPL3 and MIDI player initialised */
static int digi_volume = 255;
opl3_chip _fm_chip;

/* ---------------------------------------------------------------- audio thread */

static int sample_at(const SAMPLE *s, unsigned long i)
{
   if (s->bits == 8)
      return ((int)((const uint8_t *)s->data)[i] - 128) << 8;
   return (int)((const uint16_t *)s->data)[i] - 32768;
}

static void mix_voices(int32_t *acc, int frames)
{
   int v, f;
   for (v = 0; v < NUM_VOICES; v++) {
      VOICE *vc = &voices[v];
      const SAMPLE *s = vc->spl;
      int lvol, rvol;
      if (!vc->playing || !s || !s->len)
         continue;
      lvol = vc->vol * (255 - vc->pan) / 255 * digi_volume / 255;
      rvol = vc->vol * vc->pan / 255 * digi_volume / 255;
      for (f = 0; f < frames; f++) {
         unsigned long i = (unsigned long)(vc->pos >> 32);
         uint32_t frac = (uint32_t)(vc->pos >> 16) & 0xFFFF;
         int a, b, x;
         if (i >= s->len) {
            if (vc->loop) {
               vc->pos -= (uint64_t)s->len << 32;
               i -= s->len;
               if (i >= s->len) { vc->pos = 0; i = 0; }
            }
            else {
               vc->playing = 0;
               break;
            }
         }
         a = sample_at(s, i);
         b = (i + 1 < s->len) ? sample_at(s, i + 1) : (vc->loop ? sample_at(s, 0) : a);
         x = a + (int)(((int64_t)(b - a) * frac) >> 16);   /* linear interpolation */
         acc[f * 2] += x * lvol / 256;
         acc[f * 2 + 1] += x * rvol / 256;
         vc->pos += vc->step;
      }
   }
}

static void audio_cb(int16_t *out, int frames, void *user)
{
   static int32_t acc[4096 * 2];
   static int16_t fm[4096 * 2];
   static double pit_per_frame = 1193181.0 / MIX_RATE, pit_frac;
   int done = 0, f;
   (void)user;
   if (frames > 4096)
      frames = 4096;
   memset(acc, 0, sizeof(int32_t) * 2 * (size_t)frames);
   /* FM music: render in chunks up to the next MIDI event */
   while (done < frames) {
      long next = _midi_advance(0);
      int chunk = frames - done;
      if (next != LONG_MAX) {
         double need = ((double)next - pit_frac) / pit_per_frame;
         int n = need <= 0 ? 0 : (int)need + 1;
         if (n < chunk)
            chunk = n;
      }
      if (chunk > 0) {
         if (_gm_active())
            _gm_render(fm + done * 2, chunk);
         else
            OPL3_GenerateStream(&_fm_chip, fm + done * 2, (uint32_t)chunk);
         done += chunk;
         pit_frac += chunk * pit_per_frame;
      }
      {
         long whole = (long)pit_frac;
         pit_frac -= (double)whole;
         _midi_advance(whole);
      }
   }
   for (f = 0; f < frames * 2; f++)
      acc[f] += fm[f];
   mix_voices(acc, frames);
   for (f = 0; f < frames * 2; f++) {
      int32_t x = acc[f];
      out[f] = (int16_t)(x > 32767 ? 32767 : x < -32768 ? -32768 : x);
   }
}

/* offline rendering for tests: mix `frames` stereo frames into out */
void sound_render(int16_t *out, int frames)
{
   while (frames > 0) {
      int n = frames > 4096 ? 4096 : frames;
      audio_cb(out, n, NULL);
      out += n * 2;
      frames -= n;
   }
}

/* ---------------------------------------------------------------- API (game thread) */

int install_sound(int digi, int midi, const char *cfg_path)
{
   (void)digi; (void)midi; (void)cfg_path;
   memset(voices, 0, sizeof(voices));
   OPL3_Reset(&_fm_chip, MIX_RATE);
   midi_fm.init();
   {
      /* General MIDI through a SoundFont if one is configured or found next to the data */
      char sf2[1024];
      if (sys_find_soundfont(sf2, sizeof(sf2)) && _gm_open(sf2, MIX_RATE) == 0) {
         midi_driver = &midi_gm;
         sys_log("music: General MIDI, SoundFont %s", sf2);
      }
   }
   _midi_init();
   sound_ready = 1;
   if (sys_audio_open(MIX_RATE, audio_cb, NULL) != 0) {
      sound_installed = 0;
      return 0;   /* stay "installed" silently (headless tests) */
   }
   sound_installed = 1;
   return 0;
}

void remove_sound(void)
{
   if (sound_ready)   /* allegro_exit() may run before install_sound() */
      stop_midi();
}

void set_volume(int digi_vol, int midi_vol)
{
   sys_audio_lock();
   if (digi_vol >= 0)
      digi_volume = MID(0, digi_vol, 255);
   if (midi_vol >= 0)
      _midi_volume = MID(0, midi_vol, 255);
   sys_audio_unlock();
}

static int allocate_voice(const SAMPLE *spl)
{
   int c, best = -1, best_score = 0, score;
   for (c = 0; c < NUM_VOICES; c++)
      if (!voices[c].spl || (voices[c].autokill && !voices[c].playing))
         return c;
   for (c = 0; c < NUM_VOICES; c++) {
      VOICE *v = &voices[c];
      if (v->priority <= spl->priority) {
         score = 65536 - v->priority * 256;
         score += MID(0, retrace_count - v->time, 32768);
         if (!v->loop)
            score += 32768;
         if (score > best_score) {
            best = c;
            best_score = score;
         }
      }
   }
   return best;
}

static uint64_t step_for(const SAMPLE *spl, int freq)
{
   /* freq is relative: 1000 = original rate */
   double hz = (double)spl->freq * freq / 1000.0;
   return (uint64_t)(hz / MIX_RATE * 4294967296.0);
}

int play_sample(const SAMPLE *spl, int vol, int pan, int freq, int loop)
{
   int v;
   if (!spl)
      return -1;
   sys_audio_lock();
   v = allocate_voice(spl);
   if (v >= 0) {
      VOICE *vc = &voices[v];
      vc->spl = spl;
      vc->priority = spl->priority ? spl->priority : 128;
      vc->time = retrace_count;
      vc->vol = MID(0, vol, 255);
      vc->pan = MID(0, pan, 255);
      vc->loop = loop;
      vc->pos = 0;
      vc->step = step_for(spl, freq);
      vc->playing = 1;
      vc->autokill = 1;
   }
   sys_audio_unlock();
   return v;
}

void adjust_sample(const SAMPLE *spl, int vol, int pan, int freq, int loop)
{
   int c;
   sys_audio_lock();
   for (c = 0; c < NUM_VOICES; c++) {
      if (voices[c].spl == spl) {
         voices[c].vol = MID(0, vol, 255);
         voices[c].pan = MID(0, pan, 255);
         voices[c].step = step_for(spl, freq);
         voices[c].loop = loop;
         break;
      }
   }
   sys_audio_unlock();
}

void stop_sample(const SAMPLE *spl)
{
   int c;
   sys_audio_lock();
   for (c = 0; c < NUM_VOICES; c++)
      if (voices[c].spl == spl) {
         voices[c].spl = NULL;
         voices[c].playing = 0;
      }
   sys_audio_unlock();
}

/* ---------------------------------------------------------------- MIDI files */

static int rd32(const uint8_t *p) { return (int)((unsigned)p[0] << 24 | (unsigned)p[1] << 16 | (unsigned)p[2] << 8 | p[3]); }
static int rd16(const uint8_t *p) { return (int)(p[0] << 8 | p[1]); }

/* load a standard MIDI file (type 0/1), like Allegro's load_midi() */
MIDI *load_midi(const char *filename)
{
   FILE *f = fopen(filename, "rb");
   uint8_t *buf, *p, *end;
   long n;
   MIDI *m;
   int tracks, c;
   if (!f)
      return NULL;
   fseek(f, 0, SEEK_END);
   n = ftell(f);
   fseek(f, 0, SEEK_SET);
   buf = malloc((size_t)(n > 0 ? n : 1));
   n = (long)fread(buf, 1, (size_t)n, f);
   fclose(f);
   if (n < 14 || memcmp(buf, "MThd", 4)) {
      free(buf);
      return NULL;
   }
   tracks = rd16(buf + 10);
   if (tracks < 1 || tracks > 32) {
      free(buf);
      return NULL;
   }
   m = calloc(1, sizeof(MIDI));
   m->divisions = rd16(buf + 12);
   p = buf + 8 + rd32(buf + 4);
   end = buf + n;
   for (c = 0; c < tracks && p + 8 <= end; c++) {
      int len;
      if (memcmp(p, "MTrk", 4))
         break;
      len = rd32(p + 4);
      p += 8;
      if (len < 0 || p + len > end)
         break;
      m->track[c].len = len;
      m->track[c].data = malloc((size_t)len);
      memcpy(m->track[c].data, p, (size_t)len);
      p += len;
   }
   free(buf);
   return m;
}

void destroy_midi(MIDI *midi)
{
   int c;
   if (!midi)
      return;
   for (c = 0; c < 32; c++)
      free(midi->track[c].data);
   free(midi);
}
