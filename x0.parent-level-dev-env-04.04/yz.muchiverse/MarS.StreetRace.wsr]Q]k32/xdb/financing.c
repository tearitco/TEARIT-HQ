#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h> // Added for timestamp

#define MAX_INDUSTRIES 50
#define MAX_LEN 100

void create_directory(const char *path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        mkdir(path, 0700);
    }
}

void clear_screen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

char* get_active_entity() {
    FILE *fp = fopen("data/active_entity.txt", "r");
    if (fp == NULL) return "N/A";
    static char entity[100];
    fgets(entity, sizeof(entity), fp);
    fclose(fp);
    char *newline = strchr(entity, '\n');
    if (newline) *newline = '\0';
    return entity;
}

int is_corp_name_taken(const char* name) {
    DIR *d;
    struct dirent *dir;
    d = opendir("corporations/generated");
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) {
                continue;
            }

            char filepath[256];
            sprintf(filepath, "corporations/generated/%s/%s.txt", dir->d_name, dir->d_name);
            FILE *fp = fopen(filepath, "r");
            if (fp) {
                char line[256];
                if (fgets(line, sizeof(line), fp)) {
                    char *start = strstr(line, "FINANCIAL PROFILE FOR ");
                    if (start) {
                        start += strlen("FINANCIAL PROFILE FOR ");
                        char *end = strstr(start, " (");
                        if (end) {
                            *end = '\0';
                            if (strcmp(start, name) == 0) {
                                fclose(fp);
                                closedir(d);
                                return 1; // Name is taken
                            }
                        }
                    }
                }
                fclose(fp);
            }
        }
        closedir(d);
    }
    return 0; // Name is not taken
}

char* str_replace(char *orig, const char *rep, const char *with) {
    char *result; // the return string
    char *ins;    // the next insert point
    char *tmp;    // varies
    int len_rep;  // length of rep (the string to remove)
    int len_with; // length of with (the string to replace rep with)
    int len_front; // distance between rep and end of last rep
    int count;    // number of replacements

    if (!orig || !rep) return NULL;
    len_rep = strlen(rep);
    if (len_rep == 0) return NULL;
    if (!with) with = "";
    len_with = strlen(with);

    ins = orig;
    for (count = 0; (tmp = strstr(ins, rep)); ++count) {
        ins = tmp + len_rep;
    }

    tmp = result = malloc(strlen(orig) + (len_with - len_rep) * count + 1);

    if (!result) return NULL;

    char *p = tmp;
    while (count--) {
        ins = strstr(orig, rep);
        len_front = ins - orig;
        p = strncpy(p, orig, len_front) + len_front;
        p = strcpy(p, with) + len_with;
        orig += len_front + len_rep;
    }
    strcpy(p, orig);
    return result;
}

void update_portfolio(const char* player_name, const char* corp_name, const char* ticker, int funding_amount) {
    char portfolio_path[256];
    sprintf(portfolio_path, "players/%s/portfolio.txt", player_name);

    char player_dir[256];
    sprintf(player_dir, "players/%s", player_name);
    create_directory(player_dir);

    FILE *portfolio_fp = fopen(portfolio_path, "r");

    if (portfolio_fp == NULL) {
        // Create new portfolio file
        portfolio_fp = fopen(portfolio_path, "w");
        if (portfolio_fp == NULL) {
            printf("Error: Could not create portfolio file.\n");
            return;
        }

        char upper_player_name[100];
        int i = 0;
        for (; player_name[i]; i++) {
            upper_player_name[i] = toupper(player_name[i]);
        }
        upper_player_name[i] = '\0';

        fprintf(portfolio_fp, "STOCK & BOND PORTFOLIO LISTING FOR %s\n", upper_player_name);
        fprintf(portfolio_fp, "STOCK PORTFOLIO FOR %s (In U.S. Dollars)\n", upper_player_name);
        fprintf(portfolio_fp, "-----------------------------------------------------------------------------------\n");
        fprintf(portfolio_fp, "STOCK PCT%% SHARE DIV. VALUE: ANALYST'S\n");
        fprintf(portfolio_fp, "COMPANY SYM. HELD PRICE YIELD (MILLIONS) RATING\n");
        fprintf(portfolio_fp, "---------------------------- ----- ---- -------- ----- ------------- -------------\n");

        float share_price = (float)funding_amount / 100.0;
        fprintf(portfolio_fp, "%-28s %-5s 100%% %8.2f 0.0%% %13d NOT TRADED\n", corp_name, ticker, share_price, funding_amount);

        fprintf(portfolio_fp, "---------------------------- ----- ---- -------- ----- ------------- -------------\n");
        fprintf(portfolio_fp, "TOTAL STOCK PORTFOLIO VALUE ($ Million): %d\n", funding_amount);
        fprintf(portfolio_fp, "=============\n");
        fprintf(portfolio_fp, "BOND PORTFOLIO FOR %s (In U.S. Dollars)\n", upper_player_name);
        fprintf(portfolio_fp, "-----------------------------------------------------------------------------------\n");
        fprintf(portfolio_fp, "PRICE FACE MARKET BOND YIELD\n");
        fprintf(portfolio_fp, "BOND ISSUER DESCRIPTION PAR=100 VALUE VALUE TO MATURITY\n");
        fprintf(portfolio_fp, "--------------------------- -------------- ------- -------- ----------- -----------\n");
        fprintf(portfolio_fp, "%s OWNS NO BONDS ....\n", player_name);
        fprintf(portfolio_fp, "-----------------------------------------------------------------------------------\n");
        fclose(portfolio_fp);
    } else {
        // Portfolio exists, update it.
        char temp_path[256];
        sprintf(temp_path, "players/%s/portfolio.tmp", player_name);
        FILE *temp_fp = fopen(temp_path, "w");
        if (temp_fp == NULL) {
            printf("Error: Could not create temp portfolio file.\n");
            fclose(portfolio_fp);
            return;
        }

        char line[512], prev_line[512];
        prev_line[0] = '\0';
        int first_line = 1;

        int stock_total = 0;
        long grand_total = 0;
        int grand_total_found = 0;

        // Pre-scan for totals
        fseek(portfolio_fp, 0, SEEK_SET);
        char scan_line[512];
        while(fgets(scan_line, sizeof(scan_line), portfolio_fp)){
            if (strstr(scan_line, "TOTAL STOCK PORTFOLIO VALUE")) {
                char* value_str = strchr(scan_line, ':');
                if(value_str) sscanf(value_str + 1, "%d", &stock_total);
            }
            if (strstr(scan_line, "TOTAL VALUE OF STOCK AND BOND HOLDINGS:")) {
                grand_total_found = 1;
                char* value_str = strstr(scan_line, "$");
                if (value_str) {
                    char* clean_num = str_replace(value_str + 1, ",", "");
                    if(clean_num) {
                        sscanf(clean_num, " %ld", &grand_total);
                        free(clean_num);
                    }
                }
            }
        }
        
        int new_stock_total = stock_total + funding_amount;
        long new_grand_total = grand_total + funding_amount;

        fseek(portfolio_fp, 0, SEEK_SET);

        while (fgets(line, sizeof(line), portfolio_fp)) {
            if (strstr(line, "TOTAL STOCK PORTFOLIO VALUE")) {
                float share_price = (float)funding_amount / 100.0;
                fprintf(temp_fp, "%-28s %-5s 100%% %8.2f 0.0%% %13d NOT TRADED\n", corp_name, ticker, share_price, funding_amount);
                fputs(prev_line, temp_fp); // write separator
                fprintf(temp_fp, "TOTAL STOCK PORTFOLIO VALUE ($ Million): %d\n", new_stock_total);

            } else if (grand_total_found && strstr(line, "TOTAL VALUE OF STOCK AND BOND HOLDINGS:")) {
                fprintf(temp_fp, "TOTAL VALUE OF STOCK AND BOND HOLDINGS: $ %ld Million\n", new_grand_total);
            } else {
                if (!first_line) {
                    fputs(prev_line, temp_fp);
                }
                strcpy(prev_line, line);
                first_line = 0;
            }
        }
        if (!first_line) { // write last line
            fputs(prev_line, temp_fp);
        }

        fclose(portfolio_fp);
        fclose(temp_fp);

        remove(portfolio_path);
        rename(temp_path, portfolio_path);
    }
}

void create_corporation_file(const char* name, const char* ticker, int industry_choice, const char* industry_name, int funding_amount, const char* country_name, const char* player_name) {
    char template_path[256];
    if (industry_choice == 1) { // Bank
        strcpy(template_path, "corporations/bank_example.txt");
    } else if (industry_choice == 2) { // Insurance
        strcpy(template_path, "corporations/insurance_example.txt");
    } else {
        strcpy(template_path, "corporations/corporation_example.txt");
    }

    FILE *template_fp = fopen(template_path, "r");
    if (template_fp == NULL) {
        printf("Error: Could not open template file.\n");
        return;
    }

    char dir_path[256];
    sprintf(dir_path, "corporations/generated/%s", ticker);
    create_directory(dir_path);

    char new_corp_path[256];
    sprintf(new_corp_path, "corporations/generated/%s/%s.txt", ticker, ticker);
    FILE *new_corp_fp = fopen(new_corp_path, "w");
    if (new_corp_fp == NULL) {
        printf("Error: Could not create corporation file.\n");
        fclose(template_fp);
        return;
    }

    char line[512];
    while (fgets(line, sizeof(line), template_fp)) {
        char* replaced_line = str_replace(line, "[CORPORATION NAME]", name);
        char* tmp = replaced_line;
        replaced_line = str_replace(tmp, "[TICKER]", ticker);
        free(tmp);
        tmp = replaced_line;
        replaced_line = str_replace(tmp, "[COUNTRY]", country_name);
        free(tmp);

        if (strstr(replaced_line, "Total Stock Capitalization:") || strstr(replaced_line, "Total Assets:") || strstr(replaced_line, "  Total Deposits:")) {
            char* key = strtok(replaced_line, ":");
            fprintf(new_corp_fp, "%s: %d.00\n", key, funding_amount);
        } else if (strstr(replaced_line, ":")) {
            char* key = strtok(replaced_line, ":");
            fprintf(new_corp_fp, "%s: 0.00\n", key);
        } else {
            fputs(replaced_line, new_corp_fp);
        }
        free(replaced_line);
    }

    fclose(template_fp);
    fclose(new_corp_fp);

    // Create list_shareholders.txt
    char shareholders_path[256];
    sprintf(shareholders_path, "corporations/generated/%s/list_shareholders.txt", ticker);
    FILE *shareholders_fp = fopen(shareholders_path, "w");
    if (shareholders_fp == NULL) {
        printf("Error: Could not create shareholders file.\n");
        return;
    }
    fprintf(shareholders_fp, "CORPORATE STOCKHOLDER RECORDS FOR %s (%s)\n", name, ticker);
    fprintf(shareholders_fp, "Incorporated in: %s\n", country_name);
    fprintf(shareholders_fp, "-------------------------------\n");
    fprintf(shareholders_fp, "LIST OF SHAREHOLDERS OF %s (%s):\n", name, ticker);
    fprintf(shareholders_fp, "------------------------------------------------------------\n");
    fprintf(shareholders_fp, "%s\n", player_name);
    fprintf(shareholders_fp, "100%%\n");
    fprintf(shareholders_fp, "--------------------------------\n");
    fclose(shareholders_fp);

    // Append to martian_corporations.txt
    char martian_path[256];
    sprintf(martian_path, "corporations/generated/martian_corporations.txt");
    FILE *martian_fp = fopen(martian_path, "a");
    if (martian_fp) {
        fprintf(martian_fp, "%s (%s) Ind: %s\n", name, ticker, industry_name);
        fclose(martian_fp);
    }

    // Append to master_ledger.txt
    create_directory("data"); // Ensure data directory exists
    char ledger_path[256];
    sprintf(ledger_path, "data/master_ledger.txt");
    FILE *ledger_fp = fopen(ledger_path, "a");
    if (ledger_fp == NULL) {
        printf("Error: Could not open master_ledger.txt.\n");
        return;
    }
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char date_str[20];
    strftime(date_str, sizeof(date_str), "%Y-%m-%d %H:%M:%S", t);
    fprintf(ledger_fp, "%s | %s_cash | %s_cash | %d | Company creation\n", date_str, ticker, player_name, funding_amount);
    fclose(ledger_fp);

    update_portfolio(player_name, name, ticker, funding_amount);

    printf("\nCongratulations! You have successfully founded %s (%s).\n", name, ticker);
    printf("Press Enter to continue...");
    getchar();
}

void startup_new_corp(const char* player_name) {
    clear_screen();
    printf("--- Startup New Corp ---\n");

    FILE *fp = fopen("corporations/36_industries_wsr.txt", "r");
    if (fp == NULL) {
        printf("Error: Could not open industries file.\n");
        return;
    }

    char industries[MAX_INDUSTRIES][MAX_LEN];
    int num_industries = 0;
    while (fgets(industries[num_industries], MAX_LEN, fp) && num_industries < MAX_INDUSTRIES) {
        industries[num_industries][strcspn(industries[num_industries], "\n")] = 0;
        num_industries++;
    }
    fclose(fp);

    int industry_choice;
    while (1) {
        printf("Select the industry for your new corporation:\n");
        for (int i = 0; i < num_industries; i++) {
            printf("%s\n", industries[i]);
        }
        printf("\nEnter your choice (1-%d): ", num_industries);
        fflush(stdout);
        if (scanf("%d", &industry_choice) == 1 && industry_choice >= 1 && industry_choice <= num_industries) {
            while (getchar() != '\n');
            break;
        } else {
            printf("Invalid choice. Please try again.\n");
            while (getchar() != '\n');
        }
    }
    char* chosen_industry_str = industries[industry_choice - 1];
    char* industry_name = strchr(chosen_industry_str, '.');
    if (industry_name != NULL) {
        industry_name++;
        while (*industry_name == ' ') industry_name++;
    } else {
        industry_name = chosen_industry_str;
    }

    fp = fopen("governments/generated/gov-list.txt", "r");
    if (fp == NULL) {
        printf("Error: Could not open governments file.\n");
        return;
    }
    char governments[MAX_INDUSTRIES][MAX_LEN];
    int num_governments = 0;
    char header[MAX_LEN];
    fgets(header, MAX_LEN, fp);
    while (fgets(governments[num_governments], MAX_LEN, fp) && num_governments < MAX_INDUSTRIES) {
        governments[num_governments][strcspn(governments[num_governments], "\n")] = 0;
        num_governments++;
    }
    fclose(fp);
    int country_choice;
    while (1) {
        printf("\nSelect the country of incorporation:\n");
        for (int i = 0; i < num_governments; i++) {
            char temp[MAX_LEN];
            strcpy(temp, governments[i]);
            char* c_name = strtok(temp, "\t");
            c_name = strtok(NULL, "\t");
            printf("%d. %s\n", i + 1, c_name);
        }
        printf("\nEnter your choice (1-%d): ", num_governments);
        fflush(stdout);
        if (scanf("%d", &country_choice) == 1 && country_choice >= 1 && country_choice <= num_governments) {
            while (getchar() != '\n');
            break;
        } else {
            printf("Invalid choice. Please try again.\n");
            while (getchar() != '\n');
        }
    }
    char chosen_country_line[MAX_LEN];
    strcpy(chosen_country_line, governments[country_choice - 1]);
    char* country_name = strtok(chosen_country_line, "\t");
    country_name = strtok(NULL, "\t");

    int funding_amount;
    int min_funding = 100;
    if (industry_choice == 1 || industry_choice == 2) {
        min_funding = 1000;
    }

    while (1) {
        printf("Enter the amount to fund the corporation (minimum %d): ", min_funding);
        fflush(stdout);
        if (scanf("%d", &funding_amount) == 1 && funding_amount >= min_funding) {
            while (getchar() != '\n');
            break;
        } else {
            printf("Invalid amount. Please enter a number greater than or equal to %d.\n", min_funding);
            while (getchar() != '\n');
        }
    }

    char corp_name[100];
    while (1) {
        printf("Enter the name for the new corporation (5-25 characters): ");
        fflush(stdout);
        fgets(corp_name, sizeof(corp_name), stdin);
        corp_name[strcspn(corp_name, "\n")] = 0;

        if (strlen(corp_name) >= 5 && strlen(corp_name) <= 25) {
            if (!is_corp_name_taken(corp_name)) {
                break;
            } else {
                printf("Corporation name already exists. Please choose another one.\n");
            }
        } else {
            printf("Invalid name length. Please try again.\n");
        }
    }

    char ticker[10];
    char upper_ticker[10];
    while (1) {
        printf("Enter the ticker symbol (1-6 characters): ");
        fflush(stdout);
        fgets(ticker, sizeof(ticker), stdin);
        ticker[strcspn(ticker, "\n")] = 0;

        for (int i = 0; ticker[i]; i++) {
            upper_ticker[i] = toupper(ticker[i]);
        }
        upper_ticker[strlen(ticker)] = '\0';

        if (strlen(upper_ticker) >= 1 && strlen(upper_ticker) <= 6) {
            char dirpath[256];
            sprintf(dirpath, "corporations/generated/%s", upper_ticker);
            DIR* test_dir = opendir(dirpath);
            if (test_dir) {
                closedir(test_dir);
                printf("Ticker symbol already exists. Please choose another one.\n");
            } else {
                break;
            }
        } else {
            printf("Invalid ticker length. Please try again.\n");
        }
    }

    create_corporation_file(corp_name, upper_ticker, industry_choice, industry_name, funding_amount, country_name, player_name);
}

void display_financing_menu(const char* player_name) {
    char* active_entity = get_active_entity();
    int is_player_active = (strcmp(active_entity, player_name) == 0);
    int choice;
    int back_option = is_player_active ? 3 : 9;

    do {
        clear_screen();
        printf("--- Financing ---\n");

        if (is_player_active) {
            printf("1. Startup New Corp\n");
            printf("2. Capital Contribution\n");
            printf("3. Back to main menu\n");
        } else {
            printf("1. Startup New Corp\n");
            printf("2. Capital Contribution\n");
            printf("3. Public Stock Offering\n");
            printf("4. Issue Bonds\n");
            printf("5. Buy Back or Call Bonds\n");
            printf("6. Extraordinary Dividend\n");
            printf("7. Tax-Free Liquidation\n");
            printf("8. Spin Off Subsidiary\n");
            printf("9. Back to main menu\n");
        }

        printf("\nEnter your choice: ");
        fflush(stdout);
        scanf("%d", &choice);
        while (getchar() != '\n');

        if (choice == back_option) {
            break;
        }

        switch (choice) {
            case 1:
                startup_new_corp(player_name);
                break;
            default:
                printf("\nSelection %d is not yet implemented.", choice);
                printf("\nPress Enter to continue...");
                getchar();
                break;
        }

    } while (choice != back_option);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: ./financing player_name\n");
        return 1;
    }
    display_financing_menu(argv[1]);
    return 0;
}
