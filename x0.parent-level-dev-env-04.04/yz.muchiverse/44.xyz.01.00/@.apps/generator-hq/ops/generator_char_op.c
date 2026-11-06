/* generator_char_op.c — character layers, body first, clothes after.
 * The part list is a LAYER file. A part name of "naked" draws the body.
 * "shirt" draws one garment over the torso. Any other part is a sprite.csv
 * painted on top. Nothing here is a named lore person.
 *
 *   generator_char_op.+x run <layers.pdl> <outdir>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../../&.widgits/_shared-lib/stb_image_write.h"
#include "../../../&.widgits/concept-bank/ops/sd_map.h"

#define TW 48
#define TH 48
#define MAXL 16

static void mkp(const char *path) {
    char buf[700];
    snprintf(buf, sizeof buf, "%s", path);
    for (char *p = buf + 1; *p; p++) if (*p == '/') { *p = 0; mkdir(buf, 0755); *p = '/'; }
    mkdir(buf, 0755);
}
static int in_ellipse(int x, int y, int cx, int cy, int rx, int ry) {
    int dx = x - cx, dy = y - cy;
    if (rx < 1 || ry < 1) return 0;
    return dx * dx * ry * ry + dy * dy * rx * rx <= rx * rx * ry * ry;
}
static void put(unsigned char *px, int x, int y, int r, int g, int b) {
    unsigned char *p;
    if (x < 0 || y < 0 || x >= TW || y >= TH) return;
    p = px + (y * TW + x) * 4;
    p[0] = (unsigned char)r; p[1] = (unsigned char)g; p[2] = (unsigned char)b; p[3] = 255;
}
/* Super-deformed, the RPG Maker proportion: the head is about half the
 * figure, the torso and legs are short. Pixel blocks, not a realistic body. */
static void draw_naked(unsigned char *px) {
    int x, y;
    for (y = 0; y < TH; y++) for (x = 0; x < TW; x++) {
        int body = 0;
        if (in_ellipse(x, y, 24, 14, 14, 12)) body = 1;
        if (x >= 22 && x <= 25 && y >= 24 && y <= 27) body = 1;
        if (x >= 19 && x <= 28 && y >= 27 && y <= 36) body = 1;
        if (x >= 15 && x <= 19 && y >= 28 && y <= 34) body = 1;
        if (x >= 28 && x <= 32 && y >= 28 && y <= 34) body = 1;
        if (x >= 20 && x <= 23 && y >= 36 && y <= 45) body = 1;
        if (x >= 24 && x <= 27 && y >= 36 && y <= 45) body = 1;
        if (body) put(px, x, y, 214, 164, 130);
    }
    /* Two eyes and a small mouth. A mannequin face, not a named person. */
    for (y = 13; y <= 16; y++) for (x = 16; x <= 18; x++) put(px, x, y, 40, 36, 48);
    for (y = 13; y <= 16; y++) for (x = 29; x <= 31; x++) put(px, x, y, 40, 36, 48);
    for (x = 22; x <= 25; x++) put(px, x, 20, 160, 90, 90);
}
static void draw_shirt(unsigned char *px) {
    int x, y;
    for (y = 27; y <= 35; y++) for (x = 19; x <= 28; x++) put(px, x, y, 48, 72, 120);
    for (y = 28; y <= 33; y++) for (x = 16; x <= 19; x++) put(px, x, y, 48, 72, 120);
    for (y = 28; y <= 33; y++) for (x = 28; x <= 31; x++) put(px, x, y, 48, 72, 120);
}
static int paint_csv(unsigned char *px, const char *path) {
    FILE *f = fopen(path, "r");
    char line[80];
    int w = 0, h = 0, n = 0;
    if (!f) { fprintf(stderr, "no part %s\n", path); return 2; }
    if (!fgets(line, sizeof line, f) || sscanf(line, "# resolution=%d", &w) != 1) { fclose(f); return 2; }
    h = w;
    { char *xp = strchr(line, 'x'); if (xp) h = atoi(xp + 1); }
    if (w != TW || h != TH) { fclose(f); fprintf(stderr, "part not %dx%d\n", TW, TH); return 2; }
    while (fgets(line, sizeof line, f)) {
        int r, g, b, a, x, y;
        if (sscanf(line, "%d,%d,%d,%d", &r, &g, &b, &a) != 4) continue;
        x = n % TW; y = n / TW; n++;
        if (a > 16) put(px, x, y, r, g, b);
    }
    fclose(f);
    return n == TW * TH ? 0 : 2;
}
static int write_csv(const char *path, const unsigned char *px) {
    FILE *f = fopen(path, "w");
    int i, n = TW * TH;
    if (!f) return 2;
    fprintf(f, "# resolution=%d\nscale=1.0\ntransform=0,0,0\nr,g,b,a\n", TW);
    for (i = 0; i < n; i++)
        fprintf(f, "%d,%d,%d,%d\n", px[i * 4], px[i * 4 + 1], px[i * 4 + 2], px[i * 4 + 3]);
    fclose(f);
    return 0;
}
static int write_png(const char *path, const unsigned char *px) {
    return stbi_write_png(path, TW, TH, 4, px, TW * 4) ? 0 : 2;
}
int main(int argc, char **argv) {
    FILE *f;
    char line[400];
    unsigned char *px;
    char names[MAXL][32];
    int n = 0, i, rc = 0;
    char color_id[40] = "", detail_id[40] = "";
    double color_w = -1, detail_w = -1;
    char dir[700], path[760];
    if (argc != 5 || strcmp(argv[1], "run") != 0) {
        fprintf(stderr, "usage: run <layers.pdl> <outdir> <words.pdl>\n");
        return 2;
    }
    f = fopen(argv[2], "r");
    if (!f) { fprintf(stderr, "no layers\n"); return 2; }
    px = calloc(TW * TH, 4);
    if (!px) { fclose(f); return 2; }
    while (fgets(line, sizeof line, f)) {
        char *p, *name, *spec;
        if (strncmp(line, "CONCEPT |", 9) == 0) {
            p = line + 9; while (*p == ' ') p++;
            name = p; p = strchr(p, '|'); if (!p) continue;
            *p = 0; while (name[0] && name[strlen(name) - 1] == ' ') name[strlen(name) - 1] = 0;
            spec = p + 1; while (*spec == ' ') spec++;
            spec[strcspn(spec, "\r\n")] = 0;
            if (strcmp(name, "color") == 0) snprintf(color_id, sizeof color_id, "%s", spec);
            else if (strcmp(name, "detail") == 0) snprintf(detail_id, sizeof detail_id, "%s", spec);
            continue;
        }
        if (strncmp(line, "LAYER |", 7) != 0) continue;
        p = line + 7;
        while (*p == ' ') p++;
        p = strchr(p, '|'); if (!p) continue;
        name = p + 1; while (*name == ' ') name++;
        p = strchr(name, '|'); if (!p) continue;
        *p = 0;
        spec = p + 1; while (*spec == ' ') spec++;
        spec[strcspn(spec, "\r\n")] = 0;
        while (name[strlen(name) - 1] == ' ') name[strlen(name) - 1] = 0;
        if (n < MAXL) snprintf(names[n], sizeof names[n], "%s", name);
        n++;
        if (strcmp(spec, "none") == 0) continue;
        if (strcmp(spec, "naked") == 0) draw_naked(px);
        else if (strcmp(spec, "shirt") == 0) draw_shirt(px);
        else if ((rc = paint_csv(px, spec)) != 0) { free(px); fclose(f); return rc; }
    }
    fclose(f);
    if (!color_id[0] || !detail_id[0]) { free(px); fprintf(stderr, "need color and detail word ids\n"); return 2; }
    {
        FILE *wf = fopen(argv[4], "r");
        char wline[400], *f4[8];
        if (!wf) { free(px); fprintf(stderr, "no word bank\n"); return 2; }
        while (fgets(wline, sizeof wline, wf)) {
            int nf, k;
            char *bar;
            if (strncmp(wline, "WORD", 4) != 0) continue;
            nf = 0; f4[nf++] = wline;
            for (bar = wline; *bar && nf < 8; bar++) if (*bar == '|') { *bar = 0; f4[nf++] = bar + 1; }
            for (k = 0; k < nf; k++) {
                while (*f4[k] == ' ') f4[k]++;
                { char *e = f4[k] + strlen(f4[k]); while (e > f4[k] && (e[-1] == ' ' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; }
            }
            if (nf >= 5) {
                double w = atof(f4[4]);
                if (!strcmp(f4[1], color_id)) color_w = w;
                if (!strcmp(f4[1], detail_id)) detail_w = w;
            }
        }
        fclose(wf);
    }
    if (color_w < 0 || detail_w < 0) { free(px); fprintf(stderr, "word id not in the bank\n"); return 2; }
    snprintf(dir, sizeof dir, "%s", argv[3]);
    mkp(dir);
    snprintf(path, sizeof path, "%s/sprite.csv", dir);
    if (write_csv(path, px) != 0) { free(px); return 2; }
    snprintf(path, sizeof path, "%s/tile.png", dir);
    if (write_png(path, px) != 0) { free(px); return 2; }
    printf("GUIDANCE: %.2f\nSTEPS: %d\n", sd_guidance(color_w), sd_steps(detail_w));
    printf("MAP color=%s:%.2f detail=%s:%.2f\n", color_id, color_w, detail_id, detail_w);
    printf("CHAR layers=%d pixel24_28=%d,%d,%d,%d pixel16_14=%d,%d,%d,%d\n", n,
           px[(28 * TW + 24) * 4], px[(28 * TW + 24) * 4 + 1],
           px[(28 * TW + 24) * 4 + 2], px[(28 * TW + 24) * 4 + 3],
           px[(14 * TW + 16) * 4], px[(14 * TW + 16) * 4 + 1],
           px[(14 * TW + 16) * 4 + 2], px[(14 * TW + 16) * 4 + 3]);
    for (i = 0; i < n && i < MAXL; i++) printf("PART %s\n", names[i]);
    free(px);
    return 0;
}
