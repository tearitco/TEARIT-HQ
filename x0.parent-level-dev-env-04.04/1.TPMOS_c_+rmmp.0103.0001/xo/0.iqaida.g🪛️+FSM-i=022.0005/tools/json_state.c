// tools/json_state.c - Robust JSON state manager with escaping
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* json_escape(const char* s) {
    if (!s) return strdup("");
    size_t len = strlen(s);
    char* out = malloc(len * 6 + 1); // Worst case: every char is escaped
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
    if (argc < 3) { fprintf(stderr, "Usage: json_state [read|append] <file> [role] [content]\n"); return 1; }
    
    const char* action = argv[1];
    const char* file = argv[2];
    
    if (strcmp(action, "read") == 0) {
        FILE* f = fopen(file, "r");
        if (!f) { printf("[]"); return 0; }
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        char* buf = malloc(size + 1);
        if (buf) {
            size_t n = fread(buf, 1, size, f);
            buf[n] = '\0';
            printf("%s", buf);
            free(buf);
        }
        fclose(f);
        return 0;
    }
    
    if (strcmp(action, "append") == 0 && argc >= 5) {
        const char* role = argv[3];
        const char* content = argv[4];
        char* escaped_content = json_escape(content);
        
        FILE* f = fopen(file, "r");
        char* existing = NULL;
        long size = 0;
        if (f) {
            fseek(f, 0, SEEK_END);
            size = ftell(f);
            fseek(f, 0, SEEK_SET);
            if (size > 0) {
                existing = malloc(size + 1);
                if (existing) {
                    size_t n = fread(existing, 1, size, f);
                    existing[n] = '\0';
                }
            }
            fclose(f);
        }
        
        f = fopen(file, "w");
        if (!f) { perror("fopen"); free(escaped_content); free(existing); return 1; }
        
        if (existing && size > 0) {
            // Find the last ']'
            char* last_bracket = NULL;
            for (long i = size - 1; i >= 0; i--) {
                if (existing[i] == ']') {
                    last_bracket = &existing[i];
                    break;
                }
            }
            
            if (last_bracket) {
                *last_bracket = '\0';
                // Check if there are other elements
                char* first_bracket = strchr(existing, '[');
                int has_elements = 0;
                if (first_bracket) {
                    for (char* p = first_bracket + 1; p < last_bracket; p++) {
                        if (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
                            has_elements = 1;
                            break;
                        }
                    }
                }
                
                if (has_elements) {
                    fprintf(f, "%s,{\"role\":\"%s\",\"content\":\"%s\"}]", existing, role, escaped_content);
                } else {
                    fprintf(f, "[{\"role\":\"%s\",\"content\":\"%s\"}]", role, escaped_content);
                }
            } else {
                // Not a valid JSON array, start new one
                fprintf(f, "[{\"role\":\"%s\",\"content\":\"%s\"}]", role, escaped_content);
            }
        } else {
            fprintf(f, "[{\"role\":\"%s\",\"content\":\"%s\"}]", role, escaped_content);
        }
        
        fclose(f);
        free(escaped_content);
        free(existing);
    }
    return 0;
}
