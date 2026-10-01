/* horn_chat_openrouter - HORN_CHAT's OpenRouter transport.
 *
 * Pure transport: read the API key, POST one user message, write the
 * model's reply to a file, exit. It does NOT touch chat history, the
 * chtpm layout, or any view/state file - that is horn_turn's job
 * (ops/horn_turn.c). Keeping the two apart is what lets HALO_CHAT
 * later reuse this exact file while interposing Concept Bank
 * validation between turn and publish, without touching transport.
 *
 * Adapted from entity-cli's proven ai_chat_openrouter.c (2026-09-30).
 * Changes from that original, all deliberate:
 *   - entity_state_dir comes from $HORN_ENTITY_DIR (set by horn_chat.sh),
 *     not a hardcoded "entity" subdir of the project root.
 *   - the prompt comes from argv[1] (or $HORN_PROMPT) instead of
 *     getenv("ENTITY_CHAT_PROMPT"), so the pal loop can drive it without
 *     mutating the orchestrator's environment.
 *   - the reply lands in $HORN_REPLY_FILE (default
 *     pieces/horn/last_reply.txt) as well as stdout.
 *   - the key file fallback chain is unchanged: HORN_API_KEY wins, then
 *     <entity_state_dir>/openrouter_api_key.txt, then the house-wide
 *     &.widgits/open-hai/state/openrouter_api_key.txt.
 *
 * Model list is the same free-tier fallback ladder the entity-cli
 * version proved works. The first entry that returns a non-empty
 * reply wins; only if ALL of them fail does this exit non-zero.
 *
 * Usage: horn_chat_openrouter.+x "<prompt>"
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

/* Free-tier fallback ladder: try each in turn until one returns a
 * non-empty reply.
 *
 * Verified against the live API 2026-10-01, and the shape matters:
 * a slug that has moved behind a paywall answers HTTP 200 with an
 * {"error":{"message":"...unavailable for free..."}} BODY, not an HTTP
 * error status. So a model being retired from the free tier does not
 * fail loudly - it returns a perfectly well-formed response containing
 * no content, which extract_reply() correctly reports as "no content".
 * That is why the ladder exists and why a dead rung is survivable.
 *
 * Two of the three slugs carried over from entity-cli's version are now
 * paid-only; the first rung is currently the only live free one, which
 * means a rate limit on it surfaces as a slow turn rather than an
 * instant failure. Add new free slugs here as they appear. */
static const char *OR_MODELS[] = {
    "nvidia/nemotron-3-ultra-550b-a55b:free",
    "nvidia/nemotron-3-super-120b-a12b:free",
    "inclusionai/ling-3.0-flash-sante:free",
    NULL
};

static const char *SYS_PROMPT =
    "You are HORN, a terminal-based assistant. Answer clearly and concisely. "
    "Plain text only, no markdown fences.";

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) {
        snprintf(project_root, sizeof(project_root), "%s", env);
        return;
    }
    if (getcwd(project_root, sizeof(project_root)) == NULL)
        snprintf(project_root, sizeof(project_root), ".");
}

static char *read_file(const char *path, long *out_len) {
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
    if (out_len) *out_len = (long)got;
    return buf;
}

static void trim_in_place(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' ||
                     s[n - 1] == ' '  || s[n - 1] == '\t'))
        s[--n] = '\0';
}

/* Load the bearer token. Same fallback order as entity-cli's original,
 * with the house-wide location appended as the last resort. */
static char *load_api_key(void) {
    const char *direct = getenv("HORN_API_KEY");
    if (direct && direct[0]) return strdup(direct);

    const char *entity_dir = getenv("HORN_ENTITY_DIR");
    char path[PATH_BUF];

    if (entity_dir && entity_dir[0]) {
        snprintf(path, sizeof(path), "%s/openrouter_api_key.txt", entity_dir);
        char *k = read_file(path, NULL);
        if (k) { trim_in_place(k); if (k[0]) return k; free(k); }
    }
    snprintf(path, sizeof(path), "%s/&.widgits/open-hai/state/openrouter_api_key.txt", project_root);
    char *k = read_file(path, NULL);
    if (k) { trim_in_place(k); if (k[0]) return k; free(k); }

    return NULL;
}

/* Extract the first choices[0].message.content string value.
 *
 * Hand-rolled rather than linked against entity-cli's json_parser
 * because that binary is not part of this project's op set and
 * hard-linking a sibling project's build artifact into ours would make
 * HORN_CHAT unbuildable on its own. OpenRouter's chat-completions
 * envelope is stable for the fields read here, and every escape form
 * below is handled explicitly rather than assumed absent. */
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

/* Minimal JSON string escaper for the prompt we embed. */
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

/* POST the payload once against one model. Returns the model's reply, or
 * NULL with err set to why it failed (so the caller can fall through to
 * the next model in the ladder rather than aborting the whole turn).
 *
 * The payload is written to disk and passed to curl via --data-binary @file
 * rather than interpolated into the command line: a chat prompt is
 * arbitrary user text, and putting it in argv means it is exposed in the
 * process table to every other user on the box. */
/* Distinguish "the free tier is out of quota today" from "the models
 * are broken". These need very different responses from a caller and from
 * a person reading the terminal, and lumping them together as "all models
 * failed" is how a run of e2e retries against an exhausted quota managed
 * to look like a code defect.
 *
 * OpenRouter signals it with HTTP 429 and a body naming
 * openrouter_free_tier_daily, but the request goes through curl without
 * -i, so the status line is not available here - match the body. */
static int is_quota_exhausted(const char *raw) {
    return raw && (strstr(raw, "free-models-per-day") != NULL ||
                   strstr(raw, "openrouter_free_tier_daily") != NULL);
}

static char *post_chat(const char *model, const char *api_key,
                       const char *prompt, char *err, size_t err_sz) {
    char payload_path[PATH_BUF], raw_path[PATH_BUF];
    snprintf(payload_path, sizeof(payload_path), "%s/pieces/horn/.or_payload.json", project_root);
    snprintf(raw_path, sizeof(raw_path), "%s/pieces/horn/.or_raw.json", project_root);

    snprintf(err, err_sz, "payload-write-failed");
    char *sys_e = json_escape(SYS_PROMPT);
    char *usr_e = json_escape(prompt);
    if (!sys_e || !usr_e) { free(sys_e); free(usr_e); return NULL; }

    FILE *pf = fopen(payload_path, "wb");
    if (!pf) { free(sys_e); free(usr_e); return NULL; }
    fprintf(pf,
        "{\"model\":\"%s\",\"messages\":[{\"role\":\"system\",\"content\":\"%s\"},"
        "{\"role\":\"user\",\"content\":\"%s\"}]}",
        model, sys_e, usr_e);
    fclose(pf);
    free(sys_e);
    free(usr_e);

    /* Paths are single-quoted: this house's own tree contains a literal
     * '&.widgits' segment, and an unescaped '&' inside a /bin/sh word
     * backgrounds and truncates the command. Same trap prisc+x's own
     * OP_EXEC and exec_custom_op() carry a comment about.
     *
     * asprintf, not a char[8192]: api_key is heap-allocated with a length
     * gcc cannot bound, so a fixed buffer is either a real truncation risk
     * (silently sending a mangled bearer token) or a format-truncation
     * warning. */
    char *cmd = NULL;
    if (asprintf(&cmd,
        "curl -s -m 60 -X POST https://openrouter.ai/api/v1/chat/completions"
        " -H 'Content-Type: application/json'"
        " -H 'Authorization: Bearer %s'"
        " --data-binary @'%s' > '%s' 2>&1",
        api_key, payload_path, raw_path) < 0 || !cmd) {
        snprintf(err, err_sz, "cmd-alloc-failed");
        return NULL;
    }

    snprintf(err, err_sz, "curl-failed");
    int rc = system(cmd);
    free(cmd);
    (void)rc;

    char *raw = read_file(raw_path, NULL);
    if (!raw) { snprintf(err, err_sz, "no-response-file"); return NULL; }
    if (strlen(raw) > RAW_CAP) { free(raw); snprintf(err, err_sz, "response-too-large"); return NULL; }

    /* Quota exhaustion is per-KEY, not per-model: every rung shares it, so
     * do not walk the ladder on this - just report and get out. */
    if (is_quota_exhausted(raw)) {
        free(raw);
        snprintf(err, err_sz, "daily-free-quota-exhausted");
        return NULL;
    }

    char *reply = extract_reply(raw);
    free(raw);
    if (!reply) snprintf(err, err_sz, "no-content-in-response");
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
        fprintf(stderr, "horn_chat_openrouter: empty prompt\n");
        return 1;
    }

    char *api_key = load_api_key();
    if (!api_key) {
        fprintf(stderr, "horn_chat_openrouter: no OpenRouter API key found\n");
        free(prompt);
        return 1;
    }

    /* Free-tier ladder: try each model in turn until one returns a
     * non-empty reply. Only if all of them fail does this exit non-zero,
     * and the last error is preserved so horn_turn can show the player
     * something more useful than "no reply". */
    char *reply = NULL;
    char err[128] = "";
    for (int i = 0; OR_MODELS[i] && !reply; i++)
        reply = post_chat(OR_MODELS[i], api_key, prompt, err, sizeof(err));

    free(api_key);

    if (!reply) {
        /* Exit 3 = the daily free-tier quota is gone. Distinct from exit 2
         * ("the models did not answer") because it is an account state,
         * not a code fault, and retrying the ladder cannot fix it. */
        if (strcmp(err, "daily-free-quota-exhausted") == 0) {
            fprintf(stderr,
                "horn_chat_openrouter: OpenRouter daily free-tier quota exhausted "
                "(50/day on this key). Resets at the UTC day boundary, or add "
                "credits. Not a harness fault - no model will answer until then.\n");
            free(prompt);
            return 3;
        }
        fprintf(stderr, "horn_chat_openrouter: all models failed (%s)\n", err);
        free(prompt);
        return 2;
    }

    const char *reply_path = getenv("HORN_REPLY_FILE");
    char default_reply[PATH_BUF];
    if (!reply_path || !reply_path[0]) {
        snprintf(default_reply, sizeof(default_reply), "%s/pieces/horn/last_reply.txt", project_root);
        reply_path = default_reply;
    }
    FILE *rf = fopen(reply_path, "wb");
    if (rf) { fputs(reply, rf); fclose(rf); }

    printf("%s\n", reply);

    free(reply);
    free(prompt);
    return 0;
}