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

struct termios orig_termios;

void die(const char *s) {
  perror(s);
  exit(1);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
    die("tcsetattr");
}

void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetattr");
  atexit(disableRawMode);

  struct termios raw = orig_termios;
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  // ────────────────────────────────────────────────
  // Removed: raw.c_oflag &= ~(OPOST);
  // This was causing \n not to produce \r\n → staggered/corrupt render output
  // ────────────────────────────────────────────────
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
  // Write to pieces/keyboard/history.txt as per TPM standards
  FILE *fp = fopen("pieces/keyboard/history.txt", "a"); // Append mode
  if (!fp) {
    // Ensure directory exists
#ifdef _WIN32
    _mkdir("pieces\\keyboard");
#else
    system("mkdir -p pieces/keyboard");
#endif
    fp = fopen("pieces/keyboard/history.txt", "a");
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
      fprintf(ledger, "[%s] InputReceived: key_code=%d | Source: keyboard_input_service\n", timestamp, key);
      fclose(ledger);
  }
}

int main() {
  enableRawMode();

  while (1) {
    int c = editorReadKey();
    if (c == CTRL_KEY('c')) {  // Check for Ctrl+C
      disableRawMode();        // Restore terminal settings
      exit(0);                 // Exit cleanly
    }
    
    // Echo the key for immediate feedback
    if (c >= 32 && c <= 126) { // Printable ASCII characters
        printf("\n[KEY ECHO]: %c\n", c);
    } else if (c >= 1000) { // Arrow keys and special keys
        switch(c) {
            case 1000: printf("\n[KEY ECHO]: UP ARROW\n"); break;
            case 1001: printf("\n[KEY ECHO]: DOWN ARROW\n"); break;
            case 1002: printf("\n[KEY ECHO]: RIGHT ARROW\n"); break;
            case 1003: printf("\n[KEY ECHO]: LEFT ARROW\n"); break;
            default: printf("\n[KEY ECHO]: SPECIAL KEY (%d)\n", c); break;
        }
    } else {
        printf("\n[KEY ECHO]: CODE (%d)\n", c);
    }
    
    writeCommand(c);
    usleep(16667); // ~60 fps / 16.667 ms delay
  }

  return 0;
}
