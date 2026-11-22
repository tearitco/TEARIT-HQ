/* rta_momentum_op — a car keeps its speed until the road ends.
 * The ledger row is the authority: MOM | x | y | vx | why
 * push sets vx (1 to 4) and, if there is no row yet, places the car
 * on the south-most road. step adds vx to x when that cell is road
 * glyph R. A cell that is not road zeroes vx and leaves x where it was.
 * events.txt is rewritten from the row (a projection).
 *   rta_momentum_op push <grid> <ledger> <events> <vx>
 *   rta_momentum_op step <grid> <ledger> <events>
 * Prints: x y vx
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

static int road_at(char grid[][MAXW], int w, int h, int x, int y) {
    return x >= 0 && y >= 0 && x < w && y < h && grid[y][x] == 'R';
}

static int south_road(char grid[][MAXW], int w, int h, int *x, int *y) {
    for (int yy = h - 1; yy >= 0; yy--)
        for (int xx = 0; xx < w; xx++)
            if (grid[yy][xx] == 'R') { *x = xx; *y = yy; return 0; }
    return -1;
}

static int last_mom(const char *path, int *x, int *y, int *vx) {
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;
    char line[256];
    int found = 0, cx, cy, cv;
    while (fgets(line, sizeof line, fp))
        if (sscanf(line, "MOM | %d | %d | %d", &cx, &cy, &cv) == 3) {
            *x = cx; *y = cy; *vx = cv; found = 1;
        }
    fclose(fp);
    return found ? 0 : -1;
}

static int write_row(const char *ledger, const char *events, int x, int y, int vx, const char *why) {
    FILE *lf = fopen(ledger, "a");
    if (!lf) return -1;
    fprintf(lf, "MOM | %d | %d | %d | %s\n", x, y, vx, why);
    fclose(lf);
    FILE *ef = fopen(events, "w");
    if (!ef) return -1;
    fprintf(ef, "%d %d 40 210 220 block 0 2 1\n", x, y);
    fclose(ef);
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--describe")) {
        printf("rule rta_momentum inputs grid,ledger,vx outputs MOM row numbers vx 1 to 4\n");
        return 0;
    }
    if (argc < 5) {
        fprintf(stderr, "usage: rta_momentum_op push <grid> <ledger> <events> <vx>\n"
                        "       rta_momentum_op step <grid> <ledger> <events>\n");
        return 2;
    }
    static char grid[MAXH][MAXW];
    int w = 0, h = 0;
    if (load_grid(argv[2], grid, &w, &h)) return 3;
    int x = 0, y = 0, vx = 0;
    int have = last_mom(argv[3], &x, &y, &vx) == 0;
    const char *why = "step";
    if (!strcmp(argv[1], "push")) {
        if (argc != 6) return 2;
        int nv = atoi(argv[5]);
        if (nv < 1 || nv > 4) return 4;
        if (!have && south_road(grid, w, h, &x, &y)) return 5;
        if (!road_at(grid, w, h, x, y)) return 5;
        vx = nv;
        why = "push";
    } else if (!strcmp(argv[1], "step") && argc == 5) {
        if (!have) {
            if (south_road(grid, w, h, &x, &y)) return 5;
            vx = 1;
            why = "place";
        } else if (road_at(grid, w, h, x + vx, y)) {
            x += vx;
        } else {
            vx = 0;
            why = "stop";
        }
    } else {
        return 2;
    }
    if (write_row(argv[3], argv[4], x, y, vx, why)) return 6;
    printf("x=%d y=%d vx=%d\n", x, y, vx);
    return 0;
}
