/* quartermaster - fleet budget gate (ROBOT-WORKFORCE-GAMEPLAN section 8). Call `check` BEFORE spending free quota/CPU, `record` AFTER an attempt.
 * Every command needs --now <epoch> (callers pass `date +%s`; the op never reads the clock, so tests are deterministic) and:
 *   --ledger <file> (check/record/status)   --limits <file> (check/spawn-check/status)   --ghost <id> (accepted on check, informational)
 *   quartermaster check <provider> <unit> <n>      -> QM|ALLOW|prov|used=U|max=M   exit 0
 *                                                  or QM|DENY|prov|reason|used=U|max=M|retry_after=S   exit 3
 *      reasons: quota unmeasured exhausted cpu-slots review-backlog no-limit-row.  `check cpu slots 1` = live slots; `check review pending 1` = unreviewed attempts.
 *   quartermaster record use <prov> <unit> <n> <ghost> <quest> | slot acquire|release <ghost> <quest> <pid> | exhausted <prov> <reason> | review add|done <id>
 *   quartermaster spawn-check <depth> <children> <total>   (the PROPOSED child: its depth, the parent's child count including it, entities including it; each must be <= its max)
 *   quartermaster status  -> one QM|LIMIT line per limit row, then QM|STATUS|ok|warnings=N|bad_lines=M  (warning at >= 80% used)
 * Exit: 0 allow/ok, 3 deny, 2 usage error.
 * Ledger (append-only; opened for writing ONLY with O_APPEND, one write() per row, never truncated/rewritten; rows carry explicit epochs, file mtime/order is never a signal):
 *   USE | epoch | provider | unit | n | ghost | quest      SLOT | epoch | acquire|release | ghost | quest | pid
 *   REVIEW | epoch | add|done | id                          EXHAUSTED | epoch | provider | reason
 * Limits: LIMIT | provider | unit | max | window_seconds [| allow_unmeasured=1]. max 0 = UNMEASURED = deny unless allow_unmeasured=1 (then unlimited, usage still recorded).
 * A USE counts while epoch > now - window (window 0 = all time). An EXHAUSTED row blocks its provider until epoch+window (window 0 -> 3600s default).
 * A slot is live when acquires > releases for (ghost,quest,pid) AND kill(pid,0) does not fail with ESRCH (a dead pid without release is stale).
 * Bad ledger lines are skipped and counted (bad_lines). Portable POSIX; no system()/popen/fork. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

typedef long long ll;
#define NM 96
typedef struct { ll epoch, n; char prov[NM], unit[NM]; } Use;
typedef struct { char ghost[NM], quest[NM]; ll pid; int net; } Slot;
typedef struct { char id[NM]; int net; } Rev;
typedef struct { ll epoch; char prov[NM]; } Exh;
typedef struct { char prov[NM], unit[NM]; ll max, window; int allow; } Lim;

static Use *U; static int nU;
static Slot *S; static int nS;
static Rev *R; static int nR;
static Exh *X; static int nX;
static Lim *L; static int nL;
static int bad_lines;

static void *grow(void *p, int n, size_t sz) { void *q = realloc(p, (size_t)(n + 1) * sz); if (!q) { fputs("quartermaster: out of memory\n", stderr); exit(2); } return q; }
static char *trim(char *s) { char *e; while (*s == ' ' || *s == '\t' || *s == '\r') s++; e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r')) *--e = 0; return s; }
static int split(char *line, char **f, int max) { int n = 0; char *q = line; for (;;) { char *b; f[n++] = q; if (n == max || !(b = strchr(q, '|'))) break; *b = 0; q = b + 1; } for (int i = 0; i < n; i++) f[i] = trim(f[i]); return n; }
static int num(const char *s, ll *out) { char *e; if (!*s) return 0; for (const char *p = s; *p; p++) if (*p < '0' || *p > '9') return 0; if (strlen(s) > 18) return 0; *out = strtoll(s, &e, 10); return 1; }
static void cp(char *d, const char *s) { snprintf(d, NM, "%s", s); }

static char *slurp(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb"); char *buf = NULL; size_t cap = 0, n = 0, k;
    *len = 0; if (!f) return NULL;
    for (;;) { if (n + 4097 > cap) { cap = cap ? cap * 2 : 8192; buf = realloc(buf, cap); if (!buf) exit(2); } k = fread(buf + n, 1, 4096, f); if (!k) break; n += k; }
    fclose(f); buf[n] = 0; *len = n; return buf;
}
static void add_slot(const char *g, const char *q, ll pid, int d) {
    for (int i = 0; i < nS; i++) if (S[i].pid == pid && !strcmp(S[i].ghost, g) && !strcmp(S[i].quest, q)) { S[i].net += d; return; }
    S = grow(S, nS, sizeof *S); cp(S[nS].ghost, g); cp(S[nS].quest, q); S[nS].pid = pid; S[nS].net = d; nS++;
}
static void add_rev(const char *id, int d) {
    for (int i = 0; i < nR; i++) if (!strcmp(R[i].id, id)) { R[i].net += d; return; }
    R = grow(R, nR, sizeof *R); cp(R[nR].id, id); R[nR].net = d; nR++;
}
static void load_ledger(const char *path) {
    size_t len; char *buf = slurp(path, &len), *p; if (!buf) return;
    for (p = buf; *p; ) {
        char *nl = strchr(p, '\n'), *f[12]; int nf, ok = 1; ll ep, n, pid;
        if (nl) *nl = 0;
        { char *t = trim(p); if (*t && *t != '#') {
            nf = split(t, f, 12);
            if (!strcmp(f[0], "USE") && nf == 7 && num(f[1], &ep) && num(f[4], &n) && *f[2] && *f[3]) { U = grow(U, nU, sizeof *U); U[nU].epoch = ep; U[nU].n = n; cp(U[nU].prov, f[2]); cp(U[nU].unit, f[3]); nU++; }
            else if (!strcmp(f[0], "SLOT") && nf == 6 && num(f[1], &ep) && num(f[5], &pid) && (!strcmp(f[2], "acquire") || !strcmp(f[2], "release"))) add_slot(f[3], f[4], pid, f[2][0] == 'a' ? 1 : -1);
            else if (!strcmp(f[0], "REVIEW") && nf == 4 && num(f[1], &ep) && *f[3] && (!strcmp(f[2], "add") || !strcmp(f[2], "done"))) add_rev(f[3], f[2][0] == 'a' ? 1 : -1);
            else if (!strcmp(f[0], "EXHAUSTED") && nf == 4 && num(f[1], &ep) && *f[2]) { X = grow(X, nX, sizeof *X); X[nX].epoch = ep; cp(X[nX].prov, f[2]); nX++; }
            else ok = 0;
            if (!ok) bad_lines++;
        } }
        if (!nl) break;
        p = nl + 1;
    }
    free(buf);
}
static void load_limits(const char *path) {
    size_t len; char *buf = slurp(path, &len), *p; if (!buf) return;
    for (p = buf; *p; ) {
        char *nl = strchr(p, '\n'), *f[12]; int nf; ll mx, w;
        if (nl) *nl = 0;
        { char *t = trim(p); if (*t && *t != '#') {
            nf = split(t, f, 12);
            if (!strcmp(f[0], "LIMIT") && nf >= 5 && num(f[3], &mx) && num(f[4], &w)) {
                L = grow(L, nL, sizeof *L); cp(L[nL].prov, f[1]); cp(L[nL].unit, f[2]); L[nL].max = mx; L[nL].window = w; L[nL].allow = 0;
                for (int i = 5; i < nf; i++) if (!strcmp(f[i], "allow_unmeasured=1")) L[nL].allow = 1;
                nL++;
            }
        } }
        if (!nl) break;
        p = nl + 1;
    }
    free(buf);
}
static Lim *find_lim(const char *prov, const char *unit) { for (int i = 0; i < nL; i++) if (!strcmp(L[i].prov, prov) && !strcmp(L[i].unit, unit)) return &L[i]; return NULL; }
static int slot_live(const Slot *s) { if (s->net <= 0) return 0; if (s->pid <= 0 || s->pid > 2147483646LL) return 0; return kill((pid_t)s->pid, 0) == 0 || errno != ESRCH; }
static ll live_slots(void) { ll c = 0; for (int i = 0; i < nS; i++) c += slot_live(&S[i]) ? 1 : 0; return c; }
static ll pending_reviews(void) { ll c = 0; for (int i = 0; i < nR; i++) if (R[i].net > 0) c += R[i].net; return c; }
static int cmp_use(const void *a, const void *b) { ll x = ((const Use *)a)->epoch, y = ((const Use *)b)->epoch; return x < y ? -1 : x > y; }

static int deny(const char *prov, const char *why, ll used, ll max, ll retry) { printf("QM|DENY|%s|%s|used=%lld|max=%lld|retry_after=%lld\n", prov, why, used, max, retry); return 3; }
static int allow(const char *prov, ll used, ll max) { printf("QM|ALLOW|%s|used=%lld|max=%lld\n", prov, used, max); return 0; }

/* usage of a provider/unit inside its window; counted uses are returned sorted by epoch in *cnt/ncnt (caller frees) */
static ll usage(const Lim *l, ll now, Use **cnt, int *ncnt) {
    ll used = 0; int k = 0; Use *c = malloc((size_t)(nU + 1) * sizeof *c); if (!c) exit(2);
    for (int i = 0; i < nU; i++) if (!strcmp(U[i].prov, l->prov) && !strcmp(U[i].unit, l->unit) && (l->window == 0 || U[i].epoch > now - l->window)) { used += U[i].n; c[k++] = U[i]; }
    qsort(c, (size_t)k, sizeof *c, cmp_use); *cnt = c; *ncnt = k; return used;
}
static int cmd_check(const char *prov, const char *unit, ll n, ll now) {
    Lim *l = find_lim(prov, unit); Use *c; int k; ll used;
    if (!l) return deny(prov, "no-limit-row", 0, 0, 0);
    if (!strcmp(prov, "cpu") && !strcmp(unit, "slots")) {
        used = live_slots();
        if (l->max == 0 && !l->allow) return deny(prov, "unmeasured", used, 0, 0);
        if (l->max > 0 && used + n > l->max) return deny(prov, "cpu-slots", used, l->max, 0);
        return allow(prov, used, l->max);
    }
    if (!strcmp(prov, "review") && !strcmp(unit, "pending")) {
        used = pending_reviews();
        if (l->max == 0 && !l->allow) return deny(prov, "unmeasured", used, 0, 0);
        if (l->max > 0 && used + n > l->max) return deny(prov, "review-backlog", used, l->max, 0);
        return allow(prov, used, l->max);
    }
    { ll last = -1; for (int i = 0; i < nX; i++) if (!strcmp(X[i].prov, prov) && X[i].epoch > last) last = X[i].epoch;
      if (last >= 0) { ll w = l->window > 0 ? l->window : 3600; if (now < last + w) { used = usage(l, now, &c, &k); free(c); return deny(prov, "exhausted", used, l->max, last + w - now); } } }
    used = usage(l, now, &c, &k);
    if (l->max == 0) { free(c); return l->allow ? allow(prov, used, 0) : deny(prov, "unmeasured", used, 0, 0); }
    if (used + n > l->max) {
        ll rem = used, retry = 0; int i = 0;
        while (rem + n > l->max && i < k) rem -= c[i++].n;
        if (rem + n <= l->max && i > 0 && l->window > 0) retry = c[i - 1].epoch + l->window - now;
        free(c); return deny(prov, "quota", used, l->max, retry);
    }
    free(c); return allow(prov, used, l->max);
}
static int cmd_spawn(ll d, ll ch, ll tot) {
    static const char *u[3] = { "depth", "children", "total" }; ll v[3]; v[0] = d; v[1] = ch; v[2] = tot;
    for (int i = 0; i < 3; i++) {
        Lim *l = find_lim("spawn", u[i]);
        if (!l) { printf("QM|DENY|spawn|no-limit-row|%s=%lld|max=0\n", u[i], v[i]); return 3; }
        if (l->max == 0 && !l->allow) { printf("QM|DENY|spawn|unmeasured|%s=%lld|max=0\n", u[i], v[i]); return 3; }
        if (l->max > 0 && v[i] > l->max) { printf("QM|DENY|spawn|%s|%s=%lld|max=%lld\n", u[i], u[i], v[i], l->max); return 3; }
    }
    printf("QM|ALLOW|spawn|depth=%lld|children=%lld|total=%lld\n", d, ch, tot); return 0;
}
static int cmd_status(ll now) {
    int warn = 0;
    for (int i = 0; i < nL; i++) {
        ll used = 0; Use *c; int k;
        if (!strcmp(L[i].prov, "spawn")) { printf("QM|LIMIT|spawn|%s|max=%lld|used=n/a\n", L[i].unit, L[i].max); continue; }
        if (!strcmp(L[i].prov, "cpu") && !strcmp(L[i].unit, "slots")) used = live_slots();
        else if (!strcmp(L[i].prov, "review") && !strcmp(L[i].unit, "pending")) used = pending_reviews();
        else { used = usage(&L[i], now, &c, &k); free(c); }
        { int w = L[i].max > 0 && used * 100 >= 80 * L[i].max; warn += w;
          printf("QM|LIMIT|%s|%s|used=%lld|max=%lld|window=%lld%s%s\n", L[i].prov, L[i].unit, used, L[i].max, L[i].window, L[i].max == 0 ? (L[i].allow ? "|unmeasured-allowed" : "|unmeasured") : "", w ? "|WARN" : ""); }
    }
    printf("QM|STATUS|ok|warnings=%d|bad_lines=%d\n", warn, bad_lines); return 0;
}
static int clean(const char *s) { if (!*s || strlen(s) >= NM) return 0; for (; *s; s++) if (*s == '|' || *s == '\n' || *s == '\r') return 0; return 1; }
static int append_row(const char *path, const char *row) {
    char buf[1024]; int fd, rfd, len; char last = '\n';
    if ((rfd = open(path, O_RDONLY)) >= 0) { off_t sz = lseek(rfd, 0, SEEK_END); if (sz > 0 && pread(rfd, &last, 1, sz - 1) != 1) last = '\n'; close(rfd); }
    len = snprintf(buf, sizeof buf, "%s%s\n", last == '\n' ? "" : "\n", row);   /* torn last row: start on a fresh line, earlier bytes stay untouched */
    if (len <= 0 || len >= (int)sizeof buf) return -1;
    fd = open(path, O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd < 0) { fprintf(stderr, "quartermaster: cannot open ledger %s: %s\n", path, strerror(errno)); return -1; }
    if (write(fd, buf, (size_t)len) != len) { close(fd); return -1; }
    close(fd); return 0;
}
static int usage_err(const char *m) { fprintf(stderr, "quartermaster: %s\nusage: quartermaster <check|record|spawn-check|status> ... --now <epoch> [--ledger f] [--limits f] [--ghost id]\n", m); return 2; }

int main(int argc, char **argv) {
    const char *ledger = NULL, *limits = NULL, *pos[16]; int np = 0; ll now = -1; char row[900];
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--ledger") && i + 1 < argc) ledger = argv[++i];
        else if (!strcmp(argv[i], "--limits") && i + 1 < argc) limits = argv[++i];
        else if (!strcmp(argv[i], "--ghost") && i + 1 < argc) i++;
        else if (!strcmp(argv[i], "--now") && i + 1 < argc) { if (!num(argv[++i], &now)) return usage_err("--now must be an epoch (digits)"); }
        else if (!strncmp(argv[i], "--", 2)) return usage_err("unknown or valueless option");
        else if (np < 16) pos[np++] = argv[i];
    }
    if (now < 0) return usage_err("--now <epoch> is required");
    if (np < 1) return usage_err("no command");
    if (!strcmp(pos[0], "check")) {
        ll n; if (np != 4 || !num(pos[3], &n) || n < 1) return usage_err("check <provider> <unit> <n>=1..");
        if (!ledger || !limits) return usage_err("check needs --ledger and --limits");
        load_limits(limits); load_ledger(ledger); return cmd_check(pos[1], pos[2], n, now);
    }
    if (!strcmp(pos[0], "spawn-check")) {
        ll d, c, t; if (np != 4 || !num(pos[1], &d) || !num(pos[2], &c) || !num(pos[3], &t)) return usage_err("spawn-check <depth> <children> <total>");
        if (!limits) return usage_err("spawn-check needs --limits");
        load_limits(limits); return cmd_spawn(d, c, t);
    }
    if (!strcmp(pos[0], "status")) { if (!ledger || !limits) return usage_err("status needs --ledger and --limits"); load_limits(limits); load_ledger(ledger); return cmd_status(now); }
    if (!strcmp(pos[0], "record")) {
        ll v; if (!ledger) return usage_err("record needs --ledger");
        for (int i = 1; i < np; i++) if (!clean(pos[i])) return usage_err("record fields must be non-empty, without '|' or newline");
        if (np == 7 && !strcmp(pos[1], "use") && num(pos[4], &v) && v > 0) snprintf(row, sizeof row, "USE | %lld | %s | %s | %lld | %s | %s", now, pos[2], pos[3], v, pos[5], pos[6]);
        else if (np == 6 && !strcmp(pos[1], "slot") && (!strcmp(pos[2], "acquire") || !strcmp(pos[2], "release")) && num(pos[5], &v)) snprintf(row, sizeof row, "SLOT | %lld | %s | %s | %s | %lld", now, pos[2], pos[3], pos[4], v);
        else if (np == 4 && !strcmp(pos[1], "exhausted")) snprintf(row, sizeof row, "EXHAUSTED | %lld | %s | %s", now, pos[2], pos[3]);
        else if (np == 4 && !strcmp(pos[1], "review") && (!strcmp(pos[2], "add") || !strcmp(pos[2], "done"))) snprintf(row, sizeof row, "REVIEW | %lld | %s | %s", now, pos[2], pos[3]);
        else return usage_err("bad record form");
        if (append_row(ledger, row) != 0) return 2;
        printf("QM|RECORDED|%s\n", row); return 0;
    }
    return usage_err("unknown command");
}
