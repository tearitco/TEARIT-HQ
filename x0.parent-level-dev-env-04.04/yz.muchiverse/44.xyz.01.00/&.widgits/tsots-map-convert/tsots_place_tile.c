/* tsots_place_tile — live place of ONE normal RPG Maker tile.
 *
 * This is the C op the palette will call. It does not run Python.
 * It does not move the camera, and it does not touch bv_render_3d.c.
 *
 * What one call does
 *   1. Writes tile_id into layers.txt at (x, y) on layer 0..3.
 *   2. Repaints that cell's floor image (layers 0 then 1) or wall
 *      image (layers 2 then 3) at 24 px, nearest neighbor from the
 *      48 px sheet (same sample as tsots_paint.py draw_normal).
 *   3. Reuses an existing cells.rgba slot when the 24x24 block
 *      already exists, or appends one slot and rewrites the header.
 *   4. Rewrites that cell's "floor,wall" token in cells.txt.
 *   5. Bumps cells.generation in the desk folder.
 *
 * What it refuses
 *   A1-A4 autotile ids (2048..8191). Those need the four-quadrant
 *   tables and the neighbor shape. Batch paint still owns them
 *   (tsots_paint.py). A later op can port draw_autotile.
 *   Ids 1024..1535 (the unused MV gap) and anything outside 0..8191.
 *   A cell whose OTHER id in the same pair is already an autotile.
 *   Placing on top of that would wipe a painted autotile with a
 *   flat sheet copy. The files are left unchanged.
 *
 * Wall art
 *   A non-zero id on layer 2 or 3 is drawn into the wall slot.
 *   Passage flags and the star bit are not read here. The batch
 *   painter still decides walls from Tilesets.json. This op only
 *   has the id the palette handed it.
 *
 * Key 7 reload
 *   maker_load caches by desk directory and does not stat
 *   cells.rgba. This op cannot bust that cache without editing
 *   the renderer. After a place, leave the desk and open it again
 *   with key 7. cells.generation is only a counter for humans
 *   and for a future loader check.
 *
 * Build (libpng, no house renderer):
 *   gcc -O2 -Wall -Wextra -o tsots_place_tile.+x tsots_place_tile.c -lpng -lz
 *
 * Place:
 *   tsots_place_tile.+x --desk DESK --x X --y Y --layer 0..3 --id ID \
 *       --slug outside --registry tileset_registry.pdl --assets ASSETS
 *   ID 0 clears that layer. --slug matches tileset_registry.pdl
 *   (outside, overworld, inside, dungeon, sf_outside, sf_inside, ...).
 *   --assets is the directory that contains rmmv-www-img/.
 *
 * Self-test (writes only under /tmp/tsots-place-self):
 *   tsots_place_tile.+x --self-test
 *
 * PAL contract, when a row is wired later:
 *   call_op tsots_place_tile.+x --desk <desk_dir> --x <sx> --y <sy> \
 *       --layer <0-3> --id <mv_tile_id> --slug <tileset_slug> \
 *       --registry <tileset_registry.pdl> --assets <#.NNEST_ASSETS>
 *   Do not pass the camera. Do not pass a glyph letter.
 */

#include <ctype.h>
#include <errno.h>
#include <png.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define TILE_PX 24
#define TILE_A5 1536
#define TILE_A1 2048
#define TILE_MAX 8192
#define MAX_DIM 256
#define MAX_LINE 65536

typedef struct {
    int w, h;
    int id[6][MAX_DIM * MAX_DIM];
} Layers;

typedef struct {
    int w, h, tile_px, cols, ntiles;
    int floor[MAX_DIM * MAX_DIM];
    int wall[MAX_DIM * MAX_DIM];
} Cells;

typedef struct {
    int w, h;
    unsigned char *px; /* RGBA */
} Image;

static void die(const char *msg) {
    fprintf(stderr, "tsots_place_tile: %s\n", msg);
    exit(1);
}

static void *xmalloc(size_t n) {
    void *p = malloc(n ? n : 1);
    if (!p) die("out of memory");
    return p;
}

static char *trim(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) *--e = 0;
    return s;
}

static int is_normal(int id) {
    /* 0 clears. B-E are 1..1023. A5 is one static cell, 1536..2047. */
    if (id == 0) return 1;
    if (id > 0 && id < 1024) return 1;
    if (id >= TILE_A5 && id < TILE_A1) return 1;
    return 0;
}

static const char *sheet_part(int id) {
    if (id >= TILE_A5 && id < TILE_A1) return "a5";
    if (id > 0 && id < 256) return "b";
    if (id < 512) return "c";
    if (id < 768) return "d";
    if (id < 1024) return "e";
    return NULL;
}

/* Same source origin as tsots_paint.py draw_normal. Sheet tiles are 48 px. */
static void normal_src_xy(int tile, int *sx, int *sy) {
    const int w = 48;
    *sx = ((tile / 128) % 2 * 8 + tile % 8) * w;
    *sy = (((tile % 256) / 8) % 16) * w;
}

static int load_png(const char *path, Image *im) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info = png ? png_create_info_struct(png) : NULL;
    if (!png || !info || setjmp(png_jmpbuf(png))) {
        if (png) png_destroy_read_struct(&png, info ? &info : NULL, NULL);
        fclose(fp);
        return -1;
    }
    png_init_io(png, fp);
    png_read_info(png, info);
    int w = (int)png_get_image_width(png, info);
    int h = (int)png_get_image_height(png, info);
    png_byte color = png_get_color_type(png, info);
    png_byte depth = png_get_bit_depth(png, info);
    if (depth == 16) png_set_strip_16(png);
    if (color == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (color == PNG_COLOR_TYPE_GRAY && depth < 8) png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
    if (color == PNG_COLOR_TYPE_RGB || color == PNG_COLOR_TYPE_GRAY ||
        color == PNG_COLOR_TYPE_PALETTE)
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    if (color == PNG_COLOR_TYPE_GRAY || color == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);
    png_read_update_info(png, info);
    unsigned char *px = xmalloc((size_t)w * (size_t)h * 4);
    png_bytep *rows = xmalloc(sizeof(png_bytep) * (size_t)h);
    for (int y = 0; y < h; y++) rows[y] = px + (size_t)y * (size_t)w * 4;
    png_read_image(png, rows);
    png_destroy_read_struct(&png, &info, NULL);
    free(rows);
    fclose(fp);
    im->w = w;
    im->h = h;
    im->px = px;
    return 0;
}

static int save_png(const char *path, const Image *im) {
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info = png ? png_create_info_struct(png) : NULL;
    if (!png || !info || setjmp(png_jmpbuf(png))) {
        if (png) png_destroy_write_struct(&png, info ? &info : NULL);
        fclose(fp);
        return -1;
    }
    png_init_io(png, fp);
    png_set_IHDR(png, info, im->w, im->h, 8, PNG_COLOR_TYPE_RGBA,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);
    png_bytep *rows = xmalloc(sizeof(png_bytep) * (size_t)im->h);
    for (int y = 0; y < im->h; y++) rows[y] = im->px + (size_t)y * (size_t)im->w * 4;
    png_write_image(png, rows);
    png_write_end(png, NULL);
    png_destroy_write_struct(&png, &info);
    free(rows);
    fclose(fp);
    return 0;
}

static int read_layers(const char *path, Layers *L) {
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;
    char line[MAX_LINE];
    memset(L, 0, sizeof(*L));
    int layer = -1, count = 0;
    while (fgets(line, sizeof line, fp)) {
        if (sscanf(line, "width=%d", &L->w) == 1) continue;
        if (sscanf(line, "height=%d", &L->h) == 1) continue;
        if (strncmp(line, "layer ", 6) == 0) {
            layer = atoi(line + 6);
            count = 0;
            continue;
        }
        if (layer < 0 || layer > 5) continue;
        char *p = line;
        while (*p) {
            while (*p && isspace((unsigned char)*p)) p++;
            if (!*p) break;
            char *end = NULL;
            long v = strtol(p, &end, 10);
            if (end == p) break;
            if (count >= MAX_DIM * MAX_DIM) { fclose(fp); return -1; }
            L->id[layer][count++] = (int)v;
            p = end;
        }
    }
    fclose(fp);
    if (L->w < 1 || L->h < 1 || L->w > MAX_DIM || L->h > MAX_DIM) return -1;
    return 0;
}

static int write_layers(const char *path, const Layers *L) {
    char tmp[1024];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *fp = fopen(tmp, "w");
    if (!fp) return -1;
    fprintf(fp, "width=%d\nheight=%d\nlayers=6\n", L->w, L->h);
    int n = L->w * L->h;
    for (int layer = 0; layer < 6; layer++) {
        fprintf(fp, "layer %d\n", layer);
        for (int i = 0; i < n; i++) {
            fprintf(fp, "%d%s", L->id[layer][i], ((i + 1) % L->w) ? " " : "\n");
        }
    }
    if (fclose(fp) != 0) return -1;
    if (rename(tmp, path) != 0) return -1;
    return 0;
}

static int read_cells(const char *path, Cells *C) {
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;
    char line[MAX_LINE];
    memset(C, 0, sizeof(*C));
    C->tile_px = TILE_PX;
    C->cols = 16;
    int body = 0, row = 0;
    while (fgets(line, sizeof line, fp)) {
        if (!body) {
            if (strncmp(line, "cells", 5) == 0 &&
                (line[5] == '\n' || line[5] == '\r' || line[5] == 0)) {
                body = 1;
                continue;
            }
            sscanf(line, "width=%d", &C->w);
            sscanf(line, "height=%d", &C->h);
            sscanf(line, "tile_px=%d", &C->tile_px);
            sscanf(line, "atlas_cols=%d", &C->cols);
            sscanf(line, "atlas_tiles=%d", &C->ntiles);
            continue;
        }
        if (row >= C->h) break;
        char *p = line;
        for (int x = 0; x < C->w; x++) {
            int f = 0, w = 0;
            if (sscanf(p, "%d,%d", &f, &w) != 2) { fclose(fp); return -1; }
            C->floor[row * C->w + x] = f;
            C->wall[row * C->w + x] = w;
            char *comma = strchr(p, ',');
            if (!comma) { fclose(fp); return -1; }
            p = comma + 1;
            while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
            while (*p == ' ' || *p == '\t') p++;
        }
        row++;
    }
    fclose(fp);
    if (C->w < 1 || C->h < 1 || row != C->h || C->tile_px != TILE_PX || C->cols < 1)
        return -1;
    return 0;
}

static int write_cells(const char *path, const Cells *C) {
    char tmp[1024];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *fp = fopen(tmp, "w");
    if (!fp) return -1;
    fprintf(fp, "width=%d\nheight=%d\ntile_px=%d\natlas_cols=%d\natlas_tiles=%d\ncells\n",
            C->w, C->h, C->tile_px, C->cols, C->ntiles);
    for (int y = 0; y < C->h; y++) {
        for (int x = 0; x < C->w; x++) {
            int i = y * C->w + x;
            fprintf(fp, "%d,%d%s", C->floor[i], C->wall[i], x + 1 < C->w ? " " : "\n");
        }
    }
    if (fclose(fp) != 0) return -1;
    return rename(tmp, path);
}

static int atlas_rows(int ntiles, int cols) {
    return (ntiles + cols - 1) / cols;
}

static unsigned char *load_rgba(const char *path, int cols, int ntiles, int *out_bytes) {
    int aw = cols * TILE_PX;
    int ah = atlas_rows(ntiles, cols) * TILE_PX;
    size_t n = (size_t)aw * (size_t)ah * 4;
    FILE *fp = fopen(path, "rb");
    unsigned char *px = xmalloc(n);
    memset(px, 0, n);
    if (!fp) {
        *out_bytes = (int)n;
        return px;
    }
    size_t got = fread(px, 1, n, fp);
    fclose(fp);
    if (got != n) {
        /* Short file: keep the bytes we got, rest stays 0. */
    }
    *out_bytes = (int)n;
    return px;
}

static int save_rgba(const char *path, const unsigned char *px, int nbytes) {
    char tmp[1024];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *fp = fopen(tmp, "wb");
    if (!fp) return -1;
    if (fwrite(px, 1, (size_t)nbytes, fp) != (size_t)nbytes) { fclose(fp); return -1; }
    if (fclose(fp) != 0) return -1;
    return rename(tmp, path);
}

static void bump_generation(const char *desk) {
    char path[1024];
    snprintf(path, sizeof path, "%s/cells.generation", desk);
    int n = 0;
    FILE *fp = fopen(path, "r");
    if (fp) { if (fscanf(fp, "%d", &n) != 1) n = 0; fclose(fp); }
    fp = fopen(path, "w");
    if (fp) { fprintf(fp, "%d\n", n + 1); fclose(fp); }
}

static int lookup_sheet(const char *registry, const char *assets,
                        const char *slug, const char *part, char *out, size_t outsz) {
    FILE *fp = fopen(registry, "r");
    if (!fp) return -1;
    char line[1024];
    char root[512] = "";
    char file[512] = "";
    char want[128];
    snprintf(want, sizeof want, "%s.%s", slug, part);
    while (fgets(line, sizeof line, fp)) {
        char *bar1 = strchr(line, '|');
        if (!bar1) continue;
        char *bar2 = strchr(bar1 + 1, '|');
        if (!bar2) continue;
        *bar2 = 0;
        char *key = trim(bar1 + 1);
        char *val = trim(bar2 + 1);
        if (strcmp(key, "sheet_root") == 0) snprintf(root, sizeof root, "%s", val);
        if (strcmp(key, want) == 0) snprintf(file, sizeof file, "%s", val);
    }
    fclose(fp);
    if (!file[0] || !root[0]) return -1;
    snprintf(out, outsz, "%s/%s/%s", assets, root, file);
    return 0;
}

/* Dest pixel i maps to source i*2, matching PIL NEAREST 48 -> 24. */
static void blit_normal(unsigned char *dst, const Image *src, int tile) {
    int sx0, sy0;
    normal_src_xy(tile, &sx0, &sy0);
    for (int dy = 0; dy < TILE_PX; dy++) {
        for (int dx = 0; dx < TILE_PX; dx++) {
            int sx = sx0 + dx * 2;
            int sy = sy0 + dy * 2;
            unsigned char *d = dst + ((size_t)dy * TILE_PX + (size_t)dx) * 4;
            if (!src || sx < 0 || sy < 0 || sx >= src->w || sy >= src->h) {
                d[0] = d[1] = d[2] = d[3] = 0;
                continue;
            }
            const unsigned char *s = src->px + ((size_t)sy * (size_t)src->w + (size_t)sx) * 4;
            memcpy(d, s, 4);
        }
    }
}

/* Source over dest, one 24x24 RGBA block. */
static void composite(unsigned char *dst, const unsigned char *src) {
    for (int i = 0; i < TILE_PX * TILE_PX; i++) {
        unsigned char *d = dst + i * 4;
        const unsigned char *s = src + i * 4;
        int sa = s[3];
        int da = d[3];
        int inv = 255 - sa;
        int out_a = sa + da * inv / 255;
        if (out_a == 0) { d[0] = d[1] = d[2] = d[3] = 0; continue; }
        for (int c = 0; c < 3; c++) {
            int v = s[c] * sa + d[c] * da * inv / 255;
            d[c] = (unsigned char)(v / out_a);
        }
        d[3] = (unsigned char)out_a;
    }
}

static int fully_clear(const unsigned char *p) {
    for (int i = 0; i < TILE_PX * TILE_PX; i++) if (p[i * 4 + 3]) return 0;
    return 1;
}

static unsigned char *slot_ptr(unsigned char *atlas, int cols, int index) {
    int aw = cols * TILE_PX;
    int col = index % cols;
    int row = index / cols;
    return atlas + ((size_t)row * TILE_PX * (size_t)aw + (size_t)col * TILE_PX) * 4;
}

static void copy_slot_out(const unsigned char *atlas, int cols, int index, unsigned char *dst) {
    const unsigned char *src = slot_ptr((unsigned char *)atlas, cols, index);
    int aw = cols * TILE_PX;
    for (int y = 0; y < TILE_PX; y++)
        memcpy(dst + (size_t)y * TILE_PX * 4,
               src + (size_t)y * (size_t)aw * 4, TILE_PX * 4);
}

static int find_slot(const unsigned char *atlas, int cols, int ntiles, const unsigned char *tile) {
    unsigned char tmp[TILE_PX * TILE_PX * 4];
    for (int i = 0; i < ntiles; i++) {
        copy_slot_out(atlas, cols, i, tmp);
        if (memcmp(tmp, tile, sizeof tmp) == 0) return i;
    }
    return -1;
}

static unsigned char *ensure_slot(unsigned char *atlas, int *nbytes, int cols, int *ntiles, int index) {
    int need_rows = atlas_rows(index + 1, cols);
    /* Slot 0 always exists. Grow when the new index needs another row. */
    int old_bytes = *nbytes;
    int aw = cols * TILE_PX;
    int new_bytes = aw * (need_rows * TILE_PX) * 4;
    if (new_bytes > old_bytes) {
        unsigned char *grown = xmalloc((size_t)new_bytes);
        memset(grown, 0, (size_t)new_bytes);
        memcpy(grown, atlas, (size_t)old_bytes);
        free(atlas);
        atlas = grown;
        *nbytes = new_bytes;
    }
    if (index >= *ntiles) *ntiles = index + 1;
    return atlas;
}

static void write_slot(unsigned char *atlas, int cols, int index, const unsigned char *tile) {
    unsigned char *dst = slot_ptr(atlas, cols, index);
    int aw = cols * TILE_PX;
    for (int y = 0; y < TILE_PX; y++)
        memcpy(dst + (size_t)y * (size_t)aw * 4, tile + (size_t)y * TILE_PX * 4, TILE_PX * 4);
}

/* Paint ids in order into a 24x24 block. sheets[0] is unused. */
static void paint_ids(unsigned char *dst, int *ids, int n,
                      const char *registry, const char *assets, const char *slug) {
    memset(dst, 0, TILE_PX * TILE_PX * 4);
    Image cache_im[8];
    const char *cache_key[8];
    int nc = 0;
    memset(cache_im, 0, sizeof cache_im);
    for (int i = 0; i < n; i++) {
        if (ids[i] == 0) continue;
        const char *part = sheet_part(ids[i]);
        if (!part) die("internal: not a normal tile");
        Image *src = NULL;
        for (int c = 0; c < nc; c++) if (strcmp(cache_key[c], part) == 0) src = &cache_im[c];
        if (!src) {
            char path[1024];
            if (lookup_sheet(registry, assets, slug, part, path, sizeof path) != 0) {
                fprintf(stderr, "tsots_place_tile: no registry row %s.%s\n", slug, part);
                exit(1);
            }
            if (nc >= 8) die("too many sheets");
            if (load_png(path, &cache_im[nc]) != 0) {
                fprintf(stderr, "tsots_place_tile: cannot read %s\n", path);
                exit(1);
            }
            cache_key[nc] = part;
            src = &cache_im[nc];
            nc++;
        }
        unsigned char one[TILE_PX * TILE_PX * 4];
        blit_normal(one, src, ids[i]);
        composite(dst, one);
    }
    for (int c = 0; c < nc; c++) free(cache_im[c].px);
}

static int assign_slot(unsigned char **atlas, int *nbytes, Cells *C, const unsigned char *tile) {
    if (fully_clear(tile)) return 0;
    int found = find_slot(*atlas, C->cols, C->ntiles, tile);
    if (found >= 0) return found;
    int index = C->ntiles;
    *atlas = ensure_slot(*atlas, nbytes, C->cols, &C->ntiles, index);
    write_slot(*atlas, C->cols, index, tile);
    return index;
}

static int place(const char *desk, int x, int y, int layer, int id,
                 const char *registry, const char *assets, const char *slug) {
    if (layer < 0 || layer > 3) die("layer must be 0..3 (shadow and region are not placed)");
    if (!is_normal(id)) {
        fprintf(stderr,
                "tsots_place_tile: id %d is not a whole-cell sheet tile.\n"
                "This op places 0, B-E (1..1023), or A5 (1536..2047).\n"
                "A1-A4 autotiles stay on tsots_paint.py until a later op.\n", id);
        return 2;
    }
    char layers_path[1024], cells_path[1024], rgba_path[1024];
    snprintf(layers_path, sizeof layers_path, "%s/layers.txt", desk);
    snprintf(cells_path, sizeof cells_path, "%s/cells.txt", desk);
    snprintf(rgba_path, sizeof rgba_path, "%s/cells.rgba", desk);

    Layers L;
    Cells C;
    if (read_layers(layers_path, &L) != 0) die("cannot read layers.txt");
    if (read_cells(cells_path, &C) != 0) die("cannot read cells.txt");
    if (L.w != C.w || L.h != C.h) die("layers.txt and cells.txt disagree on width/height");
    if (x < 0 || y < 0 || x >= L.w || y >= L.h) die("x,y outside the desk");

    int pair0 = (layer < 2) ? 0 : 2;
    int other = pair0 + (layer == pair0 ? 1 : 0);
    /* other is the sibling layer in the floor pair or the wall pair. */
    if (layer == pair0) other = pair0 + 1;
    else other = pair0;
    int sibling = L.id[other][y * L.w + x];
    if (!is_normal(sibling)) {
        fprintf(stderr,
                "tsots_place_tile: (%d,%d) layer %d is autotile id %d.\n"
                "Atlas left unchanged so that painted cell is not flattened.\n",
                x, y, other, sibling);
        return 2;
    }

    L.id[layer][y * L.w + x] = id;
    int stack[2] = { L.id[pair0][y * L.w + x], L.id[pair0 + 1][y * L.w + x] };
    unsigned char tile[TILE_PX * TILE_PX * 4];
    if (stack[0] || stack[1]) {
        if (!registry || !assets || !slug) die("--registry, --assets, and --slug are required");
        paint_ids(tile, stack, 2, registry, assets, slug);
    } else {
        memset(tile, 0, sizeof tile);
    }

    int nbytes = 0;
    unsigned char *atlas = load_rgba(rgba_path, C.cols, C.ntiles > 0 ? C.ntiles : 1, &nbytes);
    int slot = assign_slot(&atlas, &nbytes, &C, tile);
    int i = y * C.w + x;
    if (layer < 2) C.floor[i] = slot;
    else C.wall[i] = slot;

    if (write_layers(layers_path, &L) != 0) die("write layers.txt failed");
    if (write_cells(cells_path, &C) != 0) die("write cells.txt failed");
    if (save_rgba(rgba_path, atlas, nbytes) != 0) die("write cells.rgba failed");
    bump_generation(desk);
    printf("placed %d %d layer %d id %d floor=%d wall=%d atlas_tiles=%d\n",
           x, y, layer, id, C.floor[i], C.wall[i], C.ntiles);
    printf("key 7 keeps the old atlas until this desk is left and opened again\n");
    free(atlas);
    return 0;
}

static int self_test(void) {
    const char *dir = "/tmp/tsots-place-self";
    if (mkdir(dir, 0755) != 0 && errno != EEXIST) die("mkdir self-test");
    char reg[256], assets[256], sheetdir[256], desk[256];
    snprintf(assets, sizeof assets, "%s/assets", dir);
    snprintf(sheetdir, sizeof sheetdir, "%s/rmmv-www-img/tilesets", assets);
    snprintf(desk, sizeof desk, "%s/desk", dir);
    snprintf(reg, sizeof reg, "%s/reg.pdl", dir);
    char cmd[512];
    snprintf(cmd, sizeof cmd, "mkdir -p '%s' '%s'", sheetdir, desk);
    if (system(cmd) != 0) die("mkdir sheets");

    /* 48x48 sheet: tile 0 is red, so id 1 (B, column 1) is green. */
    Image sheet;
    sheet.w = 96;
    sheet.h = 48;
    sheet.px = xmalloc(96 * 48 * 4);
    memset(sheet.px, 0, 96 * 48 * 4);
    for (int y = 0; y < 48; y++) {
        for (int x = 48; x < 96; x++) {
            unsigned char *p = sheet.px + (y * 96 + x) * 4;
            p[0] = 0; p[1] = 200; p[2] = 0; p[3] = 255;
        }
    }
    char sheet_path[512];
    snprintf(sheet_path, sizeof sheet_path, "%s/Test_B.png", sheetdir);
    if (save_png(sheet_path, &sheet) != 0) die("write test sheet");
    free(sheet.px);

    FILE *fp = fopen(reg, "w");
    if (!fp) die("write registry");
    fprintf(fp, "TILESET | sheet_root | rmmv-www-img/tilesets\n");
    fprintf(fp, "TILESET | test.b | Test_B.png\n");
    fclose(fp);

    char path[512];
    snprintf(path, sizeof path, "%s/layers.txt", desk);
    fp = fopen(path, "w");
    fprintf(fp, "width=2\nheight=1\nlayers=6\n");
    for (int layer = 0; layer < 6; layer++) fprintf(fp, "layer %d\n0 0\n", layer);
    fclose(fp);
    snprintf(path, sizeof path, "%s/cells.txt", desk);
    fp = fopen(path, "w");
    fprintf(fp, "width=2\nheight=1\ntile_px=24\natlas_cols=16\natlas_tiles=1\ncells\n0,0 0,0\n");
    fclose(fp);
    snprintf(path, sizeof path, "%s/cells.rgba", desk);
    fp = fopen(path, "wb");
    size_t z = (size_t)16 * 24 * 24 * 4;
    unsigned char *zero = xmalloc(z);
    memset(zero, 0, z);
    fwrite(zero, 1, z, fp);
    fclose(fp);
    free(zero);

    if (place(desk, 0, 0, 0, 1, reg, assets, "test") != 0) die("place green failed");
    if (place(desk, 1, 0, 0, 1, reg, assets, "test") != 0) die("reuse failed");

    Cells C;
    snprintf(path, sizeof path, "%s/cells.txt", desk);
    if (read_cells(path, &C) != 0) die("reread cells");
    if (C.floor[0] != 1 || C.floor[1] != 1 || C.ntiles != 2) {
        fprintf(stderr, "self-test: floor %d %d tiles %d\n", C.floor[0], C.floor[1], C.ntiles);
        return 1;
    }
    snprintf(path, sizeof path, "%s/cells.rgba", desk);
    fp = fopen(path, "rb");
    unsigned char *atlas = xmalloc(z);
    if (fread(atlas, 1, z, fp) != z) die("short rgba");
    fclose(fp);
    /* Slot 1 is column 1. First pixel of that tile must be the green sample. */
    unsigned char *px = atlas + (1 * TILE_PX) * 4;
    if (px[0] != 0 || px[1] != 200 || px[2] != 0 || px[3] != 255) {
        fprintf(stderr, "self-test pixel %u %u %u %u\n", px[0], px[1], px[2], px[3]);
        return 1;
    }
    free(atlas);

    if (place(desk, 0, 0, 0, 0, reg, assets, "test") != 0) die("clear failed");
    snprintf(path, sizeof path, "%s/cells.txt", desk);
    if (read_cells(path, &C) != 0) die("reread after clear");
    if (C.floor[0] != 0) die("clear did not return slot 0");

    if (place(desk, 0, 0, 0, 2816, reg, assets, "test") == 0)
        die("autotile id must be refused");

    printf("self-test ok\n");
    return 0;
}

int main(int argc, char **argv) {
    const char *desk = NULL, *registry = NULL, *assets = NULL, *slug = NULL;
    int x = -1, y = -1, layer = -1, id = -1;
    int do_test = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--self-test") == 0) do_test = 1;
        else if (strcmp(argv[i], "--desk") == 0 && i + 1 < argc) desk = argv[++i];
        else if (strcmp(argv[i], "--x") == 0 && i + 1 < argc) x = atoi(argv[++i]);
        else if (strcmp(argv[i], "--y") == 0 && i + 1 < argc) y = atoi(argv[++i]);
        else if (strcmp(argv[i], "--layer") == 0 && i + 1 < argc) layer = atoi(argv[++i]);
        else if (strcmp(argv[i], "--id") == 0 && i + 1 < argc) id = atoi(argv[++i]);
        else if (strcmp(argv[i], "--registry") == 0 && i + 1 < argc) registry = argv[++i];
        else if (strcmp(argv[i], "--assets") == 0 && i + 1 < argc) assets = argv[++i];
        else if (strcmp(argv[i], "--slug") == 0 && i + 1 < argc) slug = argv[++i];
        else {
            fprintf(stderr, "unknown arg %s\n", argv[i]);
            return 1;
        }
    }
    if (do_test) return self_test();
    if (!desk || x < 0 || y < 0 || layer < 0 || id < 0) {
        fprintf(stderr,
                "Usage: tsots_place_tile.+x --desk DIR --x X --y Y --layer 0..3 --id ID \\\n"
                "           --slug SLUG --registry REG.pdl --assets ASSETS\n"
                "       tsots_place_tile.+x --self-test\n");
        return 1;
    }
    return place(desk, x, y, layer, id, registry, assets, slug);
}
