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

int get_state(const char* piece_id, const char* key) {
    char *cmd = malloc(MAX_PATH * 2);
    if (!cmd) return -1;
    // We check map_01 first, then fallback to apps folders
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
    if (argc < 2) return 1;
    const char *npc_id = argv[1];

    // 1. Get NPC Position
    int nx = get_state(npc_id, "pos_x");
    int ny = get_state(npc_id, "pos_y");
    int nz = get_state(npc_id, "pos_z");
    if (nz == -1) nz = 0;

    // 2. Get Target (Player) Position
    // The player piece is usually 'fuzzpet' or 'player' depending on the active app.
    // For now, we'll try to find the piece id 'player' then 'fuzzpet'.
    int tx = get_state("player", "pos_x");
    int ty = get_state("player", "pos_y");
    int tz = get_state("player", "pos_z");
    if (tx == -1) {
        tx = get_state("fuzzpet", "pos_x");
        ty = get_state("fuzzpet", "pos_y");
        tz = get_state("fuzzpet", "pos_z");
    }
    if (tz == -1) tz = 0;

    // 3. Only chase if on same Z level
    if (nz != tz) return 0;

    // 4. Simple Chase AI (X then Y)
    int next_x = nx;
    int next_y = ny;

    if (tx > nx) next_x++;
    else if (tx < nx) next_x--;
    else if (ty > ny) next_y++;
    else if (ty < ny) next_y--;

    // 5. Basic bounds and collision (Don't step on player)
    if (next_x == tx && next_y == ty) {
        // Attack logic would go here
        return 0;
    }

    set_state(npc_id, "pos_x", next_x);
    set_state(npc_id, "pos_y", next_y);

    return 0;
}
