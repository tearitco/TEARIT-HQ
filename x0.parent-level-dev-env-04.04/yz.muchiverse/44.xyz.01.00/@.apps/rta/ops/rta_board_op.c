/* rta_board_op - the picture of Robot Trap Auto: the pc-hq DESK is a projection of the ledger (D113).
 * The only facts are the life ledger (PART ENTER EXIT EAT SLEEP DRIVE TURN rows of rta_life_op), the rules file and the road grid.
 * Everything this op writes is derived and safe to delete: project again and the same bytes come back.
 *   rta_board_op project <config.pdl>
 *   rta_board_op --describe
 * config.pdl rows (`BOARD | key | value`): rules, grid, life (the ledger), desk (the desk folder), ui (the game_ui feed file), data (board.pdl), atlas, entity, car, png (optional: the raw_to_png op; with it the desk gets map.png, which the engine needs to read events.txt).
 * D120: a WALK row (rta_life_op walk) after the last EXIT puts the entity on that cell, the car stays put; STAND / BESIDE are read by rta_place.h (shared with rta_life_op).
 * data (board.pdl) rows: ATLAS | cols | tiles | px | rock   TILE | glyph | index   STAND | glyphs   BESIDE | dx | dy   SPRITE | person|car | charset | index | dir | pattern   COLOR | person|car | r | g | b
 * Where things stand (all derived, nothing is stored):
 *   the car   : the last DRIVE row x y, else the spawn (the first road cell of the south-most road row, the same rule as rta_momentum_op);
 *   the entity: in the car (ENTER without a later EXIT) it is on the car cell; out of the car it stands on the first free BESIDE cell of the car.
 * Writes: desk/cells.txt (only when its bytes change; then cells.marker grows by one byte, the house reload rule), map.txt, skin.txt, extrusion.pdl, cells.rgba (copied from the atlas once),
 *         desk/events.txt (car row, then entity row, one atomic write), the ui feed (gm_ keys: who, hunger, fuel, turn).
 * Exit: 0 ok, 2 usage, 3 a config key or file is missing, 4 grid unreadable, 5 no road, 6 cannot write.
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o rta_board_op.+x rta_board_op.c
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>

#include "rta_place.h"
#define MAXW PL_W
#define MAXH PL_H
#define P 1024

static PL plc;   /* grid + STAND + BESIDE, loaded by rta_place.h (shared with rta_life_op, D120) */
#define grid plc.g
#define gw plc.w
#define gh plc.h
static char cfg[16][2][P]; static int ncfg;
static int tile_of[256];
static int at_cols = 16, at_tiles = 43, at_px = 24, at_rock = 36;
static char spr[2][64]; static int sprI[2][3]; static int col[2][3];

static const char *cv(const char *k) { for (int i = 0; i < ncfg; i++) if (!strcmp(cfg[i][0], k)) return cfg[i][1]; return NULL; }

static void trim(char *s) { size_t n = strlen(s); while (n && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' ')) s[--n] = 0; }
static char *field(char *l, int n) {   /* the n-th ' | ' field, trimmed in place */
    for (int i = 0; i < n; i++) { l = strchr(l, '|'); if (!l) return NULL; l++; }
    while (*l == ' ') l++;
    char *e = strchr(l, '|'); if (e) *e = 0; trim(l); return l;
}

static int load_cfg(const char *path) {
    FILE *f = fopen(path, "r"); char l[P * 2]; if (!f) return -1;
    while (fgets(l, sizeof l, f) && ncfg < 16) {
        if (strncmp(l, "BOARD |", 7)) continue;
        char *b1 = strchr(l, '|'), *b2 = b1 ? strchr(b1 + 1, '|') : NULL; if (!b2) continue;
        *b2 = 0; char *k = b1 + 1; while (*k == ' ') k++; trim(k);
        char *v = b2 + 1; while (*v == ' ') v++; trim(v);
        snprintf(cfg[ncfg][0], P, "%s", k); snprintf(cfg[ncfg][1], P, "%s", v); ncfg++;
    }
    fclose(f); return 0;
}

static int load_data(const char *path) {
    FILE *f = fopen(path, "r"); char l[512]; if (!f) return -1;
    for (int i = 0; i < 256; i++) tile_of[i] = -1;
    while (fgets(l, sizeof l, f)) {
        char c[512]; snprintf(c, sizeof c, "%s", l);
        if (!strncmp(l, "ATLAS |", 7)) { sscanf(l, "ATLAS | %d | %d | %d | %d", &at_cols, &at_tiles, &at_px, &at_rock); }
        else if (!strncmp(l, "TILE |", 6)) { char *g = field(c, 1); char d[512]; snprintf(d, sizeof d, "%s", l); char *t = field(d, 2); if (g && t && g[0]) tile_of[(unsigned char)g[0]] = atoi(t); }
        else if (!strncmp(l, "SPRITE |", 8)) { char w[16], cs[64]; int a, b, d; if (sscanf(l, "SPRITE | %15s | %63s | %d | %d | %d", w, cs, &a, &b, &d) == 5) { int k = !strcmp(w, "car"); snprintf(spr[k], 64, "%s", cs); sprI[k][0] = a; sprI[k][1] = b; sprI[k][2] = d; } }
        else if (!strncmp(l, "COLOR |", 7)) { char w[16]; int r, g, b; if (sscanf(l, "COLOR | %15s | %d | %d | %d", w, &r, &g, &b) == 4) { int k = !strcmp(w, "car"); col[k][0] = r; col[k][1] = g; col[k][2] = b; } }
    }
    fclose(f); return 0;
}


static int rule(const char *path, const char *key, int *out) {
    FILE *f = fopen(path, "r"); char l[256], k[64]; int v; if (!f) return -1;
    while (fgets(l, sizeof l, f)) if (sscanf(l, "RULE | %63[^ |] | %d", k, &v) == 2 && !strcmp(k, key)) { *out = v; fclose(f); return 0; }
    fclose(f); return -1;
}

static int write_atomic(const char *path, const char *data, size_t n) {
    char tp[P + 8]; snprintf(tp, sizeof tp, "%s.tmp", path);
    FILE *f = fopen(tp, "wb"); if (!f) return -1;
    if (fwrite(data, 1, n, f) != n) { fclose(f); return -1; }
    fclose(f); return rename(tp, path);
}

static int same_file(const char *path, const char *data, size_t n) {
    FILE *f = fopen(path, "rb"); if (!f) return 0;
    char *b = malloc(n + 1); size_t k = b ? fread(b, 1, n + 1, f) : 0; fclose(f);
    int same = b && k == n && !memcmp(b, data, n); free(b); return same;
}


int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--describe")) {
        printf("rule rta_board inputs life ledger,rules,grid,board data outputs desk cells.txt map.txt events.txt ui feed numbers from board.pdl and RULE rows\n"); return 0;
    }
    if (argc != 3 || strcmp(argv[1], "project")) { fprintf(stderr, "usage: rta_board_op project <config.pdl>\n"); return 2; }
    if (load_cfg(argv[2])) { fprintf(stderr, "rta_board: cannot read the config\n"); return 3; }
    const char *k_rules = cv("rules"), *k_grid = cv("grid"), *k_life = cv("life"), *k_desk = cv("desk"), *k_ui = cv("ui"), *k_data = cv("data"), *k_atlas = cv("atlas"), *k_ent = cv("entity"), *k_car = cv("car"), *k_png = cv("png");
    if (!k_rules || !k_grid || !k_life || !k_desk || !k_ui || !k_data || !k_ent || !k_car) { fprintf(stderr, "rta_board: a config key is missing\n"); return 3; }
    if (load_data(k_data)) { fprintf(stderr, "rta_board: cannot read the board data\n"); return 3; }
    if (pl_load(&plc, k_grid, k_data)) { fprintf(stderr, "rta_board: grid unreadable\n"); return 4; }
    int cx, cy; if (pl_spawn(&plc, &cx, &cy)) { fprintf(stderr, "rta_board: no road\n"); return 5; }
    int hunger = 0, hmax = 100, fuel = 0, turn = 0, in_car = 0, walked = 0, wx = 0, wy = 0;
    rule(k_rules, "hunger_start", &hunger); rule(k_rules, "hunger_max", &hmax);
    FILE *lf = fopen(k_life, "r");
    if (lf) {
        char l[512], a[64], b[64], c[64], d[64]; int n, x, y;
        while (fgets(l, sizeof l, lf)) {
            if (sscanf(l, "PART | %63s | tank | %63s | %63s | %d", a, c, d, &n) == 4 && !strcmp(a, k_car)) fuel = n;
            else if (sscanf(l, "ENTER | %63s | %63s", a, b) == 2 && !strcmp(a, k_ent)) { in_car = 1; walked = 0; }
            else if (sscanf(l, "WALK | %63s | %d | %d", a, &x, &y) == 3 && !strcmp(a, k_ent)) { walked = 1; wx = x; wy = y; }
            else if (sscanf(l, "EXIT | %63s | %63s", a, b) == 2 && !strcmp(a, k_ent)) { in_car = 0; walked = 0; }
            else if (sscanf(l, "EAT | %63s | %d", a, &n) == 2 && !strcmp(a, k_ent)) hunger = n;
            else if (sscanf(l, "SLEEP | %63s | %d | %d", a, &x, &n) == 3 && !strcmp(a, k_ent)) hunger = n;
            else if (sscanf(l, "DRIVE | %63s | %63s | %d | %d | %d", a, b, &x, &y, &n) == 5 && !strcmp(a, k_ent)) { cx = x; cy = y; fuel = n; }
            else if (sscanf(l, "TURN | %d", &n) == 1) turn = n;
        }
        fclose(lf);
    }
    int ex = cx, ey = cy;
    if (!in_car) { if (walked) { ex = wx; ey = wy; } else pl_beside(&plc, cx, cy, &ex, &ey); }

    static char buf[MAXW * MAXH * 12 + 512]; size_t w = 0;
    w += (size_t)snprintf(buf + w, sizeof buf - w, "width=%d\nheight=%d\ntile_px=%d\natlas_cols=%d\natlas_tiles=%d\nrock_slot=%d\ncells\n", gw, gh, at_px, at_cols, at_tiles, at_rock);
    for (int y = 0; y < gh; y++) { for (int x = 0; x < gw; x++) { int t = tile_of[(unsigned char)grid[y][x]]; if (t < 0) t = 0; w += (size_t)snprintf(buf + w, sizeof buf - w, "%s%d,0", x ? " " : "", t); } w += (size_t)snprintf(buf + w, sizeof buf - w, "\n"); }
    char q[P + 32]; struct stat sb; mkdir(k_desk, 0775);
    snprintf(q, sizeof q, "%s/cells.txt", k_desk);
    int changed = !same_file(q, buf, w);
    if (changed) {
        if (write_atomic(q, buf, w)) return 6;
        char mk[P + 32]; snprintf(mk, sizeof mk, "%s/cells.marker", k_desk); struct stat ms;
        FILE *mf = fopen(mk, (!stat(mk, &ms) && ms.st_size >= 4096) ? "w" : "a"); if (mf) { fputc('1', mf); fclose(mf); }   /* the house marker rule: the engine reloads the desk when this file GROWS */
    }
    w = 0; for (int y = 0; y < gh; y++) { for (int x = 0; x < gw; x++) buf[w++] = '.'; buf[w++] = '\n'; }
    snprintf(q, sizeof q, "%s/map.txt", k_desk); if (!same_file(q, buf, w) && write_atomic(q, buf, w)) return 6;
    snprintf(q, sizeof q, "%s/skin.txt", k_desk); if (stat(q, &sb)) write_atomic(q, "rmmv\n", 5);
    snprintf(q, sizeof q, "%s/extrusion.pdl", k_desk);
    if (stat(q, &sb)) { const char *e = "SECTION      | KEY                | VALUE\n----------------------------------------------------------------------\nMETA         | map_id             | rta-board\nMETA         | desk_id            | board\nMETA         | note               | projection of the rta ledgers (rta_board_op)\n"; write_atomic(q, e, strlen(e)); }
    snprintf(q, sizeof q, "%s/cells.rgba", k_desk);
    if (k_atlas && stat(q, &sb)) { FILE *a = fopen(k_atlas, "rb"); if (!a) { fprintf(stderr, "rta_board: cannot read the atlas\n"); return 3; } FILE *o = fopen(q, "wb"); if (!o) { fclose(a); return 6; } char b[8192]; size_t n; while ((n = fread(b, 1, sizeof b, a)) > 0) fwrite(b, 1, n, o); fclose(a); fclose(o); }

    /* map.png: the engine reads events.txt only BESIDE a map.png, so the desk has the 2D picture too: each cell's atlas tile blitted 1:1 (house op raw_to_png, same as game_core desk_picture and vox_op) */
    snprintf(q, sizeof q, "%s/map.png", k_desk);
    if (k_png && k_atlas && (changed || stat(q, &sb))) {
        FILE *af = fopen(k_atlas, "rb"); if (!af) return 3;
        size_t atw = (size_t)at_cols * at_px, asz = 0; fseek(af, 0, SEEK_END); asz = (size_t)ftell(af); rewind(af);
        unsigned char *at = malloc(asz); if (!at || fread(at, 1, asz, af) != asz) { fclose(af); free(at); return 3; } fclose(af);
        size_t ow = (size_t)gw * at_px, oh = (size_t)gh * at_px; unsigned char *o = malloc(ow * oh * 4); if (!o) return 6;
        for (int y = 0; y < gh; y++) for (int x = 0; x < gw; x++) { int t = tile_of[(unsigned char)grid[y][x]]; if (t < 0) t = 0;
            for (int py = 0; py < at_px; py++) for (int px = 0; px < at_px; px++) {
                unsigned char *d = &o[(((size_t)y * at_px + py) * ow + (size_t)x * at_px + px) * 4];
                const unsigned char *a = &at[(((size_t)(t / at_cols) * at_px + py) * atw + (size_t)(t % at_cols) * at_px + px) * 4];
                if (t && a[3]) { d[0] = a[0]; d[1] = a[1]; d[2] = a[2]; } else { d[0] = 24; d[1] = 24; d[2] = 32; } d[3] = 255; } }
        char raw[P + 32], sz[40]; snprintf(raw, sizeof raw, "%s/map.raw", k_desk); snprintf(sz, sizeof sz, "%zux%zu", ow, oh);
        int bad = write_atomic(raw, (const char *)o, ow * oh * 4); free(o); free(at);
        if (!bad) { fflush(NULL); pid_t p = fork(); if (!p) { if (!freopen("/dev/null", "w", stdout) || !freopen("/dev/null", "w", stderr)) _exit(126); execl(k_png, k_png, raw, q, "--size", sz, (char *)NULL); _exit(127); }
            int st = 0; if (p > 0) waitpid(p, &st, 0); if (!(WIFEXITED(st) && !WEXITSTATUS(st))) bad = 1; }
        unlink(raw); if (bad) { fprintf(stderr, "rta_board: cannot write map.png\n"); return 6; }
    }
    /* events.txt: x y r g b charset index dir pattern (the car, then the entity on top of it) */
    char ev[512]; size_t en = (size_t)snprintf(ev, sizeof ev, "%d %d %d %d %d %s %d %d %d\n%d %d %d %d %d %s %d %d %d\n", cx, cy, col[1][0], col[1][1], col[1][2], spr[1], sprI[1][0], sprI[1][1], sprI[1][2],
        ex, ey, col[0][0], col[0][1], col[0][2], spr[0], sprI[0][0], sprI[0][1], sprI[0][2]);
    snprintf(q, sizeof q, "%s/events.txt", k_desk); if (write_atomic(q, ev, en)) return 6;

    char leg[900] = ""; int nleg = 0;   /* D116: key legend lines are DATA, LEGEND rows of keys.pdl beside the data file; none, no extra line */
    { char kp[1100], kl[400]; snprintf(kp, sizeof kp, "%s", k_data); char *sl = strrchr(kp, '/'); if (sl) { snprintf(sl, sizeof kp - (size_t)(sl - kp), "/keys.pdl"); FILE *kf = fopen(kp, "r"); if (kf) { while (fgets(kl, sizeof kl, kf)) if (!strncmp(kl, "LEGEND | ", 9) && nleg < 2) { kl[strcspn(kl, "\r\n")] = 0; size_t ll = strlen(leg); snprintf(leg + ll, sizeof leg - ll, "gmt_%d_text=%s\n", 3 + nleg, kl + 9); nleg++; } fclose(kf); } } }
    char ui[2048]; size_t un = (size_t)snprintf(ui, sizeof ui, "gm_on=1\ngm_title=Robot Trap Auto\ngmt_0_text=%s: %s (car at %d,%d)\ngmt_1_text=hunger %d of %d\ngmt_2_text=fuel %d  turn %d\n%sgm_nt=%d\ngm_nb=0\ngm_ne=0\ngm_nl=0\n",
        k_ent, in_car ? "in the car" : "on foot", cx, cy, hunger, hmax, fuel, turn, leg, 3 + nleg);
    if (write_atomic(k_ui, ui, un)) return 6;
    printf("car=%d,%d entity=%d,%d in_car=%d hunger=%d fuel=%d turn=%d\n", cx, cy, ex, ey, in_car, hunger, fuel, turn);
    return 0;
}
