#define _GNU_SOURCE
/* mcl_op - early-survivor step 5 (16.game/EARLY-SURVIVOR-AND-COMMON-EVENTS.md "Craft places", MCL place), QUARANTINED in the editor's own book (2026-11-04). Items move ONLY through item_ledger_op (burn / mint source craft); this op never edits a count.
 *   mcl_op <book> craft <actor> <recipe> <x> <y>        2x2 recipes (cells fit one 2x2 corner) need no bench; bigger ones need a placed `bench` cell AT <x>,<y>. Refused with NOTHING burned when a cell's item is missing.
 *   mcl_op <book> smelt <actor> <x> <y>                 SMELT row of mcl_recipes.pdl: needs a placed `furnace` at <x>,<y>; burns 1 input + 1 fuel, mints the output, logs the minutes
 *   mcl_op <book> place <actor> <id> <x> <y>            id = bench|bed|furnace: burns 1 item mcl.<id> from the actor and writes the cell (state/mcl_cells.txt `id|x|y|owner`); one cell per x,y
 *   mcl_op <book> respawn <actor> <fx> <fy>             writes state/mcl_pos.txt `actor|x|y`: the actor's own BED cell if it still stands, else the fallback fx,fy (a bed is not the CDDA sleep verb; it is what spawn reads on death)
 *   mcl_op <book> burst <x> <y> <radius>                creeper: radius 1..4; every block row (state/mcl_blocks.txt `x|y|kind`) and every placed cell within the radius (dx*dx+dy*dy <= r*r) becomes air. NO drops, no item is written: the log says so
 *   mcl_op <book> status
 * Runs only in a folder that holds QUARANTINE (exit 4). Exit 0 ok, 2 usage, 3 refused (reason on stdout). Env MCL_LEDGER_OP overrides the ledger op path. COST: one short process per call (plus one ledger process per item row). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>
static char BOOK[PATH_MAX], LOP[PATH_MAX];
static void P(char *o, const char *rel) { snprintf(o, PATH_MAX, "%s/%s", BOOK, rel); }
static void sanit(char *s) { for (; *s; s++) if (*s == '|' || *s == '\n' || *s == '\r' || *s == ' ') *s = '_'; }
static void logev(const char *actor, const char *verb, const char *detail) { char p[PATH_MAX]; P(p, "state/events.txt"); FILE *f = fopen(p, "a"); if (f) { fprintf(f, "%ld|%s|%s|%s\n", (long)time(NULL), actor, verb, detail); fclose(f); } }
static int ledger(char *out, size_t cap, const char *a, const char *b, const char *c, const char *d, const char *e, const char *f) {
    const char *av[10]; int n = 0; av[n++] = LOP; av[n++] = BOOK; const char *x[6] = { a, b, c, d, e, f }; for (int i = 0; i < 6 && x[i]; i++) av[n++] = x[i]; av[n] = NULL;
    int pf[2]; if (pipe(pf) != 0) return -1; pid_t k = fork(); if (k < 0) return -1; if (k == 0) { dup2(pf[1], 1); close(pf[0]); execv(LOP, (char *const *)av); _exit(127); }
    close(pf[1]); size_t got = 0; ssize_t r; while (got + 1 < cap && (r = read(pf[0], out + got, cap - 1 - got)) > 0) got += (size_t)r; out[got] = 0; close(pf[0]);
    while (got > 0 && (out[got - 1] == '\n' || out[got - 1] == '\r')) out[--got] = 0; int st = 0; waitpid(k, &st, 0); return WIFEXITED(st) ? WEXITSTATUS(st) : -1; }
static long held(const char *actor, const char *item) { char o[4096]; if (ledger(o, sizeof o, "balance", actor, NULL, NULL, NULL, NULL) != 0) return 0; long n = 0;
    for (char *sv = NULL, *l = strtok_r(o, "\n", &sv); l; l = strtok_r(NULL, "\n", &sv)) { char *a = strchr(l, '|'); if (!a) continue; *a++ = 0; char *b = strchr(a, '|'); if (!b) continue; *b++ = 0; if (!strcmp(l, actor) && !strcmp(a, item)) n = atol(b); } return n; }
#define MAXN 300
typedef struct { char f[4][40]; } Row;
static int load(const char *rel, Row *r, int max) { char p[PATH_MAX], ln[200]; P(p, rel); FILE *f = fopen(p, "r"); if (!f) return 0; int n = 0; while (n < max && fgets(ln, sizeof ln, f)) { ln[strcspn(ln, "\r\n")] = 0; if (!ln[0] || ln[0] == '#') continue; memset(&r[n], 0, sizeof r[n]); char *sv = NULL; int k = 0; for (char *t = strtok_r(ln, "|", &sv); t && k < 4; t = strtok_r(NULL, "|", &sv)) snprintf(r[n].f[k++], 40, "%s", t); n++; } fclose(f); return n; }
static void save(const char *rel, Row *r, int n, int nf) { char p[PATH_MAX], t[PATH_MAX]; P(p, rel); snprintf(t, sizeof t, "%s.tmp", p); FILE *f = fopen(t, "w"); if (!f) return; for (int i = 0; i < n; i++) { for (int k = 0; k < nf; k++) fprintf(f, "%s%s", k ? "|" : "", r[i].f[k]); fputc('\n', f); } fclose(f); rename(t, p); }
static int cell_at(const char *id, int x, int y) { static Row c[MAXN]; int n = load("state/mcl_cells.txt", c, MAXN); for (int i = 0; i < n; i++) if (!strcmp(c[i].f[0], id) && atoi(c[i].f[1]) == x && atoi(c[i].f[2]) == y) return 1; return 0; }
/* recipe row lines can be longer than Row's 40-char fields: read the raw line */
static int find_line(const char *tag, const char *id, char *out, size_t cap) { char p[PATH_MAX], ln[400]; P(p, "mcl_recipes.pdl"); FILE *f = fopen(p, "r"); if (!f) return 0; size_t tl = strlen(tag); while (fgets(ln, sizeof ln, f)) { ln[strcspn(ln, "\r\n")] = 0; if (strncmp(ln, tag, tl) || ln[tl] != '|') continue; if (id && (strncmp(ln + tl + 1, id, strlen(id)) || ln[tl + 1 + strlen(id)] != '|')) continue; snprintf(out, cap, "%s", ln + tl + 1); fclose(f); return 1; } fclose(f); return 0; }
static int need(const char *actor, char names[][40], int cnt[], int n) { for (int i = 0; i < n; i++) { char it[60]; snprintf(it, sizeof it, "%s", names[i]); if (held(actor, it) < cnt[i]) { printf("REFUSED: %s needs %d x %s, holds %ld\n", actor, cnt[i], it, held(actor, it)); return 0; } } return 1; }
static int do_craft(const char *actor, const char *rid, int x, int y) {
    char line[400]; if (!find_line("RECIPE", rid, line, sizeof line)) { printf("REFUSED: no recipe %s\n", rid); return 3; }
    char *f1 = strchr(line, '|'), *f2 = f1 ? strchr(f1 + 1, '|') : NULL, *f3 = f2 ? strchr(f2 + 1, '|') : NULL; if (!f3) { printf("REFUSED: bad recipe row\n"); return 3; } *f1 = *f2 = *f3 = 0; char *grid = f1 + 1, *out = f2 + 1; int ocount = atoi(f3 + 1); if (ocount < 1 || ocount > 64) { printf("REFUSED: bad output count\n"); return 3; }
    char cells[9][40]; int nc = 0; { char g[200]; snprintf(g, sizeof g, "%s", grid); char *sv = NULL; for (char *t = strtok_r(g, ",", &sv); t && nc < 10; t = strtok_r(NULL, ",", &sv)) { if (nc < 9) snprintf(cells[nc], 40, "%s", t); nc++; } }
    if (nc != 9) { printf("REFUSED: recipe grid has %d cells, want 9\n", nc); return 3; }
    int maxr = -1, maxc = -1; char names[9][40]; int cnt[9]; int nn = 0;
    for (int i = 0; i < 9; i++) { if (!strcmp(cells[i], ".")) continue; if (i / 3 > maxr) maxr = i / 3; if (i % 3 > maxc) maxc = i % 3; char it[40]; snprintf(it, sizeof it, "mcl.%s", cells[i]); int k; for (k = 0; k < nn; k++) if (!strcmp(names[k], it)) break; if (k == nn) { snprintf(names[nn], 40, "%s", it); cnt[nn++] = 0; } cnt[k]++; }
    if (!nn) { printf("REFUSED: empty recipe\n"); return 3; }
    int big = maxr > 1 || maxc > 1; if (big && !cell_at("bench", x, y)) { printf("REFUSED: %s is a 3x3 recipe: stand on a crafting bench cell (none at %d,%d)\n", rid, x, y); return 3; }
    if (!need(actor, names, cnt, nn)) return 3;
    char o[300], why[80], n1[16]; snprintf(why, sizeof why, "craft:%s", rid);
    for (int i = 0; i < nn; i++) { snprintf(n1, sizeof n1, "%d", cnt[i]); if (ledger(o, sizeof o, "burn", actor, names[i], n1, why, NULL) != 0) { printf("REFUSED: ledger: %s\n", o); return 3; } }
    snprintf(n1, sizeof n1, "%d", ocount); if (ledger(o, sizeof o, "mint", actor, out, n1, "craft", NULL) != 0) { printf("REFUSED: ledger: %s\n", o); return 3; }
    char d[200]; snprintf(d, sizeof d, "%s %s x%d (%s)", rid, out, ocount, big ? "3x3 on a bench" : "2x2 no bench"); logev(actor, "mcl_craft", d); printf("CRAFTED %s: %s x%d (%s)\n", rid, out, ocount, big ? "3x3 on a bench" : "2x2 no bench"); return 0;
}
static int do_smelt(const char *actor, int x, int y) {
    char line[300]; if (!find_line("SMELT", NULL, line, sizeof line)) { printf("REFUSED: no SMELT row\n"); return 3; } char in[40], fuel[40], out[40]; int cnt = 0, mins = 0; { char *sv = NULL, *t[5] = {0}; int k = 0; for (char *p = strtok_r(line, "|", &sv); p && k < 5; p = strtok_r(NULL, "|", &sv)) t[k++] = p; if (k < 5) { printf("REFUSED: bad SMELT row\n"); return 3; } snprintf(in, 40, "%s", t[0]); snprintf(fuel, 40, "%s", t[1]); snprintf(out, 40, "%s", t[2]); cnt = atoi(t[3]); mins = atoi(t[4]); }
    if (!cell_at("furnace", x, y)) { printf("REFUSED: no furnace cell at %d,%d\n", x, y); return 3; } char names[2][40]; int c[2] = {1, 1}; if (!strcmp(in, fuel)) c[0] = 2; snprintf(names[0], 40, "%s", in); snprintf(names[1], 40, "%s", fuel); int nn = strcmp(in, fuel) ? 2 : 1; if (!need(actor, names, c, nn)) return 3;
    char o[300], n1[16] = "1"; if (ledger(o, sizeof o, "burn", actor, in, n1, "smelt", NULL) != 0 || ledger(o, sizeof o, "burn", actor, fuel, n1, "fuel", NULL) != 0) { printf("REFUSED: ledger: %s\n", o); return 3; }
    snprintf(n1, sizeof n1, "%d", cnt); if (ledger(o, sizeof o, "mint", actor, out, n1, "craft", NULL) != 0) { printf("REFUSED: ledger: %s\n", o); return 3; } char d[160]; snprintf(d, sizeof d, "%s + %s -> %s x%d, %d minutes", in, fuel, out, cnt, mins); logev(actor, "smelt", d); printf("SMELTED %s (spend %d game minutes)\n", d, mins); return 0;
}
static int do_place(const char *actor, const char *id, int x, int y) {
    if (strcmp(id, "bench") && strcmp(id, "bed") && strcmp(id, "furnace")) { printf("REFUSED: %s is not a placeable id (bench, bed, furnace)\n", id); return 3; }
    static Row c[MAXN]; int n = load("state/mcl_cells.txt", c, MAXN); for (int i = 0; i < n; i++) if (atoi(c[i].f[1]) == x && atoi(c[i].f[2]) == y) { printf("REFUSED: cell %d,%d is taken by a %s\n", x, y, c[i].f[0]); return 3; } if (n >= MAXN) { printf("REFUSED: too many cells\n"); return 3; }
    char it[60], o[300], n1[] = "1"; snprintf(it, sizeof it, "mcl.%s", id); if (held(actor, it) < 1) { printf("REFUSED: %s holds no %s\n", actor, it); return 3; } if (ledger(o, sizeof o, "burn", actor, it, n1, "placed", NULL) != 0) { printf("REFUSED: ledger: %s\n", o); return 3; }
    snprintf(c[n].f[0], 40, "%s", id); snprintf(c[n].f[1], 40, "%d", x); snprintf(c[n].f[2], 40, "%d", y); snprintf(c[n].f[3], 40, "%s", actor); save("state/mcl_cells.txt", c, n + 1, 4); char d[100]; snprintf(d, sizeof d, "%s at %d,%d", id, x, y); logev(actor, "mcl_place", d); printf("PLACED %s\n", d); return 0;
}
static int do_respawn(const char *actor, int fx, int fy) {
    static Row c[MAXN]; int n = load("state/mcl_cells.txt", c, MAXN), x = fx, y = fy, bed = 0; for (int i = 0; i < n; i++) if (!strcmp(c[i].f[0], "bed") && !strcmp(c[i].f[3], actor)) { x = atoi(c[i].f[1]); y = atoi(c[i].f[2]); bed = 1; }
    static Row p[MAXN]; int np = load("state/mcl_pos.txt", p, MAXN), k; for (k = 0; k < np; k++) if (!strcmp(p[k].f[0], actor)) break; if (k == np) { if (np >= MAXN) return 3; np++; } snprintf(p[k].f[0], 40, "%s", actor); snprintf(p[k].f[1], 40, "%d", x); snprintf(p[k].f[2], 40, "%d", y); save("state/mcl_pos.txt", p, np, 3);
    printf("RESPAWN %s at %d,%d (%s)\n", actor, x, y, bed ? "own bed" : "fallback, no bed"); return 0;
}
static int do_burst(int x, int y, int r) {
    if (r < 1 || r > 4) { printf("REFUSED: radius must be 1..4\n"); return 3; } static Row b[MAXN], c[MAXN]; int nb = load("state/mcl_blocks.txt", b, MAXN), nc = load("state/mcl_cells.txt", c, MAXN), wb = 0, wc = 0;
    for (int i = 0; i < nb; i++) { int dx = atoi(b[i].f[0]) - x, dy = atoi(b[i].f[1]) - y; if (dx * dx + dy * dy > r * r) b[wb++] = b[i]; } for (int i = 0; i < nc; i++) { int dx = atoi(c[i].f[1]) - x, dy = atoi(c[i].f[2]) - y; if (dx * dx + dy * dy > r * r) c[wc++] = c[i]; }
    save("state/mcl_blocks.txt", b, wb, 3); save("state/mcl_cells.txt", c, wc, 4); char d[140]; snprintf(d, sizeof d, "burst %d,%d r%d: %d blocks and %d placed cells became air, NO drops, no item written", x, y, r, nb - wb, nc - wc); logev("creeper", "burst", d); printf("BURST %s\n", d); return 0;
}
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: mcl_op <book_dir> craft|smelt|place|respawn|burst|status ...\n"); return 2; }
    snprintf(BOOK, sizeof BOOK, "%s", argv[1]); char q[PATH_MAX]; snprintf(q, sizeof q, "%s/QUARANTINE", BOOK); if (access(q, F_OK) != 0) { printf("NOT A QUARANTINED BOOK: %s has no QUARANTINE file\n", BOOK); return 4; }
    const char *lo = getenv("MCL_LEDGER_OP"); if (lo && lo[0]) snprintf(LOP, sizeof LOP, "%s", lo); else { char self[PATH_MAX]; ssize_t k = readlink("/proc/self/exe", self, sizeof self - 1); if (k < 0) return 2; self[k] = 0; char *sl = strrchr(self, '/'); if (sl) *sl = 0; snprintf(LOP, sizeof LOP, "%s/item_ledger_op.+x", self); }
    char sd[PATH_MAX]; P(sd, "state"); mkdir(sd, 0755); const char *v = argv[2]; char actor[64] = ""; if (argc > 3) { snprintf(actor, sizeof actor, "%s", argv[3]); sanit(actor); }
    if (!strcmp(v, "craft") && argc == 7) return do_craft(actor, argv[4], atoi(argv[5]), atoi(argv[6]));
    if (!strcmp(v, "smelt") && argc == 6) return do_smelt(actor, atoi(argv[4]), atoi(argv[5]));
    if (!strcmp(v, "place") && argc == 7) return do_place(actor, argv[4], atoi(argv[5]), atoi(argv[6]));
    if (!strcmp(v, "respawn") && argc == 6) return do_respawn(actor, atoi(argv[4]), atoi(argv[5]));
    if (!strcmp(v, "burst") && argc == 6) return do_burst(atoi(argv[3]), atoi(argv[4]), atoi(argv[5]));
    if (!strcmp(v, "status")) { static Row a[MAXN], b[MAXN]; printf("cells %d blocks %d\n", load("state/mcl_cells.txt", a, MAXN), load("state/mcl_blocks.txt", b, MAXN)); return 0; }
    fprintf(stderr, "usage: mcl_op <book_dir> craft <actor> <recipe> <x> <y> | smelt <actor> <x> <y> | place <actor> <id> <x> <y> | respawn <actor> <fx> <fy> | burst <x> <y> <r> | status\n"); return 2;
}
