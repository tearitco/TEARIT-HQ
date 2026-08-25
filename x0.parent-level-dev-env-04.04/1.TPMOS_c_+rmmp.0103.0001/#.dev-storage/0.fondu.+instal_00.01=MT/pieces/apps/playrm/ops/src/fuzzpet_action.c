/*
 * Op: fuzzpet_action
 * Usage: ./+x/fuzzpet_action.+x <piece_id> <action>
 * Actions: feed, play, sleep
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
                if (strcmp(k, "project_root") == 0) strncpy(project_root, v, MAX_PATH-1);
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
            if (strcmp(trim_str(line), key) == 0) { val = atoi(trim_str(eq + 1)); break; }
        }
    }
    fclose(f); free(path);
    return val;
}

void set_state_int(const char* piece_id, const char* key, int val) {
    char *path = NULL;
    if (asprintf(&path, "%s/projects/%s/pieces/%s/state.txt", project_root, current_project, piece_id) == -1) return;
    char lines[100][MAX_LINE];
    int line_count = 0, found = 0;
    FILE *f = fopen(path, "r");
    if (f) {
        while (fgets(lines[line_count], sizeof(lines[0]), f) && line_count < 99) {
            char *eq = strchr(lines[line_count], '=');
            if (eq) {
                *eq = '\0';
                if (strcmp(trim_str(lines[line_count]), key) == 0) {
                    snprintf(lines[line_count], sizeof(lines[0]), "%s=%d\n", key, val);
                    found = 1;
                } else *eq = '=';
            }
            line_count++;
        }
        fclose(f);
    }
    if (!found && line_count < 100) snprintf(lines[line_count++], sizeof(lines[0]), "%s=%d\n", key, val);
    f = fopen(path, "w");
    if (f) {
        for (int i = 0; i < line_count; i++) fputs(lines[i], f);
        fclose(f);
    }
    free(path);
}

void set_response(const char* piece_id, const char* key, const char* msg) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/master_ledger/master_ledger.txt", project_root) != -1) {
        FILE *f = fopen(path, "a");
        if (f) {
            time_t now = time(NULL);
            fprintf(f, "[%ld] ResponseRequest: %s | %s: %s\n", now, piece_id, key, msg);
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

int main(int argc, char *argv[]) {
    if (argc < 3) return 1;
    resolve_paths();
    const char *id = argv[1];
    const char *action = argv[2];
    
    if (strcmp(action, "feed") == 0) {
        int h = get_state_int(id, "hunger");
        set_state_int(id, "hunger", (h > 20) ? h - 20 : 0);
        set_response(id, action, "Yum! Thanks for the food.");
    }
    else if (strcmp(action, "play") == 0) {
        int h = get_state_int(id, "happiness");
        set_state_int(id, "happiness", (h < 90) ? h + 10 : 100);
        set_response(id, action, "Yay! That was fun.");
    }
    else if (strcmp(action, "sleep") == 0) {
        int e = get_state_int(id, "energy");
        set_state_int(id, "energy", (e < 70) ? e + 30 : 100);
        set_response(id, action, "Zzz... feeling refreshed.");
    }
    else if (strcmp(action, "toggle_emoji") == 0) {
        int em = get_state_int(id, "emoji_mode");
        set_state_int(id, "emoji_mode", em ? 0 : 1);
        set_response(id, "emoji", em ? "ASCII Mode" : "Emoji Mode");
    }
    
    hit_frame_marker();
    return 0;
}
