/* prism_op - render the "Custom" theme prism: a tiling of shaped facets lit by a hex/colour algorithm (owner 2026-10-28: settings > theme > Custom,
 * a default rainbow prism with 4 adjustable variables). Deterministic: the same four variables give the same bytes. No state, no network.
 *   prism_op <out_prefix> <shape> <algo> <hue> <light> [size]
 *     shape 0 triangle | 1 hexagon | 2 diamond | 3 circle        algo 0 radial | 1 linear | 2 spiral | 3 noise (how the rainbow spreads over the facets)
 *     hue   0-359 (rotates the whole rainbow)                     light 0-100 (brightness of the facets)         size 64-1024, default 256 (square)
 *   Writes <out_prefix>.rgba (size*size*4 bytes, R G B A), <out_prefix>.rgba.receipt.txt (overlay_w=/overlay_h=, what the shared <canvas sprite=...> element reads to show it) and <out_prefix>.png (uncompressed PNG), prints
 *   `prism: shape=<s> algo=<a> hue=<h> light=<l> size=<n> fnv=<16 hex of the pixels>`.   Exit 0 | 1 cannot write | 2 usage / out of range.
 * The default preset is shape 1, algo 2, hue 0, light 60 ("rainbow").   Build: sh build_prism_op.sh (links -lm) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

static void hsl(double h, double s, double l, unsigned char *o) {
    double c = (1 - fabs(2 * l - 1)) * s, hp = fmod(h, 360.0) / 60.0, x = c * (1 - fabs(fmod(hp, 2.0) - 1)), r = 0, g = 0, b = 0;
    if (hp < 1) { r = c; g = x; } else if (hp < 2) { r = x; g = c; } else if (hp < 3) { g = c; b = x; } else if (hp < 4) { g = x; b = c; } else if (hp < 5) { r = x; b = c; } else { r = c; b = x; }
    double m = l - c / 2; o[0] = (unsigned char)lround((r + m) * 255); o[1] = (unsigned char)lround((g + m) * 255); o[2] = (unsigned char)lround((b + m) * 255); o[3] = 255;
}
static int inside(int shape, double u, double v) {
    switch (shape) {
    case 0: return v >= -0.85 && v <= 0.85 && fabs(u) <= (v + 0.85) / 1.7 * 0.9;
    case 1: return fabs(v) <= 0.85 && fabs(u) * 0.866 + fabs(v) * 0.5 <= 0.8;
    case 2: return fabs(u) + fabs(v) <= 0.9;
    default: return u * u + v * v <= 0.8;
    }
}
static double spread(int algo, int cx, int cy, int n) {
    double px = cx + 0.5 - n / 2.0, py = cy + 0.5 - n / 2.0, d = sqrt(px * px + py * py) / (n / 2.0 * 1.4142);
    if (algo == 0) { return d; }
    if (algo == 1) { return (double)(cx + cy) / (2.0 * n - 2.0); }
    if (algo == 2) { return fmod(atan2(py, px) / 6.283185307 + 0.5 + d * 1.5, 1.0); }
    uint32_t h = (uint32_t)cx * 73856093u ^ (uint32_t)cy * 19349663u; h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return (double)h / 4294967295.0;
}
static uint32_t crc_t[256];
static void crc_init(void) { for (uint32_t i = 0; i < 256; i++) { uint32_t c = i; for (int k = 0; k < 8; k++) { c = (c & 1) ? 0xedb88320u ^ (c >> 1) : c >> 1; } crc_t[i] = c; } }
static uint32_t crc(uint32_t c, const unsigned char *b, size_t n) { for (size_t i = 0; i < n; i++) { c = crc_t[(c ^ b[i]) & 255] ^ (c >> 8); } return c; }
static void be32(unsigned char *p, uint32_t v) { p[0] = (unsigned char)(v >> 24); p[1] = (unsigned char)(v >> 16); p[2] = (unsigned char)(v >> 8); p[3] = (unsigned char)v; }
static int chunk(FILE *f, const char *type, const unsigned char *d, size_t n) {
    unsigned char h[8]; be32(h, (uint32_t)n); memcpy(h + 4, type, 4); uint32_t c = crc(0xffffffffu, (const unsigned char *)type, 4); c = crc(c, d, n) ^ 0xffffffffu; unsigned char t[4]; be32(t, c);
    return fwrite(h, 1, 8, f) == 8 && (n == 0 || fwrite(d, 1, n, f) == n) && fwrite(t, 1, 4, f) == 4;
}
static int write_png(const char *path, const unsigned char *px, int n) {
    size_t raw = (size_t)n * (1 + (size_t)n * 4), nb = (raw + 65534) / 65535, zl = 2 + raw + nb * 5 + 4; unsigned char *z = malloc(zl), *r = malloc(raw); if (!z || !r) { free(z); free(r); return 0; }
    for (int y = 0; y < n; y++) { r[(size_t)y * (1 + (size_t)n * 4)] = 0; memcpy(r + (size_t)y * (1 + (size_t)n * 4) + 1, px + (size_t)y * n * 4, (size_t)n * 4); }
    size_t o = 0; z[o++] = 0x78; z[o++] = 0x01; uint32_t a = 1, b = 0;
    for (size_t i = 0; i < raw; i++) { a = (a + r[i]) % 65521; b = (b + a) % 65521; }
    for (size_t pos = 0; pos < raw; ) { size_t len = raw - pos > 65535 ? 65535 : raw - pos; z[o++] = (raw - pos <= 65535) ? 1 : 0; z[o++] = (unsigned char)len; z[o++] = (unsigned char)(len >> 8); z[o++] = (unsigned char)~len; z[o++] = (unsigned char)(~len >> 8); memcpy(z + o, r + pos, len); o += len; pos += len; }
    be32(z + o, (b << 16) | a); o += 4;
    FILE *f = fopen(path, "wb"); int ok = f != NULL; unsigned char ih[13]; be32(ih, (uint32_t)n); be32(ih + 4, (uint32_t)n); ih[8] = 8; ih[9] = 6; ih[10] = 0; ih[11] = 0; ih[12] = 0;
    if (ok) { crc_init(); ok = fwrite("\x89PNG\r\n\x1a\n", 1, 8, f) == 8 && chunk(f, "IHDR", ih, 13) && chunk(f, "IDAT", z, o) && chunk(f, "IEND", NULL, 0); if (fclose(f)) { ok = 0; } }
    free(z); free(r); return ok;
}
int main(int argc, char **argv) {
    if (argc < 6 || argc > 7) { fprintf(stderr, "Usage: prism_op <out_prefix> <shape 0-3> <algo 0-3> <hue 0-359> <light 0-100> [size 64-1024]\n"); return 2; }
    char *e; long shape = strtol(argv[2], &e, 10); if (*e) { return 2; } long algo = strtol(argv[3], &e, 10); if (*e) { return 2; } long hue = strtol(argv[4], &e, 10); if (*e) { return 2; }
    long light = strtol(argv[5], &e, 10); if (*e) { return 2; } long size = 256; if (argc == 7) { size = strtol(argv[6], &e, 10); if (*e) { return 2; } }
    if (shape < 0 || shape > 3 || algo < 0 || algo > 3 || hue < 0 || hue > 359 || light < 0 || light > 100 || size < 64 || size > 1024) { fprintf(stderr, "prism_op: variable out of range\n"); return 2; }
    int n = (int)size, cell = n / 8; unsigned char *px = malloc((size_t)n * n * 4); if (!px) { return 1; } uint64_t fnv = 1469598103934665603ull;
    for (int y = 0; y < n; y++) for (int x = 0; x < n; x++) {
        int cx = x / cell, cy = y / cell; if (cx > 7) { cx = 7; } if (cy > 7) { cy = 7; }
        double u = ((x - cx * cell) + 0.5) / cell * 2 - 1, v = ((y - cy * cell) + 0.5) / cell * 2 - 1, t = spread((int)algo, cx, cy, 8), h = fmod((double)hue + t * 300.0, 360.0);
        unsigned char *p = px + ((size_t)y * n + x) * 4;
        if (inside((int)shape, u, v)) { double l = 0.25 + light * 0.004 + 0.12 * (u - v) / 2; if (l < 0.02) { l = 0.02; } if (l > 0.95) { l = 0.95; } hsl(h, 0.85, l, p); }
        else { hsl(h, 0.3, 0.08, p); }
        for (int k = 0; k < 4; k++) { fnv ^= p[k]; fnv *= 1099511628211ull; }
    }
    char path[4200]; snprintf(path, sizeof path, "%.4000s.rgba", argv[1]); FILE *f = fopen(path, "wb"); if (!f || fwrite(px, 1, (size_t)n * n * 4, f) != (size_t)n * n * 4) { if (f) { fclose(f); } free(px); return 1; } if (fclose(f)) { free(px); return 1; }
    snprintf(path, sizeof path, "%.4000s.rgba.receipt.txt", argv[1]); f = fopen(path, "w"); if (!f) { free(px); return 1; } fprintf(f, "overlay_w=%d\noverlay_h=%d\n", n, n); if (fclose(f)) { free(px); return 1; }
    snprintf(path, sizeof path, "%.4000s.png", argv[1]); if (!write_png(path, px, n)) { free(px); return 1; }
    free(px); printf("prism: shape=%ld algo=%ld hue=%ld light=%ld size=%d fnv=%016llx\n", shape, algo, hue, light, n, (unsigned long long)fnv); return 0;
}
