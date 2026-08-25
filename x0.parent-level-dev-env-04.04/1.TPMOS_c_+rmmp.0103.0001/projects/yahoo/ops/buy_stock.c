#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>

#define MAX_PATH 4096

char project_root[MAX_PATH] = ".";

void resolve_paths() {
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[1024];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                if (strcmp(line, "project_root") == 0) {
                    char *val = eq + 1;
                    val[strcspn(val, "\n")] = 0;
                    strncpy(project_root, val, MAX_PATH - 1);
                }
            }
        }
        fclose(kvp);
    }
}

float get_state_float(const char* user_hash, const char* key) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/projects/yahoo/pieces/user_%s/state.txt", project_root, user_hash);
    FILE *f = fopen(path, "r");
    if (!f) return 0.0f;
    char line[1024];
    float val = 0.0f;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (eq && strncmp(line, key, strlen(key)) == 0) {
            val = atof(eq + 1);
            break;
        }
    }
    fclose(f);
    return val;
}

void set_state_float(const char* user_hash, const char* key, float val) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/projects/yahoo/pieces/user_%s/state.txt", project_root, user_hash);
    char temp_path[MAX_PATH];
    snprintf(temp_path, sizeof(temp_path), "%s.tmp", path);
    FILE *f = fopen(path, "r");
    FILE *tf = fopen(temp_path, "w");
    if (!tf) return;
    int found = 0;
    if (f) {
        char line[1024];
        while (fgets(line, sizeof(line), f)) {
            char *eq = strchr(line, '=');
            if (eq && strncmp(line, key, strlen(key)) == 0) {
                fprintf(tf, "%s=%.2f\n", key, val);
                found = 1;
            } else {
                fputs(line, tf);
            }
        }
        fclose(f);
    }
    if (!found) fprintf(tf, "%s=%.2f\n", key, val);
    fclose(tf);
    rename(temp_path, path);
}

void update_stocks(const char* user_hash, const char* symbol, float shares) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/projects/yahoo/pieces/user_%s/state.txt", project_root, user_hash);
    char temp_path[MAX_PATH];
    snprintf(temp_path, sizeof(temp_path), "%s.tmp", path);
    
    FILE *f = fopen(path, "r");
    FILE *tf = fopen(temp_path, "w");
    if (!tf) return;
    
    char stocks_line[1024] = "";
    int found_stocks = 0;
    
    if (f) {
        char line[1024];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "stocks=", 7) == 0) {
                strcpy(stocks_line, line + 7);
                stocks_line[strcspn(stocks_line, "\n")] = 0;
                found_stocks = 1;
            } else {
                fputs(line, tf);
            }
        }
        fclose(f);
    }
    
    // Update or add symbol
    char new_stocks[1024] = "";
    char *token = strtok(stocks_line, ",");
    int symbol_found = 0;
    while (token) {
        char *colon = strchr(token, ':');
        if (colon) {
            *colon = '\0';
            if (strcmp(token, symbol) == 0) {
                float current_shares = atof(colon + 1);
                if (strlen(new_stocks) > 0) strcat(new_stocks, ",");
                char buf[64];
                snprintf(buf, sizeof(buf), "%s:%.2f", symbol, current_shares + shares);
                strcat(new_stocks, buf);
                symbol_found = 1;
            } else {
                if (strlen(new_stocks) > 0) strcat(new_stocks, ",");
                *colon = ':';
                strcat(new_stocks, token);
            }
        }
        token = strtok(NULL, ",");
    }
    
    if (!symbol_found) {
        if (strlen(new_stocks) > 0) strcat(new_stocks, ",");
        char buf[64];
        snprintf(buf, sizeof(buf), "%s:%.2f", symbol, shares);
        strcat(new_stocks, buf);
    }
    
    fprintf(tf, "stocks=%s\n", new_stocks);
    fclose(tf);
    rename(temp_path, path);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <user_hash> <symbol> <shares>\n", argv[0]);
        return 1;
    }
    resolve_paths();
    char *user_hash = argv[1];
    char *symbol = argv[2];
    float shares_to_buy = atof(argv[3]);
    for (char *p = symbol; *p; p++) *p = toupper(*p);

    // Get current price using lookup_stock op logic
    char command[512];
    snprintf(command, sizeof(command), "'%s/projects/yahoo/ops/+x/lookup_stock.+x' '%s' '%s'", project_root, user_hash, symbol);
    system(command);
    
    // We can read it back from user state or just re-run read_price
    snprintf(command, sizeof(command), "'%s/projects/yahoo/ops/+x/read_price.+x' '%s'", project_root, symbol);
    FILE *pipe = popen(command, "r");
    float price = 0.0;
    if (pipe) {
        char output[1024];
        if (fgets(output, sizeof(output), pipe)) {
            char *p = strstr(output, "Current Price: ");
            if (p) price = atof(p + 15);
        }
        pclose(pipe);
    }

    if (price <= 0) {
        printf("Could not get price for %s\n", symbol);
        return 1;
    }

    float cost = shares_to_buy * price;
    float balance = get_state_float(user_hash, "balance");
    
    if (cost > balance) {
        printf("Insufficient funds. Need $%.2f, have $%.2f\n", cost, balance);
        return 1;
    }

    set_state_float(user_hash, "balance", balance - cost);
    update_stocks(user_hash, symbol, shares_to_buy);
    
    printf("Bought %.2f shares of %s at $%.2f. New balance: $%.2f\n", shares_to_buy, symbol, price, balance - cost);
    
    return 0;
}
