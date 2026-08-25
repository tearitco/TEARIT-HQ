/* destroy_card - one verb, one binary, no shared headers.
 * The other half of export_card.c's issue/destroy gate: flips a pet's
 * card_status from "issued" back to "destroyed", the local stand-in for
 * a future on-chain burn, so export_card.+x can mint a new one.
 *
 * Usage: destroy_card.+x <pet_piece_id>
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

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <pet_piece_id>\n", argv[0]);
        return 1;
    }
    const char *pet_id = argv[1];
    resolve_root();

    char state_path[PATH_BUF + 32];
    snprintf(state_path, sizeof(state_path), "%s/pieces/world_01/map_lobby/%s/state.txt", project_root, pet_id);

    FILE *f = fopen(state_path, "r");
    if (!f) { printf("Destroy failed: unknown pet %s.\n", pet_id); return 1; }

    char lines[MAX_LINES][MAX_LINE];
    int nlines = 0;
    char card_status[16] = "none";
    int seen_card_status = 0;
    while (nlines < MAX_LINES && fgets(lines[nlines], MAX_LINE, f)) {
        char *eq = strchr(lines[nlines], '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(lines[nlines], "card_status") == 0) {
                char v[16];
                snprintf(v, sizeof(v), "%s", eq + 1);
                v[strcspn(v, "\r\n")] = '\0'; /* CRLF-safe */
                snprintf(card_status, sizeof(card_status), "%s", v);
                seen_card_status = 1;
            }
            *eq = '=';
        }
        nlines++;
    }
    fclose(f);

    if (strcmp(card_status, "issued") != 0) {
        printf("No issued card to destroy for %s.\n", pet_id);
        return 1;
    }

    f = fopen(state_path, "w");
    if (!f) { printf("Destroy failed: could not write state.\n"); return 1; }
    for (int i = 0; i < nlines; i++) {
        char *eq = strchr(lines[i], '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(lines[i], "card_status") == 0) { fprintf(f, "card_status=destroyed\n"); *eq = '='; continue; }
            *eq = '=';
        }
        fputs(lines[i], f);
    }
    if (!seen_card_status) fprintf(f, "card_status=destroyed\n");
    fclose(f);

    append_ledger(pet_id, "card_status", "destroyed", "destroy_card");

    printf("Destroyed the card for %s. A new one can now be exported.\n", pet_id);
    return 0;
}
