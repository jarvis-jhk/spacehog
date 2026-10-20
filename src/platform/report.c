/* Fault reports through Warpzone (report.h). */
#if !defined(_WIN32) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE                    /* dladdr, REG_RIP */
#endif
#include "platform/report.h"
#include "platform/sys.h"
#include "platform/net.h"
#include <SDL3/SDL.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef SPACEHOG_VERSION
#define SPACEHOG_VERSION "dev"
#endif

#ifdef HAVE_WARPZONE
#include <warpzone.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <execinfo.h>
#include <fcntl.h>
#include <signal.h>
#ifdef __APPLE__
#include <sys/ucontext.h>             /* <ucontext.h> wants _XOPEN_SOURCE there */
#else
#include <ucontext.h>
#endif
#include <unistd.h>
#endif

#define MAX_REPORTS 8                  /* per run */
#define EXE_SPAN 0x4000000u            /* addresses this far above the base are "spacehog+0x..." */

static int enabled = -1;
static SDL_AtomicInt in_flight;
static int sent;
static char crash_path[1200];
static uintptr_t exe_base;

int report_enabled(void)
{
   if (enabled < 0) {
      const char *on = getenv("SPACEHOG_REPORTS"), *off = getenv("SPACEHOG_NO_REPORTS");
      if (off && *off && strcmp(off, "0"))
         enabled = 0;
      else if (on && !strcmp(on, "1"))
         enabled = 1;
      else
         enabled = !sys_cfg.headless && strcmp(SPACEHOG_VERSION, "dev") != 0;
   }
   return enabled;
}

typedef struct Job {
   char kind[8];
   char title[160];
   char *detail;
   char delete_path[1200];             /* crash file to remove once the server took it */
} Job;

static int SDLCALL send_job(void *p)
{
   Job *j = p;
   if (wz_report_problem(net_cfg.server, "spacehog", SPACEHOG_VERSION, j->kind, j->title, j->detail) == 0) {
      if (j->delete_path[0])
         remove(j->delete_path);
   }
   else
      sys_log("report: not sent (%s)", wz_last_error());
   free(j->detail);
   free(j);
   SDL_AddAtomicInt(&in_flight, -1);
   return 0;
}

static void send_background(const char *kind, const char *title, const char *detail, const char *delete_path)
{
   Job *j;
   SDL_Thread *t;
   if (sent >= MAX_REPORTS || !(j = calloc(1, sizeof(*j))))
      return;
   sent++;
   snprintf(j->kind, sizeof(j->kind), "%s", kind);
   snprintf(j->title, sizeof(j->title), "%s", title);
   j->detail = SDL_strdup(detail);
   if (delete_path)
      snprintf(j->delete_path, sizeof(j->delete_path), "%s", delete_path);
   SDL_AddAtomicInt(&in_flight, 1);
   t = SDL_CreateThread(send_job, "report", j);
   if (t)
      SDL_DetachThread(t);
   else {
      SDL_AddAtomicInt(&in_flight, -1);
      free(j->detail);
      free(j);
   }
}

/* at exit: give reports that are on their way a moment (a fault right before exit(1)) */
static void flush_reports(void)
{
   uint64_t t = SDL_GetTicks();
   while (SDL_GetAtomicInt(&in_flight) > 0 && SDL_GetTicks() - t < 3000)
      SDL_Delay(20);
}

void report_fault(const char *title, const char *fmt, ...)
{
   char detail[2048];
   int n;
   va_list ap;
   if (!report_enabled())
      return;
   n = snprintf(detail, sizeof(detail), "spacehog %s on %s\n", SPACEHOG_VERSION, SDL_GetPlatform());
   va_start(ap, fmt);
   vsnprintf(detail + n, sizeof(detail) - (size_t)n, fmt, ap);
   va_end(ap);
   sys_log("report: %s", title);
   if (net_report("bug", title, detail))
      return;
   send_background("bug", title, detail, NULL);
}

/* ---------------------------------------------------------------- crash handler
 * Only async-signal-safe calls in the handler: the file holds raw numbers, turned into
 * the report at the next start. Format: "spacehog-crash 1", "signal N", "pc X",
 * "base X", "frame X"... (hex). */

static char *hex(char *p, uintptr_t v)
{
   int i, started = 0;
   for (i = (int)sizeof(v) * 8 - 4; i >= 0; i -= 4) {
      int d = (int)(v >> i) & 15;
      if (d || started || i == 0) {
         *p++ = "0123456789abcdef"[d];
         started = 1;
      }
   }
   return p;
}

static char *put(char *p, const char *s)
{
   while (*s)
      *p++ = *s++;
   return p;
}

/* builds the crash file text in buf (no allocation); returns its length */
static size_t crash_text(char *buf, size_t cap, unsigned code, uintptr_t pc, void *const *frames, int n)
{
   char *p = buf, *end = buf + cap - 40;
   int i;
   p = put(p, "spacehog-crash 1\ncode ");
   p = hex(p, code);
   p = put(p, "\npc ");
   p = hex(p, pc);
   p = put(p, "\nbase ");
   p = hex(p, exe_base);
   *p++ = '\n';
   for (i = 0; i < n && p < end; i++) {
      p = put(p, "frame ");
      p = hex(p, (uintptr_t)frames[i]);
      *p++ = '\n';
   }
   return (size_t)(p - buf);
}

#ifdef _WIN32
static LONG WINAPI crash_filter(EXCEPTION_POINTERS *ep)
{
   static char buf[4096];
   void *frames[48];
   USHORT n = CaptureStackBackTrace(0, 48, frames, NULL);
   size_t len = crash_text(buf, sizeof(buf), (unsigned)ep->ExceptionRecord->ExceptionCode,
                           (uintptr_t)ep->ExceptionRecord->ExceptionAddress, frames, n);
   HANDLE h = CreateFileA(crash_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
   if (h != INVALID_HANDLE_VALUE) {
      DWORD w;
      WriteFile(h, buf, (DWORD)len, &w, NULL);
      CloseHandle(h);
   }
   return EXCEPTION_CONTINUE_SEARCH;
}

static void install_handlers(void)
{
   exe_base = (uintptr_t)GetModuleHandleA(NULL);
   SetUnhandledExceptionFilter(crash_filter);
}

static const char *code_name(unsigned code)
{
   switch (code) {
      case 0xC0000005u: return "access violation";
      case 0xC00000FDu: return "stack overflow";
      case 0xC0000094u: return "integer divide by zero";
      case 0xC000001Du: return "illegal instruction";
      case 0xC0000409u: return "stack buffer overrun";
      default: return "exception";
   }
}
#else
static const int crash_signals[] = { SIGSEGV, SIGBUS, SIGFPE, SIGILL, SIGABRT };

static uintptr_t context_pc(void *ucv)
{
   ucontext_t *uc = ucv;
#if defined(__APPLE__) && defined(__aarch64__)
   return (uintptr_t)uc->uc_mcontext->__ss.__pc;
#elif defined(__APPLE__) && defined(__x86_64__)
   return (uintptr_t)uc->uc_mcontext->__ss.__rip;
#elif defined(__linux__) && defined(__x86_64__)
   return (uintptr_t)uc->uc_mcontext.gregs[REG_RIP];
#elif defined(__linux__) && defined(__aarch64__)
   return (uintptr_t)uc->uc_mcontext.pc;
#else
   (void)uc;
   return 0;
#endif
}

static void crash_handler(int sig, siginfo_t *si, void *uc)
{
   static char buf[4096];
   void *frames[48];
   int n = backtrace(frames, 48), fd;
   size_t len = crash_text(buf, sizeof(buf), (unsigned)sig, context_pc(uc), frames, n);
   (void)si;
   fd = open(crash_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
   if (fd >= 0) {
      ssize_t w = write(fd, buf, len);
      (void)w;
      close(fd);
   }
   signal(sig, SIG_DFL);
   raise(sig);
}

static void install_handlers(void)
{
   struct sigaction sa;
   Dl_info info;
   void *warm[2];
   size_t i;
   static char altstack[65536];        /* so a stack overflow can still be reported */
   stack_t ss;
   if (dladdr((void *)install_handlers, &info) && info.dli_fbase)
      exe_base = (uintptr_t)info.dli_fbase;
   backtrace(warm, 2);                 /* loads the unwinder now, not in the handler */
   ss.ss_sp = altstack;
   ss.ss_size = sizeof(altstack);
   ss.ss_flags = 0;
   sigaltstack(&ss, NULL);
   memset(&sa, 0, sizeof(sa));
   sa.sa_sigaction = crash_handler;
   sa.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESETHAND;
   sigemptyset(&sa.sa_mask);
   for (i = 0; i < sizeof(crash_signals) / sizeof(crash_signals[0]); i++)
      sigaction(crash_signals[i], &sa, NULL);
}

static const char *code_name(unsigned code)
{
   switch ((int)code) {
      case SIGSEGV: return "SIGSEGV";
      case SIGBUS: return "SIGBUS";
      case SIGFPE: return "SIGFPE";
      case SIGILL: return "SIGILL";
      case SIGABRT: return "SIGABRT";
      default: return "signal";
   }
}
#endif

/* "spacehog+0x1234" for addresses in the program, else "0x..." (libraries: ASLR) */
static void where(char *out, size_t cap, uintptr_t a, uintptr_t base)
{
   if (base && a >= base && a - base < EXE_SPAN)
      snprintf(out, cap, "spacehog+0x%llx", (unsigned long long)(a - base));
   else
      snprintf(out, cap, "0x%llx", (unsigned long long)a);
}

/* a crash file from the last run: build title + detail, send, delete once sent */
static void send_pending_crash(void)
{
   size_t n;
   char *text = SDL_LoadFile(crash_path, &n), *line, *save = NULL;
   char title[160], detail[4096], w[48];
   unsigned long long v;
   uintptr_t pc = 0, base = 0, frames[48];
   unsigned code = 0;
   int nf = 0, i, first = 0, d;
   if (!text)
      return;
   if (strncmp(text, "spacehog-crash 1\n", 17) != 0) {
      SDL_free(text);
      remove(crash_path);
      return;
   }
   for (line = SDL_strtok_r(text, "\n", &save); line; line = SDL_strtok_r(NULL, "\n", &save)) {
      if (sscanf(line, "code %llx", &v) == 1) code = (unsigned)v;
      else if (sscanf(line, "pc %llx", &v) == 1) pc = (uintptr_t)v;
      else if (sscanf(line, "base %llx", &v) == 1) base = (uintptr_t)v;
      else if (sscanf(line, "frame %llx", &v) == 1 && nf < 48) frames[nf++] = (uintptr_t)v;
   }
   SDL_free(text);
   /* skip the handler's own frames: start at the faulting pc if it is on the stack */
   for (i = 0; i < nf; i++)
      if (frames[i] == pc) {
         first = i;
         break;
      }
   /* title: the fault and the first frame inside the program (stable per version) */
   where(w, sizeof(w), pc, base);
   for (i = first; i < nf; i++)
      if (base && frames[i] >= base && frames[i] - base < EXE_SPAN) {
         where(w, sizeof(w), frames[i], base);
         break;
      }
   snprintf(title, sizeof(title), "crash: %s at %s", code_name(code), w);
   d = snprintf(detail, sizeof(detail), "spacehog %s on %s\n%s (0x%x), pc ", SPACEHOG_VERSION,
                SDL_GetPlatform(), code_name(code), code);
   where(w, sizeof(w), pc, base);
   d += snprintf(detail + d, sizeof(detail) - (size_t)d, "%s\nbacktrace:\n", w);
   for (i = 0; i < nf && d < (int)sizeof(detail) - 64; i++) {
      where(w, sizeof(w), frames[i], base);
      d += snprintf(detail + d, sizeof(detail) - (size_t)d, "  #%d %s\n", i, w);
   }
   sys_log("report: %s (from the last run)", title);
   send_background("crash", title, detail, crash_path);
}

void report_init(void)
{
   if (!report_enabled())
      return;
   if (sys_save_path("crash-report.txt", crash_path, sizeof(crash_path)) != 0)
      return;
   send_pending_crash();
   install_handlers();
   atexit(flush_reports);
}

#else /* no libwarpzone: nothing to send with */

void report_init(void) {}
int report_enabled(void) { return 0; }
void report_fault(const char *title, const char *fmt, ...) { (void)title; (void)fmt; }

#endif
