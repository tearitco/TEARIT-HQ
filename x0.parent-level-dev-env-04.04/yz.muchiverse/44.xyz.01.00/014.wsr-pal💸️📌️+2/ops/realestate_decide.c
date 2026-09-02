/* realestate_decide - the "deciding" state: if occupied, collect rent
 * (pending_action=1); if vacant, lower rent to attract tenants
 * (pending_action=2) - a real, simple supply/demand-style adjustment,
 * matching this whole family's own weighted-formula pattern.
 *
 * NOTE (per direct instruction to note where new features should
 * broaden later): occupancy itself doesn't yet respond to a lowered
 * rent (no connection to `pop_downtown`'s housing demand yet) - rent
 * goes down, but nothing currently moves a tenant in. Real future
 * work, named here so it isn't lost.
 *
 * Self-contained, no shared headers.
 * Usage: realestate_decide.+x <piece_id> */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_LINE 512
#define MAX_FIELD 256

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void read_state_field(const char *state_path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(state_path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t key_len = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            char *v = line + key_len + 1;
            v[strcspn(v, "\n")] = '\0';
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(out, out_sz, "%s", v);
#pragma GCC diagnostic pop
            break;
        }
    }
    fclose(f);
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
    if (argc < 2) return 1;
    char state_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/projects/wsr-pal/pieces/%s/state.txt", project_root, argv[1]);

    char occ_str[MAX_FIELD];
    read_state_field(state_path, "occupied", occ_str, sizeof(occ_str));
    int occupied = occ_str[0] ? atoi(occ_str) : 0;

    int action = occupied ? 1 : 2;
    char action_str[8];
    snprintf(action_str, sizeof(action_str), "%d", action);
    write_state_field(state_path, "pending_action", action_str);
    write_state_field(state_path, "current_state", "2");
    printf("[weighted(real)] %s\n", occupied ? "will collect rent" : "will lower rent to attract a tenant");
    return 0;
}
