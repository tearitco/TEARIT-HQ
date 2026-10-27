/* petslike_life.c - the GENERIC life layer of a pets game: hp, hunger, death, respawn, the bottom-bar life bars and the `life` verb. TEXT-INCLUDED by a skin
 * (house rule: pure logic with 2+ consumers = text-include, never header + link). Lifted out of rpg-space's skin 2026-10-27 (owner: "backwards merge ... deaths,
 * lifebars, etc. onto the other pets games") with no behaviour change there (its gate stays green); a game opts in by including this file.
 * Config: life.pdl in the app folder (LIFE | key | value, re-read every 2 s): enabled, hunger_ms, starve_hp, drop_pct, death_mode (respawn|permadeath), respawn_room/x/y, take_range.
 * Per character state/life_<id>.txt (hp, hunger, dead, deaths, inv=key:n,...); log rows DEATH / RESPAWN / TAKE in state/events.txt; event pages events/death and events/respawn
 * (trigger parallel, RPG Maker's death state and common events) run first, the C effect runs when no page exists. COST: when life.pdl says enabled=0 or the file is missing and
 * the skin never calls pl_life_tick, nothing runs; the tick itself is a few file reads every 1.5 s (state/life_*.txt signature).
 * HOOKS the including skin defines BEFORE the include (defaults below when absent):
 *   PL_DROPS_BEGIN() / PL_DROPS_END()      load / save the skin's drop list around one death;
 *   PL_DROP(ent, item, n)                  one stack falls where the dead stood; returns 1 if it was placed (default: nothing drops);
 *   PL_LIFE_VERB_EXTRA(a, argc, argv, msg, cap)   extra `life` verbs of the skin, returns 1 when handled (rpg-space: take);
 *   PL_LIFE_VERB_KEEPS_NOSLOT(a)           1 for verbs that need no character argument (rpg-space: "take").
 * Needs from the core: hero, party, nparty, tr, others, rooms, pet, active, use_ent, pet_save, room_idx, spath, read_file, write_if_changed, append_line, split_bar, now_ms, run_event_x, ctl_idx, NP. */
#ifndef PL_DROPS_BEGIN
#define PL_DROPS_BEGIN() ((void)0)
#define PL_DROPS_END() ((void)0)
#endif
#ifndef PL_DROP
#define PL_DROP(e, item, n) 0
#endif
#ifndef PL_LIFE_VERB_EXTRA
#define PL_LIFE_VERB_EXTRA(a, argc, argv, msg, cap) 0
#endif
#ifndef PL_LIFE_VERB_KEEPS_NOSLOT
#define PL_LIFE_VERB_KEEPS_NOSLOT(a) 0
#endif
typedef struct { int hp, hunger, dead, deaths; char inv[400]; long last; } PlLife;
static PlLife pl_life[NP + 1];      /* index 0 = the hero, 1.. = party */
static char pl_lc_mode[16] = "respawn", pl_lc_room[16] = "bridge"; static int pl_lc_on = 1, pl_lc_hunger_ms = 60000, pl_lc_starve = 1, pl_lc_drop = 50, pl_lc_rx = 7, pl_lc_ry = 5, pl_lc_range = 2; static long pl_lc_at;
static void pl_life_cfg(void) {
    long now = now_ms(); if (pl_lc_at && now - pl_lc_at < 2000) return; pl_lc_at = now; char p[PATH_MAX], ln[300]; char *f[4]; snprintf(p, sizeof p, "%s/life.pdl", APP); FILE *fp = fopen(p, "r"); if (!fp) return;
    while (fgets(ln, sizeof ln, fp)) { if (ln[0] == '#' || split_bar(ln, f, 4) < 3 || strcmp(f[0], "LIFE")) continue; const char *k = f[1], *v = f[2];
        if (!strcmp(k, "enabled")) pl_lc_on = atoi(v); else if (!strcmp(k, "hunger_ms")) pl_lc_hunger_ms = atoi(v); else if (!strcmp(k, "starve_hp")) pl_lc_starve = atoi(v); else if (!strcmp(k, "drop_pct")) pl_lc_drop = atoi(v);
        else if (!strcmp(k, "death_mode")) snprintf(pl_lc_mode, sizeof pl_lc_mode, "%.15s", v); else if (!strcmp(k, "respawn_room")) snprintf(pl_lc_room, sizeof pl_lc_room, "%.15s", v); else if (!strcmp(k, "respawn_x")) pl_lc_rx = atoi(v); else if (!strcmp(k, "respawn_y")) pl_lc_ry = atoi(v); else if (!strcmp(k, "take_range")) pl_lc_range = atoi(v); }
    fclose(fp);
    for (char *q = pl_lc_mode; *q; q++) if (*q == ' ') *q = 0;      /* trailing blanks of the pdl cell */
    for (char *q = pl_lc_room; *q; q++) if (*q == ' ') *q = 0;
}
static int pl_life_slot(const char *id) { if (!id || !id[0]) return -2; char lo[24]; snprintf(lo, sizeof lo, "%s", id); for (char *q = lo; *q; q++) if (*q >= 'A' && *q <= 'Z') *q += 32; char hn[24]; snprintf(hn, sizeof hn, "%s", hero.name); for (char *q = hn; *q; q++) if (*q >= 'A' && *q <= 'Z') *q += 32; if (!strcmp(lo, hn)) return 0; for (int i = 0; i < nparty; i++) if (!strcmp(lo, party[i].id)) return i + 1; return -2; }
static const char *pl_life_id(int slot) { static char b[24]; if (slot == 0) { snprintf(b, sizeof b, "%s", hero.name); for (char *q = b; *q; q++) if (*q >= 'A' && *q <= 'Z') *q += 32; return b; } return party[slot - 1].id; }
static int pl_life_mhp(int slot) { int m = slot == 0 ? hero.mhp : party[slot - 1].mhp; return m > 0 ? m : 40; }
static Ent *pl_life_ent(int slot) { return slot == 0 ? &tr : &others[slot - 1]; }
static void pl_life_path(int slot, char *p, size_t n) { char nm[40]; snprintf(nm, sizeof nm, "life_%s.txt", pl_life_id(slot)); spath(p, nm); (void)n; }
static void pl_life_load(int slot) { PlLife *L = &pl_life[slot]; char p[PATH_MAX], b[900]; pl_life_path(slot, p, sizeof p); int had = read_file(p, b, sizeof b); L->hp = pl_life_mhp(slot); L->hunger = 0; L->dead = 0; L->deaths = 0; L->inv[0] = 0;
    if (had) for (char *l = strtok(b, "\n"); l; l = strtok(NULL, "\n")) { if (!strncmp(l, "hp=", 3)) L->hp = atoi(l + 3); else if (!strncmp(l, "hunger=", 7)) L->hunger = atoi(l + 7); else if (!strncmp(l, "dead=", 5)) L->dead = atoi(l + 5); else if (!strncmp(l, "deaths=", 7)) L->deaths = atoi(l + 7); else if (!strncmp(l, "inv=", 4)) snprintf(L->inv, sizeof L->inv, "%s", l + 4); }
    if (!L->last) L->last = now_ms(); }
static void pl_life_save(int slot) { PlLife *L = &pl_life[slot]; char p[PATH_MAX], b[900]; pl_life_path(slot, p, sizeof p); snprintf(b, sizeof b, "hp=%d\nhunger=%d\ndead=%d\ndeaths=%d\ninv=%s\n", L->hp, L->hunger, L->dead, L->deaths, L->inv); write_if_changed(p, b, strlen(b)); }
static int pl_inv_count(PlLife *L, const char *item) { char b[400]; snprintf(b, sizeof b, "%s", L->inv); for (char *t = strtok(b, ","); t; t = strtok(NULL, ",")) { char *c = strchr(t, ':'); if (c && !strncmp(t, item, (size_t)(c - t)) && (size_t)(c - t) == strlen(item)) return atoi(c + 1); } return 0; }
static void pl_inv_set(PlLife *L, const char *item, int n) { char b[400], out[400] = ""; int found = 0; snprintf(b, sizeof b, "%s", L->inv); for (char *t = strtok(b, ","); t; t = strtok(NULL, ",")) { char *c = strchr(t, ':'); if (!c) continue; char k[24]; snprintf(k, sizeof k, "%.*s", (int)(c - t), t); int cnt = atoi(c + 1); if (!strcmp(k, item)) { cnt = n; found = 1; } if (cnt > 0) { size_t o = strlen(out); snprintf(out + o, sizeof out - o, "%s%s:%d", o ? "," : "", k, cnt); } }
    if (!found && n > 0) { size_t o = strlen(out); snprintf(out + o, sizeof out - o, "%s%s:%d", o ? "," : "", item, n); } snprintf(L->inv, sizeof L->inv, "%s", out); }
static void pl_life_log(const char *kind, int slot, const char *extra) { char b[200]; snprintf(b, sizeof b, "%s|%ld|%s|%s", kind, (long)time(NULL), pl_life_id(slot), extra); append_line("events.txt", b); }
static void pl_life_respawn_effect(int slot) {      /* the respawn: back on the safe place, full hp, hunger 0 (life.pdl respawn_*); what events/respawn calls */
    PlLife *L = &pl_life[slot]; char ex[100]; int rr = room_idx(pl_lc_room); if (rr < 0) rr = 0; pl_life_load(slot); L->hp = pl_life_mhp(slot); L->hunger = 0; L->dead = 0; L->last = now_ms();
    int ci = slot == 0 ? -1 : slot - 1; use_ent(ci); pet.room = rr; pet.x = pl_lc_rx; pet.y = pl_lc_ry; pet.tx = pet.ty = -1; pet_save(); use_ent(active);
    snprintf(ex, sizeof ex, "%s,%d,%d", pl_lc_room, pl_lc_rx, pl_lc_ry); pl_life_log("RESPAWN", slot, ex); pl_life_save(slot); }
static void pl_life_die_effect(int slot) {      /* the death itself: drop_pct of every stack falls where they stood (PL_DROP hook); then the respawn page / effect by death_mode. What events/death calls */
    PlLife *L = &pl_life[slot]; pl_life_load(slot); Ent *e = pl_life_ent(slot); char b[400], ex[160], *sv = NULL; int dropped = 0; snprintf(b, sizeof b, "%s", L->inv);
    PL_DROPS_BEGIN(); for (char *t = strtok_r(b, ",", &sv); t; t = strtok_r(NULL, ",", &sv)) { char *c = strchr(t, ':'); if (!c) continue; char k[24]; snprintf(k, sizeof k, "%.*s", (int)(c - t), t); int cnt = atoi(c + 1), d = cnt * pl_lc_drop / 100; if (d <= 0) continue;
        if (PL_DROP(e, k, d)) dropped += d; }
    { char cp[400], *sv2 = NULL; snprintf(cp, sizeof cp, "%s", L->inv); for (char *t = strtok_r(cp, ",", &sv2); t; t = strtok_r(NULL, ",", &sv2)) { char *c = strchr(t, ':'); if (!c) continue; char k[24]; snprintf(k, sizeof k, "%.*s", (int)(c - t), t); int cnt = atoi(c + 1); pl_inv_set(L, k, cnt - cnt * pl_lc_drop / 100); } }
    PL_DROPS_END(); L->deaths++; L->hp = 0; L->dead = 1; snprintf(ex, sizeof ex, "%s,%d,%d|dropped %d (%d%%)|mode %s", rooms[e->room].id, e->x, e->y, dropped, pl_lc_drop, pl_lc_mode); pl_life_log("DEATH", slot, ex); pl_life_save(slot);
    if (!strcmp(pl_lc_mode, "respawn")) { char ap[PATH_MAX]; spath(ap, "life_actor.txt"); char ab[40]; snprintf(ab, sizeof ab, "%s\n", pl_life_id(slot)); write_if_changed(ap, ab, strlen(ab)); if (!run_event_x("respawn", "parallel", "")) pl_life_respawn_effect(slot); pl_life_load(slot); } }
static void pl_life_die(int slot) {      /* hp reached 0: the Death state, then the death page (RPG Maker's common event). No page = the effect runs directly. */
    PlLife *L = &pl_life[slot]; L->hp = 0; L->dead = 1; pl_life_save(slot); char ap[PATH_MAX], ab[40]; spath(ap, "life_actor.txt"); snprintf(ab, sizeof ab, "%s\n", pl_life_id(slot)); write_if_changed(ap, ab, strlen(ab));
    if (!run_event_x("death", "parallel", "")) pl_life_die_effect(slot); pl_life_load(slot); }
static int pl_life_tick(void) {      /* hunger grows, starving hurts, 0 hp dies. Returns 1 when anything changed */
    pl_life_cfg(); if (!pl_lc_on) return 0; int changed = 0; long now = now_ms();
    { static long lchk; static unsigned lsig = 1; if (now - lchk >= 1500) { lchk = now; unsigned sg = 7; for (int s = 0; s <= nparty; s++) { pl_life_load(s); sg = sg * 31 + (unsigned)pl_life[s].hp * 3 + (unsigned)pl_life[s].dead * 1009 + (unsigned)(pl_life[s].hunger / 10); } if (sg != lsig) { lsig = sg; changed = 1; } } }      /* a verb or an event page in another process changed a life file: tell the board (health line) */
    for (int s = 0; s <= nparty; s++) { PlLife *L = &pl_life[s]; static int loaded[NP + 1]; if (!loaded[s]) { pl_life_load(s); loaded[s] = 1; } if (L->dead) continue;
        if (pl_lc_hunger_ms > 0 && now - L->last >= pl_lc_hunger_ms) { L->last = now; pl_life_load(s); L->last = now; if (L->dead) continue;      /* re-read first: verbs and event pages write these files from other processes, a cached copy would overwrite their changes */
            if (L->hunger < 100) L->hunger++; else L->hp -= pl_lc_starve; pl_life_save(s); changed = 1; }
        if (L->hp <= 0) { pl_life_die(s); changed = 1; } }
    return changed; }
static const char *pl_life_bar_state(int slot) { if (slot < 0 || slot > nparty) return ""; static char loaded[NP + 1]; pl_life_cfg(); if (!loaded[slot]) { pl_life_load(slot); loaded[slot] = 1; } PlLife *L = &pl_life[slot]; int mh = pl_life_mhp(slot); if (L->dead || L->hp <= 0) return "ko"; return L->hp * 100 / mh < 30 ? "low" : "ok"; }      /* bar.txt 6th field: written when the desk exports, so a health change shows at the next export (the life tick asks for one) */
static void pl_life_ui(char *out, size_t cap) {      /* the bottom bar: per character a bar of block cells (green, red under 30 percent, grey when KO'd), vars life_N_name / _bar / _ok / _low / _ko */
    size_t o = 0; out[0] = 0; for (int s = 0; s <= nparty && s < 5; s++) { PlLife *L = &pl_life[s]; pl_life_load(s); int mh = pl_life_mhp(s), cells = L->hp <= 0 ? 0 : 1 + L->hp * 3 / mh; if (cells > 4) cells = 4; char bar[48] = ""; for (int c = 0; c < 4; c++) strcat(bar, c < cells ? "\xe2\x96\xac" : "\xc2\xb7"); int ko = L->dead || L->hp <= 0, low = !ko && L->hp * 100 / mh < 30; char nm[24]; snprintf(nm, sizeof nm, "%.1s", pl_life_id(s)); if (nm[0] >= 'a' && nm[0] <= 'z') nm[0] -= 32;
        o += (size_t)snprintf(out + o, cap - o, "life_%d_name=%s\nlife_%d_bar=%s %s\nlife_%d_ok=%s\nlife_%d_low=%s\nlife_%d_ko=%s\n", s, nm, s, nm, ko ? "KO" : bar, s, !ko && !low ? "1" : "", s, low ? "1" : "", s, ko ? "1" : ""); } }
static void pl_life_verb(int argc, char **argv, char *msg, size_t cap) {
    pl_life_cfg(); sk_drops_load(); const char *a = argc >= 2 ? argv[1] : "status"; static int loaded[NP + 1];
    for (int s = 0; s <= nparty; s++) if (!loaded[s]) { pl_life_load(s); loaded[s] = 1; }
    if (!strcmp(a, "status")) { int slot = argc >= 3 ? pl_life_slot(argv[2]) : -2; char b[500] = ""; size_t o = 0;
        for (int s = 0; s <= nparty; s++) { if (slot >= 0 && s != slot) continue; PlLife *L = &pl_life[s]; o += (size_t)snprintf(b + o, sizeof b - o, "%s hp %d/%d hunger %d%s deaths %d inv [%s]; ", pl_life_id(s), L->hp, pl_life_mhp(s), L->hunger, L->dead ? " DEAD" : "", L->deaths, L->inv); }
        snprintf(msg, cap, "%s drops %d, mode %s, drop %d%%", b, sk_ndrops, pl_lc_mode, pl_lc_drop); return; }
    if (!strcmp(a, "reset")) { for (int s = 0; s <= nparty; s++) { PlLife *L = &pl_life[s]; L->hp = pl_life_mhp(s); L->hunger = 0; L->dead = 0; L->deaths = 0; L->last = now_ms(); pl_life_save(s); } sk_ndrops = 0; sk_drops_save(); snprintf(msg, cap, "life reset: everyone alive, drops cleared"); return; }
    char actor[32] = ""; if (argc >= 3 && !strcmp(argv[2], "@actor")) { char ap[PATH_MAX]; spath(ap, "life_actor.txt"); read_file(ap, actor, sizeof actor); actor[strcspn(actor, "\r\n")] = 0; argv[2] = actor; }      /* @actor = the character the death / respawn page was played for */
    int slot = argc >= 3 ? pl_life_slot(argv[2]) : -2; if (slot < 0 && !PL_LIFE_VERB_KEEPS_NOSLOT(a)) { snprintf(msg, cap, "life: unknown character (use harold, ember, ...)"); return; }
    if (!strcmp(a, "hurt") && argc >= 4) { pl_life[slot].hp -= atoi(argv[3]); if (pl_life[slot].hp <= 0) pl_life_die(slot); else pl_life_save(slot); snprintf(msg, cap, "%s hp %d", pl_life_id(slot), pl_life[slot].hp); return; }
    if (!strcmp(a, "die")) { pl_life_die_effect(slot); snprintf(msg, cap, "%s died (dropped %d%% of the stacks)", pl_life_id(slot), pl_lc_drop); return; }
    if (!strcmp(a, "respawn")) { pl_life_respawn_effect(slot); snprintf(msg, cap, "%s is back on %s %d,%d", pl_life_id(slot), pl_lc_room, pl_lc_rx, pl_lc_ry); return; }
    if (!strcmp(a, "feed") && argc >= 4) { pl_life[slot].hunger -= atoi(argv[3]); if (pl_life[slot].hunger < 0) pl_life[slot].hunger = 0; pl_life_save(slot); snprintf(msg, cap, "%s hunger %d", pl_life_id(slot), pl_life[slot].hunger); return; }
    if (!strcmp(a, "set") && argc >= 5) { if (!strcmp(argv[3], "hunger")) pl_life[slot].hunger = atoi(argv[4]); else if (!strcmp(argv[3], "hp")) pl_life[slot].hp = atoi(argv[4]); pl_life_save(slot); snprintf(msg, cap, "%s %s %s", pl_life_id(slot), argv[3], argv[4]); return; }
    if (!strcmp(a, "give") && argc >= 4) { int n = argc >= 5 ? atoi(argv[4]) : 1; pl_inv_set(&pl_life[slot], argv[3], pl_inv_count(&pl_life[slot], argv[3]) + n); pl_life_save(slot); snprintf(msg, cap, "%s now has [%s]", pl_life_id(slot), pl_life[slot].inv); return; }
    if (PL_LIFE_VERB_EXTRA(a, argc, argv, msg, cap)) return;      /* the skin's own life verbs (rpg-space: take <drop#>) */
    snprintf(msg, cap, "usage: life status [id] | die <id|@actor> | respawn <id|@actor> | hurt <id> <n> | feed <id> <n> | give <id> <item> [n] | set <id> hunger|hp <n> | take <drop#> [id] | reset");
}
