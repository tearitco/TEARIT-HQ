/* rag/compact.c - RAG index compactor for open-gema.
 * memory/ grows every rebuild. This op shrinks it so the agent's RAG stays
 * bounded and fast. Deterministic (tools never fake results): it drops
 * low-value single-occurrence rows, merges contiguous ranges, and emits a
 * per-file word-frequency summary that gemma reasons over when it "writes"
 * the compact memory notes.
 *
 * Usage: compact.+x <index_path> <out_path> [min_count]
 *   min_count - drop rows whose [count] < min_count (default 2)
 *
 * Output (out_path, .pdl):
 *   IDX   | word|file:lo-hi [count]     (surviving rows)
 *   FILE_SUM| file | top8 word:count    (compact per-file fingerprint)
 *   CMP_SUM| rows_before=N rows_after=M files=K
 *
 * Self-contained, no shared headers - house style. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ROW 200000

typedef struct {
    char word[64];
    char file[4096];
    int lo, hi, count;
} Row;

static Row rows[MAX_ROW];
static int n_rows = 0;

typedef struct { char word[64]; int count; } WF;
typedef struct {
    char file[4096];
    WF wf[8];
    int n_wf;
} FileSum;
#define MAX_FILES 2048
static FileSum files[MAX_FILES];
static int n_files = 0;

static int cmp_row(const void *a, const void *b) {
    const Row *ra = a, *rb = b;
    int c = strcmp(ra->word, rb->word);
    if (c != 0) return c;
    c = strcmp(ra->file, rb->file);
    if (c != 0) return c;
    return ra->lo - rb->lo;
}

static FileSum *get_file(const char *path) {
    for (int i = 0; i < n_files; i++)
        if (strcmp(files[i].file, path) == 0) return &files[i];
    if (n_files >= MAX_FILES) return NULL;
    FileSum *f = &files[n_files++];
    snprintf(f->file, sizeof(f->file), "%s", path);
    f->n_wf = 0;
    return f;
}

static void add_wf(FileSum *f, const char *word, int count) {
    for (int i = 0; i < f->n_wf; i++) {
        if (strcmp(f->wf[i].word, word) == 0) { f->wf[i].count += count; return; }
    }
    if (f->n_wf < 8) {
        snprintf(f->wf[f->n_wf].word, sizeof(f->wf[f->n_wf].word), "%s", word);
        f->wf[f->n_wf].count = count;
        f->n_wf++;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: compact.+x <index_path> <out_path> [min_count]\n");
        return 1;
    }
    const char *index_path = argv[1];
    const char *out_path = argv[2];
    int min_count = (argc > 3) ? atoi(argv[3]) : 2;
    if (min_count < 1) min_count = 1;

    FILE *f = fopen(index_path, "r");
    if (!f) { perror("fopen index"); return 1; }
    char *line = NULL;
    size_t len = 0;
    while (getline(&line, &len, f) != -1) {
        if (strncmp(line, "IDX|", 4) != 0) continue;
        char *w = line + 4;
        char *pf = strchr(w, '|');
        if (!pf) continue;
        *pf = '\0';
        char *rest = pf + 1;
        char *pcolon = strchr(rest, ':');
        char *pdash = strchr(rest, '-');
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
    int before = n_rows;

    /* drop low-value rows */
    int out_n = 0;
    for (int i = 0; i < n_rows; i++)
        if (rows[i].count >= min_count) rows[out_n++] = rows[i];
    n_rows = out_n;

    /* sort + merge contiguous same word+file ranges */
    qsort(rows, n_rows, sizeof(Row), cmp_row);
    int m = 0;
    for (int i = 0; i < n_rows; i++) {
        if (m > 0 && strcmp(rows[m-1].word, rows[i].word) == 0 &&
            strcmp(rows[m-1].file, rows[i].file) == 0 &&
            rows[i].lo == rows[m-1].hi + 1) {
            rows[m-1].hi = rows[i].hi;
            rows[m-1].count += rows[i].count;
        } else {
            rows[m++] = rows[i];
        }
    }
    n_rows = m;

    /* build per-file top-word summary */
    for (int i = 0; i < n_rows; i++) {
        FileSum *fs = get_file(rows[i].file);
        if (fs) add_wf(fs, rows[i].word, rows[i].count);
    }

    FILE *out = fopen(out_path, "w");
    if (!out) { perror("fopen out"); return 1; }
    fprintf(out, "SECTION|WORD|LOC\n");
    fprintf(out, "------------------------------------\n");
    for (int i = 0; i < n_rows; i++)
        fprintf(out, "IDX|%s|%s:%d-%d [%d]\n", rows[i].word, rows[i].file,
                rows[i].lo, rows[i].hi, rows[i].count);
    fprintf(out, "------------------------------------\n");
    fprintf(out, "FILE_SUM\n");
    for (int i = 0; i < n_files; i++) {
        fprintf(out, "FILE_SUM|%s|", files[i].file);
        for (int k = 0; k < files[i].n_wf; k++)
            fprintf(out, "%s:%d%s", files[i].wf[k].word, files[i].wf[k].count,
                    k + 1 < files[i].n_wf ? " " : "");
        fprintf(out, "\n");
    }
    fprintf(out, "CMP_SUM|rows_before=%d rows_after=%d files=%d\n",
            before, n_rows, n_files);
    fclose(out);

    printf("compact: %d -> %d rows (%d files) -> %s\n",
           before, n_rows, n_files, out_path);
    return 0;
}
