/* ledger_append.c - Append action to master ledger
 * Input: epoch player_num word action_type
 * Output: appends to master_ledger.txt
 * Self-contained */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_PATH 256
#define MAX_LINE 1024

int main(int argc, char **argv) {
    if (argc < 5) {
        fprintf(stderr, "Usage: ledger_append <ledger_path> <epoch> <player> <word> <action_type>\n");
        return 1;
    }

    const char *ledger_path = argv[1];
    int epoch = atoi(argv[2]);
    const char *player = argv[3];
    const char *word = argv[4];
    const char *action_type = argv[5];

    /* Generate timestamp */
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[50];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", tm_info);

    /* Count existing turns in ledger to get turn number */
    FILE *fp = fopen(ledger_path, "r");
    int turn_count = -1;  /* -1 because of header line */
    if (fp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), fp)) {
            turn_count++;
        }
        fclose(fp);
    } else {
        turn_count = 0;
    }

    /* Append to ledger */
    fp = fopen(ledger_path, "a");
    if (!fp) {
        fprintf(stderr, "Error: cannot open ledger for append\n");
        return 1;
    }

    fprintf(fp, "%s|%d|%s|%d|%s|%s\n",
            timestamp, epoch, player, turn_count, word, action_type);

    fclose(fp);

    printf("Appended: epoch=%d, player=%s, word=%s, turn=%d\n",
           epoch, player, word, turn_count);

    return 0;
}
