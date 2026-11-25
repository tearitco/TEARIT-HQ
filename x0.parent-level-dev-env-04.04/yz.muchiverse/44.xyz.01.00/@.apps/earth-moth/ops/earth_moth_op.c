/* earth-moth: the round body is the molten core, Mineclonia lava, smaller than the page.
 * The desk is a projection. The ledger is the player and the animals.
 * Config (not a ledger): #.NNEST_ASSETS/rta-sources/earth-moth/ETOPO5.DAT
 * Page grid is a disc. Column height follows the sphere, so the body is round
 * in the board, not a flat square. Space outside the disc is an empty cell.
 */
#include <errno.h>
#include <math.h>
#include <png.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define N 72
#define R_LAVA 12.0
#define R_OIL 16.0
#define R_WATER 20.0
#define R_GRASS 24.0
#define ETW 4320
#define ETH 2160

enum { T_EMPTY = 0, T_LAVA = 1, T_OIL = 2, T_WATER = 3, T_GRASS = 4, T_LEAVES = 5 };

/* Mineclonia has no oil block. The oil shell uses the coal block, the darkest liquid-dark block in mcl_core. */
static const char *SHELL_PNG[] = {
    "ITEMS/mcl_core/textures/default_lava_source_animated.png",
    "ITEMS/mcl_core/textures/default_coal_block.png",
    "ITEMS/mcl_core/textures/default_water_source_animated.png",
    "ITEMS/mcl_core/textures/mcl_core_grass_block_top.png"
};
static const char *LEAVES_PNG = "ITEMS/mcl_core/textures/default_leaves.png";

static char hglyph(int h) {
    if (h < 1) h = 1;
    if (h > 60) h = 60;
    if (h <= 10) return (char)('0' + (h - 1));
    if (h <= 36) return (char)('A' + (h - 11));
    return (char)('a' + (h - 37));
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

static int cell_of(const int16_t *e, int x, int y, int *lat_m, int *lon_m, int *hh) {
    double cx = (N - 1) / 2.0, cy = (N - 1) / 2.0;
    double dx = x - cx, dy = cy - y, dist, z, lat, lon;
    int tile, h;
    dist = sqrt(dx * dx + dy * dy);
    if (dist > R_GRASS) { *hh = 0; return T_EMPTY; }
    z = sqrt(1.0 - (dist / R_GRASS) * (dist / R_GRASS));
    lat = asin(dy / R_GRASS) * 180.0 / M_PI;
    lon = atan2(dx, R_GRASS * z) * 180.0 / M_PI;
    (void)e;
    if (lat_m) *lat_m = (int)lround(lat);
    if (lon_m) *lon_m = (int)lround(lon);
    h = 4 + (int)(z * 18.0);
    if (h > 60) h = 60;
    if (dist <= R_LAVA) tile = T_LAVA;
    else if (dist <= R_OIL) tile = T_OIL;
    else if (dist <= R_WATER) tile = T_WATER;
    else tile = T_GRASS;
    *hh = h;
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

static int spawn_of(const int16_t *e, int *sx, int *sy) {
    int x, y, best = 1 << 30;
    *sx = N / 2; *sy = N / 2;
    for (y = 0; y < N; y++) for (x = 0; x < N; x++) {
        int lat, lon, h, tile = cell_of(e, x, y, &lat, &lon, &h);
        int d;
        if (tile == T_EMPTY) continue;
        d = abs(lat - 30) * 3 + abs(lon - 15);
        if (d < best) { best = d; *sx = x; *sy = y; }
    }
    return best < (1 << 30) ? 0 : 4;
}

static int is_tree(int x, int y, int tile) {
    return tile == T_GRASS && ((x * 7 + y * 3) % 9) == 0;
}

static int grass_open(const int16_t *e, int x, int y) {
    int h, tile;
    if (x < 0 || y < 0 || x >= N || y >= N) return 0;
    tile = cell_of(e, x, y, NULL, NULL, &h);
    return tile == T_GRASS && !is_tree(x, y, tile);
}

/* A grass cell that is not a tree, walking the ring so the seats are not stacked. */
static int pick_grass(const int16_t *e, int n, int *x, int *y) {
    int i, seen = 0;
    for (i = 0; i < N * N; i++) {
        int xx = i % N, yy = i / N;
        if (!grass_open(e, xx, yy)) continue;
        if (seen == n) { *x = xx; *y = yy; return 0; }
        seen++;
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
    int h, tile;
    if (x < 0 || y < 0 || x >= N || y >= N) return 0;
    tile = cell_of(e, x, y, NULL, NULL, &h);
    return tile != T_EMPTY;
}

static int desk(const char *etopo, const char *mine, const char *ledger, const char *dir) {
    int16_t *e = load_etopo(etopo);
    FILE *f;
    char path[1100];
    int x, y, sx = N / 2, sy = N / 2, px, py;
    struct stat ms;
    if (!e) { printf("fail etopo\n"); return 5; }
    if (mkdir(dir, 0755) != 0 && errno != EEXIST) { free(e); return 5; }
    spawn_of(e, &sx, &sy);
    where(ledger, sx, sy, &px, &py);
    if (!grass_open(e, px, py) && !pick_grass(e, 0, &px, &py)) {
        FILE *lf = fopen(ledger, "a");
        if (lf) { fprintf(lf, "MOVE | %d | %d | grass\n", px, py); fclose(lf); }
    }
    {
        const char *kinds[3] = { "CREEPER", "PIG", "COW" };
        int k, ax, ay, have;
        for (k = 0; k < 3; k++) {
            last_of(ledger, kinds[k], &ax, &ay, &have);
            if (have && grass_open(e, ax, ay)) continue;
            if (pick_grass(e, 4 + k * 5, &ax, &ay)) continue;
            {
                FILE *lf = fopen(ledger, "a");
                if (lf) { fprintf(lf, "%s | %d | %d\n", kinds[k], ax, ay); fclose(lf); }
            }
        }
    }
    snprintf(path, sizeof path, "%s/cells.txt", dir);
    f = fopen(path, "w");
    if (!f) { free(e); return 5; }
    fprintf(f, "width=%d\nheight=%d\ntile_px=16\natlas_cols=8\natlas_tiles=8\ncells\n", N, N);
    for (y = 0; y < N; y++) {
        for (x = 0; x < N; x++) {
            int h, tile = cell_of(e, x, y, NULL, NULL, &h);
            if (is_tree(x, y, tile)) tile = T_LEAVES;
            fprintf(f, "%s%d,0", x ? " " : "", tile);
        }
        fputc('\n', f);
    }
    fclose(f);
    snprintf(path, sizeof path, "%s/cells.rgba", dir);
    f = fopen(path, "w");
    if (!f) { free(e); return 5; }
    /* Slot 0 empty, then lava, oil, water, grass. Pad to 8 tiles. */
    for (y = 0; y < 16 * 16; y++) { fputc(0, f); fputc(0, f); fputc(0, f); fputc(0, f); }
    for (x = 0; x < 4; x++) blit16(f, mine, SHELL_PNG[x]);
    blit16(f, mine, LEAVES_PNG);
    for (x = 6; x < 8; x++) for (y = 0; y < 16 * 16; y++) { fputc(0, f); fputc(0, f); fputc(0, f); fputc(0, f); }
    fclose(f);
    snprintf(path, sizeof path, "%s/map.txt", dir);
    f = fopen(path, "w");
    if (!f) { free(e); return 5; }
    for (y = 0; y < N; y++) {
        for (x = 0; x < N; x++) {
            int h, tile = cell_of(e, x, y, NULL, NULL, &h);
            if (is_tree(x, y, tile)) h += 12;
            fputc(tile == T_EMPTY ? '.' : hglyph(h), f);
        }
        fputc('\n', f);
    }
    fclose(f);
    snprintf(path, sizeof path, "%s/extrusion.pdl", dir);
    f = fopen(path, "w");
    if (!f) { free(e); return 5; }
    fputs("EXTRUDE | . | 1\n", f);
    for (x = 1; x <= 60; x++) fprintf(f, "EXTRUDE | %c | %d\n", hglyph(x), x);
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
    printf("desk=%s player=%d,%d spawn=%d,%d\n", dir, px, py, sx, sy);
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
    if (!land_ok(e, nx, ny)) { free(e); printf("fail step\n"); return 4; }
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

int main(int argc, char **argv) {
    const char *cmd = argc > 1 ? argv[1] : "";
    if (argc >= 6 && !strcmp(cmd, "desk")) return desk(argv[2], argv[3], argv[4], argv[5]);
    if (argc >= 4 && !strcmp(cmd, "spawn")) return ensure_ledger(argv[2], argv[3]);
    if (argc >= 5 && !strcmp(cmd, "step")) return step_op(argv[2], argv[3], argv[4]);
    if (argc >= 4 && !strcmp(cmd, "animals")) return animals_op(argv[2], argv[3]);
    fprintf(stderr, "usage: earth_moth_op desk <etopo> <mineclonia-mods> <ledger> <deskdir>\n"
                    "       earth_moth_op spawn <etopo> <ledger>\n"
                    "       earth_moth_op step <etopo> <ledger> n|e|s|w\n"
                    "       earth_moth_op animals <etopo> <ledger>\n");
    return 2;
}
