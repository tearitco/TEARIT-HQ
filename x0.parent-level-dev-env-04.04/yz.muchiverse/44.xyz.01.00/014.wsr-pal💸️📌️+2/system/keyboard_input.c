/* keyboard_input - standalone raw-terminal input capture, no ncurses.
 * Mirrors the real TPMOS pattern (pieces/system/input_dispatcher/plugins/
 * input_capture.c): puts STDIN into raw termios mode directly, decodes
 * "ESC [ A/B/C/D" arrow sequences itself, and appends bare decimal
 * keycodes to pieces/apps/player_app/history.txt - one int per line,
 * matching what prisc+x's read_history opcode already expects (it
 * fseeks to a byte cursor and fscanf("%d", ...), so this file must
 * stay bare-decimal, not the timestamped "[ts] KEY_PRESSED: N" format
 * some other TPMOS subsystems use).
 *
 * Windows: conio _getch/_kbhit path from 1.TPMOS keyboard_input.c —
 * surgical #ifdef _WIN32, Linux termios path unchanged.
 *
 * Self-contained: own root resolution, own constants, no shared headers.
 * Exits (and restores the terminal) on Ctrl+C (ETX, keycode 3) - the quit
 * signal is written to history.txt first, so prisc+x's pal script sees
 * it and halts itself independently. This process additionally drops
 * a byte in pieces/system/quit_flag.txt on exit so the renderer knows
 * to stop polling.
 *
 * NOT switched onto shared-ops/keyboard_input.c (see this family's own
 * xyzos-standards.txt §1c on when NOT to force-share): that file
 * normalizes Ctrl+C/ETX to 'q' before appending, and quits on 'q';
 * this file appends the raw ETX byte (3) and quits directly on 3 - a
 * real, existing behavioral difference (wsr-pal's own pal script
 * presumably keys off keycode 3, not 113), not just a style choice,
 * so this file stays its own local copy.
 *
 * CHTPM-BRIDGE ADDITION (see chtpm-to-pal-layout-plan.txt and
 * shared-ops/chtpm_parser_pal.c's own header comment for the full
 * why): append_key() now ALSO appends "KEY_PRESSED: N\n" to
 * pieces/keyboard/history.txt - real 1.TPMOS's own chtpm_parser.c
 * reads that exact file in that exact format, completely unmodified.
 * Mirrors the identical addition already made to shared-ops/
 * keyboard_input.c for mutaclsym/zoo_0000/muchipal-editor-0.0. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#include <direct.h>
#include <sys/stat.h>
#define usleep(x) Sleep((DWORD)((x) / 1000))
#define getcwd _getcwd
#else
#include <unistd.h>
#include <termios.h>
#include <sys/stat.h>
#endif

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif
/* Room for MAX_PATH worth of project_root plus the longest relative
 * suffix this file appends, so gcc can prove snprintf can't truncate. */
#define PATH_BUF (MAX_PATH + 256)

#define ARROW_LEFT  1000
#define ARROW_RIGHT 1001
#define ARROW_UP    1002
#define ARROW_DOWN  1003

static char project_root[MAX_PATH] = ".";
#ifndef _WIN32
static struct termios orig_term;
#endif

static void resolve_root(void) {
#ifdef _WIN32
    /* Prefer "." when CWD has pieces/ — absolute emoji getcwd + fopen fail.
     * button.ps1 sets WorkingDirectory to 8.3 short path of project. */
    {
        struct stat st;
        if (stat("pieces", &st) == 0) {
            snprintf(project_root, sizeof(project_root), ".");
            return;
        }
    }
#endif
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) { snprintf(project_root, sizeof(project_root), "%s", env); return; }
    if (!getcwd(project_root, sizeof(project_root))) snprintf(project_root, sizeof(project_root), ".");
}

static void disable_raw_mode(void) {
#ifndef _WIN32
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_term);
#endif
}

static void write_quit_flag(void);

static void handle_signal(int sig) {
    /* Defensive fallback for a real external signal (e.g. `kill -TERM`
     * from outside, or `button.sh kill`). Ctrl+C itself does NOT reach
     * here in the normal case - see the ETX check in main()'s loop and
     * the comment on enable_raw_mode() below for why. */
    (void)sig;
    disable_raw_mode();
    write_quit_flag();
    _exit(0);
}

static void enable_raw_mode(void) {
#ifndef _WIN32
    tcgetattr(STDIN_FILENO, &orig_term);
    atexit(disable_raw_mode);
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGHUP, handle_signal);

    struct termios raw = orig_term;
    raw.c_iflag &= ~(unsigned long)(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(unsigned long)(OPOST);
    raw.c_cflag |= (CS8);
    /* ISIG cleared means the tty line discipline stops turning Ctrl+C
     * into SIGINT - it arrives as ordinary input, byte 0x03 (ETX), like
     * any other keystroke. handle_signal() above is only a fallback for
     * a real external signal; Ctrl+C itself must be caught as data in
     * main()'s read loop, same as real TPMOS's input_capture.c does. */
    raw.c_lflag &= ~(unsigned long)(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
#else
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
#endif
}

static int read_key(void) {
#ifdef _WIN32
    /* TPMOS Windows keyboard path: _kbhit/_getch. Native console only;
     * mintty/MSYS2 pseudo-ttys may not deliver arrows (known TPMOS lim). */
    while (!_kbhit()) {
        usleep(20000);
    }
    int c = _getch();
    if (c == 0 || c == 224) {
        switch (_getch()) {
            case 72: return ARROW_UP;
            case 80: return ARROW_DOWN;
            case 77: return ARROW_RIGHT;
            case 75: return ARROW_LEFT;
        }
        return -1;
    }
    /* Map Windows Enter (CR 13) to LF 10 for menu Enter parity */
    if (c == 13) return 10;
    return c;
#else
    char c;
    int nread;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        /* XYZOS-PITFALLS #22 (2026-07-26): nread==0 (EOF/no data) has no
         * natural throttle here on a non-tty stdin - a real terminal's
         * VMIN=0/VTIME=1 (set in enable_raw_mode()) makes read() block
         * ~100ms before returning 0, but tcsetattr() silently fails on a
         * non-tty fd (headless/API-sandbox test run, stdin from
         * /dev/null or a closed pipe), so read() returns 0 INSTANTLY
         * forever and this loop pegs one CPU core at ~100% with no
         * upper bound - observed live, ~93-100% CPU per session,
         * multiple concurrent test sessions compounding it. Cheap on a
         * real terminal (this path is already rare there, throttled by
         * VTIME) - real fix for the headless case.
         *
         * FOLLOW-UP FIX (2026-07-29, file-menu widget testing): the
         * hard-error branch (nread==-1 && errno != EAGAIN) used to
         * `return -1` BEFORE reaching the usleep below, so a stdin fd
         * that fails outright (e.g. a backgrounded process with no
         * controlling terminal at all - confirmed live, read() coming
         * back with a real errno every call, never EAGAIN) hit this
         * exact same unthrottled spin the EOF case above was already
         * fixed for. main()'s `if (key == -1) continue;` then re-enters
         * read_key() immediately with zero sleep anywhere in the cycle -
         * this is what actually pegged the CPU (confirmed: ~99% CPU
         * process, and unlike the EOF case, on a hard-error fd it never
         * lets up). Throttling BEFORE returning -1 keeps the existing
         * "signal a hard read failure upward" contract (still returns
         * -1, main() still treats it as a no-op tick) while capping the
         * retry rate the same way the EOF path already does. */
        usleep(20000);
        if (nread == -1 && errno != EAGAIN) return -1;
    }
    if (c == '\x1b') {
        char seq[2];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
        if (seq[0] == '[') {
            switch (seq[1]) {
                case 'A': return ARROW_UP;
                case 'B': return ARROW_DOWN;
                case 'C': return ARROW_RIGHT;
                case 'D': return ARROW_LEFT;
            }
        }
        return '\x1b';
    }
    return (unsigned char)c;
#endif
}

static void append_key(int key) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/history.txt", project_root);
    FILE *f = fopen(path, "a");
    if (f) { fprintf(f, "%d\n", key); fclose(f); }

    /* CHTPM-BRIDGE ADDITION - see this file's own top-of-file comment.
     * A second, differently-formatted line for chtpm_parser_pal.c's
     * own unmodified history-reading code, which expects real 1.TPMOS's
     * native "KEY_PRESSED: N" shape. Never read by prisc+x's own
     * OP_READ_HISTORY (a different file), so this can't collide with
     * the write above. */
    char chtpm_path[PATH_BUF];
    snprintf(chtpm_path, sizeof(chtpm_path), "%s/pieces/keyboard/history.txt", project_root);
    FILE *cf = fopen(chtpm_path, "a");
    if (cf) { fprintf(cf, "KEY_PRESSED: %d\n", key); fclose(cf); }
}

static void write_quit_flag(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/quit_flag.txt", project_root);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fputs("1", f);
    fclose(f);
}

int main(void) {
    resolve_root();

    /* Clear history.txt on startup - avoids replaying old keys from previous
     * session (race condition: shell-script clearing is not reliable). Truncate
     * the file to ensure fresh input stream for this session. */
    char history_path[PATH_BUF];
    snprintf(history_path, sizeof(history_path), "%s/pieces/apps/player_app/history.txt", project_root);
    FILE *hf = fopen(history_path, "w");
    if (hf) fclose(hf);

    enable_raw_mode();

    for (;;) {
        int key = read_key();
        if (key == -1) continue;
        append_key(key);
        if (key == 3) break; /* Ctrl+C (ETX) - see enable_raw_mode() */
    }

    disable_raw_mode();
    write_quit_flag();
    return 0;
}
