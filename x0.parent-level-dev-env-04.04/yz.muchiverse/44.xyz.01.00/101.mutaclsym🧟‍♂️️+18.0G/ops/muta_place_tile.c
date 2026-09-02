/* muta_place_tile - set one ASCII cell in map.txt
 * Usage: muta_place_tile.+x <project_root> <map_id> <x> <y> <glyph>
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PATH_BUF 4352
#define MAX_LINE 4096
#define MAX_ROWS 256

int main(int argc, char **argv) {
    if (argc < 6) {
        fprintf(stderr, "Usage: muta_place_tile.+x <project_root> <map_id> <x> <y> <glyph>\n");
        return 1;
    }
    const char *proj = argv[1];
    const char *map_id = argv[2];
    int x = atoi(argv[3]);
    int y = atoi(argv[4]);
    char glyph = argv[5][0];
    if (glyph < 32 || glyph > 126 || x < 0 || y < 0) {
        fprintf(stderr, "place_tile: bad args\n");
        return 1;
    }

    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/world_01/%s/map.txt", proj, map_id);
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "place_tile: cannot open %s\n", path);
        return 1;
    }

    char rows[MAX_ROWS][MAX_LINE];
    int nrows = 0;
    while (nrows < MAX_ROWS && fgets(rows[nrows], MAX_LINE, f)) {
        rows[nrows][strcspn(rows[nrows], "\r\n")] = '\0';
        nrows++;
    }
    fclose(f);

    if (y >= nrows) {
        fprintf(stderr, "place_tile: y out of range\n");
        return 1;
    }

    size_t len = strlen(rows[y]);
    if ((size_t)x >= len) {
        while (len < (size_t)x && len + 1 < MAX_LINE - 1)
            rows[y][len++] = ' ';
        rows[y][x] = glyph;
        rows[y][x + 1] = '\0';
    } else {
        rows[y][x] = glyph;
    }

    f = fopen(path, "w");
    if (!f) return 1;
    for (int i = 0; i < nrows; i++)
        fprintf(f, "%s\n", rows[i]);
    fclose(f);

    printf("place_tile: %s (%d,%d)='%c'\n", map_id, x, y, glyph);
    return 0;
}
