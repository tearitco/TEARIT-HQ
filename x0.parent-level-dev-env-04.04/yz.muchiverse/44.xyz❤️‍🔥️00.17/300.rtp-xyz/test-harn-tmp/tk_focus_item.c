#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
static int parse_item_number(const char *line) {
    const char *bracket = strchr(line, ']');
    if (!bracket) return -1;
    const char *p = bracket + 1;
    while (*p == ' ' || *p == '\t') p++;
    if (!isdigit((unsigned char)*p)) return -1;
    int n = atoi(p);
    while (isdigit((unsigned char)*p)) p++;
    if (*p != '.') return -1;
    return n;
}
int main(int argc, char **argv) {
    if (argc < 4) return 1;
    FILE *f = fopen(argv[2], "r");
    if (!f) return 1;
    char line[512]; int found = -1;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, argv[3])) { int n = parse_item_number(line); if (n > 0) { found = n; break; } }
    }
    fclose(f);
    if (found < 0) return 1;
    char numbuf[16]; snprintf(numbuf, sizeof(numbuf), "%d", found);
    FILE *hf = fopen(argv[1], "a");
    for (char *d = numbuf; *d; d++) { fprintf(hf, "[2026-07-31 00:00:00] KEY_PRESSED: %d\n", (int)(unsigned char)*d); }
    fclose(hf);
    printf("%d\n", found);
    return 0;
}
