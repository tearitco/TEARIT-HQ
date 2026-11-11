/* enter_op stamp <cells.txt> <x0> <y0>
 * enter_op walk <cells.txt> <door.txt> <ledger> <x> <y> <z> <open|shut> <moves>
 *
 * stamp paints the test-page house (5 by 5, wall slot 36, south door,
 * one stair cell whose depth is one higher) onto an existing cells.txt.
 * walk appends EVENT rows. A wall blocks. A shut door blocks. A rise or
 * drop of more than 1 refuses. Sky refuses. The camera is not written.
 * Exit 2 when a move is refused. Exit 0 when every move lands.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAXW 80
#define MAXH 64
static int W, H;
static int fl[MAXH][MAXW], wl[MAXH][MAXW], dep[MAXH][MAXW];
static int door_bit, leaf_n, lx[4], ly[4];
static int load_cells(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[4096];
    int in = 0, row = 0;
    W = H = 0;
    while (fgets(line, sizeof line, f)) {
        if (!in) {
            if (!strncmp(line, "width=", 6)) W = atoi(line + 6);
            else if (!strncmp(line, "height=", 7)) H = atoi(line + 7);
            else if (!strncmp(line, "cells", 5)) in = 1;
            continue;
        }
        if (row >= H || row >= MAXH) break;
        char *q = line;
        for (int x = 0; x < W && x < MAXW; x++) {
            char *e; long a = strtol(q, &e, 10);
            if (e == q || *e != ',') break;
            q = e + 1; long b = strtol(q, &e, 10); q = e;
            long d = 0;
            if (*q == ',') { d = strtol(q + 1, &e, 10); q = e; }
            fl[row][x] = (int)a; wl[row][x] = (int)b; dep[row][x] = (int)d;
            while (*q == ' ') q++;
        }
        row++;
    }
    fclose(f);
    return in && row == H && W > 0 && W <= MAXW && H > 0 && H <= MAXH;
}
static int save_cells(const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return 0;
    fprintf(f, "width=%d\nheight=%d\ntile_px=24\natlas_cols=16\natlas_tiles=43\nrock_slot=36\ncells\n", W, H);
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            if (x) fputc(' ', f);
            fprintf(f, "%d,%d,%d", fl[y][x], wl[y][x], dep[y][x]);
        }
        fputc('\n', f);
    }
    fclose(f);
    return 1;
}
static void beside(const char *cells, const char *name, char *out, size_t n) {
    snprintf(out, n, "%s", cells);
    char *sl = strrchr(out, '/');
    if (sl) snprintf(sl + 1, n - (size_t)(sl + 1 - out), "%s", name);
    else snprintf(out, n, "%s", name);
}
static int stamp(const char *cells, int x0, int y0) {
    if (!load_cells(cells)) return 1;
    if (x0 < 0 || y0 < 0 || x0 + 5 > W || y0 + 5 > H) return 2;
    for (int y = 0; y < 5; y++) for (int x = 0; x < 5; x++) {
        int edge = (x == 0 || y == 0 || x == 4 || y == 4);
        int door = (y == 4 && (x == 2 || x == 3));
        int stair = (x == 2 && y == 2);
        fl[y0 + y][x0 + x] = 1;
        wl[y0 + y][x0 + x] = (edge && !door) ? 36 : 0;
        dep[y0 + y][x0 + x] = stair ? 7 : 6;
    }
    if (!save_cells(cells)) return 1;
    char doorp[4200]; beside(cells, "door.txt", doorp, sizeof doorp);
    FILE *d = fopen(doorp, "w");
    if (!d) return 1;
    fprintf(d, "bit=0\nleaf=%d,%d,%d,%d\nleaf=%d,%d,%d,%d\n",
            x0 + 2, y0 + 4, x0 + 2, y0 + 3, x0 + 3, y0 + 4, x0 + 3, y0 + 3);
    fclose(d);
    printf("STAMP x0=%d y0=%d door=%d,%d stair=%d,%d\n", x0, y0, x0 + 2, y0 + 4, x0 + 2, y0 + 2);
    return 0;
}
static int load_door(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char ln[128]; leaf_n = 0; door_bit = 0;
    while (fgets(ln, sizeof ln, f)) {
        int a, b, c, d;
        if (!strncmp(ln, "bit=", 4)) door_bit = atoi(ln + 4);
        else if (sscanf(ln, "leaf=%d,%d,%d,%d", &a, &b, &c, &d) == 4 && leaf_n < 4) {
            lx[leaf_n] = a; ly[leaf_n] = b; leaf_n++;
        }
    }
    fclose(f);
    return 1;
}
static int is_leaf(int x, int y) {
    for (int i = 0; i < leaf_n; i++) if (lx[i] == x && ly[i] == y) return 1;
    return 0;
}
static int walk(int argc, char **argv) {
    /* walk cells door ledger x y z open|shut moves */
    if (argc < 9) return 2;
    if (!load_cells(argv[2]) || !load_door(argv[3])) return 1;
    int x = atoi(argv[5]), y = atoi(argv[6]), z = atoi(argv[7]);
    int open = !strcmp(argv[8], "open");
    FILE *led = fopen(argv[4], "a");
    if (!led) return 1;
    int round = 0, rc = 0;
    for (int i = 9; i < argc; i++) {
        int nx = x, ny = y;
        if (!strcmp(argv[i], "N")) ny--;
        else if (!strcmp(argv[i], "S")) ny++;
        else if (!strcmp(argv[i], "E")) nx++;
        else if (!strcmp(argv[i], "W")) nx--;
        else { rc = 2; break; }
        const char *why = "ok";
        if (nx < 0 || ny < 0 || nx >= W || ny >= H || !fl[ny][nx]) why = "sky";
        else if (wl[ny][nx]) why = "wall";
        else if (is_leaf(nx, ny) && !open && !door_bit) why = "shut";
        else if (abs(dep[ny][nx] - z) > 1) why = "step";
        int ok = !strcmp(why, "ok");
        fprintf(led, "EVENT|%d|hero|walk|ground|%d,%d,%d|%s\n", round, nx, ny, ok ? dep[ny][nx] : z, why);
        if (!ok) { rc = 2; printf("REFUSED %s at %d,%d\n", why, nx, ny); break; }
        x = nx; y = ny; z = dep[ny][nx];
        round++;
    }
    fclose(led);
    if (rc == 0) printf("WALK x=%d y=%d z=%d steps=%d\n", x, y, z, round);
    return rc;
}
int main(int argc, char **argv) {
    if (argc >= 5 && !strcmp(argv[1], "stamp")) return stamp(argv[2], atoi(argv[3]), atoi(argv[4]));
    if (argc >= 9 && !strcmp(argv[1], "walk")) return walk(argc, argv);
    fprintf(stderr, "usage: enter_op stamp <cells> <x0> <y0>\n"
                    "       enter_op walk <cells> <door> <ledger> <x> <y> <z> <open|shut> <N|S|E|W>...\n");
    return 2;
}
