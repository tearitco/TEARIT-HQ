/* ghost_run - runs ONE attempt of ONE delegated quest, end to end, safely (ROBOT-WORKFORCE-GAMEPLAN sections 3, 4, 10).
 * Usage: ghost_run <quest_dir> <target_branch_ref> <repo_dir> [--ghost <id>] [--worktrees <dir>] [--backend <exe>] [--backend-root <dir>] [--dry-run]
 * Steps (stop at the first failure and say why): 1 precheck (quest_check LOCK/BUDGET/ATTEMPTS, next to this binary) -> 2 worker worktree from the CURRENT tip of the
 * ref -> 3 backend call (prompt.txt verbatim, HORN_TOOLS=off, wall-clock watchdog) -> 4 first fenced code block -> OUTPUT file -> 5 full quest_check (all five) ->
 * 6 HARNESS (cwd = worktree) + VERDICT_FILE last line -> 7 append attempts/NNN/ -> 8 final `GHOST|...` line.
 * Exit: 0 PASS, 10 harness FAIL, 11 backend failure, 12 no-code-block (or OUTPUT unwritable), 13 precheck refused, 14 scope/base/lock violated, 2 usage.
 * Never merges, pushes or touches the target branch; the worktree stays for the manager. Subprocesses: fork+exec only (git, quest_check, backend, harness); no shell. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <poll.h>
#include <signal.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>

#define PM 4096
#define REPLY_CAP (1024 * 1024)
#define LOG_CAP (64 * 1024)
typedef struct { char *p; size_t n, cap, limit; int dropped; } Buf;
static void binit(Buf *b, size_t limit) { b->cap = 256; b->p = malloc(b->cap); b->p[0] = 0; b->n = 0; b->limit = limit; b->dropped = 0; }
static void bput(Buf *b, const char *s, size_t k) {
    if (b->n + k > b->limit) { k = b->limit > b->n ? b->limit - b->n : 0; b->dropped = 1; }
    if (b->n + k + 1 > b->cap) { while (b->n + k + 1 > b->cap) b->cap *= 2; b->p = realloc(b->p, b->cap); }
    memcpy(b->p + b->n, s, k); b->n += k; b->p[b->n] = 0;
}
static void bprintf(Buf *b, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
#include <stdarg.h>
static void bprintf(Buf *b, const char *fmt, ...) { char t[2048]; va_list ap; va_start(ap, fmt); int k = vsnprintf(t, sizeof t, fmt, ap); va_end(ap); if (k > 0) bput(b, t, (size_t)(k < (int)sizeof t ? k : (int)sizeof t - 1)); }

static double now_s(void) { struct timeval tv; gettimeofday(&tv, NULL); return (double)tv.tv_sec + tv.tv_usec / 1e6; }

typedef struct { Buf out, err; int status, timed_out; } Run;
/* fork+exec argv in its own process group; stdout (and stderr, or merged into out) captured; killed (group, then pid) after tmo seconds; always reaped. */
static void run_cap(char *const argv[], const char *cwd, const char *const *envset, int merge, int tmo, size_t cap, Run *r) {
    int po[2], pe[2]; pid_t pid; double dl = now_s() + tmo; int fo, fe, st = 0, done = 0;
    binit(&r->out, cap); binit(&r->err, cap); r->status = -1; r->timed_out = 0;
    fflush(stdout); fflush(stderr);
    if (pipe(po) != 0) return;
    if (pipe(pe) != 0) { close(po[0]); close(po[1]); return; }
    if ((pid = fork()) == 0) {
        int dn = open("/dev/null", O_RDWR);
        setpgid(0, 0);
        if (dn >= 0) dup2(dn, 0);
        dup2(po[1], 1); dup2(merge ? po[1] : pe[1], 2);
        close(po[0]); close(po[1]); close(pe[0]); close(pe[1]);
        for (const char *const *e = envset; e && *e; e++) { char t[PM]; snprintf(t, sizeof t, "%s", *e); char *eq = strchr(t, '='); if (eq) { *eq = 0; setenv(t, eq + 1, 1); } }
        if (cwd && chdir(cwd) != 0) _exit(126);
        execvp(argv[0], argv); _exit(127);
    }
    close(po[1]); close(pe[1]);
    if (pid < 0) { close(po[0]); close(pe[0]); return; }
    setpgid(pid, pid);
    fo = po[0]; fe = pe[0];
    fcntl(fo, F_SETFL, O_NONBLOCK); fcntl(fe, F_SETFL, O_NONBLOCK);
    for (;;) {
        struct pollfd pf[2]; int np = 0; char tmp[8192]; ssize_t k;
        pf[np].fd = fo; pf[np++].events = POLLIN; pf[np].fd = fe; pf[np++].events = POLLIN;
        poll(pf, 2, 20);
        for (int i = 0; i < 2; i++) {
            int *fd = i ? &fe : &fo; Buf *b = i ? &r->err : &r->out;
            while (*fd >= 0 && (k = read(*fd, tmp, sizeof tmp)) > 0) bput(b, tmp, (size_t)k);
            if (*fd >= 0 && (k == 0 || (k < 0 && errno != EAGAIN && errno != EINTR))) { close(*fd); *fd = -1; }
        }
        if (waitpid(pid, &st, WNOHANG) == pid) { done = 1; break; }
        if (now_s() > dl) { kill(-pid, SIGKILL); kill(pid, SIGKILL); waitpid(pid, &st, 0); r->timed_out = 1; done = 1; break; }
    }
    if (done) for (int i = 0; i < 2; i++) { int *fd = i ? &fe : &fo; Buf *b = i ? &r->err : &r->out; char tmp[8192]; ssize_t k; while (*fd >= 0 && (k = read(*fd, tmp, sizeof tmp)) > 0) bput(b, tmp, (size_t)k); }
    if (fo >= 0) close(fo);
    if (fe >= 0) close(fe);
    r->status = WIFEXITED(st) ? WEXITSTATUS(st) : (WIFSIGNALED(st) ? 128 + WTERMSIG(st) : -1);
}
static void run_free(Run *r) { free(r->out.p); free(r->err.p); }

static int git_run(const char *repo, Run *r, ...) {   /* NULL-terminated git args after repo */
    char *av[24]; int n = 0; va_list ap; const char *x;
    av[n++] = "git"; av[n++] = "-C"; av[n++] = (char *)repo;
    va_start(ap, r); while (n < 22 && (x = va_arg(ap, const char *))) av[n++] = (char *)x; va_end(ap);
    av[n] = NULL; run_cap(av, NULL, NULL, 0, 60, 4 * 1024 * 1024, r); return r->status;
}
static void first_line(Buf *b, char *res, size_t rn) { snprintf(res, rn, "%s", b->p); char *nl = strchr(res, '\n'); if (nl) *nl = 0; }

/* ---- state ---- */
static struct {
    char quest[256], qdir[PM], ref[PM], repo[PM], wtroot[PM], ghost[64], backend[PM], broot[PM], wt[PM], branch[512], base[96], opsdir[PM], qc[PM + 32], ts[40];
    char output[PM], harness[PM], vfile[PM];
    int n, maxatt, maxsec, record, have_wt, dry;
    double t0;
} G;
static Buf reply, berr, diffb, hlog, chk, reason;
static char bclass[40] = "-", provider[300] = "-", result[160] = "-", verdict[600] = "";

static void clean_line(char *s) { for (; *s; s++) if (*s == '|' || (unsigned char)*s < 32) *s = '?'; }
static int wfile(const char *dir, const char *name, const char *data, size_t n) {
    char p[PM + 64]; snprintf(p, sizeof p, "%s/%s", dir, name);
    FILE *f = fopen(p, "wb"); if (!f) return -1;
    if (n && fwrite(data, 1, n, f) != n) { fclose(f); return -1; }
    return fclose(f);
}
static void record_attempt(void) {
    char ad[PM + 32], d[PM + 64]; Buf m; char t[1024];
    snprintf(ad, sizeof ad, "%s/attempts", G.qdir); mkdir(ad, 0755);
    snprintf(d, sizeof d, "%s/%03d", ad, G.n);
    if (mkdir(d, 0755) != 0) { fprintf(stderr, "ghost_run: cannot create %s: %s (attempt NOT recorded)\n", d, strerror(errno)); return; }
    binit(&m, 65536);
    bprintf(&m, "ATTEMPT | %d | %s | %s\n", G.n, G.ghost, G.ts);
    bprintf(&m, "BACKEND | %s\n", bclass);
    snprintf(t, sizeof t, "%s", provider); clean_line(t); bprintf(&m, "PROVIDER | %s\n", t);
    bprintf(&m, "WALL | %.1f\n", now_s() - G.t0);
    bprintf(&m, "RESULT | %s\n", result);
    snprintf(t, sizeof t, "%.500s", reason.p); clean_line(t); bprintf(&m, "REASON | %s\n", t);
    snprintf(t, sizeof t, "%s", G.have_wt ? G.branch : "-"); bprintf(&m, "BRANCH | %s\n", t);
    bprintf(&m, "WORKTREE | %s\n", G.have_wt ? G.wt : "-");
    wfile(d, "meta.pdl", m.p, m.n); free(m.p);
    wfile(d, "reply.txt", reply.p, reply.n);
    wfile(d, "backend_stderr.txt", berr.p, berr.n);
    wfile(d, "output.diff", diffb.p, diffb.n);
    if (hlog.dropped) bput(&hlog, "\n[harness.log truncated at 64 KB]\n", 33);
    wfile(d, "harness.log", hlog.p, hlog.n);
    wfile(d, "check.txt", chk.p, chk.n);
    snprintf(t, sizeof t, "%s\n", verdict); wfile(d, "verdict.txt", t, strlen(t));
}
static void finish(int code, const char *res, const char *why) {
    snprintf(result, sizeof result, "%s", res);
    if (why) { reason.n = 0; reason.p[0] = 0; bput(&reason, why, strlen(why)); }
    if (G.record) record_attempt();
    char r2[200]; snprintf(r2, sizeof r2, "%s", result); clean_line(r2);
    if (reason.n && strcmp(result, "PASS")) { char w[300]; snprintf(w, sizeof w, "%.200s", reason.p); clean_line(w); fprintf(stderr, "ghost_run: %s: %s\n", r2, w); }
    printf("GHOST|%s|attempt=%d|RESULT|%s|worktree=%s\n", G.quest, G.n, r2, G.have_wt ? G.wt : "-");
    exit(code);
}

/* ---- helpers ---- */
static char *rd_file(const char *path) {
    FILE *f = fopen(path, "rb"); size_t cap = 4096, n = 0, k; char *b;
    if (!f) return NULL;
    b = malloc(cap);
    while ((k = fread(b + n, 1, cap - n - 1, f)) > 0) { n += k; if (n + 1 >= cap) { cap *= 2; b = realloc(b, cap); } }
    b[n] = 0; fclose(f); return b;
}
static char *trim(char *s) { while (isspace((unsigned char)*s)) s++; char *e = s + strlen(s); while (e > s && isspace((unsigned char)e[-1])) *--e = 0; return s; }
static void absolutize(char *dst, size_t n, const char *p) {
    char cw[PM];
    if (p[0] == '/') snprintf(dst, n, "%s", p); else { if (!getcwd(cw, sizeof cw)) cw[0] = 0; snprintf(dst, n, "%s/%s", cw, p); }
    size_t l = strlen(dst); while (l > 1 && dst[l - 1] == '/') dst[--l] = 0;
}
static int safe_name(const char *s) { if (!*s) return 0; for (; *s; s++) if (!isalnum((unsigned char)*s) && *s != '.' && *s != '_' && *s != '-') return 0; return 1; }
/* repo-relative path: no absolute, no empty/./../.git component, no control characters */
static int safe_rel(const char *p) {
    if (!*p || p[0] == '/' || strlen(p) > 1000) return 0;
    for (const char *c = p; *c; c++) if ((unsigned char)*c < 32 || *c == '\\') return 0;
    const char *s = p;
    for (;;) {
        const char *e = strchr(s, '/'); size_t l = e ? (size_t)(e - s) : strlen(s);
        if (l == 0 || (l == 1 && s[0] == '.') || (l == 2 && !strncmp(s, "..", 2)) || (l == 4 && !strncmp(s, ".git", 4))) return 0;
        if (!e) break;
        s = e + 1;
    }
    return 1;
}
static int parse_pdl_row(char *ln, char **key, char **val) {
    char *t = trim(ln), *bar; if (!*t || *t == '#') return 0;
    if (!(bar = strchr(t, '|'))) return 0;
    *bar = 0; *key = trim(t); *val = trim(bar + 1); return 1;
}

/* 8 outputs of quest_check: FAIL lines of selected checks are copied into dst; returns exit code */
static int run_qcheck(const char *wtdir, Buf *dst, int tmo) {
    char *av[] = { G.qc, G.qdir, (char *)wtdir, G.ref, NULL }; Run r;
    run_cap(av, NULL, NULL, 0, tmo, 256 * 1024, &r);
    bput(dst, r.out.p, r.out.n);
    int rc = r.timed_out ? 127 : r.status; run_free(&r); return rc;
}
static void fail_lines(Buf *src, char *dst, size_t dn, int only_lab) {
    char *s = src->p; dst[0] = 0;
    while (*s) {
        char *nl = strchr(s, '\n'); size_t l = nl ? (size_t)(nl - s) : strlen(s);
        if (!strncmp(s, "QCHECK|FAIL|", 12)) {
            int ok = !only_lab || !strncmp(s + 12, "LOCK|", 5) || !strncmp(s + 12, "BUDGET|", 7) || !strncmp(s + 12, "ATTEMPTS|", 9);
            if (ok && strlen(dst) + l + 4 < dn) { strncat(dst, s + 12, l - 12); strcat(dst, "; "); }
        }
        if (!nl) break;
        s = nl + 1;
    }
}

/* ---- first fenced block ---- */
static int extract_block(const char *txt, Buf *out) {
    const char *s = txt; int in = 0;
    while (*s) {
        const char *nl = strchr(s, '\n'); size_t l = nl ? (size_t)(nl - s) : strlen(s);
        size_t a = 0, z = l; while (a < l && (s[a] == ' ' || s[a] == '\t')) a++;
        while (z > a && (s[z - 1] == '\r' || s[z - 1] == ' ' || s[z - 1] == '\t')) z--;
        if (!in) { if (z - a >= 3 && !strncmp(s + a, "```", 3)) in = 1; }
        else {
            if (z - a == 3 && !strncmp(s + a, "```", 3)) return out->n > 0;
            bput(out, s, l); bput(out, "\n", 1);
        }
        if (!nl) break;
        s = nl + 1;
    }
    return in && out->n > 0;
}
static int write_output(const char *rel, const Buf *data, char *err, size_t en) {
    char p[PM * 2]; size_t k = strlen(G.wt); struct stat st;
    memcpy(p, G.wt, k); p[k] = 0;
    const char *s = rel;
    for (;;) {
        const char *e = strchr(s, '/'); size_t l = e ? (size_t)(e - s) : strlen(s);
        if (strlen(p) + l + 2 >= sizeof p) { snprintf(err, en, "path too long"); return -1; }
        { size_t pl = strlen(p); p[pl] = '/'; memcpy(p + pl + 1, s, l); p[pl + 1 + l] = 0; }
        if (!e) break;
        if (lstat(p, &st) == 0) { if (!S_ISDIR(st.st_mode)) { snprintf(err, en, "path component is not a plain directory (symlink or file): %s", p); return -1; } }
        else if (mkdir(p, 0755) != 0) { snprintf(err, en, "mkdir %s: %s", p, strerror(errno)); return -1; }
        s = e + 1;
    }
    if (lstat(p, &st) == 0 && !S_ISREG(st.st_mode)) { snprintf(err, en, "OUTPUT exists and is not a regular file: %s", p); return -1; }
    int fd = open(p, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW, 0644);
    if (fd < 0) { snprintf(err, en, "open %s: %s", p, strerror(errno)); return -1; }
    size_t off = 0; while (off < data->n) { ssize_t w = write(fd, data->p + off, data->n - off); if (w <= 0) { close(fd); snprintf(err, en, "write failed"); return -1; } off += (size_t)w; }
    close(fd); return 0;
}

static int usage(const char *m) {
    if (m) fprintf(stderr, "ghost_run: %s\n", m);
    fprintf(stderr, "usage: ghost_run <quest_dir> <target_branch_ref> <repo_dir> [--ghost <id>] [--worktrees <dir>] [--backend <exe>] [--backend-root <dir>] [--dry-run]\n");
    return 2;
}

int main(int argc, char **argv) {
    const char *pos[3]; int np = 0; const char *ghost = "ghost", *wtroot = "/home/no/staging/ghosts", *backend = NULL, *broot = NULL;
    unsetenv("GIT_DIR"); unsetenv("GIT_WORK_TREE"); unsetenv("GIT_INDEX_FILE");
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--dry-run")) G.dry = 1;
        else if (!strcmp(argv[i], "--ghost") || !strcmp(argv[i], "--worktrees") || !strcmp(argv[i], "--backend") || !strcmp(argv[i], "--backend-root")) {
            if (i + 1 >= argc) return usage("flag needs a value");
            const char *v = argv[i + 1];
            if (!strcmp(argv[i], "--ghost")) ghost = v; else if (!strcmp(argv[i], "--worktrees")) wtroot = v; else if (!strcmp(argv[i], "--backend")) backend = v; else broot = v;
            i++;
        } else if (argv[i][0] == '-' && argv[i][1]) return usage("unknown flag");
        else if (np < 3) pos[np++] = argv[i];
        else return usage("too many arguments");
    }
    if (np != 3) return usage("need <quest_dir> <target_branch_ref> <repo_dir>");
    if (!safe_name(ghost) || strlen(ghost) > 40) return usage("--ghost must be [A-Za-z0-9._-]");
    if (pos[1][0] == '-' || !pos[1][0]) return usage("bad target ref");
    binit(&reply, REPLY_CAP); binit(&berr, LOG_CAP); binit(&diffb, 4 << 20); binit(&hlog, LOG_CAP); binit(&chk, 256 * 1024); binit(&reason, 4096);
    G.t0 = now_s();
    { time_t t = time(NULL); struct tm tm; gmtime_r(&t, &tm); strftime(G.ts, sizeof G.ts, "%Y-%m-%dT%H:%M:%SZ", &tm); }
    absolutize(G.qdir, sizeof G.qdir, pos[0]); absolutize(G.repo, sizeof G.repo, pos[2]); absolutize(G.wtroot, sizeof G.wtroot, wtroot);
    snprintf(G.ref, sizeof G.ref, "%s", pos[1]); snprintf(G.ghost, sizeof G.ghost, "%s", ghost);
    { const char *b = strrchr(G.qdir, '/'); snprintf(G.quest, sizeof G.quest, "%s", b ? b + 1 : G.qdir); }
    if (!safe_name(G.quest)) { G.n = 0; finish(2, "usage", "quest dir name must be [A-Za-z0-9._-]"); }
    { char self[PM]; snprintf(self, sizeof self, "%s", argv[0]); char *sl = strrchr(self, '/'); if (sl) { *sl = 0; absolutize(G.opsdir, sizeof G.opsdir, self); } else snprintf(G.opsdir, sizeof G.opsdir, "."); }
    snprintf(G.qc, sizeof G.qc, "%s/quest_check.+x", G.opsdir);
    { char d[PM]; snprintf(d, sizeof d, "%s/../../../..", G.opsdir);
      absolutize(G.broot, sizeof G.broot, broot ? broot : d);
      if (backend) absolutize(G.backend, sizeof G.backend, backend); else snprintf(G.backend, sizeof G.backend, "%s/../../../../^.hai-horn/ops/+x/horn_chat_backend.+x", G.opsdir); }

    /* ---- step 1: precheck ---- */
    { char p[PM + 32], *txt; int cnt = 0; DIR *d; struct dirent *e;
      snprintf(p, sizeof p, "%s/attempts", G.qdir);
      if ((d = opendir(p))) { while ((e = readdir(d))) if (isdigit((unsigned char)e->d_name[0])) cnt++; closedir(d); }
      G.n = cnt + 1; G.maxsec = 600; G.maxatt = 0;
      snprintf(p, sizeof p, "%s/budget.pdl", G.qdir);
      if ((txt = rd_file(p))) { char *sv = NULL; for (char *ln = strtok_r(txt, "\n", &sv); ln; ln = strtok_r(NULL, "\n", &sv)) {
            char *k, *v; if (!parse_pdl_row(ln, &k, &v) || strcmp(k, "BUDGET")) continue;
            char *b2 = strchr(v, '|'); if (!b2) continue; *b2 = 0; char *kk = trim(v), *vv = trim(b2 + 1);
            if (!strcmp(kk, "max_attempts")) G.maxatt = atoi(vv); else if (!strcmp(kk, "max_seconds") && atoi(vv) > 0) G.maxsec = atoi(vv); }
        free(txt); }
      char fl[1024]; fl[0] = 0; Buf pc; binit(&pc, 256 * 1024);
      int rc = run_qcheck(G.repo, &pc, 120);
      if (rc == 127 || rc == 126 || rc == 64 || rc < 0) { free(pc.p); finish(13, "REFUSED-precheck", "quest_check could not run (looked for quest_check.+x next to ghost_run)"); }
      fail_lines(&pc, fl, sizeof fl, 1);
      if (rc & (1 | 8 | 16)) { free(pc.p); finish(13, "REFUSED-precheck", fl[0] ? fl : "LOCK/BUDGET/ATTEMPTS failed"); }
      free(pc.p);
      if (G.n > G.maxatt) { char w[100]; snprintf(w, sizeof w, "attempt %d exceeds max_attempts=%d", G.n, G.maxatt); finish(13, "REFUSED-attempts", w); }
    }
    { /* RUN.pdl */
      char p[PM + 32], *txt; int no = 0, nh = 0, nv = 0; snprintf(p, sizeof p, "%s/RUN.pdl", G.qdir);
      if (!(txt = rd_file(p))) finish(13, "REFUSED-runpdl", "RUN.pdl missing");
      char *sv = NULL;
      for (char *ln = strtok_r(txt, "\n", &sv); ln; ln = strtok_r(NULL, "\n", &sv)) {
          char *k, *v; if (!parse_pdl_row(ln, &k, &v)) continue;
          if (!strcmp(k, "OUTPUT")) { no++; snprintf(G.output, sizeof G.output, "%s", v); }
          else if (!strcmp(k, "HARNESS")) { nh++; snprintf(G.harness, sizeof G.harness, "%s", v); }
          else if (!strcmp(k, "VERDICT_FILE")) { nv++; snprintf(G.vfile, sizeof G.vfile, "%s", v); }
      }
      free(txt);
      if (no != 1 || nh != 1 || nv != 1) finish(13, "REFUSED-runpdl", "RUN.pdl needs exactly one OUTPUT, one HARNESS and one VERDICT_FILE row");
      if (!safe_rel(G.output)) finish(13, "REFUSED-output", "OUTPUT must be a repo-relative path without '..', '.git' or empty parts");
      if (!safe_rel(G.vfile)) finish(13, "REFUSED-output", "VERDICT_FILE must be a repo-relative path without '..', '.git' or empty parts");
      if (!strcmp(G.output, G.vfile)) finish(13, "REFUSED-output", "OUTPUT and VERDICT_FILE must differ");
      if (!G.harness[0]) finish(13, "REFUSED-runpdl", "empty HARNESS");
    }
    /* ---- step 2 preflight (shared with --dry-run) ---- */
    snprintf(G.branch, sizeof G.branch, "ghost/%s/%s-a%03d", G.ghost, G.quest, G.n);
    snprintf(G.wt, sizeof G.wt, "%s/%s/%s-a%03d", G.wtroot, G.ghost, G.quest, G.n);
    { Run r; struct stat st; char spec[PM + 16], rb[300];
      snprintf(spec, sizeof spec, "%s^{commit}", G.ref);
      git_run(G.repo, &r, "rev-parse", "--verify", "-q", spec, NULL, NULL, NULL);
      if (r.status != 0) { run_free(&r); finish(13, "REFUSED-ref", "target ref does not resolve in repo_dir"); }
      run_free(&r);
      snprintf(rb, sizeof rb, "refs/heads/%s", G.branch);
      git_run(G.repo, &r, "show-ref", "--verify", "--quiet", rb, NULL, NULL, NULL);
      if (r.status == 0) { run_free(&r); finish(13, "REFUSED-branch-exists", G.branch); }
      run_free(&r);
      if (lstat(G.wt, &st) == 0) finish(13, "REFUSED-path-exists", G.wt);
    }
    if (G.dry) {
        printf("DRYRUN|precheck ok: attempt %d of max %d, timeout %ds\n", G.n, G.maxatt, G.maxsec);
        printf("DRYRUN|would create branch %s from %s at %s\n", G.branch, G.ref, G.wt);
        printf("DRYRUN|would call backend %s with %s/prompt.txt, write %s, run quest_check, run '%s', record attempts/%03d\n", G.backend, G.qdir, G.output, G.harness, G.n);
        printf("GHOST|%s|attempt=%d|RESULT|DRYRUN|worktree=%s\n", G.quest, G.n, G.wt);
        return 0;
    }
    /* ---- step 2: worktree ---- */
    { Run r; git_run(G.repo, &r, "worktree", "add", "-q", "-b", G.branch, G.wt, G.ref, NULL);
      if (r.status != 0) { char w[600]; snprintf(w, sizeof w, "git worktree add failed: %.400s", r.err.p); run_free(&r); finish(13, "REFUSED-worktree", w); }
      run_free(&r);
      G.have_wt = 1; G.record = 1;
      git_run(G.wt, &r, "rev-parse", "HEAD", NULL); first_line(&r.out, G.base, sizeof G.base); run_free(&r);
    }
    /* ---- step 3: backend ---- */
    { char pp[PM + 32], *prompt; Run r; char e1[64], e2[PM + 40], e3[PM + 40]; const char *envs[4];
      snprintf(pp, sizeof pp, "%s/prompt.txt", G.qdir);
      if (!(prompt = rd_file(pp))) finish(11, "backend-failed", "prompt.txt missing");
      snprintf(e1, sizeof e1, "HORN_TOOLS=off"); { char t[64]; snprintf(t, sizeof t, "HORN_CURL_TIMEOUT=%d", G.maxsec); snprintf(e2, sizeof e2, "%s", t); }
      snprintf(e3, sizeof e3, "PRISC_PROJECT_ROOT=%s", G.broot);
      envs[0] = e1; envs[1] = e2; envs[2] = e3; envs[3] = NULL;
      char *av[] = { G.backend, prompt, NULL };
      run_cap(av, G.broot, envs, 0, G.maxsec, REPLY_CAP, &r);
      bput(&reply, r.out.p, r.out.n); bput(&berr, r.err.p, r.err.n);
      /* provider line: last stderr line starting with "answered by", else the last line */
      { char *s = berr.p, *best = NULL, *lastl = NULL; while (*s) { if (!strncmp(s, "answered by", 11)) best = s; if (*s != '\n') lastl = s; char *nl = strchr(s, '\n'); if (!nl) break;
        s = nl + 1; }
        const char *pick = best ? best : lastl; if (pick) { snprintf(provider, sizeof provider, "%s", pick); char *nl = strchr(provider, '\n'); if (nl) *nl = 0; } }
      free(prompt);
      if (r.timed_out) { run_free(&r); snprintf(bclass, sizeof bclass, "timeout"); { char w[100]; snprintf(w, sizeof w, "backend killed after %ds wall clock", G.maxsec); finish(11, "backend-failed", w); } }
      if (r.status != 0) {
          if (r.status == 127) snprintf(bclass, sizeof bclass, "exec-failed"); else if (r.status > 128) snprintf(bclass, sizeof bclass, "signal%d", r.status - 128); else snprintf(bclass, sizeof bclass, "%d", r.status);
          char w[600]; snprintf(w, sizeof w, "backend exit %s: %.400s", bclass, berr.p); run_free(&r); finish(11, "backend-failed", w);
      }
      snprintf(bclass, sizeof bclass, "0"); run_free(&r);
    }
    /* ---- step 4: code block -> OUTPUT ---- */
    { Buf code; char err[PM + 100]; binit(&code, REPLY_CAP);
      if (!extract_block(reply.p, &code)) finish(12, "no-code-block", "reply has no non-empty fenced code block");
      if (write_output(G.output, &code, err, sizeof err) != 0) finish(12, "output-unwritable", err);
      free(code.p); }
    /* ---- step 5: full quest_check ---- */
    { int rc = run_qcheck(G.wt, &chk, 120);
      if (rc != 0) { char fl[1024]; fail_lines(&chk, fl, sizeof fl, 0); char w[1200]; snprintf(w, sizeof w, "quest_check mask=%d: %s", rc, fl); finish(14, "VIOLATION-check", w); } }
    /* capture the worker's change before the harness adds files of its own */
    { Run r; git_run(G.wt, &r, "add", "-N", "--", G.output, NULL); run_free(&r);
      git_run(G.wt, &r, "diff", "--no-color", "--no-ext-diff", G.base, NULL); bput(&diffb, r.out.p, r.out.n); run_free(&r); }
    /* ---- step 6: harness ---- */
    { char vp[PM * 2], *hav[65], *cmd = strdup(G.harness); int nh = 0; char *sv = NULL; Run r; char *txt;
      snprintf(vp, sizeof vp, "%s/%s", G.wt, G.vfile); unlink(vp);
      for (char *t = strtok_r(cmd, " \t", &sv); t && nh < 64; t = strtok_r(NULL, " \t", &sv)) hav[nh++] = t;
      hav[nh] = NULL;
      run_cap(hav, G.wt, NULL, 1, G.maxsec, LOG_CAP, &r);
      bput(&hlog, r.out.p, r.out.n); hlog.dropped |= r.out.dropped;
      int hto = r.timed_out, hst = r.status; run_free(&r); free(cmd);
      if (hto) { snprintf(verdict, sizeof verdict, "(harness timed out)"); finish(10, "FAIL", "harness exceeded max_seconds and was killed"); }
      if (hst == 127 || hst == 126) { snprintf(verdict, sizeof verdict, "(harness not runnable)"); finish(10, "FAIL", "harness command could not be executed"); }
      if (!(txt = rd_file(vp))) { snprintf(verdict, sizeof verdict, "(no verdict file)"); finish(10, "FAIL", "harness wrote no VERDICT_FILE"); }
      { char *end = txt + strlen(txt); while (end > txt && isspace((unsigned char)end[-1])) *--end = 0; char *ls = strrchr(txt, '\n'); ls = ls ? ls + 1 : txt; snprintf(verdict, sizeof verdict, "%s", ls); }
      free(txt);
      if (strstr(verdict, "PASS") && !strstr(verdict, "FAIL")) finish(0, "PASS", NULL);
      finish(10, "FAIL", "verdict line does not hold PASS without FAIL");
    }
    return 0;
}
