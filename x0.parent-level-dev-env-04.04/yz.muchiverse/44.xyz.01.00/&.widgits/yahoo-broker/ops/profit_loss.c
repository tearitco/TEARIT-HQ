#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

#define MAX_LINE 256
#define MAX_HISTORY 1000

static void to_upper(char *str) {
    for (char *p = str; *p; p++) *p = toupper(*p);
}

static void read_history(const char *hash, char history_type[MAX_HISTORY][16], char history_symbol[MAX_HISTORY][MAX_LINE],
                         float history_shares[MAX_HISTORY], float history_price[MAX_HISTORY],
                         char history_time[MAX_HISTORY][32], char history_expiration[MAX_HISTORY][32],
                         float history_strike[MAX_HISTORY], int *history_count) {
    char filename[32];
    snprintf(filename, sizeof(filename), "usr_acc.%s.txt", hash);
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "[%s] No user account file: %s\n", filename, strerror(errno));
        exit(1);
    }
    *history_count = 0;

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
        while (token) {
            if (strcmp(token, "history") == 0) {
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
                break;
            }
            token = strtok(NULL, ",");
        }
        free(line_copy);
    }
    fclose(fp);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <user_hash>\n", argv[0]);
        return 1;
    }
    char *hash = argv[1];

    char history_type[MAX_HISTORY][16] = {0};
    char history_symbol[MAX_HISTORY][MAX_LINE] = {0};
    float history_shares[MAX_HISTORY] = {0};
    float history_price[MAX_HISTORY] = {0};
    char history_time[MAX_HISTORY][32] = {0};
    char history_expiration[MAX_HISTORY][32] = {0};
    float history_strike[MAX_HISTORY] = {0};
    int history_count = 0;

    read_history(hash, history_type, history_symbol, history_shares, history_price,
                 history_time, history_expiration, history_strike, &history_count);

    printf("Profit/Loss:\n");
    if (history_count == 0) {
        printf("(no transactions)\n");
    } else {
        float total_pl = 0.0;
        for (int i = 0; i < history_count; i++) {
            float amount = history_shares[i] * history_price[i] * (strcmp(history_type[i], "Call") == 0 || strcmp(history_type[i], "Put") == 0 ? 100 : 1);
            if (history_shares[i] > 0) {
                total_pl -= amount; // Buying reduces profit
            } else {
                total_pl += amount; // Selling increases profit
            }
            printf("%s %s: %.2f %s at $%.2f (%s)%s\n", history_type[i], history_symbol[i],
                   history_shares[i] > 0 ? history_shares[i] : -history_shares[i],
                   strcmp(history_type[i], "Call") == 0 || strcmp(history_type[i], "Put") == 0 ? "contracts" : "shares",
                   history_price[i], history_time[i],
                   history_expiration[i][0] ? ", Expiry: " : "");
            if (history_expiration[i][0]) {
                printf("Strike: $%.2f, Expiry: %s\n", history_strike[i], history_expiration[i]);
            }
        }
        printf("Total Profit/Loss: $%.2f\n", total_pl);
    }
    return 0;
}
