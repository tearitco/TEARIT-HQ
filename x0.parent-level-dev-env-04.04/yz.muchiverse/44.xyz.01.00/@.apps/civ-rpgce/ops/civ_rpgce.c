/* civ_rpgce.c - civ-rpgce on the shared pets-like core (&.widgits/_shared-lib/petslike_core.c). Two roles in one file: the top builds the program, and the core
 * re-includes this file once per SECTION to pull in what is specific to this game. Design: @.apps/../#.#.calendar-dox/!.HQ-IQ-BOOK/16.game/PETSLIKE-FRAMEWORK-PLAN.md */
#ifndef PET_SECTION
#define PET_APP "civ-rpgce"
#define PET_OP "civ_rpgce"
#define PET_SKIN "civ_rpgce.c"
#include "../../../&.widgits/_shared-lib/petslike_core.c"
#else
#if PET_SECTION == 1      /* core line 209 */
    nparty = 0; char names[NP][24]; int nn = 0, inparty = 0, inhero = 0; memset(&hero, 0, sizeof hero); snprintf(hero.name, 24, "Red"); snprintf(hero.sheet, 24, "viking"); hero.idx = 0; /* settler. The picture is the grey armor on the viking sheet, tinted. */
#elif PET_SECTION == 2      /* core line 211 */
    if ((fp = fopen(p, "r"))) { while (fgets(line, sizeof line, fp)) { if (split_bar(line, f, 3) < 3 || strcmp(f[0], "SYSTEM")) continue; if (!strcmp(f[1], "name")) { inparty = !strcmp(f[2], getenv("PET_PARTY") && getenv("PET_PARTY")[0] ? getenv("PET_PARTY") : "civ_rpgceParty"); inhero = 0; } else if (inparty && !strncmp(f[1], "member", 6) && nn < NP) snprintf(names[nn++], 24, "%s", f[2]); } fclose(fp); }
    if (nn == 0) { const char *fb[] = {"Blue", "Green"}; for (int i = 0; i < 2 && nn < NP; i++) snprintf(names[nn++], 24, "%s", fb[i]); }
    for (int i = 0; i < nn; i++) { memset(&party[nparty], 0, sizeof party[nparty]); snprintf(party[nparty].name, 24, "%s", names[i]); snprintf(party[nparty].id, 16, "%s", names[i]); for (char *c = party[nparty].id; *c; c++) if (*c >= 'A' && *c <= 'Z') *c += 32; party[nparty].idx = i; snprintf(party[nparty].sheet, 24, "viking"); party[nparty].idx = i + 1; nparty++; }
#elif PET_SECTION == 3      /* core line 369 */
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

#elif PET_SECTION == 4      /* core line 445 */
static int sea_beside_land(const Room *rm, int x, int y) {
    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) if (!over_sea(rm, x + dx, y + dy)) return 1; return 0;
}
#elif PET_SECTION == 5      /* core line 449 */
    for (int y = 0; y < rm->rows; y++) for (int x = 0; x < rm->cols; x++) {
        if (!over_sea(rm, x, y)) ohr_at("320px-TS02_town.bmp.png", 1, 0, x, y);
        else if (sea_beside_land(rm, x, y)) ohr_at("320px-TS02_town.bmp.png", 3, 1, x, y);
        else ohr_at("320px-TS02_town.bmp.png", 1, 7, x, y);
    }
#elif PET_SECTION == 6      /* core line 459 */
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
#elif PET_SECTION == 7      /* core line 472 */
    for (int a = 0; a < n; a++) { Placed *p = &pl[idx[a]]; Item *d = &cat[p->item]; paint_cells(d->sheet, d->c, d->r, d->w, d->h, p->x, p->y); }
#elif PET_SECTION == 8      /* core line 479 */
    for (int k = 0; k < (g_bg_only ? 0 : no); k++) { int i = order[k]; Ent *e = i < 0 ? &tr : &others[i]; int si = i < 0 ? hero.idx : party[i].idx; if (si < 0) si = 0; if (si > 2) si = 2;
        static const int tn[3][3] = {{255, 70, 70}, {80, 140, 255}, {80, 220, 90}};
        blit_box(ohr_sheet("320px-Vikings_tilemaps_000.bmp.png"), 10 * 20, 2 * 20, 20, 40, OX + e->x * T, OY + e->y * T - T / 2, T, T + T / 2, tn[si][0], tn[si][1], tn[si][2]); }
#elif PET_SECTION == 9      /* core line 513 */
    { static const char *dflt[4] = {"hmm, tell me more.", "I'm listening.", "interesting!", "okay."}; unsigned h = 0; for (const char *c = s; *c; c++) h = h * 31 + (unsigned char)*c;
#elif PET_SECTION == 10      /* core line 763 */

static void ohr24(const char *file, int c, int r, unsigned char *out) {
    Sheet *s = ohr_sheet(file); memset(out, 0, XT * XT * 4); if (!s) return; int sx = c * 20, sy = r * 20;
    for (int y = 0; y < XT; y++) for (int x = 0; x < XT; x++) { int px = sx + x * 20 / XT, py = sy + y * 20 / XT; if (px >= 0 && py >= 0 && px < s->w && py < s->h) memcpy(out + (y * XT + x) * 4, s->px + ((size_t)py * s->w + px) * 4, 4); }
}
#elif PET_SECTION == 11      /* core line 782 */
    if (!strcmp(sh, "viking")) {
        Sheet *s = ohr_sheet("320px-Vikings_tilemaps_000.bmp.png"); if (!s) return; char d[PATH_MAX], p[PATH_MAX]; snprintf(d, sizeof d, "%s/../tsots-characters", ASSETS); mkdir(d, 0755); snprintf(d, sizeof d, "%s/../tsots-characters/frames", ASSETS); mkdir(d, 0755);
        static const int dc[4] = {2, 4, 6, 8}; snprintf(p, sizeof p, "%s/viking_%d_%d_%d.rgba", d, idx, dc[dir4 & 3], pat); if (fexists(p)) return;
        static const int tn[3][3] = {{255, 70, 70}, {80, 140, 255}, {80, 220, 90}}; int si = idx < 0 ? 0 : idx > 2 ? 2 : idx; unsigned char out[16 * 24 * 4];
        for (int y = 0; y < 24; y++) for (int x = 0; x < 16; x++) { int px = 10 * 20 + x * 20 / 16, py = 2 * 20 + y * 40 / 24; unsigned char *a = s->px + ((size_t)py * s->w + px) * 4; unsigned char *o = out + (y * 16 + x) * 4;
            if (px >= s->w || py >= s->h || (a[0] > 240 && a[2] > 240 && a[1] < 40)) { o[0] = o[1] = o[2] = o[3] = 0; continue; }
            o[0] = (unsigned char)(a[0] * tn[si][0] / 255); o[1] = (unsigned char)(a[1] * tn[si][1] / 255); o[2] = (unsigned char)(a[2] * tn[si][2] / 255); o[3] = a[3]; }
        FILE *f = fopen(p, "wb"); if (f) { fwrite(out, 1, sizeof out, f); fclose(f); } return;
    }
#elif PET_SECTION == 12      /* core line 833 */
    for (int i = 0; i < nparty; i++) if (others[i].room == r) { int pat = others[i].steps % 3; write_frame(party[i].sheet, party[i].idx, others[i].dir, pat); unsigned char px[4] = {128, 128, 128, 255}; static const int tn[3][3] = {{255, 70, 70}, {80, 140, 255}, {80, 220, 90}}; int si = party[i].idx; if (si < 0) si = 0; if (si > 2) si = 2; if (!strcmp(party[i].sheet, "viking")) { px[0] = (unsigned char)tn[si][0]; px[1] = (unsigned char)tn[si][1]; px[2] = (unsigned char)tn[si][2]; } else frame_rgb(sheet("characters", party[i].sheet), party[i].idx, others[i].dir & 3, px);
#elif PET_SECTION == 13      /* core line 835 */
    if (tr.room == r) { int pat = tr.steps % 3; write_frame(hero.sheet, hero.idx, tr.dir, pat); unsigned char px[4] = {255, 70, 70, 255}; if (strcmp(hero.sheet, "viking")) frame_rgb(sheet("characters", hero.sheet), hero.idx, tr.dir & 3, px);
        o += snprintf(buf + o, sizeof buf - o, "%d %d %d %d %d %s %d %d %d @\n", tr.x, tr.y, px[0], px[1], px[2], hero.sheet, hero.idx, dc[tr.dir & 3], pat); }
#elif PET_SECTION == 14      /* core line 928 */
        Room *rm = &rooms[r];
#elif PET_SECTION == 15      /* core line 930 */
        { int castle = !strcmp(rm->id, "castle"); const char *town = "320px-TS02_town.bmp.png";
          ohr24(town, castle ? 7 : 3, castle ? 8 : 1, floorpx); ohr24(town, 8, castle ? 7 : 2, wallpx); ohr24(town, 8, 0, ceilpx); memcpy(pathpx, floorpx, sizeof pathpx); ohr24(town, 8, 6, doorpx); memcpy(matpx, floorpx, sizeof matpx); }
#elif PET_SECTION == 16      /* core line 939 */
        for (int i = 0; i < npl; i++) if (pl[i].room == r) { Item *it = &cat[pl[i].item]; const char *of = ohr_file(it->sheet); Sheet *s = of ? NULL : sheet("tilesets", it->sheet);
#elif PET_SECTION == 17      /* core line 941 */
                if (of) ohr24(of, it->c + ci, it->r + cj, a); else tile24(s, (it->c + ci) * S48, (it->r + cj) * S48, a);
#endif
#endif
