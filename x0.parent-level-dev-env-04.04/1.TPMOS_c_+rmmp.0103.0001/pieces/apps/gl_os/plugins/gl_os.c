#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>

#define MAX_LINE 1024
#define MAX_PATH 4096
#define MAX_WINDOWS 10

static char project_root[MAX_PATH] = ".";

typedef struct {
    int id;
    int x, y, w, h;
    char title[64];
    char content_source[256]; // Path to another piece's view.txt
    bool active;
} VirtualWindow;

VirtualWindow windows[MAX_WINDOWS];
int window_count = 0;
int active_window_id = -1;

void log_master(const char* type, const char* event, const char* piece, const char* source) {
    time_t rawtime;
    struct tm *timeinfo;
    char timestamp[100];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);

    char *path = NULL;
    asprintf(&path, "%s/pieces/apps/gl_os/ledger/master_ledger.txt", project_root);
    FILE *ledger = fopen(path, "a");
    if (ledger) {
        fprintf(ledger, "[%s] %s: %s on %s | Source: %s\n", timestamp, type, event, piece, source);
        fclose(ledger);
    }
    free(path); // Free allocated memory
}

void save_gl_os_state() {
    FILE *f = fopen("pieces/apps/gl_os/state.txt", "w");
    if (!f) return;
    fprintf(f, "window_count=%d\n", window_count);
    fprintf(f, "active_window_id=%d\n", active_window_id);
    for (int i = 0; i < window_count; i++) {
        fprintf(f, "win_%d_pos=%d,%d,%d,%d\n", windows[i].id, windows[i].x, windows[i].y, windows[i].w, windows[i].h);
        fprintf(f, "win_%d_title=%s\n", windows[i].id, windows[i].title);
    }
    fclose(f);
}

void open_terminal_window() {
    if (window_count >= MAX_WINDOWS) return;
    int id = window_count++;
    windows[id].id = id;
    windows[id].x = 50 + (id * 30);
    windows[id].y = 50 + (id * 30);
    windows[id].w = 400;
    windows[id].h = 300;
    strcpy(windows[id].title, "Terminal");
    strcpy(windows[id].content_source, "pieces/chtpm/frame_buffer/current_frame.txt");
    windows[id].active = true;
    active_window_id = id;
    
    log_master("EventFire", "window_opened", "gl_os", "gl_os_module");
    save_gl_os_state();
}

void write_sovereign_view() {
    FILE *fp = fopen("pieces/apps/gl_os/view.txt", "w");
    if (!fp) return;
    
    // For GL-OS, the "view.txt" is a meta-description of the desktop for the Renderer
    fprintf(fp, "DESKTOP_VIEW\n");
    fprintf(fp, "BG: #000044\n");
    for (int i = 0; i < window_count; i++) {
        fprintf(fp, "WINDOW | %d | %d | %d | %d | %d | %s | %s\n", 
                windows[i].id, windows[i].x, windows[i].y, windows[i].w, windows[i].h, 
                windows[i].title, windows[i].content_source);
    }
    fclose(fp);
    
    // Signal view change
    FILE *marker = fopen("pieces/apps/gl_os/view_changed.txt", "a");
    if (marker) {
        fprintf(marker, "X\n");
        fclose(marker);
    }
}

int main() {
    // TPM: Launch dedicated renderer as a child process
    system("./pieces/apps/gl_os/plugins/+x/gl_os_renderer.+x &");
    
    // Initial State: One terminal window open by default
    open_terminal_window();
    write_sovereign_view();
    
    while (1) {
        // In a real PMO module, we would poll history.txt here
        // For the blueprint demo, we just maintain the view
        usleep(100000); 
    }
    return 0;
}
