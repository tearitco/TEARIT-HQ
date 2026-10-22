/* civ_rpgce.c - civ-rpgce on the shared pets-like core (&.widgits/_shared-lib/petslike_core.c). Two roles in one file: the top builds the program, and the core
 * re-includes this file once per SECTION to pull in what is specific to this game. Design: @.apps/../#.#.calendar-dox/!.HQ-IQ-BOOK/16.game/PETSLIKE-FRAMEWORK-PLAN.md */
#ifndef PET_SECTION
#define PET_APP "civ-rpgce"
#define PET_OP "civ_rpgce"
#define PET_SKIN "civ_rpgce.c"
#include "../../../&.widgits/_shared-lib/petslike_core.c"
#else
#if PET_SECTION == 1      /* core line 21 */
 * Every picture is an RPG Maker MV tile or character frame (sheets under #.NNEST_ASSETS/rmmv-www-img), nothing else is drawn.
#elif PET_SECTION == 2      /* core line 52 */
#include "../../../&.widgits/_shared-lib/kh_state_notify.h"      /* STATE_BACKEND=file|marker|shmem: "did it change" without reading it (branch marker, 2026-10-12) */
#elif PET_SECTION == 3      /* core line 54 */
/* Per-room geometry: the room being processed is g_gr (set by render, use_ent, walking, placement, export). A normal room is 13 x 8 cells of 48 px with a sky/wall strip (trig row 2); an OVERMAP room (ROOM ... over) is a
 * world-map biome (the island city): cols x rows cells of tpx px (the 48 px RPG Maker tiles are scaled down), no wall strip (trig row 0). Source tile size in the sheets is always S48. */
#define S48 48
#define MAXC 40
#define MAXR 30
static int g_gr;
#define T (rooms[g_gr].tpx)
#elif PET_SECTION == 4      /* core line 63 */
#define COLS (rooms[g_gr].cols)
#define ROWS (rooms[g_gr].rows)
#elif PET_SECTION == 5      /* core line 68 */
#define TRIG_ROW (rooms[g_gr].trig)
#elif PET_SECTION == 6      /* core line 72 */
#define NP 8      /* up to 6 DB members + the pals made with "+ new pal" (pals.pdl) */
#elif PET_SECTION == 7      /* core line 75 */
typedef struct { char id[16], name[32], biome[16]; int fc, fr, outdoor, over, cols, rows, tpx, trig; } Room;
#elif PET_SECTION == 8      /* core line 77 */
typedef struct { int room, tx, ty, dest, ax, ay, autow; } Door;      /* the door object is the cell (tx,ty); its trigger is the cell below it (tx,ty+1); ty = WALL_ROW in a normal room */
#elif PET_SECTION == 9      /* core line 83 */
static char PCHQ[PATH_MAX];      /* this game's OWN pc-hq instance (data only: world, hero, xelector, maps, engine sessions): <app>/civ-rpgce-pchq, see PCHQ-SESSIONS-DESIGN.md. Programs stay in @.apps/piececraft-hq. */
#define SIBLING_APP "/@.apps/" "rpg-pet" "/"      /* the carbon-copy game that shares the one pc-hq engine + world: its board window is NOT a viewer of this game (see viewer_kinds); the copy sets this to the other game */
#elif PET_SECTION == 10      /* core line 89 */
#elif PET_SECTION == 11      /* core line 90 */
static Sheet sheets[24]; static int nsheet;
#elif PET_SECTION == 12      /* core line 139 */
/* STATE_BACKEND (kh_state_notify.h): file = every tick re-reads everything (the old behaviour); marker / shmem = a slot moves when a writer wrote, readers reload only then. Slots: */
enum { SL_PETS = 0, SL_FLAGS, SL_DAY };
static SnState g_sn; static long g_flag_epoch = 1;      /* g_flag_epoch: bumped when flags/switches changed, invalidates the flag cache (non-file modes) */
static void sn_note_write(const char *p) {      /* every state write funnels through write_if_changed: the writer tells the slot after it wrote */
    if (!sn_active(&g_sn)) return; const char *b = strrchr(p, '/'); b = b ? b + 1 : p; int slot = -1;
    if (!strncmp(b, "pet_", 4) || !strcmp(b, "trainer.txt") || !strcmp(b, "active.txt")) slot = SL_PETS;
    else if (!strncmp(b, "flag_", 5) || !strcmp(b, "switches.txt")) slot = SL_FLAGS;
    else if (!strcmp(b, "daylight.txt")) slot = SL_DAY;
    if (slot >= 0) sn_bump_own(&g_sn, slot);
}
#elif PET_SECTION == 13      /* core line 153 */
    f = fopen(tmp, "wb"); if (!f) return 0; size_t wn = fwrite(data, 1, n, f); int ce = fclose(f); if (wn != n || ce != 0 || rename(tmp, p) != 0) { remove(tmp); return 0; } sn_note_write(p); return 1;
#elif PET_SECTION == 14      /* core line 188 */
static struct { char k[48]; char v[64]; long t; long ep; } g_fc[32];
#elif PET_SECTION == 15      /* core line 190 */
    char ck[48]; long cnow = 0; if (g_fcache_on) { snprintf(ck, sizeof ck, "%s|%s", name, def); cnow = now_ms(); for (int i = 0; i < g_fcn; i++) if ((sn_active(&g_sn) ? g_fc[i].ep == g_flag_epoch : cnow - g_fc[i].t < tune("flag_cache_ms", 150)) && !strcmp(g_fc[i].k, ck)) { snprintf(out, cap, "%s", g_fc[i].v); return; } }
#elif PET_SECTION == 16      /* core line 192 */
    if (g_fcache_on) { int s = -1; for (int i = 0; i < g_fcn; i++) if (!strcmp(g_fc[i].k, ck)) s = i; if (s < 0 && g_fcn < 32) s = g_fcn++; if (s >= 0) { snprintf(g_fc[s].k, sizeof g_fc[s].k, "%s", ck); snprintf(g_fc[s].v, sizeof g_fc[s].v, "%s", out); g_fc[s].t = cnow; g_fc[s].ep = g_flag_epoch; } }
#elif PET_SECTION == 17      /* core line 198 */
#elif PET_SECTION == 18      /* core line 209 */
    nparty = 0; char names[NP][24]; int nn = 0, inparty = 0, inhero = 0; memset(&hero, 0, sizeof hero); snprintf(hero.name, 24, "Red"); snprintf(hero.sheet, 24, "viking"); hero.idx = 0; /* settler. The picture is the grey armor on the viking sheet, tinted. */
#elif PET_SECTION == 19      /* core line 211 */
    if ((fp = fopen(p, "r"))) { while (fgets(line, sizeof line, fp)) { if (split_bar(line, f, 3) < 3 || strcmp(f[0], "SYSTEM")) continue; if (!strcmp(f[1], "name")) { inparty = !strcmp(f[2], getenv("PET_PARTY") && getenv("PET_PARTY")[0] ? getenv("PET_PARTY") : "civ_rpgceParty"); inhero = 0; } else if (inparty && !strncmp(f[1], "member", 6) && nn < NP) snprintf(names[nn++], 24, "%s", f[2]); } fclose(fp); }
    if (nn == 0) { const char *fb[] = {"Blue", "Green"}; for (int i = 0; i < 2 && nn < NP; i++) snprintf(names[nn++], 24, "%s", fb[i]); }
    for (int i = 0; i < nn; i++) { memset(&party[nparty], 0, sizeof party[nparty]); snprintf(party[nparty].name, 24, "%s", names[i]); snprintf(party[nparty].id, 16, "%s", names[i]); for (char *c = party[nparty].id; *c; c++) if (*c >= 'A' && *c <= 'Z') *c += 32; party[nparty].idx = i; snprintf(party[nparty].sheet, 24, "viking"); party[nparty].idx = i + 1; nparty++; }
#elif PET_SECTION == 20      /* core line 217 */
        party[nparty].idx = atoi(g[3]) & 7; snprintf(party[nparty].sheet, 24, "%s", nparty ? party[0].sheet : "Monster"); party[nparty].mhp = 40; party[nparty].mmp = 20; party[nparty].atk = party[nparty].def = party[nparty].agi = party[nparty].luk = 10; party[nparty].mat = party[nparty].mdf = 8; nparty++; } fclose(fp); }
#elif PET_SECTION == 21      /* core line 249 */
            Door *d = &cache[ncache++]; d->room = a; d->tx = tx; d->ty = WALL_ROW; d->dest = b; d->ax = ax; d->ay = ay; d->autow = au; }
#elif PET_SECTION == 22      /* core line 258 */
#elif PET_SECTION == 23      /* core line 260 */
        while (fgets(line, sizeof line, fp)) { int n = split_bar(line, f, 12); if (n >= 5 && !strcmp(f[0], "ROOM") && nroom < 8) { Room *r = &rooms[nroom++]; memset(r, 0, sizeof *r); snprintf(r->id, 16, "%s", f[1]); snprintf(r->name, 32, "%s", f[2]); r->fc = atoi(f[3]); r->fr = atoi(f[4]); r->outdoor = n >= 6 && (!strcmp(f[5], "outdoor") || !strcmp(f[5], "over")); r->cols = 13; r->rows = 8; r->tpx = 48; r->trig = 2;
            if (n >= 9 && !strcmp(f[5], "over")) { r->over = 1; r->cols = atoi(f[6]); r->rows = atoi(f[7]); r->tpx = atoi(f[8]); r->trig = 0; snprintf(r->biome, 16, "%s", n >= 10 ? f[9] : "worldmap"); if (r->cols < 4 || r->cols > MAXC || r->rows < 4 || r->rows > MAXR || r->tpx < 8 || r->tpx > 48) { r->cols = 20; r->rows = 13; r->tpx = 32; } } } }
#elif PET_SECTION == 24      /* core line 263 */
        while (fgets(line, sizeof line, fp)) { int n = split_bar(line, f, 12); if (n >= 7 && !strcmp(f[0], "DOOR") && ndoor < 24) { int a = room_idx(f[1]), b = room_idx(f[3]); if (a < 0 || b < 0) continue; Door *d = &doors[ndoor++]; d->room = a; d->tx = atoi(f[2]); d->ty = n >= 8 ? atoi(f[7]) : WALL_ROW; d->dest = b; d->ax = atoi(f[4]); d->ay = atoi(f[5]); d->autow = atoi(f[6]); } }
#elif PET_SECTION == 25      /* core line 266 */
#elif PET_SECTION == 26      /* core line 268 */
#elif PET_SECTION == 27      /* core line 279 */
#elif PET_SECTION == 28      /* core line 281 */
    char p[PATH_MAX], line[300]; spath(p, "house.txt");
#elif PET_SECTION == 29      /* core line 299 */
    e->room = 0; e->x = i < 0 ? 8 : 7 + i; e->y = i < 0 ? 8 : 7; e->dir = 0; e->steps = 0; e->tx = e->ty = -1;
#elif PET_SECTION == 30      /* core line 302 */
    { int cw = (nroom > 0 && e->room >= 0 && e->room < nroom) ? rooms[e->room].cols : 13, ch = (nroom > 0 && e->room >= 0 && e->room < nroom) ? rooms[e->room].rows : 8; if (cw < 1) cw = 1; if (ch < 1) ch = 1; if (cw > MAXC) cw = MAXC; if (ch > MAXR) ch = MAXR;
#elif PET_SECTION == 31      /* core line 313 */
static void use_ent(int i) { me = i; pet = i < 0 ? tr : others[i]; if (pet.room >= 0 && pet.room < nroom) g_gr = pet.room; }
#elif PET_SECTION == 32      /* core line 316 */
    write_if_changed(p, b, strlen(b)); if (me < 0) tr = pet; else others[me] = pet; calc_view();
#elif PET_SECTION == 33      /* core line 326 */
static int over_land(const Room *rm, int x, int y);
static int floor_blocked(int room, int x, int y, int skip_id) { if (rooms[room].over && !over_land(&rooms[room], x, y)) return 9999; for (int i = 0; i < nscen; i++) if (scen[i].room == room && x >= scen[i].x && x < scen[i].x + scen[i].w && y >= scen[i].y && y < scen[i].y + scen[i].h) return 9999; for (int i = 0; i < npl; i++) { Placed *q = &pl[i]; if (q->room != room || q->id == skip_id || cat[q->item].wall) continue; if (x >= q->x && x < q->x + cat[q->item].w && y >= q->y && y < q->y + cat[q->item].h) return q->id; } return 0; }
static int door_here(int room, int x, int y) { for (int i = 0; i < ndoor; i++) if (doors[i].room == room && doors[i].tx == x && y == doors[i].ty + 1) return i; return -1; }
/* overmap land: an island on a square ocean. Cells inside the rounded rectangle (2..cols-3, 1..rows-2, corners cut) are land, the outermost land ring is beach. Everything else is ocean (blocked). */
static int over_land(const Room *rm, int x, int y) { if (x < 2 || x > rm->cols - 3 || y < 1 || y > rm->rows - 2) return 0; if ((x <= 3 || x >= rm->cols - 4) && (y <= 2 || y >= rm->rows - 3)) return 0; return 1; }
static int over_beach(const Room *rm, int x, int y) { return over_land(rm, x, y) && (!over_land(rm, x - 1, y) || !over_land(rm, x + 1, y) || !over_land(rm, x, y - 1) || !over_land(rm, x, y + 1)); }
#elif PET_SECTION == 34      /* core line 333 */
    static char m[120]; Item *d = &cat[item]; g_gr = room;
#elif PET_SECTION == 35      /* core line 351 */
static int first_free(int room, int item, int *ox, int *oy) { Item *d = &cat[item]; g_gr = room; if (d->wall) { int wy = WALL_ROW + 1 - d->h; for (int x = 0; x < COLS; x++) if (!check_place(room, item, x, wy, 0)[0]) { *ox = x; *oy = wy; return 1; } return 0; } for (int y = TRIG_ROW; y < ROWS; y++) for (int x = 0; x < COLS; x++) if (!check_place(room, item, x, y, 0)[0]) { *ox = x; *oy = y; return 1; } return 0; }
#elif PET_SECTION == 36      /* core line 356 */
    if (nsheet >= 24) return NULL;
#elif PET_SECTION == 37      /* core line 360 */
static void blit(Sheet *s, int sx, int sy, int w, int h, int dx, int dy) {      /* w,h = source px (48 px tiles); drawn T/S48 times the size (nearest) */
    if (!s) return; int dw = w * T / S48, dh = h * T / S48; if (dw < 0 || dh < 0 || dw > 4 * FW || dh > 4 * FH) return;      /* HARDENING: a size from a data file never makes this loop for ever */
    for (int y = 0; y < dh; y++) { int Y = dy + y, srcy = sy + y * S48 / T; if (Y < 0 || Y >= FH || srcy >= s->h) continue;
        for (int x = 0; x < dw; x++) { int X = dx + x, srcx = sx + x * S48 / T; if (X < 0 || X >= FW || srcx >= s->w) continue;
            unsigned char *a = s->px + ((size_t)srcy * s->w + srcx) * 4, *b = fr + ((size_t)Y * FW + X) * 4; int al = a[3]; if (!al) continue;
#elif PET_SECTION == 38      /* core line 367 */
static void tile(const char *sh, int c, int r, int w, int h, int tx, int ty) { blit(sheet("tilesets", sh), c * S48, r * S48, w * S48, h * S48, OX + tx * T, OY + ty * T); }

static Sheet *ohr_sheet(const char *file) {
    for (int i = 0; i < nsheet; i++) if (!strcmp(sheets[i].name, file)) return &sheets[i];
    if (nsheet >= 24) return NULL;
    char p[PATH_MAX]; snprintf(p, sizeof p, "%s/../ohrrpgce-tiles/%s", ASSETS, file); int n; Sheet *s = &sheets[nsheet];
    s->px = stbi_load(p, &s->w, &s->h, &n, 4); if (!s->px) return NULL; snprintf(s->name, 40, "%s", file); nsheet++; return s;
}
/* 20 px OHR cells. Magenta (the sheet's empty colour) is skipped. tr,tg,tb recolor the pixel (255 = unchanged). */
static void blit_box(Sheet *s, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh, int tr, int tg, int tb) {
    if (!s || sw < 1 || sh < 1 || dw < 1 || dh < 1) return;
    for (int y = 0; y < dh; y++) { int Y = dy + y, srcy = sy + y * sh / dh; if (Y < 0 || Y >= FH || srcy < 0 || srcy >= s->h) continue;
        for (int x = 0; x < dw; x++) { int X = dx + x, srcx = sx + x * sw / dw; if (X < 0 || X >= FW || srcx < 0 || srcx >= s->w) continue;
            unsigned char *a = s->px + ((size_t)srcy * s->w + srcx) * 4, *b = fr + ((size_t)Y * FW + X) * 4; int al = a[3]; if (!al) continue;
            if (a[0] > 240 && a[2] > 240 && a[1] < 40) continue;
            int r = a[0] * tr / 255, g = a[1] * tg / 255, bl = a[2] * tb / 255;
            b[0] = (unsigned char)((r * al + b[0] * (255 - al)) / 255); b[1] = (unsigned char)((g * al + b[1] * (255 - al)) / 255); b[2] = (unsigned char)((bl * al + b[2] * (255 - al)) / 255); b[3] = 255; } }
}
static void ohr_at(const char *file, int c, int r, int tx, int ty) { blit_box(ohr_sheet(file), c * 20, r * 20, 20, 20, OX + tx * T, OY + ty * T, T, T, 255, 255, 255); }
static const char *ohr_file(const char *sh) {
    if (!strcmp(sh, "house")) return "320px-TS07_house.bmp.png";
    if (!strcmp(sh, "fantasy")) return "320px-Fantasy_tileset0FINISHED.bmp.png";
    if (!strcmp(sh, "viking")) return "320px-Vikings_tilemaps_000.bmp.png";
    if (!strcmp(sh, "town")) return "320px-TS02_town.bmp.png";
    if (!strcmp(sh, "castle")) return "320px-TS10_castle.bmp.png";
    if (!strcmp(sh, "forest")) return "320px-BG02_forest-bmp.png";
    if (!strcmp(sh, "grass")) return "320px-BG01_grass-bmp.png";
    return NULL;
}
static void paint_cells(const char *sh, int c, int r, int w, int h, int tx, int ty) {
    const char *f = ohr_file(sh);
    if (!f) { tile(sh, c, r, w, h, tx, ty); return; }
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) ohr_at(f, c + i, r + j, tx + i, ty + j);
}
static void paint_scen(const Scen *s) {
    const char *town = "320px-TS02_town.bmp.png";
    if (!strcmp(s->sheet, "Outside_B")) {
        for (int j = 0; j < s->h && j < 2; j++) ohr_at(town, 14, 3 + j, s->x, s->y + j);
        return;
    }
    if (!strcmp(s->sheet, "World_B")) {
        /* Neighbors on the town sheet, copied as that block. Stone keep, or roof over windows. */
        static const int keep[2][2][2] = {{{8, 0}, {9, 0}}, {{8, 2}, {9, 2}}};
        static const int house[2][2][2] = {{{11, 3}, {12, 3}}, {{11, 4}, {12, 4}}};
        const int (*b)[2][2] = s->y < 4 ? keep : house;
        for (int j = 0; j < s->h && j < 2; j++) for (int i = 0; i < s->w && i < 2; i++) ohr_at(town, b[j][i][0], b[j][i][1], s->x + i, s->y + j);
        return;
    }
    tile(s->sheet, s->c, s->r, s->w, s->h, s->x, s->y);
}

/* RPG Maker autotiles (verified against the tsots maps: tile id = kind * 48 + shape, shape picked from the 8 neighbours of the same kind; all 8 present = shape 0; the 48 shapes are 4 quadrant pieces).
 * Same rule as _.monads/_.livedesk-taskbar/ops/tile_autotile.c's FLOOR_AUTOTILE_TABLE, computed per quadrant: for each corner look at its two adjacent sides and its diagonal. Pieces are 24 px
 * halves inside the kind's 2 x 3 tile block (bx,by = the block's top-left tile in the sheet). */
static void draw_auto(const char *sh, int bx, int by, int n, int e, int s, int w, int ne, int se, int sw, int nw, int dx, int dy) {
    Sheet *sp = sheet("tilesets", sh); int q[4][2];
    if (!n && !e && !s && !w) { int iso[4][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}}; memcpy(q, iso, sizeof q); }
    else {
        #define PICK(i, a, b, d, bothA, bothB, onlyA_, onlyB_, none_) { const int *p_ = ((a) && (b)) ? ((d) ? bothA : bothB) : (a) ? onlyA_ : (b) ? onlyB_ : none_; q[i][0] = p_[0]; q[i][1] = p_[1]; }
        static const int TLf[2] = {2, 4}, TLn[2] = {2, 0}, TLw[2] = {2, 2}, TLm[2] = {0, 4}, TLx[2] = {0, 2};      /* flat, notch (diagonal missing), N missing, W missing, both missing */
        static const int TRf[2] = {1, 4}, TRn[2] = {3, 0}, TRw[2] = {1, 2}, TRm[2] = {3, 4}, TRx[2] = {3, 2};
        static const int BLf[2] = {2, 3}, BLn[2] = {2, 1}, BLw[2] = {2, 5}, BLm[2] = {0, 3}, BLx[2] = {0, 5};
        static const int BRf[2] = {1, 3}, BRn[2] = {3, 1}, BRw[2] = {1, 5}, BRm[2] = {3, 3}, BRx[2] = {3, 5};
        PICK(0, n, w, nw, TLf, TLn, TLw, TLm, TLx)      /* "only A" = the first side present, i.e. the other side (W) is missing */
        PICK(1, n, e, ne, TRf, TRn, TRw, TRm, TRx)
        PICK(2, s, w, sw, BLf, BLn, BLw, BLm, BLx)
        PICK(3, s, e, se, BRf, BRn, BRw, BRm, BRx)
        #undef PICK
        /* PICK(a present only) means the OTHER side is missing: swap to the table's meaning (TLm = W missing, TLw = N missing) */
        if (n && !w) { q[0][0] = TLm[0]; q[0][1] = TLm[1]; } else if (!n && w) { q[0][0] = TLw[0]; q[0][1] = TLw[1]; }
        if (n && !e) { q[1][0] = TRm[0]; q[1][1] = TRm[1]; } else if (!n && e) { q[1][0] = TRw[0]; q[1][1] = TRw[1]; }
        if (s && !w) { q[2][0] = BLm[0]; q[2][1] = BLm[1]; } else if (!s && w) { q[2][0] = BLw[0]; q[2][1] = BLw[1]; }
        if (s && !e) { q[3][0] = BRm[0]; q[3][1] = BRm[1]; } else if (!s && e) { q[3][0] = BRw[0]; q[3][1] = BRw[1]; }
    }
    for (int i = 0; i < 4; i++) blit(sp, bx * S48 + q[i][0] * 24, by * S48 + q[i][1] * 24, 24, 24, dx + (i % 2) * T / 2, dy + (i / 2) * T / 2);
}
/* the overmap biome "worldmap": the whole map is grass (World_A2 kind 0), the ocean is the World_A1 sea autotile (kind 0) whose edge shapes draw the shore wherever it meets land; the island is whatever over_land() says. */
static int over_sea(const Room *rm, int x, int y) { if (x < 0 || y < 0 || x >= rm->cols || y >= rm->rows) return 1; return !over_land(rm, x, y); }      /* outside the map counts as sea (MV treats the border as connected) */
static int sea_beside_land(const Room *rm, int x, int y) {
    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) if (!over_sea(rm, x + dx, y + dy)) return 1; return 0;
}
static void draw_over(const Room *rm) {
    for (int y = 0; y < rm->rows; y++) for (int x = 0; x < rm->cols; x++) {
        if (!over_sea(rm, x, y)) ohr_at("320px-TS02_town.bmp.png", 1, 0, x, y);
        else if (sea_beside_land(rm, x, y)) ohr_at("320px-TS02_town.bmp.png", 3, 1, x, y);
        else ohr_at("320px-TS02_town.bmp.png", 1, 7, x, y);
    }
}
#elif PET_SECTION == 39      /* core line 456 */
    g_gr = vroom;
#elif PET_SECTION == 40      /* core line 458 */
    Room *rm = &rooms[vroom]; char phase[40] = "day"; { char dp[PATH_MAX], db[64]; spath(dp, "daylight.txt"); read_file(dp, db, sizeof db); if (strstr(db, "night")) snprintf(phase, sizeof phase, "night"); else if (strstr(db, "dusk")) snprintf(phase, sizeof phase, "dusk"); else if (strstr(db, "dawn")) snprintf(phase, sizeof phase, "dawn"); }
    if (rm->over) draw_over(rm); else {
        int castle = !strcmp(rm->id, "castle");
        const char *town = "320px-TS02_town.bmp.png";
        for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLS; x++) {
            if (y == 0) ohr_at(town, 8, 0, x, y);
            else if (y == WALL_ROW) ohr_at(town, castle ? 8 : 8, castle ? 7 : 2, x, y);
            else ohr_at(town, castle ? 7 : 3, castle ? 8 : 1, x, y);
        }
    }
    for (int i = 0; i < ndoor; i++) if (doors[i].room == vroom && !rm->over) { ohr_at("320px-TS02_town.bmp.png", 8, 6, doors[i].tx, doors[i].ty); if (!rm->outdoor) ohr_at("320px-TS02_town.bmp.png", 3, 1, doors[i].tx, doors[i].ty + 1); }
    for (int i = 0; i < nscen; i++) if (scen[i].room == vroom) paint_scen(&scen[i]);
#elif PET_SECTION == 41      /* core line 472 */
    for (int a = 0; a < n; a++) { Placed *p = &pl[idx[a]]; Item *d = &cat[p->item]; paint_cells(d->sheet, d->c, d->r, d->w, d->h, p->x, p->y); }
#elif PET_SECTION == 42      /* core line 479 */
    for (int k = 0; k < (g_bg_only ? 0 : no); k++) { int i = order[k]; Ent *e = i < 0 ? &tr : &others[i]; int si = i < 0 ? hero.idx : party[i].idx; if (si < 0) si = 0; if (si > 2) si = 2;
        static const int tn[3][3] = {{255, 70, 70}, {80, 140, 255}, {80, 220, 90}};
        blit_box(ohr_sheet("320px-Vikings_tilemaps_000.bmp.png"), 10 * 20, 2 * 20, 20, 40, OX + e->x * T, OY + e->y * T - T / 2, T, T + T / 2, tn[si][0], tn[si][1], tn[si][2]); }
#elif PET_SECTION == 43      /* core line 484 */
        if (tr_ || tg_ || tb_ || gy) for (int y = OY + ((od && !rm->over) ? 2 * T : 0); y < OY + ROWS * T; y++) for (int x = OX; x < OX + COLS * T; x++) { unsigned char *q = fr + ((size_t)y * FW + x) * 4; int r = q[0], g = q[1], b = q[2], l = (r * 30 + g * 59 + b * 11) / 100;
#elif PET_SECTION == 44      /* core line 489 */
      if (mr != 256 || mg != 256 || mb != 256) for (int y = OY + ((od && !rm->over) ? 2 * T : 0); y < OY + ROWS * T; y++) for (int x = OX; x < OX + COLS * T; x++) { unsigned char *q = fr + ((size_t)y * FW + x) * 4; q[0] = (unsigned char)(q[0] * mr / 256); q[1] = (unsigned char)(q[1] * mg / 256); q[2] = (unsigned char)(q[2] * mb / 256); } } }
#elif PET_SECTION == 45      /* core line 504 */
#elif PET_SECTION == 46      /* core line 513 */
    { static const char *dflt[4] = {"hmm, tell me more.", "I'm listening.", "interesting!", "okay."}; unsigned h = 0; for (const char *c = s; *c; c++) h = h * 31 + (unsigned char)*c;
#elif PET_SECTION == 47      /* core line 594 */
    if (pid == 0) { int fd = open("/dev/null", O_RDWR); if (fd >= 0) { dup2(fd, 0); dup2(fd, 1); dup2(fd, 2); } setenv("LC_CLOCK_NO_POPUP", "1", 1); if (daemon) setenv("LC_CLOCK_EVENT_RUNNER", SELF, 1); setenv("DSR_RPG_ROOT", APP, 1); signal(SIGPIPE, SIG_DFL); execv(LC, av); _exit(127); }
#elif PET_SECTION == 48      /* core line 612 */
#elif PET_SECTION == 49      /* core line 613 */
#elif PET_SECTION == 50      /* core line 647 */
/* a pinned bar's text rows are one line high and never grow (the layout counts one row per text): long ui values are also written as <key>_0 .. <key>_1, split at spaces into pieces of at most 26 characters (2026-10-17) */
static int ui_wrap_keys(char *u, int o, size_t cap) {
    static const char *keys[] = { "nick_line", "friend_line", NULL };
    for (int k = 0; keys[k]; k++) {
        char pat[40]; snprintf(pat, sizeof pat, "%s=", keys[k]); char *s = strstr(u, pat); if (!s || (s != u && s[-1] != '\n')) continue;
        s += strlen(pat); char val[200]; size_t n = strcspn(s, "\n"); if (n >= sizeof val) n = sizeof val - 1; memcpy(val, s, n); val[n] = 0;
        char *p = val;
        for (int r = 0; r < 3; r++) { size_t len = strlen(p), cut = len; if (len > 26) { cut = 26; while (cut > 8 && p[cut] != ' ') cut--; if (p[cut] != ' ') cut = 26; }
            char piece[40]; snprintf(piece, sizeof piece, "%.*s", (int)cut, p); p += cut; while (*p == ' ') p++;
            if (o + 60 < (int)cap) o += snprintf(u + o, cap - o, "%s_%d=%s\n", keys[k], r, piece); }
    }
    return o;
}
#elif PET_SECTION == 51      /* core line 662 */
    char p[PATH_MAX]; spath(p, "scene.raw"); if (write_if_changed(p, fr, sizeof fr)) g_scene_ver++;
#elif PET_SECTION == 52      /* core line 667 */
    int chat = flag_on("chat", 0), hb = flag_on("hb", 0), in = flag_on("int", 0); char view[16]; flag_str("view", "2d", view, sizeof view); for (char *c = view; *c; c++) if (*c >= 'a' && *c <= 'z') *c -= 32;
    W("title=civ-rpgce  -  %s  -  coins %d\nscene_raw=%s\ncanvas_raw=%s\ncoins=%d\nmsg=%s\nroom_label=%s\n", rooms[vroom].name, coins, raw, raw, coins, msg && msg[0] ? msg : (in ? "INT on: turn based like a roguelike. Time moves when you act. Tab switches pet/player." : "real time: the pets live on their own. INT lets you control a pet or the player."), rooms[vroom].name);
#elif PET_SECTION == 53      /* core line 672 */
    W("left_visible=%s\n", flag_on("left", 1) ? "1" : "");      /* the pinned info bar (default on, Menu > Info show / hide, its _ button) */
    W("chat_visible=%s\nhb_visible=%s\nview_label=view %s\nchat_toggle_label=window %s\n", chat ? "1" : "", hb ? "1" : "", view, chat ? "on" : "off");
    { char cp[PATH_MAX], cb[4000]; spath(cp, "chat.txt"); read_tail(cp, cb, sizeof cb); char *ln[6] = {0}; int k = 0; for (char *l = strtok(cb, "\n"); l; l = strtok(NULL, "\n")) { ln[k % 6] = l; k++; } for (int i = 0; i < 6; i++) { int j = k - 6 + i; W("chat_%d=%s\n", i, j >= 0 ? ln[j % 6] : ""); } }
#elif PET_SECTION == 54      /* core line 676 */
#elif PET_SECTION == 55      /* core line 688 */
#elif PET_SECTION == 56      /* core line 696 */
static int passable(int x, int y) { g_gr = pet.room; return x >= 0 && x < COLS && y >= TRIG_ROW && y < ROWS && !floor_blocked(pet.room, x, y, 0) && !other_at(pet.room, x, y); }
#elif PET_SECTION == 57      /* core line 700 */
    pet.room = d->dest; g_gr = pet.room; pet.x = d->ax; pet.y = d->ay; pet.dir = 0; pet.tx = pet.ty = -1; { int fx, fy; if (free_near(pet.x, pet.y, &fx, &fy)) { pet.x = fx; pet.y = fy; } }      /* land on the nearest FREE cell, not on whoever is already there */
    if (me >= 0 && me == active && (pet.room == vroom || di_from_room == vroom)) { snprintf(b, sizeof b, "went through the door to the %s", rooms[pet.room].name); say(party[me].name, b); }
#elif PET_SECTION == 58      /* core line 706 */
    g_gr = pet.room;
#elif PET_SECTION == 59      /* core line 717 */
    g_gr = pet.room;
#elif PET_SECTION == 60      /* core line 722 */
#elif PET_SECTION == 61      /* core line 726 */
    g_gr = pet.room; int dist[MAXR][MAXC], qx[MAXR * MAXC], qy[MAXR * MAXC], h = 0, t = 0; for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLS; x++) dist[y][x] = -1;
    int fx[MAXR][MAXC], fy[MAXR][MAXC]; dist[ty][tx] = 0; qx[t] = tx; qy[t++] = ty; static const int mx[4] = {0, -1, 1, 0}, my[4] = {1, 0, 0, -1};
#elif PET_SECTION == 62      /* core line 734 */
    g_gr = pet.room; int r = rand() % 100;
#elif PET_SECTION == 63      /* core line 737 */
        else { int pick = -1; for (int i = 0; i < ndoor; i++) if (doors[i].room == pet.room && doors[i].autow && doors[i].dest == others[j].room) pick = i; if (pick < 0) for (int i = 0; i < ndoor; i++) if (doors[i].room == pet.room && doors[i].autow) pick = i; if (pick >= 0) { pet.tx = doors[pick].tx; pet.ty = doors[pick].ty + 1; return; } } }
    if (r < 47) { int cand[8], nc = 0; for (int i = 0; i < ndoor; i++) if (doors[i].room == pet.room && doors[i].autow && nc < 8) cand[nc++] = i; if (nc) { Door *d = &doors[cand[rand() % nc]]; pet.tx = d->tx; pet.ty = d->ty + 1; return; } }
#elif PET_SECTION == 64      /* core line 742 */
#elif PET_SECTION == 65      /* core line 743 */
#elif PET_SECTION == 66      /* core line 744 */
#elif PET_SECTION == 67      /* core line 745 */
#elif PET_SECTION == 68      /* core line 746 */
    if (pet.tx < 0 && rand() % 100 < 45) pick_target();
#elif PET_SECTION == 69      /* core line 752 */
#elif PET_SECTION == 70      /* core line 753 */
static void end_turn(void) { if (!run_on() || !turn_mode()) return; clock_turn(); int in = flag_on("int", 0), c = ctl_idx(); for (int i = 0; i < nparty; i++) if (!(in && i == c)) ai_step(i); use_ent(active); }      /* the pet you control does not act on its own */
#elif PET_SECTION == 71      /* core line 763 */

static void ohr24(const char *file, int c, int r, unsigned char *out) {
    Sheet *s = ohr_sheet(file); memset(out, 0, XT * XT * 4); if (!s) return; int sx = c * 20, sy = r * 20;
    for (int y = 0; y < XT; y++) for (int x = 0; x < XT; x++) { int px = sx + x * 20 / XT, py = sy + y * 20 / XT; if (px >= 0 && py >= 0 && px < s->w && py < s->h) memcpy(out + (y * XT + x) * 4, s->px + ((size_t)py * s->w + px) * 4, 4); }
}
#elif PET_SECTION == 72      /* core line 782 */
    if (!strcmp(sh, "viking")) {
        Sheet *s = ohr_sheet("320px-Vikings_tilemaps_000.bmp.png"); if (!s) return; char d[PATH_MAX], p[PATH_MAX]; snprintf(d, sizeof d, "%s/../tsots-characters", ASSETS); mkdir(d, 0755); snprintf(d, sizeof d, "%s/../tsots-characters/frames", ASSETS); mkdir(d, 0755);
        static const int dc[4] = {2, 4, 6, 8}; snprintf(p, sizeof p, "%s/viking_%d_%d_%d.rgba", d, idx, dc[dir4 & 3], pat); if (fexists(p)) return;
        static const int tn[3][3] = {{255, 70, 70}, {80, 140, 255}, {80, 220, 90}}; int si = idx < 0 ? 0 : idx > 2 ? 2 : idx; unsigned char out[16 * 24 * 4];
        for (int y = 0; y < 24; y++) for (int x = 0; x < 16; x++) { int px = 10 * 20 + x * 20 / 16, py = 2 * 20 + y * 40 / 24; unsigned char *a = s->px + ((size_t)py * s->w + px) * 4; unsigned char *o = out + (y * 16 + x) * 4;
            if (px >= s->w || py >= s->h || (a[0] > 240 && a[2] > 240 && a[1] < 40)) { o[0] = o[1] = o[2] = o[3] = 0; continue; }
            o[0] = (unsigned char)(a[0] * tn[si][0] / 255); o[1] = (unsigned char)(a[1] * tn[si][1] / 255); o[2] = (unsigned char)(a[2] * tn[si][2] / 255); o[3] = a[3]; }
        FILE *f = fopen(p, "wb"); if (f) { fwrite(out, 1, sizeof out, f); fclose(f); } return;
    }
#elif PET_SECTION == 73      /* core line 795 */
#elif PET_SECTION == 74      /* core line 799 */
    if (!s) return; int fx = (idx % 4) * 3 * S48 + S48 + S48 / 2, fy = (idx / 4) * 4 * S48 + dir4 * S48 + S48 / 2; if (fx < s->w && fy < s->h) memcpy(px, s->px + ((size_t)fy * s->w + fx) * 4, 4);
#elif PET_SECTION == 75      /* core line 806 */
    static unsigned char crop[FW * FH * 4]; unsigned long sum = 5381; for (int y = 0; y < ROWS * T; y++) memcpy(crop + (size_t)y * COLS * T * 4, fr + ((size_t)(OY + y) * FW + OX) * 4, COLS * T * 4);
#elif PET_SECTION == 76      /* core line 808 */
    if (r >= 0 && r < 16 && sum != last[r]) { last[r] = sum; char d[PATH_MAX], p[PATH_MAX]; snprintf(d, sizeof d, "%s/pieces/system/maps/civ-rpgce/%s", PCHQ, rooms[r].id); mkdir(d, 0755); snprintf(p, sizeof p, "%s/map.png", d); png_atomic(p, COLS * T, ROWS * T, crop, COLS * T * 4); }
#elif PET_SECTION == 77      /* core line 832 */
    snprintf(d, sizeof d, "%s/pieces/system/maps/civ-rpgce/%s", PCHQ, rooms[r].id); mkdir(d, 0755);
    for (int i = 0; i < nparty; i++) if (others[i].room == r) { int pat = others[i].steps % 3; write_frame(party[i].sheet, party[i].idx, others[i].dir, pat); unsigned char px[4] = {128, 128, 128, 255}; static const int tn[3][3] = {{255, 70, 70}, {80, 140, 255}, {80, 220, 90}}; int si = party[i].idx; if (si < 0) si = 0; if (si > 2) si = 2; if (!strcmp(party[i].sheet, "viking")) { px[0] = (unsigned char)tn[si][0]; px[1] = (unsigned char)tn[si][1]; px[2] = (unsigned char)tn[si][2]; } else frame_rgb(sheet("characters", party[i].sheet), party[i].idx, others[i].dir & 3, px);
        o += snprintf(buf + o, sizeof buf - o, "%d %d %d %d %d %s %d %d %d %c\n", others[i].x, others[i].y, px[0], px[1], px[2], party[i].sheet, party[i].idx, dc[others[i].dir & 3], pat, party[i].name[0] >= 'A' && party[i].name[0] <= 'Z' ? party[i].name[0] + 32 : (party[i].name[0] ? party[i].name[0] : 'm')); }
    if (tr.room == r) { int pat = tr.steps % 3; write_frame(hero.sheet, hero.idx, tr.dir, pat); unsigned char px[4] = {255, 70, 70, 255}; if (strcmp(hero.sheet, "viking")) frame_rgb(sheet("characters", hero.sheet), hero.idx, tr.dir & 3, px);
        o += snprintf(buf + o, sizeof buf - o, "%d %d %d %d %d %s %d %d %d @\n", tr.x, tr.y, px[0], px[1], px[2], hero.sheet, hero.idx, dc[tr.dir & 3], pat); }
#elif PET_SECTION == 78      /* core line 840 */
    static const char leg[] = "W|3|150|150|160|-|wall|#\nD|3|170|110|50|-|door|+\nT|2|50|150|60|-|tree|T\nf|1|190|150|90|-|furniture|&\n.|0|90|80|60|-|floor|.\n";
#elif PET_SECTION == 79      /* core line 842 */
    { char bp[PATH_MAX]; snprintf(bp, sizeof bp, "%s/pieces/system/maps/civ-rpgce/view.pdl", PCHQ); char vw[PATH_MAX + 160]; char sc[PATH_MAX]; spath(sc, "scene.raw"); int vn = snprintf(vw, sizeof vw, "VIEW | cell_px | 48\nVIEW | default_style | scene\nVIEW | scene_raw | %s\nVIEW | scene_w | %d\nVIEW | scene_h | %d\n", sc, FW, FH); write_if_changed(bp, vw, (size_t)vn); }      /* the engine draws this book's 2D at civ-rpgce's own 48 px tile (house desk_grid.pdl says 80) */
#elif PET_SECTION == 80      /* core line 868 */
static int viewer_kinds(void) {
    /* file mode: scan every /proc/<pid>/cmdline each viewer_scan_ms (the old behaviour; ~200 opens/s). marker/shmem: remember the window pids found, check only that they are alive (kill 0), and rescan /proc only when a
     * launcher appended to the process ledger (livedesk_proc_list.txt grew) or every 30 s as a safety net: a window opening still renders at once, an idle desk stops reading 400 cmdlines every 2 s. */
    static long last = 0; static int v = 3; static int wpid[8], wflag[8], nw = 0; static long ledger_seen = -1; long t = now_ms();
    if (sn_active(&g_sn)) {
        static long lt = 0; static int grew = 0; if (t - lt >= 250) { lt = t; char lp[PATH_MAX]; snprintf(lp, sizeof lp, "%s/#.desktop/livedesk_proc_list.txt", HOUSE); long ls = fsize(lp); grew = ls != ledger_seen; ledger_seen = ls; }
        if (last && !grew && t - last < tune("viewer_safety_ms", 30000)) { int nv = 0, k = 0; for (int i = 0; i < nw; i++) if (kill(wpid[i], 0) == 0) { wpid[k] = wpid[i]; wflag[k] = wflag[i]; nv |= wflag[i]; k++; } nw = k; v = nv; return v; }
    } else if (last && t - last < tune("viewer_scan_ms", 2000)) return v;
    last = t; v = 0; nw = 0;
#elif PET_SECTION == 81      /* core line 879 */
        if (!strstr(b, "khtpm_core_render")) continue; for (size_t i = 0; i < n; i++) if (!b[i]) b[i] = ' '; int fl = 0; if (strstr(b, "pchq-board.xhtpm") && !strstr(b, SIBLING_APP)) fl |= 1; if (strstr(b, "civ-rpgce.xhtpm")) fl |= 2;
        if (fl) { v |= fl; if (nw < 8) { wpid[nw] = atoi(e->d_name); wflag[nw] = fl; nw++; } } }
#elif PET_SECTION == 82      /* core line 896 */
#elif PET_SECTION == 83      /* core line 897 */
    char wp[PATH_MAX], xp[PATH_MAX], hp[PATH_MAX], m[64] = "", dk[64] = ""; snprintf(wp, sizeof wp, "%s/pieces/world_01/state.txt", PCHQ);
#elif PET_SECTION == 84      /* core line 902 */
#elif PET_SECTION == 85      /* core line 903 */
    snprintf(xp, sizeof xp, "%s/pieces/xelector_01/state.txt", PCHQ); snprintf(hp, sizeof hp, "%s/pieces/hero_01/state.txt", PCHQ);
#elif PET_SECTION == 86      /* core line 908 */
    if (strcmp(dk, rooms[e->room].id) && lw != e->room) { lw = e->room; char ib[PATH_MAX], c[96]; snprintf(ib, sizeof ib, "%s/pieces/system/widget_cmds/inbox.txt", PCHQ); snprintf(c, sizeof c, "CONFIRM_SET_DESK:%s", rooms[e->room].id); FILE *f = fopen(ib, "a"); if (f) { fprintf(f, "%s\n", c); fclose(f); } changed = 1; }
#elif PET_SECTION == 87      /* core line 921 */
    char root[PATH_MAX], d[PATH_MAX], p[PATH_MAX]; snprintf(root, sizeof root, "%s/pieces/system/maps/civ-rpgce", PCHQ);
    { char a[PATH_MAX]; snprintf(a, sizeof a, "%s/pieces/system/maps", PCHQ); mkdir(a, 0755); } mkdir(root, 0755);
#elif PET_SECTION == 88      /* core line 927 */
        g_gr = r; char glyph[MAXR][MAXC + 1]; int fs[MAXR][MAXC], ws[MAXR][MAXC]; unsigned char a[XT * XT * 4], b[XT * XT * 4], fl[XT * XT * 4]; nslot = 1; memset(slotpx[0], 0, XT * XT * 4);
        Room *rm = &rooms[r];
#elif PET_SECTION == 89      /* core line 930 */
        { int castle = !strcmp(rm->id, "castle"); const char *town = "320px-TS02_town.bmp.png";
          ohr24(town, castle ? 7 : 3, castle ? 8 : 1, floorpx); ohr24(town, 8, castle ? 7 : 2, wallpx); ohr24(town, 8, 0, ceilpx); memcpy(pathpx, floorpx, sizeof pathpx); ohr24(town, 8, 6, doorpx); memcpy(matpx, floorpx, sizeof matpx); }
#elif PET_SECTION == 90      /* core line 933 */
        if (!rm->over) for (int x = 0; x < COLS; x++) { glyph[0][x] = glyph[1][x] = 'W'; ws[0][x] = slot_of(ceilpx); ws[1][x] = slot_of(wallpx); }
#elif PET_SECTION == 91      /* core line 935 */
        for (int i = 0; i < ndoor; i++) if (doors[i].room == r) { int tx = doors[i].tx; memcpy(a, wallpx, sizeof a); over24(a, doorpx); glyph[doors[i].ty][tx] = 'D'; ws[doors[i].ty][tx] = slot_of(a);
            if (rm->over) { } else if (rm->outdoor) { for (int y = TRIG_ROW; y < ROWS - 1; y++) fs[y][tx] = slot_of(pathpx); } else fs[doors[i].ty + 1][tx] = slot_of(matpx); }
#elif PET_SECTION == 92      /* core line 938 */
            Sheet *s = sheet("tilesets", scen[i].sheet); memcpy(b, floorpx, sizeof b); tile24(s, (scen[i].c + ci) * S48, (scen[i].r + cj) * S48, a); over24(b, a); glyph[y][x] = scen[i].h >= 2 ? 'T' : 'f'; ws[y][x] = slot_of(b); }
        for (int i = 0; i < npl; i++) if (pl[i].room == r) { Item *it = &cat[pl[i].item]; const char *of = ohr_file(it->sheet); Sheet *s = of ? NULL : sheet("tilesets", it->sheet);
#elif PET_SECTION == 93      /* core line 941 */
                if (of) ohr24(of, it->c + ci, it->r + cj, a); else tile24(s, (it->c + ci) * S48, (it->r + cj) * S48, a);
#elif PET_SECTION == 94      /* core line 943 */
        if (rm->over) {      /* the overmap 3D: every cell = the painted 2D cell downscaled to a 24 px tile; the buildings (scenery) are 2 high blocks, the ocean is a flat floor */
            int sv = vroom; vroom = r; g_bg_only = 1; render(); g_bg_only = 0; vroom = sv; g_gr = r;
            for (int y = 0; y < rm->rows; y++) for (int x = 0; x < rm->cols; x++) { unsigned char cellpx[XT * XT * 4];
                for (int yy = 0; yy < XT; yy++) for (int xx = 0; xx < XT; xx++) { int sx = OX + x * T + xx * T / XT, sy = OY + y * T + yy * T / XT; memcpy(cellpx + (yy * XT + xx) * 4, fr + ((size_t)sy * FW + sx) * 4, 4); }
                int blocked = 0; for (int i = 0; i < nscen; i++) if (scen[i].room == r && x >= scen[i].x && x < scen[i].x + scen[i].w && y >= scen[i].y && y < scen[i].y + scen[i].h) blocked = 1;
                glyph[y][x] = blocked ? 'T' : '.'; fs[y][x] = slot_of(cellpx); ws[y][x] = 0; } }
#elif PET_SECTION == 95      /* core line 951 */
        snprintf(p, sizeof p, "%s/extrusion.pdl", d); { FILE *f = fopen_tmp(p); if (f) { fprintf(f, "SECTION      | KEY                | VALUE\n----------------------------------------------------------------------\nMETA         | map_id             | civ-rpgce\nMETA         | desk_id            | %s\nMETA         | note               | %s (civ-rpgce house, exported from the live house)\nEXTRUDE      | W                  | 3\nEXTRUDE      | D                  | 3\nEXTRUDE      | T                  | 2\nEXTRUDE      | f                  | 1\nEXTRUDE      | default            | 0\n", rooms[r].id, rooms[r].name); fclose_tmp(f, p); } }
#elif PET_SECTION == 96      /* core line 964 */
static int copy_file(const char *from, const char *to) { FILE *in = fopen(from, "rb"); if (!in) return 0; char tmpc[PATH_MAX + 24]; snprintf(tmpc, sizeof tmpc, "%s.tmp.%d", to, (int)getpid()); FILE *out = fopen(tmpc, "wb"); if (!out) { fclose(in); return 0; } char buf[8192]; size_t n; int ok = 1; while ((n = fread(buf, 1, sizeof buf, in)) > 0) if (fwrite(buf, 1, n, out) != n) { ok = 0; break; } fclose(in); if (fclose(out) != 0) ok = 0; if (ok && rename(tmpc, to) != 0) ok = 0; if (!ok) remove(tmpc); return ok; }
#elif PET_SECTION == 97      /* core line 968 */
#elif PET_SECTION == 98      /* core line 976 */
    if (!strcmp(v, "newgame")) {      /* a fresh game: the current one is saved to slot 0 first (Undo new game = Load slot 0), then every game file is removed so the next read starts from the defaults. The clock is not touched. */
#elif PET_SECTION == 99      /* core line 979 */
        static const char *fixed[] = { "house.txt", "trainer.txt", "relations.txt", "chat.txt", "active.txt", "daylight.txt", "flag_ctl.txt", "flag_follow.txt", "events.txt" };
#elif PET_SECTION == 100      /* core line 982 */
        pet_load(); replay();
        snprintf(msg, cap, "new game started; the game you left is in slot 0 (Player > Undo new game)"); return; }
    if (!strcmp(v, "reset")) { char p[PATH_MAX], q[PATH_MAX]; spath(p, "house.txt"); snprintf(q, sizeof q, "%s/house.txt.%ld.bak", ST, now); if (fexists(p)) rename(p, q); snprintf(msg, cap, "house reset (the old ledger is kept as house.txt.%ld.bak)", now); return; }
#elif PET_SECTION == 101      /* core line 1002 */
        char p[PATH_MAX], bb[40]; spath(p, "active.txt"); snprintf(bb, sizeof bb, "%s\n", party[k].id); write_if_changed(p, bb, strlen(bb)); set_flag("follow", "pet"); set_flag("ctl", "pet"); active = k; calc_view(); snprintf(msg, cap, "watching %s (in the %s)", party[k].name, rooms[others[k].room].name); return; }
#elif PET_SECTION == 102      /* core line 1024 */
        if (flag_on("int", 0)) { int ci = ctl_idx(); use_ent(ci); pet.room = r; g_gr = r; pet.dir = 0; pet.tx = pet.ty = -1; int placed = 0;
#elif PET_SECTION == 103      /* core line 1030 */
#elif PET_SECTION == 104      /* core line 1039 */
        char names[40][48]; int nn = 0; snprintf(names[nn++], 48, "house.txt"); snprintf(names[nn++], 48, "trainer.txt"); snprintf(names[nn++], 48, "relations.txt"); snprintf(names[nn++], 48, "chat.txt"); snprintf(names[nn++], 48, "active.txt"); snprintf(names[nn++], 48, "daylight.txt"); snprintf(names[nn++], 48, "flag_ctl.txt"); snprintf(names[nn++], 48, "flag_follow.txt"); snprintf(names[nn++], 48, "events.txt");      /* events.txt is bigger than the 64 KB read buffer below: streamed */
#elif PET_SECTION == 105      /* core line 1045 */
            if (!strcmp(names[i], "events.txt")) { if (copy_file(a, bpath)) ok++; continue; }
#elif PET_SECTION == 106      /* core line 1047 */
        if (!saving) { char b[40]; read_file(mp, b, sizeof b); long long ms = atoll(b); if (ms > 0) { clock_paths(); clock_install(); char ma[32]; snprintf(ma, sizeof ma, "%lld", ms); lc_run(0, "cmd", "rpg", "settime", ma, "--source", "rpg-load", NULL); lc_run(0, "step", "5", NULL); } sn_bump(&g_sn, SL_PETS); sn_bump(&g_sn, SL_FLAGS); sn_bump(&g_sn, SL_DAY); g_flag_epoch++; pet_load(); }
        snprintf(msg, cap, "%s slot %d (%d files)", saving ? "saved to" : "loaded from", slot, ok); return; }
#elif PET_SECTION == 107      /* core line 1050 */
    if (!strcmp(v, "toggle") && argc >= 2 && (!strcmp(argv[1], "chat") || !strcmp(argv[1], "hb") || !strcmp(argv[1], "voice") || !strcmp(argv[1], "left"))) { set_flag(argv[1], flag_on(argv[1], !strcmp(argv[1], "voice") || !strcmp(argv[1], "left")) ? "0" : "1"); snprintf(msg, cap, "%s toggled", argv[1]); return; }
#elif PET_SECTION == 108      /* core line 1131 */
    char wp[PATH_MAX], wb[2048], m[64] = "", dk[64] = "", xp[PATH_MAX], kb[512]; snprintf(wp, sizeof wp, "%s/pieces/world_01/state.txt", PCHQ); read_file(wp, wb, sizeof wb);
#elif PET_SECTION == 109      /* core line 1134 */
    snprintf(xp, sizeof xp, "%s/pieces/xelector_01/state.txt", PCHQ); read_file(xp, kb, sizeof kb); int xx = -1, xy = -1;
#elif PET_SECTION == 110      /* core line 1144 */
    char wp[PATH_MAX], wb[2048], m[64] = "", dk[64] = "", xp[PATH_MAX], kb[512]; snprintf(wp, sizeof wp, "%s/pieces/world_01/state.txt", PCHQ); read_file(wp, wb, sizeof wb);
#elif PET_SECTION == 111      /* core line 1146 */
    if (strcmp(m, "civ-rpgce")) return 0; snprintf(xp, sizeof xp, "%s/pieces/xelector_01/state.txt", PCHQ); read_file(xp, kb, sizeof kb); int xx = -1, xy = -1;
#elif PET_SECTION == 112      /* core line 1177 */
    signal(SIGTERM, bye); signal(SIGINT, bye); signal(SIGHUP, bye); srand((unsigned)(time(NULL) ^ getpid()));
#elif PET_SECTION == 113      /* core line 1185 */
        { static long pl_safety = 0; long pn = now_ms(); if (!sn_active(&g_sn) || sn_changed(&g_sn, SL_PETS) || pn - pl_safety > 30000) { pl_safety = pn; pet_load(); } }      /* marker/shmem: reload the pets only when a writer moved the slot (or every 30 s for a hand edit) */
        int in = flag_on("int", 0), tm = turn_mode(); static int prev_in = 0; static long int_on_at = 0; if (in && !prev_in) int_on_at = now_ms(); prev_in = in;
#elif PET_SECTION == 114      /* core line 1190 */
        if (in && n) { char kl[300], one[16]; kl[0] = 0; for (int i = 0; i < n && i < 20; i++) { snprintf(one, sizeof one, "%d ", codes[i]); strncat(kl, one, sizeof kl - strlen(kl) - 1); } char kp[PATH_MAX]; spath(kp, "keys_seen.txt"); write_if_changed(kp, kl, strlen(kl)); }      /* last batch of codes, for debugging */
#elif PET_SECTION == 115      /* core line 1224 */
            if (board_window_open() && t - cchk >= tune("click_poll_ms", 150)) { cchk = t; char cp[PATH_MAX] = "", ub2[1024]; { char up2[PATH_MAX]; snprintf(up2, sizeof up2, "%s/@.apps/civ-rpgce/board/state/ui.txt", HOUSE); if (read_file(up2, ub2, sizeof ub2) > 0) for (char *l2 = strtok(ub2, "\n"); l2; l2 = strtok(NULL, "\n")) if (!strncmp(l2, "bv_session=", 11)) snprintf(cp, sizeof cp, "%s/pieces/display/canvas_click.txt", l2 + 11); }      /* THIS board's own click file (the window writes it inside its engine session): another board's clicks never select here */
                if (!cp[0]) snprintf(cp, sizeof cp, "%s/pieces/display/canvas_click.txt", ST); struct stat cs;
#elif PET_SECTION == 116      /* core line 1229 */
                    if (cw > 0 && ch > 0) { g_gr = vroom; int col = (cx * FW / cw - OX) / T, row = (cy * FH / ch - OY) / T; char *sa[2]; char sid[24]; sa[0] = "select"; sa[1] = sid; int hit = 0;
#elif PET_SECTION == 117      /* core line 1238 */
        { char dp[PATH_MAX], db[40]; static long dl_safety = 0; long dn = now_ms(); if (!sn_active(&g_sn) || sn_changed(&g_sn, SL_DAY) || dn - dl_safety > 30000) { dl_safety = dn; spath(dp, "daylight.txt"); read_file(dp, db, sizeof db); } else snprintf(db, sizeof db, "%s", lastday); if (strcmp(db, lastday)) { snprintf(lastday, sizeof lastday, "%s", db); dirty = 1; } }
#elif PET_SECTION == 118      /* core line 1259 */
    signal(SIGPIPE, SIG_IGN);      /* a closed pipe / dead child must never kill a verb or the daemon */
    signal(SIGPIPE, SIG_IGN);      /* a closed pipe / dead child must never kill a verb or the daemon */
    snprintf(ST, sizeof ST, "%s/state", APP); mkdir(ST, 0755); sn_init(&g_sn, ST);
    snprintf(PCHQ, sizeof PCHQ, "%s/civ-rpgce-pchq", APP);
#elif PET_SECTION == 119      /* core line 1268 */
    load_data(); replay(); pet_load(); clock_paths();
#endif
#endif
