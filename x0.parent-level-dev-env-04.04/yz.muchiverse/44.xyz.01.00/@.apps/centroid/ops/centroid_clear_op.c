/* centroid_clear_op <cells.txt>
 * Puts that desk back to a flat 16x16 grass page and drops the door,
 * the roof events, and door.txt. The camera is not written. */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: centroid_clear_op <cells.txt>\n"); return 2; }
    FILE *o = fopen(argv[1], "w");
    if (!o) { perror(argv[1]); return 1; }
    fprintf(o, "width=16\nheight=16\ntile_px=24\natlas_cols=16\natlas_tiles=43\nrock_slot=36\ncells\n");
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) fprintf(o, "%s1,0,0", x ? " " : "");
        fputc('\n', o);
    }
    fclose(o);
    char p[4200]; snprintf(p, sizeof p, "%s", argv[1]);
    char *sl = strrchr(p, '/');
    if (sl) snprintf(sl + 1, sizeof p - (size_t)(sl + 1 - p), "events.txt");
    else snprintf(p, sizeof p, "events.txt");
    FILE *e = fopen(p, "w"); if (e) fclose(e);
    if (sl) snprintf(sl + 1, sizeof p - (size_t)(sl + 1 - p), "door.txt");
    else snprintf(p, sizeof p, "door.txt");
    unlink(p);
    printf("CLEARED 16x16\n");
    return 0;
}
