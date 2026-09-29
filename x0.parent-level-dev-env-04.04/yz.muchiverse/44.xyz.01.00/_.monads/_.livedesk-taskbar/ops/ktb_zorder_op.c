/* ktb_zorder_op.c - the process-management half of the taskbar's "@" always-on-top toggle.
 *
 * Usage: ktb_zorder_op.+x <house_root> <above|normal> [--dry-run]
 *
 * WHY THIS IS A STANDALONE OP (dock unfactor stage 2, 2026-09-20,
 * 08-roadmap/design-docs/DOCK-UNFACTOR-AUDIT.md): these ~250 lines used to live
 * inside khtpm_core_render.c (ktb_toggle_zorder_respawn/_apply/_apply_tree). They
 * never rendered anything - they scan /proc, SIGTERM windows and re-exec them -
 * so they are process management, not renderer code. The renderer now only flips
 * the mode (khtpm_zorder_mode.state.txt + livedesk_override_redirect.pdl), raises/
 * lowers its OWN dock windows, and spawns this op detached (same "renderer spawns a
 * standalone op" shape as swatch_picker_manager / apply_theme_op).
 *
 * WHAT IT DOES
 *  1. Raises/lowers every top-level "tile:*" entity window (X property WM_NAME).
 *  2. Respawns entity windows so their create-time-only swa.override_redirect picks
 *     up the new mode: SIGTERM, wait for death, re-exec with the same argv.
 *
 * History that used to live in the renderer (kept here, see AUDIT.md section 6):
 *  - 2026-09-13 live report: the strip's own windows are unconditionally WM-managed
 *    regardless of always-on-top (dock_managed in khtpm_core_render.c main()), so the
 *    toggle must NOT respawn the strip - only entities. Identity check = argv contains
 *    a strip template name (same test khtpm_taskbar_manager.c livedesk_kill_strip_
 *    renderers() uses).
 *  - Death is polled with kill(pid,0) (<=6 x 30ms) instead of a flat 300ms sleep:
 *    SIGTERM interrupts select() immediately (handlers installed without SA_RESTART).
 *  - Respawns are staggered 30ms and nice(8) so launching 7-8 GUI processes at once
 *    doesn't saturate this weak-CPU machine (project note nice-heavy-background-work).
 *
 * FIXED HERE (found by the audit): (a) desktop pals run as khtpm_entity.+x since the
 * pal unfactor (da57ae00) but the old needle list never matched them, so the toggle
 * silently stopped respawning pals; (b) the old scan matched processes of EVERY house
 * on the machine (strstr on argv0), so a test/second house could SIGTERM the live
 * desktop - matching is now restricted to binaries under <house_root>.
 *
 * --dry-run prints what would be signalled/respawned and touches nothing (also skips X).
 * Progress/summary goes to <house_root>/#.desktop/ktb_zorder_op.log (rewritten each run).
 */
#define _DEFAULT_SOURCE
#include <X11/Xlib.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

#define PATH_BUF 4096
#define MAX_FOUND 64
#define MAX_ARGS 8

typedef struct {
    pid_t pid;
    int which;
    int argc;
    char arg[MAX_ARGS][PATH_BUF];
} Found;

/* needle in argv0 -> binary (relative to house_root) used to re-exec it */
static const struct { const char *needle; const char *rel; } kKinds[] = {
    { "tp_desktop_window_rgb", "/_.monads/_.livedesk-taskbar/ops/+x/tp_desktop_window_rgb.+x" },
    { "khtpm_entity",          "/_.monads/_.livedesk-taskbar/ops/+x/khtpm_entity.+x" },
    { "khtpm_core_render",     "/_.monads/_.livedesk-taskbar/ops/+x/khtpm_core_render.+x" },
    { "network_browser_render", "/&.hq-apps/network/+x/network_browser_render.+x" },
};
#define N_KINDS ((int)(sizeof(kKinds) / sizeof(kKinds[0])))

static FILE *g_log;
static int g_dry;

static void lograw(const char *s) {
    if (g_dry) { fputs(s, stdout); }
    if (g_log) { fputs(s, g_log); fflush(g_log); }
}

static void raise_or_lower_tiles(int raise) {
    Display *d = XOpenDisplay(NULL);
    Window root, root_ret, parent_ret, *children = NULL;
    unsigned int n = 0, i;
    if (!d) return;
    root = DefaultRootWindow(d);
    if (XQueryTree(d, root, &root_ret, &parent_ret, &children, &n) && children) {
        for (i = 0; i < n; i++) {
            char *nm = NULL;
            if (!XFetchName(d, children[i], &nm) || !nm) continue;
            if (strncmp(nm, "tile:", 5) == 0) {
                if (raise) XRaiseWindow(d, children[i]);
                else XLowerWindow(d, children[i]);
            }
            XFree(nm);
        }
        XFree(children);
    }
    XFlush(d);
    XCloseDisplay(d);
}

static int is_strip(const Found *f) {
    int k;
    for (k = 0; k < f->argc; k++) {
        const char *av = f->arg[k];
        if (strstr(av, "khtpm_strip_header.xhtpm") || strstr(av, "khtpm_strip_bottom.xhtpm") ||
            strstr(av, "strip_header.chtpm") || strstr(av, "strip_bottom.chtpm")) return 1;
    }
    return 0;
}

static int scan(const char *house, Found *found) {
    DIR *pd = opendir("/proc");
    struct dirent *ent;
    size_t hl = strlen(house);
    int n = 0;
    pid_t self = getpid();
    if (!pd) return 0;
    while ((ent = readdir(pd)) != NULL && n < MAX_FOUND) {
        char cpath[300];
        char *cmdbuf;
        FILE *cf;
        size_t got;
        const char *p;
        int which = -1, k;
        if (ent->d_name[0] < '0' || ent->d_name[0] > '9') continue;
        if ((pid_t)atoi(ent->d_name) == self) continue;
        snprintf(cpath, sizeof(cpath), "/proc/%s/cmdline", ent->d_name);
        cf = fopen(cpath, "r");
        if (!cf) continue;
        cmdbuf = malloc(PATH_BUF * MAX_ARGS);
        if (!cmdbuf) { fclose(cf); continue; }
        got = fread(cmdbuf, 1, PATH_BUF * MAX_ARGS - 1, cf);
        fclose(cf);
        if (got == 0) { free(cmdbuf); continue; }
        cmdbuf[got] = '\0';
        /* house-scoped: argv0 must live under THIS house root (see header, fix b) */
        if (strncmp(cmdbuf, house, hl) != 0 || cmdbuf[hl] != '/') { free(cmdbuf); continue; }
        for (k = 0; k < N_KINDS; k++) if (strstr(cmdbuf + hl, kKinds[k].needle)) { which = k; break; }
        if (which < 0) { free(cmdbuf); continue; }
        found[n].pid = (pid_t)atoi(ent->d_name);
        found[n].which = which;
        found[n].argc = 0;
        p = cmdbuf;
        for (k = 0; k < MAX_ARGS; k++) {
            size_t l = strlen(p);
            if (l == 0) break;
            snprintf(found[n].arg[k], PATH_BUF, "%s", p);
            found[n].argc++;
            p += l + 1;
            if (p >= cmdbuf + got) break;
        }
        free(cmdbuf);
        if (is_strip(&found[n])) continue;
        n++;
    }
    closedir(pd);
    return n;
}

int main(int argc, char **argv) {
    const char *house;
    int above, n, i, wait_i;
    Found *found;
    char buf[PATH_BUF * 2];
    if (argc < 3) {
        fprintf(stderr, "usage: %s <house_root> <above|normal> [--dry-run]\n", argv[0]);
        return 2;
    }
    house = argv[1];
    above = strcmp(argv[2], "above") == 0;
    g_dry = (argc > 3 && strcmp(argv[3], "--dry-run") == 0);
    if (!g_dry) {
        char lp[PATH_BUF];
        snprintf(lp, sizeof(lp), "%s/#.desktop/ktb_zorder_op.log", house);
        g_log = fopen(lp, "w");
    }
    found = calloc(MAX_FOUND, sizeof(Found));
    if (!found) return 1;
    n = scan(house, found);
    snprintf(buf, sizeof(buf), "mode=%s house=%s dry=%d matched=%d\n", above ? "above" : "normal", house, g_dry, n);
    lograw(buf);
    for (i = 0; i < n; i++) {
        snprintf(buf, sizeof(buf), "  %s pid=%d argv0=%s argc=%d\n", g_dry ? "WOULD respawn" : "respawn", (int)found[i].pid,
                 found[i].arg[0], found[i].argc);
        lograw(buf);
    }
    if (g_dry) { free(found); return 0; }

    raise_or_lower_tiles(above);
    for (i = 0; i < n; i++) kill(found[i].pid, SIGTERM);
    for (wait_i = 0; wait_i < 6; wait_i++) {
        int all_dead = 1;
        for (i = 0; i < n; i++)
            if (kill(found[i].pid, 0) == 0 || errno != ESRCH) { all_dead = 0; break; }
        if (all_dead) break;
        usleep(30000);
    }
    for (i = 0; i < n; i++) {
        pid_t pid;
        usleep(30000);
        pid = fork();
        if (pid == 0) {
            char bin[PATH_BUF];
            char *av[MAX_ARGS + 1];
            int j, m, devnull;
            setsid();
            (void)!nice(8);
            devnull = open("/dev/null", O_RDWR);
            if (devnull >= 0) {
                dup2(devnull, 0); dup2(devnull, 1); dup2(devnull, 2);
                if (devnull > 2) close(devnull);
            }
            snprintf(bin, sizeof(bin), "%s%s", house, kKinds[found[i].which].rel);
            av[0] = bin;
            m = found[i].argc < MAX_ARGS ? found[i].argc : MAX_ARGS;
            for (j = 1; j < m; j++) av[j] = found[i].arg[j];
            av[m] = NULL;
            execve(bin, av, environ);
            _exit(1);
        }
        snprintf(buf, sizeof(buf), "  spawned pid=%d for old pid=%d\n", (int)pid, (int)found[i].pid);
        lograw(buf);
    }
    /* reap nothing: children are setsid'd and this process exits; init adopts them */
    if (g_log) fclose(g_log);
    free(found);
    return 0;
}
