/* rpg_edit.c — town for the edit board. One outdoor page (start) and one
 * cabin. RPG Maker tiles. Eight sprites walk. Ava uses the door and the
 * board page follows her. No camera math. No People4, Actor2, or Actor3.
 *   rpg_edit.+x <house> <app>     resident
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <time.h>
#include <sys/stat.h>
#define STB_IMAGE_IMPLEMENTATION
#include "../../../&.widgits/_shared-lib/stb_image.h"
#include "../../../&.widgits/_shared-lib/csv_frames.c"      /* sprite.csv -> 16x24 3D walk frames (shared; also usable by any pet board) */

#define XT 24
#define S48 48
#define COLS 16
#define ROWS 12
#define MAXSLOT 64

static char HOUSE[PATH_MAX], APP[PATH_MAX], ASSETS[PATH_MAX], PCHQ[PATH_MAX];
static unsigned char slotpx[MAXSLOT][XT * XT * 4];
static int nslot = 1;

typedef struct { unsigned char *px; int w, h; } Sheet;
static Sheet load_rel(const char *rel) {
    Sheet s = {0}; char p[PATH_MAX];
    snprintf(p, sizeof p, "%s/%s", ASSETS, rel);
    int ch; s.px = stbi_load(p, &s.w, &s.h, &ch, 4); return s;
}
static void tile24(Sheet *s, int sx, int sy, unsigned char *out) {
    for (int y = 0; y < XT; y++) for (int x = 0; x < XT; x++) {
        int acc[4] = {0}, wsum = 0;
        for (int dy = 0; dy < 2; dy++) for (int dx = 0; dx < 2; dx++) {
            int px = sx + x * 2 + dx, py = sy + y * 2 + dy;
            if (!s || !s->px || px >= s->w || py >= s->h) continue;
            unsigned char *a = s->px + ((size_t)py * s->w + px) * 4;
            acc[0] += a[0] * a[3]; acc[1] += a[1] * a[3]; acc[2] += a[2] * a[3]; acc[3] += a[3]; wsum += a[3];
        }
        unsigned char *o = out + (y * XT + x) * 4;
        if (wsum) { o[0] = acc[0] / wsum; o[1] = acc[1] / wsum; o[2] = acc[2] / wsum; o[3] = acc[3] / 4; }
        else o[0] = o[1] = o[2] = o[3] = 0;
    }
}
static void over24(unsigned char *dst, const unsigned char *src) {
    for (int i = 0; i < XT * XT; i++) {
        int al = src[i * 4 + 3]; if (!al) continue;
        for (int c = 0; c < 3; c++) dst[i * 4 + c] = (unsigned char)((src[i * 4 + c] * al + dst[i * 4 + c] * (255 - al)) / 255);
        dst[i * 4 + 3] = 255;
    }
}
static int slot_of(const unsigned char *px) {
    int any = 0; for (int i = 0; i < XT * XT; i++) if (px[i * 4 + 3]) { any = 1; break; }
    if (!any) return 0;
    for (int i = 1; i < nslot; i++) if (!memcmp(slotpx[i], px, XT * XT * 4)) return i;
    if (nslot >= MAXSLOT) return 0;
    memcpy(slotpx[nslot], px, XT * XT * 4); return nslot++;
}
static void find_assets(void) {
    char d[PATH_MAX]; snprintf(d, sizeof d, "%s", HOUSE);
    for (int i = 0; i < 8; i++) {
        snprintf(ASSETS, sizeof ASSETS, "%s/#.NNEST_ASSETS/rmmv-www-img", d);
        if (access(ASSETS, R_OK) == 0) return;
        char *s = strrchr(d, '/'); if (!s || s == d) break; *s = 0;
    }
    ASSETS[0] = 0;
}

typedef struct { int x, y, dir, room, r, g, b, steps, art; char name[16]; char letter; } Pet;     /* art = 1: terumon frames exist, so events.txt names charset "terumon" and this index */
static Pet pet[8];
static char glyph[2][ROWS][COLS + 1];
static int fs[2][ROWS][COLS], ws[2][ROWS][COLS];

static void paint_desk(int desk, Sheet *floorS, int fx, int fy, Sheet *wallS, int wx, int wy) {
    unsigned char fl[XT * XT * 4], wa[XT * XT * 4];
    tile24(floorS, fx, fy, fl); tile24(wallS, wx, wy, wa);
    int fls = slot_of(fl), was = slot_of(wa);
    for (int y = 0; y < ROWS; y++) {
        for (int x = 0; x < COLS; x++) { glyph[desk][y][x] = '.'; fs[desk][y][x] = fls; ws[desk][y][x] = 0; }
        glyph[desk][y][COLS] = 0;
    }
    if (desk == 1) {
        for (int x = 0; x < COLS; x++) { glyph[1][0][x] = glyph[1][1][x] = 'W'; ws[1][0][x] = ws[1][1][x] = was; }
        for (int y = 0; y < ROWS; y++) { glyph[1][y][0] = glyph[1][y][COLS - 1] = 'W'; ws[1][y][0] = ws[1][y][COLS - 1] = was; }
    }
}
static void stamp(int desk, Sheet *s, int c, int r, int w, int h, int x, int y, char g, int tall) {
    unsigned char a[XT * XT * 4], b[XT * XT * 4];
    for (int cj = 0; cj < h; cj++) for (int ci = 0; ci < w; ci++) {
        int xx = x + ci, yy = y + cj; if (xx < 0 || yy < 0 || xx >= COLS || yy >= ROWS) continue;
        tile24(s, (c + ci) * S48, (r + cj) * S48, a);
        memset(b, 0, sizeof b);
        if (fs[desk][yy][xx]) memcpy(b, slotpx[fs[desk][yy][xx]], sizeof b);
        over24(b, a);
        glyph[desk][yy][xx] = g;
        int sl = slot_of(b);
        if (tall) ws[desk][yy][xx] = sl; else { fs[desk][yy][xx] = sl; ws[desk][yy][xx] = 0; }
    }
}

static void write_atlas(const char *dir, int desk) {
    mkdir(dir, 0755);
    char p[PATH_MAX]; FILE *f;
    snprintf(p, sizeof p, "%s/map.txt", dir);
    f = fopen(p, "w"); if (f) { for (int y = 0; y < ROWS; y++) fprintf(f, "%s\n", glyph[desk][y]); fclose(f); }
    snprintf(p, sizeof p, "%s/extrusion.pdl", dir);
    f = fopen(p, "w");
    if (f) {
        fputs("SECTION      | KEY                | VALUE\n----------------------------------------------------------------------\n"
              "META         | map_id             | edit\n"
              "EXTRUDE      | W                  | 3\nEXTRUDE      | D                  | 3\n"
              "EXTRUDE      | T                  | 2\nEXTRUDE      | f                  | 1\n"
              "EXTRUDE      | default            | 0\n", f);
        fclose(f);
    }
    int cols = 16, nrows = (nslot + cols - 1) / cols, aw = cols * XT, ah = nrows * XT;
    unsigned char *atlas = calloc((size_t)aw * ah * 4, 1);
    for (int i = 0; i < nslot; i++) for (int y = 0; y < XT; y++)
        memcpy(atlas + ((size_t)((i / cols) * XT + y) * aw + (i % cols) * XT) * 4, slotpx[i] + y * XT * 4, XT * 4);
    snprintf(p, sizeof p, "%s/cells.rgba", dir);
    f = fopen(p, "w"); if (f) { fwrite(atlas, 1, (size_t)aw * ah * 4, f); fclose(f); }
    free(atlas);
    snprintf(p, sizeof p, "%s/cells.txt", dir);
    f = fopen(p, "w");
    if (f) {
        fprintf(f, "width=%d\nheight=%d\ntile_px=%d\natlas_cols=16\natlas_tiles=%d\ncells\n", COLS, ROWS, XT, nslot);
        for (int y = 0; y < ROWS; y++) {
            for (int x = 0; x < COLS; x++) fprintf(f, "%s%d,%d", x ? " " : "", fs[desk][y][x], ws[desk][y][x]);
            fputc('\n', f);
        }
        fclose(f);
    }
    static const char leg[] = "W|3|150|150|160|-|wall|#\nD|3|170|110|50|-|door|+\nT|2|40|140|50|-|tree|T\n.|0|70|140|60|-|floor|.\n";
    snprintf(p, sizeof p, "%s/ascii_legend.txt", dir);
    f = fopen(p, "w"); if (f) { fputs(leg, f); fclose(f); }
}

static int blocked(int desk, int x, int y) {
    if (x < 1 || y < 1 || x >= COLS - 1 || y >= ROWS - 1) return 1;
    char g = glyph[desk][y][x];
    return g == 'W' || g == 'T' || g == 'B';
}
static void write_events(void) {
    const char *page[2] = {"start", "cabin"};
    for (int d = 0; d < 2; d++) {
        char p[PATH_MAX]; snprintf(p, sizeof p, "%s/pieces/system/maps/edit/%s/events.txt", PCHQ, page[d]);
        FILE *f = fopen(p, "w"); if (!f) continue;
        for (int i = 0; i < 8; i++) if (pet[i].room == d) {
            static const int face[4] = {8, 6, 2, 4};      /* dir index 0 up, 1 right, 2 down, 3 left -> RPG Maker 8/6/2/4 */
            int dir = pet[i].dir & 3;
            if (pet[i].art) fprintf(f, "%d %d %d %d %d terumon %d 2 %d %c\n", pet[i].x, pet[i].y, pet[i].r, pet[i].g, pet[i].b, i, pet[i].steps % 3, pet[i].letter);      /* one front picture: always the front plane (a side facing plane is seen edge-on by the 3D camera) */
            else fprintf(f, "%d %d %d %d %d edit 0 %d %d %c\n", pet[i].x, pet[i].y, pet[i].r, pet[i].g, pet[i].b, face[dir], pet[i].steps % 3, pet[i].letter);
        }
        fclose(f);
    }
}
static void follow_ava(void) {
    char p[PATH_MAX]; snprintf(p, sizeof p, "%s/pieces/system/widget_cmds/inbox.txt", PCHQ);
    FILE *f = fopen(p, "a"); if (!f) return;
    fprintf(f, "CONFIRM_SET_DESK:%s\n", pet[4].room ? "cabin" : "start");
    fclose(f);
}
static void step_all(void) {
    static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
    for (int i = 0; i < 8; i++) {
        int dir = rand() % 4;
        int nx = pet[i].x + dx[dir], ny = pet[i].y + dy[dir];
        int desk = pet[i].room;
        if (nx < 0 || ny < 0 || nx >= COLS || ny >= ROWS) continue;
        if (glyph[desk][ny][nx] == 'D') {
            if (i != 4) continue; /* Ava is the one who uses the door */
            pet[i].room = desk ? 0 : 1;
            pet[i].x = desk ? 7 : 8;
            pet[i].y = desk ? 6 : 6;
            pet[i].dir = dir;
            follow_ava();
            continue;
        }
        if (blocked(desk, nx, ny)) continue;
        int occ = 0;
        for (int k = 0; k < 8; k++) if (k != i && pet[k].room == desk && pet[k].x == nx && pet[k].y == ny) occ = 1;
        if (occ) continue;
        pet[i].x = nx; pet[i].y = ny; pet[i].dir = dir; pet[i].steps++;
    }
    write_events();
}

int main(int argc, char **argv) {
    if (argc < 3) return 2;
    snprintf(HOUSE, sizeof HOUSE, "%s", argv[1]);
    snprintf(APP, sizeof APP, "%s", argv[2]);
    snprintf(PCHQ, sizeof PCHQ, "%s/rpg-edit-hq-pchq", APP);
    find_assets();
    Sheet a5 = load_rel("tilesets/Outside_A5.png");
    Sheet wb = load_rel("tilesets/World_B.png");
    Sheet ob = load_rel("tilesets/Outside_B.png");
    Sheet in = load_rel("tilesets/Inside_A5.png");
    Sheet door = load_rel("characters/!Door1.png");
    if (!a5.px || !wb.px) { fprintf(stderr, "rpg_edit: missing RPG Maker sheets under %s\n", ASSETS); return 1; }
    paint_desk(0, &a5, 0, 2 * S48, &a5, 2 * S48, 0);          /* grass floor */
    paint_desk(1, &in.px ? &in : &a5, 0, 0, &in.px ? &in : &a5, S48, 3 * S48);
    /* town house, World_B town cell 6,11 (2x2), and a second house 4,7 */
    stamp(0, &wb, 6, 11, 2, 2, 3, 2, 'B', 1);
    stamp(0, &wb, 4, 7, 2, 2, 10, 2, 'B', 1);
    if (ob.px) { stamp(0, &ob, 0, 0, 1, 1, 1, 3, 'T', 1); stamp(0, &ob, 0, 0, 1, 1, 14, 4, 'T', 1); stamp(0, &ob, 1, 0, 1, 1, 8, 8, 'T', 1); }
    /* door in front of the first house, and the cabin door back */
    if (door.px) {
        unsigned char dpx[XT * XT * 4], b[XT * XT * 4];
        tile24(&door, 0, 192, dpx);
        memcpy(b, slotpx[fs[0][6][4]], sizeof b); over24(b, dpx);
        glyph[0][6][4] = 'D'; ws[0][6][4] = slot_of(b);
        memcpy(b, slotpx[fs[1][6][8]], sizeof b); over24(b, dpx);
        glyph[1][6][8] = 'D'; ws[1][6][8] = slot_of(b);
    } else { glyph[0][6][4] = 'D'; glyph[1][6][8] = 'D'; }
    char root[PATH_MAX];
    snprintf(root, sizeof root, "%s/pieces/system/maps/edit", PCHQ); mkdir(root, 0755);
    char d0[PATH_MAX], d1[PATH_MAX];
    snprintf(d0, sizeof d0, "%s/start", root); snprintf(d1, sizeof d1, "%s/cabin", root);
    write_atlas(d0, 0); write_atlas(d1, 1);
    { char gp[PATH_MAX]; snprintf(gp, sizeof gp, "%s/game.pdl", root); FILE *f = fopen(gp, "w");
      if (f) { fputs("SECTION      | KEY                | VALUE\n----------------------------------------------------------------------\n"
        "GAME         | type               | board-game\nGAME         | icon               | 🖊️\n"
        "GAME         | label              | edit\nGAME         | n_chunks           | 1\n"
        "GAME         | chunk_0_x          | 0\nGAME         | chunk_0_y          | 0\n"
        "GAME         | n_desks            | 2\n"
        "GAME         | desk_1_id          | start\nGAME         | desk_1_label       | start\n"
        "GAME         | desk_2_id          | cabin\nGAME         | desk_2_label       | cabin\n", f); fclose(f); } }
    static const int col[8][3] = {{220,40,40},{40,80,220},{160,40,200},{40,180,60},{240,180,200},{200,200,210},{60,80,200},{180,180,190}};
    static const char *nm[8] = {"Ember","Glacine","Murmur","Solvent","Ava","Cursword","Asa","robot"};
    static const char let[8] = {'e','g','m','s','a','c','A','r'};
    static const int sx[8] = {2,3,6,9,12,13,7,11}, sy[8] = {8,9,9,8,9,7,10,10};
    for (int i = 0; i < 8; i++) {
        pet[i].x = sx[i]; pet[i].y = sy[i]; pet[i].room = 0; pet[i].dir = 0;
        pet[i].r = col[i][0]; pet[i].g = col[i][1]; pet[i].b = col[i][2];
        snprintf(pet[i].name, sizeof pet[i].name, "%s", nm[i]); pet[i].letter = let[i];
    }
    {   /* terumon art: app art/<id>.csv (copied from the pet folders, 64x64 sprite.csv) -> frames under <ASSETS>/../tsots-characters/frames; Ava has none yet and keeps her colour box */
        static const char *art_id[8] = {"terumon_001_ember","terumon_002_glacine","terumon_003_murmur","terumon_004_solvent",0,"cursword","asa","robot_chat_001"};
        char fd[PATH_MAX]; snprintf(fd, sizeof fd, "%s/../tsots-characters", ASSETS); mkdir(fd, 0755); snprintf(fd, sizeof fd, "%s/../tsots-characters/frames", ASSETS); mkdir(fd, 0755);
        for (int i = 0; i < 8; i++) {
            if (!art_id[i]) continue;
            char cp[PATH_MAX]; int avg[3] = {pet[i].r, pet[i].g, pet[i].b}; snprintf(cp, sizeof cp, "%s/art/%s.csv", APP, art_id[i]);
            if (csv_write_frames(cp, fd, "terumon", i, avg)) { pet[i].art = 1; pet[i].r = avg[0]; pet[i].g = avg[1]; pet[i].b = avg[2]; }
            else fprintf(stderr, "rpg_edit: no art for %s (%s)\n", pet[i].name, cp);
        }
    }
    srand((unsigned)time(NULL));
    write_events();
    { char bp[PATH_MAX]; snprintf(bp, sizeof bp, "%s/bar.txt", d0); FILE *bf = fopen(bp, "w");
      if (bf) { for (int i = 0; i < 8; i++) fprintf(bf, "%d\t%s\t%d\t%d\ton-click\n", i + 1, pet[i].name, pet[i].x, pet[i].y); fclose(bf); } }
    for (;;) { step_all(); usleep(700000); }
}
