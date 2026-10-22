/* rpg_pet.c - rpg-pet on the shared pets-like core (&.widgits/_shared-lib/petslike_core.c). Two roles in one file: the top builds the program, and the core
 * re-includes this file once per SECTION to pull in what is specific to this game. Design: @.apps/../#.#.calendar-dox/!.HQ-IQ-BOOK/16.game/PETSLIKE-FRAMEWORK-PLAN.md */
#ifndef PET_SECTION
#define PET_APP "rpg-pet"
#define PET_OP "rpg_pet"
#define PET_SKIN "rpg_pet.c"
#include "../../../&.widgits/_shared-lib/petslike_core.c"
#else
#if PET_SECTION == 1      /* core line 21 */
 * Every picture is an RPG Maker MV tile or character frame (sheets under #.NNEST_ASSETS/rmmv-www-img), nothing else is drawn.
#elif PET_SECTION == 2      /* core line 52 */
#elif PET_SECTION == 3      /* core line 54 */
#define T 48
#elif PET_SECTION == 4      /* core line 63 */
#define COLS 13
#define ROWS 8
#elif PET_SECTION == 5      /* core line 68 */
#define TRIG_ROW 2
#elif PET_SECTION == 6      /* core line 72 */
#define NP 8      /* the party: up to 6 DB monsters + the pals made with "+ new pal" (pals.pdl); 8 = the 8 Monster.png pictures */
#elif PET_SECTION == 7      /* core line 75 */
typedef struct { char id[16], name[32]; int fc, fr, outdoor; } Room;
#elif PET_SECTION == 8      /* core line 77 */
typedef struct { int room, tx, dest, ax, ay, autow; } Door;
#elif PET_SECTION == 9      /* core line 83 */
#define SIBLING_APP "/@.apps/" "dsr-rpg" "/"      /* the carbon-copy game that shares the one pc-hq engine + world: its board window is NOT a viewer of this game (see viewer_kinds); the copy sets this to the other game */
#elif PET_SECTION == 10      /* core line 89 */
#elif PET_SECTION == 11      /* core line 90 */
static Sheet sheets[12]; static int nsheet;
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
#elif PET_SECTION == 18      /* core line 209 */
    nparty = 0; char names[NP][24]; int nn = 0, inparty = 0, inhero = 0; memset(&hero, 0, sizeof hero); snprintf(hero.name, 24, "Harold"); snprintf(hero.sheet, 24, "Actor1");
#elif PET_SECTION == 19      /* core line 211 */
    if ((fp = fopen(p, "r"))) { while (fgets(line, sizeof line, fp)) { if (split_bar(line, f, 3) < 3 || strcmp(f[0], "SYSTEM")) continue; if (!strcmp(f[1], "name")) { inparty = !strcmp(f[2], getenv("PET_PARTY") && getenv("PET_PARTY")[0] ? getenv("PET_PARTY") : "RpgPetParty"); inhero = !strcmp(f[2], "Party"); } else if (inhero && !strcmp(f[1], "member1")) snprintf(hero.name, 24, "%s", f[2]); else if (inparty && !strncmp(f[1], "member", 6) && nn < NP) snprintf(names[nn++], 24, "%s", f[2]); } fclose(fp); }
    for (int i = 0; i < nn; i++) { memset(&party[nparty], 0, sizeof party[nparty]); snprintf(party[nparty].name, 24, "%s", names[i]); snprintf(party[nparty].id, 16, "%s", names[i]); for (char *c = party[nparty].id; *c; c++) if (*c >= 'A' && *c <= 'Z') *c += 32; party[nparty].idx = i; snprintf(party[nparty].sheet, 24, "Monster"); nparty++; }
#elif PET_SECTION == 20      /* core line 217 */
        party[nparty].idx = atoi(g[3]) & 7; snprintf(party[nparty].sheet, 24, "Monster"); party[nparty].mhp = 40; party[nparty].mmp = 20; party[nparty].atk = party[nparty].def = party[nparty].agi = party[nparty].luk = 10; party[nparty].mat = party[nparty].mdf = 8; nparty++; } fclose(fp); }
#elif PET_SECTION == 21      /* core line 249 */
            Door *d = &cache[ncache++]; d->room = a; d->tx = tx; d->dest = b; d->ax = ax; d->ay = ay; d->autow = au; }
#elif PET_SECTION == 22      /* core line 258 */
#elif PET_SECTION == 23      /* core line 260 */
        while (fgets(line, sizeof line, fp)) { int n = split_bar(line, f, 12); if (n >= 5 && !strcmp(f[0], "ROOM") && nroom < 8) { Room *r = &rooms[nroom++]; snprintf(r->id, 16, "%s", f[1]); snprintf(r->name, 32, "%s", f[2]); r->fc = atoi(f[3]); r->fr = atoi(f[4]); r->outdoor = n >= 6 && !strcmp(f[5], "outdoor"); } }
#elif PET_SECTION == 24      /* core line 263 */
        while (fgets(line, sizeof line, fp)) { int n = split_bar(line, f, 12); if (n >= 7 && !strcmp(f[0], "DOOR") && ndoor < 24) { int a = room_idx(f[1]), b = room_idx(f[3]); if (a < 0 || b < 0) continue; Door *d = &doors[ndoor++]; d->room = a; d->tx = atoi(f[2]); d->dest = b; d->ax = atoi(f[4]); d->ay = atoi(f[5]); d->autow = atoi(f[6]); } }
#elif PET_SECTION == 25      /* core line 266 */
#elif PET_SECTION == 26      /* core line 268 */
#elif PET_SECTION == 27      /* core line 279 */
#elif PET_SECTION == 28      /* core line 281 */
    char p[PATH_MAX], line[300]; spath(p, "house.txt");
#elif PET_SECTION == 29      /* core line 299 */
    e->room = 0; e->x = i < 0 ? 2 : 4 + i; e->y = i < 0 ? 6 : 5; e->dir = 0; e->steps = 0; e->tx = e->ty = -1;
#elif PET_SECTION == 30      /* core line 302 */
    { int cw = 13, ch = 8;
#elif PET_SECTION == 31      /* core line 313 */
static void use_ent(int i) { me = i; pet = i < 0 ? tr : others[i]; }
#elif PET_SECTION == 32      /* core line 316 */
    write_if_changed(p, b, strlen(b)); if (me < 0) tr = pet; else others[me] = pet; calc_view();
#elif PET_SECTION == 33      /* core line 326 */
static int floor_blocked(int room, int x, int y, int skip_id) { for (int i = 0; i < nscen; i++) if (scen[i].room == room && x >= scen[i].x && x < scen[i].x + scen[i].w && y >= scen[i].y && y < scen[i].y + scen[i].h) return 9999; for (int i = 0; i < npl; i++) { Placed *q = &pl[i]; if (q->room != room || q->id == skip_id || cat[q->item].wall) continue; if (x >= q->x && x < q->x + cat[q->item].w && y >= q->y && y < q->y + cat[q->item].h) return q->id; } return 0; }
static int door_here(int room, int x, int y) { for (int i = 0; i < ndoor; i++) if (doors[i].room == room && doors[i].tx == x && y == TRIG_ROW) return i; return -1; }
#elif PET_SECTION == 34      /* core line 333 */
    static char m[120]; Item *d = &cat[item];
#elif PET_SECTION == 35      /* core line 351 */
static int first_free(int room, int item, int *ox, int *oy) { Item *d = &cat[item]; if (d->wall) { int wy = WALL_ROW + 1 - d->h; for (int x = 0; x < COLS; x++) if (!check_place(room, item, x, wy, 0)[0]) { *ox = x; *oy = wy; return 1; } return 0; } for (int y = TRIG_ROW; y < ROWS; y++) for (int x = 0; x < COLS; x++) if (!check_place(room, item, x, y, 0)[0]) { *ox = x; *oy = y; return 1; } return 0; }
#elif PET_SECTION == 36      /* core line 356 */
    if (nsheet >= 12) return NULL;
#elif PET_SECTION == 37      /* core line 360 */
static void blit(Sheet *s, int sx, int sy, int w, int h, int dx, int dy) {
    if (!s || w < 0 || h < 0 || w > 4 * FW || h > 4 * FH) return;
    for (int y = 0; y < h; y++) { int Y = dy + y; if (Y < 0 || Y >= FH || sy + y >= s->h) continue;
        for (int x = 0; x < w; x++) { int X = dx + x; if (X < 0 || X >= FW || sx + x >= s->w) continue;
            unsigned char *a = s->px + ((size_t)(sy + y) * s->w + sx + x) * 4, *b = fr + ((size_t)Y * FW + X) * 4; int al = a[3]; if (!al) continue;
#elif PET_SECTION == 38      /* core line 367 */
static void tile(const char *sh, int c, int r, int w, int h, int tx, int ty) { blit(sheet("tilesets", sh), c * T, r * T, w * T, h * T, OX + tx * T, OY + ty * T); }

#elif PET_SECTION == 39      /* core line 456 */
#elif PET_SECTION == 40      /* core line 458 */
    Room *rm = &rooms[vroom]; char phase[40] = "day"; { char dp[PATH_MAX], db[64]; spath(dp, "daylight.txt"); read_file(dp, db, sizeof db); if (strstr(db, "night")) snprintf(phase, sizeof phase, "night"); else if (strstr(db, "dusk")) snprintf(phase, sizeof phase, "dusk"); else if (strstr(db, "dawn")) snprintf(phase, sizeof phase, "dawn"); }
    if (rm->outdoor) {      /* sky = an RPG Maker parallax picked by the game clock; ground, path and trees are Outside tiles */
        Sheet *sk = sheet("parallaxes", !strcmp(phase, "night") ? "StarlitSky" : (!strcmp(phase, "dusk") || !strcmp(phase, "dawn")) ? "Sunset" : "CloudySky1"); if (sk) blit(sk, 0, !strcmp(phase, "day") ? 330 : 120, COLS * T, 2 * T, OX, OY);
        for (int x = 0; x < COLS; x++) { tile("Outside_A5", 2, 0, 1, 1, x, WALL_ROW); for (int y = TRIG_ROW; y < ROWS; y++) tile("Outside_A5", 0, 2, 1, 1, x, y); }
        for (int i = 0; i < ndoor; i++) if (doors[i].room == vroom) for (int y = TRIG_ROW; y < ROWS - 1; y++) tile("Outside_A5", 1, 2, 1, 1, doors[i].tx, y);
    } else
    for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLS; x++) { if (y == 0) tile("Inside_A5", 0, 0, 1, 1, x, y); else if (y == WALL_ROW) tile("Inside_A5", 1, 3, 1, 1, x, y); else tile("Inside_A5", rm->fc, rm->fr, 1, 1, x, y); }
    Sheet *dr = sheet("characters", "!Door1");                                  /* the wood panel door: closed frame of the 4th door object */
    for (int i = 0; i < ndoor; i++) if (doors[i].room == vroom) { blit(dr, 0, 192, T, T, OX + doors[i].tx * T, OY + WALL_ROW * T); if (!rm->outdoor) tile("Inside_A5", 0, 6, 1, 1, doors[i].tx, TRIG_ROW); }
    for (int i = 0; i < nscen; i++) if (scen[i].room == vroom) tile(scen[i].sheet, scen[i].c, scen[i].r, scen[i].w, scen[i].h, scen[i].x, scen[i].y);
#elif PET_SECTION == 41      /* core line 472 */
    for (int a = 0; a < n; a++) { Placed *p = &pl[idx[a]]; Item *d = &cat[p->item]; tile(d->sheet, d->c, d->r, d->w, d->h, p->x, p->y); }
#elif PET_SECTION == 42      /* core line 479 */
    for (int k = 0; k < (g_bg_only ? 0 : no); k++) { int i = order[k]; Ent *e = i < 0 ? &tr : &others[i]; int col = e->steps ? cyc[e->steps % 4] : 1, ci = i < 0 ? 0 : party[i].idx;
        blit(sheet("characters", i < 0 ? hero.sheet : party[i].sheet), (ci % 4) * 3 * T + col * T, (ci / 4) * 4 * T + e->dir * T, T, T, OX + e->x * T, OY + e->y * T - 8); }
#elif PET_SECTION == 43      /* core line 484 */
        if (tr_ || tg_ || tb_ || gy) for (int y = OY + (od ? 2 * T : 0); y < OY + ROWS * T; y++) for (int x = OX; x < OX + COLS * T; x++) { unsigned char *q = fr + ((size_t)y * FW + x) * 4; int r = q[0], g = q[1], b = q[2], l = (r * 30 + g * 59 + b * 11) / 100;
#elif PET_SECTION == 44      /* core line 489 */
      if (mr != 256 || mg != 256 || mb != 256) for (int y = OY + (od ? 2 * T : 0); y < OY + ROWS * T; y++) for (int x = OX; x < OX + COLS * T; x++) { unsigned char *q = fr + ((size_t)y * FW + x) * 4; q[0] = (unsigned char)(q[0] * mr / 256); q[1] = (unsigned char)(q[1] * mg / 256); q[2] = (unsigned char)(q[2] * mb / 256); } } }
#elif PET_SECTION == 45      /* core line 504 */
#elif PET_SECTION == 46      /* core line 513 */
    { static const char *dflt[4] = {"hmm, tell me more.", "I'm listening.", "interesting!", "okay, Harold."}; unsigned h = 0; for (const char *c = s; *c; c++) h = h * 31 + (unsigned char)*c;
#elif PET_SECTION == 47      /* core line 594 */
    if (pid == 0) { int fd = open("/dev/null", O_RDWR); if (fd >= 0) { dup2(fd, 0); dup2(fd, 1); dup2(fd, 2); } setenv("LC_CLOCK_NO_POPUP", "1", 1); if (daemon) setenv("LC_CLOCK_EVENT_RUNNER", SELF, 1); setenv("RPG_PET_ROOT", APP, 1); signal(SIGPIPE, SIG_DFL); execv(LC, av); _exit(127); }
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
    W("title=rpg-pet  -  %s  -  coins %d\nscene_raw=%s\ncanvas_raw=%s\ncoins=%d\nmsg=%s\nroom_label=%s\n", rooms[vroom].name, coins, raw, raw, coins, msg && msg[0] ? msg : (in ? "INT on: turn based like a roguelike. Time moves when you act. Tab switches pet/player." : "real time: the pets live on their own. INT lets you control a pet or the player."), rooms[vroom].name);
#elif PET_SECTION == 53      /* core line 672 */
    W("left_visible=%s\n", flag_on("left", 1) ? "1" : "");      /* the pinned info bar (default on, Menu > Info show / hide, its _ button) */
    W("chat_visible=%s\nhb_visible=%s\nview_label=view %s\nchat_toggle_label=window %s\n", chat ? "1" : "", hb ? "1" : "", view, chat ? "on" : "off");
    { char cp[PATH_MAX], cb[4000]; spath(cp, "chat.txt"); read_tail(cp, cb, sizeof cb); char *ln[6] = {0}; int k = 0; for (char *l = strtok(cb, "\n"); l; l = strtok(NULL, "\n")) { ln[k % 6] = l; k++; } for (int i = 0; i < 6; i++) { int j = k - 6 + i; W("chat_%d=%s\n", i, j >= 0 ? ln[j % 6] : ""); } }
#elif PET_SECTION == 54      /* core line 676 */
#elif PET_SECTION == 55      /* core line 688 */
#elif PET_SECTION == 56      /* core line 696 */
static int passable(int x, int y) { return x >= 0 && x < COLS && y >= TRIG_ROW && y < ROWS && !floor_blocked(pet.room, x, y, 0) && !other_at(pet.room, x, y); }
#elif PET_SECTION == 57      /* core line 700 */
    pet.room = d->dest; pet.x = d->ax; pet.y = d->ay; pet.dir = 0; pet.tx = pet.ty = -1; { int fx, fy; if (free_near(pet.x, pet.y, &fx, &fy)) { pet.x = fx; pet.y = fy; } }      /* land on the nearest FREE cell, not on whoever is already there */
    if (me >= 0 && me == active && (pet.room == vroom || di_from_room == vroom)) { snprintf(b, sizeof b, "went through the door to the %s", rooms[pet.room].name); say(party[me].name, b); }
#elif PET_SECTION == 58      /* core line 706 */
#elif PET_SECTION == 59      /* core line 717 */
#elif PET_SECTION == 60      /* core line 722 */
#elif PET_SECTION == 61      /* core line 726 */
    int dist[ROWS][COLS], qx[ROWS * COLS], qy[ROWS * COLS], h = 0, t = 0; for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLS; x++) dist[y][x] = -1;
    int fx[ROWS][COLS], fy[ROWS][COLS]; dist[ty][tx] = 0; qx[t] = tx; qy[t++] = ty; static const int mx[4] = {0, -1, 1, 0}, my[4] = {1, 0, 0, -1};
#elif PET_SECTION == 62      /* core line 734 */
    int r = rand() % 100;
#elif PET_SECTION == 63      /* core line 737 */
        else { int pick = -1; for (int i = 0; i < ndoor; i++) if (doors[i].room == pet.room && doors[i].autow && doors[i].dest == others[j].room) pick = i; if (pick < 0) for (int i = 0; i < ndoor; i++) if (doors[i].room == pet.room && doors[i].autow) pick = i; if (pick >= 0) { pet.tx = doors[pick].tx; pet.ty = TRIG_ROW; return; } } }
    if (r < 47) { int cand[8], nc = 0; for (int i = 0; i < ndoor; i++) if (doors[i].room == pet.room && doors[i].autow && nc < 8) cand[nc++] = i; if (nc) { Door *d = &doors[cand[rand() % nc]]; pet.tx = d->tx; pet.ty = TRIG_ROW; return; } }
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
#elif PET_SECTION == 72      /* core line 782 */
#elif PET_SECTION == 73      /* core line 795 */
#elif PET_SECTION == 74      /* core line 799 */
    if (!s) return; int fx = (idx % 4) * 3 * T + T + T / 2, fy = (idx / 4) * 4 * T + dir4 * T + T / 2; if (fx < s->w && fy < s->h) memcpy(px, s->px + ((size_t)fy * s->w + fx) * 4, 4);
#elif PET_SECTION == 75      /* core line 806 */
    static unsigned char crop[COLS * T * ROWS * T * 4]; unsigned long sum = 5381; for (int y = 0; y < ROWS * T; y++) memcpy(crop + (size_t)y * COLS * T * 4, fr + ((size_t)(OY + y) * FW + OX) * 4, COLS * T * 4);
#elif PET_SECTION == 76      /* core line 808 */
    if (r >= 0 && r < 16 && sum != last[r]) { last[r] = sum; char d[PATH_MAX], p[PATH_MAX]; snprintf(d, sizeof d, "%s/@.apps/piececraft-hq/pieces/system/maps/rpg-pet/%s", HOUSE, rooms[r].id); mkdir(d, 0755); snprintf(p, sizeof p, "%s/map.png", d); png_atomic(p, COLS * T, ROWS * T, crop, COLS * T * 4); }
#elif PET_SECTION == 77      /* core line 832 */
    snprintf(d, sizeof d, "%s/@.apps/piececraft-hq/pieces/system/maps/rpg-pet/%s", HOUSE, rooms[r].id); mkdir(d, 0755);
    for (int i = 0; i < nparty; i++) if (others[i].room == r) { int pat = others[i].steps % 3; write_frame(party[i].sheet, party[i].idx, others[i].dir, pat); unsigned char px[4] = {128, 128, 128, 255}; frame_rgb(sheet("characters", party[i].sheet), party[i].idx, others[i].dir & 3, px);
        o += snprintf(buf + o, sizeof buf - o, "%d %d %d %d %d %s %d %d %d %c\n", others[i].x, others[i].y, px[0], px[1], px[2], party[i].sheet, party[i].idx, dc[others[i].dir & 3], pat, party[i].name[0] >= 'A' && party[i].name[0] <= 'Z' ? party[i].name[0] + 32 : (party[i].name[0] ? party[i].name[0] : 'm')); }
    if (tr.room == r) { int pat = tr.steps % 3; write_frame(hero.sheet, 0, tr.dir, pat); unsigned char px[4] = {200, 60, 60, 255}; frame_rgb(sheet("characters", hero.sheet), 0, tr.dir & 3, px);
        o += snprintf(buf + o, sizeof buf - o, "%d %d %d %d %d %s 0 %d %d @\n", tr.x, tr.y, px[0], px[1], px[2], hero.sheet, dc[tr.dir & 3], pat); }
#elif PET_SECTION == 78      /* core line 840 */
    static const char leg[] = "W|3|150|150|160|-|wall|#\nD|3|170|110|50|-|door|+\nT|2|50|150|60|-|tree|T\nf|1|190|150|90|-|furniture|&\n.|0|90|80|60|-|floor|.\n";
#elif PET_SECTION == 79      /* core line 842 */
    { char bp[PATH_MAX]; snprintf(bp, sizeof bp, "%s/@.apps/piececraft-hq/pieces/system/maps/rpg-pet/view.pdl", HOUSE); char vw[PATH_MAX + 160]; char sc[PATH_MAX]; spath(sc, "scene.raw"); int vn = snprintf(vw, sizeof vw, "VIEW | cell_px | 48\nVIEW | default_style | scene\nVIEW | scene_raw | %s\nVIEW | scene_w | %d\nVIEW | scene_h | %d\n", sc, FW, FH); write_if_changed(bp, vw, (size_t)vn); }      /* the engine draws this book's 2D at rpg-pet's own 48 px tile (house desk_grid.pdl says 80) */
#elif PET_SECTION == 80      /* core line 868 */
static int viewer_kinds(void) {
    static long last = 0; static int v = 3; long t = now_ms(); if (last && t - last < tune("viewer_scan_ms", 2000)) return v; last = t; v = 0;
#elif PET_SECTION == 81      /* core line 879 */
        if (!strstr(b, "khtpm_core_render")) continue; for (size_t i = 0; i < n; i++) if (!b[i]) b[i] = ' '; if (strstr(b, "pchq-board.xhtpm") && !strstr(b, SIBLING_APP)) v |= 1; if (strstr(b, "rpg-pet.xhtpm")) v |= 2; }
#elif PET_SECTION == 82      /* core line 896 */
#elif PET_SECTION == 83      /* core line 897 */
    char wp[PATH_MAX], xp[PATH_MAX], hp[PATH_MAX], m[64] = "", dk[64] = ""; snprintf(wp, sizeof wp, "%s/@.apps/piececraft-hq/pieces/world_01/state.txt", HOUSE);
#elif PET_SECTION == 84      /* core line 902 */
#elif PET_SECTION == 85      /* core line 903 */
    snprintf(xp, sizeof xp, "%s/@.apps/piececraft-hq/pieces/xelector_01/state.txt", HOUSE); snprintf(hp, sizeof hp, "%s/@.apps/piececraft-hq/pieces/hero_01/state.txt", HOUSE);
#elif PET_SECTION == 86      /* core line 908 */
    if (strcmp(dk, rooms[e->room].id) && lw != e->room) { lw = e->room; char ib[PATH_MAX], c[96]; snprintf(ib, sizeof ib, "%s/@.apps/piececraft-hq/pieces/system/widget_cmds/inbox.txt", HOUSE); snprintf(c, sizeof c, "CONFIRM_SET_DESK:%s", rooms[e->room].id); FILE *f = fopen(ib, "a"); if (f) { fprintf(f, "%s\n", c); fclose(f); } changed = 1; }
#elif PET_SECTION == 87      /* core line 921 */
    char root[PATH_MAX], d[PATH_MAX], p[PATH_MAX]; snprintf(root, sizeof root, "%s/@.apps/piececraft-hq/pieces/system/maps/rpg-pet", HOUSE);
    { char a[PATH_MAX]; snprintf(a, sizeof a, "%s/@.apps/piececraft-hq/pieces/system/maps", HOUSE); mkdir(a, 0755); } mkdir(root, 0755);
#elif PET_SECTION == 88      /* core line 927 */
        char glyph[ROWS][COLS + 1]; int fs[ROWS][COLS], ws[ROWS][COLS]; unsigned char a[XT * XT * 4], b[XT * XT * 4], fl[XT * XT * 4]; nslot = 1; memset(slotpx[0], 0, XT * XT * 4);
        Room *rm = &rooms[r]; Sheet *a5i = sheet("tilesets", "Inside_A5"), *a5o = sheet("tilesets", "Outside_A5"), *dr = sheet("characters", "!Door1");
#elif PET_SECTION == 89      /* core line 930 */
        if (rm->outdoor) { tile24(a5o, rm->fc * T, rm->fr * T, floorpx); tile24(a5o, 2 * T, 0, wallpx); memcpy(ceilpx, wallpx, sizeof ceilpx); tile24(a5o, 1 * T, 2 * T, pathpx); }
        else { tile24(a5i, rm->fc * T, rm->fr * T, floorpx); tile24(a5i, 1 * T, 3 * T, wallpx); tile24(a5i, 0, 0, ceilpx); memcpy(pathpx, floorpx, sizeof pathpx); }
        tile24(dr, 0, 192, doorpx); tile24(a5i, 0, 6 * T, matpx);
#elif PET_SECTION == 90      /* core line 933 */
        for (int x = 0; x < COLS; x++) { glyph[0][x] = glyph[1][x] = 'W'; ws[0][x] = slot_of(ceilpx); ws[1][x] = slot_of(wallpx); }
#elif PET_SECTION == 91      /* core line 935 */
        for (int i = 0; i < ndoor; i++) if (doors[i].room == r) { int tx = doors[i].tx; memcpy(a, wallpx, sizeof a); over24(a, doorpx); glyph[1][tx] = 'D'; ws[1][tx] = slot_of(a);
            if (rm->outdoor) { for (int y = TRIG_ROW; y < ROWS - 1; y++) fs[y][tx] = slot_of(pathpx); } else fs[TRIG_ROW][tx] = slot_of(matpx); }
#elif PET_SECTION == 92      /* core line 938 */
            Sheet *s = sheet("tilesets", scen[i].sheet); memcpy(b, floorpx, sizeof b); tile24(s, (scen[i].c + ci) * T, (scen[i].r + cj) * T, a); over24(b, a); glyph[y][x] = scen[i].h >= 2 ? 'T' : 'f'; ws[y][x] = slot_of(b); }
        for (int i = 0; i < npl; i++) if (pl[i].room == r) { Item *it = &cat[pl[i].item]; Sheet *s = sheet("tilesets", it->sheet);
#elif PET_SECTION == 93      /* core line 941 */
                tile24(s, (it->c + ci) * T, (it->r + cj) * T, a);
#elif PET_SECTION == 94      /* core line 943 */
#elif PET_SECTION == 95      /* core line 951 */
        snprintf(p, sizeof p, "%s/extrusion.pdl", d); { FILE *f = fopen_tmp(p); if (f) { fprintf(f, "SECTION      | KEY                | VALUE\n----------------------------------------------------------------------\nMETA         | map_id             | rpg-pet\nMETA         | desk_id            | %s\nMETA         | note               | %s (rpg-pet house, exported from the live house)\nEXTRUDE      | W                  | 3\nEXTRUDE      | D                  | 3\nEXTRUDE      | T                  | 2\nEXTRUDE      | f                  | 1\nEXTRUDE      | default            | 0\n", rooms[r].id, rooms[r].name); fclose_tmp(f, p); } }
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
        if (flag_on("int", 0)) { int ci = ctl_idx(); use_ent(ci); pet.room = r; pet.dir = 0; pet.tx = pet.ty = -1; int placed = 0;
#elif PET_SECTION == 103      /* core line 1030 */
#elif PET_SECTION == 104      /* core line 1039 */
        char names[40][48]; int nn = 0; snprintf(names[nn++], 48, "house.txt"); snprintf(names[nn++], 48, "trainer.txt"); snprintf(names[nn++], 48, "relations.txt"); snprintf(names[nn++], 48, "chat.txt"); snprintf(names[nn++], 48, "active.txt"); snprintf(names[nn++], 48, "daylight.txt"); snprintf(names[nn++], 48, "flag_ctl.txt"); snprintf(names[nn++], 48, "flag_follow.txt"); snprintf(names[nn++], 48, "events.txt");      /* events.txt is bigger than the 64 KB read buffer below: streamed */
#elif PET_SECTION == 105      /* core line 1045 */
            if (!strcmp(names[i], "events.txt")) { if (copy_file(a, bpath)) ok++; continue; }
#elif PET_SECTION == 106      /* core line 1047 */
        if (!saving) { char b[40]; read_file(mp, b, sizeof b); long long ms = atoll(b); if (ms > 0) { clock_paths(); clock_install(); char ma[32]; snprintf(ma, sizeof ma, "%lld", ms); lc_run(0, "cmd", "rpg", "settime", ma, "--source", "rpg-load", NULL); lc_run(0, "step", "5", NULL); } pet_load(); }
        snprintf(msg, cap, "%s slot %d (%d files)", saving ? "saved to" : "loaded from", slot, ok); return; }
#elif PET_SECTION == 107      /* core line 1050 */
    if (!strcmp(v, "toggle") && argc >= 2 && (!strcmp(argv[1], "chat") || !strcmp(argv[1], "hb") || !strcmp(argv[1], "voice") || !strcmp(argv[1], "left"))) { set_flag(argv[1], flag_on(argv[1], !strcmp(argv[1], "voice") || !strcmp(argv[1], "left")) ? "0" : "1"); snprintf(msg, cap, "%s toggled", argv[1]); return; }
#elif PET_SECTION == 108      /* core line 1131 */
    char wp[PATH_MAX], wb[2048], m[64] = "", dk[64] = "", xp[PATH_MAX], kb[512]; snprintf(wp, sizeof wp, "%s/@.apps/piececraft-hq/pieces/world_01/state.txt", HOUSE); read_file(wp, wb, sizeof wb);
#elif PET_SECTION == 109      /* core line 1134 */
    snprintf(xp, sizeof xp, "%s/@.apps/piececraft-hq/pieces/xelector_01/state.txt", HOUSE); read_file(xp, kb, sizeof kb); int xx = -1, xy = -1;
#elif PET_SECTION == 110      /* core line 1144 */
    char wp[PATH_MAX], wb[2048], m[64] = "", dk[64] = "", xp[PATH_MAX], kb[512]; snprintf(wp, sizeof wp, "%s/@.apps/piececraft-hq/pieces/world_01/state.txt", HOUSE); read_file(wp, wb, sizeof wb);
#elif PET_SECTION == 111      /* core line 1146 */
    if (strcmp(m, "rpg-pet")) return 0; snprintf(xp, sizeof xp, "%s/@.apps/piececraft-hq/pieces/xelector_01/state.txt", HOUSE); read_file(xp, kb, sizeof kb); int xx = -1, xy = -1;
#elif PET_SECTION == 112      /* core line 1177 */
    signal(SIGTERM, bye); signal(SIGINT, bye); signal(SIGHUP, bye); srand((unsigned)(time(NULL) ^ getpid()));
#elif PET_SECTION == 113      /* core line 1185 */
        pet_load(); int in = flag_on("int", 0), tm = turn_mode(); static int prev_in = 0; static long int_on_at = 0; if (in && !prev_in) int_on_at = now_ms(); prev_in = in;
#elif PET_SECTION == 114      /* core line 1190 */
        if (in && n) { char kl[300], one[16]; kl[0] = 0; for (int i = 0; i < n && i < 20; i++) { snprintf(one, sizeof one, "%d ", codes[i]); strncat(kl, one, sizeof kl - strlen(kl) - 1); } char kp[PATH_MAX]; spath(kp, "keys_seen.txt"); write_if_changed(kp, kl, strlen(kl)); }      /* last batch of codes, for debugging */
#elif PET_SECTION == 115      /* core line 1224 */
            if (board_window_open() && t - cchk >= tune("click_poll_ms", 150)) { cchk = t; char cp[PATH_MAX]; snprintf(cp, sizeof cp, "%s/#.desktop/pchq_canvas_click.txt", HOUSE); struct stat cs;
#elif PET_SECTION == 116      /* core line 1229 */
                    if (cw > 0 && ch > 0) { int col = (cx * FW / cw - OX) / T, row = (cy * FH / ch - OY) / T; char *sa[2]; char sid[24]; sa[0] = "select"; sa[1] = sid; int hit = 0;
#elif PET_SECTION == 117      /* core line 1238 */
        { char dp[PATH_MAX], db[40]; spath(dp, "daylight.txt"); read_file(dp, db, sizeof db); if (strcmp(db, lastday)) { snprintf(lastday, sizeof lastday, "%s", db); dirty = 1; } }
#elif PET_SECTION == 118      /* core line 1259 */
    signal(SIGPIPE, SIG_IGN);      /* a closed pipe / dead child must never kill a verb or the daemon (the children get SIG_DFL back before exec) */
    signal(SIGPIPE, SIG_IGN);      /* a closed pipe / dead child must never kill a verb or the daemon (the children get SIG_DFL back before exec) */
    snprintf(ST, sizeof ST, "%s/state", APP); mkdir(ST, 0755);
#elif PET_SECTION == 119      /* core line 1268 */
    load_data(); replay(); pet_load(); clock_paths();
#endif
#endif
