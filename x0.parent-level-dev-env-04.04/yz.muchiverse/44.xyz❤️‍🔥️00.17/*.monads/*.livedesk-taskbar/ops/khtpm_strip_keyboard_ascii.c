/* khtpm_strip_keyboard_ascii.c — raw-terminal keyboard input capture for
 * the taskbar's ASCII mirror, matching TPMOS's real system/keyboard_
 * input.c pattern EXACTLY: raw termios, reads keys, writes resolved codes
 * to a relay file. NEVER prints anything to stdout - see khtpm_strip_
 * render_ascii.c's own header comment for why this split exists (the
 * previous combined binary's raw mode broke \n->\r\n translation for its
 * OWN stdout, producing a visible staircase in the rendered output -
 * direct user report, 2026-08-18).
 *
 * Writes:
 *   - #.desktop/livedesk_agent_relay.txt (bare decimal code per line, the
 *     real, existing external input relay - confirmed via direct read of
 *     poll_agent_relay() in khtpm_strip_parser.c)
 *
 * Reads: nothing but its own stdin.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

static char house_root[MAX_PATH] = ".";

static void resolve_root(int argc, char **argv) {
    if (argc > 1 && argv[1][0]) snprintf(house_root, sizeof(house_root), "%s", argv[1]);
}

static void relay_send(int code) {
    if (code <= 0) return;
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/livedesk_agent_relay.txt", house_root);
    FILE *f = fopen(path, "a");
    if (f) { fprintf(f, "%d\n", code); fclose(f); }
}

#define KSC_HQ_HEADER_BASE 4000
#define KSC_FOCUS_LEFT  1001
#define KSC_FOCUS_RIGHT 1002

static struct termios orig_term;
static void disable_raw_mode(void) { tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_term); }
static void enable_raw_mode(void) {
    tcgetattr(STDIN_FILENO, &orig_term);
    atexit(disable_raw_mode);
    struct termios raw = orig_term;
    raw.c_iflag &= ~(unsigned long)(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(unsigned long)(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(unsigned long)(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;  /* blocking single-byte read - this binary does
                            nothing else, no select()/redraw loop to share
                            time with, matching keyboard_input.c's own
                            real blocking read_key() shape exactly. */
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

/* Real, direct instruction (2026-08-18): "h" opens the HQ popup (cell 1) -
 * there is no digit/enter path to OPEN a header cell (only to navigate
 * once one is already open) - see dispatch_onclick()'s own ACTIVATE
 * branch in khtpm_strip_parser.c, the real precedent this mirrors
 * exactly. Digits, Enter, Escape, Backspace pass through raw. */
static void handle_key(int c) {
    if (c == 3) { disable_raw_mode(); exit(0); } /* Ctrl+C: quit only this binary */
    if (c == 'h' || c == 'H') { relay_send(KSC_HQ_HEADER_BASE + 1); return; }
    if (c >= '0' && c <= '9') { relay_send(c); return; }
    if (c == '\r' || c == '\n') { relay_send(13); return; }
    if (c == 127 || c == 8) { relay_send(8); return; }
    if (c == 27) {
        /* REAL BUG FIX 2026-08-18, direct user request ("index input jump
         * arrow control like chtpm parsers are used to having"): arrow
         * keys were read (ESC-sequence detected) but deliberately
         * discarded, not relayed. Now forwards KSC_FOCUS_LEFT/RIGHT -
         * matches the real GUI's own real Left/Up=-1, Right/Down=+1
         * collapse (khtpm_strip_parser.c's KeyPress handler, "ks ==
         * XK_Left || ks == XK_Up" / "ks == XK_Right || ks == XK_Down") -
         * a 1D strip has no real vertical axis, so up/down mean the same
         * thing left/right do, same as the real window. See
         * khtpm_strip_parser.c's own dispatch_key_code() for the matching
         * relay-forwarding fix this depends on (same pass). */
        char seq[2];
        if (read(STDIN_FILENO, &seq[0], 1) == 1 && read(STDIN_FILENO, &seq[1], 1) == 1) {
            if (seq[0] == '[') {
                if (seq[1] == 'D' || seq[1] == 'A') { relay_send(KSC_FOCUS_LEFT); return; }  /* Left or Up */
                if (seq[1] == 'C' || seq[1] == 'B') { relay_send(KSC_FOCUS_RIGHT); return; } /* Right or Down */
            }
            /* recognized ESC-prefixed sequence but not an arrow (e.g. Home/End/Fn) - fall through to bare Escape below */
        }
        relay_send(27);
        return;
    }
}

int main(int argc, char **argv) {
    resolve_root(argc, argv);
    enable_raw_mode();
    char c;
    while (read(STDIN_FILENO, &c, 1) == 1) {
        handle_key((unsigned char)c);
    }
    disable_raw_mode();
    return 0;
}
