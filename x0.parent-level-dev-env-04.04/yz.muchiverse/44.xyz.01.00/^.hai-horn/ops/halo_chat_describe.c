/* halo_chat_describe.c - DESCRIBE step for HALO_CHAT
 *
 * Runs after each HALO_CHAT turn. Reads the entity's recent chat history,
 * calls Gemma LAN to pick applicable Concept Bank master nodes, writes
 * candidate EDIT records to the entity's pending_review.txt.
 *
 * Usage: halo_chat_describe.+x <entity_dir> <house_root>
 *
 * Output: writes to <entity_dir>/pending_review.txt in EDIT format:
 *   [timestamp] EDIT | id=<uuid> | type=spoke_weight_delta | target=<node>
 *     | slot=<master> | delta=<float> | reason="<phrase>" | proposer=halo_chat | status=candidate
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <uuid/uuid.h>

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

static int load_spokes(const char *house_root, char spokes[][64], char masters[][64], int max) {
    char spokes_dir[PATH_BUF];
    snprintf(spokes_dir, sizeof(spokes_dir), "%s/&.widgits/concept-bank/data/spokes", house_root);
    DIR *d = opendir(spokes_dir);
    if (!d) return 0;
    int n = 0;
    struct dirent *de;
    while (n < max && (de = readdir(d)) != NULL) {
        size_t len = strlen(de->d_name);
        if (len > 4 && strcmp(de->d_name + len - 4, ".pdl") == 0) {
            size_t namelen = len - 4;
            if (namelen >= sizeof(spokes[0])) namelen = sizeof(spokes[0]) - 1;
            memcpy(spokes[n], de->d_name, namelen);
            spokes[n][namelen] = '\0';

            char spoke_path[PATH_BUF];
            snprintf(spoke_path, sizeof(spoke_path), "%s/%s.pdl", spokes_dir, spokes[n]);
            FILE *f = fopen(spoke_path, "r");
            if (f) {
                char line[256];
                while (fgets(line, sizeof(line), f)) {
                    char *p = line;
                    while (*p == ' ') p++;
                    if (*p == '#') continue;
                    if (strncmp(p, "SLOT", 4) == 0) {
                        char *pt = strstr(p, "POINTS_TO=");
                        if (pt) {
                            pt += 10;
                            int i = 0;
                            while (pt[i] && pt[i] != ' ' && pt[i] != '|' && pt[i] != '\n' && i < 63) {
                                masters[n][i] = pt[i];
                                i++;
                            }
                            masters[n][i] = '\0';
                            break;
                        }
                    }
                }
                fclose(f);
            }
            if (masters[n][0] == '\0') {
                snprintf(masters[n], sizeof(masters[0]), "unknown");
            }
            n++;
        }
    }
    closedir(d);
    return n;
}

static void load_recent_chat(const char *entity_dir, char *out, size_t out_sz) {
    out[0] = '\0';
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/chat_history.txt", entity_dir);
    FILE *f = fopen(path, "r");
    if (!f) { snprintf(out, out_sz, "(no chat yet)"); return; }
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
    if (count == 0) snprintf(out, out_sz, "(chat_history.txt empty)");
}

static void json_escaped(FILE *out, const char *s) {
    for (; *s; s++) {
        if (*s == '"' || *s == '\\') { fputc('\\', out); fputc(*s, out); }
        else if (*s == '\n') fputs("\\n", out);
        else fputc(*s, out);
    }
}

static char *load_bank_context(const char *house_root, const char *entity_dir) {
    char ctx_bin[PATH_BUF];
    snprintf(ctx_bin, sizeof(ctx_bin), "%s/ops/concept_bank_ctx.+x %s", entity_dir, house_root);
    FILE *fp = popen(ctx_bin, "r");
    if (!fp) return NULL;
    char buf[4096];
    size_t total = 0;
    buf[0] = '\0';
    char chunk[512];
    while (fgets(chunk, sizeof(chunk), fp)) {
        size_t clen = strlen(chunk);
        if (total + clen >= sizeof(buf) - 1) break;
        memcpy(buf + total, chunk, clen);
        total += clen;
        buf[total] = '\0';
    }
    pclose(fp);
    if (total == 0) return NULL;
    return strdup(buf);
}

static char *gemma_ask(const char *entity_dir, const char *house_root, const char *prompt_text) {
    char request_path[PATH_BUF], response_path[PATH_BUF];
    snprintf(request_path, sizeof(request_path), "/tmp/halo_describe_request_%d.json", getpid());
    snprintf(response_path, sizeof(response_path), "/tmp/halo_describe_response_%d.json", getpid());

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

static double strength_to_delta(const char *strength) {
    if (strcmp(strength, "high") == 0) return 0.15;
    if (strcmp(strength, "medium") == 0) return 0.08;
    if (strcmp(strength, "low") == 0) return 0.03;
    return 0.0;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: halo_chat_describe.+x <entity_dir> <house_root>\n");
        return 1;
    }
    const char *entity_dir = argv[1];
    const char *house_root = argv[2];

    kh_load_gemma_lan_config(house_root);

    char spokes[MAX_NODES][64];
    char masters[MAX_NODES][64];
    int n_spokes = load_spokes(house_root, spokes, masters, MAX_NODES);
    if (n_spokes == 0) {
        fprintf(stderr, "halo_chat_describe: no real Concept Bank spokes found under "
                        "%s/&.widgits/concept-bank/data/spokes - nothing to describe against\n", house_root);
        return 1;
    }

    char node_list[1024];
    size_t off = 0;
    for (int i = 0; i < n_spokes; i++) {
        int written = snprintf(node_list + off, sizeof(node_list) - off, "%s (→ %s)%s", spokes[i], masters[i], i ? ", " : "");
        if (written < 0 || (size_t)written >= sizeof(node_list) - off) break;
        off += (size_t)written;
    }

    char chat_observation[2048];
    load_recent_chat(entity_dir, chat_observation, sizeof(chat_observation));

    char *bank_ctx = load_bank_context(house_root, entity_dir);

    char prompt[4096];
    if (bank_ctx && bank_ctx[0] && strcmp(bank_ctx, "[]") != 0) {
        snprintf(prompt, sizeof(prompt),
            "OBSERVATION (HALO_CHAT recent conversation):\n%s\n\n"
            "CONCEPT BANK CONTEXT (current weights):\n%s\n\n"
            "EXISTING CONCEPT SPOKES (pick ONLY from this exact list, never invent a new name):\n%s\n\n"
            "Each spoke points to a master concept (shown as: spoke → master).\n"
            "Respond with EXACTLY this format, one line per concept you think applies (1-3 lines), nothing else:\n"
            "TARGET: <spoke from the list> | STRENGTH: high|medium|low | REASON: <one short phrase>\n\n"
            "Example:\n"
            "TARGET: gravity_constant | STRENGTH: high | REASON: user asked about gravity\n"
            "TARGET: gravity_constant | STRENGTH: medium | REASON: discussed force and mass\n"
            "Do NOT use markdown, bold, or extra text. Only the exact format above.",
            chat_observation, bank_ctx, node_list);
    } else {
        snprintf(prompt, sizeof(prompt),
            "OBSERVATION (HALO_CHAT recent conversation):\n%s\n\n"
            "EXISTING CONCEPT SPOKES (pick ONLY from this exact list, never invent a new name):\n%s\n\n"
            "Each spoke points to a master concept (shown as: spoke → master).\n"
            "Respond with EXACTLY this format, one line per concept you think applies (1-3 lines), nothing else:\n"
            "TARGET: <spoke from the list> | STRENGTH: high|medium|low | REASON: <one short phrase>\n\n"
            "Example:\n"
            "TARGET: gravity_constant | STRENGTH: high | REASON: user asked about gravity\n"
            "TARGET: gravity_constant | STRENGTH: medium | REASON: discussed force and mass\n"
            "Do NOT use markdown, bold, or extra text. Only the exact format above.",
            chat_observation, node_list);
    }
    free(bank_ctx);

    char *response = gemma_ask(entity_dir, house_root, prompt);
    if (!response) {
        fprintf(stderr, "halo_chat_describe: gemma call failed\n");
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
            char master_name[64] = "unknown";
            for (int i = 0; i < n_spokes; i++) {
                if (strcmp(spokes[i], target) == 0) {
                    snprintf(master_name, sizeof(master_name), "%s", masters[i]);
                    break;
                }
            }
            uuid_t uuid;
            uuid_generate_random(uuid);
            char uuid_str[37];
            uuid_unparse_lower(uuid, uuid_str);
            double delta = strength_to_delta(strength);
            fprintf(rf, "[%s] EDIT | id=%s | type=spoke_weight_delta | target=%s | slot=%s | delta=%.4f | reason=\"%s\" | proposer=halo_chat | status=candidate\n",
                    timebuf, uuid_str, target, master_name, delta, reason);
        } else {
            fprintf(rf, "[%s] EDIT | REJECTED malformed_output | raw=\"%s\"\n", timebuf, response);
        }
        fclose(rf);
    }

    printf("%s\n", ok ? "candidate written to pending_review.txt" : "malformed output, logged as rejected");
    free(response);
    return 0;
}