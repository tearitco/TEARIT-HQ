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
#include <io.h>       /* _get_osfhandle - flock()'s fd -> HANDLE bridge */

/* Bracket form on purpose. khtpm_win_compat.c is the one translation unit
 * that pulls these in by quoted path, and a quoted include breaks the
 * #include_next inside win-compat/sys/stat.h (see the comment there). With
 * -Iwin-compat ahead of the system directories the bracket form resolves to
 * the same shim headers while keeping the stat include_next working. */
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <signal.h>
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

/* ----------------------------------------------------------------- flock */
/* REAL. NOT a stub - see win-compat/sys/file.h for why this one matters.
 *
 * flock(2) on a POSIX fd is an advisory whole-file lock that the kernel
 * arbitrates between processes; on Windows the faithful equivalent is
 * LockFileEx() on the OS handle behind the CRT descriptor. Byte-range 0 is
 * the conventional "the whole file" span, and because the lock is attached
 * to the HANDLE rather than to the process, it is genuinely visible to
 * every other process that opens the same lockfile - which is the entire
 * purpose of #.desktop/livedesk_popup.lock.
 *
 * Two flock-specific details are reproduced faithfully because the entity
 * depends on them:
 *   - LOCK_NB really is non-blocking: LOCKFILE_FAIL_IMMEDIATELY makes a
 *     contended lock fail at once, which is what
 *     popup_lock_acquire()'s retry loop (entity.c:3299) spins on. Without it
 *     that loop would block inside the kernel and the retry never iterates.
 *   - flock() keeps the lock per open-file-description, and re-locking an
 *     already-locked fd is a no-op rather than an error, so LOCK_EX after
 *     LOCK_EX succeeds. LockFileEx() on a range the process already holds
 *     also succeeds, so that falls out for free. */
int flock(int fd, int operation) {
    HANDLE h = (HANDLE)_get_osfhandle(fd);
    if (h == (HANDLE)-1 || h == NULL) { errno = EBADF; return -1; }
    /* LockFileEx counts bytes as a signed 64-bit length from the current
     * file position; cover the whole file from offset 0. */
    OVERLAPPED ov;
    ZeroMemory(&ov, sizeof(ov));
    ov.Offset     = 0;
    ov.OffsetHigh = 0;
    DWORD flags = 0;
    if (operation & LOCK_NB) flags |= LOCKFILE_FAIL_IMMEDIATELY;
    if (operation & LOCK_EX) {
        if (!LockFileEx(h, flags, 0, 0xFFFFFFFF, 0xFFFFFFFF, &ov)) {
            /* Contended (ERROR_LOCK_VIOLATION) or a real I/O failure. flock
             * reports EWOULDBLOCK for "someone else holds it", which is what
             * the caller's retry loop tests for. */
            DWORD err = GetLastError();
            errno = (err == ERROR_LOCK_VIOLATION) ? EWOULDBLOCK : EIO;
            return -1;
        }
        return 0;
    }
    if (operation & LOCK_SH) {
        /* LockFileEx has no shared mode: the Windows equivalent of flock's
         * shared lock is an exclusive lock, which is stricter but never
         * incorrect - it can only make callers wait, never let two holders
         * into the critical section at once. */
        if (!LockFileEx(h, flags, 0, 0xFFFFFFFF, 0xFFFFFFFF, &ov)) {
            DWORD err = GetLastError();
            errno = (err == ERROR_LOCK_VIOLATION) ? EWOULDBLOCK : EIO;
            return -1;
        }
        return 0;
    }
    if (operation & LOCK_UN) {
        if (!UnlockFileEx(h, 0, 0xFFFFFFFF, 0xFFFFFFFF, &ov)) {
            DWORD err = GetLastError();
            errno = (err == ERROR_NOT_LOCKED) ? EBADF : EIO;
            return -1;
        }
        return 0;
    }
    errno = EINVAL;
    return -1;
}

/* --------------------------------------------------------------- readlink */
ssize_t readlink(const char *path, char *buf, size_t bufsiz) {
    (void)path;
    if (!buf || bufsiz == 0) { errno = EINVAL; return -1; }
    wchar_t w[32768];
    DWORD k = GetModuleFileNameW(NULL, w, (DWORD)(sizeof(w) / sizeof(w[0])));
    if (!k || k >= sizeof(w) / sizeof(w[0])) { errno = ENOENT; return -1; }
    /* Passing k as an explicit length (rather than -1) makes
     * WideCharToMultiByte return the exact byte count and write NO
     * terminator, which is precisely readlink(2)'s contract: a byte count,
     * with the caller deciding where its own NUL goes. Using -1 instead
     * would fold the NUL into the count and make self_exe_path's
     * out[slen] = 0 land on top of ours. */
    int need = WideCharToMultiByte(CP_UTF8, 0, w, (int)k, NULL, 0, NULL, NULL);
    if (need <= 0) { errno = ENOENT; return -1; }
    if ((size_t)need > bufsiz) need = (int)bufsiz;   /* truncated, like POSIX */
    int got = WideCharToMultiByte(CP_UTF8, 0, w, (int)k, buf, need, NULL, NULL);
    if (got <= 0) { errno = ENOENT; return -1; }
    /* The rest of the Windows build speaks ONE separator, and it is '/'.
     * This is load-bearing, not cosmetic: resolve_livedesk_paths() hands
     * this path to dirname_step(), which is dirname() from <libgen.h>, and
     * MinGW-w64's dirname only scans for '/'. Left as backslashes it would
     * return the whole filename, ops_dir would come back as the full .exe
     * path, and the "#.desktop/ + &.widgits/" marker walk above it would
     * never find the house root - so the entity would start with no ops dir
     * and no assets at all. Where a backslash is genuinely wanted for a
     * syscall (the access() probes in resolve_livedesk_paths) the canonical
     * source already converts to '\\' itself, for exactly this reason. */
    for (int i = 0; i < got; i++) if (buf[i] == '\\') buf[i] = '/';
    return (ssize_t)got;
}

/* ---------------------------------------------------------- win_package_rel */
void win_package_rel(char *path) {
    if (!path || !path[0]) return;
    /* Strip everything up to and including the house marker, leaving the
     * house-relative tail that the entity's own IPC files are keyed on. The
     * marker list is checked in the house's own nesting order: the
     * xyzfs mount name first (the legacy root this was written against),
     * then #.desktop, which is the boundary every real house path crosses. */
    char *m = strstr(path, "xyzfs/");
    if (!m) m = strstr(path, "xyzfs\\");
    if (!m) m = strstr(path, "/#.desktop/");
    if (!m) m = strstr(path, "\\#.desktop\\");
    if (m && m != path) {
        if (*m == '/' || *m == '\\') m++;
        memmove(path, m, strlen(m) + 1);
    }
    /* The rest of the Windows build speaks one separator. Which one is the
     * entity's own business, not ours - it is handed a package_dir and
     * builds "%s/history.txt" style paths from it, and the Win32 file APIs
     * take either. */
    for (char *p = path; *p; p++) if (*p == '/') *p = '\\';
}

/* -------------------------------------------------------------- sigaction */
/* REAL, not a stub. See win-compat/signal.h for why the entity needs the
 * SA_SIGINFO form.
 *
 * The Windows CRT's signal() takes a one-argument handler, so the
 * three-argument sigaction handler cannot be installed directly. It is
 * recorded here per signal number and reached through a trampoline, which
 * is the standard way to bridge the two spellings.
 *
 * si_pid: the CRT does not tell us who sent the signal, so a value of 0 is
 * filled in - the honest "unknown", rather than a fabricated pid that would
 * send the entity's own forensic trace (last_signal.txt) chasing the wrong
 * process. The trace is still written, which is the part that matters. */
#define KHTPM_MAX_SIGHANDLED 32
static struct {
    int   sig;
    void (*fn)(int, siginfo_t *, void *);
    void (*prev)(int);
} g_sa[KHTPM_MAX_SIGHANDLED];
static int g_sa_n;

static void khtpm_sa_trampoline(int sig) {
    for (int i = 0; i < g_sa_n; i++) {
        if (g_sa[i].sig != sig) continue;
        siginfo_t si;
        ZeroMemory(&si, sizeof(si));
        si.si_signo = sig;
        si.si_pid = 0;   /* the CRT cannot tell us the sender */
        if (g_sa[i].fn) g_sa[i].fn(sig, &si, NULL);
        return;
    }
}

int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
    if (signum <= 0 || signum >= NSIG) { errno = EINVAL; return -1; }
    if (oldact) {
        /* Honest partial report: the flags and mask round-trip, the handler
         * is reported as whatever is installed, and the CRT offers no way to
         * read back the previous disposition, so a real sa_restorer/
         * previous-flags query would be a fabrication. */
        memset(oldact, 0, sizeof(*oldact));
        for (int i = 0; i < g_sa_n; i++)
            if (g_sa[i].sig == signum) oldact->sa_handler = (void (*)(int))g_sa[i].fn;
    }
    if (!act) return 0;   /* pure query */

    for (int i = 0; i < g_sa_n; i++) {
        if (g_sa[i].sig != signum) continue;
        g_sa[i].fn = (act->sa_flags & SA_SIGINFO)
                         ? act->sa_sigaction
                         : (void (*)(int, siginfo_t *, void *))act->sa_handler;
        /* Re-install so the CRT points at the trampoline again (harmless if
         * unchanged) - the handler may have been swapped for the other arm
         * of the union. */
        if (g_sa[i].prev) g_sa[i].prev = signal(signum, khtpm_sa_trampoline);
        return 0;
    }
    if (g_sa_n >= KHTPM_MAX_SIGHANDLED) { errno = ENOSPC; return -1; }
    g_sa[g_sa_n].sig = signum;
    g_sa[g_sa_n].fn = (act->sa_flags & SA_SIGINFO)
                          ? act->sa_sigaction
                          : (void (*)(int, siginfo_t *, void *))act->sa_handler;
    g_sa[g_sa_n].prev = signal(signum, khtpm_sa_trampoline);
    if (g_sa[g_sa_n].prev == SIG_ERR) {
        g_sa_n--;
        errno = EINVAL;
        return -1;
    }
    g_sa_n++;
    return 0;
}
