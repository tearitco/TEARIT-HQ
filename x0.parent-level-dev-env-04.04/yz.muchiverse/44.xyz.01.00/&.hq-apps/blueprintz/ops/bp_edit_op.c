#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
/* bp_edit_op - drive one event-page edit through the events-hq manager, headless (the "drive blueprintz" primitive of the verb-page design,
 * BOOK 16.game/EVENT-VERB-PAGES-DESIGN.md section 10).
 *
 *   bp_edit_op <house> <event_pkg_dir> <label> edit   <cmd id> <type> <params>
 *   bp_edit_op <house> <event_pkg_dir> <label> append <type> <params>
 *   bp_edit_op <house> <event_pkg_dir> <label> delete <cmd id>
 *   bp_edit_op <house> <event_pkg_dir> <label> show
 *
 * It does what a click in events-hq does, with no window: starts the real events-hq manager (<house>/&.widgits/events-hq/ops/+x/
 * khtpm_events_hq_manager.+x <house> <pkg> <label>) on the package, writes the same request line evhq_action.sh's "commit"/"del" write to
 * <pkg>/.hq_manager/action.txt (edit:<id>|<type>|<params>, append:<type>|<params>, delete:<id>), waits until the manager has consumed it and
 * recompiled (event.ir.pdl, event.pal, cmd_N.sh), then stops the manager. So the IR edit and the compile are the manager's, never this op's:
 * one write path (BLUEPRINTZ-DESIGN.md section 3). <params> is the manager's own form, key=value pairs joined by '|' (verb=advance|arg=).
 * Prints "bp_edit: ok ..." and the NODE rows of page_1 afterwards. Exit 0 ok, 1 the manager did not apply it in time or the IR does not show it,
 * 2 usage / setup. The manager child is always killed and reaped, even on failure. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>

static long now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000L + t.tv_nsec / 1000000L; }
static int exists(const char *p) { return access(p, F_OK) == 0; }
static long fsz(const char *p) { struct stat s; return stat(p, &s) == 0 ? (long)s.st_size : -1; }
static int read_all(const char *p, char *buf, size_t cap) { FILE *f = fopen(p, "r"); if (!f) return -1; size_t n = fread(buf, 1, cap - 1, f); buf[n] = 0; fclose(f); return (int)n; }
static pid_t g_mgr = -1;
static void stop_mgr(void) { if (g_mgr > 0) { kill(g_mgr, SIGTERM); for (int i = 0; i < 20; i++) { int st; if (waitpid(g_mgr, &st, WNOHANG) == g_mgr) { g_mgr = -1; return; } usleep(50000); } kill(g_mgr, SIGKILL); waitpid(g_mgr, NULL, 0); g_mgr = -1; } }

/* does the IR (page_1) show the edit? */
static int ir_shows(const char *ir, const char *mode, const char *id, const char *type, const char *params) {
    char buf[16384]; if (read_all(ir, buf, sizeof buf) < 0) return 0;
    if (!strcmp(mode, "delete")) { char want[64]; snprintf(want, sizeof want, "id=%s ", id); for (char *l = strtok(buf, "\n"); l; l = strtok(NULL, "\n")) if (!strncmp(l, "NODE", 4) && strstr(l, want)) return 0; return 1; }
    char want[1200]; snprintf(want, sizeof want, "type=%s | %s", type, params);
    for (char *l = strtok(buf, "\n"); l; l = strtok(NULL, "\n")) if (!strncmp(l, "NODE", 4) && strstr(l, want)) return 1;
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 5) { fprintf(stderr, "usage: bp_edit_op <house> <event_pkg_dir> <label> edit <id> <type> <params> | append <type> <params> | delete <id> | show\n"); return 2; }
    const char *house = argv[1], *pkg = argv[2], *label = argv[3], *mode = argv[4];
    char mgr[PATH_MAX], mdir[PATH_MAX], act[PATH_MAX], page[PATH_MAX], ir[PATH_MAX], line[2000] = "";
    snprintf(mgr, sizeof mgr, "%s/&.widgits/events-hq/ops/+x/khtpm_events_hq_manager.+x", house);
    snprintf(mdir, sizeof mdir, "%s/.hq_manager", pkg); snprintf(act, sizeof act, "%s/action.txt", mdir);
    snprintf(page, sizeof page, "%s/pages/page_1", pkg); snprintf(ir, sizeof ir, "%s/event.ir.pdl", page);
    if (!exists(mgr)) { fprintf(stderr, "bp_edit: no manager at %s\n", mgr); return 2; }
    if (!exists(ir)) { fprintf(stderr, "bp_edit: no page_1 IR in %s\n", pkg); return 2; }
    const char *id = "", *type = "", *params = "";
    if (!strcmp(mode, "edit") && argc >= 8) { id = argv[5]; type = argv[6]; params = argv[7]; snprintf(line, sizeof line, "edit:%s|%s|%s\n", id, type, params); }
    else if (!strcmp(mode, "append") && argc >= 7) { type = argv[5]; params = argv[6]; snprintf(line, sizeof line, "append:%s|%s\n", type, params); }
    else if (!strcmp(mode, "delete") && argc >= 6) { id = argv[5]; snprintf(line, sizeof line, "delete:%s\n", id); }
    else if (strcmp(mode, "show")) { fprintf(stderr, "bp_edit: bad mode or arguments\n"); return 2; }
    for (const char *c = line; *c; c++) if (*c == '\r' || (*c == '\n' && c[1])) { fprintf(stderr, "bp_edit: a newline in the request\n"); return 2; }

    if (line[0]) {
        pid_t pid = fork(); if (pid < 0) return 2;
        if (pid == 0) { int fd = open("/dev/null", O_RDWR); if (fd >= 0) { dup2(fd, 0); dup2(fd, 1); dup2(fd, 2); } execl(mgr, mgr, house, pkg, label, (char *)NULL); _exit(127); }
        g_mgr = pid; atexit(stop_mgr);
        { char ps[PATH_MAX]; snprintf(ps, sizeof ps, "%s/page.state.txt", mdir); long t0 = now_ms(); while (now_ms() - t0 < 6000 && !exists(ps)) usleep(50000); }      /* the manager is up once it has published the page */
        usleep(900000);      /* the manager truncates action.txt once at start; its loop runs every 400 ms: be past the first pass before writing */
        char tmp[PATH_MAX]; snprintf(tmp, sizeof tmp, "%s.bp", act);
        FILE *f = fopen(tmp, "w"); if (!f) { fprintf(stderr, "bp_edit: cannot write the request\n"); return 1; } fputs(line, f); fclose(f); rename(tmp, act);
        int applied = 0; long t2 = now_ms();
        while (now_ms() - t2 < 10000) { if (fsz(act) == 0 && ir_shows(ir, mode, id, type, params)) { applied = 1; break; } usleep(100000); }
        if (!applied) { fprintf(stderr, "bp_edit: the manager did not apply the request in 10 s (action.txt %ld bytes)\n", fsz(act)); return 1; }
        usleep(300000);      /* let the compile finish writing event.pal and cmd_N.sh */
    }
    char buf[16384]; if (read_all(ir, buf, sizeof buf) >= 0) { printf("bp_edit: ok %s %s\n", mode, label); for (char *l = strtok(buf, "\n"); l; l = strtok(NULL, "\n")) if (!strncmp(l, "NODE", 4)) printf("%s\n", l); }
    return 0;
}
