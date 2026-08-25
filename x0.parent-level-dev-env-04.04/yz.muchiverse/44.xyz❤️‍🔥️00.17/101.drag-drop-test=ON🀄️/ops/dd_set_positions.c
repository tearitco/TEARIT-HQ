/* dd_set_positions.c - Write window positions to drag_drop_test.pdl
 *
 * Usage: dd_set_positions <config_file> <gl_x> <gl_y> <egg_x> <egg_y>
 *
 * The windows (gl_mirror, egg_window) poll this file every second
 * and update their positions accordingly.
 */
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc != 6) {
        fprintf(stderr, "Usage: %s <config_file> <gl_x> <gl_y> <egg_x> <egg_y>\n", argv[0]);
        return 1;
    }

    const char *config_file = argv[1];
    int gl_x = atoi(argv[2]);
    int gl_y = atoi(argv[3]);
    int egg_x = atoi(argv[4]);
    int egg_y = atoi(argv[5]);

    FILE *f = fopen(config_file, "w");
    if (!f) {
        fprintf(stderr, "ERROR: cannot write to %s\n", config_file);
        return 1;
    }

    fprintf(f, "SECTION      | KEY                | VALUE\n");
    fprintf(f, "----------------------------------------\n");
    fprintf(f, "WINDOW       | gl_mirror_x        | %d\n", gl_x);
    fprintf(f, "WINDOW       | gl_mirror_y        | %d\n", gl_y);
    fprintf(f, "WINDOW       | egg_window_x       | %d\n", egg_x);
    fprintf(f, "WINDOW       | egg_window_y       | %d\n", egg_y);
    fprintf(f, "PET          | pet_id             | egg_1\n");

    fclose(f);
    printf("OK: positions written to %s (gl_mirror=%d,%d egg_window=%d,%d)\n",
           config_file, gl_x, gl_y, egg_x, egg_y);
    return 0;
}
