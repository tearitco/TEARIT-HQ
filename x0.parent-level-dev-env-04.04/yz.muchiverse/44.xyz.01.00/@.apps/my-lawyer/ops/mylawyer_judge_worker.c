/* mylawyer_judge_worker - Present to Judge (MY_LAWYER_DESIGN.md §4).
 *
 * *** BUILT WITH PITFALL 69 / xyzos-standards §42 ALREADY APPLIED - see
 * my-biotech's ops/mybiotech_fda_verdict.c full header comment for the
 * measured evidence trail. gemma3:270m was live-measured UNRELIABLE at
 * directly self-classifying (wrong 2/3 times on an obvious case, zero
 * real reasoning when asked to explain), but RELIABLE (6/6) at an open-
 * ended DESCRIBE task. This judge therefore NEVER asks gemma "A or B" -
 * it asks gemma to compare in its own words, then a deterministic,
 * hand-authored keyword+argument-density scorer (owned and auditable by
 * us, not another LLM call) derives the actual winner from that real
 * text. Do not "simplify" this into a direct classify prompt - that is
 * the exact mistake this pattern already fixed once. ***
 *
 * Reads BOTH real case files (data/cases/<id>/plaintiff_case.txt and
 * defendant_case.txt) - actual file contents at judgment time, not a
 * cached summary. Async/PID-tracked (data/cases/<id>/judge_worker.pid,
 * judge_worker_status.txt), same pattern as mylawyer_case_worker.c.
 *
 * Office/bias (§5) is NOT implemented - explicitly out of scope for this
 * first pass (design doc §7 flags the thresholds as "not decided"). The
 * raw, unbiased verdict is written as final.
 *
 * Self-contained, no shared headers (duplicates gemma_ask() - matches
 * this house's convention).
 *
 * Usage: mylawyer_judge_worker.+x <project_root> <case_id> */
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

static void write_kv_int(const char *path, const char *key, int value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", value);
    write_kv(path, key, buf);
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

static void read_kv_str(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char l[MAX_LINE];
    size_t key_len = strlen(key);
    while (fgets(l, sizeof(l), f)) {
        if (strncmp(l, key, key_len) == 0 && l[key_len] == '=') {
            char *v = l + key_len + 1;
            v[strcspn(v, "\n")] = '\0';
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(out, out_sz, "%s", v);
#pragma GCC diagnostic pop
        }
    }
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
    snprintf(persona_path, sizeof(persona_path), "%s/pieces/registry/personas/judge.txt", root);
    char *persona = read_full_file(persona_path);
    if (!persona) return NULL;

    char request_path[PATH_BUF], response_path[PATH_BUF];
    snprintf(request_path, sizeof(request_path), "/tmp/mylawyer_judge_request_%d.json", getpid());
    snprintf(response_path, sizeof(response_path), "/tmp/mylawyer_judge_response_%d.json", getpid());

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

static void write_status(const char *root, int case_id, const char *step) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/data/cases/%d/judge_worker_status.txt", root, case_id);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "step=%s\n", step ? step : "");
    fprintf(f, "updated_at=%ld\n", (long)time(NULL));
    fclose(f);
}

/* sanitize a gemma response for storage as a single-line case_meta.txt
 * value (this project's kv store, like every sibling's, is one
 * key=value per line - an embedded newline would corrupt every key
 * after it). */
static void sanitize_single_line(char *s) {
    for (char *p = s; *p; p++) {
        if (*p == '\n' || *p == '\r') *p = ' ';
    }
}

static int count_occurrences(const char *haystack_lower, const char *needle) {
    int count = 0;
    const char *p = haystack_lower;
    size_t nlen = strlen(needle);
    while ((p = strstr(p, needle)) != NULL) { count++; p += nlen; }
    return count;
}

static int count_arguments(const char *doc) {
    if (!doc) return 0;
    return count_occurrences(doc, "[Argument");
}

/* DESCRIBE, not CLASSIFY (PITFALL 69 / §42, see this file's own header
 * comment). Scores the REAL comparison text gemma actually wrote for
 * phrases favoring the plaintiff/Case A vs the defendant/Case B - a
 * simple, auditable keyword count, not another LLM call. Falls back to
 * argument-count/citation density (§4 step 5's own suggested proxy) on
 * a tie, then document length, then defaults to plaintiff (an honest,
 * documented default - never crashes, never leaves the case stuck). */
static const char *PLAINTIFF_WORDS[] = {
    "case a", "plaintiff", "plaintiff's case is stronger", "favors the plaintiff",
    "favors case a", "a is stronger", "a is more convincing", "a demonstrates"
};
#define NUM_PLAINTIFF_WORDS (int)(sizeof(PLAINTIFF_WORDS) / sizeof(PLAINTIFF_WORDS[0]))

static const char *DEFENDANT_WORDS[] = {
    "case b", "defendant", "defendant's case is stronger", "favors the defendant",
    "favors case b", "b is stronger", "b is more convincing", "b demonstrates"
};
#define NUM_DEFENDANT_WORDS (int)(sizeof(DEFENDANT_WORDS) / sizeof(DEFENDANT_WORDS[0]))

static const char *classify_comparison(const char *comparison, const char *plaintiff_doc, const char *defendant_doc) {
    char lower[3200];
    size_t clen = strlen(comparison);
    if (clen >= sizeof(lower)) clen = sizeof(lower) - 1;
    for (size_t i = 0; i < clen; i++) lower[i] = (char)tolower((unsigned char)comparison[i]);
    lower[clen] = '\0';

    int plaintiff_score = 0, defendant_score = 0;
    for (int i = 0; i < NUM_PLAINTIFF_WORDS; i++) plaintiff_score += count_occurrences(lower, PLAINTIFF_WORDS[i]);
    for (int i = 0; i < NUM_DEFENDANT_WORDS; i++) defendant_score += count_occurrences(lower, DEFENDANT_WORDS[i]);

    if (plaintiff_score != defendant_score) {
        return plaintiff_score > defendant_score ? "plaintiff" : "defendant";
    }

    int plaintiff_args = count_arguments(plaintiff_doc);
    int defendant_args = count_arguments(defendant_doc);
    if (plaintiff_args != defendant_args) {
        return plaintiff_args > defendant_args ? "plaintiff" : "defendant";
    }

    size_t plen = plaintiff_doc ? strlen(plaintiff_doc) : 0;
    size_t dlen = defendant_doc ? strlen(defendant_doc) : 0;
    if (plen != dlen) return plen > dlen ? "plaintiff" : "defendant";

    return "plaintiff";
}

static void bump_screen_changed(const char *root) {
    char marker_path[PATH_BUF];
    snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/mylawyer_screen_changed.txt", root);
    FILE *mf = fopen(marker_path, "a");
    if (mf) { fputc('.', mf); fclose(mf); }
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

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "Usage: mylawyer_judge_worker.+x <project_root> <case_id>\n"); return 1; }
    const char *root = argv[1];
    int case_id = atoi(argv[2]);

    char pid_path[PATH_BUF];
    snprintf(pid_path, sizeof(pid_path), "%s/data/cases/%d/judge_worker.pid", root, case_id);
    FILE *pf = fopen(pid_path, "w");
    if (pf) { fprintf(pf, "%d\n", (int)getpid()); fclose(pf); }

    write_status(root, case_id, "reading_cases");

    char meta_path[PATH_BUF], pcase_path[PATH_BUF], dcase_path[PATH_BUF], config_path[PATH_BUF];
    snprintf(meta_path, sizeof(meta_path), "%s/data/cases/%d/case_meta.txt", root, case_id);
    snprintf(pcase_path, sizeof(pcase_path), "%s/data/cases/%d/plaintiff_case.txt", root, case_id);
    snprintf(dcase_path, sizeof(dcase_path), "%s/data/cases/%d/defendant_case.txt", root, case_id);
    snprintf(config_path, sizeof(config_path), "%s/pieces/system/config.txt", root);

    char *plaintiff_doc = read_full_file(pcase_path);
    char *defendant_doc = read_full_file(dcase_path);

    char title[256], player_side[32];
    read_kv_str(meta_path, "title", title, sizeof(title));
    read_kv_str(meta_path, "player_side", player_side, sizeof(player_side));
    int day = read_kv_int(config_path, "day", 1);

    const char *winner = "plaintiff";
    char comparison_stored[2048] = "gemma-lan unreachable - defaulted to argument/document strength comparison.";

    if (plaintiff_doc && defendant_doc) {
        write_status(root, case_id, "comparing");
        char compare_question[6200];
        snprintf(compare_question, sizeof(compare_question),
                 "Here is Case A (Plaintiff):\n%s\n\nHere is Case B (Defendant):\n%s\n\n"
                 "Compare the legal strength of these two cases in one or two sentences.",
                 plaintiff_doc, defendant_doc);

        char *comparison = gemma_ask(root, compare_question);
        if (comparison) {
            sanitize_single_line(comparison);
            snprintf(comparison_stored, sizeof(comparison_stored), "%s", comparison);
            winner = classify_comparison(comparison, plaintiff_doc, defendant_doc);
            free(comparison);
        } else {
            winner = classify_comparison("", plaintiff_doc, defendant_doc);
        }
    }

    write_kv(meta_path, "status", "judged");
    write_kv(meta_path, "winner", winner);
    write_kv(meta_path, "raw_comparison_text", comparison_stored);
    write_kv(meta_path, "raw_verdict", strcmp(winner, "plaintiff") == 0 ? "A" : "B");
    write_kv_int(meta_path, "bias_applied", 0);

    int player_won = strcmp(winner, player_side) == 0;
    if (player_won) {
        int money = read_kv_int(config_path, "money", 500);
        money += 200;
        write_kv_int(config_path, "money", money);
    }
    write_kv_int(config_path, "active_case_id", 0);

    char details[512];
    snprintf(details, sizeof(details), "case:%d|title:%s|winner:%s|player_won:%d", case_id, title, winner, player_won);
    ledger_append(root, day, "case_judged", details);

    if (plaintiff_doc) free(plaintiff_doc);
    if (defendant_doc) free(defendant_doc);

    write_status(root, case_id, "done");
    remove(pid_path);
    bump_screen_changed(root);

    return 0;
}
