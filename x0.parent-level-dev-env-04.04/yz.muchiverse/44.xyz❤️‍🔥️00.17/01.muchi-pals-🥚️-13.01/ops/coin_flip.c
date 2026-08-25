/* coin_flip - one verb, one binary, no shared headers.
 * The faucet's gamble verb: stakes a fixed amount of tokens on a 50/50
 * flip - heads doubles the stake (net +stake), tails loses it (net
 * -stake). Fixed stake for v1 (no amount-entry UI yet); read the stake
 * from an argv if that gets added later.
 *
 * Usage: coin_flip.+x <owner_piece_id>
 * Prints a one-line result message to stdout. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define FLIP_STAKE 10

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static unsigned int random_seed(void) {
    unsigned int seed;
    FILE *f = fopen("/dev/urandom", "rb");
    if (f) {
        size_t n = fread(&seed, sizeof(seed), 1, f);
        fclose(f);
        if (n == 1) return seed;
    }
    return (unsigned int)(time(NULL) ^ getpid());
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <owner_piece_id>\n", argv[0]);
        return 1;
    }
    const char *owner_id = argv[1];
    resolve_root();
    srand(random_seed());

    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/world_01/map_lobby/%s/state.txt", project_root, owner_id);
    FILE *f = fopen(path, "r");
    if (!f) { printf("Coin flip failed: unknown owner.\n"); return 1; }

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

    if (tokens < FLIP_STAKE) {
        printf("Coin flip failed: need %d tokens to stake, have %d.\n", FLIP_STAKE, tokens);
        return 1;
    }

    int heads = rand() % 2;
    tokens += heads ? FLIP_STAKE : -FLIP_STAKE;

    f = fopen(path, "w");
    if (!f) { printf("Coin flip failed: could not write state.\n"); return 1; }
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

    if (heads) printf("Heads! Won %d tokens. Balance: %d\n", FLIP_STAKE, tokens);
    else printf("Tails. Lost %d tokens. Balance: %d\n", FLIP_STAKE, tokens);
    return 0;
}
