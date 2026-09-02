/* wsr_search_input - updates search criteria in wsr_search_menu/state.txt */
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
            snprintf(out, out_sz, "%s", v);
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

    int criterion = atoi(argv[1]);
    char search_state[PATH_BUF];
    snprintf(search_state, sizeof(search_state), "%s/projects/wsr-pal/pieces/wsr_search_menu/state.txt", project_root);

    char buf[64];
    char msg[256];

    switch (criterion) {
        case 1:
            read_state_field(search_state, "high_roe", buf, sizeof(buf));
            snprintf(msg, sizeof(msg), "High ROE: %s%% (adjust via menu)", buf);
            break;
        case 15:
            write_state_field(search_state, "high_roe", "20");
            write_state_field(search_state, "low_pe", "10");
            write_state_field(search_state, "price_pct_book", "95");
            write_state_field(search_state, "div_yield", "5");
            write_state_field(search_state, "min_market_cap", "0");
            write_state_field(search_state, "max_market_cap", "1000");
            snprintf(msg, sizeof(msg), "Search criteria reset to defaults.");
            break;
        default:
            snprintf(msg, sizeof(msg), "Criterion %d (interactive mode coming)", criterion);
    }

    printf("%s\n", msg);
    return 0;
}
