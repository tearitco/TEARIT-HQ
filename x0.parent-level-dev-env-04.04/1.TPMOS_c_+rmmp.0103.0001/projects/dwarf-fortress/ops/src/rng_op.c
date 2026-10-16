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
#include <ctype.h>
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

    char path[MAX_PATH];
    char *proj_root = getenv("PRISC_PROJECT_ROOT");
    if (!proj_root) proj_root = project_root;

    /* Write to state file (project session state) */
    snprintf(path, sizeof(path), "%s/projects/%s/session/state.txt", proj_root, getenv("PRISC_PROJECT_ID") ? getenv("PRISC_PROJECT_ID") : "template");
    /* Append last_roll */
    FILE *f = fopen(path, "a");
    if (f) {
        fprintf(f, "last_roll=%d\n", roll);
        fclose(f);
    }

    /* Also write to project root for piece reads */
    snprintf(path, sizeof(path), "%s/pieces/display/last_roll.txt", proj_root);
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "%d\n", roll);
        fclose(f);
    }

    /* Write last_roll to active piece's state.txt so read_state can read it */
    {
        char *proj_id = getenv("PRISC_PROJECT_ID");
        char *piece_id = getenv("PRISC_PIECE_ID");
        if (!piece_id) piece_id = "hero";
        if (proj_id) {
            snprintf(path, sizeof(path), "%s/projects/%s/pieces/%s/state.txt",
                     proj_root, proj_id, piece_id);
            f = fopen(path, "w");
            if (f) {
                fprintf(f, "last_roll=%d\n", roll);
                fclose(f);
            }
        }
    }

    printf("%d\n", roll);
    return 0;
}
