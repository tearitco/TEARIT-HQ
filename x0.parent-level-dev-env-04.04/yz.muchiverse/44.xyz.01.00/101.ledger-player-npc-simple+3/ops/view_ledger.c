/* view_ledger.c - Display master ledger contents (safe version)
 * Shows all recorded actions in chronological order
 * Self-contained */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATH 256
#define MAX_LINE 1024

int main(int argc, char **argv) {
    const char *ledger_path = "data/master_ledger.txt";
    if (argc > 1) ledger_path = argv[1];

    printf("\x1b[2J\x1b[H");
    printf("=========================================\n");
    printf("MASTER LEDGER\n");
    printf("=========================================\n\n");

    FILE *fp = fopen(ledger_path, "r");
    if (!fp) {
        printf("No ledger found.\n");
        return 1;
    }

    char line[MAX_LINE];
    int line_num = 0;
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = '\0';
        printf("%s\n", line);
        line_num++;
    }
    fclose(fp);

    printf("\n=========================================\n");
    printf("Total entries: %d\n", line_num - 1);

    return 0;
}
