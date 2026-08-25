#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define HISTORY_PATH "pieces\\keyboard\\history.txt"
#define LEDGER_PATH "pieces\\keyboard\\ledger.txt"
#define MASTER_LEDGER_PATH "pieces\\master_ledger\\master_ledger.txt"

enum editorKey {
    ARROW_LEFT = 1000, ARROW_RIGHT = 1001, ARROW_UP = 1002, ARROW_DOWN = 1003, ESC_KEY = 27
};

HANDLE hIn = INVALID_HANDLE_VALUE;
DWORD origMode = 0;

void disableRawMode() {
    if (hIn != INVALID_HANDLE_VALUE) {
        SetConsoleMode(hIn, origMode);
    }
}

void enableRawMode() {
    hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (hIn == INVALID_HANDLE_VALUE) exit(1);
    GetConsoleMode(hIn, &origMode);
    atexit(disableRawMode);
    
    DWORD mode = ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT | ENABLE_PROCESSED_INPUT;
    SetConsoleMode(hIn, mode);
}

void writeCommand(int key) {
    if (key == 3) { // Ctrl+C detected
        printf("\n[KEYBOARD] Ctrl+C signal triggered.\n");
        GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0);
        return;
    }
    time_t rawtime;
    struct tm *timeinfo;
    char timestamp[100];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);

    FILE *kb_ledger = fopen(LEDGER_PATH, "a");
    if (kb_ledger) {
        char key_char = (key >= 32 && key <= 126) ? key : '?';
        fprintf(kb_ledger, "[%s] KeyCaptured: %d ('%c') | Source: keyboard_win\n", timestamp, key, key_char);
        fclose(kb_ledger);
    }

    FILE *fp = fopen(HISTORY_PATH, "a");
    if (fp) {
        fprintf(fp, "[%s] KEY_PRESSED: %d\n", timestamp, key);
        fclose(fp);
    }

    FILE *master = fopen(MASTER_LEDGER_PATH, "a");
    if (master) {
        fprintf(master, "[%s] InputReceived: key_code=%d | Source: keyboard_win\n", timestamp, key);
        fclose(master);
    }
}

int main() {
    enableRawMode();
    printf("Windows Keyboard Muscle Active (Direct Hardware Access)\n");
    
    INPUT_RECORD ir[128];
    DWORD count;
    
    while (1) {
        if (!ReadConsoleInput(hIn, ir, 128, &count)) break;
        
        for (DWORD i = 0; i < count; i++) {
            if (ir[i].EventType == KEY_EVENT && ir[i].Event.KeyEvent.bKeyDown) {
                int key = 0;
                WORD vk = ir[i].Event.KeyEvent.wVirtualKeyCode;
                char c = ir[i].Event.KeyEvent.uChar.AsciiChar;
                
                if (vk == VK_LEFT) key = ARROW_LEFT;
                else if (vk == VK_RIGHT) key = ARROW_RIGHT;
                else if (vk == VK_UP) key = ARROW_UP;
                else if (vk == VK_DOWN) key = ARROW_DOWN;
                else if (vk == VK_ESCAPE) key = ESC_KEY;
                else if (c != 0) key = (unsigned char)c;
                
                if (key != 0) {
                    writeCommand(key);
                }
            }
        }
    }
    return 0;
}
