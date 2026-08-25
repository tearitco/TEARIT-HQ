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

static void generate_user_hash(char *hash) {
    const char *chars = "0123456789ABCDEF";
    for (int i = 0; i < 6; i++) {
        hash[i] = chars[rand() % 16];
    }
    hash[6] = '\0';
}

static void create_user_file(const char *hash) {
    char filename[32];
    snprintf(filename, sizeof(filename), "usr_acc.%s.txt", hash);
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "[INIT] Failed to create %s: %s\n", filename, strerror(errno));
        exit(1);
    }
    fprintf(fp, "balance,0.00,watchlist,stocks,options,history,last_lookup,,0.00,\n");
    fclose(fp);
    fprintf(stderr, "[INIT] Created %s\n", filename);
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
        create_user_file(hash);
        fp = fopen(filename, "r");
        if (!fp) {
            fprintf(stderr, "[%s] Failed to open after creation: %s\n", filename, strerror(errno));
            exit(1);
        }
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
            token = strtok(NULL, ",");
        }
        if (token && strcmp(token, "watchlist") == 0) {
            token = strtok(NULL, ",");
            while (token && strcmp(token, "stocks") != 0 && *watchlist_count < MAX_WATCHLIST) {
                strncpy(watchlist[*watchlist_count], token, MAX_LINE - 1);
                to_upper(watchlist[*watchlist_count]);
                (*watchlist_count)++;
                token = strtok(NULL, ",");
            }
        }
        if (token && strcmp(token, "stocks") == 0) {
            token = strtok(NULL, ",");
            while (token && strcmp(token, "options") != 0 && *stocks_count < MAX_STOCKS) {
                strncpy(stocks[*stocks_count], token, MAX_LINE - 1);
                to_upper(stocks[*stocks_count]);
                token = strtok(NULL, ",");
                if (!token || strcmp(token, "options") == 0) {
                    token = NULL;
                    continue;
                }
                shares[*stocks_count] = atof(token);
                (*stocks_count)++;
                token = strtok(NULL, ",");
            }
        }
        if (token && strcmp(token, "options") == 0) {
            token = strtok(NULL, ",");
            while (token && strcmp(token, "history") != 0 && *options_count < MAX_OPTIONS) {
                strncpy(options_symbol[*options_count], token, MAX_LINE - 1);
                to_upper(options_symbol[*options_count]);
                token = strtok(NULL, ",");
                if (!token) {
                    token = NULL;
                    continue;
                }
                strncpy(options_type[*options_count], token, 15);
                token = strtok(NULL, ",");
                if (!token) {
                    token = NULL;
                    continue;
                }
                options_contracts[*options_count] = atof(token);
                token = strtok(NULL, ",");
                if (!token) {
                    token = NULL;
                    continue;
                }
                options_strike[*options_count] = atof(token);
                token = strtok(NULL, ",");
                if (!token) {
                    token = NULL;
                    continue;
                }
                strncpy(options_expiry[*options_count], token, 31);
                (*options_count)++;
                token = strtok(NULL, ",");
            }
        }
        if (token && strcmp(token, "history") == 0) {
            token = strtok(NULL, ",");
            while (token && strcmp(token, "last_lookup") != 0 && *history_count < MAX_HISTORY) {
                strncpy(history_type[*history_count], token, 15);
                history_type[*history_count][15] = '\0';
                token = strtok(NULL, ",");
                if (!token) {
                    token = NULL;
                    continue;
                }
                strncpy(history_symbol[*history_count], token, MAX_LINE - 1);
                to_upper(history_symbol[*history_count]);
                token = strtok(NULL, ",");
                if (!token) {
                    token = NULL;
                    continue;
                }
                history_shares[*history_count] = atof(token);
                token = strtok(NULL, ",");
                if (!token) {
                    token = NULL;
                    continue;
                }
                history_price[*history_count] = atof(token);
                token = strtok(NULL, ",");
                if (!token) {
                    token = NULL;
                    continue;
                }
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

int main(int argc, char *argv[]) {
    srand(time(NULL));
    char user_hash[7];
    if (argc == 2) {
        strncpy(user_hash, argv[1], 6);
        user_hash[6] = '\0';
    } else {
        generate_user_hash(user_hash);
        printf("Generated new user hash: %s\n", user_hash);
    }

    float balance = 0.0;
    char watchlist[MAX_WATCHLIST][MAX_LINE] = {0};
    int watchlist_count = 0;
    char stocks[MAX_STOCKS][MAX_LINE] = {0};
    float shares[MAX_STOCKS] = {0};
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

    while (1) {
        read_user_account(user_hash, &balance, watchlist, &watchlist_count, stocks, shares, &stocks_count,
                         options_symbol, options_type, options_contracts, options_strike, options_expiry, &options_count,
                         history_type, history_symbol, history_shares, history_price, history_time,
                         history_expiration, history_strike, &history_count,
                         last_lookup_symbol, &last_lookup_price, last_lookup_time);

        printf("\nUser: %s (Balance: $%.2f)\n", user_hash, balance);
        if (last_lookup_symbol[0]) {
            printf("Latest lookup: %s %.2f (%s)\n", last_lookup_symbol, last_lookup_price, last_lookup_time);
        } else {
            printf("Latest lookup: 0.00 0.00 ()\n");
        }
        printf("Menu:\n1. Lookup stock price\n2. Check balance\n3. Add credit\n4. Buy stock\n5. Sell stock\n6. Sell options\n7. View portfolio\n8. View profit/loss\n9. Add to watchlist\n10. Options\n11. Predict stock price\n12. Quit\n");
        printf("Enter option (1-12): ");

        char input[32];
        if (!fgets(input, sizeof(input), stdin)) break;
        int option = atoi(input);

        if (option == 12) break;

        char command[512];
        switch (option) {
            case 1: {
                printf("Enter stock symbol: ");
                char symbol[MAX_LINE];
                if (!fgets(symbol, sizeof(symbol), stdin)) break;
                symbol[strcspn(symbol, "\n")] = 0;
                to_upper(symbol);
                snprintf(command, sizeof(command), "./+x/lookup_stock.+x %s %s", user_hash, symbol);
                FILE *pipe = popen(command, "r");
                if (!pipe) {
                    printf("[%s] Lookup failed: %s\n", symbol, strerror(errno));
                    break;
                }
                char output[MAX_LINE];
                float price = 0.0;
                char time_str[32] = {0};
                if (fgets(output, sizeof(output), pipe)) {
                    char *price_start = strstr(output, "Current Price: ");
                    if (price_start) {
                        price_start += strlen("Current Price: ");
                        price = atof(price_start);
                        char *time_start = strstr(output, "(cached, ");
                        if (time_start) {
                            time_start += strlen("(cached, ");
                            strncpy(time_str, time_start, 31);
                            time_str[strcspn(time_str, ")")] = 0;
                        }
                    }
                }
                pclose(pipe);
                if (price > 0) {
                    strncpy(last_lookup_symbol, symbol, MAX_LINE - 1);
                    last_lookup_price = price;
                    strncpy(last_lookup_time, time_str, 31);
                    write_user_account(user_hash, balance, watchlist, watchlist_count, stocks, shares, stocks_count,
                                      options_symbol, options_type, options_contracts, options_strike, options_expiry, options_count,
                                      history_type, history_symbol, history_shares, history_price, history_time,
                                      history_expiration, history_strike, history_count,
                                      last_lookup_symbol, last_lookup_price, last_lookup_time);
                }
                system(command); // Display lookup output
                break;
            }
            case 2:
                printf("Current balance: $%.2f\n", balance);
                break;
            case 3: {
                printf("Enter amount to add: ");
                char amount[32];
                if (!fgets(amount, sizeof(amount), stdin)) break;
                snprintf(command, sizeof(command), "./+x/add_credit.+x %s %s", user_hash, amount);
                system(command);
                break;
            }
            case 4: {
                printf("Enter symbol and shares (e.g., NVDA 10): ");
                char symbol[MAX_LINE], shares_str[32];
                if (!fgets(input, sizeof(input), stdin)) break;
                if (sscanf(input, "%s %s", symbol, shares_str) != 2) {
                    printf("Invalid input\n");
                    break;
                }
                to_upper(symbol);
                snprintf(command, sizeof(command), "./+x/buy_stock.+x %s %s %s", user_hash, symbol, shares_str);
                system(command);
                break;
            }
            case 5: {
                printf("Enter symbol and shares (e.g., NVDA 5): ");
                char symbol[MAX_LINE], shares_str[32];
                if (!fgets(input, sizeof(input), stdin)) break;
                if (sscanf(input, "%s %s", symbol, shares_str) != 2) {
                    printf("Invalid input\n");
                    break;
                }
                to_upper(symbol);
                snprintf(command, sizeof(command), "./+x/sell_stock.+x %s %s %s", user_hash, symbol, shares_str);
                system(command);
                break;
            }
            case 6: {
                if (options_count == 0) {
                    printf("No options held\n");
                    break;
                }
                printf("Held Options:\n");
                for (int i = 0; i < options_count; i++) {
                    if (options_contracts[i] > 0) {
                        printf("%d. %s: %.2f %s contracts, Strike=$%.2f, Expiry=%s\n",
                               i + 1, options_symbol[i], options_contracts[i], options_type[i],
                               options_strike[i], options_expiry[i]);
                    }
                }
                printf("Enter option index (1-%d) or 0 to cancel: ", options_count);
                if (!fgets(input, sizeof(input), stdin)) break;
                int index = atoi(input);
                if (index < 1 || index > options_count) {
                    printf("Cancelled\n");
                    break;
                }
                index--; // Adjust to 0-based
                printf("Enter number of contracts to sell: ");
                char contracts[32];
                if (!fgets(contracts, sizeof(contracts), stdin)) break;
                contracts[strcspn(contracts, "\n")] = 0;
                snprintf(command, sizeof(command), "./+x/sell_option_inventory.+x %s %d %s",
                         user_hash, index, contracts);
                system(command);
                break;
            }
            case 7: {
                snprintf(command, sizeof(command), "./+x/portfolio_new.+x %s", user_hash);
                system(command);
                break;
            }
            case 8: {
                snprintf(command, sizeof(command), "./+x/profit_loss.+x %s", user_hash);
                system(command);
                break;
            }
            case 9: {
                printf("Enter symbol to add to watchlist: ");
                char symbol[MAX_LINE];
                if (!fgets(symbol, sizeof(symbol), stdin)) break;
                symbol[strcspn(symbol, "\n")] = 0;
                to_upper(symbol);
                if (watchlist_count < MAX_WATCHLIST) {
                    strncpy(watchlist[watchlist_count], symbol, MAX_LINE - 1);
                    watchlist_count++;
                    write_user_account(user_hash, balance, watchlist, watchlist_count, stocks, shares, stocks_count,
                                      options_symbol, options_type, options_contracts, options_strike, options_expiry, options_count,
                                      history_type, history_symbol, history_shares, history_price, history_time,
                                      history_expiration, history_strike, history_count,
                                      last_lookup_symbol, last_lookup_price, last_lookup_time);
                    printf("Added %s to watchlist\n", symbol);
                } else {
                    printf("Watchlist full\n");
                }
                break;
            }
            case 10: {
                char symbol[MAX_LINE];
                float price = 0.0;
                if (last_lookup_symbol[0]) {
                    strncpy(symbol, last_lookup_symbol, MAX_LINE - 1);
                    price = last_lookup_price;
                    printf("Using latest lookup: %s (Price: %.2f)\n", symbol, price);
                } else {
                    printf("No recent lookup. Enter symbol: ");
                    if (!fgets(symbol, sizeof(symbol), stdin)) break;
                    symbol[strcspn(symbol, "\n")] = 0;
                    to_upper(symbol);
                    snprintf(command, sizeof(command), "./+x/lookup_stock.+x %s %s", user_hash, symbol);
                    FILE *pipe = popen(command, "r");
                    if (!pipe) {
                        printf("[%s] Lookup failed: %s\n", symbol, strerror(errno));
                        break;
                    }
                    char output[MAX_LINE];
                    if (fgets(output, sizeof(output), pipe)) {
                        char *price_start = strstr(output, "Current Price: ");
                        if (price_start) {
                            price_start += strlen("Current Price: ");
                            price = atof(price_start);
                            char *time_start = strstr(output, "(cached, ");
                            if (time_start) {
                                time_start += strlen("(cached, ");
                                strncpy(last_lookup_time, time_start, 31);
                                last_lookup_time[strcspn(last_lookup_time, ")")] = 0;
                            }
                        }
                    }
                    pclose(pipe);
                    if (price <= 0) {
                        printf("[%s] Failed to fetch price\n", symbol);
                        break;
                    }
                    strncpy(last_lookup_symbol, symbol, MAX_LINE - 1);
                    last_lookup_price = price;
                    write_user_account(user_hash, balance, watchlist, watchlist_count, stocks, shares, stocks_count,
                                      options_symbol, options_type, options_contracts, options_strike, options_expiry, options_count,
                                      history_type, history_symbol, history_shares, history_price, history_time,
                                      history_expiration, history_strike, history_count,
                                      last_lookup_symbol, last_lookup_price, last_lookup_time);
                }
                time_t now = time(NULL);
                struct tm *tm = localtime(&now);
                char time_str[32];
                strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%S", tm);
                snprintf(command, sizeof(command), "./+x/options_pricing.+x -s %s -p %.2f -k %.2f -r 0.05 -v 0.2 -d 0.00 -t %s > option_prices.%s.csv",
                         symbol, price, price, time_str, symbol);
                printf("[%s] Executing: %s\n", symbol, command);
                system(command);
                printf("\nOptions for %s (Strike: %.2f):\n", symbol, price);
                printf("Index | Type | Expiry     | Strike | Price\n");
                printf("------+------+------------+--------+-------\n");
                char filename[64];
                snprintf(filename, sizeof(filename), "option_prices.%s.csv", symbol);
                FILE *fp = fopen(filename, "r");
                if (fp) {
                    char line[MAX_LINE];
                    if (fgets(line, sizeof(line), fp)) { // Skip header
                    }
                    while (fgets(line, sizeof(line), fp)) {
                        int index;
                        char type[16], expiry[32];
                        float strike_val, option_price;
                        if (sscanf(line, "%d,%[^,],%[^,],%f,%f", &index, type, expiry, &strike_val, &option_price) == 5) {
                            printf("%-5d | %-4s | %-10s | %-6.2f | %.2f\n", index, type, expiry, strike_val, option_price);
                        }
                    }
                    fclose(fp);
                } else {
                    printf("[%s] No options data available\n", symbol);
                    break;
                }
                printf("\nEnter option index (1-10) or 0 to cancel: ");
                if (!fgets(input, sizeof(input), stdin)) break;
                int index = atoi(input);
                if (index < 1 || index > 10) {
                    printf("Cancelled\n");
                    break;
                }
                printf("Enter action (buy/sell): ");
                char action[32];
                if (!fgets(action, sizeof(action), stdin)) break;
                action[strcspn(action, "\n")] = 0;
                to_upper(action);
                printf("Enter number of contracts: ");
                char contracts[32];
                if (!fgets(contracts, sizeof(contracts), stdin)) break;
                contracts[strcspn(contracts, "\n")] = 0;
                if (strcmp(action, "BUY") == 0) {
                    snprintf(command, sizeof(command), "./+x/buy_option.+x %s %s %d %s", user_hash, symbol, index, contracts);
                } else if (strcmp(action, "SELL") == 0) {
                    snprintf(command, sizeof(command), "./+x/sell_option.+x %s %s %d %s", user_hash, symbol, index, contracts);
                } else {
                    printf("Invalid action\n");
                    break;
                }
                system(command);
                break;
            }
            case 11: {
                printf("Enter stock symbol to predict: ");
                char symbol[MAX_LINE];
                if (!fgets(symbol, sizeof(symbol), stdin)) break;
                symbol[strcspn(symbol, "\n")] = 0;
                to_upper(symbol);
                snprintf(command, sizeof(command), "./+x/predictions.+x %s %s", user_hash, symbol);
                system(command);
                break;
            }
            default:
                printf("Invalid option\n");
        }
    }
    return 0;
}
