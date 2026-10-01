/* win-compat/sys/stat.h — MinGW compat for <sys/stat.h>.
 *
 * Two POSIX/Glibc things khtpm_core_render.c relies on are missing or
 * different under MinGW-w64:
 *
 * 1) mkdir(path, mode) — MinGW's is 1-arg (_mkdir). core_render:1691 calls
 *    the 2-arg POSIX form. Redirected to _mkdir; the mode is meaningless on
 *    Win32 anyway (no permission bits), so dropping it loses nothing.
 *
 * 2) st_mtim — MinGW-w64 defines NO st_mtim anywhere (verified: the only
 *    hits for "st_mtim" in the whole MinGW include tree are unrelated
 *    lmserver.h fields). Its struct stat has a 1-second __time64_t st_mtime.
 *    core_render's dock-peer freshness check reads nanosecond stamps:
 *
 *        if (stat(g_dock_peer_path, &pst) == 0 &&
 *            (pst.st_mtim.tv_sec != g_dock_peer_mtime.tv_sec ||
 *             pst.st_mtim.tv_nsec != g_dock_peer_mtime.tv_nsec))
 *        ...
 *        g_dock_peer_mtime = pst.st_mtim;
 *
 *    and assigns st_mtim straight into a `struct timespec`, so the member
 *    must be exactly a struct timespec. That assignment is a genuine
 *    nanosecond-resolution freshness gate on the peer template — losing it
 *    silently would make the renderer stop noticing peer-template edits, so
 *    it is preserved rather than stubbed.
 *
 * Approach: struct stat is a thin wrapper whose first 11 fields are laid out
 * identically to MinGW's struct _stat64, with POSIX st_mtim appended. Because
 * the prefix matches byte-for-byte, the real _stat64()/_fstat64() can write
 * straight into it, and khtpm_win_compat.c then fills st_mtim from st_mtime.
 * `struct stat` / stat() / fstat() are macro-rewritten to that wrapper, which
 * is why this lives in a header rather than as a plain helper: the call sites
 * are canonical source we are not editing.
 *
 * Consequence, stated plainly: on NTFS the peer-mtime gate degrades to
 * 1-second resolution, because that is all the platform's stat gives us. The
 * gate still functions (it is a change detector, not a rate limiter); it is
 * simply coarser than on Linux.
 */
#ifndef KHTPM_WINCOMPAT_SYS_STAT_H
#define KHTPM_WINCOMPAT_SYS_STAT_H

#include <sys/types.h>
#include <time.h>

/* Real MinGW declarations first, so _stat64()/_fstat64() and the underlying
 * struct _stat64 are available. Include guard of the real header is separate
 * from ours, so this is safe to re-enter.
 *
 * The two includes below are belt-and-braces, and the reason is a genuine
 * GCC trap rather than paranoia. #include_next continues the *bracket* search
 * path after the directory the current file was found in - but that only
 * works if the current file was reached through a bracket include. Reached via
 * a quoted include (#include "win-compat/sys/stat.h") GCC anchors the search
 * at the current file's own directory, <...>/win-compat/sys/, and
 * #include_next <sys/stat.h> then looks for <...>/win-compat/sys/sys/stat.h,
 * finds nothing, and fails silently. The result is a translation unit where
 * struct _stat64 is incomplete and _stat64() is undeclared - which is exactly
 * what khtpm_win_compat.c hit. So:
 *
 *   - <_mingw_stat64.h> is a real, unambiguous MinGW header that defines
 *     struct _stat64 and carries its own include guard, so it works no matter
 *     how this file was reached;
 *   - #include_next then additionally picks up the _stat64()/_fstat64()
 *     prototypes from the real <sys/stat.h> on the normal bracket path.
 */
#include <_mingw_stat64.h>
#include_next <sys/stat.h>

/* _mkdir() lives in <direct.h> on MinGW-w64, not <sys/stat.h>, so the
 * redirect below would otherwise expand to an implicit declaration. */
#include <direct.h>

/* mkdir(path, mode) -> _mkdir(path) */
#undef mkdir
#define mkdir(path, mode) _mkdir(path)

#ifndef _dev_t
#define _dev_t unsigned int
#endif

struct khtpm_stat {
    /* Byte-identical prefix to MinGW's struct _stat64 -- do not reorder. */
    _dev_t            st_dev;
    _ino_t            st_ino;
    unsigned short    st_mode;
    short             st_nlink;
    short             st_uid;
    short             st_gid;
    _dev_t            st_rdev;
    __int64           st_size;
    __time64_t        st_atime;
    __time64_t        st_mtime;
    __time64_t        st_ctime;
    /* POSIX spelling MinGW lacks. Must stay exactly `struct timespec`:
     * core_render assigns it straight into `struct timespec`. */
    struct timespec   st_mtim;
};

#define stat  khtpm_stat
#define fstat khtpm_fstat

/* Implemented in khtpm_win_compat.c. Both wrap the real _stat64/_fstat64
 * (safe: identical prefix) and then synthesise st_mtim from st_mtime. */
int khtpm_stat(const char *path, struct khtpm_stat *out);
int khtpm_fstat(int fd, struct khtpm_stat *out);

#endif /* KHTPM_WINCOMPAT_SYS_STAT_H */
