// tools/json_parser.c - Extracts key values, strips markdown, unescapes strings
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* skip_ws(const char* s) { while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++; return s; }

static int extract_value(const char* json, const char* key, char* out, size_t out_size) {
    const char* p = json; char k_buf[256] = {0};
    while ((p = strchr(p, '"')) != NULL) {
        p++; size_t i = 0;
        while (*p && *p != '"' && i < sizeof(k_buf)-1) { 
            if (*p == '\\' && *(p+1)) { p++; k_buf[i++] = *p++; }
            else k_buf[i++] = *p++; 
        }
        k_buf[i] = '\0'; 
        if (*p == '"') p = skip_ws(p+1);
        
        if (*p == ':' && strcmp(k_buf, key) == 0) {
            p = skip_ws(p+1);
            if (*p == '"') {
                p++; size_t j = 0;
                while (*p && j < out_size-1) {
                    if (*p == '\\' && *(p+1)) {
                        p++;
                        if (*p == 'n') out[j++] = '\n';
                        else if (*p == 't') out[j++] = '\t';
                        else if (*p == 'r') out[j++] = '\r';
                        else if (*p == '"') out[j++] = '"';
                        else if (*p == '\\') out[j++] = '\\';
                        else out[j++] = *p;
                        p++;
                    } else if (*p == '"') {
                        break; // Unescaped quote
                    } else {
                        out[j++] = *p++;
                    }
                }
                out[j] = '\0'; return 1;
            } else if (*p == '{') {
                // Extract object
                int depth = 0;
                size_t j = 0;
                while (*p && j < out_size - 1) {
                    if (*p == '{') depth++;
                    else if (*p == '}') depth--;
                    out[j++] = *p++;
                    if (depth == 0) break;
                }
                out[j] = '\0'; return 1;
            } else if (*p == '[') {
                // Extract array
                int depth = 0;
                size_t j = 0;
                while (*p && j < out_size - 1) {
                    if (*p == '[') depth++;
                    else if (*p == ']') depth--;
                    out[j++] = *p++;
                    if (depth == 0) break;
                }
                out[j] = '\0'; return 1;
            }
            size_t j = 0; while (*p && *p != ',' && *p != '}' && *p != ']' && j < out_size-1) out[j++] = *p++;
            out[j] = '\0'; return 1;
        }
    }
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc != 3) { fprintf(stderr, "Usage: json_parser <file> <key>\n"); return 1; }
    FILE* f = fopen(argv[1], "r"); if (!f) { perror("fopen"); return 1; }
    fseek(f, 0, SEEK_END); long size = ftell(f); fseek(f, 0, SEEK_SET);
    char* raw = calloc(size + 1, 1);
    if (!raw) { fclose(f); return 1; }
    size_t n = fread(raw, 1, size, f); fclose(f); raw[n] = '\0';

    // Strip markdown fences if present
    char* start = strstr(raw, "```json"); char* json_start = start ? start + 7 : raw;
    char* end = strstr(json_start, "```"); if (end) *end = '\0';

    char* val = calloc(size + 1, 1);
    if (extract_value(json_start, argv[2], val, size)) {
        printf("%s", val);
        free(raw); free(val); return 0;
    } else {
        // Completely silent on stdout and stderr for "key not found"
        free(raw); free(val); return 1;
    }
}