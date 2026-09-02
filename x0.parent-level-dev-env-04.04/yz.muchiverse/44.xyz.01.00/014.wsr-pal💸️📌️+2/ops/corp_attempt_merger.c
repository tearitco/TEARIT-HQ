/* corp_attempt_merger - a real WSR mechanic (SOCIETY-ECONOMY-
 * ARCHITECTURE.txt §5.5, grounded against the real manual, NOT
 * invented): acquirer offers to buy 100% of target at a premium over
 * target's current stock_price, resolved by a probabilistic
 * counterparty-acceptance check (higher premium -> higher odds),
 * matching real WSR's own documented shape more closely than a fixed
 * threshold would - this is a negotiation, not a formula lookup.
 *
 * SIMPLIFICATIONS, named honestly rather than silently assumed:
 *  - Acceptance probability = min(1.0, premium_pct / 60.0) - a simple
 *    linear approximation of real WSR's own (undocumented in detail)
 *    shareholder-vote/premium-based acceptance odds, not a claimed
 *    exact replica.
 *  - Antitrust/government approval is ALWAYS granted in this version -
 *    a real gate (per §5.5's own note that it "reuses a gov_* piece's
 *    own existing state") is real future work, not implemented here.
 *  - On success, target is NOT financially absorbed into acquirer
 *    (no balance-sheet merging) - target keeps its own state.txt,
 *    only gains an `owned_by` field. Real subsidiary-financials
 *    consolidation is real future work.
 *
 * Self-contained, no shared headers.
 * Usage: corp_attempt_merger.+x <acquirer_id> <target_id> <premium_pct> */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

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
    if (argc < 4) { fprintf(stderr, "Usage: corp_attempt_merger.+x <acquirer_id> <target_id> <premium_pct>\n"); return 1; }
    const char *acquirer_id = argv[1];
    const char *target_id = argv[2];
    float premium_pct = atof(argv[3]);
    if (premium_pct < 5.0f || premium_pct > 100.0f) {
        fprintf(stderr, "premium_pct must be between 5 and 100 (real WSR's own range)\n");
        return 1;
    }

    char acquirer_state_path[PATH_BUF], target_state_path[PATH_BUF];
    snprintf(acquirer_state_path, sizeof(acquirer_state_path), "%s/projects/wsr-pal/pieces/%s/state.txt", project_root, acquirer_id);
    snprintf(target_state_path, sizeof(target_state_path), "%s/projects/wsr-pal/pieces/%s/state.txt", project_root, target_id);

    char cash_str[MAX_FIELD], price_str[MAX_FIELD], shares_out_str[MAX_FIELD], owned_by[MAX_FIELD];
    read_state_field(acquirer_state_path, "cash", cash_str, sizeof(cash_str));
    read_state_field(target_state_path, "stock_price", price_str, sizeof(price_str));
    read_state_field(target_state_path, "shares_outstanding", shares_out_str, sizeof(shares_out_str));
    read_state_field(target_state_path, "owned_by", owned_by, sizeof(owned_by));

    if (owned_by[0]) {
        printf("merger failed: %s is already owned by %s\n", target_id, owned_by);
        return 1;
    }

    float cash = cash_str[0] ? atof(cash_str) : 0;
    float price = price_str[0] ? atof(price_str) : 0;
    float shares_outstanding = shares_out_str[0] ? atof(shares_out_str) : 0;
    /* shares_outstanding is stored in millions (matching the real
     * migrated data's own unit, e.g. ORB's "11.24 million") - total
     * cost is priced per-share x total shares, same units throughout. */
    float offer_price = price * (1.0f + premium_pct / 100.0f);
    float total_cost = offer_price * shares_outstanding;

    if (total_cost > cash) {
        printf("merger failed: total cost %.2f exceeds acquirer cash %.2f (offer price %.2f/share x %.2f shares)\n",
               total_cost, cash, offer_price, shares_outstanding);
        return 1;
    }

    float accept_prob = premium_pct / 60.0f;
    if (accept_prob > 1.0f) accept_prob = 1.0f;

    srand((unsigned int)(time(NULL) ^ getpid()));
    float roll = (float)rand() / (float)RAND_MAX;

    if (roll > accept_prob) {
        printf("merger REJECTED: %s offered %s a %.1f%% premium ($%.2f/share, accept odds %.0f%%), rolled %.2f - not accepted\n",
               acquirer_id, target_id, premium_pct, offer_price, accept_prob * 100.0f, roll);
        return 0;
    }

    cash -= total_cost;
    char new_cash[MAX_FIELD];
    snprintf(new_cash, sizeof(new_cash), "%.2f", cash);
    write_state_field(acquirer_state_path, "cash", new_cash);
    write_state_field(target_state_path, "owned_by", acquirer_id);

    printf("merger ACCEPTED: %s acquired %s for %.2f total (%.1f%% premium, $%.2f/share) - acquirer cash now %.2f\n",
           acquirer_id, target_id, total_cost, premium_pct, offer_price, cash);
    return 0;
}
