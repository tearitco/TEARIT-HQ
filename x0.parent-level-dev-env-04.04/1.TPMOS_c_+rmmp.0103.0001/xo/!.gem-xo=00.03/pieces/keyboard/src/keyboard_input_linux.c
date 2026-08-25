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

#define HISTORY_PATH "pieces/keyboard/history.txt"
#define LEDGER_PATH "pieces/keyboard/ledger.txt"
#define MASTER_LEDGER_PATH "pieces/master_ledger/master_ledger.txt"

enum editorKey {
    ARROW_LEFT = 1000, ARROW_RIGHT = 1001, ARROW_UP = 1002, ARROW_DOWN = 1003, ESC_KEY = 27
};

struct termios orig_termios;
int tty_fd = -1;

void disableRawMode() {
    if (tty_fd >= 0) {
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

void enableRawMode() {
    tty_fd = open("/dev/tty", O_RDONLY);
    if (tty_fd == -1) {
        perror("open /dev/tty");
        exit(1);
    }
    tcgetattr(tty_fd, &orig_termios);
    
    /* Setup signal handlers to restore terminal on exit */
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
}

int readKey() {
    int nread;
    char c;
    while ((nread = read(tty_fd, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) return -1;
        usleep(10000);
    }
    if (c == '\x1b') {
        char seq[3];
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

void writeCommand(int key) {
    time_t rawtime;
    struct tm *timeinfo;
    char timestamp[100];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);

    FILE *kb_ledger = fopen(LEDGER_PATH, "a");
    if (kb_ledger) {
        char key_char = (key >= 32 && key <= 126) ? key : '?';
        fprintf(kb_ledger, "[%s] KeyCaptured: %d ('%c') | Source: keyboard_linux\n", timestamp, key, key_char);
        fclose(kb_ledger);
    }

    FILE *fp = fopen(HISTORY_PATH, "a");
    if (fp) {
        fprintf(fp, "[%s] KEY_PRESSED: %d\n", timestamp, key);
        fclose(fp);
    }

    FILE *master = fopen(MASTER_LEDGER_PATH, "a");
    if (master) {
        fprintf(master, "[%s] InputReceived: key_code=%d | Source: keyboard_linux\n", timestamp, key);
        fclose(master);
    }
}

int main() {
    enableRawMode();
    printf("Linux Keyboard Muscle Active (Press Ctrl+C to exit via Orchestrator)\n");
    while (1) {
        int c = readKey();
        if (c == -1) continue;
        if (c == 3) break; /* Ctrl+C handled by orchestrator, but we check just in case */
        writeCommand(c);
    }
    return 0;
}
