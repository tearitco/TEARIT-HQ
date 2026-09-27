/* khtpm_win_compat.c — Windows implementations for the shims declared in
 * ops/win-compat/. Deliberately small: it exists so the canonical
 * khtpm_core_render.c can be compiled unmodified on Windows (see
 * win-compat/X11/Xlib.h for the reasoning), NOT to grow a second X11 layer.
 * The X11/Xft surface itself lives in khtpm_strip_x11_win.c.
 *
 * Everything here is a case where MinGW-w64 genuinely lacks the POSIX
 * spelling the renderer uses. Behaviour is chosen to be truthful rather than
 * convenient: where a faithful implementation is impossible (no fork, so no
 * children to wait for) it reports the honest failure instead of a fake
 * success, because the callers branch on those return values.
 */
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

/* Bracket form on purpose. khtpm_win_compat.c is the one translation unit
 * that pulls these in by quoted path, and a quoted include breaks the
 * #include_next inside win-compat/sys/stat.h (see the comment there). With
 * -Iwin-compat ahead of the system directories the bracket form resolves to
 * the same shim headers while keeping the stat include_next working. */
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "win-compat/khtpm_win_compat_prelude.h"

/* The prelude routes fclose() through khtpm_fclose() so open_memstream can
 * publish its buffer on close. Inside this file we need the REAL fclose, so
 * drop the macro again after taking the declarations. */
#undef fclose

/* ---------------------------------------------------------------- select */
int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
           struct timeval *timeout) {
    (void)nfds; (void)readfds; (void)writefds; (void)exceptfds;

    /* The only descriptor ever passed is ConnectionNumber(dpy), which on this
     * platform is a synthetic handle: there is no socket and no X connection
     * that could become readable. Real events are drained separately via
     * XPending()/XNextEvent() in the same loop, so reporting "0 fds ready"
     * after honouring the timeout is exactly the between-frames behaviour the
     * caller paces against. A NULL timeout would block forever, which is a
     * caller bug, so return immediately rather than hang the renderer. */
    if (timeout) {
        long ms = (long)(timeout->tv_sec * 1000L + (timeout->tv_usec + 999L) / 1000L);
        if (ms > 0) Sleep((DWORD)ms);
    }
    return 0;
}

/* --------------------------------------------------------------- waitpid */
int waitpid(int pid, int *status, int options) {
    (void)pid; (void)options;
    if (status) *status = 0;
    /* fork() is stubbed out (khtpm_strip_posix_win.c), so there is never a
     * real child to reap. ECHILD is truthful and matters: core_render.c:9688
     * compares the result against the pid it is tracking, so reporting
     * success would make it treat a running child as exited. */
    errno = ECHILD;
    return -1;
}

/* ------------------------------------------------------------ stat/fstat */
/* struct khtpm_stat's first 11 fields are byte-identical to MinGW's
 * struct _stat64, so the real call can write straight into it and only the
 * appended POSIX st_mtim needs filling in. */
static void khtpm_stat_fill(struct khtpm_stat *out, const struct _stat64 *real) {
    memcpy(out, real, sizeof(struct _stat64));
    /* NTFS/HFS stat exposes whole seconds only, so tv_nsec is honestly 0
     * rather than a fabricated sub-second value. The dock-peer gate in
     * core_render.c:1856-1859 still detects changes, just at 1s
     * resolution. */
    out->st_mtim.tv_sec  = (time_t)real->st_mtime;
    out->st_mtim.tv_nsec = 0;
}

int khtpm_stat(const char *path, struct khtpm_stat *out) {
    struct _stat64 real;
    if (!path || !out) { errno = EINVAL; return -1; }
    if (_stat64(path, &real) != 0) return -1;
    khtpm_stat_fill(out, &real);
    return 0;
}

int khtpm_fstat(int fd, struct khtpm_stat *out) {
    struct _stat64 real;
    if (!out) { errno = EINVAL; return -1; }
    if (_fstat64(fd, &real) != 0) return -1;
    khtpm_stat_fill(out, &real);
    return 0;
}

/* ------------------------------------------------------------ setenv etc */
int setenv(const char *name, const char *value, int overwrite) {
    if (!name || !*name) { errno = EINVAL; return -1; }
    if (!overwrite) {
        /* POSIX: leave an existing variable alone. There is no read-only
         * getenv-backed store to consult cheaply, so ask the CRT block. */
        if (getenv(name)) return 0;
    }
    /* _putenv_s copies the string, which is what setenv's contract needs. */
    return _putenv_s(name, value ? value : "");
}

int unsetenv(const char *name) {
    if (!name || !*name) { errno = EINVAL; return -1; }
    /* On the Windows CRT, assigning an empty value removes the variable. */
    return _putenv_s(name, "");
}

/* ------------------------------------------------------------------ kill */
int kill(pid_t pid, int sig) {
    if (pid <= 0) { errno = EINVAL; return -1; }
    /* sig==0 is the POSIX existence probe, which core_render.c:11431 uses for
     * its own liveness check. It must NOT terminate anything. */
    DWORD access = PROCESS_QUERY_LIMITED_INFORMATION;
    if (sig != 0) access |= PROCESS_TERMINATE;
    HANDLE h = OpenProcess(access, FALSE, (DWORD)pid);
    if (!h) { errno = ESRCH; return -1; }
    if (sig == 0) { CloseHandle(h); return 0; }
    BOOL ok = TerminateProcess(h, (UINT)sig);
    DWORD err = GetLastError();
    CloseHandle(h);
    if (!ok) { errno = (err == ERROR_ACCESS_DENIED) ? EACCES : ESRCH; return -1; }
    return 0;
}

/* --------------------------------------------------------- open_memstream */
/* Backed by a real temp file rather than a hand-rolled FILE, because the CRT
 * gives no way to attach custom vtable behaviour to a FILE*. The buffer is
 * published when the stream is closed, which is what the callers in
 * core_render.c:8089/8164 expect (they fclose() then read the pointer). */
typedef struct {
    FILE    *fp;
    char   **ptr;
    size_t  *sizeloc;
} KhtpmMemStream;

#define KHTPM_MEMSTREAM_MAX 16
static KhtpmMemStream g_ms[KHTPM_MEMSTREAM_MAX];
static int            g_ms_n;

FILE *open_memstream(char **ptr, size_t *sizeloc) {
    if (!ptr || !sizeloc) { errno = EINVAL; return NULL; }
    if (g_ms_n >= KHTPM_MEMSTREAM_MAX) { errno = EMFILE; return NULL; }
    FILE *fp = tmpfile();
    if (!fp) return NULL;
    g_ms[g_ms_n].fp     = fp;
    g_ms[g_ms_n].ptr    = ptr;
    g_ms[g_ms_n].sizeloc = sizeloc;
    g_ms_n++;
    *ptr = NULL;
    *sizeloc = 0;
    return fp;
}

int khtpm_fclose(FILE *fp) {
    for (int i = 0; i < g_ms_n; i++) {
        if (g_ms[i].fp != fp) continue;

        fflush(fp);
        if (fseek(fp, 0L, SEEK_SET) != 0) {
            *g_ms[i].ptr = NULL; *g_ms[i].sizeloc = 0;
        } else {
            long n = ftell(fp);
            if (n < 0) n = 0;
            char *buf = (char *)malloc((size_t)n + 1);
            if (!buf) {
                *g_ms[i].ptr = NULL; *g_ms[i].sizeloc = 0;
            } else {
                size_t got = fread(buf, 1, (size_t)n, fp);
                buf[got] = '\0';
                *g_ms[i].ptr = buf;
                *g_ms[i].sizeloc = got;
            }
        }
        /* Close the slot first so a reentrant close can't double-publish. */
        g_ms[i] = g_ms[g_ms_n - 1];
        g_ms_n--;
        return fclose(fp);
    }
    return fclose(fp);
}
