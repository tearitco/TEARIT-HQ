/* corp_set_human_decision - a human's manual "buy"/"sell"/"hold" trade
 * choice, queued into state.txt for the next tick's corp_decide.+x
 * (decision_mode=4) to consume. Not called from any .pal script -
 * invoked directly by button.sh's `choose` action. Same shape as
 * muchi-evo-pal's bot_set_human_decision.c.
 * Self-contained, no shared headers.
 * Usage: corp_set_human_decision.+x <piece_id> <buy|sell|hold> */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_LINE 512

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void write_state_field(const char *state_path, const char *key, const char *value) {
    FILE *f = fopen(state_path, "r");
    char lines[64][MAX_LINE];
    int nlines = 0;
    if (f) {
        while (nlines < 64 && fgets(lines[nlines], MAX_LINE, f)) nlines++;
        fclose(f);
    }
    size_t key_len = strlen(key);
    f = fopen(state_path, "w");
    if (!f) return;
    int found = 0;
    for (int i = 0; i < nlines; i++) {
        if (strncmp(lines[i], key, key_len) == 0 && lines[i][key_len] == '=') {
            fprintf(f, "%s=%s\n", key, value);
            found = 1;
        } else {
            fputs(lines[i], f);
        }
    }
    if (!found) fprintf(f, "%s=%s\n", key, value);
    fclose(f);
}

int main(int argc, char *argv[]) {
    resolve_root();
    if (argc < 3) { fprintf(stderr, "Usage: corp_set_human_decision.+x <piece_id> <buy|sell|hold>\n"); return 1; }
    if (strcmp(argv[2], "buy") != 0 && strcmp(argv[2], "sell") != 0 && strcmp(argv[2], "hold") != 0) {
        fprintf(stderr, "decision must be 'buy', 'sell', or 'hold'\n");
        return 1;
    }
    char state_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/projects/wsr-pal/pieces/%s/state.txt", project_root, argv[1]);
    write_state_field(state_path, "human_decision", argv[2]);
    printf("queued human decision: %s\n", argv[2]);
    return 0;
}
