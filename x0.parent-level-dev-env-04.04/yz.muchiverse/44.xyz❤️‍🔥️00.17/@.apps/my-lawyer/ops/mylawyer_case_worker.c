/* mylawyer_case_worker - the async Build Case FSM (MY_LAWYER_DESIGN.md
 * §3): SELECT_ANGLE -> TOOL_CALL (deterministic dispatch: search_corpus
 * first, gemma-lan search_precedent only if the corpus has nothing
 * usable - matches 045.muchi-pal-agent's own gemma_strategy.c precedent
 * of deterministic tool detection, never asking gemma to emit a TOOL:
 * format itself) -> WRITE_ARGUMENT_POINT (plain string-template fill,
 * NOT another gemma call), repeated for ROUNDS rounds, then ONE closing
 * gemma call. Appends each argument point directly to the REAL, player-
 * visible data/cases/<id>/<side>_case.txt as it's written - never
 * hidden internal state.
 *
 * Async from day one (§3's own explicit design goal, learning directly
 * from my-biotech's real synchronous-blocking mistake, not retrofitted
 * here): PID-tracked via data/cases/<id>/<side>_worker.pid, status
 * polled via data/cases/<id>/<side>_worker_status.txt, same pattern as
 * my-biotech's own research.pid/research_status.txt (and for the same
 * reason - these live under data/, symlinked to the persistent project
 * root, NOT under the ephemeral per-session pieces/system/, so a worker
 * survives the player quitting mid-build).
 *
 * Self-contained, no shared headers (duplicates gemma_ask() from
 * mybiotech_research_worker.c - matches this house's own established
 * per-worker-file convention, not an oversight).
 *
 * Usage: mylawyer_case_worker.+x <project_root> <case_id> <side> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>

#define MAX_LINE 2048
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define ROUNDS 3

static const char *GEMMA_LAN_URL = "http://10.0.0.144:11434";
static const char *GEMMA_LAN_MODEL = "gemma3:270m";

static const char *ANGLES[] = {
    "duty of care", "notice requirements", "breach of contract elements",
    "precedent for similar disputes", "burden of proof", "damages calculation"
};
#define NUM_ANGLES (int)(sizeof(ANGLES) / sizeof(ANGLES[0]))

static void write_kv(const char *path, const char *key, const char *value) {
    FILE *f = fopen(path, "r");
    char lines[64][MAX_LINE];
    int nlines = 0;
    if (f) {
        while (nlines < 64 && fgets(lines[nlines], MAX_LINE, f)) nlines++;
        fclose(f);
    }
    size_t key_len = strlen(key);
    f = fopen(path, "w");
    if (!f) return;
    int found = 0;
    for (int i = 0; i < nlines; i++) {
        if (strncmp(lines[i], key, key_len) == 0 && lines[i][key_len] == '=') {
            fprintf(f, "%s=%s\n", key, value);
            found = 1;
        } else {
            fputs(lines[i], f);
        }
    }
    if (!found) fprintf(f, "%s=%s\n", key, value);
    fclose(f);
}

static char *read_full_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(size + 1);
    if (buf) {
        size_t n = fread(buf, 1, size, f);
        buf[n] = '\0';
    }
    fclose(f);
    return buf;
}

static void json_escaped(FILE *out, const char *s) {
    for (const char *p = s; *p; p++) {
        if (*p == '"') fputs("\\\"", out);
        else if (*p == '\\') fputs("\\\\", out);
        else if (*p == '\n') fputs("\\n", out);
        else fputc(*p, out);
    }
}

static char *gemma_ask(const char *root, const char *user_question) {
    char persona_path[PATH_BUF];
    snprintf(persona_path, sizeof(persona_path), "%s/pieces/registry/personas/lawyer_researcher.txt", root);
    char *persona = read_full_file(persona_path);
    if (!persona) return NULL;

    char request_path[PATH_BUF], response_path[PATH_BUF];
    snprintf(request_path, sizeof(request_path), "/tmp/mylawyer_case_request_%d.json", getpid());
    snprintf(response_path, sizeof(response_path), "/tmp/mylawyer_case_response_%d.json", getpid());

    FILE *pf = fopen(request_path, "w");
    if (!pf) { free(persona); return NULL; }
    fprintf(pf, "{\"model\":\"%s\",\"stream\":false,\"messages\":[{\"role\":\"system\",\"content\":\"", GEMMA_LAN_MODEL);
    json_escaped(pf, persona);
    fputs("\"},{\"role\":\"user\",\"content\":\"", pf);
    json_escaped(pf, user_question);
    fputs("\"}]}", pf);
    fclose(pf);
    free(persona);

    char full_url[256];
    snprintf(full_url, sizeof(full_url), "%s/api/chat", GEMMA_LAN_URL);

    char connect_cmd[PATH_BUF * 3];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(connect_cmd, sizeof(connect_cmd), "'%s/ops/+x/connect_op.+x' '%s' '%s' '%s'",
             root, full_url, request_path, response_path);
#pragma GCC diagnostic pop
    int rc = system(connect_cmd);
    remove(request_path);
    if (rc != 0) { remove(response_path); return NULL; }

    char json_parser_cmd[PATH_BUF * 2];
    snprintf(json_parser_cmd, sizeof(json_parser_cmd), "'%s/ops/+x/json_parser.+x' '%s' 'message.content'", root, response_path);
    FILE *jp = popen(json_parser_cmd, "r");
    if (!jp) { remove(response_path); return NULL; }
    char *content = malloc(4096);
    size_t total = 0, n;
    while ((n = fread(content + total, 1, 4095 - total, jp)) > 0) {
        total += n;
        if (total >= 4095) break;
    }
    content[total] = '\0';
    pclose(jp);
    remove(response_path);

    if (total == 0) { free(content); return NULL; }
    while (total > 0 && (content[total - 1] == '\n' || content[total - 1] == '\r' || content[total - 1] == ' ')) {
        content[--total] = '\0';
    }
    if (total == 0) { free(content); return NULL; }
    return content;
}

static void write_status(const char *root, int case_id, const char *side, int round, const char *step) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/data/cases/%d/%s_worker_status.txt", root, case_id, side);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "round=%d\n", round);
    fprintf(f, "step=%s\n", step ? step : "");
    fprintf(f, "updated_at=%ld\n", (long)time(NULL));
    fclose(f);
}

static void case_doc_append(const char *doc_path, const char *text) {
    FILE *f = fopen(doc_path, "a");
    if (!f) return;
    fprintf(f, "%s\n\n", text);
    fclose(f);
}

/* search_corpus: cheap, instant, no network - grep the side's own
 * corpus file for a line mentioning the angle. Returns a malloc'd
 * matching line, or NULL if nothing usable was found. */
static char *search_corpus(const char *root, const char *side, const char *angle) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/data/corpus/%s.txt", root, side);
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    char lower_angle[128];
    size_t alen = strlen(angle);
    if (alen >= sizeof(lower_angle)) alen = sizeof(lower_angle) - 1;
    for (size_t i = 0; i < alen; i++) lower_angle[i] = (char)tolower((unsigned char)angle[i]);
    lower_angle[alen] = '\0';

    char line[MAX_LINE];
    char *found = NULL;
    while (fgets(line, sizeof(line), f)) {
        char lower_line[MAX_LINE];
        size_t llen = strlen(line);
        if (llen >= sizeof(lower_line)) llen = sizeof(lower_line) - 1;
        for (size_t i = 0; i < llen; i++) lower_line[i] = (char)tolower((unsigned char)line[i]);
        lower_line[llen] = '\0';
        if (strstr(lower_line, lower_angle)) {
            line[strcspn(line, "\r\n")] = '\0';
            found = strdup(line);
            break;
        }
    }
    fclose(f);
    return found;
}

static void corpus_append(const char *root, const char *side, const char *fact_line) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/data/corpus/%s.txt", root, side);
    FILE *f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "%s\n", fact_line);
    fclose(f);
}

static void bump_screen_changed(const char *root) {
    char marker_path[PATH_BUF];
    snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/mylawyer_screen_changed.txt", root);
    FILE *mf = fopen(marker_path, "a");
    if (mf) { fputc('.', mf); fclose(mf); }
}

int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "Usage: mylawyer_case_worker.+x <project_root> <case_id> <side>\n"); return 1; }
    const char *root = argv[1];
    int case_id = atoi(argv[2]);
    const char *side = argv[3];
    srand((unsigned int)(time(NULL) ^ getpid()));

    char pid_path[PATH_BUF];
    snprintf(pid_path, sizeof(pid_path), "%s/data/cases/%d/%s_worker.pid", root, case_id, side);
    FILE *pf = fopen(pid_path, "w");
    if (pf) { fprintf(pf, "%d\n", (int)getpid()); fclose(pf); }

    char meta_path[PATH_BUF], case_doc_path[PATH_BUF];
    snprintf(meta_path, sizeof(meta_path), "%s/data/cases/%d/case_meta.txt", root, case_id);
    snprintf(case_doc_path, sizeof(case_doc_path), "%s/data/cases/%d/%s_case.txt", root, case_id, side);

    char title[256];
    char *meta_content = read_full_file(meta_path);
    title[0] = '\0';
    if (meta_content) {
        char *p = strstr(meta_content, "title=");
        if (p) {
            p += 6;
            char *end = strchr(p, '\n');
            size_t len = end ? (size_t)(end - p) : strlen(p);
            if (len >= sizeof(title)) len = sizeof(title) - 1;
            memcpy(title, p, len);
            title[len] = '\0';
        }
        free(meta_content);
    }

    /* Pick ROUNDS distinct random angles for this build session. */
    int used[NUM_ANGLES] = {0};
    int chosen[ROUNDS];
    for (int i = 0; i < ROUNDS; i++) {
        int idx;
        do { idx = rand() % NUM_ANGLES; } while (used[idx]);
        used[idx] = 1;
        chosen[i] = idx;
    }

    int arg_num = 0;
    for (int r = 0; r < ROUNDS; r++) {
        const char *angle = ANGLES[chosen[r]];

        write_status(root, case_id, side, r + 1, "select_angle");

        char *source_text = search_corpus(root, side, angle);
        int from_precedent_call = 0;
        if (!source_text) {
            write_status(root, case_id, side, r + 1, "search_precedent");
            char question[512];
            snprintf(question, sizeof(question),
                     "Name one plausible legal precedent case relevant to %s. Just the case name, one line, no explanation.",
                     angle);
            char *precedent = gemma_ask(root, question);
            if (precedent) {
                char buf[700];
                snprintf(buf, sizeof(buf), "Precedent: %s (relevant to %s)", precedent, angle);
                source_text = strdup(buf);
                corpus_append(root, side, source_text);
                from_precedent_call = 1;
                free(precedent);
            } else {
                char buf[256];
                snprintf(buf, sizeof(buf), "General principle of %s applies to this dispute.", angle);
                source_text = strdup(buf);
            }
        }

        write_status(root, case_id, side, r + 1, "write_argument_point");
        arg_num++;
        char arg_text[900];
        snprintf(arg_text, sizeof(arg_text),
                 "[Argument %d] Regarding %s: %s%s",
                 arg_num, angle, source_text,
                 from_precedent_call ? "" : " (drawn from prior research corpus)");
        case_doc_append(case_doc_path, arg_text);
        free(source_text);
    }

    /* CLOSING - the one genuinely open-ended synthesis call, per §3. */
    write_status(root, case_id, side, ROUNDS, "closing");
    char *doc_so_far = read_full_file(case_doc_path);
    if (doc_so_far) {
        char closing_question[2600];
        snprintf(closing_question, sizeof(closing_question),
                 "Here is a set of legal arguments for the case \"%s\":\n%s\n"
                 "Write one short closing paragraph that ties them together persuasively. "
                 "Plain text, no markdown, 2-3 sentences.",
                 title[0] ? title : "this case", doc_so_far);
        free(doc_so_far);

        char *closing = gemma_ask(root, closing_question);
        char closing_line[3200];
        if (closing) {
            snprintf(closing_line, sizeof(closing_line), "[Closing] %s", closing);
            free(closing);
        } else {
            snprintf(closing_line, sizeof(closing_line), "[Closing] The evidence and precedent above support this side's position.");
        }
        case_doc_append(case_doc_path, closing_line);
    }

    char status_key[32];
    snprintf(status_key, sizeof(status_key), "%s_status", side);
    write_kv(meta_path, status_key, "ready");

    write_status(root, case_id, side, ROUNDS, "done");
    remove(pid_path);
    bump_screen_changed(root);

    return 0;
}
