/* human_turn.c - Display input screen for human player
 * Reads keyboard input, appends to ledger
 * Self-contained */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_PATH 256
#define MAX_LINE 512

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: human_turn <player_num> <config_path> <ledger_path>\n");
        return 1;
    }

    int player_num = atoi(argv[1]);
    const char *config_path = argv[2];
    const char *ledger_path = argv[3];

    /* Clear screen */
    printf("\x1b[2J\x1b[H");

    /* Get player name */
    FILE *fp = fopen(config_path, "r");
    char player_name[50] = "";
    snprintf(player_name, sizeof(player_name), "Player%d", player_num);

    if (fp) {
        char line[MAX_LINE];
        char key[50];
        snprintf(key, sizeof(key), "player_%d_name=", player_num);
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, key, strlen(key)) == 0) {
                char *start = strchr(line, '=');
                if (start) {
                    start++;
                    strncpy(player_name, start, sizeof(player_name) - 1);
                    player_name[strcspn(player_name, "\n")] = '\0';
                }
                break;
            }
        }
        fclose(fp);
    }

    /* Get current epoch */
    fp = fopen(config_path, "r");
    int current_epoch = 1;
    if (fp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), fp)) {
            if (sscanf(line, "current_epoch=%d", &current_epoch) == 1) break;
        }
        fclose(fp);
    }

    printf("=========================================\n");
    printf("LEDGER WORD GAME - Player %d\n", player_num);
    printf("=========================================\n\n");
    printf("Hello, %s!\n", player_name);
    printf("Epoch %d - It's your turn.\n\n", current_epoch);

    printf("Enter a word: ");
    fflush(stdout);

    char word[256];
    if (fgets(word, sizeof(word), stdin)) {
        word[strcspn(word, "\n")] = '\0';

        /* Append to ledger */
        FILE *lfp = fopen(ledger_path, "a");
        if (lfp) {
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

            fprintf(lfp, "%s|%d|%s|%d|%s|human_input\n",
                    timestamp, current_epoch, player_name, turn_count, word);
            fclose(lfp);

            printf("\nYour word '%s' has been recorded to the ledger.\n", word);
            printf("Press Enter to continue...\n");
            getchar();
        }
    }

    return 0;
}
