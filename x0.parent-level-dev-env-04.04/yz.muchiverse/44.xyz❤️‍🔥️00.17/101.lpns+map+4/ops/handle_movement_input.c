/* handle_movement_input.c - Process and validate player movement
 * Input: player_name x y config_path ledger_path
 * Output: appends MOVE action to ledger if valid
 * Self-contained */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_PATH 256
#define MAX_LINE 512
#define MAP_SIZE 8

typedef struct {
    char name[50];
    int x;
    int y;
} Actor;

int is_occupied(const char *ledger_path, int x, int y) {
    Actor actors[8];
    int num_actors = 0;

    FILE *ledger = fopen(ledger_path, "r");
    if (!ledger) return 0;

    /* Replay moves to find current positions */
    char line[MAX_LINE];
    int line_num = 0;
    while (fgets(line, sizeof(line), ledger)) {
        line_num++;
        if (line_num == 1) continue;

        char timestamp[50], player[50], action_data[256], action_type[50];
        int epoch, turn;

        if (sscanf(line, "%49[^|]|%d|%49[^|]|%d|%255[^|]|%49s",
                   timestamp, &epoch, player, &turn, action_data, action_type) == 6) {

            if (strcmp(action_type, "move") == 0) {
                int nx = -1, ny = -1;
                if (sscanf(action_data, "x:%d,y:%d", &nx, &ny) == 2) {
                    /* Find or add actor */
                    int found = 0;
                    for (int i = 0; i < num_actors; i++) {
                        if (strcmp(actors[i].name, player) == 0) {
                            actors[i].x = nx;
                            actors[i].y = ny;
                            found = 1;
                            break;
                        }
                    }
                    if (!found && num_actors < 8) {
                        strncpy(actors[num_actors].name, player, sizeof(actors[num_actors].name) - 1);
                        actors[num_actors].x = nx;
                        actors[num_actors].y = ny;
                        num_actors++;
                    }
                }
            }
        }
    }
    fclose(ledger);

    /* Check if target position is occupied */
    for (int i = 0; i < num_actors; i++) {
        if (actors[i].x == x && actors[i].y == y) {
            return 1;  /* Occupied */
        }
    }

    return 0;  /* Empty */
}

int main(int argc, char **argv) {
    if (argc < 5) {
        fprintf(stderr, "Usage: handle_movement_input <player_name> <x> <y> <config> <ledger>\n");
        return 1;
    }

    const char *player_name = argv[1];
    int target_x = atoi(argv[2]);
    int target_y = atoi(argv[3]);
    const char *config_path = argv[4];
    const char *ledger_path = argv[5];

    /* Validate bounds */
    if (target_x < 0 || target_x >= MAP_SIZE || target_y < 0 || target_y >= MAP_SIZE) {
        printf("ERROR: Out of bounds (0-7)\n");
        return 1;
    }

    /* Validate occupancy */
    if (is_occupied(ledger_path, target_x, target_y)) {
        printf("ERROR: Cell occupied\n");
        return 1;
    }

    /* Get current epoch */
    FILE *cfg = fopen(config_path, "r");
    int current_epoch = 1;
    if (cfg) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), cfg)) {
            if (sscanf(line, "current_epoch=%d", &current_epoch) == 1) break;
        }
        fclose(cfg);
    }

    /* Append MOVE to ledger */
    FILE *ledger = fopen(ledger_path, "a");
    if (!ledger) {
        fprintf(stderr, "Error: cannot open ledger\n");
        return 1;
    }

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[50];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", tm_info);

    /* Count turns */
    FILE *count_fp = fopen(ledger_path, "r");
    int turn_count = -1;
    if (count_fp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), count_fp)) {
            turn_count++;
        }
        fclose(count_fp);
    }

    fprintf(ledger, "%s|%d|%s|%d|x:%d,y:%d|move\n",
            timestamp, current_epoch, player_name, turn_count, target_x, target_y);
    fclose(ledger);

    printf("Moved to (%d,%d)\n", target_x, target_y);
    return 0;
}
