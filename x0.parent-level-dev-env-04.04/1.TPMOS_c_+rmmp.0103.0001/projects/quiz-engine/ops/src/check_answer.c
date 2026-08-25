#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>

#define MAX_PATH 4096
#define MAX_LINE 1024

char project_root[MAX_PATH] = ".";
char current_project[MAX_LINE] = "quiz-engine";

char* trim_str(char *str) {
    char *end;
    if (!str) return str;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
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

int main(int argc, char *argv[]) {
    resolve_paths();

    if (argc < 4) {
        fprintf(stderr, "Usage: check_answer <question_id> <quiz_type> <user_answer>\n");
        return 1;
    }

    char *q_id = argv[1];
    int quiz_type = atoi(argv[2]); // 1=Type, 2=Groups
    char *user_answer = argv[3];

    char *state_path = NULL;
    if (asprintf(&state_path, "%s/projects/%s/pieces/question_bank/%s/state.txt", 
                 project_root, current_project, q_id) == -1) return 1;

    FILE *sf = fopen(state_path, "r");
    if (!sf) {
        fprintf(stderr, "Error: Question state not found\n");
        free(state_path);
        return 1;
    }

    char correct_val[MAX_LINE] = "";
    char line[MAX_LINE];
    char *target_key = (quiz_type == 1) ? "type" : "groups";

    while (fgets(line, sizeof(line), sf)) {
        char *eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(trim_str(line), target_key) == 0) {
                strcpy(correct_val, trim_str(eq + 1));
                break;
            }
        }
    }
    fclose(sf);
    free(state_path);

    int is_correct = (strcmp(user_answer, correct_val) == 0);
    int reward = is_correct ? 10 : -5;

    // Log to rl_training_data.txt
    char *log_path = NULL;
    if (asprintf(&log_path, "%s/projects/%s/pieces/session_logs/rl_training_data.txt", project_root, current_project) != -1) {
        FILE *lf = fopen(log_path, "a");
        if (lf) {
            fprintf(lf, "%ld | %s | %d | 0 | %d\n", (long)time(NULL), q_id, is_correct, reward);
            fclose(lf);
        }
        free(log_path);
    }

    // Update manager state
    char *mgr_state_path = NULL;
    if (asprintf(&mgr_state_path, "%s/pieces/apps/player_app/manager/state.txt", project_root) != -1) {
        FILE *rf = fopen(mgr_state_path, "r");
        char lines[200][MAX_LINE];
        int lc = 0;
        char *keys_to_update[] = {"last_result", "correct_answer", NULL};
        int updated[2] = {0};

        if (rf) {
            while (fgets(lines[lc], MAX_LINE, rf) && lc < 190) {
                char temp[MAX_LINE];
                strcpy(temp, lines[lc]);
                char *tk = strtok(temp, "=");
                if (tk) {
                    char *trimmed_tk = trim_str(tk);
                    for (int j = 0; keys_to_update[j]; j++) {
                        if (strcmp(trimmed_tk, keys_to_update[j]) == 0) {
                            if (strcmp(trimmed_tk, "last_result") == 0) snprintf(lines[lc], MAX_LINE, "last_result=%s\n", is_correct ? "Correct" : "Incorrect");
                            else if (strcmp(trimmed_tk, "correct_answer") == 0) snprintf(lines[lc], MAX_LINE, "correct_answer=%s\n", correct_val);
                            updated[j] = 1;
                        }
                    }
                }
                lc++;
            }
            fclose(rf);
        }

        if (lc < 190) {
            if (!updated[0]) snprintf(lines[lc++], MAX_LINE, "last_result=%s\n", is_correct ? "Correct" : "Incorrect");
            if (!updated[1]) snprintf(lines[lc++], MAX_LINE, "correct_answer=%s\n", correct_val);
        }

        FILE *wf = fopen(mgr_state_path, "w");
        if (wf) {
            for (int i = 0; i < lc; i++) fputs(lines[i], wf);
            fclose(wf);
        }
        free(mgr_state_path);
    }

    printf("%s\n", is_correct ? "CORRECT" : "INCORRECT");
    return 0;
}
