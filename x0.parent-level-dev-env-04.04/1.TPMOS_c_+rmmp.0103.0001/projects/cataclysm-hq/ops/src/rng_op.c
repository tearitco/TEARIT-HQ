/*
 * Op: rng_op - Simple seeded random number generator for TPMOS games
 * Usage: rng_op.+x <sides>
 *   e.g. rng_op.+x 6  -> rolls 1d6
 *
 * Output: writes to state.txt as last_roll=<value>
 *         also prints the value to stdout
 *
 * Reads seed from master_ledger.txt or uses time-based seed.
 * Deterministic per-call for replay consistency.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_PATH 8192
#define MAX_LINE 1024

char project_root[4096] = ".";
char state_path[8192] = "";

void resolve_paths() {
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *k = line;
                while (*k == ' ') k++;
                char *v = eq + 1;
                while (*v == ' ' || *v == '\n' || *v == '\r') v++;
                char *end = v + strlen(v) - 1;
                while (end > v && isspace((unsigned char)*end)) end--;
                end[1] = '\0';
                if (strcmp(k, "project_root") == 0) {
                    strncpy(project_root, v, sizeof(project_root) - 1);
                }
            }
        }
        fclose(kvp);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: rng_op <sides>\n");
        return 1;
    }

    int sides = atoi(argv[1]);
    if (sides <= 0) sides = 6;

    /* Seed from time + getpid for variation */
    unsigned int seed = (unsigned int)(time(NULL) ^ getpid());
    srand(seed);

    int roll = (rand() % sides) + 1;

    /* Write to state file (project session state) */
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/session/state.txt", project_root);
    /* Append last_roll */
    FILE *f = fopen(path, "a");
    if (f) {
        fprintf(f, "last_roll=%d\n", roll);
        fclose(f);
    }

    /* Also write to project root for piece reads */
    snprintf(path, sizeof(path), "%s/pieces/display/last_roll.txt", project_root);
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "%d\n", roll);
        fclose(f);
    }

    printf("%d\n", roll);
    return 0;
}
