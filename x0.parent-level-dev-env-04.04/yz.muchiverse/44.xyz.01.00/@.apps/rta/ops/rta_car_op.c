/* rta_car_op — one car on a Battery road.
 * The ledger row is the position. events.txt is rewritten from that row
 * every call (a projection). The car steps +1 in x when that cell is a
 * road glyph R. Otherwise it stays. A missing CAR row starts on the
 * south-most road in the south third of the grid.
 *   rta_car_op step <grid.txt> <ledger.txt> <events.txt>
 *   rta_car_op --describe
 * Prints: x y
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o rta_car_op.+x rta_car_op.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXW 220
#define MAXH 300

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

static int is_road(char grid[][MAXW], int w, int h, int x, int y) {
    return x >= 0 && y >= 0 && x < w && y < h && grid[y][x] == 'R';
}

static int near_b(char grid[][MAXW], int w, int h, int x, int y) {
    for (int dy = -2; dy <= 2; dy++)
        for (int dx = -2; dx <= 2; dx++) {
            int yy = y + dy, xx = x + dx;
            if (yy >= 0 && xx >= 0 && yy < h && xx < w && grid[yy][xx] == 'B') return 1;
        }
    return 0;
}

static int south_road(char grid[][MAXW], int w, int h, int *x, int *y) {
    int ax = -1, ay = -1;
    for (int yy = h - 1; yy >= 0; yy--)
        for (int xx = 0; xx < w; xx++) {
            if (grid[yy][xx] != 'R') continue;
            if (ax < 0) { ax = xx; ay = yy; }
            if (near_b(grid, w, h, xx, yy)) { *x = xx; *y = yy; return 0; }
        }
    if (ax < 0) return -1;
    *x = ax; *y = ay;
    return 0;
}

static int last_car(const char *path, int *x, int *y) {
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;
    char line[256];
    int found = 0;
    while (fgets(line, sizeof line, fp)) {
        int cx, cy;
        if (sscanf(line, "CAR | %d | %d", &cx, &cy) == 2) {
            *x = cx; *y = cy; found = 1;
        }
    }
    fclose(fp);
    return found ? 0 : -1;
}

static int write_proj(const char *ledger, const char *events, int x, int y, const char *why) {
    FILE *lf = fopen(ledger, "a");
    if (!lf) return -1;
    fprintf(lf, "CAR | %d | %d | %s\n", x, y, why);
    fclose(lf);
    FILE *ef = fopen(events, "w");
    if (!ef) return -1;
    /* x y r g b charset index direction pattern. Yellow block, no sheet. */
    fprintf(ef, "%d %d 40 210 220 block 0 2 1\n", x, y);
    fclose(ef);
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--describe") == 0) {
        printf("rule rta_car inputs grid,ledger outputs CAR row and events.txt numbers none\n");
        return 0;
    }
    if (argc != 5 || strcmp(argv[1], "step") != 0) {
        fprintf(stderr, "usage: rta_car_op step <grid.txt> <ledger.txt> <events.txt>\n");
        return 2;
    }
    static char grid[MAXH][MAXW];
    int w = 0, h = 0;
    if (load_grid(argv[2], grid, &w, &h)) return 3;
    int x = 0, y = 0;
    const char *why = "step";
    if (last_car(argv[3], &x, &y) != 0) {
        if (south_road(grid, w, h, &x, &y)) return 4;
        why = "place";
    } else if (is_road(grid, w, h, x + 1, y)) {
        x += 1;
    }
    if (!is_road(grid, w, h, x, y)) return 5;
    if (write_proj(argv[3], argv[4], x, y, why)) return 6;
    printf("x=%d y=%d\n", x, y);
    return 0;
}
