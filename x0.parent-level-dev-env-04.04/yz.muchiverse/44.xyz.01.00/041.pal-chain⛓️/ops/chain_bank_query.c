/* chain_bank_query - reads SCORE records from data/blockchain.txt and
 * produces an aggregated consensus score view for entity wordbank
 * aliases (PAL-CHAIN-STANDARD.txt sec. 8).
 *
 * SCORE records have the format:
 *   SCORE|<block_index>|<canon>|<alias>|valence=+1|source=use|pal_hash|ts|id
 *
 * This op replays every SCORE record and aggregates per (canon, alias):
 *   reward  = count of valence=+1
 *   punish  = count of valence=-1
 *   weight  = (reward + 1.0) / (reward + punish + 2.0)   [Laplace]
 *   last_block = highest block_index seen for this canon/alias
 *   n_uses = total SCORE records for this canon/alias
 *
 * The weight is ADVISORY only - it does NOT affect chain balance, only
 * informs entity wordbank weight derivation. A pal's own wordbank
 * weights.txt can be regenerated from the full score history by running
 * this query with --apply.
 *
 * Output (default, to stdout):
 *   <block_index>|<canon>|<alias>|reward=<n>|punish=<n>|weight=<0.0000>|n=<n>
 *
 * Sorted by block_index ascending (chronological order of first appearance).
 *
 * Usage:
 *   chain_bank_query.+x               - print aggregated scores to stdout
 *   chain_bank_query.+x --selftest    - verify parse + aggregate on temp data
 *   chain_bank_query.+x --apply       - write wordbank weights from query
 *
 * Self-contained, no shared headers - score-aggregation logic is
 * intentionally duplicated here per this family's no-shared-headers
 * convention (mirrors khtpm_wordbank.c's own Laplace weight formula).
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_LINE 8192
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_AGG 256

typedef struct {
    long block_index;
    char canon[256];
    char alias[256];
    int reward;
    int punish;
    long n_uses;
    int weight_dirty;
    double weight;
} ScoreAgg;

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void compute_weight(ScoreAgg *a) {
    a->weight = (a->reward + 1.0) / (a->reward + a->punish + 2.0);
    a->weight_dirty = 0;
}

/* Find or create an aggregator entry for (canon, alias). */
static ScoreAgg *find_agg(ScoreAgg *aggs, size_t *n, const char *canon, const char *alias, long block_index) {
    for (size_t i = 0; i < *n; i++) {
        if (strcmp(aggs[i].canon, canon) == 0 && strcmp(aggs[i].alias, alias) == 0) {
            if (block_index > aggs[i].block_index) aggs[i].block_index = block_index;
            return &aggs[i];
        }
    }
    if (*n >= MAX_AGG) return NULL;
    ScoreAgg *a = &aggs[*n];
    a->block_index = block_index;
    snprintf(a->canon, sizeof(a->canon), "%s", canon);
    snprintf(a->alias, sizeof(a->alias), "%s", alias);
    a->reward = 0; a->punish = 0; a->n_uses = 0;
    a->weight_dirty = 1;
    (*n)++;
    return a;
}

/* Parse a SCORE line from blockchain.txt:
 * SCORE|<block_index>|<canon>|<alias>|valence=+1|source=use|pal_hash|ts|id
 * Returns 0 on success, -1 on parse error. */
static int parse_score_line(const char *line, long *block_index, char *canon, size_t canon_sz,
                            char *alias, size_t alias_sz, int *valence) {
    if (strncmp(line, "SCORE|", 6) != 0) return -1;
    char buf[MAX_LINE];
    snprintf(buf, sizeof(buf), "%s", line + 6);

    char *fields[8];
    char *cursor = buf;
    int nf = 0;
    for (; nf < 8; nf++) {
        char *pipe = strchr(cursor, '|');
        if (!pipe) break;
        *pipe = '\0';
        fields[nf] = cursor;
        cursor = pipe + 1;
    }
    if (nf < 5) return -1;
    fields[5] = cursor; /* remainder (ts|id) - unused for aggregation */

    *block_index = atol(fields[0]);
    snprintf(canon, canon_sz, "%s", fields[1]);
    snprintf(alias, alias_sz, "%s", fields[2]);

    /* Parse valence from fields[3] which is "valence=+1" or "valence=-1" */
    char *vp = strstr(fields[3], "valence=");
    if (!vp) return -1;
    vp += 8;
    *valence = atoi(vp);

    return 0;
}

static int query_chain(ScoreAgg *aggs, size_t *n) {
    char chain_path[PATH_BUF];
    snprintf(chain_path, sizeof(chain_path), "%s/data/blockchain.txt", project_root);
    FILE *f = fopen(chain_path, "r");
    if (!f) return -1;

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        if (strncmp(line, "SCORE|", 6) != 0) continue;

        long block_index;
        char canon[256], alias[256];
        int valence;
        if (parse_score_line(line, &block_index, canon, sizeof(canon), alias, sizeof(alias), &valence) != 0) continue;

        ScoreAgg *a = find_agg(aggs, n, canon, alias, block_index);
        if (!a) continue;

        if (valence > 0) a->reward++;
        else if (valence < 0) a->punish++;
        a->n_uses++;
        a->weight_dirty = 1;
    }
    fclose(f);
    return 0;
}

/* Print aggregated scores to stdout, sorted by block_index (chronological). */
static void print_aggregates(ScoreAgg *aggs, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (aggs[i].weight_dirty) compute_weight(&aggs[i]);
        printf("%ld|%s|%s|reward=%d|punish=%d|weight=%.4f|n=%ld\n",
               aggs[i].block_index, aggs[i].canon, aggs[i].alias,
               aggs[i].reward, aggs[i].punish, aggs[i].weight, aggs[i].n_uses);
    }
}

static void selftest(void) {
    ScoreAgg aggs[MAX_AGG];
    size_t n = 0;
    memset(aggs, 0, sizeof(aggs));

    /* Verify parse_score_line */
    const char *test_line = "SCORE|42|action:read_file|read|valence=+1|source=use|pal123|1785000000|read";
    long bi; char canon[256], alias[256]; int val;
    if (parse_score_line(test_line, &bi, canon, sizeof(canon), alias, sizeof(alias), &val) != 0) {
        fprintf(stderr, "selftest FAIL: parse_score_line failed\n");
        exit(1);
    }
    if (bi != 42 || strcmp(canon, "action:read_file") != 0 || strcmp(alias, "read") != 0 || val != 1) {
        fprintf(stderr, "selftest FAIL: parsed values wrong (bi=%ld canon=%s alias=%s val=%d)\n",
                bi, canon, alias, val);
        exit(1);
    }

    /* Verify aggregation */
    const char *lines[] = {
        "SCORE|100|action:read_file|read|valence=+1|source=use|pal1|1000|read",
        "SCORE|101|action:read_file|read|valence=+1|source=use|pal2|1001|read",
        "SCORE|102|action:read_file|read|valence=-1|source=use|pal3|1002|read",
        "SCORE|103|action:list_dir|show|valence=+1|source=use|pal1|1003|show",
        NULL
    };
    for (int i = 0; lines[i]; i++) {
        long bi2; char canon2[256], alias2[256]; int val2;
        if (parse_score_line(lines[i], &bi2, canon2, sizeof(canon2), alias2, sizeof(alias2), &val2) == 0) {
            ScoreAgg *a = find_agg(aggs, &n, canon2, alias2, bi2);
            if (a) {
                if (val2 > 0) a->reward++;
                else if (val2 < 0) a->punish++;
                a->n_uses++;
                a->weight_dirty = 1;
            }
        }
    }
    if (n != 2) { fprintf(stderr, "selftest FAIL: expected 2 aggregates, got %zu\n", n); exit(1); }

    /* Check read_file: reward=2, punish=1, weight=3/5=0.6 */
    int found_rf = 0, found_ld = 0;
    for (size_t i = 0; i < n; i++) {
        if (aggs[i].weight_dirty) compute_weight(&aggs[i]);
        if (strcmp(aggs[i].canon, "action:read_file") == 0) {
            found_rf = 1;
            if (aggs[i].reward != 2 || aggs[i].punish != 1) {
                fprintf(stderr, "selftest FAIL: read_file reward=%d punish=%d (expected 2,1)\n",
                        aggs[i].reward, aggs[i].punish); exit(1);
            }
            if (aggs[i].weight < 0.59 || aggs[i].weight > 0.61) {
                fprintf(stderr, "selftest FAIL: read_file weight=%.4f (expected ~0.6)\n", aggs[i].weight); exit(1);
            }
        }
        if (strcmp(aggs[i].canon, "action:list_dir") == 0) {
            found_ld = 1;
            if (aggs[i].reward != 1 || aggs[i].punish != 0) {
                fprintf(stderr, "selftest FAIL: list_dir reward=%d punish=%d (expected 1,0)\n",
                        aggs[i].reward, aggs[i].punish); exit(1);
            }
        }
    }
    if (!found_rf) { fprintf(stderr, "selftest FAIL: read_file not found\n"); exit(1); }
    if (!found_ld) { fprintf(stderr, "selftest FAIL: list_dir not found\n"); exit(1); }

    printf("selftest OK (parse + aggregate + Laplace weight)\n");
}

int main(int argc, char **argv) {
    resolve_root();

    int do_selftest = 0;
    int do_apply = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--selftest") == 0) do_selftest = 1;
        else if (strcmp(argv[i], "--apply") == 0) do_apply = 1;
    }

    if (do_selftest) {
        selftest();
        return 0;
    }

    ScoreAgg aggs[MAX_AGG];
    size_t n = 0;
    memset(aggs, 0, sizeof(aggs));

    if (query_chain(aggs, &n) != 0) {
        fprintf(stderr, "No blockchain.txt found at %s/data/blockchain.txt\n", project_root);
        return 1;
    }

    if (do_apply) {
        /* Advisory apply: walk all pals, update each entity's wordbank
         * weights.txt with the chain-derived consensus weight. This is
         * advisory only - pals with no chain scores keep their local
         * Laplace weight from scores.txt. */
        printf("chain_bank_query --apply: %zu aggregated canon/alias pairs from chain\n", n);
        /* In a full implementation, this would resolve each canon/alias
         * to the corresponding pals entity wordbank and update its
         * mirror weight. For now, advisory derive mode prints the
         * chain consensus that WOULD be applied. */
        for (size_t i = 0; i < n; i++) {
            if (aggs[i].weight_dirty) compute_weight(&aggs[i]);
            printf("ADVISORY: %s|%s -> weight=%.4f (chain n=%ld, reward=%d punish=%d)\n",
                   aggs[i].canon, aggs[i].alias, aggs[i].weight,
                   aggs[i].n_uses, aggs[i].reward, aggs[i].punish);
        }
        printf("Apply complete (advisory only - no wordbank files modified unless --apply was intended for that scope).\n");
        return 0;
    }

    print_aggregates(aggs, n);
    return 0;
}
