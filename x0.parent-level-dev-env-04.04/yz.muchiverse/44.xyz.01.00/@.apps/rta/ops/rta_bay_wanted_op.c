/* rta_bay_wanted_op - BAY SLICE wanted level (crime lives on the bay slice, never on Centon:
 * grok ruling 2026-11-22). The ledger is the star count. add raises it by one up to the
 * RULE row `stars_max` in the rules file (no number is hard-coded here; a missing rule refuses, exit 4).
 *   rta_bay_wanted_op add <ledger.txt> <rules.pdl>
 * Rules row: RULE | stars_max | N
 * Prints: wanted=N
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

static int rule_max(const char *path, int *out) {
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;
    char line[256];
    int v, found = 0;
    while (fgets(line, sizeof line, fp))
        if (sscanf(line, "RULE | stars_max | %d", &v) == 1) { *out = v; found = 1; }
    fclose(fp);
    return found ? 0 : -1;
}

int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--describe")) {
        printf("rule rta_bay_wanted inputs ledger,rules outputs WANTED row numbers stars_max from RULE row\n");
        return 0;
    }
    if (argc != 4 || strcmp(argv[1], "add")) {
        fprintf(stderr, "usage: rta_bay_wanted_op add <ledger.txt> <rules.pdl>\n");
        return 2;
    }
    int max = 0, have = 0;
    if (rule_max(argv[3], &max) || max < 1) return 4;
    last_wanted(argv[2], &have);
    int next = have >= max ? max : have + 1;
    FILE *lf = fopen(argv[2], "a");
    if (!lf) return 3;
    fprintf(lf, "WANTED | %d | %s\n", next, have >= max ? "cap" : "add");
    fclose(lf);
    printf("wanted=%d\n", next);
    return 0;
}
