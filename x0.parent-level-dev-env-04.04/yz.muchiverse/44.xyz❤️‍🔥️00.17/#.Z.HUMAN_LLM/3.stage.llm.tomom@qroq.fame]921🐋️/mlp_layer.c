#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <libgen.h>

#define EMBEDDING_DIM 7
#define HIDDEN_DIM 16

// MLP Layer
typedef struct {
    float weights[EMBEDDING_DIM][HIDDEN_DIM];
    float biases[HIDDEN_DIM];
} MlpLayer;

// Function to initialize MLP layer with random weights
void initialize_mlp(MlpLayer *mlp) {
    for (int i = 0; i < EMBEDDING_DIM; i++) {
        for (int j = 0; j < HIDDEN_DIM; j++) {
            mlp->weights[i][j] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
        }
    }
    for (int i = 0; i < HIDDEN_DIM; i++) {
        mlp->biases[i] = 0.0f;
    }
}

// ReLU activation function
void relu(float *x, int size) {
    for (int i = 0; i < size; i++) {
        if (x[i] < 0) {
            x[i] = 0;
        }
    }
}

// Forward propagation through MLP
void mlp_forward(float *input, MlpLayer *mlp, float *output) {
    for (int j = 0; j < HIDDEN_DIM; j++) {
        output[j] = 0;
        for (int i = 0; i < EMBEDDING_DIM; i++) {
            output[j] += input[i] * mlp->weights[i][j];
        }
        output[j] += mlp->biases[j];
    }
    relu(output, HIDDEN_DIM);
}

// Backward propagation through MLP
void mlp_backward(float *input, float *grad_output, MlpLayer *mlp, float learning_rate) {
    // Update weights and biases
    for (int i = 0; i < EMBEDDING_DIM; i++) {
        for (int j = 0; j < HIDDEN_DIM; j++) {
            // Gradient of ReLU is 1 for positive values, 0 for negative
            float grad_relu = (input[i] > 0) ? 1.0f : 0.0f;
            mlp->weights[i][j] -= learning_rate * grad_output[j] * input[i] * grad_relu;
        }
    }
    
    for (int j = 0; j < HIDDEN_DIM; j++) {
        mlp->biases[j] -= learning_rate * grad_output[j];
    }
}

// Function to save MLP to file
void save_mlp(const char *filename, MlpLayer *mlp) {
    FILE *file = fopen(filename, "w");
    if (!file) {
        perror("Error opening MLP file for writing");
        return;
    }
    
    for (int i = 0; i < EMBEDDING_DIM; i++) {
        for (int j = 0; j < HIDDEN_DIM; j++) {
            fprintf(file, "%f ", mlp->weights[i][j]);
        }
        fprintf(file, "\n");
    }
    
    for (int i = 0; i < HIDDEN_DIM; i++) {
        fprintf(file, "%f ", mlp->biases[i]);
    }
    fprintf(file, "\n");
    
    fclose(file);
}

// Function to load MLP from file
void load_mlp(const char *filename, MlpLayer *mlp) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        // Initialize with random weights if file doesn't exist
        initialize_mlp(mlp);
        return;
    }
    
    for (int i = 0; i < EMBEDDING_DIM; i++) {
        for (int j = 0; j < HIDDEN_DIM; j++) {
            fscanf(file, "%f", &mlp->weights[i][j]);
        }
    }
    
    for (int i = 0; i < HIDDEN_DIM; i++) {
        fscanf(file, "%f", &mlp->biases[i]);
    }
    
    fclose(file);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <operation> [args...]\n", argv[0]);
        fprintf(stderr, "Operations:\n");
        fprintf(stderr, "  init <model_file>                  - Initialize MLP and save to file\n");
        fprintf(stderr, "  forward <model_file> <input_file>  - Forward pass using model and input\n");
        fprintf(stderr, "  backward <model_file> <grad_file>  - Backward pass using gradients\n");
        return 1;
    }
    
    char *operation = argv[1];
    MlpLayer mlp;
    
    if (strcmp(operation, "init") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s init <model_file>\n", argv[0]);
            return 1;
        }
        
        srand(time(NULL));
        initialize_mlp(&mlp);
        save_mlp(argv[2], &mlp);
        printf("MLP initialized and saved to %s\n", argv[2]);
        
    } else if (strcmp(operation, "forward") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: %s forward <model_file> <input_file>\n", argv[0]);
            return 1;
        }
        
        load_mlp(argv[2], &mlp);

        // Path generation logic
        char *model_path = argv[2];
        char *model_path_copy = strdup(model_path);
        char *output_dir = dirname(model_path_copy);

        char mlp_output_path[1024];
        sprintf(mlp_output_path, "%s/mlp_output.txt", output_dir);
        // End of path generation logic
        
        // Load input from file
        FILE *input_file = fopen(argv[3], "r");
        if (!input_file) {
            perror("Error opening input file");
            return 1;
        }
        
        float input[EMBEDDING_DIM];
        for (int i = 0; i < EMBEDDING_DIM; i++) {
            fscanf(input_file, "%f", &input[i]);
        }
        fclose(input_file);
        
        // Forward pass
        float output[HIDDEN_DIM];
        mlp_forward(input, &mlp, output);
        
        // Save output to file
        FILE *output_file = fopen(mlp_output_path, "w");
        if (output_file) {
            for (int i = 0; i < HIDDEN_DIM; i++) {
                fprintf(output_file, "%f\n", output[i]);
            }
            fclose(output_file);
            printf("Forward pass completed. Output saved to %s\n", mlp_output_path);
        } else {
            perror("Error opening MLP output file for writing");
        }
        free(model_path_copy);
        
    } else if (strcmp(operation, "backward") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: %s backward <model_file> <grad_file>\n", argv[0]);
            return 1;
        }
        
        load_mlp(argv[2], &mlp);
        
        // Load gradients from file
        FILE *grad_file = fopen(argv[3], "r");
        if (!grad_file) {
            perror("Error opening gradient file");
            return 1;
        }
        
        float input[EMBEDDING_DIM]; // Would normally be loaded from a file
        float grad_output[HIDDEN_DIM];
        for (int i = 0; i < HIDDEN_DIM; i++) {
            fscanf(grad_file, "%f", &grad_output[i]);
        }
        fclose(grad_file);
        
        // Backward pass
        float learning_rate = 0.01f;
        mlp_backward(input, grad_output, &mlp, learning_rate);
        
        // Save updated model
        save_mlp(argv[2], &mlp);
        printf("Backward pass completed. Model updated in %s\n", argv[2]);
        
    } else {
        fprintf(stderr, "Unknown operation: %s\n", operation);
        return 1;
    }
    
    return 0;
}
