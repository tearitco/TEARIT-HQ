#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <math.h>

#define MAX_LINE 256
#define TEST_USER "TEST123"

static void reset_user_file(const char *hash) {
    char filename[32];
    snprintf(filename, sizeof(filename), "usr_acc.%s.txt", hash);
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "[RESET] Failed to create %s: %s\n", filename, strerror(errno));
        exit(1);
    }
    fprintf(fp, "balance,0.00,watchlist,stocks,options,history,last_lookup,,0.00,\n");
    fclose(fp);
    fprintf(stderr, "[RESET] Created %s\n", filename);
}

static void create_mock_option_prices(const char *symbol) {
    char filename[64];
    snprintf(filename, sizeof(filename), "option_prices.%s.csv", symbol);
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "[MOCK] Failed to create %s: %s\n", filename, strerror(errno));
        exit(1);
    }
    fprintf(fp, "index,type,expiration,strike,price\n");
    fprintf(fp, "1,Call,1 hour,143.96,0.10\n");
    fprintf(fp, "2,Call,1 day,143.96,0.51\n");
    fprintf(fp, "3,Call,1 week,143.96,1.37\n");
    fprintf(fp, "4,Call,1 month,143.96,2.97\n");
    fprintf(fp, "5,Call,1 year,143.96,11.82\n");
    fprintf(fp, "6,Put,1 hour,143.96,0.10\n");
    fprintf(fp, "7,Put,1 day,143.96,0.50\n");
    fprintf(fp, "8,Put,1 week,143.96,1.29\n");
    fprintf(fp, "9,Put,1 month,143.96,2.60\n");
    fprintf(fp, "10,Put,1 year,143.96,7.66\n");
    fclose(fp);
    fprintf(stderr, "[MOCK] Created %s with options\n", filename);
}

static char *get_mock_time(char *time_str, size_t size) {
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    strftime(time_str, size, "%Y-%m-%dT%H:%M:%S", tm);
    return time_str;
}

static char *get_mock_expiry_time(char *time_str, size_t size) {
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    tm->tm_mday += 1; // 1 day expiry
    mktime(tm);
    strftime(time_str, size, "%Y-%m-%dT%H:%M:%S", tm);
    return time_str;
}

static int check_user_state(const char *hash, float expected_balance, const char *expected_stocks,
                           const char *expected_options, const char *expected_history,
                           const char *expected_last_lookup) {
    char filename[32];
    snprintf(filename, sizeof(filename), "usr_acc.%s.txt", hash);
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "[CHECK] Failed to open %s: %s\n", filename, strerror(errno));
        return 0;
    }
    char line[MAX_LINE];
    int result = 1;
    if (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = 0;
        char *line_copy = strdup(line);
        if (!line_copy) {
            fprintf(stderr, "[%s] Memory allocation failed\n", filename);
            fclose(fp);
            return 0;
        }
        char stocks_str[256] = {0}, options_str[256] = {0}, history_str[512] = {0}, last_lookup_str[128] = {0};
        float balance = 0.0;
        int section = 0; // 0: balance, 1: watchlist, 2: stocks, 3: options, 4: history, 5: last_lookup
        char *token = strtok(line_copy, ",");
        while (token) {
            if (strcmp(token, "balance") == 0) {
                section = 0;
                token = strtok(NULL, ",");
                if (token) balance = atof(token);
            } else if (strcmp(token, "watchlist") == 0) {
                section = 1;
            } else if (strcmp(token, "stocks") == 0) {
                section = 2;
            } else if (strcmp(token, "options") == 0) {
                section = 3;
            } else if (strcmp(token, "history") == 0) {
                section = 4;
            } else if (strcmp(token, "last_lookup") == 0) {
                section = 5;
            } else {
                if (section == 2 && section != 3) {
                    strcat(stocks_str, token);
                    strcat(stocks_str, ",");
                } else if (section == 3 && section != 4) {
                    strcat(options_str, token);
                    strcat(options_str, ",");
                } else if (section == 4 && section != 5) {
                    strcat(history_str, token);
                    strcat(history_str, ",");
                } else if (section == 5) {
                    strcat(last_lookup_str, token);
                    strcat(last_lookup_str, ",");
                }
            }
            token = strtok(NULL, ",");
        }
        if (fabs(balance - expected_balance) > 0.01) {
            fprintf(stderr, "Balance incorrect: expected %.2f, got %.2f\n", expected_balance, balance);
            result = 0;
        }
        if (strcmp(stocks_str, expected_stocks) != 0) {
            fprintf(stderr, "Stocks mismatch: expected '%s', got '%s'\n", expected_stocks, stocks_str);
            result = 0;
        }
        if (strcmp(options_str, expected_options) != 0) {
            fprintf(stderr, "Options mismatch: expected '%s', got '%s'\n", expected_options, options_str);
            result = 0;
        }
        if (strcmp(history_str, expected_history) != 0) {
            fprintf(stderr, "History mismatch: expected '%s', got '%s'\n", expected_history, history_str);
            result = 0;
        }
        if (strcmp(last_lookup_str, expected_last_lookup) != 0) {
            fprintf(stderr, "Last lookup mismatch: expected '%s', got '%s'\n", expected_last_lookup, last_lookup_str);
            result = 0;
        }
        free(line_copy);
    }
    fclose(fp);
    return result;
}

static int check_portfolio_output(const char *hash, const char *expected_output) {
    char command[64];
    snprintf(command, sizeof(command), "./+x/portfolio_new.+x %s", hash);
    FILE *pipe = popen(command, "r");
    if (!pipe) {
        fprintf(stderr, "[CHECK] Failed to run portfolio command: %s\n", strerror(errno));
        return 0;
    }
    char output[1024] = {0};
    size_t len = 0;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), pipe)) {
        len += strlen(line);
        if (len < sizeof(output)) strcat(output, line);
    }
    pclose(pipe);
    if (strcmp(output, expected_output) != 0) {
        fprintf(stderr, "Portfolio output mismatch:\nExpected:\n%s\nGot:\n%s\n", expected_output, output);
        return 0;
    }
    return 1;
}

int main() {
    printf("Starting automated tests for stock trading system...\n");

    char mock_time[32], mock_expiry[32];
    get_mock_time(mock_time, sizeof(mock_time));
    get_mock_expiry_time(mock_expiry, sizeof(mock_expiry));

    // Test 1: Lookup stock price
    printf("Test 1: Lookup stock price for NVDA\n");
    reset_user_file(TEST_USER);
    system("echo 'NVDA Current Price: 143.96 (cached, 2025-06-10T23:54:40)' > lookup_output.txt");
    system("cat lookup_output.txt | ./+x/lookup_stock.+x TEST123 NVDA");
    char expected_last_lookup[128];
    snprintf(expected_last_lookup, sizeof(expected_last_lookup), "NVDA,143.96,%s,", mock_time);
    if (check_user_state(TEST_USER, 0.00, "", "", "", expected_last_lookup)) {
        printf("Test 1 PASSED\n");
    } else {
        printf("Test 1 FAILED\n");
    }

    // Test 2: Add $1000 credit
    printf("\nTest 2: Add $1000 credit\n");
    reset_user_file(TEST_USER);
    system("./+x/add_credit.+x TEST123 1000");
    if (check_user_state(TEST_USER, 1000.00, "", "", "", "")) {
        printf("Test 2 PASSED\n");
    } else {
        printf("Test 2 FAILED\n");
    }

    // Test 3: Buy 10 NVDA shares
    printf("\nTest 3: Buy 10 NVDA shares\n");
    reset_user_file(TEST_USER);
    system("echo 'NVDA Current Price: 143.96 (cached, 2025-06-10T23:54:40)' > lookup_output.txt");
    system("./+x/add_credit.+x TEST123 2000");
    system("cat lookup_output.txt | ./+x/buy_stock.+x TEST123 NVDA 10");
    char expected_history[256];
    snprintf(expected_history, sizeof(expected_history), "Buy,NVDA,10.00,143.96,%s,", mock_time);
    if (check_user_state(TEST_USER, 560.40, "NVDA,10.00,", "", expected_history, "")) {
        printf("Test 3 PASSED\n");
    } else {
        printf("Test 3 FAILED\n");
    }

    // Test 4: Sell 5 NVDA shares
    printf("\nTest 4: Sell 5 NVDA shares\n");
    reset_user_file(TEST_USER);
    system("echo 'NVDA Current Price: 143.96 (cached, 2025-06-10T23:54:40)' > lookup_output.txt");
    system("./+x/add_credit.+x TEST123 2000");
    system("cat lookup_output.txt | ./+x/buy_stock.+x TEST123 NVDA 10");
    system("cat lookup_output.txt | ./+x/sell_stock.+x TEST123 NVDA 5");
    char expected_history_sell[512];
    snprintf(expected_history_sell, sizeof(expected_history_sell), "Buy,NVDA,10.00,143.96,%s,Sell,NVDA,-5.00,143.96,%s,", mock_time, mock_time);
    if (check_user_state(TEST_USER, 1280.20, "NVDA,5.00,", "", expected_history_sell, "")) {
        printf("Test 4 PASSED\n");
    } else {
        printf("Test 4 FAILED\n");
    }

    // Test 5: Buy 3 NVDA call options
    printf("\nTest 5: Buy 3 NVDA call options\n");
    reset_user_file(TEST_USER);
    create_mock_option_prices("NVDA");
    system("./+x/add_credit.+x TEST123 1000");
    system("./+x/buy_option.+x TEST123 NVDA 2 3");
    char expected_options[256], expected_history_opt[512];
    snprintf(expected_options, sizeof(expected_options), "NVDA,Call,3.00,143.96,%s,", mock_expiry);
    snprintf(expected_history_opt, sizeof(expected_history_opt), "Call,NVDA,3.00,0.51,%s,%s,143.96,", mock_time, mock_expiry);
    if (check_user_state(TEST_USER, 847.00, "", expected_options, expected_history_opt, "")) {
        printf("Test 5 PASSED\n");
    } else {
        printf("Test 5 FAILED\n");
    }

    // Test 6: Sell 1 NVDA call option
    printf("\nTest 6: Sell 1 NVDA call option\n");
    reset_user_file(TEST_USER);
    create_mock_option_prices("NVDA");
    system("./+x/add_credit.+x TEST123 1000");
    system("./+x/buy_option.+x TEST123 NVDA 2 3");
    system("./+x/sell_option.+x TEST123 NVDA 2 1");
    char expected_options_sell[256], expected_history_sell_opt[512];
    snprintf(expected_options_sell, sizeof(expected_options_sell), "NVDA,Call,2.00,143.96,%s,", mock_expiry);
    snprintf(expected_history_sell_opt, sizeof(expected_history_sell_opt), "Call,NVDA,3.00,0.51,%s,%s,143.96,Call,NVDA,-1.00,0.51,%s,%s,143.96", mock_time, mock_expiry, mock_time, mock_expiry);
    if (check_user_state(TEST_USER, 898.00, "", expected_options_sell, expected_history_sell_opt, "")) {
        printf("Test 6 PASSED\n");
    } else {
        printf("Test 6 FAILED\n");
    }

    // Test 7: View portfolio with stocks and options
    printf("\nTest 7: View portfolio with stocks and options\n");
    reset_user_file(TEST_USER);
    char mock_time_test7[32], mock_expiry_test7[32];
    get_mock_time(mock_time_test7, sizeof(mock_time_test7));
    get_mock_expiry_time(mock_expiry_test7, sizeof(mock_expiry_test7));
    FILE *fp = fopen("usr_acc.TEST123.txt", "w");
    fprintf(fp, "balance,1000.00,watchlist,stocks,NVDA,10.00,options,NVDA,Call,3.00,143.96,%s,history,Buy,NVDA,10.00,143.96,%s,Call,NVDA,3.00,0.51,%s,%s,143.96,last_lookup,NVDA,143.96,%s,\n",
            mock_expiry_test7, mock_time_test7, mock_time_test7, mock_expiry_test7, mock_time_test7);
    fclose(fp);
    char expected_output[512];
    snprintf(expected_output, sizeof(expected_output), "Portfolio:\nStocks:\nNVDA: 10.00 shares\nOptions:\nNVDA: 3.00 Call contracts, Strike: $143.96, Expiry: %s\n", mock_expiry_test7);
    if (check_portfolio_output(TEST_USER, expected_output)) {
        printf("Test 7 PASSED\n");
    } else {
        printf("Test 7 FAILED\n");
    }

    return 0;
}
