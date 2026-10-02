/* horn_chat_backend - HORN_CHAT's LLM transport.
 *
 * One file, several providers, plus the tool-calling protocol. OpenRouter,
 * Groq and Poolside all expose an OpenAI-compatible POST /chat/completions
 * with a Bearer token and a choices[0].message reply, so they share one
 * request path, one reply parser and one payload writer. What differs —
 * endpoint, key file, model ladder, whether thinking defaults on — is table
 * data, not a second 300-line copy of this.
 *
 * PROVIDER ORDER IS FALLBACK ORDER, and it is measured, not alphabetical.
 * All three were probed with the SAME deliberately ambiguous prompt ("List
 * the files in the current directory using the tool.", which names no path)
 * because an unambiguous prompt flatters every provider:
 *
 *   groq       15/15 tool_calls across gpt-oss-120b, qwen3.8-27b,
 *              gpt-oss-20b          - best measured tool reliability
 *   poolside    8/10 tool_calls     - good, but it sometimes narrates
 *                                    about the tool instead of calling it
 *   openrouter  unavailable on this key while the free tier was spent
 *
 * Groq leads because tool-calling reliability is the property that matters
 * for HORN: it is HORN that will help build HALO, and an IRL harness
 * comparing two models needs the tool to fire consistently or the
 * comparison is measuring noise.
 *
 * TOOL CALLING. A model that wants to run a tool replies with
 * choices[0].message.tool_calls and content: null. That is NOT an empty
 * reply, and the first version of this file got it wrong: it only looked
 * for a content string, so every tool call came back as
 * "no-content-in-response" and the caller showed the user "no reply from
 * model" while the model had in fact asked for a tool. Exit code 10 now
 * means "assistant wants tools", which is a normal outcome, not a failure.
 *
 * Conversation state lives in pieces/horn/convo.json as a JSON array of
 * messages. horn_turn seeds it, this file appends the assistant turn, and
 * horn_turn appends the tool results and calls again. Keeping the loop in
 * horn_turn means this file stays a transport: it does not decide policy
 * and it does not execute anything.
 *
 * Adapted from entity-cli's proven ai_chat_openrouter.c (2026-09-30).
 * Changes from that original, all deliberate:
 *   - the provider table replaces its hardcoded OpenRouter endpoint
 *   - entity_state_dir comes from $HORN_ENTITY_DIR (set by horn_chat.sh),
 *     not a hardcoded "entity" subdir of the project root
 *   - the prompt comes from argv[1] (or $HORN_PROMPT) so the pal loop can
 *     drive it without mutating the orchestrator's environment
 *   - the reply lands in $HORN_REPLY_FILE (default
 *     pieces/horn/last_reply.txt) as well as stdout
 *
 * EXIT CODES - callers depend on the distinction:
 *   0  a final text reply was produced (last_reply.txt)
 *   1  bad usage, or no API key at all
 *   2  every provider/model was reachable but none answered
 *   3  every failure was a quota/rate limit (an account state, not a code
 *      fault - retrying cannot fix it until the window resets)
 *  10  the assistant wants to call tools (tool_calls.json); NOT an error
 *
 * Usage: horn_chat_backend.+x "<prompt>"
 * Env:  HORN_TOOLS=off          disable tool definitions entirely
 *       HORN_TOOL_CHOICE=required  force a tool call on every request
 *       HORN_MODEL_OVERRIDE=<id>  pin one provider/model, skipping the ladder
 * Self-contained: own root resolution, own helpers, no shared headers.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif
#define PATH_BUF  (MAX_PATH + 256)
#define REPLY_CAP 65536
#define RAW_CAP   (4 * 1024 * 1024)
#define CONVO_CAP (2 * 1024 * 1024)

typedef struct {
    const char *name;
    const char *endpoint;
    const char *key_env;   /* env var that overrides the key file */
    const char *key_file;  /* basename inside the entity state dir */
    const char *models[4]; /* NULL-terminated ladder */
    /* Poolside-hosted inference enables thinking by default, which
     * prepends a reasoning block to the reply. A chat box wants the
     * answer; set this and the payload carries
     * chat_template_kwargs.enable_thinking=false. Documented as a
     * Poolside-API-only field. */
    int no_thinking;
} Provider;

/* Verified live 2026-10-01/02. See the header for the measurement behind
 * the ordering. Model names go stale fast: a slug retired from a free tier
 * does not fail loudly, it answers HTTP 200 with an error BODY, which this
 * file reports as "no content". Refresh with:
 *   curl -s https://api.groq.com/openai/v1/models
 *   curl -s https://inference.poolside.ai/v1/models -H "Authorization: Bearer $KEY"
 *   curl -s https://openrouter.ai/api/v1/models
 */
static const Provider PROVIDERS[] = {
    { "groq",
      "https://api.groq.com/openai/v1/chat/completions",
      "GROQ_API_KEY", "raw_groq.txt",
      { "openai/gpt-oss-120b", "qwen/qwen3.8-27b", "openai/gpt-oss-20b", NULL }, 0 },

    { "poolside",
      "https://inference.poolside.ai/v1/chat/completions",
      "HORN_POOLSIDE_KEY", "raw_poolside.txt",
      { "poolside/laguna-s-2.1", "poolside/laguna-xs-2.1", NULL }, 1 },

    { "openrouter",
      "https://openrouter.ai/api/v1/chat/completions",
      "HORN_API_KEY", "openrouter_api_key.txt",
      { "nvidia/nemotron-3-super-120b-a12b:free",
        "nvidia/nemotron-3-ultra-550b-a55b:free",
        "inclusionai/ling-3.0-flash-sante:free", NULL }, 0 },
};
#define N_PROVIDERS ((int)(sizeof(PROVIDERS) / sizeof(PROVIDERS[0])))

static const char *SYS_PROMPT =
    "You are HORN, a terminal-based assistant. Answer clearly and concisely. "
    "Plain text only, no markdown fences. You have read-only tools for "
    "listing, reading and searching files in the project; use them when the "
    "answer depends on the actual contents of the code rather than on "
    "assumption.";

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) { snprintf(project_root, sizeof(project_root), "%s", env); return; }
    if (getcwd(project_root, sizeof(project_root)) == NULL)
        snprintf(project_root, sizeof(project_root), ".");
}

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long n = ftell(f);
    if (n < 0) { fclose(f); return NULL; }
    rewind(f);
    char *buf = malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)n, f);
    buf[got] = '\0';
    fclose(f);
    return buf;
}

static void trim_in_place(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' ||
                     s[n - 1] == ' '  || s[n - 1] == '\t'))
        s[--n] = '\0';
}

static const char *entity_state_dir(void) {
    const char *e = getenv("HORN_ENTITY_DIR");
    if (e && e[0]) return e;
    return NULL;
}

static char *path_in(const char *rel) {
    char *p = NULL;
    if (asprintf(&p, "%s/%s", project_root, rel) < 0) return NULL;
    return p;
}

/* ── JSON value slicing ────────────────────────────────────────────────
 *
 * The manifest and the response both need whole JSON values copied out
 * VERBATIM (a tools array, an assistant message object) rather than
 * re-serialized. Re-serializing would mean a real parser, and re-encoding
 * risks mangling escape sequences. So: walk the text, honouring string
 * literals and escapes, and copy the balanced value out as written. */

static const char *skip_ws(const char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

/* If p sits on a '{' or '[', copy that whole balanced value into out.
 * Returns 1 on success. Returns 0 for null/empty and leaves out empty -
 * providers send literal `null` for content alongside tool_calls. */
static int json_slice_value(const char *p, char *out, size_t out_sz) {
    out[0] = '\0';
    if (!p) return 0;
    p = skip_ws(p);
    if (*p != '{' && *p != '[') return 0;

    char open = *p, close = (open == '{') ? '}' : ']';
    int depth = 0, in_str = 0, esc = 0, closed = 0;
    size_t o = 0;

    for (; *p; p++) {
        char c = *p;
        if (in_str) {
            if (esc) esc = 0;
            else if (c == '\\') esc = 1;
            else if (c == '"') in_str = 0;
        } else {
            if (c == '"') in_str = 1;
            else if (c == open) depth++;
            else if (c == close) { depth--; if (depth == 0) closed = 1; }
        }
        /* Write the real character, THEN stop. Zeroing `c` first and then
         * appending it wrote a NUL in place of the closing bracket, so the
         * sliced array/object came back unterminated - "invalid character
         * ':' after array element" from every provider, on every request. */
        if (o < out_sz - 1) out[o++] = c;
        if (closed) break;
    }
    out[o] = '\0';
    return 1;
}

/* Find the value for "key" in a JSON object. `want_open` selects a '{' or
 * '[' value; pass 0 to accept whatever is there. */
static char *json_object_field(const char *json, const char *key, char want_open) {
    char pat[128];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = json ? strstr(json, pat) : NULL;
    if (!p) return NULL;
    p += strlen(pat);
    p = skip_ws(p);
    if (*p != ':') return NULL;
    p = skip_ws(p + 1);

    char *out = malloc(CONVO_CAP);
    if (!out) return NULL;
    if (want_open) {
        if (!json_slice_value(p, out, CONVO_CAP)) { free(out); return NULL; }
    } else {
        size_t o = 0;
        while (*p && *p != ',' && *p != '}' && o < CONVO_CAP - 1) out[o++] = *p++;
        out[o] = '\0';
        trim_in_place(out);
    }
    return out;
}

/* Unescape a JSON string body (no surrounding quotes) into out.
 * Handles every escape form a provider can emit, including \uXXXX with
 * surrogate pairs, so a reply containing an emoji survives intact. */
static void json_unescape(const char *src, char *out, size_t out_sz) {
    size_t o = 0;
    while (*src && o < out_sz - 1) {
        if (*src != '\\') { out[o++] = *src++; continue; }
        src++;
        switch (*src) {
            case 'n': out[o++] = '\n'; src++; break;
            case 't': out[o++] = '\t'; src++; break;
            case 'r': out[o++] = '\r'; src++; break;
            case 'b': out[o++] = '\b'; src++; break;
            case 'f': out[o++] = '\f'; src++; break;
            case 'u': {
                unsigned cp = 0;
                if (sscanf(src + 1, "%4x", &cp) != 1) { out[o++] = ' '; src++; break; }
                src += 5;
                if (cp >= 0xD800 && cp <= 0xDBFF && src[0] == '\\' && src[1] == 'u') {
                    unsigned lo = 0;
                    if (sscanf(src + 2, "%4x", &lo) == 1 && lo >= 0xDC00 && lo <= 0xDFFF) {
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        src += 6;
                    }
                }
                if (cp < 0x80) out[o++] = (char)cp;
                else if (cp < 0x800) {
                    out[o++] = (char)(0xC0 | (cp >> 6));
                    out[o++] = (char)(0x80 | (cp & 0x3F));
                } else if (cp < 0x10000) {
                    out[o++] = (char)(0xE0 | (cp >> 12));
                    out[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                    out[o++] = (char)(0x80 | (cp & 0x3F));
                } else {
                    out[o++] = (char)(0xF0 | (cp >> 18));
                    out[o++] = (char)(0x80 | ((cp >> 12) & 0x3F));
                    out[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                    out[o++] = (char)(0x80 | (cp & 0x3F));
                }
                break;
            }
            case '\0': out[o++] = '\\'; break;   /* trailing lone backslash */
            default: out[o++] = *src++; break;
        }
    }
    out[o] = '\0';
}

/* choices[0].message.content as a malloc'd string, or NULL.
 *
 * NULL is a NORMAL outcome, not a bug: a model replying with tool_calls
 * sends content: null. The first version of this file treated that as a
 * transport failure and the user saw "no reply from model" while the model
 * was in fact asking for a tool. Callers must check tool_calls first. */
static char *take_reply(const char *json) {
    char *v = json_object_field(json, "content", 0);
    if (!v) return NULL;
    trim_in_place(v);
    if (!v[0] || strcmp(v, "null") == 0) { free(v); return NULL; }

    /* Work on a mutable copy: stripping the closing quote means writing,
     * and body must not point into the const string literal path. */
    char *scratch = strdup(v);
    free(v);
    if (!scratch) return NULL;

    char *body = scratch;
    if (body[0] == '"') {
        body++;
        size_t n = strlen(body);
        if (n && body[n - 1] == '"') body[n - 1] = '\0';
    }

    char *out = malloc(REPLY_CAP);
    if (!out) { free(scratch); return NULL; }
    json_unescape(body, out, REPLY_CAP);
    free(scratch);
    trim_in_place(out);
    if (!out[0]) { free(out); return NULL; }
    return out;
}

/* Load one provider's bearer token: env override first, then the key file
 * in the entity state dir, then the same file under the project root.
 * Returns NULL when no key is configured - not an error, a provider to skip.
 *
 * The skip is ANNOUNCED on stderr. It was silent at first, and that hid a
 * real problem: key files live in the main checkout while this project runs
 * from a git worktree, which has its own &.widgits tree, so a key in one was
 * invisible in the other and every turn silently fell through to the next
 * provider. A provider that is configured-but-absent must be visible. */
static char *load_key(const Provider *p) {
    const char *direct = getenv(p->key_env);
    if (direct && direct[0]) return strdup(direct);

    const char *dir = entity_state_dir();
    if (dir && dir[0]) {
        char *full = NULL;
        if (asprintf(&full, "%s/%s", dir, p->key_file) >= 0 && full) {
            char *k = read_file(full);
            free(full);
            if (k) { trim_in_place(k); if (k[0]) return k; free(k); }
        }
    }
    char *full = NULL;
    if (asprintf(&full, "%s/&.widgits/open-hai/state/%s", project_root, p->key_file) >= 0 && full) {
        char *k = read_file(full);
        free(full);
        if (k) { trim_in_place(k); if (k[0]) return k; free(k); }
    }

    fprintf(stderr, "horn_chat_backend: %s has no key (%s / <state>/%s) - skipping\n",
            p->name, p->key_env, p->key_file);
    return NULL;
}

/* Classify a raw response body that carried no content.
 *
 * Quota exhaustion is per-ACCOUNT, not per-model, so it must not walk the
 * model ladder - every rung shares the bucket. OpenRouter signals it with a
 * 429 naming openrouter_free_tier_daily; curl runs without -i so the status
 * line is unavailable and the body is what we match on. */
static int is_quota_exhausted(const char *raw) {
    return raw && (strstr(raw, "free-models-per-day") != NULL ||
                   strstr(raw, "openrouter_free_tier_daily") != NULL ||
                   strstr(raw, "rate_limit_exceeded") != NULL ||
                   strstr(raw, "insufficient_quota") != NULL ||
                   strstr(raw, "insufficient_user_quota") != NULL ||
                   strstr(raw, "Too Many Requests") != NULL);
}

/* ── tools manifest ─────────────────────────────────────────────────── */

/* The OpenAI `tools` array, spliced verbatim out of
 * tools/horn_tools.json. NULL when tools are off or the manifest is
 * missing - not an error, chat without tools still works. */
static char *load_tools_array(void) {
    const char *off = getenv("HORN_TOOLS");
    if (off && strcmp(off, "off") == 0) return NULL;

    char *path = path_in("tools/horn_tools.json");
    if (!path) return NULL;
    char *buf = read_file(path);
    free(path);
    if (!buf) {
        fprintf(stderr, "horn_chat_backend: tools/horn_tools.json unreadable, running without tools\n");
        return NULL;
    }
    char *tools_json = NULL;
    const char *tkey = strstr(buf, "\"tools\"");
    if (tkey) {
        const char *p = skip_ws(tkey + strlen("\"tools\""));
        if (*p == ':') {
            p = skip_ws(p + 1);
            tools_json = malloc(strlen(p) + 2);
            if (tools_json && json_slice_value(p, tools_json, strlen(p) + 2)) {
                if (!tools_json[0]) { free(tools_json); tools_json = NULL; }
            } else { free(tools_json); tools_json = NULL; }
        }
    }
    free(buf);
    return tools_json;
}

/* ── conversation state ─────────────────────────────────────────────── */

/* Append a message object to pieces/horn/convo.json, creating the file if
 * needed. The array is rewritten textually rather than re-serialized: the
 * assistant message is copied verbatim out of the provider response, so
 * every escape survives untouched. */
static int convo_append(const char *msg_obj) {
    char *path = path_in("pieces/horn/convo.json");
    if (!path) return 0;
    char *cur = read_file(path);
    if (!cur) cur = strdup("[]");

    char *last = strrchr(cur, ']');
    if (!last) { free(cur); free(path); return 0; }

    int empty = 1;
    for (char *q = cur; q < last; q++) {
        if (*q != ' ' && *q != '\n' && *q != '\r' && *q != '\t' && *q != '[') { empty = 0; break; }
    }

    size_t need = strlen(cur) + strlen(msg_obj) + 8;
    char *out = malloc(need);
    if (!out) { free(cur); free(path); return 0; }
    *last = '\0';
    snprintf(out, need, "%s%s%s]", cur, empty ? "" : ",", msg_obj);

    FILE *f = fopen(path, "wb");
    int ok = 0;
    if (f) { fputs(out, f); fclose(f); ok = 1; }
    free(out);
    free(cur);
    free(path);
    return ok;
}


/* Rebuild the assistant message as a REQUEST-SAFE object.
 *
 * The obvious implementation - copy the provider's message verbatim - is
 * wrong, and it broke Groq outright. gpt-oss returns
 *   {"role":"assistant","content":"...","reasoning_content":"...","tool_calls":[...]}
 * and echoing that back verbatim puts `reasoning_content` into the NEXT
 * request, where Groq's own schema rejects it:
 *   'messages.17' : property 'reasoning_content' is unsupported
 * Since the tool loop replays the conversation every round, that killed
 * every turn after the first on the provider that leads the ladder - and
 * it surfaced as the confusing "no-content-in-response", because the
 * error body has no `content` field for extract_reply to find.
 *
 * Keep only the fields the OpenAI chat schema defines for an assistant
 * turn. Anything provider-specific is a display artefact, not state. */
static char *sanitise_assistant(const char *msg) {
    char *out = malloc(CONVO_CAP);
    if (!out) return NULL;
    size_t o = 0;

    o += (size_t)snprintf(out + o, CONVO_CAP - o, "{\"role\":\"assistant\"");

    /* content may legitimately be null (a pure tool call). */
    char *content = json_object_field(msg, "content", 0);
    if (content && strcmp(content, "null") != 0 && content[0]) {
        char *c = NULL;
        if (asprintf(&c, "%s", content) >= 0 && c) {
            o += (size_t)snprintf(out + o, CONVO_CAP - o, ",\"content\":%s", c);
            free(c);
        }
    } else {
        o += (size_t)snprintf(out + o, CONVO_CAP - o, ",\"content\":null");
    }
    free(content);

    /* tool_calls copied verbatim - it is the one part that MUST survive. */
    const char *tc = strstr(msg, "\"tool_calls\"");
    if (tc) {
        const char *q = skip_ws(tc + strlen("\"tool_calls\""));
        if (*q == ':') {
            q = skip_ws(q + 1);
            char *arr = malloc(CONVO_CAP);
            if (arr && json_slice_value(q, arr, CONVO_CAP) && arr[0] == '[')
                o += (size_t)snprintf(out + o, CONVO_CAP - o, ",\"tool_calls\":%s", arr);
            free(arr);
        }
    }
    snprintf(out + o, CONVO_CAP - o, "}");
    return out;
}

/* POST one request against one provider+model.
 *
 * The request body is the seeded convo.json plus the tools array, so the
 * whole file goes to disk and reaches curl via --data-binary @file rather
 * than being interpolated into the command line: prompts are arbitrary user
 * text and putting them in argv exposes them in the process table to every
 * other user on the box.
 *
 * Returns 0 and fills *reply_out on a final text answer; returns 10 and
 * writes tool_calls.json when the assistant wants tools; returns -1 on a
 * failure with *quota set and err describing why. */
static int post_chat(const Provider *p, const char *model, const char *api_key,
                     char *err, size_t err_sz, int *quota,
                     char **reply_out, char **tools_out) {
    *quota = 0;
    *reply_out = NULL;
    *tools_out = NULL;

    char *convo = path_in("pieces/horn/convo.json");
    char *payload_path = path_in("pieces/horn/.req_payload.json");
    char *raw_path = path_in("pieces/horn/.req_raw.json");
    char *tools_json = load_tools_array();

    if (!convo || !payload_path || !raw_path) {
        snprintf(err, err_sz, "path-alloc-failed");
        goto fail;
    }

    char *convo_buf = read_file(convo);
    if (!convo_buf) convo_buf = strdup("[]");
    /* strip the outer brackets to get the message list verbatim */
    char *msgs = malloc(CONVO_CAP);
    if (!msgs || !json_slice_value(convo_buf, msgs, CONVO_CAP)) {
        snprintf(err, err_sz, "convo.json-malformed");
        free(convo_buf); free(msgs);
        goto fail;
    }
    free(convo_buf);

    const char *choice = getenv("HORN_TOOL_CHOICE");
    const char *tc = (choice && strcmp(choice, "required") == 0)
                     ? ",\"tool_choice\":\"required\"" : "";

    snprintf(err, err_sz, "payload-write-failed");
    FILE *pf = fopen(payload_path, "wb");
    if (!pf) { free(msgs); goto fail; }
    fprintf(pf, "{\"model\":\"%s\",\"messages\":%s", model, msgs);
    if (tools_json) fprintf(pf, ",\"tools\":%s", tools_json);
    fputs(tc, pf);
    if (p->no_thinking) fputs(",\"chat_template_kwargs\":{\"enable_thinking\":false}", pf);
    fputs("}", pf);
    fclose(pf);
    free(msgs);

    /* Paths are single-quoted: this house's tree contains literal
     * '&.widgits' and '^.hai-horn' segments, and an unescaped '&' inside a
     * /bin/sh word backgrounds and truncates the command. Same trap
     * prisc+x's OP_EXEC and exec_custom_op() carry a comment about.
     *
     * asprintf, not char[N]: api_key is heap-allocated with a length gcc
     * cannot bound, so a fixed buffer is either a real truncation risk
     * (silently sending a mangled bearer token) or a format-truncation
     * warning. */
    char *cmd = NULL;
    if (asprintf(&cmd,
        "curl -s -m 60 -X POST '%s'"
        " -H 'Content-Type: application/json'"
        " -H 'Authorization: Bearer %s'"
        " --data-binary @'%s' > '%s' 2>&1",
        p->endpoint, api_key, payload_path, raw_path) < 0 || !cmd) {
        snprintf(err, err_sz, "cmd-alloc-failed");
        goto fail;
    }
    snprintf(err, err_sz, "curl-failed");
    int rc = system(cmd);
    free(cmd);
    (void)rc;

    char *raw = read_file(raw_path);
    if (!raw) { snprintf(err, err_sz, "no-response-file"); goto fail; }
    if (strlen(raw) > RAW_CAP) { free(raw); snprintf(err, err_sz, "response-too-large"); goto fail; }

    if (is_quota_exhausted(raw)) {
        free(raw);
        snprintf(err, err_sz, "rate-or-quota-limited");
        *quota = 1;
        goto fail;
    }

    /* assistant message, verbatim - append it to the conversation whether
     * it is text or tool_calls, because both are part of the history the
     * next request must replay. */
    char *msg = NULL;
    const char *mkey = strstr(raw, "\"message\"");
    if (mkey) {
        const char *q = skip_ws(mkey + strlen("\"message\""));
        if (*q == ':') {
            q = skip_ws(q + 1);
            msg = malloc(CONVO_CAP);
            if (msg && !json_slice_value(q, msg, CONVO_CAP)) { free(msg); msg = NULL; }
        }
    }

    char *tool_calls = NULL;
    if (msg) {
        const char *tk = strstr(msg, "\"tool_calls\"");
        if (tk) {
            const char *q = skip_ws(tk + strlen("\"tool_calls\""));
            if (*q == ':') {
                q = skip_ws(q + 1);
                if (*q == '[') {
                    tool_calls = malloc(CONVO_CAP);
                    if (tool_calls && !json_slice_value(q, tool_calls, CONVO_CAP)) {
                        free(tool_calls); tool_calls = NULL;
                    }
                    if (tool_calls && !tool_calls[0]) { free(tool_calls); tool_calls = NULL; }
                }
            }
        }
    }

    char *reply = take_reply(raw);

    if (msg) {
        char *safe = sanitise_assistant(msg);
        convo_append(safe ? safe : msg);
        free(safe);
    }

    if (tool_calls && tool_calls[0] == '[') {
        char *tp = path_in("pieces/horn/tool_calls.json");
        if (tp) {
            FILE *tf = fopen(tp, "wb");
            if (tf) { fputs(tool_calls, tf); fclose(tf); }
            free(tp);
        }
        /* Hand the calls to the caller. NOT freeing them here was the bug:
         * post_chat returned 10 with *tools_out still NULL, so main saw
         * "no reply and no tools", printed "groq unavailable", and walked
         * the entire ladder - the tool call was captured correctly and then
         * thrown away by the code that was supposed to act on it. */
        *tools_out = tool_calls;
        free(msg);
        free(reply);
        free(raw);
        free(convo); free(payload_path); free(raw_path); free(tools_json);
        return 10;
    }

    if (!reply) snprintf(err, err_sz, "no-content-in-response");
    *reply_out = reply;
    free(msg);
    free(raw);
    free(convo); free(payload_path); free(raw_path); free(tools_json);
    return reply ? 0 : -1;

fail:
    free(convo); free(payload_path); free(raw_path); free(tools_json);
    return -1;
}

int main(int argc, char *argv[]) {
    resolve_root();

    char *prompt = NULL;
    if (argc > 1 && argv[1][0]) {
        prompt = strdup(argv[1]);
    } else {
        const char *env = getenv("HORN_PROMPT");
        if (env && env[0]) prompt = strdup(env);
    }
    if (!prompt || !prompt[0]) {
        fprintf(stderr, "horn_chat_backend: empty prompt\n");
        return 1;
    }

    /* Seed the conversation ONLY if horn_turn has not already done it.
     *
     * This was the worst bug in the tool loop: the transport re-seeded
     * convo.json on every invocation, so each call within ONE user turn
     * threw away the accumulated tool results. The model therefore saw
     * system + user + a fresh tool call every round, had no memory of what
     * it had already been told, and re-issued the same call until the round
     * cap - which looked like a hang and burned six live requests.
     *
     * horn_turn owns conversation policy and seeds the file; the transport
     * only seeds when running standalone (no convo.json present), which is
     * the `horn_chat.sh send` and test path. */
    /* Explicit modes: a prompt argument means "start fresh", no argument
     * means "continue whatever horn_turn seeded". The old rule - seed only
     * when convo.json was ABSENT - meant a standalone call silently
     * inherited the previous turn's whole conversation, provider-specific
     * assistant fields included. */
    if (prompt) {
        char *p = path_in("pieces/horn/convo.json");
        if (!p) goto seeded;

        FILE *f = fopen(p, "wb");
        if (f) {
            /* The system prompt has no quotes or newlines; the user turn
             * does, hence the loop. */
            fprintf(f, "[{\"role\":\"system\",\"content\":\"%s\"},"
                       "{\"role\":\"user\",\"content\":\"", SYS_PROMPT);
            for (const char *q = prompt; *q; q++) {
                switch (*q) {
                    case '"':  fputs("\\\"", f); break;
                    case '\\': fputs("\\\\", f); break;
                    case '\n': fputs("\\n", f);  break;
                    case '\r': fputs("\\r", f);  break;
                    case '\t': fputs("\\t", f);  break;
                    default:   fputc(*q, f);     break;
                }
            }
            fputs("\"}]", f);
            fclose(f);
        }
        free(p);
    }
seeded:;
    char *reply = NULL, *tool_calls = NULL;
    char err[128] = "";
    int any_quota = 0, any_key = 0;
    const char *used = NULL;
    char used_model[160] = "";

    for (int i = 0; i < N_PROVIDERS && !reply && !tool_calls; i++) {
        const Provider *p = &PROVIDERS[i];

        char *key = load_key(p);
        if (!key) continue;
        any_key = 1;

        for (int m = 0; p->models[m] && !reply && !tool_calls; m++) {
            int quota = 0, rc;
            rc = post_chat(p, p->models[m], key, err, sizeof(err), &quota, &reply, &tool_calls);
            if (rc == 10) { used = p->name; snprintf(used_model, sizeof(used_model), "%s", p->models[m]); break; }
            if (quota) { any_quota = 1; break; }
            if (reply) { used = p->name; snprintf(used_model, sizeof(used_model), "%s", p->models[m]); }
        }
        free(key);
        if (!reply && !tool_calls)
            fprintf(stderr, "horn_chat_backend: %s unavailable (%s), trying next provider\n",
                    p->name, err[0] ? err : "unknown");
    }

    free(prompt);

    if (tool_calls) {
        fprintf(stderr, "horn_chat_backend: %s wants to call tools\n", used ? used : "?");
        return 10;
    }

    if (!reply) {
        if (!any_key) {
            fprintf(stderr,
                "horn_chat_backend: no API key configured for any provider. "
                "(looked for raw_groq.txt, raw_poolside.txt, "
                "openrouter_api_key.txt under &.widgits/open-hai/state/)\n");
            return 1;
        }
        if (any_quota) {
            fprintf(stderr,
                "horn_chat_backend: every provider was rate/quota limited. "
                "For OpenRouter that is the daily free tier; it resets at "
                "the UTC day boundary. Not a harness fault.\n");
            return 3;
        }
        fprintf(stderr, "horn_chat_backend: all providers failed (%s)\n", err);
        return 2;
    }

    /* Record WHICH rung actually answered, not just the provider.
     *
     * The provider ladder exists so a rate-limited or retired free slug does
     * not break a turn, which means the model that replies is frequently NOT
     * the one the UI advertises. For an IRL harness that is the whole
     * question: a comparison between "HORN" and "HALO" means nothing unless
     * both turns record the model that produced them. Written here rather
     * than parsed back out of stderr, so it cannot drift. */
    if (used) {
        char *mp = path_in("pieces/horn/last_model.txt");
        if (mp) {
            FILE *mf = fopen(mp, "wb");
            if (mf) { fprintf(mf, "%s\t%s\n", used, used_model); fclose(mf); }
            free(mp);
        }
        fprintf(stderr, "horn_chat_backend: answered by %s (%s)\n", used, used_model);
    }

    const char *reply_path = getenv("HORN_REPLY_FILE");
    char *default_reply = NULL;
    if (!reply_path || !reply_path[0]) {
        default_reply = path_in("pieces/horn/last_reply.txt");
        reply_path = default_reply;
    }
    if (reply_path) {
        FILE *rf = fopen(reply_path, "wb");
        if (rf) { fputs(reply, rf); fclose(rf); }
        free(default_reply);
    }

    printf("%s\n", reply);
    free(reply);
    return 0;
}