#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

void create_directory(const char *path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        mkdir(path, 0700);
    }
}

char* str_replace(char *orig, const char *rep, const char *with) {
    char *result;
    char *ins;
    char *tmp;
    int len_rep;
    int len_with;
    int len_front;
    int count;

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
                fputs(prev_line, temp_fp);
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
        if (!first_line) {
            fputs(prev_line, temp_fp);
        }

        fclose(portfolio_fp);
        fclose(temp_fp);

        remove(portfolio_path);
        rename(temp_path, portfolio_path);
    }
}

void create_corporation(const char* date_str, const char* ticker, const char* player_name, int funding_amount, const char* corp_name, const char* industry_name, const char* country_name) {
    int industry_choice = 3; // Default to generic industry
    if (strcmp(industry_name, "Bank") == 0) {
        industry_choice = 1;
    } else if (strcmp(industry_name, "Insurance") == 0) {
        industry_choice = 2;
    }

    char template_path[256];
    if (industry_choice == 1) {
        strcpy(template_path, "corporations/bank_example.txt");
    } else if (industry_choice == 2) {
        strcpy(template_path, "corporations/insurance_example.txt");
    } else {
        strcpy(template_path, "corporations/corporation_example.txt");
    }

    FILE *template_fp = fopen(template_path, "r");
    if (template_fp == NULL) {
        printf("Error: Could not open template file %s.\n", template_path);
        return;
    }

    char dir_path[256];
    sprintf(dir_path, "corporations/generated/%s", ticker);
    create_directory(dir_path);

    char new_corp_path[256];
    sprintf(new_corp_path, "corporations/generated/%s/%s.txt", ticker, ticker);
    FILE *new_corp_fp = fopen(new_corp_path, "w");
    if (new_corp_fp == NULL) {
        printf("Error: Could not create corporation file %s.\n", new_corp_path);
        fclose(template_fp);
        return;
    }

    char line[512];
    while (fgets(line, sizeof(line), template_fp)) {
        char* replaced_line = str_replace(line, "[CORPORATION NAME]", corp_name);
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

    char shareholders_path[256];
    sprintf(shareholders_path, "corporations/generated/%s/list_shareholders.txt", ticker);
    FILE *shareholders_fp = fopen(shareholders_path, "w");
    if (shareholders_fp == NULL) {
        printf("Error: Could not create shareholders file %s.\n", shareholders_path);
        return;
    }
    fprintf(shareholders_fp, "CORPORATE STOCKHOLDER RECORDS FOR %s (%s)\n", corp_name, ticker);
    fprintf(shareholders_fp, "Incorporated in: %s\n", country_name);
    fprintf(shareholders_fp, "-------------------------------\n");
    fprintf(shareholders_fp, "LIST OF SHAREHOLDERS OF %s (%s):\n", corp_name, ticker);
    fprintf(shareholders_fp, "------------------------------------------------------------\n");
    fprintf(shareholders_fp, "%s\n", player_name);
    fprintf(shareholders_fp, "100%%\n");
    fprintf(shareholders_fp, "--------------------------------\n");
    fclose(shareholders_fp);

    char martian_path[256];
    sprintf(martian_path, "corporations/generated/martian_corporations.txt");
    FILE *martian_fp = fopen(martian_path, "a");
    if (martian_fp) {
        fprintf(martian_fp, "%s (%s) Ind: %s\n", corp_name, ticker, industry_name);
        fclose(martian_fp);
    }

    update_portfolio(player_name, corp_name, ticker, funding_amount);
}

int main(int argc, char *argv[]) {
    if (argc != 8) {
        printf("Usage: ./incorporation.+x date_str ticker player_name funding_amount corp_name industry_name country_name\n");
        return 1;
    }

    char* date_str = argv[1];
    char* ticker = argv[2];
    char* player_name = argv[3];
    int funding_amount = atoi(argv[4]);
    char* corp_name = argv[5];
    char* industry_name = argv[6];
    char* country_name = argv[7];

    create_corporation(date_str, ticker, player_name, funding_amount, corp_name, industry_name, country_name);
    return 0;
}
