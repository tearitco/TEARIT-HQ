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

char project_root[MAX_PATH] = ".";
int board[BOARD_SIZE]; // 0=empty, 1=red, -1=black, 2=red king, -2=black king

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
    // Placeholder implementation
    return 0; 
}

int is_valid_move(int from, int to, int player) {
    resolve_paths();
    
    char state_path[MAX_PATH];
    snprintf(state_path, sizeof(state_path), "%s/projects/checkers/manager/state.txt", project_root);
    
    FILE *sf = fopen(state_path, "r");
    if (!sf) return 0;
    char line[MAX_LINE];
    int current_player_id = 0;
    while (fgets(line, sizeof(line), sf)) {
        if (strncmp(line, "player_turn=", 11) == 0) {
            current_player_id = atoi(line + 11);
            break;
        }
    }
    fclose(sf);
    
    if (player != current_player_id) return 0; // Not the current player's turn

    if (from < 0 || from >= BOARD_SIZE || to < 0 || to >= BOARD_SIZE) return 0;
    if (board[from] != player && board[from] != player * 2) return 0; // Not player's piece
    if (board[to] != 0) return 0; // Destination occupied

    int is_king = abs(board[from]) == 2;
    int diff = to - from;
    int row_diff = (to / 4) - (from / 4);

    if (has_captures(player) && abs(diff) <= 4) return 0; // Simple move when capture is available

    if (abs(diff) == 3 || abs(diff) == 4) { // Simple move
        if (player == 1 && !is_king && row_diff > 0) return 0; // Red moves down (increasing row index)
        if (player == -1 && !is_king && row_diff < 0) return 0; // Black moves up (decreasing row index)
        return 1;
    }

    if (abs(diff) == 7 || abs(diff) == 9) { // Capture move
        int mid = from + diff / 2;
        if (board[mid] == -player || board[mid] == -player * 2) { // Opponent piece to capture
            if (player == 1 && !is_king && row_diff > 0) return 0;
            if (player == -1 && !is_king && row_diff < 0) return 0;
            return 1;
        }
    }
    return 0;
}

int main(int argc, char *argv[]) {
    resolve_paths();
    
    if (argc < 4) {
        fprintf(stderr, "Usage: checkers_validate <from> <to> <player>\n");
        return 1;
    }

    int from = atoi(argv[1]);
    int to = atoi(argv[2]);
    int player = atoi(argv[3]);

    // Load board state (simplistic: assumes board is managed externally or loaded here)
    for (int i = 0; i < BOARD_SIZE; i++) {
        if (i < 12) board[i] = -1;        // Black pieces
        else if (i < 20) board[i] = 0;     // Empty middle
        else board[i] = 1;                 // Red pieces
    }

    int valid = is_valid_move(from, to, player);
    printf("%d\n", valid); // Output 1 for valid, 0 for invalid

    return 0;
}
