/* claim_tokens - one verb, one binary, no shared headers.
 * The faucet's "claim" verb: adds a fixed amount of tokens to an owner
 * piece's state.txt. No cooldown yet (v1 - "something is better than
 * nothing"); add one later by stamping a last_claim=<epoch> field and
 * checking it here, same self-contained pattern.
 *
 * Usage: claim_tokens.+x <owner_piece_id>
 * Prints a one-line result message to stdout for the caller (menu_input)
 * to show as last_message. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define CLAIM_AMOUNT 10

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <owner_piece_id>\n", argv[0]);
        return 1;
    }
    const char *owner_id = argv[1];
    resolve_root();

    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/world_01/map_lobby/%s/state.txt", project_root, owner_id);
    FILE *f = fopen(path, "r");
    if (!f) { printf("Claim failed: unknown owner.\n"); return 1; }

    char lines[32][MAX_LINE];
    int nlines = 0;
    int tokens = 0;
    while (nlines < 32 && fgets(lines[nlines], MAX_LINE, f)) {
        char *eq = strchr(lines[nlines], '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(lines[nlines], "tokens") == 0) tokens = atoi(eq + 1);
            *eq = '=';
        }
        nlines++;
    }
    fclose(f);

    tokens += CLAIM_AMOUNT;

    f = fopen(path, "w");
    if (!f) { printf("Claim failed: could not write state.\n"); return 1; }
    for (int i = 0; i < nlines; i++) {
        char *eq = strchr(lines[i], '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(lines[i], "tokens") == 0) { fprintf(f, "tokens=%d\n", tokens); *eq = '='; continue; }
            *eq = '=';
        }
        fputs(lines[i], f);
    }
    fclose(f);

    printf("Claimed %d tokens! Balance: %d\n", CLAIM_AMOUNT, tokens);
    return 0;
}
