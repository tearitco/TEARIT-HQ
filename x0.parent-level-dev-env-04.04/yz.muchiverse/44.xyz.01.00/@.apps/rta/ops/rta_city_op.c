/* rta_city_op — raster a Battery .pdl and paint it.
 * Reads BOX, CELL, and FEAT rows (road, path, park, water, pier, building).
 * One cell, one kind. Buildings do not cover water or pier. A grass or
 * park cell that touches water becomes beach. Tile samples are 16 px,
 * nearest neighbour, so the whole slice fits one picture. Polish of the
 * autotile edges is later.
 *   rta_city_op <tileset_dir> <feat.pdl> <out.png> <grid.txt>
 * grid.txt is north row first. Row 0 of the file is the north edge.
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o rta_city_op.+x rta_city_op.c -lpng -lz -lm
 */
#include <png.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TW 16
#define MAXW 220
#define MAXH 300
#define GRASS 0
#define PARK 1
#define PATH 2
#define ROAD 3
#define BEACH 4
#define WATER 5
#define PIER 6
#define BLDG 7

static const char GLY[] = ".KPRSWiB";

typedef struct { int w, h; unsigned char *px; } Img;

static int load_png(const char *path, Img *im) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info = png ? png_create_info_struct(png) : NULL;
    if (!png || !info || setjmp(png_jmpbuf(png))) { if (fp) fclose(fp); return -1; }
    png_init_io(png, fp);
    png_read_info(png, info);
    int w = (int)png_get_image_width(png, info), h = (int)png_get_image_height(png, info);
    png_byte color = png_get_color_type(png, info), depth = png_get_bit_depth(png, info);
    if (depth == 16) png_set_strip_16(png);
    if (color == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (color == PNG_COLOR_TYPE_GRAY && depth < 8) png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
    if (color == PNG_COLOR_TYPE_RGB || color == PNG_COLOR_TYPE_GRAY || color == PNG_COLOR_TYPE_PALETTE)
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    if (color == PNG_COLOR_TYPE_GRAY || color == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_gray_to_rgb(png);
    png_read_update_info(png, info);
    unsigned char *px = malloc((size_t)w * (size_t)h * 4);
    png_bytep *rows = malloc(sizeof(png_bytep) * (size_t)h);
    if (!px || !rows) return -1;
    for (int y = 0; y < h; y++) rows[y] = px + (size_t)y * (size_t)w * 4;
    png_read_image(png, rows);
    png_destroy_read_struct(&png, &info, NULL);
    free(rows); fclose(fp);
    im->w = w; im->h = h; im->px = px;
    return 0;
}

static int save_png(const char *path, int w, int h, unsigned char *px) {
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info = png ? png_create_info_struct(png) : NULL;
    if (!png || !info || setjmp(png_jmpbuf(png))) { fclose(fp); return -1; }
    png_init_io(png, fp);
    png_set_IHDR(png, info, w, h, 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);
    png_bytep *rows = malloc(sizeof(png_bytep) * (size_t)h);
    for (int y = 0; y < h; y++) rows[y] = px + (size_t)y * (size_t)w * 4;
    png_write_image(png, rows);
    png_write_end(png, NULL);
    png_destroy_write_struct(&png, &info);
    free(rows); fclose(fp);
    return 0;
}

static void sample(const Img *s, int sx, int sy, unsigned char *dst) {
    for (int y = 0; y < TW; y++) for (int x = 0; x < TW; x++) {
        int X = sx + x * 2, Y = sy + y * 2;
        if (X >= s->w) X = s->w - 1;
        if (Y >= s->h) Y = s->h - 1;
        memcpy(dst + ((size_t)y * TW + x) * 4, s->px + ((size_t)Y * s->w + X) * 4, 4);
    }
}

static int kind_of(const char *s) {
    if (!strcmp(s, "park")) return PARK;
    if (!strcmp(s, "path")) return PATH;
    if (!strcmp(s, "road")) return ROAD;
    if (!strcmp(s, "water")) return WATER;
    if (!strcmp(s, "pier")) return PIER;
    if (!strcmp(s, "building")) return BLDG;
    if (!strcmp(s, "beach")) return BEACH;
    return -1;
}

static int pip(double x, double y, const double *xs, const double *ys, int n) {
    int inside = 0;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        if (((ys[i] > y) != (ys[j] > y)) &&
            (x < (xs[j] - xs[i]) * (y - ys[i]) / (ys[j] - ys[i] + 1e-30) + xs[i]))
            inside = !inside;
    }
    return inside;
}

static void stamp(unsigned char *g, int gw, int gh, int x, int y, int k) {
    if (x < 0 || y < 0 || x >= gw || y >= gh) return;
    unsigned char *c = g + y * gw + x;
    if (k == BLDG && (*c == WATER || *c == PIER)) return;
    if (k > *c) *c = (unsigned char)k;
}

int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr, "usage: rta_city_op <tileset_dir> <feat.pdl> <out.png> <grid.txt>\n");
        return 2;
    }
    double south = 0, west = 0, north = 0, east = 0, meters = 10;
    int have_box = 0, have_cell = 0;
    FILE *fp = fopen(argv[2], "r");
    if (!fp) return 3;
    char line[512];
    /* First pass: header only, so the grid exists before features. */
    while (fgets(line, sizeof line, fp)) {
        if (strncmp(line, "BOX |", 5) == 0) {
            if (sscanf(line, "BOX | south | %lf | west | %lf | north | %lf | east | %lf", &south, &west, &north, &east) == 4)
                have_box = 1;
        } else if (strncmp(line, "CELL |", 6) == 0) {
            double olon, olat;
            if (sscanf(line, "CELL | meters | %lf | origin_lon | %lf | origin_lat | %lf", &meters, &olon, &olat) >= 1)
                have_cell = 1;
        }
    }
    if (!have_box || !have_cell || meters < 1) return 4;
    double dlat = meters / 110540.0;
    double dlon = meters / (111320.0 * cos(south * 3.141592653589793 / 180.0));
    int gw = (int)((east - west) / dlon + 0.5);
    int gh = (int)((north - south) / dlat + 0.5);
    if (gw < 1 || gh < 1 || gw > MAXW || gh > MAXH) {
        fprintf(stderr, "grid %dx%d outside cap\n", gw, gh);
        return 4;
    }
    unsigned char *g = calloc((size_t)gw * gh, 1);
    if (!g) return 5;

    rewind(fp);
    double *rx = NULL, *ry = NULL;
    int rn = 0, rcap = 0;
    char fkind[32] = "", fname[160] = "";
    int fgeom_poly = 0, open = 0;
    int nfeat = 0;

    void flush(void) {
        if (!open || rn < 2) { rn = 0; open = 0; return; }
        int k = kind_of(fkind);
        if (k < 0) { rn = 0; open = 0; return; }
        if (fgeom_poly) {
            double minx = rx[0], maxx = rx[0], miny = ry[0], maxy = ry[0];
            for (int i = 1; i < rn; i++) {
                if (rx[i] < minx) minx = rx[i];
                if (rx[i] > maxx) maxx = rx[i];
                if (ry[i] < miny) miny = ry[i];
                if (ry[i] > maxy) maxy = ry[i];
            }
            int x0 = (int)((minx - west) / dlon) - 1, x1 = (int)((maxx - west) / dlon) + 1;
            int y0 = (int)((miny - south) / dlat) - 1, y1 = (int)((maxy - south) / dlat) + 1;
            if (x0 < 0) x0 = 0;
            if (y0 < 0) y0 = 0;
            if (x1 >= gw) x1 = gw - 1;
            if (y1 >= gh) y1 = gh - 1;
            for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
                double lon = west + (x + 0.5) * dlon, lat = south + (y + 0.5) * dlat;
                if (pip(lon, lat, rx, ry, rn)) stamp(g, gw, gh, x, y, k);
            }
        } else {
            for (int i = 1; i < rn; i++) {
                double dx = (rx[i] - rx[i - 1]) * 111320.0 * cos(south * 3.141592653589793 / 180.0);
                double dy = (ry[i] - ry[i - 1]) * 110540.0;
                int steps = (int)(sqrt(dx * dx + dy * dy) / (meters * 0.5));
                if (steps < 1) steps = 1;
                for (int s = 0; s <= steps; s++) {
                    double t = (double)s / steps;
                    double lon = rx[i - 1] + (rx[i] - rx[i - 1]) * t;
                    double lat = ry[i - 1] + (ry[i] - ry[i - 1]) * t;
                    stamp(g, gw, gh, (int)((lon - west) / dlon), (int)((lat - south) / dlat), k);
                }
            }
        }
        nfeat++;
        rn = 0;
        open = 0;
    }

    while (fgets(line, sizeof line, fp)) {
        if (strncmp(line, "FEAT |", 6) != 0) continue;
        char kind[32], name[160], geom[16];
        int part = 0;
        double lon = 0, lat = 0;
        char *bar[7], *q = line;
        int nf = 0;
        for (; nf < 7; nf++) {
            bar[nf] = q;
            char *b = strchr(q, '|');
            if (!b) { nf++; break; }
            *b = 0;
            q = b + 1;
        }
        if (nf < 7) continue;
        for (int i = 0; i < 7; i++) {
            while (*bar[i] == ' ') bar[i]++;
            char *e = bar[i] + strlen(bar[i]);
            while (e > bar[i] && (e[-1] == ' ' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0;
        }
        snprintf(kind, sizeof kind, "%s", bar[1]);
        snprintf(name, sizeof name, "%s", bar[2]);
        snprintf(geom, sizeof geom, "%s", bar[3]);
        part = atoi(bar[4]);
        lon = atof(bar[5]);
        lat = atof(bar[6]);
        int poly = strcmp(geom, "polygon") == 0;
        int gap = 0;
        if (open && rn > 0) {
            double dx = (lon - rx[rn - 1]) * 111320.0 * cos(south * 3.141592653589793 / 180.0);
            double dy = (lat - ry[rn - 1]) * 110540.0;
            double dist = sqrt(dx * dx + dy * dy);
            gap = dist > (poly ? 250.0 : 80.0);
        }
        /* The .pdl stores part 0 on every vertex. A polygon ring ends when it returns to its first point. */
        int closed = poly && open && rn >= 3 && fabs(lon - rx[0]) < 1e-6 && fabs(lat - ry[0]) < 1e-6;
        if (!open || strcmp(kind, fkind) || strcmp(name, fname) || poly != fgeom_poly || gap) {
            flush();
            closed = 0;
        }
        if (!open) {
            snprintf(fkind, sizeof fkind, "%s", kind);
            snprintf(fname, sizeof fname, "%s", name);
            fgeom_poly = poly;
            open = 1;
        }
        if (rn + 1 > rcap) {
            rcap = rcap ? rcap * 2 : 64;
            rx = realloc(rx, (size_t)rcap * sizeof(double));
            ry = realloc(ry, (size_t)rcap * sizeof(double));
            if (!rx || !ry) return 5;
        }
        rx[rn] = lon;
        ry[rn] = lat;
        rn++;
        (void)part;
        if (closed) flush();
    }
    flush();
    fclose(fp);

    /* Grass that can reach the map border without crossing a road or a
     * building is the harbour outside the peninsula, not a yard. */
    {
        int *stack = malloc((size_t)gw * gh * sizeof(int));
        int sp = 0;
        if (!stack) return 5;
        #define PUSH(i) do { int _i = (i); if (g[_i] == GRASS) { g[_i] = WATER; stack[sp++] = _i; } } while (0)
        for (int x = 0; x < gw; x++) { PUSH(x); PUSH((gh - 1) * gw + x); }
        for (int y = 0; y < gh; y++) { PUSH(y * gw); PUSH(y * gw + gw - 1); }
        while (sp > 0) {
            int i = stack[--sp];
            int x = i % gw, y = i / gw;
            if (x > 0) PUSH(i - 1);
            if (x + 1 < gw) PUSH(i + 1);
            if (y > 0) PUSH(i - gw);
            if (y + 1 < gh) PUSH(i + gw);
        }
        #undef PUSH
        free(stack);
    }

    for (int y = 0; y < gh; y++) for (int x = 0; x < gw; x++) {
        if (g[y * gw + x] != GRASS && g[y * gw + x] != PARK) continue;
        int touch = 0;
        if (x > 0 && g[y * gw + x - 1] == WATER) touch = 1;
        if (x + 1 < gw && g[y * gw + x + 1] == WATER) touch = 1;
        if (y > 0 && g[(y - 1) * gw + x] == WATER) touch = 1;
        if (y + 1 < gh && g[(y + 1) * gw + x] == WATER) touch = 1;
        if (touch) g[y * gw + x] = BEACH;
    }

    char path[512];
    Img a1, a2, b;
    snprintf(path, sizeof path, "%s/World_A1.png", argv[1]); if (load_png(path, &a1)) return 6;
    snprintf(path, sizeof path, "%s/World_A2.png", argv[1]); if (load_png(path, &a2)) return 6;
    snprintf(path, sizeof path, "%s/World_B.png", argv[1]); if (load_png(path, &b)) return 6;
    unsigned char tile[8][TW * TW * 4];
    sample(&a2, 2 * 48 + 40, 48 + 40, tile[GRASS]);
    sample(&a2, 2 * 48 + 40, 48 + 40, tile[PARK]);
    sample(&a2, 10 * 48 + 40, 6 * 48 + 48 + 40, tile[PATH]);
    sample(&a2, 4 * 48 + 40, 6 * 48 + 48 + 40, tile[ROAD]);
    sample(&a2, 40, 6 * 48 + 48 + 40, tile[BEACH]);
    sample(&a1, 40, 48 + 40, tile[WATER]);
    sample(&b, 2 * 48 + 8, 8, tile[PIER]);
    sample(&b, 4 * 48 + 8, 5 * 48 + 8, tile[BLDG]);

    int pw = gw * TW, ph = gh * TW;
    unsigned char *px = malloc((size_t)pw * ph * 4);
    if (!px) return 7;
    for (int y = 0; y < gh; y++) for (int x = 0; x < gw; x++) {
        int sy = gh - 1 - y; /* png row 0 is north */
        unsigned char *src = tile[g[y * gw + x]];
        for (int ty = 0; ty < TW; ty++)
            memcpy(px + ((size_t)(sy * TW + ty) * pw + x * TW) * 4, src + (size_t)ty * TW * 4, TW * 4);
    }
    if (save_png(argv[3], pw, ph, px)) return 8;

    fp = fopen(argv[4], "w");
    if (!fp) return 9;
    fprintf(fp, "# north row first w=%d h=%d meters=%.0f west=%.5f south=%.5f\n", gw, gh, meters, west, south);
    int count[8] = {0};
    for (int y = gh - 1; y >= 0; y--) {
        for (int x = 0; x < gw; x++) {
            unsigned char k = g[y * gw + x];
            count[k]++;
            fputc(GLY[k], fp);
        }
        fputc('\n', fp);
    }
    fclose(fp);
    printf("w=%d h=%d features=%d grass=%d park=%d path=%d road=%d beach=%d water=%d pier=%d building=%d png=%dx%d\n",
           gw, gh, nfeat, count[GRASS], count[PARK], count[PATH], count[ROAD], count[BEACH], count[WATER], count[PIER], count[BLDG], pw, ph);
    return (count[WATER] > 0 && count[ROAD] > 0) ? 0 : 10;
}
