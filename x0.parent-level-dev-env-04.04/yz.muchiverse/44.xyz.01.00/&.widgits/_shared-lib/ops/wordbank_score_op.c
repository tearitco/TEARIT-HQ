/* wordbank_score_op - hand-score entity word bank rows (+/-/0/skip) over a copy of pals.
 *
 * Usage: wordbank_score_op.+x --pals-root <pals_root> --queue [--report FILE]
 *        wordbank_score_op.+x --pals-root <pals_root> --apply <score_file> [--report FILE]
 *        wordbank_score_op.+x --interactive <pals_root>
 *        wordbank_score_op.+x --selftest
 *
 *   --queue           list every un-scored SOURCE=seed row across all entities (the to-score queue).
 *                     Format: <entity_relpath>|<canon>|<alias>|<weight>
 *   --apply FILE      read a score file (see --batch-file format below) and apply: change
 *                     SOURCE=seed to SOURCE=user in words.txt, set WEIGHT=1.0 (+1) / 0.0 (-1) /
 *                     0.5 (0), and append a SCORE row to scores.txt with source=hand.
 *   --interactive     console review: one row at a time, keys + / - / 0 / s (skip).
 *   --selftest        check parse + apply on a temp tree.
 *
 * Batch score file format (one row per scored item):
 *   <entity_relpath>|<canon>|<alias>|<valence>
 * where <entity_relpath> is relative to --pals-root (e.g. "asa", "dsr_castle_b"),
 * <canon> and <alias> match a SOURCE=seed row in that entity's words.txt,
 * <valence> is +1 / 0 / -1.
 *
 * The work is text-included from &.widgits/_shared-lib/khtpm_wordbank.c (wb_ensure / wb_* helpers).
 * Design: 08-roadmap/design-docs/ENTITY-WORD-BANK-DESIGN.md sec 3, 4.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <ctype.h>
#include <glob.h>
#include "../khtpm_wordbank.c"

/* ---- helpers not in the core ---- */

/* read entire file into a malloc'd buffer; returns NULL on error */
static WB_UNUSED char *wb_read_all(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return NULL; }
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)sz, f); fclose(f);
    buf[n] = '\0';
    return buf;
}

/* rewrite words.txt: for the matching CANON+ALIAS row whose SOURCE=seed,
   replace it with SOURCE=user + given weight. Returns 1 if a row was updated. */
static WB_UNUSED int wb_update_row_source(const char *words_path, const char *canon, const char *alias, double weight) {
    char *buf = wb_read_all(words_path);
    if (!buf) return 0;
    size_t cap = strlen(buf) + 512;
    char *out = malloc(cap);
    if (!out) { free(buf); return 0; }
    size_t ow = 0;
    int updated = 0;
    const char *p = buf;
    size_t blen = strlen(buf);
    while (p < buf + blen) {
        const char *nl = strchr(p, '\n');
        size_t llen = nl ? (size_t)(nl - p) : strlen(p);
        char row[WB_PATH];
        if (llen >= sizeof(row)) llen = sizeof(row) - 1;
        memcpy(row, p, llen); row[llen] = '\0';
        int matched = 0;
        if (strncmp(row, "CANON=", 6) == 0) {
            char *cbar = strchr(row + 6, '|');
            char *abar = cbar ? strstr(cbar, "ALIAS=") : NULL;
            if (cbar && abar) {
                size_t clen = (size_t)(cbar - (row + 6));
                char *aval = abar + 6;
                char *aend = strchr(aval, '|');
                if (aend) {
                    size_t alen = (size_t)(aend - aval);
                    char cv[512], av[256];
                    if (clen < sizeof(cv) - 1 && alen < sizeof(av) - 1) {
                        memcpy(cv, row + 6, clen); cv[clen] = '\0';
                        memcpy(av, aval, alen); av[alen] = '\0';
                        char *src = strstr(aend, "SOURCE=");
                        if (src && strstr(src, "seed") && !strcmp(cv, canon) && !strcmp(av, alias)) {
                            matched = 1; updated = 1;
                        }
                    }
                }
            }
        }
        if (matched) {
            size_t need = strlen(canon) + strlen(alias) + 64;
            while (ow + need >= cap) { cap *= 2; out = realloc(out, cap); }
            ow += snprintf(out + ow, cap - ow,
                "CANON=%s|ALIAS=%s|WEIGHT=%.4f|SOURCE=user\n", canon, alias, weight);
        } else {
            if (ow + llen + 2 >= cap) { cap *= 2; out = realloc(out, cap); }
            memcpy(out + ow, row, llen); ow += llen;
            if (nl) out[ow++] = '\n';
        }
        p += llen + (nl ? 1 : 0);
    }
    out[ow] = '\0';
    wb_write_file(words_path, out);
    free(buf);
    free(out);
    return updated;
}

/* count un-scored seed rows and build a queue */
typedef struct { char entity[WB_PATH]; char canon[512]; char alias[256]; double weight; } WbQueueRow;

static int sort_rows(const void *a, const void *b) {
    const WbQueueRow *ra = (const WbQueueRow *)a;
    const WbQueueRow *rb = (const WbQueueRow *)b;
    int c = strcmp(ra->entity, rb->entity);
    if (c != 0) return c;
    return strcmp(ra->canon, rb->canon);
}

static int wb_queue_scan(const char *root, WbQueueRow *rows, int max_rows, int *out_n) {
    int n = 0;
    DIR *d = opendir(root); if (!d) return 0;
    struct dirent *e;
    char ents[4096][256]; size_t ne = 0;
    while ((e = readdir(d)) && ne < 4096) {
        if (e->d_name[0] == '.') continue;
        snprintf(ents[ne], sizeof(ents[ne]), "%s", e->d_name);
        ne++;
    }
    closedir(d);
    /* sort */
    for (size_t i = 0; i < ne; i++) for (size_t j = i+1; j < ne; j++)
        if (strcmp(ents[i], ents[j]) > 0) { char t[256]; strcpy(t, ents[i]); strcpy(ents[i], ents[j]); strcpy(ents[j], t); }
    for (size_t i = 0; i < ne && n < max_rows; i++) {
        char pals_ent[WB_BUF]; snprintf(pals_ent, sizeof(pals_ent), "%s/%s", root, ents[i]);
        char wbdir[WB_BUF], words[WB_BUF];
        snprintf(wbdir, sizeof(wbdir), "%s/inventory/zz.wordbank", pals_ent);
        snprintf(words, sizeof(words), "%s/words.txt", wbdir);
        if (!wb_is_dir(wbdir) || !wb_exists(words)) continue;
        FILE *f = fopen(words, "r"); if (!f) continue;
        char line[WB_BUF];
        while (fgets(line, sizeof(line), f) && n < max_rows) {
            if (strncmp(line, "CANON=", 6) != 0) continue;
            char *cbar = strchr(line + 6, '|'); if (!cbar) continue;
            char *abar = strstr(cbar, "ALIAS="); if (!abar) continue;
            char *sbar = strstr(abar, "SOURCE="); if (!sbar) continue;
            *sbar = '\0'; /* cut for comparison */
            if (strstr(sbar + 7, "seed") == NULL) { *sbar = '|'; continue; }
            /* extract canon + alias */
            size_t clen = (size_t)(cbar - (line + 6));
            char *aval = abar + 6; char *aend = strchr(aval, '|');
            size_t alen = aend ? (size_t)(aend - aval) : strlen(aval);
            /* extract weight */
            char *wbar = strstr(line, "WEIGHT=");
            double weight = 0.5;
            if (wbar) weight = atof(wbar + 7);
            if (clen < 512 && alen < 256) {
                memcpy(rows[n].entity, ents[i], sizeof(rows[n].entity));
                memcpy(rows[n].canon, line + 6, clen); rows[n].canon[clen] = '\0';
                memcpy(rows[n].alias, aval, alen > 255 ? 255 : alen); rows[n].alias[alen > 255 ? 255 : alen] = '\0';
                rows[n].weight = weight;
                n++;
            }
            /* restore for any further reads (not needed, file is done) */
            *sbar = '|';
        }
        fclose(f);
        /* recurse into nested inventory entities with pal.pdl... omitted for v1 queue (top-level scope) */
    }
    /* also recurse */
    for (size_t i = 0; i < ne; i++) {
        char pals_ent[WB_BUF]; snprintf(pals_ent, sizeof(pals_ent), "%s/%s", root, ents[i]);
        char inv[WB_BUF]; snprintf(inv, sizeof(inv), "%s/inventory", pals_ent);
        if (wb_is_dir(inv)) {
            DIR *d2 = opendir(inv); if (!d2) continue;
            struct dirent *e2;
            while ((e2 = readdir(d2))) {
                if (e2->d_name[0] == '.') continue;
                char child[WB_BUF], cp[WB_BUF];
                snprintf(child, sizeof(child), "%s/%s", inv, e2->d_name);
                snprintf(cp, sizeof(cp), "%s/pal.pdl", child);
                if (wb_is_dir(child) && wb_exists(cp)) {
                    /* recurse */
                    char sub_rel[WB_PATH]; snprintf(sub_rel, sizeof(sub_rel), "%s/inventory/%s", ents[i], e2->d_name);
                    char sub_path[WB_BUF]; snprintf(sub_path, sizeof(sub_path), "%s/%s", root, sub_rel);
                    char wbdir[WB_BUF], words[WB_BUF];
                    snprintf(wbdir, sizeof(wbdir), "%s/inventory/zz.wordbank", sub_path);
                    snprintf(words, sizeof(words), "%s/words.txt", wbdir);
                    if (!wb_is_dir(wbdir) || !wb_exists(words)) continue;
                    FILE *f = fopen(words, "r"); if (!f) continue;
                    char line[WB_BUF];
                    while (fgets(line, sizeof(line), f) && n < max_rows) {
                        if (strncmp(line, "CANON=", 6) != 0) continue;
                        char *cbar = strchr(line + 6, '|'); if (!cbar) continue;
                        char *abar = strstr(cbar, "ALIAS="); if (!abar) continue;
                        char *wbar = strstr(abar, "WEIGHT=");
                        double weight = 0.5; if (wbar) weight = atof(wbar + 7);
                        char *sbar = strstr(abar, "SOURCE="); if (!sbar) continue;
                        if (strstr(sbar, "seed") == NULL) continue;
                        size_t clen = (size_t)(cbar - (line + 6));
                        char *aval = abar + 6; char *aend = strchr(aval, '|');
                        size_t alen = aend ? (size_t)(aend - aval) : strlen(aval);
                        if (clen < 512 && alen < 256) {
                            memcpy(rows[n].entity, sub_rel, sizeof(rows[n].entity));
                            memcpy(rows[n].canon, line + 6, clen); rows[n].canon[clen] = '\0';
                            memcpy(rows[n].alias, aval, alen > 255 ? 255 : alen); rows[n].alias[alen > 255 ? 255 : 255] = '\0';
                            rows[n].weight = weight;
                            n++;
                        }
                    }
                    fclose(f);
                }
            }
            closedir(d2);
        }
    }
    *out_n = n;
    return n;
}

/* append a SCORE row to scores.txt */
static WB_UNUSED void wb_append_score(const char *scores_path, const char *canon, const char *alias, int valence) {
    char row[WB_BUF];
    /* ts = unix epoch; id = a simple unique-ish stamp */
    snprintf(row, sizeof(row), "SCORE|%s|%s|valence=%+d|source=hand|ts=%ld|id=%s\n",
             canon, alias, valence, (long)time(NULL), alias);
    wb_append_file(scores_path, row);
}

static int selftest(void) {
    char tmpdir[] = "/tmp/wb_score_selftest_XXXXXX";
    int bad = 0;
    if (!mkdtemp(tmpdir)) { fprintf(stderr, "FAIL mkdtemp\n"); return 1; }
    char invdir[512], wbdir[512], words[512], scores[512];
    snprintf(invdir, sizeof(invdir), "%s/inventory", tmpdir);
    mkdir(invdir, 0755);
    snprintf(wbdir, sizeof(wbdir), "%s/zz.wordbank", invdir);
    mkdir(wbdir, 0755);
    snprintf(words, sizeof(words), "%s/words.txt", wbdir);
    snprintf(scores, sizeof(scores), "%s/scores.txt", wbdir);
    FILE *f = fopen(words, "w"); fprintf(f, "CANON=name:foo|ALIAS=foo|WEIGHT=0.5000|SOURCE=seed\n"); fclose(f);
    f = fopen(scores, "w"); fclose(f);
    /* test update */
    if (!wb_update_row_source(words, "name:foo", "foo", 1.0)) { fprintf(stderr, "FAIL update row source\n"); bad++; }
    /* read back */
    char *buf = wb_read_all(words);
    if (!buf || strstr(buf, "SOURCE=user") == NULL || strstr(buf, "WEIGHT=1.0000") == NULL) {
        fprintf(stderr, "FAIL update: words.txt not updated correctly: %s\n", buf ? buf : "(null)"); bad++; }
    free(buf);
    /* already-scored rows should not be re-scored: source is user now, not seed */
    if (wb_update_row_source(words, "name:foo", "foo", 0.5)) { fprintf(stderr, "FAIL update overwrote user row\n"); bad++; }
    /* test append score */
    wb_append_score(scores, "name:foo", "foo", 1);
    buf = wb_read_all(scores);
    if (!buf || strstr(buf, "SCORE|name:foo|foo|valence=+1|source=hand") == NULL) {
        fprintf(stderr, "FAIL append score: %s\n", buf ? buf : "(null)"); bad++; }
    free(buf);
    char cmd[512]; snprintf(cmd, sizeof(cmd), "rm -rf '%s'", tmpdir); system(cmd);
    printf(bad ? "selftest FAILED (%d)\n" : "selftest ok (update, dedup, append)\n", bad);
    return bad ? 1 : 0;
}

int main(int argc, char **argv) {
    int selftest_flag = 0, interactive = 0, queue = 0, apply = 0;
    const char *pals_root = NULL, *report = NULL, *batch_file = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--selftest")) selftest_flag = 1;
        else if (!strcmp(argv[i], "--queue")) queue = 1;
        else if (!strcmp(argv[i], "--apply") && i+1 < argc) { apply = 1; batch_file = argv[++i]; }
        else if (!strcmp(argv[i], "--interactive")) interactive = 1;
        else if (!strcmp(argv[i], "--report") && i+1 < argc) report = argv[++i];
        else if (!strcmp(argv[i], "--pals-root") && i+1 < argc) pals_root = argv[++i];
        else if (argv[i][0] != '-' && !pals_root) pals_root = argv[i];
        else { fprintf(stderr, "unknown argument: %s\n", argv[i]); return 2; }
    }
    if (selftest_flag) return selftest();
    if (apply && (!batch_file || !pals_root)) { fprintf(stderr, "usage: wordbank_score_op.+x --pals-root <root> --apply <score_file> [--report FILE]\n"); return 2; }
    if (!pals_root && !interactive) { fprintf(stderr, "usage: wordbank_score_op.+x --queue <pals_root> [--report FILE] | --pals-root <root> --apply <score_file> | --interactive <pals_root> | --selftest\n"); return 2; }

    WbQueueRow *rows = malloc(sizeof(WbQueueRow) * 4096);
    if (!rows) { fprintf(stderr, "OOM\n"); return 2; }
    int nrows;
    FILE *rf = NULL;
    if (report && !queue) rf = fopen(report, "w");
    else if (report) rf = fopen(report, "w");
    if (!queue && rf) { fprintf(rf, "wordbank_score_op\n"); }

    if (queue) {
        wb_queue_scan(pals_root, rows, 4096, &nrows);
        qsort(rows, nrows, sizeof(WbQueueRow), sort_rows);
        FILE *out = rf ? rf : stdout;
        if (report) fprintf(out, "wordbank_score_op  --queue  pals_root=%s\n", pals_root);
        for (int i = 0; i < nrows; i++)
            fprintf(out, "%s|%s|%s|%.4f\n", rows[i].entity, rows[i].canon, rows[i].alias, rows[i].weight);
        if (report) fprintf(out, "\nSUMMARY: %d un-scored seed rows across all entities\n", nrows);
        if (rf) fclose(rf);
        free(rows);
        return 0;
    }

    if (apply) {
        FILE *sf = fopen(batch_file, "r");
        if (!sf) { fprintf(stderr, "cannot open score file: %s\n", batch_file); return 2; }
        char line[WB_BUF]; int applied = 0, skipped = 0;
        FILE *out = rf ? rf : stdout;
        if (report) fprintf(out, "wordbank_score_op  --apply  file=%s\n", batch_file);
        while (fgets(line, sizeof(line), sf)) {
            line[strcspn(line, "\r\n")] = '\0';
            if (line[0] == '#' || line[0] == '\0') continue;
            char *c1 = strchr(line, '|'); if (!c1) continue; *c1 = '\0';
            char *c2 = strchr(c1+1, '|'); if (!c2) continue; *c2 = '\0';
            char *c3 = strchr(c2+1, '|'); if (!c3) continue; *c3 = '\0';
            char *c4 = c3 + 1; while (*c4 == ' ') c4++;
            char *vend = c4 + strlen(c4); while (vend > c4 && isspace((unsigned char)*(vend-1))) *(--vend) = '\0';
            int valence = atoi(c4);
            if (valence < -1 || valence > 1) { fprintf(out, "SKIP %s: invalid valence '%s'\n", line, c4); skipped++; continue; }
            char words[WB_BUF], scores[WB_BUF];
            snprintf(words, sizeof(words), "%s/%s/inventory/zz.wordbank/words.txt", pals_root, line);
            snprintf(scores, sizeof(scores), "%s/%s/inventory/zz.wordbank/scores.txt", pals_root, line);
            double weight = (valence > 0) ? 1.0 : (valence < 0) ? 0.0 : 0.5;
            if (wb_update_row_source(words, c1+1, c2+1, weight)) {
                wb_append_score(scores, c1+1, c2+1, valence);
                fprintf(out, "APPLIED %s|%s|%s|%d  weight=%.4f\n", line, c1+1, c2+1, valence, weight);
                applied++;
            } else {
                fprintf(out, "SKIP %s|%s (no matching SOURCE=seed row)\n", line, c1+1);
                skipped++;
            }
        }
        fclose(sf);
        if (report) fprintf(out, "\nSUMMARY: applied %d, skipped %d\n", applied, skipped);
        if (rf) fclose(rf);
        free(rows);
        return 0;
    }

    if (interactive) {
        wb_queue_scan(pals_root, rows, 4096, &nrows);
        qsort(rows, nrows, sizeof(WbQueueRow), sort_rows);
        int total = nrows, scored = 0, skipped = 0;
        for (int i = 0; i < nrows; i++) {
            fprintf(stderr, "[%d/%d] %s\n  CANON=%s  ALIAS=%s  weight=%.4f\n  score as: + / - / 0 / s> ",
                    i+1, total, rows[i].entity, rows[i].canon, rows[i].alias, rows[i].weight);
            char resp[8];
            if (!fgets(resp, sizeof(resp), stdin)) break;
            char r = resp[0];
            char words[WB_BUF], scores[WB_BUF];
            snprintf(words, sizeof(words), "%s/%s/inventory/zz.wordbank/words.txt", pals_root, rows[i].entity);
            snprintf(scores, sizeof(scores), "%s/%s/inventory/zz.wordbank/scores.txt", pals_root, rows[i].entity);
            if (r == '+') {
                if (wb_update_row_source(words, rows[i].canon, rows[i].alias, 1.0)) {
                    wb_append_score(scores, rows[i].canon, rows[i].alias, 1);
                    fprintf(stderr, "promoted\n"); scored++;
                }
            } else if (r == '-') {
                if (wb_update_row_source(words, rows[i].canon, rows[i].alias, 0.0)) {
                    wb_append_score(scores, rows[i].canon, rows[i].alias, -1);
                    fprintf(stderr, "demoted\n"); scored++;
                }
            } else if (r == '0') {
                if (wb_update_row_source(words, rows[i].canon, rows[i].alias, 0.5)) {
                    wb_append_score(scores, rows[i].canon, rows[i].alias, 0);
                    fprintf(stderr, "neutral\n"); scored++;
                }
            } else {
                fprintf(stderr, "skipped\n"); skipped++;
            }
        }
        fprintf(stderr, "\nSUMMARY: %d/%d scored, %d skipped\n", scored, total, skipped);
        if (rf) fclose(rf);
        free(rows);
        return 0;
    }

    fprintf(stderr, "no mode selected\n");
    return 2;
}
