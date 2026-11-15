/* layout_op.c - one-call layout tools for simple agents (layout-studio layer 4-5, 2026-11-15; strategy BOOK 08-roadmap/design-docs/LAYOUT-STUDIO-FOR-SIMPLE-AGENTS-STRATEGY-2026-10-09.md).
 *   layout_op preview <house> <layout.xhtpm> <out.png> [--inst <instance_dir>] [--wait <s>]
 *     1. runs layout_check (refuses with its ERROR lines, exit 1, nothing launched, when the layout has errors)
 *     2. starts the real renderer on the layout through the spawn funnel (registered process, role tool), --inst = the instance dir (the renderer's argv[3] hook; needed by windows that read one)
 *     3. finds the new window by DIFFING the X window list (khtpm windows set no _NET_WM_PID and have no name), waits for it to be viewable, lets it draw, photographs it with dump_frame_png_op
 *     4. closes the window by its exact pid.  Prints PREVIEW-OK <png> <w>x<h> or PREVIEW-FAIL <reason> (exit 1). The nav numbers are the badges in the PNG (they are global across windows, so read them there).
 * Build: sh build_layout_op.sh (gcc -std=gnu11 -Wall -O2 ... -lX11) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <X11/Xlib.h>
#define P 4096
static int run_out(char *const av[], const char *outf) { pid_t c = fork(); if (c < 0) return -1;
    if (c == 0) { int fd = open(outf, O_WRONLY | O_CREAT | O_TRUNC, 0644); if (fd >= 0) { dup2(fd, 1); dup2(fd, 2); } int nul = open("/dev/null", O_RDONLY); if (nul >= 0) dup2(nul, 0); execv(av[0], av); _exit(127); }
    int st; waitpid(c, &st, 0); return WIFEXITED(st) ? WEXITSTATUS(st) : -1; }
static int fail(const char *why) { printf("PREVIEW-FAIL %s\n", why); return 1; }
static int list_wins(Display *d, Window *w, int cap) { Window r, p, *k; unsigned n = 0; if (!XQueryTree(d, DefaultRootWindow(d), &r, &p, &k, &n)) return 0; int m = 0; for (unsigned i = 0; i < n && m < cap; i++) w[m++] = k[i]; if (k) XFree(k); return m; }
int main(int argc, char **argv) {
    if (argc < 5 || strcmp(argv[1], "preview")) { fprintf(stderr, "usage: layout_op preview <house> <layout.xhtpm> <out.png> [--inst dir] [--wait s]\n"); return 2; }
    const char *house = argv[2], *layout = argv[3], *png = argv[4], *inst = NULL; int wait_s = 12;
    for (int i = 5; i + 1 < argc; i += 2) { if (!strcmp(argv[i], "--inst")) inst = argv[i + 1]; else if (!strcmp(argv[i], "--wait")) wait_s = atoi(argv[i + 1]); }
    if (access(layout, R_OK)) return fail("layout file not readable");
    char chk[P], exe[P], funnel[P], dump[P], tmp[P]; snprintf(chk, sizeof chk, "%s/@.apps/layout-studio/ops/+x/layout_check.+x", house); snprintf(exe, sizeof exe, "%s/_.monads/_.livedesk-taskbar/ops/+x/khtpm_core_render.+x", house);
    snprintf(funnel, sizeof funnel, "%s/&.widgits/_shared-lib/kh_spawn.sh", house); snprintf(dump, sizeof dump, "%s/&.widgits/_shared-lib/ops/+x/dump_frame_png_op.+x", house); snprintf(tmp, sizeof tmp, "%s.preview.tmp", png);
    if (access(chk, X_OK) || access(exe, X_OK) || access(funnel, R_OK) || access(dump, X_OK)) return fail("a tool is missing (layout_check, renderer, funnel or dump op)");
    { char *av[] = { chk, (char *)layout, NULL }; int rc = run_out(av, tmp); if (rc != 0) { FILE *f = fopen(tmp, "r"); char ln[400]; while (f && fgets(ln, sizeof ln, f)) fputs(ln, stdout); if (f) fclose(f); unlink(tmp); return fail("layout_check found errors (fix them first)"); } }
    Display *d = XOpenDisplay(NULL); if (!d) { unlink(tmp); return fail("no X display"); }
    static Window before[4096]; int nb = list_wins(d, before, 4096);
    setenv("KH_ROLE", "tool", 1); setenv("KH_APP", "layout-preview", 1);
    char *av[16]; int k = 0; av[k++] = "/bin/sh"; av[k++] = funnel; av[k++] = (char *)house; av[k++] = "0"; av[k++] = "layout-preview"; av[k++] = "--"; av[k++] = exe; av[k++] = (char *)house; av[k++] = (char *)layout; if (inst) av[k++] = (char *)inst; av[k] = NULL;
    run_out(av, tmp); long pid = 0; { FILE *f = fopen(tmp, "r"); char out[64] = ""; if (f) { char *r = fgets(out, sizeof out, f); (void)r; fclose(f); } pid = atol(out); } unlink(tmp);
    if (pid <= 1) { XCloseDisplay(d); return fail("the renderer did not start"); }
    Window win = 0; int w = 0, h = 0;
    for (int t = 0; t < wait_s * 4 && !win; t++) { usleep(250000); static Window now[4096]; int nn = list_wins(d, now, 4096);
        for (int i = 0; i < nn && !win; i++) { int seen = 0; for (int j = 0; j < nb; j++) if (before[j] == now[i]) seen = 1;
            if (seen) continue; XWindowAttributes a; if (XGetWindowAttributes(d, now[i], &a) && a.map_state == IsViewable && a.width > 60 && a.height > 60) { win = now[i]; w = a.width; h = a.height; } } }
    if (!win) { kill((pid_t)pid, SIGTERM); XCloseDisplay(d); return fail("no new window appeared"); }
    sleep(3); { XWindowAttributes a; if (XGetWindowAttributes(d, win, &a)) { w = a.width; h = a.height; } }
    char ws[32]; snprintf(ws, sizeof ws, "0x%lx", (unsigned long)win); char *dv[] = { dump, ws, (char *)png, NULL }; int rc = run_out(dv, tmp); unlink(tmp);
    kill((pid_t)pid, SIGTERM); XCloseDisplay(d);
    if (rc != 0 || access(png, R_OK)) return fail("the frame dump failed");
    printf("PREVIEW-OK %s %dx%d\n", png, w, h); return 0;
}
