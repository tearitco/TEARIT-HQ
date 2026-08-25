/*
 * Op: move_player
 * Usage: ./+x/move_player.+x <piece_id> <direction>
 * Args:
 *   piece_id  - The piece to move (e.g., "player", "selector")
 *   direction - One of: up, down, left, right
 * 
 * Behavior:
 *   1. Reads engine state for current project_id
 *   2. Reads piece state.txt for pos_x, pos_y, pos_z
 *   3. Calculates new position based on direction
 *   4. Checks collision (walls '#', rocks 'R') in project map
 *   5. Updates state.txt with new pos_x, pos_y
 *   6. Hits frame_changed.txt to trigger re-render
 *   7. Logs to master_ledger.txt
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
#define MAP_COLS 20
#define MAP_ROWS 10

char project_root[MAX_PATH] = ".";
char current_project[MAX_LINE] = "template";

/* Trim whitespace from string */
char* trim_str(char *str) {
    char *end;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

/* Resolve paths from location_kvp */
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
    
    /* Load current project_id from engine state */
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

/* Read integer state from piece */
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

/* Set integer state in piece */
void set_state_int(const char* piece_id, const char* key, int val) {
    char *path = NULL;
    if (asprintf(&path, "%s/projects/%s/pieces/%s/state.txt", project_root, current_project, piece_id) == -1) return;
    
    /* Read all lines, update the one we want */
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
    
    /* Write back */
    f = fopen(path, "w");
    if (f) {
        for (int i = 0; i < line_count; i++) {
            fputs(lines[i], f);
        }
        fclose(f);
    }
    free(path);
}

/* Check if tile is walkable */
int is_walkable(int x, int y, int z) {
    char *map_path = NULL;
    if (asprintf(&map_path, "%s/projects/%s/maps/map_0001_z%d.txt", project_root, current_project, z) == -1) return 1;
    
    if (access(map_path, F_OK) != 0) { free(map_path); return 1; }
    
    FILE *mf = fopen(map_path, "r");
    if (!mf) { free(map_path); return 1; }
    
    char line[MAX_LINE];
    int current_y = 0;
    int result = 1;
    while (fgets(line, sizeof(line), mf)) {
        if (current_y == y) {
            if (x < (int)strlen(line)) {
                char tile = line[x];
                if (tile == '#' || tile == 'R') result = 0;
            }
            break;
        }
        current_y++;
    }
    fclose(mf);
    free(map_path);
    return result;
}

/* Hit frame marker to trigger re-render */
void hit_frame_marker() {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/display/frame_changed.txt", project_root) != -1) {
        FILE *f = fopen(path, "a");
        if (f) {
            fprintf(f, "X\n");
            fclose(f);
        }
        free(path);
    }
}

/* Log to master ledger */
void log_event(const char* event, const char* piece, const char* details) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/master_ledger/master_ledger.txt", project_root) != -1) {
        FILE *f = fopen(path, "a");
        if (f) {
            time_t rawtime;
            struct tm *timeinfo;
            char timestamp[100];
            time(&rawtime);
            timeinfo = localtime(&rawtime);
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
        fprintf(stderr, "  direction: up, down, left, right\n");
        return 1;
    }
    
    resolve_paths();
    
    const char *piece_id = argv[1];
    const char *direction = argv[2];
    
    /* Read current position */
    int pos_x = get_state_int(piece_id, "pos_x");
    int pos_y = get_state_int(piece_id, "pos_y");
    int pos_z = get_state_int(piece_id, "pos_z");
    
    if (pos_x == -1 || pos_y == -1) {
        /* Fallback if state doesn't exist yet */
        pos_x = 5; pos_y = 5; pos_z = 0;
    }
    if (pos_z == -1) pos_z = 0;
    
    /* Calculate new position */
    int new_x = pos_x;
    int new_y = pos_y;
    
    if (strcmp(direction, "up") == 0) new_y--;
    else if (strcmp(direction, "down") == 0) new_y++;
    else if (strcmp(direction, "left") == 0) new_x--;
    else if (strcmp(direction, "right") == 0) new_x++;
    else {
        fprintf(stderr, "Error: Unknown direction '%s'\n", direction);
        return 1;
    }
    
    /* Bounds checking */
    if (new_x < 0) new_x = 0; if (new_x >= MAP_COLS) new_x = MAP_COLS - 1;
    if (new_y < 0) new_y = 0; if (new_y >= MAP_ROWS) new_y = MAP_ROWS - 1;
    
    /* Check collision (Skip for xlector) */
    if (strcmp(piece_id, "xlector") != 0 && !is_walkable(new_x, new_y, pos_z)) {
        log_event("MoveBlocked", piece_id, "collision");
        return 0;  /* Not an error, just can't move */
    }
    
    /* Update position */
    set_state_int(piece_id, "pos_x", new_x);
    set_state_int(piece_id, "pos_y", new_y);
    
    /* Log event */
    char details[512];
    snprintf(details, sizeof(details), "from (%d,%d) to (%d,%d) project=%s direction=%s", pos_x, pos_y, new_x, new_y, current_project, direction);
    log_event("MoveEntity", piece_id, details);
    
    return 0;
}
