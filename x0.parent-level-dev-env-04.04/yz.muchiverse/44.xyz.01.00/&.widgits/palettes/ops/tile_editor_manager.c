/* tile_editor_manager — Tiled-like 16x16 stamp canvas. Reads sprite.csv
 * thumbs from palettes/sprites/tiled (or first my-library). No renderer C. */
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#define MW 16
#define MH 16
#define CELL 24
#define CW (MW * CELL)
#define CH (MH * CELL)
#define MAXPAL 64
#define RES 48

static char house[PATH_MAX], pkg[PATH_MAX];
static int map[MH][MW];
static int cx, cy, sel, last_seq;
static unsigned char pal[MAXPAL][RES * RES * 4];
static int npal;
static unsigned char canvas[CW * CH * 4];

static void load_map(void) {
    char p[PATH_MAX];
    snprintf(p, sizeof(p), "%s/state/tile_editor_map.csv", pkg);
    FILE *f = fopen(p, "r");
    if (!f) { memset(map, 0, sizeof(map)); return; }
    for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++) {
            int v = 0;
            if (fscanf(f, "%d%*[, \n]", &v) == 1) map[y][x] = v;
        }
    fclose(f);
}
static void save_map(void) {
    char p[PATH_MAX], tmp[PATH_MAX];
    snprintf(p, sizeof(p), "%s/state/tile_editor_map.csv", pkg);
    snprintf(tmp, sizeof(tmp), "%s.tmp", p);
    FILE *f = fopen(tmp, "w");
    if (!f) return;
    for (int y = 0; y < MH; y++) {
        for (int x = 0; x < MW; x++)
            fprintf(f, "%s%d", x ? "," : "", map[y][x]);
        fputc('\n', f);
    }
    fclose(f);
    rename(tmp, p);
}

static int load_sprite_csv(const char *path, unsigned char *dst) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[128];
    int n = 0, skip = 1;
    while (fgets(line, sizeof(line), f) && n < RES * RES) {
        if (line[0] == '#' || !strncmp(line, "r,g,b,a", 7)) continue;
        int r, g, b, a;
        if (sscanf(line, "%d,%d,%d,%d", &r, &g, &b, &a) != 4) continue;
        dst[n * 4] = (unsigned char)r;
        dst[n * 4 + 1] = (unsigned char)g;
        dst[n * 4 + 2] = (unsigned char)b;
        dst[n * 4 + 3] = (unsigned char)a;
        n++;
    }
    fclose(f);
    return n == RES * RES;
}

static void load_palette(void) {
    npal = 0;
    char root[PATH_MAX];
    snprintf(root, sizeof(root), "%s/sprites/tiled/Tilesets/Grass", pkg);
    struct stat st;
    if (stat(root, &st) != 0)
        snprintf(root, sizeof(root), "%s/sprites/ohr", pkg);
    for (int i = 1; i <= MAXPAL; i++) {
        char csv[PATH_MAX];
        snprintf(csv, sizeof(csv), "%s/%03d/sprite.csv", root, i);
        if (!load_sprite_csv(csv, pal[npal])) break;
        npal++;
    }
    if (npal < 1) {
        memset(pal[0], 80, sizeof(pal[0]));
        npal = 1;
    }
}

static void blit_cell(int mx, int my, int tid, int cursor) {
    if (tid < 0) tid = 0;
    if (tid >= npal) tid = npal - 1;
    unsigned char *src = pal[tid];
    for (int y = 0; y < CELL; y++)
        for (int x = 0; x < CELL; x++) {
            int sx = x * RES / CELL, sy = y * RES / CELL;
            unsigned char *p = &src[((size_t)sy * RES + sx) * 4];
            int dx = mx * CELL + x, dy = my * CELL + y;
            unsigned char *d = &canvas[((size_t)dy * CW + dx) * 4];
            d[0] = p[0]; d[1] = p[1]; d[2] = p[2]; d[3] = 255;
            if (cursor && (x == 0 || y == 0 || x == CELL - 1 || y == CELL - 1)) {
                d[0] = 255; d[1] = 180; d[2] = 40;
            }
        }
}

static void compose(void) {
    for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++)
            blit_cell(x, y, map[y][x], x == cx && y == cy);
    char raw[PATH_MAX], tmp[PATH_MAX], rec[PATH_MAX];
    snprintf(raw, sizeof(raw), "%s/state/tile_editor_canvas.raw", pkg);
    snprintf(tmp, sizeof(tmp), "%s/state/tile_editor_canvas.raw.tmp", pkg);
    snprintf(rec, sizeof(rec), "%s/state/tile_editor_canvas.receipt.txt", pkg);
    FILE *f = fopen(tmp, "wb");
    if (f) { fwrite(canvas, 1, sizeof(canvas), f); fclose(f); rename(tmp, raw); }
    FILE *r = fopen(rec, "w");
    if (r) { fprintf(r, "overlay_w=%d\noverlay_h=%d\n", CW, CH); fclose(r); }
}

static void write_ui(void) {
    char dst[PATH_MAX], tmp[PATH_MAX];
    snprintf(dst, sizeof(dst), "%s/state/tile_editor_ui.txt", pkg);
    snprintf(tmp, sizeof(tmp), "%s.tmp", dst);
    FILE *f = fopen(tmp, "w");
    if (!f) return;
    fprintf(f, "canvas_raw=%s/state/tile_editor_canvas.raw\n", pkg);
    fprintf(f, "cell=%d,%d\nsel=%d\nn_pal=%d\nempty=0\n", cx, cy, sel, npal);
    fprintf(f, "gutter=cell %d,%d  tile %d / %d  Plot stamps  Fill fills map\n",
            cx, cy, sel, npal);
    fclose(f);
    rename(tmp, dst);
}

static void flood(int x, int y, int from, int to) {
    if (x < 0 || y < 0 || x >= MW || y >= MH) return;
    if (map[y][x] != from || from == to) return;
    map[y][x] = to;
    flood(x + 1, y, from, to);
    flood(x - 1, y, from, to);
    flood(x, y + 1, from, to);
    flood(x, y - 1, from, to);
}

static void apply_cmd(const char *cmd) {
    if (!strcmp(cmd, "CUR:L") && cx > 0) cx--;
    else if (!strcmp(cmd, "CUR:R") && cx < MW - 1) cx++;
    else if (!strcmp(cmd, "CUR:U") && cy > 0) cy--;
    else if (!strcmp(cmd, "CUR:D") && cy < MH - 1) cy++;
    else if (!strcmp(cmd, "PAL:+")) sel = (sel + 1) % npal;
    else if (!strcmp(cmd, "PAL:-")) sel = (sel + npal - 1) % npal;
    else if (!strcmp(cmd, "PLOT")) { map[cy][cx] = sel; save_map(); }
    else if (!strcmp(cmd, "FILL")) { flood(cx, cy, map[cy][cx], sel); save_map(); }
    else if (!strcmp(cmd, "ERASE")) { map[cy][cx] = 0; save_map(); }
}

int main(int argc, char **argv) {
    if (argc < 3) return 1;
    snprintf(house, sizeof(house), "%s", argv[1]);
    snprintf(pkg, sizeof(pkg), "%s", argv[2]);
    char d[PATH_MAX]; snprintf(d, sizeof(d), "%s/state", pkg);
    char mk[PATH_MAX]; snprintf(mk, sizeof(mk), "mkdir -p '%s'", d);
    int ign = system(mk); (void)ign;
    load_palette();
    load_map();
    compose();
    write_ui();
    char act[PATH_MAX];
    snprintf(act, sizeof(act), "%s/state/tile_editor_action.txt", pkg);
    for (;;) {
        FILE *f = fopen(act, "r");
        if (f) {
            char line[256]; int seq = 0; char cmd[128] = "";
            while (fgets(line, sizeof(line), f)) {
                if (!strncmp(line, "seq=", 4)) seq = atoi(line + 4);
                if (!strncmp(line, "cmd=", 4)) {
                    snprintf(cmd, sizeof(cmd), "%s", line + 4);
                    char *nl = strpbrk(cmd, "\r\n"); if (nl) *nl = 0;
                }
            }
            fclose(f);
            if (seq != last_seq && cmd[0]) {
                last_seq = seq;
                apply_cmd(cmd);
                compose();
                write_ui();
            }
        }
        usleep(100000);
    }
    return 0;
}
