#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif

#define MAX_LINE 4096

static char project_root[MAX_PATH] = ".";

static int root_has_anchors(const char *root) {
    char pieces_path[MAX_PATH];
    char projects_path[MAX_PATH];

    snprintf(pieces_path, sizeof(pieces_path), "%s/pieces", root);
    snprintf(projects_path, sizeof(projects_path), "%s/projects", root);
    return access(pieces_path, F_OK) == 0 && access(projects_path, F_OK) == 0;
}

static void resolve_root(void) {
    FILE *kvp = NULL;

    if (getcwd(project_root, sizeof(project_root)) && root_has_anchors(project_root)) {
        return;
    }

    kvp = fopen("pieces/locations/location_kvp", "r");
    if (!kvp) return;

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), kvp)) {
        if (strncmp(line, "project_root=", 13) == 0) {
            char *v = line + 13;
            v[strcspn(v, "\n\r")] = 0;
            if (strlen(v) > 0 && root_has_anchors(v)) {
                strncpy(project_root, v, sizeof(project_root) - 1);
                project_root[sizeof(project_root) - 1] = '\0';
            }
            break;
        }
    }
    fclose(kvp);
}

static void get_current_layout_name(char *out, size_t out_sz) {
    char layout_path[MAX_PATH];
    snprintf(layout_path, sizeof(layout_path), "%s/pieces/display/current_layout.txt", project_root);

    FILE *f = fopen(layout_path, "r");
    if (!f) {
        strncpy(out, "desktop.chtpm", out_sz - 1);
        out[out_sz - 1] = '\0';
        return;
    }

    if (!fgets(out, (int)out_sz, f)) {
        strncpy(out, "desktop.chtpm", out_sz - 1);
        out[out_sz - 1] = '\0';
    } else {
        out[strcspn(out, "\n\r")] = '\0';
    }
    fclose(f);
}

int main(void) {
    resolve_root();

    char state_path[MAX_PATH];
    char audit_path[MAX_PATH];
    char view_path[MAX_PATH];
    char active_layout[128];
    snprintf(state_path, sizeof(state_path), "%s/pieces/apps/gl_os/session/state.txt", project_root);
    snprintf(audit_path, sizeof(audit_path), "%s/pieces/apps/gl_os/session/audit_frame.txt", project_root);
    snprintf(view_path, sizeof(view_path), "%s/pieces/apps/gl_os/session/view.txt", project_root);

    get_current_layout_name(active_layout, sizeof(active_layout));

    int window_count = 0;
    int active_window_id = 0;
    int selected_index = 0;
    char menu_options[4096] = "";

    FILE *state = fopen(state_path, "r");
    if (state) {
        char line[MAX_LINE];
        char *current_val = NULL;
        int max_len = 0;
        while (fgets(line, sizeof(line), state)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *key = line;
                char *value = eq + 1;
                if (strcmp(key, "window_count") == 0) { window_count = atoi(value); current_val = NULL; }
                else if (strcmp(key, "active_window_id") == 0) { active_window_id = atoi(value); current_val = NULL; }
                else if (strcmp(key, "selected_index") == 0) { selected_index = atoi(value); current_val = NULL; }
                else if (strcmp(key, "menu_options") == 0) { strncpy(menu_options, value, sizeof(menu_options) - 1); current_val = menu_options; max_len = sizeof(menu_options) - 1; }
                else current_val = NULL;
            } else if (current_val) {
                strncat(current_val, line, max_len - strlen(current_val));
            }
        }
        fclose(state);
    }

    FILE *view = fopen(view_path, "r");
    if (view) fclose(view);

    time_t rawtime;
    struct tm *timeinfo;
    char timestamp[100];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);

    FILE *out = fopen(audit_path, "w");
    if (!out) return 1;

    fprintf(out, "GL_OS_AUDIT_FRAME\n");
    fprintf(out, "timestamp=%s\n", timestamp);
    fprintf(out, "project_id=gl-os\n");
    fprintf(out, "active_layout=%s\n", active_layout);
    fprintf(out, "window_count=%d\n", window_count);
    fprintf(out, "active_window_id=%d\n", active_window_id);
    fprintf(out, "selected_index=%d\n", selected_index);
    fprintf(out, "active_menu_options=\n%s", menu_options);
    fprintf(out, "source=session/state.txt\n");
    fprintf(out, "source=session/view.txt\n");

    fclose(out);
    return 0;
}
