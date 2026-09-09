/* kh_proc_registry.h — orchestrator-owned, PID-tracked process teardown.
 *
 * The house standard for "an orchestrator kills every child it started,
 * on its way out" — ported from TPMOS's
 *   1.TPMOS_c_+rmmp.0103.0001/pieces/chtpm/plugins/orchestrator.c
 *   (log_pid / kill_all_tracked_processes)
 *
 * Design doc:
 *   #.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/
 *   PROC-LIFECYCLE-ORCHESTRATOR-TEARDOWN.md
 *
 * Idiom (same as kh_plat.h): ONE .c per program does
 *     #define KH_PROC_REGISTRY_IMPL
 *     #include ".../_shared-lib/kh_proc_registry.h"
 * every other TU just #includes it for the declarations.
 *
 * The registry file is  <house_root>/#.desktop/livedesk_proc_list.txt ,
 * one line per tracked process:
 *     <pid> <pgid> <starttime> <name>
 * `starttime` is field 22 of /proc/<pid>/stat (clock ticks since boot)
 * and is the PID-REUSE GUARD: the reaper re-reads it before signalling
 * and skips the line if the live value differs (a reused PID is a
 * different process). This is the load-bearing safety property — see
 * HOUSE_CODE_PITFALLS.md #15 (`kill(getppid())` logged the desktop out).
 *
 * v0 (2026-09-09): POSIX real. Windows: register still logs; reap uses
 * OpenProcess/TerminateProcess per-pid (no group concept), matching
 * TPMOS's win_kill. Linux behaviour is authoritative.
 */
#ifndef KH_PROC_REGISTRY_H
#define KH_PROC_REGISTRY_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Fill `buf` with <house_root>/#.desktop/livedesk_proc_list.txt */
void kh_proc_registry_path(const char *house_root, char *buf, size_t n);

/* Truncate the registry. Call ONCE at orchestrator (taskbar) startup,
 * before any launch. Returns 0 on success, -1 on real failure. */
int kh_proc_registry_reset(const char *house_root);

/* Append one tracked entry; call immediately after a successful spawn.
 * `pgid` <= 0 means "use pid" (the setsid group-leader case).
 * `name` may be NULL. flock(LOCK_EX)+fsync, like TPMOS's log_pid.
 * Returns 0 on success, -1 on failure.
 * kh_proc_register() == kh_proc_register_owned() with master_pid 0
 * ("owned by the orchestrator; reaped only by reap_all"). */
int kh_proc_register(const char *house_root, long pid, long pgid,
                     const char *name);

/* The master-ledger register. `master_pid` = the process that owns this
 * child for teardown (getpid() of whoever spawned it: the taskbar
 * manager for a tb-launch; an app's own root pid for a child it forked
 * itself). Writes the 5-field line
 *   <pid> <pgid> <master_pid> <starttime> <name>
 * Returns 0 / -1. */
int kh_proc_register_owned(const char *house_root, long pid, long pgid,
                           long master_pid, const char *name);

/* A house binary exec'd outside a kh_spawn funnel registers ITSELF from
 * main() and drops its line via kh_proc_self_unregister() from atexit.
 * Line: getpid() getpgrp() getppid() starttime <name>. */
int kh_proc_self_register(const char *house_root, const char *name);
int kh_proc_self_unregister(const char *house_root);

/* The teardown, ported from TPMOS. For each registry entry whose pid+
 * starttime still match a live process:
 *     kill(-pgid, SIGTERM); kill(pid, SIGTERM);
 *   ... grace_ms ...
 *     kill(-pgid, SIGKILL); kill(pid, SIGKILL); waitpid(pid, WNOHANG);
 * then the registry file is truncated. NEVER signals pid 0/1, the
 * caller's own pid, or the caller's own process group. `grace_ms` <= 0
 * defaults to 200. `verbose` != 0 prints one line per action to stderr.
 * Returns the number of processes signalled. */
int kh_proc_reap_all(const char *house_root, int grace_ms, int verbose);

/* Reap a single tracked pid (targeted "close this window"). Same
 * TERM->grace->KILL escalation + starttime guard. Rewrites the registry
 * without that entry. Returns 1 if it signalled, 0 if not found/stale,
 * -1 on error. */
int kh_proc_reap_one(const char *house_root, long pid, int grace_ms);

/* Scoped teardown — "close one app", NOT "quit the desktop". Reaps
 * every ledger entry owned by `master_pid` (row.master == master_pid)
 * plus the row FOR `master_pid` itself, one level deep (a deeper chain
 * relies on each intermediate having registered its own children under
 * its own pid). Same TERM->grace->KILL + starttime guard; never signals
 * pid 0/1, the caller, or the caller's group. Rewrites the ledger
 * keeping the untouched rows. Returns processes signalled. */
int kh_proc_reap_subtree(const char *house_root, long master_pid,
                         int grace_ms, int verbose);

/* Rewrite the registry keeping only entries whose pid+starttime still
 * match a live process. Call on a menu open / idle tick to stop the
 * file growing across a long session. Returns entries kept, -1 on error. */
int kh_proc_registry_prune(const char *house_root);

#ifdef __cplusplus
}
#endif

/* ======================================================================
 * Implementation
 * ==================================================================== */
#ifdef KH_PROC_REGISTRY_IMPL

/* POSIX surface (flock/fileno/kill/nanosleep/getpgrp). Safe here only
 * because the house idiom includes this header before any libc header
 * in the one IMPL translation unit. */
#if !defined(_WIN32) && !defined(_GNU_SOURCE) && !defined(_POSIX_C_SOURCE)
#  define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#  include <unistd.h>
#  include <signal.h>
#  include <sys/file.h>
#  include <sys/wait.h>
#  include <sys/types.h>
#  include <errno.h>
#  include <time.h>
#else
#  include <windows.h>
#endif

#ifndef KHPR_LINE_MAX
#  define KHPR_LINE_MAX 512
#endif
#ifndef KHPR_PATH_MAX
#  define KHPR_PATH_MAX 2048
#endif

void kh_proc_registry_path(const char *house_root, char *buf, size_t n) {
    snprintf(buf, n, "%s/#.desktop/livedesk_proc_list.txt",
             house_root ? house_root : ".");
}

/* ---- POSIX ---------------------------------------------------------- */
#ifndef _WIN32

/* field 22 of /proc/<pid>/stat, in clock ticks since boot. 0 = not found
 * / unreadable. comm (field 2) is parenthesised and may itself contain
 * spaces and ')', so scan from the LAST ')' . */
static unsigned long long khpr_starttime(long pid) {
    char path[64], buf[1024];
    snprintf(path, sizeof(path), "/proc/%ld/stat", pid);
    FILE *f = fopen(path, "r");
    if (!f) return 0ULL;
    size_t got = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    if (got == 0) return 0ULL;
    buf[got] = '\0';
    char *rp = strrchr(buf, ')');
    if (!rp) return 0ULL;
    /* after ") " : field 3 (state) onward, space-separated. want field 22,
     * i.e. skip 19 tokens after state, then read the 20th... count:
     * fields 3..22 => 20 tokens starting at rp+2. */
    char *p = rp + 1;
    int field = 2;               /* we're positioned just past field 2 */
    char *tok = strtok(p, " ");
    while (tok) {
        field++;
        if (field == 22) return strtoull(tok, NULL, 10);
        tok = strtok(NULL, " ");
    }
    return 0ULL;
}

static int khpr_alive_matches(long pid, unsigned long long want_start) {
    if (pid <= 1) return 0;
    unsigned long long s = khpr_starttime(pid);
    if (s == 0ULL) return 0;                 /* gone */
    if (want_start != 0ULL && s != want_start) return 0;  /* PID reused */
    return 1;
}

int kh_proc_registry_reset(const char *house_root) {
    char path[KHPR_PATH_MAX];
    kh_proc_registry_path(house_root, path, sizeof(path));
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fclose(f);
    return 0;
}

int kh_proc_register_owned(const char *house_root, long pid, long pgid,
                           long master_pid, const char *name) {
    if (pid <= 1) return -1;
    if (pgid <= 0) pgid = pid;
    if (master_pid < 0) master_pid = 0;
    char path[KHPR_PATH_MAX];
    kh_proc_registry_path(house_root, path, sizeof(path));
    FILE *f = fopen(path, "a");
    if (!f) return -1;
    flock(fileno(f), LOCK_EX);
    fprintf(f, "%ld %ld %ld %llu %s\n", pid, pgid, master_pid,
            khpr_starttime(pid),
            (name && *name) ? name : "-");
    fflush(f);
    fsync(fileno(f));
    flock(fileno(f), LOCK_UN);
    fclose(f);
    return 0;
}

int kh_proc_register(const char *house_root, long pid, long pgid,
                     const char *name) {
    return kh_proc_register_owned(house_root, pid, pgid, 0, name);
}

int kh_proc_self_register(const char *house_root, const char *name) {
    return kh_proc_register_owned(house_root, (long)getpid(),
                                  (long)getpgrp(), (long)getppid(), name);
}

/* one registry row */
typedef struct { long pid, pgid, master; unsigned long long start; char name[128]; } KhprRow;

static int khpr_load(const char *path, KhprRow *rows, int max) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[KHPR_LINE_MAX];
    int n = 0;
    while (n < max && fgets(line, sizeof(line), f)) {
        KhprRow r; r.name[0] = '\0'; r.master = 0;
        unsigned long long st = 0ULL;
        /* 5-field master-ledger form first; fall back to the v1 4-field
         * form (no master_pid -> 0). A 4-field line's 3rd token is a
         * huge starttime, so the 5-field %llu on the 4th (name) token
         * fails and `got` comes back < 5. */
        int got = sscanf(line, "%ld %ld %ld %llu %127s",
                         &r.pid, &r.pgid, &r.master, &st, r.name);
        if (got < 5) {
            r.master = 0; st = 0ULL; r.name[0] = '\0';
            got = sscanf(line, "%ld %ld %llu %127s",
                         &r.pid, &r.pgid, &st, r.name);
            if (got < 2) continue;           /* unparseable -> no-op */
            if (got < 3) st = 0ULL;
        }
        if (r.pgid <= 0) r.pgid = r.pid;
        r.start = st;
        rows[n++] = r;
    }
    fclose(f);
    return n;
}

/* rewrite `path` with `rows[0..n)` minus every index i where drop(i). */
static void khpr_rewrite(const char *path, const KhprRow *rows, int n,
                         int (*drop)(int, void *), void *ctx) {
    FILE *f = fopen(path, "w");
    if (!f) return;
    for (int i = 0; i < n; i++) {
        if (drop && drop(i, ctx)) continue;
        fprintf(f, "%ld %ld %ld %llu %s\n", rows[i].pid, rows[i].pgid,
                rows[i].master, rows[i].start, rows[i].name);
    }
    fclose(f);
}
static int khpr_drop_index(int i, void *c) { return i == *(int *)c; }

int kh_proc_reap_all(const char *house_root, int grace_ms, int verbose) {
    if (grace_ms <= 0) grace_ms = 200;
    char path[KHPR_PATH_MAX];
    kh_proc_registry_path(house_root, path, sizeof(path));

    KhprRow rows[1024];
    int n = khpr_load(path, rows, 1024);

    long me = (long)getpid();
    long my_pgrp = (long)getpgrp();
    int signalled = 0;

    /* Phase 1: SIGTERM to group + pid */
    for (int i = 0; i < n; i++) {
        long pid = rows[i].pid, pgid = rows[i].pgid;
        if (pid <= 1 || pid == me || pgid == my_pgrp) continue;
        if (!khpr_alive_matches(pid, rows[i].start)) {
            if (verbose) fprintf(stderr, "kh_proc_reap: skip stale %ld (%s)\n",
                                 pid, rows[i].name);
            continue;
        }
        if (verbose) fprintf(stderr, "kh_proc_reap: TERM -%ld / %ld (%s)\n",
                             pgid, pid, rows[i].name);
        kill((pid_t)-pgid, SIGTERM);
        kill((pid_t)pid, SIGTERM);
        signalled++;
    }

    /* grace */
    struct timespec ts = { grace_ms / 1000, (long)(grace_ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);

    /* Phase 2: SIGKILL survivors + reap */
    for (int i = 0; i < n; i++) {
        long pid = rows[i].pid, pgid = rows[i].pgid;
        if (pid <= 1 || pid == me || pgid == my_pgrp) continue;
        if (!khpr_alive_matches(pid, rows[i].start)) continue;
        if (verbose) fprintf(stderr, "kh_proc_reap: KILL -%ld / %ld (%s)\n",
                             pgid, pid, rows[i].name);
        kill((pid_t)-pgid, SIGKILL);
        kill((pid_t)pid, SIGKILL);
        waitpid((pid_t)pid, NULL, WNOHANG);
    }

    /* clear the file for the next run */
    FILE *f = fopen(path, "w");
    if (f) fclose(f);
    return signalled;
}

int kh_proc_reap_one(const char *house_root, long pid, int grace_ms) {
    if (grace_ms <= 0) grace_ms = 200;
    if (pid <= 1) return -1;
    char path[KHPR_PATH_MAX];
    kh_proc_registry_path(house_root, path, sizeof(path));

    KhprRow rows[1024];
    int n = khpr_load(path, rows, 1024);

    int idx = -1;
    for (int i = 0; i < n; i++) if (rows[i].pid == pid) { idx = i; break; }
    if (idx < 0) return 0;

    long me = (long)getpid(), my_pgrp = (long)getpgrp();
    long pgid = rows[idx].pgid;
    int did = 0;
    if (pid != me && pgid != my_pgrp &&
        khpr_alive_matches(pid, rows[idx].start)) {
        kill((pid_t)-pgid, SIGTERM);
        kill((pid_t)pid, SIGTERM);
        struct timespec ts = { grace_ms / 1000, (long)(grace_ms % 1000) * 1000000L };
        nanosleep(&ts, NULL);
        if (khpr_alive_matches(pid, rows[idx].start)) {
            kill((pid_t)-pgid, SIGKILL);
            kill((pid_t)pid, SIGKILL);
            waitpid((pid_t)pid, NULL, WNOHANG);
        }
        did = 1;
    }

    /* rewrite without this row */
    { int _one = idx; khpr_rewrite(path, rows, n, khpr_drop_index, &_one); }
    return did;
}

/* ctx for the subtree/self rewrites */
typedef struct { const KhprRow *rows; long key; long self_start; } KhprDropCtx;

static int khpr_drop_dead(int i, void *c) {
    const KhprRow *r = &((KhprDropCtx *)c)->rows[i];
    return !khpr_alive_matches(r->pid, r->start);
}
static int khpr_drop_subtree(int i, void *c) {
    KhprDropCtx *x = (KhprDropCtx *)c;
    const KhprRow *r = &x->rows[i];
    return (r->master == x->key || r->pid == x->key);
}
static int khpr_drop_self(int i, void *c) {
    KhprDropCtx *x = (KhprDropCtx *)c;
    const KhprRow *r = &x->rows[i];
    return (r->pid == x->key &&
            (x->self_start == 0 || r->start == 0 || r->start == (unsigned long long)x->self_start));
}

int kh_proc_registry_prune(const char *house_root) {
    char path[KHPR_PATH_MAX];
    kh_proc_registry_path(house_root, path, sizeof(path));
    KhprRow rows[1024];
    int n = khpr_load(path, rows, 1024);
    KhprDropCtx ctx = { rows, 0, 0 };
    int kept = 0;
    for (int i = 0; i < n; i++)
        if (khpr_alive_matches(rows[i].pid, rows[i].start)) kept++;
    khpr_rewrite(path, rows, n, khpr_drop_dead, &ctx);
    return kept;
}

int kh_proc_reap_subtree(const char *house_root, long master_pid,
                         int grace_ms, int verbose) {
    if (grace_ms <= 0) grace_ms = 200;
    if (master_pid <= 1) return 0;
    char path[KHPR_PATH_MAX];
    kh_proc_registry_path(house_root, path, sizeof(path));
    KhprRow rows[1024];
    int n = khpr_load(path, rows, 1024);
    long me = (long)getpid(), my_pgrp = (long)getpgrp();
    int signalled = 0;

    for (int phase = 0; phase < 2; phase++) {
        for (int i = 0; i < n; i++) {
            long pid = rows[i].pid, pgid = rows[i].pgid;
            if (!(rows[i].master == master_pid || rows[i].pid == master_pid)) continue;
            if (pid <= 1 || pid == me || pgid == my_pgrp) continue;
            if (!khpr_alive_matches(pid, rows[i].start)) continue;
            int sig = phase == 0 ? SIGTERM : SIGKILL;
            if (verbose) fprintf(stderr, "kh_proc_reap_subtree(%ld): %s -%ld / %ld (%s)\n",
                                 master_pid, phase ? "KILL" : "TERM", pgid, pid, rows[i].name);
            kill((pid_t)-pgid, sig);
            kill((pid_t)pid, sig);
            if (phase == 0) signalled++;
            else waitpid((pid_t)pid, NULL, WNOHANG);
        }
        if (phase == 0) {
            struct timespec ts = { grace_ms / 1000, (long)(grace_ms % 1000) * 1000000L };
            nanosleep(&ts, NULL);
        }
    }
    { KhprDropCtx ctx = { rows, master_pid, 0 };
      khpr_rewrite(path, rows, n, khpr_drop_subtree, &ctx); }
    return signalled;
}

int kh_proc_self_unregister(const char *house_root) {
    char path[KHPR_PATH_MAX];
    kh_proc_registry_path(house_root, path, sizeof(path));
    KhprRow rows[1024];
    int n = khpr_load(path, rows, 1024);
    KhprDropCtx ctx = { rows, (long)getpid(), (long)khpr_starttime((long)getpid()) };
    khpr_rewrite(path, rows, n, khpr_drop_self, &ctx);
    return 0;
}

/* ---- Windows (minimal, per-pid, no group) ------------------------- */
#else  /* _WIN32 */

int kh_proc_registry_reset(const char *house_root) {
    char path[KHPR_PATH_MAX];
    kh_proc_registry_path(house_root, path, sizeof(path));
    FILE *f = fopen(path, "w"); if (!f) return -1; fclose(f); return 0;
}

int kh_proc_register_owned(const char *house_root, long pid, long pgid,
                           long master_pid, const char *name) {
    (void)pgid;
    if (pid <= 1) return -1;
    if (master_pid < 0) master_pid = 0;
    char path[KHPR_PATH_MAX];
    kh_proc_registry_path(house_root, path, sizeof(path));
    FILE *f = fopen(path, "a"); if (!f) return -1;
    fprintf(f, "%ld %ld %ld 0 %s\n", pid, pid, master_pid,
            (name && *name) ? name : "-");
    fclose(f);
    return 0;
}
int kh_proc_register(const char *house_root, long pid, long pgid,
                     const char *name) {
    return kh_proc_register_owned(house_root, pid, pgid, 0, name);
}
int kh_proc_self_register(const char *house_root, const char *name) {
    return kh_proc_register_owned(house_root, (long)GetCurrentProcessId(),
                                 (long)GetCurrentProcessId(), 0, name);
}
int kh_proc_self_unregister(const char *house_root) { (void)house_root; return 0; }

static void khpr_win_kill(long pid) {
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)pid);
    if (h) { TerminateProcess(h, 1); CloseHandle(h); }
}

int kh_proc_reap_all(const char *house_root, int grace_ms, int verbose) {
    (void)grace_ms; (void)verbose;
    char path[KHPR_PATH_MAX];
    kh_proc_registry_path(house_root, path, sizeof(path));
    FILE *f = fopen(path, "r"); if (!f) return 0;
    char line[KHPR_LINE_MAX]; long pid, pgid; int n = 0;
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "%ld %ld", &pid, &pgid) >= 1 && pid > 1) {
            khpr_win_kill(pid); n++;
        }
    }
    fclose(f);
    f = fopen(path, "w"); if (f) fclose(f);
    return n;
}

int kh_proc_reap_one(const char *house_root, long pid, int grace_ms) {
    (void)house_root; (void)grace_ms;
    if (pid <= 1) return -1;
    khpr_win_kill(pid);
    return 1;
}

int kh_proc_reap_subtree(const char *house_root, long master_pid,
                         int grace_ms, int verbose) {
    (void)grace_ms; (void)verbose;
    if (master_pid <= 1) return 0;
    char path[KHPR_PATH_MAX];
    kh_proc_registry_path(house_root, path, sizeof(path));
    FILE *f = fopen(path, "r"); if (!f) return 0;
    char line[KHPR_LINE_MAX]; long pid, pgid, mst; int n = 0;
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "%ld %ld %ld", &pid, &pgid, &mst) == 3 &&
            pid > 1 && (mst == master_pid || pid == master_pid)) {
            khpr_win_kill(pid); n++;
        }
    }
    fclose(f);
    return n;   /* ledger rewrite omitted on the minimal Windows path */
}

int kh_proc_registry_prune(const char *house_root) { (void)house_root; return 0; }

#endif /* _WIN32 */

#endif /* KH_PROC_REGISTRY_IMPL */
#endif /* KH_PROC_REGISTRY_H */
