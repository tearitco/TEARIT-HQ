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
#define COLS 56       /* the town is 24 wide; the rest holds the far island, an unconnected width test within normal camera scrolling (BOOK EDIT-ISLANDS-DESIGN section 8) */
#define ROWS 17
#define IW 24          /* the interior's width: the outdoor page is COLS wide, the room is not */
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

typedef struct { int x, y, dir, room, door, r, g, b, steps, art; char name[16]; char letter; } Pet;     /* art = 1: terumon frames exist, so events.txt names charset "terumon" and this index */
#define MAXPET 16
static Pet pet[MAXPET]; static int npet;      /* the roster: party.pdl */
static char artid[MAXPET][48];
static int doorpet = -1;      /* the pet the board page follows through a door when built with -DFOLLOW_AVA (party.pdl DOOR | name); every pet uses the doors */
static char glyph[2][ROWS][COLS + 1];
static int fs[2][ROWS][COLS], ws[2][ROWS][COLS], cdep[2][ROWS][COLS];      /* cdep: blocks hung under the cell (negative z), tapered by taper_islands() */

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
        for (int y = 0; y < ROWS; y++) for (int x = IW - 1; x < COLS; x++) { glyph[1][y][x] = 'W'; ws[1][y][x] = was; }
        for (int y = 0; y < ROWS; y++) { glyph[1][y][0] = 'W'; ws[1][y][0] = was; }
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
/* Far island: unconnected, 10 x 7, 15 cells past the hotel island (x 38, y 5). No bridge, no door, nobody can walk there: it only proves the board draws that wide. */
static const char *FAR[7] = { "..GGGG....", ".GGGTGGG..", "GGGGGGGGG.", "GGpppGGGGG", "GGGGFGGTG.", ".GGGGGGG..", "..GGGG...." };
#define FARX 38
#define FARY 5
static char isl_at(int x, int y) {
    if (x < 24) return ISL[y][x];
    if (x >= FARX && x < FARX + 10 && y >= FARY && y < FARY + 7) return FAR[y - FARY][x - FARX];
    return '.';
}
typedef struct { int x, y, bx, by, wx, wy; const char *name; } Bld;      /* building top-left, World_B cell (col,row) */
static const Bld bld[5] = {{9, 9, 8, 0, 0, 0, "cabin"}, {3, 1, 6, 11, 0, 0, "bank"}, {13, 1, 0, 7, 0, 0, "store"}, {9, 5, 4, 7, 0, 0, "school"}, {22, 2, 4, 11, 0, 0, "hotel"}};
static int doorx[8], doory[8], ndoor, lastdoor, rockslot;      /* rockslot: Outside_A4 rock-cliff tile, painted on the hung (negative z) blocks via cells.txt rock_slot= */
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
        char c = isl_at(x, y); glyph[0][y][x] = c; fs[0][y][x] = grass; ws[0][y][x] = 0;
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
        char c = isl_at(x, y);
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

/* Island undersides taper: the hung depth grows with the ring distance from the nearest air cell (edge 1, then 3, 5, 7, cap 9), so each island ends in an inverted cone; bridges and rubble stay thin. */
static void taper_islands(void) {
    for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLS; x++) {
        char c = glyph[0][y][x];
        if (c == '_') { cdep[0][y][x] = 0; continue; }
        if (c == 'b' || c == 's') { cdep[0][y][x] = 1; continue; }
        if (c == 'r') { cdep[0][y][x] = 2; continue; }
        int best = 99;
        for (int yy = -1; yy <= ROWS; yy++) for (int xx = -1; xx <= COLS; xx++) {
            int air = (yy < 0 || xx < 0 || yy >= ROWS || xx >= COLS) || glyph[0][yy][xx] == '_';
            if (!air) continue;
            int dx = abs(xx - x), dy = abs(yy - y), d = dx > dy ? dx : dy;
            if (d < best) best = d;
        }
        int dep = 2 * best - 1; if (dep > 9) dep = 9; if (dep < 1) dep = 1;
        cdep[0][y][x] = dep;
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
              "EXTRUDE      | B                  | 3\n"
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
        fprintf(f, "width=%d\nheight=%d\ntile_px=%d\natlas_cols=16\natlas_tiles=%d\nrock_slot=%d\ncells\n", COLS, ROWS, XT, nslot, rockslot);
        for (int y = 0; y < ROWS; y++) {
            for (int x = 0; x < COLS; x++) fprintf(f, "%s%d,%d,%d", x ? " " : "", fs[desk][y][x], ws[desk][y][x], cdep[desk][y][x]);
            fputc('\n', f);
        }
        fclose(f);
    }
    static const char leg[] = "W|3|150|150|160|-|wall|#\nD|3|170|110|50|-|door|+\nT|2|40|140|50|-|tree|T\n.|0|70|140|60|-|floor|.\n";
    snprintf(p, sizeof p, "%s/ascii_legend.txt", dir);
    f = fopen(p, "w"); if (f) { fputs(leg, f); fclose(f); }
}

static int blocked(int desk, int x, int y) {
    if (desk && (x < 1 || y < 1 || x >= IW - 1 || y >= ROWS - 1)) return 1;
    char g = glyph[desk][y][x];
    return g == 'W' || g == 'T' || g == 'B' || g == '_' || g == '~' || g == 'r' || g == 'F' || g == 'S';
}
static void write_events(void) {
    const char *page[2] = {"start", "cabin"};
    for (int d = 0; d < 2; d++) {
        char p[PATH_MAX]; snprintf(p, sizeof p, "%s/pieces/system/maps/edit/%s/events.txt", PCHQ, page[d]);
        FILE *f = fopen(p, "w"); if (!f) continue;
        for (int i = 0; i < npet; i++) if (pet[i].room == d) {
            static const int face[4] = {8, 6, 2, 4};      /* dir index 0 up, 1 right, 2 down, 3 left -> RPG Maker 8/6/2/4 */
            int dir = pet[i].dir & 3;
            if (pet[i].art) fprintf(f, "%d %d %d %d %d terumon %d 2 %d %c\n", pet[i].x, pet[i].y, pet[i].r, pet[i].g, pet[i].b, i, pet[i].steps % 3, pet[i].letter);      /* one front picture: always the front plane (a side facing plane is seen edge-on by the 3D camera) */
            else fprintf(f, "%d %d %d %d %d edit 0 %d %d %c\n", pet[i].x, pet[i].y, pet[i].r, pet[i].g, pet[i].b, face[dir], pet[i].steps % 3, pet[i].letter);
        }
        fclose(f);
    }
}
/* Side bars (left: the eight pets, right: the log). state/side_ui.txt is a var file of the board window: pet_0..7, log_0..5, n_pet. */
static char sidelog[6][64];
static void say(const char *who, const char *text) {
    memmove(sidelog[0], sidelog[1], sizeof sidelog[0] * 5);
    snprintf(sidelog[5], sizeof sidelog[5], "%s: %s", who, text);
}
static void write_side(void) {
    char p[PATH_MAX]; snprintf(p, sizeof p, "%s/board/state/side_ui.txt", APP);
    FILE *f = fopen(p, "w"); if (!f) return;
    fprintf(f, "n_pet=%d\n", npet);
    for (int i = 0; i < npet; i++) fprintf(f, "pet_%d_label=%s %d,%d %s\npet_%d_idx=%d\n", i, pet[i].name, pet[i].x, pet[i].y, pet[i].room ? "(inside)" : "", i, i);
    for (int i = 0; i < 6; i++) fprintf(f, "chat_%d=%s\n", i, sidelog[i]);
    fclose(f);
}
/* set one key=value line of a state file (add it if absent); the viewer and the clock daemon re-read these files */
static void set_kv(const char *path, const char *key, const char *val) {
    char buf[8192] = ""; size_t n = 0; FILE *f = fopen(path, "r");
    if (f) { n = fread(buf, 1, sizeof buf - 1, f); buf[n] = 0; fclose(f); }
    char out[8300]; size_t o = 0; int done = 0, kl = (int)strlen(key);
    for (char *l = buf; *l; ) {
        char *e = strchr(l, '\n'); size_t len = e ? (size_t)(e - l) : strlen(l);
        if (!strncmp(l, key, kl) && l[kl] == '=') { o += snprintf(out + o, sizeof out - o, "%s=%s\n", key, val); done = 1; }
        else o += snprintf(out + o, sizeof out - o, "%.*s\n", (int)len, l);
        l += len + (e ? 1 : 0);
    }
    if (!done) o += snprintf(out + o, sizeof out - o, "%s=%s\n", key, val);
    f = fopen(path, "w"); if (f) { fwrite(out, 1, o, f); fclose(f); }
}
/* typed chat and the pet buttons arrive as lines in board/state/say_in.txt (appended by "rpg_edit.+x <board dir> say <text>" / "sel <n>", the cli_io and item actions of the board window) */
static long sayoff;
static void read_inbox(void) {
    char p[PATH_MAX]; snprintf(p, sizeof p, "%s/board/state/say_in.txt", APP);
    FILE *f = fopen(p, "r"); if (!f) return;
    fseek(f, 0, SEEK_END); long end = ftell(f); if (end < sayoff) sayoff = 0;
    fseek(f, sayoff, SEEK_SET);
    char ln[200];
    while (fgets(ln, sizeof ln, f)) {
        ln[strcspn(ln, "\n")] = 0; if (!ln[0]) continue;
        static const char *re[6] = {"hello!", "I was just walking", "nice weather up here", "have you seen the bank?", "the sky is so big", "hi hi"};
        if (!strcmp(ln, "newpal")) {      /* the Menu "+ new pal": one more PET row in party.pdl (a coloured block, spawn in front of the cabin), live */
            if (npet >= MAXPET) { say("you", "no more pals (16)"); continue; }
            int k = npet, r = 60 + (k * 53) % 180, g = 60 + (k * 97) % 180, b = 60 + (k * 29) % 180; Pet *pp = &pet[npet]; memset(pp, 0, sizeof *pp);
            snprintf(pp->name, sizeof pp->name, "Pal %d", k + 1); artid[k][0] = 0; pp->letter = 'p'; pp->r = r; pp->g = g; pp->b = b; pp->x = 9 + k % 5; pp->y = 13 + (k / 5) % 3; npet++;
            char rp[PATH_MAX]; snprintf(rp, sizeof rp, "%s/party.pdl", APP); FILE *rf = fopen(rp, "a");
            if (rf) { fprintf(rf, "PET | %s | none | p | %d | %d | %d | %d | %d\n", pp->name, r, g, b, pp->x, pp->y); fclose(rf); }
            say("you", pp->name); continue; }
        if (!strncmp(ln, "sel ", 4)) {
            int n = atoi(ln + 4); if (n < 0 || n >= npet) continue;
            char xp[PATH_MAX], v[16]; snprintf(xp, sizeof xp, "%s/pieces/xelector_01/state.txt", PCHQ);
            snprintf(v, sizeof v, "%d", pet[n].x); set_kv(xp, "pos_x", v);
            snprintf(v, sizeof v, "%d", pet[n].y); set_kv(xp, "pos_y", v);
            { char mp[PATH_MAX], ln2[120]; int fz = 16; snprintf(mp, sizeof mp, "%s/pieces/system/board_manifest.txt", PCHQ);      /* the floor level is board_manifest floor_z; the selector stands one above it */
              FILE *mf = fopen(mp, "r"); if (mf) { while (fgets(ln2, sizeof ln2, mf)) if (!strncmp(ln2, "floor_z=", 8)) fz = atoi(ln2 + 8); fclose(mf); }
              snprintf(v, sizeof v, "%d", fz + 1); set_kv(xp, "pos_z", v); }
            say("you", pet[n].name);
        } else { say("you", ln); if (npet) say(pet[rand() % npet].name, re[rand() % 6]); }
    }
    sayoff = ftell(f); fclose(f);
}
static void follow_ava(void) {
    /* The board stays on the island page when Ava goes through a door (she just vanishes into the building and comes back out). Following her switched the whole board to the cabin page, which renders as empty sky from the island camera, so the xelector and camera looked gone (owner 2026-10-21). Open the cabin by hand from the Desk menu; build with -DFOLLOW_AVA to bring the old behaviour back. */
#ifndef FOLLOW_AVA
    return;
#endif
    char p[PATH_MAX]; snprintf(p, sizeof p, "%s/pieces/system/widget_cmds/inbox.txt", PCHQ);
    FILE *f = fopen(p, "a"); if (!f) return;
    fprintf(f, "CONFIRM_SET_DESK:%s\n", doorpet >= 0 && pet[doorpet].room ? "cabin" : "start");
    fclose(f);
}
/* An idle engine only renders on input (BOOK 16.game/PETSLIKE-FRAMEWORK-PLAN.md, same rule as petslike_core.c): after the pets stepped, append the no-op key 126 to this board's engine session relay so the 3D view redraws the moved sprites. The session is the bv_session= line of board/state/ui.txt. */
static void nudge_engine(void) {
    char up[PATH_MAX], ub[2048], sess[PATH_MAX] = "", rp[PATH_MAX]; snprintf(up, sizeof up, "%s/board/state/ui.txt", APP);
    FILE *f = fopen(up, "r"); if (!f) return;
    size_t n = fread(ub, 1, sizeof ub - 1, f); ub[n] = 0; fclose(f);
    for (char *l = strtok(ub, "\n"); l; l = strtok(NULL, "\n")) if (!strncmp(l, "bv_session=", 11)) snprintf(sess, sizeof sess, "%s", l + 11);
    if (!sess[0]) return;
    snprintf(rp, sizeof rp, "%s/pieces/apps/player_app/interact_relay.txt", sess);
    if (access(rp, F_OK)) return;
    f = fopen(rp, "a"); if (f) { fputs("126\n", f); fclose(f); }
}
static int quiet[MAXPET][MAXPET];      /* steps until a pair of pets greets again */
static void step_all(void) {
    read_inbox();
    for (int a = 0; a < npet; a++) for (int b = 0; b < npet; b++) if (quiet[a][b] > 0) quiet[a][b]--;
    static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
    for (int i = 0; i < npet; i++) {
        int dir = rand() % 4;
        int nx = pet[i].x + dx[dir], ny = pet[i].y + dy[dir];
        int desk = pet[i].room;
        if (nx < 0 || ny < 0 || nx >= COLS || ny >= ROWS) continue;
        if (glyph[desk][ny][nx] == 'D') {
            if (desk == 0) {      /* any island door leads into the shared interior; remember which one */
                for (int k = 0; k < ndoor; k++) if (doorx[k] == nx && doory[k] == ny) lastdoor = pet[i].door = k;
                pet[i].room = 1; pet[i].x = IW / 2; pet[i].y = ROWS - 3;
                for (int t = 0; t < 40; t++) {      /* the first free cell near the cabin door, not on top of a pet already inside */
                    int cx = IW / 2 + (t % 5) - 2, cy = ROWS - 3 - t / 5, occ = 0;
                    for (int k = 0; k < npet; k++) if (k != i && pet[k].room == 1 && pet[k].x == cx && pet[k].y == cy) occ = 1;
                    if (!occ && !blocked(1, cx, cy) && glyph[1][cy][cx] != 'D') { pet[i].x = cx; pet[i].y = cy; break; } }
            } else {              /* back out beside the door she came in by (down, left, right, up: the first walkable cell) */
                static const int ox[4] = {0, -1, 1, 0}, oy[4] = {1, 0, 0, -1};
                int bx = doorx[pet[i].door], by = doory[pet[i].door];
                pet[i].room = 0; pet[i].x = bx; pet[i].y = by;
                for (int k = 0; k < 4; k++) if (!blocked(0, bx + ox[k], by + oy[k]) && glyph[0][by + oy[k]][bx + ox[k]] != 'D') { pet[i].x = bx + ox[k]; pet[i].y = by + oy[k]; break; }
            }
            pet[i].dir = dir;
            { char t[48]; snprintf(t, sizeof t, "%s door %d", pet[i].room ? "went in at" : "came out of", pet[i].door); say(pet[i].name, t); }
            follow_ava();
            continue;
        }
        if (blocked(desk, nx, ny)) continue;
        int occ = 0;
        for (int k = 0; k < npet; k++) if (k != i && pet[k].room == desk && pet[k].x == nx && pet[k].y == ny) occ = 1;
        if (occ) continue;
        pet[i].x = nx; pet[i].y = ny; pet[i].dir = dir; pet[i].steps++;
        for (int k = 0; k < npet; k++) {      /* two pets side by side greet each other (chat bar); a pair stays quiet for a while after */
            if (k == i || pet[k].room != desk || abs(pet[k].x - nx) + abs(pet[k].y - ny) != 1) continue;
            if (quiet[i][k] > 0) continue;
            static const char *hi[4] = {"hi %s!", "hello %s", "oh, %s", "hey %s, nice day"};
            char t[48]; snprintf(t, sizeof t, hi[rand() % 4], pet[k].name); say(pet[i].name, t);
            snprintf(t, sizeof t, "hello %s", pet[i].name); say(pet[k].name, t);
            quiet[i][k] = quiet[k][i] = 40;
        }
    }
    write_events(); write_side();
    nudge_engine();
}

int main(int argc, char **argv) {
    if (argc >= 3 && strstr(argv[1], "/board") && strlen(argv[1]) > 6 && !strcmp(strrchr(argv[1], '/'), "/board")) {      /* action form (cli_io: board dir, house, text / item: board dir, sel, n) */
        char ip[PATH_MAX]; snprintf(ip, sizeof ip, "%s/state/say_in.txt", argv[1]);
        FILE *f = fopen(ip, "a"); if (!f) return 1;
        if (!strcmp(argv[2], "sel") && argc >= 4) fprintf(f, "sel %s\n", argv[3]);
        else if (!strcmp(argv[2], "newpal")) fprintf(f, "newpal\n");
        else if (argc >= 4) fprintf(f, "%s\n", argv[3]);
        fclose(f); return 0;
    }
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
    {   Sheet a4 = load_rel("tilesets/Outside_A4.png");        /* the rocky-earth cliff face (row 3, first block) */
        if (a4.px) { unsigned char rk[XT * XT * 4]; tile24(&a4, 0, 13 * S48, rk); rockslot = slot_of(rk); }
    }
    paint_islands(&wb, &ob, &door);
    {   /* the interior's way out: one door in the middle of the south wall row */
        unsigned char dpx[XT * XT * 4], bb[XT * XT * 4];
        glyph[1][ROWS - 2][IW / 2] = 'D';
        if (door.px) { tile24(&door, 0, 192, dpx); memcpy(bb, slotpx[fs[1][ROWS - 2][IW / 2]], sizeof bb); over24(bb, dpx); ws[1][ROWS - 2][IW / 2] = slot_of(bb); }
    }
    taper_islands();
    char root[PATH_MAX];
    snprintf(root, sizeof root, "%s/pieces/system/maps/edit", PCHQ); mkdir(root, 0755);
    char d0[PATH_MAX], d1[PATH_MAX];
    snprintf(d0, sizeof d0, "%s/start", root); snprintf(d1, sizeof d1, "%s/cabin", root);
    write_atlas(d0, 0); write_atlas(d1, 1);
    { char gp[PATH_MAX], extra[16][48]; int nx = 0; snprintf(gp, sizeof gp, "%s/game.pdl", root);
      { FILE *g = fopen(gp, "r"); char ln[200];      /* pages added from the Desk menu (+ new page) stay listed: keep every desk id that is not one of the two built here */
        while (g && fgets(ln, sizeof ln, g)) { char *k = strstr(ln, "| desk_"), *v = k ? strstr(k, "_id") : NULL; if (!v) continue; v = strchr(v, '|'); if (!v) continue;
            v++; while (*v == ' ') v++; v[strcspn(v, " \r\n")] = 0;
            if (v[0] && strcmp(v, "start") && strcmp(v, "cabin") && nx < 16) snprintf(extra[nx++], 48, "%s", v); }
        if (g) fclose(g); }
      FILE *f = fopen(gp, "w");
      if (f) { fputs("SECTION      | KEY                | VALUE\n----------------------------------------------------------------------\n"
        "GAME         | type               | board-game\nGAME         | icon               | 🖊️\n"
        "GAME         | label              | edit\nGAME         | n_chunks           | 1\n"
        "GAME         | chunk_0_x          | 0\nGAME         | chunk_0_y          | 0\n"
        "GAME         | desk_1_id          | start\nGAME         | desk_1_label       | start\n"
        "GAME         | desk_2_id          | cabin\nGAME         | desk_2_label       | cabin\n", f);
        for (int i = 0; i < nx; i++) fprintf(f, "GAME         | desk_%d_id          | %s\nGAME         | desk_%d_label       | %s\n", i + 3, extra[i], i + 3, extra[i]);
        fprintf(f, "GAME         | n_desks            | %d\n", 2 + nx); fclose(f); } }
    {   /* the roster is data: party.pdl, PET | name | art id | letter | r | g | b | spawn x | spawn y */
        char rp[PATH_MAX], ln[300]; snprintf(rp, sizeof rp, "%s/party.pdl", APP); FILE *rf = fopen(rp, "r");
        while (rf && fgets(ln, sizeof ln, rf) && npet < MAXPET) {
            char *g[10]; int n = 0; char *q = ln;
            if (!strncmp(ln, "DOOR", 4) && ln[4] == ' ') { char *bar = strchr(ln, '|'); if (bar) { bar++; while (*bar == ' ') bar++; bar[strcspn(bar, " \r\n")] = 0; for (int k = 0; k < npet; k++) if (!strcmp(pet[k].name, bar)) doorpet = k; } continue; }
            if (strncmp(ln, "PET", 3)) continue;
            for (; n < 10; n++) { char *bar = strchr(q, '|'); if (bar) *bar = 0; while (*q == ' ') q++; { size_t l = strlen(q); while (l && (q[l-1] == ' ' || q[l-1] == '\n' || q[l-1] == '\r')) q[--l] = 0; } g[n] = q; if (!bar) { n++; break; } q = bar + 1; }
            if (n < 9) continue;
            Pet *pp = &pet[npet]; memset(pp, 0, sizeof *pp);
            snprintf(pp->name, sizeof pp->name, "%s", g[1]); snprintf(artid[npet], sizeof artid[npet], "%s", g[2]); pp->letter = g[3][0] ? g[3][0] : '?';
            pp->r = atoi(g[4]); pp->g = atoi(g[5]); pp->b = atoi(g[6]); pp->x = atoi(g[7]); pp->y = atoi(g[8]); pp->room = 0; pp->dir = 0; npet++;
        }
        if (rf) fclose(rf);
        if (!npet) fprintf(stderr, "rpg_edit: no PET rows in %s: the island has no pets\n", rp);
    }
    {   /* terumon art: app art/<id>.csv (copied from the pet folders, 64x64 sprite.csv) -> frames under <ASSETS>/../tsots-characters/frames; Ava is pals/ava/sprite.csv */
        char fd[PATH_MAX]; snprintf(fd, sizeof fd, "%s/../tsots-characters", ASSETS); mkdir(fd, 0755); snprintf(fd, sizeof fd, "%s/../tsots-characters/frames", ASSETS); mkdir(fd, 0755);
        for (int i = 0; i < npet; i++) {
            if (!artid[i][0]) continue;
            char cp[PATH_MAX]; int avg[3] = {pet[i].r, pet[i].g, pet[i].b}; snprintf(cp, sizeof cp, "%s/art/%s.csv", APP, artid[i]);
            if (csv_write_frames(cp, fd, "terumon", i, avg)) { pet[i].art = 1; pet[i].r = avg[0]; pet[i].g = avg[1]; pet[i].b = avg[2]; }
            else fprintf(stderr, "rpg_edit: no art for %s (%s)\n", pet[i].name, cp);
        }
    }
    {   /* the footer's entity images: the projector draws <PCHQ>/pieces/<event id>/sprite.csv (event id = roster index + 1), the same file as the pet's art csv */
        for (int i = 0; i < npet; i++) {
            if (!pet[i].art) continue;
            char sd[PATH_MAX], src[PATH_MAX]; snprintf(sd, sizeof sd, "%s/pieces/%d", PCHQ, i + 1); mkdir(sd, 0755);
            snprintf(src, sizeof src, "%s/art/%s.csv", APP, artid[i]); strncat(sd, "/sprite.csv", sizeof sd - strlen(sd) - 1);
            FILE *in_ = fopen(src, "rb"), *out = in_ ? fopen(sd, "wb") : NULL; char cb[8192]; size_t cn;
            while (out && (cn = fread(cb, 1, sizeof cb, in_)) > 0) fwrite(cb, 1, cn, out);
            if (out) fclose(out); if (in_) fclose(in_);
        }
    }
    {   /* wide islands: this map sets its own fog (the house default fades everything past 48 cells to sky) */
        char vp[PATH_MAX], buf[4096] = ""; snprintf(vp, sizeof vp, "%s/pieces/system/view.pdl", PCHQ);
        FILE *vf = fopen(vp, "r"); if (vf) { size_t n = fread(buf, 1, sizeof buf - 1, vf); buf[n] = 0; fclose(vf); }
        if (!strstr(buf, "view_fog_end") && (vf = fopen(vp, "a"))) { fputs("# the island map is wide: fog from 90 to 250 cells, not the house 18..48\nview_fog_start=90\nview_fog_end=250\n", vf); fclose(vf); }
        if (!strstr(buf, "xelector_z_min") && (vf = fopen(vp, "a"))) { fputs("# the hung rock is up to 9 levels deep: z / x may take the xelector down into it (levels relative to the floor)\nxelector_z_min=-8\n", vf); fclose(vf); }
        { char wp[PATH_MAX]; snprintf(wp, sizeof wp, "%s/pieces/world_01/state.txt", PCHQ); set_kv(wp, "autotick_enabled", "1"); }      /* the session's pc_clock_daemon then runs the day: sky, light and the sun and moon follow it */
    }
    srand((unsigned)time(NULL));
    write_events(); write_side();
    { char bp[PATH_MAX]; snprintf(bp, sizeof bp, "%s/bar.txt", d0); FILE *bf = fopen(bp, "w");
      if (bf) { for (int i = 0; i < npet; i++) fprintf(bf, "%d\t%s\t%d\t%d\ton-click\n", i + 1, pet[i].name, pet[i].x, pet[i].y); fclose(bf); } }
    for (;;) { step_all(); usleep(700000); }
}
