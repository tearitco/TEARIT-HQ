#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <libgen.h>

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

// Attention Layer
typedef struct {
    float W_q[EMBEDDING_DIM][EMBEDDING_DIM];
    float W_k[EMBEDDING_DIM][EMBEDDING_DIM];
    float W_v[EMBEDDING_DIM][EMBEDDING_DIM];
} AttentionLayer;

// Function to initialize attention layer with random weights
void initialize_attention(AttentionLayer *attention) {
    for (int i = 0; i < EMBEDDING_DIM; i++) {
        for (int j = 0; j < EMBEDDING_DIM; j++) {
            attention->W_q[i][j] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
            attention->W_k[i][j] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
            attention->W_v[i][j] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
        }
    }
}

// Softmax function
void softmax(float *x, int size) {
    float max_val = x[0];
    for (int i = 1; i < size; i++) {
        if (x[i] > max_val) {
            max_val = x[i];
        }
    }
    float sum = 0.0f;
    for (int i = 0; i < size; i++) {
        x[i] = expf(x[i] - max_val);
        sum += x[i];
    }
    for (int i = 0; i < size; i++) {
        x[i] /= sum;
    }
}

// Apply causal mask to attention scores
void apply_causal_mask(float *attn_scores, int vocab_size, int current_position) {
    for (int i = current_position + 1; i < vocab_size; i++) {
        attn_scores[i] = -1e9f; // Set future positions to negative infinity
    }
}

// Forward propagation through attention mechanism
void attention_forward(float *input_vec, AttentionLayer *attention, struct VocabEntry *vocab, int vocab_size, 
                       float *attn_scores, float *context, int causal_attention, int current_position) {
    float q[EMBEDDING_DIM], k[EMBEDDING_DIM], v[EMBEDDING_DIM];
    
    // Compute Q, K, V
    for(int j=0; j<EMBEDDING_DIM; j++){
        q[j] = k[j] = v[j] = 0;
        for(int l=0; l<EMBEDDING_DIM; l++){
            q[j] += input_vec[l] * attention->W_q[l][j];
            k[j] += input_vec[l] * attention->W_k[l][j];
            v[j] += input_vec[l] * attention->W_v[l][j];
        }
    }

    // Compute attention scores
    for(int j=0; j<vocab_size; j++){
        float next_word_k[EMBEDDING_DIM] = {vocab[j].embedding, vocab[j].pe, vocab[j].weight, vocab[j].bias1, vocab[j].bias2, vocab[j].bias3, vocab[j].bias4};
        attn_scores[j] = 0;
        for(int l=0; l<EMBEDDING_DIM; l++){
            attn_scores[j] += q[l] * next_word_k[l];
        }
    }

    // Apply causal mask if enabled
    if (causal_attention) {
        apply_causal_mask(attn_scores, vocab_size, current_position);
    }

    softmax(attn_scores, vocab_size);

    // Compute context vector
    for(int l=0; l<EMBEDDING_DIM; l++) {
        context[l] = 0;
    }
    for(int j=0; j<vocab_size; j++){
        float next_word_v[EMBEDDING_DIM] = {vocab[j].embedding, vocab[j].pe, vocab[j].weight, vocab[j].bias1, vocab[j].bias2, vocab[j].bias3, vocab[j].bias4};
        for(int l=0; l<EMBEDDING_DIM; l++){
            context[l] += attn_scores[j] * next_word_v[l];
        }
    }
}

// Function to save attention to file
void save_attention(const char *filename, AttentionLayer *attention) {
    FILE *file = fopen(filename, "w");
    if (!file) {
        perror("Error opening attention file for writing");
        return;
    }
    
    for (int i = 0; i < EMBEDDING_DIM; i++) {
        for (int j = 0; j < EMBEDDING_DIM; j++) {
            fprintf(file, "%f ", attention->W_q[i][j]);
        }
        fprintf(file, "\n");
    }
    
    for (int i = 0; i < EMBEDDING_DIM; i++) {
        for (int j = 0; j < EMBEDDING_DIM; j++) {
            fprintf(file, "%f ", attention->W_k[i][j]);
        }
        fprintf(file, "\n");
    }
    
    for (int i = 0; i < EMBEDDING_DIM; i++) {
        for (int j = 0; j < EMBEDDING_DIM; j++) {
            fprintf(file, "%f ", attention->W_v[i][j]);
        }
        fprintf(file, "\n");
    }
    
    fclose(file);
}

// Function to load attention from file
void load_attention(const char *filename, AttentionLayer *attention) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        // Initialize with random weights if file doesn't exist
        initialize_attention(attention);
        return;
    }
    
    for (int i = 0; i < EMBEDDING_DIM; i++) {
        for (int j = 0; j < EMBEDDING_DIM; j++) {
            fscanf(file, "%f", &attention->W_q[i][j]);
        }
    }
    
    for (int i = 0; i < EMBEDDING_DIM; i++) {
        for (int j = 0; j < EMBEDDING_DIM; j++) {
            fscanf(file, "%f", &attention->W_k[i][j]);
        }
    }
    
    for (int i = 0; i < EMBEDDING_DIM; i++) {
        for (int j = 0; j < EMBEDDING_DIM; j++) {
            fscanf(file, "%f", &attention->W_v[i][j]);
        }
    }
    
    fclose(file);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <operation> [args...]\n", argv[0]);
        fprintf(stderr, "Operations:\n");
        fprintf(stderr, "  init <model_file>                  - Initialize attention and save to file\n");
        fprintf(stderr, "  forward <model_file> <vocab_file> <word_index>  - Forward pass using model and input\n");
        return 1;
    }
    
    char *operation = argv[1];
    AttentionLayer attention;
    
    if (strcmp(operation, "init") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s init <model_file>\n", argv[0]);
            return 1;
        }
        
        srand(time(NULL));
        initialize_attention(&attention);
        save_attention(argv[2], &attention);
        printf("Attention initialized and saved to %s\n", argv[2]);
        
    } else if (strcmp(operation, "forward") == 0) {
        if (argc < 5) {
            fprintf(stderr, "Usage: %s forward <model_file> <vocab_file> <word_index>\n", argv[0]);
            return 1;
        }
        
        load_attention(argv[2], &attention);
        
        // Path generation logic
        char *vocab_path = argv[3];
        char *vocab_path_copy = strdup(vocab_path);
        char *output_dir = dirname(vocab_path_copy);

        char attn_scores_path[1024];
        sprintf(attn_scores_path, "%s/attn_scores.txt", output_dir);
        // End of path generation logic

        // Load vocabulary
        FILE *vocab_file = fopen(vocab_path, "r");
        if (!vocab_file) {
            perror("Error opening vocab file");
            return 1;
        }
        
        struct VocabEntry *vocab = malloc(MAX_VOCAB_SIZE * sizeof(struct VocabEntry));
        if (!vocab) {
            perror("Failed to allocate memory for vocabulary");
            fclose(vocab_file);
            return 1;
        }
        
        char line[1024];
        int vocab_size = 0;
        
        // Skip header line
        fgets(line, sizeof(line), vocab_file);
        
        while (fgets(line, sizeof(line), vocab_file) && vocab_size < MAX_VOCAB_SIZE) {
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
        
        fclose(vocab_file);
        
        int word_index = atoi(argv[4]);
        if (word_index < 0 || word_index >= vocab_size) {
            fprintf(stderr, "Invalid word index\n");
            free(vocab);
            return 1;
        }
        
        // Prepare input
        float input_vec[EMBEDDING_DIM] = {vocab[word_index].embedding, vocab[word_index].pe, vocab[word_index].weight, 
                                          vocab[word_index].bias1, vocab[word_index].bias2, vocab[word_index].bias3, 
                                          vocab[word_index].bias4};
        
        // Forward pass
        float *attn_scores = malloc(vocab_size * sizeof(float));
        float context[EMBEDDING_DIM];
        // Read causal attention setting from config
        int causal_attention = 0;
        char config_path[1024];
        sprintf(config_path, "%s/config.txt", output_dir);
        FILE *config_file = fopen(config_path, "r");
        if (config_file) {
            char line[256];
            while (fgets(line, sizeof(line), config_file)) {
                if (strstr(line, "causal_attention=1")) {
                    causal_attention = 1;
                    break;
                }
            }
            fclose(config_file);
        }
        attention_forward(input_vec, &attention, vocab, vocab_size, attn_scores, context, causal_attention, word_index);
        
        // Save attention scores to file
        FILE *attn_file = fopen(attn_scores_path, "w");
        if (attn_file) {
            for (int i = 0; i < vocab_size; i++) {
                fprintf(attn_file, "%f\n", attn_scores[i]);
            }
            fclose(attn_file);
            printf("Forward pass completed. Attention scores saved to %s\n", attn_scores_path);
        } else {
            perror("Error opening attention scores file for writing");
        }
        
        free(vocab);
        free(attn_scores);
        free(vocab_path_copy);
        
    } else {
        fprintf(stderr, "Unknown operation: %s\n", operation);
        return 1;
    }
    
    return 0;
}
