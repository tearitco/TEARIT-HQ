/* harness_case_op - runs one .pdl CASE FILE and appends PASS/FAIL rows to a results ledger. The op a pal harness execs (prisc `exec` ignores a child's exit status and
 * discards its output, so a pal cannot assert by itself: assertions are rows written to a file, and harness_verdict_op turns them into a verdict).
 *
 * Usage: harness_case_op <cases.pdl> [house_root]      (house_root defaults to the case file's own folder walked up to the dir that holds `xyzfs`)
 * Case file (pipe-delimited `VERB | a | b | ...`, '#' comments; strings may use \n \t \p (a literal '|') and $T = the scratch dir, $HOUSE = the house root):
 *   RESULTS   | <path>                 results ledger, relative to the case file's folder (required, first)
 *   CASE      | <name>                 label prefix for the checks that follow
 *   SCRATCH   |                        create a scratch dir (/tmp/hc_XXXXXX) = $T
 *   MKDIR     | <path under $T>
 *   COPY      | <src under $HOUSE> | <dst under $T>      copies a file, keeps its mode
 *   WRITE     | <path under $T> | <text>                write a file (the text is everything after the path, so it may contain ' | ')
 *   RUN       | <argv0> | <arg1> | ...                   fork/exec (NO shell), stdout+stderr captured, exit code kept; fields are argv entries, so no quoting
 *   EXPECT_EXIT   | <n>            | <label>
 *   EXPECT_FILE   | <path> | <exact text> | <label>
 *   EXPECT_NOFILE | <path>         | <label>          EXPECT_EXISTS | <path> | <label>      the path exists (a touched, empty file counts)
 *   EXPECT_HAS    | <path> | <substring> | <label>       file contains it   (EXPECT_LACKS: file does not contain it)
 *   EXPECT_OUT    | <substring>    | <label>              last RUN's output contains it
 *   EXPECT_OUT_IS | <text> | <label>                    last RUN's output (trailing newlines trimmed) equals it exactly;  EXPECT_OUT_LACKS | <substring> | <label>
 *   ENV       | <NAME> | <value>                         setenv for every later RUN/SPAWN (an empty value unsets it)
 *   SPAWN     | <var> | <argv0> | <arg> ...              start a background process in its own process group, no shell; $<var> = its pid; killed (group) by CLEAN / at the end
 *   SLEEP     | <ms>
 *   WAIT_HAS  | <path under $T> | <substring> | <max_ms> | <label>     poll every 10 ms until the file contains it: PASS inside max_ms, else FAIL (the PASS row's detail is the wait)
 *   EXPECT_ALIVE | <pid or $var> | <label>   /   EXPECT_DEAD | <pid or $var> | <label>      kill(pid, 0)
 *   APPEND    | <path under $T> | <text>               append to a file (like WRITE, but keeps what is there)
 *   CAPTURE   | <var>                                    $var = the last RUN's output with trailing newlines trimmed
 *   SUMTREE   | <var> | <dir under $T>                    $var = a hash of every file under the dir (sorted path + content): "did this tree change?"
 *   EXPECT_EQ | <a> | <b> | <label>      /   EXPECT_NE | <a> | <b> | <label>      compare two expanded strings (e.g. two $vars)
 *   CLEAN     |                        kill every SPAWNed group, remove the scratch dir (only one this op created)
 * Variables: $T, $HOUSE, and every SPAWN var ($name = [A-Za-z0-9_]+; an unknown $name stays literal).
 * Results rows (append-only): RUN|<epoch_ms>|<case file>   then   PASS|<case>/<label>   or   FAIL|<case>/<label>|<what was seen>
 * Nothing here touches the live house unless a case file names a live path itself: always work under $T. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <ftw.h>
#include <ctype.h>
#include <libgen.h>
#include <signal.h>
#include <fcntl.h>

#define P 4096
#define FMAX 128   /* fields per row; a WRITE of a whole .pdl is one row with many bars */
static char T[P], HOUSE[P], CASE[256] = "", RESULTS[P], OUT[16384];
static int last_exit = -1, made_scratch;
#define MAXVAR 32
static struct { char name[48], val[P]; } VARS[MAXVAR]; static int NVARS;
static pid_t SPAWNED[64]; static int NSPAWNED;
static const char *var_get(const char *name) {
    if (!strcmp(name, "T")) return T;
    if (!strcmp(name, "HOUSE")) return HOUSE;
    for (int i = 0; i < NVARS; i++) if (!strcmp(VARS[i].name, name)) return VARS[i].val;
    return NULL;
}
static void var_set(const char *name, const char *val) {
    for (int i = 0; i < NVARS; i++) if (!strcmp(VARS[i].name, name)) { snprintf(VARS[i].val, sizeof VARS[i].val, "%s", val); return; }
    if (NVARS < MAXVAR) { snprintf(VARS[NVARS].name, sizeof VARS[NVARS].name, "%s", name); snprintf(VARS[NVARS].val, sizeof VARS[NVARS].val, "%s", val); NVARS++; }
}
static void kill_spawned(void) { for (int i = 0; i < NSPAWNED; i++) { kill(-SPAWNED[i], SIGKILL); waitpid(SPAWNED[i], NULL, 0);   /* SIGKILL cannot be ignored, so a blocking reap is safe and leaves no zombie for EXPECT_DEAD */ } NSPAWNED = 0; }
static long long now_ms(void) { struct timeval tv; gettimeofday(&tv, NULL); return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000; }

static char *trim(char *s) { char *e; while (*s == ' ' || *s == '\t') s++; e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0; return s; }
/* expand \n \t and $T / $HOUSE / $spawnvar into out */
static void expand(const char *in, char *out, size_t n) {
    size_t o = 0;
    for (const char *p = in; *p && o + 2 < n; ) {
        if (p[0] == '\\' && p[1] == 'n') { out[o++] = '\n'; p += 2; }
        else if (p[0] == '\\' && p[1] == 't') { out[o++] = '\t'; p += 2; }
        else if (p[0] == '\\' && p[1] == 'p') { out[o++] = '|'; p += 2; }   /* \p = a literal bar (a field cannot contain one) */
        else if (p[0] == '$' && (isalnum((unsigned char)p[1]) || p[1] == '_')) {
            char nm[48]; size_t k = 0; const char *q = p + 1; const char *val;
            while ((isalnum((unsigned char)*q) || *q == '_') && k < sizeof nm - 1) nm[k++] = *q++;
            nm[k] = 0;
            if ((val = var_get(nm))) { o += (size_t)snprintf(out + o, n - o, "%s", val); p = q; }
            else out[o++] = *p++;
        }
        else out[o++] = *p++;
    }
    out[o] = 0;
}
static void row(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void row(const char *fmt, ...) {
    char buf[8192]; __builtin_va_list ap; __builtin_va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); __builtin_va_end(ap);
    FILE *f = RESULTS[0] ? fopen(RESULTS, "a") : NULL;
    if (f) { fputs(buf, f); fputc('\n', f); fclose(f); }
    puts(buf);
}
static void check(int ok, const char *label, const char *seen) {
    if (ok) row("PASS|%s/%s", CASE, label); else row("FAIL|%s/%s|%s", CASE, label, seen ? seen : "");
}
/* FNV-1a 64 over every regular file under dir: sorted relative path, then content. Enough to tell "changed or not"; not a security hash. */
static char (*TREE)[P]; static int NTREE, CTREE; static size_t TREELEN;
static int tree_cb(const char *p, const struct stat *sb, int t, struct FTW *w) {
    (void)sb; (void)w; if (t != FTW_F) return 0;
    if (NTREE == CTREE) { CTREE = CTREE ? CTREE * 2 : 256; TREE = realloc(TREE, (size_t)CTREE * P); if (!TREE) return 1; }
    snprintf(TREE[NTREE++], P, "%s", p + TREELEN); return 0;
}
static int tree_cmp(const void *a, const void *b) { return strcmp((const char *)a, (const char *)b); }
static void tree_hash(const char *dir, char *out, size_t n) {
    unsigned long long h = 1469598103934665603ULL; char path[P + 8]; unsigned char buf[8192];
    NTREE = 0; TREELEN = strlen(dir) + 1;
    nftw(dir, tree_cb, 16, FTW_PHYS);
    if (NTREE) qsort(TREE, (size_t)NTREE, P, tree_cmp);
    for (int i = 0; i < NTREE; i++) {
        FILE *f; size_t k; const unsigned char *c;
        for (c = (const unsigned char *)TREE[i]; *c; c++) { h ^= *c; h *= 1099511628211ULL; }
        h ^= 0; h *= 1099511628211ULL;
        snprintf(path, sizeof path, "%s/%s", dir, TREE[i]);
        if ((f = fopen(path, "rb"))) { while ((k = fread(buf, 1, sizeof buf, f)) > 0) for (size_t j = 0; j < k; j++) { h ^= buf[j]; h *= 1099511628211ULL; } fclose(f); }
        h ^= 1; h *= 1099511628211ULL;
    }
    snprintf(out, n, "%016llx", h);
}
static int rm_cb(const char *p, const struct stat *sb, int t, struct FTW *w) { (void)sb; (void)t; (void)w; return remove(p); }
static int rd_all(const char *path, char *out, size_t n) { FILE *f = fopen(path, "r"); size_t k; if (!f) return -1; k = fread(out, 1, n - 1, f); out[k] = 0; fclose(f); return (int)k; }
static int copy_file(const char *src, const char *dst) {
    FILE *a = fopen(src, "rb"), *b; char buf[8192]; size_t k; struct stat st;
    if (!a) return -1;
    b = fopen(dst, "wb");
    if (!b) { fclose(a); return -1; }
    while ((k = fread(buf, 1, sizeof buf, a)) > 0) fwrite(buf, 1, k, b);
    fclose(a); fclose(b);
    if (stat(src, &st) == 0) chmod(dst, st.st_mode & 0777);
    return 0;
}
static void mkdirs(const char *path) { char t[P]; snprintf(t, sizeof t, "%s", path); for (char *p = t + 1; *p; p++) if (*p == '/') { *p = 0; mkdir(t, 0755); *p = '/'; } mkdir(t, 0755); }

int main(int argc, char **argv) {
    char line[P], cdir[P], cpath[P]; FILE *f;
    if (argc < 2) { fprintf(stderr, "usage: harness_case_op <cases.pdl> [house_root]\n"); return 2; }
    if (!realpath(argv[1], cpath)) { fprintf(stderr, "harness_case_op: no case file %s\n", argv[1]); return 2; }
    snprintf(cdir, sizeof cdir, "%s", cpath); { char *d = dirname(cdir); memmove(cdir, d, strlen(d) + 1); }
    if (argc > 2) snprintf(HOUSE, sizeof HOUSE, "%s", argv[2]);
    else { snprintf(HOUSE, sizeof HOUSE, "%s", cdir); while (strlen(HOUSE) > 1) { char t[P]; struct stat st; snprintf(t, sizeof t, "%s/xyzfs", HOUSE); if (stat(t, &st) == 0) break; char *s = strrchr(HOUSE, '/'); if (!s || s == HOUSE) break; *s = 0; } }
    if (!(f = fopen(cpath, "r"))) return 2;
    while (fgets(line, sizeof line, f)) {
        char *fld[FMAX]; int nf = 0; char *p = trim(line), *q;
        if (!*p || *p == '#') continue;
        /* split on '|'; the LAST slot keeps the whole remainder (bars included) instead of silently dropping it */
        for (q = p; nf < FMAX; ) { char *bar = strchr(q, '|'); fld[nf++] = q; if (!bar || nf == FMAX) break; *bar = 0; q = bar + 1; }
        for (int i = 0; i < nf; i++) fld[i] = trim(fld[i]);
        const char *v = fld[0]; char a[P], b[P], pa[P + 8], pb[P + 8];
        {   /* more fields than the verb takes = an unescaped '|' in the text (use \p): fail loudly instead of misreading the row */
            static const struct { const char *verb; int max; } LIM[] = { {"RESULTS",2},{"CASE",2},{"SCRATCH",1},{"MKDIR",2},{"COPY",3},{"EXPECT_EXIT",3},{"EXPECT_FILE",4},{"EXPECT_NOFILE",3},{"CAPTURE",2},{"SUMTREE",3},{"EXPECT_EQ",4},{"EXPECT_NE",4},{"EXPECT_EXISTS",3},
                {"EXPECT_HAS",4},{"EXPECT_LACKS",4},{"EXPECT_OUT",3},{"EXPECT_OUT_IS",3},{"EXPECT_OUT_LACKS",3},{"ENV",3},{"SLEEP",2},{"WAIT_HAS",5},{"EXPECT_ALIVE",3},{"EXPECT_DEAD",3},{"CLEAN",1} };
            int bad = 0; for (size_t li = 0; li < sizeof LIM / sizeof LIM[0]; li++) if (!strcmp(v, LIM[li].verb) && nf > LIM[li].max && !(nf == LIM[li].max + 1 && !fld[nf - 1][0])) bad = 1;
            if (bad) { check(0, v, "row has extra fields: an unescaped | in the text? use \\p"); continue; }
        }
        if (!strcmp(v, "RESULTS") && nf >= 2) {
            snprintf(RESULTS, sizeof RESULTS, "%s/%s", cdir, fld[1]);
            { struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts); row("RUN|%lld|%s", (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000, cpath); }
        } else if (!strcmp(v, "CASE") && nf >= 2) snprintf(CASE, sizeof CASE, "%s", fld[1]);
        else if (!strcmp(v, "SCRATCH")) { snprintf(T, sizeof T, "/tmp/hc_XXXXXX"); if (mkdtemp(T)) made_scratch = 1; else { T[0] = 0; check(0, "scratch", "mkdtemp failed"); } }
        else if (!strcmp(v, "MKDIR") && nf >= 2 && T[0]) { expand(fld[1], a, sizeof a); snprintf(pa, sizeof pa, "%s/%s", T, a); mkdirs(pa); }
        else if (!strcmp(v, "COPY") && nf >= 3 && T[0]) {
            expand(fld[1], a, sizeof a); expand(fld[2], b, sizeof b); snprintf(pa, sizeof pa, "%s/%s", HOUSE, a); snprintf(pb, sizeof pb, "%s/%s", T, b);
            { char d[P + 8]; snprintf(d, sizeof d, "%s", pb); char *sl = strrchr(d, '/'); if (sl) { *sl = 0; mkdirs(d); } }
            if (copy_file(pa, pb) != 0) check(0, "copy", pa);
        } else if (!strcmp(v, "WRITE") && nf >= 3 && T[0]) {
            char raw[P * 2]; size_t rl = 0; raw[0] = 0;   /* the text is everything after the path, so it may itself contain " | " (a .pdl row): rejoin the fields */
            for (int i = 2; i < nf; i++) rl += (size_t)snprintf(raw + rl, sizeof raw - rl, "%s%s", i > 2 ? " | " : "", fld[i]);
            expand(fld[1], a, sizeof a); expand(raw, b, sizeof b); snprintf(pa, sizeof pa, "%s/%s", T, a);
            { char d[P + 8]; snprintf(d, sizeof d, "%s", pa); char *sl = strrchr(d, '/'); if (sl) { *sl = 0; mkdirs(d); } }
            FILE *w = fopen(pa, "w"); if (w) { fputs(b, w); fclose(w); } else check(0, "write", pa);
        } else if (!strcmp(v, "RUN") && nf >= 2) {
            char *av[18]; char store[16][P]; int ac = 0, pfd[2]; pid_t pid; int st = 0; size_t k = 0;
            for (int i = 1; i < nf && ac < 16; i++) { expand(fld[i], store[ac], P); av[ac] = store[ac]; ac++; }
            av[ac] = NULL; OUT[0] = 0;
            if (pipe(pfd) != 0) continue;
            fflush(stdout);
            if ((pid = fork()) == 0) { dup2(pfd[1], 1); dup2(pfd[1], 2); close(pfd[0]); close(pfd[1]); if (T[0]) { if (chdir(T)) {} } execvp(av[0], av); _exit(127); }
            close(pfd[1]);
            { ssize_t r; while (k < sizeof OUT - 1 && (r = read(pfd[0], OUT + k, sizeof OUT - 1 - k)) > 0) k += (size_t)r; OUT[k] = 0; }
            close(pfd[0]); waitpid(pid, &st, 0); last_exit = WIFEXITED(st) ? WEXITSTATUS(st) : 128 + WTERMSIG(st);
        } else if (!strcmp(v, "EXPECT_EXIT") && nf >= 3) { char seen[64]; snprintf(seen, sizeof seen, "exit=%d", last_exit); check(last_exit == atoi(fld[1]), fld[2], seen); }
        else if (!strcmp(v, "EXPECT_FILE") && nf >= 4 && T[0]) {
            char got[8192]; expand(fld[1], a, sizeof a); expand(fld[2], b, sizeof b); snprintf(pa, sizeof pa, "%s/%s", T, a);
            if (rd_all(pa, got, sizeof got) < 0) check(0, fld[3], "file missing");
            else check(!strcmp(got, b), fld[3], "content differs");
        } else if ((!strcmp(v, "EXPECT_HAS") || !strcmp(v, "EXPECT_LACKS")) && nf >= 4 && T[0]) {
            char got[16384]; int has; expand(fld[1], a, sizeof a); expand(fld[2], b, sizeof b); snprintf(pa, sizeof pa, "%s/%s", T, a);
            if (rd_all(pa, got, sizeof got) < 0) check(0, fld[3], "file missing");
            else if (!b[0]) check(0, fld[3], "EMPTY expectation (a bar in the text? use \\p)");
            else { has = strstr(got, b) != NULL; check(!strcmp(v, "EXPECT_HAS") ? has : !has, fld[3], got); }
        } else if (!strcmp(v, "EXPECT_EXISTS") && nf >= 3 && T[0]) { expand(fld[1], a, sizeof a); snprintf(pa, sizeof pa, "%s/%s", T, a); check(access(pa, F_OK) == 0, fld[2], "no such file");
        } else if (!strcmp(v, "EXPECT_NOFILE") && nf >= 3 && T[0]) { expand(fld[1], a, sizeof a); snprintf(pa, sizeof pa, "%s/%s", T, a); check(access(pa, F_OK) != 0, fld[2], "file exists"); }
        else if (!strcmp(v, "CAPTURE") && nf >= 2) { char got[sizeof OUT]; size_t gl; snprintf(got, sizeof got, "%s", OUT); gl = strlen(got); while (gl && (got[gl - 1] == '\n' || got[gl - 1] == '\r')) got[--gl] = 0; var_set(fld[1], got); }
        else if (!strcmp(v, "SUMTREE") && nf >= 3 && T[0]) { char hs[40]; expand(fld[2], a, sizeof a); snprintf(pa, sizeof pa, "%s/%s", T, a); tree_hash(pa, hs, sizeof hs); var_set(fld[1], hs); }
        else if ((!strcmp(v, "EXPECT_EQ") || !strcmp(v, "EXPECT_NE")) && nf >= 4) {
            expand(fld[1], a, sizeof a); expand(fld[2], b, sizeof b);
            if (!a[0] && !b[0]) check(0, fld[3], "both sides empty (an unset $var?)");
            else { char seen[P * 2 + 8]; snprintf(seen, sizeof seen, "%s vs %s", a, b); check(!strcmp(v, "EXPECT_EQ") ? !strcmp(a, b) : strcmp(a, b) != 0, fld[3], seen); }
        }
        else if (!strcmp(v, "APPEND") && nf >= 3 && T[0]) {
            char raw[P * 2]; size_t rl = 0; raw[0] = 0;
            for (int i = 2; i < nf; i++) rl += (size_t)snprintf(raw + rl, sizeof raw - rl, "%s%s", i > 2 ? " | " : "", fld[i]);
            expand(fld[1], a, sizeof a); expand(raw, b, sizeof b); snprintf(pa, sizeof pa, "%s/%s", T, a);
            { FILE *w = fopen(pa, "a"); if (w) { fputs(b, w); fclose(w); } else check(0, "append", pa); }
        }
        else if (!strcmp(v, "EXPECT_OUT_IS") && nf >= 3) {
            char got[sizeof OUT]; size_t gl; expand(fld[1], a, sizeof a); snprintf(got, sizeof got, "%s", OUT); gl = strlen(got);
            while (gl && (got[gl - 1] == '\n' || got[gl - 1] == '\r')) got[--gl] = 0;
            check(!strcmp(got, a), fld[2], got);
        } else if (!strcmp(v, "EXPECT_OUT_LACKS") && nf >= 3) { expand(fld[1], a, sizeof a); check(strstr(OUT, a) == NULL, fld[2], OUT); }
        else if (!strcmp(v, "ENV") && nf >= 2) { expand(fld[2 < nf ? 2 : 1], b, sizeof b); if (nf >= 3 && b[0]) setenv(fld[1], b, 1); else unsetenv(fld[1]); }
        else if (!strcmp(v, "SLEEP") && nf >= 2) usleep((useconds_t)atoi(fld[1]) * 1000);
        else if (!strcmp(v, "SPAWN") && nf >= 3 && NSPAWNED < 64) {
            char *av[18]; char store[16][P]; int ac = 0; pid_t pid;
            for (int i = 2; i < nf && ac < 16; i++) { expand(fld[i], store[ac], P); av[ac] = store[ac]; ac++; }
            av[ac] = NULL; fflush(stdout);
            if ((pid = fork()) == 0) { setpgid(0, 0); { int dn = open("/dev/null", O_RDWR); if (dn >= 0) { dup2(dn, 0); dup2(dn, 1); dup2(dn, 2); if (dn > 2) close(dn); } } if (T[0]) { if (chdir(T)) {} } execvp(av[0], av); _exit(127); }
            setpgid(pid, pid); SPAWNED[NSPAWNED++] = pid; snprintf(b, sizeof b, "%d", (int)pid); var_set(fld[1], b);
        } else if (!strcmp(v, "WAIT_HAS") && nf >= 5 && T[0]) {
            char got[16384]; long long t0 = now_ms(), max = atoll(fld[3]); int ok = 0; expand(fld[1], a, sizeof a); expand(fld[2], b, sizeof b); snprintf(pa, sizeof pa, "%s/%s", T, a);
            do { if (rd_all(pa, got, sizeof got) >= 0 && strstr(got, b)) { ok = 1; break; } usleep(10000); } while (now_ms() - t0 <= max);
            { char seen[96]; snprintf(seen, sizeof seen, "not there after %lld ms", now_ms() - t0); check(ok, fld[4], seen); if (ok) printf("  (%s took %lld ms)\n", fld[4], now_ms() - t0); }
        } else if ((!strcmp(v, "EXPECT_ALIVE") || !strcmp(v, "EXPECT_DEAD")) && nf >= 3) {
            int alive; expand(fld[1], a, sizeof a);
            if (atoi(a) > 0) waitpid((pid_t)atoi(a), NULL, WNOHANG);   /* reap a SPAWNed child that already exited: a zombie still answers kill(pid, 0) */
            alive = atoi(a) > 0 && kill((pid_t)atoi(a), 0) == 0;
            check(!strcmp(v, "EXPECT_ALIVE") ? alive : !alive, fld[2], alive ? "alive" : "dead");
        }
        else if (!strcmp(v, "EXPECT_OUT") && nf >= 3) { expand(fld[1], a, sizeof a); if (!a[0]) check(0, fld[2], "EMPTY expectation (a bar in the text? use \\p)"); else check(strstr(OUT, a) != NULL, fld[2], OUT); }
        else if (!strcmp(v, "CLEAN")) { kill_spawned(); if (made_scratch && !strncmp(T, "/tmp/hc_", 8)) { nftw(T, rm_cb, 16, FTW_DEPTH | FTW_PHYS); T[0] = 0; made_scratch = 0; } }
    }
    fclose(f);
    kill_spawned();   /* a case that forgot CLEAN leaves no background process either */
    if (made_scratch && !strncmp(T, "/tmp/hc_", 8))
        nftw(T, rm_cb, 16, FTW_DEPTH | FTW_PHYS);   /* a case that forgot CLEAN still leaves nothing behind */
    return 0;
}
