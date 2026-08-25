// tools/web_search.c - DuckDuckGo Instant Answer wrapper
#define _GNU_SOURCE
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_OUTPUT 131072
#define MAX_ITEMS 5

static void trim_in_place(char *s) {
    if (!s) return;
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) s[--len] = '\0';
    size_t start = 0;
    while (s[start] && isspace((unsigned char)s[start])) start++;
    if (start > 0) memmove(s, s + start, len - start + 1);
}

static void append_text(char *dst, size_t dst_sz, const char *text) {
    if (!dst || !text || dst_sz == 0) return;
    size_t used = strlen(dst);
    if (used >= dst_sz - 1) return;
    snprintf(dst + used, dst_sz - used, "%s", text);
}

static void append_line(char *dst, size_t dst_sz, const char *text) {
    if (!text || !*text) return;
    append_text(dst, dst_sz, text);
    append_text(dst, dst_sz, "\n");
}

static char *json_unescape(const char *src, size_t len) {
    char *out = calloc(len + 1, 1);
    if (!out) return NULL;
    size_t i = 0, j = 0;
    while (i < len) {
        if (src[i] == '\\' && i + 1 < len) {
            i++;
            switch (src[i]) {
                case 'n': out[j++] = '\n'; break;
                case 'r': out[j++] = '\r'; break;
                case 't': out[j++] = '\t'; break;
                case '"': out[j++] = '"'; break;
                case '\\': out[j++] = '\\'; break;
                case '/': out[j++] = '/'; break;
                case 'b': out[j++] = '\b'; break;
                case 'f': out[j++] = '\f'; break;
                case 'u':
                    if (i + 4 < len) {
                        out[j++] = '?';
                        i += 4;
                    } else {
                        out[j++] = 'u';
                    }
                    break;
                default: out[j++] = src[i]; break;
            }
        } else {
            out[j++] = src[i];
        }
        i++;
    }
    out[j] = '\0';
    return out;
}

static char *extract_json_string(const char *json, const char *key, const char *start_at) {
    if (!json || !key) return NULL;
    const char *p = start_at ? start_at : json;
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
    p = strstr(p, pattern);
    if (!p) return NULL;
    p += strlen(pattern);
    const char *start = p;
    while (*p) {
        if (*p == '"' && (p == start || p[-1] != '\\')) break;
        p++;
    }
    if (*p != '"') return NULL;
    return json_unescape(start, (size_t)(p - start));
}

static void html_entity_decode(char *s) {
    if (!s) return;
    char *src = s;
    char *dst = s;
    while (*src) {
        if (strncmp(src, "&quot;", 6) == 0) {
            *dst++ = '"';
            src += 6;
        } else if (strncmp(src, "&amp;", 5) == 0) {
            *dst++ = '&';
            src += 5;
        } else if (strncmp(src, "&lt;", 4) == 0) {
            *dst++ = '<';
            src += 4;
        } else if (strncmp(src, "&gt;", 4) == 0) {
            *dst++ = '>';
            src += 4;
        } else if (strncmp(src, "&#39;", 5) == 0) {
            *dst++ = '\'';
            src += 5;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

static char *run_duckduckgo_query(const char *query) {
    int pipefd[2];
    if (pipe(pipefd) != 0) return NULL;

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return NULL;
    }

    if (pid == 0) {
        char q_arg[2048];
        snprintf(q_arg, sizeof(q_arg), "q=%s", query);

        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        char *argv[] = {
            "curl",
            "-fsSLG",
            "--max-time", "15",
            "--data-urlencode", q_arg,
            "--data", "format=json",
            "--data", "no_html=1",
            "--data", "no_redirect=1",
            "--data", "skip_disambig=1",
            "https://api.duckduckgo.com/",
            NULL
        };
        execvp("curl", argv);
        _exit(127);
    }

    close(pipefd[1]);
    char *buf = calloc(MAX_OUTPUT, 1);
    if (!buf) {
        close(pipefd[0]);
        return NULL;
    }

    size_t total = 0;
    ssize_t n;
    while ((n = read(pipefd[0], buf + total, MAX_OUTPUT - total - 1)) > 0) {
        total += (size_t)n;
        if (total >= MAX_OUTPUT - 1) break;
    }
    close(pipefd[0]);
    buf[total] = '\0';

    int status = 0;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0 || total == 0) {
        free(buf);
        return NULL;
    }
    return buf;
}

static int collect_topic_items(const char *json, const char *section_name, char items[][2048], int max_items) {
    if (!json || !section_name || max_items <= 0) return 0;
    char section_pat[128];
    snprintf(section_pat, sizeof(section_pat), "\"%s\":[", section_name);
    const char *section = strstr(json, section_pat);
    if (!section) return 0;

    const char *p = section;
    int count = 0;
    while (count < max_items) {
        char *text = extract_json_string(json, "Text", p);
        if (!text) break;

        char *url = extract_json_string(json, "FirstURL", p);
        trim_in_place(text);
        if (url) trim_in_place(url);
        html_entity_decode(text);
        if (url) html_entity_decode(url);

        if (*text) {
            if (url && *url) {
                snprintf(items[count], 2048, "%s - %s", text, url);
            } else {
                snprintf(items[count], 2048, "%s", text);
            }
            count++;
        }

        const char *next = strstr(p, "\"Text\":\"");
        if (!next) {
            free(text);
            if (url) free(url);
            break;
        }
        p = next + 8;
        free(text);
        if (url) free(url);
    }

    return count;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: web_search <query>\n");
        return 1;
    }

    char *json = run_duckduckgo_query(argv[1]);
    if (!json) {
        printf("DuckDuckGo search failed for '%s'.\n", argv[1]);
        return 1;
    }

    char *answer = extract_json_string(json, "Answer", NULL);
    char *abstract = extract_json_string(json, "AbstractText", NULL);
    char *definition = extract_json_string(json, "Definition", NULL);
    char *heading = extract_json_string(json, "Heading", NULL);
    char related[MAX_ITEMS][2048] = {{0}};
    char results[MAX_ITEMS][2048] = {{0}};
    int related_count = collect_topic_items(json, "RelatedTopics", related, MAX_ITEMS);
    int results_count = collect_topic_items(json, "Results", results, MAX_ITEMS);
    int has_content = 0;

    if (answer) { trim_in_place(answer); html_entity_decode(answer); }
    if (abstract) { trim_in_place(abstract); html_entity_decode(abstract); }
    if (definition) { trim_in_place(definition); html_entity_decode(definition); }
    if (heading) { trim_in_place(heading); html_entity_decode(heading); }

    char out[16384] = {0};
    char line[2304];

    snprintf(line, sizeof(line), "DuckDuckGo result for '%s':", argv[1]);
    append_line(out, sizeof(out), line);

    if (heading && *heading) {
        snprintf(line, sizeof(line), "Heading: %s", heading);
        append_line(out, sizeof(out), line);
        has_content = 1;
    }
    if (answer && *answer) {
        snprintf(line, sizeof(line), "Answer: %s", answer);
        append_line(out, sizeof(out), line);
        has_content = 1;
    }
    if (abstract && *abstract) {
        snprintf(line, sizeof(line), "Abstract: %s", abstract);
        append_line(out, sizeof(out), line);
        has_content = 1;
    }
    if (definition && *definition) {
        snprintf(line, sizeof(line), "Definition: %s", definition);
        append_line(out, sizeof(out), line);
        has_content = 1;
    }

    if (results_count > 0) {
        append_line(out, sizeof(out), "Results:");
        for (int i = 0; i < results_count; i++) {
            snprintf(line, sizeof(line), "%d. %s", i + 1, results[i]);
            append_line(out, sizeof(out), line);
        }
        has_content = 1;
    }

    if (related_count > 0) {
        append_line(out, sizeof(out), "Related Topics:");
        for (int i = 0; i < related_count; i++) {
            snprintf(line, sizeof(line), "%d. %s", i + 1, related[i]);
            append_line(out, sizeof(out), line);
        }
        has_content = 1;
    }

    if (!has_content) {
        snprintf(out, sizeof(out), "DuckDuckGo had no instant answer or related topics for '%s'.", argv[1]);
    }

    printf("%s", out);

    free(json);
    if (answer) free(answer);
    if (abstract) free(abstract);
    if (definition) free(definition);
    if (heading) free(heading);
    return 0;
}
