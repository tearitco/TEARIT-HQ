/* rta_walk_op — one step on foot in Centon.
 * The ledger row is the position. A missing WALK row starts on the
 * south-most cell you can stand on. A step moves +1 x when that cell
 * is walkable, and stays when it is not. Walkable glyphs are grass,
 * park, path, road, beach, and pier. Water and a roof are not.
 *   rta_walk_op step <grid.txt> <ledger.txt>
 * Prints: x y
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXW 220
#define MAXH 300

static int walkable(char g) {
    return g == '.' || g == 'K' || g == 'P' || g == 'R' || g == 'S' || g == 'i';
}

static int load_grid(const char *path, char grid[][MAXW], int *w, int *h) {
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;
    char row[MAXW + 8];
    *w = 0; *h = 0;
    while (fgets(row, sizeof row, fp)) {
        if (row[0] == '#' || row[0] == '\n') continue;
        int n = (int)strcspn(row, "\r\n");
        if (n < 1 || n > MAXW || *h >= MAXH) { fclose(fp); return -1; }
        memcpy(grid[*h], row, (size_t)n);
        if (*w == 0) *w = n;
        if (n != *w) { fclose(fp); return -1; }
        (*h)++;
    }
    fclose(fp);
    return (*w > 0 && *h > 0) ? 0 : -1;
}

static int last_walk(const char *path, int *x, int *y) {
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;
    char line[256];
    int found = 0, cx, cy;
    while (fgets(line, sizeof line, fp))
        if (sscanf(line, "WALK | %d | %d", &cx, &cy) == 2) { *x = cx; *y = cy; found = 1; }
    fclose(fp);
    return found ? 0 : -1;
}

int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--describe")) {
        printf("rule rta_walk inputs grid,ledger outputs WALK row numbers none\n");
        return 0;
    }
    if (argc != 4 || strcmp(argv[1], "step")) {
        fprintf(stderr, "usage: rta_walk_op step <grid.txt> <ledger.txt>\n");
        return 2;
    }
    static char grid[MAXH][MAXW];
    int w = 0, h = 0, x = 0, y = 0;
    if (load_grid(argv[2], grid, &w, &h)) return 3;
    const char *why = "step";
    if (last_walk(argv[3], &x, &y)) {
        int found = 0;
        for (int yy = h - 1; yy >= 0 && !found; yy--)
            for (int xx = 0; xx < w; xx++)
                if (walkable(grid[yy][xx])) { x = xx; y = yy; found = 1; break; }
        if (!found) return 4;
        why = "place";
    } else if (x + 1 < w && y >= 0 && y < h && walkable(grid[y][x + 1])) {
        x += 1;
    }
    if (y < 0 || y >= h || x < 0 || x >= w || !walkable(grid[y][x])) return 5;
    FILE *lf = fopen(argv[3], "a");
    if (!lf) return 6;
    fprintf(lf, "WALK | %d | %d | %s\n", x, y, why);
    fclose(lf);
    printf("x=%d y=%d\n", x, y);
    return 0;
}
