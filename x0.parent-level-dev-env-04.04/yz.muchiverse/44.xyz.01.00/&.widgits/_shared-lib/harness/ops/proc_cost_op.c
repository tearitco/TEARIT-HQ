/* proc_cost_op - what one process costs, measured the way CPU-OPTIMIZATION-PITFALLS 26 asks (children and wakeups, not `top`). 2026-11-09.
 *
 *   proc_cost_op <pid> <seconds> [sample_ms]
 *
 * Samples /proc/<pid>/stat (utime+stime = fields 14+15, cutime+cstime = 16+17: children it has WAITED for), /proc/<pid>/status (voluntary_ctxt_switches, nonvoluntary_ctxt_switches, Threads) and the number of
 * live children (processes whose ppid is <pid>, scanned every sample_ms, default 100) at the start and the end of a window of <seconds>, plus the whole machine's busy share from /proc/stat over the same window
 * (so the number can be read NEXT TO what else ran: a browser, a build). One line of key=value output:
 *   COST pid=.. secs=.. cpu_pct=..  child_cpu_pct=..  wakeups_per_s=..  involuntary_per_s=..  threads=..  children_max=..  children_seen=..  machine_busy_pct=..  load1=..
 * cpu_pct is the share of ONE core (100 = a whole core). wakeups_per_s = voluntary context switches per second (each is a sleep ended by a timer, an fd or a signal). children_seen counts the distinct child pids
 * observed in any sample (a fork storm shorter than sample_ms can still be missed: child_cpu_pct catches the ones that were waited for). Exit 0; 2 usage; 3 the pid is gone.
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/proc_cost_op.+x proc_cost_op.c */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>

static int stat_times(long pid, long long *own, long long *kids) {
    char p[64], b[2048]; snprintf(p, sizeof p, "/proc/%ld/stat", pid); FILE *f = fopen(p, "r"); if (!f) return -1; size_t n = fread(b, 1, sizeof b - 1, f); fclose(f); b[n] = 0;
    char *c = strrchr(b, ')'); if (!c) return -1; long long f14 = 0, f15 = 0, f16 = 0, f17 = 0; int fld = 3;   /* after ") " the next field is number 3 */
    char *s = c + 2; for (char *tok = strtok(s, " "); tok; tok = strtok(NULL, " "), fld++) { long long v = atoll(tok); if (fld == 14) f14 = v; else if (fld == 15) f15 = v; else if (fld == 16) f16 = v; else if (fld == 17) { f17 = v; break; } }
    *own = f14 + f15; *kids = f16 + f17; return 0;
}
static int status_vals(long pid, long long *vol, long long *invol, long *threads) {
    char p[64], ln[256]; snprintf(p, sizeof p, "/proc/%ld/status", pid); FILE *f = fopen(p, "r"); if (!f) return -1; *vol = *invol = 0; *threads = 0;
    while (fgets(ln, sizeof ln, f)) { if (!strncmp(ln, "voluntary_ctxt_switches:", 24)) *vol = atoll(ln + 24); else if (!strncmp(ln, "nonvoluntary_ctxt_switches:", 27)) *invol = atoll(ln + 27); else if (!strncmp(ln, "Threads:", 8)) *threads = atol(ln + 8); }
    fclose(f); return 0;
}
static void machine(long long *busy, long long *total) {
    FILE *f = fopen("/proc/stat", "r"); *busy = *total = 0; if (!f) return; char ln[256]; if (fgets(ln, sizeof ln, f)) { long long v[8] = { 0 }; sscanf(ln, "cpu %lld %lld %lld %lld %lld %lld %lld %lld", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7]); for (int i = 0; i < 8; i++) *total += v[i]; *busy = *total - v[3] - v[4]; } fclose(f);
}
#define MAXSEEN 4096
static long seen[MAXSEEN]; static int nseen;
static int children_now(long pid) {   /* live children of pid; remembers every distinct child pid it sees */
    DIR *d = opendir("/proc"); if (!d) return 0; struct dirent *e; int n = 0;
    while ((e = readdir(d))) { if (e->d_name[0] < '0' || e->d_name[0] > '9') continue; long cp = atol(e->d_name); char p[64], b[1024]; snprintf(p, sizeof p, "/proc/%ld/stat", cp); FILE *f = fopen(p, "r"); if (!f) continue; size_t k = fread(b, 1, sizeof b - 1, f); fclose(f); b[k] = 0;
        char *c = strrchr(b, ')'); if (!c) continue; long ppid = 0; char st; if (sscanf(c + 2, "%c %ld", &st, &ppid) != 2) continue; if (ppid != pid) continue; n++;
        int known = 0; for (int i = 0; i < nseen; i++) if (seen[i] == cp) { known = 1; break; } if (!known && nseen < MAXSEEN) seen[nseen++] = cp; }
    closedir(d); return n;
}
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: proc_cost_op <pid> <seconds> [sample_ms]\n"); return 2; }
    long pid = atol(argv[1]); double secs = atof(argv[2]); long step = argc > 3 ? atol(argv[3]) : 100; if (pid < 2 || secs < 0.2 || secs > 3600 || step < 10 || step > 5000) { fprintf(stderr, "proc_cost_op: pid >= 2, seconds 0.2 to 3600, sample_ms 10 to 5000\n"); return 2; }
    long long o0, k0, o1, k1, v0, i0, v1, i1, b0, t0, b1, t1; long th0, th1; if (stat_times(pid, &o0, &k0) || status_vals(pid, &v0, &i0, &th0)) { fprintf(stderr, "proc_cost_op: pid %ld is gone\n", pid); return 3; }
    machine(&b0, &t0); struct timespec a, z; clock_gettime(CLOCK_MONOTONIC, &a); int cmax = children_now(pid);
    for (;;) { clock_gettime(CLOCK_MONOTONIC, &z); double el = (double)(z.tv_sec - a.tv_sec) + (double)(z.tv_nsec - a.tv_nsec) / 1e9; if (el >= secs) break; double left = secs - el; long ms = left * 1000 < step ? (long)(left * 1000) + 1 : step; struct timespec sl = { ms / 1000, (ms % 1000) * 1000000L }; nanosleep(&sl, NULL);
        int c = children_now(pid); if (c > cmax) cmax = c; }
    clock_gettime(CLOCK_MONOTONIC, &z); double el = (double)(z.tv_sec - a.tv_sec) + (double)(z.tv_nsec - a.tv_nsec) / 1e9;
    if (stat_times(pid, &o1, &k1) || status_vals(pid, &v1, &i1, &th1)) { fprintf(stderr, "proc_cost_op: pid %ld ended during the window\n", pid); return 3; } machine(&b1, &t1);
    double hz = (double)sysconf(_SC_CLK_TCK); double loadv = 0; FILE *lf = fopen("/proc/loadavg", "r"); if (lf) { if (fscanf(lf, "%lf", &loadv) != 1) loadv = 0; fclose(lf); }
    printf("COST pid=%ld secs=%.2f cpu_pct=%.2f child_cpu_pct=%.2f wakeups_per_s=%.1f involuntary_per_s=%.1f threads=%ld children_max=%d children_seen=%d machine_busy_pct=%.1f load1=%.2f\n", pid, el,
        100.0 * (double)(o1 - o0) / hz / el, 100.0 * (double)(k1 - k0) / hz / el, (double)(v1 - v0) / el, (double)(i1 - i0) / el, th1, cmax, nseen, t1 > t0 ? 100.0 * (double)(b1 - b0) / (double)(t1 - t0) : 0.0, loadv);
    return 0;
}
