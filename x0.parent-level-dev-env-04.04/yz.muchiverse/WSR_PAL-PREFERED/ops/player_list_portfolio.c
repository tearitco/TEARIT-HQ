/* player_list_portfolio - display player's holdings with cost basis and current prices */
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

int main(int argc, char *argv[]) {
    resolve_root();
    (void)argc; (void)argv;

    char tx_path[PATH_BUF], corp_state[PATH_BUF], buf[MAX_LINE];
    snprintf(tx_path, sizeof(tx_path), "%s/projects/wsr-pal/pieces/player_you/transactions.txt", project_root);

    typedef struct { char ticker[16]; int shares; float cost_basis; float total_cost; } Holding;
    Holding holdings[256];
    int n_holdings = 0;

    FILE *f = fopen(tx_path, "r");
    if (f) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            char ticker[16], action[16], date_str[32];
            int shares;
            float price;
            if (sscanf(line, "%15[^|]|%15[^|]|%d|%f|%31s", ticker, action, &shares, &price, date_str) != 5) continue;

            int idx = -1;
            for (int i = 0; i < n_holdings; i++) {
                if (strcmp(holdings[i].ticker, ticker) == 0) { idx = i; break; }
            }
            if (idx == -1) {
                strcpy(holdings[n_holdings].ticker, ticker);
                holdings[n_holdings].shares = 0;
                holdings[n_holdings].total_cost = 0;
                idx = n_holdings++;
            }

            if (strcmp(action, "buy") == 0 || strcmp(action, "cover") == 0) {
                holdings[idx].total_cost += price * shares;
                holdings[idx].shares += shares;
            } else if (strcmp(action, "sell") == 0 || strcmp(action, "short") == 0) {
                holdings[idx].shares -= shares;
                if (holdings[idx].shares > 0) holdings[idx].total_cost -= (price * shares);
                else holdings[idx].total_cost = 0;
            }
        }
        fclose(f);
    }

    printf("YOUR PORTFOLIO\n");
    printf("%-8s  Shares   Avg Cost  Current   Total Value  Gain/Loss $  Gain/Loss %%\n", "Ticker");
    printf("--------  ------   --------  -------   -----------  -----------  -----------\n");

    float total_value = 0, total_cost = 0;
    for (int i = 0; i < n_holdings; i++) {
        if (holdings[i].shares == 0) continue;

        snprintf(corp_state, sizeof(corp_state), "%s/projects/wsr-pal/pieces/corp_%s/state.txt", project_root, holdings[i].ticker);
        read_state_field(corp_state, "stock_price", buf, sizeof(buf));
        float current_price = atof(buf);
        if (current_price <= 0) current_price = 0;

        float avg_cost = (holdings[i].shares > 0) ? (holdings[i].total_cost / holdings[i].shares) : 0;
        float position_value = current_price * holdings[i].shares;
        float gain_loss = position_value - holdings[i].total_cost;
        float gain_loss_pct = (holdings[i].total_cost > 0) ? (gain_loss / holdings[i].total_cost * 100) : 0;

        printf("%-8s  %6d   $%-7.2f  $%-6.2f   $%-10.2f  $%-10.2f  %6.2f%%\n",
               holdings[i].ticker, holdings[i].shares, avg_cost, current_price, position_value, gain_loss, gain_loss_pct);

        total_value += position_value;
        total_cost += holdings[i].total_cost;
    }

    printf("--------  ------   --------  -------   -----------  -----------  -----------\n");
    float total_gain_loss = total_value - total_cost;
    float total_gain_loss_pct = (total_cost > 0) ? (total_gain_loss / total_cost * 100) : 0;
    printf("TOTAL:                                 $%-10.2f  $%-10.2f  %6.2f%%\n", total_value, total_gain_loss, total_gain_loss_pct);

    return 0;
}
