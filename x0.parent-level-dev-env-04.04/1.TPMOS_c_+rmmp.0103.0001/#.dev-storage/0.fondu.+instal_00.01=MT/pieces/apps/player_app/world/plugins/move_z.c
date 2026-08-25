#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#ifndef _WIN32
#include <sys/wait.h>
#endif
#include <ctype.h>

#define TPM_PIECE_MANAGER_CMD "'./pieces/master_ledger/plugins/+x/piece_manager.+x'"

// move_z.c - Engine Operation (v1.3 - ENGINE AWARE)
// Responsibility: Update Z-level of active piece or specified piece.

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

void get_engine_active_piece(char *out) {
    strcpy(out, "selector"); // Default
    FILE *f = fopen("./pieces/apps/player_app/manager/state.txt", "r");
    if (f) {
        char line[1024];
        while (fgets(line, sizeof(line), f)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                if (strcmp(trim_str(line), "active_piece") == 0) {
                    strcpy(out, trim_str(eq + 1));
                    break;
                }
            }
        }
        fclose(f);
    }
}

int get_state(const char* piece_id, const char* key) {
    char *cmd = malloc(MAX_PATH * 2);
    if (!cmd) return -1;
    snprintf(cmd, MAX_PATH * 2, "%s %s get-state %s", TPM_PIECE_MANAGER_CMD, piece_id, key);
    FILE *fp = popen(cmd, "r");
    free(cmd);
    if (!fp) return -1;
    char val[128];
    int res = -1;
    if (fgets(val, sizeof(val), fp)) {
        char *t = trim_str(val);
        if (strstr(t, "STATE_NOT_FOUND") == NULL) res = atoi(t);
    }
    pclose(fp);
    return res;
}

void set_state(const char* piece_id, const char* key, int val) {
    char *cmd = malloc(MAX_PATH * 2);
    if (!cmd) return;
    snprintf(cmd, MAX_PATH * 2, "%s %s set-state %s %d > /dev/null 2>&1", TPM_PIECE_MANAGER_CMD, piece_id, key, val);
    system(cmd);
    free(cmd);
}

int main(int argc, char *argv[]) {
    char piece_id[64];
    int dz = 0;

    if (argc < 2) return 1;

    if (argc == 2) {
        // Only one argument: dz
        // Read piece_id from engine state
        get_engine_active_piece(piece_id);
        dz = atoi(argv[1]);
    } else {
        // Two arguments: piece_id dz
        strncpy(piece_id, argv[1], 63);
        dz = atoi(argv[2]);
    }

    int z = get_state(piece_id, "pos_z");
    if (z == -1) z = 0;

    int new_z = z + dz;
    
    // Update state
    set_state(piece_id, "pos_z", new_z);

    // Frame pulse handled by manager (player_manager.c)
    // system("echo 'Z' >> ./pieces/display/frame_changed.txt");

    return 0;
}
