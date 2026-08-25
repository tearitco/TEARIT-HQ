#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define MAX_VOCAB_SIZE 100000
#define EMBED_DIM 256
#define N_HEADS 8
#define N_LAYERS 4
#define SEQ_LEN 128
#define FF_HIDDEN (EMBED_DIM * 4)
#define KV_DIM (EMBED_DIM / N_HEADS)

#define MAX_VOCAB_SIZE 100000
#define EMBED_DIM 256
#define N_HEADS 8
#define N_LAYERS 4
#define SEQ_LEN 128
#define FF_HIDDEN (EMBED_DIM * 4)
#define KV_DIM (EMBED_DIM / N_HEADS)

// Safe memory freeing macro to prevent double free errors
#define SAFE_FREE(ptr) do { if (ptr) { free(ptr); ptr = NULL; } } while(0)

// Vocabulary structure - based on vocab_model_12.c format, enhanced with QKV projections for shared encoder/decoder use
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

// Model structure - simplified to align with karp model but maintain our vocabulary
typedef struct {
    // Token embedding table - uses vocabulary embeddings
    float *token_embedding_table; // [vocab_size * dim]
    
    // RMSNorm weights
    float *rms_att_weight; // [n_layers * dim] 
    float *rms_ffn_weight; // [n_layers * dim]
    float *rms_final_weight; // [dim]
    
    // Attention weights (multi-query attention like karp model)
    float *wq; // [n_layers * dim * dim] - query weights
    float *wk; // [n_layers * dim * kv_dim] - key weights
    float *wv; // [n_layers * dim * kv_dim] - value weights
    float *wo; // [n_layers * dim * dim] - output weights
    
    // Feed-forward network weights (SwiGLU like karp model)
    float *w1; // [n_layers * hidden_dim * dim] - gate projection
    float *w2; // [n_layers * dim * hidden_dim] - output projection
    float *w3; // [n_layers * hidden_dim * dim] - up projection
    
    // Output classifier weights
    float *wcls; // [dim * vocab_size]

    // --- NEW: Gradients for all weights ---
    float *grad_token_embedding_table;
    float *grad_rms_att_weight;
    float *grad_rms_ffn_weight;
    float *grad_rms_final_weight;
    float *grad_wq;
    float *grad_wk;
    float *grad_wv;
    float *grad_wo;
    float *grad_w1;
    float *grad_w2;
    float *grad_w3;
    float *grad_wcls;
    
    int dim;        // transformer dimension
    int hidden_dim; // for ffn layers
    int n_layers;   // number of layers
    int n_heads;    // number of query heads
    int n_kv_heads; // number of key/value heads (can be < query heads because of multiquery)
    int vocab_size; // vocabulary size
    int seq_len;    // max sequence length
    int kv_dim;     // kv dimension per head (dim/n_heads for multiquery)
    
    // KV Cache for inference - back to original dimensions
    float *key_cache;   // [n_layers * seq_len * dim]
    float *value_cache; // [n_layers * seq_len * dim]
} SimpleModel;

// Run state structure - activations and buffers
typedef struct {
    // current wave of activations
    float *x;  // activation at current time stamp (dim,)
    float *xb; // same, but inside a residual branch (dim,)
    float *xb2; // an additional buffer just for convenience (dim,)
    float *hb; // buffer for hidden dimension in the ffn (hidden_dim,)
    float *hb2; // buffer for hidden dimension in the ffn (hidden_dim,)
    float *q; // query (dim,)
    float *att; // buffer for scores/attention values (n_heads * seq_len)
    float *logits; // output logits (vocab_size,)

    // --- NEW: Gradients for all activations ---
    float *grad_x;
    float *grad_xb;
    float *grad_xb2;
    float *grad_hb;
    float *grad_hb2;
    float *grad_q;
    float *grad_att;
    float *grad_logits;

    int dim;        // dimensions stored for safety checks
    int hidden_dim;
    int n_heads;
    int seq_len;
    int vocab_size;
} RunState;

// Function prototypes
void malloc_run_state(RunState* s, SimpleModel* m);
void free_run_state(RunState* s);
void free_model_memory(SimpleModel* m);
void transformer_backward(SimpleModel* model, SimpleVocab* vocab, RunState* state, int token, int pos, int target_token_id);
void zero_gradients(SimpleModel *model);
void update_weights(SimpleModel *model, float learning_rate);
float calculate_cross_entropy_loss(float* probabilities, int target_token_id);
void rmsnorm_backward(float* grad_o, float* grad_x, float* grad_weight, float* x, float* weight, int size);
void softmax_backward(float* grad_out, float* out, int size);
void swiglu_backward(float* grad_xb, float* grad_hb, float* grad_hb2, float* hb, float* hb2, int hidden_dim);
void rope_backward(float* grad_vec, int vec_size, int pos, int head_size);
void matmul_backward(float* grad_xout, float* grad_x, float* grad_w, float* x, float* w, int n, int d);


// Sampling structures and functions
typedef struct {
    float prob;
    int index;
} ProbIndex; // struct used when sorting probabilities during top-p sampling

int compare_prob_index(const void* a, const void* b) {
    ProbIndex* a_ = (ProbIndex*) a;
    ProbIndex* b_ = (ProbIndex*) b;
    if (a_->prob > b_->prob) return -1;
    if (a_->prob < b_->prob) return 1;
    return 0;
}

int sample_argmax(float* probabilities, int n) {
    // return the index that has the highest probability
    int max_i = 0;
    float max_p = probabilities[0];
    for (int i = 1; i < n; i++) {
        if (probabilities[i] > max_p) {
            max_i = i;
            max_p = probabilities[i];
        }
    }
    return max_i;
}

int sample_mult(float* probabilities, int n, float coin) {
    // sample index from probabilities (they must sum to 1!)
    // coin is a random number in [0, 1), usually from random_f32()
    float cdf = 0.0f;
    for (int i = 0; i < n; i++) {
        cdf += probabilities[i];
        if (coin < cdf) {
            return i;
        }
    }
    return n - 1; // in case of rounding errors
}

int sample_topp(float* probabilities, int n, float topp, ProbIndex* probindex, float coin) {
    // top-p sampling (or "nucleus sampling") samples from the smallest set of
    // tokens that exceed probability topp. This way we never sample tokens that
    // have very low probabilities and are less likely to go "off the rails".
    // coin is a random number in [0, 1), usually from random_f32()

    int n0 = 0;
    // quicksort indices in descending order of probabilities
    // values smaller than (1 - topp) / (n - 1) cannot be part of the result
    // so for efficiency we crop these out as candidates before sorting
    const float cutoff = (1.0f - topp) / (n - 1);
    for (int i = 0; i < n; i++) {
        if (probabilities[i] >= cutoff) {
            probindex[n0].index = i;
            probindex[n0].prob = probabilities[i];
            n0++;
        }
    }
    qsort(probindex, n0, sizeof(ProbIndex), compare_prob_index);

    // truncate the list where cumulative probability exceeds topp
    float cumulative_prob = 0.0f;
    int last_idx = n0 - 1; // in case of rounding errors consider all elements
    for (int i = 0; i < n0; i++) {
        cumulative_prob += probindex[i].prob;
        if (cumulative_prob > topp) {
            last_idx = i;
            break; // we've exceeded topp by including last_idx
        }
    }

    // sample from the truncated list
    float r = coin * cumulative_prob;
    float cdf = 0.0f;
    for (int i = 0; i <= last_idx; i++) {
        cdf += probindex[i].prob;
        if (r < cdf) {
            return probindex[i].index;
        }
    }
    return probindex[last_idx].index; // in case of rounding errors
}

unsigned int random_u32(unsigned long long *state) {
    // xorshift rng: https://en.wikipedia.org/wiki/Xorshift#xorshift.2A
    *state ^= *state >> 12;
    *state ^= *state << 25;
    *state ^= *state >> 27;
    return (*state * 0x2545F4914F6CDD1Dull) >> 32;
}

float random_f32(unsigned long long *state) { // random float32 in [0,1)
    return (random_u32(state) >> 8) / 16777216.0f;
}

int sample_with_temperature(float* logits, int vocab_size, float temperature, float topp, unsigned long long *rng_state) {
    // sample the token given the logits and some hyperparameters
    int next;
    if (temperature == 0.0f) {
        // greedy argmax sampling: take the token with the highest probability
        next = sample_argmax(logits, vocab_size);
    } else {
        // apply the temperature to the logits
        for (int q = 0; q < vocab_size; q++) { 
            logits[q] /= temperature; 
        }
        // apply softmax to the logits to get the probabilities for next token
        // Find max logit for numerical stability
        float max_logit = logits[0];
        for (int i = 1; i < vocab_size; i++) {
            if (logits[i] > max_logit) {
                max_logit = logits[i];
            }
        }
        // Exp and sum with numerical stability
        float sum = 0.0f;
        for (int i = 0; i < vocab_size; i++) {
            logits[i] = expf(logits[i] - max_logit);
            sum += logits[i];
        }
        // Normalize
        for (int i = 0; i < vocab_size; i++) {
            logits[i] /= sum;
        }
        // flip a (float) coin (this is our source of entropy for sampling)
        float coin = random_f32(rng_state);
        // we sample from this distribution to get the next token
        if (topp <= 0 || topp >= 1) {
            // simply sample from the predicted probability distribution
            next = sample_mult(logits, vocab_size, coin);
        } else {
            // top-p (nucleus) sampling, clamping the least likely tokens to zero
            ProbIndex* probindex = malloc(vocab_size * sizeof(ProbIndex));
            next = sample_topp(logits, vocab_size, topp, probindex, coin);
            free(probindex);
        }
    }
    return next;
}

// Utility functions
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
        // Apply RMSNorm: x * (1/sqrt(mean(x^2) + eps)) * weight (like LLaMA2)
        o[j] = x[j] * ss * weight[j];
    }
}

void rmsnorm_backward(float* grad_o, float* grad_x, float* grad_weight, float* x, float* weight, int size) {
    // backward pass for rmsnorm
    float ss = 0.0f;
    for (int j = 0; j < size; j++) {
        ss += x[j] * x[j];
    }
    ss /= size;
    ss += 1e-5f;
    float inv_ss = 1.0f / sqrtf(ss);

    for (int j = 0; j < size; j++) {
        // gradient of weight
        grad_weight[j] += grad_o[j] * (inv_ss * x[j]);
        // gradient of x
        float grad_x_part1 = grad_o[j] * (inv_ss * weight[j]);
        float grad_x_part2 = 0.0f;
        for (int k = 0; k < size; k++) {
            grad_x_part2 += grad_o[k] * weight[k] * x[k];
        }
        grad_x_part2 *= (-powf(ss, -1.5f) / size) * x[j];
        grad_x[j] += grad_x_part1 + grad_x_part2;
    }
}

void softmax_backward(float* grad_out, float* out, int size) {
    // Backprop through softmax. grad_out is the gradient of the loss w.r.t. the output of softmax.
    // The input `grad_out` is modified in-place to become the gradient w.r.t. the input of softmax.
    for (int i = 0; i < size; i++) {
        float grad_in_i = 0.0f;
        for (int j = 0; j < size; j++) {
            float delta = (i == j) ? 1.0f : 0.0f;
            grad_in_i += grad_out[j] * out[i] * (delta - out[j]);
        }
        grad_out[i] = grad_in_i;
    }
}

void swiglu_backward(float* grad_xb, float* grad_hb, float* grad_hb2, float* hb, float* hb2, int hidden_dim) {
    for (int i = 0; i < hidden_dim; i++) {
        float val = hb[i];
        float sig = 1.0f / (1.0f + expf(-val));
        float silu = val * sig;
        
        // grad w.r.t. hb
        float dsilu_dhb = sig * (1.0f + val * (1.0f - sig));
        grad_hb[i] += grad_xb[i] * hb2[i] * dsilu_dhb;
        
        // grad w.r.t. hb2
        grad_hb2[i] += grad_xb[i] * silu;
    }
}

void rope_backward(float* grad_vec, int vec_size, int pos, int head_size) {
    // This is the inverse rotation of RoPE.
    for (int i = 0; i < vec_size; i += 2) {
        int head_dim = i % head_size;
        float freq = 1.0f / powf(10000.0f, head_dim / (float)head_size);
        float val = pos * freq;
        float fcr = cosf(val);
        float fci = sinf(val);
        
        float gv0 = grad_vec[i];
        float gv1 = grad_vec[i+1];
        
        // Inverse rotation
        grad_vec[i]   = gv0 * fcr + gv1 * fci;
        grad_vec[i+1] = -gv0 * fci + gv1 * fcr;
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
    if (!xout || !x || !w || n <= 0 || d <= 0) {
        printf("Warning: Invalid parameters in matmul\n");
        return;
    }
    for (int i = 0; i < d; i++) {
        float val = 0.0f;
        for (int j = 0; j < n; j++) {
            // Bounds check before access
            if (i * n + j >= d * n || j >= n) {
                printf("Warning: Matmul access out of bounds\n");
                break;
            }
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

void swiglu(float* xb, float* hb, float* hb2, int hidden_dim) {
    for (int i = 0; i < hidden_dim; i++) {
        float val = hb[i];
        // silu(x)=x*σ(x), where σ(x) is the logistic sigmoid
        val *= (1.0f / (1.0f + expf(-val)));
        // elementwise multiply with w3(x)
        val *= hb2[i];
        xb[i] = val;
    }
}

// Function to add generated token to vocabulary with QKV projections
int expand_vocabulary_with_token(SimpleVocab* vocab, const char* token_word, int* new_token_id) {
    // Check if token already exists
    for (int i = 0; i < vocab->vocab_size; i++) {
        if (strcmp(vocab->words[i], token_word) == 0) {
            *new_token_id = i;
            return 0; // Token already exists
        }
    }
    
    // Check if we have space for a new token
    if (vocab->vocab_size >= vocab->max_size) {
        printf("Error: Vocabulary is at maximum size, cannot add more tokens\n");
        return -1;
    }
    
    // Add new token
    int new_id = vocab->vocab_size;
    
    // Allocate memory for the new word
    vocab->words[new_id] = malloc((strlen(token_word) + 1) * sizeof(char));
    strcpy(vocab->words[new_id], token_word);
    
    // Initialize embedding for this token (deterministic based on word content)
    unsigned long hash = 5381;
    int c;
    const char* str = token_word;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;

    for (int d = 0; d < EMBED_DIM; d++) {
        vocab->embeddings[new_id * EMBED_DIM + d] = 
            (float)(hash % 1000000) / 1000000.0f + (float)d * 0.001f;
        
        // Initialize RoPE positional encodings
        float freq = 1.0f / powf(10000.0f, (float)(d % (EMBED_DIM/2)) / (EMBED_DIM/2));
        float angle = vocab->vocab_size * freq;
        
        if (d % 2 == 0) {
            vocab->rope_pos_enc[new_id * EMBED_DIM + d] = sinf(angle);
        } else {
            vocab->rope_pos_enc[new_id * EMBED_DIM + d] = cosf(angle);
        }
        
        // Initialize Q, K, V projections for this token (deterministic based on word content) 
        vocab->q_proj[new_id * EMBED_DIM + d] = 
            (float)(hash % 1000000) / 1000000.0f + (float)d * 0.001f;
        vocab->k_proj[new_id * EMBED_DIM + d] = 
            (float)(hash % 1000001) / 1000000.0f + (float)d * 0.001f;  // Slightly different hash
        vocab->v_proj[new_id * EMBED_DIM + d] = 
            (float)(hash % 1000002) / 1000000.0f + (float)d * 0.001f;  // Slightly different hash
    }
    
    // Initialize other parameters for this token
    vocab->attention_bias[new_id] = 0.0f;
    vocab->ffn_bias[new_id] = 0.0f;
    vocab->weight1[new_id] = 0.1f;
    vocab->weight2[new_id] = 0.1f;
    vocab->bias1[new_id] = 0.0f;
    vocab->bias2[new_id] = 0.0f;
    vocab->bias3[new_id] = 0.0f;
    vocab->bias4[new_id] = 0.0f;
    
    // Initialize note for this token
    vocab->notes[new_id] = malloc(50 * sizeof(char));  // Allocate space for note
    strcpy(vocab->notes[new_id], "generated_token");
    
    *new_token_id = new_id;
    vocab->vocab_size++;
    
    return 0; // Success
}

int process_prompt_tokens(const char* prompt, SimpleVocab* vocab, int** tokens_output, int* num_tokens_output) {
    // Parse prompt into tokens (simple space-based tokenization for now)
    int num_prompt_tokens = 0;
    
    // Count prompt tokens
    char* temp_tokenizer = malloc(strlen(prompt) + 1);
    strcpy(temp_tokenizer, prompt);
    char* token_check = strtok(temp_tokenizer, " \n\t");
    while (token_check != NULL) {
        num_prompt_tokens++;
        token_check = strtok(NULL, " \n\t");
    }
    free(temp_tokenizer);
    
    // Allocate and parse prompt tokens
    int* tokens = malloc(num_prompt_tokens * sizeof(int));
    strcpy(temp_tokenizer = malloc(strlen(prompt) + 1), prompt);
    token_check = strtok(temp_tokenizer, " \n\t");
    int prompt_token_idx = 0;
    while (token_check != NULL && prompt_token_idx < num_prompt_tokens) {
        // Map token to ID
        int token_id = -1;
        for (int i = 0; i < vocab->vocab_size; i++) {
            if (strcmp(vocab->words[i], token_check) == 0) {
                token_id = i;
                break;
            }
        }
        if (token_id == -1) {
            // Use UNK token if not found
            for (int i = 0; i < vocab->vocab_size; i++) {
                if (strcmp(vocab->words[i], "<UNK>") == 0) {
                    token_id = i;
                    break;
                }
            }
            // If still not found, default to first token
            if (token_id == -1) token_id = 0;
        }
        tokens[prompt_token_idx] = token_id;
        prompt_token_idx++;
        token_check = strtok(NULL, " \n\t");
    }
    free(temp_tokenizer);
    
    *tokens_output = tokens;
    *num_tokens_output = num_prompt_tokens;
    
    return 0; // Success
}

// KV cache management
void zero_kv_cache(SimpleModel *model) {
    // Zero out the KV cache before starting generation
    for (int i = 0; i < model->n_layers * model->seq_len * model->kv_dim; i++) {
        model->key_cache[i] = 0.0f;
        model->value_cache[i] = 0.0f;
    }
}

// Full transformer forward pass with KV caching for generation using vocab QKV projections
float* transformer_forward(SimpleModel *model, SimpleVocab *vocab, RunState *state, int token, int pos) {
    // Full transformer forward pass with KV caching for generation
    // Using vocabulary Q, K, V projections as specified in mirror_v3 requirements
    
    // Bounds checking
    if (token < 0 || token >= model->vocab_size) {
        printf("Warning: Token %d out of bounds (vocab size: %d), using token 0\n", token, model->vocab_size);
        token = 0; // Use first token as fallback
    }
    // Additional bounds checking for pos
    if (pos < 0 || pos >= model->seq_len) {
        printf("Warning: Position %d out of bounds (seq_len: %d)\n", pos, model->seq_len);
        return state->logits; // Return early to avoid crash
    }
    
    // a few convenience variables
    float *x = state->x;
    int dim = model->dim;
    int kv_dim = model->dim;  // For non-GQA: K and V vectors have the same dimension as Q
    int kv_mul = model->n_heads / model->n_kv_heads; // integer multiplier of the kv sharing in multiquery (will be 1 when n_heads = n_kv_heads)
    int hidden_dim = model->hidden_dim;
    int head_size = dim / model->n_heads;
    
    // copy the token embedding into x
    float* content_row = model->token_embedding_table + token * dim;
    memcpy(x, content_row, dim * sizeof(*x));
    
    // forward all the layers
    for(int l = 0; l < model->n_layers; l++) {
        
        // attention rmsnorm
        rmsnorm(state->xb, x, model->rms_att_weight + l*dim, dim);
        
        // key and value point to the kv cache
        int loff = l * model->seq_len * dim; // kv cache layer offset for convenience
        float* k = model->key_cache + loff + pos * dim;
        float* v = model->value_cache + loff + pos * dim;

        // qkv matmuls for this position
        matmul(state->q, state->xb, model->wq + l*dim*dim, dim, dim);
        matmul(k, state->xb, model->wk + l*dim*dim, dim, dim);
        matmul(v, state->xb, model->wv + l*dim*dim, dim, dim);

        // RoPE relative positional encoding: complex-valued rotate q and k in each head
        for (int i = 0; i < dim; i+=2) {
            int head_dim = i % head_size;
            float freq = 1.0f / powf(10000.0f, head_dim / (float)head_size);
            float val = pos * freq;
            float fcr = cosf(val);
            float fci = sinf(val);
            
            float q0 = state->q[i];
            float q1 = state->q[i+1];
            state->q[i]   = q0 * fcr - q1 * fci;
            state->q[i+1] = q0 * fci + q1 * fcr;

            float k0 = k[i];
            float k1 = k[i+1];
            k[i]   = k0 * fcr - k1 * fci;
            k[i+1] = k0 * fci + k1 * fcr;
        }
        
        // multihead attention. iterate over all heads
        int h;
        for (h = 0; h < model->n_heads; h++) {
            // get the query vector for this head
            float* q = state->q + h * head_size;
            // attention scores for this head
            float* att = state->att + h * model->seq_len;
            // iterate over all timesteps, including the current one
            for (int t = 0; t <= pos; t++) {
                // get the key vector for this head and at this timestep
                float* k = model->key_cache + loff + t * dim + h * head_size;
                // calculate the attention score as the dot product of q and k
                float score = 0.0f;
                for (int i = 0; i < head_size; i++) {
                    score += q[i] * k[i];
                }
                score /= sqrtf(head_size);
                // save the score to the attention buffer
                att[t] = score;
            }
            
            // softmax the scores to get attention weights, from 0..pos inclusively
            softmax(att, pos + 1);
            
            // weighted sum of the values, store back into xb
            float* xb = state->xb + h * head_size;
            memset(xb, 0, head_size * sizeof(float));
            for (int t = 0; t <= pos; t++) {
                // get the value vector for this head and at this timestep
                float* v = model->value_cache + loff + t * dim + h * head_size;
                // get the attention weight for this timestep
                float a = att[t];
                // accumulate the weighted value into xb
                for (int i = 0; i < head_size; i++) {
                    xb[i] += a * v[i];
                }
            }
        }
        
        // final matmul to get the output of the attention
        matmul(state->xb2, state->xb, model->wo + l*dim*dim, dim, dim);
        
        // residual connection back into x
        for (int i = 0; i < dim; i++) {
            x[i] += state->xb2[i];
        }
        
        // ffn rmsnorm
        rmsnorm(state->xb, x, model->rms_ffn_weight + l*dim, dim);
        
        // Now for FFN in PyTorch we have: self.w2(F.silu(self.w1(x)) * self.w3(x))
        // first calculate self.w1(x) and self.w3(x)
        matmul(state->hb, state->xb, model->w1 + l*dim*hidden_dim, dim, hidden_dim);
        matmul(state->hb2, state->xb, model->w3 + l*dim*hidden_dim, dim, hidden_dim);
        
        // SwiGLU non-linearity
        swiglu(state->hb, state->hb2, state->hb, hidden_dim);
        
        // final matmul to get the output of the ffn
        matmul(state->xb, state->hb, model->w2 + l*dim*hidden_dim, hidden_dim, dim);
        
        // residual connection
        for (int i = 0; i < dim; i++) {
            x[i] += state->xb[i];
        }
    }
    
    // final rmsnorm
    rmsnorm(x, x, model->rms_final_weight, dim);
    
    // classifier into logits
    matmul(state->logits, x, model->wcls, model->dim, model->vocab_size);
    return state->logits;
}

int generate_text_with_dynamic_vocab_for_file(SimpleModel* model, SimpleVocab* vocab, char* vocab_file, const char* prompt, char* output, int max_tokens, float temperature, float topp) {
    printf("Generating text with dynamic vocabulary expansion: '%s'\n", prompt);
    
    // Initialize run state using the helper function
    RunState state;
    malloc_run_state(&state, model);
    
    // Initialize RNG state for sampling
    unsigned long long rng_state = time(NULL);
    
    // Zero out the KV cache before starting generation
    zero_kv_cache(model);
    
    // Process prompt tokens
    int* prompt_tokens;
    int num_prompt_tokens;
    int result = process_prompt_tokens(prompt, vocab, &prompt_tokens, &num_prompt_tokens);
    if (result != 0) {
        printf("Error processing prompt tokens\n");
        free_run_state(&state);
        return -1;
    }
    
    // Copy prompt to output
    strcpy(output, prompt);
    
    // Process prompt tokens first (fill the KV cache)
    int pos = 0;
    if (num_prompt_tokens > 0) {
        for (int i = 0; i < num_prompt_tokens; i++) {
            transformer_forward(model, vocab, &state, prompt_tokens[i], pos++);
        }
    }
    
    // Use the last prompt token as the starting token for generation, or START token if no prompt
    int current_token = (num_prompt_tokens > 0) ? prompt_tokens[num_prompt_tokens - 1] : 1; // 1 is <START>
    
    // Generate additional tokens based on the prompt
    for (int gen_idx = 0; gen_idx < max_tokens; gen_idx++) {
        // Forward pass to get next token prediction
        float* logits = transformer_forward(model, vocab, &state, current_token, pos);
        
        // Sample next token using the sampling function with temperature
        int next_token_id = sample_with_temperature(logits, model->vocab_size, temperature, topp, &rng_state);
        
        // Get the string representation of the generated token
        char* generated_token_str = vocab->words[next_token_id];
        
        // Add the generated token to output
        strcat(output, " ");
        strcat(output, generated_token_str);
        
        // Update current token for next iteration
        current_token = next_token_id;
        pos++;

        if (pos >= model->seq_len) {
            // Context window is full, break for now.
            // More advanced implementations would handle sliding windows.
            break;
        }
    }
    
    // Free allocated memory
    free(prompt_tokens);
    free_run_state(&state);
    
    printf("Generation completed. Output: %s\n", output);
    return 0; // Success
}

// Function to generate attention map between curriculum and prompt tokens
int generate_attention_map(const char* vocab_file, const char* prompt) {
    printf("Generating attention map for prompt: '%s' with curriculum: %s\n", prompt, vocab_file);
    
    // Load vocabulary to get all tokens
    FILE *file = fopen(vocab_file, "r");
    if (!file) {
        printf("Error: could not open vocabulary file %s\n", vocab_file);
        return -1;
    }
    
    // Skip header line
    char line[1024];
    if (fgets(line, sizeof(line), file) == NULL) {
        printf("Error: could not read header from vocabulary file\n");
        fclose(file);
        return -1;
    }
    
    // Parse prompt into tokens
    char prompt_copy[1000];
    strcpy(prompt_copy, prompt);
    char* prompt_token;
    int prompt_tokens[100];  // Up to 100 prompt tokens
    int num_prompt_tokens = 0;
    
    prompt_token = strtok(prompt_copy, " \n\t");
    while (prompt_token != NULL && num_prompt_tokens < 100) {
        // We'll store token strings temporarily, then match them in curriculum
        prompt_tokens[num_prompt_tokens] = num_prompt_tokens; // Placeholder
        num_prompt_tokens++;
        prompt_token = strtok(NULL, " \n\t");
    }
    
    // Read curriculum tokens
    char** curriculum_tokens = malloc(1000 * sizeof(char*));  // Max 1000 tokens
    int num_curriculum_tokens = 0;
    int token_numbers[1000];  // Store token numbers from curriculum
    float attention_scores[100][1000];  // Scores: prompt x curriculum
    
    // Read curriculum entries
    while (fgets(line, sizeof(line), file) != NULL && num_curriculum_tokens < 1000) {
        int index;
        char word[1000];
        float embedding, pe_value, attention_bias_val, ffn_bias_val, weight1_val, weight2_val, bias1_val, bias2_val, bias3_val, bias4_val, q_val, k_val, v_val;
        char note_str[1000];
        
        if (sscanf(line, "%d %s %f %f %f %f %f %f %f %f %f %f %f %f %f %s", 
                   &index, word, &embedding, &pe_value, 
                   &attention_bias_val, &ffn_bias_val,
                   &weight1_val, &weight2_val, 
                   &bias1_val, &bias2_val, &bias3_val, &bias4_val,
                   &q_val, &k_val, &v_val, note_str) == 16) {
            
            curriculum_tokens[num_curriculum_tokens] = malloc((strlen(word) + 1) * sizeof(char));
            strcpy(curriculum_tokens[num_curriculum_tokens], word);
            token_numbers[num_curriculum_tokens] = index;
            
            // Initialize attention scores with some function of the parameters
            // This simulates the attention strength based on parameter similarities
            for (int p = 0; p < num_prompt_tokens; p++) {
                // Calculate attention based on parameter similarity
                float attn = fabsf(embedding) * 0.3f + fabsf(attention_bias_val) * 0.2f + 
                             fabsf(q_val) * 0.1f + fabsf(k_val) * 0.1f + fabsf(v_val) * 0.1f;
                
                // Normalize to [0, 1] range approximately
                attn = atanf(attn) / M_PI + 0.5f;
                attention_scores[p][num_curriculum_tokens] = attn;
            }
            
            num_curriculum_tokens++;
        }
    }
    fclose(file);
    
    // Write attention map to file
    FILE *map_file = fopen("attention_map.txt", "w");
    if (!map_file) {
        printf("Error: could not create attention_map.txt\n");
        // Free allocated memory
        for (int i = 0; i < num_curriculum_tokens; i++) {
            free(curriculum_tokens[i]);
        }
        free(curriculum_tokens);
        return -1;
    }
    
    // Write header
    fprintf(map_file, "Prompt\tCurriculum_Token\tToken_Number\tAttention_Score\n");
    
    // Write attention scores for each prompt-curriculum token pair
    strcpy(prompt_copy, prompt);
    char* prompt_tok = strtok(prompt_copy, " \n\t");
    int prompt_idx = 0;
    
    while (prompt_tok != NULL && prompt_idx < num_prompt_tokens) {
        for (int c = 0; c < num_curriculum_tokens; c++) {
            fprintf(map_file, "%s\t%s\t%d\t%.6f\n", 
                    prompt_tok, curriculum_tokens[c], token_numbers[c], attention_scores[prompt_idx][c]);
        }
        prompt_tok = strtok(NULL, " \n\t");
        prompt_idx++;
    }
    
    fclose(map_file);
    
    printf("Attention map generated: %d prompt tokens, %d curriculum tokens, %d attention scores saved to attention_map.txt\n", 
           num_prompt_tokens, num_curriculum_tokens, num_prompt_tokens * num_curriculum_tokens);
    
    // Free allocated memory
    for (int i = 0; i < num_curriculum_tokens; i++) {
        free(curriculum_tokens[i]);
    }
    free(curriculum_tokens);
    
    return 0; // Success
}

int load_model_from_vocab_file(const char* vocab_file, SimpleModel* model, SimpleVocab* vocab) {
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
    
    // Initialize model with the loaded vocabulary
    // Set model parameters
    model->dim = EMBED_DIM;
    model->hidden_dim = FF_HIDDEN;
    model->n_layers = N_LAYERS;
    model->n_heads = N_HEADS;
    model->n_kv_heads = N_HEADS;  // For now, using same as n_heads
    model->vocab_size = vocab->vocab_size;
    model->seq_len = SEQ_LEN;
    model->kv_dim = model->dim; // In non-grouped attention, K and V vectors have the same dimension as Q.
    
    // Allocate memory for model weights and KV cache
    model->token_embedding_table = malloc(model->vocab_size * model->dim * sizeof(float));
    model->rms_att_weight = malloc(model->n_layers * model->dim * sizeof(float));
    model->rms_ffn_weight = malloc(model->n_layers * model->dim * sizeof(float));
    model->rms_final_weight = malloc(model->dim * sizeof(float));
    
    // Attention weights
    model->wq = malloc(model->n_layers * model->dim * model->dim * sizeof(float));
    model->wk = malloc(model->n_layers * model->dim * model->kv_dim * sizeof(float)); // Fixed: dim x kv_dim weights
    model->wv = malloc(model->n_layers * model->dim * model->kv_dim * sizeof(float)); // Fixed: dim x kv_dim weights
    model->wo = malloc(model->n_layers * model->dim * model->dim * sizeof(float));
    
    // FFN weights (SwiGLU)
    model->w1 = malloc(model->n_layers * model->hidden_dim * model->dim * sizeof(float));
    model->w2 = malloc(model->n_layers * model->dim * model->hidden_dim * sizeof(float));
    model->w3 = malloc(model->n_layers * model->hidden_dim * model->dim * sizeof(float));
    
    // Output classifier weights
    model->wcls = malloc(model->dim * model->vocab_size * sizeof(float));
    
    // KV Cache - CORRECTED DIMENSIONS FOR GQA
    model->key_cache = malloc(model->n_layers * model->seq_len * model->n_kv_heads * model->kv_dim * sizeof(float));
    model->value_cache = malloc(model->n_layers * model->seq_len * model->n_kv_heads * model->kv_dim * sizeof(float));
    
    // --- Allocate memory for gradients ---
    model->grad_token_embedding_table = malloc(model->vocab_size * model->dim * sizeof(float));
    model->grad_rms_att_weight = malloc(model->n_layers * model->dim * sizeof(float));
    model->grad_rms_ffn_weight = malloc(model->n_layers * model->dim * sizeof(float));
    model->grad_rms_final_weight = malloc(model->dim * sizeof(float));
    model->grad_wq = malloc(model->n_layers * model->dim * model->dim * sizeof(float));
    model->grad_wk = malloc(model->n_layers * model->dim * model->dim * sizeof(float)); // Keep original: dim x dim weights  
    model->grad_wv = malloc(model->n_layers * model->dim * model->dim * sizeof(float)); // Keep original: dim x dim weights
    model->grad_wo = malloc(model->n_layers * model->dim * model->dim * sizeof(float));
    model->grad_w1 = malloc(model->n_layers * model->hidden_dim * model->dim * sizeof(float));
    model->grad_w2 = malloc(model->n_layers * model->dim * model->hidden_dim * sizeof(float));
    model->grad_w3 = malloc(model->n_layers * model->hidden_dim * model->dim * sizeof(float));
    model->grad_wcls = malloc(model->dim * model->vocab_size * sizeof(float));

    // Initialize weights with values from vocabulary where available, otherwise random
    for (int i = 0; i < model->vocab_size * model->dim; i++) {
        if (i < vocab->vocab_size * EMBED_DIM) {
            model->token_embedding_table[i] = vocab->embeddings[i];
        } else {
            model->token_embedding_table[i] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
        }
    }
    
    // Initialize RMSNorm weights to 1.0
    for (int i = 0; i < model->n_layers * model->dim; i++) {
        model->rms_att_weight[i] = 1.0f;
        model->rms_ffn_weight[i] = 1.0f;
    }
    for (int i = 0; i < model->dim; i++) {
        model->rms_final_weight[i] = 1.0f;
    }
    
    // Initialize attention weights
    for (int i = 0; i < model->n_layers * model->dim * model->dim; i++) {
        model->wq[i] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
        model->wo[i] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
    }
    for (int i = 0; i < model->n_layers * model->dim * model->kv_dim; i++) {
        model->wk[i] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
        model->wv[i] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
    }
    
    // Initialize FFN weights
    for (int i = 0; i < model->n_layers * model->hidden_dim * model->dim; i++) {
        model->w1[i] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
        model->w3[i] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
    }
    for (int i = 0; i < model->n_layers * model->dim * model->hidden_dim; i++) {
        model->w2[i] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
    }
    
    // Initialize output classifier weights
    for (int i = 0; i < model->dim * model->vocab_size; i++) {
        model->wcls[i] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
    }
    
    // Initialize KV cache to zero
    for (int i = 0; i < model->n_layers * model->seq_len * model->dim; i++) {
        model->key_cache[i] = 0.0f;
        model->value_cache[i] = 0.0f;
    }
    
    printf("Loaded model with vocabulary from %s (%d tokens)\n", vocab_file, vocab->vocab_size);
    return 0; // Success
}

void malloc_run_state(RunState* s, SimpleModel* m) {
    s->dim = m->dim;
    s->hidden_dim = m->hidden_dim;
    s->n_heads = m->n_heads;
    s->seq_len = m->seq_len;
    s->vocab_size = m->vocab_size;
    
    // Activations
    s->x = calloc(s->dim, sizeof(float));
    s->xb = calloc(s->dim, sizeof(float));
    s->xb2 = calloc(s->dim, sizeof(float));
    s->hb = calloc(s->hidden_dim, sizeof(float));
    s->hb2 = calloc(s->hidden_dim, sizeof(float));
    s->q = calloc(s->dim, sizeof(float));
    s->att = calloc(s->n_heads * s->seq_len, sizeof(float));
    s->logits = calloc(s->vocab_size, sizeof(float));

    // Gradients
    s->grad_x = calloc(s->dim, sizeof(float));
    s->grad_xb = calloc(s->dim, sizeof(float));
    s->grad_xb2 = calloc(s->dim, sizeof(float));
    s->grad_hb = calloc(s->hidden_dim, sizeof(float));
    s->grad_hb2 = calloc(s->hidden_dim, sizeof(float));
    s->grad_q = calloc(s->dim, sizeof(float));
    s->grad_att = calloc(s->n_heads * s->seq_len, sizeof(float));
    s->grad_logits = calloc(s->vocab_size, sizeof(float));
    
    // Ensure all mallocs went fine
    if (!s->x || !s->xb || !s->xb2 || !s->hb || !s->hb2 || !s->q
     || !s->att || !s->logits || !s->grad_x || !s->grad_xb || !s->grad_xb2
     || !s->grad_hb || !s->grad_hb2 || !s->grad_q || !s->grad_att || !s->grad_logits) {
        fprintf(stderr, "malloc failed!\n");
        exit(EXIT_FAILURE);
    }
}

void free_run_state(RunState* s) {
    SAFE_FREE(s->x);
    SAFE_FREE(s->xb);
    SAFE_FREE(s->xb2);
    SAFE_FREE(s->hb);
    SAFE_FREE(s->hb2);
    SAFE_FREE(s->q);
    SAFE_FREE(s->att);
    SAFE_FREE(s->logits);
    SAFE_FREE(s->grad_x);
    SAFE_FREE(s->grad_xb);
    SAFE_FREE(s->grad_xb2);
    SAFE_FREE(s->grad_hb);
    SAFE_FREE(s->grad_hb2);
    SAFE_FREE(s->grad_q);
    SAFE_FREE(s->grad_att);
    SAFE_FREE(s->grad_logits);
}

/**
 * Calculates the cross-entropy loss.
 *
 * @param probabilities The output from the softmax function (must sum to 1.0).
 * @param target_token_id The index of the correct next token.
 *
 * @return The cross-entropy loss value.
 */
float calculate_cross_entropy_loss(float* probabilities, int target_token_id) {
    // Ensure the probability is not zero to avoid log(0) which is -infinity.
    float prob_of_target = probabilities[target_token_id];
    if (prob_of_target == 0.0f) {
        return -logf(1e-9); // Return a very large loss value
    }
    return -logf(prob_of_target);
}

void matmul_backward(float* grad_xout, float* grad_x, float* grad_w, float* x, float* w, int n, int d) {
    // Backprop through W (d,n) @ x (n,) -> xout (d,)
    // grad_x = W.T @ grad_xout
    for (int j = 0; j < n; j++) {
        float val = 0.0f;
        for (int i = 0; i < d; i++) {
            val += w[i * n + j] * grad_xout[i];
        }
        grad_x[j] += val; // Accumulate gradients
    }
    // grad_w = grad_xout @ x.T
    for (int i = 0; i < d; i++) {
        for (int j = 0; j < n; j++) {
            grad_w[i * n + j] += grad_xout[i] * x[j]; // Accumulate gradients
        }
    }
}

void transformer_backward(SimpleModel* model, SimpleVocab* vocab, RunState* state, int token, int pos, int target_token_id) {
    // A few convenience variables
    int dim = model->dim;
    int hidden_dim = model->hidden_dim;
    int head_size = dim / model->n_heads;

    // Zero out all RunState gradients
    memset(state->grad_x, 0, state->dim * sizeof(float));
    memset(state->grad_xb, 0, state->dim * sizeof(float));
    memset(state->grad_xb2, 0, state->dim * sizeof(float));
    memset(state->grad_hb, 0, state->hidden_dim * sizeof(float));
    memset(state->grad_hb2, 0, state->hidden_dim * sizeof(float));
    memset(state->grad_q, 0, state->dim * sizeof(float));
    memset(state->grad_att, 0, state->n_heads * state->seq_len * sizeof(float));
    memset(state->grad_logits, 0, state->vocab_size * sizeof(float));

    // Start backprop at the loss
    memcpy(state->grad_logits, state->logits, model->vocab_size * sizeof(float));
    state->grad_logits[target_token_id] -= 1.0f;

    // Backprop through the final classifier matmul
    matmul_backward(state->grad_logits, state->grad_x, model->grad_wcls, state->x, model->wcls, model->dim, model->vocab_size);

    // Backprop through final rmsnorm
    rmsnorm_backward(state->grad_x, state->grad_x, model->grad_rms_final_weight, state->x, model->rms_final_weight, dim);

    for (int l = model->n_layers - 1; l >= 0; l--) {
        // Backprop through residual connection
        for (int i = 0; i < dim; i++) {
            state->grad_xb[i] = state->grad_x[i]; // Copy gradient to residual branch
            // The gradient also flows directly to the input of the block
        }

        // Backprop through FFN
        matmul_backward(state->grad_xb, state->grad_hb, model->grad_w2 + l*dim*hidden_dim, state->hb, model->w2 + l*dim*hidden_dim, hidden_dim, dim);
        
        swiglu_backward(state->grad_hb, state->grad_hb, state->grad_hb2, state->hb, state->hb2, hidden_dim);

        // Create a temporary buffer for the gradient of xb from the FFN
        float* grad_xb_ffn = calloc(dim, sizeof(float));
        matmul_backward(state->grad_hb, grad_xb_ffn, model->grad_w1 + l*dim*hidden_dim, state->xb, model->w1 + l*dim*hidden_dim, dim, hidden_dim);
        matmul_backward(state->grad_hb2, grad_xb_ffn, model->grad_w3 + l*dim*hidden_dim, state->xb, model->w3 + l*dim*hidden_dim, dim, hidden_dim);

        // Backprop through ffn rmsnorm
        rmsnorm_backward(grad_xb_ffn, state->grad_x, model->grad_rms_ffn_weight + l*dim, state->x, model->rms_ffn_weight + l*dim, dim);
        free(grad_xb_ffn);

        // Backprop through residual connection
        for (int i = 0; i < dim; i++) {
            state->grad_xb2[i] = state->grad_x[i];
        }

        // Backprop through attention output matmul
        float* grad_xb_attn = calloc(dim, sizeof(float));
        matmul_backward(state->grad_xb2, grad_xb_attn, model->grad_wo + l*dim*dim, state->xb, model->wo + l*dim*dim, dim, dim);

        // Allocate temporary buffers for k and v gradients for this layer
        float* grad_k_layer = calloc(dim, sizeof(float));
        float* grad_v_layer = calloc(dim, sizeof(float));

        for (int h = model->n_heads - 1; h >= 0; h--) {
            float* grad_att = state->grad_att + h * model->seq_len;
            float* att = state->att + h * model->seq_len;
            float* grad_xb_h = grad_xb_attn + h * head_size;

            // Backprop through weighted sum of values
            for (int t = pos; t >= 0; t--) {
                float* v = model->value_cache + l * model->seq_len * dim + t * dim + h * head_size;
                float a = att[t];
                for (int i = 0; i < head_size; i++) {
                    grad_att[t] += grad_xb_h[i] * v[i];
                    if (t == pos) { 
                        grad_v_layer[h * head_size + i] += a * grad_xb_h[i];
                    }
                }
            }

            // Backprop through softmax
            softmax_backward(grad_att, att, pos + 1);

            // Backprop through score calculation
            for (int t = pos; t >= 0; t--) {
                float* k = model->key_cache + l * model->seq_len * dim + t * dim + h * head_size;
                float* q = state->q + h * head_size;
                float* grad_q = state->grad_q + h * head_size;
                float score_grad = grad_att[t] / sqrtf(head_size);

                for (int i = 0; i < head_size; i++) {
                    grad_q[i] += k[i] * score_grad;
                     if (t == pos) { 
                        grad_k_layer[h * head_size + i] += q[i] * score_grad;
                    }
                }
            }
        }
        free(grad_xb_attn);

        // Backprop through RoPE for q and k
        rope_backward(state->grad_q, dim, pos, head_size);
        rope_backward(grad_k_layer, dim, pos, head_size);

        // Backprop through q, k, v matmuls
        float* grad_xb_qkv = calloc(dim, sizeof(float));
        matmul_backward(state->grad_q, grad_xb_qkv, model->grad_wq + l*dim*dim, state->xb, model->wq + l*dim*dim, dim, dim);
        matmul_backward(grad_k_layer, grad_xb_qkv, model->grad_wk + l*dim*dim, state->xb, model->wk + l*dim*dim, dim, dim);
        matmul_backward(grad_v_layer, grad_xb_qkv, model->grad_wv + l*dim*dim, state->xb, model->wv + l*dim*dim, dim, dim);

        // Free temporary buffers
        free(grad_k_layer);
        free(grad_v_layer);

        // Backprop through attention rmsnorm
        rmsnorm_backward(grad_xb_qkv, state->grad_x, model->grad_rms_att_weight + l*dim, state->x, model->rms_att_weight + l*dim, dim);
        free(grad_xb_qkv);
    }

    // Backprop to token embeddings
    for (int i = 0; i < dim; i++) {
        model->grad_token_embedding_table[token * dim + i] += state->grad_x[i];
    }
}

void zero_gradients(SimpleModel *model) {
    memset(model->grad_token_embedding_table, 0, model->vocab_size * model->dim * sizeof(float));
    memset(model->grad_rms_att_weight, 0, model->n_layers * model->dim * sizeof(float));
    memset(model->grad_rms_ffn_weight, 0, model->n_layers * model->dim * sizeof(float));
    memset(model->grad_rms_final_weight, 0, model->dim * sizeof(float));
    memset(model->grad_wq, 0, model->n_layers * model->dim * model->dim * sizeof(float));
    memset(model->grad_wk, 0, model->n_layers * model->dim * model->dim * sizeof(float));
    memset(model->grad_wv, 0, model->n_layers * model->dim * model->dim * sizeof(float));
    memset(model->grad_wo, 0, model->n_layers * model->dim * model->dim * sizeof(float));
    memset(model->grad_w1, 0, model->n_layers * model->hidden_dim * model->dim * sizeof(float));
    memset(model->grad_w2, 0, model->n_layers * model->dim * model->hidden_dim * sizeof(float));
    memset(model->grad_w3, 0, model->n_layers * model->hidden_dim * model->dim * sizeof(float));
    memset(model->grad_wcls, 0, model->dim * model->vocab_size * sizeof(float));
}

void update_weights(SimpleModel *model, float learning_rate) {
    // Update weights using SGD
    for (int i = 0; i < model->vocab_size * model->dim; i++) model->token_embedding_table[i] -= learning_rate * model->grad_token_embedding_table[i];
    for (int i = 0; i < model->n_layers * model->dim; i++) model->rms_att_weight[i] -= learning_rate * model->grad_rms_att_weight[i];
    for (int i = 0; i < model->n_layers * model->dim; i++) model->rms_ffn_weight[i] -= learning_rate * model->grad_rms_ffn_weight[i];
    for (int i = 0; i < model->dim; i++) model->rms_final_weight[i] -= learning_rate * model->grad_rms_final_weight[i];
    for (int i = 0; i < model->n_layers * model->dim * model->dim; i++) model->wq[i] -= learning_rate * model->grad_wq[i];
    for (int i = 0; i < model->n_layers * model->dim * model->dim; i++) model->wk[i] -= learning_rate * model->grad_wk[i];
    for (int i = 0; i < model->n_layers * model->dim * model->dim; i++) model->wv[i] -= learning_rate * model->grad_wv[i];
    for (int i = 0; i < model->n_layers * model->dim * model->dim; i++) model->wo[i] -= learning_rate * model->grad_wo[i];
    for (int i = 0; i < model->n_layers * model->hidden_dim * model->dim; i++) model->w1[i] -= learning_rate * model->grad_w1[i];
    for (int i = 0; i < model->n_layers * model->dim * model->hidden_dim; i++) model->w2[i] -= learning_rate * model->grad_w2[i];
    for (int i = 0; i < model->n_layers * model->hidden_dim * model->dim; i++) model->w3[i] -= learning_rate * model->grad_w3[i];
    for (int i = 0; i < model->dim * model->vocab_size; i++) model->wcls[i] -= learning_rate * model->grad_wcls[i];
}

void save_model_weights(SimpleModel* model, const char* curriculum_path) {
    char weights_path[1024];
    snprintf(weights_path, sizeof(weights_path), "%s.weights", curriculum_path);

    FILE* file = fopen(weights_path, "w");
    if (!file) {
        printf("Error: could not open file %s for writing\n", weights_path);
        return;
    }

    fprintf(file, "token_embedding_table\n");
    for(int i = 0; i < model->vocab_size * model->dim; i++) { fprintf(file, "%f ", model->token_embedding_table[i]); }
    fprintf(file, "\n");

    fprintf(file, "rms_att_weight\n");
    for(int i = 0; i < model->n_layers * model->dim; i++) { fprintf(file, "%f ", model->rms_att_weight[i]); }
    fprintf(file, "\n");

    fprintf(file, "rms_ffn_weight\n");
    for(int i = 0; i < model->n_layers * model->dim; i++) { fprintf(file, "%f ", model->rms_ffn_weight[i]); }
    fprintf(file, "\n");

    fprintf(file, "rms_final_weight\n");
    for(int i = 0; i < model->dim; i++) { fprintf(file, "%f ", model->rms_final_weight[i]); }
    fprintf(file, "\n");

    fprintf(file, "wq\n");
    for(int i = 0; i < model->n_layers * model->dim * model->dim; i++) { fprintf(file, "%f ", model->wq[i]); }
    fprintf(file, "\n");

    fprintf(file, "wk\n");
    for(int i = 0; i < model->n_layers * model->dim * model->dim; i++) { fprintf(file, "%f ", model->wk[i]); }
    fprintf(file, "\n");

    fprintf(file, "wv\n");
    for(int i = 0; i < model->n_layers * model->dim * model->dim; i++) { fprintf(file, "%f ", model->wv[i]); }
    fprintf(file, "\n");

    fprintf(file, "wo\n");
    for(int i = 0; i < model->n_layers * model->dim * model->dim; i++) { fprintf(file, "%f ", model->wo[i]); }
    fprintf(file, "\n");

    fprintf(file, "w1\n");
    for(int i = 0; i < model->n_layers * model->hidden_dim * model->dim; i++) { fprintf(file, "%f ", model->w1[i]); }
    fprintf(file, "\n");

    fprintf(file, "w2\n");
    for(int i = 0; i < model->n_layers * model->dim * model->hidden_dim; i++) { fprintf(file, "%f ", model->w2[i]); }
    fprintf(file, "\n");

    fprintf(file, "w3\n");
    for(int i = 0; i < model->n_layers * model->hidden_dim * model->dim; i++) { fprintf(file, "%f ", model->w3[i]); }
    fprintf(file, "\n");

    fprintf(file, "wcls\n");
    for(int i = 0; i < model->dim * model->vocab_size; i++) { fprintf(file, "%f ", model->wcls[i]); }
    fprintf(file, "\n");

    fclose(file);
    printf("Saved model weights to %s\n", weights_path);
}

void load_model_weights(SimpleModel* model, const char* curriculum_path) {
    char weights_path[1024];
    snprintf(weights_path, sizeof(weights_path), "%s.weights", curriculum_path);

    FILE* file = fopen(weights_path, "r");
    if (!file) {
        printf("Warning: could not open file %s for reading. Using random weights.\n", weights_path);
        return;
    }

    char header[256];
    while (fscanf(file, "%s", header) == 1) {
        if (strcmp(header, "token_embedding_table") == 0) {
            for(int i = 0; i < model->vocab_size * model->dim; i++) { fscanf(file, "%f", &model->token_embedding_table[i]); }
        } else if (strcmp(header, "rms_att_weight") == 0) {
            for(int i = 0; i < model->n_layers * model->dim; i++) { fscanf(file, "%f", &model->rms_att_weight[i]); }
        } else if (strcmp(header, "rms_ffn_weight") == 0) {
            for(int i = 0; i < model->n_layers * model->dim; i++) { fscanf(file, "%f", &model->rms_ffn_weight[i]); }
        } else if (strcmp(header, "rms_final_weight") == 0) {
            for(int i = 0; i < model->dim; i++) { fscanf(file, "%f", &model->rms_final_weight[i]); }
        } else if (strcmp(header, "wq") == 0) {
            for(int i = 0; i < model->n_layers * model->dim * model->dim; i++) { fscanf(file, "%f", &model->wq[i]); }
        } else if (strcmp(header, "wk") == 0) {
            for(int i = 0; i < model->n_layers * model->dim * model->dim; i++) { fscanf(file, "%f", &model->wk[i]); }
        } else if (strcmp(header, "wv") == 0) {
            for(int i = 0; i < model->n_layers * model->dim * model->dim; i++) { fscanf(file, "%f", &model->wv[i]); }
        } else if (strcmp(header, "wo") == 0) {
            for(int i = 0; i < model->n_layers * model->dim * model->dim; i++) { fscanf(file, "%f", &model->wo[i]); }
        } else if (strcmp(header, "w1") == 0) {
            for(int i = 0; i < model->n_layers * model->hidden_dim * model->dim; i++) { fscanf(file, "%f", &model->w1[i]); }
        } else if (strcmp(header, "w2") == 0) {
            for(int i = 0; i < model->n_layers * model->dim * model->hidden_dim; i++) { fscanf(file, "%f", &model->w2[i]); }
        } else if (strcmp(header, "w3") == 0) {
            for(int i = 0; i < model->n_layers * model->hidden_dim * model->dim; i++) { fscanf(file, "%f", &model->w3[i]); }
        } else if (strcmp(header, "wcls") == 0) {
            for(int i = 0; i < model->dim * model->vocab_size; i++) { fscanf(file, "%f", &model->wcls[i]); }
        }
    }

    fclose(file);
    printf("Loaded model weights from %s\n", weights_path);
}

void print_usage(char* program_name) {
    printf("Usage: %s <command> [args...]\n", program_name);
    printf("Commands:\n");
    printf("  generate <vocab_file> <temperature> <top_p> <max_tokens> <prompt> - Generate text with dynamic vocab expansion\n");
    printf("  train <vocab_file> <epochs> - Train the model using the specified curriculum\n");
    printf("\nExamples:\n");
    printf("  %s generate curriculum/test/test.txt 1.0 0.9 20 \"hello world\"\n", program_name);
    printf("  %s train curriculum/test/test.txt 3\n", program_name);
}

int main(int argc, char *argv[]) {
    srand(time(NULL));

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    char* command = argv[1];
    
    if (strcmp(command, "generate") == 0) {
        if (argc < 7) {
            printf("Error: Missing required parameters for generate command\n");
            print_usage(argv[0]);
            return 1;
        }
        
        char* vocab_file = argv[2];
        float temperature = atof(argv[3]);
        float top_p = atof(argv[4]);
        int max_tokens = atoi(argv[5]);
        char* prompt = argv[6];
        
        printf("Generation mode: vocab_file=%s, temp=%.2f, top_p=%.2f, max_tokens=%d, prompt='%s'\n",
               vocab_file, temperature, top_p, max_tokens, prompt);
        
        // Initialize model and vocabulary
        SimpleModel model;
        SimpleVocab vocab;
        
        // Load model from vocabulary file
        int result = load_model_from_vocab_file(vocab_file, &model, &vocab);
        if (result != 0) {
            printf("Error loading model from vocabulary file\n");
            return result;
        }
        load_model_weights(&model, vocab_file);
        
        // Generate text with dynamic vocabulary expansion
        char output[2000];
        strcpy(output, prompt);
        
        result = generate_text_with_dynamic_vocab_for_file(&model, &vocab, vocab_file, prompt, output, max_tokens, temperature, top_p);
        if (result != 0) {
            printf("Error during text generation\n");
            
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
            
            return result;
        }
        
        printf("Final generated text: %s\n", output);
        
        // Generate attention map based on the prompt and curriculum
        printf("Generating attention map for prompt: '%s'...\n", prompt);
        int attn_result = generate_attention_map(vocab_file, prompt);
        if (attn_result != 0) {
            printf("Warning: Failed to generate attention map\n");
        } else {
            printf("Attention map successfully generated in attention_map.txt\n");
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
        
    } else if (strcmp(command, "train") == 0) {
        if (argc < 5) {
            printf("Error: Missing required parameters for train command\n");
            printf("Usage: %s train <vocab_file> <epochs> <learning_rate>\n", argv[0]);
            return 1;
        }
        
        char* vocab_file = argv[2];
        int epochs = atoi(argv[3]);
        float learning_rate = atof(argv[4]);
        
        printf("Training mode: vocab_file=%s, epochs=%d, lr=%.6f\n", vocab_file, epochs, learning_rate);
        
        // Initialize model, vocab, and run state
        SimpleModel model;
        SimpleVocab vocab;
        RunState state;

        // Load model and vocab from the curriculum file
        if (load_model_from_vocab_file(vocab_file, &model, &vocab) != 0) {
            printf("Error: Failed to load model from vocab file.\n");
            return 1;
        }
        load_model_weights(&model, vocab_file);
        malloc_run_state(&state, &model);
        
        // Load the full corpus sequence into an array of token IDs
        FILE *file = fopen(vocab_file, "r");
        if (!file) {
            printf("Error: could not open vocabulary file %s\n", vocab_file);
            return -1;
        }
        char line[2048];
        fgets(line, sizeof(line), file); // Skip header

        int* corpus_token_ids = malloc(vocab.vocab_size * sizeof(int));
        if (!corpus_token_ids) {
            printf("Error: Memory allocation failed for corpus tokens.\n");
            return 1;
        }
        int num_corpus_tokens = 0;
        int temp_id;
        while (fscanf(file, "%d %*s %*f %*f %*f %*f %*f %*f %*f %*f %*f %*f %*f %*f %*f %*s", &temp_id) == 1) {
            corpus_token_ids[num_corpus_tokens++] = temp_id - 1; // Convert to 0-indexed
        }
        fclose(file);

        if (num_corpus_tokens < 2) {
            printf("Error: Not enough tokens in the corpus for training.\n");
            free(corpus_token_ids);
            return 1;
        }

        // --- Main Training Loop ---
        for (int epoch = 0; epoch < epochs; epoch++) {
            float total_epoch_loss = 0.0f;
            
            // Before each epoch, reset the KV cache to process the sequence from the start
            zero_kv_cache(&model);

            for (int i = 0; i < num_corpus_tokens - 1; i++) {
                int current_token_id = corpus_token_ids[i];
                int target_token_id = corpus_token_ids[i+1];

                // 1. FORWARD PASS: Run the model to get logits
                float* logits = transformer_forward(&model, &vocab, &state, current_token_id, i);

                // 2. SOFTMAX: Convert logits to probabilities (in-place)
                softmax(logits, model.vocab_size);

                // 3. LOSS CALCULATION: Calculate the real cross-entropy loss
                float loss = calculate_cross_entropy_loss(logits, target_token_id);
                total_epoch_loss += loss;
                
                // 4. BACKWARD PASS: Compute all gradients
                // Clear gradients from previous step
                zero_gradients(&model);
                transformer_backward(&model, &vocab, &state, current_token_id, i, target_token_id);

                // 5. UPDATE WEIGHTS: Apply gradients to update model weights
                update_weights(&model, learning_rate);
            }

            float avg_loss = total_epoch_loss / (num_corpus_tokens - 1);
            
            // 6. LOGGING: Log the real loss for the epoch
            printf("Epoch %d/%d, Average Loss: %f\n", epoch + 1, epochs, avg_loss);
            FILE *loss_file = fopen("loss.txt", "a");
            if (loss_file) {
                fprintf(loss_file, "%d,training,epoch_avg,token,%.6f\n", epoch + 1, avg_loss);
                fclose(loss_file);
            }
        }

        printf("Training finished.\n");
        save_model_weights(&model, vocab_file);
        
        // Free memory
        free(corpus_token_ids);
        free_run_state(&state);
        free_model_memory(&model);
        
    } else {
        printf("Error: Unknown command '%s'\n", command);
        print_usage(argv[0]);
        return 1;
    }
    
    return 0;
}
void free_model_memory(SimpleModel* m) {
    SAFE_FREE(m->token_embedding_table);
    SAFE_FREE(m->rms_att_weight);
    SAFE_FREE(m->rms_ffn_weight);
    SAFE_FREE(m->rms_final_weight);
    SAFE_FREE(m->wq);
    SAFE_FREE(m->wk);
    SAFE_FREE(m->wv);
    SAFE_FREE(m->wo);
    SAFE_FREE(m->w1);
    SAFE_FREE(m->w2);
    SAFE_FREE(m->w3);
    SAFE_FREE(m->wcls);
    SAFE_FREE(m->key_cache);
    SAFE_FREE(m->value_cache);
    SAFE_FREE(m->grad_token_embedding_table);
    SAFE_FREE(m->grad_rms_att_weight);
    SAFE_FREE(m->grad_rms_ffn_weight);
    SAFE_FREE(m->grad_rms_final_weight);
    SAFE_FREE(m->grad_wq);
    SAFE_FREE(m->grad_wk);
    SAFE_FREE(m->grad_wv);
    SAFE_FREE(m->grad_wo);
    SAFE_FREE(m->grad_w1);
    SAFE_FREE(m->grad_w2);
    SAFE_FREE(m->grad_w3);
    SAFE_FREE(m->grad_wcls);
}