/* corp_trade - the "trading" state's op: reads pending_action
 * (0=hold 1=buy 2=sell, written by corp_decide.+x) and executes it -
 * buy: cash -= price, shares_held += 1; sell: cash += price,
 * shares_held -= 1; hold: no-op. Returns to idle (current_state 2 -> 0).
 * Real op, not a stub - the actual trade execution, matching muchi-
 * evo-pal's bot_eat.c/bot_evolve.c shape.
 * Self-contained, no shared headers.
 * Usage: corp_trade.+x <piece_id> */
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

int main(int argc, char *argv[]) {
    resolve_root();
    if (argc < 2) return 1;
    char state_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/projects/wsr-pal/pieces/%s/state.txt", project_root, argv[1]);

    char action_str[MAX_FIELD], cash_str[MAX_FIELD], price_str[MAX_FIELD], shares_str[MAX_FIELD];
    read_state_field(state_path, "pending_action", action_str, sizeof(action_str));
    read_state_field(state_path, "cash", cash_str, sizeof(cash_str));
    read_state_field(state_path, "stock_price", price_str, sizeof(price_str));
    read_state_field(state_path, "shares_held", shares_str, sizeof(shares_str));

    int action = action_str[0] ? atoi(action_str) : 0;
    float cash = cash_str[0] ? atof(cash_str) : 0;
    float price = price_str[0] ? atof(price_str) : 0;
    int shares_held = shares_str[0] ? atoi(shares_str) : 0;
    const char *label;

    if (action == 1 && cash >= price) {
        cash -= price;
        shares_held += 1;
        label = "buy";
    } else if (action == 2 && shares_held > 0) {
        cash += price;
        shares_held -= 1;
        label = "sell";
    } else {
        label = "hold";
    }

    char new_cash[MAX_FIELD], new_shares[MAX_FIELD];
    snprintf(new_cash, sizeof(new_cash), "%.2f", cash);
    snprintf(new_shares, sizeof(new_shares), "%d", shares_held);
    write_state_field(state_path, "cash", new_cash);
    write_state_field(state_path, "shares_held", new_shares);
    write_state_field(state_path, "pending_action", "");
    write_state_field(state_path, "last_action", label);
    write_state_field(state_path, "current_state", "0");
    printf("traded: %s - cash now %.2f, shares_held now %d\n", label, cash, shares_held);
    return 0;
}
