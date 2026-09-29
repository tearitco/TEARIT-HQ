#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <ctype.h>

#define MAX_GOVS 100
#define MAX_CORPS 100
#define MAX_BANKS 20
#define MAX_LINE_LEN 256
#define MAX_FILE_SIZE 1048576 // 1MB buffer for input file

typedef struct {
    char name[100];
} Government;

typedef struct {
    char name[100];
    char ticker[20];
} Corporation;

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

int read_governments(Government govs[]) {
    FILE *fp = fopen("governments/generated/gov-list.txt", "r");
    if (fp == NULL) {
        perror("Error opening governments/generated/gov-list.txt");
        return 0;
    }

    int count = 0;
    char line[MAX_LINE_LEN];
    fgets(line, sizeof(line), fp); // Skip header
    while (fgets(line, sizeof(line), fp) != NULL && count < MAX_GOVS) {
        char *token = strtok(line, "\t");
        if (token) {
            token = strtok(NULL, "\t");
            if (token) {
                strcpy(govs[count].name, trim(token));
                count++;
            }
        }
    }

    fclose(fp);
    return count;
}

int read_banks(Corporation banks[]) {
    FILE *fp = fopen("corporations/starting_corporations.txt", "r");
    if (fp == NULL) {
        perror("Error opening corporations/starting_corporations.txt");
        return 0;
    }

    int count = 0;
    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), fp) != NULL && count < MAX_BANKS) {
        if (strstr(line, "Ind: BANKING") != NULL) {
            char *name_end = strrchr(line, '(');
            if (name_end) {
                *name_end = '\0';
                char* name = trim(line);

                char *ticker_start = name_end + 1;
                char *ticker_end = strrchr(ticker_start, ')');
                if (ticker_end) {
                    *ticker_end = '\0';
                    sprintf(banks[count].name, "%s (%s)", name, ticker_start);
                    count++;
                }
            }
        }
    }

    fclose(fp);
    return count;
}

void update_corporation_file(const char *filename, Government govs[], int num_govs, Corporation banks[], int num_banks) {
    char file_path[512];
    sprintf(file_path, "corporations/generated/%s", filename);
    char temp_path[512];
    sprintf(temp_path, "corporations/generated/%s.tmp", filename);

    FILE *fp = fopen(file_path, "r");
    if (fp == NULL) {
        perror("Error opening corporation file for reading");
        return;
    }

    FILE *temp_fp = fopen(temp_path, "w");
    if (temp_fp == NULL) {
        perror("Error opening temp file for writing");
        fclose(fp);
        return;
    }

    char line[MAX_LINE_LEN];
    int is_bank = 0;
    // first check if the file is a bank
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strstr(line, "Industry Group:") && strstr(line, "BANKING")) {
            is_bank = 1;
            break;
        }
    }
    rewind(fp);

    if(is_bank) {
        while (fgets(line, sizeof(line), fp) != NULL) {
            fprintf(temp_fp, "%s", line);
        }
    } else {
        while (fgets(line, sizeof(line), fp) != NULL) {
            if (strstr(line, "Incorporated in:")) {
                int random_gov_index = rand() % num_govs;
                fprintf(temp_fp, "  Incorporated in:\t\t\t%s\n", govs[random_gov_index].name);
            } else if (strstr(line, "Lending Bank:")) {
                int random_bank_index = rand() % num_banks;
                fprintf(temp_fp, "  Lending Bank:\t\t\t\t%s\n", banks[random_bank_index].name);
            } else {
                fprintf(temp_fp, "%s", line);
            }
        }
    }

    fclose(fp);
    fclose(temp_fp);

    remove(file_path);
    rename(temp_path, file_path);
}

int main() {
    srand(time(NULL));

    // Open input file to read contents
    FILE *input_fp = fopen("corporations/starting_corporations.txt", "r");
    if (input_fp == NULL) {
        perror("Error opening corporations/starting_corporations.txt");
        return 1;
    }

    // Read and store entire input file content
    char *file_content = malloc(MAX_FILE_SIZE);
    if (file_content == NULL) {
        perror("Memory allocation failed");
        fclose(input_fp);
        return 1;
    }
    size_t content_size = 0;
    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), input_fp) && content_size < MAX_FILE_SIZE - MAX_LINE_LEN) {
        strncpy(file_content + content_size, line, MAX_LINE_LEN);
        content_size += strlen(line);
    }
    fclose(input_fp);

    Government govs[MAX_GOVS];
    int num_govs = read_governments(govs);
    if (num_govs == 0) {
        printf("No governments found.\n");
        free(file_content);
        return 1;
    }

    Corporation banks[MAX_BANKS];
    int num_banks = read_banks(banks);
    if (num_banks == 0) {
        printf("No banks found.\n");
        free(file_content);
        return 1;
    }

    DIR *d;
    struct dirent *dir;
    d = opendir("corporations/generated");
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_type == DT_DIR && strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
                char file_path[512];
                sprintf(file_path, "%s/%s.txt", dir->d_name, dir->d_name);
                update_corporation_file(file_path, govs, num_govs, banks, num_banks);
            }
        }
        closedir(d);
    }

    // Save input file contents to corporations/generated/martian_corporations.txt
    FILE *out_fp = fopen("corporations/generated/martian_corporations.txt", "w");
    if (out_fp == NULL) {
        perror("Error creating martian_corporations.txt");
        free(file_content);
        return 1;
    }
    fwrite(file_content, 1, content_size, out_fp);
    fclose(out_fp);
    free(file_content);

    printf("Corporation files updated and martian_corporations.txt created.\n");

    return 0;
}
