/* gta_hq.c - gta-hq on the shared pets-like core (&.widgits/_shared-lib/petslike_core.c). Two roles in one file: the top builds the program, and the core
 * re-includes this file once per SECTION to pull in what is specific to this game. Design: @.apps/../#.#.calendar-dox/!.HQ-IQ-BOOK/16.game/PETSLIKE-FRAMEWORK-PLAN.md */
#ifndef PET_SECTION
#define PET_APP "gta-hq"
#define PET_OP "gta_hq"
#define PET_SKIN "gta_hq.c"
#include "../../../&.widgits/_shared-lib/petslike_core.c"
#else
#if PET_SECTION == 1
 * Map terrain is drawn from shared/gta-tiles.pdl roles (first TILE row in shared/cdda-tiles.pdl). People and furniture stay on the RPG Maker sheets.
#elif PET_SECTION == 2
#elif PET_SECTION == 3
static char PCHQ[PATH_MAX];      /* this game's OWN pc-hq instance (data only: world, hero, xelector, maps, engine sessions): <app>/gta-hq-pchq, see PCHQ-SESSIONS-DESIGN.md. Programs stay in @.apps/piececraft-hq. */
#define SIBLING_APP "/@.apps/" "rpg-pet" "/"      /* the carbon-copy game that shares the one pc-hq engine + world: its board window is NOT a viewer of this game (see viewer_kinds); the copy sets this to the other game */
#elif PET_SECTION == 4
#elif PET_SECTION == 5
#elif PET_SECTION == 6
    f = fopen(tmp, "wb"); if (!f) return 0; size_t wn = fwrite(data, 1, n, f); int ce = fclose(f); if (wn != n || ce != 0 || rename(tmp, p) != 0) { remove(tmp); return 0; } return 1;      /* ENOSPC / EIO: the good file stays, the half-written tmp goes (hq-crash-hardening.md) */
#elif PET_SECTION == 7
static struct { char k[48]; char v[64]; long t; } g_fc[32];
#elif PET_SECTION == 8
    char ck[48]; long cnow = 0; if (g_fcache_on) { snprintf(ck, sizeof ck, "%s|%s", name, def); cnow = now_ms(); for (int i = 0; i < g_fcn; i++) if (cnow - g_fc[i].t < tune("flag_cache_ms", 150) && !strcmp(g_fc[i].k, ck)) { snprintf(out, cap, "%s", g_fc[i].v); return; } }
#elif PET_SECTION == 9
    if (g_fcache_on) { int s = -1; for (int i = 0; i < g_fcn; i++) if (!strcmp(g_fc[i].k, ck)) s = i; if (s < 0 && g_fcn < 32) s = g_fcn++; if (s >= 0) { snprintf(g_fc[s].k, sizeof g_fc[s].k, "%s", ck); snprintf(g_fc[s].v, sizeof g_fc[s].v, "%s", out); g_fc[s].t = cnow; } }
#elif PET_SECTION == 10
#elif PET_SECTION == 11
    nparty = 0; char names[NP][24]; int nn = 0, inparty = 0, inhero = 0; memset(&hero, 0, sizeof hero); snprintf(hero.name, 24, "Harold"); snprintf(hero.sheet, 24, "Actor1");
#elif PET_SECTION == 12
    if ((fp = fopen(p, "r"))) { while (fgets(line, sizeof line, fp)) { if (split_bar(line, f, 3) < 3 || strcmp(f[0], "SYSTEM")) continue; if (!strcmp(f[1], "name")) { inparty = !strcmp(f[2], getenv("PET_PARTY") && getenv("PET_PARTY")[0] ? getenv("PET_PARTY") : "DsrRpgParty"); inhero = !strcmp(f[2], "Party"); } else if (inhero && !strcmp(f[1], "member1")) snprintf(hero.name, 24, "%s", f[2]); else if (inparty && !strncmp(f[1], "member", 6) && nn < NP) snprintf(names[nn++], 24, "%s", f[2]); } fclose(fp); }
    for (int i = 0; i < nn; i++) { memset(&party[nparty], 0, sizeof party[nparty]); snprintf(party[nparty].name, 24, "%s", names[i]); snprintf(party[nparty].id, 16, "%s", names[i]); for (char *c = party[nparty].id; *c; c++) if (*c >= 'A' && *c <= 'Z') *c += 32; party[nparty].idx = i; snprintf(party[nparty].sheet, 24, "Monster"); nparty++; }
#elif PET_SECTION == 13
    { char np[PATH_MAX], nl[200]; snprintf(np, sizeof np, "%s/party.pdl", APP); FILE *nf = fopen(np, "r"); if (nf) { int pi = 0; while (fgets(nl, sizeof nl, nf)) { char *g[4]; char cp[200]; snprintf(cp, sizeof cp, "%s", nl); int n = split_bar(cp, g, 4); if (n >= 2 && !strcmp(g[0], "PLAYER")) { snprintf(hero.name, 24, "%s", g[1]); snprintf(hero.nick, 24, "%s", g[1]); } else if (n >= 2 && !strcmp(g[0], "PET") && pi < nparty) { snprintf(party[pi].name, 24, "%s", g[1]); snprintf(party[pi].id, 16, "%s", g[1]); for (char *c = party[pi].id; *c; c++) if (*c >= 'A' && *c <= 'Z') *c += 32; pi++; } } fclose(nf); } }
#elif PET_SECTION == 14
#elif PET_SECTION == 15
#elif PET_SECTION == 16
#elif PET_SECTION == 17
    char p[PATH_MAX], line[300]; spath(p, "house.txt");
#elif PET_SECTION == 18
static void use_ent(int i) { me = i; pet = i < 0 ? tr : others[i]; if (pet.room >= 0 && pet.room < nroom) g_gr = pet.room; }
#elif PET_SECTION == 19
    write_if_changed(p, b, strlen(b)); if (me < 0) tr = pet; else others[me] = pet; calc_view();
#elif PET_SECTION == 20
static int over_land(const Room *rm, int x, int y);
static int floor_blocked(int room, int x, int y, int skip_id) { if (rooms[room].over && !over_land(&rooms[room], x, y)) return 9999; for (int i = 0; i < nscen; i++) if (scen[i].room == room && x >= scen[i].x && x < scen[i].x + scen[i].w && y >= scen[i].y && y < scen[i].y + scen[i].h) return 9999; for (int i = 0; i < npl; i++) { Placed *q = &pl[i]; if (q->room != room || q->id == skip_id || cat[q->item].wall) continue; if (x >= q->x && x < q->x + cat[q->item].w && y >= q->y && y < q->y + cat[q->item].h) return q->id; } return 0; }
static int door_here(int room, int x, int y) { for (int i = 0; i < ndoor; i++) if (doors[i].room == room && doors[i].tx == x && y == doors[i].ty + 1) return i; return -1; }
/* overmap land: an island on a square ocean. Cells inside the rounded rectangle (2..cols-3, 1..rows-2, corners cut) are land, the outermost land ring is beach. Everything else is ocean (blocked). */
static int over_land(const Room *rm, int x, int y) { if (x < 2 || x > rm->cols - 3 || y < 1 || y > rm->rows - 2) return 0; if ((x <= 3 || x >= rm->cols - 4) && (y <= 2 || y >= rm->rows - 3)) return 0; return 1; }
static int over_beach(const Room *rm, int x, int y) { return over_land(rm, x, y) && (!over_land(rm, x - 1, y) || !over_land(rm, x + 1, y) || !over_land(rm, x, y - 1) || !over_land(rm, x, y + 1)); }
#elif PET_SECTION == 21
static void tile(const char *sh, int c, int r, int w, int h, int tx, int ty) { blit(sheet("tilesets", sh), c * S48, r * S48, w * S48, h * S48, OX + tx * T, OY + ty * T); }

/* gta-tiles.pdl role -> first TILE png in cdda-tiles.pdl. Loaded once. Missing role falls back to the RPG Maker tile. */
static char ULTICA[PATH_MAX];
typedef struct { char role[24]; char id[48]; char png[160]; } GRole;
static GRole groles[48]; static int ngrole;
static Sheet gpng[24]; static int ngpng;
static void gta_tiles_load(void) {
    if (ngrole) return;
    char d[PATH_MAX]; snprintf(d, sizeof d, "%s", HOUSE[0] ? HOUSE : APP);
    for (int i = 0; i < 8 && !ULTICA[0]; i++) {
        char p[PATH_MAX]; snprintf(p, sizeof p, "%s/#.NNEST_ASSETS/cdda-tilesets/gfx/UltimateCataclysm", d);
        if (is_dir(p)) snprintf(ULTICA, sizeof ULTICA, "%s", p);
        char *s = strrchr(d, '/'); if (!s || s == d) break; *s = 0;
    }
    char gp[PATH_MAX]; snprintf(gp, sizeof gp, "%s/shared/gta-tiles.pdl", HOUSE);
    FILE *f = fopen(gp, "r"); if (!f) return;
    char line[512];
    while (fgets(line, sizeof line, f) && ngrole < 47) {
        if (strncmp(line, "GTA |", 5)) continue;
        char role[24], id[48];
        if (sscanf(line, "GTA | %23s | %47s", role, id) != 2) continue;
        snprintf(groles[ngrole].role, 24, "%s", role);
        snprintf(groles[ngrole].id, 48, "%s", id);
        ngrole++;
    }
    fclose(f);
    int have_floor = 0; for (int i = 0; i < ngrole; i++) if (!strcmp(groles[i].role, "floor")) have_floor = 1;
    if (!have_floor && ngrole < 48) { snprintf(groles[ngrole].role, 24, "floor"); snprintf(groles[ngrole].id, 48, "t_floor_center"); ngrole++; }
    char cp[PATH_MAX]; snprintf(cp, sizeof cp, "%s/shared/cdda-tiles.pdl", HOUSE);
    f = fopen(cp, "r"); if (!f) return;
    int left = ngrole;
    while (left && fgets(line, sizeof line, f)) {
        if (strncmp(line, "TILE |", 6)) continue;
        char id[48], png[160];
        if (sscanf(line, "TILE | %47s | %159s", id, png) != 2) continue;
        for (int i = 0; i < ngrole; i++) if (!groles[i].png[0] && !strcmp(groles[i].id, id)) {
            snprintf(groles[i].png, 160, "%s", png); left--; break;
        }
    }
    fclose(f);
}
static Sheet *gta_png(const char *role) {
    gta_tiles_load();
    const char *rel = NULL;
    for (int i = 0; i < ngrole; i++) if (!strcmp(groles[i].role, role)) { rel = groles[i].png; break; }
    if (!rel || !rel[0] || !ULTICA[0]) return NULL;
    for (int i = 0; i < ngpng; i++) if (!strcmp(gpng[i].name, role)) return gpng[i].px ? &gpng[i] : NULL;
    if (ngpng >= 24) return NULL;
    char p[PATH_MAX]; snprintf(p, sizeof p, "%s/%s", ULTICA, rel);
    Sheet *s = &gpng[ngpng++]; int n; s->px = stbi_load(p, &s->w, &s->h, &n, 4);
    snprintf(s->name, 40, "%s", role); return s->px ? s : NULL;
}
static void blit_fit(Sheet *s, int dx, int dy, int dw, int dh) {
    if (!s || !s->px || dw < 1 || dh < 1 || dw > 4 * FW || dh > 4 * FH) return;
    for (int y = 0; y < dh; y++) { int Y = dy + y, srcy = y * s->h / dh; if (Y < 0 || Y >= FH || srcy < 0 || srcy >= s->h) continue;
        for (int x = 0; x < dw; x++) { int X = dx + x, srcx = x * s->w / dw; if (X < 0 || X >= FW || srcx < 0 || srcx >= s->w) continue;
            unsigned char *a = s->px + ((size_t)srcy * s->w + srcx) * 4, *b = fr + ((size_t)Y * FW + X) * 4; int al = a[3]; if (!al) continue;
            for (int c = 0; c < 3; c++) b[c] = (unsigned char)((a[c] * al + b[c] * (255 - al)) / 255); b[3] = 255; } }
}
static int cell_role(const char *role, int tx, int ty, int tw, int th) {
    Sheet *s = gta_png(role); if (!s) return 0;
    blit_fit(s, OX + tx * T, OY + (ty - (th - 1)) * T, tw * T, th * T); return 1;
}
/* art: cdda (default, gta-tiles roles), rmmv (the old sheets), brixel (socketed clay). emoji is a separate overlay. Keys 96 and 92 set the flags; the board may still eat 96 as cjk_view before it arrives. */
static int g_art, g_emoji;
static void art_refresh(void) {
    char b[16]; flag_str("art", "brixel", b, sizeof b);
    g_art = !strcmp(b, "rmmv") ? 0 : !strcmp(b, "brixel") ? 2 : 1;
    g_emoji = flag_on("emoji", 0);
}
/* A person stands in the cell. shared/brixels/<name>-front.txt is 32x48 named parts. Feet sit on the bottom of the cell. The same picture is the 3D wall face. */
#define FIG_W 32
#define FIG_H 48
#define FIG_N 8
static void figure_rgb(char c, int *r, int *g, int *b, int *a) {
    *a = 255; *r = 0; *g = 0; *b = 0;
    if (c == '.') *a = 0;
    else if (c == 'H') { *r = 28; *g = 22; *b = 18; }
    else if (c == 'h') { *r = 62; *g = 48; *b = 36; }
    else if (c == 'L') { *r = 214; *g = 176; *b = 90; }
    else if (c == 'S') { *r = 214; *g = 166; *b = 130; }
    else if (c == 's') { *r = 176; *g = 124; *b = 96; }
    else if (c == 'B') { *r = 42; *g = 32; *b = 26; }
    else if (c == 'W') { *r = 236; *g = 232; *b = 220; }
    else if (c == 'e') { *r = 24; *g = 28; *b = 34; }
    else if (c == 'N') { *r = 168; *g = 112; *b = 86; }
    else if (c == 'M') { *r = 150; *g = 72; *b = 68; }
    else if (c == 'D') { *r = 70; *g = 48; *b = 36; }
    else if (c == 'J') { *r = 36; *g = 48; *b = 72; }
    else if (c == 'j') { *r = 22; *g = 28; *b = 44; }
    else if (c == 'G') { *r = 36; *g = 96; *b = 52; }
    else if (c == 'g') { *r = 22; *g = 64; *b = 36; }
    else if (c == 'R') { *r = 160; *g = 42; *b = 44; }
    else if (c == 'Y') { *r = 210; *g = 168; *b = 40; }
    else if (c == 'Z') { *r = 110; *g = 72; *b = 40; }
    else if (c == 'K') { *r = 36; *g = 36; *b = 40; }
    else if (c == 'T') { *r = 214; *g = 214; *b = 208; }
    else if (c == 'C') { *r = 186; *g = 186; *b = 178; }
    else if (c == 'A') { *r = 214; *g = 166; *b = 130; }
    else if (c == 'P') { *r = 42; *g = 58; *b = 86; }
    else if (c == 'p') { *r = 28; *g = 38; *b = 58; }
    else if (c == 'O') { *r = 28; *g = 28; *b = 30; }
    else if (c == 'o') { *r = 12; *g = 12; *b = 14; }
    else *a = 0;
}
static struct { char name[16]; unsigned char img[FIG_W * FIG_H * 4]; int ok; } g_fig[FIG_N];
static int g_fig_n;
static int figure_slot(const char *name) {
    for (int i = 0; i < g_fig_n; i++) if (!strcmp(g_fig[i].name, name)) return i;
    if (g_fig_n >= FIG_N) return -1;
    int n = g_fig_n++; snprintf(g_fig[n].name, 16, "%s", name); g_fig[n].ok = 0;
    char p[PATH_MAX]; snprintf(p, sizeof p, "%s/shared/brixels/%s-front.txt", HOUSE, name);
    FILE *f = fopen(p, "r"); if (!f) return n;
    char line[80]; int y = 0;
    while (y < FIG_H && fgets(line, sizeof line, f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        for (int x = 0; x < FIG_W && line[x] && line[x] != '\n'; x++) {
            int r, g, b, a; figure_rgb(line[x], &r, &g, &b, &a);
            int i = (y * FIG_W + x) * 4; g_fig[n].img[i] = (unsigned char)r; g_fig[n].img[i+1] = (unsigned char)g; g_fig[n].img[i+2] = (unsigned char)b; g_fig[n].img[i+3] = (unsigned char)a;
        }
        y++;
    }
    fclose(f); g_fig[n].ok = y >= 40; return n;
}
static int blit_figure(const char *name, int tx, int ty) {
    int n = figure_slot(name); if (n < 0 || !g_fig[n].ok) return 0;
    int dw = T, dh = T * FIG_H / FIG_W; if (dh < 36) dh = 36;
    int x0 = OX + tx * T, y0 = OY + (ty + 1) * T - dh;
    for (int y = 0; y < dh; y++) for (int x = 0; x < dw; x++) {
        int X = x0 + x, Y = y0 + y; if (X < 0 || Y < 0 || X >= FW || Y >= FH) continue;
        int sx = x * FIG_W / dw, sy = y * FIG_H / dh; unsigned char *a = g_fig[n].img + (sy * FIG_W + sx) * 4; if (!a[3]) continue;
        unsigned char *q = fr + ((size_t)Y * FW + X) * 4; q[0] = a[0]; q[1] = a[1]; q[2] = a[2]; q[3] = 255;
    }
    return 1;
}
/* The 3D block face is this same front view, scaled into one tile. Empty pixels stay transparent so the floor shows around the body. */
static int figure_tile(const char *name, unsigned char *dst, int n) {
    int s = figure_slot(name); if (s < 0 || !g_fig[s].ok || n < 1) return 0;
    memset(dst, 0, (size_t)n * n * 4);
    for (int y = 0; y < n; y++) for (int x = 0; x < n; x++) {
        int sx = x * FIG_W / n, sy = y * FIG_H / n; unsigned char *a = g_fig[s].img + (sy * FIG_W + sx) * 4;
        unsigned char *q = dst + (y * n + x) * 4; q[0] = a[0]; q[1] = a[1]; q[2] = a[2]; q[3] = a[3];
    }
    return 1;
}
/* A modern building covers its whole scenery footprint (the city ones are 2x2). kind is the count of World_B pieces before this one: tower, vault, shop, lot, motel. A tree is a canopy and a trunk. */
static void put_px(int X, int Y, int r, int g, int b) {
    if (X < 0 || Y < 0 || X >= FW || Y >= FH) return;
    unsigned char *q = fr + ((size_t)Y * FW + X) * 4; q[0] = (unsigned char)r; q[1] = (unsigned char)g; q[2] = (unsigned char)b; q[3] = 255;
}
static void paint_scenery(const Scen *s, int index) {
    int tree = !strcmp(s->sheet, "Outside_B");
    int kind = 0; for (int i = 0; i < index; i++) if (scen[i].room == s->room && strcmp(scen[i].sheet, "Outside_B")) kind++;
    int x0 = OX + s->x * T, y0 = OY + s->y * T, dw = s->w * T, dh = s->h * T; if (dw < 1) dw = T; if (dh < 1) dh = T;
    static const int wall[5][3] = {{42, 52, 68}, {168, 162, 146}, {186, 132, 96}, {142, 144, 140}, {168, 112, 88}};
    static const int glass[5][3] = {{150, 196, 214}, {196, 206, 196}, {210, 228, 232}, {120, 168, 190}, {230, 210, 170}};
    static const int trim[5][3] = {{24, 26, 30}, {120, 96, 48}, {160, 36, 36}, {70, 74, 72}, {196, 48, 56}};
    int k = kind % 5;
    for (int y = 0; y < dh; y++) for (int x = 0; x < dw; x++) {
        int r, g, b;
        if (tree) {
            /* Top-down: a round crown with the trunk in the middle. No side view. */
            int cx = dw / 2, cy = dh / 2, dx = x - cx, dy = y - cy, rad = (dw < dh ? dw : dh) / 2;
            int d2 = dx * dx + dy * dy;
            if (d2 > rad * rad) continue;
            if (d2 <= (rad / 5) * (rad / 5)) { r = 92; g = 64; b = 40; }
            else if (d2 > (rad * 3 / 4) * (rad * 3 / 4)) { r = 22; g = 72; b = 32; }
            else { r = 36; g = 118; b = 46; }
        } else {
            /* One floor only. The stories above this plan exist in 3D (glyph M or B), not in this picture. */
            int edge = dw / 7; if (edge < 2) edge = 2;
            int door = (y > dh - edge && x > dw * 2 / 5 && x < dw * 3 / 5);
            int rim = (x < edge || x >= dw - edge || y < edge || y >= dh - edge);
            r = wall[k][0] + 28; g = wall[k][1] + 24; b = wall[k][2] + 18;
            if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
            if (rim && !door) {
                r = wall[k][0]; g = wall[k][1]; b = wall[k][2];
                int win = (y > edge / 2 && y < edge + edge / 2) || (x > edge / 2 && x < edge);
                if (win && (x / (edge + 2)) % 2 == 0) { r = glass[k][0]; g = glass[k][1]; b = glass[k][2]; }
            } else if (door) { r = 48; g = 36; b = 28; }
        }
        put_px(x0 + x, y0 + y, r, g, b);
    }
}
static void fill_cell(int tx, int ty, int r, int g, int b) {
    int x0 = OX + tx * T, y0 = OY + ty * T;
    for (int y = 0; y < T; y++) { int Y = y0 + y; if (Y < 0 || Y >= FH) continue;
        for (int x = 0; x < T; x++) { int X = x0 + x; if (X < 0 || X >= FW) continue;
            unsigned char *q = fr + ((size_t)Y * FW + X) * 4; q[0] = (unsigned char)r; q[1] = (unsigned char)g; q[2] = (unsigned char)b; q[3] = 255; } }
}
static void kind_rgb(const char *k, int *r, int *g, int *b) {
    *r = 90; *g = 90; *b = 90;
    if (!strcmp(k, "body")) { *r = 70; *g = 74; *b = 82; }
    else if (!strcmp(k, "wheel")) { *r = 28; *g = 28; *b = 28; }
    else if (!strcmp(k, "engine")) { *r = 190; *g = 48; *b = 40; }
    else if (!strcmp(k, "glass")) { *r = 140; *g = 190; *b = 220; }
    else if (!strcmp(k, "door")) { *r = 40; *g = 90; *b = 170; }
    else if (!strcmp(k, "seat")) { *r = 150; *g = 96; *b = 48; }
    else if (!strcmp(k, "roof")) { *r = 50; *g = 52; *b = 58; }
    else if (!strcmp(k, "light")) { *r = 240; *g = 210; *b = 50; }
    else if (!strcmp(k, "head")) { *r = 220; *g = 176; *b = 140; }
    else if (!strcmp(k, "limb")) { *r = 196; *g = 140; *b = 110; }
}
/* Top-down clay for the GAP rows: car frame/engine/roof and a person, from shared/brixels.pdl sockets. Crosswalk stripes are painted on the road. CDDA parts that exist are drawn on top of the matching kind when emoji is off. */
static void draw_brixels(int ox, int oy) {
    char gp[PATH_MAX]; snprintf(gp, sizeof gp, "%s/shared/brixels.pdl", HOUSE);
    FILE *f = fopen(gp, "r"); if (!f) return;
    char kind_of[16][24], kind_name[16][16]; int nk = 0, line_w[16], line_h[16];
    char sock_name[24][24], sock_bx[24][24]; int sock_x[24], sock_y[24], ns = 0;
    char line[256];
    while (fgets(line, sizeof line, f)) {
        if (!strncmp(line, "BRIXEL |", 8) && nk < 16) {
            char id[24], kind[16]; int w, h, d;
            if (sscanf(line, "BRIXEL | %23s | %15s | %d | %d | %d", id, kind, &w, &h, &d) == 5) {
                snprintf(kind_of[nk], 24, "%s", id); snprintf(kind_name[nk], 16, "%s", kind); line_w[nk] = w; line_h[nk] = h; nk++;
            }
        } else if (!strncmp(line, "SOCKET |", 8) && ns < 24) {
            char bx[24], name[24], typ[24], face[8]; int x, y, z;
            if (sscanf(line, "SOCKET | %23s | %23s | %23s | %d | %d | %d | %7s", bx, name, typ, &x, &y, &z, face) == 7) {
                snprintf(sock_bx[ns], 24, "%s", bx); snprintf(sock_name[ns], 24, "%s", name); sock_x[ns] = x; sock_y[ns] = y; ns++;
            }
        }
    }
    fclose(f);
    int sc = T / 2; if (sc < 4) sc = 4;
    /* car root at (ox, oy), person one sidewalk row above, a few tiles to the right */
    for (int i = 0; i < nk; i++) {
        int px = 0, py = 0, on_car = !strncmp(kind_of[i], "car_", 4);
        if (!strcmp(kind_of[i], "car_frame")) { px = 0; py = 0; }
        else if (on_car) {
            for (int s = 0; s < ns; s++) if (!strcmp(sock_bx[s], "car_frame")) {
                if ((!strcmp(kind_of[i], "car_wheel") && !strcmp(sock_name[s], "fl_wheel")) ||
                    (!strcmp(kind_of[i], "car_engine") && !strcmp(sock_name[s], "engine_bay")) ||
                    (!strcmp(kind_of[i], "car_windshield") && !strcmp(sock_name[s], "windshield_frame")) ||
                    (!strcmp(kind_of[i], "car_door") && !strcmp(sock_name[s], "left_door")) ||
                    (!strcmp(kind_of[i], "car_seat") && !strcmp(sock_name[s], "driver_seat")) ||
                    (!strcmp(kind_of[i], "car_roof") && !strcmp(sock_name[s], "roof_mount")) ||
                    (!strcmp(kind_of[i], "car_headlight") && !strcmp(sock_name[s], "left_headlight"))) { px = sock_x[s]; py = sock_y[s]; }
            }
        } else {
            px = 10; py = -2;
            for (int s = 0; s < ns; s++) if (!strcmp(sock_bx[s], "person_torso")) {
                if ((!strcmp(kind_of[i], "person_head") && !strcmp(sock_name[s], "neck")) ||
                    (!strcmp(kind_of[i], "person_arm") && !strcmp(sock_name[s], "left_shoulder")) ||
                    (!strcmp(kind_of[i], "person_leg") && !strcmp(sock_name[s], "left_hip"))) { px = 10 + sock_x[s]; py = -2 + sock_y[s]; }
            }
            if (!strcmp(kind_of[i], "person_torso")) { px = 10; py = -2; }
        }
        int r, g, b; kind_rgb(kind_name[i], &r, &g, &b);
        int x0 = OX + ox * T + px * sc, y0 = OY + oy * T + py * sc;
        int dw = line_w[i] * sc, dh = line_h[i] * sc; if (dw < 2) dw = 2; if (dh < 2) dh = 2;
        int is_wheel = !strcmp(kind_of[i], "car_wheel");
        int is_glass = !strcmp(kind_of[i], "car_windshield");
        int is_light = !strcmp(kind_of[i], "car_headlight");
        int is_frame = !strcmp(kind_of[i], "car_frame");
        for (int y = 0; y < dh; y++) { int Y = y0 + y; if (Y < 0 || Y >= FH) continue;
            for (int x = 0; x < dw; x++) { int X = x0 + x; if (X < 0 || X >= FW) continue;
                int pr = r, pg = g, pb = b, draw = 1;
                if (is_wheel) {
                    int cx = dw / 2, cy = dh / 2, dx = x - cx, dy = y - cy;
                    int d2 = dx * dx + dy * dy, rad = (dw < dh ? dw : dh) / 2;
                    if (d2 > rad * rad) draw = 0;
                    else if (d2 < (rad / 3) * (rad / 3)) { pr = 150; pg = 150; pb = 146; }
                    else if (d2 > (rad * 3 / 4) * (rad * 3 / 4)) { pr = 16; pg = 16; pb = 16; }
                } else if (is_glass && x > dw / 6 && x < dw * 5 / 6 && y > dh / 5 && y < dh * 4 / 5 && x < dw / 3) { pr = 210; pg = 230; pb = 240; }
                else if (is_light && x > dw / 5 && x < dw * 4 / 5 && y > dh / 5 && y < dh * 4 / 5) { pr = 255; pg = 244; pb = 180; }
                else if (is_frame && y < dh / 5) { pr = r + 18; pg = g + 18; pb = b + 16; }
                else if (is_frame && y > dh * 3 / 5 && y < dh * 3 / 5 + 2) { pr = r - 16; pg = g - 16; pb = b - 14; }
                if (!draw) continue;
                if (pr < 0) pr = 0; if (pg < 0) pg = 0; if (pb < 0) pb = 0;
                if (pr > 255) pr = 255; if (pg > 255) pg = 255; if (pb > 255) pb = 255;
                unsigned char *q = fr + ((size_t)Y * FW + X) * 4; q[0] = (unsigned char)pr; q[1] = (unsigned char)pg; q[2] = (unsigned char)pb; q[3] = 255; } }
        if (!g_emoji && !strcmp(kind_of[i], "car_wheel")) cell_role("car_wheel", ox + px * sc / T, oy + py * sc / T, 1, 1);
        if (!g_emoji && !strcmp(kind_of[i], "car_windshield")) cell_role("car_windshield_n", ox + px * sc / T, oy + py * sc / T, 1, 1);
        if (!g_emoji && !strcmp(kind_of[i], "car_door")) cell_role("car_door", ox + px * sc / T, oy + py * sc / T, 1, 1);
    }
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
static void draw_over(const Room *rm) {
    for (int y = 0; y < rm->rows; y++) for (int x = 0; x < rm->cols; x++) { int dx = OX + x * T, dy = OY + y * T;
        if (!over_sea(rm, x, y)) { draw_auto("World_A2", 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, dx, dy); continue; }
        draw_auto("World_A1", 0, 0, over_sea(rm, x, y - 1), over_sea(rm, x + 1, y), over_sea(rm, x, y + 1), over_sea(rm, x - 1, y), over_sea(rm, x + 1, y - 1), over_sea(rm, x + 1, y + 1), over_sea(rm, x - 1, y + 1), over_sea(rm, x - 1, y - 1), dx, dy); }
}
#elif PET_SECTION == 22
    Room *rm = &rooms[vroom]; art_refresh(); char phase[40] = "day"; { char dp[PATH_MAX], db[64]; spath(dp, "daylight.txt"); read_file(dp, db, sizeof db); if (strstr(db, "night")) snprintf(phase, sizeof phase, "night"); else if (strstr(db, "dusk")) snprintf(phase, sizeof phase, "dusk"); else if (strstr(db, "dawn")) snprintf(phase, sizeof phase, "dawn"); }
    int cdda = g_art != 0;
    if (rm->over) {
        for (int y = 0; y < rm->rows; y++) for (int x = 0; x < rm->cols; x++) {
            if (over_sea(rm, x, y)) { if (g_art == 2) fill_cell(x, y, 40, 70, 140); else if (!(cdda && cell_role("water", x, y, 1, 1))) draw_auto("World_A1", 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, OX + x * T, OY + y * T); }
            else if (g_art == 2) fill_cell(x, y, 70, 120, 55); else if (!(cdda && cell_role("grass", x, y, 1, 1))) draw_auto("World_A2", 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, OX + x * T, OY + y * T);
        }
    } else if (rm->outdoor) {
        for (int x = 0; x < COLS; x++) {
            if (g_art == 2) fill_cell(x, WALL_ROW, 90, 70, 60); else if (!(cdda && cell_role("wall_brick", x, WALL_ROW, 1, 1))) tile("Outside_A5", 2, 0, 1, 1, x, WALL_ROW);
            for (int y = TRIG_ROW; y < ROWS; y++) {
                const char *role = (y == TRIG_ROW + 2) ? "road_yellow" : (y == TRIG_ROW + 1 || y == TRIG_ROW + 3) ? "sidewalk" : "grass";
                if (g_art == 2) {
                    if (y == TRIG_ROW + 2) fill_cell(x, y, 55, 55, 58);
                    else if (y == TRIG_ROW + 1 || y == TRIG_ROW + 3) fill_cell(x, y, 150, 148, 142);
                    else fill_cell(x, y, 70, 120, 55);
                    if (y == TRIG_ROW + 2 && x >= 6 && x <= 8 && (x % 2 == 0)) fill_cell(x, y, 230, 230, 230);
                } else if (!(cdda && cell_role(role, x, y, 1, 1))) tile("Outside_A5", 0, 2, 1, 1, x, y);
            }
        }
    } else
    for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLS; x++) {
        if (y == 0 || y == WALL_ROW) { if (g_art == 2) fill_cell(x, y, 90, 70, 60); else if (!(cdda && cell_role("wall_brick", x, y, 1, 1))) tile("Inside_A5", y == 0 ? 0 : 1, y == 0 ? 0 : 3, 1, 1, x, y); }
        else if (g_art == 2) fill_cell(x, y, 120, 110, 96); else if (!(cdda && cell_role("floor", x, y, 1, 1))) tile("Inside_A5", rm->fc, rm->fr, 1, 1, x, y);
    }
    for (int i = 0; i < ndoor; i++) if (doors[i].room == vroom && !rm->over) {
        if (g_art == 2) fill_cell(doors[i].tx, doors[i].ty, 120, 80, 40);
        else if (!(cdda && cell_role("door_closed", doors[i].tx, doors[i].ty, 1, 1))) { Sheet *dr = sheet("characters", "!Door1"); blit(dr, 0, 192, S48, S48, OX + doors[i].tx * T, OY + doors[i].ty * T); }
    }
    for (int i = 0; i < nscen; i++) if (scen[i].room == vroom) {
        if (g_art == 2) { paint_scenery(&scen[i], i); continue; }
        if (cdda && !strcmp(scen[i].sheet, "Outside_B") && cell_role("tree", scen[i].x, scen[i].y, scen[i].w, scen[i].h)) continue;
        tile(scen[i].sheet, scen[i].c, scen[i].r, scen[i].w, scen[i].h, scen[i].x, scen[i].y);
    }
#elif PET_SECTION == 23
    for (int a = 0; a < n; a++) { Placed *p = &pl[idx[a]]; Item *d = &cat[p->item]; if (g_art == 2) fill_cell(p->x, p->y, 160, 120, 70); else tile(d->sheet, d->c, d->r, d->w, d->h, p->x, p->y); }
    if (g_art == 2 && rm->over) draw_brixels(6, 5);
    else if (g_art == 2 && rm->outdoor) draw_brixels(1, TRIG_ROW + 2);
#elif PET_SECTION == 24
    for (int k = 0; k < (g_bg_only ? 0 : no); k++) { int i = order[k]; Ent *e = i < 0 ? &tr : &others[i]; int col = e->steps ? cyc[e->steps % 4] : 1, ci = i < 0 ? 0 : party[i].idx;
        if (g_art == 2 && blit_figure(i < 0 ? "voss" : party[i].id, e->x, e->y)) { }
        else if (g_art == 2 || g_emoji) fill_cell(e->x, e->y, g_emoji ? 255 : 220, g_emoji ? 200 : 176, g_emoji ? 40 : 140);
        else blit(sheet("characters", i < 0 ? hero.sheet : party[i].sheet), (ci % 4) * 3 * S48 + col * S48, (ci / 4) * 4 * S48 + e->dir * S48, S48, S48, OX + e->x * T, OY + e->y * T - 8 * T / S48); }
#elif PET_SECTION == 25
#elif PET_SECTION == 26
    { static const char *dflt[4] = {"hmm, tell me more.", "I'm listening.", "interesting!", "okay, Voss."}; unsigned h = 0; for (const char *c = s; *c; c++) h = h * 31 + (unsigned char)*c;
#elif PET_SECTION == 27
    if (pid == 0) { int fd = open("/dev/null", O_RDWR); if (fd >= 0) { dup2(fd, 0); dup2(fd, 1); dup2(fd, 2); } setenv("LC_CLOCK_NO_POPUP", "1", 1); if (daemon) setenv("LC_CLOCK_EVENT_RUNNER", SELF, 1); setenv("GTA_HQ_ROOT", APP, 1); signal(SIGPIPE, SIG_DFL); execv(LC, av); _exit(127); }
#elif PET_SECTION == 28
#elif PET_SECTION == 29
#elif PET_SECTION == 30
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
#elif PET_SECTION == 31
    char p[PATH_MAX]; spath(p, "scene.raw"); if (write_if_changed(p, fr, sizeof fr)) { g_scene_ver++; char png[PATH_MAX]; spath(png, "scene.png"); png_atomic(png, FW, FH, fr, FW * 4); }
#elif PET_SECTION == 32
    int chat = flag_on("chat", 0), hb = flag_on("hb", 0), in = flag_on("int", 0); char view[16]; flag_str("view", "2d", view, sizeof view); for (char *c = view; *c; c++) if (*c >= 'a' && *c <= 'z') *c -= 32;
    W("title=gta-hq  -  %s  -  coins %d\nscene_raw=%s\ncanvas_raw=%s\ncoins=%d\nmsg=%s\nroom_label=%s\n", rooms[vroom].name, coins, raw, raw, coins, msg && msg[0] ? msg : (in ? "INT on: turn based like a roguelike. Time moves when you act. Tab switches pet/player." : "real time: the pets live on their own. INT lets you control a pet or the player."), rooms[vroom].name);
#elif PET_SECTION == 33
    { char ab[16]; flag_str("art", "brixel", ab, sizeof ab); W("left_visible=%s\n", flag_on("left", 1) ? "1" : ""); W("chat_visible=%s\nhb_visible=%s\nview_label=view %s\nchat_toggle_label=window %s\nart=%s\nemoji=%d\nart_label=art %s%s\n", chat ? "1" : "", hb ? "1" : "", view, chat ? "on" : "off", ab, flag_on("emoji", 0), ab, flag_on("emoji", 0) ? " + emoji" : ""); }
    { char cp[PATH_MAX], cb[4000]; spath(cp, "chat.txt"); read_tail(cp, cb, sizeof cb); char *ln[6] = {0}; int k = 0; for (char *l = strtok(cb, "\n"); l; l = strtok(NULL, "\n")) { ln[k % 6] = l; k++; } for (int i = 0; i < 6; i++) { int j = k - 6 + i; W("chat_%d=%s\n", i, j >= 0 ? ln[j % 6] : ""); } }
#elif PET_SECTION == 34
#elif PET_SECTION == 35
#elif PET_SECTION == 36
    pet.room = d->dest; g_gr = pet.room; pet.x = d->ax; pet.y = d->ay; pet.dir = 0; pet.tx = pet.ty = -1; { int fx, fy; if (free_near(pet.x, pet.y, &fx, &fy)) { pet.x = fx; pet.y = fy; } }      /* land on the nearest FREE cell, not on whoever is already there */
    if (me >= 0 && me == active && (pet.room == vroom || di_from_room == vroom)) { snprintf(b, sizeof b, "went through the door to the %s", rooms[pet.room].name); say(party[me].name, b); }
#elif PET_SECTION == 37
#elif PET_SECTION == 38
#elif PET_SECTION == 39
#elif PET_SECTION == 40
#elif PET_SECTION == 41
#elif PET_SECTION == 42
    if (pet.tx < 0 && rand() % 100 < 45) pick_target();
#elif PET_SECTION == 43
#elif PET_SECTION == 44
static void end_turn(void) { if (!run_on() || !turn_mode()) return; clock_turn(); int in = flag_on("int", 0), c = ctl_idx(); for (int i = 0; i < nparty; i++) if (!(in && i == c)) ai_step(i); use_ent(active); }      /* the pet you control does not act on its own */
#elif PET_SECTION == 45
#elif PET_SECTION == 46
#elif PET_SECTION == 47
    FILE *f = fopen(p, "wb"); if (f) { fwrite(out, 1, sizeof out, f); fclose(f); }
}
/* The same 16x24 frame a pet uses, painted from the brixel front view. The 3D board extrudes every opaque pixel 8 deep. */
static void write_brixel_frame(const char *name, int dir4, int pat) {
    int n = figure_slot(name); if (n < 0 || !g_fig[n].ok) return;
    char d[PATH_MAX], p[PATH_MAX]; snprintf(d, sizeof d, "%s/../tsots-characters/frames", ASSETS); mkdir(d, 0755);
    static const int dc[4] = {2, 4, 6, 8};
    snprintf(p, sizeof p, "%s/%s_0_%d_%d.rgba", d, name, dc[dir4 & 3], pat);
    unsigned char out[16 * 24 * 4];
    for (int y = 0; y < 24; y++) for (int x = 0; x < 16; x++) {
        int sx = x * FIG_W / 16, sy = y * FIG_H / 24;
        memcpy(out + (y * 16 + x) * 4, g_fig[n].img + (sy * FIG_W + sx) * 4, 4);
    }
#elif PET_SECTION == 48
    snprintf(d, sizeof d, "%s/pieces/system/maps/gta-hq/%s", PCHQ, rooms[r].id); mkdir(d, 0755);
    art_refresh();
    for (int i = 0; i < nparty; i++) if (others[i].room == r) { int pat = others[i].steps % 3; unsigned char px[4] = {220, 176, 140, 255};
        if (g_art == 2) write_brixel_frame(party[i].id, others[i].dir, pat);
        else { write_frame(party[i].sheet, party[i].idx, others[i].dir, pat); frame_rgb(sheet("characters", party[i].sheet), party[i].idx, others[i].dir & 3, px); }
        o += snprintf(buf + o, sizeof buf - o, "%d %d %d %d %d %s %d %d %d %c\n", others[i].x, others[i].y, px[0], px[1], px[2], g_art == 2 ? party[i].id : party[i].sheet, g_art == 2 ? 0 : party[i].idx, dc[others[i].dir & 3], pat, party[i].name[0] >= 'A' && party[i].name[0] <= 'Z' ? party[i].name[0] + 32 : (party[i].name[0] ? party[i].name[0] : 'm')); }
    if (tr.room == r) { int pat = tr.steps % 3; unsigned char px[4] = {220, 176, 140, 255};
        if (g_art == 2) write_brixel_frame("voss", tr.dir, pat);
        else { write_frame(hero.sheet, 0, tr.dir, pat); frame_rgb(sheet("characters", hero.sheet), 0, tr.dir & 3, px); }
        o += snprintf(buf + o, sizeof buf - o, "%d %d %d %d %d %s 0 %d %d @\n", tr.x, tr.y, px[0], px[1], px[2], g_art == 2 ? "voss" : hero.sheet, dc[tr.dir & 3], pat); }
#elif PET_SECTION == 49
    static const char leg[] = "W|3|150|150|160|-|wall|#\nD|3|170|110|50|-|door|+\nT|2|50|150|60|-|tree|T\nf|1|190|150|90|-|furniture|&\n.|0|90|80|60|-|floor|.\n";
#elif PET_SECTION == 50
static int viewer_kinds(void) {
    static long last = 0; static int v = 3; long t = now_ms(); if (last && t - last < tune("viewer_scan_ms", 2000)) return v; last = t; v = 0;
#elif PET_SECTION == 51
        if (!strstr(b, "khtpm_core_render")) continue; for (size_t i = 0; i < n; i++) if (!b[i]) b[i] = ' '; if (strstr(b, "/@.apps/gta-hq/board/pchq-board.xhtpm")) v |= 1; if (strstr(b, "gta-hq.xhtpm")) v |= 2; }
#elif PET_SECTION == 52
#elif PET_SECTION == 53
#elif PET_SECTION == 54
        g_gr = r; char glyph[MAXR][MAXC + 1]; int fs[MAXR][MAXC], ws[MAXR][MAXC]; unsigned char a[XT * XT * 4], b[XT * XT * 4], fl[XT * XT * 4]; nslot = 1; memset(slotpx[0], 0, XT * XT * 4);
        Room *rm = &rooms[r]; Sheet *a5i = sheet("tilesets", "Inside_A5"), *a5o = sheet("tilesets", "Outside_A5"), *dr = sheet("characters", "!Door1");
#elif PET_SECTION == 55
        art_refresh();
        if (g_art == 2) {
            for (int i = 0; i < XT * XT; i++) { floorpx[i*4]=120; floorpx[i*4+1]=110; floorpx[i*4+2]=96; floorpx[i*4+3]=255; wallpx[i*4]=90; wallpx[i*4+1]=70; wallpx[i*4+2]=60; wallpx[i*4+3]=255; ceilpx[i*4]=70; ceilpx[i*4+1]=72; ceilpx[i*4+2]=78; ceilpx[i*4+3]=255; pathpx[i*4]=150; pathpx[i*4+1]=148; pathpx[i*4+2]=142; pathpx[i*4+3]=255; }
            for (int yy = 6; yy < 16; yy++) for (int xx = 6; xx < 18; xx++) { wallpx[(yy*XT+xx)*4]=160; wallpx[(yy*XT+xx)*4+1]=190; wallpx[(yy*XT+xx)*4+2]=200; }
        } else if (rm->outdoor) { tile24(a5o, rm->fc * S48, rm->fr * S48, floorpx); tile24(a5o, 2 * S48, 0, wallpx); memcpy(ceilpx, wallpx, sizeof ceilpx); tile24(a5o, 1 * S48, 2 * S48, pathpx); }
        else { tile24(a5i, rm->fc * S48, rm->fr * S48, floorpx); tile24(a5i, 1 * S48, 3 * S48, wallpx); tile24(a5i, 0, 0, ceilpx); memcpy(pathpx, floorpx, sizeof pathpx); }
        tile24(dr, 0, 192, doorpx); tile24(a5i, 0, 6 * S48, matpx);
#elif PET_SECTION == 56
            Sheet *s = sheet("tilesets", scen[i].sheet); memcpy(b, floorpx, sizeof b); tile24(s, (scen[i].c + ci) * S48, (scen[i].r + cj) * S48, a); over24(b, a); glyph[y][x] = scen[i].h >= 2 ? 'T' : 'f'; ws[y][x] = slot_of(b); }
        for (int i = 0; i < npl; i++) if (pl[i].room == r) { Item *it = &cat[pl[i].item]; Sheet *s = sheet("tilesets", it->sheet);
#elif PET_SECTION == 57
                tile24(s, (it->c + ci) * S48, (it->r + cj) * S48, a);
#elif PET_SECTION == 58
        if (rm->over) {      /* the overmap 3D: every cell = the painted 2D cell downscaled to a 24 px tile; the buildings (scenery) are 2 high blocks, the ocean is a flat floor */
            int sv = vroom; vroom = r; g_bg_only = 1; render(); g_bg_only = 0; vroom = sv; g_gr = r;
            for (int y = 0; y < rm->rows; y++) for (int x = 0; x < rm->cols; x++) { unsigned char cellpx[XT * XT * 4];
                for (int yy = 0; yy < XT; yy++) for (int xx = 0; xx < XT; xx++) { int sx = OX + x * T + xx * T / XT, sy = OY + y * T + yy * T / XT; memcpy(cellpx + (yy * XT + xx) * 4, fr + ((size_t)sy * FW + sx) * 4, 4); }
                int blocked = 0, bkind = -1; for (int i = 0; i < nscen; i++) if (scen[i].room == r && x >= scen[i].x && x < scen[i].x + scen[i].w && y >= scen[i].y && y < scen[i].y + scen[i].h) blocked = 1;
                char gch = '.'; if (blocked) { gch = 't'; int bk = 0; for (int i = 0; i < nscen; i++) if (scen[i].room == r && strcmp(scen[i].sheet, "Outside_B")) { if (x >= scen[i].x && x < scen[i].x + scen[i].w && y >= scen[i].y && y < scen[i].y + scen[i].h) { bkind = bk; gch = bk == 0 ? 'M' : 'B'; } bk++; } }
                glyph[y][x] = gch; fs[y][x] = slot_of(cellpx); ws[y][x] = 0;
                if (gch == 't') { unsigned char leaf[XT * XT * 4]; for (int yy = 0; yy < XT; yy++) for (int xx = 0; xx < XT; xx++) { int gi = (yy * XT + xx) * 4; int trunk = yy > XT * 2 / 3 && xx > XT / 3 && xx < XT * 2 / 3; int edge = xx < 2 || xx > XT - 3 || yy < 2; leaf[gi] = (unsigned char)(trunk ? 92 : edge ? 22 : 34); leaf[gi+1] = (unsigned char)(trunk ? 64 : edge ? 78 : 120); leaf[gi+2] = (unsigned char)(trunk ? 40 : edge ? 30 : 44); leaf[gi+3] = 255; } ws[y][x] = slot_of(leaf); }
                if (bkind >= 0) { unsigned char face[XT * XT * 4]; int wr = 42, wg = 52, wb = 68, gr = 150, gg = 196, gb = 214; if (bkind == 1) { wr = 168; wg = 162; wb = 146; } else if (bkind == 2) { wr = 186; wg = 132; wb = 96; } else if (bkind == 3) { wr = 142; wg = 144; wb = 140; } else if (bkind >= 4) { wr = 168; wg = 112; wb = 88; } for (int yy = 0; yy < XT; yy++) for (int xx = 0; xx < XT; xx++) { int gi = (yy * XT + xx) * 4; int glass = yy > XT / 5 && yy < XT * 3 / 5 && xx > XT / 6 && xx < XT * 5 / 6; face[gi] = (unsigned char)(glass ? gr : wr); face[gi+1] = (unsigned char)(glass ? gg : wg); face[gi+2] = (unsigned char)(glass ? gb : wb); face[gi+3] = 255; } ws[y][x] = slot_of(face); }
                /* The car is drawn at cell (6,5), 4 tiles by 2. 2D keeps that top-down picture. 3D gives those cells a body-high side so the car is a low block, not a flat decal. */
                if (x >= 6 && x < 10 && y >= 5 && y < 7 && glyph[y][x] == '.') { unsigned char side[XT * XT * 4]; for (int yy = 0; yy < XT; yy++) for (int xx = 0; xx < XT; xx++) { int gi = (yy * XT + xx) * 4; int glass = yy < XT / 3; int wheel = yy > XT * 2 / 3 && (xx < XT / 5 || xx > XT * 4 / 5); side[gi] = (unsigned char)(wheel ? 20 : glass ? 140 : 70); side[gi+1] = (unsigned char)(wheel ? 20 : glass ? 190 : 74); side[gi+2] = (unsigned char)(wheel ? 22 : glass ? 220 : 82); side[gi+3] = 255; } glyph[y][x] = 'C'; ws[y][x] = slot_of(side); } } }
        if (g_art == 2) for (int i = -1; i < nparty; i++) { Ent *e = i < 0 ? &tr : &others[i]; if (e->room != r || e->x < 0 || e->y < 0 || e->x >= COLS || e->y >= ROWS) continue; if (glyph[e->y][e->x] == 'B' || glyph[e->y][e->x] == 'M' || glyph[e->y][e->x] == 't' || glyph[e->y][e->x] == 'C') continue; (void)i; (void)e; }
#elif PET_SECTION == 59
        snprintf(p, sizeof p, "%s/extrusion.pdl", d); { FILE *f = fopen_tmp(p); if (f) { fprintf(f, "SECTION      | KEY                | VALUE\n----------------------------------------------------------------------\nMETA         | map_id             | gta-hq\nMETA         | desk_id            | %s\nMETA         | note               | %s (gta-hq house, exported from the live house)\nEXTRUDE      | W                  | 3\nEXTRUDE      | D                  | 3\nEXTRUDE      | T                  | 2\nEXTRUDE      | M                  | 8\nEXTRUDE      | B                  | 4\nEXTRUDE      | t                  | 4\nEXTRUDE      | C                  | 2\nEXTRUDE      | P                  | 3\nEXTRUDE      | f                  | 1\nEXTRUDE      | default            | 0\n", rooms[r].id, rooms[r].name); fclose_tmp(f, p); } }
#elif PET_SECTION == 60
static int copy_file(const char *from, const char *to) { FILE *in = fopen(from, "rb"); if (!in) return 0; char tmpc[PATH_MAX + 24]; snprintf(tmpc, sizeof tmpc, "%s.tmp.%d", to, (int)getpid()); FILE *out = fopen(tmpc, "wb"); if (!out) { fclose(in); return 0; } char buf[8192]; size_t n; int ok = 1; while ((n = fread(buf, 1, sizeof buf, in)) > 0) if (fwrite(buf, 1, n, out) != n) { ok = 0; break; } fclose(in); if (fclose(out) != 0) ok = 0; if (ok && rename(tmpc, to) != 0) ok = 0; if (!ok) remove(tmpc); return ok; }
#elif PET_SECTION == 61
#elif PET_SECTION == 62
    if (!strcmp(v, "newgame")) {      /* a fresh game: the current one is saved to slot 0 first (Undo new game = Load slot 0), then every game file is removed so the next read starts from the defaults. The clock is not touched. */
#elif PET_SECTION == 63
        static const char *fixed[] = { "house.txt", "trainer.txt", "relations.txt", "chat.txt", "active.txt", "daylight.txt", "flag_ctl.txt", "flag_follow.txt", "events.txt" };
#elif PET_SECTION == 64
        pet_load(); replay();
        snprintf(msg, cap, "new game started; the game you left is in slot 0 (Player > Undo new game)"); return; }
    if (!strcmp(v, "reset")) { char p[PATH_MAX], q[PATH_MAX]; spath(p, "house.txt"); snprintf(q, sizeof q, "%s/house.txt.%ld.bak", ST, now); if (fexists(p)) rename(p, q); snprintf(msg, cap, "house reset (the old ledger is kept as house.txt.%ld.bak)", now); return; }
#elif PET_SECTION == 65
        char p[PATH_MAX], bb[40]; spath(p, "active.txt"); snprintf(bb, sizeof bb, "%s\n", party[k].id); write_if_changed(p, bb, strlen(bb)); set_flag("follow", "pet"); set_flag("ctl", "pet"); active = k; calc_view(); snprintf(msg, cap, "watching %s (in the %s)", party[k].name, rooms[others[k].room].name); return; }
#elif PET_SECTION == 66
#elif PET_SECTION == 67
        char names[40][48]; int nn = 0; snprintf(names[nn++], 48, "house.txt"); snprintf(names[nn++], 48, "trainer.txt"); snprintf(names[nn++], 48, "relations.txt"); snprintf(names[nn++], 48, "chat.txt"); snprintf(names[nn++], 48, "active.txt"); snprintf(names[nn++], 48, "daylight.txt"); snprintf(names[nn++], 48, "flag_ctl.txt"); snprintf(names[nn++], 48, "flag_follow.txt"); snprintf(names[nn++], 48, "events.txt");      /* events.txt is bigger than the 64 KB read buffer below: streamed */
#elif PET_SECTION == 68
            if (!strcmp(names[i], "events.txt")) { if (copy_file(a, bpath)) ok++; continue; }
#elif PET_SECTION == 69
        if (!saving) { char b[40]; read_file(mp, b, sizeof b); long long ms = atoll(b); if (ms > 0) { clock_paths(); clock_install(); char ma[32]; snprintf(ma, sizeof ma, "%lld", ms); lc_run(0, "cmd", "rpg", "settime", ma, "--source", "rpg-load", NULL); lc_run(0, "step", "5", NULL); } pet_load(); }
        snprintf(msg, cap, "%s slot %d (%d files)", saving ? "saved to" : "loaded from", slot, ok); return; }
#elif PET_SECTION == 70
    if (!strcmp(v, "toggle") && argc >= 2 && (!strcmp(argv[1], "chat") || !strcmp(argv[1], "hb") || !strcmp(argv[1], "voice") || !strcmp(argv[1], "left"))) { set_flag(argv[1], flag_on(argv[1], !strcmp(argv[1], "voice") || !strcmp(argv[1], "left")) ? "0" : "1"); snprintf(msg, cap, "%s toggled", argv[1]); return; }
#elif PET_SECTION == 71
    signal(SIGTERM, bye); signal(SIGINT, bye); signal(SIGHUP, bye); srand((unsigned)(time(NULL) ^ getpid()));
#elif PET_SECTION == 72
        pet_load(); int in = flag_on("int", 0), tm = turn_mode(); static int prev_in = 0; static long int_on_at = 0; if (in && !prev_in) int_on_at = now_ms(); prev_in = in;
#elif PET_SECTION == 73
        if (n) { char kl[300], one[16]; kl[0] = 0; for (int i = 0; i < n && i < 20; i++) { snprintf(one, sizeof one, "%d ", codes[i]); strncat(kl, one, sizeof kl - strlen(kl) - 1); } char kp[PATH_MAX]; spath(kp, "keys_seen.txt"); write_if_changed(kp, kl, strlen(kl)); }
        for (int i = 0; i < n; i++) {
            if (codes[i] == 96) { set_flag("emoji", flag_on("emoji", 0) ? "0" : "1"); snprintf(msg, sizeof msg, "emoji %s", flag_on("emoji", 0) ? "on" : "off"); dirty = 1; }
            else if (codes[i] == 92) { char ab[16]; flag_str("art", "brixel", ab, sizeof ab); const char *nx = !strcmp(ab, "brixel") ? "rmmv" : !strcmp(ab, "rmmv") ? "cdda" : "brixel"; set_flag("art", nx); snprintf(msg, sizeof msg, "art %s", nx); dirty = 1; }
        }
#elif PET_SECTION == 74
        { char dp[PATH_MAX], db[40]; spath(dp, "daylight.txt"); read_file(dp, db, sizeof db); if (strcmp(db, lastday)) { snprintf(lastday, sizeof lastday, "%s", db); dirty = 1; } }
#elif PET_SECTION == 75
    signal(SIGPIPE, SIG_IGN);      /* a closed pipe / dead child must never kill a verb or the daemon (the children get SIG_DFL back before exec) */
    signal(SIGPIPE, SIG_IGN);      /* a closed pipe / dead child must never kill a verb or the daemon (the children get SIG_DFL back before exec) */
    snprintf(ST, sizeof ST, "%s/state", APP); mkdir(ST, 0755);
    snprintf(PCHQ, sizeof PCHQ, "%s/gta-hq-pchq", APP);
#elif PET_SECTION == 76
    load_data(); replay(); pet_load(); clock_paths();
#endif
#endif
