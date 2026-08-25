#include <stdio.h>
#include <conio.h>
#include <windows.h>
#include <signal.h>

volatile sig_atomic_t g_interrupted = 0;

void ctrlc_handler(int sig) {
    (void)sig;
    g_interrupted = 1;
}

int main(void) {
    int ch, ch2;
    HANDLE hConsole;
    DWORD mode;

    // Set up Ctrl+C handler
    signal(SIGINT, ctrlc_handler);

    // Enable processed input to allow Ctrl+C to work
    hConsole = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(hConsole, &mode);
    SetConsoleMode(hConsole, mode | ENABLE_PROCESSED_INPUT);

    printf("Enter text (Press 'q' or Ctrl+C to exit):\n");

    while (1) {
        // Check for Ctrl+C signal
        if (g_interrupted) {
            printf("\nInterrupted by Ctrl+C. Exiting...\n");
            break;
        }

        ch = _getch();

        // Special key prefix (0xE0 = 224)
        if (ch == 0xE0) {
            ch2 = _getch();  // Read actual key code
            if (ch2 == 0x48)
                printf("[UP]\n");
            else if (ch2 == 0x50)
                printf("[DOWN]\n");
            else if (ch2 == 0x4B)
                printf("[LEFT]\n");
            else if (ch2 == 0x4D)
                printf("[RIGHT]\n");
            else
                printf("[SPECIAL: 0x%02X]\n", ch2);
        } else if (ch == 'q') {
            printf("\nExiting...\n");
            break;
        } else if (ch == 3) {
            // Ctrl+C detected directly via _getch()
            printf("\nInterrupted by Ctrl+C. Exiting...\n");
            break;
        } else {
            printf("'%c' -> %d (0x%02X)\n", ch, ch, ch);
        }
        fflush(stdout);
    }

    return 0;
}
