// management.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Function to check if a file exists
int file_exists(const char *filename) {
    FILE *file;
    if ((file = fopen(filename, "r"))) {
        fclose(file);
        return 1;
    }
    return 0;
}

char* get_active_entity() {
    FILE *fp = fopen("data/active_entity.txt", "r");
    if (fp == NULL) return "N/A";
    static char entity[100];
    if (fgets(entity, sizeof(entity), fp) == NULL) {
        fclose(fp);
        return "N/A";
    }
    fclose(fp);
    char *newline = strchr(entity, '\n');
    if (newline) *newline = '\0';
    if (strlen(entity) == 0) {
        return "N/A";
    }
    return entity;
}

int main() {
    system("clear");
    char* active_entity = get_active_entity();

    if (strcmp(active_entity, "N/A") == 0) {
        printf("No active entity selected.\n");
        printf("\nPress Enter to return to the main menu.");
        getchar();
        return 0;
    }

    char player_portfolio_path[256];
    sprintf(player_portfolio_path, "players/%s/portfolio.txt", active_entity);

    if (file_exists(player_portfolio_path)) {
        printf("placeholder for player management\n");
    } else {
        printf("\n--- %s Management ---\n", active_entity);
        printf("1. Elect me as CEO of %s\n", active_entity);
        printf("2. Resign me as CEO of %s\n", active_entity);
        printf("3. Change Managers\n");
        printf("4. Set Dividend Payout\n");
        printf("5. Set R&D Spend\n");
        printf("6. Set Marketing Spend\n");
        printf("7. Set Growth Rate\n");
        printf("8. Micromanage\n");
        printf("9. Legal\n");
        printf("10. Toggle AutoPilot ON\n");
        printf("----------------------------------\n");
    }

    printf("\nPress Enter to return to the main menu.");
    getchar();

    return 0;
}
