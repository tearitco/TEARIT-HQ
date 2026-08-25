#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_VOCAB_SIZE 100000
#define EMBED_DIM 16

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
    int *positions;         // [vocab_size] position in original corpus where token appeared
    int vocab_size;
    int max_size;
} SimpleVocab;

// FUNCTION DECLARATIONS

int read_corpus_tokens(const char* filename, char*** tokens_output, int* num_tokens);
int generate_vocab_from_tokens(char** tokens, int num_tokens, SimpleVocab* vocab);
int save_vocab_to_curriculum(SimpleVocab* vocab, const char* corpus_path);
int load_model_from_vocab_file(const char* vocab_file, void* model, SimpleVocab* vocab);
int create_directory(const char* path);

// MAIN FUNCTION
int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <command> [args...]\n", argv[0]);
        printf("Commands:\n");
        printf("  read_corpus <corpus_file> - Read tokens from corpus file\n");
        printf("  generate_vocab <corpus_file> - Generate vocabulary from corpus\n");
        printf("  save_vocab <curriculum_file> - Save vocabulary to curriculum\n");
        printf("  load_vocab <vocab_file> - Load vocabulary from file\n");
        return 1;
    }
    
    char* command = argv[1];
    
    if (strcmp(command, "read_corpus") == 0) {
        if (argc < 3) {
            printf("Error: Missing corpus file\n");
            return 1;
        }
        
        char** tokens;
        int num_tokens;
        int result = read_corpus_tokens(argv[2], &tokens, &num_tokens);
        if (result != 0) {
            printf("Error reading corpus tokens\n");
            return result;
        }
        
        printf("Read %d tokens from corpus\n", num_tokens);
        
        // Free tokens
        for (int i = 0; i < num_tokens; i++) {
            free(tokens[i]);
        }
        free(tokens);
        
    } else if (strcmp(command, "generate_vocab") == 0) {
        if (argc < 3) {
            printf("Error: Missing corpus file\n");
            return 1;
        }
        
        char** tokens;
        int num_tokens;
        int result = read_corpus_tokens(argv[2], &tokens, &num_tokens);
        if (result != 0) {
            printf("Error reading corpus tokens\n");
            return result;
        }
        
        SimpleVocab vocab;
        result = generate_vocab_from_tokens(tokens, num_tokens, &vocab);
        if (result != 0) {
            printf("Error generating vocabulary\n");
            // Free tokens before returning
            for (int i = 0; i < num_tokens; i++) {
                free(tokens[i]);
            }
            free(tokens);
            return result;
        }
        
        printf("Generated vocabulary with %d tokens\n", vocab.vocab_size);
        
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
        SAFE_FREE(vocab.positions);
        
        // Free tokens
        for (int i = 0; i < num_tokens; i++) {
            free(tokens[i]);
        }
        free(tokens);
        
    } else if (strcmp(command, "generate_and_save_vocab") == 0) {
        if (argc < 3) {
            printf("Error: Missing corpus file\n");
            return 1;
        }
        
        char** tokens;
        int num_tokens;
        int result = read_corpus_tokens(argv[2], &tokens, &num_tokens);
        if (result != 0) {
            printf("Error reading corpus tokens\n");
            return result;
        }
        
        SimpleVocab vocab;
        result = generate_vocab_from_tokens(tokens, num_tokens, &vocab);
        if (result != 0) {
            printf("Error generating vocabulary\n");
            // Free tokens before returning
            for (int i = 0; i < num_tokens; i++) {
                free(tokens[i]);
            }
            free(tokens);
            return result;
        }
        
        printf("Generated vocabulary with %d tokens\n", vocab.vocab_size);
        
        // Save vocabulary to curriculum
        result = save_vocab_to_curriculum(&vocab, argv[2]);
        if (result != 0) {
            printf("Error saving vocabulary to curriculum\n");
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
            SAFE_FREE(vocab.positions);
            
            // Free tokens
            for (int i = 0; i < num_tokens; i++) {
                free(tokens[i]);
            }
            free(tokens);
            return result;
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
        SAFE_FREE(vocab.positions);
        
        // Free tokens
        for (int i = 0; i < num_tokens; i++) {
            free(tokens[i]);
        }
        free(tokens);
        
    } else if (strcmp(command, "save_vocab") == 0) {
        printf("Save vocab function not implemented in this module\n");
        printf("This function should be called from the main orchestrator\n");
        return 1;
        
    } else if (strcmp(command, "load_vocab") == 0) {
        if (argc < 3) {
            printf("Error: Missing vocab file\n");
            return 1;
        }
        
        SimpleVocab vocab;
        int result = load_model_from_vocab_file(argv[2], NULL, &vocab);
        if (result != 0) {
            printf("Error loading vocabulary from file\n");
            return result;
        }
        
        printf("Loaded vocabulary with %d tokens\n", vocab.vocab_size);
        
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
        SAFE_FREE(vocab.positions);
        
    } else {
        printf("Error: Unknown command '%s'\n", command);
        return 1;
    }
    
    return 0;
}

int read_corpus_tokens(const char* filename, char*** tokens_output, int* num_tokens) {
    printf("Reading corpus tokens from: %s\n", filename);
    
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Error: could not open corpus file %s\n", filename);
        return -1;
    }
    
    // First pass: count tokens
    char word[1000];
    *num_tokens = 0;
    while (fscanf(file, "%999s", word) == 1) {
        (*num_tokens)++;
    }
    fclose(file);
    
    printf("Found %d tokens in corpus\n", *num_tokens);
    
    // Allocate memory for tokens
    *tokens_output = malloc(*num_tokens * sizeof(char*));
    if (!*tokens_output) {
        printf("Error: memory allocation failed for tokens\n");
        return -1;
    }
    
    // Second pass: read tokens
    file = fopen(filename, "r");
    if (!file) {
        printf("Error: could not reopen corpus file %s\n", filename);
        free(*tokens_output);
        return -1;
    }
    
    int idx = 0;
    while (fscanf(file, "%999s", word) == 1 && idx < *num_tokens) {
        // Clean the token by removing common punctuation
        int len = strlen(word);
        while (len > 0 && 
               (word[len-1] == '.' || word[len-1] == ',' || word[len-1] == '!' || 
                word[len-1] == '?' || word[len-1] == ';' || word[len-1] == ':' ||
                word[len-1] == '"' || word[len-1] == '\'' || 
                word[len-1] == ')' || word[len-1] == ']')) {
            word[--len] = '\0';
        }
        int start = 0;
        while (start < len && 
               (word[start] == '(' || word[start] == '[' || 
                word[start] == '"' || word[start] == '\'')) {
            start++;
        }
        if (start > 0) {
            memmove(word, word + start, len - start + 1);
        }
        
        if (strlen(word) > 0) {
            (*tokens_output)[idx] = malloc((strlen(word) + 1) * sizeof(char));
            strcpy((*tokens_output)[idx], word);
            idx++;
        }
    }
    *num_tokens = idx; // Update count with cleaned tokens
    fclose(file);
    
    printf("Successfully read and cleaned %d tokens\n", *num_tokens);
    return 0; // Success
}

int generate_vocab_from_tokens(char** tokens, int num_tokens, SimpleVocab* vocab) {
    printf("Generating vocabulary from %d tokens\n", num_tokens);
    
    // Initialize vocabulary structure
    vocab->vocab_size = 0;
    vocab->max_size = MAX_VOCAB_SIZE;
    vocab->words = malloc(MAX_VOCAB_SIZE * sizeof(char*));
    vocab->word_to_id = malloc(MAX_VOCAB_SIZE * sizeof(int));
    vocab->embeddings = malloc(MAX_VOCAB_SIZE * EMBED_DIM * sizeof(float));
    vocab->rope_pos_enc = malloc(MAX_VOCAB_SIZE * EMBED_DIM * sizeof(float));
    vocab->weight1 = malloc(MAX_VOCAB_SIZE * sizeof(float));
    vocab->weight2 = malloc(MAX_VOCAB_SIZE * sizeof(float));
    vocab->bias1 = malloc(MAX_VOCAB_SIZE * sizeof(float));
    vocab->bias2 = malloc(MAX_VOCAB_SIZE * sizeof(float));
    vocab->bias3 = malloc(MAX_VOCAB_SIZE * sizeof(float));
    vocab->bias4 = malloc(MAX_VOCAB_SIZE * sizeof(float));
    vocab->attention_bias = malloc(MAX_VOCAB_SIZE * sizeof(float));
    vocab->ffn_bias = malloc(MAX_VOCAB_SIZE * sizeof(float));
    vocab->q_proj = malloc(MAX_VOCAB_SIZE * EMBED_DIM * sizeof(float));
    vocab->k_proj = malloc(MAX_VOCAB_SIZE * EMBED_DIM * sizeof(float));
    vocab->v_proj = malloc(MAX_VOCAB_SIZE * EMBED_DIM * sizeof(float));
    vocab->notes = malloc(MAX_VOCAB_SIZE * sizeof(char*));
    vocab->positions = malloc(MAX_VOCAB_SIZE * sizeof(int));
    
    if (!vocab->words || !vocab->word_to_id || !vocab->embeddings || !vocab->rope_pos_enc ||
        !vocab->weight1 || !vocab->weight2 || !vocab->bias1 || !vocab->bias2 ||
        !vocab->bias3 || !vocab->bias4 || !vocab->attention_bias || !vocab->ffn_bias ||
        !vocab->q_proj || !vocab->k_proj || !vocab->v_proj || !vocab->notes || !vocab->positions) {
        printf("Error: memory allocation failed for vocabulary\n");
        return -1;
    }
    
    // Add special tokens first
    char* special_tokens[] = {"<PAD>", "<START>", "<END>", "<UNK>"};
    int num_special = 4;
    
    for (int i = 0; i < num_special; i++) {
        vocab->words[vocab->vocab_size] = malloc((strlen(special_tokens[i]) + 1) * sizeof(char));
        strcpy(vocab->words[vocab->vocab_size], special_tokens[i]);
        vocab->word_to_id[vocab->vocab_size] = vocab->vocab_size;
        
        // Initialize embedding for special token
        for (int d = 0; d < EMBED_DIM; d++) {
            vocab->embeddings[vocab->vocab_size * EMBED_DIM + d] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
            
            // Initialize RoPE positional encodings
            float freq = 1.0f / powf(10000.0f, (float)(d % (EMBED_DIM/2)) / (EMBED_DIM/2));
            float angle = vocab->vocab_size * freq;
            
            if (d % 2 == 0) {
                vocab->rope_pos_enc[vocab->vocab_size * EMBED_DIM + d] = sinf(angle);
            } else {
                vocab->rope_pos_enc[vocab->vocab_size * EMBED_DIM + d] = cosf(angle);
            }
        }
        
        // Initialize other parameters for special tokens
        vocab->attention_bias[vocab->vocab_size] = 0.0f;
        vocab->ffn_bias[vocab->vocab_size] = 0.0f;
        vocab->weight1[vocab->vocab_size] = 0.1f;
        vocab->weight2[vocab->vocab_size] = 0.1f;
        vocab->bias1[vocab->vocab_size] = 0.0f;
        vocab->bias2[vocab->vocab_size] = 0.0f;
        vocab->bias3[vocab->vocab_size] = 0.0f;
        vocab->bias4[vocab->vocab_size] = 0.0f;
        
        // Initialize Q, K, V projections for special tokens
        for (int d = 0; d < EMBED_DIM; d++) {
            vocab->q_proj[vocab->vocab_size * EMBED_DIM + d] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
            vocab->k_proj[vocab->vocab_size * EMBED_DIM + d] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
            vocab->v_proj[vocab->vocab_size * EMBED_DIM + d] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
        }
        
        // Initialize note for special tokens
        vocab->notes[vocab->vocab_size] = malloc(50 * sizeof(char));  // Allocate space for note
        strcpy(vocab->notes[vocab->vocab_size], "special_token");
        
        vocab->vocab_size++;
    }
    
    printf("Added %d special tokens\n", num_special);
    
    // Add ALL tokens from corpus preserving position information (including duplicates)
    for (int i = 0; i < num_tokens; i++) {
        // Add every token in sequence, preserving duplicates and positional information
        if (vocab->vocab_size < MAX_VOCAB_SIZE) {
            // Add new token (including duplicates)
            vocab->words[vocab->vocab_size] = malloc((strlen(tokens[i]) + 1) * sizeof(char));
            strcpy(vocab->words[vocab->vocab_size], tokens[i]);
            vocab->word_to_id[vocab->vocab_size] = vocab->vocab_size;
            
            // Initialize embedding for this token (deterministic based on word content and position)
            unsigned long hash = 5381;
            int c;
            const char *str = tokens[i];
            while ((c = *str++))
                hash = ((hash << 5) + hash) + c;
            // Include position in the hash to differentiate same tokens at different positions
            hash = hash ^ (i << 16); // XOR with position information
            
            for (int d = 0; d < EMBED_DIM; d++) {
                vocab->embeddings[vocab->vocab_size * EMBED_DIM + d] = 
                    (float)(hash % 1000000) / 1000000.0f + (float)d * 0.001f;
                
                // Initialize RoPE positional encodings based on actual token position
                float freq = 1.0f / powf(10000.0f, (float)(d % (EMBED_DIM/2)) / (EMBED_DIM/2));
                float angle = i * freq;  // Use actual position 'i' instead of vocab_size
                
                if (d % 2 == 0) {
                    vocab->rope_pos_enc[vocab->vocab_size * EMBED_DIM + d] = sinf(angle);
                } else {
                    vocab->rope_pos_enc[vocab->vocab_size * EMBED_DIM + d] = cosf(angle);
                }
            }
            
            // Initialize other parameters for this token
            vocab->attention_bias[vocab->vocab_size] = 0.0f;
            vocab->ffn_bias[vocab->vocab_size] = 0.0f;
            vocab->weight1[vocab->vocab_size] = 0.1f;
            vocab->weight2[vocab->vocab_size] = 0.1f;
            vocab->bias1[vocab->vocab_size] = 0.0f;
            vocab->bias2[vocab->vocab_size] = 0.0f;
            vocab->bias3[vocab->vocab_size] = 0.0f;
            vocab->bias4[vocab->vocab_size] = 0.0f;
            
            // Initialize Q, K, V projections for this token (deterministic based on word content and position)
            unsigned long hash_qkv = 5381;
            int c_qkv;
            const char *str_qkv = tokens[i];
            while ((c_qkv = *str_qkv++))
                hash_qkv = ((hash_qkv << 5) + hash_qkv) + c_qkv;
            // Include position in the hash to differentiate same tokens at different positions
            hash_qkv = hash_qkv ^ (i << 16); // XOR with position information
            
            for (int d = 0; d < EMBED_DIM; d++) {
                vocab->q_proj[vocab->vocab_size * EMBED_DIM + d] = 
                    (float)(hash_qkv % 1000000) / 1000000.0f + (float)d * 0.001f;
                vocab->k_proj[vocab->vocab_size * EMBED_DIM + d] = 
                    (float)(hash_qkv % 1000001) / 1000000.0f + (float)d * 0.001f;  // Slightly different hash
                vocab->v_proj[vocab->vocab_size * EMBED_DIM + d] = 
                    (float)(hash_qkv % 1000002) / 1000000.0f + (float)d * 0.001f;  // Slightly different hash
            }
            
            // Initialize note for this token with positional information
            vocab->notes[vocab->vocab_size] = malloc(50 * sizeof(char));  // Allocate space for note
            char note_str[50];
            snprintf(note_str, sizeof(note_str), "pos_%d", i);
            strcpy(vocab->notes[vocab->vocab_size], note_str);
            
            vocab->vocab_size++;
        }
    }
    
    printf("Final vocabulary size: %d unique tokens\n", vocab->vocab_size);
    return 0; // Success
}

int save_vocab_to_curriculum(SimpleVocab* vocab, const char* corpus_path) {
    printf("Saving vocabulary to curriculum from corpus: %s\n", corpus_path);
    
    // Extract dataset name from corpus path
    char dataset_name[500];
    const char* basename = strrchr(corpus_path, '/');
    if (basename) basename++; // Skip the '/'
    else basename = corpus_path;
    
    // Copy the name without extension
    strcpy(dataset_name, basename);
    char* ext = strrchr(dataset_name, '.');
    if (ext) *ext = '\0'; // Remove extension
    
    // Create curriculum directory path
    char curriculum_dir[1000];
    snprintf(curriculum_dir, sizeof(curriculum_dir), "curriculum/%s", dataset_name);
    
    // Create directory if it doesn't exist
    create_directory(curriculum_dir);
    
    // Create curriculum file path using dynamic allocation
    int path_len = strlen(curriculum_dir) + strlen("/") + strlen(dataset_name) + strlen(".txt") + 1;
    char* curriculum_file = malloc(path_len * sizeof(char));
    snprintf(curriculum_file, path_len, "%s/%s.txt", curriculum_dir, dataset_name);
    
    FILE* file = fopen(curriculum_file, "w");
    if (!file) {
        printf("Error: could not create curriculum file %s\n", curriculum_file);
        return -1;
    }
    
    // Write header with Q, K, V and notes columns added
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
    printf("Successfully saved vocabulary to curriculum file: %s\n", curriculum_file);
    free(curriculum_file); // Free allocated memory
    return 0; // Success
}

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
    vocab->positions = malloc(vocab->vocab_size * sizeof(int));
    
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

int create_directory(const char* path) {
    // Create directory if it doesn't exist (Linux/Unix specific)
    char mkdir_cmd[1024];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", path);
    int result = system(mkdir_cmd);
    if (result == 0) {
        return 0; // Success
    } else {
        printf("Warning: Could not create directory %s\n", path);
        return -1; // Error
    }
}