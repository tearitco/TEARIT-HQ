/* trunk_op - the house standard for shared, versioned data: one TRUNK file and per-owner BRANCH files that store only their own changes
 * (BOOK 02-architecture/TRUNK-AND-BRANCH-SHARED-DATA-STANDARD-2026-11-16.md, decision D87). Plain rows, append-only, replayed; NO git, no network, no shell.
 *
 * Usage: trunk_op <file> <verb> [args] [--as <owner>] [--now-ms N] [--max-bytes N] [--private]
 *   init-trunk <trunk> | init-branch <branch> <trunk path>      create a file (refused if it exists); --as = its owner
 *   get <key> | list | status | verify                          read (any owner)
 *   put <key> <value> [--private] | del <key>                    write (only the file's OWNER, --as must match); a trunk refuses --private
 *   sync                                                         branch: record that it has seen the trunk's newest row (idempotent)
 *   publish <key>                                                branch row -> trunk (only the TRUNK's owner; refuses a private row)
 *   compact                                                      branch: drop rows equal to the trunk's value (verified identical view before and after)
 * Row: <id>|<op>|<key>|<value>|<flags>   id = <ms>-<owner>-<n>, ops PUT DEL OWNER TRUNK HEAD; '|' in a value is \p, newline \n, backslash \\.
 *   branch header: 0|OWNER|<owner>||   0|TRUNK|<trunk path>|0|     trunk header: 0|OWNER|<owner>||
 * Reading a key: the BRANCH's latest row for it wins (a PUT gives the value, a DEL hides it), else the TRUNK's latest row. A later trunk edit never clobbers a branch override.
 * Every file has a sidecar <file>.seal (bytes + sha256 of the whole file) rewritten after each op write: `verify` fails on any edit made outside this op (a row edited in place, a deleted row, a hand-appended row).
 * Caps: a value is at most 4000 bytes; a file refuses an append past --max-bytes (default 1048576; rotation is a later step).
 * Exit: 0 ok, 1 refused or not found (one line on stdout, nothing written), 2 usage. Prints OK-style lines starting with a verb name so a harness can match them. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

#define PN 4352
#define VMAX 4000
typedef struct { char id[96], op[8], key[256], flags[64]; char *val; } Row;
typedef struct { Row *r; int n, cap; char path[PN]; long bytes; } Book;

static long long NOW = 0; static long MAXB = 1048576; static char AS[64] = "anon"; static int PRIV = 0;

/* ---- sha256 (self-contained) ---- */
typedef struct { unsigned int h[8]; unsigned long long len; unsigned char buf[64]; int bl; } Sha;
static const unsigned int K[64] = {0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
static void sha_block(Sha *s, const unsigned char *p) {
    unsigned int w[64], a, b, c, d, e, f, g, h, t1, t2; int i;
    for (i = 0; i < 16; i++) w[i] = (unsigned)p[i*4] << 24 | (unsigned)p[i*4+1] << 16 | (unsigned)p[i*4+2] << 8 | p[i*4+3];
    for (i = 16; i < 64; i++) { unsigned s0 = ROR(w[i-15], 7) ^ ROR(w[i-15], 18) ^ (w[i-15] >> 3), s1 = ROR(w[i-2], 17) ^ ROR(w[i-2], 19) ^ (w[i-2] >> 10); w[i] = w[i-16] + s0 + w[i-7] + s1; }
    a = s->h[0]; b = s->h[1]; c = s->h[2]; d = s->h[3]; e = s->h[4]; f = s->h[5]; g = s->h[6]; h = s->h[7];
    for (i = 0; i < 64; i++) { t1 = h + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i]; t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c)); h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2; }
    s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d; s->h[4] += e; s->h[5] += f; s->h[6] += g; s->h[7] += h;
}
static void sha_init(Sha *s) { static const unsigned int i0[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19}; memcpy(s->h, i0, sizeof i0); s->len = 0; s->bl = 0; }
static void sha_add(Sha *s, const unsigned char *p, size_t n) { s->len += n; while (n--) { s->buf[s->bl++] = *p++; if (s->bl == 64) { sha_block(s, s->buf); s->bl = 0; } } }
static void sha_hex(Sha *s, char *out) { unsigned long long bits = s->len * 8; unsigned char pad = 0x80, z = 0; sha_add(s, &pad, 1); while (s->bl != 56) sha_add(s, &z, 1); for (int i = 7; i >= 0; i--) { unsigned char b = (unsigned char)(bits >> (i * 8)); sha_add(s, &b, 1); } for (int i = 0; i < 8; i++) sprintf(out + i * 8, "%08x", s->h[i]); }
static int file_sha(const char *path, char *hex, long *bytes) { FILE *f = fopen(path, "rb"); if (!f) return 0; Sha s; sha_init(&s); unsigned char b[4096]; size_t n; long t = 0; while ((n = fread(b, 1, sizeof b, f)) > 0) { sha_add(&s, b, n); t += (long)n; } fclose(f); sha_hex(&s, hex); *bytes = t; return 1; }

/* ---- rows ---- */
static void esc(const char *in, char *out, size_t cap) { size_t o = 0; for (; *in && o + 3 < cap; in++) { if (*in == '\\') { out[o++] = '\\'; out[o++] = '\\'; } else if (*in == '|') { out[o++] = '\\'; out[o++] = 'p'; } else if (*in == '\n') { out[o++] = '\\'; out[o++] = 'n'; } else out[o++] = *in; } out[o] = 0; }
static void unesc(char *s) { char *w = s; for (char *r = s; *r; r++) { if (*r == '\\' && r[1]) { r++; *w++ = (*r == 'p') ? '|' : (*r == 'n') ? '\n' : *r; } else *w++ = *r; } *w = 0; }
static int word_ok(const char *s, size_t max) { size_t n = strlen(s); if (!n || n >= max) return 0; for (; *s; s++) if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') || *s == '_' || *s == '.' || *s == '-')) return 0; return 1; }
static long long now_ms(void) { if (NOW) return NOW; struct timespec t; clock_gettime(CLOCK_REALTIME, &t); return (long long)t.tv_sec * 1000 + t.tv_nsec / 1000000; }
static void add_row(Book *b, const char *id, const char *op, const char *key, const char *val, const char *flags) {
    if (b->n == b->cap) { b->cap = b->cap ? b->cap * 2 : 64; b->r = realloc(b->r, (size_t)b->cap * sizeof(Row)); if (!b->r) exit(2); }
    Row *r = &b->r[b->n++]; snprintf(r->id, sizeof r->id, "%s", id); snprintf(r->op, sizeof r->op, "%s", op); snprintf(r->key, sizeof r->key, "%s", key); snprintf(r->flags, sizeof r->flags, "%s", flags); r->val = strdup(val);
}
static void free_book(Book *b) { for (int i = 0; i < b->n; i++) free(b->r[i].val); free(b->r); b->r = NULL; b->n = b->cap = 0; }
/* returns the number of invalid lines (-1 = cannot open) */
static int load(Book *b, const char *path) {
    memset(b, 0, sizeof *b); snprintf(b->path, sizeof b->path, "%s", path); FILE *f = fopen(path, "r"); if (!f) return -1;
    static char ln[9000]; int bad = 0;
    while (fgets(ln, sizeof ln, f)) {
        size_t L = strlen(ln); if (L && ln[L - 1] == '\n') ln[--L] = 0; else if (L >= sizeof ln - 1) { bad++; int ch; while ((ch = fgetc(f)) != EOF && ch != '\n') ; continue; }
        char *p[5]; int k = 0; p[k++] = ln; for (char *c = ln; *c && k < 5; c++) if (*c == '|') { *c = 0; p[k++] = c + 1; }
        if (k != 5 || !p[0][0] || !p[1][0]) { bad++; continue; }
        char val[VMAX + 8]; snprintf(val, sizeof val, "%s", p[3]); unesc(val); add_row(b, p[0], p[1], p[2], val, p[4]);
    }
    fclose(f); struct stat st; b->bytes = stat(path, &st) == 0 ? (long)st.st_size : 0; return bad;
}
static const char *hdr(Book *b, const char *op) { for (int i = 0; i < b->n; i++) if (!strcmp(b->r[i].op, op) && !strcmp(b->r[i].id, "0")) return b->r[i].key; return NULL; }
static const char *hdr_val(Book *b, const char *op) { for (int i = 0; i < b->n; i++) if (!strcmp(b->r[i].op, op) && !strcmp(b->r[i].id, "0")) return b->r[i].val; return NULL; }
static int is_branch(Book *b) { return hdr(b, "TRUNK") != NULL; }
/* the latest PUT/DEL row for a key (NULL = none) */
static Row *last_row(Book *b, const char *key) { Row *r = NULL; for (int i = 0; i < b->n; i++) if ((!strcmp(b->r[i].op, "PUT") || !strcmp(b->r[i].op, "DEL")) && !strcmp(b->r[i].key, key)) r = &b->r[i]; return r; }
static const char *last_id(Book *b) { const char *id = "0"; for (int i = 0; i < b->n; i++) if (!strcmp(b->r[i].op, "PUT") || !strcmp(b->r[i].op, "DEL")) id = b->r[i].id; return id; }
/* resolve a key through the branch then the trunk: 1 = value in *out (points into a book), 0 = none/hidden */
static int resolve(Book *br, Book *tr, const char *key, const char **out, const char **src) {
    Row *r = br ? last_row(br, key) : NULL; if (r) { if (!strcmp(r->op, "DEL")) return 0; *out = r->val; *src = "branch"; return 1; }
    r = tr ? last_row(tr, key) : NULL; if (r) { if (!strcmp(r->op, "DEL")) return 0; *out = r->val; *src = "trunk"; return 1; }
    return 0;
}
/* ---- seal + append ---- */
static void write_seal(const char *path) { char sp[PN + 8], hex[80]; long bytes; if (!file_sha(path, hex, &bytes)) return; snprintf(sp, sizeof sp, "%s.seal", path); FILE *f = fopen(sp, "w"); if (f) { fprintf(f, "bytes=%ld sha256=%s\n", bytes, hex); fclose(f); } }
static int append_row(const char *path, const char *id, const char *op, const char *key, const char *val, const char *flags) {
    char ev[2 * VMAX + 16], ek[600]; esc(val, ev, sizeof ev); esc(key, ek, sizeof ek); struct stat st; long sz = stat(path, &st) == 0 ? (long)st.st_size : 0;
    if (sz + (long)strlen(id) + (long)strlen(ev) + (long)strlen(ek) + 24 > MAXB) { printf("REFUSED the file is at its size cap (%ld bytes): rotation is a later step\n", MAXB); return 0; }
    FILE *f = fopen(path, "a"); if (!f) { printf("REFUSED cannot write %s\n", path); return 0; }
    fprintf(f, "%s|%s|%s|%s|%s\n", id, op, ek, ev, flags); fclose(f); write_seal(path); return 1;
}
static void new_id(Book *b, char *id, size_t n) { snprintf(id, n, "%lld-%s-%d", now_ms(), AS, b->n + 1); }
static int owner_ok(Book *b, const char *what) { const char *o = hdr(b, "OWNER"); if (!o || strcmp(o, AS)) { printf("REFUSED only the %s owner (%s) may write it (--as %s)\n", what, o ? o : "?", AS); return 0; } return 1; }
static int load_pair(const char *file, Book *br, Book *tr, int *branch) {
    int bad = load(br, file); if (bad < 0) { printf("REFUSED no such file %s\n", file); return 0; }
    *branch = is_branch(br); memset(tr, 0, sizeof *tr);
    if (*branch) { if (load(tr, hdr_val(br, "TRUNK") ? hdr(br, "TRUNK") : "") < 0) { printf("REFUSED the trunk %s cannot be read\n", hdr(br, "TRUNK")); return 0; } }
    return 1;
}
/* the merged view as one string of "key=value" lines SORTED BY KEY (so two views with the same content compare equal whichever file a key lives in), for compare and byte counts */
static int cmp_line(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }
static size_t view(Book *br, Book *tr, char *out, size_t cap) {
    char **ls = NULL; int nl = 0, cl = 0; Book *bk[2] = { tr, br }; out[0] = 0;
    for (int s = 0; s < 2; s++) {
        if (!bk[s]) continue;
        for (int i = 0; i < bk[s]->n; i++) {
            Row *r = &bk[s]->r[i]; if (strcmp(r->op, "PUT") && strcmp(r->op, "DEL")) continue;
            int seen = 0; for (int j = 0; j < i; j++) if (!strcmp(bk[s]->r[j].key, r->key) && (!strcmp(bk[s]->r[j].op, "PUT") || !strcmp(bk[s]->r[j].op, "DEL"))) { seen = 1; break; }
            if (seen) continue;
            if (s == 0 && br && last_row(br, r->key)) continue;
            const char *v, *src; if (!resolve(br, tr, r->key, &v, &src)) continue;
            if (nl == cl) { cl = cl ? cl * 2 : 32; ls = realloc(ls, (size_t)cl * sizeof *ls); if (!ls) exit(2); }
            char *line = malloc(strlen(r->key) + strlen(v) + 3); if (!line) exit(2); sprintf(line, "%s=%s\n", r->key, v); ls[nl++] = line;
        }
    }
    if (nl > 1) qsort(ls, (size_t)nl, sizeof *ls, cmp_line);
    size_t o = 0; for (int i = 0; i < nl; i++) { if (o + strlen(ls[i]) + 1 < cap) { memcpy(out + o, ls[i], strlen(ls[i])); o += strlen(ls[i]); out[o] = 0; } free(ls[i]); }
    free(ls); return o;
}

static int v_get(Book *br, Book *tr, const char *key) { const char *v, *src; if (!resolve(br, tr, key, &v, &src)) { printf("NOT-FOUND %s\n", key); return 1; } printf("%s\n", v); return 0; }
static int v_list(Book *br, Book *tr) { Book *bk[2] = { tr, br }; int n = 0; for (int s = 0; s < 2; s++) { if (!bk[s]) continue; for (int i = 0; i < bk[s]->n; i++) { Row *r = &bk[s]->r[i]; if (strcmp(r->op, "PUT") && strcmp(r->op, "DEL")) continue; int seen = 0; for (int j = 0; j < i; j++) if (!strcmp(bk[s]->r[j].key, r->key) && (!strcmp(bk[s]->r[j].op, "PUT") || !strcmp(bk[s]->r[j].op, "DEL"))) { seen = 1; break; } if (seen) continue; if (s == 0 && br && last_row(br, r->key)) continue; const char *v, *src; if (resolve(br, tr, r->key, &v, &src)) { char ev[2 * VMAX + 16]; esc(v, ev, sizeof ev); printf("%s|%s|%s\n", r->key, ev, src); n++; } } } printf("LIST-END %d\n", n); return 0; }
static int v_put(const char *file, Book *br, Book *tr, int branch, const char *key, const char *val) {
    if (!word_ok(key, 200)) { printf("REFUSED bad key (letters, digits, _ . - only, 1 to 199)\n"); return 1; }
    if (strlen(val) > VMAX) { printf("REFUSED value over %d bytes\n", VMAX); return 1; }
    if (!owner_ok(br, branch ? "branch" : "trunk")) return 1;
    if (PRIV && !branch) { printf("REFUSED a trunk holds no private rows\n"); return 1; }
    char id[96]; new_id(br, id, sizeof id); (void)tr; if (!append_row(file, id, "PUT", key, val, PRIV ? "private" : "")) return 1; printf("PUT-OK %s %s\n", key, id); return 0;
}
static int v_del(const char *file, Book *br, int branch, const char *key) { if (!owner_ok(br, branch ? "branch" : "trunk")) return 1; char id[96]; new_id(br, id, sizeof id); if (!append_row(file, id, "DEL", key, "", "")) return 1; printf("DEL-OK %s %s\n", key, id); return 0; }
static int v_status(Book *br, Book *tr, int branch) {
    long full = branch ? tr->bytes : br->bytes; int rows = 0;      /* a FULL COPY of the trunk is what a branch avoids: compare with the trunk file size (small files show the header overhead honestly) */ for (int i = 0; i < br->n; i++) if (!strcmp(br->r[i].op, "PUT") || !strcmp(br->r[i].op, "DEL")) rows++;
    if (!branch) { printf("STATUS trunk owner=%s rows=%d bytes=%ld head=%s\n", hdr(br, "OWNER") ? hdr(br, "OWNER") : "?", rows, br->bytes, last_id(br)); return 0; }
    const char *head = hdr_val(br, "TRUNK"); for (int i = 0; i < br->n; i++) if (!strcmp(br->r[i].op, "HEAD")) head = br->r[i].key; if (!head) head = "0";
    int behind = 0, seen = !strcmp(head, "0"); for (int i = 0; i < tr->n; i++) { if (strcmp(tr->r[i].op, "PUT") && strcmp(tr->r[i].op, "DEL")) continue; if (seen) behind++; else if (!strcmp(tr->r[i].id, head)) seen = 1; }
    printf("STATUS branch owner=%s rows=%d bytes=%ld full_copy_bytes=%ld saved=%ld head=%s trunk_head=%s behind=%d\n", hdr(br, "OWNER") ? hdr(br, "OWNER") : "?", rows, br->bytes, full, full - br->bytes, head, last_id(tr), behind); return 0;
}
static int v_sync(const char *file, Book *br, Book *tr) {
    if (!owner_ok(br, "branch")) return 1;
    const char *head = hdr_val(br, "TRUNK"); for (int i = 0; i < br->n; i++) if (!strcmp(br->r[i].op, "HEAD")) head = br->r[i].key; if (!head) head = "0";
    const char *th = last_id(tr); int behind = 0, seen = !strcmp(head, "0"); for (int i = 0; i < tr->n; i++) { if (strcmp(tr->r[i].op, "PUT") && strcmp(tr->r[i].op, "DEL")) continue; if (seen) behind++; else if (!strcmp(tr->r[i].id, head)) seen = 1; }
    if (!behind && !strcmp(head, th)) { printf("SYNC-OK behind=0 nothing to do\n"); return 0; }
    if (!behind && strcmp(th, "0") == 0) { printf("SYNC-OK behind=0 nothing to do\n"); return 0; }
    char id[96]; new_id(br, id, sizeof id); if (!append_row(file, id, "HEAD", th, "", "")) return 1; printf("SYNC-OK behind=%d head=%s\n", behind, th); return 0;
}
static int v_publish(Book *br, Book *tr, const char *key) {
    Row *r = last_row(br, key); if (!r || strcmp(r->op, "PUT")) { printf("REFUSED no branch row for %s to publish\n", key); return 1; }
    if (!strcmp(r->flags, "private")) { printf("REFUSED %s is private and is never published\n", key); return 1; }
    if (!owner_ok(tr, "trunk")) return 1;
    char id[96]; new_id(tr, id, sizeof id); if (!append_row(tr->path, id, "PUT", key, r->val, "")) return 1; printf("PUBLISH-OK %s %s\n", key, id); return 0;
}
static int v_verify(const char *file) {
    Book b; int bad = load(&b, file); if (bad < 0) { printf("VERIFY-FAIL cannot read\n"); return 1; } if (bad) { printf("VERIFY-FAIL %d malformed row(s)\n", bad); free_book(&b); return 1; }
    char sp[PN + 8], ln[200], hex[80]; long bytes; snprintf(sp, sizeof sp, "%s.seal", file); FILE *f = fopen(sp, "r"); if (!f) { printf("VERIFY-FAIL no seal: the file was not written by trunk_op\n"); free_book(&b); return 1; }
    long sb = -1; char sh[80] = ""; if (fgets(ln, sizeof ln, f)) sscanf(ln, "bytes=%ld sha256=%79s", &sb, sh); fclose(f);
    if (!file_sha(file, hex, &bytes) || sb != bytes || strcmp(sh, hex)) { printf("VERIFY-FAIL the file was changed outside trunk_op (sealed %ld bytes, now %ld): a row was edited, removed or hand-appended\n", sb, bytes); free_book(&b); return 1; }
    printf("VERIFY-OK rows=%d bytes=%ld\n", b.n, bytes); free_book(&b); return 0;
}
static int v_compact(const char *file, Book *br, Book *tr) {
    if (!owner_ok(br, "branch")) return 1;
    static char before[400000], after[400000]; view(br, tr, before, sizeof before);
    char tmp[PN + 8]; snprintf(tmp, sizeof tmp, "%s.tmp", file); FILE *f = fopen(tmp, "w"); if (!f) { printf("REFUSED cannot write the temporary file\n"); return 1; }
    int kept = 0, dropped = 0; Book nb; memset(&nb, 0, sizeof nb);
    for (int i = 0; i < br->n; i++) { Row *r = &br->r[i]; char ev[2 * VMAX + 16], ek[600]; int keep = 1;
        if (!strcmp(r->op, "PUT") || !strcmp(r->op, "DEL")) {
            if (last_row(br, r->key) != r) keep = 0;                                  /* superseded by a later row of the same branch */
            else if (!strcmp(r->op, "PUT") && strcmp(r->flags, "private")) { Row *t = last_row(tr, r->key); if (t && !strcmp(t->op, "PUT") && !strcmp(t->val, r->val)) keep = 0; }   /* equal to the trunk's value: inherit instead */
            else if (!strcmp(r->op, "DEL")) { Row *t = last_row(tr, r->key); if (!t || !strcmp(t->op, "DEL")) keep = 0; }
        } else if (!strcmp(r->op, "HEAD")) { int later = 0; for (int j = i + 1; j < br->n; j++) if (!strcmp(br->r[j].op, "HEAD")) later = 1; if (later) keep = 0; }
        if (!keep) { dropped++; continue; }
        esc(r->val, ev, sizeof ev); esc(r->key, ek, sizeof ek); fprintf(f, "%s|%s|%s|%s|%s\n", r->id, r->op, ek, ev, r->flags); add_row(&nb, r->id, r->op, r->key, r->val, r->flags); kept++; }
    fclose(f); view(&nb, tr, after, sizeof after);
    if (strcmp(before, after)) { remove(tmp); free_book(&nb); printf("REFUSED compaction would change the merged view: nothing written\n"); return 1; }
    long ob = br->bytes; if (rename(tmp, file)) { remove(tmp); free_book(&nb); printf("REFUSED cannot replace the file\n"); return 1; } write_seal(file); struct stat st; stat(file, &st); free_book(&nb);
    printf("COMPACT-OK before=%ld after=%ld dropped=%d kept=%d (the merged view is identical)\n", ob, (long)st.st_size, dropped, kept); return 0;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: trunk_op <file> init-trunk|init-branch <trunk>|get <key>|list|put <key> <value>|del <key>|sync|publish <key>|compact|status|verify [--as OWNER] [--now-ms N] [--max-bytes N] [--private]\n"); return 2; }
    const char *a[8]; int na = 0;
    for (int i = 1; i < argc; i++) { if (!strcmp(argv[i], "--as") && i + 1 < argc) snprintf(AS, sizeof AS, "%s", argv[++i]); else if (!strcmp(argv[i], "--now-ms") && i + 1 < argc) NOW = atoll(argv[++i]); else if (!strcmp(argv[i], "--max-bytes") && i + 1 < argc) MAXB = atol(argv[++i]); else if (!strcmp(argv[i], "--private")) PRIV = 1; else if (na < 8) a[na++] = argv[i]; }
    if (na < 2) return 2;
    if (!word_ok(AS, 60)) { printf("REFUSED bad --as owner id\n"); return 1; }
    const char *file = a[0], *verb = a[1]; Book br, tr; int branch = 0;
    if (!strcmp(verb, "init-trunk") || !strcmp(verb, "init-branch")) {
        if (access(file, F_OK) == 0) { printf("REFUSED %s already exists\n", file); return 1; }
        FILE *f = fopen(file, "w"); if (!f) { printf("REFUSED cannot create %s\n", file); return 1; } fprintf(f, "0|OWNER|%s||\n", AS);
        if (!strcmp(verb, "init-branch")) { if (na < 3) { fclose(f); remove(file); return 2; } if (access(a[2], R_OK)) { fclose(f); remove(file); printf("REFUSED the trunk %s does not exist\n", a[2]); return 1; } char et[PN * 2]; esc(a[2], et, sizeof et); fprintf(f, "0|TRUNK|%s|0|\n", et); }
        fclose(f); write_seal(file); printf("INIT-OK %s owner=%s\n", verb, AS); return 0;
    }
    if (!strcmp(verb, "verify")) return v_verify(file);
    if (!load_pair(file, &br, &tr, &branch)) return 1;
    int rc = 2; Book *view_br = branch ? &br : NULL, *view_tr = branch ? &tr : &br;
    if (!strcmp(verb, "get") && na >= 3) rc = v_get(view_br, view_tr, a[2]);
    else if (!strcmp(verb, "list")) rc = v_list(view_br, view_tr);
    else if (!strcmp(verb, "put") && na >= 4) rc = v_put(file, &br, &tr, branch, a[2], a[3]);
    else if (!strcmp(verb, "del") && na >= 3) rc = v_del(file, &br, branch, a[2]);
    else if (!strcmp(verb, "status")) rc = v_status(&br, &tr, branch);
    else if (!strcmp(verb, "sync")) { if (!branch) { printf("REFUSED sync is for a branch\n"); rc = 1; } else rc = v_sync(file, &br, &tr); }
    else if (!strcmp(verb, "publish") && na >= 3) { if (!branch) { printf("REFUSED publish is for a branch\n"); rc = 1; } else rc = v_publish(&br, &tr, a[2]); }
    else if (!strcmp(verb, "compact")) { if (!branch) { printf("REFUSED compact is for a branch\n"); rc = 1; } else rc = v_compact(file, &br, &tr); }
    free_book(&br); if (branch) free_book(&tr); return rc;
}
