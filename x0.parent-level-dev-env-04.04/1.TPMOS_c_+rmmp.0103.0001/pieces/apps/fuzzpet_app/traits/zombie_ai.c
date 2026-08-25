#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <stdarg.h>
#include <time.h>

#include <dirent.h>
#include <sys/stat.h>

// TPM Zombie AI Trait (v1.4 - RECURSIVE PATHING)
// Responsibility: Calculate step toward target and signal renderer.

char *project_root = NULL;

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

void log_debug(const char* fmt, ...) {
    if (!project_root) return;
    char *path = NULL; asprintf(&path, "%s/pieces/apps/fuzzpet_app/manager/debug_log.txt", project_root);
    FILE *f = fopen(path, "a");
    if (f) {
        va_list args; va_start(args, fmt);
        time_t now = time(NULL);
        fprintf(f, "[%ld] [AI] ", now);
        vfprintf(f, fmt, args);
        fprintf(f, "\n");
        va_end(args);
        fclose(f);
    }
    free(path);
}

void resolve_paths() {
    const char* attempts[] = {"pieces/locations/location_kvp", "../pieces/locations/location_kvp", "../../pieces/locations/location_kvp", "../../../pieces/locations/location_kvp"};
    for (int i = 0; i < 4; i++) {
        FILE *kvp = fopen(attempts[i], "r");
        if (kvp) {
            char line[4096];
            while (fgets(line, sizeof(line), kvp)) {
                if (strncmp(line, "project_root=", 13) == 0) {
                    project_root = strdup(trim_str(line + 13));
                    fclose(kvp); return;
                }
            }
            fclose(kvp);
        }
    }
}

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
    if (!project_root) return -1;

    char current_project[64] = "template";
    char *mgr_state = NULL;
    asprintf(&mgr_state, "%s/pieces/apps/player_app/manager/state.txt", project_root);
    FILE *mf = fopen(mgr_state, "r");
    if (mf) {
        char line[1024];
        while (fgets(line, sizeof(line), mf)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                if (strcmp(trim_str(line), "project_id") == 0) strncpy(current_project, trim_str(eq + 1), 63);
            }
        }
        fclose(mf);
    }
    free(mgr_state);

    char path[4096];
    int found = 0;
    
    if (strcmp(current_project, "template") != 0) {
        /* Try recursive search in project pieces first */
        char pieces_root[4096];
        snprintf(pieces_root, sizeof(pieces_root), "%s/projects/%s/pieces", project_root, current_project);
        found = find_piece_state_recursive(pieces_root, piece_id, path, sizeof(path), 0);
    }
    
    if (!found) {
        /* Fallback to global world search */
        char pieces_root[4096];
        snprintf(pieces_root, sizeof(pieces_root), "%s/pieces", project_root);
        found = find_piece_state_recursive(pieces_root, piece_id, path, sizeof(path), 0);
    }

    if (!found && strcmp(piece_id, "xlector") == 0) {
        snprintf(path, sizeof(path), "%s/pieces/apps/fuzzpet_app/xlector/state.txt", project_root);
        if (access(path, F_OK) == 0) found = 1;
    }

    if (!found) return -1;
    
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char line[4096]; int val = -1;
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

int main(int argc, char *argv[]) {
    if (argc < 3) return 1;
    resolve_paths();
    if (!project_root) return 1;

    char *zombie_id = argv[1];
    char *target_id = argv[2];

    int zx = get_state_int(zombie_id, "pos_x");
    int zy = get_state_int(zombie_id, "pos_y");
    int tx = get_state_int(target_id, "pos_x");
    int ty = get_state_int(target_id, "pos_y");

    if (zx == -1 || tx == -1) return 1;

    int dx = (tx > zx) ? 1 : (tx < zx ? -1 : 0);
    int dy = (ty > zy) ? 1 : (ty < zy ? -1 : 0);

    int nx = zx, ny = zy;
    if (dx != 0) nx += dx; else if (dy != 0) ny += dy;

    if (nx < 1) nx = 1; if (nx > 18) nx = 18;
    if (ny < 1) ny = 1; if (ny > 8) ny = 8;

    /* ZOMBIE COLLISION: Don't move onto tiles with other entities */
    char *pieces[] = {"xlector", "pet_01", "pet_02", "fuzzpet", NULL};
    for (int i = 0; pieces[i] != NULL; i++) {
        int ox = get_state_int(pieces[i], "pos_x");
        int oy = get_state_int(pieces[i], "pos_y");
        if (ox == nx && oy == ny) {
            /* Tile occupied, try alternate direction */
            if (dx != 0) { nx = zx; ny = zy + dy; }
            else { ny = zy; nx = zx + dx; }
            /* If still blocked, don't move */
            if (ox == nx && oy == ny) { nx = zx; ny = zy; }
            break;
        }
    }

    char current_project[64] = "template";
    char *mgr_state = NULL;
    asprintf(&mgr_state, "%s/pieces/apps/player_app/manager/state.txt", project_root);
    FILE *mf = fopen(mgr_state, "r");
    if (mf) {
        char line[1024];
        while (fgets(line, sizeof(line), mf)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                if (strcmp(trim_str(line), "project_id") == 0) strncpy(current_project, trim_str(eq + 1), 63);
            }
        }
        fclose(mf);
    }
    free(mgr_state);

    /* Direct state.txt update (bypass piece_manager) */
    char zstate_path[4096];
    int found_z = 0;
    if (strcmp(current_project, "template") != 0) {
        char pieces_root[4096];
        snprintf(pieces_root, sizeof(pieces_root), "%s/projects/%s/pieces", project_root, current_project);
        found_z = find_piece_state_recursive(pieces_root, zombie_id, zstate_path, sizeof(zstate_path), 0);
    }
    if (!found_z) {
        char pieces_root[4096];
        snprintf(pieces_root, sizeof(pieces_root), "%s/pieces", project_root);
        found_z = find_piece_state_recursive(pieces_root, zombie_id, zstate_path, sizeof(zstate_path), 0);
    }

    if (found_z) {
        char lines[100][256];
        int lc = 0, fx = 0, fy = 0;
        FILE *zf = fopen(zstate_path, "r");
        if (zf) {
            while (fgets(lines[lc], sizeof(lines[0]), zf) && lc < 99) {
                if (strncmp(lines[lc], "pos_x=", 6) == 0) { snprintf(lines[lc], sizeof(lines[0]), "pos_x=%d\n", nx); fx = 1; }
                else if (strncmp(lines[lc], "pos_y=", 6) == 0) { snprintf(lines[lc], sizeof(lines[0]), "pos_y=%d\n", ny); fy = 1; }
                lc++;
            }
            fclose(zf);
        }
        if (!fx && lc < 100) snprintf(lines[lc++], sizeof(lines[0]), "pos_x=%d\n", nx);
        if (!fy && lc < 100) snprintf(lines[lc++], sizeof(lines[0]), "pos_y=%d\n", ny);
        zf = fopen(zstate_path, "w");
        if (zf) { for (int i = 0; i < lc; i++) fputs(lines[i], zf); fclose(zf); }
    }

    // PULSE: Ensure render triggers after zombie move
    char *pulse = NULL; asprintf(&pulse, "%s/pieces/display/frame_changed.txt", project_root);
    FILE *pf = fopen(pulse, "a"); if (pf) { fputc('Z', pf); fclose(pf); }
    free(pulse); free(project_root);

    return 0;
}
