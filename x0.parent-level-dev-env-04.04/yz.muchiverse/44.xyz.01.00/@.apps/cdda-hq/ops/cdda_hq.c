/* cdda_hq.c - cdda-hq on the shared pets-like core (&.widgits/_shared-lib/petslike_core.c). Two roles in one file: the top builds the program, and the core
 * re-includes this file once per SECTION to pull in what is specific to this game. Design: @.apps/../#.#.calendar-dox/!.HQ-IQ-BOOK/16.game/PETSLIKE-FRAMEWORK-PLAN.md */
#ifndef PET_SECTION
#define PET_APP "cdda-hq"
#define PET_OP "cdda_hq"
#define PET_SKIN "cdda_hq.c"
#include "../../../&.widgits/_shared-lib/petslike_core.c"
#else
#if PET_SECTION == 1
 * Every picture is an RPG Maker MV tile or character frame (sheets under #.NNEST_ASSETS/rmmv-www-img), nothing else is drawn.
#elif PET_SECTION == 2
static const char *cd_rule(const char *actor, const char *verb, const char *target, const char *at); static void cd_reset(void);
#define GE_RULE_HOOK(a, v, t, at) cd_rule(a, v, t, at)
#define GE_RESET_HOOK() cd_reset()
static void ge_logline(const char *actor, const char *verb, const char *target, const char *res);
#define GE_LOG_HOOK(a, v, t, at, r) ge_logline(a, v, t, r)
#define GE_COMPACT_BYTES() ge_compact_limit()      /* tuning.pdl: ledger_compact_kb, default 4096 (4 MB) */
static long ge_compact_limit(void); static int tune(const char *name, int def); static void ge_failnote(const char *why);
#define GE_FAIL_HOOK(w) ge_failnote(w)
static void cd_cksave(FILE *o); static void cd_ckload(char *p);
#define GE_CKSAVE_HOOK(o) cd_cksave(o)
#define GE_CKLOAD_HOOK(p) cd_ckload(p)
#include "../../../&.widgits/_shared-lib/game_events.c"      /* S1 event line + resolver (also df-hq, cdda-hq, mcl-hq); cdda-hq's rules are cd_rule() below */
#include "../../../&.widgits/_shared-lib/game_decisions.c"      /* M3 decision ledger (append-only DEC rows + daily review) */
#elif PET_SECTION == 3
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
#elif PET_SECTION == 12
    /* HARDENING: sizes and cells from rooms.pdl / catalog.pdl are untrusted: a scenery of width 2147483648999 made blit() loop for ever (found by the fuzz harness, hugenum on rooms.pdl) */
    for (int i = 0; i < nscen; i++) { Scen *s_ = &scen[i]; if (s_->c < 0 || s_->c > 255) s_->c = 0; if (s_->r < 0 || s_->r > 255) s_->r = 0; if (s_->w < 1 || s_->w > MAXC) s_->w = 1; if (s_->h < 1 || s_->h > MAXC) s_->h = 1; if (s_->x < -MAXC || s_->x > 2 * MAXC) s_->x = 0; if (s_->y < -MAXR || s_->y > 2 * MAXR) s_->y = 0; }
    for (int i = 0; i < ncat; i++) { Item *t_ = &cat[i]; if (t_->c < 0 || t_->c > 255) t_->c = 0; if (t_->r < 0 || t_->r > 255) t_->r = 0; if (t_->w < 1 || t_->w > 8) t_->w = 1; if (t_->h < 1 || t_->h > 8) t_->h = 1; if (t_->price < 0 || t_->price > 1000000) t_->price = 0; }
    for (int i = 0; i < ndoor; i++) { Door *d_ = &doors[i]; if (d_->tx < 0 || d_->tx >= MAXC) d_->tx = 0; if (d_->ty < 0 || d_->ty >= MAXR) d_->ty = 1; if (d_->ax < 0 || d_->ax >= MAXC) d_->ax = 0; if (d_->ay < 0 || d_->ay >= MAXR) d_->ay = 0; }
    for (int i = 0; i < nroom; i++) { if (rooms[i].fc < 0 || rooms[i].fc > 255) rooms[i].fc = 0; if (rooms[i].fr < 0 || rooms[i].fr > 255) rooms[i].fr = 0; }
    if (nroom == 0) { Room *r0 = &rooms[nroom++]; memset(r0, 0, sizeof *r0); snprintf(r0->id, 16, "home"); snprintf(r0->name, 32, "Home"); r0->cols = 13; r0->rows = 8; r0->tpx = 48; r0->trig = 2; }      /* HARDENING: rooms.pdl missing or all garbage = one default room, never a zero-size room (tpx 0 divides) */
#elif PET_SECTION == 13
#elif PET_SECTION == 14
    char p[PATH_MAX], line[300]; spath(p, "house.txt");
#elif PET_SECTION == 15
static void use_ent(int i) { me = i; pet = i < 0 ? tr : others[i]; if (pet.room >= 0 && pet.room < nroom) g_gr = pet.room; }
#elif PET_SECTION == 16
    write_if_changed(p, b, strlen(b)); if (me < 0) tr = pet; else others[me] = pet; calc_view();
#elif PET_SECTION == 17
static int over_land(const Room *rm, int x, int y);
static int floor_blocked(int room, int x, int y, int skip_id) { if (rooms[room].over && !over_land(&rooms[room], x, y)) return 9999; for (int i = 0; i < nscen; i++) if (scen[i].room == room && x >= scen[i].x && x < scen[i].x + scen[i].w && y >= scen[i].y && y < scen[i].y + scen[i].h) return 9999; for (int i = 0; i < npl; i++) { Placed *q = &pl[i]; if (q->room != room || q->id == skip_id || cat[q->item].wall) continue; if (x >= q->x && x < q->x + cat[q->item].w && y >= q->y && y < q->y + cat[q->item].h) return q->id; } return 0; }
static int door_here(int room, int x, int y) { for (int i = 0; i < ndoor; i++) if (doors[i].room == room && doors[i].tx == x && y == doors[i].ty + 1) return i; return -1; }
/* overmap land: an island on a square ocean. Cells inside the rounded rectangle (2..cols-3, 1..rows-2, corners cut) are land, the outermost land ring is beach. Everything else is ocean (blocked). */
static int over_land(const Room *rm, int x, int y) { if (x < 2 || x > rm->cols - 3 || y < 1 || y > rm->rows - 2) return 0; if ((x <= 3 || x >= rm->cols - 4) && (y <= 2 || y >= rm->rows - 3)) return 0; return 1; }
/* ---------- crafting (2026-10-17, S1 slice; rules by RULES-cdda-hq.md) ---------- recipes.pdl: RECIPE | id | output | count | tool | skill | minutes | inputs   (inputs = item:count,item:count, no spaces).
 * craft is an EVENT in state/events.txt; the bags are a replay of that ledger (cd_reset() reloads recipes.pdl at the top of every ge_scan). Only tool "none" is supported (any other tool: missing), skill is read and not
 * checked yet, minutes are charged to the game clock by the craft verb (not by the replay). An empty or absent recipes.pdl means every craft is "missing". */
#define MAXREC 24
typedef struct { char id[16], out[16], in[8][16]; int cnt, nin, inn[8], minutes, tool; } Recipe;
static Recipe recs[MAXREC]; static int nrec, nrec_bad;
/* wounds (RULES-cdda-hq.md, C1 slice 2): six parts, each starts at the victim's mhp / 6 (integer); one strike takes 1 from one part. The part is a pure function of the ledger prefix (a hash of the row plus the number of
 * strikes before it), so a replay gives the same wounds. state/wounds.pdl is the readable copy, rewritten by the strike verbs. A leg at 0 only makes walking "slow" (one cell still). Fighting beyond this is later. */
typedef struct { char who[16]; int part, left, max; } Wnd;
static Wnd wd[64]; static int nwd, nstrike;
static const char *PARTS[6] = { "head", "torso", "arm_l", "arm_r", "leg_l", "leg_r" };
static int mhp_of(const char *who);
static Ent zed; static int zed_ok; static void zed_load(void);      /* the one zed, defined below */
static void cd_reset(void) {
    nrec = nrec_bad = 0; nwd = 0; nstrike = 0; char p[PATH_MAX], line[400], *f[12]; snprintf(p, sizeof p, "%s/recipes.pdl", APP); FILE *fp = fopen(p, "r"); if (!fp) return;
    while (fgets(line, sizeof line, fp)) { int n = split_bar(line, f, 12); if (n < 8 || strcmp(f[0], "RECIPE")) continue; Recipe r; memset(&r, 0, sizeof r);
        int ok = f[1][0] && strlen(f[1]) < 16 && f[2][0] && strlen(f[2]) < 16 && atoi(f[3]) > 0 && atoi(f[3]) < 100 && atoi(f[7 - 1]) >= 0 && atoi(f[6]) >= 0 && atoi(f[6]) < 1000;
        snprintf(r.id, 16, "%s", f[1]); snprintf(r.out, 16, "%s", f[2]); r.cnt = atoi(f[3]); r.minutes = atoi(f[6]); r.tool = strcmp(f[4], "none") != 0;
        char ins[200]; snprintf(ins, sizeof ins, "%s", n > 7 ? f[7] : ""); for (char *t = strtok(ins, ","); ok && t; t = strtok(NULL, ",")) { char *c = strchr(t, ':'); if (!c || c == t || r.nin >= 8 || (size_t)(c - t) >= 16 || atoi(c + 1) < 1) { ok = 0; break; } *c = 0; snprintf(r.in[r.nin], 16, "%s", t); r.inn[r.nin++] = atoi(c + 1); }
        if (!ok || r.nin == 0) { nrec_bad++; continue; } if (nrec < MAXREC) recs[nrec++] = r; }
    fclose(fp);
}
static Recipe *cd_recipe(const char *id) { for (int i = 0; i < nrec; i++) if (!strcmp(recs[i].id, id)) return &recs[i]; return NULL; }
/* craft: the named recipe, tool none; the inputs must be in the actor's bag, they are taken and count of output given. Result ok or missing (unknown recipe, other tool, short of an input). */
/* compaction snapshot of the wound table: the strike counter (it seeds the wound-part hash, so it must survive) and every wound in table order */
static void cd_cksave(FILE *o) { fprintf(o, "CKG|NSTRIKE|%d\n", nstrike); for (int i = 0; i < nwd; i++) fprintf(o, "CKG|WND|%s|%d|%d|%d\n", wd[i].who, wd[i].part, wd[i].left, wd[i].max); }
static void cd_ckload(char *p) { char *f[8]; int n = split_bar(p, f, 8); if (n >= 2 && !strcmp(f[0], "NSTRIKE")) nstrike = atoi(f[1]); else if (n >= 5 && !strcmp(f[0], "WND") && nwd < 64 && strlen(f[1]) < 16) { snprintf(wd[nwd].who, 16, "%s", f[1]); wd[nwd].part = atoi(f[2]) % 6; wd[nwd].left = atoi(f[3]); wd[nwd].max = atoi(f[4]); nwd++; } }
static const char *cd_rule(const char *actor, const char *verb, const char *target, const char *at) {
    if (!strcmp(verb, "strike")) {      /* actor strikes target (a party id, trainer, or zed) */
        int m = mhp_of(target); if (m <= 0) return "unknown"; unsigned h = 5381; for (const char *s = actor; *s; s++) h = h * 33 + (unsigned char)*s; for (const char *s = target; *s; s++) h = h * 33 + (unsigned char)*s; for (const char *s = at; *s; s++) h = h * 33 + (unsigned char)*s;
        int part = (int)((h + (unsigned)nstrike++) % 6), i; for (i = 0; i < nwd; i++) if (!strcmp(wd[i].who, target) && wd[i].part == part) break;
        if (i == nwd) { if (nwd >= 64) return "full"; snprintf(wd[nwd].who, 16, "%s", target); wd[nwd].part = part; wd[nwd].max = m / 6; wd[nwd].left = m / 6; nwd++; }
        if (wd[i].left > 0) wd[i].left--; static char res[24]; snprintf(res, sizeof res, "wound:%s", PARTS[part]); return res;
    }
    (void)at; if (strcmp(verb, "craft")) return NULL; Recipe *rc = cd_recipe(target); if (!rc || rc->tool) return "missing";      /* tools are later: a recipe that names one cannot be crafted yet */
    for (int i = 0; i < rc->nin; i++) { GeInv *q = ge_inv_find(actor, rc->in[i], 0); if (!q || q->n < rc->inn[i]) return "missing"; }
    GeInv *o = ge_inv_find(actor, rc->out, 1); if (!o) return "full"; for (int i = 0; i < rc->nin; i++) { GeInv *q = ge_inv_find(actor, rc->in[i], 0); if (q) q->n -= rc->inn[i]; } o->n += rc->cnt; return "ok";
}
static int mhp_of(const char *who) { if (!strcmp(who, "zed")) return 18; if (!strcmp(who, "trainer")) return hero.mhp; for (int i = 0; i < nparty; i++) if (!strcmp(party[i].id, who)) return party[i].mhp; return 0; }
static void write_wounds(void) { char p[PATH_MAX], b[2400]; size_t o = 0; b[0] = 0; for (int i = 0; i < nwd && o + 60 < sizeof b; i++) o += (size_t)snprintf(b + o, sizeof b - o, "WOUND | %s | %s | %d | %d\n", wd[i].who, PARTS[wd[i].part], wd[i].left, wd[i].max); spath(p, "wounds.pdl"); write_if_changed(p, b, o); }
static int leg_hurt(const char *who) {      /* from the readable copy: a leg at 0 */
    char p[PATH_MAX], b[2400]; spath(p, "wounds.pdl"); read_file(p, b, sizeof b); for (char *l = strtok(b, "\n"); l; l = strtok(NULL, "\n")) { char *f[6]; int n = split_bar(l, f, 6); if (n >= 5 && !strcmp(f[0], "WOUND") && !strcmp(f[1], who) && !strncmp(f[2], "leg_", 4) && atoi(f[3]) <= 0) return 1; } return 0;
}
static void note_set(const char *s) { char p[PATH_MAX], b[320]; spath(p, "note.txt"); int n = snprintf(b, sizeof b, "%s\n", s); if (n >= (int)sizeof b) n = (int)sizeof b - 1; write_if_changed(p, b, (size_t)n); }      /* the last craft / help message, painted green under the coins (a file: it must outlive the verb process) */
static int over_beach(const Room *rm, int x, int y) { return over_land(rm, x, y) && (!over_land(rm, x - 1, y) || !over_land(rm, x + 1, y) || !over_land(rm, x, y - 1) || !over_land(rm, x, y + 1)); }
#elif PET_SECTION == 18
/* UltimateCataclysm loose PNGs. The map tiles come from these. People and furniture stay on the RPG Maker sheets until a later slice. df-hq is not this path. */
static char CDDA[PATH_MAX];
static Sheet cdda_s[32]; static int n_cdda;
static void cdda_root(void) {
    if (CDDA[0]) return; char d[PATH_MAX]; snprintf(d, sizeof d, "%s", HOUSE[0] ? HOUSE : APP);
    for (int i = 0; i < 8; i++) { char p[PATH_MAX]; snprintf(p, sizeof p, "%s/#.NNEST_ASSETS/cdda-tilesets/gfx/UltimateCataclysm", d); if (is_dir(p)) { snprintf(CDDA, sizeof CDDA, "%s", p); return; } char *s = strrchr(d, '/'); if (!s || s == d) break; *s = 0; }
}
static Sheet *cdda_png(const char *rel) {
    const char *base = strrchr(rel, '/'); base = base ? base + 1 : rel;
    for (int i = 0; i < n_cdda; i++) if (!strcmp(cdda_s[i].name, base)) return cdda_s[i].px ? &cdda_s[i] : NULL;
    cdda_root(); if (!CDDA[0] || n_cdda >= 32) return NULL;
    char p[PATH_MAX]; snprintf(p, sizeof p, "%s/%s", CDDA, rel); Sheet *s = &cdda_s[n_cdda++]; int n; s->px = stbi_load(p, &s->w, &s->h, &n, 4); snprintf(s->name, 40, "%s", base); return s->px ? s : NULL;
}
static void blit_fit(Sheet *s, int dx, int dy, int dw, int dh) {
    if (!s || !s->px || dw < 1 || dh < 1 || dw > 4 * FW || dh > 4 * FH) return;
    for (int y = 0; y < dh; y++) { int Y = dy + y, srcy = y * s->h / dh; if (Y < 0 || Y >= FH || srcy < 0 || srcy >= s->h) continue;
        for (int x = 0; x < dw; x++) { int X = dx + x, srcx = x * s->w / dw; if (X < 0 || X >= FW || srcx < 0 || srcx >= s->w) continue;
            unsigned char *a = s->px + ((size_t)srcy * s->w + srcx) * 4, *b = fr + ((size_t)Y * FW + X) * 4; int al = a[3]; if (!al) continue;
            for (int c = 0; c < 3; c++) b[c] = (unsigned char)((a[c] * al + b[c] * (255 - al)) / 255); b[3] = 255; } }
}
static void cell_png(const char *rel, int tx, int ty, int tw, int th) { blit_fit(cdda_png(rel), OX + tx * T, OY + (ty - (th - 1)) * T, tw * T, th * T); }
static void tile(const char *sh, int c, int r, int w, int h, int tx, int ty) { blit(sheet("tilesets", sh), c * S48, r * S48, w * S48, h * S48, OX + tx * T, OY + ty * T); }

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
        if (sea) cell_png("pngs_normal_32x32/terrain/t_water_moving_dp/t_water_moving_dp_unconnected.png", x, y, 1, 1);
        else if (shore) cell_png("pngs_normal_32x32/terrain/t_sand/t_sand_center.png", x, y, 1, 1);
        else cell_png("pngs_normal_32x32/terrain/t_grass/t_grass_center.png", x, y, 1, 1);
    }
}
#elif PET_SECTION == 19
    Room *rm = &rooms[vroom]; char phase[40] = "day"; { char dp[PATH_MAX], db[64]; spath(dp, "daylight.txt"); read_file(dp, db, sizeof db); if (strstr(db, "night")) snprintf(phase, sizeof phase, "night"); else if (strstr(db, "dusk")) snprintf(phase, sizeof phase, "dusk"); else if (strstr(db, "dawn")) snprintf(phase, sizeof phase, "dawn"); }
    if (rm->over) draw_over(rm); else if (rm->outdoor) {
        for (int x = 0; x < COLS; x++) { cell_png("pngs_normal_32x32/terrain/t_wall_wood/t_wall_wood_unconnected.png", x, WALL_ROW, 1, 1); for (int y = TRIG_ROW; y < ROWS; y++) cell_png("pngs_normal_32x32/terrain/t_grass/t_grass_center.png", x, y, 1, 1); }
    } else
    for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLS; x++) { if (y == 0 || y == WALL_ROW) cell_png("pngs_normal_32x32/terrain/t_wall_wood/t_wall_wood_unconnected.png", x, y, 1, 1); else cell_png("pngs_normal_32x32/terrain/t_floor/t_floor_center.png", x, y, 1, 1); }
    for (int i = 0; i < ndoor; i++) if (doors[i].room == vroom && !rm->over) cell_png("pngs_normal_32x32/terrain/t_door/t_door_c.png", doors[i].tx, doors[i].ty, 1, 1);
    for (int i = 0; i < nscen; i++) if (scen[i].room == vroom) {
        if (!strcmp(scen[i].sheet, "Outside_B")) cell_png("pngs_giant_96x96/terrain/t_tree/t_tree.png", scen[i].x, scen[i].y, scen[i].w, scen[i].h);
        else if (!strcmp(scen[i].sheet, "World_B")) cell_png("pngs_normal_32x32/overmap/otr_house_1_faceN.png", scen[i].x, scen[i].y, scen[i].w, scen[i].h);
        else tile(scen[i].sheet, scen[i].c, scen[i].r, scen[i].w, scen[i].h, scen[i].x, scen[i].y);
    }
#elif PET_SECTION == 20
    for (int a = 0; a < n; a++) { Placed *p = &pl[idx[a]]; Item *d = &cat[p->item]; tile(d->sheet, d->c, d->r, d->w, d->h, p->x, p->y); }
#elif PET_SECTION == 21
    for (int k = 0; k < (g_bg_only ? 0 : no); k++) { int i = order[k]; Ent *e = i < 0 ? &tr : &others[i]; int col = e->steps ? cyc[e->steps % 4] : 1, ci = i < 0 ? 0 : party[i].idx;
        blit(sheet("characters", i < 0 ? hero.sheet : party[i].sheet), (ci % 4) * 3 * S48 + col * S48, (ci / 4) * 4 * S48 + e->dir * S48, S48, S48, OX + e->x * T, OY + e->y * T - 8 * T / S48); }
    if (!g_bg_only) { zed_load(); if (zed_ok && zed.room == vroom) blit(sheet("characters", "Monster"), 1 * S48, 0 * S48 + zed.dir * S48, S48, S48, OX + zed.x * T, OY + zed.y * T - 8 * T / S48); }      /* the zed: first Monster sheet picture, standing frame */
#elif PET_SECTION == 22
static void clock_stamp(char *out, size_t cap);
static void log_stamped(const char *who, const char *text) { char st[24], ln[300]; clock_stamp(st, sizeof st); snprintf(ln, sizeof ln, "%s%s%s %s", st, st[0] ? " " : "", who, text); append_line("chat.txt", ln); chat_trim(); }
static const char *log_name(const char *id, char *buf, size_t n) { if (!strcmp(id, "zed")) return "the zed"; snprintf(buf, n, "%s", id); if (buf[0] >= 'a' && buf[0] <= 'z') buf[0] = (char)(buf[0] - 32); return buf; }
/* the event log (grok 2026-10-17, from-sonnet-question-event-log.md): "D1 08:02 Rex crafts bandage" - the game clock, the actor, one present-tense line; appended once per newly resolved EVENT into chat.txt */
static long ge_compact_limit(void) { if (!g_tnn) load_tune(); return (long)tune("ledger_compact_kb", 4096) * 1024L; }      /* a verb process never ran load_tune(): read tuning.pdl on first use */
static void ge_failnote(const char *why) { note_set(why); }      /* a ledger over the hard cap: the reason goes to the note line (S1-EVENT-LEDGER.md) */
static void ge_logline(const char *actor, const char *verb, const char *target, const char *res) { char t[160], st[24], an[40], tn[40], ln[300]; t[0] = 0; (void)tn; if (!strcmp(verb, "strike") && !strncmp(res, "wound:", 6)) for (int i = 0; i < nwd; i++) if (!strcmp(wd[i].who, target) && !strcmp(PARTS[wd[i].part], res + 6)) { snprintf(t, sizeof t, "hits %s: %s %d/%d", log_name(target, tn, sizeof tn), res + 6, wd[i].left, wd[i].max); break; }
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
    int chat = flag_on("chat", 1), hb = flag_on("hb", 0), in = flag_on("int", 0); char view[16]; flag_str("view", "2d", view, sizeof view); for (char *c = view; *c; c++) if (*c >= 'a' && *c <= 'z') *c -= 32;
    char nbuf[200]; { char np[PATH_MAX]; spath(np, "note.txt"); read_file(np, nbuf, sizeof nbuf); size_t nl = strlen(nbuf); while (nl && (nbuf[nl - 1] == '\n' || nbuf[nl - 1] == '\r')) nbuf[--nl] = 0; for (char *q_ = nbuf; *q_; q_++) if ((unsigned char)*q_ < 32) *q_ = ' '; }
    W("title=cdda-hq  -  %s  -  coins %d\nscene_raw=%s\ncanvas_raw=%s\ncoins=%d\nnote=%s\nmsg=%s\nroom_label=%s\n", rooms[vroom].name, coins, raw, raw, coins, nbuf, msg && msg[0] ? msg : (in ? "INT on: turn based like a roguelike. Time moves when you act. Tab switches pet/player." : "real time: the pets live on their own. INT lets you control a pet or the player."), rooms[vroom].name);
#elif PET_SECTION == 28
    W("left_visible=%s\n", flag_on("left", 1) ? "1" : "");      /* the pinned info bar (default on, Menu > Info show / hide, its _ button) */
    W("chat_visible=%s\nhb_visible=%s\nview_label=view %s\nchat_toggle_label=window %s\n", chat ? "1" : "", hb ? "1" : "", view, chat ? "on" : "off");
    { char cp[PATH_MAX], cb[4000]; spath(cp, "chat.txt"); read_tail(cp, cb, sizeof cb); char *ln[10] = {0}; int k = 0; for (char *l = strtok(cb, "\n"); l; l = strtok(NULL, "\n")) { ln[k % 10] = l; k++; } for (int i = 0; i < 10; i++) { int j = k - 10 + i; const char *cl = j >= 0 ? ln[j % 10] : ""; W("chat_%d=%s\nchat_cls_%d=%s\n", i, cl, i, (strstr(cl, " cannot ") || strstr(cl, " hits ") || strstr(cl, " has no ") || strstr(cl, " finds no ")) ? "rp-chat-bad" : (strstr(cl, " finds ") || strstr(cl, " crafts ")) ? "rp-chat-ok" : ""); }      /* the last ten lines (was six): the event log shares this scroll */

    { char hl[11][64]; char hb[16]; flag_str("help_page", "0", hb, sizeof hb); int pg = atoi(hb), np = help_pages(-1, hl); if (pg < 0 || pg >= np) pg = 0; help_pages(pg, hl); snprintf(hl[0], 64, "help - page %d of %d", pg + 1, np); W("help_visible=%s\n", flag_on("help", 0) ? "1" : ""); for (int i = 0; i < 11; i++) W("help_%d=%s\n", i, hl[i]); } }
#elif PET_SECTION == 29
#elif PET_SECTION == 30
    /* TABS (tab_brief 2026-10-17): Inventory / Craft / Body / Look rows, built from the ledger replay (read-only ge_scan, rerun only when events.txt or recipes.pdl changed size), recipes.pdl, the wound table and the room. Labels stay under 25 characters. */
    { static long ksz = -1; char ep2[PATH_MAX], rp2[PATH_MAX]; spath(ep2, "events.txt"); snprintf(rp2, sizeof rp2, "%s/recipes.pdl", APP); long key = fsize(ep2) * 1000003L + fsize(rp2); if (key != ksz) { ksz = key; ge_scan(ep2, (long)time(NULL), 0, NULL); }
      int ci2 = ctl_idx(); const char *who2 = ci2 < 0 ? "trainer" : party[ci2].id; Ent *me2 = ci2 < 0 ? &tr : &others[ci2]; char lb[64];
      int ni = 0; for (int i = 0; i < ge_ninv && ni < 7; i++) if (!strcmp(ge_inv[i].actor, who2) && ge_inv[i].n > 0) ni++;
      W("n_inv=%d\n", ni ? ni : 1); if (!ni) W("inv_0_label=(bag empty)\n"); ni = 0; for (int i = 0; i < ge_ninv && ni < 7; i++) if (!strcmp(ge_inv[i].actor, who2) && ge_inv[i].n > 0) { W("inv_%d_label=%.14s x%ld\n", ni, ge_inv[i].item, ge_inv[i].n); ni++; }
      int nc = nrec < 6 ? nrec : 6; W("n_craft=%d\n", nc ? nc : 1); if (!nc) W("craft_0_label=(no recipes)\ncraft_0_id=none\n"); for (int i = 0; i < nc; i++) W("craft_%d_label=Craft %.12s %dm\ncraft_%d_id=%s\n", i, recs[i].id, recs[i].minutes, i, recs[i].id);
      W("n_body=6\n"); for (int pi = 0; pi < 6; pi++) { int left = mhp_of(who2) / 6, mx = left; for (int i = 0; i < nwd; i++) if (!strcmp(wd[i].who, who2) && wd[i].part == pi) { left = wd[i].left; mx = wd[i].max; } W("body_%d_label=%s %d/%d\n", pi, PARTS[pi], left, mx); }
      int nl = 0; zed_load(); int adx = me2->dir == 1 ? -1 : me2->dir == 2 ? 1 : 0, ady = me2->dir == 0 ? 1 : me2->dir == 3 ? -1 : 0, ax = me2->x + adx, ay = me2->y + ady;
#define LK(...) do { snprintf(lb, sizeof lb, __VA_ARGS__); if (nl < 8) { W("look_%d_label=%.24s\n", nl, lb); nl++; } } while (0)
      if (zed_ok && zed.room == me2->room) LK("zed at %d,%d", zed.x, zed.y); else LK("no zed here");
      { const char *ah = "nothing"; char hb2[40]; if (zed_ok && zed.room == me2->room && zed.x == ax && zed.y == ay) ah = "the zed"; else { int di = door_here(me2->room, ax, ay); if (di >= 0) { snprintf(hb2, sizeof hb2, "to %s", rooms[doors[di].dest].name); ah = hb2; } else for (int i = 0; i < npl; i++) if (pl[i].room == me2->room && pl[i].x == ax && pl[i].y == ay) { snprintf(hb2, sizeof hb2, "%s", cat[pl[i].item].label); ah = hb2; break; } } LK("ahead: %s", ah); }
      int np2 = 0; for (int i = 0; i < npl; i++) if (pl[i].room == me2->room) { np2++; if (np2 <= 3) LK("%.12s at %d,%d", cat[pl[i].item].label, pl[i].x, pl[i].y); } if (np2 > 3) LK("+%d more", np2 - 3); if (!np2) LK("no items");
      int nd2 = 0; for (int i = 0; i < ndoor && nl < 8; i++) if (doors[i].room == me2->room) { nd2++; LK("to %s", rooms[doors[i].dest].name); } if (!nd2) LK("no doors");
      W("n_look=%d\n", nl); }
#elif PET_SECTION == 31
    pet.room = d->dest; g_gr = pet.room; pet.x = d->ax; pet.y = d->ay; pet.dir = 0; pet.tx = pet.ty = -1; { int fx, fy; if (free_near(pet.x, pet.y, &fx, &fy)) { pet.x = fx; pet.y = fy; } }      /* land on the nearest FREE cell, not on whoever is already there */
    if (me >= 0 && me == active && (pet.room == vroom || di_from_room == vroom)) { snprintf(b, sizeof b, "moves to the %s", rooms[pet.room].name); log_stamped(party[me].name, b); }      /* "D1 08:20 Rex moves to the Cellar" (grok 2026-10-17) */
#elif PET_SECTION == 32
    if (msg && leg_hurt(me < 0 ? "trainer" : party[me].id)) { size_t ml = strlen(msg); snprintf(msg + ml, cap - ml, " (slow: a leg is wounded)"); }      /* RULES-cdda-hq.md: a leg at 0 = walk answers slow, still one cell */
#elif PET_SECTION == 33
#elif PET_SECTION == 34
/* M3 decision ledger (BOOK 16.game/DECISION-LEDGER-AND-DAILY-REVIEW.md): one DEC row per autonomous choice of a pet (pick a target, walk, dig, blocked), written
 * AFTER the step from the before/after state, so ai_step_inner's own logic is untouched. Idle steps (nothing chosen) write nothing. */
static void dec_bank(void) { char dp[PATH_MAX], bp[PATH_MAX]; spath(dp, "decisions.txt"); spath(bp, "bank"); dec_bank_update(dp, bp, "cdda-hq"); }      /* DEC rows -> bank entries (state/bank/dec_cdda-hq_<action>.behavior.pdl) */
static void dec_log(const char *actor, const char *state, const char *action, const char *target, const char *outcome) {
    char p[PATH_MAX], st[24]; static int since; spath(p, "decisions.txt"); clock_stamp(st, sizeof st); dec_append(p, st, actor, state, action, target, outcome);
    if (++since >= 200) { since = 0; dec_bank(); } }      /* the bank follows the ledger every 200 decisions, and on the review verb */
/* the read-back that closes the loop: how readily a pet picks a new target. 45 % with no evidence; a low success weight of its own walk choice (it keeps being blocked)
 * raises it (it gives up and retargets sooner), a high one lowers it (it keeps its goal longer). Clamped 15..85; the file is re-read at most every 10 s. */
static int dec_pick_pct(void) {
    static double w = 0.5; static time_t at; time_t nw = time(NULL);
    if (nw - at >= 10) { char bp[PATH_MAX]; spath(bp, "bank"); w = dec_weight(bp, "cdda-hq", "walk"); at = nw; }
    int pct = 45 + (int)((0.5 - w) * 60.0); return pct < 15 ? 15 : pct > 85 ? 85 : pct; }
static int ai_step_inner(int i);
#elif PET_SECTION == 35
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
#elif PET_SECTION == 36
#elif PET_SECTION == 37
    if (pet.tx < 0 && rand() % 100 < dec_pick_pct()) pick_target();
#elif PET_SECTION == 38
/* the one zed (RULES-cdda-hq.md): a single hostile actor in the Cellar, state/zed.txt (room x y dir; absent = the default spot). After the controlled mover's turn, if it is in the zed's room the zed acts: adjacent = it strikes (an EVENT strike, resolved), else one bfs step toward it.
 * No spawn table, no second monster, and it acts only in turn mode (INT on) in this slice. */
static void zed_load(void) {
    char p[PATH_MAX], b[200], rid[16] = "cellar"; int x = 6, y = 7, d = 0; spath(p, "zed.txt"); if (fexists(p)) { read_file(p, b, sizeof b); char *q = strstr(b, "room="); if (q) sscanf(q + 5, "%15[^\n]", rid); q = strstr(b, "x="); if (q) x = atoi(q + 2); q = strstr(b, "y="); if (q) y = atoi(q + 2); q = strstr(b, "dir="); if (q) d = atoi(q + 4); }
    memset(&zed, 0, sizeof zed); zed.room = room_idx(rid); zed.x = x; zed.y = y; zed.dir = d & 3; zed.tx = zed.ty = -1; if (zed.room >= 0) { int cw = rooms[zed.room].cols, ch = rooms[zed.room].rows; if (cw < 1) cw = 1; if (ch < 1) ch = 1; if (zed.x < 0 || zed.x >= cw) zed.x = cw / 2; if (zed.y < 0 || zed.y >= ch) zed.y = ch / 2; } zed_ok = zed.room >= 0;
}
static void zed_save(void) { char p[PATH_MAX], b[120]; spath(p, "zed.txt"); snprintf(b, sizeof b, "room=%s\nx=%d\ny=%d\ndir=%d\n", rooms[zed.room].id, zed.x, zed.y, zed.dir); write_if_changed(p, b, strlen(b)); }
static void zed_turn(void) {
    zed_load(); if (!zed_ok) return; int c = ctl_idx(); Ent *pl = c < 0 ? &tr : &others[c]; const char *who = c < 0 ? "trainer" : party[c].id; if (pl->room != zed.room) return;
    char ep[PATH_MAX]; spath(ep, "events.txt");
    if (abs(pl->x - zed.x) + abs(pl->y - zed.y) == 1) { char at[48], er[40], nb[120]; long t = (long)time(NULL); snprintf(at, sizeof at, "%s:%d,%d", rooms[zed.room].id, pl->x, pl->y);
        { int okz = ge_append(ep, t, "zed", "strike", who, at, er, sizeof er); dec_log("zed", "adjacent", "strike", who, okz ? "hit" : "refused"); if (!okz) return; }
        if (ge_scan(ep, t, 1, NULL)) { write_wounds(); const char *pt = strchr(ge_last, ':'); int l = 0, mx = 0; if (pt) for (int i = 0; i < nwd; i++) if (!strcmp(wd[i].who, who) && !strcmp(PARTS[wd[i].part], pt + 1)) { l = wd[i].left; mx = wd[i].max; } snprintf(nb, sizeof nb, "the zed hits %s: %s %d/%d", who, pt ? pt + 1 : "?", l, mx); note_set(nb); } return; }
    Ent sv = pet; int sme = me; pet = zed; me = -2; int dx = 0, dy = 0, ok = bfs_first(pl->x, pl->y, &dx, &dy); pet = sv; me = sme;
    if (ok && (dx || dy)) { zed.dir = dy > 0 ? 0 : dx < 0 ? 1 : dx > 0 ? 2 : 3; zed.x += dx; zed.y += dy; zed_save(); dec_log("zed", "hunting", "chase", who, "moved"); }
}
#elif PET_SECTION == 39
static void end_turn(void) { if (!run_on() || !turn_mode()) return; clock_turn(); int in = flag_on("int", 0), c = ctl_idx(); for (int i = 0; i < nparty; i++) if (!(in && i == c)) ai_step(i); use_ent(active); zed_turn(); use_ent(active); }      /* the pet you control does not act on its own; then the zed */
#elif PET_SECTION == 40
#elif PET_SECTION == 41
#elif PET_SECTION == 42
#elif PET_SECTION == 43
    snprintf(d, sizeof d, "%s/pieces/system/maps/cdda-hq/%s", PCHQ, rooms[r].id); mkdir(d, 0755);
    for (int i = 0; i < nparty; i++) if (others[i].room == r) { int pat = others[i].steps % 3; write_frame(party[i].sheet, party[i].idx, others[i].dir, pat); unsigned char px[4] = {128, 128, 128, 255}; frame_rgb(sheet("characters", party[i].sheet), party[i].idx, others[i].dir & 3, px);
        o += snprintf(buf + o, sizeof buf - o, "%d %d %d %d %d %s %d %d %d %c\n", others[i].x, others[i].y, px[0], px[1], px[2], party[i].sheet, party[i].idx, dc[others[i].dir & 3], pat, party[i].name[0] >= 'A' && party[i].name[0] <= 'Z' ? party[i].name[0] + 32 : (party[i].name[0] ? party[i].name[0] : 'm')); }
    if (tr.room == r) { int pat = tr.steps % 3; write_frame(hero.sheet, 0, tr.dir, pat); unsigned char px[4] = {200, 60, 60, 255}; frame_rgb(sheet("characters", hero.sheet), 0, tr.dir & 3, px);
        o += snprintf(buf + o, sizeof buf - o, "%d %d %d %d %d %s 0 %d %d @\n", tr.x, tr.y, px[0], px[1], px[2], hero.sheet, dc[tr.dir & 3], pat); }
    zed_load(); if (zed_ok && zed.room == r) { int pat = 1; write_frame("Monster", 0, zed.dir, pat); unsigned char px[4] = {90, 140, 80, 255}; frame_rgb(sheet("characters", "Monster"), 0, zed.dir & 3, px);      /* the zed is an entity row too (2026-10-17): the 3D book and the glyph view (letter z) show it where the 2D picture does */
        o += snprintf(buf + o, sizeof buf - o, "%d %d %d %d %d Monster 0 %d %d z\n", zed.x, zed.y, px[0], px[1], px[2], dc[zed.dir & 3], pat); }
#elif PET_SECTION == 44
    static const char leg[] = "W|3|150|150|160|-|wall|#\nD|3|170|110|50|-|door|+\nT|2|50|150|60|-|tree|T\nf|1|190|150|90|-|furniture|&\n.|0|90|80|60|-|floor|.\n";
#elif PET_SECTION == 45
static int viewer_kinds(void) {
    static long last = 0; static int v = 3; long t = now_ms(); if (last && t - last < tune("viewer_scan_ms", 2000)) return v; last = t; v = 0;
#elif PET_SECTION == 46
        if (!strstr(b, "khtpm_core_render")) continue; for (size_t i = 0; i < n; i++) if (!b[i]) b[i] = ' '; if (strstr(b, "/@.apps/cdda-hq/board/pchq-board.xhtpm")) v |= 1; if (strstr(b, "cdda-hq.xhtpm")) v |= 2; }
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
        snprintf(p, sizeof p, "%s/extrusion.pdl", d); { FILE *f = fopen_tmp(p); if (f) { fprintf(f, "SECTION      | KEY                | VALUE\n----------------------------------------------------------------------\nMETA         | map_id             | cdda-hq\nMETA         | desk_id            | %s\nMETA         | note               | %s (cdda-hq house, exported from the live house)\nEXTRUDE      | W                  | 3\nEXTRUDE      | D                  | 3\nEXTRUDE      | T                  | 2\nEXTRUDE      | f                  | 1\nEXTRUDE      | default            | 0\n", rooms[r].id, rooms[r].name); fclose_tmp(f, p); } }
#elif PET_SECTION == 55
static int copy_file(const char *from, const char *to) { FILE *in = fopen(from, "rb"); if (!in) return 0; char tmpc[PATH_MAX + 24]; snprintf(tmpc, sizeof tmpc, "%s.tmp.%d", to, (int)getpid()); FILE *out = fopen(tmpc, "wb"); if (!out) { fclose(in); return 0; } char buf[8192]; size_t n; int ok = 1; while ((n = fread(buf, 1, sizeof buf, in)) > 0) if (fwrite(buf, 1, n, out) != n) { ok = 0; break; } fclose(in); if (fclose(out) != 0) ok = 0; if (ok && rename(tmpc, to) != 0) ok = 0; if (!ok) remove(tmpc); return ok; }
#elif PET_SECTION == 56
    if (!strcmp(v, "helppanel")) {      /* Help tab: open / next / prev / close the help panel (help.txt, pages split by ---) */
        const char *w = argc >= 2 ? argv[1] : "open"; char hb[16]; flag_str("help_page", "0", hb, sizeof hb); int pg = atoi(hb), np = help_pages(-1, NULL);
        if (!strcmp(w, "close")) { set_flag("help", "0"); snprintf(msg, cap, "help closed"); return; }
        if (!strcmp(w, "next")) pg = (pg + 1) % np; else if (!strcmp(w, "prev")) pg = (pg + np - 1) % np; else if (!flag_on("help", 0)) pg = 0;
        snprintf(hb, sizeof hb, "%d", pg); set_flag("help_page", hb); set_flag("help", "1"); snprintf(msg, cap, "help page %d of %d", pg + 1, np); return;
    }
    if (!strcmp(v, "strike") || !strcmp(v, "wounds")) {      /* BOOK 10-user-docs/CDDA-HQ-CRAFT-HOWTO.md: the controlled mover hits an adjacent zed (an S1 EVENT strike), or lists who is wounded */
        int ci = ctl_idx(); const char *who = ci < 0 ? "trainer" : party[ci].id; char ep[PATH_MAX], nb[200], er[60]; spath(ep, "events.txt"); GeStats gs; long gnow = (long)time(NULL);
        use_ent(ci); Ent mv = pet; use_ent(active); zed_load();
        if (!ge_scan(ep, gnow, 0, &gs)) { snprintf(msg, cap, "ledger unreadable"); return; }
        if (!strcmp(v, "wounds")) { size_t o = 0; nb[0] = 0; for (int i = 0; i < nwd && o + 36 < sizeof nb; i++) if ((!strcmp(wd[i].who, who) || !strcmp(wd[i].who, "zed")) && wd[i].left < wd[i].max) o += (size_t)snprintf(nb + o, sizeof nb - o, "%s%s %s %d/%d", o ? ", " : "", wd[i].who, PARTS[wd[i].part], wd[i].left, wd[i].max); if (!o) snprintf(nb, sizeof nb, "nobody here is wounded"); note_set(nb); snprintf(msg, cap, "%s", nb); return; }
        if (!zed_ok || mv.room != zed.room || abs(mv.x - zed.x) + abs(mv.y - zed.y) != 1) { snprintf(nb, sizeof nb, "no zed next to %s", who); note_set(nb); snprintf(msg, cap, "%s", nb); return; }
        char at[48]; snprintf(at, sizeof at, "%s:%d,%d", rooms[zed.room].id, zed.x, zed.y);
        if (!ge_append(ep, gnow, who, "strike", "zed", at, er, sizeof er)) { snprintf(nb, sizeof nb, "refused: %s", er); note_set(nb); snprintf(msg, cap, "%s", nb); return; }
        if (!ge_scan(ep, gnow, 1, &gs)) { snprintf(msg, cap, "ledger not writable"); return; } write_wounds();
        { const char *pt = strchr(ge_last, ':'); int l = 0, mx = 0; if (pt) for (int i = 0; i < nwd; i++) if (!strcmp(wd[i].who, "zed") && !strcmp(PARTS[wd[i].part], pt + 1)) { l = wd[i].left; mx = wd[i].max; } snprintf(nb, sizeof nb, "%s hits the zed: %s %d/%d", who, pt ? pt + 1 : "?", l, mx); }
        note_set(nb); snprintf(msg, cap, "%s", nb); if (run_on() && turn_mode()) { end_turn(); use_ent(active); } return; }
    if (!strcmp(v, "craft") || !strcmp(v, "scavenge") || !strcmp(v, "bag") || !strcmp(v, "recipes") || !strcmp(v, "help")) {      /* survival basics (BOOK 10-user-docs/CDDA-HQ-CRAFT-HOWTO.md): the controlled mover crafts from its bag; every change is an S1 event */
        int ci = ctl_idx(); const char *who = ci < 0 ? "trainer" : party[ci].id; char ep[PATH_MAX], nb[200], er[60]; spath(ep, "events.txt"); GeStats gs; long gnow = (long)time(NULL);
        use_ent(ci); int room = pet.room, px = pet.x, py = pet.y; use_ent(active); char at[48]; snprintf(at, sizeof at, "%s:%d,%d", rooms[room].id, px, py);
        if (!ge_scan(ep, gnow, 0, &gs)) { snprintf(msg, cap, "ledger unreadable"); return; }
        if (!strcmp(v, "help")) {
            static const char *H[8] = { "1/8 Pick who acts: Menu > Control (Tab).", "2/8 Inventory > Scavenge rag finds one (a stand-in until a real source exists).", "3/8 Craft > bandage: 1 rag, 1 minute of game time. Inventory lists what you have.", "4/8 The Castle has a stair: walk through the door in its left wall to the Cellar.", "5/8 The Cellar has one zed. It acts only in INT (turn) mode, after each of your turns.", "6/8 Next to it: Body > Strike zed hits it. Body shows who is hurt.", "7/8 Six body parts, each starts at max HP / 6. One hit takes 1 from one part.", "8/8 A leg at 0 makes walking slow, but you still move one cell." };
            char hs[PATH_MAX], hb[16]; spath(hs, "help_step.txt"); read_file(hs, hb, sizeof hb); int s = ((atoi(hb) % 8) + 8) % 8; { char hv[16]; int hn = snprintf(hv, sizeof hv, "%d\n", s + 1); write_if_changed(hs, hv, (size_t)hn); } note_set(H[s]); snprintf(msg, cap, "%s", H[s]); return; }
        if (!strcmp(v, "bag") || !strcmp(v, "recipes")) { size_t o = 0; nb[0] = 0;
            if (!strcmp(v, "bag")) { for (int i = 0; i < ge_ninv; i++) if (!strcmp(ge_inv[i].actor, who) && ge_inv[i].n > 0 && o + 30 < sizeof nb) o += (size_t)snprintf(nb + o, sizeof nb - o, "%s %ld, ", ge_inv[i].item, ge_inv[i].n); if (o) nb[o - 2] = 0; else snprintf(nb, sizeof nb, "bag empty: Craft > Scavenge"); }
            else { for (int i = 0; i < nrec && o + 40 < sizeof nb; i++) o += (size_t)snprintf(nb + o, sizeof nb - o, "%s%s (%s)", i ? ", " : "", recs[i].id, recs[i].nin ? recs[i].in[0] : "-"); if (!nrec) snprintf(nb, sizeof nb, "no recipes (recipes.pdl)"); if (nrec_bad && strlen(nb) + 24 < sizeof nb) snprintf(nb + strlen(nb), sizeof nb - strlen(nb), "; %d bad rows", nrec_bad); }
            note_set(nb); snprintf(msg, cap, "%s", nb); return; }
        const char *verb = !strcmp(v, "scavenge") ? "give_item" : "craft", *tgt = !strcmp(v, "scavenge") ? "rag" : (argc >= 2 ? argv[1] : "");
        if (!strcmp(v, "craft") && !tgt[0]) { snprintf(nb, sizeof nb, "craft which? Craft > Craft bandage"); note_set(nb); snprintf(msg, cap, "%s", nb); return; }
        if (!ge_append(ep, gnow, who, verb, tgt, !strcmp(v, "scavenge") ? at : "-", er, sizeof er)) { snprintf(nb, sizeof nb, "refused: %s", er); note_set(nb); snprintf(msg, cap, "%s", nb); return; }
        if (!ge_scan(ep, gnow, 1, &gs)) { snprintf(msg, cap, "ledger not writable"); return; }
        if (!strcmp(v, "scavenge")) snprintf(nb, sizeof nb, "found a rag (bag %ld)", ge_inv_get(who, "rag"));
        else if (!strcmp(ge_last, "ok")) { Recipe *rc = cd_recipe(tgt); snprintf(nb, sizeof nb, "crafted %s x%d (bag %ld)", rc ? rc->out : tgt, rc ? rc->cnt : 0, rc ? ge_inv_get(who, rc->out) : 0L);
            if (rc && rc->minutes > 0) { char dur[16]; snprintf(dur, sizeof dur, "%dm", rc->minutes); clock_paths(); if (clock_ok() && fexists(CFILE)) { lc_run(0, "cmd", "rpg", "advance", dur, "--source", "rpg-craft", NULL); lc_run(0, "step", "5", NULL); } } }      /* the minutes are charged here, once, never in the replay */
        else if (cd_recipe(tgt) && cd_recipe(tgt)->tool) snprintf(nb, sizeof nb, "cannot craft %s: it needs a tool (not built yet)", tgt); else if (cd_recipe(tgt)) snprintf(nb, sizeof nb, "cannot craft %s: need the inputs in the bag", tgt); else snprintf(nb, sizeof nb, "no recipe named %s (Craft > Help)", tgt);
        note_set(nb); snprintf(msg, cap, "%s", nb); return; }
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
        write_wounds();      /* the readable copy of the wound table follows every scan (event / resolve / inventory) */
        if (!strcmp(v, "inventory")) { size_t o = 0; msg[0] = 0; for (int i = 0; i < ge_ninv; i++) if ((argc < 2 || !strcmp(argv[1], ge_inv[i].actor)) && ge_inv[i].n > 0 && o + 60 < cap) o += (size_t)snprintf(msg + o, cap - o, "%s:%s=%ld ", ge_inv[i].actor, ge_inv[i].item, ge_inv[i].n); if (!o) snprintf(msg, cap, "(nothing)"); return; }
        snprintf(msg, cap, "%s: %ld events, %ld resolved now, %ld rejected now (%ld rejects in all); last result %s", v, gs.events, gs.resolved_new, gs.rejected_new, gs.rejects_total, ge_last[0] ? ge_last : "-"); return; }
#elif PET_SECTION == 57
    if (!strcmp(v, "newgame")) {      /* a fresh game: the current one is saved to slot 0 first (Undo new game = Load slot 0), then every game file is removed so the next read starts from the defaults. The clock is not touched. */
#elif PET_SECTION == 58
        static const char *fixed[] = { "house.txt", "trainer.txt", "relations.txt", "chat.txt", "active.txt", "daylight.txt", "flag_ctl.txt", "flag_follow.txt", "events.txt", "note.txt", "help_step.txt", "zed.txt", "wounds.pdl" };
#elif PET_SECTION == 59
        pet_load(); replay();
        snprintf(msg, cap, "new game started; the game you left is in slot 0 (Player > Undo new game)"); return; }
    if (!strcmp(v, "reset")) { char p[PATH_MAX], q[PATH_MAX]; spath(p, "house.txt"); snprintf(q, sizeof q, "%s/house.txt.%ld.bak", ST, now); if (fexists(p)) rename(p, q); snprintf(msg, cap, "house reset (the old ledger is kept as house.txt.%ld.bak)", now); return; }
#elif PET_SECTION == 60
        char p[PATH_MAX], bb[40]; spath(p, "active.txt"); snprintf(bb, sizeof bb, "%s\n", party[k].id); write_if_changed(p, bb, strlen(bb)); set_flag("follow", "pet"); set_flag("ctl", "pet"); active = k; calc_view(); snprintf(msg, cap, "watching %s (in the %s)", party[k].name, rooms[others[k].room].name); return; }
#elif PET_SECTION == 61
#elif PET_SECTION == 62
        char names[40][48]; int nn = 0; snprintf(names[nn++], 48, "house.txt"); snprintf(names[nn++], 48, "trainer.txt"); snprintf(names[nn++], 48, "relations.txt"); snprintf(names[nn++], 48, "chat.txt"); snprintf(names[nn++], 48, "active.txt"); snprintf(names[nn++], 48, "daylight.txt"); snprintf(names[nn++], 48, "flag_ctl.txt"); snprintf(names[nn++], 48, "flag_follow.txt"); snprintf(names[nn++], 48, "events.txt"); snprintf(names[nn++], 48, "zed.txt"); snprintf(names[nn++], 48, "wounds.pdl");      /* events.txt is bigger than the 64 KB read buffer below: streamed */
#elif PET_SECTION == 63
            if (!strcmp(names[i], "events.txt")) { if (copy_file(a, bpath)) ok++; continue; }
#elif PET_SECTION == 64
        if (!saving) { char b[40]; read_file(mp, b, sizeof b); long long ms = atoll(b); if (ms > 0) { clock_paths(); clock_install(); char ma[32]; snprintf(ma, sizeof ma, "%lld", ms); lc_run(0, "cmd", "rpg", "settime", ma, "--source", "rpg-load", NULL); lc_run(0, "step", "5", NULL); } pet_load(); }
        snprintf(msg, cap, "%s slot %d (%d files)", saving ? "saved to" : "loaded from", slot, ok); return; }
#elif PET_SECTION == 65
    if (!strcmp(v, "toggle") && argc >= 2 && (!strcmp(argv[1], "chat") || !strcmp(argv[1], "hb") || !strcmp(argv[1], "voice") || !strcmp(argv[1], "left"))) { set_flag(argv[1], flag_on(argv[1], !strcmp(argv[1], "voice") || !strcmp(argv[1], "chat") || !strcmp(argv[1], "left")) ? "0" : "1");      /* the defaults of voice and chat are on: a toggle with no flag file yet must turn them OFF */ snprintf(msg, cap, "%s toggled", argv[1]); return; }
#elif PET_SECTION == 66
    signal(SIGTERM, bye); signal(SIGINT, bye); signal(SIGHUP, bye); srand((unsigned)(time(NULL) ^ getpid()));
#elif PET_SECTION == 67
        pet_load(); int in = flag_on("int", 0), tm = turn_mode(); static int prev_in = 0; static long int_on_at = 0; if (in && !prev_in) int_on_at = now_ms(); prev_in = in;
#elif PET_SECTION == 68
        if (in && n) { char kl[300], one[16]; kl[0] = 0; for (int i = 0; i < n && i < 20; i++) { snprintf(one, sizeof one, "%d ", codes[i]); strncat(kl, one, sizeof kl - strlen(kl) - 1); } char kp[PATH_MAX]; spath(kp, "keys_seen.txt"); write_if_changed(kp, kl, strlen(kl)); }      /* last batch of codes, for debugging */
#elif PET_SECTION == 69
        { char dp[PATH_MAX], db[40]; spath(dp, "daylight.txt"); read_file(dp, db, sizeof db); if (strcmp(db, lastday)) { snprintf(lastday, sizeof lastday, "%s", db); dirty = 1; } }
#elif PET_SECTION == 70
    signal(SIGPIPE, SIG_IGN);      /* a closed pipe / dead child must never kill a verb or the daemon (the children get SIG_DFL back before exec) */
    signal(SIGPIPE, SIG_IGN);      /* a closed pipe / dead child must never kill a verb or the daemon (the children get SIG_DFL back before exec) */
    snprintf(ST, sizeof ST, "%s/state", APP); mkdir(ST, 0755);
    snprintf(PCHQ, sizeof PCHQ, "%s/cdda-hq-pchq", APP);
#elif PET_SECTION == 71
    load_data(); replay(); pet_load(); clock_paths();
#endif
#endif
