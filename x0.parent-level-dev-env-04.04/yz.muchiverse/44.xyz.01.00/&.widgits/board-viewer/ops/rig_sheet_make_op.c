/* rig_sheet_make_op <out.png> <single|sheet|empty|emptysingle>: synthetic RPG Maker MV sheets for the harness rig_convert (BOOK 16.game/BOX-LIMB-RIG-FOR-WALKING-CHARACTERS-2026-11-25.md).
 *   single       144x192, ONE character (3x4 frames of 48x48): the down-facing standing frame (column 1, row 0) holds figure A (hair brown 100 60 30, skin tan 220 170 130, shirt blue 40 80 200, trousers 20 30 90, shoes black 0 0 0)
 *   sheet        576x384, 12x8 frames of 48x48: character block 3 (columns 9..11, row 0..3) holds figure A in its standing frame; block 0 holds figure B (shirt red 200 40 40, trousers 60 120 60, hair 240 220 60, skin 230 190 160, shoes 120 70 20); the rest is transparent
 *   empty        576x384 fully transparent; emptysingle 144x192 fully transparent
 * Figure (48 rows, 0..47): rows 0..8 hair over cols 16..31, rows 9..23 skin, rows 24..39 shirt over cols 14..33, rows 40..45 trousers over cols 16..31, rows 46..47 shoes.
 * Build: gcc -std=gnu11 -O2 -o rig_sheet_make_op rig_sheet_make_op.c -lpng -lz */
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct { unsigned char hair[3], skin[3], shirt[3], legs[3], shoe[3]; } Fig;
static const Fig A = { {100,60,30}, {220,170,130}, {40,80,200}, {20,30,90}, {0,0,0} };
static const Fig B = { {240,220,60}, {230,190,160}, {200,40,40}, {60,120,60}, {120,70,20} };
static unsigned char *img; static int W, Hh;
static void px(int x, int y, const unsigned char *c) { unsigned char *p = img + ((size_t)y * W + x) * 4; p[0] = c[0]; p[1] = c[1]; p[2] = c[2]; p[3] = 255; }
static void fig(int ox, int oy, const Fig *f) {
    for (int y = 0; y < 48; y++) for (int x = 0; x < 48; x++) {
        const unsigned char *c = NULL;
        if (y <= 8 && x >= 16 && x <= 31) c = f->hair; else if (y <= 23 && x >= 16 && x <= 31) c = f->skin;
        else if (y >= 24 && y <= 39 && x >= 14 && x <= 33) c = f->shirt; else if (y >= 40 && y <= 45 && x >= 16 && x <= 31) c = f->legs; else if (y >= 46 && x >= 16 && x <= 31) c = f->shoe;
        if (c) px(ox + x, oy + y, c);
    }
}
int main(int argc, char **argv) {
    if (argc != 3) { fprintf(stderr, "usage: rig_sheet_make_op <out.png> <single|sheet|empty|emptysingle>\n"); return 2; }
    int single = !strcmp(argv[2], "single") || !strcmp(argv[2], "emptysingle");
    W = single ? 144 : 576; Hh = single ? 192 : 384;
    img = calloc((size_t)W * Hh, 4); if (!img) return 1;
    if (!strcmp(argv[2], "single")) fig(48, 0, &A);
    else if (!strcmp(argv[2], "sheet")) { fig(3 * 3 * 48 + 48, 0, &A); fig(48, 0, &B); }
    else if (strcmp(argv[2], "empty") && strcmp(argv[2], "emptysingle")) return 2;
    FILE *fp = fopen(argv[1], "wb"); if (!fp) return 1;
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL); png_infop info = png_create_info_struct(png);
    if (setjmp(png_jmpbuf(png))) return 1;
    png_init_io(png, fp);
    png_set_IHDR(png, info, (png_uint_32)W, (png_uint_32)Hh, 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);
    for (int y = 0; y < Hh; y++) png_write_row(png, img + (size_t)y * W * 4);
    png_write_end(png, NULL); fclose(fp); return 0;
}
