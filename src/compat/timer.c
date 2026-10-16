/* Emulated Allegro timers. Callbacks run synchronously from sys_idle(), driven by the
 * emulated PIT clock, so game logic sees the same tick pattern in real and fast modes. */
#include "alleg.h"
#include "internal.h"
#include "../platform/sys.h"

#define MAX_TIMERS 16

static struct {
   void (*proc)(void);
   uint64_t speed;
   uint64_t due;
} timers[MAX_TIMERS];

volatile int retrace_count;

int install_timer(void)
{
   return 0;
}

int install_int_ex(void (*proc)(void), long speed)
{
   int i, slot = -1;
   if (speed <= 0)
      speed = 1;
   for (i = 0; i < MAX_TIMERS; i++) {
      if (timers[i].proc == proc) {
         slot = i;
         break;
      }
      if (!timers[i].proc && slot < 0)
         slot = i;
   }
   if (slot < 0)
      return -1;
   timers[slot].proc = proc;
   timers[slot].speed = (uint64_t)speed;
   timers[slot].due = sys_clock() + (uint64_t)speed;
   return 0;
}

int install_int(void (*proc)(void), long speed)
{
   return install_int_ex(proc, MSEC_TO_TIMER(speed));
}

void remove_int(void (*proc)(void))
{
   int i;
   for (i = 0; i < MAX_TIMERS; i++)
      if (timers[i].proc == proc)
         timers[i].proc = NULL;
}

uint64_t _timer_next_event(uint64_t now)
{
   uint64_t best = now + TIMERS_PER_SECOND;
   int i;
   for (i = 0; i < MAX_TIMERS; i++)
      if (timers[i].proc && timers[i].due < best)
         best = timers[i].due;
   return best;
}

void _timer_fire(uint64_t now)
{
   int i;
   for (i = 0; i < MAX_TIMERS; i++) {
      while (timers[i].proc && timers[i].due <= now) {
         timers[i].due += timers[i].speed;
         timers[i].proc();
      }
   }
}

void rest(long time)
{
   uint64_t end = sys_clock() + (uint64_t)MSEC_TO_TIMER(time);
   sys_sync_enter('R');
   while (sys_clock() < end)
      sys_idle();
   sys_sync_leave(0);
}

void vsync(void)
{
   sys_wait_retrace();
}
