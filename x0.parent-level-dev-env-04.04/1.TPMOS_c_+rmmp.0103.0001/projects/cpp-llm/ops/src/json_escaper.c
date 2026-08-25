// tools/json_escaper.c - Escapes raw text for JSON injection
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* json_escape(const char* s) {
    if (!s) return strdup("");
    size_t len = strlen(s);
    char* out = malloc(len * 6 + 1);
    if (!out) return NULL;
    char* p = out;
    while (*s) {
        if (*s == '"') { *p++ = '\\'; *p++ = '"'; }
        else if (*s == '\\') { *p++ = '\\'; *p++ = '\\'; }
        else if (*s == '\n') { *p++ = '\\'; *p++ = 'n'; }
        else if (*s == '\r') { *p++ = '\\'; *p++ = 'r'; }
        else if (*s == '\t') { *p++ = '\\'; *p++ = 't'; }
        else if ((unsigned char)*s < 32) {
            sprintf(p, "\\u%04x", *s);
            p += 6;
        } else {
            *p++ = *s;
        }
        s++;
    }
    *p = '\0';
    return out;
}

int main(int argc, char* argv[]) {
    if (argc < 2) return 0;
    
    // Read from file or string
    char* input = NULL;
    if (argc == 2) {
        input = argv[1];
    } else {
        // Read from file if 2nd arg is -f
        if (strcmp(argv[1], "-f") == 0) {
            FILE* f = fopen(argv[2], "r");
            if (!f) return 1;
            fseek(f, 0, SEEK_END); long size = ftell(f); fseek(f, 0, SEEK_SET);
            input = malloc(size + 1);
            fread(input, 1, size, f); input[size] = '\0';
            fclose(f);
        }
    }

    if (input) {
        char* escaped = json_escape(input);
        if (escaped) {
            printf("%s", escaped);
            free(escaped);
        }
        if (argc == 3) free(input);
    }
    return 0;
}
