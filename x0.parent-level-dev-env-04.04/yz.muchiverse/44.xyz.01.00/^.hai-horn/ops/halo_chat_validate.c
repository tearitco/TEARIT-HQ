/* halo_chat_validate.c - validation & promotion step for HALO_CHAT
 *
 * Reads the entity's pending_review.txt, finds the latest halo_chat
 * candidate EDIT record, validates it against the Concept Bank using
 * the same rules as concept_edit_validate.c, and if valid, either
 * promotes directly to the bank (for high-tier terumons) or queues
 * for review (per AUTO-PROMOTION-RULE.md).
 *
 * Usage: halo_chat_validate.+x <entity_dir> <house_root>
 *
 * Exit 0 = validated & action taken, exit 1 = no candidate or validation failed
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#define MAXLINE 4096
#define DELTA_MIN -0.2
#define DELTA_MAX  0.2

static void trim(char *s) {
    size_t n = strlen(s);
    while (n && (s[n-1] == '\n' || s[n-1] == '\r' || isspace((unsigned char)s[n-1]))) s[--n] = '\0';
}

static int get_field(const char *line, const char *key, char *out, size_t outsz) {
    char buf[MAXLINE];
    snprintf(buf, sizeof(buf), "%s", line);
    char *tok = strtok(buf, "|");
    size_t keylen = strlen(key);
    while (tok) {
        while (*tok == ' ') tok++;
        if (strncmp(tok, key, keylen) == 0 && tok[keylen] == '=') {
            char *val = tok + keylen + 1;
            size_t vlen = strlen(val);
            while (vlen && (val[vlen-1] == ' ' || val[vlen-1] == '\n' || val[vlen-1] == '\r')) { val[--vlen] = '\0'; }
            if (vlen >= 2 && val[0] == '"' && val[vlen-1] == '"') {
                val[vlen-1] = '\0';
                val++;
            }
            snprintf(out, outsz, "%s", val);
            return 1;
        }
        tok = strtok(NULL, "|");
    }
    return 0;
}

static int file_exists(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    fclose(f);
    return 1;
}

static int spoke_has_slot_to(const char *spoke_path, const char *master_name) {
    FILE *f = fopen(spoke_path, "r");
    if (!f) return -1;
    char line[MAXLINE];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        trim(line);
        char *p = line;
        while (*p == ' ') p++;
        if (strncmp(p, "SLOT", 4) != 0) continue;
        char *pt = strstr(p, "POINTS_TO=");
        if (!pt) continue;
        pt += strlen("POINTS_TO=");
        char name[256] = {0};
        int i = 0;
        while (pt[i] && pt[i] != ' ' && pt[i] != '|' && i < 255) { name[i] = pt[i]; i++; }
        name[i] = '\0';
        if (strcmp(name, master_name) == 0) { found = 1; break; }
    }
    fclose(f);
    return found;
}

static int read_tier(const char *entity_dir) {
    char path[4096];
    snprintf(path, sizeof(path), "%s/learning_limits.pdl", entity_dir);
    FILE *f = fopen(path, "r");
    if (!f) return 0; /* preschool = 0 */
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "max_tier")) {
            char *eq = strchr(line, ':');
            if (eq) {
                char *val = eq + 1;
                while (*val == ' ' || *val == '\t') val++;
                val[strcspn(val, "\r\n")] = '\0';
                if (strcmp(val, "preschool") == 0) { fclose(f); return 0; }
                if (strcmp(val, "elementary_hs") == 0) { fclose(f); return 1; }
                if (strcmp(val, "associate_bachelor") == 0) { fclose(f); return 2; }
                if (strcmp(val, "master_phd") == 0) { fclose(f); return 3; }
            }
        }
    }
    fclose(f);
    return 0;
}

static int promote_to_bank(const char *bank_dir, const char *target, const char *slot, double delta, const char *reason) {
    char spoke_path[4096];
    snprintf(spoke_path, sizeof(spoke_path), "%s/data/spokes/%s.pdl", bank_dir, target);
    FILE *f = fopen(spoke_path, "r");
    if (!f) return 0;
    char lines[100][MAXLINE];
    int n = 0;
    char line[MAXLINE];
    while (fgets(line, sizeof(line), f) && n < 100) {
        snprintf(lines[n], sizeof(lines[0]), "%s", line);
        n++;
    }
    fclose(f);

    int weight_line = -1;
    for (int i = 0; i < n; i++) {
        char *p = lines[i];
        while (*p == ' ') p++;
        if (strncmp(p, "SLOT", 4) == 0 && strstr(p, "POINTS_TO=") && strstr(p, slot)) {
            char *wt = strstr(p, "WEIGHT=");
            if (wt) {
                weight_line = i;
                break;
            }
        }
    }
    if (weight_line < 0) return 0;

    double current = 0.0;
    char *wt = strstr(lines[weight_line], "WEIGHT=");
    if (wt) {
        wt += 7;
        current = strtod(wt, NULL);
    }
    double new_weight = current + delta;
    if (new_weight > 1.0) new_weight = 1.0;
    if (new_weight < -1.0) new_weight = -1.0;

    char *eq = strstr(lines[weight_line], "WEIGHT=");
    if (eq) {
        eq += 7;
        char *end = eq;
        while (*end && *end != ' ' && *end != '|' && *end != '\n') end++;
        int prefix_len = eq - lines[weight_line];
        char new_line[MAXLINE];
        snprintf(new_line, sizeof(new_line), "%.*s%.4f%s", prefix_len, lines[weight_line], new_weight, end);
        snprintf(lines[weight_line], sizeof(lines[0]), "%s", new_line);
    }

    f = fopen(spoke_path, "w");
    if (!f) return 0;
    for (int i = 0; i < n; i++) {
        fputs(lines[i], f);
    }
    fclose(f);
    return 1;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: halo_chat_validate <entity_dir> <house_root>\n");
        return 1;
    }
    const char *entity_dir = argv[1];
    const char *house_root = argv[2];

    char pending_path[4096];
    snprintf(pending_path, sizeof(pending_path), "%s/pending_review.txt", entity_dir);
    FILE *pf = fopen(pending_path, "r");
    if (!pf) {
        printf("no pending_review.txt\n");
        return 1;
    }

    char lines[200][MAXLINE];
    int n = 0;
    char line[MAXLINE];
    while (fgets(line, sizeof(line), pf) && n < 200) {
        snprintf(lines[n], sizeof(lines[0]), "%s", line);
        n++;
    }
    fclose(pf);

    int candidate_idx = -1;
    char type[128], target[256], slot[256], delta_s[64], id[64], reason[256], proposer[64];
    for (int i = n - 1; i >= 0; i--) {
        trim(lines[i]);
        if (lines[i][0] == '\0' || lines[i][0] == '#') continue;
        char *p = lines[i];
        while (*p == ' ') p++;
        if (*p == '[') {
            p = strchr(p, ']');
            if (p) {
                p++;
                while (*p == ' ') p++;
            }
        }
        if (!p || strncmp(p, "EDIT", 4) != 0) continue;
        if (!get_field(p, "proposer", proposer, sizeof(proposer)) || strcmp(proposer, "halo_chat") != 0) continue;
        if (!get_field(p, "status", type, sizeof(type)) || strcmp(type, "candidate") != 0) continue;
        candidate_idx = i;
        get_field(p, "id", id, sizeof(id));
        get_field(p, "type", type, sizeof(type));
        get_field(p, "target", target, sizeof(target));
        get_field(p, "slot", slot, sizeof(slot));
        get_field(p, "delta", delta_s, sizeof(delta_s));
        get_field(p, "reason", reason, sizeof(reason));
        break;
    }

    if (candidate_idx < 0) {
        printf("no halo_chat candidate found\n");
        return 1;
    }

    if (strcmp(type, "spoke_weight_delta") != 0) {
        printf("REJECT unsupported type '%s'\n", type);
        return 1;
    }

    char *endptr = NULL;
    double delta = strtod(delta_s, &endptr);
    if (endptr == delta_s || *endptr != '\0') {
        printf("REJECT malformed delta: %s\n", delta_s);
        return 1;
    }
    if (delta < DELTA_MIN || delta > DELTA_MAX) {
        printf("REJECT delta %.4f out of bounds [%.2f, %.2f]\n", delta, DELTA_MIN, DELTA_MAX);
        return 1;
    }

    char bank_dir[4096];
    snprintf(bank_dir, sizeof(bank_dir), "%s/&.widgits/concept-bank", house_root);

    char master_path[4096];
    snprintf(master_path, sizeof(master_path), "%s/data/masters/%s.pdl", bank_dir, slot);
    if (!file_exists(master_path)) {
        printf("REJECT slot='%s' does not resolve to existing master\n", slot);
        return 1;
    }

    char spoke_path[4096];
    snprintf(spoke_path, sizeof(spoke_path), "%s/data/spokes/%s.pdl", bank_dir, target);
    if (!file_exists(spoke_path)) {
        printf("REJECT target='%s' does not resolve to existing spoke\n", target);
        return 1;
    }

    int has_slot = spoke_has_slot_to(spoke_path, slot);
    if (has_slot <= 0) {
        printf("REJECT spoke '%s' has no slot pointing at master '%s'\n", target, slot);
        return 1;
    }

    int tier = read_tier(entity_dir);
    int auto_promote = 0;
    if (tier >= 2) {
        auto_promote = 1;
    }

    if (auto_promote) {
        if (promote_to_bank(bank_dir, target, slot, delta, reason)) {
            lines[candidate_idx][0] = '\0';
            char *bracket = strchr(lines[candidate_idx], '[');
            if (bracket) {
                memmove(lines[candidate_idx], bracket, strlen(bracket) + 1);
            }
            char *status = strstr(lines[candidate_idx], "status=candidate");
            if (status) {
                strcpy(status, "status=promoted");
            }
            pf = fopen(pending_path, "w");
            if (pf) {
                for (int i = 0; i < n; i++) {
                    if (lines[i][0]) fprintf(pf, "%s\n", lines[i]);
                }
                fclose(pf);
            }
            printf("PROMOTED target=%s slot=%s delta=%.4f\n", target, slot, delta);
            return 0;
        }
    }

    printf("QUEUED_FOR_REVIEW target=%s slot=%s delta=%.4f (tier=%d)\n", target, slot, delta, tier);
    return 0;
}