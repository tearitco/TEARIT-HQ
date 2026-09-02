/* corp_buy_stake - a corporation buys shares IN ANOTHER corporation
 * (cross-corp ownership), spending buyer cash at target's stock_price.
 * This is the prerequisite primitive `corp_decide.c`/`corp_trade.c`
 * never needed: those ops only ever traded a corp's OWN stock
 * (treasury buyback/issue), matching the single-entity vertical slice
 * they were built for. Real M&A (SOCIETY-ECONOMY-ARCHITECTURE.txt
 * §5.5 - merger/greenmail/LBO) needs an actual cross-corp holdings
 * record, which is what this op and the `holdings.txt` file it
 * maintains are for.
 *
 * holdings.txt lives inside the BUYER's own piece directory (matching
 * this whole family's "state lives with the piece that owns it"
 * convention) - one line per holding, `<target_ticker>|<shares>`.
 *
 * Self-contained, no shared headers.
 * Usage: corp_buy_stake.+x <buyer_piece_id> <target_piece_id> <shares> */
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

/* Reads target's current holding count from buyer's holdings.txt (0 if
 * none), then rewrites the file with the updated count - same
 * read-all/rewrite-all shape write_state_field already uses for
 * state.txt, applied to a second per-piece file. */
static int get_holding(const char *holdings_path, const char *target_id) {
    FILE *f = fopen(holdings_path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    size_t target_len = strlen(target_id);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, target_id, target_len) == 0 && line[target_len] == '|') {
            fclose(f);
            return atoi(line + target_len + 1);
        }
    }
    fclose(f);
    return 0;
}

static void set_holding(const char *holdings_path, const char *target_id, int shares) {
    FILE *f = fopen(holdings_path, "r");
    char lines[256][MAX_LINE];
    int nlines = 0;
    if (f) {
        while (nlines < 256 && fgets(lines[nlines], MAX_LINE, f)) nlines++;
        fclose(f);
    }
    size_t target_len = strlen(target_id);
    f = fopen(holdings_path, "w");
    if (!f) return;
    int found = 0;
    for (int i = 0; i < nlines; i++) {
        if (strncmp(lines[i], target_id, target_len) == 0 && lines[i][target_len] == '|') {
            if (shares > 0) fprintf(f, "%s|%d\n", target_id, shares);
            found = 1;
        } else {
            fputs(lines[i], f);
        }
    }
    if (!found && shares > 0) fprintf(f, "%s|%d\n", target_id, shares);
    fclose(f);
}

int main(int argc, char *argv[]) {
    resolve_root();
    if (argc < 4) { fprintf(stderr, "Usage: corp_buy_stake.+x <buyer_id> <target_id> <shares>\n"); return 1; }
    const char *buyer_id = argv[1];
    const char *target_id = argv[2];
    int shares = atoi(argv[3]);
    if (shares <= 0) { fprintf(stderr, "shares must be positive\n"); return 1; }

    char buyer_state_path[PATH_BUF], target_state_path[PATH_BUF], holdings_path[PATH_BUF];
    snprintf(buyer_state_path, sizeof(buyer_state_path), "%s/projects/wsr-pal/pieces/%s/state.txt", project_root, buyer_id);
    snprintf(target_state_path, sizeof(target_state_path), "%s/projects/wsr-pal/pieces/%s/state.txt", project_root, target_id);
    snprintf(holdings_path, sizeof(holdings_path), "%s/projects/wsr-pal/pieces/%s/holdings.txt", project_root, buyer_id);

    char cash_str[MAX_FIELD], price_str[MAX_FIELD];
    read_state_field(buyer_state_path, "cash", cash_str, sizeof(cash_str));
    read_state_field(target_state_path, "stock_price", price_str, sizeof(price_str));
    float cash = cash_str[0] ? atof(cash_str) : 0;
    float price = price_str[0] ? atof(price_str) : 0;
    float cost = price * (float)shares;

    if (cost > cash) {
        printf("cannot buy stake: cost %.2f exceeds buyer cash %.2f\n", cost, cash);
        return 1;
    }

    cash -= cost;
    char new_cash[MAX_FIELD];
    snprintf(new_cash, sizeof(new_cash), "%.2f", cash);
    write_state_field(buyer_state_path, "cash", new_cash);

    int existing = get_holding(holdings_path, target_id);
    set_holding(holdings_path, target_id, existing + shares);

    printf("stake bought: %s now holds %d shares of %s (cash now %.2f)\n", buyer_id, existing + shares, target_id, cash);
    return 0;
}
