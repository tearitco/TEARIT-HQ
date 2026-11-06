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
static int W = 32, H = 24, RW = 5, RH = 5, BW = 2, BH = 3, IW = 8, IH = 6;
static int wall_slot = 36;
static int leaf_n, sx[2], sy[2], ox[2], oy[2];
static struct { char dir[32]; float b[6]; } faces[8];
static int face_n;
static struct { int ox, oy; char dir[32]; } wins[32];
static int win_n;
static int face_id(const char *s) {
    if (!strcmp(s, "north")) return 0;
    if (!strcmp(s, "east")) return 1;
    if (!strcmp(s, "south")) return 2;
    if (!strcmp(s, "west")) return 3;
    return -1;
}
static int read_pdl(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "no parts file %s\n", path); return 0; }
    face_n = 0; win_n = 0;
    char ln[256];
    while (fgets(ln, sizeof ln, f)) {
        int a, b; char name[64], kind[32];
        float b0, b1, b2, b3, b4, b5;
        if (face_n < 8 && sscanf(ln, " FACE | %15s | %f | %f | %f | %f | %f | %f", kind, &b0, &b1, &b2, &b3, &b4, &b5) == 7) {
            snprintf(faces[face_n].dir, sizeof faces[0].dir, "%s", kind);
            faces[face_n].b[0] = b0; faces[face_n].b[1] = b1; faces[face_n].b[2] = b2;
            faces[face_n].b[3] = b3; faces[face_n].b[4] = b4; faces[face_n].b[5] = b5;
            face_n++;
            continue;
        }
        if (win_n < 32 && sscanf(ln, " WIN | %d | %d | %15s", &a, &b, kind) == 3) {
            wins[win_n].ox = a; wins[win_n].oy = b;
            snprintf(wins[win_n].dir, sizeof wins[0].dir, "%s", kind);
            win_n++;
            continue;
        }
        if (sscanf(ln, " PART | %63s | %d | %d | %31s", name, &a, &b, kind) == 4) {
            if (!strcmp(name, "centroid.island")) { W = a; H = b; }
            else if (!strcmp(name, "centroid.room")) { RW = a; RH = b; }
            else if (!strcmp(name, "centroid.bridge")) { BW = a; BH = b; }
            else if (!strcmp(name, "centroid.isle")) { IW = a; IH = b; }
        }
    }
    fclose(f);
    if (W < 3 || H < 3 || W > MAXD || H > MAXD || RW < 3 || RH < 3) return 0;
    if (BW < 1 || BH < 1 || IW < 1 || IH < 1) return 0;
    if (H + BH + IH > MAXD) return 0;
    return 1;
}
static void place_door(void) {
    /* Room origin stays at 6,6 so the camera already aimed at this room still sees the door. */
    int x0 = 6, y0 = 6, y = y0 + RH - 1, x = x0 + RW / 2 - 1;
    leaf_n = 2;
    sx[0] = x; sy[0] = y; ox[0] = x; oy[0] = y - 1;
    sx[1] = x + 1; sy[1] = y; ox[1] = x + 1; oy[1] = y - 1;
}
static int depth_in(int x, int y, int x0, int y0, int w, int h) {
    int lx = x - x0, ly = y - y0;
    int dx = lx < (w - 1 - lx) ? lx : (w - 1 - lx);
    int dy = ly < (h - 1 - ly) ? ly : (h - 1 - ly);
    int edge = dx < dy ? dx : dy;
    int d = edge + 1;
    return d > 6 ? 6 : d;
}
static void write_event(const char *cells, int x, int y) {
    char p[4200]; snprintf(p, sizeof p, "%s", cells);
    char *sl = strrchr(p, '/');
    if (sl) snprintf(sl + 1, sizeof p - (size_t)(sl + 1 - p), "events.txt");
    else snprintf(p, sizeof p, "events.txt");
    FILE *f = fopen(p, "w");
    if (!f) return;
    /* Same brown as the office door emoji voxels. Name door loads door_0_2_1.rgba. */
    fprintf(f, "%d %d 132 75 49 door 0 2 1\n", x, y);
    /* Windows come from WIN rows. The wall cells stay three cubes. The pane is only a sheet. */
    int x0 = 6, y0 = 6;
    for (int i = 0; i < win_n; i++) {
        int id = face_id(wins[i].dir), fi = -1;
        for (int k = 0; k < face_n; k++) if (!strcmp(faces[k].dir, wins[i].dir)) fi = k;
        if (id < 0 || fi < 0) continue;
        float *b = faces[fi].b;
        fprintf(f, "%d %d 176 214 230 window 0 %d 0 %.2f %.2f %.2f %.2f %.2f %.2f\n",
                x0 + wins[i].ox, y0 + wins[i].oy, id, b[0], b[1], b[2], b[3], b[4], b[5]);
    }
    /* One flat cap on every cell of the room, including the wall tops. The
     * opening stays open underneath. Gray so it is not the door. */
    for (int ry = y0; ry < y0 + RH; ry++)
        for (int rx = x0; rx < x0 + RW; rx++)
            fprintf(f, "%d %d 158 156 150 roof 0 0 0\n", rx, ry);
    fclose(f);
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
/* kind: 0 air, 1 land (taper under it), 2 bridge (floor only, sky under it). */
static int split_ok(int kind[MAXD][MAXD], int total, int ix, int iy) {
    int seen[MAXD][MAXD];
    int qx[MAXD * MAXD], qy[MAXD * MAXD], qh = 0, qt = 0;
    memset(seen, 0, sizeof seen);
    qx[qt] = ix; qy[qt] = iy; qt++; seen[iy][ix] = 1;
    while (qh < qt) {
        int x = qx[qh], y = qy[qh]; qh++;
        if (y < H) return 0;                 /* the side island reached the main land with the bridge removed */
        int d[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
        for (int k = 0; k < 4; k++) {
            int nx = x + d[k][0], ny = y + d[k][1];
            if (nx < 0 || ny < 0 || nx >= W || ny >= total || seen[ny][nx]) continue;
            if (kind[ny][nx] != 1) continue;
            seen[ny][nx] = 1; qx[qt] = nx; qy[qt] = ny; qt++;
        }
    }
    return 1;
}
static int stamp(const char *cells, const char *pdl) {
    if (!read_pdl(pdl)) return 1;
    place_door();
    int x0 = 6, y0 = 6;
    int total = H + BH + IH;
    int bx = 6;                                          /* south of the main island, in front of selector x=7 */
    int ix = bx - (IW - BW) / 2; if (ix < 0) ix = 0;
    int iy = H + BH;
    if (bx + BW > W || ix + IW > W || iy + IH != total) return 1;
    static int F[MAXD][MAXD], Wv[MAXD][MAXD], Dp[MAXD][MAXD], kind[MAXD][MAXD];
    for (int y = 0; y < total; y++) {
        for (int x = 0; x < W; x++) {
            F[y][x] = 0; Wv[y][x] = 0; Dp[y][x] = 0; kind[y][x] = 0;
            if (y < H) {
                int wall = 0;
                int in = x >= x0 && x < x0 + RW && y >= y0 && y < y0 + RH;
                int rim = in && (x == x0 || x == x0 + RW - 1 || y == y0 || y == y0 + RH - 1);
                int opening = (x == sx[1] && y == sy[1]);
                if (rim && !opening) wall = wall_slot;
                F[y][x] = 1; Wv[y][x] = wall; Dp[y][x] = depth_in(x, y, 0, 0, W, H); kind[y][x] = 1;
            } else if (y < H + BH && x >= bx && x < bx + BW) {
                F[y][x] = 1; kind[y][x] = 2;          /* depth 0: the span has no body under it */
            } else if (y >= iy && x >= ix && x < ix + IW) {
                F[y][x] = 1; Dp[y][x] = depth_in(x, y, ix, iy, IW, IH); kind[y][x] = 1;
            }
        }
    }
    if (!split_ok(kind, total, ix, iy)) { fprintf(stderr, "bridge does not split the islands\n"); return 1; }
    FILE *o = fopen(cells, "w");
    if (!o) { perror(cells); return 1; }
    fprintf(o, "width=%d\nheight=%d\ntile_px=24\natlas_cols=16\natlas_tiles=43\nrock_slot=%d\ncells\n", W, total, wall_slot);
    for (int y = 0; y < total; y++) {
        for (int x = 0; x < W; x++) fprintf(o, "%s%d,%d,%d", x ? " " : "", F[y][x], Wv[y][x], Dp[y][x]);
        fputc('\n', o);
    }
    fclose(o);
    write_door_file(cells, 0);
    write_event(cells, sx[1], sy[1]);
    printf("STAMP w=%d h=%d room=%dx%d door=%d,%d+%d,%d bridge=%dx%d isle=%dx%d\n",
           W, H, RW, RH, sx[0], sy[0], sx[1], sy[1], BW, BH, IW, IH);
    printf("SPLIT ok\n");
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
    /* The opening stays a hole in the wall. The picture moves; the rock does not. */
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
    write_event(cells, nb ? ox[1] : sx[1], nb ? oy[1] : sy[1]);
    printf("%s bit=%d\n", nb ? "OPEN" : "SHUT", nb);
    return 0;
}
static int apply_part(const char *part, const char *cells, const char *pdl) {
    static const char *shown[] = { "floor", "wall", "door", "roof", "bridge" };
    if (!strcmp(part, "all")) {
        for (int i = 0; i < 5; i++) {
            int rc = stamp(cells, pdl);
            if (rc) return rc;
            printf("APPLY %s\n", shown[i]);
        }
        return 0;
    }
    int known = 0;
    for (int i = 0; i < 5; i++) if (!strcmp(part, shown[i])) known = 1;
    if (!known) { fprintf(stderr, "no part %s\n", part); return 2; }
    int rc = stamp(cells, pdl);
    if (rc) return rc;
    printf("APPLY %s\n", part);
    return 0;
}
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: centroid_parts_op stamp <cells.txt> [parts.pdl] | apply <floor|wall|door|roof|bridge|all> <cells.txt> [parts.pdl] | door <cells.txt>\n"); return 2; }
    if (!strcmp(argv[1], "stamp")) return stamp(argv[2], argc > 3 ? argv[3] : "centroid.parts.pdl");
    if (!strcmp(argv[1], "apply")) {
        if (argc < 4) { fprintf(stderr, "usage: centroid_parts_op apply <part> <cells.txt> [parts.pdl]\n"); return 2; }
        return apply_part(argv[2], argv[3], argc > 4 ? argv[4] : "centroid.parts.pdl");
    }
    if (!strcmp(argv[1], "door")) return door_flip(argv[2]);
    fprintf(stderr, "usage: centroid_parts_op stamp <cells.txt> [parts.pdl] | apply <floor|wall|door|roof|bridge|all> <cells.txt> [parts.pdl] | door <cells.txt>\n");
    return 2;
}
