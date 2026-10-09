/* pet_gen.c - generator for a small animated pet (tamagotchi-style), seeded, no downloads.
 *
 *   pet_gen <out_dir> [seed] [pet.pdl]
 *
 * Writes into <out_dir>:
 *   obj/<anim>_<NN>.obj + pet.mtl   one OBJ per animation frame (OBJ has no animation, so an animation is a numbered
 *                                   sequence of poses; play them in order, then loop). Anims: idle, eat, hungry.
 *   sprites/<anim>_<NN>.rgba        16x24 RGBA8 front view of the same pose (the house's frame format, alpha 0 = empty)
 *   sprites_csv/<anim>_<NN>/sprite.csv  the same frame as a 24x24 sprite.csv (the format an <item sprite="DIR"> in a layout loads)
 *   sheet.png                       every frame side by side, 4x scaled, for a quick look
 *   pet.pdl                         what was generated (seed, colours, frame counts), readable by a layout manager
 *
 * The pet is built from low-poly ellipsoids (body, head, two ears, two eyes, tail). The seed picks body colour, ear
 * shape and size; an optional pet.pdl can pin any of them:  KEY | body_r | 220   (0-255, also body_g, body_b, ear_len,
 * ear_w, scale). No FBX: FBX is a binary container, OBJ frames are plain text and every tool reads them. An ASCII-FBX
 * exporter can be added on top of the same mesh list later.
 *
 * Build: gcc -std=c11 -O2 -Wall -o ops/+x/pet_gen.+x ops/pet_gen.c -lm   (see build_pet_gen.sh)
 */
#define _DEFAULT_SOURCE
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../../&.widgits/_shared-lib/stb_image_write.h"

#define SEG 12
#define RING 8
#define MAXV 4096
#define MAXF 8192
#define PARTS 7
#define SW 16
#define SH 24
#define RS 8 /* render supersample */

typedef struct { float x, y, z; } V3;
typedef struct { float cx, cy, cz, rx, ry, rz; int mat; } Part;   /* an ellipsoid */
typedef struct { V3 v[MAXV]; int f[MAXF][3]; int mat[MAXF]; int nv, nf; } Mesh;

static unsigned rng_s = 1;
static float rnd(void) { rng_s = rng_s * 1664525u + 1013904223u; return (float)(rng_s >> 8) / 16777216.0f; }

static int g_body[3], g_belly[3] = { 245, 235, 215 }, g_dark[3] = { 40, 30, 40 }, g_cheek[3] = { 255, 140, 150 };
static float g_ear_len = 0.5f, g_ear_w = 0.22f, g_scale = 1.0f;

static void add_ellipsoid(Mesh *m, const Part *p) {
    int base = m->nv;
    for (int r = 0; r <= RING; r++) {
        float th = 3.14159265f * (float)r / RING;
        for (int s = 0; s < SEG; s++) {
            float ph = 6.2831853f * (float)s / SEG;
            if (m->nv >= MAXV) return;
            m->v[m->nv++] = (V3){ p->cx + p->rx * sinf(th) * cosf(ph), p->cy + p->ry * cosf(th), p->cz + p->rz * sinf(th) * sinf(ph) };
        }
    }
    for (int r = 0; r < RING; r++)
        for (int s = 0; s < SEG; s++) {
            int a = base + r * SEG + s, b = base + r * SEG + (s + 1) % SEG, c = a + SEG, d = b + SEG;
            if (m->nf + 2 > MAXF) return;
            m->f[m->nf][0] = a; m->f[m->nf][1] = c; m->f[m->nf][2] = b; m->mat[m->nf++] = p->mat;
            m->f[m->nf][0] = b; m->f[m->nf][1] = c; m->f[m->nf][2] = d; m->mat[m->nf++] = p->mat;
        }
}

/* pose: sq = body squash (1 = rest), bob = head offset up, tilt = head forward (z), ear = ear flap, blink = eyes closed 0..1 */
static void build(Mesh *m, float sq, float bob, float tilt, float ear, float blink) {
    memset(m, 0, sizeof *m);
    float s = g_scale, st = 1.0f / sq;                       /* stretch sideways when squashed: volume stays about the same */
    Part body = { 0, 0.55f * s * sq, 0, 0.55f * s * sqrtf(st), 0.5f * s * sq, 0.5f * s * sqrtf(st), 0 };
    Part belly = { 0, 0.5f * s * sq, 0.28f * s, 0.34f * s, 0.32f * s * sq, 0.28f * s, 1 };
    float hy = 1.2f * s * sq + bob * s;
    Part head = { 0, hy, tilt * s, 0.42f * s, 0.38f * s, 0.4f * s, 0 };
    Part earL = { -0.3f * s, hy + (0.38f + g_ear_len * 0.4f) * s + ear * 0.05f * s, tilt * s, g_ear_w * s, g_ear_len * 0.5f * s, 0.1f * s, 0 };
    Part earR = earL; earR.cx = 0.3f * s; earR.cy = hy + (0.38f + g_ear_len * 0.4f) * s - ear * 0.05f * s;
    float ey = 0.12f * s * (1.0f - blink * 0.85f);
    Part eyeL = { -0.15f * s, hy + 0.06f * s, tilt * s + 0.36f * s, 0.07f * s, ey, 0.05f * s, 2 };
    Part eyeR = eyeL; eyeR.cx = 0.15f * s;
    Part tail = { 0.0f, 0.4f * s * sq, -0.55f * s, 0.12f * s, 0.12f * s, 0.18f * s, 0 };
    Part cheekL = { -0.27f * s, hy - 0.1f * s, tilt * s + 0.3f * s, 0.07f * s, 0.05f * s, 0.04f * s, 3 };
    Part cheekR = cheekL; cheekR.cx = 0.27f * s;
    Part parts[PARTS + 3] = { body, belly, head, earL, earR, eyeL, eyeR, tail, cheekL, cheekR };
    for (int i = 0; i < PARTS + 3; i++) add_ellipsoid(m, &parts[i]);
}

static void mat_rgb(int mat, int out[3]) {
    const int *src = mat == 0 ? g_body : mat == 1 ? g_belly : mat == 2 ? g_dark : g_cheek;
    for (int i = 0; i < 3; i++) out[i] = src[i];
}

static int write_obj(const Mesh *m, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "# pet_gen frame\nmtllib pet.mtl\n");
    for (int i = 0; i < m->nv; i++) fprintf(f, "v %.4f %.4f %.4f\n", m->v[i].x, m->v[i].y, m->v[i].z);
    int cur = -1;
    for (int i = 0; i < m->nf; i++) {
        if (m->mat[i] != cur) { cur = m->mat[i]; fprintf(f, "usemtl m%d\n", cur); }
        fprintf(f, "f %d %d %d\n", m->f[i][0] + 1, m->f[i][1] + 1, m->f[i][2] + 1);
    }
    fclose(f);
    return 0;
}

/* orthographic front view, z-buffer, flat shading. Output SWxSH RGBA, supersampled RS x then box filtered. */
static void render(const Mesh *m, unsigned char *out /* SW*SH*4 */) {
    int W = SW * RS, H = SH * RS;
    float *zb = malloc(sizeof(float) * (size_t)W * H);
    unsigned char *px = calloc((size_t)W * H, 4);
    for (int i = 0; i < W * H; i++) zb[i] = -1e9f;
    float top = 2.05f * g_scale, unit = (float)H / top;          /* world y 0..top fills the frame height */
    float lx = -0.4f, ly = 0.6f, lz = 0.7f, ll = sqrtf(lx * lx + ly * ly + lz * lz);
    lx /= ll; ly /= ll; lz /= ll;
    for (int t = 0; t < m->nf; t++) {
        V3 a = m->v[m->f[t][0]], b = m->v[m->f[t][1]], c = m->v[m->f[t][2]];
        V3 e1 = { b.x - a.x, b.y - a.y, b.z - a.z }, e2 = { c.x - a.x, c.y - a.y, c.z - a.z };
        V3 n = { e1.y * e2.z - e1.z * e2.y, e1.z * e2.x - e1.x * e2.z, e1.x * e2.y - e1.y * e2.x };
        float nl = sqrtf(n.x * n.x + n.y * n.y + n.z * n.z);
        if (nl < 1e-9f) continue;
        n.x /= nl; n.y /= nl; n.z /= nl;
        if (n.z < 0) continue;                                    /* back face (camera looks down -z from +z) */
        float shade = 0.45f + 0.55f * fmaxf(0.0f, n.x * lx + n.y * ly + n.z * lz);
        int col[3]; mat_rgb(m->mat[t], col);
        float X[3], Y[3], Z[3]; V3 P[3] = { a, b, c };
        for (int k = 0; k < 3; k++) { X[k] = (float)W * 0.5f + P[k].x * unit; Y[k] = (float)H - 1 - P[k].y * unit; Z[k] = P[k].z; }
        int x0 = (int)floorf(fminf(X[0], fminf(X[1], X[2]))), x1 = (int)ceilf(fmaxf(X[0], fmaxf(X[1], X[2])));
        int y0 = (int)floorf(fminf(Y[0], fminf(Y[1], Y[2]))), y1 = (int)ceilf(fmaxf(Y[0], fmaxf(Y[1], Y[2])));
        if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 >= W) x1 = W - 1; if (y1 >= H) y1 = H - 1;
        float den = (Y[1] - Y[2]) * (X[0] - X[2]) + (X[2] - X[1]) * (Y[0] - Y[2]);
        if (fabsf(den) < 1e-9f) continue;
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++) {
                float w0 = ((Y[1] - Y[2]) * ((float)x + 0.5f - X[2]) + (X[2] - X[1]) * ((float)y + 0.5f - Y[2])) / den;
                float w1 = ((Y[2] - Y[0]) * ((float)x + 0.5f - X[2]) + (X[0] - X[2]) * ((float)y + 0.5f - Y[2])) / den;
                float w2 = 1.0f - w0 - w1;
                if (w0 < 0 || w1 < 0 || w2 < 0) continue;
                float z = w0 * Z[0] + w1 * Z[1] + w2 * Z[2];
                int i = y * W + x;
                if (z <= zb[i]) continue;
                zb[i] = z;
                px[i * 4] = (unsigned char)fminf(255.0f, (float)col[0] * shade);
                px[i * 4 + 1] = (unsigned char)fminf(255.0f, (float)col[1] * shade);
                px[i * 4 + 2] = (unsigned char)fminf(255.0f, (float)col[2] * shade);
                px[i * 4 + 3] = 255;
            }
    }
    for (int y = 0; y < SH; y++)
        for (int x = 0; x < SW; x++) {
            int sr = 0, sg = 0, sb = 0, sa = 0;
            for (int dy = 0; dy < RS; dy++)
                for (int dx = 0; dx < RS; dx++) {
                    const unsigned char *p = px + (((y * RS + dy) * W) + x * RS + dx) * 4;
                    if (p[3]) { sr += p[0]; sg += p[1]; sb += p[2]; sa++; }
                }
            unsigned char *o = out + (y * SW + x) * 4;
            if (sa * 2 >= RS * RS) { o[0] = (unsigned char)(sr / sa); o[1] = (unsigned char)(sg / sa); o[2] = (unsigned char)(sb / sa); o[3] = 255; }
            else { o[0] = o[1] = o[2] = o[3] = 0; }
        }
    free(zb); free(px);
}

static int pdl_int(const char *path, const char *key, int def) {
    FILE *f = fopen(path, "r"); if (!f) return def;
    char line[256]; int v = def;
    while (fgets(line, sizeof line, f)) {
        char *b1 = strchr(line, '|'); if (!b1) continue;
        char *b2 = strchr(b1 + 1, '|'); if (!b2) continue;
        char name[64]; int n = 0; char *p = b1 + 1;
        while (*p == ' ') p++;
        while (p < b2 && *p != ' ' && n < 63) name[n++] = *p++;
        name[n] = 0;
        if (strcmp(name, key) == 0) { v = atoi(b2 + 1); break; }
    }
    fclose(f);
    return v;
}

typedef struct { const char *name; int n; } Anim;

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: pet_gen <out_dir> [seed] [pet.pdl]\n"); return 2; }
    const char *out = argv[1];
    unsigned seed = argc > 2 ? (unsigned)strtoul(argv[2], NULL, 10) : 1;
    const char *pdl = argc > 3 ? argv[3] : "";
    rng_s = seed * 2654435761u + 12345u;
    g_body[0] = 120 + (int)(rnd() * 120); g_body[1] = 120 + (int)(rnd() * 120); g_body[2] = 120 + (int)(rnd() * 120);
    g_ear_len = 0.35f + rnd() * 0.5f; g_ear_w = 0.16f + rnd() * 0.14f; g_scale = 0.9f + rnd() * 0.2f;
    g_body[0] = pdl_int(pdl, "body_r", g_body[0]); g_body[1] = pdl_int(pdl, "body_g", g_body[1]); g_body[2] = pdl_int(pdl, "body_b", g_body[2]);
    g_ear_len = (float)pdl_int(pdl, "ear_len", (int)(g_ear_len * 100)) / 100.0f;
    g_ear_w = (float)pdl_int(pdl, "ear_w", (int)(g_ear_w * 100)) / 100.0f;
    g_scale = (float)pdl_int(pdl, "scale", (int)(g_scale * 100)) / 100.0f;

    char p[1024];
    snprintf(p, sizeof p, "%s", out); mkdir(p, 0755);
    snprintf(p, sizeof p, "%s/obj", out); mkdir(p, 0755);
    snprintf(p, sizeof p, "%s/sprites", out); mkdir(p, 0755);
    snprintf(p, sizeof p, "%s/sprites_csv", out); mkdir(p, 0755);

    snprintf(p, sizeof p, "%s/obj/pet.mtl", out);
    FILE *mf = fopen(p, "w");
    if (!mf) { fprintf(stderr, "pet_gen: cannot write %s\n", p); return 1; }
    for (int i = 0; i < 4; i++) { int c[3]; mat_rgb(i, c); fprintf(mf, "newmtl m%d\nKd %.3f %.3f %.3f\n", i, c[0] / 255.0, c[1] / 255.0, c[2] / 255.0); }
    fclose(mf);

    Anim anims[3] = { { "idle", 8 }, { "eat", 8 }, { "hungry", 8 } };
    int total = 24;
    unsigned char *sheet = calloc((size_t)(SW * 8) * (SH * 3) * 4, 1);
    Mesh *m = malloc(sizeof *m);
    for (int a = 0; a < 3; a++)
        for (int fi = 0; fi < anims[a].n; fi++) {
            float t = 6.2831853f * (float)fi / anims[a].n;
            float sq = 1, bob = 0, tilt = 0, ear = 0, blink = 0;
            if (a == 0) { sq = 1.0f - 0.04f * sinf(t); bob = 0.02f * sinf(t); ear = sinf(t); blink = fi == 5 ? 1.0f : 0.0f; }
            else if (a == 1) { sq = 1.0f - 0.03f * sinf(t * 2); bob = -0.12f * (0.5f - 0.5f * cosf(t * 2)); tilt = 0.1f * (0.5f - 0.5f * cosf(t * 2)); ear = sinf(t * 2); }
            else { sq = 0.88f + 0.02f * sinf(t); bob = -0.1f; tilt = 0.05f; ear = -1.5f; blink = 0.5f; }
            build(m, sq, bob, tilt, ear, blink);
            snprintf(p, sizeof p, "%s/obj/%s_%02d.obj", out, anims[a].name, fi);
            if (write_obj(m, p) != 0) { fprintf(stderr, "pet_gen: cannot write %s\n", p); return 1; }
            unsigned char fr[SW * SH * 4];
            render(m, fr);
            snprintf(p, sizeof p, "%s/sprites/%s_%02d.rgba", out, anims[a].name, fi);
            FILE *rf = fopen(p, "wb");
            if (!rf) { fprintf(stderr, "pet_gen: cannot write %s\n", p); return 1; }
            fwrite(fr, 1, sizeof fr, rf); fclose(rf);
            /* 24x24 sprite.csv: the 16x24 frame centred (4 empty columns each side), row-major r,g,b,a */
            snprintf(p, sizeof p, "%s/sprites_csv/%s_%02d", out, anims[a].name, fi); mkdir(p, 0755);
            snprintf(p, sizeof p, "%s/sprites_csv/%s_%02d/sprite.csv", out, anims[a].name, fi);
            FILE *cf = fopen(p, "w");
            if (!cf) { fprintf(stderr, "pet_gen: cannot write %s\n", p); return 1; }
            fprintf(cf, "# resolution=24\n# scale=1.0\n# transform=0,0,0\nr,g,b,a\n");
            for (int y = 0; y < 24; y++)
                for (int x = 0; x < 24; x++) {
                    int sx = x - 4;
                    const unsigned char *q = (sx >= 0 && sx < SW) ? fr + (y * SW + sx) * 4 : NULL;
                    if (q && q[3]) fprintf(cf, "%d,%d,%d,%d\n", q[0], q[1], q[2], 255); else fprintf(cf, "0,0,0,0\n");
                }
            fclose(cf);
            for (int y = 0; y < SH; y++) memcpy(sheet + (((a * SH) + y) * (SW * 8) + fi * SW) * 4, fr + y * SW * 4, SW * 4);
        }
    /* 4x scaled sheet, grey background so the alpha is visible */
    int sw = SW * 8 * 4, sh = SH * 3 * 4;
    unsigned char *big = malloc((size_t)sw * sh * 3);
    for (int y = 0; y < sh; y++)
        for (int x = 0; x < sw; x++) {
            const unsigned char *s = sheet + (((y / 4) * (SW * 8)) + x / 4) * 4;
            unsigned char *d = big + ((size_t)y * sw + x) * 3;
            int bg = ((x / 16 + y / 16) & 1) ? 90 : 70;
            d[0] = (unsigned char)(s[3] ? s[0] : bg); d[1] = (unsigned char)(s[3] ? s[1] : bg); d[2] = (unsigned char)(s[3] ? s[2] : bg);
        }
    snprintf(p, sizeof p, "%s/sheet.png", out);
    stbi_write_png(p, sw, sh, 3, big, sw * 3);
    snprintf(p, sizeof p, "%s/pet.pdl", out);
    FILE *pf = fopen(p, "w");
    if (pf) {
        fprintf(pf, "SECTION | KEY | VALUE\n---------------------------------\nPET | seed | %u\nPET | body_r | %d\nPET | body_g | %d\nPET | body_b | %d\n", seed, g_body[0], g_body[1], g_body[2]);
        fprintf(pf, "PET | ear_len | %d\nPET | ear_w | %d\nPET | scale | %d\n", (int)(g_ear_len * 100), (int)(g_ear_w * 100), (int)(g_scale * 100));
        for (int a = 0; a < 3; a++) fprintf(pf, "ANIM | %s | %d\n", anims[a].name, anims[a].n);
        fclose(pf);
    }
    printf("pet_gen: seed %u -> %d frames (obj + 16x24 rgba) + sheet.png in %s\n", seed, total, out);
    free(m); free(sheet); free(big);
    return 0;
}
