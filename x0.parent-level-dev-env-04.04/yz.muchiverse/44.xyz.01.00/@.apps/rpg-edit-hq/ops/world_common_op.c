#define _GNU_SOURCE
/* world_common_op - early-survivor step 4 (16.game/EARLY-SURVIVOR-AND-COMMON-EVENTS.md "Common events"), QUARANTINED in the editor's own book (2026-11-04). ONE cell changes per call; no item is ever written (water in a container is a number on the container row, not an item).
 *   world_common_op <book_dir> flow            one FLUID step: the fullest row (first on a tie) gives 1 volume to its first neighbour (N,E,S,W) that is inside the page, not solid, and EMPTY or at least 2 volume LOWER. Refused (and nothing written) if the move would change the volume sum.
 *                                              Lava landing on a furniture cell lights it (FIRE fuel 3). Water landing on a FIRE cell deletes that fire row (the water stays).
 *   world_common_op <book_dir> rain on|off    writes state/rain.txt
 *   world_common_op <book_dir> rain            one rain tick, only if rain is on: every container +1 water (up to its cap), every FIRE not under a roof loses 1 fuel (deleted at 0), every dirt cell gets a wet row. Off does not dry anything.
 *   world_common_op <book_dir> fire            one fire tick: every FIRE loses 1 fuel; if fuel remains it may light ONE neighbouring furniture cell (first N,E,S,W without fire; new fire fuel 3); at 0 the row is deleted
 *   world_common_op <book_dir> status          counts
 * Files in <book_dir>/state (all optional, one row per line): fluids.txt `x|y|water or lava|volume`; fire.txt `x|y|fuel` (survivor_op's light writes the same file); placed.txt `id|x|y|hp` (furniture = fuel, read only); solid.txt `x|y`; roofed.txt `x|y`; containers.txt `x|y|water|cap`; dirt.txt `x|y`; wet.txt `x|y` (written); rain.txt `on` or `off`; events.txt (log, capped 64 KB then trimmed to the last 32 KB).
 * Page bounds 0..63 x 0..63. Runs only in a folder that holds QUARANTINE (exit 4). Exit 0 ok, 2 usage, 3 refused. COST: one short process, files of a few hundred bytes. Not wired to the clock yet (the clock would run it as common:flow / rain / fire events). Single room: the design's room column is not used in the quarantined book. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <time.h>
#include <sys/stat.h>
#define MAXR 400
#define NF 6
typedef struct { char f[NF][24]; int n; } Row;
static char BOOK[PATH_MAX];
static void P(char *o, const char *rel) { snprintf(o, PATH_MAX, "%s/state/%s", BOOK, rel); }
static int load(const char *rel, Row *r, int max) { char p[PATH_MAX], ln[200]; P(p, rel); FILE *f = fopen(p, "r"); if (!f) return 0; int n = 0; while (n < max && fgets(ln, sizeof ln, f)) { if (ln[0] == '#') continue; ln[strcspn(ln, "\r\n")] = 0; if (!ln[0]) continue; Row *q = &r[n]; q->n = 0; char *sv = NULL; for (char *t = strtok_r(ln, "|", &sv); t && q->n < NF; t = strtok_r(NULL, "|", &sv)) snprintf(q->f[q->n++], 24, "%s", t); n++; } fclose(f); return n; }
static void save(const char *rel, Row *r, int n) { char p[PATH_MAX], t[PATH_MAX]; P(p, rel); snprintf(t, sizeof t, "%s.tmp", p); FILE *f = fopen(t, "w"); if (!f) return; for (int i = 0; i < n; i++) { for (int k = 0; k < r[i].n; k++) fprintf(f, "%s%s", k ? "|" : "", r[i].f[k]); fputc('\n', f); } fclose(f); rename(t, p); }
static void logev(const char *what) { char p[PATH_MAX]; P(p, "events.txt"); struct stat st; if (stat(p, &st) == 0 && st.st_size > 65536) { FILE *f = fopen(p, "r"); if (f) { static char b[32768]; fseek(f, -32768, SEEK_END); size_t n = fread(b, 1, sizeof b, f); fclose(f); char *nl = memchr(b, '\n', n); f = fopen(p, "w"); if (f) { if (nl) fwrite(nl + 1, 1, n - (size_t)(nl + 1 - b), f); fclose(f); } } } FILE *f = fopen(p, "a"); if (f) { fprintf(f, "%ld|common|%s\n", (long)time(NULL), what); fclose(f); } }
static int has(const char *rel, int x, int y) { static Row r[MAXR]; int n = load(rel, r, MAXR); for (int i = 0; i < n; i++) if (r[i].n >= 2 && atoi(r[i].f[0]) == x && atoi(r[i].f[1]) == y) return 1; return 0; }
static int furn_at(int x, int y) { static Row r[MAXR]; int n = load("placed.txt", r, MAXR); for (int i = 0; i < n; i++) if (r[i].n >= 3 && atoi(r[i].f[1]) == x && atoi(r[i].f[2]) == y) return 1; return 0; }
static const int DX[4] = {0, 1, 0, -1}, DY[4] = {-1, 0, 1, 0};
static int inpage(int x, int y) { return x >= 0 && y >= 0 && x < 64 && y < 64; }
static int fire_idx(Row *fr, int n, int x, int y) { for (int i = 0; i < n; i++) if (fr[i].n >= 3 && atoi(fr[i].f[0]) == x && atoi(fr[i].f[1]) == y) return i; return -1; }
static void fire_add(Row *fr, int *n, int x, int y, int fuel) { if (*n >= MAXR || fire_idx(fr, *n, x, y) >= 0) return; Row *q = &fr[(*n)++]; q->n = 3; snprintf(q->f[0], 24, "%d", x); snprintf(q->f[1], 24, "%d", y); snprintf(q->f[2], 24, "%d", fuel); }
static void fire_del(Row *fr, int *n, int i) { for (int k = i; k < *n - 1; k++) fr[k] = fr[k + 1]; (*n)--; }
static long sumvol(Row *r, int n) { long s = 0; for (int i = 0; i < n; i++) if (r[i].n >= 4) s += atol(r[i].f[3]); return s; }

static int do_flow(void) {
    static Row fl[MAXR], fr[MAXR]; int n = load("fluids.txt", fl, MAXR), nf = load("fire.txt", fr, MAXR); long before = sumvol(fl, n); int best = -1;
    for (int i = 0; i < n; i++) if (fl[i].n >= 4 && atol(fl[i].f[3]) > 0 && (best < 0 || atol(fl[i].f[3]) > atol(fl[best].f[3]))) best = i;
    if (best < 0) { printf("NOTHING: no fluid with volume\n"); return 0; }
    int x = atoi(fl[best].f[0]), y = atoi(fl[best].f[1]); long v = atol(fl[best].f[3]);
    for (int d = 0; d < 4; d++) { int nx = x + DX[d], ny = y + DY[d]; if (!inpage(nx, ny) || has("solid.txt", nx, ny)) continue; int j = -1; for (int i = 0; i < n; i++) if (atoi(fl[i].f[0]) == nx && atoi(fl[i].f[1]) == ny) j = i;
        if (j >= 0 && strcmp(fl[j].f[2], fl[best].f[2])) continue;      /* water and lava never mix in this slice */
        if (j >= 0 && atol(fl[j].f[3]) > v - 2) continue;
        if (j < 0) { if (n >= MAXR) continue; j = n++; fl[j].n = 4; snprintf(fl[j].f[0], 24, "%d", nx); snprintf(fl[j].f[1], 24, "%d", ny); snprintf(fl[j].f[2], 24, "%s", fl[best].f[2]); snprintf(fl[j].f[3], 24, "0"); }
        snprintf(fl[best].f[3], 24, "%ld", v - 1); snprintf(fl[j].f[3], 24, "%ld", atol(fl[j].f[3]) + 1);
        if (sumvol(fl, n) != before) { printf("REFUSED: the move would change the volume sum\n"); return 3; }
        char note[120] = ""; if (!strcmp(fl[j].f[2], "lava") && furn_at(nx, ny)) { fire_add(fr, &nf, nx, ny, 3); snprintf(note, sizeof note, " lava lit the furniture at %d,%d", nx, ny); }
        if (!strcmp(fl[j].f[2], "water")) { int fi = fire_idx(fr, nf, nx, ny); if (fi >= 0) { fire_del(fr, &nf, fi); snprintf(note, sizeof note, " water put out the fire at %d,%d", nx, ny); } }
        int w = 0; for (int i = 0; i < n; i++) if (atol(fl[i].f[3]) > 0) fl[w++] = fl[i]; save("fluids.txt", fl, w); save("fire.txt", fr, nf);
        char lg[200]; snprintf(lg, sizeof lg, "flow|%s %d,%d -> %d,%d%s", fl[j].f[2], x, y, nx, ny, note); logev(lg); printf("FLOW %s 1 from %d,%d to %d,%d (sum %ld)%s\n", fl[j].f[2], x, y, nx, ny, before, note); return 0; }
    printf("NOTHING: the fullest cell %d,%d (volume %ld) has no open or lower neighbour\n", x, y, v); return 0;
}
static int do_fire(void) {
    static Row fr[MAXR]; int n = load("fire.txt", fr, MAXR); if (!n) { printf("NOTHING: no fire\n"); return 0; } int lit = 0, out = 0; int orig = n;
    for (int i = 0; i < orig; i++) { long fuel = atol(fr[i].f[2]) - 1; snprintf(fr[i].f[2], 24, "%ld", fuel); if (fuel > 0) { int x = atoi(fr[i].f[0]), y = atoi(fr[i].f[1]); for (int d = 0; d < 4; d++) { int nx = x + DX[d], ny = y + DY[d]; if (inpage(nx, ny) && furn_at(nx, ny) && fire_idx(fr, n, nx, ny) < 0) { fire_add(fr, &n, nx, ny, 3); lit++; break; } } } }
    int w = 0; for (int i = 0; i < n; i++) { if (atol(fr[i].f[2]) > 0) fr[w++] = fr[i]; else out++; } save("fire.txt", fr, w);
    char lg[100]; snprintf(lg, sizeof lg, "fire|%d burning, %d lit, %d out", w, lit, out); logev(lg); printf("FIRE %d burning (%d newly lit, %d out)\n", w, lit, out); return 0;
}
static int do_rain(void) {
    char p[PATH_MAX], b[16] = ""; P(p, "rain.txt"); FILE *f = fopen(p, "r"); if (f) { if (fgets(b, sizeof b, f)) b[strcspn(b, "\r\n")] = 0; fclose(f); } if (strcmp(b, "on")) { printf("NOTHING: it is not raining\n"); return 0; }
    static Row ct[MAXR], fr[MAXR], dr[MAXR], wt[MAXR]; int nc = load("containers.txt", ct, MAXR), nf = load("fire.txt", fr, MAXR), nd = load("dirt.txt", dr, MAXR), nw = load("wet.txt", wt, MAXR); int filled = 0, doused = 0, wet = 0;
    for (int i = 0; i < nc; i++) { long w = atol(ct[i].f[2]), cap = atol(ct[i].f[3]); if (w < cap) { snprintf(ct[i].f[2], 24, "%ld", w + 1); filled++; } }
    for (int i = 0; i < nf;) { if (has("roofed.txt", atoi(fr[i].f[0]), atoi(fr[i].f[1]))) { i++; continue; } long fu = atol(fr[i].f[2]) - 1; if (fu <= 0) { fire_del(fr, &nf, i); doused++; } else { snprintf(fr[i].f[2], 24, "%ld", fu); i++; } }
    for (int i = 0; i < nd; i++) { int x = atoi(dr[i].f[0]), y = atoi(dr[i].f[1]), seen = 0; for (int k = 0; k < nw; k++) if (atoi(wt[k].f[0]) == x && atoi(wt[k].f[1]) == y) seen = 1; if (!seen && nw < MAXR) { Row *q = &wt[nw++]; q->n = 2; snprintf(q->f[0], 24, "%d", x); snprintf(q->f[1], 24, "%d", y); wet++; } }
    save("containers.txt", ct, nc); save("fire.txt", fr, nf); save("wet.txt", wt, nw); char lg[120]; snprintf(lg, sizeof lg, "rain|%d containers filled, %d fires out, %d cells wet", filled, doused, wet); logev(lg); printf("RAIN %d containers +1, %d fires out, %d cells newly wet\n", filled, doused, wet); return 0;
}
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: world_common_op <book_dir> flow|fire|rain [on|off]|status\n"); return 2; }
    snprintf(BOOK, sizeof BOOK, "%s", argv[1]); char q[PATH_MAX]; snprintf(q, sizeof q, "%s/QUARANTINE", BOOK); if (access(q, F_OK) != 0) { printf("NOT A QUARANTINED BOOK: %s has no QUARANTINE file\n", BOOK); return 4; }
    char sd[PATH_MAX]; snprintf(sd, sizeof sd, "%s/state", BOOK); mkdir(sd, 0755); const char *v = argv[2];
    if (!strcmp(v, "flow") && argc == 3) return do_flow();
    if (!strcmp(v, "fire") && argc == 3) return do_fire();
    if (!strcmp(v, "rain") && argc == 3) return do_rain();
    if (!strcmp(v, "rain") && argc == 4 && (!strcmp(argv[3], "on") || !strcmp(argv[3], "off"))) { char p[PATH_MAX]; P(p, "rain.txt"); FILE *f = fopen(p, "w"); if (!f) return 3; fprintf(f, "%s\n", argv[3]); fclose(f); logev(!strcmp(argv[3], "on") ? "rain|on" : "rain|off"); printf("RAIN %s\n", argv[3]); return 0; }
    if (!strcmp(v, "status")) { static Row a[MAXR], b[MAXR]; int nf = load("fluids.txt", a, MAXR), nr = load("fire.txt", b, MAXR); printf("fluid cells %d volume %ld, fires %d\n", nf, sumvol(a, nf), nr); return 0; }
    fprintf(stderr, "usage: world_common_op <book_dir> flow|fire|rain [on|off]|status\n"); return 2;
}
