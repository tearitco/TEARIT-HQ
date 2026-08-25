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

    if (argc < 3) {
        fprintf(stderr, "Usage: fs_write <path> <mode: W or A> <content>\n");
        return 1;
    }

    char *target = argv[1];
    char mode_char = toupper(argv[2][0]);
    char *mode = (mode_char == 'W') ? "w" : "a";
    char *content = argv[3];

    FILE *f = fopen(target, mode);
    if (f) {
        fprintf(f, "%s\n", content);
        fclose(f);
        printf("Content written to %s (mode: %s)\n", target, mode);
    } else {
        fprintf(stderr, "Error: Could not open file %s for mode %s\n", target, mode);
        return 1;
    }

    return 0;
}
