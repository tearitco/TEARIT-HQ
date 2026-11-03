/* pal_side - the editor's left side bar switch: pets <-> palettes (BOOK RPG-EDIT-HQ-PALETTES-SIDEBAR-RESEARCH-2026-11-02.md).
 * Usage: pal_side <house> <verb> [arg]
 *   open        show the palettes bar (hides the pets bar: they share the left slot), list view
 *   pets        show the pets bar (hides the palettes bar)
 *   none        hide both (the bars' "_" buttons)
 *   tiles <key> palettes bar, tile view of one category (key = a CATEGORY row of &.widgits/palettes/pallets.pdl)
 *   back        palettes bar, back to the category list
 *   arm <i>     arm tile i of the open category as the editor's brush: board/state/brush.txt (key, glyph, sprite) + pal_armed in pal.txt; a piececraft tile also goes
 *               through palettes_menu.sh arm-pc (the house's own brush file, palettes/state/pc_brush.txt)
 *   list        only rewrite pal.txt (category list for the window)
 * Files (board/state/): sides.txt (side_left / side_pal: "1" = shown, EMPTY = hidden, other keys kept; same convention as layout-studio side_toggle) and pal.txt
 * (what the window binds: n_pcat, pcat_<i>_key/_label, pal_is_list, pal_is_tiles, pal_key, pal_title). Both written atomically. A state-key switch, not an href stack (owner OK 2026-11-02). */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/wait.h>
#define MAXC 40
static char KEY[MAXC][64], LAB[MAXC][96]; static int nc;
static void trim(char *s) { size_t n = strlen(s); while (n && (s[n-1]==' '||s[n-1]=='\n'||s[n-1]=='\r'||s[n-1]=='\t')) s[--n]=0; char *p=s; while (*p==' '||*p=='\t') p++; if (p!=s) memmove(s,p,strlen(p)+1); }
static void load_cats(const char *house) {
    char p[4200]; snprintf(p, sizeof p, "%s/&.widgits/palettes/pallets.pdl", house);
    FILE *f = fopen(p, "r"); if (!f) return; char ln[512];
    while (nc < MAXC && fgets(ln, sizeof ln, f)) {
        if (strncmp(ln, "CATEGORY", 8)) continue;
        char *fld[4]; int k = 0; char *c = ln;
        while (k < 4) { fld[k++] = c; char *b = strchr(c, '|'); if (!b) break; *b = 0; c = b + 1; }
        if (k < 3) continue;
        trim(fld[1]); trim(fld[2]);
        if (!fld[1][0] || !fld[2][0]) continue;
        snprintf(KEY[nc], sizeof KEY[0], "%s", fld[1]); snprintf(LAB[nc], sizeof LAB[0], "%s", fld[2]); nc++;
    }
    fclose(f);
}
/* set key=value ("" = hidden) in a kv file, keeping every other line */
static void kv_set(const char *path, const char *key, const char *val) {
    static char L[640][256]; int n = 0, at = -1; size_t kl = strlen(key); FILE *f = fopen(path, "r");
    if (f) { while (n < 640 && fgets(L[n], sizeof L[0], f)) { if (!strncmp(L[n], key, kl) && L[n][kl] == '=') at = n; n++; } fclose(f); }
    char row[256]; snprintf(row, sizeof row, "%s=%s\n", key, val);
    if (at >= 0) memcpy(L[at], row, strlen(row) + 1); else if (n < 640) { memcpy(L[n], row, strlen(row) + 1); n++; }
    char tmp[4300]; snprintf(tmp, sizeof tmp, "%s.tmp", path); f = fopen(tmp, "w"); if (!f) return;
    for (int i = 0; i < n; i++) { fputs(L[i], f); }
    fclose(f); rename(tmp, path);
}
#define MAXT 48
static char TG[MAXT][64], TS[MAXT][1024]; static int nt;
/* the category's tiles: the file its palette window's projector publishes (palettes/state/palettes-<key>_ui.txt: t_<i>_glyph / t_<i>_sprite, the first tileset). Missing file: run the
 * projector ONCE, niced, for about 2 s (it is a polling daemon in the taskbar windows; here it is bounded and gone again), then read what it wrote. */
static void load_tiles(const char *house, const char *key) {
    char p[4200]; snprintf(p, sizeof p, "%s/&.widgits/palettes/state/palettes-%s_ui.txt", house, key);
    if (access(p, R_OK) != 0) {
        char proj[4200], pkg[4200]; snprintf(proj, sizeof proj, "%s/&.widgits/palettes/ops/+x/palettes_projector.+x", house); snprintf(pkg, sizeof pkg, "%s/&.widgits/palettes", house);
        if (access(proj, X_OK) == 0) {
            pid_t c = fork();
            if (c == 0) { execlp("timeout", "timeout", "2", "nice", "-n", "15", proj, house, pkg, key, (char *)NULL); _exit(127); }
            if (c > 0) { int st; waitpid(c, &st, 0); }
        }
    }
    FILE *f = fopen(p, "r"); if (!f) return; static char ln[2048];
    while (fgets(ln, sizeof ln, f)) {
        int i; char *eq = strchr(ln, '='); if (!eq) continue; *eq = 0; char *v = eq + 1; v[strcspn(v, "\r\n")] = 0;
        if (sscanf(ln, "t_%d_glyph", &i) == 1 && !strstr(ln, "sprite") && i >= 0 && i < MAXT) { snprintf(TG[i], sizeof TG[0], "%.14s", v); if (i + 1 > nt) nt = i + 1; }
        else if (sscanf(ln, "t_%d_sprite", &i) == 1 && i >= 0 && i < MAXT) { snprintf(TS[i], sizeof TS[0], "%s", v); if (i + 1 > nt) nt = i + 1; }
    }
    fclose(f);
}
static void kv_get(const char *path, const char *key, char *out, size_t n) {
    out[0] = 0; FILE *f = fopen(path, "r"); if (!f) return; static char ln[2048]; size_t kl = strlen(key);
    while (fgets(ln, sizeof ln, f)) if (!strncmp(ln, key, kl) && ln[kl] == '=') { snprintf(out, n, "%s", ln + kl + 1); out[strcspn(out, "\r\n")] = 0; }
    fclose(f);
}
/* Sidebar layout history + start preference (owner 2026-11-03: "set in a .pdl history like the other thing, changeable by preference").
 * state/sides_history.pdl: append-only rows `SIDES | <epoch> | pets|palettes|none`, one per change (capped: past 8 KB the last 20 rows are kept; read by identity, last row wins).
 * Start layout, applied by `pal_side <house> restore` when the window opens: state/prefs.txt (set from the editor's Settings tab) `sidebar_start=last|pets|palettes|none`; default `last`; with no history yet, SIDE_FIRST_RUN. */
#define SIDE_FIRST_RUN "palettes"
static void hist_path(const char *house, char *o, size_t n) { snprintf(o, n, "%s/@.apps/rpg-edit-hq/state/sides_history.pdl", house); }
static void hist_add(const char *house, const char *mode) {
    char p[4200]; hist_path(house, p, sizeof p); struct stat st;
    if (stat(p, &st) == 0 && st.st_size > 8192) { static char rows[64][64]; int nr = 0; FILE *r = fopen(p, "r"); if (r) { static char ln[256]; while (fgets(ln, sizeof ln, r)) { snprintf(rows[nr % 64], 64, "%s", ln); nr++; } fclose(r); }
        FILE *w = fopen(p, "w"); if (w) { int from = nr > 20 ? nr - 20 : 0; for (int i = from; i < nr; i++) fputs(rows[i % 64], w); fclose(w); } }
    FILE *f = fopen(p, "a"); if (f) { fprintf(f, "SIDES | %ld | %s\n", (long)time(NULL), mode); fclose(f); }
}
static void hist_last(const char *house, char *o, size_t n) {
    char p[4200]; hist_path(house, p, sizeof p); o[0] = 0; FILE *f = fopen(p, "r"); if (!f) return; static char ln[256];
    while (fgets(ln, sizeof ln, f)) { char *b = strrchr(ln, '|'); if (b && !strncmp(ln, "SIDES", 5)) { b++; while (*b == ' ') b++; b[strcspn(b, "\r\n ")] = 0; snprintf(o, n, "%s", b); } }
    fclose(f);
}
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: pal_side <house> open|pets|none|tiles <key>|back|list\n"); return 2; }
    const char *house = argv[1], *verb = argv[2], *arg = argc > 3 ? argv[3] : "";
    char sides[4200], pal[4200]; snprintf(sides, sizeof sides, "%s/@.apps/rpg-edit-hq/board/state/sides.txt", house); snprintf(pal, sizeof pal, "%s/@.apps/rpg-edit-hq/board/state/pal.txt", house);
    load_cats(house);
    int tiles = 0, log_hist = 1; char key[64] = "", title[96] = "";
    if (!strcmp(verb, "restore")) {   /* the window just opened: apply the start layout (preference, else the last one, else the first-run default) without logging a new row */
        char pr[4200], want[32], last[32]; snprintf(pr, sizeof pr, "%s/@.apps/rpg-edit-hq/state/prefs.txt", house); kv_get(pr, "sidebar_start", want, sizeof want); hist_last(house, last, sizeof last);
        const char *m = (!strcmp(want, "pets") || !strcmp(want, "palettes") || !strcmp(want, "none")) ? want : (last[0] ? last : SIDE_FIRST_RUN);
        static char mv[16]; snprintf(mv, sizeof mv, "%s", !strcmp(m, "palettes") ? "open" : !strcmp(m, "pets") ? "pets" : !strcmp(m, "none") ? "none" : "open"); verb = mv; log_hist = 0;
    }
    if (!strcmp(verb, "open")) { if (log_hist) hist_add(house, "palettes"); kv_set(sides, "side_pal", "1"); kv_set(sides, "side_left", ""); }
    else if (!strcmp(verb, "pets")) { if (log_hist) hist_add(house, "pets"); kv_set(sides, "side_left", "1"); kv_set(sides, "side_pal", ""); }
    else if (!strcmp(verb, "none")) { if (log_hist) hist_add(house, "none"); kv_set(sides, "side_left", ""); kv_set(sides, "side_pal", ""); }
    else if (!strcmp(verb, "tiles")) { if (log_hist) hist_add(house, "palettes");
        int at = -1; for (int i = 0; i < nc; i++) if (!strcmp(KEY[i], arg)) at = i;
        if (at < 0) { fprintf(stderr, "pal_side: no such category %s\n", arg); return 1; }
        tiles = 1; snprintf(key, sizeof key, "%s", KEY[at]); snprintf(title, sizeof title, "%s", LAB[at]);
        kv_set(sides, "side_pal", "1"); kv_set(sides, "side_left", "");
        load_tiles(house, key);
    } else if (!strcmp(verb, "arm")) {
        int i = atoi(arg); char g[64], sp[1024], k[64], pm[4200];
        char kk[64]; snprintf(kk, sizeof kk, "ptile_%d_glyph", i); kv_get(pal, kk, g, sizeof g); snprintf(kk, sizeof kk, "ptile_%d_sprite", i); kv_get(pal, kk, sp, sizeof sp); kv_get(pal, "pal_key", k, sizeof k);
        if (!g[0] && !sp[0]) { fprintf(stderr, "pal_side: no tile %d\n", i); return 1; }
        char bp[4200], bt[4300]; snprintf(bp, sizeof bp, "%s/@.apps/rpg-edit-hq/board/state/brush.txt", house); snprintf(bt, sizeof bt, "%s.tmp", bp);
        FILE *o = fopen(bt, "w"); if (!o) return 1; fprintf(o, "key=%s\nglyph=%s\nsprite=%s\n", k, g, sp); fclose(o); rename(bt, bp);
        kv_set(pal, "pal_armed", g[0] ? g : "tile");
        {   /* engine side: brush mode + the green placer armed at the selector (the Move placer's shape; Enter then places, Esc leaves) */
            char ui[4200], sess[4200], xp[4300], xb[512] = "", bm[4400], pp[4400]; snprintf(ui, sizeof ui, "%s/@.apps/rpg-edit-hq/board/state/ui.txt", house); kv_get(ui, "bv_session", sess, sizeof sess);
            if (sess[0] && access(sess, F_OK) == 0) {
                int px = 8, py = 8, pz = 0; snprintf(xp, sizeof xp, "%s/@.apps/rpg-edit-hq/rpg-edit-hq-pchq/pieces/xelector_01/state.txt", house);
                char v[32]; kv_get(xp, "pos_x", v, sizeof v); if (v[0]) px = atoi(v); kv_get(xp, "pos_y", v, sizeof v); if (v[0]) py = atoi(v); kv_get(xp, "pos_z", v, sizeof v); if (v[0]) pz = atoi(v); (void)xb;
                snprintf(bm, sizeof bm, "%s/pieces/display/brush_mode.txt", sess); FILE *bf = fopen(bm, "w"); if (bf) { fprintf(bf, "glyph=%s\n", g); fclose(bf); }
                snprintf(pp, sizeof pp, "%s/pieces/display/placer.txt", sess); char nb[16]; snprintf(nb, sizeof nb, "%d", px); kv_set(pp, "x", nb); snprintf(nb, sizeof nb, "%d", py); kv_set(pp, "y", nb); snprintf(nb, sizeof nb, "%d", pz); kv_set(pp, "z", nb); kv_set(pp, "armed", "1");
            } }
        if (!strcmp(k, "piececraft")) { snprintf(pm, sizeof pm, "%s/&.widgits/palettes/palettes_menu.sh", house); pid_t c = fork(); if (c == 0) { execlp("sh", "sh", pm, "arm-pc", sp, g, (char *)NULL); _exit(127); } if (c > 0) { int st; waitpid(c, &st, 0); } }
        return 0;
    } else if (strcmp(verb, "back") && strcmp(verb, "list")) { fprintf(stderr, "pal_side: unknown verb %s\n", verb); return 2; }
    char tmp[4300]; snprintf(tmp, sizeof tmp, "%s.tmp", pal); FILE *o = fopen(tmp, "w"); if (!o) return 1;
    fprintf(o, "n_pcat=%d\npal_is_list=%s\npal_is_tiles=%s\npal_key=%s\npal_title=%s\n", nc, tiles ? "" : "1", tiles ? "1" : "", key, title[0] ? title : "palettes");
    for (int i = 0; i < nc; i++) fprintf(o, "pcat_%d_key=%s\npcat_%d_label=%s\n", i, KEY[i], i, LAB[i]);
    fprintf(o, "n_ptile=%d\npal_empty=%s\n", nt, (tiles && nt == 0) ? "1" : "");
    for (int i = 0; i < nt; i++) fprintf(o, "ptile_%d_glyph=%s\nptile_%d_sprite=%s\n", i, TG[i], i, TS[i]);
    fclose(o); rename(tmp, pal); return 0;
}
