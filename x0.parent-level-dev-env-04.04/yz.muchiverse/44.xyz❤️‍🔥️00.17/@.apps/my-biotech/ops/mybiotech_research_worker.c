/* mybiotech_research_worker - P3 UPGRADE (2026-08-02, MY_BIOTECH_DESIGN.md
 * §3/§7): originally a one-shot "name a compound, append one corpus
 * line" worker (P2). Now runs the FULL FSM: HYPOTHESIZE (name a
 * compound) -> ENRICH (4 separate simple calls: use_case/effect/
 * side_effect/price, each appended as its own [Section] to a REAL,
 * player-visible per-compound dossier.txt, same pattern as the sibling
 * my-lawyer game's case documents) -> RECORD (discovered_compounds.txt)
 * -> FDA_REVIEW (a Gemma "regulator" reads the REAL dossier.txt and
 * renders APPROVED/REJECTED, appended to the dossier itself).
 *
 * Still async-from-day-one (the P2 fix, unchanged): PID-tracked via
 * research.pid, status polled via research_status.txt, same pattern as
 * 041.pal-chain's own chain_miner.+x. current_step now advances through
 * the FSM stages so mybiotech_compose_frame.c can show real progress
 * ("⏳ Researching sulfur... [enrich_side_effect]") instead of a single
 * static "researching" message.
 *
 * Self-contained, no shared headers (matches this family's convention -
 * duplicates gemma_ask()/corpus_append()/ledger_append() from
 * mybiotech_menu_input.c rather than sharing a header; that's the
 * house's own established norm, not an oversight).
 *
 * Usage: mybiotech_research_worker.+x <project_root> <element> */
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

static const char *GEMMA_LAN_URL = "http://10.0.0.144:11434";
static const char *GEMMA_LAN_MODEL = "gemma3:270m";

static void write_kv(const char *path, const char *key, const char *value) {
    FILE *f = fopen(path, "r");
    char lines[32][MAX_LINE];
    int nlines = 0;
    if (f) {
        while (nlines < 32 && fgets(lines[nlines], MAX_LINE, f)) nlines++;
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

static int read_kv_int(const char *path, const char *key, int def) {
    FILE *f = fopen(path, "r");
    if (!f) return def;
    char line[MAX_LINE];
    int val = def;
    size_t key_len = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') val = atoi(line + key_len + 1);
    }
    fclose(f);
    return val;
}

static void ledger_append(const char *root, int day, const char *action_type, const char *details) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/data/master_ledger.txt", root);
    FILE *f = fopen(path, "a");
    if (!f) return;
    time_t now = time(NULL);
    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", localtime(&now));
    fprintf(f, "%s|%d|%s|%s\n", ts, day, action_type, details);
    fclose(f);
}

static void corpus_append(const char *root, const char *fact_line) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/data/corpus/player.txt", root);
    FILE *f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "%s\n", fact_line);
    fclose(f);
}

static void json_escaped(FILE *out, const char *s) {
    for (const char *p = s; *p; p++) {
        if (*p == '"') fputs("\\\"", out);
        else if (*p == '\\') fputs("\\\\", out);
        else if (*p == '\n') fputs("\\n", out);
        else fputc(*p, out);
    }
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

static char *gemma_ask(const char *root, const char *user_question) {
    char persona_path[PATH_BUF];
    snprintf(persona_path, sizeof(persona_path), "%s/pieces/registry/personas/biotech_researcher.txt", root);
    char *persona = read_full_file(persona_path);
    if (!persona) return NULL;

    char request_path[PATH_BUF], response_path[PATH_BUF];
    snprintf(request_path, sizeof(request_path), "/tmp/mybiotech_request_%d.json", getpid());
    snprintf(response_path, sizeof(response_path), "/tmp/mybiotech_response_%d.json", getpid());

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

static void write_status(const char *root, int running, const char *element, const char *step) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/data/research_status.txt", root);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "running=%d\n", running);
    fprintf(f, "element=%s\n", element ? element : "");
    fprintf(f, "step=%s\n", step ? step : "");
    fprintf(f, "updated_at=%ld\n", (long)time(NULL));
    fclose(f);
}

/* Compound names come back from Gemma as free text ("Sulfuric acid",
 * "N,N-Dimethyltryptamine") - sanitize into a safe directory/file
 * component: lowercase, alnum only, spaces/punctuation -> underscore,
 * collapsed. Matches chain_create_wallet.c's own valid_wallet_id()
 * philosophy (this family's own precedent for "external text used as a
 * path component is a real injection/traversal risk, sanitize it"). */
static void sanitize_for_path(const char *in, char *out, size_t out_sz) {
    size_t j = 0;
    int last_was_us = 0;
    for (const char *p = in; *p && j < out_sz - 1; p++) {
        if (isalnum((unsigned char)*p)) {
            out[j++] = (char)tolower((unsigned char)*p);
            last_was_us = 0;
        } else if (!last_was_us && j > 0) {
            out[j++] = '_';
            last_was_us = 1;
        }
    }
    while (j > 0 && out[j - 1] == '_') j--;
    out[j] = '\0';
    if (j == 0) snprintf(out, out_sz, "unknown_compound");
}

static void mkdir_p(const char *path) {
    char cmd[PATH_BUF + 32];
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", path);
    int rc = system(cmd);
    (void)rc;
}

static void dossier_append(const char *dossier_path, const char *section_line) {
    FILE *f = fopen(dossier_path, "a");
    if (!f) return;
    fprintf(f, "%s\n", section_line);
    fclose(f);
}

static void bump_screen_changed(const char *root) {
    char marker_path[PATH_BUF];
    snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/mybiotech_screen_changed.txt", root);
    FILE *mf = fopen(marker_path, "a");
    if (mf) { fputc('.', mf); fclose(mf); }
}

/* The deterministic FDA_REVIEW classifier - PITFALL 69 / standards §42.
 * Duplicated from ops/mybiotech_fda_verdict.c (that file's own copy is
 * kept in sync manually - matches this family's no-shared-headers
 * convention). Reads gemma's own REAL DESCRIPTION text (never asked to
 * self-classify) and derives a verdict from simple, auditable keyword
 * counting - not another LLM call. Ties default to REJECTED. */
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

/* One ENRICH sub-call: ask a simple plain-text question about the
 * compound, append the answer as "[Label] <answer>" to the real
 * dossier.txt AND as a one-line summary to the general corpus. Returns
 * 1 if the section was written (gemma responded usably), 0 if this
 * section came back inconclusive (still writes "[Label] unknown" to
 * the dossier - the persona's own "if you don't know, say unknown"
 * instruction, not a silent gap). */
static int enrich_section(const char *root, const char *compound, const char *dossier_path,
                            const char *question, const char *label) {
    char *answer = gemma_ask(root, question);
    char section_line[700];
    if (!answer) {
        snprintf(section_line, sizeof(section_line), "[%s] unknown", label);
        dossier_append(dossier_path, section_line);
        return 0;
    }
    snprintf(section_line, sizeof(section_line), "[%s] %s", label, answer);
    dossier_append(dossier_path, section_line);

    char fact_line[700];
    snprintf(fact_line, sizeof(fact_line), "%s: %s", compound, answer);
    corpus_append(root, fact_line);
    free(answer);
    return 1;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "Usage: mybiotech_research_worker.+x <project_root> <element>\n"); return 1; }
    const char *root = argv[1];
    const char *element = argv[2];

    char pid_path[PATH_BUF];
    snprintf(pid_path, sizeof(pid_path), "%s/data/research.pid", root);
    FILE *pf = fopen(pid_path, "w");
    if (pf) { fprintf(pf, "%d\n", (int)getpid()); fclose(pf); }

    write_status(root, 1, element, "hypothesize");

    char state_path[PATH_BUF], config_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/projects/my-biotech/pieces/mybiotech_menu/state.txt", root);
    snprintf(config_path, sizeof(config_path), "%s/pieces/system/config.txt", root);
    int day = read_kv_int(config_path, "day", 1);

    char question[512];
    snprintf(question, sizeof(question),
             "Name one real chemical compound that could plausibly involve %s. "
             "It should be a pesticide or drug. Just the name, one line, no explanation.",
             element);

    char *compound = gemma_ask(root, question);
    char message[MAX_LINE];

    if (!compound) {
        char detail[256];
        snprintf(detail, sizeof(detail), "gemma unreachable or empty response for %s", element);
        corpus_append(root, detail);
        ledger_append(root, day, "research_attempt", detail);
        snprintf(message, sizeof(message), "Research on %s: inconclusive (gemma-lan did not respond usably).", element);
        write_kv(state_path, "last_message", message);
        write_status(root, 0, element, "done");
        remove(pid_path);
        bump_screen_changed(root);
        return 0;
    }

    char fact_line[600];
    snprintf(fact_line, sizeof(fact_line), "%s can be involved in %s", element, compound);
    corpus_append(root, fact_line);

    char detail[600];
    snprintf(detail, sizeof(detail), "element:%s|compound:%s", element, compound);
    ledger_append(root, day, "research_attempt", detail);

    /* ENRICH: create the real, player-visible dossier and fill it with
     * 4 separate simple calls (MY_BIOTECH_DESIGN.md §3/§5). */
    char safe_name[128];
    sanitize_for_path(compound, safe_name, sizeof(safe_name));

    char dossier_dir[PATH_BUF];
    snprintf(dossier_dir, sizeof(dossier_dir), "%s/data/research/%s", root, safe_name);
    mkdir_p(dossier_dir);

    char dossier_path[PATH_BUF];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(dossier_path, sizeof(dossier_path), "%s/dossier.txt", dossier_dir);
#pragma GCC diagnostic pop

    char header[512];
    snprintf(header, sizeof(header), "COMPOUND: %s\nDISCOVERED FROM: %s\n", compound, element);
    dossier_append(dossier_path, header);

    write_status(root, 1, element, "enrich_use_case");
    char q_use[512];
    snprintf(q_use, sizeof(q_use), "What is %s's primary use case? One short sentence.", compound);
    int ok_use = enrich_section(root, compound, dossier_path, q_use, "Use Case");

    write_status(root, 1, element, "enrich_effect");
    char q_effect[512];
    snprintf(q_effect, sizeof(q_effect), "What is %s's primary effect? One short sentence.", compound);
    int ok_effect = enrich_section(root, compound, dossier_path, q_effect, "Effect");

    write_status(root, 1, element, "enrich_side_effect");
    char q_side[512];
    snprintf(q_side, sizeof(q_side), "What is a known side effect of %s? One short sentence.", compound);
    int ok_side = enrich_section(root, compound, dossier_path, q_side, "Side Effect");

    write_status(root, 1, element, "enrich_price");
    char q_price[512];
    snprintf(q_price, sizeof(q_price), "Estimate a plausible market price in dollars for one unit of %s. Just a number.", compound);
    int ok_price = enrich_section(root, compound, dossier_path, q_price, "Market Price");

    int sections_ok = ok_use + ok_effect + ok_side + ok_price;

    /* FDA_REVIEW - REAL PATTERN CORRECTION 2026-08-02 (PITFALL 69 in
     * !.xyzos-pitfalls+1.txt / §42 in !.xyzos-standards+1.txt - read
     * both, this is the production implementation of that finding).
     * Originally asked gemma to directly classify APPROVED/REJECTED -
     * live-measured as unreliable (wrong 2/3 times on an obviously
     * lethal test compound, and produced ZERO real reasoning when
     * asked to explain). FIXED: ask gemma to DESCRIBE safety concerns
     * (open-ended - what it's actually good at, confirmed by the same
     * measurement: 6/6 correct, real content-aware descriptions), then
     * classify the REAL description text ourselves via a deterministic
     * keyword scorer - see classify_description() below. Only run if
     * at least 2 of the 4 dossier sections came back usably - a
     * dossier that's mostly "unknown" isn't worth judging (real,
     * tunable threshold, see design doc §9 open question 2). */
    const char *approval = "REJECTED";
    char fda_description[3000] = "";
    if (sections_ok >= 2) {
        write_status(root, 1, element, "fda_review");
        char *dossier_content = read_full_file(dossier_path);
        if (dossier_content) {
            char fda_question[3300];
            snprintf(fda_question, sizeof(fda_question),
                     "Here is a research dossier for a proposed compound:\n%s\n"
                     "Describe the safety concerns of this compound in one sentence.",
                     dossier_content);
            free(dossier_content);

            char *description = gemma_ask(root, fda_question);
            if (description) {
                snprintf(fda_description, sizeof(fda_description), "%s", description);
                approval = classify_description(description);
                free(description);
            }
        }
    }

    char verdict_line[3200];
    if (fda_description[0]) {
        snprintf(verdict_line, sizeof(verdict_line), "[FDA Verdict] %s (%s)", approval, fda_description);
    } else {
        snprintf(verdict_line, sizeof(verdict_line), "[FDA Verdict] %s", approval);
    }
    dossier_append(dossier_path, verdict_line);

    /* RECORD - full compound catalog entry, derived from the dossier. */
    char catalog_path[PATH_BUF];
    snprintf(catalog_path, sizeof(catalog_path), "%s/data/discovered_compounds.txt", root);
    FILE *cf = fopen(catalog_path, "a");
    if (cf) {
        fprintf(cf, "%s|%s|%s|%d\n", compound, element, approval, day);
        fclose(cf);
    }
    ledger_append(root, day, "compound_discovered", compound);

    snprintf(message, sizeof(message), "Research on %s -> discovered %s (%s, %d/4 dossier sections filled)",
             element, compound, approval, sections_ok);
    free(compound);

    write_kv(state_path, "last_message", message);
    write_status(root, 0, element, "done");
    remove(pid_path);
    bump_screen_changed(root);

    return 0;
}
