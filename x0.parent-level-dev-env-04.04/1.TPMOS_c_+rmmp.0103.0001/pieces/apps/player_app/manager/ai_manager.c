#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>

#define MAX_PATH 16384

char project_root[MAX_PATH] = ".";
char current_project[64] = "fuzz-op";

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
    if (!getcwd(project_root, sizeof(project_root))) strncpy(project_root, ".", sizeof(project_root) - 1);
    
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[2048];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                if (strcmp(trim_str(line), "project_root") == 0) snprintf(project_root, sizeof(project_root), "%s", trim_str(eq + 1));
            }
        }
        fclose(kvp);
    }
}

void get_state_str(const char* piece_id, const char* key, char* out) {
    char path[MAX_PATH];
    snprintf(path, MAX_PATH, "%s/projects/%s/pieces/world_01/map_01_z0/%s/state.txt", project_root, current_project, piece_id);
    
    FILE *f = fopen(path, "r");
    if (!f) {
        if (strcmp(piece_id, "clock_daemon") == 0) strcpy(path, "./pieces/system/clock_daemon/state.txt");
        else { strcpy(out, "unknown"); return; }
        f = fopen(path, "r");
        if (!f) { strcpy(out, "unknown"); return; }
    }
    
    char line[MAX_PATH];
    strcpy(out, "unknown");
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            char *k = trim_str(line);
            if (strcmp(k, key) == 0) {
                char *v = trim_str(eq + 1);
                strcpy(out, v);
                break;
            }
        }
    }
    fclose(f);
}

int get_state_int(const char* piece_id, const char* key) {
    char val[MAX_PATH];
    get_state_str(piece_id, key, val);
    if (strcmp(val, "unknown") == 0) return -1;
    return atoi(val);
}

int main() {
    resolve_paths();
    int last_turn = get_state_int("clock_daemon", "turn");
    printf("AI Manager started. Last turn: %d\n", last_turn);

    while(1) {
        int current_turn = get_state_int("clock_daemon", "turn");
        if (current_turn > last_turn) {
            printf("Turn change detected: %d -> %d. Updating NPCs...\n", last_turn, current_turn);
            
            char map_path[MAX_PATH];
            snprintf(map_path, MAX_PATH, "%s/projects/%s/pieces/world_01/map_01_z0", project_root, current_project);
            DIR *dir = opendir(map_path);
            if (dir) {
                struct dirent *entry;
                while ((entry = readdir(dir)) != NULL) {
                    if (entry->d_name[0] == '.') continue;
                    char piece_dir[MAX_PATH];
                    snprintf(piece_dir, MAX_PATH, "%s/%s", map_path, entry->d_name);
                    struct stat st;
                    if (stat(piece_dir, &st) != 0 || !S_ISDIR(st.st_mode)) continue;

                    char behavior[64];
                    get_state_str(entry->d_name, "behavior", behavior);

                    if (strcmp(behavior, "aggressive") == 0 || strcmp(behavior, "chase") == 0) {
                        char cmd[MAX_PATH];
                        snprintf(cmd, MAX_PATH, "'%s/pieces/apps/player_app/traits/+x/npc_chase.+x' %s", project_root, entry->d_name);
                        system(cmd);
                    }
                }
                closedir(dir);
            }
            
            last_turn = current_turn;
            char pulse_path[MAX_PATH];
            snprintf(pulse_path, MAX_PATH, "%s/pieces/display/frame_changed.txt", project_root);
            FILE *pf = fopen(pulse_path, "a"); if (pf) { fprintf(pf, "A\n"); fclose(pf); }
        }
        usleep(100000);
    }
    return 0;
}
