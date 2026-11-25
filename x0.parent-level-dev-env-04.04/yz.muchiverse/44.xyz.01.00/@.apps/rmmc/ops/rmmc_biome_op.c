/* rmmc_biome_op: paint a desk (page) from BIOME DATA ROWS, the rpg-space way (BOOK 16.game): the numbers live in biomes.pdl, this op holds none.
 *
 *   rmmc_biome_op <biomes.pdl> <desk_dir>                      build: repaint the desk in place
 *   rmmc_biome_op check <biomes.pdl> <desk_dir> [<src_desk|-> [key]]   read the desk back and print `key value` lines (with a key: only that value) for the harness
 *   rmmc_biome_op png <desk_dir> <out.png> [px_per_cell]       top-down picture of the desk (tile pictures + relief shading)
 *
 * Why (owner 2026-11-25): rmmc-0001 is the Centroid floating island at 3x. It wants taller mountains, lakes, snow and sand, Minecraft-look blocks. Grok's
 * rmmc_field_op painted terrain from magic coordinate ranges; here a BIOME row is data and one generic painter reads it.
 *
 * What it reads: the desk's CURRENT cells.txt for the ISLAND OUTLINE (a cell with floor!=0 or wall!=0 is land) and the underside depth (third token per cell) which it
 * keeps. Everything else is repainted. Same input = same bytes (integer-only seeded hash, no clock, no float anywhere in the painting).
 *
 * Desk shape written (identical to the rpg-space bridge desk, so the same desk drives 2D and 3D; the renderer reads them in bv_render_3d.c maker_load_base):
 *   cells.txt    header (width height tile_px atlas_cols atlas_tiles rock_slot) then `cells`, then one row per map row of tokens "floor,wall,depth".
 *                floor = atlas slot of the TOP face, wall = atlas slot of the SIDES above the floor level, depth = blocks hung under the cell (island underside).
 *   map.txt      one glyph per cell; the glyph is the cell's HEIGHT H (levels above the floor level):  'a' -2, 'b' -1, 'c' 0, '1'..'9' = 1..9, 'A'..'Z' = 10..35,
 *                '.' = no land (sky). Legend also in legend.txt beside map.txt.
 *   extrusion.pdl one row per glyph used, glyph -> engine height value: H >= 1 gives +H; H <= 0 gives H-1 (the engine sinks a surface by a NEGATIVE value, and a sunk
 *                column keeps one block, so a surface that ends H above the floor level is written H-1; see bv_render_3d.c zs[] / zh[]).
 *   cells.rgba   the atlas, raw RGBA8, 16 tiles per row, tile slot 0 = empty. Built from the Mineclonia textures named by TILE rows (asset root from
 *                shared/MINECLONIA-ASSET-SOURCE-LOCATION.pdl found by walking up from biomes.pdl, or $RMMC_HOUSE/shared/...). A missing PNG is reported on stderr
 *                ("MISSING asset") and the tile is a FLAT colour (the TILE row's fallback_rgb).
 *   cells.marker projection written last: size = old size mod 4000 + 1 (the engine reloads on a SIZE change); 1 byte when there was none.
 *   events.txt   written empty, or (RULE tree_mode events) one `x y r g b tree:<trunk>:<crown_r>:<crown_h> 0 0 0` row per TREE on land; the game adds its own actor rows.
 *   skin.txt     untouched.
 *
 * Rows of biomes.pdl ('#' comment, fields separated by '|', blanks trimmed):
 *   RULE  | key | value            seed, noise_scale, snow_line, rock_tile (a TILE name), tile_px, tree_trunk (TILE name), tree_leaves (TILE name)
 *   INCLUDE | path                 rows of another file first (relative to this file, or absolute); a later BIOME/TILE/RULE row with the SAME name replaces the earlier
 *                                  one in place (how a variant is written: include the base, restate one row)
 *   TILE  | name | slot | png_rel | fallback_rgb | tint_rgb | overlay_png     png_rel is under the asset root; tint multiplies; overlay is drawn over the base (tinted)
 *   BIOME | name | shape | cx | cy | rx | ry | mode | base | peak | top | side | cap | cap_from | jitter
 *           shape: all (the whole island), ellipse, rect (cx cy centre, rx ry half sizes), rim (land cells within rx cells of the outline; cx cy ry unused)
 *           mode:  flat (H = base) or cone (raise: H falls linearly from peak at the centre to base at the edge, written only where it is above base and above what is there)
 *           top / side: TILE names; cap: TILE name or '-' (top face of a cell whose H >= cap_from); cap_from: number, a RULE name (snow_line) or '-'
 *           jitter: smooth integer value noise in levels (+- jitter), lattice spacing RULE noise_scale; biomes apply in FILE ORDER, later ones overwrite earlier
 *   TREE  | name | cx | cy | trunk_h | crown_r | crown_h      trunk column (side wood, top leaves) trunk_h above the ground at cx cy; a leaf block of half size crown_r around it, crown_h high
 *
 * Limits (exit 5, nothing written): board over 256 cells, a height above 255, a height outside the glyph table.
 * Exit codes: 0 ok, 2 usage, 3 a file is missing or unreadable, 4 a bad row (unknown tile, bad number, no biome rows), 5 engine limit.
 * Nothing is half written: the whole desk is built in memory first, then the files are written.
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/rmmc_biome_op.+x rmmc_biome_op.c -lpng -lz
 */
#include <limits.h>
#include <png.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAXT 40
#define MAXB 96
#define MAXR 24
#define MAXTREE 256
#define MAXINC 4
#define NAMELEN 40

typedef struct { char name[NAMELEN]; int slot; char png[256]; unsigned fallback, tint; char overlay[256]; } Tile;
typedef struct { char name[NAMELEN], shape[12], mode[8], top[NAMELEN], side[NAMELEN], cap[NAMELEN], capfrom[NAMELEN]; int cx, cy, rx, ry, base, peak, jitter; } Biome;
typedef struct { char name[NAMELEN]; int cx, cy, trunk, crown_r, crown_h; } Tree;
typedef struct { char key[NAMELEN], val[NAMELEN]; } Rule;
typedef struct { Tile t[MAXT]; int nt; Biome b[MAXB]; int nb; Tree tr[MAXTREE]; int ntr; Rule r[MAXR]; int nr; char dir[1024]; } Book;

static int bad(const char *why, const char *row) { fprintf(stderr, "rmmc_biome_op: %s: %s\n", why, row); return 4; }

static void trim(char *s) {
    char *a = s; while (*a == ' ' || *a == '\t') a++;
    memmove(s, a, strlen(a) + 1);
    size_t n = strlen(s); while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\n' || s[n - 1] == '\r')) s[--n] = 0;
}
static int split(char *line, char **f, int max) {
    int n = 0; char *p = line;
    while (n < max) { f[n++] = p; char *q = strchr(p, '|'); if (!q) break; *q = 0; p = q + 1; }
    for (int i = 0; i < n; i++) trim(f[i]);
    return n;
}
static const char *rule(const Book *bk, const char *key, const char *def) {
    for (int i = 0; i < bk->nr; i++) if (!strcmp(bk->r[i].key, key)) return bk->r[i].val;
    return def;
}
static int tile_slot(const Book *bk, const char *name) {
    for (int i = 0; i < bk->nt; i++) if (!strcmp(bk->t[i].name, name)) return bk->t[i].slot;
    return -1;
}
static char *slurp(const char *p, size_t *n) {
    FILE *f = fopen(p, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char *b = malloc((size_t)sz + 1); if (!b) { fclose(f); return NULL; }
    size_t r = fread(b, 1, (size_t)sz, f); b[r] = 0; fclose(f); if (n) *n = r; return b;
}

/* ---------- biomes.pdl ---------- */
static int load_book(Book *bk, const char *path, int depth) {
    if (depth > MAXINC) return bad("INCLUDE nested too deep", path);
    FILE *f = fopen(path, "r"); if (!f) { fprintf(stderr, "rmmc_biome_op: cannot open %s\n", path); return 3; }
    char line[1024], *fl[20];
    while (fgets(line, sizeof line, f)) {
        char raw[1024]; snprintf(raw, sizeof raw, "%s", line); raw[strcspn(raw, "\n")] = 0;
        trim(line); if (!line[0] || line[0] == '#') continue;
        int n = split(line, fl, 20);
        if (!strcmp(fl[0], "INCLUDE") && n >= 2) {
            char inc[4300];
            if (fl[1][0] == '/') snprintf(inc, sizeof inc, "%s", fl[1]);
            else { char d[1024]; snprintf(d, sizeof d, "%s", path); char *s = strrchr(d, '/'); if (s) *s = 0; else strcpy(d, "."); snprintf(inc, sizeof inc, "%s/%s", d, fl[1]); }
            int rc = load_book(bk, inc, depth + 1); if (rc) { fclose(f); return rc; }
        } else if (!strcmp(fl[0], "RULE") && n >= 3) {
            int i; for (i = 0; i < bk->nr; i++) if (!strcmp(bk->r[i].key, fl[1])) break;
            if (i == bk->nr) { if (bk->nr >= MAXR) { fclose(f); return bad("too many RULE rows", raw); } bk->nr++; }
            snprintf(bk->r[i].key, NAMELEN, "%s", fl[1]); snprintf(bk->r[i].val, NAMELEN, "%s", fl[2]);
        } else if (!strcmp(fl[0], "TILE") && n >= 7) {
            int i; for (i = 0; i < bk->nt; i++) if (!strcmp(bk->t[i].name, fl[1])) break;
            if (i == bk->nt) { if (bk->nt >= MAXT) { fclose(f); return bad("too many TILE rows", raw); } bk->nt++; }
            Tile *t = &bk->t[i]; memset(t, 0, sizeof *t);
            snprintf(t->name, NAMELEN, "%s", fl[1]); t->slot = atoi(fl[2]); snprintf(t->png, sizeof t->png, "%s", fl[3]);
            t->fallback = (unsigned)strtoul(fl[4], NULL, 16); t->tint = !strcmp(fl[5], "-") ? 0xffffff : (unsigned)strtoul(fl[5], NULL, 16);
            if (strcmp(fl[6], "-")) snprintf(t->overlay, sizeof t->overlay, "%s", fl[6]);
            if (t->slot < 1 || t->slot > 255) { fclose(f); return bad("TILE slot must be 1..255", raw); }
        } else if (!strcmp(fl[0], "BIOME") && n >= 15) {
            int i; for (i = 0; i < bk->nb; i++) if (!strcmp(bk->b[i].name, fl[1])) break;
            if (i == bk->nb) { if (bk->nb >= MAXB) { fclose(f); return bad("too many BIOME rows", raw); } bk->nb++; }
            Biome *b = &bk->b[i]; memset(b, 0, sizeof *b);
            snprintf(b->name, NAMELEN, "%s", fl[1]); snprintf(b->shape, sizeof b->shape, "%s", fl[2]);
            b->cx = atoi(fl[3]); b->cy = atoi(fl[4]); b->rx = atoi(fl[5]); b->ry = atoi(fl[6]);
            snprintf(b->mode, sizeof b->mode, "%s", fl[7]); b->base = atoi(fl[8]); b->peak = atoi(fl[9]);
            snprintf(b->top, NAMELEN, "%s", fl[10]); snprintf(b->side, NAMELEN, "%s", fl[11]); snprintf(b->cap, NAMELEN, "%s", fl[12]); snprintf(b->capfrom, NAMELEN, "%s", fl[13]);
            b->jitter = atoi(fl[14]);
            if (strcmp(b->shape, "all") && strcmp(b->shape, "ellipse") && strcmp(b->shape, "rect") && strcmp(b->shape, "rim")) { fclose(f); return bad("shape is all|ellipse|rect|rim", raw); }
            if (strcmp(b->mode, "flat") && strcmp(b->mode, "cone")) { fclose(f); return bad("mode is flat|cone", raw); }
            if ((!strcmp(b->shape, "ellipse") || !strcmp(b->shape, "rect")) && (b->rx < 1 || b->ry < 1)) { fclose(f); return bad("rx and ry must be >= 1", raw); }
        } else if (!strcmp(fl[0], "TREE") && n >= 7) {
            if (bk->ntr >= MAXTREE) { fclose(f); return bad("too many TREE rows", raw); }
            Tree *t = &bk->tr[bk->ntr++]; snprintf(t->name, NAMELEN, "%s", fl[1]);
            t->cx = atoi(fl[2]); t->cy = atoi(fl[3]); t->trunk = atoi(fl[4]); t->crown_r = atoi(fl[5]); t->crown_h = atoi(fl[6]);
        } else { fclose(f); return bad("unknown or short row", raw); }
    }
    fclose(f);
    return 0;
}

/* ---------- PNG ---------- */
static unsigned char *load_png(const char *path, int *w, int *h) {
    FILE *fp = fopen(path, "rb"); if (!fp) return NULL;
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) { fclose(fp); return NULL; }
    png_infop info = png_create_info_struct(png);
    unsigned char *img = NULL; png_bytep *rows = NULL;
    if (!info || setjmp(png_jmpbuf(png))) { png_destroy_read_struct(&png, info ? &info : NULL, NULL); fclose(fp); free(img); free(rows); return NULL; }
    png_init_io(png, fp); png_read_info(png, info);
    *w = (int)png_get_image_width(png, info); *h = (int)png_get_image_height(png, info);
    png_byte ct = png_get_color_type(png, info);
    if (ct == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (ct == PNG_COLOR_TYPE_GRAY || ct == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_gray_to_rgb(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
    if (png_get_bit_depth(png, info) == 16) png_set_scale_16(png);
    if ((ct & PNG_COLOR_MASK_ALPHA) == 0) png_set_filler(png, 255, PNG_FILLER_AFTER);
    png_read_update_info(png, info);
    int rowb = (int)png_get_rowbytes(png, info);
    img = malloc((size_t)*h * (size_t)rowb); rows = malloc((size_t)*h * sizeof(png_bytep));
    if (!img || !rows) longjmp(png_jmpbuf(png), 1);
    for (int y = 0; y < *h; y++) rows[y] = img + (size_t)y * (size_t)rowb;
    png_read_image(png, rows);
    free(rows); png_destroy_read_struct(&png, &info, NULL); fclose(fp);
    return img;
}
static int write_png(const char *path, const unsigned char *rgb, int w, int h) {
    FILE *fp = fopen(path, "wb"); if (!fp) return -1;
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info = png ? png_create_info_struct(png) : NULL;
    if (!png || !info || setjmp(png_jmpbuf(png))) { fclose(fp); return -1; }
    png_init_io(png, fp);
    png_set_IHDR(png, info, (png_uint_32)w, (png_uint_32)h, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);
    for (int y = 0; y < h; y++) png_write_row(png, (png_bytep)(rgb + (size_t)y * (size_t)w * 3));
    png_write_end(png, NULL); png_destroy_write_struct(&png, &info); fclose(fp);
    return 0;
}

/* ---------- integer noise ---------- */
static uint32_t mix32(uint32_t a) { a ^= a >> 16; a *= 0x7feb352dU; a ^= a >> 15; a *= 0x846ca68bU; a ^= a >> 16; return a; }
static int lattice(uint32_t seed, int ix, int iy, int salt) { return (int)(mix32(seed ^ mix32((uint32_t)ix * 73856093U ^ (uint32_t)iy * 19349663U ^ (uint32_t)salt * 83492791U)) % 1001U); }
/* smooth value noise 0..1000 at cell (x,y): bilinear between hashed lattice points `sc` cells apart */
static int vnoise(uint32_t seed, int x, int y, int sc, int salt) {
    if (sc < 1) sc = 1;
    int ix = x / sc, iy = y / sc, fx = (x % sc) * 1000 / sc, fy = (y % sc) * 1000 / sc;
    long a = lattice(seed, ix, iy, salt), b = lattice(seed, ix + 1, iy, salt), c = lattice(seed, ix, iy + 1, salt), d = lattice(seed, ix + 1, iy + 1, salt);
    long top = a * (1000 - fx) + b * fx, bot = c * (1000 - fx) + d * fx;
    return (int)((top * (1000 - fy) + bot * fy) / 1000000);
}
static int fdiv(int a, int b) { int q = a / b; if ((a % b != 0) && ((a < 0) != (b < 0))) q--; return q; }
static int jitter_at(uint32_t seed, int x, int y, int sc, int salt, int jit) { if (jit <= 0) return 0; int n = vnoise(seed, x, y, sc, salt); return fdiv((n - 500) * jit * 2 + 500, 1000); }
static int isqrt(long v) { long r = 0; while ((r + 1) * (r + 1) <= v) r++; return (int)r; }

static char glyph_for(int H) { if (H == -2) return 'a'; if (H == -1) return 'b'; if (H == 0) return 'c'; if (H >= 1 && H <= 9) return (char)('0' + H); if (H >= 10 && H <= 35) return (char)('A' + H - 10); return 0; }
static int h_of_glyph(char g, const int *ext /* 128 entries, 0 = no row */) {
    if (g == '.') return -1000;
    int v = ext[(unsigned char)g & 127];
    if (v == 0) return 1;
    return v > 0 ? v : v + 1;
}

/* ---------- desk ---------- */
typedef struct { int w, h, tile_px, cols, tiles, rock; int *fl, *wl, *dp; char *gl; int ext[128]; unsigned char *atlas; size_t atlas_n; } Desk;

static int load_desk(Desk *d, const char *dir, int want_atlas) {
    memset(d, 0, sizeof *d); char p[2048];
    snprintf(p, sizeof p, "%s/cells.txt", dir); size_t n; char *ct = slurp(p, &n); if (!ct) { fprintf(stderr, "rmmc_biome_op: cannot read %s\n", p); return 3; }
    char *save = NULL; int in_cells = 0, row = 0;
    for (char *l = strtok_r(ct, "\n", &save); l; l = strtok_r(NULL, "\n", &save)) {
        if (!in_cells) {
            if (!strncmp(l, "width=", 6)) d->w = atoi(l + 6); else if (!strncmp(l, "height=", 7)) d->h = atoi(l + 7);
            else if (!strncmp(l, "tile_px=", 8)) d->tile_px = atoi(l + 8); else if (!strncmp(l, "atlas_cols=", 11)) d->cols = atoi(l + 11);
            else if (!strncmp(l, "atlas_tiles=", 12)) d->tiles = atoi(l + 12); else if (!strncmp(l, "rock_slot=", 10)) d->rock = atoi(l + 10);
            else if (!strncmp(l, "cells", 5)) {
                if (d->w < 1 || d->h < 1 || d->w > 512 || d->h > 512) { free(ct); return 3; }
                d->fl = calloc((size_t)d->w * d->h, sizeof(int)); d->wl = calloc((size_t)d->w * d->h, sizeof(int)); d->dp = calloc((size_t)d->w * d->h, sizeof(int));
                in_cells = 1;
            }
            continue;
        }
        if (row >= d->h) break;
        const char *q = l;
        for (int x = 0; x < d->w; x++) {
            char *e; long a = strtol(q, &e, 10); if (e == q || *e != ',') break; q = e + 1;
            long b = strtol(q, &e, 10); q = e; long c = 0;
            if (*q == ',') { c = strtol(q + 1, &e, 10); q = e; }
            while (*q == ',') { strtol(q + 1, &e, 10); q = e; }   /* any extra numbers (a second run) are not ours */
            while (*q == ' ') q++;
            size_t i = (size_t)row * d->w + x; d->fl[i] = (int)a; d->wl[i] = (int)b; d->dp[i] = (int)c;
        }
        row++;
    }
    free(ct);
    if (!in_cells || row < d->h) { fprintf(stderr, "rmmc_biome_op: %s has %d rows, want %d\n", p, row, d->h); return 3; }
    d->gl = malloc((size_t)d->w * d->h); memset(d->gl, '.', (size_t)d->w * d->h);
    snprintf(p, sizeof p, "%s/map.txt", dir); char *mt = slurp(p, &n);
    if (mt) { char *a = mt; for (int y = 0; y < d->h && *a; y++) { char *e = strchr(a, '\n'); size_t len = e ? (size_t)(e - a) : strlen(a); for (size_t x = 0; x < len && (int)x < d->w; x++) d->gl[(size_t)y * d->w + x] = a[x]; a = e ? e + 1 : a + len; } free(mt); }
    snprintf(p, sizeof p, "%s/extrusion.pdl", dir); char *ex = slurp(p, &n);
    if (ex) {
        char *sv = NULL;
        for (char *l = strtok_r(ex, "\n", &sv); l; l = strtok_r(NULL, "\n", &sv)) {
            if (!strstr(l, "EXTRUDE")) continue;
            char *b1 = strchr(l, '|'); if (!b1) continue; b1++; while (*b1 == ' ') b1++;
            char g = *b1; char *b2 = strchr(b1, '|'); if (!b2 || g < 33 || g > 126) continue;
            d->ext[(unsigned char)g] = atoi(b2 + 1);
        }
        free(ex);
    }
    if (want_atlas) {
        snprintf(p, sizeof p, "%s/cells.rgba", dir); d->atlas = (unsigned char *)slurp(p, &d->atlas_n);
        if (!d->atlas) { fprintf(stderr, "rmmc_biome_op: cannot read %s\n", p); return 3; }
    }
    return 0;
}
static int cell_h(const Desk *d, int x, int y) { return h_of_glyph(d->gl[(size_t)y * d->w + x], d->ext); }
static int is_land(const Desk *d, int x, int y) { if (x < 0 || y < 0 || x >= d->w || y >= d->h) return 0; size_t i = (size_t)y * d->w + x; return d->fl[i] != 0 || d->wl[i] != 0; }

/* ---------- atlas ---------- */
static int find_asset_root(const char *biomes_path, char *out, size_t cap) {
    const char *envh = getenv("RMMC_HOUSE"); char cand[4300];
    if (envh && *envh) { snprintf(cand, sizeof cand, "%s/shared/MINECLONIA-ASSET-SOURCE-LOCATION.pdl", envh); if (access(cand, R_OK) == 0) goto found_at; }
    {
        char real[PATH_MAX]; if (!realpath(biomes_path, real)) return -1;
        for (;;) {
            char *cut = strrchr(real, '/'); if (!cut) return -1; *cut = 0;
            snprintf(cand, sizeof cand, "%s/shared/MINECLONIA-ASSET-SOURCE-LOCATION.pdl", real);
            if (access(cand, R_OK) == 0) goto found_at;
            if (!real[0]) return -1;
        }
    }
found_at:;
    FILE *f = fopen(cand, "r"); if (!f) return -1; char line[2048]; int ok = -1;
    while (fgets(line, sizeof line, f)) {
        char *fl[6]; trim(line); if (line[0] == '#') continue; int n = split(line, fl, 6);
        if (n >= 3 && !strcmp(fl[0], "SOURCE") && !strcmp(fl[1], "img_root")) { snprintf(out, cap, "%s", fl[2]); ok = 0; break; }
    }
    fclose(f); return ok;
}
static void sample(const unsigned char *im, int w, int h, int x, int y, int px, unsigned char *o) {
    int sx = x * w / px, sy = y * w / px; if (sx >= w) sx = w - 1; if (sy >= h) sy = h - 1;
    memcpy(o, im + ((size_t)sy * w + sx) * 4, 4);
}
/* one tile at slot s of the atlas; returns 1 when a PNG was used, 0 for the flat fallback */
static int paint_tile(const Tile *t, const char *root, unsigned char *atlas, int aw, int px) {
    int cols = 16, tx = (t->slot % cols) * px, ty = (t->slot / cols) * px; int w = 0, h = 0, ow = 0, oh = 0; char p[2400]; int used = 0;
    unsigned char *im = NULL, *ov = NULL;
    if (root[0]) { snprintf(p, sizeof p, "%s/%s", root, t->png); im = load_png(p, &w, &h); }
    if (!im) fprintf(stderr, "rmmc_biome_op: MISSING asset %s/%s: tile %s is a flat colour #%06x\n", root, t->png, t->name, t->fallback);
    else used = 1;
    if (t->overlay[0] && root[0]) { snprintf(p, sizeof p, "%s/%s", root, t->overlay); ov = load_png(p, &ow, &oh); if (!ov) fprintf(stderr, "rmmc_biome_op: MISSING asset %s/%s (overlay of %s)\n", root, t->overlay, t->name); }
    unsigned tr = (t->tint >> 16) & 255, tg = (t->tint >> 8) & 255, tb = t->tint & 255;
    for (int y = 0; y < px; y++) for (int x = 0; x < px; x++) {
        unsigned char c[4] = { (unsigned char)((t->fallback >> 16) & 255), (unsigned char)((t->fallback >> 8) & 255), (unsigned char)(t->fallback & 255), 255 };
        if (im) { sample(im, w, h, x, y, px, c); c[3] = 255; c[0] = (unsigned char)(c[0] * tr / 255); c[1] = (unsigned char)(c[1] * tg / 255); c[2] = (unsigned char)(c[2] * tb / 255); }
        if (ov) { unsigned char o[4]; sample(ov, ow, oh, x, y, px, o); unsigned a = o[3]; unsigned char oc[3] = { (unsigned char)(o[0] * tr / 255), (unsigned char)(o[1] * tg / 255), (unsigned char)(o[2] * tb / 255) };
            for (int k = 0; k < 3; k++) c[k] = (unsigned char)((c[k] * (255 - a) + oc[k] * a) / 255); }
        memcpy(atlas + ((size_t)(ty + y) * aw + (tx + x)) * 4, c, 4);
    }
    free(im); free(ov); return used;
}

/* ---------- build ---------- */
static int cmp_int(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }

static int build(const char *biomes_path, const char *dir) {
    Book *bk = calloc(1, sizeof *bk); int rc = load_book(bk, biomes_path, 0); if (rc) return rc;
    if (bk->nb == 0) return bad("no BIOME rows in", biomes_path);
    Desk d; rc = load_desk(&d, dir, 0); if (rc) return rc;
    if (d.w > 256 || d.h > 256) { fprintf(stderr, "rmmc_biome_op: board %dx%d is over 256 cells (engine limit)\n", d.w, d.h); return 5; }
    int W = d.w, Hh = d.h; size_t N = (size_t)W * Hh;
    uint32_t seed = (uint32_t)strtoul(rule(bk, "seed", "1"), NULL, 10); int sc = atoi(rule(bk, "noise_scale", "6")); int px = atoi(rule(bk, "tile_px", "16"));
    if (px < 4 || px > 48) return bad("RULE tile_px must be 4..48", rule(bk, "tile_px", ""));
    int rock = tile_slot(bk, rule(bk, "rock_tile", "")); if (rock < 1) return bad("RULE rock_tile is not a TILE name", rule(bk, "rock_tile", ""));
    int trunk = tile_slot(bk, rule(bk, "tree_trunk", "")), leaves = tile_slot(bk, rule(bk, "tree_leaves", ""));
    if (bk->ntr && (trunk < 1 || leaves < 1)) return bad("RULE tree_trunk / tree_leaves are not TILE names", "");

    /* the outline and the distance from the outline (8-neighbour BFS in passes; land cell next to sky = 1) */
    unsigned char *land = calloc(N, 1); int *dist = calloc(N, sizeof(int));
    for (int y = 0; y < Hh; y++) for (int x = 0; x < W; x++) land[(size_t)y * W + x] = (unsigned char)is_land(&d, x, y);
    for (size_t i = 0; i < N; i++) dist[i] = land[i] ? 9999 : 0;
    for (int pass = 0; pass < 400; pass++) {
        int ch = 0;
        for (int y = 0; y < Hh; y++) for (int x = 0; x < W; x++) {
            size_t i = (size_t)y * W + x; if (!land[i]) continue; int m = dist[i];
            for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
                int nx = x + dx, ny = y + dy; int nd = (nx < 0 || ny < 0 || nx >= W || ny >= Hh) ? 0 : dist[(size_t)ny * W + nx];
                if (nd + 1 < m) m = nd + 1;
            }
            if (m < dist[i]) { dist[i] = m; ch = 1; }
        }
        if (!ch) break;
    }

    int *H = calloc(N, sizeof(int)), *top = calloc(N, sizeof(int)), *side = calloc(N, sizeof(int)); int *inb = calloc(N, sizeof(int));
    (void)inb;
    for (int bi = 0; bi < bk->nb; bi++) {
        Biome *b = &bk->b[bi];
        int tt = tile_slot(bk, b->top), ts = tile_slot(bk, b->side), tc = strcmp(b->cap, "-") ? tile_slot(bk, b->cap) : 0;
        if (tt < 1 || ts < 1 || (strcmp(b->cap, "-") && tc < 1)) return bad("BIOME names an unknown TILE", b->name);
        int capfrom = INT_MAX;
        if (strcmp(b->capfrom, "-")) { const char *rv = rule(bk, b->capfrom, NULL); capfrom = atoi(rv ? rv : b->capfrom); }
        int cone = !strcmp(b->mode, "cone");
        for (int y = 0; y < Hh; y++) for (int x = 0; x < W; x++) {
            size_t i = (size_t)y * W + x; if (!land[i]) continue;
            long d2 = 0; int inside = 0;
            if (!strcmp(b->shape, "all")) inside = 1;
            else if (!strcmp(b->shape, "rim")) inside = dist[i] <= b->rx;
            else if (!strcmp(b->shape, "rect")) inside = abs(x - b->cx) <= b->rx && abs(y - b->cy) <= b->ry;
            else { long dxs = (long)(x - b->cx) * 1000 / b->rx, dys = (long)(y - b->cy) * 1000 / b->ry; d2 = dxs * dxs + dys * dys; inside = d2 <= 1000000L; }
            if (!inside) continue;
            int h, j = jitter_at(seed, x, y, sc, bi + 1, b->jitter);
            if (cone) {
                int dd = isqrt(d2); h = b->base + (b->peak - b->base) * (1000 - dd) / 1000;
                if (h <= b->base) continue;
                h += j; if (h < b->base) h = b->base;
                if (h <= b->base || h <= H[i]) continue;
            } else h = b->base + j;
            H[i] = h; top[i] = (tc && h >= capfrom) ? tc : tt; side[i] = ts;
        }
    }
    int tree_events = !strcmp(rule(bk, "tree_mode", "columns"), "events");
    for (int ti = 0; ti < bk->ntr; ti++) {
        Tree *t = &bk->tr[ti];
        if (t->cx < 0 || t->cy < 0 || t->cx >= W || t->cy >= Hh || !land[(size_t)t->cy * W + t->cx]) continue;
        int ground = H[(size_t)t->cy * W + t->cx];
        if (tree_events) continue;     /* RULE tree_mode events: the tree is an EVENT (tree:<trunk>:<crown_r>:<crown_h>, the engine draws a trunk and a floating crown), the ground stays as painted */
        for (int dy = -t->crown_r; dy <= t->crown_r; dy++) for (int dx = -t->crown_r; dx <= t->crown_r; dx++) {
            int x = t->cx + dx, y = t->cy + dy; if (x < 0 || y < 0 || x >= W || y >= Hh || !land[(size_t)y * W + x]) continue;
            size_t i = (size_t)y * W + x;
            if (dx == 0 && dy == 0) { H[i] = ground + t->trunk; top[i] = leaves; side[i] = trunk; }
            else { H[i] = ground + t->crown_h; top[i] = leaves; side[i] = leaves; }
        }
    }
    /* clamp: a column must keep at least one block (top above the hung bottom), the engine takes 255 levels in all */
    int maxH = 0, zoff = 0;
    for (size_t i = 0; i < N; i++) if (land[i]) { if (d.dp[i] > zoff) zoff = d.dp[i]; }
    for (size_t i = 0; i < N; i++) if (land[i]) {
        int dep = d.dp[i]; if (H[i] < 1 - dep) H[i] = 1 - dep;
        if (H[i] > maxH) maxH = H[i];
        if (!glyph_for(H[i])) { fprintf(stderr, "rmmc_biome_op: height %d at cell %d,%d is outside the glyph table (-2..35)\n", H[i], (int)(i % W), (int)(i / W)); return 5; }
        if (H[i] + zoff > 255) { fprintf(stderr, "rmmc_biome_op: height %d + underside %d is over 255\n", H[i], zoff); return 5; }
    }

    /* the atlas */
    int maxslot = rock; for (int i = 0; i < bk->nt; i++) if (bk->t[i].slot > maxslot) maxslot = bk->t[i].slot;
    int tiles = maxslot + 1, aw = 16 * px, ah = ((tiles + 15) / 16) * px; size_t an = (size_t)aw * ah * 4;
    unsigned char *atlas = calloc(an, 1); char root[2048] = ""; int have_root = find_asset_root(biomes_path, root, sizeof root) == 0;
    if (!have_root) fprintf(stderr, "rmmc_biome_op: MISSING asset root (shared/MINECLONIA-ASSET-SOURCE-LOCATION.pdl not found): every tile is a flat colour\n");
    int from_png = 0; for (int i = 0; i < bk->nt; i++) from_png += paint_tile(&bk->t[i], have_root ? root : "", atlas, aw, px);

    /* text files */
    char *cells = malloc(N * 16 + 4096), *map = malloc(N + (size_t)Hh + 16); size_t cn = 0, mn = 0;
    cn += (size_t)sprintf(cells + cn, "width=%d\nheight=%d\ntile_px=%d\natlas_cols=16\natlas_tiles=%d\nrock_slot=%d\nevent_lift=1\nfurniture_cards=0\ncells\n", W, Hh, px, tiles, rock);
    int used[128]; memset(used, 0, sizeof used);
    for (int y = 0; y < Hh; y++) {
        for (int x = 0; x < W; x++) {
            size_t i = (size_t)y * W + x;
            if (!land[i]) { cn += (size_t)sprintf(cells + cn, "%s0,0,0", x ? " " : ""); map[mn++] = '.'; continue; }
            cn += (size_t)sprintf(cells + cn, "%s%d,%d,%d", x ? " " : "", top[i], side[i], d.dp[i]);
            char g = glyph_for(H[i]); map[mn++] = g; used[(unsigned char)g] = 1;
        }
        cells[cn++] = '\n'; map[mn++] = '\n';
    }
    char ext[8192]; size_t en = 0; char dd[512]; snprintf(dd, sizeof dd, "%s", dir);
    while (strlen(dd) > 1 && dd[strlen(dd) - 1] == '/') dd[strlen(dd) - 1] = 0;
    const char *desk_id = strrchr(dd, '/') ? strrchr(dd, '/') + 1 : dd; char mp[512]; snprintf(mp, sizeof mp, "%s", dd); { char *s = strrchr(mp, '/'); if (s) *s = 0; }
    const char *map_id = strrchr(mp, '/') ? strrchr(mp, '/') + 1 : mp;
    en += (size_t)sprintf(ext + en, "SECTION      | KEY                | VALUE\n----------------------------------------------------------------------\nMETA         | map_id             | %s\nMETA         | desk_id            | %s\n", map_id, desk_id);
    for (int Hv = -2; Hv <= 35; Hv++) { char g = glyph_for(Hv); if (!used[(unsigned char)g]) continue; en += (size_t)sprintf(ext + en, "EXTRUDE      | %c                  | %d\n", g, Hv >= 1 ? Hv : Hv - 1); }
    char leg[8192]; size_t ln = 0;
    ln += (size_t)sprintf(leg + ln, "# legend of map.txt (written by rmmc_biome_op from biomes.pdl; do not edit by hand)\n# glyph = the cell's height H in levels above the floor level. '.' = no land.\n");
    ln += (size_t)sprintf(leg + ln, "# glyph | H | engine extrusion value\n");
    for (int Hv = -2; Hv <= 35; Hv++) { char g = glyph_for(Hv); if (used[(unsigned char)g]) ln += (size_t)sprintf(leg + ln, "GLYPH | %c | %d | %d\n", g, Hv, Hv >= 1 ? Hv : Hv - 1); }
    ln += (size_t)sprintf(leg + ln, "# atlas slot | tile name | source png\n");
    for (int i = 0; i < bk->nt; i++) ln += (size_t)sprintf(leg + ln, "SLOT | %d | %s | %s\n", bk->t[i].slot, bk->t[i].name, bk->t[i].png);
    ln += (size_t)sprintf(leg + ln, "SLOT | rock_slot | %s | blocks hung under the island\n", rule(bk, "rock_tile", ""));

    /* old marker size, then write everything (after the whole build) */
    char p[2400]; snprintf(p, sizeof p, "%s/cells.marker", dir); struct stat st; long msz = stat(p, &st) == 0 ? (long)st.st_size : 0; long nm = (msz % 4000) + 1;
    struct { const char *name; const void *data; size_t n; } out[] = { {"cells.txt", cells, cn}, {"map.txt", map, mn}, {"extrusion.pdl", ext, en}, {"legend.txt", leg, ln}, {"cells.rgba", atlas, an} };
    for (size_t k = 0; k < sizeof out / sizeof out[0]; k++) {
        snprintf(p, sizeof p, "%s/%s", dir, out[k].name); FILE *f = fopen(p, "wb"); if (!f) { fprintf(stderr, "rmmc_biome_op: cannot write %s\n", p); return 3; }
        fwrite(out[k].data, 1, out[k].n, f); fclose(f);
    }
    snprintf(p, sizeof p, "%s/events.txt", dir); FILE *f = fopen(p, "wb");
    if (f && tree_events) {        /* one event row per TREE on land: `x y r g b tree:<trunk>:<crown_r>:<crown_h> 0 0 0` (r g b = the leaf colour, RULE tree_leaf_rgb as hex); actors are added by the game, never here */
        unsigned lv = 0x4f9a34; sscanf(rule(bk, "tree_leaf_rgb", "4f9a34"), "%x", &lv);
        for (int ti = 0; ti < bk->ntr; ti++) { Tree *t = &bk->tr[ti]; if (t->cx < 0 || t->cy < 0 || t->cx >= W || t->cy >= Hh || !land[(size_t)t->cy * W + t->cx]) continue;
            fprintf(f, "%d %d %u %u %u tree:%d:%d:%d 0 0 0\n", t->cx, t->cy, (lv >> 16) & 255, (lv >> 8) & 255, lv & 255, t->trunk, t->crown_r, t->crown_h); } }
    if (f) fclose(f);
    snprintf(p, sizeof p, "%s/cells.marker", dir); f = fopen(p, "wb"); if (f) { for (long i = 0; i < nm; i++) fputc('1', f); fclose(f); }
    int landn = 0; for (size_t i = 0; i < N; i++) landn += land[i];
    int hist[64] = {0}; (void)hist; (void)cmp_int;
    printf("rmmc_biome: %dx%d land=%d maxH=%d zoff=%d tiles=%d (from png %d) marker=%ld biomes=%d trees=%d\n", W, Hh, landn, maxH, zoff, tiles, from_png, nm, bk->nb, bk->ntr);
    return 0;
}

/* ---------- check ---------- */
static void emit(const char *want, const char *key, int v) { if (!want) printf("%s %d\n", key, v); else if (!strcmp(want, key)) printf("%d\n", v); }
static int check(const char *biomes_path, const char *dir, const char *srcdir, const char *want) {
    Book *bk = calloc(1, sizeof *bk); int rc = load_book(bk, biomes_path, 0); if (rc) return rc;
    Desk d; rc = load_desk(&d, dir, 1); if (rc) return rc;
    int W = d.w, Hh = d.h; size_t N = (size_t)W * Hh;
    int s_grass = tile_slot(bk, "grass_top"), s_sand = tile_slot(bk, "sand"), s_snow = tile_slot(bk, "snow"), s_w1 = tile_slot(bk, "water"), s_w2 = tile_slot(bk, "water_deep"); int s_stone = tile_slot(bk, "stone");
    int snow_line = atoi(rule(bk, "snow_line", "0"));
    int land = 0, maxh = -1000, mount_h = -1000, hist[300]; memset(hist, 0, sizeof hist);
    for (int y = 0; y < Hh; y++) for (int x = 0; x < W; x++) if (is_land(&d, x, y)) {
        land++; int h = cell_h(&d, x, y); if (h > maxh) maxh = h; if ((d.fl[(size_t)y * W + x] == s_stone || d.fl[(size_t)y * W + x] == s_snow) && h > mount_h) mount_h = h;
        if (d.fl[(size_t)y * W + x] == s_grass && h >= 0 && h < 300) hist[h]++;
    }
    int grass_h = 0, best = 0; for (int h = 0; h < 300; h++) if (hist[h] > best) { best = hist[h]; grass_h = h; }
    int lake = 0, lake_bad = 0, osand = 0, snow = 0, snow_low = 0, slot_bad = 0, glyph_bad = 0; int slots_used[256]; memset(slots_used, 0, sizeof slots_used);
    int dx4[4] = { 1, -1, 0, 0 }, dy4[4] = { 0, 0, 1, -1 };
    for (int y = 0; y < Hh; y++) for (int x = 0; x < W; x++) {
        size_t i = (size_t)y * W + x; if (!is_land(&d, x, y)) { if (d.gl[i] != '.') glyph_bad++; continue; }
        int f = d.fl[i], w = d.wl[i], h = cell_h(&d, x, y);
        if (d.gl[i] == '.') glyph_bad++;
        for (int k = 0; k < 2; k++) { int s = k ? w : f; if (!s) continue; if (s >= 0 && s < 256) slots_used[s] = 1;
            int ok = s > 0 && s < d.tiles;
            if (ok) { int px = d.tile_px, cols = d.cols, tx = (s % cols) * px, ty = (s / cols) * px, any = 0; int aw = cols * px;
                for (int yy = 0; yy < px && !any; yy++) for (int xx = 0; xx < px; xx++) { size_t o = (((size_t)(ty + yy) * aw) + (size_t)(tx + xx)) * 4 + 3; if (o < d.atlas_n && d.atlas[o] > 128) { any = 1; break; } }
                ok = any; }
            if (!ok) slot_bad++; }
        if (d.dp[i] < 0) glyph_bad++;
        int iswater = f == s_w1 || f == s_w2;
        if (iswater) {
            lake++;
            for (int k = 0; k < 4; k++) { int nx = x + dx4[k], ny = y + dy4[k]; if (!is_land(&d, nx, ny)) continue; size_t j = (size_t)ny * W + nx; int nf = d.fl[j]; if (nf == s_w1 || nf == s_w2) continue; if (cell_h(&d, nx, ny) <= h) { lake_bad++; break; } }
        }
        if (f == s_sand) { for (int k = 0; k < 4; k++) if (!is_land(&d, x + dx4[k], y + dy4[k])) { osand++; break; } }
        if (f == s_snow) { snow++; if (h < snow_line) snow_low++; }
    }
    int rk = d.rock; if (rk < 1 || rk >= d.tiles) slot_bad++;
    int nused = 0; for (int s = 0; s < 256; s++) nused += slots_used[s];
    int diff = 0, srcland = -1;
    if (srcdir) { Desk s; rc = load_desk(&s, srcdir, 0); if (rc) return rc; if (s.w != W || s.h != Hh) diff = (int)N; else { srcland = 0; for (int y = 0; y < Hh; y++) for (int x = 0; x < W; x++) { int a = is_land(&s, x, y), b = is_land(&d, x, y); srcland += a; if (a != b) diff++; } } }
    emit(want, "land", land); emit(want, "max_h", maxh); emit(want, "mount_h", mount_h); emit(want, "grass_h", grass_h); emit(want, "lake_cells", lake); emit(want, "lake_bad", lake_bad);
    emit(want, "outline_sand", osand); emit(want, "snow_cells", snow); emit(want, "snow_low", snow_low); emit(want, "snow_line", snow_line);
    emit(want, "slot_bad", slot_bad); emit(want, "slots_used", nused); emit(want, "glyph_bad", glyph_bad);
    if (srcdir) { emit(want, "outline_diff", diff); emit(want, "src_land", srcland); }
    return 0;
}

/* ---------- png ---------- */
static int picture(const char *dir, const char *outp, int ppc) {
    Desk d; int rc = load_desk(&d, dir, 1); if (rc) return rc;
    if (ppc < 1) ppc = 4;
    if (ppc > 16) ppc = 16;
    int W = d.w * ppc, Hh = d.h * ppc; unsigned char *img = calloc((size_t)W * Hh, 3); int px = d.tile_px, aw = d.cols * px;
    for (size_t i = 0; i < (size_t)W * Hh; i++) { img[i * 3] = 24; img[i * 3 + 1] = 28; img[i * 3 + 2] = 48; }   /* sky */
    for (int y = 0; y < d.h; y++) for (int x = 0; x < d.w; x++) {
        if (!is_land(&d, x, y)) continue;
        size_t i = (size_t)y * d.w + x; int s = d.fl[i] ? d.fl[i] : d.wl[i]; int h = cell_h(&d, x, y);
        int hn = (x > 0 && y > 0 && is_land(&d, x - 1, y - 1)) ? cell_h(&d, x - 1, y - 1) : h;
        int he = (x + 1 < d.w && y + 1 < d.h && is_land(&d, x + 1, y + 1)) ? cell_h(&d, x + 1, y + 1) : h;
        int shade = (h - hn) * 14 - (h - he) * 6 + (h > 0 ? h * 2 : h * 6);
        for (int yy = 0; yy < ppc; yy++) for (int xx = 0; xx < ppc; xx++) {
            int tx = (s % d.cols) * px + xx * px / ppc + px / (2 * ppc), ty = (s / d.cols) * px + yy * px / ppc + px / (2 * ppc);
            size_t o = ((size_t)ty * aw + (size_t)tx) * 4; unsigned char *c = &img[((size_t)(y * ppc + yy) * W + (x * ppc + xx)) * 3];
            for (int k = 0; k < 3; k++) { int v = (o + 3 < d.atlas_n ? d.atlas[o + k] : 128) + shade; c[k] = (unsigned char)(v < 0 ? 0 : v > 255 ? 255 : v); }
        }
    }
    if (write_png(outp, img, W, Hh)) { fprintf(stderr, "rmmc_biome_op: cannot write %s\n", outp); return 3; }
    printf("png %s %dx%d\n", outp, W, Hh);
    return 0;
}

int main(int argc, char **argv) {
    if (argc >= 4 && !strcmp(argv[1], "check")) return check(argv[2], argv[3], (argc >= 5 && strcmp(argv[4], "-")) ? argv[4] : NULL, argc >= 6 ? argv[5] : NULL);
    if (argc >= 4 && !strcmp(argv[1], "png")) return picture(argv[2], argv[3], argc >= 5 ? atoi(argv[4]) : 4);
    if (argc == 3) return build(argv[1], argv[2]);
    fprintf(stderr, "usage: rmmc_biome_op <biomes.pdl> <desk_dir> | check <biomes.pdl> <desk_dir> [<src_desk>] | png <desk_dir> <out.png> [px_per_cell]\n");
    return 2;
}
