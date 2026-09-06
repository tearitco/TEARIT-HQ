/* khtpm_kbd_ascii.c — GENERIC raw-terminal keyboard relay for any khtpm
 * window.
 *
 * The strip's khtpm_strip_keyboard_ascii.c, retargeted from
 * strip_history.txt to the PER-PID relay every non-strip khtpm window
 * already polls (poll_agent_history() / history_path() in
 * khtpm_core_render.c):
 *     #.desktop/entity_menu_history/<pid>.txt
 * One event per line, appended, cursor-based - NEVER truncate it here.
 *
 * Line format that relay expects (see _.0.aigent-testing-k9.txt):
 *   KEY_PRESSED: <decimal>
 *     printable ASCII 32..126 -> that code; 13=Enter 27=Escape
 *     8=Backspace 9=Tab; 200/201/202/203 = Up/Down/Left/Right;
 *     204/205 = PageUp/PageDown.
 *
 * NO stdout - raw termios clears OPOST for the whole tty; anything that
 * prints on the same terminal staircases (see khtpm_strip_render_
 * ascii.c's header). Presenter and keyboard are always two processes.
 *
 * Usage: khtpm_kbd_ascii.+x <house_root> <pid>
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

static char g_relay_path[PATH_BUF];

static void relay(int code) {
    if (code <= 0) return;
    FILE *f = fopen(g_relay_path, "a");
    if (f) { fprintf(f, "KEY_PRESSED: %d\n", code); fclose(f); }
}

static struct termios g_orig;
static void raw_off(void) { tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig); }
static void raw_on(void) {
    tcgetattr(STDIN_FILENO, &g_orig);
    atexit(raw_off);
    struct termios raw = g_orig;
    raw.c_iflag &= ~(unsigned long)(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(unsigned long)(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(unsigned long)(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static void handle(int c) {
    if (c == 3) { raw_off(); _exit(0); }          /* Ctrl+C: quit this relay only */
    if (c >= 32 && c <= 126) { relay(c); return; }
    if (c == '\r' || c == '\n') { relay(13); return; }
    if (c == 127 || c == 8)    { relay(8);  return; }
    if (c == '\t')             { relay(9);  return; }
    if (c == 27) {
        char s0, s1;
        if (read(STDIN_FILENO, &s0, 1) == 1 && read(STDIN_FILENO, &s1, 1) == 1 && s0 == '[') {
            switch (s1) {
                case 'A': relay(200); return;      /* Up    */
                case 'B': relay(201); return;      /* Down  */
                case 'C': relay(203); return;      /* Right */
                case 'D': relay(202); return;      /* Left  */
                case '5': { char t; if (read(STDIN_FILENO,&t,1)==1) {} relay(204); return; } /* PgUp  */
                case '6': { char t; if (read(STDIN_FILENO,&t,1)==1) {} relay(205); return; } /* PgDn  */
            }
        }
        relay(27);                                 /* bare Escape */
        return;
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <house_root> <pid>\n", argv[0]);
        return 1;
    }
    snprintf(g_relay_path, sizeof(g_relay_path),
             "%s/#.desktop/entity_menu_history/%s.txt", argv[1], argv[2]);
    raw_on();
    char c;
    while (read(STDIN_FILENO, &c, 1) == 1) handle((unsigned char)c);
    raw_off();
    return 0;
}
