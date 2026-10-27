/* cpu_guard.c - always-on CPU safety guard for the house (2026-10-26).
 *
 * WHY: the machine (Surface Laptop 3, 8 threads) has hard-frozen five times
 * (2026-10-15/16/17 and 2026-10-26 22:35). Every boot's journal just stops:
 * no OOM, no panic, and the firmware does not enforce thermal limits
 * (03-pitfalls/hq-cpu-safety.md section 7). The cause is not proven, but sustained
 * all-core load is the likeliest trigger, and the only handle we have is to act
 * BEFORE the machine locks up. cpu_watch_daemon.sh only logged single processes and
 * was never started by anything; this replaces it.
 *
 * WHAT (one sample every sample_ms, default 2 s, reading /proc only - no forks):
 *   WARN  busy >= warn_pct (or die temp >= temp_warn_c) for warn_samples in a row:
 *         incident report (top processes), log line, toast. Kills nothing.
 *   ACT   busy >= act_pct (or temp >= temp_act_c) for act_samples in a row:
 *         1. if the house accounts for >= house_min_pct of the machine, end every
 *            house process (script hosts first so nothing respawns, then TERM, then
 *            KILL), except the desktop shell (taskbar + its process groups);
 *         2. if the machine is STILL hot afterwards (or the house was not the
 *            culprit), ASK (zenity, default answer No, times out to No) whether to
 *            kill the biggest outside program (Chrome, Firefox, a script...).
 *            NEVER killed without a Yes.
 *   Every ACT writes state/cpu_guard_alert.txt = "the house needs a CPU-safety review".
 * EVIDENCE for the next freeze: state/cpu_guard_last.txt is rewritten every sample and
 * fdatasync'd every 3rd, so after a hard freeze it shows what the machine looked like in
 * its last seconds. A loop that wakes much later than asked is logged as STALL.
 *
 * Config (key=value, re-read every few samples): <house>/#.desktop/proc_mon_guard.pdl,
 * created with defaults on first run. dry_run=1 logs what would be killed and kills
 * nothing. Test hooks: CPU_GUARD_STATE (state dir), CPU_GUARD_ASK (program that exits
 * 0 for Yes, replaces zenity), CPU_GUARD_NOTIFY (program replacing notify-send).
 *
 * Usage: cpu_guard <house_root> [config_path]       (single instance: flock)
 * Build: gcc -O2 -Wall -Wextra -o ops/+x/cpu_guard.+x ops/cpu_guard.c  (guard.sh does it)
 */
#define _GNU_SOURCE
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAXP 8192

typedef struct {
    int pid, ppid, pgrp, uid;
    char comm[32];
    unsigned long long start;  /* with pid identifies a process (pid reuse) */
    unsigned long long ticks;  /* utime+stime+cutime+cstime: reaped children count (the world_manager lesson) */
    double pct;                /* percent of ONE core since the last sample */
    int cls;                   /* 0 = unknown yet, 1 = outside, 2 = house, 3 = protected */
} Proc;

static Proc cur[MAXP], prv[MAXP];
static int ncur, nprv;

typedef struct {
    int enabled, sample_ms, warn_pct, act_pct, warn_samples, act_samples;
    int temp_warn_c, temp_act_c, house_min_pct, ask_min_pct;
    int cooldown_s, ask_timeout_s, ask_cooldown_s, kill_house, dry_run;
} Cfg;
static Cfg cfg;

static char house[PATH_MAX], cfg_path[PATH_MAX], state_dir[PATH_MAX];
static char log_path[PATH_MAX], last_path[PATH_MAX], alert_path[PATH_MAX], inc_dir[PATH_MAX];
static long hz;
static int ncpu;
static pid_t self_pid;
static int temp_fd = -1;

static double mono(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}

static void stamp(char *buf, size_t n, const char *fmt) {
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(buf, n, fmt, &tm);
}

static void logf_(const char *fmt, ...) {
    char ts[32], line[1024];
    stamp(ts, sizeof ts, "%Y-%m-%d %H:%M:%S");
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    struct stat st;
    if (stat(log_path, &st) == 0 && st.st_size > (1 << 20)) {  /* keep one old generation */
        char old[PATH_MAX + 8];
        snprintf(old, sizeof old, "%s.1", log_path);
        rename(log_path, old);
    }
    FILE *f = fopen(log_path, "a");
    if (!f) return;
    fprintf(f, "%s | %s\n", ts, line);
    fflush(f);
    fsync(fileno(f));  /* a log line that dies in the page cache is useless after a freeze */
    fclose(f);
}

static void cfg_defaults(void) {
    cfg = (Cfg){1, 2000, 70, 85, 3, 5, 85, 92, 20, 15, 120, 90, 600, 1, 0};
}

static void cfg_load(void) {
    FILE *f = fopen(cfg_path, "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof line, f)) {
        char *eq = strchr(line, '=');
        if (line[0] == '#' || !eq) continue;
        *eq = 0;
        int v = atoi(eq + 1);
        if (!strcmp(line, "enabled")) cfg.enabled = v;
        else if (!strcmp(line, "sample_ms")) cfg.sample_ms = v < 500 ? 500 : v;  /* never poll faster than 2 Hz */
        else if (!strcmp(line, "warn_pct")) cfg.warn_pct = v;
        else if (!strcmp(line, "act_pct")) cfg.act_pct = v;
        else if (!strcmp(line, "warn_samples")) cfg.warn_samples = v < 1 ? 1 : v;
        else if (!strcmp(line, "act_samples")) cfg.act_samples = v < 1 ? 1 : v;
        else if (!strcmp(line, "temp_warn_c")) cfg.temp_warn_c = v;
        else if (!strcmp(line, "temp_act_c")) cfg.temp_act_c = v;
        else if (!strcmp(line, "house_min_pct")) cfg.house_min_pct = v;
        else if (!strcmp(line, "ask_min_pct")) cfg.ask_min_pct = v;
        else if (!strcmp(line, "cooldown_s")) cfg.cooldown_s = v;
        else if (!strcmp(line, "ask_timeout_s")) cfg.ask_timeout_s = v < 10 ? 10 : v;
        else if (!strcmp(line, "ask_cooldown_s")) cfg.ask_cooldown_s = v;
        else if (!strcmp(line, "kill_house")) cfg.kill_house = v;
        else if (!strcmp(line, "dry_run")) cfg.dry_run = v;
    }
    fclose(f);
}

static void cfg_seed(void) {
    if (access(cfg_path, F_OK) == 0) return;
    FILE *f = fopen(cfg_path, "w");
    if (!f) return;
    fprintf(f,
        "# proc-mon CPU guard (ops/cpu_guard.c). Percentages are of the WHOLE machine (all threads).\n"
        "# Re-read while running. Docs: 03-pitfalls/hq-cpu-safety.md section 8.\n"
        "enabled=%d\nsample_ms=%d\n"
        "# WARN: report + toast, kills nothing\nwarn_pct=%d\nwarn_samples=%d\ntemp_warn_c=%d\n"
        "# ACT: end the house's own processes, then ASK about the biggest outside program\n"
        "act_pct=%d\nact_samples=%d\ntemp_act_c=%d\n"
        "# the house is only blamed (and ended) when it uses at least this share of the machine\n"
        "house_min_pct=%d\n"
        "# an outside program is only offered for killing when it uses at least this share\n"
        "ask_min_pct=%d\n"
        "cooldown_s=%d\nask_timeout_s=%d\nask_cooldown_s=%d\n"
        "kill_house=%d\n# 1 = log what would be killed, kill nothing\ndry_run=%d\n",
        cfg.enabled, cfg.sample_ms, cfg.warn_pct, cfg.warn_samples, cfg.temp_warn_c, cfg.act_pct,
        cfg.act_samples, cfg.temp_act_c, cfg.house_min_pct, cfg.ask_min_pct, cfg.cooldown_s,
        cfg.ask_timeout_s, cfg.ask_cooldown_s, cfg.kill_house, cfg.dry_run);
    fclose(f);
}

/* ---- sensors ---- */

static int read_cpu_busy(unsigned long long *busy, unsigned long long *total) {
    char buf[512];
    int fd = open("/proc/stat", O_RDONLY | O_CLOEXEC);
    if (fd < 0) return -1;
    ssize_t n = read(fd, buf, sizeof buf - 1);
    close(fd);
    if (n <= 0) return -1;
    buf[n] = 0;
    unsigned long long v[8] = {0};
    if (sscanf(buf, "cpu %llu %llu %llu %llu %llu %llu %llu %llu", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7]) < 4)
        return -1;
    unsigned long long idle = v[3] + v[4];  /* idle + iowait: waiting on disk is not CPU load */
    unsigned long long all = 0;
    for (int i = 0; i < 8; i++) all += v[i];
    *busy = all - idle;
    *total = all;
    return 0;
}

static void temp_open(void) {
    char p[96], name[32];
    for (int i = 0; i < 16 && temp_fd < 0; i++) {
        snprintf(p, sizeof p, "/sys/class/hwmon/hwmon%d/name", i);
        int fd = open(p, O_RDONLY | O_CLOEXEC);
        if (fd < 0) continue;
        ssize_t n = read(fd, name, sizeof name - 1);
        close(fd);
        if (n <= 0) continue;
        name[n] = 0;
        if (strncmp(name, "k10temp", 7) && strncmp(name, "coretemp", 8)) continue;
        snprintf(p, sizeof p, "/sys/class/hwmon/hwmon%d/temp1_input", i);
        temp_fd = open(p, O_RDONLY | O_CLOEXEC);
    }
}

static double read_temp(void) {
    char b[24];
    if (temp_fd < 0) return -1;
    ssize_t n = pread(temp_fd, b, sizeof b - 1, 0);
    if (n <= 0) return -1;
    b[n] = 0;
    return atoi(b) / 1000.0;
}

/* ---- process table ---- */

static int read_file(const char *path, char *buf, size_t cap) {
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return -1;
    ssize_t n = read(fd, buf, cap - 1);
    close(fd);
    if (n < 0) return -1;
    buf[n] = 0;
    return (int)n;
}

static void scan_procs(void) {
    memcpy(prv, cur, sizeof(Proc) * ncur);
    nprv = ncur;
    ncur = 0;
    DIR *d = opendir("/proc");
    if (!d) return;
    struct dirent *e;
    char path[64], buf[1024];
    while ((e = readdir(d)) && ncur < MAXP) {
        if (e->d_name[0] < '1' || e->d_name[0] > '9') continue;
        snprintf(path, sizeof path, "/proc/%s/stat", e->d_name);
        int fd = open(path, O_RDONLY | O_CLOEXEC);
        if (fd < 0) continue;
        struct stat st;
        fstat(fd, &st);
        ssize_t n = read(fd, buf, sizeof buf - 1);
        close(fd);
        if (n <= 0) continue;
        buf[n] = 0;
        char *lp = strchr(buf, '('), *rp = strrchr(buf, ')');
        if (!lp || !rp || rp < lp) continue;
        Proc *p = &cur[ncur];
        memset(p, 0, sizeof *p);
        p->pid = atoi(e->d_name);
        p->uid = (int)st.st_uid;
        size_t cl = (size_t)(rp - lp - 1);
        if (cl >= sizeof p->comm) cl = sizeof p->comm - 1;
        memcpy(p->comm, lp + 1, cl);
        long long ut, stt, cut, cst;
        unsigned long long start;
        char state;
        if (sscanf(rp + 2, "%c %d %d %*d %*d %*d %*u %*u %*u %*u %*u %lld %lld %lld %lld %*d %*d %*d %*d %llu", &state, &p->ppid,
                   &p->pgrp, &ut, &stt, &cut, &cst, &start) < 8)
            continue;
        if (state == 'Z') continue;
        p->start = start;
        p->ticks = (unsigned long long)(ut + stt + (cut > 0 ? cut : 0) + (cst > 0 ? cst : 0));
        ncur++;
    }
    closedir(d);
}

static void compute_deltas(double dt) {
    for (int i = 0; i < ncur; i++) {
        Proc *p = &cur[i];
        for (int j = 0; j < nprv; j++) {
            Proc *q = &prv[j];
            if (q->pid == p->pid && q->start == p->start) {
                p->pct = (p->ticks >= q->ticks ? (double)(p->ticks - q->ticks) : 0) * 100.0 / ((double)hz * dt);
                p->cls = q->cls;  /* classification is cached for the life of the process */
                break;
            }
        }
    }
}

/* ---- classification: house / protected / outside ---- */

static int under_house(const char *path) {
    size_t hl = strlen(house);
    return strncmp(path, house, hl) == 0 && (path[hl] == '/' || path[hl] == 0);
}

static const char *protected_pats[] = {"khtpm_taskbar_manager_main", "khtpm_strip_parser", "run_khtpm_strip.sh",
                                       "khtpm_strip_header.xhtpm", "khtpm_strip_bottom.xhtpm", "livedesk_splash", "cpu_guard", NULL};
static const char *system_comms[] = {"systemd", "(sd-pam)", "gnome-shell", "gnome-session-b", "Xorg", "Xwayland", "gdm", "gdm3", "dbus-daemon",
                                     "pipewire", "pipewire-pulse", "wireplumber", "pulseaudio", "claude", "sshd", "login", "NetworkManager", "bash-login", NULL};

static int cmdline_of(int pid, char *out, size_t cap) { /* argv[0] and argv[1] only: "bash -c <text mentioning X>" must not look like X */
    char path[48], raw[1024];
    snprintf(path, sizeof path, "/proc/%d/cmdline", pid);
    int n = read_file(path, raw, sizeof raw);
    out[0] = 0;
    if (n <= 0) return 0;
    size_t l0 = strlen(raw);
    snprintf(out, cap, "%s", raw);
    if ((int)l0 + 1 < n) snprintf(out + l0, cap - l0, " %s", raw + l0 + 1);
    return 1;
}

/* sh/bash/dash running a house .sh script (NOT "sh -c ...": an agent's tool shell is "-c") */
static int is_house_script_host(int pid, const char *comm) {
    if (strcmp(comm, "sh") && strcmp(comm, "bash") && strcmp(comm, "dash")) return 0;
    char path[48], raw[4096], cwd[PATH_MAX];
    snprintf(path, sizeof path, "/proc/%d/cmdline", pid);
    int n = read_file(path, raw, sizeof raw);
    if (n <= 0) return 0;
    const char *arg = NULL;
    for (int off = (int)strlen(raw) + 1; off < n; off += (int)strlen(raw + off) + 1) {
        if (!strcmp(raw + off, "-c")) return 0;
        if (raw[off] == '-') continue;
        arg = raw + off;
        break;
    }
    if (!arg) return 0;
    if (arg[0] == '/') return under_house(arg);
    snprintf(path, sizeof path, "/proc/%d/cwd", pid);
    ssize_t l = readlink(path, cwd, sizeof cwd - 1);
    if (l <= 0) return 0;
    cwd[l] = 0;
    return under_house(cwd);
}

static void classify_all(void) {
    /* protected process groups: the desktop shell and everything it spawned in its group */
    static int shell_pgrp[64];
    int nsp = 0;
    char cl[2048];
    for (int i = 0; i < ncur; i++) {
        Proc *p = &cur[i];
        if (p->cls) continue;
        cmdline_of(p->pid, cl, sizeof cl);
        int prot = p->pid == self_pid || p->pid <= 2;
        for (int k = 0; protected_pats[k] && !prot; k++) if (strstr(cl, protected_pats[k]) || strstr(p->comm, protected_pats[k])) prot = 1;
        if (prot) { p->cls = 3; if (nsp < 64 && p->pid != self_pid && p->pgrp > 1) shell_pgrp[nsp++] = p->pgrp; }
    }
    for (int i = 0; i < ncur; i++) {
        Proc *p = &cur[i];
        if (p->cls) continue;
        int prot = 0;
        for (int k = 0; k < nsp; k++) if (p->pgrp == shell_pgrp[k]) prot = 1;
        for (int k = 0; system_comms[k] && !prot; k++) if (!strcmp(p->comm, system_comms[k])) prot = 1;
        if (prot) { p->cls = 3; continue; }
        char path[48], exe[PATH_MAX];
        snprintf(path, sizeof path, "/proc/%d/exe", p->pid);
        ssize_t l = readlink(path, exe, sizeof exe - 1);
        if (l > 0) {
            exe[l] = 0;
            char *del = strstr(exe, " (deleted)");  /* session binary copies are deleted while running */
            if (del) *del = 0;
            if (under_house(exe)) { p->cls = 2; continue; }
        }
        p->cls = is_house_script_host(p->pid, p->comm) ? 2 : 1;
    }
}

static int is_ancestor_protected(int pid) { /* never end the guard's own ancestors */
    for (int i = 0; i < ncur; i++) if (cur[i].pid == self_pid) {
        int q = cur[i].ppid, guard = 0;
        while (q > 1 && guard++ < 64) {
            if (q == pid) return 1;
            int nx = 0;
            for (int j = 0; j < ncur; j++) if (cur[j].pid == q) { nx = cur[j].ppid; break; }
            q = nx;
        }
    }
    return 0;
}

/* ---- reporting ---- */

static int cmp_pct(const void *a, const void *b) {
    double x = cur[*(const int *)a].pct, y = cur[*(const int *)b].pct;
    return x < y ? 1 : x > y ? -1 : 0;
}

typedef struct { char name[32]; double pct; int n; } Group;

static int outside_groups(Group *g, int cap) {  /* same-comm outside processes summed (Chrome = dozens of pids) */
    int ng = 0;
    for (int i = 0; i < ncur; i++) {
        Proc *p = &cur[i];
        if (p->cls != 1 || p->uid != (int)getuid() || p->pct <= 0) continue;
        int k;
        for (k = 0; k < ng; k++) if (!strcmp(g[k].name, p->comm)) break;
        if (k == ng) { if (ng == cap) continue; snprintf(g[ng].name, sizeof g[ng].name, "%s", p->comm); g[ng].pct = 0; g[ng].n = 0; ng++; }
        g[k].pct += p->pct;
        g[k].n++;
    }
    for (int a = 0; a < ng; a++) for (int b = a + 1; b < ng; b++) if (g[b].pct > g[a].pct) { Group t = g[a]; g[a] = g[b]; g[b] = t; }
    return ng;
}

static double house_core_pct(void) {
    double s = 0;
    for (int i = 0; i < ncur; i++) if (cur[i].cls == 2) s += cur[i].pct;
    return s;
}

static const char *friendly(const char *comm) {
    if (!strcmp(comm, "chrome")) return "Google Chrome";
    if (!strcmp(comm, "firefox") || !strcmp(comm, "firefox-bin")) return "Firefox";
    return comm;
}

static void write_incident(const char *level, double busy, double temp, double base, char *summary, size_t scap) {
    static int idx[MAXP];
    for (int i = 0; i < ncur; i++) idx[i] = i;
    qsort(idx, ncur, sizeof idx[0], cmp_pct);
    char ts[32], path[PATH_MAX + 64];
    stamp(ts, sizeof ts, "%Y%m%d-%H%M%S");
    snprintf(path, sizeof path, "%s/%s-%s.txt", inc_dir, ts, level);
    FILE *f = fopen(path, "w");
    Group g[48];
    int ng = outside_groups(g, 48);
    double hp = house_core_pct();
    if (f) {
        fprintf(f, "%s  machine busy %.0f%% (normal ~%.0f%%)  die temp %.0f C  house uses %.0f%% of one core = %.0f%% of the machine\n\n",
                level, busy, base, temp, hp, hp / ncpu);
        fprintf(f, "TOP PROCESSES (percent of ONE core; inclusive of reaped children)\n  pid      cpu%%   class    comm / exe\n");
        for (int r = 0; r < 14 && r < ncur && cur[idx[r]].pct > 0.5; r++) {
            Proc *p = &cur[idx[r]];
            char ep[48], exe[PATH_MAX] = "";
            snprintf(ep, sizeof ep, "/proc/%d/exe", p->pid);
            ssize_t l = readlink(ep, exe, sizeof exe - 1);
            exe[l > 0 ? l : 0] = 0;
            const char *rel = under_house(exe) ? exe + strlen(house) + 1 : exe;
            fprintf(f, "  %-8d %-6.0f %-8s %s  %s\n", p->pid, p->pct, p->cls == 2 ? "HOUSE" : p->cls == 3 ? "protect" : "outside", p->comm, rel);
        }
        fprintf(f, "\nOUTSIDE PROGRAMS BY NAME (all pids summed)\n");
        for (int k = 0; k < ng && k < 6; k++) fprintf(f, "  %-24s %5.0f%% of one core, %d processes\n", g[k].name, g[k].pct, g[k].n);
        fclose(f);
    }
    int off = snprintf(summary, scap, "busy %.0f%% (normal ~%.0f%%), %.0f C. house %.0f%% of machine.", busy, base, temp, hp / ncpu);
    for (int k = 0; k < ng && k < 3 && off < (int)scap - 40; k++)
        off += snprintf(summary + off, scap - off, " %s %.0f%%;", friendly(g[k].name), g[k].pct);
    /* keep the newest 40 reports */
    DIR *d = opendir(inc_dir);
    if (d) {
        static char names[400][40];
        int nn = 0;
        struct dirent *e;
        while ((e = readdir(d)) && nn < 400) if (e->d_name[0] != '.') snprintf(names[nn++], 40, "%s", e->d_name);
        closedir(d);
        for (int a = 0; a < nn; a++) for (int b = a + 1; b < nn; b++) if (strcmp(names[b], names[a]) < 0) { char t[40]; memcpy(t, names[a], 40); memcpy(names[a], names[b], 40); memcpy(names[b], t, 40); }
        for (int a = 0; a < nn - 40; a++) { char rp[PATH_MAX + 64]; snprintf(rp, sizeof rp, "%s/%s", inc_dir, names[a]); unlink(rp); }
    }
}

/* ---- children: toast and ask ---- */

static pid_t ask_pid = -1;
static char ask_group[32];
static double ask_deadline;

static pid_t spawn(char *const argv[]) {
    pid_t c = fork();
    if (c == 0) {
        setsid();
        int dn = open("/dev/null", O_RDWR);
        if (dn >= 0) { dup2(dn, 0); dup2(dn, 1); dup2(dn, 2); }
        execvp(argv[0], argv);
        _exit(127);
    }
    return c;
}

static void toast(const char *urgency, const char *title, const char *body) {
    const char *prog = getenv("CPU_GUARD_NOTIFY");
    char *argv[] = {(char *)(prog ? prog : "notify-send"), "-u", (char *)urgency, "-a", "LyfeDesk CPU guard", (char *)title, (char *)body, NULL};
    spawn(argv);
}

static void ask_kill(const Group *g, double busy) {
    char text[600], tmo[16];
    snprintf(text, sizeof text,
             "The machine has been at %.0f%% CPU and may freeze.\n\n%s is the biggest program outside LyfeDesk: %.0f%% of one core across %d processes.\n\n"
             "Kill it now? (No / no answer = nothing is killed.)",
             busy, friendly(g->name), g->pct, g->n);
    snprintf(tmo, sizeof tmo, "--timeout=%d", cfg.ask_timeout_s);
    const char *prog = getenv("CPU_GUARD_ASK");
    char title[80];
    snprintf(title, sizeof title, "--title=LyfeDesk CPU guard");
    char okl[64], cal[32] = "--cancel-label=Leave it";
    snprintf(okl, sizeof okl, "--ok-label=Kill %s", friendly(g->name));
    char *argv_z[] = {"zenity", "--question", "--icon-name=dialog-warning", title, okl, cal, tmo, "--no-wrap", NULL, NULL};
    char textarg[640];
    snprintf(textarg, sizeof textarg, "--text=%s", text);
    argv_z[8] = textarg;
    char *argv_t[] = {(char *)prog, (char *)g->name, NULL};
    ask_pid = spawn(prog ? argv_t : argv_z);
    snprintf(ask_group, sizeof ask_group, "%s", g->name);
    ask_deadline = mono() + cfg.ask_timeout_s + 5;
    logf_("ASK | %s (%.0f%% of one core, %d procs) - waiting for an answer, default is No", g->name, g->pct, g->n);
}

/* ---- killing ---- */

static int still_same(int pid, unsigned long long start) {
    char path[48], buf[1024];
    snprintf(path, sizeof path, "/proc/%d/stat", pid);
    if (read_file(path, buf, sizeof buf) <= 0) return 0;
    char *rp = strrchr(buf, ')');
    unsigned long long s;
    return rp && sscanf(rp + 2, "%*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %*u %*u %*d %*d %*d %*d %*d %*d %llu", &s) == 1 && s == start;
}

typedef struct { int pid; unsigned long long start; } Victim;
static int blocked;  /* the guard itself slept inside kill_set: the next interval is not a machine stall */

static int kill_set(int cls, const char *only_comm, int scripts_first, const char *why) {
    static Victim v[MAXP];
    int nv = 0;
    for (int pass = 0; pass < (scripts_first ? 2 : 1); pass++) {
        for (int i = 0; i < ncur; i++) {
            Proc *p = &cur[i];
            if (p->cls != cls || p->pid == self_pid || is_ancestor_protected(p->pid)) continue;
            if (only_comm && (strcmp(p->comm, only_comm) || p->uid != (int)getuid())) continue;
            int host = !strcmp(p->comm, "sh") || !strcmp(p->comm, "bash") || !strcmp(p->comm, "dash");
            if (scripts_first && ((pass == 0) != host)) continue;
            if (!scripts_first && pass) continue;
            logf_("%s | %s pid=%d %.0f%% %s", cfg.dry_run ? "WOULD-KILL" : "KILL", why, p->pid, p->pct, p->comm);
            if (cfg.dry_run) continue;
            v[nv].pid = p->pid; v[nv].start = p->start; nv++;
            kill(p->pid, SIGTERM);
        }
        if (scripts_first && pass == 0 && nv && !cfg.dry_run) usleep(300000);  /* hosts first: nothing may respawn what we end next */
    }
    if (!nv) return 0;
    usleep(1500000);
    blocked = 1;
    int hard = 0;
    for (int i = 0; i < nv; i++) if (still_same(v[i].pid, v[i].start)) { kill(v[i].pid, SIGKILL); hard++; }
    logf_("KILLED | %s: %d ended, %d needed SIGKILL", why, nv, hard);
    return nv;
}

/* ---- main loop ---- */

int main(int argc, char **argv) {
    if (argc < 2 || !realpath(argv[1], house)) { fprintf(stderr, "usage: cpu_guard <house_root> [config]\n"); return 2; }
    self_pid = getpid();
    hz = sysconf(_SC_CLK_TCK);
    ncpu = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (ncpu < 1) ncpu = 1;
    snprintf(cfg_path, sizeof cfg_path, "%s", argc > 2 ? argv[2] : "");
    if (!cfg_path[0]) snprintf(cfg_path, sizeof cfg_path, "%s/#.desktop/proc_mon_guard.pdl", house);
    const char *sd = getenv("CPU_GUARD_STATE");
    snprintf(state_dir, sizeof state_dir, "%s", sd ? sd : "");
    if (!state_dir[0]) snprintf(state_dir, sizeof state_dir, "%s/&.hq-apps/proc-mon/state", house);
    snprintf(inc_dir, sizeof inc_dir, "%s/incidents", state_dir);
    snprintf(log_path, sizeof log_path, "%s/cpu_guard.log", state_dir);
    snprintf(last_path, sizeof last_path, "%s/cpu_guard_last.txt", state_dir);
    snprintf(alert_path, sizeof alert_path, "%s/cpu_guard_alert.txt", state_dir);
    mkdir(state_dir, 0755);
    mkdir(inc_dir, 0755);

    char lockp[PATH_MAX + 32];
    snprintf(lockp, sizeof lockp, "%s/.cpu_guard.lock", state_dir);
    int lk = open(lockp, O_RDWR | O_CREAT | O_CLOEXEC, 0644);
    if (lk < 0 || flock(lk, LOCK_EX | LOCK_NB) != 0) return 0;  /* already running */

    cfg_defaults();
    cfg_seed();
    cfg_load();
    temp_open();
    signal(SIGPIPE, SIG_IGN);
    logf_("START | pid=%d house=%s threads=%d temp=%s warn=%d%% act=%d%% dry_run=%d", self_pid, house, ncpu, temp_fd >= 0 ? "k10temp" : "none",
          cfg.warn_pct, cfg.act_pct, cfg.dry_run);

    unsigned long long pb = 0, pt = 0;
    read_cpu_busy(&pb, &pt);
    scan_procs();
    double t_prev = mono(), t_next = t_prev, baseline = -1;
    int warn_n = 0, act_n = 0, cool_n = 0, tick = 0, in_episode = 0, need_ask = 0;
    double cooldown_until = 0, ask_cooldown_until = 0, recheck_at = 0;
    int last_fd = open(last_path, O_WRONLY | O_CREAT | O_CLOEXEC, 0644);
    char summary[700];

    for (;;) {
        t_next += cfg.sample_ms / 1000.0;
        double now = mono();
        if (t_next > now) {
            struct timespec ts = {(time_t)(t_next - now), (long)(((t_next - now) - (long)(t_next - now)) * 1e9)};
            nanosleep(&ts, NULL);
        } else t_next = now;
        now = mono();
        double dt = now - t_prev;
        t_prev = now;
        if (blocked) blocked = 0;
        else if (dt > 3.0 * cfg.sample_ms / 1000.0)
            logf_("STALL | the guard slept %.1fs (asked for %.1fs): the machine was nearly frozen", dt, cfg.sample_ms / 1000.0);
        if (++tick % 5 == 0) cfg_load();
        if (!cfg.enabled) { read_cpu_busy(&pb, &pt); continue; }

        unsigned long long b, t;
        if (read_cpu_busy(&b, &t) || t == pt) continue;
        double busy = (double)(b - pb) * 100.0 / (double)(t - pt);
        pb = b; pt = t;
        double temp = read_temp();
        scan_procs();
        compute_deltas(dt);
        baseline = baseline < 0 ? busy : baseline + (busy - baseline) / 300.0;  /* ~10 minute memory = "normal" */

        /* reap toasts and the ask */
        int st;
        pid_t w;
        while ((w = waitpid(-1, &st, WNOHANG)) > 0) {
            if (w != ask_pid) continue;
            int code = WIFEXITED(st) ? WEXITSTATUS(st) : -1;
            ask_pid = -1;
            if (code == 0) {
                classify_all();
                logf_("ANSWER | Yes - ending %s", ask_group);
                int n = kill_set(1, ask_group, 0, "outside program, owner said Yes");
                char msg[160];
                snprintf(msg, sizeof msg, "Ended %d %s processes at your request.", n, friendly(ask_group));
                toast("normal", "LyfeDesk CPU guard", msg);
            } else logf_("ANSWER | %s (exit %d) - nothing killed", code == 5 ? "timed out" : "No", code);
        }
        if (ask_pid > 0 && now > ask_deadline) { kill(ask_pid, SIGKILL); logf_("ANSWER | dialog never answered - nothing killed"); }

        int warm = busy >= cfg.warn_pct || (temp > 0 && temp >= cfg.temp_warn_c);
        int hot = busy >= cfg.act_pct || (temp > 0 && temp >= cfg.temp_act_c);
        warn_n = warm ? warn_n + 1 : 0;
        act_n = hot ? act_n + 1 : 0;
        cool_n = busy < cfg.warn_pct - 15 && !(temp > 0 && temp >= cfg.temp_warn_c - 5) ? cool_n + 1 : 0;  /* calm needs the die cool too */

        if (last_fd >= 0) {  /* last-seconds evidence for a hard freeze */
            char rec[200];
            int ti = 0;
            for (int i = 1; i < ncur; i++) if (cur[i].pct > cur[ti].pct) ti = i;
            int n = snprintf(rec, sizeof rec, "%ld busy=%.0f temp=%.0f warn_n=%d act_n=%d top=%s:%.0f\n", (long)time(NULL), busy, temp, warn_n, act_n,
                             ncur ? cur[ti].comm : "-", ncur ? cur[ti].pct : 0.0);
            ssize_t ign = pwrite(last_fd, rec, n, 0);
            (void)ign;
            if (ftruncate(last_fd, n)) {}
            if (tick % 3 == 0) fdatasync(last_fd);
        }

        if (warn_n >= cfg.warn_samples && !in_episode) {
            in_episode = 1;
            classify_all();
            write_incident("WARN", busy, temp, baseline, summary, sizeof summary);
            logf_("WARN | %s", summary);
            toast("normal", "LyfeDesk: CPU is high", summary);
        }
        if (cool_n >= 5 && in_episode) { in_episode = 0; need_ask = 0; logf_("CALM | back to %.0f%%", busy); }

        if (act_n >= cfg.act_samples && now >= cooldown_until) {
            classify_all();
            double hshare = house_core_pct() / ncpu;
            write_incident("ACT", busy, temp, baseline, summary, sizeof summary);
            logf_("ACT | %s", summary);
            cooldown_until = now + cfg.cooldown_s;
            int ended = 0;
            if (cfg.kill_house && hshare >= cfg.house_min_pct) {
                ended = kill_set(2, NULL, 1, "house process");
                recheck_at = now + 3.0 * cfg.sample_ms / 1000.0 + 2.0;  /* let the load drain before judging */
            } else {
                logf_("ACT | house is only %.0f%% of the machine (< %d%%): not blaming it", hshare, cfg.house_min_pct);
                recheck_at = now;
            }
            need_ask = 1;
            FILE *af = fopen(alert_path, "w");
            if (af) {
                char ts[32];
                stamp(ts, sizeof ts, "%Y-%m-%d %H:%M:%S");
                fprintf(af, "%s CPU GUARD ACTED. %s\nended %d house processes%s.\nThe house needs a CPU-safety review: read the newest file in incidents/ and 03-pitfalls/hq-cpu-safety.md section 8. Delete this file when reviewed.\n",
                        ts, summary, ended, cfg.dry_run ? " (dry run: none really)" : "");
                fclose(af);
            }
            char body[900];
            snprintf(body, sizeof body, "%s\nLyfeDesk processes ended: %d. See proc-mon/state/incidents.", summary, ended);
            toast("critical", "LyfeDesk CPU guard acted", body);
            act_n = 0;
        }

        if (need_ask && now >= recheck_at && ask_pid < 0) {
            need_ask = 0;
            if (busy >= cfg.act_pct - 10 || (temp > 0 && temp >= cfg.temp_act_c) ) {
                classify_all();
                Group g[48];
                int ng = outside_groups(g, 48);
                if (ng && g[0].pct / ncpu >= cfg.ask_min_pct && now >= ask_cooldown_until) {
                    ask_cooldown_until = now + cfg.ask_cooldown_s;
                    ask_kill(&g[0], busy);
                } else logf_("STILL-HOT | %.0f%% but no outside program is a clear culprit (top: %s %.0f%%) or asked recently", busy, ng ? g[0].name : "none", ng ? g[0].pct : 0.0);
            } else logf_("RECOVERED | load drained to %.0f%% after the house was ended; not asking about outside programs", busy);
        }
    }
}
