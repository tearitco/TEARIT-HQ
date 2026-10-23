/* rpg_edit_core.c - the edit board (floating-island town) on the shared pets-like core (&.widgits/_shared-lib/petslike_core.c). Two roles in one file: the top builds the program, and the core
 * re-includes this file once per SECTION to pull in what is specific to this game. Design: @.apps/../#.#.calendar-dox/!.HQ-IQ-BOOK/16.game/PETSLIKE-FRAMEWORK-PLAN.md (P3) */
#ifndef PET_SECTION
#define PET_APP "rpg-edit-core"
#define PET_OP "rpg_edit_core"
#define PET_SKIN "rpg_edit_core.c"
/* what this game needs beyond the core defaults (the core takes them as #ifndef): 16 pets, island pages 56 wide, a 2D frame that holds a 56 x 17 page at 24 px, tiles as the default 2D view, its own room export */
#define NP 16
#define MAXC 64
#define MAXR 24
#define FW 1360
#define FH 440
#define PET_VIEW_CELL "24"
#define PET_VIEW_STYLE "tiles"
#define PET_OWN_EXPORT 1
#include "../../../&.widgits/_shared-lib/petslike_core.c"
#else
#if PET_SECTION == 1
 * The floating-island town of the edit board (BOOK 16.game/EDIT-ISLANDS-DESIGN.md, PETSLIKE-FRAMEWORK-PLAN.md P3): a ninth skin of the shared core. Its map painter (islands, interiors, hung rock) and its pet sheets are section 73;
 * everything else (pets, chat, clock, doors, shop, board) is the core. Every picture is an RPG Maker MV tile or character frame (sheets under #.NNEST_ASSETS/rmmv-www-img), or a pet picture from art/.
#elif PET_SECTION == 2
#include "../../../&.widgits/_shared-lib/kh_state_notify.h"      /* STATE_BACKEND=file|marker|shmem: "did it change" without reading it (branch marker, 2026-10-12) */
#include "../../../&.widgits/_shared-lib/csv_frames.c"      /* a pet's sprite.csv -> the 16x24 3D walk frames */
#elif PET_SECTION == 3
/* ---- rpg-edit-core: the floating-island town (BOOK 16.game/EDIT-ISLANDS-DESIGN.md). sk_ensure_map() (section 73) builds the map into these arrays once per process; the index is the room's place in rooms.pdl
 * (start, bank, store, hotel, school, cabin). glyph: G grass, p path, T pine, B building, D door, ~ water, b bridge, s ramp, F fire, S sign, r rubble, _ void (a hole in the island), W wall, . floor.
 * fs / ws: the floor and the wall/object slot of the 24 px tile atlas, cdep: how many blocks hang under the cell (negative z, tapered into an inverted cone). ---- */
#define SK_NR 8
#define SK_W 56
#define SK_H 17
#define SK_IW 24
static char sk_glyph[SK_NR][SK_H][SK_W + 1];
static int sk_fs[SK_NR][SK_H][SK_W], sk_ws[SK_NR][SK_H][SK_W], sk_cdep[SK_NR][SK_H][SK_W];
static int sk_built, sk_rockslot, sk_ndoor, sk_doorx[8], sk_doory[8], sk_arrx[8], sk_arry[8]; static char sk_bname[8][16];
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
    for (int i = 0; i < nparty; i++) {      /* the island pets are not DB actors: default stats, and every pet wears sheet !edit<i/8> index i%8 (the skin writes those sheets, section 73) */
        if (!party[i].mhp) { party[i].mhp = 40; party[i].mmp = 20; party[i].atk = party[i].def = party[i].agi = party[i].luk = 10; party[i].mat = party[i].mdf = 8; }
        if (!party[i].nick[0]) snprintf(party[i].nick, 24, "island pet"); if (!party[i].profile[0]) snprintf(party[i].profile, 96, "A pet of the floating islands.");
        snprintf(party[i].sheet, 24, "!edit%d", i / 8); party[i].idx = i % 8; }
    sk_pets_sync();
#elif PET_SECTION == 11
#elif PET_SECTION == 12
    for (int i = 1; i < nroom; i++) rooms[i].outdoor = 0;      /* only the island page (room 0) is outdoors: the interiors get no 3D sky and only the mild indoor tint */
    sk_ensure_map();      /* the doors are the D cells of the island map: one on the island (trigger = the D cell itself, so ty is the row above) and one back out of each interior */
    ndoor = 0;
    for (int k = 0; k < sk_ndoor; k++) { int dest = room_idx(sk_bname[k]); if (dest < 0 || ndoor + 2 > 24) continue;
        Door *a = &doors[ndoor++]; a->room = 0; a->tx = sk_doorx[k]; a->ty = sk_doory[k] - 1; a->dest = dest; a->ax = SK_IW / 2; a->ay = SK_H - 3; a->autow = 1;
        Door *b = &doors[ndoor++]; b->room = dest; b->tx = SK_IW / 2; b->ty = SK_H - 3; b->dest = 0; b->ax = sk_arrx[k]; b->ay = sk_arry[k]; b->autow = 1; }
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
    W("title=rpg-edit-core  -  %s  -  coins %d\nscene_raw=%s\ncanvas_raw=%s\ncoins=%d\nmsg=%s\nroom_label=%s\n", rooms[vroom].name, coins, raw, raw, coins, msg && msg[0] ? msg : (in ? "INT on: turn based like a roguelike. Time moves when you act. Tab switches pet/player." : "real time: the pets live on their own. INT lets you control a pet or the player."), rooms[vroom].name);
#elif PET_SECTION == 28
    W("left_visible=%s\n", flag_on("left", 1) ? "1" : "");      /* the pinned info bar (default on, Menu > Info show / hide, its _ button) */
    W("chat_visible=%s\nhb_visible=%s\nview_label=view %s\nchat_toggle_label=window %s\n", chat ? "1" : "", hb ? "1" : "", view, chat ? "on" : "off");
    { char cp[PATH_MAX], cb[4000]; spath(cp, "chat.txt"); read_tail(cp, cb, sizeof cb); char *ln[6] = {0}; int k = 0; for (char *l = strtok(cb, "\n"); l; l = strtok(NULL, "\n")) { ln[k % 6] = l; k++; } for (int i = 0; i < 6; i++) { int j = k - 6 + i; W("chat_%d=%s\n", i, j >= 0 ? ln[j % 6] : ""); } }
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
    if (!strncmp(sh, "!edit", 5)) return;      /* the island pets' 3D frames are written with their sheets (csv_write_frames, section 73) */
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
        if (!strstr(b, "khtpm_core_render")) continue; for (size_t i = 0; i < n; i++) if (!b[i]) b[i] = ' '; int hit = 0; if (strstr(b, "/@.apps/rpg-edit-core/board/pchq-board.xhtpm")) { v |= 1; hit = 1; } if (strstr(b, "rpg-edit-core.xhtpm")) { v |= 2; hit = 1; } if (hit && np < 8) pids[np++] = (pid_t)atoi(e->d_name); }
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
    if (!strcmp(v, "toggle") && argc >= 2 && (!strcmp(argv[1], "chat") || !strcmp(argv[1], "hb") || !strcmp(argv[1], "voice") || !strcmp(argv[1], "left"))) { set_flag(argv[1], flag_on(argv[1], !strcmp(argv[1], "voice") || !strcmp(argv[1], "left")) ? "0" : "1"); snprintf(msg, cap, "%s toggled", argv[1]); return; }
#elif PET_SECTION == 66
    signal(SIGTERM, bye); signal(SIGINT, bye); signal(SIGHUP, bye); srand((unsigned)(time(NULL) ^ getpid()));
#elif PET_SECTION == 67
        { static long pl_safety = 0; long pn = now_ms(); if (!sn_active(&g_sn) || sn_changed(&g_sn, SL_PETS) || pn - pl_safety > 30000) { pl_safety = pn; pet_load(); } }      /* marker/shmem: reload the pets only when a writer moved the slot (or every 30 s for a hand edit) */
        int in = flag_on("int", 0), tm = turn_mode(); static int prev_in = 0; static long int_on_at = 0; if (in && !prev_in) int_on_at = now_ms(); prev_in = in;
#elif PET_SECTION == 68
        if (in && n) { char kl[300], one[16]; kl[0] = 0; for (int i = 0; i < n && i < 20; i++) { snprintf(one, sizeof one, "%d ", codes[i]); strncat(kl, one, sizeof kl - strlen(kl) - 1); } char kp[PATH_MAX]; spath(kp, "keys_seen.txt"); write_if_changed(kp, kl, strlen(kl)); }      /* last batch of codes, for debugging */
#elif PET_SECTION == 69
        { char dp[PATH_MAX], db[40]; static long dl_safety = 0; long dn = now_ms(); if (!sn_active(&g_sn) || sn_changed(&g_sn, SL_DAY) || dn - dl_safety > 30000) { dl_safety = dn; spath(dp, "daylight.txt"); read_file(dp, db, sizeof db); } else snprintf(db, sizeof db, "%s", lastday); if (strcmp(db, lastday)) { snprintf(lastday, sizeof lastday, "%s", db); dirty = 1; } }
#elif PET_SECTION == 70
    signal(SIGPIPE, SIG_IGN);      /* a closed pipe / dead child must never kill a verb or the daemon */
    signal(SIGPIPE, SIG_IGN);      /* a closed pipe / dead child must never kill a verb or the daemon */
    snprintf(ST, sizeof ST, "%s/state", APP); mkdir(ST, 0755); sn_init(&g_sn, ST);
    snprintf(PCHQ, sizeof PCHQ, "%s/rpg-edit-core-pchq", APP);
#elif PET_SECTION == 71
    load_data(); replay(); pet_load(); clock_paths();
#elif PET_SECTION == 72
    if (!strncmp(name, "!edit", 5)) snprintf(p, sizeof p, "%s/../tsots-characters/%s.png", ASSETS, name);      /* the generated pet sheets live next to the 2D renderer's own charsets */
#elif PET_SECTION == 73
/* ===== rpg-edit-core: the island town. Ported from the old standalone rpg_edit.c (map painter, taper, atlas, pet sheets); everything else is the shared core. ===== */
/* The island page, drawn in EDIT-ISLANDS-DESIGN.md section 2: G grass, p path, T pine, B building, D door, ~ water, b bridge, s ramp, F fire, S sign, r rubble, . sky */
static const char *SK_ISL[SK_H] = {
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
static const char *SK_FAR[7] = { "..GGGG....", ".GGGTGGG..", "GGGGGGGGG.", "GGpppGGGGG", "GGGGFGGTG.", ".GGGGGGG..", "..GGGG...." };
#define SK_FARX 38
#define SK_FARY 5
static char sk_isl_at(int x, int y) {
    if (x < 24) return SK_ISL[y][x];
    if (x >= SK_FARX && x < SK_FARX + 10 && y >= SK_FARY && y < SK_FARY + 7) return SK_FAR[y - SK_FARY][x - SK_FARX];
    return '.';
}
typedef struct { int x, y, bx, by; const char *name; } SkBld;      /* building top-left, World_B cell (col,row) */
static const SkBld sk_bld[5] = {{9, 9, 8, 0, "cabin"}, {3, 1, 6, 11, "bank"}, {13, 1, 0, 7, "store"}, {9, 5, 4, 7, "school"}, {22, 2, 4, 11, "hotel"}};

static void sk_solid(int d, int x, int y, char g, int r, int gr, int b, int plank, int tall) {
    unsigned char t[XT * XT * 4];
    for (int i = 0; i < XT * XT; i++) {
        int sh = ((i * 2654435761u) >> 28) % 7 - 3, dk = (plank && ((i / XT) % 6 == 5)) ? -30 : 0;
        t[i * 4] = (unsigned char)(r + sh + dk); t[i * 4 + 1] = (unsigned char)(gr + sh + dk); t[i * 4 + 2] = (unsigned char)(b + sh + dk); t[i * 4 + 3] = 255;
    }
    sk_glyph[d][y][x] = g; if (tall) sk_ws[d][y][x] = slot_of(t); else sk_fs[d][y][x] = slot_of(t);
}
static void sk_stamp(int d, Sheet *s, int c, int r, int w, int h, int x, int y, char g, int tall) {
    unsigned char a[XT * XT * 4], b[XT * XT * 4];
    for (int cj = 0; cj < h; cj++) for (int ci = 0; ci < w; ci++) {
        int xx = x + ci, yy = y + cj; if (xx < 0 || yy < 0 || xx >= SK_W || yy >= SK_H) continue;
        tile24(s, (c + ci) * S48, (r + cj) * S48, a);
        memset(b, 0, sizeof b);
        if (sk_fs[d][yy][xx]) memcpy(b, slotpx[sk_fs[d][yy][xx]], sizeof b);
        over24(b, a);
        sk_glyph[d][yy][xx] = g;
        int sl = slot_of(b);
        if (tall) sk_ws[d][yy][xx] = sl; else { sk_fs[d][yy][xx] = sl; sk_ws[d][yy][xx] = 0; }
    }
}
/* an island's underside tapers: the hung depth grows with the ring distance from the nearest air cell (edge 1, then 3, 5, 7, cap 9), so each island ends in an inverted cone; bridges and rubble stay thin */
static void sk_taper(void) {
    for (int y = 0; y < SK_H; y++) for (int x = 0; x < SK_W; x++) {
        char c = sk_glyph[0][y][x];
        if (c == '_') { sk_cdep[0][y][x] = 0; continue; }
        if (c == 'b' || c == 's') { sk_cdep[0][y][x] = 1; continue; }
        if (c == 'r') { sk_cdep[0][y][x] = 2; continue; }
        int best = 99;
        for (int yy = -1; yy <= SK_H; yy++) for (int xx = -1; xx <= SK_W; xx++) {
            int air = (yy < 0 || xx < 0 || yy >= SK_H || xx >= SK_W) || sk_glyph[0][yy][xx] == '_';
            if (!air) continue;
            int dx = abs(xx - x), dy = abs(yy - y), dd = dx > dy ? dx : dy;
            if (dd < best) best = dd;
        }
        int dep = 2 * best - 1; if (dep > 9) dep = 9; if (dep < 1) dep = 1;
        sk_cdep[0][y][x] = dep;
    }
}
static int sk_blocked(int room, int x, int y) {
    sk_ensure_map();
    if (room < 0 || room >= SK_NR || x < 0 || y < 0 || x >= SK_W || y >= SK_H) return 1;
    if (room && (x < 1 || y < 1 || x >= SK_IW - 1 || y >= SK_H - 1)) return 1;
    char g = sk_glyph[room][y][x];
    return g == 'W' || g == 'T' || g == 'B' || g == '_' || g == '~' || g == 'r' || g == 'F' || g == 'S';
}
static void sk_drop_sheet(const char *name) { for (int i = 0; i < nsheet; i++) if (!strcmp(sheets[i].name, name)) { stbi_image_free(sheets[i].px); sheets[i] = sheets[--nsheet]; return; } }
static void sk_set_kv_line(const char *path, const char *key, const char *val) { char b[8192], out[8300]; size_t o = 0; int done = 0, kl = (int)strlen(key); read_file(path, b, sizeof b);
    for (char *l = b; *l; ) { char *e = strchr(l, '\n'); size_t len = e ? (size_t)(e - l) : strlen(l); if (!strncmp(l, key, kl) && l[kl] == '=') { o += (size_t)snprintf(out + o, sizeof out - o, "%s=%s\n", key, val); done = 1; } else o += (size_t)snprintf(out + o, sizeof out - o, "%.*s\n", (int)len, l); l += len + (e ? 1 : 0); }
    if (!done) o += (size_t)snprintf(out + o, sizeof out - o, "%s=%s\n", key, val); write_if_changed(path, out, o); }

/* Build every room's tiles once per process. Needs rooms.pdl (the interiors' floor tile), the RPG Maker sheets and nothing else. */
static void sk_ensure_map(void) {
    if (sk_built) return; sk_built = 1;
    Sheet *a5 = sheet("tilesets", "Outside_A5"), *wb = sheet("tilesets", "World_B"), *ob = sheet("tilesets", "Outside_B"), *in = sheet("tilesets", "Inside_A5"), *a4 = sheet("tilesets", "Outside_A4"), *door = sheet("characters", "!Door1");
    nslot = 1; memset(slotpx[0], 0, XT * XT * 4); memset(sk_glyph, 0, sizeof sk_glyph); memset(sk_fs, 0, sizeof sk_fs); memset(sk_ws, 0, sizeof sk_ws); memset(sk_cdep, 0, sizeof sk_cdep);
    if (!a5 || !wb) { fprintf(stderr, PET_OP ": missing RPG Maker sheets under %s\n", ASSETS); return; }
    unsigned char t[XT * XT * 4], u[XT * XT * 4], v[XT * XT * 4];
    /* the island page: grass floor everywhere, then the cells by glyph */
    tile24(a5, 0, 2 * S48, t); int grass = slot_of(t);
    for (int y = 0; y < SK_H; y++) for (int x = 0; x < SK_W; x++) { char c = sk_isl_at(x, y); sk_glyph[0][y][x] = c; sk_fs[0][y][x] = grass; sk_ws[0][y][x] = 0;
        switch (c) {
        case '.': sk_glyph[0][y][x] = '_'; sk_fs[0][y][x] = 0; break;      /* the void is a hole: the sky shows through */
        case '~': sk_solid(0, x, y, '~', 50, 110, 200, 0, 0); break;
        case 'p': sk_solid(0, x, y, 'p', 214, 190, 130, 0, 0); break;
        case 'r': sk_solid(0, x, y, 'r', 190, 165, 110, 0, 0); break;
        case 'b': case 's': sk_solid(0, x, y, c, 150, 100, 55, 1, 0); break;
        case 'T': case 'B': case 'D': case 'S': case 'F': sk_glyph[0][y][x] = 'G'; break;
        } }
    for (int i = 0; i < 5; i++) sk_stamp(0, wb, sk_bld[i].bx, sk_bld[i].by, 2, 2, sk_bld[i].x, sk_bld[i].y, 'B', 1);
    sk_ndoor = 0;
    for (int y = 0; y < SK_H; y++) for (int x = 0; x < SK_W; x++) { char c = sk_isl_at(x, y);
        if (c == 'T' && ob) sk_stamp(0, ob, (x + y) & 1, 0, 1, 1, x, y, 'T', 1);
        else if (c == 'T') sk_solid(0, x, y, 'T', 40, 120, 50, 0, 1);
        else if (c == 'S') sk_solid(0, x, y, 'S', 130, 90, 50, 0, 1);
        else if (c == 'F') sk_solid(0, x, y, 'F', 230, 110, 30, 0, 1);
        else if (c == 'D' && sk_ndoor < 8) {
            int best = 0, bd = 1 << 30; for (int b = 0; b < 5; b++) { int dx = x - (sk_bld[b].x + 1), dy = y - (sk_bld[b].y + 2), d2 = dx * dx + dy * dy; if (d2 < bd) { bd = d2; best = b; } }
            snprintf(sk_bname[sk_ndoor], sizeof sk_bname[0], "%s", sk_bld[best].name); sk_doorx[sk_ndoor] = x; sk_doory[sk_ndoor] = y; sk_ndoor++;
            if (door) { tile24(door, 0, 192, u); memcpy(v, slotpx[sk_fs[0][y][x]], sizeof v); over24(v, u); sk_glyph[0][y][x] = 'D'; sk_ws[0][y][x] = slot_of(v); } else sk_glyph[0][y][x] = 'D'; } }
    sk_taper();
    if (a4) { tile24(a4, 0, 13 * S48, t); sk_rockslot = slot_of(t); }      /* the rocky-earth cliff face (Outside_A4 row 13): the hung blocks under the islands */
    /* the interiors: a walled room, the floor tile of rooms.pdl for this room (an Inside_A5 cell), one door out in the middle of the south wall row */
    Sheet *fl = in ? in : a5; unsigned char wallpx[XT * XT * 4]; tile24(fl, S48, 3 * S48, wallpx); int wallslot = slot_of(wallpx);
    for (int g = 1; g < nroom && g < SK_NR; g++) {
        unsigned char ft[XT * XT * 4]; tile24(fl, rooms[g].fc * S48, rooms[g].fr * S48, ft); int fsl = slot_of(ft);
        for (int y = 0; y < SK_H; y++) { for (int x = 0; x < SK_W; x++) { sk_glyph[g][y][x] = '.'; sk_fs[g][y][x] = fsl; sk_ws[g][y][x] = 0; } sk_glyph[g][y][SK_W] = 0; }
        for (int x = 0; x < SK_W; x++) { sk_glyph[g][0][x] = sk_glyph[g][1][x] = 'W'; sk_ws[g][0][x] = sk_ws[g][1][x] = wallslot; }
        for (int y = 0; y < SK_H; y++) { for (int x = SK_IW - 1; x < SK_W; x++) { sk_glyph[g][y][x] = 'W'; sk_ws[g][y][x] = wallslot; } sk_glyph[g][y][0] = 'W'; sk_ws[g][y][0] = wallslot; }
        sk_glyph[g][SK_H - 2][SK_IW / 2] = 'D';
        if (door) { tile24(door, 0, 192, u); memcpy(v, slotpx[fsl], sizeof v); over24(v, u); sk_ws[g][SK_H - 2][SK_IW / 2] = slot_of(v); }
    }
    /* where a pet comes out of a door: the first walkable cell beside the door (down, left, right, up) */
    for (int k = 0; k < sk_ndoor; k++) { static const int ox[4] = {0, -1, 1, 0}, oy[4] = {1, 0, 0, -1}; sk_arrx[k] = sk_doorx[k]; sk_arry[k] = sk_doory[k];
        for (int q = 0; q < 4; q++) { int cx = sk_doorx[k] + ox[q], cy = sk_doory[k] + oy[q]; if (!sk_blocked(0, cx, cy) && sk_glyph[0][cy][cx] != 'D') { sk_arrx[k] = cx; sk_arry[k] = cy; break; } } }
    /* wide islands: this map sets its own fog (the house default fades everything past 48 cells to sky), and the xelector may go down into the hung rock */
    { char vp[PATH_MAX], buf[4096] = ""; snprintf(vp, sizeof vp, "%s/pieces/system/view.pdl", PCHQ); read_file(vp, buf, sizeof buf); FILE *vf;
      if (!strstr(buf, "view_fog_end") && (vf = fopen(vp, "a"))) { fputs("# the island map is wide: fog from 90 to 250 cells, not the house 18..48\nview_fog_start=90\nview_fog_end=250\n", vf); fclose(vf); }
      if (!strstr(buf, "xelector_z_min") && (vf = fopen(vp, "a"))) { fputs("# the hung rock is up to 9 levels deep: z / x may take the xelector down into it (levels relative to the floor)\nxelector_z_min=-8\n", vf); fclose(vf); } }
}

/* the cached background of one room: sky behind the void cells (the parallax of the phase, tiled in pixels), then the floor slot and the wall / object slot of each cell, at 24 px per cell. Written into fr (the 2D frame) at OX,OY. */
static unsigned char *sk_bg[SK_NR]; static char sk_bgkey[SK_NR][24];
static void sk_paint_bg(int room, const char *phase) {
    sk_ensure_map(); if (room < 0 || room >= nroom || room >= SK_NR) return;
    Room *rm = &rooms[room]; int W = rm->cols * XT, H = rm->rows * XT; char key[24]; snprintf(key, sizeof key, "%s", rm->outdoor ? phase : "-");
    if (!sk_bg[room] || strcmp(sk_bgkey[room], key)) {
        free(sk_bg[room]); sk_bg[room] = (unsigned char *)malloc((size_t)W * H * 4); if (!sk_bg[room]) return;
        Sheet *sky = rm->outdoor ? sheet("parallaxes", pet_sky_sheet(phase)) : NULL;
        for (int y = 0; y < rm->rows; y++) for (int x = 0; x < rm->cols; x++) for (int py = 0; py < XT; py++) for (int px = 0; px < XT; px++) {
            unsigned char c[3] = {120, 170, 220}; int ax = x * XT + px, ay = y * XT + py;
            if (sky && sky->px) { const unsigned char *q = sky->px + ((size_t)(ay % sky->h) * sky->w + ax % sky->w) * 4; c[0] = q[0]; c[1] = q[1]; c[2] = q[2]; }
            for (int layer = 0; layer < 2; layer++) { int sl = layer ? sk_ws[room][y][x] : sk_fs[room][y][x]; if (sl <= 0) continue; const unsigned char *q = slotpx[sl] + (py * XT + px) * 4; int al = q[3]; if (!al) continue;
                for (int k = 0; k < 3; k++) c[k] = (unsigned char)((q[k] * al + c[k] * (255 - al)) / 255); }
            unsigned char *o = sk_bg[room] + ((size_t)ay * W + ax) * 4; o[0] = c[0]; o[1] = c[1]; o[2] = c[2]; o[3] = 255; }
        snprintf(sk_bgkey[room], sizeof sk_bgkey[room], "%s", key);
    }
    for (int y = 0; y < rm->rows * T; y++) { int sy = y * XT / T; if (OY + y < 0 || OY + y >= FH) continue;
        for (int x = 0; x < rm->cols * T; x++) { if (OX + x < 0 || OX + x >= FW) continue; memcpy(fr + ((size_t)(OY + y) * FW + OX + x) * 4, sk_bg[room] + ((size_t)sy * W + x * XT / T) * 4, 4); } }
}

/* ---- the pets: what each looks like. party.pdl PET rows (name | sheet | index | art id | letter | r | g | b | x | y); the skin reads the picture, spawn and colour columns ---- */
static struct { char name[24], art[48]; char letter; int r, g, b, x, y; } sk_pt[NP]; static int sk_npt;
static int sk_row(const char *name) { for (int k = 0; k < sk_npt; k++) if (!strcmp(sk_pt[k].name, name)) return k; return -1; }
static void sk_spawn(int i, int *x, int *y) {
    if (i < 0) { *x = 12; *y = 13; return; }      /* the trainer: south of the cabin */
    int k = i < nparty ? sk_row(party[i].name) : -1; if (k >= 0) { *x = sk_pt[k].x; *y = sk_pt[k].y; } else { *x = 9 + i % 5; *y = 13 + (i / 5) % 3; }
}
/* the pet's 48x48 picture: its art csv (64x64) box-filtered, or a coloured disc */
static void sk_pic(int i, unsigned char *out, int *avg) {
    int k = sk_row(party[i].name); char cp[PATH_MAX]; unsigned char big[64 * 64 * 4];
    if (k >= 0 && sk_pt[k].art[0] && strcmp(sk_pt[k].art, "none")) { snprintf(cp, sizeof cp, "%s/art/%s.csv", APP, sk_pt[k].art);
        if (csvf_load64(cp, big)) { for (int y = 0; y < 48; y++) for (int x = 0; x < 48; x++) { int acc[4] = {0}, w = 0, x0 = x * 64 / 48, x1 = (x + 1) * 64 / 48, y0 = y * 64 / 48, y1 = (y + 1) * 64 / 48; if (x1 <= x0) x1 = x0 + 1; if (y1 <= y0) y1 = y0 + 1;
                for (int yy = y0; yy < y1; yy++) for (int xx = x0; xx < x1; xx++) { unsigned char *q = big + (yy * 64 + xx) * 4; acc[0] += q[0] * q[3]; acc[1] += q[1] * q[3]; acc[2] += q[2] * q[3]; acc[3] += q[3]; w++; }
                unsigned char *o = out + (y * 48 + x) * 4; if (acc[3]) { o[0] = (unsigned char)(acc[0] / acc[3]); o[1] = (unsigned char)(acc[1] / acc[3]); o[2] = (unsigned char)(acc[2] / acc[3]); o[3] = (unsigned char)(acc[3] / w); } else o[0] = o[1] = o[2] = o[3] = 0; }
            sk_art[i] = 1; return; } }
    sk_art[i] = 0; int r = k >= 0 ? sk_pt[k].r : 60 + (i * 53) % 180, g = k >= 0 ? sk_pt[k].g : 60 + (i * 97) % 180, b = k >= 0 ? sk_pt[k].b : 60 + (i * 29) % 180; if (avg) { avg[0] = r; avg[1] = g; avg[2] = b; }
    for (int y = 0; y < 48; y++) for (int x = 0; x < 48; x++) { int dx = x - 24, dy = y - 24, in = dx * dx + dy * dy <= 20 * 20; unsigned char *o = out + (y * 48 + x) * 4; o[0] = (unsigned char)r; o[1] = (unsigned char)g; o[2] = (unsigned char)b; o[3] = in ? 255 : 0; }
}
/* party.pdl -> sk_pt; then the character sheets !edit<g>.png (12 x 8 cells of 48 px, the pet's picture in every direction and walk frame) and, for art pets, the 3D walk frames. Rewritten only when the roster, a picture
 * or an art file changed (state/sheets_sig.txt): every verb process calls this, so an unchanged roster costs a few stat() calls. */
static void sk_pets_sync(void) {
    char p[PATH_MAX], line[400]; char *f[12]; snprintf(p, sizeof p, "%s/party.pdl", APP); FILE *fp = fopen(p, "r"); sk_npt = 0;
    while (fp && fgets(line, sizeof line, fp) && sk_npt < NP) { int n = split_bar(line, f, 12); if (n < 5 || strcmp(f[0], "PET")) continue;
        snprintf(sk_pt[sk_npt].name, 24, "%s", f[1]); snprintf(sk_pt[sk_npt].art, 48, "%s", f[4]); sk_pt[sk_npt].letter = n > 5 && f[5][0] ? f[5][0] : 'p';
        sk_pt[sk_npt].r = n > 6 ? atoi(f[6]) : 128; sk_pt[sk_npt].g = n > 7 ? atoi(f[7]) : 128; sk_pt[sk_npt].b = n > 8 ? atoi(f[8]) : 128; sk_pt[sk_npt].x = n > 9 ? atoi(f[9]) : 9; sk_pt[sk_npt].y = n > 10 ? atoi(f[10]) : 13; sk_npt++; }
    if (fp) fclose(fp);
    unsigned long sig = 5381; for (int i = 0; i < nparty; i++) { int k = sk_row(party[i].name); const char *a = k >= 0 ? sk_pt[k].art : ""; struct stat st; char cp[PATH_MAX]; snprintf(cp, sizeof cp, "%s/art/%s.csv", APP, a);
        for (const char *c = party[i].name; *c; c++) sig = sig * 33 + (unsigned char)*c; for (const char *c = a; *c; c++) sig = sig * 33 + (unsigned char)*c; if (k >= 0) sig = sig * 33 + (unsigned long)(sk_pt[k].r * 7 + sk_pt[k].g * 3 + sk_pt[k].b);
        if (a[0] && stat(cp, &st) == 0) sig = sig * 33 + (unsigned long)st.st_size + (unsigned long)st.st_mtime; }
    char want[40], have[40], sp[PATH_MAX], d0[PATH_MAX]; snprintf(want, sizeof want, "%lu\n", sig); spath(sp, "sheets_sig.txt"); read_file(sp, have, sizeof have);
    snprintf(d0, sizeof d0, "%s/../tsots-characters/!edit0.png", ASSETS); int fresh = !strcmp(want, have) && fexists(d0);
    char fd[PATH_MAX]; snprintf(fd, sizeof fd, "%s/../tsots-characters", ASSETS); mkdir(fd, 0755); snprintf(fd, sizeof fd, "%s/../tsots-characters/frames", ASSETS); mkdir(fd, 0755);
    static unsigned char pic[48 * 48 * 4], sheetpx[576 * 384 * 4];
    for (int g = 0; g <= (nparty - 1) / 8; g++) {
        if (!fresh) memset(sheetpx, 0, sizeof sheetpx);
        for (int k = 0; k < 8 && g * 8 + k < nparty; k++) { int i = g * 8 + k, avg[3] = {128, 128, 128};
            if (fresh) { int kk = sk_row(party[i].name); char cp[PATH_MAX]; snprintf(cp, sizeof cp, "%s/art/%s.csv", APP, kk >= 0 ? sk_pt[kk].art : "none"); sk_art[i] = kk >= 0 && strcmp(sk_pt[kk].art, "none") && fexists(cp); continue; }      /* unchanged roster: only note which pets have art */
            sk_pic(i, pic, avg);
            for (int pat = 0; pat < 3; pat++) for (int row = 0; row < 4; row++) { int sx = ((k % 4) * 3 + pat) * 48, sy = ((k / 4) * 4 + row) * 48; for (int y = 0; y < 48; y++) memcpy(sheetpx + ((size_t)(sy + y) * 576 + sx) * 4, pic + y * 48 * 4, 48 * 4); }
            if (sk_art[i]) { int kk = sk_row(party[i].name); char cp[PATH_MAX], csn[16]; snprintf(cp, sizeof cp, "%s/art/%s.csv", APP, sk_pt[kk].art); snprintf(csn, sizeof csn, "!edit%d", g); csv_write_frames(cp, fd, csn, k, avg); } }
        if (!fresh) { char pp[PATH_MAX + 16], tp[PATH_MAX + 32], nm[16]; snprintf(pp, sizeof pp, "%s/../tsots-characters/!edit%d.png", ASSETS, g); snprintf(tp, sizeof tp, "%s.tmp", pp); snprintf(nm, sizeof nm, "!edit%d", g);
            if (stbi_write_png(tp, 576, 384, 4, sheetpx, 576 * 4)) rename(tp, pp); else remove(tp); sk_drop_sheet(nm); }
    }
    if (!fresh) write_if_changed(sp, want, strlen(want));
}

/* ---- the 3D export of one room (called by export3d for every room instead of the common painter): map.txt, extrusion.pdl, the 24 px tile atlas and cells.txt with the hung depth of every cell and the rock slot ---- */
static void pet_own_export_room(int r, const char *d) {
    sk_ensure_map(); if (r < 0 || r >= nroom || r >= SK_NR) return;
    int cols = rooms[r].cols, rows = rooms[r].rows; char p[PATH_MAX];
    static char gl[SK_H][SK_W + 1]; static int fs[SK_H][SK_W], ws[SK_H][SK_W]; memcpy(gl, sk_glyph[r], sizeof gl); memcpy(fs, sk_fs[r], sizeof fs); memcpy(ws, sk_ws[r], sizeof ws);
    for (int i = 0; i < npl; i++) if (pl[i].room == r) {      /* furniture the house placed: a 1 high block with the item's tile on the floor tile, like the common painter */
        Item *it = &cat[pl[i].item]; Sheet *s = sheet("tilesets", it->sheet); unsigned char a[XT * XT * 4], b[XT * XT * 4];
        for (int ci = 0; ci < it->w; ci++) for (int cj = 0; cj < it->h; cj++) { int x = pl[i].x + ci, y = pl[i].y + cj; if (x < 0 || x >= cols || y < 0 || y >= rows) continue;
            tile24(s, (it->c + ci) * S48, (it->r + cj) * S48, a); memset(b, 0, sizeof b); if (fs[y][x]) memcpy(b, slotpx[fs[y][x]], sizeof b); over24(b, a); gl[y][x] = 'f'; ws[y][x] = slot_of(b); } }
    snprintf(p, sizeof p, "%s/map.txt", d); { FILE *f = fopen_tmp(p); if (f) { for (int y = 0; y < rows; y++) fprintf(f, "%.*s\n", cols, gl[y]); fclose_tmp(f, p); } }
    snprintf(p, sizeof p, "%s/extrusion.pdl", d); { FILE *f = fopen_tmp(p); if (f) { fprintf(f, "SECTION      | KEY                | VALUE\n----------------------------------------------------------------------\nMETA         | map_id             | " PET_APP "\nMETA         | desk_id            | %s\nEXTRUDE      | W                  | 3\nEXTRUDE      | D                  | 3\nEXTRUDE      | T                  | 2\nEXTRUDE      | f                  | 1\nEXTRUDE      | B                  | 3\nEXTRUDE      | default            | 0\n", rooms[r].id); fclose_tmp(f, p); } }
    int acols = 16, nrows = (nslot + acols - 1) / acols, aw = acols * XT, ah = nrows * XT; unsigned char *atlas = (unsigned char *)calloc((size_t)aw * ah * 4, 1); if (!atlas) return;
    for (int i = 0; i < nslot; i++) for (int y = 0; y < XT; y++) memcpy(atlas + ((size_t)((i / acols) * XT + y) * aw + (i % acols) * XT) * 4, slotpx[i] + y * XT * 4, XT * 4);
    snprintf(p, sizeof p, "%s/cells.rgba", d); { FILE *f = fopen_tmp(p); if (f) { fwrite(atlas, 1, (size_t)aw * ah * 4, f); fclose_tmp(f, p); } } snprintf(p, sizeof p, "%s/cells.png", d); png_atomic(p, aw, ah, atlas, aw * 4); free(atlas);
    snprintf(p, sizeof p, "%s/cells.txt", d); { FILE *f = fopen_tmp(p); if (f) { fprintf(f, "width=%d\nheight=%d\ntile_px=%d\natlas_cols=16\natlas_tiles=%d\nrock_slot=%d\ncells\n", cols, rows, XT, nslot, sk_rockslot);
        for (int y = 0; y < rows; y++) { for (int x = 0; x < cols; x++) fprintf(f, "%s%d,%d,%d", x ? " " : "", fs[y][x], ws[y][x], sk_cdep[r][y][x]); fputc('\n', f); } fclose_tmp(f, p); } }
}
#elif PET_SECTION == 74
    { int sx, sy; sk_spawn(i, &sx, &sy); e->room = 0; e->x = sx; e->y = sy; }
#endif
#endif
