/*
 * Op: move_selector
 * Usage: ./move_selector.+x <direction> [project_id]
 * Args:
 *   direction - One of: up, down, left, right
 *   project_id - Optional, defaults to "template"
 * 
 * Moves the editor selector piece and updates state.
 * This is a SHARED OP - works for editor selector AND player selector.
 * OPTIMIZED: Uses PRISC_PROJECT_ID env var as highest priority
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

#define MAX_PATH 4096
#define MAX_CMD 16384
#define MAX_LINE 256
#define MAP_ROWS 10
#define MAP_COLS 20

char project_root[MAX_PATH] = ".";
char current_project[MAX_LINE] = "template";  // Default to template (system default)


char* trim_str(char *str) {
    char *end;
    if(!str) return str;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

void resolve_paths() {
    // PRIORITY 1: Environment variable from PAL module (highest priority)
    char *env_project = getenv("PRISC_PROJECT_ID");
    if (env_project && strlen(env_project) > 0) {
        strncpy(current_project, env_project, sizeof(current_project)-1);
        return;  // Don't override with file values
    }
    
    // PRIORITY 2: location_kvp for project_root
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (!kvp) kvp = fopen("../../../locations/location_kvp", "r");
    if (!kvp) kvp = fopen("../../../../locations/location_kvp", "r");
    
    if (kvp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *k = trim_str(line);
                char *v = trim_str(eq + 1);
                if (strcmp(k, "project_root") == 0) strncpy(project_root, v, MAX_PATH-1);
            }
        }
        fclose(kvp);
    }
    
    // PRIORITY 3: Manager state for project_id (fallback only)
    char *p_mgr_path = NULL;
    if (asprintf(&p_mgr_path, "%s/pieces/apps/player_app/manager/state.txt", project_root) != -1) {
        FILE *f = fopen(p_mgr_path, "r");
        if (f) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), f)) {
                char *eq = strchr(line, '=');
                if (eq) {
                    *eq = '\0';
                    char *k = trim_str(line);
                    char *v = trim_str(eq + 1);
                    if (strcmp(k, "project_id") == 0) {
                        // Only use if current_project is still default
                        if (strcmp(current_project, "template") == 0) {
                            strncpy(current_project, v, sizeof(current_project)-1);
                        }
                    }
                }
            }
            fclose(f);
        }
        free(p_mgr_path);
    }
}

int get_selector_pos(const char *state_file, int *x, int *y) {
    FILE *f = fopen(state_file, "r");
    if (!f) return -1;
    
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            char *k = trim_str(line);
            char *v = trim_str(eq + 1);
            if (strcmp(k, "pos_x") == 0) *x = atoi(v);
            else if (strcmp(k, "pos_y") == 0) *y = atoi(v);
        }
    }
    fclose(f);
    return 0;
}

void hit_frame_marker() {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/display/frame_changed.txt", project_root) != -1) {
        FILE *f = fopen(path, "a");
        if (f) { fprintf(f, "X\n"); fclose(f); }
        free(path);
    }
}

void log_event(const char* event, const char* details) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/master_ledger/master_ledger.txt", project_root) != -1) {
        FILE *f = fopen(path, "a");
        if (f) {
            time_t now = time(NULL);
            fprintf(f, "[%ld] %s: %s\n", now, event, details);
            fclose(f);
        }
        free(path);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <direction> [project_id]\n", argv[0]);
        return 1;
    }
    
    // PRIORITY 0: Command line argument (before resolve_paths)
    if (argc > 2) {
        strncpy(current_project, argv[2], sizeof(current_project)-1);
    }
    
    resolve_paths();
    
    // DEBUG: Print what we're doing
    fprintf(stderr, "[move_selector] direction=%s project=%s root=%s\n", argv[1], current_project, project_root);
    
    const char *direction = argv[1];
    // argv[2] already handled above
    
    // Resolve selector state path using project_root
    char *selector_state = NULL;
    if (asprintf(&selector_state, "%s/projects/%s/pieces/selector/state.txt", project_root, current_project) == -1) return 1;
    
    fprintf(stderr, "[move_selector] Writing to: %s\n", selector_state);
    
    int pos_x = 5, pos_y = 5;
    get_selector_pos(selector_state, &pos_x, &pos_y);
    
    int new_x = pos_x;
    int new_y = pos_y;
    
    if (strcmp(direction, "up") == 0) new_y--;
    else if (strcmp(direction, "down") == 0) new_y++;
    else if (strcmp(direction, "left") == 0) new_x--;
    else if (strcmp(direction, "right") == 0) new_x++;
    
    if (new_x < 0) new_x = 0;
    if (new_x >= MAP_COLS) new_x = MAP_COLS - 1;
    if (new_y < 0) new_y = 0;
    if (new_y >= MAP_ROWS) new_y = MAP_ROWS - 1;
    
    FILE *f = fopen(selector_state, "w");
    if (f) {
        fprintf(f, "pos_x=%d\npos_y=%d\npos_z=0\non_map=1\ntype=selector\n", new_x, new_y);
        fclose(f);
    }
    
    free(selector_state);
    
    char *details = NULL;
    if (asprintf(&details, "selector to (%d,%d) project=%s", new_x, new_y, current_project) != -1) {
        log_event("MoveSelector", details);
        if (details) free(details);
    }
    
    hit_frame_marker();
    printf("Selector moved to (%d,%d)\n", new_x, new_y);
    return 0;
}
