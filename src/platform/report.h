/* Fault reports through Warpzone's feedback API (libwarpzone wz_report /
 * wz_report_problem, game id "spacehog").
 *
 * Caught faults (a data file that is there but cannot be read, a short SPACEHOG.SAV, a
 * netplay desync or a session that never starts) are sent in the background with a
 * stable one-line title. Crashes: the signal handler (Windows: the unhandled exception
 * filter) writes title + backtrace to crash-report.txt in the save directory; the next
 * start sends it and deletes it. No personal data: titles and details hold only the
 * version, the platform and module-relative addresses (libwarpzone also removes the
 * home directory and login name).
 *
 * Off without libwarpzone, in --headless runs (tests) and in "dev" builds, unless
 * SPACEHOG_REPORTS=1; SPACEHOG_NO_REPORTS=1 turns it off everywhere. */
#ifndef SH_REPORT_H
#define SH_REPORT_H

/* after sys_init (the save directory is known): crash handlers, pending crash report */
void report_init(void);
int report_enabled(void);
/* kind "bug"; title stays the same for the same fault, varying parts go to detail */
void report_fault(const char *title, const char *fmt, ...);

#endif
