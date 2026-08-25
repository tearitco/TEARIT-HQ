/* corp_action - real logic for the management/financing menu options
 * that were pure printf stubs in the ORIGINAL source (management.c
 * has NO scanf/switch at all behind Set Dividend/Set R&D Spend/Set
 * Marketing Spend/Set Growth Rate/Toggle AutoPilot; financing.c's own
 * switch only implements Startup New Corp, everything else falls to
 * "not yet implemented" - see wsr-pal-final-handoff.txt §2). These are
 * inferred, common-sense implementations - simple by design so they're
 * easy to observe and adjust once played, not a claim these are the
 * "correct" real-WSR formulas (no such formulas exist to port, they
 * were never built in the original game).
 *
 * All actions here require the acting player to actually own/control
 * the corp (owned_by == player_you) - can't manage a corp you don't
 * run. corp_apply_finances.c (a separate op) is what actually SPENDS
 * the R&D/marketing/dividend percentages set here, once per End Turn.
 *
 * Usage: corp_action.+x <corp_piece_id> <action> [amount_or_pct]
 * actions: capital_contribution|public_offering|issue_bonds|
 *          buyback_bonds|extraordinary_dividend (need amount)
 *          set_dividend|set_rnd|set_marketing|set_growth (need pct)
 *          toggle_autopilot (no arg) */
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

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: corp_action.+x <corp_piece_id> <action> [value]\n");
        return 1;
    }
    resolve_root();
    const char *piece_id = argv[1];
    const char *action = argv[2];
    float value = (argc >= 4) ? atof(argv[3]) : 0.0f;

    char corp_state[PATH_BUF], player_state[PATH_BUF];
    snprintf(corp_state, sizeof(corp_state), "%s/projects/wsr-pal/pieces/%s/state.txt", project_root, piece_id);
    snprintf(player_state, sizeof(player_state), "%s/projects/wsr-pal/pieces/player_you/state.txt", project_root);

    char owned_by[MAX_LINE];
    read_state_field(corp_state, "owned_by", owned_by, sizeof(owned_by));
    if (strcmp(owned_by, "player_you") != 0) {
        printf("You do not control %s - can't manage it.\n", piece_id);
        return 1;
    }

    char buf[MAX_LINE];
    read_state_field(corp_state, "cash", buf, sizeof(buf));
    float corp_cash = atof(buf);
    read_state_field(player_state, "cash", buf, sizeof(buf));
    float player_cash = atof(buf);

    if (strcmp(action, "capital_contribution") == 0) {
        if (player_cash < value) { printf("You don't have $%.2f to contribute.\n", value); return 1; }
        write_float(player_state, "cash", player_cash - value);
        write_float(corp_state, "cash", corp_cash + value);
        printf("Contributed $%.2f of your own cash into %s.\n", value, piece_id);

    } else if (strcmp(action, "public_offering") == 0) {
        read_state_field(corp_state, "stock_price", buf, sizeof(buf));
        float price = atof(buf);
        if (price <= 0) { printf("No stock price on record - can't price a new offering.\n"); return 1; }
        float new_shares = value / price;
        read_state_field(corp_state, "shares_outstanding", buf, sizeof(buf));
        float shares_outstanding = atof(buf) + new_shares;
        write_float(corp_state, "cash", corp_cash + value);
        write_float(corp_state, "shares_outstanding", shares_outstanding);
        printf("Issued %.2fM new shares at $%.2f, raised $%.2f. Existing holders are now diluted.\n", new_shares, price, value);

    } else if (strcmp(action, "issue_bonds") == 0) {
        read_state_field(corp_state, "bonds_outstanding", buf, sizeof(buf));
        float bonds = atof(buf) + value;
        write_float(corp_state, "cash", corp_cash + value);
        write_float(corp_state, "bonds_outstanding", bonds);
        write_state_field(corp_state, "bond_rate", "0.06");
        printf("Issued $%.2fM in bonds at 6%% - raised $%.2f in cash.\n", value, value);

    } else if (strcmp(action, "buyback_bonds") == 0) {
        read_state_field(corp_state, "bonds_outstanding", buf, sizeof(buf));
        float bonds = atof(buf);
        if (bonds <= 0) { printf("%s has no outstanding bonds to call.\n", piece_id); return 1; }
        float pay = (value > bonds) ? bonds : value;
        if (corp_cash < pay) { printf("Not enough cash on hand ($%.2f) to call $%.2f of bonds.\n", corp_cash, pay); return 1; }
        write_float(corp_state, "cash", corp_cash - pay);
        write_float(corp_state, "bonds_outstanding", bonds - pay);
        printf("Called $%.2fM of bonds early, using corporate cash.\n", pay);

    } else if (strcmp(action, "extraordinary_dividend") == 0) {
        if (corp_cash < value) { printf("%s only has $%.2f cash - can't pay out $%.2f.\n", piece_id, corp_cash, value); return 1; }
        write_float(corp_state, "cash", corp_cash - value);
        write_float(player_state, "cash", player_cash + value);
        printf("Paid an extraordinary dividend of $%.2f from %s to you.\n", value, piece_id);

    } else if (strcmp(action, "set_dividend") == 0 || strcmp(action, "set_rnd") == 0 ||
               strcmp(action, "set_marketing") == 0 || strcmp(action, "set_growth") == 0) {
        const char *field = strcmp(action, "set_dividend") == 0 ? "dividend_pct" :
                             strcmp(action, "set_rnd") == 0 ? "rnd_pct" :
                             strcmp(action, "set_marketing") == 0 ? "marketing_pct" : "growth_pct";
        read_state_field(corp_state, field, buf, sizeof(buf));
        float pct = atof(buf) + value; /* value is a delta here, e.g. +10 */
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
        write_float(corp_state, field, pct);
        printf("%s now set to %.0f%% of cash per turn.\n", field, pct);

    } else if (strcmp(action, "toggle_autopilot") == 0) {
        read_state_field(corp_state, "decision_mode", buf, sizeof(buf));
        int mode = atoi(buf);
        int new_mode = (mode == 4) ? 1 : 4; /* human <-> weighted */
        char mode_str[8];
        snprintf(mode_str, sizeof(mode_str), "%d", new_mode);
        write_state_field(corp_state, "decision_mode", mode_str);
        printf("%s is now %s.\n", piece_id, new_mode == 4 ? "under YOUR manual control" : "on AutoPilot (AI-managed)");

    } else {
        printf("Unknown action '%s'.\n", action);
        return 1;
    }
    return 0;
}
