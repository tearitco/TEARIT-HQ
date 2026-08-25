/* player_settle_futures - run once per End Turn (no args), decrements
 * every open futures/options position's turns_remaining and settles
 * any that reach 0 into the player's cash. Scans corp_* pieces'
 * CURRENT stock_price at settlement time - same dynamic-discovery-by-
 * ticker lookup shape used elsewhere in this project, just against a
 * flat player-owned list instead of a directory of pieces.
 *
 * Usage: player_settle_futures.+x (no args) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_LINE 512
#define MAX_POSITIONS 256

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

static float current_price(const char *ticker) {
    char state_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/projects/wsr-pal/pieces/corp_%s/state.txt", project_root, ticker);
    char buf[MAX_LINE];
    read_state_field(state_path, "stock_price", buf, sizeof(buf));
    return buf[0] ? atof(buf) : 0.0f;
}

int main(void) {
    resolve_root();

    char futures_path[PATH_BUF], player_state[PATH_BUF];
    snprintf(futures_path, sizeof(futures_path), "%s/projects/wsr-pal/pieces/player_you/futures.txt", project_root);
    snprintf(player_state, sizeof(player_state), "%s/projects/wsr-pal/pieces/player_you/state.txt", project_root);

    FILE *f = fopen(futures_path, "r");
    if (!f) return 0; /* nothing open - not an error */

    char lines[MAX_POSITIONS][MAX_LINE];
    int n = 0;
    while (n < MAX_POSITIONS && fgets(lines[n], MAX_LINE, f)) n++;
    fclose(f);

    char buf[MAX_LINE];
    read_state_field(player_state, "cash", buf, sizeof(buf));
    float player_cash = buf[0] ? atof(buf) : 0.0f;
    int settled_count = 0;
    float total_payout = 0.0f;

    FILE *out = fopen(futures_path, "w");
    if (!out) return 1;

    for (int i = 0; i < n; i++) {
        char kind[16], ticker[32], direction[8];
        int shares, turns;
        float entry_price, strike, premium;
        if (sscanf(lines[i], "%15[^|]|%31[^|]|%7[^|]|%d|%f|%f|%f|%d",
                   kind, ticker, direction, &shares, &entry_price, &strike, &premium, &turns) != 8) {
            fputs(lines[i], out); /* malformed line - keep as-is rather than silently drop */
            continue;
        }
        turns--;
        if (turns > 0) {
            fprintf(out, "%s|%s|%s|%d|%.2f|%.2f|%.2f|%d\n", kind, ticker, direction, shares, entry_price, strike, premium, turns);
            continue;
        }
        /* Settling now. */
        float settle_price = current_price(ticker);
        float payout = 0.0f;
        if (strcmp(kind, "future") == 0) {
            payout = (settle_price - entry_price) * shares * (strcmp(direction, "long") == 0 ? 1.0f : -1.0f);
        } else if (strcmp(kind, "option") == 0) {
            if (strcmp(direction, "call") == 0) {
                float itm = settle_price - strike;
                payout = (itm > 0) ? itm * shares : 0.0f;
            } else {
                float itm = strike - settle_price;
                payout = (itm > 0) ? itm * shares : 0.0f;
            }
            /* premium already charged upfront - not deducted again here */
        }
        player_cash += payout;
        total_payout += payout;
        settled_count++;
    }
    fclose(out);

    if (settled_count > 0) {
        char cash_str[64];
        snprintf(cash_str, sizeof(cash_str), "%.2f", player_cash);
        write_state_field(player_state, "cash", cash_str);
        printf("Settled %d position(s), net payout $%.2f.\n", settled_count, total_payout);
    }
    return 0;
}
