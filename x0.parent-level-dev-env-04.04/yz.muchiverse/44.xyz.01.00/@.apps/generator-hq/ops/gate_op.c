/* gate_op - the attention gate over materials, D35 step M3 (BOOK 16.game/MATERIAL-ATTENTION-AND-EXPERT-MIX-GENERATOR-DESIGN-2026-11-05.md).
 *   gate_op gate  <concepts.pdl> <hidden_layer.+x> <words.txt> <approved.txt> <user.pdl|-> <temp> <phrase words...>
 *   gate_op check <concepts.pdl>                                       GATECHECK materials=N wordaff=M ; exit 2 on any bad row
 * The phrase goes through hidden_layer (HL_KEYS = the nine look dimensions) to get the query vector q (the learned word table, reused, not re-implemented).
 * Score of material m:   s_m = (q . k_m) / sqrt(9) + b_m + WORD_GAIN * sum over phrase words of WORDAFF(word, m)
 * Gate:                  g = softmax(s / temp)      (temp in 0.02..2; small = decisive)        Output is deterministic: same words, same numbers.
 * The user layer (studio state/studio_bias.pdl) may carry  KEY | material | c1,..c9   BIAS | material | b   WORDAFF | word | material | a   and OVERRIDES / ADDS to the shared file; the shared file is never written.
 * Reads files, runs hidden_layer once (fork+exec+pipe, 5 s alarm), writes stdout. Exit 0 ok, 2 usage / bad data / hidden_layer failed. Build: gcc -std=gnu11 -O2 -Wall -Wextra -o gate_op.+x gate_op.c -lm */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#define ND 9
#define MAXM 64
#define MAXA 512
#define WORD_GAIN 0.5
typedef struct { char name[40]; double k[ND], b; int have_k; } Mat;
typedef struct { char word[32], mat[40]; double a; } Aff;
static Mat M[MAXM]; static int nM; static Aff A[MAXA]; static int nA;
static const char *DIMS = "warm,wet,wear,grain,edge,round,metal,big,worn";
static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; return s; }
static int split(char *line, char **f, int max) { int n = 0; char *p = line; while (n < max) { f[n++] = p; p = strchr(p, '|'); if (!p) break; *p++ = 0; } for (int i = 0; i < n; i++) f[i] = trim(f[i]); return n; }
static int find_mat(const char *n) { for (int i = 0; i < nM; i++) if (!strcmp(M[i].name, n)) return i; return -1; }
/* layer 0 = shared file (strict: duplicates are errors), layer 1 = user overrides (KEY replaces, BIAS sets, WORDAFF replaces or adds; the material must already exist) */
static int load(const char *path, int user) {
    FILE *f = fopen(path, "r"); char ln[1024], *fl[8]; int rc = 0, line = 0;
    if (!f) { if (user) return 0; fprintf(stderr, "cannot read %s\n", path); return 2; }
    while (fgets(ln, sizeof ln, f)) {
        int nf; line++;
        if (!strncmp(ln, "KEY |", 5)) {
            double v[ND]; int i = 0, mi; char *c, *sv = NULL; nf = split(ln, fl, 8);
            if (nf < 3) { fprintf(stderr, "%s:%d bad KEY row\n", path, line); rc = 2; continue; }
            for (c = strtok_r(fl[2], ",", &sv); c && i < ND; c = strtok_r(NULL, ",", &sv)) { v[i] = atof(c); if (v[i] < -1 || v[i] > 1) { fprintf(stderr, "%s:%d value %s outside -1..1\n", path, line, c); rc = 2; } i++; }
            if (i != ND || c) { fprintf(stderr, "%s:%d KEY %s needs exactly %d numbers\n", path, line, fl[1], ND); rc = 2; continue; }
            mi = find_mat(fl[1]);
            if (mi >= 0 && !user) { fprintf(stderr, "%s:%d duplicate KEY %s\n", path, line, fl[1]); rc = 2; continue; }
            if (mi < 0) { if (user) { fprintf(stderr, "%s:%d user KEY for unknown material %s\n", path, line, fl[1]); rc = 2; continue; } if (nM >= MAXM || strlen(fl[1]) >= sizeof M[0].name) { rc = 2; continue; } mi = nM++; memset(&M[mi], 0, sizeof M[mi]); snprintf(M[mi].name, sizeof M[mi].name, "%s", fl[1]); }
            memcpy(M[mi].k, v, sizeof v); M[mi].have_k = 1;
        } else if (!strncmp(ln, "BIAS |", 6)) {
            int mi; nf = split(ln, fl, 8); if (nf < 3 || (mi = find_mat(fl[1])) < 0) { fprintf(stderr, "%s:%d BIAS for unknown material\n", path, line); rc = 2; continue; }
            M[mi].b = atof(fl[2]); if (fabs(M[mi].b) > 2) { fprintf(stderr, "%s:%d BIAS outside -2..2\n", path, line); rc = 2; }
        } else if (!strncmp(ln, "WORDAFF |", 9)) {
            int i, hit = -1; double a; nf = split(ln, fl, 8);
            if (nf < 4 || find_mat(fl[2]) < 0 || strlen(fl[1]) >= sizeof A[0].word) { fprintf(stderr, "%s:%d bad WORDAFF row\n", path, line); rc = 2; continue; }
            a = atof(fl[3]); if (a < 0 || a > 1) { fprintf(stderr, "%s:%d affinity outside 0..1\n", path, line); rc = 2; continue; }
            for (i = 0; i < nA; i++) if (!strcmp(A[i].word, fl[1]) && !strcmp(A[i].mat, fl[2])) hit = i;
            if (hit >= 0 && !user) { fprintf(stderr, "%s:%d duplicate WORDAFF %s %s\n", path, line, fl[1], fl[2]); rc = 2; continue; }
            if (hit < 0) { if (nA >= MAXA) { rc = 2; continue; } hit = nA++; snprintf(A[hit].word, sizeof A[hit].word, "%s", fl[1]); snprintf(A[hit].mat, sizeof A[hit].mat, "%s", fl[2]); }
            A[hit].a = a;
        }
    }
    fclose(f); return rc;
}
static int query(const char *hl, const char *words, const char *appr, const char *phrase, double q[ND], char *src, size_t ssz, int *known, int *nwords) {
    int fd[2], st; pid_t pid; char buf[2048]; ssize_t n, got = 0; char *p;
    if (pipe(fd)) return 2;
    pid = fork(); if (pid < 0) return 2;
    if (!pid) { dup2(fd[1], 1); close(fd[0]); close(fd[1]); setenv("HL_KEYS", DIMS, 1); alarm(5); execl(hl, hl, "forward", words, appr, phrase, (char *)NULL); _exit(127); }
    close(fd[1]);
    while ((n = read(fd[0], buf + got, sizeof buf - 1 - (size_t)got)) > 0) { got += n; }
    buf[got] = 0; close(fd[0]); waitpid(pid, &st, 0);
    if (!WIFEXITED(st) || WEXITSTATUS(st) || !(p = strstr(buf, "HIDDEN |"))) { fprintf(stderr, "hidden_layer forward failed\n"); return 2; }
    { static const char *K[ND] = { "warm=", "wet=", "wear=", "grain=", "edge=", "round=", "metal=", "big=", "worn=" }; int i; for (i = 0; i < ND; i++) { const char *k = strstr(p, K[i]); if (!k) return 2; q[i] = atof(k + strlen(K[i])); } }
    { const char *s = strstr(p, "source="); size_t i = 0; if (s) { s += 7; while (s[i] && s[i] != ' ' && s[i] != '\n' && i + 1 < ssz) { src[i] = s[i]; i++; } } src[i] = 0; }
    { const char *k = strstr(p, "known="), *w = strstr(p, "words="); *known = k ? atoi(k + 6) : 0; *nwords = w ? atoi(w + 6) : 0; }
    return 0;
}
int main(int argc, char **argv) {
    if (argc >= 5 && !strcmp(argv[1], "key")) {    /* key <concepts.pdl> <user.pdl|-> <material>: the effective key vector and bias (what the studio edits) */
        int rc = load(argv[2], 0), mi, i; if (rc) return rc;
        if (strcmp(argv[3], "-") && (rc = load(argv[3], 1))) return rc;
        if ((mi = find_mat(argv[4])) < 0) { fprintf(stderr, "unknown material %s\n", argv[4]); return 2; }
        printf("KEYVEC %s ", M[mi].name); for (i = 0; i < ND; i++) printf("%s%.2f", i ? "," : "", M[mi].k[i]); printf(" bias=%.2f\n", M[mi].b); return 0;
    }
    if (argc >= 3 && !strcmp(argv[1], "check")) { int rc = load(argv[2], 0); if (!rc) printf("GATECHECK materials=%d wordaff=%d\n", nM, nA); return rc; }
    if (argc < 8 || strcmp(argv[1], "gate")) { fprintf(stderr, "usage: gate_op gate <concepts.pdl> <hidden_layer> <words.txt> <approved.txt> <user.pdl|-> <temp> <words...>\n       gate_op check <concepts.pdl>\n"); return 2; }
    {
        double q[ND], s[MAXM], g[MAXM], temp = 0, mx = -1e9, sum = 0; char phrase[1024] = "", src[16] = "", *w, nt[1024]; int known = 0, nwords = 0, i, j, rc, order[MAXM];
        temp = atof(argv[7]); if (temp < 0.02 || temp > 2) { fprintf(stderr, "temp must be 0.02..2\n"); return 2; }
        for (i = 8; i < argc; i++) { size_t u = strlen(phrase); snprintf(phrase + u, sizeof phrase - u, "%s%s", i > 8 ? " " : "", argv[i]); }
        if ((rc = load(argv[2], 0))) return rc;
        if (strcmp(argv[6], "-") && (rc = load(argv[6], 1))) return rc;
        if ((rc = query(argv[3], argv[4], argv[5], phrase, q, src, sizeof src, &known, &nwords))) return rc;
        printf("QUERY source=%s known=%d words=%d |", src, known, nwords); for (i = 0; i < ND; i++) printf(" %.2f", q[i]); printf("\n");
        for (i = 0; i < nM; i++) { double d = 0; for (j = 0; j < ND; j++) d += q[j] * M[i].k[j]; s[i] = d / sqrt((double)ND) + M[i].b; }
        { size_t o = 0; for (j = 0; phrase[j] && o + 2 < sizeof nt; j++) nt[o++] = isalpha((unsigned char)phrase[j]) ? (char)tolower((unsigned char)phrase[j]) : ' '; nt[o] = 0; }
        { char *sv = NULL; for (w = strtok_r(nt, " ", &sv); w; w = strtok_r(NULL, " ", &sv)) { char alt[32]; size_t l = strlen(w); snprintf(alt, sizeof alt, "%s", w); if (l > 3 && alt[l - 1] == 's') alt[l - 1] = 0;
            for (j = 0; j < nA; j++) if (!strcmp(A[j].word, w) || !strcmp(A[j].word, alt)) { int mi = find_mat(A[j].mat); if (mi >= 0) s[mi] += WORD_GAIN * A[j].a; } } }
        for (i = 0; i < nM; i++) if (s[i] > mx) mx = s[i];
        for (i = 0; i < nM; i++) { g[i] = exp((s[i] - mx) / temp); sum += g[i]; } for (i = 0; i < nM; i++) g[i] /= sum;
        for (i = 0; i < nM; i++) order[i] = i;
        for (i = 0; i < nM; i++) for (j = i + 1; j < nM; j++) if (g[order[j]] > g[order[i]] || (g[order[j]] == g[order[i]] && strcmp(M[order[j]].name, M[order[i]].name) < 0)) { int t = order[i]; order[i] = order[j]; order[j] = t; }
        for (i = 0; i < 5 && i < nM; i++) printf("GATE %d %s score=%.3f gate=%.3f\n", i + 1, M[order[i]].name, s[order[i]], g[order[i]]);
        { double t2 = g[order[0]] + (nM > 1 ? g[order[1]] : 0); printf("TOP2 %s=%.3f %s=%.3f sum=%.3f\n", M[order[0]].name, g[order[0]] / t2, nM > 1 ? M[order[1]].name : "-", nM > 1 ? g[order[1]] / t2 : 0.0, 1.0); }
        { double tot = 0; for (i = 0; i < nM; i++) tot += g[i]; printf("GATESUM %.3f\n", tot); }
    }
    return 0;
}
