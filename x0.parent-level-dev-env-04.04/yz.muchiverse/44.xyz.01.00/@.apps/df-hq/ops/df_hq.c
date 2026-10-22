/* df_hq.c - df-hq on the shared pets-like core (&.widgits/_shared-lib/petslike_core.c). Two roles in one file: the top builds the program, and the core
 * re-includes this file once per SECTION to pull in what is specific to this game. Design: @.apps/../#.#.calendar-dox/!.HQ-IQ-BOOK/16.game/PETSLIKE-FRAMEWORK-PLAN.md */
#ifndef PET_SECTION
#define PET_APP "df-hq"
#define PET_OP "df_hq"
#define PET_SKIN "df_hq.c"
#include "../../../&.widgits/_shared-lib/petslike_core.c"
#else
#if PET_SECTION == 1      /* core line 21 */
 * Every picture is an RPG Maker MV tile or character frame (sheets under #.NNEST_ASSETS/rmmv-www-img), nothing else is drawn.
#elif PET_SECTION == 2      /* core line 52 */
static void ge_logline(const char *actor, const char *verb, const char *target, const char *res);
#define GE_LOG_HOOK(a, v, t, at, r) ge_logline(a, v, t, r)
#define GE_COMPACT_BYTES() ge_compact_limit()      /* tuning.pdl: ledger_compact_kb, default 4096 (4 MB) */
static long ge_compact_limit(void); static int tune(const char *name, int def); static void ge_failnote(const char *why);
#define GE_FAIL_HOOK(w) ge_failnote(w)
#include "../../../&.widgits/_shared-lib/game_events.c"      /* S1 event line + resolver (also df-hq, cdda-hq, mcl-hq) */
#include "../../../&.widgits/_shared-lib/game_decisions.c"      /* M3 decision ledger (append-only DEC rows + daily review) */
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
static char PCHQ[PATH_MAX];      /* this game's OWN pc-hq instance (data only: world, hero, xelector, maps, engine sessions): <app>/df-hq-pchq, see PCHQ-SESSIONS-DESIGN.md. Programs stay in @.apps/piececraft-hq. */
#define SIBLING_APP "/@.apps/" "rpg-pet" "/"      /* the carbon-copy game that shares the one pc-hq engine + world: its board window is NOT a viewer of this game (see viewer_kinds); the copy sets this to the other game */
#elif PET_SECTION == 10      /* core line 89 */
/* fortress state (see the fortress block below the entity code): rock rectangles, dug cells, pending dig marks */
typedef struct { int room, x, y, w, h; } Stone;
static Stone stn[8]; static int nstn;
typedef struct { int room, x, y; } Cell;
static Cell dug[256]; static int ndug;
static Cell job[64]; static int njob; static char jowner[64][16]; static Cell corner = { -1, 0, 0 };      /* jowner: who took the job (CLAIM row), "" = nobody yet; corner: first corner of an area dig (CORNER row) */
static long ev_off = 0, jb_off = 0;
#elif PET_SECTION == 11      /* core line 90 */
static Sheet sheets[24]; static int nsheet;
#elif PET_SECTION == 12      /* core line 139 */
#elif PET_SECTION == 13      /* core line 153 */
    f = fopen(tmp, "wb"); if (!f) return 0; size_t wn = fwrite(data, 1, n, f); int ce = fclose(f); if (wn != n || ce != 0 || rename(tmp, p) != 0) { remove(tmp); return 0; } return 1;      /* ENOSPC / EIO: the good file stays, the half-written tmp goes (hq-crash-hardening.md) */
#elif PET_SECTION == 14      /* core line 188 */
static struct { char k[48]; char v[64]; long t; } g_fc[32];
#elif PET_SECTION == 15      /* core line 190 */
    char ck[48]; long cnow = 0; if (g_fcache_on) { snprintf(ck, sizeof ck, "%s|%s", name, def); cnow = now_ms(); for (int i = 0; i < g_fcn; i++) if (cnow - g_fc[i].t < tune("flag_cache_ms", 150) && !strcmp(g_fc[i].k, ck)) { snprintf(out, cap, "%s", g_fc[i].v); return; } }
#elif PET_SECTION == 16      /* core line 192 */
    if (g_fcache_on) { int s = -1; for (int i = 0; i < g_fcn; i++) if (!strcmp(g_fc[i].k, ck)) s = i; if (s < 0 && g_fcn < 32) s = g_fcn++; if (s >= 0) { snprintf(g_fc[s].k, sizeof g_fc[s].k, "%s", ck); snprintf(g_fc[s].v, sizeof g_fc[s].v, "%s", out); g_fc[s].t = cnow; } }
#elif PET_SECTION == 17      /* core line 198 */

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
#elif PET_SECTION == 18      /* core line 209 */
    nparty = 0; char names[NP][24]; int nn = 0, inparty = 0, inhero = 0; memset(&hero, 0, sizeof hero); snprintf(hero.name, 24, "Harold"); snprintf(hero.sheet, 24, "Actor1");
#elif PET_SECTION == 19      /* core line 211 */
    if ((fp = fopen(p, "r"))) { while (fgets(line, sizeof line, fp)) { if (split_bar(line, f, 3) < 3 || strcmp(f[0], "SYSTEM")) continue; if (!strcmp(f[1], "name")) { inparty = !strcmp(f[2], getenv("PET_PARTY") && getenv("PET_PARTY")[0] ? getenv("PET_PARTY") : "DsrRpgParty"); inhero = !strcmp(f[2], "Party"); } else if (inhero && !strcmp(f[1], "member1")) snprintf(hero.name, 24, "%s", f[2]); else if (inparty && !strncmp(f[1], "member", 6) && nn < NP) snprintf(names[nn++], 24, "%s", f[2]); } fclose(fp); }
    for (int i = 0; i < nn; i++) { memset(&party[nparty], 0, sizeof party[nparty]); snprintf(party[nparty].name, 24, "%s", names[i]); snprintf(party[nparty].id, 16, "%s", names[i]); for (char *c = party[nparty].id; *c; c++) if (*c >= 'A' && *c <= 'Z') *c += 32; party[nparty].idx = i; snprintf(party[nparty].sheet, 24, "Monster"); nparty++; }
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
        rewind(fp); nstn = 0;
        while (fgets(line, sizeof line, fp)) { int n = split_bar(line, f, 12); if (n >= 6 && !strcmp(f[0], "STONE") && nstn < 8) { int a = room_idx(f[1]); if (a < 0) continue; stn[nstn++] = (Stone){ a, atoi(f[2]), atoi(f[3]), atoi(f[4]), atoi(f[5]) }; } }
#elif PET_SECTION == 26      /* core line 268 */
    /* HARDENING: sizes and cells from rooms.pdl / catalog.pdl are untrusted: a scenery of width 2147483648999 made blit() loop for ever (found by the fuzz harness, hugenum on rooms.pdl) */
    for (int i = 0; i < nscen; i++) { Scen *s_ = &scen[i]; if (s_->c < 0 || s_->c > 255) s_->c = 0; if (s_->r < 0 || s_->r > 255) s_->r = 0; if (s_->w < 1 || s_->w > MAXC) s_->w = 1; if (s_->h < 1 || s_->h > MAXC) s_->h = 1; if (s_->x < -MAXC || s_->x > 2 * MAXC) s_->x = 0; if (s_->y < -MAXR || s_->y > 2 * MAXR) s_->y = 0; }
    for (int i = 0; i < ncat; i++) { Item *t_ = &cat[i]; if (t_->c < 0 || t_->c > 255) t_->c = 0; if (t_->r < 0 || t_->r > 255) t_->r = 0; if (t_->w < 1 || t_->w > 8) t_->w = 1; if (t_->h < 1 || t_->h > 8) t_->h = 1; if (t_->price < 0 || t_->price > 1000000) t_->price = 0; }
    for (int i = 0; i < ndoor; i++) { Door *d_ = &doors[i]; if (d_->tx < 0 || d_->tx >= MAXC) d_->tx = 0; if (d_->ty < 0 || d_->ty >= MAXR) d_->ty = 1; if (d_->ax < 0 || d_->ax >= MAXC) d_->ax = 0; if (d_->ay < 0 || d_->ay >= MAXR) d_->ay = 0; }
    for (int i = 0; i < nroom; i++) { if (rooms[i].fc < 0 || rooms[i].fc > 255) rooms[i].fc = 0; if (rooms[i].fr < 0 || rooms[i].fr > 255) rooms[i].fr = 0; }
    if (nroom == 0) { Room *r0 = &rooms[nroom++]; memset(r0, 0, sizeof *r0); snprintf(r0->id, 16, "home"); snprintf(r0->name, 32, "Home"); r0->cols = 13; r0->rows = 8; r0->tpx = 48; r0->trig = 2; }      /* HARDENING: rooms.pdl missing or all garbage = one default room, never a zero-size room (tpx 0 divides) */
#elif PET_SECTION == 27      /* core line 279 */
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

#elif PET_SECTION == 28      /* core line 281 */
    char p[PATH_MAX], line[300]; world_sync(); spath(p, "house.txt");
#elif PET_SECTION == 29      /* core line 299 */
    e->room = 0; e->x = i < 0 ? 8 : 7 + i; e->y = i < 0 ? 8 : 7; e->dir = 0; e->steps = 0; e->tx = e->ty = -1;
#elif PET_SECTION == 30      /* core line 302 */
    { int cw = (nroom > 0 && e->room >= 0 && e->room < nroom) ? rooms[e->room].cols : 13, ch = (nroom > 0 && e->room >= 0 && e->room < nroom) ? rooms[e->room].rows : 8; if (cw < 1) cw = 1; if (ch < 1) ch = 1; if (cw > MAXC) cw = MAXC; if (ch > MAXR) ch = MAXR;
#elif PET_SECTION == 31      /* core line 313 */
static void use_ent(int i) { me = i; pet = i < 0 ? tr : others[i]; if (pet.room >= 0 && pet.room < nroom) g_gr = pet.room; }
static int g_daemon;      /* 1 inside daemon_main: its own pet_save needs no marker, its memory is already current */
static void ent_mark(void) { append_line("ent_changed.txt", "."); }      /* append-only marker (size growth): a verb process changed an entity file, so the daemon re-reads them; N3 idle-cost gate */
#elif PET_SECTION == 32      /* core line 316 */
    if (write_if_changed(p, b, strlen(b)) && !g_daemon) ent_mark(); if (me < 0) tr = pet; else others[me] = pet; calc_view();
#elif PET_SECTION == 33      /* core line 326 */
static int over_land(const Room *rm, int x, int y);
static int floor_blocked(int room, int x, int y, int skip_id) { if (rooms[room].over && !over_land(&rooms[room], x, y)) return 9999; if (is_stone(room, x, y)) return 9999; for (int i = 0; i < nscen; i++) if (scen[i].room == room && x >= scen[i].x && x < scen[i].x + scen[i].w && y >= scen[i].y && y < scen[i].y + scen[i].h) return 9999; for (int i = 0; i < npl; i++) { Placed *q = &pl[i]; if (q->room != room || q->id == skip_id || cat[q->item].wall) continue; if (x >= q->x && x < q->x + cat[q->item].w && y >= q->y && y < q->y + cat[q->item].h) return q->id; } return 0; }
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
    for (int y = 0; y < rm->rows; y++) for (int x = 0; x < rm->cols; x++) {
        int sea = over_sea(rm, x, y);
        int shore = !sea && (over_sea(rm, x, y - 1) || over_sea(rm, x + 1, y) || over_sea(rm, x, y + 1) || over_sea(rm, x - 1, y));
        df_glyph(sea ? 247 : shore ? 176 : 250, x, y, 1, 1);
    }
}
#elif PET_SECTION == 39      /* core line 456 */
    g_gr = vroom;
#elif PET_SECTION == 40      /* core line 458 */
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
#elif PET_SECTION == 41      /* core line 472 */
    for (int a = 0; a < n; a++) { Placed *p = &pl[idx[a]]; Item *d = &cat[p->item]; tile(d->sheet, d->c, d->r, d->w, d->h, p->x, p->y); }
#elif PET_SECTION == 42      /* core line 479 */
    for (int k = 0; k < (g_bg_only ? 0 : no); k++) { int i = order[k]; Ent *e = i < 0 ? &tr : &others[i]; int col = e->steps ? cyc[e->steps % 4] : 1, ci = i < 0 ? 0 : party[i].idx;
        blit(sheet("characters", i < 0 ? hero.sheet : party[i].sheet), (ci % 4) * 3 * S48 + col * S48, (ci / 4) * 4 * S48 + e->dir * S48, S48, S48, OX + e->x * T, OY + e->y * T - 8 * T / S48); }
#elif PET_SECTION == 43      /* core line 484 */
        if (tr_ || tg_ || tb_ || gy) for (int y = OY + ((od && !rm->over) ? 2 * T : 0); y < OY + ROWS * T; y++) for (int x = OX; x < OX + COLS * T; x++) { unsigned char *q = fr + ((size_t)y * FW + x) * 4; int r = q[0], g = q[1], b = q[2], l = (r * 30 + g * 59 + b * 11) / 100;
#elif PET_SECTION == 44      /* core line 489 */
      if (mr != 256 || mg != 256 || mb != 256) for (int y = OY + ((od && !rm->over) ? 2 * T : 0); y < OY + ROWS * T; y++) for (int x = OX; x < OX + COLS * T; x++) { unsigned char *q = fr + ((size_t)y * FW + x) * 4; q[0] = (unsigned char)(q[0] * mr / 256); q[1] = (unsigned char)(q[1] * mg / 256); q[2] = (unsigned char)(q[2] * mb / 256); } } }
#elif PET_SECTION == 45      /* core line 504 */
static void clock_stamp(char *out, size_t cap);
static void log_stamped(const char *who, const char *text) { char st[24], ln[300]; clock_stamp(st, sizeof st); snprintf(ln, sizeof ln, "%s%s%s %s", st, st[0] ? " " : "", who, text); append_line("chat.txt", ln); chat_trim(); }
static const char *log_name(const char *id, char *buf, size_t n) { if (!strcmp(id, "zed")) return "the zed"; snprintf(buf, n, "%s", id); if (buf[0] >= 'a' && buf[0] <= 'z') buf[0] = (char)(buf[0] - 32); return buf; }
/* the event log (grok 2026-10-17, from-sonnet-question-event-log.md): "D1 08:02 Rex crafts bandage" - the game clock, the actor, one present-tense line; appended once per newly resolved EVENT into chat.txt */
static void note_set(const char *s);
static long ge_compact_limit(void) { if (!g_tnn) load_tune(); return (long)tune("ledger_compact_kb", 4096) * 1024L; }      /* a verb process never ran load_tune(): read tuning.pdl on first use */
static void ge_failnote(const char *why) { note_set(why); }      /* a ledger over the hard cap: the reason goes to the note line (S1-EVENT-LEDGER.md) */
static void ge_logline(const char *actor, const char *verb, const char *target, const char *res) { char t[160], st[24], an[40], tn[40], ln[300]; t[0] = 0; (void)tn; 
    if (!t[0] && !ge_describe(verb, target, res, t, sizeof t)) return; clock_stamp(st, sizeof st); snprintf(ln, sizeof ln, "%s%s%s %s", st, st[0] ? " " : "", log_name(actor, an, sizeof an), t); append_line("chat.txt", ln); chat_trim(); }
#elif PET_SECTION == 46      /* core line 513 */
    { static const char *dflt[4] = {"hmm, tell me more.", "I'm listening.", "interesting!", "okay, Harold."}; unsigned h = 0; for (const char *c = s; *c; c++) h = h * 31 + (unsigned char)*c;
#elif PET_SECTION == 47      /* core line 594 */
    if (pid == 0) { int fd = open("/dev/null", O_RDWR); if (fd >= 0) { dup2(fd, 0); dup2(fd, 1); dup2(fd, 2); } setenv("LC_CLOCK_NO_POPUP", "1", 1); if (daemon) setenv("LC_CLOCK_EVENT_RUNNER", SELF, 1); setenv("DF_HQ_ROOT", APP, 1); signal(SIGPIPE, SIG_DFL); execv(LC, av); _exit(127); }
#elif PET_SECTION == 48      /* core line 612 */
    { static long long cdn = -1; long long dn = d - d0 + 1; if (dn != cdn) { cdn = dn;      /* "D2 00:00 the day turns" (grok 2026-10-17): the last day number lives in state/clock_lastday.txt (a verb is a new process), read only when the number changed; New game / Load going back in time writes no line */
        char lp[PATH_MAX], lb[24]; spath(lp, "clock_lastday.txt"); lb[0] = 0; read_file(lp, lb, sizeof lb); long long was = lb[0] ? atoll(lb) : -1;
        if (was >= 0 && dn > was) { char ln[40]; snprintf(ln, sizeof ln, "D%lld 00:00 the day turns", dn); append_line("chat.txt", ln); chat_trim(); }
        if (dn != was) { snprintf(lb, sizeof lb, "%lld", dn); write_if_changed(lp, lb, strlen(lb)); } } }
#elif PET_SECTION == 49      /* core line 613 */
}
static void clock_stamp(char *out, size_t cap) {      /* "D3 06:10", the same arithmetic as clock_ui, computed now (the label file is up to 5 s old) */
    out[0] = 0; clock_paths(); if (!clock_ok()) return; long long ms = clock_kv_ll("game_time_epoch_ms"); if (ms < 0) return; char d0b[32], dp[PATH_MAX]; d0b[0] = 0; spath(dp, "clock_day0.txt"); read_file(dp, d0b, sizeof d0b);
    long long d0 = atoll(d0b), d = ms / 86400000, s = (ms - d * 86400000) / 1000; snprintf(out, cap, "D%lld %02d:%02d", d - d0 + 1, (int)(s / 3600), (int)(s % 3600 / 60));
#elif PET_SECTION == 50      /* core line 647 */
#elif PET_SECTION == 51      /* core line 662 */
    char p[PATH_MAX]; spath(p, "scene.raw"); if (write_if_changed(p, fr, sizeof fr)) g_scene_ver++;
#elif PET_SECTION == 52      /* core line 667 */
    int chat = flag_on("chat", 0), hb = flag_on("hb", 0), in = flag_on("int", 0); char view[16]; flag_str("view", "2d", view, sizeof view); for (char *c = view; *c; c++) if (*c >= 'a' && *c <= 'z') *c -= 32;
    char nbuf[200]; { char np[PATH_MAX]; spath(np, "note.txt"); read_file(np, nbuf, sizeof nbuf); size_t nl = strlen(nbuf); while (nl && (nbuf[nl - 1] == '\n' || nbuf[nl - 1] == '\r')) nbuf[--nl] = 0; for (char *q_ = nbuf; *q_; q_++) if ((unsigned char)*q_ < 32) *q_ = ' '; }
    W("title=df-hq  -  %s  -  coins %d\nscene_raw=%s\ncanvas_raw=%s\ncoins=%d\nnote=%s\nmsg=%s\nroom_label=%s\n", rooms[vroom].name, coins, raw, raw, coins, nbuf, msg && msg[0] ? msg : (in ? "INT on: turn based like a roguelike. Time moves when you act. Tab switches pet/player." : "real time: the pets live on their own. INT lets you control a pet or the player."), rooms[vroom].name);
#elif PET_SECTION == 53      /* core line 672 */
    W("left_visible=%s\n", flag_on("left", 1) ? "1" : "");      /* the pinned info bar (default on, Menu > Info show / hide, its _ button) */
    W("chat_visible=%s\nhb_visible=%s\nview_label=view %s\nchat_toggle_label=window %s\n", chat ? "1" : "", hb ? "1" : "", view, chat ? "on" : "off");
    { char cp[PATH_MAX], cb[4000]; spath(cp, "chat.txt"); read_tail(cp, cb, sizeof cb); char *ln[10] = {0}; int k = 0; for (char *l = strtok(cb, "\n"); l; l = strtok(NULL, "\n")) { ln[k % 10] = l; k++; } for (int i = 0; i < 10; i++) { int j = k - 10 + i; const char *cl = j >= 0 ? ln[j % 10] : ""; W("chat_%d=%s\nchat_cls_%d=%s\n", i, cl, i, (strstr(cl, " cannot ") || strstr(cl, " hits ") || strstr(cl, " has no ") || strstr(cl, " finds no ")) ? "rp-chat-bad" : (strstr(cl, " finds ") || strstr(cl, " crafts ")) ? "rp-chat-ok" : ""); }      /* the last ten lines (was six): the event log shares this scroll */

    { char hl[11][64]; char hb[16]; flag_str("help_page", "0", hb, sizeof hb); int pg = atoi(hb), np = help_pages(-1, hl); if (pg < 0 || pg >= np) pg = 0; help_pages(pg, hl); snprintf(hl[0], 64, "help - page %d of %d", pg + 1, np); W("help_visible=%s\n", flag_on("help", 0) ? "1" : ""); for (int i = 0; i < 11; i++) W("help_%d=%s\n", i, hl[i]); } }
#elif PET_SECTION == 54      /* core line 676 */
    { static off_t sg_sz = -1; static long sg_ns = -1; static int sg_cnt[16]; struct stat sst; char evp[PATH_MAX]; spath(evp, "events.txt"); off_t zs = 0; long zn = 0; if (!stat(evp, &sst)) { zs = sst.st_size; zn = sst.st_mtim.tv_nsec; }
      if (zs != sg_sz || zn != sg_ns) { sg_sz = zs; sg_ns = zn; memset(sg_cnt, 0, sizeof sg_cnt); if (ge_scan(evp, (long)time(NULL), 0, NULL)) for (int i = 0; i < nparty && i < 16; i++) sg_cnt[i] = (int)ge_inv_get(party[i].id, "stone"); }      /* stone per dwarf: replay of events.txt, re-run only when the ledger file changed (size + mtime ns), read-only (resolve 0) */
      int tot = 0, ns = 0; for (int i = 0; i < nparty && i < 16; i++) { tot += sg_cnt[i]; if (sg_cnt[i] > 0) ns++; }
      W("stock_total=Stone total %d\nn_stock=%d\n", tot, ns); ns = 0; for (int i = 0; i < nparty && i < 16; i++) if (sg_cnt[i] > 0) { W("stock_%d_label=%.8s: stone %d\nstock_%d_key=%s\n", ns, party[i].name, sg_cnt[i], ns, party[i].id); ns++; }
      W("n_unit=%d\n", nparty); for (int i = 0; i < nparty; i++) W("unit_%d_label=%.6s, %.8s, %d stone\nunit_%d_key=%s\n", i, party[i].name, rooms[others[i].room].name, i < 16 ? sg_cnt[i] : 0, i, party[i].id); }
#elif PET_SECTION == 55      /* core line 688 */
#elif PET_SECTION == 56      /* core line 696 */
static int passable(int x, int y) { g_gr = pet.room; return x >= 0 && x < COLS && y >= TRIG_ROW && y < ROWS && !floor_blocked(pet.room, x, y, 0) && !other_at(pet.room, x, y); }
#elif PET_SECTION == 57      /* core line 700 */
    pet.room = d->dest; g_gr = pet.room; pet.x = d->ax; pet.y = d->ay; pet.dir = 0; pet.tx = pet.ty = -1; { int fx, fy; if (free_near(pet.x, pet.y, &fx, &fy)) { pet.x = fx; pet.y = fy; } }      /* land on the nearest FREE cell, not on whoever is already there */
    if (me >= 0 && me == active && (pet.room == vroom || di_from_room == vroom)) { snprintf(b, sizeof b, "moves to the %s", rooms[pet.room].name); log_stamped(party[me].name, b); }      /* "D1 08:20 Rex moves to the Cellar" (grok 2026-10-17) */
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
#elif PET_SECTION == 65      /* core line 743 */
/* M3 decision ledger (BOOK 16.game/DECISION-LEDGER-AND-DAILY-REVIEW.md): one DEC row per autonomous choice of a pet (pick a target, walk, dig, blocked), written
 * AFTER the step from the before/after state, so ai_step_inner's own logic is untouched. Idle steps (nothing chosen) write nothing. */
static void dec_bank(void) { char dp[PATH_MAX], bp[PATH_MAX]; spath(dp, "decisions.txt"); spath(bp, "bank"); dec_bank_update(dp, bp, "df-hq"); }      /* DEC rows -> bank entries (state/bank/dec_df-hq_<action>.behavior.pdl) */
static void dec_log(const char *actor, const char *state, const char *action, const char *target, const char *outcome) {
    char p[PATH_MAX], st[24]; static int since; spath(p, "decisions.txt"); clock_stamp(st, sizeof st); dec_append(p, st, actor, state, action, target, outcome);
    if (++since >= 200) { since = 0; dec_bank(); } }      /* the bank follows the ledger every 200 decisions, and on the review verb */
/* the read-back that closes the loop: how readily a pet picks a new target. 45 % with no evidence; a low success weight of its own walk choice (it keeps being blocked)
 * raises it (it gives up and retargets sooner), a high one lowers it (it keeps its goal longer). Clamped 15..85; the file is re-read at most every 10 s. */
static int dec_pick_pct(void) {
    static double w = 0.5; static time_t at; time_t nw = time(NULL);
    if (nw - at >= 10) { char bp[PATH_MAX]; spath(bp, "bank"); w = dec_weight(bp, "df-hq", "walk"); at = nw; }
    int pct = 45 + (int)((0.5 - w) * 60.0); return pct < 15 ? 15 : pct > 85 ? 85 : pct; }
static int ai_step_inner(int i);
#elif PET_SECTION == 66      /* core line 744 */
    use_ent(i); Ent *e0 = i < 0 ? &tr : &others[i]; int tx0 = e0->tx, x0 = e0->x, y0 = e0->y, r0 = e0->room;
    int moved = ai_step_inner(i); Ent *e = i < 0 ? &tr : &others[i]; char tg[32];
    const char *act = NULL, *out = "ok";
    if (moved && (e->x != x0 || e->y != y0 || e->room != r0)) { act = e->room != r0 ? "door" : "walk"; snprintf(tg, sizeof tg, "%d,%d", e->x, e->y); }
    else if (moved) { act = "dig"; snprintf(tg, sizeof tg, "%d,%d", e->x, e->y); }
    else if (tx0 < 0 && e->tx >= 0) { act = "pick"; snprintf(tg, sizeof tg, "%d,%d", e->tx, e->ty); }
    else if (tx0 >= 0) { act = "walk"; out = "blocked"; snprintf(tg, sizeof tg, "%d,%d", tx0, e0->ty); }
    if (act) dec_log(i < 0 ? "trainer" : party[i].id, tx0 >= 0 ? "goal" : "idle", act, tg, out);
    return moved;
}
static int ai_step_inner(int i) {
#elif PET_SECTION == 67      /* core line 745 */
    { int jx, jy, sx, sy; if (job_pick(&jx, &jy)) {      /* a marked cell in this room comes before wandering */
        if (!jowner[g_jk][0]) { char cb[160]; snprintf(cb, sizeof cb, "CLAIM|%ld|%s|%s|%d|%d", (long)time(NULL), me < 0 ? "trainer" : party[me].id, rooms[pet.room].id, jx, jy); append_line("jobs.txt", cb); world_sync(); }
        if (abs(jx - pet.x) + abs(jy - pet.y) == 1) { dig_now(jx, jy); pet.tx = pet.ty = -1; pet_save(); return 1; }
        if (stand_tile(jx, jy, &sx, &sy)) { pet.tx = sx; pet.ty = sy; } } }
#elif PET_SECTION == 68      /* core line 746 */
    if (pet.tx < 0 && rand() % 100 < dec_pick_pct()) pick_target();
#elif PET_SECTION == 69      /* core line 752 */
#elif PET_SECTION == 70      /* core line 753 */
static void end_turn(void) { if (!run_on() || !turn_mode()) return; clock_turn(); int in = flag_on("int", 0), c = ctl_idx(); for (int i = 0; i < nparty; i++) if (!(in && i == c)) ai_step(i); use_ent(active); }      /* the pet you control does not act on its own */
#elif PET_SECTION == 71      /* core line 763 */
#elif PET_SECTION == 72      /* core line 782 */
#elif PET_SECTION == 73      /* core line 795 */
#elif PET_SECTION == 74      /* core line 799 */
    if (!s) return; int fx = (idx % 4) * 3 * S48 + S48 + S48 / 2, fy = (idx / 4) * 4 * S48 + dir4 * S48 + S48 / 2; if (fx < s->w && fy < s->h) memcpy(px, s->px + ((size_t)fy * s->w + fx) * 4, 4);
#elif PET_SECTION == 75      /* core line 806 */
    static unsigned char crop[FW * FH * 4]; unsigned long sum = 5381; for (int y = 0; y < ROWS * T; y++) memcpy(crop + (size_t)y * COLS * T * 4, fr + ((size_t)(OY + y) * FW + OX) * 4, COLS * T * 4);
#elif PET_SECTION == 76      /* core line 808 */
    if (r >= 0 && r < 16 && sum != last[r]) { last[r] = sum; char d[PATH_MAX], p[PATH_MAX]; snprintf(d, sizeof d, "%s/pieces/system/maps/df-hq/%s", PCHQ, rooms[r].id); mkdir(d, 0755); snprintf(p, sizeof p, "%s/map.png", d); png_atomic(p, COLS * T, ROWS * T, crop, COLS * T * 4); }
#elif PET_SECTION == 77      /* core line 832 */
    snprintf(d, sizeof d, "%s/pieces/system/maps/df-hq/%s", PCHQ, rooms[r].id); mkdir(d, 0755);
    for (int i = 0; i < nparty; i++) if (others[i].room == r) { int pat = others[i].steps % 3; write_frame(party[i].sheet, party[i].idx, others[i].dir, pat); unsigned char px[4] = {128, 128, 128, 255}; frame_rgb(sheet("characters", party[i].sheet), party[i].idx, others[i].dir & 3, px);
        o += snprintf(buf + o, sizeof buf - o, "%d %d %d %d %d %s %d %d %d %c\n", others[i].x, others[i].y, px[0], px[1], px[2], party[i].sheet, party[i].idx, dc[others[i].dir & 3], pat, party[i].name[0] >= 'A' && party[i].name[0] <= 'Z' ? party[i].name[0] + 32 : (party[i].name[0] ? party[i].name[0] : 'm')); }
    if (tr.room == r) { int pat = tr.steps % 3; write_frame(hero.sheet, 0, tr.dir, pat); unsigned char px[4] = {200, 60, 60, 255}; frame_rgb(sheet("characters", hero.sheet), 0, tr.dir & 3, px);
        o += snprintf(buf + o, sizeof buf - o, "%d %d %d %d %d %s 0 %d %d @\n", tr.x, tr.y, px[0], px[1], px[2], hero.sheet, dc[tr.dir & 3], pat); }
#elif PET_SECTION == 78      /* core line 840 */
    static const char leg[] = "W|3|150|150|160|-|wall|#\nD|3|170|110|50|-|door|+\nT|2|50|150|60|-|tree|T\nf|1|190|150|90|-|furniture|&\n.|0|90|80|60|-|floor|.\n";
#elif PET_SECTION == 79      /* core line 842 */
    { char bp[PATH_MAX]; snprintf(bp, sizeof bp, "%s/pieces/system/maps/df-hq/view.pdl", PCHQ); char vw[PATH_MAX + 160]; char sc[PATH_MAX]; spath(sc, "scene.raw"); int vn = snprintf(vw, sizeof vw, "VIEW | cell_px | 48\nVIEW | default_style | scene\nVIEW | scene_raw | %s\nVIEW | scene_w | %d\nVIEW | scene_h | %d\n", sc, FW, FH); write_if_changed(bp, vw, (size_t)vn); }      /* the engine draws this book's 2D at df-hq's own 48 px tile (house desk_grid.pdl says 80) */
#elif PET_SECTION == 80      /* core line 868 */
static int viewer_kinds(void) {      /* N3 idle-cost gate: the /proc scan (every cmdline of every process, ~170 opens a second on a busy desktop, measured as most of this daemon's idle kernel time) now runs only when the launchers' process list changed size (every launcher appends to #.desktop/livedesk_proc_list.txt), when a window found by the last scan is gone, or every viewer_full_ms (30 s) as a safety net; in between the answer costs one kill(pid, 0) per known window. */
    static long last = 0, last_full = 0, sig = -1; static int v = 3, np = 0; static pid_t pids[8]; long t = now_ms();
    if (last && t - last < tune("viewer_scan_ms", 2000)) return v;
    last = t;
    char pl[PATH_MAX]; snprintf(pl, sizeof pl, "%s/#.desktop/livedesk_proc_list.txt", HOUSE); long s = fsize(pl); int gone = 0; for (int i = 0; i < np; i++) if (kill(pids[i], 0) != 0 && errno == ESRCH) gone = 1;
    if (last_full && s == sig && !gone && t - last_full < tune("viewer_full_ms", 30000)) return v;
    last_full = t; sig = s; v = 0; np = 0;
#elif PET_SECTION == 81      /* core line 879 */
        if (!strstr(b, "khtpm_core_render")) continue; for (size_t i = 0; i < n; i++) if (!b[i]) b[i] = ' '; int hit = 0; if (strstr(b, "/@.apps/df-hq/board/pchq-board.xhtpm")) { v |= 1; hit = 1; } if (strstr(b, "df-hq.xhtpm")) { v |= 2; hit = 1; } if (hit && np < 8) pids[np++] = (pid_t)atoi(e->d_name); }
#elif PET_SECTION == 82      /* core line 896 */
#elif PET_SECTION == 83      /* core line 897 */
    char wp[PATH_MAX], xp[PATH_MAX], hp[PATH_MAX], m[64] = "", dk[64] = ""; snprintf(wp, sizeof wp, "%s/pieces/world_01/state.txt", PCHQ);
#elif PET_SECTION == 84      /* core line 902 */
#elif PET_SECTION == 85      /* core line 903 */
    snprintf(xp, sizeof xp, "%s/pieces/xelector_01/state.txt", PCHQ); snprintf(hp, sizeof hp, "%s/pieces/hero_01/state.txt", PCHQ);
#elif PET_SECTION == 86      /* core line 908 */
    if (strcmp(dk, rooms[e->room].id) && lw != e->room) { lw = e->room; char ib[PATH_MAX], c[96]; snprintf(ib, sizeof ib, "%s/pieces/system/widget_cmds/inbox.txt", PCHQ); snprintf(c, sizeof c, "CONFIRM_SET_DESK:%s", rooms[e->room].id); FILE *f = fopen(ib, "a"); if (f) { fprintf(f, "%s\n", c); fclose(f); } changed = 1; }
#elif PET_SECTION == 87      /* core line 921 */
    char root[PATH_MAX], d[PATH_MAX], p[PATH_MAX]; snprintf(root, sizeof root, "%s/pieces/system/maps/df-hq", PCHQ);
    { char a[PATH_MAX]; snprintf(a, sizeof a, "%s/pieces/system/maps", PCHQ); mkdir(a, 0755); } mkdir(root, 0755);
#elif PET_SECTION == 88      /* core line 927 */
        g_gr = r; char glyph[MAXR][MAXC + 1]; int fs[MAXR][MAXC], ws[MAXR][MAXC]; unsigned char a[XT * XT * 4], b[XT * XT * 4], fl[XT * XT * 4]; nslot = 1; memset(slotpx[0], 0, XT * XT * 4);
        Room *rm = &rooms[r]; Sheet *a5i = sheet("tilesets", "Inside_A5"), *a5o = sheet("tilesets", "Outside_A5"), *dr = sheet("characters", "!Door1");
#elif PET_SECTION == 89      /* core line 930 */
        if (rm->outdoor) { tile24(a5o, rm->fc * S48, rm->fr * S48, floorpx); tile24(a5o, 2 * S48, 0, wallpx); memcpy(ceilpx, wallpx, sizeof ceilpx); tile24(a5o, 1 * S48, 2 * S48, pathpx); }
        else { tile24(a5i, rm->fc * S48, rm->fr * S48, floorpx); tile24(a5i, 1 * S48, 3 * S48, wallpx); tile24(a5i, 0, 0, ceilpx); memcpy(pathpx, floorpx, sizeof pathpx); }
        tile24(dr, 0, 192, doorpx); tile24(a5i, 0, 6 * S48, matpx);
#elif PET_SECTION == 90      /* core line 933 */
        if (!rm->over) for (int x = 0; x < COLS; x++) { glyph[0][x] = glyph[1][x] = 'W'; ws[0][x] = slot_of(ceilpx); ws[1][x] = slot_of(wallpx); }
#elif PET_SECTION == 91      /* core line 935 */
        for (int i = 0; i < ndoor; i++) if (doors[i].room == r) { int tx = doors[i].tx; memcpy(a, wallpx, sizeof a); over24(a, doorpx); glyph[doors[i].ty][tx] = 'D'; ws[doors[i].ty][tx] = slot_of(a);
            if (rm->over) { } else if (rm->outdoor) { for (int y = TRIG_ROW; y < ROWS - 1; y++) fs[y][tx] = slot_of(pathpx); } else fs[doors[i].ty + 1][tx] = slot_of(matpx); }
#elif PET_SECTION == 92      /* core line 938 */
            Sheet *s = sheet("tilesets", scen[i].sheet); memcpy(b, floorpx, sizeof b); tile24(s, (scen[i].c + ci) * S48, (scen[i].r + cj) * S48, a); over24(b, a); glyph[y][x] = scen[i].h >= 2 ? 'T' : 'f'; ws[y][x] = slot_of(b); }
        for (int i = 0; i < npl; i++) if (pl[i].room == r) { Item *it = &cat[pl[i].item]; Sheet *s = sheet("tilesets", it->sheet);
#elif PET_SECTION == 93      /* core line 941 */
                tile24(s, (it->c + ci) * S48, (it->r + cj) * S48, a);
#elif PET_SECTION == 94      /* core line 943 */
        if (rm->over) {      /* the overmap 3D: every cell = the painted 2D cell downscaled to a 24 px tile; the buildings (scenery) are 2 high blocks, the ocean is a flat floor */
            int sv = vroom; vroom = r; g_bg_only = 1; render(); g_bg_only = 0; vroom = sv; g_gr = r;
            for (int y = 0; y < rm->rows; y++) for (int x = 0; x < rm->cols; x++) { unsigned char cellpx[XT * XT * 4];
                for (int yy = 0; yy < XT; yy++) for (int xx = 0; xx < XT; xx++) { int sx = OX + x * T + xx * T / XT, sy = OY + y * T + yy * T / XT; memcpy(cellpx + (yy * XT + xx) * 4, fr + ((size_t)sy * FW + sx) * 4, 4); }
                int blocked = 0; for (int i = 0; i < nscen; i++) if (scen[i].room == r && x >= scen[i].x && x < scen[i].x + scen[i].w && y >= scen[i].y && y < scen[i].y + scen[i].h) blocked = 1;
                glyph[y][x] = blocked ? 'T' : '.'; fs[y][x] = slot_of(cellpx); ws[y][x] = 0; } }
#elif PET_SECTION == 95      /* core line 951 */
        snprintf(p, sizeof p, "%s/extrusion.pdl", d); { FILE *f = fopen_tmp(p); if (f) { fprintf(f, "SECTION      | KEY                | VALUE\n----------------------------------------------------------------------\nMETA         | map_id             | df-hq\nMETA         | desk_id            | %s\nMETA         | note               | %s (df-hq house, exported from the live house)\nEXTRUDE      | W                  | 3\nEXTRUDE      | D                  | 3\nEXTRUDE      | T                  | 2\nEXTRUDE      | f                  | 1\nEXTRUDE      | default            | 0\n", rooms[r].id, rooms[r].name); fclose_tmp(f, p); } }
#elif PET_SECTION == 96      /* core line 964 */
#elif PET_SECTION == 97      /* core line 968 */
    if (!strcmp(v, "helppanel")) {      /* Help tab: open / next / prev / close the help panel (help.txt, pages split by ---) */
        const char *w = argc >= 2 ? argv[1] : "open"; char hb[16]; flag_str("help_page", "0", hb, sizeof hb); int pg = atoi(hb), np = help_pages(-1, NULL);
        if (!strcmp(w, "close")) { set_flag("help", "0"); snprintf(msg, cap, "help closed"); return; }
        if (!strcmp(w, "next")) pg = (pg + 1) % np; else if (!strcmp(w, "prev")) pg = (pg + np - 1) % np; else if (!flag_on("help", 0)) pg = 0;
        snprintf(hb, sizeof hb, "%d", pg); set_flag("help_page", hb); set_flag("help", "1"); snprintf(msg, cap, "help page %d of %d", pg + 1, np); return;
    }
    if (!strcmp(v, "decpct")) { snprintf(msg, cap, "a pet without a target picks a new one with %d%% chance each step (base 45, moved by the walk weight in state/bank)", dec_pick_pct()); return; }
    if (!strcmp(v, "review")) {      /* M3: the daily review of state/decisions.txt -> state/review.txt (review [day-number], default the latest day) */
        char dp[PATH_MAX], rp[PATH_MAX]; spath(dp, "decisions.txt"); spath(rp, "review.txt"); int n = dec_review(dp, rp, argc > 1 ? atoi(argv[1]) : -1); dec_bank();
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
#elif PET_SECTION == 98      /* core line 976 */
    if (!strcmp(v, "newgame")) {      /* a fresh game: the current one is saved to slot 0 first (Load slot 0 brings it back), then every game file is removed so the next read starts from the defaults. The clock is not touched. */
#elif PET_SECTION == 99      /* core line 979 */
        static const char *fixed[] = { "house.txt", "trainer.txt", "relations.txt", "chat.txt", "active.txt", "daylight.txt", "flag_ctl.txt", "flag_follow.txt", "jobs.txt", "events.txt", "note.txt", "help_step.txt" };
#elif PET_SECTION == 100      /* core line 982 */
        append_line("world_epoch.txt", "NEW"); world_sync(); pet_load(); replay();
        snprintf(msg, cap, "new game started; the game you left is in slot 0 (Player > Load slot 0)"); return; }
    if (!strcmp(v, "reset")) { char p[PATH_MAX], q[PATH_MAX]; spath(p, "house.txt"); snprintf(q, sizeof q, "%s/house.txt.%ld.bak", ST, now); if (fexists(p)) rename(p, q);
        { char rb[60]; snprintf(rb, sizeof rb, "RESET|%ld", now); append_line("events.txt", rb); append_line("jobs.txt", rb); world_sync(); }      /* the dug cells and the marks go too: the rock is back (rows before RESET stay as history) */
        snprintf(msg, cap, "house reset, the rock is back (the old ledger is kept as house.txt.%ld.bak)", now); return; }
#elif PET_SECTION == 101      /* core line 1002 */
        char p[PATH_MAX], bb[40]; spath(p, "active.txt"); snprintf(bb, sizeof bb, "%s\n", party[k].id); if (write_if_changed(p, bb, strlen(bb)) && !g_daemon) ent_mark(); set_flag("follow", "pet"); set_flag("ctl", "pet"); active = k; calc_view(); snprintf(msg, cap, "watching %s (in the %s)", party[k].name, rooms[others[k].room].name); return; }
#elif PET_SECTION == 102      /* core line 1024 */
        if (flag_on("int", 0)) { int ci = ctl_idx(); use_ent(ci); pet.room = r; g_gr = r; pet.dir = 0; pet.tx = pet.ty = -1; int placed = 0;
#elif PET_SECTION == 103      /* core line 1030 */
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
#elif PET_SECTION == 104      /* core line 1039 */
        if (argc >= 2 && !strcmp(argv[1], "fortress")) {      /* the shipped test fortress (saves/test-fortress, tracked, read-only): the running game goes to slot 0 first, same refusal as New game; no clock file in it, so the clock is untouched */
            if (saving) { snprintf(msg, cap, "the test fortress is read-only"); return; } char m2[200] = ""; char *sv[] = { (char *)"save", (char *)"0", NULL }; verb(2, sv, m2, sizeof m2);
            char b0[PATH_MAX]; snprintf(b0, sizeof b0, "%s/slots/0/events.txt", ST); if (!fexists(b0)) { snprintf(msg, cap, "test fortress NOT loaded: the backup to slot 0 failed (%s)", m2); return; }
            snprintf(sd, sizeof sd, "%s/saves/test-fortress", APP); }
        char names[40][48]; int nn = 0; snprintf(names[nn++], 48, "house.txt"); snprintf(names[nn++], 48, "trainer.txt"); snprintf(names[nn++], 48, "relations.txt"); snprintf(names[nn++], 48, "chat.txt"); snprintf(names[nn++], 48, "active.txt"); snprintf(names[nn++], 48, "daylight.txt"); snprintf(names[nn++], 48, "flag_ctl.txt"); snprintf(names[nn++], 48, "flag_follow.txt"); snprintf(names[nn++], 48, "jobs.txt"); snprintf(names[nn++], 48, "events.txt");      /* the dug cells and the marks (events.txt is bigger than the 64 KB read buffer below: streamed) */
#elif PET_SECTION == 105      /* core line 1045 */
            if (!strcmp(names[i], "events.txt") || !strcmp(names[i], "jobs.txt")) { if (copy_file(a, bpath)) ok++; continue; }
#elif PET_SECTION == 106      /* core line 1047 */
        if (!saving) { append_line("world_epoch.txt", "LOAD"); world_sync(); }
        if (!saving) { char b[40]; read_file(mp, b, sizeof b); long long ms = atoll(b); if (ms > 0) { clock_paths(); clock_install(); char ma[32]; snprintf(ma, sizeof ma, "%lld", ms); lc_run(0, "cmd", "rpg", "settime", ma, "--source", "rpg-load", NULL); lc_run(0, "step", "5", NULL); } pet_load(); }
        if (argc >= 2 && !strcmp(argv[1], "fortress")) { snprintf(msg, cap, "test fortress loaded (%d files); your game is in slot 0 (Undo new game)", ok); return; } snprintf(msg, cap, "%s slot %d (%d files)", saving ? "saved to" : "loaded from", slot, ok); return; }
#elif PET_SECTION == 107      /* core line 1050 */
    if (!strcmp(v, "toggle") && argc >= 2 && (!strcmp(argv[1], "chat") || !strcmp(argv[1], "hb") || !strcmp(argv[1], "voice") || !strcmp(argv[1], "left"))) { set_flag(argv[1], flag_on(argv[1], !strcmp(argv[1], "voice") || !strcmp(argv[1], "left")) ? "0" : "1"); snprintf(msg, cap, "%s toggled", argv[1]); return; }
#elif PET_SECTION == 108      /* core line 1131 */
    char wp[PATH_MAX], wb[2048], m[64] = "", dk[64] = "", xp[PATH_MAX], kb[512]; snprintf(wp, sizeof wp, "%s/pieces/world_01/state.txt", PCHQ); read_file(wp, wb, sizeof wb);
#elif PET_SECTION == 109      /* core line 1134 */
    snprintf(xp, sizeof xp, "%s/pieces/xelector_01/state.txt", PCHQ); read_file(xp, kb, sizeof kb); int xx = -1, xy = -1;
#elif PET_SECTION == 110      /* core line 1144 */
    char wp[PATH_MAX], wb[2048], m[64] = "", dk[64] = "", xp[PATH_MAX], kb[512]; snprintf(wp, sizeof wp, "%s/pieces/world_01/state.txt", PCHQ); read_file(wp, wb, sizeof wb);
#elif PET_SECTION == 111      /* core line 1146 */
    if (strcmp(m, "df-hq")) return 0; snprintf(xp, sizeof xp, "%s/pieces/xelector_01/state.txt", PCHQ); read_file(xp, kb, sizeof kb); int xx = -1, xy = -1;
#elif PET_SECTION == 112      /* core line 1177 */
    signal(SIGTERM, bye); signal(SIGINT, bye); signal(SIGHUP, bye); srand((unsigned)(time(NULL) ^ getpid())); g_daemon = 1;
#elif PET_SECTION == 113      /* core line 1185 */
        {   /* N3 idle-cost gate (grok back-to-sonnet-oc16): pet_load re-read every pet file and the trainer on every 100 ms pass (measured 720 read calls/s, 106 KB/s idle). Now only when ent_changed.txt or world_epoch.txt GREW (a verb process changed an entity, a load or new game happened) or every pet_reload_ms (2 s) as a safety net for files written by hand; otherwise just the cheap reset of the mover and view. */
            static long sig = -1, last_pl = 0; char mk[PATH_MAX], we[PATH_MAX]; spath(mk, "ent_changed.txt"); spath(we, "world_epoch.txt"); if (fsize(mk) > 262144) { FILE *tf = fopen(mk, "w"); if (tf) fclose(tf); sig = -1; }      /* only this daemon reads the marker: keep it small */
            long ps = fsize(mk) * 1000003L + fsize(we), tn = now_ms();
            if (ps != sig || tn - last_pl >= tune("pet_reload_ms", 2000)) { sig = ps; last_pl = tn; pet_load(); } else { me = active; pet = others[active]; calc_view(); } }
        int in = flag_on("int", 0), tm = turn_mode(); static int prev_in = 0; static long int_on_at = 0; if (in && !prev_in) int_on_at = now_ms(); prev_in = in;
#elif PET_SECTION == 114      /* core line 1190 */
        if (in && n) { char kl[300], one[16]; kl[0] = 0; for (int i = 0; i < n && i < 20; i++) { snprintf(one, sizeof one, "%d ", codes[i]); strncat(kl, one, sizeof kl - strlen(kl) - 1); } char kp[PATH_MAX]; spath(kp, "keys_seen.txt"); write_if_changed(kp, kl, strlen(kl)); }      /* last batch of codes, for debugging */
#elif PET_SECTION == 115      /* core line 1224 */
            if (board_window_open() && t - cchk >= tune("click_poll_ms", 150)) { cchk = t; char cp[PATH_MAX] = "", ub2[1024]; { char up2[PATH_MAX]; snprintf(up2, sizeof up2, "%s/@.apps/df-hq/board/state/ui.txt", HOUSE); if (read_file(up2, ub2, sizeof ub2) > 0) for (char *l2 = strtok(ub2, "\n"); l2; l2 = strtok(NULL, "\n")) if (!strncmp(l2, "bv_session=", 11)) snprintf(cp, sizeof cp, "%s/pieces/display/canvas_click.txt", l2 + 11); }      /* THIS board's own click file (the window writes it inside its engine session): another board's clicks never select here */
                if (!cp[0]) snprintf(cp, sizeof cp, "%s/pieces/display/canvas_click.txt", ST); struct stat cs;
#elif PET_SECTION == 116      /* core line 1229 */
                    if (cw > 0 && ch > 0) { g_gr = vroom; int col = (cx * FW / cw - OX) / T, row = (cy * FH / ch - OY) / T; char *sa[2]; char sid[24]; sa[0] = "select"; sa[1] = sid; int hit = 0;
#elif PET_SECTION == 117      /* core line 1238 */
        { char dp[PATH_MAX], db[40]; spath(dp, "daylight.txt"); read_file(dp, db, sizeof db); if (strcmp(db, lastday)) { snprintf(lastday, sizeof lastday, "%s", db); dirty = 1; } }
#elif PET_SECTION == 118      /* core line 1259 */
    signal(SIGPIPE, SIG_IGN);      /* a closed pipe / dead child must never kill a verb or the daemon (the children get SIG_DFL back before exec) */
    signal(SIGPIPE, SIG_IGN);      /* a closed pipe / dead child must never kill a verb or the daemon (the children get SIG_DFL back before exec) */
    snprintf(ST, sizeof ST, "%s/state", APP); mkdir(ST, 0755);
    snprintf(PCHQ, sizeof PCHQ, "%s/df-hq-pchq", APP);
#elif PET_SECTION == 119      /* core line 1268 */
    load_data(); replay(); pet_load(); clock_paths();
#endif
#endif
