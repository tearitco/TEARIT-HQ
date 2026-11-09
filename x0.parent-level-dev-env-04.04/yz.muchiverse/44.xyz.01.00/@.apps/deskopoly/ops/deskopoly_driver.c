/* deskopoly_driver - the game side of PLAY for the Deskopoly book inside rpg-edit-hq. 2026-11-08.
 *   deskopoly_driver <house_root> <book_app_dir> <editor_app_dir>
 * Started by the editor's Play (rpg_edit_hq `play` reads <book>/game.pdl `GAME | driver | ...`); one instance only (flock). It is the RPG Maker idea: Play launches the game, the game opens on its own
 * setup ("title") screen, and the editor's Stop freezes it again.
 *
 * It decides NO game rule: every fact comes from the engine (<book>/ops/+x/deskopoly.+x) by fork/exec of its verbs against ONE ledger folder <editor>/state/games/deskopoly. It publishes the generic GAME BAR feed
 * <editor>/state/game_ui.txt (keys gm_on, gm_title, gm_nt + gmt_<i>_text, gm_nb + gmb_<i>_label / gmb_<i>_act, plus the engine's ui keys) that the editor window draws in its game popup; the buttons run
 * deskopoly_ctl, which appends one command line to <editor>/state/games/deskopoly/action.txt that this driver reads.
 * Owner rule "no ghost data": the ledger is the only authority for the game. Speed and the setup choices are window PREFERENCES (they change WHEN a verb is asked for, never an outcome); who plays a seat is a
 * `seat`/`join` row. PAUSE IS NOT OURS: the editor's switch running (state/world/switches.txt, Player > Play / Stop) is the one authority; this driver ticks only while running=1.
 * Cost: idle = a stat() and an 80 ms nap per loop; the engine is asked for a move only while a game is on, running and not over. Files: game_ui.txt (rewritten), games/deskopoly/action.txt (capped at 64 KiB),
 * games/archive (the last 3 finished games). Ends when the editor is gone (parent pid 1) or on SIGTERM.
 * Commands: players_up players_down human_next seed_next speed_next start step new_game roll buy skip end jail_pay jail_roll pay_debt bankrupt pass bid_min bid_up; the TRADE screen: tr_open tr_close tr_counter tr_amend tr_cancel tr_accept tr_reject tr_partner tr_send tr_g<tile> tr_r<tile> tr_gk<0|1> tr_rk<0|1> tr_gc+ tr_gc- tr_rc+ tr_rc- tr_pa+ tr_pb+ (modes new / counter / amend, see the M9 block). */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <dirent.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/wait.h>
#define P 2048

static char HOUSE[P], BOOK[P], EDIT[P], ENG[P], GAME[P], GDIR[P], FEED[P], ACT[P], ARCH[P], CTL[P], SW[P];
static long OWNER = 0;   /* the pid that owns this game (the editor window): the driver ends when it is gone (GAME_OWNER_PID, set by the editor's Play) */
static int mode_play = 0, speed = 1, nplayers = 3, human_seat = 0, seed = 7, dirty = 1;
static char msg[200] = "Choose the players, then START";
static const int INTERVAL_MS[3] = { 120, 550, 1400 };
static const char *SPEED_NAME[3] = { "fast", "normal", "slow" };

static void catn(char *d, size_t cap, const char *s) { strncat(d, s, cap - strlen(d) - 1); }
static long now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000L + t.tv_nsec / 1000000L; }
static int running(void) { FILE *f = fopen(SW, "r"); if (!f) return 0; char ln[80]; int r = 0; while (fgets(ln, sizeof ln, f)) if (!strncmp(ln, "running=", 8)) r = atoi(ln + 8); fclose(f); return r; }

/* run the engine; stdout+stderr into out; returns the exit code (-1 if it could not run). A 10 s alarm in the child ends a hung engine. */
static int run_eng(char *out, size_t cap, int na, const char **a) {
    int fd[2]; if (pipe(fd)) return -1; if (out) out[0] = 0;
    pid_t pid = fork(); if (pid < 0) return -1;
    if (pid == 0) { alarm(10); close(fd[0]); dup2(fd[1], 1); dup2(fd[1], 2); close(fd[1]);
        const char *av[16]; int k = 0; av[k++] = ENG; av[k++] = BOOK; av[k++] = GAME; for (int i = 0; i < na && k < 15; i++) av[k++] = a[i]; av[k] = NULL; execv(ENG, (char *const *)av); _exit(127); }
    close(fd[1]); size_t n = 0; char c[512]; ssize_t r;
    while ((r = read(fd[0], c, sizeof c)) > 0) { if (out && n + (size_t)r < cap) { memcpy(out + n, c, (size_t)r); n += (size_t)r; out[n] = 0; } }
    close(fd[0]); int st = 0; waitpid(pid, &st, 0); return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}
static void first_line(char *s) { char *e = strpbrk(s, "\r\n"); if (e) *e = 0; }
static int eng1(const char *v, const char *x, char *out, size_t cap) { const char *a[2] = { v, x }; return run_eng(out, cap, x ? 2 : 1, a); }

/* THE MAP: the live game is drawn as its own generated desk "play" of the book (the engine's `page` verb: decorations, owner markers, tokens; it appends a byte to cells.marker, the house reload rule),
 * never into the authored "board" desk. While a game is on, the editor world (state.txt desk_id) points at "play"; when the game is reset or this driver ends it points back at "board".
 * Throttled to one page per 250 ms. */
static char DESKDIR[P], ATLAS[P], WORLD[P]; static int page_pending = 0; static long last_page = 0;
static void set_desk(const char *want) {
    FILE *f = fopen(WORLD, "r"); if (!f) return; static char buf[4096]; size_t n = fread(buf, 1, sizeof buf - 1, f); buf[n] = 0; fclose(f); static char out[4400]; out[0] = 0; int saw = 0, same = 0;
    for (char *p = buf; *p; ) { char *e = strchr(p, '\n'); size_t ll = e ? (size_t)(e - p) : strlen(p);
        if (ll >= 8 && !strncmp(p, "desk_id=", 8)) { snprintf(out + strlen(out), sizeof out - strlen(out), "desk_id=%s\n", want); saw = 1; if (ll == 8 + strlen(want) && !strncmp(p + 8, want, strlen(want))) same = 1; }
        else if (ll < 600) snprintf(out + strlen(out), sizeof out - strlen(out), "%.*s\n", (int)ll, p);
        if (!e) break; p = e + 1; }
    if (!saw || same) return; char tmp[P + 8]; snprintf(tmp, sizeof tmp, "%s.tmp", WORLD); FILE *w = fopen(tmp, "w"); if (!w) return; fputs(out, w); fclose(w); rename(tmp, WORLD);
}
/* 2D: the RPG Maker picture of the page (map.png beside map.txt, which the 2D renderer blits 1:1 and draws the events.txt characters over). Made from the page by the engine's `preview` verb and the house raw_to_png op. */
static int run_prog(char *const av[]) { pid_t pid = fork(); if (pid < 0) return -1; if (pid == 0) { alarm(10); int fd = open("/dev/null", O_RDWR); if (fd >= 0) { dup2(fd, 1); dup2(fd, 2); } execv(av[0], av); _exit(127); } int st = 0; waitpid(pid, &st, 0); return WIFEXITED(st) ? WEXITSTATUS(st) : -1; }
static void make_map_png(void) {
    char raw[P + 16], png[P + 16], cp[P + 16], r2p[P + 96]; snprintf(raw, sizeof raw, "%s/map.raw", DESKDIR); snprintf(png, sizeof png, "%s/map.png", DESKDIR); snprintf(cp, sizeof cp, "%s/cells.txt", DESKDIR); snprintf(r2p, sizeof r2p, "%s/&.widgits/_shared-lib/ops/+x/raw_to_png.+x", HOUSE);
    const char *b[3] = { "preview", DESKDIR, raw }; if (run_eng(NULL, 0, 3, b) != 0) return;
    FILE *f = fopen(cp, "r"); if (!f) return; char ln[256]; int W = 0, H = 0, px = 24; for (int i = 0; i < 6 && fgets(ln, sizeof ln, f); i++) { if (!strncmp(ln, "width=", 6)) W = atoi(ln + 6); else if (!strncmp(ln, "height=", 7)) H = atoi(ln + 7); else if (!strncmp(ln, "tile_px=", 8)) px = atoi(ln + 8); } fclose(f);
    if (W < 1 || H < 1) return; char sz[40]; snprintf(sz, sizeof sz, "%dx%d", W * px, H * px); char *av[] = { r2p, raw, png, (char *)"--size", sz, NULL }; run_prog(av); unlink(raw);
}

/* The 3D walk frames of the characters (People1 x 8 x 4 directions x 3 patterns) are made once per game start by deskopoly_frames: only the MISSING ones, never overwriting a frame another game made. */
static void ensure_frames(void) {
    char ex[P], sheet[P + 64], fr[P + 64], as[P]; snprintf(ex, sizeof ex, "%s/ops/+x/deskopoly_frames.+x", BOOK); snprintf(as, sizeof as, "%s/../../../#.NNEST_ASSETS/tsots-characters", HOUSE);
    snprintf(sheet, sizeof sheet, "%s/People1.png", as); snprintf(fr, sizeof fr, "%s/frames", as); if (access(ex, X_OK) || access(sheet, R_OK) || access(fr, W_OK)) return;
    char *av[] = { ex, sheet, (char *)"People1", fr, NULL }; run_prog(av);
}
static void page_sync(int force) {
    long t = now_ms(); if (!force && t - last_page < 250) { page_pending = 1; return; }
    mkdir(DESKDIR, 0755); const char *a[4] = { "page", DESKDIR, ATLAS, "noevents" }; run_eng(NULL, 0, 4, a); make_map_png(); last_page = t; page_pending = 0;
}
/* BUILD THE PROJECTION, THEN POINT AT IT: the play desk is made fresh (the previous session's character rows are emptied first, the page and the 2D picture rewritten) and only then does the editor world switch to it.
 * The old order switched first, so for about 100 ms the editor showed the LAST game's characters on the new board (seen in the dsk_driver case 2026-11-08: a stale row was still there when the world flipped). */
static void play_desk_fresh(void) {
    char ev[P + 16]; mkdir(DESKDIR, 0755); snprintf(ev, sizeof ev, "%s/events.txt", DESKDIR); FILE *f = fopen(ev, "w"); if (f) fclose(f);
    ensure_frames(); page_sync(1); set_desk("play");
}

static char UI[20000];
static int load_ui(void) { char p[P]; snprintf(p, sizeof p, "%s/ui.txt", GAME); FILE *f = fopen(p, "r"); if (!f) { UI[0] = 0; return 0; } size_t n = fread(UI, 1, sizeof UI - 1, f); UI[n] = 0; fclose(f); return 1; }
static const char *uget(const char *k, char *dst, size_t cap) {
    size_t kl = strlen(k); dst[0] = 0; for (char *p = UI; *p; ) { char *e = strchr(p, '\n'); size_t ll = e ? (size_t)(e - p) : strlen(p);
        if (ll > kl && !strncmp(p, k, kl) && p[kl] == '=') { size_t vl = ll - kl - 1; if (vl >= cap) vl = cap - 1; memcpy(dst, p + kl + 1, vl); dst[vl] = 0; return dst; } if (!e) break; p = e + 1; }
    return dst;
}
static int uint_(const char *k) { char t[64]; return atoi(uget(k, t, sizeof t)); }
static void ring_xy(int idx, int n, int *bx, int *by) { int s = n - 1; if (idx <= s) { *bx = idx; *by = 0; } else if (idx <= 2 * s) { *bx = s; *by = idx - s; } else if (idx <= 3 * s) { *bx = s - (idx - 2 * s); *by = s; } else { *bx = 0; *by = s - (idx - 3 * s); } }

/* WALKING (owner 2026-11-08: the players move around the board physically, in 2D RPG Maker and in 3D). The engine says WHERE each player is (p<i>_pos); this driver animates the walk: a character steps one map
 * cell at a time along the ring (3 cells per tile) toward its tile, and writes the characters to the play desk's events.txt, which the 3D engine re-reads every frame and the 2D picture blits (an RPG Maker
 * charset: People1, facing and walking pattern from the step). The next game move waits until everyone has arrived. A move that is not a forward walk of up to 12 tiles (a card, Go To Jail) is a jump. */
static int w_cur[8], w_sub[8], w_tgt[8], w_known[8], w_out[8], w_n = 0, w_nt = 40, walking_now = 0, w_step = 0, w_resync = 1;
static const int W_MS[3] = { 60, 130, 240 };
static void walk_reset(void) { memset(w_known, 0, sizeof w_known); w_n = 0; walking_now = 0; w_resync = 1; }
static void walk_sync(void) {
    load_ui(); int np = uint_("n_players"); w_nt = uint_("n_tiles"); if (w_nt < 4) w_nt = 40; w_n = np > 8 ? 8 : np; char k[24];
    for (int i = 0; i < w_n; i++) { snprintf(k, sizeof k, "p%d_pos", i + 1); int tg = uint_(k); snprintf(k, sizeof k, "p%d_out", i + 1); w_out[i] = uint_(k);
        if (!w_known[i]) { w_known[i] = 1; w_cur[i] = tg; w_sub[i] = 0; }
        else if (tg != w_tgt[i]) { int d = (tg - w_cur[i] + w_nt) % w_nt; if (d == 0 || d > 12) { w_cur[i] = tg; w_sub[i] = 0; } }
        w_tgt[i] = tg; }
    w_resync = 0;
}
static int walk_step(void) {
    int moved = 0, any = 0;
    for (int i = 0; i < w_n; i++) if (!w_out[i] && w_cur[i] != w_tgt[i]) { any = 1; moved = 1; if (++w_sub[i] >= 3) { w_cur[i] = (w_cur[i] + 1) % w_nt; w_sub[i] = 0; } if (w_cur[i] == w_tgt[i] && w_sub[i] == 0) any = any; }
    int still = 0; for (int i = 0; i < w_n; i++) if (!w_out[i] && w_cur[i] != w_tgt[i]) still = 1; (void)any; walking_now = still; if (moved) w_step++; return moved;
}
static void write_events(void) {
    static const int pr[8][3] = {{220, 60, 60}, {60, 120, 230}, {60, 190, 90}, {230, 200, 50}, {180, 80, 200}, {60, 200, 200}, {240, 140, 40}, {200, 200, 200}};
    static const int ox[4] = {0, -1, 1, 0}, oy[4] = {0, -1, -1, 1}; static const int patt[4] = {0, 1, 2, 1}; static char evb[2048]; evb[0] = 0; int share[64]; memset(share, 0, sizeof share);
    for (int i = 0; i < w_n; i++) if (!w_out[i]) { int bx, by, nx, ny, t = w_cur[i], nt2 = (t + 1) % w_nt; ring_xy(t, 11, &bx, &by); ring_xy(nt2, 11, &nx, &ny);
        int dx = nx > bx ? 1 : nx < bx ? -1 : 0, dy = ny > by ? 1 : ny < by ? -1 : 0; int cx = bx * 3 + 1, cy = by * 3 + 1, dir = 2, pat = 1;
        if (w_cur[i] != w_tgt[i]) { cx += dx * w_sub[i]; cy += dy * w_sub[i]; dir = dx > 0 ? 6 : dx < 0 ? 4 : dy > 0 ? 2 : 8; pat = patt[w_step % 4]; }
        else { int s = share[t < 64 ? t : 0]++; if (s > 3) s = 3; cx += ox[s]; cy += oy[s]; }
        snprintf(evb + strlen(evb), sizeof evb - strlen(evb), "%d %d %d %d %d People1 %d %d %d %c\n", cx, cy, pr[i][0], pr[i][1], pr[i][2], i, dir, pat, (char)(65 + i)); }   /* the 10th field (the glyph) MUST be a non-number: the 3D loader reads 9 or 15 fields and a digit parses as a float = 10 fields = the row is silently dropped (3D showed no players until 2026-11-08) */
    char tmp[P + 16]; snprintf(tmp, sizeof tmp, "%s/events.txt.tmp", DESKDIR); char fin[P + 16]; snprintf(fin, sizeof fin, "%s/events.txt", DESKDIR); mkdir(DESKDIR, 0755);
    FILE *f = fopen(tmp, "w"); if (!f) return; fputs(evb, f); fclose(f); rename(tmp, fin);
}

/* the feed: text rows + the buttons that are legal NOW. Every button's action string is complete (the editor's repeat just substitutes it). */
static int nb_, nt_; static char *OUT_; static size_t OUTCAP_;
static void add_text(const char *s) { char b[400]; snprintf(b, sizeof b, "gmt_%d_text=%s\n", nt_++, s[0] ? s : " "); catn(OUT_, OUTCAP_, b); }
static int ne_, nl_; static char ENT_[400]; static void add_ent(const char *s) { if (ENT_[0]) strncat(ENT_, "   ", sizeof ENT_ - strlen(ENT_) - 1); strncat(ENT_, s, sizeof ENT_ - strlen(ENT_) - 1); }   /* all players are ONE wide footer item (separate items do not fit beside the footer buttons) */
static void add_chat(const char *s) { char b[300]; snprintf(b, sizeof b, "gml_%d_text=%s\n", nl_++, s[0] ? s : " "); catn(OUT_, OUTCAP_, b); }
static void add_btn(const char *label, const char *cmd) { char b[P + 300]; snprintf(b, sizeof b, "gmb_%d_label=%s\ngmb_%d_act='%s' %s '%s'\n", nb_, label, nb_, CTL, cmd, GDIR); nb_++; catn(OUT_, OUTCAP_, b); }

/* THE BOARD PICTURE ONLY CHANGES WHEN OWNERSHIP, BUILDINGS OR MORTGAGES DO (players are drawn from events.txt, not from the page). Rebuilding it per tick cost three engine launches (page replays the ledger, preview renders
 * 792x792, raw_to_png compresses 2.5 MB) AND grew cells.marker, which makes the 3D engine reload the desk: measured 2026-11-08 at 6.1% of a core for the driver while a game ran (5.3% of it in those children).
 * A byte cursor reads only the NEW ledger rows. FAIL-SAFE: only verbs known not to touch the picture are skipped; any other row (buy, build, mortgage, start, and verbs that do not exist yet: auctions, trades) rebuilds. */
static long pg_cur = 0;
static int page_needed(void) {
    char p[P]; snprintf(p, sizeof p, "%s/ledger.txt", GAME); FILE *f = fopen(p, "r"); if (!f) { pg_cur = 0; return 1; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); int need = 0; if (sz < pg_cur) { pg_cur = 0; need = 1; }   /* a smaller ledger = a new game */
    fseek(f, pg_cur, SEEK_SET); char ln[600];
    static const char *quiet[] = { "roll", "order", "first", "end_turn", "skip", "bid", "pass", "offer", "counter", "amend", "reject", "cancel", "expire", "jail_pay", "jail_roll", "jail_card", "lobby", "join", "seat", NULL };   /* M7: a bid or a pass never changes the picture (a pets game makes ~115 bids: rebuilding the page for each was the 6% CPU burn again); the SALE does, as the non-quiet auction_end row; M9: offer/counter/amend/reject/cancel/expire never change the picture, but ACCEPT moves tiles and is NOT quiet (an unsold auction rebuilds once, harmlessly) */
    while (fgets(ln, sizeof ln, f)) { char *q = ln; for (int k = 0; k < 3 && q; k++) { q = strchr(q, '|'); if (q) q++; } if (!q) { need = 1; continue; }
        char *e = strchr(q, '|'); size_t vl = e ? (size_t)(e - q) : strcspn(q, "\r\n"); int isq = 0; for (int k = 0; quiet[k]; k++) if (strlen(quiet[k]) == vl && !strncmp(q, quiet[k], vl)) isq = 1; if (!isq) need = 1; }
    pg_cur = ftell(f); fclose(f); return need;
}
/* ===== M9 TRADE SCREEN (owner 2026-11-08: "a menu to show both traders' inventory and 2 empty panes for dropping things in or out for proposing deals", "use nav to click in or out"). FOUR panes left to right: [my tiles] [I give] [I get] [their tiles],
 * drawn by the editor layout's canvas-overlay-at rows tpa / tpg / tpr / tpb (positions and contents come from this feed, nothing is hard-wired per game). Pressing a tile in an inventory MOVES it into the neighbouring offer pane; pressing it there moves it back;
 * cash goes in steps of 10. The DRAFT lives only in this process (a preference: it changes no outcome, a restart loses it); Send calls the canonical `offer` verb, so a pet or a harness sees the same row. ===== */
#define TR_PAGE 4
static int tr_open_ = 0, tr_partner = 0, tr_pg[2]; static unsigned long long tr_give_t, tr_get_t; static int tr_give_c, tr_get_c;
/* ONE draft model, three modes (2026-11-08): 0 NEW offer (`offer`), 1 COUNTER to an offer addressed to me (`counter <id>`: the draft starts as the SWAPPED terms, the partner is the proposer and cannot be changed),
 * 2 AMEND my own open offer (`amend <id>`: the draft starts as my terms, the partner is the addressee). tr_oid = the offer id of modes 1 and 2. tr_gk / tr_rk = Get Out card bits (bit 0 luck, bit 1 fund) carried over from a
 * prefilled offer so a counter does not silently drop a card; a card can only be taken OUT of the draft here. The draft is a PREFERENCE (this process only); the ledger gets a row only when Send is pressed. */
static int tr_mode = 0, tr_oid = 0, tr_gk = 0, tr_rk = 0;
static int tr_me(void) { return human_seat > 0 ? human_seat : 1; }
static int tr_active(int i) { char k[24], v[8]; snprintf(k, sizeof k, "p%d_out", i); return i >= 1 && i <= uint_("n_players") && uget(k, v, sizeof v)[0] != '1'; }
static void tr_clear(void) { tr_give_t = tr_get_t = 0; tr_give_c = tr_get_c = 0; tr_pg[0] = tr_pg[1] = 0; tr_mode = 0; tr_oid = 0; tr_gk = tr_rk = 0; }
static const char *TR_DK[2] = { "luck", "fund" };   /* the ENGINE words (commands, ledger): never shown to a player as they are */
static const char *deck_disp(int d, char *o, size_t cap) { char k[16]; snprintf(k, sizeof k, "deck%d_name", d); uget(k, o, cap); if (!o[0]) snprintf(o, cap, "%s", TR_DK[d]); return o; }   /* what the player sees: the ui.txt deck names (page.pdl LABEL rows) */
static void tr_parse(const char *as, unsigned long long *mask, int *cash, int *cards) {   /* "prop:8+cash:50+card:luck" (the engine's canonical form) -> the draft fields */
    char tmp[160]; snprintf(tmp, sizeof tmp, "%s", as); *mask = 0; *cash = 0; *cards = 0; char *sv = NULL;
    for (char *t = strtok_r(tmp, "+", &sv); t; t = strtok_r(NULL, "+", &sv)) { if (!strncmp(t, "prop:", 5)) { int n = atoi(t + 5); if (n >= 0 && n < 64) *mask |= 1ULL << n; } else if (!strncmp(t, "cash:", 5)) *cash = atoi(t + 5); else if (!strncmp(t, "card:", 5)) for (int d = 0; d < 2; d++) if (!strcmp(t + 5, TR_DK[d])) *cards |= 1 << d; }
}
static int tr_owns(int pl, int t) { char k[24], v[16], idb[8]; snprintf(k, sizeof k, "t%d_owner", t); snprintf(idb, sizeof idb, "p%d", pl); return !strcmp(uget(k, v, sizeof v), idb); }
static int tr_tradable(int t) { char k[24], v[8]; snprintf(k, sizeof k, "t%d_trade", t); return uget(k, v, sizeof v)[0] == '1'; }
static void tr_tilename(int t, char *o, size_t cap) { char k[24], nm[40], mo[8]; snprintf(k, sizeof k, "t%d_label", t); uget(k, nm, sizeof nm); if (!nm[0]) { snprintf(k, sizeof k, "t%d_name", t); uget(k, nm, sizeof nm); }   /* the player-facing label (page.pdl LABEL rows), the id when the feed has none */
    snprintf(k, sizeof k, "t%d_mort", t); uget(k, mo, sizeof mo); snprintf(o, cap, "%s%s", nm, mo[0] ? " (mortgaged)" : ""); }
static void tr_assets(unsigned long long mask, int cash, int cards, char *o, size_t cap) { o[0] = 0; for (int t = 0; t < 64; t++) if (mask >> t & 1) { size_t l = strlen(o); snprintf(o + l, cap - l, "%sprop:%d", o[0] ? "+" : "", t); } if (cash > 0) { size_t l = strlen(o); snprintf(o + l, cap - l, "%scash:%d", o[0] ? "+" : "", cash); }
    for (int d = 0; d < 2; d++) if (cards >> d & 1) { size_t l = strlen(o); snprintf(o + l, cap - l, "%scard:%s", o[0] ? "+" : "", TR_DK[d]); } if (!o[0]) snprintf(o, cap, "-"); }
static void tr_nice(const char *as, char *o, size_t cap) {   /* prop:8+cash:50 -> "Sky2 + $50" for the player's eyes */
    char tmp[160]; snprintf(tmp, sizeof tmp, "%s", as); o[0] = 0; char *sv = NULL; for (char *t = strtok_r(tmp, "+", &sv); t; t = strtok_r(NULL, "+", &sv)) { char one[60]; if (!strncmp(t, "prop:", 5)) tr_tilename(atoi(t + 5), one, sizeof one); else if (!strncmp(t, "cash:", 5)) snprintf(one, sizeof one, "$%s", t + 5); else if (!strncmp(t, "card:", 5)) { char dn[80]; snprintf(one, sizeof one, "%s card", deck_disp(!strcmp(t + 5, "fund"), dn, sizeof dn)); } else snprintf(one, sizeof one, "nothing"); size_t l = strlen(o); snprintf(o + l, cap - l, "%s%s", o[0] ? " + " : "", one); }
    if (!o[0]) snprintf(o, cap, "nothing"); }
static void tp_head(char id, int x, const char *title) { char b[200]; snprintf(b, sizeof b, "tp%c_x=%d\ntp%c_y=8\ntp%c_title=%s\n", id, x, id, id, title); catn(OUT_, OUTCAP_, b); }
static void tp_item(char id, int *n, const char *label, const char *cmd) { char b[P + 300]; snprintf(b, sizeof b, "tp%c_%d_label=%s\ntp%c_%d_act='%s' %s '%s'\n", id, *n, label, id, *n, CTL, cmd, GDIR); (*n)++; catn(OUT_, OUTCAP_, b); }
static void tp_end(char id, int n) { char b[40]; snprintf(b, sizeof b, "tp%c_n=%d\n", id, n); catn(OUT_, OUTCAP_, b); }
static void tr_inventory(char id, int x, int owner_pl, unsigned long long moved, const char *mvcmd, int pg_idx, const char *pgcmd) {
    char title[80], nm[24], ca[24], k[24], cmd[24], lab[70]; snprintf(k, sizeof k, "p%d_name", owner_pl); uget(k, nm, sizeof nm); snprintf(k, sizeof k, "p%d_cash", owner_pl); uget(k, ca, sizeof ca); snprintf(title, sizeof title, "p%d %s $%s holds", owner_pl, nm, ca); tp_head(id, x, title);
    int nt = uint_("n_tiles"), cnt = 0, n = 0; for (int t = 0; t < nt; t++) if (tr_owns(owner_pl, t) && tr_tradable(t) && !(moved >> t & 1)) cnt++;
    int pages = (cnt + TR_PAGE - 1) / TR_PAGE; if (pages < 1) pages = 1; if (tr_pg[pg_idx] >= pages) tr_pg[pg_idx] = 0; int skip = tr_pg[pg_idx] * TR_PAGE, shown = 0, seen = 0;
    for (int t = 0; t < nt; t++) if (tr_owns(owner_pl, t) && tr_tradable(t) && !(moved >> t & 1)) { if (seen++ < skip) continue; if (shown >= TR_PAGE) break; tr_tilename(t, lab, sizeof lab); snprintf(cmd, sizeof cmd, "%s%d", mvcmd, t); tp_item(id, &n, lab, cmd); shown++; }
    if (!cnt) tp_item(id, &n, "(nothing to trade)", "void"); if (pages > 1) { snprintf(lab, sizeof lab, "more... (page %d of %d)", tr_pg[pg_idx] + 1, pages); tp_item(id, &n, lab, pgcmd); } tp_end(id, n);
}
static void tr_offerpane(char id, int x, const char *title, unsigned long long mask, int cash, int cards, const char *mvcmd, const char *up, const char *dn, const char *upf, const char *dnf) {
    tp_head(id, x, title); int nt = uint_("n_tiles"), n = 0; char lab[80], cmd[24]; snprintf(lab, sizeof lab, upf, cash); tp_item(id, &n, lab, up); snprintf(lab, sizeof lab, dnf, cash); tp_item(id, &n, lab, dn);
    for (int t = 0; t < nt; t++) if (mask >> t & 1) { char nm[60]; tr_tilename(t, nm, sizeof nm); snprintf(lab, sizeof lab, "%s (press: back)", nm); snprintf(cmd, sizeof cmd, "%s%d", mvcmd, t); tp_item(id, &n, lab, cmd); }
    for (int d = 0; d < 2; d++) if (cards >> d & 1) { { char dn[80]; snprintf(lab, sizeof lab, "%s card (press: back)", deck_disp(d, dn, sizeof dn)); } snprintf(cmd, sizeof cmd, "%sk%d", mvcmd, d); tp_item(id, &n, lab, cmd); } tp_end(id, n);
}
static void tr_screen(void) {   /* the feed while the trade screen is open: four panes + a three-button control popup under them */
    char b[300], v[8]; int me = tr_me(), pa = tr_partner; load_ui();
    if (pa < 1 || pa == me || !tr_active(pa)) { pa = 0; for (int i = 1; i <= uint_("n_players"); i++) if (i != me && tr_active(i)) { pa = i; break; } tr_partner = pa; }
    char tl[8]; uget("trade_left", tl, sizeof tl); char pn[24], k[24]; snprintf(k, sizeof k, "p%d_name", pa); uget(k, pn, sizeof pn);
    catn(OUT_, OUTCAP_, "tp_on=1\ngame-ov_x=344\ngame-ov_y=270\n"); (void)v;
    if (tr_mode == 1) snprintf(b, sizeof b, "COUNTER to p%d's offer #%d", pa, tr_oid);
    else if (tr_mode == 2) snprintf(b, sizeof b, "AMEND your offer #%d to p%d", tr_oid, pa);
    else snprintf(b, sizeof b, "p%d offers p%d  (%s new offers left)", me, pa, tl);
    add_text(b);
    snprintf(b, sizeof b, "Partner: p%d %s", pa, pn);
    add_btn(tr_mode == 1 ? "Send counter" : tr_mode == 2 ? "Send amendment" : "Send offer", "tr_send");
    if (tr_mode == 0) add_btn(b, "tr_partner"); else add_text(b);   /* the partner of a counter / amendment is fixed by the offer */
    add_btn("Back to game", "tr_close");
    tr_inventory('a', 0, me, tr_give_t, "tr_g", 0, "tr_pa+"); tr_offerpane('g', 224, "you GIVE", tr_give_t, tr_give_c, tr_gk, "tr_g", "tr_gc+", "tr_gc-", "give $%d: add 10", "give $%d: take back 10"); tr_offerpane('r', 448, "you GET", tr_get_t, tr_get_c, tr_rk, "tr_r", "tr_rc+", "tr_rc-", "ask $%d: ask 10 more", "ask $%d: ask 10 less"); tr_inventory('b', 672, pa, tr_get_t, "tr_r", 1, "tr_pb+");
}
static int tr_bits(unsigned long long m) { int n = 0; while (m) { n += (int)(m & 1); m >>= 1; } return n; }
static int tr_pid_of(const char *id) { return (id[0] == 'p' && id[1] >= '1' && id[1] <= '9' && !id[2]) ? id[1] - '0' : 0; }   /* "p3" -> 3 (0 when it is not a player id) */
/* the open offer addressed to ME (the ui's show_offer) / my OWN open offer (offer_from = me and show_offer empty); fills the id; returns 1 when it exists */
static int tr_incoming(int me, int *oid, int *from) { char v[8], to[16], fr[16], id[16]; if (!uget("show_offer", v, sizeof v)[0]) return 0; uget("offer_to", to, sizeof to); if (tr_pid_of(to) != me) return 0; uget("offer_from", fr, sizeof fr); uget("offer_id", id, sizeof id); *oid = atoi(id); *from = tr_pid_of(fr); return *oid > 0 && *from > 0; }
static int tr_own(int me, int *oid, int *to) { char v[8], to_[16], fr[16], id[16]; if (uget("show_offer", v, sizeof v)[0]) return 0; uget("offer_from", fr, sizeof fr); if (tr_pid_of(fr) != me) return 0; uget("offer_to", to_, sizeof to_); uget("offer_id", id, sizeof id); *oid = atoi(id); *to = tr_pid_of(to_); return *oid > 0 && *to > 0; }
static void tr_cmd(const char *c) {
    if (!mode_play) return; load_ui(); int me = tr_me(); char out[400], k[24], v[24]; int oid = 0, who = 0;
    if (!strcmp(c, "open")) { tr_clear(); tr_partner = 0; tr_open_ = 1; snprintf(msg, sizeof msg, "trade: press a tile to move it in or out"); }
    else if (!strcmp(c, "counter")) { if (!tr_incoming(me, &oid, &who)) snprintf(msg, sizeof msg, "no offer for you to counter");
        else { char og[130], ot[130]; uget("offer_give", og, sizeof og); uget("offer_get", ot, sizeof ot); tr_clear(); tr_mode = 1; tr_oid = oid; tr_partner = who;
            tr_parse(ot, &tr_give_t, &tr_give_c, &tr_gk); tr_parse(og, &tr_get_t, &tr_get_c, &tr_rk);   /* what they ask becomes my GIVE, what they give becomes my GET */
            tr_open_ = 1; snprintf(msg, sizeof msg, "counter: change the terms, then Send counter"); } }
    else if (!strcmp(c, "amend")) { if (!tr_own(me, &oid, &who)) snprintf(msg, sizeof msg, "you have no open offer to amend");
        else { char og[130], ot[130]; uget("offer_give", og, sizeof og); uget("offer_get", ot, sizeof ot); tr_clear(); tr_mode = 2; tr_oid = oid; tr_partner = who;
            tr_parse(og, &tr_give_t, &tr_give_c, &tr_gk); tr_parse(ot, &tr_get_t, &tr_get_c, &tr_rk); tr_open_ = 1; snprintf(msg, sizeof msg, "amend: change the terms, then Send amendment"); } }
    else if (!strcmp(c, "cancel")) { if (!tr_own(me, &oid, &who)) snprintf(msg, sizeof msg, "you have no open offer to cancel");
        else { char id[16], as[24]; snprintf(id, sizeof id, "%d", oid); snprintf(as, sizeof as, "as=p%d", me); const char *a[3] = { "cancel", id, as }; int rc = run_eng(out, sizeof out, 3, a); first_line(out); eng1("ui", NULL, NULL, 0); w_resync = 1; tr_open_ = 0; tr_clear(); snprintf(msg, sizeof msg, "%s%.170s", rc ? "refused: " : "you: ", out); } }
    else if (!strcmp(c, "accept") || !strcmp(c, "reject")) { char id[16], to[16], as[24]; uget("offer_id", id, sizeof id); uget("offer_to", to, sizeof to); snprintf(as, sizeof as, "as=%s", to); const char *a[3] = { c, id, as }; int rc = run_eng(out, sizeof out, 3, a); first_line(out); eng1("ui", NULL, NULL, 0); w_resync = 1; snprintf(msg, sizeof msg, "%s%.170s", rc ? "refused: " : "you: ", out); }
    else if (!strcmp(c, "close")) { tr_open_ = 0; tr_clear(); }
    else if (!tr_open_) { /* every other trade command edits the DRAFT: with the screen closed there is no draft to edit (a stale or hostile line changes nothing) */ }
    else if (!strcmp(c, "partner")) { if (tr_mode == 0) { int n = uint_("n_players"), p = tr_partner ? tr_partner : me; for (int s = 1; s <= n; s++) { int q = (p - 1 + s) % n + 1; if (q != me && tr_active(q)) { tr_partner = q; break; } } tr_get_t = 0; tr_get_c = 0; tr_rk = 0; tr_pg[1] = 0; } }
    else if ((c[0] == 'g' || c[0] == 'r') && c[1] == 'k' && (c[2] == '0' || c[2] == '1') && !c[3]) { if (c[0] == 'g') tr_gk &= ~(1 << (c[2] - '0')); else tr_rk &= ~(1 << (c[2] - '0')); }   /* a carried-over card can only be taken back out */
    else if ((c[0] == 'g' || c[0] == 'r') && c[1] >= '0' && c[1] <= '9') { long tl = strtol(c + 1, NULL, 10); int give = c[0] == 'g'; unsigned long long *m = give ? &tr_give_t : &tr_get_t; int owner_pl = give ? me : tr_partner;
        if (tl < 0 || tl > 63) { /* not a tile: ignored (a shift by 64 or more is undefined in C) */ } else { int t = (int)tl; if (*m >> t & 1) *m &= ~(1ULL << t); else if (tr_owns(owner_pl, t) && tr_tradable(t) && tr_bits(*m) < 8) *m |= 1ULL << t; } }
    else if (!strcmp(c, "gc+") || !strcmp(c, "gc-")) { snprintf(k, sizeof k, "p%d_cash", me); int mx = atoi(uget(k, v, sizeof v)); tr_give_c += c[2] == '+' ? 10 : -10; if (tr_give_c < 0) tr_give_c = 0; if (tr_give_c > mx) tr_give_c = mx; }
    else if (!strcmp(c, "rc+") || !strcmp(c, "rc-")) { snprintf(k, sizeof k, "p%d_cash", tr_partner); int mx = atoi(uget(k, v, sizeof v)); tr_get_c += c[2] == '+' ? 10 : -10; if (tr_get_c < 0) tr_get_c = 0; if (tr_get_c > mx) tr_get_c = mx; }
    else if (!strcmp(c, "pa+")) tr_pg[0]++; else if (!strcmp(c, "pb+")) tr_pg[1]++;
    else if (!strcmp(c, "send")) { char gs[130], rs[130], pn[8], as[8], ids[16]; tr_assets(tr_give_t, tr_give_c, tr_gk, gs, sizeof gs); tr_assets(tr_get_t, tr_get_c, tr_rk, rs, sizeof rs); snprintf(pn, sizeof pn, "p%d", tr_partner); snprintf(as, sizeof as, "as=p%d", me); snprintf(ids, sizeof ids, "%d", tr_oid);
        const char *a_new[7] = { "offer", pn, "give", gs, "get", rs, as }; const char *a_old[7] = { tr_mode == 1 ? "counter" : "amend", ids, "give", gs, "get", rs, as };   /* counter <id> / amend <id>: the engine checks the id, the actor and the terms */
        int rc = run_eng(out, sizeof out, 7, tr_mode ? a_old : a_new); first_line(out); eng1("ui", NULL, NULL, 0); w_resync = 1;
        if (rc == 0) { int md = tr_mode; tr_open_ = 0; tr_clear(); snprintf(msg, sizeof msg, md == 1 ? "counter sent to %s" : md == 2 ? "offer amended (%s)" : "offer sent to %s", pn); } else snprintf(msg, sizeof msg, "refused: %.170s", out); }
}
/* ===== CAMERA SNAP (owner 2026-11-08: "cam should snap to entity esp p1"). When it becomes a HUMAN seat's turn, and on the popup button `Find me`, the 3D camera is brought to that player's figure. HOW (found with positive controls
 * in the live editor 2026-11-08, PNGs in the commit message): the engine's default camera is mode 2 (third-person), which sits tp_distance BEHIND the xelector (the editor's cursor, <editor>-pchq/pieces/xelector_01/state.txt pos_x/pos_y)
 * and looks along cam_yaw; the engine re-reads that file every frame it renders. So: write pos_x/pos_y there (the same two lines the engine's own arrow handler writes), mirror them into the session's bv_state.txt selector_x/y (the arrow
 * handler reads THAT copy, so the next arrow press moves from the new cell), then append one redraw nudge (126) to the session's interact_relay.txt: WITHOUT the nudge the engine does not re-render (checked: a file write alone left the old picture).
 * Writing the board's selector_x/y alone moves nothing (the 2026-11-08 trap). The xelector is aimed `camera_lead` cells BEYOND the figure along the view direction: aimed at the figure itself, the figure sits exactly at the middle of the
 * picture, which is where the game popup stands (the first live try hid p1 behind it); 4 cells beyond puts it in the clear strip under the popup. It is a VIEW preference: no ledger row, no game fact. It runs only on a turn change or the button
 * (never per tick). Nothing happens when: the owner's tuning row `TUNE | camera_follow | 0`; the camera is not mode 2 (the owner flies a free camera); the xelector possesses an entity OTHER than the editor's own hero_01 (the owner's choice; it is pinned to that entity; hero_01, the state of every fresh editor launch, is let go of like the engine's key 9 does); no board session. ===== */
static char PCHQ[P];
static int tune_int(const char *name, int dflt) {   /* TUNE | name | value | ... rows of <book>/tuning.pdl */
    char p[P + 16]; snprintf(p, sizeof p, "%s/tuning.pdl", BOOK); FILE *f = fopen(p, "r"); if (!f) return dflt; char ln[400]; int v = dflt; size_t nl = strlen(name);
    while (fgets(ln, sizeof ln, f)) { if (strncmp(ln, "TUNE", 4)) continue; char *a = strchr(ln, '|'); if (!a) continue; a++; while (*a == ' ') a++; if (strncmp(a, name, nl)) continue; char *b = a + nl; while (*b == ' ') b++; if (*b != '|') continue; v = atoi(b + 1); }
    fclose(f); return v;
}
static int kv_get(const char *path, const char *key, char *out, size_t cap) {   /* first `key=value` line; 1 when found */
    FILE *f = fopen(path, "r"); if (!f) return 0; char ln[1024]; size_t kl = strlen(key); int ok = 0;
    while (fgets(ln, sizeof ln, f)) if (!strncmp(ln, key, kl) && ln[kl] == '=') { first_line(ln); snprintf(out, cap, "%s", ln + kl + 1); ok = 1; break; }
    fclose(f); return ok;
}
static int kv_puts(const char *path, const char *key, const char *val) {   /* replace the `key=` line (or append it), file rewritten whole through a temp + rename */
    FILE *f = fopen(path, "r"); if (!f) return 0; static char buf[8192], nb[8300]; size_t n = fread(buf, 1, sizeof buf - 1, f); buf[n] = 0; fclose(f); nb[0] = 0; size_t kl = strlen(key); int done = 0;
    for (char *p = buf; *p; ) { char *e = strchr(p, '\n'); size_t ll = e ? (size_t)(e - p) : strlen(p);
        if (ll > kl && !strncmp(p, key, kl) && p[kl] == '=' && !done) { snprintf(nb + strlen(nb), sizeof nb - strlen(nb), "%s=%s\n", key, val); done = 1; } else snprintf(nb + strlen(nb), sizeof nb - strlen(nb), "%.*s\n", (int)ll, p);
        if (!e) break;
        p = e + 1; }
    if (!done) snprintf(nb + strlen(nb), sizeof nb - strlen(nb), "%s=%s\n", key, val);
    char tmp[P + 16]; snprintf(tmp, sizeof tmp, "%s.tmp", path); FILE *w = fopen(tmp, "w"); if (!w) return 0; fputs(nb, w); fclose(w); return rename(tmp, path) == 0;
}
static int kv_put(const char *path, const char *key, int val) { char b[24]; snprintf(b, sizeof b, "%d", val); return kv_puts(path, key, b); }
/* the map-cell of player i's figure (33x33 page, 3 cells per tile), the same arithmetic as write_events without the shared-tile spread: the tile centre plus the walking offset */
static int fig_cell(int i, int *cx, int *cy) {
    if (i < 0 || i >= w_n || !w_known[i] || w_out[i]) return 0;
    int t = w_cur[i], bx, by, nx, ny;
    ring_xy(t, 11, &bx, &by); ring_xy((t + 1) % w_nt, 11, &nx, &ny);
    *cx = bx * 3 + 1; *cy = by * 3 + 1; if (w_cur[i] != w_tgt[i]) { *cx += (nx > bx ? 1 : nx < bx ? -1 : 0) * w_sub[i]; *cy += (ny > by ? 1 : ny < by ? -1 : 0) * w_sub[i]; } return 1;
}
/* Snap the camera to map cell (fx, fy); returns 0 when the camera was moved, else a negative reason with `why` filled (shown in the popup). */
static int cam_snap(int fx, int fy, char *why, size_t wcap) {
    char ui[P + 32], sess[P] = "", v[64]; snprintf(ui, sizeof ui, "%s/board/state/ui.txt", EDIT);
    if (!kv_get(ui, "bv_session", sess, sizeof sess) || !sess[0]) { snprintf(why, wcap, "no 3D board session to aim"); return -1; }
    char bs[P + 48], xs[P + 48], rl[P + 64]; snprintf(bs, sizeof bs, "%s/pieces/system/bv_state.txt", sess); snprintf(xs, sizeof xs, "%s/pieces/xelector_01/state.txt", PCHQ); snprintf(rl, sizeof rl, "%s/pieces/apps/player_app/interact_relay.txt", sess);
    int mode = kv_get(bs, "camera_mode", v, sizeof v) ? atoi(v) : 2; if (mode != 2) { snprintf(why, wcap, "free camera in use: the follow camera (mode 2) snaps"); return -2; }
    if (kv_get(xs, "possessed_id", v, sizeof v) && v[0] && strcmp(v, "none")) {
        if (strcmp(v, "hero_01")) { snprintf(why, wcap, "the cursor is possessing %.30s", v); return -3; }
        kv_puts(xs, "last_possessed_id", v); kv_puts(xs, "possessed_id", "none");   /* a fresh editor launch has the cursor possessing the editor's own hero: that pins the camera to the hero, so the snap lets go of it exactly as the engine's key 9 does (last_possessed_id is kept: key 9 jumps back). Any OTHER possession is the owner's choice and is left alone. */
    }
    int yaw = kv_get(bs, "cam_yaw", v, sizeof v) ? atoi(v) : 180; yaw = ((yaw % 360) + 360) % 360; int q = ((yaw + 45) / 90) % 4;   /* view direction (sin yaw, cos yaw) on the nearest axis */
    static const int DX[4] = { 0, 1, 0, -1 }, DY[4] = { 1, 0, -1, 0 }; int lead = tune_int("camera_lead", 4); if (lead < 0) lead = 0; if (lead > 12) lead = 12;
    int tx = fx + DX[q] * lead, ty = fy + DY[q] * lead;
    if (tx < 0) tx = 0; if (ty < 0) ty = 0; if (tx > 32) tx = 32; if (ty > 32) ty = 32;   /* 2026-11-09: the page is 33x33; a figure on the top row (25,1) was aimed at y=-3 and the view was empty sky. Clamped, the cursor stays on the board (case dsk_camera, scenario edge) */
    if (!kv_put(xs, "pos_x", tx) || !kv_put(xs, "pos_y", ty)) { snprintf(why, wcap, "cannot move the cursor"); return -4; }
    kv_put(bs, "selector_x", tx); kv_put(bs, "selector_y", ty);
    FILE *r = fopen(rl, "a"); if (r) { fputs("126\n", r); fclose(r); }   /* the redraw nudge (bound to nothing) */
    return 0;
}
/* the seat of the first HUMAN player (from the feed's p<i>_driver), 0 when there is none: a continued game has no `you play` preference, the ledger knows who is human */
static int human_pid(void) { char k[24], v[16]; int n = uint_("n_players"); for (int i = 1; i <= n && i <= 8; i++) { snprintf(k, sizeof k, "p%d_driver", i); if (!strcmp(uget(k, v, sizeof v), "human")) return i; } return 0; }
static long msg_keep = 0;   /* the answer to `Find me` stays in the popup this long (4 s): while a human waits the loop would put "your turn" back within half a second */
static char snap_key[40] = "";   /* "<round>:<current>" of the last turn the camera was aimed for: one snap per turn, never per tick */
static int snap_for(int pl, char *why, size_t wcap) { int cx, cy; why[0] = 0; if (pl < 1 || !fig_cell(pl - 1, &cx, &cy)) { snprintf(why, wcap, "your figure is not on the board"); return -5; } return cam_snap(cx, cy, why, wcap); }
/* AUTO SNAP BOOKKEEPING. A snap that found no board session yet (-1) is retried on the next loops (at most 20, about 11 s) instead of being given up for the whole turn. The FIRST auto snap of a driver run is repeated once 6 s later: the editor relaunch
 * (board launcher) rewrites the cursor to its home cell a moment after a driver that started with it (seen live 2026-11-08: the cursor was back at 8,8 and the camera never moved). */
static int snap_tries = 0, snap_first_done = 0, snap_rep = 0; static long snap_again = 0, snap_t0 = 0;   /* 2026-11-09: snap_t0 = when the first try of this turn started (the retry window for "no board session yet" is 90 s of wall time, not 20 loops: a relaunch starts the driver BEFORE the engine session is announced, and after 11 s the snap gave up for good: figures drawn, camera elsewhere); snap_rep = repeats left after a successful FIRST snap */

static void publish(void) {
    static char o[40000]; o[0] = 0; OUT_ = o; OUTCAP_ = sizeof o; nb_ = nt_ = ne_ = nl_ = 0; ENT_[0] = 0; char t[256], k[48], b[600];
    int run = running(); int have = mode_play && load_ui(); int over = have ? uint_("over") : 0;
    catn(o, sizeof o, "gm_on=1\n"); { char gp[40]; snprintf(gp, sizeof gp, "gm_pid=%ld\n", (long)getpid()); catn(o, sizeof o, gp); }   /* 2026-11-09 the editor's driver watchdog checks this pid: a dead driver leaves gm_on=1 behind (crash), and a feed that still claims "on" fooled it for 45 minutes after a reboot */
    catn(o, sizeof o, "gm_title=TEROPOLY classic\n");
    /* IN-GAME FEATURES REPLACE THE EDITOR TOOLS WHILE A GAME IS ON: this feed is the LAST vars file of the editor window, so these keys override the editor ones (data, no renderer change). The palettes bar is hidden,
     * the pets bar is shown with the game players as its party, and the h-ai chat gives way to the game chat. When the game ends the feed drops to gm_on= and the editor values show again. */
    catn(o, sizeof o, "side_pal=\nside_left=1\nn_chat=0\nchat_is_hai=\nchat_is_edit=\nchat_is_net=\ngm_chat_title=TEROPOLY table talk\nnick_line=Teropoly players\nstat_line=pets play, you watch or take a seat\nstat_line2=Stop freezes, Play resumes\n");
    if (have) catn(o, sizeof o, UI);
    snprintf(b, sizeof b, "%.90s", msg); add_text(b);   /* the renderer wraps long popup text itself; the old 30-char cut hid "choose the players, then START" and "(opening throw)" */
    if (!have) { catn(o, sizeof o, "n_party=0\n");
        snprintf(b, sizeof b, "%d players, seed %d", nplayers, seed); add_text(b);
        add_btn("players -", "players_down"); add_btn("players +", "players_up");
        snprintf(b, sizeof b, "you play: %s", human_seat == 0 ? "none" : (snprintf(t, sizeof t, "p%d", human_seat), t)); add_btn(b, "human_next");
        snprintf(b, sizeof b, "seed %d (next)", seed); add_btn(b, "seed_next");
        snprintf(b, sizeof b, "speed: %s", SPEED_NAME[speed]); add_btn(b, "speed_next"); add_btn("START GAME", "start");
    } else {
        char ph[16], cur[16], cn[16], dr[16]; uget("phase", ph, sizeof ph); uget("current", cur, sizeof cur); uget("current_name", cn, sizeof cn); uget("current_driver", dr, sizeof dr);
        snprintf(b, sizeof b, "r%d %s  %s %s %s", uint_("round"), ph, cur, cn, over ? "OVER" : (run ? "RUNNING" : "STOPPED")); add_text(b);
        int np = uint_("n_players");
        for (int i = 1; i <= np; i++) { char nm[24], dv[12], ca[24], po[12], ou[8], ne[24], tu[8], jl[8]; snprintf(k, sizeof k, "p%d_name", i); uget(k, nm, sizeof nm); snprintf(k, sizeof k, "p%d_driver", i); uget(k, dv, sizeof dv); snprintf(k, sizeof k, "p%d_cash", i); uget(k, ca, sizeof ca);
            snprintf(k, sizeof k, "p%d_pos", i); uget(k, po, sizeof po); snprintf(k, sizeof k, "p%d_out", i); uget(k, ou, sizeof ou); snprintf(k, sizeof k, "p%d_net", i); uget(k, ne, sizeof ne); snprintf(k, sizeof k, "p%d_turn", i); uget(k, tu, sizeof tu); snprintf(k, sizeof k, "p%d_jail", i); uget(k, jl, sizeof jl);
            snprintf(b, sizeof b, "%sp%d %s%s%s", tu[0] ? ">" : "", i, ne, jl[0] ? " J" : "", ou[0] == '1' ? " X" : ""); (void)dv; add_ent(b); { char pb[220]; snprintf(pb, sizeof pb, "party_%d_label=%sp%d %s $%s\nparty_%d_id=gm%d\nparty_%d_cls=%s\n", i - 1, tu[0] ? "> " : "", i, nm, ca, i - 1, i, i - 1, tu[0] ? "active" : ""); catn(OUT_, OUTCAP_, pb); } }
        { char pb[40]; snprintf(pb, sizeof pb, "n_party=%d\n", np); catn(OUT_, OUTCAP_, pb); }
        for (int i = 7; i >= 0; i--) { char l[260]; snprintf(k, sizeof k, "logt_%d", i); uget(k, l, sizeof l); if (!l[0]) { snprintf(k, sizeof k, "log_%d", i); uget(k, l, sizeof l); } if (l[0]) add_chat(l); }   /* 2026-11-09 the table talk is the engine's SENTENCE (logt_, names + tile labels); an engine without it, or a row it left empty, shows the raw ledger text (log_) as before */
        char v[8];
        /* the screen closes itself when it can no longer do its job: a NEW offer needs my turn; a COUNTER needs the offer to still be open and addressed to me (a counter may be made when it is not my turn); an AMENDMENT needs my offer still open */
        if (tr_open_) { int me3 = tr_me(), id3 = 0, w3 = 0, ok = !over;
            if (tr_mode == 0) ok = ok && uget("human_turn", v, sizeof v)[0]; else if (tr_mode == 1) ok = ok && tr_incoming(me3, &id3, &w3) && id3 == tr_oid; else ok = ok && tr_own(me3, &id3, &w3) && id3 == tr_oid;
            if (!ok) { tr_open_ = 0; tr_clear(); } }
        if (tr_open_) tr_screen(); else {
        if (uget("show_roll", v, sizeof v)[0]) add_btn("Roll", "roll");
        if (uget("show_jail", v, sizeof v)[0]) { add_btn("Jail: roll for doubles", "jail_roll"); add_btn("Jail: pay 50", "jail_pay"); }
        if (uget("show_buy", v, sizeof v)[0]) { char pt[40]; uget("pending_label", pt, sizeof pt); if (!pt[0]) uget("pending_tile", pt, sizeof pt); snprintf(b, sizeof b, "Buy %s (%d)", pt, uint_("pending_price")); add_btn(b, "buy"); }
        if (uget("show_skip", v, sizeof v)[0]) add_btn("Skip", "skip");
        { char at[40]; uget("auction_label", at, sizeof at); if (!at[0]) uget("auction_tile", at, sizeof at); if (at[0]) { char ah[16], bd[16]; uget("auction_high", ah, sizeof ah); uget("auction_bidder", bd, sizeof bd);
            snprintf(b, sizeof b, "AUCTION %s %d%s%s", at, uint_("auction_bid"), ah[0] ? " by " : "", ah); (void)bd; add_text(b); } }   /* one short line: a long one wrapped to three and the first button covered its end (seen in the live window 2026-11-08) */
        if (uget("show_bid", v, sizeof v)[0]) { int mn = uint_("auction_min"); char ck[24]; snprintf(ck, sizeof ck, "%s_cash", cur); snprintf(b, sizeof b, "Bid %d", mn); add_btn(b, "bid_min"); if (uint_(ck) >= mn + 50) { snprintf(b, sizeof b, "Bid %d", mn + 50); add_btn(b, "bid_up"); } }   /* the human picks the amount: the minimum raise or 50 above it (the engine refuses anything it does not allow) */
        if (uget("show_pass", v, sizeof v)[0]) add_btn("Pass", "pass");
        if (uget("show_pay_debt", v, sizeof v)[0]) { snprintf(b, sizeof b, "Pay debt %d", uint_("debt")); add_btn(b, "pay_debt"); }
        if (uget("show_bankrupt", v, sizeof v)[0]) add_btn("Declare bankruptcy", "bankrupt");
        if (uget("show_end", v, sizeof v)[0]) add_btn("End turn", "end");
        { char tl[8]; uget("trade_left", tl, sizeof tl); if (uget("human_turn", v, sizeof v)[0] && atoi(tl) > 0 && (!strcmp(ph, "pre") || !strcmp(ph, "post") || !strcmp(ph, "debt"))) add_btn("Trade...", "tr_open"); }
        if (uget("show_offer", v, sizeof v)[0]) { char of[16], og[130], ot[130], gn[200], rn[200]; uget("offer_from", of, sizeof of); uget("offer_give", og, sizeof og); uget("offer_get", ot, sizeof ot); tr_nice(og, gn, sizeof gn); tr_nice(ot, rn, sizeof rn); snprintf(b, sizeof b, "%s OFFERS you:", of); add_text(b); snprintf(b, sizeof b, "they give: %s", gn); add_text(b); snprintf(b, sizeof b, "they want: %s", rn); add_text(b);   /* three SHORT lines: the popup does not measure wrapped text, a two-line one was covered by the first button (seen live 2026-11-08) */ add_btn("Accept offer", "tr_accept"); add_btn("Reject offer", "tr_reject"); add_btn("Counter...", "tr_counter"); }
        else { int me2 = tr_me(), oid2 = 0, to2 = 0; if (tr_own(me2, &oid2, &to2)) { char og[130], ot[130], gn[200], rn[200]; uget("offer_give", og, sizeof og); uget("offer_get", ot, sizeof ot); tr_nice(og, gn, sizeof gn); tr_nice(ot, rn, sizeof rn);   /* MY open offer: amend it or take it back (the engine only lets the proposer do either) */
            snprintf(b, sizeof b, "YOUR OFFER #%d to p%d", oid2, to2); add_text(b); snprintf(b, sizeof b, "you give: %s", gn); add_text(b); snprintf(b, sizeof b, "you want: %s", rn); add_text(b); add_btn("Amend offer...", "tr_amend"); add_btn("Cancel offer", "tr_cancel"); } }
        if (!run && !over) add_btn("Step one move", "step");
        snprintf(b, sizeof b, "speed: %s", SPEED_NAME[speed]); add_btn(b, "speed_next"); add_btn("New game", "new_game");
        if (!over && human_pid() && tune_int("camera_follow", 1)) add_btn("Find me", "find_me");   /* LAST, so no other button changes its index; only with a human seat and while the owner's tuning row camera_follow is on */
        }
    }
    if (!(tr_open_ && have)) catn(o, sizeof o, "tp_on=\ngame-ov_x=\ngame-ov_y=\n");   /* the trade screen is off: the popup is centred again */
    if (ENT_[0]) { char eb[460]; snprintf(eb, sizeof eb, "gme_0_label=%s\n", ENT_); catn(o, sizeof o, eb); ne_ = 1; }
    snprintf(b, sizeof b, "gm_nt=%d\ngm_nb=%d\ngm_ne=%d\ngm_nl=%d\n", nt_, nb_, ne_, nl_); catn(o, sizeof o, b);
    if (have && page_needed()) page_sync(0);
    char tmp[P]; snprintf(tmp, sizeof tmp, "%s.tmp", FEED); FILE *f = fopen(tmp, "w"); if (!f) return; fputs(o, f); fclose(f); rename(tmp, FEED); dirty = 0;
}

static void rm_tree(const char *d) { DIR *dr = opendir(d); if (!dr) return; struct dirent *e; while ((e = readdir(dr))) { if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue; char p[P]; snprintf(p, sizeof p, "%s/%s", d, e->d_name); struct stat s; if (!lstat(p, &s) && S_ISDIR(s.st_mode)) rm_tree(p); else unlink(p); } closedir(dr); rmdir(d); }
static int cmp_str(const void *a, const void *b) { return strcmp((const char *)a, (const char *)b); }
static void archive_game(void) {   /* keep the last 3 games in archive/, delete older ones (the cap) */
    struct stat s; char p[P]; snprintf(p, sizeof p, "%s/ledger.txt", GAME); if (stat(p, &s)) { rm_tree(GAME); return; }
    mkdir(ARCH, 0755); char to[P]; snprintf(to, sizeof to, "%s/game-%ld", ARCH, (long)time(NULL)); rename(GAME, to);
    static char names[64][48]; int n = 0; DIR *d = opendir(ARCH); if (d) { struct dirent *e; while ((e = readdir(d)) && n < 64) if (!strncmp(e->d_name, "game-", 5)) snprintf(names[n++], 48, "%s", e->d_name); closedir(d); }
    qsort(names, (size_t)n, 48, cmp_str); for (int i = 0; i < n - 3; i++) { snprintf(p, sizeof p, "%s/%s", ARCH, names[i]); rm_tree(p); }
}
static void begin_game(void) {
    char out[400]; archive_game(); mkdir(GAME, 0755); char sd[16]; snprintf(sd, sizeof sd, "%d", seed);
    if (eng1("lobby", "classic", out, sizeof out)) { first_line(out); snprintf(msg, sizeof msg, "lobby refused: %s", out); return; }
    for (int i = 1; i <= nplayers; i++) { char nm[4] = { (char)('A' + i - 1), 0 }; const char *a[3] = { "join", nm, i == human_seat ? "human" : "pet" }; if (run_eng(out, sizeof out, 3, a)) { first_line(out); snprintf(msg, sizeof msg, "join refused: %s", out); return; } }
    if (eng1("start", sd, out, sizeof out)) { first_line(out); snprintf(msg, sizeof msg, "start refused: %s", out); return; }
    snap_key[0] = 0; mode_play = 1; play_desk_fresh(); walk_reset(); snprintf(msg, sizeof msg, "game started: %d players, seed %d, %s", nplayers, seed, human_seat ? "you are a seat at the table" : "all pets");
    if (human_seat) snprintf(msg, sizeof msg, "game started: %d players, seed %d, you are p%d", nplayers, seed, human_seat);
    eng1("ui", NULL, out, sizeof out);
}
static void tick_once(void) {
    char out[600]; int rc = eng1("tick", NULL, out, sizeof out); first_line(out); eng1("ui", NULL, NULL, 0);
    w_resync = 1;
    if (rc != 0) snprintf(msg, sizeof msg, "stuck: %.150s", out); else snprintf(msg, sizeof msg, "%.190s", out);
    dirty = 1;
}
static void human_verb2(const char *engine_verb, const char *arg) {
    char out[400], as[24], cur[16], ph[16]; load_ui(); uget("current", cur, sizeof cur); uget("phase", ph, sizeof ph); snprintf(as, sizeof as, "as=%s", cur);
    if (!strcmp(ph, "order") && !strcmp(engine_verb, "roll")) engine_verb = "order";   /* the opening throw is its own verb; the Roll button covers both */
    const char *a[3] = { engine_verb, arg ? arg : as, arg ? as : NULL }; int rc = run_eng(out, sizeof out, arg ? 3 : 2, a); first_line(out); eng1("ui", NULL, NULL, 0);
    w_resync = 1;
    snprintf(msg, sizeof msg, "%s%.170s", rc ? "refused: " : "you: ", out); dirty = 1;
}
static void human_verb(const char *engine_verb) { human_verb2(engine_verb, NULL); }
static void handle(const char *c) {
    if (!strcmp(c, "players_up")) { if (!mode_play && nplayers < 6) nplayers++; if (human_seat > nplayers) human_seat = 0; }
    else if (!strcmp(c, "players_down")) { if (!mode_play && nplayers > 2) nplayers--; if (human_seat > nplayers) human_seat = 0; }
    else if (!strcmp(c, "human_next")) { if (!mode_play) human_seat = (human_seat + 1) % (nplayers + 1); }
    else if (!strcmp(c, "seed_next")) { if (!mode_play) seed = seed * 31 % 9973 + 7; }
    else if (!strcmp(c, "speed_next")) { speed = (speed + 1) % 3; }
    else if (!strcmp(c, "setup_reset")) { if (!mode_play) { nplayers = 3; human_seat = 0; seed = 7; speed = 1; snprintf(msg, sizeof msg, "Choose the players, then START"); } }   /* a defined starting point for a script: the setup choices PERSIST across New game (a convenience), so a repeatable run resets them first; preferences only, never the ledger */
    else if (!strcmp(c, "start")) { if (!mode_play) begin_game(); }
    else if (!strcmp(c, "step")) { if (mode_play && !running()) tick_once(); }
    else if (!strcmp(c, "new_game")) { if (mode_play) { archive_game(); mode_play = 0; set_desk("board"); walk_reset(); snprintf(msg, sizeof msg, "the last game is in archive/; choose the players, then START"); } }
    else if (!strcmp(c, "roll")) human_verb("roll"); else if (!strcmp(c, "buy")) human_verb("buy"); else if (!strcmp(c, "skip")) human_verb("skip"); else if (!strcmp(c, "end")) human_verb("end_turn");
    else if (!strcmp(c, "pass")) human_verb("pass"); else if (!strcmp(c, "bid_min") || !strcmp(c, "bid_up")) { load_ui(); char ab[16]; snprintf(ab, sizeof ab, "%d", uint_("auction_min") + (!strcmp(c, "bid_up") ? 50 : 0)); human_verb2("bid", ab); }   /* M7: the amount is read from the feed at CLICK time, so a stale label cannot bid a wrong number */
    else if (!strcmp(c, "jail_pay")) human_verb("jail_pay"); else if (!strcmp(c, "jail_roll")) human_verb("jail_roll"); else if (!strcmp(c, "pay_debt")) human_verb("pay_debt"); else if (!strcmp(c, "bankrupt")) human_verb("bankrupt");
    else if (!strncmp(c, "tr_", 3)) tr_cmd(c + 3);
    else if (!strcmp(c, "refresh")) { w_resync = 1; char rp[P]; snprintf(rp, sizeof rp, "%s/chat_reply.txt", GDIR); FILE *f = fopen(rp, "r");   /* the editor chat hook (deskopoly_chat --hook) wrote a verb to the ledger: republish; its one answer line is shown in the popup once (consumed) */
        if (f) { char ln[300]; if (fgets(ln, sizeof ln, f)) { first_line(ln); if (ln[0]) { snprintf(msg, sizeof msg, "chat: %.180s", ln); msg_keep = now_ms() + 6000; } } fclose(f); unlink(rp); } }
    else if (!strcmp(c, "find_me")) { if (mode_play && tune_int("camera_follow", 1)) { load_ui(); int hp = human_pid(); char why[120]; if (!hp) snprintf(msg, sizeof msg, "no human seat to find"); else { snap_for(hp, why, sizeof why); snprintf(msg, sizeof msg, "%s", why[0] ? why : "camera: here is your figure"); msg_keep = now_ms() + 4000; } } }
    else snprintf(msg, sizeof msg, "unknown command %.40s", c);
    dirty = 1;
}
static volatile sig_atomic_t stop_ = 0; static void on_term(int s) { (void)s; stop_ = 1; }

int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: deskopoly_driver <house_root> <book_app_dir> <editor_app_dir>\n"); return 2; }
    snprintf(HOUSE, sizeof HOUSE, "%s", argv[1]); snprintf(BOOK, sizeof BOOK, "%s", argv[2]); snprintf(EDIT, sizeof EDIT, "%s", argv[3]);
    snprintf(ENG, sizeof ENG, "%s/ops/+x/deskopoly.+x", BOOK); snprintf(CTL, sizeof CTL, "%s/ops/+x/deskopoly_ctl.+x", BOOK);
    snprintf(GDIR, sizeof GDIR, "%s/state/games/deskopoly", EDIT); snprintf(GAME, sizeof GAME, "%s/game", GDIR); snprintf(ARCH, sizeof ARCH, "%s/archive", GDIR);
    snprintf(FEED, sizeof FEED, "%s/state/game_ui.txt", EDIT); snprintf(ACT, sizeof ACT, "%s/action.txt", GDIR); snprintf(SW, sizeof SW, "%s/state/world/switches.txt", EDIT);
    { char eb[P]; snprintf(eb, sizeof eb, "%s", EDIT); char *bn = strrchr(eb, '/'); bn = bn ? bn + 1 : eb; snprintf(PCHQ, sizeof PCHQ, "%s/%s-pchq", EDIT, bn); snprintf(WORLD, sizeof WORLD, "%s/pieces/world_01/state.txt", PCHQ); }
    snprintf(DESKDIR, sizeof DESKDIR, "%s/deskopoly-pchq/pieces/system/maps/deskopoly/play", BOOK); snprintf(ATLAS, sizeof ATLAS, "%s/deskopoly-pchq/pieces/system/maps/deskopoly/board/cells.rgba", BOOK);
    char d[P]; snprintf(d, sizeof d, "%s/state/games", EDIT); mkdir(d, 0755); mkdir(GDIR, 0755);
    { const char *ow = getenv("GAME_OWNER_PID"); OWNER = ow ? atol(ow) : 0; }
    char lk[P]; snprintf(lk, sizeof lk, "%s/driver.lock", GDIR); int lf = open(lk, O_RDWR | O_CREAT, 0644);
    if (lf < 0) { fprintf(stderr, "deskopoly_driver: cannot open the lock file %s (does the editor's state folder exist?)\n", lk); return 1; }   /* a missing folder is NOT "already running" (found by the dsk_driver case 2026-11-08) */
    if (flock(lf, LOCK_EX | LOCK_NB)) { fprintf(stderr, "deskopoly_driver: already running\n"); return 0; }
    struct sigaction sa; memset(&sa, 0, sizeof sa); sa.sa_handler = on_term; sigaction(SIGTERM, &sa, NULL); sigaction(SIGINT, &sa, NULL); sigaction(SIGHUP, &sa, NULL);
    if (access(ENG, X_OK)) { fprintf(stderr, "deskopoly_driver: the engine is missing: %s\n", ENG); return 1; }
    { char out[200]; if (eng1("ui", NULL, out, sizeof out) == 0) { mode_play = 1; play_desk_fresh(); walk_reset(); snprintf(msg, sizeof msg, "a game was in progress; it continues while the Player menu says playing"); } }   /* the mode comes from the LEDGER */
    struct stat s; long cursor = stat(ACT, &s) ? 0 : (long)s.st_size; long next = now_ms(); publish();
    while (!stop_) {
        if (OWNER > 0 ? (kill((pid_t)OWNER, 0) != 0 && errno == ESRCH) : getppid() == 1) break;
        if (!stat(ACT, &s) && s.st_size > cursor) { FILE *f = fopen(ACT, "r"); if (f) { fseek(f, cursor, SEEK_SET); char ln[128]; while (fgets(ln, sizeof ln, f)) { first_line(ln); if (ln[0]) handle(ln); } cursor = ftell(f); fclose(f); } if (cursor > 65536 && truncate(ACT, 0) == 0) cursor = 0; }
        else if (!stat(ACT, &s) && s.st_size < cursor) cursor = 0;
        static int hwait = 0; int run = running(); static int was_run = -1; if (run != was_run) { was_run = run; dirty = 1; }
        if (mode_play) { static long nw = 0; static int was = 0; long tw = now_ms(); if (tw >= nw) { if (w_resync) { walk_sync(); write_events(); } int m = walk_step(); if (m || was) write_events(); was = walking_now; nw = tw + W_MS[speed]; } }
        if (mode_play && run && !walking_now && !w_resync) { long t = now_ms(); if (t >= next) { char ov[8], ht[8], cn[24], yt[80], pr[8]; load_ui(); uget("over", ov, sizeof ov); uget("human_turn", ht, sizeof ht); uget("current_name", cn, sizeof cn); uget("pet_reply", pr, sizeof pr);   /* pet_reply=1 only while an offer to a PET is open (M9): the pet answers even though a human is up; otherwise 0 calls */
            /* A HUMAN'S TURN COSTS NOTHING: while the game waits for a seat that is a person the driver does not poll the engine (measured 2026-11-08 in the real editor: 17 engine calls in 10 s, ledger frozen, before this; 0 after). ui.txt is rewritten after every verb and tick, so the moment the turn passes to the human the next loop sees human_turn=1. */
            if (ov[0] != '1' && ht[0] == '1' && !pr[0]) { hwait = 1; snprintf(yt, sizeof yt, "your turn, %s: use the buttons", cn); if (now_ms() >= msg_keep && strcmp(msg, yt)) { snprintf(msg, sizeof msg, "%s", yt); dirty = 1; }
                { char cu[16], key[40], why[120]; uget("current", cu, sizeof cu); snprintf(key, sizeof key, "%d:%s", uint_("round"), cu);   /* CAMERA SNAP: once per human turn (the tuning row is read only when the turn is new) */
                  if (strcmp(key, snap_key)) {
                      int on = tune_int("camera_follow", 1), rc = on ? snap_for(tr_pid_of(cu), why, sizeof why) : 0;
                      if (rc == -1 && (snap_t0 ? now_ms() - snap_t0 < 90000 : (snap_t0 = now_ms()) > 0)) { /* no board session yet: try again on the next loop (up to 90 s), do not give up this turn */ }
                      else { snprintf(snap_key, sizeof snap_key, "%s", key); snap_tries = 0; snap_t0 = 0; if (on && rc == 0 && !snap_first_done) { snap_first_done = 1; snap_rep = 2; snap_again = now_ms() + 6000; } } }
                  else if (snap_again && now_ms() >= snap_again) { snap_again = 0; if (tune_int("camera_follow", 1)) snap_for(tr_pid_of(cu), why, sizeof why); if (--snap_rep > 0) snap_again = now_ms() + 9000; } } }
            else if (ov[0] != '1') { hwait = 0; tick_once(); } else if (strcmp(msg, "the game is over")) { snprintf(msg, sizeof msg, "the game is over"); dirty = 1; } next = t + INTERVAL_MS[speed]; } }
        if (page_pending && now_ms() - last_page >= 250) page_sync(1);
        /* DESK GUARD (owner 2026-11-08 "the entities seemed to disappear from the board"): the players live on the generated PLAY desk; the editor's board launcher writes the world's desk from its own page (page=board) a little AFTER a driver that started at launch (the core watchdog) has switched it, and the players vanished with a game running. While a game is in play the driver keeps asserting its desk (a 4 KB read per 2 s, a write only when it really differs). */
        { static long dg = 0; long td = now_ms(); if (mode_play && td - dg >= 2000) { dg = td; set_desk("play"); } }
        if (dirty) publish();
        /* ADAPTIVE POLL (owner 2026-11-08 "irregular sleep cycles"): 80 ms while the game is live (walking, ticking); 250 ms on the setup screen, while stopped, and while a human is thinking: nothing can happen but a button press. Measured with the context-switch counter: 12-14 wakeups/s before. */
        usleep((mode_play && ((run && !hwait) || walking_now || w_resync || page_pending)) ? 80000 : 250000);   /* anything with a deadline (a walk, a resync, a deferred page) keeps the fast poll even while stopped */
    }
    set_desk("board");
    char tmp[P]; snprintf(tmp, sizeof tmp, "%s.tmp", FEED); { FILE *f = fopen(tmp, "w"); if (f) { fputs("gm_on=\n", f); fclose(f); rename(tmp, FEED); } }   /* the bar disappears when the game ends */
    return 0;
}
