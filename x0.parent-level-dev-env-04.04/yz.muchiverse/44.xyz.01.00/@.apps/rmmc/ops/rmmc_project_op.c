/* rmmc_project_op - fold the vox LEDGER over the SEED island and write the PROJECTION the engine reads (plan 6 and 7.3, gate G3, D137).
 *   rmmc_project_op project <seed_dir> <out_dir> <ledger> <pose.pdl> <terrain.heightmap> [--idle] [--no-marker]
 * Seed (CONFIG, never written here): <seed_dir>/{cells.txt, map.txt, events.txt, legend.txt, extrusion.pdl, cells.rgba, skin.txt, rig_*.pdl}.
 * Projection (safe to delete, rewritten byte for byte; NEVER read back as input): <out_dir>/{cells.txt, map.txt, events.txt, cells.marker} are regenerated; extrusion.pdl legend.txt cells.rgba skin.txt rig_*.pdl are COPIES of the seed
 * (the engine reads this folder, so they must be here; they are rewritten only when they differ). Order: the support files, then cells.txt, map.txt, events.txt, and cells.marker LAST (the engine must see a whole desk).
 * What it does with the ledger (rows of vox_op: MOVE FACE RESET SPAWNPOINT DAMAGE MINE PLACE; every other row is ignored here):
 *   events.txt   each seed row `x y r g b rig:<actor> 0 dir pattern` whose actor has ledger MOVE rows is rewritten with the actor's position (x, vox z = desk row), direction and pattern; other seed rows are kept.
 *                direction = the last MOVE delta (n 8, e 6, s 2, w 4) or the last FACE row, whichever is later (DIR rows of pose.pdl); pattern from pose.pdl by the LAST pose row of that actor (MOVE_ODD/MOVE_EVEN alternate with the
 *                count of ground MOVEs, MOVE_LEVEL, DAMAGE, MINE, PLACE, FACE, RESET). --idle writes pattern 0 for every actor (IDLE): the expiry belongs to the driver, never to a ledger row.
 *   cells.txt, map.txt  the seed bytes, except a MINE of the top block of a column (level H -> H-1, the floor becomes the wall tile) and a PLACE on top of it (H+1, floor and wall = the placed item's tile): NOT VERIFIED on the island yet
 *                (break and place are priority 3); overhangs and tunnels are not representable (a MINE or PLACE that is not at the column top is ignored).
 *   cells.marker grows: new size = (old size mod 4000) + 1 byte of '1', on EVERY run (the engine reloads the desk when the SIZE changes, never on mtime). --no-marker skips it (an events-only refresh if a full reload hitches).
 * `event_lift=1` stays because cells.txt is the seed's header. Axes: desk(x, row) = vox(x, y height, z = row). Prints: projected actors=<n> pattern=<last> marker=<size>.
 * Exit: 0 ok, 2 usage, 3 a seed file is missing, 4 a bad row, 5 cannot write.
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/rmmc_project_op.+x rmmc_project_op.c */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#define NM 32
#define MAXD 256
#define MAXA 8
#define MAXP 32
static int split(char *line, char **f, int max) {
    int n = 0; char *s = line;
    while (n < max) { while (*s == ' ') s++; f[n++] = s; char *b = strchr(s, '|'); if (!b) break; *b = 0; s = b + 1; }
    for (int i = 0; i < n; i++) { char *e = f[i] + strlen(f[i]); while (e > f[i] && isspace((unsigned char)e[-1])) *--e = 0; }
    return n;
}
static char *slurp(const char *p, size_t *len) { FILE *f = fopen(p, "rb"); if (!f) return NULL; fseek(f, 0, SEEK_END); long n = ftell(f); rewind(f); char *b = malloc((size_t)n + 1);
    if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(b); return NULL; } b[n] = 0; fclose(f); if (len) *len = (size_t)n; return b; }
static int write_atomic(const char *path, const void *buf, size_t n) {
    char t[1300]; snprintf(t, sizeof t, "%s.tmp", path); FILE *f = fopen(t, "wb"); if (!f) return -1;
    if (fwrite(buf, 1, n, f) != n) { fclose(f); return -1; } if (fclose(f)) return -1; return rename(t, path);
}
static int same_file(const char *a, const char *b) { size_t la, lb; char *x = slurp(a, &la), *y = slurp(b, &lb); int r = x && y && la == lb && !memcmp(x, y, la); free(x); free(y); return r; }
static int copy_if_differs(const char *src, const char *dst) { if (same_file(src, dst)) return 0; size_t n; char *b = slurp(src, &n); if (!b) return 3; int r = write_atomic(dst, b, n) ? 5 : 0; free(b); return r; }

static struct { char key[NM]; int pat; } PO[MAXP]; static int npo; static struct { char l; int code; } DR[4]; static int ndr;
static int pose_of(const char *key) { for (int i = 0; i < npo; i++) if (!strcmp(PO[i].key, key)) return PO[i].pat; return -1; }
static int dir_code(char l) { for (int i = 0; i < ndr; i++) if (DR[i].l == l) return DR[i].code; return -1; }
static int load_pose(const char *p) { FILE *f = fopen(p, "r"); if (!f) return 3; char l[256], *x[6];
    while (fgets(l, sizeof l, f)) { int n = split(l, x, 6);
        if (n >= 3 && !strcmp(x[0], "POSE") && npo < MAXP) { snprintf(PO[npo].key, NM, "%s", x[1]); PO[npo++].pat = atoi(x[2]); }
        else if (n >= 3 && !strcmp(x[0], "DIR") && ndr < 4 && x[1][0]) { DR[ndr].l = x[1][0]; DR[ndr++].code = atoi(x[2]); } }
    fclose(f); return npo && ndr ? 0 : 4; }

typedef struct { char name[NM]; int has, x, y, z, sx, sy, sz, dir, ground, key; char last[16]; } Act;
static Act A[MAXA]; static int na;
static Act *act(const char *n) { for (int i = 0; i < na; i++) if (!strcmp(A[i].name, n)) return &A[i]; if (na >= MAXA) return NULL; memset(&A[na], 0, sizeof A[na]); snprintf(A[na].name, NM, "%s", n); A[na].dir = -1; return &A[na++]; }

/* the seed desk */
static int W, Hh, shift; static char tok[MAXD][MAXD][20]; static char glyph[MAXD][MAXD]; static int gh[256], gh_ok[256]; static char h2g[64]; static int h2g_ok[64], slot_of_name_n; static struct { char name[NM]; int slot; } SL[64];
static int slot_by_name(const char *n) { for (int i = 0; i < slot_of_name_n; i++) if (!strcmp(SL[i].name, n)) return SL[i].slot; return 0; }
/* BREAK FEEDBACK (owner 2026-11-25: "i didn't see breakage or empty space or collection of mini block"): the projection draws what the ledger says happened. A DAMAGE row leaves a crack:<1..3> event on that column until a MINE or PLACE
 * at it; the hole is the column one level lower (edit_col, already); the block that fell out is an item:<name> event at the mined cell, in the average colour of its atlas tile, for as long as the MINE is the LAST action; --idle
 * (the driver's expiry, the ledger has no clock) takes it away: it was collected into the bag. */
static struct { int x, z, prog; } DM[64]; static int ndm; static int lastact, lastx = -1, lastz = -1; static char lastdrop[NM];
static void dm_set(int x, int z, int prog) { for (int i = 0; i < ndm; i++) if (DM[i].x == x && DM[i].z == z) { DM[i].prog = prog; return; } if (ndm < 64) { DM[ndm].x = x; DM[ndm].z = z; DM[ndm++].prog = prog; } }
static void dm_clear(int x, int z) { for (int i = 0; i < ndm; i++) if (DM[i].x == x && DM[i].z == z) { DM[i] = DM[--ndm]; return; } }
static int tile_avg(const char *seed, int slot, int *r, int *g, int *b) {   /* mean colour of atlas tile <slot> (16x16, atlas_cols 16 = 256 px wide RGBA rows) */
    char p[1300]; snprintf(p, sizeof p, "%s/cells.rgba", seed); FILE *f = fopen(p, "rb"); if (!f) return 0; fseek(f, 0, SEEK_END); long sz = ftell(f); rewind(f); unsigned char *d = malloc((size_t)sz); if (!d) { fclose(f); return 0; }
    size_t got = fread(d, 1, (size_t)sz, f); fclose(f); long s = 0, rs = 0, gs = 0, bs = 0; int col = slot % 16, row = slot / 16;
    for (int py = 0; py < 16; py++) for (int px = 0; px < 16; px++) { long o = ((long)(row * 16 + py) * 256 + col * 16 + px) * 4; if ((size_t)(o + 3) >= got) continue; rs += d[o]; gs += d[o + 1]; bs += d[o + 2]; s++; }
    free(d); if (!s) return 0; *r = (int)(rs / s); *g = (int)(gs / s); *b = (int)(bs / s); return 1; }
static int load_heightmap_shift(const char *p) { FILE *f = fopen(p, "r"); if (!f) return 3; char l[512], *x[10]; int ok = 0; while (fgets(l, sizeof l, f)) { int n = split(l, x, 10); if (n >= 3 && !strcmp(x[0], "META") && !strcmp(x[1], "shift")) { shift = atoi(x[2]); ok = 1; break; } } fclose(f); return ok ? 0 : 4; }
static char *cells_head; static size_t cells_head_len; static char *seed_events;
static int load_seed(const char *dir) {
    char p[1300], l[1024], *x[8]; FILE *f; snprintf(p, sizeof p, "%s/legend.txt", dir); if (!(f = fopen(p, "r"))) return 3;
    while (fgets(l, sizeof l, f)) { int n = split(l, x, 8);
        if (n >= 4 && !strcmp(x[0], "GLYPH") && strlen(x[1]) == 1) { int h = atoi(x[2]); gh[(unsigned char)x[1][0]] = h; gh_ok[(unsigned char)x[1][0]] = 1; if (h >= 0 && h < 64) { h2g[h] = x[1][0]; h2g_ok[h] = 1; } }
        else if (n >= 3 && !strcmp(x[0], "SLOT") && isdigit((unsigned char)x[1][0]) && slot_of_name_n < 64) { snprintf(SL[slot_of_name_n].name, NM, "%s", x[2]); SL[slot_of_name_n++].slot = atoi(x[1]); } }
    fclose(f);
    snprintf(p, sizeof p, "%s/cells.txt", dir); size_t cl; char *ct = slurp(p, &cl); if (!ct) return 3; snprintf(p, sizeof p, "%s/map.txt", dir); size_t ml_; char *mp = slurp(p, &ml_); if (!mp) { free(ct); return 3; }
    char *hdr_end = strstr(ct, "\ncells\n"); if (!hdr_end) { free(ct); free(mp); return 4; } hdr_end += 7; cells_head_len = (size_t)(hdr_end - ct); cells_head = malloc(cells_head_len + 1); memcpy(cells_head, ct, cells_head_len); cells_head[cells_head_len] = 0;
    char *wp = strstr(ct, "width="), *hp = strstr(ct, "height="); if (!wp || !hp) { free(ct); free(mp); return 4; } W = atoi(wp + 6); Hh = atoi(hp + 7); if (W < 1 || Hh < 1 || W > MAXD || Hh > MAXD) { free(ct); free(mp); return 4; }
    char *s = hdr_end, *m = mp;
    for (int z = 0; z < Hh; z++) { char *me = strchr(m, '\n'); if (!me || (int)(me - m) < W) { free(ct); free(mp); return 4; } memcpy(glyph[z], m, (size_t)W); m = me + 1;
        for (int xx = 0; xx < W; xx++) { while (*s == ' ') s++; char *e = s; while (*e && *e != ' ' && *e != '\n') e++; if (e == s || e - s >= 20) { free(ct); free(mp); return 4; } memcpy(tok[z][xx], s, (size_t)(e - s)); tok[z][xx][e - s] = 0; s = e; }
        if (*s == '\n') s++; }
    free(ct); free(mp); snprintf(p, sizeof p, "%s/events.txt", dir); seed_events = slurp(p, NULL); return seed_events ? 0 : 3;
}
static void edit_col(int x, int z, int y, int mine, const char *item) {   /* NOT VERIFIED on the island (break and place are later): only the top block of a column */
    if (x < 0 || z < 0 || x >= W || z >= Hh || glyph[z][x] == '.' || !gh_ok[(unsigned char)glyph[z][x]]) return;
    int H = gh[(unsigned char)glyph[z][x]], top = H - 1 + shift; int fl, wl, dp; if (sscanf(tok[z][x], "%d,%d,%d", &fl, &wl, &dp) < 3) return;
    if (mine) { if (y != top || H - 1 < 0 || !h2g_ok[H - 1]) return; glyph[z][x] = h2g[H - 1]; snprintf(tok[z][x], 20, "%d,%d,%d", wl ? wl : fl, wl ? wl : fl, dp); }
    else { int sl = slot_by_name(item); if (y != top + 1 || !sl || H + 1 >= 64 || !h2g_ok[H + 1]) return; glyph[z][x] = h2g[H + 1]; snprintf(tok[z][x], 20, "%d,%d,%d", sl, sl, dp); }
}
static int fold_ledger(const char *p) {
    FILE *f = fopen(p, "r"); if (!f) return 3; char l[512], *x[10];
    while (fgets(l, sizeof l, f)) { int n = split(l, x, 10);
        if (n >= 4 && !strcmp(x[0], "DROP") && lastact == 1 && !lastdrop[0]) snprintf(lastdrop, sizeof lastdrop, "%s", x[2]);
        if (n >= 5 && !strcmp(x[0], "MOVE")) { Act *a = act(x[1]); if (!a) continue; lastact = 0; int nx = atoi(x[2]), ny = atoi(x[3]), nz = atoi(x[4]);
            if (!a->has) { a->sx = nx; a->sy = ny; a->sz = nz; } else { int dx = nx - a->x, dz = nz - a->z;
                if (dx || dz) { char d = abs(dx) >= abs(dz) ? (dx > 0 ? 'e' : 'w') : (dz > 0 ? 's' : 'n'); int c = dir_code(d); if (c >= 0) a->dir = c; }
                if (ny != a->y) snprintf(a->last, sizeof a->last, "MOVE_LEVEL"); else { a->ground++; snprintf(a->last, sizeof a->last, (a->ground & 1) ? "MOVE_ODD" : "MOVE_EVEN"); } }
            a->x = nx; a->y = ny; a->z = nz; a->has = 1; }
        else if (n >= 3 && !strcmp(x[0], "FACE")) { Act *a = act(x[1]); lastact = 0; if (a) { int c = dir_code(x[2][0]); if (c >= 0) a->dir = c; snprintf(a->last, sizeof a->last, "FACE"); } }
        else if (n >= 5 && !strcmp(x[0], "SPAWNPOINT")) { Act *a = act(x[1]); if (a) { a->sx = atoi(x[2]); a->sy = atoi(x[3]); a->sz = atoi(x[4]); } }
        else if (n >= 3 && !strcmp(x[0], "RESET")) { Act *a = act(x[1]); lastact = 0; if (a && a->has) { a->x = a->sx; a->y = a->sy; a->z = a->sz; snprintf(a->last, sizeof a->last, "RESET"); } }
        else if (n >= 8 && !strcmp(x[0], "DAMAGE")) { Act *a = act(x[1]); if (a) snprintf(a->last, sizeof a->last, "DAMAGE"); dm_set(atoi(x[3]), atoi(x[5]), atoi(x[7])); lastact = 0; }
        else if (n >= 6 && !strcmp(x[0], "MINE")) { Act *a = act(x[1]); if (a) snprintf(a->last, sizeof a->last, "MINE"); dm_clear(atoi(x[2]), atoi(x[4])); lastact = 1; lastx = atoi(x[2]); lastz = atoi(x[4]); lastdrop[0] = 0; edit_col(atoi(x[2]), atoi(x[4]), atoi(x[3]), 1, ""); }
        else if (n >= 6 && !strcmp(x[0], "PLACE")) { Act *a = act(x[1]); if (a) snprintf(a->last, sizeof a->last, "PLACE"); dm_clear(atoi(x[3]), atoi(x[5])); lastact = 0; edit_col(atoi(x[3]), atoi(x[5]), atoi(x[4]), 0, x[2]); } }
    fclose(f); return 0;
}
int main(int argc, char **argv) {
    int idle = 0, nomarker = 0, np = 0; const char *pa[8];
    for (int i = 1; i < argc; i++) { if (!strcmp(argv[i], "--idle")) idle = 1; else if (!strcmp(argv[i], "--no-marker")) nomarker = 1; else if (np < 8) pa[np++] = argv[i]; }
    if (np != 6 || strcmp(pa[0], "project")) { fprintf(stderr, "usage: rmmc_project_op project <seed_dir> <out_dir> <ledger> <pose.pdl> <terrain.heightmap> [--idle] [--no-marker]\n"); return 2; }
    const char *seed = pa[1], *out = pa[2]; int r;
    if ((r = load_pose(pa[4]))) { fprintf(stderr, "bad pose file\n"); return r; } if ((r = load_heightmap_shift(pa[5]))) { fprintf(stderr, "bad heightmap (no META shift)\n"); return r; }
    if ((r = load_seed(seed))) { fprintf(stderr, "bad seed desk %s\n", seed); return r; }
    if ((r = fold_ledger(pa[3]))) { fprintf(stderr, "cannot read ledger\n"); return r; }
    char p[1300], q[1300]; mkdir(out, 0775);
    { const char *sup[] = { "extrusion.pdl", "legend.txt", "cells.rgba", "skin.txt" }; for (int i = 0; i < 4; i++) { snprintf(p, sizeof p, "%s/%s", seed, sup[i]); snprintf(q, sizeof q, "%s/%s", out, sup[i]); if ((r = copy_if_differs(p, q))) return r; } }
    { DIR *d = opendir(seed); struct dirent *e; if (d) { while ((e = readdir(d))) if (!strncmp(e->d_name, "rig_", 4)) { snprintf(p, sizeof p, "%s/%s", seed, e->d_name); snprintf(q, sizeof q, "%s/%s", out, e->d_name); if ((r = copy_if_differs(p, q))) { closedir(d); return r; } } closedir(d); } }
    size_t cap = (size_t)W * Hh * 20 + cells_head_len + 64; char *cb = malloc(cap), *mb = malloc((size_t)(W + 1) * Hh + 1); if (!cb || !mb) return 5; size_t cn = 0, mn = 0;
    memcpy(cb, cells_head, cells_head_len); cn = cells_head_len;
    for (int z = 0; z < Hh; z++) { for (int xx = 0; xx < W; xx++) { if (xx) cb[cn++] = ' '; size_t k = strlen(tok[z][xx]); memcpy(cb + cn, tok[z][xx], k); cn += k; mb[mn++] = glyph[z][xx]; } cb[cn++] = '\n'; mb[mn++] = '\n'; }
    char *ev = malloc(strlen(seed_events) + 64 * MAXA + 100 * 66 + 64); size_t en = 0; int projected = 0, lastpat = -1;
    for (char *s = seed_events; *s; ) { char *e = strchr(s, '\n'); size_t len = e ? (size_t)(e - s) : strlen(s); char ln[512]; if (len >= sizeof ln) len = sizeof ln - 1; memcpy(ln, s, len); ln[len] = 0; s += len + (e ? 1 : 0); if (!ln[0]) continue;
        int ex, ey, cr, cg, cb2, fl, dir, pat; char nm[NM] = "";
        if (sscanf(ln, "%d %d %d %d %d %31s %d %d %d", &ex, &ey, &cr, &cg, &cb2, nm, &fl, &dir, &pat) == 9 && !strncmp(nm, "rig:", 4)) {
            Act *a = NULL; for (int i = 0; i < na; i++) if (!strcmp(A[i].name, nm + 4)) a = &A[i];
            if (a && a->has) { ex = a->x; ey = a->z; if (a->dir >= 0) dir = a->dir; int pt = pose_of(idle ? "IDLE" : (a->last[0] ? a->last : "FACE")); if (pt < 0) { fprintf(stderr, "pose.pdl has no row for %s\n", a->last); return 4; } pat = pt; projected++; lastpat = pat; }
            else if (idle) { int pt = pose_of("IDLE"); if (pt >= 0) pat = pt; }
            en += (size_t)snprintf(ev + en, 200, "%d %d %d %d %d %s %d %d %d\n", ex, ey, cr, cg, cb2, nm, fl, dir, pat); }
        else en += (size_t)snprintf(ev + en, 600, "%s\n", ln); }
    for (int i = 0; i < ndm; i++) { int st = DM[i].prog < 1 ? 1 : DM[i].prog > 3 ? 3 : DM[i].prog; en += (size_t)snprintf(ev + en, 100, "%d %d 34 26 22 crack:%d 0 2 0\n", DM[i].x, DM[i].z, st); }
    if (!idle && lastact == 1 && lastdrop[0] && lastx >= 0) { int sl = slot_by_name(lastdrop), cr = 130, cg = 100, cbb = 70; if (sl) tile_avg(seed, sl, &cr, &cg, &cbb); en += (size_t)snprintf(ev + en, 100, "%d %d %d %d %d item:%s 0 2 0\n", lastx, lastz, cr, cg, cbb, lastdrop); }
    snprintf(p, sizeof p, "%s/cells.txt", out); if (write_atomic(p, cb, cn)) return 5;
    snprintf(p, sizeof p, "%s/map.txt", out); if (write_atomic(p, mb, mn)) return 5;
    snprintf(p, sizeof p, "%s/events.txt", out); if (write_atomic(p, ev, en)) return 5;
    long msz = -1;
    if (!nomarker) { snprintf(p, sizeof p, "%s/cells.marker", out); struct stat st; long old = stat(p, &st) == 0 ? (long)st.st_size : 0; msz = old % 4000 + 1; char *mk = malloc((size_t)msz); memset(mk, '1', (size_t)msz); if (write_atomic(p, mk, (size_t)msz)) return 5; free(mk); }
    printf("projected actors=%d pattern=%d marker=%ld\n", projected, lastpat, msz); free(cb); free(mb); free(ev); return 0;
}
