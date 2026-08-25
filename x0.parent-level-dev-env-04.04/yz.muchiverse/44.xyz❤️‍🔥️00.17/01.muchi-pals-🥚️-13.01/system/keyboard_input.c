/* keyboard_input - standalone raw-terminal input capture, no ncurses.
 * Mirrors the real TPMOS pattern (pieces/system/input_dispatcher/plugins/
 * input_capture.c): puts STDIN into raw mode directly, decodes arrow keys
 * itself, and appends bare decimal keycodes to
 * pieces/apps/player_app/history.txt - one int per line, matching what
 * prisc+x's read_history opcode already expects (it fseeks to a byte
 * cursor and fscanf("%d", ...), so this file must stay bare-decimal, not
 * the timestamped "[ts] KEY_PRESSED: N" format some other TPMOS
 * subsystems use).
 *
 * NOT switched onto shared-ops/keyboard_input.c (see xyzos-standards.txt
 * §1c on when NOT to force-share): this file has genuine, real Win32
 * Console API support the shared version doesn't have - patched here
 * locally instead, same precedent already used for wsr-pal's own
 * keyboard_input.c.
 *
 * CHTPM-BRIDGE ADDITION (see chtpm-to-pal-layout-plan.txt and
 * shared-ops/chtpm_parser_pal.c's own header comment for the full
 * why): append_key() now ALSO appends "KEY_PRESSED: N\n" to
 * pieces/keyboard/history.txt - real 1.TPMOS's own chtpm_parser.c
 * reads that exact file in that exact format, completely unmodified.
 *
 * Self-contained: own root resolution, own constants, no shared headers.
 * QUIT KEY REMOVED (mass-refactor 2026-07-26, direct user instruction:
 * "never use q to quit in any of the refactors"): the literal letter
 * 'q'/'Q' used to double as a global quit key, which meant it could
 * never be typed as ordinary content (a username, a cli_io field, plain
 * nav) without accidentally exiting the whole game. Ctrl+C (ETX, byte
 * 3) is now the ONLY quit trigger - it's a real control character, not
 * a printable letter, so it can never collide with typed text. It is
 * handled directly here (not forwarded into history.txt as input at
 * all) and drops a byte in pieces/system/quit_flag.txt on exit so the
 * renderer knows to stop polling; the rest of the process tree (prisc+x
 * module, chtpm_parser_pal, orchestrator) is torn down externally by
 * button.sh's own trap/kill cascade once this process exits, same as
 * before.
 *
 * Two backends behind the same read_key()/history.txt contract:
 * POSIX raw termios (Linux/Mac), or the Win32 Console API (Windows) -
 * chosen at compile time via _WIN32, same decimal keycodes either way. */
#ifdef _WIN32
#include <windows.h>
#else
#define _GNU_SOURCE
#include <termios.h>
#include <errno.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#define PROJ_MAX_PATH 4096
/* Room for PROJ_MAX_PATH worth of project_root plus the longest relative
 * suffix this file appends, so gcc can prove snprintf can't truncate. */
#define PATH_BUF (PROJ_MAX_PATH + 256)

#define ARROW_LEFT  1000
#define ARROW_RIGHT 1001
#define ARROW_UP    1002
#define ARROW_DOWN  1003

static char project_root[PROJ_MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) { snprintf(project_root, sizeof(project_root), "%s", env); return; }
    if (!getcwd(project_root, sizeof(project_root))) snprintf(project_root, sizeof(project_root), ".");
}

static void write_quit_flag(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/quit_flag.txt", project_root);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fputs("1", f);
    fclose(f);
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

#ifdef _WIN32

static HANDLE g_stdin_handle;
static DWORD g_orig_console_mode;
static int g_have_orig_mode = 0;

static void disable_raw_mode(void) {
    if (g_have_orig_mode) SetConsoleMode(g_stdin_handle, g_orig_console_mode);
}

static BOOL WINAPI handle_ctrl_event(DWORD ctrl_type) {
    /* Defensive fallback for the console itself going away (window
     * closed, logoff, shutdown) - Ctrl+C is NOT handled here since
     * ENABLE_PROCESSED_INPUT is off below, so it arrives as ordinary
     * input (byte 0x03) through ReadConsoleInput instead, same as the
     * POSIX build's ISIG-cleared termios handles it in main()'s loop. */
    if (ctrl_type == CTRL_CLOSE_EVENT || ctrl_type == CTRL_LOGOFF_EVENT || ctrl_type == CTRL_SHUTDOWN_EVENT) {
        disable_raw_mode();
        write_quit_flag();
        return FALSE; /* let the default handler proceed after our cleanup */
    }
    return FALSE;
}

static void enable_raw_mode(void) {
    g_stdin_handle = GetStdHandle(STD_INPUT_HANDLE);
    if (GetConsoleMode(g_stdin_handle, &g_orig_console_mode)) g_have_orig_mode = 1;
    else {
        /* stdin isn't a real Win32 console handle - happens under mintty
         * (MSYS2/Git-Bash's default terminal) and some other pseudo-tty
         * hosts, since ReadConsoleInput/SetConsoleMode only work against
         * an actual console object, not a pipe. Fail loudly instead of
         * falling through to a ReadConsoleInputA loop that would busy-spin
         * at ~100% CPU forever (every call fails instantly rather than
         * blocking, since there's no real console to block on). */
        fprintf(stderr, "keyboard_input: stdin is not a Windows console (mintty/MSYS2 terminal?). "
                         "Run this from cmd.exe, PowerShell, or Windows Terminal instead.\n");
        exit(1);
    }
    SetConsoleCtrlHandler(handle_ctrl_event, TRUE);

    /* No line/echo/processed input: keys arrive one at a time, unecho'd,
     * and Ctrl+C arrives as data (0x03) instead of tearing the process
     * down - the Win32 equivalent of clearing ICANON|ECHO|ISIG below. */
    DWORD raw_mode = g_have_orig_mode ? g_orig_console_mode : 0;
    raw_mode &= (DWORD)~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT | ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT);
    SetConsoleMode(g_stdin_handle, raw_mode);
}

/* Blocks until one key is available, honoring a key held down long enough
 * to auto-repeat by re-emitting it wRepeatCount times via the static
 * queue below - matches the POSIX build effectively getting one byte per
 * repeat out of the tty driver. */
static int read_key(void) {
    static WORD pending_vk = 0;
    static char pending_char = 0;
    static int pending_count = 0;

    for (;;) {
        if (pending_count > 0) {
            pending_count--;
            if (pending_vk == VK_LEFT) return ARROW_LEFT;
            if (pending_vk == VK_RIGHT) return ARROW_RIGHT;
            if (pending_vk == VK_UP) return ARROW_UP;
            if (pending_vk == VK_DOWN) return ARROW_DOWN;
            return (unsigned char)pending_char;
        }

        INPUT_RECORD rec;
        DWORD read_count = 0;
        if (!ReadConsoleInputA(g_stdin_handle, &rec, 1, &read_count) || read_count == 0) {
            Sleep(10); /* back off instead of a tight-loop retry on a transient failure */
            return -1;
        }
        if (rec.EventType != KEY_EVENT || !rec.Event.KeyEvent.bKeyDown) continue;

        WORD vk = rec.Event.KeyEvent.wVirtualKeyCode;
        char ch = rec.Event.KeyEvent.uChar.AsciiChar;
        if (vk != VK_LEFT && vk != VK_RIGHT && vk != VK_UP && vk != VK_DOWN && ch == 0) continue; /* modifier-only, etc. */

        pending_vk = vk;
        pending_char = ch;
        pending_count = rec.Event.KeyEvent.wRepeatCount > 0 ? (int)rec.Event.KeyEvent.wRepeatCount : 1;
    }
}

#else /* POSIX */

static struct termios orig_term;

static void disable_raw_mode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_term);
}

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
}

static int read_key(void) {
    char c;
    int nread;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) return -1;
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
         * VTIME) - real fix for the headless case. */
        usleep(20000);
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
}

#endif

int main(void) {
    resolve_root();
    enable_raw_mode();

    for (;;) {
        int key = read_key();
        if (key == -1) continue;
        /* Ctrl+C (ETX, byte 3) - see enable_raw_mode()'s own comment for
         * why it arrives here as data instead of a real SIGINT. This is
         * the ONLY quit trigger now (mass-refactor 2026-07-26) - it's
         * consumed directly, never forwarded into history.txt, so it
         * can't be confused with ordinary typed content either. */
        if (key == 3) break;
        append_key(key);
    }

    disable_raw_mode();
    write_quit_flag();
    return 0;
}
