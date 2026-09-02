#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_VOCAB_SIZE 100000
#define EMBED_DIM 16
#define N_HEADS 4
#define SEQ_LEN 32
#define KV_DIM (EMBED_DIM / N_HEADS)

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

// Safe memory freeing macro to prevent double free errors
#define SAFE_FREE(ptr) do { if (ptr) { free(ptr); ptr = NULL; } } while(0)

// FUNCTION DECLARATIONS

int load_model_from_vocab_file(const char* vocab_file, void* model, SimpleVocab* vocab);
void rmsnorm(float *o, float *x, float *weight, int size);
void softmax(float *x, int size);
void matmul(float *xout, float *x, float *w, int n, int d);
void rope_rotate(float *vec, int vec_size, int pos, int head_size);
int process_attention_with_curriculum(const char* vocab_file, int* input_tokens, int num_input_tokens, float* output);

// MAIN FUNCTION
int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <command> [args...]\n", argv[0]);
        printf("Commands:\n");
        printf("  process <vocab_file> <input_token_ids> - Process attention using curriculum parameters\n");
        printf("  rmsnorm <size> - Apply RMS normalization\n"); 
        printf("  softmax <size> - Apply softmax function\n");
        printf("  load_params <vocab_file> - Load attention parameters from curriculum\n");
        return 1;
    }
    
    char* command = argv[1];
    
    if (strcmp(command, "process") == 0) {
        if (argc < 4) {
            printf("Error: Missing vocab_file and input tokens\n");
            printf("Usage: %s process <vocab_file> <token1,token2,...>\n", argv[0]);
            return 1;
        }
        
        char* vocab_file = argv[2];
        char* tokens_str = argv[3];
        
        printf("Processing attention: vocab_file=%s, tokens=%s\n", vocab_file, tokens_str);
        
        // Parse input tokens from comma-separated string
        int input_tokens[100]; // Max 100 tokens for this example
        int num_tokens = 0;
        
        char* token_copy = malloc(strlen(tokens_str) + 1);
        strcpy(token_copy, tokens_str);
        
        char* token = strtok(token_copy, ",");
        while (token != NULL && num_tokens < 100) {
            input_tokens[num_tokens] = atoi(token);
            num_tokens++;
            token = strtok(NULL, ",");
        }
        free(token_copy);
        
        // Process attention using curriculum parameters
        float* output = malloc(num_tokens * EMBED_DIM * sizeof(float));
        int result = process_attention_with_curriculum(vocab_file, input_tokens, num_tokens, output);
        
        if (result == 0) {
            printf("Attention processing completed successfully\n");
            // Print first few outputs as example
            printf("First output (first 5 dims): ");
            for (int i = 0; i < 5 && i < EMBED_DIM; i++) {
                printf("%.3f ", output[i]);
            }
            printf("...\n");
        }
        
        free(output);
        
    } else if (strcmp(command, "rmsnorm") == 0) {
        if (argc < 3) {
            printf("Error: Missing size\n");
            return 1;
        }
        
        int size = atoi(argv[2]);
        
        // Create sample vectors
        float* o = malloc(size * sizeof(float));
        float* x = malloc(size * sizeof(float));
        float* weight = malloc(size * sizeof(float));
        
        // Initialize with sample values
        for (int i = 0; i < size; i++) {
            x[i] = (float)(i % 10) * 0.1f - 0.5f;  // Values between -0.5 and 0.4
            weight[i] = 1.0f;  // Standard weight
        }
        
        printf("Input for RMSNorm (first 5): ");
        for (int i = 0; i < 5 && i < size; i++) {
            printf("%.3f ", x[i]);
        }
        printf("...\n");
        
        // Apply RMS normalization
        rmsnorm(o, x, weight, size);
        
        printf("Output from RMSNorm (first 5): ");
        for (int i = 0; i < 5 && i < size; i++) {
            printf("%.3f ", o[i]);
        }
        printf("...\n");
        
        free(o);
        free(x);
        free(weight);
        
    } else if (strcmp(command, "softmax") == 0) {
        if (argc < 3) {
            printf("Error: Missing size\n");
            return 1;
        }
        
        int size = atoi(argv[2]);
        
        // Create sample vector
        float* x = malloc(size * sizeof(float));
        
        // Initialize with sample values
        for (int i = 0; i < size; i++) {
            x[i] = (float)(i % 5) * 0.5f - 1.0f;  // Values between -1.0 and 1.0
        }
        
        printf("Input for Softmax (first 5): ");
        for (int i = 0; i < 5 && i < size; i++) {
            printf("%.3f ", x[i]);
        }
        printf("...\n");
        
        // Apply softmax
        softmax(x, size);
        
        printf("Output from Softmax (first 5): ");
        for (int i = 0; i < 5 && i < size; i++) {
            printf("%.3f ", x[i]);
        }
        printf("...\n");
        
        free(x);
        
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
            printf("Sample attention bias for token 0: %.6f\n", vocab.attention_bias[0]);
            printf("Sample Q projection for token 0, dim 0: %.6f\n", vocab.q_proj[0]);
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

void rmsnorm(float *o, float *x, float *weight, int size) {
    // Calculate sum of squares
    float ss = 0.0f;
    for (int j = 0; j < size; j++) {
        ss += x[j] * x[j];
    }
    ss /= size;
    ss += 1e-5f; // eps from karp model
    ss = 1.0f / sqrtf(ss);
    // normalize and scale
    for (int j = 0; j < size; j++) {
        o[j] = weight[j] * (ss * x[j]);
    }
}

void softmax(float *x, int size) {
    // Find max value (for numerical stability)
    float max_val = x[0];
    for (int i = 1; i < size; i++) {
        if (x[i] > max_val) {
            max_val = x[i];
        }
    }
    // Exp and sum
    float sum = 0.0f;
    for (int i = 0; i < size; i++) {
        x[i] = expf(x[i] - max_val);
        sum += x[i];
    }
    // Normalize
    for (int i = 0; i < size; i++) {
        x[i] /= sum;
    }
}

void matmul(float *xout, float *x, float *w, int n, int d) {
    // W (d,n) @ x (n,) -> xout (d,)
    // by far the most amount of time is spent inside this little function
    for (int i = 0; i < d; i++) {
        float val = 0.0f;
        for (int j = 0; j < n; j++) {
            val += w[i * n + j] * x[j];
        }
        xout[i] = val;
    }
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

// Function to process attention using parameters from curriculum
int process_attention_with_curriculum(const char* vocab_file, int* input_tokens, int num_input_tokens, float* output) {
    printf("Processing attention with curriculum: %s\n", vocab_file);
    
    // Load vocabulary parameters from curriculum file
    SimpleVocab vocab;
    int result = load_model_from_vocab_file(vocab_file, NULL, &vocab);
    if (result != 0) {
        printf("Error loading vocabulary from curriculum\n");
        return result;
    }
    
    // Process each input token using attention mechanisms with curriculum parameters
    for (int t = 0; t < num_input_tokens; t++) {
        int token_id = input_tokens[t];
        
        if (token_id < 0 || token_id >= vocab.vocab_size) {
            printf("Warning: Token ID %d out of range [0, %d)\n", token_id, vocab.vocab_size);
            continue;
        }
        
        // Get the embedding for this token
        float* token_embedding = &(vocab.embeddings[token_id * EMBED_DIM]);
        
        // For simplicity, we'll do a basic transformation using curriculum parameters
        // In a real implementation, this would involve full attention computation
        for (int d = 0; d < EMBED_DIM; d++) {
            // Apply attention bias from curriculum
            float val = token_embedding[d];
            val += vocab.attention_bias[token_id] * 0.1f;
            
            // Apply Q projection from curriculum
            val += vocab.q_proj[token_id * EMBED_DIM + d] * 0.05f;
            
            // Apply K projection from curriculum
            val += vocab.k_proj[token_id * EMBED_DIM + d] * 0.05f;
            
            // Apply V projection from curriculum
            val += vocab.v_proj[token_id * EMBED_DIM + d] * 0.05f;
            
            // Add positional encoding from curriculum
            val += vocab.rope_pos_enc[token_id * EMBED_DIM + d] * 0.1f;
            
            // Store in output
            output[t * EMBED_DIM + d] = val;
        }
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

// Function to load model from vocabulary file (same implementation as in generation_module.c)
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