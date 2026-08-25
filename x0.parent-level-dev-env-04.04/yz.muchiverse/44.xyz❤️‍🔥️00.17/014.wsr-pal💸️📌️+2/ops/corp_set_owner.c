/* corp_set_owner - sets/clears a corp's owned_by field, the SAME field
 * corp_attempt_merger.c already uses on a successful acquisition. This
 * is the "control" primitive behind Elect me as CEO / Resign as CEO /
 * founding a new corp (inferred from the real incorporation.c, which
 * makes a corp's founder its 100% shareholder - our simplified
 * equivalent since we don't track a full cap table). Anyone can Elect
 * only an UNOWNED corp (owned_by empty) - taking control of an
 * ALREADY-owned corp requires corp_attempt_merger.c instead (a real
 * negotiation, not a free grab).
 *
 * Usage: corp_set_owner.+x <corp_piece_id> <owner_or_empty_string> */
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
    if (argc < 3) {
        printf("Usage: corp_set_owner.+x <corp_piece_id> <owner_or_empty>\n");
        return 1;
    }
    resolve_root();
    const char *piece_id = argv[1];
    const char *new_owner = argv[2];

    char state_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/projects/wsr-pal/pieces/%s/state.txt", project_root, piece_id);

    char current_owner[MAX_LINE];
    read_state_field(state_path, "owned_by", current_owner, sizeof(current_owner));

    /* Clearing ownership (resign) or force-setting via merger flow
     * (called by corp_attempt_merger.c on real acceptance) is always
     * allowed. A fresh CLAIM (electing yourself CEO of an unowned
     * corp) is only allowed if nobody owns it yet. */
    if (new_owner[0] != '\0' && current_owner[0] != '\0' && strcmp(current_owner, new_owner) != 0) {
        printf("%s is already controlled by %s - use Attempt Acquisition instead.\n", piece_id, current_owner);
        return 1;
    }

    write_state_field(state_path, "owned_by", new_owner);
    if (new_owner[0] != '\0') printf("You are now the owner/CEO of %s.\n", piece_id);
    else printf("You resigned as owner/CEO of %s.\n", piece_id);
    return 0;
}
