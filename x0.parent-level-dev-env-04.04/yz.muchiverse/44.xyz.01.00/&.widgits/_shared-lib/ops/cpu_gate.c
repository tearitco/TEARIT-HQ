/* cpu_gate.c - the pre-launch gate for heavy work (owner 2026-11-03, after the CPU guard ended 31 house processes: HQ-IQ-BOOK 03-pitfalls/hq-cpu-safety.md section 10).
 * The CPU guard (proc-mon/ops/cpu_guard.c) reacts AFTER the machine is hot. This asks BEFORE: a compile, an engine start, a screenshot loop, a TTS batch.
 *   cpu_gate <house> check [limits]                    sample the machine now; exit 0 ok, 3 too busy (the reason on stderr)
 *   cpu_gate <house> run <label> [limits] [--wait S] -- <cmd> [args]
 *                                                      the same check (waiting up to S s for the machine to calm down), then ONE heavy job at a time (flock on
 *                                                      #.desktop/heavy.lock; exit 4 when another holds it and S ran out), then <cmd> runs niced 15 at idle I/O
 *                                                      priority; its exit status is returned. One row per run in #.desktop/heavy_runs.txt (capped at 200 rows).
 * limits (defaults come from #.desktop/proc_mon_guard.pdl warn_pct / temp_warn_c, else 70 / 85):  --max-busy <pct>  --max-temp <C>  --max-house <pct of the machine, default 30>
 * busy = the whole machine over a 1 s sample (/proc/stat); house = CPU used by processes whose command line names this house, over the same second, as a percent of the machine;
 * temp = the hottest /sys/class/thermal zone (0 when unreadable, then the check ignores it).
 * Plain fork/exec, no shell. Exit: 0 ok / child's status, 2 usage, 3 busy, 4 heavy lock held. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <dirent.h>
#include <limits.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/syscall.h>

static char house[PATH_MAX];
static int read_total(unsigned long long *busy, unsigned long long *all) {
    FILE *f = fopen("/proc/stat", "r"); if (!f) return 0; unsigned long long u, n, s, i, w, q, sq, st; int k = fscanf(f, "cpu %llu %llu %llu %llu %llu %llu %llu %llu", &u, &n, &s, &i, &w, &q, &sq, &st); fclose(f);
    if (k < 8) return 0; *all = u + n + s + i + w + q + sq + st; *busy = *all - i - w; return 1;
}
/* clock ticks used by every process whose command line names the house */
static unsigned long long house_ticks(void) {
    DIR *d = opendir("/proc"); if (!d) return 0; struct dirent *e; unsigned long long sum = 0; static char cmd[8192], st[2048];
    while ((e = readdir(d))) {
        if (e->d_name[0] < '0' || e->d_name[0] > '9') continue; char p[64]; snprintf(p, sizeof p, "/proc/%s/cmdline", e->d_name); FILE *f = fopen(p, "r"); if (!f) continue;
        size_t n = fread(cmd, 1, sizeof cmd - 1, f); fclose(f); cmd[n] = 0; for (size_t j = 0; j + 1 < n; j++) if (!cmd[j]) cmd[j] = ' '; if (!strstr(cmd, house)) continue;
        snprintf(p, sizeof p, "/proc/%s/stat", e->d_name); f = fopen(p, "r"); if (!f) continue; n = fread(st, 1, sizeof st - 1, f); fclose(f); st[n] = 0;
        char *rp = strrchr(st, ')'); if (!rp) continue; unsigned long long ut = 0, stt = 0; int fld = 0; char *tok = strtok(rp + 2, " ");
        while (tok) { fld++; if (fld == 12) ut = strtoull(tok, NULL, 10); else if (fld == 13) { stt = strtoull(tok, NULL, 10); break; } tok = strtok(NULL, " "); }
        sum += ut + stt;
    }
    closedir(d); return sum;
}
static int hottest(void) {
    int best = 0; for (int z = 0; z < 24; z++) { char p[96]; snprintf(p, sizeof p, "/sys/class/thermal/thermal_zone%d/temp", z); FILE *f = fopen(p, "r"); if (!f) continue; long v = 0; if (fscanf(f, "%ld", &v) == 1 && v / 1000 > best && v / 1000 < 150) best = (int)(v / 1000); fclose(f); }
    return best;
}
static int pdl_int(const char *key, int def) {
    char p[PATH_MAX]; snprintf(p, sizeof p, "%s/#.desktop/proc_mon_guard.pdl", house); FILE *f = fopen(p, "r"); if (!f) return def; char ln[200]; size_t kl = strlen(key); int v = def;
    while (fgets(ln, sizeof ln, f)) if (!strncmp(ln, key, kl) && ln[kl] == '=') v = atoi(ln + kl + 1); fclose(f); return v;
}
typedef struct { int busy, temp, hse; } Limits; static Limits L;
/* one sample; returns 0 ok, 3 too busy (reason in why) */
static int sample(char *why, size_t wn, int *busy_out) {
    unsigned long long b0, a0, b1, a1; long tick = sysconf(_SC_CLK_TCK), ncpu = sysconf(_SC_NPROCESSORS_ONLN); if (!read_total(&b0, &a0)) return 0; unsigned long long h0 = house_ticks(); struct timespec ts = { 1, 0 }; nanosleep(&ts, NULL);
    if (!read_total(&b1, &a1)) return 0; unsigned long long h1 = house_ticks(); double tot = (double)(a1 - a0); if (tot < 1) tot = 1;
    int busy = (int)(100.0 * (double)(b1 - b0) / tot + 0.5), hse = (int)(100.0 * (double)(h1 >= h0 ? h1 - h0 : 0) / tot + 0.5); int temp = hottest(); (void)tick; (void)ncpu;
    if (busy_out) *busy_out = busy; snprintf(why, wn, "machine %d%% busy, house %d%% of the machine, %d C", busy, hse, temp);
    if (busy >= L.busy) { snprintf(why, wn, "machine %d%% busy (limit %d%%)", busy, L.busy); return 3; }
    if (temp && temp >= L.temp) { snprintf(why, wn, "die temperature %d C (limit %d C)", temp, L.temp); return 3; }
    if (hse >= L.hse) { snprintf(why, wn, "the house already uses %d%% of the machine (limit %d%%)", hse, L.hse); return 3; }
    return 0;
}
static void trim_log(const char *p) {
    FILE *f = fopen(p, "r"); if (!f) return; static char rows[400][300]; int n = 0; while (n < 400 && fgets(rows[n], sizeof rows[0], f)) n++; int more = !feof(f); fclose(f);
    if (n < 399 && !more) return; int from = n > 200 ? n - 200 : 0; char tmp[PATH_MAX + 8]; snprintf(tmp, sizeof tmp, "%s.tmp", p); FILE *o = fopen(tmp, "w"); if (!o) return;
    for (int i = from; i < n; i++) fputs(rows[i], o); if (fclose(o) == 0) rename(tmp, p); else remove(tmp);
}
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: cpu_gate <house> check [limits] | run <label> [limits] [--wait S] -- <cmd...>\n"); return 2; }
    snprintf(house, sizeof house, "%s", argv[1]); const char *verb = argv[2];
    L.busy = pdl_int("warn_pct", 70); L.temp = pdl_int("temp_warn_c", 85); L.hse = 30; int wait_s = 0, ci = -1; const char *label = "heavy";
    int i = 3; if (!strcmp(verb, "run")) { if (argc < 4) return 2; label = argv[3]; i = 4; } else if (strcmp(verb, "check")) { fprintf(stderr, "cpu_gate: unknown verb %s\n", verb); return 2; }
    for (; i < argc; i++) {
        if (!strcmp(argv[i], "--")) { ci = i + 1; break; }
        else if (!strcmp(argv[i], "--max-busy") && i + 1 < argc) L.busy = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--max-temp") && i + 1 < argc) L.temp = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--max-house") && i + 1 < argc) L.hse = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--wait") && i + 1 < argc) wait_s = atoi(argv[++i]);
        else { fprintf(stderr, "cpu_gate: unknown argument %s\n", argv[i]); return 2; }
    }
    char why[200]; int busy = 0; time_t t0 = time(NULL); int rc;
    while ((rc = sample(why, sizeof why, &busy)) != 0 && (time(NULL) - t0) < wait_s) { fprintf(stderr, "cpu_gate: waiting: %s\n", why); sleep(4); }
    if (rc) { fprintf(stderr, "cpu_gate: NOT NOW: %s\n", why); return 3; }
    if (!strcmp(verb, "check")) { printf("ok: %s\n", why); return 0; }
    if (ci < 0 || ci >= argc) { fprintf(stderr, "cpu_gate: run needs -- <cmd>\n"); return 2; }
    char lp[PATH_MAX]; snprintf(lp, sizeof lp, "%s/#.desktop/heavy.lock", house); int lfd = open(lp, O_CREAT | O_RDWR, 0644); if (lfd < 0) { perror("cpu_gate: heavy.lock"); return 2; }
    while (flock(lfd, LOCK_EX | LOCK_NB) != 0) {
        if (time(NULL) - t0 >= wait_s) { char who[200] = ""; ssize_t n = pread(lfd, who, sizeof who - 1, 0); if (n > 0) who[n] = 0; who[strcspn(who, "\n")] = 0; fprintf(stderr, "cpu_gate: NOT NOW: another heavy job holds the lock (%s)\n", who[0] ? who : "unknown"); return 4; }
        sleep(2);
    }
    { char me[200]; int n = snprintf(me, sizeof me, "%s pid=%d since=%ld\n", label, (int)getpid(), (long)time(NULL)); if (ftruncate(lfd, 0) == 0) { ssize_t w = pwrite(lfd, me, (size_t)n, 0); (void)w; } }
    time_t t1 = time(NULL); pid_t pid = fork(); if (pid < 0) return 2;
    if (pid == 0) {
        setpriority(PRIO_PROCESS, 0, 15); syscall(SYS_ioprio_set, 1, 0, (3 << 13));      /* idle I/O class */
        close(lfd);      /* the job does not hold the lock: the gate does, for as long as the job runs */
        execvp(argv[ci], &argv[ci]); fprintf(stderr, "cpu_gate: cannot run %s: %s\n", argv[ci], strerror(errno)); _exit(127);
    }
    int st = 0; waitpid(pid, &st, 0); int ec = WIFEXITED(st) ? WEXITSTATUS(st) : 128 + (WIFSIGNALED(st) ? WTERMSIG(st) : 0);
    char gp[PATH_MAX]; snprintf(gp, sizeof gp, "%s/#.desktop/heavy_runs.txt", house); FILE *g = fopen(gp, "a"); if (g) { fprintf(g, "%ld | %s | rc=%d | %lds | busy_before=%d%% | %.60s\n", (long)t1, label, ec, (long)(time(NULL) - t1), busy, argv[ci]); fclose(g); trim_log(gp); }
    flock(lfd, LOCK_UN); close(lfd); return ec;
}
