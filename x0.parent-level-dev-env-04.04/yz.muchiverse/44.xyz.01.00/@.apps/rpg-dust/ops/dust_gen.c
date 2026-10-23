/* dust_gen - R2 of rpg-dust (BOOK 16.game/RPG-DUST-DESIGN.md sections 3, 4, 9): the first real generator, two neighbouring levels and the zoom between them. Integer arithmetic only, so the files are byte-identical on any machine.
 *   dust_gen region   <scale.pdl> <addr> <out>              region page for an address: N x N heights 0..99 (N = 16): a coarse 5 x 5 noise lattice from the region-level seed, bilinear in integers, plus small per-cell noise
 *   dust_gen area     <scale.pdl> <region-page> <cx> <cy> <addr> <out>   area page for ONE cell of that region page ("zoom in"): N x N heights, seeded from the area-level seed and <addr>/cell:cx,cy
 *   dust_gen page     <scale.pdl> <addr> <out>              the page at an address, from the address alone: a region page, or (address ending /cell:cx,cy) the area page of that region cell
 *   dust_gen zoom_in  <addr> <cx> <cy>                      prints addr/cell:cx,cy (cx, cy 0..15; the address must not already be an area address)
 *   dust_gen zoom_out <addr>                                prints "<parent addr> <cx> <cy>" (exit 2 on a region address, which has no parent here)
 *   dust_gen conserve <region-page> <cx> <cy> <area-page>   the zoom must invent nothing: the area page's heights sum to exactly N*N*(the region cell's height), and every height is 0..99. Prints OK or MISMATCH, exit 0 / 1, 2 on unreadable input
 * File format (pipe rows like the rest of the house): `PAGE | <level> | <addr> | <seed> | <n>` then n rows of n two-digit heights. Parent link for an area page: its addr ends in /cell:cx,cy.
 * The conservation rule is the design's "zoom in makes the new page agree with the summary row": the region cell is the summary, the area page is the detail, and the check counts that no height was invented or lost.
 * Build: sh ops/build_dust_check.sh */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "dust_hash.c"
#define N 16
static int noise(uint64_t seed, int x, int y, int mod) { return (int)(mix(seed ^ mix((uint64_t)(unsigned)x << 32 | (unsigned)y)) % (uint64_t)mod); }
static int write_page(const char *out, const char *level, const char *addr, uint64_t seed, int h[N][N]) {
    FILE *f = fopen(out, "w"); if (!f) { fprintf(stderr, "dust_gen: cannot write %s\n", out); return 2; }
    fprintf(f, "PAGE | %s | %s | %llu | %d\n", level, addr, (unsigned long long)seed, N);
    for (int y = 0; y < N; y++) { for (int x = 0; x < N; x++) fprintf(f, "%s%02d", x ? " " : "", h[y][x]); fprintf(f, "\n"); }
    return fclose(f) ? 2 : 0;
}
static int read_page(const char *path, int h[N][N]) {
    FILE *f = fopen(path, "r"); if (!f) return 0; char ln[256]; if (!fgets(ln, sizeof ln, f) || strncmp(ln, "PAGE |", 6)) { fclose(f); return 0; }
    for (int y = 0; y < N; y++) { if (!fgets(ln, sizeof ln, f)) { fclose(f); return 0; } char *p = ln; for (int x = 0; x < N; x++) { while (*p == ' ') p++; if (!isdigit((unsigned char)p[0]) || !isdigit((unsigned char)p[1])) { fclose(f); return 0; } h[y][x] = (p[0] - '0') * 10 + (p[1] - '0'); p += 2; } }
    fclose(f); return 1;
}
static int region_h(const char *pdl, const char *addr, int h[N][N], uint64_t *seed) {
    uint64_t s; if (!level_seed(pdl, "region", &s)) return 0; s = fold(s, addr);
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
        int fx = x * 4 * 16 / (N - 1), fy = y * 4 * 16 / (N - 1);       /* lattice coordinate in 1/16 units, 0..64 */
        int ix = fx / 16, iy = fy / 16; if (ix > 3) ix = 3; if (iy > 3) iy = 3; int tx = fx - ix * 16, ty = fy - iy * 16;
        int a = noise(s, ix, iy, 100), b = noise(s, ix + 1, iy, 100), c = noise(s, ix, iy + 1, 100), d = noise(s, ix + 1, iy + 1, 100);
        int top = a * (16 - tx) + b * tx, bot = c * (16 - tx) + d * tx; int v = (top * (16 - ty) + bot * ty) / 256;
        v += noise(s, 1000 + x, 1000 + y, 7) - 3; h[y][x] = v < 0 ? 0 : v > 99 ? 99 : v;
    }
    *seed = s; return 1;
}
/* area page of region cell (cx,cy): rh is that region's page; full = "<region addr>/cell:cx,cy" */
static int area_h(const char *pdl, int rh[N][N], int cx, int cy, const char *addr, int h[N][N], uint64_t *seed, char *full, size_t fsz) {
    uint64_t s; if (!level_seed(pdl, "area", &s)) return 0; snprintf(full, fsz, "%s/cell:%d,%d", addr, cx, cy); s = fold(s, full);
    int parent = rh[cy][cx]; long sum = 0;
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) { int v = parent + noise(s, x, y, 17) - 8; h[y][x] = v < 0 ? 0 : v > 99 ? 99 : v; sum += h[y][x]; }
    long want = (long)N * N * parent;                                  /* fix the residual cell by cell in scan order, within 0..99: always reachable for 0 <= parent <= 99 */
    while (sum != want) { int moved = 0; for (int y = 0; y < N && sum != want; y++) for (int x = 0; x < N && sum != want; x++) { if (sum < want && h[y][x] < 99) { h[y][x]++; sum++; moved = 1; } else if (sum > want && h[y][x] > 0) { h[y][x]--; sum--; moved = 1; } } if (!moved) return 0; }
    *seed = s; return 1;
}
static int gen_region(const char *pdl, const char *addr, const char *out) {
    int h[N][N]; uint64_t s; if (!region_h(pdl, addr, h, &s)) return 2; return write_page(out, "region", addr, s, h);
}
static int gen_area(const char *pdl, const char *rpage, int cx, int cy, const char *addr, const char *out) {
    int rh[N][N], h[N][N]; if (!read_page(rpage, rh)) { fprintf(stderr, "dust_gen: cannot read region page %s\n", rpage); return 2; }
    if (cx < 0 || cy < 0 || cx >= N || cy >= N) { fprintf(stderr, "dust_gen: cell out of range\n"); return 2; }
    uint64_t s; char full[400]; if (!area_h(pdl, rh, cx, cy, addr, h, &s, full, sizeof full)) return 2; return write_page(out, "area", full, s, h);
}
/* ---- addresses: a region address is path components joined by '/'; an AREA address is a region address plus a last component cell:cx,cy ---- */
static int split_cell(const char *addr, char *parent, size_t psz, int *cx, int *cy) {
    const char *slash = strrchr(addr, '/'); const char *last = slash ? slash + 1 : addr; char junk;
    if (!slash || sscanf(last, "cell:%d,%d%c", cx, cy, &junk) != 2 || *cx < 0 || *cy < 0 || *cx >= N || *cy >= N) return 0;
    size_t l = (size_t)(slash - addr); if (l == 0 || l >= psz) return 0; memcpy(parent, addr, l); parent[l] = 0; return 1;
}
/* page <scale.pdl> <addr> <out>: the page that lives at an address, made from the address alone (region, or area when the address ends in /cell:cx,cy) */
static int gen_page(const char *pdl, const char *addr, const char *out) {
    char parent[400]; int cx, cy; const char *sl = strrchr(addr, '/'); int cellish = !strncmp(sl ? sl + 1 : addr, "cell:", 5);
    if (!split_cell(addr, parent, sizeof parent, &cx, &cy)) { if (cellish) { fprintf(stderr, "dust_gen: malformed cell address %s\n", addr); return 2; } return gen_region(pdl, addr, out); }
    int rh[N][N], h[N][N]; uint64_t rs, s; char full[400];
    if (!region_h(pdl, parent, rh, &rs) || !area_h(pdl, rh, cx, cy, parent, h, &s, full, sizeof full)) { return 2; }
    return write_page(out, "area", full, s, h);
}
static int conserve(const char *rpage, int cx, int cy, const char *apage) {
    int rh[N][N], ah[N][N]; if (!read_page(rpage, rh) || !read_page(apage, ah)) { fprintf(stderr, "dust_gen: unreadable page\n"); return 2; }
    if (cx < 0 || cy < 0 || cx >= N || cy >= N) { fprintf(stderr, "dust_gen: cell out of range\n"); return 2; }
    long sum = 0; for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) sum += ah[y][x]; long want = (long)N * N * rh[cy][cx];
    if (sum != want) { printf("MISMATCH cell %d,%d: region height %d needs sum %ld, area page sums to %ld\n", cx, cy, rh[cy][cx], want, sum); return 1; }
    printf("OK cell %d,%d: region height %d, area sum %ld\n", cx, cy, rh[cy][cx], sum); return 0;
}
int main(int argc, char **argv) {
    if (argc == 5 && !strcmp(argv[1], "region")) return gen_region(argv[2], argv[3], argv[4]);
    if (argc == 8 && !strcmp(argv[1], "area")) return gen_area(argv[2], argv[3], atoi(argv[4]), atoi(argv[5]), argv[6], argv[7]);
    if (argc == 5 && !strcmp(argv[1], "page")) return gen_page(argv[2], argv[3], argv[4]);
    if (argc == 5 && !strcmp(argv[1], "zoom_in")) { char p[400]; int a, b; int cx = atoi(argv[3]), cy = atoi(argv[4]); if (cx < 0 || cy < 0 || cx >= N || cy >= N || split_cell(argv[2], p, sizeof p, &a, &b)) return 2; printf("%s/cell:%d,%d\n", argv[2], cx, cy); return 0; }
    if (argc == 3 && !strcmp(argv[1], "zoom_out")) { char p[400]; int cx, cy; if (!split_cell(argv[2], p, sizeof p, &cx, &cy)) return 2; printf("%s %d %d\n", p, cx, cy); return 0; }
    if (argc == 6 && !strcmp(argv[1], "conserve")) return conserve(argv[2], atoi(argv[3]), atoi(argv[4]), argv[5]);
    fprintf(stderr, "usage: dust_gen region <scale.pdl> <addr> <out> | area <scale.pdl> <region-page> <cx> <cy> <addr> <out> | page <scale.pdl> <addr> <out> | zoom_in <addr> <cx> <cy> | zoom_out <addr> | conserve <region-page> <cx> <cy> <area-page>\n"); return 2;
}
