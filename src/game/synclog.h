/* Sync log for the differential tester (re/refemu): at every call that can change the
 * environment (input, timers) the port records the canonical game state hash on entry
 * and the environment on exit. The emulated original replays the environment and
 * compares its own state at the same points. */
#ifndef SH_SYNCLOG_H
#define SH_SYNCLOG_H

int synclog_open(const char *path, const char *dump_range);

/* used by the generated sync_state.c */
void sync_w_raw(const void *p, int n);
void sync_w_ptr(const void *p);
void sync_w_rng(void);
void sync_state_section(int i);
extern const int sync_num_sections;
extern const char *const sync_section_names[];

#endif
