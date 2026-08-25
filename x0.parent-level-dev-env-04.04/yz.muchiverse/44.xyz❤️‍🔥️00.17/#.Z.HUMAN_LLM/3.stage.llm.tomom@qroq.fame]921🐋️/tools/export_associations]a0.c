#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_LINE_LENGTH 1024
#define MAX_VOCAB_SIZE 100000
#define EMBEDDING_DIM 7

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

// Function to read vocabulary from file
int read_vocab(const char *filename, struct VocabEntry *vocab) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening vocab file");
        return -1;
    }
    
    char line[MAX_LINE_LENGTH];
    int vocab_size = 0;
    
    // Skip header line
    fgets(line, sizeof(line), file);
    
    while (fgets(line, sizeof(line), file) && vocab_size < MAX_VOCAB_SIZE) {
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
    
    fclose(file);
    return vocab_size;
}

// Function to compute dot product between two word vectors
float dot_product(struct VocabEntry *word1, struct VocabEntry *word2) {
    float vec1[EMBEDDING_DIM] = {word1->embedding, word1->pe, word1->weight, 
                                word1->bias1, word1->bias2, word1->bias3, word1->bias4};
    float vec2[EMBEDDING_DIM] = {word2->embedding, word2->pe, word2->weight, 
                                word2->bias1, word2->bias2, word2->bias3, word2->bias4};
    
    float dot = 0.0f;
    for (int i = 0; i < EMBEDDING_DIM; i++) {
        dot += vec1[i] * vec2[i];
    }
    return dot;
}

// Function to normalize a vector
void normalize_vector(float *vec, int size) {
    float norm = 0.0f;
    for (int i = 0; i < size; i++) {
        norm += vec[i] * vec[i];
    }
    norm = sqrtf(norm);
    
    if (norm > 0.0f) {
        for (int i = 0; i < size; i++) {
            vec[i] /= norm;
        }
    }
}

// Function to compute cosine similarity between two word vectors
float cosine_similarity(struct VocabEntry *word1, struct VocabEntry *word2) {
    float vec1[EMBEDDING_DIM] = {word1->embedding, word1->pe, word1->weight, 
                                word1->bias1, word1->bias2, word1->bias3, word1->bias4};
    float vec2[EMBEDDING_DIM] = {word2->embedding, word2->pe, word2->weight, 
                                word2->bias1, word2->bias2, word2->bias3, word2->bias4};
    
    normalize_vector(vec1, EMBEDDING_DIM);
    normalize_vector(vec2, EMBEDDING_DIM);
    
    float dot = 0.0f;
    for (int i = 0; i < EMBEDDING_DIM; i++) {
        dot += vec1[i] * vec2[i];
    }
    return dot;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <vocab_model.txt> <output_prefix>\n", argv[0]);
        fprintf(stderr, "Example: %s vocab_model.txt associations\n", argv[0]);
        return 1;
    }
    
    const char *vocab_file = argv[1];
    const char *output_prefix = argv[2];
    
    struct VocabEntry *vocab = malloc(MAX_VOCAB_SIZE * sizeof(struct VocabEntry));
    if (!vocab) {
        perror("Failed to allocate memory for vocabulary");
        return 1;
    }
    
    printf("Reading vocabulary from %s...\n", vocab_file);
    int vocab_size = read_vocab(vocab_file, vocab);
    if (vocab_size < 0) {
        free(vocab);
        return 1;
    }
    
    printf("Vocabulary size: %d words\n", vocab_size);
    
    // Create output filename for CSV
    char csv_filename[256];
    snprintf(csv_filename, sizeof(csv_filename), "%s.csv", output_prefix);
    
    FILE *csv_file = fopen(csv_filename, "w");
    if (!csv_file) {
        perror("Error opening CSV output file");
        free(vocab);
        return 1;
    }
    
    // Write CSV header
    fprintf(csv_file, ",");
    for (int i = 0; i < vocab_size; i++) {
        fprintf(csv_file, "%s", vocab[i].word);
        if (i < vocab_size - 1) {
            fprintf(csv_file, ",");
        }
    }
    fprintf(csv_file, "\n");
    
    // Compute and write association matrix
    float max_assoc = -INFINITY;
    float min_assoc = INFINITY;
    float sum_assoc = 0.0f;
    int count = 0;
    
    printf("Computing associations...\n");
    for (int i = 0; i < vocab_size; i++) {
        fprintf(csv_file, "%s", vocab[i].word);
        for (int j = 0; j < vocab_size; j++) {
            float assoc = cosine_similarity(&vocab[i], &vocab[j]);
            fprintf(csv_file, ",%f", assoc);
            
            if (assoc > max_assoc) max_assoc = assoc;
            if (assoc < min_assoc) min_assoc = assoc;
            sum_assoc += assoc;
            count++;
        }
        fprintf(csv_file, "\n");
        
        // Progress indicator
        if ((i + 1) % 10 == 0 || i == vocab_size - 1) {
            printf("\rProgress: %d/%d (%.1f%%)", i + 1, vocab_size, 
                   (float)(i + 1) / vocab_size * 100);
            fflush(stdout);
        }
    }
    
    printf("\n");
    fclose(csv_file);
    
    // Print statistics
    printf("Association matrix saved to %s\n", csv_filename);
    printf("Matrix shape: %dx%d\n", vocab_size, vocab_size);
    printf("Max association: %.4f\n", max_assoc);
    printf("Min association: %.4f\n", min_assoc);
    printf("Mean association: %.4f\n", sum_assoc / count);
    
    // Also save some statistics to a file
    char stats_filename[256];
    snprintf(stats_filename, sizeof(stats_filename), "%s_stats.txt", output_prefix);
    
    FILE *stats_file = fopen(stats_filename, "w");
    if (stats_file) {
        fprintf(stats_file, "Vocabulary size: %d\n", vocab_size);
        fprintf(stats_file, "Matrix shape: %dx%d\n", vocab_size, vocab_size);
        fprintf(stats_file, "Max association: %.6f\n", max_assoc);
        fprintf(stats_file, "Min association: %.6f\n", min_assoc);
        fprintf(stats_file, "Mean association: %.6f\n", sum_assoc / count);
        fclose(stats_file);
        printf("Statistics saved to %s\n", stats_filename);
    }
    
    free(vocab);
    printf("Done!\n");
    return 0;
}