#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>

#define MAX_LINE 256
#define MAX_STOCKS 50
#define MAX_HISTORY 1000

static void to_upper(char *str) {
    for (char *p = str; *p; p++) *p = toupper(*p);
}

static void read_user_account(const char *hash, float *balance, char stocks[MAX_STOCKS][MAX_LINE], float shares[MAX_STOCKS], int *stocks_count,
                              char history_type[MAX_HISTORY][16], char history_symbol[MAX_HISTORY][MAX_LINE],
                              float history_shares[MAX_HISTORY], float history_price[MAX_HISTORY],
                              char history_time[MAX_HISTORY][32], int *history_count,
                              char last_lookup_symbol[MAX_LINE], float *last_lookup_price, char last_lookup_time[32]) {
    char filename[32];
    snprintf(filename, sizeof(filename), "usr_acc.%s.txt", hash);
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "[%s] Failed to open: %s\n", filename, strerror(errno));
        exit(1);
    }
    *balance = 0.0;
    *stocks_count = 0;
    *history_count = 0;
    last_lookup_symbol[0] = '\0';
    *last_lookup_price = 0.0;
    last_lookup_time[0] = '\0';

    char line[MAX_LINE];
    if (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = 0;
        char *line_copy = strdup(line);
        if (!line_copy) {
            fprintf(stderr, "[%s] Memory allocation failed\n", filename);
            fclose(fp);
            exit(1);
        }
        char *token = strtok(line_copy, ",");
        if (token && strcmp(token, "balance") == 0) {
            token = strtok(NULL, ",");
            if (token) *balance = atof(token);
            token = strtok(NULL, ",");
        }
        while (token && strcmp(token, "stocks") != 0) token = strtok(NULL, ",");
        if (token && strcmp(token, "stocks") == 0) {
            token = strtok(NULL, ",");
            while (token && strcmp(token, "options") != 0 && *stocks_count < MAX_STOCKS) {
                strncpy(stocks[*stocks_count], token, MAX_LINE - 1);
                to_upper(stocks[*stocks_count]);
                token = strtok(NULL, ",");
                if (!token || strcmp(token, "options") == 0) break;
                shares[*stocks_count] = atof(token);
                (*stocks_count)++;
                token = strtok(NULL, ",");
            }
        }
        while (token && strcmp(token, "history") != 0) token = strtok(NULL, ",");
        if (token && strcmp(token, "history") == 0) {
            token = strtok(NULL, ",");
            while (token && strcmp(token, "last_lookup") != 0 && *history_count < MAX_HISTORY) {
                strncpy(history_type[*history_count], token, 15);
                token = strtok(NULL, ",");
                if (!token) break;
                strncpy(history_symbol[*history_count], token, MAX_LINE - 1);
                to_upper(history_symbol[*history_count]);
                token = strtok(NULL, ",");
                if (!token) break;
                history_shares[*history_count] = atof(token);
                token = strtok(NULL, ",");
                if (!token) break;
                history_price[*history_count] = atof(token);
                token = strtok(NULL, ",");
                if (!token) break;
                strncpy(history_time[*history_count], token, 31);
                /* Uniform 7-field history rows (option ops carry
                 * expiration+strike). Stock rows use the '-' sentinel +
                 * 0.00 strike so all readers (incl. strtok-based ones, which
                 * collapse empty fields) can consume the same shape. */
                token = strtok(NULL, ","); /* expiration ('' -> skipped) */
                if (token && strcmp(token, "last_lookup") != 0) token = strtok(NULL, ","); /* strike */
                (*history_count)++;
                token = strtok(NULL, ",");
            }
        }
        if (token && strcmp(token, "last_lookup") == 0) {
            token = strtok(NULL, ",");
            if (token) strncpy(last_lookup_symbol, token, MAX_LINE - 1);
            token = strtok(NULL, ",");
            if (token) *last_lookup_price = atof(token);
            token = strtok(NULL, ",");
            if (token) strncpy(last_lookup_time, token, 31);
        }
        free(line_copy);
    }
    fclose(fp);
}

static void write_user_account(const char *hash, float balance, char stocks[MAX_STOCKS][MAX_LINE], float shares[MAX_STOCKS], int stocks_count,
                               char history_type[MAX_HISTORY][16], char history_symbol[MAX_HISTORY][MAX_LINE],
                               float history_shares[MAX_HISTORY], float history_price[MAX_HISTORY],
                               char history_time[MAX_HISTORY][32], int history_count,
                               char last_lookup_symbol[MAX_LINE], float last_lookup_price, char last_lookup_time[32]) {
    char filename[32];
    snprintf(filename, sizeof(filename), "usr_acc.%s.txt", hash);
    FILE *fp = fopen(filename, "r");
    char watchlist[256] = {0}, options[512] = {0};
    if (fp) {
        char line[MAX_LINE];
        if (fgets(line, sizeof(line), fp)) {
            line[strcspn(line, "\n")] = 0;
            char *line_copy = strdup(line);
            char *token = strtok(line_copy, ",");
            while (token && strcmp(token, "watchlist") != 0) token = strtok(NULL, ",");
            if (token && strcmp(token, "watchlist") == 0) {
                token = strtok(NULL, ",");
                while (token && strcmp(token, "stocks") != 0) {
                    strcat(watchlist, ",");
                    strcat(watchlist, token);
                    token = strtok(NULL, ",");
                }
            }
            while (token && strcmp(token, "options") != 0) token = strtok(NULL, ",");
            if (token && strcmp(token, "options") == 0) {
                token = strtok(NULL, ",");
                while (token && strcmp(token, "history") != 0) {
                    strcat(options, ",");
                    strcat(options, token);
                    token = strtok(NULL, ",");
                }
            }
            free(line_copy);
        }
        fclose(fp);
    }

    fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "[%s] Failed to open for writing\n", filename);
        exit(1);
    }
    fprintf(fp, "balance,%.2f,watchlist%s,stocks", balance, watchlist);
    for (int i = 0; i < stocks_count; i++) {
        fprintf(fp, ",%s,%.2f", stocks[i], shares[i]);
    }
    fprintf(fp, ",options%s,history", options);
    for (int i = 0; i < history_count; i++) {
        fprintf(fp, ",%s,%s,%.2f,%.2f,%s,-,0.00", history_type[i], history_symbol[i],
                history_shares[i], history_price[i], history_time[i]);
    }
    fprintf(fp, ",last_lookup,%s,%.2f,%s\n", last_lookup_symbol, last_lookup_price, last_lookup_time);
    fclose(fp);
    fprintf(stderr, "[%s] Updated: balance=%.2f, stocks=%d, history=%d\n", filename, balance, stocks_count, history_count);
}

/* Ledger player column uses the house-logged-in human user id when one is
 * active (current_login.txt), else falls back to the session hash. */
static const char *resolve_player(const char *fallback) {
    static char buf[128];
    buf[0] = '\0';
    FILE *f = fopen("pieces/system/house_root.txt", "r");
    char house_root[MAX_LINE] = "";
    if (f) {
        if (fgets(house_root, sizeof(house_root), f)) house_root[strcspn(house_root, "\r\n")] = '\0';
        fclose(f);
    }
    if (house_root[0]) {
        char login_path[384];
        snprintf(login_path, sizeof(login_path), "%s/0.user-pal👤️/00.login-signup/current_login.txt", house_root);
        FILE *lf = fopen(login_path, "r");
        if (lf) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), lf)) {
                if (strncmp(line, "current_user_id=", 16) == 0) {
                    char *v = line + 16;
                    v[strcspn(v, "\r\n")] = '\0';
                    snprintf(buf, sizeof(buf), "%s", v);
                    break;
                }
            }
            fclose(lf);
        }
    }
    if (!buf[0]) snprintf(buf, sizeof(buf), "%s", fallback);
    return buf;
}

static void append_ledger(const char *hash, const char *action_type, const char *word) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "./+x/ledger_append.+x data/master_ledger.txt 0 %s \"%s\" %s",
             resolve_player(hash), word, action_type);
    FILE *fp = popen(cmd, "r");
    if (fp) pclose(fp);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <user_hash> <symbol> <shares>\n", argv[0]);
        return 1;
    }
    char *hash = argv[1], *symbol = argv[2];
    float shares_to_buy = atof(argv[3]);
    if (shares_to_buy <= 0) {
        printf("Invalid shares amount\n");
        return 1;
    }
    to_upper(symbol);

    float balance = 0.0;
    char stocks[MAX_STOCKS][MAX_LINE] = {0};
    float shares[MAX_STOCKS] = {0};
    int stocks_count = 0;
    char history_type[MAX_HISTORY][16] = {0};
    char history_symbol[MAX_HISTORY][MAX_LINE] = {0};
    float history_shares[MAX_HISTORY] = {0};
    float history_price[MAX_HISTORY] = {0};
    char history_time[MAX_HISTORY][32] = {0};
    int history_count = 0;
    char last_lookup_symbol[MAX_LINE] = {0};
    float last_lookup_price = 0.0;
    char last_lookup_time[32] = {0};

    read_user_account(hash, &balance, stocks, shares, &stocks_count,
                      history_type, history_symbol, history_shares, history_price, history_time, &history_count,
                      last_lookup_symbol, &last_lookup_price, last_lookup_time);

    char command[128];
    snprintf(command, sizeof(command), "./+x/lookup_stock.+x %s %s", hash, symbol);
    FILE *pipe = popen(command, "r");
    if (!pipe) {
        printf("[%s] Lookup failed: %s\n", symbol, strerror(errno));
        return 1;
    }
    char output[MAX_LINE];
    float price = 0.0;
    char time_str[32] = {0};
    if (fgets(output, sizeof(output), pipe)) {
        char *price_start = strstr(output, "Current Price: ");
        if (price_start) {
            price_start += strlen("Current Price: ");
            price = atof(price_start);
            char *time_start = strstr(output, "cached, ");
            if (time_start) {
                time_start += strlen("cached, ");
                strncpy(time_str, time_start, 31);
                time_str[strcspn(time_str, "\n")] = 0;
            }
        }
    }
    pclose(pipe);
    if (price <= 0) {
        printf("[%s] Failed to fetch price\n", symbol);
        return 1;
    }

    float cost = shares_to_buy * price;
    if (cost > balance) {
        printf("[%s] Insufficient balance: $%.2f needed, $%.2f available\n", symbol, cost, balance);
        return 1;
    }

    int stock_index = -1;
    for (int i = 0; i < stocks_count; i++) {
        if (strcmp(stocks[i], symbol) == 0) {
            stock_index = i;
            break;
        }
    }
    if (stock_index == -1 && stocks_count < MAX_STOCKS) {
        stock_index = stocks_count++;
        strncpy(stocks[stock_index], symbol, MAX_LINE - 1);
    }
    if (stock_index != -1) {
        shares[stock_index] += shares_to_buy;
        balance -= cost;
    } else {
        printf("[%s] Stock limit reached\n", symbol);
        return 1;
    }

    if (history_count < MAX_HISTORY) {
        strncpy(history_type[history_count], "Buy", 15);
        strncpy(history_symbol[history_count], symbol, MAX_LINE - 1);
        history_shares[history_count] = shares_to_buy;
        history_price[history_count] = price;
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);
        strftime(history_time[history_count], sizeof(history_time[0]), "%Y-%m-%dT%H:%M:%S", tm);
        history_count++;
    }

    write_user_account(hash, balance, stocks, shares, stocks_count,
                       history_type, history_symbol, history_shares, history_price, history_time, history_count,
                       last_lookup_symbol, last_lookup_price, last_lookup_time);

    char ledger_word[128];
    snprintf(ledger_word, sizeof(ledger_word), "buy:%s:%.2f:%.2f:%.2f", symbol, shares_to_buy, price, balance);
    append_ledger(hash, "buy", ledger_word);

    printf("Bought %.2f shares of %s at $%.2f. New balance: $%.2f\n", shares_to_buy, symbol, price, balance);
    return 0;
}
