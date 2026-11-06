/* centroid_parts_op stamp <cells.txt>
 * Reads parts/centroid.parts.pdl next to the app (argv0 is not used).
 * Writes one cells.txt: a tapered island (third number is the depth
 * hung under the floor) and the 5x5 room with the near-middle gap.
 * The camera is not written. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define W 32
#define H 24
int main(int argc, char **argv) {
    if (argc < 3 || strcmp(argv[1], "stamp") != 0) {
        fprintf(stderr, "usage: centroid_parts_op stamp <cells.txt>\n");
        return 2;
    }
    FILE *o = fopen(argv[2], "w");
    if (!o) { perror(argv[2]); return 1; }
    fprintf(o, "width=%d\nheight=%d\ntile_px=24\natlas_cols=16\natlas_tiles=43\nrock_slot=36\ncells\n", W, H);
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            int dx = x < (W - 1 - x) ? x : (W - 1 - x);
            int dy = y < (H - 1 - y) ? y : (H - 1 - y);
            int edge = dx < dy ? dx : dy;
            int depth = edge + 1;
            if (depth > 6) depth = 6;
            int wall = 0;
            int in = x >= 6 && x <= 10 && y >= 6 && y <= 10;
            int rim = in && (x == 6 || x == 10 || y == 6 || y == 10);
            if (rim && !(x == 8 && y == 10)) wall = 36;
            fprintf(o, "%s1,%d,%d", x ? " " : "", wall, depth);
        }
        fputc('\n', o);
    }
    fclose(o);
    printf("STAMP w=%d h=%d room=5 gap=8,10 taper=1..6\n", W, H);
    return 0;
}
