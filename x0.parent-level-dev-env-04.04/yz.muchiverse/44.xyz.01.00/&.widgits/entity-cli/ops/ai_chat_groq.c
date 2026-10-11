/* ai_chat_groq.c - COPY of ai_chat_groq.c for Groq (free tier, OpenAI-compatible API), added 2026-10-07 so the robot window can chat with Groq
 * (the OpenRouter free quota is 50 requests/day per account; Groq is the measured-best provider in ^.hai-horn/ops/horn_chat_backend.c). Same file contract,
 * same per-worker duplication convention. Differences: endpoint, key file raw_groq.txt, models, names. Usage: ai_chat_groq.+x <entity_dir> <house_root> <message_text>
 * Original header follows.
 *
 * ai_chat_groq.c - same real MVP chat round trip as ai_chat.c,
 * OpenRouter backend instead of LAN Gemma. Direct instruction
 * 2026-09-29 ("i also wanna add another, chat-api, that uses the
 * openrouter api can we do that?") - robot_chat_001's "Chat-gemma"
 * button (ai_chat.c) and this one ("Chat-api") are two separate,
 * independent backends on the SAME entity/conversation log, exactly
 * the disambiguation written up the same day in
 * 17.ai/AI-BACKENDS-DISAMBIGUATION.md.
 *
 * Verbatim structure from ai_chat.c (label/history loading, prompt
 * shape, chat_history.txt append) - same per-worker-file duplication
 * convention that file's own header documents. The only real
 * difference is the network call: OpenRouter's chat/completions
 * endpoint (Bearer auth) instead of a bare LAN Ollama /api/chat call,
 * so connect_op.+x (no header support) can't be reused as-is - this
 * forks+execs curl directly with the auth header, same real shape
 * khtpm_open_hai_manager.c's own send_to_openrouter() already uses.
 * json_parser.+x still does the extraction (OpenRouter's real,
 * documented shape: choices[0].message.content - no tools array here,
 * this is plain free-text chat, not open-hai's tool-calling mode).
 *
 * Model: nvidia/nemotron-3-ultra-550b-a55b:free - the one of the three
 * models added 2026-09-29 confirmed reliable end-to-end (see
 * 12.calendar/2026-09-29/2do.md; poolside hit a real 429, dots-studio
 * needed a real extractor fix). Not user-selectable yet - a real,
 * later addition if multiple Groq models are wanted here too.
 *
 * Usage: ai_chat_groq.+x <entity_dir> <house_root> <message_text>
 * Appends to: <entity_dir>/chat_history.txt (same log ai_chat.c uses -
 * one real conversation per entity, backend-agnostic).
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define PATH_BUF 4352
#define MAX_HISTORY_LINES 5
#define MAX_CHAT_TURNS 6
#define OR_MODEL "openai/gpt-oss-120b"

/* Verbatim from ai_chat.c. */
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

/* Verbatim from ai_chat.c. */
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

/* Same real key file khtpm_open_hai_manager.c's own load_groq_key()
 * reads - a shared DATA file location, not shared C (this house's own
 * duplication-per-op convention for the code itself). */
static int load_groq_key(const char *house_root, char *out, size_t outsz) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/&.widgits/open-hai/state/raw_groq.txt", house_root);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char buf[512] = "";
    if (!fgets(buf, sizeof(buf), f)) { fclose(f); return 0; }
    fclose(f);
    buf[strcspn(buf, "\r\n")] = '\0';
    if (!buf[0]) return 0;
    snprintf(out, outsz, "%s", buf);
    return 1;
}

/* REAL, NEW 2026-09-29, direct instruction ("yes we should auto cycle
 * till one works") - live-confirmed nemotron alone is not reliable
 * enough to hardcode: a real, live test this same session got a real
 * upstream 503 "Service temporarily overloaded" from Nvidia, and
 * poolside independently got a real 429 rate-limit - both are exactly
 * the free-tier congestion risk 12.calendar/2026-09-29/2do.md already
 * flagged. dots-studio answered cleanly in the same live test. Order
 * matters only as "try first" - not a quality ranking, all three are
 * the real, verified-working OpenRouter free models from that same
 * doc (open-hai's own g_models[]/BACKEND_OPENROUTER). */
static const char *OR_MODELS[] = {
    "openai/gpt-oss-120b",
    "openai/gpt-oss-20b",
    "qwen/qwen3.8-27b",
};
#define OR_N_MODELS (int)(sizeof(OR_MODELS) / sizeof(OR_MODELS[0]))

/* One real attempt against one model. NULL on any failure (network,
 * timeout, upstream error JSON with no real "choices" - json_parser
 * naturally returns nothing for those shapes, no special-case error
 * detection needed) so the caller can just try the next model. */
static char *groq_ask_one(const char *house_root, const char *key,
                                 const char *model, const char *prompt_text) {
    char request_path[PATH_BUF], response_path[PATH_BUF];
    snprintf(request_path, sizeof(request_path), "/tmp/ai_chat_or_request_%d.json", getpid());
    snprintf(response_path, sizeof(response_path), "/tmp/ai_chat_or_response_%d.json", getpid());

    FILE *pf = fopen(request_path, "w");
    if (!pf) return NULL;
    fprintf(pf, "{\"model\":\"%s\",\"messages\":[{\"role\":\"user\",\"content\":\"", model);
    json_escaped(pf, prompt_text);
    fputs("\"}]}", pf);
    fclose(pf);

    char auth_hdr[600];
    snprintf(auth_hdr, sizeof(auth_hdr), "Authorization: Bearer %s", key);
    char data_arg[PATH_BUF + 2];
    snprintf(data_arg, sizeof(data_arg), "@%s", request_path);

    pid_t pid = fork();
    if (pid == 0) {
        int fd = open(response_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) { dup2(fd, 1); close(fd); }
        execlp("curl", "curl", "-s", "-m", "60", "-X", "POST",
               "https://api.groq.com/openai/v1/chat/completions",
               "-H", "Content-Type: application/json",
               "-H", auth_hdr,
               "-d", data_arg,
               (char *)NULL);
        _exit(127);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    } else {
        remove(request_path);
        return NULL;
    }
    remove(request_path);

    char parser_bin[PATH_BUF];
    snprintf(parser_bin, sizeof(parser_bin), "%s/&.widgits/entity-cli/ops/json_parser.+x", house_root);
    char parser_cmd[PATH_BUF * 2];
    snprintf(parser_cmd, sizeof(parser_cmd), "'%s' '%s' 'choices[0].message.content'", parser_bin, response_path);
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

/* Real OpenRouter round trip: fork+exec curl directly (Bearer auth
 * header - connect_op.+x has no header support, so it can't be reused
 * here), same real endpoint/shape send_to_openrouter()
 * (khtpm_open_hai_manager.c) already uses in production, minus the
 * tools array (plain free-text chat, no tool-calling here). Tries
 * OR_MODELS in order, first real reply wins. */
/* Return: 1 = real reply, 0 = no key at all (config problem, not a
 * provider outage), -1 = key loaded fine but every model in OR_MODELS
 * failed independently - see this function's own header for why that
 * specific case is worth telling apart from "no key" rather than
 * folding both into one generic message. */
static int groq_ask(const char *house_root, const char *prompt_text, char **out_reply) {
    char key[512];
    if (!load_groq_key(house_root, key, sizeof(key))) return 0;
    for (int i = 0; i < OR_N_MODELS; i++) {
        char *reply = groq_ask_one(house_root, key, OR_MODELS[i], prompt_text);
        if (reply) { *out_reply = reply; return 1; }
        fprintf(stderr, "ai_chat_groq: %s failed, trying next model\n", OR_MODELS[i]);
    }
    return -1;
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: ai_chat_groq.+x <entity_dir> <house_root> <message_text>\n");
        return 1;
    }
    const char *entity_dir = argv[1];
    const char *house_root = argv[2];
    const char *message = argv[3];

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

    char *reply = NULL;
    int rc = groq_ask(house_root, prompt, &reply);
    if (rc == 0) {
        fprintf(stderr, "ai_chat_groq: no Groq key at "
                "&.widgits/open-hai/state/raw_groq.txt\n");
        return 1;
    }
    if (rc < 0) {
        /* Real, deliberate distinct message: all %d independent
         * providers failing at once is far more likely a local problem
         * (key revoked/invalid, no network, DNS) than a genuine
         * simultaneous outage across separate providers - direct live
         * feedback ("u can quit after trying all but thats probably a
         * problem and not true"). */
        fprintf(stderr, "ai_chat_groq: all %d Groq models failed - "
                "key loaded fine, so this is more likely a local/network problem "
                "than 3 providers being down at once; check connectivity before "
                "assuming it's just free-tier congestion\n", OR_N_MODELS);
        return 1;
    }
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
