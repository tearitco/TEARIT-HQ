/* corp_apply_finances - processes ONE corp's per-turn financial
 * effects: bond interest, loan interest, R&D/marketing spend, dividend
 * payout, growth reinvestment. Run once per corp per End Turn (before
 * corp_update_price.c, so this turn's spend/interest feed into that
 * turn's valuation - same ordering principle as corp_update_price.c
 * itself vs the idle/decide/trade round loop).
 *
 * None of these formulas are ported from anywhere - the original
 * source names R&D/marketing/dividend/growth as menu labels but has
 * ZERO logic behind any of them (see wsr-pal-final-handoff.txt §2).
 * These are deliberately SIMPLE, common-sense placeholder effects
 * (spend has a real cash cost; R&D nudges risk_bias; marketing nudges
 * stock_price directly; growth nudges book_value) so the numbers are
 * observable and easy to rebalance after playtesting, not a claim
 * these are realistic economic models.
 *
 * Usage: corp_apply_finances.+x <corp_piece_id> */
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

static void write_float(const char *state_path, const char *key, float value) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.2f", value);
    write_state_field(state_path, key, buf);
}

static float field_f(const char *state_path, const char *key) {
    char buf[MAX_LINE];
    read_state_field(state_path, key, buf, sizeof(buf));
    return buf[0] ? atof(buf) : 0.0f;
}

int main(int argc, char *argv[]) {
    if (argc < 2) return 1;
    resolve_root();
    const char *piece_id = argv[1];

    char corp_state[PATH_BUF], player_state[PATH_BUF];
    snprintf(corp_state, sizeof(corp_state), "%s/projects/wsr-pal/pieces/%s/state.txt", project_root, piece_id);
    snprintf(player_state, sizeof(player_state), "%s/projects/wsr-pal/pieces/player_you/state.txt", project_root);

    float cash = field_f(corp_state, "cash");
    char owned_by[MAX_LINE];
    read_state_field(corp_state, "owned_by", owned_by, sizeof(owned_by));

    /* Bond interest - simple flat rate on the outstanding balance. */
    float bonds = field_f(corp_state, "bonds_outstanding");
    if (bonds > 0) {
        float rate = field_f(corp_state, "bond_rate");
        if (rate <= 0) rate = 0.06f;
        cash -= bonds * rate;
    }

    /* Loan interest - compounds onto the balance itself (owed to the
     * lending bank, not paid automatically - see bank_loan_op.c's own
     * "repay" action for that). */
    float loan = field_f(corp_state, "loan_balance");
    if (loan > 0) {
        float rate = field_f(corp_state, "loan_rate");
        if (rate <= 0) rate = 0.08f;
        loan *= (1.0f + rate);
        write_float(corp_state, "loan_balance", loan);
    }

    /* R&D spend: real cash cost, common-sense payoff is a small,
     * capped nudge to risk_bias (the same field the real fundamental-
     * value formula already treats as "market confidence"). */
    float rnd_pct = field_f(corp_state, "rnd_pct");
    if (rnd_pct > 0) {
        float spend = cash * (rnd_pct / 100.0f);
        cash -= spend;
        int risk_bias = (int)field_f(corp_state, "risk_bias");
        if (risk_bias < 100) {
            char buf[16]; snprintf(buf, sizeof(buf), "%d", risk_bias + 1);
            write_state_field(corp_state, "risk_bias", buf);
        }
    }

    /* Marketing spend: real cash cost, common-sense payoff is a direct
     * small bump to stock_price BEFORE corp_update_price.c's own
     * formula runs this same turn (so it's visible immediately, not a
     * separate hidden variable). */
    float marketing_pct = field_f(corp_state, "marketing_pct");
    if (marketing_pct > 0) {
        float spend = cash * (marketing_pct / 100.0f);
        cash -= spend;
        float price = field_f(corp_state, "stock_price");
        write_float(corp_state, "stock_price", price * (1.0f + marketing_pct / 1000.0f));
    }

    /* Growth spend: real cash cost (reinvestment), common-sense payoff
     * is a small bump to book_value (the fundamental the valuation
     * formula is built on). */
    float growth_pct = field_f(corp_state, "growth_pct");
    if (growth_pct > 0) {
        float spend = cash * (growth_pct / 100.0f);
        cash -= spend;
        float book_value = field_f(corp_state, "book_value");
        write_float(corp_state, "book_value", book_value * (1.0f + growth_pct / 1000.0f));
    }

    /* Dividend payout: real cash cost. Only routes to the player's own
     * wallet if THEY are the owner - we don't track a full shareholder
     * registry, so a dividend on an AI/independent corp just leaves the
     * corp (honest simplification, not silently swallowed - named here). */
    float dividend_pct = field_f(corp_state, "dividend_pct");
    if (dividend_pct > 0 && cash > 0) {
        float payout = cash * (dividend_pct / 100.0f);
        cash -= payout;
        if (strcmp(owned_by, "player_you") == 0) {
            float player_cash = field_f(player_state, "cash");
            write_float(player_state, "cash", player_cash + payout);
        }
    }

    write_float(corp_state, "cash", cash);
    return 0;
}
