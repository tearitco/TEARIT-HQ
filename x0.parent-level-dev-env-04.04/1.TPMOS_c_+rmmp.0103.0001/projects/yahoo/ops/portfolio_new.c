#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <user_hash>\n", argv[0]);
        return 1;
    }
    resolve_paths();
    char *hash = argv[1];
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/projects/yahoo/pieces/user_%s/state.txt", project_root, hash);
    
    FILE *f = fopen(path, "r");
    if (!f) {
        printf("No portfolio found for user %s\n", hash);
        return 1;
    }
    
    char line[1024];
    float balance = 0.0;
    char stocks_line[1024] = "";
    
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "balance=", 8) == 0) balance = atof(line + 8);
        if (strncmp(line, "stocks=", 7) == 0) strcpy(stocks_line, line + 7);
    }
    fclose(f);
    
    printf("--- Portfolio for %s ---\n", hash);
    printf("Cash Balance: $%.2f\n", balance);
    printf("Stocks:\n");
    
    char *token = strtok(stocks_line, ",");
    float total_value = balance;
    while (token) {
        char *colon = strchr(token, ':');
        if (colon) {
            *colon = '\0';
            char *symbol = token;
            float shares = atof(colon + 1);
            
            // For a real app, we'd call lookup_stock here to get live prices
            // For now, we'll just show shares
            printf("  %s: %.2f shares\n", symbol, shares);
        }
        token = strtok(NULL, ",");
    }
    
    return 0;
}
