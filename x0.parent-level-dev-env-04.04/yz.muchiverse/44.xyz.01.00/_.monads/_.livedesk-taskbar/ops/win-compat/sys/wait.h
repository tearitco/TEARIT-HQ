/* win-compat/sys/wait.h — MinGW has no <sys/wait.h>.
 *
 * khtpm_core_render.c uses waitpid() in exactly two places, both of which
 * only ever run in db-hq mode:
 *
 *     586:  waitpid(g_module_pids[i], NULL, WNOHANG);   reap-on-tick
 *     9688: if (waitpid(g_khtpm_menu_pid, &wstatus, WNOHANG) == g_khtpm_menu_pid)
 *     2404: (void)waitpid(p, NULL, 0);                  blocking reap
 *
 * All three follow a fork(). On Windows fork() cannot exist, so
 * khtpm_strip_posix_win.c already stubs it to fail with -1/ENOSYS and no
 * child is ever created — which means there is never a real pid to reap and
 * reporting ECHILD is the truthful answer, not a silent success. A fake
 * "reaped successfully" would make line 9688 treat a live child as exited.
 */
#ifndef KHTPM_WINCOMPAT_SYS_WAIT_H
#define KHTPM_WINCOMPAT_SYS_WAIT_H

#ifndef WNOHANG
#define WNOHANG 1
#endif
#ifndef WUNTRACED
#define WUNTRACED 2
#endif
#ifndef WIFEXITED
#define WIFEXITED(status) 1
#endif
#ifndef WEXITSTATUS
#define WEXITSTATUS(status) ((status) & 0xff)
#endif
#ifndef WIFSIGNALED
#define WIFSIGNALED(status) (((status) & 0x7f) != 0)
#endif
#ifndef WTERMSIG
#define WTERMSIG(status) ((status) & 0x7f)
#endif

/* Implemented in khtpm_win_compat.c: always -1/ECHILD, since fork() cannot
 * create a child to wait for. */
int waitpid(int pid, int *status, int options);

#endif /* KHTPM_WINCOMPAT_SYS_WAIT_H */
