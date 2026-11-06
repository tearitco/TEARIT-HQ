/* generator_voxel_op: tiny voxel models from rows, rendered to RPG Maker-sized PNG (48 x 48 per view). Decision D29.
 *   render <recipes.pdl> <model> <pose> <out.png>   4 views in a 192x48 strip: down, left, right, up
 *   sheet  <recipes.pdl> <model> <out.png>           one row per pose (idle first), 4 views each
 *   check  <recipes.pdl>                              parse only: counts, exit 2 on a bad row
 * Rows ('|' separated, '#' comments): MODEL | id | kind (char, item, building)
 *   PART | model | partid | x | y | z | w | d | h | #rrggbb      (a solid box in a 24 x 24 x 24 grid; x right, y depth (0 = front), z up)
 *   POSE | model | pose | partid | dx | dy | dz                   (moves that part for that pose: attack, mine, walk...)
 * One model, any view, any pose: the 2D sheet and the 3D board read the same rows. Colours are one flat ink-outlined tone with a lighter top and a darker right side. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../../&.widgits/_shared-lib/stb_image_write.h"
#define N 24
#define MAXP 256
#define MAXO 256
#define MAXM 64
typedef struct { char model[40], id[40]; int x, y, z, w, d, h; unsigned r, g, b; } Part;
typedef struct { char model[40], pose[40], part[40]; int dx, dy, dz; } Pose;
typedef struct { char id[40], kind[16]; } Model;
static Part P[MAXP]; static int np;
static Pose O[MAXO]; static int no;
static Model M[MAXM]; static int nm;
static int hexv(int c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }
static char *fld(char **p) {
    char *s = *p, *e; while (*s == ' ') s++;
    e = strchr(s, '|'); if (e) { *e = 0; *p = e + 1; } else { e = s + strlen(s); *p = e; }
    while (e > s && (e[-1] == ' ' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0;
    return s;
}
static int read_rows(const char *path) {
    FILE *f = fopen(path, "r"); char line[600];
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return 2; }
    while (fgets(line, sizeof line, f)) {
        char *p = line, *k;
        if (line[0] == '#' || line[0] == '\n') continue;
        k = fld(&p);
        if (!strcmp(k, "MODEL")) {
            if (nm >= MAXM) { fprintf(stderr, "too many models\n"); fclose(f); return 2; }
            snprintf(M[nm].id, sizeof M[nm].id, "%s", fld(&p)); snprintf(M[nm].kind, sizeof M[nm].kind, "%s", fld(&p));
            if (!M[nm].id[0] || (strcmp(M[nm].kind, "char") && strcmp(M[nm].kind, "item") && strcmp(M[nm].kind, "building"))) { fprintf(stderr, "bad MODEL row: %s\n", line); fclose(f); return 2; }
            nm++;
        } else if (!strcmp(k, "PART")) {
            Part *q = &P[np]; char *c; int i, v[6];
            if (np >= MAXP) { fprintf(stderr, "too many parts\n"); fclose(f); return 2; }
            snprintf(q->model, sizeof q->model, "%s", fld(&p)); snprintf(q->id, sizeof q->id, "%s", fld(&p));
            q->x = atoi(fld(&p)); q->y = atoi(fld(&p)); q->z = atoi(fld(&p)); q->w = atoi(fld(&p)); q->d = atoi(fld(&p)); q->h = atoi(fld(&p));
            c = fld(&p); if (*c == '#') c++;
            for (i = 0; i < 6; i++) v[i] = hexv(c[i]);
            if (!q->model[0] || !q->id[0] || strlen(c) != 6 || v[0] < 0 || v[1] < 0 || v[2] < 0 || v[3] < 0 || v[4] < 0 || v[5] < 0 || q->w < 1 || q->d < 1 || q->h < 1 || q->x < 0 || q->y < 0 || q->z < 0 || q->x + q->w > N || q->y + q->d > N || q->z + q->h > N) { fprintf(stderr, "bad PART row: %s\n", line); fclose(f); return 2; }
            q->r = v[0] * 16 + v[1]; q->g = v[2] * 16 + v[3]; q->b = v[4] * 16 + v[5]; np++;
        } else if (!strcmp(k, "POSE")) {
            Pose *q = &O[no];
            if (no >= MAXO) { fprintf(stderr, "too many poses\n"); fclose(f); return 2; }
            snprintf(q->model, sizeof q->model, "%s", fld(&p)); snprintf(q->pose, sizeof q->pose, "%s", fld(&p)); snprintf(q->part, sizeof q->part, "%s", fld(&p));
            q->dx = atoi(fld(&p)); q->dy = atoi(fld(&p)); q->dz = atoi(fld(&p));
            if (!q->model[0] || !q->pose[0] || !q->part[0]) { fprintf(stderr, "bad POSE row: %s\n", line); fclose(f); return 2; }
            no++;
        }
    }
    fclose(f);
    return 0;
}
static int has_model(const char *id) { int i; for (i = 0; i < nm; i++) if (!strcmp(M[i].id, id)) return 1; return 0; }
static int check_refs(void) { /* every PART and POSE names a real model; every POSE names a real part of it */
    int i, j, bad = 0;
    for (i = 0; i < np; i++) if (!has_model(P[i].model)) { fprintf(stderr, "PART %s: unknown model %s\n", P[i].id, P[i].model); bad = 1; }
    for (i = 0; i < no; i++) {
        int ok = 0; for (j = 0; j < np; j++) if (!strcmp(P[j].model, O[i].model) && !strcmp(P[j].id, O[i].part)) ok = 1;
        if (!ok) { fprintf(stderr, "POSE %s: no part %s in model %s\n", O[i].pose, O[i].part, O[i].model); bad = 1; }
    }
    return bad ? 2 : 0;
}
static unsigned G[N][N][N]; /* [z][y][x]: 0 empty, else 0xFF000000 | rgb */
static int build(const char *model, const char *pose, int *count) {
    int i, j, x, y, z; *count = 0;
    memset(G, 0, sizeof G);
    for (i = 0; i < np; i++) {
        int dx = 0, dy = 0, dz = 0;
        if (strcmp(P[i].model, model)) continue;
        for (j = 0; j < no; j++) if (!strcmp(O[j].model, model) && !strcmp(O[j].pose, pose) && !strcmp(O[j].part, P[i].id)) { dx += O[j].dx; dy += O[j].dy; dz += O[j].dz; }
        for (z = 0; z < P[i].h; z++) for (y = 0; y < P[i].d; y++) for (x = 0; x < P[i].w; x++) {
            int X = P[i].x + x + dx, Y = P[i].y + y + dy, Z = P[i].z + z + dz;
            if (X < 0 || Y < 0 || Z < 0 || X >= N || Y >= N || Z >= N) continue;
            if (!G[Z][Y][X]) (*count)++;
            G[Z][Y][X] = 0xFF000000u | (P[i].r << 16) | (P[i].g << 8) | P[i].b;
        }
    }
    return 0;
}
static void px(unsigned char *o, int ox, int sw, int x, int y, unsigned c, int shade) {
    unsigned char *q; int r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
    if (x < 0 || y < 0 || x >= 48 || y >= 48) return;
    if (shade > 0) { r += (255 - r) / 3; g += (255 - g) / 3; b += (255 - b) / 3; }
    if (shade < 0) { r = r * 2 / 3; g = g * 2 / 3; b = b * 2 / 3; }
    q = o + (y * sw + ox + x) * 4; q[0] = r; q[1] = g; q[2] = b; q[3] = 255;
}
/* view 0 down (front), 1 left, 2 right, 3 up (back): the grid is rotated about its centre, then drawn far to near */
static void view(unsigned char *o, int ox, int sw, int v) {
    int x, y, z, X, Y;
    for (y = N - 1; y >= 0; y--) for (z = 0; z < N; z++) for (x = 0; x < N; x++) {
        unsigned c; int sx, sy;
        if (v == 0) { X = x; Y = y; } else if (v == 3) { X = N - 1 - x; Y = N - 1 - y; } else if (v == 1) { X = y; Y = N - 1 - x; } else { X = N - 1 - y; Y = x; }
        c = G[z][Y][X]; if (!c) continue;
        sx = x * 2; sy = 44 - z * 2 - y / 2;
        { int rx, ry; int ex = 0; /* is the voxel to the right (in view) empty? */
          rx = x + 1; if (rx >= N) ex = 1; else { int RX, RY; if (v == 0) { RX = rx; RY = y; } else if (v == 3) { RX = N - 1 - rx; RY = N - 1 - y; } else if (v == 1) { RX = y; RY = N - 1 - rx; } else { RX = N - 1 - y; RY = rx; } ex = !G[z][RY][RX]; }
          px(o, ox, sw, sx, sy, c, 0); px(o, ox, sw, sx, sy + 1, c, 0); px(o, ox, sw, sx + 1, sy, c, ex ? -1 : 0); px(o, ox, sw, sx + 1, sy + 1, c, ex ? -1 : 0);
          { int tz = z + 1, empty; if (tz >= N) empty = 1; else empty = !G[tz][Y][X]; if (empty) { px(o, ox, sw, sx, sy - 1, c, 1); px(o, ox, sw, sx + 1, sy - 1, c, 1); } }
          (void)ry; }
    }
}
static void outline(unsigned char *o, int ox, int sw) { /* a dark ink pixel on every empty pixel that touches a filled one (inside the 48 x 48 cell) */
    static unsigned char t[48 * 48]; int x, y;
    for (y = 0; y < 48; y++) for (x = 0; x < 48; x++) t[y * 48 + x] = o[(y * sw + ox + x) * 4 + 3] != 0;
    for (y = 0; y < 48; y++) for (x = 0; x < 48; x++) {
        if (t[y * 48 + x]) continue;
        if ((x > 0 && t[y * 48 + x - 1]) || (x < 47 && t[y * 48 + x + 1]) || (y > 0 && t[(y - 1) * 48 + x]) || (y < 47 && t[(y + 1) * 48 + x])) { unsigned char *q = o + (y * sw + ox + x) * 4; q[0] = 20; q[1] = 20; q[2] = 28; q[3] = 255; }
    }
}
static int render_row(unsigned char *o, int row, int sw, const char *model, const char *pose, int *count) {
    int v; build(model, pose, count);
    if (!*count) return 3;
    for (v = 0; v < 4; v++) { unsigned char *base = o + (size_t)row * 48 * sw * 4; view(base, v * 48, sw, v); outline(base, v * 48, sw); }
    return 0;
}
static int opaque(const unsigned char *o, int sw, int row, int n) { int c = 0, x, y; for (y = 0; y < 48; y++) for (x = 0; x < n; x++) if (o[((row * 48 + y) * sw + x) * 4 + 3]) c++; return c; }
int main(int argc, char **argv) {
    int rc;
    if (argc < 3) { fprintf(stderr, "usage: render <recipes> <model> <pose> <out.png> | sheet <recipes> <model> <out.png> | check <recipes>\n"); return 2; }
    rc = read_rows(argv[2]); if (rc) return rc;
    rc = check_refs(); if (rc) return rc;
    if (!strcmp(argv[1], "check") && argc == 3) { printf("VOXELCHECK models=%d parts=%d poses=%d\n", nm, np, no); return 0; }
    if (!strcmp(argv[1], "render") && argc == 6) {
        static unsigned char o[192 * 48 * 4]; int count;
        if (!has_model(argv[3])) { fprintf(stderr, "unknown model %s\n", argv[3]); return 2; }
        memset(o, 0, sizeof o); rc = render_row(o, 0, 192, argv[3], argv[4], &count); if (rc) { fprintf(stderr, "model %s has no voxels\n", argv[3]); return rc; }
        if (!stbi_write_png(argv[5], 192, 48, 4, o, 192 * 4)) return 2;
        printf("VOXEL model=%s pose=%s views=4 voxels=%d opaque=%d\n", argv[3], argv[4], count, opaque(o, 192, 0, 192)); return 0;
    }
    if (!strcmp(argv[1], "sheet") && argc == 5) {
        char names[32][40]; int k = 1, i, j, count; unsigned char *o;
        if (!has_model(argv[3])) { fprintf(stderr, "unknown model %s\n", argv[3]); return 2; }
        snprintf(names[0], 40, "idle");
        for (i = 0; i < no && k < 32; i++) { int dup = 0; if (strcmp(O[i].model, argv[3])) continue; for (j = 0; j < k; j++) if (!strcmp(names[j], O[i].pose)) dup = 1; if (!dup) snprintf(names[k++], 40, "%s", O[i].pose); }
        o = calloc((size_t)192 * 48 * k * 4, 1); if (!o) return 2;
        for (i = 0; i < k; i++) { rc = render_row(o, i, 192, argv[3], names[i], &count); if (rc) { free(o); return rc; } }
        if (!stbi_write_png(argv[4], 192, 48 * k, 4, o, 192 * 4)) { free(o); return 2; }
        printf("VOXELSHEET model=%s poses=%d size=192x%d\n", argv[3], k, 48 * k); free(o); return 0;
    }
    fprintf(stderr, "bad arguments\n"); return 2;
}
