/* harness_bank_op - the harness BEHAVIOR BANK: each harness has a sidecar bank/<case file name>.behavior.pdl (a behavior entry in the house Concept-Bank / Behavior-Bank shape:
 * keywords, weighted synonyms, sentences about how it works, weighted slots to z-node concepts = the "hidden layer", corpus weights, the exec sequence) and its results ledger
 * feeds the reward/punish counts, so the entry's weight is a LIVE Laplace estimate, not a guess.  Design: HARNESS-BEHAVIOR-BANK-DESIGN.md
 *
 * Usage: harness_bank_op <cases.pdl>   (= `update <cases.pdl>`, the one-argument form a pal's exec can pass; prisc exec passes only one literal): reads the case file's `RESULTS | <path>` row, reads new ledger rows since the entry's
 *                                               `COUNTS ... cursor=<bytes>` (append-only marker, never mtime), adds PASS rows to reward and FAIL rows to punish, rewrites COUNTS and
 *                                               `WEIGHT | <(reward+1)/(reward+punish+2)>` into bank/live/<name>.behavior.pdl (a git-ignored copy of the seed; other lines untouched). Running it twice counts nothing twice.
 *        harness_bank_op find <bank_dir> <word>...   ranks the entries for the query words: per word the best of keyword 1.0, synonym <weight>, z-node name 0.6*<slot weight>,
 *                                               word inside a sentence 0.25; summed over the words, times the entry weight; prints  <score>|<id>|<file>  best first.
 * Sidecar rows (pipe-delimited `KEY | a | b`, '#' comments):  BEHAVIOR | id    KEYWORDS | a, b, c    SYNONYM | word | weight    SENTENCE | text
 *   SLOT | idx | POINTS_TO=<master> | WEIGHT=<-1..1>    CORPUS | name | WEIGHT=<w>    SEQUENCE | n | <exec line>    COUNTS | reward=N | punish=N | cursor=BYTES    WEIGHT | w
 * Weights on SYNONYM/SLOT/CORPUS rows are hand-seeded (SOURCE | hand-authored ...): the owner/validation loop promotes real ones; the entry WEIGHT is the only measured number here. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <libgen.h>
#include <sys/stat.h>
#define P 4096
#define MAXL 400
typedef struct { char file[P], id[128]; char *lines[MAXL]; int n; double weight; } Entry;

static char *trim(char *s) { char *e; while (*s == ' ' || *s == '\t') s++; e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0; return s; }
static void lower(char *s) { for (; *s; s++) *s = (char)tolower((unsigned char)*s); }
static int load(const char *path, Entry *e) {
    FILE *f = fopen(path, "r"); char line[P];
    memset(e, 0, sizeof *e); snprintf(e->file, sizeof e->file, "%s", path); e->weight = 0.5;
    if (!f) return -1;
    while (e->n < MAXL && fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0; e->lines[e->n++] = strdup(line);
        if (!strncmp(line, "BEHAVIOR", 8)) { char *b = strchr(line, '|'); if (b) snprintf(e->id, sizeof e->id, "%s", trim(b + 1)); }
        else if (!strncmp(line, "WEIGHT", 6)) { char *b = strchr(line, '|'); if (b) e->weight = atof(b + 1); }
    }
    fclose(f); return 0;
}
static void free_entry(Entry *e) { for (int i = 0; i < e->n; i++) free(e->lines[i]); e->n = 0; }

static int cmd_update(const char *casefile) {
    char cpath[P], cdir[P], cbase[P], line[P], results[P] = "", bank[P], live[P]; FILE *f; Entry e; long cursor = 0; int reward = 0, punish = 0, ci = -1, wi = -1;
    if (!realpath(casefile, cpath)) { fprintf(stderr, "harness_bank_op: no case file\n"); return 2; }
    snprintf(cdir, sizeof cdir, "%s", cpath); { char *d = dirname(cdir); memmove(cdir, d, strlen(d) + 1); }
    { char t[P]; snprintf(t, sizeof t, "%s", cpath); snprintf(cbase, sizeof cbase, "%s", basename(t)); char *dot = strrchr(cbase, '.'); if (dot) *dot = 0; }
    snprintf(bank, sizeof bank, "%s/../bank/%s.behavior.pdl", cdir, cbase);
    snprintf(live, sizeof live, "%s/../bank/live/%s.behavior.pdl", cdir, cbase);
    if (!(f = fopen(cpath, "r"))) return 2;
    while (fgets(line, sizeof line, f)) { char *p = trim(line); if (!strncmp(p, "RESULTS", 7)) { char *b = strchr(p, '|'); if (b) snprintf(results, sizeof results, "%s/%s", cdir, trim(b + 1)); break; } }
    fclose(f);
    if (!results[0]) return 2;
    /* the tracked sidecar is the hand-authored SEED; live counts go to bank/live/ (git-ignored runtime state), which is read first when it exists */
    if (load(live, &e) != 0 && load(bank, &e) != 0) { fprintf(stderr, "harness_bank_op: no sidecar %s\n", bank); return 2; }
    for (int i = 0; i < e.n; i++) {
        if (!strncmp(e.lines[i], "COUNTS", 6)) {
            ci = i; char *p = strstr(e.lines[i], "reward="); if (p) reward = atoi(p + 7);
            p = strstr(e.lines[i], "punish="); if (p) punish = atoi(p + 7);
            p = strstr(e.lines[i], "cursor="); if (p) cursor = atol(p + 7);
        } else if (!strncmp(e.lines[i], "WEIGHT", 6)) wi = i;
    }
    if ((f = fopen(results, "r"))) {
        struct stat st; if (fstat(fileno(f), &st) == 0 && cursor > st.st_size) cursor = 0;   /* ledger replaced/shrunk: start over */
        fseek(f, cursor, SEEK_SET);
        while (fgets(line, sizeof line, f)) { if (!strncmp(line, "PASS|", 5)) reward++; else if (!strncmp(line, "FAIL|", 5)) punish++; }
        cursor = ftell(f); fclose(f);
    }
    {
        char counts[256], wl[64]; double w = (reward + 1.0) / (reward + punish + 2.0);
        snprintf(counts, sizeof counts, "COUNTS | reward=%d | punish=%d | cursor=%ld", reward, punish, cursor); snprintf(wl, sizeof wl, "WEIGHT | %.4f", w);
        if (ci >= 0) { free(e.lines[ci]); e.lines[ci] = strdup(counts); } else if (e.n < MAXL) e.lines[e.n++] = strdup(counts);
        if (wi >= 0) { free(e.lines[wi]); e.lines[wi] = strdup(wl); } else if (e.n < MAXL) e.lines[e.n++] = strdup(wl);
        { char tmp[P + 8], ld[P]; FILE *o; snprintf(ld, sizeof ld, "%s", live); { char *sl = strrchr(ld, '/'); if (sl) { *sl = 0; mkdir(ld, 0755); } }
          snprintf(tmp, sizeof tmp, "%s.tmp", live); o = fopen(tmp, "w"); if (!o) return 1; for (int i = 0; i < e.n; i++) fprintf(o, "%s\n", e.lines[i]); fclose(o); rename(tmp, live); }
        printf("%s reward=%d punish=%d weight=%.4f\n", e.id, reward, punish, w);
    }
    free_entry(&e); return 0;
}

typedef struct { double score; char id[128], file[P]; } Hit;
static int hit_cmp(const void *a, const void *b) { double d = ((const Hit *)b)->score - ((const Hit *)a)->score; return d > 0 ? 1 : d < 0 ? -1 : strcmp(((const Hit *)a)->id, ((const Hit *)b)->id); }
static int has_word(const char *hay, const char *w) {   /* whole-word, case-insensitive */
    char h[P]; snprintf(h, sizeof h, "%s", hay); lower(h);
    size_t wl = strlen(w); for (char *p = h; (p = strstr(p, w)); p++) { int l = (p == h) || !isalnum((unsigned char)p[-1]); int r = !isalnum((unsigned char)p[wl]); if (l && r) return 1; }
    return 0;
}
static int cmd_find(const char *dir, int nq, char **q) {
    DIR *d = opendir(dir); struct dirent *de; static Hit hits[256]; int nh = 0;
    if (!d) return 2;
    while ((de = readdir(d)) && nh < 256) {
        size_t l = strlen(de->d_name); char path[P + 300]; Entry e; double s = 0;
        if (l < 14 || strcmp(de->d_name + l - 13, ".behavior.pdl")) continue;
        snprintf(path, sizeof path, "%s/live/%s", dir, de->d_name);   /* live counts win over the seed */
        if (load(path, &e) != 0) { snprintf(path, sizeof path, "%s/%s", dir, de->d_name); if (load(path, &e) != 0) continue; }
        for (int qi = 0; qi < nq; qi++) {
            char w[128]; double best = 0; snprintf(w, sizeof w, "%s", q[qi]); lower(w);
            for (int i = 0; i < e.n; i++) {
                char row[P], *a, *b, *c; snprintf(row, sizeof row, "%s", e.lines[i]);
                a = strchr(row, '|'); if (!a) continue; *a++ = 0; b = strchr(a, '|'); if (b) *b++ = 0; c = b ? strchr(b, '|') : NULL; if (c) *c++ = 0;
                trim(row); a = trim(a);
                if (!strcmp(row, "KEYWORDS")) { char k[P]; snprintf(k, sizeof k, "%s", a); for (char *t = strtok(k, ","); t; t = strtok(NULL, ",")) { char *x = trim(t); char xl[128]; snprintf(xl, sizeof xl, "%s", x); lower(xl); if (!strcmp(xl, w) && best < 1.0) best = 1.0; } }
                else if (!strcmp(row, "SYNONYM") && b) { char xl[128]; snprintf(xl, sizeof xl, "%s", a); lower(xl); if (!strcmp(xl, w)) { double sw = atof(b); if (sw > best) best = sw; } }
                else if (!strcmp(row, "SLOT") && b && c) { char *m = strstr(c, "POINTS_TO="); char *wt = strstr(c, "WEIGHT="); if (!m && b) m = strstr(b, "POINTS_TO="); if (!wt) wt = strstr(b, "WEIGHT="); if (m && wt) { char mn[128]; snprintf(mn, sizeof mn, "%s", m + 10); mn[strcspn(mn, " |")] = 0; lower(mn); if (has_word(mn, w) || !strcmp(mn, w)) { double sw = 0.6 * atof(wt + 7); if (sw > best) best = sw; } } }
                else if (!strcmp(row, "SENTENCE")) { if (has_word(a, w) && best < 0.25) best = 0.25; }
            }
            s += best;
        }
        if (s > 0) { hits[nh].score = s * e.weight; snprintf(hits[nh].id, sizeof hits[nh].id, "%s", e.id); snprintf(hits[nh].file, sizeof hits[nh].file, "%s", de->d_name); nh++; }
        free_entry(&e);
    }
    closedir(d); qsort(hits, (size_t)nh, sizeof(Hit), hit_cmp);
    for (int i = 0; i < nh; i++) printf("%.3f|%s|%s\n", hits[i].score, hits[i].id, hits[i].file);
    return 0;
}
int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "update") && strcmp(argv[1], "find")) return cmd_update(argv[1]);   /* one argument = update (a pal's exec passes only one literal) */
    if (argc >= 3 && !strcmp(argv[1], "update")) return cmd_update(argv[2]);
    if (argc >= 4 && !strcmp(argv[1], "find")) return cmd_find(argv[2], argc - 3, argv + 3);
    fprintf(stderr, "usage: harness_bank_op update <cases.pdl> | find <bank_dir> <word>...\n"); return 2;
}
