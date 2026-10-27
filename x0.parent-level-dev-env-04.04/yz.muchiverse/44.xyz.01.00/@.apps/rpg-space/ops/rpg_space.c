/* rpg_space.c - the planet board (rpg-space) on the shared pets-like core (&.widgits/_shared-lib/petslike_core.c). Two roles in one file: the top builds the program, and the core
 * re-includes this file once per SECTION to pull in what is specific to this game. Design: @.apps/../#.#.calendar-dox/!.HQ-IQ-BOOK/16.game/PETSLIKE-FRAMEWORK-PLAN.md (P3) */
#ifndef PET_SECTION
#define PET_APP "rpg-space"
#define PET_OP "rpg_space"
#define PET_SKIN "rpg_space.c"
/* what this game needs beyond the core defaults (the core takes them as #ifndef): a few explorers, planet pages 16 x 16, a 2D frame that holds a 16 x 16 page at 24 px, tiles as the default 2D view, its own room export */
#define NP 4
#define MAXC 16
#define MAXR 16
#define FW 400
#define FH 416
#define PET_VIEW_CELL "24"
#define PET_VIEW_STYLE "tiles"
#define PET_OWN_EXPORT 1
#define PET_SHEET_PFX "!space"      /* the generated pet character sheets are !space0.png ... (shared pet code: _shared-lib/petslike_skin_lib.c) */
#define PET_ART_FALLBACK "../rpg-edit-hq/art"      /* the pals pictures are the island game's, shared */
static const char *sk_life_bar_state(int slot);
#define PET_BAR_HP(slot) sk_life_bar_state(slot)
#define PET_ROOM_PHASE(r, ph) do { if ((r) < 4 || (r) == 6) snprintf(ph, sizeof ph, "night"); } while (0)      /* rooms 0..3 are the ship: deep space is always night */
#include "../../../&.widgits/_shared-lib/petslike_core.c"
#else
#if PET_SECTION == 1
 * The planet board of rpg-space (BOOK 16.game/RPG-DUST-PETSLIKE-BOARD-DESIGN.md): a tenth skin of the shared core. Its pages come from the world layer (ops/dust_gen.c, dust_zoom.c) and are painted in section 73;
 * everything else (pets, chat, clock and day / night, hotbar, board) is the core. Pictures are RPG Maker MV tiles (sheets under #.NNEST_ASSETS/rmmv-www-img) for now.
#elif PET_SECTION == 2
#include "../../../&.widgits/_shared-lib/kh_state_notify.h"      /* STATE_BACKEND=file|marker|shmem: "did it change" without reading it (branch marker, 2026-10-12) */
#include "../../../&.widgits/_shared-lib/csv_frames.c"      /* a pet's sprite.csv -> the 16x24 3D walk frames */
#elif PET_SECTION == 3
/* ---- rpg-space: room 0 = the region page, room 1 = the area page (section 73 builds them into these arrays once per process). glyph: G grass, p sand, T forest tree, ~ water, r rock / snow.
 * fs / ws: the floor and the wall/object slot of the 24 px tile atlas, cdep: hung blocks under the cell (always 0 here). ---- */
#define SK_NR 16
#define SK_NB0 7      /* rooms 7..15: the nine region pages around the ship's planet page, index (dy+1)*3+(dx+1); 11 is the middle one and unused (room SK_SHIP is the middle page). Only storage for the painted pages, they are no desks (owner 2026-10-24: 'widen the map so the ship can move further') */
static int sk_nb_ok[9]; static int sk_land_live(void);
static int sk_wroom(int wx, int wy, int *lx, int *ly) {      /* the world cell (wx, wy) in page units (0..15 = the planet page, -16..-1 and 16..31 = its neighbours) -> the room that holds its painted cell, or -1 = open space */
    int px = wx < 0 ? -1 - (-wx - 1) / 16 : wx / 16, py = wy < 0 ? -1 - (-wy - 1) / 16 : wy / 16; if (px < -1 || px > 1 || py < -1 || py > 1) return -1;
    *lx = wx - px * 16; *ly = wy - py * 16; if (!px && !py) return 4; int i = (py + 1) * 3 + (px + 1); return sk_nb_ok[i] ? SK_NB0 + i : -1; }
#define SK_SPACE 6      /* the space desk: planets as islands, the ship as a small hull box that the helm moves (design 13b, owner 2026-10-23 / 2026-10-24) */
#define SOX 2      /* the ship's footprint inside its 16 x 16 desk: 12 x 12 cells at (2,2); the planet is the backdrop around it */
#define SOY 2
#define SSZ 12
#define SK_SHIP 4      /* rooms 0..3 are the ship (bridge, quarters, hold, engine); 4 = the region page, 5 = the area page of the current planet */
#define SK_W 16
#define SK_H 16
static char sk_glyph[SK_NR][SK_H][SK_W + 1];
static int sk_fs[SK_NR][SK_H][SK_W], sk_ws[SK_NR][SK_H][SK_W], sk_cdep[SK_NR][SK_H][SK_W];
static int sk_fs3[SK_NR][SK_H][SK_W], sk_ws3[SK_NR][SK_H][SK_W];      /* the same cells painted with the plain flat tiles, for the 3D export (owner 2026-10-23: autotiling in 2D only, "roll back only for 3d for now"); sk_fs / sk_ws are what the 2D scene paints */
static int sk_built, sk_rockslot; static long sk_pin_until;      /* until this time the daemon keeps pinning the xelector to the active pet (a book reload resets the engine's xelector to 1,1) */
typedef struct { char id[24], label[40], kind[8], root[160]; } SkTs;
typedef struct { char id[24], label[40], ts[24]; unsigned long long seed, def; } SkPl;
static SkPl sk_pl[16]; static int sk_pl_n, sk_pl_cur; static SkTs sk_ts[8]; static int sk_ts_n; static char sk_ts_cur[24] = "rmmv", sk_addr[200] = "globe:3,4/region:0,1"; static unsigned long long sk_seed; static long sk_look_sig = -1, sk_built_sig = -2;
static const char *sk_room_label(int room); static void sk_look_load(void); static void sk_desk_sync(void); static void sk_rebuild(char *msg, size_t cap); static void export3d(char *msg, size_t cap);
static void sk_wd_verb(int argc, char **argv, char *msg, size_t cap); static void sk_item_rgb(const char *item, int dmg, int *r, int *g, int *b);
static int sk_rem_idx(const char *item, int dmg) {      /* the frame index of a remnant: item kind * 2 + damaged; the frame file rem_<idx>_2_0.rgba (16x24, a small block at the foot of the cell) is written on first use */
    static const char *const kinds[] = { "stone", "iron", "gem", "scrap", "bread", "fish", "ore" }; int kd = 7; for (int i = 0; i < 7; i++) if (!strncmp(item, kinds[i], strlen(kinds[i]))) { kd = i; break; } if (kd == 6) kd = 1; int idx = kd * 2 + (dmg > 0 ? 1 : 0);
    char d[PATH_MAX], p[PATH_MAX]; snprintf(d, sizeof d, "%s/../tsots-characters/frames", ASSETS); mkdir(d, 0755);
    static const int dirs[4] = { 2, 6, 4, 8 }, wid[4] = { 8, 6, 3, 6 };      /* the four facings of a REVOLVING remnant (owner 2026-10-25: it turns to show it can be picked up): wide, three-quarter, edge-on, three-quarter */
    for (int k = 0; k < 4; k++) { snprintf(p, sizeof p, "%s/rem_%d_%d_0.rgba", d, idx, dirs[k]); if (access(p, F_OK) == 0) continue; int r, g, b; sk_item_rgb(item, dmg, &r, &g, &b); unsigned char out[16 * 24 * 4]; memset(out, 0, sizeof out); int x0 = 8 - wid[k] / 2;
        for (int y = 17; y < 24; y++) for (int x = x0; x < x0 + wid[k]; x++) { unsigned char *q = out + (y * 16 + x) * 4; int edge = (x == x0 || x == x0 + wid[k] - 1 || y == 23), top = (y == 17); q[0] = (unsigned char)(edge ? r * 60 / 100 : top ? (r + 255) / 2 : r); q[1] = (unsigned char)(edge ? g * 60 / 100 : top ? (g + 255) / 2 : g); q[2] = (unsigned char)(edge ? b * 60 / 100 : top ? (b + 255) / 2 : b); q[3] = 255; }
        FILE *f = fopen(p, "wb"); if (f) { fwrite(out, 1, sizeof out, f); fclose(f); } }
    return idx; }
static int sk_node_idx(const char *item) {      /* frame index of a resource node: 40 + item kind; a bigger block than a remnant (12 wide, 10 high) with lighter flecks */
    static const char *const kinds[] = { "stone", "iron", "gem", "scrap", "bread", "fish" }; int kd = 6; for (int i = 0; i < 6; i++) if (!strncmp(item, kinds[i], strlen(kinds[i]))) { kd = i; break; } int idx = 40 + kd;
    char d[PATH_MAX], p[PATH_MAX]; snprintf(d, sizeof d, "%s/../tsots-characters/frames", ASSETS); mkdir(d, 0755);
    static const int dirs[4] = { 2, 6, 4, 8 }; for (int k = 0; k < 4; k++) { snprintf(p, sizeof p, "%s/rem_%d_%d_0.rgba", d, idx, dirs[k]); if (access(p, F_OK) == 0) continue; int r, g, b; sk_item_rgb(item, 0, &r, &g, &b); unsigned char out[16 * 24 * 4]; memset(out, 0, sizeof out);
        for (int y = 13; y < 24; y++) for (int x = 2; x < 14; x++) { unsigned char *q = out + (y * 16 + x) * 4; int edge = (x == 2 || x == 13 || y == 23), fleck = ((x * 7 + y * 3) % 5 == 0), top = (y == 13); int rr = edge ? r * 55 / 100 : (top || fleck) ? (r + 255) / 2 : r, gg = edge ? g * 55 / 100 : (top || fleck) ? (g + 255) / 2 : g, bb = edge ? b * 55 / 100 : (top || fleck) ? (b + 255) / 2 : b; q[0] = (unsigned char)rr; q[1] = (unsigned char)gg; q[2] = (unsigned char)bb; q[3] = 255; }
        FILE *f = fopen(p, "wb"); if (f) { fwrite(out, 1, sizeof out, f); fclose(f); } }
    return idx; }
static char sk_rem_rows[16][1100];      /* per room: the remnant rows appended to that room's events.txt (section 43), "x y r g b rem 0 2 0 *" = a coloured box in the cell (the same path the pets are drawn by) */
static int sk_remnants_tick(void); static int sk_coll_check(int nx, int ny, int nz, char *msg, size_t cap); static void sk_coll_verb(int argc, char **argv, char *msg, size_t cap);
static void sk_helm_verb(int argc, char **argv, char *msg, size_t cap);      /* the helm (section 75) */
static void sk_life_verb(int argc, char **argv, char *msg, size_t cap); static int sk_env_tick(void); static void sk_env_verb(int argc, char **argv, char *msg, size_t cap); static void sk_env_breach(const char *room, int on); static void sk_env_ui(char *out, size_t cap); static int sk_life_tick(void); static void sk_life_ui(char *out, size_t cap);      /* life, death, respawn, drops (section 75) */
static long sk_helm_settle = 0;      /* until this time the xelector is not read as a steering move (a desk change or a move puts it somewhere else by itself) */
static int sk_pos_of(const char *id, int *gx, int *gy); static void sk_ship_load(int *gx, int *gy);
static int sk_choice(char *title, char *q, char *yes, char *no, char *direct);
static int sk_art[NP];      /* 1: pet i has art (csv frames exist: the 3D view shows its front picture) */
static void sk_world_draw(void); static void sk_veh_write(void); static void sk_veh_clear(void); static int sk_world_block(int nx, int ny, int nz, char *msg, size_t cap); static int sk_world_reach_idx(void);
static int sk_space_block(int gx, int gy, char *msg, size_t cap); static void sk_space_draw(int g); static void sk_space_verb(int argc, char **argv, char *msg, size_t cap);
static void sk_ensure_map(void); static int sk_blocked(int room, int x, int y); static int sk_node_solid(int room, int x, int y); static void sk_pets_sync(void); static void sk_paint_bg(int room, const char *phase); static void sk_spawn(int i, int *x, int *y);
#elif PET_SECTION == 4
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
#elif PET_SECTION == 5
    f = fopen(tmp, "wb"); if (!f) return 0; size_t wn = fwrite(data, 1, n, f); int ce = fclose(f); if (wn != n || ce != 0 || rename(tmp, p) != 0) { remove(tmp); return 0; } sn_note_write(p); return 1;
#elif PET_SECTION == 6
static struct { char k[48]; char v[64]; long t; long ep; } g_fc[32];
#elif PET_SECTION == 7
    char ck[48]; long cnow = 0; if (g_fcache_on) { snprintf(ck, sizeof ck, "%s|%s", name, def); cnow = now_ms(); for (int i = 0; i < g_fcn; i++) if ((sn_active(&g_sn) ? g_fc[i].ep == g_flag_epoch : cnow - g_fc[i].t < tune("flag_cache_ms", 150)) && !strcmp(g_fc[i].k, ck)) { snprintf(out, cap, "%s", g_fc[i].v); return; } }
#elif PET_SECTION == 8
    if (g_fcache_on) { int s = -1; for (int i = 0; i < g_fcn; i++) if (!strcmp(g_fc[i].k, ck)) s = i; if (s < 0 && g_fcn < 32) s = g_fcn++; if (s >= 0) { snprintf(g_fc[s].k, sizeof g_fc[s].k, "%s", ck); snprintf(g_fc[s].v, sizeof g_fc[s].v, "%s", out); g_fc[s].t = cnow; g_fc[s].ep = g_flag_epoch; } }
#elif PET_SECTION == 9
#elif PET_SECTION == 10
    for (int i = 0; i < nparty; i++) {      /* the explorers are not DB actors: default stats, and every pet wears sheet !dust<i/8> index i%8 (the skin writes those sheets, section 73) */
        if (!party[i].mhp) { party[i].mhp = 40; party[i].mmp = 20; party[i].atk = party[i].def = party[i].agi = party[i].luk = 10; party[i].mat = party[i].mdf = 8; }
        if (!party[i].nick[0]) snprintf(party[i].nick, 24, "explorer"); if (!party[i].profile[0]) snprintf(party[i].profile, 96, "An explorer of the planet.");
        snprintf(party[i].sheet, 24, "!space%d", i / 8); party[i].idx = i % 8; }
    sk_pets_sync();
#elif PET_SECTION == 11
#elif PET_SECTION == 12
    for (int i = 0; i < nroom; i++) rooms[i].outdoor = 1;      /* every desk has sky: the ship hovers above the planet (later: dark, by distance from the sun, design D16) */
    sk_ensure_map(); ndoor = 0;      /* the ship's doors: a D cell in a wall, standing on it goes through (ty is the row above the D cell, ax/ay the arrival cell inside the other room) */
    { static const int dd[6][7] = { {0, 5, 12, 1, 5, 4, 1}, {1, 5, 2, 0, 5, 12, 1}, {0, 10, 12, 2, 10, 4, 1}, {2, 10, 2, 0, 10, 12, 1}, {0, 2, 7, 3, 12, 8, 1}, {3, 13, 7, 0, 3, 8, 1} };      /* room, tx, ty, dest, ax, ay, autow */
      for (int k = 0; k < 6 && ndoor < 24; k++) { Door *d = &doors[ndoor++]; d->room = dd[k][0]; d->tx = dd[k][1]; d->ty = dd[k][2]; d->dest = dd[k][3]; d->ax = dd[k][4]; d->ay = dd[k][5]; d->autow = dd[k][6]; } }
#elif PET_SECTION == 13
#elif PET_SECTION == 14
    char p[PATH_MAX], line[300]; spath(p, "house.txt");
#elif PET_SECTION == 15
static void use_ent(int i) { me = i; pet = i < 0 ? tr : others[i]; if (pet.room >= 0 && pet.room < nroom) g_gr = pet.room; }
#elif PET_SECTION == 16
    write_if_changed(p, b, strlen(b)); if (me < 0) tr = pet; else others[me] = pet; calc_view();
#elif PET_SECTION == 17
static int floor_blocked(int room, int x, int y, int skip_id) {
    if (room < 0 || room >= nroom || x < 0 || y < 0 || x >= rooms[room].cols || y >= rooms[room].rows) return 9999;
    if (sk_blocked(room, x, y)) return 9999;
    for (int i = 0; i < npl; i++) { Placed *q = &pl[i]; if (q->room != room || q->id == skip_id || cat[q->item].wall) continue; if (x >= q->x && x < q->x + cat[q->item].w && y >= q->y && y < q->y + cat[q->item].h) return q->id; }
    return 0; }
static int door_here(int room, int x, int y) { for (int i = 0; i < ndoor; i++) if (doors[i].room == room && doors[i].tx == x && y == doors[i].ty + 1) return i; return -1; }
#elif PET_SECTION == 18
static void tile(const char *sh, int c, int r, int w, int h, int tx, int ty) { blit(sheet("tilesets", sh), c * S48, r * S48, w * S48, h * S48, OX + tx * T, OY + ty * T); }
#elif PET_SECTION == 19
    Room *rm = &rooms[vroom]; char phase[40] = "day"; { char dp[PATH_MAX], db[64]; spath(dp, "daylight.txt"); read_file(dp, db, sizeof db); if (strstr(db, "night")) snprintf(phase, sizeof phase, "night"); else if (strstr(db, "dusk")) snprintf(phase, sizeof phase, "dusk"); else if (strstr(db, "dawn")) snprintf(phase, sizeof phase, "dawn"); }
    sk_paint_bg(vroom, phase);      /* the island page or the interior: sky behind the void cells, then the floor and the wall / object slot of every cell (cached per room and phase) */
#elif PET_SECTION == 20
    for (int a = 0; a < n; a++) { Placed *p = &pl[idx[a]]; Item *d = &cat[p->item]; tile(d->sheet, d->c, d->r, d->w, d->h, p->x, p->y); }
#elif PET_SECTION == 21
    for (int k = 0; k < (g_bg_only ? 0 : no); k++) { int i = order[k]; Ent *e = i < 0 ? &tr : &others[i]; int col = e->steps ? cyc[e->steps % 4] : 1, ci = i < 0 ? 0 : party[i].idx;
        blit(sheet("characters", i < 0 ? hero.sheet : party[i].sheet), (ci % 4) * 3 * S48 + col * S48, (ci / 4) * 4 * S48 + e->dir * S48, S48, S48, OX + e->x * T, OY + e->y * T - 8 * T / S48); }
#elif PET_SECTION == 22
#elif PET_SECTION == 23
#elif PET_SECTION == 24
#elif PET_SECTION == 25
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
#elif PET_SECTION == 26
    char p[PATH_MAX]; spath(p, "scene.raw"); if (write_if_changed(p, fr, sizeof fr)) g_scene_ver++;
#elif PET_SECTION == 27
    int chat = flag_on("chat", 0), hb = flag_on("hb", 0), in = flag_on("int", 0); char view[16]; flag_str("view", "2d", view, sizeof view); for (char *c = view; *c; c++) if (*c >= 'a' && *c <= 'z') *c -= 32;
    W("title=rpg-space  -  %s  -  coins %d\nscene_raw=%s\ncanvas_raw=%s\ncoins=%d\nmsg=%s\nroom_label=%s\n", rooms[vroom].name, coins, raw, raw, coins, msg && msg[0] ? msg : (in ? "INT on: turn based like a roguelike. Time moves when you act. Tab switches pet/player." : "real time: the pets live on their own. INT lets you control a pet or the player."), sk_room_label(vroom));      /* the toolbar room label says where the doors are */
#elif PET_SECTION == 28
    W("left_visible=%s\n", flag_on("left", 1) ? "1" : "");      /* the pinned info bar (default on, Menu > Info show / hide, its _ button) */
    W("chat_visible=%s\nhb_visible=%s\nview_label=view %s\nchat_toggle_label=window %s\n", chat ? "1" : "", hb ? "1" : "", view, chat ? "on" : "off");
    { char ct[48] = "", cq[80] = "", cy[48] = "", cn[48] = "", cd[48] = ""; int ask = flag_on("ask", 0) && sk_choice(ct, cq, cy, cn, cd); W("ask_visible=%s\nask_title=%s\nask_text=%s\nask_yes=%s\nask_direct=%s\nask_direct_show=%s\nask_no=%s\n", ask ? "1" : "", ct, cq, cy, cd, cd[0] ? "1" : "", cn); }      /* the yes / no popup of an event (events/<name>/choice.pdl) */
    { char cp[PATH_MAX], cb[4000]; spath(cp, "chat.txt"); read_tail(cp, cb, sizeof cb); char *ln[6] = {0}; int k = 0; for (char *l = strtok(cb, "\n"); l; l = strtok(NULL, "\n")) { ln[k % 6] = l; k++; } for (int i = 0; i < 6; i++) { int j = k - 6 + i; W("chat_%d=%s\n", i, j >= 0 ? ln[j % 6] : ""); } }
    { char lb[1200]; sk_life_ui(lb, sizeof lb); W("%s", lb); }
    { char eb[300]; sk_env_ui(eb, sizeof eb); W("%s", eb); }
    sk_look_load(); { const char *cl = sk_pl_cur < 0 ? "No planet (deep space)" : sk_pl_n ? sk_pl[sk_pl_cur].label : "-";      /* the left bar: the address, the seed, Regen, zoom and the planets (design D8, D12) */
      W("seed=%llu\naddr_line=%s\npl_label=%s\nn_pl=%d\n", sk_seed, sk_addr, cl, sk_pl_n);
      for (int k = 0; k < sk_pl_n; k++) W("pl_%d_label=%s\npl_%d_id=%s\npl_%d_cls=%s\n", k, sk_pl[k].label, k, sk_pl[k].id, k, k == sk_pl_cur ? "active nav-selected" : ""); }
#elif PET_SECTION == 29
#elif PET_SECTION == 30
#elif PET_SECTION == 31
    pet.room = d->dest; g_gr = pet.room; pet.x = d->ax; pet.y = d->ay; pet.dir = 0; pet.tx = pet.ty = -1; { int fx, fy; if (free_near(pet.x, pet.y, &fx, &fy)) { pet.x = fx; pet.y = fy; } }      /* land on the nearest FREE cell, not on whoever is already there */
    if (me >= 0 && me == active && (pet.room == vroom || di_from_room == vroom)) { snprintf(b, sizeof b, "went through the door to the %s", rooms[pet.room].name); say(party[me].name, b); }
#elif PET_SECTION == 32
#elif PET_SECTION == 33
#elif PET_SECTION == 34
#elif PET_SECTION == 35
#elif PET_SECTION == 36
#elif PET_SECTION == 37
    if (pet.tx < 0 && rand() % 100 < 45) pick_target();
#elif PET_SECTION == 38
#elif PET_SECTION == 39
static void end_turn(void) { if (!run_on() || !turn_mode()) return; clock_turn(); int in = flag_on("int", 0), c = ctl_idx(); for (int i = 0; i < nparty; i++) if (!(in && i == c)) ai_step(i); use_ent(active); }      /* the pet you control does not act on its own */
#elif PET_SECTION == 40
#elif PET_SECTION == 41
    if (!strncmp(sh, "!space", 5)) return;      /* the island pets' 3D frames are written with their sheets (csv_write_frames, section 73) */
#elif PET_SECTION == 42
#elif PET_SECTION == 43
    snprintf(d, sizeof d, "%s/pieces/system/maps/" PET_APP "/%s", PCHQ, rooms[r].id); mkdir(d, 0755);
    for (int i = 0; i < nparty; i++) if (others[i].room == r) { int pat = others[i].steps % 3, fd = sk_art[i] ? 0 : (others[i].dir & 3); write_frame(party[i].sheet, party[i].idx, fd, pat); unsigned char px[4] = {128, 128, 128, 255}; frame_rgb(sheet("characters", party[i].sheet), party[i].idx, fd, px);      /* an art pet shows one front picture (a side facing plane is seen edge-on by the 3D camera) */
        o += snprintf(buf + o, sizeof buf - o, "%d %d %d %d %d %s %d %d %d %c\n", others[i].x, others[i].y, px[0], px[1], px[2], party[i].sheet, party[i].idx, dc[fd], pat, party[i].name[0] >= 'A' && party[i].name[0] <= 'Z' ? party[i].name[0] + 32 : (party[i].name[0] ? party[i].name[0] : 'm')); }
    if (tr.room == r) { int pat = tr.steps % 3; write_frame(hero.sheet, 0, tr.dir, pat); unsigned char px[4] = {200, 60, 60, 255}; frame_rgb(sheet("characters", hero.sheet), 0, tr.dir & 3, px);
        o += snprintf(buf + o, sizeof buf - o, "%d %d %d %d %d %s 0 %d %d @\n", tr.x, tr.y, px[0], px[1], px[2], hero.sheet, dc[tr.dir & 3], pat); }
    if (r >= 0 && r < 16 && sk_rem_rows[r][0] && o + strlen(sk_rem_rows[r]) < sizeof buf) { memcpy(buf + o, sk_rem_rows[r], strlen(sk_rem_rows[r])); o += (int)strlen(sk_rem_rows[r]); }      /* remnants lying in this room (after the pets and the hero, so their ids do not move) */
#elif PET_SECTION == 44
    static const char leg[] = "W|3|150|150|160|-|wall|#\nD|3|170|110|50|-|door|+\nT|2|50|150|60|-|tree|T\nf|1|190|150|90|-|furniture|&\n.|0|90|80|60|-|floor|.\n";
#elif PET_SECTION == 45
static int viewer_kinds(void) {      /* idle-cost gate (df-hq's N3 version): the /proc scan (every cmdline of every process) runs only when a launcher appended to #.desktop/livedesk_proc_list.txt, when a window found by the last scan is gone, or every viewer_full_ms (30 s) as a safety net; in between the answer costs one kill(pid, 0) per known window. */
    static long last = 0, last_full = 0, sig = -1; static int v = 3, np = 0; static pid_t pids[8]; long t = now_ms();
    if (last && t - last < tune("viewer_scan_ms", 2000)) return v;
    last = t;
    char pl[PATH_MAX]; snprintf(pl, sizeof pl, "%s/#.desktop/livedesk_proc_list.txt", HOUSE); long s = fsize(pl); int gone = 0; for (int i = 0; i < np; i++) if (kill(pids[i], 0) != 0 && errno == ESRCH) gone = 1;
    if (last_full && s == sig && !gone && t - last_full < tune("viewer_full_ms", 30000)) return v;
    last_full = t; sig = s; v = 0; np = 0;
#elif PET_SECTION == 46
        if (!strstr(b, "khtpm_core_render")) continue; for (size_t i = 0; i < n; i++) if (!b[i]) b[i] = ' '; int hit = 0; if (strstr(b, "/@.apps/rpg-space/board/pchq-board.xhtpm")) { v |= 1; hit = 1; } if (strstr(b, "rpg-space.xhtpm")) { v |= 2; hit = 1; } if (hit && np < 8) pids[np++] = (pid_t)atoi(e->d_name); }
#elif PET_SECTION == 47
#elif PET_SECTION == 48
#elif PET_SECTION == 49
#elif PET_SECTION == 50
#elif PET_SECTION == 51
#elif PET_SECTION == 52
#elif PET_SECTION == 53
#elif PET_SECTION == 54
#elif PET_SECTION == 55
static int copy_file(const char *from, const char *to) { FILE *in = fopen(from, "rb"); if (!in) return 0; char tmpc[PATH_MAX + 24]; snprintf(tmpc, sizeof tmpc, "%s.tmp.%d", to, (int)getpid()); FILE *out = fopen(tmpc, "wb"); if (!out) { fclose(in); return 0; } char buf[8192]; size_t n; int ok = 1; while ((n = fread(buf, 1, sizeof buf, in)) > 0) if (fwrite(buf, 1, n, out) != n) { ok = 0; break; } fclose(in); if (fclose(out) != 0) ok = 0; if (ok && rename(tmpc, to) != 0) ok = 0; if (!ok) remove(tmpc); return ok; }
#elif PET_SECTION == 56
#elif PET_SECTION == 57
    if (!strcmp(v, "newgame")) {      /* a fresh game: the current one is saved to slot 0 first (Undo new game = Load slot 0), then every game file is removed so the next read starts from the defaults. The clock is not touched. */
#elif PET_SECTION == 58
        static const char *fixed[] = { "house.txt", "trainer.txt", "relations.txt", "chat.txt", "active.txt", "daylight.txt", "flag_ctl.txt", "flag_follow.txt", "events.txt" };
#elif PET_SECTION == 59
        pet_load(); replay();
        snprintf(msg, cap, "new game started; the game you left is in slot 0 (Player > Undo new game)"); return; }
    if (!strcmp(v, "reset")) { char p[PATH_MAX], q[PATH_MAX]; spath(p, "house.txt"); snprintf(q, sizeof q, "%s/house.txt.%ld.bak", ST, now); if (fexists(p)) rename(p, q); snprintf(msg, cap, "house reset (the old ledger is kept as house.txt.%ld.bak)", now); return; }
#elif PET_SECTION == 60
        char p[PATH_MAX], bb[40]; spath(p, "active.txt"); snprintf(bb, sizeof bb, "%s\n", party[k].id); write_if_changed(p, bb, strlen(bb)); set_flag("follow", "pet"); set_flag("ctl", "pet"); active = k; calc_view(); snprintf(msg, cap, "watching %s (in the %s)", party[k].name, rooms[others[k].room].name); return; }
#elif PET_SECTION == 61
#elif PET_SECTION == 62
        char names[40][48]; int nn = 0; snprintf(names[nn++], 48, "house.txt"); snprintf(names[nn++], 48, "trainer.txt"); snprintf(names[nn++], 48, "relations.txt"); snprintf(names[nn++], 48, "chat.txt"); snprintf(names[nn++], 48, "active.txt"); snprintf(names[nn++], 48, "daylight.txt"); snprintf(names[nn++], 48, "flag_ctl.txt"); snprintf(names[nn++], 48, "flag_follow.txt"); snprintf(names[nn++], 48, "events.txt");      /* events.txt is bigger than the 64 KB read buffer below: streamed */
#elif PET_SECTION == 63
            if (!strcmp(names[i], "events.txt")) { if (copy_file(a, bpath)) ok++; continue; }
#elif PET_SECTION == 64
        if (!saving) { char b[40]; read_file(mp, b, sizeof b); long long ms = atoll(b); if (ms > 0) { clock_paths(); clock_install(); char ma[32]; snprintf(ma, sizeof ma, "%lld", ms); lc_run(0, "cmd", "rpg", "settime", ma, "--source", "rpg-load", NULL); lc_run(0, "step", "5", NULL); } sn_bump(&g_sn, SL_PETS); sn_bump(&g_sn, SL_FLAGS); sn_bump(&g_sn, SL_DAY); g_flag_epoch++; pet_load(); }
        snprintf(msg, cap, "%s slot %d (%d files)", saving ? "saved to" : "loaded from", slot, ok); return; }
#elif PET_SECTION == 65
    if (!strcmp(v, "regen") || !strcmp(v, "planet") || !strcmp(v, "newplanet") || !strcmp(v, "zoomhere") || !strcmp(v, "zoomout")) {      /* the look and the place are LEDGER rows (state/look_ledger.txt, zoom_ledger.txt): a verb appends, every process replays */
        char lp[PATH_MAX], row[160]; snprintf(lp, sizeof lp, "%s/state/look_ledger.txt", APP); sk_look_load();
        if (strcmp(v, "zoomhere") && strcmp(v, "zoomout")) { char dp[PATH_MAX]; spath(dp, "direct.txt"); write_if_changed(dp, "0 0\n", 4); }      /* a new planet or look: the hull is centred over it again */
        char from_pl[24]; snprintf(from_pl, sizeof from_pl, "%s", sk_pl_cur < 0 ? "none" : sk_pl[sk_pl_cur].id);      /* where the ship is before the move */
        if (!strcmp(v, "newplanet")) { static const char *TS[4] = { "rmmv", "cdda", "mcl", "doom" }; int n = sk_pl_n + 1; struct timespec ts_; clock_gettime(CLOCK_REALTIME, &ts_); unsigned long long z = ((unsigned long long)ts_.tv_sec * 1000000007ULL + (unsigned long long)ts_.tv_nsec) ^ ((unsigned long long)n * 0x9E3779B97F4A7C15ULL); z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL; z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL; z ^= z >> 31; unsigned long long sd = 20270000ULL + z % 90000000ULL;      /* a new planet gets a fresh seed from the clock (nanoseconds + its number, mixed): two quick clicks make two different worlds; the row stores it, so replay is exact */ if (sk_pl_n >= 16) { snprintf(msg, cap, "the system is full (16 planets)"); return; } snprintf(row, sizeof row, "NEWPLANET | p%d | Planet %d | %s | %llu\n", n, n, TS[n % 4], sd); }
        else if (!strcmp(v, "planet")) { int ok = argc >= 2 && !strcmp(argv[1], "none"); for (int k = 0; argc >= 2 && k < sk_pl_n; k++) if (!strcmp(sk_pl[k].id, argv[1])) ok = 1; if (!ok) { snprintf(msg, cap, "unknown planet (see planets.pdl)"); return; } snprintf(row, sizeof row, "PLANET | %s\n", argv[1]); }
        else if (!strcmp(v, "regen")) { if (sk_pl_cur < 0) { snprintf(msg, cap, "no planet to regenerate"); return; } unsigned long long ns = argc >= 2 ? strtoull(argv[1], NULL, 10) : sk_seed + 1; snprintf(row, sizeof row, "REGEN | %llu\n", ns); }
        else { char cmd[PATH_MAX * 2]; if (!strcmp(v, "zoomout")) snprintf(cmd, sizeof cmd, "DUST_STATE='%s/state' '%s/ops/+x/dust_zoom.+x' zoom_out >/dev/null 2>&1", APP, APP);
            else snprintf(cmd, sizeof cmd, "DUST_STATE='%s/state' '%s/ops/+x/dust_zoom.+x' zoom_in %d %d >/dev/null 2>&1", APP, APP, tr.x < 0 ? 0 : tr.x > 15 ? 15 : tr.x, tr.y < 0 ? 0 : tr.y > 15 ? 15 : tr.y);
            int rc = system(cmd); sk_rebuild(msg, cap); if (rc) snprintf(msg, cap, "zoom refused (already at that level?)"); return; }
        FILE *lf = fopen(lp, "a"); if (!lf) { snprintf(msg, cap, "cannot write the look ledger"); return; } fputs(row, lf); fclose(lf);
        sk_rebuild(msg, cap);
        if (!strcmp(v, "planet") || !strcmp(v, "newplanet")) {      /* the ship goes ABOVE the planet: the backdrop is the new planet, the crew (the hero and every pet) gathers in the middle of the bridge and the window follows */
            static const int dk[8][2] = { {7, 8}, {8, 8}, {9, 8}, {7, 9}, {8, 9}, {9, 9}, {8, 7}, {9, 7} }; int slot = 0;
            if (!(argc >= 3 && !strcmp(argv[2], "fly"))) for (int ci = -1; ci < nparty; ci++) { use_ent(ci); pet.room = 0; pet.x = dk[slot % 8][0]; pet.y = dk[slot % 8][1]; slot++; pet.dir = 0; pet.tx = pet.ty = -1; pet_save(); }
            if (!(argc >= 3 && !strcmp(argv[2], "fly"))) { use_ent(active); g_gr = 0; set_flag("follow", "pet"); sk_desk_sync(); }      /* "planet <id> fly" (the helm landing) leaves the crew and the view where the helm put them */
            { char b[96]; snprintf(b, sizeof b, "DOOR|%ld|ship|%s|%s", (long)time(NULL), from_pl, sk_pl_cur < 0 ? "none" : sk_pl[sk_pl_cur].id);      /* the same row a door writes (who | from | to): the ship went from one planet to the next */ append_line("events.txt", b); }
            if (sk_pl_cur < 0) snprintf(msg, cap, "the ship floats in deep space"); else snprintf(msg, cap, "the ship is above %s", sk_pl[sk_pl_cur].label); }
        return; }
    if (!strcmp(v, "toggle") && argc >= 2 && (!strcmp(argv[1], "chat") || !strcmp(argv[1], "hb") || !strcmp(argv[1], "voice") || !strcmp(argv[1], "left"))) { set_flag(argv[1], flag_on(argv[1], !strcmp(argv[1], "voice") || !strcmp(argv[1], "left")) ? "0" : "1"); snprintf(msg, cap, "%s toggled", argv[1]); return; }
    if (!strcmp(v, "helm")) { sk_helm_verb(argc, argv, msg, cap); return; }
    if (!strcmp(v, "life")) { sk_life_verb(argc, argv, msg, cap); return; }
    if (!strcmp(v, "mine") || !strcmp(v, "destroy") || !strcmp(v, "hold") || !strcmp(v, "build")) { sk_wd_verb(argc, argv, msg, cap); return; }
    if (!strcmp(v, "playpage") && argc >= 2) { int ok = run_event_x(argv[1], argc >= 3 ? argv[2] : "parallel", ""); snprintf(msg, cap, "playpage %s: %s", argv[1], ok ? "page played" : "no page / failed"); return; }      /* what events-hq's Play does: the page by name (a harness plays the air and collision pages with it) */
    if (!strcmp(v, "collision")) { sk_coll_verb(argc, argv, msg, cap); return; }
    if (!strcmp(v, "env")) { sk_env_verb(argc, argv, msg, cap); return; }
    if (!strcmp(v, "space")) { sk_space_verb(argc, argv, msg, cap); return; }
    if (!strcmp(v, "setpos")) {      /* setpos <room id> <x> <y>: put the controlled one on a cell (test setup for board_ctl; refuses a blocked cell and the space desk) */
        int rr = argc >= 2 ? room_idx(argv[1]) : -1, px = argc >= 4 ? atoi(argv[2]) : -1, py = argc >= 4 ? atoi(argv[3]) : -1;
        if (argc >= 4) { rr = room_idx(argv[1]); px = atoi(argv[2]); py = atoi(argv[3]); } else if (argc == 3) { snprintf(msg, cap, "usage: setpos <room> <x> <y>"); return; }
        if (argc < 4 || rr < 0 || rr == SK_SPACE || sk_blocked(rr, px, py)) { snprintf(msg, cap, "setpos: unknown room, the space desk, or a blocked cell"); return; }
        { int ci = ctl_idx(); use_ent(ci); pet.room = rr; pet.x = px; pet.y = py; pet.dir = 0; pet.tx = pet.ty = -1; pet_save(); use_ent(active); set_flag("follow", ci < 0 ? "player" : "pet"); g_pin_now = 1;
          { char xz[PATH_MAX], hz[PATH_MAX]; snprintf(xz, sizeof xz, "%s/pieces/xelector_01/state.txt", PCHQ); snprintf(hz, sizeof hz, "%s/pieces/hero_01/state.txt", PCHQ); kv_set_int(xz, "pos_x", px); kv_set_int(xz, "pos_y", py); kv_set_int(hz, "pos_x", px); kv_set_int(hz, "pos_y", py); } }      /* the xelector goes with the character, or the daemon walks the character back to where the xelector was */
        snprintf(msg, cap, "set on %s %d,%d", argv[1], px, py); return; }
#elif PET_SECTION == 66
    signal(SIGTERM, bye); signal(SIGINT, bye); signal(SIGHUP, bye); srand((unsigned)(time(NULL) ^ getpid()));
#elif PET_SECTION == 67
    if (now_ms() < sk_pin_until) { g_pin_now = 1; char xz[PATH_MAX], hz[PATH_MAX]; snprintf(xz, sizeof xz, "%s/pieces/xelector_01/state.txt", PCHQ); snprintf(hz, sizeof hz, "%s/pieces/hero_01/state.txt", PCHQ); kv_set_int(xz, "pos_z", 17); kv_set_int(hz, "pos_z", 17); }      /* the engine starts the camera one above the LOWEST surface, which is now the planet sunk under the ship: hold it at the ship's floor level (floor 16, camera 17) */      /* after a planet / Regen reload the xelector follows the active pet again (follow_pchq re-writes its cell while the pin is set) */
        { static long pl_safety = 0; long pn = now_ms(); if (!sn_active(&g_sn) || sn_changed(&g_sn, SL_PETS) || pn - pl_safety > 30000) { pl_safety = pn; pet_load(); } }      /* marker/shmem: reload the pets only when a writer moved the slot (or every 30 s for a hand edit) */
        int in = flag_on("int", 0), tm = turn_mode(); static int prev_in = 0; static long int_on_at = 0; if (in && !prev_in) int_on_at = now_ms(); prev_in = in;
        if (!in && flag_on("ask", 0)) set_flag("ask", "0");      /* a popup left open when In: goes off is closed */
        { static int hw = 0; int hh = flag_on("helm", 0); if (hh && !hw) sk_helm_settle = now_ms() + 1800; hw = hh; }      /* the helm page runs in its own process: the daemon notices the helm turning on and waits for the desk change to settle */
        if (!in && flag_on("helm", 0)) { char hm[160], *hv[2] = { "helm", "off" }; sk_helm_verb(2, hv, hm, sizeof hm); }      /* In: off = AI control again: the helm lets go (at the helm the xelector is read and put back by sk_helm_follow, section 78) */
#elif PET_SECTION == 68
        if (in && n) { char kl[300], one[16]; kl[0] = 0; for (int i = 0; i < n && i < 20; i++) { snprintf(one, sizeof one, "%d ", codes[i]); strncat(kl, one, sizeof kl - strlen(kl) - 1); } char kp[PATH_MAX]; spath(kp, "keys_seen.txt"); write_if_changed(kp, kl, strlen(kl)); }      /* last batch of codes, for debugging */
#elif PET_SECTION == 69
        { char dp[PATH_MAX], db[40]; static long dl_safety = 0; long dn = now_ms(); if (!sn_active(&g_sn) || sn_changed(&g_sn, SL_DAY) || dn - dl_safety > 30000) { dl_safety = dn; spath(dp, "daylight.txt"); read_file(dp, db, sizeof db); } else snprintf(db, sizeof db, "%s", lastday); if (strcmp(db, lastday)) { snprintf(lastday, sizeof lastday, "%s", db); dirty = 1; } }
#elif PET_SECTION == 70
    signal(SIGPIPE, SIG_IGN);      /* a closed pipe / dead child must never kill a verb or the daemon */
    signal(SIGPIPE, SIG_IGN);      /* a closed pipe / dead child must never kill a verb or the daemon */
    snprintf(ST, sizeof ST, "%s/state", APP); mkdir(ST, 0755); sn_init(&g_sn, ST);
    snprintf(PCHQ, sizeof PCHQ, "%s/rpg-space-pchq", APP);
#elif PET_SECTION == 71
    load_data(); replay(); pet_load(); clock_paths();
#elif PET_SECTION == 72
    if (!strncmp(name, "!space", 5)) snprintf(p, sizeof p, "%s/../tsots-characters/%s.png", ASSETS, name);      /* the generated pet sheets live next to the 2D renderer's own charsets */
#elif PET_SECTION == 73
/* ===== rpg-space: the planet pages. Room 0 = the region page, room 1 = the area page of the ledger's address (dust_zoom where); both are made by dust_gen page (the world layer, harness-gated) and painted here.
 * A band of the height (dust_gen BANDS) becomes a cell: a, b deep water / water ~ (blocked), c sand p, d grass G, e forest T (blocked), f rock r, g snow r (blocked). RPG Maker tiles first (the tileset choice comes later, design D5). ===== */
static int sk_read_page(const char *path, int h[SK_H][SK_W]) {
    FILE *f = fopen(path, "r"); if (!f) return 0; char ln[256]; if (!fgets(ln, sizeof ln, f) || strncmp(ln, "PAGE |", 6)) { fclose(f); return 0; }
    for (int y = 0; y < SK_H; y++) { if (!fgets(ln, sizeof ln, f)) { fclose(f); return 0; } char *p = ln; for (int x = 0; x < SK_W; x++) { char *e; long v = strtol(p, &e, 10); if (e == p) { fclose(f); return 0; } h[y][x] = (int)v; p = e; } }
    fclose(f); return 1;
}
static void sk_current_address(char *addr, size_t cap) {      /* the ledger's current address; an empty ledger starts at the default region */
    char cmd[PATH_MAX * 2]; addr[0] = 0;
    for (int pass = 0; pass < 2 && !addr[0]; pass++) {
        snprintf(cmd, sizeof cmd, "DUST_STATE='%s/state' '%s/ops/+x/dust_zoom.+x' %s 2>/dev/null", APP, APP, pass ? "start globe:3,4/region:0,1" : "where");
        FILE *p = popen(cmd, "r"); if (!p) continue; if (fgets(addr, (int)cap, p)) addr[strcspn(addr, "\r\n")] = 0; else addr[0] = 0; pclose(p);
    }
    if (!addr[0]) snprintf(addr, cap, "globe:3,4/region:0,1");
}
/* the shared pieces both own-map skins used to copy: sk_solid, sk_drop_sheet and the pet roster (see petslike_skin_lib.c) */
#include "../../../&.widgits/_shared-lib/petslike_skin_lib.c"
static void sk_stamp(int d, Sheet *s, int c, int r, int x, int y, char g) {      /* one 48 px tile of a sheet over the floor of the cell, as the object slot */
    unsigned char a[XT * XT * 4], b[XT * XT * 4]; tile24(s, c * S48, r * S48, a); memset(b, 0, sizeof b);
    if (sk_fs[d][y][x]) memcpy(b, slotpx[sk_fs[d][y][x]], sizeof b);
    over24(b, a); sk_glyph[d][y][x] = g; sk_ws[d][y][x] = slot_of(b);
}
static int sk_blocked(int room, int x, int y) {
    sk_ensure_map();
    if (room < 0 || room >= SK_NR || x < 0 || y < 0 || x >= SK_W || y >= SK_H) return 1;
    if (room == SK_SPACE) return 1;      /* nobody walks in space: the one who is there is the ship, put on its cell by the helm */
    if (room < SK_SHIP && (x < SOX || y < SOY || x >= SOX + SSZ || y >= SOY + SSZ)) return 1;      /* beyond the hull is the planet far below: nobody walks there */
    if (sk_node_solid(room, x, y)) return 1;      /* a resource node or a block you built stands in the way until it is mined away (owner 2026-10-25: building walls) */
    char g = sk_glyph[room][y][x]; return g == 'T' || g == '~' || g == 'r' || g == 'W' || g == 'f';
}
static void sk_near_free(int room, int *x, int *y) {      /* the nearest walkable cell to (x, y), searched in growing squares */
    for (int rad = 0; rad < SK_W; rad++) for (int dy = -rad; dy <= rad; dy++) for (int dx = -rad; dx <= rad; dx++) {
        if (abs(dx) != rad && abs(dy) != rad) continue; if (!sk_blocked(room, *x + dx, *y + dy)) { *x += dx; *y += dy; return; } }
}
/* ---- the look: tilesets.pdl (what a tileset is) and state/look_ledger.txt (which one is on, which seed: append-only rows TILESET | id, REGEN | seed; the last of each wins). The address is replayed from zoom_ledger.txt by dust_zoom. ---- */
static const char *SK_BAND[7] = { "deep", "water", "sand", "grass", "forest", "rock", "snow" };
static const unsigned char SK_FLAT[7][3] = { {20, 50, 130}, {50, 100, 190}, {215, 195, 130}, {90, 160, 70}, {40, 110, 50}, {130, 125, 120}, {240, 240, 245} };
static void sk_look_load(void) {
    static long last = 0; long t = now_ms(); if (sk_look_sig != -1 && t - last < 400) return; last = t;
    char lp[PATH_MAX], zp[PATH_MAX], tp[PATH_MAX]; snprintf(lp, sizeof lp, "%s/state/look_ledger.txt", APP); snprintf(zp, sizeof zp, "%s/state/zoom_ledger.txt", APP); snprintf(tp, sizeof tp, "%s/tilesets.pdl", APP);
    long sig = fsize(lp) * 1000003L + fsize(zp) * 7919L + fsize(tp); if (sig == sk_look_sig) return; sk_look_sig = sig;
    sk_ts_n = 0; FILE *f = fopen(tp, "r"); char ln[512], *fl[6];
    while (f && fgets(ln, sizeof ln, f)) { int n = split_bar(ln, fl, 6); if (n < 5 || strcmp(fl[0], "TILESET") || sk_ts_n >= 8) continue; SkTs *q = &sk_ts[sk_ts_n++]; snprintf(q->id, sizeof q->id, "%s", fl[1]); snprintf(q->label, sizeof q->label, "%s", fl[2]); snprintf(q->kind, sizeof q->kind, "%s", fl[3]); snprintf(q->root, sizeof q->root, "%s", fl[4]); }
    if (f) fclose(f);
    sk_pl_n = 0; snprintf(tp, sizeof tp, "%s/planets.pdl", APP); f = fopen(tp, "r");
    while (f && fgets(ln, sizeof ln, f)) { int n = split_bar(ln, fl, 6); if (n < 5 || strcmp(fl[0], "PLANET") || sk_pl_n >= 16) continue; SkPl *q = &sk_pl[sk_pl_n++]; snprintf(q->id, sizeof q->id, "%s", fl[1]); snprintf(q->label, sizeof q->label, "%s", fl[2]); snprintf(q->ts, sizeof q->ts, "%s", fl[3]); q->def = q->seed = strtoull(fl[4], NULL, 10); }
    if (f) fclose(f);
    sk_pl_cur = 0; snprintf(tp, sizeof tp, "%s/state/look_ledger.txt", APP); f = fopen(tp, "r");      /* replay: PLANET | id picks the planet, REGEN | seed sets the seed of the CURRENT planet (each planet keeps its own) */
    while (f && fgets(ln, sizeof ln, f)) { int n = split_bar(ln, fl, 6); if (n < 2) continue; if (!strcmp(fl[0], "PLANET")) { if (!strcmp(fl[1], "none")) sk_pl_cur = -1; else for (int k = 0; k < sk_pl_n; k++) if (!strcmp(sk_pl[k].id, fl[1])) sk_pl_cur = k; }
        else if (!strcmp(fl[0], "NEWPLANET") && n >= 5 && sk_pl_n < 16) { SkPl *q = &sk_pl[sk_pl_n]; snprintf(q->id, sizeof q->id, "%s", fl[1]); snprintf(q->label, sizeof q->label, "%s", fl[2]); snprintf(q->ts, sizeof q->ts, "%s", fl[3]); q->def = q->seed = strtoull(fl[4], NULL, 10); sk_pl_cur = sk_pl_n++; }      /* a planet made in space: its row carries everything, so replay makes the same one */
        else if (!strcmp(fl[0], "REGEN") && sk_pl_cur >= 0) sk_pl[sk_pl_cur].seed = strtoull(fl[1], NULL, 10); }
    if (f) fclose(f);
    if (sk_pl_n) { int pc = sk_pl_cur < 0 ? 0 : sk_pl_cur; sk_seed = sk_pl[pc].seed; snprintf(sk_ts_cur, sizeof sk_ts_cur, "%s", sk_pl[pc].ts); }      /* no planet: the region / area pages keep planet 0 (the Region menu still works), the ship desks show only space */ else { sk_seed = 0; snprintf(sk_ts_cur, sizeof sk_ts_cur, "rmmv"); }
    sk_current_address(sk_addr, sizeof sk_addr);
}
/* a RUN copy of scale.pdl whose universe SEED is the look ledger's: dust_gen reads this one, the checked-in file is never edited by a click */
static void sk_scale_run(char *out, size_t cap) {
    char sp[PATH_MAX], ln[1024]; snprintf(sp, sizeof sp, "%s/scale.pdl", APP); snprintf(out, cap, "%s/state/scale_run.pdl", APP);
    FILE *in = fopen(sp, "r"), *o = fopen(out, "w"); if (!in || !o) { if (in) fclose(in); if (o) fclose(o); snprintf(out, cap, "%s", sp); return; }
    while (fgets(ln, sizeof ln, in)) { if (!strncmp(ln, "SEED", 4) && strstr(ln, "universe")) fprintf(o, "SEED | universe | %llu\n", sk_seed); else fputs(ln, o); }
    fclose(in); fclose(o);
}
/* one source picture -> a 24 px tile: the top-left square (an animated strip is taller than wide), box-averaged with alpha weights, then tinted */
static void sk_box(const unsigned char *src, int stride, int sq, unsigned tint, int tinted, unsigned char *out) {
    for (int y = 0; y < XT; y++) for (int x = 0; x < XT; x++) {
        int x0 = x * sq / XT, x1 = (x + 1) * sq / XT, y0 = y * sq / XT, y1 = (y + 1) * sq / XT; if (x1 <= x0) x1 = x0 + 1; if (y1 <= y0) y1 = y0 + 1;
        long acc[3] = {0, 0, 0}, wa = 0, cnt = 0; for (int yy = y0; yy < y1; yy++) for (int xx = x0; xx < x1; xx++) { const unsigned char *q = src + ((size_t)yy * stride + xx) * 4; acc[0] += q[0] * q[3]; acc[1] += q[1] * q[3]; acc[2] += q[2] * q[3]; wa += q[3]; cnt++; }
        unsigned char *o = out + (y * XT + x) * 4; if (wa) { for (int c = 0; c < 3; c++) o[c] = (unsigned char)(acc[c] / wa); o[3] = (unsigned char)(wa / cnt); } else o[0] = o[1] = o[2] = o[3] = 0;
        if (tinted && o[3]) { o[0] = (unsigned char)(o[0] * ((tint >> 16) & 255) / 255); o[1] = (unsigned char)(o[1] * ((tint >> 8) & 255) / 255); o[2] = (unsigned char)(o[2] * (tint & 255) / 255); } }
}
static int sk_tile_row(const SkTs *ts, int band, char *src, size_t cap, unsigned *tint, int *tinted) {      /* the TILE row of (tileset, band), 0 when there is none */
    char tp[PATH_MAX], ln[512], *fl[6]; snprintf(tp, sizeof tp, "%s/tilesets.pdl", APP); FILE *f = fopen(tp, "r"); int found = 0; *tinted = 0;
    while (f && fgets(ln, sizeof ln, f)) { int n = split_bar(ln, fl, 6); if (n < 4 || strcmp(fl[0], "TILE") || strcmp(fl[1], ts->id) || strcmp(fl[2], SK_BAND[band])) continue; snprintf(src, cap, "%s", fl[3]); if (n >= 5 && fl[4][0]) { *tint = (unsigned)strtoul(fl[4], NULL, 16); *tinted = 1; } found = 1; }
    if (f) fclose(f); return found;
}
/* a palette sprite.csv of ANY square resolution (the header says # resolution=N; doom flats are 48, cdda sprites 32 or 64): the shared csvf_load64 only takes 64 */
static unsigned char *sk_csv_any(const char *path, int *res) {
    FILE *f = fopen(path, "r"); if (!f) return NULL; char l[128]; int r = 0, n = 0, hdr = 1; unsigned char *px = NULL;
    while (fgets(l, sizeof l, f)) {
        if (l[0] == '#') { if (!strncmp(l, "# resolution=", 13)) r = atoi(l + 13); continue; }
        if (hdr) { hdr = 0; if (l[0] == 'r') continue; }
        if (r < 1 || r > 256) break; if (!px) px = (unsigned char *)calloc((size_t)r * r, 4); if (!px) break;
        int rr, g, b, a; if (sscanf(l, "%d,%d,%d,%d", &rr, &g, &b, &a) != 4 || n >= r * r) break; px[n * 4] = (unsigned char)rr; px[n * 4 + 1] = (unsigned char)g; px[n * 4 + 2] = (unsigned char)b; px[n * 4 + 3] = (unsigned char)a; n++; }
    fclose(f); if (!px || n != r * r) { free(px); return NULL; } *res = r; return px;
}
static int sk_tile_load(const SkTs *ts, int band, unsigned char *out) {      /* 1 = the picture of this band in this tileset, 0 = no row or no file (the caller uses the flat colour) */
    char src[200], path[PATH_MAX * 2]; unsigned tint = 0; int tinted = 0; if (!sk_tile_row(ts, band, src, sizeof src, &tint, &tinted)) return 0;
    if (!strcmp(ts->kind, "png")) { snprintf(path, sizeof path, "%s/../%s/%s", ASSETS, ts->root, src); int w, h, n; unsigned char *px = stbi_load(path, &w, &h, &n, 4); if (!px) return 0; sk_box(px, w, w < h ? w : h, tint, tinted, out); stbi_image_free(px); return 1; }
    if (!strcmp(ts->kind, "csv")) { snprintf(path, sizeof path, "%s/&.widgits/palettes/sprites/%s/%s/sprite.csv", HOUSE, ts->root, src); int res = 0; unsigned char *big = sk_csv_any(path, &res); if (!big) return 0; sk_box(big, res, res, tint, tinted, out); free(big); return 1; }
    return 0;
}
static int sk_band_of(int v) { return v < 20 ? 0 : v < 35 ? 1 : v < 45 ? 2 : v < 65 ? 3 : v < 80 ? 4 : v < 92 ? 5 : 6; }
/* RPG Maker autotiling for the planet's water, sand, rock and snow (the shared rule, petslike_autotile.c; owner 2026-10-23): a cell's neighbours of the same kind pick its edge shape, laid over the grass tile. Water joins water (World_A1 sea); sand joins sand and water (World_A2 kind 16); rock joins rock and snow (kind 18, grey); snow joins snow (kind 24). Outside the page counts as the same kind (MV treats the border as connected). */
static int sk_nb(int h[SK_H][SK_W], int x, int y, int lo, int hi) { if (x < 0 || y < 0 || x >= SK_W || y >= SK_H) return 1; int b = sk_band_of(h[y][x]); return b >= lo && b <= hi; }
static void sk_auto_cell(int g, int x, int y, int band, int h[SK_H][SK_W], int grass, Sheet *a1, Sheet *a2) {
    int lo = band <= 1 ? 0 : band == 2 ? 0 : band == 5 ? 5 : 6, hi = band <= 1 ? 1 : band == 2 ? 2 : 6, nb[8]; static const int D[8][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}, {1, -1}, {1, 1}, {-1, 1}, {-1, -1}};
    for (int k = 0; k < 8; k++) nb[k] = sk_nb(h, x + D[k][0], y + D[k][1], lo, hi);
    if (band <= 1) sk_auto_ground(g, x, y, '~', a1, 0, 0, nb, grass, 0); else if (band == 2) sk_auto_ground(g, x, y, 'p', a2, 0, 6, nb, grass, 0);
    else if (band == 5) sk_auto_ground(g, x, y, 'r', a2, 4, 6, nb, grass, 1); else sk_auto_ground(g, x, y, 'r', a2, 0, 9, nb, grass, 1);
}
static void sk_cell(int g, int x, int y, int band, int grass, unsigned char tl[7][XT * XT * 4], const int have[7]) {
    static const char GL[7] = { '~', '~', 'p', 'G', 'T', 'r', 'r' };
    if (band <= 3) { if (have[band]) { sk_glyph[g][y][x] = GL[band]; sk_fs[g][y][x] = slot_of(tl[band]); } else sk_solid(g, x, y, GL[band], SK_FLAT[band][0], SK_FLAT[band][1], SK_FLAT[band][2], 0, 0); if (band == 3) sk_glyph[g][y][x] = 'G'; return; }
    unsigned char b[XT * XT * 4]; if (have[band]) { memcpy(b, slotpx[grass], sizeof b); over24(b, tl[band]); sk_glyph[g][y][x] = GL[band]; sk_ws[g][y][x] = slot_of(b); }
    else sk_solid(g, x, y, GL[band], SK_FLAT[band][0], SK_FLAT[band][1], SK_FLAT[band][2], 0, 1);
}
/* ---- the ship: three interior rooms of sci-fi RPG Maker tiles (SF_Inside_A4 floor and wall, !SF_Door1), a ring of wall, doors in the wall cells (the pets walk onto a D cell to pass: sections 12 and 31). Tile cells are (col, row) of the 48 px sheet. ---- */
static void sk_ship_room(int g, Sheet *a4, Sheet *door, int fc, int fr, int wc, int wr, const int *dx, const int *dy, int nd) {      /* dx, dy: door cells local to the footprint */
    unsigned char ft[XT * XT * 4], wt[XT * XT * 4], u[XT * XT * 4], v[XT * XT * 4];
    if (a4) { tile24(a4, fc * S48, fr * S48, ft); tile24(a4, wc * S48, wr * S48, wt); } else { for (int i = 0; i < XT * XT; i++) { ft[i * 4] = 90; ft[i * 4 + 1] = 100; ft[i * 4 + 2] = 110; ft[i * 4 + 3] = 255; wt[i * 4] = 40; wt[i * 4 + 1] = 45; wt[i * 4 + 2] = 55; wt[i * 4 + 3] = 255; } }
    int fl = slot_of(ft), wl = slot_of(wt);
    for (int ly = 0; ly < SSZ; ly++) for (int lx = 0; lx < SSZ; lx++) { int x = SOX + lx, y = SOY + ly, wall = ly < 2 || ly == SSZ - 1 || lx == 0 || lx == SSZ - 1; sk_glyph[g][y][x] = wall ? 'W' : '.'; sk_fs[g][y][x] = fl; sk_ws[g][y][x] = wall ? wl : 0; sk_cdep[g][y][x] = 1; }
    for (int k = 0; k < nd; k++) { int x = SOX + dx[k], y = SOY + dy[k]; sk_glyph[g][y][x] = 'D'; if (door) { tile24(door, 0, 192, u); memcpy(v, slotpx[fl], sizeof v); over24(v, u); sk_ws[g][y][x] = slot_of(v); } else sk_ws[g][y][x] = wl; }
}
/* ---- what makes the ship rooms different: objects of SF_Inside_B (16 x 16 sheet of 48 px cells) stamped over the floor, glyph f (blocked, one block high). Cells are local to the ship footprint (floor x 1..10, y 2..10); the doors, their arrival cells and the crew's gathering cells (local 5..7, 6..7) stay free. room, sheet col, row, w, h in cells, local x, y ---- */
static const int SK_DECO[][7] = {
    {0, 8, 0, 1, 1, 2, 2}, {0, 8, 0, 1, 1, 4, 2}, {0, 8, 0, 1, 1, 6, 2}, {0, 8, 0, 1, 1, 8, 2},      /* bridge: monitors along the north wall ... */
    {0, 8, 3, 1, 1, 2, 3}, {0, 8, 3, 1, 1, 4, 3}, {0, 8, 3, 1, 1, 6, 3}, {0, 8, 3, 1, 1, 8, 3},      /* ... and a blue chair in front of each */
    {1, 8, 12, 2, 2, 7, 3}, {1, 12, 12, 2, 2, 9, 3}, {1, 9, 7, 1, 1, 1, 3}, {1, 10, 7, 1, 1, 1, 4},      /* quarters: two beds (purple, blue) and cabinets */
    {2, 4, 9, 1, 2, 2, 3}, {2, 4, 9, 1, 2, 3, 3}, {2, 1, 9, 1, 1, 5, 3}, {2, 10, 10, 1, 1, 2, 8}, {2, 10, 10, 1, 1, 3, 8}, {2, 1, 9, 1, 1, 9, 8},      /* hold: crates, barrels, cardboard boxes */
    {3, 4, 6, 2, 2, 2, 3}, {3, 4, 6, 2, 2, 5, 3}, {3, 1, 9, 1, 1, 8, 3}, {3, 1, 9, 1, 1, 8, 5}, {3, 3, 6, 1, 1, 3, 8}, {3, 3, 8, 1, 1, 6, 8},      /* engine: machinery grates, power cells, hazard signs */
};
static void sk_ship_deco(void) {
    Sheet *b = sheet("tilesets", "SF_Inside_B"); if (!b) return;
    for (size_t k = 0; k < sizeof SK_DECO / sizeof SK_DECO[0]; k++) { const int *d = SK_DECO[k];
        for (int j = 0; j < d[4]; j++) for (int i = 0; i < d[3]; i++) {
            int x = SOX + d[5] + i, y = SOY + d[6] + j; if (sk_glyph[d[0]][y][x] != '.') continue;      /* never over a wall, a door or another object */
            unsigned char a[XT * XT * 4], f[XT * XT * 4]; tile24(b, (d[1] + i) * S48, (d[2] + j) * S48, a); memcpy(f, slotpx[sk_fs[d[0]][y][x]], sizeof f); over24(f, a);
            sk_glyph[d[0]][y][x] = 'f'; sk_ws[d[0]][y][x] = slot_of(f); } }
}
/* the room's name with its exits, for the toolbar: a ship room is shown as "Engine (door E)" so nobody has to hunt for the way out; compass letters are the walls of the 3D / 2D picture (N = top) */
static const char *sk_room_label(int room) {
    static char b[64]; static const char *ex[4] = { " (doors S, S, W)", " (door N)", " (door N)", " (door E)" };
    snprintf(b, sizeof b, "%s%s", rooms[room].name, room >= 0 && room < SK_SHIP ? ex[room] : ""); return b;
}
static int sk_dox, sk_doy; static unsigned sk_ship_ver = 0;      /* DIRECT flight: the planet page under the hull is shifted by (sk_dox, sk_doy) cells, -direct_max..direct_max; state/direct.txt "ox oy"; the ship rooms and the camera never move, the planet slides away under them */
static void sk_direct_load(void) { char p[PATH_MAX], b[40]; spath(p, "direct.txt"); read_file(p, b, sizeof b); if (sscanf(b, "%d %d", &sk_dox, &sk_doy) != 2) sk_dox = sk_doy = 0; int mx = tune("direct_max", 16); if (mx > 16) mx = 16; if (sk_dox < -mx) sk_dox = -mx; if (sk_dox > mx) sk_dox = mx; if (sk_doy < -mx) sk_doy = -mx; if (sk_doy > mx) sk_doy = mx; }      /* tuning.pdl direct_max: how far the planet may slide away under the ship (default 10 of 16 cells; the planet leaves the screen into open space, the hull and the camera stay) */
static void sk_build_ship(void) {      /* called after the planet pages are painted: room SK_SHIP (the region page) is copied into every ship desk as the backdrop, then the ship is drawn over its middle */
    sk_direct_load(); sk_ship_ver++;
    Sheet *a4 = sheet("tilesets", "SF_Inside_A4"), *door = sheet("characters", "!SF_Door1");
    static const int bx[3] = {3, 8, 0}, by[3] = {11, 11, 6}, qx[1] = {3}, qy[1] = {1}, hx[1] = {8}, hy[1] = {1}, ex[1] = {11}, ey[1] = {6};
    static int pf_[SK_SHIP][SK_H][SK_W], pf3_[SK_SHIP][SK_H][SK_W];      /* the planet cell's pictures, kept to tell planet cells from hull cells after the ship rooms are drawn */
    for (int g = 0; g < SK_SHIP; g++) for (int y = 0; y < SK_H; y++) for (int x = 0; x < SK_W; x++) {      /* the planet below: its cells are copied in and SUNK under the ship's floor (negative extrusion in the ship desks' extrusion file), with a body hung under them; no planet = holes, only space */
        int sx_ = x + sk_dox, sy_ = y + sk_doy, lx_ = 0, ly_ = 0, pr_ = sk_pl_cur < 0 || sk_land_live() ? -1 : sk_wroom(sx_, sy_, &lx_, &ly_);      /* the world cell under the desk cell, shifted by direct flight: the planet page or one of its eight neighbours; none = open space */
        if (pr_ < 0) { sk_glyph[g][y][x] = '_'; sk_fs[g][y][x] = 0; sk_ws[g][y][x] = 0; sk_cdep[g][y][x] = 0; }
        else { sk_glyph[g][y][x] = sk_glyph[pr_][ly_][lx_]; sk_fs[g][y][x] = sk_fs[pr_][ly_][lx_]; sk_ws[g][y][x] = 0; sk_cdep[g][y][x] = 10 + sk_cdep[pr_][ly_][lx_]; if (sk_cdep[g][y][x] > 16) sk_cdep[g][y][x] = 16;      /* a body under every planet cell, at least as deep as the sink (negative EXTRUDE in the ship desks) or the sunk surface would be nothing */ if (sk_glyph[g][y][x] == 'T' || sk_glyph[g][y][x] == 'r') sk_glyph[g][y][x] = sk_glyph[g][y][x] == 'T' ? 'G' : 'p'; }
        if (pr_ >= 0 && tune("map_box", 0) && !(sx_ == -16 || sx_ == 31 || sy_ == -16 || sy_ == 31) && (lx_ == 0 || lx_ == 15 || ly_ == 0 || ly_ == 15)) sk_solid(g, x, y, 'G', 205, 205, 215, 0, 0);      /* the border of every 16 x 16 page: a pale line, so crossing from one page to the next shows */
    { int ex_ = x + sk_dox, ey_ = y + sk_doy; if (tune("map_box", 0) && sk_pl_cur >= 0 && ex_ >= 0 && ey_ >= 0 && ex_ < SK_W && ey_ < SK_H && (ex_ == 0 || ex_ == SK_W - 1 || ey_ == 0 || ey_ == SK_H - 1)) sk_solid(g, x, y, 'G', 150, 150, 160, 0, 0); }      /* MAP LIMIT in 2D (owner: 'the other one was 2D'): the edge of the planet page is a grey ring in the 2D scene and the minimap; the 3D picture gets the engine's wireframe cube instead (view.pdl debug_land), so the cell's 3D copy below keeps the planet underneath */
        pf_[g][y][x] = sk_fs[g][y][x]; pf3_[g][y][x] = pr_ >= 0 ? sk_fs3[pr_][ly_][lx_] : 0; }      /* trees and rocks become flat ground: seen from the ship the planet is a surface */
    sk_ship_room(0, a4, door, 8, 11, 6, 2, bx, by, 3);      /* bridge, the hub: green hex floor, doors in the south wall to the quarters (left) and the hold (right) and in the west wall to the engine room */
    sk_ship_room(1, a4, door, 4, 1, 4, 2, qx, qy, 1);      /* quarters: wood, a door in the north wall */
    sk_ship_room(2, a4, door, 10, 11, 6, 2, hx, hy, 1);      /* hold: dark grating, a door in the north wall */
    sk_ship_room(3, a4, door, 8, 6, 6, 2, ex, ey, 1);      /* engine room: dark diamond plate, a door in the east wall back to the bridge */
    sk_ship_deco();
    for (int g = 0; g < SK_SHIP; g++) for (int y = 0; y < SK_H; y++) for (int x = 0; x < SK_W; x++) { sk_fs3[g][y][x] = sk_fs[g][y][x] == pf_[g][y][x] ? pf3_[g][y][x] : sk_fs[g][y][x]; sk_ws3[g][y][x] = sk_ws[g][y][x]; }      /* the 3D export reads the flat copies: planet cells from the flat pages, the hull as drawn. The copy used to be made once at the full build, so the cells outside the hull stayed stale after any ship rebuild and the planet never showed outside the hull */
    { char lp[PATH_MAX]; snprintf(lp, sizeof lp, "%s/pieces/system/limit_box.txt", PCHQ); if (sk_pl_cur >= 0 && tune("map_box", 0)) { char lb[160]; snprintf(lb, sizeof lb, "%d -9 %d %d 6 %d\n", -16 - sk_dox, -16 - sk_doy, 32 - sk_dox, 32 - sk_doy); write_if_changed(lp, lb, strlen(lb)); } else unlink(lp); }      /* the map limit as ONE wireframe box for the engine (owner 2026-10-24: 'meant to be a single wireframe for debugging'): world x0 y0 z0 x1 y1 z1 of the 48 x 48 map around the planet page, shifted with the planet under the ship */
    for (int g = 0; g < SK_SHIP; g++) for (int ly = 0; ly < SSZ; ly++) for (int lx = 0; lx < SSZ; lx++) {      /* the hull is thin (one block hung under the floor): the ship hovers, the planet surface is sunk below it */
        int x = SOX + lx, y = SOY + ly; sk_cdep[g][y][x] = 1; }
}
/* THE SPACE DESK (design 13b): deep space is holes, every planet with a POS row (planets.pdl) is an island of 3 x 3 cells with a body hung under it (cell = 2 + 2 * grid cell, so grid 0..5 is cells 2..12), tinted by its tileset;
 * the ship is a hull of 3 x 1 cells (a tall middle block, a low wing each side) on the cell of state/ship.txt. A helm move moves these hull cells (a rigid translation of the box `AREA | hull | 3d | space`); the planets stay. */
static unsigned sk_space_ver = 0;      /* bumped whenever the space desk was rebuilt: the cached 2D background of that room is keyed on it */
static void sk_build_space(void) {
    int g = SK_SPACE; sk_space_ver++; static const char *TS[4] = { "rmmv", "cdda", "mcl", "doom" }; static const int TC[5][3] = { {90, 160, 70}, {120, 120, 128}, {150, 105, 65}, {175, 55, 35}, {130, 130, 130} };
    for (int y = 0; y < SK_H; y++) { for (int x = 0; x < SK_W; x++) { sk_glyph[g][y][x] = '_'; sk_fs[g][y][x] = sk_ws[g][y][x] = 0; sk_cdep[g][y][x] = 0; } sk_glyph[g][y][SK_W] = 0; }
    for (int k = 0; k < sk_pl_n; k++) { int gx, gy, ti = 4; if (!sk_pos_of(sk_pl[k].id, &gx, &gy)) continue; for (int t = 0; t < 4; t++) if (!strcmp(sk_pl[k].ts, TS[t])) ti = t;
        for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) { int x = 2 + gx * 2 + dx, y = 2 + gy * 2 + dy; if (x < 0 || y < 0 || x >= SK_W || y >= SK_H) continue;
            sk_solid(g, x, y, 'G', TC[ti][0], TC[ti][1], TC[ti][2], 0, 0); sk_cdep[g][y][x] = dx && dy ? 2 : dx || dy ? 3 : 5; } }
    int sx, sy; sk_ship_load(&sx, &sy); int cx = 2 + sx * 2, cy = 2 + sy * 2;
    for (int dx = -1; dx <= 1; dx++) { int x = cx + dx; if (x < 0 || x >= SK_W) continue; sk_cdep[g][cy][x] = 0;
        if (dx) sk_solid(g, x, cy, 'f', 170, 175, 185, 0, 0); else { sk_solid(g, x, cy, 'W', 205, 210, 220, 0, 1); sk_solid(g, x, cy, 'W', 150, 155, 165, 0, 0); } }
    sk_space_draw(g);      /* asteroids and other ships (space.pdl) */
    memcpy(sk_fs3[g], sk_fs[g], sizeof sk_fs3[g]); memcpy(sk_ws3[g], sk_ws[g], sizeof sk_ws3[g]);
}
static void sk_ensure_map(void) {
    sk_look_load(); if (sk_built && sk_built_sig == sk_look_sig) return; sk_pin_until = now_ms() + (sk_built ? 9000 : 45000); sk_built = 1;      /* a reload pins for 9 s, the first build (the launch, the engine takes a while to come up) for 45 s */
     sk_built_sig = sk_look_sig;
    Sheet *a5 = sheet("tilesets", "Outside_A5"), *ob = sheet("tilesets", "Outside_B");
    nslot = 1; memset(slotpx[0], 0, XT * XT * 4); memset(sk_glyph, 0, sizeof sk_glyph); memset(sk_fs, 0, sizeof sk_fs); memset(sk_ws, 0, sizeof sk_ws); memset(sk_cdep, 0, sizeof sk_cdep);
    char reg[200], cmd[PATH_MAX * 3], pg[2][PATH_MAX], sc[PATH_MAX]; int cx = 7, cy = 9, h[SK_H][SK_W];
    snprintf(reg, sizeof reg, "%s", sk_addr); char *c = strstr(reg, "/cell:"); if (c) { sscanf(c + 6, "%d,%d", &cx, &cy); *c = 0; }
    sk_scale_run(sc, sizeof sc);
    snprintf(pg[0], PATH_MAX, "%s/state/page_region.txt", APP); snprintf(pg[1], PATH_MAX, "%s/state/page_area.txt", APP);      /* pg[0] / pg[1] feed rooms SK_SHIP / SK_SHIP + 1 */
    if (!sh_arg_ok(sc) || !sh_arg_ok(reg) || !sh_arg_ok(sk_ts_cur)) { fprintf(stderr, PET_OP ": unsafe characters in the scale or address, the map is not regenerated\n"); return; }      /* the three strings below go into single quotes of a shell command */
    snprintf(cmd, sizeof cmd, "'%s/ops/+x/dust_gen.+x' page '%s' '%s' '%s' >/dev/null 2>&1", APP, sc, reg, pg[0]); if (system(cmd)) fprintf(stderr, PET_OP ": dust_gen region failed (%s)\n", reg);
    snprintf(cmd, sizeof cmd, "'%s/ops/+x/dust_gen.+x' page '%s' '%s/cell:%d,%d' '%s' >/dev/null 2>&1", APP, sc, reg, cx, cy, pg[1]); if (system(cmd)) fprintf(stderr, PET_OP ": dust_gen area failed (%s/cell:%d,%d)\n", reg, cx, cy);
    { int rx = 0, ry = 0; char *rg = strstr(reg, "region:"); if (rg) sscanf(rg + 7, "%d,%d", &rx, &ry); char gl[120]; snprintf(gl, sizeof gl, "%s", reg); if ((rg = strstr(gl, "region:"))) *rg = 0;
      for (int i = 0; i < 9; i++) { sk_nb_ok[i] = 0; if (i == 4) continue; char np[PATH_MAX]; snprintf(np, sizeof np, "%s/state/page_n%d.txt", APP, i);
        snprintf(cmd, sizeof cmd, "'%s/ops/+x/dust_gen.+x' page '%s' '%sregion:%d,%d' '%s' >/dev/null 2>&1", APP, sc, gl, rx + i % 3 - 1, ry + i / 3 - 1, np); unlink(np); (void)system(cmd); } }      /* the eight neighbour region pages of the planet page (a missing one, off the globe, stays open space) */
    const SkTs *ts = NULL; for (int k = 0; k < sk_ts_n; k++) if (!strcmp(sk_ts[k].id, sk_ts_cur)) ts = &sk_ts[k];
    static unsigned char tl[7][XT * XT * 4]; int have[7] = {0}, rm = !ts || !strcmp(ts->kind, "rmmv");
    if (!rm && ts && strcmp(ts->kind, "plain")) for (int b = 0; b < 7; b++) have[b] = sk_tile_load(ts, b, tl[b]);
    if (rm) { if (!a5) { fprintf(stderr, PET_OP ": missing RPG Maker sheets under %s\n", ASSETS); return; } tile24(a5, 0, 2 * S48, tl[3]); have[3] = 1; }
    int grass = slot_of(tl[3]); sk_rockslot = grass; if (!have[3]) { unsigned char g[XT * XT * 4]; for (int i = 0; i < XT * XT; i++) { g[i * 4] = SK_FLAT[3][0]; g[i * 4 + 1] = SK_FLAT[3][1]; g[i * 4 + 2] = SK_FLAT[3][2]; g[i * 4 + 3] = 255; } memcpy(tl[3], g, sizeof g); grass = slot_of(tl[3]); have[3] = 1; }
    for (int pass = 0; pass < 2; pass++) {      /* pass 0 paints the flat tiles (kept for the 3D export), pass 1 the autotiled ones (what the 2D scene shows) */
    Sheet *sa1 = rm && pass && !getenv("SK_NOAUTO") ? sheet("tilesets", "World_A1") : NULL, *sa2 = rm && pass && !getenv("SK_NOAUTO") ? sheet("tilesets", "World_A2") : NULL;      /* SK_NOAUTO=1 in the daemon environment paints the old flat tiles (to compare the 3D look) */
    for (int gi = 0; gi < 11; gi++) {
        int g = gi < 2 ? SK_SHIP + gi : SK_NB0 + gi - 2; if (g == SK_NB0 + 4) continue; char np[PATH_MAX];
        if (g >= SK_NB0) snprintf(np, sizeof np, "%s/state/page_n%d.txt", APP, g - SK_NB0);
        memset(h, 0, sizeof h); int okp = sk_read_page(g >= SK_NB0 ? np : pg[g - SK_SHIP], h); if (g >= SK_NB0) { sk_nb_ok[g - SK_NB0] = okp; if (!okp) continue; }
        for (int y = 0; y < SK_H; y++) { for (int x = 0; x < SK_W; x++) { int v = h[y][x], band = v < 20 ? 0 : v < 35 ? 1 : v < 45 ? 2 : v < 65 ? 3 : v < 80 ? 4 : v < 92 ? 5 : 6;
            sk_glyph[g][y][x] = 'G'; sk_fs[g][y][x] = grass; sk_ws[g][y][x] = 0; sk_cdep[g][y][x] = v / 10;
            if (rm && band == 4) { if (ob) sk_stamp(g, ob, (x + y) & 1, 0, x, y, 'T'); else sk_solid(g, x, y, 'T', 40, 110, 50, 0, 1); }
            else if (rm && band <= 2 && sa1 && sa2) sk_auto_cell(g, x, y, band, h, grass, sa1, sa2);
            else if (rm && band <= 2) sk_solid(g, x, y, band == 2 ? 'p' : '~', SK_FLAT[band][0], SK_FLAT[band][1], SK_FLAT[band][2], 0, 0);
            else if (rm && band >= 5 && sa1 && sa2) sk_auto_cell(g, x, y, band, h, grass, sa1, sa2);
            else if (rm && band >= 5) sk_solid(g, x, y, 'r', SK_FLAT[band][0], SK_FLAT[band][1], SK_FLAT[band][2], 0, 1);
            else if (!rm) sk_cell(g, x, y, band, grass, tl, have); }
            sk_glyph[g][y][SK_W] = 0; }
    }
    sk_world_draw(); sk_build_ship(); sk_build_space();
    if (!pass) { memcpy(sk_fs3, sk_fs, sizeof sk_fs3); memcpy(sk_ws3, sk_ws, sizeof sk_ws3); }
    }
    if (nslot > MAXSLOT - 40) fprintf(stderr, PET_OP ": tile slots %d of %d used, the picture slots are nearly full (slot_of then returns 0 for a new picture)\n", nslot, MAXSLOT);
}
/* a book reload resets the engine to the first desk (the bridge); the core only switches desks when a pet changes room, so after a reload the desk of the room being watched is selected again */
static void sk_desk_sync(void) {
    calc_view(); char ib[PATH_MAX]; snprintf(ib, sizeof ib, "%s/pieces/system/widget_cmds/inbox.txt", PCHQ); FILE *f = fopen(ib, "a");
    if (f) { fprintf(f, "CONFIRM_SET_DESK:%s\n", rooms[vroom].id); fclose(f); }
}
/* the verb side: drop the cache, rebuild, write the book again and tell the board engine to reload it */
static void sk_rebuild(char *msg, size_t cap) {
    sk_look_sig = -1; sk_look_load(); sk_built = 0; sk_ensure_map(); export3d(msg, cap);
    char ib[PATH_MAX]; snprintf(ib, sizeof ib, "%s/pieces/system/widget_cmds/inbox.txt", PCHQ); FILE *f = fopen(ib, "a"); if (f) { fputs("CONFIRM_START_MAP:" PET_APP "\n", f); fclose(f); }
    sk_desk_sync();
    snprintf(msg, cap, "%s, seed %llu, %s", sk_ts_cur, sk_seed, sk_addr);
}
/* the cached background of one room: sky behind the void cells, then the floor slot and the wall / object slot of each cell, at 24 px per cell. Written into fr (the 2D frame) at OX,OY. */
static unsigned char *sk_bg[SK_NR]; static char sk_bgkey[SK_NR][24];
static void sk_paint_bg(int room, const char *phase) {
    if (room < SK_SHIP || room == SK_SPACE) phase = "night";      /* the ship is in deep space: always dark (design D16) */
    sk_ensure_map(); if (room < 0 || room >= nroom || room >= SK_NR) return;
    Room *rm = &rooms[room]; int W = rm->cols * XT, H = rm->rows * XT; char key[24]; snprintf(key, sizeof key, "%s%lx%x", rm->outdoor ? phase : "-", (unsigned long)sk_built_sig & 0xffffff, room == SK_SPACE ? sk_space_ver & 0xfff : room < SK_SHIP ? sk_ship_ver & 0xfff : 0);
    if (!sk_bg[room] || strcmp(sk_bgkey[room], key)) {
        free(sk_bg[room]); sk_bg[room] = (unsigned char *)malloc((size_t)W * H * 4); if (!sk_bg[room]) return;
        Sheet *sky = rm->outdoor ? sheet("parallaxes", pet_sky_sheet(phase)) : NULL;
        for (int y = 0; y < rm->rows; y++) for (int x = 0; x < rm->cols; x++) for (int py = 0; py < XT; py++) for (int px = 0; px < XT; px++) {
            unsigned char c[3] = {120, 170, 220}; int ax = x * XT + px, ay = y * XT + py;
            if (sky && sky->px) { const unsigned char *q = sky->px + ((size_t)(ay % sky->h) * sky->w + ax % sky->w) * 4; c[0] = q[0]; c[1] = q[1]; c[2] = q[2]; }
            for (int layer = 0; layer < 2; layer++) { int sl = layer ? sk_ws[room][y][x] : sk_fs[room][y][x]; if (sl <= 0) continue; const unsigned char *q = slotpx[sl] + (py * XT + px) * 4; int al = q[3]; if (!al) continue; if (room < SK_SHIP && (x < SOX || y < SOY || x >= SOX + SSZ || y >= SOY + SSZ)) al = al * 55 / 100;      /* around the ship the planet is far below: seen through the stars at 55 % (the parallax sky stays visible in 2D) */
                for (int k = 0; k < 3; k++) c[k] = (unsigned char)((q[k] * al + c[k] * (255 - al)) / 255); }
            unsigned char *o = sk_bg[room] + ((size_t)ay * W + ax) * 4; o[0] = c[0]; o[1] = c[1]; o[2] = c[2]; o[3] = 255; }
        snprintf(sk_bgkey[room], sizeof sk_bgkey[room], "%s", key);
    }
    for (int y = 0; y < rm->rows * T; y++) { int sy = y * XT / T; if (OY + y < 0 || OY + y >= FH) continue;
        for (int x = 0; x < rm->cols * T; x++) { if (OX + x < 0 || OX + x >= FW) continue; memcpy(fr + ((size_t)(OY + y) * FW + OX + x) * 4, sk_bg[room] + ((size_t)sy * W + x * XT / T) * 4, 4); } }
}

static void sk_spawn(int i, int *x, int *y) {
    if (i < 0) { *x = 8; *y = 8; sk_near_free(0, x, y); return; }      /* the hero: the middle of the page, or the nearest dry cell */
    int k = i < nparty ? sk_row(party[i].name) : -1; if (k >= 0) { *x = sk_pt[k].x; *y = sk_pt[k].y; } else { *x = 7 + i % 3; *y = 9; } sk_near_free(0, x, y);
}
/* the pet's 48x48 picture: its art csv (64x64) box-filtered, or a coloured disc */
/* ---- the 3D export of one room (called by export3d for every room instead of the common painter): map.txt, extrusion.pdl, the 24 px tile atlas and cells.txt with the hung depth of every cell and the rock slot ---- */
static void pet_own_export_room(int r, const char *d) {
    sk_ensure_map(); if (r < 0 || r >= nroom || r >= SK_NR) return;
    int cols = rooms[r].cols, rows = rooms[r].rows; char p[PATH_MAX];
    static char gl[SK_H][SK_W + 1]; static int fs[SK_H][SK_W], ws[SK_H][SK_W]; memcpy(gl, sk_glyph[r], sizeof gl); memcpy(fs, sk_fs3[r], sizeof fs); memcpy(ws, sk_ws3[r], sizeof ws);
    for (int i = 0; i < npl; i++) if (pl[i].room == r) {      /* furniture the house placed: a 1 high block with the item's tile on the floor tile, like the common painter */
        Item *it = &cat[pl[i].item]; Sheet *s = sheet("tilesets", it->sheet); unsigned char a[XT * XT * 4], b[XT * XT * 4];
        for (int ci = 0; ci < it->w; ci++) for (int cj = 0; cj < it->h; cj++) { int x = pl[i].x + ci, y = pl[i].y + cj; if (x < 0 || x >= cols || y < 0 || y >= rows) continue;
            tile24(s, (it->c + ci) * S48, (it->r + cj) * S48, a); memset(b, 0, sizeof b); if (fs[y][x]) memcpy(b, slotpx[fs[y][x]], sizeof b); over24(b, a); gl[y][x] = 'f'; ws[y][x] = slot_of(b); } }
    snprintf(p, sizeof p, "%s/map.txt", d); { FILE *f = fopen_tmp(p); if (f) { for (int y = 0; y < rows; y++) fprintf(f, "%.*s\n", cols, gl[y]); fclose_tmp(f, p); } }
    snprintf(p, sizeof p, "%s/extrusion.pdl", d); { FILE *f = fopen_tmp(p); if (f) {
        static const char HEAD[] = "SECTION      | KEY                | VALUE\n----------------------------------------------------------------------\nMETA         | map_id             | " PET_APP "\nMETA         | desk_id            | %s\n";
        static const char PLANET[] = "EXTRUDE      | W                  | 3\nEXTRUDE      | D                  | 3\nEXTRUDE      | p                  | 1\nEXTRUDE      | G                  | 1\nEXTRUDE      | T                  | 2\nEXTRUDE      | r                  | 3\nEXTRUDE      | f                  | 1\nEXTRUDE      | default            | 0\n";
        static const char SHIP[] = "EXTRUDE      | ~                  | -9\nEXTRUDE      | p                  | -8\nEXTRUDE      | G                  | -8\nEXTRUDE      | W                  | 4\nEXTRUDE      | P                  | 8\nEXTRUDE      | D                  | 1\nEXTRUDE      | f                  | 1\nEXTRUDE      | default            | 0\n";      /* the ship is always 4 tall (owner): walls 4, floor level; the planet is a body hung UNDER the ship (the island trick: the engine draws hung rock under a floor, never under a hole) */
        static const char SPACE[] = "EXTRUDE      | G                  | 1\nEXTRUDE      | W                  | 3\nEXTRUDE      | f                  | 1\nEXTRUDE      | default            | 0\n";      /* islands 1 high, the hull 3 (middle) and 1 (wings) */
        fprintf(f, HEAD, rooms[r].id); fputs(r == SK_SPACE ? SPACE : r < SK_SHIP ? SHIP : PLANET, f); fclose_tmp(f, p); } }
    snprintf(p, sizeof p, "%s/lights.pdl", d);      /* this room's own lamps (the app's lights.pdl rows for it); a room with none gets no file, so the engine lights it by the sun as before */
    { char lp[PATH_MAX], ln[256], out[2048] = ""; size_t o = 0; snprintf(lp, sizeof lp, "%s/lights.pdl", APP); FILE *lf = fopen(lp, "r");
      while (lf && fgets(ln, sizeof ln, lf)) { char *fl[12]; int n = split_bar(ln, fl, 12); if (n < 3 || ln[0] == '#' || strcmp(fl[1], rooms[r].id)) continue;
          if (!strcmp(fl[0], "AMBIENT")) o += (size_t)snprintf(out + o, sizeof out - o, "AMBIENT | %s\n", fl[2]);
          else if (!strcmp(fl[0], "LIGHT") && n >= 10) o += (size_t)snprintf(out + o, sizeof out - o, "LIGHT | %s | %s | %s | %s | %s | %s | %s | %s\n", fl[2], fl[3], fl[4], fl[5], fl[6], fl[7], fl[8], fl[9]);
          if (o > sizeof out - 160) break; }
      if (lf) fclose(lf);
      if (o) { FILE *f = fopen_tmp(p); if (f) { fputs(out, f); fclose_tmp(f, p); } } else remove(p); }
    snprintf(p, sizeof p, "%s/hotspots.txt", d);      /* event voxels of this room: its doors (the door object is the cell tx,ty) + the app's hotspots.pdl rows; the engine outlines them in orange while In: is off */
    { char hp[PATH_MAX], ln[256], out[2048] = ""; size_t o = 0; int nh = 0;
      for (int i = 0; i < ndoor && r < SK_SHIP; i++) if (doors[i].room == r) { o += (size_t)snprintf(out + o, sizeof out - o, "HOT | %d | %d | 0 | door to %s\n", doors[i].tx, doors[i].ty, rooms[doors[i].dest].name); nh++; }
      snprintf(hp, sizeof hp, "%s/hotspots.pdl", APP); FILE *hf = fopen(hp, "r");
      while (hf && fgets(ln, sizeof ln, hf)) { char *fl[8]; int n = split_bar(ln, fl, 8); if (n < 6 || ln[0] == '#' || strcmp(fl[0], "HOT") || strcmp(fl[1], rooms[r].id)) continue;
          o += (size_t)snprintf(out + o, sizeof out - o, "HOT | %s | %s | %s | %s\n", fl[2], fl[3], fl[4], fl[5]); nh++; if (o > sizeof out - 200) break; }
      if (hf) fclose(hf);
      if (nh) { FILE *f = fopen_tmp(p); if (f) { fputs(out, f); fclose_tmp(f, p); } } else remove(p); }
    int acols = 16, nrows = (nslot + acols - 1) / acols, aw = acols * XT, ah = nrows * XT; unsigned char *atlas = (unsigned char *)calloc((size_t)aw * ah * 4, 1); if (!atlas) return;
    for (int i = 0; i < nslot; i++) for (int y = 0; y < XT; y++) memcpy(atlas + ((size_t)((i / acols) * XT + y) * aw + (i % acols) * XT) * 4, slotpx[i] + y * XT * 4, XT * 4);
    snprintf(p, sizeof p, "%s/cells.rgba", d); { FILE *f = fopen_tmp(p); if (f) { fwrite(atlas, 1, (size_t)aw * ah * 4, f); fclose_tmp(f, p); } } { char mk[PATH_MAX]; snprintf(mk, sizeof mk, "%s/cells.marker", d); FILE *mf = fopen(mk, "a"); if (mf) { fputc('\n', mf); fclose(mf); } } snprintf(p, sizeof p, "%s/cells.png", d); png_atomic(p, aw, ah, atlas, aw * 4); free(atlas);
    snprintf(p, sizeof p, "%s/cells.txt", d); { FILE *f = fopen_tmp(p); if (f) { fprintf(f, "width=%d\nheight=%d\ntile_px=%d\natlas_cols=16\natlas_tiles=%d\nrock_slot=%d\ncells\n", cols, rows, XT, nslot, sk_rockslot);
        for (int y = 0; y < rows; y++) { for (int x = 0; x < cols; x++) {
            fprintf(f, "%s%d,%d,%d", x ? " " : "", fs[y][x], ws[y][x], sk_cdep[r][y][x]);
            int ur_ = -1, ulx_ = 0, uly_ = 0; if (r < SK_SHIP && sk_pl_cur >= 0 && !sk_land_live() && x >= SOX && x < SOX + SSZ && y >= SOY && y < SOY + SSZ) ur_ = sk_wroom(x + sk_dox, y + sk_doy, &ulx_, &uly_);
            if (ur_ >= 0 && sk_fs3[ur_][uly_][ulx_]) {      /* the planet's own centre under the hovering hull: a second solid run (engine cells.txt tokens 4..6: tile, depth, sink) so the planet is whole under the ship */
                char pg = sk_glyph[ur_][uly_][ulx_]; int ud = 10 + sk_cdep[ur_][uly_][ulx_]; if (ud > 16) ud = 16;
                fprintf(f, ",%d,%d,%d", sk_fs3[ur_][uly_][ulx_], ud, pg == '~' ? 9 : 8); } }
          fputc('\n', f); } fclose_tmp(f, p); } }
}
#elif PET_SECTION == 74
    { int sx, sy; sk_spawn(i, &sx, &sy); e->room = 0; e->x = sx; e->y = sy; }
#elif PET_SECTION == 75
/* THE HELM (owner 2026-10-24: "do what must be done for drive event to make sense"). The console is an EVENT (events/helm/, trigger "action") in the RPG Maker sense: a page with a condition, an AREA of voxels it affects (area.pdl) and a
 * command. Enter next to a console cell (hotspots.pdl HOT row with an event name) plays the page; the page runs the verb "helm on": flag_helm=1. At the helm the arrows (Up Down Left Right) move the SHIP one cell on the galaxy grid
 * (planets.pdl POS rows, state/ship.txt); a cell with a planet puts the ship above it (the same "planet" verb), an empty cell is deep space. Esc (or In: off) lets go. Design: 16.game/EVENT-AREA-SELECTION-DESIGN.md */
#define SK_GRID 6
static int sk_pos_file(const char *id, int *gx, int *gy) {      /* the POS row of planets.pdl, if any */
    char p[PATH_MAX], ln[300]; char *f[6]; snprintf(p, sizeof p, "%s/planets.pdl", APP); FILE *fp = fopen(p, "r"); int hit = 0; while (fp && fgets(ln, sizeof ln, fp)) if (ln[0] != '#' && split_bar(ln, f, 6) >= 4 && !strcmp(f[0], "POS") && !strcmp(f[1], id)) { *gx = atoi(f[2]); *gy = atoi(f[3]); hit = 1; } if (fp) fclose(fp); return hit; }
/* A planet with no POS row (newplanet) gets the first EMPTY grid cell: scan row by row, column by column, and take the first cell at least 2 grid cells (Chebyshev) from every planet that already has a place (islands are 3 cells wide, 2 cells apart per grid step, so 2 apart keeps them from touching). Planets are placed in list order after all the POS rows, so the result never changes between calls. */
static int sk_pos_of(const char *id, int *gx, int *gy) {
    if (sk_pos_file(id, gx, gy)) return 1; int n = sk_pl_n, px[16], py[16], has[16] = {0}; if (n > 16) n = 16;
    for (int k = 0; k < n; k++) has[k] = sk_pos_file(sk_pl[k].id, &px[k], &py[k]);
    for (int k = 0; k < n; k++) { if (has[k]) continue; int fx = -1, fy = -1;
        for (int y = 0; y < 6 && fx < 0; y++) for (int x = 0; x < 6 && fx < 0; x++) { int ok = 1; for (int m = 0; m < n; m++) if (has[m] && abs(px[m] - x) < 2 && abs(py[m] - y) < 2) ok = 0; if (ok) { fx = x; fy = y; } }
        if (fx < 0) return 0;      /* the map is full */
        px[k] = fx; py[k] = fy; has[k] = 1; if (!strcmp(sk_pl[k].id, id)) { *gx = fx; *gy = fy; return 1; } }
    return 0; }
static void sk_ship_load(int *gx, int *gy) { char p[PATH_MAX], b[40]; spath(p, "ship.txt"); read_file(p, b, sizeof b); if (sscanf(b, "%d %d", gx, gy) == 2) return; *gx = *gy = 0; sk_look_load(); if (sk_pl_cur >= 0) sk_pos_of(sk_pl[sk_pl_cur].id, gx, gy); }
static void sk_ship_save(int gx, int gy) { char p[PATH_MAX], b[40]; spath(p, "ship.txt"); snprintf(b, sizeof b, "%d %d\n", gx, gy); write_if_changed(p, b, strlen(b)); }
static int sk_area_cells(char *what, size_t cap) {      /* events/helm/area.pdl: AREA | name | mode | room | x0 | y0 | z0 | x1 | y1 | z1  ->  how many voxels the event touches (the hull) */
    char p[PATH_MAX], ln[300]; char *f[12]; long vox = 0; int n = 0; snprintf(p, sizeof p, "%s/events/helm/area.pdl", APP); FILE *fp = fopen(p, "r");
    while (fp && fgets(ln, sizeof ln, fp)) if (ln[0] != '#' && split_bar(ln, f, 12) >= 10 && !strcmp(f[0], "AREA")) { vox += (long)(atoi(f[7]) - atoi(f[4]) + 1) * (atoi(f[8]) - atoi(f[5]) + 1) * (atoi(f[9]) - atoi(f[6]) + 1); n++; }
    if (fp) fclose(fp); snprintf(what, cap, "%d boxes, %ld voxels", n, vox); return n;
}
static int sk_choice(char *title, char *q, char *yes, char *no, char *direct) {      /* events/helm/choice.pdl: CHOICE | title | question | yes label | no label */
    char p[PATH_MAX], ln[300]; char *f[8]; snprintf(p, sizeof p, "%s/events/helm/choice.pdl", APP); FILE *fp = fopen(p, "r"); int hit = 0;
    while (fp && !hit && fgets(ln, sizeof ln, fp)) { int nf = ln[0] != '#' ? split_bar(ln, f, 7) : 0; if (nf >= 5 && !strcmp(f[0], "CHOICE")) { snprintf(title, 48, "%s", f[1]); snprintf(q, 80, "%s", f[2]); snprintf(yes, 48, "%s", f[3]); direct[0] = 0; if (nf >= 6) { snprintf(direct, 48, "%s", f[4]); snprintf(no, 48, "%s", f[5]); } else snprintf(no, 48, "%s", f[4]); hit = 1; } }
    if (fp) fclose(fp); return hit; }
/* THE LAND AS ENTITIES (owner 2026-10-24: "even the land is meant to be its own entity", "nothing has to change for direct, just redirect the arrow keys into a vehicle move"). In direct flight the planet is nine 16 x 16 land pieces (pieces/land_0..8: state.txt + model.txt, the page and its eight neighbours) drawn on the BRIDGE desk under the real hull. The arrows only change state/direct.txt and the pieces' positions: page cell (x+ox, y+oy) lies under desk cell (x, y), so the ship moves over the land (camera and crew stay on the ship). The baked planet is dropped from the desks while the pieces are live (one reload when the helm is taken). */
static int sk_helm_direct(void);
static int sk_land_live(void) { char p[PATH_MAX], b[20]; spath(p, "helm_mode.txt"); read_file(p, b, sizeof b); return flag_on("helm", 0) && !strncmp(b, "direct", 6); }
static void sk_slot_avg(int slot, int *r, int *g, int *b) { long R = 0, G = 0, B = 0, n = 0; if (slot > 0 && slot < MAXSLOT) for (int i = 0; i < XT * XT; i++) { const unsigned char *q = slotpx[slot] + i * 4; if (q[3] < 128) continue; R += q[0]; G += q[1]; B += q[2]; n++; } if (!n) { *r = 120; *g = 125; *b = 135; return; } *r = (int)(R / n); *g = (int)(G / n); *b = (int)(B / n); }
static int sk_doz_get(void) { char p[PATH_MAX], b[16]; spath(p, "direct_z.txt"); read_file(p, b, sizeof b); return atoi(b); }      /* the ship's altitude over its start (owner 2026-10-25: z / x move the ship up and down); the land pieces sink by the same amount, the picture around the hull stays */
static void sk_ship_world_write(void) {      /* the ship is the entity that moves: its world position (cells from the page origin) is the truth, the land pieces are placed relative to it, so the picture around the hull never changes while it flies. desk=none = the engine does not draw it (the real hull stays the baked bridge desk). */
    char d[PATH_MAX], p[PATH_MAX], b[200]; snprintf(d, sizeof d, "%s/pieces/ship_01", PCHQ); mkdir(d, 0775); snprintf(p, sizeof p, "%s/state.txt", d);
    snprintf(b, sizeof b, "entity_type=vehicle\ndesk=none\npos_x=%d\npos_y=%d\npos_z=%d\nsize_x=12\nsize_y=12\nsize_h=4\n", sk_dox, sk_doy, sk_doz_get()); write_if_changed(p, b, strlen(b)); }
static void sk_land_write(int models) {      /* the land is ONE entity now (pieces/land_01): the engine fills the empty cells of the bridge desk from the planet page desk, shifted by the entity's position, with the old textured / extruded look (engine maker_load). models is unused. */
    (void)models; sk_direct_load(); sk_ship_world_write(); sk_veh_write(); char d[PATH_MAX], p[PATH_MAX], b[300];
    snprintf(d, sizeof d, "%s/pieces/land_01", PCHQ); mkdir(d, 0775); snprintf(p, sizeof p, "%s/state.txt", d);
    snprintf(b, sizeof b, "entity_type=land\npos_x=%d\npos_y=%d\nalt=%d\nsrc=region\n", -sk_dox, -sk_doy, sk_doz_get()); write_if_changed(p, b, strlen(b));
}
static void sk_land_remove(void) { char p[PATH_MAX]; sk_veh_clear(); snprintf(p, sizeof p, "%s/pieces/ship_01/state.txt", PCHQ); unlink(p); snprintf(p, sizeof p, "%s/pieces/land_01/state.txt", PCHQ); unlink(p); }
static int sk_helm_direct(void) { char p[PATH_MAX], b[20]; spath(p, "helm_mode.txt"); read_file(p, b, sizeof b); return !strncmp(b, "direct", 6); }
static void sk_ship_refresh(char *msg, size_t cap) {      /* the page under the hull moved: rebuild the ship rooms, write the book, reload it, show the same desk again */
    sk_build_ship(); export3d(msg, cap); char ib[PATH_MAX]; snprintf(ib, sizeof ib, "%s/pieces/system/widget_cmds/inbox.txt", PCHQ); FILE *f = fopen(ib, "a"); if (f) { fputs("CONFIRM_START_MAP:" PET_APP "\n", f); fclose(f); } pet_load(); calc_view(); sk_desk_sync();
}
static void sk_helm_put(int room, int x, int y) {      /* the controlled one stands on (room, x, y): the helm makes them the ship */
    int ci = ctl_idx(); use_ent(ci); pet.room = room; pet.x = x; pet.y = y; pet.dir = 0; pet.tx = pet.ty = -1; pet_save(); use_ent(active); set_flag("follow", ci < 0 ? "player" : "pet"); g_pin_now = 1; sk_helm_settle = now_ms() + 1800;
}
static void sk_space_refresh(char *msg, size_t cap) {      /* the hull cells moved: rebuild the space desk, write the book, tell the engine to reload it, show the desk again */
    sk_build_space(); export3d(msg, cap); char ib[PATH_MAX]; snprintf(ib, sizeof ib, "%s/pieces/system/widget_cmds/inbox.txt", PCHQ); FILE *f = fopen(ib, "a"); if (f) { fputs("CONFIRM_START_MAP:" PET_APP "\n", f); fclose(f); } pet_load(); calc_view(); sk_desk_sync();
}
static void sk_helm_verb(int argc, char **argv, char *msg, size_t cap) {
    const char *a = argc >= 2 ? argv[1] : "status";
    if (!strcmp(a, "no")) { set_flag("ask", "0"); snprintf(msg, cap, "you stay at the console"); return; }
    if (!strcmp(a, "direct") || !strcmp(a, "galaxy")) { char mp[PATH_MAX]; spath(mp, "helm_mode.txt"); write_if_changed(mp, a[0] == 'd' ? "direct\n" : "galaxy\n", a[0] == 'd' ? 7 : 7); a = "yes"; }
    if (!strcmp(a, "yes")) { set_flag("ask", "0"); if (flag_on("helm", 0)) { snprintf(msg, cap, "already at the helm"); return; } if (!run_event_x("helm", "action", "")) { char *pv[2] = { "helm", "on" }; sk_helm_verb(2, pv, msg, cap); } else snprintf(msg, cap, "at the helm: the arrows steer the ship. Esc lets go"); return; }
    if (!strcmp(a, "on")) { char w[80]; sk_area_cells(w, sizeof w); if (flag_on("helm", 0)) { snprintf(msg, cap, "already at the helm"); return; }
        if (sk_helm_direct()) {      /* direct: the ship does not change; the planet becomes land pieces under it */
            sk_direct_load(); set_flag("helm", "1"); sk_ship_refresh(msg, cap); sk_land_write(1); sk_helm_settle = now_ms() + 2500;
            char b[160]; snprintf(b, sizeof b, "HELM|%ld|on-direct|%d,%d", (long)time(NULL), sk_dox, sk_doy); append_line("events.txt", b); snprintf(msg, cap, "direct flight: the land is nine pieces under the ship; the arrows move them. Esc lets go"); return; }
        { int ci = ctl_idx(); Ent *e = ci < 0 ? &tr : &others[ci]; char b[80]; snprintf(b, sizeof b, "%d %d %d\n", e->room, e->x, e->y); char hp[PATH_MAX]; spath(hp, "helm_from.txt"); write_if_changed(hp, b, strlen(b)); }      /* where to put the pilot back */
        int gx, gy; sk_ship_load(&gx, &gy); set_flag("helm", "1"); sk_helm_put(SK_SPACE, 2 + gx * 2, 2 + gy * 2); sk_space_refresh(msg, cap);
        char b[160]; snprintf(b, sizeof b, "HELM|%ld|on|%s", (long)time(NULL), w); append_line("events.txt", b); snprintf(msg, cap, "at the helm: the arrows steer the ship over the space map (area: %s). Esc lets go", w); return; }
    if (!strcmp(a, "off")) { if (!flag_on("helm", 0)) { snprintf(msg, cap, "not at the helm"); return; } set_flag("helm", "0");
        if (sk_helm_direct()) { sk_land_remove(); sk_rebuild(msg, cap);      /* full rebuild: the planet goes back into the bridge desk's book (a plain refresh left the stale flat copy and no planet) */
            char b[80]; snprintf(b, sizeof b, "HELM|%ld|off-direct", (long)time(NULL)); append_line("events.txt", b); snprintf(msg, cap, "the helm is released: the ship holds over %s", sk_pl_cur < 0 ? "deep space" : sk_pl[sk_pl_cur].label); return; }
        { char hp[PATH_MAX], b[80] = ""; int r = 0, x = 7, y = 4; spath(hp, "helm_from.txt"); read_file(hp, b, sizeof b); sscanf(b, "%d %d %d", &r, &x, &y); sk_helm_put(r, x, y); }
        { sk_look_load(); int gx, gy; sk_ship_load(&gx, &gy); const char *to = "none", *cur = sk_pl_cur < 0 ? "none" : sk_pl[sk_pl_cur].id; for (int k = 0; k < sk_pl_n; k++) { int px, py; if (sk_pos_of(sk_pl[k].id, &px, &py) && px == gx && py == gy) to = sk_pl[k].id; }
          if (strcmp(to, cur)) { char *pv[3] = { "planet", (char *)to, "fly" }; verb(3, pv, msg, cap); } else sk_space_refresh(msg, cap); }      /* landing: the planet under the ship becomes the one under the hull */
        char b[80]; snprintf(b, sizeof b, "HELM|%ld|off", (long)time(NULL)); append_line("events.txt", b); snprintf(msg, cap, "the helm is released: the ship holds over %s", sk_pl_cur < 0 ? "deep space" : sk_pl[sk_pl_cur].label); return; }
    if (!strcmp(a, "lift") && argc >= 3) {
        if (!flag_on("helm", 0) || !sk_helm_direct()) { snprintf(msg, cap, "not in direct flight"); return; }
        int z = sk_doz_get() + atoi(argv[2]), zmin = tune("direct_z_min", -8), zmax = tune("direct_z_max", 12); if (z < zmin || z > zmax) { snprintf(msg, cap, "the ship cannot go %s than this (%d)", z < 0 ? "lower" : "higher", sk_doz_get()); return; }
        sk_direct_load(); if (sk_coll_check(sk_dox, sk_doy, z, msg, cap)) return;      /* the hull would touch the land (collision.pdl) */
        { char zp[PATH_MAX], zb[16]; spath(zp, "direct_z.txt"); snprintf(zb, sizeof zb, "%d\n", z); write_if_changed(zp, zb, strlen(zb)); char rb[100]; snprintf(rb, sizeof rb, "DIRECT|%ld|lift|%d", (long)time(NULL), z); append_line("events.txt", rb); }
        sk_direct_load(); sk_land_write(0); snprintf(msg, cap, "ship altitude %d", z); return; }
    if (!strcmp(a, "move") && argc >= 4) {
        if (!flag_on("helm", 0)) { snprintf(msg, cap, "not at the helm"); return; }
        if (sk_helm_direct()) {      /* direct: the arrows move the land pieces under the ship (state/direct.txt + pieces/land_*); no rebuild, no reload */
            sk_direct_load(); int nx = sk_dox + atoi(argv[2]), ny = sk_doy + atoi(argv[3]); int mx = tune("direct_max", 16); if (mx > 16) mx = 16;
            if (nx < -mx || nx > mx || ny < -mx || ny > mx) { snprintf(msg, cap, "the edge of the map: the ship stops (%d,%d)", sk_dox, sk_doy); return; }      /* step 1 of canPass: the map limit */
            if (sk_world_block(nx, ny, sk_doz_get(), msg, cap)) return;      /* an asteroid or another ship of the planet map under the hull */
            if (sk_coll_check(nx, ny, sk_doz_get(), msg, cap)) return;      /* step 3: solid land */
            { char dp[PATH_MAX], db[24], rb[120]; spath(dp, "direct.txt"); snprintf(db, sizeof db, "%d %d\n", nx, ny); write_if_changed(dp, db, strlen(db)); snprintf(rb, sizeof rb, "DIRECT|%ld|ship|%d,%d|%d,%d", (long)time(NULL), sk_dox, sk_doy, nx, ny); append_line("events.txt", rb); }
            sk_dox = nx; sk_doy = ny; sk_land_write(0); sk_helm_settle = now_ms() + 300; snprintf(msg, cap, "ship at %d,%d over %s", nx, ny, sk_pl_cur < 0 ? "deep space" : sk_pl[sk_pl_cur].label); return; }
        int gx, gy, dx = atoi(argv[2]), dy = atoi(argv[3]); sk_ship_load(&gx, &gy); int nx = gx + dx, ny = gy + dy;
        if (nx < 0 || ny < 0 || nx >= SK_GRID || ny >= SK_GRID) { snprintf(msg, cap, "the edge of the charted system (%d,%d)", gx, gy); return; }
        if (sk_space_block(nx, ny, msg, cap)) return;      /* an asteroid or another ship sits there */
        sk_ship_save(nx, ny); char b[100]; snprintf(b, sizeof b, "FLY|%ld|ship|%d,%d|%d,%d", (long)time(NULL), gx, gy, nx, ny); append_line("events.txt", b);
        sk_helm_put(SK_SPACE, 2 + nx * 2, 2 + ny * 2); sk_space_refresh(msg, cap);
        const char *over = "deep space"; sk_look_load(); for (int k = 0; k < sk_pl_n; k++) { int px, py; if (sk_pos_of(sk_pl[k].id, &px, &py) && px == nx && py == ny) over = sk_pl[k].label; }
        snprintf(msg, cap, "ship at %d,%d over %s", nx, ny, over); return; }
    int gx, gy; sk_ship_load(&gx, &gy); snprintf(msg, cap, "helm %s, ship at %d,%d", flag_on("helm", 0) ? "on" : "off", gx, gy);
}
static int sk_helm_follow(char *msg, size_t cap) {
    static int lx = -9, ly = -9; static long settle_unused = 0; (void)settle_unused; if (now_ms() < sk_helm_settle) { g_pin_now = 1; return 0; }      /* a planet change reloads the scene and moves the xelector by itself: wait it out */
    int ci = ctl_idx(); Ent *e = ci < 0 ? &tr : &others[ci]; char xp[PATH_MAX], kb[512]; snprintf(xp, sizeof xp, "%s/pieces/xelector_01/state.txt", PCHQ); read_file(xp, kb, sizeof kb); int xx = -1, xy = -1;
    for (char *l = strtok(kb, "\n"); l; l = strtok(NULL, "\n")) { if (!strncmp(l, "pos_x=", 6)) xx = atoi(l + 6); else if (!strncmp(l, "pos_y=", 6)) xy = atoi(l + 6); }
    { static int bz = -99; int zz = -99; char kz[512]; read_file(xp, kz, sizeof kz); char *q = strstr(kz, "pos_z="); if (q) zz = atoi(q + 6); if (!flag_on("helm", 0)) bz = -99; else if (zz != -99) { if (bz == -99) bz = zz; else if (zz != bz && sk_helm_direct()) { char zs[8], *hv[3] = { "helm", "lift", zs }; snprintf(zs, sizeof zs, "%d", zz > bz ? 1 : -1); bz = zz; sk_helm_verb(3, hv, msg, cap); return 1; } } }
    if (xx < 0 || xy < 0) return 0; if (xx == e->x && xy == e->y) { lx = ly = -9; return 0; } if (xx == lx && xy == ly) { g_pin_now = 1; return 0; }
    lx = xx; ly = xy; int ddx = xx - e->x, ddy = xy - e->y, dx = 0, dy = 0; if (abs(ddx) + abs(ddy) != 1) { g_pin_now = 1; return 0; }      /* an arrow moves the xelector exactly one cell from the pilot; any other offset is the engine's own desk reload, not a key */ if (abs(ddx) >= abs(ddy)) dx = ddx > 0 ? 1 : -1; else dy = ddy > 0 ? 1 : -1;
    char a[16], b[16], *hv[4]; snprintf(a, sizeof a, "%d", dx); snprintf(b, sizeof b, "%d", dy); hv[0] = "helm"; hv[1] = "move"; hv[2] = a; hv[3] = b; sk_helm_verb(4, hv, msg, cap); g_pin_now = 1; sk_helm_settle = now_ms() + 1500; lx = ly = -9; return 1;
}
/* Enter next to an event cell: the first HOT row of this room within one cell (not diagonal) of the controlled character that names an event page plays that page with trigger "action". */
static int sk_helm_interact(char *msg, size_t cap) {
    if (flag_on("helm", 0)) return 0;      /* at the helm (direct flight keeps the pilot on the bridge, next to the console): Enter is not a second question */
    int ci = ctl_idx(); Ent *e = ci < 0 ? &tr : &others[ci]; char p[PATH_MAX], ln[400]; char *f[8]; snprintf(p, sizeof p, "%s/hotspots.pdl", APP); FILE *fp = fopen(p, "r"); int done = 0;
    while (fp && !done && fgets(ln, sizeof ln, fp)) { if (ln[0] == '#' || split_bar(ln, f, 8) < 7 || strcmp(f[0], "HOT") || strcmp(f[1], rooms[e->room].id) || !f[6][0]) continue;
        int hx = atoi(f[2]), hy = atoi(f[3]); if (abs(hx - e->x) + abs(hy - e->y) != 1) continue;
        char ev[48]; snprintf(ev, sizeof ev, "%s", f[6]); fclose(fp); fp = NULL; done = 1;
        { char ct[48], cq[80], cy[48], cn[48], cd[48]; if (!strcmp(ev, "helm") && sk_choice(ct, cq, cy, cn, cd)) { set_flag("ask", "1"); snprintf(msg, cap, "%s", cq); { char b[120]; snprintf(b, sizeof b, "EVENT|%ld|%s|ask|%s|%d,%d", (long)time(NULL), ev, rooms[e->room].id, hx, hy); append_line("events.txt", b); } return 1; } }      /* a choice popup first: Yes plays the page (verb helm yes) */
        if (!run_event_x(ev, "action", "") ) { char *pv[2] = { "helm", "on" }; verb(2, pv, msg, cap); } else if (flag_on("helm", 0)) snprintf(msg, cap, "at the helm: the arrows steer the ship. Esc lets go"); else snprintf(msg, cap, "%s: played", ev);
        sk_helm_settle = now_ms() + 2500;      /* the page moved the pilot and the desk: this iteration of the daemon must not read the xelector as a steering move */
        { char b[120]; snprintf(b, sizeof b, "EVENT|%ld|%s|action|%s|%d,%d", (long)time(NULL), ev, rooms[e->room].id, hx, hy); append_line("events.txt", b); } }
    if (fp) fclose(fp); return done;
}
/* LIFE, DEATH, RESPAWN, DROPS (owner 2026-10-25: "everyone dies, pets included; hunger + damage, vacuum later by events; respawn on the ship bridge; half the items drop, anyone can take them; toggle the drop and permadeath / per-session death in .pdl").
 * Config: life.pdl (LIFE | key | value, re-read every 2 s). Per character: state/life_<id>.txt (hp, hunger, dead, deaths, inv=key:n,...). Drops: state/drops.txt (live drops, rewritten on change). Log rows DEATH / RESPAWN / DROP / TAKE in state/events.txt.
 * The behaviour is in these C functions; event pages (events/death, events/respawn, trigger parallel) can call the verbs `life hurt|feed|give|take|status|reset`, the way RPG Maker's death state and common events do. Design: 16.game/rpg-space-vehicle-entities (death section). */
typedef struct { int id, room, x, y, n, dmg; char item[24], owner[24], src[24]; } SkDrop;      /* a remnant: what a destroyed thing leaves, sits there until collected; dmg > 0 = damaged (collected as <item>_damaged), src = what it came from */
static SkDrop sk_drops[64]; static int sk_ndrops, sk_dropid = 1;
typedef struct { int hp, hunger, dead, deaths; char inv[400]; long last; } SkLife;
static SkLife sk_life[NP + 1];      /* index 0 = the hero, 1.. = party */
static char sk_lc_mode[16] = "respawn", sk_lc_room[16] = "bridge"; static int sk_lc_on = 1, sk_lc_hunger_ms = 60000, sk_lc_starve = 1, sk_lc_drop = 50, sk_lc_rx = 7, sk_lc_ry = 5, sk_lc_range = 2; static long sk_lc_at;
static void sk_life_cfg(void) {
    long now = now_ms(); if (sk_lc_at && now - sk_lc_at < 2000) return; sk_lc_at = now; char p[PATH_MAX], ln[300]; char *f[4]; snprintf(p, sizeof p, "%s/life.pdl", APP); FILE *fp = fopen(p, "r"); if (!fp) return;
    while (fgets(ln, sizeof ln, fp)) { if (ln[0] == '#' || split_bar(ln, f, 4) < 3 || strcmp(f[0], "LIFE")) continue; const char *k = f[1], *v = f[2];
        if (!strcmp(k, "enabled")) sk_lc_on = atoi(v); else if (!strcmp(k, "hunger_ms")) sk_lc_hunger_ms = atoi(v); else if (!strcmp(k, "starve_hp")) sk_lc_starve = atoi(v); else if (!strcmp(k, "drop_pct")) sk_lc_drop = atoi(v);
        else if (!strcmp(k, "death_mode")) snprintf(sk_lc_mode, sizeof sk_lc_mode, "%.15s", v); else if (!strcmp(k, "respawn_room")) snprintf(sk_lc_room, sizeof sk_lc_room, "%.15s", v); else if (!strcmp(k, "respawn_x")) sk_lc_rx = atoi(v); else if (!strcmp(k, "respawn_y")) sk_lc_ry = atoi(v); else if (!strcmp(k, "take_range")) sk_lc_range = atoi(v); }
    fclose(fp);
    for (char *q = sk_lc_mode; *q; q++) if (*q == ' ') *q = 0;      /* trailing blanks of the pdl cell */
    for (char *q = sk_lc_room; *q; q++) if (*q == ' ') *q = 0;
}
static int sk_life_slot(const char *id) { if (!id || !id[0]) return -2; char lo[24]; snprintf(lo, sizeof lo, "%s", id); for (char *q = lo; *q; q++) if (*q >= 'A' && *q <= 'Z') *q += 32; char hn[24]; snprintf(hn, sizeof hn, "%s", hero.name); for (char *q = hn; *q; q++) if (*q >= 'A' && *q <= 'Z') *q += 32; if (!strcmp(lo, hn)) return 0; for (int i = 0; i < nparty; i++) if (!strcmp(lo, party[i].id)) return i + 1; return -2; }
static const char *sk_life_id(int slot) { static char b[24]; if (slot == 0) { snprintf(b, sizeof b, "%s", hero.name); for (char *q = b; *q; q++) if (*q >= 'A' && *q <= 'Z') *q += 32; return b; } return party[slot - 1].id; }
static int sk_life_mhp(int slot) { int m = slot == 0 ? hero.mhp : party[slot - 1].mhp; return m > 0 ? m : 40; }
static Ent *sk_life_ent(int slot) { return slot == 0 ? &tr : &others[slot - 1]; }
static void sk_life_path(int slot, char *p, size_t n) { char nm[40]; snprintf(nm, sizeof nm, "life_%s.txt", sk_life_id(slot)); spath(p, nm); (void)n; }
static void sk_life_load(int slot) { SkLife *L = &sk_life[slot]; char p[PATH_MAX], b[900]; sk_life_path(slot, p, sizeof p); int had = read_file(p, b, sizeof b); L->hp = sk_life_mhp(slot); L->hunger = 0; L->dead = 0; L->deaths = 0; L->inv[0] = 0;
    if (had) for (char *l = strtok(b, "\n"); l; l = strtok(NULL, "\n")) { if (!strncmp(l, "hp=", 3)) L->hp = atoi(l + 3); else if (!strncmp(l, "hunger=", 7)) L->hunger = atoi(l + 7); else if (!strncmp(l, "dead=", 5)) L->dead = atoi(l + 5); else if (!strncmp(l, "deaths=", 7)) L->deaths = atoi(l + 7); else if (!strncmp(l, "inv=", 4)) snprintf(L->inv, sizeof L->inv, "%s", l + 4); }
    if (!L->last) L->last = now_ms(); }
static void sk_life_save(int slot) { SkLife *L = &sk_life[slot]; char p[PATH_MAX], b[900]; sk_life_path(slot, p, sizeof p); snprintf(b, sizeof b, "hp=%d\nhunger=%d\ndead=%d\ndeaths=%d\ninv=%s\n", L->hp, L->hunger, L->dead, L->deaths, L->inv); write_if_changed(p, b, strlen(b)); }
static int sk_inv_count(SkLife *L, const char *item) { char b[400]; snprintf(b, sizeof b, "%s", L->inv); for (char *t = strtok(b, ","); t; t = strtok(NULL, ",")) { char *c = strchr(t, ':'); if (c && !strncmp(t, item, (size_t)(c - t)) && (size_t)(c - t) == strlen(item)) return atoi(c + 1); } return 0; }
static void sk_inv_set(SkLife *L, const char *item, int n) { char b[400], out[400] = ""; int found = 0; snprintf(b, sizeof b, "%s", L->inv); for (char *t = strtok(b, ","); t; t = strtok(NULL, ",")) { char *c = strchr(t, ':'); if (!c) continue; char k[24]; snprintf(k, sizeof k, "%.*s", (int)(c - t), t); int cnt = atoi(c + 1); if (!strcmp(k, item)) { cnt = n; found = 1; } if (cnt > 0) { size_t o = strlen(out); snprintf(out + o, sizeof out - o, "%s%s:%d", o ? "," : "", k, cnt); } }
    if (!found && n > 0) { size_t o = strlen(out); snprintf(out + o, sizeof out - o, "%s%s:%d", o ? "," : "", item, n); } snprintf(L->inv, sizeof L->inv, "%s", out); }
static void sk_drops_save(void) { char p[PATH_MAX], b[64 * 80 + 32], *w = b; spath(p, "drops.txt"); b[0] = 0; for (int i = 0; i < sk_ndrops; i++) { SkDrop *d = &sk_drops[i]; w += snprintf(w, sizeof b - (size_t)(w - b), "%d|%s|%d|%d|%d|%s|%d|%d|%s\n", d->id, rooms[d->room].id, d->x, d->y, d->n, d->item, 0, d->dmg, d->src); (void)d->owner; } write_if_changed(p, b, strlen(b)); }
static void sk_drops_load(void) { char p[PATH_MAX], b[64 * 80 + 32]; spath(p, "drops.txt"); read_file(p, b, sizeof b); sk_ndrops = 0; for (char *l = strtok(b, "\n"); l && sk_ndrops < 64; l = strtok(NULL, "\n")) { char *f[10]; int nf = split_bar(l, f, 10); if (nf < 6) continue; SkDrop *d = &sk_drops[sk_ndrops]; d->dmg = nf >= 8 ? atoi(f[7]) : 0; snprintf(d->src, sizeof d->src, "%s", nf >= 9 ? f[8] : ""); d->id = atoi(f[0]); d->room = room_idx(f[1]); d->x = atoi(f[2]); d->y = atoi(f[3]); d->n = atoi(f[4]); snprintf(d->item, sizeof d->item, "%s", f[5]); d->owner[0] = 0; if (d->room < 0 || d->n <= 0) continue; if (d->id >= sk_dropid) sk_dropid = d->id + 1; sk_ndrops++; } }
static void sk_life_log(const char *kind, int slot, const char *extra) { char b[200]; snprintf(b, sizeof b, "%s|%ld|%s|%s", kind, (long)time(NULL), sk_life_id(slot), extra); append_line("events.txt", b); }
static void sk_life_respawn_effect(int slot) {      /* the respawn: back on the safe place, full hp, hunger 0 (life.pdl respawn_*); what events/respawn calls */
    SkLife *L = &sk_life[slot]; char ex[100]; int rr = room_idx(sk_lc_room); if (rr < 0) rr = 0; sk_life_load(slot); L->hp = sk_life_mhp(slot); L->hunger = 0; L->dead = 0; L->last = now_ms();
    int ci = slot == 0 ? -1 : slot - 1; use_ent(ci); pet.room = rr; pet.x = sk_lc_rx; pet.y = sk_lc_ry; pet.tx = pet.ty = -1; pet_save(); use_ent(active);
    snprintf(ex, sizeof ex, "%s,%d,%d", sk_lc_room, sk_lc_rx, sk_lc_ry); sk_life_log("RESPAWN", slot, ex); sk_life_save(slot); }
static void sk_life_die_effect(int slot) {      /* the death itself: drop_pct of every stack falls where they stood; then the respawn page / effect by death_mode. What events/death calls */
    SkLife *L = &sk_life[slot]; sk_life_load(slot); Ent *e = sk_life_ent(slot); char b[400], ex[160], *sv = NULL; int dropped = 0; snprintf(b, sizeof b, "%s", L->inv);
    sk_drops_load(); for (char *t = strtok_r(b, ",", &sv); t; t = strtok_r(NULL, ",", &sv)) { char *c = strchr(t, ':'); if (!c) continue; char k[24]; snprintf(k, sizeof k, "%.*s", (int)(c - t), t); int cnt = atoi(c + 1), d = cnt * sk_lc_drop / 100; if (d <= 0 || sk_ndrops >= 64) continue;
        SkDrop *q = &sk_drops[sk_ndrops++]; q->id = sk_dropid++; q->room = e->room; q->x = e->x; q->y = e->y; q->n = d; snprintf(q->item, sizeof q->item, "%s", k); q->owner[0] = 0; q->dmg = 0; snprintf(q->src, sizeof q->src, "death"); dropped += d; }
    { char cp[400], *sv2 = NULL; snprintf(cp, sizeof cp, "%s", L->inv); for (char *t = strtok_r(cp, ",", &sv2); t; t = strtok_r(NULL, ",", &sv2)) { char *c = strchr(t, ':'); if (!c) continue; char k[24]; snprintf(k, sizeof k, "%.*s", (int)(c - t), t); int cnt = atoi(c + 1); sk_inv_set(L, k, cnt - cnt * sk_lc_drop / 100); } }
    sk_drops_save(); L->deaths++; L->hp = 0; L->dead = 1; snprintf(ex, sizeof ex, "%s,%d,%d|dropped %d (%d%%)|mode %s", rooms[e->room].id, e->x, e->y, dropped, sk_lc_drop, sk_lc_mode); sk_life_log("DEATH", slot, ex); sk_life_save(slot);
    if (!strcmp(sk_lc_mode, "respawn")) { char ap[PATH_MAX]; spath(ap, "life_actor.txt"); char ab[40]; snprintf(ab, sizeof ab, "%s\n", sk_life_id(slot)); write_if_changed(ap, ab, strlen(ab)); if (!run_event_x("respawn", "parallel", "")) sk_life_respawn_effect(slot); sk_life_load(slot); } }
static void sk_life_die(int slot) {      /* hp reached 0: the Death state, then the death page (RPG Maker's common event). No page = the effect runs directly. */
    SkLife *L = &sk_life[slot]; L->hp = 0; L->dead = 1; sk_life_save(slot); char ap[PATH_MAX], ab[40]; spath(ap, "life_actor.txt"); snprintf(ab, sizeof ab, "%s\n", sk_life_id(slot)); write_if_changed(ap, ab, strlen(ab));
    if (!run_event_x("death", "parallel", "")) sk_life_die_effect(slot); sk_life_load(slot); }
static int sk_life_tick(void) {      /* hunger grows, starving hurts, 0 hp dies. Returns 1 when anything changed */
    sk_life_cfg(); if (!sk_lc_on) return 0; int changed = 0; long now = now_ms();
    { static long lchk; static unsigned lsig = 1; if (now - lchk >= 1500) { lchk = now; unsigned sg = 7; for (int s = 0; s <= nparty; s++) { sk_life_load(s); sg = sg * 31 + (unsigned)sk_life[s].hp * 3 + (unsigned)sk_life[s].dead * 1009 + (unsigned)(sk_life[s].hunger / 10); } if (sg != lsig) { lsig = sg; changed = 1; } } }      /* a verb or an event page in another process changed a life file: tell the board (health line) */
    for (int s = 0; s <= nparty; s++) { SkLife *L = &sk_life[s]; static int loaded[NP + 1]; if (!loaded[s]) { sk_life_load(s); loaded[s] = 1; } if (L->dead) continue;
        if (sk_lc_hunger_ms > 0 && now - L->last >= sk_lc_hunger_ms) { L->last = now; sk_life_load(s); L->last = now; if (L->dead) continue;      /* re-read first: verbs and event pages write these files from other processes, a cached copy would overwrite their changes */
            if (L->hunger < 100) L->hunger++; else L->hp -= sk_lc_starve; sk_life_save(s); changed = 1; }
        if (L->hp <= 0) { sk_life_die(s); changed = 1; } }
    return changed; }
static const char *sk_life_bar_state(int slot) { if (slot < 0 || slot > nparty) return ""; static char loaded[NP + 1]; sk_life_cfg(); if (!loaded[slot]) { sk_life_load(slot); loaded[slot] = 1; } SkLife *L = &sk_life[slot]; int mh = sk_life_mhp(slot); if (L->dead || L->hp <= 0) return "ko"; return L->hp * 100 / mh < 30 ? "low" : "ok"; }      /* bar.txt 6th field: written when the desk exports, so a health change shows at the next export (the life tick asks for one) */
static void sk_life_ui(char *out, size_t cap) {      /* the bottom bar: per character a bar of block cells (green, red under 30 percent, grey when KO'd), vars life_N_name / _bar / _ok / _low / _ko */
    size_t o = 0; out[0] = 0; for (int s = 0; s <= nparty && s < 5; s++) { SkLife *L = &sk_life[s]; sk_life_load(s); int mh = sk_life_mhp(s), cells = L->hp <= 0 ? 0 : 1 + L->hp * 3 / mh; if (cells > 4) cells = 4; char bar[48] = ""; for (int c = 0; c < 4; c++) strcat(bar, c < cells ? "\xe2\x96\xac" : "\xc2\xb7"); int ko = L->dead || L->hp <= 0, low = !ko && L->hp * 100 / mh < 30; char nm[24]; snprintf(nm, sizeof nm, "%.1s", sk_life_id(s)); if (nm[0] >= 'a' && nm[0] <= 'z') nm[0] -= 32;
        o += (size_t)snprintf(out + o, cap - o, "life_%d_name=%s\nlife_%d_bar=%s %s\nlife_%d_ok=%s\nlife_%d_low=%s\nlife_%d_ko=%s\n", s, nm, s, nm, ko ? "KO" : bar, s, !ko && !low ? "1" : "", s, low ? "1" : "", s, ko ? "1" : ""); } }
static void sk_life_verb(int argc, char **argv, char *msg, size_t cap) {
    sk_life_cfg(); sk_drops_load(); const char *a = argc >= 2 ? argv[1] : "status"; static int loaded[NP + 1];
    for (int s = 0; s <= nparty; s++) if (!loaded[s]) { sk_life_load(s); loaded[s] = 1; }
    if (!strcmp(a, "status")) { int slot = argc >= 3 ? sk_life_slot(argv[2]) : -2; char b[500] = ""; size_t o = 0;
        for (int s = 0; s <= nparty; s++) { if (slot >= 0 && s != slot) continue; SkLife *L = &sk_life[s]; o += (size_t)snprintf(b + o, sizeof b - o, "%s hp %d/%d hunger %d%s deaths %d inv [%s]; ", sk_life_id(s), L->hp, sk_life_mhp(s), L->hunger, L->dead ? " DEAD" : "", L->deaths, L->inv); }
        snprintf(msg, cap, "%s drops %d, mode %s, drop %d%%", b, sk_ndrops, sk_lc_mode, sk_lc_drop); return; }
    if (!strcmp(a, "reset")) { for (int s = 0; s <= nparty; s++) { SkLife *L = &sk_life[s]; L->hp = sk_life_mhp(s); L->hunger = 0; L->dead = 0; L->deaths = 0; L->last = now_ms(); sk_life_save(s); } sk_ndrops = 0; sk_drops_save(); snprintf(msg, cap, "life reset: everyone alive, drops cleared"); return; }
    char actor[32] = ""; if (argc >= 3 && !strcmp(argv[2], "@actor")) { char ap[PATH_MAX]; spath(ap, "life_actor.txt"); read_file(ap, actor, sizeof actor); actor[strcspn(actor, "\r\n")] = 0; argv[2] = actor; }      /* @actor = the character the death / respawn page was played for */
    int slot = argc >= 3 ? sk_life_slot(argv[2]) : -2; if (slot < 0 && strcmp(a, "take")) { snprintf(msg, cap, "life: unknown character (use harold, ember, ...)"); return; }
    if (!strcmp(a, "hurt") && argc >= 4) { sk_life[slot].hp -= atoi(argv[3]); if (sk_life[slot].hp <= 0) sk_life_die(slot); else sk_life_save(slot); snprintf(msg, cap, "%s hp %d", sk_life_id(slot), sk_life[slot].hp); return; }
    if (!strcmp(a, "die")) { sk_life_die_effect(slot); snprintf(msg, cap, "%s died (dropped %d%% of the stacks)", sk_life_id(slot), sk_lc_drop); return; }
    if (!strcmp(a, "respawn")) { sk_life_respawn_effect(slot); snprintf(msg, cap, "%s is back on %s %d,%d", sk_life_id(slot), sk_lc_room, sk_lc_rx, sk_lc_ry); return; }
    if (!strcmp(a, "feed") && argc >= 4) { sk_life[slot].hunger -= atoi(argv[3]); if (sk_life[slot].hunger < 0) sk_life[slot].hunger = 0; sk_life_save(slot); snprintf(msg, cap, "%s hunger %d", sk_life_id(slot), sk_life[slot].hunger); return; }
    if (!strcmp(a, "set") && argc >= 5) { if (!strcmp(argv[3], "hunger")) sk_life[slot].hunger = atoi(argv[4]); else if (!strcmp(argv[3], "hp")) sk_life[slot].hp = atoi(argv[4]); sk_life_save(slot); snprintf(msg, cap, "%s %s %s", sk_life_id(slot), argv[3], argv[4]); return; }
    if (!strcmp(a, "give") && argc >= 4) { int n = argc >= 5 ? atoi(argv[4]) : 1; sk_inv_set(&sk_life[slot], argv[3], sk_inv_count(&sk_life[slot], argv[3]) + n); sk_life_save(slot); snprintf(msg, cap, "%s now has [%s]", sk_life_id(slot), sk_life[slot].inv); return; }
    if (!strcmp(a, "take") && argc >= 3) { int id = atoi(argv[2]), k = -1; for (int i = 0; i < sk_ndrops; i++) if (sk_drops[i].id == id) k = i; if (k < 0) { snprintf(msg, cap, "no drop #%d", id); return; }
        int ts = argc >= 4 ? sk_life_slot(argv[3]) : (ctl_idx() < 0 ? 0 : ctl_idx() + 1); if (ts < 0) { snprintf(msg, cap, "life take: unknown taker"); return; } Ent *e = sk_life_ent(ts); SkDrop *d = &sk_drops[k];
        if (sk_life[ts].dead) { snprintf(msg, cap, "the dead cannot take things"); return; }
        if (e->room != d->room || abs(e->x - d->x) + abs(e->y - d->y) > sk_lc_range) { snprintf(msg, cap, "too far from the drop (%s at %d,%d, range %d)", rooms[d->room].id, d->x, d->y, sk_lc_range); return; }
        { char ik[40]; snprintf(ik, sizeof ik, "%s%s", d->item, d->dmg > 0 ? "_damaged" : ""); sk_inv_set(&sk_life[ts], ik, sk_inv_count(&sk_life[ts], ik) + d->n); } sk_life_save(ts); { char ex[100]; snprintf(ex, sizeof ex, "#%d %s x%d", d->id, d->item, d->n); sk_life_log("TAKE", ts, ex); } snprintf(msg, cap, "%s took %s x%d", sk_life_id(ts), d->item, d->n); sk_drops[k] = sk_drops[--sk_ndrops]; sk_drops_save(); return; }
    snprintf(msg, cap, "usage: life status [id] | die <id|@actor> | respawn <id|@actor> | hurt <id> <n> | feed <id> <n> | give <id> <item> [n] | set <id> hunger|hp <n> | take <drop#> [id] | reset");
}
/* COLLISION (owner 2026-10-25; design BOOK 16.game/RPG-SPACE-COLLISION-AND-DESTRUCTION-DESIGN.md): the ship's move is tested against the land like RPG Maker's canPass (map limit first, in sk_helm_verb; then solid land here). A hit logs COLLISION, plays events/collision (trigger parallel, fallback = the effect directly), refuses the move when block=1, and costs every crew member a very small hp share (more at speed) plus a mark on the ship's own damage register state/ship_damage.txt. First slice: land = a square reach around the planet centre, touched at or below contact_alt (approximation, calibrate); per-cell solids and destruction are not built. */
static int sk_cl_on = 1, sk_cl_block = 1, sk_cl_contact = -7, sk_cl_ship = 12, sk_cl_land = 16, sk_cl_base = 5, sk_cl_spd = 5; static long sk_cl_at, sk_cl_last;
static void sk_coll_cfg(void) {
    long now = now_ms(); if (sk_cl_at && now - sk_cl_at < 2000) return; sk_cl_at = now; char p[PATH_MAX], ln[300]; char *f[4]; snprintf(p, sizeof p, "%s/collision.pdl", APP); FILE *fp = fopen(p, "r"); if (!fp) return;
    while (fgets(ln, sizeof ln, fp)) { if (ln[0] == '#' || split_bar(ln, f, 4) < 3 || strcmp(f[0], "COLL")) continue; const char *k = f[1], *v = f[2];
        if (!strcmp(k, "enabled")) sk_cl_on = atoi(v); else if (!strcmp(k, "block")) sk_cl_block = atoi(v); else if (!strcmp(k, "contact_alt")) sk_cl_contact = atoi(v); else if (!strcmp(k, "ship_size")) sk_cl_ship = atoi(v); else if (!strcmp(k, "land_size")) sk_cl_land = atoi(v); else if (!strcmp(k, "crew_base_pm")) sk_cl_base = atoi(v); else if (!strcmp(k, "crew_speed_pm")) sk_cl_spd = atoi(v); }
    fclose(fp); }
/* ENVIRONMENT (owner 2026-10-25, 4b/4c/7e): air per room as plain data. environment.pdl holds the constants, state/air.txt one row per room "room|o2|temp|breach". A hole (breach 1) lets the room's oxygen drain toward the outside value, a
 * room without a hole is topped up by the recycler toward its own level, a character in air under o2_ok loses a slice of max hp per tick through the same `life hurt` sink hunger and collisions use (so death, drops and respawn follow by themselves). */
typedef struct { char id[24]; int o2, temp, breach; } SkAir;
static SkAir sk_air[16]; static int sk_nair; static int sk_ev_on = 1, sk_ev_tick = 2000, sk_ev_ok = 16, sk_ev_pm = 30, sk_ev_scrub = 1, sk_ev_leak = 12, sk_ev_bspeed = 6, sk_ev_out_o2 = 0; static int sk_ev_cold = 5, sk_ev_hot = 45, sk_ev_cool = 6, sk_ev_warm = 2, sk_ev_tpm = 20, sk_ev_out_t = -150; static char sk_ev_broom[24] = "bridge"; static struct { char id[24]; int o2, temp; } sk_ev_base[16]; static int sk_ev_nbase; static long sk_ev_cfg_at, sk_ev_last;
static void sk_env_cfg(void) {
    long now = now_ms(); if (sk_ev_cfg_at && now - sk_ev_cfg_at < 2000) return; sk_ev_cfg_at = now; char p[PATH_MAX], ln[300], *f[8]; snprintf(p, sizeof p, "%s/environment.pdl", APP); FILE *fp = fopen(p, "r"); if (!fp) return; sk_ev_nbase = 0;
    while (fgets(ln, sizeof ln, fp)) { int n; if (ln[0] == '#' || (n = split_bar(ln, f, 8)) < 2) continue;
        if (!strcmp(f[0], "ENV") && n >= 3) { int v = atoi(f[2]); if (!strcmp(f[1], "enabled")) sk_ev_on = v; else if (!strcmp(f[1], "tick_ms")) sk_ev_tick = v > 100 ? v : 100; else if (!strcmp(f[1], "o2_ok")) sk_ev_ok = v; else if (!strcmp(f[1], "choke_pm")) sk_ev_pm = v; else if (!strcmp(f[1], "scrub_per_tick")) sk_ev_scrub = v; else if (!strcmp(f[1], "leak_pct")) sk_ev_leak = v; else if (!strcmp(f[1], "breach_speed")) sk_ev_bspeed = v; else if (!strcmp(f[1], "cold_c")) sk_ev_cold = v; else if (!strcmp(f[1], "hot_c")) sk_ev_hot = v; else if (!strcmp(f[1], "cool_pct")) sk_ev_cool = v; else if (!strcmp(f[1], "warm_per_tick")) sk_ev_warm = v; else if (!strcmp(f[1], "temp_pm")) sk_ev_tpm = v; else if (!strcmp(f[1], "breach_room")) snprintf(sk_ev_broom, sizeof sk_ev_broom, "%s", f[2]); }
        else if (!strcmp(f[0], "OUTSIDE") && n >= 2) { sk_ev_out_o2 = atoi(f[1]); if (n >= 3) sk_ev_out_t = atoi(f[2]); }
        else if (!strcmp(f[0], "ROOM") && n >= 4 && sk_ev_nbase < 16) { snprintf(sk_ev_base[sk_ev_nbase].id, sizeof sk_ev_base[0].id, "%s", f[1]); sk_ev_base[sk_ev_nbase].o2 = atoi(f[2]); sk_ev_base[sk_ev_nbase++].temp = atoi(f[3]); } }
    fclose(fp); }
static int sk_env_base(const char *id, int *temp) { for (int i = 0; i < sk_ev_nbase; i++) if (!strcmp(sk_ev_base[i].id, id)) { if (temp) *temp = sk_ev_base[i].temp; return sk_ev_base[i].o2; } if (temp) *temp = 20; return 21; }
static void sk_env_save(void) { char p[PATH_MAX], b[16 * 60 + 8], *w = b; spath(p, "air.txt"); b[0] = 0; for (int i = 0; i < sk_nair; i++) w += snprintf(w, sizeof b - (size_t)(w - b), "%s|%d|%d|%d\n", sk_air[i].id, sk_air[i].o2, sk_air[i].temp, sk_air[i].breach); write_if_changed(p, b, strlen(b)); }
static void sk_env_load(void) {      /* every room has a row; a room without one starts at its base level */
    sk_env_cfg(); char p[PATH_MAX], b[16 * 60 + 8], *f[5]; spath(p, "air.txt"); b[0] = 0; read_file(p, b, sizeof b); sk_nair = 0;
    for (char *l = strtok(b, "\n"); l && sk_nair < 16; l = strtok(NULL, "\n")) { if (split_bar(l, f, 5) < 4) continue; SkAir *a = &sk_air[sk_nair++]; snprintf(a->id, sizeof a->id, "%s", f[0]); a->o2 = atoi(f[1]); a->temp = atoi(f[2]); a->breach = atoi(f[3]); }
    int added = 0; for (int r = 0; r < nroom && r < 16; r++) { int have = 0; for (int i = 0; i < sk_nair; i++) if (!strcmp(sk_air[i].id, rooms[r].id)) have = 1; if (!have && sk_nair < 16) { SkAir *a = &sk_air[sk_nair++]; snprintf(a->id, sizeof a->id, "%s", rooms[r].id); a->o2 = sk_env_base(a->id, &a->temp); a->breach = 0; added = 1; } }
    if (added) sk_env_save(); }
static SkAir *sk_env_room(const char *id) { for (int i = 0; i < sk_nair; i++) if (!strcmp(sk_air[i].id, id)) return &sk_air[i]; return NULL; }
static void sk_env_breach(const char *room, int on) { sk_env_load(); SkAir *a = sk_env_room(room); if (!a || a->breach == on) return; a->breach = on; sk_env_save(); char rb[120]; snprintf(rb, sizeof rb, "%s|%ld|%s|o2 %d", on ? "BREACH" : "SEALED", (long)time(NULL), room, a->o2); append_line("events.txt", rb); }
static int sk_env_tick(void) {
    sk_env_cfg(); if (!sk_ev_on) return 0; long now = now_ms(); if (now - sk_ev_last < sk_ev_tick) return 0; sk_ev_last = now; sk_env_load(); int changed = 0;
    for (int i = 0; i < sk_nair; i++) { SkAir *a = &sk_air[i]; int wt, want = sk_env_base(a->id, &wt), o = a->o2, ot = a->temp;
        if (a->breach) { int d = (a->o2 - sk_ev_out_o2) * sk_ev_leak / 100; if (d < 1 && a->o2 > sk_ev_out_o2) d = 1; a->o2 -= d; int dt = (a->temp - sk_ev_out_t) * sk_ev_cool / 100; if (dt < 1 && a->temp > sk_ev_out_t) dt = 1; a->temp -= dt; }      /* a hole lets the heat out too */
        else { if (a->o2 < want) { a->o2 += sk_ev_scrub; if (a->o2 > want) a->o2 = want; } else if (a->o2 > want) a->o2 = want;
               if (a->temp < wt) { a->temp += sk_ev_warm; if (a->temp > wt) a->temp = wt; } else if (a->temp > wt) { a->temp -= sk_ev_warm; if (a->temp < wt) a->temp = wt; } }      /* the heating and the cooling of a sealed room move it back to its own temperature */
        if (a->o2 != o || a->temp != ot) changed = 1; }
    if (changed) sk_env_save();
    char pend[600] = ""; int np = 0;      /* who suffers this tick: state/env_pending.txt "slot|hp|kind|room|value", applied by events/environment (env apply) */
    for (int s = 0; s <= nparty; s++) { sk_life_load(s); if (sk_life[s].dead || sk_life[s].hp <= 0) continue; Ent *e = sk_life_ent(s); if (e->room < 0 || e->room >= nroom) continue; SkAir *a = sk_env_room(rooms[e->room].id); if (!a) continue;
        const char *kind = a->o2 < sk_ev_ok ? "CHOKE" : a->temp < sk_ev_cold ? "FREEZE" : a->temp > sk_ev_hot ? "SCORCH" : NULL; if (!kind) continue;
        int pm = a->o2 < sk_ev_ok ? sk_ev_pm : sk_ev_tpm, n = sk_life_mhp(s) * pm / 1000; if (n < 1) n = 1; char t[100]; snprintf(t, sizeof t, "%d|%d|%s|%s|%d\n", s, n, kind, a->id, a->o2 < sk_ev_ok ? a->o2 : a->temp); if (strlen(pend) + strlen(t) < sizeof pend) { strcat(pend, t); np++; } }
    if (np) { char pp[PATH_MAX]; spath(pp, "env_pending.txt"); write_if_changed(pp, pend, strlen(pend)); if (!run_event_x("environment", "parallel", "")) { char *av[2] = { "env", "apply" }; char m2[200]; sk_env_verb(2, av, m2, sizeof m2); } changed = 1; }      /* no page = the C fallback applies it at once */
    return changed; }
static void sk_env_ui(char *out, size_t cap) {      /* vars env_o2 / env_temp / env_breach: the air in the room of the controlled character (the bar can show them) */
    sk_env_load(); int sh = ctl_idx() < 0 ? 0 : ctl_idx() + 1; Ent *e = sk_life_ent(sh); SkAir *a = e->room >= 0 && e->room < nroom ? sk_env_room(rooms[e->room].id) : NULL; int o2 = a ? a->o2 : 0, tc = a ? a->temp : 0, br = a ? a->breach : 0, warn = br || o2 < sk_ev_ok || tc < sk_ev_cold || tc > sk_ev_hot;
    snprintf(out, cap, "env_o2=%d\nenv_temp=%d\nenv_breach=%d\nenv_label=Air %d%% %dC%s\nenv_ok=%s\nenv_warn=%s\n", o2, tc, br, o2, tc, br ? " HOLE" : "", warn ? "" : "1", warn ? "1" : ""); }
static void sk_env_verb(int argc, char **argv, char *msg, size_t cap) {      /* env status | breach <room> [on|off] | set <room> o2 <n> */
    sk_env_load(); const char *a = argc >= 2 ? argv[1] : "status";
    if (!strcmp(a, "status")) { char b[400] = ""; for (int i = 0; i < sk_nair; i++) { char t[80]; snprintf(t, sizeof t, "%s o2 %d%% %dC%s; ", sk_air[i].id, sk_air[i].o2, sk_air[i].temp, sk_air[i].breach ? " HOLE" : ""); strncat(b, t, sizeof b - strlen(b) - 1); } snprintf(msg, cap, "air: %s", b); return; }
    if (!strcmp(a, "apply")) {      /* what events/environment calls: hurt everyone listed in state/env_pending.txt and log CHOKE / FREEZE / SCORCH */
        char p[PATH_MAX], b[600], *f[6], *sv = NULL; spath(p, "env_pending.txt"); b[0] = 0; read_file(p, b, sizeof b); write_if_changed(p, "", 0); int cnt = 0;
        for (char *l = strtok_r(b, "\n", &sv); l; l = strtok_r(NULL, "\n", &sv)) { if (split_bar(l, f, 6) < 5) continue; int slot = atoi(f[0]); if (slot < 0 || slot > nparty) continue; char *hv[4] = { "life", "hurt", (char *)sk_life_id(slot), f[1] }; char m2[160]; sk_life_verb(4, hv, m2, sizeof m2);
            char rb[140]; snprintf(rb, sizeof rb, "%s|%ld|%s|%s %s %s|-%s hp", f[2], (long)time(NULL), sk_life_id(slot), f[3], !strcmp(f[2], "CHOKE") ? "o2" : "temp", f[4], f[1]); append_line("events.txt", rb); cnt++; }
        snprintf(msg, cap, "environment: %d hurt", cnt); return; }
    if (!strcmp(a, "breach") && argc >= 3) { int on = !(argc >= 4 && !strcmp(argv[3], "off")); if (!sk_env_room(argv[2])) { snprintf(msg, cap, "env: no room %s", argv[2]); return; } sk_env_breach(argv[2], on); snprintf(msg, cap, "%s %s", argv[2], on ? "has a hole in the hull" : "is sealed"); return; }
    if (!strcmp(a, "set") && argc >= 5 && (!strcmp(argv[3], "o2") || !strcmp(argv[3], "temp"))) { SkAir *r = sk_env_room(argv[2]); if (!r) { snprintf(msg, cap, "env: no room %s", argv[2]); return; } if (argv[3][0] == 'o') r->o2 = atoi(argv[4]); else r->temp = atoi(argv[4]); sk_env_save(); snprintf(msg, cap, "%s o2 %d%% %dC", r->id, r->o2, r->temp); return; }
    snprintf(msg, cap, "usage: env status | breach <room> [on|off] | set <room> o2|temp <n> | apply"); }
static void sk_coll_effect(int speed, char *msg, size_t cap) {      /* what events/collision calls: slight crew damage, more at speed, and the ship's own damage register */
    sk_coll_cfg(); int hurt = 0; for (int s = 0; s <= nparty; s++) { int mh = sk_life_mhp(s), n = mh * (sk_cl_base + speed * sk_cl_spd) / 1000; if (n < 1) n = 1; char nb[16], m2[160]; snprintf(nb, sizeof nb, "%d", n); char *hv[4] = { "life", "hurt", (char *)sk_life_id(s), nb }; sk_life_verb(4, hv, m2, sizeof m2); hurt += n; }
    { char p[PATH_MAX], b[120]; int hits = 0, dmg = 0; spath(p, "ship_damage.txt"); read_file(p, b, sizeof b); sscanf(b, "hits=%d dmg=%d", &hits, &dmg); hits++; dmg += speed; snprintf(b, sizeof b, "hits=%d dmg=%d last=%ld\n", hits, dmg, (long)time(NULL)); write_if_changed(p, b, strlen(b)); }
    sk_env_cfg(); if (speed >= sk_ev_bspeed) sk_env_breach(sk_ev_broom, 1);      /* a hard hit punctures the hull: the room leaks until `env breach <room> off` */
    if (msg) snprintf(msg, cap, "collision at speed %d: crew lost %d hp in all", speed, hurt); }
static int sk_coll_check(int nx, int ny, int nz, char *msg, size_t cap) {      /* 1 = the move is refused */
    sk_coll_cfg(); long now = now_ms(); long dt = now - sk_cl_last; sk_cl_last = now; if (!sk_cl_on) return 0;
    if (nz > sk_cl_contact || !(nx < sk_cl_land && nx + sk_cl_ship > 0 && ny < sk_cl_land && ny + sk_cl_ship > 0)) return 0;      /* the hull's footprint [d, d+ship) against the planet page [0, land) on both axes */
    int speed = dt < 300 ? 4 : dt < 700 ? 3 : dt < 1500 ? 2 : 1; char p[PATH_MAX], b[24], rb[100]; spath(p, "coll_speed.txt"); snprintf(b, sizeof b, "%d\n", speed); write_if_changed(p, b, strlen(b));
    snprintf(rb, sizeof rb, "COLLISION|%ld|ship|%d,%d,%d|speed %d|%s", (long)time(NULL), nx, ny, nz, speed, sk_cl_block ? "blocked" : "passed"); append_line("events.txt", rb);
    char m2[160] = ""; if (!run_event_x("collision", "parallel", "")) sk_coll_effect(speed, m2, sizeof m2);
    snprintf(msg, cap, "%s the land at %d,%d altitude %d (speed %d)", sk_cl_block ? "the hull hits" : "the hull scrapes", nx, ny, nz, speed); return sk_cl_block; }
static void sk_coll_verb(int argc, char **argv, char *msg, size_t cap) {      /* collision hit [speed]: what the collision page runs; collision status */
    sk_coll_cfg(); if (argc >= 2 && !strcmp(argv[1], "hit")) { char p[PATH_MAX], b[16] = ""; spath(p, "coll_speed.txt"); read_file(p, b, sizeof b); int sp = argc >= 3 ? atoi(argv[2]) : atoi(b); if (sp < 1) sp = 1; if (sp > 4) sp = 4; sk_coll_effect(sp, msg, cap); return; }
    { char p[PATH_MAX], b[100] = ""; spath(p, "ship_damage.txt"); read_file(p, b, sizeof b); b[strcspn(b, "\n")] = 0; snprintf(msg, cap, "collision %s, block %d, contact_alt %d, ship %d land %d; ship %s", sk_cl_on ? "on" : "off", sk_cl_block, sk_cl_contact, sk_cl_ship, sk_cl_land, b[0] ? b : "undamaged"); } }
/* WORLD OBJECTS: mining, destruction, remnants and the ship hold (owner 2026-10-25; design 16.game/RPG-SPACE-COLLISION-AND-DESTRUCTION-DESIGN.md 3, 4f, 4g). Infrastructure: everything is data (blocks.pdl, hold.pdl) + event pages (events/destroyed); the C only does what a page calls. mine adds tool power to a per-cell progress (state/damage.txt); at hardness the cell is destroyed: events/destroyed (parallel, fallback = the effect) runs `destroy last`, which turns the BLOCK row into remnants (state/drops.txt, maybe damaged) and logs DESTROYED. hold = state/hold.txt, items and the entities inside (position relative to the ship). NOT built: finding the block under a cell from the engine (kind is passed), drawing remnants in the scene, the beam key. */
typedef struct { char kind[24], drop[24]; int hard, cnt, dpct; } SkBlk; typedef struct { char name[16]; int power; } SkTool; typedef struct { char kind[8], id[24]; int n, x, y; } SkHold; typedef struct { char kind[24], item[24]; int n; } SkBuild;
static char sk_node_seed[2400]; static int sk_pk_range = 2, sk_pk_ms = 600, sk_pk_size = 40, sk_pk_spin = 220; static SkBlk sk_blk[24]; static int sk_nblk; static SkBuild sk_bld[12]; static int sk_nbld; static SkTool sk_tool[8]; static int sk_ntool; static SkHold sk_hold[64]; static int sk_nhold, sk_hold_cap = 40, sk_hold_wreck = 100; static long sk_wd_at;
static void sk_wd_cfg(void) {
    long now = now_ms(); if (sk_wd_at && now - sk_wd_at < 2000) return; sk_wd_at = now; char p[PATH_MAX], ln[400]; char *f[10]; int n;
    snprintf(p, sizeof p, "%s/blocks.pdl", APP); FILE *fp = fopen(p, "r"); if (fp) { sk_nblk = sk_ntool = sk_nbld = 0; sk_node_seed[0] = 0;
        while (fgets(ln, sizeof ln, fp)) { if (ln[0] == '#' || (n = split_bar(ln, f, 10)) < 3) continue;
            if (!strcmp(f[0], "BLOCK") && n >= 6 && sk_nblk < 24) { SkBlk *b = &sk_blk[sk_nblk++]; snprintf(b->kind, sizeof b->kind, "%s", f[1]); b->hard = atoi(f[2]); snprintf(b->drop, sizeof b->drop, "%s", f[3]); b->cnt = atoi(f[4]); b->dpct = atoi(f[5]); }
            else if (!strcmp(f[0], "NODE") && n >= 6 && strlen(sk_node_seed) < sizeof sk_node_seed - 80) { char t[100]; snprintf(t, sizeof t, "%s|%s|%s|%s|%s\n", f[1], f[2], f[3], f[4], f[5]); strcat(sk_node_seed, t); }
            else if (!strcmp(f[0], "PICKUP")) { if (!strcmp(f[1], "range")) sk_pk_range = atoi(f[2]); else if (!strcmp(f[1], "ms")) sk_pk_ms = atoi(f[2]); else if (!strcmp(f[1], "size")) sk_pk_size = atoi(f[2]); else if (!strcmp(f[1], "spin_ms")) sk_pk_spin = atoi(f[2]); }
            else if (!strcmp(f[0], "BUILD") && n >= 4 && sk_nbld < 12) { SkBuild *q = &sk_bld[sk_nbld++]; snprintf(q->kind, sizeof q->kind, "%s", f[1]); snprintf(q->item, sizeof q->item, "%s", f[2]); q->n = atoi(f[3]); }      /* BUILD | kind | item | count: what placing one block costs */
            else if (!strcmp(f[0], "TOOL") && sk_ntool < 8) { snprintf(sk_tool[sk_ntool].name, sizeof sk_tool[0].name, "%s", f[1]); sk_tool[sk_ntool++].power = atoi(f[2]); } }
        fclose(fp); }
    snprintf(p, sizeof p, "%s/hold.pdl", APP); fp = fopen(p, "r"); if (fp) { while (fgets(ln, sizeof ln, fp)) { if (ln[0] == '#' || split_bar(ln, f, 10) < 3 || strcmp(f[0], "HOLD")) continue; if (!strcmp(f[1], "capacity")) sk_hold_cap = atoi(f[2]); else if (!strcmp(f[1], "wreck_drop")) sk_hold_wreck = atoi(f[2]); } fclose(fp); } }
static SkBlk *sk_blk_find(const char *k) { for (int i = 0; i < sk_nblk; i++) if (!strcmp(sk_blk[i].kind, k)) return &sk_blk[i]; return NULL; }
static void sk_hold_load(void) { char p[PATH_MAX], b[64 * 70 + 32]; spath(p, "hold.txt"); read_file(p, b, sizeof b); sk_nhold = 0; for (char *l = strtok(b, "\n"); l && sk_nhold < 64; l = strtok(NULL, "\n")) { char *f[8]; int nf = split_bar(l, f, 8); if (nf < 3) continue; SkHold *h = &sk_hold[sk_nhold++]; snprintf(h->kind, sizeof h->kind, "%s", f[0]); snprintf(h->id, sizeof h->id, "%s", f[1]); h->n = atoi(f[2]); h->x = nf >= 4 ? atoi(f[3]) : 0; h->y = nf >= 5 ? atoi(f[4]) : 0; } }
static void sk_hold_save(void) { char p[PATH_MAX], b[64 * 70 + 32], *w = b; spath(p, "hold.txt"); b[0] = 0; for (int i = 0; i < sk_nhold; i++) { SkHold *h = &sk_hold[i]; w += snprintf(w, sizeof b - (size_t)(w - b), "%s|%s|%d|%d|%d\n", h->kind, h->id, h->n, h->x, h->y); } write_if_changed(p, b, strlen(b)); }
static SkHold *sk_hold_find(const char *kind, const char *id) { for (int i = 0; i < sk_nhold; i++) if (!strcmp(sk_hold[i].kind, kind) && !strcmp(sk_hold[i].id, id)) return &sk_hold[i]; return NULL; }
typedef struct { int room, x, y, amount; char kind[24]; } SkNode; static SkNode sk_nodes[32]; static int sk_nnodes;      /* resource nodes: ore patches you mine (blocks.pdl NODE rows seed state/nodes.txt; amount -1 = endless, 0 = used up and gone) */
static void sk_nodes_save(void) { char p[PATH_MAX], b[32 * 70 + 8], *w = b; spath(p, "nodes.txt"); b[0] = 0; for (int i = 0; i < sk_nnodes; i++) w += snprintf(w, sizeof b - (size_t)(w - b), "%s|%d|%d|%s|%d\n", rooms[sk_nodes[i].room].id, sk_nodes[i].x, sk_nodes[i].y, sk_nodes[i].kind, sk_nodes[i].amount); write_if_changed(p, b, strlen(b)); }
static void sk_nodes_load(void) {
    char p[PATH_MAX], b[32 * 70 + 8]; spath(p, "nodes.txt"); int have = read_file(p, b, sizeof b) > 0; if (!have) { sk_wd_cfg(); snprintf(b, sizeof b, "%s", sk_node_seed); have = 2; }
    sk_nnodes = 0; for (char *l = strtok(b, "\n"); l && sk_nnodes < 32; l = strtok(NULL, "\n")) { char *f[6]; if (split_bar(l, f, 6) < 5) continue; SkNode *nd = &sk_nodes[sk_nnodes]; nd->room = room_idx(f[0]); nd->x = atoi(f[1]); nd->y = atoi(f[2]); snprintf(nd->kind, sizeof nd->kind, "%s", f[3]); nd->amount = atoi(f[4]); if (nd->room >= 0 && nd->amount != 0) sk_nnodes++; }
    if (have == 2) sk_nodes_save(); }
static SkNode *sk_node_at(int room, int x, int y) { for (int i = 0; i < sk_nnodes; i++) if (sk_nodes[i].room == room && sk_nodes[i].x == x && sk_nodes[i].y == y) return &sk_nodes[i]; return NULL; }
static int sk_node_solid(int room, int x, int y) {      /* cached for 400 ms: the walk checks run for every pet and every path step */
    static long at; static int n, r[32], px[32], py[32]; long now = now_ms(); if (!at || now - at > 400) { at = now; sk_nodes_load(); n = sk_nnodes; for (int i = 0; i < n; i++) { r[i] = sk_nodes[i].room; px[i] = sk_nodes[i].x; py[i] = sk_nodes[i].y; } }
    for (int i = 0; i < n; i++) if (r[i] == room && px[i] == x && py[i] == y) return 1; return 0; }
/* SPACE OBJECTS (owner 2026-10-25, 4e/4f/7f): asteroids are resource nodes of the space room (so mining, remnants, solidity and drawing are the ones that exist), another ship is a registered entity with crew and cargo, both seeded from space.pdl once. */
typedef struct { char id[24], label[32], crew[120], cargo[80]; int gx, gy, wx, wy; } SkCraft;      /* wx, wy: the cell on the PLANET MAP (-1 = none), the same space as the ship and the land (owner 2026-10-26: 'i expected to see them on same map as planet/ship') */
static SkCraft sk_craft[6]; static int sk_ncraft;
static void sk_craft_save(void) { char p[PATH_MAX], b[6 * 300], *w = b; spath(p, "craft.txt"); b[0] = 0; for (int i = 0; i < sk_ncraft; i++) w += snprintf(w, sizeof b - (size_t)(w - b), "%s|%d|%d|%s|%s|%s|%d|%d\n", sk_craft[i].id, sk_craft[i].gx, sk_craft[i].gy, sk_craft[i].label, sk_craft[i].crew, sk_craft[i].cargo, sk_craft[i].wx, sk_craft[i].wy); write_if_changed(p, b, strlen(b)); }
static void sk_craft_load(void) {
    static long at; long now = now_ms(); if (at && now - at < 700) return; at = now; char p[PATH_MAX], b[6 * 300], sd[800], *f[8]; spath(p, "craft.txt"); b[0] = 0; read_file(p, b, sizeof b); sk_ncraft = 0;
    for (char *l = strtok(b, "\n"); l && sk_ncraft < 6; l = strtok(NULL, "\n")) { int nf = split_bar(l, f, 8); if (nf < 6) continue; SkCraft *c = &sk_craft[sk_ncraft++]; c->wx = nf >= 8 ? atoi(f[6]) : -1; c->wy = nf >= 8 ? atoi(f[7]) : -1; snprintf(c->id, sizeof c->id, "%s", f[0]); c->gx = atoi(f[1]); c->gy = atoi(f[2]); snprintf(c->label, sizeof c->label, "%s", f[3]); snprintf(c->crew, sizeof c->crew, "%s", f[4]); snprintf(c->cargo, sizeof c->cargo, "%s", f[5]); }
    spath(p, "space_seeded.txt"); sd[0] = 0; read_file(p, sd, sizeof sd); int added = 0; char pd[PATH_MAX], ln[400]; snprintf(pd, sizeof pd, "%s/space.pdl", APP); FILE *fp = fopen(pd, "r"); if (!fp) return;
    while (fgets(ln, sizeof ln, fp)) { int n; if (ln[0] == '#' || (n = split_bar(ln, f, 8)) < 6) continue; char kid[40]; snprintf(kid, sizeof kid, "%s%s", f[0][0] == 'W' ? "w_" : "", f[1]); char key[48]; snprintf(key, sizeof key, "\n%s\n", kid); char hay[820]; snprintf(hay, sizeof hay, "\n%s", sd); if (strstr(hay, key)) continue;
        if (!strcmp(f[0], "ROCK")) { sk_nodes_load(); if (sk_nnodes < 32) { SkNode *nd = &sk_nodes[sk_nnodes++]; nd->room = SK_SPACE; nd->x = 2 + atoi(f[2]) * 2; nd->y = 2 + atoi(f[3]) * 2; nd->amount = atoi(f[5]); snprintf(nd->kind, sizeof nd->kind, "%s", f[4]); sk_nodes_save(); added = 1; } else continue; }
        else if (!strcmp(f[0], "CRAFT") && n >= 7 && sk_ncraft < 6) { SkCraft *c = &sk_craft[sk_ncraft++]; snprintf(c->id, sizeof c->id, "%s", f[1]); c->gx = atoi(f[2]); c->gy = atoi(f[3]); snprintf(c->label, sizeof c->label, "%s", f[4]); snprintf(c->crew, sizeof c->crew, "%s", f[5]); snprintf(c->cargo, sizeof c->cargo, "%s", f[6]); sk_craft_save(); added = 1; }
        else if (!strcmp(f[0], "WROCK") && n >= 7) { sk_nodes_load(); if (sk_nnodes < 32) { SkNode *nd = &sk_nodes[sk_nnodes++]; nd->room = SK_SHIP; nd->x = atoi(f[3]); nd->y = atoi(f[4]); nd->amount = atoi(f[6]); snprintf(nd->kind, sizeof nd->kind, "%s", f[5]); sk_nodes_save(); char pp[PATH_MAX]; spath(pp, "pworld_planet.txt"); char pb[40]; snprintf(pb, sizeof pb, "%s\n", f[2]); write_if_changed(pp, pb, strlen(pb)); added = 1; } else continue; }      /* an asteroid over the PLANET MAP: a node of the planet page room (so mining, damage and remnants are the ones that exist), at world cell (wx, wy) of planet f[2] */
        else if (!strcmp(f[0], "WCRAFT")) { int hit = 0; for (int i = 0; i < sk_ncraft; i++) if (!strcmp(sk_craft[i].id, f[1])) { sk_craft[i].wx = atoi(f[3]); sk_craft[i].wy = atoi(f[4]); hit = 1; } if (!hit) continue; sk_craft_save(); char pp[PATH_MAX]; spath(pp, "pworld_planet.txt"); char pb[40]; snprintf(pb, sizeof pb, "%s\n", f[2]); write_if_changed(pp, pb, strlen(pb)); added = 1; }
        else continue;
        strncat(sd, kid, sizeof sd - strlen(sd) - 2); strcat(sd, "\n"); }
    fclose(fp); if (added) { spath(p, "space_seeded.txt"); write_if_changed(p, sd, strlen(sd)); } }
static int sk_craft_at(int gx, int gy) { sk_craft_load(); for (int i = 0; i < sk_ncraft; i++) if (sk_craft[i].gx == gx && sk_craft[i].gy == gy) return i; return -1; }
static int sk_space_block(int gx, int gy, char *msg, size_t cap) {      /* 1 = the ship cannot fly onto this grid cell */
    sk_craft_load(); int ci = sk_craft_at(gx, gy); if (ci >= 0) { snprintf(msg, cap, "%s is there: the helm holds off (try `space hail %s`)", sk_craft[ci].label, sk_craft[ci].id); return 1; }
    if (sk_node_solid(SK_SPACE, 2 + gx * 2, 2 + gy * 2)) { snprintf(msg, cap, "an asteroid is in the way at %d,%d: mine it with m from a neighbouring cell", gx, gy); return 1; } return 0; }
static void sk_space_draw(int g) {      /* asteroids: a grey rock cell under their block; other ships: a one-cell teal hull (rows of the space desk, no engine piece) */
    sk_craft_load(); sk_nodes_load(); for (int i = 0; i < sk_nnodes; i++) if (sk_nodes[i].room == SK_SPACE) { int x = sk_nodes[i].x, y = sk_nodes[i].y; if (x < 0 || y < 0 || x >= SK_W || y >= SK_H) continue; sk_solid(g, x, y, 'G', 110, 105, 100, 0, 0); sk_cdep[g][y][x] = 3; }
    for (int i = 0; i < sk_ncraft; i++) { int x = 2 + sk_craft[i].gx * 2, y = 2 + sk_craft[i].gy * 2; if (x < 0 || y < 0 || x >= SK_W || y >= SK_H) continue; sk_cdep[g][y][x] = 0; sk_solid(g, x, y, 'W', 95, 190, 190, 0, 1); sk_solid(g, x, y, 'W', 60, 140, 150, 0, 0); } }
/* OBJECTS ON THE PLANET MAP (owner 2026-10-26: "i expected to see them on same map as planet/ship cata prime"). In direct flight the planet is the land overlay: the engine fills the empty cells of the bridge desk from the planet page (room SK_SHIP), shifted by (state/direct.txt). So an asteroid or a ship painted ON that page slides with the planet for free, and is a node / craft with a world cell (wx, wy) = a cell of the page. The hull covers desk cells [SOX, SOX+SSZ); a desk cell is world cell minus (ox, oy), so only a ring around the hull shows and flying brings other objects into it. space.pdl WROCK / WCRAFT rows seed them once; tuning.pdl obj_reach / obj_top. */
static int sk_world_here(void) { if (sk_pl_cur < 0) return 0; char p[PATH_MAX], b[40] = ""; spath(p, "pworld_planet.txt"); read_file(p, b, sizeof b); b[strcspn(b, "\n")] = 0; return b[0] && !strcmp(b, sk_pl[sk_pl_cur].id); }
static void sk_world_draw(void) {      /* called with the page rooms painted, before sk_build_ship copies them */
    sk_craft_load(); sk_nodes_load(); if (!sk_world_here() || tune("obj_draw", 1)) return;      /* obj_draw=1 (default): asteroids and the other ship are VOXEL PIECES (sk_veh_write); 0 = the old flat tiles */
    for (int i = 0; i < sk_nnodes; i++) if (sk_nodes[i].room == SK_SHIP) { int x = sk_nodes[i].x, y = sk_nodes[i].y; if (x < 0 || y < 0 || x >= SK_W || y >= SK_H) continue; int cr = 120, cg = 115, cb = 108; if (!strcmp(sk_nodes[i].kind, "ore")) { cr = 205; cg = 120; cb = 60; } else if (!strcmp(sk_nodes[i].kind, "crystal")) { cr = 110; cg = 225; cb = 235; }      /* grey rock, orange ore, cyan crystal */
        sk_solid(SK_SHIP, x, y, 'r', cr, cg, cb, 0, 1); sk_solid(SK_SHIP, x, y, 'r', cr, cg, cb, 0, 0); sk_cdep[SK_SHIP][y][x] = 9; }      /* BOTH layers: the engine's land overlay (maker_load) copies only the FLOOR tile of the page (walls are dropped, rocks and trees flattened), so a wall-only rock vanished from the planet under the ship */
    for (int i = 0; i < sk_ncraft; i++) { int x = sk_craft[i].wx, y = sk_craft[i].wy; if (x < 0 || y < 0 || x >= SK_W || y >= SK_H) continue; sk_solid(SK_SHIP, x, y, 'W', 95, 190, 190, 0, 1); sk_cdep[SK_SHIP][y][x] = 9; } }
/* WORLD OBJECTS AS VOXEL PIECES (owner 2026-10-26: 'asteroid is not a flat tile, it should be a random nugget of 5-11 or so voxels ... same goes for the other ship, a smaller version of the current ship').
 * The engine draws pieces/veh_1..9 (state.txt + model.txt, bv_render_3d.c load_vehicle) as one box with a dense voxel model; nothing ever wrote them before, which is why they "never drew".
 * Slots 1.. = the asteroids of the planet map (nodes in room SK_SHIP) then the craft that have a world cell. Desk cell = world cell - the direct-flight shift, like the land. Fallback: obj_draw=0. */
#define SK_VEH_MAXV 600
typedef struct { int x, y, z, r, g, b; } SkVox;
static unsigned sk_hash(const char *s) { unsigned h = 2166136261u; for (; *s; s++) h = (h ^ (unsigned char)*s) * 16777619u; return h | 1u; }
static unsigned sk_rng(unsigned *s) { *s ^= *s << 13; *s ^= *s >> 17; *s ^= *s << 5; return *s; }
static int sk_nugget(const char *kind, int wx, int wy, SkVox *v, int dim[3]) {      /* a random face-connected blob of rock_vox_min..rock_vox_max voxels, the same one every time for the same asteroid (seeded by kind and cell) */
    char key[64]; snprintf(key, sizeof key, "%s|%d|%d", kind, wx, wy); unsigned s = sk_hash(key);
    int lo = tune("rock_vox_min", 5), hi = tune("rock_vox_max", 11); if (lo < 1) lo = 1; if (hi < lo) hi = lo; if (hi > 40) hi = 40;
    int n = lo + (int)(sk_rng(&s) % (unsigned)(hi - lo + 1));
    enum { GX = 5, GY = 4, GZ = 5 }; static signed char g[GX][GY][GZ]; memset(g, 0, sizeof g);
    int px[64], py[64], pz[64], c = 1; px[0] = 2; py[0] = 0; pz[0] = 2; g[2][0][2] = 1;      /* starts on the ground layer so the nugget rests on the planet */
    for (int guard = 0; c < n && guard < 4000; guard++) {
        int k = (int)(sk_rng(&s) % (unsigned)c), d = (int)(sk_rng(&s) % 6u);
        int nx = px[k] + (d == 0) - (d == 1), ny = py[k] + (d == 2) - (d == 3), nz = pz[k] + (d == 4) - (d == 5);
        if (nx < 0 || ny < 0 || nz < 0 || nx >= GX || ny >= GY || nz >= GZ || g[nx][ny][nz]) continue;
        g[nx][ny][nz] = 1; px[c] = nx; py[c] = ny; pz[c] = nz; c++; }
    int mn[3] = {99, 99, 99}, mx[3] = {-1, -1, -1};
    for (int i = 0; i < c; i++) { int q[3] = {px[i], py[i], pz[i]}; for (int a = 0; a < 3; a++) { if (q[a] < mn[a]) mn[a] = q[a]; if (q[a] > mx[a]) mx[a] = q[a]; } }
    for (int a = 0; a < 3; a++) dim[a] = mx[a] - mn[a] + 1;
    int br = 120, bg = 115, bb = 108; if (!strcmp(kind, "ore")) { br = 150; bg = 108; bb = 88; } else if (!strcmp(kind, "crystal")) { br = 110; bg = 215; bb = 230; }      /* grey rock, rust ore, cyan crystal */
    for (int i = 0; i < c; i++) {
        int j = (int)(sk_rng(&s) % 31u) - 15, r = br + j, gg = bg + j, b = bb + j;
        if (!strcmp(kind, "ore") && sk_rng(&s) % 3u == 0) { r = 225; gg = 130; b = 55; }      /* orange flecks: the ore you mine */
        v[i] = (SkVox){px[i] - mn[0], py[i] - mn[1], pz[i] - mn[2], r < 0 ? 0 : r > 255 ? 255 : r, gg < 0 ? 0 : gg > 255 ? 255 : gg, b < 0 ? 0 : b > 255 ? 255 : b}; }
    return c; }
static int sk_skiff_model(SkVox *v, int cap, int dim[3]) {      /* the player's own hull model (pieces/ship_01/model.txt) shrunk by skiff_div (default 2): 'a smaller version of the current ship' */
    char p[PATH_MAX], line[160]; snprintf(p, sizeof p, "%s/pieces/ship_01/model.txt", PCHQ); FILE *f = fopen(p, "r"); if (!f) return 0;
    int div = tune("skiff_div", 2); if (div < 1) div = 1; static signed char seen[32][32][32]; memset(seen, 0, sizeof seen); int n = 0; dim[0] = dim[1] = dim[2] = 0;
    while (fgets(line, sizeof line, f)) { int a, b, c, r, g, bl; if (sscanf(line, "%d %d %d %d %d %d", &a, &b, &c, &r, &g, &bl) != 6) continue; a /= div; b /= div; c /= div;
        if (a < 0 || b < 0 || c < 0 || a >= 32 || b >= 32 || c >= 32 || seen[a][b][c] || n >= cap) continue; seen[a][b][c] = 1; v[n++] = (SkVox){a, b, c, r, g, bl};
        if (a + 1 > dim[0]) dim[0] = a + 1; if (b + 1 > dim[1]) dim[1] = b + 1; if (c + 1 > dim[2]) dim[2] = c + 1; }
    fclose(f); return n; }
/* THE SAME TILES AS THE SHIP (owner 2026-10-26: 'u can use the same tiles for the asteroids, skiff, as scifi'). The engine's voxel pieces carry one RGB per voxel, so the colour is taken from real pixels of the ship's own sheet SF_Inside_A4 (the 24 x 24 tiles of sk_ship_room): the skiff's top faces from the bridge's green hex floor (8,11) and every other voxel from the hull wall (6,2); an asteroid from the engine room's diamond plate (8,6), tinted per kind so ore and crystal stay recognisable. A missing sheet leaves the flat colours. */
static const unsigned char *sk_tile_px(int col, int row) {
    static unsigned char buf[4][XT * XT * 4]; static int kc[4], kr[4], n; for (int i = 0; i < n; i++) if (kc[i] == col && kr[i] == row) return buf[i];
    Sheet *a4 = n < 4 ? sheet("tilesets", "SF_Inside_A4") : NULL; if (!a4) return NULL; tile24(a4, col * S48, row * S48, buf[n]); kc[n] = col; kr[n] = row; return buf[n++]; }
static void sk_retile(SkVox *v, int n, const int dim[3], int tc, int tr, int sc, int sr, const char *kind) {
    const unsigned char *top = sk_tile_px(tc, tr), *side = sk_tile_px(sc, sr); if (!top || !side) return;
    static signed char occ[32][32][32]; memset(occ, 0, sizeof occ); for (int i = 0; i < n; i++) if (v[i].x < 32 && v[i].y < 32 && v[i].z < 32) occ[v[i].x][v[i].y][v[i].z] = 1;
    int dx = dim[0] > 0 ? dim[0] : 1, dy = dim[1] > 0 ? dim[1] : 1, dz = dim[2] > 0 ? dim[2] : 1;
    for (int i = 0; i < n; i++) {
        int is_top = v[i].y + 1 >= 32 || !occ[v[i].x][v[i].y + 1][v[i].z], tx = (v[i].x * XT + XT / 2) / dx % XT, ty = is_top ? (v[i].z * XT + XT / 2) / dz % XT : (v[i].y * XT + XT / 2) / dy % XT;
        const unsigned char *q = (is_top ? top : side) + (ty * XT + tx) * 4; double r = q[0], g = q[1], b = q[2];
        if (!kind) { r *= 1.6; g *= 1.6; b *= 1.6; }      /* the other ship's hull: the wall tile is dark in an unlit voxel, lift it */
        else if (!strcmp(kind, "rock")) { r *= 2.0; g *= 2.0; b *= 2.0; }      /* the engine room's tiles are almost black; the hull wall tile lifted gives a rock grey */
        else if (!strcmp(kind, "ore")) { r = r * 2.0 + 70; g = g * 1.6 + 25; b *= 1.0; }      /* leans orange */
        else if (!strcmp(kind, "crystal")) { r = r * 1.4 + 20; g = g * 2.2 + 90; b = b * 2.4 + 110; }      /* leans cyan */
        v[i].r = r > 255 ? 255 : (int)r; v[i].g = g > 255 ? 255 : (int)g; v[i].b = b > 255 ? 255 : (int)b; } }
static void sk_veh_put(int slot, int wx, int wy, const SkVox *v, int n, const int dim[3]) {
    static char mb[SK_VEH_MAXV * 32 + 64]; char d[PATH_MAX], p[PATH_MAX], sb[300]; snprintf(d, sizeof d, "%s/pieces/veh_%d", PCHQ, slot); mkdir(d, 0775);
    char *w = mb; w += snprintf(w, sizeof mb, "dim %d %d %d\n", dim[0], dim[1], dim[2]);
    for (int i = 0; i < n && (size_t)(w - mb) < sizeof mb - 40; i++) w += snprintf(w, sizeof mb - (size_t)(w - mb), "%d %d %d %d %d %d\n", v[i].x, v[i].y, v[i].z, v[i].r, v[i].g, v[i].b);
    snprintf(p, sizeof p, "%s/model.txt", d); write_if_changed(p, mb, (size_t)(w - mb));
    double cell = tune("obj_vox_pct", 100) / 100.0, sx = dim[0] * cell, sy = dim[2] * cell, sh = dim[1] * cell;
    snprintf(sb, sizeof sb, "entity_type=vehicle\ndesk=bridge\npos_x=%.2f\npos_y=%.2f\npos_z=%d\nsize_x=%.2f\nsize_y=%.2f\nsize_h=%.2f\n", wx - sk_dox + 0.5 - sx / 2, wy - sk_doy + 0.5 - sy / 2, tune("obj_z", 0) - sk_doz_get(), sx, sy, sh);
    snprintf(p, sizeof p, "%s/state.txt", d); write_if_changed(p, sb, strlen(sb)); }
static void sk_veh_clear(void) { char p[PATH_MAX]; for (int k = 1; k <= 9; k++) { snprintf(p, sizeof p, "%s/pieces/veh_%d/state.txt", PCHQ, k); unlink(p); } }
static void sk_veh_write(void) {
    if (!tune("obj_draw", 1) || !sk_world_here()) { sk_veh_clear(); return; }
    static SkVox v[SK_VEH_MAXV]; int dim[3], slot = 1; sk_craft_load(); sk_nodes_load();
    for (int i = 0; i < sk_nnodes && slot <= 9; i++) if (sk_nodes[i].room == SK_SHIP && sk_nodes[i].amount != 0) { int n = sk_nugget(sk_nodes[i].kind, sk_nodes[i].x, sk_nodes[i].y, v, dim); sk_retile(v, n, dim, 6, 2, 6, 2, sk_nodes[i].kind); sk_veh_put(slot++, sk_nodes[i].x, sk_nodes[i].y, v, n, dim); }
    for (int i = 0; i < sk_ncraft && slot <= 9; i++) if (sk_craft[i].wx >= 0) { int n = sk_skiff_model(v, SK_VEH_MAXV, dim); if (n > 0) sk_retile(v, n, dim, 8, 11, 6, 2, NULL); if (n > 0) sk_veh_put(slot++, sk_craft[i].wx, sk_craft[i].wy, v, n, dim); }
    for (char p[PATH_MAX]; slot <= 9; slot++) { snprintf(p, sizeof p, "%s/pieces/veh_%d/state.txt", PCHQ, slot); unlink(p); } }
static int sk_world_gap(int wx, int wy, int ox, int oy) {      /* Chebyshev distance from the desk cell of a world cell to the hull rectangle (0 = under the hull, 99 = off the desk) */
    int dx = wx - ox, dy = wy - oy; if (dx < 0 || dy < 0 || dx >= SK_W || dy >= SK_H) return 99; int ex = dx < SOX ? SOX - dx : dx >= SOX + SSZ ? dx - (SOX + SSZ - 1) : 0, ey = dy < SOY ? SOY - dy : dy >= SOY + SSZ ? dy - (SOY + SSZ - 1) : 0; return ex > ey ? ex : ey; }
static int sk_world_block(int nx, int ny, int nz, char *msg, size_t cap) {      /* 1 = the hull would sit on an asteroid or another ship that is taller than the ship flies */
    if (!sk_world_here() || nz > tune("obj_top", 6)) return 0; sk_craft_load(); sk_nodes_load();
    for (int i = 0; i < sk_nnodes; i++) if (sk_nodes[i].room == SK_SHIP && sk_world_gap(sk_nodes[i].x, sk_nodes[i].y, nx, ny) == 0) { snprintf(msg, cap, "an asteroid is in the way at %d,%d: lift above it, or mine it with m from beside it", sk_nodes[i].x, sk_nodes[i].y); return 1; }
    for (int i = 0; i < sk_ncraft; i++) if (sk_craft[i].wx >= 0 && sk_world_gap(sk_craft[i].wx, sk_craft[i].wy, nx, ny) == 0) { snprintf(msg, cap, "%s is there: the helm holds off (try `space hail %s`)", sk_craft[i].label, sk_craft[i].id); return 1; } return 0; }
static int sk_world_reach_idx(void) {      /* the asteroid node of the planet map nearest to the hull and within the beam's reach (-1 = none) */
    if (!sk_world_here()) return -1; sk_nodes_load(); int best = -1, bd = 99, reach = tune("obj_reach", 2); for (int i = 0; i < sk_nnodes; i++) if (sk_nodes[i].room == SK_SHIP) { int d = sk_world_gap(sk_nodes[i].x, sk_nodes[i].y, sk_dox, sk_doy); if (d <= reach && d < bd) { bd = d; best = i; } } return best; }
static void sk_space_verb(int argc, char **argv, char *msg, size_t cap) {      /* space status | hail <id> | move <id> <gx> <gy> */
    sk_craft_load(); const char *a = argc >= 2 ? argv[1] : "status";
    if (!strcmp(a, "status")) { char b[500] = ""; sk_nodes_load(); int rocks = 0; for (int i = 0; i < sk_nnodes; i++) if (sk_nodes[i].room == SK_SPACE) { char t[60]; snprintf(t, sizeof t, "%s@%d,%d x%d; ", sk_nodes[i].kind, (sk_nodes[i].x - 2) / 2, (sk_nodes[i].y - 2) / 2, sk_nodes[i].amount); strncat(b, t, sizeof b - strlen(b) - 1); rocks++; }
        for (int i = 0; i < sk_ncraft; i++) { char t[200]; snprintf(t, sizeof t, "%s '%s' @%d,%d crew %s cargo %s; ", sk_craft[i].id, sk_craft[i].label, sk_craft[i].gx, sk_craft[i].gy, sk_craft[i].crew, sk_craft[i].cargo); strncat(b, t, sizeof b - strlen(b) - 1); } snprintf(msg, cap, "space: %d asteroids, %d ships: %s", rocks, sk_ncraft, b); return; }
    if (!strcmp(a, "hail") && argc >= 3) { for (int i = 0; i < sk_ncraft; i++) if (!strcmp(sk_craft[i].id, argv[2])) { char who[40] = "", cp[120]; snprintf(cp, sizeof cp, "%s", sk_craft[i].crew); char *c0 = strtok(cp, ":,"); if (c0) snprintf(who, sizeof who, "%s", c0); char rb[160]; snprintf(rb, sizeof rb, "HAIL|%ld|%s|%s", (long)time(NULL), sk_craft[i].id, who); append_line("events.txt", rb); snprintf(msg, cap, "%s answers from the %s: \"%s here. Crew %s. Cargo %s.\"", who[0] ? who : "someone", sk_craft[i].label, who[0] ? who : "we", sk_craft[i].crew, sk_craft[i].cargo); return; } snprintf(msg, cap, "space: no ship %s", argv[2]); return; }
    if (!strcmp(a, "move") && argc >= 5) { for (int i = 0; i < sk_ncraft; i++) if (!strcmp(sk_craft[i].id, argv[2])) { int gx = atoi(argv[3]), gy = atoi(argv[4]); if (gx < 0 || gy < 0 || gx >= SK_GRID || gy >= SK_GRID) { snprintf(msg, cap, "space: off the chart"); return; } sk_craft[i].gx = gx; sk_craft[i].gy = gy; sk_craft_save(); sk_space_refresh(msg, cap); snprintf(msg, cap, "%s moved to %d,%d", sk_craft[i].id, gx, gy); return; } snprintf(msg, cap, "space: no ship %s", argv[2]); return; }
    snprintf(msg, cap, "usage: space status | hail <id> | move <id> <gx> <gy>"); }
static void sk_destroy_effect(const char *kind, int room, int x, int y, const char *src, char *msg, size_t cap) {      /* what events/destroyed calls: the BLOCK row becomes remnants where it stood */
    sk_wd_cfg(); SkBlk *b = sk_blk_find(kind); char rb[160]; int made = 0, dm = 0;
    if (b && b->cnt > 0) { sk_drops_load(); if (sk_ndrops < 64) { SkDrop *q = &sk_drops[sk_ndrops++]; q->id = sk_dropid++; q->room = room; q->x = x; q->y = y; q->n = b->cnt; snprintf(q->item, sizeof q->item, "%s", b->drop); q->owner[0] = 0; srand((unsigned)(now_ms() ^ ((long)getpid() << 7))); dm = (rand() % 100) < b->dpct;      /* every verb is a fresh process: seed or the first rand() is always the same */ q->dmg = dm; snprintf(q->src, sizeof q->src, "%s", kind); made = q->n; sk_drops_save(); } }
    { char p[PATH_MAX], bb[1200]; spath(p, "damage.txt"); read_file(p, bb, sizeof bb); char out[1200] = "", cp[1200], *sv = NULL; snprintf(cp, sizeof cp, "%s", bb); for (char *l = strtok_r(cp, "\n", &sv); l; l = strtok_r(NULL, "\n", &sv)) { char t[100], *f2[5]; snprintf(t, sizeof t, "%s", l); if (split_bar(t, f2, 5) >= 4 && !strcmp(f2[0], rooms[room].id) && atoi(f2[1]) == x && atoi(f2[2]) == y) continue; strncat(out, l, sizeof out - strlen(out) - 2); strcat(out, "\n"); } write_if_changed(p, out, strlen(out)); }
    { sk_nodes_load(); SkNode *nd = sk_node_at(room, x, y); if (nd && !strcmp(nd->kind, kind) && nd->amount > 0) { nd->amount--; if (nd->amount == 0) *nd = sk_nodes[--sk_nnodes]; sk_nodes_save(); } }      /* a node gives one load each time it is broken; endless nodes (-1) never run out */
    snprintf(rb, sizeof rb, "DESTROYED|%ld|%s|%s,%d,%d|%s x%d%s|by %s", (long)time(NULL), kind, rooms[room].id, x, y, b ? b->drop : "-", made, dm ? " (damaged)" : "", src && src[0] ? src : "?"); append_line("events.txt", rb);
    if (msg) snprintf(msg, cap, "%s destroyed: %s x%d%s", kind, b ? b->drop : "nothing", made, dm ? " (damaged)" : ""); }
static void sk_wd_verb(int argc, char **argv, char *msg, size_t cap) {
    sk_wd_cfg(); const char *v = argv[0];
    if (!strcmp(v, "build")) {      /* build here [kind] | build list | build kind <kind>: put a block in the cell in front of the controlled character; the BUILD row's items leave their inventory, the block is a node with amount 1 (solid, mined like any node) */
        if (argc >= 2 && !strcmp(argv[1], "list")) { char b[400] = ""; for (int i = 0; i < sk_nbld; i++) { char t[80]; snprintf(t, sizeof t, "%s=%s x%d; ", sk_bld[i].kind, sk_bld[i].item, sk_bld[i].n); strncat(b, t, sizeof b - strlen(b) - 1); } snprintf(msg, cap, "build: %s", b[0] ? b : "nothing is buildable"); return; }
        char kp[PATH_MAX]; spath(kp, "build_kind.txt"); if (argc >= 3 && !strcmp(argv[1], "kind")) { char t[40]; snprintf(t, sizeof t, "%s\n", argv[2]); write_if_changed(kp, t, strlen(t)); snprintf(msg, cap, "build kind %s", argv[2]); return; }
        if (argc < 2 || strcmp(argv[1], "here")) { snprintf(msg, cap, "usage: build here [kind] | build list | build kind <kind>"); return; }
        int sh = ctl_idx() < 0 ? 0 : ctl_idx() + 1; Ent *e = sk_life_ent(sh); static const int ddx[4] = { 0, -1, 1, 0 }, ddy[4] = { 1, 0, 0, -1 }; int tx = e->x + ddx[e->dir & 3], ty = e->y + ddy[e->dir & 3];
        if (sk_blocked(e->room, tx, ty) || other_at(e->room, tx, ty) || floor_blocked(e->room, tx, ty, 0)) { snprintf(msg, cap, "build: %d,%d is not free", tx, ty); return; }
        sk_life_load(sh); SkLife *L = &sk_life[sh]; char want[24] = ""; if (argc >= 3) snprintf(want, sizeof want, "%s", argv[2]); else { char t[40] = ""; read_file(kp, t, sizeof t); t[strcspn(t, "\n")] = 0; snprintf(want, sizeof want, "%s", t); }
        SkBuild *use = NULL; for (int i = 0; i < sk_nbld; i++) { if (sk_inv_count(L, sk_bld[i].item) < sk_bld[i].n || !sk_blk_find(sk_bld[i].kind)) continue; if (!use) use = &sk_bld[i]; if (want[0] && !strcmp(sk_bld[i].kind, want)) { use = &sk_bld[i]; break; } }      /* the wanted kind when affordable, else the first one the inventory pays for */
        if (!use) { snprintf(msg, cap, "build: %s lacks the items for %s (build list)", sk_life_id(sh), want[0] ? want : "any block"); return; }
        sk_inv_set(L, use->item, sk_inv_count(L, use->item) - use->n); sk_life_save(sh); sk_nodes_load(); if (sk_nnodes < 32) { SkNode *nd = &sk_nodes[sk_nnodes++]; nd->room = e->room; nd->x = tx; nd->y = ty; nd->amount = 1; snprintf(nd->kind, sizeof nd->kind, "%s", use->kind); sk_nodes_save(); }
        { char rb[160]; snprintf(rb, sizeof rb, "BUILT|%ld|%s|%s,%d,%d|cost %s x%d|by %s", (long)time(NULL), use->kind, rooms[e->room].id, tx, ty, use->item, use->n, sk_life_id(sh)); append_line("events.txt", rb); }
        snprintf(msg, cap, "%s built at %d,%d (cost %s x%d)", use->kind, tx, ty, use->item, use->n); return; }
    if (!strcmp(v, "mine") && argc >= 2 && (!strcmp(argv[1], "here") || !strcmp(argv[1], "nodes"))) {      /* mine here [tool]: the node in front of the controlled character (or under it), mined with the pick on foot and the ship's beam at the helm; mine nodes lists them */
        sk_nodes_load(); if (!strcmp(argv[1], "nodes")) { char b[600] = ""; for (int i = 0; i < sk_nnodes; i++) { char t[80]; snprintf(t, sizeof t, "%s %d,%d %s x%d; ", rooms[sk_nodes[i].room].id, sk_nodes[i].x, sk_nodes[i].y, sk_nodes[i].kind, sk_nodes[i].amount); strncat(b, t, sizeof b - strlen(b) - 1); } snprintf(msg, cap, "nodes %d: %s", sk_nnodes, b[0] ? b : "none"); return; }
        int sh = ctl_idx() < 0 ? 0 : ctl_idx() + 1; Ent *e = sk_life_ent(sh); static const int ddx[4] = { 0, -1, 1, 0 }, ddy[4] = { 1, 0, 0, -1 }; int tx = e->x + ddx[e->dir & 3], ty = e->y + ddy[e->dir & 3]; SkNode *nd = sk_node_at(e->room, tx, ty); if (!nd) nd = sk_node_at(e->room, e->x, e->y);
        if (!nd && e->room == SK_SPACE) for (int i = 0; i < sk_nnodes && !nd; i++) if (sk_nodes[i].room == SK_SPACE && abs(sk_nodes[i].x - e->x) <= 2 && abs(sk_nodes[i].y - e->y) <= 2) nd = &sk_nodes[i];      /* at the helm in space the beam reaches the asteroid on a neighbouring grid cell */
        if (!nd && flag_on("helm", 0) && sk_helm_direct()) { sk_direct_load(); int wi = sk_world_reach_idx(); if (wi >= 0) nd = &sk_nodes[wi]; }      /* direct flight: the beam reaches the asteroid of the planet map nearest to the hull */
        if (!nd) { snprintf(msg, cap, "nothing to mine in front of %s (%d,%d)", sk_life_id(sh), tx, ty); return; }
        char xs[12], ys[12]; snprintf(xs, sizeof xs, "%d", nd->x); snprintf(ys, sizeof ys, "%d", nd->y); char kd[24], rm[16]; snprintf(kd, sizeof kd, "%s", nd->kind); snprintf(rm, sizeof rm, "%s", rooms[nd->room].id); char *nv[6] = { "mine", kd, rm, xs, ys, argc >= 3 ? argv[2] : (flag_on("helm", 0) ? "beam" : "pick") }; sk_wd_verb(6, nv, msg, cap); return; }
    if (!strcmp(v, "mine")) {      /* mine <kind> <room> <x> <y> [tool]: adds the tool's power to the cell's progress; at hardness the cell is destroyed (event page) */
        if (argc < 5) { snprintf(msg, cap, "usage: mine <kind> <room> <x> <y> [tool]"); return; } SkBlk *b = sk_blk_find(argv[1]); int room = room_idx(argv[2]); if (!b || room < 0) { snprintf(msg, cap, "mine: unknown %s", !b ? "block kind" : "room"); return; }
        int x = atoi(argv[3]), y = atoi(argv[4]), pw = 1; if (argc >= 6) { pw = 0; for (int i = 0; i < sk_ntool; i++) if (!strcmp(sk_tool[i].name, argv[5])) pw = sk_tool[i].power; if (!pw) { snprintf(msg, cap, "mine: unknown tool %s", argv[5]); return; } }
        char p[PATH_MAX], bb[1200], out[1200] = "", cp[1200], *sv = NULL; int prog = 0, seen = 0; spath(p, "damage.txt"); read_file(p, bb, sizeof bb); snprintf(cp, sizeof cp, "%s", bb);
        for (char *l = strtok_r(cp, "\n", &sv); l; l = strtok_r(NULL, "\n", &sv)) { char t[100], *f2[5]; snprintf(t, sizeof t, "%s", l); if (split_bar(t, f2, 5) >= 4 && !strcmp(f2[0], rooms[room].id) && atoi(f2[1]) == x && atoi(f2[2]) == y) { prog = atoi(f2[3]); seen = 1; continue; } strncat(out, l, sizeof out - strlen(out) - 2); strcat(out, "\n"); } (void)seen;
        prog += pw; if (prog < b->hard) { char ln[100]; snprintf(ln, sizeof ln, "%s|%d|%d|%d|%s\n", rooms[room].id, x, y, prog, b->kind); strncat(out, ln, sizeof out - strlen(out) - 1); write_if_changed(p, out, strlen(out)); snprintf(msg, cap, "%s at %s %d,%d: %d/%d", b->kind, rooms[room].id, x, y, prog, b->hard); return; }
        write_if_changed(p, out, strlen(out)); { char dp[PATH_MAX], db[120]; spath(dp, "destroyed.txt"); snprintf(db, sizeof db, "%s|%s|%d|%d|mine %s\n", b->kind, rooms[room].id, x, y, argc >= 6 ? argv[5] : "hand"); write_if_changed(dp, db, strlen(db)); }
        if (!run_event_x("destroyed", "parallel", "")) sk_destroy_effect(b->kind, room, x, y, "mine", msg, cap); else snprintf(msg, cap, "%s broke at %s %d,%d (the destroyed page plays)", b->kind, rooms[room].id, x, y); return; }
    if (!strcmp(v, "destroy")) {      /* destroy last | destroy <kind> <room> <x> <y> [source]: remnants of one destroyed thing */
        char k[24] = "", rm[24] = "", src[24] = "page"; int x = 0, y = 0;
        if (argc >= 2 && !strcmp(argv[1], "last")) { char p[PATH_MAX], b[120] = "", *f2[6]; spath(p, "destroyed.txt"); read_file(p, b, sizeof b); b[strcspn(b, "\n")] = 0; if (split_bar(b, f2, 6) < 4) { snprintf(msg, cap, "destroy: nothing pending"); return; } snprintf(k, sizeof k, "%s", f2[0]); snprintf(rm, sizeof rm, "%s", f2[1]); x = atoi(f2[2]); y = atoi(f2[3]); if (f2[4]) snprintf(src, sizeof src, "%s", f2[4]); }
        else if (argc >= 5) { snprintf(k, sizeof k, "%s", argv[1]); snprintf(rm, sizeof rm, "%s", argv[2]); x = atoi(argv[3]); y = atoi(argv[4]); if (argc >= 6) snprintf(src, sizeof src, "%s", argv[5]); }
        else { snprintf(msg, cap, "usage: destroy last | destroy <kind> <room> <x> <y> [source]"); return; }
        int room = room_idx(rm); if (room < 0) { snprintf(msg, cap, "destroy: unknown room %s", rm); return; } sk_destroy_effect(k, room, x, y, src, msg, cap); return; }
    if (!strcmp(v, "hold")) {      /* hold status | give <item> [n] | board <id> [x y] | leave <id> | store <char> <item> [n] | fetch <char> <item> [n] | wreck */
        sk_hold_load(); const char *a = argc >= 2 ? argv[1] : "status";
        if (!strcmp(a, "status")) { char b[400] = ""; for (int i = 0; i < sk_nhold; i++) { char t[80]; if (!strcmp(sk_hold[i].kind, "item")) snprintf(t, sizeof t, "%s x%d; ", sk_hold[i].id, sk_hold[i].n); else snprintf(t, sizeof t, "[%s at %d,%d]; ", sk_hold[i].id, sk_hold[i].x, sk_hold[i].y); strncat(b, t, sizeof b - strlen(b) - 1); } snprintf(msg, cap, "hold %d/%d: %s", sk_nhold, sk_hold_cap, b[0] ? b : "empty"); return; }
        if (!strcmp(a, "give") && argc >= 3) { int n = argc >= 4 ? atoi(argv[3]) : 1; SkHold *h = sk_hold_find("item", argv[2]); if (!h) { if (sk_nhold >= sk_hold_cap || sk_nhold >= 64) { snprintf(msg, cap, "the hold is full (%d)", sk_hold_cap); return; } h = &sk_hold[sk_nhold++]; snprintf(h->kind, sizeof h->kind, "item"); snprintf(h->id, sizeof h->id, "%s", argv[2]); h->n = 0; h->x = h->y = 0; } h->n += n; sk_hold_save(); snprintf(msg, cap, "hold: %s x%d", h->id, h->n); return; }
        if (!strcmp(a, "board") && argc >= 3) { if (sk_hold_find("entity", argv[2])) { snprintf(msg, cap, "%s is already aboard", argv[2]); return; } if (sk_nhold >= sk_hold_cap || sk_nhold >= 64) { snprintf(msg, cap, "the hold is full (%d)", sk_hold_cap); return; } SkHold *h = &sk_hold[sk_nhold++]; snprintf(h->kind, sizeof h->kind, "entity"); snprintf(h->id, sizeof h->id, "%s", argv[2]); h->n = 1; h->x = argc >= 4 ? atoi(argv[3]) : 0; h->y = argc >= 5 ? atoi(argv[4]) : 0; sk_hold_save(); char rb[100]; snprintf(rb, sizeof rb, "BOARD|%ld|%s|%d,%d", (long)time(NULL), h->id, h->x, h->y); append_line("events.txt", rb); snprintf(msg, cap, "%s boarded at %d,%d (relative to the ship)", h->id, h->x, h->y); return; }
        if (!strcmp(a, "leave") && argc >= 3) { SkHold *h = sk_hold_find("entity", argv[2]); if (!h) { snprintf(msg, cap, "%s is not aboard", argv[2]); return; } *h = sk_hold[--sk_nhold]; sk_hold_save(); char rb[100]; snprintf(rb, sizeof rb, "LEAVE|%ld|%s", (long)time(NULL), argv[2]); append_line("events.txt", rb); snprintf(msg, cap, "%s left the ship", argv[2]); return; }
        if ((!strcmp(a, "store") || !strcmp(a, "fetch")) && argc >= 4) { sk_life_cfg(); int slot = sk_life_slot(argv[2]); if (slot < 0) { snprintf(msg, cap, "hold: unknown character %s", argv[2]); return; } sk_life_load(slot); SkLife *L = &sk_life[slot]; int n = argc >= 5 ? atoi(argv[4]) : 1, st = !strcmp(a, "store"); SkHold *h = sk_hold_find("item", argv[3]);
            if (st) { if (sk_inv_count(L, argv[3]) < n) { snprintf(msg, cap, "%s has no %d %s", argv[2], n, argv[3]); return; } if (!h) { if (sk_nhold >= sk_hold_cap || sk_nhold >= 64) { snprintf(msg, cap, "the hold is full (%d)", sk_hold_cap); return; } h = &sk_hold[sk_nhold++]; snprintf(h->kind, sizeof h->kind, "item"); snprintf(h->id, sizeof h->id, "%s", argv[3]); h->n = 0; h->x = h->y = 0; } sk_inv_set(L, argv[3], sk_inv_count(L, argv[3]) - n); h->n += n; }
            else { if (!h || h->n < n) { snprintf(msg, cap, "the hold has no %d %s", n, argv[3]); return; } h->n -= n; sk_inv_set(L, argv[3], sk_inv_count(L, argv[3]) + n); if (h->n <= 0) *h = sk_hold[--sk_nhold]; }
            sk_life_save(slot); sk_hold_save(); snprintf(msg, cap, "%s %s %d %s", argv[2], st ? "stored" : "fetched", n, argv[3]); return; }
        if (!strcmp(a, "wreck")) { int room = room_idx(sk_lc_room); if (room < 0) room = 0; sk_drops_load(); int made = 0, ents = 0; for (int i = 0; i < sk_nhold; i++) { SkHold *h = &sk_hold[i]; if (strcmp(h->kind, "item")) { ents++; continue; } int d = h->n * sk_hold_wreck / 100; if (d <= 0 || sk_ndrops >= 64) continue; SkDrop *q = &sk_drops[sk_ndrops++]; q->id = sk_dropid++; q->room = room; q->x = sk_lc_rx; q->y = sk_lc_ry; q->n = d; snprintf(q->item, sizeof q->item, "%s", h->id); q->owner[0] = 0; q->dmg = 1; snprintf(q->src, sizeof q->src, "wreck"); made += d; }
            sk_drops_save(); char rb[120]; snprintf(rb, sizeof rb, "WRECK|%ld|ship|dropped %d (%d%%)|entities released %d", (long)time(NULL), made, sk_hold_wreck, ents); append_line("events.txt", rb); sk_nhold = 0; sk_hold_save(); snprintf(msg, cap, "the ship is wrecked: %d items fall as damaged remnants, %d entities released", made, ents); return; }
        snprintf(msg, cap, "usage: hold status | give <item> [n] | board <id> [x y] | leave <id> | store <char> <item> [n] | fetch <char> <item> [n] | wreck"); return; } }
/* REMNANTS IN THE SCENE + AUTO PICKUP (owner 2026-10-25: "it will auto enter the entities inventory when it walks close (animated)"). Every remnant row of state/drops.txt is drawn as a mini block: a vehicle piece pieces/veh_1..9 (desk = its room, pos = the cell, size = blocks.pdl PICKUP size percent of a cell, self-lit, colour by item, darker when damaged) so the engine shows it with no new engine code (9 slots: the shown room first). A living character within PICKUP range cells of a remnant starts its pickup: the block hops and slides to the character over PICKUP ms (the pieces are rewritten every daemon tick and the engine is nudged), then the take runs (`life take`) and the block is gone. Data in blocks.pdl (PICKUP | range | ms | size). */
typedef struct { int id, slot; long start; } SkAnim;
static SkAnim sk_anim[16]; static int sk_nanim;
static void sk_item_rgb(const char *item, int dmg, int *r, int *g, int *b) {
    if (!strncmp(item, "stone", 5)) { *r = 140; *g = 140; *b = 148; } else if (!strncmp(item, "iron", 4)) { *r = 205; *g = 120; *b = 85; } else if (!strncmp(item, "gem", 3)) { *r = 90; *g = 225; *b = 235; } else if (!strncmp(item, "scrap", 5)) { *r = 170; *g = 175; *b = 185; } else if (!strncmp(item, "bread", 5)) { *r = 225; *g = 180; *b = 90; } else if (!strncmp(item, "fish", 4)) { *r = 120; *g = 170; *b = 230; } else if (!strncmp(item, "ore", 3)) { *r = 205; *g = 120; *b = 85; } else { *r = 230; *g = 210; *b = 70; }
    if (dmg > 0) { *r = *r * 55 / 100; *g = *g * 55 / 100; *b = *b * 55 / 100; } }
static void sk_remnant_nudge(void) {      /* wake the idle board engine so it redraws the moved blocks (same nudge as follow_pchq) */
    const char *cand[3] = {"@.apps/" PET_APP "/board/state/ui.txt", "@.apps/piececraft-hq/state/ui.txt", "@.apps/layout-studio/sandbox/state/ui.txt"}; char cp[3][PATH_MAX]; const char *cpp[3];
    for (int k = 0; k < 3; k++) { snprintf(cp[k], sizeof cp[k], "%s/%s", HOUSE, cand[k]); cpp[k] = cp[k]; } pet_nudge(cpp, 3); }
static void write_room_events(int r);
static int sk_remnants_tick(void) {
    sk_wd_cfg(); static long last_at; long now = now_ms(); if (now - last_at < 90) return 0; last_at = now; sk_drops_load(); int changed = 0;
    for (int i = 0; i < sk_ndrops; i++) {      /* who is close enough to pick it up */
        SkDrop *d = &sk_drops[i]; int a = -1; for (int k = 0; k < sk_nanim; k++) if (sk_anim[k].id == d->id) a = k; if (a >= 0) continue;
        int best = -1, bd = 1000; for (int s = 0; s <= nparty; s++) { if (sk_life[s].dead) continue; Ent *e = sk_life_ent(s); if (e->room != d->room) continue; int dist = abs(e->x - d->x) + abs(e->y - d->y); if (dist <= sk_pk_range && dist < bd) { bd = dist; best = s; } }
        if (best >= 0 && sk_nanim < 16) { sk_anim[sk_nanim].id = d->id; sk_anim[sk_nanim].slot = best; sk_anim[sk_nanim++].start = now; } }
    static char prev[16][1100]; char rows[16][1100]; for (int r = 0; r < 16; r++) rows[r][0] = 0; int cnt[16] = {0};
    for (int i = 0; i < sk_ndrops; i++) {
        SkDrop *d = &sk_drops[i]; if (d->room < 0 || d->room >= 16 || d->room >= nroom || cnt[d->room] >= 24) continue; int x = d->x, y = d->y, a = -1; for (int k = 0; k < sk_nanim; k++) if (sk_anim[k].id == d->id) a = k;
        if (a >= 0) { double t = (double)(now - sk_anim[a].start) / (double)(sk_pk_ms > 0 ? sk_pk_ms : 1); if (t > 1) t = 1; Ent *e = sk_life_ent(sk_anim[a].slot); x = (int)lround(d->x + (e->x - d->x) * t); y = (int)lround(d->y + (e->y - d->y) * t); }
        int r, g, b; sk_item_rgb(d->item, d->dmg, &r, &g, &b); size_t o = strlen(rows[d->room]); snprintf(rows[d->room] + o, sizeof rows[0] - o, "%d %d %d %d %d rem %d %d 0 *\n", x, y, r, g, b, sk_rem_idx(d->item, d->dmg), a >= 0 || sk_pk_spin <= 0 ? 2 : (int[]){ 2, 6, 4, 8 }[(int)((now / sk_pk_spin + d->id) % 4)]); cnt[d->room]++; }
    sk_nodes_load(); for (int i = 0; i < sk_nnodes; i++) { SkNode *nd = &sk_nodes[i]; if (nd->room >= 16 || cnt[nd->room] >= 28) continue; SkBlk *bk = sk_blk_find(nd->kind); int r, g, b; sk_item_rgb(bk ? bk->drop : nd->kind, 0, &r, &g, &b); size_t o = strlen(rows[nd->room]); snprintf(rows[nd->room] + o, sizeof rows[0] - o, "%d %d %d %d %d rem %d 2 0 *\n", nd->x, nd->y, r, g, b, sk_node_idx(bk ? bk->drop : nd->kind)); cnt[nd->room]++; }
    for (int r = 0; r < nroom && r < 16; r++) { if (strcmp(rows[r], prev[r])) { snprintf(prev[r], sizeof prev[0], "%s", rows[r]); snprintf(sk_rem_rows[r], sizeof sk_rem_rows[0], "%s", rows[r]); write_room_events(r); changed = 1; } }
    for (int k = 0; k < sk_nanim; k++) {      /* finished slides: the pickup itself */
        if (now - sk_anim[k].start < sk_pk_ms) continue; int id = sk_anim[k].id, slot = sk_anim[k].slot; char ib[16], m2[160]; snprintf(ib, sizeof ib, "%d", id); char *hv[4] = { "life", "take", ib, (char *)sk_life_id(slot) }; sk_life_verb(4, hv, m2, sizeof m2); sk_drops_load(); changed = 1;
        sk_anim[k] = sk_anim[--sk_nanim]; k--; }
    for (int k = 0; k < sk_nanim; k++) { int alive = 0; for (int i = 0; i < sk_ndrops; i++) if (sk_drops[i].id == sk_anim[k].id) alive = 1; if (!alive) { sk_anim[k] = sk_anim[--sk_nanim]; k--; } }
    if (changed) sk_remnant_nudge(); return changed; }
#elif PET_SECTION == 76
            if ((c == 'm' || c == 'M') && run_on() && !flag_on("ask", 0)) { char *mv[2] = { "mine", "here" }; sk_wd_verb(2, mv, msg, sizeof msg); dirty = 1; continue; }      /* m = mine / beam: the node in front of the controlled character (blocks.pdl NODE rows) */
            if ((c == 'b' || c == 'B') && run_on() && !flag_on("ask", 0)) { char *bv[2] = { "build", "here" }; sk_wd_verb(2, bv, msg, sizeof msg); dirty = 1; continue; }      /* b = place a block in front (blocks.pdl BUILD rows) */
            if (c == 27 && flag_on("helm", 0)) { char *hv[2] = { "helm", "off" }; sk_helm_verb(2, hv, msg, sizeof msg); dirty = 1; }      /* Esc lets go of the helm and still leaves In: (the default below) */
#elif PET_SECTION == 77
            { char tb[100]; snprintf(tb, sizeof tb, "%ld enter-in-engine hero=%d,%d room=%s ctl=%d", (long)time(NULL), tr.x, tr.y, rooms[tr.room].id, ctl_idx()); append_line("int_trace.txt", tb); }
            took = sk_helm_interact(msg, sizeof msg);
#elif PET_SECTION == 79
        if (run_on() && sk_life_tick()) dirty = 1;
        if (run_on() && sk_env_tick()) dirty = 1;      /* air: leaks through holes, the recycler, choking */
        if (run_on() && sk_remnants_tick()) dirty = 1;      /* remnants drawn as mini blocks, auto pickup when a character walks close */      /* hunger, starving, death, respawn (life.pdl) */
#elif PET_SECTION == 78
            if (sk_helm_follow(msg, sizeof msg)) dirty = 1;
#endif
#endif
