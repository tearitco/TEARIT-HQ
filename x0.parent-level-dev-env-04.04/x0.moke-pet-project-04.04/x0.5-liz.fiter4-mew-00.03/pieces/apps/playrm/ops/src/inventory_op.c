/*
 * Op: inventory_op
 * Usage: ./+x/inventory_op.+x
 * 
 * Shows detailed stats/inventory for the selector.
 * OPTIMIZED: Uses PRISC_PROJECT_ROOT and PRISC_PROJECT_ID environment variables
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
#define MAX_LINE 1024

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
    // PRIORITY 1: Environment variables from PAL module
    char *env_root = getenv("PRISC_PROJECT_ROOT");
    char *env_project = getenv("PRISC_PROJECT_ID");
    
    if (env_root && strlen(env_root) > 0) {
        strncpy(project_root, env_root, MAX_PATH-1);
    }
    if (env_project && strlen(env_project) > 0) {
        strncpy(current_project, env_project, sizeof(current_project)-1);
    }
    
    // PRIORITY 2: Fallback to location_kvp file
    if (strlen(project_root) == 0 || strcmp(project_root, ".") == 0) {
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
    }
    
    // PRIORITY 3: Fallback to manager state
    if (strlen(current_project) == 0) {
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
                            strncpy(current_project, v, sizeof(current_project)-1);
                        }
                    }
                }
                fclose(f);
            }
            free(p_mgr_path);
        }
    }
    
    if (strlen(current_project) == 0) {
        strncpy(current_project, "template", sizeof(current_project)-1);
    }
}

void get_piece_state(const char* piece_id, const char* key, char* out_val) {
    strcpy(out_val, "");
    char *path = NULL;
    if (asprintf(&path, "%s/projects/%s/pieces/%s/state.txt", project_root, current_project, piece_id) == -1) return;
    FILE *f = fopen(path, "r");
    if (f) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                if (strcmp(trim_str(line), key) == 0) {
                    strcpy(out_val, trim_str(eq + 1));
                    break;
                }
            }
        }
        fclose(f);
    }
    free(path);
}

void set_response(const char* msg) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/master_ledger/master_ledger.txt", project_root) != -1) {
        FILE *f = fopen(path, "a");
        if (f) {
            time_t now = time(NULL);
            fprintf(f, "[%ld] EventFire: %s | Source: inventory_op\n", now, msg);
            fclose(f);
        }
        free(path);
    }
}

void hit_frame_marker() {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/display/frame_changed.txt", project_root) != -1) {
        FILE *f = fopen(path, "a");
        if (f) { fprintf(f, "X\n"); fclose(f); }
        free(path);
    }
}

int main() {
    resolve_paths();
    
    char h[16], e[16], hp[16];
    get_piece_state("selector", "hunger", h);
    get_piece_state("selector", "energy", e);
    get_piece_state("selector", "happiness", hp);
    
    char final_msg[512];
    sprintf(final_msg, "SELECTOR: Hunger:%s Happiness:%s Energy:%s", h, hp, e);
    set_response(final_msg);
    printf("%s\n", final_msg);
    
    hit_frame_marker();
    
    return 0;
}
