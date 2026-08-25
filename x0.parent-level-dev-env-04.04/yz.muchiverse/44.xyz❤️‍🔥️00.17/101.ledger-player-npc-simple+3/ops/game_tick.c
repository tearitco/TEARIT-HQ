/* game_tick.c - Main game loop tick
 * Reads config, determines whose turn it is, dispatches to appropriate handler
 * Self-contained */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_PATH 256
#define MAX_LINE 512

typedef struct {
    int num_players;
    int epoch_length;
    int current_epoch;
    int current_turn;
    char game_state[50];
    char last_input[256];
} GameState;

static int read_config(const char *path, GameState *state) {
    FILE *fp = fopen(path, "r");
    if (!fp) return 0;

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "num_players=%d", &state->num_players) == 1) continue;
        if (sscanf(line, "epoch_length=%d", &state->epoch_length) == 1) continue;
        if (sscanf(line, "current_epoch=%d", &state->current_epoch) == 1) continue;
        if (sscanf(line, "current_turn=%d", &state->current_turn) == 1) continue;
        if (sscanf(line, "game_state=%49s", state->game_state) == 1) continue;
    }
    fclose(fp);
    return 1;
}

static int write_config(const char *path, GameState *state) {
    FILE *fp = fopen(path, "w");
    if (!fp) return 0;

    fprintf(fp, "num_players=%d\n", state->num_players);
    fprintf(fp, "epoch_length=%d\n", state->epoch_length);
    fprintf(fp, "current_epoch=%d\n", state->current_epoch);
    fprintf(fp, "current_turn=%d\n", state->current_turn);
    fprintf(fp, "game_state=%s\n", state->game_state);
    fprintf(fp, "last_input=%s\n", state->last_input);
    fprintf(fp, "player_1_type=human\n");
    fprintf(fp, "player_1_name=Player1\n");
    fprintf(fp, "player_2_type=human\n");
    fprintf(fp, "player_2_name=Player2\n");
    fprintf(fp, "player_3_type=human\n");
    fprintf(fp, "player_3_name=Player3\n");
    fprintf(fp, "player_4_type=human\n");
    fprintf(fp, "player_4_name=Player4\n");
    fprintf(fp, "show_ledger=0\n");
    fprintf(fp, "menu_active=0\n");

    fclose(fp);
    return 1;
}

static char *get_player_type(const char *config_path, int player_num) {
    static char type[20];
    FILE *fp = fopen(config_path, "r");
    if (!fp) return "human";

    char line[MAX_LINE];
    char key[50];
    snprintf(key, sizeof(key), "player_%d_type=", player_num);

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, key, strlen(key)) == 0) {
            sscanf(line, "%*[^=]=%s", type);
            fclose(fp);
            return type;
        }
    }
    fclose(fp);
    return "human";
}

int main(int argc, char **argv) {
    char config_path[MAX_PATH] = "data/config.txt";
    char ledger_path[MAX_PATH] = "data/master_ledger.txt";

    if (argc > 1) snprintf(config_path, sizeof(config_path), "%s", argv[1]);
    if (argc > 2) snprintf(ledger_path, sizeof(ledger_path), "%s", argv[2]);

    GameState state = {0};
    if (!read_config(config_path, &state)) {
        fprintf(stderr, "Error reading config\n");
        return 1;
    }

    if (strcmp(state.game_state, "setup") == 0) {
        printf("SETUP\n");
        return 0;
    }

    if (strcmp(state.game_state, "finished") == 0) {
        printf("FINISHED\n");
        return 0;
    }

    /* Determine current player (ring-based turn management) */
    int current_player = (state.current_turn % state.num_players) + 1;

    /* Dispatch to appropriate handler */
    char *player_type = get_player_type(config_path, current_player);

    if (strcmp(player_type, "human") == 0) {
        printf("HUMAN_TURN|%d\n", current_player);
    } else {
        printf("COMPUTER_TURN|%d\n", current_player);
    }

    return 0;
}
