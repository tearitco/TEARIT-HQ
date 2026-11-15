/* window_place_op.c - the ONE window-placement service (2026-11-15, owner: phones must not open on top of each other, "we could use the same service for any window, if opted .pdl").
 *   window_place_op place <app_dir> <instance_dir> <w> <h> [--screen-w N]
 * Picks a free slot for a window of w x h and writes <instance_dir>/window_pos.txt ("x y") and window_size.txt ("w h"). The shared renderer (khtpm_core_render.c, kh_instance_window_pos) moves a window opened with
 * that instance dir to its window_pos.txt, so ANY window that has an instance dir can opt in by (1) having a placement.pdl in its app folder and (2) its launcher calling this op before it starts the window.
 * Slot rule (flex-wrap): scan left to right from the margin; a spot that overlaps a LIVE sibling moves to the right of that sibling; past the screen's right edge it wraps to a new row below the lowest overlapping sibling.
 * A sibling = a folder under <app_dir>/<GROUP>/ (default "instances") other than this one, with window.pid naming a live process, window_pos.txt and window_size.txt. A dead sibling is ignored (its slot is free again).
 * <app_dir>/placement.pdl (all rows optional, "KEY | value"): GROUP | instances    MARGIN_X | 20    TOP_Y | 110    GAP | 12.   A window with no placement.pdl is NOT placed (the op refuses, nothing is written).
 * Screen width: X11 root width, or --screen-w N (tests), fallback 1920. Build: gcc -std=gnu11 -Wall -O2 -o ops/+x/window_place_op.+x ops/window_place_op.c -lX11 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <dirent.h>
#include <sys/stat.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#define P 4096
#define MAXSIB 32
typedef struct { int x, y, w, h; } Rect;

static int kv(const char *file, const char *key, char *out, size_t cap) { out[0] = 0; FILE *fp = fopen(file, "r"); if (!fp) return 0; char ln[300]; size_t kl = strlen(key); int hit = 0; while (fgets(ln, sizeof ln, fp)) { if (strncmp(ln, key, kl) || strncmp(ln + kl, " | ", 3)) continue; snprintf(out, cap, "%s", ln + kl + 3); out[strcspn(out, "\r\n")] = 0; hit = 1; break; } fclose(fp); return hit; }
static int num(const char *file, const char *key, int def, int lo, int hi) { char b[32]; if (!kv(file, key, b, sizeof b)) return def; char *e; long v = strtol(b, &e, 10); return (*b && !*e && v >= lo && v <= hi) ? (int)v : def; }
static int alive(const char *f) { FILE *fp = fopen(f, "r"); if (!fp) return 0; char b[32] = ""; if (fgets(b, sizeof b, fp)) {} fclose(fp); long pid = atol(b); if (pid <= 0) return 0; return kill((pid_t)pid, 0) == 0 || errno == EPERM; }
static int overlap(Rect a, Rect b, int gap) { return a.x < b.x + b.w + gap && b.x < a.x + a.w + gap && a.y < b.y + b.h + gap && b.y < a.y + a.h + gap; }
static void wr(const char *path, const char *text) { char t[P + 8]; snprintf(t, sizeof t, "%s.tmp", path); FILE *w = fopen(t, "w"); if (!w) return; fputs(text, w); fclose(w); rename(t, path); }
/* 2026-11-15 (owner: phones must not open over the left GNOME dock): the desktop publishes its usable area (_NET_WORKAREA = x y w h, the dock and panel already subtracted); used only when --screen-w is NOT given, so a harness stays deterministic */
static int work_area(int *x, int *y, int *w) { Display *d = XOpenDisplay(NULL); if (!d) return 0; Atom a = XInternAtom(d, "_NET_WORKAREA", True); Atom t; int f; unsigned long n = 0, rem = 0; unsigned char *p = NULL; int ok = 0;
    if (a != None && XGetWindowProperty(d, DefaultRootWindow(d), a, 0, 4, False, XA_CARDINAL, &t, &f, &n, &rem, &p) == Success && p && n >= 4 && f == 32) { long *v = (long *)p; if (v[2] > 400) { *x = (int)v[0]; *y = (int)v[1]; *w = (int)v[2]; ok = 1; } } if (p) XFree(p); XCloseDisplay(d); return ok; }
static int screen_w(void) { Display *d = XOpenDisplay(NULL); if (!d) return 1920; int w = DisplayWidth(d, DefaultScreen(d)); XCloseDisplay(d); return w > 400 ? w : 1920; }

int main(int argc, char **argv) {
    if (argc < 6 || strcmp(argv[1], "place")) { fprintf(stderr, "usage: window_place_op place <app_dir> <instance_dir> <w> <h> [--screen-w N]\n"); return 2; }
    const char *app = argv[2], *inst = argv[3]; int w = atoi(argv[4]), h = atoi(argv[5]); int sw = 0; for (int i = 6; i + 1 < argc; i++) if (!strcmp(argv[i], "--screen-w")) sw = atoi(argv[i + 1]);
    if (w < 50 || h < 50 || w > 20000 || h > 20000) { fprintf(stderr, "window_place_op: bad size\n"); return 2; }
    char pdl[P]; snprintf(pdl, sizeof pdl, "%s/placement.pdl", app); if (access(pdl, R_OK)) { printf("PLACE-REFUSED no placement.pdl in %s (the app has not opted in)\n", app); return 1; }
    char group[64]; if (!kv(pdl, "GROUP", group, sizeof group) || !group[0] || strchr(group, '/') || strstr(group, "..")) snprintf(group, sizeof group, "instances");
    int mx = num(pdl, "MARGIN_X", 20, 0, 2000), ty = num(pdl, "TOP_Y", 110, 0, 2000), gap = num(pdl, "GAP", 12, 0, 500); if (sw <= 0) { int wx, wy, ww; if (work_area(&wx, &wy, &ww)) { if (mx < wx + 10) mx = wx + 10; if (ty < wy + 8) ty = wy + 8; sw = wx + ww; } else sw = screen_w(); }
    static Rect sib[MAXSIB]; int n = 0; char gd[P]; snprintf(gd, sizeof gd, "%s/%s", app, group); DIR *dp = opendir(gd);
    if (dp) { struct dirent *e; while ((e = readdir(dp)) && n < MAXSIB) { if (e->d_name[0] == '.') continue; char in[P], f[P]; snprintf(in, sizeof in, "%s/%s", gd, e->d_name);
            char a[P], b[P]; if (realpath(in, a) && realpath(inst, b) && !strcmp(a, b)) continue;      /* not itself */
            snprintf(f, sizeof f, "%s/window.pid", in); if (!alive(f)) continue; Rect r; snprintf(f, sizeof f, "%s/window_pos.txt", in); FILE *fp = fopen(f, "r"); if (!fp) continue; int ok = fscanf(fp, "%d %d", &r.x, &r.y) == 2; fclose(fp); if (!ok) continue;
            snprintf(f, sizeof f, "%s/window_size.txt", in); fp = fopen(f, "r"); if (!fp) continue; ok = fscanf(fp, "%d %d", &r.w, &r.h) == 2; fclose(fp); if (!ok) continue; sib[n++] = r; } closedir(dp); }
    Rect me = { mx, ty, w, h };
    for (int guard = 0; guard < 200; guard++) {
        int hit = -1; for (int i = 0; i < n; i++) if (overlap(me, sib[i], gap)) { hit = i; break; }
        if (hit < 0) break;
        me.x = sib[hit].x + sib[hit].w + gap;
        if (me.x + me.w > sw - mx) { me.x = mx; int low = 0; Rect row = { mx, me.y, sw, me.h }; for (int i = 0; i < n; i++) if (overlap(row, sib[i], gap) && sib[i].y + sib[i].h > low) low = sib[i].y + sib[i].h; me.y = (low ? low : me.y) + gap; }
    }
    char f[P], t[40]; snprintf(t, sizeof t, "%d %d\n", me.x, me.y); snprintf(f, sizeof f, "%s/window_pos.txt", inst); wr(f, t); snprintf(t, sizeof t, "%d %d\n", w, h); snprintf(f, sizeof f, "%s/window_size.txt", inst); wr(f, t);
    printf("PLACED %d %d\n", me.x, me.y); return 0;
}
