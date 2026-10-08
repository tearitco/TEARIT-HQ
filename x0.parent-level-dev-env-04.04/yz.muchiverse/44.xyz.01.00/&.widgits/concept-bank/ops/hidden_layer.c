/* hidden_layer - the first real HIDDEN LAYER of the house: text -> a 3-vector over the masters (energy, force, motion), built only from REVIEWED data.
 *
 *   hidden_layer apply   <draft.txt> <review.txt> <approved_out.txt>
 *       draft:   `ASSOC | n | phrase | energy=a | force=b | motion=c | ...`   review: `REVIEW | n | accept|adjust|reject | [energy= force= motion=] | by=<who> | why`
 *       Every draft row needs a review (the LAST review row for an n wins); accept keeps the draft weights, adjust takes the review's three weights (all in [-1,1]), reject drops the row.
 *       Writes `APPROVED | n | phrase | energy= | force= | motion= | by=<who> | basis=accept|adjust`. Nothing is ever approved without a review row.
 *   hidden_layer train   <approved.txt> <words_out.txt>
 *       The learned input->hidden table: for every word (lower-case letters, 2+ long) the MEAN vector of the approved phrases containing it. `WORD | w | n=<phrases> | energy= | force= | motion=` sorted by word.
 *       A word found in more than a fifth of the phrases ("the", "a") carries no signal: its row gets `stop=1` and forward ignores it. Deterministic counting, no random start,
 *       no gradient: the weights are hand-reviewed averages ("hand-trained").
 *   hidden_layer forward <words.txt> <approved.txt> <text...>
 *       Prints `HIDDEN | energy= | force= | motion= | source=exact|words|none | known=<k> | words=<m>`. exact = the text IS an approved phrase (letters-only, case-insensitive);
 *       words = mean of the known words' vectors; none = no known word (all zeros).
 * Exit 0 ok | 2 usage / unreadable | 3 review incomplete or invalid. All paths from argv. Build: gcc -std=gnu11 -Wall -Wextra -Werror -O2 -o hidden_layer.+x hidden_layer.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#define MAXR 1024
#define MAXW 8192
#define LN 2048
typedef struct { int n; char phrase[256]; double v[3]; int have; char by[64]; char basis[16]; int act; } Row;
static Row R[MAXR]; static int nR = 0;
static const char *KEYS[3] = { "energy=", "force=", "motion=" };
static int col(const char *row, int k, char *out, size_t sz) {      /* trimmed k-th pipe column */
    const char *p = row; for (int i = 0; i < k; i++) { p = strchr(p, '|'); if (!p) return 0; p++; }
    while (*p == ' ') p++;
    size_t n = 0; while (p[n] && p[n] != '|' && p[n] != '\n' && p[n] != '\r') n++;
    while (n && p[n - 1] == ' ') n--;
    if (n >= sz) n = sz - 1;
    memcpy(out, p, n); out[n] = 0; return 1;
}
static int weights_of(const char *row, double v[3]) {                 /* all three key=value present and in [-1,1] */
    for (int i = 0; i < 3; i++) {
        const char *p = strstr(row, KEYS[i]); char *e;
        if (!p) return 0;
        v[i] = strtod(p + strlen(KEYS[i]), &e);
        if (e == p + strlen(KEYS[i]) || v[i] < -1.0 || v[i] > 1.0) return 0;
    }
    return 1;
}
static void norm(const char *in, char *out, size_t sz) {              /* lower-case letters only, words joined by one space */
    size_t o = 0; int sp = 1;
    for (; *in && o + 2 < sz; in++) {
        if (isalpha((unsigned char)*in)) { out[o++] = (char)tolower((unsigned char)*in); sp = 0; }
        else if (!sp) { out[o++] = ' '; sp = 1; }
    }
    if (o && out[o - 1] == ' ') o--;
    out[o] = 0;
}
static int cmp_s(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }
int main(int argc, char **argv) {
    char ln[LN];
    if (argc >= 5 && !strcmp(argv[1], "apply")) {
        FILE *d = fopen(argv[2], "r"), *rv = fopen(argv[3], "r");
        if (!d || !rv) { fprintf(stderr, "cannot read draft/review\n"); return 2; }
        while (fgets(ln, sizeof ln, d)) if (!strncmp(ln, "ASSOC |", 7) && nR < MAXR) {
            char c[256]; Row *r = &R[nR]; memset(r, 0, sizeof *r);
            if (!col(ln, 1, c, sizeof c) || !col(ln, 2, r->phrase, sizeof r->phrase) || !weights_of(ln, r->v)) { fprintf(stderr, "bad draft row: %.60s\n", ln); return 3; }
            r->n = atoi(c); nR++;
        }
        fclose(d);
        while (fgets(ln, sizeof ln, rv)) if (!strncmp(ln, "REVIEW |", 8)) {
            char c[256], verdict[32], who[64] = "?"; int n, i, hit = -1;
            if (!col(ln, 1, c, sizeof c) || !col(ln, 2, verdict, sizeof verdict)) continue;
            n = atoi(c); for (i = 0; i < nR; i++) if (R[i].n == n) hit = i;
            if (hit < 0) { fprintf(stderr, "review for unknown row %d\n", n); return 3; }
            const char *by = strstr(ln, "by="); if (by) { size_t k = 0; by += 3; while (by[k] && by[k] != ' ' && by[k] != '|' && by[k] != '\n' && k < 63) { who[k] = by[k]; k++; } who[k] = 0; }
            snprintf(R[hit].by, sizeof R[hit].by, "%s", who);
            if (!strcmp(verdict, "accept")) { R[hit].act = 1; snprintf(R[hit].basis, sizeof R[hit].basis, "accept"); }
            else if (!strcmp(verdict, "reject")) { R[hit].act = 2; }
            else if (!strcmp(verdict, "adjust")) {
                double w[3]; if (!weights_of(ln, w)) { fprintf(stderr, "adjust for row %d needs energy= force= motion= in [-1,1]\n", n); return 3; }
                R[hit].act = 3; memcpy(R[hit].v, w, sizeof w); snprintf(R[hit].basis, sizeof R[hit].basis, "adjust");
            } else { fprintf(stderr, "unknown verdict '%s' for row %d\n", verdict, n); return 3; }
        }
        fclose(rv);
        int missing = 0, ok = 0, rej = 0, adj = 0;
        for (int i = 0; i < nR; i++) if (!R[i].act) { missing++; fprintf(stderr, "row %d has no review\n", R[i].n); }
        if (missing) return 3;
        FILE *o = fopen(argv[4], "w"); if (!o) { fprintf(stderr, "cannot write %s\n", argv[4]); return 2; }
        fprintf(o, "# APPROVED | n | phrase | energy | force | motion | by | basis   (only rows a reviewer accepted or adjusted; built by hidden_layer apply)\n");
        for (int i = 0; i < nR; i++) {
            if (R[i].act == 2) { rej++; continue; }
            if (R[i].act == 3) adj++;
            ok++; fprintf(o, "APPROVED | %d | %s | energy=%.2f | force=%.2f | motion=%.2f | by=%s | basis=%s\n", R[i].n, R[i].phrase, R[i].v[0], R[i].v[1], R[i].v[2], R[i].by, R[i].basis);
        }
        fclose(o); printf("approved=%d adjusted=%d rejected=%d\n", ok, adj, rej); return 0;
    }
    if (argc == 4 && !strcmp(argv[1], "train")) {
        FILE *f = fopen(argv[2], "r"); if (!f) { fprintf(stderr, "cannot read %s\n", argv[2]); return 2; }
        static char *W[MAXW]; static double S[MAXW][3]; static int C[MAXW]; int nW = 0, nP = 0;
        while (fgets(ln, sizeof ln, f)) if (!strncmp(ln, "APPROVED |", 10)) {
            char ph[256], nm[300], *seen[64]; int ns = 0; double v[3];
            if (!col(ln, 2, ph, sizeof ph) || !weights_of(ln, v)) continue;
            nP++; norm(ph, nm, sizeof nm);
            for (char *t = strtok(nm, " "); t; t = strtok(NULL, " ")) {
                if (strlen(t) < 2) continue;
                int dup = 0; for (int i = 0; i < ns; i++) if (!strcmp(seen[i], t)) dup = 1;
                if (dup || ns >= 64) continue;
                char *keep = strdup(t); seen[ns++] = keep;
                int k = -1; for (int i = 0; i < nW; i++) if (!strcmp(W[i], keep)) { k = i; break; }
                if (k < 0 && nW < MAXW) { k = nW++; W[k] = strdup(keep); }
                if (k >= 0) { for (int j = 0; j < 3; j++) S[k][j] += v[j]; C[k]++; }
            }
        }
        fclose(f);
        int *idx = malloc(sizeof(int) * (size_t)(nW ? nW : 1)); char **names = malloc(sizeof(char *) * (size_t)(nW ? nW : 1));
        for (int i = 0; i < nW; i++) names[i] = W[i];
        qsort(names, (size_t)nW, sizeof(char *), cmp_s);
        FILE *o = fopen(argv[3], "w"); if (!o) { fprintf(stderr, "cannot write %s\n", argv[3]); return 2; }
        fprintf(o, "# WORD | word | n | energy | force | motion   (mean of the approved phrases that contain the word; stop=1 = filler, ignored by forward; built by hidden_layer train)\n");
        for (int i = 0; i < nW; i++) { int k = 0; for (int j = 0; j < nW; j++) if (!strcmp(W[j], names[i])) { k = j; break; }
            fprintf(o, "WORD | %s | n=%d | energy=%.2f | force=%.2f | motion=%.2f | stop=%d\n", names[i], C[k], S[k][0] / C[k], S[k][1] / C[k], S[k][2] / C[k], C[k] * 5 > nP ? 1 : 0); }
        fclose(o); free(idx); free(names); printf("words=%d\n", nW); return 0;
    }
    if (argc >= 5 && !strcmp(argv[1], "forward")) {
        char text[LN] = ""; for (int i = 4; i < argc; i++) { size_t u = strlen(text); snprintf(text + u, sizeof text - u, "%s%s", i > 4 ? " " : "", argv[i]); }
        char nt[LN]; norm(text, nt, sizeof nt);
        FILE *a = fopen(argv[3], "r"); if (a) {
            while (fgets(ln, sizeof ln, a)) if (!strncmp(ln, "APPROVED |", 10)) {
                char ph[256], np[300]; double v[3];
                if (!col(ln, 2, ph, sizeof ph) || !weights_of(ln, v)) continue;
                norm(ph, np, sizeof np);
                if (!strcmp(np, nt)) { printf("HIDDEN | energy=%.2f | force=%.2f | motion=%.2f | source=exact | known=0 | words=0\n", v[0], v[1], v[2]); fclose(a); return 0; }
            }
            fclose(a);
        }
        FILE *w = fopen(argv[2], "r"); if (!w) { fprintf(stderr, "cannot read %s\n", argv[2]); return 2; }
        double sum[3] = { 0, 0, 0 }; int known = 0, words = 0;
        for (char *t = strtok(nt, " "); t; t = strtok(NULL, " ")) {
            if (strlen(t) < 2) continue;
            words++; rewind(w);
            while (fgets(ln, sizeof ln, w)) if (!strncmp(ln, "WORD |", 6)) {
                char name[64]; double v[3]; if (!col(ln, 1, name, sizeof name) || strcmp(name, t) || !weights_of(ln, v)) continue;
                if (strstr(ln, "stop=1")) { words--; break; }       /* filler word: ignored */
                for (int j = 0; j < 3; j++) sum[j] += v[j];
                known++; break;
            }
        }
        fclose(w);
        if (known) for (int j = 0; j < 3; j++) sum[j] /= known;
        printf("HIDDEN | energy=%.2f | force=%.2f | motion=%.2f | source=%s | known=%d | words=%d\n", sum[0], sum[1], sum[2], known ? "words" : "none", known, words);
        return 0;
    }
    fprintf(stderr, "usage: hidden_layer apply <draft> <review> <approved_out> | train <approved> <words_out> | forward <words> <approved> <text...>\n");
    return 2;
}
