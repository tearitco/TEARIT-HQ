#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define MAX_VOCAB_SIZE 100000
#define EMBED_DIM 256
#define CONTEXT_SIZE 8  // Number of tokens to use as context
#define FF_HIDDEN (EMBED_DIM * 4)
#define HEAD_DIM (EMBED_DIM / 4)  // Assuming 4 attention heads
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

// Context window structure for attention computation
typedef struct {
    float *token_embeddings; // [CONTEXT_SIZE * EMBED_DIM]
    float *q_values;         // [CONTEXT_SIZE * EMBED_DIM] 
    float *k_values;         // [CONTEXT_SIZE * EMBED_DIM]
    float *v_values;         // [CONTEXT_SIZE * EMBED_DIM]
    float *attn_scores;      // [CONTEXT_SIZE * CONTEXT_SIZE] attention scores
    float *attn_weights;     // [CONTEXT_SIZE * CONTEXT_SIZE] attention weights
    float *attn_output;      // [CONTEXT_SIZE * EMBED_DIM] output after attention
    float *ffn_intermediate; // [CONTEXT_SIZE * FF_HIDDEN] intermediate in FFN
    float *ffn_output;       // [CONTEXT_SIZE * EMBED_DIM] output after FFN
    int *token_ids;          // [CONTEXT_SIZE] token IDs in context
} ContextWindow;

// FUNCTION DECLARATIONS

void rms_norm(float *output, float *input, float *weight, int size);
void matmul(float *xout, float *x, float *w, int n, int d);
void softmax(float *x, int size);
void rope_embed(float *vec, int vec_size, int pos);
float cross_entropy_loss(float* predicted_probs, int actual_token, int vocab_size);
void compute_query_key_value(ContextWindow* ctx, SimpleVocab* vocab, float learning_rate);
void apply_attention(ContextWindow* ctx);
void apply_ffn(ContextWindow* ctx, SimpleVocab* vocab, float learning_rate);
void update_parameters(ContextWindow* ctx, SimpleVocab* vocab, int target_token, float learning_rate);
int process_feedback_tx_with_curriculum(const char* vocab_file, int* token_sequence, int seq_len, float learning_rate);

// MAIN FUNCTION
int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <command> [args...]\n", argv[0]);
        printf("Commands:\n");
        printf("  process <vocab_file> <token_sequence_comma_separated> <learning_rate> - Process TX feedback learning\n");
        printf("  cross_entropy <vocab_size> <actual_token> - Compute cross-entropy loss\n");
        return 1;
    }
    
    char* command = argv[1];
    
    if (strcmp(command, "process") == 0) {
        if (argc < 4) {
            printf("Error: Missing vocab_file, token_sequence, and learning_rate\n");
            printf("Usage: %s process <vocab_file> <token_sequence_comma_separated> <learning_rate>\n", argv[0]);
            return 1;
        }
        
        char* vocab_file = argv[2];
        char* tokens_str = argv[3];
        float learning_rate = atof(argv[4]);
        
        printf("Processing TX feedback learning: vocab_file=%s, lr=%.6f\n", vocab_file, learning_rate);
        printf("Token sequence: %s\n", tokens_str);
        
        // Parse token sequence from comma-separated string
        int token_sequence[100]; // Max 100 tokens for this example
        int seq_len = 0;
        
        char* token_copy = malloc(strlen(tokens_str) + 1);
        strcpy(token_copy, tokens_str);
        
        char* token = strtok(token_copy, ",");
        while (token != NULL && seq_len < 100) {
            token_sequence[seq_len] = atoi(token);
            seq_len++;
            token = strtok(NULL, ",");
        }
        free(token_copy);
        
        // If sequence is too short for context size, pad it by repeating
        if (seq_len < CONTEXT_SIZE + 1) {
            printf("Warning: Sequence too short (%d) for context size %d. Padding with first token...\n", seq_len, CONTEXT_SIZE);
            if (seq_len > 0) {
                int first_token = token_sequence[0];
                while (seq_len < CONTEXT_SIZE + 1) {
                    token_sequence[seq_len] = first_token;
                    seq_len++;
                }
            } else {
                // If no tokens provided, use the first token in vocab
                printf("Error: No tokens provided in sequence\n");
                return -1;
            }
        }
        
        // Process TX feedback learning using curriculum parameters
        int result = process_feedback_tx_with_curriculum(vocab_file, token_sequence, seq_len, learning_rate);
        
        if (result == 0) {
            printf("TX feedback learning completed successfully\n");
        } else {
            printf("TX feedback learning failed with code %d\n", result);
        }
        
    } else if (strcmp(command, "process_with_vocab") == 0) {
        // NEW: Process using the full vocabulary file as sequence
        if (argc < 4) {
            printf("Error: Missing vocab_file and learning_rate\n");
            printf("Usage: %s process_with_vocab <vocab_file> <learning_rate>\n", argv[0]);
            return 1;
        }
        
        char* vocab_file = argv[2];
        float learning_rate = atof(argv[3]);
        
        printf("Processing TX feedback learning with full vocabulary as sequence: %s, lr=%.6f\n", vocab_file, learning_rate);
        
        // Load the vocabulary file to get token sequence from the curriculum contents
        int token_sequence[200]; // Larger buffer for reading from vocab file
        int seq_len = 0;
        
        FILE *file = fopen(vocab_file, "r");
        if (!file) {
            printf("Error: couldn't open vocabulary file %s\n", vocab_file);
            return -1;
        }
        
        char line[1024];
        int index;
        char word[1000];
        float embedding, pe_value, attention_bias_val, ffn_bias_val, weight1_val, weight2_val, bias1_val, bias2_val, bias3_val, bias4_val, q_val, k_val, v_val;
        char note_str[1000];
        
        // Skip header line
        if (fgets(line, sizeof(line), file) == NULL) {
            printf("Error: could not read header from vocabulary file\n");
            fclose(file);
            return -1;
        }
        
        // Read tokens from vocabulary file
        while (fgets(line, sizeof(line), file) != NULL && seq_len < 200) {
            if (sscanf(line, "%d %s %f %f %f %f %f %f %f %f %f %f %f %f %f %s", 
                       &index, word, &embedding, &pe_value, 
                       &attention_bias_val, &ffn_bias_val,
                       &weight1_val, &weight2_val, 
                       &bias1_val, &bias2_val, &bias3_val, &bias4_val,
                       &q_val, &k_val, &v_val, note_str) == 16) {
                
                token_sequence[seq_len] = index - 1; // Convert to 0-indexed
                seq_len++;
            }
        }
        fclose(file);
        
        if (seq_len < CONTEXT_SIZE + 1) {
            printf("Error: Vocabulary file has too few entries (%d) for context size %d\n", seq_len, CONTEXT_SIZE);
            return -1;
        }
        
        // Process TX feedback learning using the full sequence from vocabulary
        int result = process_feedback_tx_with_curriculum(vocab_file, token_sequence, seq_len, learning_rate);
        
        if (result == 0) {
            printf("TX feedback learning completed successfully using vocabulary sequence\n");
        } else {
            printf("TX feedback learning failed with code %d\n", result);
        }
        
    } else if (strcmp(command, "cross_entropy") == 0) {
        if (argc < 4) {
            printf("Error: Missing vocab_size and actual_token\n");
            return 1;
        }
        
        int vocab_size = atoi(argv[2]);
        int actual_token = atoi(argv[3]);
        
        // Create sample probabilities (should sum to 1)
        float* probs = malloc(vocab_size * sizeof(float));
        
        // Initialize with sample values and normalize to form probabilities
        float sum = 0.0f;
        for (int i = 0; i < vocab_size; i++) {
            probs[i] = (float)(i % 100) / 1000.0f + 0.001f; // Ensure positive
            sum += probs[i];
        }
        // Normalize
        for (int i = 0; i < vocab_size; i++) {
            probs[i] /= sum;
        }
        
        printf("Computing cross-entropy loss for vocab_size=%d, actual_token=%d\n", vocab_size, actual_token);
        
        // Compute cross-entropy loss
        float loss = cross_entropy_loss(probs, actual_token, vocab_size);
        
        printf("Cross-entropy loss: %.6f\n", loss);
        
        free(probs);
        
    } else {
        printf("Error: Unknown command '%s'\n", command);
        return 1;
    }
    
    return 0;
}

void rms_norm(float *output, float *input, float *weight, int size) {
    // Calculate sum of squares
    float ss = 0.0f;
    for (int j = 0; j < size; j++) {
        ss += input[j] * input[j];
    }
    ss /= size;
    ss += 1e-5f; // eps
    ss = 1.0f / sqrtf(ss);
    // normalize and scale
    for (int j = 0; j < size; j++) {
        output[j] = weight[j] * (ss * input[j]);
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

void rope_embed(float *vec, int vec_size, int pos) {
    // Apply rotary positional encoding (RoPE)
    for (int i = 0; i < vec_size; i += 2) {
        int head_dim = i % HEAD_DIM;
        float freq = 1.0f / powf(10000.0f, head_dim / (float)HEAD_DIM);
        float val = pos * freq;
        float fcr = cosf(val);
        float fci = sinf(val);
        float v0 = vec[i];
        float v1 = vec[i+1];
        vec[i]   = v0 * fcr - v1 * fci;
        vec[i+1] = v0 * fci + v1 * fcr;
    }
}

float cross_entropy_loss(float* predicted_probs, int actual_token, int vocab_size) {
    // Ensure probability is not zero to avoid log(0)
    float prob = predicted_probs[actual_token];
    if (prob < 1e-8f) prob = 1e-8f;
    
    // Calculate negative log likelihood
    return -logf(prob);
}

// Function to compute query, key, and value for each token in context
void compute_query_key_value(ContextWindow* ctx, SimpleVocab* vocab, float learning_rate) {
    for (int pos = 0; pos < CONTEXT_SIZE; pos++) {
        if (ctx->token_ids[pos] < 0 || ctx->token_ids[pos] >= vocab->vocab_size) continue;
        
        int token_id = ctx->token_ids[pos];
        float* token_emb = &(ctx->token_embeddings[pos * EMBED_DIM]);
        
        // Compute Query for this position
        for (int d = 0; d < EMBED_DIM; d++) {
            ctx->q_values[pos * EMBED_DIM + d] = token_emb[d] * vocab->q_proj[token_id * EMBED_DIM + d % EMBED_DIM];
        }
        
        // Compute Key for this position (using different projection)
        for (int d = 0; d < EMBED_DIM; d++) {
            ctx->k_values[pos * EMBED_DIM + d] = token_emb[d] * vocab->k_proj[token_id * EMBED_DIM + d % EMBED_DIM];
        }
        
        // Compute Value for this position
        for (int d = 0; d < EMBED_DIM; d++) {
            ctx->v_values[pos * EMBED_DIM + d] = token_emb[d] * vocab->v_proj[token_id * EMBED_DIM + d % EMBED_DIM];
        }
        
        // Apply RoPE to Q and K for positional encoding
        rope_embed(&(ctx->q_values[pos * EMBED_DIM]), EMBED_DIM, pos);
        rope_embed(&(ctx->k_values[pos * EMBED_DIM]), EMBED_DIM, pos);
    }
}

// Function to apply attention mechanism: Q @ K^T, then softmax, then @ V
void apply_attention(ContextWindow* ctx) {
    // Compute attention scores: Q @ K^T
    for (int i = 0; i < CONTEXT_SIZE; i++) {
        for (int j = 0; j <= i; j++) {  // Only causal attention (i >= j)
            float score = 0.0f;
            for (int d = 0; d < EMBED_DIM; d++) {
                score += ctx->q_values[i * EMBED_DIM + d] * ctx->k_values[j * EMBED_DIM + d];
            }
            // Scale by square root of dimension for stability
            ctx->attn_scores[i * CONTEXT_SIZE + j] = score / sqrtf(EMBED_DIM);
        }
        // Mask future positions for causal attention
        for (int j = i + 1; j < CONTEXT_SIZE; j++) {
            ctx->attn_scores[i * CONTEXT_SIZE + j] = -1e9f; // Large negative value
        }
    }
    
    // Apply softmax to each row to get attention weights
    for (int i = 0; i < CONTEXT_SIZE; i++) {
        // Apply softmax to get attention weights for this position
        for (int j = 0; j < CONTEXT_SIZE; j++) {
            ctx->attn_weights[i * CONTEXT_SIZE + j] = ctx->attn_scores[i * CONTEXT_SIZE + j];
        }
        softmax(&(ctx->attn_weights[i * CONTEXT_SIZE]), CONTEXT_SIZE);
    }
    
    // Apply attention weights to values: weights @ V
    for (int i = 0; i < CONTEXT_SIZE; i++) {
        for (int d = 0; d < EMBED_DIM; d++) {
            float sum = 0.0f;
            for (int j = 0; j < CONTEXT_SIZE; j++) {
                sum += ctx->attn_weights[i * CONTEXT_SIZE + j] * ctx->v_values[j * EMBED_DIM + d];
            }
            ctx->attn_output[i * EMBED_DIM + d] = sum;
        }
    }
}

// Function to apply feedforward network
void apply_ffn(ContextWindow* ctx, SimpleVocab* vocab, float learning_rate) {
    for (int pos = 0; pos < CONTEXT_SIZE; pos++) {
        if (ctx->token_ids[pos] < 0 || ctx->token_ids[pos] >= vocab->vocab_size) continue;
        
        int token_id = ctx->token_ids[pos];
        
        // First layer: transform from EMBED_DIM to FF_HIDDEN
        for (int h = 0; h < FF_HIDDEN; h++) {
            float val = 0.0f;
            for (int d = 0; d < EMBED_DIM; d++) {
                // Use position embedding for the first layer
                val += ctx->attn_output[pos * EMBED_DIM + d];
            }
            // Apply learned transformation (simplified - using vocabulary parameters)
            val = val * vocab->weight1[token_id] + vocab->bias1[token_id];
            ctx->ffn_intermediate[pos * FF_HIDDEN + h] = val;
        }
        
        // Apply activation function (silu/gelu approximation)
        for (int h = 0; h < FF_HIDDEN; h++) {
            float x = ctx->ffn_intermediate[pos * FF_HIDDEN + h];
            ctx->ffn_intermediate[pos * FF_HIDDEN + h] = x * (1.0f / (1.0f + expf(-x))); // silu(x) = x * sigmoid(x)
        }
        
        // Second layer: back to EMBED_DIM
        for (int d = 0; d < EMBED_DIM; d++) {
            float val = 0.0f;
            for (int h = 0; h < FF_HIDDEN; h++) {
                val += ctx->ffn_intermediate[pos * FF_HIDDEN + h];
            }
            // Apply learned transformation
            val = val * vocab->weight2[token_id] + vocab->bias2[token_id];
            ctx->ffn_output[pos * EMBED_DIM + d] = val;
        }
    }
}

// Function to update parameters based on prediction error
void update_parameters(ContextWindow* ctx, SimpleVocab* vocab, int target_token, float learning_rate) {
    // Calculate gradients based on how well the prediction matched the target
    for (int pos = 0; pos < CONTEXT_SIZE; pos++) {
        if (ctx->token_ids[pos] < 0 || ctx->token_ids[pos] >= vocab->vocab_size) continue;
        
        int token_id = ctx->token_ids[pos];
        
        // Simple gradient calculation based on prediction error
        float grad_magnitude = 0.0f;
        
        // Calculate how much the output differs from expectations
        // This is a simplified gradient calculation
        for (int d = 0; d < EMBED_DIM; d++) {
            float output_val = ctx->ffn_output[pos * EMBED_DIM + d];
            float target_val = vocab->embeddings[target_token * EMBED_DIM + d];
            float diff = output_val - target_val;
            grad_magnitude += diff * diff;
        }
        grad_magnitude = sqrtf(grad_magnitude / EMBED_DIM) * learning_rate;
        
        // Update attention-related parameters
        vocab->attention_bias[token_id] -= grad_magnitude * 0.01f;
        vocab->ffn_bias[token_id] -= grad_magnitude * 0.01f;
        
        // Update weights with gradients
        vocab->weight1[token_id] -= grad_magnitude * 0.005f;
        vocab->weight2[token_id] -= grad_magnitude * 0.005f;
        vocab->bias1[token_id] -= grad_magnitude * 0.002f;
        vocab->bias2[token_id] -= grad_magnitude * 0.002f;
        
        // Update QKV projections
        for (int d = 0; d < EMBED_DIM; d++) {
            vocab->q_proj[token_id * EMBED_DIM + d] -= grad_magnitude * 0.001f;
            vocab->k_proj[token_id * EMBED_DIM + d] -= grad_magnitude * 0.001f;
            vocab->v_proj[token_id * EMBED_DIM + d] -= grad_magnitude * 0.001f;
        }
    }
}

// Function to load model from vocabulary file
int load_model_from_vocab_file(const char* vocab_file, SimpleVocab* vocab) {
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
    
    return 0; // Success
}

// Function to save updated vocabulary back to file
int save_model_to_vocab_file(const char* vocab_file, SimpleVocab* vocab) {
    printf("Saving updated model to vocab file: %s\n", vocab_file);
    
    // Create a temporary file for updates
    char temp_file[2048];
    snprintf(temp_file, sizeof(temp_file), "%s.tmp", vocab_file);
    
    FILE *file = fopen(temp_file, "w");
    if (!file) {
        printf("Error: could not create temporary vocabulary file %s\n", temp_file);
        return -1;
    }
    
    // Write header
    fprintf(file, "number word embedding pe attention_bias ffn_bias weight1 weight2 bias1 bias2 bias3 bias4 q_proj k_proj v_proj note\n");
    
    // Write each token and its parameters
    for (int i = 0; i < vocab->vocab_size; i++) {
        // Write the first dimension values for embedding and PE (for consistency with examples)
        float emb_val = vocab->embeddings[i * EMBED_DIM + 0];
        float pe_val = vocab->rope_pos_enc[i * EMBED_DIM + 0];
        // Use first dimension of Q, K, V projections
        float q_val = vocab->q_proj[i * EMBED_DIM + 0];
        float k_val = vocab->k_proj[i * EMBED_DIM + 0];
        float v_val = vocab->v_proj[i * EMBED_DIM + 0];
        // Get note for this token (or empty string if none)
        const char* note = vocab->notes[i] ? vocab->notes[i] : "";
        
        fprintf(file, "%d %s %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %s\n",
                i+1, vocab->words[i], emb_val, pe_val, 
                vocab->attention_bias[i], vocab->ffn_bias[i],
                vocab->weight1[i], vocab->weight2[i],
                vocab->bias1[i], vocab->bias2[i], vocab->bias3[i], vocab->bias4[i],
                q_val, k_val, v_val, note);
    }
    
    fclose(file);
    
    // Replace original file with updated one
    if (rename(temp_file, vocab_file) != 0) {
        printf("Error: could not replace original vocabulary file with updated version\n");
        return -1;
    }
    
    printf("Successfully updated vocabulary file with new parameters\n");
    return 0; // Success
}

// Function to process TX (Transformer) feedback learning using parameters from curriculum
int process_feedback_tx_with_curriculum(const char* vocab_file, int* token_sequence, int seq_len, float learning_rate) {
    printf("Processing TX feedback learning with curriculum: %s, sequence length: %d, lr: %.6f\n", 
           vocab_file, seq_len, learning_rate);
    
    // Load vocabulary parameters from curriculum file
    SimpleVocab vocab;
    int result = load_model_from_vocab_file(vocab_file, &vocab);
    if (result != 0) {
        printf("Error loading vocabulary from curriculum\n");
        return result;
    }
    
    // Process sequence in sliding context windows to predict next token
    float total_loss = 0.0f;
    int num_predictions = 0;
    
    // Process the sequence in sliding windows
    for (int pos = CONTEXT_SIZE; pos < seq_len; pos++) {
        // Create context window: tokens [pos-CONTEXT_SIZE] to [pos-1]
        ContextWindow ctx;
        ctx.token_embeddings = malloc(CONTEXT_SIZE * EMBED_DIM * sizeof(float));
        ctx.q_values = malloc(CONTEXT_SIZE * EMBED_DIM * sizeof(float));
        ctx.k_values = malloc(CONTEXT_SIZE * EMBED_DIM * sizeof(float));
        ctx.v_values = malloc(CONTEXT_SIZE * EMBED_DIM * sizeof(float));
        ctx.attn_scores = malloc(CONTEXT_SIZE * CONTEXT_SIZE * sizeof(float));
        ctx.attn_weights = malloc(CONTEXT_SIZE * CONTEXT_SIZE * sizeof(float));
        ctx.attn_output = malloc(CONTEXT_SIZE * EMBED_DIM * sizeof(float));
        ctx.ffn_intermediate = malloc(CONTEXT_SIZE * FF_HIDDEN * sizeof(float));
        ctx.ffn_output = malloc(CONTEXT_SIZE * EMBED_DIM * sizeof(float));
        ctx.token_ids = malloc(CONTEXT_SIZE * sizeof(int));
        
        // Fill context with tokens and embeddings
        for (int ctx_offset = 0; ctx_offset < CONTEXT_SIZE; ctx_offset++) {
            int ctx_pos = pos - CONTEXT_SIZE + ctx_offset;
            ctx.token_ids[ctx_offset] = token_sequence[ctx_pos];
            
            // Validate token ID
            if (ctx.token_ids[ctx_offset] < 0 || ctx.token_ids[ctx_offset] >= vocab.vocab_size) {
                ctx.token_ids[ctx_offset] = 0; // Use first token as fallback
            }
            
            // Copy embedding for this context token
            for (int d = 0; d < EMBED_DIM; d++) {
                ctx.token_embeddings[ctx_offset * EMBED_DIM + d] = 
                    vocab.embeddings[ctx.token_ids[ctx_offset] * EMBED_DIM + d];
            }
        }
        
        // Target token to predict
        int target_token = token_sequence[pos];
        if (target_token < 0 || target_token >= vocab.vocab_size) {
            target_token = 0; // Use first token as fallback
        }
        
        // 1. Compute Q, K, V for each token in context
        compute_query_key_value(&ctx, &vocab, learning_rate);
        
        // 2. Apply attention mechanism
        apply_attention(&ctx);
        
        // 3. Apply feedforward network
        apply_ffn(&ctx, &vocab, learning_rate);
        
        // 4. Calculate loss - this is a simplification
        // In a full implementation, we'd compute logits for the entire vocabulary
        float prediction_error = 0.0f;
        for (int d = 0; d < EMBED_DIM; d++) {
            float pred = ctx.ffn_output[(CONTEXT_SIZE-1) * EMBED_DIM + d];  // Last position output
            float target = vocab.embeddings[target_token * EMBED_DIM + d];
            float diff = pred - target;
            prediction_error += diff * diff;
        }
        float loss = sqrtf(prediction_error / EMBED_DIM);
        total_loss += loss;
        num_predictions++;
        
        // 5. Update parameters based on prediction error
        update_parameters(&ctx, &vocab, target_token, learning_rate);
        
        printf("Position %d: predicting %d (target: %d), loss: %.6f\n", 
               pos, ctx.token_ids[CONTEXT_SIZE-1], target_token, loss);
        
        // Free context window memory
        SAFE_FREE(ctx.token_embeddings);
        SAFE_FREE(ctx.q_values);
        SAFE_FREE(ctx.k_values);
        SAFE_FREE(ctx.v_values);
        SAFE_FREE(ctx.attn_scores);
        SAFE_FREE(ctx.attn_weights);
        SAFE_FREE(ctx.attn_output);
        SAFE_FREE(ctx.ffn_intermediate);
        SAFE_FREE(ctx.ffn_output);
        SAFE_FREE(ctx.token_ids);
    }
    
    if (num_predictions > 0) {
        float avg_loss = total_loss / num_predictions;
        printf("Average loss across %d predictions: %.6f\n", num_predictions, avg_loss);
    }
    
    // Save the updated parameters back to the curriculum file
    result = save_model_to_vocab_file(vocab_file, &vocab);
    if (result != 0) {
        printf("Error saving updated vocabulary to curriculum\n");
        
        // Free vocabulary memory before returning
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
        
        return result;
    }
    
    printf("TX feedback learning completed for %d prediction steps\n", num_predictions);
    
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