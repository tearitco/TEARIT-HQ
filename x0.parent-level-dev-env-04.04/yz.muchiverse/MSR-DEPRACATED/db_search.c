#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <dirent.h> // Added for directory operations

#define MAX_STRING_LEN 50

// Search criteria variables
int high_return_on_equity = 20;
int low_pe_ratio = 10;
int stock_price_less_than_net_worth = 95;
int dividend_yield = 5;
int min_market_cap = 0;
int max_market_cap = 1000;
int earnings_up_next_quarter = 10;
char management_rated[MAX_STRING_LEN] = "Good";
char credit_rating[MAX_STRING_LEN] = "A";
char analyst_rating[MAX_STRING_LEN] = "Buy";
// New variables for options 11, 12, and 13
int industry_supply_demand_improving = 0; // 0 = Off, 1 = On
int publicly_traded_only = 0; // 0 = Off, 1 = On
int has_bonds_issued = 0; // 0 = Off, 1 = On

// Struct to hold corporation data
struct Corporation {
    char name[100];
    char ticker[10];
    double net_worth_per_share;
    double current_stock_price;
    double total_stock_capitalization; // in millions
    double long_term_bonds_issued;
    int return_on_equity;
    int pe_ratio;
    int dividend_yield;
    char management_rated[MAX_STRING_LEN];
    char credit_rating[MAX_STRING_LEN];
    char analyst_rating[MAX_STRING_LEN];
};

// Function prototypes
int parse_corporation_file(const char *filename, struct Corporation *corp, const char *ticker_from_filename);
void display_search_results();

// Function to parse corporation data from a file
int parse_corporation_file(const char *filename, struct Corporation *corp, const char *ticker_from_filename) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        return 0; // Cannot open file
    }

    strncpy(corp->ticker, ticker_from_filename, sizeof(corp->ticker) - 1);
    corp->ticker[sizeof(corp->ticker) - 1] = '\0';

    char line[256];
    char *ptr;

    // Initialize corp data to default values
    memset(corp->name, 0, sizeof(corp->name));
    corp->net_worth_per_share = 0.0;
    corp->current_stock_price = 0.0;
    corp->total_stock_capitalization = 0.0;
    corp->long_term_bonds_issued = 0.0;
    corp->return_on_equity = 0;
    corp->pe_ratio = 0;
    corp->dividend_yield = 0;
    memset(corp->management_rated, 0, sizeof(corp->management_rated));
    memset(corp->credit_rating, 0, sizeof(corp->credit_rating));
    memset(corp->analyst_rating, 0, sizeof(corp->analyst_rating));


    // Read the first line for the company name
    if (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = 0; // Remove newline
        char *name_start = strstr(line, "FINANCIAL PROFILE FOR ");
        if (name_start) {
            name_start += strlen("FINANCIAL PROFILE FOR ");
            char *name_end = strstr(name_start, " (");
            if (name_end) {
                *name_end = '\0';
            }
            strncpy(corp->name, name_start, sizeof(corp->name) - 1);
        } else {
            // Fallback for different format
            name_start = strstr(line, "FINANCIAL BALANCE SHEET: ");
            if (name_start) {
                name_start += strlen("FINANCIAL BALANCE SHEET: ");
                char *name_end = strstr(name_start, " (");
                if (name_end) {
                    *name_end = '\0';
                }
                strncpy(corp->name, name_start, sizeof(corp->name) - 1);
            }
        }
    }

    // Parse other values
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = 0; // Remove newline

        if ((ptr = strstr(line, "Net Worth Per Share:"))) 
            sscanf(ptr + strlen("Net Worth Per Share:"), "%lf", &corp->net_worth_per_share);
        else if ((ptr = strstr(line, "Current Stock Price:"))) 
            sscanf(ptr + strlen("Current Stock Price:"), "%lf", &corp->current_stock_price);
        else if ((ptr = strstr(line, "Total Stock Capitalization:"))) 
            sscanf(ptr + strlen("Total Stock Capitalization:"), "%lf", &corp->total_stock_capitalization);
        else if ((ptr = strstr(line, "Long-Term Bonds Issued:"))) 
            sscanf(ptr + strlen("Long-Term Bonds Issued:"), "%lf", &corp->long_term_bonds_issued);
        else if ((ptr = strstr(line, "Return on Equity (2024):"))) 
            if (strstr(ptr, "NMF")) corp->return_on_equity = 0; else sscanf(ptr + strlen("Return on Equity (2024):"), "%d", &corp->return_on_equity);
        else if ((ptr = strstr(line, "Price/Earnings Ratio:"))) 
            if (strstr(ptr, "NMF")) corp->pe_ratio = 0; else sscanf(ptr + strlen("Price/Earnings Ratio:"), "%d", &corp->pe_ratio);
        else if ((ptr = strstr(line, "Dividend Yield:"))) 
            sscanf(ptr + strlen("Dividend Yield:"), "%d", &corp->dividend_yield);
        else if ((ptr = strstr(line, "Credit Rating:"))) 
            sscanf(ptr + strlen("Credit Rating:"), "%s", corp->credit_rating);

    }

    fclose(fp);
    return 1;
}


void display_search_results() {
    printf("\n--- Search Results ---\n");
    DIR *d;
    struct dirent *dir;
    char filepath[1024];
    struct Corporation corp;
    int found_results = 0;
    char found_tickers[100][10];

    d = opendir("corporations/generated");
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) {
                continue;
            }

            char ticker_name[10];
            strncpy(ticker_name, dir->d_name, sizeof(ticker_name) - 1);
            ticker_name[sizeof(ticker_name) - 1] = '\0';

            snprintf(filepath, sizeof(filepath), "corporations/generated/%s/%s.txt", dir->d_name, dir->d_name);
            if (parse_corporation_file(filepath, &corp, ticker_name)) {
                // Apply search criteria
                int matches = 1;

                if (corp.net_worth_per_share > 0 && stock_price_less_than_net_worth > 0 && corp.current_stock_price * 100.0 / corp.net_worth_per_share >= stock_price_less_than_net_worth) {
                    matches = 0;
                }

                if (corp.total_stock_capitalization < min_market_cap || corp.total_stock_capitalization > max_market_cap) {
                    matches = 0;
                }

                if (publicly_traded_only && corp.total_stock_capitalization <= 0) {
                    matches = 0;
                }

                if (has_bonds_issued && corp.long_term_bonds_issued <= 0) {
                    matches = 0;
                }

                if (matches) {
                    printf("[%d] %s (%s)\n", found_results + 1, corp.name, corp.ticker);
                    strncpy(found_tickers[found_results], corp.ticker, sizeof(found_tickers[found_results]) - 1);
                    found_tickers[found_results][sizeof(found_tickers[found_results]) - 1] = '\0';
                    found_results++;
                }
            }
        }
        closedir(d);
    }

    if (found_results > 0) {
        printf("\nEnter index to select, or 0 to cancel: ");
        int choice;
        scanf("%d", &choice);
        while (getchar() != '\n'); // Clear input buffer

        if (choice > 0 && choice <= found_results) {
            FILE *fp = fopen("data/active_entity.txt", "w");
            if (fp != NULL) {
                fprintf(fp, "%s\n", found_tickers[choice - 1]);
                fclose(fp);
                printf("Selected entity '%s' saved.\n", found_tickers[choice - 1]);
            } else {
                printf("Error: Could not open data/active_entity.txt for writing.\n");
            }
        }
    } else {
        printf("No corporations found matching the criteria.\n");
    }
}


void clear_screen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void handle_sigint(int sig) {
    printf("Exiting submenu...\n");
    fflush(stdout);
    exit(0);
}

void save_search_criteria() {
    FILE *fp = fopen("data/db_search.txt", "w");
    if (fp == NULL) {
        printf("Error: Could not open data/db_search.txt for writing.\n");
        fflush(stdout);
        return;
    }
    fprintf(fp, "high_return_on_equity:%d\n", high_return_on_equity);
    fprintf(fp, "low_pe_ratio:%d\n", low_pe_ratio);
    fprintf(fp, "stock_price_less_than_net_worth:%d\n", stock_price_less_than_net_worth);
    fprintf(fp, "dividend_yield:%d\n", dividend_yield);
    fprintf(fp, "min_market_cap:%d\n", min_market_cap);
    fprintf(fp, "max_market_cap:%d\n", max_market_cap);
    fprintf(fp, "earnings_up_next_quarter:%d\n", earnings_up_next_quarter);
    fprintf(fp, "management_rated:%s\n", management_rated);
    fprintf(fp, "credit_rating:%s\n", credit_rating);
    fprintf(fp, "analyst_rating:%s\n", analyst_rating);
    // Save new variables
    fprintf(fp, "industry_supply_demand_improving:%d\n", industry_supply_demand_improving);
    fprintf(fp, "publicly_traded_only:%d\n", publicly_traded_only);
    fprintf(fp, "has_bonds_issued:%d\n", has_bonds_issued);
    fclose(fp);
}

void load_search_criteria() {
    FILE *fp = fopen("data/db_search.txt", "r");
    if (fp == NULL) {
        // If the file doesn't exist, create it with default values
        save_search_criteria();
        return;
    }

    char line[100];
    while (fgets(line, sizeof(line), fp)) {
        char *key = strtok(line, ":");
        char *value = strtok(NULL, "\n");
        if (key != NULL && value != NULL) {
            if (strcmp(key, "high_return_on_equity") == 0) {
                high_return_on_equity = atoi(value);
            } else if (strcmp(key, "low_pe_ratio") == 0) {
                low_pe_ratio = atoi(value);
            } else if (strcmp(key, "stock_price_less_than_net_worth") == 0) {
                stock_price_less_than_net_worth = atoi(value);
            } else if (strcmp(key, "dividend_yield") == 0) {
                dividend_yield = atoi(value);
            } else if (strcmp(key, "min_market_cap") == 0) {
                min_market_cap = atoi(value);
            } else if (strcmp(key, "max_market_cap") == 0) {
                max_market_cap = atoi(value);
            } else if (strcmp(key, "earnings_up_next_quarter") == 0) {
                earnings_up_next_quarter = atoi(value);
            } else if (strcmp(key, "management_rated") == 0) {
                strncpy(management_rated, value, MAX_STRING_LEN - 1);
                management_rated[MAX_STRING_LEN - 1] = '\0';
            } else if (strcmp(key, "credit_rating") == 0) {
                strncpy(credit_rating, value, MAX_STRING_LEN - 1);
                credit_rating[MAX_STRING_LEN - 1] = '\0';
            } else if (strcmp(key, "analyst_rating") == 0) {
                strncpy(analyst_rating, value, MAX_STRING_LEN - 1);
                analyst_rating[MAX_STRING_LEN - 1] = '\0';
            }
        }
    }
    fclose(fp);
}

void calculate_and_save_averages() {
    DIR *d;
    struct dirent *dir;
    char filepath[1024];
    struct Corporation corp;
    int count = 0;

    long total_return_on_equity = 0;
    long total_pe_ratio = 0;
    long total_dividend_yield = 0;
    long total_market_cap = 0;

    // For string fields, we'll count occurrences
    char credit_ratings[100][MAX_STRING_LEN];
    int credit_rating_counts[100] = {0};
    int credit_rating_unique_count = 0;

    d = opendir("corporations/generated");
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) {
                continue;
            }

            char ticker_name[10];
            strncpy(ticker_name, dir->d_name, sizeof(ticker_name) - 1);
            ticker_name[sizeof(ticker_name) - 1] = '\0';

            snprintf(filepath, sizeof(filepath), "corporations/generated/%s/%s.txt", dir->d_name, dir->d_name);
            if (parse_corporation_file(filepath, &corp, ticker_name)) {
                total_return_on_equity += corp.return_on_equity;
                total_pe_ratio += corp.pe_ratio;
                total_dividend_yield += corp.dividend_yield;
                total_market_cap += corp.total_stock_capitalization;

                // Handle credit rating
                int found = 0;
                for (int i = 0; i < credit_rating_unique_count; i++) {
                    if (strcmp(credit_ratings[i], corp.credit_rating) == 0) {
                        credit_rating_counts[i]++;
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    strncpy(credit_ratings[credit_rating_unique_count], corp.credit_rating, MAX_STRING_LEN - 1);
                    credit_ratings[credit_rating_unique_count][MAX_STRING_LEN - 1] = '\0';
                    credit_rating_counts[credit_rating_unique_count] = 1;
                    credit_rating_unique_count++;
                }

                count++;
            }
        }
        closedir(d);
    }

    if (count > 0) {
        high_return_on_equity = total_return_on_equity / count;
        low_pe_ratio = total_pe_ratio / count;
        dividend_yield = total_dividend_yield / count;
        max_market_cap = total_market_cap / count;

        int max_count = 0;
        char best_credit_rating[MAX_STRING_LEN] = "N/A";
        for (int i = 0; i < credit_rating_unique_count; i++) {
            if (credit_rating_counts[i] > max_count) {
                max_count = credit_rating_counts[i];
                strncpy(best_credit_rating, credit_ratings[i], MAX_STRING_LEN - 1);
                best_credit_rating[MAX_STRING_LEN - 1] = '\0';
            }
        }
        strncpy(credit_rating, best_credit_rating, MAX_STRING_LEN - 1);
        credit_rating[MAX_STRING_LEN - 1] = '\0';

        save_search_criteria();
        printf("Default search criteria have been updated based on company averages.\n");
    } else {
        printf("No corporation files found to calculate averages.\n");
    }
}

void display_db_search_menu() {

    int choice;
    do {
        clear_screen();
        printf("\n--- Database Search Menu ---\n");
        printf("1. High Return on Equity (at least %%): [%d%%]\n", high_return_on_equity);
        printf("2. Low P/E Ratio Stocks - P/E Less Than: [%d]\n", low_pe_ratio);
        printf("3. Stock Price Less than %% of Net Worth: [%d%%]\n", stock_price_less_than_net_worth);
        printf("4. Dividend Yield is %% or More: [%d%%]\n", dividend_yield);
        printf("5. Min/Max Market Cap (Millions): [%d/%d]\n", min_market_cap, max_market_cap);
        printf("6. Earnings up Next Quarter by %% or More: [%d%%]\n", earnings_up_next_quarter);
        printf("7. Earning up Last 1,2,3 Years (Not implemented)\n");
        printf("8. Management Rated at least: [%s] [Poor,Good,AVG]\n", management_rated);
        printf("9. Credit Rating is at least: [%s] [AAA,AA,A,BBB,BB,B,CCC,CC,C,D]\n", credit_rating);
        printf("10. Analyst Rating is: [%s] [StrongBuy,Buy,Hold,Sell,StrongSell]\n", analyst_rating);
        // New menu items
        printf("11. Industry Supply/Demand Improving: [%s]\n", industry_supply_demand_improving ? "On" : "Off");
        printf("12. Don't Show if Company's Stock is Not Publicly Traded: [%s]\n", publicly_traded_only ? "On" : "Off");
        printf("13. Has Bonds Issued/Outstanding: [%s]\n", has_bonds_issued ? "On" : "Off");
        printf("14. Display Results\n");
        printf("15. Clear Checkboxes\n");
        printf("16. Help\n");
        printf("17. Back to Main Menu\n");
        printf("Enter your choice: ");
        fflush(stdout);

        scanf("%d", &choice);
        while (getchar() != '\n'); // Clear input buffer

        switch (choice) {
            case 1:
                printf("Enter new value for High Return on Equity: ");
                fflush(stdout);
                scanf("%d", &high_return_on_equity);
                while (getchar() != '\n');
                save_search_criteria();
                break;
            case 2:
                printf("Enter new value for Low P/E Ratio: ");
                fflush(stdout);
                scanf("%d", &low_pe_ratio);
                while (getchar() != '\n');
                save_search_criteria();
                break;
            case 3:
                printf("Enter new value for Stock Price Less than Net Worth %%: ");
                fflush(stdout);
                scanf("%d", &stock_price_less_than_net_worth);
                while (getchar() != '\n');
                save_search_criteria();
                break;
            case 4:
                printf("Enter new value for Dividend Yield %%: ");
                fflush(stdout);
                scanf("%d", &dividend_yield);
                while (getchar() != '\n');
                save_search_criteria();
                break;
            case 5:
                printf("Enter new value for Min Market Cap: ");
                fflush(stdout);
                scanf("%d", &min_market_cap);
                printf("Enter new value for Max Market Cap: ");
                fflush(stdout);
                scanf("%d", &max_market_cap);
                while (getchar() != '\n');
                save_search_criteria();
                break;
            case 6:
                printf("Enter new value for Earnings up Next Quarter %%: ");
                fflush(stdout);
                scanf("%d", &earnings_up_next_quarter);
                while (getchar() != '\n');
                save_search_criteria();
                break;
            case 7:
                printf("Not implemented yet.\n");
                printf("Press Enter to continue...");
                fflush(stdout);
                while (getchar() != '\n');
                break;
            case 8:
                printf("Enter new value for Management Rating [Poor,Good,AVG]: ");
                fflush(stdout);
                scanf("%s", management_rated);
                while (getchar() != '\n');
                save_search_criteria();
                break;
            case 9:
                printf("Enter new value for Credit Rating [AAA,AA,A,BBB,BB,B,CCC,CC,C,D]: ");
                fflush(stdout);
                scanf("%s", credit_rating);
                while (getchar() != '\n');
                save_search_criteria();
                break;
            case 10:
                printf("Enter new value for Analyst Rating [StrongBuy,Buy,Hold,Sell,StrongSell]: ");
                fflush(stdout);
                scanf("%s", analyst_rating);
                while (getchar() != '\n');
                save_search_criteria();
                break;
            case 11:
                printf("Toggle Industry Supply/Demand Improving (0 = Off, 1 = On): ");
                fflush(stdout);
                scanf("%d", &industry_supply_demand_improving);
                industry_supply_demand_improving = (industry_supply_demand_improving != 0); // Normalize to 0 or 1
                while (getchar() != '\n');
                save_search_criteria();
                break;
            case 12:
                printf("Toggle Don't Show if Company's Stock is Not Publicly Traded: [%s]\n", publicly_traded_only ? "On" : "Off");
                fflush(stdout);
                scanf("%d", &publicly_traded_only);
                publicly_traded_only = (publicly_traded_only != 0); // Normalize to 0 or 1
                while (getchar() != '\n');
                save_search_criteria();
                break;
            case 13:
                printf("Toggle Has Bonds Issued/Outstanding (0 = Off, 1 = On): ");
                fflush(stdout);
                scanf("%d", &has_bonds_issued);
                has_bonds_issued = (has_bonds_issued != 0); // Normalize to 0 or 1
                while (getchar() != '\n');
                save_search_criteria();
                break;
            case 14:
                display_search_results();
                printf("Press Enter to continue...");
                fflush(stdout);
                while (getchar() != '\n');
                break;
            case 15:
                printf("Clearing checkboxes...\n");
                printf("Press Enter to continue...");
                fflush(stdout);
                while (getchar() != '\n');
                break;
            case 16:
                calculate_and_save_averages();
                printf("Press Enter to continue...");
                fflush(stdout);
                while (getchar() != '\n');
                break;
            case 17:
                break;
            default:
                printf("Invalid choice. Please try again.\n");
                printf("Press Enter to continue...");
                fflush(stdout);
                while (getchar() != '\n');
                break;
        }
    } while (choice != 17);
}

int main() {
    signal(SIGINT, handle_sigint);
    load_search_criteria();
    display_db_search_menu();
    return 0;
}
