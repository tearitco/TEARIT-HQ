/*
 * Op: create_piece
 * Usage: ./create_piece.+x <type> <x> <y> [project_id] [--world <world_id>] [--map <map_id>]
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <errno.h>

/* Type → icon mapping (canonical: see pieces/os/type_registry.pdl) */
#define TPM_OPS_ROOT "pieces/apps/playrm/ops/+x"
static const char* icon_from_type(const char *t) {
    if (!t) return "?";
    if (strcmp(t, "player") == 0 || strcmp(t, "@") == 0) return "@";
    if (strcmp(t, "npc") == 0 || strcmp(t, "&") == 0) return "&";
    if (strcmp(t, "zombie") == 0 || strcmp(t, "Z") == 0) return "Z";
    if (strcmp(t, "chest") == 0 || strcmp(t, "T") == 0) return "T";
    if (strcmp(t, "item") == 0 || strcmp(t, "*") == 0) return "*";
    if (strcmp(t, "selector") == 0 || strcmp(t, "X") == 0) return "X";
    return "?";
}

#ifdef _WIN32
    #include <direct.h>
    #define MKDIR(path, mode) _mkdir(path)
#else
    #define MKDIR(path, mode) mkdir(path, mode)
#endif

/* MAX_PATH per compile clean standards - use ifndef to avoid Windows redefinition */
#ifndef MAX_PATH
#define MAX_PATH 4096
#endif
#define MAX_CMD 16384
#define MAX_LINE 256

char project_root[MAX_PATH] = ".";
char current_project[MAX_LINE] = "template";
char target_world[MAX_LINE] = "";
char target_map[MAX_LINE] = "";

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
                char *k = trim_str(line), *v = trim_str(eq + 1);
                if (strcmp(k, "project_root") == 0) snprintf(project_root, sizeof(project_root), "%s", v);
            }
        }
        fclose(kvp);
    }
}

const char* get_piece_id_from_type(const char *type) {
    if (!type) return "entity";
    if (strcmp(type, "player") == 0 || strcmp(type, "@") == 0) return "player";
    if (strcmp(type, "npc") == 0 || strcmp(type, "&") == 0) return "npc";
    if (strcmp(type, "zombie") == 0 || strcmp(type, "Z") == 0) return "zombie";
    if (strcmp(type, "chest") == 0 || strcmp(type, "T") == 0) return "chest";
    if (strcmp(type, "item") == 0 || strcmp(type, "*") == 0) return "item";
    if (strcmp(type, "selector") == 0 || strcmp(type, "X") == 0) return "selector";
    return "entity";
}

const char* get_icon_from_type(const char *type) {
    return icon_from_type(type);
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <type> <x> <y> [project_id] [--world <world_id>] [--map <map_id>]\n", argv[0]);
        return 1;
    }
    
    resolve_paths();
    const char *type = argv[1];
    int x = atoi(argv[2]);
    int y = atoi(argv[3]);
    int project_set = 0;
    for (int i = 4; i < argc; i++) {
        if (strcmp(argv[i], "--world") == 0 && i + 1 < argc) {
            snprintf(target_world, sizeof(target_world), "%s", argv[++i]);
        } else if (strcmp(argv[i], "--map") == 0 && i + 1 < argc) {
            snprintf(target_map, sizeof(target_map), "%s", argv[++i]);
        } else if (!project_set && argv[i][0] != '-') {
            snprintf(current_project, sizeof(current_project), "%s", argv[i]);
            project_set = 1;
        }
    }

    if (!project_set) {
        // Fallback: Read project_id from manager state
        char *state_path = NULL;
        if (asprintf(&state_path, "%s/pieces/apps/player_app/manager/state.txt", project_root) != -1) {
            FILE *msf = fopen(state_path, "r");
            if (msf) {
                char line[MAX_LINE];
                while (fgets(line, sizeof(line), msf)) {
                    char *eq = strchr(line, '=');
                    if (eq) { *eq = '\0'; if (strcmp(trim_str(line), "project_id") == 0) snprintf(current_project, sizeof(current_project), "%s", trim_str(eq + 1)); }
                }
                fclose(msf);
            }
            free(state_path);
        }
    }
    
    /* FIX: Read current_map from manager state for map_id association */
    char current_map[MAX_LINE] = "";
    char *mgr_state_path = NULL;
    if (asprintf(&mgr_state_path, "%s/pieces/apps/player_app/manager/state.txt", project_root) != -1) {
        FILE *msf = fopen(mgr_state_path, "r");
        if (msf) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), msf)) {
                char *eq = strchr(line, '=');
                if (eq) {
                    *eq = '\0';
                    if (strcmp(trim_str(line), "current_map") == 0) {
                        snprintf(current_map, sizeof(current_map), "%s", trim_str(eq + 1));
                    }
                }
            }
            fclose(msf);
        }
        free(mgr_state_path);
    }
    
    const char *piece_id = get_piece_id_from_type(type);
    const char *icon = get_icon_from_type(type);
    
    char *piece_dir = NULL;
    if (strlen(target_world) > 0 && strlen(target_map) > 0) {
        char *world_dir = NULL;
        char *map_dir = NULL;
        if (asprintf(&world_dir, "%s/projects/%s/pieces/%s", project_root, current_project, target_world) == -1) return 1;
        if (MKDIR(world_dir, 0755) != 0 && errno != EEXIST) {
            fprintf(stderr, "Error: Cannot create world directory %s\n", world_dir);
            free(world_dir);
            return 1;
        }
        if (asprintf(&map_dir, "%s/%s", world_dir, target_map) == -1) { free(world_dir); return 1; }
        if (MKDIR(map_dir, 0755) != 0 && errno != EEXIST) {
            fprintf(stderr, "Error: Cannot create map directory %s\n", map_dir);
            free(world_dir); free(map_dir);
            return 1;
        }
        if (asprintf(&piece_dir, "%s/%s_%d_%d", map_dir, piece_id, x, y) == -1) {
            free(world_dir); free(map_dir);
            return 1;
        }
        free(world_dir);
        free(map_dir);
    } else {
        if (asprintf(&piece_dir, "%s/projects/%s/pieces/%s_%d_%d", project_root, current_project, piece_id, x, y) == -1) return 1;
    }
    
    if (MKDIR(piece_dir, 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "Error: Cannot create directory %s\n", piece_dir);
        free(piece_dir); return 1;
    }
    
    char *state_path = NULL;
    if (asprintf(&state_path, "%s/state.txt", piece_dir) != -1) {
        FILE *f = fopen(state_path, "w");
        if (f) {
            /* FIX: Include map_id if current_map is set */
            if (strlen(current_map) > 0) {
                fprintf(f, "name=%s_%d_%d\ntype=%s\npos_x=%d\npos_y=%d\npos_z=0\non_map=1\nmap_id=%s\nicon=%s\n",
                        piece_id, x, y, piece_id, x, y, current_map, icon);
            } else {
                fprintf(f, "name=%s_%d_%d\ntype=%s\npos_x=%d\npos_y=%d\npos_z=0\non_map=1\nicon=%s\n",
                        piece_id, x, y, piece_id, x, y, icon);
            }
            if (strcmp(piece_id, "npc") == 0) fprintf(f, "behavior=passive\ndialogue=Hello!\n");
            else if (strcmp(piece_id, "zombie") == 0) fprintf(f, "behavior=aggressive\nspeed=1\n");
            fclose(f);
        }
        free(state_path);
    }

    char *pdl_path = NULL;
    if (asprintf(&pdl_path, "%s/piece.pdl", piece_dir) != -1) {
        FILE *f = fopen(pdl_path, "w");
        if (f) {
            fprintf(f, "# Piece: %s_%d_%d\n", piece_id, x, y);
            fprintf(f, "META         | piece_id           | %s_%d_%d\n", piece_id, x, y);
            fprintf(f, "STATE        | pos_x              | %d\n", x);
            fprintf(f, "STATE        | pos_y              | %d\n", y);
            fprintf(f, "STATE        | pos_z              | 0\n");
            fprintf(f, "STATE        | on_map             | 1\n");
            fprintf(f, "STATE        | type               | %s\n", piece_id);
            if (strlen(current_map) > 0) {
                fprintf(f, "STATE        | map_id             | %s\n", current_map);
            }
            
            if (strcmp(piece_id, "npc") == 0) fprintf(f, "METHOD       | on_interact        | ./%s/interact.+x %s_%d_%d\n", TPM_OPS_ROOT, piece_id, x, y);
            else if (strcmp(piece_id, "zombie") == 0) fprintf(f, "METHOD       | on_turn            | ./%s/zombie_ai.+x %s_%d_%d\n", TPM_OPS_ROOT, piece_id, x, y);
            fclose(f);
        }
        free(pdl_path);
    }

    // Log to ledger
    char ledger_path[MAX_CMD];
    snprintf(ledger_path, sizeof(ledger_path), "%s/projects/%s/config/editor_history.ledger", project_root, current_project);
    FILE *ledger = fopen(ledger_path, "a");
    if (ledger) {
        fprintf(ledger, "CREATE_PIECE|%s|%d|%d\n", piece_id, x, y);
        fclose(ledger);
    }
    
    free(piece_dir);
    printf("Created piece: %s_%d_%d\n", piece_id, x, y);
    return 0;
}
