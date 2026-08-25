/* run_native_tools.c - Native-tools probe harness (pure C).
 *
 * Tests whether a model emits native tool_calls via Ollama /api/chat for
 * code read/edit/exec (tool contract aligned to gem-dev standard:
 * read_file(path), edit_file(path,search,replace), exec_cmd(cmd)).
 *
 * Native path (models that accept "tools"): send payload with tools, loop
 * on message.tool_calls -> execute -> feed role=tool result -> re-query.
 * Post-hack path (post_hack* scenarios): no tools field; parse plain-text
 * {"tool":..,"args":{..}} JSON out of the model's content (agent-45 legacy
 * TOOL:-style hack, kept here only to measure hallucination, not as the
 * house pattern). The house pattern is deterministic app-side detection,
 * which this harness does NOT model - it measures the model itself.
 *
 * Usage: run_native_tools [model ...] [--scenario=read|edit|edit_hinted|run|post_hack|post_hack_noop]
 * Results: results/<model>/<scenario>.json + results/summary.txt
 * Fixture restored pristine before each scenario. Build: cc -O2 -Wall.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>

#define OLLAMA "http://10.0.0.144:11434/api/chat"
#define WORK "/tmp/code-tools-harness-work"
#define FIX "fixtures/sample.c"
#define BASE_DIR "."
#define MAX_TURNS 6
#define MAX_MSGS 64
#define BIG 65536
#define MID 16384

typedef struct {
    char role[16];
    char content[BIG];
    char tc_name[64];
    char tc_args[16384];
} Msg;

/* ---------- tiny JSON helpers (raw substring / unescaped string) ---------- */

/* decode JSON escapes (\" \\ \n \r \t \/ \uXXXX) into dst (>= len+1 bytes) */
static void json_decode(const char *src, size_t len, char *dst) {
    size_t i = 0, o = 0;
    while (i < len) {
        if (src[i] == '\\' && i + 1 < len) {
            i++;
            switch (src[i]) {
            case 'n': dst[o++] = '\n'; break;
            case 't': dst[o++] = '\t'; break;
            case 'r': dst[o++] = '\r'; break;
            case '"': dst[o++] = '"'; break;
            case '\\': dst[o++] = '\\'; break;
            case '/': dst[o++] = '/'; break;
            case 'u': {
                if (i + 4 < len) {
                    unsigned cp = 0;
                    for (int k = 1; k <= 4; k++) {
                        char c = src[i + k];
                        cp <<= 4;
                        if (c >= '0' && c <= '9') cp |= (unsigned)(c - '0');
                        else if (c >= 'a' && c <= 'f') cp |= (unsigned)(c - 'a' + 10);
                        else if (c >= 'A' && c <= 'F') cp |= (unsigned)(c - 'A' + 10);
                        else { cp = 0; break; }
                    }
                    i += 4;
                    if (cp) {
                        /* BMP only; surrogate pairs rare in tool args */
                        if (cp >= 0x80) {
                            if (cp < 0x800) {
                                dst[o++] = (char)(0xC0 | (cp >> 6));
                                dst[o++] = (char)(0x80 | (cp & 0x3F));
                            } else {
                                dst[o++] = (char)(0xE0 | (cp >> 12));
                                dst[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                                dst[o++] = (char)(0x80 | (cp & 0x3F));
                            }
                        } else {
                            dst[o++] = (char)cp;
                        }
                    }
                }
                break;
            }
            default: dst[o++] = src[i]; break;
            }
        } else {
            dst[o++] = src[i];
        }
        i++;
    }
    dst[o] = '\0';
}

static char *jraw(const char *json, const char *key) {
    const char *p = json;
    size_t klen = strlen(key);
    while ((p = strstr(p, key)) != NULL) {
        const char *q = p + klen;
        while (*q && *q != ':') q++;
        if (*q != ':') { p += 1; continue; }
        q++;
        while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r') q++;
        if (*q == '"') {
            q++;
            const char *start = q;
            while (*q && !(*q == '"' && *(q - 1) != '\\')) q++;
            size_t n = (size_t)(q - start);
            char *out = malloc(n + 1);
            json_decode(start, n, out);
            return out;
        }
        if (*q == '{' || *q == '[') {
            const char *start = q;
            int depth = 0, in_str = 0;
            while (*q) {
                if (in_str) { if (*q == '"' && *(q - 1) != '\\') in_str = 0; }
                else if (*q == '"') in_str = 1;
                else if (*q == '{' || *q == '[') depth++;
                else if (*q == '}' || *q == ']') { depth--; if (depth == 0) { q++; break; } }
                q++;
            }
            size_t n = (size_t)(q - start);
            char *out = malloc(n + 1);
            memcpy(out, start, n);
            out[n] = '\0';
            return out;
        }
        return NULL;
    }
    return NULL;
}

/* next '{' balanced object containing top-level "tool" key -> raw object */
static char *extract_tool_json(const char *text) {
    const char *p = text;
    while ((p = strchr(p, '{')) != NULL) {
        int depth = 0, in_str = 0;
        const char *start = p;
        while (*p) {
            if (in_str) { if (*p == '"' && *(p - 1) != '\\') in_str = 0; }
            else if (*p == '"') in_str = 1;
            else if (*p == '{') depth++;
            else if (*p == '}') { depth--; if (depth == 0) { p++; break; } }
            p++;
        }
        size_t n = (size_t)(p - start);
        char *obj = malloc(n + 1);
        memcpy(obj, start, n);
        obj[n] = '\0';
        if (strstr(obj, "\"tool\"")) return obj;
        free(obj);
        p = start + 1;
    }
    return NULL;
}

static void json_escape(FILE *f, const char *s) {
    while (*s) {
        if (*s == '"' || *s == '\\') { fputc('\\', f); fputc(*s, f); }
        else if (*s == '\n') fputs("\\n", f);
        else if (*s == '\r') fputs("\\r", f);
        else if (*s == '\t') fputs("\\t", f);
        else fputc(*s, f);
        s++;
    }
}

/* ---------- payload build ---------- */

static void write_tools(FILE *f) {
    fputs(",\"tools\":["
          "{\"type\":\"function\",\"function\":{\"name\":\"read_file\","
          "\"description\":\"Read the contents of a file from the local filesystem.\","
          "\"parameters\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\","
          "\"description\":\"The absolute or relative path to the file to read\"}},"
          "\"required\":[\"path\"]}}},"
          "{\"type\":\"function\",\"function\":{\"name\":\"edit_file\","
          "\"description\":\"Surgically search and replace a block of text in an existing file.\","
          "\"parameters\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\","
          "\"description\":\"The file path to edit\"},\"search\":{\"type\":\"string\","
          "\"description\":\"The exact contiguous block to match\"},\"replace\":{\"type\":\"string\","
          "\"description\":\"The new replacement text block\"}},\"required\":[\"path\",\"search\",\"replace\"]}}},"
          "{\"type\":\"function\",\"function\":{\"name\":\"exec_cmd\","
          "\"description\":\"Run a shell command on the host system.\","
          "\"parameters\":{\"type\":\"object\",\"properties\":{\"cmd\":{\"type\":\"string\","
          "\"description\":\"The shell command line string to execute\"}},\"required\":[\"cmd\"]}}}"
          "]", f);
}

static void build_payload(const char *path, const char *model, Msg *msgs, int n, int use_tools) {
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "{\"model\":\"%s\",\"messages\":[", model);
    for (int i = 0; i < n; i++) {
        if (i) fputs(",", f);
        fprintf(f, "{\"role\":\"%s\"", msgs[i].role);
        if (strcmp(msgs[i].role, "assistant") == 0 && msgs[i].tc_name[0]) {
            fputs(",\"content\":\"\"", f);
            fputs(",\"tool_calls\":[{\"function\":{\"name\":\"", f);
            fputs(msgs[i].tc_name, f);
            fputs("\",\"arguments\":", f);
            fputs(msgs[i].tc_args, f);
            fputs("}}]", f);
        } else {
            fputs(",\"content\":\"", f);
            json_escape(f, msgs[i].content);
            fputs("\"", f);
        }
        fputs("}", f);
    }
    fputs("]", f);
    if (use_tools) write_tools(f);
    fputs(",\"stream\":false}", f);
    fclose(f);
}

/* ---------- HTTP via curl ---------- */

static char *http_chat(const char *payload_path) {
    char cmd[1024];
    snprintf(cmd, sizeof cmd, "curl -sS --max-time 300 %s -d @%s -o %s/response.json 2>/dev/null",
             OLLAMA, payload_path, WORK);
    if (system(cmd) != 0) return NULL;
    char path[512];
    snprintf(path, sizeof path, "%s/response.json", WORK);
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    size_t got = fread(buf, 1, sz, f);
    buf[got] = '\0';
    fclose(f);
    return buf;
}

static char *read_whole(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    /* Linux fopen() on a DIRECTORY succeeds and ftell returns LONG_MAX
     * (0x7fffffffffffffff) - malloc(LONG_MAX+1) would abort. Guard. */
    if (sz <= 0 || sz > (8 * 1024 * 1024)) { fclose(f); return NULL; }
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, sz, f);
    buf[got] = '\0';
    fclose(f);
    return buf;
}

static void restore_fixture(void) {
    mkdir(WORK, 0755);
    char cmd[1024];
    snprintf(cmd, sizeof cmd, "cp %s/%s %s/sample.c", BASE_DIR, FIX, WORK);
    if (system(cmd) != 0) {}
}

/* ---------- tool execution ---------- */

static void exec_read_file(const char *path, char *out, size_t outsz) {
    char p[1024];
    if (path[0] != '/') snprintf(p, sizeof p, "%s/%s", WORK, path);
    else snprintf(p, sizeof p, "%s", path);
    char *content = read_whole(p);
    if (!content) { snprintf(out, outsz, "[read_file] error: cannot read %s", p); return; }
    snprintf(out, outsz, "[read_file] %s", content);
    free(content);
}

static void exec_edit_file(const char *path, const char *search, const char *replace,
                           char *out, size_t outsz) {
    char p[1024];
    if (path[0] != '/') snprintf(p, sizeof p, "%s/%s", WORK, path);
    else snprintf(p, sizeof p, "%s", path);
    char *content = read_whole(p);
    if (!content) { snprintf(out, outsz, "[edit_file] error: cannot read %s", p); return; }
    size_t slen = strlen(search);
    int count = 0;
    char *at = content;
    while ((at = strstr(at, search)) != NULL) { count++; at += slen; }
    if (count != 1) {
        snprintf(out, outsz, "[edit_file] ERROR: expected exactly one match, found %d", count);
        free(content);
        return;
    }
    char *pos = strstr(content, search);
    size_t before = (size_t)(pos - content);
    char *new_content = malloc(strlen(content) + strlen(replace) + 1);
    memcpy(new_content, content, before);
    strcpy(new_content + before, replace);
    strcat(new_content + before, pos + slen);
    FILE *f = fopen(p, "w");
    if (!f) { snprintf(out, outsz, "[edit_file] error: cannot write %s", p); free(new_content); free(content); return; }
    fputs(new_content, f);
    fclose(f);
    snprintf(out, outsz, "[edit_file] replaced one occurrence. New contents:\n%s", new_content);
    free(new_content);
    free(content);
}

static void exec_cmd(const char *cmd, char *out, size_t outsz) {
    char full[4096];
    snprintf(full, sizeof full, "cd %s && ( %s ) 2>&1", WORK, cmd);
    FILE *fp = popen(full, "r");
    if (!fp) { snprintf(out, outsz, "[exec_cmd] error: popen failed"); return; }
    char buf[8192];
    size_t used = 0;
    out[0] = '\0';
    while (fgets(buf, sizeof buf, fp)) {
        size_t l = strlen(buf);
        if (used + l < outsz - 1) { memcpy(out + used, buf, l); used += l; }
    }
    int rc = pclose(fp);
    out[used] = '\0';
    /* prepend exit code, keep the tail */
    char tmp[16384];
    snprintf(tmp, sizeof tmp, "[exec_cmd] exit=%d\n%s", rc, out);
    snprintf(out, outsz, "%s", tmp);
}

static void execute_tool(const char *name, const char *args_json, char *out, size_t outsz) {
    char *path = NULL, *search = NULL, *replace = NULL, *cmd = NULL;
    if (strcmp(name, "read_file") == 0) {
        path = jraw(args_json, "\"path\"");
        exec_read_file(path ? path : "", out, outsz);
    } else if (strcmp(name, "edit_file") == 0) {
        path = jraw(args_json, "\"path\"");
        search = jraw(args_json, "\"search\"");
        replace = jraw(args_json, "\"replace\"");
        exec_edit_file(path ? path : "", search ? search : "", replace ? replace : "", out, outsz);
    } else if (strcmp(name, "exec_cmd") == 0) {
        cmd = jraw(args_json, "\"cmd\"");
        exec_cmd(cmd ? cmd : "", out, outsz);
    } else {
        snprintf(out, outsz, "[error] unknown tool %s", name);
    }
    free(path); free(search); free(replace); free(cmd);
}

/* ---------- native tools loop ---------- */

static int run_native_loop(const char *model, const char *prompt, const char *tag,
                           const char *scenario, Msg *msgs, int *n_out) {
    (void)scenario;
    Msg *msgs_local = calloc(MAX_MSGS, sizeof(Msg));
    int n = 0;
    snprintf(msgs_local[n].role, sizeof msgs_local[0].role, "user");
    snprintf(msgs_local[n].content, sizeof msgs_local[0].content, "%s", prompt);
    n++;

    char payload_path[512], tagdir[512];
    snprintf(tagdir, sizeof tagdir, "%s/results/%s", BASE_DIR, tag);
    mkdir(tagdir, 0755);
    snprintf(payload_path, sizeof payload_path, "%s/payload.json", WORK);

    for (int turn = 1; turn <= MAX_TURNS; turn++) {
        build_payload(payload_path, model, msgs_local, n, 1);
        char *resp = http_chat(payload_path);
        if (!resp) { free(msgs_local); return 0; }
        char *msg = jraw(resp, "\"message\"");
        if (!msg) { free(resp); free(msgs_local); return 0; }
        char *content = jraw(msg, "\"content\"");
        char *tc_arr = jraw(msg, "\"tool_calls\"");
        snprintf(msgs_local[n].role, sizeof msgs_local[0].role, "assistant");
        if (content) snprintf(msgs_local[n].content, sizeof msgs_local[0].content, "%s", content);
        else msgs_local[n].content[0] = '\0';
        if (tc_arr && strcmp(tc_arr, "[]") != 0 && tc_arr[0] != '\0') {
            /* parse first tool_call function name + arguments */
            char *fn = jraw(tc_arr, "\"function\"");
            char *name = NULL, *args = NULL;
            if (fn) {
                name = jraw(fn, "\"name\"");
                args = jraw(fn, "\"arguments\"");
            }
            if (name) snprintf(msgs_local[n].tc_name, sizeof msgs_local[0].tc_name, "%s", name);
            if (args) {
                /* arguments may be object or quoted string; keep raw */
                if (args[0] != '{' && args[0] != '[') {
                    char q[16500];
                    snprintf(q, sizeof q, "\"%s\"", args);
                    size_t qlen = strlen(q) + 1;
                    if (qlen > sizeof msgs_local[0].tc_args) qlen = sizeof msgs_local[0].tc_args;
                    memcpy(msgs_local[n].tc_args, q, qlen);
                    msgs_local[n].tc_args[sizeof msgs_local[0].tc_args - 1] = '\0';
                } else {
                    snprintf(msgs_local[n].tc_args, sizeof msgs_local[0].tc_args, "%s", args);
                }
            } else {
                snprintf(msgs_local[n].tc_args, sizeof msgs_local[0].tc_args, "{}");
            }
            free(fn); free(name); free(args);
            n++;
            /* execute and append tool result */
            snprintf(msgs_local[n].role, sizeof msgs_local[0].role, "tool");
            char result[20000];
            execute_tool(msgs_local[n - 1].tc_name, msgs_local[n - 1].tc_args, result, sizeof result);
            snprintf(msgs_local[n].content, sizeof msgs_local[0].content, "%s", result);
            n++;
            free(content); free(tc_arr); free(msg); free(resp);
            continue;
        }
        /* no tool calls -> final text answer */
        n++;
        free(content); free(tc_arr); free(msg); free(resp);
        break;
    }
    *n_out = n;
    memcpy(msgs, msgs_local, sizeof(Msg) * n);
    free(msgs_local);
    return 1;
}

/* ---------- post-hack loop (no tools field, parse text JSON) ---------- */

static int run_post_hack(const char *model, const char *prompt, const char *tag,
                         const char *scenario, Msg *msgs, int *n_out) {
    (void)scenario;
    Msg *msgs_local = calloc(MAX_MSGS, sizeof(Msg));
    int n = 0;
    snprintf(msgs_local[n].role, sizeof msgs_local[0].role, "user");
    snprintf(msgs_local[n].content, sizeof msgs_local[0].content,
             "You are an agent with file tools. When a user asks you to read, edit, or run a file, "
             "reply with EXACTLY one JSON object and nothing else, of the form "
             "{\"tool\":\"<name>\",\"args\":{...}}. Available tools: "
             "read_file(path), edit_file(path,search,replace), exec_cmd(cmd).\n\n%s", prompt);
    n++;

    char payload_path[512], tagdir[512];
    snprintf(tagdir, sizeof tagdir, "%s/results/%s", BASE_DIR, tag);
    mkdir(tagdir, 0755);
    snprintf(payload_path, sizeof payload_path, "%s/payload.json", WORK);

    for (int turn = 1; turn <= MAX_TURNS; turn++) {
        build_payload(payload_path, model, msgs_local, n, 0);
        char *resp = http_chat(payload_path);
        if (!resp) { free(msgs_local); return 0; }
        char *msg = jraw(resp, "\"message\"");
        char *content = msg ? jraw(msg, "\"content\"") : NULL;
        snprintf(msgs_local[n].role, sizeof msgs_local[0].role, "assistant");
        if (content) snprintf(msgs_local[n].content, sizeof msgs_local[0].content, "%s", content);
        else msgs_local[n].content[0] = '\0';
        char *obj = content ? extract_tool_json(content) : NULL;
        free(msg);
        if (obj) {
            char *tname = jraw(obj, "\"tool\"");
            char *targs = jraw(obj, "\"args\"");
            snprintf(msgs_local[n].tc_name, sizeof msgs_local[0].tc_name, "%s", tname ? tname : "");
            snprintf(msgs_local[n].tc_args, sizeof msgs_local[0].tc_args, "%s", targs ? targs : "{}");
            n++;
            snprintf(msgs_local[n].role, sizeof msgs_local[0].role, "user");
            char result[20000];
            execute_tool(msgs_local[n - 1].tc_name, msgs_local[n - 1].tc_args, result, sizeof result);
            snprintf(msgs_local[n].content, sizeof msgs_local[0].content, "Tool result:\n%s", result);
            n++;
            free(obj); free(tname); free(targs);
            free(content); free(resp);
            continue;
        }
        n++;
        free(obj); free(content); free(resp);
        break;
    }
    *n_out = n;
    memcpy(msgs, msgs_local, sizeof(Msg) * n);
    free(msgs_local);
    return 1;
}

/* ---------- HARNECIENT path (my-lawyer strategy, au11-hq/HARNECIENT-HACK.md) ----------
 * No tool-call requests at all: plain /api/chat (no tools field), a persona that
 * forbids structure, short plain-text questions, tolerant extraction, DETERMINISTIC
 * app-side dispatch, real-file folding, fallback on every step. The model never
 * picks a tool - it only supplies short plain-text snippets. */

static const char *HARNECIENT_PERSONA =
    "You are a code assistant in a game. Give short, direct, plain-text answers - "
    "one line or one sentence, never a list, never markdown, never JSON. "
    "If you don't know, say 'unknown' plainly.";

static void harnecient_ask(const char *model, const char *question, char *out, size_t outsz) {
    Msg *m = calloc(4, sizeof(Msg));
    snprintf(m[0].role, sizeof m[0].role, "system");
    snprintf(m[0].content, sizeof m[0].content, "%s", HARNECIENT_PERSONA);
    snprintf(m[1].role, sizeof m[1].role, "user");
    snprintf(m[1].content, sizeof m[1].content, "%s", question);
    char payload[512];
    snprintf(payload, sizeof payload, "%s/payload.json", WORK);
    build_payload(payload, model, m, 2, 0);
    free(m);
    char *resp = http_chat(payload);
    if (!resp) { out[0] = '\0'; return; }
    char *msg = jraw(resp, "\"message\"");
    char *content = msg ? jraw(msg, "\"content\"") : NULL;
    if (content) snprintf(out, outsz, "%s", content);
    else out[0] = '\0';
    free(content); free(msg); free(resp);
}

/* Scenario transcript (msgs): user intent, [read_file] tool msg, assistant snippet,
 * [edit_file]/[exec_cmd] tool msg, assistant snippet. */
static int run_harnecient(const char *model, const char *scenario, Msg *msgs, int *n_out) {
    Msg *msgs_local = calloc(MAX_MSGS, sizeof(Msg));
    int n = 0;
    snprintf(msgs_local[n].role, sizeof msgs_local[0].role, "user");
    snprintf(msgs_local[n].content, sizeof msgs_local[0].content,
             "Task: %s.", scenario);
    n++;

    char *fixture = read_whole(WORK "/sample.c");
    if (!fixture) { free(msgs_local); return 0; }

    /* deterministic READ (app-side, no LLM) */
    snprintf(msgs_local[n].role, sizeof msgs_local[0].role, "tool");
    snprintf(msgs_local[n].content, sizeof msgs_local[0].content, "[read_file] %s", fixture);
    n++;

    if (strcmp(scenario, "harnecient_read") == 0) {
        char q[BIG]; char answer[20000];
        snprintf(q, sizeof q,
                 "Here is the content of a C program:\n%s\n"
                 "Summarize in one sentence what the program computes.", fixture);
        harnecient_ask(model, q, answer, sizeof answer);
        snprintf(msgs_local[n].role, sizeof msgs_local[0].role, "assistant");
        snprintf(msgs_local[n].content, sizeof msgs_local[0].content, "%s", answer);
        n++;
    } else if (strcmp(scenario, "harnecient_edit") == 0) {
        char q[BIG]; char answer[20000];
        snprintf(q, sizeof q,
                 "Here is a C program with an off-by-one bug:\n%s\n"
                 "The loop 'for (int i = 1; i < N; i++)' stops at N-1 instead of N. "
                 "Write the corrected for-loop line. One line of C code, no explanation.", fixture);
        harnecient_ask(model, q, answer, sizeof answer);
        snprintf(msgs_local[n].role, sizeof msgs_local[0].role, "assistant");
        snprintf(msgs_local[n].content, sizeof msgs_local[0].content, "[suggested_fix] %s", answer);
        n++;

        /* hand-holding: model noise is TOLERATED, never trusted verbatim. We credit
         * the model only if it identified the fix concept; the applied line is always
         * the canonical correct one so model garbage can never corrupt the code. */
        int model_ok = (strstr(answer, "<=") != NULL && strstr(answer, "i = 1") != NULL);
        char edit_out[20000];
        exec_edit_file(WORK "/sample.c", "for (int i = 1; i < N; i++)",
                       "for (int i = 1; i <= N; i++)", edit_out, sizeof edit_out);
        snprintf(msgs_local[n].role, sizeof msgs_local[0].role, "tool");
        snprintf(msgs_local[n].content, sizeof msgs_local[0].content,
                 "[edit_file] %s (model_suggested_fix=%s)", edit_out, model_ok ? "true" : "false");
        n++;
    } else { /* harnecient_run */
        char run_out[20000];
        exec_cmd("gcc -o sample sample.c && ./sample", run_out, sizeof run_out);
        snprintf(msgs_local[n].role, sizeof msgs_local[0].role, "tool");
        snprintf(msgs_local[n].content, sizeof msgs_local[0].content, "%s", run_out);
        n++;
        char q[BIG]; char answer[20000];
        snprintf(q, sizeof q,
                 "A C program printed this output:\n%s\n"
                 "In one sentence, what does the program compute?", run_out);
        harnecient_ask(model, q, answer, sizeof answer);
        snprintf(msgs_local[n].role, sizeof msgs_local[0].role, "assistant");
        snprintf(msgs_local[n].content, sizeof msgs_local[0].content, "%s", answer);
        n++;
    }

    free(fixture);
    *n_out = n;
    memcpy(msgs, msgs_local, sizeof(Msg) * n);
    free(msgs_local);
    return 1;
}

/* ---------- verdict + results ---------- */

static void write_results(const char *model, const char *tag, const char *scenario,
                          Msg *msgs, int n, const char *verdict_json) {
    char dir[512], path[1024];
    snprintf(dir, sizeof dir, "%s/results/%s", BASE_DIR, tag);
    mkdir(dir, 0755);
    snprintf(path, sizeof path, "%s/%s.json", dir, scenario);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "{\n\"model\": \"%s\",\n\"scenario\": \"%s\",\n\"turns\": [\n", model, scenario);
    for (int i = 0; i < n; i++) {
        fprintf(f, "  {\"role\":\"%s\"", msgs[i].role);
        if (strcmp(msgs[i].role, "assistant") == 0 && msgs[i].tc_name[0]) {
            fprintf(f, ",\"tool_call\":{\"name\":\"%s\",\"arguments\":%s}", msgs[i].tc_name, msgs[i].tc_args);
        } else {
            fprintf(f, ",\"content\":\"");
            json_escape(f, msgs[i].content);
            fprintf(f, "\"");
        }
        fprintf(f, "}%s\n", (i == n - 1) ? "" : ",");
    }
    fprintf(f, "],\n\"verdict\": %s\n}\n", verdict_json);
    fclose(f);

    FILE *s = fopen("results/summary.txt", "a");
    if (s) { fprintf(s, "%s | %s | %s\n", model, scenario, verdict_json); fclose(s); }
}

static void run_all(const char *model, int only, char **only_scen) {
    char tag[128];
    snprintf(tag, sizeof tag, "%s", model);
    for (char *c = tag; *c; c++) if (*c == '/' || *c == ':') *c = '_';

    const char *prompts[][2] = {
        {"read",
         "Use the read_file tool to read " WORK "/sample.c, then summarize in a sentence what the program computes."},
        {"edit",
         "Use the read_file tool to read " WORK "/sample.c, then use the edit_file tool to fix the "
         "off-by-one bug so the loop includes N. When done, reply with the corrected printf line."},
        {"edit_hinted",
         "Use the edit_file tool to fix " WORK "/sample.c: search for the exact block "
         "'for (int i = 1; i < N; i++)' and replace it with 'for (int i = 1; i <= N; i++)'. "
         "Then reply with the corrected printf line."},
        {"run",
         "Use the exec_cmd tool to compile and run " WORK "/sample.c: gcc -o sample sample.c && ./sample. "
         "Then tell me what the program printed."},
        {"post_hack",
         "Read the file " WORK "/sample.c using the read_file tool, then summarize what it computes."},
        {"post_hack_noop", "Say hello in one sentence."},
        {"harnecient_read", "Read " WORK "/sample.c and summarize what it computes."},
        {"harnecient_edit", "Fix the off-by-one bug in " WORK "/sample.c so it prints the sum through N."},
        {"harnecient_run", "Compile and run " WORK "/sample.c, then tell me what it printed."},
    };
    int np = 9;
    for (int i = 0; i < np; i++) {
        const char *scenario = prompts[i][0];
        if (only && !only_scen[0]) {}
        if (only) {
            int match = 0;
            for (int k = 0; only_scen[k]; k++) if (strcmp(scenario, only_scen[k]) == 0) match = 1;
            if (!match) continue;
        }
        printf("=== %s / %s ===\n", model, scenario);
        fflush(stdout);
        restore_fixture();
        Msg msgs[MAX_MSGS];
        int n = 0;
        int is_post = strncmp(scenario, "post_hack", 9) == 0;
        int is_harnecient = strncmp(scenario, "harnecient_", 11) == 0;
        int ok = is_post ? run_post_hack(model, prompts[i][1], tag, scenario, msgs, &n)
                         : is_harnecient ? run_harnecient(model, scenario, msgs, &n)
                                         : run_native_loop(model, prompts[i][1], tag, scenario, msgs, &n);
        if (!ok) { printf("  api/loop error\n"); continue; }

        /* verdict */
        char v[2048] = "";
        if (is_post) {
            int emitted = 0;
            for (int j = 0; j < n; j++)
                if (strcmp(msgs[j].role, "assistant") == 0 && msgs[j].tc_name[0]) emitted = 1;
            if (strcmp(scenario, "post_hack") == 0) {
                int called = 0;
                for (int j = 0; j < n; j++)
                    if (strcmp(msgs[j].role, "assistant") == 0 && strcmp(msgs[j].tc_name, "read_file") == 0)
                        called = 1;
                const char *final_content = "";
                for (int j = n - 1; j >= 0; j--)
                    if (strcmp(msgs[j].role, "assistant") == 0 && msgs[j].content[0]) { final_content = msgs[j].content; break; }
                snprintf(v, sizeof v,
                         "{\"emitted_tool_json\":%s,\"read_file_called\":%s,\"model_used_contents\":%s}",
                         emitted ? "true" : "false", called ? "true" : "false",
                         (strstr(final_content, "sum") || strstr(final_content, "loop")) ? "true" : "false");
            } else {
                snprintf(v, sizeof v, "{\"emitted_tool_json\":%s,\"no_tool_hallucination\":%s}",
                         emitted ? "true" : "false", emitted ? "false" : "true");
            }
        } else {
            int emitted = 0;
            for (int j = 0; j < n; j++)
                if (strcmp(msgs[j].role, "assistant") == 0 && msgs[j].tc_name[0]) emitted = 1;
            if (strcmp(scenario, "edit") == 0 || strcmp(scenario, "edit_hinted") == 0) {
                char ck[1024];
                snprintf(ck, sizeof ck, "cd %s && gcc -o sample sample.c && ./sample 2>&1", WORK);
    FILE *fp = popen(ck, "r");
    char out[512] = "";
    if (fp) { char *r = fgets(out, sizeof out, fp); (void)r; pclose(fp); }
                snprintf(v, sizeof v, "{\"emitted_native_tool_calls\":%s,\"edit_compiles_and_prints_15\":%s}",
                         emitted ? "true" : "false", strstr(out, "= 15") ? "true" : "false");
            } else if (strcmp(scenario, "run") == 0) {
                int ran = 0;
                const char *exec_out = "";
                for (int j = 0; j < n; j++)
                    if (strcmp(msgs[j].role, "tool") == 0 && strstr(msgs[j].content, "[exec_cmd]")) {
                        ran = 1;
                        exec_out = msgs[j].content;
                    }
                const char *final_content = "";
                for (int j = n - 1; j >= 0; j--)
                    if (strcmp(msgs[j].role, "assistant") == 0 && msgs[j].content[0]) { final_content = msgs[j].content; break; }
                snprintf(v, sizeof v, "{\"emitted_native_tool_calls\":%s,\"ran_cleanly\":%s,\"model_relayed_output\":%s}",
                         emitted ? "true" : "false",
                         (ran && strstr(exec_out, "exit=0") && strstr(exec_out, "sum(1..5)")) ? "true" : "false",
                         (strstr(final_content, "10") && strstr(final_content, "sum")) ? "true" : "false");
            } else if (strcmp(scenario, "harnecient_edit") == 0) {
                char ck[1024];
                snprintf(ck, sizeof ck, "cd %s && gcc -o sample sample.c && ./sample 2>&1", WORK);
                FILE *fp = popen(ck, "r");
                char out[512] = "";
                if (fp) { char *r = fgets(out, sizeof out, fp); (void)r; pclose(fp); }
                const char *edit_out = "";
                int model_ok = 0;
                for (int j = 0; j < n; j++) {
                    if (strcmp(msgs[j].role, "tool") == 0 && strstr(msgs[j].content, "[edit_file]")) {
                        edit_out = msgs[j].content;
                        if (strstr(msgs[j].content, "model_suggested_fix=true")) model_ok = 1;
                    }
                }
                snprintf(v, sizeof v,
                         "{\"deterministic_edit_applied\":%s,\"compiles_and_prints_15\":%s,\"model_suggested_fix\":%s}",
                         (strstr(edit_out, "replaced one occurrence")) ? "true" : "false",
                         strstr(out, "= 15") ? "true" : "false",
                         model_ok ? "true" : "false");
            } else if (strcmp(scenario, "harnecient_run") == 0) {
                const char *exec_out = "";
                for (int j = 0; j < n; j++)
                    if (strcmp(msgs[j].role, "tool") == 0 && strstr(msgs[j].content, "[exec_cmd]")) exec_out = msgs[j].content;
                const char *final_content = "";
                for (int j = n - 1; j >= 0; j--)
                    if (strcmp(msgs[j].role, "assistant") == 0 && msgs[j].content[0]) { final_content = msgs[j].content; break; }
                snprintf(v, sizeof v,
                         "{\"deterministic_run\":true,\"ran_cleanly\":%s,\"model_interpreted\":%s}",
                         (strstr(exec_out, "exit=0") && strstr(exec_out, "sum(1..5)")) ? "true" : "false",
                         (strstr(final_content, "sum") || strstr(final_content, "compute")) ? "true" : "false");
            } else if (strcmp(scenario, "harnecient_read") == 0) {
                const char *final_content = "";
                for (int j = n - 1; j >= 0; j--)
                    if (strcmp(msgs[j].role, "assistant") == 0 && msgs[j].content[0]) { final_content = msgs[j].content; break; }
                snprintf(v, sizeof v,
                         "{\"deterministic_read\":true,\"model_summarized\":%s}",
                         (strstr(final_content, "sum") || strstr(final_content, "loop")) ? "true" : "false");
            } else {
                int read_occ = 0;
                const char *final_content = "";
                for (int j = 0; j < n; j++) {
                    if (strcmp(msgs[j].role, "tool") == 0 && strstr(msgs[j].content, "[read_file]")) read_occ = 1;
                    if (strcmp(msgs[j].role, "assistant") == 0 && msgs[j].content[0]) final_content = msgs[j].content;
                }
                snprintf(v, sizeof v, "{\"emitted_native_tool_calls\":%s,\"read_occurred\":%s,\"model_used_contents\":%s}",
                         emitted ? "true" : "false", read_occ ? "true" : "false",
                         (strstr(final_content, "sum") || strstr(final_content, "loop")) ? "true" : "false");
            }
        }
        write_results(model, tag, scenario, msgs, n, v);
        printf("%s | %s | %s\n", model, scenario, v);
        fflush(stdout);
    }
}

int main(int argc, char **argv) {
    mkdir("results", 0755);
    /* clear stale summary for this run set */
    FILE *s = fopen("results/summary.txt", "w");
    if (s) fclose(s);

    int only = 0;
    char *only_scen[16] = {0};
    int oc = 0;
    int model_argc = 0;
    char *models[8];
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--scenario=", 11) == 0) {
            only = 1;
            if (oc < 15) only_scen[oc++] = argv[i] + 11;
        } else if (model_argc < 8) {
            models[model_argc++] = argv[i];
        }
    }
    only_scen[oc] = NULL;
    if (model_argc == 0) { models[0] = "stable-code:latest"; models[1] = "llama3-groq-tool-use:8b"; model_argc = 2; }
    for (int i = 0; i < model_argc; i++) run_all(models[i], only, only_scen);
    printf("summary -> results/summary.txt\n");
    return 0;
}
