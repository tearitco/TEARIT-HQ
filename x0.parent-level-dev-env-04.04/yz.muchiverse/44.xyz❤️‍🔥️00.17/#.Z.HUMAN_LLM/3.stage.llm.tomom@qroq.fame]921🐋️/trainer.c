#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <libgen.h>
#include <sys/stat.h>

#define EPSILON 1e-8
#define MAX_GRADIENT_NORM 1.0f

#define MAX_LINE_LENGTH 1024
#define MAX_VOCAB_SIZE 100000
#define EMBEDDING_DIM 7
#define HIDDEN_DIM 16

// --- Configuration Structure ---
typedef struct {
    int epochs;
    float learning_rate;
    float beta1;
    float beta2;
    float max_gradient_norm;
    float attention_dropout;
    float mlp_dropout;
    float attention_noise;
    float attention_weights_noise;
    int causal_attention;
} Config;

// --- Configuration Functions ---
int load_config(Config *config, const char *config_file) {
    FILE *file = fopen(config_file, "r");
    if (!file) {
        fprintf(stderr, "Warning: Could not open config file %s, using defaults\n", config_file);
        // Set default values
        config->epochs = 10;
        config->learning_rate = 0.001f;
        config->beta1 = 0.9f;
        config->beta2 = 0.999f;
        config->max_gradient_norm = 1.0f;
        config->attention_dropout = 0.1f;
        config->mlp_dropout = 0.2f;
        config->attention_noise = 0.01f;
        config->attention_weights_noise = 0.005f;
        config->causal_attention = 0;
        return 0;
    }
    
    char line[1024];
    while (fgets(line, sizeof(line), file)) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        
        // Parse key=value pairs
        char key[100];
        float value;
        if (sscanf(line, "%[^=]=%f", key, &value) == 2) {
            if (strcmp(key, "epochs") == 0) config->epochs = (int)value;
            else if (strcmp(key, "learning_rate") == 0) config->learning_rate = value;
            else if (strcmp(key, "beta1") == 0) config->beta1 = value;
            else if (strcmp(key, "beta2") == 0) config->beta2 = value;
            else if (strcmp(key, "max_gradient_norm") == 0) config->max_gradient_norm = value;
            else if (strcmp(key, "attention_dropout") == 0) config->attention_dropout = value;
            else if (strcmp(key, "mlp_dropout") == 0) config->mlp_dropout = value;
            else if (strcmp(key, "attention_noise") == 0) config->attention_noise = value;
            else if (strcmp(key, "attention_weights_noise") == 0) config->attention_weights_noise = value;
            else if (strcmp(key, "causal_attention") == 0) config->causal_attention = (int)value;
        }
    }
    
    fclose(file);
    return 1;
}

void print_config(Config *config) {
    fprintf(stderr, "Training configuration:\n");
    fprintf(stderr, "  Epochs: %d\n", config->epochs);
    fprintf(stderr, "  Learning rate: %f\n", config->learning_rate);
    fprintf(stderr, "  Beta1: %f\n", config->beta1);
    fprintf(stderr, "  Beta2: %f\n", config->beta2);
    fprintf(stderr, "  Max gradient norm: %f\n", config->max_gradient_norm);
    fprintf(stderr, "  Attention dropout: %f\n", config->attention_dropout);
    fprintf(stderr, "  MLP dropout: %f\n", config->mlp_dropout);
    fprintf(stderr, "  Attention noise: %f\n", config->attention_noise);
    fprintf(stderr, "  Attention weights noise: %f\n", config->attention_weights_noise);
    fprintf(stderr, "  Causal attention: %s\n", config->causal_attention ? "enabled" : "disabled");
}

// --- Data Structures ---
struct VocabEntry { int number; char word[100]; float embedding, pe, weight, bias1, bias2, bias3, bias4; };

// --- Utility Functions ---
void write_loss(float loss, const char *output_dir) { char loss_path[1024]; sprintf(loss_path, "%s/loss.txt", output_dir); FILE *file = fopen(loss_path, "a"); if (file) { fprintf(file, "%.9g\n", loss); fclose(file); } }
int save_vocab(struct VocabEntry *vocab, int vocab_size, const char *filename) { FILE *outfile = fopen(filename, "w"); if (!outfile) { perror("Failed to open output file"); return 0; } fprintf(outfile, "number word embedding pe weight bias1 bias2 bias3 bias4\n"); for (int i = 0; i < vocab_size; i++) { fprintf(outfile, "%d %s %f %f %f %f %f %f %f\n", vocab[i].number, vocab[i].word, vocab[i].embedding, vocab[i].pe, vocab[i].weight, vocab[i].bias1, vocab[i].bias2, vocab[i].bias3, vocab[i].bias4); } fclose(outfile); return 1; }
int load_vocab(struct VocabEntry *vocab, int max_vocab_size, const char *filename, int *vocab_size) { 
    FILE *infile = fopen(filename, "r"); 
    if (!infile) { 
        perror("Error opening vocab file"); 
        return 0; 
    } 
    char line[MAX_LINE_LENGTH]; 
    *vocab_size = 0; 
    fgets(line, sizeof(line), infile); 
    while (fgets(line, sizeof(line), infile) && *vocab_size < max_vocab_size) { 
        int result = sscanf(line, "%d %s %f %f %f %f %f %f %f", 
                          &vocab[*vocab_size].number, vocab[*vocab_size].word, 
                          &vocab[*vocab_size].embedding, &vocab[*vocab_size].pe, 
                          &vocab[*vocab_size].weight, &vocab[*vocab_size].bias1, 
                          &vocab[*vocab_size].bias2, &vocab[*vocab_size].bias3, 
                          &vocab[*vocab_size].bias4);
        if (result == 9) { // Successfully parsed all fields
            (*vocab_size)++;
        }
    } 
    fclose(infile); 
    return 1; 
}
float compute_cross_entropy_loss_and_gradient(float *predictions, int target_index, int size, float *grad) { 
    // Check for NaN values in predictions
    for (int i = 0; i < size; i++) {
        if (isnan(predictions[i]) || isinf(predictions[i])) {
            fprintf(stderr, "Warning: NaN or Inf detected in predictions at index %d\n", i);
            // Reset to small values
            predictions[i] = 0.0f;
        }
    }
    
    float *probs = malloc(size * sizeof(float)); 
    float max_val = predictions[0]; 
    for (int i = 1; i < size; i++) { 
        if (predictions[i] > max_val) max_val = predictions[i]; 
    } 
    float sum = 0.0f; 
    for (int i = 0; i < size; i++) { 
        probs[i] = expf(predictions[i] - max_val); 
        // Prevent overflow
        if (isinf(probs[i]) || isnan(probs[i])) {
            probs[i] = 0.0f;
        }
        sum += probs[i]; 
    } 
    // Handle case where sum is zero
    if (sum < 1e-12f) {
        for (int i = 0; i < size; i++) {
            probs[i] = 1.0f / size;
        }
        sum = 1.0f;
    }
    for (int i = 0; i < size; i++) { 
        probs[i] /= sum; 
        grad[i] = probs[i]; 
    } 
    grad[target_index] -= 1.0f; 
    float loss = -logf(probs[target_index] + EPSILON); 
    free(probs); 
    return loss; 
}

// --- Main Training Logic ---
void train_model(struct VocabEntry *vocab, int vocab_size, const char *vocab_filename) {
    fprintf(stderr, "Training model...\n");
    
    // Load configuration
    Config config;
    char config_path[1024];
    strcpy(config_path, vocab_filename);
    char *dir = dirname(config_path);
    sprintf(config_path, "%s/config.txt", dir);
    load_config(&config, config_path);
    print_config(&config);
    
    char *output_dir = dirname(strdup(vocab_filename));
    char *cmd = NULL;  // Will use dynamic allocation
    int cmd_len;       // For dynamic command string length calculation
    char pth[1024];
    char attn_path[1024], mlp_path[1024], optim_path[1024], out_path[1024];
    sprintf(attn_path, "%s/attention_model.txt", output_dir); sprintf(mlp_path, "%s/mlp_model.txt", output_dir);
    sprintf(optim_path, "%s/optimizer_state.txt", output_dir); sprintf(out_path, "%s/output_layer.txt", output_dir);

    // Use dynamic allocation for command string
        free(cmd);
        cmd_len = snprintf(NULL, 0, "./+x/optimizer.+x adam-init %s %s %d", optim_path, output_dir, vocab_size);
        cmd = malloc(cmd_len + 1);
        if (cmd) {
            snprintf(cmd, cmd_len + 1, "./+x/optimizer.+x adam-init %s %s %d", optim_path, output_dir, vocab_size);
        }
    if (system(cmd) != 0) { fprintf(stderr, "Failed to initialize optimizer\n"); return; }
    
    // Update optimizer state with config values
    FILE *sf = fopen(optim_path, "w");
    if (sf) {
        fprintf(sf, "%.9g %.9g %.9g 0", config.learning_rate, config.beta1, config.beta2);
        fclose(sf);
    }

    // Initialize model matrices if they don't exist yet (keep human-readable text format)
    struct stat mst = {0};
    if (stat(attn_path, &mst) == -1) {
        char init_cmd[1024]; sprintf(init_cmd, "./+x/attention.+x init %s", attn_path);
        if (system(init_cmd) != 0) { fprintf(stderr, "Failed to init attention\n"); return; }
    }
    if (stat(mlp_path, &mst) == -1) {
        char init_cmd[1024]; sprintf(init_cmd, "./+x/mlp_layer.+x init %s", mlp_path);
        if (system(init_cmd) != 0) { fprintf(stderr, "Failed to init mlp\n"); return; }
    }
    if (stat(out_path, &mst) == -1) {
        char init_cmd[1024]; sprintf(init_cmd, "./+x/optimizer.+x output-init %s %d", out_path, vocab_size);
        if (system(init_cmd) != 0) { fprintf(stderr, "Failed to init output\n"); return; }
    }

    for (int epoch = 0; epoch < config.epochs; epoch++) {
        float total_loss = 0.0f;
        for (int i = 0; i < vocab_size - 1; i++) {
            fprintf(stderr, "\rEpoch %d/%d, Word %d/%d", epoch + 1, config.epochs, i + 1, vocab_size - 1);
            // Use dynamic allocation for command string
            free(cmd);
            cmd_len = snprintf(NULL, 0, "./+x/forward_prop.+x %s %d %s %s %s %d", vocab_filename, i, attn_path, mlp_path, out_path, config.causal_attention);
            cmd = malloc(cmd_len + 1);
            if (cmd) {
                snprintf(cmd, cmd_len + 1, "./+x/forward_prop.+x %s %d %s %s %s %d", vocab_filename, i, attn_path, mlp_path, out_path, config.causal_attention);
            }
            if (system(cmd) != 0) { fprintf(stderr, "\nForward prop failed\n"); continue; }

            char pred_path[1024], gloss_path[1024], hidden_path[1024], ctx_path[1024], q_path[1024], k_path[1024], v_path[1024], asr_path[1024];
            sprintf(pred_path, "%s/predictions.txt", output_dir); sprintf(gloss_path, "%s/grad_loss.txt", output_dir);
            sprintf(hidden_path, "%s/hidden_state.txt", output_dir); sprintf(ctx_path, "%s/context.txt", output_dir);
            sprintf(q_path, "%s/q.txt", output_dir); sprintf(k_path, "%s/k.txt", output_dir); sprintf(v_path, "%s/v.txt", output_dir);
            sprintf(asr_path, "%s/attn_scores_raw.txt", output_dir);

            FILE *pred_f = fopen(pred_path, "r"); if (!pred_f) { fprintf(stderr, "\nNo predictions file.\n"); continue; }
            float *preds = malloc(vocab_size * sizeof(float)); for(int j=0; j<vocab_size; j++) fscanf(pred_f, "%f", &preds[j]); fclose(pred_f);
            float *grad = malloc(vocab_size * sizeof(float));
            float loss = compute_cross_entropy_loss_and_gradient(preds, i + 1, vocab_size, grad);
            // Check for NaN loss
            if (isnan(loss) || isinf(loss)) {
                fprintf(stderr, "\nWarning: NaN or Inf loss detected, setting to 0\n");
                loss = 0.0f;
                // Reset gradients to zero
                for (int j = 0; j < vocab_size; j++) {
                    grad[j] = 0.0f;
                }
            }
            total_loss += loss;
            FILE *gloss_f = fopen(gloss_path, "w"); if(gloss_f){ for(int j=0; j<vocab_size; j++) fprintf(gloss_f, "%.9g ", grad[j]); fclose(gloss_f); }

            // Use dynamic allocation for command string
            free(cmd);
            cmd_len = snprintf(NULL, 0, "./+x/backward_prop.+x %s %d %s %s %s %s %s %s %s %s %s %s", vocab_filename, i, gloss_path, attn_path, mlp_path, out_path, hidden_path, ctx_path, q_path, k_path, v_path, asr_path);
            cmd = malloc(cmd_len + 1);
            if (cmd) {
                snprintf(cmd, cmd_len + 1, "./+x/backward_prop.+x %s %d %s %s %s %s %s %s %s %s %s %s", vocab_filename, i, gloss_path, attn_path, mlp_path, out_path, hidden_path, ctx_path, q_path, k_path, v_path, asr_path);
            }
            if (system(cmd) != 0) { fprintf(stderr, "\nBackward prop failed\n"); continue; }

            char grad_prefix[1024]; sprintf(grad_prefix, "%s/grad", output_dir);
            // Use dynamic allocation for command string
            free(cmd);
            cmd_len = snprintf(NULL, 0, "./+x/optimizer.+x update %d %s %s %s %s %s", vocab_size, optim_path, attn_path, mlp_path, out_path, grad_prefix);
            cmd = malloc(cmd_len + 1);
            if (cmd) {
                snprintf(cmd, cmd_len + 1, "./+x/optimizer.+x update %d %s %s %s %s %s", vocab_size, optim_path, attn_path, mlp_path, out_path, grad_prefix);
            }
            if (system(cmd) != 0) { fprintf(stderr, "\nOptimizer update failed\n"); continue; }

            free(preds); free(grad);
        }
        write_loss(total_loss / (vocab_size - 1), output_dir);
        fprintf(stderr, "\nEpoch %d/%d, Loss: %f\n", epoch + 1, config.epochs, total_loss / (vocab_size - 1));
    }
    fprintf(stderr, "Training complete.\n");
    
    // Free dynamically allocated command buffer
    free(cmd);
}

int main(int argc, char *argv[]) {
    if (argc < 2) { fprintf(stderr, "Usage: %s <vocab_model.txt>\n", argv[0]); return 1; }
    
    // Dynamically allocate vocab array
    struct VocabEntry *vocab = NULL;
    int vocab_size = 0;
    int capacity = 1000; // Start with reasonable capacity
    vocab = malloc(capacity * sizeof(struct VocabEntry));
    if (!vocab) {
        fprintf(stderr, "Failed to allocate memory for vocab\n");
        return 1;
    }
    
    // Load vocab with dynamic resizing
    FILE *infile = fopen(argv[1], "r");
    if (!infile) { 
        perror("Error opening vocab file"); 
        free(vocab);
        return 1; 
    }
    
    char line[MAX_LINE_LENGTH];
    fgets(line, sizeof(line), infile); // Skip header
    
    while (fgets(line, sizeof(line), infile)) {
        // Expand array if needed
        if (vocab_size >= capacity) {
            capacity *= 2;
            struct VocabEntry *temp = realloc(vocab, capacity * sizeof(struct VocabEntry));
            if (!temp) {
                fprintf(stderr, "Failed to reallocate memory for vocab\n");
                free(vocab);
                fclose(infile);
                return 1;
            }
            vocab = temp;
        }
        
        // Parse line
        int result = sscanf(line, "%d %s %f %f %f %f %f %f %f",
                           &vocab[vocab_size].number, vocab[vocab_size].word, 
                           &vocab[vocab_size].embedding, &vocab[vocab_size].pe, 
                           &vocab[vocab_size].weight, &vocab[vocab_size].bias1, 
                           &vocab[vocab_size].bias2, &vocab[vocab_size].bias3, 
                           &vocab[vocab_size].bias4);
        if (result == 9) { // Successfully parsed all fields
            vocab_size++;
        }
    }
    fclose(infile);
    
    srand(time(NULL));
    train_model(vocab, vocab_size, argv[1]);
    
    // Save the updated vocab
    if (!save_vocab(vocab, vocab_size, argv[1])) { 
        fprintf(stderr, "Failed to save updated vocab to %s\n", argv[1]); 
    }
    
    // Note: OS will automatically free memory when process exits
    fprintf(stderr, "Vocab model updated in %s\n", argv[1]);
    return 0;
}