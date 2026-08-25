#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <signal.h>
#include <time.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

#define ARROW_LEFT  1000
#define ARROW_RIGHT 1001
#define ARROW_UP    1002
#define ARROW_DOWN  1003

static char project_root[MAX_PATH] = ".";
static struct termios orig_term;

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) {
        snprintf(project_root, sizeof(project_root), "%s", env);
        return;
    }
    if (!getcwd(project_root, sizeof(project_root))) {
        snprintf(project_root, sizeof(project_root), ".");
    }
}

static void disable_raw_mode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_term);
}

static void write_quit_flag(void);

static void handle_signal(int sig) {
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

static void append_key(int key) {
    char path[PATH_BUF];
    char timestamp[64];
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);

    /* Write bare int to player_app/history.txt (for game_manager / prisc+x) */
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/history.txt", project_root);
    FILE *f = fopen(path, "a");
    if (f) {
        fprintf(f, "%d\n", key);
        fclose(f);
    }

    /* Write KEY_PRESSED format to keyboard/history.txt (for chtpm_parser) */
    snprintf(path, sizeof(path), "%s/pieces/keyboard/history.txt", project_root);
    f = fopen(path, "a");
    if (f) {
        fprintf(f, "[%s] KEY_PRESSED: %d\n", timestamp, key);
        fclose(f);
    }
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

    char relay_path[PATH_BUF];
    snprintf(relay_path, sizeof(relay_path), "%s/pieces/apps/player_app/interact_relay.txt", project_root);
    FILE *hf = fopen(relay_path, "w");
    if (hf) fclose(hf);

    enable_raw_mode();

    for (;;) {
        int key = read_key();
        if (key == -1) continue;
        append_key(key);
        if (key == 3) break;
    }

    disable_raw_mode();
    write_quit_flag();
    return 0;
}
