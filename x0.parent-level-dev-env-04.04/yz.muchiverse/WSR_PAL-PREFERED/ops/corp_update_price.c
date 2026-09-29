/* corp_update_price - the REAL analysis_loop.c mechanic that had never
 * been ported: wsr-pal's own analyze_all_corporations() runs a fresh
 * calculate_new_stock_price() for EVERY corporation once per real day,
 * unconditionally - decoupled from any individual corp's own buy/sell
 * decision. corp_decide.c's fundamental_value() (same formula, ported
 * earlier) was only ever used to pick buy/sell/hold - nothing wrote a
 * new stock_price back, so prices never actually moved. This op is
 * that missing write-back, run once per corp per End Turn (BEFORE the
 * idle->decide->trade round loop, matching the real day_loop.c ->
 * analysis_loop.c ordering - a day's price is set before anyone reacts
 * to it that day).
 *
 * Also appends a price_history.txt line (turn_number,old,new,change%)
 * next to the piece's own state.txt - same shape as the real
 * corporations/generated/<ticker>/price_history.txt (date,old,new,
 * change%), just using turn_number instead of a wall-clock date since
 * that's this port's own real "day" counter (wsr_menu/state.txt). This
 * is what wsr_news_op.c reads to find the day's biggest movers.
 *
 * Self-contained, no shared headers, same helper shape as corp_decide.c.
 * Usage: corp_update_price.+x <piece_id> */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

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

/* Identical formula to corp_decide.c's fundamental_value() / the real
 * analysis_loop.c's calculate_new_stock_price() - book value/share x
 * market-cap multiplier x leverage factor x risk-bias factor, blended
 * 70% fundamentals / 30% price momentum. Kept as its own copy (not a
 * shared header) per this family's "duplicate freely" convention. */
static float calculate_new_stock_price(float book_value, float shares_outstanding,
                                        float market_cap, float debt_to_equity,
                                        int risk_bias, float current_price) {
    float book_value_per_share = (shares_outstanding > 0) ? book_value / shares_outstanding : 0.0f;

    float market_cap_multiplier = 1.0f;
    if (market_cap > 0) market_cap_multiplier = 1.0f + (log10f(market_cap) - 3) * 0.05f;

    float leverage_factor;
    if (debt_to_equity > 1.0f) leverage_factor = 1.0f - (debt_to_equity - 1.0f) * 0.1f;
    else leverage_factor = 1.0f + (0.5f - debt_to_equity) * 0.05f;

    float bias_factor = 0.5f + (risk_bias / 100.0f) * 1.0f;

    float value = book_value_per_share * market_cap_multiplier * leverage_factor * bias_factor;
    float momentum_factor = 0.7f;
    float new_price = (value * momentum_factor) + (current_price * (1.0f - momentum_factor));
    if (new_price < 0.1f) new_price = 0.1f;
    return new_price;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: corp_update_price.+x <piece_id>\n");
        return 1;
    }
    resolve_root();
    const char *piece_id = argv[1];

    char state_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/projects/wsr-pal/pieces/%s/state.txt", project_root, piece_id);

    char buf[MAX_LINE];
    read_state_field(state_path, "book_value", buf, sizeof(buf));
    float book_value = atof(buf);
    read_state_field(state_path, "shares_outstanding", buf, sizeof(buf));
    float shares_outstanding = atof(buf);
    read_state_field(state_path, "market_cap", buf, sizeof(buf));
    float market_cap = atof(buf);
    read_state_field(state_path, "debt_to_equity", buf, sizeof(buf));
    float debt_to_equity = atof(buf);
    read_state_field(state_path, "risk_bias", buf, sizeof(buf));
    int risk_bias = atoi(buf);
    read_state_field(state_path, "stock_price", buf, sizeof(buf));
    float old_price = atof(buf);

    float new_price = calculate_new_stock_price(book_value, shares_outstanding, market_cap,
                                                 debt_to_equity, risk_bias, old_price);

    char price_str[64];
    snprintf(price_str, sizeof(price_str), "%.2f", new_price);
    write_state_field(state_path, "stock_price", price_str);

    /* Recompute market_cap too, since it's a real derived field the
     * real source also treats as stock_price x shares_outstanding
     * (analysis_loop.c's analyze_corporation(), market_cap fallback
     * branch) - keeps it honest instead of going stale forever. */
    if (shares_outstanding > 0) {
        char mcap_str[64];
        snprintf(mcap_str, sizeof(mcap_str), "%.2f", new_price * shares_outstanding);
        write_state_field(state_path, "market_cap", mcap_str);
    }

    char turn_str[MAX_LINE];
    char menu_state_path[PATH_BUF];
    snprintf(menu_state_path, sizeof(menu_state_path), "%s/projects/wsr-pal/pieces/wsr_menu/state.txt", project_root);
    read_state_field(menu_state_path, "turn_number", turn_str, sizeof(turn_str));

    char hist_path[PATH_BUF];
    snprintf(hist_path, sizeof(hist_path), "%s/projects/wsr-pal/pieces/%s/price_history.txt", project_root, piece_id);
    FILE *hist_fp = fopen(hist_path, "a");
    if (hist_fp) {
        float change_percent = (old_price > 0) ? ((new_price - old_price) / old_price) * 100.0f : 0.0f;
        fprintf(hist_fp, "%s,%.2f,%.2f,%.2f%%\n", turn_str[0] ? turn_str : "0", old_price, new_price, change_percent);
        fclose(hist_fp);
    }

    return 0;
}
