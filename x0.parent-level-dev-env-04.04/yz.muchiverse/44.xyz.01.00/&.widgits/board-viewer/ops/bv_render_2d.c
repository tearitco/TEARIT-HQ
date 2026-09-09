/* bv_render_2d.c - the flat top-down tile view for pc-hq's 2D
 * (render_mode == 0) board. PCHQ-2D-TILE-VIEW.md P1.
 *
 * A plain painter, NO raymarch, NO chrome, NO legend/status text:
 *   - each board cell filled with its terrain-legend colour,
 *   - a manually drawn "matrix" grid on every cell boundary,
 *   - the xelector as an inset highlight box,
 *   - entities as solid colour squares.
 * Emoji glyphs and real palette tilesets are P2 (view_2d_style, the
 * `` ` `` toggle) - this pass is colour-only so the pipeline (bv_dispatch
 * routing render_mode==0 here, projector publishing rgb_frame_2d.raw,
 * no bv_compose_frame chrome) can be verified end to end.
 *
 * Reads (cwd = the board-viewer session dir, or $PRISC_PROJECT_ROOT):
 *   pieces/system/bv_state.txt        focused_project_root, selector_x/y, current_z
 *   pieces/system/house_root.txt      -> house root (for desk_grid.pdl)
 *   <focused>/pieces/system/board.txt            flat glyph grid (one row per line)
 *   <focused>/<z_base><current_z>.txt            if a z-manifest exists
 *   <focused>/pieces/system/terrain_legend.txt   glyph|height|r|g|b|asset|name
 *   <focused>/pieces/system/entities.txt         pos_x=,pos_y=,hex= (or r/g/b)
 *   <house>/#.desktop/desk_grid.pdl              GRID | cell_px | N   (default 80)
 * Writes:
 *   pieces/display/rgb_frame_2d.raw              RGBA, w*h*4
 *   pieces/display/rgb_frame_2d.receipt.txt      frame_w=%d\nframe_h=%d\n
 *
 * Self-contained, no shared headers (board-viewer house convention).
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define MAX_LINE     1024
#define PATH_BUF     4096
#define MAX_DIM      256          /* board cells per side, hard ceiling */
#define MAX_FRAME_PX 2400         /* per side; CELL shrinks to fit P1's whole-board view */
#define MAX_LEGEND   64
#define MAX_ENT      256

static char project_root[PATH_BUF] = ".";
static char house_root[PATH_BUF]   = "";
static char focused_root[PATH_BUF] = "";

static FILE *host_fopen(const char *p, const char *m) { return fopen(p, m); }

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) { snprintf(project_root, sizeof(project_root), "%s", env); return; }
    if (!getcwd(project_root, sizeof(project_root))) snprintf(project_root, sizeof(project_root), ".");
}

static void read_kv_str(const char *path, const char *key, char *out, size_t osz) {
    out[0] = '\0';
    FILE *f = host_fopen(path, "r");
    if (!f) return;
    size_t kl = strlen(key);
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, kl) == 0 && line[kl] == '=') {
            char *v = line + kl + 1;
            v[strcspn(v, "\r\n")] = '\0';
            snprintf(out, osz, "%s", v);
            break;
        }
    }
    fclose(f);
}
static int read_kv_int(const char *path, const char *key, int def) {
    char b[64]; read_kv_str(path, key, b, sizeof(b));
    return b[0] ? atoi(b) : def;
}

static void load_house_root(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/house_root.txt", project_root);
    FILE *f = host_fopen(path, "r");
    if (!f) return;
    if (fgets(house_root, sizeof(house_root), f)) {
        if ((unsigned char)house_root[0] == 0xEF && (unsigned char)house_root[1] == 0xBB &&
            (unsigned char)house_root[2] == 0xBF)
            memmove(house_root, house_root + 3, strlen(house_root + 3) + 1);
        house_root[strcspn(house_root, "\r\n")] = '\0';
    }
    fclose(f);
}

/* focused_project_root is usually absolute; if relative, resolve vs house_root */
static void resolve_focused(const char *raw) {
    if (!raw || !raw[0]) { focused_root[0] = '\0'; return; }
    if (raw[0] == '/' || (raw[0] && raw[1] == ':')) { snprintf(focused_root, sizeof(focused_root), "%s", raw); return; }
    if (house_root[0]) snprintf(focused_root, sizeof(focused_root), "%s/%s", house_root, raw);
    else snprintf(focused_root, sizeof(focused_root), "%s", raw);
}

/* ---- terrain legend: glyph -> rgb ---- */
typedef struct { char glyph; unsigned char r, g, b; } Leg;
static Leg  g_leg[MAX_LEGEND];
static int  g_nleg = 0;
static void load_legend(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/terrain_legend.txt", focused_root);
    FILE *f = host_fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (g_nleg < MAX_LEGEND && fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (!line[0] || (line[0] == '#' && line[1] != '|')) continue;   /* comment, not the '#' wall glyph */
        char *sv = NULL;
        char *g  = strtok_r(line, "|", &sv);
        strtok_r(NULL, "|", &sv);                    /* height (unused in 2D) */
        char *rt = strtok_r(NULL, "|", &sv);
        char *gt = strtok_r(NULL, "|", &sv);
        char *bt = strtok_r(NULL, "|", &sv);
        if (!g || !g[0] || !rt || !gt || !bt) continue;
        g_leg[g_nleg].glyph = g[0];
        g_leg[g_nleg].r = (unsigned char)atoi(rt);
        g_leg[g_nleg].g = (unsigned char)atoi(gt);
        g_leg[g_nleg].b = (unsigned char)atoi(bt);
        g_nleg++;
    }
    fclose(f);
}
static int legend_rgb(char glyph, unsigned char *r, unsigned char *g, unsigned char *b) {
    for (int i = 0; i < g_nleg; i++)
        if (g_leg[i].glyph == glyph) { *r = g_leg[i].r; *g = g_leg[i].g; *b = g_leg[i].b; return 1; }
    return 0;
}

/* ---- entities: pos + colour ---- */
typedef struct { int x, y; unsigned char r, g, b; } Ent;
static Ent g_ent[MAX_ENT];
static int g_nent = 0;
static void hex_to_rgb(const char *hx, unsigned char *r, unsigned char *g, unsigned char *b) {
    if (hx[0] == '#') hx++;
    unsigned v = (unsigned)strtoul(hx, NULL, 16);
    *r = (v >> 16) & 0xFF; *g = (v >> 8) & 0xFF; *b = v & 0xFF;
}
static void load_entities(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/entities.txt", focused_root);
    FILE *f = host_fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    Ent cur; int have = 0;
    memset(&cur, 0, sizeof(cur));
    while (g_nent < MAX_ENT && fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (!line[0]) {                              /* blank line = record separator */
            if (have) g_ent[g_nent++] = cur;
            memset(&cur, 0, sizeof(cur)); have = 0; continue;
        }
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0'; char *k = line, *v = eq + 1;
        if      (!strcmp(k, "pos_x")) { cur.x = atoi(v); have = 1; }
        else if (!strcmp(k, "pos_y")) { cur.y = atoi(v); have = 1; }
        else if (!strcmp(k, "hex") || !strcmp(k, "color") || !strcmp(k, "colour")) {
            if (v[0]) { hex_to_rgb(v, &cur.r, &cur.g, &cur.b); have = 1; }
        }
    }
    if (have) g_ent[g_nent++] = cur;
    fclose(f);
}

/* ---- board glyphs (one z-slice) ---- */
static char g_board[MAX_DIM][MAX_DIM];
static int  g_bw = 0, g_bh = 0;
static void load_board(int current_z) {
    /* z-manifest? board_manifest.txt: "z_base=<prefix>" + "z_count=N" */
    char man[PATH_BUF], zbase[256] = "";
    int zcount = 0;
    snprintf(man, sizeof(man), "%s/pieces/system/board_manifest.txt", focused_root);
    read_kv_str(man, "z_base", zbase, sizeof(zbase));
    zcount = read_kv_int(man, "z_count", 0);

    char path[PATH_BUF];
    if (zbase[0] && zcount > 0) {
        int z = current_z; if (z < 0) z = 0; if (z >= zcount) z = zcount - 1;
        snprintf(path, sizeof(path), "%s/%s%d.txt", focused_root, zbase, z);
    } else {
        snprintf(path, sizeof(path), "%s/pieces/system/board.txt", focused_root);
    }
    FILE *f = host_fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    int row = 0;
    while (row < MAX_DIM && fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        int len = (int)strlen(line);
        if (len == 0) continue;
        if (len > MAX_DIM) len = MAX_DIM;
        memcpy(g_board[row], line, len);
        if (len > g_bw) g_bw = len;
        row++;
    }
    if (row > g_bh) g_bh = row;
    fclose(f);
}

static void write_atomic(const char *path, const void *data, size_t len) {
    char tmp[PATH_BUF];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *f = host_fopen(tmp, "wb");
    if (!f) return;
    if (fwrite(data, 1, len, f) != len) { fclose(f); remove(tmp); return; }
    fclose(f);
    remove(path);
    if (rename(tmp, path) != 0) {
        FILE *o = host_fopen(path, "wb");
        if (o) { fwrite(data, 1, len, o); fclose(o); }
        remove(tmp);
    }
}

int main(void) {
    resolve_root();
    load_house_root();

    char st[PATH_BUF];
    snprintf(st, sizeof(st), "%s/pieces/system/bv_state.txt", project_root);
    char fpr[PATH_BUF] = "";
    read_kv_str(st, "focused_project_root", fpr, sizeof(fpr));
    resolve_focused(fpr);
    int sel_x    = read_kv_int(st, "selector_x", -1);
    int sel_y    = read_kv_int(st, "selector_y", -1);
    int cur_z    = read_kv_int(st, "current_z", 0);

    load_legend();
    load_entities();
    load_board(cur_z);

    /* Empty / not-yet-generated board -> still show a grid so `0` isn't blank. */
    int bw = g_bw > 0 ? g_bw : 20;
    int bh = g_bh > 0 ? g_bh : 15;

    /* cell size: house grid, shrunk to keep P1's whole-board frame sane */
    char grid_pdl[PATH_BUF];
    snprintf(grid_pdl, sizeof(grid_pdl), "%s/#.desktop/desk_grid.pdl", house_root);
    int cell = 80;
    { FILE *gf = host_fopen(grid_pdl, "r");
      if (gf) { char l[MAX_LINE];
        while (fgets(l, sizeof(l), gf)) {
            /* "GRID | cell_px | N" */
            char *p = strstr(l, "cell_px");
            if (!p) continue;
            p = strchr(p, '|'); if (!p) continue;
            int v = atoi(p + 1); if (v >= 4 && v <= 512) cell = v;
            break;
        }
        fclose(gf);
      } }
    while ((long)bw * cell > MAX_FRAME_PX || (long)bh * cell > MAX_FRAME_PX) {
        if (cell <= 8) break;
        cell -= 4;
    }

    int W = bw * cell, H = bh * cell;
    if (W < 1) W = 1;
    if (H < 1) H = 1;
    unsigned char *px = calloc((size_t)W * H, 4);
    if (!px) return 1;

    /* --- ground fill --- */
    unsigned char air_r = 24, air_g = 24, air_b = 28;   /* desk-ish dark; NOT sky blue */
    for (int cy = 0; cy < bh; cy++) {
        for (int cx = 0; cx < bw; cx++) {
            unsigned char r = air_r, g = air_g, b = air_b;
            if (cy < g_bh && cx < g_bw) {
                char gl = g_board[cy][cx];
                if (gl && gl != '_' && gl != ' ')
                    if (!legend_rgb(gl, &r, &g, &b)) { r = 90; g = 90; b = 96; }  /* unknown glyph */
            }
            for (int yy = 0; yy < cell; yy++) {
                unsigned char *row = px + ((size_t)(cy * cell + yy) * W + cx * cell) * 4;
                for (int xx = 0; xx < cell; xx++) {
                    row[xx * 4 + 0] = r; row[xx * 4 + 1] = g; row[xx * 4 + 2] = b; row[xx * 4 + 3] = 255;
                }
            }
        }
    }

    /* --- entities: solid square, inner 60% --- */
    for (int i = 0; i < g_nent; i++) {
        int cx = g_ent[i].x, cy = g_ent[i].y;
        if (cx < 0 || cy < 0 || cx >= bw || cy >= bh) continue;
        int m = cell / 5;                    /* inset */
        for (int yy = m; yy < cell - m; yy++) {
            unsigned char *row = px + ((size_t)(cy * cell + yy) * W + cx * cell) * 4;
            for (int xx = m; xx < cell - m; xx++) {
                row[xx * 4 + 0] = g_ent[i].r; row[xx * 4 + 1] = g_ent[i].g;
                row[xx * 4 + 2] = g_ent[i].b; row[xx * 4 + 3] = 255;
            }
        }
    }

    /* --- the manual "matrix" grid: 1px on every cell boundary --- */
    unsigned char gl_r = 60, gl_g = 90, gl_b = 70;      /* faint matrix green-grey */
    for (int cx = 0; cx <= bw; cx++) {
        int x = cx * cell; if (x >= W) x = W - 1;
        for (int y = 0; y < H; y++) {
            unsigned char *pxl = px + ((size_t)y * W + x) * 4;
            pxl[0] = gl_r; pxl[1] = gl_g; pxl[2] = gl_b; pxl[3] = 255;
        }
    }
    for (int cy = 0; cy <= bh; cy++) {
        int y = cy * cell; if (y >= H) y = H - 1;
        unsigned char *rowp = px + (size_t)y * W * 4;
        for (int x = 0; x < W; x++) {
            rowp[x * 4 + 0] = gl_r; rowp[x * 4 + 1] = gl_g; rowp[x * 4 + 2] = gl_b; rowp[x * 4 + 3] = 255;
        }
    }

    /* --- xelector: 2px inset border, bright accent --- */
    if (sel_x >= 0 && sel_y >= 0 && sel_x < bw && sel_y < bh) {
        unsigned char xr = 255, xg = 204, xb = 0;
        int x0 = sel_x * cell, y0 = sel_y * cell;
        for (int t = 1; t <= 2; t++) {
            for (int x = x0 + t; x < x0 + cell - t; x++) {
                for (int yy = 0; yy < 2; yy++) {
                    unsigned char *a = px + ((size_t)(y0 + t + yy) * W + x) * 4;
                    unsigned char *c = px + ((size_t)(y0 + cell - 1 - t - yy) * W + x) * 4;
                    a[0] = xr; a[1] = xg; a[2] = xb; a[3] = 255;
                    c[0] = xr; c[1] = xg; c[2] = xb; c[3] = 255;
                }
            }
            for (int y = y0 + t; y < y0 + cell - t; y++) {
                for (int xx = 0; xx < 2; xx++) {
                    unsigned char *a = px + ((size_t)y * W + x0 + t + xx) * 4;
                    unsigned char *c = px + ((size_t)y * W + x0 + cell - 1 - t - xx) * 4;
                    a[0] = xr; a[1] = xg; a[2] = xb; a[3] = 255;
                    c[0] = xr; c[1] = xg; c[2] = xb; c[3] = 255;
                }
            }
        }
    }

    char out[PATH_BUF], rec[PATH_BUF];
    snprintf(out, sizeof(out), "%s/pieces/display/rgb_frame_2d.raw", project_root);
    snprintf(rec, sizeof(rec), "%s/pieces/display/rgb_frame_2d.receipt.txt", project_root);
    write_atomic(out, px, (size_t)W * H * 4);
    { char rb[128]; int n = snprintf(rb, sizeof(rb), "frame_w=%d\nframe_h=%d\n", W, H);
      if (n > 0) write_atomic(rec, rb, (size_t)n); }
    free(px);
    return 0;
}
