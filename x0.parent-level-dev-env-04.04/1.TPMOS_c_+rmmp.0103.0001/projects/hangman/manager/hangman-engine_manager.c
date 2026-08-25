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

#define MODULE_NAME "hangman-engine"
#define MAX_PATH 4096
#define MAX_LINE 1024
#define MAX_WORD_LEN 100

char project_root[MAX_PATH] = ".";
char secret_word[MAX_WORD_LEN] = "";
char display_word[MAX_WORD_LEN] = "";
int remaining_lives = 6;
int player1_type = 0; // 0: human, 1: ai
int player2_type = 0; // 0: human, 1: ai

static volatile sig_atomic_t g_shutdown = 0;

static void handle_sigint(int sig) {
    (void)sig;
    g_shutdown = 1;
}

static int run_command(const char* cmd) {
    pid_t pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execl("/bin/sh", "/bin/sh", "-c", cmd, (char *)NULL);
        _exit(127);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
    return -1;
}

static char* trim_str(char *str) {
    char *end;
    if (!str) return str;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

static void resolve_paths() {
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

static void load_word() {
    char word_list_path[MAX_PATH];
    snprintf(word_list_path, sizeof(word_list_path), "%s/projects/hangman/pieces/word_bank/wordlist.txt", project_root);
    FILE *wl = fopen(word_list_path, "r");
    if (!wl) {
        fprintf(stderr, "Error: Word list not found at %s
", word_list_path);
        strcpy(secret_word, "default");
        return;
    }

    int line_count = 0;
    char lines[100][MAX_LINE];
    while (fgets(lines[line_count], MAX_LINE, wl) && line_count < 99) {
        lines[line_count][strcspn(lines[line_count], "
")] = 0; // Remove newline
        line_count++;
    }
    fclose(wl);

    if (line_count == 0) {
        fprintf(stderr, "Error: Word list is empty
");
        strcpy(secret_word, "empty");
        return;
    }

    int random_index = rand() % line_count;
    strcpy(secret_word, lines[random_index]);
    printf("Loaded word: %s
", secret_word);
}

static void init_game() {
    load_word();
    for (int i = 0; i < strlen(secret_word); i++) {
        display_word[i] = '_';
    }
    display_word[strlen(secret_word)] = '\0';
    remaining_lives = 6;
}

static void update_state() {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/manager/state.txt", project_root);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "project_id=hangman
");
        fprintf(f, "current_word=%s
", secret_word);
        fprintf(f, "display_word=%s
", display_word);
        fprintf(f, "remaining_lives=%d
", remaining_lives);
        fprintf(f, "last_key=%s
", last_key_str);
        fprintf(f, "p1_type=%s
", p1_type);
        fprintf(f, "p2_type=%s
", p2_type);
        fclose(f);
    }
}

static void process_key(int key) {
    if (current_state == STATE_SETUP) {
        if (key == '1') { // Human vs AI
            strcpy(p1_type, "human");
            strcpy(p2_type, "ai");
            printf("Mode: Human vs AI
");
        } else if (key == '2') { // AI vs AI
            strcpy(p1_type, "ai");
            strcpy(p2_type, "ai");
            printf("Mode: AI vs AI
");
        } else if (key == '3') { // Human vs Human (P2P)
            strcpy(p1_type, "human");
            strcpy(p2_type, "human");
            printf("Mode: Human vs Human (P2P)
");
        }

        if (key == 10 || key == 13) { // ENTER
            init_game();
            current_state = STATE_PLAY;
        }
    } else if (current_state == STATE_PLAY) {
        if (key >= 'a' && key <= 'z') {
            char letter = (char)key;
            char *guess_ptr = strchr(secret_word, letter);
            int found = 0;
            for (int i = 0; i < strlen(secret_word); i++) {
                if (secret_word[i] == letter) {
                    display_word[i] = letter;
                    found = 1;
                }
            }

            if (found) {
                printf("Correct guess: %c
", letter);
            } else {
                remaining_lives--;
                printf("Incorrect guess: %c. Lives left: %d
", letter, remaining_lives);
            }
            
            // Check for win/loss
            if (strcmp(display_word, secret_word) == 0) {
                printf("You win! The word was: %s
", secret_word);
                current_state = STATE_GAME_OVER;
            } else if (remaining_lives <= 0) {
                printf("You lose! The word was: %s
", secret_word);
                current_state = STATE_GAME_OVER;
            }
        } else if (key == 27) { // ESC
            current_state = STATE_SETUP; // Go back to setup
        }
    } else if (current_state == STATE_GAME_OVER) {
        if (key == 10 || key == 13) { // ENTER to restart or exit
            current_state = STATE_SETUP;
        }
    }
}

int main(void) {
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);
    setpgid(0, 0);
    resolve_paths();
    
    char hist_path[MAX_PATH];
    snprintf(hist_path, sizeof(hist_path), "%s/pieces/keyboard/history.txt", project_root);
    
    long last_pos = 0;
    struct stat st;
    if (stat(hist_path, &st) == 0) last_pos = st.st_size;

    update_state(); // Initial state update

    while (!g_shutdown) {
        if (!is_active_layout()) {
            usleep(100000);
            continue;
        }

        if (stat(hist_path, &st) == 0 && st.st_size > last_pos) {
            FILE *hf = fopen(hist_path, "r");
            if (hf) {
                fseek(hf, last_pos, SEEK_SET);
                int key;
                while (fscanf(hf, "%d", &key) == 1) {
                    process_key(key);
                    update_state();
                }
                last_pos = ftell(hf);
                fclose(hf);
            }
        } else if (st.st_size < last_pos) {
            last_pos = 0; // File truncated, reset position
        }
        usleep(16667); // ~60 FPS poll rate
    }
    return 0;
}
