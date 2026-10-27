/* df_hq.c - df-hq on the shared pets-like core (&.widgits/_shared-lib/petslike_core.c). Two roles in one file: the top builds the program, and the core
 * re-includes this file once per SECTION to pull in what is specific to this game. Design: @.apps/../#.#.calendar-dox/!.HQ-IQ-BOOK/16.game/PETSLIKE-FRAMEWORK-PLAN.md */
#ifndef PET_SECTION
#define PET_APP "df-hq"
#define PET_OP "df_hq"
#define PET_SKIN "df_hq.c"
static const char *pl_life_bar_state(int slot);
#define PET_BAR_HP(slot) pl_life_bar_state(slot)      /* life bars in the bottom bar (the generic life layer, petslike_life.c; off until life.pdl says enabled 1) */
#include "../../../&.widgits/_shared-lib/petslike_core.c"
#else
#if PET_SECTION == 1
 * Every picture is an RPG Maker MV tile or character frame (sheets under #.NNEST_ASSETS/rmmv-www-img), nothing else is drawn.
#elif PET_SECTION == 2
static void ge_logline(const char *actor, const char *verb, const char *target, const char *res);
#define GE_LOG_HOOK(a, v, t, at, r) ge_logline(a, v, t, r)
#define GE_COMPACT_BYTES() ge_compact_limit()      /* tuning.pdl: ledger_compact_kb, default 4096 (4 MB) */
static long ge_compact_limit(void); static int tune(const char *name, int def); static void ge_failnote(const char *why);
#define GE_FAIL_HOOK(w) ge_failnote(w)
#include "../../../&.widgits/_shared-lib/game_events.c"      /* S1 event line + resolver (also df-hq, cdda-hq, mcl-hq) */
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
/* M3 decision ledger: the digest is the op dec_op (no text include any more); see dec_row / dec_exec below */
#elif PET_SECTION == 3
static void pl_life_verb(int argc, char **argv, char *msg, size_t cap); static void pl_life_ui(char *out, size_t cap); static int pl_life_tick(void);      /* the generic life layer (section 75) */
/* fortress state (see the fortress block below the entity code): rock rectangles, dug cells, pending dig marks */
typedef struct { int room, x, y, w, h; } Stone;
static Stone stn[8]; static int nstn;
typedef struct { int room, x, y; } Cell;
static Cell dug[256]; static int ndug;
static Cell job[64]; static int njob; static char jowner[64][16]; static Cell corner = { -1, 0, 0 };      /* jowner: who took the job (CLAIM row), "" = nobody yet; corner: first corner of an area dig (CORNER row) */
static long ev_off = 0, jb_off = 0;
#elif PET_SECTION == 4
#elif PET_SECTION == 5
    f = fopen(tmp, "wb"); if (!f) return 0; size_t wn = fwrite(data, 1, n, f); int ce = fclose(f); if (wn != n || ce != 0 || rename(tmp, p) != 0) { remove(tmp); return 0; } return 1;      /* ENOSPC / EIO: the good file stays, the half-written tmp goes (hq-crash-hardening.md) */
#elif PET_SECTION == 6
static struct { char k[48]; char v[64]; long t; } g_fc[32];
#elif PET_SECTION == 7
    char ck[48]; long cnow = 0; if (g_fcache_on) { snprintf(ck, sizeof ck, "%s|%s", name, def); cnow = now_ms(); for (int i = 0; i < g_fcn; i++) if (cnow - g_fc[i].t < tune("flag_cache_ms", 150) && !strcmp(g_fc[i].k, ck)) { snprintf(out, cap, "%s", g_fc[i].v); return; } }
#elif PET_SECTION == 8
    if (g_fcache_on) { int s = -1; for (int i = 0; i < g_fcn; i++) if (!strcmp(g_fc[i].k, ck)) s = i; if (s < 0 && g_fcn < 32) s = g_fcn++; if (s >= 0) { snprintf(g_fc[s].k, sizeof g_fc[s].k, "%s", ck); snprintf(g_fc[s].v, sizeof g_fc[s].v, "%s", out); g_fc[s].t = cnow; } }
#elif PET_SECTION == 9

/* ---------- Help panel (2026-10-17): help.txt in the app folder is the full help, pages separated by a line "---"; the panel (Help tab) shows one page at a time, 10 non-blank lines of 47 characters (blank lines are skipped: every row is tall). flags help (0/1) and help_page (0..); ui.txt gets help_visible and help_0..help_13 (help_0 = the "page N of M" title). ---------- */
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
static int help_pages(int want, char out[11][64]) {
    char p[PATH_MAX]; snprintf(p, sizeof p, "%s/help.txt", APP); FILE *f = fopen(p, "r"); int n = 1, k = 1; if (want >= 0) for (int i = 0; i < 11; i++) out[i][0] = 0;
    if (!f) { if (want >= 0) snprintf(out[1], 64, "help.txt is missing from the app folder"); return 1; }
    char ln[256]; while (fgets(ln, sizeof ln, f)) { ln[strcspn(ln, "\r\n")] = 0; if (!strcmp(ln, "---")) { n++; continue; } if (n - 1 == want && ln[0] && k < 11) { snprintf(out[k], 64, "%.47s", ln); k++; } }
    fclose(f); return n;
}
#elif PET_SECTION == 10
#elif PET_SECTION == 11
        rewind(fp); nstn = 0;
        while (fgets(line, sizeof line, fp)) { int n = split_bar(line, f, 12); if (n >= 6 && !strcmp(f[0], "STONE") && nstn < 8) { int a = room_idx(f[1]); if (a < 0) continue; stn[nstn++] = (Stone){ a, atoi(f[2]), atoi(f[3]), atoi(f[4]), atoi(f[5]) }; } }
#elif PET_SECTION == 12
    /* HARDENING: sizes and cells from rooms.pdl / catalog.pdl are untrusted: a scenery of width 2147483648999 made blit() loop for ever (found by the fuzz harness, hugenum on rooms.pdl) */
    for (int i = 0; i < nscen; i++) { Scen *s_ = &scen[i]; if (s_->c < 0 || s_->c > 255) s_->c = 0; if (s_->r < 0 || s_->r > 255) s_->r = 0; if (s_->w < 1 || s_->w > MAXC) s_->w = 1; if (s_->h < 1 || s_->h > MAXC) s_->h = 1; if (s_->x < -MAXC || s_->x > 2 * MAXC) s_->x = 0; if (s_->y < -MAXR || s_->y > 2 * MAXR) s_->y = 0; }
    for (int i = 0; i < ncat; i++) { Item *t_ = &cat[i]; if (t_->c < 0 || t_->c > 255) t_->c = 0; if (t_->r < 0 || t_->r > 255) t_->r = 0; if (t_->w < 1 || t_->w > 8) t_->w = 1; if (t_->h < 1 || t_->h > 8) t_->h = 1; if (t_->price < 0 || t_->price > 1000000) t_->price = 0; }
    for (int i = 0; i < ndoor; i++) { Door *d_ = &doors[i]; if (d_->tx < 0 || d_->tx >= MAXC) d_->tx = 0; if (d_->ty < 0 || d_->ty >= MAXR) d_->ty = 1; if (d_->ax < 0 || d_->ax >= MAXC) d_->ax = 0; if (d_->ay < 0 || d_->ay >= MAXR) d_->ay = 0; }
    for (int i = 0; i < nroom; i++) { if (rooms[i].fc < 0 || rooms[i].fc > 255) rooms[i].fc = 0; if (rooms[i].fr < 0 || rooms[i].fr > 255) rooms[i].fr = 0; }
    if (nroom == 0) { Room *r0 = &rooms[nroom++]; memset(r0, 0, sizeof *r0); snprintf(r0->id, 16, "home"); snprintf(r0->name, 32, "Home"); r0->cols = 13; r0->rows = 8; r0->tpx = 48; r0->trig = 2; }      /* HARDENING: rooms.pdl missing or all garbage = one default room, never a zero-size room (tpx 0 divides) */
#elif PET_SECTION == 13
/* ---------- fortress: rock, dig designations, dug cells (2026-10-16, sprint step D1) ----------
 * STONE rows in rooms.pdl are solid rock. A designation is a DESIGNATE row in state/jobs.txt; a free pet in that room walks to a tile next to it and digs, which appends
 * DIG to state/events.txt (the result) and DONE to jobs.txt. A dug cell is simply a DIG row: both files are append-only and read by a byte cursor, never by mtime. The rule
 * for HOW LONG a dig takes, tools, stone type and what drops is left empty on purpose (grok's: see FORTRESS-TODO in the README). */
static int cell_in(const Cell *a, int n, int room, int x, int y) { for (int i = 0; i < n; i++) if (a[i].room == room && a[i].x == x && a[i].y == y) return i; return -1; }
static int in_rock(int room, int x, int y) { for (int i = 0; i < nstn; i++) if (stn[i].room == room && x >= stn[i].x && x < stn[i].x + stn[i].w && y >= stn[i].y && y < stn[i].y + stn[i].h) return 1; return 0; }
static int is_stone(int room, int x, int y) { return in_rock(room, x, y) && cell_in(dug, ndug, room, x, y) < 0; }
static void tail_lines(const char *name, long *off, void (*fn)(char **, int)) {      /* feed the new WHOLE lines since byte *off to fn (append-only ledger, read by size growth) */
    char p[PATH_MAX], line[300]; spath(p, name); if (fsize(p) <= *off) return; FILE *fp = fopen(p, "r"); if (!fp) return; fseek(fp, *off, SEEK_SET);
    while (fgets(line, sizeof line, fp)) { size_t l = strlen(line); if (!l || line[l - 1] != '\n') break; *off += (long)l; char *f[8]; int n = split_bar(line, f, 8); fn(f, n); }
    fclose(fp);
}
static void ev_row(char **f, int n) { if (n >= 2 && !strcmp(f[0], "RESET")) { ndug = 0; return; }      /* RESET|ts: the rock is back (appended by the reset verb; the history before it stays in the file) */
    if (n >= 6 && !strcmp(f[0], "DIG") && ndug < 256) { int r = room_idx(f[3]), x = atoi(f[4]), y = atoi(f[5]); if (r >= 0 && cell_in(dug, ndug, r, x, y) < 0) dug[ndug++] = (Cell){ r, x, y }; } }
static void job_remove(int i) { job[i] = job[njob - 1]; memcpy(jowner[i], jowner[njob - 1], 16); njob--; }
/* jobs.txt rows: DESIGNATE|ts|room|x|y   CLAIM|ts|actor|room|x|y   DONE|ts|room|x|y   CANCEL|ts|room|x|y   CORNER|ts|room|x|y (room "-" clears the corner) */
static void jb_row(char **f, int n) {
    if (n >= 2 && !strcmp(f[0], "RESET")) { njob = 0; corner.room = -1; return; }
    if (n < 4) return;
    if (!strcmp(f[0], "CLAIM")) { if (n < 6) return; int r = room_idx(f[3]); if (r < 0) return; int i = cell_in(job, njob, r, atoi(f[4]), atoi(f[5])); if (i >= 0) snprintf(jowner[i], 16, "%s", f[2]); return; }
    int r = room_idx(f[2]);
    if (!strcmp(f[0], "CORNER")) { corner = (r < 0 || n < 5) ? (Cell){ -1, 0, 0 } : (Cell){ r, atoi(f[3]), atoi(f[4]) }; return; }
    if (r < 0) return; int x = atoi(f[3]), y = n >= 5 ? atoi(f[4]) : 0;
    if (!strcmp(f[0], "DESIGNATE") && njob < 64 && cell_in(job, njob, r, x, y) < 0) { job[njob] = (Cell){ r, x, y }; jowner[njob][0] = 0; njob++; }
    else if (!strcmp(f[0], "DONE") || !strcmp(f[0], "CANCEL")) { int i = cell_in(job, njob, r, x, y); if (i >= 0) job_remove(i); }
}
static long ep_off;      /* size of state/world_epoch.txt: one line is appended whenever a load replaces the ledgers wholesale */
static void world_sync(void) {
    char p[PATH_MAX];
    spath(p, "world_epoch.txt"); if (fsize(p) != ep_off) { ep_off = fsize(p); ndug = 0; ev_off = 0; njob = 0; jb_off = 0; corner.room = -1; }      /* a load swapped the files under us: read both from the start */
    spath(p, "events.txt"); if (fsize(p) < ev_off) { ndug = 0; ev_off = 0; }      /* a shorter file = the ledger was reset: start over */
    spath(p, "jobs.txt");   if (fsize(p) < jb_off) { njob = 0; jb_off = 0; corner.room = -1; }
    tail_lines("events.txt", &ev_off, ev_row); tail_lines("jobs.txt", &jb_off, jb_row);
}

#elif PET_SECTION == 14
    char p[PATH_MAX], line[300]; world_sync(); spath(p, "house.txt");
#elif PET_SECTION == 15
static void use_ent(int i) { me = i; pet = i < 0 ? tr : others[i]; if (pet.room >= 0 && pet.room < nroom) g_gr = pet.room; }
static int g_daemon;      /* 1 inside daemon_main: its own pet_save needs no marker, its memory is already current */
static void ent_mark(void) { append_line("ent_changed.txt", "."); }      /* append-only marker (size growth): a verb process changed an entity file, so the daemon re-reads them; N3 idle-cost gate */
#elif PET_SECTION == 16
    if (write_if_changed(p, b, strlen(b)) && !g_daemon) ent_mark(); if (me < 0) tr = pet; else others[me] = pet; calc_view();
#elif PET_SECTION == 17
static int over_land(const Room *rm, int x, int y);
static int floor_blocked(int room, int x, int y, int skip_id) { if (rooms[room].over && !over_land(&rooms[room], x, y)) return 9999; if (is_stone(room, x, y)) return 9999; for (int i = 0; i < nscen; i++) if (scen[i].room == room && x >= scen[i].x && x < scen[i].x + scen[i].w && y >= scen[i].y && y < scen[i].y + scen[i].h) return 9999; for (int i = 0; i < npl; i++) { Placed *q = &pl[i]; if (q->room != room || q->id == skip_id || cat[q->item].wall) continue; if (x >= q->x && x < q->x + cat[q->item].w && y >= q->y && y < q->y + cat[q->item].h) return q->id; } return 0; }
static int door_here(int room, int x, int y) { for (int i = 0; i < ndoor; i++) if (doors[i].room == room && doors[i].tx == x && y == doors[i].ty + 1) return i; return -1; }
/* overmap land: an island on a square ocean. Cells inside the rounded rectangle (2..cols-3, 1..rows-2, corners cut) are land, the outermost land ring is beach. Everything else is ocean (blocked). */
static int over_land(const Room *rm, int x, int y) { if (x < 2 || x > rm->cols - 3 || y < 1 || y > rm->rows - 2) return 0; if ((x <= 3 || x >= rm->cols - 4) && (y <= 2 || y >= rm->rows - 3)) return 0; return 1; }
static int over_beach(const Room *rm, int x, int y) { return over_land(rm, x, y) && (!over_land(rm, x - 1, y) || !over_land(rm, x + 1, y) || !over_land(rm, x, y - 1) || !over_land(rm, x, y + 1)); }
#elif PET_SECTION == 18
static void tile(const char *sh, int c, int r, int w, int h, int tx, int ty) { blit(sheet("tilesets", sh), c * S48, r * S48, w * S48, h * S48, OX + tx * T, OY + ty * T); }
/* Spacefox 16x16 is a CP437 sheet (256x256, 16 columns). Codes: 247 water, 176 shore, 250 floor, 219 wall, 5 tree, 15 building, 197 door. Other packs stay on disk unused until one is chosen. */
static Sheet df_sheet; static int df_sheet_try;
static Sheet *df_tiles(void) {
    if (df_sheet.px || df_sheet_try) return df_sheet.px ? &df_sheet : NULL; df_sheet_try = 1;
    char d[PATH_MAX]; snprintf(d, sizeof d, "%s", HOUSE[0] ? HOUSE : APP);
    for (int i = 0; i < 8; i++) { char p[PATH_MAX]; snprintf(p, sizeof p, "%s/#.NNEST_ASSETS/df-tilesets/Spacefox/data/art/Spacefox_16x16.png", d); if (fexists(p)) { int n; df_sheet.px = stbi_load(p, &df_sheet.w, &df_sheet.h, &n, 4); break; } char *s = strrchr(d, '/'); if (!s || s == d) break; *s = 0; }
    return df_sheet.px ? &df_sheet : NULL;
}
static void blit_fit(Sheet *s, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh) {
    if (!s || !s->px || sw < 1 || sh < 1 || dw < 1 || dh < 1 || dw > 4 * FW || dh > 4 * FH) return;
    for (int y = 0; y < dh; y++) { int Y = dy + y, srcy = sy + y * sh / dh; if (Y < 0 || Y >= FH || srcy < 0 || srcy >= s->h) continue;
        for (int x = 0; x < dw; x++) { int X = dx + x, srcx = sx + x * sw / dw; if (X < 0 || X >= FW || srcx < 0 || srcx >= s->w) continue;
            unsigned char *a = s->px + ((size_t)srcy * s->w + srcx) * 4, *b = fr + ((size_t)Y * FW + X) * 4; int al = a[3]; if (!al) continue;
            for (int c = 0; c < 3; c++) b[c] = (unsigned char)((a[c] * al + b[c] * (255 - al)) / 255); b[3] = 255; } }
}
static void df_glyph(int code, int tx, int ty, int tw, int th) {
    Sheet *s = df_tiles(); if (!s) return; int ts = 16; if (s->w < 256) ts = s->w / 16; if (ts < 1) return;
    int col = code % 16, row = code / 16;
    blit_fit(s, col * ts, row * ts, ts, ts, OX + tx * T, OY + (ty - (th - 1)) * T, tw * T, th * T);
}

/* RPG Maker autotiles (verified against the tsots maps: tile id = kind * 48 + shape, shape picked from the 8 neighbours of the same kind; all 8 present = shape 0; the 48 shapes are 4 quadrant pieces).
 * Same rule as _.monads/_.livedesk-taskbar/ops/tile_autotile.c's FLOOR_AUTOTILE_TABLE, computed per quadrant: for each corner look at its two adjacent sides and its diagonal. Pieces are 24 px
 * halves inside the kind's 2 x 3 tile block (bx,by = the block's top-left tile in the sheet). */
#include "../../../&.widgits/_shared-lib/petslike_autotile.c"
static void draw_auto(const char *sh, int bx, int by, int n, int e, int s, int w, int ne, int se, int sw, int nw, int dx, int dy) {
    Sheet *sp = sheet("tilesets", sh); int q[4][2]; sk_auto_quads(n, e, s, w, ne, se, sw, nw, q);
    for (int i = 0; i < 4; i++) blit(sp, bx * S48 + q[i][0] * 24, by * S48 + q[i][1] * 24, 24, 24, dx + (i % 2) * T / 2, dy + (i / 2) * T / 2);
}
/* the overmap biome "worldmap": the whole map is grass (World_A2 kind 0), the ocean is the World_A1 sea autotile (kind 0) whose edge shapes draw the shore wherever it meets land; the island is whatever over_land() says. */
static int over_sea(const Room *rm, int x, int y) { if (x < 0 || y < 0 || x >= rm->cols || y >= rm->rows) return 1; return !over_land(rm, x, y); }      /* outside the map counts as sea (MV treats the border as connected) */
static void draw_over(const Room *rm) {
    for (int y = 0; y < rm->rows; y++) for (int x = 0; x < rm->cols; x++) {
        int sea = over_sea(rm, x, y);
        int shore = !sea && (over_sea(rm, x, y - 1) || over_sea(rm, x + 1, y) || over_sea(rm, x, y + 1) || over_sea(rm, x - 1, y));
        df_glyph(sea ? 247 : shore ? 176 : 250, x, y, 1, 1);
    }
}
#elif PET_SECTION == 19
    Room *rm = &rooms[vroom]; char phase[40] = "day"; { char dp[PATH_MAX], db[64]; spath(dp, "daylight.txt"); read_file(dp, db, sizeof db); if (strstr(db, "night")) snprintf(phase, sizeof phase, "night"); else if (strstr(db, "dusk")) snprintf(phase, sizeof phase, "dusk"); else if (strstr(db, "dawn")) snprintf(phase, sizeof phase, "dawn"); }
    (void)phase;
    if (rm->over) draw_over(rm); else if (rm->outdoor) {
        for (int x = 0; x < COLS; x++) { df_glyph(219, x, WALL_ROW, 1, 1); for (int y = TRIG_ROW; y < ROWS; y++) df_glyph(250, x, y, 1, 1); }
    } else
    for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLS; x++) df_glyph((y == 0 || y == WALL_ROW) ? 219 : 250, x, y, 1, 1);
    for (int i = 0; i < ndoor; i++) if (doors[i].room == vroom && !rm->over) df_glyph(197, doors[i].tx, doors[i].ty, 1, 1);
    for (int i = 0; i < nscen; i++) if (scen[i].room == vroom) {
        if (!strcmp(scen[i].sheet, "Outside_B")) df_glyph(5, scen[i].x, scen[i].y, scen[i].w, scen[i].h);
        else if (!strcmp(scen[i].sheet, "World_B")) df_glyph(15, scen[i].x, scen[i].y, scen[i].w, scen[i].h);
        else tile(scen[i].sheet, scen[i].c, scen[i].r, scen[i].w, scen[i].h, scen[i].x, scen[i].y);
    }
    for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLS; x++) if (is_stone(vroom, x, y)) df_glyph(178, x, y, 1, 1);      /* rock (dug cells fall through to the floor) */
    for (int i = 0; i < njob; i++) if (job[i].room == vroom) df_glyph(jowner[i][0] ? 4 : 88, job[i].x, job[i].y, 1, 1);      /* X = marked for digging, nobody on it yet; a diamond = a dwarf has taken it */
    if (corner.room == vroom) df_glyph(43, corner.x, corner.y, 1, 1);      /* + = first corner of an area dig */
#elif PET_SECTION == 20
    for (int a = 0; a < n; a++) { Placed *p = &pl[idx[a]]; Item *d = &cat[p->item]; tile(d->sheet, d->c, d->r, d->w, d->h, p->x, p->y); }
#elif PET_SECTION == 21
    for (int k = 0; k < (g_bg_only ? 0 : no); k++) { int i = order[k]; Ent *e = i < 0 ? &tr : &others[i]; int col = e->steps ? cyc[e->steps % 4] : 1, ci = i < 0 ? 0 : party[i].idx;
        blit(sheet("characters", i < 0 ? hero.sheet : party[i].sheet), (ci % 4) * 3 * S48 + col * S48, (ci / 4) * 4 * S48 + e->dir * S48, S48, S48, OX + e->x * T, OY + e->y * T - 8 * T / S48); }
#elif PET_SECTION == 22
static void clock_stamp(char *out, size_t cap);
static void log_stamped(const char *who, const char *text) { char st[24], ln[300]; clock_stamp(st, sizeof st); snprintf(ln, sizeof ln, "%s%s%s %s", st, st[0] ? " " : "", who, text); append_line("chat.txt", ln); chat_trim(); }
static const char *log_name(const char *id, char *buf, size_t n) { if (!strcmp(id, "zed")) return "the zed"; snprintf(buf, n, "%s", id); if (buf[0] >= 'a' && buf[0] <= 'z') buf[0] = (char)(buf[0] - 32); return buf; }
/* the event log (grok 2026-10-17, from-sonnet-question-event-log.md): "D1 08:02 Rex crafts bandage" - the game clock, the actor, one present-tense line; appended once per newly resolved EVENT into chat.txt */
static void note_set(const char *s);
static long ge_compact_limit(void) { if (!g_tnn) load_tune(); return (long)tune("ledger_compact_kb", 4096) * 1024L; }      /* a verb process never ran load_tune(): read tuning.pdl on first use */
static void ge_failnote(const char *why) { note_set(why); }      /* a ledger over the hard cap: the reason goes to the note line (S1-EVENT-LEDGER.md) */
static void ge_logline(const char *actor, const char *verb, const char *target, const char *res) { char t[160], st[24], an[40], tn[40], ln[300]; t[0] = 0; (void)tn; 
    if (!t[0] && !ge_describe(verb, target, res, t, sizeof t)) return; clock_stamp(st, sizeof st); snprintf(ln, sizeof ln, "%s%s%s %s", st, st[0] ? " " : "", log_name(actor, an, sizeof an), t); append_line("chat.txt", ln); chat_trim(); }
#elif PET_SECTION == 23
    { static long long cdn = -1; long long dn = d - d0 + 1; if (dn != cdn) { cdn = dn;      /* "D2 00:00 the day turns" (grok 2026-10-17): the last day number lives in state/clock_lastday.txt (a verb is a new process), read only when the number changed; New game / Load going back in time writes no line */
        char lp[PATH_MAX], lb[24]; spath(lp, "clock_lastday.txt"); lb[0] = 0; read_file(lp, lb, sizeof lb); long long was = lb[0] ? atoll(lb) : -1;
        if (was >= 0 && dn > was) { char ln[40]; snprintf(ln, sizeof ln, "D%lld 00:00 the day turns", dn); append_line("chat.txt", ln); chat_trim(); }
        if (dn != was) { snprintf(lb, sizeof lb, "%lld", dn); write_if_changed(lp, lb, strlen(lb)); } } }
#elif PET_SECTION == 24
}
static void clock_stamp(char *out, size_t cap) {      /* "D3 06:10", the same arithmetic as clock_ui, computed now (the label file is up to 5 s old) */
    out[0] = 0; clock_paths(); if (!clock_ok()) return; long long ms = clock_kv_ll("game_time_epoch_ms"); if (ms < 0) return; char d0b[32], dp[PATH_MAX]; d0b[0] = 0; spath(dp, "clock_day0.txt"); read_file(dp, d0b, sizeof d0b);
    long long d0 = atoll(d0b), d = ms / 86400000, s = (ms - d * 86400000) / 1000; snprintf(out, cap, "D%lld %02d:%02d", d - d0 + 1, (int)(s / 3600), (int)(s % 3600 / 60));
#elif PET_SECTION == 25
#elif PET_SECTION == 26
    char p[PATH_MAX]; spath(p, "scene.raw"); if (write_if_changed(p, fr, sizeof fr)) g_scene_ver++;
#elif PET_SECTION == 27
    int chat = flag_on("chat", 0), hb = flag_on("hb", 0), in = flag_on("int", 0); char view[16]; flag_str("view", "2d", view, sizeof view); for (char *c = view; *c; c++) if (*c >= 'a' && *c <= 'z') *c -= 32;
    char nbuf[200]; { char np[PATH_MAX]; spath(np, "note.txt"); read_file(np, nbuf, sizeof nbuf); size_t nl = strlen(nbuf); while (nl && (nbuf[nl - 1] == '\n' || nbuf[nl - 1] == '\r')) nbuf[--nl] = 0; for (char *q_ = nbuf; *q_; q_++) if ((unsigned char)*q_ < 32) *q_ = ' '; }
    W("title=df-hq  -  %s  -  coins %d\nscene_raw=%s\ncanvas_raw=%s\ncoins=%d\nnote=%s\nmsg=%s\nroom_label=%s\n", rooms[vroom].name, coins, raw, raw, coins, nbuf, msg && msg[0] ? msg : (in ? "INT on: turn based like a roguelike. Time moves when you act. Tab switches pet/player." : "real time: the pets live on their own. INT lets you control a pet or the player."), rooms[vroom].name);
#elif PET_SECTION == 28
    W("left_visible=%s\n", flag_on("left", 1) ? "1" : "");      /* the pinned info bar (default on, Menu > Info show / hide, its _ button) */
    W("chat_visible=%s\nhb_visible=%s\nview_label=view %s\nchat_toggle_label=window %s\n", chat ? "1" : "", hb ? "1" : "", view, chat ? "on" : "off");
    { char lb[1200]; pl_life_ui(lb, sizeof lb); W("%s", lb); }      /* the bottom bar: a life bar per character (petslike_life.c) */
    { char cp[PATH_MAX], cb[4000]; spath(cp, "chat.txt"); read_tail(cp, cb, sizeof cb); char *ln[10] = {0}; int k = 0; for (char *l = strtok(cb, "\n"); l; l = strtok(NULL, "\n")) { ln[k % 10] = l; k++; } for (int i = 0; i < 10; i++) { int j = k - 10 + i; const char *cl = j >= 0 ? ln[j % 10] : ""; W("chat_%d=%s\nchat_cls_%d=%s\n", i, cl, i, (strstr(cl, " cannot ") || strstr(cl, " hits ") || strstr(cl, " has no ") || strstr(cl, " finds no ")) ? "rp-chat-bad" : (strstr(cl, " finds ") || strstr(cl, " crafts ")) ? "rp-chat-ok" : ""); }      /* the last ten lines (was six): the event log shares this scroll */

    { char hl[11][64]; char hb[16]; flag_str("help_page", "0", hb, sizeof hb); int pg = atoi(hb), np = help_pages(-1, hl); if (pg < 0 || pg >= np) pg = 0; help_pages(pg, hl); snprintf(hl[0], 64, "help - page %d of %d", pg + 1, np); W("help_visible=%s\n", flag_on("help", 0) ? "1" : ""); for (int i = 0; i < 11; i++) W("help_%d=%s\n", i, hl[i]); } }
#elif PET_SECTION == 29
    { static off_t sg_sz = -1; static long sg_ns = -1; static int sg_cnt[16]; struct stat sst; char evp[PATH_MAX]; spath(evp, "events.txt"); off_t zs = 0; long zn = 0; if (!stat(evp, &sst)) { zs = sst.st_size; zn = sst.st_mtim.tv_nsec; }
      if (zs != sg_sz || zn != sg_ns) { sg_sz = zs; sg_ns = zn; memset(sg_cnt, 0, sizeof sg_cnt); if (ge_scan(evp, (long)time(NULL), 0, NULL)) for (int i = 0; i < nparty && i < 16; i++) sg_cnt[i] = (int)ge_inv_get(party[i].id, "stone"); }      /* stone per dwarf: replay of events.txt, re-run only when the ledger file changed (size + mtime ns), read-only (resolve 0) */
      int tot = 0, ns = 0; for (int i = 0; i < nparty && i < 16; i++) { tot += sg_cnt[i]; if (sg_cnt[i] > 0) ns++; }
      W("stock_total=Stone total %d\nn_stock=%d\n", tot, ns); ns = 0; for (int i = 0; i < nparty && i < 16; i++) if (sg_cnt[i] > 0) { W("stock_%d_label=%.8s: stone %d\nstock_%d_key=%s\n", ns, party[i].name, sg_cnt[i], ns, party[i].id); ns++; }
      W("n_unit=%d\n", nparty); for (int i = 0; i < nparty; i++) W("unit_%d_label=%.6s, %.8s, %d stone\nunit_%d_key=%s\n", i, party[i].name, rooms[others[i].room].name, i < 16 ? sg_cnt[i] : 0, i, party[i].id); }
#elif PET_SECTION == 30
#elif PET_SECTION == 31
    pet.room = d->dest; g_gr = pet.room; pet.x = d->ax; pet.y = d->ay; pet.dir = 0; pet.tx = pet.ty = -1; { int fx, fy; if (free_near(pet.x, pet.y, &fx, &fy)) { pet.x = fx; pet.y = fy; } }      /* land on the nearest FREE cell, not on whoever is already there */
    if (me >= 0 && me == active && (pet.room == vroom || di_from_room == vroom)) { snprintf(b, sizeof b, "moves to the %s", rooms[pet.room].name); log_stamped(party[me].name, b); }      /* "D1 08:20 Rex moves to the Cellar" (grok 2026-10-17) */
#elif PET_SECTION == 32
#elif PET_SECTION == 33
/* dig jobs: the nearest marked cell in this pet's room, a free tile beside it, and the dig itself (the result is a DIG row; how long it takes / what drops is grok's rule) */
static int stand_tile(int jx, int jy, int *sx, int *sy);
static int g_jk = -1;      /* index of the job job_pick chose */
static int taken_by_other(int k) {      /* a dwarf other than me took it and still stands in that room: leave it to them (a dwarf that left the room gives it up) */
    if (!jowner[k][0]) return 0; const char *mine = me < 0 ? "trainer" : party[me].id; if (!strcmp(jowner[k], mine)) return 0;
    for (int j = 0; j < nparty; j++) if (!strcmp(party[j].id, jowner[k])) return others[j].room == job[k].room; return 0;
}
static int job_pick(int *jx, int *jy) { int best = -1, bd = 1 << 30; for (int k = 0; k < njob; k++) if (job[k].room == pet.room && !taken_by_other(k) && (abs(job[k].x - pet.x) + abs(job[k].y - pet.y) == 1 || stand_tile(job[k].x, job[k].y, &(int){0}, &(int){0}))) {      /* a cell with no free tile beside it (rock all round) waits until a neighbour is dug; it must not block the reachable ones */
         int d = abs(job[k].x - pet.x) + abs(job[k].y - pet.y); if (d < bd) { bd = d; best = k; } } if (best < 0) return 0; *jx = job[best].x; *jy = job[best].y; g_jk = best; return 1; }
static int stand_tile(int jx, int jy, int *sx, int *sy) {
    g_gr = pet.room; static const int nx[4] = {0, -1, 1, 0}, ny[4] = {1, 0, 0, -1}; int bd = 1 << 30, ok = 0;
    for (int k = 0; k < 4; k++) { int x = jx + nx[k], y = jy + ny[k]; if (x < 0 || x >= COLS || y < TRIG_ROW || y >= ROWS || floor_blocked(pet.room, x, y, 0) || other_at(pet.room, x, y) || door_here(pet.room, x, y) >= 0) continue; int d = abs(x - pet.x) + abs(y - pet.y); if (d < bd) { bd = d; *sx = x; *sy = y; ok = 1; } }
    return ok;
}
static int copy_file(const char *from, const char *to) { FILE *in = fopen(from, "rb"); if (!in) return 0; char tmpc[PATH_MAX + 24]; snprintf(tmpc, sizeof tmpc, "%s.tmp.%d", to, (int)getpid()); FILE *out = fopen(tmpc, "wb"); if (!out) { fclose(in); return 0; } char buf[8192]; size_t n; int ok = 1; while ((n = fread(buf, 1, sizeof buf, in)) > 0) if (fwrite(buf, 1, n, out) != n) { ok = 0; break; } fclose(in); if (fclose(out) != 0) ok = 0; if (ok && rename(tmpc, to) != 0) ok = 0; if (!ok) remove(tmpc); return ok; }
static void note_set(const char *s) { char p[PATH_MAX], b[320]; spath(p, "note.txt"); int n = snprintf(b, sizeof b, "%s\n", s); if (n >= (int)sizeof b) n = (int)sizeof b - 1; write_if_changed(p, b, (size_t)n); }      /* the last dig message, shown in the window (ui.txt note=): msg= is not painted anywhere */
static int dig_target(int argc, char **argv, int *room, int *x, int *y) {      /* the tile in front of the controlled mover, or two numbers = coordinates in the room being viewed */
    int ci = ctl_idx(); use_ent(ci); *room = pet.room;
    if (argc >= 3 && argv[1][0] >= '0' && argv[1][0] <= '9' && argv[2][0] >= '0' && argv[2][0] <= '9') { *room = vroom; *x = atoi(argv[1]); *y = atoi(argv[2]); }      /* the window appends extra words to a menu action: only two numbers mean coordinates */
    else { static const int ddx[4] = {0, -1, 1, 0}, ddy[4] = {1, 0, 0, -1}; *x = pet.x + ddx[pet.dir & 3]; *y = pet.y + ddy[pet.dir & 3]; }
    use_ent(active); return 1;
}
static void dig_now(int x, int y) {
    char b[160]; long t = (long)time(NULL); snprintf(b, sizeof b, "DIG|%ld|%s|%s|%d|%d|stone", t, me < 0 ? "trainer" : party[me].id, rooms[pet.room].id, x, y); append_line("events.txt", b);
    snprintf(b, sizeof b, "DONE|%ld|%s|%d|%d", t, rooms[pet.room].id, x, y); append_line("jobs.txt", b); world_sync();
    {   /* S1: the dug stone goes to the digger as an EVENT give_item (the event line of _shared-lib/game_events.c; the haul / stockpile rule is grok's, TODO in S1-EVENT-LEDGER.md) */
        char ep[PATH_MAX], er[40], at[40], nb[100]; const char *who = me < 0 ? "trainer" : party[me].id; spath(ep, "events.txt"); snprintf(at, sizeof at, "%s:%d,%d", rooms[pet.room].id, x, y);
        if (ge_append(ep, t, who, "give_item", "stone", at, er, sizeof er) && ge_scan(ep, t, 1, NULL)) { snprintf(nb, sizeof nb, "%s dug stone at %d,%d and carries %ld", who, x, y, ge_inv_get(who, "stone")); note_set(nb); } }
}
#elif PET_SECTION == 34
/* M3 decision ledger (BOOK 16.game/DECISION-LEDGER-AND-DAILY-REVIEW.md): one DEC row per autonomous choice of a pet (pick a target, walk, dig, blocked), written
 * AFTER the step from the before/after state, so ai_step_inner's own logic is untouched. Idle steps (nothing chosen) write nothing. */
/* HOT PATH = dec_row(): one O_APPEND write per decision. Everything else (bank, review, rotation, the pick chance) is the OP &.widgits/_shared-lib/ops/+x/dec_op.+x, run by
 * fork/exec off the tick (owner 2026-10-27: shared C is ops/events, not text includes; docs: BOOK 16.game/DECISION-LEDGER-AND-DAILY-REVIEW.md). */
static void dec_clean(char *o, size_t cap, const char *s) {
    size_t n = 0; if (!s || !*s) s = "-";
    for (; *s && n + 1 < cap && n < 40; s++) o[n++] = (*s == '|') ? '/' : (*s == '\n' || *s == '\r' || *s == '\t') ? ' ' : *s;
    o[n] = 0; if (!n) { o[0] = '-'; o[1] = 0; } }
static int dec_row(const char *path, const char *stamp, const char *actor, const char *state, const char *action, const char *target, const char *outcome) {
    char a[48], s[48], c[48], t[48], o[48], st[32], ln[400];
    dec_clean(a, sizeof a, actor); dec_clean(s, sizeof s, state); dec_clean(c, sizeof c, action); dec_clean(t, sizeof t, target); dec_clean(o, sizeof o, outcome);
    if (stamp && *stamp) dec_clean(st, sizeof st, stamp); else st[0] = 0;
    int n = snprintf(ln, sizeof ln, "DEC|%ld|%s|%s|%s|%s|%s|%s\n", (long)time(NULL), st, a, s, c, t, o); if (n <= 0 || n >= (int)sizeof ln) return 0;
    int fd = open(path, O_WRONLY | O_APPEND | O_CREAT, 0644); if (fd < 0) return 0;
    int ok = write(fd, ln, (size_t)n) == n; close(fd); return ok; }
/* run dec_op <verb> a b [c]. wait=0: fire and forget (one at a time; the previous child is reaped first, a still-running one means skip). wait=1: capture "rows=N" (5 s watchdog), return N, -1 on failure. */
static int dec_exec(const char *verb, const char *a, const char *b, const char *c, int wait) {
    static pid_t last = 0; int st; if (last > 0) { if (waitpid(last, &st, WNOHANG) == 0) { if (!wait) return 0; } else last = 0; }
    char exe[PATH_MAX]; snprintf(exe, sizeof exe, "%s/&.widgits/_shared-lib/ops/+x/dec_op.+x", HOUSE[0] ? HOUSE : APP); if (access(exe, X_OK)) return -1;
    int pf[2] = {-1, -1}; if (wait && pipe(pf)) return -1;
    pid_t pid = fork(); if (pid < 0) return -1;
    if (!pid) { int dn = open("/dev/null", O_RDWR); if (dn >= 0) { dup2(dn, 0); dup2(wait ? pf[1] : dn, 1); dup2(dn, 2); } if (wait) { close(pf[0]); close(pf[1]); }
        (void)!nice(10); execl(exe, exe, verb, a, b, c ? c : (char *)NULL, (char *)NULL); _exit(97); }
    if (!wait) { last = pid; return 0; }
    close(pf[1]); char out[64] = ""; size_t n = 0; time_t t0 = time(NULL); fcntl(pf[0], F_SETFL, O_NONBLOCK); int done = 0;
    while (!done && time(NULL) - t0 < 5) { ssize_t r = read(pf[0], out + n, sizeof out - 1 - n); if (r > 0) n += (size_t)r; else if (r == 0) done = 1; else usleep(10000); }
    out[n] = 0; close(pf[0]); if (!done) kill(pid, SIGKILL); waitpid(pid, &st, 0); char *q = strstr(out, "rows="); return (done && q) ? atoi(q + 5) : -1; }
static void dec_bank(void) { char dp[PATH_MAX], bp[PATH_MAX]; spath(dp, "decisions.txt"); spath(bp, "bank"); dec_exec("bank", dp, bp, "df-hq", 0); }      /* DEC rows -> bank entries + pick_pct (state/bank/) */
static void dec_log(const char *actor, const char *state, const char *action, const char *target, const char *outcome) {
    char p[PATH_MAX], st[24]; static int since; spath(p, "decisions.txt"); clock_stamp(st, sizeof st); dec_row(p, st, actor, state, action, target, outcome);
    if (++since >= 200) { since = 0; dec_bank(); } }      /* the bank follows the ledger every 200 decisions (off the tick, niced), and on the review verb */
/* the read-back that closes the loop: how readily a pet picks a new target. 45 % with no evidence; the op computes it from the walk weight (45 + (0.5 - w) * 60, clamped 15..85) and
 * writes state/bank/dec_df-hq_pick_pct.txt; a pet that keeps being blocked retargets sooner. The file is re-read at most every 10 s. */
static int dec_pick_pct(void) {
    static int pct = 45; static time_t at; time_t nw = time(NULL);
    if (nw - at >= 10) { char bp[PATH_MAX], fp[PATH_MAX + 64]; spath(bp, "bank"); snprintf(fp, sizeof fp, "%s/dec_df-hq_pick_pct.txt", bp); FILE *f = fopen(fp, "r"); int v; if (f) { if (fscanf(f, "%d", &v) == 1 && v >= 15 && v <= 85) pct = v; fclose(f); } at = nw; }
    return pct; }
static int ai_step_inner(int i);
#elif PET_SECTION == 35
    use_ent(i); Ent *e0 = i < 0 ? &tr : &others[i]; int tx0 = e0->tx, ty0 = e0->ty, x0 = e0->x, y0 = e0->y, r0 = e0->room;
    int moved = ai_step_inner(i); Ent *e = i < 0 ? &tr : &others[i]; char tg[32];
    const char *act = NULL, *out = "ok";
    if (moved && (e->x != x0 || e->y != y0 || e->room != r0)) { act = e->room != r0 ? "door" : "walk"; snprintf(tg, sizeof tg, "%d,%d", e->x, e->y); }
    else if (moved) { act = "dig"; snprintf(tg, sizeof tg, "%d,%d", e->x, e->y); }
    else if (tx0 < 0 && e->tx >= 0) { act = "pick"; snprintf(tg, sizeof tg, "%d,%d", e->tx, e->ty); }
    else if (tx0 >= 0) { act = "walk"; out = "blocked"; snprintf(tg, sizeof tg, "%d,%d", tx0, ty0); }
    if (act) dec_log(i < 0 ? "trainer" : party[i].id, tx0 >= 0 ? "goal" : "idle", act, tg, out);
    return moved;
}
static int ai_step_inner(int i) {
#elif PET_SECTION == 36
    { int jx, jy, sx, sy; if (job_pick(&jx, &jy)) {      /* a marked cell in this room comes before wandering */
        if (!jowner[g_jk][0]) { char cb[160]; snprintf(cb, sizeof cb, "CLAIM|%ld|%s|%s|%d|%d", (long)time(NULL), me < 0 ? "trainer" : party[me].id, rooms[pet.room].id, jx, jy); append_line("jobs.txt", cb); world_sync(); }
        if (abs(jx - pet.x) + abs(jy - pet.y) == 1) { dig_now(jx, jy); pet.tx = pet.ty = -1; pet_save(); return 1; }
        if (stand_tile(jx, jy, &sx, &sy)) { pet.tx = sx; pet.ty = sy; } } }
#elif PET_SECTION == 37
    if (pet.tx < 0 && rand() % 100 < dec_pick_pct()) pick_target();
#elif PET_SECTION == 38
#elif PET_SECTION == 39
static void end_turn(void) { if (!run_on() || !turn_mode()) return; clock_turn(); int in = flag_on("int", 0), c = ctl_idx(); for (int i = 0; i < nparty; i++) if (!(in && i == c)) ai_step(i); use_ent(active); }      /* the pet you control does not act on its own */
#elif PET_SECTION == 40
#elif PET_SECTION == 41
#elif PET_SECTION == 42
#elif PET_SECTION == 43
    snprintf(d, sizeof d, "%s/pieces/system/maps/df-hq/%s", PCHQ, rooms[r].id); mkdir(d, 0755);
    for (int i = 0; i < nparty; i++) if (others[i].room == r) { int pat = others[i].steps % 3; write_frame(party[i].sheet, party[i].idx, others[i].dir, pat); unsigned char px[4] = {128, 128, 128, 255}; frame_rgb(sheet("characters", party[i].sheet), party[i].idx, others[i].dir & 3, px);
        o += snprintf(buf + o, sizeof buf - o, "%d %d %d %d %d %s %d %d %d %c\n", others[i].x, others[i].y, px[0], px[1], px[2], party[i].sheet, party[i].idx, dc[others[i].dir & 3], pat, party[i].name[0] >= 'A' && party[i].name[0] <= 'Z' ? party[i].name[0] + 32 : (party[i].name[0] ? party[i].name[0] : 'm')); }
    if (tr.room == r) { int pat = tr.steps % 3; write_frame(hero.sheet, 0, tr.dir, pat); unsigned char px[4] = {200, 60, 60, 255}; frame_rgb(sheet("characters", hero.sheet), 0, tr.dir & 3, px);
        o += snprintf(buf + o, sizeof buf - o, "%d %d %d %d %d %s 0 %d %d @\n", tr.x, tr.y, px[0], px[1], px[2], hero.sheet, dc[tr.dir & 3], pat); }
#elif PET_SECTION == 44
    static const char leg[] = "W|3|150|150|160|-|wall|#\nD|3|170|110|50|-|door|+\nT|2|50|150|60|-|tree|T\nf|1|190|150|90|-|furniture|&\n.|0|90|80|60|-|floor|.\n";
#elif PET_SECTION == 45
static int viewer_kinds(void) {      /* N3 idle-cost gate: the /proc scan (every cmdline of every process, ~170 opens a second on a busy desktop, measured as most of this daemon's idle kernel time) now runs only when the launchers' process list changed size (every launcher appends to #.desktop/livedesk_proc_list.txt), when a window found by the last scan is gone, or every viewer_full_ms (30 s) as a safety net; in between the answer costs one kill(pid, 0) per known window. */
    static long last = 0, last_full = 0, sig = -1; static int v = 3, np = 0; static pid_t pids[8]; long t = now_ms();
    if (last && t - last < tune("viewer_scan_ms", 2000)) return v;
    last = t;
    char pl[PATH_MAX]; snprintf(pl, sizeof pl, "%s/#.desktop/livedesk_proc_list.txt", HOUSE); long s = fsize(pl); int gone = 0; for (int i = 0; i < np; i++) if (kill(pids[i], 0) != 0 && errno == ESRCH) gone = 1;
    if (last_full && s == sig && !gone && t - last_full < tune("viewer_full_ms", 30000)) return v;
    last_full = t; sig = s; v = 0; np = 0;
#elif PET_SECTION == 46
        if (!strstr(b, "khtpm_core_render")) continue; for (size_t i = 0; i < n; i++) if (!b[i]) b[i] = ' '; int hit = 0; if (strstr(b, "/@.apps/df-hq/board/pchq-board.xhtpm")) { v |= 1; hit = 1; } if (strstr(b, "df-hq.xhtpm")) { v |= 2; hit = 1; } if (hit && np < 8) pids[np++] = (pid_t)atoi(e->d_name); }
#elif PET_SECTION == 47
#elif PET_SECTION == 48
#elif PET_SECTION == 49
        g_gr = r; char glyph[MAXR][MAXC + 1]; int fs[MAXR][MAXC], ws[MAXR][MAXC]; unsigned char a[XT * XT * 4], b[XT * XT * 4], fl[XT * XT * 4]; nslot = 1; memset(slotpx[0], 0, XT * XT * 4);
        Room *rm = &rooms[r]; Sheet *a5i = sheet("tilesets", "Inside_A5"), *a5o = sheet("tilesets", "Outside_A5"), *dr = sheet("characters", "!Door1");
#elif PET_SECTION == 50
        if (rm->outdoor) { tile24(a5o, rm->fc * S48, rm->fr * S48, floorpx); tile24(a5o, 2 * S48, 0, wallpx); memcpy(ceilpx, wallpx, sizeof ceilpx); tile24(a5o, 1 * S48, 2 * S48, pathpx); }
        else { tile24(a5i, rm->fc * S48, rm->fr * S48, floorpx); tile24(a5i, 1 * S48, 3 * S48, wallpx); tile24(a5i, 0, 0, ceilpx); memcpy(pathpx, floorpx, sizeof pathpx); }
        tile24(dr, 0, 192, doorpx); tile24(a5i, 0, 6 * S48, matpx);
#elif PET_SECTION == 51
            Sheet *s = sheet("tilesets", scen[i].sheet); memcpy(b, floorpx, sizeof b); tile24(s, (scen[i].c + ci) * S48, (scen[i].r + cj) * S48, a); over24(b, a); glyph[y][x] = scen[i].h >= 2 ? 'T' : 'f'; ws[y][x] = slot_of(b); }
        for (int i = 0; i < npl; i++) if (pl[i].room == r) { Item *it = &cat[pl[i].item]; Sheet *s = sheet("tilesets", it->sheet);
#elif PET_SECTION == 52
                tile24(s, (it->c + ci) * S48, (it->r + cj) * S48, a);
#elif PET_SECTION == 53
        if (rm->over) {      /* the overmap 3D: every cell = the painted 2D cell downscaled to a 24 px tile; the buildings (scenery) are 2 high blocks, the ocean is a flat floor */
            int sv = vroom; vroom = r; g_bg_only = 1; render(); g_bg_only = 0; vroom = sv; g_gr = r;
            for (int y = 0; y < rm->rows; y++) for (int x = 0; x < rm->cols; x++) { unsigned char cellpx[XT * XT * 4];
                for (int yy = 0; yy < XT; yy++) for (int xx = 0; xx < XT; xx++) { int sx = OX + x * T + xx * T / XT, sy = OY + y * T + yy * T / XT; memcpy(cellpx + (yy * XT + xx) * 4, fr + ((size_t)sy * FW + sx) * 4, 4); }
                int blocked = 0; for (int i = 0; i < nscen; i++) if (scen[i].room == r && x >= scen[i].x && x < scen[i].x + scen[i].w && y >= scen[i].y && y < scen[i].y + scen[i].h) blocked = 1;
                glyph[y][x] = blocked ? 'T' : '.'; fs[y][x] = slot_of(cellpx); ws[y][x] = 0; } }
#elif PET_SECTION == 54
        snprintf(p, sizeof p, "%s/extrusion.pdl", d); { FILE *f = fopen_tmp(p); if (f) { fprintf(f, "SECTION      | KEY                | VALUE\n----------------------------------------------------------------------\nMETA         | map_id             | df-hq\nMETA         | desk_id            | %s\nMETA         | note               | %s (df-hq house, exported from the live house)\nEXTRUDE      | W                  | 3\nEXTRUDE      | D                  | 3\nEXTRUDE      | T                  | 2\nEXTRUDE      | f                  | 1\nEXTRUDE      | default            | 0\n", rooms[r].id, rooms[r].name); fclose_tmp(f, p); } }
#elif PET_SECTION == 55
#elif PET_SECTION == 56
    if (!strcmp(v, "helppanel")) {      /* Help tab: open / next / prev / close the help panel (help.txt, pages split by ---) */
        const char *w = argc >= 2 ? argv[1] : "open"; char hb[16]; flag_str("help_page", "0", hb, sizeof hb); int pg = atoi(hb), np = help_pages(-1, NULL);
        if (!strcmp(w, "close")) { set_flag("help", "0"); snprintf(msg, cap, "help closed"); return; }
        if (!strcmp(w, "next")) pg = (pg + 1) % np; else if (!strcmp(w, "prev")) pg = (pg + np - 1) % np; else if (!flag_on("help", 0)) pg = 0;
        snprintf(hb, sizeof hb, "%d", pg); set_flag("help_page", hb); set_flag("help", "1"); snprintf(msg, cap, "help page %d of %d", pg + 1, np); return;
    }
    if (!strcmp(v, "decpct")) { snprintf(msg, cap, "a pet without a target picks a new one with %d%% chance each step (base 45, moved by the walk weight in state/bank)", dec_pick_pct()); return; }
    if (!strcmp(v, "review")) {      /* M3: the daily review of state/decisions.txt -> state/review.txt (review [day-number], default the latest day) */
        char dp[PATH_MAX], rp[PATH_MAX]; spath(dp, "decisions.txt"); spath(rp, "review.txt"); char ds[16]; snprintf(ds, sizeof ds, "%d", argc > 1 ? atoi(argv[1]) : -1); int n = dec_exec("review", dp, rp, ds, 1); dec_bank();
        if (n < 0) snprintf(msg, cap, "review: no decisions yet"); else snprintf(msg, cap, "review: %d decisions reviewed, written to review.txt", n); return; }
    if (!strcmp(v, "compact")) {      /* S1 compaction (BOOK 16.game/S1-EVENT-LEDGER.md): force it now; the old ledger stays as state/events.archive.<n>.txt */
        char ep[PATH_MAX]; spath(ep, "events.txt"); GeCompact cc; if (!ge_compact(ep, (long)time(NULL), &cc)) { snprintf(msg, cap, "compact: not done (%s)", cc.why); return; }
        snprintf(msg, cap, "compact: %ld -> %ld bytes, old ledger kept as events.archive.%d.txt", cc.before, cc.after, cc.archive); return; }
    if (!strcmp(v, "event") || !strcmp(v, "resolve") || !strcmp(v, "inventory")) {      /* S1 (GAME-MECHANICS-SPRINT.md): the event line and resolver of _shared-lib/game_events.c on this game's state/events.txt. event <actor> <verb> <target> <room:x,y|-> appends one validated EVENT row and resolves it; resolve = resolve any row that has no EVRES yet (idempotent); inventory [actor] = replay of the ledger, writes nothing */
        char ep[PATH_MAX], er[60] = ""; spath(ep, "events.txt"); GeStats gs; long gnow = (long)time(NULL);
        if (!strcmp(v, "event")) { if (argc < 5) { snprintf(msg, cap, "event <actor> <verb> <target> <room:x,y or ->"); return; } if (!ge_append(ep, gnow, argv[1], argv[2], argv[3], argv[4], er, sizeof er)) { snprintf(msg, cap, "event refused: %s", er); return; } }
        if (!ge_scan(ep, gnow, strcmp(v, "inventory") != 0, &gs)) { snprintf(msg, cap, "ledger unreadable or not writable"); return; }
        if (!strcmp(v, "inventory")) { size_t o = 0; msg[0] = 0; for (int i = 0; i < ge_ninv; i++) if ((argc < 2 || !strcmp(argv[1], ge_inv[i].actor)) && ge_inv[i].n > 0 && o + 60 < cap) o += (size_t)snprintf(msg + o, cap - o, "%s:%s=%ld ", ge_inv[i].actor, ge_inv[i].item, ge_inv[i].n); if (!o) snprintf(msg, cap, "(nothing)"); return; }
        snprintf(msg, cap, "%s: %ld events, %ld resolved now, %ld rejected now (%ld rejects in all); last result %s", v, gs.events, gs.resolved_new, gs.rejected_new, gs.rejects_total, ge_last[0] ? ge_last : "-"); return; }
#elif PET_SECTION == 57
    if (!strcmp(v, "newgame")) {      /* a fresh game: the current one is saved to slot 0 first (Load slot 0 brings it back), then every game file is removed so the next read starts from the defaults. The clock is not touched. */
#elif PET_SECTION == 58
        static const char *fixed[] = { "house.txt", "trainer.txt", "relations.txt", "chat.txt", "active.txt", "daylight.txt", "flag_ctl.txt", "flag_follow.txt", "jobs.txt", "events.txt", "note.txt", "help_step.txt" };
#elif PET_SECTION == 59
        append_line("world_epoch.txt", "NEW"); world_sync(); pet_load(); replay();
        snprintf(msg, cap, "new game started; the game you left is in slot 0 (Player > Load slot 0)"); return; }
    if (!strcmp(v, "reset")) { char p[PATH_MAX], q[PATH_MAX]; spath(p, "house.txt"); snprintf(q, sizeof q, "%s/house.txt.%ld.bak", ST, now); if (fexists(p)) rename(p, q);
        { char rb[60]; snprintf(rb, sizeof rb, "RESET|%ld", now); append_line("events.txt", rb); append_line("jobs.txt", rb); world_sync(); }      /* the dug cells and the marks go too: the rock is back (rows before RESET stay as history) */
        snprintf(msg, cap, "house reset, the rock is back (the old ledger is kept as house.txt.%ld.bak)", now); return; }
#elif PET_SECTION == 60
        char p[PATH_MAX], bb[40]; spath(p, "active.txt"); snprintf(bb, sizeof bb, "%s\n", party[k].id); if (write_if_changed(p, bb, strlen(bb)) && !g_daemon) ent_mark(); set_flag("follow", "pet"); set_flag("ctl", "pet"); active = k; calc_view(); snprintf(msg, cap, "watching %s (in the %s)", party[k].name, rooms[others[k].room].name); return; }
#elif PET_SECTION == 61
    if (!strcmp(v, "dig")) {      /* mark a cell for digging: the tile in front of the controlled mover, or "dig <x> <y>" in the room being viewed. The pets do the work. */
        int room, x, y; dig_target(argc, argv, &room, &x, &y);
        if (!in_rock(room, x, y)) snprintf(msg, cap, "nothing to dig at %d,%d", x, y);
        else if (cell_in(dug, ndug, room, x, y) >= 0) snprintf(msg, cap, "%d,%d is already dug", x, y);
        else if (cell_in(job, njob, room, x, y) >= 0) snprintf(msg, cap, "%d,%d is already marked", x, y);
        else { char b[100]; snprintf(b, sizeof b, "DESIGNATE|%ld|%s|%d|%d", (long)time(NULL), rooms[room].id, x, y); append_line("jobs.txt", b); world_sync(); snprintf(msg, cap, "dig marked at %d,%d", x, y); }
        note_set(msg); return; }
    if (!strcmp(v, "help")) {      /* Menu > Help: one step of the dig instructions per click, painted in the note line (full text: BOOK 10-user-docs/DF-HQ-DIG-HOWTO.md) */
        static const char *H[6] = { "Dig help 1/6: turn INT on (top-left tab). The arrow keys then walk your pet, one turn per key.", "Dig help 2/6: walk up to the rock and face it (an arrow key into the rock turns you to it).", "Dig help 3/6: Menu > Dig: mark the tile in front. An X appears on the rock.",
            "Dig help 4/6: other pets in the room dig it on their turns. The X becomes a diamond once one has taken it.", "Dig help 5/6: Menu > Dig area: first use sets a corner, second use (facing the opposite corner) marks the whole block.", "Dig help 6/6: Menu > Dig: cancel the mark in front removes a mark. Click Help again to start over." };
        char hp[PATH_MAX], hb[16]; spath(hp, "help_step.txt"); read_file(hp, hb, sizeof hb); int s = ((atoi(hb) % 6) + 6) % 6; { char hv[16]; int hn = snprintf(hv, sizeof hv, "%d\n", s + 1); write_if_changed(hp, hv, (size_t)hn); }
        snprintf(msg, cap, "%s", H[s]); note_set(msg); return; }
    if (!strcmp(v, "undig")) {      /* cancel the mark on the tile in front (or "undig <x> <y>") */
        int room, x, y; dig_target(argc, argv, &room, &x, &y); int k = cell_in(job, njob, room, x, y);
        if (k < 0) snprintf(msg, cap, "no dig mark at %d,%d", x, y);
        else { char b[100]; snprintf(b, sizeof b, "CANCEL|%ld|%s|%d|%d", (long)time(NULL), rooms[room].id, x, y); append_line("jobs.txt", b); world_sync(); snprintf(msg, cap, "dig mark at %d,%d cancelled", x, y); }
        note_set(msg); return; }
    if (!strcmp(v, "dig_area")) {      /* area dig: first use stores a corner, second use marks every rock cell of the rectangle between the two corners (same room) */
        int room, x, y; dig_target(argc, argv, &room, &x, &y); char b[100];
        if (!in_rock(room, x, y)) snprintf(msg, cap, "nothing to dig at %d,%d: face rock to set a corner", x, y);
        else if (corner.room < 0 || corner.room != room) { snprintf(b, sizeof b, "CORNER|%ld|%s|%d|%d", (long)time(NULL), rooms[room].id, x, y); append_line("jobs.txt", b); world_sync(); snprintf(msg, cap, "corner 1 at %d,%d: face the opposite corner and use Dig area again", x, y); }
        else {
            int x0 = corner.x < x ? corner.x : x, x1 = corner.x < x ? x : corner.x, y0 = corner.y < y ? corner.y : y, y1 = corner.y < y ? y : corner.y, cnt = 0, full = 0;
            for (int yy = y0; yy <= y1; yy++) for (int xx = x0; xx <= x1; xx++) if (in_rock(room, xx, yy) && cell_in(dug, ndug, room, xx, yy) < 0 && cell_in(job, njob, room, xx, yy) < 0) {
                if (njob >= 64) { full = 1; continue; } snprintf(b, sizeof b, "DESIGNATE|%ld|%s|%d|%d", (long)time(NULL), rooms[room].id, xx, yy); append_line("jobs.txt", b); world_sync(); cnt++; }
            snprintf(b, sizeof b, "CORNER|%ld|-|0|0", (long)time(NULL)); append_line("jobs.txt", b); world_sync();
            snprintf(msg, cap, full ? "area %d,%d to %d,%d: %d cells marked (job list is full at 64)" : "area %d,%d to %d,%d: %d cells marked", x0, y0, x1, y1, cnt); }
        note_set(msg); return; }
#elif PET_SECTION == 62
        if (argc >= 2 && !strcmp(argv[1], "fortress")) {      /* the shipped test fortress (saves/test-fortress, tracked, read-only): the running game goes to slot 0 first, same refusal as New game; no clock file in it, so the clock is untouched */
            if (saving) { snprintf(msg, cap, "the test fortress is read-only"); return; } char m2[200] = ""; char *sv[] = { (char *)"save", (char *)"0", NULL }; verb(2, sv, m2, sizeof m2);
            char b0[PATH_MAX]; snprintf(b0, sizeof b0, "%s/slots/0/events.txt", ST); if (!fexists(b0)) { snprintf(msg, cap, "test fortress NOT loaded: the backup to slot 0 failed (%s)", m2); return; }
            snprintf(sd, sizeof sd, "%s/saves/test-fortress", APP); }
        char names[40][48]; int nn = 0; snprintf(names[nn++], 48, "house.txt"); snprintf(names[nn++], 48, "trainer.txt"); snprintf(names[nn++], 48, "relations.txt"); snprintf(names[nn++], 48, "chat.txt"); snprintf(names[nn++], 48, "active.txt"); snprintf(names[nn++], 48, "daylight.txt"); snprintf(names[nn++], 48, "flag_ctl.txt"); snprintf(names[nn++], 48, "flag_follow.txt"); snprintf(names[nn++], 48, "jobs.txt"); snprintf(names[nn++], 48, "events.txt");      /* the dug cells and the marks (events.txt is bigger than the 64 KB read buffer below: streamed) */
#elif PET_SECTION == 63
            if (!strcmp(names[i], "events.txt") || !strcmp(names[i], "jobs.txt")) { if (copy_file(a, bpath)) ok++; continue; }
#elif PET_SECTION == 64
        if (!saving) { append_line("world_epoch.txt", "LOAD"); world_sync(); }
        if (!saving) { char b[40]; read_file(mp, b, sizeof b); long long ms = atoll(b); if (ms > 0) { clock_paths(); clock_install(); char ma[32]; snprintf(ma, sizeof ma, "%lld", ms); lc_run(0, "cmd", "rpg", "settime", ma, "--source", "rpg-load", NULL); lc_run(0, "step", "5", NULL); } pet_load(); }
        if (argc >= 2 && !strcmp(argv[1], "fortress")) { snprintf(msg, cap, "test fortress loaded (%d files); your game is in slot 0 (Undo new game)", ok); return; } snprintf(msg, cap, "%s slot %d (%d files)", saving ? "saved to" : "loaded from", slot, ok); return; }
#elif PET_SECTION == 65
    if (!strcmp(v, "life")) { pl_life_verb(argc, argv, msg, cap); return; }
    if (!strcmp(v, "toggle") && argc >= 2 && (!strcmp(argv[1], "chat") || !strcmp(argv[1], "hb") || !strcmp(argv[1], "voice") || !strcmp(argv[1], "left"))) { set_flag(argv[1], flag_on(argv[1], !strcmp(argv[1], "voice") || !strcmp(argv[1], "left")) ? "0" : "1"); snprintf(msg, cap, "%s toggled", argv[1]); return; }
#elif PET_SECTION == 66
    signal(SIGTERM, bye); signal(SIGINT, bye); signal(SIGHUP, bye); srand((unsigned)(time(NULL) ^ getpid())); g_daemon = 1;
#elif PET_SECTION == 67
        {   /* N3 idle-cost gate (grok back-to-sonnet-oc16): pet_load re-read every pet file and the trainer on every 100 ms pass (measured 720 read calls/s, 106 KB/s idle). Now only when ent_changed.txt or world_epoch.txt GREW (a verb process changed an entity, a load or new game happened) or every pet_reload_ms (2 s) as a safety net for files written by hand; otherwise just the cheap reset of the mover and view. */
            static long sig = -1, last_pl = 0; char mk[PATH_MAX], we[PATH_MAX]; spath(mk, "ent_changed.txt"); spath(we, "world_epoch.txt"); if (fsize(mk) > 262144) { FILE *tf = fopen(mk, "w"); if (tf) fclose(tf); sig = -1; }      /* only this daemon reads the marker: keep it small */
            long ps = fsize(mk) * 1000003L + fsize(we), tn = now_ms();
            if (ps != sig || tn - last_pl >= tune("pet_reload_ms", 2000)) { sig = ps; last_pl = tn; pet_load(); } else { me = active; pet = others[active]; calc_view(); } }
        int in = flag_on("int", 0), tm = turn_mode(); static int prev_in = 0; static long int_on_at = 0; if (in && !prev_in) int_on_at = now_ms(); prev_in = in;
#elif PET_SECTION == 68
        if (in && n) { char kl[300], one[16]; kl[0] = 0; for (int i = 0; i < n && i < 20; i++) { snprintf(one, sizeof one, "%d ", codes[i]); strncat(kl, one, sizeof kl - strlen(kl) - 1); } char kp[PATH_MAX]; spath(kp, "keys_seen.txt"); write_if_changed(kp, kl, strlen(kl)); }      /* last batch of codes, for debugging */
#elif PET_SECTION == 69
        { char dp[PATH_MAX], db[40]; spath(dp, "daylight.txt"); read_file(dp, db, sizeof db); if (strcmp(db, lastday)) { snprintf(lastday, sizeof lastday, "%s", db); dirty = 1; } }
#elif PET_SECTION == 70
    signal(SIGPIPE, SIG_IGN);      /* a closed pipe / dead child must never kill a verb or the daemon (the children get SIG_DFL back before exec) */
    signal(SIGPIPE, SIG_IGN);      /* a closed pipe / dead child must never kill a verb or the daemon (the children get SIG_DFL back before exec) */
    snprintf(ST, sizeof ST, "%s/state", APP); mkdir(ST, 0755);
    snprintf(PCHQ, sizeof PCHQ, "%s/df-hq-pchq", APP);
#elif PET_SECTION == 71
    load_data(); replay(); pet_load(); clock_paths();
#elif PET_SECTION == 75
/* LIFE (owner 2026-10-27: the generic things every pets game has, backwards-merged from rpg-space): hp, hunger, death, respawn, life bars and the `life` verb from the shared text-include. OFF until this app's life.pdl says LIFE | enabled | 1. No drop list in this game (the PL_DROP hook defaults to nothing), so a death costs no items. */
#include "../../../&.widgits/_shared-lib/petslike_life.c"
#elif PET_SECTION == 79
        if (run_on() && pl_life_tick()) dirty = 1;      /* hunger, starving, death, respawn (life.pdl; nothing runs while it is disabled) */
#endif
#endif
