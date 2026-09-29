#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <math.h>
#include <time.h>

#define MAX_LINE_LEN 512
#define MAX_CORP_NAME 100

// Structure to hold corporation financial data
typedef struct {
    char name[MAX_CORP_NAME];
    char ticker[20];
    float total_assets;
    float total_liabilities;
    float book_value;  // assets - liabilities
    float debt_to_equity;  // leverage
    float shares_outstanding;
    float current_stock_price;
    float market_cap;
    int risk_bias;  // from weights.txt (1-100)
} CorpData;

// Function to trim leading/trailing whitespace
char *trim(char *str) {
    char *end;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

// Function to extract a float value from a line that matches a pattern
float extract_float_value(const char* line, const char* pattern) {
    if (strstr(line, pattern)) {
        const char* start = strstr(line, pattern) + strlen(pattern);
        // Skip any colon or equals sign and whitespace
        while (*start && (*start == ':' || *start == '=' || isspace(*start))) start++;
        return atof(start);
    }
    return 0.0;
}

// Function to extract a string value from a line that matches a pattern
void extract_string_value(const char* line, const char* pattern, char* output, size_t output_size) {
    if (output_size == 0) return;  // Handle edge case
    
    if (strstr(line, pattern)) {
        const char* start = strstr(line, pattern) + strlen(pattern);
        // Skip any colon or equals sign and whitespace
        while (*start && (*start == ':' || *start == '=' || isspace(*start))) start++;
        // Copy until end of line or newline, ensuring we don't exceed buffer size
        size_t i = 0;
        size_t max_len = output_size - 1; // Leave space for null terminator
        while (i < max_len && *start && *start != '\n' && *start != '\r') {
            output[i] = *start;
            i++;
            start++;
        }
        output[i] = '\0';
    }
}

// Function to read risk bias from weights.txt
int read_risk_bias(const char* ticker) {
    char weights_path[256];
    sprintf(weights_path, "corporations/generated/%s/weights.txt", ticker);
    
    FILE *weights_fp = fopen(weights_path, "r");
    if (weights_fp == NULL) {
        // Default to 50 if weights.txt doesn't exist
        return 50;
    }
    
    char line[MAX_LINE_LEN];
    int risk = 50; // Default value
    
    if (fgets(line, sizeof(line), weights_fp) != NULL) {
        char* token = strtok(line, " \t");
        if (token && strcmp(token, "risk") == 0) {
            token = strtok(NULL, " \t\n");
            if (token) {
                risk = atoi(token);
                if (risk < 1) risk = 1;
                if (risk > 100) risk = 100;
            }
        }
    }
    
    fclose(weights_fp);
    return risk;
}

// Function to calculate new stock price based on fundamentals and bias
float calculate_new_stock_price(CorpData *corp) {
    // Base factors:
    // 1. Book value per share (book_value / shares_outstanding)
    float book_value_per_share = (corp->shares_outstanding > 0) ? 
                                 corp->book_value / corp->shares_outstanding : 0.0;
    
    // 2. Market cap influence (adjust based on current market conditions)
    float market_cap_multiplier = 1.0;
    if (corp->market_cap > 0) {
        // Normalize market cap effects - larger companies may have more stable prices
        market_cap_multiplier = 1.0 + (log10(corp->market_cap) - 3) * 0.05;
    }
    
    // 3. Leverage factor - high debt/leverage should reduce stock price
    // debt_to_equity > 1.0 is generally considered high risk
    float leverage_factor = 1.0;
    if (corp->debt_to_equity > 1.0) {
        leverage_factor = 1.0 - (corp->debt_to_equity - 1.0) * 0.1; // Reduce price for high leverage
    } else {
        // For companies with healthy leverage, slightly increase price
        leverage_factor = 1.0 + (0.5 - corp->debt_to_equity) * 0.05;
    }
    
    // 4. Bank bias factor (1-100 scale normalized to 0.5-1.5 range)
    float bias_factor = 0.5 + (corp->risk_bias / 100.0) * 1.0;  // Converts 1-100 to 0.5-1.5
    
    // 5. Fundamental value calculation
    float fundamental_value = book_value_per_share * market_cap_multiplier * leverage_factor * bias_factor;
    
    // 6. Current stock price influence - keep some momentum
    float momentum_factor = 0.7;  // 70% from fundamentals, 30% from current price
    float new_price = (fundamental_value * momentum_factor) + 
                     (corp->current_stock_price * (1.0 - momentum_factor));
    
    // Ensure the price doesn't become negative or too small
    if (new_price < 0.1) new_price = 0.1;
    
    return new_price;
}

// Function to update the stock price in the corporation's file
void update_stock_price_in_file(const char* ticker, float new_price, float old_price) {
    char file_path[256];
    char temp_path[256];
    sprintf(file_path, "corporations/generated/%s/%s.txt", ticker, ticker);
    sprintf(temp_path, "corporations/generated/%s/%s_temp.txt", ticker, ticker);
    
    FILE *fp = fopen(file_path, "r");
    if (fp == NULL) return;
    
    FILE *temp_fp = fopen(temp_path, "w");
    if (temp_fp == NULL) {
        fclose(fp);
        return;
    }
    
    char line[MAX_LINE_LEN];
    int found_stock_price = 0;
    
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strstr(line, "Current Stock Price:")) {
            // Replace with the new stock price
            fprintf(temp_fp, "  Current Stock Price:\t\t\t%.2f\n", new_price);
            found_stock_price = 1;
        } else {
            fprintf(temp_fp, "%s", line);
        }
    }
    
    fclose(fp);
    fclose(temp_fp);
    
    // Replace the old file with the new file
    remove(file_path);
    rename(temp_path, file_path);
    
    // Create or update price history
    char hist_path[256];
    sprintf(hist_path, "corporations/generated/%s/price_history.txt", ticker);
    
    FILE *hist_fp = fopen(hist_path, "a");
    if (hist_fp != NULL) {
        float change_percent = ((new_price - old_price) / old_price) * 100.0;
        time_t t = time(NULL);
        struct tm *tm_info = localtime(&t);
        fprintf(hist_fp, "%d-%02d-%02d,%.2f,%.2f,%.2f%%\n", 
                tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
                old_price, new_price, change_percent);
        fclose(hist_fp);
    }
}

// Function to analyze a single corporation file
int analyze_corporation(const char* ticker) {
    char file_path[256];
    sprintf(file_path, "corporations/generated/%s/%s.txt", ticker, ticker);
    
    FILE *fp = fopen(file_path, "r");
    if (fp == NULL) {
        return 0; // File doesn't exist
    }
    
    CorpData corp;
    strcpy(corp.ticker, ticker);
    strcpy(corp.name, ticker);  // Default name to ticker
    
    // Initialize all values to 0
    corp.total_assets = 0.0;
    corp.total_liabilities = 0.0;
    corp.book_value = 0.0;
    corp.debt_to_equity = 0.0;
    corp.shares_outstanding = 0.0;
    corp.current_stock_price = 0.0;
    corp.market_cap = 0.0;
    corp.risk_bias = read_risk_bias(ticker);  // Read from weights.txt
    
    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), fp) != NULL) {
        // Extract total assets
        if (strstr(line, "Total Assets:")) {
            corp.total_assets = extract_float_value(line, "Total Assets:");
        }
        // Extract total liabilities
        else if (strstr(line, "Total Liabilities:")) {
            corp.total_liabilities = extract_float_value(line, "Total Liabilities:");
        }
        // Extract debt to equity ratio
        else if (strstr(line, "Debt to Equity Ratio:")) {
            corp.debt_to_equity = extract_float_value(line, "Debt to Equity Ratio:");
        }
        // Extract shares outstanding (remove " million")
        else if (strstr(line, "Shares of Stock Outstanding:")) {
            char temp_str[100];
            extract_string_value(line, "Shares of Stock Outstanding:", temp_str, sizeof(temp_str));
            // Remove " million" from the string if present
            char* pos = strstr(temp_str, " million");
            if (pos) *pos = '\0';
            corp.shares_outstanding = atof(temp_str);
        }
        // Extract current stock price
        else if (strstr(line, "Current Stock Price:")) {
            corp.current_stock_price = extract_float_value(line, "Current Stock Price:");
        }
        // Extract total stock capitalization (market cap)
        else if (strstr(line, "Total Stock Capitalization:")) {
            corp.market_cap = extract_float_value(line, "Total Stock Capitalization:");
        }
        // Extract corporation name
        else if (strstr(line, "FINANCIAL PROFILE FOR")) {
            char *temp_str = malloc(512);  // Allocate memory for temporary string
            if(temp_str != NULL) {
                extract_string_value(line, "FINANCIAL PROFILE FOR", temp_str, 512);
                // Extract only the name part before the ticker in parentheses
                char* paren_pos = strchr(temp_str, '(');
                if (paren_pos) {
                    *paren_pos = '\0';
                    trim(temp_str);
                }
                strncpy(corp.name, temp_str, sizeof(corp.name) - 1);
                corp.name[sizeof(corp.name) - 1] = '\0';
                free(temp_str);  // Free allocated memory
            }
        }
    }
    
    fclose(fp);
    
    // Calculate book value (assets - liabilities)
    corp.book_value = corp.total_assets - corp.total_liabilities;
    
    // Calculate market cap if we don't have it from the file
    if (corp.market_cap <= 0 && corp.current_stock_price > 0 && corp.shares_outstanding > 0) {
        corp.market_cap = corp.current_stock_price * corp.shares_outstanding;
    }
    
    // Calculate new stock price based on fundamentals and bias
    float new_stock_price = calculate_new_stock_price(&corp);
    
    // Update the stock price in the file
    update_stock_price_in_file(ticker, new_stock_price, corp.current_stock_price);
    
    // Removed verbose output to prevent spam in GUI
    // printf("Analyzed %s (%s): Old price: %.2f, New price: %.2f, Risk bias: %d\n", 
    //        corp.name, ticker, corp.current_stock_price, new_stock_price, corp.risk_bias);
    
    return 1; // Success
}

// Function to analyze all corporations
void analyze_all_corporations() {
    DIR *dir;
    struct dirent *entry;
    
    dir = opendir("corporations/generated");
    if (dir == NULL) {
        perror("Could not open corporations/generated directory");
        return;
    }
    
    while ((entry = readdir(dir)) != NULL) {
        // Check if this is a directory (corporation)
        char path[512];
        sprintf(path, "corporations/generated/%s", entry->d_name);
        
        // Skip . and .. entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // Check if it's a directory
        DIR* sub_dir = opendir(path);
        if (sub_dir != NULL) {
            closedir(sub_dir);  // It's a directory
            // Analyze this corporation
            analyze_corporation(entry->d_name);
        }
    }
    
    closedir(dir);
}

int main() {
    // Removed verbose output to prevent spam in GUI
    // printf("Starting analysis loop...\n");
    
    // Analyze all corporations
    analyze_all_corporations();
    
    // Removed verbose output to prevent spam in GUI
    // printf("Analysis loop completed.\n");
    
    return 0;
}