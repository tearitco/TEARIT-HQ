/* rag/rag_search.c - RAG memory retriever for open-gema.
 * Query -> top-N snippets from memory/index.pdl. This is how gemma "sees"
 * a codebase bigger than its context window: it asks, and gets only the
 * relevant file:line snippets to reason over.
 *
 * Usage: rag_search.+x <query> <index_path> [topN]
 *   query      - space-separated words, e.g. "build index read"
 *   index_path - the .pdl index produced by build_index.+x
 *   topN       - max rows to emit (default 8, cap 32)
 *
 * Output (.pdl, deterministic - same query + index = same answer):
 *   SECTION|HIT|FILE|RANGE|COUNT
 *   ------------------------------------
 *   HIT  | file:line-range | words=N | score=M
 *   ...
 *   RAG_SUM|hits=N query="..."
 *
 * Scoring: each query word counts for rows whose word matches. Words that
 * appear in the query text but never in the index are listed in RAG_MISS
 * so a supervisor can see tokenizer gaps (that is a TOOL bug, not gemma's).
 *
 * Self-contained, no shared headers - house style. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ROW 200000
#define MAX_WORD 64

typedef struct {
    char word[MAX_WORD];
    char file[4096];
    int lo, hi, count;
} Row;

static Row rows[MAX_ROW];
static int n_rows = 0;

typedef struct {
    char word[MAX_WORD];
    int score;
} Q;

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: rag_search.+x <query> <index_path> [topN]\n");
        return 1;
    }
    const char *query = argv[1];
    const char *index_path = argv[2];
    int topN = (argc > 3) ? atoi(argv[3]) : 8;
    if (topN < 1) topN = 1;
    if (topN > 32) topN = 32;

    /* tokenize query */
    Q qs[64];
    int n_q = 0;
    char qbuf[1024];
    snprintf(qbuf, sizeof(qbuf), "%s", query);
    char *save = NULL;
    for (char *w = strtok_r(qbuf, " \t\n,.;:()[]{}", &save); w;
         w = strtok_r(NULL, " \t\n,.;:()[]{}", &save)) {
        if (strlen(w) < 2) continue;
        if (n_q < 64) {
            snprintf(qs[n_q].word, sizeof(qs[n_q].word), "%s", w);
            qs[n_q].score = 1;
            n_q++;
        }
    }
    if (n_q == 0) { fprintf(stderr, "rag_search: empty query\n"); return 2; }

    /* load index rows (skip SECTION header + IDX_SUM) */
    FILE *f = fopen(index_path, "r");
    if (!f) { perror("fopen index"); return 1; }
    char *line = NULL;
    size_t len = 0;
    while (getline(&line, &len, f) != -1) {
        if (strncmp(line, "IDX|", 4) != 0) continue;
        /* IDX|word|file:lo-hi [count] */
        char *w = line + 4;
        char *pf = strchr(w, '|');
        if (!pf) continue;
        *pf = '\0';
        char *rest = pf + 1;
        char *pcolon = strchr(rest, ':');
        char *pdash = strrchr(rest, '-');
        char *pcount = strrchr(rest, '[');
        if (!pcolon || !pdash || !pcount) continue;
        if (n_rows >= MAX_ROW) break;
        Row *r = &rows[n_rows++];
        snprintf(r->word, sizeof(r->word), "%s", w);
        size_t flen = pcolon - rest;
        if (flen >= sizeof(r->file)) flen = sizeof(r->file) - 1;
        memcpy(r->file, rest, flen);
        r->file[flen] = '\0';
        r->lo = atoi(pcolon + 1);
        r->hi = atoi(pdash + 1);
        r->count = atoi(pcount + 1);
    }
    free(line);
    fclose(f);

    /* score rows by matching query words */
    int *score = calloc(n_rows, sizeof(int));
    int matched_words[32]; /* distinct query words that hit */
    int n_matched = 0;
    for (int i = 0; i < n_q; i++) {
        int hit_any = 0;
        for (int j = 0; j < n_rows; j++) {
            if (strcmp(rows[j].word, qs[i].word) == 0) {
                score[j] += qs[i].score;
                hit_any = 1;
            }
        }
        if (hit_any && n_matched < 32) {
            matched_words[n_matched++] = i;
        }
    }

    /* rank: score desc, then count desc */
    int *idx = malloc(n_rows * sizeof(int));
    for (int i = 0; i < n_rows; i++) idx[i] = i;
    for (int i = 0; i < n_rows; i++)
        for (int j = i + 1; j < n_rows; j++) {
            int ai = idx[i], bj = idx[j];
            int s = score[bj] - score[ai];
            if (s != 0) { if (s > 0) { int t = idx[i]; idx[i] = idx[j]; idx[j] = t; } continue; }
            s = rows[bj].count - rows[ai].count;
            if (s != 0) { if (s > 0) { int t = idx[i]; idx[i] = idx[j]; idx[j] = t; } continue; }
            if (strcmp(rows[bj].file, rows[ai].file) < 0) { int t = idx[i]; idx[i] = idx[j]; idx[j] = t; }
        }

    printf("SECTION|HIT|FILE|RANGE|COUNT\n");
    printf("------------------------------------\n");
    int emitted = 0;
    for (int i = 0; i < n_rows && emitted < topN; i++) {
        int j = idx[i];
        if (score[j] <= 0) continue;
        printf("HIT|%s:%d-%d|words=%d|score=%d\n",
               rows[j].file, rows[j].lo, rows[j].hi, rows[j].count, score[j]);
        emitted++;
    }

    /* miss report: query words with zero hits (tokenizer gaps) */
    for (int i = 0; i < n_q; i++) {
        int in_matched = 0;
        for (int k = 0; k < n_matched; k++)
            if (matched_words[k] == i) { in_matched = 1; break; }
        if (!in_matched)
            printf("RAG_MISS|word=%s\n", qs[i].word);
    }

    printf("RAG_SUM|hits=%d query=\"%s\"\n", emitted, query);
    free(score);
    free(idx);
    return 0;
}
