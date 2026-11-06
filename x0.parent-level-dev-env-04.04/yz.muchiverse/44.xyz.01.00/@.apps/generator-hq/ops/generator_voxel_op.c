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
#define MAXP 6000
#define MAXO 6000
#define MAXM 400
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
/* ---- characters from the cast file, walk sheets, RPG Maker slots, icons (D29 threads 1 to 3) ---- */
static void col(char *out, const char *s) { snprintf(out, 16, "%s%.7s", *s == '#' ? "" : "#", s); }
static void emit(FILE *o, const char *m, const char *id, int x, int y, int z, int w, int d, int h, const char *c) { char b[16]; col(b, c); fprintf(o, "PART | %s | %s | %d | %d | %d | %d | %d | %d | %s\n", m, id, x, y, z, w, d, h, b); }
static void pose(FILE *o, const char *m, const char *p, const char *part, int dx, int dy, int dz) { fprintf(o, "POSE | %s | %s | %s | %d | %d | %d\n", m, p, part, dx, dy, dz); }
/* ---- PARTSET reader (D30): the humanoid body is data in events/gen_voxel/partsets.pdl; Look carries the knobs ---- */
typedef struct { char set[40], id[40], ex[6][40], colour[24], when[120]; } PsRow;
typedef struct { char set[40], pose[40], part[40]; int dx, dy, dz; } PpRow;
typedef struct { char skin[16], style[16], hair[16], gar[16], trim[16], equip[160]; int bw, lh, hw, mark; } Look;
typedef struct { char id[40], slot[16], item[40], part[40], ex[6][40], colour[24], follow[40]; } EqRow;
static EqRow EQ[200]; static int neq;
static PsRow PS[600]; static int nps;
static PpRow PP[600]; static int npp;
static int load_partsets(const char *path) {
    FILE *f = fopen(path, "r"); char line[600];
    if (!f) { fprintf(stderr, "cannot open partsets %s\n", path); return 2; }
    while (fgets(line, sizeof line, f)) {
        char *p = line, *k; int i;
        if (line[0] == '#' || line[0] == '\n') continue;
        k = fld(&p);
        if (!strcmp(k, "PS")) {
            PsRow *q = &PS[nps];
            if (nps >= 600) { fprintf(stderr, "too many PS rows\n"); fclose(f); return 2; }
            snprintf(q->set, sizeof q->set, "%s", fld(&p)); snprintf(q->id, sizeof q->id, "%s", fld(&p));
            for (i = 0; i < 6; i++) snprintf(q->ex[i], sizeof q->ex[i], "%s", fld(&p));
            snprintf(q->colour, sizeof q->colour, "%s", fld(&p)); snprintf(q->when, sizeof q->when, "%s", fld(&p));
            if (!q->set[0] || !q->id[0] || !q->ex[5][0] || !q->colour[0]) { fprintf(stderr, "bad PS row: %s\n", line); fclose(f); return 2; }
            nps++;
        } else if (!strcmp(k, "PP")) {
            PpRow *q = &PP[npp];
            if (npp >= 600) { fprintf(stderr, "too many PP rows\n"); fclose(f); return 2; }
            snprintf(q->set, sizeof q->set, "%s", fld(&p)); snprintf(q->pose, sizeof q->pose, "%s", fld(&p)); snprintf(q->part, sizeof q->part, "%s", fld(&p));
            q->dx = atoi(fld(&p)); q->dy = atoi(fld(&p)); q->dz = atoi(fld(&p));
            if (!q->set[0] || !q->pose[0] || !q->part[0]) { fprintf(stderr, "bad PP row: %s\n", line); fclose(f); return 2; }
            npp++;
        } else if (!strcmp(k, "EQ")) {
            EqRow *q = &EQ[neq]; static const char *slots[5] = { "weapon", "shield", "head", "body", "accessory" }; int ok = 0;
            if (neq >= 200) { fprintf(stderr, "too many EQ rows\n"); fclose(f); return 2; }
            snprintf(q->id, sizeof q->id, "%s", fld(&p)); snprintf(q->slot, sizeof q->slot, "%s", fld(&p)); snprintf(q->item, sizeof q->item, "%s", fld(&p)); snprintf(q->part, sizeof q->part, "%s", fld(&p));
            for (i = 0; i < 6; i++) snprintf(q->ex[i], sizeof q->ex[i], "%s", fld(&p));
            snprintf(q->colour, sizeof q->colour, "%s", fld(&p)); snprintf(q->follow, sizeof q->follow, "%s", fld(&p));
            for (i = 0; i < 5; i++) if (!strcmp(q->slot, slots[i])) ok = 1;
            if (!q->id[0] || !ok || !q->item[0] || !q->part[0] || !q->ex[5][0] || !q->colour[0] || !q->follow[0]) { fprintf(stderr, "bad EQ row: %s\n", line); fclose(f); return 2; }
            neq++;
        }
    }
    fclose(f);
    return nps ? 0 : (fprintf(stderr, "no PS rows in %s\n", path), 2);
}
#define EV_BAD (-99999)
static int ev(const char *s, const Look *L) { /* integers and the variables tx bw lh hx hw joined by + and - */
    int sum = 0, sign = 1, tx = 12 - L->bw / 2, hx = 12 - L->hw / 2;
    while (*s) {
        int v;
        if (*s == ' ') { s++; continue; }
        if (*s == '+') { sign = 1; s++; continue; }
        if (*s == '-') { sign = -1; s++; continue; }
        if (!strncmp(s, "tx", 2)) v = tx; else if (!strncmp(s, "bw", 2)) v = L->bw; else if (!strncmp(s, "lh", 2)) v = L->lh;
        else if (!strncmp(s, "hx", 2)) v = hx; else if (!strncmp(s, "hw", 2)) v = L->hw;
        else if (*s >= '0' && *s <= '9') { v = 0; while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0'); sum += sign * v; sign = 1; continue; }
        else return EV_BAD;
        sum += sign * v; sign = 1; s += 2;
    }
    return sum;
}
static int when_ok(const char *when, const char *style, int mark, int *bad) { /* "mark=1,style=short,style!=masked": all must hold */
    char b[120], *t, *sv = NULL;
    snprintf(b, sizeof b, "%s", when);
    for (t = strtok_r(b, ",", &sv); t; t = strtok_r(NULL, ",", &sv)) {
        while (*t == ' ') t++;
        if (!strncmp(t, "mark=", 5)) { if (atoi(t + 5) != mark) return 0; }
        else if (!strncmp(t, "style!=", 7)) { if (!strcmp(t + 7, style)) return 0; }
        else if (!strncmp(t, "style=", 6)) { if (strcmp(t + 6, style)) return 0; }
        else { *bad = 1; return 0; }
    }
    return 1;
}
static const char *colour_of(const char *c, const Look *L) {
    return !strcmp(c, "$skin") ? L->skin : !strcmp(c, "$hair") ? L->hair : !strcmp(c, "$gar") ? L->gar : !strcmp(c, "$trim") ? L->trim : c;
}
static void lookof(Look *L, const char *skin, const char *style, const char *hair, const char *gar, const char *trim, int body, int mark) {
    snprintf(L->skin, 16, "%s", skin); snprintf(L->style, 16, "%s", style); snprintf(L->hair, 16, "%s", hair); snprintf(L->gar, 16, "%s", gar); snprintf(L->trim, 16, "%s", trim);
    L->bw = body == 0 ? 6 : body == 2 ? 10 : 8; L->lh = 5; L->hw = 8; L->mark = mark; L->equip[0] = 0;
}
static int human(FILE *o, const char *m, const Look *L) {
    int i, bad = 0;
    fprintf(o, "MODEL | %s | char\n", m);
    for (i = 0; i < nps; i++) {
        PsRow *q = &PS[i]; int v[6], j;
        if (strcmp(q->set, "humanoid")) continue;
        if (q->when[0] && !when_ok(q->when, L->style, L->mark, &bad)) { if (bad) { fprintf(stderr, "bad condition in PS %s: %s\n", q->id, q->when); return 2; } continue; }
        for (j = 0; j < 6; j++) { v[j] = ev(q->ex[j], L); if (v[j] == EV_BAD) { fprintf(stderr, "bad expression in PS %s: %s\n", q->id, q->ex[j]); return 2; } }
        emit(o, m, q->id, v[0], v[1], v[2], v[3], v[4], v[5], colour_of(q->colour, L));
    }
    { char b[160], *t, *sv = NULL; /* equipment: the parts of each named item, drawn after the body so they cover it */
      snprintf(b, sizeof b, "%s", L->equip);
      for (t = strtok_r(b, ",", &sv); t; t = strtok_r(NULL, ",", &sv)) for (i = 0; i < neq; i++) {
        EqRow *q = &EQ[i]; int v[6], j; char pid[128];
        if (strcmp(q->id, t)) continue;
        for (j = 0; j < 6; j++) { v[j] = ev(q->ex[j], L); if (v[j] == EV_BAD) { fprintf(stderr, "bad expression in EQ %s %s: %s\n", q->id, q->part, q->ex[j]); return 2; } }
        snprintf(pid, sizeof pid, "%.40s_%.40s", q->id, q->part);
        emit(o, m, pid, v[0], v[1], v[2], v[3], v[4], v[5], q->colour);
      } }
    for (i = 0; i < npp; i++) if (!strcmp(PP[i].set, "humanoid")) pose(o, m, PP[i].pose, PP[i].part, PP[i].dx, PP[i].dy, PP[i].dz);
    { char b[160], *t, *sv = NULL; /* a weapon or shield follows the hand: copy that part's pose offsets */
      snprintf(b, sizeof b, "%s", L->equip);
      for (t = strtok_r(b, ",", &sv); t; t = strtok_r(NULL, ",", &sv)) for (i = 0; i < neq; i++) {
        EqRow *q = &EQ[i]; int k; char pid[128];
        if (strcmp(q->id, t) || !strcmp(q->follow, "-")) continue;
        snprintf(pid, sizeof pid, "%.40s_%.40s", q->id, q->part);
        for (k = 0; k < npp; k++) if (!strcmp(PP[k].set, "humanoid") && !strcmp(PP[k].part, q->follow)) pose(o, m, PP[k].pose, pid, PP[k].dx, PP[k].dy, PP[k].dz);
      } }
    return 0;
}
static void creature(FILE *o, const char *m, const char *kind, const char *body, const char *belly, const char *ear, const char *glow) {
    fprintf(o, "MODEL | %s | char\n", m);
    if (!strcmp(kind, "quad")) {
        emit(o, m, "legl", 8, 9, 0, 2, 2, 4, ear); emit(o, m, "legr", 14, 9, 0, 2, 2, 4, ear); emit(o, m, "legl2", 8, 14, 0, 2, 2, 4, ear); emit(o, m, "legr2", 14, 14, 0, 2, 2, 4, ear);
        emit(o, m, "body", 7, 8, 4, 10, 8, 5, body); emit(o, m, "belly", 8, 8, 4, 8, 1, 2, belly); emit(o, m, "head", 9, 5, 6, 6, 4, 5, body);
        emit(o, m, "earl", 9, 6, 11, 2, 2, 2, ear); emit(o, m, "earr", 13, 6, 11, 2, 2, 2, ear); emit(o, m, "eyel", 10, 4, 9, 1, 1, 1, glow); emit(o, m, "eyer", 13, 4, 9, 1, 1, 1, glow);
        pose(o, m, "w1", "legl", 0, -1, 1); pose(o, m, "w1", "legr2", 0, 1, 1); pose(o, m, "w2", "legr", 0, -1, 1); pose(o, m, "w2", "legl2", 0, 1, 1);
    } else if (!strcmp(kind, "blob")) {
        emit(o, m, "base", 7, 8, 0, 10, 8, 2, body); emit(o, m, "mid", 8, 9, 2, 8, 6, 3, body); emit(o, m, "top", 9, 10, 5, 6, 4, 2, belly);
        emit(o, m, "eyel", 10, 8, 3, 1, 1, 1, glow); emit(o, m, "eyer", 13, 8, 3, 1, 1, 1, glow);
        pose(o, m, "w1", "top", 0, 0, 1); pose(o, m, "w2", "base", 0, 0, 0); pose(o, m, "w2", "mid", 0, 0, -1);
    } else if (!strcmp(kind, "winged")) {
        emit(o, m, "body", 9, 9, 6, 6, 5, 7, body); emit(o, m, "belly", 10, 8, 7, 4, 1, 4, belly); emit(o, m, "head", 9, 9, 13, 6, 5, 4, body);
        emit(o, m, "wingl", 3, 10, 8, 6, 1, 4, ear); emit(o, m, "wingr", 15, 10, 8, 6, 1, 4, ear); emit(o, m, "eyel", 10, 8, 15, 1, 1, 1, glow); emit(o, m, "eyer", 13, 8, 15, 1, 1, 1, glow);
        pose(o, m, "w1", "wingl", 0, 0, 3); pose(o, m, "w1", "wingr", 0, 0, 3); pose(o, m, "w2", "wingl", 0, 0, -2); pose(o, m, "w2", "wingr", 0, 0, -2);
    } else if (!strcmp(kind, "plant")) {
        emit(o, m, "stem", 11, 11, 0, 2, 2, 8, ear); emit(o, m, "leafl", 7, 11, 1, 4, 2, 1, ear); emit(o, m, "leafr", 15, 11, 1, 4, 2, 1, ear);
        emit(o, m, "bulb", 8, 8, 8, 8, 8, 4, body); emit(o, m, "jaws", 8, 8, 12, 8, 8, 2, belly); emit(o, m, "eyel", 10, 7, 11, 1, 1, 1, glow); emit(o, m, "eyer", 13, 7, 11, 1, 1, 1, glow);
        pose(o, m, "w1", "jaws", 0, 0, 2); pose(o, m, "w2", "jaws", 0, 0, 4);
    } else {
        emit(o, m, "dome", 6, 8, 8, 12, 8, 4, body); emit(o, m, "top", 8, 9, 12, 8, 6, 2, belly); emit(o, m, "eyel", 9, 7, 10, 1, 1, 1, glow); emit(o, m, "eyer", 14, 7, 10, 1, 1, 1, glow);
        emit(o, m, "t1", 7, 10, 0, 1, 1, 8, ear); emit(o, m, "t2", 10, 12, 0, 1, 1, 8, ear); emit(o, m, "t3", 13, 10, 0, 1, 1, 8, ear); emit(o, m, "t4", 16, 12, 0, 1, 1, 8, ear);
        pose(o, m, "w1", "t1", 1, 0, 0); pose(o, m, "w1", "t3", -1, 0, 0); pose(o, m, "w2", "t2", 1, 0, 0); pose(o, m, "w2", "t4", -1, 0, 0);
    }
}
static unsigned hash32(unsigned x) { x ^= x << 13; x ^= x >> 17; x ^= x << 5; x *= 2654435761u; x ^= x >> 15; return x; }
static int fromcast(const char *castf, const char *outf, const char *psf) {
    FILE *f = fopen(castf, "r"), *o; char line[600]; int made = 0;
    static const char *skins[5] = { "#e6bfa0", "#d6a47c", "#c08a60", "#8a5a3a", "#5a3a24" }, *hairs[6] = { "#1a1410", "#4a3020", "#8a5a30", "#c8a050", "#5a5a66", "#c04a8a" }, *gars[8] = { "#8a4a2e", "#2a4a7a", "#3a6a3a", "#6a3a7a", "#7a7a86", "#a08a3a", "#2a2a3a", "#a03a3a" }, *sty[3] = { "short", "long", "mohawk" };
    if (!f) { fprintf(stderr, "cannot open %s\n", castf); return 2; }
    if (load_partsets(psf)) { fclose(f); return 2; }
    o = fopen(outf, "w"); if (!o) { fclose(f); return 2; }
    fprintf(o, "# GENERATED by generator_voxel_op fromcast from the cast file and partsets.pdl. Do not edit: edit those. Echo (mark 2) is not drawn (voxels have no alpha); masked, clown and the kinds are.\n");
    while (fgets(line, sizeof line, f)) {
        char *p = line, *k, id[40], skin[16], style[16], hair[16], gar[16], trim[16], kind[16], pre[40]; int body, mark;
        if (line[0] == '#' || line[0] == '\n') continue;
        k = fld(&p);
        if (!strcmp(k, "CAST")) {
            snprintf(id, sizeof id, "%s", fld(&p)); snprintf(skin, sizeof skin, "%s", fld(&p)); snprintf(style, sizeof style, "%s", fld(&p)); snprintf(hair, sizeof hair, "%s", fld(&p));
            snprintf(gar, sizeof gar, "%s", fld(&p)); snprintf(trim, sizeof trim, "%s", fld(&p)); body = atoi(fld(&p)); mark = atoi(fld(&p)); snprintf(kind, sizeof kind, "%s", fld(&p));
            if (!id[0] || !skin[0]) { fprintf(stderr, "bad CAST row\n"); fclose(f); fclose(o); return 2; }
            if (kind[0] && strcmp(kind, "humanoid")) creature(o, id, kind, skin, hair, gar, trim); else { Look L; lookof(&L, skin, style, hair, gar, trim, body, mark); if (human(o, id, &L)) { fclose(f); fclose(o); return 2; } }
            made++;
        } else if (!strcmp(k, "GENERIC")) {
            int n, i; unsigned seed; snprintf(pre, sizeof pre, "%s", fld(&p)); n = atoi(fld(&p)); seed = (unsigned)atoi(fld(&p));
            for (i = 0; i < n; i++) { unsigned h = hash32(seed + (unsigned)i * 7919u + 1); char nm[64]; snprintf(nm, sizeof nm, "%s_%02d", pre, i);
                { Look L; lookof(&L, skins[h % 5], sty[(h >> 4) % 3], hairs[(h >> 8) % 6], gars[(h >> 12) % 8], "#ffd24a", (int)((h >> 16) % 3), 0); if (human(o, nm, &L)) { fclose(f); fclose(o); return 2; } } made++; }
        }
    }
    fclose(f); fclose(o); printf("FROMCAST models=%d\n", made); return 0;
}
/* ---- fromchars (D30 step 2): CHAR rows with a seed and optional knobs, STYLE presets ----
 *   STYLE | name | knob=value knob=value ...        a preset (a patch)
 *   CHAR  | id | seed | knob=value ...               knobs, in order of strength: explicit here, then preset=<STYLE name>, then filled from the seed
 * knobs: preset hair_style(short long mohawk masked clown none) skin hair gar trim (#rrggbb) build(0..2) legs(3..7) head(0..2) mark(0|1) equip(id,id,...: one item per slot, ids from the EQ rows)
 * The same seed and knobs always give the same character: every unset knob comes from hash32(seed, knob number). */
typedef struct { char name[40], kv[300]; } StyleRow;
static StyleRow STY[64]; static int nsty;
enum { K_SKIN, K_HAIR, K_HSTYLE, K_GAR, K_BUILD, K_LEGS, K_HEAD, K_TRIM, K_MARK, K_EQUIP, K_N };
static int valid_colour(const char *c) { int i; if (c[0] != '#' || strlen(c) != 7) return 0; for (i = 1; i < 7; i++) if (hexv(c[i]) < 0) return 0; return 1; }
static int num_in(const char *s, int lo, int hi, int *out) { char *e; long v = strtol(s, &e, 10); if (*s == 0 || *e || v < lo || v > hi) return 0; *out = (int)v; return 1; }
/* applies "k=v k=v" to the knob slots that are still unset; returns 2 on a bad token */
static int apply_kv(const char *kv, char *vals[K_N], char store[K_N][160], int set[K_N], char *preset, int presetok) {
    char b[300], *t, *sv = NULL;
    snprintf(b, sizeof b, "%s", kv);
    for (t = strtok_r(b, " ", &sv); t; t = strtok_r(NULL, " ", &sv)) {
        char *eq = strchr(t, '='), *k, *v; int slot = -1, n;
        if (!eq) { fprintf(stderr, "bad knob (need key=value): %s\n", t); return 2; }
        *eq = 0; k = t; v = eq + 1;
        if (!strcmp(k, "preset")) { if (!presetok) { fprintf(stderr, "preset inside a preset is not allowed\n"); return 2; } if (!preset[0]) snprintf(preset, 40, "%s", v); continue; }
        if (!strcmp(k, "skin")) slot = K_SKIN; else if (!strcmp(k, "hair")) slot = K_HAIR; else if (!strcmp(k, "gar")) slot = K_GAR; else if (!strcmp(k, "trim")) slot = K_TRIM;
        else if (!strcmp(k, "hair_style")) slot = K_HSTYLE; else if (!strcmp(k, "build")) slot = K_BUILD; else if (!strcmp(k, "legs")) slot = K_LEGS; else if (!strcmp(k, "head")) slot = K_HEAD; else if (!strcmp(k, "mark")) slot = K_MARK; else if (!strcmp(k, "equip")) slot = K_EQUIP; else if (!strcmp(k, "equip")) slot = K_EQUIP;
        else { fprintf(stderr, "unknown knob: %s\n", k); return 2; }
        if (slot == K_SKIN || slot == K_HAIR || slot == K_GAR || slot == K_TRIM) { if (!valid_colour(v)) { fprintf(stderr, "knob %s needs #rrggbb, got %s\n", k, v); return 2; } }
        else if (slot == K_EQUIP) { if (strlen(v) > 150) { fprintf(stderr, "equip list too long\n"); return 2; } }
        else if (slot == K_HSTYLE) { if (strcmp(v, "short") && strcmp(v, "long") && strcmp(v, "mohawk") && strcmp(v, "masked") && strcmp(v, "clown") && strcmp(v, "none")) { fprintf(stderr, "unknown hair_style: %s\n", v); return 2; } }
        else { int lo = 0, hi = 2; if (slot == K_LEGS) { lo = 3; hi = 7; } if (slot == K_MARK) hi = 1; if (!num_in(v, lo, hi, &n)) { fprintf(stderr, "knob %s must be %d..%d, got %s\n", k, lo, hi, v); return 2; } }
        if (!set[slot]) { snprintf(store[slot], 160, "%s", v); vals[slot] = store[slot]; set[slot] = 1; }
    }
    return 0;
}
static unsigned kh(unsigned seed, int k) { return hash32(seed * 2654435761u + (unsigned)k * 977u + 13u); }
static int equip_ok(const char *list, const char *who) { /* every id exists in EQ, one item per slot */
    char b[160], *t, *sv = NULL, used[5][16]; int n = 0, i, j;
    snprintf(b, sizeof b, "%s", list);
    for (t = strtok_r(b, ",", &sv); t; t = strtok_r(NULL, ",", &sv)) {
        const char *slot = NULL;
        for (i = 0; i < neq; i++) if (!strcmp(EQ[i].id, t)) { slot = EQ[i].slot; break; }
        if (!slot) { fprintf(stderr, "%s: unknown equipment id: %s\n", who, t); return 2; }
        for (j = 0; j < n; j++) if (!strcmp(used[j], slot)) { fprintf(stderr, "%s: two items for the %s slot (%s)\n", who, slot, t); return 2; }
        if (n >= 5) return 2;
        snprintf(used[n++], 16, "%s", slot);
    }
    return 0;
}
/* check-equip <partsets.pdl> <db data dir>: every EQ item that names a database item must exist in armors.pdl or weapons.pdl (the stats' only home) */
static int db_has(const char *dir, const char *file, const char *name) {
    char path[1000], line[400]; FILE *f; int found = 0;
    snprintf(path, sizeof path, "%s/%s", dir, file); f = fopen(path, "r"); if (!f) return 0;
    while (fgets(line, sizeof line, f)) { char *p = line, *k, *key, *val; k = fld(&p); (void)k; key = fld(&p); val = fld(&p); if (!strcmp(key, "name") && !strcmp(val, name)) found = 1; }
    fclose(f); return found;
}
static int check_equip(const char *psf, const char *dbdir) {
    int i, j, ids = 0, linked = 0, none = 0, missing = 0;
    if (load_partsets(psf)) return 2;
    for (i = 0; i < neq; i++) {
        int first = 1; for (j = 0; j < i; j++) if (!strcmp(EQ[j].id, EQ[i].id)) first = 0;
        if (!first) continue;
        ids++;
        if (!strcmp(EQ[i].item, "-")) { none++; continue; }
        if (db_has(dbdir, "armors.pdl", EQ[i].item) || db_has(dbdir, "weapons.pdl", EQ[i].item)) linked++; else { missing++; fprintf(stderr, "EQ %s names the database item \"%s\" which does not exist\n", EQ[i].id, EQ[i].item); }
    }
    printf("EQCHECK items=%d linked=%d no_db_row=%d missing=%d\n", ids, linked, none, missing);
    return missing ? 1 : 0;
}
static int fromchars(const char *charf, const char *outf, const char *psf) {
    FILE *f = fopen(charf, "r"), *o; char line[900]; int made = 0, i;
    static const char *skins[5] = { "#e6bfa0", "#d6a47c", "#c08a60", "#8a5a3a", "#5a3a24" }, *hairs[6] = { "#1a1410", "#4a3020", "#8a5a30", "#c8a050", "#5a5a66", "#c04a8a" }, *gars[8] = { "#8a4a2e", "#2a4a7a", "#3a6a3a", "#6a3a7a", "#7a7a86", "#a08a3a", "#2a2a3a", "#a03a3a" }, *hst[4] = { "short", "long", "mohawk", "none" };
    if (!f) { fprintf(stderr, "cannot open %s\n", charf); return 2; }
    if (load_partsets(psf)) { fclose(f); return 2; }
    o = fopen(outf, "w"); if (!o) { fclose(f); return 2; }
    fprintf(o, "# GENERATED by generator_voxel_op fromchars from the characters file and partsets.pdl. Do not edit: edit those.\n");
    while (fgets(line, sizeof line, f)) { /* pass 1: presets */
        char *p = line, *k;
        if (line[0] == '#' || line[0] == '\n') continue;
        k = fld(&p);
        if (!strcmp(k, "STYLE")) { if (nsty >= 64) { fprintf(stderr, "too many STYLE rows\n"); fclose(f); fclose(o); return 2; } snprintf(STY[nsty].name, 40, "%s", fld(&p)); snprintf(STY[nsty].kv, 300, "%s", fld(&p)); nsty++; }
    }
    rewind(f);
    while (fgets(line, sizeof line, f)) {
        char *p = line, *k, id[40], *vals[K_N] = { 0 }, store[K_N][160], preset[40] = "", kvs[300]; int set[K_N] = { 0 }, seed;
        Look L;
        if (line[0] == '#' || line[0] == '\n') continue;
        k = fld(&p);
        if (strcmp(k, "CHAR")) continue;
        snprintf(id, sizeof id, "%s", fld(&p));
        { char *sd = fld(&p); if (!id[0] || !num_in(sd, 0, 2000000000, &seed)) { fprintf(stderr, "bad CHAR row (id | seed | knobs): %s\n", line); fclose(f); fclose(o); return 2; } }
        snprintf(kvs, sizeof kvs, "%s", fld(&p));
        if (apply_kv(kvs, vals, store, set, preset, 1)) { fclose(f); fclose(o); return 2; }
        if (preset[0]) {
            int found = 0;
            for (i = 0; i < nsty; i++) if (!strcmp(STY[i].name, preset)) { found = 1; if (apply_kv(STY[i].kv, vals, store, set, preset, 0)) { fclose(f); fclose(o); return 2; } }
            if (!found) { fprintf(stderr, "unknown preset: %s\n", preset); fclose(f); fclose(o); return 2; }
        }
        for (i = 0; i < K_N; i++) if (!set[i]) { /* the seed fills what is left */
            unsigned h = kh((unsigned)seed, i);
            switch (i) {
            case K_SKIN: snprintf(store[i], 160, "%s", skins[h % 5]); break; case K_HAIR: snprintf(store[i], 160, "%s", hairs[h % 6]); break;
            case K_HSTYLE: snprintf(store[i], 160, "%s", hst[h % 4]); break; case K_GAR: snprintf(store[i], 160, "%s", gars[h % 8]); break;
            case K_BUILD: snprintf(store[i], 160, "%u", h % 3); break; case K_LEGS: snprintf(store[i], 160, "%u", 3 + h % 5); break;
            case K_HEAD: snprintf(store[i], 160, "%u", h % 3); break; case K_TRIM: snprintf(store[i], 160, "#ffd24a"); break; case K_EQUIP: store[i][0] = 0; break; default: snprintf(store[i], 160, "0"); break;
            }
            vals[i] = store[i];
        }
        lookof(&L, vals[K_SKIN], vals[K_HSTYLE], vals[K_HAIR], vals[K_GAR], vals[K_TRIM], atoi(vals[K_BUILD]), atoi(vals[K_MARK]));
        if (equip_ok(vals[K_EQUIP], id)) { fclose(f); fclose(o); return 2; }
        snprintf(L.equip, sizeof L.equip, "%s", vals[K_EQUIP]);
        L.lh = atoi(vals[K_LEGS]); L.hw = 6 + 2 * atoi(vals[K_HEAD]);
        if (human(o, id, &L)) { fclose(f); fclose(o); return 2; }
        made++;
    }
    fclose(f); fclose(o); printf("FROMCHARS models=%d presets=%d\n", made, nsty); return made ? 0 : (fprintf(stderr, "no CHAR rows\n"), 2);
}
static void cellcopy(unsigned char *dst, int dw, int dx, int dy, const unsigned char *strip, int v, int mirror) {
    int x, y;
    for (y = 0; y < 48; y++) for (x = 0; x < 48; x++) memcpy(dst + ((dy + y) * dw + dx + x) * 4, strip + (y * 192 + v * 48 + (mirror ? 47 - x : x)) * 4, 4);
}
static int walk_into(unsigned char *dst, int dw, int ox, int oy, const char *model, unsigned char *face) {
    static unsigned char st[3][192 * 48 * 4]; const char *pn[3] = { "w1", "idle", "w2" }; int i, c, rc = 0;
    for (i = 0; i < 3; i++) { memset(st[i], 0, sizeof st[i]); rc = render_row(st[i], 0, 192, model, pn[i], &c); if (rc) return rc; }
    for (i = 0; i < 3; i++) { cellcopy(dst, dw, ox + i * 48, oy, st[i], 0, 0); cellcopy(dst, dw, ox + i * 48, oy + 48, st[i], 1, 0); cellcopy(dst, dw, ox + i * 48, oy + 96, st[i], 1, 1); cellcopy(dst, dw, ox + i * 48, oy + 144, st[i], 3, 0); }
    if (face) { int x, y; memset(face, 0, 144 * 144 * 4);
        for (y = 0; y < 144; y++) for (x = 0; x < 128; x++) memcpy(face + (y * 144 + 8 + x) * 4, st[1] + ((y / 4) * 192 + 8 + x / 4) * 4, 4); }
    return 0;
}
static int cast_sheets(const char *slotf, const char *outroot) {
    FILE *f = fopen(slotf, "r"); char line[900]; int rows = 0, rc = 0;
    unsigned char *cs = malloc((size_t)576 * 384 * 4), *fs = malloc((size_t)576 * 288 * 4), *face = malloc(144 * 144 * 4);
    if (!f || !cs || !fs || !face) { fprintf(stderr, "no slots file\n"); return 2; }
    while (fgets(line, sizeof line, f)) {
        char *p, *name, *ids, *tok, dir[900], path[1000]; int j = 0, y;
        if (strncmp(line, "SLOT |", 6) != 0) continue;
        p = line + 6; name = fld(&p); ids = fld(&p);
        memset(cs, 0, (size_t)576 * 384 * 4); memset(fs, 0, (size_t)576 * 288 * 4);
        for (tok = strtok(ids, ","); tok; tok = strtok(NULL, ",")) {
            if (!has_model(tok) || j >= 8) { fprintf(stderr, "SLOT %s: unknown model or more than 8: %s\n", name, tok); rc = 2; break; }
            if (walk_into(cs, 576, (j % 4) * 144, (j / 4) * 192, tok, face)) { fprintf(stderr, "model %s has no voxels\n", tok); rc = 2; break; }
            for (y = 0; y < 144; y++) memcpy(fs + (((j / 4) * 144 + y) * 576 + (j % 4) * 144) * 4, face + y * 144 * 4, 144 * 4);
            j++;
        }
        if (rc) break;
        snprintf(dir, sizeof dir, "%.600s/characters", outroot); { char cmd[1000]; snprintf(cmd, sizeof cmd, "mkdir -p '%s'", dir); if (system(cmd)) { rc = 2; break; } }
        snprintf(path, sizeof path, "%.600s/%.40s.png", dir, name); if (!stbi_write_png(path, 576, 384, 4, cs, 576 * 4)) { rc = 2; break; }
        snprintf(dir, sizeof dir, "%.600s/faces", outroot); { char cmd[1000]; snprintf(cmd, sizeof cmd, "mkdir -p '%s'", dir); if (system(cmd)) { rc = 2; break; } }
        snprintf(path, sizeof path, "%.600s/%.40s.png", dir, name); if (!stbi_write_png(path, 576, 288, 4, fs, 576 * 4)) { rc = 2; break; }
        rows++;
    }
    fclose(f); free(cs); free(fs); free(face);
    if (!rc && !rows) { fprintf(stderr, "no SLOT rows\n"); rc = 2; }
    if (!rc) printf("VOXELSLOTS sheets=%d\n", rows);
    return rc;
}
static int icons(const char *kind, int cols, const char *out) {
    int i, n = 0, rows; unsigned char *o;
    for (i = 0; i < nm; i++) if (!strcmp(M[i].kind, kind)) n++;
    if (!n || cols < 1) { fprintf(stderr, "no models of kind %s\n", kind); return 2; }
    rows = (n + cols - 1) / cols; o = calloc((size_t)cols * 48 * rows * 48 * 4, 1); if (!o) return 2;
    n = 0;
    for (i = 0; i < nm; i++) { static unsigned char st[192 * 48 * 4]; int c; if (strcmp(M[i].kind, kind)) continue; memset(st, 0, sizeof st); if (render_row(st, 0, 192, M[i].id, "idle", &c)) continue; cellcopy(o, cols * 48, (n % cols) * 48, (n / cols) * 48, st, 0, 0); n++; }
    if (!stbi_write_png(out, cols * 48, rows * 48, 4, o, cols * 48 * 4)) { free(o); return 2; }
    printf("VOXELICONS kind=%s models=%d size=%dx%d\n", kind, n, cols * 48, rows * 48); free(o); return 0;
}
int main(int argc, char **argv) {
    int rc;
    if (argc < 3) { fprintf(stderr, "usage: render | sheet | check | fromcast <cast.pdl> <out.pdl> <partsets.pdl> | fromchars <characters.pdl> <out.pdl> <partsets.pdl> | check-equip <partsets.pdl> <db data dir> | walksheet <recipes> <model> <out.png> | cast-sheets <recipes> <slots.pdl> <outroot> | icons <recipes> <kind> <cols> <out.png>\n"); return 2; }
    if (!strcmp(argv[1], "fromcast") && argc == 5) return fromcast(argv[2], argv[3], argv[4]);
    if (!strcmp(argv[1], "fromchars") && argc == 5) return fromchars(argv[2], argv[3], argv[4]);
    if (!strcmp(argv[1], "check-equip") && argc == 4) return check_equip(argv[2], argv[3]);
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
    if (!strcmp(argv[1], "walksheet") && argc == 5) { static unsigned char w[144 * 192 * 4]; if (!has_model(argv[3])) { fprintf(stderr, "unknown model %s\n", argv[3]); return 2; } memset(w, 0, sizeof w); if (walk_into(w, 144, 0, 0, argv[3], NULL)) return 3; if (!stbi_write_png(argv[4], 144, 192, 4, w, 144 * 4)) return 2; printf("VOXELWALK model=%s size=144x192 frames=12\n", argv[3]); return 0; }
    if (!strcmp(argv[1], "cast-sheets") && argc == 5) return cast_sheets(argv[3], argv[4]);
    if (!strcmp(argv[1], "icons") && argc == 6) return icons(argv[3], atoi(argv[4]), argv[5]);
    fprintf(stderr, "bad arguments\n"); return 2;
}
