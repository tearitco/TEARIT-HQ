/* bv_2d_piece_test - parity gate for entity pieces in the 2D view (entity frames spec 2026-10-27, section 5): a piece folder that the 3D engine draws must also change the 2D frame at its footprint, by the
 * same rules (desk= empty / * / equal to world_01's desk_id; model.txt top-down; tile_model.txt + tiles.rgba top tiles), and must change nothing outside it. Builds a scratch project in a directory given as argv[1],
 * runs the 2D renderer (env BV2D_BIN, default ops/+x/bv_render_2d.+x) with that directory as cwd, reads pieces/display/rgb_frame_2d.raw (640x480 RGBA) and view_map.txt (ox oy cell).
 * Negative test: run it with BV2D_BIN = a renderer built before the piece code; the piece cases must FAIL.   Exit 0 = every check held, 1 = a check failed, 2 = setup/run problem. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <png.h>
#define W 640
#define H 480
static char dir[600]; static int bad = 0; static unsigned char base[W * H * 4], cur[W * H * 4]; static int ox, oy, cell;
static void wr(const char *rel, const char *txt) { char p[800]; snprintf(p, sizeof p, "%s/%s", dir, rel); FILE *f = fopen(p, "w"); if (!f) { perror(p); exit(2); } fputs(txt, f); fclose(f); }
static void md(const char *rel) { char p[800]; snprintf(p, sizeof p, "%s/%s", dir, rel); mkdir(p, 0755); }
static void base_project(void) {
    char b[512]; md(""); md("pieces"); md("pieces/system"); md("pieces/display"); md("pieces/world_01"); md("pieces/veh_1"); md("pieces/xelector_01");
    snprintf(b, sizeof b, "focused_project_root=%s\nrender_mode=0\nselector_x=3\nselector_y=3\n", dir); wr("pieces/system/bv_state.txt", b);
    wr("pieces/world_01/state.txt", "desk_id=d1\nmap_id=m\n"); wr("pieces/xelector_01/state.txt", "pos_x=2\npos_y=2\n");
    char rows[200] = ""; for (int i = 0; i < 8; i++) strcat(rows, "................\n"); wr("pieces/system/board.txt", rows); }
static void piece(const char *desk, int tiled) {
    char s[300]; snprintf(s, sizeof s, "entity_type=vehicle\n%s%spos_x=2\npos_y=2\npos_z=0\nsize_x=2\nsize_y=2\nsize_h=1\n", desk[0] ? "desk=" : "", desk[0] ? desk : "");
    if (desk[0]) { char t[320]; snprintf(t, sizeof t, "entity_type=vehicle\ndesk=%s\npos_x=2\npos_y=2\npos_z=0\nsize_x=2\nsize_y=2\nsize_h=1\n", desk); wr("pieces/veh_1/state.txt", t); } else wr("pieces/veh_1/state.txt", s);
    wr("pieces/veh_1/model.txt", "dim 2 1 2\n0 0 0 255 0 0\n1 0 0 255 0 0\n0 0 1 255 0 0\n1 0 1 255 0 0\n");
    if (tiled) { wr("pieces/veh_1/tile_model.txt", "dim 2 1 2\n0 0 0 0 0\n1 0 0 0 0\n0 0 1 0 0\n1 0 1 0 0\n");
        static unsigned char tex[24 * 24 * 4]; for (int y = 0; y < 24; y++) for (int x = 0; x < 24; x++) { unsigned char *p = tex + (y * 24 + x) * 4; int l = x < 12; p[0] = 0; p[1] = l ? 0 : 200; p[2] = l ? 200 : 0; p[3] = 255; }
        char p[800]; snprintf(p, sizeof p, "%s/pieces/veh_1/tiles.rgba", dir); FILE *f = fopen(p, "wb"); fwrite(tex, 1, sizeof tex, f); fclose(f); }
    else { char p[800]; snprintf(p, sizeof p, "%s/pieces/veh_1/tile_model.txt", dir); unlink(p); snprintf(p, sizeof p, "%s/pieces/veh_1/tiles.rgba", dir); unlink(p); } }
static void nopiece(void) { char p[800]; const char *f[] = {"state.txt", "model.txt", "tile_model.txt", "tiles.rgba"}; for (int i = 0; i < 4; i++) { snprintf(p, sizeof p, "%s/pieces/veh_1/%s", dir, f[i]); unlink(p); } }
static int render(unsigned char *out) {
    const char *bin = getenv("BV2D_BIN"); if (!bin) bin = "ops/+x/bv_render_2d.+x"; char abs[800]; if (bin[0] != '/') { char cw[512]; if (!getcwd(cw, sizeof cw)) return 2; snprintf(abs, sizeof abs, "%s/%s", cw, bin); } else snprintf(abs, sizeof abs, "%s", bin);
    char cmd[1700]; snprintf(cmd, sizeof cmd, "cd '%s' && timeout 30 '%s' >/dev/null 2>&1", dir, abs); if (system(cmd) != 0) return 2;
    char p[800]; snprintf(p, sizeof p, "%s/pieces/display/rgb_frame_2d.raw", dir); FILE *f = fopen(p, "rb"); if (!f) return 2; size_t n = fread(out, 1, W * H * 4, f); fclose(f); if (n != (size_t)W * H * 4) return 2;
    snprintf(p, sizeof p, "%s/pieces/display/view_map.txt", dir); f = fopen(p, "r"); if (!f) return 2; char l[64]; ox = oy = 0; cell = 80; while (fgets(l, sizeof l, f)) { if (!strncmp(l, "ox=", 3)) ox = atoi(l + 3); if (!strncmp(l, "oy=", 3)) oy = atoi(l + 3); if (!strncmp(l, "cell=", 5)) cell = atoi(l + 5); } fclose(f); return 0; }
/* the desk-picture branch (what rpg-space uses): maps/m/d1/map.png (8 x 6 tiles of 24 px, solid grey) + map.txt (first line = 8 columns) */
static void map_branch(void) {
    char p[800]; md("pieces/system/maps"); md("pieces/system/maps/m"); md("pieces/system/maps/m/d1"); snprintf(p, sizeof p, "%s/pieces/system/maps/m/d1/map.txt", dir); FILE *f = fopen(p, "w"); fputs("........\n", f); fclose(f);
    snprintf(p, sizeof p, "%s/pieces/system/maps/m/d1/map.png", dir); f = fopen(p, "wb"); png_structp pg = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL); png_infop in = png_create_info_struct(pg); png_init_io(pg, f);
    png_set_IHDR(pg, in, 192, 144, 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT); png_write_info(pg, in);
    static unsigned char row[192 * 4]; for (int x = 0; x < 192; x++) { row[x * 4] = row[x * 4 + 1] = row[x * 4 + 2] = 90; row[x * 4 + 3] = 255; } for (int y = 0; y < 144; y++) png_write_row(pg, row);
    png_write_end(pg, NULL); png_destroy_write_struct(&pg, &in); fclose(f); }
static void zoom(int z) { char b[600]; snprintf(b, sizeof b, "focused_project_root=%s\nrender_mode=0\nselector_x=3\nselector_y=3\nview_2d_zoom=%d\n", dir, z); wr("pieces/system/bv_state.txt", b); }
static void piece_at(int x, int y) { char t[320]; snprintf(t, sizeof t, "entity_type=vehicle\ndesk=d1\npos_x=%d\npos_y=%d\npos_z=0\nsize_x=2\nsize_y=2\nsize_h=1\n", x, y); wr("pieces/veh_1/state.txt", t);
    wr("pieces/veh_1/model.txt", "dim 2 1 2\n0 0 0 255 0 0\n1 0 0 255 0 0\n0 0 1 255 0 0\n1 0 1 255 0 0\n"); }
static int reds(const unsigned char *f) { int n = 0; for (int i = 0; i < W * H; i++) { const unsigned char *p = f + (size_t)i * 4; if (p[0] > 200 && p[1] < 60 && p[2] < 60) n++; } return n; }
static void check(int ok, const char *what) { printf("%s: %s\n", ok ? "ok  " : "FAIL", what); if (!ok) bad = 1; }
static void count(int *red, int *blue, int *green, int *outside_diff) {
    int x0 = (2 - ox) * cell, y0 = (2 - oy) * cell, x1 = x0 + 2 * cell, y1 = y0 + 2 * cell; *red = *blue = *green = *outside_diff = 0;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) { const unsigned char *p = cur + ((size_t)y * W + x) * 4; int in = x >= x0 && x < x1 && y >= y0 && y < y1;
        if (in) { if (p[0] > 200 && p[1] < 60 && p[2] < 60) (*red)++; else if (p[2] > 150 && p[0] < 60 && p[1] < 60) (*blue)++; else if (p[1] > 150 && p[0] < 60 && p[2] < 60) (*green)++; }
        else if (memcmp(p, base + ((size_t)y * W + x) * 4, 4)) (*outside_diff)++; } }
int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <scratch dir>\n", argv[0]); return 2; } snprintf(dir, sizeof dir, "%s", argv[1]); int r, b, g, od;
    base_project(); nopiece(); if (render(base)) { fprintf(stderr, "renderer run failed\n"); return 2; } memcpy(cur, base, sizeof cur); count(&r, &b, &g, &od); check(r == 0, "no piece: no red in the footprint (baseline)");
    piece("d1", 0); if (render(cur)) return 2; count(&r, &b, &g, &od); printf("voxel piece desk=d1: red %d, changed outside %d\n", r, od);
    check(r > 15000, "a voxel piece of the shown desk is drawn over its footprint (red top)"); check(od == 0, "nothing outside the footprint changed");
    piece("other", 0); if (render(cur)) return 2; count(&r, &b, &g, &od); check(r == 0, "a piece of another desk is not drawn (same rule as the 3D engine)");
    piece("*", 0); if (render(cur)) return 2; count(&r, &b, &g, &od); check(r > 15000, "desk=* is drawn on every desk");
    piece("*", 1); if (render(cur)) return 2; count(&r, &b, &g, &od); printf("tile-faced piece: red %d blue %d green %d\n", r, b, g);
    check(b > 3000 && g > 3000 && r == 0, "a tile-faced piece shows its tile picture (both halves) and not the voxel colour"); check(od == 0, "tile-faced: nothing outside the footprint changed");
    /* the desk-picture branch: a piece inside the page is drawn; a piece outside the page shows only at zoom -1 (the whole scene); zoom -1 with no piece must not break the frame */
    nopiece(); map_branch(); zoom(0); if (render(base)) return 2; check(reds(base) == 0, "desk picture, no piece: no red");
    piece_at(2, 2); zoom(0); if (render(cur)) return 2; r = reds(cur); printf("desk picture, piece on the page: red %d\n", r); check(r > 5000, "desk-picture branch: a piece on the page is drawn");
    piece_at(12, 2); zoom(0); if (render(cur)) return 2; check(reds(cur) == 0, "a piece outside the page is not visible at fit-page zoom 0");
    zoom(-1); if (render(cur)) return 2; r = reds(cur); printf("desk picture, piece off the page, zoom -1: red %d\n", r); check(r > 300, "zoom -1 (whole scene) shows the piece that lies outside the page");
    nopiece(); zoom(-1); if (render(cur)) return 2; zoom(0); if (render(base)) return 2; check(memcmp(cur, base, sizeof cur) == 0, "zoom -1 with no piece looks exactly like fit-page (no crash, no change)");
    return bad; }
