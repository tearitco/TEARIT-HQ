#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <libgen.h>

#define MAX_LINE_LENGTH 1024
#define MAX_VOCAB_SIZE 100000

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

// Function to calculate cosine similarity between two vectors
double cosine_similarity(float *v1, float *v2, int size) {
    double dot_product = 0.0;
    double norm1 = 0.0;
    double norm2 = 0.0;
    for (int i = 0; i < size; i++) {
        dot_product += v1[i] * v2[i];
        norm1 += v1[i] * v1[i];
        norm2 += v2[i] * v2[i];
    }
    if (norm1 == 0.0 || norm2 == 0.0) {
        return 0.0;
    }
    return dot_product / (sqrt(norm1) * sqrt(norm2));
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <vocab_model.txt> <word1> <word2>\n", argv[0]);
        return 1;
    }

    char *vocab_file = argv[1];
    char *word1 = argv[2];
    char *word2 = argv[3];

    // Path generation logic
    char *vocab_path_copy = strdup(vocab_file);
    char *output_dir = dirname(vocab_path_copy);
    char similarity_path[1024];
    sprintf(similarity_path, "%s/similarity.txt", output_dir);
    // End of path generation logic

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

    int index1 = -1, index2 = -1;
    for (int i = 0; i < vocab_size; i++) {
        if (strcmp(vocab[i].word, word1) == 0) {
            index1 = i;
        }
        if (strcmp(vocab[i].word, word2) == 0) {
            index2 = i;
        }
    }

    if (index1 == -1 || index2 == -1) {
        fprintf(stderr, "One or both words not found in the vocabulary.\n");
        free(vocab);
        return 1;
    }

    float vec1[] = {vocab[index1].embedding, vocab[index1].pe, vocab[index1].weight, vocab[index1].bias1, vocab[index1].bias2, vocab[index1].bias3, vocab[index1].bias4};
    float vec2[] = {vocab[index2].embedding, vocab[index2].pe, vocab[index2].weight, vocab[index2].bias1, vocab[index2].bias2, vocab[index2].bias3, vocab[index2].bias4};

    double similarity = cosine_similarity(vec1, vec2, 7);

    printf("Cosine similarity between '%s' and '%s': %f\n", word1, word2, similarity);

    // Save the result to a file
    FILE *outfile = fopen(similarity_path, "a");
    if (outfile) {
        fprintf(outfile, "Cosine similarity between '%s' and '%s': %f\n", word1, word2, similarity);
        fclose(outfile);
    }

    free(vocab);
    free(vocab_path_copy);

    return 0;
}