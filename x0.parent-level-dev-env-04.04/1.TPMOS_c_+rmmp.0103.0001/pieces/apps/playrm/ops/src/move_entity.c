/*
 * Op: move_entity
 * Usage: ./+x/move_entity.+x <piece_id> <direction>
 * 
 * Refactored generic movement with Fuzzpet interaction support.
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

#define MAX_PATH 8192
#define MAX_CMD 16384
#define MAX_LINE 1024
#define MAP_COLS 20
#define MAP_ROWS 10

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
    char *env_root = getenv("PRISC_PROJECT_ROOT");
    char *env_project = getenv("PRISC_PROJECT_ID");
    
    if (env_root && strlen(env_root) > 0) strncpy(project_root, env_root, MAX_PATH-1);
    if (env_project && strlen(env_project) > 0) strncpy(current_project, env_project, sizeof(current_project)-1);
    
    if (strlen(project_root) == 0 || strcmp(project_root, ".") == 0) {
        FILE *kvp = fopen("pieces/locations/location_kvp", "r");
        if (!kvp) kvp = fopen("../../../locations/location_kvp", "r");
        if (kvp) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), kvp)) {
                char *eq = strchr(line, '=');
                if (eq) {
                    *eq = '\0';
                    if (strcmp(trim_str(line), "project_root") == 0) strncpy(project_root, trim_str(eq + 1), MAX_PATH-1);
                }
            }
            fclose(kvp);
        }
    }
    
    if (strlen(current_project) == 0 || strcmp(current_project, "template") == 0) {
        char *path = NULL;
        asprintf(&path, "%s/pieces/apps/player_app/manager/state.txt", project_root);
        FILE *f = fopen(path, "r");
        if (f) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), f)) {
                char *eq = strchr(line, '=');
                if (eq) {
                    *eq = '\0';
                    if (strcmp(trim_str(line), "project_id") == 0) strncpy(current_project, trim_str(eq + 1), sizeof(current_project)-1);
                }
            }
            fclose(f);
        }
        free(path);
    }
    if (strlen(current_project) == 0) strncpy(current_project, "template", sizeof(current_project)-1);
}


#include <dirent.h>
#include <sys/stat.h>

int find_piece_state_recursive(const char* dir_path, const char* piece_id, char* out_path, size_t out_size, int depth) {
    DIR *dir;
    struct dirent *entry;

    if (!dir_path || !piece_id || !out_path || depth > 8) return 0;

    dir = opendir(dir_path);
    if (!dir) return 0;

    while ((entry = readdir(dir)) != NULL) {
        char child[4096];
        struct stat st;

        if (entry->d_name[0] == '.') continue;
        snprintf(child, sizeof(child), "%s/%s", dir_path, entry->d_name);
        if (stat(child, &st) != 0 || !S_ISDIR(st.st_mode)) continue;

        if (strcmp(entry->d_name, piece_id) == 0) {
            char state_path[4096];
            snprintf(state_path, sizeof(state_path), "%s/state.txt", child);
            if (access(state_path, F_OK) == 0) {
                strncpy(out_path, state_path, out_size - 1);
                out_path[out_size - 1] = '\0';
                closedir(dir);
                return 1;
            }
        }

        if (find_piece_state_recursive(child, piece_id, out_path, out_size, depth + 1)) {
            closedir(dir);
            return 1;
        }
    }

    closedir(dir);
    return 0;
}

int get_state_int(const char* piece_id, const char* key) {
    char path[4096];
    int found = 0;

    /* 1. Try project-specific recursive search */
    char pieces_root[4096];
    snprintf(pieces_root, sizeof(pieces_root), "%s/projects/%s/pieces", project_root, current_project);
    found = find_piece_state_recursive(pieces_root, piece_id, path, sizeof(path), 0);

    /* 2. Try global pieces recursive search */
    if (!found) {
        snprintf(pieces_root, sizeof(pieces_root), "%s/pieces", project_root);
        found = find_piece_state_recursive(pieces_root, piece_id, path, sizeof(path), 0);
    }

    if (!found) return -1;

    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char line[MAX_LINE];
    int val = -1;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(trim_str(line), key) == 0) { val = atoi(trim_str(eq + 1)); break; }
        }
    }
    fclose(f);
    return val;
}

void set_state_int(const char* piece_id, const char* key, int val) {
    char path[4096];
    int found_path = 0;

    /* 1. Try project-specific recursive search */
    char pieces_root[4096];
    snprintf(pieces_root, sizeof(pieces_root), "%s/projects/%s/pieces", project_root, current_project);
    found_path = find_piece_state_recursive(pieces_root, piece_id, path, sizeof(path), 0);

    /* 2. Try global pieces recursive search */
    if (!found_path) {
        snprintf(pieces_root, sizeof(pieces_root), "%s/pieces", project_root);
        found_path = find_piece_state_recursive(pieces_root, piece_id, path, sizeof(path), 0);
    }

    if (!found_path) return;

    char lines[100][MAX_LINE];
    int line_count = 0, found_key = 0;
    FILE *f = fopen(path, "r");
    if (f) {
        while (fgets(lines[line_count], sizeof(lines[0]), f) && line_count < 99) {
            char *eq = strchr(lines[line_count], '=');
            if (eq) {
                *eq = '\0';
                if (strcmp(trim_str(lines[line_count]), key) == 0) {
                    snprintf(lines[line_count], sizeof(lines[0]), "%s=%d\n", key, val);
                    found_key = 1;
                } else *eq = '=';
            }
            line_count++;
        }
        fclose(f);
    }
    if (!found_key && line_count < 100) snprintf(lines[line_count++], sizeof(lines[0]), "%s=%d\n", key, val);
    f = fopen(path, "w");
    if (f) {
        for (int i = 0; i < line_count; i++) fputs(lines[i], f);
        fclose(f);
    }
}

char get_tile_at(int x, int y) {
    /* Safety check for negative coordinates (Infinite World Support) */
    if (x < 0 || y < 0) return '.';

    char *pdl_path = NULL;
    char map_base[MAX_LINE] = "map_01";
    if (asprintf(&pdl_path, "%s/projects/%s/project.pdl", project_root, current_project) != -1) {
        FILE *pf = fopen(pdl_path, "r");
        if (pf) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), pf)) {
                if (strstr(line, "starting_map")) {
                    char *pipe = strrchr(line, '|');
                    if (pipe) {
                        char *v = trim_str(pipe + 1);
                        strncpy(map_base, v, MAX_LINE - 1);
                    }
                }
            }
            fclose(pf);
        }
        free(pdl_path);
    }

    char *map_path = NULL;
    FILE *mf = NULL;
    
    // Try map_base_z0.txt
    if (asprintf(&map_path, "%s/projects/%s/maps/%s_z0.txt", project_root, current_project, map_base) != -1) {
        mf = fopen(map_path, "r");
        if (!mf) { free(map_path); map_path = NULL; }
    }
    
    // Try map_base.txt
    if (!mf && asprintf(&map_path, "%s/projects/%s/maps/%s.txt", project_root, current_project, map_base) != -1) {
        mf = fopen(map_path, "r");
        if (!mf) { free(map_path); map_path = NULL; }
    }
    
    // Try map_base_z (from current_z)
    if (!mf && asprintf(&map_path, "%s/projects/%s/maps/%s_z%d.txt", project_root, current_project, map_base, 0) != -1) {
        mf = fopen(map_path, "r");
        if (!mf) { free(map_path); map_path = NULL; }
    }

    if (!mf) return '.'; // Default to empty if no map found, instead of blocking
    
    char line[MAX_LINE];
    int cur_y = 0;
    char tile = '.';
    while (fgets(line, sizeof(line), mf)) {
        if (cur_y == y) {
            if (x < (int)strlen(line)) {
                tile = line[x];
                if (tile == '\n' || tile == '\r') tile = ' ';
            }
            break;
        }
        cur_y++;
    }
    fclose(mf); if (map_path) free(map_path);
    return tile;
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
            time_t now = time(NULL);
            fprintf(f, "[%ld] %s: %s | %s\n", now, event, piece, details);
            fclose(f);
        }
        free(path);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) return 1;

    // Support optional project_id as 4th argument (argv[3], overrides manager state)
    int explicit_project = 0;
    if (argc >= 4) {
        strncpy(current_project, argv[3], MAX_LINE - 1);
        explicit_project = 1;
    }

    resolve_paths();

    /* If explicit project was passed, restore it (resolve_paths may have overwritten it) */
    if (explicit_project) {
        strncpy(current_project, argv[3], MAX_LINE - 1);
    }
    const char *id = argv[1];
    const char *dir = argv[2];
    
    int x = get_state_int(id, "pos_x");
    int y = get_state_int(id, "pos_y");
    int nx = x, ny = y;
    
    if (strcmp(dir, "up") == 0 || strcmp(dir, "w") == 0 || strcmp(dir, "W") == 0) ny--;
    else if (strcmp(dir, "down") == 0 || strcmp(dir, "s") == 0 || strcmp(dir, "S") == 0) ny++;
    else if (strcmp(dir, "left") == 0 || strcmp(dir, "a") == 0 || strcmp(dir, "A") == 0) nx--;
    else if (strcmp(dir, "right") == 0 || strcmp(dir, "d") == 0 || strcmp(dir, "D") == 0) nx++;
    
    /* BOUNDS CHECK REMOVED FOR INFINITE WORLD */
    // if (nx < 0 || nx >= MAP_COLS || ny < 0 || ny >= MAP_ROWS) return 0;
    
    char tile = get_tile_at(nx, ny);
    /* Xlector passes through obstacles when not selecting an entity */
    if (strcmp(id, "xlector") != 0) {
        if (tile == '#' || tile == 'R') {
            log_event("MoveBlocked", id, "collision");
            return 0;
        }
    }
    
    if (tile == 'T') {
        int hap = get_state_int(id, "happiness");
        if (hap != -1) set_state_int(id, "happiness", (hap < 90) ? hap + 10 : 100);
        log_event("TreasureFound", id, "");
    }
    
    set_state_int(id, "pos_x", nx);
    set_state_int(id, "pos_y", ny);
    
    int enr = get_state_int(id, "energy");
    if (enr != -1) set_state_int(id, "energy", (enr > 0) ? enr - 1 : 0);
    
    char details[256];
    snprintf(details, sizeof(details), "from (%d,%d) to (%d,%d)", x, y, nx, ny);
    log_event("EntityMoved", id, details);
    
    hit_frame_marker();
    return 0;
}
