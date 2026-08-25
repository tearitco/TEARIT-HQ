#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>

#define CTRL_KEY(k) ((k) & 0x1f)

enum editorKey {
    BACKSPACE = 127,
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};

struct termios orig_term;

void die(const char *s) {
    perror(s);
    exit(1);
}

void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_term) == -1)
        die("tcsetattr");
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &orig_term) == -1) die("tcgetattr");
    atexit(disableRawMode);

    struct termios raw = orig_term;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

int editorReadKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }

    if (c == '\x1b') {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1': return HOME_KEY;
                        case '3': return DEL_KEY;
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                    }
                }
            } else {
                switch (seq[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                }
            }
        } else if (seq[0] == 'O') {
            switch (seq[1]) {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        }
        return '\x1b';
    } else {
        return c;
    }
}

void writeCommand(int key) {
    // Ensure directory exists
    mkdir("pieces/keyboard", 0755);
    mkdir("pieces/master_ledger", 0755);
    
    // Write to pieces/keyboard/history.txt as per TPM standards
    FILE *fp = fopen("pieces/keyboard/history.txt", "a");
    if (!fp) {
        fp = fopen("pieces/keyboard/history.txt", "w");
    }
    if (!fp) die("fopen keyboard history file");
    
    // Add timestamp for audit trail
    time_t rawtime;
    struct tm *timeinfo;
    char timestamp[100];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
    
    fprintf(fp, "[%s] KEY_PRESSED: %d\n", timestamp, key);
    fclose(fp);
    
    // Also log to master ledger for complete audit trail
    FILE *ledger = fopen("pieces/master_ledger/master_ledger.txt", "a");
    if (ledger) {
        fprintf(ledger, "[%s] InputReceived: key_code=%d | Source: input_capture\n", timestamp, key);
        fclose(ledger);
    }
}

int main() {
    // Ensure directories exist
    mkdir("pieces", 0755);
    mkdir("pieces/keyboard", 0755);
    mkdir("pieces/master_ledger", 0755);
    
    enableRawMode();

    while (1) {
        int c = editorReadKey();
        if (c == CTRL_KEY('c')) {
            disableRawMode();
            exit(0);
        }
        
        writeCommand(c);
        usleep(16667); // ~60 fps
    }

    return 0;
}
