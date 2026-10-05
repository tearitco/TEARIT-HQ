#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <stdbool.h>
// Finds the first all-caps symbol in a line
const char* find_symbol_in_line(const char* line) {
    static char symbol[100];
    char line_copy[256];
    if (strlen(line) >= sizeof(line_copy)) {
        return "N/A";
    }
    strcpy(line_copy, line);

    char* token = strtok(line_copy, " \t\n");
    while (token != NULL) {
        bool is_symbol = true;
        if (strlen(token) < 2) {
            is_symbol = false;
        } else {
            for (int i = 0; i < strlen(token); i++) {
                if (!isupper(token[i]) && !isdigit(token[i])) {
                    is_symbol = false;
                    break;
                }
            }
        }

        if (is_symbol) {
            if (strchr(token, '%') == NULL) {
                strcpy(symbol, token);
                return symbol;
            }
        }
        token = strtok(NULL, " \t\n");
    }
    return "N/A";
}

int main(int argc, char *argv[]) {
    printf("\x1b[2J\x1b[H");
    if (argc < 2) {
        printf("Usage: ./display_portfolio <player_name>\n");
        return 1;
    }
    char* player_name = argv[1];

    char portfolio_path[100];
    sprintf(portfolio_path, "players/%s/portfolio.txt", player_name);

    FILE *fp = fopen(portfolio_path, "r");

    if (fp == NULL) {
        printf("No portfolio found for %s.\n", player_name);
        printf("Press enter to return to the main menu.");
        fflush(stdout);
        getchar();
        return 1;
    }

    char stock_lines[100][256];
    int stock_count = 0;
    char buffer[256];
    bool in_stock_section = false;

    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        if (strstr(buffer, "STOCK PORTFOLIO FOR")) {
            in_stock_section = true;
            continue;
        }
        if (strstr(buffer, "TOTAL STOCK PORTFOLIO VALUE")) {
            in_stock_section = false;
            break;
        }

        if (in_stock_section) {
            if (strstr(buffer, "STOCK") && strstr(buffer, "SHARES")) continue;
            
            char* p = buffer;
            while(isspace((unsigned char)*p)) p++;
            if (*p == '\0') continue;

            strcpy(stock_lines[stock_count], buffer);
            stock_count++;
        }
    }
    fclose(fp);

    printf("Portfolio Holdings for %s:\n", player_name);
    printf("----------------------------------\n");

    if (stock_count == 0) {
        printf("No stocks found in portfolio.\n");
    } else {
        for (int i = 0; i < stock_count; i++) {
            char* newline = strchr(stock_lines[i], '\n');
            if (newline) *newline = '\0';
            printf("%d. %s\n", i + 1, stock_lines[i]);
        }
    }

    printf("----------------------------------\n");
    printf("0. Back to Main Menu\n");
    printf("\nSelect an entity to make it active: ");
    fflush(stdout);

    int choice = -1;
    scanf("%d", &choice);

    if (choice > 0 && choice <= stock_count) {
        const char* entity_name = find_symbol_in_line(stock_lines[choice - 1]);
        if (strcmp(entity_name, "N/A") != 0) {
            FILE *entity_fp = fopen("data/active_entity.txt", "w");
            if (entity_fp != NULL) {
                fprintf(entity_fp, "%s", entity_name);
                fclose(entity_fp);
            }
        }
    }

    return 0;
}
