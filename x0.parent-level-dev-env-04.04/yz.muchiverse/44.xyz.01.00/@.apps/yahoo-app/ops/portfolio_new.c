#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>

#define MAX_LINE 256
#define MAX_STOCKS 50
#define MAX_OPTIONS 50

// Map relative expiry (e.g., "1 week") to seconds
static long expiry_to_seconds(const char *expiry) {
    if (strcmp(expiry, "1 hour") == 0) return 3600;
    if (strcmp(expiry, "1 day") == 0) return 86400;
    if (strcmp(expiry, "1 week") == 0) return 7 * 86400;
    if (strcmp(expiry, "1 month") == 0) return 30 * 86400;
    if (strcmp(expiry, "1 year") == 0) return 365 * 86400;
    return 0;
}

// Compare two dates (YYYY-MM-DD) for approximate match (same day)
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

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <user_hash>\n", argv[0]);
        return 1;
    }

    char filename[32];
    snprintf(filename, sizeof(filename), "usr_acc.%s.txt", argv[1]);
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "[%s] Failed to open: %s\n", filename, strerror(errno));
        return 1;
    }

    float balance = 0.0;
    char stocks[MAX_STOCKS][MAX_LINE] = {0};
    float shares[MAX_STOCKS] = {0};
    int stocks_count = 0;
    char options_symbol[MAX_OPTIONS][MAX_LINE] = {0};
    char options_type[MAX_OPTIONS][16] = {0};
    float options_contracts[MAX_OPTIONS] = {0};
    float options_strike[MAX_OPTIONS] = {0};
    char options_expiry[MAX_OPTIONS][32] = {0};
    int options_count = 0;

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
        while (token && strcmp(token, "stocks") != 0) {
            token = strtok(NULL, ",");
        }
        if (token && strcmp(token, "stocks") == 0) {
            token = strtok(NULL, ",");
            while (token && strcmp(token, "options") != 0 && stocks_count < MAX_STOCKS) {
                strncpy(stocks[stocks_count], token, MAX_LINE - 1);
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
        free(line_copy);
    }
    fclose(fp);

    float stock_value = 0.0;
    float option_value = 0.0;

    printf("Portfolio:\nStocks:\n");
    for (int i = 0; i < stocks_count; i++) {
        if (shares[i] == 0.0) continue;
        printf("%s: %.2f shares\n", stocks[i], shares[i]);

        char command[512];
        snprintf(command, sizeof(command), "./+x/lookup_stock.+x %s %s", argv[1], stocks[i]);
        FILE *pipe = popen(command, "r");
        float price = 0.0;
        if (pipe) {
            char output[MAX_LINE];
            if (fgets(output, sizeof(output), pipe)) {
                char *price_start = strstr(output, "Current Price: ");
                if (price_start) {
                    price_start += strlen("Current Price: ");
                    price = atof(price_start);
                }
            }
            pclose(pipe);
        }
        stock_value += shares[i] * price;
    }

    printf("Options:\n");
    if (options_count == 0) {
        printf("(none)\n");
    } else {
        for (int i = 0; i < options_count; i++) {
            if (options_contracts[i] == 0.0) continue;
            printf("%s: %.2f %s contracts, Strike=$%.2f, Expiry=%s\n",
                   options_symbol[i], options_contracts[i], options_type[i],
                   options_strike[i], options_expiry[i]);

            time_t now = time(NULL);
            struct tm *tm = localtime(&now);
            char time_str[32];
            strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%S", tm);
            char command[512];
            snprintf(command, sizeof(command), "./+x/options_pricing.+x -s %s -p %.2f -k %.2f -r 0.05 -v 0.2 -d 0.00 -t %s > option_prices.%s.csv",
                     options_symbol[i], options_strike[i], options_strike[i], time_str, options_symbol[i]);
            system(command);

            char filename[64];
            snprintf(filename, sizeof(filename), "option_prices.%s.csv", options_symbol[i]);
            FILE *opt_fp = fopen(filename, "r");
            float option_price = 0.0;
            if (opt_fp) {
                char opt_line[MAX_LINE];
                fgets(opt_line, sizeof(opt_line), opt_fp); // Skip header
                while (fgets(opt_line, sizeof(opt_line), opt_fp)) {
                    int index;
                    char type[16], expiry[32];
                    float strike_val, price;
                    if (sscanf(opt_line, "%d,%[^,],%[^,],%f,%f", &index, type, expiry, &strike_val, &price) == 5) {
                        if (strcmp(type, options_type[i]) == 0 && strike_val == options_strike[i]) {
                            long seconds = expiry_to_seconds(expiry);
                            if (seconds > 0) {
                                time_t base_time = time(NULL);
                                time_t exp_time = base_time + seconds;
                                struct tm *exp_tm = localtime(&exp_time);
                                char computed_expiry[32];
                                strftime(computed_expiry, sizeof(computed_expiry), "%Y-%m-%dT%H:%M:%S", exp_tm);
                                if (dates_match(computed_expiry, options_expiry[i])) {
                                    option_price = price;
                                    break;
                                }
                            }
                        }
                    }
                }
                fclose(opt_fp);
            }
            option_value += options_contracts[i] * 100.0 * option_price;
        }
    }

    printf("\nPortfolio Value: $%.2f (Stocks: $%.2f, Options: $%.2f, Cash: $%.2f)\n",
           balance + stock_value + option_value, stock_value, option_value, balance);

    return 0;
}
