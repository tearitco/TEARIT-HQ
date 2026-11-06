/* centroid_parts_op stamp <cells.txt> [parts.pdl]
 * centroid_parts_op door <cells.txt>
 * The pdl is the blueprint (PART rows). Stamp writes the island, the
 * room, and a two-voxel door. door flips that door: shut, the two
 * leaves fill the gap; open, they sit one cell inside the room.
 * The camera is not written. A door never names another page. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAXD 48
static int W = 32, H = 24, RW = 5, RH = 5;
static int wall_slot = 36;
static int leaf_n, sx[2], sy[2], ox[2], oy[2];
static int read_pdl(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "no parts file %s\n", path); return 0; }
    char ln[256];
    while (fgets(ln, sizeof ln, f)) {
        int a, b; char name[64], kind[32];
        if (sscanf(ln, " PART | %63s | %d | %d | %31s", name, &a, &b, kind) == 4) {
            if (!strcmp(name, "centroid.island")) { W = a; H = b; }
            else if (!strcmp(name, "centroid.room")) { RW = a; RH = b; }
        }
    }
    fclose(f);
    if (W < 3 || H < 3 || W > MAXD || H > MAXD || RW < 3 || RH < 3) return 0;
    return 1;
}
static void place_door(void) {
    /* Room origin stays at 6,6 so the camera already aimed at this room still sees the door. */
    int x0 = 6, y0 = 6, y = y0 + RH - 1, x = x0 + RW / 2 - 1;
    leaf_n = 2;
    sx[0] = x; sy[0] = y; ox[0] = x; oy[0] = y - 1;
    sx[1] = x + 1; sy[1] = y; ox[1] = x + 1; oy[1] = y - 1;
}
static int depth_at(int x, int y) {
    int dx = x < (W - 1 - x) ? x : (W - 1 - x);
    int dy = y < (H - 1 - y) ? y : (H - 1 - y);
    int edge = dx < dy ? dx : dy;
    int d = edge + 1;
    return d > 6 ? 6 : d;
}
static void write_door_file(const char *cells, int bit) {
    char p[4200];
    snprintf(p, sizeof p, "%s", cells);
    char *sl = strrchr(p, '/');
    if (sl) snprintf(sl + 1, sizeof p - (size_t)(sl + 1 - p), "door.txt");
    else snprintf(p, sizeof p, "door.txt");
    FILE *f = fopen(p, "w");
    if (!f) return;
    fprintf(f, "bit=%d\n", bit);
    for (int i = 0; i < leaf_n; i++) fprintf(f, "leaf=%d,%d,%d,%d\n", sx[i], sy[i], ox[i], oy[i]);
    fclose(f);
}
static int stamp(const char *cells, const char *pdl) {
    if (!read_pdl(pdl)) return 1;
    place_door();
    int x0 = 6, y0 = 6;
    FILE *o = fopen(cells, "w");
    if (!o) { perror(cells); return 1; }
    fprintf(o, "width=%d\nheight=%d\ntile_px=24\natlas_cols=16\natlas_tiles=43\nrock_slot=%d\ncells\n", W, H, wall_slot);
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            int wall = 0;
            int in = x >= x0 && x < x0 + RW && y >= y0 && y < y0 + RH;
            int rim = in && (x == x0 || x == x0 + RW - 1 || y == y0 || y == y0 + RH - 1);
            int door = 0;
            for (int i = 0; i < leaf_n; i++) if (x == sx[i] && y == sy[i]) door = 1;
            if (rim || door) wall = wall_slot;
            fprintf(o, "%s1,%d,%d", x ? " " : "", wall, depth_at(x, y));
        }
        fputc('\n', o);
    }
    fclose(o);
    write_door_file(cells, 0);
    printf("STAMP w=%d h=%d room=%dx%d door=%d,%d+%d,%d\n", W, H, RW, RH, sx[0], sy[0], sx[1], sy[1]);
    return 0;
}
static int door_path(const char *cells, char *p, size_t n) {
    snprintf(p, n, "%s", cells);
    char *sl = strrchr(p, '/');
    if (sl) snprintf(sl + 1, n - (size_t)(sl + 1 - p), "door.txt");
    else snprintf(p, n, "door.txt");
    return 1;
}
static int door_flip(const char *cells) {
    char p[4200]; door_path(cells, p, sizeof p);
    FILE *df = fopen(p, "r");
    if (!df) { fprintf(stderr, "no door.txt beside %s\n", cells); return 1; }
    char ln[128]; int bit = 0; leaf_n = 0;
    while (fgets(ln, sizeof ln, df)) {
        int a, b, c, d;
        if (sscanf(ln, "bit=%d", &bit) == 1) continue;
        if (sscanf(ln, "leaf=%d,%d,%d,%d", &a, &b, &c, &d) == 4 && leaf_n < 2) {
            sx[leaf_n] = a; sy[leaf_n] = b; ox[leaf_n] = c; oy[leaf_n] = d; leaf_n++;
        }
    }
    fclose(df);
    if (leaf_n != 2) return 1;
    static int F[MAXD][MAXD], Wv[MAXD][MAXD], D[MAXD][MAXD];
    int gw = 0, gh = 0, hdr = 0;
    char head[1024]; head[0] = 0;
    FILE *cf = fopen(cells, "r");
    if (!cf) { perror(cells); return 1; }
    char raw[2048];
    while (fgets(raw, sizeof raw, cf)) {
        if (!hdr) {
            size_t L = strlen(head), n = strlen(raw);
            if (L + n < sizeof head) memcpy(head + L, raw, n + 1);
            if (!strncmp(raw, "cells", 5)) hdr = 1;
            continue;
        }
        if (gh >= MAXD) { fclose(cf); return 1; }
        int x = 0; char *s = raw;
        while (*s && x < MAXD) {
            if (*s == ' ' || *s == '\r' || *s == '\n') { s++; continue; }
            int f, w, d; if (sscanf(s, "%d,%d,%d", &f, &w, &d) < 2) break;
            F[gh][x] = f; Wv[gh][x] = w; D[gh][x] = d; x++;
            while (*s && *s != ' ' && *s != '\n') s++;
        }
        if (x > gw) gw = x;
        gh++;
    }
    fclose(cf);
    int nb = bit ? 0 : 1;
    for (int i = 0; i < leaf_n; i++) {
        int fx = bit ? ox[i] : sx[i], fy = bit ? oy[i] : sy[i];
        int tx = bit ? sx[i] : ox[i], ty = bit ? sy[i] : oy[i];
        if (fy < 0 || fx < 0 || ty < 0 || tx < 0 || fy >= gh || fx >= gw || ty >= gh || tx >= gw) return 1;
        int moved = Wv[fy][fx] ? Wv[fy][fx] : wall_slot;
        Wv[fy][fx] = 0;
        Wv[ty][tx] = moved;
    }
    cf = fopen(cells, "w");
    if (!cf) return 1;
    fputs(head, cf);
    for (int y = 0; y < gh; y++) {
        for (int x = 0; x < gw; x++) fprintf(cf, "%s%d,%d,%d", x ? " " : "", F[y][x], Wv[y][x], D[y][x]);
        fputc('\n', cf);
    }
    fclose(cf);
    FILE *out = fopen(p, "w");
    if (!out) return 1;
    fprintf(out, "bit=%d\n", nb);
    for (int i = 0; i < leaf_n; i++) fprintf(out, "leaf=%d,%d,%d,%d\n", sx[i], sy[i], ox[i], oy[i]);
    fclose(out);
    printf("%s bit=%d\n", nb ? "OPEN" : "SHUT", nb);
    return 0;
}
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: centroid_parts_op stamp <cells.txt> [parts.pdl] | door <cells.txt>\n"); return 2; }
    if (!strcmp(argv[1], "stamp")) return stamp(argv[2], argc > 3 ? argv[3] : "centroid.parts.pdl");
    if (!strcmp(argv[1], "door")) return door_flip(argv[2]);
    fprintf(stderr, "usage: centroid_parts_op stamp <cells.txt> [parts.pdl] | door <cells.txt>\n");
    return 2;
}
