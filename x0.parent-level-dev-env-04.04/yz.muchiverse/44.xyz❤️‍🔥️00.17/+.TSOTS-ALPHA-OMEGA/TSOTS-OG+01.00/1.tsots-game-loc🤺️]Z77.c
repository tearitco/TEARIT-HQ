#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#define BIBLE_BEGIN 3000
#define BIBLE_END 100109
#define LINE_SIZE 256
#define NUM_VERSES 4
#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0
#define LOCATION_FILE "location.txt"
#define PATH_SIZE 1024

const int SHOW_PREFIX = 0;

int randint(int seed) {
    if (seed < BIBLE_BEGIN || seed > BIBLE_END) {
        printf("Invalid seed. Must be between %d and %d.\n", BIBLE_BEGIN, BIBLE_END);
        exit(EXIT_FAILURE);
    }
    srand(time(NULL) + seed);
    return (rand() % (BIBLE_END + 1 - BIBLE_BEGIN)) + BIBLE_BEGIN;
}

void go_to_line(FILE *file, unsigned int num) {
    if (!file) {
        printf("Error: File pointer is null.\n");
        exit(EXIT_FAILURE);
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        printf("Error seeking file.\n");
        exit(EXIT_FAILURE);
    }
    struct stat st;
    if (fstat(fileno(file), &st) == 0 && num > st.st_size / 10) {
        printf("Error: Line number %u too large for file.\n", num);
        exit(EXIT_FAILURE);
    }
    for (unsigned int i = 0; i < num - 1; ++i) {
        char buffer[LINE_SIZE];
        if (!fgets(buffer, LINE_SIZE, file)) {
            printf("Error reading file at line %u.\n", i + 1);
            exit(EXIT_FAILURE);
        }
    }
}

int is_blank(const char *str) {
    if (!str) return 1;
    while (*str) {
        if (!isspace((unsigned char)*str)) {
            return 0;
        }
        str++;
    }
    return 1;
}

int has_verse_reference(char *line) {
    if (!line || !*line) return 0;
    char *ptr = line;
    while (*ptr && isdigit((unsigned char)*ptr)) {
        ptr++;
    }
    if (*ptr != ' ' || !*(ptr + 1)) return 0;
    ptr++;
    if (!isdigit((unsigned char)*ptr)) return 0;
    while (*ptr && isdigit((unsigned char)*ptr)) {
        ptr++;
    }
    if (*ptr != ':' || !*(ptr + 1)) return 0;
    ptr++;
    if (!isdigit((unsigned char)*ptr)) return 0;
    while (*ptr && isdigit((unsigned char)*ptr)) {
        ptr++;
    }
    if (*ptr != ' ' || !*(ptr + 1)) return 0;
    return 1;
}

void shuffle(int *array, int n) {
    if (!array || n <= 0) return;
    for (int i = n - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int temp = array[i];
        array[i] = array[j];
        array[j] = temp;
    }
}

void get_bible_path(char *path, size_t size) {
    FILE *loc = fopen(LOCATION_FILE, "r");
    if (!loc) {
        printf("Error: Could not open location file %s.\n", LOCATION_FILE);
        exit(EXIT_FAILURE);
    }
    if (!fgets(path, (int)size, loc)) {
        printf("Error: Could not read from %s.\n", LOCATION_FILE);
        fclose(loc);
        exit(EXIT_FAILURE);
    }
    fclose(loc);
    path[strcspn(path, "\r\n")] = '\0';
    if (*path == '\0') {
        printf("Error: %s is empty.\n", LOCATION_FILE);
        exit(EXIT_FAILURE);
    }
}

char *strip_prefix(char *verse) {
    if (SHOW_PREFIX || !verse || !*verse) {
        return verse ? verse : "";
    }
    char *ptr = verse;
    while (*ptr && isdigit((unsigned char)*ptr)) {
        ptr++;
    }
    if (*ptr == ' ') ptr++;
    while (*ptr && (isdigit((unsigned char)*ptr) || *ptr == ':')) {
        ptr++;
    }
    while (*ptr && isspace((unsigned char)*ptr)) {
        ptr++;
    }
    return *ptr ? ptr : "";
}



int main(int argc, char **argv) {
    int seed;
    int test_mode = 0;
    char *test_input = NULL;
    if (argc == 1) {
        srand(time(NULL));
        seed = (rand() % (BIBLE_END + 1 - BIBLE_BEGIN)) + BIBLE_BEGIN;
        printf("No seed provided. Using random seed: %d\n", seed);
    } else if (argc == 2) {
        seed = atoi(argv[1]);
        if (seed < BIBLE_BEGIN || seed > BIBLE_END) {
            printf("Invalid seed. Must be between %d and %d.\n", BIBLE_BEGIN, BIBLE_END);
            exit(EXIT_FAILURE);
        }
    } else if (argc == 3 && strcmp(argv[1], "--test") == 0) {
        test_mode = 1;
        seed = atoi(argv[2]);
        if (seed < BIBLE_BEGIN || seed > BIBLE_END) {
            printf("Invalid seed. Must be between %d and %d.\n", BIBLE_BEGIN, BIBLE_END);
            exit(EXIT_FAILURE);
        }
    } else if (argc == 4 && strcmp(argv[1], "--test") == 0) {
        test_mode = 1;
        seed = atoi(argv[2]);
        test_input = argv[3];
        if (seed < BIBLE_BEGIN || seed > BIBLE_END) {
            printf("Invalid seed. Must be between %d and %d.\n", BIBLE_BEGIN, BIBLE_END);
            exit(EXIT_FAILURE);
        }
    } else {
        printf("Usage: ./program [seed]\n");
        printf("  or: ./program --test seed [input]\n");
        exit(EXIT_FAILURE);
    }

    int start_line = randint(seed);
    printf("Starting at line: %d\n", start_line);
    char bible_path[PATH_SIZE];
    get_bible_path(bible_path, sizeof(bible_path));
    FILE *bible = fopen(bible_path, "r");
    if (!bible) {
        printf("Error opening file: %s\n", bible_path);
        return EXIT_FAILURE;
    }

    struct stat st;
    if (fstat(fileno(bible), &st) != 0 || st.st_size == 0) {
        printf("Error: Invalid or empty file.\n");
        fclose(bible);
        return EXIT_FAILURE;
    }

    char verses[NUM_VERSES][LINE_SIZE];
    int verse_count = 0;
    int retries = 0;
    const int MAX_RETRIES = 10; // Prevent infinite loops
    while (verse_count < NUM_VERSES && retries < MAX_RETRIES) {
        go_to_line(bible, start_line);
        unsigned int current_line = start_line;
        while (verse_count < NUM_VERSES) {
            char buffer[LINE_SIZE] = {0};
            if (!fgets(buffer, LINE_SIZE, bible)) {
                break; // Hit EOF, retry with new start_line
            }
            buffer[strcspn(buffer, "\n")] = '\0';
            current_line++;
            if (has_verse_reference(buffer)) {
                char *stripped = strip_prefix(buffer);
                if (*stripped != '\0' && !is_blank(stripped)) {
                    strncpy(verses[verse_count], buffer, LINE_SIZE - 1);
                    verses[verse_count][LINE_SIZE - 1] = '\0';
                    verse_count++;
                }
            }
            if (current_line - start_line > 1000) {
                break; // Too many lines, retry with new start_line
            }
        }
        if (verse_count < NUM_VERSES) {
            retries++;
            start_line = (rand() % (BIBLE_END + 1 - BIBLE_BEGIN)) + BIBLE_BEGIN;
            verse_count = 0; // Reset and try again
            printf("Retrying with new start line: %d (retry %d/%d)\n", start_line, retries, MAX_RETRIES);
        }
    }

    fclose(bible);

    if (verse_count < NUM_VERSES) {
        printf("Error: Could not find %d valid verses after %d retries.\n", NUM_VERSES, MAX_RETRIES);
        return EXIT_FAILURE;
    }

    int original[NUM_VERSES] = {0, 1, 2, 3};
    int scrambled[NUM_VERSES];
    for (int i = 0; i < verse_count; i++) {
        scrambled[i] = i;
    }
    shuffle(scrambled, verse_count);

    printf("\nScrambled Verses:\n");
    for (int i = 0; i < verse_count; i++) {
        char *verse_text = strip_prefix(verses[scrambled[i]]);
        if (*verse_text) {
            printf("Index %d: %s\n", i + 1, verse_text);
        } else {
            printf("Index %d: (empty verse, please report)\n", i + 1);
        }
    }

    if (test_mode) {
        char input[LINE_SIZE];
        if (test_input) {
            strncpy(input, test_input, LINE_SIZE - 1);
            input[LINE_SIZE - 1] = '\0';
        } else {
            snprintf(input, LINE_SIZE, "%d%d%d%d", 1, 2, 3, verse_count);
        }

        char cleaned[LINE_SIZE] = {0};
        int clean_idx = 0;
        for (int i = 0; input[i]; i++) {
            if (!isspace((unsigned char)input[i])) {
                cleaned[clean_idx++] = input[i];
            }
        }

        int parsed_count = 0;
        int user_answer[NUM_VERSES];
        for (int i = 0; cleaned[i] && parsed_count < verse_count; i++) {
            if (!isdigit((unsigned char)cleaned[i])) {
                parsed_count = -1;
                break;
            }
            int num = cleaned[i] - '0';
            if (num < 1 || num > verse_count) {
                parsed_count = -1;
                break;
            }
            user_answer[parsed_count] = num - 1;
            parsed_count++;
        }

        if (parsed_count == verse_count) {
            int used[verse_count];
            for (int i = 0; i < verse_count; i++) used[i] = 0;
            for (int i = 0; i < verse_count; i++) {
                if (used[user_answer[i]]) {
                    parsed_count = -1;
                    break;
                }
                used[user_answer[i]] = 1;
            }
        }

        if (parsed_count == verse_count) {
            printf("Test input '%s': VALID\n", input);
        } else {
            printf("Test input '%s': INVALID\n", input);
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }

    int user_answer[NUM_VERSES];
    int valid_input = 0;
    char input[LINE_SIZE];
    while (!valid_input) {
        printf("\nEnter the correct order (e.g., '");
        for (int i = 0; i < verse_count; i++) {
            printf("%d%s", i + 1, i < verse_count - 1 ? " " : "");
        }
        printf("' or '");
        for (int i = 0; i < verse_count; i++) {
            printf("%d", i + 1);
        }
        printf("'): ");

        if (!fgets(input, LINE_SIZE, stdin)) {
            printf("Error reading input. Please try again.\n");
            continue;
        }
        input[strcspn(input, "\n")] = '\0';

        char cleaned[LINE_SIZE] = {0};
        int clean_idx = 0;
        for (int i = 0; input[i]; i++) {
            if (!isspace((unsigned char)input[i])) {
                cleaned[clean_idx++] = input[i];
            }
        }

        int parsed_count = 0;
        for (int i = 0; cleaned[i] && parsed_count < verse_count; i++) {
            if (!isdigit((unsigned char)cleaned[i])) {
                parsed_count = -1;
                break;
            }
            int num = cleaned[i] - '0';
            if (num < 1 || num > verse_count) {
                parsed_count = -1;
                break;
            }
            user_answer[parsed_count] = num - 1;
            parsed_count++;
        }

        if (parsed_count == verse_count) {
            int used[verse_count];
            for (int i = 0; i < verse_count; i++) used[i] = 0;
            for (int i = 0; i < verse_count; i++) {
                if (used[user_answer[i]]) {
                    parsed_count = -1;
                    break;
                }
                used[user_answer[i]] = 1;
            }
        }

        if (parsed_count == verse_count) {
            valid_input = 1;
        } else {
            printf("Invalid input. Please enter %d unique numbers between 1 and %d (e.g., '1 2 3 4' or '1234').\n", verse_count, verse_count);
        }
    }

    int correct = 1;
    for (int i = 0; i < verse_count; i++) {
        if (user_answer[i] != original[scrambled[i]]) {
            correct = 0;
            break;
        }
    }

    if (correct) {
        printf("\nCorrect! Well done!\n");
    } else {
        printf("\nIncorrect.\n");
    }
    printf("Correct order was: ");
    for (int i = 0; i < verse_count; i++) {
        printf("%d%s", original[scrambled[i]] + 1, i < verse_count - 1 ? " " : "\n");
    }

    return EXIT_SUCCESS;
}
