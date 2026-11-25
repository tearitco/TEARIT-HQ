/* earth-moth page earth: a full ball of the ETOPO world.
 * The desk is a projection. The ledger is the player and the animals.
 * Config (not a ledger): #.NNEST_ASSETS/rta-sources/earth-moth/ETOPO5.DAT
 * Longitude runs across the disc and latitude runs up it, so Africa,
 * the Americas, and Australia are on the face you walk. Column height
 * and the hung underside are the same radius, so the body is a sphere.
 * Page backup is the older ringed dome and is not written here.
 */
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <png.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Radii come from @.apps/earth-moth/bodies.pdl (BODY | name | radius), read in
 * main() before any command. Found from $HOUSE, else by walking up from argv[0]. */
#define BODIES_REL "@.apps/earth-moth/bodies.pdl"
static double R_BALL, R_MOON, R_MARS;

static int locate_bodies(const char *argv0, char *out, size_t n) {
    const char *house = getenv("HOUSE");
    char real[PATH_MAX], *cut;
    if (house && *house) {
        snprintf(out, n, "%s/%s", house, BODIES_REL);
        if (access(out, R_OK) == 0) return 0;
    }
    if (!argv0 || !realpath(argv0, real)) return -1;
    for (;;) {
        cut = strrchr(real, '/');
        if (!cut) return -1;
        *cut = '\0';
        if (strlen(real) + sizeof BODIES_REL + 1 > n) return -1;
        snprintf(out, n, "%s/%s", real, BODIES_REL);
        if (access(out, R_OK) == 0) return 0;
        if (real[0] == '\0') return -1;
    }
}

/* Sets R_BALL (earth), R_MOON, R_MARS from BODY rows. Returns 0 only if all three exist. */
static int load_bodies(const char *argv0) {
    char path[PATH_MAX], line[256], name[32];
    FILE *f;
    int radius, got = 0;
    if (locate_bodies(argv0, path, sizeof path) != 0) return -1;
    f = fopen(path, "r");
    if (!f) return -1;
    while (fgets(line, sizeof line, f)) {
        char *p;
        if (line[0] == '#' || line[0] == '\n') continue;
        if (sscanf(line, " BODY | %31[^|] | %d", name, &radius) != 2) continue;
        p = name + strlen(name);
        while (p > name && (p[-1] == ' ' || p[-1] == '\t')) *--p = '\0';
        if (!strcmp(name, "earth")) { R_BALL = radius; got |= 1; }
        else if (!strcmp(name, "moon")) { R_MOON = radius; got |= 2; }
        else if (!strcmp(name, "mars")) { R_MARS = radius; got |= 4; }
    }
    fclose(f);
    return got == 7 ? 0 : -1;
}

#define N 200
#define ETW 4320
#define ETH 2160

enum { T_EMPTY = 0, T_WATER = 1, T_GRASS = 2, T_SAND = 3, T_SNOW = 4, T_LEAVES = 5, T_STONE = 6, T_DIRT = 7, T_REGOLITH = 8, T_DUST = 9 };

static const char *TILE_PNG[] = {
    "ITEMS/mcl_core/textures/default_water_source_animated.png",
    "ITEMS/mcl_core/textures/mcl_core_grass_block_top.png",
    "ITEMS/mcl_core/textures/default_sand.png",
    "ITEMS/mcl_core/textures/default_snow.png"
};
static const char *LEAVES_PNG = "ITEMS/mcl_core/textures/default_leaves.png";
static const char *STONE_PNG = "ITEMS/mcl_core/textures/default_stone.png";
static const char *DIRT_PNG = "ITEMS/mcl_core/textures/default_dirt.png";
static const char *REGOLITH_PNG = "ITEMS/mcl_core/textures/default_gravel.png";
static const char *DUST_PNG = "ITEMS/mcl_core/textures/mcl_core_red_sand.png";

static int nglyph;
static char gly[96];

static void init_gly(void) {
    int i;
    if (nglyph) return;
    for (i = 33; i < 127; i++) {
        if (i == '|' || i == '.') continue;
        gly[nglyph++] = (char)i;
    }
}

static char hglyph(int h) {
    init_gly();
    if (h < 1) h = 1;
    if (h > nglyph) h = nglyph;
    return gly[h - 1];
}

static int16_t *load_etopo(const char *path) {
    FILE *f = fopen(path, "rb");
    int16_t *e;
    size_t n = (size_t)ETW * ETH, i;
    if (!f) return NULL;
    e = malloc(n * sizeof(int16_t));
    if (!e) { fclose(f); return NULL; }
    if (fread(e, 2, n, f) != n) { free(e); fclose(f); return NULL; }
    fclose(f);
    for (i = 0; i < n; i++) {
        unsigned u = (unsigned)(uint16_t)e[i];
        e[i] = (int16_t)((u >> 8) | ((u & 0xff) << 8));
    }
    return e;
}

static int elev_at(const int16_t *e, double lat, double lon) {
    double east = lon < 0 ? lon + 360.0 : lon;
    int col, row;
    if (east >= 360.0) east = 359.999;
    if (lat > 89.9) lat = 89.9;
    if (lat < -89.9) lat = -89.9;
    col = (int)(east * 12.0);
    row = (int)((90.0 - lat) * 12.0);
    if (col < 0) col = 0;
    if (col >= ETW) col = ETW - 1;
    if (row < 0) row = 0;
    if (row >= ETH) row = ETH - 1;
    return e[(size_t)row * ETW + col];
}

static unsigned char *load_png(const char *path, int *w, int *h) {
    FILE *fp = fopen(path, "rb");
    png_structp png;
    png_infop info;
    png_byte ct;
    int y, rowb;
    unsigned char *img;
    png_bytep *rows;
    if (!fp) return NULL;
    png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) { fclose(fp); return NULL; }
    info = png_create_info_struct(png);
    if (!info || setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, info ? &info : NULL, NULL);
        fclose(fp);
        return NULL;
    }
    png_init_io(png, fp);
    png_read_info(png, info);
    *w = (int)png_get_image_width(png, info);
    *h = (int)png_get_image_height(png, info);
    ct = png_get_color_type(png, info);
    if (ct == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (ct == PNG_COLOR_TYPE_GRAY || ct == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_gray_to_rgb(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
    if (png_get_bit_depth(png, info) == 16) png_set_scale_16(png);
    if ((ct & PNG_COLOR_MASK_ALPHA) == 0) png_set_filler(png, 255, PNG_FILLER_AFTER);
    png_read_update_info(png, info);
    rowb = (int)png_get_rowbytes(png, info);
    img = malloc((size_t)(*h) * (size_t)rowb);
    rows = malloc((size_t)(*h) * sizeof(png_bytep));
    if (!img || !rows) { free(img); free(rows); png_destroy_read_struct(&png, &info, NULL); fclose(fp); return NULL; }
    for (y = 0; y < *h; y++) rows[y] = img + (size_t)y * (size_t)rowb;
    png_read_image(png, rows);
    free(rows);
    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);
    return img;
}

static void blit16(FILE *o, const char *root, const char *rel) {
    char path[1024];
    int w = 0, h = 0, y, x;
    unsigned char *im;
    snprintf(path, sizeof path, "%s/%s", root, rel);
    im = load_png(path, &w, &h);
    for (y = 0; y < 16; y++) for (x = 0; x < 16; x++) {
        int sx = im && w >= 16 ? x * w / 16 : 0;
        int sy = im && h >= 16 ? y * (h < 16 ? h : 16) / 16 : 0;
        if (im && sx < w && sy < h) {
            unsigned char *s = im + ((size_t)sy * (size_t)w + (size_t)sx) * 4;
            fputc(s[0], o); fputc(s[1], o); fputc(s[2], o); fputc(255, o);
        } else { fputc(80, o); fputc(80, o); fputc(80, o); fputc(255, o); }
    }
    free(im);
}

/* Whole world on the disc. The upper height and the hung depth are the
 * sphere radius at that cell, so the column is a chord of the ball. */
static int cell_of(const int16_t *e, int x, int y, int *lat_m, int *lon_m, int *hh, int *dep) {
    double cx = (N - 1) / 2.0, cy = (N - 1) / 2.0;
    double dx = x - cx, dy = cy - y, dist, z, lat, lon;
    int elev, tile, up, dn;
    dist = hypot(dx, dy);
    if (dist > R_BALL) { *hh = 0; if (dep) *dep = 0; return T_EMPTY; }
    z = sqrt(1.0 - (dist / R_BALL) * (dist / R_BALL));
    lon = (dx / R_BALL) * 180.0;
    lat = (dy / R_BALL) * 90.0;
    if (lat_m) *lat_m = (int)lround(lat);
    if (lon_m) *lon_m = (int)lround(lon);
    elev = elev_at(e, lat, lon);
    up = (int)lround(z * R_BALL);
    if (up < 1) up = 1;
    dn = up;
    if (elev <= 0) {
        tile = T_WATER;
        if (up > 6) up -= 5;
    } else if (lat > 66.0 || lat < -58.0 || elev > 2200) {
        tile = T_SNOW;
        if (elev > 4500 && up < 250) up += 3;
        else if (elev > 2200 && up < 250) up += 1;
    } else if (elev < 80) {
        tile = T_SAND;
    } else {
        tile = T_GRASS;
    }
    init_gly();
    if (up > nglyph) up = nglyph;
    if (dn > 255) dn = 255;
    *hh = up;
    if (dep) *dep = dn;
    return tile;
}

static void where(const char *ledger, int sx, int sy, int *x, int *y) {
    FILE *f = fopen(ledger, "r");
    char line[256];
    int a, b;
    *x = sx; *y = sy;
    if (!f) return;
    while (fgets(line, sizeof line, f))
        if (sscanf(line, "MOVE | %d | %d", &a, &b) == 2) { *x = a; *y = b; }
    fclose(f);
}

static int is_tree(int x, int y, int tile) {
    return tile == T_GRASS && ((x * 13 + y * 7) % 67) == 0;
}

static int grass_open(const int16_t *e, int x, int y) {
    int h, tile;
    if (x < 0 || y < 0 || x >= N || y >= N) return 0;
    tile = cell_of(e, x, y, NULL, NULL, &h, NULL);
    return (tile == T_GRASS || tile == T_SAND || tile == T_SNOW) && !is_tree(x, y, tile);
}

static int spawn_of(const int16_t *e, int *sx, int *sy) {
    int x, y, best = 1 << 30;
    *sx = N / 2; *sy = N / 2;
    for (y = 0; y < N; y++) for (x = 0; x < N; x++) {
        int lat, lon, h, tile = cell_of(e, x, y, &lat, &lon, &h, NULL);
        int d;
        if (tile != T_GRASS && tile != T_SAND) continue;
        if (is_tree(x, y, tile)) continue;
        d = abs(lat - 30) * 3 + abs(lon - 15);
        if (d < best) { best = d; *sx = x; *sy = y; }
    }
    return best < (1 << 30) ? 0 : 4;
}

static int pick_near(const int16_t *e, int cx, int cy, int n, int *ox, int *oy) {
    int r, seen = 0;
    for (r = 1; r < N; r++) {
        int y, x;
        for (y = cy - r; y <= cy + r; y++) for (x = cx - r; x <= cx + r; x++) {
            if (abs(x - cx) != r && abs(y - cy) != r) continue;
            if (!grass_open(e, x, y)) continue;
            if (seen == n) { *ox = x; *oy = y; return 0; }
            seen++;
        }
    }
    return 4;
}

static void last_of(const char *ledger, const char *kind, int *x, int *y, int *have) {
    FILE *f = fopen(ledger, "r");
    char line[256], pat[64];
    int a, b;
    *have = 0;
    if (!f) return;
    snprintf(pat, sizeof pat, "%s | %%d | %%d", kind);
    while (fgets(line, sizeof line, f))
        if (sscanf(line, pat, &a, &b) == 2) { *x = a; *y = b; *have = 1; }
    fclose(f);
}

static int land_ok(const int16_t *e, int x, int y) {
    return grass_open(e, x, y);
}

static void probe(const int16_t *e, const char *name, double lat, double lon) {
    double cx = (N - 1) / 2.0, cy = (N - 1) / 2.0;
    int x = (int)lround(cx + (lon / 180.0) * R_BALL);
    int y = (int)lround(cy - (lat / 90.0) * R_BALL);
    int h, tile = cell_of(e, x, y, NULL, NULL, &h, NULL);
    const char *tn = tile == T_WATER ? "water" : tile == T_GRASS ? "grass" :
        tile == T_SAND ? "sand" : tile == T_SNOW ? "snow" : tile == T_EMPTY ? "empty" : "other";
    printf("spot %s %d,%d %s h=%d\n", name, x, y, tn, h);
}

static int desk(const char *etopo, const char *mine, const char *ledger, const char *dir) {
    int16_t *e = load_etopo(etopo);
    FILE *f;
    char path[1100];
    int x, y, sx = N / 2, sy = N / 2, px, py;
    int nwater = 0, ngrass = 0, nsand = 0, nsnow = 0, ntree = 0;
    struct stat ms;
    if (!e) { printf("fail etopo\n"); return 5; }
    init_gly();
    if (mkdir(dir, 0755) != 0 && errno != EEXIST) { free(e); return 5; }
    spawn_of(e, &sx, &sy);
    where(ledger, sx, sy, &px, &py);
    if (!grass_open(e, px, py)) {
        px = sx; py = sy;
        if (!grass_open(e, px, py) && pick_near(e, sx, sy, 0, &px, &py)) {
            free(e); printf("fail land\n"); return 4;
        }
        {
            FILE *lf = fopen(ledger, "a");
            if (lf) { fprintf(lf, "MOVE | %d | %d | grass\n", px, py); fclose(lf); }
        }
    }
    {
        const char *kinds[3] = { "CREEPER", "PIG", "COW" };
        int k, ax, ay, have;
        for (k = 0; k < 3; k++) {
            last_of(ledger, kinds[k], &ax, &ay, &have);
            if (have && grass_open(e, ax, ay)) continue;
            if (pick_near(e, px, py, 2 + k * 3, &ax, &ay)) continue;
            {
                FILE *lf = fopen(ledger, "a");
                if (lf) { fprintf(lf, "%s | %d | %d\n", kinds[k], ax, ay); fclose(lf); }
            }
        }
    }
    snprintf(path, sizeof path, "%s/cells.txt", dir);
    f = fopen(path, "w");
    if (!f) { free(e); return 5; }
    fprintf(f, "width=%d\nheight=%d\ntile_px=16\natlas_cols=8\natlas_tiles=8\nevent_lift=1\ncells\n", N, N);
    for (y = 0; y < N; y++) {
        for (x = 0; x < N; x++) {
            int h, dep = 0, tile = cell_of(e, x, y, NULL, NULL, &h, &dep);
            if (tile == T_WATER) nwater++;
            else if (tile == T_GRASS) ngrass++;
            else if (tile == T_SAND) nsand++;
            else if (tile == T_SNOW) nsnow++;
            if (is_tree(x, y, tile)) { tile = T_LEAVES; ntree++; }
            /* Floor slot is the surface. Wall slot is stone: one column is
             * one solid run, so the Minecraft dirt band (the three cells
             * under the surface) cannot be its own voxels. The formula
             * verb prints that band. Sides read as the interior. */
            if (tile == T_EMPTY) fprintf(f, "%s0,0", x ? " " : "");
            else fprintf(f, "%s%d,%d,%d", x ? " " : "", tile, T_STONE, dep);
        }
        fputc('\n', f);
    }
    fclose(f);
    snprintf(path, sizeof path, "%s/cells.rgba", dir);
    f = fopen(path, "w");
    if (!f) { free(e); return 5; }
    for (y = 0; y < 16 * 16; y++) { fputc(0, f); fputc(0, f); fputc(0, f); fputc(0, f); }
    for (x = 0; x < 4; x++) blit16(f, mine, TILE_PNG[x]);
    blit16(f, mine, LEAVES_PNG);
    blit16(f, mine, STONE_PNG);
    blit16(f, mine, DIRT_PNG);
    fclose(f);
    snprintf(path, sizeof path, "%s/map.txt", dir);
    f = fopen(path, "w");
    if (!f) { free(e); return 5; }
    for (y = 0; y < N; y++) {
        for (x = 0; x < N; x++) {
            int h, tile = cell_of(e, x, y, NULL, NULL, &h, NULL);
            if (tile == T_EMPTY) { fputc('.', f); continue; }
            if (is_tree(x, y, tile) && h < nglyph) h += 8;
            fputc(hglyph(h), f);
        }
        fputc('\n', f);
    }
    fclose(f);
    snprintf(path, sizeof path, "%s/extrusion.pdl", dir);
    f = fopen(path, "w");
    if (!f) { free(e); return 5; }
    fputs("EXTRUDE | . | 1\n", f);
    for (x = 1; x <= nglyph; x++) fprintf(f, "EXTRUDE | %c | %d\n", hglyph(x), x);
    fclose(f);
    snprintf(path, sizeof path, "%s/events.txt", dir);
    f = fopen(path, "w");
    if (!f) { free(e); return 5; }
    fprintf(f, "%d %d 210 170 120 block 0 2 1\n", px, py);
    {
        const char *kinds[3] = { "CREEPER", "PIG", "COW" };
        const int col[3][3] = { {40, 140, 40}, {220, 150, 160}, {90, 60, 40} };
        int k, ax, ay, have;
        for (k = 0; k < 3; k++) {
            last_of(ledger, kinds[k], &ax, &ay, &have);
            if (have) fprintf(f, "%d %d %d %d %d block 0 2 1\n", ax, ay, col[k][0], col[k][1], col[k][2]);
        }
    }
    fclose(f);
    snprintf(path, sizeof path, "%s/view.pdl", dir);
    f = fopen(path, "w");
    if (f) { fprintf(f, "start | %d %d\n", px, py); fclose(f); }
    snprintf(path, sizeof path, "%s/cells.marker", dir);
    {
        int restart = (stat(path, &ms) == 0 && ms.st_size >= 4096);
        f = fopen(path, restart ? "w" : "a");
        if (f) { fputc('1', f); fclose(f); }
    }
    printf("desk=%s player=%d,%d spawn=%d,%d water=%d grass=%d sand=%d snow=%d trees=%d\n",
           dir, px, py, sx, sy, nwater, ngrass, nsand, nsnow, ntree);
    probe(e, "africa", 5, 20);
    probe(e, "samerica", -15, -60);
    probe(e, "namerica", 40, -100);
    probe(e, "australia", -25, 135);
    probe(e, "pacific", 0, -160);
    probe(e, "himalaya", 28, 87);
    free(e);
    return 0;
}

static int ensure_ledger(const char *etopo, const char *ledger) {
    struct stat st;
    int16_t *e;
    int sx, sy;
    FILE *f;
    if (stat(ledger, &st) == 0) return 0;
    e = load_etopo(etopo);
    if (!e) return 5;
    if (spawn_of(e, &sx, &sy)) { free(e); return 4; }
    f = fopen(ledger, "w");
    if (!f) { free(e); return 5; }
    fprintf(f, "MAP | earth-moth | etopo5 | %d | %d\n", N, N);
    fprintf(f, "SPAWN | %d | %d\n", sx, sy);
    fclose(f);
    free(e);
    printf("spawn=%d,%d\n", sx, sy);
    return 0;
}

static int step_op(const char *etopo, const char *ledger, const char *dir) {
    int16_t *e = load_etopo(etopo);
    int sx, sy, x, y, dx = 0, dy = 0, nx, ny;
    FILE *f;
    if (!e) return 5;
    if (ensure_ledger(etopo, ledger)) { free(e); return 5; }
    spawn_of(e, &sx, &sy);
    where(ledger, sx, sy, &x, &y);
    if (!strcmp(dir, "e")) dx = 1;
    else if (!strcmp(dir, "w")) dx = -1;
    else if (!strcmp(dir, "s")) dy = 1;
    else if (!strcmp(dir, "n")) dy = -1;
    else { free(e); printf("fail dir\n"); return 2; }
    nx = x + dx; ny = y + dy;
    /* Open land: grass, sand, or snow. Water and a tree refuse the step. */
    if (!grass_open(e, nx, ny)) { free(e); printf("fail step\n"); return 4; }
    f = fopen(ledger, "a");
    if (!f) { free(e); return 5; }
    fprintf(f, "MOVE | %d | %d | %s\n", nx, ny, dir);
    fclose(f);
    printf("step=%d,%d %s\n", nx, ny, dir);
    free(e);
    return 0;
}

static int animals_op(const char *etopo, const char *ledger) {
    int16_t *e = load_etopo(etopo);
    int sx, sy, x, y, i, n = 0;
    const int off[3][2] = { { 3, 1 }, { -2, 2 }, { 1, -3 } };
    const char *name[3] = { "CREEPER", "PIG", "COW" };
    FILE *f;
    if (!e) return 5;
    if (ensure_ledger(etopo, ledger)) { free(e); return 5; }
    f = fopen(ledger, "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof line, f)) if (!strncmp(line, "CREEPER |", 9)) n = 1;
        fclose(f);
    }
    if (n) { free(e); printf("animals=have\n"); return 0; }
    spawn_of(e, &sx, &sy);
    where(ledger, sx, sy, &x, &y);
    f = fopen(ledger, "a");
    if (!f) { free(e); return 5; }
    for (i = 0; i < 3; i++) {
        int ax = x + off[i][0], ay = y + off[i][1], k;
        for (k = 0; k < N && !land_ok(e, ax, ay); k++) { ax++; if (ax >= N) { ax = 0; ay++; } }
        if (!land_ok(e, ax, ay) || (ax == x && ay == y)) continue;
        fprintf(f, "%s | %d | %d\n", name[i], ax, ay);
        printf("%s=%d,%d\n", name[i], ax, ay);
    }
    fclose(f);
    free(e);
    return 0;
}

/* Radii come from bodies.pdl (same file as planet_ball_op). Moon and Mars have
 * no height grid on disk, so their relief is the chord. Earth on the solar page
 * still reads ETOPO. */
#define SOLAR_N 256

static int chord_tile(const int16_t *e, int x, int y, double cx, double cy, double R, int kind, int *hh, int *dep) {
    double dx = x - cx, dy = cy - y, dist, z, lat, lon;
    int elev, tile, up, dn;
    dist = hypot(dx, dy);
    if (dist > R) { *hh = 0; if (dep) *dep = 0; return T_EMPTY; }
    z = sqrt(1.0 - (dist / R) * (dist / R));
    up = (int)lround(z * R);
    if (up < 1) up = 1;
    dn = up;
    if (kind == 0 && e) {
        lon = (dx / R) * 180.0;
        lat = (dy / R) * 90.0;
        elev = elev_at(e, lat, lon);
        if (elev <= 0) { tile = T_WATER; if (up > 6) up -= 5; }
        else if (lat > 66.0 || lat < -58.0 || elev > 2200) tile = T_SNOW;
        else if (elev < 80) tile = T_SAND;
        else tile = T_GRASS;
    } else if (kind == 1) tile = T_REGOLITH;
    else tile = T_DUST;
    init_gly();
    if (up > nglyph) up = nglyph;
    if (dn > 255) dn = 255;
    *hh = up;
    if (dep) *dep = dn;
    return tile;
}

static int write_page(const char *dir, int n, const int16_t *e, const char *mine, int solar) {
    FILE *f;
    char path[1100];
    int x, y, sx = 0, sy = 0;
    int nearth = 0, nmoon = 0, nmars = 0;
    struct stat ms;
    const double ecx = 94.0, ecy = 94.0, mcx = 211.0, mcy = 94.0, acx = 206.0, acy = 206.0;
    if (mkdir(dir, 0755) != 0 && errno != EEXIST) return 5;
    snprintf(path, sizeof path, "%s/cells.txt", dir);
    f = fopen(path, "w");
    if (!f) return 5;
    fprintf(f, "width=%d\nheight=%d\ntile_px=16\natlas_cols=8\natlas_tiles=16\nevent_lift=1\ncells\n", n, n);
    for (y = 0; y < n; y++) {
        for (x = 0; x < n; x++) {
            int h = 0, dep = 0, tile = T_EMPTY;
            double cx, cy, R;
            int kind;
            if (!solar) {
                cx = (n - 1) / 2.0; cy = cx; R = (n > 80) ? R_MARS : R_MOON; kind = (n > 80) ? 2 : 1;
                tile = chord_tile(e, x, y, cx, cy, R, kind, &h, &dep);
            } else {
                double de = hypot(x - ecx, y - ecy), dm = hypot(x - mcx, y - mcy), da = hypot(x - acx, y - acy);
                if (de <= R_BALL && de <= dm && de <= da) { tile = chord_tile(e, x, y, ecx, ecy, R_BALL, 0, &h, &dep); nearth++; }
                else if (dm <= R_MOON && dm <= da) { tile = chord_tile(e, x, y, mcx, mcy, R_MOON, 1, &h, &dep); nmoon++; }
                else if (da <= R_MARS) { tile = chord_tile(e, x, y, acx, acy, R_MARS, 2, &h, &dep); nmars++; }
            }
            if (!solar && tile != T_EMPTY) { if (tile == T_REGOLITH) nmoon++; else nmars++; }
            if (tile == T_EMPTY) fprintf(f, "%s0,0", x ? " " : "");
            else fprintf(f, "%s%d,%d,%d", x ? " " : "", tile, T_STONE, dep);
            (void)sx; (void)sy;
        }
        fputc('\n', f);
    }
    fclose(f);
    if (!solar) { sx = n / 2; sy = n / 2; }
    else { sx = (int)ecx; sy = (int)ecy; }
    snprintf(path, sizeof path, "%s/cells.rgba", dir);
    f = fopen(path, "w");
    if (!f) return 5;
    for (y = 0; y < 16 * 16; y++) { fputc(0, f); fputc(0, f); fputc(0, f); fputc(0, f); }
    for (x = 0; x < 4; x++) blit16(f, mine, TILE_PNG[x]);
    blit16(f, mine, LEAVES_PNG);
    blit16(f, mine, STONE_PNG);
    blit16(f, mine, DIRT_PNG);
    blit16(f, mine, REGOLITH_PNG);
    blit16(f, mine, DUST_PNG);
    for (x = 10; x < 16; x++) for (y = 0; y < 16 * 16; y++) { fputc(0, f); fputc(0, f); fputc(0, f); fputc(0, f); }
    fclose(f);
    snprintf(path, sizeof path, "%s/map.txt", dir);
    f = fopen(path, "w");
    if (!f) return 5;
    for (y = 0; y < n; y++) {
        for (x = 0; x < n; x++) {
            int h = 0, tile;
            if (!solar) {
                double cx = (n - 1) / 2.0;
                tile = chord_tile(e, x, y, cx, cx, (n > 80) ? R_MARS : R_MOON, (n > 80) ? 2 : 1, &h, NULL);
            } else {
                double de = hypot(x - ecx, y - ecy), dm = hypot(x - mcx, y - mcy), da = hypot(x - acx, y - acy);
                if (de <= R_BALL && de <= dm && de <= da) tile = chord_tile(e, x, y, ecx, ecy, R_BALL, 0, &h, NULL);
                else if (dm <= R_MOON && dm <= da) tile = chord_tile(e, x, y, mcx, mcy, R_MOON, 1, &h, NULL);
                else if (da <= R_MARS) tile = chord_tile(e, x, y, acx, acy, R_MARS, 2, &h, NULL);
                else tile = T_EMPTY;
            }
            fputc(tile == T_EMPTY ? '.' : hglyph(h), f);
        }
        fputc('\n', f);
    }
    fclose(f);
    snprintf(path, sizeof path, "%s/extrusion.pdl", dir);
    f = fopen(path, "w");
    if (!f) return 5;
    fputs("EXTRUDE | . | 1\n", f);
    init_gly();
    for (x = 1; x <= nglyph; x++) fprintf(f, "EXTRUDE | %c | %d\n", hglyph(x), x);
    fclose(f);
    snprintf(path, sizeof path, "%s/events.txt", dir);
    f = fopen(path, "w");
    if (!f) return 5;
    fprintf(f, "%d %d 210 170 120 block 0 2 1\n", sx, sy);
    fclose(f);
    snprintf(path, sizeof path, "%s/view.pdl", dir);
    f = fopen(path, "w");
    if (f) { fprintf(f, "start | %d %d\n", sx, sy); fclose(f); }
    snprintf(path, sizeof path, "%s/cells.marker", dir);
    {
        int restart = (stat(path, &ms) == 0 && ms.st_size >= 4096);
        f = fopen(path, restart ? "w" : "a");
        if (f) { fputc('1', f); fclose(f); }
    }
    printf("PAGE %s spawn=%d,%d earth=%d moon=%d mars=%d\n", dir, sx, sy, nearth, nmoon, nmars);
    return 0;
}

static int pages_op(const char *etopo, const char *mine, const char *maps) {
    int16_t *e = load_etopo(etopo);
    char path[1100];
    int rc;
    if (!e) { printf("fail etopo\n"); return 5; }
    snprintf(path, sizeof path, "%s/moon", maps);
    rc = write_page(path, 64, e, mine, 0);
    if (rc) { free(e); return rc; }
    snprintf(path, sizeof path, "%s/mars", maps);
    rc = write_page(path, 112, e, mine, 0);
    if (rc) { free(e); return rc; }
    snprintf(path, sizeof path, "%s/solar", maps);
    rc = write_page(path, SOLAR_N, e, mine, 1);
    free(e);
    return rc;
}

/* Same chord as cell_of, and the chunk formula from pc_generate_chunk:
 * z == surface is the tile, the next three are dirt, the rest is stone.
 * A desk column cannot store that stack, so the wall slot is stone. */
static int formula_op(void) {
    double z = sqrt(1.0 - 0.0);
    int up = (int)lround(z * R_BALL);
    int surface = up;
    printf("FORMULA chord R=%d center up=%d dn=%d\n", (int)R_BALL, up, up);
    printf("FORMULA outside dist>%d empty\n", (int)R_BALL);
    printf("FORMULA ocean elev<=0 water dip=5\n");
    printf("FORMULA mc z==surface tile z>=surface-3 dirt else stone\n");
    printf("FORMULA example surface=%d dirt=%d,%d,%d stone=%d\n",
           surface, surface - 3, surface - 2, surface - 1, surface - 4);
    printf("FORMULA slots floor=surface wall=stone\n");
    printf("FORMULA atlas stone=%d dirt=%d\n", T_STONE, T_DIRT);
    if (up != (int)R_BALL) { printf("fail chord\n"); return 4; }
    return 0;
}

int main(int argc, char **argv) {
    const char *cmd = argc > 1 ? argv[1] : "";
    if (load_bodies(argc > 0 ? argv[0] : 0) != 0) {
        fprintf(stderr, "fail bodies\n");
        return 3;
    }
    if (!strcmp(cmd, "formula")) {
        if (argc != 2) { fprintf(stderr, "fail formula\n"); return 2; }
        return formula_op();
    }
    if (argc >= 5 && !strcmp(cmd, "pages")) return pages_op(argv[2], argv[3], argv[4]);
    if (argc >= 6 && !strcmp(cmd, "desk")) return desk(argv[2], argv[3], argv[4], argv[5]);
    if (argc >= 4 && !strcmp(cmd, "spawn")) return ensure_ledger(argv[2], argv[3]);
    if (argc >= 5 && !strcmp(cmd, "step")) return step_op(argv[2], argv[3], argv[4]);
    if (argc >= 4 && !strcmp(cmd, "animals")) return animals_op(argv[2], argv[3]);
    fprintf(stderr, "usage: earth_moth_op formula\n"
                    "       earth_moth_op pages <etopo> <mineclonia-mods> <maps-dir>\n"
                    "       earth_moth_op desk <etopo> <mineclonia-mods> <ledger> <deskdir>\n"
                    "       earth_moth_op spawn <etopo> <ledger>\n"
                    "       earth_moth_op step <etopo> <ledger> n|e|s|w\n"
                    "       earth_moth_op animals <etopo> <ledger>\n");
    return 2;
}
