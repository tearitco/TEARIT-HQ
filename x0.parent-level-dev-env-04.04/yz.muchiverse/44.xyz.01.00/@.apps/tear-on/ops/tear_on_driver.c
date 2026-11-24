#define _GNU_SOURCE
/* tear_on_driver - the game side of PLAY for the tear-on book inside rpg-edit-hq (2026-11-24). Shape of @.apps/deskopoly/ops/deskopoly_driver.c.
 *   tear_on_driver <house_root> <book_app_dir> <editor_app_dir>
 * Started by the editor's Play (`GAME | driver` row of the book's game.pdl); one instance (flock). It decides NO rule: every action is ONE ROW appended to the chain through
 * &.hq-apps/tear-on/ops/+x/tear_on_op.+x do (which mines it with tear_chain_op, the C port of the web page's rules, and re-projects the ranch and arena pages the board draws).
 * It publishes the generic GAME BAR feed <editor>/state/game_ui.txt (gm_on, gm_pid, gm_title, gm_nt + gmt_<i>_text, gm_nb + gmb_<i>_label / gmb_<i>_act, gm_ne, gm_nl); each button runs
 * tear_on_ctl, which appends one command word to <book>/play/action.txt that this driver reads.  PAUSE IS NOT OURS: the editor's switch running (state/world/switches.txt) is the one authority;
 * commands are handled only while running=1.  The selected pet and the selected arena are PREFERENCES (they change what is asked, never an outcome).
 * The bar has four tabs (a PREFERENCE, tab_care tab_train tab_walk tab_arena tab_wallet) so the narrow popup shows few short lines and at most nine buttons.
 * Commands: refresh tab_care tab_train tab_walk tab_arena tab_wallet pet_next hatch faucet feed_bread feed_meat train_sprint train_lift train_study rest rank move_up move_down move_left move_right
 *           arena_open_1v1 arena_open_2v2 arena_open_ffa arena_join arena_fill arena_fight arena_leave arena_next  wo_cap wo_reserve wo_sale  close
 * Cost: idle = a stat() of the switch file and the queue per 80 ms nap; one op call per click.  Files: game_ui.txt (rewritten), play/action.txt (capped at 64 KiB, oldest half dropped), play/driver.lock.
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/tear_on_driver.+x tear_on_driver.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/wait.h>
#define P 2048
static char HOUSE[P], BOOK[P], EDIT[P], CHAIN[P], CHAINOP[P], ONOP[P], CTL[P], FEED[P], ACT[P], PLAY[P], SW[P], OWNERF[P];
static char owner[48] = "you", sel[48] = "", asel[48] = "", msg[300] = "Press a button.";
static long OWNERPID = 0; static volatile sig_atomic_t stop_ = 0;
typedef struct { char name[48], owner[48], species[16], stats[48], items[40]; int age, life, fat, x, y, frozen, retired, wins, cups, best, cap, reserve, sale; long coins, price; } Pet;
typedef struct { char id[48], mode[8], owner[48], members[240], winners[120], scores[120]; int open; long pot; } Arena;
static Pet pets[48]; static int np; static Arena ars[16]; static int nar;
static char tab[12] = "care";
static int cap_i = 0, res_i = 0; static const int CAPS[] = { 0, 10, 25, 50, 100 }, RESV[] = { 0, 50, 100, 250, 500 };
static long now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000L + t.tv_nsec / 1000000L; }
static int running(void) { FILE *f = fopen(SW, "r"); if (!f) return 0; char ln[80]; int r = 0; while (fgets(ln, sizeof ln, f)) if (!strncmp(ln, "running=", 8)) r = atoi(ln + 8); fclose(f); return r; }
static int run(char *const av[], char *out, size_t os) {
    int p[2]; if (pipe(p)) return 99; pid_t c = fork();
    if (c == 0) { alarm(20); dup2(p[1], 1); dup2(p[1], 2); close(p[0]); close(p[1]); execv(av[0], av); _exit(127); }
    close(p[1]); size_t n = 0; ssize_t r; while (n + 1 < os && (r = read(p[0], out + n, os - n - 1)) > 0) n += (size_t)r; out[n] = 0; close(p[0]);
    int st = 0; waitpid(c, &st, 0); return WIFEXITED(st) ? WEXITSTATUS(st) : 98; }
static void chain_read(void) {
    static char big[32768]; char *av[4]; int c; np = nar = 0;
    av[0] = CHAINOP; av[1] = "pets"; av[2] = CHAIN; av[3] = NULL; c = run(av, big, sizeof big);
    if (c == 0) for (char *p = big; *p && np < 48;) { char *e = strchr(p, '\n'); if (e) *e = 0; if (*p) { char *f[17]; int k = 0; f[k++] = p; for (char *q = p; *q && k < 17; q++) if (*q == '|') { *q = 0; f[k++] = q + 1; }
            if (k >= 17) { Pet *t = &pets[np++]; memset(t, 0, sizeof *t); snprintf(t->name, 48, "%s", f[0]); snprintf(t->owner, 48, "%s", f[1]); snprintf(t->species, 16, "%s", f[2]); sscanf(f[3], "%d/%d", &t->age, &t->life); t->fat = atoi(f[4]); sscanf(f[5], "%d,%d", &t->x, &t->y); snprintf(t->stats, 48, "%s", f[6]); sscanf(f[7], "%d/%d", &t->wins, &t->cups); t->best = atoi(f[8]); t->price = atol(f[9]); t->frozen = atoi(f[11]); t->retired = atoi(f[12]); t->coins = atol(f[15]); snprintf(t->items, 40, "%s", f[16]); t->sale = 1; } }
        if (!e) break; p = e + 1; }
    av[1] = "wopts"; c = run(av, big, sizeof big);
    if (c == 0) for (char *p = big; *p;) { char *e = strchr(p, '\n'); if (e) *e = 0; char n[48] = ""; int a = 0, b = 0, s = 1; if (sscanf(p, "%47[^|]|%d|%d|%d", n, &a, &b, &s) == 4) for (int i = 0; i < np; i++) if (!strcmp(pets[i].name, n)) { pets[i].cap = a; pets[i].reserve = b; pets[i].sale = s; } if (!e) break; p = e + 1; }
    av[1] = "arenas"; c = run(av, big, sizeof big);
    if (c == 0) for (char *p = big; *p && nar < 16;) { char *e = strchr(p, '\n'); if (e) *e = 0; if (*p) { char *f[9]; int k = 0; f[k++] = p; for (char *q = p; *q && k < 9; q++) if (*q == '|') { *q = 0; f[k++] = q + 1; }
            if (k >= 8) { Arena *a = &ars[nar++]; memset(a, 0, sizeof *a); snprintf(a->id, 48, "%s", f[0]); snprintf(a->mode, 8, "%s", f[1]); snprintf(a->owner, 48, "%s", f[2]); a->open = atoi(f[3]); a->pot = atol(f[4]); snprintf(a->members, 240, "%s", f[5]); snprintf(a->winners, 120, "%s", f[6]); snprintf(a->scores, 120, "%s", f[7]); } }
        if (!e) break; p = e + 1; }
    int ok = 0; for (int i = 0; i < np; i++) if (!strcmp(pets[i].name, sel) && !strcmp(pets[i].owner, owner) && !pets[i].frozen) ok = 1;
    if (!ok) { sel[0] = 0; for (int i = 0; i < np; i++) if (!strcmp(pets[i].owner, owner) && !pets[i].frozen) { snprintf(sel, sizeof sel, "%s", pets[i].name); break; } }
    int fa = 0, lo = -1, la = -1; for (int i = 0; i < nar; i++) { if (!strcmp(ars[i].id, asel)) fa = 1; if (ars[i].members[0]) la = i; if (ars[i].open && ars[i].members[0]) lo = i; }
    if (!fa) { int pick = lo >= 0 ? lo : la >= 0 ? la : nar - 1; if (pick >= 0) snprintf(asel, sizeof asel, "%s", ars[pick].id); else asel[0] = 0; } }
static Pet *selpet(void) { for (int i = 0; i < np; i++) if (!strcmp(pets[i].name, sel)) return &pets[i]; return NULL; }
static Arena *selar(void) { for (int i = 0; i < nar; i++) if (!strcmp(ars[i].id, asel)) return &ars[i]; return NULL; }
static char OUT_[16384]; static int nt_, nb_;
static void catn(const char *s) { strncat(OUT_, s, sizeof OUT_ - strlen(OUT_) - 1); }
static void add_text(const char *s) { char b[500]; snprintf(b, sizeof b, "gmt_%d_text=%s\n", nt_++, s[0] ? s : " "); catn(b); }
static void add_btn(const char *label, const char *cmd) { char b[P + 300]; snprintf(b, sizeof b, "gmb_%d_label=%s\ngmb_%d_act='%s' %s '%s'\n", nb_, label, nb_, CTL, cmd, PLAY); nb_++; catn(b); }
static void publish(void) {
    OUT_[0] = 0; nt_ = nb_ = 0; char b[600]; Pet *p = selpet(); Arena *a = selar(); static const char *RK[] = { "E", "D", "C", "B", "A", "S" };
    catn("gm_on=1\n"); snprintf(b, sizeof b, "gm_pid=%ld\ngm_title=TEAR-IT Ranch  -  %s\n", (long)getpid(), owner); catn(b);
    /* SIDE BARS FOR THIS BOOK (owner rule 2026-11-24, BOOK 16.game open-a-project doc section 11): while Play runs the left bar is the PALS bar, not the palettes: side_pal empty hides the tile palettes, side_left=1 shows the pets bar, its party is this owner's pets, its lines the selected pet. The editor's own values return when the feed drops (gm_on=). A PREFERENCE of the view, never game state. */
    catn("side_pal=\nside_left=1\n");
    { int k = 0; char pb[400]; for (int i = 0; i < np; i++) { if (strcmp(pets[i].owner, owner) || pets[i].frozen) continue; int on = !strcmp(pets[i].name, sel); snprintf(pb, sizeof pb, "party_%d_label=%s%s  %ldc\nparty_%d_id=tear_%s\nparty_%d_cls=%s\n", k, on ? "> " : "", pets[i].name, pets[i].coins, k, pets[i].name, k, on ? "active" : ""); catn(pb); k++; } snprintf(pb, sizeof pb, "n_party=%d\n", k); catn(pb); }
    if (p) { snprintf(b, sizeof b, "coins=%ld\nnick_line=%s the %s\nstat_line=age %d/%d tired %d\nstat_line2=cap %d res %d %s\n", p->coins, p->name, p->species, p->age, p->life, p->fat, p->cap, p->reserve, p->sale ? "open" : "locked"); catn(b); }
    else catn("coins=0\nnick_line=no pet yet\nstat_line=Hatch a pet on the Care tab\nstat_line2=\n");
    if (!strcmp(tab, "arena")) {
        if (a) { snprintf(b, sizeof b, "%s  %s  pot %ld", a->id, a->mode, a->pot); add_text(b); snprintf(b, sizeof b, "%s", a->open ? "open" : "finished"); add_text(b); snprintf(b, sizeof b, "fighters: %s", a->members[0] ? a->members : "(none)"); add_text(b); if (a->winners[0]) { snprintf(b, sizeof b, "winners: %s", a->winners); add_text(b); snprintf(b, sizeof b, "scores: %s", a->scores); add_text(b); } }
        else add_text("no arena yet: open one");
    } else if (p) {
        snprintf(b, sizeof b, "%s the %s", p->name, p->species); add_text(b); snprintf(b, sizeof b, "age %d/%d   tired %d", p->age, p->life, p->fat); add_text(b); if (!strcmp(tab, "train") || !strcmp(tab, "walk")) snprintf(b, sizeof b, "%ld coins  cell %d,%d", p->coins, p->x, p->y); else snprintf(b, sizeof b, "%ld coins", p->coins); add_text(b);
        if (!strcmp(tab, "train")) { snprintf(b, sizeof b, "LIF %.*s", 0, ""); int v[6] = { 0 }; sscanf(p->stats, "%d %d %d %d %d %d", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]); snprintf(b, sizeof b, "LIF %d POW %d INT %d", v[0], v[1], v[2]); add_text(b); snprintf(b, sizeof b, "SKI %d SPD %d DEF %d", v[3], v[4], v[5]); add_text(b); snprintf(b, sizeof b, "wins %d of %d  rank %s", p->wins, p->cups, p->best >= 0 ? RK[p->best] : "none"); add_text(b); }
        if (!strcmp(tab, "wallet")) { snprintf(b, sizeof b, "cap %d (0 = none)", p->cap); add_text(b); snprintf(b, sizeof b, "reserve %d", p->reserve); add_text(b); snprintf(b, sizeof b, "sale %s", p->sale ? "open" : "LOCKED"); add_text(b); }
    } else add_text("no pet yet: Hatch a pet");
    add_text(msg);
    add_btn("[Care]", "tab_care"); add_btn("[Train]", "tab_train"); add_btn("[Walk]", "tab_walk"); add_btn("[Arena]", "tab_arena"); add_btn("[Wallet]", "tab_wallet");
    if (!strcmp(tab, "care")) { add_btn("Hatch a pet", "hatch"); add_btn("Next pet", "pet_next"); add_btn("Pet faucet +1000", "faucet"); add_btn("Feed bread (5)", "feed_bread"); add_btn("Feed meat (20)", "feed_meat"); add_btn("Rest (free)", "rest"); add_btn("Redraw board", "refresh"); }
    else if (!strcmp(tab, "train")) { add_btn("Train sprint", "train_sprint"); add_btn("Train lift", "train_lift"); add_btn("Train study", "train_study"); add_btn("Next ranked fight", "rank"); }
    else if (!strcmp(tab, "walk")) { add_btn("Walk up", "move_up"); add_btn("Walk down", "move_down"); add_btn("Walk left", "move_left"); add_btn("Walk right", "move_right"); }
    else if (!strcmp(tab, "arena")) { add_btn("Open a 1v1", "arena_open_1v1"); add_btn("Open a 2v2", "arena_open_2v2"); add_btn("Open free for all", "arena_open_ffa"); add_btn("Join with my pet", "arena_join"); add_btn("Fill with defaults", "arena_fill"); add_btn("FIGHT!", "arena_fight"); add_btn("Leave (fee back)", "arena_leave"); add_btn("Next arena", "arena_next"); }
    else { add_btn("Wallet cap: next", "wo_cap"); add_btn("Wallet reserve: next", "wo_reserve"); add_btn("Sale lock: toggle", "wo_sale"); add_btn("Next pet", "pet_next"); }
    add_btn("Close the game", "close");
    snprintf(b, sizeof b, "gm_nt=%d\ngm_nb=%d\ngm_ne=0\ngm_nl=0\n", nt_, nb_); catn(b);
    char tmp[P + 8]; snprintf(tmp, sizeof tmp, "%s.tmp", FEED); FILE *f = fopen(tmp, "w"); if (f) { fputs(OUT_, f); fclose(f); rename(tmp, FEED); } }
/* BOARD REFRESH (seen live 2026-11-24). The rpg-edit board draws a page when a DIFFERENT page is selected: five walks gave a zero image difference, a same-page CONFIRM_SET_DESK did nothing, arena then ranch loaded the new cells at once (each load restarts the board engine, several seconds).
 * So: the bar text is instant; an arena_* command shows the arena page, hatch and walking show the ranch page (a real switch when the editor is on the other page, nothing when it is already there);
 * the button Refresh board does the double switch on request. Only while the editor is on book tear-on; the wait for each switch is TEAR_ON_REFRESH_WAIT seconds (default 12). */
static void set_msg(const char *s);
static int world_desk(char *map, size_t ms, char *desk, size_t ds) { char wp[P], l[200]; snprintf(wp, sizeof wp, "%s/rpg-edit-hq-pchq/pieces/world_01/state.txt", EDIT); map[0] = desk[0] = 0; FILE *f = fopen(wp, "r"); if (!f) return 0; while (fgets(l, sizeof l, f)) { if (!strncmp(l, "map_id=", 7)) { snprintf(map, ms, "%s", l + 7); map[strcspn(map, "\r\n")] = 0; } else if (!strncmp(l, "desk_id=", 8)) { snprintf(desk, ds, "%s", l + 8); desk[strcspn(desk, "\r\n")] = 0; } } fclose(f); return 1; }
static void send_desk(const char *d) { char ip[P]; snprintf(ip, sizeof ip, "%s/rpg-edit-hq-pchq/pieces/system/widget_cmds/inbox.txt", EDIT); FILE *o = fopen(ip, "a"); if (o) { fprintf(o, "CONFIRM_SET_DESK:%s\n", d); fclose(o); } }
static void wait_desk(const char *want, int secs) { char m[48], d[48]; for (int i = 0; i < secs * 2; i++) { if (world_desk(m, sizeof m, d, sizeof d) && !strcmp(d, want)) return; usleep(500000); } }
/* LIVE MOTION (fixed 2026-11-24). The engine reloads a page when the SIZE of its cells.marker changes (tear_chain_op desk now grows it after every projection) and only draws when a frame is requested; the nudge key wakes it. So a row moves the pet on screen with NO page switch. */
static void nudge_board(void) {
    char ui[P], l[P + 16], sess[P] = "", bk[P]; snprintf(ui, sizeof ui, "%s/board/state/ui.txt", EDIT); FILE *f = fopen(ui, "r"); if (!f) return;
    while (fgets(l, sizeof l, f)) if (!strncmp(l, "bv_session=", 11)) { snprintf(sess, sizeof sess, "%s", l + 11); sess[strcspn(sess, "\r\n")] = 0; } fclose(f);
    if (!sess[0]) return; snprintf(bk, sizeof bk, "%s/&.widgits/board-viewer/tools/+x/bv_keys.+x", HOUSE); char out[200]; char *av[] = { bk, sess, "~~", NULL }; run(av, out, sizeof out); }
static void reload_board(const char *c) {
    char map[48], desk[48]; if (!world_desk(map, sizeof map, desk, sizeof desk) || strcmp(map, "tear-on")) return;
    const char *target = !strncmp(c, "arena_", 6) ? "arena" : (!strcmp(c, "hatch") || !strncmp(c, "move_", 5)) ? "ranch" : NULL; if (!target || !strcmp(target, desk)) return; send_desk(target); }
static void refresh_board(void) {
    char map[48], desk[48]; if (!world_desk(map, sizeof map, desk, sizeof desk) || strcmp(map, "tear-on")) { set_msg("open the tear-on book first"); return; }
    const char *back = !strcmp(desk, "arena") ? "arena" : "ranch", *other = !strcmp(back, "arena") ? "ranch" : "arena"; const char *w = getenv("TEAR_ON_REFRESH_WAIT"); int secs = w ? atoi(w) : 12; if (secs < 1) secs = 1; if (secs > 30) secs = 30;
    (void)other; (void)back; (void)secs; nudge_board(); set_msg("board redrawn from the chain"); }
static void set_msg(const char *s) { snprintf(msg, sizeof msg, "%s", s); for (char *c = msg; *c; c++) if (*c == '\n' || *c == '=') *c = ' '; }
/* append ONE row to the chain (mined + pages re-projected by tear_on_op do); the refusal text of the rules becomes the message */
/* THE PAGE IS THE PATH (EVENT-VERB-PAGES-DESIGN, D92): a button plays the verb page @.apps/tear-on/events/<verb>/ (written by verbgen from verbs.pdl) through events-hq play_event.sh; the page runs tear_on_verb, which mines ONE row and writes play/result.txt (ok or the refusal). If the page did not run (package deleted, play_event failed, timeout) the driver mines the row directly and says so on the bar, never silently. A page run is a sh + bash + prisc launch: fine for a click. */
static int do_row(const char *row, const char *okmsg) {
    char verb[24], pd[P], pl[P], rp[P], wd[P], args[600], out[600]; const char *bar = strchr(row, '|'); if (!bar || bar - row >= (int)sizeof verb) { set_msg("bad row"); return 2; }
    snprintf(verb, sizeof verb, "%.*s", (int)(bar - row), row); for (char *q = verb; *q; q++) if (*q >= 'A' && *q <= 'Z') *q = (char)(*q + 32); snprintf(args, sizeof args, "%s", bar + 1);
    snprintf(pd, sizeof pd, "%s/events/%s", BOOK, verb); snprintf(pl, sizeof pl, "%s/&.widgits/events-hq/ops/play_event.sh", HOUSE); snprintf(rp, sizeof rp, "%s/play/result.txt", BOOK); snprintf(wd, sizeof wd, "%s/play/world", BOOK);
    /* a Blueprintz debugger pause (paused=1 in #.desktop/blueprintz_debug.txt, the owner's switch) would only QUEUE the page: skip it, mine directly, say so; never touch that file or its queue */
    int held = 0; { const char *ov = getenv("BLUEPRINTZ_DEBUG_FILE"); char bp[P], bl[64]; snprintf(bp, sizeof bp, "%s", ov ? ov : ""); if (!ov) snprintf(bp, sizeof bp, "%s/#.desktop/blueprintz_debug.txt", HOUSE); FILE *bf = fopen(bp, "r"); if (bf) { while (fgets(bl, sizeof bl, bf)) if (!strncmp(bl, "paused=1", 8)) held = 1; fclose(bf); } }
    struct stat ps; if (!held && !stat(pd, &ps) && !access(pl, R_OK)) { mkdir(wd, 0755); unlink(rp); setenv("GAME_ARG", args, 1); setenv("MUCHI_TARGET_ENT", wd, 1); char *av[] = { "/bin/bash", pl, pd, HOUSE, "ui", NULL }; run(av, out, sizeof out); unsetenv("GAME_ARG"); unsetenv("MUCHI_TARGET_ENT");
        char rb[300] = ""; FILE *rf = fopen(rp, "r"); if (rf) { if (fgets(rb, sizeof rb, rf)) rb[strcspn(rb, "\r\n")] = 0; fclose(rf); }
        if (rb[0]) { if (!strcmp(rb, "ok")) { set_msg(okmsg); return 0; } set_msg(rb); return 1; } }
    char o2[600]; char *av2[] = { ONOP, "do", HOUSE, (char *)row, NULL }; int c = run(av2, o2, sizeof o2); char *e = strchr(o2, '\n'); if (e) *e = 0;
    if (c == 0) { char m2[320]; snprintf(m2, sizeof m2, "%s (direct: the %s page did not run%s)", okmsg, verb, held ? ", events are paused in Blueprintz" : ""); set_msg(m2); return 0; } set_msg(o2[0] ? o2 : "refused"); return c; }
static void do_cmd(const char *c) {
    char row[400], ts[24], m[200]; snprintf(ts, sizeof ts, "%ld", (long)time(NULL)); Pet *p = selpet(); Arena *a = selar(); chain_read();
    if (!strcmp(c, "close")) { FILE *sf = fopen(SW, "w"); if (sf) { fputs("running=0\nint=1\n", sf); fclose(sf); } stop_ = 1; return; }
    if (!strcmp(c, "refresh")) { refresh_board(); return; }
    if (!strncmp(c, "tab_", 4)) { if (!strcmp(c + 4, "care") || !strcmp(c + 4, "train") || !strcmp(c + 4, "walk") || !strcmp(c + 4, "arena") || !strcmp(c + 4, "wallet")) { snprintf(tab, sizeof tab, "%s", c + 4); set_msg(""); } else set_msg("unknown tab"); return; }
    if (!strcmp(c, "pet_next")) { int cur = -1, nx = -1; for (int i = 0; i < np; i++) if (!strcmp(pets[i].name, sel)) cur = i; for (int k = 1; k <= np; k++) { int i = (cur + k + np) % np; if (!strcmp(pets[i].owner, owner) && !pets[i].frozen) { nx = i; break; } } if (nx >= 0) snprintf(sel, sizeof sel, "%s", pets[nx].name); set_msg("next pet"); return; }
    if (!strcmp(c, "arena_next")) { int cur = -1; for (int i = 0; i < nar; i++) if (!strcmp(ars[i].id, asel)) cur = i; if (nar > 0) snprintf(asel, sizeof asel, "%s", ars[(cur + 1) % nar].id); set_msg("next arena"); return; }
    if (!strcmp(c, "hatch")) { static const char *W[] = { "wild moss", "river mist", "ember sky", "frost bell", "star dust", "iron leaf", "moon tide", "sun grain" }; int n = 1; char nm[48]; for (;;) { snprintf(nm, sizeof nm, "mochi%d", n); int used = 0; for (int i = 0; i < np; i++) if (!strcmp(pets[i].name, nm)) used = 1; if (!used) break; n++; }
        snprintf(row, sizeof row, "PET|%s|%s|%s|%s", nm, W[n % 8], owner, ts); if (!do_row(row, "hatched")) snprintf(sel, sizeof sel, "%s", nm); return; }
    if (!strncmp(c, "arena_open_", 11)) { char id[48]; int n = 1; for (;;) { snprintf(id, sizeof id, "arena%d", n); int used = 0; for (int i = 0; i < nar; i++) if (!strcmp(ars[i].id, id)) used = 1; if (!used) break; n++; }
        snprintf(row, sizeof row, "ARENA|%s|%s|%s|%s", id, c + 11, owner, ts); if (!do_row(row, "arena opened")) snprintf(asel, sizeof asel, "%s", id); return; }
    if (!strncmp(c, "arena_", 6)) { if (!a) { set_msg("open an arena first"); return; }
        if (!strcmp(c, "arena_fill")) { snprintf(row, sizeof row, "FILL|%s|%s|%s", a->id, owner, ts); do_row(row, "default fighters stepped in"); return; }
        if (!strcmp(c, "arena_fight")) { snprintf(row, sizeof row, "FIGHT|%s|%s|%s", a->id, owner, ts); do_row(row, "the fight is over: see the winners above"); return; }
        if (!p) { set_msg("hatch a pet first"); return; }
        if (!strcmp(c, "arena_join")) { snprintf(row, sizeof row, "JOIN|%s|%s|%s|%s", a->id, p->name, owner, ts); do_row(row, "joined (entry fee 20 paid)"); return; }
        if (!strcmp(c, "arena_leave")) { snprintf(row, sizeof row, "LEAVE|%s|%s|%s|%s", a->id, p->name, owner, ts); do_row(row, "left, the entry fee came back"); return; } set_msg("unknown command"); return; }
    if (!p) { set_msg("hatch a pet first"); return; }
    if (!strcmp(c, "faucet")) { snprintf(row, sizeof row, "FAUCET|%s|1000|%s", p->name, ts); do_row(row, "the pet claimed 1000 from the faucet"); return; }
    if (!strcmp(c, "rest")) { snprintf(row, sizeof row, "REST|%s|%s|%s", p->name, owner, ts); do_row(row, "rested"); return; }
    if (!strncmp(c, "feed_", 5) || !strncmp(c, "train_", 6)) { int feed = c[0] == 'f'; snprintf(row, sizeof row, "%s|%s|%s|%s|%s", feed ? "FEED" : "TRAIN", p->name, c + (feed ? 5 : 6), owner, ts); snprintf(m, sizeof m, "%s %s", feed ? "fed" : "trained", c + (feed ? 5 : 6)); do_row(row, m); return; }
    if (!strcmp(c, "rank")) { static const char *RK[] = { "E", "D", "C", "B", "A", "S" }; int nb = p->best + 1; if (nb > 5) nb = 5; long w0 = p->wins; snprintf(row, sizeof row, "COMPETE|%s|%s|%s|%s", p->name, RK[nb], owner, ts); snprintf(m, sizeof m, "ranked fight, rank %s: done", RK[nb]);
        if (!do_row(row, m)) { chain_read(); Pet *q = selpet(); if (q) set_msg(q->wins > w0 ? "won the ranked fight" : "lost the ranked fight: train and try again"); } return; }
    if (!strncmp(c, "move_", 5)) { int dx = !strcmp(c, "move_left") ? -1 : !strcmp(c, "move_right") ? 1 : 0, dy = !strcmp(c, "move_up") ? -1 : !strcmp(c, "move_down") ? 1 : 0; snprintf(row, sizeof row, "MOVE|%s|%d|%d|%s|%s", p->name, p->x + dx, p->y + dy, owner, ts); do_row(row, "walked one cell"); return; }
    if (!strcmp(c, "wo_cap") || !strcmp(c, "wo_reserve") || !strcmp(c, "wo_sale")) { int v;
        if (!strcmp(c, "wo_cap")) { cap_i = 0; for (int i = 0; i < 5; i++) if (CAPS[i] == p->cap) cap_i = i; v = CAPS[(cap_i + 1) % 5]; snprintf(row, sizeof row, "WOPT|%s|cap|%d|%s|%s", p->name, v, owner, ts); snprintf(m, sizeof m, "wallet cap is now %d (0 = no cap)", v); }
        else if (!strcmp(c, "wo_reserve")) { res_i = 0; for (int i = 0; i < 5; i++) if (RESV[i] == p->reserve) res_i = i; v = RESV[(res_i + 1) % 5]; snprintf(row, sizeof row, "WOPT|%s|reserve|%d|%s|%s", p->name, v, owner, ts); snprintf(m, sizeof m, "wallet reserve is now %d", v); }
        else { v = p->sale ? 0 : 1; snprintf(row, sizeof row, "WOPT|%s|sale|%d|%s|%s", p->name, v, owner, ts); snprintf(m, sizeof m, v ? "the pet can be sold again" : "the pet is locked from sale"); }
        do_row(row, m); return; }
    set_msg("unknown command"); }
static void bye(int s) { (void)s; stop_ = 1; }
int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: tear_on_driver <house_root> <book_app_dir> <editor_app_dir>\n"); return 2; }
    snprintf(HOUSE, sizeof HOUSE, "%s", argv[1]); snprintf(BOOK, sizeof BOOK, "%s", argv[2]); snprintf(EDIT, sizeof EDIT, "%s", argv[3]);
    snprintf(CHAIN, sizeof CHAIN, "%s/ledger/web_chain.txt", BOOK); snprintf(CHAINOP, sizeof CHAINOP, "%s/&.hq-apps/tear-offline/ops/+x/tear_chain_op.+x", HOUSE); snprintf(ONOP, sizeof ONOP, "%s/&.hq-apps/tear-on/ops/+x/tear_on_op.+x", HOUSE);
    snprintf(CTL, sizeof CTL, "%s/ops/+x/tear_on_ctl.+x", BOOK); snprintf(PLAY, sizeof PLAY, "%s/play", BOOK); snprintf(FEED, sizeof FEED, "%s/state/game_ui.txt", EDIT); snprintf(ACT, sizeof ACT, "%s/play/action.txt", BOOK); snprintf(SW, sizeof SW, "%s/state/world/switches.txt", EDIT); snprintf(OWNERF, sizeof OWNERF, "%s/play/owner.txt", BOOK);
    mkdir(PLAY, 0755); { FILE *f = fopen(OWNERF, "r"); if (f) { char l[48]; if (fgets(l, sizeof l, f)) { l[strcspn(l, "\r\n")] = 0; int ok = l[0] != 0; for (char *c = l; *c; c++) if (!((*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || *c == '_' || *c == '-')) ok = 0; if (ok) snprintf(owner, sizeof owner, "%s", l); } fclose(f); } }
    char lp[P + 16]; snprintf(lp, sizeof lp, "%s/driver.lock", PLAY); int lf = open(lp, O_WRONLY | O_CREAT, 0644); if (lf < 0 || flock(lf, LOCK_EX | LOCK_NB)) { fprintf(stderr, "tear_on_driver: already running\n"); return 0; }
    { const char *ow = getenv("GAME_OWNER_PID"); OWNERPID = ow ? atol(ow) : 0; } signal(SIGTERM, bye); signal(SIGINT, bye);
    { FILE *f = fopen(ACT, "a"); if (f) fclose(f); } struct stat st; long cur = stat(ACT, &st) ? 0 : (long)st.st_size; long chain_sz = -1; chain_read(); publish(); long last = now_ms();
    while (!stop_) {
        usleep(80000); if (getppid() == 1 && !OWNERPID) { /* parent gone: fine for a detached start, keep going unless an owner pid was given */ } if (OWNERPID > 0 && kill((pid_t)OWNERPID, 0) != 0) break;
        if (!stat(ACT, &st) && (long)st.st_size > 65536) { /* drop the oldest half, keep whole lines */ FILE *f = fopen(ACT, "r"); if (f) { fseek(f, st.st_size / 2, SEEK_SET); char junk[200]; if (fgets(junk, sizeof junk, f)) {} char *keep = malloc((size_t)st.st_size); size_t n = fread(keep, 1, (size_t)st.st_size, f); fclose(f); FILE *o = fopen(ACT, "w"); if (o) { fwrite(keep, 1, n, o); fclose(o); } free(keep); cur = 0; } }
        int changed = 0; if (running() && !stat(ACT, &st) && (long)st.st_size > cur) { FILE *f = fopen(ACT, "r"); if (f) { fseek(f, cur, SEEK_SET); char ln[100]; while (fgets(ln, sizeof ln, f) && !stop_) { cur += (long)strlen(ln); ln[strcspn(ln, "\r\n")] = 0; if (ln[0]) { do_cmd(ln); if (strncmp(ln, "tab_", 4) && strcmp(ln, "pet_next") && strcmp(ln, "arena_next") && strcmp(ln, "close") && strcmp(ln, "refresh")) { reload_board(ln); nudge_board(); } changed = 1; } } fclose(f); } }
        struct stat cs; long csz = stat(CHAIN, &cs) ? -1 : (long)cs.st_size; if (csz != chain_sz) { chain_sz = csz; changed = 1; }
        if (changed || now_ms() - last > 5000) { chain_read(); publish(); last = now_ms(); } }
    char tmp[P + 8]; snprintf(tmp, sizeof tmp, "%s.tmp", FEED); { FILE *f = fopen(tmp, "w"); if (f) { fputs("gm_on=\n", f); fclose(f); rename(tmp, FEED); } } return 0; }
