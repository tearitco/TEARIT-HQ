/* proc_reg_op.c - shell / pal front end of the process registry v2 (kh_proc_reg.h). 2026-11-02.
 * The whole registry lives HERE and nowhere else (house rule: stateful logic shared by several consumers = one op, called with plain fork/exec; never a header + link).
 * C programs, shell scripts, pal and the CPU guard all just run this op. File format and rules: see the plan, section 3.1.
 *   PROC | pid | start | pgid | lifetime | owner_pid | owner_start | role | app | name | started_epoch | cmd      in <house>/#.desktop/proc_registry.pdl
 *   lifetime: owner (ends when its owner is gone, the default) | keep (survives its owner, opt-in) | house (infrastructure, never orphan-reaped)
 *   start = field 22 of /proc/<pid>/stat: a row is only acted on while pid AND start match a live process (pid-reuse guard)
 * Every change also keeps the legacy v1 ledger (#.desktop/livedesk_proc_list.txt) in step so the taskbar's quit reaper reaches these rows too.
 * Audit trail: #.desktop/proc_registry.log.txt (START / END rows, capped at 256 KB).
 *   proc_reg_op <house> register <pid> <owner|keep|house> <owner_pid> <role> <app> <name> [cmd]     (cmd: the command line to record, default: read from /proc)
 *   proc_reg_op <house> unregister <pid>
 *   proc_reg_op <house> list                       the registry file, as is
 *   proc_reg_op <house> prune                      drop rows whose process is gone
 *   proc_reg_op <house> reap-orphans [grace_ms] [orphan_grace_s] [dry]    end lifetime=owner rows whose owner has been gone for orphan_grace_s (default 15)
 *   proc_reg_op <house> audit [min_age_s]          house processes with NO row (violations); also writes #.desktop/proc_violations.txt; exit 3 when any
 *   proc_reg_op <house> owner-alive <pid> <start>  exit 0 alive / 1 gone
 *   proc_reg_op <house> adopt <owner_pid> <exe_prefix> <role> <app> <name>    register unregistered house processes whose exe starts with the prefix, owned by owner_pid
 *   proc_reg_op <house> rule-add <owner_pid> <exe_prefix|@file:path#key> <role> <app> <name>   declare ownership of a detached stack; applied every pass while the owner lives
 *   proc_reg_op <house> apply-rules                 apply the declared rules now (reap-orphans does it first)
 *   proc_reg_op <house> hold <seconds>             test helper: sleep (a process whose exe is this op, so a harness can put one under a scratch house)
 * Exit: 0 ok, 2 usage, 3 violations found (audit), 1 failure. Plan: PROC-REGISTRY-STANDARD-AND-REFACTOR-PLAN-2026-11-02.md */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#define PR_OWNER 0
#define PR_KEEP 1
#define PR_HOUSE 2


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <dirent.h>
#include <limits.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#define KHP_LINE 1024
#define KHP_MAXROWS 2048
#define KHP_LOG_MAX 262144

typedef struct { long pid, start, pgid, owner, owner_start, started; int lifetime; char role[24], app[40], name[48], cmd[220]; } KhpRow;

static void khp_path(const char *house, const char *leaf, char *out, size_t n) { snprintf(out, n, "%s/#.desktop/%s", house, leaf); }

static long pr_starttime(long pid) {
    if (pid <= 0) return 0;
    char p[64], buf[1024];
    snprintf(p, sizeof p, "/proc/%ld/stat", pid);
    FILE *f = fopen(p, "r");
    if (!f) return 0;
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n] = 0;
    char *rp = strrchr(buf, ')');
    if (!rp) return 0;
    char state = 0; long v = 0;
    /* after ") ": state ppid pgrp session tty tpgid flags minflt cminflt majflt cmajflt utime stime cutime cstime priority nice threads itrealvalue starttime */
    if (sscanf(rp + 2, "%c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %*u %*u %*d %*d %*d %*d %*d %*d %ld", &state, &v) != 2) return 0;
    if (state == 'Z' || state == 'X') return 0;       /* a zombie is not alive */
    return v > 0 ? v : 0;
}

static int pr_owner_alive(long owner_pid, long owner_start) {
    if (owner_pid <= 0) return 1;
    long s = pr_starttime(owner_pid);
    if (!s) return 0;
    return owner_start <= 0 || s == owner_start;      /* 0 = was unknown at registration: liveness only */
}

static void khp_clean(char *s) { for (; *s; s++) if (*s == '|' || *s == '\n' || *s == '\r' || *s == '\t') *s = '_'; }

static int khp_lock(const char *house) {
    char p[PATH_MAX]; khp_path(house, ".proc_registry.lock", p, sizeof p);
    int fd = open(p, O_RDWR | O_CREAT | O_CLOEXEC, 0644);
    if (fd >= 0) flock(fd, LOCK_EX);
    return fd;
}
static void khp_unlock(int fd) { if (fd >= 0) { flock(fd, LOCK_UN); close(fd); } }

static void khp_log(const char *house, const char *kind, long pid, const char *detail) {
    char p[PATH_MAX]; khp_path(house, "proc_registry.log.txt", p, sizeof p);
    struct stat st;
    if (stat(p, &st) == 0 && st.st_size > KHP_LOG_MAX) {            /* cap: keep the newest quarter */
        FILE *f = fopen(p, "r");
        if (f) {
            fseek(f, st.st_size - KHP_LOG_MAX / 4, SEEK_SET);
            char *keep = malloc(KHP_LOG_MAX / 4 + 1); size_t n = keep ? fread(keep, 1, KHP_LOG_MAX / 4, f) : 0; fclose(f);
            if (keep) { keep[n] = 0; char *nl = strchr(keep, '\n'); char tmp[PATH_MAX + 8]; snprintf(tmp, sizeof tmp, "%s.tmp", p);
                FILE *o = fopen(tmp, "w"); if (o) { fputs(nl ? nl + 1 : keep, o); fclose(o); rename(tmp, p); } free(keep); }
        }
    }
    FILE *f = fopen(p, "a");
    if (f) { fprintf(f, "%ld %s %ld %s\n", (long)time(NULL), kind, pid, detail ? detail : ""); fclose(f); }
}

static int khp_load(const char *house, KhpRow *rows, int max) {
    char p[PATH_MAX]; khp_path(house, "proc_registry.pdl", p, sizeof p);
    FILE *f = fopen(p, "r");
    if (!f) return 0;
    int n = 0; char line[KHP_LINE];
    while (n < max && fgets(line, sizeof line, f)) {
        if (strncmp(line, "PROC |", 6) != 0) continue;
        KhpRow *r = &rows[n]; memset(r, 0, sizeof *r);
        char life[16] = "";
        char *c = line + 6; char *tok[12]; int k = 0;
        while (k < 11) { tok[k++] = c; char *bar = strstr(c, " | "); if (!bar) break; *bar = 0; c = bar + 3; }
        if (k < 11) { /* cmd is the last field */ continue; }
        tok[11] = c; c[strcspn(c, "\r\n")] = 0;
        for (int i = 0; i < 11; i++) while (*tok[i] == ' ') tok[i]++;
        r->pid = atol(tok[0]); r->start = atol(tok[1]); r->pgid = atol(tok[2]);
        snprintf(life, sizeof life, "%s", tok[3]); r->lifetime = !strcmp(life, "keep") ? PR_KEEP : !strcmp(life, "house") ? PR_HOUSE : PR_OWNER;
        r->owner = atol(tok[4]); r->owner_start = atol(tok[5]);
        snprintf(r->role, sizeof r->role, "%s", tok[6]); snprintf(r->app, sizeof r->app, "%s", tok[7]); snprintf(r->name, sizeof r->name, "%s", tok[8]);
        r->started = atol(tok[9]); snprintf(r->cmd, sizeof r->cmd, "%s", tok[10]);
        /* tok[11] is the tail beyond the cmd (none) */
        if (r->pid > 1) n++;
    }
    fclose(f);
    return n;
}

static const char *khp_life(int l) { return l == PR_KEEP ? "keep" : l == PR_HOUSE ? "house" : "owner"; }

static void khp_write_row(FILE *f, const KhpRow *r) {
    fprintf(f, "PROC | %ld | %ld | %ld | %s | %ld | %ld | %s | %s | %s | %ld | %s\n", r->pid, r->start, r->pgid, khp_life(r->lifetime), r->owner, r->owner_start,
            r->role, r->app, r->name, r->started, r->cmd);
}

/* the legacy v1 ledger, kept in step so the existing taskbar reaper reaches v2 rows */
static void khp_legacy_add(const char *house, const KhpRow *r) {
    char p[PATH_MAX]; khp_path(house, "livedesk_proc_list.txt", p, sizeof p);
    FILE *f = fopen(p, "a");
    if (f) { fprintf(f, "%ld %ld %ld %ld %s\n", r->pid, r->pgid, r->owner > 0 ? r->owner : 0, r->start, r->name); fclose(f); }
}
static void khp_legacy_drop(const char *house, long pid) {
    char p[PATH_MAX], tmp[PATH_MAX + 8], line[KHP_LINE]; khp_path(house, "livedesk_proc_list.txt", p, sizeof p); snprintf(tmp, sizeof tmp, "%s.tmp", p);
    FILE *f = fopen(p, "r"); if (!f) return;
    FILE *o = fopen(tmp, "w"); if (!o) { fclose(f); return; }
    int dropped = 0;
    while (fgets(line, sizeof line, f)) { if (atol(line) == pid) { dropped = 1; continue; } fputs(line, o); }
    fclose(f);
    if (fclose(o) == 0 && dropped) rename(tmp, p); else remove(tmp);
}

static void khp_save(const char *house, const KhpRow *rows, int n) {
    char p[PATH_MAX], tmp[PATH_MAX + 8]; khp_path(house, "proc_registry.pdl", p, sizeof p); snprintf(tmp, sizeof tmp, "%s.%ld.tmp", p, (long)getpid());
    FILE *o = fopen(tmp, "w");
    if (!o) return;
    fputs("# proc_registry.pdl - one row per live house process (ops/proc_reg_op.c). A process not listed here is a violation: proc_reg_op audit. Plan: PROC-REGISTRY-STANDARD-AND-REFACTOR-PLAN-2026-11-02.md\n", o);
    for (int i = 0; i < n; i++) khp_write_row(o, &rows[i]);
    if (fclose(o) == 0) rename(tmp, p); else remove(tmp);
}

static int pr_register(const char *house, long pid, int lifetime, long owner_pid, const char *role, const char *app, const char *name, const char *cmd_override) {
    if (!house || pid <= 1) return -1;
    long st = pr_starttime(pid);
    if (!st) return -1;                                   /* not alive: nothing to register */
    if (lifetime == PR_OWNER && owner_pid <= 0) return -1;      /* an owned process must name its owner */
    KhpRow r; memset(&r, 0, sizeof r);
    r.pid = pid; r.start = st; r.pgid = (long)getpgid((pid_t)pid); if (r.pgid <= 0) r.pgid = pid;
    r.lifetime = lifetime; r.owner = owner_pid > 0 ? owner_pid : 0; r.owner_start = owner_pid > 0 ? pr_starttime(owner_pid) : 0;
    snprintf(r.role, sizeof r.role, "%s", role && *role ? role : "daemon"); snprintf(r.app, sizeof r.app, "%s", app && *app ? app : "-");
    snprintf(r.name, sizeof r.name, "%s", name && *name ? name : "proc"); r.started = (long)time(NULL);
    { char p[64], raw[512]; snprintf(p, sizeof p, "/proc/%ld/cmdline", pid); FILE *f = fopen(p, "r");
      size_t n = f ? fread(raw, 1, sizeof raw - 1, f) : 0; if (f) fclose(f); raw[n] = 0;
      for (size_t i = 0; i + 1 < n; i++) if (!raw[i]) raw[i] = ' ';
      snprintf(r.cmd, sizeof r.cmd, "%s", cmd_override && *cmd_override ? cmd_override : raw); }      /* a launcher registers BEFORE exec: it passes the command it is about to run */
    khp_clean(r.role); khp_clean(r.app); khp_clean(r.name); khp_clean(r.cmd);
    int lk = khp_lock(house);
    static KhpRow rows[KHP_MAXROWS];
    int n = khp_load(house, rows, KHP_MAXROWS - 1), at = -1;
    for (int i = 0; i < n; i++) if (rows[i].pid == pid) at = i;
    if (at >= 0) { khp_legacy_drop(house, pid); rows[at] = r; } else rows[n++] = r;     /* re-registering a pid replaces its row (an exec'd script) */
    khp_save(house, rows, n);
    khp_legacy_add(house, &r);
    char d[300]; snprintf(d, sizeof d, "%s app=%s name=%s life=%s owner=%ld", r.role, r.app, r.name, khp_life(lifetime), r.owner);
    khp_log(house, "START", pid, d);
    khp_unlock(lk);
    return 0;
}

static int pr_unregister(const char *house, long pid, const char *why) {
    int lk = khp_lock(house);
    static KhpRow rows[KHP_MAXROWS];
    int n = khp_load(house, rows, KHP_MAXROWS), j = 0, found = 0;
    for (int i = 0; i < n; i++) { if (rows[i].pid == pid) { found = 1; continue; } rows[j++] = rows[i]; }
    if (found) { khp_save(house, rows, j); khp_legacy_drop(house, pid); khp_log(house, "END", pid, why ? why : "exit"); }
    khp_unlock(lk);
    return found;
}

static int pr_prune(const char *house) {
    int lk = khp_lock(house);
    static KhpRow rows[KHP_MAXROWS];
    int n = khp_load(house, rows, KHP_MAXROWS), j = 0;
    for (int i = 0; i < n; i++) {
        if (pr_starttime(rows[i].pid) == rows[i].start) rows[j++] = rows[i];
        else { khp_legacy_drop(house, rows[i].pid); khp_log(house, "END", rows[i].pid, "gone"); }
    }
    if (j != n) khp_save(house, rows, j);
    khp_unlock(lk);
    return n - j;
}

/* an owner must be seen dead for orphan_grace_s before its process is ended: a window relaunch (old window gone, new one not registered yet) must not cost the engine
 * and daemon it is about to adopt again. First sighting is recorded in #.desktop/.proc_orphan_seen ("<pid> <start> <first_seen_epoch>"); the file is rewritten each pass
 * to hold only the rows still orphaned, so it cannot grow. */
typedef struct { long pid, start, first; } PrSeen;
static int pr_seen_load(const char *house, PrSeen *v, int max) {
    char p[PATH_MAX]; khp_path(house, ".proc_orphan_seen", p, sizeof p);
    FILE *f = fopen(p, "r"); int n = 0; if (!f) return 0;
    while (n < max && fscanf(f, "%ld %ld %ld", &v[n].pid, &v[n].start, &v[n].first) == 3) n++;
    fclose(f); return n;
}
static void pr_seen_save(const char *house, const PrSeen *v, int n) {
    char p[PATH_MAX]; khp_path(house, ".proc_orphan_seen", p, sizeof p);
    if (!n) { remove(p); return; }
    FILE *f = fopen(p, "w"); if (!f) return;
    for (int i = 0; i < n; i++) fprintf(f, "%ld %ld %ld\n", v[i].pid, v[i].start, v[i].first);
    fclose(f);
}

static int pr_reap_orphans(const char *house, int grace_ms, int orphan_grace_s, int dry_run, int verbose) {
    int lk = khp_lock(house);
    static KhpRow rows[KHP_MAXROWS]; static long victims[KHP_MAXROWS], vstart[KHP_MAXROWS], vpgid[KHP_MAXROWS];
    int n = khp_load(house, rows, KHP_MAXROWS), nv = 0, j = 0;
    long me = (long)getpid(), mypg = (long)getpgrp();
    static KhpRow keep[KHP_MAXROWS]; static PrSeen seen[KHP_MAXROWS], seen_out[KHP_MAXROWS]; int nseen = pr_seen_load(house, seen, KHP_MAXROWS), nseen_out = 0;
    for (int i = 0; i < n; i++) {
        KhpRow *r = &rows[i];
        int alive = pr_starttime(r->pid) == r->start;
        if (!alive) { khp_legacy_drop(house, r->pid); khp_log(house, "END", r->pid, "gone"); continue; }      /* dead or pid reused: drop the row, never signal */
        if (r->lifetime == PR_OWNER && !pr_owner_alive(r->owner, r->owner_start) && r->pid != me) {
            long first = 0; for (int k = 0; k < nseen; k++) if (seen[k].pid == r->pid && seen[k].start == r->start) first = seen[k].first;
            if (!first) first = (long)time(NULL);
            if (nseen_out < KHP_MAXROWS) { seen_out[nseen_out].pid = r->pid; seen_out[nseen_out].start = r->start; seen_out[nseen_out].first = first; nseen_out++; }
            if ((long)time(NULL) - first < orphan_grace_s) { if (verbose) fprintf(stderr, "WAIT %ld %s/%s owner %ld gone %lds ago\n", r->pid, r->app, r->name, r->owner, (long)time(NULL) - first); keep[j++] = *r; continue; }
            if (verbose) fprintf(stderr, "%s %ld %s/%s owner %ld gone\n", dry_run ? "WOULD-END" : "END", r->pid, r->app, r->name, r->owner);
            if (dry_run) { keep[j++] = *r; continue; }
            victims[nv] = r->pid; vstart[nv] = r->start; vpgid[nv] = r->pgid; nv++;
            continue;                                                  /* its row is dropped below */
        }
        keep[j++] = *r;
    }
    for (int i = 0; i < nv; i++) {
        kill((pid_t)victims[i], SIGTERM);
        if (vpgid[i] == victims[i] && vpgid[i] != mypg) kill((pid_t)-vpgid[i], SIGTERM);      /* a setsid leader: its whole group */
    }
    if (nv) {
        usleep((useconds_t)(grace_ms > 0 ? grace_ms : 300) * 1000);
        for (int i = 0; i < nv; i++) {
            if (pr_starttime(victims[i]) == vstart[i]) {            /* still the same process: escalate */
                kill((pid_t)victims[i], SIGKILL);
                if (vpgid[i] == victims[i] && vpgid[i] != mypg) kill((pid_t)-vpgid[i], SIGKILL);
            }
            waitpid((pid_t)victims[i], NULL, WNOHANG);
            khp_legacy_drop(house, victims[i]);
            khp_log(house, "END", victims[i], "orphan");
        }
    }
    if (j != n) khp_save(house, keep, j);
    pr_seen_save(house, seen_out, nseen_out);
    khp_unlock(lk);
    return dry_run ? 0 : nv;
}

static int pr_audit(const char *house, int min_age_s, char *out, size_t cap) {
    char hreal[PATH_MAX];
    if (!realpath(house, hreal)) return -1;
    size_t hl = strlen(hreal);
    static KhpRow rows[KHP_MAXROWS];
    int n = khp_load(house, rows, KHP_MAXROWS), count = 0;
    DIR *d = opendir("/proc");
    if (!d) return -1;
    if (out && cap) out[0] = 0;
    struct dirent *e; long hz = sysconf(_SC_CLK_TCK); if (hz <= 0) hz = 100;
    double up = 0; { FILE *f = fopen("/proc/uptime", "r"); if (f) { if (fscanf(f, "%lf", &up) != 1) up = 0; fclose(f); } }
    while ((e = readdir(d))) {
        if (e->d_name[0] < '1' || e->d_name[0] > '9') continue;
        long pid = atol(e->d_name);
        char p[64], exe[PATH_MAX];
        snprintf(p, sizeof p, "/proc/%ld/exe", pid);
        ssize_t l = readlink(p, exe, sizeof exe - 1);
        if (l <= 0) continue;
        exe[l] = 0; char *del = strstr(exe, " (deleted)"); if (del) *del = 0;
        if (strncmp(exe, hreal, hl) != 0 || (exe[hl] != '/' && exe[hl] != 0)) continue;
        if (pid == (long)getpid()) continue;
        long st = pr_starttime(pid); if (!st) continue;
        if (min_age_s > 0 && up > 0 && up - (double)st / (double)hz < min_age_s) continue;   /* too young: it may be mid-startup */
        int listed = 0;
        for (int i = 0; i < n; i++) if (rows[i].pid == pid && rows[i].start == st) { listed = 1; break; }
        if (listed) continue;
        count++;
        if (out && cap) {
            char cmd[160] = ""; snprintf(p, sizeof p, "/proc/%ld/cmdline", pid); FILE *f = fopen(p, "r");
            if (f) { size_t k = fread(cmd, 1, sizeof cmd - 1, f); fclose(f); cmd[k] = 0; for (size_t i = 0; i + 1 < k; i++) if (!cmd[i]) cmd[i] = ' '; }
            size_t used = strlen(out);
            if (used + 400 < cap) snprintf(out + used, cap - used, "VIOLATION %ld %s | %s\n", pid, exe + hl, cmd);
        }
    }
    closedir(d);
    return count;
}

/* adopt: register every house process whose exe starts with `prefix` and that has no row yet, owned by `owner_pid`. For a stack that detaches from the process we registered
 * (the pchq engine's session: trigger watcher, clock daemon, board-viewer prisc / parser / renderer): the PREFIX is an explicit declaration made by the launcher, and the
 * result is rows in the registry file, so what is adopted is auditable and ends with its owner. Returns how many were adopted. */
static int pr_adopt(const char *house, long owner_pid, const char *prefix, const char *role, const char *app, const char *name) {
    static KhpRow rows[KHP_MAXROWS];
    int n = khp_load(house, rows, KHP_MAXROWS), adopted = 0;
    DIR *d = opendir("/proc"); if (!d) return 0;
    size_t pl = strlen(prefix); struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] < '1' || e->d_name[0] > '9') continue;
        long pid = atol(e->d_name); char p[64], exe[PATH_MAX];
        snprintf(p, sizeof p, "/proc/%ld/exe", pid);
        ssize_t l = readlink(p, exe, sizeof exe - 1); if (l <= 0) continue;
        exe[l] = 0; char *del = strstr(exe, " (deleted)"); if (del) *del = 0;
        if (strncmp(exe, prefix, pl) != 0) continue;
        long st = pr_starttime(pid); if (!st) continue;
        int listed = 0; for (int i = 0; i < n; i++) if (rows[i].pid == pid && rows[i].start == st) { listed = 1; break; }
        if (listed) continue;
        if (pr_register(house, pid, PR_OWNER, owner_pid, role, app, name, NULL) == 0) adopted++;
    }
    closedir(d);
    return adopted;
}

/* adoption RULES: a declaration in #.desktop/proc_adopt_rules.pdl, applied on every registry pass while the owner lives, because a stack respawns (the pchq engine makes a new
 * session when the board reloads its book) and a one-time adopt misses the new processes.
 *   RULE | owner_pid | owner_start | prefix | role | app | name
 * prefix is an exe-path prefix, or "@file:<path>#<key>": the value of key=<value> in that file (a board's state/ui.txt bv_session=), re-read at every pass.
 * A rule whose owner is gone is dropped. What a rule adopts becomes rows in proc_registry.pdl, so it is all auditable and ends with the owner. */
typedef struct { long owner, owner_start; char prefix[PATH_MAX], role[24], app[40], name[48]; } PrRule;
static int pr_rules_load(const char *house, PrRule *r, int max) {
    char p[PATH_MAX]; khp_path(house, "proc_adopt_rules.pdl", p, sizeof p);
    FILE *f = fopen(p, "r"); int n = 0; if (!f) return 0;
    char line[PATH_MAX + 256];
    while (n < max && fgets(line, sizeof line, f)) {
        if (strncmp(line, "RULE | ", 7) != 0) continue;
        char *tok[6]; char *c = line + 7; int k = 0;
        while (k < 6) { tok[k++] = c; char *bar = strstr(c, " | "); if (!bar) break; *bar = 0; c = bar + 3; }
        if (k < 6) continue;
        c[strcspn(c, "\r\n")] = 0;
        r[n].owner = atol(tok[0]); r[n].owner_start = atol(tok[1]);
        snprintf(r[n].prefix, sizeof r[n].prefix, "%s", tok[2]); snprintf(r[n].role, sizeof r[n].role, "%s", tok[3]);
        snprintf(r[n].app, sizeof r[n].app, "%s", tok[4]); snprintf(r[n].name, sizeof r[n].name, "%s", tok[5]);
        n++;
    }
    fclose(f); return n;
}
static void pr_rules_save(const char *house, const PrRule *r, int n) {
    char p[PATH_MAX], tmp[PATH_MAX + 8]; khp_path(house, "proc_adopt_rules.pdl", p, sizeof p); snprintf(tmp, sizeof tmp, "%s.tmp", p);
    if (!n) { remove(p); return; }
    FILE *o = fopen(tmp, "w"); if (!o) return;
    fputs("# proc_adopt_rules.pdl - declared ownership of detached stacks (proc_reg_op rule-add / apply-rules). A rule ends with its owner.\n", o);
    for (int i = 0; i < n; i++) fprintf(o, "RULE | %ld | %ld | %s | %s | %s | %s\n", r[i].owner, r[i].owner_start, r[i].prefix, r[i].role, r[i].app, r[i].name);
    if (fclose(o) == 0) rename(tmp, p); else remove(tmp);
}
static int pr_rule_add(const char *house, long owner, const char *prefix, const char *role, const char *app, const char *name) {
    long st = pr_starttime(owner); if (!st) return -1;
    static PrRule r[256];
    int lk = khp_lock(house);
    int n = pr_rules_load(house, r, 255), at = -1;
    for (int i = 0; i < n; i++) if (r[i].owner == owner && !strcmp(r[i].prefix, prefix)) at = i;
    if (at < 0) at = n++;
    r[at].owner = owner; r[at].owner_start = st;
    snprintf(r[at].prefix, sizeof r[at].prefix, "%s", prefix); snprintf(r[at].role, sizeof r[at].role, "%s", role);
    snprintf(r[at].app, sizeof r[at].app, "%s", app); snprintf(r[at].name, sizeof r[at].name, "%s", name);
    khp_clean(r[at].role); khp_clean(r[at].app); khp_clean(r[at].name);
    pr_rules_save(house, r, n);
    khp_unlock(lk);
    return 0;
}
static int pr_adopt(const char *house, long owner_pid, const char *prefix, const char *role, const char *app, const char *name);
static int pr_apply_rules(const char *house) {
    static PrRule r[256], keep[256];
    int lk = khp_lock(house);
    int n = pr_rules_load(house, r, 256), j = 0;
    for (int i = 0; i < n; i++) if (pr_owner_alive(r[i].owner, r[i].owner_start)) keep[j++] = r[i];
    if (j != n) pr_rules_save(house, keep, j);
    khp_unlock(lk);
    int adopted = 0;
    for (int i = 0; i < j; i++) {
        char prefix[PATH_MAX];
        if (!strncmp(keep[i].prefix, "@file:", 6)) {       /* @file:<path>#<key> */
            char path[PATH_MAX]; snprintf(path, sizeof path, "%s", keep[i].prefix + 6);
            char *hash = strrchr(path, '#'); if (!hash) continue; *hash = 0;
            char want[64]; snprintf(want, sizeof want, "%s=", hash + 1);
            FILE *f = fopen(path, "r"); if (!f) continue;
            char line[PATH_MAX + 64]; prefix[0] = 0;
            while (fgets(line, sizeof line, f)) if (!strncmp(line, want, strlen(want))) { line[strcspn(line, "\r\n")] = 0; snprintf(prefix, sizeof prefix, "%s/", line + strlen(want)); }
            fclose(f);
            if (strlen(prefix) < 3) continue;              /* never adopt on an empty value */
        } else snprintf(prefix, sizeof prefix, "%s", keep[i].prefix);
        adopted += pr_adopt(house, keep[i].owner, prefix, keep[i].role, keep[i].app, keep[i].name);
    }
    return adopted;
}

static int life_of(const char *s) { return !strcmp(s, "keep") ? PR_KEEP : !strcmp(s, "house") ? PR_HOUSE : PR_OWNER; }

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: proc_reg_op <house> register|unregister|list|prune|reap-orphans|audit|owner-alive ...\n"); return 2; }
    const char *house = argv[1], *cmd = argv[2];
    if (!strcmp(cmd, "register")) {
        if (argc < 9) { fprintf(stderr, "usage: proc_reg_op <house> register <pid> <owner|keep|house> <owner_pid> <role> <app> <name>\n"); return 2; }
        return pr_register(house, atol(argv[3]), life_of(argv[4]), atol(argv[5]), argv[6], argv[7], argv[8], argc > 9 ? argv[9] : NULL) == 0 ? 0 : 1;
    }
    if (!strcmp(cmd, "unregister")) { if (argc < 4) return 2; pr_unregister(house, atol(argv[3]), "unregister"); return 0; }
    if (!strcmp(cmd, "list")) {
        char p[PATH_MAX]; snprintf(p, sizeof p, "%s/#.desktop/proc_registry.pdl", house);
        FILE *f = fopen(p, "r"); if (!f) return 0;
        char line[1200]; while (fgets(line, sizeof line, f)) fputs(line, stdout); fclose(f); return 0;
    }
    if (!strcmp(cmd, "prune")) { printf("%d\n", pr_prune(house)); return 0; }
    if (!strcmp(cmd, "reap-orphans")) {
        int grace = argc > 3 ? atoi(argv[3]) : 300, og = argc > 4 ? atoi(argv[4]) : 15, dry = argc > 5 && !strcmp(argv[5], "dry");
        pr_apply_rules(house);      /* adopt what the declared rules say belongs to a live owner, THEN end what is orphaned */
        int n = pr_reap_orphans(house, grace, og, dry, 1);
        printf("%d\n", n); return 0;
    }
    if (!strcmp(cmd, "audit")) {
        static char out[16384]; int age = argc > 3 ? atoi(argv[3]) : 5;
        int n = pr_audit(house, age, out, sizeof out);
        if (n < 0) return 1;
        char p[PATH_MAX], tmp[PATH_MAX + 8]; snprintf(p, sizeof p, "%s/#.desktop/proc_violations.txt", house); snprintf(tmp, sizeof tmp, "%s.tmp", p);
        FILE *f = fopen(tmp, "w");      /* rewritten each audit: the CURRENT violations, never a growing log */
        if (f) { fprintf(f, "# house processes with no row in proc_registry.pdl (proc_reg_op audit)\n%s", out); fclose(f); rename(tmp, p); }
        fputs(out, stdout); printf("violations=%d\n", n);
        return n ? 3 : 0;
    }
    if (!strcmp(cmd, "adopt")) {
        if (argc < 8) { fprintf(stderr, "usage: proc_reg_op <house> adopt <owner_pid> <exe_prefix> <role> <app> <name>\n"); return 2; }
        printf("%d\n", pr_adopt(house, atol(argv[3]), argv[4], argv[5], argv[6], argv[7])); return 0;
    }
    if (!strcmp(cmd, "rule-add")) {
        if (argc < 8) { fprintf(stderr, "usage: proc_reg_op <house> rule-add <owner_pid> <exe_prefix|@file:path#key> <role> <app> <name>\n"); return 2; }
        return pr_rule_add(house, atol(argv[3]), argv[4], argv[5], argv[6], argv[7]) == 0 ? 0 : 1;
    }
    if (!strcmp(cmd, "apply-rules")) { printf("%d\n", pr_apply_rules(house)); return 0; }
    if (!strcmp(cmd, "owner-alive")) { if (argc < 5) return 2; return pr_owner_alive(atol(argv[3]), atol(argv[4])) ? 0 : 1; }
    if (!strcmp(cmd, "hold")) { sleep((unsigned)(argc > 3 ? atoi(argv[3]) : 60)); return 0; }
    fprintf(stderr, "proc_reg_op: unknown command %s\n", cmd);
    return 2;
}
