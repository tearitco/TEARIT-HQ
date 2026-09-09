/* kh_spawn.h — the one blessed way to start a long-lived house child.
 *
 * fork + (optional) setsid + (optional) chdir(house_root) + execv, and
 * in the parent kh_proc_register_owned(child, child, master_pid, name)
 * so the process-lifecycle reaper
 * (PROC-LIFECYCLE-ORCHESTRATOR-TEARDOWN.md) reaches it. Same TPMOS
 * `launch_and_register()` shape.
 *
 * House convention: spawn a long-lived child via kh_spawn() / the shell
 * `kh_spawn.sh`, never a raw fork/`setsid nohup … &`. A binary exec'd
 * outside this funnel calls kh_proc_self_register() in main() instead.
 *
 * Idiom (same as kh_proc_registry.h / kh_plat.h): the ONE .c that owns
 * the impl does
 *     #define KH_SPAWN_IMPL
 *     #define KH_PROC_REGISTRY_IMPL
 *     #include ".../_shared-lib/kh_proc_registry.h"
 *     #include ".../_shared-lib/kh_spawn.h"
 * every other TU just #includes for the declarations.
 */
#ifndef KH_SPAWN_H
#define KH_SPAWN_H

#ifdef __cplusplus
extern "C" {
#endif

#define KH_SPAWN_SETSID       (1u << 0)   /* new session/group (detached) */
#define KH_SPAWN_CHDIR_HOUSE  (1u << 1)   /* chdir(house_root) in the child */
#define KH_SPAWN_QUIET        (1u << 2)   /* child stdout/stderr -> /dev/null */

/* Spawn argv[0..] (argv NULL-terminated). `master_pid` <= 0 -> getpid().
 * `name` labels the ledger line. Returns the child pid, or -1. */
long kh_spawn(const char *house_root, long master_pid, const char *name,
              char *const argv[], unsigned flags);

#ifdef __cplusplus
}
#endif

#ifdef KH_SPAWN_IMPL
#include <stdio.h>
#include <stdlib.h>
#ifndef _WIN32
#  include <unistd.h>
#  include <fcntl.h>
#  include <sys/types.h>

long kh_spawn(const char *house_root, long master_pid, const char *name,
              char *const argv[], unsigned flags) {
    if (!argv || !argv[0]) return -1;
    if (master_pid <= 0) master_pid = (long)getpid();
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        if (flags & KH_SPAWN_SETSID) setsid();
        if ((flags & KH_SPAWN_CHDIR_HOUSE) && house_root && house_root[0]) {
            if (chdir(house_root) != 0) { /* non-fatal: keep going */ }
        }
        if (flags & KH_SPAWN_QUIET) {
            int dn = open("/dev/null", O_WRONLY);
            if (dn >= 0) { dup2(dn, 1); dup2(dn, 2); if (dn > 2) close(dn); }
        }
        execv(argv[0], argv);
        _exit(127);
    }
    /* parent */
    kh_proc_register_owned(house_root, (long)pid, (long)pid, master_pid,
                           (name && *name) ? name : "kh_spawn");
    return (long)pid;
}

#else  /* _WIN32 */
#  include <process.h>
long kh_spawn(const char *house_root, long master_pid, const char *name,
              char *const argv[], unsigned flags) {
    (void)flags;
    if (!argv || !argv[0]) return -1;
    intptr_t h = _spawnv(_P_NOWAIT, argv[0], (const char *const *)argv);
    if (h <= 0) return -1;
    long pid = (long)GetProcessId((HANDLE)h);
    kh_proc_register_owned(house_root, pid, pid,
                           master_pid > 0 ? master_pid : (long)GetCurrentProcessId(),
                           (name && *name) ? name : "kh_spawn");
    return pid;
}
#endif /* _WIN32 */
#endif /* KH_SPAWN_IMPL */
#endif /* KH_SPAWN_H */
