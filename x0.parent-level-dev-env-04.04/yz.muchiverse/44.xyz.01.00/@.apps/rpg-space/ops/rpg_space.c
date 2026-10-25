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
static void sk_helm_verb(int argc, char **argv, char *msg, size_t cap);      /* the helm (section 75) */
static void sk_life_verb(int argc, char **argv, char *msg, size_t cap); static int sk_life_tick(void); static void sk_life_ui(char *out, size_t cap);      /* life, death, respawn, drops (section 75) */
static long sk_helm_settle = 0;      /* until this time the xelector is not read as a steering move (a desk change or a move puts it somewhere else by itself) */
static int sk_pos_of(const char *id, int *gx, int *gy); static void sk_ship_load(int *gx, int *gy);
static int sk_choice(char *title, char *q, char *yes, char *no, char *direct);
static int sk_art[NP];      /* 1: pet i has art (csv frames exist: the 3D view shows its front picture) */
static void sk_ensure_map(void); static int sk_blocked(int room, int x, int y); static void sk_pets_sync(void); static void sk_paint_bg(int room, const char *phase); static void sk_spawn(int i, int *x, int *y);
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
    sk_build_ship(); sk_build_space();
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
    snprintf(p, sizeof p, "%s/cells.rgba", d); { FILE *f = fopen_tmp(p); if (f) { fwrite(atlas, 1, (size_t)aw * ah * 4, f); fclose_tmp(f, p); } } snprintf(p, sizeof p, "%s/cells.png", d); png_atomic(p, aw, ah, atlas, aw * 4); free(atlas);
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
    (void)models; sk_direct_load(); sk_ship_world_write(); char d[PATH_MAX], p[PATH_MAX], b[300];
    snprintf(d, sizeof d, "%s/pieces/land_01", PCHQ); mkdir(d, 0775); snprintf(p, sizeof p, "%s/state.txt", d);
    snprintf(b, sizeof b, "entity_type=land\npos_x=%d\npos_y=%d\nalt=%d\nsrc=region\n", -sk_dox, -sk_doy, sk_doz_get()); write_if_changed(p, b, strlen(b));
}
static void sk_land_remove(void) { char p[PATH_MAX]; snprintf(p, sizeof p, "%s/pieces/ship_01/state.txt", PCHQ); unlink(p); snprintf(p, sizeof p, "%s/pieces/land_01/state.txt", PCHQ); unlink(p); }
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
        int z = sk_doz_get() + atoi(argv[2]); if (z < -6 || z > 12) { snprintf(msg, cap, "the ship cannot go %s than this (%d)", z < 0 ? "lower" : "higher", sk_doz_get()); return; }
        { char zp[PATH_MAX], zb[16]; spath(zp, "direct_z.txt"); snprintf(zb, sizeof zb, "%d\n", z); write_if_changed(zp, zb, strlen(zb)); char rb[100]; snprintf(rb, sizeof rb, "DIRECT|%ld|lift|%d", (long)time(NULL), z); append_line("events.txt", rb); }
        sk_direct_load(); sk_land_write(0); snprintf(msg, cap, "ship altitude %d", z); return; }
    if (!strcmp(a, "move") && argc >= 4) {
        if (!flag_on("helm", 0)) { snprintf(msg, cap, "not at the helm"); return; }
        if (sk_helm_direct()) {      /* direct: the arrows move the land pieces under the ship (state/direct.txt + pieces/land_*); no rebuild, no reload */
            sk_direct_load(); int nx = sk_dox + atoi(argv[2]), ny = sk_doy + atoi(argv[3]); int mx = tune("direct_max", 16); if (mx > 16) mx = 16;
            if (nx < -mx || nx > mx || ny < -mx || ny > mx) { snprintf(msg, cap, "the edge of the map: the ship stops (%d,%d)", sk_dox, sk_doy); return; }
            { char dp[PATH_MAX], db[24], rb[120]; spath(dp, "direct.txt"); snprintf(db, sizeof db, "%d %d\n", nx, ny); write_if_changed(dp, db, strlen(db)); snprintf(rb, sizeof rb, "DIRECT|%ld|ship|%d,%d|%d,%d", (long)time(NULL), sk_dox, sk_doy, nx, ny); append_line("events.txt", rb); }
            sk_dox = nx; sk_doy = ny; sk_land_write(0); sk_helm_settle = now_ms() + 300; snprintf(msg, cap, "ship at %d,%d over %s", nx, ny, sk_pl_cur < 0 ? "deep space" : sk_pl[sk_pl_cur].label); return; }
        int gx, gy, dx = atoi(argv[2]), dy = atoi(argv[3]); sk_ship_load(&gx, &gy); int nx = gx + dx, ny = gy + dy;
        if (nx < 0 || ny < 0 || nx >= SK_GRID || ny >= SK_GRID) { snprintf(msg, cap, "the edge of the charted system (%d,%d)", gx, gy); return; }
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
typedef struct { int id, room, x, y, n; char item[24], owner[24]; } SkDrop;
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
static void sk_drops_save(void) { char p[PATH_MAX], b[64 * 80 + 32], *w = b; spath(p, "drops.txt"); b[0] = 0; for (int i = 0; i < sk_ndrops; i++) { SkDrop *d = &sk_drops[i]; w += snprintf(w, sizeof b - (size_t)(w - b), "%d|%s|%d|%d|%d|%s|%d\n", d->id, rooms[d->room].id, d->x, d->y, d->n, d->item, 0); (void)d->owner; } write_if_changed(p, b, strlen(b)); }
static void sk_drops_load(void) { char p[PATH_MAX], b[64 * 80 + 32]; spath(p, "drops.txt"); read_file(p, b, sizeof b); sk_ndrops = 0; for (char *l = strtok(b, "\n"); l && sk_ndrops < 64; l = strtok(NULL, "\n")) { char *f[8]; if (split_bar(l, f, 8) < 6) continue; SkDrop *d = &sk_drops[sk_ndrops]; d->id = atoi(f[0]); d->room = room_idx(f[1]); d->x = atoi(f[2]); d->y = atoi(f[3]); d->n = atoi(f[4]); snprintf(d->item, sizeof d->item, "%s", f[5]); d->owner[0] = 0; if (d->room < 0 || d->n <= 0) continue; if (d->id >= sk_dropid) sk_dropid = d->id + 1; sk_ndrops++; } }
static void sk_life_log(const char *kind, int slot, const char *extra) { char b[200]; snprintf(b, sizeof b, "%s|%ld|%s|%s", kind, (long)time(NULL), sk_life_id(slot), extra); append_line("events.txt", b); }
static void sk_life_respawn_effect(int slot) {      /* the respawn: back on the safe place, full hp, hunger 0 (life.pdl respawn_*); what events/respawn calls */
    SkLife *L = &sk_life[slot]; char ex[100]; int rr = room_idx(sk_lc_room); if (rr < 0) rr = 0; sk_life_load(slot); L->hp = sk_life_mhp(slot); L->hunger = 0; L->dead = 0; L->last = now_ms();
    int ci = slot == 0 ? -1 : slot - 1; use_ent(ci); pet.room = rr; pet.x = sk_lc_rx; pet.y = sk_lc_ry; pet.tx = pet.ty = -1; pet_save(); use_ent(active);
    snprintf(ex, sizeof ex, "%s,%d,%d", sk_lc_room, sk_lc_rx, sk_lc_ry); sk_life_log("RESPAWN", slot, ex); sk_life_save(slot); }
static void sk_life_die_effect(int slot) {      /* the death itself: drop_pct of every stack falls where they stood; then the respawn page / effect by death_mode. What events/death calls */
    SkLife *L = &sk_life[slot]; sk_life_load(slot); Ent *e = sk_life_ent(slot); char b[400], ex[160], *sv = NULL; int dropped = 0; snprintf(b, sizeof b, "%s", L->inv);
    sk_drops_load(); for (char *t = strtok_r(b, ",", &sv); t; t = strtok_r(NULL, ",", &sv)) { char *c = strchr(t, ':'); if (!c) continue; char k[24]; snprintf(k, sizeof k, "%.*s", (int)(c - t), t); int cnt = atoi(c + 1), d = cnt * sk_lc_drop / 100; if (d <= 0 || sk_ndrops >= 64) continue;
        SkDrop *q = &sk_drops[sk_ndrops++]; q->id = sk_dropid++; q->room = e->room; q->x = e->x; q->y = e->y; q->n = d; snprintf(q->item, sizeof q->item, "%s", k); q->owner[0] = 0; dropped += d; }
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
        sk_inv_set(&sk_life[ts], d->item, sk_inv_count(&sk_life[ts], d->item) + d->n); sk_life_save(ts); { char ex[100]; snprintf(ex, sizeof ex, "#%d %s x%d", d->id, d->item, d->n); sk_life_log("TAKE", ts, ex); } snprintf(msg, cap, "%s took %s x%d", sk_life_id(ts), d->item, d->n); sk_drops[k] = sk_drops[--sk_ndrops]; sk_drops_save(); return; }
    snprintf(msg, cap, "usage: life status [id] | die <id|@actor> | respawn <id|@actor> | hurt <id> <n> | feed <id> <n> | give <id> <item> [n] | set <id> hunger|hp <n> | take <drop#> [id] | reset");
}
#elif PET_SECTION == 76
            if (c == 27 && flag_on("helm", 0)) { char *hv[2] = { "helm", "off" }; sk_helm_verb(2, hv, msg, sizeof msg); dirty = 1; }      /* Esc lets go of the helm and still leaves In: (the default below) */
#elif PET_SECTION == 77
            { char tb[100]; snprintf(tb, sizeof tb, "%ld enter-in-engine hero=%d,%d room=%s ctl=%d", (long)time(NULL), tr.x, tr.y, rooms[tr.room].id, ctl_idx()); append_line("int_trace.txt", tb); }
            took = sk_helm_interact(msg, sizeof msg);
#elif PET_SECTION == 79
        if (run_on() && sk_life_tick()) dirty = 1;      /* hunger, starving, death, respawn (life.pdl) */
#elif PET_SECTION == 78
            if (sk_helm_follow(msg, sizeof msg)) dirty = 1;
#endif
#endif
