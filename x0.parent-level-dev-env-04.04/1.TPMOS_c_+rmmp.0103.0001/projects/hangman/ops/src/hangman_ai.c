#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>

#define MAX_PATH 4096
#define MAX_LINE 1024
#define MAX_WORD_LEN 100

char project_root[MAX_PATH] = "."; // Assuming this is globally available or passed in

char* trim_str(char *str) {
    char *end;
    if (!str) return str;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

void resolve_paths() { // Simplified for Op, assumes it's run from correct dir or proj_root is set
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *k = trim_str(line);
                char *v = trim_str(eq + 1);
                if (strcmp(k, "project_root") == 0) snprintf(project_root, sizeof(project_root), "%s", v);
            }
        }
        fclose(kvp);
    }
}

// AI Strategy: Guess common letters first, then less common. Fallback to unused.
char* guess_letter_strategy(const char* secret_word, const char* guessed_letters) {
    const char* common_letters = "etaoinsrhdlu"; // Frequency order for English
    const char* less_common = "wcfgypvbjqkxz";
    
    char* guess = malloc(2 * sizeof(char)); // For the guessed letter + null terminator
    if (!guess) {
        perror("malloc failed");
        return NULL;
    }

    // Try common letters first
    for (int i = 0; i < strlen(common_letters); i++) {
        char c = common_letters[i];
        if (strchr(secret_word, c) != NULL && strchr(guessed_letters, c) == NULL) {
            sprintf(guess, "%c", c);
            return guess;
        }
    }

    // Try less common letters
    for (int i = 0; i < strlen(less_common); i++) {
        char c = less_common[i];
        if (strchr(secret_word, c) != NULL && strchr(guessed_letters, c) == NULL) {
            sprintf(guess, "%c", c);
            return guess;
        }
    }
    
    // Fallback: guess a random unused letter from alphabet if no common ones found
    char alphabet[] = "abcdefghijklmnopqrstuvwxyz";
    for (int i = 0; i < strlen(alphabet); i++) {
        char c = alphabet[i];
        if (strchr(secret_word, c) == NULL && strchr(guessed_letters, c) == NULL) { // If not in secret word AND not guessed
             sprintf(guess, "%c", c);
             return guess;
        }
    }
    
    return NULL; // Should not happen if alphabet is complete and there are unguessed letters
}

int main(int argc, char *argv[]) {
    resolve_paths();

    if (argc < 3) {
        fprintf(stderr, "Usage: hangman_ai <secret_word> <guessed_letters>\n");
        return 1;
    }

    const char *secret_word_arg = argv[1];
    const char *guessed_letters_str = argv[2];
    
    char *guess = guess_letter_strategy(secret_word_arg, guessed_letters_str);
    
    if (guess) {
        printf("%s\n", guess); // Output the AI's guess
        free(guess);
    } else {
        fprintf(stderr, "AI could not determine a guess.\n");
        return 1;
    }

    return 0;
}
