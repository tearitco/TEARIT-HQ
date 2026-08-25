#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>

#define MAX_LINE_LEN 256
#define MAX_FILE_SIZE 1048576 // 1MB buffer for input file

// Structure to hold government data
typedef struct {
    char name[100];
    long long gdp;
    long long population;
} Government;

// Function to create a directory if it doesn't exist
void create_directory(const char *path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        mkdir(path, 0700);
    }
}

// Function to trim leading and trailing whitespace
void trim_whitespace(char *str) {
    int start = 0, end = strlen(str) - 1;
    while (start <= end && isspace((unsigned char)str[start])) start++;
    while (end >= start && isspace((unsigned char)str[end])) end--;
    str[end + 1] = '\0';
    memmove(str, str + start, end - start + 2);
}

// Function to sanitize name for filename
void sanitize_name(char *dest, const char *src) {
    int j = 0;
    for (int i = 0; src[i] && j < 99; i++) {
        if (isalnum((unsigned char)src[i]) || src[i] == ' ') {
            dest[j++] = src[i];
        } else {
            dest[j++] = '_';
        }
    }
    dest[j] = '\0';
    trim_whitespace(dest);
}

// Function to insert commas into a number string
void insert_commas(char *dest, long long num) {
    char buf[64];
    sprintf(buf, "%lld", num);
    int len = strlen(buf);
    int comma_count = (len - 1) / 3;
    int dest_len = len + comma_count;
    dest[dest_len] = '\0';
    int j = dest_len - 1;
    int k = len - 1;
    int count = 0;
    while (k >= 0) {
        dest[j--] = buf[k--];
        if (++count % 3 == 0 && k >= 0) {
            dest[j--] = ',';
        }
    }
}

// Function to generate and save a government's sheet
void generate_gov_sheet(Government gov) {
    char file_path[256];
    char sanitized_name[100];

    sanitize_name(sanitized_name, gov.name);
    sprintf(file_path, "governments/generated/%s.txt", sanitized_name);

    FILE *fp = fopen(file_path, "w");
    if (fp == NULL) {
        perror("Error creating government sheet");
        return;
    }

    fprintf(fp, "GOVERNMENT FINANCIAL DATA: %s\n", gov.name);
    fprintf(fp, "---------------------------------------------\n");
    fprintf(fp, "GDP : %lld\n", gov.gdp );
    fprintf(fp, "Population: %lld\n", gov.population);

    fclose(fp);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    // Process preset_bank.txt to get humanoid_bank values
    FILE *preset_fp = fopen("presets/preset_bank.txt", "r");
    if (preset_fp == NULL) {
        perror("Error opening presets/preset_bank.txt");
        return 1;
    }

    create_directory("multiverse");
    create_directory("multiverse/humanoids");

    FILE *data_bank_fp = fopen("multiverse/humanoids/humanoid_data_bank.txt", "w");
    if (data_bank_fp == NULL) {
        perror("Error creating humanoid_data_bank.txt");
        fclose(preset_fp);
        return 1;
    }

    long long total_population = 0;
    long long total_gdp = 0;
    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), preset_fp)) {
        char pool_name[100];
        long long pool_pop = 0;
        long long cash_per_cap = 0;

        // Parse line: name pop cash_per_cap
        char *token = strtok(line, " ");
        if (token) strncpy(pool_name, token, sizeof(pool_name) - 1);
        token = strtok(NULL, " ");
        if (token) pool_pop = atoll(token);
        token = strtok(NULL, " ");
        if (token) cash_per_cap = atoll(token);

        // Only use humanoid_bank for totals
        if (strcmp(pool_name, "humanoid_bank") == 0) {
            total_population = pool_pop;
            total_gdp = pool_pop * cash_per_cap;
        }

        long long pool_money = pool_pop * cash_per_cap;
        char money_str[64];
        insert_commas(money_str, pool_money);
        fprintf(data_bank_fp, "%s %lld %s.00\n", pool_name, pool_pop, money_str);
    }

    fclose(preset_fp);
    fclose(data_bank_fp);

    // Process the input gov list
    FILE *fp = fopen(argv[1], "r");
    if (fp == NULL) {
        perror("Error opening input file");
        return 1;
    }

    system("rm -f governments/generated/*");
    create_directory("governments/generated");

    FILE *out_fp = fopen("governments/generated/gov-list.txt", "w");
    if (out_fp == NULL) {
        perror("Error creating gov-list.txt");
        fclose(fp);
        return 1;
    }

    fprintf(out_fp, "Country pop gdp\n");

    // Process input file to generate individual government sheets
    while (fgets(line, sizeof(line), fp)) {
        Government gov = {0};
        char rank[100];
        char pop_perc[100];
        char gdp_share_perc[100];

        char *token;
        token = strtok(line, "\t"); // Rank
        if (token != NULL) {
            strncpy(rank, token, sizeof(rank) - 1);
            rank[sizeof(rank) - 1] = '\0';
            trim_whitespace(rank);
        } else {
            continue;
        }

        token = strtok(NULL, "\t"); // Country Name
        if (token != NULL) {
            strncpy(gov.name, token, sizeof(gov.name) - 1);
            gov.name[sizeof(gov.name) - 1] = '\0';
            trim_whitespace(gov.name);
        } else {
            continue;
        }

        token = strtok(NULL, "\t"); // pop%
        if (token != NULL) {
            strncpy(pop_perc, token, sizeof(pop_perc) - 1);
            trim_whitespace(pop_perc);
            int len = strlen(pop_perc);
            if (len > 0 && pop_perc[len - 1] == '%') pop_perc[len - 1] = '\0';
        } else {
            continue;
        }

        token = strtok(NULL, "\t"); // gdp.share%
        if (token != NULL) {
            strncpy(gdp_share_perc, token, sizeof(gdp_share_perc) - 1);
            trim_whitespace(gdp_share_perc);
            int len = strlen(gdp_share_perc);
            if (len > 0 && gdp_share_perc[len - 1] == '%') gdp_share_perc[len - 1] = '\0';
        } else {
            continue;
        }

        // Skip the last gdp 0
        strtok(NULL, "\t");

        double pop_frac = atof(pop_perc) / 100.0;
        double gdp_frac = atof(gdp_share_perc) / 100.0;

        gov.population = (long long)(pop_frac * total_population + 0.5);
        gov.gdp = (long long)(gdp_frac * total_gdp + 0.5);

        if (strlen(gov.name) > 0) {
            generate_gov_sheet(gov);
            fprintf(out_fp, "%s\t%s\t%lld %lld\n", rank, gov.name, gov.population, gov.gdp);
        }
    }

    fclose(fp);
    fclose(out_fp);

    return 0;
}
