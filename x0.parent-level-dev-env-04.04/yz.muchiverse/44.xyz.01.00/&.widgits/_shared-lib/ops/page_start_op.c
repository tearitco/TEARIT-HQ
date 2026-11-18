/* page_start_op - put an editor's cursor (the xelector) on the page's own start cell (a `start | x y` row in the page's view.pdl), else the page middle: a painted page can be mostly water, and tile ids alone do not say what is land (tried 2026-11-17: the centroid of non-empty cells was still ocean), so a freshly opened page shows its land and not the empty corner (owner 2026-11-17: "start over land so there is something to see", phone and board).
 * Usage: page_start_op <data dir> <book> <page> [<session dir>]      (data dir = a pc-hq instance, e.g. <house>/@.apps/rpg-edit-hq/rpg-edit-hq-pchq; session dir = the engine session whose bv_state.txt holds selector_x/y)
 * Reads width= / height= from <data>/pieces/system/maps/<book>/<page>/cells.txt, then makes the SAME writes a click or arrow key makes (bv_xelector_to): selector_x/y in <session>/pieces/system/bv_state.txt (when a session is given) and pos_x/pos_y in
 * <data>/pieces/xelector_01/state.txt, then appends a line to <session>/pieces/display/bv_screen_changed.txt so the next frame draws. 
 * It does nothing for a page without cells.txt (not painted). Prints START <x> <y> or START-SKIP <why>. Exit 0 ok / skip, 2 usage. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../khtpm_locations.c"
#include "../pchq_book_path.c"   /* the book may live in its app, not in the data folder: no symlinks (owner 2026-11-18) */
static int kv_int(const char *file, const char *key, int dflt) { FILE *f = fopen(file, "r"); if (!f) return dflt; char ln[300]; size_t kl = strlen(key); while (fgets(ln, sizeof ln, f)) if (!strncmp(ln, key, kl) && ln[kl] == '=') { fclose(f); return atoi(ln + kl + 1); } fclose(f); return dflt; }
static void kv_set(const char *file, const char *key, int val) {      /* replace the key's line, or append it; whole file rewritten through a temp name */
    static char buf[65536]; size_t n = 0; FILE *f = fopen(file, "r"); if (f) { n = fread(buf, 1, sizeof buf - 1, f); fclose(f); } buf[n] = 0;
    static char out[65536 + 128]; size_t o = 0; int done = 0; size_t kl = strlen(key); const char *p = buf;
    while (*p) { const char *e = strchr(p, '\n'); size_t ll = e ? (size_t)(e - p) : strlen(p);
        if (!done && ll > kl && !strncmp(p, key, kl) && p[kl] == '=') { o += (size_t)snprintf(out + o, sizeof out - o, "%s=%d\n", key, val); done = 1; } else { memcpy(out + o, p, ll); o += ll; out[o++] = '\n'; }
        p += ll + (e ? 1 : 0); }
    if (!done) o += (size_t)snprintf(out + o, sizeof out - o, "%s=%d\n", key, val);
    char t[4500]; snprintf(t, sizeof t, "%s.tmp%d", file, (int)getpid()); f = fopen(t, "w"); if (!f) return; fwrite(out, 1, o, f); fclose(f); rename(t, file);
}
static int pdl_start(const char *file, int *x, int *y) {      /* a row  start | x y  */
    FILE *f = fopen(file, "r"); if (!f) return 0; char ln[300]; int ok = 0;
    while (fgets(ln, sizeof ln, f)) { char *c = ln; while (*c == ' ') c++; if (strncmp(c, "start", 5)) continue; char *b = strchr(c, '|'); if (!b) continue; if (sscanf(b + 1, " %d %d", x, y) == 2) { ok = 1; break; } }
    fclose(f); return ok;
}
int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: page_start_op <data dir> <book> <page> [<session dir>]\n"); return 2; }
    const char *data = argv[1], *book = argv[2], *page = argv[3], *sess = argc > 4 ? argv[4] : "";
    if (strchr(book, '/') || strchr(page, '/') || strstr(book, "..") || strstr(page, "..")) { fprintf(stderr, "page_start_op: bad book or page name\n"); return 2; }
    char pd[4400], f[4500]; snprintf(pd, sizeof pd, "%s/%s", bp_dir(data, book), page);
    snprintf(f, sizeof f, "%s/cells.txt", pd); int w = kv_int(f, "width", 0), h = kv_int(f, "height", 0);
    if (w < 1 || h < 1) { printf("START-SKIP page is not painted\n"); return 0; }
    char xp[4500]; snprintf(xp, sizeof xp, "%s/pieces/xelector_01/state.txt", data);      /* a possessed cursor is moved too: this is a FRESH page open, and the view follows the selector (checked live 2026-11-17 with hero_01 possessing) */
    int x = w / 2, y = h / 2;
    snprintf(f, sizeof f, "%s/view.pdl", pd); pdl_start(f, &x, &y);                                            /* a start row of the page wins */
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= w) x = w - 1;
    if (y >= h) y = h - 1;
    kv_set(xp, "pos_x", x); kv_set(xp, "pos_y", y);
    if (sess[0]) { char sp[4500]; snprintf(sp, sizeof sp, "%s/pieces/system/bv_state.txt", sess); kv_set(sp, "selector_x", x); kv_set(sp, "selector_y", y);
        snprintf(sp, sizeof sp, "%s/pieces/display/bv_screen_changed.txt", sess); FILE *m = fopen(sp, "a"); if (m) { fputs("start\n", m); fclose(m); } }
    printf("START %d %d\n", x, y); return 0;
}
