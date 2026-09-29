#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>

#define MAX_COMPANIES 100
#define MAX_LINE_LEN 256

typedef struct {
    char name[100];
    char ticker[10];
} Company;

int read_corporations(Company companies[]) {
    DIR *d;
    struct dirent *dir;
    d = opendir("corporations/generated");
    if (!d) {
        printf("Error: Could not open corporations/generated\n");
        return 0;
    }

    int count = 0;
    char line[MAX_LINE_LEN];
    while ((dir = readdir(d)) != NULL && count < MAX_COMPANIES) {
        if (dir->d_type == DT_DIR && strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
            strncpy(companies[count].ticker, dir->d_name, sizeof(companies[count].ticker) - 1);
            companies[count].ticker[sizeof(companies[count].ticker) - 1] = '\0';

            char filepath[1024];
            snprintf(filepath, sizeof(filepath), "corporations/generated/%s/%s.txt", dir->d_name, dir->d_name);
            FILE *fp = fopen(filepath, "r");
            if (fp) {
                if (fgets(line, sizeof(line), fp)) {
                    line[strcspn(line, "\n")] = 0;
                    char *name_start = strstr(line, "FINANCIAL PROFILE FOR ");
                    if (name_start) {
                        name_start += strlen("FINANCIAL PROFILE FOR ");
                        char *name_end = strstr(name_start, " (");
                        if (name_end) {
                            *name_end = '\0';
                        }
                        strncpy(companies[count].name, name_start, sizeof(companies[count].name) - 1);
                        companies[count].name[sizeof(companies[count].name) - 1] = '\0';
                    }
                }
                fclose(fp);
            } else {
                strncpy(companies[count].name, companies[count].ticker, sizeof(companies[count].name) - 1);
            }
            count++;
        }
    }
    closedir(d);
    return count;
}

void save_active_entity(const char *ticker) {
    FILE *fp = fopen("data/active_entity.txt", "w");
    if (fp == NULL) {
        printf("Error: Could not open data/active_entity.txt for writing.\n");
        return;
    }
    fprintf(fp, "%s\n", ticker);
    fclose(fp);
    printf("Selected entity '%s' saved.\n", ticker);
}

int main() {
    Company companies[MAX_COMPANIES];
    int num_companies = read_corporations(companies);

    if (num_companies == 0) {
        return 1;
    }

    printf("\n1. Enter stock symbol\n");
    printf("2. Look up company\n");
    printf("Enter your choice: ");

    int choice;
    scanf("%d", &choice);

    if (choice == 1) {
        char symbol[10];
        printf("Enter stock symbol: ");
        scanf("%9s", symbol);

        int found = 0;
        for (int i = 0; i < num_companies; i++) {
            if (strcasecmp(symbol, companies[i].ticker) == 0) {
                save_active_entity(companies[i].ticker);
                found = 1;
                break;
            }
        }
        if (!found) {
            printf("Entity doesn't exist.\n");
        }
    } else if (choice == 2) {
        for (int i = 0; i < num_companies; i++) {
            printf("%d. %s (%s)\n", i + 1, companies[i].name, companies[i].ticker);
        }
        printf("Enter your choice: ");
        int lookup_choice;
        scanf("%d", &lookup_choice);

        if (lookup_choice > 0 && lookup_choice <= num_companies) {
            save_active_entity(companies[lookup_choice - 1].ticker);
        } else {
            printf("Invalid choice.\n");
        }
    } else {
        printf("Invalid choice.\n");
    }

    printf("Press Enter to continue...\n");
    while(getchar()!='\n'); // Clear input buffer
    getchar(); // Wait for Enter

    return 0;
}
