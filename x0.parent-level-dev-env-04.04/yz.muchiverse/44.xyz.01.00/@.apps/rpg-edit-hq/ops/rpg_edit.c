/* rpg_edit.c — floating-island town for the edit board (BOOK 16.game/
 * EDIT-ISLANDS-DESIGN.md). One outdoor page (start: five islands joined by
 * bridges, one building and one door each) and one shared interior (cabin). RPG Maker tiles. Eight sprites walk. Ava uses the door and the
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
#define COLS 24
#define ROWS 17
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

/* The island page, drawn in EDIT-ISLANDS-DESIGN.md section 2: G grass, p path, T pine, B building, D door, ~ water, b bridge, s ramp, F fire, S sign, r rubble, . sky */
static const char *ISL[ROWS] = {
    "........................",
    "..GBBSGGG...GBBGGGTT....",
    "TGGBBGGGGbbbGBBGGGGT.GBB",
    "..GTDGGTG...GGDGGGGGbGBB",
    "..GGGGGGG....GGGGGb..GGD",
    ".........BBTT.....bb....",
    ".........BB~~......b....",
    ".........GDGG......b....",
    "...........b.......b....",
    "..r.r...GBBGGG..GGGGG...",
    "...r....GBBFGG..GGGTG...",
    ".....r..GTDGGGGGGGGGG...",
    "...r....GGpppppGGTGGs...",
    "........TGGGGGppppGGG...",
    "........GGGGGTGGGpGGG...",
    "........GGGGGGGGGGGGG...",
    "........................" };
typedef struct { int x, y, bx, by, wx, wy; const char *name; } Bld;      /* building top-left, World_B cell (col,row) */
static const Bld bld[5] = {{9, 9, 8, 0, 0, 0, "cabin"}, {3, 1, 6, 11, 0, 0, "bank"}, {13, 1, 0, 7, 0, 0, "store"}, {9, 5, 4, 7, 0, 0, "school"}, {22, 2, 4, 11, 0, 0, "hotel"}};
static int doorx[8], doory[8], ndoor, lastdoor;
static void solid(int desk, int x, int y, char g, int r, int gr, int b, int plank, int tall) {
    unsigned char t[XT * XT * 4];
    for (int i = 0; i < XT * XT; i++) {
        int sh = ((i * 2654435761u) >> 28) % 7 - 3, dk = (plank && ((i / XT) % 6 == 5)) ? -30 : 0;
        t[i * 4] = (unsigned char)(r + sh + dk); t[i * 4 + 1] = (unsigned char)(gr + sh + dk); t[i * 4 + 2] = (unsigned char)(b + sh + dk); t[i * 4 + 3] = 255;
    }
    glyph[desk][y][x] = g; if (tall) ws[desk][y][x] = slot_of(t); else fs[desk][y][x] = slot_of(t);
}
static void paint_islands(Sheet *wb, Sheet *ob, Sheet *door) {
    int grass = fs[0][0][0];
    for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLS; x++) {
        char c = ISL[y][x]; glyph[0][y][x] = c; fs[0][y][x] = grass; ws[0][y][x] = 0;
        switch (c) {
        case '.': glyph[0][y][x] = '_'; fs[0][y][x] = 0; break;      /* the void is a hole: extrusion.pdl `_ | hole` leaves the cell without a block, so the sky shows */
        case '~': solid(0, x, y, '~', 50, 110, 200, 0, 0); break;
        case 'p': solid(0, x, y, 'p', 214, 190, 130, 0, 0); break;
        case 'r': solid(0, x, y, 'r', 190, 165, 110, 0, 0); break;
        case 'b': case 's': solid(0, x, y, c, 150, 100, 55, 1, 0); break;
        case 'T': case 'B': case 'D': case 'S': case 'F': glyph[0][y][x] = 'G'; break;
        }
    }
    for (int i = 0; i < 5; i++) stamp(0, wb, bld[i].bx, bld[i].by, 2, 2, bld[i].x, bld[i].y, 'B', 1);
    for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLS; x++) {
        char c = ISL[y][x];
        if (c == 'T' && ob->px) stamp(0, ob, (x + y) & 1, 0, 1, 1, x, y, 'T', 1);
        else if (c == 'T') solid(0, x, y, 'T', 40, 120, 50, 0, 1);
        else if (c == 'S') solid(0, x, y, 'S', 130, 90, 50, 0, 1);
        else if (c == 'F') solid(0, x, y, 'F', 230, 110, 30, 0, 1);
        else if (c == 'D' && ndoor < 8) {
            doorx[ndoor] = x; doory[ndoor] = y; ndoor++;
            if (door->px) {
                unsigned char dpx[XT * XT * 4], b[XT * XT * 4];
                tile24(door, 0, 192, dpx);
                memcpy(b, slotpx[fs[0][y][x]], sizeof b); over24(b, dpx);
                glyph[0][y][x] = 'D'; ws[0][y][x] = slot_of(b);
            } else glyph[0][y][x] = 'D';
        }
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
              "EXTRUDE      | B                  | 3\nEXTRUDE      | _                  | hole\n"
              "DEPTH        | G                  | 3\nDEPTH        | p                  | 3\nDEPTH        | B                  | 3\n"
              "DEPTH        | T                  | 3\nDEPTH        | D                  | 3\nDEPTH        | S                  | 3\n"
              "DEPTH        | F                  | 3\nDEPTH        | r                  | 2\nDEPTH        | b                  | 1\n"
              "DEPTH        | s                  | 1\nDEPTH        | ~                  | 1\n"
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
    if (desk && (x < 1 || y < 1 || x >= COLS - 1 || y >= ROWS - 1)) return 1;
    char g = glyph[desk][y][x];
    return g == 'W' || g == 'T' || g == 'B' || g == '_' || g == '~' || g == 'r' || g == 'F' || g == 'S';
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
            if (desk == 0) {      /* any island door leads into the shared interior; remember which one */
                for (int k = 0; k < ndoor; k++) if (doorx[k] == nx && doory[k] == ny) lastdoor = k;
                pet[i].room = 1; pet[i].x = COLS / 2; pet[i].y = ROWS - 3;
            } else {              /* back out beside the door she came in by (down, left, right, up: the first walkable cell) */
                static const int ox[4] = {0, -1, 1, 0}, oy[4] = {1, 0, 0, -1};
                int bx = doorx[lastdoor], by = doory[lastdoor];
                pet[i].room = 0; pet[i].x = bx; pet[i].y = by;
                for (int k = 0; k < 4; k++) if (!blocked(0, bx + ox[k], by + oy[k]) && glyph[0][by + oy[k]][bx + ox[k]] != 'D') { pet[i].x = bx + ox[k]; pet[i].y = by + oy[k]; break; }
            }
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
    paint_islands(&wb, &ob, &door);
    {   /* the interior's way out: one door in the middle of the south wall row */
        unsigned char dpx[XT * XT * 4], bb[XT * XT * 4];
        glyph[1][ROWS - 2][COLS / 2] = 'D';
        if (door.px) { tile24(&door, 0, 192, dpx); memcpy(bb, slotpx[fs[1][ROWS - 2][COLS / 2]], sizeof bb); over24(bb, dpx); ws[1][ROWS - 2][COLS / 2] = slot_of(bb); }
    }
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
    static const int sx[8] = {9,10,11,12,12,13,10,11}, sy[8] = {13,13,13,13,14,13,14,15};      /* island F, in front of the cabin */
    for (int i = 0; i < 8; i++) {
        pet[i].x = sx[i]; pet[i].y = sy[i]; pet[i].room = 0; pet[i].dir = 0;
        pet[i].r = col[i][0]; pet[i].g = col[i][1]; pet[i].b = col[i][2];
        snprintf(pet[i].name, sizeof pet[i].name, "%s", nm[i]); pet[i].letter = let[i];
    }
    {   /* terumon art: app art/<id>.csv (copied from the pet folders, 64x64 sprite.csv) -> frames under <ASSETS>/../tsots-characters/frames; Ava is pals/ava/sprite.csv */
        static const char *art_id[8] = {"terumon_001_ember","terumon_002_glacine","terumon_003_murmur","terumon_004_solvent","ava","cursword","asa","robot_chat_001"};
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
