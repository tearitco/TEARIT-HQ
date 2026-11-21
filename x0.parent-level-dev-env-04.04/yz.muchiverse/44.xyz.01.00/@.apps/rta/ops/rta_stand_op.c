/* rta_stand_op — turn a Battery city grid into a desk the board can stand up.
 * Glyphs are rta_city_op's ".KPRSWiB" (grass, park, path, road, beach,
 * water, pier, building). North row is file row 0. Building cells are
 * map glyph B with extrusion 3. Every other glyph stays height 1.
 * The atlas is eight 48 px samples. No map picture is written.
 *   rta_stand_op <tileset_dir> <grid.txt> <desk_dir>
 * Prints: w h buildings road_x road_y
 * road_x road_y is a road cell in the south third, or -1 -1.
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o rta_stand_op.+x rta_stand_op.c -lpng -lz
 */
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define T 48
#define MAXW 220
#define MAXH 300

typedef struct { int w, h; unsigned char *px; } Img;

static int load_png(const char *path, Img *im) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info = png ? png_create_info_struct(png) : NULL;
    if (!png || !info || setjmp(png_jmpbuf(png))) { if (fp) fclose(fp); return -1; }
    png_init_io(png, fp);
    png_read_info(png, info);
    im->w = (int)png_get_image_width(png, info);
    im->h = (int)png_get_image_height(png, info);
    png_byte color = png_get_color_type(png, info);
    if (color == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (color == PNG_COLOR_TYPE_GRAY || color == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_gray_to_rgb(png);
    if (!(color & PNG_COLOR_MASK_ALPHA)) png_set_add_alpha(png, 255, PNG_FILLER_AFTER);
    png_read_update_info(png, info);
    im->px = malloc((size_t)im->w * im->h * 4);
    png_bytep rows[im->h];
    for (int y = 0; y < im->h; y++) rows[y] = im->px + (size_t)y * im->w * 4;
    png_read_image(png, rows);
    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);
    return im->px ? 0 : -1;
}

static void blit(const Img *src, int sx, int sy, Img *dst, int dx, int dy) {
    for (int y = 0; y < T; y++) {
        if (sy + y >= src->h || dy + y >= dst->h) continue;
        for (int x = 0; x < T; x++) {
            if (sx + x >= src->w || dx + x >= dst->w) continue;
            memcpy(dst->px + ((size_t)(dy + y) * dst->w + (dx + x)) * 4,
                   src->px + ((size_t)(sy + y) * src->w + (sx + x)) * 4, 4);
        }
    }
}

/* City glyph -> atlas slot 1..8 and a height glyph. B is the only tall one. */
static int slot_of(char g) {
    switch (g) {
    case 'K': return 2;
    case 'P': return 3;
    case 'R': return 4;
    case 'S': return 5;
    case 'W': return 6;
    case 'i': return 7;
    case 'B': return 8;
    default: return 1;
    }
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "usage: rta_stand_op <tileset_dir> <grid.txt> <desk_dir>\n");
        return 2;
    }
    char path[512];
    Img a1, a2, b;
    snprintf(path, sizeof path, "%s/World_A1.png", argv[1]); if (load_png(path, &a1)) return 3;
    snprintf(path, sizeof path, "%s/World_A2.png", argv[1]); if (load_png(path, &a2)) return 3;
    snprintf(path, sizeof path, "%s/World_B.png", argv[1]); if (load_png(path, &b)) return 3;

    FILE *gf = fopen(argv[2], "r");
    if (!gf) return 4;
    char row[MAXW + 8];
    static char grid[MAXH][MAXW];
    int w = 0, h = 0;
    while (fgets(row, sizeof row, gf)) {
        if (row[0] == '#' || row[0] == '\n') continue;
        int n = (int)strcspn(row, "\r\n");
        if (n < 1 || n > MAXW || h >= MAXH) { fclose(gf); return 4; }
        memcpy(grid[h], row, (size_t)n);
        if (w == 0) w = n;
        if (n != w) { fclose(gf); return 4; }
        h++;
    }
    fclose(gf);
    if (w < 1 || h < 1) return 4;

    mkdir(argv[3], 0755);
    Img atlas = { 16 * T, T, calloc((size_t)16 * T * T, 4) };
    if (!atlas.px) return 5;
    blit(&a2, 2 * T, 0 * T, &atlas, 1 * T, 0);
    blit(&a2, 2 * T, 0 * T, &atlas, 2 * T, 0);
    blit(&a2, 10 * T, 6 * T, &atlas, 3 * T, 0);
    blit(&a2, 4 * T, 6 * T, &atlas, 4 * T, 0);
    blit(&a2, 0 * T, 6 * T, &atlas, 5 * T, 0);
    blit(&a1, 0 * T, 0 * T, &atlas, 6 * T, 0);
    blit(&b, 2 * T, 0 * T, &atlas, 7 * T, 0);
    blit(&b, 4 * T, 5 * T, &atlas, 8 * T, 0);

    snprintf(path, sizeof path, "%s/cells.rgba", argv[3]);
    FILE *rf = fopen(path, "wb");
    if (!rf) return 6;
    fwrite(atlas.px, 1, (size_t)atlas.w * atlas.h * 4, rf);
    fclose(rf);

    snprintf(path, sizeof path, "%s/cells.txt", argv[3]);
    FILE *cf = fopen(path, "w");
    if (!cf) return 6;
    fprintf(cf, "width=%d\nheight=%d\ntile_px=%d\natlas_cols=16\natlas_tiles=9\ncells\n", w, h, T);
    int nb = 0;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int slot = slot_of(grid[y][x]);
            if (grid[y][x] == 'B') nb++;
            fprintf(cf, "%s%d,0", x ? " " : "", slot);
        }
        fprintf(cf, "\n");
    }
    fclose(cf);

    snprintf(path, sizeof path, "%s/map.txt", argv[3]);
    FILE *mf = fopen(path, "w");
    if (!mf) return 6;
    for (int y = 0; y < h; y++) {
        fwrite(grid[y], 1, (size_t)w, mf);
        fputc('\n', mf);
    }
    fclose(mf);

    snprintf(path, sizeof path, "%s/extrusion.pdl", argv[3]);
    FILE *ef = fopen(path, "w");
    if (!ef) return 6;
    fprintf(ef, "EXTRUDE | . | 1\nEXTRUDE | K | 1\nEXTRUDE | P | 1\nEXTRUDE | R | 1\n"
                "EXTRUDE | S | 1\nEXTRUDE | W | 1\nEXTRUDE | i | 1\nEXTRUDE | B | 3\n");
    fclose(ef);

    snprintf(path, sizeof path, "%s/events.txt", argv[3]);
    FILE *ev = fopen(path, "w");
    if (ev) fclose(ev);
    snprintf(path, sizeof path, "%s/cells.marker", argv[3]);
    FILE *mk = fopen(path, "ab");
    if (mk) { fputc('x', mk); fclose(mk); }

    int rx = -1, ry = -1, anyx = -1, anyy = -1;
    for (int y = h - 1; y >= 0 && rx < 0; y--) {
        for (int x = 0; x < w; x++) {
            if (grid[y][x] != 'R') continue;
            if (anyx < 0) { anyx = x; anyy = y; }
            int near = 0;
            for (int dy = -2; dy <= 2 && !near; dy++)
                for (int dx = -2; dx <= 2; dx++) {
                    int yy = y + dy, xx = x + dx;
                    if (yy >= 0 && yy < h && xx >= 0 && xx < w && grid[yy][xx] == 'B') near = 1;
                }
            if (near) { rx = x; ry = y; break; }
        }
    }
    if (rx < 0) { rx = anyx; ry = anyy; }
    printf("w=%d h=%d buildings=%d road_x=%d road_y=%d\n", w, h, nb, rx, ry);
    return nb > 0 ? 0 : 8;
}
