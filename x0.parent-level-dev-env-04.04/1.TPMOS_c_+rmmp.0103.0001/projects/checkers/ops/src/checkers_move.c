#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>
#include <math.h>

#define MAX_PATH 4096
#define MAX_LINE 1024
#define BOARD_SIZE 32
#define WEIGHT_COUNT (BOARD_SIZE + 1)

char project_root[MAX_PATH] = ".";
int board[BOARD_SIZE]; // 0=empty, 1=red, -1=black, 2=red king, -2=black king
int game_mode = 0; // Default to Human vs AI

float weights_red[WEIGHT_COUNT];
float weights_black[WEIGHT_COUNT];

char* trim_str(char *str) {
    char *end;
    if (!str) return NULL;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

void resolve_paths() {
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *k = trim_str(line);
                char *v = trim_str(eq + 1);
                if (strcmp(k, "project_root") == 0) snprintf(project_root, sizeof(project_root), "%s", v);
            }
        }
        fclose(kvp);
    }
}

int has_captures(int player) {
    return 0; // Placeholder
}

int is_valid_move(int from, int to, int player) {
    // Placeholder implementation
    if (from < 0 || from >= BOARD_SIZE || to < 0 || to >= BOARD_SIZE) return 0;
    if (board[from] == 0) return 0;
    if (board[to] != 0) return 0;
    return 1;
}

void make_move(int from, int to, int player) {
    board[to] = board[from];
    board[from] = 0;
    // Basic king promotion
    if (player == 1 && to < 4) board[to] = 2;
    if (player == -1 && to > 27) board[to] = -2;
}

void init_game() {
    for (int i = 0; i < BOARD_SIZE; i++) {
        if (i < 12) board[i] = -1;        // Black pieces
        else if (i < 20) board[i] = 0;     // Empty middle
        else board[i] = 1;                 // Red pieces
    }
}

void print_board() {
    for (int i = 0; i < BOARD_SIZE; i++) {
        if (board[i] == 1) printf("r ");
        else if (board[i] == -1) printf("b ");
        else if (board[i] == 2) printf("R ");
        else if (board[i] == -2) printf("B ");
        else printf(". ");
        if ((i + 1) % 4 == 0) printf("%2d\n", i);
    }
    printf("\n");
}

void load_weights(int player, float *weights) {
    char filename[64];
    snprintf(filename, sizeof(filename), "%s/projects/checkers/weights_%s.dat", project_root, player == 1 ? "red" : "black");
    FILE *fp = fopen(filename, "rb");
    if (fp) {
        fread(weights, sizeof(float), WEIGHT_COUNT, fp);
        fclose(fp);
    } else {
        for (int i = 0; i < WEIGHT_COUNT; i++) {
            weights[i] = (float)(rand() % 100) / 100.0;
        }
    }
}

void save_weights(int player) {
    char filename[64];
    snprintf(filename, sizeof(filename), "%s/projects/checkers/weights_%s.dat", project_root, player == 1 ? "red" : "black");
    FILE *fp = fopen(filename, "wb");
    if (fp) {
        float *weights = (player == 1) ? weights_red : weights_black;
        fwrite(weights, sizeof(float), WEIGHT_COUNT, fp);
        fclose(fp);
    }
}

void update_weights(int winner, float *weights) {
    float learning_rate = 0.1;
    for (int i = 0; i < BOARD_SIZE; i++) {
        if (board[i] == winner || board[i] == winner * 2) {
            weights[i] += learning_rate;
        }
    }
    weights[WEIGHT_COUNT-1] += learning_rate * (winner > 0 ? 1 : -1);
}

void print_weights(float *weights, const char *player) {
    printf("%s weights: ", player);
    for (int i = 0; i < 5; i++) {
        printf("%.2f ", weights[i]);
    }
    printf("... bias: %.2f\n", weights[WEIGHT_COUNT-1]);
}

int get_human_move(int player, int *from, int *to) {
    printf("Player %s - Enter piece to move (0-31): ", player == 1 ? "Red" : "Black");
    if (scanf("%d", from) != 1) return 0;
    printf("Enter destination (0-31): ");
    if (scanf("%d", to) != 1) return 0;
    return 1;
}

int get_ai_move(int player, int *from, int *to) {
    float best_score = -1000.0;
    int best_from = -1, best_to = -1;
    float *weights = (player == 1) ? weights_red : weights_black;

    for (int i = 0; i < BOARD_SIZE; i++) {
        if (board[i] != player && board[i] != player * 2) continue;
        int moves[4] = {i-4, i-3, i+3, i+4};
        for (int j = 0; j < 4; j++) {
            if (moves[j] >= 0 && moves[j] < BOARD_SIZE) {
                if (is_valid_move(i, moves[j], player)) {
                    float score = weights[moves[j]] + weights[WEIGHT_COUNT-1];
                    if (score > best_score) {
                        best_score = score;
                        best_from = i;
                        best_to = moves[j];
                    }
                }
            }
        }
    }
    if (best_from == -1) return 0;
    *from = best_from;
    *to = best_to;
    return 1;
}

void update_game_state(int current_player) {
    int red_pieces = 0, black_pieces = 0;
    for (int i = 0; i < BOARD_SIZE; i++) {
        if (board[i] == 1 || board[i] == 2) red_pieces++;
        if (board[i] == -1 || board[i] == -2) black_pieces++;
    }

    if (red_pieces == 0) {
        printf("Game Over! Black wins!\n");
        exit(0);
    }
    if (black_pieces == 0) {
        printf("Game Over! Red wins!\n");
        exit(0);
    }
}

int main(int argc, char *argv[]) {
    srand(time(NULL));
    resolve_paths();
    
    if (argc > 1) {
        game_mode = atoi(argv[1]);
    } else {
        printf("Select mode: 0 (Human vs AI) or 1 (AI vs AI): ");
        if (scanf("%d", &game_mode) != 1) game_mode = 0;
    }
    
    load_weights(1, weights_red);
    load_weights(-1, weights_black);
    
    printf("Initial weights:\n");
    print_weights(weights_red, "Red");
    print_weights(weights_black, "Black");
    
    init_game();
    
    int current_player = 1;
    while (1) {
        print_board();
        int from, to, move_made = 0;
        
        if (game_mode == 0 && current_player == 1) {
            move_made = get_human_move(current_player, &from, &to);
        } else {
            move_made = get_ai_move(current_player, &from, &to);
            if (move_made) printf("AI (%s) moves %d to %d\n", current_player == 1 ? "Red" : "Black", from, to);
        }
        
        if (move_made && is_valid_move(from, to, current_player)) {
            make_move(from, to, current_player);
            update_game_state(current_player);
            current_player = -current_player;
        } else if (!move_made) {
            printf("No valid moves found! %s wins!\n", current_player == 1 ? "Black" : "Red");
            update_weights(current_player == 1 ? -1 : 1, current_player == 1 ? weights_black : weights_red);
            save_weights(current_player);
            break;
        } else {
            printf("Invalid move attempted: %d to %d\n", from, to);
            if (game_mode == 0 && current_player == 1) {
                printf("Try again.\n");
            } else {
                current_player = -current_player;
            }
        }
    }
    return 0;
}
