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
        fprintf(stderr, "Usage: bot_tune <bot_id> <key> <value>\n");
        return 1;
    }

    char *bot_id = argv[1];
    char *key = argv[2];
    char *value = argv[3];

    char *state_path = NULL;
    asprintf(&state_path, "%s/pieces/ai_bots/%s/state.txt", project_root, bot_id);

    struct stat st;
    if (stat(state_path, &st) != 0) {
        fprintf(stderr, "Error: Bot %s not found\n", bot_id);
        free(state_path);
        return 1;
    }

    // Read and update state.txt
    FILE *rf = fopen(state_path, "r");
    char lines[100][MAX_LINE];
    int lc = 0, found = 0;
    if (rf) {
        while (fgets(lines[lc], MAX_LINE, rf) && lc < 99) {
            char *eq = strchr(lines[lc], '=');
            if (eq) {
                char temp[MAX_LINE];
                strcpy(temp, lines[lc]);
                char *tk = strtok(temp, "=");
                if (strcmp(trim_str(tk), key) == 0) {
                    snprintf(lines[lc], MAX_LINE, "%s=%s\n", key, value);
                    found = 1;
                }
            }
            lc++;
        }
        fclose(rf);
    }

    if (!found && lc < 100) {
        snprintf(lines[lc++], MAX_LINE, "%s=%s\n", key, value);
    }

    FILE *wf = fopen(state_path, "w");
    if (wf) {
        for (int i = 0; i < lc; i++) fputs(lines[i], wf);
        fclose(wf);
    }

    printf("Bot %s updated: %s=%s\n", bot_id, key, value);
    free(state_path);

    return 0;
}
