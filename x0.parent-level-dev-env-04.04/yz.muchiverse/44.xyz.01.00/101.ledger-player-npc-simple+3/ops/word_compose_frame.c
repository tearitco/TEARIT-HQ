/* word_compose_frame.c - Render word game state to state.txt
 * Writes game_map variable for chtpm_parser to substitute into layout */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void read_config_str(const char *key, char *buf, size_t bufsz) {
    buf[0] = '\0';
    FILE *f = fopen("config.txt", "r");
    if (!f) return;
    char line[512];
    char prefix[128];
    snprintf(prefix, sizeof(prefix), "%s=", key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, prefix, strlen(prefix)) == 0) {
            char *val = line + strlen(prefix);
            val[strcspn(val, "\r\n")] = '\0';
            strncpy(buf, val, bufsz - 1);
            buf[bufsz - 1] = '\0';
            break;
        }
    }
    fclose(f);
}

int main(void) {
    FILE *cfg = fopen("config.txt", "r");
    if (!cfg) return 1;

    int current_epoch = 1, current_turn = 0, total_actors = 2;
    char line[512];
    while (fgets(line, sizeof(line), cfg)) {
        sscanf(line, "current_epoch=%d", &current_epoch);
        sscanf(line, "current_turn=%d", &current_turn);
        sscanf(line, "num_players=%d", &total_actors);
    }
    fclose(cfg);

    /* Get current player name and type */
    int current_player = (current_turn % total_actors) + 1;
    char player_name[50] = "player";
    char player_type[32] = "human";
    cfg = fopen("config.txt", "r");
    if (cfg) {
        while (fgets(line, sizeof(line), cfg)) {
            int p;
            if (sscanf(line, "player_%d_name=%49s", &p, player_name) == 2 && p == current_player) break;
        }
        fclose(cfg);
    }
    /* Get player type */
    char type_key[64];
    snprintf(type_key, sizeof(type_key), "player_%d_type", current_player);
    read_config_str(type_key, player_type, sizeof(player_type));

    /* Build game_map content - fits inside box borders */
    char game_map[4096] = "";
    int off = 0;
    off += snprintf(game_map + off, sizeof(game_map) - off,
        "║ Turn: %d / Epoch: %d\\n", current_turn, current_epoch);
    
    /* Show player name with type indicator */
    if (strcmp(player_type, "computer") == 0) {
        off += snprintf(game_map + off, sizeof(game_map) - off,
            "║ Current Player: %s (NPC)\\n\\n", player_name);
    } else {
        off += snprintf(game_map + off, sizeof(game_map) - off,
            "║ Current Player: %s (YOU)\\n\\n", player_name);
    }
    
    off += snprintf(game_map + off, sizeof(game_map) - off,
        "║ Recent Words:\\n");

    FILE *ledger = fopen("data/master_ledger.txt", "r");
    if (ledger) {
        int count = 0;
        while (fgets(line, sizeof(line), ledger) && count < 5) {
            char word[100], action_type[20];
            if (sscanf(line, "%*[^|]|%*d|%*[^|]|%*d|%99[^|]|%19s", word, action_type) == 2) {
                if (strcmp(action_type, "human_input") == 0 || strcmp(action_type, "computer_auto") == 0) {
                    off += snprintf(game_map + off, sizeof(game_map) - off,
                        "║   - %s\\n", word);
                    count++;
                }
            }
        }
        fclose(ledger);
    }

    /* Write game_map to state.txt for chtpm_parser ${game_map} substitution */
    FILE *state = fopen("pieces/apps/player_app/state.txt", "w");
    if (state) {
        fprintf(state, "game_map=%s\n", game_map);
        fclose(state);
    }

    return 0;
}
