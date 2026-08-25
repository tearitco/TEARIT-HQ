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
        fprintf(stderr, "Usage: bot_create <bot_id> [template_id]\n");
        return 1;
    }

    char *bot_id = argv[1];
    char *template_id = (argc > 2) ? argv[2] : "base_bot";

    char *template_path = NULL;
    asprintf(&template_path, "%s/pieces/ai_bots/templates/%s", project_root, template_id);
    
    char *target_path = NULL;
    asprintf(&target_path, "%s/pieces/ai_bots/%s", project_root, bot_id);

    struct stat st;
    if (stat(template_path, &st) != 0) {
        fprintf(stderr, "Error: Template %s not found\n", template_id);
        return 1;
    }

    if (stat(target_path, &st) == 0) {
        fprintf(stderr, "Error: Bot %s already exists\n", bot_id);
        return 1;
    }

    char cmd[MAX_PATH * 2];
    snprintf(cmd, sizeof(cmd), "cp -r '%s' '%s'", template_path, target_path);
    system(cmd);

    // Update piece.pdl with new ID
    char *pdl_path = NULL;
    asprintf(&pdl_path, "%s/piece.pdl", target_path);
    
    FILE *pf = fopen(pdl_path, "w");
    if (pf) {
        fprintf(pf, "SECTION      | KEY                | VALUE\n");
        fprintf(pf, "----------------------------------------\n");
        fprintf(pf, "META         | piece_id           | %s\n", bot_id);
        fprintf(pf, "META         | type               | bot\n");
        fclose(pf);
    }

    printf("Bot %s created from template %s\n", bot_id, template_id);

    free(template_path);
    free(target_path);
    free(pdl_path);

    return 0;
}
