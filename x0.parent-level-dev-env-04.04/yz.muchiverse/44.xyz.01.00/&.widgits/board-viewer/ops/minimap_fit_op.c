/* minimap_fit_op — the corner-map size and the camera pixel.
 *   minimap_fit_op box <cells_w> <cells_h> <px_per_col> <max_px>
 *   minimap_fit_op dot <cam> <origin> <cells> <span_px> <cellpx>
 *   minimap_fit_op tileavg <cells.rgba> <cols> <tile_px> <slot>      (the colour the corner map paints for that atlas slot)
 * Prints `box W H CELL`, `dot PX OUTSIDE` or `tileavg R G B` (`tileavg none`). Exit 2 on a bad call.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bv_minimap_fit.h"
int main(int argc, char **argv) {
    if (argc == 6 && !strcmp(argv[1], "box")) {
        int w, h, cp;
        bv_minimap_box(atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), atoi(argv[5]), &w, &h, &cp);
        printf("box %d %d %d\n", w, h, cp);
        return 0;
    }
    if (argc == 7 && !strcmp(argv[1], "dot")) {
        int px, out;
        out = bv_minimap_dot(atof(argv[2]), atof(argv[3]), atoi(argv[4]), atoi(argv[5]), atoi(argv[6]), &px);
        printf("dot %d %d\n", px, out);
        return 0;
    }
    if (argc == 6 && !strcmp(argv[1], "tileavg")) {
        FILE *f = fopen(argv[2], "rb"); int cols = atoi(argv[3]), tp = atoi(argv[4]), slot = atoi(argv[5]), r, g, b;
        if (!f || cols < 1 || tp < 1) { fprintf(stderr, "tileavg: cannot read the atlas\n"); return 2; }
        fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
        unsigned char *a = malloc((size_t)sz); if (!a || fread(a, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); return 2; } fclose(f);
        int aw = cols * tp, ah = (int)(sz / ((long)aw * 4));
        if (bv_minimap_tile_avg(a, aw, ah, cols, tp, slot, &r, &g, &b)) printf("tileavg %d %d %d\n", r, g, b); else printf("tileavg none\n");
        free(a); return 0;
    }
    fprintf(stderr, "usage: minimap_fit_op box <w> <h> <px> <max> | dot <cam> <origin> <cells> <span> <cellpx> | tileavg <rgba> <cols> <tile_px> <slot>\n");
    return 2;
}
