/* gen.c — procedural noise + GPU textures (AAA generative look) */
#include "sw.h"
#include <GL/glew.h>
#include <GL/gl.h>

static int g_perm[512];

void gen_init(unsigned seed) {
    int i, j, t;
    srand(seed);
    for (i = 0; i < 256; i++) g_perm[i] = i;
    for (i = 255; i > 0; i--) {
        j = rand() % (i + 1);
        t = g_perm[i]; g_perm[i] = g_perm[j]; g_perm[j] = t;
    }
    for (i = 0; i < 256; i++) g_perm[256 + i] = g_perm[i];
}

static float fade(float t) { return t * t * t * (t * (t * 6.f - 15.f) + 10.f); }
static float grad(int h, float x, float y) {
    int hh = h & 3;
    float u = hh < 2 ? x : y;
    float v = hh < 2 ? y : x;
    return ((hh & 1) ? -u : u) + ((hh & 2) ? -2.f * v : 2.f * v);
}

float gen_noise2(float x, float y) {
    int X = ((int)floorf(x)) & 255;
    int Y = ((int)floorf(y)) & 255;
    float xf = x - floorf(x);
    float yf = y - floorf(y);
    float u = fade(xf), v = fade(yf);
    int aa = g_perm[g_perm[X] + Y];
    int ab = g_perm[g_perm[X] + Y + 1];
    int ba = g_perm[g_perm[X + 1] + Y];
    int bb = g_perm[g_perm[X + 1] + Y + 1];
    float x1 = lerpf(grad(aa, xf, yf), grad(ba, xf - 1, yf), u);
    float x2 = lerpf(grad(ab, xf, yf - 1), grad(bb, xf - 1, yf - 1), u);
    return lerpf(x1, x2, v) * 0.5f + 0.5f;
}

float gen_fbm2(float x, float y, int oct) {
    float a = 0.5f, f = 1.f, s = 0.f, m = 0.f;
    int i;
    for (i = 0; i < oct; i++) {
        s += a * gen_noise2(x * f, y * f);
        m += a;
        a *= 0.5f;
        f *= 2.05f;
    }
    return s / m;
}

float gen_height(enum Planet p, float x, float z) {
    float n, h;
    switch (p) {
    case PLANET_ENDOR:
        n = gen_fbm2(x * 0.03f, z * 0.03f, 5);
        h = n * 18.f + gen_noise2(x * 0.2f, z * 0.2f) * 2.f;
        /* clearings */
        if (gen_noise2(x * 0.01f + 9, z * 0.01f) > 0.72f) h *= 0.55f;
        return h;
    case PLANET_TATOOINE:
        n = gen_fbm2(x * 0.02f, z * 0.02f, 4);
        h = n * 12.f + sinf(x * 0.04f) * 3.f * n;
        return h;
    case PLANET_HOTH:
        n = gen_fbm2(x * 0.025f, z * 0.025f, 5);
        h = n * 22.f;
        return h;
    case PLANET_MUSTAFAR:
        n = gen_fbm2(x * 0.035f, z * 0.035f, 4);
        h = n * 16.f;
        /* lava rivers as lowlands */
        if (gen_noise2(x * 0.05f, z * 0.05f) < 0.38f) h = 1.5f + n * 2.f;
        return h;
    case PLANET_SPACE:
    default:
        return 0.f;
    }
}

static void upload_rgba(unsigned *tex_id, int size, unsigned char *px) {
    if (!*tex_id) glGenTextures(1, tex_id);
    glBindTexture(GL_TEXTURE_2D, *tex_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glGenerateMipmap(GL_TEXTURE_2D);
}

void gen_make_noise_tex(unsigned *tex_id, int size) {
    unsigned char *px = (unsigned char *)malloc((size_t)size * size * 4);
    int i, j;
    if (!px) return;
    for (j = 0; j < size; j++) {
        for (i = 0; i < size; i++) {
            float n = gen_fbm2(i * 0.08f, j * 0.08f, 4);
            float n2 = gen_noise2(i * 0.3f + 3, j * 0.3f);
            int k = (j * size + i) * 4;
            unsigned char v = (unsigned char)(n * 200 + n2 * 55);
            px[k + 0] = v;
            px[k + 1] = (unsigned char)(v * 0.92f);
            px[k + 2] = (unsigned char)(v * 0.85f);
            px[k + 3] = 255;
        }
    }
    upload_rgba(tex_id, size, px);
    free(px);
}

void gen_make_star_tex(unsigned *tex_id, int size) {
    unsigned char *px = (unsigned char *)calloc((size_t)size * size * 4, 1);
    int n, i;
    if (!px) return;
    for (n = 0; n < size * 6; n++) {
        int x = rand() % size, y = rand() % size;
        int k = (y * size + x) * 4;
        int b = 180 + rand() % 75;
        px[k] = px[k + 1] = px[k + 2] = (unsigned char)b;
        px[k + 3] = 255;
        if (rand() % 8 == 0) {
            for (i = -1; i <= 1; i++) {
                int xx = (x + i + size) % size;
                int kk = (y * size + xx) * 4;
                px[kk] = px[kk + 1] = px[kk + 2] = 255;
                px[kk + 3] = 255;
            }
        }
    }
    upload_rgba(tex_id, size, px);
    free(px);
}

void gen_make_hull_tex(unsigned *tex_id, int size, float r, float g, float b) {
    unsigned char *px = (unsigned char *)malloc((size_t)size * size * 4);
    int i, j;
    if (!px) return;
    for (j = 0; j < size; j++) {
        for (i = 0; i < size; i++) {
            float panel = ((i / 8) ^ (j / 8)) & 1 ? 1.f : 0.82f;
            float n = gen_noise2(i * 0.15f, j * 0.15f);
            float edge = 1.f;
            if (i % 16 == 0 || j % 16 == 0) edge = 0.55f;
            float shade = panel * edge * (0.75f + n * 0.25f);
            int k = (j * size + i) * 4;
            px[k + 0] = (unsigned char)(clampf(r * shade, 0, 1) * 255);
            px[k + 1] = (unsigned char)(clampf(g * shade, 0, 1) * 255);
            px[k + 2] = (unsigned char)(clampf(b * shade, 0, 1) * 255);
            px[k + 3] = 255;
        }
    }
    upload_rgba(tex_id, size, px);
    free(px);
}
