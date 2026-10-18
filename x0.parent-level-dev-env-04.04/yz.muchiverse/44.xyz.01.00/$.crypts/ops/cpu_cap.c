/* cpu_cap <percent-of-one-core> <cmd...>: run cmd in its own process group and keep the WHOLE group (children included) at or under the cap.
 * Why it exists: kernel cgroup CPU quotas are not delegated to user scopes on this machine, and cpulimit 2.4 throttles only the top process
 * (measured 732% for a job whose 8 children burned CPU). Method: every slice (100 ms) read the group's utime+stime from /proc (processes whose
 * pgrp equals the child's pid), and SIGSTOP the whole group whenever its cumulative CPU is ahead of cap*elapsed, until the budget catches up, then SIGCONT. Only ever signals its own child group. A stopped group cannot starve the desktop; the child's exit status is passed through.
 * Build: gcc -O2 -Wall -o +x/cpu_cap.+x cpu_cap.c   (see button.sh build list / cpu-quota.sh) */
#define _GNU_SOURCE
#include <dirent.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static pid_t kid;
static void fwd(int s) { if (kid > 0) { kill(-kid, SIGCONT); kill(-kid, s); } }
static long long now_ns(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (long long)t.tv_sec * 1000000000LL + t.tv_nsec; }

/* total utime+stime ticks of every live process in group g (processes that exited add nothing more, so the sum is only used as a delta of live work) */
static long long group_ticks(pid_t g, int *n) {
    long long sum = 0; *n = 0;
    DIR *d = opendir("/proc"); if (!d) return 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] < '1' || e->d_name[0] > '9') continue;
        char p[300], buf[1024]; snprintf(p, sizeof p, "/proc/%s/stat", e->d_name);
        FILE *f = fopen(p, "r"); if (!f) continue;
        size_t k = fread(buf, 1, sizeof buf - 1, f); fclose(f); buf[k] = 0;
        char *r = strrchr(buf, ')'); if (!r) continue;
        int pgrp = 0; long long ut = 0, st = 0; char state;
        int ppid; int sess, tty, tpg; unsigned fl; unsigned long mf, cmf, mj, cmj;
        if (sscanf(r + 2, "%c %d %d %d %d %d %u %lu %lu %lu %lu %lld %lld", &state, &ppid, &pgrp, &sess, &tty, &tpg, &fl, &mf, &cmf, &mj, &cmj, &ut, &st) < 13) continue;
        if (pgrp == g) { sum += ut + st; (*n)++; }
    }
    closedir(d); return sum;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: cpu_cap <percent-of-one-core> <cmd...>\n"); return 2; }
    int cap = atoi(argv[1]); if (cap < 5) cap = 5;
    long hz = sysconf(_SC_CLK_TCK);
    kid = fork();
    if (kid < 0) { perror("fork"); return 125; }
    if (kid == 0) { setpgid(0, 0); execvp(argv[2], argv + 2); perror("exec"); _exit(127); }
    setpgid(kid, kid);
    signal(SIGINT, fwd); signal(SIGTERM, fwd); signal(SIGHUP, fwd);
    const long long slice = 100000000LL;          /* 100 ms: each scan walks /proc, so a shorter slice costs measurable CPU itself */
    long long total = 0, start = now_ns(); int status = 0;
    for (;;) {
        pid_t w = waitpid(kid, &status, WNOHANG);
        if (w == kid) break;
        int n;
        long long a = group_ticks(kid, &n);
        struct timespec ts = { 0, slice }; nanosleep(&ts, NULL);
        long long b = group_ticks(kid, &n);
        if (b > a) total += b - a;                                 /* a process that exited in between can lower the sum: ignore that slice's negative */
        /* cumulative budget: wall time at which the group has earned the CPU it has used so far, at the cap rate */
        double earned_at = (double)total / ((cap / 100.0) * hz);
        double elapsed = (now_ns() - start) / 1e9;
        if (earned_at > elapsed + 0.01) {
            double stop = earned_at - elapsed; if (stop > 1.0) stop = 1.0;
            kill(-kid, SIGSTOP);
            struct timespec st = { (time_t)stop, (long)((stop - (time_t)stop) * 1e9) }; nanosleep(&st, NULL);
            kill(-kid, SIGCONT);
        }
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + (WIFSIGNALED(status) ? WTERMSIG(status) : 0);
}
