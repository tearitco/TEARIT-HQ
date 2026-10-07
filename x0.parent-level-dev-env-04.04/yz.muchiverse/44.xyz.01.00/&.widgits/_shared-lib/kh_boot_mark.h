/* kh_boot_mark.h - timestamped startup marks, so a real start writes its own timeline.
 *
 * Owner 2026-10-06: the livedesk bottom bar is the last thing to appear at startup, and "it takes quite a while". A warm restart
 * could not reproduce it (see 04-bugs/TASKBAR-STARTUP-LATENCY-RESEARCH-2026-10-06.md), so every stage that could be the wait now
 * appends one line to #.desktop/boot_timeline.txt:   <epoch_ms> <who> <what>
 * run_khtpm_strip.sh truncates the file at the start of a boot and writes its own marks with the same format (shell side).
 * Cost: one open/append/close per mark (a handful per start); failure is silent. Header-only, static inline, no link step. */
#ifndef KH_BOOT_MARK_H
#define KH_BOOT_MARK_H

#include <stdio.h>
#include <time.h>

static inline void kh_boot_mark(const char *house_root, const char *who, const char *what) {
    char p[4096];
    struct timespec ts;
    FILE *f;
    if (!house_root || !who || !what) return;
    snprintf(p, sizeof(p), "%s/#.desktop/boot_timeline.txt", house_root);
    clock_gettime(CLOCK_REALTIME, &ts);
    if ((f = fopen(p, "a"))) {
        fprintf(f, "%lld %s %s\n", (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000L, who, what);
        fclose(f);
    }
}

#endif
