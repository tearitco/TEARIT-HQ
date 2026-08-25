#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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
        fprintf(stderr, "Usage: fs_mk <path> [type: F or D]\n");
        return 1;
    }

    char *target = argv[1];
    char type = (argc > 2) ? toupper(argv[2][0]) : 'F';

    if (type == 'D') {
        char cmd[MAX_PATH + 10];
        snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", target);
        system(cmd);
        printf("Directory created: %s\n", target);
    } else {
        FILE *f = fopen(target, "a");
        if (f) {
            fclose(f);
            printf("File initialized: %s\n", target);
        } else {
            fprintf(stderr, "Error: Could not create file %s\n", target);
            return 1;
        }
    }

    return 0;
}
