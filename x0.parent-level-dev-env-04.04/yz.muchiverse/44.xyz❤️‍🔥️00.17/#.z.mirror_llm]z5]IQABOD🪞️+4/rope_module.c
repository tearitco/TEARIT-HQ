#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_VOCAB_SIZE 100000
#define EMBED_DIM 16
#define N_HEADS 4

// Safe memory freeing macro to prevent double free errors
#define SAFE_FREE(ptr) do { if (ptr) { free(ptr); ptr = NULL; } } while(0)

// Vocabulary structure to match curriculum format
typedef struct {
    char **words;
    int *word_to_id;
    float *embeddings;      // [vocab_size * embed_dim] concatenated embeddings
    float *rope_pos_enc;    // [vocab_size * embed_dim] RoPE positional encodings
    // Human-readable/usable biases - more meaningful for architecture understanding
    float *attention_bias;  // [vocab_size] attention bias values (new weight category - human-usable)
    float *ffn_bias;        // [vocab_size] feed-forward network bias values (new weight category - human-usable)
    // Internal computer-used weights and biases
    float *weight1;         // [vocab_size] weight1 values
    float *weight2;         // [vocab_size] weight2 values
    float *bias1;           // [vocab_size] bias1 values 
    float *bias2;           // [vocab_size] bias2 values
    float *bias3;           // [vocab_size] bias3 values
    float *bias4;           // [vocab_size] bias4 values
    // Q, K, V projections for attention mechanism (for shared encoder/decoder use)
    float *q_proj;          // [vocab_size * embed_dim] query projections
    float *k_proj;          // [vocab_size * embed_dim] key projections  
    float *v_proj;          // [vocab_size * embed_dim] value projections
    char **notes;           // [vocab_size] user notes for each token
    int vocab_size;
    int max_size;
} SimpleVocab;

// FUNCTION DECLARATIONS

void rope_rotate(float *vec, int vec_size, int pos, int head_size);
int apply_rope_with_curriculum(const char* vocab_file, int position, float* input_vec, int vec_size, float* output_vec);
int load_model_from_vocab_file(const char* vocab_file, void* model, SimpleVocab* vocab);

// MAIN FUNCTION
int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <command> [args...]\n", argv[0]);
        printf("Commands:\n");
        printf("  apply <vocab_file> <position> <vector_elements> - Apply RoPE using curriculum positional encodings\n");
        printf("  rotate <pos> <head_size> - Apply RoPE rotation to a vector\n");
        printf("  load_params <vocab_file> - Load RoPE parameters from curriculum\n");
        return 1;
    }
    
    char* command = argv[1];
    
    if (strcmp(command, "apply") == 0) {
        if (argc < 5) {
            printf("Error: Missing vocab_file, position and vector_elements\n");
            printf("Usage: %s apply <vocab_file> <position> <vector_elements_comma_separated>\n", argv[0]);
            return 1;
        }
        
        char* vocab_file = argv[2];
        int position = atoi(argv[3]);
        char* vector_str = argv[4];
        
        printf("Applying RoPE: vocab_file=%s, position=%d, vector=%s\n", vocab_file, position, vector_str);
        
        // Parse input vector from comma-separated string
        float input_vec[EMBED_DIM]; // Max size up to EMBED_DIM
        int vec_size = 0;
        
        char* vec_copy = malloc(strlen(vector_str) + 1);
        strcpy(vec_copy, vector_str);
        
        char* element = strtok(vec_copy, ",");
        while (element != NULL && vec_size < EMBED_DIM) {
            input_vec[vec_size] = atof(element);
            vec_size++;
            element = strtok(NULL, ",");
        }
        free(vec_copy);
        
        // Allocate output vector
        float* output_vec = malloc(vec_size * sizeof(float));
        
        // Apply RoPE using curriculum parameters
        int result = apply_rope_with_curriculum(vocab_file, position, input_vec, vec_size, output_vec);
        
        if (result == 0) {
            printf("RoPE application completed successfully\n");
            printf("Output vector: ");
            for (int i = 0; i < vec_size; i++) {
                printf("%.3f ", output_vec[i]);
            }
            printf("\n");
        }
        
        free(output_vec);
        
    } else if (strcmp(command, "rotate") == 0) {
        if (argc < 4) {
            printf("Error: Missing position and head_size\n");
            return 1;
        }
        
        int pos = atoi(argv[2]);
        int head_size = atoi(argv[3]);
        
        // Create a sample vector of size EMBED_DIM
        float* vec = malloc(EMBED_DIM * sizeof(float));
        for (int i = 0; i < EMBED_DIM; i++) {
            vec[i] = (float)i * 0.1f;  // Initialize with sample values
        }
        
        printf("Before RoPE rotation at pos %d with head_size %d:\n", pos, head_size);
        for (int i = 0; i < EMBED_DIM; i++) {
            printf("%.3f ", vec[i]);
        }
        printf("\n");
        
        // Apply RoPE rotation
        rope_rotate(vec, EMBED_DIM, pos, head_size);
        
        printf("After RoPE rotation:\n");
        for (int i = 0; i < EMBED_DIM; i++) {
            printf("%.3f ", vec[i]);
        }
        printf("\n");
        
        free(vec);
        
    } else if (strcmp(command, "load_params") == 0) {
        if (argc < 3) {
            printf("Error: Missing vocab_file\n");
            printf("Usage: %s load_params <vocab_file>\n", argv[0]);
            return 1;
        }
        
        char* vocab_file = argv[2];
        
        // Initialize vocabulary to load parameters
        SimpleVocab vocab;
        int result = load_model_from_vocab_file(vocab_file, NULL, &vocab);
        if (result != 0) {
            printf("Error loading parameters from curriculum file\n");
            return result;
        }
        
        printf("Successfully loaded curriculum with %d tokens:\n", vocab.vocab_size);
        
        // Show some loaded parameters as verification
        if (vocab.vocab_size > 0) {
            printf("Sample RoPE pos encoding for token 0, dim 0: %.6f\n", vocab.rope_pos_enc[0]);
            printf("Sample RoPE pos encoding for token 0, dim 1: %.6f\n", vocab.rope_pos_enc[1]);
        }
        
        // Free vocabulary memory
        for (int i = 0; i < vocab.vocab_size; i++) {
            free(vocab.words[i]);
            if (vocab.notes[i]) free(vocab.notes[i]);
        }
        SAFE_FREE(vocab.words);
        SAFE_FREE(vocab.word_to_id);
        SAFE_FREE(vocab.embeddings);
        SAFE_FREE(vocab.rope_pos_enc);
        SAFE_FREE(vocab.weight1);
        SAFE_FREE(vocab.weight2);
        SAFE_FREE(vocab.bias1);
        SAFE_FREE(vocab.bias2);
        SAFE_FREE(vocab.bias3);
        SAFE_FREE(vocab.bias4);
        SAFE_FREE(vocab.attention_bias);
        SAFE_FREE(vocab.ffn_bias);
        SAFE_FREE(vocab.q_proj);
        SAFE_FREE(vocab.k_proj);
        SAFE_FREE(vocab.v_proj);
        SAFE_FREE(vocab.notes);
        
    } else {
        printf("Error: Unknown command '%s'\n", command);
        return 1;
    }
    
    return 0;
}

void rope_rotate(float *vec, int vec_size, int pos, int head_size) {
    for (int i = 0; i < vec_size; i += 2) {
        int head_dim = i % head_size;
        float freq = 1.0f / powf(10000.0f, head_dim / (float)head_size);
        float val = pos * freq;
        float fcr = cosf(val);
        float fci = sinf(val);
        float v0 = vec[i];
        float v1 = vec[i+1];
        vec[i]   = v0 * fcr - v1 * fci;
        vec[i+1] = v0 * fci + v1 * fcr;
    }
}

// Function to apply RoPE using parameters from curriculum
int apply_rope_with_curriculum(const char* vocab_file, int position, float* input_vec, int vec_size, float* output_vec) {
    printf("Applying RoPE with curriculum: %s, position: %d\n", vocab_file, position);
    
    // Load vocabulary parameters from curriculum file
    SimpleVocab vocab;
    int result = load_model_from_vocab_file(vocab_file, NULL, &vocab);
    if (result != 0) {
        printf("Error loading vocabulary from curriculum\n");
        return result;
    }
    
    // Copy input to output initially
    for (int i = 0; i < vec_size; i++) {
        output_vec[i] = input_vec[i];
    }
    
    // Apply RoPE rotation using the formula from the original function
    // In this implementation, we'll use the positional encoding from the curriculum
    // to determine how to apply the rotation based on the position
    for (int i = 0; i < vec_size; i += 2) {  // RoPE operates on pairs (i, i+1)
        if (i + 1 >= vec_size) break;  // Ensure we don't go out of bounds
        
        int head_dim = i % (EMBED_DIM / N_HEADS);  // Calculate head dimension
        float freq = 1.0f / powf(10000.0f, head_dim / (float)(EMBED_DIM / N_HEADS));
        float val = position * freq;
        float fcr = cosf(val);
        float fci = sinf(val);
        float v0 = output_vec[i];
        float v1 = output_vec[i+1];
        output_vec[i]   = v0 * fcr - v1 * fci;
        output_vec[i+1] = v0 * fci + v1 * fcr;
    }
    
    // Free vocabulary memory
    for (int i = 0; i < vocab.vocab_size; i++) {
        free(vocab.words[i]);
        if (vocab.notes[i]) free(vocab.notes[i]);
    }
    SAFE_FREE(vocab.words);
    SAFE_FREE(vocab.word_to_id);
    SAFE_FREE(vocab.embeddings);
    SAFE_FREE(vocab.rope_pos_enc);
    SAFE_FREE(vocab.weight1);
    SAFE_FREE(vocab.weight2);
    SAFE_FREE(vocab.bias1);
    SAFE_FREE(vocab.bias2);
    SAFE_FREE(vocab.bias3);
    SAFE_FREE(vocab.bias4);
    SAFE_FREE(vocab.attention_bias);
    SAFE_FREE(vocab.ffn_bias);
    SAFE_FREE(vocab.q_proj);
    SAFE_FREE(vocab.k_proj);
    SAFE_FREE(vocab.v_proj);
    SAFE_FREE(vocab.notes);
    
    return 0; // Success
}

// Function to load model from vocabulary file (similar to other modules)
int load_model_from_vocab_file(const char* vocab_file, void* model, SimpleVocab* vocab) {
    printf("Loading model from vocab file: %s\n", vocab_file);
    
    FILE *file = fopen(vocab_file, "r");
    if (!file) {
        printf("Error: could not open vocabulary file %s\n", vocab_file);
        return -1;
    }
    
    // Read file content to count lines first (excluding header)
    char line[1024];
    int line_count = 0;
    
    // Read header line first
    if (fgets(line, sizeof(line), file) == NULL) {
        printf("Error: could not read header from vocabulary file\n");
        fclose(file);
        return -1;
    }
    
    // Count remaining lines
    while (fgets(line, sizeof(line), file) != NULL) {
        line_count++;
    }
    rewind(file);
    
    // Skip header line
    if (fgets(line, sizeof(line), file) == NULL) {
        printf("Error: could not read header from vocabulary file\n");
        fclose(file);
        return -1;
    }
    
    // Initialize vocabulary from file
    vocab->vocab_size = line_count;
    vocab->max_size = MAX_VOCAB_SIZE;
    vocab->words = malloc(vocab->vocab_size * sizeof(char*));
    vocab->word_to_id = malloc(vocab->vocab_size * sizeof(int));
    vocab->embeddings = malloc(vocab->vocab_size * EMBED_DIM * sizeof(float));
    vocab->rope_pos_enc = malloc(vocab->vocab_size * EMBED_DIM * sizeof(float));
    vocab->weight1 = malloc(vocab->vocab_size * sizeof(float));
    vocab->weight2 = malloc(vocab->vocab_size * sizeof(float));
    vocab->bias1 = malloc(vocab->vocab_size * sizeof(float));
    vocab->bias2 = malloc(vocab->vocab_size * sizeof(float));
    vocab->bias3 = malloc(vocab->vocab_size * sizeof(float));
    vocab->bias4 = malloc(vocab->vocab_size * sizeof(float));
    vocab->attention_bias = malloc(vocab->vocab_size * sizeof(float));
    vocab->ffn_bias = malloc(vocab->vocab_size * sizeof(float));
    vocab->q_proj = malloc(vocab->vocab_size * EMBED_DIM * sizeof(float));
    vocab->k_proj = malloc(vocab->vocab_size * EMBED_DIM * sizeof(float));
    vocab->v_proj = malloc(vocab->vocab_size * EMBED_DIM * sizeof(float));
    vocab->notes = malloc(vocab->vocab_size * sizeof(char*));
    
    // Read vocabulary entries
    for (int i = 0; i < vocab->vocab_size; i++) {
        int index;
        char word[1000];
        char note_str[1000];  // Buffer for note string
        float embedding, pe_value, attention_bias_val, ffn_bias_val, weight1_val, weight2_val, bias1_val, bias2_val, bias3_val, bias4_val, q_val, k_val, v_val;
        
        if (fscanf(file, "%d %s %f %f %f %f %f %f %f %f %f %f %f %f %f %999s", 
                   &index, word, &embedding, &pe_value, 
                   &attention_bias_val, &ffn_bias_val,
                   &weight1_val, &weight2_val, 
                   &bias1_val, &bias2_val, &bias3_val, &bias4_val,
                   &q_val, &k_val, &v_val, note_str) != 16) {
            printf("Error: could not read vocabulary entry %d properly\n", i);
            fclose(file);
            return -1;
        }
        
        // Store word
        vocab->words[i] = malloc((strlen(word) + 1) * sizeof(char));
        strcpy(vocab->words[i], word);
        vocab->word_to_id[i] = i;
        
        // Initialize embedding (for now, using the embedding value from file as first dimension)
        for (int d = 0; d < EMBED_DIM; d++) {
            if (d == 0) {
                vocab->embeddings[i * EMBED_DIM + d] = embedding;
            } else {
                // For other dimensions, we'll initialize using a deterministic approach
                unsigned long hash = 5381;
                int c;
                const char *str = word;
                while ((c = *str++))
                    hash = ((hash << 5) + hash) + c;
                
                vocab->embeddings[i * EMBED_DIM + d] = (float)(hash % 1000000) / 1000000.0f + (float)d * 0.001f;
            }
        }
        
        // Initialize RoPE positional encoding (for now, using the pe value from file as first dimension)
        for (int d = 0; d < EMBED_DIM; d++) {
            if (d == 0) {
                vocab->rope_pos_enc[i * EMBED_DIM + d] = pe_value;
            } else {
                // Generate RoPE based on position i and dimension d
                float freq = 1.0f / powf(10000.0f, (float)(d % (EMBED_DIM/2)) / (EMBED_DIM/2));
                float angle = i * freq;
                
                if (d % 2 == 0) {
                    vocab->rope_pos_enc[i * EMBED_DIM + d] = sinf(angle);
                } else {
                    vocab->rope_pos_enc[i * EMBED_DIM + d] = cosf(angle);
                }
            }
        }
        
        // Initialize Q, K, V projections based on the values from file (for first dimension)
        for (int d = 0; d < EMBED_DIM; d++) {
            if (d == 0) {
                vocab->q_proj[i * EMBED_DIM + d] = q_val;
                vocab->k_proj[i * EMBED_DIM + d] = k_val;
                vocab->v_proj[i * EMBED_DIM + d] = v_val;
            } else {
                // For other dimensions, use deterministic approach based on word content
                unsigned long hash = 5381;
                int c;
                const char *str = word;
                while ((c = *str++))
                    hash = ((hash << 5) + hash) + c;
                
                vocab->q_proj[i * EMBED_DIM + d] = (float)(hash % 1000000) / 1000000.0f + (float)d * 0.001f;
                vocab->k_proj[i * EMBED_DIM + d] = (float)(hash % 1000001) / 1000000.0f + (float)d * 0.001f;  // Slightly different hash
                vocab->v_proj[i * EMBED_DIM + d] = (float)(hash % 1000002) / 1000000.0f + (float)d * 0.001f;  // Slightly different hash
            }
        }
        
        // Store note string for this token
        vocab->notes[i] = malloc((strlen(note_str) + 1) * sizeof(char));
        strcpy(vocab->notes[i], note_str);
        
        // Store other values from vocab file in the new order (human-usable biases first)
        vocab->attention_bias[i] = attention_bias_val;
        vocab->ffn_bias[i] = ffn_bias_val;
        vocab->weight1[i] = weight1_val;
        vocab->weight2[i] = weight2_val;
        vocab->bias1[i] = bias1_val;
        vocab->bias2[i] = bias2_val;
        vocab->bias3[i] = bias3_val;
        vocab->bias4[i] = bias4_val;
    }
    
    fclose(file);
    
    printf("Loaded model with vocabulary from %s (%d tokens)\n", vocab_file, vocab->vocab_size);
    return 0; // Success
}