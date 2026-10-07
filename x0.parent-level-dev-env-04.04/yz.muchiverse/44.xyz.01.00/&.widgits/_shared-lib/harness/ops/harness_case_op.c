/* harness_case_op - runs one .pdl CASE FILE and appends PASS/FAIL rows to a results ledger. The op a pal harness execs (prisc `exec` ignores a child's exit status and
 * discards its output, so a pal cannot assert by itself: assertions are rows written to a file, and harness_verdict_op turns them into a verdict).
 *
 * Usage: harness_case_op <cases.pdl> [house_root]      (house_root defaults to the case file's own folder walked up to the dir that holds `xyzfs`)
 * Case file (pipe-delimited `VERB | a | b | ...`, '#' comments; strings may use \n \t and $T = the scratch dir, $HOUSE = the house root):
 *   RESULTS   | <path>                 results ledger, relative to the case file's folder (required, first)
 *   CASE      | <name>                 label prefix for the checks that follow
 *   SCRATCH   |                        create a scratch dir (/tmp/hc_XXXXXX) = $T
 *   MKDIR     | <path under $T>
 *   COPY      | <src under $HOUSE> | <dst under $T>      copies a file, keeps its mode
 *   WRITE     | <path under $T> | <text>                write a file (the text is everything after the path, so it may contain ' | ')
 *   RUN       | <argv0> | <arg1> | ...                   fork/exec (NO shell), stdout+stderr captured, exit code kept; fields are argv entries, so no quoting
 *   EXPECT_EXIT   | <n>            | <label>
 *   EXPECT_FILE   | <path> | <exact text> | <label>
 *   EXPECT_NOFILE | <path>         | <label>
 *   EXPECT_HAS    | <path> | <substring> | <label>       file contains it   (EXPECT_LACKS: file does not contain it)
 *   EXPECT_OUT    | <substring>    | <label>              last RUN's output contains it
 *   CLEAN     |                        remove the scratch dir (only one this op created)
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
#include <libgen.h>

#define P 4096
static char T[P], HOUSE[P], CASE[256] = "", RESULTS[P], OUT[16384];
static int last_exit = -1, made_scratch;

static char *trim(char *s) { char *e; while (*s == ' ' || *s == '\t') s++; e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0; return s; }
/* expand \n \t and $T / $HOUSE into out */
static void expand(const char *in, char *out, size_t n) {
    size_t o = 0;
    for (const char *p = in; *p && o + 2 < n; ) {
        if (p[0] == '\\' && p[1] == 'n') { out[o++] = '\n'; p += 2; }
        else if (p[0] == '\\' && p[1] == 't') { out[o++] = '\t'; p += 2; }
        else if (!strncmp(p, "$HOUSE", 6)) { o += (size_t)snprintf(out + o, n - o, "%s", HOUSE); p += 6; }
        else if (!strncmp(p, "$T", 2)) { o += (size_t)snprintf(out + o, n - o, "%s", T); p += 2; }
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
        char *fld[16]; int nf = 0; char *p = trim(line), *q;
        if (!*p || *p == '#') continue;
        for (q = p; nf < 16; ) { char *bar = strchr(q, '|'); fld[nf++] = q; if (!bar) break; *bar = 0; q = bar + 1; }
        for (int i = 0; i < nf; i++) fld[i] = trim(fld[i]);
        const char *v = fld[0]; char a[P], b[P], pa[P + 8], pb[P + 8];
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
            char raw[P]; size_t rl = 0; raw[0] = 0;   /* the text is everything after the path, so it may itself contain " | " (a .pdl row): rejoin the fields */
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
            else { has = strstr(got, b) != NULL; check(!strcmp(v, "EXPECT_HAS") ? has : !has, fld[3], got); }
        } else if (!strcmp(v, "EXPECT_NOFILE") && nf >= 3 && T[0]) { expand(fld[1], a, sizeof a); snprintf(pa, sizeof pa, "%s/%s", T, a); check(access(pa, F_OK) != 0, fld[2], "file exists"); }
        else if (!strcmp(v, "EXPECT_OUT") && nf >= 3) { expand(fld[1], a, sizeof a); check(strstr(OUT, a) != NULL, fld[2], OUT); }
        else if (!strcmp(v, "CLEAN") && made_scratch && !strncmp(T, "/tmp/hc_", 8)) { nftw(T, rm_cb, 16, FTW_DEPTH | FTW_PHYS); T[0] = 0; made_scratch = 0; }
    }
    fclose(f);
    if (made_scratch && !strncmp(T, "/tmp/hc_", 8))
        nftw(T, rm_cb, 16, FTW_DEPTH | FTW_PHYS);   /* a case that forgot CLEAN still leaves nothing behind */
    return 0;
}
