#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define MAX_CURRICULA 20
#define MAX_PROMPT_LENGTH 1024
#define MAX_FEATURES 50
#define LEARNING_RATE 0.1f

// Structure to hold curriculum information
struct Curriculum {
    char path[1024];
    char name[256];
    float weight;  // RL weight for selection
    float bias;    // RL bias for selection
};

// Structure to hold prompt features
struct PromptFeatures {
    int word_count;
    int has_question;
    int has_exclamation;
    int has_emoji;
    int has_greeting;
    int has_emotion;
    int length;
    // Add more features as needed
};

// Global variables
struct Curriculum available_curricula[MAX_CURRICULA];
int num_available_curricula = 0;
struct PromptFeatures current_features;
float rl_weights[MAX_FEATURES];
float rl_bias;

// Function to initialize available curricula
void initialize_curricula() {
    // Add available curricula with initial weights and biases
    // Only use proper curriculum directories, not corpuses
    strcpy(available_curricula[0].path, "./curriculum/corpus]similarity10/corpus]similarity10.txt");
    strcpy(available_curricula[0].name, "corpus_similarity10");
    available_curricula[0].weight = 0.5f;
    available_curricula[0].bias = 0.0f;
    
    strcpy(available_curricula[1].path, "./curriculum/test_emoji/test_emoji.txt");
    strcpy(available_curricula[1].name, "test_emoji");
    available_curricula[1].weight = 0.5f;
    available_curricula[1].bias = 0.0f;
    
    num_available_curricula = 2;
    
    // Initialize RL weights and bias
    for (int i = 0; i < MAX_FEATURES; i++) {
        rl_weights[i] = 0.0f;
    }
    rl_bias = 0.0f;
}

// Function to extract features from prompt
void extract_prompt_features(const char* prompt) {
    current_features.word_count = 0;
    current_features.has_question = 0;
    current_features.has_exclamation = 0;
    current_features.has_emoji = 0;
    current_features.has_greeting = 0;
    current_features.has_emotion = 0;
    current_features.length = strlen(prompt);
    
    // Count words
    char* prompt_copy = strdup(prompt);
    char* token = strtok(prompt_copy, " ");
    while (token != NULL) {
        current_features.word_count++;
        token = strtok(NULL, " ");
    }
    free(prompt_copy);
    
    // Check for question mark
    if (strchr(prompt, '?') != NULL) {
        current_features.has_question = 1;
    }
    
    // Check for exclamation mark
    if (strchr(prompt, '!') != NULL) {
        current_features.has_exclamation = 1;
    }
    
    // Simple emoji detection (very basic)
    if (strchr(prompt, ':') != NULL || strchr(prompt, ';') != NULL) {
        current_features.has_emoji = 1;
    }
    
    // Check for greetings
    if (strstr(prompt, "hello") || strstr(prompt, "hi") || strstr(prompt, "hey")) {
        current_features.has_greeting = 1;
    }
    
    // Check for emotional words (very basic)
    if (strstr(prompt, "happy") || strstr(prompt, "sad") || strstr(prompt, "angry") || 
        strstr(prompt, "excited") || strstr(prompt, "love") || strstr(prompt, "hate")) {
        current_features.has_emotion = 1;
    }
}

// Function to calculate selection score for a curriculum
float calculate_selection_score(int curriculum_index) {
    float score = 0.0f;
    
    // Simple linear combination of features and weights
    score += current_features.word_count * rl_weights[0];
    score += current_features.has_question * rl_weights[1];
    score += current_features.has_exclamation * rl_weights[2];
    score += current_features.has_emoji * rl_weights[3];
    score += current_features.has_greeting * rl_weights[4];
    score += current_features.has_emotion * rl_weights[5];
    score += current_features.length * rl_weights[6];
    
    // Add curriculum-specific weight and bias
    score += available_curricula[curriculum_index].weight + available_curricula[curriculum_index].bias;
    
    // Add global bias
    score += rl_bias;
    
    return score;
}

// Function to select curricula based on scores
int select_curricula(char selected_paths[MAX_CURRICULA][1024], int max_selections) {
    float scores[MAX_CURRICULA];
    
    // Calculate scores for all curricula
    for (int i = 0; i < num_available_curricula; i++) {
        scores[i] = calculate_selection_score(i);
    }
    
    // Simple selection: select top N curricula
    int num_selected = 0;
    for (int i = 0; i < num_available_curricula && num_selected < max_selections; i++) {
        // Select curricula with positive scores or randomly with some probability
        if (scores[i] > 0.0f || ((float)rand() / RAND_MAX) < 0.3f) {
            strcpy(selected_paths[num_selected], available_curricula[i].path);
            num_selected++;
        }
    }
    
    // Always select at least one curriculum
    if (num_selected == 0) {
        strcpy(selected_paths[0], available_curricula[0].path);
        num_selected = 1;
    }
    
    return num_selected;
}

// Function to create curriculum bank file
void create_curriculum_bank(const char* bank_file_path, char selected_paths[MAX_CURRICULA][1024], int num_selected) {
    FILE* bank_file = fopen(bank_file_path, "w");
    if (!bank_file) {
        perror("Error creating curriculum bank file");
        return;
    }
    
    for (int i = 0; i < num_selected; i++) {
        fprintf(bank_file, "%s\n", selected_paths[i]);
    }
    
    fclose(bank_file);
}

// Function to get user feedback
float get_user_feedback() {
    char feedback[10];
    printf("\n--- Chatbot response completed ---\n");
    printf("Please provide feedback on the response quality (1-10): ");
    fflush(stdout);
    
    if (fgets(feedback, sizeof(feedback), stdin)) {
        int score = atoi(feedback);
        if (score >= 1 && score <= 10) {
            return (float)(score - 5) / 5.0f; // Normalize to [-1, 1]
        }
    }
    
    printf("Invalid input, assuming neutral feedback (0)\n");
    return 0.0f;
}

// Function to update RL weights based on feedback
void update_rl_weights(float feedback) {
    // Simple gradient ascent update
    rl_weights[0] += LEARNING_RATE * feedback * current_features.word_count;
    rl_weights[1] += LEARNING_RATE * feedback * current_features.has_question;
    rl_weights[2] += LEARNING_RATE * feedback * current_features.has_exclamation;
    rl_weights[3] += LEARNING_RATE * feedback * current_features.has_emoji;
    rl_weights[4] += LEARNING_RATE * feedback * current_features.has_greeting;
    rl_weights[5] += LEARNING_RATE * feedback * current_features.has_emotion;
    rl_weights[6] += LEARNING_RATE * feedback * current_features.length;
    rl_bias += LEARNING_RATE * feedback;
    
    // Update curriculum weights (simplified)
    // In a more sophisticated implementation, we would track which curricula were selected
    // and update their weights accordingly
    for (int i = 0; i < num_available_curricula; i++) {
        available_curricula[i].weight += LEARNING_RATE * feedback * 0.1f;
    }
}

// Function to save RL weights to file
void save_rl_weights(const char* weights_file) {
    FILE* file = fopen(weights_file, "w");
    if (!file) {
        perror("Error saving RL weights");
        return;
    }
    
    fprintf(file, "rl_bias %f\n", rl_bias);
    for (int i = 0; i < MAX_FEATURES; i++) {
        fprintf(file, "rl_weight_%d %f\n", i, rl_weights[i]);
    }
    
    for (int i = 0; i < num_available_curricula; i++) {
        fprintf(file, "curriculum_%s_weight %f\n", available_curricula[i].name, available_curricula[i].weight);
        fprintf(file, "curriculum_%s_bias %f\n", available_curricula[i].name, available_curricula[i].bias);
    }
    
    fclose(file);
}

// Function to load RL weights from file
void load_rl_weights(const char* weights_file) {
    FILE* file = fopen(weights_file, "r");
    if (!file) {
        printf("No existing weights file found, using default values\n");
        return;
    }
    
    char line[1024];
    while (fgets(line, sizeof(line), file)) {
        char key[256];
        float value;
        if (sscanf(line, "%s %f", key, &value) == 2) {
            if (strcmp(key, "rl_bias") == 0) {
                rl_bias = value;
            } else if (strncmp(key, "rl_weight_", 10) == 0) {
                int index = atoi(key + 10);
                if (index >= 0 && index < MAX_FEATURES) {
                    rl_weights[index] = value;
                }
            } else if (strncmp(key, "curriculum_", 11) == 0) {
                // Parse curriculum weights and biases
                // This is a simplified implementation
            }
        }
    }
    
    fclose(file);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s \"<prompt>\" [length] [temperature]\n", argv[0]);
        return 1;
    }
    
    srand(time(NULL));
    
    char *prompt = argv[1];
    int desired_length = 20;  // Default
    float temperature = 1.0f; // Default
    
    if (argc >= 3) {
        desired_length = atoi(argv[2]);
    }
    if (argc >= 4) {
        temperature = atof(argv[3]);
    }
    
    // Initialize
    initialize_curricula();
    load_rl_weights("meta_rl_weights.txt");
    extract_prompt_features(prompt);
    
    // Select curricula
    char selected_paths[MAX_CURRICULA][1024];
    int num_selected = select_curricula(selected_paths, 3); // Select up to 3 curricula
    
    printf("Selected %d curricula:\n", num_selected);
    for (int i = 0; i < num_selected; i++) {
        printf("  %d. %s\n", i+1, selected_paths[i]);
    }
    
    // Create curriculum bank
    create_curriculum_bank("meta_curriculum_bank.txt", selected_paths, num_selected);
    
    // Run chatbot with selected curricula
    char command[2048];
    snprintf(command, sizeof(command), 
             "./+x/chatbot_moe_v1.+x meta_curriculum_bank.txt \"%s\" %d %f", 
             prompt, desired_length, temperature);
    
    printf("\nRunning chatbot with selected curricula...\n");
    int result = system(command);
    
    if (result != 0) {
        fprintf(stderr, "Error running chatbot\n");
        return 1;
    }
    
    // Get user feedback
    float feedback = get_user_feedback();
    
    // Update RL weights
    update_rl_weights(feedback);
    
    // Save updated weights
    save_rl_weights("meta_rl_weights.txt");
    
    printf("RL weights updated based on feedback: %f\n", feedback);
    
    return 0;
}