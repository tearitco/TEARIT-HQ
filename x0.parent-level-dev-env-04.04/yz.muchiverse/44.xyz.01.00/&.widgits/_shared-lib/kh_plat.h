/* kh_plat.h — shared platform surface for manager processes.
 *
 * Purpose: a manager (*_manager.c behind a <module>) should never type
 * `#ifdef _WIN32`. Include this and call the portable wrappers instead
 * of raw `usleep` / `clock_gettime` / `mkdir` / `signal` /
 * `system("sh ...")`.
 *
 * Idiom (matches khtpm_render_core.c's text-include style): ONE .c per
 * program does
 *     #define KH_PLAT_IMPL
 *     #include ".../_shared-lib/kh_plat.h"
 * every other TU just #includes it for the declarations.
 *
 * Design doc + porter guidance:
 *   #.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/
 *   CROSS-PLATFORM-SEAM-AND-SHARED-INFRA.md  (Part 1)
 *
 * v1 (2026-09-06): sleep_ms, mono_ms, mkdir_p, run_house_script,
 * on_terminate. POSIX real; Windows real where trivial, and one
 * documented stub (run_house_script) pending the house's .sh-vs-.ps1
 * decision. Linux behaviour is authoritative and unaffected.
 */
#ifndef KH_PLAT_H
#define KH_PLAT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Sleep the calling thread for `ms` milliseconds. */
void kh_plat_sleep_ms(int ms);

/* Monotonic clock in milliseconds (arbitrary origin; for deltas). */
long long kh_plat_mono_ms(void);

/* `mkdir -p` for `path`. Returns 0 on success or if it already exists,
 * -1 on real failure. */
int kh_plat_mkdir_p(const char *path);

/* Run a house shell script: `sh '<house_root>/<rel_script>' <args...>`,
 * output discarded. `args` may be NULL when `nargs` is 0. Returns the
 * child's exit status (0 = ok), or -1 if it could not be launched.
 *
 * WINDOWS: see the design doc's "PORTER NOTE" — currently a logged
 * no-op returning -1 until the house picks bundled-sh.exe vs .ps1
 * mirrors. A Windows build still links and runs; only this side-effect
 * is inert. */
int kh_plat_run_house_script(const char *house_root, const char *rel_script,
                             const char *const *args, int nargs);

/* Install `fn` as the handler for "please terminate" signals
 * (SIGTERM/SIGINT/SIGHUP on POSIX; console CTRL events on Windows).
 * `fn` is called with the signal number (POSIX) or SIGINT (Windows). */
void kh_plat_on_terminate(void (*fn)(int));

#ifdef __cplusplus
}
#endif

/* ------------------------------------------------------------------ */
#ifdef KH_PLAT_IMPL
#ifndef KH_PLAT_IMPL_ONCE
#define KH_PLAT_IMPL_ONCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
/* ===================== Windows backend ========================= */
#include <windows.h>
#include <direct.h>
#include <signal.h>

void kh_plat_sleep_ms(int ms) { if (ms > 0) Sleep((DWORD)ms); }

long long kh_plat_mono_ms(void) { return (long long)GetTickCount64(); }

int kh_plat_mkdir_p(const char *path) {
    char buf[1024];
    size_t n = strlen(path);
    if (n == 0 || n >= sizeof(buf)) return -1;
    memcpy(buf, path, n + 1);
    for (char *p = buf + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char c = *p; *p = 0;
            _mkdir(buf);            /* TODO kh_plat v2: _wmkdir + UTF-16 for emoji house paths */
            *p = c;
        }
    }
    _mkdir(buf);
    return 0;                       /* best-effort, mirrors POSIX "exists is ok" */
}

int kh_plat_run_house_script(const char *house_root, const char *rel_script,
                             const char *const *args, int nargs) {
    (void)house_root; (void)rel_script; (void)args; (void)nargs;
    /* PORTER: decide bundled sh.exe vs .ps1 mirrors — see design doc
     * Part 1 "PORTER NOTE". Until then this is a visible no-op. */
    fprintf(stderr, "[kh_plat] run_house_script not wired on Windows yet: %s\n",
            rel_script ? rel_script : "(null)");
    return -1;
}

static void (*g_kh_term_fn)(int) = NULL;
static BOOL WINAPI kh_plat__ctrl(DWORD t) {
    if ((t == CTRL_C_EVENT || t == CTRL_CLOSE_EVENT || t == CTRL_BREAK_EVENT ||
         t == CTRL_SHUTDOWN_EVENT) && g_kh_term_fn) { g_kh_term_fn(SIGINT); return TRUE; }
    return FALSE;
}
void kh_plat_on_terminate(void (*fn)(int)) {
    g_kh_term_fn = fn;
    SetConsoleCtrlHandler(kh_plat__ctrl, TRUE);
}

#else
/* ====================== POSIX backend ========================== */
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>

void kh_plat_sleep_ms(int ms) {
    if (ms <= 0) return;
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR) { /* resume remaining */ }
}

long long kh_plat_mono_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

int kh_plat_mkdir_p(const char *path) {
    char buf[1024];
    size_t n = strlen(path);
    if (n == 0 || n >= sizeof(buf)) return -1;
    memcpy(buf, path, n + 1);
    for (char *p = buf + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(buf, 0777) == -1 && errno != EEXIST) { *p = '/'; return -1; }
            *p = '/';
        }
    }
    if (mkdir(buf, 0777) == -1 && errno != EEXIST) return -1;
    return 0;
}

/* append `s` to buf single-quote-escaped for /bin/sh */
static void kh_plat__shq(char *buf, size_t cap, const char *s) {
    size_t len = strlen(buf);
    #define KH_PUT(ch) do { if (len + 1 < cap) buf[len++] = (ch); } while (0)
    KH_PUT('\'');
    for (; *s; s++) {
        if (*s == '\'') { KH_PUT('\''); KH_PUT('\\'); KH_PUT('\''); KH_PUT('\''); }
        else KH_PUT(*s);
    }
    KH_PUT('\'');
    if (len < cap) buf[len] = 0; else buf[cap - 1] = 0;
    #undef KH_PUT
}

int kh_plat_run_house_script(const char *house_root, const char *rel_script,
                             const char *const *args, int nargs) {
    char cmd[4096];
    char scriptpath[2048];
    snprintf(scriptpath, sizeof(scriptpath), "%s/%s", house_root, rel_script);
    strcpy(cmd, "sh ");
    kh_plat__shq(cmd, sizeof(cmd), scriptpath);
    for (int i = 0; i < nargs; i++) {
        strncat(cmd, " ", sizeof(cmd) - strlen(cmd) - 1);
        kh_plat__shq(cmd, sizeof(cmd), args && args[i] ? args[i] : "");
    }
    strncat(cmd, " >/dev/null 2>&1", sizeof(cmd) - strlen(cmd) - 1);
    int rc = system(cmd);
    if (rc == -1) return -1;
    return rc;
}

static void (*g_kh_term_fn)(int) = NULL;
static void kh_plat__sig(int s) { if (g_kh_term_fn) g_kh_term_fn(s); }
void kh_plat_on_terminate(void (*fn)(int)) {
    g_kh_term_fn = fn;
    signal(SIGTERM, kh_plat__sig);
    signal(SIGINT,  kh_plat__sig);
    signal(SIGHUP,  kh_plat__sig);
}

#endif /* _WIN32 */
#endif /* KH_PLAT_IMPL_ONCE */
#endif /* KH_PLAT_IMPL */
#endif /* KH_PLAT_H */
