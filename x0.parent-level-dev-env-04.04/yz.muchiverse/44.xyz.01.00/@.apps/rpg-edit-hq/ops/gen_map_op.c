#define _GNU_SOURCE
/* gen_map_op - ONE deterministic map writer, three readers later (16.game/EARLY-SURVIVOR-AND-COMMON-EVENTS.md step 3), QUARANTINED in the editor's own book (2026-11-04). Output is FILES, so the same seed gives byte-identical files.
 *   gen_map_op <book_dir> cdda_house <seed> <out_dir>    one house chunk: wall ring, partition with a door gap, a front door, furniture from furniture.pdl, loot rows
 *   gen_map_op <book_dir> df_column  <seed> <out_dir>    one site column: soil, stone, minerals from minerals.pdl written up front (digging reveals, there is no second roll)
 *   gen_map_op <book_dir> verify_house|verify_column <out_dir>   re-reads the files and checks the invariants (exit 1 + reason when broken)
 * Runs only in a folder that holds QUARANTINE (exit 4 otherwise). Writes nothing outside <out_dir>, never an item (loot is ROWS, a pickup verb mints later).
 * Files (cdda_house): chunk.txt (ASCII rows: # wall . floor + door B/L/F furniture), legend.txt (feed legend_page_op to get cells.txt), placed.txt (id|x|y|hp: the exact format of survivor_op's state/placed.txt, so the furniture is smashable), loot.txt (LOOT|x|y|item:n).
 * Files (df_column): column.txt (x|y|z|kind|mineral), header with W H Z. Sizes are bounded: house 10..14 x 8..10, column 8x8x12. Randomness = splitmix64 seeded from an FNV-1a hash of the seed text: no time, no libc rand, no hash-table order.
 * Data: furniture.pdl `FURNITURE|id|hp|drops` (marker = first letter uppercased), loot.pdl `LOOTITEM|item`, minerals.pdl `MINERAL|id|tile|item`. COST: one short process, files of a few KB. Exit: 0 ok, 1 verify failed, 2 usage, 3 refused, 4 not quarantined. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <ctype.h>
#include <sys/stat.h>
typedef unsigned long long u64;
static u64 rs;
static u64 rnd(void) { u64 z = (rs += 0x9E3779B97F4A7C15ULL); z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL; z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL; return z ^ (z >> 31); }
static int rr(int n) { return n <= 1 ? 0 : (int)(rnd() % (u64)n); }
static char BOOK[PATH_MAX], OUT[PATH_MAX];
static void seed_from(const char *s) { u64 h = 1469598103934665603ULL; for (; *s; s++) { h ^= (unsigned char)*s; h *= 1099511628211ULL; } rs = h; }
static FILE *outf(const char *name, const char *mode) { char p[PATH_MAX]; snprintf(p, sizeof p, "%s/%s", OUT, name); return fopen(p, mode); }
#define MAXF 16
static char fid[MAXF][32]; static int fhp[MAXF]; static char fch[MAXF]; static int nf;
static void load_furn(void) { char p[PATH_MAX]; snprintf(p, sizeof p, "%s/furniture.pdl", BOOK); FILE *f = fopen(p, "r"); if (!f) return; char ln[400]; while (fgets(ln, sizeof ln, f) && nf < MAXF) { if (strncmp(ln, "FURNITURE|", 10)) continue; char id[32]; int hp = 0; if (sscanf(ln + 10, "%31[^|]|%d", id, &hp) < 2) continue; snprintf(fid[nf], 32, "%s", id); fhp[nf] = hp; fch[nf] = (char)toupper((unsigned char)id[0]); nf++; } fclose(f); }
static int list_pdl(const char *file, const char *tag, char out[][48], int max) { char p[PATH_MAX]; snprintf(p, sizeof p, "%s/%s", BOOK, file); FILE *f = fopen(p, "r"); if (!f) return 0; char ln[300]; int n = 0; size_t tl = strlen(tag); while (fgets(ln, sizeof ln, f) && n < max) { if (strncmp(ln, tag, tl) || ln[tl] != '|') continue; char *a = ln + tl + 1; a[strcspn(a, "|\r\n")] = 0; snprintf(out[n++], 48, "%s", a); } fclose(f); return n; }

static int gen_house(const char *seed) {
    load_furn(); if (!nf) { printf("REFUSED: no furniture in furniture.pdl\n"); return 3; } for (int i = 0; i < nf; i++) for (int j = i + 1; j < nf; j++) if (fch[i] == fch[j]) { printf("REFUSED: furniture %s and %s share the marker %c\n", fid[i], fid[j], fch[i]); return 3; }
    static char loot[8][48]; int nl = list_pdl("loot.pdl", "LOOTITEM", loot, 8);
    seed_from(seed); int W = 10 + rr(5), H = 8 + rr(3); static char g[12][16]; for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) g[y][x] = (x == 0 || y == 0 || x == W - 1 || y == H - 1) ? '#' : '.';
    int px = 3 + rr(W - 6); for (int y = 1; y < H - 1; y++) g[y][px] = '#'; int gap = 1 + rr(H - 2); g[gap][px] = '+';      /* partition with one door gap */
    int dx = 1 + rr(W - 2); g[H - 1][dx] = '+';      /* the front door in the bottom wall */
    int nfur = 3 + rr(4); static int fx[8], fy[8]; static char fids[8][32]; int hps[8]; int placed = 0, guard = 0;
    while (placed < nfur && guard++ < 200) { int x = 1 + rr(W - 2), y = 1 + rr(H - 2); if (g[y][x] != '.') continue; if (x == dx && y == H - 2) continue; if (x == px && abs(y - gap) <= 1) continue; if (x == px - 1 && y == gap) continue; if (x == px + 1 && y == gap) continue;      /* never block the front door's inside cell or the partition gap */
        int k = rr(nf); g[y][x] = fch[k]; fx[placed] = x; fy[placed] = y; snprintf(fids[placed], 32, "%s", fid[k]); hps[placed] = fhp[k]; placed++; }
    FILE *f = outf("chunk.txt", "w"); if (!f) { printf("REFUSED: cannot write %s\n", OUT); return 3; } fprintf(f, "# seed=%s v1 cdda_house %dx%d\n", seed, W, H); for (int y = 0; y < H; y++) { fwrite(g[y], 1, (size_t)W, f); fputc('\n', f); } fclose(f);
    f = outf("legend.txt", "w"); fprintf(f, "# seed=%s v1\nL # 1,3,1\nL . 1,0,1\nL + 1,0,1\n", seed); for (int i = 0; i < nf; i++) fprintf(f, "L %c 1,2,1\n", fch[i]); fprintf(f, "MAP\n"); for (int y = 0; y < H; y++) { fwrite(g[y], 1, (size_t)W, f); fputc('\n', f); } fclose(f);
    f = outf("placed.txt", "w"); for (int i = 0; i < placed; i++) fprintf(f, "%s|%d|%d|%d\n", fids[i], fx[i], fy[i], hps[i]); fclose(f);
    f = outf("loot.txt", "w"); int nloot = nl ? 2 + rr(3) : 0; for (int i = 0; i < nloot; i++) { int x = 1 + rr(W - 2), y = 1 + rr(H - 2); if (g[y][x] != '.') { i--; if (rr(50) == 0) break; continue; } fprintf(f, "LOOT|%d|%d|%s:%d\n", x, y, loot[rr(nl)], 1 + rr(2)); } fclose(f);
    printf("HOUSE %dx%d furniture=%d seed=%s\n", W, H, placed, seed); return 0;
}
static int gen_column(const char *seed) {
    static char min[8][48]; int nm = list_pdl("minerals.pdl", "MINERAL", min, 8); seed_from(seed); const int W = 8, H = 8, Z = 12;
    FILE *f = outf("column.txt", "w"); if (!f) { printf("REFUSED: cannot write %s\n", OUT); return 3; } fprintf(f, "# seed=%s v1 df_column W=%d H=%d Z=%d\n", seed, W, H, Z); int nore = 0;
    for (int x = 0; x < W; x++) for (int y = 0; y < H; y++) for (int z = 0; z < Z; z++) { const char *kind = z < 2 ? "soil" : "stone"; const char *m = "-"; if (z >= 2 && nm && rr(100) < 8) { m = min[rr(nm)]; nore++; } fprintf(f, "%d|%d|%d|%s|%s\n", x, y, z, kind, m); }
    fclose(f); printf("COLUMN %dx%dx%d minerals=%d seed=%s\n", W, H, Z, nore, seed); return 0;
}
static int verify_house(void) {
    FILE *f = outf("chunk.txt", "r"); if (!f) { printf("VERIFY FAILED: no chunk.txt\n"); return 1; } char ln[200]; int W = 0, H = 0, doors = 0; static char g[12][200]; if (!fgets(ln, sizeof ln, f) || ln[0] != '#') { printf("VERIFY FAILED: no header\n"); return 1; }
    while (fgets(ln, sizeof ln, f)) { ln[strcspn(ln, "\r\n")] = 0; if (!ln[0]) continue; if (H >= 12) { printf("VERIFY FAILED: too many rows\n"); return 1; } if (!W) W = (int)strlen(ln); if ((int)strlen(ln) != W) { printf("VERIFY FAILED: row %d has width %d, row 1 has %d\n", H + 1, (int)strlen(ln), W); return 1; } snprintf(g[H++], 200, "%s", ln); } fclose(f);
    if (W < 10 || W > 14 || H < 8 || H > 10) { printf("VERIFY FAILED: size %dx%d is outside 10..14 x 8..10\n", W, H); return 1; }
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) { char c = g[y][x]; int edge = x == 0 || y == 0 || x == W - 1 || y == H - 1; if (edge && c != '#' && c != '+') { printf("VERIFY FAILED: the outer wall is broken at %d,%d ('%c')\n", x, y, c); return 1; } if (c == '+') doors++; }
    int front = 0; for (int x = 0; x < W; x++) if (g[H - 1][x] == '+') front++; if (front != 1) { printf("VERIFY FAILED: the bottom wall has %d doors, want exactly 1\n", front); return 1; } if (doors < 1) { printf("VERIFY FAILED: no door\n"); return 1; }
    load_furn(); f = outf("placed.txt", "r"); int np = 0; if (f) { while (fgets(ln, sizeof ln, f)) { char id[32]; int x, y, hp; if (sscanf(ln, "%31[^|]|%d|%d|%d", id, &x, &y, &hp) != 4) { printf("VERIFY FAILED: bad placed row\n"); return 1; } int ok = 0; for (int i = 0; i < nf; i++) if (!strcmp(fid[i], id) && g[y][x] == fch[i] && fhp[i] == hp) ok = 1; if (!ok) { printf("VERIFY FAILED: placed %s at %d,%d does not match the chunk or furniture.pdl\n", id, x, y); return 1; } np++; } fclose(f); }
    int fc = 0; for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) for (int i = 0; i < nf; i++) if (g[y][x] == fch[i]) fc++; if (fc != np) { printf("VERIFY FAILED: %d furniture cells but %d placed rows\n", fc, np); return 1; }
    f = outf("loot.txt", "r"); int nloot = 0; if (f) { while (fgets(ln, sizeof ln, f)) { int x, y; if (sscanf(ln, "LOOT|%d|%d|", &x, &y) != 2 || g[y][x] != '.') { printf("VERIFY FAILED: loot not on a floor cell: %s", ln); return 1; } nloot++; } fclose(f); }
    printf("VERIFY OK %dx%d doors=%d furniture=%d loot=%d\n", W, H, doors, np, nloot); return 0;
}
static int verify_column(void) {
    FILE *f = outf("column.txt", "r"); if (!f) { printf("VERIFY FAILED: no column.txt\n"); return 1; } char ln[200]; if (!fgets(ln, sizeof ln, f) || ln[0] != '#') { printf("VERIFY FAILED: no header\n"); return 1; } long n = 0; int bad = 0; int lx = -1, ly = -1, lz = -1;
    while (fgets(ln, sizeof ln, f)) { int x, y, z; char kind[16], m[48]; if (sscanf(ln, "%d|%d|%d|%15[^|]|%47s", &x, &y, &z, kind, m) != 5) { printf("VERIFY FAILED: bad row %ld\n", n + 1); return 1; } if (x < 0 || x > 7 || y < 0 || y > 7 || z < 0 || z > 11) { printf("VERIFY FAILED: cell %d,%d,%d is outside the 8x8x12 column\n", x, y, z); return 1; }
        if (z < 2 && strcmp(kind, "soil")) bad = 1; if (z >= 2 && strcmp(kind, "stone")) bad = 1; if (z < 2 && strcmp(m, "-")) { printf("VERIFY FAILED: a mineral in the soil at %d,%d,%d\n", x, y, z); return 1; } if (bad) { printf("VERIFY FAILED: wrong kind at %d,%d,%d\n", x, y, z); return 1; }
        if (x < lx || (x == lx && (y < ly || (y == ly && z <= lz)))) { printf("VERIFY FAILED: rows are not in x,y,z order at row %ld\n", n + 1); return 1; } lx = x; ly = y; lz = z; n++; }
    if (n != 8 * 8 * 12) { printf("VERIFY FAILED: %ld cells, want %d\n", n, 8 * 8 * 12); return 1; } printf("VERIFY OK cells=%ld\n", n); return 0;
}
int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: gen_map_op <book_dir> cdda_house|df_column <seed> <out_dir> | verify_house|verify_column <out_dir>\n"); return 2; }
    snprintf(BOOK, sizeof BOOK, "%s", argv[1]); char q[PATH_MAX]; snprintf(q, sizeof q, "%s/QUARANTINE", BOOK); if (access(q, F_OK) != 0) { printf("NOT A QUARANTINED BOOK: %s has no QUARANTINE file\n", BOOK); return 4; }
    const char *v = argv[2];
    if (!strcmp(v, "verify_house") || !strcmp(v, "verify_column")) { snprintf(OUT, sizeof OUT, "%s", argv[3]); return !strcmp(v, "verify_house") ? verify_house() : verify_column(); }
    if (argc != 5) { fprintf(stderr, "usage: gen_map_op <book_dir> cdda_house|df_column <seed> <out_dir>\n"); return 2; }
    if (!argv[3][0] || strlen(argv[3]) > 60) { printf("REFUSED: the seed must be 1..60 characters\n"); return 3; } snprintf(OUT, sizeof OUT, "%s", argv[4]); mkdir(OUT, 0755);
    if (!strcmp(v, "cdda_house")) return gen_house(argv[3]); if (!strcmp(v, "df_column")) return gen_column(argv[3]);
    fprintf(stderr, "usage: gen_map_op <book_dir> cdda_house|df_column <seed> <out_dir>\n"); return 2;
}
