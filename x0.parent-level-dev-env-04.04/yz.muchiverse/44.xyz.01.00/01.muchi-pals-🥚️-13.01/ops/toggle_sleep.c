/* toggle_sleep - one verb, one binary, no shared headers.
 * Flips a pet's asleep flag. While asleep, tick_pets.c regenerates
 * energy faster and pauses movement/hunger-gain rate accordingly.
 *
 * Usage: toggle_sleep.+x <pet_piece_id>
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

/* Reads the pet's asleep flag, flips it, rewrites the file - single
 * read-modify-write pass, same shape as buy_egg.c's read_write_tokens. */
static int apply_toggle(const char *path, int *out_asleep) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    char lines[MAX_LINES][MAX_LINE];
    int nlines = 0;
    int asleep = 0;
    while (nlines < MAX_LINES && fgets(lines[nlines], MAX_LINE, f)) {
        char *eq = strchr(lines[nlines], '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(lines[nlines], "asleep") == 0) asleep = atoi(eq + 1);
            *eq = '=';
        }
        nlines++;
    }
    fclose(f);

    asleep = asleep ? 0 : 1;

    f = fopen(path, "w");
    if (!f) return 0;
    int seen = 0;
    for (int i = 0; i < nlines; i++) {
        char *eq = strchr(lines[i], '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(lines[i], "asleep") == 0) { fprintf(f, "asleep=%d\n", asleep); seen = 1; *eq = '='; continue; }
            *eq = '=';
        }
        fputs(lines[i], f);
    }
    if (!seen) fprintf(f, "asleep=%d\n", asleep);
    fclose(f);

    *out_asleep = asleep;
    return 1;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <pet_piece_id>\n", argv[0]);
        return 1;
    }
    const char *pet_id = argv[1];
    resolve_root();

    char state_path[PATH_BUF + 32];
    snprintf(state_path, sizeof(state_path), "%s/pieces/world_01/map_lobby/%s/state.txt", project_root, pet_id);

    int asleep = 0;
    if (!apply_toggle(state_path, &asleep)) {
        printf("Sleep toggle failed: unknown pet %s.\n", pet_id);
        return 1;
    }

    append_ledger(pet_id, "asleep", asleep ? "1" : "0", "toggle_sleep");

    if (asleep) printf("%s is now asleep.\n", pet_id);
    else printf("%s woke up.\n", pet_id);
    return 0;
}
