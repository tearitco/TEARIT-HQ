/* win-compat/sys/file.h — MinGW-w64 has no <sys/file.h>, and khtpm_entity.c
 * hard-includes it with ZERO _WIN32 branches (line 41), exactly like its
 * Linux build.
 *
 * The one thing it needs from it is flock(), and it needs it for something
 * load-bearing rather than incidental: the house-wide popup mutex. Every
 * entity process is separate, and XGrabKeyboard/XGrabPointer are
 * display-wide exclusive resources, so two entities whose popups open at the
 * same instant would fight over the grab. khtpm_entity.c serialises them
 * with flock(LOCK_EX) on one shared lockfile
 * (#.desktop/livedesk_popup.lock) - see its own comment above
 * g_popup_lock_fd, and the same for the nav registry above
 * g_registry_lock_fd.
 *
 * This is therefore NOT a link-only stub in the fork()/setsid() sense that
 * sys/wait.h is. The mutex is the whole point, and a fake flock would let
 * two processes grab the keyboard simultaneously - the exact bug the flock
 * was added to fix. The real implementation is in khtpm_win_compat.c, over
 * LockFileEx()/UnlockFileEx() on the CRT descriptor's real OS handle, which
 * is the genuine cross-process advisory lock that flock(2) is.
 *
 * LOCK_* values are the real Linux <sys/file.h> numbers.
 */
#ifndef KHTPM_WINCOMPAT_SYS_FILE_H
#define KHTPM_WINCOMPAT_SYS_FILE_H

/* Real flock(2) operation codes. */
#define LOCK_SH 1        /* shared lock                              */
#define LOCK_EX 2        /* exclusive lock                          */
#define LOCK_NB 4        /* fail immediately instead of blocking    */
#define LOCK_UN 8        /* unlock                                   */

int flock(int fd, int operation);

#endif /* KHTPM_WINCOMPAT_SYS_FILE_H */
