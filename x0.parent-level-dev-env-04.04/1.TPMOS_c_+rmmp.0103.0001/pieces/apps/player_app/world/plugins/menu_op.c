#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

// menu_op.c - Engine Operation (v1.0)
// Responsibility: Render a menu from a PDL file and return selected index.

#define MAX_OPTIONS 16
#define MAX_LINE 1024

char* trim_str(char *str) {
    char *end;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: menu_op <pdl_file>\n");
        return 1;
    }

    FILE *f = fopen(argv[1], "r");
    if (!f) {
        fprintf(stderr, "Error: Could not open menu PDL %s\n", argv[1]);
        return 1;
    }

    char title[256] = "Menu";
    char labels[MAX_OPTIONS][256];
    int count = 0;

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f) && count < MAX_OPTIONS) {
        if (strncmp(line, "META | title", 12) == 0) {
            char *v = strrchr(line, '|');
            if (v) strcpy(title, trim_str(v + 1));
        } else if (strncmp(line, "METHOD", 6) == 0) {
            char *v = strchr(line, '|');
            if (v) {
                v++;
                char *end = strchr(v, '|');
                if (end) *end = '\0';
                strcpy(labels[count++], trim_str(v));
            }
        }
    }
    fclose(f);

    // Render Menu
    fprintf(stderr, "\n+================ %s ================+\n", title);
    for (int i = 0; i < count; i++) {
        fprintf(stderr, "|  %d. %-45s  |\n", i + 1, labels[i]);
    }
    fprintf(stderr, "+======================================+\n");
    fprintf(stderr, "Select [1-%d]: ", count);
    fflush(stderr);

    // Get input
    int sel = 0;
    char buf[16];
    if (fgets(buf, sizeof(buf), stdin)) {
        sel = atoi(buf);
    }

    // Return to Prisc VM
    printf("%d\n", sel);
    return 0;
}
