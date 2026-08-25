#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <signal.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

#define ARROW_LEFT  1000
#define ARROW_RIGHT 1001
#define ARROW_UP    1002
#define ARROW_DOWN  1003

static char project_root[MAX_PATH] = ".";
static struct termios orig_term;
static int tty_fd = -1;

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
    if (tty_fd >= 0) tcsetattr(tty_fd, TCSAFLUSH, &orig_term);
}

static void write_quit_flag(void);

static void handle_signal(int sig) {
    (void)sig;
    disable_raw_mode();
    write_quit_flag();
    _exit(0);
}

static void enable_raw_mode(void) {
    tty_fd = open("/dev/tty", O_RDWR);
    if (tty_fd < 0) { perror("open /dev/tty"); _exit(1); }
    tcgetattr(tty_fd, &orig_term);
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
    tcsetattr(tty_fd, TCSAFLUSH, &raw);
}

static int read_key(void) {
    char c;
    int nread;
    while ((nread = read(tty_fd, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) return -1;
        usleep(10000);
    }
    if (c == '\x1b') {
        char seq[2];
        if (read(tty_fd, &seq[0], 1) != 1) return '\x1b';
        if (read(tty_fd, &seq[1], 1) != 1) return '\x1b';
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
    snprintf(path, sizeof(path), "%s/pieces/keyboard/history.txt", project_root);
    FILE *f = fopen(path, "a");
    if (f) {
        fprintf(f, "KEY_PRESSED: %d\n", key);
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

    char history_path[PATH_BUF];
    snprintf(history_path, sizeof(history_path), "%s/pieces/apps/player_app/interact_relay.txt", project_root);
    FILE *hf = fopen(history_path, "w");
    if (hf) fclose(hf);

    enable_raw_mode();

    for (;;) {
        int key = read_key();
        if (key == -1) { usleep(10000); continue; }
        append_key(key);
        usleep(10000);
        if (key == 3) break;
    }

    disable_raw_mode();
    write_quit_flag();
    return 0;
}
