/* feed_pet - one verb, one binary, no shared headers.
 * Deducts FEED_COST tokens from the owner and lowers a pet's hunger.
 * Simplification vs egg-pals.txt sec.5's fuller "buy a food-item piece,
 * move it into the pet's inventory" design: feeding costs tokens
 * directly, same trivial-first spirit the doc asks for training.
 *
 * Usage: feed_pet.+x <pet_piece_id> <owner_piece_id>
 * Prints a one-line result message to stdout. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_LINE 512
#define MAX_LINES 48
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define FEED_COST 5
#define HUNGER_RELIEF 30

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void append_ledger(const char *piece_id, const char *key, const char *value, const char *trigger) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/master_ledger.txt", project_root);
    FILE *f = fopen(path, "a");
    if (!f) return;
    time_t now = time(NULL);
    struct tm tmv;
#ifdef _WIN32
    gmtime_s(&tmv, &now);
#else
    gmtime_r(&now, &tmv);
#endif
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmv);
    fprintf(f, "[%s] StateChange: %s %s %s | Trigger: %s\n", ts, piece_id, key, value, trigger);
    fclose(f);
}

static int read_write_tokens(const char *owner_id, int delta, int *out_balance) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/world_01/map_lobby/%s/state.txt", project_root, owner_id);
    FILE *f = fopen(path, "r");
    if (!f) return 0;

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

    if (tokens + delta < 0) { *out_balance = tokens; return 0; }
    tokens += delta;

    f = fopen(path, "w");
    if (!f) return 0;
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

    *out_balance = tokens;
    return 1;
}

/* Reads the pet's hunger field, applies relief, rewrites it - single
 * read-modify-write pass, same shape as buy_egg.c's read_write_tokens. */
static int apply_feed(const char *path, int *out_hunger) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    char lines[MAX_LINES][MAX_LINE];
    int nlines = 0;
    int hunger = 0, seen = 0;
    while (nlines < MAX_LINES && fgets(lines[nlines], MAX_LINE, f)) {
        char *eq = strchr(lines[nlines], '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(lines[nlines], "hunger") == 0) { hunger = atoi(eq + 1); seen = 1; }
            *eq = '=';
        }
        nlines++;
    }
    fclose(f);

    hunger -= HUNGER_RELIEF;
    if (hunger < 0) hunger = 0;

    f = fopen(path, "w");
    if (!f) return 0;
    for (int i = 0; i < nlines; i++) {
        char *eq = strchr(lines[i], '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(lines[i], "hunger") == 0) { fprintf(f, "hunger=%d\n", hunger); *eq = '='; continue; }
            *eq = '=';
        }
        fputs(lines[i], f);
    }
    if (!seen) fprintf(f, "hunger=%d\n", hunger);
    fclose(f);

    *out_hunger = hunger;
    return 1;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <pet_piece_id> <owner_piece_id>\n", argv[0]);
        return 1;
    }
    const char *pet_id = argv[1];
    const char *owner_id = argv[2];
    resolve_root();

    int balance = 0;
    if (!read_write_tokens(owner_id, -FEED_COST, &balance)) {
        printf("Feed failed: need %d tokens, have %d.\n", FEED_COST, balance);
        return 1;
    }

    char state_path[PATH_BUF + 32];
    snprintf(state_path, sizeof(state_path), "%s/pieces/world_01/map_lobby/%s/state.txt", project_root, pet_id);

    int new_hunger = 0;
    if (!apply_feed(state_path, &new_hunger)) {
        printf("Feed failed: unknown pet %s.\n", pet_id);
        read_write_tokens(owner_id, FEED_COST, &balance); /* refund */
        return 1;
    }

    char valbuf[16];
    snprintf(valbuf, sizeof(valbuf), "%d", new_hunger);
    append_ledger(pet_id, "hunger", valbuf, "feed_pet");

    printf("Fed %s. Hunger: %d. Balance: %d\n", pet_id, new_hunger, balance);
    return 0;
}
