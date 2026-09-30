/* ai_describe.c - the real Track 1 first step (AI-PUSH-ROADMAP-AND-
 * NUANCES.md): one real ai_describe call, one real entity, nothing
 * else. Registered as a real event COMMAND in
 * #.ref/menu/event_commands.registry.pdl.
 *
 * House law, unchanged: DESCRIBE, never CLASSIFY. Gemma never emits a
 * number or a decision - it picks ONLY from the real, existing
 * Concept Bank master nodes (read live from
 * &.widgits/concept-bank/data/masters/ - energy/force/motion as
 * of this writing, NOT the illustrative hunger/satiation example from
 * the design docs, which was never actually created as real files),
 * in a fixed, constrained line format tested against the real
 * production LAN model (see PIPELINE-EMOJI-DIAGRAM-REVISED.md):
 *   TARGET: <node> | STRENGTH: high|medium|low | REASON: <phrase>
 *
 * Scope, deliberately minimal, matching the house's own already-
 * established precedent for this exact step
 * (ai_lab_concept_bank_propose.sh's own header comment: "prove the
 * Gemma call + the draft write before building the review/accept UI
 * on top of it... NO accept/merge path yet"): this op proves the call
 * and writes ONE real line to the entity's own pending_review.txt. It
 * does NOT call concept_edit_validate.+x and does NOT touch any real
 * spoke/master file - promotion is a separate, later, not-yet-built
 * step, same as every other honesty checkpoint in this house's AI
 * track has said.
 *
 * Self-contained, no shared headers - matches this house's own
 * established per-worker-file convention (duplicates gemma_ask()'s
 * shape from mylawyer_case_worker.c, connect_op.c/json_parser.c
 * duplicated verbatim into this same ops/ dir).
 *
 * Usage: ai_describe.+x <entity_dir> <house_root>
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>

#define PATH_BUF 4352
#define MAX_NODES 32
#define MAX_HISTORY_LINES 5

static char g_gemma_lan_url[256] = "http://10.0.0.144:11434";
static char g_gemma_lan_model[64] = "gemma3:270m";

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

/* Real candidate node list - read live from the real master files, so
 * this NEVER hardcodes/invents a node name. Empty dir -> empty list
 * (the prompt honestly says so; no fabricated fallback). */
static int load_real_master_nodes(const char *house_root, char nodes[][64], int max) {
    char masters_dir[PATH_BUF];
    snprintf(masters_dir, sizeof(masters_dir), "%s/&.widgits/concept-bank/data/masters", house_root);
    DIR *d = opendir(masters_dir);
    if (!d) return 0;
    int n = 0;
    struct dirent *de;
    while (n < max && (de = readdir(d)) != NULL) {
        size_t len = strlen(de->d_name);
        if (len > 4 && strcmp(de->d_name + len - 4, ".pdl") == 0) {
            size_t namelen = len - 4;
            if (namelen >= sizeof(nodes[0])) namelen = sizeof(nodes[0]) - 1;
            memcpy(nodes[n], de->d_name, namelen);
            nodes[n][namelen] = '\0';
            n++;
        }
    }
    closedir(d);
    return n;
}

/* Real observation: the entity's own last few history.txt lines - its
 * own accumulated real data, per this house's own chatbot-personality
 * design (NIGHT_27_THE_ROBOT_YOU_CAN_TALK_TO.txt): "the entity's own
 * accumulated state... becomes the OBSERVATION". No invented example
 * data. */
static void load_recent_history(const char *entity_dir, char *out, size_t out_sz) {
    out[0] = '\0';
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/history.txt", entity_dir);
    FILE *f = fopen(path, "r");
    if (!f) { snprintf(out, out_sz, "(no history yet)"); return; }
    char lines[MAX_HISTORY_LINES][256];
    int n = 0;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (!line[0]) continue;
        snprintf(lines[n % MAX_HISTORY_LINES], sizeof(lines[0]), "%s", line);
        n++;
    }
    fclose(f);
    int start = n > MAX_HISTORY_LINES ? n % MAX_HISTORY_LINES : 0;
    int count = n < MAX_HISTORY_LINES ? n : MAX_HISTORY_LINES;
    size_t off = 0;
    for (int i = 0; i < count && off < out_sz - 1; i++) {
        int idx = (start + i) % MAX_HISTORY_LINES;
        int written = snprintf(out + off, out_sz - off, "%s\n", lines[idx]);
        if (written < 0) break;
        off += (size_t)written;
    }
    if (count == 0) snprintf(out, out_sz, "(history.txt empty)");
}

static void json_escaped(FILE *out, const char *s) {
    for (; *s; s++) {
        if (*s == '"' || *s == '\\') { fputc('\\', out); fputc(*s, out); }
        else if (*s == '\n') fputs("\\n", out);
        else fputc(*s, out);
    }
}

/* Same real fork/exec + connect_op.+x/json_parser.+x pattern
 * mylawyer_case_worker.c's own gemma_ask() already uses - duplicated,
 * not shared, matching this file's own header note. Returns a
 * malloc'd response string (caller frees), or NULL on any failure. */
static char *gemma_ask(const char *entity_dir, const char *house_root, const char *prompt_text) {
    char request_path[PATH_BUF], response_path[PATH_BUF];
    snprintf(request_path, sizeof(request_path), "/tmp/ai_describe_request_%d.json", getpid());
    snprintf(response_path, sizeof(response_path), "/tmp/ai_describe_response_%d.json", getpid());

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
    if (rc != 0) { remove(response_path); (void)entity_dir; return NULL; }

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

/* Fixed-format parse - never NLP/keyword-guessing on free prose. If
 * the model drifts from the exact format, this simply fails to match
 * and the candidate is dropped (logged, not partially applied) - same
 * "no partial apply" discipline concept_edit_validate.c already uses
 * at its own layer. */
static int parse_response_line(const char *resp, char *target, size_t target_sz,
                                char *strength, size_t strength_sz,
                                char *reason, size_t reason_sz) {
    const char *t = strstr(resp, "TARGET:");
    const char *s = strstr(resp, "STRENGTH:");
    const char *r = strstr(resp, "REASON:");
    if (!t || !s || !r || t > s || s > r) return 0;

    t += 7; while (*t == ' ') t++;
    const char *t_end = strchr(t, '|');
    if (!t_end || t_end > s) return 0;
    size_t tlen = (size_t)(t_end - t);
    while (tlen > 0 && t[tlen - 1] == ' ') tlen--;
    if (tlen == 0 || tlen >= target_sz) return 0;
    memcpy(target, t, tlen); target[tlen] = '\0';

    s += 9; while (*s == ' ') s++;
    const char *s_end = strchr(s, '|');
    if (!s_end || s_end > r) return 0;
    size_t slen = (size_t)(s_end - s);
    while (slen > 0 && s[slen - 1] == ' ') slen--;
    if (slen == 0 || slen >= strength_sz) return 0;
    memcpy(strength, s, slen); strength[slen] = '\0';

    r += 7; while (*r == ' ') r++;
    size_t rlen = strcspn(r, "\r\n");
    while (rlen > 0 && r[rlen - 1] == ' ') rlen--;
    if (rlen >= reason_sz) rlen = reason_sz - 1;
    memcpy(reason, r, rlen); reason[rlen] = '\0';

    return 1;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: ai_describe.+x <entity_dir> <house_root>\n");
        return 1;
    }
    const char *entity_dir = argv[1];
    const char *house_root = argv[2];

    kh_load_gemma_lan_config(house_root);

    char nodes[MAX_NODES][64];
    int n_nodes = load_real_master_nodes(house_root, nodes, MAX_NODES);
    if (n_nodes == 0) {
        fprintf(stderr, "ai_describe: no real Concept Bank master nodes found under "
                        "%s/&.widgits/concept-bank/data/masters - nothing to describe against\n", house_root);
        return 1;
    }

    char node_list[512];
    size_t off = 0;
    for (int i = 0; i < n_nodes; i++) {
        int written = snprintf(node_list + off, sizeof(node_list) - off, "%s%s", i ? ", " : "", nodes[i]);
        if (written < 0 || (size_t)written >= sizeof(node_list) - off) break;
        off += (size_t)written;
    }

    char observation[2048];
    load_recent_history(entity_dir, observation, sizeof(observation));

    char prompt[4096];
    snprintf(prompt, sizeof(prompt),
        "OBSERVATION (this entity's own recent real history):\n%s\n\n"
        "EXISTING CONCEPT NODES (pick ONLY from this exact list, never invent a new name):\n%s\n\n"
        "Respond with EXACTLY this format, one line per concept you think applies (1-3 lines), nothing else:\n"
        "TARGET: <node from the list> | STRENGTH: high|medium|low | REASON: <one short phrase>",
        observation, node_list);

    char *response = gemma_ask(entity_dir, house_root, prompt);
    if (!response) {
        fprintf(stderr, "ai_describe: gemma call failed\n");
        return 1;
    }

    char target[64], strength[16], reason[256];
    int ok = parse_response_line(response, target, sizeof(target), strength, sizeof(strength), reason, sizeof(reason));

    char review_path[PATH_BUF];
    snprintf(review_path, sizeof(review_path), "%s/pending_review.txt", entity_dir);
    FILE *rf = fopen(review_path, "a");
    if (rf) {
        time_t now = time(NULL);
        char timebuf[32];
        strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", localtime(&now));
        if (ok) {
            fprintf(rf, "[%s] AI_DESCRIBE | target=%s | strength=%s | reason=\"%s\" | proposer=ai_describe | status=candidate\n",
                    timebuf, target, strength, reason);
        } else {
            fprintf(rf, "[%s] AI_DESCRIBE | REJECTED malformed_output | raw=\"%s\"\n", timebuf, response);
        }
        fclose(rf);
    }

    printf("%s\n", ok ? "candidate written to pending_review.txt" : "malformed output, logged as rejected");
    free(response);
    return 0;
}
