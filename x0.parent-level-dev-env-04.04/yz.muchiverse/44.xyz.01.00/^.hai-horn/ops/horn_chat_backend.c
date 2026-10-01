/* horn_chat_backend - HORN_CHAT's LLM transport.
 *
 * One file, several providers. OpenRouter and Poolside both expose an
 * OpenAI-compatible POST /chat/completions with a Bearer token and a
 * choices[0].message.content reply, so they share one request path, one
 * reply parser and one payload writer. What differs is the endpoint, the
 * key file, the model ladder, and whether thinking is on by default - so
 * those are a table below rather than a second 300-line copy of this.
 *
 * PROVIDER ORDER IS FALLBACK ORDER. Poolside comes first because its key
 * is independent of OpenRouter's daily free-tier quota (50 requests/day
 * per key, exhausted 2026-10-01 by testing), which is exactly the
 * failure mode a second provider is worth having for. Within a provider,
 * the ladder walks models until one answers.
 *
 * Adapted from entity-cli's proven ai_chat_openrouter.c (2026-09-30).
 * Changes from that original, all deliberate:
 *   - the provider table above replaces its hardcoded OpenRouter endpoint
 *   - entity_state_dir comes from $HORN_ENTITY_DIR (set by horn_chat.sh),
 *     not a hardcoded "entity" subdir of the project root
 *   - the prompt comes from argv[1] (or $HORN_PROMPT) so the pal loop can
 *     drive it without mutating the orchestrator's environment
 *   - the reply lands in $HORN_REPLY_FILE (default
 *     pieces/horn/last_reply.txt) as well as stdout
 *
 * EXIT CODES - callers depend on the distinction:
 *   0  a reply was produced
 *   1  bad usage, or no API key at all
 *   2  every provider/model was reachable but none answered
 *   3  every failure was a quota/rate limit (an account state, not a code
 *      fault - retrying cannot fix it until the window resets)
 *
 * Usage: horn_chat_backend.+x "<prompt>"
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
/* Room for MAX_PATH worth of project_root plus the longest relative
 * suffix this file appends, so gcc can prove snprintf can't truncate. */
#define PATH_BUF (MAX_PATH + 256)
#define REPLY_CAP 65536
#define RAW_CAP   (4 * 1024 * 1024)

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

/* Verified live 2026-10-01. */
static const Provider PROVIDERS[] = {
    { "poolside",
      "https://inference.poolside.ai/v1/chat/completions",
      "HORN_POOLSIDE_KEY", "raw_poolside.txt",
      { "poolside/laguna-s-2.1", "poolside/laguna-xs-2.1", NULL }, 1 },

    { "openrouter",
      "https://openrouter.ai/api/v1/chat/completions",
      "HORN_API_KEY", "openrouter_api_key.txt",
      { "nvidia/nemotron-3-ultra-550b-a55b:free",
        "nvidia/nemotron-3-super-120b-a12b:free",
        "inclusionai/ling-3.0-flash-sante:free", NULL }, 0 },
};
#define N_PROVIDERS ((int)(sizeof(PROVIDERS) / sizeof(PROVIDERS[0])))

/* Model names go stale fast. A slug retired from a free tier does not
 * fail loudly: OpenRouter answers HTTP 200 with an error BODY, which this
 * file reports as "no content". Refresh with
 *   curl -s https://openrouter.ai/api/v1/models
 * and with
 *   curl -s https://inference.poolside.ai/v1/models -H "Authorization: Bearer $KEY"
 */
static const char *SYS_PROMPT =
    "You are HORN, a terminal-based assistant. Answer clearly and concisely. "
    "Plain text only, no markdown fences.";

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

/* Load one provider's bearer token: env override first, then the key file
 * in the entity state dir, then the same file under the project root.
 * Returns NULL when no key is configured for that provider - which is not
 * an error, just a provider to skip.
 *
 * The skip is ANNOUNCED on stderr. It was silent at first, and that hid a
 * real problem: the key file lives in the main checkout while this project
 * runs from a git worktree, which has its own &.widgits tree, so Poolside
 * was skipped without a word and every turn silently fell through to
 * OpenRouter. A provider that is configured-but-absent must be visible. */
static char *load_key(const Provider *p) {
    const char *direct = getenv(p->key_env);
    if (direct && direct[0]) return strdup(direct);

    const char *dir = entity_state_dir();
    if (dir && dir[0]) {
        char *path = NULL;
        if (asprintf(&path, "%s/%s", dir, p->key_file) >= 0 && path) {
            char *k = read_file(path);
            free(path);
            if (k) { trim_in_place(k); if (k[0]) return k; free(k); }
        }
    }

    char *path = NULL;
    if (asprintf(&path, "%s/&.widgits/open-hai/state/%s", project_root, p->key_file) >= 0 && path) {
        char *k = read_file(path);
        free(path);
        if (k) { trim_in_place(k); if (k[0]) return k; free(k); }
    }

    fprintf(stderr, "horn_chat_backend: %s has no key (%s / <state>/%s) - skipping\n",
            p->name, p->key_env, p->key_file);
    return NULL;
}

/* Extract the first choices[0].message.content string value.
 *
 * Hand-rolled rather than linked against entity-cli's json_parser
 * because that binary is not part of this project's op set and
 * hard-linking a sibling project's build artifact in would make
 * HORN_CHAT unbuildable on its own. Both providers return the standard
 * envelope for the fields read here, and every escape form is handled
 * explicitly rather than assumed absent. */
static char *extract_reply(const char *json) {
    const char *anchor = strstr(json, "\"content\":");
    if (!anchor) return NULL;
    anchor += strlen("\"content\":");

    while (*anchor == ' ' || *anchor == '\t') anchor++;
    if (*anchor != '"') return NULL;
    anchor++;

    char *out = malloc(REPLY_CAP);
    if (!out) return NULL;
    size_t o = 0;

    while (*anchor && o < REPLY_CAP - 1) {
        if (*anchor != '\\') {
            if (*anchor == '"') break;
            out[o++] = *anchor++;
            continue;
        }
        anchor++;
        switch (*anchor) {
            case 'n':  out[o++] = '\n'; anchor++; break;
            case 't':  out[o++] = '\t'; anchor++; break;
            case 'r':  out[o++] = '\r'; anchor++; break;
            case 'b':  out[o++] = '\b'; anchor++; break;
            case 'f':  out[o++] = '\f'; anchor++; break;
            case '"':  out[o++] = '"';  anchor++; break;
            case '\\': out[o++] = '\\'; anchor++; break;
            case '/':  out[o++] = '/';  anchor++; break;
            case 'u': {
                unsigned cp = 0;
                if (sscanf(anchor + 1, "%4x", &cp) != 1) { anchor++; break; }
                anchor += 5;
                if (cp >= 0xD800 && cp <= 0xDBFF && anchor[0] == '\\' && anchor[1] == 'u') {
                    unsigned lo = 0;
                    if (sscanf(anchor + 2, "%4x", &lo) == 1 && lo >= 0xDC00 && lo <= 0xDFFF) {
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        anchor += 6;
                    }
                }
                if (cp < 0x80) {
                    out[o++] = (char)cp;
                } else if (cp < 0x800) {
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
            default: out[o++] = *anchor ? *anchor : ' '; if (*anchor) anchor++; break;
        }
    }
    out[o] = '\0';
    trim_in_place(out);
    if (o == 0) { free(out); return NULL; }
    return out;
}

static char *json_escape(const char *s) {
    size_t n = strlen(s);
    char *out = malloc(n * 6 + 16);
    if (!out) return NULL;
    size_t o = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
            case '"':  out[o++] = '\\'; out[o++] = '"';  break;
            case '\\': out[o++] = '\\'; out[o++] = '\\'; break;
            case '\n': out[o++] = '\\'; out[o++] = 'n';  break;
            case '\r': out[o++] = '\\'; out[o++] = 'r';  break;
            case '\t': out[o++] = '\\'; out[o++] = 't';  break;
            default:
                if (c < 0x20) { o += (size_t)sprintf(out + o, "\\u%04x", c); }
                else out[o++] = (char)c;
        }
    }
    out[o] = '\0';
    return out;
}

/* Classify a raw response body that carried no content.
 *
 * Quota exhaustion is per-ACCOUNT, not per-model, so it must not walk the
 * model ladder - every rung shares the same bucket - and it is not a code
 * fault. OpenRouter signals it with a 429 naming openrouter_free_tier_daily;
 * curl runs without -i so the status line is unavailable and the body is
 * what we match on. */
static int is_quota_exhausted(const char *raw) {
    return raw && (strstr(raw, "free-models-per-day") != NULL ||
                   strstr(raw, "openrouter_free_tier_daily") != NULL ||
                   strstr(raw, "rate_limit_exceeded") != NULL ||
                   strstr(raw, "insufficient_quota") != NULL ||
                   strstr(raw, "Too Many Requests") != NULL);
}

/* POST one request against one provider+model. Returns the reply, or NULL
 * with *quota set to 1 when the failure was a rate/quota limit (so the
 * caller can stop walking that provider) and err describing why.
 *
 * The prompt goes to disk and reaches curl via --data-binary @file rather
 * than being interpolated into the command line: it is arbitrary user
 * text, and putting it in argv exposes it in the process table to every
 * other user on the box. */
static char *post_chat(const Provider *p, const char *model, const char *api_key,
                       const char *prompt, char *err, size_t err_sz, int *quota) {
    *quota = 0;

    char *payload_path = NULL;
    if (asprintf(&payload_path, "%s/pieces/horn/.req_payload.json", project_root) < 0 || !payload_path) {
        snprintf(err, err_sz, "payload-path-alloc-failed");
        return NULL;
    }
    char *raw_path = NULL;
    if (asprintf(&raw_path, "%s/pieces/horn/.req_raw.json", project_root) < 0 || !raw_path) {
        free(payload_path);
        snprintf(err, err_sz, "raw-path-alloc-failed");
        return NULL;
    }

    snprintf(err, err_sz, "payload-write-failed");
    char *sys_e = json_escape(SYS_PROMPT);
    char *usr_e = json_escape(prompt);
    if (!sys_e || !usr_e) { free(sys_e); free(usr_e); free(payload_path); free(raw_path); return NULL; }

    FILE *pf = fopen(payload_path, "wb");
    if (!pf) {
        free(sys_e); free(usr_e); free(payload_path); free(raw_path);
        return NULL;
    }
    fprintf(pf,
        "{\"model\":\"%s\",\"messages\":[{\"role\":\"system\",\"content\":\"%s\"},"
        "{\"role\":\"user\",\"content\":\"%s\"}]%s}",
        model, sys_e, usr_e,
        p->no_thinking ? ",\"chat_template_kwargs\":{\"enable_thinking\":false}" : "");
    fclose(pf);
    free(sys_e);
    free(usr_e);

    /* Paths are single-quoted: this house's own tree contains literal
     * '&.widgits' and '^.hai-horn' segments, and an unescaped '&' inside
     * a /bin/sh word backgrounds and truncates the command. Same trap
     * prisc+x's own OP_EXEC and exec_custom_op() carry a comment about.
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
        free(payload_path); free(raw_path);
        return NULL;
    }

    snprintf(err, err_sz, "curl-failed");
    int rc = system(cmd);
    free(cmd);
    (void)rc;

    char *raw = read_file(raw_path);
    free(payload_path);
    free(raw_path);
    if (!raw) { snprintf(err, err_sz, "no-response-file"); return NULL; }
    if (strlen(raw) > RAW_CAP) { free(raw); snprintf(err, err_sz, "response-too-large"); return NULL; }

    if (is_quota_exhausted(raw)) {
        free(raw);
        snprintf(err, err_sz, "rate-or-quota-limited");
        *quota = 1;
        return NULL;
    }

    char *reply = extract_reply(raw);
    if (!reply) snprintf(err, err_sz, "no-content-in-response");
    free(raw);
    return reply;
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

    char *reply = NULL;
    char err[128] = "";
    int any_quota = 0, any_key = 0;
    const char *used = NULL;

    for (int i = 0; i < N_PROVIDERS && !reply; i++) {
        const Provider *p = &PROVIDERS[i];

        char *key = load_key(p);
        if (!key) {
            /* A provider without a key is simply not configured. Not an
             * error: the whole point of a chain is that any subset works. */
            continue;
        }
        any_key = 1;

        for (int m = 0; p->models[m] && !reply; m++) {
            int quota = 0;
            reply = post_chat(p, p->models[m], key, prompt, err, sizeof(err), &quota);
            if (quota) {
                /* Account-wide: every model in this provider shares the
                 * bucket, so stop walking it and move to the next
                 * provider rather than burning the ladder. */
                any_quota = 1;
                break;
            }
            if (reply) used = p->name;
        }
        free(key);

        if (!reply) {
            fprintf(stderr, "horn_chat_backend: %s unavailable (%s), trying next provider\n",
                    p->name, err[0] ? err : "unknown");
        }
    }

    free(prompt);

    if (!reply) {
        if (!any_key) {
            fprintf(stderr,
                "horn_chat_backend: no API key configured for any provider "
                "(looked for %s)\n",
                "&.widgits/open-hai/state/{raw_poolside.txt,openrouter_api_key.txt}");
            return 1;
        }
        if (any_quota) {
            fprintf(stderr,
                "horn_chat_backend: every provider was rate/quota limited. "
                "For OpenRouter that is the 50/day free tier; it resets at "
                "the UTC day boundary. Not a harness fault.\n");
            return 3;
        }
        fprintf(stderr, "horn_chat_backend: all providers failed (%s)\n", err);
        return 2;
    }

    if (used) fprintf(stderr, "horn_chat_backend: answered by %s\n", used);

    const char *reply_path = getenv("HORN_REPLY_FILE");
    char *default_reply = NULL;
    if (!reply_path || !reply_path[0]) {
        if (asprintf(&default_reply, "%s/pieces/horn/last_reply.txt", project_root) < 0)
            default_reply = NULL;
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