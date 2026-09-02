/* Thin Win32 shims for aomorai ops (flock / kill / usleep / open).
 * Include AFTER standard headers. Linux: no-op include of sys/file.h path.
 * Per WIN-COMPAT-RULE: platform only, not design logic. */
#ifndef AOMORAI_WIN_POSIX_SHIM_H
#define AOMORAI_WIN_POSIX_SHIM_H

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <process.h>
#include <direct.h>
#include <string.h>

#ifndef F_OK
#define F_OK 0
#endif
#ifndef R_OK
#define R_OK 4
#endif
#ifndef W_OK
#define W_OK 2
#endif

#ifndef O_RDWR
#define O_RDWR _O_RDWR
#endif
#ifndef O_CREAT
#define O_CREAT _O_CREAT
#endif
#ifndef O_RDONLY
#define O_RDONLY _O_RDONLY
#endif

#define access _access
#define open _open
#define close _close
#define read _read
#define write _write
#define getcwd _getcwd
#define getpid _getpid
#define ftruncate _chsize
#define usleep(x) Sleep((DWORD)((x) / 1000))
#ifndef X_OK
#define X_OK 0 /* existence only on Win */
#endif
#ifndef F_OK
#define F_OK 0
#endif
#define strtok_r strtok_s
/* windows.h defines MAX_PATH=260; house C files want 4096 buffers */
#ifdef MAX_PATH
#undef MAX_PATH
#endif

#ifndef LOCK_SH
#define LOCK_SH 1
#define LOCK_EX 2
#define LOCK_NB 4
#define LOCK_UN 8
#endif

static inline int flock(int fd, int operation) {
    HANDLE h = (HANDLE)_get_osfhandle(fd);
    if (h == INVALID_HANDLE_VALUE) return -1;
    OVERLAPPED ov;
    memset(&ov, 0, sizeof(ov));
    if (operation & LOCK_UN) {
        return UnlockFileEx(h, 0, 0xFFFFFFFFu, 0xFFFFFFFFu, &ov) ? 0 : -1;
    }
    DWORD flags = LOCKFILE_EXCLUSIVE_LOCK;
    if (operation & LOCK_NB) flags |= LOCKFILE_FAIL_IMMEDIATELY;
    if (!(operation & LOCK_EX) && (operation & LOCK_SH)) flags = 0;
    return LockFileEx(h, flags, 0, 0xFFFFFFFFu, 0xFFFFFFFFu, &ov) ? 0 : -1;
}

/* kill(pid,0) = liveness; kill(pid,!=0) = TerminateProcess */
static inline int kill(int pid, int sig) {
    if (pid <= 0) return -1;
    DWORD access = PROCESS_QUERY_INFORMATION;
    if (sig != 0) access |= PROCESS_TERMINATE;
    HANDLE h = OpenProcess(access, FALSE, (DWORD)pid);
    if (!h) return -1;
    if (sig == 0) {
        CloseHandle(h);
        return 0;
    }
    BOOL ok = TerminateProcess(h, 1);
    CloseHandle(h);
    return ok ? 0 : -1;
}

/* recursive mkdir (mkdir -p) */
static inline void win_mkdir_p(const char *path) {
    char tmp[4096];
    size_t n = strlen(path);
    if (n >= sizeof(tmp)) return;
    memcpy(tmp, path, n + 1);
    for (char *p = tmp; *p; p++) if (*p == '/') *p = '\\';
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '\\') {
            char c = *p;
            *p = '\0';
            _mkdir(tmp);
            *p = c;
        }
    }
    _mkdir(tmp);
}

#else /* POSIX */
#include <unistd.h>
#include <sys/file.h>
#include <signal.h>
#endif /* _WIN32 */

#endif /* AOMORAI_WIN_POSIX_SHIM_H */
