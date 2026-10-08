/* quest_check - decides whether a delegated quest attempt is allowed to count (Quest Packet, ROBOT-WORKFORCE-GAMEPLAN section 3).
 * Usage:  quest_check <quest_dir> <worktree_dir> <target_branch_ref>      |      quest_check --selftest
 * Output: one `QCHECK|<PASS|FAIL>|<check>|<detail>` line per verdict, then `QCHECK|VERDICT|<PASS|FAIL>|checks=N|failed=M`.
 * Exit:   0 = packet valid; else a bitmask of failed classes: 1 LOCK, 2 SCOPE, 4 BASE, 8 BUDGET, 16 ATTEMPTS. 64 = usage error. --selftest: 0/1.
 * Checks: LOCK     <quest_dir>/LOCK.sha256 (sha256sum format, paths relative to harness/) matches every file under <quest_dir>/harness/ exactly
 *                  (edited byte, extra file, missing row, row for a deleted file all fail).
 *         SCOPE    changed (git diff --name-only <merge-base>) + untracked paths in the worktree must each match a row of <quest_dir>/scope.txt
 *                  (`dir/` = everything under, trailing `*` = prefix, else exact; `#` comments). Quest harness/, LOCK.sha256, scope.txt are ALWAYS out of scope.
 *         BASE     merge-base(HEAD, ref) must equal the current tip of ref (stale base fails, both hashes in the detail).
 *         BUDGET   <quest_dir>/budget.pdl rows `BUDGET | key | value`: max_attempts, max_seconds, max_quota_units (positive ints), escalate_to (a word).
 *         ATTEMPTS <quest_dir>/attempts/ entries numbered 001.. without gaps, count <= max_attempts.
 * Missing quest_dir / worktree / ref is a clean FAIL line. Subprocesses: fork+exec of git only (no shell, no system(), no popen). */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define PM 4096
static int NCHK = 0, NFAIL = 0;

static void qline(int ok, const char *check, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
#include <stdarg.h>
static void qline(int ok, const char *check, const char *fmt, ...) {
    char d[2 * PM]; va_list ap; va_start(ap, fmt); vsnprintf(d, sizeof d, fmt, ap); va_end(ap);
    for (char *p = d; *p; p++) if (*p == '|' || (unsigned char)*p < 32) *p = '?';
    NCHK++; if (!ok) NFAIL++;
    printf("QCHECK|%s|%s|%s\n", ok ? "PASS" : "FAIL", check, d);
}

/* ---- SHA-256 (FIPS 180-4) ---- */
typedef struct { uint32_t h[8]; uint64_t len; unsigned char buf[64]; size_t n; } Sha;
static const uint32_t K[64] = {
0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2 };
#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
static void sha_block(Sha *s, const unsigned char *p) {
    uint32_t w[64], a, b, c, d, e, f, g, h, t1, t2;
    for (int i = 0; i < 16; i++) w[i] = (uint32_t)p[4*i] << 24 | (uint32_t)p[4*i+1] << 16 | (uint32_t)p[4*i+2] << 8 | p[4*i+3];
    for (int i = 16; i < 64; i++) { uint32_t s0 = ROR(w[i-15], 7) ^ ROR(w[i-15], 18) ^ (w[i-15] >> 3), s1 = ROR(w[i-2], 17) ^ ROR(w[i-2], 19) ^ (w[i-2] >> 10); w[i] = w[i-16] + s0 + w[i-7] + s1; }
    a = s->h[0]; b = s->h[1]; c = s->h[2]; d = s->h[3]; e = s->h[4]; f = s->h[5]; g = s->h[6]; h = s->h[7];
    for (int i = 0; i < 64; i++) {
        t1 = h + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i];
        t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d; s->h[4] += e; s->h[5] += f; s->h[6] += g; s->h[7] += h;
}
static void sha_init(Sha *s) {
    static const uint32_t i0[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    memcpy(s->h, i0, sizeof i0); s->len = 0; s->n = 0;
}
static void sha_upd(Sha *s, const unsigned char *p, size_t k) {
    s->len += k;
    while (k) { size_t c = 64 - s->n; if (c > k) c = k; memcpy(s->buf + s->n, p, c); s->n += c; p += c; k -= c; if (s->n == 64) { sha_block(s, s->buf); s->n = 0; } }
}
static void sha_hex(Sha *s, char out[65]) {
    uint64_t bits = s->len * 8; unsigned char pad = 0x80, z = 0, lb[8];
    sha_upd(s, &pad, 1); while (s->n != 56) sha_upd(s, &z, 1);
    for (int i = 0; i < 8; i++) lb[i] = (unsigned char)(bits >> (56 - 8 * i));
    sha_upd(s, lb, 8);
    for (int i = 0; i < 8; i++) snprintf(out + 8 * i, 9, "%08x", s->h[i]);
}
static int sha_file(const char *path, char out[65]) {
    FILE *f = fopen(path, "rb"); unsigned char b[8192]; size_t k; Sha s;
    if (!f) return -1;
    sha_init(&s); while ((k = fread(b, 1, sizeof b, f)) > 0) sha_upd(&s, b, k);
    int bad = ferror(f); fclose(f); if (bad) return -1;
    sha_hex(&s, out); return 0;
}
static void sha_str(const char *str, char out[65]) { Sha s; sha_init(&s); sha_upd(&s, (const unsigned char *)str, strlen(str)); sha_hex(&s, out); }

static int selftest(void) {
    char h[65]; int bad = 0;
    sha_str("abc", h);
    if (strcmp(h, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")) bad = 1;
    qline(!bad, "SELFTEST", "sha256(abc)=%s", h);
    int b2 = 0; sha_str("", h);
    if (strcmp(h, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")) b2 = 1;
    qline(!b2, "SELFTEST", "sha256(empty)=%s", h);
    int b3 = 0; sha_str("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", h);   /* 2-block message */
    if (strcmp(h, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1")) b3 = 1;
    qline(!b3, "SELFTEST", "sha256(448-bit)=%s", h);
    printf("QCHECK|VERDICT|%s|checks=%d|failed=%d\n", NFAIL ? "FAIL" : "PASS", NCHK, NFAIL);
    return NFAIL ? 1 : 0;
}

/* ---- helpers ---- */
static char *rd_file(const char *path) {   /* whole file as malloc'd NUL-terminated text, or NULL */
    FILE *f = fopen(path, "rb"); size_t cap = 4096, n = 0, k; char *b;
    if (!f) return NULL;
    b = malloc(cap);
    while ((k = fread(b + n, 1, cap - n - 1, f)) > 0) { n += k; if (n + 1 >= cap) { cap *= 2; b = realloc(b, cap); } }
    b[n] = 0; fclose(f); return b;
}
static char *trim(char *s) { while (isspace((unsigned char)*s)) s++; char *e = s + strlen(s); while (e > s && isspace((unsigned char)e[-1])) *--e = 0; return s; }
static int is_dir(const char *p) { struct stat st; return stat(p, &st) == 0 && S_ISDIR(st.st_mode); }

/* fork+exec git (argv[0] = "git"); stdout captured (malloc'd, *len set), stderr discarded. returns exit code, or -1 */
static int run_git(char *const argv[], char **out, size_t *len) {
    int pfd[2]; pid_t pid; size_t cap = 4096, n = 0; ssize_t r; int st = 0; char *b = malloc(cap);
    if (pipe(pfd) != 0) { free(b); return -1; }
    fflush(stdout);
    if ((pid = fork()) == 0) {
        int dn = open("/dev/null", 2); dup2(pfd[1], 1); if (dn >= 0) { dup2(dn, 0); dup2(dn, 2); }
        close(pfd[0]); close(pfd[1]);
        execvp("git", argv); _exit(127);
    }
    close(pfd[1]);
    if (pid < 0) { close(pfd[0]); free(b); return -1; }
    while ((r = read(pfd[0], b + n, cap - n - 1)) > 0) { n += (size_t)r; if (n + 1 >= cap) { cap *= 2; b = realloc(b, cap); } }
    close(pfd[0]); waitpid(pid, &st, 0); b[n] = 0;
    if (out) *out = b; else free(b);
    if (len) *len = n;
    return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}
static int git1(const char *wt, const char *a, const char *b, const char *c, const char *d, char *res, size_t rn) {   /* git -C wt a [b [c [d]]] -> first line of stdout */
    char *av[10]; int n = 0; char *o = NULL;
    av[n++] = "git"; av[n++] = "-C"; av[n++] = (char *)wt; av[n++] = (char *)a;
    if (b) av[n++] = (char *)b;
    if (c) av[n++] = (char *)c;
    if (d) av[n++] = (char *)d;
    av[n] = NULL;
    int rc = run_git(av, &o, NULL);
    if (res) { snprintf(res, rn, "%s", o ? o : ""); char *nl = strchr(res, '\n'); if (nl) *nl = 0; }
    free(o); return rc;
}

/* ---- LOCK ---- */
typedef struct { char **v; int n, cap; } List;
static void lpush(List *l, const char *s) { if (l->n == l->cap) { l->cap = l->cap ? l->cap * 2 : 16; l->v = realloc(l->v, (size_t)l->cap * sizeof *l->v); } l->v[l->n++] = strdup(s); }
static int lfind(List *l, const char *s) { for (int i = 0; i < l->n; i++) if (!strcmp(l->v[i], s)) return i; return -1; }
static int walk(const char *root, const char *rel, List *files, int *bad) {   /* regular files into files (relative to root); symlinks/specials set *bad */
    char dir[PM], p[PM * 2]; DIR *d; struct dirent *e;
    snprintf(dir, sizeof dir, "%s%s%s", root, rel[0] ? "/" : "", rel);
    if (!(d = opendir(dir))) return -1;
    while ((e = readdir(d))) {
        struct stat st;
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char r2[PM]; snprintf(r2, sizeof r2, "%s%s%s", rel, rel[0] ? "/" : "", e->d_name);
        snprintf(p, sizeof p, "%s/%s", root, r2);
        if (lstat(p, &st) != 0) { (*bad)++; qline(0, "LOCK", "cannot stat harness/%s", r2); continue; }
        if (S_ISDIR(st.st_mode)) walk(root, r2, files, bad);
        else if (S_ISREG(st.st_mode)) lpush(files, r2);
        else { (*bad)++; qline(0, "LOCK", "harness/%s is a symlink or special file", r2); }
    }
    closedir(d); return 0;
}
static int check_lock(const char *q) {
    char hd[PM], lp[PM], *txt, *save = NULL; List files = {0}, lrows = {0}, lhash = {0}; int bad = 0, nbad = 0;
    snprintf(hd, sizeof hd, "%s/harness", q); snprintf(lp, sizeof lp, "%s/LOCK.sha256", q);
    if (!is_dir(hd)) { qline(0, "LOCK", "no harness/ directory in %s", q); return 1; }
    if (!(txt = rd_file(lp))) { qline(0, "LOCK", "LOCK.sha256 missing or unreadable"); return 1; }
    walk(hd, "", &files, &nbad);
    for (char *ln = strtok_r(txt, "\n", &save); ln; ln = strtok_r(NULL, "\n", &save)) {
        char *t = trim(ln), *path;
        if (!*t || *t == '#') continue;
        int hex = 1; for (int i = 0; i < 64; i++) if (!isxdigit((unsigned char)t[i]) || isupper((unsigned char)t[i])) hex = 0;
        if (!hex || (t[64] != ' ' && t[64] != '\t')) { qline(0, "LOCK", "malformed lock row: %.60s", t); bad++; continue; }
        path = t + 65; if (*path == ' ' || *path == '*') path++;
        while (path[0] == '.' && path[1] == '/') path += 2;
        if (!*path) { qline(0, "LOCK", "lock row has no path"); bad++; continue; }
        if (lfind(&lrows, path) >= 0) { qline(0, "LOCK", "duplicate lock row for %s", path); bad++; continue; }
        t[64] = 0; lpush(&lrows, path); lpush(&lhash, t);
    }
    for (int i = 0; i < files.n; i++) {
        int j = lfind(&lrows, files.v[i]); char fp[PM * 2], got[65];
        if (j < 0) { qline(0, "LOCK", "harness file has no lock row (added): %s", files.v[i]); bad++; continue; }
        snprintf(fp, sizeof fp, "%s/%s", hd, files.v[i]);
        if (sha_file(fp, got) != 0) { qline(0, "LOCK", "cannot read harness/%s", files.v[i]); bad++; continue; }
        if (strcmp(got, lhash.v[j])) { qline(0, "LOCK", "hash mismatch (edited) %s: locked %.12s actual %.12s", files.v[i], lhash.v[j], got); bad++; }
    }
    for (int j = 0; j < lrows.n; j++) if (lfind(&files, lrows.v[j]) < 0) { qline(0, "LOCK", "lock row has no file (removed): %s", lrows.v[j]); bad++; }
    if (!files.n && !bad) { qline(0, "LOCK", "harness/ holds no files: nothing to judge with"); bad++; }
    if (!bad && !nbad) qline(1, "LOCK", "%d harness file(s) match LOCK.sha256", files.n);
    free(txt); return (bad || nbad) ? 1 : 0;
}

/* ---- BASE + SCOPE (git) ---- */
static char BASE[128], TIP[128];
static int git_ready = 0, base_ok = 0;

static int check_base(const char *wt, const char *ref) {
    char top[PM];
    if (!is_dir(wt)) { qline(0, "BASE", "worktree_dir missing: %s", wt); return 1; }
    if (ref[0] == '-' || !ref[0]) { qline(0, "BASE", "bad ref: %s", ref); return 1; }
    if (git1(wt, "rev-parse", "--show-toplevel", NULL, NULL, top, sizeof top) != 0 || !top[0]) { qline(0, "BASE", "not a git worktree: %s", wt); return 1; }
    git_ready = 1;
    char spec[PM + 16]; snprintf(spec, sizeof spec, "%s^{commit}", ref);
    if (git1(wt, "rev-parse", "--verify", "-q", spec, TIP, sizeof TIP) != 0 || !TIP[0]) { qline(0, "BASE", "target ref missing: %s", ref); return 1; }
    if (git1(wt, "merge-base", "HEAD", ref, NULL, BASE, sizeof BASE) != 0 || !BASE[0]) { qline(0, "BASE", "no merge-base between HEAD and %s", ref); return 1; }
    base_ok = 1;
    if (strcmp(BASE, TIP)) { qline(0, "BASE", "stale base: merge-base %s != tip of %s %s", BASE, ref, TIP); return 1; }
    qline(1, "BASE", "merge-base == tip of %s (%.12s)", ref, TIP); return 0;
}
static int scope_match(const char *row, const char *path) {
    size_t n = strlen(row);
    if (n && row[n - 1] == '/') return !strncmp(path, row, n);
    if (n && row[n - 1] == '*') return !strncmp(path, row, n - 1);
    return !strcmp(path, row);
}
static void split_z(char *buf, size_t len, List *out) { size_t i = 0; while (i < len) { if (buf[i]) lpush(out, buf + i); i += strlen(buf + i) + 1; } }
static int check_scope(const char *q, const char *wt) {
    char sp[PM], *txt, *save = NULL, pre[PM] = ""; List rows = {0}, paths = {0}; int bad = 0;
    snprintf(sp, sizeof sp, "%s/scope.txt", q);
    if (!(txt = rd_file(sp))) { qline(0, "SCOPE", "scope.txt missing or unreadable"); return 1; }
    if (!git_ready || !base_ok) { qline(0, "SCOPE", "cannot diff: worktree/ref/merge-base unavailable"); free(txt); return 1; }
    for (char *ln = strtok_r(txt, "\n", &save); ln; ln = strtok_r(NULL, "\n", &save)) { char *t = trim(ln); if (*t && *t != '#') lpush(&rows, t); }
    /* protected paths = the packet's own harness/, LOCK.sha256, scope.txt, as repo-relative paths of the quest dir's repo */
    char *o = NULL; char *av[] = { "git", "-C", (char *)q, "rev-parse", "--show-prefix", NULL };
    if (run_git(av, &o, NULL) == 0 && o) { char *nl = strchr(o, '\n'); if (nl) *nl = 0; snprintf(pre, sizeof pre, "%s", o); }
    free(o);
    char *a1[] = { "git", "-C", (char *)wt, "diff", "--name-only", "--no-renames", "-z", BASE, NULL }, *a2[] = { "git", "-C", (char *)wt, "ls-files", "--others", "--exclude-standard", "--full-name", "-z", NULL };
    char *d1 = NULL, *d2 = NULL; size_t l1 = 0, l2 = 0;
    if (run_git(a1, &d1, &l1) != 0 || run_git(a2, &d2, &l2) != 0) { qline(0, "SCOPE", "git diff / ls-files failed"); free(txt); return 1; }
    split_z(d1, l1, &paths); split_z(d2, l2, &paths);
    for (int i = 0; i < paths.n; i++) {
        const char *p = paths.v[i]; char prot[PM * 2]; int hit = 0, why = 0;
        {
            snprintf(prot, sizeof prot, "%sharness/", pre);   if (!strncmp(p, prot, strlen(prot))) why = 1;
            snprintf(prot, sizeof prot, "%sLOCK.sha256", pre); if (!strcmp(p, prot)) why = 2;
            snprintf(prot, sizeof prot, "%sscope.txt", pre);   if (!strcmp(p, prot)) why = 3;
        }
        if (why) { qline(0, "SCOPE", "protected packet file touched (always out of scope): %s", p); bad++; continue; }
        for (int r = 0; r < rows.n && !hit; r++) hit = scope_match(rows.v[r], p);
        if (!hit) { qline(0, "SCOPE", "outside scope: %s", p); bad++; }
    }
    if (!bad) qline(1, "SCOPE", "%d changed/new path(s), all inside %d scope row(s)", paths.n, rows.n);
    free(txt); free(d1); free(d2); return bad ? 1 : 0;
}

/* ---- BUDGET + ATTEMPTS ---- */
static long MAXATT = -1;
static int pos_int(const char *s, long *v) { if (!*s || strlen(s) > 9) return 0; for (const char *p = s; *p; p++) if (!isdigit((unsigned char)*p)) return 0; *v = atol(s); return *v > 0; }
static int check_budget(const char *q) {
    static const char *keys[] = { "max_attempts", "max_seconds", "max_quota_units", "escalate_to" };
    char bp[PM], *txt, *save = NULL, seen[4] = {0, 0, 0, 0}; int bad = 0;
    snprintf(bp, sizeof bp, "%s/budget.pdl", q);
    if (!(txt = rd_file(bp))) { qline(0, "BUDGET", "budget.pdl missing or unreadable"); return 1; }
    for (char *ln = strtok_r(txt, "\n", &save); ln; ln = strtok_r(NULL, "\n", &save)) {
        char *t = trim(ln), *f[4]; int nf = 0;
        if (!*t || *t == '#') continue;
        for (char *c = t; nf < 4; ) { char *bar = strchr(c, '|'); f[nf++] = c; if (!bar) break; *bar = 0; c = bar + 1; }
        for (int i = 0; i < nf; i++) f[i] = trim(f[i]);
        if (strcmp(f[0], "BUDGET") || nf != 3) continue;
        for (int k = 0; k < 4; k++) if (!strcmp(f[1], keys[k])) {
            long v;
            if (seen[k]) { qline(0, "BUDGET", "duplicate key %s", keys[k]); bad++; break; }
            seen[k] = 1;
            if (k == 3) { int w = f[2][0] != 0; for (char *p = f[2]; *p; p++) if (!isalnum((unsigned char)*p) && *p != '_' && *p != '-') w = 0; if (!w) { qline(0, "BUDGET", "escalate_to must be a word, got '%s'", f[2]); bad++; } }
            else if (!pos_int(f[2], &v)) { qline(0, "BUDGET", "%s must be a positive integer, got '%s'", keys[k], f[2]); bad++; }
            else if (k == 0) MAXATT = v;
            break;
        }
    }
    for (int k = 0; k < 4; k++) if (!seen[k]) { qline(0, "BUDGET", "missing key %s", keys[k]); bad++; }
    if (!bad) qline(1, "BUDGET", "all 4 keys present and valid (max_attempts=%ld)", MAXATT);
    free(txt); return bad ? 1 : 0;
}
static int cmpl(const void *a, const void *b) { long x = *(const long *)a, y = *(const long *)b; return x < y ? -1 : x > y; }
static int check_attempts(const char *q) {
    char ap[PM]; DIR *d; struct dirent *e; long nums[4096]; int n = 0, bad = 0;
    snprintf(ap, sizeof ap, "%s/attempts", q);
    if (!is_dir(ap)) { qline(1, "ATTEMPTS", "no attempts/ directory (0 attempts)"); return 0; }
    if (!(d = opendir(ap))) { qline(0, "ATTEMPTS", "attempts/ unreadable"); return 1; }
    while ((e = readdir(d))) {
        long v; if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        if (strlen(e->d_name) < 3 || !pos_int(e->d_name, &v)) { qline(0, "ATTEMPTS", "entry not numbered 001..: %s", e->d_name); bad++; continue; }
        if (n < 4096) nums[n++] = v;
    }
    closedir(d);
    qsort(nums, (size_t)n, sizeof *nums, cmpl);
    for (int i = 0; i < n; i++) if (nums[i] != i + 1) { qline(0, "ATTEMPTS", "gap or duplicate: expected %03d, found %03ld", i + 1, nums[i]); bad++; break; }
    if (MAXATT < 0) { qline(0, "ATTEMPTS", "cannot compare count %d: no valid max_attempts", n); bad++; }
    else if (n > MAXATT) { qline(0, "ATTEMPTS", "%d attempts exceeds max_attempts=%ld", n, MAXATT); bad++; }
    if (!bad) qline(1, "ATTEMPTS", "%d attempt(s), numbered 001.. without gaps, within max_attempts=%ld", n, MAXATT);
    return bad ? 1 : 0;
}

int main(int argc, char **argv) {
    int rc = 0;
    unsetenv("GIT_DIR"); unsetenv("GIT_WORK_TREE"); unsetenv("GIT_INDEX_FILE");
    if (argc == 2 && !strcmp(argv[1], "--selftest")) return selftest();
    if (argc != 4) { fprintf(stderr, "usage: quest_check <quest_dir> <worktree_dir> <target_branch_ref> | --selftest\n"); return 64; }
    const char *q = argv[1], *wt = argv[2], *ref = argv[3];
    int qok = is_dir(q);
    if (!qok) { qline(0, "LOCK", "quest_dir missing: %s", q); qline(0, "SCOPE", "quest_dir missing"); qline(0, "BUDGET", "quest_dir missing"); qline(0, "ATTEMPTS", "quest_dir missing"); rc |= 1 | 2 | 8 | 16; if (check_base(wt, ref)) rc |= 4; }
    else {
        if (check_lock(q)) rc |= 1;
        if (check_base(wt, ref)) rc |= 4;
        if (check_scope(q, wt)) rc |= 2;
        if (check_budget(q)) rc |= 8;
        if (check_attempts(q)) rc |= 16;
    }
    printf("QCHECK|VERDICT|%s|checks=%d|failed=%d\n", rc ? "FAIL" : "PASS", NCHK, NFAIL);
    return rc;
}
