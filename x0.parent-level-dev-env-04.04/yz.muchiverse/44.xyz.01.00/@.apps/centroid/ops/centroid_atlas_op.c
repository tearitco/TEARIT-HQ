/* centroid_atlas_op — make the two desk atlases from the Islands RPG Maker atlas.
 * rmmv is that atlas, byte for byte. gen is the same pixels with red and blue swapped,
 * so backslash has a second sheet until a studio atlas of the same size exists.
 * usage: centroid_atlas_op <islands.rgba> <deskdir>
 */
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) {
    if (argc != 3) { fprintf(stderr, "usage: centroid_atlas_op <rgba> <deskdir>\n"); return 2; }
    FILE *in = fopen(argv[1], "rb");
    if (!in) { perror(argv[1]); return 2; }
    fseek(in, 0, SEEK_END); long n = ftell(in); fseek(in, 0, SEEK_SET);
    if (n < 4) { fclose(in); return 2; }
    unsigned char *b = malloc((size_t)n);
    if (!b || fread(b, 1, (size_t)n, in) != (size_t)n) { fclose(in); return 2; }
    fclose(in);
    char path[512];
    snprintf(path, sizeof path, "%s/cells-rmmv.rgba", argv[2]);
    FILE *o = fopen(path, "wb"); if (!o) return 2; fwrite(b, 1, (size_t)n, o); fclose(o);
    snprintf(path, sizeof path, "%s/cells.rgba", argv[2]);
    o = fopen(path, "wb"); if (!o) return 2; fwrite(b, 1, (size_t)n, o); fclose(o);
    for (long i = 0; i + 3 < n; i += 4) { unsigned char r = b[i]; b[i] = b[i + 2]; b[i + 2] = r; }
    snprintf(path, sizeof path, "%s/cells-gen.rgba", argv[2]);
    o = fopen(path, "wb"); if (!o) return 2; fwrite(b, 1, (size_t)n, o); fclose(o);
    snprintf(path, sizeof path, "%s/skin.txt", argv[2]);
    o = fopen(path, "w"); if (!o) return 2; fputs("rmmv\n", o); fclose(o);
    snprintf(path, sizeof path, "%s/cells.marker", argv[2]);
    o = fopen(path, "ab"); if (!o) return 2; fputc('x', o); fclose(o);
    printf("ATLAS bytes=%ld skin=rmmv\n", n);
    free(b);
    return 0;
}
