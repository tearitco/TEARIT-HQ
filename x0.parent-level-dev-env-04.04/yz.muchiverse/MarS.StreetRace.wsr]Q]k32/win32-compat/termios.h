/* termios.h - Windows shim for the Mars StreetRace CLI (MarS).
 *
 * WHY THIS EXISTS
 *   9 of this project's .c files do `#include <termios.h>` to put stdin
 *   into a no-echo line mode. mingw-w64 has no <termios.h> at all, so
 *   none of them compile on Windows as-is.
 *
 * WHY IT IS A HEADER AND NOT AN EDIT TO EACH FILE
 *   The Linux side must keep working untouched, so nothing is #ifdef'd
 *   into the 9 call sites. This shim is injected with `-I` on Windows
 *   only; the .c files are byte-identical on both platforms.
 *
 * THE TRICK
 *   The project only ever uses this one idiom:
 *
 *       struct termios t;
 *       tcgetattr(STDIN_FILENO, &t);
 *       t.c_lflag &= ~(ECHO | ICANON);
 *       tcsetattr(STDIN_FILENO, TCSANOW, &t);
 *
 *   and later `tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios)` to
 *   restore. There is no cfmakeraw, no speed handling, no tcflush.
 *
 *   So: store the REAL Windows console mode DWORD in c_lflag, and define
 *   ICANON/ECHO to be the two ENABLE_*_INPUT bits it is made of. The
 *   standard Linux line then does the right thing by arithmetic:
 *
 *       c_lflag &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT)
 *
 *   is exactly "raw mode" in Windows console terms. No interpretation
 *   layer, no translation table to keep in sync.
 */
#ifndef MARS_WIN32_TERMIOS_H
#define MARS_WIN32_TERMIOS_H

#include <windows.h>
#include <string.h>

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

/* Only TCSANOW is used by this project; the others exist so that a
 * future TCSAFLUSH compiles rather than failing confusingly. */
#define TCSANOW   0
#define TCSADRAIN 1
#define TCSAFLUSH 2

/* Aliased onto the real console mode bits - see the header comment. */
#define ICANON ENABLE_LINE_INPUT
#define ECHO   ENABLE_ECHO_INPUT

/* Indices into c_cc, matching the Linux ones closely enough for the
 * single VMIN/VTIME use in this project. */
#define VMIN   6
#define VTIME  5

typedef unsigned long  tcflag_t;
typedef unsigned char  cc_t;
typedef unsigned int   speed_t;

struct termios {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;   /* holds the raw Windows console mode DWORD */
    cc_t     c_cc[32];
    speed_t  c_ispeed;
    speed_t  c_ospeed;
};

static int tcgetattr(int fd, struct termios *t) {
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode;
    (void)fd;
    if (h == NULL || h == INVALID_HANDLE_VALUE) return -1;
    /* Fails when stdin is a pipe or a file rather than a console. That is
     * NOT fatal for this project: a piped-stdin run simply cannot be put
     * into raw mode, and the caller's `if (tcgetattr(...) == 0)` guard
     * handles it. Returning 0 with the current mode zeroed is wrong, so
     * report failure honestly instead. */
    if (!GetConsoleMode(h, &mode)) return -1;
    memset(t, 0, sizeof(*t));
    t->c_lflag = mode;
    t->c_cc[VMIN]  = 1;
    t->c_cc[VTIME] = 0;
    return 0;
}

static int tcsetattr(int fd, int action, const struct termios *t) {
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode, want;
    (void)fd; (void)action;
    if (h == NULL || h == INVALID_HANDLE_VALUE) return -1;
    if (!GetConsoleMode(h, &mode)) return -1;
    want = t->c_lflag;
    if (!SetConsoleMode(h, want)) return -1;
    return 0;
}

/* POSIX signal-set API, used once in game.c:
 *
 *     // Ensure SIGINT is not blocked
 *     sigset_t set; sigemptyset(&set); sigaddset(&set, SIGINT);
 *     pthread_sigmask(SIG_UNBLOCK, &set, NULL);
 *
 * Windows has no signal blocking at all - the console delivers Ctrl+C to
 * the handler installed by signal(SIGINT, ...) regardless. So this is a
 * deliberate no-op rather than a real emulation: there is no state to
 * block or unblock. The macros expand to valid C so the call sites need
 * no #ifdef.
 *
 * Placed here because game.c includes <termios.h> BEFORE <signal.h> and
 * well before the use, so the declarations are in scope by line 318. */
typedef int mars_sigset_t;
#define sigset_t mars_sigset_t
#define sigemptyset(s)      (*(s) = 0)
#define sigaddset(s, signo) (*(s) |= (1 << ((signo) - 1)))
#define sigismember(s, signo) (((*(s) >> ((signo) - 1)) & 1) != 0)
#define sigdelset(s, signo) (*(s) &= ~(1 << ((signo) - 1)))

/* Only if winpthreads has not already declared the real one. */
#ifndef SIG_BLOCK
#define SIG_BLOCK   0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2
static int pthread_sigmask_stub(int how, const sigset_t *set, sigset_t *old) {
    (void)how; (void)set; (void)old;
    return 0;   /* nothing to do on Windows */
}
#ifndef pthread_sigmask
#define pthread_sigmask(how, set, old) pthread_sigmask_stub((how), (set), (old))
#endif
#endif

#endif /* MARS_WIN32_TERMIOS_H */
