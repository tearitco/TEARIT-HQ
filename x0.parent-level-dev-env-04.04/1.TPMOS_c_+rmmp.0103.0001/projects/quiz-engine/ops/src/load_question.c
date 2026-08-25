#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
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
    srand(time(NULL));

    char question_id[64];
    if (argc > 1) {
        strncpy(question_id, argv[1], sizeof(question_id)-1);
    } else {
        // Select random question from 1-36
        int q_num = (rand() % 36) + 1;
        snprintf(question_id, sizeof(question_id), "q_%03d", q_num);
    }

    char *state_path = NULL;
    if (asprintf(&state_path, "%s/projects/%s/pieces/question_bank/%s/state.txt", 
                 project_root, current_project, question_id) == -1) return 1;

    FILE *sf = fopen(state_path, "r");
    if (!sf) {
        fprintf(stderr, "Error: Question state not found at %s\n", state_path);
        free(state_path);
        return 1;
    }

    char name[MAX_LINE] = "", formula[MAX_LINE] = "", type[MAX_LINE] = "", groups[MAX_LINE] = "", relevance[MAX_LINE] = "";
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), sf)) {
        char *eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            char *k = trim_str(line);
            char *v = trim_str(eq + 1);
            if (strcmp(k, "name") == 0) strcpy(name, v);
            else if (strcmp(k, "formula") == 0) strcpy(formula, v);
            else if (strcmp(k, "type") == 0) strcpy(type, v);
            else if (strcmp(k, "groups") == 0) strcpy(groups, v);
            else if (strcmp(k, "relevance") == 0) strcpy(relevance, v);
        }
    }
    fclose(sf);
    free(state_path);

    // Write to manager state for UI substitution
    char *mgr_state_path = NULL;
    if (asprintf(&mgr_state_path, "%s/pieces/apps/player_app/manager/state.txt", project_root) != -1) {
        // Read existing state to preserve other project variables
        FILE *rf = fopen(mgr_state_path, "r");
        char lines[200][MAX_LINE];
        int lc = 0;
        char *keys_to_update[] = {"q_id", "q_name", "q_formula", "q_type", "q_groups", "q_relevance", NULL};
        int updated[6] = {0};

        if (rf) {
            while (fgets(lines[lc], MAX_LINE, rf) && lc < 190) {
                char temp[MAX_LINE];
                strcpy(temp, lines[lc]);
                char *tk = strtok(temp, "=");
                if (tk) {
                    char *trimmed_tk = trim_str(tk);
                    for (int j = 0; keys_to_update[j]; j++) {
                        if (strcmp(trimmed_tk, keys_to_update[j]) == 0) {
                            if (strcmp(trimmed_tk, "q_id") == 0) snprintf(lines[lc], MAX_LINE, "q_id=%s\n", question_id);
                            else if (strcmp(trimmed_tk, "q_name") == 0) snprintf(lines[lc], MAX_LINE, "q_name=%s\n", name);
                            else if (strcmp(trimmed_tk, "q_formula") == 0) snprintf(lines[lc], MAX_LINE, "q_formula=%s\n", formula);
                            else if (strcmp(trimmed_tk, "q_type") == 0) snprintf(lines[lc], MAX_LINE, "q_type=%s\n", type);
                            else if (strcmp(trimmed_tk, "q_groups") == 0) snprintf(lines[lc], MAX_LINE, "q_groups=%s\n", groups);
                            else if (strcmp(trimmed_tk, "q_relevance") == 0) snprintf(lines[lc], MAX_LINE, "q_relevance=%s\n", relevance);
                            updated[j] = 1;
                        }
                    }
                }
                lc++;
            }
            fclose(rf);
        }

        // Add missing keys
        if (lc < 190) {
            if (!updated[0]) snprintf(lines[lc++], MAX_LINE, "q_id=%s\n", question_id);
            if (!updated[1]) snprintf(lines[lc++], MAX_LINE, "q_name=%s\n", name);
            if (!updated[2]) snprintf(lines[lc++], MAX_LINE, "q_formula=%s\n", formula);
            if (!updated[3]) snprintf(lines[lc++], MAX_LINE, "q_type=%s\n", type);
            if (!updated[4]) snprintf(lines[lc++], MAX_LINE, "q_groups=%s\n", groups);
            if (!updated[5]) snprintf(lines[lc++], MAX_LINE, "q_relevance=%s\n", relevance);
        }

        FILE *wf = fopen(mgr_state_path, "w");
        if (wf) {
            for (int i = 0; i < lc; i++) fputs(lines[i], wf);
            fclose(wf);
        }
        free(mgr_state_path);
    }

    printf("Loaded question: %s (%s)\n", name, question_id);
    return 0;
}
