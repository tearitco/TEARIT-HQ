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

void set_state(const char* piece_id, const char* key, int val) {
    char *cmd = malloc(MAX_PATH * 2);
    if (!cmd) return;
    snprintf(cmd, MAX_PATH * 2, "%s %s set-state %s %d > /dev/null 2>&1", TPM_PIECE_MANAGER_CMD, piece_id, key, val);
    system(cmd);
    free(cmd);
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

int main(int argc, char *argv[]) {
    if (argc < 3) return 1;
    const char *piece_id = argv[1];
    char dir = argv[2][0];

    int x = get_state(piece_id, "pos_x");
    int y = get_state(piece_id, "pos_y");
    if (x == -1 || y == -1) return 1;

    if (dir == 'w') y--;
    else if (dir == 's') y++;
    else if (dir == 'a') x--;
    else if (dir == 'd') x++;

    // Basic bounds checking
    if (x < 0) x = 0; if (x >= 20) x = 19;
    if (y < 0) y = 0; if (y >= 10) y = 9;

    set_state(piece_id, "pos_x", x);
    set_state(piece_id, "pos_y", y);

    // Frame pulse handled by manager (player_manager.c)
    // system("echo 'M' >> ./pieces/display/frame_changed.txt");

    return 0;
}
