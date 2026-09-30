/* ai_chat.c - MVP chat round trip for a drop-in entity (AI-PUSH-
 * ROADMAP-AND-NUANCES.md, "Track 1's Natural Test Surface: the Drop-In
 * Chatbot Entity"). Direct instruction 2026-09-28: "keep building out
 * and testing the ai till chat works to some extent using the
 * architecture."
 *
 * HONEST SCOPE, matching every other checkpoint in this house's AI
 * track: this is ONLY the free natural-language round trip (user text
 * in -> Gemma reply out -> appended to this entity's own real
 * chat_history.txt). It is NOT the instance-scoped Concept-Bank
 * personality mechanism, NOT the template/delta bank split, NOT the
 * /command-vs-free-text HARNECIENT-HACK dispatch layer - all three are
 * still real designs, not yet built (see the roadmap doc's own
 * "Design resolved this session (not yet built)" section). This proves
 * the plumbing those need to sit on top of: a real conversation an
 * entity can actually hold, whose accumulated turns become exactly the
 * kind of real OBSERVATION data ai_describe.c already knows how to
 * read (it already reads history.txt; chat_history.txt is the same
 * shape, one more real input file, not a new mechanism).
 *
 * Deliberately NOT constrained-format like ai_describe.c's TARGET/
 * STRENGTH/REASON line - chat is genuinely free text by design (the
 * roadmap's own "Free natural-language chat" mode), so there is
 * nothing to parse/reject here; the entire response is the message.
 *
 * Self-contained, no shared headers - same per-worker-file duplication
 * convention as ai_describe.c (gemma_ask()/connect_op.+x/
 * json_parser.+x, verbatim shape, not shared via a header).
 *
 * Usage: ai_chat.+x <entity_dir> <house_root> <message_text>
 * Appends to: <entity_dir>/chat_history.txt
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define PATH_BUF 4352
#define MAX_HISTORY_LINES 5
#define MAX_CHAT_TURNS 6

static char g_gemma_lan_url[256] = "http://10.0.0.144:11434";
static char g_gemma_lan_model[64] = "gemma3:270m";

/* Verbatim from ai_describe.c - same config, same per-file duplication
 * convention (see that file's own header for why). */
static void kh_load_gemma_lan_config(const char *house_root) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/ai_backend.pdl", house_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *val = eq + 1;
        val[strcspn(val, "\r\n")] = '\0';
        if (strcmp(line, "gemma_lan_url") == 0 && val[0])
            snprintf(g_gemma_lan_url, sizeof(g_gemma_lan_url), "%s", val);
        else if (strcmp(line, "gemma_lan_model") == 0 && val[0])
            snprintf(g_gemma_lan_model, sizeof(g_gemma_lan_model), "%s", val);
    }
    fclose(f);
}

/* Real entity label - reads pal.pdl's own "PAL | name | <value>" line
 * (confirmed real format by reading terumon_001_ember/pal.pdl live).
 * Falls back to the directory's own basename if pal.pdl is missing/
 * malformed - never a fabricated placeholder name. */
static void load_entity_label(const char *entity_dir, char *out, size_t out_sz) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pal.pdl", entity_dir);
    FILE *f = fopen(path, "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            char *p = line;
            char *tok1 = strtok(p, "|");
            char *tok2 = tok1 ? strtok(NULL, "|") : NULL;
            char *tok3 = tok2 ? strtok(NULL, "|\r\n") : NULL;
            if (tok2 && tok3) {
                while (*tok2 == ' ') tok2++;
                size_t l2 = strlen(tok2); while (l2 > 0 && tok2[l2-1] == ' ') tok2[--l2] = '\0';
                if (strcmp(tok2, "name") == 0) {
                    while (*tok3 == ' ') tok3++;
                    size_t l3 = strlen(tok3); while (l3 > 0 && (tok3[l3-1] == ' ' || tok3[l3-1] == '\n' || tok3[l3-1] == '\r')) tok3[--l3] = '\0';
                    snprintf(out, out_sz, "%s", tok3);
                    fclose(f);
                    return;
                }
            }
        }
        fclose(f);
    }
    const char *base = strrchr(entity_dir, '/');
    snprintf(out, out_sz, "%s", base ? base + 1 : entity_dir);
}

/* Same real "own history.txt" observation ai_describe.c already reads
 * - shared real convention, duplicated per this file's own house rule. */
static void load_recent_lines(const char *path, char *out, size_t out_sz, int max_lines) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char lines[MAX_CHAT_TURNS][256];
    int n = 0;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (!line[0]) continue;
        snprintf(lines[n % max_lines], sizeof(lines[0]), "%s", line);
        n++;
    }
    fclose(f);
    int start = n > max_lines ? n % max_lines : 0;
    int count = n < max_lines ? n : max_lines;
    size_t off = 0;
    for (int i = 0; i < count && off < out_sz - 1; i++) {
        int idx = (start + i) % max_lines;
        int written = snprintf(out + off, out_sz - off, "%s\n", lines[idx]);
        if (written < 0) break;
        off += (size_t)written;
    }
}

static void json_escaped(FILE *out, const char *s) {
    for (; *s; s++) {
        if (*s == '"' || *s == '\\') { fputc('\\', out); fputc(*s, out); }
        else if (*s == '\n') fputs("\\n", out);
        else fputc(*s, out);
    }
}

/* Verbatim shape from ai_describe.c's own gemma_ask() - fork/exec
 * connect_op.+x then json_parser.+x via popen, remove() only AFTER
 * pclose() (the real race fix ai_describe.c found and documents in its
 * own header). Duplicated, not shared, matching this file's header. */
static char *gemma_ask(const char *house_root, const char *prompt_text) {
    char request_path[PATH_BUF], response_path[PATH_BUF];
    snprintf(request_path, sizeof(request_path), "/tmp/ai_chat_request_%d.json", getpid());
    snprintf(response_path, sizeof(response_path), "/tmp/ai_chat_response_%d.json", getpid());

    FILE *pf = fopen(request_path, "w");
    if (!pf) return NULL;
    fprintf(pf, "{\"model\":\"%s\",\"stream\":false,\"messages\":[{\"role\":\"user\",\"content\":\"", g_gemma_lan_model);
    json_escaped(pf, prompt_text);
    fputs("\"}]}", pf);
    fclose(pf);

    char full_url[300];
    snprintf(full_url, sizeof(full_url), "%s/api/chat", g_gemma_lan_url);

    char connect_bin[PATH_BUF];
    snprintf(connect_bin, sizeof(connect_bin), "%s/&.widgits/entity-cli/ops/connect_op.+x", house_root);
    char connect_cmd[PATH_BUF * 3];
    snprintf(connect_cmd, sizeof(connect_cmd), "'%s' '%s' '%s' '%s'",
             connect_bin, full_url, request_path, response_path);
    int rc = system(connect_cmd);
    remove(request_path);
    if (rc != 0) { remove(response_path); return NULL; }

    char parser_bin[PATH_BUF];
    snprintf(parser_bin, sizeof(parser_bin), "%s/&.widgits/entity-cli/ops/json_parser.+x", house_root);
    char parser_cmd[PATH_BUF * 2];
    snprintf(parser_cmd, sizeof(parser_cmd), "'%s' '%s' 'message.content'", parser_bin, response_path);
    FILE *jp = popen(parser_cmd, "r");
    if (!jp) { remove(response_path); return NULL; }

    char buf[4096];
    size_t total = 0;
    buf[0] = '\0';
    char chunk[512];
    while (fgets(chunk, sizeof(chunk), jp)) {
        size_t clen = strlen(chunk);
        if (total + clen >= sizeof(buf) - 1) break;
        memcpy(buf + total, chunk, clen);
        total += clen;
        buf[total] = '\0';
    }
    pclose(jp);
    remove(response_path);
    if (total == 0) return NULL;
    return strdup(buf);
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: ai_chat.+x <entity_dir> <house_root> <message_text>\n");
        return 1;
    }
    const char *entity_dir = argv[1];
    const char *house_root = argv[2];
    const char *message = argv[3];

    kh_load_gemma_lan_config(house_root);

    char label[128];
    load_entity_label(entity_dir, label, sizeof(label));

    char chat_path[PATH_BUF];
    snprintf(chat_path, sizeof(chat_path), "%s/chat_history.txt", entity_dir);
    char recent_chat[2048];
    load_recent_lines(chat_path, recent_chat, sizeof(recent_chat), MAX_CHAT_TURNS);

    char hist_path[PATH_BUF];
    snprintf(hist_path, sizeof(hist_path), "%s/history.txt", entity_dir);
    char recent_hist[1024];
    load_recent_lines(hist_path, recent_hist, sizeof(recent_hist), MAX_HISTORY_LINES);

    char prompt[4096];
    snprintf(prompt, sizeof(prompt),
        "You are %s, a small companion entity living on someone's desktop. "
        "Reply in-character, warm and brief (1-2 sentences), plain text only - "
        "no markdown, no stage directions, no TARGET/STRENGTH format.\n\n"
        "Your own recent activity (context only, don't recite it back):\n%s\n\n"
        "Recent conversation:\n%s\n"
        "User: %s\n%s:",
        label, recent_hist[0] ? recent_hist : "(nothing yet)",
        recent_chat[0] ? recent_chat : "(first conversation)",
        message, label);

    char *reply = gemma_ask(house_root, prompt);
    if (!reply) {
        fprintf(stderr, "ai_chat: gemma call failed\n");
        return 1;
    }
    /* trim */
    size_t rl = strlen(reply);
    while (rl > 0 && (reply[rl-1] == '\n' || reply[rl-1] == ' ')) reply[--rl] = '\0';
    char *rp = reply; while (*rp == ' ' || *rp == '\n') rp++;

    time_t now = time(NULL);
    char timebuf[32];
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", localtime(&now));

    FILE *cf = fopen(chat_path, "a");
    if (cf) {
        fprintf(cf, "[%s] USER: %s\n", timebuf, message);
        fprintf(cf, "[%s] %s: %s\n", timebuf, label, rp);
        fclose(cf);
    }

    printf("%s: %s\n", label, rp);
    free(reply);
    return 0;
}
