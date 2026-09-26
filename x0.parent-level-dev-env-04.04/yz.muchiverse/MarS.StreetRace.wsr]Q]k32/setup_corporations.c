#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <ctype.h>

#define MAX_CORPS 100
#define MAX_LINE_LEN 256

// Structure to hold corporation data
typedef struct {
    char name[100];
    char ticker[20];
    char industry[100];
} Corporation;

// Function to create a directory if it doesn't exist
void create_directory(const char *path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        mkdir(path, 0700);
    }
}

// Function to trim trailing whitespace
void trim_trailing_whitespace(char *str) {
    int i = strlen(str) - 1;
    while (i >= 0 && isspace((unsigned char)str[i])) {
        str[i] = '\0';
        i--;
    }
}

// Function to read corporations from file
int read_corporations(Corporation corporations[]) {
    FILE *fp = fopen("corporations/starting_corporations.txt", "r");
    if (fp == NULL) {
        perror("Error opening starting_corporations.txt");
        return 0;
    }

    int count = 0;
    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), fp) != NULL && count < MAX_CORPS) {
        line[strcspn(line, "\r\n")] = 0; // Remove newline chars

        // Find ticker and industry delimiters
        char *ticker_start = strrchr(line, '(');
        char *ticker_end = strrchr(line, ')');
        char *industry_start = strstr(line, "Ind:");
        if (ticker_start && ticker_end && industry_start && ticker_start < ticker_end && ticker_end < industry_start) {
            *ticker_start = '\0'; // Separate name
            *ticker_end = '\0';   // End ticker
            char *industry_info = industry_start + 4; // Skip "Ind:"
            while (isspace((unsigned char)*industry_info)) industry_info++; // Skip leading spaces

            // Trim trailing whitespace from name
            trim_trailing_whitespace(line);

            // Copy name, ticker, and industry to corporation struct
            strncpy(corporations[count].name, line, sizeof(corporations[count].name) - 1);
            corporations[count].name[sizeof(corporations[count].name) - 1] = '\0';

            strncpy(corporations[count].ticker, ticker_start + 1, sizeof(corporations[count].ticker) - 1);
            corporations[count].ticker[sizeof(corporations[count].ticker) - 1] = '\0';

            strncpy(corporations[count].industry, industry_info, sizeof(corporations[count].industry) - 1);
            corporations[count].industry[sizeof(corporations[count].industry) - 1] = '\0';

            count++;
        }
    }

    fclose(fp);
    return count;
}

// Function to generate a random float between min and max
float rand_float(float min, float max) {
    return min + (float)rand() / ((float)RAND_MAX / (max - min));
}

// Function to determine business type based on industry
int is_bank(const char *industry) {
    return strcasecmp(industry, "BANKING") == 0;
}

int is_insurance(const char *industry) {
    return strcasecmp(industry, "INSURANCE") == 0;
}

// Function to create weights.txt file for corporation
void create_weights_file(Corporation corp) {
    char weights_path[256];
    sprintf(weights_path, "corporations/generated/%s/weights.txt", corp.ticker);
    
    FILE *weights_fp = fopen(weights_path, "w");
    if (weights_fp == NULL) {
        perror("Error creating weights.txt");
        return;
    }
    
    // Generate random risk appetite between 1-100
    int risk_appetite = rand() % 100 + 1;
    
    fprintf(weights_fp, "risk %d\n", risk_appetite);
    fclose(weights_fp);
}

// Function to generate a bank's financial sheet
void generate_bank_sheet(Corporation corp) {
    char dir_path[256];
    sprintf(dir_path, "corporations/generated/%s", corp.ticker);
    create_directory(dir_path);

    char file_path[256];
    sprintf(file_path, "corporations/generated/%s/%s.txt", corp.ticker, corp.ticker);

    FILE *fp = fopen(file_path, "w");
    if (fp == NULL) {
        perror("Error creating bank sheet");
        return;
    }

    // Generate random financial data
    float free_cash = rand_float(100.0, 3000.0);
    float business_loans = rand_float(500.0, 2000.0);
    float consumer_loans = rand_float(100.0, 500.0);
    float mortgage_loans = rand_float(200.0, 1000.0);
    float bad_debt_reserves = rand_float(-300.0, -100.0);
    float total_assets = free_cash + business_loans + consumer_loans + mortgage_loans + bad_debt_reserves;
    float demand_deposits = rand_float(100.0, 500.0);
    float cd = rand_float(500.0, 1500.0);
    float total_liabilities = demand_deposits + cd;
    float net_worth = total_assets - total_liabilities;
    float shares_outstanding = rand_float(10.0, 100.0);
    float net_worth_per_share = net_worth / shares_outstanding;
    float stock_price = net_worth_per_share * rand_float(0.8, 1.5);
    float week_52_high = stock_price * rand_float(1.1, 2.0);
    float week_52_low = stock_price * rand_float(0.5, 0.9);
    float total_cap = stock_price * shares_outstanding;
    float bad_debt_reserve_ratio = (-bad_debt_reserves / (business_loans + consumer_loans + mortgage_loans)) * 100.0;
    float reserves_to_deposits = (-bad_debt_reserves / (demand_deposits + cd)) * 100.0;

    fprintf(fp, "FINANCIAL PROFILE FOR %s (%s)\n", corp.name, corp.ticker);
    fprintf(fp, "================================================\n");
    fprintf(fp, "FINANCIAL BALANCE SHEET: %s (%s)\n", corp.name, corp.ticker);
    fprintf(fp, "--------------------------------------------------\n");
    fprintf(fp, "(Amounts in millions of U.S. Dollars, except per share amounts)\n\n");
    fprintf(fp, "ASSETS:\n");
    fprintf(fp, "  Free Cash and Equivalents:\t\t%.2f\n", free_cash);
    fprintf(fp, "  Stock in Subsidiary Corps.:\t\t0.00\n");
    fprintf(fp, "  Options Long/Short: Net Value:\t0.00\n");
    fprintf(fp, "  Government Bonds (mkt. value):\t0.00\n");
    fprintf(fp, "  Corporate Bonds (mkt. value):\t0.00\n");
    fprintf(fp, "  Business Loan Portfolio:\t\t%.2f\n", business_loans);
    fprintf(fp, "  Consumer Loan Portfolio:\t\t%.2f\n", consumer_loans);
    fprintf(fp, "  Mortgage Loans/Securities:\t\t%.2f\n", mortgage_loans);
    fprintf(fp, "  Less: Bad Debt Reserves:\t\t%.2f\n", bad_debt_reserves);
    fprintf(fp, "  ---------------------------------------------\n");
    fprintf(fp, "  Total Assets:\t\t\t\t%.2f\n", total_assets);
    fprintf(fp, "  ---------------------------------------------\n\n");
    fprintf(fp, "LIABILITIES AND EQUITY:\n");
    fprintf(fp, "  Long-Term Bonds Issued:\t\t0.00\n");
    fprintf(fp, "  Demand Deposits:\t\t\t%.2f\n", demand_deposits);
    fprintf(fp, "  Certificates of Deposit:\t\t%.2f\n", cd);
    fprintf(fp, "  Interbank Debt -- Fed Funds:\t\t0.00\n");
    fprintf(fp, "  Accrued Income Tax Payable:\t\t0.00\n");
    fprintf(fp, "  ---------------------------------------------\n");
    fprintf(fp, "  Total Liabilities:\t\t\t%.2f\n", total_liabilities);
    fprintf(fp, "  ---------------------------------------------\n");
    fprintf(fp, "  Equity (Net Worth):\t\t\t%.2f\n", net_worth);
    fprintf(fp, "  =============================================\n\n");
    fprintf(fp, "STOCK AND EARNINGS DATA:\n");
    fprintf(fp, "------------------------\n");
    fprintf(fp, "  Net Worth Per Share:\t\t\t%.2f\n", net_worth_per_share);
    fprintf(fp, "  Current Stock Price:\t\t\t%.2f\n", stock_price);
    fprintf(fp, "  52-Week High/Low Stock Price:\t\t%.2f / %.2f\n", week_52_high, week_52_low);
    fprintf(fp, "  Shares of Stock Outstanding:\t\t%.2f million\n", shares_outstanding);
    fprintf(fp, "  Total Stock Capitalization:\t\t%.2f million\n", total_cap);
    fprintf(fp, "  2022 Earnings Per Share:\t\t0.00 (Operating Earnings)\n");
    fprintf(fp, "  2023 Earnings Per Share:\t\t0.00 (Operating Earnings)\n");
    fprintf(fp, "  2024 Earnings Per Share:\t\t0.00 (Operating Earnings)\n");
    fprintf(fp, "  2025 Earnings Per Share:\t\t0.00 (Operating Earnings)\n");
    fprintf(fp, "  2026 E/P/S through Qtr. #2:\t\t0.00 (Operating Earnings)\n");
    fprintf(fp, "  ---------------------------------------------\n\n");
    fprintf(fp, "VARIOUS FINANCIAL RATIOS:\n");
    fprintf(fp, "-------------------------\n");
    fprintf(fp, "  Price/Earnings Ratio:\t\t\tNMF (No Meaningful Figure)\n");
    fprintf(fp, "  Dividend Yield:\t\t\t0.00%%\n");
    fprintf(fp, "  Debt to Equity Ratio:\t\t\t0.00 to 1\n");
    fprintf(fp, "  Bad Debt Reserve Ratio:\t\t%.2f%%\n", bad_debt_reserve_ratio);
    fprintf(fp, "  Reserves to Deposits %%:\t\t%.2f%%\n", reserves_to_deposits);
    fprintf(fp, "  Return on Equity (2025):\t\tNMF (No Meaningful Figure)\n");
    fprintf(fp, "  ---------------------------------------------\n\n");
    fprintf(fp, "*** CREDIT INFO: ***\n");
    fprintf(fp, "-------------------\n");
    fprintf(fp, "  Credit Rating:\t\t\tAA\n");
    fprintf(fp, "  Borrowing Rate (Interbank):\t\t9.21%% (LIBOR is: 8.91%%)\n");
    fprintf(fp, "  ---------------------------------------------\n\n");
    fprintf(fp, "VARIABLE SETTINGS (Changeable by controlling player):\n");
    fprintf(fp, "----------------------------------------------------\n");
    fprintf(fp, "  Dividend Payout Per Share:\t\t0.00\n");
    fprintf(fp, "  %% of Net Interest for Advertising:\t1%%\n");
    fprintf(fp, "  ---------------------------------------------\n\n");
    fprintf(fp, "MISCELLANEOUS INFO:\n");
    fprintf(fp, "-------------------\n");
    fprintf(fp, "  Industry Group:\t\t\t%s\n", corp.industry);
    fprintf(fp, "  Incorporated in:\t\t\tUNITED STATES\n");
    fprintf(fp, "  Controlled (and actively managed) by:\ttest_acc\n");
    fprintf(fp, "  Wholly owned by:\t\t\ttest_acc\n");
    fprintf(fp, "  Tax Loss Carryover:\t\t\t$%.2f million\n", rand_float(100.0, 500.0));
    fprintf(fp, "  Annual CEO Salary (approx.):\t\t$%.2f million (Plus possible bonuses)\n", rand_float(0.1, 2.0));
    fprintf(fp, "  Today's (Game) Date:\t\t\tJanuary 1, 2026\n");
    fprintf(fp, "  Next Earnings Report:\t\t\tApril 1, 2026\n");
    fprintf(fp, "  ---------------------------------------------\n\n");
    fprintf(fp, "FOOTNOTE TO FINANCIAL STATEMENT:\n");
    fprintf(fp, "-------------------------------\n");
    fprintf(fp, "[1] The company is not currently engaged in any interest rate swap transactions and has no undisclosed off-balance sheet liabilities.\n");

    fclose(fp);
    
    // Create weights.txt file for corporation
    create_weights_file(corp);
}

// Function to generate an insurance company's financial sheet
void generate_insurance_sheet(Corporation corp) {
    char dir_path[256];
    sprintf(dir_path, "corporations/generated/%s", corp.ticker);
    create_directory(dir_path);

    char file_path[256];
    sprintf(file_path, "corporations/generated/%s/%s.txt", corp.ticker, corp.ticker);

    FILE *fp = fopen(file_path, "w");
    if (fp == NULL) {
        perror("Error creating insurance sheet");
        return;
    }

    // Generate random financial data
    float free_cash = rand_float(1000.0, 3000.0);
    float bank_loan = rand_float(0.0, 500.0);
    float insurance_reserves = rand_float(500.0, 1500.0);
    float total_assets = free_cash;
    float total_liabilities = bank_loan + insurance_reserves;
    float net_worth = total_assets - total_liabilities;
    float shares_outstanding = rand_float(10.0, 100.0);
    float net_worth_per_share = net_worth / shares_outstanding;
    float stock_price = net_worth_per_share * rand_float(0.8, 1.5);
    float week_52_high = stock_price * rand_float(1.1, 2.0);
    float week_52_low = stock_price * rand_float(0.5, 0.9);
    float total_cap = stock_price * shares_outstanding;
    float debt_to_equity = bank_loan / (net_worth > 0 ? net_worth : 1.0);

    fprintf(fp, "FINANCIAL PROFILE FOR %s (%s)\n", corp.name, corp.ticker);
    fprintf(fp, "================================================\n");
    fprintf(fp, "FINANCIAL BALANCE SHEET: %s (%s)\n", corp.name, corp.ticker);
    fprintf(fp, "--------------------------------------------------\n");
    fprintf(fp, "(Amounts in millions of U.S. Dollars, except per share amounts)\n\n");
    fprintf(fp, "ASSETS:\n");
    fprintf(fp, "  Free Cash and Equivalents:\t\t%.2f\n", free_cash);
    fprintf(fp, "  Stock in Subsidiary Corps.:\t\t0.00\n");
    fprintf(fp, "  Options Long/Short: Net Value:\t0.00\n");
    fprintf(fp, "  Government Bonds (mkt. value):\t0.00\n");
    fprintf(fp, "  Corporate Bonds (mkt. value):\t0.00\n");
    fprintf(fp, "  Subprime Mortgage Securities:\t\t0.00\n");
    fprintf(fp, "  Index Futures: Marked to Mkt.:\t0.00\n");
    fprintf(fp, "  Commodity A/C Margin Balance:\t0.00\n");
    fprintf(fp, "  ---------------------------------------------\n");
    fprintf(fp, "  Total Assets:\t\t\t\t%.2f\n", total_assets);
    fprintf(fp, "  ---------------------------------------------\n\n");
    fprintf(fp, "LIABILITIES AND EQUITY:\n");
    fprintf(fp, "  Long-Term Bonds Issued:\t\t0.00\n");
    fprintf(fp, "  Bank Loan:\t\t\t\t%.2f\n", bank_loan);
    fprintf(fp, "  Insurance Policy Reserves:\t\t%.2f\n", insurance_reserves);
    fprintf(fp, "  Accrued Income Tax Payable:\t\t0.00\n");
    fprintf(fp, "  ---------------------------------------------\n");
    fprintf(fp, "  Total Liabilities:\t\t\t%.2f\n", total_liabilities);
    fprintf(fp, "  ---------------------------------------------\n");
    fprintf(fp, "  Equity (Net Worth):\t\t\t%.2f\n", net_worth);
    fprintf(fp, "  =============================================\n\n");
    fprintf(fp, "STOCK AND EARNINGS DATA:\n");
    fprintf(fp, "------------------------\n");
    fprintf(fp, "  Net Worth Per Share:\t\t\t%.2f\n", net_worth_per_share);
    fprintf(fp, "  Current Stock Price:\t\t\t%.2f\n", stock_price);
    fprintf(fp, "  52-Week High/Low Stock Price:\t\t%.2f / %.2f\n", week_52_high, week_52_low);
    fprintf(fp, "  Shares of Stock Outstanding:\t\t%.2f million\n", shares_outstanding);
    fprintf(fp, "  Total Stock Capitalization:\t\t%.2f million\n", total_cap);
    fprintf(fp, "  2021 Earnings Per Share:\t\t0.00 (Operating Earnings)\n");
    fprintf(fp, "  2022 Earnings Per Share:\t\t0.00 (Operating Earnings)\n");
    fprintf(fp, "  2023 Earnings Per Share:\t\t0.00 (Operating Earnings)\n");
    fprintf(fp, "  2024 Earnings Per Share:\t\t0.00 (Operating Earnings)\n");
    fprintf(fp, "  2025 E/P/S through Qtr. #1:\t\t(Not Yet Announced)\n");
    fprintf(fp, "  ---------------------------------------------\n\n");
    fprintf(fp, "VARIOUS FINANCIAL RATIOS:\n");
    fprintf(fp, "-------------------------\n");
    fprintf(fp, "  Price/Earnings Ratio:\t\t\tNMF (No Meaningful Figure)\n");
    fprintf(fp, "  Dividend Yield:\t\t\t0.00%%\n");
    fprintf(fp, "  Debt to Equity Ratio:\t\t\t%.2f to 1\n", debt_to_equity);
    fprintf(fp, "  Return on Equity (2024):\t\tNMF (No Meaningful Figure)\n");
    fprintf(fp, "  ---------------------------------------------\n\n");
    fprintf(fp, "*** CREDIT INFO: ***\n");
    fprintf(fp, "-------------------\n");
    fprintf(fp, "  Credit Rating:\t\t\tA\n");
    fprintf(fp, "  Unused Bank Line of Credit:\t\t%.2f million\n", rand_float(0.0, 2000.0));
    fprintf(fp, "  Borrowing Rate (Interest):\t\t11.00%% (Prime is: 10.00%%)\n");
    fprintf(fp, "  Lending Bank:\t\t\t\tShould_setup?\n");
    fprintf(fp, "  ---------------------------------------------\n\n");
    fprintf(fp, "VARIABLE SETTINGS (Changeable by controlling player):\n");
    fprintf(fp, "----------------------------------------------------\n");
    fprintf(fp, "  Dividend Payout Per Share:\t\t0.00\n");
    fprintf(fp, "  %% of Net Premiums for Advertising:\t7%%\n");
    fprintf(fp, "  Growth Rate (Insurance in Force):\t6%%\n");
    fprintf(fp, "  ---------------------------------------------\n\n");
    fprintf(fp, "MISCELLANEOUS INFO:\n");
    fprintf(fp, "-------------------\n");
    fprintf(fp, "  Industry Group:\t\t\t%s\n", corp.industry);
    fprintf(fp, "  Incorporated in:\t\t\tMars?\n");
    fprintf(fp, "  Controlled (and actively managed) by:\tsol supreme\n");
    fprintf(fp, "  Public Owns:\t\t\t%%\n");
    fprintf(fp, "  Annual CEO Salary (approx.):\t\t$%.2f million (Plus possible bonuses)\n", rand_float(0.1, 2.0));
    fprintf(fp, "  Today's (Game) Date:\t\t\tJanuary 1, 2025\n");
    fprintf(fp, "  Next Earnings Report:\t\t\tJanuary 8, 2025\n");
    fprintf(fp, "  ---------------------------------------------\n\n");
    fprintf(fp, "FOOTNOTE TO FINANCIAL STATEMENT:\n");
    fprintf(fp, "-------------------------------\n");
    fprintf(fp, "[1] The company is not currently engaged in any interest rate swap transactions and has no undisclosed off-balance sheet liabilities.\n");

    fclose(fp);
    
    // Create weights.txt file for corporation
    create_weights_file(corp);
}

// Function to generate a corporation's financial sheet
void generate_corporation_sheet(Corporation corp) {
    char dir_path[256];
    sprintf(dir_path, "corporations/generated/%s", corp.ticker);
    create_directory(dir_path);

    char file_path[256];
    sprintf(file_path, "corporations/generated/%s/%s.txt", corp.ticker, corp.ticker);

    FILE *fp = fopen(file_path, "w");
    if (fp == NULL) {
        perror("Error creating corporation sheet");
        return;
    }

    // Generate random financial data
    float free_cash = rand_float(100.0, 1000.0);
    float working_capital = rand_float(50.0, 500.0);
    float business_assets = rand_float(500.0, 2000.0);
    float total_assets = free_cash + working_capital + business_assets;
    float bank_loan = rand_float(0.0, 500.0);
    float total_liabilities = bank_loan;
    float net_worth = total_assets - total_liabilities;
    float shares_outstanding = rand_float(10.0, 100.0);
    float net_worth_per_share = net_worth / shares_outstanding;
    float stock_price = net_worth_per_share * rand_float(0.8, 1.5);
    float week_52_high = stock_price * rand_float(1.1, 2.0);
    float week_52_low = stock_price * rand_float(0.5, 0.9);
    float total_cap = stock_price * shares_outstanding;
    float debt_to_equity = bank_loan / (net_worth > 0 ? net_worth : 1.0);

    fprintf(fp, "FINANCIAL PROFILE FOR %s (%s)\n", corp.name, corp.ticker);
    fprintf(fp, "================================================\n");
    fprintf(fp, "FINANCIAL BALANCE SHEET: %s (%s)\n", corp.name, corp.ticker);
    fprintf(fp, "--------------------------------------------------\n");
    fprintf(fp, "(Amounts in millions of U.S. Dollars, except per share amounts)\n\n");
    fprintf(fp, "ASSETS:\n");
    fprintf(fp, "  Free Cash and Equivalents:\t\t%.2f\n", free_cash);
    fprintf(fp, "  Working Capital (A/R, Inven.):\t%.2f\n", working_capital);
    fprintf(fp, "  Business Assets/Equipment:\t\t%.2f\n", business_assets);
    fprintf(fp, "  Stock in Subsidiary Cos.:\t\t0.00\n");
    fprintf(fp, "  Options Long/Short: Net Value:\t0.00\n");
    fprintf(fp, "  ---------------------------------------------\n");
    fprintf(fp, "  Total Assets:\t\t\t\t%.2f\n", total_assets);
    fprintf(fp, "  ---------------------------------------------\n\n");
    fprintf(fp, "LIABILITIES AND EQUITY:\n");
    fprintf(fp, "  Long-Term Bonds Issued:\t\t0.00\n");
    fprintf(fp, "  Bank Loan:\t\t\t\t%.2f\n", bank_loan);
    fprintf(fp, "  Accrued Income Tax Payable:\t\t0.00\n");
    fprintf(fp, "  ---------------------------------------------\n");
    fprintf(fp, "  Total Liabilities:\t\t\t%.2f\n", total_liabilities);
    fprintf(fp, "  ---------------------------------------------\n");
    fprintf(fp, "  Equity (Net Worth):\t\t\t%.2f\n", net_worth);
    fprintf(fp, "  =============================================\n\n");
    fprintf(fp, "STOCK AND EARNINGS DATA:\n");
    fprintf(fp, "------------------------\n");
    fprintf(fp, "  Net Worth Per Share:\t\t\t%.2f\n", net_worth_per_share);
    fprintf(fp, "  Current Stock Price:\t\t\t%.2f\n", stock_price);
    fprintf(fp, "  52-Week High/Low Stock Price:\t\t%.2f / %.2f\n", week_52_high, week_52_low);
    fprintf(fp, "  Shares of Stock Outstanding:\t\t%.2f million\n", shares_outstanding);
    fprintf(fp, "  Total Stock Capitalization:\t\t%.2f million\n", total_cap);
    fprintf(fp, "  2021 Earnings Per Share:\t\t0.00 (Operating Earnings)\n");
    fprintf(fp, "  2022 Earnings Per Share:\t\t0.00 (Operating Earnings)\n");
    fprintf(fp, "  2023 Earnings Per Share:\t\t0.00 (Operating Earnings)\n");
    fprintf(fp, "  2024 Earnings Per Share:\t\t0.00 (Operating Earnings)\n");
    fprintf(fp, "  2025 E/P/S through Qtr. #1:\t\t(Not Yet Announced)\n");
    fprintf(fp, "  ---------------------------------------------\n\n");
    fprintf(fp, "VARIOUS FINANCIAL RATIOS:\n");
    fprintf(fp, "-------------------------\n");
    fprintf(fp, "  Price/Earnings Ratio:\t\t\tNMF (No Meaningful Figure)\n");
    fprintf(fp, "  Dividend Yield:\t\t\t0.00%%\n");
    fprintf(fp, "  Debt to Equity Ratio:\t\t\t%.2f to 1\n", debt_to_equity);
    fprintf(fp, "  Return on Equity (2024):\t\tNMF (No Meaningful Figure)\n");
    fprintf(fp, "  ---------------------------------------------\n\n");
    fprintf(fp, "*** CREDIT INFO: ***\n");
    fprintf(fp, "-------------------\n");
    fprintf(fp, "  Credit Rating:\t\t\tAA\n");
    fprintf(fp, "  Unused Bank Line of Credit:\t\t%.2f million\n", rand_float(0.0, 3000.0));
    fprintf(fp, "  Borrowing Rate (Interest):\t\t10.50%% (Prime is: 10.00%%)\n");
    fprintf(fp, "  Lending Bank:\t\t\t\tFIRST HORIZON NATIONAL (FHN)\n");
    fprintf(fp, "  ---------------------------------------------\n\n");
    fprintf(fp, "VARIABLE SETTINGS (Changeable by controlling player):\n");
    fprintf(fp, "----------------------------------------------------\n");
    fprintf(fp, "  Dividend Payout Per Share:\t\t0.00\n");
    fprintf(fp, "  Percent of Sales Spent on R & D:\t4%%\n");
    fprintf(fp, "  Growth Rate (Assets, Sales):\t\t0%%\n");
    fprintf(fp, "  ---------------------------------------------\n\n");
    fprintf(fp, "MISCELLANEOUS INFO:\n");
    fprintf(fp, "-------------------\n");
    fprintf(fp, "  Industry Group:\t\t\t%s\n", corp.industry);
    fprintf(fp, "  Incorporated in:\t\t\tUNITED STATES\n");
    fprintf(fp, "  Controlled (and actively managed) by:\tpublic\n");
    fprintf(fp, "  Public Owns:\t\t\t%%\n");
    fprintf(fp, "  Accrued R&D and/or Investment Tax Credits:\t$%.2f million\n", rand_float(50.0, 200.0));
    fprintf(fp, "  Annual CEO Salary (approx.):\t\t$%.2f million (Plus possible bonuses)\n", rand_float(0.1, 2.0));
    fprintf(fp, "  Today's (Game) Date:\t\t\tJanuary 1, 2025\n");
    fprintf(fp, "  Next Earnings Report:\t\t\tJanuary 27, 2025\n");
    fprintf(fp, "  ---------------------------------------------\n\n");
    fprintf(fp, "FOOTNOTE TO FINANCIAL STATEMENT:\n");
    fprintf(fp, "-------------------------------\n");
    fprintf(fp, "[1] The company is not currently engaged in any interest rate swap transactions and has no undisclosed off-balance sheet liabilities.\n");

    fclose(fp);
    
    // Create weights.txt file for corporation
    create_weights_file(corp);
}

int main() {
    srand(time(NULL));
    system("rm -rf corporations/generated/*");
    create_directory("corporations/generated");

    Corporation corporations[MAX_CORPS] = {0};
    int num_corps = read_corporations(corporations);

    if (num_corps > 0) {
        for (int i = 0; i < num_corps; i++) {
            if (is_bank(corporations[i].industry)) {
                generate_bank_sheet(corporations[i]);
            } else if (is_insurance(corporations[i].industry)) {
                generate_insurance_sheet(corporations[i]);
            } else {
                generate_corporation_sheet(corporations[i]);
            }
        }
        printf("%d corporation sheets generated successfully.\n", num_corps);
        
        // Run analysis loop once to set initial stock prices based on fundamentals
        printf("Running analysis to set initial stock prices...\n");
        int result = system("./analysis_loop");
        if (result == 0) {
            printf("Analysis completed successfully. Initial prices set based on fundamentals.\n");
        } else {
            printf("Analysis failed. Initial prices may not be properly calculated.\n");
        }
    } else {
        printf("No corporations found or error reading file.\n");
    }

    return 0;
}
