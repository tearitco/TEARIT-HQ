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
 * Commands: players_up players_down human_next seed_next speed_next start step new_game roll buy skip end jail_pay jail_roll pay_debt bankrupt. */
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
    static const char *quiet[] = { "roll", "order", "first", "end_turn", "skip", "bid", "pass", "jail_pay", "jail_roll", "jail_card", "lobby", "join", "seat", NULL };   /* M7: a bid or a pass never changes the picture (a pets game makes ~115 bids: rebuilding the page for each was the 6% CPU burn again); the SALE does, as the non-quiet auction_end row (an unsold auction rebuilds once, harmlessly) */
    while (fgets(ln, sizeof ln, f)) { char *q = ln; for (int k = 0; k < 3 && q; k++) { q = strchr(q, '|'); if (q) q++; } if (!q) { need = 1; continue; }
        char *e = strchr(q, '|'); size_t vl = e ? (size_t)(e - q) : strcspn(q, "\r\n"); int isq = 0; for (int k = 0; quiet[k]; k++) if (strlen(quiet[k]) == vl && !strncmp(q, quiet[k], vl)) isq = 1; if (!isq) need = 1; }
    pg_cur = ftell(f); fclose(f); return need;
}
static void publish(void) {
    static char o[40000]; o[0] = 0; OUT_ = o; OUTCAP_ = sizeof o; nb_ = nt_ = ne_ = nl_ = 0; ENT_[0] = 0; char t[256], k[48], b[600];
    int run = running(); int have = mode_play && load_ui(); int over = have ? uint_("over") : 0;
    catn(o, sizeof o, "gm_on=1\ngm_title=TEROPOLY classic\n");
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
        for (int i = 7; i >= 0; i--) { snprintf(k, sizeof k, "log_%d", i); char l[120]; uget(k, l, sizeof l); if (l[0]) add_chat(l); }
        char v[8];
        if (uget("show_roll", v, sizeof v)[0]) add_btn("Roll", "roll");
        if (uget("show_jail", v, sizeof v)[0]) { add_btn("Jail: roll for doubles", "jail_roll"); add_btn("Jail: pay 50", "jail_pay"); }
        if (uget("show_buy", v, sizeof v)[0]) { char pt[40]; uget("pending_tile", pt, sizeof pt); snprintf(b, sizeof b, "Buy %s (%d)", pt, uint_("pending_price")); add_btn(b, "buy"); }
        if (uget("show_skip", v, sizeof v)[0]) add_btn("Skip", "skip");
        { char at[40]; uget("auction_tile", at, sizeof at); if (at[0]) { char ah[16], bd[16]; uget("auction_high", ah, sizeof ah); uget("auction_bidder", bd, sizeof bd);
            snprintf(b, sizeof b, "AUCTION %s %d%s%s", at, uint_("auction_bid"), ah[0] ? " by " : "", ah); (void)bd; add_text(b); } }   /* one short line: a long one wrapped to three and the first button covered its end (seen in the live window 2026-11-08) */
        if (uget("show_bid", v, sizeof v)[0]) { int mn = uint_("auction_min"); char ck[24]; snprintf(ck, sizeof ck, "%s_cash", cur); snprintf(b, sizeof b, "Bid %d", mn); add_btn(b, "bid_min"); if (uint_(ck) >= mn + 50) { snprintf(b, sizeof b, "Bid %d", mn + 50); add_btn(b, "bid_up"); } }   /* the human picks the amount: the minimum raise or 50 above it (the engine refuses anything it does not allow) */
        if (uget("show_pass", v, sizeof v)[0]) add_btn("Pass", "pass");
        if (uget("show_pay_debt", v, sizeof v)[0]) { snprintf(b, sizeof b, "Pay debt %d", uint_("debt")); add_btn(b, "pay_debt"); }
        if (uget("show_bankrupt", v, sizeof v)[0]) add_btn("Declare bankruptcy", "bankrupt");
        if (uget("show_end", v, sizeof v)[0]) add_btn("End turn", "end");
        if (!run && !over) add_btn("Step one move", "step");
        snprintf(b, sizeof b, "speed: %s", SPEED_NAME[speed]); add_btn(b, "speed_next"); add_btn("New game", "new_game");
    }
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
    mode_play = 1; play_desk_fresh(); walk_reset(); snprintf(msg, sizeof msg, "game started: %d players, seed %d, %s", nplayers, seed, human_seat ? "you are a seat at the table" : "all pets");
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
    { char eb[P]; snprintf(eb, sizeof eb, "%s", EDIT); char *bn = strrchr(eb, '/'); bn = bn ? bn + 1 : eb; snprintf(WORLD, sizeof WORLD, "%s/%s-pchq/pieces/world_01/state.txt", EDIT, bn); }
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
        if (mode_play && run && !walking_now && !w_resync) { long t = now_ms(); if (t >= next) { char ov[8], ht[8], cn[24], yt[80]; load_ui(); uget("over", ov, sizeof ov); uget("human_turn", ht, sizeof ht); uget("current_name", cn, sizeof cn);
            /* A HUMAN'S TURN COSTS NOTHING: while the game waits for a seat that is a person the driver does not poll the engine (measured 2026-11-08 in the real editor: 17 engine calls in 10 s, ledger frozen, before this; 0 after). ui.txt is rewritten after every verb and tick, so the moment the turn passes to the human the next loop sees human_turn=1. */
            if (ov[0] != '1' && ht[0] == '1') { hwait = 1; snprintf(yt, sizeof yt, "your turn, %s: use the buttons", cn); if (strcmp(msg, yt)) { snprintf(msg, sizeof msg, "%s", yt); dirty = 1; } }
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
