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

static char UI[20000];
static int load_ui(void) { char p[P]; snprintf(p, sizeof p, "%s/ui.txt", GAME); FILE *f = fopen(p, "r"); if (!f) { UI[0] = 0; return 0; } size_t n = fread(UI, 1, sizeof UI - 1, f); UI[n] = 0; fclose(f); return 1; }
static const char *uget(const char *k, char *dst, size_t cap) {
    size_t kl = strlen(k); dst[0] = 0; for (char *p = UI; *p; ) { char *e = strchr(p, '\n'); size_t ll = e ? (size_t)(e - p) : strlen(p);
        if (ll > kl && !strncmp(p, k, kl) && p[kl] == '=') { size_t vl = ll - kl - 1; if (vl >= cap) vl = cap - 1; memcpy(dst, p + kl + 1, vl); dst[vl] = 0; return dst; } if (!e) break; p = e + 1; }
    return dst;
}
static int uint_(const char *k) { char t[64]; return atoi(uget(k, t, sizeof t)); }
static void ring_xy(int idx, int n, int *bx, int *by) { int s = n - 1; if (idx <= s) { *bx = idx; *by = 0; } else if (idx <= 2 * s) { *bx = s; *by = idx - s; } else if (idx <= 3 * s) { *bx = s - (idx - 2 * s); *by = s; } else { *bx = 0; *by = s - (idx - 3 * s); } }

/* the feed: text rows + the buttons that are legal NOW. Every button's action string is complete (the editor's repeat just substitutes it). */
static int nb_, nt_; static char *OUT_; static size_t OUTCAP_;
static void add_text(const char *s) { char b[400]; snprintf(b, sizeof b, "gmt_%d_text=%s\n", nt_++, s[0] ? s : " "); catn(OUT_, OUTCAP_, b); }
static void add_btn(const char *label, const char *cmd) { char b[P + 300]; snprintf(b, sizeof b, "gmb_%d_label=%s\ngmb_%d_act='%s' %s '%s'\n", nb_, label, nb_, CTL, cmd, GDIR); nb_++; catn(OUT_, OUTCAP_, b); }

static void publish(void) {
    static char o[40000]; o[0] = 0; OUT_ = o; OUTCAP_ = sizeof o; nb_ = nt_ = 0; char t[256], k[48], b[600];
    int run = running(); int have = mode_play && load_ui(); int over = have ? uint_("over") : 0;
    catn(o, sizeof o, "gm_on=1\ngm_title=DESKOPOLY classic\n");
    if (have) catn(o, sizeof o, UI);
    snprintf(b, sizeof b, "%.30s", msg); add_text(b);
    if (!have) {
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
            snprintf(b, sizeof b, "%s p%d %.3s $%s%s%s", tu[0] ? ">" : " ", i, nm, ne, jl[0] ? " JAIL" : "", ou[0] == '1' ? " OUT" : ""); (void)dv; (void)ca; (void)po; add_text(b); }
        for (int i = 0; i < 3; i++) { snprintf(k, sizeof k, "log_%d", i); char l[120]; uget(k, l, sizeof l); if (l[0]) { l[30] = 0; add_text(l); } }
        char v[8];
        if (uget("show_roll", v, sizeof v)[0]) add_btn("Roll", "roll");
        if (uget("show_jail", v, sizeof v)[0]) { add_btn("Jail: roll for doubles", "jail_roll"); add_btn("Jail: pay 50", "jail_pay"); }
        if (uget("show_buy", v, sizeof v)[0]) { char pt[40]; uget("pending_tile", pt, sizeof pt); snprintf(b, sizeof b, "Buy %s (%d)", pt, uint_("pending_price")); add_btn(b, "buy"); }
        if (uget("show_skip", v, sizeof v)[0]) add_btn("Skip", "skip");
        if (uget("show_pay_debt", v, sizeof v)[0]) { snprintf(b, sizeof b, "Pay debt %d", uint_("debt")); add_btn(b, "pay_debt"); }
        if (uget("show_bankrupt", v, sizeof v)[0]) add_btn("Declare bankruptcy", "bankrupt");
        if (uget("show_end", v, sizeof v)[0]) add_btn("End turn", "end");
        if (!run && !over) add_btn("Step one move", "step");
        snprintf(b, sizeof b, "speed: %s", SPEED_NAME[speed]); add_btn(b, "speed_next"); add_btn("New game", "new_game");
    }
    snprintf(b, sizeof b, "gm_nt=%d\ngm_nb=%d\n", nt_, nb_); catn(o, sizeof o, b);
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
    mode_play = 1; snprintf(msg, sizeof msg, "game started: %d players, seed %d, %s", nplayers, seed, human_seat ? "you are a seat at the table" : "all pets");
    if (human_seat) snprintf(msg, sizeof msg, "game started: %d players, seed %d, you are p%d", nplayers, seed, human_seat);
    eng1("ui", NULL, out, sizeof out);
}
static void tick_once(void) {
    char out[600]; int rc = eng1("tick", NULL, out, sizeof out); first_line(out); eng1("ui", NULL, NULL, 0);
    if (rc != 0) snprintf(msg, sizeof msg, "stuck: %.150s", out); else snprintf(msg, sizeof msg, "%.190s", out);
    dirty = 1;
}
static void human_verb(const char *engine_verb) {
    char out[400], as[24], cur[16], ph[16]; load_ui(); uget("current", cur, sizeof cur); uget("phase", ph, sizeof ph); snprintf(as, sizeof as, "as=%s", cur);
    if (!strcmp(ph, "order") && !strcmp(engine_verb, "roll")) engine_verb = "order";   /* the opening throw is its own verb; the Roll button covers both */
    const char *a[2] = { engine_verb, as }; int rc = run_eng(out, sizeof out, 2, a); first_line(out); eng1("ui", NULL, NULL, 0);
    snprintf(msg, sizeof msg, "%s%.170s", rc ? "refused: " : "you: ", out); dirty = 1;
}
static void handle(const char *c) {
    if (!strcmp(c, "players_up")) { if (!mode_play && nplayers < 6) nplayers++; if (human_seat > nplayers) human_seat = 0; }
    else if (!strcmp(c, "players_down")) { if (!mode_play && nplayers > 2) nplayers--; if (human_seat > nplayers) human_seat = 0; }
    else if (!strcmp(c, "human_next")) { if (!mode_play) human_seat = (human_seat + 1) % (nplayers + 1); }
    else if (!strcmp(c, "seed_next")) { if (!mode_play) seed = seed * 31 % 9973 + 7; }
    else if (!strcmp(c, "speed_next")) { speed = (speed + 1) % 3; }
    else if (!strcmp(c, "start")) { if (!mode_play) begin_game(); }
    else if (!strcmp(c, "step")) { if (mode_play && !running()) tick_once(); }
    else if (!strcmp(c, "new_game")) { if (mode_play) { archive_game(); mode_play = 0; snprintf(msg, sizeof msg, "the last game is in archive/; choose the players, then START"); } }
    else if (!strcmp(c, "roll")) human_verb("roll"); else if (!strcmp(c, "buy")) human_verb("buy"); else if (!strcmp(c, "skip")) human_verb("skip"); else if (!strcmp(c, "end")) human_verb("end_turn");
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
    char d[P]; snprintf(d, sizeof d, "%s/state/games", EDIT); mkdir(d, 0755); mkdir(GDIR, 0755);
    { const char *ow = getenv("GAME_OWNER_PID"); OWNER = ow ? atol(ow) : 0; }
    char lk[P]; snprintf(lk, sizeof lk, "%s/driver.lock", GDIR); int lf = open(lk, O_RDWR | O_CREAT, 0644); if (lf < 0 || flock(lf, LOCK_EX | LOCK_NB)) { fprintf(stderr, "deskopoly_driver: already running\n"); return 0; }
    struct sigaction sa; memset(&sa, 0, sizeof sa); sa.sa_handler = on_term; sigaction(SIGTERM, &sa, NULL); sigaction(SIGINT, &sa, NULL); sigaction(SIGHUP, &sa, NULL);
    if (access(ENG, X_OK)) { fprintf(stderr, "deskopoly_driver: the engine is missing: %s\n", ENG); return 1; }
    { char out[200]; if (eng1("ui", NULL, out, sizeof out) == 0) { mode_play = 1; snprintf(msg, sizeof msg, "a game was in progress; it continues while the Player menu says playing"); } }   /* the mode comes from the LEDGER */
    struct stat s; long cursor = stat(ACT, &s) ? 0 : (long)s.st_size; long next = now_ms(); publish();
    while (!stop_) {
        if (OWNER > 0 ? (kill((pid_t)OWNER, 0) != 0 && errno == ESRCH) : getppid() == 1) break;
        if (!stat(ACT, &s) && s.st_size > cursor) { FILE *f = fopen(ACT, "r"); if (f) { fseek(f, cursor, SEEK_SET); char ln[128]; while (fgets(ln, sizeof ln, f)) { first_line(ln); if (ln[0]) handle(ln); } cursor = ftell(f); fclose(f); } if (cursor > 65536 && truncate(ACT, 0) == 0) cursor = 0; }
        else if (!stat(ACT, &s) && s.st_size < cursor) cursor = 0;
        int run = running(); static int was_run = -1; if (run != was_run) { was_run = run; dirty = 1; }
        if (mode_play && run) { long t = now_ms(); if (t >= next) { char ov[8]; load_ui(); uget("over", ov, sizeof ov); if (ov[0] != '1') tick_once(); else if (strcmp(msg, "the game is over")) { snprintf(msg, sizeof msg, "the game is over"); dirty = 1; } next = t + INTERVAL_MS[speed]; } }
        if (dirty) publish();
        usleep(80000);
    }
    char tmp[P]; snprintf(tmp, sizeof tmp, "%s.tmp", FEED); { FILE *f = fopen(tmp, "w"); if (f) { fputs("gm_on=\n", f); fclose(f); rename(tmp, FEED); } }   /* the bar disappears when the game ends */
    return 0;
}
