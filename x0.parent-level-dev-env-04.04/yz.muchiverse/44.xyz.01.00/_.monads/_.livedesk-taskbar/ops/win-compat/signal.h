/* win-compat/signal.h — Windows funnel for <signal.h>.
 *
 * MinGW-w64 ships a real signal.h (raise/signal/abort), but it has no
 * sigaction() and no struct sigaction, which is POSIX-only. Two canonical
 * sources in this runtime need the sigaction form:
 *
 *   - khtpm_entity.c:3853-3858 installs handle_shutdown_signal_info() for
 *     SIGTERM and SIGINT with SA_SIGINFO, at the very top of tp_main().
 *   - khtpm_core_render.c and khtpm_strip_parser.c include <signal.h> too,
 *     but only use the plain signal()/SIGTERM spelling, which MinGW already
 *     provides - so including this funnel in front of theirs changes nothing
 *     for them.
 *
 * Why the entity bothers with sigaction at all (its own header comment, and
 * worth repeating because it is the reason this cannot be a stub): plain
 * signal(SIGTERM,...) tells you THAT a signal arrived, never WHO sent it.
 * SA_SIGINFO exists so an entity that dies within ~1s of reaching its event
 * loop with no crash evidence anywhere leaves a forensic trail naming the
 * sending pid. On Windows the CRT's own handler chain gives us a real
 * signal-delivery path, so this is wired for real in khtpm_win_compat.c: the
 * three-argument handler is recorded per signal number and reached through a
 * trampoline installed with the CRT's signal(). si_pid is filled from the
 * sender where the OS tells us, and reported as 0 where it does not - the
 * honest value, and the trace is still written.
 *
 * Field order matches real POSIX struct sigaction so a caller that
 * initialises it positionally behaves the same on both platforms.
 */
#ifndef KHTPM_WINCOMPAT_SIGNAL_H
#define KHTPM_WINCOMPAT_SIGNAL_H

/* real signal.h first, so signal()/raise()/SIGTERM/SIGINT, NSIG and the
 * sig_atomic_t spelling all come from the CRT unchanged */
#include_next <signal.h>

/* sys/types.h for pid_t and _sigset_t, both used below. This funnel is
 * reached from khtpm_entity.c:28, which the force-included prelude has
 * already given sys/types.h, but khtpm_win_compat.c includes this header
 * directly and must not depend on that. */
#include <sys/types.h>

/* MinGW-w64 gates the POSIX spelling on _POSIX, which no MinGW-w64 build
 * defines (sys/types.h:109-111), so sigset_t is absent and only the
 * underscore-prefixed _sigset_t exists. Re-typedef is legal in C11 when the
 * type is identical, and it is the same type, so this holds whether or not
 * sys/types.h already opened the _SIGSET_T_ block. */
#ifndef _POSIX
typedef _sigset_t sigset_t;
#endif

/* Real POSIX value (SA_SIGINFO | SA_RESTART is 0x10000000; the flag the
 * entity actually sets is the SA_SIGINFO bit). */
#define SA_SIGINFO 4

/* siginfo_t. The entity reads exactly one field, si_pid
 * (khtpm_entity.c:3191); the rest are here so the type is a plausible
 * siginfo_t rather than a one-field stub, and so a future caller reading
 * si_signo does not silently read garbage. */
typedef struct {
    int  si_signo;
    int  si_errno;
    int  si_code;
    union {
        int    _pad[28];
        pid_t  si_pid;   /* sender pid for SI_USER/SI_TKILL  */
    } _u;
} siginfo_t;

/* Define the si_* accessors real POSIX provides, over the union above, so
 * the canonical code's info->si_pid spelling works unchanged. */
#define si_pid   _u.si_pid

/* The struct as POSIX declares it. sa_sigaction is a union member there too
 * (sa_handler / sa_sigaction share storage); only the SA_SIGINFO-selected arm
 * is populated here, which is all the entity uses. */
struct sigaction {
    union {
        void (*sa_handler)(int);
        void (*sa_sigaction)(int, siginfo_t *, void *);
    } __sa_handler;
    sigset_t sa_mask;
    int       sa_flags;
    void (*sa_restorer)(void);
};

/* Real POSIX spelling: the members are macros in the header, because
 * sa_sigaction is a union member above. */
#define sa_handler   __sa_handler.sa_handler
#define sa_sigaction __sa_handler.sa_sigaction

int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);

#endif /* KHTPM_WINCOMPAT_SIGNAL_H */
