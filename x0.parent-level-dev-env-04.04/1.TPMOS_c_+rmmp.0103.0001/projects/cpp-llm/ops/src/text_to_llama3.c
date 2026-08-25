// projects/cpp-llm/ops/src/text_to_llama3.c - Converts JSON context history
// ([{"role":...,"content":...}, ...], the same format json_state.c produces
// for every TPMOS agent project) into a raw Llama3 token-formatted prompt
// for llama.cpp's /completion endpoint.
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char* role;
    char* content;
} Message;

static char* read_file(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = malloc(size + 1);
    if (buf) {
        size_t n = fread(buf, 1, size, f);
        buf[n] = '\0';
    }
    fclose(f);
    return buf;
}

static char* json_unescape(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char* out = malloc(len + 1);
    if (!out) return NULL;
    char* p = out;
    while (*s) {
        if (*s == '\\' && *(s + 1)) {
            s++;
            if (*s == 'n') *p++ = '\n';
            else if (*s == 't') *p++ = '\t';
            else if (*s == 'r') *p++ = '\r';
            else if (*s == '"') *p++ = '"';
            else if (*s == '\\') *p++ = '\\';
            else *p++ = *s;
        } else {
            *p++ = *s;
        }
        s++;
    }
    *p = '\0';
    return out;
}

static int parse_messages(const char* json, Message* messages, int max_messages) {
    int count = 0;
    const char* p = json;

    while (count < max_messages) {
        p = strstr(p, "\"role\"");
        if (!p) break;
        p = strchr(p, ':');
        if (!p) break;
        p++;
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (*p != '"') break;
        p++;
        const char* role_start = p;
        const char* role_end = strchr(p, '"');
        if (!role_end) break;
        size_t role_len = role_end - role_start;
        char* role = malloc(role_len + 1);
        memcpy(role, role_start, role_len);
        role[role_len] = '\0';

        p = role_end + 1;
        p = strstr(p, "\"content\"");
        if (!p) { free(role); break; }
        p = strchr(p, ':');
        if (!p) { free(role); break; }
        p++;
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (*p != '"') { free(role); break; }
        p++;

        const char* content_start = p;
        const char* q = p;
        while (*q) {
            if (*q == '\\' && *(q + 1)) {
                q += 2;
            } else if (*q == '"') {
                break;
            } else {
                q++;
            }
        }
        if (!*q) { free(role); break; }
        size_t content_len = q - content_start;
        char* content = malloc(content_len + 1);
        memcpy(content, content_start, content_len);
        content[content_len] = '\0';

        messages[count].role = role;
        messages[count].content = content;
        count++;
        p = q + 1;
    }
    return count;
}

int main(int argc, char* argv[]) {
    if (argc < 2) { fprintf(stderr, "Usage: text_to_llama3 <context.json>\n"); return 1; }

    char* json_data = read_file(argv[1]);
    if (!json_data) return 1;

    Message messages[1024];
    int count = parse_messages(json_data, messages, 1024);
    free(json_data);

    for (int i = 0; i < count; i++) {
        char* text = json_unescape(messages[i].content);
        const char* role = messages[i].role;

        if (strcmp(role, "system") == 0) {
            printf("<|start_header_id|>system<|end_header_id|>\n\n%s<|eot_id|>", text ? text : "");
        } else if (strcmp(role, "user") == 0) {
            printf("<|start_header_id|>user<|end_header_id|>\n\n%s<|eot_id|>", text ? text : "");
        } else if (strcmp(role, "assistant") == 0) {
            printf("<|start_header_id|>assistant<|end_header_id|>\n\n%s<|eot_id|>", text ? text : "");
        } else if (strcmp(role, "tool") == 0) {
            printf("<|start_header_id|>system<|end_header_id|>\n\nTOOL_RESULT: %s<|eot_id|>", text ? text : "");
        }

        if (text) free(text);
        free(messages[i].role);
        free(messages[i].content);
    }

    printf("<|start_header_id|>system<|end_header_id|>\n\nRespond ONLY with JSON.<|eot_id|><|start_header_id|>assistant<|end_header_id|>\n\n");

    return 0;
}
