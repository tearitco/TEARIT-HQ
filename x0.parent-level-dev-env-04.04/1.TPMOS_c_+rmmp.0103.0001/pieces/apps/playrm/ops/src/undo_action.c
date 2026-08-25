/*
 * Op: undo_action
 * Usage: ./undo_action.+x [project_id]
 * 
 * Reads the last entry from editor_history.ledger,
 * executes the inverse operation, and pops the line.
 * 
 * Ledger Format:
 *   PLACE_TILE|map|x|y|old_tile|new_tile
 *   DELETE_PIECE|type|x|y
 *   CREATE_PIECE|type|x|y
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#define MAX_PATH 4096
#define MAX_CMD 16384
#define MAX_LINE 512

char project_root[MAX_PATH] = ".";
char current_project[MAX_LINE] = "template";

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
    
    // Load from editor state
    FILE *state = fopen("pieces/apps/editor/state.txt", "r");
    if (state) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), state)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *k = trim_str(line);
                char *v = trim_str(eq + 1);
                if (strcmp(k, "project_id") == 0) snprintf(current_project, sizeof(current_project), "%s", v);
            }
        }
        fclose(state);
    }
}

int delete_file(const char *path) {
    return remove(path);
}

int delete_directory_recursive(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) return 0;
    
    struct dirent *entry;
    char entry_path[MAX_CMD];
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        
        snprintf(entry_path, sizeof(entry_path), "%s/%s", path, entry->d_name);
        
        struct stat st;
        if (stat(entry_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                delete_directory_recursive(entry_path);
            } else {
                remove(entry_path);
            }
        }
    }
    
    closedir(dir);
    rmdir(path);
    return 1;
}

void place_tile(const char *map_file, int x, int y, char tile) {
    char map_path[MAX_CMD];
    snprintf(map_path, sizeof(map_path),
             "%s/projects/%s/maps/%s", project_root, current_project, map_file);
    
    FILE *f = fopen(map_path, "r");
    if (!f) return;
    
    char lines[30][MAX_LINE];
    int count = 0;
    while (fgets(lines[count], sizeof(lines[0]), f) && count < 29) {
        lines[count][strcspn(lines[count], "\n\r")] = 0;
        count++;
    }
    fclose(f);
    
    if (y >= 0 && y < count && x >= 0 && x < (int)strlen(lines[y])) {
        lines[y][x] = tile;
        
        f = fopen(map_path, "w");
        if (f) {
            for (int i = 0; i < count; i++) {
                fprintf(f, "%s\n", lines[i]);
            }
            fclose(f);
        }
    }
}

void hit_frame_marker() {
    char path[MAX_CMD];
    snprintf(path, sizeof(path), "%s/pieces/display/frame_changed.txt", project_root);
    FILE *f = fopen(path, "a");
    if (f) { fprintf(f, "X\n"); fclose(f); }
}

void hit_editor_view_marker() {
    char path[MAX_CMD];
    snprintf(path, sizeof(path), "%s/pieces/apps/editor/view_changed.txt", project_root);
    FILE *f = fopen(path, "a");
    if (f) { fprintf(f, "X\n"); fclose(f); }
}

void log_event(const char* event, const char* details) {
    char path[MAX_CMD];
    snprintf(path, sizeof(path), "%s/pieces/master_ledger/master_ledger.txt", project_root);
    FILE *f = fopen(path, "a");
    if (f) {
        fprintf(f, "Undo: %s\n", details);
        fclose(f);
    }
}

int main(int argc, char *argv[]) {
    resolve_paths();
    
    if (argc > 1) {
        snprintf(current_project, sizeof(current_project), "%s", argv[1]);
    }
    
    // Read ledger
    char ledger_path[MAX_CMD];
    snprintf(ledger_path, sizeof(ledger_path),
             "%s/projects/%s/config/editor_history.ledger", project_root, current_project);
    
    FILE *f = fopen(ledger_path, "r");
    if (!f) {
        printf("No undo history available\n");
        return 0;
    }
    
    // Read all lines
    char lines[1000][MAX_LINE];
    int count = 0;
    while (fgets(lines[count], sizeof(lines[0]), f) && count < 999) {
        lines[count][strcspn(lines[count], "\n\r")] = 0;
        count++;
    }
    fclose(f);
    
    if (count == 0) {
        printf("No undo history available\n");
        return 0;
    }
    
    // Get last entry
    char *last_entry = lines[count - 1];
    printf("Undoing: %s\n", last_entry);
    
    // Parse and execute inverse
    char action[64], arg1[64], arg2[64], arg3[64], arg4[64], arg5[64];
    int x, y;
    
    int parsed = sscanf(last_entry, "%63[^|]|%63[^|]|%d|%63[^|]|%63[^|]|%63[^|]",
                        action, arg1, &x, arg2, arg3, arg4);
    
    if (strcmp(action, "PLACE_TILE") == 0 && parsed >= 5) {
        // Inverse: place old tile back
        place_tile(arg1, x, atoi(arg2), arg3[0]);
        printf("Undone: Placed '%c' at (%d,%d) on %s\n", arg3[0], x, atoi(arg2), arg1);
        log_event("UndoPlaceTile", arg1);
    }
    else if (strcmp(action, "DELETE_PIECE") == 0 && parsed >= 3) {
        // Inverse: Cannot restore deleted piece (would need backup)
        printf("Cannot undo piece deletion (no backup)\n");
        log_event("UndoDeletePiece", "Cannot restore");
    }
    else if (strcmp(action, "CREATE_PIECE") == 0 && parsed >= 3) {
        // Inverse: Delete the created piece
        char piece_path[MAX_CMD];
        snprintf(piece_path, sizeof(piece_path),
                 "%s/projects/%s/pieces/%s_%d_%d",
                 project_root, current_project, arg1, x, atoi(arg2));
        delete_directory_recursive(piece_path);
        printf("Undone: Deleted piece %s_%d_%d\n", arg1, x, atoi(arg2));
        log_event("UndoCreatePiece", arg1);
    }
    else {
        printf("Unknown action: %s\n", action);
    }
    
    // Remove last line from ledger
    f = fopen(ledger_path, "w");
    if (f) {
        for (int i = 0; i < count - 1; i++) {
            fprintf(f, "%s\n", lines[i]);
        }
        fclose(f);
    }
    
    hit_frame_marker();
    hit_editor_view_marker();
    
    return 0;
}
