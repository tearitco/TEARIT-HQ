#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

#define MAX_LINE 256
#define MAX_WATCHLIST 50
#define MAX_STOCKS 50
#define MAX_HISTORY 1000

static void to_upper(char *str) {
    for (char *p = str; *p; p++) *p = toupper(*p);
}

static void read_user_account(const char *hash, float *balance, char watchlist[MAX_WATCHLIST][MAX_LINE], int *watchlist_count, 
                              char stocks[MAX_STOCKS][MAX_LINE], float shares[MAX_STOCKS], int *stocks_count,
                              char history_type[MAX_HISTORY][16], char history_symbol[MAX_HISTORY][MAX_LINE],
                              float history_shares[MAX_HISTORY], float history_price[MAX_HISTORY], 
                              char history_time[MAX_HISTORY][32], char history_expiration[MAX_HISTORY][32],
                              float history_strike[MAX_HISTORY], int *history_count) {
    char filename[32];
    snprintf(filename, sizeof(filename), "usr_acc.%s.txt", hash);
    FILE *fp = fopen(filename, "r");
    *balance = 0.0;
    *watchlist_count = 0;
    *stocks_count = 0;
    *history_count = 0;

    if (!fp) {
        fprintf(stderr, "[%s] No user account file\n", filename);
        return;
    }

    char line[MAX_LINE];
    if (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = 0;
        char *line_copy = strdup(line);
        if (!line_copy) {
            fprintf(stderr, "[%s] Memory allocation failed\n", filename);
            fclose(fp);
            return;
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
                if (strcmp(token, "history") == 0) break;
                strncpy(stocks[*stocks_count], token, MAX_LINE - 1);
                to_upper(stocks[*stocks_count]);
                token = strtok(NULL, ",");
                if (token) {
                    shares[*stocks_count] = atof(token);
                    (*stocks_count)++;
                }
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
        free(line_copy);
    }
    fclose(fp);
    if (*watchlist_count < 0 || *stocks_count < 0) {
        fprintf(stderr, "[%s] Invalid counts: watchlist=%d, stocks=%d\n", filename, *watchlist_count, *stocks_count);
        *watchlist_count = 0;
        *stocks_count = 0;
    }
}

static void write_user_account(const char *hash, float balance, char watchlist[MAX_WATCHLIST][MAX_LINE], int watchlist_count, 
                               char stocks[MAX_STOCKS][MAX_LINE], float shares[MAX_STOCKS], int stocks_count,
                               char history_type[MAX_HISTORY][16], char history_symbol[MAX_HISTORY][MAX_LINE],
                               float history_shares[MAX_HISTORY], float history_price[MAX_HISTORY], 
                               char history_time[MAX_HISTORY][32], char history_expiration[MAX_HISTORY][32],
                               float history_strike[MAX_HISTORY], int history_count) {
    char filename[32];
    snprintf(filename, sizeof(filename), "usr_acc.%s.txt", hash);
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "[%s] Failed to open for writing\n", filename);
        return;
    }

    fprintf(fp, "balance,%.2f,watchlist", balance);
    for (int i = 0; i < watchlist_count; i++) {
        fprintf(fp, ",%s", watchlist[i]);
    }
    fprintf(fp, ",stocks");
    for (int i = 0; i < stocks_count; i++) {
        fprintf(fp, ",%s,%.2f", stocks[i], shares[i]);
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
    fprintf(fp, "\n");
    fclose(fp);
    fprintf(stderr, "[%s] Updated: balance=%.2f, watchlist=%d, stocks=%d, history=%d\n",
            filename, balance, watchlist_count, stocks_count, history_count);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <user_hash> <amount>\n", argv[0]);
        return 1;
    }

    char *hash = argv[1];
    float credit = atof(argv[2]);
    if (credit <= 0) {
        fprintf(stderr, "Invalid amount: %s\n", argv[2]);
        return 1;
    }

    float balance = 0.0;
    char watchlist[MAX_WATCHLIST][MAX_LINE];
    memset(watchlist, 0, sizeof(watchlist));
    int watchlist_count = 0;
    char stocks[MAX_STOCKS][MAX_LINE];
    memset(stocks, 0, sizeof(stocks));
    float shares[MAX_STOCKS];
    memset(shares, 0, sizeof(shares));
    int stocks_count = 0;
    char history_type[MAX_HISTORY][16];
    memset(history_type, 0, sizeof(history_type));
    char history_symbol[MAX_HISTORY][MAX_LINE];
    memset(history_symbol, 0, sizeof(history_symbol));
    float history_shares[MAX_HISTORY];
    memset(history_shares, 0, sizeof(history_shares));
    float history_price[MAX_HISTORY];
    memset(history_price, 0, sizeof(history_price));
    char history_time[MAX_HISTORY][32];
    memset(history_time, 0, sizeof(history_time));
    char history_expiration[MAX_HISTORY][32];
    memset(history_expiration, 0, sizeof(history_expiration));
    float history_strike[MAX_HISTORY];
    memset(history_strike, 0, sizeof(history_strike));
    int history_count = 0;

    read_user_account(hash, &balance, watchlist, &watchlist_count, stocks, shares, &stocks_count,
                     history_type, history_symbol, history_shares, history_price, history_time,
                     history_expiration, history_strike, &history_count);

    balance += credit;
    write_user_account(hash, balance, watchlist, watchlist_count, stocks, shares, stocks_count,
                      history_type, history_symbol, history_shares, history_price, history_time,
                      history_expiration, history_strike, history_count);

    printf("Added $%.2f. New balance: $%.2f\n", credit, balance);
    return 0;
}
