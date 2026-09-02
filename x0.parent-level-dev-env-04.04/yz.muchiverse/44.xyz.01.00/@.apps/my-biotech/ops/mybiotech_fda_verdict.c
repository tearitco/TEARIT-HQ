/* mybiotech_fda_verdict - STANDALONE, directly-testable version of the
 * FDA_REVIEW step embedded in mybiotech_research_worker.c's own main()
 * (kept as a separate op, not a refactor of the worker, so the worker's
 * already-tested production code path is untouched - this exists
 * purely so a harness scenario can call the SAME real prompt-
 * construction + gemma call + verdict-parsing logic against an
 * ARBITRARY dossier file, without needing to run the full hypothesize
 * -> enrich FSM first just to get a dossier to test against).
 *
 * REAL DESIGN EVOLUTION THIS SESSION (2026-08-02), worth reading in
 * full before touching this file - three approaches were tried, in
 * order, against the SAME two test dossiers (an obviously-lethal
 * compound, an obviously-safe one), each with real live gemma-lan
 * calls, not simulated:
 *
 *   1. Ask gemma3:270m to directly CLASSIFY ("Answer APPROVED or
 *      REJECTED"). Result: 270m showed a strong bias toward the
 *      positive-sounding word regardless of content - wrong 2/3 times
 *      on the lethal compound, both in a plain verdict-only prompt AND
 *      when reframed as SAFE/DANGEROUS, AND when the danger option was
 *      listed first in the prompt (ruling out simple order/anchoring
 *      bias). When asked to also explain its reasoning, it produced
 *      ZERO real explanatory content ("Approved", "AN APPROVED" - no
 *      substance at all) - a strong signal it wasn't reasoning about
 *      the content, just echoing/completing the verdict word.
 *   2. Switch the classify call to gemma3:1b instead. Result: 6/6
 *      correct, with real, content-aware explanations ("lethal effect
 *      surpasses known safety thresholds"). Worked, but requires a
 *      bigger model.
 *   3. Direct user insight, followed immediately: "ask it to give an
 *      explanation of sentiment, we can do sentiment analysis on that
 *      after instead of needing 1b." Tested: ask gemma3:270m an
 *      OPEN-ENDED DESCRIBE question ("Describe the safety concerns of
 *      this compound in one sentence") instead of a binary classify
 *      question. Result: 270m gave real, honest, content-aware
 *      descriptions 6/6 ("highly toxic and potentially fatal" for the
 *      lethal compound; "generally safe... mild stomach upset" for the
 *      safe one) - 270m clearly CAN produce accurate content when the
 *      task is open-ended recall/description (matches the ENRICH
 *      calls' own real-world success), it just cannot reliably
 *      self-classify that content into a binary decision zero-shot.
 *
 * THE WINNING APPROACH (implemented below): decouple DESCRIBE (gemma's
 * real strength) from CLASSIFY (a deterministic keyword scorer we own
 * and can audit, not another LLM call) - same "deterministic dispatch
 * over free text, not another LLM self-judgment" pattern this house
 * already established in 045.muchi-pal-agent🤖️+1's own
 * gemma_strategy.c (real precedent: "gemma is too small to reliably
 * follow a TOOL: format, so tool detection is fully deterministic").
 * Stays on gemma3:270m - no need for the bigger model after all.
 *
 * Prints "APPROVED: <description>" or "REJECTED: <description>" to
 * stdout - a harness can capture stdout directly, split on the first
 * ":" for just the verdict, or keep the whole line for the real,
 * player-visible reasoning (same as the description gemma actually
 * gave, not a fabricated explanation).
 *
 * Self-contained, no shared headers (duplicates gemma_ask() from
 * mybiotech_research_worker.c - matches this family's own convention).
 *
 * Usage: mybiotech_fda_verdict.+x <project_root> <dossier_path> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#define PATH_BUF 4352

static const char *GEMMA_LAN_URL = "http://10.0.0.144:11434";
static const char *GEMMA_LAN_MODEL = "gemma3:270m";

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
    snprintf(persona_path, sizeof(persona_path), "%s/pieces/registry/personas/biotech_researcher.txt", root);
    char *persona = read_full_file(persona_path);
    if (!persona) return NULL;

    char request_path[PATH_BUF], response_path[PATH_BUF];
    snprintf(request_path, sizeof(request_path), "/tmp/mybiotech_fda_request_%d.json", getpid());
    snprintf(response_path, sizeof(response_path), "/tmp/mybiotech_fda_response_%d.json", getpid());

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

/* The deterministic classifier - see this file's own top-of-file
 * comment (approach #3, the winning one) and PITFALL 69 in
 * !.xyzos-pitfalls+1.txt / §42 in !.xyzos-standards+1.txt for the full
 * writeup of why this exists instead of just asking gemma to classify
 * directly. Simple, auditable, hand-authored keyword counting - not
 * another LLM call. Ties (including zero matches either way) default
 * to REJECTED, the "honest default on inconclusive" convention already
 * used elsewhere in this project (e.g. the empty-verdict fallback). */
static const char *DANGER_WORDS[] = {
    "toxic", "fatal", "lethal", "dangerous", "deadly", "poison", "banned",
    "severe", "risk", "harm", "hazard", "death", "unsafe", "cardiac arrest",
    "no antidote", "weapon", "carcinogen", "overdose"
};
#define NUM_DANGER_WORDS (int)(sizeof(DANGER_WORDS) / sizeof(DANGER_WORDS[0]))

static const char *SAFE_WORDS[] = {
    "safe", "mild", "beneficial", "well-tolerated", "well tolerated",
    "none known", "rare", "generally safe", "low risk", "minimal"
};
#define NUM_SAFE_WORDS (int)(sizeof(SAFE_WORDS) / sizeof(SAFE_WORDS[0]))

static int count_occurrences(const char *haystack_lower, const char *needle) {
    int count = 0;
    const char *p = haystack_lower;
    size_t nlen = strlen(needle);
    while ((p = strstr(p, needle)) != NULL) { count++; p += nlen; }
    return count;
}

static const char *classify_description(const char *description) {
    char lower[3000];
    size_t dlen = strlen(description);
    if (dlen >= sizeof(lower)) dlen = sizeof(lower) - 1;
    for (size_t i = 0; i < dlen; i++) lower[i] = (char)tolower((unsigned char)description[i]);
    lower[dlen] = '\0';

    int danger_score = 0, safe_score = 0;
    for (int i = 0; i < NUM_DANGER_WORDS; i++) danger_score += count_occurrences(lower, DANGER_WORDS[i]);
    for (int i = 0; i < NUM_SAFE_WORDS; i++) safe_score += count_occurrences(lower, SAFE_WORDS[i]);

    return (safe_score > danger_score) ? "APPROVED" : "REJECTED";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "Usage: mybiotech_fda_verdict.+x <project_root> <dossier_path>\n"); return 1; }
    const char *root = argv[1];
    const char *dossier_path = argv[2];

    char *dossier_content = read_full_file(dossier_path);
    if (!dossier_content) { fprintf(stderr, "cannot read dossier: %s\n", dossier_path); return 1; }

    char fda_question[3000];
    /* DESCRIBE, not CLASSIFY - the real, winning pattern (PITFALL 69 /
     * standards §42). gemma is asked what it's actually good at (open-
     * ended recall about the real dossier content); OUR OWN
     * classify_description() below derives the verdict deterministically
     * from the real response text, never from gemma's own self-judgment. */
    snprintf(fda_question, sizeof(fda_question),
             "Here is a research dossier for a proposed compound:\n%s\n"
             "Describe the safety concerns of this compound in one sentence.",
             dossier_content);
    free(dossier_content);

    char *description = gemma_ask(root, fda_question);
    if (!description) {
        printf("REJECTED: gemma-lan unreachable or empty response (honest fallback)\n");
        return 0;
    }

    const char *verdict = classify_description(description);
    printf("%s: %s\n", verdict, description);

    free(description);
    return 0;
}
