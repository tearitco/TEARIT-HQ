/* game_compose_frame.c - Render current game state to frame buffer
 * Reads config, reconstructs map from ledger, outputs to pieces/display/current_frame.txt
 * Self-contained prisc+x op */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 1024
#define MAX_ACTORS 8
#define MAP_SIZE 8

typedef struct {
    char name[50];
    int x;
    int y;
    int is_active;
} Actor;

int main() {
    /* Read game state from config */
    FILE *cfg = fopen("config.txt", "r");
    if (!cfg) {
        fprintf(stderr, "Cannot read config.txt\n");
        return 1;
    }

    int current_epoch = 1, current_turn = 0, total_actors = 2;
    char game_state[20] = "playing";
    
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), cfg)) {
        sscanf(line, "current_epoch=%d", &current_epoch);
        sscanf(line, "current_turn=%d", &current_turn);
        sscanf(line, "num_players=%d", &total_actors);
        sscanf(line, "game_state=%19s", game_state);
    }
    fclose(cfg);

    /* Load actor data */
    Actor actors[MAX_ACTORS] = {0};
    cfg = fopen("config.txt", "r");
    if (cfg) {
        while (fgets(line, sizeof(line), cfg)) {
            int i;
            char name[50];
            if (sscanf(line, "player_%d_name=%49s", &i, name) == 2 && i > 0 && i <= total_actors) {
                strncpy(actors[i-1].name, name, sizeof(actors[i-1].name) - 1);
                actors[i-1].is_active = 1;
                actors[i-1].x = 0;
                actors[i-1].y = 0;
            }
            if (sscanf(line, "player_%d_start_x=%d", &i, &actors[i-1].x) == 2 && i > 0 && i <= total_actors) {}
            if (sscanf(line, "player_%d_start_y=%d", &i, &actors[i-1].y) == 2 && i > 0 && i <= total_actors) {}
        }
        fclose(cfg);
    }

    /* Replay ledger to get current positions */
    FILE *ledger = fopen("data/master_ledger.txt", "r");
    if (ledger) {
        int line_num = 0;
        while (fgets(line, sizeof(line), ledger)) {
            line_num++;
            if (line_num == 1) continue;
            
            char player[50], action_type[50];
            int new_x, new_y;
            if (sscanf(line, "%*[^|]|%*d|%49[^|]|%*d|x:%d,y:%d|%49s", player, &new_x, &new_y, action_type) >= 3) {
                if (strcmp(action_type, "move") == 0) {
                    for (int i = 0; i < total_actors; i++) {
                        if (strcmp(actors[i].name, player) == 0) {
                            actors[i].x = new_x;
                            actors[i].y = new_y;
                            break;
                        }
                    }
                }
            }
        }
        fclose(ledger);
    }

    /* Write COMPLETE rendered frame to view.txt (chtpm_parser_pal reads this for ${game_map}) */
    FILE *frame = fopen("pieces/apps/player_app/view.txt", "w");
    if (frame) {
        fprintf(frame, "═══════════════════════════════════════\n");
        fprintf(frame, "  LPNS - Ledger-Player-NPC-Simple\n");
        fprintf(frame, "  (Map Variant - 8x8 Grid)\n");
        fprintf(frame, "═══════════════════════════════════════\n\n");

        fprintf(frame, "Status: Turn %d / Epoch %d\n", current_turn, current_epoch);
        fprintf(frame, "Current Player: %s\n\n", (current_turn % total_actors) < total_actors ? actors[current_turn % total_actors].name : "unknown");

        fprintf(frame, "8x8 MAP\n");
        fprintf(frame, "   01234567\n");

        for (int y = 0; y < MAP_SIZE; y++) {
            fprintf(frame, " %d ", y);
            for (int x = 0; x < MAP_SIZE; x++) {
                char cell = '.';
                for (int i = 0; i < total_actors; i++) {
                    if (actors[i].is_active && actors[i].x == x && actors[i].y == y) {
                        cell = '1' + i;
                        break;
                    }
                }
                fprintf(frame, "%c", cell);
            }
            fprintf(frame, "\n");
        }

        fprintf(frame, "\nActors:\n");
        for (int i = 0; i < total_actors; i++) {
            if (actors[i].is_active) {
                fprintf(frame, "  %d: %s at (%d,%d)\n", i+1, actors[i].name, actors[i].x, actors[i].y);
            }
        }

        fprintf(frame, "\nControls: (w)ord, (m)ove, (e)nd turn, Ctrl+C to quit\n");

        fclose(frame);
    }

    /* Mark that renderer should update (x0.moke: single trigger via frame_changed.txt) */
    FILE *marker = fopen("pieces/display/frame_changed.txt", "a");
    if (marker) {
        fprintf(marker, "G");
        fclose(marker);
    }

    return 0;
}
