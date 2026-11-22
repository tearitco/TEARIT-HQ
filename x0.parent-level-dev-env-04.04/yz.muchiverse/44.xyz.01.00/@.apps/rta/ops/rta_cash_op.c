/* rta_cash_op — Centon cash. The ledger is the balance.
 * A missing CASH row starts at 0. add sums a positive integer onto it.
 *   rta_cash_op add <ledger.txt> <amount>
 * Prints: cash N
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int last_cash(const char *path, int *n) {
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;
    char line[256];
    int found = 0, v;
    while (fgets(line, sizeof line, fp))
        if (sscanf(line, "CASH | %d", &v) == 1) { *n = v; found = 1; }
    fclose(fp);
    return found ? 0 : -1;
}

int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--describe")) {
        printf("rule rta_cash inputs ledger,amount outputs CASH row numbers amount\n");
        return 0;
    }
    if (argc != 4 || strcmp(argv[1], "add")) {
        fprintf(stderr, "usage: rta_cash_op add <ledger.txt> <amount>\n");
        return 2;
    }
    int add = atoi(argv[3]);
    if (add < 1 || add > 1000000) return 3;
    int have = 0;
    last_cash(argv[2], &have);
    int next = have + add;
    FILE *lf = fopen(argv[2], "a");
    if (!lf) return 4;
    fprintf(lf, "CASH | %d | add %d\n", next, add);
    fclose(lf);
    printf("cash=%d\n", next);
    return 0;
}
