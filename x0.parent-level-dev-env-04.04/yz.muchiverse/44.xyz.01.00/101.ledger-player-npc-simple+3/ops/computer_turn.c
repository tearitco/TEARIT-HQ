/* computer_turn.c - Computer player chooses a word and appends to ledger
 * Input: player_num config_path ledger_path
 * Self-contained */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_PATH 256
#define MAX_LINE 512

static const char *words[] = {
    "hello", "world", "adventure", "serendipity", "quantum",
    "melody", "eclipse", "thunder", "whisper", "crystal",
    "voyage", "radiant", "phoenix", "cascade", "cipher",
    "aurora", "sentinel", "luminous", "tempest", "zenith"
};
#define NUM_WORDS 20

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: computer_turn <player_num> <config_path> <ledger_path>\n");
        return 1;
    }

    int player_num = atoi(argv[1]);
    const char *config_path = argv[2];
    const char *ledger_path = argv[3];

    /* Seed random with PID for some variation */
    srand(time(NULL) + player_num);

    /* Pick a random word */
    int word_idx = rand() % NUM_WORDS;
    const char *chosen_word = words[word_idx];

    printf("Player %d chooses: %s\n", player_num, chosen_word);

    /* Get current epoch and player name */
    FILE *fp = fopen(config_path, "r");
    int current_epoch = 1;
    char player_name[50] = "";
    snprintf(player_name, sizeof(player_name), "Player%d", player_num);

    if (fp) {
        char line[MAX_LINE];
        char key[50];
        snprintf(key, sizeof(key), "player_%d_name=", player_num);

        rewind(fp);
        while (fgets(line, sizeof(line), fp)) {
            if (sscanf(line, "current_epoch=%d", &current_epoch) == 1) continue;
            if (strncmp(line, key, strlen(key)) == 0) {
                char *start = strchr(line, '=');
                if (start) {
                    start++;
                    strncpy(player_name, start, sizeof(player_name) - 1);
                    player_name[strcspn(player_name, "\n")] = '\0';
                }
            }
        }
        fclose(fp);
    }

    /* Append to ledger */
    fp = fopen(ledger_path, "a");
    if (fp) {
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

        fprintf(fp, "%s|%d|%s|%d|%s|computer_auto\n",
                timestamp, current_epoch, player_name, turn_count, chosen_word);
        fclose(fp);
    }

    return 0;
}
