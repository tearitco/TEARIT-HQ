/* tax_loop - yearly tax collection for wsr-pal.
 *
 * Fires on the calendar's year boundary (schedule row 1_year).
 * For each government, collects tax_rate_adj percent of cash
 * from every corporation and the player, crediting the government.
 *
 * This replaces the gov_trade.c self-inflation (revenue += revenue * 0.01f)
 * with real money movement. The government's revenue field is updated
 * to reflect the actual collections.
 *
 * A government's tax_rate_adj is expressed in percentage points
 * (1.0 = 1% of cash). An entity with cash=1000 and tax_rate_adj=5
 * pays 50.00.
 *
 * Self-contained, no shared headers.
 * Usage: tax_loop.+x (no arguments) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <math.h>
#ifdef _WIN32
#include <windows.h>
#include <process.h>
#define getpid _getpid
#define popen _popen
#define pclose _pclose
#else
#include <unistd.h>
#endif

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif
#define PATH_BUF (MAX_PATH + 256)
#define MAX_LINE 512
#define MAX_FIELD 256
#define MAX_PIECES 512

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

static int write_state_field(const char *state_path, const char *key, const char *value) {
    FILE *f = fopen(state_path, "r");
    char lines[64][MAX_LINE];
    int nlines = 0;
    if (f) {
        while (nlines < 64 && fgets(lines[nlines], MAX_LINE, f)) nlines++;
        fclose(f);
    }
    size_t key_len = strlen(key);
    f = fopen(state_path, "w");
    if (!f) return 0;
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
    return fclose(f) == 0;
}

static float field_f(const char *state_path, const char *key) {
    char buf[MAX_LINE];
    read_state_field(state_path, key, buf, sizeof(buf));
    return buf[0] ? (float)atof(buf) : 0.0f;
}

static void write_float(const char *state_path, const char *key, float value) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.2f", value);
    write_state_field(state_path, key, buf);
}

static int file_exists(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    fclose(f);
    return 1;
}

static void state_path(char *out, size_t out_sz, const char *piece_id) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(out, out_sz, "%s/projects/wsr-pal/pieces/%s/state.txt", project_root, piece_id);
#pragma GCC diagnostic pop
}

static int is_taxable(const char *piece_id) {
    return (strncmp(piece_id, "corp_", 5) == 0 || strncmp(piece_id, "player_", 7) == 0);
}

static int is_government(const char *piece_id) {
    return strncmp(piece_id, "gov_", 4) == 0;
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    resolve_root();

    char pieces_dir[PATH_BUF];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(pieces_dir, sizeof(pieces_dir), "%s/projects/wsr-pal/pieces", project_root);
#pragma GCC diagnostic pop

    DIR *d = opendir(pieces_dir);
    if (!d) {
        fprintf(stderr, "cannot open pieces dir: %s\n", pieces_dir);
        return 1;
    }

    /* Collect piece ids in two passes: first governments, then all
     * taxable entities. This lets us tax every entity under every
     * government (multi-jurisdiction model - each government applies
     * its own rate independently). */
    char govs[MAX_PIECES][64];
    int n_govs = 0;
    char taxable[MAX_PIECES][64];
    int n_taxable = 0;

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        char st[PATH_BUF];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(st, sizeof(st), "%s/%s/state.txt", pieces_dir, ent->d_name);
#pragma GCC diagnostic pop
        if (!file_exists(st)) continue;
        if (is_government(ent->d_name) && n_govs < MAX_PIECES) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(govs[n_govs++], sizeof(govs[0]), "%s", ent->d_name);
#pragma GCC diagnostic pop
        } else if (is_taxable(ent->d_name) && n_taxable < MAX_PIECES) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(taxable[n_taxable++], sizeof(taxable[0]), "%s", ent->d_name);
#pragma GCC diagnostic pop
        }
    }
    closedir(d);

    if (n_govs == 0) {
        printf("tax_loop: no governments found, nothing to collect\n");
        return 0;
    }

    printf("tax_loop: %d governments, %d taxable entities\n", n_govs, n_taxable);

    for (int g = 0; g < n_govs; g++) {
        char gov_state[PATH_BUF];
        state_path(gov_state, sizeof(gov_state), govs[g]);

        float tax_rate = field_f(gov_state, "tax_rate_adj");
        if (tax_rate <= 0.0f) {
            printf("tax_loop: %s tax_rate_adj=%.1f, skipping\n", govs[g], tax_rate);
            continue;
        }

        float spending = field_f(gov_state, "spending");
        float gov_cash = field_f(gov_state, "cash");
        float total_collected = 0.0f;
        int n_paid = 0;

        for (int t = 0; t < n_taxable; t++) {
            char ent_state[PATH_BUF];
            state_path(ent_state, sizeof(ent_state), taxable[t]);

            float ent_cash = field_f(ent_state, "cash");
            if (ent_cash <= 0.0f) continue;

            float tax = ent_cash * (tax_rate / 100.0f);
            if (tax < 0.01f) continue;

            ent_cash -= tax;
            char buf[64];
            snprintf(buf, sizeof(buf), "%.2f", ent_cash);
            write_state_field(ent_state, "cash", buf);

            total_collected += tax;
            n_paid++;
        }

        gov_cash += total_collected;
        write_float(gov_state, "cash", gov_cash);
        write_float(gov_state, "revenue", total_collected);
        write_float(gov_state, "net_operating", total_collected - spending);

        printf("tax_loop: %s collected %.2f from %d entities (rate=%.1f%%), gov cash now %.2f\n",
               govs[g], total_collected, n_paid, tax_rate, gov_cash);
    }

    return 0;
}
