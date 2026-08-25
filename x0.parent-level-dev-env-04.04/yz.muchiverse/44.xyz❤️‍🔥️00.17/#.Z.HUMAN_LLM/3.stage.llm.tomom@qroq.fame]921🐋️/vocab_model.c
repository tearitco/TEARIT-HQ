#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <errno.h>
#include <libgen.h>

#define MAX_WORD_LENGTH 100
#define MAX_VOCAB_SIZE 100000

// Structure to hold a vocabulary entry
struct VocabEntry {
    char word[MAX_WORD_LENGTH];
    int count;
    float embedding;
    float pe;
    float weight;
    float bias1;
    float bias2;
    float bias3;
    float bias4;
};

// Function to add a word to the vocabulary
int add_word(struct VocabEntry *vocab, int vocab_size, const char *word) {
    if (vocab_size >= MAX_VOCAB_SIZE) {
        return vocab_size;
    }

    // Use strncpy to ensure we don't overflow the buffer, and handle UTF-8 properly
    strncpy(vocab[vocab_size].word, word, MAX_WORD_LENGTH - 1);
    vocab[vocab_size].word[MAX_WORD_LENGTH - 1] = '\0';  // Ensure null termination
    
    vocab[vocab_size].count = 1;
    vocab[vocab_size].embedding = (float)rand() / RAND_MAX;
    vocab[vocab_size].pe = 0.0f;
    vocab[vocab_size].weight = (float)rand() / RAND_MAX;
    vocab[vocab_size].bias1 = 0.0f;
    vocab[vocab_size].bias2 = 0.0f;
    vocab[vocab_size].bias3 = 0.0f;
    vocab[vocab_size].bias4 = 0.0f;
    return vocab_size + 1;
}

// Function to count words in a file
int count_words_in_file(const char *filename) {
    FILE *file = fopen(filename, "rb");  // Open in binary mode for UTF-8
    if (!file) {
        return 0;
    }
    int count = 0;
    char buffer[MAX_WORD_LENGTH * 10];
    while (fgets(buffer, sizeof(buffer), file)) {
        char *token = strtok(buffer, ",\n");
        while (token != NULL) {
            count++;
            token = strtok(NULL, ",\n");
        }
    }
    fclose(file);
    return count;
}

// Function to check if a character is a space or tab (but not part of an emoji)
int is_word_delimiter(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ',' || c == ';' || c == ':' || 
           c == '!' || c == '?' || c == '.' || c == '(' || c == ')' || c == '[' || c == ']' || 
           c == '{' || c == '}' || c == '"' || c == '\'';
}

// Function to process a file and add its words to the vocabulary
int process_file(const char *filename, struct VocabEntry *vocab, int vocab_size) {
    fprintf(stderr, "Processing file: %s\n", filename);
    int total_words_in_file = count_words_in_file(filename);
    FILE *file = fopen(filename, "rb");  // Open in binary mode for UTF-8

    if (!file) {
        perror("Error opening file");
        return vocab_size;
    }

    char word[MAX_WORD_LENGTH];
    int is_new_sentence = 1;

    // Add start-token for the beginning of the file
    vocab_size = add_word(vocab, vocab_size, "start-token");

    char buffer[MAX_WORD_LENGTH * 10]; // Larger buffer to handle emojis
    int words_processed_in_file = 0;

    while (fgets(buffer, sizeof(buffer), file)) {
        // Process the buffer character by character to handle emojis properly
        int i = 0;
        int len = strlen(buffer);
        
        while (i < len) {
            // Skip whitespace
            while (i < len && is_word_delimiter(buffer[i])) {
                if (buffer[i] == '.' || buffer[i] == '?' || buffer[i] == '!') {
                    vocab_size = add_word(vocab, vocab_size, "end-token");
                    words_processed_in_file++;
                    is_new_sentence = 1;
                }
                i++;
            }
            
            if (i >= len) break;
            
            // Extract word (including potential emojis)
            int word_start = i;
            while (i < len && !is_word_delimiter(buffer[i])) {
                i++;
            }
            
            // Copy the word
            int word_len = i - word_start;
            if (word_len > 0 && word_len < MAX_WORD_LENGTH - 1) {
                strncpy(word, &buffer[word_start], word_len);
                word[word_len] = '\0';
                
                // Add word to vocabulary
                if (strlen(word) > 0) {
                    vocab_size = add_word(vocab, vocab_size, word);
                    words_processed_in_file++;
                    is_new_sentence = 0;
                }
            }
            
            if (words_processed_in_file % 100 == 0 || words_processed_in_file == total_words_in_file) {
                fprintf(stderr, "\rProcessing %s: Words processed: %d/%d (Current Vocab size: %d)", filename, words_processed_in_file, total_words_in_file, vocab_size);
                fflush(stderr);
            }
        }
    }

    fprintf(stderr, "\n"); // Newline after file processing

    fclose(file);
    return vocab_size;
}



int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file1.txt> [file2.txt] ...\n", argv[0]);
        return 1;
    }

    // --- Path generation logic ---
    char curriculum_base_name[256] = "";
    for (int i = 1; i < argc; i++) {
        char *path_copy = strdup(argv[i]);
        char *bname = basename(path_copy);
        char *dot = strrchr(bname, '.');
        if (dot) {
            *dot = '\0';
        }
        if (i > 1) {
            strcat(curriculum_base_name, "_");
        }
        strcat(curriculum_base_name, bname);
        free(path_copy);
    }

    char curriculum_dir[512];
    sprintf(curriculum_dir, "curriculum/%s", curriculum_base_name);

    // Create curriculum directory
    struct stat st = {0};
    if (stat("curriculum", &st) == -1) {
        mkdir("curriculum", 0700);
    }
    if (stat(curriculum_dir, &st) == -1) {
        mkdir(curriculum_dir, 0700);
    }

    char outfile_path[1024];
    sprintf(outfile_path, "%s/%s.txt", curriculum_dir, curriculum_base_name);
    // --- End of path generation logic ---

    struct VocabEntry *vocab = malloc(MAX_VOCAB_SIZE * sizeof(struct VocabEntry));
    if (!vocab) {
        perror("Failed to allocate memory for vocabulary");
        return 1;
    }
    int vocab_size = 0;

    for (int i = 1; i < argc; i++) {
        vocab_size = process_file(argv[i], vocab, vocab_size);
    }

    // Ensure end-token is always in the vocabulary
    vocab_size = add_word(vocab, vocab_size, "end-token");

    // Calculate positional encoding
    for (int i = 0; i < vocab_size; i++) {
        vocab[i].pe = (float)i / (float)vocab_size;
    }

    FILE *outfile = fopen(outfile_path, "w");
    if (!outfile) {
        perror("Failed to open output file");
        free(vocab);
        return 1;
    }

    fprintf(outfile, "number word embedding pe weight bias1 bias2 bias3 bias4\n");
    for (int i = 0; i < vocab_size; i++) {
        fprintf(outfile, "%d %s %f %f %f %f %f %f %f\n",
                i + 1,
                vocab[i].word,
                vocab[i].embedding,
                vocab[i].pe,
                vocab[i].weight,
                vocab[i].bias1,
                vocab[i].bias2,
                vocab[i].bias3,
                vocab[i].bias4);
    }

    fclose(outfile);
    free(vocab);

    // Print the path for other tools
    printf("%s\n", outfile_path);

    return 0;
}