#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINES 50
#define MAX_LEN 1024

void clear_screen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

int main() {
    FILE *fp;
    char lines[MAX_LINES][MAX_LEN];
    int line_count = 0;

    fp = fopen("data/setting.txt", "r");
    if (fp == NULL) {
        printf("Error opening setting.txt\n");
        return 1;
    }

    while (fgets(lines[line_count], MAX_LEN, fp) && line_count < MAX_LINES) {
        lines[line_count][strcspn(lines[line_count], "\n")] = 0; // remove newline
        line_count++;
    }
    fclose(fp);

    int choice;
    while (1) {
        clear_screen();
        printf("--- Settings ---\n");
        for (int i = 0; i < line_count; i++) {
            printf("%d. %s\n", i + 1, lines[i]);
        }
        printf("%d. Back to main menu\n", line_count + 1);
        printf("\nEnter your choice: ");
        scanf("%d", &choice);

        if (choice > 0 && choice <= line_count) {
            if (choice == 1) { // Corresponds to the first line in data/setting.txt, assumed to be ticker_speed
                int speed_choice;
                const char *options[] = {"cent", "sec", "min", "hour", "day"};
                int num_options = sizeof(options) / sizeof(options[0]);

                clear_screen();
                printf("--- Change Ticker Speed ---\n");
                printf("Current setting: %s\n\n", lines[choice - 1]);
                printf("Select new speed:\n");
                for (int i = 0; i < num_options; i++) {
                    printf("%d. %s\n", i + 1, options[i]);
                }
                printf("\nEnter your choice: ");
                scanf("%d", &speed_choice);

                if (speed_choice > 0 && speed_choice <= num_options) {
                    char setting_name[MAX_LEN];
                    char *colon_ptr = strchr(lines[choice - 1], ':');
                    if (colon_ptr != NULL) {
                        int name_len = colon_ptr - lines[choice - 1];
                        strncpy(setting_name, lines[choice - 1], name_len);
                        setting_name[name_len] = '\0';

                        char new_setting[MAX_LEN];
                        // Dynamically allocate memory to avoid buffer overflow warnings
                        int needed = snprintf(NULL, 0, "%s: %s", setting_name, options[speed_choice - 1]);
                        char *temp_setting = malloc(needed + 1);
                        if (temp_setting) {
                            sprintf(temp_setting, "%s: %s", setting_name, options[speed_choice - 1]);
                            // Copy to new_setting with bounds checking
                            strncpy(new_setting, temp_setting, sizeof(new_setting) - 1);
                            new_setting[sizeof(new_setting) - 1] = '\0';
                            free(temp_setting);
                        }
                        strcpy(lines[choice - 1], new_setting);

                        fp = fopen("data/setting.txt", "w");
                        if (fp == NULL) {
                            printf("Error saving settings.\n");
                        } else {
                            for (int i = 0; i < line_count; i++) {
                                fprintf(fp, "%s\n", lines[i]);
                            }
                            fclose(fp);
                            printf("Ticker speed updated successfully.\n");
                        }
                    } else {
                        printf("Error: Invalid format in settings file (missing :).\n");
                    }
                } else {
                    printf("Invalid speed choice.\n");
                }
            } else {
                printf("Setting '%s' not implemented yet.\n", lines[choice - 1]);
            }
            printf("\nPress Enter to continue...");
            while (getchar() != '\n'); // Consume leftover newline
            getchar(); // Wait for user input
        } else if (choice == line_count + 1) {
            break;
        } else {
            printf("Invalid choice.\n");
            printf("Press Enter to continue...");
            while (getchar() != '\n'); // Consume leftover newline
            getchar(); // Wait for user input
        }
    }

    return 0;
}
