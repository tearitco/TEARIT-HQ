#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define GUI_STATE "projects/tsots-online/manager/gui_state.txt"

typedef struct {
    char auth_status[64];
    char tsots_online_screen[4096];
} AppState;

void load_layout(char *markup, const char *file) {
    FILE *fp = fopen(file, "r");
    if (!fp) { strcpy(markup, "Error loading layout"); return; }
    size_t n = fread(markup, 1, 4095, fp);
    markup[n] = '\0';
    fclose(fp);
}

int main() {
    AppState state;
    strcpy(state.auth_status, "GUEST"); // Default for now
    load_layout(state.tsots_online_screen, "projects/tsots-online/layouts/tsots-login.chtpm");

    FILE *debug = fopen("projects/tsots-online/manager/debug.log", "w");
    if (debug) { fprintf(debug, "Manager started\n"); fclose(debug); }

    while (1) {
        FILE *fp = fopen(GUI_STATE, "w");
        if (!fp) {
            FILE *debug = fopen("projects/tsots-online/manager/debug.log", "a");
            if (debug) { fprintf(debug, "Error opening gui_state.txt\n"); fclose(debug); }
        } else {
            fprintf(fp, "auth_status=%s\n", state.auth_status);
            fprintf(fp, "tsots_online_screen=%s\n", state.tsots_online_screen);
            fclose(fp);
        }
        usleep(100000);
    }
    return 0;
}
