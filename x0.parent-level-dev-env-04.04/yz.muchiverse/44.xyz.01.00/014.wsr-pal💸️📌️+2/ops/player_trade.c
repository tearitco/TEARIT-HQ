/* player_trade - the human player's OWN buy/sell/short/cover action,
 * distinct from any corp's own decision_mode-driven trading. Inferred
 * from the real menu shape (game.c's option 12 "Buy/Sell" has no
 * working case in the real source at all - confirmed by direct
 * reading, see wsr-pal-final-handoff.txt) plus common sense about how
 * a stock-market game must actually work for a human to play it: the
 * player is their OWN economic actor with a cash balance and a stock
 * portfolio, separate from any corp they may also found/run.
 *
 * `pieces/player_you/holdings.txt` - one line per ticker held,
 * `<ticker>|<shares>`, shares may be NEGATIVE (an open short position -
 * the simplest honest way to model long vs short without a separate
 * mechanism: buy/sell move a normal positive holding, short/cover move
 * it negative and back). Trades do NOT move the target corp's own
 * stock_price - only the real ported analysis_loop.c formula
 * (corp_update_price.c) does that, once per End Turn. This is a
 * deliberate v1 simplification (no order-book price impact from player
 * trades) - flagged here, not silently decided.
 *
 * Usage: player_trade.+x <corp_piece_id> <buy|sell|short|cover> <shares> */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

/* Reads current shares held for a ticker from holdings.txt (0 if not
 * present). Rewrites the whole file with the updated value (or drops
 * the line if it nets to exactly 0), same small-file-rewrite shape
 * corp_decide.c's write_state_field already uses for state.txt. */
static int update_holdings(const char *holdings_path, const char *ticker, int delta_shares) {
    char lines[256][MAX_LINE];
    int nlines = 0;
    int current = 0;
    FILE *f = fopen(holdings_path, "r");
    if (f) {
        while (nlines < 256 && fgets(lines[nlines], MAX_LINE, f)) nlines++;
        fclose(f);
    }
    size_t ticker_len = strlen(ticker);
    int found = 0;
    f = fopen(holdings_path, "w");
    if (!f) return 0;
    for (int i = 0; i < nlines; i++) {
        char *pipe = strchr(lines[i], '|');
        if (pipe && strncmp(lines[i], ticker, ticker_len) == 0 && lines[i][ticker_len] == '|') {
            current = atoi(pipe + 1);
            int new_val = current + delta_shares;
            found = 1;
            if (new_val != 0) fprintf(f, "%s|%d\n", ticker, new_val);
            current = new_val;
        } else {
            fputs(lines[i], f);
        }
    }
    if (!found) {
        current = delta_shares;
        if (current != 0) fprintf(f, "%s|%d\n", ticker, current);
    }
    fclose(f);
    return current;
}

static void log_transaction(const char *project_root_, const char *ticker, const char *action, int shares, float price) {
    char tx_path[PATH_BUF];
    snprintf(tx_path, sizeof(tx_path), "%s/projects/wsr-pal/pieces/player_you/transactions.txt", project_root_);
    FILE *f = fopen(tx_path, "a");
    if (!f) return;
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char date_str[32];
    strftime(date_str, sizeof(date_str), "%Y-%m-%d", tm_info);
    fprintf(f, "%s|%s|%d|%.2f|%s\n", ticker, action, shares, price, date_str);
    fclose(f);
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage: player_trade.+x <corp_piece_id> <buy|sell|short|cover> <shares>\n");
        return 1;
    }
    resolve_root();
    const char *piece_id = argv[1];
    const char *action = argv[2];
    int shares = atoi(argv[3]);
    if (shares <= 0) { printf("Invalid share count.\n"); return 1; }

    const char *ticker = (strncmp(piece_id, "corp_", 5) == 0) ? piece_id + 5 : piece_id;

    char corp_state[PATH_BUF], player_state[PATH_BUF], holdings_path[PATH_BUF];
    snprintf(corp_state, sizeof(corp_state), "%s/projects/wsr-pal/pieces/%s/state.txt", project_root, piece_id);
    snprintf(player_state, sizeof(player_state), "%s/projects/wsr-pal/pieces/player_you/state.txt", project_root);
    snprintf(holdings_path, sizeof(holdings_path), "%s/projects/wsr-pal/pieces/player_you/holdings.txt", project_root);

    char buf[MAX_LINE];
    read_state_field(corp_state, "stock_price", buf, sizeof(buf));
    float price = atof(buf);
    if (price <= 0) { printf("Unknown corporation or no price data.\n"); return 1; }

    read_state_field(player_state, "cash", buf, sizeof(buf));
    float cash = atof(buf);
    float cost = price * shares;

    if (strcmp(action, "buy") == 0) {
        if (cash < cost) { printf("Insufficient cash: need $%.2f, have $%.2f.\n", cost, cash); return 1; }
        cash -= cost;
        int held = update_holdings(holdings_path, ticker, shares);
        char cash_str[64]; snprintf(cash_str, sizeof(cash_str), "%.2f", cash);
        write_state_field(player_state, "cash", cash_str);
        log_transaction(project_root, ticker, "buy", shares, price);
        printf("Bought %d shares of %s at $%.2f (cost $%.2f). You now hold %d.\n", shares, ticker, price, cost, held);
    } else if (strcmp(action, "sell") == 0) {
        char lines[256][MAX_LINE]; (void)lines;
        FILE *f = fopen(holdings_path, "r");
        int current = 0;
        if (f) {
            char line[MAX_LINE];
            size_t ticker_len = strlen(ticker);
            while (fgets(line, sizeof(line), f)) {
                if (strncmp(line, ticker, ticker_len) == 0 && line[ticker_len] == '|') {
                    current = atoi(line + ticker_len + 1);
                    break;
                }
            }
            fclose(f);
        }
        if (current < shares) { printf("You only hold %d shares of %s - can't sell %d.\n", current, ticker, shares); return 1; }
        cash += cost;
        int held = update_holdings(holdings_path, ticker, -shares);
        char cash_str[64]; snprintf(cash_str, sizeof(cash_str), "%.2f", cash);
        write_state_field(player_state, "cash", cash_str);
        log_transaction(project_root, ticker, "sell", shares, price);
        printf("Sold %d shares of %s at $%.2f (proceeds $%.2f). You now hold %d.\n", shares, ticker, price, cost, held);
    } else if (strcmp(action, "short") == 0) {
        cash += cost;
        int held = update_holdings(holdings_path, ticker, -shares);
        char cash_str[64]; snprintf(cash_str, sizeof(cash_str), "%.2f", cash);
        write_state_field(player_state, "cash", cash_str);
        log_transaction(project_root, ticker, "short", shares, price);
        printf("Opened short: sold %d shares of %s at $%.2f (proceeds $%.2f). Position now %d.\n", shares, ticker, price, cost, held);
    } else if (strcmp(action, "cover") == 0) {
        char line[MAX_LINE];
        int current = 0;
        FILE *f = fopen(holdings_path, "r");
        if (f) {
            size_t ticker_len = strlen(ticker);
            while (fgets(line, sizeof(line), f)) {
                if (strncmp(line, ticker, ticker_len) == 0 && line[ticker_len] == '|') {
                    current = atoi(line + ticker_len + 1);
                    break;
                }
            }
            fclose(f);
        }
        if (current >= 0 || -current < shares) { printf("You don't have a large enough short position in %s to cover %d.\n", ticker, shares); return 1; }
        if (cash < cost) { printf("Insufficient cash to cover: need $%.2f, have $%.2f.\n", cost, cash); return 1; }
        cash -= cost;
        int held = update_holdings(holdings_path, ticker, shares);
        char cash_str[64]; snprintf(cash_str, sizeof(cash_str), "%.2f", cash);
        write_state_field(player_state, "cash", cash_str);
        log_transaction(project_root, ticker, "cover", shares, price);
        printf("Covered %d shares of %s at $%.2f (cost $%.2f). Position now %d.\n", shares, ticker, price, cost, held);
    } else {
        printf("Unknown action '%s'.\n", action);
        return 1;
    }
    return 0;
}
