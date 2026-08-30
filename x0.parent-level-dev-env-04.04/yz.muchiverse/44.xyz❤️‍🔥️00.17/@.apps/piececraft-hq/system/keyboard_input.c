/* keyboard_input - SHARED OP family copy for piececraft-hq.
 * Linux: raw termios (unchanged). Windows: conio _getch/_kbhit
 * (surgical #ifdef _WIN32, same pattern as 014.wsr-pal keyboard_input.c).
 * CHTPM-BRIDGE: also writes pieces/keyboard/history.txt KEY_PRESSED lines.
 * Quit: Ctrl+C (ETX) only — never 'q' (xyzos-standards §23). */
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
#define usleep(x) Sleep((DWORD)((x) / 1000))
#define getcwd _getcwd
#else
#include <unistd.h>
#include <termios.h>
#endif

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif
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
    /* Prefer "." when CWD already has pieces/ (emoji abs paths break fopen). */
    {
#ifdef F_OK
        if (access("pieces", F_OK) == 0) {
#else
        if (access("pieces", 0) == 0) {
#endif
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
    if (c == 13) return 10; /* Enter parity with Linux LF */
    return c;
#else
    char c;
    int nread;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
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

    char history_path[PATH_BUF];
    snprintf(history_path, sizeof(history_path), "%s/pieces/apps/player_app/history.txt", project_root);
    FILE *hf = fopen(history_path, "w");
    if (hf) fclose(hf);

    enable_raw_mode();

    /* Ctrl+C quits; do NOT append ETX; never quit on 'q'. */
    for (;;) {
        int key = read_key();
        if (key == -1) continue;
        if (key == 3) break;
        append_key(key);
    }

    disable_raw_mode();
    write_quit_flag();
    return 0;
}
