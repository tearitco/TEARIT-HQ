#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <math.h>
#include <time.h>

#define MAX_LINE_LEN 512
#define MAX_CORP_NAME 100

// Structure to hold corporation info for news
typedef struct {
    char name[MAX_CORP_NAME];
    char ticker[20];
    float current_stock_price;
    float previous_price;
    float change_percent;
} CorpNews;

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

// Function to get the previous price from price_history.txt
float get_previous_price(const char* ticker) {
    char hist_path[256];
    sprintf(hist_path, "corporations/generated/%s/price_history.txt", ticker);
    
    FILE *hist_fp = fopen(hist_path, "r");
    if (hist_fp == NULL) {
        return 0.0; // No history available
    }
    
    // Read the last line to get the previous price
    char line[MAX_LINE_LEN];
    float last_price = 0.0;
    
    // Read through all lines to find the last one
    while (fgets(line, sizeof(line), hist_fp) != NULL) {
        // Parse the line: date,old_price,new_price,change_percent
        char date_str[20];
        float old_price, new_price, change_pct;
        if (sscanf(line, "%[^,],%f,%f,%f%%", date_str, &old_price, &new_price, &change_pct) == 4) {
            last_price = new_price; // The new_price from the last entry becomes the "previous" for next calc
        }
    }
    
    fclose(hist_fp);
    return last_price;
}

// Function to collect corporation data for news
void collect_corp_news(CorpNews *news, int *count) {
    DIR *dir;
    struct dirent *entry;
    
    *count = 0;
    
    dir = opendir("corporations/generated");
    if (dir == NULL) {
        perror("Could not open corporations/generated directory");
        return;
    }
    
    while ((entry = readdir(dir)) != NULL && *count < 100) {  // Limit to 100 corporations
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
            
            // Read the corporation's file to get current stock price
            char file_path[1024];
            snprintf(file_path, sizeof(file_path), "corporations/generated/%s/%s.txt", entry->d_name, entry->d_name);
            
            FILE *fp = fopen(file_path, "r");
            if (fp != NULL) {
                char line[MAX_LINE_LEN];
                float current_price = 0.0;
                char corp_name[MAX_CORP_NAME] = {0};
                
                while (fgets(line, sizeof(line), fp) != NULL) {
                    // Extract current stock price
                    if (strstr(line, "Current Stock Price:")) {
                        current_price = extract_float_value(line, "Current Stock Price:");
                    }
                    // Extract corporation name
                    else if (strstr(line, "FINANCIAL PROFILE FOR")) {
                        char temp_str[200];
                        extract_string_value(line, "FINANCIAL PROFILE FOR", temp_str, sizeof(temp_str));
                        // Extract only the name part before the ticker in parentheses
                        char* paren_pos = strchr(temp_str, '(');
                        if (paren_pos) {
                            *paren_pos = '\0';
                            trim(temp_str);
                        }
                        strncpy(corp_name, temp_str, sizeof(corp_name) - 1);
                        corp_name[sizeof(corp_name) - 1] = '\0';
                    }
                }
                
                fclose(fp);
                
                // Get previous price from history
                float previous_price = get_previous_price(entry->d_name);
                
                // Calculate change percentage
                float change_percent = 0.0;
                if (previous_price > 0) {
                    change_percent = ((current_price - previous_price) / previous_price) * 100.0;
                }
                
                // Add to news array
                strncpy(news[*count].ticker, entry->d_name, sizeof(news[*count].ticker) - 1);
                news[*count].ticker[sizeof(news[*count].ticker) - 1] = '\0';
                
                if (strlen(corp_name) > 0) {
                    strncpy(news[*count].name, corp_name, sizeof(news[*count].name) - 1);
                    news[*count].name[sizeof(news[*count].name) - 1] = '\0';
                } else {
                    strncpy(news[*count].name, entry->d_name, sizeof(news[*count].name) - 1);
                    news[*count].name[sizeof(news[*count].name) - 1] = '\0';
                }
                
                news[*count].current_stock_price = current_price;
                news[*count].previous_price = previous_price;
                news[*count].change_percent = change_percent;
                
                (*count)++;
            }
        }
    }
    
    closedir(dir);
}

// Function to write news to data/news.txt
void write_news_to_file(CorpNews *news, int count) {
    FILE *fp = fopen("data/news.txt", "w");
    if (fp == NULL) {
        perror("Could not open data/news.txt for writing");
        return;
    }
    
    fprintf(fp, "FINANCIAL NEWS HEADLINES\n");
    fprintf(fp, "========================\n");
    
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    fprintf(fp, "Market Update: %04d-%02d-%02d %02d:%02d:%02d\n", 
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, 
            tm.tm_hour, tm.tm_min, tm.tm_sec);
    fprintf(fp, "------------------------\n");
    
    // Sort by percentage change (largest changes first, both positive and negative)
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - 1 - i; j++) {
            if (fabs(news[j].change_percent) < fabs(news[j+1].change_percent)) {
                // Swap
                CorpNews temp = news[j];
                news[j] = news[j+1];
                news[j+1] = temp;
            }
        }
    }
    
    // Write top movements to news file
    for (int i = 0; i < count && i < 20; i++) {  // Limit to top 20 movers
        if (news[i].change_percent != 0.0 || news[i].previous_price > 0) {  // Only show if there was a change
            char sign = (news[i].change_percent >= 0) ? '+' : '-';
            fprintf(fp, "%s (%s): $%.2f (%c%.2f%%)\n", 
                    news[i].name, news[i].ticker, 
                    news[i].current_stock_price, sign, 
                    (sign == '+') ? news[i].change_percent : -news[i].change_percent);
        }
    }
    
    fprintf(fp, "------------------------\n");
    
    fclose(fp);
}

int main() {
    printf("Generating financial news...\n");
    
    CorpNews news[100];  // Support up to 100 corporations
    int count = 0;
    
    // Collect corporation data
    collect_corp_news(news, &count);
    
    // Write news to file
    write_news_to_file(news, count);
    
    printf("Financial news generated for %d corporations.\n", count);
    
    return 0;
}