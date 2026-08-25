/* ops/list_curricula.c - Supervisor op: print the curricula manifest.
 * Runs entirely on THIS box - curricula.pdl is a tiny pointer table
 * (storage rule: manifest here, payload on nodes). Reads the pdl section
 * rows and prints them with a one-line-per-curriculum summary plus a
 * per-key value listing for the harness.
 *
 * Usage: list_curricula.+x [manifest_path]
 *   manifest_path - optional; default resolves relative to PRISC_PROJECT_ROOT
 *                   -> pieces/registry/curricula.pdl (house convention)
 *
 * Output (pdl-ish, grep-friendly):
 *   CURRICULA|name=<name>|topic=...|node=...|status=...|report=...
 * one line per CURRICULUM row found.
 *
 * Self-contained, house style. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATH 4096
#define MAX_LINE 4096

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r')) *--end = '\0';
    return s;
}

int main(int argc, char *argv[]) {
    char path[MAX_PATH] = {0};
    if (argc > 1) {
        snprintf(path, sizeof(path), "%s", argv[1]);
    } else {
        const char *root = getenv("PRISC_PROJECT_ROOT");
        if (root && root[0])
            snprintf(path, sizeof(path), "%s/pieces/registry/curricula.pdl", root);
        else
            snprintf(path, sizeof(path), "pieces/registry/curricula.pdl");
    }

    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "Cannot open manifest %s\n", path); return 1; }

    char line[MAX_LINE];
    int rows = 0;
    while (fgets(line, sizeof(line), f)) {
        char *s = trim(line);
        if (s[0] == '#' || s[0] == '\0' || s[0] == '-') continue;
        if (strncmp(s, "SECTION", 7) == 0 && strstr(s, "CURRICULUM")) continue;
        if (strncmp(s, "CURRICULUM", 10) != 0) continue;

        char *key = s;
        char *p = strstr(s, "|");
        if (!p) continue;
        *p = '\0';
        key = trim(key);
        char *val = trim(p + 1);
        if (strlen(val) == 0) continue;

        printf("CURRICULA|%s\n", val);
        rows++;
    }
    fclose(f);

    printf("CURRICULA_SUM|manifest=%s|rows=%d\n", path, rows);
    return rows ? 0 : 2;
}
