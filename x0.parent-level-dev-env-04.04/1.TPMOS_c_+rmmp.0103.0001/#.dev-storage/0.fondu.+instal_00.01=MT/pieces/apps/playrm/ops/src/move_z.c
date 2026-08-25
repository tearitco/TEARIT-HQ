/*
 * Op: move_z
 * Usage: ./+x/move_z.+x <piece_id> <direction>
 * Args:
 *   piece_id  - The piece to move (e.g., "player", "selector")
 *   direction - One of: up, down
 * 
 * Behavior:
 *   1. Reads piece state.txt for pos_z
 *   2. Increments (up) or decrements (down) pos_z
 *   3. Updates state.txt with new pos_z
 *   4. Hits frame_changed.txt to trigger re-render
 *   5. Logs to master_ledger.txt
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>

#define MAX_PATH 4096
#define MAX_LINE 256

char project_root[MAX_PATH] = ".";
char current_project[MAX_LINE] = "template";

char* trim_str(char *str) {
    char *end;
    while(*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') str++;
    if(*str == 0) return str;
    end = str + strlen(str) - 1;
    while(end > str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end--;
    end[1] = '\0';
    return str;
}

void resolve_paths() {
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *k = trim_str(line);
                char *v = trim_str(eq + 1);
                if (strcmp(k, "project_root") == 0) snprintf(project_root, sizeof(project_root), "%s", v);
            }
        }
        fclose(kvp);
    }
    
    char *engine_state_path = NULL;
    if (asprintf(&engine_state_path, "%s/pieces/apps/player_app/manager/state.txt", project_root) != -1) {
        FILE *ef = fopen(engine_state_path, "r");
        if (ef) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), ef)) {
                char *eq = strchr(line, '=');
                if (eq) {
                    *eq = '\0';
                    if (strcmp(trim_str(line), "project_id") == 0) strncpy(current_project, trim_str(eq + 1), sizeof(current_project)-1);
                }
            }
            fclose(ef);
        }
        free(engine_state_path);
    }
}

int get_state_int(const char* piece_id, const char* key) {
    char *path = NULL;
    if (asprintf(&path, "%s/projects/%s/pieces/%s/state.txt", project_root, current_project, piece_id) == -1) return -1;
    
    FILE *f = fopen(path, "r");
    if (!f) { free(path); return -1; }
    
    char line[MAX_LINE];
    int val = -1;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            char *k = trim_str(line);
            char *v = trim_str(eq + 1);
            if (strcmp(k, key) == 0) {
                val = atoi(v);
                break;
            }
        }
    }
    fclose(f);
    free(path);
    return val;
}

void set_state_int(const char* piece_id, const char* key, int val) {
    char *path = NULL;
    if (asprintf(&path, "%s/projects/%s/pieces/%s/state.txt", project_root, current_project, piece_id) == -1) return;
    
    char lines[100][MAX_LINE];
    int line_count = 0;
    int found = 0;
    
    FILE *f = fopen(path, "r");
    if (f) {
        while (fgets(lines[line_count], sizeof(lines[0]), f) && line_count < 99) {
            char *eq = strchr(lines[line_count], '=');
            if (eq) {
                *eq = '\0';
                char *k = trim_str(lines[line_count]);
                if (strcmp(k, key) == 0) {
                    snprintf(lines[line_count], sizeof(lines[0]), "%s=%d\n", key, val);
                    found = 1;
                } else {
                    *eq = '=';
                }
            }
            line_count++;
        }
        fclose(f);
    }
    
    if (!found && line_count < 100) {
        snprintf(lines[line_count++], sizeof(lines[0]), "%s=%d\n", key, val);
    }
    
    f = fopen(path, "w");
    if (f) {
        for (int i = 0; i < line_count; i++) fputs(lines[i], f);
        fclose(f);
    }
    free(path);
}

void hit_frame_marker() {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/display/frame_changed.txt", project_root) != -1) {
        FILE *f = fopen(path, "a");
        if (f) { fprintf(f, "X\n"); fclose(f); }
        free(path);
    }
}

void log_event(const char* event, const char* piece, const char* details) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/master_ledger/master_ledger.txt", project_root) != -1) {
        FILE *f = fopen(path, "a");
        if (f) {
            time_t rawtime; struct tm *timeinfo; char timestamp[100];
            time(&rawtime); timeinfo = localtime(&rawtime);
            strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
            fprintf(f, "[%s] %s: %s | %s\n", timestamp, event, piece, details);
            fclose(f);
        }
        free(path);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <piece_id> <direction>\n", argv[0]);
        fprintf(stderr, "  direction: up, down\n");
        return 1;
    }
    
    resolve_paths();
    
    const char *piece_id = argv[1];
    const char *direction = argv[2];
    
    int pos_z = get_state_int(piece_id, "pos_z");
    if (pos_z == -1) pos_z = 0;
    
    int new_z = pos_z;
    if (strcmp(direction, "up") == 0) new_z++;
    else if (strcmp(direction, "down") == 0) new_z--;
    else {
        fprintf(stderr, "Error: Unknown direction '%s'\n", direction);
        return 1;
    }
    
    set_state_int(piece_id, "pos_z", new_z);
    
    char details[128];
    snprintf(details, sizeof(details), "from z=%d to z=%d", pos_z, new_z);
    log_event("MoveZ", piece_id, details);
    
    return 0;
}
