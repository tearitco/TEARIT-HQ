#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>

#define MAX_LINE 256
#define MAX_WATCHLIST 50
#define MAX_STOCKS 50
#define MAX_OPTIONS 50
#define MAX_HISTORY 1000

static void to_upper(char *str) {
    for (char *p = str; *p; p++) *p = toupper(*p);
}

static void read_user_account(const char *hash, float *balance, char watchlist[MAX_WATCHLIST][MAX_LINE], int *watchlist_count, 
                              char stocks[MAX_STOCKS][MAX_LINE], float shares[MAX_STOCKS], int *stocks_count,
                              char options_symbol[MAX_OPTIONS][MAX_LINE], char options_type[MAX_OPTIONS][16],
                              float options_contracts[MAX_OPTIONS], float options_strike[MAX_OPTIONS],
                              char options_expiry[MAX_OPTIONS][32], int *options_count,
                              char history_type[MAX_HISTORY][16], char history_symbol[MAX_HISTORY][MAX_LINE],
                              float history_shares[MAX_HISTORY], float history_price[MAX_HISTORY], 
                              char history_time[MAX_HISTORY][32], char history_expiration[MAX_HISTORY][32],
                              float history_strike[MAX_HISTORY], int *history_count,
                              char last_lookup_symbol[MAX_LINE], float *last_lookup_price, char last_lookup_time[32]) {
    char filename[32];
    snprintf(filename, sizeof(filename), "usr_acc.%s.txt", hash);
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "[%s] No user account file\n", filename);
        exit(1);
    }
    *balance = 0.0;
    *watchlist_count = 0;
    *stocks_count = 0;
    *options_count = 0;
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
        }
        token = strtok(NULL, ",");
        if (token && strcmp(token, "watchlist") == 0) {
            while ((token = strtok(NULL, ",")) && *watchlist_count < MAX_WATCHLIST) {
                if (strcmp(token, "stocks") == 0) break;
                strncpy(watchlist[*watchlist_count], token, MAX_LINE - 1);
                to_upper(watchlist[*watchlist_count]);
                (*watchlist_count)++;
            }
        }
        if (token && strcmp(token, "stocks") == 0) {
            while ((token = strtok(NULL, ",")) && *stocks_count < MAX_STOCKS) {
                if (strcmp(token, "options") == 0) break;
                strncpy(stocks[*stocks_count], token, MAX_LINE - 1);
                to_upper(stocks[*stocks_count]);
                token = strtok(NULL, ",");
                if (token) {
                    shares[*stocks_count] = atof(token);
                    (*stocks_count)++;
                }
            }
        }
        if (token && strcmp(token, "options") == 0) {
            while ((token = strtok(NULL, ",")) && *options_count < MAX_OPTIONS) {
                if (strcmp(token, "history") == 0) break;
                strncpy(options_symbol[*options_count], token, MAX_LINE - 1);
                to_upper(options_symbol[*options_count]);
                token = strtok(NULL, ",");
                if (!token) break;
                strncpy(options_type[*options_count], token, 15);
                token = strtok(NULL, ",");
                if (!token) break;
                options_contracts[*options_count] = atof(token);
                token = strtok(NULL, ",");
                if (!token) break;
                options_strike[*options_count] = atof(token);
                token = strtok(NULL, ",");
                if (!token) break;
                strncpy(options_expiry[*options_count], token, 31);
                (*options_count)++;
            }
        }
        if (token && strcmp(token, "history") == 0) {
            while ((token = strtok(NULL, ",")) && *history_count < MAX_HISTORY) {
                strncpy(history_type[*history_count], token, 15);
                history_type[*history_count][15] = '\0';
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
                token = strtok(NULL, ",");
                if (!token) {
                    history_expiration[*history_count][0] = '\0';
                    history_strike[*history_count] = 0.0;
                } else {
                    strncpy(history_expiration[*history_count], token, 31);
                    token = strtok(NULL, ",");
                    history_strike[*history_count] = token ? atof(token) : 0.0;
                }
                (*history_count)++;
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

static void write_user_account(const char *hash, float balance, char watchlist[MAX_WATCHLIST][MAX_LINE], int watchlist_count, 
                               char stocks[MAX_STOCKS][MAX_LINE], float shares[MAX_STOCKS], int stocks_count,
                               char options_symbol[MAX_OPTIONS][MAX_LINE], char options_type[MAX_OPTIONS][16],
                               float options_contracts[MAX_OPTIONS], float options_strike[MAX_OPTIONS],
                               char options_expiry[MAX_OPTIONS][32], int options_count,
                               char history_type[MAX_HISTORY][16], char history_symbol[MAX_HISTORY][MAX_LINE],
                               float history_shares[MAX_HISTORY], float history_price[MAX_HISTORY], 
                               char history_time[MAX_HISTORY][32], char history_expiration[MAX_HISTORY][32],
                               float history_strike[MAX_HISTORY], int history_count,
                               char last_lookup_symbol[MAX_LINE], float last_lookup_price, char last_lookup_time[32]) {
    char filename[32];
    snprintf(filename, sizeof(filename), "usr_acc.%s.txt", hash);
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "[%s] Failed to open for writing\n", filename);
        exit(1);
    }

    fprintf(fp, "balance,%.2f,watchlist", balance);
    for (int i = 0; i < watchlist_count; i++) {
        fprintf(fp, ",%s", watchlist[i]);
    }
    fprintf(fp, ",stocks");
    for (int i = 0; i < stocks_count; i++) {
        fprintf(fp, ",%s,%.2f", stocks[i], shares[i]);
    }
    fprintf(fp, ",options");
    for (int i = 0; i < options_count; i++) {
        fprintf(fp, ",%s,%s,%.2f,%.2f,%s", options_symbol[i], options_type[i], 
                options_contracts[i], options_strike[i], options_expiry[i]);
    }
    fprintf(fp, ",history");
    for (int i = 0; i < history_count; i++) {
        if (history_expiration[i][0]) {
            fprintf(fp, ",%s,%s,%.2f,%.2f,%s,%s,%.2f", history_type[i], history_symbol[i], 
                    history_shares[i], history_price[i], history_time[i], 
                    history_expiration[i], history_strike[i]);
        } else {
            fprintf(fp, ",%s,%s,%.2f,%.2f,%s", history_type[i], history_symbol[i], 
                    history_shares[i], history_price[i], history_time[i]);
        }
    }
    fprintf(fp, ",last_lookup,%s,%.2f,%s", last_lookup_symbol, last_lookup_price, last_lookup_time);
    fprintf(fp, "\n");
    fclose(fp);
}

static char *get_expiry_time(const char *expiry, char *time_str, size_t time_str_size) {
    time_t now = time(NULL);
    struct tm exp_tm = *localtime(&now);
    if (strcmp(expiry, "1 hour") == 0) exp_tm.tm_hour += 1;
    else if (strcmp(expiry, "1 day") == 0) exp_tm.tm_mday += 1;
    else if (strcmp(expiry, "1 week") == 0) exp_tm.tm_mday += 7;
    else if (strcmp(expiry, "1 month") == 0) exp_tm.tm_mon += 1;
    else if (strcmp(expiry, "1 year") == 0) exp_tm.tm_year += 1;
    else {
        fprintf(stderr, "Invalid expiry: %s\n", expiry);
        return NULL;
    }
    mktime(&exp_tm);
    strftime(time_str, time_str_size, "%Y-%m-%dT%H:%M:%S", &exp_tm);
    return time_str;
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <user_hash> <symbol> <index> <contracts>\n", argv[0]);
        return 1;
    }

    char *hash = argv[1];
    char symbol[MAX_LINE];
    strncpy(symbol, argv[2], MAX_LINE - 1);
    symbol[MAX_LINE - 1] = '\0';
    to_upper(symbol);
    int index = atoi(argv[3]);
    if (index < 1 || index > 10) {
        fprintf(stderr, "[%s] Invalid index: %d\n", symbol, index);
        return 1;
    }
    float contracts = atof(argv[4]);
    if (contracts <= 0) {
        fprintf(stderr, "[%s] Invalid contracts: %s\n", symbol, argv[4]);
        return 1;
    }

    float balance = 0.0;
    char watchlist[MAX_WATCHLIST][MAX_LINE] = {0};
    int watchlist_count = 0;
    char stocks[MAX_STOCKS][MAX_LINE] = {0};
    float stock_shares[MAX_STOCKS] = {0};
    int stocks_count = 0;
    char options_symbol[MAX_OPTIONS][MAX_LINE] = {0};
    char options_type[MAX_OPTIONS][16] = {0};
    float options_contracts[MAX_OPTIONS] = {0};
    float options_strike[MAX_OPTIONS] = {0};
    char options_expiry[MAX_OPTIONS][32] = {0};
    int options_count = 0;
    char history_type[MAX_HISTORY][16] = {0};
    char history_symbol[MAX_HISTORY][MAX_LINE] = {0};
    float history_shares[MAX_HISTORY] = {0};
    float history_price[MAX_HISTORY] = {0};
    char history_time[MAX_HISTORY][32] = {0};
    char history_expiration[MAX_HISTORY][32] = {0};
    float history_strike[MAX_HISTORY] = {0};
    int history_count = 0;
    char last_lookup_symbol[MAX_LINE] = {0};
    float last_lookup_price = 0.0;
    char last_lookup_time[32] = {0};

    read_user_account(hash, &balance, watchlist, &watchlist_count, stocks, stock_shares, &stocks_count,
                     options_symbol, options_type, options_contracts, options_strike, options_expiry, &options_count,
                     history_type, history_symbol, history_shares, history_price, history_time,
                     history_expiration, history_strike, &history_count,
                     last_lookup_symbol, &last_lookup_price, last_lookup_time);

    char filename[64];
    snprintf(filename, sizeof(filename), "option_prices.%s.csv", symbol);
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "[%s] No options file: %s\n", symbol, filename);
        return 1;
    }

    char line[MAX_LINE];
    fgets(line, sizeof(line), fp); // Skip header
    int current_index = 0;
    char opt_type[16] = {0}, expiry[32] = {0};
    float strike = 0.0, price = 0.0;
    while (fgets(line, sizeof(line), fp)) {
        current_index++;
        if (current_index == index) {
            if (sscanf(line, "%*d,%[^,],%[^,],%f,%f", opt_type, expiry, &strike, &price) != 4) {
                fprintf(stderr, "[%s] Failed to parse option at index %d\n", symbol, index);
                fclose(fp);
                return 1;
            }
            break;
        }
    }
    fclose(fp);

    if (current_index != index) {
        fprintf(stderr, "[%s] Option index %d not found\n", symbol, index);
        return 1;
    }

    float cost = price * contracts * 100; // 1 contract = 100 shares
    if (cost > balance) {
        fprintf(stderr, "[%s] Insufficient funds: $%.2f required, $%.2f available\n", symbol, cost, balance);
        return 1;
    }

    balance -= cost;
    if (history_count < MAX_HISTORY) {
        strncpy(history_type[history_count], opt_type, 15);
        history_type[history_count][15] = '\0';
        strncpy(history_symbol[history_count], symbol, MAX_LINE - 1);
        history_shares[history_count] = contracts;
        history_price[history_count] = price;
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);
        strftime(history_time[history_count], sizeof(history_time[0]), "%Y-%m-%dT%H:%M:%S", tm);
        char exp_time[32];
        if (!get_expiry_time(expiry, exp_time, sizeof(exp_time))) {
            return 1;
        }
        strncpy(history_expiration[history_count], exp_time, 31);
        history_expiration[history_count][31] = '\0';
        history_strike[history_count] = strike;
        history_count++;
    } else {
        fprintf(stderr, "[%s] History full\n", symbol);
        return 1;
    }

    // Update options portfolio
    int option_index = -1;
    for (int i = 0; i < options_count; i++) {
        if (strcmp(options_symbol[i], symbol) == 0 && 
            strcmp(options_type[i], opt_type) == 0 && 
            options_strike[i] == strike && 
            strcmp(options_expiry[i], history_expiration[history_count-1]) == 0) {
            option_index = i;
            break;
        }
    }
    if (option_index >= 0) {
        options_contracts[option_index] += contracts;
    } else if (options_count < MAX_OPTIONS) {
        strncpy(options_symbol[options_count], symbol, MAX_LINE - 1);
        strncpy(options_type[options_count], opt_type, 15);
        options_contracts[options_count] = contracts;
        options_strike[options_count] = strike;
        strncpy(options_expiry[options_count], history_expiration[history_count-1], 31);
        options_count++;
    } else {
        fprintf(stderr, "[%s] Options portfolio full\n", symbol);
        return 1;
    }

    write_user_account(hash, balance, watchlist, watchlist_count, stocks, stock_shares, stocks_count,
                      options_symbol, options_type, options_contracts, options_strike, options_expiry, options_count,
                      history_type, history_symbol, history_shares, history_price, history_time,
                      history_expiration, history_strike, history_count,
                      last_lookup_symbol, last_lookup_price, last_lookup_time);

    printf("Bought %.2f %s %s option (Strike: %.2f, Expiry: %s) at $%.2f. New balance: $%.2f\n",
           contracts, opt_type, symbol, strike, expiry, price, balance);
    return 0;
}
