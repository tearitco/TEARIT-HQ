#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define MAX_LINE_LENGTH 1024
#define MAX_VOCAB_SIZE 100000
#define MAX_RESPONSE_TOKENS 100

// Structure to hold a vocabulary entry
struct VocabEntry {
    int number;
    char word[100];
    float embedding;
    float pe;
    float weight;
    float bias1;
    float bias2;
    float bias3;
    float bias4;
};

// Function to find a word in the vocabulary
int find_word(struct VocabEntry *vocab, int vocab_size, const char *word) {
    for (int i = 0; i < vocab_size; i++) {
        if (strcmp(vocab[i].word, word) == 0) {
            return i;
        }
    }
    return -1;
}

// Function to apply softmax to a set of scores
void softmax(float *scores, int size, float temperature) {
    float max_score = scores[0];
    for (int i = 1; i < size; i++) {
        if (scores[i] > max_score) {
            max_score = scores[i];
        }
    }

    float sum = 0.0f;
    for (int i = 0; i < size; i++) {
        scores[i] = expf((scores[i] - max_score) / temperature);
        sum += scores[i];
    }

    for (int i = 0; i < size; i++) {
        scores[i] /= sum;
    }
}

// Function to predict the next word using temperature sampling
const char* predict_next_word(struct VocabEntry *vocab, int vocab_size, int current_word_index, float temperature) {
    float *scores = malloc(vocab_size * sizeof(float));
    if (!scores) {
        return "end-token";
    }

    // Get the vector of the current word
    float current_word_vec[] = {vocab[current_word_index].embedding, vocab[current_word_index].pe, vocab[current_word_index].weight, vocab[current_word_index].bias1, vocab[current_word_index].bias2, vocab[current_word_index].bias3, vocab[current_word_index].bias4};

    for (int i = 0; i < vocab_size; i++) {
        float next_word_vec[] = {vocab[i].embedding, vocab[i].pe, vocab[i].weight, vocab[i].bias1, vocab[i].bias2, vocab[i].bias3, vocab[i].bias4};
        // Simple dot product to influence the score
        float dot_product = 0.0f;
        for(int j=0; j<7; j++){
            dot_product += current_word_vec[j] * next_word_vec[j];
        }
        scores[i] = dot_product;
    }

    softmax(scores, vocab_size, temperature);

    // Determine how many top scores to consider based on temperature
    int top_n = 10;
    if (temperature < 0.5) {
        top_n = 5;  // More restrictive
    } else if (temperature > 2.0) {
        top_n = 20; // Less restrictive
    }
    
    // Make sure top_n doesn't exceed vocab_size
    if (top_n > vocab_size) {
        top_n = vocab_size;
    }

    // Save debug information to file
    FILE *debug_file = fopen("debug_chain.txt", "a");
    if (debug_file) {
        fprintf(debug_file, "\nDebug: Current word: %s (index: %d)\n", vocab[current_word_index].word, current_word_index);
        fprintf(debug_file, "Temperature: %f\n", temperature);
        fprintf(debug_file, "Considering top %d scores:\n", top_n);
        fclose(debug_file);
    }

    // Create array to store indices of top N scores
    int *top_indices = malloc(top_n * sizeof(int));
    float *top_scores = malloc(top_n * sizeof(float));
    
    // Initialize with first top_n elements
    for (int i = 0; i < top_n; i++) {
        top_indices[i] = i;
        top_scores[i] = scores[i];
    }
    
    // Find the actual top N scores using a simple selection sort approach
    for (int i = 0; i < top_n; i++) {
        int max_idx = i;
        for (int j = i + 1; j < top_n; j++) {
            if (top_scores[j] > top_scores[max_idx]) {
                max_idx = j;
            }
        }
        // Swap scores
        float temp_score = top_scores[i];
        top_scores[i] = top_scores[max_idx];
        top_scores[max_idx] = temp_score;
        // Swap indices
        int temp_idx = top_indices[i];
        top_indices[i] = top_indices[max_idx];
        top_indices[max_idx] = temp_idx;
    }
    
    // Now check the rest of the vocabulary to see if any scores are higher
    for (int i = top_n; i < vocab_size; i++) {
        for (int j = 0; j < top_n; j++) {
            if (scores[i] > top_scores[j]) {
                // Shift elements down
                for (int k = top_n - 1; k > j; k--) {
                    top_scores[k] = top_scores[k-1];
                    top_indices[k] = top_indices[k-1];
                }
                // Insert new element
                top_scores[j] = scores[i];
                top_indices[j] = i;
                break;
            }
        }
    }

    // Log top scores to debug file
    debug_file = fopen("debug_chain.txt", "a");
    if (debug_file) {
        for (int i = 0; i < top_n; i++) {
            fprintf(debug_file, "  %d. %s (index: %d): %f\n", i+1, vocab[top_indices[i]].word, top_indices[i], top_scores[i]);
        }
        fclose(debug_file);
    }

    // Randomly select from the top N indices
    int next_word_index = top_indices[0]; // Default to top score
    
    if (top_n > 1) {
        // Use temperature to influence selection from top N
        float r = (float)rand() / (float)RAND_MAX;
        int selected_idx = (int)(r * top_n);
        if (selected_idx >= top_n) selected_idx = top_n - 1;
        next_word_index = top_indices[selected_idx];
    }

    // Log the chosen word
    debug_file = fopen("debug_chain.txt", "a");
    if (debug_file) {
        fprintf(debug_file, "Chosen word: %s (index: %d)\n", vocab[next_word_index].word, next_word_index);
        fclose(debug_file);
    }

    free(scores);
    free(top_indices);
    free(top_scores);

    return vocab[next_word_index].word;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <vocab_model.txt> \"<prompt>\" [length] [temperature]\n", argv[0]);
        return 1;
    }

    srand(time(NULL));

    char *vocab_file = argv[1];
    char *prompt = argv[2];
    int desired_length = -1;
    float temperature = 1.0f;

    if (argc >= 4) {
        desired_length = atoi(argv[3]);
    }
    if (argc >= 5) {
        temperature = atof(argv[4]);
    }

    FILE *infile = fopen(vocab_file, "r");
    if (!infile) {
        perror("Error opening vocab file");
        return 1;
    }

    struct VocabEntry *vocab = malloc(MAX_VOCAB_SIZE * sizeof(struct VocabEntry));
    if (!vocab) {
        perror("Failed to allocate memory for vocabulary");
        fclose(infile);
        return 1;
    }

    char line[MAX_LINE_LENGTH];
    int vocab_size = 0;

    // Skip header line
    fgets(line, sizeof(line), infile);

    while (fgets(line, sizeof(line), infile) && vocab_size < MAX_VOCAB_SIZE) {
        sscanf(line, "%d %s %f %f %f %f %f %f %f",
               &vocab[vocab_size].number,
               vocab[vocab_size].word,
               &vocab[vocab_size].embedding,
               &vocab[vocab_size].pe,
               &vocab[vocab_size].weight,
               &vocab[vocab_size].bias1,
               &vocab[vocab_size].bias2,
               &vocab[vocab_size].bias3,
               &vocab[vocab_size].bias4);
        vocab_size++;
    }

    fclose(infile);

    printf("Prompt: %s\n", prompt);
    
    // Create debug file and write prompt
    FILE *debug_file = fopen("debug_chain.txt", "w");
    if (debug_file) {
        fprintf(debug_file, "=== Chatbot Debug Log ===\n");
        fprintf(debug_file, "Prompt: %s\n", prompt);
        fprintf(debug_file, "Desired length: %d\n", desired_length);
        fprintf(debug_file, "Temperature: %f\n", temperature);
        fprintf(debug_file, "========================\n\n");
        fclose(debug_file);
    }
    
    printf("Generating response: ");

    char *token = strtok(prompt, " ");
    int last_word_index = -1;

    while (token != NULL) {
        last_word_index = find_word(vocab, vocab_size, token);
        token = strtok(NULL, " ");
    }

    if (last_word_index == -1) {
        last_word_index = find_word(vocab, vocab_size, "start-token");
    }

    int length_count = 0;
    char response_buffer[MAX_RESPONSE_TOKENS * 100]; // Max 100 tokens, each max 100 chars
    response_buffer[0] = '\0'; // Initialize empty string

    // Ensure we have a minimum temperature for variety
    float min_temperature = 0.1f;
    if (temperature < min_temperature) temperature = min_temperature;

    const char* next_word = predict_next_word(vocab, vocab_size, last_word_index, temperature);

    // Generate at least one word
    if (strcmp(next_word, "end-token") == 0) {
        // Try once more with higher temperature
        next_word = predict_next_word(vocab, vocab_size, last_word_index, temperature * 2.0f);
    }

    while (strcmp(next_word, "end-token") != 0 && (desired_length == -1 || length_count < desired_length) && length_count < MAX_RESPONSE_TOKENS) {
        snprintf(response_buffer + strlen(response_buffer), sizeof(response_buffer) - strlen(response_buffer), "%s ", next_word);
        printf("\rGenerating response: %d/%d tokens", length_count + 1, desired_length == -1 ? MAX_RESPONSE_TOKENS : desired_length);
        fflush(stdout);

        last_word_index = find_word(vocab, vocab_size, next_word);
        if (last_word_index == -1) {
            // If word not found, use start-token
            last_word_index = find_word(vocab, vocab_size, "start-token");
        }
        
        next_word = predict_next_word(vocab, vocab_size, last_word_index, temperature);
        length_count++;
    }

    printf("\rResponse: %s\n", response_buffer);
    
    // Write final response to debug file
    debug_file = fopen("debug_chain.txt", "a");
    if (debug_file) {
        fprintf(debug_file, "\n=== Final Response ===\n");
        fprintf(debug_file, "%s\n", response_buffer);
        fprintf(debug_file, "======================\n");
        fclose(debug_file);
    }

    free(vocab);

    return 0;
}