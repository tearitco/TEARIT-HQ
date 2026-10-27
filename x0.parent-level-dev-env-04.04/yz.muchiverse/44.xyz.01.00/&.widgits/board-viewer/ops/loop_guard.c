/* loop_guard - lets a board's pal loop END when its engine is gone (owner 2026-10-26: "have loops exit as well").
 *
 * Why: the engine stack of a board (bv_render_3d --daemon + two prisc loops + a clock daemon) is launched detached. When the window or the engine dies
 * without the launcher's exit trap running (a crash, a force-kill, a reboot of one process), the prisc loops kept going for hours, starting about 9 processes
 * a second between them for nothing (measured 2026-10-26: two loops, 3 hours, load average 18). The pal VM has no "is that pid alive" op, so the loop execs
 * this guard once every ~480 passes (~8 s, 0.12 spawns/s) and the guard decides.
 *
 *   loop_guard [<pidfile> <grace_s> [stop_marker]]      (no arguments: pieces/display/.gpu_render.pid, 120 s, "@real_root" - what the pal loop uses)
 *     pidfile      the engine daemon's pid file (bv_render_3d --daemon writes pieces/display/.gpu_render.pid in the session, removes it on a clean exit)
 *     grace_s      the loop's first <grace_s> seconds are never judged (the engine starts after the loop)
 *     stop_marker  optional ("@real_root" = <pieces/system/real_project_root.txt's path>/pieces/system/stack_stop.txt): a file that gets ONE appended byte before the loop is ended; the sibling piececraft loop compares its SIZE with the size it saw
 *                  at boot (marker standard, never mtime) and ends too. Not created when the engine is alive.
 *   Only an engine that was SEEN ALIVE once is ever judged: the first time the pid names a live process the guard drops <dir of pidfile>/.engine_seen. A session
 *   that never started the 3D daemon (2D mode, the standalone widget) has no pid file and no .engine_seen, and its loop is left alone.
 *   The engine is judged gone when the pid file is missing or names a dead pid (after it was seen). Judged twice, 3 s apart, so a keep-alive respawn (new pid in the file)
 *   is not mistaken for a death. Then: append the marker byte, SIGTERM the parent (the prisc VM running this loop, because prisc's exec forks the op).
 *   Exit 0 always (prisc ignores the status); one line on stderr when it ends a loop. No shell, no other side effects. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>
#include <time.h>

static int engine_alive(const char *pidfile) {
    FILE *f = fopen(pidfile, "r"); if (!f) return 0;
    int pid = 0; if (fscanf(f, "%d", &pid) != 1) pid = 0; fclose(f);
    if (pid <= 1) return 0;
    if (kill((pid_t)pid, 0) == 0) return 1;
    return errno == EPERM;      /* exists, not ours to signal */
}

/* seconds since the process started (field 22 of /proc/<pid>/stat against /proc/uptime); -1 when unknown */
static double age_s(pid_t pid) {
    char p[64], buf[2048]; snprintf(p, sizeof p, "/proc/%d/stat", (int)pid);
    FILE *f = fopen(p, "r"); if (!f) return -1; size_t n = fread(buf, 1, sizeof buf - 1, f); fclose(f); buf[n] = 0;
    char *r = strrchr(buf, ')'); if (!r) return -1;
    unsigned long long start = 0; int field = 3; char *t = r + 2;      /* field 3 = state */
    for (char *tok = strtok(t, " "); tok; tok = strtok(NULL, " "), field++) if (field == 22) { start = strtoull(tok, NULL, 10); break; }
    f = fopen("/proc/uptime", "r"); if (!f) return -1; double up = 0; if (fscanf(f, "%lf", &up) != 1) up = 0; fclose(f);
    long hz = sysconf(_SC_CLK_TCK); if (hz <= 0) hz = 100;
    return up - (double)start / (double)hz;
}

/* Is a bv_render_3d engine process running with THIS session as its working directory? The pid file can be missing or stale while the engine lives (a second engine of the session exits and removes it, a relaunch rewrites it), so a missing file alone must never end a loop (owner 2026-10-26: camera keys died in a live board after the guard fired at 21:35 while frames kept coming until 21:42). */
static int engine_process_here(const char *here) {
    DIR *d = opendir("/proc"); if (!d) return 0; struct dirent *e; int found = 0;
    while (!found && (e = readdir(d))) { if (e->d_name[0] < '0' || e->d_name[0] > '9') continue;
        char p[96], comm[64] = "", cwd[4200]; snprintf(p, sizeof p, "/proc/%s/comm", e->d_name); FILE *f = fopen(p, "r"); if (!f) continue; if (fgets(comm, sizeof comm, f)) comm[strcspn(comm, "\n")] = 0; fclose(f);
        if (strncmp(comm, "bv_render_3d", 12)) continue;
        snprintf(p, sizeof p, "/proc/%s/cwd", e->d_name); ssize_t n = readlink(p, cwd, sizeof cwd - 1); if (n <= 0) continue; cwd[n] = 0;
        if (!strcmp(cwd, here)) { snprintf(p, sizeof p, "/proc/%s/stat", e->d_name); char sb[512] = ""; f = fopen(p, "r"); if (f) { size_t m = fread(sb, 1, sizeof sb - 1, f); sb[m] = 0; fclose(f); } char *r = strrchr(sb, ')'); if (r && r[2] != 'Z') found = 1; } }
    closedir(d); return found; }
static void guard_log(const char *fmt, const char *a, const char *b) {      /* one line per decision to pieces/display/.loop_guard.log (cap 40 lines: it is only for finding out why a loop ended) */
    FILE *f = fopen("pieces/display/.loop_guard.log", "a"); if (!f) return; fprintf(f, "%ld ", (long)time(NULL)); fprintf(f, fmt, a, b); fputc('\n', f); fclose(f); }
int main(int argc, char **argv) {
    /* No arguments = the session defaults. The board-viewer session copies the OLD prisc VM, whose exec delivers no arguments (prisc_exec_args.pal fails on it), so the loop
     * runs this bare; arguments exist only so a test can point it elsewhere. */
    const char *pidfile = argc > 1 ? argv[1] : "pieces/display/.gpu_render.pid"; double grace = argc > 2 ? atof(argv[2]) : (getenv("LOOP_GUARD_GRACE") ? atof(getenv("LOOP_GUARD_GRACE")) : 120.0); const char *marker = argc > 3 ? argv[3] : "@real_root";
    char mk[4096] = "";
    if (marker && !strcmp(marker, "@real_root")) {      /* the pal cannot read a path out of a file: resolve it here */
        FILE *rf = fopen("pieces/system/real_project_root.txt", "r");
        if (rf) { char root[3800] = ""; if (fgets(root, sizeof root, rf)) { root[strcspn(root, "\r\n")] = 0; if (root[0]) snprintf(mk, sizeof mk, "%s/pieces/system/stack_stop.txt", root); } fclose(rf); }
        marker = mk;
    }
    pid_t parent = getppid(); if (parent <= 1) return 0;      /* started by hand or reparented: never kill init */
    { char cp[64], cm[64] = ""; snprintf(cp, sizeof cp, "/proc/%d/comm", (int)parent); FILE *f = fopen(cp, "r"); if (f) { if (fgets(cm, sizeof cm, f)) cm[strcspn(cm, "\n")] = 0; fclose(f); }
      if (strcmp(cm, "prisc+x")) return 0; }      /* only a pal VM is ever ended: run by hand from a shell (or any other parent) the guard does nothing, it used to SIGTERM that shell */
    char seen[4200]; { snprintf(seen, sizeof seen, "%s", pidfile); char *sl = strrchr(seen, '/'); if (sl) { snprintf(sl + 1, sizeof seen - (size_t)(sl + 1 - seen), ".engine_seen"); } else snprintf(seen, sizeof seen, ".engine_seen"); }
    if (engine_alive(pidfile)) { if (access(seen, F_OK) != 0) { int fd = open(seen, O_WRONLY | O_CREAT, 0644); if (fd >= 0) close(fd); } return 0; }
    if (access(seen, F_OK) != 0) return 0;      /* never saw an engine here: not this guard's business */
    double age = age_s(parent); if (age >= 0 && age < grace) return 0;      /* still starting up */
    char here[4200] = ""; if (!getcwd(here, sizeof here)) here[0] = 0;
    sleep(3); if (engine_alive(pidfile)) return 0;      /* a respawn writes a new pid within this window */
    if (here[0] && engine_process_here(here)) { guard_log("pid file %s is missing or dead but an engine process runs here: loop kept (%s)", pidfile, "process scan"); return 0; }
    if (marker && marker[0]) { int fd = open(marker, O_WRONLY | O_APPEND | O_CREAT, 0644); if (fd >= 0) { if (write(fd, "x", 1) < 0) { /* best effort */ } close(fd); } }
    { char b[24]; snprintf(b, sizeof b, "%d", (int)parent); guard_log("engine gone (pid file %s, no engine process in this session): ending loop pid %s", pidfile, b); }
    fprintf(stderr, "loop_guard: engine gone (%s), ending loop pid %d\n", pidfile, (int)parent);
    kill(parent, SIGTERM);
    return 0;
}
