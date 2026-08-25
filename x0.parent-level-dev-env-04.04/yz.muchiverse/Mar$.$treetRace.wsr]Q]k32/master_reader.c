#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    // Check and initialize data/master_reader.txt
    char reader_path[256] = "data/master_reader.txt";
    FILE *reader_fp = fopen(reader_path, "r");
    if (reader_fp == NULL) {
        reader_fp = fopen(reader_path, "w");
        if (reader_fp == NULL) {
            printf("Error: Could not create data/master_reader.txt.\n");
            return 1;
        }
        fprintf(reader_fp, "0\n");
        fclose(reader_fp);
        reader_fp = fopen(reader_path, "r");
        if (reader_fp == NULL) {
            printf("Error: Could not open data/master_reader.txt after creation.\n");
            return 1;
        }
    }

    // Read the last processed line number
    long last_processed_line = 0;
    if (fscanf(reader_fp, "%ld", &last_processed_line) != 1) {
        last_processed_line = 0;
    }
    fclose(reader_fp);

    // Open master_ledger.txt
    FILE *ledger_fp = fopen("data/master_ledger.txt", "r");
    if (ledger_fp == NULL) {
        printf("Error: Could not open data/master_ledger.txt.\n");
        return 1;
    }

    // Count total lines and process new lines
    char line[512];
    long current_line = 0;
    long new_last_processed_line = last_processed_line;

    while (fgets(line, sizeof(line), ledger_fp)) {
        current_line++;
        if (current_line <= last_processed_line) {
            continue; // Skip already processed lines
        }

        if (strstr(line, "Event: incorporation.+x")) {
            char date_str[20], ticker[10], player_name[100], corp_name[100], industry_name[100], country_name[100];
            int funding_amount;

            // Parse the ledger entry
            char *token = strtok(line, "|");
            if (token && sscanf(token, "Time: %19s", date_str) == 1) {
                token = strtok(NULL, "|");
                if (token && sscanf(token, " Debit: %9s", ticker) == 1) {
                    token = strtok(NULL, "|");
                    if (token && sscanf(token, " Credit: %99[^\n|]", player_name) == 1) {
                        token = strtok(NULL, "|");
                        if (token && sscanf(token, " Amount: %d.00 Dollars", &funding_amount) == 1) {
                            token = strtok(NULL, "|");
                            if (token && strstr(token, "Event: incorporation.+x")) {
                                token = strtok(NULL, "|");
                                if (token && sscanf(token, " Corp Name: %99[^\n|]", corp_name) == 1) {
                                    token = strtok(NULL, "|");
                                    if (token && sscanf(token, " Industry: %99[^\n|]", industry_name) == 1) {
                                        token = strtok(NULL, "|");
                                        if (token && sscanf(token, " Country: %99[^\n|]", country_name) == 1) {
                                            // Construct and execute the system command
                                            char command[512];
                                            snprintf(command, sizeof(command), "./+x/incorporation.+x \"%s\" %s \"%s\" %d \"%s\" \"%s\" \"%s\"",
                                                     date_str, ticker, player_name, funding_amount, corp_name, industry_name, country_name);
                                            system(command);
                                            new_last_processed_line = current_line;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    fclose(ledger_fp);

    // Update data/master_reader.txt with the new last processed line number
    reader_fp = fopen(reader_path, "w");
    if (reader_fp == NULL) {
        printf("Error: Could not open data/master_reader.txt for writing.\n");
        return 1;
    }
    fprintf(reader_fp, "%ld\n", new_last_processed_line);
    fclose(reader_fp);

    return 0;
}
