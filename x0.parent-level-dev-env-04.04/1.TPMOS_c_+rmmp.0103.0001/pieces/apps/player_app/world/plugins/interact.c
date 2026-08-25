#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>

// interact.c - Engine Operation (v2.0 - PRISC VM INTEGRATION)
// Responsibility: Trigger sovereign piece scripts using Prisc-Xpanse.

#define MAX_PATH 16384

char* trim_str(char *str) {
    char *end;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

void get_state_str(const char* proj_id, const char* piece_id, const char* key, char* out) {
    char path[MAX_PATH];
    snprintf(path, MAX_PATH, "./projects/%s/pieces/%s/state.txt", proj_id, piece_id);
    FILE *f = fopen(path, "r");
    if (!f) { strcpy(out, "unknown"); return; }
    char line[4096];
    strcpy(out, "unknown");
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(trim_str(line), key) == 0) {
                strcpy(out, trim_str(eq + 1));
                break;
            }
        }
    }
    fclose(f);
}

int get_state_int(const char* proj_id, const char* piece_id, const char* key) {
    char val[4096];
    get_state_str(proj_id, piece_id, key, val);
    if (strcmp(val, "unknown") == 0) return -1;
    return atoi(val);
}

int main(int argc, char* argv[]) {
    if (argc < 2) return 1;
    const char* active_piece = argv[1];
    
    // 1. Get Engine State
    char proj_id[256] = "template";
    FILE *ef = fopen("./pieces/apps/player_app/manager/state.txt", "r");
    if (ef) {
        char line[1024];
        while (fgets(line, sizeof(line), ef)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                if (strcmp(trim_str(line), "project_id") == 0) strncpy(proj_id, trim_str(eq + 1), 255);
            }
        }
        fclose(ef);
    }

    // 2. Get active piece position
    int ax = get_state_int(proj_id, active_piece, "pos_x");
    int ay = get_state_int(proj_id, active_piece, "pos_y");
    int az = get_state_int(proj_id, active_piece, "pos_z");
    if (az == -1) az = 0;

    // 3. Find piece at same location
    char pieces_dir[MAX_PATH];
    snprintf(pieces_dir, MAX_PATH, "./projects/%s/pieces", proj_id);
    DIR *dir = opendir(pieces_dir);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            if (strcmp(entry->d_name, active_piece) == 0) continue;
            
            int px = get_state_int(proj_id, entry->d_name, "pos_x");
            int py = get_state_int(proj_id, entry->d_name, "pos_y");
            int pz = get_state_int(proj_id, entry->d_name, "pos_z");
            if (pz == -1) pz = 0;

            if (px == ax && py == ay && pz == az) {
                // FOUND PIECE
                char script_rel[MAX_PATH];
                get_state_str(proj_id, entry->d_name, "on_interact", script_rel);
                
                if (strcmp(script_rel, "unknown") != 0) {
                    // Execute via Prisc VM
                    // prisc+x <script> <mem_in> <mem_out> <ops_file>
                    char *cmd = NULL;
                    asprintf(&cmd,
                        "../../../../prisc-xpanse+4]SHIP/prisc+x ./projects/%s/%s '' '' ./pieces/apps/player_app/manager/player_ops.txt",
                        proj_id, script_rel);
                    if (cmd) {
                        system(cmd);
                        free(cmd);
                    }

                }
                break;
            }
        }
        closedir(dir);
    }

    return 0;
}
