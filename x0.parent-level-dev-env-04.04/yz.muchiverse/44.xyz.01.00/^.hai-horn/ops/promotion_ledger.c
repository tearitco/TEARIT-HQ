/* promotion_ledger.c - replay/simulate and Laplace-scored promotion ledger per A-TEARIT §2.5
 *
 * Manages candidate edits, replays them against observation logs,
 * maintains (reward, punish) counts per edit, computes Laplace score:
 *   score = (reward + 1) / (reward + punish + 2)
 *
 * Usage:
 *   promotion_ledger.+x <concept_bank_dir> init
 *   promotion_ledger.+x <concept_bank_dir> add_candidate <edit_record_file>
 *   promotion_ledger.+x <concept_bank_dir> replay <candidate_id> <obs_feedback_log>
 *   promotion_ledger.+x <concept_bank_dir> score <candidate_id>
 *   promotion_ledger.+x <concept_bank_dir> list [threshold]
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>

#define PATH_BUF 4352
#define MAXLINE 4096
#define MAX_CANDIDATES 1000
#define MAX_OBS 5000

typedef struct {
    char id[64];
    char type[64];
    char target[64];
    char slot[64];
    double delta;
    char reason[256];
    char proposer[64];
    int reward;
    int punish;
    int replayed;
} Candidate;

typedef struct {
    char id[64];
    char type[16];  // OBS or FEEDBACK
    char target[64];
    char outcome[256];
    int valence;   // +1, -1, 0
    char concept[64];
    double intensity;
} ObsRecord;

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

static void ensure_ledger_dir(const char *bank_dir) {
    char ledger_dir[PATH_BUF];
    snprintf(ledger_dir, sizeof(ledger_dir), "%s/promotion_ledger", bank_dir);
    mkdir(ledger_dir, 0755);
}

static char *ledger_path(const char *bank_dir) {
    static char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/promotion_ledger/ledger.txt", bank_dir);
    return path;
}

static char *candidate_path(const char *bank_dir, const char *cand_id) {
    static char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/promotion_ledger/candidates/%s.txt", bank_dir, cand_id);
    return path;
}

static int load_candidates(const char *bank_dir, Candidate *cands, int max) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/promotion_ledger/ledger.txt", bank_dir);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    int n = 0;
    char line[MAXLINE];
    while (fgets(line, sizeof(line), f) && n < max) {
        trim(line);
        if (!line[0] || line[0] == '#') continue;
        Candidate *c = &cands[n];
        get_field(line, "id", c->id, sizeof(c->id));
        get_field(line, "type", c->type, sizeof(c->type));
        get_field(line, "target", c->target, sizeof(c->target));
        get_field(line, "slot", c->slot, sizeof(c->slot));
        char delta_s[64];
        if (get_field(line, "delta", delta_s, sizeof(delta_s))) c->delta = strtod(delta_s, NULL);
        get_field(line, "reason", c->reason, sizeof(c->reason));
        get_field(line, "proposer", c->proposer, sizeof(c->proposer));
        char reward_s[32], punish_s[32], replayed_s[32];
        c->reward = get_field(line, "reward", reward_s, sizeof(reward_s)) ? atoi(reward_s) : 0;
        c->punish = get_field(line, "punish", punish_s, sizeof(punish_s)) ? atoi(punish_s) : 0;
        c->replayed = get_field(line, "replayed", replayed_s, sizeof(replayed_s)) ? atoi(replayed_s) : 0;
        n++;
    }
    fclose(f);
    return n;
}

static int save_candidates(const char *bank_dir, Candidate *cands, int n) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/promotion_ledger/ledger.txt", bank_dir);
    FILE *f = fopen(path, "w");
    if (!f) return 0;
    for (int i = 0; i < n; i++) {
        Candidate *c = &cands[i];
        fprintf(f, "EDIT | id=%s | type=%s | target=%s | slot=%s | delta=%.4f | reason=\"%s\" | proposer=%s | reward=%d | punish=%d | replayed=%d\n",
                c->id, c->type, c->target, c->slot, c->delta, c->reason, c->proposer, c->reward, c->punish, c->replayed);
    }
    fclose(f);
    return 1;
}

static double laplace_score(const Candidate *c) {
    return (double)(c->reward + 1) / (double)(c->reward + c->punish + 2);
}

static int parse_obs_record(const char *line, ObsRecord *o) {
    trim(line);
    if (!line[0] || line[0] == '#') return 0;
    char *p = line;
    while (*p == ' ') p++;
    if (*p == '[') {
        p = strchr(p, ']');
        if (p) {
            p++;
            while (*p == ' ') p++;
        }
    }
    if (strncmp(p, "OBS", 3) == 0) {
        o->type[0] = 'O'; o->type[1] = 'B'; o->type[2] = 'S'; o->type[3] = '\0';
        p += 3;
    } else if (strncmp(p, "FEEDBACK", 8) == 0) {
        strcpy(o->type, "FEEDBACK");
        p += 8;
    } else {
        return 0;
    }
    while (*p == ' ') p++;
    if (*p != '|') return 0;
    p++;
    get_field(p, "id", o->id, sizeof(o->id));
    get_field(p, "target", o->target, sizeof(o->target));
    get_field(p, "outcome", o->outcome, sizeof(o->outcome));
    char valence_s[16], intensity_s[32];
    if (get_field(p, "valence", valence_s, sizeof(valence_s))) o->valence = atoi(valence_s);
    else o->valence = 0;
    get_field(p, "concept", o->concept, sizeof(o->concept));
    if (get_field(p, "intensity", intensity_s, sizeof(intensity_s))) o->intensity = strtod(intensity_s, NULL);
    else o->intensity = 1.0;
    return 1;
}

static int load_obs_log(const char *obs_path, ObsRecord *obs, int max) {
    FILE *f = fopen(obs_path, "r");
    if (!f) return 0;
    int n = 0;
    char line[MAXLINE];
    while (fgets(line, sizeof(line), f) && n < max) {
        if (parse_obs_record(line, &obs[n])) n++;
    }
    fclose(f);
    return n;
}

static int replay_candidate(const Candidate *c, const ObsRecord *obs, int n_obs) {
    int reward = 0, punish = 0;
    for (int i = 0; i < n_obs; i++) {
        const ObsRecord *o = &obs[i];
        if (strcmp(o->type, "FEEDBACK") != 0) continue;
        if (o->concept[0] && strcmp(o->concept, c->slot) != 0) continue;
        if (o->valence > 0) reward += (int)(o->intensity + 0.5);
        else if (o->valence < 0) punish += (int)(o->intensity + 0.5);
    }
    return (reward << 16) | punish;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: promotion_ledger.+x <concept_bank_dir> <init|add_candidate|replay|score|list> [args...]\n");
        return 1;
    }
    const char *bank_dir = argv[1];
    const char *cmd = argv[2];

    ensure_ledger_dir(bank_dir);
    char cand_dir[PATH_BUF];
    snprintf(cand_dir, sizeof(cand_dir), "%s/promotion_ledger/candidates", bank_dir);
    mkdir(cand_dir, 0755);

    if (strcmp(cmd, "init") == 0) {
        printf("Promotion ledger initialized at %s/promotion_ledger\n", bank_dir);
        return 0;
    }

    Candidate cands[MAX_CANDIDATES];
    int n_cands = load_candidates(bank_dir, cands, MAX_CANDIDATES);

    if (strcmp(cmd, "add_candidate") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: promotion_ledger.+x <bank_dir> add_candidate <edit_record_file>\n");
            return 1;
        }
        const char *edit_path = argv[3];
        FILE *ef = fopen(edit_path, "r");
        if (!ef) {
            fprintf(stderr, "Cannot open %s\n", edit_path);
            return 1;
        }
        char line[MAXLINE];
        Candidate new_c = {0};
        new_c.reward = 0;
        new_c.punish = 0;
        new_c.replayed = 0;
        while (fgets(line, sizeof(line), ef)) {
            trim(line);
            if (!line[0] || line[0] == '#') continue;
            char *p = line;
            while (*p == ' ') p++;
            if (*p == '[') {
                p = strchr(p, ']');
                if (p) { p++; while (*p == ' ') p++; }
            }
            if (strncmp(p, "EDIT", 4) != 0) continue;
            get_field(p, "id", new_c.id, sizeof(new_c.id));
            get_field(p, "type", new_c.type, sizeof(new_c.type));
            get_field(p, "target", new_c.target, sizeof(new_c.target));
            get_field(p, "slot", new_c.slot, sizeof(new_c.slot));
            char delta_s[64];
            if (get_field(p, "delta", delta_s, sizeof(delta_s))) new_c.delta = strtod(delta_s, NULL);
            get_field(p, "reason", new_c.reason, sizeof(new_c.reason));
            get_field(p, "proposer", new_c.proposer, sizeof(new_c.proposer));
            break;
        }
        fclose(ef);

        if (!new_c.id[0]) {
            fprintf(stderr, "No valid EDIT record found\n");
            return 1;
        }

        char cand_file[PATH_BUF];
        snprintf(cand_file, sizeof(cand_file), "%s/promotion_ledger/candidates/%s.txt", bank_dir, new_c.id);
        FILE *cf = fopen(cand_file, "w");
        if (!cf) { fprintf(stderr, "Cannot write candidate file\n"); return 1; }
        fprintf(cf, "%s", line);
        fclose(cf);

        if (n_cands >= MAX_CANDIDATES) { fprintf(stderr, "Too many candidates\n"); return 1; }
        cands[n_cands++] = new_c;
        save_candidates(bank_dir, cands, n_cands);
        printf("Added candidate %s to ledger\n", new_c.id);
        return 0;
    }

    if (strcmp(cmd, "replay") == 0) {
        if (argc < 5) {
            fprintf(stderr, "Usage: promotion_ledger.+x <bank_dir> replay <candidate_id> <obs_feedback_log>\n");
            return 1;
        }
        const char *cand_id = argv[3];
        const char *obs_path = argv[4];

        int idx = -1;
        for (int i = 0; i < n_cands; i++) {
            if (strcmp(cands[i].id, cand_id) == 0) { idx = i; break; }
        }
        if (idx < 0) { fprintf(stderr, "Candidate %s not found\n", cand_id); return 1; }

        ObsRecord obs[MAX_OBS];
        int n_obs = load_obs_log(obs_path, obs, MAX_OBS);
        int result = replay_candidate(&cands[idx], obs, n_obs);
        int reward = result >> 16;
        int punish = result & 0xFFFF;
        cands[idx].reward += reward;
        cands[idx].punish += punish;
        cands[idx].replayed = 1;
        save_candidates(bank_dir, cands, n_cands);
        printf("Replayed %s: +%d reward, +%d punish (score=%.4f)\n", cand_id, reward, punish, laplace_score(&cands[idx]));
        return 0;
    }

    if (strcmp(cmd, "score") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: promotion_ledger.+x <bank_dir> score <candidate_id>\n");
            return 1;
        }
        const char *cand_id = argv[3];
        for (int i = 0; i < n_cands; i++) {
            if (strcmp(cands[i].id, cand_id) == 0) {
                printf("%.4f (reward=%d, punish=%d)\n", laplace_score(&cands[i]), cands[i].reward, cands[i].punish);
                return 0;
            }
        }
        fprintf(stderr, "Candidate %s not found\n", cand_id);
        return 1;
    }

    if (strcmp(cmd, "list") == 0) {
        double threshold = 0.0;
        if (argc >= 4) threshold = strtod(argv[3], NULL);
        for (int i = 0; i < n_cands; i++) {
            double s = laplace_score(&cands[i]);
            if (s >= threshold) {
                printf("%s: score=%.4f reward=%d punish=%d delta=%.4f target=%s slot=%s\n",
                       cands[i].id, s, cands[i].reward, cands[i].punish, cands[i].delta, cands[i].target, cands[i].slot);
            }
        }
        return 0;
    }

    fprintf(stderr, "Unknown command: %s\n", cmd);
    return 1;
}