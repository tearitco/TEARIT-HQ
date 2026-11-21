/* rta_shore_op — one 16x12 shore so the Battery tile choice can be seen.
 * Park (World_A2 solid grass), beach (World_A2 sand autotile),
 * water (World_A1 sea), pier (World_B wood floor).
 * Neighbour rule matches gta_hq.c draw_auto. No network.
 *   rta_shore_op <tileset_dir> <out.png>
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o rta_shore_op.+x rta_shore_op.c -lpng -lz
 */
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W 16
#define H 12
#define T 48

enum { PARK, BEACH, WATER, PIER };

typedef struct { int w, h; unsigned char *px; } Img;

static int load_png(const char *path, Img *im) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info = png ? png_create_info_struct(png) : NULL;
    if (!png || !info || setjmp(png_jmpbuf(png))) { fclose(fp); return -1; }
    png_init_io(png, fp);
    png_read_info(png, info);
    int w = (int)png_get_image_width(png, info), h = (int)png_get_image_height(png, info);
    png_byte color = png_get_color_type(png, info), depth = png_get_bit_depth(png, info);
    if (depth == 16) png_set_strip_16(png);
    if (color == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (color == PNG_COLOR_TYPE_GRAY && depth < 8) png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
    if (color == PNG_COLOR_TYPE_RGB || color == PNG_COLOR_TYPE_GRAY || color == PNG_COLOR_TYPE_PALETTE)
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    if (color == PNG_COLOR_TYPE_GRAY || color == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_gray_to_rgb(png);
    png_read_update_info(png, info);
    unsigned char *px = malloc((size_t)w * (size_t)h * 4);
    png_bytep *rows = malloc(sizeof(png_bytep) * (size_t)h);
    if (!px || !rows) return -1;
    for (int y = 0; y < h; y++) rows[y] = px + (size_t)y * (size_t)w * 4;
    png_read_image(png, rows);
    png_destroy_read_struct(&png, &info, NULL);
    free(rows); fclose(fp);
    im->w = w; im->h = h; im->px = px;
    return 0;
}

static int save_png(const char *path, const Img *im) {
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info = png ? png_create_info_struct(png) : NULL;
    if (!png || !info || setjmp(png_jmpbuf(png))) { fclose(fp); return -1; }
    png_init_io(png, fp);
    png_set_IHDR(png, info, im->w, im->h, 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);
    png_bytep *rows = malloc(sizeof(png_bytep) * (size_t)im->h);
    for (int y = 0; y < im->h; y++) rows[y] = im->px + (size_t)y * (size_t)im->w * 4;
    png_write_image(png, rows);
    png_write_end(png, NULL);
    png_destroy_write_struct(&png, &info);
    free(rows); fclose(fp);
    return 0;
}

static void blit(const Img *s, int sx, int sy, int sw, int sh, Img *d, int dx, int dy) {
    for (int y = 0; y < sh; y++) for (int x = 0; x < sw; x++) {
        int X = sx + x, Y = sy + y, DX = dx + x, DY = dy + y;
        if (X < 0 || Y < 0 || X >= s->w || Y >= s->h || DX < 0 || DY < 0 || DX >= d->w || DY >= d->h) continue;
        unsigned char *sp = s->px + ((size_t)Y * s->w + X) * 4;
        unsigned char *dp = d->px + ((size_t)DY * d->w + DX) * 4;
        if (sp[3] < 16) continue;
        dp[0] = sp[0]; dp[1] = sp[1]; dp[2] = sp[2]; dp[3] = 255;
    }
}

/* Quadrant pieces from gta_hq.c draw_auto. bx,by are the block's top-left tile. */
static void draw_auto(const Img *sh, int bx, int by, int n, int e, int s, int w, int ne, int se, int sw, int nw, Img *d, int dx, int dy) {
    int q[4][2];
    if (!n && !e && !s && !w) {
        int iso[4][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
        memcpy(q, iso, sizeof q);
    } else {
#define PICK(i, a, b, dia, bothA, bothB, onlyA_, onlyB_, none_) do { const int *p_ = ((a) && (b)) ? ((dia) ? bothA : bothB) : (a) ? onlyA_ : (b) ? onlyB_ : none_; q[i][0] = p_[0]; q[i][1] = p_[1]; } while (0)
        static const int TLf[2] = {2, 4}, TLn[2] = {2, 0}, TLw[2] = {2, 2}, TLm[2] = {0, 4}, TLx[2] = {0, 2};
        static const int TRf[2] = {1, 4}, TRn[2] = {3, 0}, TRw[2] = {1, 2}, TRm[2] = {3, 4}, TRx[2] = {3, 2};
        static const int BLf[2] = {2, 3}, BLn[2] = {2, 1}, BLw[2] = {2, 5}, BLm[2] = {0, 3}, BLx[2] = {0, 5};
        static const int BRf[2] = {1, 3}, BRn[2] = {3, 1}, BRw[2] = {1, 5}, BRm[2] = {3, 3}, BRx[2] = {3, 5};
        PICK(0, n, w, nw, TLf, TLn, TLw, TLm, TLx);
        PICK(1, n, e, ne, TRf, TRn, TRw, TRm, TRx);
        PICK(2, s, w, sw, BLf, BLn, BLw, BLm, BLx);
        PICK(3, s, e, se, BRf, BRn, BRw, BRm, BRx);
#undef PICK
        if (n && !w) { q[0][0] = TLm[0]; q[0][1] = TLm[1]; } else if (!n && w) { q[0][0] = TLw[0]; q[0][1] = TLw[1]; }
        if (n && !e) { q[1][0] = TRm[0]; q[1][1] = TRm[1]; } else if (!n && e) { q[1][0] = TRw[0]; q[1][1] = TRw[1]; }
        if (s && !w) { q[2][0] = BLm[0]; q[2][1] = BLm[1]; } else if (!s && w) { q[2][0] = BLw[0]; q[2][1] = BLw[1]; }
        if (s && !e) { q[3][0] = BRm[0]; q[3][1] = BRm[1]; } else if (!s && e) { q[3][0] = BRw[0]; q[3][1] = BRw[1]; }
    }
    for (int i = 0; i < 4; i++)
        blit(sh, bx * T + q[i][0] * 24, by * T + q[i][1] * 24, 24, 24, d, dx + (i % 2) * 24, dy + (i / 2) * 24);
}

static int cell(const int *g, int x, int y) {
    if (x < 0 || x >= W || y < 0 || y >= H) return -1;
    return g[y * W + x];
}

static int same(const int *g, int x, int y, int k) {
    int c = cell(g, x, y);
    if (c < 0) return k == WATER ? (y >= H) : (k == PARK ? (y < 0) : 0);
    return c == k;
}

int main(int argc, char **argv) {
    if (argc != 3) { fprintf(stderr, "usage: rta_shore_op <tileset_dir> <out.png>\n"); return 2; }
    char path[512];
    Img a1, a2, b;
    snprintf(path, sizeof path, "%s/World_A1.png", argv[1]); if (load_png(path, &a1)) return 3;
    snprintf(path, sizeof path, "%s/World_A2.png", argv[1]); if (load_png(path, &a2)) return 3;
    snprintf(path, sizeof path, "%s/World_B.png", argv[1]); if (load_png(path, &b)) return 3;
    int g[W * H], npark = 0, nbeach = 0, nwater = 0, npier = 0;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        int k = y <= 6 ? PARK : y == 7 ? BEACH : WATER;
        if (x >= 7 && x <= 8 && y >= 5 && y <= 10) k = PIER;
        g[y * W + x] = k;
        if (k == PARK) npark++; else if (k == BEACH) nbeach++; else if (k == WATER) nwater++; else npier++;
    }
    Img out = { W * T, H * T, calloc((size_t)W * H * T * T, 4) };
    if (!out.px) return 4;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        int dx = x * T, dy = y * T, k = g[y * W + x];
        draw_auto(&a2, 2, 0, 1, 1, 1, 1, 1, 1, 1, 1, &out, dx, dy);
        if (k == BEACH || k == WATER)
            draw_auto(&a2, 0, 6,
                same(g, x, y - 1, BEACH), same(g, x + 1, y, BEACH), same(g, x, y + 1, BEACH), same(g, x - 1, y, BEACH),
                same(g, x + 1, y - 1, BEACH), same(g, x + 1, y + 1, BEACH), same(g, x - 1, y + 1, BEACH), same(g, x - 1, y - 1, BEACH),
                &out, dx, dy);
        if (k == WATER)
            draw_auto(&a1, 0, 0,
                same(g, x, y - 1, WATER), same(g, x + 1, y, WATER), same(g, x, y + 1, WATER), same(g, x - 1, y, WATER),
                same(g, x + 1, y - 1, WATER), same(g, x + 1, y + 1, WATER), same(g, x - 1, y + 1, WATER), same(g, x - 1, y - 1, WATER),
                &out, dx, dy);
        if (k == PIER) blit(&b, 2 * T, 0, T, T, &out, dx, dy);
    }
    if (save_png(argv[2], &out)) return 5;
    /* Centre of a deep-water cell must read blue, not grass. */
    unsigned char *wp = out.px + ((size_t)(11 * T + 24) * out.w + (2 * T + 24)) * 4;
    int blue = wp[2] > wp[0] && wp[2] > 80;
    printf("park=%d beach=%d water=%d pier=%d blue=%d png=%dx%d\n", npark, nbeach, nwater, npier, blue, out.w, out.h);
    return blue && npark == 108 && nbeach == 14 && nwater == 58 && npier == 12 ? 0 : 6;
}
