/* game_turn_input.c - Process game action (move direction or word/end_turn)
 * Action codes: 1=word, 3=end_turn, 4=move_right, 5=move_left, 6=move_up, 7=move_down
 * Updates master_ledger and increments turn */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_ACTORS 8
#define MAP_SIZE 8

int main(int argc, char **argv) {
    if (argc < 2) return 1;

    int action = atoi(argv[1]);

    /* DEBUG: Log when called */
    FILE *dbg = fopen("game_turn_input.log", "a");
    if (dbg) {
        fprintf(dbg, "game_turn_input called with action=%d argv[1]=%s\n", action, argv[1]);
        fclose(dbg);
    }

    FILE *cfg = fopen("config.txt", "r");
    if (!cfg) return 1;

    int current_turn = 0, current_epoch = 1, total_actors = 4;
    char line[512];
    while (fgets(line, sizeof(line), cfg)) {
        sscanf(line, "current_turn=%d", &current_turn);
        sscanf(line, "current_epoch=%d", &current_epoch);
        sscanf(line, "num_players=%d", &total_actors);
    }
    fclose(cfg);

    int current_player_idx = current_turn % total_actors;
    int current_player = current_player_idx + 1;

    char player_name[50] = "player";
    cfg = fopen("config.txt", "r");
    if (cfg) {
        while (fgets(line, sizeof(line), cfg)) {
            int pidx;
            if (sscanf(line, "player_%d_name=%49s", &pidx, player_name) == 2 && pidx == current_player) break;
        }
        fclose(cfg);
    }

    /* Get current player position (replay ledger to reconstruct) */
    int player_x = 0, player_y = 0;

    /* Read starting position from config */
    cfg = fopen("config.txt", "r");
    if (cfg) {
        char search[50];
        sprintf(search, "player_%d_start_x=", current_player);
        while (fgets(line, sizeof(line), cfg)) {
            int pidx;
            if (sscanf(line, "player_%d_start_x=%d", &pidx, &player_x) == 2 && pidx == current_player) break;
        }
        fclose(cfg);
    }

    cfg = fopen("config.txt", "r");
    if (cfg) {
        while (fgets(line, sizeof(line), cfg)) {
            int pidx;
            if (sscanf(line, "player_%d_start_y=%d", &pidx, &player_y) == 2 && pidx == current_player) break;
        }
        fclose(cfg);
    }

    /* Replay ledger to get current position */
    FILE *ledger = fopen("data/master_ledger.txt", "r");
    if (ledger) {
        int line_num = 0;
        while (fgets(line, sizeof(line), ledger)) {
            line_num++;
            if (line_num == 1) continue;  /* Skip header */

            char pname[50];
            int new_x, new_y;
            if (sscanf(line, "%*[^|]|%*d|%49[^|]|%*d|x:%d,y:%d", pname, &new_x, &new_y) >= 3) {
                if (strcmp(pname, player_name) == 0) {
                    player_x = new_x;
                    player_y = new_y;
                }
            }
        }
        fclose(ledger);
    }

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[50];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", tm_info);

    /* Process action */
    ledger = fopen("data/master_ledger.txt", "a");
    if (ledger) {
        if (action == 1) {
            /* Word action */
            fprintf(ledger, "%s|%d|%s|%d|hello|word\n", timestamp, current_epoch, player_name, current_turn);
        } else if (action >= 4 && action <= 7) {
            /* Move action - update position based on direction */
            int new_x = player_x, new_y = player_y;

            if (action == 4) new_x++;  /* right */
            if (action == 5) new_x--;  /* left */
            if (action == 6) new_y--;  /* up */
            if (action == 7) new_y++;  /* down */

            /* Bounds check */
            if (new_x < 0) new_x = 0;
            if (new_x >= MAP_SIZE) new_x = MAP_SIZE - 1;
            if (new_y < 0) new_y = 0;
            if (new_y >= MAP_SIZE) new_y = MAP_SIZE - 1;

            fprintf(ledger, "%s|%d|%s|%d|x:%d,y:%d|move\n", timestamp, current_epoch, player_name, current_turn, new_x, new_y);
        } else if (action == 3) {
            /* End turn action */
            fprintf(ledger, "%s|%d|%s|%d|N/A|end_turn\n", timestamp, current_epoch, player_name, current_turn);
        }
        fclose(ledger);
    }

    /* Increment turn and epoch */
    int new_turn = current_turn + 1;
    int new_epoch = current_epoch;

    if (new_turn % total_actors == 0) {
        new_epoch = current_epoch + 1;
    }

    /* Update config */
    cfg = fopen("config.txt", "r");
    FILE *temp = fopen("config.txt.tmp", "w");
    if (cfg && temp) {
        while (fgets(line, sizeof(line), cfg)) {
            if (strncmp(line, "current_turn=", 13) == 0) {
                fprintf(temp, "current_turn=%d\n", new_turn);
            } else if (strncmp(line, "current_epoch=", 14) == 0) {
                fprintf(temp, "current_epoch=%d\n", new_epoch);
            } else {
                fprintf(temp, "%s", line);
            }
        }
        fclose(cfg);
        fclose(temp);
        rename("config.txt.tmp", "config.txt");
    }

    return 0;
}
