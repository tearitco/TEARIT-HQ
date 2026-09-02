/* sync_player_ledger.c - Sync player's view of ledger (filtered entries)
 * Input: master_ledger_path player_name output_path
 * Output: writes player_name's entries to output_path
 * Self-contained */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATH 256
#define MAX_LINE 512

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: sync_player_ledger <master_ledger> <player_name> <output_path>\n");
        return 1;
    }

    const char *master_ledger = argv[1];
    const char *player_name = argv[2];
    const char *output_path = argv[3];

    FILE *master = fopen(master_ledger, "r");
    if (!master) {
        fprintf(stderr, "Error: cannot open master ledger\n");
        return 1;
    }

    FILE *output = fopen(output_path, "w");
    if (!output) {
        fprintf(stderr, "Error: cannot open output path\n");
        fclose(master);
        return 1;
    }

    /* Write header */
    fprintf(output, "timestamp|epoch|player|turn|word|action_type\n");

    /* Read master ledger, filter to player's entries */
    char line[MAX_LINE];
    int line_num = 0;
    while (fgets(line, sizeof(line), master)) {
        line_num++;

        /* Skip header */
        if (line_num == 1) continue;

        /* Parse line: timestamp|epoch|player|turn|word|action_type */
        char timestamp[50], player_col[256], word[256], action_type[50];
        int epoch, turn;

        if (sscanf(line, "%49[^|]|%d|%255[^|]|%d|%255[^|]|%49s",
                   timestamp, &epoch, player_col, &turn, word, action_type) == 6) {

            /* Check if this entry is for our player */
            if (strcmp(player_col, player_name) == 0) {
                fprintf(output, "%s|%d|%s|%d|%s|%s\n",
                        timestamp, epoch, player_col, turn, word, action_type);
            }
        }
    }

    fclose(master);
    fclose(output);

    return 0;
}
