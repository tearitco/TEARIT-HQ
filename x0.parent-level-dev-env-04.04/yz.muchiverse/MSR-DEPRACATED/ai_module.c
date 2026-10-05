#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <ctype.h>

#define MAX_CORPS 100

// Simplified corporation structure for AI
typedef struct {
    char name[100];
    char ticker[20];
    char industry[100];
    float cash;
    float net_worth;
    // Add more fields as needed for AI decisions
} AI_Corporation;

// Function to read generated corporation sheets
int load_corporation_data(AI_Corporation corporations[]) {
    DIR *d;
    struct dirent *dir;
    d = opendir("corporations/generated");
    int count = 0;
    if (d) {
        while ((dir = readdir(d)) != NULL && count < MAX_CORPS) {
            if (strstr(dir->d_name, ".txt")) {
                char file_path[512];
                snprintf(file_path, sizeof(file_path), "corporations/generated/%s", dir->d_name);
                FILE *fp = fopen(file_path, "r");
                if (fp) {
                    char line[256];
                    while(fgets(line, sizeof(line), fp)) {
                        line[strcspn(line, "\r\n")] = 0; // remove newline

                        if(strstr(line, "FINANCIAL BALANCE SHEET:")) {
                            char *name_start = strstr(line, ":") + 1;
                            char *ticker_start = strstr(name_start, "(");
                            if (ticker_start) {
                                *ticker_start = '\0';
                                char *ticker_end = strstr(ticker_start, ")");
                                if (ticker_end) *ticker_end = '\0';

                                while(isspace((unsigned char)*name_start)) name_start++;
                                char *name_end = ticker_start - 1;
                                while(name_end > name_start && isspace((unsigned char)*name_end)) {
                                    *name_end = '\0';
                                    name_end--;
                                }
                                strncpy(corporations[count].name, name_start, sizeof(corporations[count].name) - 1);
                                corporations[count].name[sizeof(corporations[count].name) - 1] = '\0';

                                strncpy(corporations[count].ticker, ticker_start + 1, sizeof(corporations[count].ticker) - 1);
                                corporations[count].ticker[sizeof(corporations[count].ticker) - 1] = '\0';
                            }
                        }
                        if(strstr(line, "Free Cash and Equivalents")) {
                            sscanf(line, "Free Cash and Equivalents %f", &corporations[count].cash);
                        }
                        if(strstr(line, "Equity (Net Worth):")){
                             sscanf(line, "Equity (Net Worth): %f", &corporations[count].net_worth);
                        }
                        if(strstr(line, "Industry Group:")){
                             char* industry_info = strstr(line, "Industry Group:") + 15;
                             while(isspace((unsigned char)*industry_info)) industry_info++;
                             strncpy(corporations[count].industry, industry_info, sizeof(corporations[count].industry) - 1);
                             corporations[count].industry[sizeof(corporations[count].industry) - 1] = '\0';
                        }
                    }
                    fclose(fp);
                    count++;
                }
            }
        }
        closedir(d);
    }
    return count;
}


// Placeholder for AI to make a decision for a corporation
void corporation_ai_turn(AI_Corporation *corp, AI_Corporation all_corps[], int num_corps) {
    printf("AI evaluating turn for %s\n", corp->name);
    // Simple AI logic: if cash is high, consider investing
    // if cash is low, consider seeking a loan.
    if (corp->cash > 500) { // Arbitrary threshold
        printf("  %s has high cash reserves, considering investments.\n", corp->name);
    } else if (corp->cash < 150) {
        printf("  %s has low cash reserves, considering a loan.\n", corp->name);
    }

    // Placeholder for inter-corporation business
    // Pick a random other corporation to interact with
    int other_corp_index = rand() % num_corps;
    if (strcmp(corp->ticker, all_corps[other_corp_index].ticker) != 0) {
        printf("  %s is considering doing business with %s\n", corp->name, all_corps[other_corp_index].name);
    }
}

int main() {
    srand(time(NULL));

    AI_Corporation corporations[MAX_CORPS] = {0};
    int num_corps = load_corporation_data(corporations);

    if (num_corps > 0) {
        printf("Loaded data for %d corporations.\n", num_corps);
        // Main AI loop - for now, just one turn for each corp
        for (int i = 0; i < num_corps; i++) {
            corporation_ai_turn(&corporations[i], corporations, num_corps);
        }
    } else {
        printf("AI module: No corporation data found.\n");
    }

    return 0;
}
