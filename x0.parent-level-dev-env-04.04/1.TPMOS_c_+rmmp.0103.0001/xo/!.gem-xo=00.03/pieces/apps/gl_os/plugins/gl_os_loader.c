/*
 * gl_os_loader.c - Switch to GL-OS desktop layout
 * Simple loader that sets up GL-OS context
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_PATH 4096

#ifdef _WIN32
#ifndef _vscprintf
/* Simple asprintf for Windows */
#include <stdarg.h>
int asprintf(char** strp, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int len = _vscprintf(fmt, args);
    if (len < 0) return -1;
    *strp = (char*)malloc(len + 1);
    if (!*strp) return -1;
    int result = vsprintf(*strp, fmt, args);
    va_end(args);
    return result;
}
#endif
#endif

char project_root[MAX_PATH] = ".";

void resolve_root(void) {
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (!kvp) return;
    
    char line[2048];
    while (fgets(line, sizeof(line), kvp)) {
        if (strncmp(line, "project_root=", 13) == 0) {
            char *v = line + 13;
            v[strcspn(v, "\n\r")] = 0;
            if (strlen(v) > 0) strncpy(project_root, v, MAX_PATH-1);
            break;
        }
    }
    fclose(kvp);
}

int main(void) {
    resolve_root();
    
    /* Set project_id for GL-OS */
    char *state_path = NULL;
    if (asprintf(&state_path, "%s/pieces/apps/player_app/manager/state.txt", project_root) != -1) {
        FILE *f = fopen(state_path, "a");
        if (f) {
            fprintf(f, "project_id=gl-os\n");
            fprintf(f, "active_target_id=desktop\n");
            fclose(f);
        }
        free(state_path);
    }
    
    /* Switch to GL-OS desktop layout */
    char *layout_path = NULL;
    if (asprintf(&layout_path, "%s/pieces/apps/gl_os/layouts/desktop.chtpm", project_root) != -1) {
        FILE *lf = fopen("pieces/display/layout_changed.txt", "a");
        if (lf) {
            fprintf(lf, "%s\n", layout_path);
            fclose(lf);
        }
        free(layout_path);
    }

    /* Touch frame changed to trigger render (GL-OS session ledger) */
    FILE *mf = fopen("pieces/apps/gl_os/ledger/frame_changed.txt", "a");
    if (mf) {
        fprintf(mf, "G\n");
        fclose(mf);
    }

    return 0;
}
