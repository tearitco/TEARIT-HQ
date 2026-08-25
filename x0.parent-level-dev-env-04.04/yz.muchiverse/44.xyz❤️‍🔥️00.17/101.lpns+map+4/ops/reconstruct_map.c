/* reconstruct_map.c - Rebuild 8x8 game map from master ledger
 * Replays all MOVE actions to reconstruct current map state
 * Self-contained */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATH 256
#define MAX_LINE 1024
#define MAP_SIZE 8
#define MAX_ACTORS 8

typedef struct {
    char name[50];
    int x;
    int y;
    int is_active;
} Actor;

int main(int argc, char **argv) {
    const char *ledger_path = "data/master_ledger.txt";
    const char *config_path = "data/config.txt";

    if (argc > 1) ledger_path = argv[1];
    if (argc > 2) config_path = argv[2];

    /* Initialize actors (start at 0,0) */
    Actor actors[MAX_ACTORS];
    int num_actors = 0;

    FILE *cfg = fopen(config_path, "r");
    if (cfg) {
        char line[MAX_LINE];
        int total = 0;
        while (fgets(line, sizeof(line), cfg)) {
            if (sscanf(line, "num_players=%d", &total) == 1) {
                num_actors = total;
                break;
            }
        }
        fclose(cfg);
    }

    if (num_actors == 0) num_actors = 2;  /* default */

    /* Load actor names and starting positions from config */
    cfg = fopen(config_path, "r");
    if (cfg) {
        char line[MAX_LINE];
        int actor_idx = 0;
        int *positions[MAX_ACTORS][2];
        for (int i = 0; i < MAX_ACTORS; i++) {
            actors[i].x = 0;
            actors[i].y = 0;
        }

        rewind(cfg);
        while (fgets(line, sizeof(line), cfg)) {
            for (int i = 1; i <= num_actors; i++) {
                char name_key[30];
                char value[50];
                int player_idx, pos_val;

                snprintf(name_key, sizeof(name_key), "player_%d_name=", i);
                if (strncmp(line, name_key, strlen(name_key)) == 0) {
                    sscanf(line, "%*[^=]=%49s", value);
                    strncpy(actors[i-1].name, value, sizeof(actors[i-1].name) - 1);
                    actors[i-1].is_active = 1;
                }
                else if (sscanf(line, "player_%d_start_x=%d", &player_idx, &pos_val) == 2) {
                    if (player_idx > 0 && player_idx <= num_actors) {
                        actors[player_idx-1].x = pos_val;
                    }
                }
                else if (sscanf(line, "player_%d_start_y=%d", &player_idx, &pos_val) == 2) {
                    if (player_idx > 0 && player_idx <= num_actors) {
                        actors[player_idx-1].y = pos_val;
                    }
                }
            }
        }
        fclose(cfg);
    }

    /* Replay ledger to reconstruct positions */
    FILE *ledger = fopen(ledger_path, "r");
    if (ledger) {
        char line[MAX_LINE];
        int line_num = 0;
        while (fgets(line, sizeof(line), ledger)) {
            line_num++;
            if (line_num == 1) continue;  /* Skip header */

            /* Parse: timestamp|epoch|player|turn|action_data|action_type */
            char timestamp[50], player[50], action_data[256], action_type[50];
            int epoch, turn;

            if (sscanf(line, "%49[^|]|%d|%49[^|]|%d|%255[^|]|%49s",
                       timestamp, &epoch, player, &turn, action_data, action_type) == 6) {

                /* Check if this is a MOVE action */
                if (strcmp(action_type, "move") == 0) {
                    /* Parse move data: "x:N,y:M" */
                    int new_x = -1, new_y = -1;
                    if (sscanf(action_data, "x:%d,y:%d", &new_x, &new_y) == 2) {
                        /* Find actor and update position */
                        for (int i = 0; i < num_actors; i++) {
                            if (strcmp(actors[i].name, player) == 0) {
                                actors[i].x = new_x;
                                actors[i].y = new_y;
                                break;
                            }
                        }
                    }
                }
            }
        }
        fclose(ledger);
    }

    /* Render map as ASCII */
    printf("8x8 MAP\n");
    printf("   01234567\n");

    for (int y = 0; y < MAP_SIZE; y++) {
        printf(" %d ", y);
        for (int x = 0; x < MAP_SIZE; x++) {
            char cell = '.';

            /* Find actor at this position */
            for (int i = 0; i < num_actors; i++) {
                if (actors[i].is_active && actors[i].x == x && actors[i].y == y) {
                    /* Display actor as digit (1-8) */
                    cell = '1' + i;
                    break;
                }
            }

            printf("%c", cell);
        }
        printf("\n");
    }

    printf("\nActors:\n");
    for (int i = 0; i < num_actors; i++) {
        if (actors[i].is_active) {
            printf("  %d: %s at (%d,%d)\n", i+1, actors[i].name, actors[i].x, actors[i].y);
        }
    }

    return 0;
}
