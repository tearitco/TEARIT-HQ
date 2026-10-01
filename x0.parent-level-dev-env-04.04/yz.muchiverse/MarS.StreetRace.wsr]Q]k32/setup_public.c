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

// Function to trim trailing whitespace
void trim_trailing_whitespace(char *str) {
    int i = strlen(str) - 1;
    while (i >= 0 && isspace((unsigned char)str[i])) {
        str[i] = '\0';
        i--;
    }
}

// Function to generate and save a government's sheet
void generate_gov_sheet(Government gov) {
    char file_path[256];
    char sanitized_name[100];

    // Sanitize the country name for the filename
    int j = 0;
    for (int i = 0; gov.name[i]; i++) {
        if (isalnum((unsigned char)gov.name[i])) {
            sanitized_name[j++] = gov.name[i];
        } else {
            sanitized_name[j++] = '_';
        }
    }
    sanitized_name[j] = '\0';

    sprintf(file_path, "governments/generated/%s.txt", sanitized_name);

    FILE *fp = fopen(file_path, "w");
    if (fp == NULL) {
        perror("Error creating government sheet");
        return;
    }

    fprintf(fp, "GOVERNMENT FINANCIAL DATA: %s\n", gov.name);
    fprintf(fp, "---------------------------------------------\n");
    fprintf(fp, "GDP (in millions): %lld\n", gov.gdp / 1000000);
    fprintf(fp, "Population: %lld\n", gov.population);

    fclose(fp);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    // Open input file to read contents
    FILE *fp = fopen(argv[1], "r");
    if (fp == NULL) {
        perror("Error opening input file");
        return 1;
    }

    // Read and store entire input file content
    char *file_content = malloc(MAX_FILE_SIZE);
    if (file_content == NULL) {
        perror("Memory allocation failed");
        fclose(fp);
        return 1;
    }
    size_t content_size = 0;
    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), fp) && content_size < MAX_FILE_SIZE - MAX_LINE_LEN) {
        strncpy(file_content + content_size, line, MAX_LINE_LEN);
        content_size += strlen(line);
    }

    // Reset file pointer to beginning for processing
    rewind(fp);

    system("rm -f governments/generated/*");
    create_directory("governments/generated");

    // Process input file to generate individual government sheets
    while (fgets(line, sizeof(line), fp)) {
        Government gov = {0};
        char gdp_str[100];
        char pop_str[100];

        char *token;
        token = strtok(line, "\t"); // Rank
        token = strtok(NULL, "\t"); // Country Name
        if (token != NULL) {
            strncpy(gov.name, token, sizeof(gov.name) - 1);
            gov.name[sizeof(gov.name) - 1] = '\0';
        }

        token = strtok(NULL, "\t"); // GDP
        if (token != NULL) {
            int j = 0;
            for (int i = 0; token[i]; i++) {
                if (isdigit((unsigned char)token[i])) {
                    gdp_str[j++] = token[i];
                }
            }
            gdp_str[j] = '\0';
            gov.gdp = atoll(gdp_str);
        }

        token = strtok(NULL, "\t"); // GDP (short)
        token = strtok(NULL, "\t"); // Growth
        token = strtok(NULL, "\t"); // Population
        if (token != NULL) {
            int j = 0;
            for (int i = 0; token[i]; i++) {
                if (isdigit((unsigned char)token[i])) {
                    pop_str[j++] = token[i];
                }
            }
            pop_str[j] = '\0';
            gov.population = atoll(pop_str);
        }

        if (strlen(gov.name) > 0) {
            generate_gov_sheet(gov);
        }
    }

    fclose(fp);

    // Save input file contents to generated/gov-list.txt
    FILE *out_fp = fopen("governments/generated/gov-list.txt", "w");
    if (out_fp == NULL) {
        perror("Error creating gov-list.txt");
        free(file_content);
        return 1;
    }
    fwrite(file_content, 1, content_size, out_fp);
    fclose(out_fp);
    free(file_content);

    return 0;
}
