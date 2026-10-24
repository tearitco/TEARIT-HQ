/* petslike_skin_lib.c - pure text-include for the pets-like skins that paint their OWN maps (rpg-edit-hq, rpg-dust): the code both had a copy of (BOOK 16.game/RPG-DUST-PETSLIKE-BOARD-DESIGN.md, 2026-10-23).
 *   sk_solid       a noisy flat-colour 24 px tile into slot arrays sk_glyph / sk_fs / sk_ws (declared by the skin, section 3)
 *   sk_drop_sheet  forget a loaded sheet so it is read again
 *   the pet roster: party.pdl PET rows -> sk_pt, each pet's 48 px picture (its art csv box-filtered, or a coloured disc), the generated character sheets <PET_SHEET_PFX><g>.png and the 3D walk frames; rewritten only when the roster, a picture or an art file changed
 * The skin defines before including the core:  PET_SHEET_PFX  ("!edit", "!dust": the generated sheet name prefix)  and optionally  PET_ART_FALLBACK  (a folder relative to the app, searched when <app>/art has no such picture: rpg-dust shares rpg-edit-hq's pals pictures instead of copying them).
 * It needs what the core and the skin sections already give: XT, S48, APP, ASSETS, NP, party / nparty, sheets / nsheet, slot_of, split_bar, fexists, read_file, write_if_changed, sk_art[] (section 3) and csv_frames.c (section 2). Include it once, in section 73, before sk_spawn. */
static void sk_solid(int d, int x, int y, char g, int r, int gr, int b, int plank, int tall) {
    unsigned char t[XT * XT * 4];
    for (int i = 0; i < XT * XT; i++) {
        int sh = ((i * 2654435761u) >> 28) % 7 - 3, dk = (plank && ((i / XT) % 6 == 5)) ? -30 : 0;
        t[i * 4] = (unsigned char)(r + sh + dk); t[i * 4 + 1] = (unsigned char)(gr + sh + dk); t[i * 4 + 2] = (unsigned char)(b + sh + dk); t[i * 4 + 3] = 255;
    }
    sk_glyph[d][y][x] = g; if (tall) sk_ws[d][y][x] = slot_of(t); else sk_fs[d][y][x] = slot_of(t);
}
/* ---- RPG Maker autotiles (owner 2026-10-23: "i thought they would all share tiling"). The rule rpg-pet and df-hq each had a copy of (verified against the tsots maps: tile id = kind * 48 + shape, the shape picked from the 8 neighbours of the same kind; all 8 present = shape 0; the 48 shapes are 4 quadrant pieces; same as _.monads/_.livedesk-taskbar/ops/tile_autotile.c's FLOOR_AUTOTILE_TABLE, computed per quadrant: for each corner look at its two adjacent sides and its diagonal).
 *   sk_auto_quads  (petslike_autotile.c) the pure picker: neighbours (1 = the same kind) -> the four 24 px pieces (column, row in 24 px units) inside the kind's 2 x 3 tile block, order TL TR BL BR
 *   sk_auto_tile   those four pieces of a sheet's block (bx, by = the block's top-left 48 px tile) as one XT x XT RGBA picture (each 24 px piece box-filtered to XT / 2); transparent where the art is, so the caller lays it over the ground tile with over24
 * A kind's block in World_A1 / World_A2 sheets: bx = (kind % 8) * 2, by = (kind / 8) * 3 (A2); World_A1 sea is kind 0 at 0,0. ---- */
#include "petslike_autotile.c"      /* sk_auto_quads, the pure picker (also included by rpg-pet and df-hq) */
static void sk_auto_tile(Sheet *sp, int bx, int by, int n, int e, int s, int w, int ne, int se, int sw, int nw, unsigned char *out) {
    int q[4][2], H = XT / 2; sk_auto_quads(n, e, s, w, ne, se, sw, nw, q); memset(out, 0, (size_t)XT * XT * 4); if (!sp) return;
    for (int i = 0; i < 4; i++) { int sx = bx * S48 + q[i][0] * 24, sy = by * S48 + q[i][1] * 24, ox = (i % 2) * H, oy = (i / 2) * H;
        for (int y = 0; y < H; y++) for (int x = 0; x < H; x++) { int acc[3] = {0, 0, 0}, al = 0, wsum = 0;
            for (int dy = 0; dy < 2; dy++) for (int dx = 0; dx < 2; dx++) { int px = sx + x * 2 + dx, py = sy + y * 2 + dy; if (px >= sp->w || py >= sp->h) continue; const unsigned char *a = sp->px + ((size_t)py * sp->w + px) * 4; acc[0] += a[0] * a[3]; acc[1] += a[1] * a[3]; acc[2] += a[2] * a[3]; wsum += a[3]; al += a[3]; }
            unsigned char *o = out + ((oy + y) * XT + ox + x) * 4; if (wsum) { o[0] = (unsigned char)(acc[0] / wsum); o[1] = (unsigned char)(acc[1] / wsum); o[2] = (unsigned char)(acc[2] / wsum); o[3] = (unsigned char)(al / 4); } } }
}
/* one cell of an autotiled kind: the ground tile (slot `ground`) with the kind's edge-shaped picture laid over it, written as the cell's floor slot (tall = 0) or its wall slot (tall = 1), glyph g. nb = the 8 neighbours (1 = the same kind) in the order n, e, s, w, ne, se, sw, nw; the skin decides what "same kind" means. */
static void sk_auto_ground(int d, int x, int y, char g, Sheet *sp, int bx, int by, const int nb[8], int ground, int tall) {
    unsigned char t[XT * XT * 4], a[XT * XT * 4]; memcpy(t, slotpx[ground], sizeof t);
    sk_auto_tile(sp, bx, by, nb[0], nb[1], nb[2], nb[3], nb[4], nb[5], nb[6], nb[7], a); over24(t, a);
    sk_glyph[d][y][x] = g; if (tall) { sk_ws[d][y][x] = slot_of(t); } else { sk_fs[d][y][x] = slot_of(t); sk_ws[d][y][x] = 0; }
}
static void sk_drop_sheet(const char *name) { for (int i = 0; i < nsheet; i++) if (!strcmp(sheets[i].name, name)) { stbi_image_free(sheets[i].px); sheets[i] = sheets[--nsheet]; return; } }
/* ---- the pets: what each looks like. party.pdl PET rows (name | sheet | index | art id | letter | r | g | b | x | y); the skin reads the picture, spawn and colour columns ---- */
/* a pet's art picture: <app>/art first, then PET_ART_FALLBACK when the skin defines it (a folder relative to the app: the sibling game's pictures shared instead of copied); without a fallback the first path is returned even when the file is missing */
static void sk_art_path(char *out, size_t cap, const char *id) {
    snprintf(out, cap, "%s/art/%s.csv", APP, id);
#ifdef PET_ART_FALLBACK
    if (fexists(out)) return;
    { char alt[PATH_MAX]; snprintf(alt, sizeof alt, "%s/" PET_ART_FALLBACK "/%s.csv", APP, id); if (fexists(alt)) snprintf(out, cap, "%s", alt); }
#endif
}
static struct { char name[24], art[48]; char letter; int r, g, b, x, y; } sk_pt[NP]; static int sk_npt;
static int sk_row(const char *name) { for (int k = 0; k < sk_npt; k++) if (!strcmp(sk_pt[k].name, name)) return k; return -1; }
static void sk_pic(int i, unsigned char *out, int *avg) {
    int k = sk_row(party[i].name); char cp[PATH_MAX]; unsigned char big[64 * 64 * 4];
    if (k >= 0 && sk_pt[k].art[0] && strcmp(sk_pt[k].art, "none")) { sk_art_path(cp, sizeof cp, sk_pt[k].art);
        if (csvf_load64(cp, big)) { for (int y = 0; y < 48; y++) for (int x = 0; x < 48; x++) { int acc[4] = {0}, w = 0, x0 = x * 64 / 48, x1 = (x + 1) * 64 / 48, y0 = y * 64 / 48, y1 = (y + 1) * 64 / 48; if (x1 <= x0) x1 = x0 + 1; if (y1 <= y0) y1 = y0 + 1;
                for (int yy = y0; yy < y1; yy++) for (int xx = x0; xx < x1; xx++) { unsigned char *q = big + (yy * 64 + xx) * 4; acc[0] += q[0] * q[3]; acc[1] += q[1] * q[3]; acc[2] += q[2] * q[3]; acc[3] += q[3]; w++; }
                unsigned char *o = out + (y * 48 + x) * 4; if (acc[3]) { o[0] = (unsigned char)(acc[0] / acc[3]); o[1] = (unsigned char)(acc[1] / acc[3]); o[2] = (unsigned char)(acc[2] / acc[3]); o[3] = (unsigned char)(acc[3] / w); } else o[0] = o[1] = o[2] = o[3] = 0; }
            sk_art[i] = 1; return; } }
    sk_art[i] = 0; int r = k >= 0 ? sk_pt[k].r : 60 + (i * 53) % 180, g = k >= 0 ? sk_pt[k].g : 60 + (i * 97) % 180, b = k >= 0 ? sk_pt[k].b : 60 + (i * 29) % 180; if (avg) { avg[0] = r; avg[1] = g; avg[2] = b; }
    for (int y = 0; y < 48; y++) for (int x = 0; x < 48; x++) { int dx = x - 24, dy = y - 24, in = dx * dx + dy * dy <= 20 * 20; unsigned char *o = out + (y * 48 + x) * 4; o[0] = (unsigned char)r; o[1] = (unsigned char)g; o[2] = (unsigned char)b; o[3] = in ? 255 : 0; }
}
/* party.pdl -> sk_pt; then the character sheets PET_SHEET_PFX<g>.png (12 x 8 cells of 48 px, the pet's picture in every direction and walk frame) and, for art pets, the 3D walk frames. Rewritten only when the roster, a picture
 * or an art file changed (state/sheets_sig.txt): every verb process calls this, so an unchanged roster costs a few stat() calls. */
static void sk_pets_sync(void) {
    char p[PATH_MAX], line[400]; char *f[12]; snprintf(p, sizeof p, "%s/party.pdl", APP); FILE *fp = fopen(p, "r"); sk_npt = 0;
    while (fp && fgets(line, sizeof line, fp) && sk_npt < NP) { int n = split_bar(line, f, 12); if (n < 5 || strcmp(f[0], "PET")) continue;
        snprintf(sk_pt[sk_npt].name, 24, "%s", f[1]); snprintf(sk_pt[sk_npt].art, 48, "%s", f[4]); sk_pt[sk_npt].letter = n > 5 && f[5][0] ? f[5][0] : 'p';
        sk_pt[sk_npt].r = n > 6 ? atoi(f[6]) : 128; sk_pt[sk_npt].g = n > 7 ? atoi(f[7]) : 128; sk_pt[sk_npt].b = n > 8 ? atoi(f[8]) : 128; sk_pt[sk_npt].x = n > 9 ? atoi(f[9]) : 9; sk_pt[sk_npt].y = n > 10 ? atoi(f[10]) : 13; sk_npt++; }
    if (fp) fclose(fp);
    unsigned long sig = 5381; for (int i = 0; i < nparty; i++) { int k = sk_row(party[i].name); const char *a = k >= 0 ? sk_pt[k].art : ""; struct stat st; char cp[PATH_MAX]; sk_art_path(cp, sizeof cp, a);
        for (const char *c = party[i].name; *c; c++) sig = sig * 33 + (unsigned char)*c; for (const char *c = a; *c; c++) sig = sig * 33 + (unsigned char)*c; if (k >= 0) sig = sig * 33 + (unsigned long)(sk_pt[k].r * 7 + sk_pt[k].g * 3 + sk_pt[k].b);
        if (a[0] && stat(cp, &st) == 0) sig = sig * 33 + (unsigned long)st.st_size + (unsigned long)st.st_mtime; }
    char want[40], have[40], sp[PATH_MAX], d0[PATH_MAX]; snprintf(want, sizeof want, "%lu\n", sig); spath(sp, "sheets_sig.txt"); read_file(sp, have, sizeof have);
    snprintf(d0, sizeof d0, "%s/../tsots-characters/" PET_SHEET_PFX "0.png", ASSETS); int fresh = !strcmp(want, have) && fexists(d0);
    char fd[PATH_MAX]; snprintf(fd, sizeof fd, "%s/../tsots-characters", ASSETS); mkdir(fd, 0755); snprintf(fd, sizeof fd, "%s/../tsots-characters/frames", ASSETS); mkdir(fd, 0755);
    static unsigned char pic[48 * 48 * 4], sheetpx[576 * 384 * 4];
    for (int g = 0; g <= (nparty - 1) / 8; g++) {
        if (!fresh) memset(sheetpx, 0, sizeof sheetpx);
        for (int k = 0; k < 8 && g * 8 + k < nparty; k++) { int i = g * 8 + k, avg[3] = {128, 128, 128};
            if (fresh) { int kk = sk_row(party[i].name); char cp[PATH_MAX]; sk_art_path(cp, sizeof cp, kk >= 0 ? sk_pt[kk].art : "none"); sk_art[i] = kk >= 0 && strcmp(sk_pt[kk].art, "none") && fexists(cp); continue; }      /* unchanged roster: only note which pets have art */
            sk_pic(i, pic, avg);
            for (int pat = 0; pat < 3; pat++) for (int row = 0; row < 4; row++) { int sx = ((k % 4) * 3 + pat) * 48, sy = ((k / 4) * 4 + row) * 48; for (int y = 0; y < 48; y++) memcpy(sheetpx + ((size_t)(sy + y) * 576 + sx) * 4, pic + y * 48 * 4, 48 * 4); }
            if (sk_art[i]) { int kk = sk_row(party[i].name); char cp[PATH_MAX], csn[16]; sk_art_path(cp, sizeof cp, sk_pt[kk].art); snprintf(csn, sizeof csn, PET_SHEET_PFX "%d", g); csv_write_frames(cp, fd, csn, k, avg); } }
        if (!fresh) { char pp[PATH_MAX + 16], tp[PATH_MAX + 32], nm[16]; snprintf(pp, sizeof pp, "%s/../tsots-characters/" PET_SHEET_PFX "%d.png", ASSETS, g); snprintf(tp, sizeof tp, "%s.tmp", pp); snprintf(nm, sizeof nm, PET_SHEET_PFX "%d", g);
            if (stbi_write_png(tp, 576, 384, 4, sheetpx, 576 * 4)) rename(tp, pp); else remove(tp); sk_drop_sheet(nm); }
    }
    if (!fresh) write_if_changed(sp, want, strlen(want));
}

