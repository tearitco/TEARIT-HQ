#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <libgen.h>

#define MAX_LINE_LENGTH 1024
#define MAX_VOCAB_SIZE 100000
#define MAX_RESPONSE_TOKENS 100
#define MAX_CURRICULA 30
#define EMBEDDING_DIM 32
#define HIDDEN_DIM 64

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

// Structure to hold a complete vocabulary
struct Vocabulary {
    struct VocabEntry *entries;
    int size;
};

typedef struct { float W_q[EMBEDDING_DIM][EMBEDDING_DIM], W_k[EMBEDDING_DIM][EMBEDDING_DIM], W_v[EMBEDDING_DIM][EMBEDDING_DIM]; } AttentionLayer;
typedef struct { float weights[EMBEDDING_DIM][HIDDEN_DIM]; float biases[HIDDEN_DIM]; } MlpLayer;
typedef struct { float **weights; float *biases; } OutputLayer;

typedef struct {
    int trained;
    char train_dir[MAX_LINE_LENGTH];
    AttentionLayer attn;
    MlpLayer mlp;
    OutputLayer out;
} CurriculumModel;

// Load the trained attention model (W_q, W_k, W_v each EMBEDDING_DIM x EMBEDDING_DIM)
void load_attention_text(const char *fn, AttentionLayer *l) {
    FILE *f = fopen(fn, "r");
    if (!f) return;
    for (int i = 0; i < EMBEDDING_DIM; i++) for (int j = 0; j < EMBEDDING_DIM; j++) fscanf(f, "%f", &l->W_q[i][j]);
    for (int i = 0; i < EMBEDDING_DIM; i++) for (int j = 0; j < EMBEDDING_DIM; j++) fscanf(f, "%f", &l->W_k[i][j]);
    for (int i = 0; i < EMBEDDING_DIM; i++) for (int j = 0; j < EMBEDDING_DIM; j++) fscanf(f, "%f", &l->W_v[i][j]);
    fclose(f);
}

// Load the trained MLP model (EMBEDDING_DIM x HIDDEN_DIM weights + HIDDEN_DIM biases)
void load_mlp_text(const char *fn, MlpLayer *l) {
    FILE *f = fopen(fn, "r");
    if (!f) return;
    for (int i = 0; i < EMBEDDING_DIM; i++) for (int j = 0; j < HIDDEN_DIM; j++) fscanf(f, "%f", &l->weights[i][j]);
    for (int i = 0; i < HIDDEN_DIM; i++) fscanf(f, "%f", &l->biases[i]);
    fclose(f);
}

// Load the trained output layer (HIDDEN_DIM x vocab_size weights + vocab_size biases)
void load_output_text(const char *fn, OutputLayer *l, int vs) {
    FILE *f = fopen(fn, "r");
    if (!f) return;
    l->weights = malloc(HIDDEN_DIM * sizeof(float *));
    for (int i = 0; i < HIDDEN_DIM; i++) l->weights[i] = malloc(vs * sizeof(float));
    l->biases = malloc(vs * sizeof(float));
    for (int i = 0; i < HIDDEN_DIM; i++) for (int j = 0; j < vs; j++) fscanf(f, "%f", &l->weights[i][j]);
    for (int i = 0; i < vs; i++) fscanf(f, "%f", &l->biases[i]);
    fclose(f);
}

// Derive the trained-model directory from a curriculum file path:
// curriculum/<Subject>/<Subject>.txt -> curriculum/<Subject>/
// (trainer.c writes matrix files to the same directory as the vocab
// file, not to a _train subdirectory — see trainer.c:173 output_dir =
// dirname(vocab_filename). The old _train convention is obsolete.)
void derive_train_dir(const char *curriculum_path, char *train_dir, int train_dir_size) {
    char dir_copy[MAX_LINE_LENGTH];
    snprintf(dir_copy, sizeof(dir_copy), "%s", curriculum_path);
    char *dir = dirname(dir_copy);
    snprintf(train_dir, train_dir_size, "%s", dir);
}

// Load all trained model files for one curriculum
void load_curriculum_model(CurriculumModel *cm, const char *curriculum_path, int vocab_size) {
    cm->trained = 0;
    cm->out.weights = NULL;
    cm->out.biases = NULL;
    derive_train_dir(curriculum_path, cm->train_dir, sizeof(cm->train_dir));
    char path[MAX_LINE_LENGTH];
    snprintf(path, sizeof(path), "%s/attention_model.txt", cm->train_dir);
    load_attention_text(path, &cm->attn);
    snprintf(path, sizeof(path), "%s/mlp_model.txt", cm->train_dir);
    load_mlp_text(path, &cm->mlp);
    snprintf(path, sizeof(path), "%s/output_layer.txt", cm->train_dir);
    load_output_text(path, &cm->out, vocab_size);
    FILE *check = fopen(path, "r");
    if (check) { fclose(check); cm->trained = 1; }
}

// Free the dynamically allocated output layer
void free_output_layer(OutputLayer *l) {
    if (!l->weights) return;
    for (int i = 0; i < HIDDEN_DIM; i++) free(l->weights[i]);
    free(l->weights);
    free(l->biases);
    l->weights = NULL;
    l->biases = NULL;
}

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

// Function to load vocabulary from file
int load_vocabulary(struct VocabEntry *vocab, const char *filename) {
    FILE *infile = fopen(filename, "r");
    if (!infile) {
        perror("Error opening vocab file");
        return 0;
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
    return vocab_size;
}

// Function to merge multiple vocabularies (MOE approach)
// Simply concatenate all vocabularies without deduplication to preserve unique positional encodings
int merge_vocabularies(struct VocabEntry *merged_vocab, struct Vocabulary *vocabularies, int num_vocabularies) {
    int total_size = 0;
    
    // For MOE, we'll combine all vocabularies without deduplication
    // This preserves unique positional encodings which can be very helpful
    for (int v = 0; v < num_vocabularies && total_size < MAX_VOCAB_SIZE; v++) {
        for (int i = 0; i < vocabularies[v].size && total_size < MAX_VOCAB_SIZE; i++) {
            merged_vocab[total_size] = vocabularies[v].entries[i];
            total_size++;
        }
    }
    
    return total_size;
}

// Run the trained forward pass for one curriculum (mirrors forward_prop.c with causal_attention=0)
void trained_forward_pass(struct VocabEntry *vocab, int vocab_size, int current_word_index, const CurriculumModel *cm, float *scores) {
    float iv[EMBEDDING_DIM] = {vocab[current_word_index].embedding, vocab[current_word_index].pe, vocab[current_word_index].weight, vocab[current_word_index].bias1, vocab[current_word_index].bias2, vocab[current_word_index].bias3, vocab[current_word_index].bias4};
    float q[EMBEDDING_DIM] = {0}, k[EMBEDDING_DIM] = {0}, val[EMBEDDING_DIM] = {0};

    for (int j = 0; j < EMBEDDING_DIM; j++) {
        for (int l = 0; l < EMBEDDING_DIM; l++) {
            q[j] += iv[l] * cm->attn.W_q[l][j];
            k[j] += iv[l] * cm->attn.W_k[l][j];
            val[j] += iv[l] * cm->attn.W_v[l][j];
        }
    }

    float scale = 1.0f / sqrtf((float)EMBEDDING_DIM);
    for (int j = 0; j < vocab_size; j++) {
        float nk[EMBEDDING_DIM] = {vocab[j].embedding, vocab[j].pe, vocab[j].weight, vocab[j].bias1, vocab[j].bias2, vocab[j].bias3, vocab[j].bias4};
        scores[j] = 0;
        for (int l = 0; l < EMBEDDING_DIM; l++) scores[j] += q[l] * nk[l];
        scores[j] *= scale;
        if (scores[j] > 10.0f) scores[j] = 10.0f;
        if (scores[j] < -10.0f) scores[j] = -10.0f;
    }

    softmax(scores, vocab_size, 1.0f);

    float ctx[EMBEDDING_DIM] = {0};
    for (int l = 0; l < EMBEDDING_DIM; l++) {
        ctx[l] = 0;
        for (int j = 0; j < vocab_size; j++) {
            float nv[EMBEDDING_DIM] = {vocab[j].embedding, vocab[j].pe, vocab[j].weight, vocab[j].bias1, vocab[j].bias2, vocab[j].bias3, vocab[j].bias4};
            ctx[l] += scores[j] * nv[l];
        }
        ctx[l] += iv[l];
    }

    float h[HIDDEN_DIM] = {0};
    for (int j = 0; j < HIDDEN_DIM; j++) {
        h[j] = 0;
        for (int l = 0; l < EMBEDDING_DIM; l++) h[j] += ctx[l] * cm->mlp.weights[l][j];
        h[j] += cm->mlp.biases[j];
    }
    for (int j = 0; j < HIDDEN_DIM; j++) if (h[j] < 0) h[j] = 0;

    for (int j = 0; j < vocab_size; j++) {
        scores[j] = 0;
        for (int l = 0; l < HIDDEN_DIM; l++) scores[j] += h[l] * cm->out.weights[l][j];
        scores[j] += cm->out.biases[j];
    }
}

const char* predict_next_word(struct VocabEntry *merged_vocab, int merged_vocab_size, int current_word_index, float temperature,
                              const struct Vocabulary *vocabularies, const CurriculumModel *models,
                              const int *curriculum_offsets, int num_curricula) {
    int owning_curriculum = 0;
    for (int c = 0; c < num_curricula; c++) {
        if (current_word_index < curriculum_offsets[c] + vocabularies[c].size) {
            owning_curriculum = c;
            break;
        }
    }
    int local_index = current_word_index - curriculum_offsets[owning_curriculum];
    const CurriculumModel *cm = &models[owning_curriculum];

    float *global_scores = malloc(merged_vocab_size * sizeof(float));
    if (!global_scores) {
        return "end-token";
    }

    float iv[EMBEDDING_DIM];
    iv[0] = merged_vocab[current_word_index].embedding;
    iv[1] = merged_vocab[current_word_index].pe;
    iv[2] = merged_vocab[current_word_index].weight;
    iv[3] = merged_vocab[current_word_index].bias1;
    iv[4] = merged_vocab[current_word_index].bias2;
    iv[5] = merged_vocab[current_word_index].bias3;
    iv[6] = merged_vocab[current_word_index].bias4;
    for (int j = 7; j < EMBEDDING_DIM; j++) iv[j] = 0.0f;

    for (int i = 0; i < merged_vocab_size; i++) {
        float nv[EMBEDDING_DIM];
        nv[0] = merged_vocab[i].embedding;
        nv[1] = merged_vocab[i].pe;
        nv[2] = merged_vocab[i].weight;
        nv[3] = merged_vocab[i].bias1;
        nv[4] = merged_vocab[i].bias2;
        nv[5] = merged_vocab[i].bias3;
        nv[6] = merged_vocab[i].bias4;
        for (int j = 6; j < EMBEDDING_DIM; j++) nv[j] = 0.0f;
        float dot = 0.0f;
        for (int j = 0; j < EMBEDDING_DIM; j++) dot += iv[j] * nv[j];

        if (strcmp(merged_vocab[i].word, "end-token") == 0 || strcmp(merged_vocab[i].word, "start-token") == 0) {
            dot = -100.0f;
        }
        global_scores[i] = dot;
    }

    if (cm->trained) {
        struct VocabEntry *owner_vocab = vocabularies[owning_curriculum].entries;
        int owner_vocab_size = vocabularies[owning_curriculum].size;
        float *local_scores = malloc(owner_vocab_size * sizeof(float));
        if (local_scores) {
            trained_forward_pass(owner_vocab, owner_vocab_size, local_index, cm, local_scores);
            int offset = curriculum_offsets[owning_curriculum];
            for (int i = 0; i < owner_vocab_size; i++) {
                global_scores[offset + i] = local_scores[i];
            }
            free(local_scores);
        }
    }

    softmax(global_scores, merged_vocab_size, temperature);

    int top_n = 10;
    if (temperature < 0.5) top_n = 5;
    else if (temperature > 2.0) top_n = 20;
    if (top_n > merged_vocab_size) top_n = merged_vocab_size;

    int *top_indices = malloc(top_n * sizeof(int));
    float *top_scores = malloc(top_n * sizeof(float));

    for (int i = 0; i < top_n; i++) {
        top_indices[i] = i;
        top_scores[i] = global_scores[i];
    }

    for (int i = 0; i < top_n; i++) {
        int max_idx = i;
        for (int j = i + 1; j < top_n; j++) {
            if (top_scores[j] > top_scores[max_idx]) max_idx = j;
        }
        float ts = top_scores[i]; top_scores[i] = top_scores[max_idx]; top_scores[max_idx] = ts;
        int ti = top_indices[i]; top_indices[i] = top_indices[max_idx]; top_indices[max_idx] = ti;
    }

    for (int i = top_n; i < merged_vocab_size; i++) {
        for (int j = 0; j < top_n; j++) {
            if (global_scores[i] > top_scores[j]) {
                for (int k = top_n - 1; k > j; k--) {
                    top_scores[k] = top_scores[k-1];
                    top_indices[k] = top_indices[k-1];
                }
                top_scores[j] = global_scores[i];
                top_indices[j] = i;
                break;
            }
        }
    }

    int top_k = 5;
    if (top_k > top_n) top_k = top_n;
    if (top_k > merged_vocab_size) top_k = merged_vocab_size;

    float topk_exp[top_k];
    float topk_sum = 0.0f;
    float topk_max = top_scores[0];
    for (int i = 1; i < top_k; i++) {
        if (top_scores[i] > topk_max) topk_max = top_scores[i];
    }
    for (int i = 0; i < top_k; i++) {
        topk_exp[i] = expf((top_scores[i] - topk_max) / temperature);
        if (topk_exp[i] != topk_exp[i] || topk_exp[i] > 1e10f) topk_exp[i] = 0.0f;
        topk_sum += topk_exp[i];
    }
    if (topk_sum < 1e-12f || topk_sum != topk_sum) {
        for (int i = 0; i < top_k; i++) topk_exp[i] = 1.0f / top_k;
        topk_sum = 1.0f;
    }
    for (int i = 0; i < top_k; i++) topk_exp[i] /= topk_sum;

    int next_word_index = top_indices[0];
    float r = (float)rand() / (float)RAND_MAX;
    float cumsum = 0.0f;
    for (int i = 0; i < top_k; i++) {
        cumsum += topk_exp[i];
        if (r < cumsum) {
            next_word_index = top_indices[i];
            break;
        }
    }
    if (next_word_index < 0 || next_word_index >= merged_vocab_size) {
        next_word_index = top_indices[0];
    }

    free(global_scores);
    free(top_indices);
    free(top_scores);

    return merged_vocab[next_word_index].word;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <curriculum_bank.txt> \"<prompt>\" [length] [temperature]\n", argv[0]);
        return 1;
    }

    srand(time(NULL));

    char *curriculum_bank_file = argv[1];
    char *prompt = argv[2];
    int desired_length = -1;
    float temperature = 1.0f;

    if (argc >= 4) {
        desired_length = atoi(argv[3]);
    }
    if (argc >= 5) {
        temperature = atof(argv[4]);
    }

    // Read curriculum paths from the bank file
    FILE *bank_file = fopen(curriculum_bank_file, "r");
    if (!bank_file) {
        perror("Error opening curriculum bank file");
        return 1;
    }

    char curriculum_paths[MAX_CURRICULA][MAX_LINE_LENGTH];
    int num_curricula = 0;
    
    char line[MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), bank_file) && num_curricula < MAX_CURRICULA) {
        // Remove newline character
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) > 0) {
            strcpy(curriculum_paths[num_curricula], line);
            num_curricula++;
        }
    }
    fclose(bank_file);

    if (num_curricula == 0) {
        fprintf(stderr, "No curriculum paths found in bank file\n");
        return 1;
    }

    // Load vocabularies from all curriculum files
    struct Vocabulary vocabularies[MAX_CURRICULA];
    CurriculumModel curriculum_models[MAX_CURRICULA];
    for (int i = 0; i < num_curricula; i++) {
        vocabularies[i].entries = malloc(MAX_VOCAB_SIZE * sizeof(struct VocabEntry));
        if (!vocabularies[i].entries) {
            perror("Failed to allocate memory for vocabulary");
            // Free previously allocated vocabularies
            for (int j = 0; j < i; j++) {
                free(vocabularies[j].entries);
            }
            return 1;
        }
        vocabularies[i].size = load_vocabulary(vocabularies[i].entries, curriculum_paths[i]);
        fprintf(stderr, "Loaded %d words from %s\n", vocabularies[i].size, curriculum_paths[i]);
        load_curriculum_model(&curriculum_models[i], curriculum_paths[i], vocabularies[i].size);
        if (curriculum_models[i].trained) {
            fprintf(stderr, "Loaded trained model from %s\n", curriculum_models[i].train_dir);
        } else {
            fprintf(stderr, "No trained model found for %s (using fallback scoring)\n", curriculum_paths[i]);
        }
    }

    // Merge vocabularies (for MOE implementation, this would be more sophisticated)
    struct VocabEntry *merged_vocab = malloc(MAX_VOCAB_SIZE * sizeof(struct VocabEntry));
    if (!merged_vocab) {
        perror("Failed to allocate memory for merged vocabulary");
        // Free allocated vocabularies
        for (int i = 0; i < num_curricula; i++) {
            free(vocabularies[i].entries);
        }
        return 1;
    }
    
    int merged_vocab_size = merge_vocabularies(merged_vocab, vocabularies, num_curricula);
    fprintf(stderr, "Merged vocabulary size: %d\n", merged_vocab_size);

    // Record the merged-vocab offset where each curriculum starts
    int curriculum_offsets[MAX_CURRICULA];
    int running_offset = 0;
    for (int i = 0; i < num_curricula; i++) {
        curriculum_offsets[i] = running_offset;
        running_offset += vocabularies[i].size;
    }

    printf("Prompt: %s\n", prompt);
    
    // Create debug file and write prompt
    FILE *debug_file = fopen("debug_chain.txt", "w");
    if (debug_file) {
        fprintf(debug_file, "=== Chatbot MOE Debug Log ===\n");
        fprintf(debug_file, "Prompt: %s\n", prompt);
        fprintf(debug_file, "Desired length: %d\n", desired_length);
        fprintf(debug_file, "Temperature: %f\n", temperature);
        fprintf(debug_file, "Using %d training sets:\n", num_curricula);
        for (int i = 0; i < num_curricula; i++) {
            fprintf(debug_file, "  %d. %s\n", i+1, curriculum_paths[i]);
        }
        fprintf(debug_file, "========================\n\n");
        fclose(debug_file);
    }
    
    printf("Generating response: ");

    char *token = strtok(prompt, " ");
    int last_word_index = -1;

    while (token != NULL) {
        last_word_index = find_word(merged_vocab, merged_vocab_size, token);
        token = strtok(NULL, " ");
    }

    if (last_word_index == -1) {
        last_word_index = find_word(merged_vocab, merged_vocab_size, "start-token");
    }

    int length_count = 0;
    char response_buffer[MAX_RESPONSE_TOKENS * 100]; // Max 100 tokens, each max 100 chars
    response_buffer[0] = '\0'; // Initialize empty string

    // Ensure we have a minimum temperature for variety
    float min_temperature = 0.1f;
    if (temperature < min_temperature) temperature = min_temperature;

    const char* next_word = predict_next_word(merged_vocab, merged_vocab_size, last_word_index, temperature, vocabularies, curriculum_models, curriculum_offsets, num_curricula);

    // Generate at least one word
    if (strcmp(next_word, "end-token") == 0) {
        // Try once more with higher temperature
        next_word = predict_next_word(merged_vocab, merged_vocab_size, last_word_index, temperature * 2.0f, vocabularies, curriculum_models, curriculum_offsets, num_curricula);
    }

    while (strcmp(next_word, "end-token") != 0 && (desired_length == -1 || length_count < desired_length) && length_count < MAX_RESPONSE_TOKENS) {
        snprintf(response_buffer + strlen(response_buffer), sizeof(response_buffer) - strlen(response_buffer), "%s ", next_word);
        printf("\rGenerating response: %d/%d tokens", length_count + 1, desired_length == -1 ? MAX_RESPONSE_TOKENS : desired_length);
        fflush(stdout);

        last_word_index = find_word(merged_vocab, merged_vocab_size, next_word);
        if (last_word_index == -1) {
            // If word not found, use start-token
            last_word_index = find_word(merged_vocab, merged_vocab_size, "start-token");
        }
        
        next_word = predict_next_word(merged_vocab, merged_vocab_size, last_word_index, temperature, vocabularies, curriculum_models, curriculum_offsets, num_curricula);
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

    // Free allocated memory
    free(merged_vocab);
    for (int i = 0; i < num_curricula; i++) {
        free(vocabularies[i].entries);
        if (curriculum_models[i].trained) {
            free_output_layer(&curriculum_models[i].out);
        }
    }

    return 0;
}