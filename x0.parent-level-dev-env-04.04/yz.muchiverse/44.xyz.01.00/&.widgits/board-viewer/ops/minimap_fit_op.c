/* minimap_fit_op — the corner-map size and the camera pixel.
 *   minimap_fit_op box <cells_w> <cells_h> <px_per_col> <max_px>
 *   minimap_fit_op dot <cam> <origin> <cells> <span_px> <cellpx>
 * Prints `box W H CELL` or `dot PX OUTSIDE`. Exit 2 on a bad call.
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
    fprintf(stderr, "usage: minimap_fit_op box <w> <h> <px> <max> | dot <cam> <origin> <cells> <span> <cellpx>\n");
    return 2;
}
