/* word_turn_input.c - Process player action (word or end_turn)
 * Called by game_manager: word_turn_input <action> [word] [player_type]
 * player_type: "human" or "computer" (determines action_type in ledger) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    
    int action = atoi(argv[1]);
    const char *word = (argc >= 3) ? argv[2] : "hello";
    const char *player_type = (argc >= 4) ? argv[3] : "human";
    
    FILE *cfg = fopen("config.txt", "r");
    if (!cfg) return 1;

    int current_turn = 0, current_epoch = 1, total_actors = 2;
    char line[512];
    while (fgets(line, sizeof(line), cfg)) {
        sscanf(line, "current_turn=%d", &current_turn);
        sscanf(line, "current_epoch=%d", &current_epoch);
        sscanf(line, "num_players=%d", &total_actors);
    }
    fclose(cfg);

    int current_player = (current_turn % total_actors) + 1;
    char player_name[50] = "player";
    cfg = fopen("config.txt", "r");
    if (cfg) {
        while (fgets(line, sizeof(line), cfg)) {
            int p;
            if (sscanf(line, "player_%d_name=%49s", &p, player_name) == 2 && p == current_player) break;
        }
        fclose(cfg);
    }

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[50];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", tm_info);

    FILE *ledger = fopen("data/master_ledger.txt", "a");
    if (ledger) {
        if (action == 1) {
            /* Determine action_type based on player_type */
            const char *action_type = (strcmp(player_type, "computer") == 0) ? "computer_auto" : "human_input";
            fprintf(ledger, "%s|%d|%s|%d|%s|%s\n", timestamp, current_epoch, player_name, current_turn, word, action_type);
        } else if (action == 3) {
            fprintf(ledger, "%s|%d|%s|%d|N/A|end_turn\n", timestamp, current_epoch, player_name, current_turn);
        }
        fclose(ledger);
    }

    int new_turn = current_turn + 1;
    int turns_this_epoch = new_turn % total_actors;
    int new_epoch = current_epoch;
    if (turns_this_epoch == 0) new_epoch++;

    cfg = fopen("config.txt", "r");
    FILE *temp = fopen("config.txt.tmp", "w");
    if (cfg && temp) {
        while (fgets(line, sizeof(line), cfg)) {
            if (strncmp(line, "current_turn=", 13) == 0) fprintf(temp, "current_turn=%d\n", new_turn);
            else if (strncmp(line, "current_epoch=", 14) == 0) fprintf(temp, "current_epoch=%d\n", new_epoch);
            else fprintf(temp, "%s", line);
        }
        fclose(cfg);
        fclose(temp);
        rename("config.txt.tmp", "config.txt");
    }

    return 0;
}
