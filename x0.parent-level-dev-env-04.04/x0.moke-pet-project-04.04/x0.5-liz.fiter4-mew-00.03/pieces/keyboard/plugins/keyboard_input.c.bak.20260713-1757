#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
    #include <termios.h>
    #include <unistd.h>
#else
    #include <conio.h>
    #include <windows.h>
    #include <direct.h>
    #define usleep(x) Sleep((x)/1000)
#endif
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>

enum editorKey {
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN
};

#ifndef _WIN32
struct termios orig_termios;
#endif
char history_path[256] = "pieces/keyboard/history.txt";
char base_path[256] = ".";  /* For TPM isolation */
int tty_fd = -1;  /* File descriptor for /dev/tty */

/* Check if GL-OS has input focus (TPM isolation protocol) */
int gl_os_has_focus(void) {
    struct stat st;
    char lock_path[512];
    snprintf(lock_path, sizeof(lock_path), "%s/pieces/apps/gl_os/session/input_focus.lock", base_path);
    return (stat(lock_path, &st) == 0);  /* File exists = GL-OS has focus */
}

void disableRawMode() {
#ifndef _WIN32
  if (tty_fd >= 0) {
    tcsetattr(tty_fd, TCSAFLUSH, &orig_termios);
    close(tty_fd);
  }
#endif
}

void enableRawMode() {
#ifndef _WIN32
  /* Open the controlling terminal directly - works even when stdin is redirected */
  tty_fd = open("/dev/tty", O_RDONLY);
  if (tty_fd < 0) {
    perror("Could not open /dev/tty");
    exit(1);
  }
  tcgetattr(tty_fd, &orig_termios);
  atexit(disableRawMode);
  struct termios raw = orig_termios;
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;
  tcsetattr(tty_fd, TCSAFLUSH, &raw);
#endif
}

int editorReadKey() {
#ifndef _WIN32
  int nread;
  char c_char;
  while ((nread = read(tty_fd, &c_char, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) exit(1);
    usleep(10000); /* 10ms sleep to prevent tight loop CPU spike */
  }
  int c = (unsigned char)c_char;
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
  return c;
#else
  int c = _getch();
  if (c == 0 || c == 224) {
    switch (_getch()) {
      case 72: return ARROW_UP;
      case 80: return ARROW_DOWN;
      case 77: return ARROW_RIGHT;
      case 75: return ARROW_LEFT;
    }
  }
  return c;
#endif
}

void writeCommand(int key) {
    /* TPM INPUT ISOLATION: Check if GL-OS has focus */
    if (gl_os_has_focus()) {
        /* GL-OS is active - skip writing to CHTPM input files */
        /* GL-OS receives input directly via GLUT callbacks */
        return;
    }

    time_t rawtime;
    struct tm *timeinfo;
    char timestamp[100];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);

    // Log to keyboard ledger (AUDIT OBSESSION!)
    FILE *kb_ledger = fopen("pieces/keyboard/ledger.txt", "a");
    if (kb_ledger) {
        char key_char = (key >= 32 && key <= 126) ? key : '?';
        fprintf(kb_ledger, "[%s] KeyCaptured: %d ('%c') | Source: keyboard_input\n",
                timestamp, key, key_char);
        fclose(kb_ledger);
    }

    // Write to history (EXISTING - KEEP)
    FILE *fp = fopen(history_path, "a");
    if (!fp) {
        // Ensure directory exists
#ifdef _WIN32
        _mkdir("pieces\\keyboard");
#else
        system("mkdir -p pieces/keyboard");
#endif
        fp = fopen(history_path, "a");
    }
    if (fp) {
        fprintf(fp, "[%s] KEY_PRESSED: %d\n", timestamp, key);
        fclose(fp);
    }

    // Log to master ledger (AUDIT OBSESSION!)
    FILE *master = fopen("pieces/master_ledger/master_ledger.txt", "a");
    if (master) {
        fprintf(master, "[%s] InputReceived: key_code=%d | Source: keyboard_input\n", timestamp, key);
        fclose(master);
    }
}

int main(int argc, char **argv) {
  if (argc > 1) strncpy(history_path, argv[1], 255);
  enableRawMode();
  while (1) {
    int c = editorReadKey();
    if (c == ((('c') & 0x1f))) break;
    writeCommand(c);
    usleep(10000);
  }
  return 0;
}
