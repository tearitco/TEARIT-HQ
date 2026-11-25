/* rmmc_field_op — page rmmc-0000 as a flat heightfield.
 * Same desk formula as rta_stand_op: one atlas row, cells.txt slots,
 * map.txt glyphs, EXTRUDE heights. No OBJ, no FBX.
 *   rmmc_field_op <tileset_dir> <desk_dir>
 * 64 by 64. Grass, a sand strip, a water corner, a snow corner,
 * one stone pillar, one tree, and a trench one step lower than the grass.
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o rmmc_field_op.+x rmmc_field_op.c -lpng -lz
 */
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define T 48
#define N 64

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

/* slot, glyph, height. Grass 1, sand 5, water 6, snow uses beach-light
 * slot 5 is sand; snow is slot 2 (second grass sample, taller). Stone 7. Tree 8. */
static char cell_of(int x, int y, int *slot, int *h) {
    char g = '.';
    *slot = 1;
    *h = 2;
    if (x >= 40 && y >= 40) { g = 'S'; *slot = 5; *h = 2; }          /* sand */
    if (x < 14 && y >= 50) { g = 'W'; *slot = 6; *h = 1; }            /* water */
    if (x >= 48 && y < 16) { g = 'N'; *slot = 2; *h = 3; }            /* snow corner, raised */
    if (x >= 30 && x <= 34 && y >= 18 && y <= 40) { g = 'D'; *slot = 1; *h = 1; } /* trench */
    if (x == 20 && y == 16) { g = 'i'; *slot = 7; *h = 5; }           /* stone pillar */
    if (x == 44 && y == 22) { g = 'B'; *slot = 8; *h = 4; }           /* tree stand-in */
    return g;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: rmmc_field_op <tileset_dir> <desk_dir>\n");
        return 2;
    }
    char path[512];
    Img a1, a2, b;
    snprintf(path, sizeof path, "%s/World_A1.png", argv[1]); if (load_png(path, &a1)) return 3;
    snprintf(path, sizeof path, "%s/World_A2.png", argv[1]); if (load_png(path, &a2)) return 3;
    snprintf(path, sizeof path, "%s/World_B.png", argv[1]); if (load_png(path, &b)) return 3;
    mkdir(argv[2], 0755);
    Img atlas = { 16 * T, T, calloc((size_t)16 * T * T, 4) };
    if (!atlas.px) return 5;
    blit(&a2, 2 * T, 0 * T, &atlas, 1 * T, 0);
    blit(&a2, 0 * T, 2 * T, &atlas, 2 * T, 0); /* second ground, used as the snow corner */
    blit(&a2, 0 * T, 6 * T, &atlas, 5 * T, 0);
    blit(&a1, 0 * T, 0 * T, &atlas, 6 * T, 0);
    blit(&b, 2 * T, 0 * T, &atlas, 7 * T, 0);
    blit(&b, 4 * T, 5 * T, &atlas, 8 * T, 0);

    snprintf(path, sizeof path, "%s/cells.rgba", argv[2]);
    FILE *rf = fopen(path, "wb");
    if (!rf) return 6;
    fwrite(atlas.px, 1, (size_t)atlas.w * atlas.h * 4, rf);
    fclose(rf);

    snprintf(path, sizeof path, "%s/cells.txt", argv[2]);
    FILE *cf = fopen(path, "w");
    snprintf(path, sizeof path, "%s/map.txt", argv[2]);
    FILE *mf = fopen(path, "w");
    if (!cf || !mf) return 6;
    fprintf(cf, "width=%d\nheight=%d\ntile_px=%d\natlas_cols=16\natlas_tiles=9\ncells\n", N, N, T);
    int trench = 0, sand = 0, water = 0, snow = 0;
    for (int y = 0; y < N; y++) {
        for (int x = 0; x < N; x++) {
            int slot, h;
            char g = cell_of(x, y, &slot, &h);
            fprintf(cf, "%s%d,0", x ? " " : "", slot);
            fputc(g, mf);
            if (g == 'D') trench++;
            else if (g == 'S') sand++;
            else if (g == 'W') water++;
            else if (g == 'N') snow++;
        }
        fputc('\n', cf);
        fputc('\n', mf);
    }
    fclose(cf);
    fclose(mf);

    snprintf(path, sizeof path, "%s/extrusion.pdl", argv[2]);
    FILE *ef = fopen(path, "w");
    if (!ef) return 6;
    fprintf(ef, "EXTRUDE | . | 2\nEXTRUDE | S | 2\nEXTRUDE | W | 1\nEXTRUDE | N | 3\n"
                "EXTRUDE | D | 1\nEXTRUDE | i | 5\nEXTRUDE | B | 4\n");
    fclose(ef);
    snprintf(path, sizeof path, "%s/events.txt", argv[2]);
    FILE *ev = fopen(path, "w"); if (ev) fclose(ev);
    snprintf(path, sizeof path, "%s/cells.marker", argv[2]);
    FILE *mk = fopen(path, "ab"); if (mk) { fputc('x', mk); fclose(mk); }
    snprintf(path, sizeof path, "%s/skin.txt", argv[2]);
    FILE *sk = fopen(path, "w"); if (sk) { fputs("rmmv\n", sk); fclose(sk); }
    printf("w=%d h=%d sand=%d water=%d snow=%d trench=%d pillar=1 tree=1\n", N, N, sand, water, snow, trench);
    return (sand > 0 && water > 0 && trench > 0) ? 0 : 4;
}
