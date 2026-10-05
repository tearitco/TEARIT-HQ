/* khtpm_strip_posix_win.h - Win32 stand-ins for the POSIX surface
 * khtpm_strip_parser.c reaches for, so the parser's own bar logic stays
 * byte-for-byte the same file the Linux build compiles. Pulled in on
 * _WIN32 only, via khtpm_strip_x11_win.h.
 *
 * The parser already hand-shims most of its POSIX needs inside its own
 * Win32 block (strtok_r, mkdir, localtime_r, usleep, getpid, kill,
 * waitpid, truncate, S_ISDIR). These are the ones it left out, because
 * its <dirent.h> and <fcntl.h> includes sit in the #ifndef _WIN32 arm.
 *
 * Two groups, and the split is load-bearing:
 *
 *   1. opendir/readdir/closedir - REAL, over FindFirstFileW. The @ zorder
 *      toggle's apply half (ktb_toggle_zorder_apply) walks
 *      <house>/#.desktop/nav_tab with exactly these three calls to restack
 *      live pieces, so this group has to actually work on Windows.
 *
 *   2. fork/setsid/execve - deliberately UNREACHABLE here, and safe by
 *      construction rather than by luck. Their only caller is
 *      ktb_toggle_zorder_respawn(), which opens "/proc" first and returns
 *      immediately when that fails. There is no /proc on Windows, so
 *      opendir() returns NULL and the entire fork/execve block is skipped
 *      before it is ever reached. These three exist purely so the
 *      translation unit links; fork() returns -1 so that even a future
 *      refactor that drops the /proc guard cannot accidentally spawn a
 *      half-torn-down child. The live Win32 relaunch story is the
 *      nav_tab registry read by group 1, not a /proc scan.
 */
#ifndef KHTPM_STRIP_POSIX_WIN_H
#define KHTPM_STRIP_POSIX_WIN_H

#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <fcntl.h>   /* real O_RDWR/O_CREAT/... - the parser's own fcntl.h
                     * include is in the #ifndef _WIN32 arm, but this one
                     * is not, so open()'s mode flags have to come from
                     * somewhere. */
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- group 1: directory iteration -------------------------------------
 * REAL, NEW 2026-09-26 - guarded on _DIRENT_H_, MinGW-w64's <dirent.h>
 * include guard. MinGW ships a working dirent.h (opendir/readdir/closedir,
 * backed by FindFirstFileA), so any translation unit that includes <dirent.h>
 * before this header - khtpm_core_render.c does, at line 68, ahead of its
 * X11 includes - would otherwise hit "redefinition of struct dirent" plus
 * three conflicting-types errors. Those TUs keep MinGW's declarations.
 *
 * khtpm_strip_posix_win.c itself never includes <dirent.h>, so the guard is
 * still open there and the definitions below compile as before, keeping the
 * old strip parser on this W-based implementation. That is also the one worth
 * linking: this repo's paths contain non-ASCII names, which FindFirstFileA
 * mangles. Both spellings are pointer-returning with identical signatures,
 * so the two can coexist across translation units without a link-time clash.
 */
#ifndef _DIRENT_H_
typedef struct {
    HANDLE           h;        /* FindFirstFileW handle, INVALID = closed */
    WIN32_FIND_DATAW data;     /* one result held back for the next readdir */
    int              pending;  /* `data` holds an unread result */
    int              first;    /* first readdir still to run the search */
} KHTPM_DIR;

typedef struct dirent {
    long d_ino;
    char d_name[MAX_PATH];
} dirent;

#define DIR KHTPM_DIR

DIR *opendir(const char *path);
struct dirent *readdir(DIR *d);
int closedir(DIR *d);
#endif /* !_DIRENT_H_ */

/* --- group 2: link-only process stubs, unreachable (see header) ------- */
int fork(void);
int setsid(void);
long khtpm_win_spawn_module(const char *path, char *const argv[]);
int execve(const char *path, char *const argv[], char *const envp[]);

#ifdef __cplusplus
}
#endif
#endif
