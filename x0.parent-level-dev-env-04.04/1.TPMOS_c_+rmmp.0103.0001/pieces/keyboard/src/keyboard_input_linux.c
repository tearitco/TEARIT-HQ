#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>

#define HISTORY_PATH "pieces/keyboard/history.txt"
#define LEDGER_PATH "pieces/keyboard/ledger.txt"
#define MASTER_LEDGER_PATH "pieces/master_ledger/master_ledger.txt"

enum editorKey {
    ARROW_LEFT = 1000, ARROW_RIGHT = 1001, ARROW_UP = 1002, ARROW_DOWN = 1003, ESC_KEY = 27
};

struct termios orig_termios;
int tty_fd = -1;
int mouse_reporting_enabled = 0;

/* Check if GL-OS has input focus (TPM isolation protocol) */
int gl_os_has_focus(void) {
    struct stat st;
    return (stat("pieces/apps/gl_os/session/input_focus.lock", &st) == 0);
}

void disableRawMode() {
    if (tty_fd >= 0) {
        /* Disable mouse tracking (1003l = Any event, 1006l = SGR) */
        write(tty_fd, "\x1b[?1003l\x1b[?1006l", 16);
        tcflush(tty_fd, TCIFLUSH);
        tcsetattr(tty_fd, TCSAFLUSH, &orig_termios);
        close(tty_fd);
        tty_fd = -1;
    }
}

void handle_signal(int sig) {
    (void)sig;
    disableRawMode();
    exit(0);
}

/* Check if any project has requested mouse reporting */
int mouse_reporting_requested(void) {
    struct stat st;
    /* Check for presence of mouse_enabled variable or lock */
    if (stat("pieces/mouse/mouse_enabled.lock", &st) == 0) return 1;
    if (stat("pieces/apps/playrm/session/mouse_enabled.lock", &st) == 0) return 1;
    if (stat("projects/mouse-test/session/mouse_enabled.lock", &st) == 0) return 1;
    return 0;
}

void sync_mouse_reporting(void) {
    int requested = mouse_reporting_requested();
    if (tty_fd < 0) return;

    if (requested && !mouse_reporting_enabled) {
        write(tty_fd, "\x1b[?1003h\x1b[?1006h", 16);
        mouse_reporting_enabled = 1;
    } else if (!requested && mouse_reporting_enabled) {
        write(tty_fd, "\x1b[?1003l\x1b[?1006l", 16);
        mouse_reporting_enabled = 0;
    }
}

void enableRawMode() {
    tty_fd = open("/dev/tty", O_RDWR);
    if (tty_fd == -1) {
        perror("open /dev/tty");
        exit(1);
    }
    
    /* AGGRESSIVE RESET: Ensure mouse mode is OFF before starting */
    write(tty_fd, "\x1b[?1003l\x1b[?1006l\x1b[?1000l", 24);
    mouse_reporting_enabled = 0;

    tcgetattr(tty_fd, &orig_termios);
    
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGHUP, handle_signal);
    
    atexit(disableRawMode);
    
    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    tcsetattr(tty_fd, TCSAFLUSH, &raw);

    sync_mouse_reporting();
}

void writeMouseCommand(int btn, int x, int y) {
    if (gl_os_has_focus()) return;

    static int last_x = -1, last_y = -1, last_btn = -1;
    static struct timespec last_send_time = {0, 0};
    
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    
    /* Calculate elapsed time in milliseconds */
    long elapsed_ms = (now.tv_sec - last_send_time.tv_sec) * 1000 + 
                      (now.tv_nsec - last_send_time.tv_nsec) / 1000000;

    /* Throttling: Only send if position changed AND at least 16ms passed (60 FPS)
       OR if a button state changed (always send clicks immediately) */
    if (btn == last_btn && x == last_x && y == last_y) return;
    if (btn == last_btn && elapsed_ms < 16) return;

    last_x = x; last_y = y; last_btn = btn;
    last_send_time = now;

    time_t rawtime; struct tm *timeinfo; char timestamp[100];
    time(&rawtime); timeinfo = localtime(&rawtime);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);

    FILE *fp = fopen(HISTORY_PATH, "a");
    if (fp) {
        /* Raw mouse audit only; project managers should consume semantic COMMAND lines. */
        fprintf(fp, "[%s] MOUSE_EVENT: %d %d %d\n", timestamp, btn, x, y);
        fclose(fp);
    }
}

int readKey() {
    int nread;
    char c;
    while ((nread = read(tty_fd, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) return -1;
        usleep(10000);
        sync_mouse_reporting();
    }
    if (c == '\x1b') {
        char seq[3];
        if (read(tty_fd, &seq[0], 1) != 1) return '\x1b';
        if (read(tty_fd, &seq[1], 1) != 1) return '\x1b';

        if (seq[0] == '[') {
            if (seq[1] == '<') {
                /* SGR Mouse Mode: \x1b[<Pb;Px;PyM or \x1b[<Pb;Px;Pym */
                int b = 0, x = 0, y = 0;
                char ch;
                /* Pb */
                while (read(tty_fd, &ch, 1) == 1 && ch != ';') {
                    if (ch >= '0' && ch <= '9') b = b * 10 + (ch - '0');
                }
                /* Px */
                while (read(tty_fd, &ch, 1) == 1 && ch != ';') {
                    if (ch >= '0' && ch <= '9') x = x * 10 + (ch - '0');
                }
                /* Py */
                while (read(tty_fd, &ch, 1) == 1 && ch != 'M' && ch != 'm') {
                    if (ch >= '0' && ch <= '9') y = y * 10 + (ch - '0');
                }
                writeMouseCommand(b, x, y);
                return 0; /* Handled as mouse */
            }
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

void writeCommand(int key) {
    if (key == 0) return;
    if (gl_os_has_focus()) return;

    time_t rawtime;
    struct tm *timeinfo;
    char timestamp[100];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);

    FILE *fp = fopen(HISTORY_PATH, "a");
    if (fp) {
        fprintf(fp, "[%s] KEY_PRESSED: %d\n", timestamp, key);
        fclose(fp);
    }
}

int main() {
    enableRawMode();
    printf("Linux Keyboard Muscle Active (Press Ctrl+C to exit via Orchestrator)\n");
    while (1) {
        int c = readKey();
        if (c == -1) continue;
        if (c == 3) break;
        writeCommand(c);
    }
    return 0;
}
