/* tile_period_op: does a rectangle of a sheet repeat with period P? (D34 scramble: "does it still tile?")
 * usage: tile_period_op <sheet.png> <x> <y> <w> <h> <period>
 * Compares every pixel (x+i, y+j) of the w x h rectangle with the pixel P px to its right AND P px below (both must lie inside the image).
 * Prints "PERIOD period=P checked=N mismatches=M"; exit 0 when M = 0, 1 when there are mismatches, 2 on a bad call.
 * Used with a flat interior region of an autotile block: the ground generator makes that texture 24 px periodic, which is what lets any two quarters meet without a seam. */
#define STB_IMAGE_IMPLEMENTATION
#include "../../../&.widgits/_shared-lib/stb_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
    int w, h, c, x0, y0, rw, rh, p, i, j; long checked = 0, bad = 0; unsigned char *px;
    if (argc != 7) { fprintf(stderr, "usage: tile_period_op <sheet.png> <x> <y> <w> <h> <period>\n"); return 2; }
    x0 = atoi(argv[2]); y0 = atoi(argv[3]); rw = atoi(argv[4]); rh = atoi(argv[5]); p = atoi(argv[6]);
    if (rw < 1 || rh < 1 || p < 1 || x0 < 0 || y0 < 0) { fprintf(stderr, "bad rectangle or period\n"); return 2; }
    px = stbi_load(argv[1], &w, &h, &c, 4); if (!px) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
    if (x0 + rw + p > w || y0 + rh + p > h) { fprintf(stderr, "rectangle plus period leaves the image\n"); stbi_image_free(px); return 2; }
    for (j = 0; j < rh; j++) for (i = 0; i < rw; i++) {
        const unsigned char *a = px + ((size_t)(y0 + j) * w + x0 + i) * 4, *r = px + ((size_t)(y0 + j) * w + x0 + i + p) * 4, *d = px + ((size_t)(y0 + j + p) * w + x0 + i) * 4;
        checked++;
        if (memcmp(a, r, 4) || memcmp(a, d, 4)) bad++;
    }
    printf("PERIOD period=%d checked=%ld mismatches=%ld\n", p, checked, bad);
    stbi_image_free(px); return bad ? 1 : 0;
}
