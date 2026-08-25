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

    if (argc < 2) {
        fprintf(stderr, "Usage: bot_load <bot_id>\n");
        return 1;
    }

    char *bot_id = argv[1];
    char *bot_path = NULL;
    asprintf(&bot_path, "%s/pieces/ai_bots/%s/state.txt", project_root, bot_id);

    struct stat st;
    if (stat(bot_path, &st) != 0) {
        fprintf(stderr, "Error: Bot %s not found in repository\n", bot_id);
        free(bot_path);
        return 1;
    }

    // Write to shared state to notify parser and manager
    char *shared_state_path = NULL;
    asprintf(&shared_state_path, "%s/pieces/apps/player_app/manager/state.txt", project_root);
    
    FILE *rf = fopen(shared_state_path, "r");
    char lines[200][MAX_LINE];
    int lc = 0, updated = 0;
    if (rf) {
        while (fgets(lines[lc], MAX_LINE, rf) && lc < 190) {
            if (strncmp(lines[lc], "active_bot=", 11) == 0) {
                snprintf(lines[lc], MAX_LINE, "active_bot=%s\n", bot_id);
                updated = 1;
            }
            lc++;
        }
        fclose(rf);
    }
    if (!updated && lc < 200) snprintf(lines[lc++], MAX_LINE, "active_bot=%s\n", bot_id);

    FILE *wf = fopen(shared_state_path, "w");
    if (wf) {
        for (int i = 0; i < lc; i++) fputs(lines[i], wf);
        fclose(wf);
    }

    printf("Bot %s loaded into workspace\n", bot_id);

    free(bot_path);
    free(shared_state_path);
    return 0;
}
