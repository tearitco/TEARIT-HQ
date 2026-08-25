#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>

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

void get_state_str(const char* piece_id, const char* key, char* out) {
    char path[MAX_PATH];
    // Check world directory
    snprintf(path, MAX_PATH, "./pieces/world/map_01/%s/state.txt", piece_id);
    if (access(path, F_OK) != 0) {
        // Fallback to specific system pieces
        if (strcmp(piece_id, "clock_daemon") == 0) strcpy(path, "./pieces/system/clock_daemon/state.txt");
        else { strcpy(out, "unknown"); return; }
    }
    
    FILE *f = fopen(path, "r");
    if (!f) { strcpy(out, "unknown"); return; }
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
    int last_turn = get_state_int("clock_daemon", "turn");
    printf("AI Manager started. Last turn: %d\n", last_turn);

    while(1) {
        int current_turn = get_state_int("clock_daemon", "turn");
        if (current_turn > last_turn) {
            printf("Turn change detected: %d -> %d. Updating NPCs...\n", last_turn, current_turn);
            
            // Iterate through world pieces to find AI-driven pieces
            DIR *dir = opendir("./pieces/world/map_01");
            if (dir) {
                struct dirent *entry;
                while ((entry = readdir(dir)) != NULL) {
                    if (entry->d_name[0] == '.') continue;

                    /* Data-driven: check for behavior field instead of hardcoded type */
                    char behavior[64];
                    get_state_str(entry->d_name, "behavior", behavior);

                    if (strcmp(behavior, "aggressive") == 0 || strcmp(behavior, "chase") == 0) {
                        char cmd[MAX_PATH];
                        snprintf(cmd, MAX_PATH, "'./pieces/apps/player_app/traits/+x/npc_chase.+x' %s", entry->d_name);
                        system(cmd);
                    }
                }
                closedir(dir);
            }
            
            last_turn = current_turn;
            // Pulse render after NPC moves
            system("echo 'A' >> ./pieces/display/frame_changed.txt");
        }
        usleep(100000); // Check every 100ms
    }
    return 0;
}
