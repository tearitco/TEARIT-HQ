/* rta_wanted_op — Centon wanted level. The ledger is the star count.
 * Stars run from 0 to 5. add raises the count by one and stops at 5.
 *   rta_wanted_op add <ledger.txt>
 * Prints: wanted N
 */
#include <stdio.h>
#include <string.h>

static int last_wanted(const char *path, int *n) {
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;
    char line[256];
    int found = 0, v;
    while (fgets(line, sizeof line, fp))
        if (sscanf(line, "WANTED | %d", &v) == 1) { *n = v; found = 1; }
    fclose(fp);
    return found ? 0 : -1;
}

int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--describe")) {
        printf("rule rta_wanted inputs ledger outputs WANTED row numbers max 5\n");
        return 0;
    }
    if (argc != 3 || strcmp(argv[1], "add")) {
        fprintf(stderr, "usage: rta_wanted_op add <ledger.txt>\n");
        return 2;
    }
    int have = 0;
    last_wanted(argv[2], &have);
    int next = have >= 5 ? 5 : have + 1;
    FILE *lf = fopen(argv[2], "a");
    if (!lf) return 3;
    fprintf(lf, "WANTED | %d | %s\n", next, have >= 5 ? "cap" : "add");
    fclose(lf);
    printf("wanted=%d\n", next);
    return 0;
}
