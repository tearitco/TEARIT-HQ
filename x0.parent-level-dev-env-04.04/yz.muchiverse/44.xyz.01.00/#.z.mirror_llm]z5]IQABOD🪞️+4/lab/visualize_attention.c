#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_VOCAB_SIZE 100000
#define EMBED_DIM 16

// Data structure to hold the probed data for a single token
typedef struct {
    char* word;
    int id;
    float* final_embedding;
    float* query_vector;
    float* key_vector;
} ProbeData;

// A structure to hold all the probe data
typedef struct {
    ProbeData* data;
    int vocab_size;
    int vector_dim;
} ProbeDataset;

// Function to calculate cosine similarity between two vectors
float cosine_similarity(float* vecA, float* vecB, int dim) {
    float dot_product = 0.0f;
    float normA = 0.0f;
    float normB = 0.0f;
    for (int i = 0; i < dim; i++) {
        dot_product += vecA[i] * vecB[i];
        normA += vecA[i] * vecA[i];
        normB += vecB[i] * vecB[i];
    }
    if (normA == 0.0f || normB == 0.0f) return 0.0f;
    return dot_product / (sqrtf(normA) * sqrtf(normB));
}

// Function to load the probe_data.txt file
ProbeDataset* load_probe_dataset(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    if (!file) {
        fprintf(stderr, "Error: Cannot open probe data file: %s\n", filepath);
        return NULL;
    }

    // Count lines to determine vocab size
    int vocab_size = 0;
    char line[10000]; // Assume a max line length
    fgets(line, sizeof(line), file); // Skip header
    while (fgets(line, sizeof(line), file) != NULL) {
        vocab_size++;
    }
    rewind(file);
    fgets(line, sizeof(line), file); // Skip header again

    ProbeDataset* dataset = malloc(sizeof(ProbeDataset));
    dataset->vocab_size = vocab_size;
    dataset->vector_dim = EMBED_DIM; // Assuming fixed dimension
    dataset->data = malloc(vocab_size * sizeof(ProbeData));

    for (int i = 0; i < vocab_size; i++) {
        char word[100];
        int id;
        dataset->data[i].final_embedding = malloc(EMBED_DIM * sizeof(float));
        dataset->data[i].query_vector = malloc(EMBED_DIM * sizeof(float));
        dataset->data[i].key_vector = malloc(EMBED_DIM * sizeof(float));

        fscanf(file, "%s %d", word, &id);
        dataset->data[i].word = strdup(word);
        dataset->data[i].id = id;

        for (int d = 0; d < EMBED_DIM; d++) { fscanf(file, "%f", &dataset->data[i].final_embedding[d]); }
        for (int d = 0; d < EMBED_DIM; d++) { fscanf(file, "%f", &dataset->data[i].query_vector[d]); }
        for (int d = 0; d < EMBED_DIM; d++) { fscanf(file, "%f", &dataset->data[i].key_vector[d]); }
    }

    fclose(file);
    printf("Loaded %d tokens from %s\n", vocab_size, filepath);
    return dataset;
}

// Function to find a token's data by its string
ProbeData* find_word_data(ProbeDataset* dataset, const char* word) {
    for (int i = 0; i < dataset->vocab_size; i++) {
        if (strcmp(dataset->data[i].word, word) == 0) {
            return &dataset->data[i];
        }
    }
    return NULL;
}

void free_dataset(ProbeDataset* dataset) {
    if (!dataset) return;
    for (int i = 0; i < dataset->vocab_size; i++) {
        free(dataset->data[i].word);
        free(dataset->data[i].final_embedding);
        free(dataset->data[i].query_vector);
        free(dataset->data[i].key_vector);
    }
    free(dataset->data);
    free(dataset);
}

void command_similarity(ProbeDataset* dataset, char* word1, char* word2) {
    ProbeData* data1 = find_word_data(dataset, word1);
    ProbeData* data2 = find_word_data(dataset, word2);

    if (!data1 || !data2) {
        fprintf(stderr, "Error: One or both words not found in the dataset.\n");
        return;
    }

    float sim = cosine_similarity(data1->final_embedding, data2->final_embedding, dataset->vector_dim);
    printf("Cosine Similarity between '%s' and '%s': %f\n", word1, word2, sim);
}

void command_analogy(ProbeDataset* dataset, char* word1, char* word2, char* word3) {
    ProbeData* data1 = find_word_data(dataset, word1);
    ProbeData* data2 = find_word_data(dataset, word2);
    ProbeData* data3 = find_word_data(dataset, word3);

    if (!data1 || !data2 || !data3) {
        fprintf(stderr, "Error: One or more words not found in the dataset.\n");
        return;
    }

    float* result_vec = malloc(dataset->vector_dim * sizeof(float));
    for (int i = 0; i < dataset->vector_dim; i++) {
        result_vec[i] = data1->final_embedding[i] - data2->final_embedding[i] + data3->final_embedding[i];
    }

    int best_idx = -1;
    float best_sim = -2.0f; // Cosine similarity is in [-1, 1]

    for (int i = 0; i < dataset->vocab_size; i++) {
        // Exclude the input words from the search
        if (strcmp(dataset->data[i].word, word1) == 0 || strcmp(dataset->data[i].word, word2) == 0 || strcmp(dataset->data[i].word, word3) == 0) {
            continue;
        }
        float sim = cosine_similarity(result_vec, dataset->data[i].final_embedding, dataset->vector_dim);
        if (sim > best_sim) {
            best_sim = sim;
            best_idx = i;
        }
    }

    if (best_idx != -1) {
        printf("Analogy: %s - %s + %s = ~%s (Similarity: %f)\n", word1, word2, word3, dataset->data[best_idx].word, best_sim);
    } else {
        printf("Could not find a suitable analogy.\n");
    }

    free(result_vec);
}

void command_generate(ProbeDataset* dataset, char* start_word, int num_tokens) {
    ProbeData* current_word_data = find_word_data(dataset, start_word);
    if (!current_word_data) {
        fprintf(stderr, "Error: Start word '%s' not found.\n", start_word);
        return;
    }

    printf("Generated text: %s", start_word);

    float* scores = malloc(dataset->vocab_size * sizeof(float));

    for (int i = 0; i < num_tokens; i++) {
        float* q_current = current_word_data->query_vector;
        
        // Calculate pseudo-attention scores
        for (int j = 0; j < dataset->vocab_size; j++) {
            scores[j] = 0.0f;
            for(int d=0; d<dataset->vector_dim; d++) {
                scores[j] += q_current[d] * dataset->data[j].key_vector[d];
            }
        }

        // Softmax the scores
        float max_score = scores[0];
        for (int j = 1; j < dataset->vocab_size; j++) { if (scores[j] > max_score) max_score = scores[j]; }
        float sum_exp = 0.0f;
        for (int j = 0; j < dataset->vocab_size; j++) { scores[j] = expf(scores[j] - max_score); sum_exp += scores[j]; }
        for (int j = 0; j < dataset->vocab_size; j++) { scores[j] /= sum_exp; }

        // Sample next token (argmax for simplicity)
        int next_token_idx = 0;
        float max_prob = 0.0f;
        for (int j = 0; j < dataset->vocab_size; j++) {
            if (scores[j] > max_prob) {
                max_prob = scores[j];
                next_token_idx = j;
            }
        }
        
        current_word_data = &dataset->data[next_token_idx];
        printf(" %s", current_word_data->word);
    }
    printf("\n");
    free(scores);
}


int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <probe_data_file> <command> [args...]\n", argv[0]);
        fprintf(stderr, "Commands:\n");
        fprintf(stderr, "  similarity <word1> <word2>\n");
        fprintf(stderr, "  analogy <word1> <word2> <word3>\n");
        fprintf(stderr, "  generate <start_word> <num_tokens>\n");
        return 1;
    }

    ProbeDataset* dataset = load_probe_dataset(argv[1]);
    if (!dataset) {
        return 1;
    }

    char* command = argv[2];

    if (strcmp(command, "similarity") == 0 && argc == 5) {
        command_similarity(dataset, argv[3], argv[4]);
    } else if (strcmp(command, "analogy") == 0 && argc == 6) {
        command_analogy(dataset, argv[3], argv[4], argv[5]);
    } else if (strcmp(command, "generate") == 0 && argc == 5) {
        command_generate(dataset, argv[3], atoi(argv[4]));
    } else {
        fprintf(stderr, "Error: Invalid command or incorrect number of arguments for '%s'.\n", command);
    }

    free_dataset(dataset);
    return 0;
}
