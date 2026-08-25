/* keyboard_input - SHARED OP (yz.muchiverse/2.muchi-verse/shared-ops/,
 * see shared-ops-refactor-plan.txt for the why - moved here from being
 * independently copied byte-identical into mutaclsym/zoo_0000/
 * muchipal-editor-0.0, confirmed via diff before the move) - standalone
 * raw-terminal input capture, no ncurses. Mirrors the real TPMOS
 * pattern (pieces/system/input_dispatcher/plugins/input_capture.c):
 * puts STDIN into raw termios mode directly, decodes "ESC [ A/B/C/D"
 * arrow sequences itself, and appends bare decimal keycodes to
 * pieces/apps/player_app/history.txt - one int per line, matching what
 * prisc+x's read_history opcode already expects (it fseeks to a byte
 * cursor and fscanf("%d", ...), so this file must stay bare-decimal,
 * not the timestamped "[ts] KEY_PRESSED: N" format some other TPMOS
 * subsystems use).
 *
 * CHTPM-BRIDGE ADDITION (see chtpm-to-pal-layout-plan.txt and
 * shared-ops/chtpm_parser_pal.c's own header comment for the full
 * why): append_key() now ALSO appends "KEY_PRESSED: N\n" to
 * pieces/keyboard/history.txt - real 1.TPMOS's own chtpm_parser.c
 * reads that exact file in that exact format, completely unmodified.
 * Deliberately NOT done inside chtpm_parser_pal.c's own main() loop
 * (a 3000-line vendored fork) - keeping the bridge here, in a small
 * file this family already accepts copying/maintaining per project,
 * keeps that fork's own diff against real upstream as small as
 * possible. Harmless for any project that never runs
 * chtpm_parser_pal.c - the second file just accumulates unread, same
 * "inert if nothing reads it" pattern used everywhere else in this
 * family.
 *
 * Self-contained: own root resolution, own constants, no shared headers.
 * Exits (and restores the terminal) on Ctrl+C ONLY (xyzos-standards.txt
 * sec. 23's own pitfall entry - 'q' used to also quit, which meant
 * typing an ordinary 'q' anywhere killed the session; removed
 * 2026-07-20). Drops a byte in pieces/system/quit_flag.txt on exit so
 * the renderer knows to stop polling - the real, sole quit signal
 * every consumer relies on, independent of which key triggered it. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <signal.h>

#define MAX_PATH 4096
/* Room for MAX_PATH worth of project_root plus the longest relative
 * suffix this file appends, so gcc can prove snprintf can't truncate. */
#define PATH_BUF (MAX_PATH + 256)

#define ARROW_LEFT  1000
#define ARROW_RIGHT 1001
#define ARROW_UP    1002
#define ARROW_DOWN  1003

static char project_root[MAX_PATH] = ".";
static struct termios orig_term;

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) { snprintf(project_root, sizeof(project_root), "%s", env); return; }
    if (!getcwd(project_root, sizeof(project_root))) snprintf(project_root, sizeof(project_root), ".");
}

static void disable_raw_mode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_term);
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

/* CORRECTION (found via direct testing, not assumed): this file used to
 * check pieces/system/gl_focus.lock (gl_os_has_focus()-style) before
 * writing, on the theory that real 1.TPMOS's own keyboard_input.c does
 * the same. It does - but for a DIFFERENT, narrower reason than "avoid
 * double-writes between keyboard_input and a GL window in general":
 * that check exists there specifically because 1.TPMOS's
 * joystick_input.c ALSO shares the same lock, and a joystick reading a
 * raw /dev/input/js0 device has NO window-focus concept at all - the
 * file lock is the only way to arbitrate a joystick against a
 * keyboard-driven GL window. mutaclsym has no joystick (not yet built -
 * see dox/00-HANDOFF.md). Between two ordinary keyboard-reading
 * processes with separate real windows (this process's controlling
 * terminal vs. system/gl_mirror.c's own GLUT window), the OS/window
 * manager ALREADY guarantees only the currently-focused window's
 * process receives any given keystroke - a second, file-based lock on
 * top of that is redundant, and was actively harmful in practice: it
 * made this process permanently stop honoring real terminal keystrokes
 * the instant gl_mirror's process started (lock file written at ITS
 * startup, unconditionally), even during the - possibly indefinite, if
 * gl_mirror's window never happens to receive real OS focus - stretch
 * where the terminal still had genuine focus and the user was actually
 * typing into it. Confirmed via egg-pals' own system/keyboard_input.c
 * (same pal-family, GL window of its own via system/egg_window.c): it
 * has NO such lock-file check at all, because egg_window's input is
 * mouse-driven, not a second keyboard source - i.e. this check was only
 * ever needed for the joystick case elsewhere in the family, and was
 * over-applied here. Removed; OS-level window focus is the real
 * arbiter, exactly as it already is for any two ordinary GUI windows. */
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

    /* 'q'-QUITS-EVERYTHING REMOVED (direct user correction, 2026-07-20:
     * "none of these projects should be using 'Q' TO QUIT. that was
     * really dumb. we use ctrl-c or would add a quit button, so not to
     * interfere with keyboard" - xyzos-standards.txt sec. 23's own
     * pitfall entry has the full writeup). The old code remapped
     * Ctrl+C (byte 3/ETX - arrives as plain data here, not a real
     * SIGINT, since enable_raw_mode() clears ISIG) to a literal 'q'
     * BEFORE appending it to history.txt, then quit on seeing 'q' -
     * meaning typing an ORDINARY 'q' anywhere (a chat message, a room
     * name, a username, a wallet ID) silently killed the whole session
     * mid-input. No consumer anywhere in this family actually needs a
     * literal 'q' in history.txt to know a quit happened - the real
     * signal every renderer.c already polls is
     * pieces/system/quit_flag.txt (written by write_quit_flag() below,
     * unconditionally, regardless of which key triggered the quit) -
     * confirmed by direct grep, no pal script anywhere checks for a
     * literal 'q'/113 key value. Ctrl+C now quits directly, on its own
     * byte value, WITHOUT being appended to history.txt at all (it was
     * never a real keystroke the app layer needed to see) and WITHOUT
     * touching the meaning of a literal 'q' keypress, which is now
     * just an ordinary character like any other. */
    for (;;) {
        int key = read_key();
        if (key == -1) continue;
        if (key == 3) break; /* Ctrl+C (ETX) - real quit, not appended as a keystroke */
        append_key(key);
    }

    disable_raw_mode();
    write_quit_flag();
    return 0;
}
