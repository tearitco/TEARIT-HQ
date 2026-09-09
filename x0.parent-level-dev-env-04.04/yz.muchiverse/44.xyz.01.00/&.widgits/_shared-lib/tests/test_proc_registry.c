/* test_proc_registry.c — standalone, desktop-safe test for
 * kh_proc_registry.h. Spawns its own detached dummy children, no house
 * processes touched.
 *
 *   cc -std=c11 -Wall -Wextra -O2 test_proc_registry.c -o /tmp/tpr && /tmp/tpr
 *
 * Cases:
 *   1. register N setsid children, kh_proc_reap_all -> all dead, file empty
 *   2. one child ignores SIGTERM -> still killed by the SIGKILL phase
 *   3. a stale line (dead pid) and a reused-PID-shaped line (live pid,
 *      wrong starttime) are BOTH skipped, not acted on
 *   4. kh_proc_reap_all never signals the test process itself / its group
 *   5. kh_proc_reap_one removes just one entry
 */
#define _GNU_SOURCE
#define KH_PROC_REGISTRY_IMPL
#include "../kh_proc_registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (cond) { printf("  ok   %s\n", msg); } \
    else { printf("  FAIL %s\n", msg); failures++; } } while (0)

static const char *HR;   /* fake house root (a tmp dir) */

static void msleep(int ms) {
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

static int alive(long pid) { return kill((pid_t)pid, 0) == 0 || errno == EPERM; }

/* fork a detached child in its own session; optionally ignore SIGTERM.
 * returns the child pid (== its pgid, because setsid). */
static long spawn_child(int ignore_term) {
    pid_t p = fork();
    if (p == 0) {
        setsid();
        if (ignore_term) signal(SIGTERM, SIG_IGN);
        for (;;) pause();     /* wait for a signal; SIGKILL still lands */
        _exit(0);
    }
    return (long)p;
}

static long count_registry_lines(void) {
    char path[2048]; kh_proc_registry_path(HR, path, sizeof(path));
    FILE *f = fopen(path, "r"); if (!f) return -1;
    long n = 0; int c, prev = '\n';
    while ((c = fgetc(f)) != EOF) { if (c == '\n') n++; prev = c; }
    if (prev != '\n' && n >= 0) n++;   /* last line w/o newline */
    fclose(f);
    return n;
}

int main(void) {
    char tmpl[] = "/tmp/tpr_house_XXXXXX";
    char *dir = mkdtemp(tmpl);
    if (!dir) { perror("mkdtemp"); return 2; }
    char desktop[2200];
    snprintf(desktop, sizeof(desktop), "%s/#.desktop", dir);
    if (mkdir(desktop, 0755) != 0) { perror("mkdir #.desktop"); return 2; }
    HR = dir;
    printf("fake house root: %s\n", HR);

    /* ---- case 1 + 2: reap N children, one SIGTERM-immune ---------- */
    printf("\n[case 1+2] reap 3 detached children (one ignores SIGTERM)\n");
    kh_proc_registry_reset(HR);
    long c1 = spawn_child(0);
    long c2 = spawn_child(0);
    long c3 = spawn_child(1);           /* ignores SIGTERM */
    msleep(100);
    kh_proc_register(HR, c1, c1, "dummy-a");
    kh_proc_register(HR, c2, c2, "dummy-b");
    kh_proc_register(HR, c3, c3, "dummy-c-termproof");
    CHECK(alive(c1) && alive(c2) && alive(c3), "all 3 children alive pre-reap");
    CHECK(count_registry_lines() == 3, "registry has 3 lines");

    int sig = kh_proc_reap_all(HR, 200, 1);
    /* let waitpid/kernel settle */
    for (int i = 0; i < 50 && (alive(c1) || alive(c2) || alive(c3)); i++) msleep(20);
    /* reap zombies we created */
    waitpid((pid_t)c1, NULL, WNOHANG);
    waitpid((pid_t)c2, NULL, WNOHANG);
    waitpid((pid_t)c3, NULL, WNOHANG);
    CHECK(sig == 3, "reap_all signalled 3 processes");
    CHECK(!alive(c1) && !alive(c2), "plain children dead after reap");
    CHECK(!alive(c3), "SIGTERM-immune child dead after reap (SIGKILL phase)");
    CHECK(count_registry_lines() == 0, "registry truncated after reap");

    /* ---- case 3: stale + reused-PID lines are skipped ------------- */
    printf("\n[case 3] stale pid + reused-PID-shaped line are not acted on\n");
    kh_proc_registry_reset(HR);
    long live = spawn_child(0);
    msleep(100);
    /* a genuinely dead pid: spawn then wait it out */
    pid_t deadp = fork();
    if (deadp == 0) _exit(0);
    waitpid(deadp, NULL, 0);

    char path[2048]; kh_proc_registry_path(HR, path, sizeof(path));
    FILE *f = fopen(path, "w");
    /* line A: dead pid, bogus starttime */
    fprintf(f, "%ld %ld 999999 dead-pid\n", (long)deadp, (long)deadp);
    /* line B: LIVE pid but WRONG starttime -> must be treated as reused */
    fprintf(f, "%ld %ld 1 reused-pid-shape\n", live, live);
    fclose(f);

    int sig3 = kh_proc_reap_all(HR, 100, 1);
    msleep(50);
    CHECK(sig3 == 0, "reap_all signalled nothing (both lines rejected)");
    CHECK(alive(live), "the live process with a mismatched starttime survived");

    /* clean up the real live child via a correct registration */
    kh_proc_registry_reset(HR);
    kh_proc_register(HR, live, live, "live-cleanup");
    kh_proc_reap_all(HR, 200, 0);
    for (int i = 0; i < 50 && alive(live); i++) msleep(20);
    waitpid((pid_t)live, NULL, WNOHANG);
    CHECK(!alive(live), "live child reaped once registered correctly");

    /* ---- case 4: never signals self / own group ------------------ */
    printf("\n[case 4] a line pointing at THIS process is ignored\n");
    kh_proc_registry_reset(HR);
    f = fopen(path, "w");
    fprintf(f, "%ld %ld %s self-should-be-skipped\n",
            (long)getpid(), (long)getpgrp(), "0");
    fclose(f);
    /* also register a real disposable child so reap has something valid */
    long c4 = spawn_child(0);
    msleep(80);
    kh_proc_register(HR, c4, c4, "dummy-d");
    int sig4 = kh_proc_reap_all(HR, 150, 1);
    for (int i = 0; i < 50 && alive(c4); i++) msleep(20);
    waitpid((pid_t)c4, NULL, WNOHANG);
    CHECK(sig4 == 1, "reap_all signalled only the real child, not self");
    printf("  (still running: pid %ld — the test itself)\n", (long)getpid());

    /* ---- case 5: reap_one ---------------------------------------- */
    printf("\n[case 5] reap_one removes exactly one entry\n");
    kh_proc_registry_reset(HR);
    long e1 = spawn_child(0), e2 = spawn_child(0);
    msleep(80);
    kh_proc_register(HR, e1, e1, "one-a");
    kh_proc_register(HR, e2, e2, "one-b");
    int r5 = kh_proc_reap_one(HR, e1, 150);
    for (int i = 0; i < 50 && alive(e1); i++) msleep(20);
    waitpid((pid_t)e1, NULL, WNOHANG);
    CHECK(r5 == 1, "reap_one reported a kill");
    CHECK(!alive(e1), "targeted child dead");
    CHECK(alive(e2), "other child untouched");
    CHECK(count_registry_lines() == 1, "registry has 1 line left");
    /* cleanup */
    kh_proc_registry_reset(HR);
    kh_proc_register(HR, e2, e2, "one-b");
    kh_proc_reap_all(HR, 150, 0);
    for (int i = 0; i < 50 && alive(e2); i++) msleep(20);
    waitpid((pid_t)e2, NULL, WNOHANG);

    /* ---- done --------------------------------------------------- */
    char rm[2300]; snprintf(rm, sizeof(rm), "rm -rf '%s'", dir);
    if (system(rm) != 0) fprintf(stderr, "warn: cleanup of %s failed\n", dir);

    printf("\n%s (%d failure%s)\n",
           failures ? "FAILED" : "ALL PASS", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
