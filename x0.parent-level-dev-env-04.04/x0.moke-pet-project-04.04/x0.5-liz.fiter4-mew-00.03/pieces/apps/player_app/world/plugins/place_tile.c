#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#define TPM_PIECE_MANAGER_CMD "'./pieces/master_ledger/plugins/+x/piece_manager.+x'"

// place_tile.c - Editor Operation (v1.0)
// Responsibility: Paint a tile character into the project's terrain file.

#define MAX_PATH 16384
#define MAP_ROWS 10
#define MAP_COLS 20

char* trim_str(char *str) {
    char *end;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

int get_piece_state_int(const char* piece_id, const char* key) {
    char cmd[MAX_PATH];
    snprintf(cmd, MAX_PATH, "%s %s get-state %s", TPM_PIECE_MANAGER_CMD, piece_id, key);
    FILE *fp = popen(cmd, "r");
    if (!fp) return -1;
    char val[128];
    int res = -1;
    if (fgets(val, sizeof(val), fp)) {
        if (strstr(val, "STATE_NOT_FOUND") == NULL) res = atoi(trim_str(val));
    }
    pclose(fp);
    return res;
}

int main(int argc, char* argv[]) {
    if (argc < 2) return 1;
    char tile_char = argv[1][0];

    // 1. Get Engine State
    char proj_id[256] = "template";
    char map_id[256] = "map_0001";
    int cz = 0;
    FILE *ef = fopen("./pieces/apps/player_app/manager/state.txt", "r");
    if (ef) {
        char line[1024];
        while (fgets(line, sizeof(line), ef)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *k = trim_str(line);
                char *v = trim_str(eq + 1);
                if (strcmp(k, "project_id") == 0) strncpy(proj_id, v, 255);
                else if (strcmp(k, "current_map") == 0) strncpy(map_id, v, 255);
                else if (strcmp(k, "current_z") == 0) cz = atoi(v);
            }
        }
        fclose(ef);
    }

    // 2. Get Selector position
    int sx = get_piece_state_int("selector", "pos_x");
    int sy = get_piece_state_int("selector", "pos_y");
    if (sx == -1 || sy == -1) return 1;

    // 3. Open Map File
    char path[MAX_PATH];
    snprintf(path, MAX_PATH, "./projects/%s/maps/%s_z%d.txt", proj_id, map_id, cz);
    
    FILE *mf = fopen(path, "r+");
    if (!mf) {
        // Try creating it if it doesn't exist? (Or just fail for now)
        return 1;
    }

    // 4. Update the character at sy, sx
    // Maps are sy lines of (MAP_COLS + 1) characters
    long offset = sy * (MAP_COLS + 1) + sx;
    fseek(mf, offset, SEEK_SET);
    fputc(tile_char, mf);
    fclose(mf);

    // Frame pulse handled by manager (player_manager.c)
    // system("echo 'M' >> ./pieces/display/frame_changed.txt");

    return 0;
}
