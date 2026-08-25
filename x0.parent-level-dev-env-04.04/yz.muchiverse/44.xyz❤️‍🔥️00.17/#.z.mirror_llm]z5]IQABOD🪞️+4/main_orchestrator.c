#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
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
    int vocab_size;
    int max_size;
} SimpleVocab;

// Configuration structure
typedef struct {
    float learning_rate;
    int epochs;
    int batch_size;
    int max_seq_len;
    int dim;
    int n_layers;
    int n_heads;
    float temperature;
    float top_p;
    int max_tokens;
} Config;

// Function declarations
int read_corpus_tokens(const char* filename, char*** tokens_output, int* num_tokens);
int generate_vocab_from_tokens(char** tokens, int num_tokens, SimpleVocab* vocab);
int save_vocab_to_curriculum(SimpleVocab* vocab, const char* corpus_path);
int create_directory(const char* path);

// Function to load configuration from file
Config load_config(const char* config_file) {
    Config conf = {0};  // Initialize all fields to 0 first
    
    // Set default values
    conf.learning_rate = 0.0001f;  // Lower default learning rate
    conf.epochs = 3;
    conf.batch_size = 1;
    conf.max_seq_len = 32;
    conf.dim = 16;
    conf.n_layers = 2;
    conf.n_heads = 4;
    conf.temperature = 1.0f;
    conf.top_p = 0.9f;
    conf.max_tokens = 100;
    
    // Try to open config file
    FILE* file = fopen(config_file, "r");
    if (!file) {
        printf("Warning: Could not open config file %s, using default values\n", config_file);
        return conf;
    }
    
    char line[500];
    while (fgets(line, sizeof(line), file) != NULL) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        
        // Parse key=value pairs
        char key[100];
        char value_str[100];
        if (sscanf(line, "%99[^=]=%99s", key, value_str) == 2) {
            float value = atof(value_str);
            int ivalue = atoi(value_str);
            
            if (strcmp(key, "learning_rate") == 0) conf.learning_rate = value;
            else if (strcmp(key, "epochs") == 0) conf.epochs = ivalue;
            else if (strcmp(key, "batch_size") == 0) conf.batch_size = ivalue;
            else if (strcmp(key, "max_seq_len") == 0) conf.max_seq_len = ivalue;
            else if (strcmp(key, "dim") == 0) conf.dim = ivalue;
            else if (strcmp(key, "n_layers") == 0) conf.n_layers = ivalue;
            else if (strcmp(key, "n_heads") == 0) conf.n_heads = ivalue;
            else if (strcmp(key, "temperature") == 0) conf.temperature = value;
            else if (strcmp(key, "top_p") == 0) conf.top_p = value;
            else if (strcmp(key, "max_tokens") == 0) conf.max_tokens = ivalue;
        }
    }
    
    fclose(file);
    printf("Loaded configuration: lr=%.6f, epochs=%d, batch_size=%d\n", 
           conf.learning_rate, conf.epochs, conf.batch_size);
    return conf;
}

void print_usage(char* program_name) {
    printf("Usage: %s <mode> [arguments...]\n", program_name);
    printf("Modes:\n");
    printf("  vocab_only [corpus_file] - Generate vocabulary from corpus and save to curriculum\n");
    printf("  train [corpus_file] [epochs] - Train model using corpus (epochs optional, default=3)\n");
    printf("  generate [curriculum_file] [temperature] [limit] [prompt] - Generate text using trained model\n");
    printf("\nExamples:\n");
    printf("  %s vocab_only corpuses/sample.txt\n", program_name);
    printf("  %s train corpuses/sample.txt\n", program_name);
    printf("  %s train corpuses/sample.txt 5\n", program_name);
    printf("  %s generate curriculum/sample/sample.txt 1.0 30 \"test generation\"\n", program_name);
}

// Token processing function to convert string to tokens
int process_prompt_tokens(const char* prompt, char*** tokens_output, int* num_tokens_output) {
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
    char** tokens = malloc(num_prompt_tokens * sizeof(char*));
    strcpy(temp_tokenizer = malloc(strlen(prompt) + 1), prompt);
    token_check = strtok(temp_tokenizer, " \n\t");
    int prompt_token_idx = 0;
    while (token_check != NULL && prompt_token_idx < num_prompt_tokens) {
        tokens[prompt_token_idx] = malloc((strlen(token_check) + 1) * sizeof(char));
        strcpy(tokens[prompt_token_idx], token_check);
        prompt_token_idx++;
        token_check = strtok(NULL, " \n\t");
    }
    free(temp_tokenizer);
    
    *tokens_output = tokens;
    *num_tokens_output = num_prompt_tokens;
    
    return 0; // Success
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    char* mode = argv[1];
    char* corpus_file = NULL;
    
    // Load configuration file for defaults
    Config conf = load_config("config.txt");
    int epochs = conf.epochs;  // Use epochs from config as default
    
    if (strcmp(mode, "vocab_only") == 0) {
        if (argc < 3) {
            printf("Error: Missing corpus file for vocab_only mode\n");
            print_usage(argv[0]);
            return 1;
        }
        corpus_file = argv[2];
        
        printf("Mode: Vocabulary Generation Only\n");
        
        // Read corpus tokens
        char** tokens;
        int num_tokens;
        int result = read_corpus_tokens(corpus_file, &tokens, &num_tokens);
        if (result != 0) {
            printf("Error reading corpus tokens\n");
            return 1;
        }
        
        // Generate vocabulary
        SimpleVocab vocab;
        result = generate_vocab_from_tokens(tokens, num_tokens, &vocab);
        if (result != 0) {
            printf("Error generating vocabulary\n");
            // Free tokens before returning
            for (int i = 0; i < num_tokens; i++) {
                free(tokens[i]);
            }
            free(tokens);
            return 1;
        }
        
        // Save vocabulary to curriculum
        result = save_vocab_to_curriculum(&vocab, corpus_file);
        if (result != 0) {
            printf("Error saving vocabulary to curriculum\n");
            // Free memory
            for (int i = 0; i < vocab.vocab_size; i++) {
                free(vocab.words[i]);
            }
            free(vocab.words);
            free(vocab.word_to_id);
            free(vocab.embeddings);
            free(vocab.rope_pos_enc);
            free(vocab.weight1);
            free(vocab.weight2);
            free(vocab.bias1);
            free(vocab.bias2);
            free(vocab.bias3);
            free(vocab.bias4);
            free(vocab.attention_bias);
            free(vocab.ffn_bias);
            free(vocab.q_proj);
            free(vocab.k_proj);
            free(vocab.v_proj);
            free(vocab.notes);
            
            // Free tokens
            for (int i = 0; i < num_tokens; i++) {
                free(tokens[i]);
            }
            free(tokens);
            return 1;
        }
        
        printf("Vocabulary generation completed. Generated %d tokens.\n", vocab.vocab_size);
        
        // Free memory
        for (int i = 0; i < vocab.vocab_size; i++) {
            free(vocab.words[i]);
        }
        free(vocab.words);
        free(vocab.word_to_id);
        free(vocab.embeddings);
        free(vocab.rope_pos_enc);
        free(vocab.weight1);
        free(vocab.weight2);
        free(vocab.bias1);
        free(vocab.bias2);
        free(vocab.bias3);
        free(vocab.bias4);
        free(vocab.attention_bias);
        free(vocab.ffn_bias);
        free(vocab.q_proj);
        free(vocab.k_proj);
        free(vocab.v_proj);
        free(vocab.notes);
        
        // Free tokens
        for (int i = 0; i < num_tokens; i++) {
            free(tokens[i]);
        }
        free(tokens);
        
    } else if (strcmp(mode, "train") == 0) {
        if (argc < 3) {
            printf("Error: Missing corpus file for train mode\n");
            print_usage(argv[0]);
            return 1;
        }
        corpus_file = argv[2];
        
        // Optional epochs parameter
        if (argc >= 4) {
            epochs = atoi(argv[3]);
        }
        if (epochs <= 0) epochs = 3;  // Default if invalid
        
        printf("Mode: Training for %d epochs\n", epochs);
        
        // For training, we use the pre-existing curriculum file that already has duplicates preserved
        // Extract dataset name from corpus path to find the curriculum file
        char dataset_name[500];
        const char* basename = strrchr(corpus_file, '/');
        if (basename) basename++; // Skip the '/'
        else basename = corpus_file;
        
        // Copy the name without extension
        strcpy(dataset_name, basename);
        char* ext = strrchr(dataset_name, '.');
        if (ext) *ext = '\0'; // Remove extension
        
        // Build the curriculum file path
        char curriculum_file[1000];
        snprintf(curriculum_file, sizeof(curriculum_file), "curriculum/%s/%s.txt", dataset_name, dataset_name);
        
        printf("Using existing curriculum file: %s\n", curriculum_file);
        printf("Note: Curriculum already contains duplicates as required\n");
        
        // Check if curriculum file exists
        FILE* test_file = fopen(curriculum_file, "r");
        if (!test_file) {
            printf("Error: Curriculum file does not exist: %s\n", curriculum_file);
            printf("Please run 'vocab_only' mode first to generate the curriculum with duplicates preserved\n");
            return 1;
        }
        fclose(test_file);
        
        // Use generation module for training (using existing curriculum with preserved duplicates)
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "./+x/generation_module.+x train %s %d %f", curriculum_file, epochs, conf.learning_rate);
        int result = system(cmd);
        if (result != 0) {
            printf("Error during training with generation module\n");
            return result;
        }
        
        printf("Training completed. Check curriculum directory for updated model.\n");
        
    } else if (strcmp(mode, "generate") == 0) {
        if (argc < 6) {
            printf("Error: Missing required parameters for generate mode\n");
            print_usage(argv[0]);
            return 1;
        }
        
        // For generate mode, we expect: generate [curriculum_file] [temperature] [limit] [prompt]
        char* curriculum_file = argv[2];
        float temperature = atof(argv[3]);
        int max_tokens = atoi(argv[4]);
        char* prompt = argv[5];
        
        printf("Mode: Generation with temperature=%.2f, max_tokens=%d, prompt='%s'\n", 
               temperature, max_tokens, prompt);
        
        // For generation, use the specialized generation module that handles dynamic vocabulary expansion
        char cmd[1000];
        snprintf(cmd, sizeof(cmd), "./+x/generation_module.+x generate %s %f %f %d \"%s\"", 
                 curriculum_file, temperature, conf.top_p, max_tokens, prompt);
        int result = system(cmd);
        if (result != 0) {
            printf("Error during text generation\n");
            return result;
        }
        
    } else {
        printf("Error: Unknown mode '%s'\n", mode);
        print_usage(argv[0]);
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
    
    if (!vocab->words || !vocab->word_to_id || !vocab->embeddings || !vocab->rope_pos_enc ||
        !vocab->weight1 || !vocab->weight2 || !vocab->bias1 || !vocab->bias2 ||
        !vocab->bias3 || !vocab->bias4 || !vocab->attention_bias || !vocab->ffn_bias ||
        !vocab->q_proj || !vocab->k_proj || !vocab->v_proj || !vocab->notes) {
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
    
    // Add tokens from corpus
    for (int i = 0; i < num_tokens; i++) {
        // Check if token already exists
        int exists = 0;
        for (int j = 0; j < vocab->vocab_size; j++) {
            if (strcmp(vocab->words[j], tokens[i]) == 0) {
                exists = 1;
                break;
            }
        }
        
        if (!exists && vocab->vocab_size < MAX_VOCAB_SIZE) {
            // Add new token
            vocab->words[vocab->vocab_size] = malloc((strlen(tokens[i]) + 1) * sizeof(char));
            strcpy(vocab->words[vocab->vocab_size], tokens[i]);
            vocab->word_to_id[vocab->vocab_size] = vocab->vocab_size;
            
            // Initialize embedding for this token (deterministic based on word content)
            unsigned long hash = 5381;
            int c;
            const char *str = tokens[i];
            while ((c = *str++))
                hash = ((hash << 5) + hash) + c;
            
            for (int d = 0; d < EMBED_DIM; d++) {
                vocab->embeddings[vocab->vocab_size * EMBED_DIM + d] = 
                    (float)(hash % 1000000) / 1000000.0f + (float)d * 0.001f;
                
                // Initialize RoPE positional encodings
                float freq = 1.0f / powf(10000.0f, (float)(d % (EMBED_DIM/2)) / (EMBED_DIM/2));
                float angle = vocab->vocab_size * freq;
                
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
            
            // Initialize Q, K, V projections for this token (deterministic based on word content)
            unsigned long hash_qkv = 5381;
            int c_qkv;
            const char *str_qkv = tokens[i];
            while ((c_qkv = *str_qkv++))
                hash_qkv = ((hash_qkv << 5) + hash_qkv) + c_qkv;
            
            for (int d = 0; d < EMBED_DIM; d++) {
                vocab->q_proj[vocab->vocab_size * EMBED_DIM + d] = 
                    (float)(hash_qkv % 1000000) / 1000000.0f + (float)d * 0.001f;
                vocab->k_proj[vocab->vocab_size * EMBED_DIM + d] = 
                    (float)(hash_qkv % 1000001) / 1000000.0f + (float)d * 0.001f;  // Slightly different hash
                vocab->v_proj[vocab->vocab_size * EMBED_DIM + d] = 
                    (float)(hash_qkv % 1000002) / 1000000.0f + (float)d * 0.001f;  // Slightly different hash
            }
            
            // Initialize note for this token
            vocab->notes[vocab->vocab_size] = malloc(50 * sizeof(char));  // Allocate space for note
            strcpy(vocab->notes[vocab->vocab_size], "auto_generated");
            
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