/*
 * windows_renderer.c - Text-based renderer for Windows
 * 
 * This is the Windows-specific text renderer that displays CHTPM+OS frames
 * in the PowerShell/CMD terminal. It reads current_frame.txt and renders
 * it to the console with proper screen clearing and cursor management.
 * 
 * Architecture: Modular separation from Linux renderer (renderer.c)
 * - Linux: Uses ANSI escape codes (\033[H\033[J)
 * - Windows: Uses Console API (FillConsoleOutputCharacter, SetConsoleCursorPosition)
 * 
 * Usage: windows_renderer.+x (spawned by orchestrator on Windows)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <windows.h>

#define MAX_LINE 1024

/* Global variables for tracking changes */
static off_t last_marker_size = 0;
static HANDLE h_console = INVALID_HANDLE_VALUE;
static CONSOLE_SCREEN_BUFFER_INFO orig_csbi;

/*
 * Clear screen using Windows Console API
 * This is the Windows equivalent of printf("\033[H\033[J") on Linux
 */
static void clear_screen(void) {
    if (h_console == INVALID_HANDLE_VALUE) {
        h_console = GetStdHandle(STD_OUTPUT_HANDLE);
        if (h_console == INVALID_HANDLE_VALUE) return;
        GetConsoleScreenBufferInfo(h_console, &orig_csbi);
    }

    COORD coordScreen = {0, 0};
    DWORD cCharsWritten;
    DWORD dwConSize = orig_csbi.dwSize.X * orig_csbi.dwSize.Y;

    /* Fill with spaces */
    FillConsoleOutputCharacterA(h_console, ' ', dwConSize, coordScreen, &cCharsWritten);
    FillConsoleOutputAttribute(h_console, orig_csbi.wAttributes, dwConSize, coordScreen, &cCharsWritten);
    SetConsoleCursorPosition(h_console, coordScreen);
}

/*
 * Check if frame history is enabled
 * Reads pieces/display/state.txt
 */
static int is_history_on(void) {
    FILE *f = fopen("pieces/display/state.txt", "r");
    if (!f) return 1;  /* Default: history on */
    
    char line[128];
    int on = 1;
    if (fgets(line, sizeof(line), f)) {
        if (strstr(line, "off")) on = 0;
    }
    fclose(f);
    return on;
}

/*
 * Render the display content from current_frame.txt
 * This is the core rendering function
 */
static void render_display(void) {
    /* Get timestamp */
    time_t rawtime;
    struct tm *timeinfo;
    char timestamp[100];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);

    if (is_history_on()) {
        /* History mode: Print frame with separator (scrolling output) */
        printf("\n\n\n\n\n");  /* 5 blank lines for visual separation */
        printf("--- FRAME UPDATE at %s ---\n", timestamp);
    } else {
        /* Static UI mode: Clear screen and redraw */
        clear_screen();
    }

    /* Read and print current_frame.txt */
    FILE *frame_file = fopen("pieces/display/current_frame.txt", "r");
    if (frame_file) {
        fseek(frame_file, 0, SEEK_END);
        long file_size = ftell(frame_file);
        fseek(frame_file, 0, SEEK_SET);

        if (file_size > 0) {
            char *content = malloc(file_size + 1);
            if (content) {
                size_t bytes_read = fread(content, 1, file_size, frame_file);
                content[bytes_read] = '\0';
                printf("%s", content);
                free(content);
            }
        }
        fclose(frame_file);
    }
    fflush(stdout);

    /* Log to display ledger (audit trail) */
    FILE *display_ledger = fopen("pieces/display/ledger.txt", "a");
    if (display_ledger) {
        fprintf(display_ledger, "[%s] FrameRendered: from current_frame.txt | Source: windows_renderer\n", timestamp);
        fclose(display_ledger);
    }

    /* Log to master ledger */
    FILE *master = fopen("pieces/master_ledger/master_ledger.txt", "a");
    if (master) {
        fprintf(master, "[%s] FrameRendered: at %s | Source: windows_renderer\n", timestamp, timestamp);
        fclose(master);
    }

    /* Log full frame to session history (debugging) */
    FILE *history = fopen("pieces/debug/frames/session_frame_history.txt", "a");
    if (history) {
        fprintf(history, "\n--- FRAME UPDATE at %s ---\n", timestamp);
        FILE *frame_file = fopen("pieces/display/current_frame.txt", "r");
        if (frame_file) {
            fseek(frame_file, 0, SEEK_END);
            long file_size = ftell(frame_file);
            fseek(frame_file, 0, SEEK_SET);
            if (file_size > 0) {
                char *content = malloc(file_size + 1);
                if (content) {
                    size_t bytes_read = fread(content, 1, file_size, frame_file);
                    content[bytes_read] = '\0';
                    fprintf(history, "%s\n", content);
                    free(content);
                }
            }
            fclose(frame_file);
        }
        fclose(history);
    }
}

/*
 * Main entry point
 * Monitors renderer_pulse.txt for changes and renders when updated
 */
int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    /* Clear session history at start */
    FILE *clear_hist = fopen("pieces/debug/frames/session_frame_history.txt", "w");
    if (clear_hist) {
        fprintf(clear_hist, "=== NEW SESSION at ");
        time_t rawtime;
        struct tm *timeinfo;
        char ts[100];
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", timeinfo);
        fprintf(clear_hist, "%s ===\n", ts);
        fclose(clear_hist);
    }

    /* Initial render */
    render_display();

    /* Monitor pulse file for changes */
    struct stat st;
    const char* pulse_path = "pieces/display/renderer_pulse.txt";
    if (stat(pulse_path, &st) == 0) {
        last_marker_size = st.st_size;
    }

    /* Main render loop - poll for changes */
    while (1) {
        if (stat(pulse_path, &st) == 0) {
            if (st.st_size != last_marker_size) {
                render_display();
                last_marker_size = st.st_size;
            }
        }
        Sleep(17);  /* ~60 FPS (16.67ms), Windows equivalent of usleep */
    }

    return 0;
}
