/* bp_ctl.c - blueprintz debugger controller (BOOK BLUEPRINTZ-DESIGN.md 12.3-12.4).
 * Usage: bp_ctl <house> <command> [args]
 *   status                 paused/attached flags, queued events, active VM run, last trace row
 *   pause | resume | detach   pause = "paused=1" in the debug file (triggers are queued by
 *                          play_event.sh, not run, not dropped); resume = paused=0 and release the
 *                          queue in order; detach = remove the debug file (back to plain Play)
 *   queue                  list the events waiting in the queue
 *   step-event             run the oldest queued event (all of it), stay paused
 *   step-cmd               if a VM run is held, let it run ONE instruction (one marker byte);
 *                          otherwise start the oldest queued event in command-stepping mode
 *   run-to-end | abort     free-run / stop the held VM run
 *   fire <pkg_dir> <trigger>   send a trigger the way a click would (goes through the pause gate)
 * State (all append-only text, change detection by size growth, never mtime):
 *   <state>/queue.txt      QUEUED | ts | pkg | trigger         (written by play_event.sh)
 *   <state>/queue.cursor   how many queue rows were released  (this tool)
 *   <state>/trace.txt      START/END rows                     (written by play_event.sh)
 *   <state>/vm/current.txt one VM step directory per line     (written by play_event.sh)
 * Test overrides (same names play_event.sh reads): BLUEPRINTZ_DEBUG_FILE, BLUEPRINTZ_STATE_DIR,
 * BLUEPRINTZ_PLAY (path of play_event.sh). Exit: 0 ok, 2 usage, 3 nothing to do, 4 run failed. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>

#define P 4400
static char house[4096], flagp[P], state[P], play[P], logp[P];

static void join(char *out, const char *a, const char *b) { snprintf(out, P, "%s/%s", a, b); }
static void paths(const char *h) {
    const char *e;
    snprintf(house, sizeof house, "%s", h);
    e = getenv("BLUEPRINTZ_DEBUG_FILE");
    if (e && *e) snprintf(flagp, P, "%s", e); else snprintf(flagp, P, "%s/#.desktop/blueprintz_debug.txt", house);
    e = getenv("BLUEPRINTZ_STATE_DIR");
    if (e && *e) snprintf(state, P, "%s", e); else snprintf(state, P, "%s/&.hq-apps/blueprintz/state", house);
    e = getenv("BLUEPRINTZ_PLAY");
    if (e && *e) snprintf(play, P, "%s", e); else snprintf(play, P, "%s/&.widgits/events-hq/ops/play_event.sh", house);
    join(logp, state, "run.log");
}
static void mkdir_p(const char *d) {
    char t[P]; snprintf(t, P, "%s", d);
    for (char *p = t + 1; *p; p++) if (*p == '/') { *p = 0; mkdir(t, 0755); *p = '/'; }
    mkdir(t, 0755);
}
static int exists(const char *p) { struct stat st; return stat(p, &st) == 0; }
static int write_text(const char *p, const char *s) { FILE *f = fopen(p, "w"); if (!f) return -1; fputs(s, f); fclose(f); return 0; }
static void append_byte(const char *p) { FILE *f = fopen(p, "a"); if (f) { fputc('x', f); fclose(f); } }

/* last line of a small text file into out; returns 1 if there is one */
static int last_line(const char *p, char *out, size_t n) {
    FILE *f = fopen(p, "r"); if (!f) return 0;
    char l[P]; int got = 0; out[0] = 0;
    while (fgets(l, sizeof l, f)) { l[strcspn(l, "\r\n")] = 0; if (l[0]) { snprintf(out, n, "%s", l); got = 1; } }
    fclose(f); return got;
}
static int count_lines(const char *p) {
    FILE *f = fopen(p, "r"); if (!f) return 0;
    char l[P]; int n = 0; while (fgets(l, sizeof l, f)) if (l[0] != '\n') n++;
    fclose(f); return n;
}
static int is_paused(void) {
    FILE *f = fopen(flagp, "r"); if (!f) return 0;
    char l[256]; int p = 0; while (fgets(l, sizeof l, f)) if (strncmp(l, "paused=1", 8) == 0) p = 1;
    fclose(f); return p;
}

/* queue: row index i (0-based) -> pkg and trigger; returns 1 if it exists */
static int queue_row(int i, char *pkg, char *trig) {
    char q[P]; join(q, state, "queue.txt");
    FILE *f = fopen(q, "r"); if (!f) return 0;
    char l[P]; int n = 0, ok = 0;
    while (fgets(l, sizeof l, f)) {
        if (strncmp(l, "QUEUED | ", 9) != 0) continue;
        if (n++ != i) continue;
        l[strcspn(l, "\r\n")] = 0;
        char *a = strstr(l + 9, " | ");          /* after ts */
        if (!a) break;
        char *b = strstr(a + 3, " | ");          /* after pkg */
        if (!b) break;
        *b = 0; snprintf(pkg, P, "%s", a + 3); snprintf(trig, 256, "%s", b + 3); ok = 1; break;
    }
    fclose(f); return ok;
}
static int cursor_get(void) { char c[P], l[64]; join(c, state, "queue.cursor"); return last_line(c, l, sizeof l) ? atoi(l) : 0; }
static void cursor_set(int v) { char c[P], t[32]; join(c, state, "queue.cursor"); snprintf(t, sizeof t, "%d\n", v); write_text(c, t); }
static int queue_total(void) { char q[P]; join(q, state, "queue.txt"); return count_lines(q); }

/* run play_event.sh for pkg/trigger. release: pass BLUEPRINTZ_RELEASE=1. stepcmd: BLUEPRINTZ_STEPCMD=1.
 * background: detach and return at once. Returns the exit status (foreground) or 0. */
static int run_play(const char *pkg, const char *trig, int release, int stepcmd, int background) {
    pid_t pid = fork();
    if (pid < 0) return 4;
    if (pid == 0) {
        if (background) {
            if (setsid() < 0) _exit(1);
            pid_t p2 = fork(); if (p2 != 0) _exit(0);
        }
        if (release) setenv("BLUEPRINTZ_RELEASE", "1", 1); else unsetenv("BLUEPRINTZ_RELEASE");
        if (stepcmd) setenv("BLUEPRINTZ_STEPCMD", "1", 1);
        mkdir_p(state);
        if (!freopen(logp, "a", stdout) || !freopen(logp, "a", stderr) || !freopen("/dev/null", "r", stdin)) _exit(126);
        execl(play, play, pkg, house, trig, (char *)NULL);
        _exit(127);
    }
    int st = 0; waitpid(pid, &st, 0);
    if (background) return 0;
    return WIFEXITED(st) ? WEXITSTATUS(st) : 4;
}

/* the held VM run, if any: dir of the newest vm step directory whose trace's last row is a STEP */
static int active_vm(char *dir, char *lastrow) {
    char cur[P], tr[P]; join(cur, state, "vm/current.txt");
    if (!last_line(cur, dir, P)) return 0;
    join(tr, dir, "vm.trace");
    if (!last_line(tr, lastrow, P)) return 0;
    return strncmp(lastrow, "STEP", 4) == 0;
}
static void wait_ms(int ms) { struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L }; nanosleep(&ts, NULL); }

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: bp_ctl <house> status|pause|resume|detach|queue|step-event|step-cmd|run-to-end|abort|fire <pkg> <trigger>\n"); return 2; }
    paths(argv[1]);
    const char *cmd = argv[2];
    mkdir_p(state);
    char pkg[P], trig[256], dir[P], row[P];

    if (!strcmp(cmd, "status")) {
        char tr[P]; join(tr, state, "trace.txt");
        printf("attached=%d\npaused=%d\nqueue_pending=%d\n", exists(flagp), is_paused(), queue_total() - cursor_get());
        if (active_vm(dir, row)) printf("vm_active=%s\nvm_next=%s\n", dir, row); else printf("vm_active=-\n");
        if (last_line(tr, row, sizeof row)) printf("last_trace=%s\n", row); else printf("last_trace=-\n");
        return 0;
    }
    if (!strcmp(cmd, "pause")) { if (write_text(flagp, "paused=1\n")) { perror("debug file"); return 4; } printf("ok paused\n"); return 0; }
    if (!strcmp(cmd, "detach")) { unlink(flagp); printf("ok detached\n"); return 0; }
    if (!strcmp(cmd, "resume")) {
        if (write_text(flagp, "paused=0\n")) { perror("debug file"); return 4; }
        int n = 0, i;
        while ((i = cursor_get()), queue_row(i, pkg, trig)) { cursor_set(i + 1); run_play(pkg, trig, 1, 0, 0); n++; }
        printf("ok resumed released=%d\n", n); return 0;
    }
    if (!strcmp(cmd, "queue")) {
        int t = queue_total(), c = cursor_get();
        for (int i = c; i < t; i++) if (queue_row(i, pkg, trig)) printf("%d %s %s\n", i - c + 1, trig, pkg);
        if (t - c <= 0) printf("(empty)\n");
        return 0;
    }
    if (!strcmp(cmd, "step-event")) {
        int i = cursor_get();
        if (!queue_row(i, pkg, trig)) { printf("nothing queued\n"); return 3; }
        cursor_set(i + 1);
        int rc = run_play(pkg, trig, 1, 0, 0);
        printf("stepped event %s trigger=%s rc=%d\n", pkg, trig, rc);
        return rc ? 4 : 0;
    }
    if (!strcmp(cmd, "step-cmd")) {
        if (active_vm(dir, row)) {
            char tr[P], mk[P]; join(tr, dir, "vm.trace"); join(mk, dir, "step.marker");
            int before = count_lines(tr);
            append_byte(mk);
            for (int k = 0; k < 300 && count_lines(tr) <= before; k++) wait_ms(10);
            last_line(tr, row, sizeof row);
            printf("stepped cmd; now: %s\n", row); return 0;
        }
        int i = cursor_get();
        if (!queue_row(i, pkg, trig)) { printf("nothing queued and no held VM run\n"); return 3; }
        cursor_set(i + 1);
        run_play(pkg, trig, 1, 1, 1);
        for (int k = 0; k < 500; k++) {            /* wait up to 5 s for the first held instruction */
            if (active_vm(dir, row)) { printf("started %s; held at: %s\n", pkg, row); return 0; }
            wait_ms(10);
        }
        printf("started %s but no held VM run appeared (see run.log)\n", pkg); return 4;
    }
    if (!strcmp(cmd, "run-to-end") || !strcmp(cmd, "abort")) {
        if (!active_vm(dir, row)) { printf("no held VM run\n"); return 3; }
        char f[P]; join(f, dir, !strcmp(cmd, "abort") ? "abort" : "run");
        write_text(f, "x\n"); printf("ok %s\n", cmd); return 0;
    }
    if (!strcmp(cmd, "fire")) {
        if (argc < 5) { fprintf(stderr, "fire needs <pkg_dir> <trigger>\n"); return 2; }
        int rc = run_play(argv[3], argv[4], 0, 0, 0);
        printf("fired %s trigger=%s rc=%d\n", argv[3], argv[4], rc);
        return rc ? 4 : 0;
    }
    fprintf(stderr, "bp_ctl: unknown command %s\n", cmd); return 2;
}
