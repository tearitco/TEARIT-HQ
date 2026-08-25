#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>

#define MAX_LINE 256
#define MAX_STOCKS 50
#define MAX_OPTIONS 50
#define MAX_HISTORY 1000
#define MAX_WATCHLIST 50

static long expiry_to_seconds(const char *expiry) {
    if (strcmp(expiry, "1 hour") == 0) return 3600;
    if (strcmp(expiry, "1 day") == 0) return 86400;
    if (strcmp(expiry, "1 week") == 0) return 7 * 86400;
    if (strcmp(expiry, "1 month") == 0) return 30 * 86400;
    if (strcmp(expiry, "1 year") == 0) return 365 * 86400;
    return 0;
}

static int dates_match(const char *date1, const char *date2) {
    struct tm tm1 = {0}, tm2 = {0};
    char date1_copy[32], date2_copy[32];
    strncpy(date1_copy, date1, 31);
    strncpy(date2_copy, date2, 31);
    char *date1_end = strstr(date1_copy, "T");
    char *date2_end = strstr(date2_copy, "T");
    if (date1_end) *date1_end = '\0';
    if (date2_end) *date2_end = '\0';
    strptime(date1_copy, "%Y-%m-%d", &tm1);
    strptime(date2_copy, "%Y-%m-%d", &tm2);
    return tm1.tm_year == tm2.tm_year && tm1.tm_mon == tm2.tm_mon && tm1.tm_mday == tm2.tm_mday;
}

static void to_upper(char *str) {
    for (char *p = str; *p; p++) *p = toupper(*p);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <user_hash> <option_index> <contracts>\n", argv[0]);
        return 1;
    }

    char *user_hash = argv[1];
    int option_index = atoi(argv[2]);
    float contracts_to_sell = atof(argv[3]);

    if (option_index < 0 || contracts_to_sell <= 0) {
        printf("Invalid index or contracts\n");
        return 1;
    }

    char filename[32];
    snprintf(filename, sizeof(filename), "usr_acc.%s.txt", user_hash);
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "[%s] Failed to open: %s\n", filename, strerror(errno));
        return 1;
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

    char line[MAX_LINE];
    if (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = 0;
        char *line_copy = strdup(line);
        if (!line_copy) {
            fprintf(stderr, "[%s] Memory allocation failed\n", filename);
            fclose(fp);
            return 1;
        }
        char *token = strtok(line_copy, ",");
        if (token && strcmp(token, "balance") == 0) {
            token = strtok(NULL, ",");
            if (token) balance = atof(token);
            token = strtok(NULL, ",");
        }
        while (token && strcmp(token, "watchlist") != 0) token = strtok(NULL, ",");
        if (token && strcmp(token, "watchlist") == 0) {
            token = strtok(NULL, ",");
            while (token && strcmp(token, "stocks") != 0 && watchlist_count < MAX_WATCHLIST) {
                strncpy(watchlist[watchlist_count], token, MAX_LINE - 1);
                to_upper(watchlist[watchlist_count]);
                watchlist_count++;
                token = strtok(NULL, ",");
            }
        }
        if (token && strcmp(token, "stocks") == 0) {
            token = strtok(NULL, ",");
            while (token && strcmp(token, "options") != 0 && stocks_count < MAX_STOCKS) {
                strncpy(stocks[stocks_count], token, MAX_LINE - 1);
                to_upper(stocks[stocks_count]);
                token = strtok(NULL, ",");
                if (!token || strcmp(token, "options") == 0) break;
                shares[stocks_count] = atof(token);
                stocks_count++;
                token = strtok(NULL, ",");
            }
        }
        if (token && strcmp(token, "options") == 0) {
            token = strtok(NULL, ",");
            while (token && strcmp(token, "history") != 0 && options_count < MAX_OPTIONS) {
                strncpy(options_symbol[options_count], token, MAX_LINE - 1);
                to_upper(options_symbol[options_count]);
                token = strtok(NULL, ",");
                if (!token) break;
                strncpy(options_type[options_count], token, 15);
                token = strtok(NULL, ",");
                if (!token) break;
                options_contracts[options_count] = atof(token);
                token = strtok(NULL, ",");
                if (!token) break;
                options_strike[options_count] = atof(token);
                token = strtok(NULL, ",");
                if (!token) break;
                strncpy(options_expiry[options_count], token, 31);
                options_count++;
                token = strtok(NULL, ",");
            }
        }
        if (token && strcmp(token, "history") == 0) {
            token = strtok(NULL, ",");
            while (token && strcmp(token, "last_lookup") != 0 && history_count < MAX_HISTORY) {
                strncpy(history_type[history_count], token, 15);
                token = strtok(NULL, ",");
                if (!token) break;
                strncpy(history_symbol[history_count], token, MAX_LINE - 1);
                to_upper(history_symbol[history_count]);
                token = strtok(NULL, ",");
                if (!token) break;
                history_shares[history_count] = atof(token);
                token = strtok(NULL, ",");
                if (!token) break;
                history_price[history_count] = atof(token);
                token = strtok(NULL, ",");
                if (!token) break;
                strncpy(history_time[history_count], token, 31);
                token = strtok(NULL, ",");
                if (!token) {
                    history_expiration[history_count][0] = '\0';
                    history_strike[history_count] = 0.0;
                } else {
                    strncpy(history_expiration[history_count], token, 31);
                    token = strtok(NULL, ",");
                    history_strike[history_count] = token ? atof(token) : 0.0;
                }
                history_count++;
                token = strtok(NULL, ",");
            }
        }
        if (token && strcmp(token, "last_lookup") == 0) {
            token = strtok(NULL, ",");
            if (token) strncpy(last_lookup_symbol, token, MAX_LINE - 1);
            token = strtok(NULL, ",");
            if (token) last_lookup_price = atof(token);
            token = strtok(NULL, ",");
            if (token) strncpy(last_lookup_time, token, 31);
        }
        free(line_copy);
    }
    fclose(fp);

    if (option_index >= options_count || options_contracts[option_index] < contracts_to_sell) {
        printf("Invalid index or insufficient contracts\n");
        return 1;
    }

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%S", tm);
    char command[512];
    snprintf(command, sizeof(command), "'./+x/options_pricing.+x' -s '%s' -p '%.2f' -k '%.2f' -r 0.05 -v 0.2 -d 0.00 -t '%s' > 'option_prices.%s.csv'",
             options_symbol[option_index], options_strike[option_index], options_strike[option_index], time_str, options_symbol[option_index]);
    system(command);

    char csv_filename[64];
    snprintf(csv_filename, sizeof(csv_filename), "option_prices.%s.csv", options_symbol[option_index]);
    FILE *csv_fp = fopen(csv_filename, "r");
    float option_price = 0.0;
    if (csv_fp) {
        char csv_line[MAX_LINE];
        fgets(csv_line, sizeof(csv_line), csv_fp); // Skip header
        while (fgets(csv_line, sizeof(csv_line), csv_fp)) {
            int index;
            char type[16], expiry[32];
            float strike_val, price;
            if (sscanf(csv_line, "%d,%[^,],%[^,],%f,%f", &index, type, expiry, &strike_val, &price) == 5) {
                if (strcmp(type, options_type[option_index]) == 0 && strike_val == options_strike[option_index]) {
                    long seconds = expiry_to_seconds(expiry);
                    if (seconds > 0) {
                        time_t base_time = now;
                        time_t exp_time = base_time + seconds;
                        struct tm *exp_tm = localtime(&exp_time);
                        char computed_expiry[32];
                        strftime(computed_expiry, sizeof(computed_expiry), "%Y-%m-%dT%H:%M:%S", exp_tm);
                        if (dates_match(computed_expiry, options_expiry[option_index])) {
                            option_price = price;
                            break;
                        }
                    }
                }
            }
        }
        fclose(csv_fp);
    }

    if (option_price == 0.0) {
        printf("Failed to fetch option price\n");
        return 1;
    }

    float sale_value = contracts_to_sell * 100.0 * option_price;
    balance += sale_value;
    options_contracts[option_index] -= contracts_to_sell;

    strncpy(history_type[history_count], "Sell", 15);
    strncpy(history_symbol[history_count], options_symbol[option_index], MAX_LINE - 1);
    history_shares[history_count] = contracts_to_sell;
    history_price[history_count] = option_price;
    strncpy(history_time[history_count], time_str, 31);
    strncpy(history_expiration[history_count], options_expiry[option_index], 31);
    history_strike[history_count] = options_strike[option_index];
    history_count++;

    FILE *out_fp = fopen(filename, "w");
    if (!out_fp) {
        fprintf(stderr, "[%s] Failed to open for writing\n", filename);
        return 1;
    }

    fprintf(out_fp, "balance,%.2f,watchlist", balance);
    for (int i = 0; i < watchlist_count; i++) {
        fprintf(out_fp, ",%s", watchlist[i]);
    }
    fprintf(out_fp, ",stocks");
    for (int i = 0; i < stocks_count; i++) {
        fprintf(out_fp, ",%s,%.2f", stocks[i], shares[i]);
    }
    fprintf(out_fp, ",options");
    for (int i = 0; i < options_count; i++) {
        if (options_contracts[i] > 0) {
            fprintf(out_fp, ",%s,%s,%.2f,%.2f,%s", options_symbol[i], options_type[i],
                    options_contracts[i], options_strike[i], options_expiry[i]);
        }
    }
    fprintf(out_fp, ",history");
    for (int i = 0; i < history_count; i++) {
        if (history_expiration[i][0]) {
            fprintf(out_fp, ",%s,%s,%.2f,%.2f,%s,%s,%.2f", history_type[i], history_symbol[i],
                    history_shares[i], history_price[i], history_time[i],
                    history_expiration[i], history_strike[i]);
        } else {
            fprintf(out_fp, ",%s,%s,%.2f,%.2f,%s", history_type[i], history_symbol[i],
                    history_shares[i], history_price[i], history_time[i]);
        }
    }
    fprintf(out_fp, ",last_lookup,%s,%.2f,%s", last_lookup_symbol, last_lookup_price, last_lookup_time);
    fprintf(out_fp, "\n");
    fclose(out_fp);

    printf("Sold %.2f %s %s option (Strike: %.2f, Expiry: %s) at $%.2f. New balance: $%.2f\n",
           contracts_to_sell, options_type[option_index], options_symbol[option_index],
           options_strike[option_index], options_expiry[option_index], option_price, balance);

    return 0;
}
