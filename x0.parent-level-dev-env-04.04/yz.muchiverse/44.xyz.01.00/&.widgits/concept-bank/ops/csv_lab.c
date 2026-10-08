/* csv_lab - reusable CSV-in / CSV-out model pipeline (first use: Simplified Chinese + pinyin for every concept bank, scored by a second model).
 * Design: #.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/CSV-MODEL-SCORING-AND-CHINESE-PIPELINE-DESIGN.md
 *
 * One self-contained C file, many verbs (the ops of the design: csv_jobs, csv_batch, csv_lint, csv_merge, the model-call wrapper, the driver). No header, no link split.
 * Source banks are NEVER written: results go to a sidecar (<stem>.zh.tsv). Model scores are advisory: low scores land in review.tsv, a person decides.
 *
 *   csv_lab jobs   <banks.pdl> <bank> <workdir> <answers.tsv> [--root DIR] [--lang zh] [--limit N]
 *        -> <workdir>/all.tsv (every job) jobs.tsv (untranslated) hits.tsv (answered by the answer bank, keyed by sha256(source text + lang))
 *   csv_lab batch  <jobs.tsv> <outdir> [--tokens N] [--rows N]          token-budgeted batches b001.tsv.. (n, jobid, src, ctx)
 *   csv_lab prompt translate|score <batch-or-rows.tsv> <out_prompt.txt>
 *   csv_lab parse  translate|score <reply.txt> <batch.tsv> <out.tsv>    model reply -> jobid, zh, pinyin | jobid, score, reason (rows the reply lacks are simply absent)
 *   csv_lab lint   <trans.tsv> <out.tsv> [--ref pointer.pdl] [--expect jobs.tsv]    deterministic checks (CJK, pinyin tone marks + syllable shape + count, length, row numbering, ref cross-check)
 *   csv_lab call   groq|mac <prompt.txt> <reply.txt> [--model M] [--timeout S] [--ledger quota.txt] [--key-dir D] [--max-tokens N] [--house H]
 *   csv_lab quota  <ledger> <now_epoch> <est_tokens> [--day-limit 1000] [--tpm 7000]
 *   csv_lab merge  <banks.pdl> <bank> <workdir> <answers.tsv> [--root DIR] [--threshold 4] [--feedback-dir D] [--quota-ledger F] [--out sidecar] [--translator T] [--judge J]
 *   csv_lab ui     <workdir> [--bank B] [--quota-ledger F]               (re)writes <workdir>/csv_lab_ui.txt
 *   csv_lab pipeline <banks.pdl> <bank> --work DIR --answers FILE [options]   extract -> batch -> translate -> lint -> repair -> score -> merge
 * Build: gcc -std=gnu11 -Wall -Wextra -Werror -Wno-format-truncation -O2 -o csv_lab.+x csv_lab.c
 * Exit: 0 ok | 2 usage / unreadable input | 3 quota / limit | 4 call failed | 5 empty reply | 6 timeout | 7 manifest problem. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <glob.h>
#include <poll.h>
#include <signal.h>
#include <netdb.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <limits.h>
#include <stdarg.h>

#define LN 16384
#define PB 4096

/* ------------------------------------------------------------------ small utils */
static char *slurp(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb"); long n; char *b;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); n = ftell(f); rewind(f);
    if (n < 0) { fclose(f); return NULL; }
    b = malloc((size_t)n + 1); if (!b) { fclose(f); return NULL; }
    n = (long)fread(b, 1, (size_t)n, f); b[n] = 0; fclose(f);
    if (len) *len = (size_t)n;
    return b;
}
static char *trim(char *s) {
    char *e; while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
    e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0;
    return s;
}
static void clean_cell(char *s) { for (; *s; s++) if (*s == '\t' || *s == '\n' || *s == '\r') *s = ' '; }  /* one TSV cell = one line, no tabs */
static int argval(int argc, char **argv, const char *name, const char **out) {
    for (int i = 0; i < argc - 1; i++) if (!strcmp(argv[i], name)) { *out = argv[i + 1]; return 1; }
    return 0;
}
static long argnum(int argc, char **argv, const char *name, long dflt) { const char *v; return argval(argc, argv, name, &v) ? atol(v) : dflt; }
static int mkdirs(const char *path) {
    char tmp[PB]; snprintf(tmp, sizeof tmp, "%s", path);
    for (char *p = tmp + 1; *p; p++) if (*p == '/') { *p = 0; mkdir(tmp, 0775); *p = '/'; }
    return mkdir(tmp, 0775) && errno != EEXIST ? -1 : 0;
}
static void dir_of(const char *path, char *out, size_t n) { snprintf(out, n, "%s", path); char *s = strrchr(out, '/'); if (s) *s = 0; else snprintf(out, n, "."); }
static long long now_s(void) { return (long long)time(NULL); }
static long long mono_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (long long)t.tv_sec * 1000 + t.tv_nsec / 1000000; }

/* ------------------------------------------------------------------ sha256 */
typedef struct { unsigned st[8]; unsigned long long len; unsigned char buf[64]; int bl; } Sha;
static const unsigned K256[64] = {
0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2 };
#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
static void sha_blk(Sha *s, const unsigned char *p) {
    unsigned w[64], a, b, c, d, e, f, g, h;
    for (int i = 0; i < 16; i++) w[i] = (unsigned)p[i*4] << 24 | (unsigned)p[i*4+1] << 16 | (unsigned)p[i*4+2] << 8 | p[i*4+3];
    for (int i = 16; i < 64; i++) { unsigned s0 = ROR(w[i-15], 7) ^ ROR(w[i-15], 18) ^ (w[i-15] >> 3), s1 = ROR(w[i-2], 17) ^ ROR(w[i-2], 19) ^ (w[i-2] >> 10); w[i] = w[i-16] + s0 + w[i-7] + s1; }
    a = s->st[0]; b = s->st[1]; c = s->st[2]; d = s->st[3]; e = s->st[4]; f = s->st[5]; g = s->st[6]; h = s->st[7];
    for (int i = 0; i < 64; i++) {
        unsigned t1 = h + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + K256[i] + w[i];
        unsigned t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    s->st[0] += a; s->st[1] += b; s->st[2] += c; s->st[3] += d; s->st[4] += e; s->st[5] += f; s->st[6] += g; s->st[7] += h;
}
static void sha_init(Sha *s) { static const unsigned iv[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19}; memcpy(s->st, iv, sizeof iv); s->len = 0; s->bl = 0; }
static void sha_add(Sha *s, const void *d, size_t n) { const unsigned char *p = d; while (n--) { s->buf[s->bl++] = *p++; s->len++; if (s->bl == 64) { sha_blk(s, s->buf); s->bl = 0; } } }
static void sha_hex(Sha *s, char out[65]) {
    unsigned long long bits = s->len * 8; unsigned char pad = 0x80, z = 0;
    sha_add(s, &pad, 1); while (s->bl != 56) sha_add(s, &z, 1);
    for (int i = 7; i >= 0; i--) { unsigned char b = (unsigned char)(bits >> (i * 8)); sha_add(s, &b, 1); }
    for (int i = 0; i < 8; i++) snprintf(out + i * 8, 9, "%08x", s->st[i]);
}
/* key of the answer bank: sha256(source text, 0x1f, target language) */
static void answer_key(const char *src, const char *lang, char out[65]) { Sha s; sha_init(&s); sha_add(&s, src, strlen(src)); sha_add(&s, "\x1f", 1); sha_add(&s, lang, strlen(lang)); sha_hex(&s, out); }

/* ------------------------------------------------------------------ UTF-8 */
static int u8dec(const char **pp, unsigned *cp) {   /* returns 0 at end; invalid bytes decode as U+FFFD, one byte each */
    const unsigned char *p = (const unsigned char *)*pp; unsigned c = *p;
    if (!c) return 0;
    if (c < 0x80) { *cp = c; *pp += 1; return 1; }
    if ((c & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) { *cp = (c & 0x1F) << 6 | (p[1] & 0x3F); *pp += 2; return 1; }
    if ((c & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) { *cp = (c & 0x0F) << 12 | (p[1] & 0x3F) << 6 | (p[2] & 0x3F); *pp += 3; return 1; }
    if ((c & 0xF8) == 0xF0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 && (p[3] & 0xC0) == 0x80) { *cp = (c & 7) << 18 | (p[1] & 0x3F) << 12 | (p[2] & 0x3F) << 6 | (p[3] & 0x3F); *pp += 4; return 1; }
    *cp = 0xFFFD; *pp += 1; return 1;
}
static int u8enc(unsigned cp, char *o) {
    if (cp < 0x80) { o[0] = (char)cp; return 1; }
    if (cp < 0x800) { o[0] = (char)(0xC0 | cp >> 6); o[1] = (char)(0x80 | (cp & 0x3F)); return 2; }
    if (cp < 0x10000) { o[0] = (char)(0xE0 | cp >> 12); o[1] = (char)(0x80 | ((cp >> 6) & 0x3F)); o[2] = (char)(0x80 | (cp & 0x3F)); return 3; }
    o[0] = (char)(0xF0 | cp >> 18); o[1] = (char)(0x80 | ((cp >> 12) & 0x3F)); o[2] = (char)(0x80 | ((cp >> 6) & 0x3F)); o[3] = (char)(0x80 | (cp & 0x3F)); return 4;
}
static int is_cjk(unsigned c) { return (c >= 0x4E00 && c <= 0x9FFF) || (c >= 0x3400 && c <= 0x4DBF) || (c >= 0x20000 && c <= 0x2A6DF) || (c >= 0xF900 && c <= 0xFAFF); }
static int is_kana_hangul(unsigned c) { return (c >= 0x3040 && c <= 0x30FF) || (c >= 0xAC00 && c <= 0xD7AF); }
static int is_cjk_punct(unsigned c) { return (c >= 0x3000 && c <= 0x303F) || (c >= 0xFF00 && c <= 0xFFEF) || c == 0x2026 || c == 0x2014 || c == 0x00B7 || (c >= 0x2018 && c <= 0x201D); }

/* ------------------------------------------------------------------ TSV tables (records point into one buffer) */
typedef struct { char *f[8]; int n; } Rec;
typedef struct { Rec *r; int n, cap; char *buf; } Tab;
static void tab_push(Tab *t, Rec *r) { if (t->n == t->cap) { t->cap = t->cap ? t->cap * 2 : 64; t->r = realloc(t->r, (size_t)t->cap * sizeof(Rec)); } t->r[t->n++] = *r; }
static int tab_load(const char *path, Tab *t) {
    memset(t, 0, sizeof *t);
    t->buf = slurp(path, NULL); if (!t->buf) return -1;
    for (char *p = t->buf; *p; ) {
        char *e = strchr(p, '\n'); Rec r; memset(&r, 0, sizeof r);
        if (e) *e = 0;
        size_t L = strlen(p); if (L && p[L - 1] == '\r') p[L - 1] = 0;
        if (*p && *p != '#') {
            char *q = p; r.f[r.n++] = q;
            while (r.n < 8 && (q = strchr(q, '\t'))) { *q++ = 0; r.f[r.n++] = q; }
        }
        if (r.n) tab_push(t, &r);
        if (!e) break;
        p = e + 1;
    }
    return 0;
}
static const char *fld(const Rec *r, int i) { return i < r->n ? r->f[i] : ""; }
static const Rec *tab_find(const Tab *t, const char *id) { for (int i = 0; i < t->n; i++) if (!strcmp(t->r[i].f[0], id)) return &t->r[i]; return NULL; }
static void tab_free(Tab *t) { free(t->r); free(t->buf); memset(t, 0, sizeof *t); }

/* ------------------------------------------------------------------ manifest (banks.pdl): BANK | name | key=value | ... */
typedef struct {
    char name[96], file[PB], format[16], rowtag[256], key[64], text[256], ctx[256], never[256], add[64], lang[16], sidecar[PB];
    int header;
} Bank;
static int split_bar(char *line, char **out, int max) {   /* split on '|' and trim */
    int n = 0; char *p = line;
    while (n < max) { char *b = strchr(p, '|'); if (b) *b = 0; out[n++] = trim(p); if (!b) break; p = b + 1; }
    return n;
}
static const char *kv(char **f, int n, const char *key) {
    size_t kl = strlen(key);
    for (int i = 0; i < n; i++) if (!strncmp(f[i], key, kl) && f[i][kl] == '=') return f[i] + kl + 1;
    return NULL;
}
static int bank_load(const char *manifest, const char *name, Bank *b) {
    char *buf = slurp(manifest, NULL); if (!buf) return -1;
    int found = 0;
    for (char *p = buf; *p && !found; ) {
        char *e = strchr(p, '\n'); char *f[32]; int n;
        if (e) *e = 0;
        char *line = trim(p);
        if (!strncmp(line, "BANK", 4) && (n = split_bar(line, f, 32)) >= 3 && !strcmp(f[0], "BANK") && !strcmp(f[1], name)) {
            memset(b, 0, sizeof *b);
            snprintf(b->name, sizeof b->name, "%s", name);
            const char *v;
            snprintf(b->file, sizeof b->file, "%s", (v = kv(f, n, "file")) ? v : "");
            snprintf(b->format, sizeof b->format, "%s", (v = kv(f, n, "format")) ? v : "pdl");
            snprintf(b->rowtag, sizeof b->rowtag, "%s", (v = kv(f, n, "rowtag")) ? v : "");
            snprintf(b->key, sizeof b->key, "%s", (v = kv(f, n, "key")) ? v : "line");
            snprintf(b->text, sizeof b->text, "%s", (v = kv(f, n, "text")) ? v : "");
            snprintf(b->ctx, sizeof b->ctx, "%s", (v = kv(f, n, "context")) ? v : "");
            snprintf(b->never, sizeof b->never, "%s", (v = kv(f, n, "never")) ? v : "");
            snprintf(b->add, sizeof b->add, "%s", (v = kv(f, n, "add")) ? v : "zh,pinyin");
            snprintf(b->lang, sizeof b->lang, "%s", (v = kv(f, n, "lang")) ? v : "zh");
            snprintf(b->sidecar, sizeof b->sidecar, "%s", (v = kv(f, n, "sidecar")) ? v : "");
            found = 1;
        }
        if (!e) break;
        p = e + 1;
    }
    free(buf);
    return found ? 0 : -2;
}
static int in_list(const char *list, const char *item) {   /* comma list membership */
    size_t il = strlen(item); const char *p = list;
    while (*p) { const char *c = strchr(p, ','); size_t l = c ? (size_t)(c - p) : strlen(p); if (l == il && !strncmp(p, item, il)) return 1; if (!c) break; p = c + 1; }
    return 0;
}
/* The house root: --root, else $CSV_LAB_HOUSE, else walk up from the executable to the first folder that holds `xyzfs`. */
static char g_exe[PB];
static void house_root(int argc, char **argv, char *out, size_t n) {
    const char *v;
    if (argval(argc, argv, "--root", &v) || argval(argc, argv, "--house", &v) || ((v = getenv("CSV_LAB_HOUSE")) && *v)) { snprintf(out, n, "%s", v); return; }
    char d[PB]; snprintf(d, sizeof d, "%s", g_exe);
    for (int i = 0; i < 12; i++) {
        char probe[PB + 16]; char *s = strrchr(d, '/'); if (!s || s == d) break; *s = 0;
        snprintf(probe, sizeof probe, "%s/xyzfs", d);
        struct stat sb; if (!stat(probe, &sb)) { snprintf(out, n, "%s", d); return; }
    }
    snprintf(out, n, ".");
}

/* ------------------------------------------------------------------ source row readers (pdl rows or csv) */
typedef struct { char *f[48]; int n; } Row;
static int csv_parse(char *buf, Row **rows, int *nrows) {   /* RFC-4180-ish, in place; quoted cells may hold commas, quotes ("") and newlines */
    int cap = 128, n = 0; Row *R = calloc((size_t)cap, sizeof(Row)); char *p = buf;
    while (*p) {
        if (n == cap) { cap *= 2; R = realloc(R, (size_t)cap * sizeof(Row)); }
        Row *r = &R[n]; r->n = 0;
        for (;;) {
            char *w; if (r->n >= 47) { while (*p && *p != '\n') p++; break; }
            if (*p == '"') {
                p++; w = p; r->f[r->n++] = w;
                for (;;) { if (*p == '"' && p[1] == '"') { *w++ = '"'; p += 2; } else if (*p == '"') { p++; break; } else if (!*p) break; else *w++ = *p++; }
                *w = 0;
                while (*p && *p != ',' && *p != '\n') p++;
            } else {
                r->f[r->n++] = p; while (*p && *p != ',' && *p != '\n') p++;
            }
            if (*p == ',') { *p++ = 0; continue; }
            if (*p == '\n') { *p++ = 0; } break;
        }
        for (int i = 0; i < r->n; i++) { size_t L = strlen(r->f[i]); if (L && r->f[i][L - 1] == '\r') r->f[i][L - 1] = 0; }
        if (r->n == 1 && !r->f[0][0]) continue;   /* blank line */
        n++;
    }
    *rows = R; *nrows = n; return 0;
}
static int col_index(const Row *hdr, const Row *r, const char *spec) {   /* number = field index; name = header cell (csv) or `name=` field (pdl) */
    if (isdigit((unsigned char)spec[0])) return atoi(spec);
    if (hdr) { for (int i = 0; i < hdr->n; i++) if (!strcmp(hdr->f[i], spec)) return i; return -1; }
    size_t sl = strlen(spec);
    for (int i = 1; i < r->n; i++) if (!strncmp(r->f[i], spec, sl) && r->f[i][sl] == '=') return i;
    return -1;
}
static const char *col_val(const Row *hdr, const Row *r, const char *spec) {
    int i = col_index(hdr, r, spec); if (i < 0 || i >= r->n) return NULL;
    if (!hdr && !isdigit((unsigned char)spec[0])) return r->f[i] + strlen(spec) + 1;
    return r->f[i];
}
static int worth_translating(const char *s) { for (; *s; s++) if (isalpha((unsigned char)*s) || (unsigned char)*s >= 0x80) return 1; return 0; }

/* ------------------------------------------------------------------ jobs */
typedef struct { char id[256], src[2048], ctx[1024]; } Job;
static int jobs_collect(const char *root, const Bank *b, Job **out, int *nout, char *err, size_t en) {
    char pat[PB * 2]; glob_t g; int cap = 256, n = 0; Job *J = malloc((size_t)cap * sizeof(Job));
    snprintf(pat, sizeof pat, "%s/%s", root, b->file);
    if (glob(pat, 0, NULL, &g) || g.gl_pathc == 0) { snprintf(err, en, "no file matches %s", b->file); free(J); return -1; }
    int multi = g.gl_pathc > 1 || strchr(b->file, '*');
    char textcols[256]; snprintf(textcols, sizeof textcols, "%s", b->text);
    char *sv1 = NULL;
    for (char *c = strtok_r(textcols, ",", &sv1); c; c = strtok_r(NULL, ",", &sv1)) {
        if (in_list(b->never, c)) { snprintf(err, en, "text column %s is also listed in never=", c); free(J); globfree(&g); return -2; }
    }
    for (size_t gi = 0; gi < g.gl_pathc; gi++) {
        char *buf = slurp(g.gl_pathv[gi], NULL); const char *bn = strrchr(g.gl_pathv[gi], '/'); bn = bn ? bn + 1 : g.gl_pathv[gi];
        if (!buf) continue;
        Row *rows = NULL; int nr = 0, line = 0, start = 0; Row *hdr = NULL;
        if (!strcmp(b->format, "csv")) { csv_parse(buf, &rows, &nr); if (nr) { hdr = &rows[0]; start = 1; } }
        else {   /* pdl rows: one record per line, fields split on '|' */
            int lines = 0; for (char *p = buf; *p; p++) if (*p == '\n') lines++;
            rows = calloc((size_t)lines + 2, sizeof(Row));
            for (char *p = buf; *p; ) {
                char *e = strchr(p, '\n'); if (e) *e = 0;
                char *l = trim(p);
                if (*l && *l != '#') { Row *r = &rows[nr++]; r->n = split_bar(l, r->f, 48); }
                if (!e) break;
                p = e + 1;
            }
        }
        for (int ri = start; ri < nr; ri++) {
            Row *r = &rows[ri]; line++;
            if (!hdr && b->rowtag[0] && !in_list(b->rowtag, r->f[0])) continue;
            char key[160];
            if (!strcmp(b->key, "line")) snprintf(key, sizeof key, "L%d", line);
            else { const char *kvv = col_val(hdr, r, b->key); if (!kvv || !*kvv) continue; snprintf(key, sizeof key, "%.150s", kvv); }
            char tc[256]; snprintf(tc, sizeof tc, "%s", b->text);
            char *sv2 = NULL;
            for (char *col = strtok_r(tc, ",", &sv2); col; col = strtok_r(NULL, ",", &sv2)) {
                const char *v = col_val(hdr, r, col); if (!v || !*v || !worth_translating(v)) continue;
                if (n == cap) { cap *= 2; J = realloc(J, (size_t)cap * sizeof(Job)); }
                Job *j = &J[n++]; memset(j, 0, sizeof *j);
                int multicol = strchr(b->text, ',') != NULL;
                if (multi) snprintf(j->id, sizeof j->id, "%.60s:%s%s%s", bn, key, multicol ? "/" : "", multicol ? col : "");
                else snprintf(j->id, sizeof j->id, "%s%s%s", key, multicol ? "/" : "", multicol ? col : "");
                snprintf(j->src, sizeof j->src, "%s", v); clean_cell(j->src);
                char cc[256]; snprintf(cc, sizeof cc, "%s", b->ctx); size_t o = 0;
                char *sv3 = NULL;
                for (char *c2 = strtok_r(cc, ",", &sv3); c2; c2 = strtok_r(NULL, ",", &sv3)) {
                    const char *cv = col_val(hdr, r, c2); if (!cv || !*cv) continue;
                    o += (size_t)snprintf(j->ctx + o, sizeof j->ctx - o, "%s%s=%.200s", o ? "; " : "", c2, cv); if (o >= sizeof j->ctx) o = sizeof j->ctx - 1;
                }
                clean_cell(j->ctx);
            }
        }
        /* the strings live in buf; jobs copied what they need, so free is safe */
        free(rows); free(buf);
    }
    globfree(&g); *out = J; *nout = n; return 0;
}
static int v_jobs(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: csv_lab jobs <banks.pdl> <bank> <workdir> <answers.tsv> [--root DIR] [--lang zh] [--limit N]\n"); return 2; }
    char root[PB], err[256]; Bank b; Job *J; int nj;
    house_root(argc, argv, root, sizeof root);
    int rc = bank_load(argv[0], argv[1], &b);
    if (rc) { fprintf(stderr, "csv_lab jobs: bank %s not in %s\n", argv[1], argv[0]); return 7; }
    const char *lang; if (argval(argc, argv, "--lang", &lang)) snprintf(b.lang, sizeof b.lang, "%s", lang);
    if (jobs_collect(root, &b, &J, &nj, err, sizeof err)) { fprintf(stderr, "csv_lab jobs: %s\n", err); return 7; }
    long limit = argnum(argc, argv, "--limit", 0);
    Tab ans; int have = tab_load(argv[3], &ans) == 0; mkdirs(argv[2]);
    char pa[PB], pj[PB], ph[PB];
    snprintf(pa, sizeof pa, "%s/all.tsv", argv[2]); snprintf(pj, sizeof pj, "%s/jobs.tsv", argv[2]); snprintf(ph, sizeof ph, "%s/hits.tsv", argv[2]);
    FILE *fa = fopen(pa, "w"), *fj = fopen(pj, "w"), *fh = fopen(ph, "w");
    if (!fa || !fj || !fh) { fprintf(stderr, "csv_lab jobs: cannot write under %s\n", argv[2]); return 2; }
    int hits = 0, todo = 0;
    for (int i = 0; i < nj; i++) {
        char k[65]; answer_key(J[i].src, b.lang, k);
        const Rec *a = NULL;
        if (have) for (int q = 0; q < ans.n; q++) if (!strcmp(ans.r[q].f[0], k) && strcmp(fld(&ans.r[q], 6), "rejected")) { a = &ans.r[q]; break; }
        fprintf(fa, "%s\t%s\t%s\n", J[i].id, J[i].src, J[i].ctx);
        if (a) { fprintf(fh, "%s\t%s\t%s\t%s\t%s\t%s\n", J[i].id, J[i].src, fld(a, 3), fld(a, 4), fld(a, 5), fld(a, 6)); hits++; }
        else if (!limit || todo < limit) { fprintf(fj, "%s\t%s\t%s\n", J[i].id, J[i].src, J[i].ctx); todo++; }
    }
    fclose(fa); fclose(fj); fclose(fh); if (have) tab_free(&ans); free(J);
    printf("jobs=%d untranslated=%d answer_hits=%d bank=%s lang=%s\n", nj, todo, hits, b.name, b.lang);
    return 0;
}

/* ------------------------------------------------------------------ batch + prompts + parsers */
static int est_tokens(const char *s) { int t = 0; unsigned c; const char *p = s; while (u8dec(&p, &c)) t += c < 0x80 ? 0 : 2; return (int)(strlen(s) / 4) + t / 3 + 2; }   /* chars/4, CJK a bit more */
static int v_batch(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: csv_lab batch <jobs.tsv> <outdir> [--tokens N] [--rows N]\n"); return 2; }
    Tab t; if (tab_load(argv[0], &t)) { fprintf(stderr, "csv_lab batch: cannot read %s\n", argv[0]); return 2; }
    long budget = argnum(argc, argv, "--tokens", 900), maxrows = argnum(argc, argv, "--rows", 30);
    mkdirs(argv[1]);
    int nb = 0, rows = 0; long used = 0; FILE *f = NULL;
    for (int i = 0; i < t.n; i++) {
        int cost = est_tokens(fld(&t.r[i], 1)) + est_tokens(fld(&t.r[i], 2)) + 6;
        if (!f || rows >= maxrows || (rows > 0 && used + cost > budget)) {
            char p[PB]; if (f) fclose(f);
            snprintf(p, sizeof p, "%s/b%03d.tsv", argv[1], ++nb); f = fopen(p, "w");
            if (!f) { fprintf(stderr, "csv_lab batch: cannot write %s\n", p); return 2; }
            rows = 0; used = 0;
        }
        fprintf(f, "%d\t%s\t%s\t%s\n", rows + 1, t.r[i].f[0], fld(&t.r[i], 1), fld(&t.r[i], 2));
        rows++; used += cost;
    }
    if (f) fclose(f);
    printf("batches=%d rows=%d\n", nb, t.n); tab_free(&t);
    return 0;
}
static int v_prompt(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: csv_lab prompt translate|score <rows.tsv> <out_prompt.txt>\n"); return 2; }
    Tab t; if (tab_load(argv[1], &t)) { fprintf(stderr, "csv_lab prompt: cannot read %s\n", argv[1]); return 2; }
    FILE *o = fopen(argv[2], "w"); if (!o) return 2;
    if (!strcmp(argv[0], "translate")) {
        fputs("Translate each English row into Simplified Chinese (zh-Hans, Mainland usage) and give Hanyu Pinyin with tone marks over the vowels (shui3 is WRONG, shuǐ is right), syllables separated by spaces, one pinyin syllable per Chinese character.\n"
              "Keep chemical formulas, numbers, symbols and identifiers exactly as written. The third column is context only: use it to pick the sense, never translate or copy it. A line that starts with FIX: names a problem in your earlier answer; correct it.\n"
              "Reply with ONLY one line per input row, tab separated: N<TAB>Chinese<TAB>pinyin. Same numbering, no header, no commentary, no markdown.\n\nINPUT (N<TAB>English<TAB>context):\n", o);
        for (int i = 0; i < t.n; i++) fprintf(o, "%s\t%s\t%s\n", t.r[i].f[0], fld(&t.r[i], 2), fld(&t.r[i], 3));
    } else {
        fputs("You are a strict bilingual reviewer (English to Simplified Chinese). For each row rate how well the Chinese translates the English, and whether the pinyin matches the Chinese, with ONE integer from 1 to 5: 5 = correct and natural, 4 = acceptable, 3 = understandable but flawed, 2 = wrong sense or wrong pinyin, 1 = unrelated.\n"
              "Reply with ONLY one line per row, tab separated: N<TAB>score<TAB>reason of at most 8 words. Same numbering, nothing else.\n\nROWS (N<TAB>English<TAB>Chinese<TAB>pinyin):\n", o);
        for (int i = 0; i < t.n; i++) fprintf(o, "%s\t%s\t%s\t%s\n", t.r[i].f[0], fld(&t.r[i], 1), fld(&t.r[i], 2), fld(&t.r[i], 3));
    }
    fclose(o); tab_free(&t); return 0;
}
/* a reply line -> up to 4 cells; tolerates "N. a b c", "N | a | b", code fences, bullets, and runs of 2+ spaces as the separator of last resort */
static int split_reply_line(char *l, char **cells, int max) {
    l = trim(l); while (*l == '-' || *l == '*' || *l == '`') l++; l = trim(l);
    int n = 0;
    if (!isdigit((unsigned char)*l)) { cells[n++] = l; return n; }
    cells[n++] = l; while (isdigit((unsigned char)*l)) l++;      /* cell 0 = the row number */
    if (*l) { *l = 0; l++; }                                      /* the one char after the digits ('.', ')', ':', tab, space) ends the number */
    while (*l == ' ' || *l == '\t' || *l == '|') l++;            /* skip the separator after the number (tab, bar or spaces) */
    if (strchr(l, '\t')) { char *p = l; while (n < max) { char *e = strchr(p, '\t'); if (e) *e = 0; cells[n++] = trim(p); if (!e) break; p = e + 1; } return n; }
    if (strstr(l, " | ")) { char *p = l; while (n < max) { char *e = strstr(p, " | "); if (e) { *e = 0; } cells[n++] = trim(p); if (!e) break; p = e + 3; } return n; }
    if (strstr(l, "  ")) { char *p = l; while (n < max) { char *e = strstr(p, "  "); if (e) *e = 0; cells[n++] = trim(p); if (!e) break; p = e + 2; while (*p == ' ') p++; } return n; }
    { char *sp = strchr(l, ' '); if (sp) { *sp = 0; cells[n++] = trim(l); cells[n++] = trim(sp + 1); } else cells[n++] = trim(l); }   /* "1 shui" / "1 水 shuǐ": last resort, first single space */
    return n;
}
static int v_parse(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: csv_lab parse translate|score <reply.txt> <batch.tsv> <out.tsv>\n"); return 2; }
    int tr = !strcmp(argv[0], "translate");
    char *reply = slurp(argv[1], NULL); Tab bt; if (!reply || tab_load(argv[2], &bt)) { fprintf(stderr, "csv_lab parse: cannot read reply or batch\n"); return 2; }
    int *seen = calloc((size_t)bt.n + 1, sizeof(int)); int parsed = 0, dup = 0, junk = 0;
    FILE *o = fopen(argv[3], "w"); if (!o) return 2;
    for (char *p = reply; *p; ) {
        char *e = strchr(p, '\n'); if (e) *e = 0;
        char *cells[4] = {"", "", "", ""}; int nc = split_reply_line(p, cells, 4);
        if (nc >= 2 && isdigit((unsigned char)cells[0][0])) {
            long n = atol(cells[0]); int isdup = 0;
            const Rec *row = NULL; for (int i = 0; i < bt.n; i++) if (atol(bt.r[i].f[0]) == n) { row = &bt.r[i]; if (seen[i]++) { dup++; isdup = 1; row = NULL; } break; }
            if (row) {
                if (tr && nc >= 3) { clean_cell(cells[1]); clean_cell(cells[2]); fprintf(o, "%s\t%s\t%s\n", row->f[1], cells[1], cells[2]); parsed++; }
                else if (!tr) {
                    int sc = 0; for (char *q = cells[1]; *q; q++) if (*q >= '1' && *q <= '5') { sc = *q - '0'; break; }
                    if (sc) { clean_cell(cells[2]); fprintf(o, "%s\t%d\t%s\n", row->f[1], sc, nc >= 3 ? cells[2] : ""); parsed++; } else junk++;
                } else junk++;
            } else if (!isdup) junk++;
        } else { char *tp = trim(p); int fence = 1; for (char *q = tp; *q; q++) if (*q != '`') fence = 0; if (*tp && !fence) junk++; }
        if (!e) break;
        p = e + 1;
    }
    fclose(o); printf("parsed=%d missing=%d duplicate=%d unparsed_lines=%d\n", parsed, bt.n - parsed, dup, junk);
    free(seen); free(reply); tab_free(&bt); return 0;
}

/* ------------------------------------------------------------------ pinyin lint */
static int tone_of(unsigned cp, char *base) {   /* tone-marked vowel codepoint -> base letter + tone; 0 if not one */
    static const struct { unsigned cp; char b; int t; } M[] = {
        {0x101,'a',1},{0xE1,'a',2},{0x1CE,'a',3},{0xE0,'a',4},{0x113,'e',1},{0xE9,'e',2},{0x11B,'e',3},{0xE8,'e',4},{0x12B,'i',1},{0xED,'i',2},{0x1D0,'i',3},{0xEC,'i',4},
        {0x14D,'o',1},{0xF3,'o',2},{0x1D2,'o',3},{0xF2,'o',4},{0x16B,'u',1},{0xFA,'u',2},{0x1D4,'u',3},{0xF9,'u',4},{0x1D6,'v',1},{0x1D8,'v',2},{0x1DA,'v',3},{0x1DC,'v',4} };
    for (unsigned i = 0; i < sizeof M / sizeof M[0]; i++) if (M[i].cp == cp) { *base = M[i].b; return M[i].t; }
    return 0;
}
/* Normalise a pinyin token to ASCII letters ('v' = ü) with a tone digit per letter position: letters[], tones[]; returns length or -1 on a bad character. */
static int py_norm(const char *tok, char *letters, int *tones, int max, char *bad) {
    int n = 0; unsigned cp; const char *p = tok;
    while (u8dec(&p, &cp)) {
        char b;
        if (cp >= 0x300 && cp <= 0x30F) {   /* combining mark after a base letter */
            int t = cp == 0x304 ? 1 : cp == 0x301 ? 2 : cp == 0x30C ? 3 : cp == 0x300 ? 4 : 0;
            if (!t || !n) { *bad = '?'; return -1; }
            tones[n - 1] = t; continue;
        }
        if (n >= max - 1) { *bad = '?'; return -1; }
        if (cp < 0x80 && isalpha((int)cp)) { letters[n] = (char)tolower((int)cp); tones[n++] = 0; if (cp == 'v' || cp == 'V') { *bad = 'v'; return -2; } continue; }
        if (cp == 0xFC) { letters[n] = 'v'; tones[n++] = 0; continue; }
        int t = tone_of(cp, &b);
        if (t) { letters[n] = b; tones[n++] = t; continue; }
        if (is_cjk_punct(cp)) continue;
        if (cp == '\'' || cp == 0x2019) { letters[n] = '\''; tones[n++] = 0; continue; }
        *bad = cp < 0x80 ? (char)cp : '?'; return -1;
    }
    letters[n] = 0; return n;
}
static int is_v(char c) { return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' || c == 'v'; }
static int final_ok(const char *ini, const char *fin) {   /* finals in written form; coarse initial/final compatibility, not a full syllable dictionary */
    static const char *F[] = {"a","o","e","i","u","v","ai","ei","ui","ao","ou","iu","ie","ve","ue","er","an","en","in","un","vn","ang","eng","ing","ong","ia","iao","ian","iang","iong","ua","uo","uai","uan","uang","ueng","van","ng","m","n",NULL};
    int ok = 0; for (int i = 0; F[i]; i++) if (!strcmp(F[i], fin)) { ok = 1; break; }
    if (!ok) return 0;
    if (!strcmp(ini, "j") || !strcmp(ini, "q") || !strcmp(ini, "x")) return fin[0] == 'i' || fin[0] == 'u' || fin[0] == 'v';
    if (!strcmp(ini, "zh") || !strcmp(ini, "ch") || !strcmp(ini, "sh") || !strcmp(ini, "r") || !strcmp(ini, "z") || !strcmp(ini, "c") || !strcmp(ini, "s")) return fin[0] != 'v' && (fin[0] != 'i' || !strcmp(fin, "i"));
    if (!strcmp(ini, "b") || !strcmp(ini, "p") || !strcmp(ini, "m") || !strcmp(ini, "f") || !strcmp(ini, "d") || !strcmp(ini, "t") || !strcmp(ini, "g") || !strcmp(ini, "k") || !strcmp(ini, "h")) return fin[0] != 'v';
    if (!strcmp(ini, "n") || !strcmp(ini, "l")) return 1;
    if (!strcmp(ini, "y")) return !strcmp(fin, "a") || !strcmp(fin, "ao") || !strcmp(fin, "an") || !strcmp(fin, "ang") || !strcmp(fin, "e") || !strcmp(fin, "i") || !strcmp(fin, "in") || !strcmp(fin, "ing") || !strcmp(fin, "o") || !strcmp(fin, "ong") || !strcmp(fin, "ou") || !strcmp(fin, "u") || !strcmp(fin, "uan") || !strcmp(fin, "ue") || !strcmp(fin, "un") || !strcmp(fin, "v") || !strcmp(fin, "ve") || !strcmp(fin, "van") || !strcmp(fin, "vn");
    if (!strcmp(ini, "w")) return !strcmp(fin, "a") || !strcmp(fin, "ai") || !strcmp(fin, "an") || !strcmp(fin, "ang") || !strcmp(fin, "ei") || !strcmp(fin, "en") || !strcmp(fin, "eng") || !strcmp(fin, "o") || !strcmp(fin, "u");
    return 1;   /* no initial */
}
/* Split one normalised token into syllables. Returns count, or -1 with *why set. Marks: tone per syllable, and whether the mark sits on the right vowel. */
static int py_syllables(const char *L, const int *T, int n, int *ntone_ok, int *nmarked, char *why, size_t wn) {
    int i = 0, cnt = 0; *nmarked = 0; *ntone_ok = 1;
    while (i < n) {
        char ini[4] = ""; char fin[8] = ""; int s = i, ti = -1, tcount = 0, vlist[8], nv = 0;
        if (L[i] == '\'') { i++; continue; }
        if (!is_v(L[i])) {
            if (i + 1 < n && L[i + 1] == 'h' && (L[i] == 'z' || L[i] == 'c' || L[i] == 's')) { ini[0] = L[i]; ini[1] = 'h'; ini[2] = 0; i += 2; }
            else if (strchr("bpmfdtnlgkhjqxrzcsyw", L[i])) { ini[0] = L[i]; ini[1] = 0; i++; }
            else { snprintf(why, wn, "bad-initial:%c", L[i]); return -1; }
        }
        int vs = i; while (i < n && is_v(L[i])) { if (nv < 8) vlist[nv] = i; nv++; i++; }
        if (nv == 0) {   /* ng / n / m / hm alone are interjections: only allow "ng"/"n"/"m"/"r" forms after an initial-less start */
            snprintf(why, wn, "no-vowel:%.*s", i - s, L + s); return -1;
        }
        /* coda: n, ng, r - take it when it is not the initial of the next syllable (next char is not a vowel) */
        int fe = i;
        if (i < n && L[i] == 'n') {
            if (i + 1 < n && L[i + 1] == 'g' && !(i + 2 < n && is_v(L[i + 2]))) fe = i + 2;
            else if (!(i + 1 < n && is_v(L[i + 1]))) fe = i + 1;
        } else if (i < n && L[i] == 'r' && !(i + 1 < n && is_v(L[i + 1])) && !strcmp(ini, "") && i - vs == 1 && L[vs] == 'e') fe = i + 1;   /* er */
        else if (i < n && L[i] == 'r' && !(i + 1 < n && is_v(L[i + 1]))) fe = i + 1;   /* erhua r */
        if (fe - vs >= (int)sizeof fin) { snprintf(why, wn, "long-final"); return -1; }
        memcpy(fin, L + vs, (size_t)(fe - vs)); fin[fe - vs] = 0; i = fe;
        for (int k = vs; k < fe; k++) if (T[k]) { tcount++; ti = k; }
        if (tcount > 1) { snprintf(why, wn, "two-marks:%.*s", i - s, L + s); return -1; }
        /* erhua: a trailing lone 'r' after a complete syllable is fine; "er" is the syllable er */
        { char fin2[8]; snprintf(fin2, sizeof fin2, "%s", fin);
          size_t fl = strlen(fin2); if (fl > 1 && fin2[fl - 1] == 'r' && strcmp(fin2, "er") != 0) fin2[fl - 1] = 0;   /* erhua tail */
          if (!final_ok(ini, fin2)) { snprintf(why, wn, "bad-syllable:%.*s", i - s, L + s); return -1; }
          if (tcount) {   /* the mark must be on the right vowel: a, else o, else e, else the second of iu/ui, else the only vowel */
            int want = -1;
            for (int k = 0; k < nv && k < 8; k++) if (L[vlist[k]] == 'a') { want = vlist[k]; break; }
            if (want < 0) for (int k = 0; k < nv && k < 8; k++) if (L[vlist[k]] == 'o') { want = vlist[k]; break; }
            if (want < 0) for (int k = 0; k < nv && k < 8; k++) if (L[vlist[k]] == 'e') { want = vlist[k]; break; }
            if (want < 0) want = nv >= 2 ? vlist[nv < 8 ? nv - 1 : 7] : vlist[0];
            if (want != ti) { *ntone_ok = 0; snprintf(why, wn, "tone-placement:%.*s", i - s, L + s); return -1; }
            (*nmarked)++;
          }
        }
        cnt++;
    }
    return cnt;
}

/* reference vocabulary (optional, external): hanzi, pinyin, gloss separated by 2+ spaces */
typedef struct { char hz[64], py[128], gl[256]; } Ref;
static Ref *g_ref; static int g_nref;
static int ref_load(const char *pointer, char *status, size_t sn) {
    char *buf = slurp(pointer, NULL); char path[PB] = "";
    if (!buf) { snprintf(status, sn, "skipped (pointer %s unreadable)", pointer); return -1; }
    for (char *p = buf; *p; ) {
        char *e = strchr(p, '\n'); char *f[6]; if (e) *e = 0;
        char *l = trim(p);
        if (*l && *l != '#' && split_bar(l, f, 6) >= 3 && !strcmp(f[0], "SOURCE") && !strcmp(f[1], "vocab_file")) snprintf(path, sizeof path, "%s", f[2]);
        if (!e) break;
        p = e + 1;
    }
    free(buf);
    if (!path[0]) { snprintf(status, sn, "skipped (no SOURCE|vocab_file row)"); return -1; }
    buf = slurp(path, NULL);
    if (!buf) { snprintf(status, sn, "skipped (vocab file missing)"); return -1; }
    free(g_ref); g_nref = 0;   /* lint runs once per repair round: start from an empty table every time */
    int cap = 1024; g_ref = malloc((size_t)cap * sizeof(Ref));
    for (char *p = buf; *p; ) {
        char *e = strchr(p, '\n'); if (e) *e = 0;
        char *l = trim(p); char *c[3] = {NULL, NULL, NULL}; int k = 0;
        unsigned cp; const char *q = l;
        if (u8dec(&q, &cp) && is_cjk(cp)) {   /* a data line starts with a headword */
            char *s = l;
            while (k < 3 && *s) {
                c[k++] = s;
                char *g = strstr(s, "  "); char *t = strchr(s, '\t'); if (t && (!g || t < g)) g = t;
                if (!g) break;
                *g = 0; s = g + 1; while (*s == ' ' || *s == '\t') s++;
            }
            if (k == 3 && g_nref < 20000) {
                if (g_nref == cap) { cap *= 2; g_ref = realloc(g_ref, (size_t)cap * sizeof(Ref)); }
                Ref *r = &g_ref[g_nref++]; snprintf(r->hz, sizeof r->hz, "%s", c[0]); snprintf(r->py, sizeof r->py, "%s", c[1]); snprintf(r->gl, sizeof r->gl, "%s", c[2]);
            }
        }
        if (!e) break;
        p = e + 1;
    }
    free(buf);
    snprintf(status, sn, "loaded %d entries", g_nref); return 0;
}
static void py_squash(const char *in, char *out, size_t n) {   /* lowercase, drop spaces/apostrophes/punctuation, keep tone marks (NFC) */
    size_t o = 0; unsigned cp; const char *p = in;
    while (u8dec(&p, &cp) && o + 5 < n) { if (cp == ' ' || cp == '\'' || cp == '-' || cp == '/' || cp == 0x2019) continue; if (cp < 0x80) { out[o++] = (char)tolower((int)cp); continue; } o += (size_t)u8enc(cp, out + o); }
    out[o] = 0;
}
static const char *TRAD = "這個們說話嗎來時會對裡麼學從還為過種機樣長間發問題東車開關愛電從點無與認讓";   /* a sample of traditional-only forms; a hit is a hard FAIL */

static int is_protected_run(const char *run, const char *src) { return strstr(src, run) != NULL; }
static void lint_row(const char *src, const char *zh, const char *py, char *fail, size_t fn, char *warn, size_t wn) {
    fail[0] = 0; warn[0] = 0;
#define FAILF(fmt, ...) do { size_t l_ = strlen(fail); snprintf(fail + l_, fn - l_, "%s" fmt, l_ ? ";" : "", __VA_ARGS__); } while (0)
#define FAILW(lit) FAILF("%s", lit)
    if (!*zh) { FAILW("empty-zh"); return; }
    int ncjk = 0, nerhua = 0; unsigned cp; const char *p = zh;
    char latin[64][48]; int nlat = 0; char run[48]; int rl = 0;
    while (u8dec(&p, &cp)) {
        if (is_cjk(cp)) { ncjk++; if (cp == 0x513F) nerhua++; }
        if (is_kana_hangul(cp)) { FAILW("kana-or-hangul"); break; }
        if (cp < 0x80 && isalnum((int)cp)) { if (rl < 47) run[rl++] = (char)cp; }
        else { if (rl && nlat < 64) { run[rl] = 0; memcpy(latin[nlat++], run, (size_t)rl + 1); } rl = 0; }
    }
    if (rl && nlat < 64) { run[rl] = 0; memcpy(latin[nlat++], run, (size_t)rl + 1); }
    if (ncjk == 0) FAILW("no-cjk");
    for (int i = 0; i < nlat; i++) if (!is_protected_run(latin[i], src)) { FAILF("latin-in-zh:%s", latin[i]); break; }
    for (const char *t = TRAD; *t; ) { const char *q = t; unsigned tc; u8dec(&q, &tc); char one[8] = {0}; memcpy(one, t, (size_t)(q - t)); if (strstr(zh, one)) { FAILF("traditional-char:%s", one); break; } t = q; }
    /* length sanity: Chinese characters vs English letters */
    int letters = 0; for (const char *s = src; *s; s++) if (isalnum((unsigned char)*s)) letters++;
    if (ncjk > 0) {
        int lo = letters / 14; if (lo < 1) lo = 1; int hi = letters + 4;
        if (ncjk < lo || ncjk > hi) FAILF("length:%dcjk-for-%dletters", ncjk, letters);
    }
    if (!*py) { FAILW("empty-pinyin"); return; }
    /* pinyin: every token is a protected Latin run or a run of valid syllables */
    int syl = 0, marked = 0; char toks[1024]; snprintf(toks, sizeof toks, "%s", py);
    for (char *s = toks; *s; s++) if ((unsigned char)*s >= 0x80) continue; else if (strchr(",.?!:;()\"", *s)) *s = ' ';
    { char *w, *sv = NULL;   /* also split fullwidth punctuation */
      for (w = strtok_r(toks, " \t", &sv); w; w = strtok_r(NULL, " \t", &sv)) {
        char L[64]; int T[64]; char bad = 0; int n;
        int prot = 0; for (int i = 0; i < nlat; i++) if (!strcmp(latin[i], w)) prot = 1;
        if (prot) continue;
        if (strchr(w, '-')) { for (char *d = w; *d; d++) if (*d == '-') *d = '\''; }
        for (const char *d = w; *d; d++) if (isdigit((unsigned char)*d)) { FAILF("tone-number:%s", w); goto done_py; }
        n = py_norm(w, L, T, 64, &bad);
        if (n == -2) { FAILW("letter-v-instead-of-u-umlaut"); goto done_py; }
        if (n < 0) { FAILF("bad-char-in-pinyin:%s", w); goto done_py; }
        { char why[64] = ""; int tok_ok, nm = 0; int c = py_syllables(L, T, n, &tok_ok, &nm, why, sizeof why);
          if (c < 0) { FAILF("%s", why); goto done_py; }
          syl += c; marked += nm; }
      }
    }
    if (syl == 0) { FAILW("no-pinyin-syllables"); return; }
    if (syl != ncjk && syl != ncjk - nerhua && ncjk > 0) FAILF("syllable-count:cjk=%d,pinyin=%d", ncjk, syl);
    if (marked == 0) FAILW("no-tone-marks");
    else if (syl - marked > (syl + 2) / 3) FAILF("too-few-tone-marks:%d/%d", marked, syl);
done_py:;
#undef FAILW
#undef FAILF
    /* reference cross-check (advisory warnings only) */
    if (g_nref && !fail[0]) {
        char sq[1024]; py_squash(py, sq, sizeof sq); char sl[256]; size_t o = 0; for (const char *s = src; *s && o + 1 < sizeof sl; s++) sl[o++] = (char)tolower((unsigned char)*s); sl[o] = 0;
        for (int i = 0; i < g_nref; i++) {
            const Ref *r = &g_ref[i]; int hit = 0;
            /* gloss: the whole source equals one comma/semicolon-separated gloss item */
            char gl[256]; snprintf(gl, sizeof gl, "%s", r->gl);
            for (char *it = strtok(gl, ";,/"); it; it = strtok(NULL, ";,/")) { char *x = trim(it); if (!strncmp(x, "to ", 3)) x += 3; if (!strcmp(x, sl) && strstr(zh, r->hz) == NULL && strlen(sl) > 2) { char m[120]; snprintf(m, sizeof m, "ref-gloss:%s=%s", sl, r->hz); size_t l = strlen(warn); if (l + strlen(m) + 2 < wn) snprintf(warn + l, wn - l, "%s%s", l ? ";" : "", m); hit = 1; break; } }
            if (hit) break;
        }
        for (int i = 0; i < g_nref; i++) {
            const Ref *r = &g_ref[i]; size_t hl = strlen(r->hz); if (hl < 6) continue;   /* multi-character headwords only: single characters are polyphonic */
            const char *at = strstr(zh, r->hz); if (!at) continue;
            char rs[128]; py_squash(r->py, rs, sizeof rs);
            if (!strstr(sq, rs)) { char m[160]; snprintf(m, sizeof m, "ref-pinyin:%s", r->hz); size_t l = strlen(warn); if (l + strlen(m) + 2 < wn) snprintf(warn + l, wn - l, "%s%s", l ? ";" : "", m); }
        }
    }
}
static int v_lint(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: csv_lab lint <trans.tsv> <out.tsv> [--ref pointer.pdl] [--expect jobs.tsv]\n"); return 2; }
    Tab t; if (tab_load(argv[0], &t)) { fprintf(stderr, "csv_lab lint: cannot read %s\n", argv[0]); return 2; }
    const char *refp, *expp; char rstat[160] = "not requested";
    if (argval(argc, argv, "--ref", &refp)) ref_load(refp, rstat, sizeof rstat);
    Tab src = {0}; int have_src = 0;
    if (argval(argc, argv, "--expect", &expp)) { if (tab_load(expp, &src)) { fprintf(stderr, "csv_lab lint: cannot read %s\n", expp); return 2; } have_src = 1; }
    FILE *o = fopen(argv[1], "w"); if (!o) return 2;
    int ok = 0, fail = 0, warn = 0;
    if (have_src) {   /* row numbering complete: every expected job appears exactly once, and nothing else */
        for (int i = 0; i < src.n; i++) {
            int c = 0; for (int j = 0; j < t.n; j++) if (!strcmp(t.r[j].f[0], src.r[i].f[0])) c++;
            if (c == 0) { fprintf(o, "%s\tFAIL\tmissing-row\t\n", src.r[i].f[0]); fail++; }
            else if (c > 1) { fprintf(o, "%s\tFAIL\tduplicate-row\t\n", src.r[i].f[0]); fail++; }
        }
    }
    for (int i = 0; i < t.n; i++) {
        const Rec *r = &t.r[i]; char f[512], w[512];
        const char *srctext = fld(r, 3);   /* trans.tsv: jobid, zh, pinyin, src (the pipeline appends the source) */
        if (have_src) { const Rec *s = tab_find(&src, r->f[0]); if (!s) { fprintf(o, "%s\tFAIL\tunexpected-row\t\n", r->f[0]); fail++; continue; } srctext = fld(s, 1); }
        else if (!*srctext) srctext = "";
        int dupe = 0; if (have_src) { int c = 0; for (int j = 0; j < t.n; j++) if (!strcmp(t.r[j].f[0], r->f[0])) c++; dupe = c > 1; }
        if (dupe) continue;
        lint_row(srctext, fld(r, 1), fld(r, 2), f, sizeof f, w, sizeof w);
        if (f[0]) { fprintf(o, "%s\tFAIL\t%s\t%s\n", r->f[0], f, w); fail++; } else { fprintf(o, "%s\tOK\t\t%s\n", r->f[0], w); ok++; }
        if (w[0]) warn++;
    }
    fclose(o); printf("ok=%d fail=%d warn=%d ref=%s\n", ok, fail, warn, rstat);
    if (have_src) tab_free(&src);
    tab_free(&t); return 0;
}

/* ------------------------------------------------------------------ JSON helpers for the model calls */
static void json_esc(FILE *o, const char *s) {
    for (; *s; s++) switch (*s) { case '"': fputs("\\\"", o); break; case '\\': fputs("\\\\", o); break; case '\n': fputs("\\n", o); break; case '\r': fputs("\\r", o); break; case '\t': fputs("\\t", o); break;
        default: if ((unsigned char)*s < 0x20) fprintf(o, "\\u%04x", *s); else fputc(*s, o); }
}
static char *json_str_after(const char *body, const char *key) {   /* value of the first "key":"..." (unescaped, malloc'd) */
    char pat[96]; snprintf(pat, sizeof pat, "\"%s\"", key); const char *p = strstr(body, pat);
    while (p) {
        p += strlen(pat); while (*p == ' ' || *p == ':' || *p == '\n') p++;
        if (*p == '"') break;
        p = strstr(p, pat);
    }
    if (!p) return NULL;
    p++; char *out = malloc(strlen(p) + 1); size_t o = 0;
    while (*p && *p != '"') {
        if (*p == '\\') {
            p++;
            switch (*p) { case 'n': out[o++] = '\n'; p++; break; case 't': out[o++] = '\t'; p++; break; case 'r': out[o++] = '\r'; p++; break; case 'b': case 'f': p++; break;
                case 'u': { unsigned cp = (unsigned)strtoul((char[5]){p[1], p[2], p[3], p[4], 0}, NULL, 16); p += 5;
                    if (cp >= 0xD800 && cp < 0xDC00 && p[0] == '\\' && p[1] == 'u') { unsigned lo = (unsigned)strtoul((char[5]){p[2], p[3], p[4], p[5], 0}, NULL, 16); cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00); p += 6; }
                    o += (size_t)u8enc(cp, out + o); break; }
                default: out[o++] = *p++; }
        } else out[o++] = *p++;
    }
    out[o] = 0; return out;
}
static long json_num_after(const char *body, const char *key) { char pat[64]; snprintf(pat, sizeof pat, "\"%s\":", key); const char *p = strstr(body, pat); return p ? atol(p + strlen(pat)) : -1; }

/* ------------------------------------------------------------------ quota ledger + pacing: REQ|epoch|provider|model|tok_in|tok_out|est|rc */
static void quota_state(const char *ledger, long long now, long *used_today, long *tok_min, long long *oldest_in_min) {
    *used_today = 0; *tok_min = 0; *oldest_in_min = 0;
    Tab t; char *buf = slurp(ledger, NULL); (void)t;
    if (!buf) return;
    for (char *p = buf; *p; ) {
        char *e = strchr(p, '\n'); char *f[10]; if (e) *e = 0;
        char *l = trim(p);
        if (!strncmp(l, "REQ|", 4) && split_bar(l, f, 10) >= 6 && !strcmp(f[2], "groq")) {
            long long ts = atoll(f[1]);
            if (ts / 86400 == now / 86400) (*used_today)++;
            if (ts > now - 60 && ts <= now) { *tok_min += atol(f[4]) + atol(f[5]); if (!*oldest_in_min || ts < *oldest_in_min) *oldest_in_min = ts; }
        }
        if (!e) break;
        p = e + 1;
    }
    free(buf);
}
static int v_quota(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: csv_lab quota <ledger> <now_epoch> <est_tokens> [--day-limit 1000] [--tpm 7000]\n"); return 2; }
    long long now = atoll(argv[1]); long est = atol(argv[2]), dl = argnum(argc, argv, "--day-limit", 1000), tpm = argnum(argc, argv, "--tpm", 7000);
    long used, tm; long long old; quota_state(argv[0], now, &used, &tm, &old);
    if (est > tpm) est = tpm;
    long wait = 0; long long t = now;
    for (int guard = 0; guard < 100; guard++) {   /* move the clock until the one-minute window has room for this call */
        long u2, tm2; long long old2; quota_state(argv[0], t, &u2, &tm2, &old2);
        if (tm2 + est <= tpm || !old2) break;
        t = old2 + 61; wait = (long)(t - now);
    }
    printf("used_today=%ld limit=%ld tokens_last_min=%ld tpm=%ld wait_s=%ld blocked=%d\n", used, dl, tm, tpm, wait, used >= dl);
    return used >= dl ? 3 : 0;
}
static void ledger_add(const char *ledger, const char *prov, const char *model, long in, long out, int est, int rc) {
    if (!ledger) return;
    FILE *f = fopen(ledger, "a"); if (!f) return;
    fprintf(f, "REQ|%lld|%s|%s|%ld|%ld|%d|%d\n", now_s(), prov, model, in, out, est, rc); fclose(f);
}

/* ------------------------------------------------------------------ model calls */
static int run_child(char *const argv[], const char *errfile, int timeout_s, char *const envadd[]) {   /* fork+exec+waitpid with a watchdog; stdout discarded; returns exit code or 128+sig, 6 on timeout */
    pid_t pid = fork(); if (pid < 0) return 4;
    if (pid == 0) {
        int dn = open("/dev/null", O_WRONLY), ef = open(errfile, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (dn >= 0) dup2(dn, 1);
        if (ef >= 0) dup2(ef, 2);
        int nul = open("/dev/null", O_RDONLY); if (nul >= 0) dup2(nul, 0);
        for (int i = 0; envadd && envadd[i]; i++) putenv(envadd[i]);
        setsid(); execv(argv[0], argv); _exit(127);
    }
    long long end = mono_ms() + (long long)timeout_s * 1000;
    for (;;) {
        int st; pid_t w = waitpid(pid, &st, WNOHANG);
        if (w == pid) return WIFEXITED(st) ? WEXITSTATUS(st) : 128 + WTERMSIG(st);
        if (mono_ms() > end) { kill(-pid, SIGKILL); waitpid(pid, &st, 0); return 6; }
        usleep(50000);
    }
}
static int call_groq(int argc, char **argv, const char *prompt, char **reply, long *tin, long *tout, int *est) {
    char root[PB], backend[PB], work[PB], keydir[PB];
    const char *v, *model = "openai/gpt-oss-120b"; int timeout = (int)argnum(argc, argv, "--timeout", 120);
    house_root(argc, argv, root, sizeof root);
    if (argval(argc, argv, "--model", &v)) model = v;
    if ((v = getenv("CSV_LAB_GROQ_BACKEND")) && *v) snprintf(backend, sizeof backend, "%s", v);
    else snprintf(backend, sizeof backend, "%s/^.hai-horn/ops/+x/horn_chat_backend.+x", root);
    if (argval(argc, argv, "--key-dir", &v) || ((v = getenv("CSV_LAB_KEY_DIR")) && *v)) snprintf(keydir, sizeof keydir, "%s", v);
    else snprintf(keydir, sizeof keydir, "%s/&.widgits/open-hai/state", root);
    if (access(backend, X_OK)) { fprintf(stderr, "csv_lab call: groq backend not built: %s\n", backend); return 4; }
    const char *wd = getenv("CSV_LAB_WORK"); snprintf(work, sizeof work, "%s", wd && *wd ? wd : "/tmp");
    char hr[PB + 64], pieces[PB + 64], rfile[PB + 64], efile[PB + 64], e1[PB + 64], e2[PB + 64], e3[PB + 64], e4[PB + 64], e5[PB + 64], e6[PB + 64], e7[PB + 64], e8[PB + 64], mx[64];
    snprintf(hr, sizeof hr, "%s/csv_lab_horn", work); snprintf(pieces, sizeof pieces, "%s/pieces/horn", hr); mkdirs(pieces);
    snprintf(rfile, sizeof rfile, "%s/reply.txt", pieces); snprintf(efile, sizeof efile, "%s/stderr.txt", hr);
    unlink(rfile);
    snprintf(e1, sizeof e1, "PRISC_PROJECT_ROOT=%s", hr); snprintf(e2, sizeof e2, "HORN_ENTITY_DIR=%s", keydir); snprintf(e3, sizeof e3, "HORN_REPLY_FILE=%s", rfile);
    snprintf(e4, sizeof e4, "HORN_PIN_PROVIDER=groq"); snprintf(e5, sizeof e5, "HORN_PIN_MODEL=%s", model); snprintf(e6, sizeof e6, "HORN_TOOLS=off");
    snprintf(e7, sizeof e7, "HORN_CURL_TIMEOUT=%d", timeout > 20 ? timeout - 10 : timeout);
    snprintf(mx, sizeof mx, "HORN_MAX_TOKENS=%ld", argnum(argc, argv, "--max-tokens", 4000)); snprintf(e8, sizeof e8, "%s", mx);
    char *envadd[] = {e1, e2, e3, e4, e5, e6, e7, e8, NULL};
    char *cargv[] = {backend, (char *)prompt, NULL};
    int rc = run_child(cargv, efile, timeout + 5, envadd);
    *tin = *tout = 0; *est = 1; *reply = NULL;
    if (rc == 0) {
        *reply = slurp(rfile, NULL);
        char raw[PB + 64]; snprintf(raw, sizeof raw, "%s/.req_raw.json", pieces); char *rb = slurp(raw, NULL);
        if (rb) { long pt = json_num_after(rb, "prompt_tokens"), ct = json_num_after(rb, "completion_tokens"); if (pt >= 0 && ct >= 0) { *tin = pt; *tout = ct; *est = 0; } free(rb); }
        if (!*reply || !**reply) { free(*reply); *reply = NULL; return 5; }
        if (*est) { *tin = est_tokens(prompt); *tout = est_tokens(*reply); }
        return 0;
    }
    return rc == 3 ? 3 : rc == 6 ? 6 : 4;   /* key and Authorization never reach our output: only the exit class is reported */
}
static int url_allowed(const char *host, int port, const char *pdl_host) {
    int loop = !strcmp(host, "127.0.0.1") || !strcmp(host, "localhost") || !strcmp(host, "::1");
    if (loop) return port != 11434;                 /* loopback is test-only and NEVER the local Ollama port */
    return !strcmp(host, pdl_host);                 /* otherwise only the Mac named in ai_backend.pdl */
}
static int parse_url(const char *url, char *host, size_t hn, int *port) {
    if (strncmp(url, "http://", 7)) return -1;
    const char *h = url + 7; const char *c = strchr(h, ':'); const char *s = strchr(h, '/');
    size_t l = c ? (size_t)(c - h) : s ? (size_t)(s - h) : strlen(h); if (l >= hn || !l) return -1;
    memcpy(host, h, l); host[l] = 0; *port = c ? atoi(c + 1) : 80; return 0;
}
static int call_mac(int argc, char **argv, const char *prompt, char **reply, long *tin, long *tout) {
    char root[PB], cfg[PB], url[256] = "", host[128], pdlhost[128] = ""; int port = 0, ppdl = 0;
    house_root(argc, argv, root, sizeof root);
    snprintf(cfg, sizeof cfg, "%s/#.desktop/ai_backend.pdl", root);
    char *c = slurp(cfg, NULL);
    if (c) { for (char *p = c; *p; ) { char *e = strchr(p, '\n'); if (e) *e = 0; if (!strncmp(p, "gemma_lan_url=", 14)) snprintf(url, sizeof url, "%s", trim(p + 14)); if (!e) break; p = e + 1; } free(c); }
    if (url[0] && parse_url(url, pdlhost, sizeof pdlhost, &ppdl)) pdlhost[0] = 0;
    const char *ov = getenv("CSV_LAB_MAC_URL"); if (ov && *ov) snprintf(url, sizeof url, "%s", ov);
    if (!url[0] || parse_url(url, host, sizeof host, &port)) { fprintf(stderr, "csv_lab call: no usable gemma_lan_url in %s\n", cfg); return 4; }
    if (!url_allowed(host, port, pdlhost)) { fprintf(stderr, "csv_lab call: refusing %s:%d (only the Mac from ai_backend.pdl, or test loopback not on 11434)\n", host, port); return 4; }
    const char *model = "qwen2.5-coder:7b", *v; if (argval(argc, argv, "--model", &v)) model = v;
    int timeout = (int)argnum(argc, argv, "--timeout", 120);
    char ps[16]; snprintf(ps, sizeof ps, "%d", port);
    struct addrinfo hints, *ai; memset(&hints, 0, sizeof hints); hints.ai_socktype = SOCK_STREAM; hints.ai_family = AF_INET;
    if (getaddrinfo(host, ps, &hints, &ai)) { fprintf(stderr, "csv_lab call: cannot resolve %s\n", host); return 4; }
    int fd = socket(ai->ai_family, SOCK_STREAM, 0); fcntl(fd, F_SETFL, O_NONBLOCK);
    int cr = connect(fd, ai->ai_addr, ai->ai_addrlen);
    if (cr < 0 && errno == EINPROGRESS) { struct pollfd pf = {fd, POLLOUT, 0}; int pr = poll(&pf, 1, 10000); int soe = 0; socklen_t sl = sizeof soe; if (pr <= 0 || getsockopt(fd, SOL_SOCKET, SO_ERROR, &soe, &sl) || soe) cr = -1; else cr = 0; }
    freeaddrinfo(ai);
    if (cr < 0) { close(fd); fprintf(stderr, "csv_lab call: cannot connect to the model host\n"); return 6; }
    fcntl(fd, F_SETFL, 0);
    /* body */
    char *bp = NULL; size_t bl = 0; FILE *mb = open_memstream(&bp, &bl);
    fputs("{\"model\":\"", mb); json_esc(mb, model); fputs("\",\"stream\":false,\"options\":{\"temperature\":0},\"messages\":[{\"role\":\"user\",\"content\":\"", mb); json_esc(mb, prompt); fputs("\"}]}", mb); fclose(mb);
    char hdr[512]; int hl = snprintf(hdr, sizeof hdr, "POST /api/chat HTTP/1.1\r\nHost: %s:%d\r\nContent-Type: application/json\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n", host, port, bl);
    struct timeval tv = {10, 0}; setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
    if (write(fd, hdr, (size_t)hl) != hl || write(fd, bp, bl) != (ssize_t)bl) { free(bp); close(fd); return 4; }
    free(bp);
    size_t cap = 1 << 16, n = 0; char *resp = malloc(cap); long long end = mono_ms() + (long long)timeout * 1000; int timed_out = 0;
    for (;;) {
        long long left = end - mono_ms(); if (left <= 0) { timed_out = 1; break; }
        struct pollfd pf = {fd, POLLIN, 0}; int pr = poll(&pf, 1, (int)(left > 1000 ? 1000 : left));
        if (pr < 0) break;
        if (pr == 0) continue;
        if (n + 8192 >= cap) { cap *= 2; resp = realloc(resp, cap); }
        ssize_t r = read(fd, resp + n, 8192); if (r <= 0) break; n += (size_t)r;
        resp[n] = 0;
        const char *he = strstr(resp, "\r\n\r\n"); const char *cl = strcasestr(resp, "content-length:");
        if (he && cl && cl < he && n >= (size_t)(he - resp) + 4 + (size_t)atol(cl + 15)) break;
    }
    close(fd); resp[n] = 0;
    if (timed_out) { free(resp); return 6; }
    if (strncmp(resp, "HTTP/1.", 7) || !strstr(resp, " 200")) { fprintf(stderr, "csv_lab call: model host answered %.12s\n", resp); free(resp); return 4; }
    char *body = strstr(resp, "\r\n\r\n"); body = body ? body + 4 : resp;
    if (strcasestr(resp, "transfer-encoding: chunked")) {   /* de-chunk in place */
        char *rd = body, *wr = body;
        for (;;) { long cs = strtol(rd, &rd, 16); while (*rd && *rd != '\n') rd++; if (*rd) rd++; if (cs <= 0) break; memmove(wr, rd, (size_t)cs); wr += cs; rd += cs; while (*rd == '\r' || *rd == '\n') rd++; }
        *wr = 0;
    }
    char *msg = strstr(body, "\"message\""); *reply = json_str_after(msg ? msg : body, "content");
    *tin = json_num_after(body, "prompt_eval_count"); *tout = json_num_after(body, "eval_count");
    if (*tin < 0) *tin = est_tokens(prompt);
    if (*tout < 0) *tout = *reply ? est_tokens(*reply) : 0;
    free(resp);
    return (*reply && **reply) ? 0 : 5;
}
static int v_call(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: csv_lab call groq|mac <prompt.txt> <reply.txt> [--model M] [--timeout S] [--ledger F] [--key-dir D] [--max-tokens N]\n"); return 2; }
    char *prompt = slurp(argv[1], NULL); if (!prompt) { fprintf(stderr, "csv_lab call: cannot read %s\n", argv[1]); return 2; }
    const char *ledger = NULL; argval(argc, argv, "--ledger", &ledger);
    const char *model = NULL; argval(argc, argv, "--model", &model);
    char *reply = NULL; long tin = 0, tout = 0; int est = 0, rc;
    if (!strcmp(argv[0], "groq")) {
        if (ledger) {   /* day limit and per-minute pacing are enforced HERE so no caller can forget them */
            long used, tm; long long old; quota_state(ledger, now_s(), &used, &tm, &old);
            if (used >= argnum(argc, argv, "--day-limit", 1000)) { fprintf(stderr, "csv_lab call: groq day limit reached (%ld requests today)\n", used); free(prompt); return 3; }
            long need = est_tokens(prompt) + 2500; long tpm = argnum(argc, argv, "--tpm", 7000); if (need > tpm) need = tpm;
            for (int g = 0; g < 100; g++) { quota_state(ledger, now_s(), &used, &tm, &old); if (tm + need <= tpm || !old) break; if (getenv("CSV_LAB_NO_SLEEP")) break; sleep(2); }
        }
        rc = call_groq(argc, argv, prompt, &reply, &tin, &tout, &est);
        ledger_add(ledger, "groq", model ? model : "openai/gpt-oss-120b", tin, tout, est, rc);
    } else if (!strcmp(argv[0], "mac")) {
        rc = call_mac(argc, argv, prompt, &reply, &tin, &tout);
        ledger_add(ledger, "mac", model ? model : "qwen2.5-coder:7b", tin, tout, 0, rc);
    } else { free(prompt); fprintf(stderr, "csv_lab call: provider must be groq or mac\n"); return 2; }
    free(prompt);
    if (rc == 0 && reply) { FILE *o = fopen(argv[2], "w"); if (!o) return 2; fputs(reply, o); fclose(o); printf("tokens_in=%ld tokens_out=%ld estimated=%d\n", tin, tout, est); }
    else printf("call_failed rc=%d\n", rc);
    free(reply); return rc;
}

/* ------------------------------------------------------------------ job status, owner review decisions, live UI feed, merge */
/* Status vocabulary (sidecar, answer bank and feed): new | translated (lint-clean, unscored) | lint_fail | low_score | ok | accepted | rejected.
 * ok and accepted are promotable; low_score / translated / lint_fail wait in review.tsv; rejected stays out and is retranslated on the next run. */
typedef struct { Tab all, hits, trans, lint, score; } WS;
static int ws_load(const char *w, WS *ws) {
    char p[PB]; memset(ws, 0, sizeof *ws);
    snprintf(p, sizeof p, "%s/all.tsv", w); if (tab_load(p, &ws->all)) return -1;
    snprintf(p, sizeof p, "%s/hits.tsv", w); if (tab_load(p, &ws->hits)) memset(&ws->hits, 0, sizeof ws->hits);
    snprintf(p, sizeof p, "%s/trans.tsv", w); if (tab_load(p, &ws->trans)) memset(&ws->trans, 0, sizeof ws->trans);
    snprintf(p, sizeof p, "%s/lint.tsv", w); if (tab_load(p, &ws->lint)) memset(&ws->lint, 0, sizeof ws->lint);
    snprintf(p, sizeof p, "%s/score.tsv", w); if (tab_load(p, &ws->score)) memset(&ws->score, 0, sizeof ws->score);
    return 0;
}
static void ws_free(WS *ws) { tab_free(&ws->all); tab_free(&ws->hits); tab_free(&ws->trans); tab_free(&ws->lint); tab_free(&ws->score); }
/* the machine status of one job from the files of the workdir, valid at any stage of a run */
static const char *derive_status(const WS *ws, const char *id, long thr, char *zh, char *py, char *sc, int *is_hit) {
    const Rec *h = tab_find(&ws->hits, id), *t = tab_find(&ws->trans, id), *l = tab_find(&ws->lint, id), *s = tab_find(&ws->score, id);
    zh[0] = py[0] = sc[0] = 0; *is_hit = 0;
    if (h) { snprintf(zh, 600, "%s", fld(h, 2)); snprintf(py, 600, "%s", fld(h, 3)); snprintf(sc, 8, "%s", fld(h, 4)); *is_hit = 1; return fld(h, 5)[0] ? fld(h, 5) : "ok"; }
    if (t) { snprintf(zh, 600, "%s", fld(t, 1)); snprintf(py, 600, "%s", fld(t, 2)); }
    if (l && strcmp(fld(l, 1), "OK")) return "lint_fail";
    if (!t && !l) return "new";
    if (l && s) { snprintf(sc, 8, "%s", fld(s, 1)); return atol(sc) >= thr ? "ok" : "low_score"; }
    return "translated";
}
/* owner decisions: append-only rows REVIEW|<bank>|<id>|accept|reject, last row for a job wins. 1 = accept, 2 = reject, 0 = none */
static char *g_review_buf;
static int review_load(const char *path) { free(g_review_buf); g_review_buf = path ? slurp(path, NULL) : NULL; return g_review_buf != NULL; }
static int review_of(const char *bank, const char *id) {
    int dec = 0; if (!g_review_buf) return 0;
    for (char *p = g_review_buf; *p; ) {
        char *e = strchr(p, '\n'); char line[1024], *f[8]; size_t L = e ? (size_t)(e - p) : strlen(p); if (L >= sizeof line) L = sizeof line - 1;
        memcpy(line, p, L); line[L] = 0;
        if (!strncmp(line, "REVIEW|", 7) && split_bar(line, f, 8) >= 4 && !strcmp(f[1], bank) && !strcmp(f[2], id)) dec = !strcmp(f[3], "accept") ? 1 : !strcmp(f[3], "reject") ? 2 : dec;
        if (!e) break;
        p = e + 1;
    }
    return dec;
}
/* live feed context, set by the pipeline (or by merge / ui when run alone) */
static char g_ui[PB], g_ui_log[PB + 8], g_run_id[48] = "manual", g_stage[16] = "extract", g_bank_name[96], g_work[PB], g_qledger[PB];
static long g_done, g_total, g_rows, g_thr = 4;
static void ui_clean(char *s) { for (; *s; s++) if (*s == '|' || *s == '\n' || *s == '\t' || *s == '\r') *s = '/'; }
static void ui_log(const char *fmt, ...) {   /* one short line to the LOG ring (a file next to the feed) */
    if (!g_ui_log[0]) return;
    { char d[PB]; dir_of(g_ui_log, d, sizeof d); mkdirs(d); }
    FILE *f = fopen(g_ui_log, "a"); if (!f) return;
    time_t tt = time(NULL); char hm[16]; strftime(hm, sizeof hm, "%H:%M:%S", localtime(&tt));
    char msg[300]; va_list ap; va_start(ap, fmt); vsnprintf(msg, sizeof msg, fmt, ap); va_end(ap); ui_clean(msg);
    fprintf(f, "LOG|%s|%s\n", hm, msg); fclose(f);
}
static void ledger_counts(const char *ledger, long long now, long *mac_today) {
    *mac_today = 0; char *buf = ledger ? slurp(ledger, NULL) : NULL; if (!buf) return;
    for (char *p = buf; *p; ) { char *e = strchr(p, '\n'); char *f[10]; if (e) *e = 0; char *l = trim(p);
        if (!strncmp(l, "REQ|", 4) && split_bar(l, f, 10) >= 6 && !strcmp(f[2], "mac") && atoll(f[1]) / 86400 == now / 86400) (*mac_today)++;
        if (!e) break;
        p = e + 1; }
    free(buf);
}
/* (Re)write the feed atomically: temp file + rename, so a reader never sees a half file. */
static int write_ui(void) {
    if (!g_ui[0] || !g_work[0]) return 0;
    WS ws; if (ws_load(g_work, &ws)) return -1;
    char tmp[PB + 8]; snprintf(tmp, sizeof tmp, "%s.tmp", g_ui);
    { char d[PB]; dir_of(g_ui, d, sizeof d); mkdirs(d); }
    FILE *o = fopen(tmp, "w"); if (!o) { ws_free(&ws); return -1; }
    char bank[96]; snprintf(bank, sizeof bank, "%s", g_bank_name); ui_clean(bank);
    fprintf(o, "HEAD|%s|%s|%s|%ld|%ld\n", g_run_id, bank, g_stage, g_done, g_total);
    long used = 0, tm = 0, mac = 0; long long old, now = now_s();
    if (g_qledger[0]) { quota_state(g_qledger, now, &used, &tm, &old); ledger_counts(g_qledger, now, &mac); }
    fprintf(o, "QUOTA|groq_req|%ld|1000\nQUOTA|groq_tok_min|%ld|7000\nQUOTA|mac_calls|%ld|0\n", used, tm, mac);
    for (int i = 0; i < ws.all.n; i++) {
        const char *id = ws.all.r[i].f[0]; char zh[600], py[600], sc[8], en[2100], idc[300], st[24]; int hit;
        snprintf(st, sizeof st, "%s", derive_status(&ws, id, g_thr, zh, py, sc, &hit));
        int d = review_of(g_bank_name, id); if (d == 1) snprintf(st, sizeof st, "accepted"); else if (d == 2) snprintf(st, sizeof st, "rejected");
        snprintf(en, sizeof en, "%s", fld(&ws.all.r[i], 1)); snprintf(idc, sizeof idc, "%s", id);
        ui_clean(idc); ui_clean(en); ui_clean(zh); ui_clean(py);
        fprintf(o, "JOB|%s|%s|%s|%s|%s|%s|%s\n", bank, idc, en, zh, py, sc[0] ? sc : "-", st);
    }
    { char *lb = slurp(g_ui_log, NULL);   /* only the last 40 LOG rows */
      if (lb) { int n = 0; for (char *p = lb; *p; p++) if (*p == '\n') n++;
          int skip = n > 40 ? n - 40 : 0; char *p = lb; while (skip-- > 0) { p = strchr(p, '\n') + 1; }
          fputs(p, o);
          if (n > 400) { FILE *tr = fopen(g_ui_log, "w"); if (tr) { fputs(p, tr); fclose(tr); } }   /* keep the ring file small */
          free(lb); } }
    fclose(o); ws_free(&ws);
    return rename(tmp, g_ui);
}
static void ui_stage(const char *stage, long done, long total) { snprintf(g_stage, sizeof g_stage, "%s", stage); g_done = done; g_total = total; write_ui(); }
static void ui_ctx_from_args(int argc, char **argv, const char *work, const char *bank) {   /* --ui FILE (default <work>/../../csv_lab_ui.txt), --review FILE (default next to the feed), --quota-ledger F, --threshold N */
    const char *v; snprintf(g_work, sizeof g_work, "%s", work); snprintf(g_bank_name, sizeof g_bank_name, "%s", bank);
    if (argval(argc, argv, "--ui", &v) || ((v = getenv("CSV_LAB_UI")) && *v)) snprintf(g_ui, sizeof g_ui, "%s", v); else snprintf(g_ui, sizeof g_ui, "%s/../../csv_lab_ui.txt", work);
    snprintf(g_ui_log, sizeof g_ui_log, "%s.log", g_ui);
    char rv[PB]; if (argval(argc, argv, "--review", &v) || ((v = getenv("CSV_LAB_REVIEW")) && *v)) snprintf(rv, sizeof rv, "%s", v); else { char d[PB]; dir_of(g_ui, d, sizeof d); snprintf(rv, sizeof rv, "%s/csv_lab_review.txt", d); }
    review_load(rv);
    g_qledger[0] = 0; if (argval(argc, argv, "--quota-ledger", &v)) snprintf(g_qledger, sizeof g_qledger, "%s", v);
    g_thr = argnum(argc, argv, "--threshold", 4);
}
static int v_ui(int argc, char **argv) {
    if (argc < 1) { fprintf(stderr, "usage: csv_lab ui <workdir> [--bank B] [--ui F] [--review F] [--quota-ledger F] [--stage S]\n"); return 2; }
    const char *b = "bank", *st = "done"; argval(argc, argv, "--bank", &b); argval(argc, argv, "--stage", &st);
    ui_ctx_from_args(argc, argv, argv[0], b); snprintf(g_stage, sizeof g_stage, "%s", st);
    return write_ui() ? 2 : 0;
}
/* set the status field (7th) of one answer-bank row in place; used for owner decisions (accepted / rejected) */
static void answers_set_status(const char *path, const char *key, const char *status) {
    char *buf = slurp(path, NULL); if (!buf) return;
    char *at = strstr(buf, key); if (!at || (at != buf && at[-1] != '\n')) { free(buf); return; }
    char *tab = at; for (int k = 0; k < 6 && tab; k++) { tab = strchr(tab, '\t'); if (tab) tab++; }
    if (!tab) { free(buf); return; }
    char *end = strchr(tab, '\t'); if (!end) end = strchr(tab, '\n');
    FILE *o = fopen(path, "w"); if (o) { fwrite(buf, 1, (size_t)(tab - buf), o); fputs(status, o); if (end) fputs(end, o); fclose(o); }
    free(buf);
}
static int v_merge(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: csv_lab merge <banks.pdl> <bank> <workdir> <answers.tsv> [--root DIR] [--threshold 4] [--feedback-dir D] [--quota-ledger F] [--out sidecar] [--ui F] [--review F]\n"); return 2; }
    char root[PB]; Bank b; house_root(argc, argv, root, sizeof root);
    if (bank_load(argv[0], argv[1], &b)) { fprintf(stderr, "csv_lab merge: bank %s not in %s\n", argv[1], argv[0]); return 7; }
    const char *w = argv[2], *fb = NULL, *out = NULL, *tr = "groq", *jd = "mac"; argval(argc, argv, "--feedback-dir", &fb); argval(argc, argv, "--out", &out);
    argval(argc, argv, "--translator", &tr); argval(argc, argv, "--judge", &jd);
    ui_ctx_from_args(argc, argv, w, b.name);
    long thr = g_thr;
    WS ws; if (ws_load(w, &ws)) { fprintf(stderr, "csv_lab merge: no all.tsv in %s\n", w); return 2; }
    if (!g_rows) g_rows = ws.all.n;
    char sc[PB];   /* sidecar path: --out, else manifest sidecar=, else <stem>.zh.tsv next to the (first) source file */
    if (out) snprintf(sc, sizeof sc, "%s", out);
    else if (b.sidecar[0]) snprintf(sc, sizeof sc, "%s/%s", root, b.sidecar);
    else { char f1[PB]; snprintf(f1, sizeof f1, "%s/%s", root, b.file); char *dot = strrchr(f1, '.'); char *sl = strrchr(f1, '/'); if (dot && sl && dot > sl && !strchr(b.file, '*')) *dot = 0; snprintf(sc, sizeof sc, "%s.zh.tsv", f1); }
    { char d[PB]; dir_of(sc, d, sizeof d); mkdirs(d); }
    char fin[PB], rv[PB]; snprintf(fin, sizeof fin, "%s/final.tsv", w); snprintf(rv, sizeof rv, "%s/review.tsv", w);
    FILE *fs = fopen(sc, "w"), *ff = fopen(fin, "w"), *fr = fopen(rv, "w");
    if (!fs || !ff || !fr) { fprintf(stderr, "csv_lab merge: cannot write sidecar/final/review\n"); return 2; }
    fprintf(fs, "# %s.zh sidecar of %s - machine translation. status: ok/accepted are promotable; low_score, translated (unscored) and lint_fail wait for a person (review.tsv); rejected stays out. Scores are advisory. columns: id, source, zh, pinyin, score, status, translator, judge\n", b.name, b.file);
    long n_new = 0, n_hit = 0, n_lintfail = 0, n_review = 0, n_ok = 0, n_acc = 0, n_rej = 0, tokens_total = 0;
    { char qp[PB]; snprintf(qp, sizeof qp, "%s/usage.txt", w); char *ub = slurp(qp, NULL); if (ub) { for (char *p = ub; (p = strstr(p, "tokens=")); p += 7) tokens_total += atol(p + 7); free(ub); } }
    char *ans_old = slurp(argv[3], NULL);   /* re-running a merge must not duplicate answer-bank rows */
    FILE *fa = fopen(argv[3], "a"); if (!fa) { fprintf(stderr, "csv_lab merge: cannot append %s\n", argv[3]); return 2; }
    char fbp[PB] = ""; if (fb) snprintf(fbp, sizeof fbp, "%s/obs_feedback_log.txt", fb);
    const char *worker = "worker"; if (fb) { const char *sl = strrchr(fb, '/'); worker = sl ? sl + 1 : fb; }
    char date[16]; { time_t tt = time(NULL); strftime(date, sizeof date, "%Y-%m-%d", localtime(&tt)); }
    char *decided_keys[512]; int decided_dec[512]; int ndec = 0;
    for (int i = 0; i < ws.all.n; i++) {
        const char *id = ws.all.r[i].f[0], *src = fld(&ws.all.r[i], 1); char zh[600], py[600], scs[8], st[24]; int hit;
        snprintf(st, sizeof st, "%s", derive_status(&ws, id, thr, zh, py, scs, &hit));
        if (!hit && !strcmp(st, "new")) continue;   /* not part of this run (--limit) */
        if (hit) n_hit++;
        else {
            n_new++;
            const Rec *l = tab_find(&ws.lint, id);
            if (!strcmp(st, "lint_fail")) fprintf(fr, "%s\t%s\t%s\t%s\t-\tlint: %s\n", id, src, zh, py, l ? fld(l, 2) : "no lint row");
            else {
                char k[65]; answer_key(src, b.lang, k);   /* only lint-clean rows enter the answer bank; a person can flip status to rejected to force a retry */
                if (!ans_old || !strstr(ans_old, k)) fprintf(fa, "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", k, b.lang, src, zh, py, scs[0] ? scs : "-", st, tr, jd);
            }
            if (fbp[0]) {   /* FEEDBACK row in the ledger_to_feedback shape; re-running never duplicates an id */
                char fid[512], key[560]; snprintf(fid, sizeof fid, "csv-zh-%s#%s", b.name, id); snprintf(key, sizeof key, "id=%s |", fid);
                char *old = slurp(fbp, NULL); int dup = old && strstr(old, key); free(old);
                if (!dup) { FILE *ffb = fopen(fbp, "a"); if (ffb) { fprintf(ffb, "[%s] FEEDBACK | id=%s | target=%s | valence=%+d | concept=zh_translation | intensity=1 | layer=curriculum | quest=csv-zh-%s | iter=%d | tokens=%ld\n", date, fid, worker, !strcmp(st, "ok") ? 1 : -1, b.name, i + 1, tokens_total / (n_new ? n_new : 1)); fclose(ffb); } }
            }
        }
        int d = review_of(b.name, id);   /* owner decision overrides the machine status */
        if (d && ndec < 512) { char k[65]; answer_key(src, b.lang, k); decided_keys[ndec] = strdup(k); decided_dec[ndec++] = d; }
        if (d == 1) snprintf(st, sizeof st, "accepted"); else if (d == 2) snprintf(st, sizeof st, "rejected");
        if (!strcmp(st, "ok")) n_ok++; else if (!strcmp(st, "accepted")) n_acc++; else if (!strcmp(st, "rejected")) n_rej++; else if (!strcmp(st, "lint_fail")) n_lintfail++; else n_review++;
        if (!strcmp(st, "low_score") || !strcmp(st, "translated")) fprintf(fr, "%s\t%s\t%s\t%s\t%s\t%s\n", id, src, zh, py, scs[0] ? scs : "-", scs[0] ? "judge: low score" : "unscored");
        { char s1[2100], z1[600], p1[600]; snprintf(s1, sizeof s1, "%s", src); snprintf(z1, sizeof z1, "%s", zh); snprintf(p1, sizeof p1, "%s", py); clean_cell(s1); clean_cell(z1); clean_cell(p1);
          fprintf(fs, "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", id, s1, z1, p1, scs[0] ? scs : "-", st, tr, jd);
          fprintf(ff, "%s\t%s\t%s\t%s\t%s\t%s\n", id, s1, z1, p1, scs[0] ? scs : "-", st); }
    }
    fclose(fs); fclose(ff); fclose(fr); fclose(fa); free(ans_old);
    for (int i = 0; i < ndec; i++) { answers_set_status(argv[3], decided_keys[i], decided_dec[i] == 1 ? "accepted" : "rejected"); free(decided_keys[i]); }
    ws_free(&ws);
    ui_log("merge %s: %ld new, %ld from answer bank, ok %ld accepted %ld rejected %ld review %ld lint_fail %ld", b.name, n_new, n_hit, n_ok, n_acc, n_rej, n_review, n_lintfail);
    ui_stage(!strcmp(g_stage, "error") ? "error" : "done", g_rows, g_rows);
    printf("sidecar=%s translated=%ld answer_hits=%ld ok=%ld review=%ld lintfail=%ld tokens=%ld accepted=%ld rejected=%ld\n", sc, n_new, n_hit, n_ok, n_review, n_lintfail, tokens_total, n_acc, n_rej);
    return 0;
}

/* ------------------------------------------------------------------ driver */
static void usage_add(const char *work, const char *stage, const char *prov, const char *model, long tin, long tout, int rc, int est) {
    char p[PB]; snprintf(p, sizeof p, "%s/usage.txt", work); FILE *f = fopen(p, "a"); if (!f) return;
    fprintf(f, "CALL | stage=%s | provider=%s | model=%s | req=1 | tokens_in=%ld | tokens_out=%ld | tokens=%ld | est=%d | rc=%d\n", stage, prov, model, tin, tout, tin + tout, est, rc); fclose(f);
}
static int file_lines(const char *p) { Tab t; int n = tab_load(p, &t) ? 0 : t.n; if (n) tab_free(&t); return n; }
static long long g_last_ms;
static int run_model(int pargc, char **pargv, const char *prov, const char *model, const char *work, const char *stage, const char *prompt, const char *reply, const char *ledger, const char *keydir) {
    /* call the model through the same code path as the call verb; account tokens in usage.txt */
    char *av[24]; int n = 0; char tin[32] = "", dummy[8]; (void)tin; (void)dummy;
    av[n++] = (char *)prov; av[n++] = (char *)prompt; av[n++] = (char *)reply;
    av[n++] = "--model"; av[n++] = (char *)model;
    if (ledger) { av[n++] = "--ledger"; av[n++] = (char *)ledger; }
    if (keydir) { av[n++] = "--key-dir"; av[n++] = (char *)keydir; }
    const char *v; if (argval(pargc, pargv, "--root", &v)) { av[n++] = "--root"; av[n++] = (char *)v; }
    if (argval(pargc, pargv, "--timeout", &v)) { av[n++] = "--timeout"; av[n++] = (char *)v; }
    FILE *cap = tmpfile(); char *bp;
    fflush(stdout); int so = dup(1); dup2(fileno(cap), 1);   /* capture the call verb's one-line report */
    long long t0 = mono_ms();
    int rc = v_call(n, av);
    g_last_ms = mono_ms() - t0;
    fflush(stdout); dup2(so, 1); close(so); fflush(cap); rewind(cap); bp = calloc(1, 512); if (bp) { size_t got = fread(bp, 1, 511, cap); bp[got] = 0; } fclose(cap);
    long a = 0, b2 = 0; int est = 0; char *q;
    if ((q = strstr(bp, "tokens_in="))) a = atol(q + 10);
    if ((q = strstr(bp, "tokens_out="))) b2 = atol(q + 11);
    if ((q = strstr(bp, "estimated="))) est = atoi(q + 10);
    free(bp); usage_add(work, stage, prov, model, a, b2, rc, est);
    return rc;
}
static int v_pipeline(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: csv_lab pipeline <banks.pdl> <bank> --work DIR --answers FILE [--translator groq] [--judge mac] [--judge-model M] [--translator-model M] [--limit N] [--rows N] [--tokens N] [--retries N] [--threshold N] [--quota-ledger F] [--feedback-dir D] [--ref pointer.pdl] [--root DIR] [--no-judge]\n"); return 2; }
    const char *work, *answers, *v, *tr = "groq", *jd = "mac", *jm = "qwen2.5-coder:7b", *tm = "openai/gpt-oss-120b", *ql = NULL, *fb = NULL, *ref = NULL, *keydir = NULL;
    if (!argval(argc, argv, "--work", &work) || !argval(argc, argv, "--answers", &answers)) { fprintf(stderr, "csv_lab pipeline: --work and --answers are required\n"); return 2; }
    argval(argc, argv, "--translator", &tr); argval(argc, argv, "--judge", &jd); argval(argc, argv, "--judge-model", &jm); argval(argc, argv, "--translator-model", &tm);
    argval(argc, argv, "--quota-ledger", &ql); argval(argc, argv, "--feedback-dir", &fb); argval(argc, argv, "--ref", &ref); argval(argc, argv, "--key-dir", &keydir);
    if (!ref && (v = getenv("CSV_LAB_REF")) && *v) ref = v;
    if (!strcmp(tr, jd) && !getenv("CSV_LAB_ALLOW_SAME_JUDGE")) { fprintf(stderr, "csv_lab pipeline: the judge must be a different provider/model from the translator\n"); return 2; }
    int retries = (int)argnum(argc, argv, "--retries", 2), no_judge = 0; for (int i = 0; i < argc; i++) if (!strcmp(argv[i], "--no-judge")) no_judge = 1;
    long rows = argnum(argc, argv, "--rows", 25), tok = argnum(argc, argv, "--tokens", 900);
    char bw[PB]; snprintf(bw, sizeof bw, "%s", work); mkdirs(bw); setenv("CSV_LAB_WORK", bw, 1);
    char p1[PB], p2[PB], p3[PB], bdir[PB], jobs[PB], trans[PB], lintf[PB], scoref[PB];
    snprintf(jobs, sizeof jobs, "%s/jobs.tsv", bw); snprintf(trans, sizeof trans, "%s/trans.tsv", bw); snprintf(lintf, sizeof lintf, "%s/lint.tsv", bw); snprintf(scoref, sizeof scoref, "%s/score.tsv", bw);
    snprintf(bdir, sizeof bdir, "%s/batches", bw);
    /* 1 extract */
    { char *a[16]; int n = 0; a[n++] = argv[0]; a[n++] = argv[1]; a[n++] = bw; a[n++] = (char *)answers; if (argval(argc, argv, "--root", &v)) { a[n++] = "--root"; a[n++] = (char *)v; } if (argval(argc, argv, "--limit", &v)) { a[n++] = "--limit"; a[n++] = (char *)v; }
      int rc = v_jobs(n, a); if (rc) return rc; }
    { snprintf(g_run_id, sizeof g_run_id, "r%lld-%d", now_s(), (int)getpid());
      ui_ctx_from_args(argc, argv, bw, argv[1]); char ap[PB]; snprintf(ap, sizeof ap, "%s/all.tsv", bw);
      unlink(g_ui_log); /* a new run starts a new log ring */
      ui_log("extract %s: %d jobs, %d answered by the answer bank", argv[1], file_lines(ap), file_lines(ap) - file_lines(jobs));
      g_rows = file_lines(ap); ui_stage("extract", g_rows, g_rows); }
    unlink(trans); unlink(scoref); { char up[PB]; snprintf(up, sizeof up, "%s/usage.txt", bw); unlink(up); }
    FILE *ft = fopen(trans, "w"); if (ft) fclose(ft);
    /* 2-5 translate, lint, repair */
    char cur[PB]; snprintf(cur, sizeof cur, "%s", jobs);
    for (int round = 0; round <= retries; round++) {
        int pending = file_lines(cur); if (!pending) break;
        char rdir[PB], bt[PB]; snprintf(rdir, sizeof rdir, "%s/r%d", bdir, round);
        { char cmd[PB]; snprintf(cmd, sizeof cmd, "%s", rdir); mkdirs(cmd); }
        { char *a[8] = {cur, rdir, "--tokens", NULL, "--rows", NULL}; char s1[24], s2[24]; snprintf(s1, sizeof s1, "%ld", tok); snprintf(s2, sizeof s2, "%ld", rows); a[3] = s1; a[5] = s2; v_batch(6, a); }
        int nbt = 0; for (;;) { snprintf(bt, sizeof bt, "%s/b%03d.tsv", rdir, nbt + 1); if (access(bt, R_OK)) break; nbt++; }
        ui_log("batch round %d: %d rows in %d batches", round, pending, nbt); ui_stage("batch", nbt, nbt);
        for (int bi = 1; bi < 1000; bi++) {
            snprintf(bt, sizeof bt, "%s/b%03d.tsv", rdir, bi); if (access(bt, R_OK)) break;
            snprintf(p1, sizeof p1, "%s/b%03d.prompt.txt", rdir, bi); snprintf(p2, sizeof p2, "%s/b%03d.reply.txt", rdir, bi); snprintf(p3, sizeof p3, "%s/b%03d.parsed.tsv", rdir, bi);
            { char *a[4] = {"translate", bt, p1, NULL}; v_prompt(3, a); }
            ui_stage("translate", bi - 1, nbt);
            int rc = run_model(argc, argv, tr, tm, bw, "translate", p1, p2, ql, keydir);
            ui_log("translate batch %d/%d %s %s %.1fs rc=%d%s", bi, nbt, tr, tm, g_last_ms / 1000.0, rc, round ? " (repair)" : "");
            if (rc) snprintf(g_stage, sizeof g_stage, "error");
            if (rc == 3) { fprintf(stderr, "csv_lab pipeline: translator quota exhausted, stopping; resume by re-running (answer bank skips finished rows)\n"); round = retries + 1; break; }
            if (rc) { fprintf(stderr, "csv_lab pipeline: translate call failed rc=%d for %s\n", rc, bt); continue; }
            { char *a[5] = {"translate", p2, bt, p3, NULL}; v_parse(4, a); }
            /* append with source attached so lint can run standalone, replacing an older row for the same id */
            Tab pt; if (!tab_load(p3, &pt)) {
                Tab old; Tab jt; tab_load(jobs, &jt);
                char tmp[PB]; snprintf(tmp, sizeof tmp, "%s/trans.new", bw); FILE *o = fopen(tmp, "w");
                if (!tab_load(trans, &old)) { for (int i = 0; i < old.n; i++) if (!tab_find(&pt, old.r[i].f[0])) fprintf(o, "%s\t%s\t%s\t%s\n", old.r[i].f[0], fld(&old.r[i], 1), fld(&old.r[i], 2), fld(&old.r[i], 3)); tab_free(&old); }
                for (int i = 0; i < pt.n; i++) { const Rec *j = tab_find(&jt, pt.r[i].f[0]); fprintf(o, "%s\t%s\t%s\t%s\n", pt.r[i].f[0], fld(&pt.r[i], 1), fld(&pt.r[i], 2), j ? fld(j, 1) : ""); }
                fclose(o); rename(tmp, trans); tab_free(&pt); tab_free(&jt);
            }
        }
        ui_stage("lint", 0, g_rows);
        { char *a[8]; int n = 0; a[n++] = trans; a[n++] = lintf; a[n++] = "--expect"; a[n++] = jobs; if (ref) { a[n++] = "--ref"; a[n++] = (char *)ref; } v_lint(n, a); }
        ui_log("lint round %d: %d rows checked", round, file_lines(lintf)); ui_stage("lint", file_lines(lintf), g_rows);
        /* rows that failed lint or are missing form the next round's job list, with the reason in the context column */
        Tab lt, jt; char nx[PB]; snprintf(nx, sizeof nx, "%s/retry%d.tsv", bw, round + 1);
        FILE *o = fopen(nx, "w");
        if (!tab_load(lintf, &lt) && !tab_load(jobs, &jt)) {
            for (int i = 0; i < lt.n; i++) if (!strcmp(fld(&lt.r[i], 1), "FAIL")) { const Rec *j = tab_find(&jt, lt.r[i].f[0]); if (j) fprintf(o, "%s\t%s\t%s FIX: %s\n", j->f[0], fld(j, 1), fld(j, 2), fld(&lt.r[i], 2)); }
            tab_free(&lt); tab_free(&jt);
        }
        fclose(o); snprintf(cur, sizeof cur, "%s", nx);
    }
    /* 6 score: lint-clean rows only, a different model than the translator */
    if (!no_judge) {
        Tab lt, tt; char sdir[PB]; snprintf(sdir, sizeof sdir, "%s/score", bw); mkdirs(sdir);
        char todo[PB]; snprintf(todo, sizeof todo, "%s/toscore.tsv", bw);
        FILE *o = fopen(todo, "w");
        if (!tab_load(lintf, &lt) && !tab_load(trans, &tt)) {
            for (int i = 0; i < lt.n; i++) if (!strcmp(fld(&lt.r[i], 1), "OK")) { const Rec *t = tab_find(&tt, lt.r[i].f[0]); if (t) fprintf(o, "%s\t%s\t%s\t%s\n", t->f[0], fld(t, 3), fld(t, 1), fld(t, 2)); }
            tab_free(&lt); tab_free(&tt);
        }
        fclose(o);
        for (int attempt = 0; attempt < 2; attempt++) {
            Tab td, st; tab_load(todo, &td); int have = !tab_load(scoref, &st);
            char rest[PB]; snprintf(rest, sizeof rest, "%s/toscore.rest.tsv", bw); FILE *ro = fopen(rest, "w");
            for (int i = 0; i < td.n; i++) if (!have || !tab_find(&st, td.r[i].f[0])) fprintf(ro, "%s\t%s\t%s\t%s\n", td.r[i].f[0], td.r[i].f[1], fld(&td.r[i], 2), fld(&td.r[i], 3));
            fclose(ro); if (have) tab_free(&st); tab_free(&td);
            if (!file_lines(rest)) break;
            Tab rt; tab_load(rest, &rt); int nb = 0, inb = 0; FILE *bf = NULL, *mf = NULL;
            for (int i = 0; i < rt.n; i++) {   /* s###.tsv = n, English, Chinese, pinyin (prompt layout); s###.map.tsv = n, jobid */
                char sb[PB], mp[PB];
                if (!bf || inb >= 20) { if (bf) { fclose(bf); fclose(mf); } snprintf(sb, sizeof sb, "%s/s%03d.tsv", sdir, ++nb); snprintf(mp, sizeof mp, "%s/s%03d.map.tsv", sdir, nb); bf = fopen(sb, "w"); mf = fopen(mp, "w"); inb = 0; }
                fprintf(bf, "%d\t%s\t%s\t%s\n", inb + 1, rt.r[i].f[1], fld(&rt.r[i], 2), fld(&rt.r[i], 3)); fprintf(mf, "%d\t%s\n", inb + 1, rt.r[i].f[0]); inb++;
            }
            if (bf) { fclose(bf); fclose(mf); }
            for (int bi = 1; bi <= nb; bi++) {
                char sbp[PB], sp1[PB], sp2[PB], sp3[PB], mp[PB];
                snprintf(sbp, sizeof sbp, "%s/s%03d.tsv", sdir, bi); snprintf(mp, sizeof mp, "%s/s%03d.map.tsv", sdir, bi);
                snprintf(sp1, sizeof sp1, "%s/s%03d.prompt.txt", sdir, bi); snprintf(sp2, sizeof sp2, "%s/s%03d.reply.txt", sdir, bi); snprintf(sp3, sizeof sp3, "%s/s%03d.parsed.tsv", sdir, bi);
                char *a[4] = {"score", sbp, sp1, NULL}; v_prompt(3, a);
                ui_stage("score", bi - 1, nb);
                int rc = run_model(argc, argv, jd, jm, bw, "score", sp1, sp2, ql, keydir);
                ui_log("score batch %d/%d %s %s %.1fs rc=%d", bi, nb, jd, jm, g_last_ms / 1000.0, rc);
                if (rc) { fprintf(stderr, "csv_lab pipeline: judge call failed rc=%d\n", rc); continue; }
                char *pa[5] = {"score", sp2, mp, sp3, NULL}; v_parse(4, pa);   /* the map file has the batch layout parse needs: n, jobid */
                Tab pt; if (!tab_load(sp3, &pt)) { FILE *so = fopen(scoref, "a"); for (int i = 0; i < pt.n; i++) fprintf(so, "%s\t%s\t%s\n", pt.r[i].f[0], pt.r[i].f[1], fld(&pt.r[i], 2)); fclose(so); tab_free(&pt); }
                ui_stage("score", bi, nb);
            }
            tab_free(&rt);
        }
    }
    ui_stage(!strcmp(g_stage, "error") ? "error" : "merge", g_rows, g_rows);
    { char *a[28]; int n = 0; a[n++] = argv[0]; a[n++] = argv[1]; a[n++] = bw; a[n++] = (char *)answers;
      if (argval(argc, argv, "--ui", &v)) { a[n++] = "--ui"; a[n++] = (char *)v; }
      if (argval(argc, argv, "--review", &v)) { a[n++] = "--review"; a[n++] = (char *)v; }
      if (argval(argc, argv, "--root", &v)) { a[n++] = "--root"; a[n++] = (char *)v; }
      if (argval(argc, argv, "--threshold", &v)) { a[n++] = "--threshold"; a[n++] = (char *)v; }
      if (argval(argc, argv, "--out", &v)) { a[n++] = "--out"; a[n++] = (char *)v; }
      if (fb) { a[n++] = "--feedback-dir"; a[n++] = (char *)fb; }
      if (ql) { a[n++] = "--quota-ledger"; a[n++] = (char *)ql; }
      a[n++] = "--translator"; a[n++] = (char *)tm; a[n++] = "--judge"; a[n++] = (char *)jm;
      return v_merge(n, a); }
}

/* ------------------------------------------------------------------ main */
int main(int argc, char **argv) {
    char *rp = realpath(argv[0], NULL); snprintf(g_exe, sizeof g_exe, "%s", rp ? rp : argv[0]); free(rp);
    if (argc < 2) { fprintf(stderr, "usage: csv_lab jobs|batch|prompt|parse|lint|call|quota|merge|ui|pipeline ... (see the header of csv_lab.c)\n"); return 2; }
    signal(SIGPIPE, SIG_IGN);
    const char *c = argv[1]; int n = argc - 2; char **a = argv + 2;
    if (!strcmp(c, "jobs")) return v_jobs(n, a);
    if (!strcmp(c, "batch")) return v_batch(n, a);
    if (!strcmp(c, "prompt")) return v_prompt(n, a);
    if (!strcmp(c, "parse")) return v_parse(n, a);
    if (!strcmp(c, "lint")) return v_lint(n, a);
    if (!strcmp(c, "call")) return v_call(n, a);
    if (!strcmp(c, "quota")) return v_quota(n, a);
    if (!strcmp(c, "merge")) return v_merge(n, a);
    if (!strcmp(c, "ui")) return v_ui(n, a);
    if (!strcmp(c, "pipeline")) return v_pipeline(n, a);
    fprintf(stderr, "csv_lab: unknown verb %s\n", c); return 2;
}
