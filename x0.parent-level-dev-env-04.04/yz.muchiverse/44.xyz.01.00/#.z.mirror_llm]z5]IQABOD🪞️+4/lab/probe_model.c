#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define MAX_VOCAB_SIZE 100000
#define EMBED_DIM 16
#define N_HEADS 4
#define N_LAYERS 2
#define SEQ_LEN 32
#define FF_HIDDEN (EMBED_DIM * 4)
#define KV_DIM (EMBED_DIM / N_HEADS)

// Safe memory freeing macro
#define SAFE_FREE(ptr) do { if (ptr) { free(ptr); ptr = NULL; } } while(0)

// Structs copied from generation_module.c
typedef struct {
    char **words;
    int vocab_size;
    // Other fields are not needed for this program
} SimpleVocab;

typedef struct {
    float *token_embedding_table;
    float *rms_att_weight;
    float *rms_ffn_weight;
    float *rms_final_weight;
    float *wq;
    float *wk;
    float *wv;
    float *wo;
    float *w1;
    float *w2;
    float *w3;
    float *wcls;
    int dim;
    int hidden_dim;
    int n_layers;
    int n_heads;
    int n_kv_heads;
    int vocab_size;
    int seq_len;
    int kv_dim;
    float *key_cache;
    float *value_cache;
} SimpleModel;

typedef struct {
    float *x;
    float *xb;
    float *xb2;
    float *hb;
    float *hb2;
    float *q;
    float *att;
    float *logits;
    int dim;
    int hidden_dim;
    int n_heads;
    int seq_len;
    int vocab_size;
} RunState;

// Forward declarations for functions copied from generation_module.c
void rmsnorm(float *o, float *x, float *weight, int size);
void softmax(float *x, int size);
void matmul(float *xout, float *x, float *w, int n, int d);
void rope_rotate(float *vec, int vec_size, int pos, int head_size);
void swiglu(float* xb, float* hb, float* hb2, int hidden_dim);
float* transformer_forward(SimpleModel *model, SimpleVocab *vocab, RunState *state, int token, int pos);
int load_model_from_vocab_file(const char* vocab_file, SimpleModel* model, SimpleVocab* vocab);
void load_model_weights(SimpleModel* model, const char* curriculum_path);
void malloc_run_state(RunState* s, SimpleModel* m);
void free_run_state(RunState* s);
void free_model_memory(SimpleModel* m);

// Main function for the probe_model program
int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s <vocab_file> <weights_file_path_base> <output_probe_file>\n", argv[0]);
        return 1;
    }

    char* vocab_file = argv[1];
    char* weights_path_base = argv[2];
    char* output_probe_file = argv[3];

    SimpleModel model;
    SimpleVocab vocab;
    RunState state;

    // Load the model structure and vocabulary
    if (load_model_from_vocab_file(vocab_file, &model, &vocab) != 0) {
        fprintf(stderr, "Error: Failed to load model from vocab file.\n");
        return 1;
    }

    // Load the trained weights
    load_model_weights(&model, weights_path_base);

    // Allocate memory for the run state
    malloc_run_state(&state, &model);

    FILE* out_file = fopen(output_probe_file, "w");
    if (!out_file) {
        fprintf(stderr, "Error: Could not open output file %s\n", output_probe_file);
        return 1;
    }

    // Write header
    fprintf(out_file, "Token Word_ID Final_Embedding Query_Vector Key_Vector\n");

    printf("Probing model... this may take a moment.\n");

    // Loop through each token in the vocabulary to probe it
    for (int i = 0; i < vocab.vocab_size; i++) {
        // Run a forward pass for the current token at position 0
        transformer_forward(&model, &vocab, &state, i, 0);

        // Capture the activations
        float* final_embedding = state.x;
        float* query_vec = state.q;
        // The key vector is in the KV cache
        float* key_vec = model.key_cache + (model.n_layers - 1) * model.seq_len * model.dim; // Last layer, pos 0

        // Save the probe data to the output file
        fprintf(out_file, "%s %d ", vocab.words[i], i);

        // Save the Final Embedding vector
        for (int d = 0; d < model.dim; d++) { fprintf(out_file, "%f ", final_embedding[d]); } 
        
        // Save the Query vector
        for (int d = 0; d < model.dim; d++) { fprintf(out_file, "%f ", query_vec[d]); } 

        // Save the Key vector
        for (int d = 0; d < model.dim; d++) { fprintf(out_file, "%f ", key_vec[d]); } 
        
        fprintf(out_file, "\n");
    }

    printf("Probe data saved to %s\n", output_probe_file);

    // Cleanup
    fclose(out_file);
    free_run_state(&state);
    // Simplified free for vocab
    for (int i = 0; i < vocab.vocab_size; i++) { free(vocab.words[i]); }
    free(vocab.words);
    free_model_memory(&model);

    return 0;
}


// --- Copied functions from generation_module.c ---

void rmsnorm(float *o, float *x, float *weight, int size) {
    float ss = 0.0f;
    for (int j = 0; j < size; j++) { ss += x[j] * x[j]; }
    ss /= size;
    ss += 1e-5f;
    ss = 1.0f / sqrtf(ss);
    for (int j = 0; j < size; j++) { o[j] = weight[j] * (ss * x[j]); }
}

void softmax(float *x, int size) {
    float max_val = x[0];
    for (int i = 1; i < size; i++) { if (x[i] > max_val) { max_val = x[i]; } }
    float sum = 0.0f;
    for (int i = 0; i < size; i++) { x[i] = expf(x[i] - max_val); sum += x[i]; }
    for (int i = 0; i < size; i++) { x[i] /= sum; }
}

void matmul(float *xout, float *x, float *w, int n, int d) {
    for (int i = 0; i < d; i++) {
        float val = 0.0f;
        for (int j = 0; j < n; j++) { val += w[i * n + j] * x[j]; }
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
        val *= (1.0f / (1.0f + expf(-val)));
        val *= hb2[i];
        xb[i] = val;
    }
}

float* transformer_forward(SimpleModel *model, SimpleVocab *vocab, RunState *state, int token, int pos) {
    float *x = state->x;
    int dim = model->dim;
    int hidden_dim = model->hidden_dim;
    int head_size = dim / model->n_heads;
    
    memcpy(x, model->token_embedding_table + token * dim, dim * sizeof(*x));
    
    for(int l = 0; l < model->n_layers; l++) {
        rmsnorm(state->xb, x, model->rms_att_weight + l*dim, dim);
        
        int loff = l * model->seq_len * dim;
        float* k = model->key_cache + loff + pos * dim;
        float* v = model->value_cache + loff + pos * dim;

        matmul(state->q, state->xb, model->wq + l*dim*dim, dim, dim);
        matmul(k, state->xb, model->wk + l*dim*dim, dim, dim);
        matmul(v, state->xb, model->wv + l*dim*dim, dim, dim);

        rope_rotate(state->q, dim, pos, head_size);
        rope_rotate(k, dim, pos, head_size);
        
        int h;
        for (h = 0; h < model->n_heads; h++) {
            float* q = state->q + h * head_size;
            float* att = state->att + h * model->seq_len;
            for (int t = 0; t <= pos; t++) {
                float* k_t = model->key_cache + loff + t * dim + h * head_size;
                float score = 0.0f;
                for (int i = 0; i < head_size; i++) { score += q[i] * k_t[i]; }
                att[t] = score / sqrtf(head_size);
            }
            softmax(att, pos + 1);
            float* xb_h = state->xb + h * head_size;
            memset(xb_h, 0, head_size * sizeof(float));
            for (int t = 0; t <= pos; t++) {
                float* v_t = model->value_cache + loff + t * dim + h * head_size;
                float a = att[t];
                for (int i = 0; i < head_size; i++) { xb_h[i] += a * v_t[i]; }
            }
        }
        
        matmul(state->xb2, state->xb, model->wo + l*dim*dim, dim, dim);
        for (int i = 0; i < dim; i++) { x[i] += state->xb2[i]; }
        
        rmsnorm(state->xb, x, model->rms_ffn_weight + l*dim, dim);
        matmul(state->hb, state->xb, model->w1 + l*dim*hidden_dim, dim, hidden_dim);
        matmul(state->hb2, state->xb, model->w3 + l*dim*hidden_dim, dim, hidden_dim);
        swiglu(state->hb, state->hb2, state->hb, hidden_dim);
        matmul(state->xb, state->hb, model->w2 + l*dim*hidden_dim, hidden_dim, dim);
        for (int i = 0; i < dim; i++) { x[i] += state->xb[i]; }
    }
    
    rmsnorm(x, x, model->rms_final_weight, dim);
    matmul(state->logits, x, model->wcls, model->dim, model->vocab_size);
    return state->logits;
}

// A simplified version that only loads what's needed for the model
int load_model_from_vocab_file(const char* vocab_file, SimpleModel* model, SimpleVocab* vocab) {
    FILE *file = fopen(vocab_file, "r");
    if (!file) { return -1; }
    char line[2048];
    int line_count = 0;
    fgets(line, sizeof(line), file); // Skip header
    while (fgets(line, sizeof(line), file) != NULL) { line_count++; }
    rewind(file);
    fgets(line, sizeof(line), file); // Skip header

    vocab->vocab_size = line_count;
    vocab->words = malloc(vocab->vocab_size * sizeof(char*));
    
    float* embeddings = malloc(vocab->vocab_size * EMBED_DIM * sizeof(float));

    for (int i = 0; i < vocab->vocab_size; i++) {
        char word[1000];
        if (fscanf(file, "%*d %999s %*f %*f %*f %*f %*f %*f %*f %*f %*f %*f %*f %*f %*f %*s", word) != 1) { return -1; }
        vocab->words[i] = malloc((strlen(word) + 1) * sizeof(char));
        strcpy(vocab->words[i], word);
    }
    fclose(file);

    model->dim = EMBED_DIM;
    model->hidden_dim = FF_HIDDEN;
    model->n_layers = N_LAYERS;
    model->n_heads = N_HEADS;
    model->n_kv_heads = N_HEADS;
    model->vocab_size = vocab->vocab_size;
    model->seq_len = SEQ_LEN;
    model->kv_dim = model->dim;
    
    model->token_embedding_table = malloc(model->vocab_size * model->dim * sizeof(float));
    model->rms_att_weight = malloc(model->n_layers * model->dim * sizeof(float));
    model->rms_ffn_weight = malloc(model->n_layers * model->dim * sizeof(float));
    model->rms_final_weight = malloc(model->dim * sizeof(float));
    model->wq = malloc(model->n_layers * model->dim * model->dim * sizeof(float));
    model->wk = malloc(model->n_layers * model->dim * model->dim * sizeof(float));
    model->wv = malloc(model->n_layers * model->dim * model->dim * sizeof(float));
    model->wo = malloc(model->n_layers * model->dim * model->dim * sizeof(float));
    model->w1 = malloc(model->n_layers * model->hidden_dim * model->dim * sizeof(float));
    model->w2 = malloc(model->n_layers * model->dim * model->hidden_dim * sizeof(float));
    model->w3 = malloc(model->n_layers * model->hidden_dim * model->dim * sizeof(float));
    model->wcls = malloc(model->dim * model->vocab_size * sizeof(float));
    model->key_cache = malloc(model->n_layers * model->seq_len * model->dim * sizeof(float));
    model->value_cache = malloc(model->n_layers * model->seq_len * model->dim * sizeof(float));
    
    // Initialize weights randomly
    for (int i = 0; i < model->vocab_size * model->dim; i++) { model->token_embedding_table[i] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f; }
    for (int i = 0; i < model->n_layers * model->dim; i++) { model->rms_att_weight[i] = 1.0f; model->rms_ffn_weight[i] = 1.0f; }
    for (int i = 0; i < model->dim; i++) { model->rms_final_weight[i] = 1.0f; }
    for (int i = 0; i < model->n_layers * model->dim * model->dim; i++) { model->wq[i] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f; model->wk[i] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f; model->wv[i] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f; model->wo[i] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f; }
    for (int i = 0; i < model->n_layers * model->hidden_dim * model->dim; i++) { model->w1[i] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f; model->w3[i] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f; }
    for (int i = 0; i < model->n_layers * model->dim * model->hidden_dim; i++) { model->w2[i] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f; }
    for (int i = 0; i < model->dim * model->vocab_size; i++) { model->wcls[i] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f; }

    free(embeddings);
    return 0;
}

void load_model_weights(SimpleModel* model, const char* curriculum_path) {
    char weights_path[1024];
    snprintf(weights_path, sizeof(weights_path), "%s.weights", curriculum_path);
    FILE* file = fopen(weights_path, "r");
    if (!file) { printf("Warning: could not open weights file %s. Using random weights.\n", weights_path); return; }
    char header[256];
    while (fscanf(file, "%s", header) == 1) {
        if (strcmp(header, "token_embedding_table") == 0) { for(int i = 0; i < model->vocab_size * model->dim; i++) { fscanf(file, "%f", &model->token_embedding_table[i]); } } 
        else if (strcmp(header, "rms_att_weight") == 0) { for(int i = 0; i < model->n_layers * model->dim; i++) { fscanf(file, "%f", &model->rms_att_weight[i]); } } 
        else if (strcmp(header, "rms_ffn_weight") == 0) { for(int i = 0; i < model->n_layers * model->dim; i++) { fscanf(file, "%f", &model->rms_ffn_weight[i]); } } 
        else if (strcmp(header, "rms_final_weight") == 0) { for(int i = 0; i < model->dim; i++) { fscanf(file, "%f", &model->rms_final_weight[i]); } } 
        else if (strcmp(header, "wq") == 0) { for(int i = 0; i < model->n_layers * model->dim * model->dim; i++) { fscanf(file, "%f", &model->wq[i]); } } 
        else if (strcmp(header, "wk") == 0) { for(int i = 0; i < model->n_layers * model->dim * model->dim; i++) { fscanf(file, "%f", &model->wk[i]); } } 
        else if (strcmp(header, "wv") == 0) { for(int i = 0; i < model->n_layers * model->dim * model->dim; i++) { fscanf(file, "%f", &model->wv[i]); } } 
        else if (strcmp(header, "wo") == 0) { for(int i = 0; i < model->n_layers * model->dim * model->dim; i++) { fscanf(file, "%f", &model->wo[i]); } } 
        else if (strcmp(header, "w1") == 0) { for(int i = 0; i < model->n_layers * model->hidden_dim * model->dim; i++) { fscanf(file, "%f", &model->w1[i]); } } 
        else if (strcmp(header, "w2") == 0) { for(int i = 0; i < model->n_layers * model->dim * model->hidden_dim; i++) { fscanf(file, "%f", &model->w2[i]); } } 
        else if (strcmp(header, "w3") == 0) { for(int i = 0; i < model->n_layers * model->hidden_dim * model->dim; i++) { fscanf(file, "%f", &model->w3[i]); } } 
        else if (strcmp(header, "wcls") == 0) { for(int i = 0; i < model->dim * model->vocab_size; i++) { fscanf(file, "%f", &model->wcls[i]); } }
    }
    fclose(file);
    printf("Loaded model weights from %s\n", weights_path);
}

void malloc_run_state(RunState* s, SimpleModel* m) {
    s->dim = m->dim;
    s->hidden_dim = m->hidden_dim;
    s->n_heads = m->n_heads;
    s->seq_len = m->seq_len;
    s->vocab_size = m->vocab_size;
    s->x = calloc(s->dim, sizeof(float));
    s->xb = calloc(s->dim, sizeof(float));
    s->xb2 = calloc(s->dim, sizeof(float));
    s->hb = calloc(s->hidden_dim, sizeof(float));
    s->hb2 = calloc(s->hidden_dim, sizeof(float));
    s->q = calloc(s->dim, sizeof(float));
    s->att = calloc(s->n_heads * s->seq_len, sizeof(float));
    s->logits = calloc(s->vocab_size, sizeof(float));
    if (!s->x || !s->xb || !s->xb2 || !s->hb || !s->hb2 || !s->q || !s->att || !s->logits) {
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
}
