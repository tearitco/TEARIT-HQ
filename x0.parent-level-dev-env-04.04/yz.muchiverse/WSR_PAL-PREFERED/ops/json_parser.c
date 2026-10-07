// ops/json_parser.c - Robust dot-notation JSON parser
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_CELL_SIZE 65536

static const char* skip_ws(const char* s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

static const char* skip_value(const char* s) {
    s = skip_ws(s);
    if (*s == '"') {
        s++;
        while (*s && (*s != '"' || (*(s-1) == '\\' && *(s-2) != '\\'))) s++;
        if (*s == '"') s++;
    } else if (*s == '{') {
        int depth = 1; s++;
        while (*s && depth > 0) {
            if (*s == '{') depth++;
            else if (*s == '}') depth--;
            else if (*s == '"') {
                s++;
                while (*s && (*s != '"' || (*(s-1) == '\\' && *(s-2) != '\\'))) s++;
            }
            if (*s) s++;
        }
    } else if (*s == '[') {
        int depth = 1; s++;
        while (*s && depth > 0) {
            if (*s == '[') depth++;
            else if (*s == ']') depth--;
            else if (*s == '"') {
                s++;
                while (*s && (*s != '"' || (*(s-1) == '\\' && *(s-2) != '\\'))) s++;
            }
            if (*s) s++;
        }
    } else {
        while (*s && !isspace((unsigned char)*s) && *s != ',' && *s != '}' && *s != ']') s++;
    }
    return s;
}

static char* unescape(const char* s, size_t len) {
    char* out = malloc(len + 1);
    size_t i = 0, j = 0;
    while (i < len) {
        if (s[i] == '\\' && i + 1 < len) {
            i++;
            switch (s[i]) {
                case 'n': out[j++] = '\n'; break;
                case 'r': out[j++] = '\r'; break;
                case 't': out[j++] = '\t'; break;
                case '"': out[j++] = '"'; break;
                case '\\': out[j++] = '\\'; break;
                default: out[j++] = s[i]; break;
            }
        } else {
            out[j++] = s[i];
        }
        i++;
    }
    out[j] = '\0';
    return out;
}

/* Strip markdown code fences from a string. */
static void strip_markdown(char *s) {
    if (!s || !*s) return;
    /* Strip leading ```json or ``` */
    if (strncmp(s, "```json", 7) == 0) {
        memmove(s, s + 7, strlen(s + 7) + 1);
    } else if (strncmp(s, "```", 3) == 0) {
        memmove(s, s + 3, strlen(s + 3) + 1);
    }
    /* Strip trailing ``` */
    char *end_fence = strstr(s, "```");
    if (end_fence) *end_fence = '\0';
    /* Trim leading/trailing whitespace */
    char *start = s;
    while (*start && isspace((unsigned char)*start)) start++;
    if (start != s) memmove(s, start, strlen(start) + 1);
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len-1])) s[--len] = '\0';
}

/* Extract the raw string value (without quotes) and unescape it from a
 * quoted JSON string.  Returns malloc'd buffer. */
static char* extract_string_value(const char* val_start) {
    const char* end = val_start;
    if (*end == '"') end++;
    while (*end && (*end != '"' || (*(end-1) == '\\' && *(end-2) != '\\'))) end++;
    size_t raw_len = end - val_start;
    char* raw_copy = malloc(raw_len + 1);
    memcpy(raw_copy, val_start, raw_len);
    raw_copy[raw_len] = '\0';
    char* clean = unescape(raw_copy + 1, raw_len - 1);
    free(raw_copy);
    return clean;
}

static char* find_in_json(const char* json, const char* path) {
    if (!path || !*path) return strdup(json);
    
    char* path_copy = strdup(path);
    char* segment = path_copy;
    char* next_segment = strchr(path_copy, '.');
    if (next_segment) *next_segment++ = '\0';
    
    // Check if segment is array index
    int is_array = 0;
    int index = 0;
    char* bracket = strchr(segment, '[');
    if (bracket) {
        *bracket++ = '\0';
        index = atoi(bracket);
        is_array = 1;
    }
    
    const char* p = skip_ws(json);
    char* result = NULL;
    
    if (segment[0] != '\0') {
        if (*p != '{') goto done;
        p = skip_ws(p + 1);
        while (*p && *p != '}') {
            if (*p != '"') break;
            p++;
            const char* key_start = p;
            while (*p && *p != '"') p++;
            size_t key_len = p - key_start;
            p = skip_ws(p + 1);
            if (*p != ':') break;
            p = skip_ws(p + 1);
            
            if (key_len == strlen(segment) && strncmp(key_start, segment, key_len) == 0) {
                if (is_array) {
                    p = skip_ws(p);
                    if (*p != '[') goto done;
                    p = skip_ws(p + 1);
                    for (int i = 0; i < index; i++) {
                        p = skip_value(p);
                        p = skip_ws(p);
                        if (*p == ',') p = skip_ws(p + 1);
                    }
                }
                
                const char* val_start = p;
                p = skip_value(p);
                size_t val_len = p - val_start;
                
                /* If this is a string value and we have more path to navigate,
                 * unescape it, strip markdown, and treat as JSON for recursion. */
                if (*val_start == '"' && next_segment && *next_segment) {
                    char *clean = extract_string_value(val_start);
                    strip_markdown(clean);
                    result = find_in_json(clean, next_segment);
                    free(clean);
                } else {
                    char* sub_json = malloc(val_len + 1);
                    memcpy(sub_json, val_start, val_len);
                    sub_json[val_len] = '\0';
                    result = find_in_json(sub_json, next_segment);
                    free(sub_json);
                }
                goto done;
            }
            p = skip_value(p);
            p = skip_ws(p);
            if (*p == ',') p = skip_ws(p + 1);
        }
    } else if (is_array) {
        if (*p != '[') goto done;
        p = skip_ws(p + 1);
        for (int i = 0; i < index; i++) {
            p = skip_value(p);
            p = skip_ws(p);
            if (*p == ',') p = skip_ws(p + 1);
        }
        const char* val_start = p;
        p = skip_value(p);
        size_t val_len = p - val_start;
        char* sub_json = malloc(val_len + 1);
        memcpy(sub_json, val_start, val_len);
        sub_json[val_len] = '\0';
        result = find_in_json(sub_json, next_segment);
        free(sub_json);
    }
    
done:
    free(path_copy);
    return result;
}

int main(int argc, char* argv[]) {
    if (argc < 3) { fprintf(stderr, "Usage: json_parser <file> <path>\n"); return 1; }
    FILE* f = fopen(argv[1], "r");
    if (!f) return 1;
    fseek(f, 0, SEEK_END); long size = ftell(f); fseek(f, 0, SEEK_SET);
    char* raw = calloc(size + 1, 1);
    size_t n = fread(raw, 1, size, f); fclose(f); raw[n] = '\0';

    /* Strip markdown fences only if the file content itself starts with
     * markdown (i.e., the LLM wrapped its output in code fences). If the
     * file is a JSON object, the markdown may be embedded inside string
     * values (e.g., Ollama message.content), which must NOT be stripped here. */
    char* json_start = raw;
    const char* s = skip_ws(raw);
    if (*s != '{') {
        char* start = strstr(raw, "```json");
        if (start) json_start = start + 7;
        char* mdfence_end = strstr(json_start, "```");
        if (mdfence_end) *mdfence_end = '\0';
    }

    char* val = find_in_json(json_start, argv[2]);
    if (val) {
        if (val[0] == '"') {
            size_t vlen = strlen(val);
            char* clean = unescape(val + 1, vlen - 2);
            strip_markdown(clean);
            printf("%s", clean);
            free(clean);
        } else {
            printf("%s", val);
        }
        free(val);
        free(raw);
        return 0;
    }
    free(raw);
    return 1;
}
