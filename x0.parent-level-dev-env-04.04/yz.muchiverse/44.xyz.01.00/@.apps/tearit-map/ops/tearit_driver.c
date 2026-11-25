/* Player > Play for the tearit-map book. One flock, a feed the editor
 * draws, and steps only while the editor switch running=1. The rules
 * stay in tearit_map_op. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>

static volatile sig_atomic_t stop_;
static void on_term(int s) { (void)s; stop_ = 1; }

/* The bar and stamp buttons only APPEND a command to the action queue (tearit_ctl); this driver reads it and plays the verb PAGE (events/<verb>/), see do_verb. */
static char CTL[1100], PLAYD[1100], HOUSE_[1100], BOOK_[1100], STATUS[200] = "Tearit map", MSG[160] = "seed 1 is on the board";
static long led_m_;
static void btn(FILE *f, const char *pre, int i, const char *label, const char *cmd) {
    fprintf(f, "%s_%d_label=%s\n%s_%d_act='%s' %s '%s'\n", pre, i, label, pre, i, CTL, cmd, PLAYD);
}

static int running(const char *sw) {
    FILE *f = fopen(sw, "r");
    char ln[80];
    int r = 0;
    if (!f) return 0;
    while (fgets(ln, sizeof ln, f)) if (!strncmp(ln, "running=", 8)) r = atoi(ln + 8);
    fclose(f);
    return r;
}

static int run_op(char *const av[]) {
    pid_t p = fork();
    int st = 0;
    if (p < 0) return 1;
    if (p == 0) { execv(av[0], av); _exit(127); }
    if (waitpid(p, &st, 0) < 0) return 1;
    return WIFEXITED(st) && WEXITSTATUS(st) == 0 ? 0 : 1;
}

static int roam_on(const char *edit) {
    char p[1100], ln[8];
    FILE *f;
    snprintf(p, sizeof p, "%s/state/games/tearit-map/roam.txt", edit);
    f = fopen(p, "r");
    if (!f) return 0;
    if (!fgets(ln, sizeof ln, f)) { fclose(f); return 0; }
    fclose(f);
    return ln[0] == '1';
}

static int fog_on(const char *edit) {
    char p[1100], ln[8];
    FILE *f;
    snprintf(p, sizeof p, "%s/state/games/tearit-map/fog.txt", edit);
    f = fopen(p, "r");
    if (!f) return 1;
    if (!fgets(ln, sizeof ln, f)) { fclose(f); return 1; }
    fclose(f);
    return ln[0] != '0';
}

/* Write buf to path only when the bytes differ from what is on disk (temp + rename as before). */
static void put_if_changed(const char *path, const char *buf, size_t n) {
    char tmp[1700];
    FILE *f = fopen(path, "r");
    if (f) {
        char cur[8192]; size_t got = fread(cur, 1, sizeof cur, f);
        int more = fgetc(f) != EOF;
        fclose(f);
        if (!more && got == n && memcmp(cur, buf, n) == 0) return;
    }
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    f = fopen(tmp, "w");
    if (!f) return;
    fwrite(buf, 1, n, f);
    fclose(f);
    rename(tmp, path);
}

static void publish(const char *feed, int on, const char *msg, const char *op, const char *ledger, const char *dir, const char *edit) {
    char *mb = NULL; size_t ml = 0;
    FILE *f = open_memstream(&mb, &ml);
    const char *fl = fog_on(edit) ? "Fog on" : "Fog off";
    if (!f) return;
    if (!on) fputs("gm_on=\ngm_pop=\ngmbar_on=\n", f);
    else {
        /* The board already draws this feed: gm_pop empty hides the centre popup, gmbar_on shows the bottom strip in its place.
         * The strip title is the Civ status line (a fold of the ledger, see refresh_status) and the last message. */
        fprintf(f, "gm_on=1\ngm_pid=%ld\ngm_pop=\ngmbar_on=1\n", (long)getpid());
        fprintf(f, "gmbar_title=%s | %s\ngmbar_n=9\n", STATUS, msg && *msg ? msg : "Tearit map");
        btn(f, "gmbar", 0, "Next turn", "endturn");
        btn(f, "gmbar", 1, "North", "step_n"); btn(f, "gmbar", 2, "East", "step_e"); btn(f, "gmbar", 3, "South", "step_s"); btn(f, "gmbar", 4, "West", "step_w");
        btn(f, "gmbar", 5, "Found city", "found"); btn(f, "gmbar", 6, "Eat", "eat"); btn(f, "gmbar", 7, "Sleep", "sleep"); btn(f, "gmbar", 8, fl, "fog");
        (void)op; (void)ledger; (void)dir;
    }
    fclose(f);
    put_if_changed(feed, mb, ml);
    free(mb);
}

/* The left stamp row already exists. Its list is stamps_ui.txt. */
static void stamps(const char *edit, int on, const char *op, const char *ledger, const char *dir) {
    char path[1000];
    char *mb = NULL; size_t ml = 0;
    FILE *f = open_memstream(&mb, &ml);
    const char *fl = fog_on(edit) ? "Fog on" : "Fog off";
    snprintf(path, sizeof path, "%s/state/stamps_ui.txt", edit);
    if (!f) return;
    if (!on) fputs("stamp_on=\nn_stamp=0\n", f);
    else {
        const char *rl = roam_on(edit) ? "Life on" : "Life off";
        const char *nm[12] = { "Next turn", "North", "East", "South", "West", "Found city", "Eat", "Sleep", "Dig", "Hut", fl, rl };
        const char *cm[12] = { "endturn", "step_n", "step_e", "step_s", "step_w", "found", "eat", "sleep", "dig", "hut", "fog", "life" };
        fputs("stamp_on=1\nn_stamp=12\n", f);
        for (int i = 0; i < 12; i++) btn(f, "stamp", i, nm[i], cm[i]);
        (void)op; (void)ledger; (void)dir;
    }
    fclose(f);
    put_if_changed(path, mb, ml);
    free(mb);
}

static void bars(const char *house, const char *book, const char *edit, const char *mode) {
    char sides[1100], elay[1100], blay[1100], uiop[1100];
    snprintf(sides, sizeof sides, "%s/board/state/sides.txt", edit);
    snprintf(elay, sizeof elay, "%s/layouts.pdl", edit);
    snprintf(blay, sizeof blay, "%s/layouts.pdl", book);
    snprintf(uiop, sizeof uiop, "%s/&.widgits/_shared-lib/ops/+x/ui_mode_op.+x", house);
    if (!strcmp(mode, "play")) {
        char ps[1100];
        snprintf(ps, sizeof ps, "%s/@.apps/rpg-edit-hq/ops/+x/pal_side.+x", house);
        if (access(ps, X_OK) == 0) {
            char *pv[] = { ps, (char *)house, "stamps", NULL };
            run_op(pv);
        }
    }
    if (access(uiop, X_OK) != 0 || access(elay, R_OK) != 0) return;
    if (access(blay, R_OK) == 0) {
        char *av[] = { uiop, sides, "--bars", blay, "--bars", elay, "--no-settings", (char *)mode, NULL };
        run_op(av);
    } else {
        char *av[] = { uiop, sides, "--bars", elay, "--no-settings", (char *)mode, NULL };
        run_op(av);
    }
}

static char hb_was[8] = "0";
static void hotbar_read(const char *edit) {
    char hb[1100], ln[16];
    FILE *hf;
    snprintf(hb, sizeof hb, "%s/state/flag_hb.txt", edit);
    hf = fopen(hb, "r");
    hb_was[0] = '0'; hb_was[1] = 0;
    if (!hf) return;
    if (fgets(ln, sizeof ln, hf) && ln[0] == '1') { hb_was[0] = '1'; hb_was[1] = 0; }
    fclose(hf);
}
static void hotbar(const char *edit, const char *v) {
    char hb[1100];
    FILE *hf;
    snprintf(hb, sizeof hb, "%s/state/flag_hb.txt", edit);
    hf = fopen(hb, "w");
    if (!hf) return;
    fprintf(hf, "%s\n", v);
    fclose(hf);
}

static int desk(const char *op, const char *ledger, const char *dir) {
    char *av[] = { (char *)op, "desk", (char *)ledger, (char *)dir, (char *)ledger, NULL };
    return run_op(av);
}

/* Play the verb PAGE (events/<verb>/, written by verbgen) through the shared play_event.sh; the page runs tearit_verb, which calls ONE rule op and writes play/result.txt.
 * Visible fallback (as tear_on_driver do_row): when the page is missing, or the owner paused events in Blueprintz (paused=1 in #.desktop/blueprintz_debug.txt: a paused page would only QUEUE), run the same engine tearit_verb directly and say so in the message. */
static int do_verb(const char *verb, const char *args, const char *ledger, const char *dir, char *msg, int msz) {
    char pd[1300], pl[1300], rp[1300], wd[1300], eng[1300], bp[1300], bl[64], rb[300] = "";
    const char *ov = getenv("BLUEPRINTZ_DEBUG_FILE");
    int held = 0, ran = 0;
    struct stat ps;
    snprintf(pd, sizeof pd, "%s/events/%s", BOOK_, verb);
    snprintf(pl, sizeof pl, "%s/&.widgits/events-hq/ops/play_event.sh", HOUSE_);
    snprintf(rp, sizeof rp, "%s/play/result.txt", BOOK_);
    snprintf(wd, sizeof wd, "%s/play/world", BOOK_);
    snprintf(eng, sizeof eng, "%s/ops/+x/tearit_verb.+x", BOOK_);
    snprintf(bp, sizeof bp, "%s", ov ? ov : ""); if (!ov) snprintf(bp, sizeof bp, "%s/#.desktop/blueprintz_debug.txt", HOUSE_);
    { FILE *bf = fopen(bp, "r"); if (bf) { while (fgets(bl, sizeof bl, bf)) if (!strncmp(bl, "paused=1", 8)) held = 1; fclose(bf); } }
    setenv("GAME_ARG", args, 1); setenv("TEARIT_LEDGER", ledger, 1); setenv("TEARIT_DESK", dir, 1); setenv("MUCHI_TARGET_ENT", wd, 1);
    if (!held && !stat(pd, &ps) && !access(pl, R_OK)) {
        char *av[] = { "/bin/bash", pl, pd, HOUSE_, "ui", NULL };
        char play[1300]; snprintf(play, sizeof play, "%s/play", BOOK_); mkdir(play, 0755); mkdir(wd, 0755); unlink(rp);
        run_op(av);
        { FILE *rf = fopen(rp, "r"); if (rf) { if (fgets(rb, sizeof rb, rf)) rb[strcspn(rb, "\r\n")] = 0; fclose(rf); } }
        ran = rb[0] != 0;
    }
    if (!ran) {
        char *av[] = { eng, (char *)verb, NULL };
        int c = run_op(av);
        FILE *rf = fopen(rp, "r"); if (rf) { if (fgets(rb, sizeof rb, rf)) rb[strcspn(rb, "\r\n")] = 0; fclose(rf); }
        if (!rb[0]) snprintf(rb, sizeof rb, "%s", c ? "refused" : "ok");
        if (!strcmp(rb, "ok")) snprintf(msg, msz, "%s (direct: the %s page did not run%s)", verb, verb, held ? ", events are paused in Blueprintz" : "");
        else snprintf(msg, msz, "%s", rb);
    } else if (!strcmp(rb, "ok")) snprintf(msg, msz, "%s", verb);
    else snprintf(msg, msz, "%s", rb);
    unsetenv("GAME_ARG"); unsetenv("TEARIT_LEDGER"); unsetenv("TEARIT_DESK"); unsetenv("MUCHI_TARGET_ENT");
    return !strcmp(rb, "ok") ? 0 : 1;
}

/* The status line is a FOLD of the ledger (tearit_civ_op status), read again only when the ledger changed: "turn N | cities C | food F". */
static void refresh_status(const char *civ, const char *ledger) {
    char out[300] = "";
    int t = 0, c = 0, f = 0;
    FILE *p;
    { int pp[2]; pid_t k; if (pipe(pp)) return; k = fork();
      if (k < 0) return;
      if (k == 0) { dup2(pp[1], 1); close(pp[0]); close(pp[1]); execl(civ, civ, "status", ledger, (char *)NULL); _exit(127); }
      close(pp[1]); p = fdopen(pp[0], "r"); if (p) { if (!fgets(out, sizeof out, p)) out[0] = 0; fclose(p); } waitpid(k, NULL, 0); }
    if (sscanf(out, "turn=%d cities=%d food=%d", &t, &c, &f) == 3) snprintf(STATUS, sizeof STATUS, "turn %d | cities %d | food %d", t, c, f);
    else snprintf(STATUS, sizeof STATUS, "turn ? (ledger refused)");
}

/* Seats are rows: the first Play of a ledger writes civ1 human and civ2 agent:basic (through the op, never by hand). */
static void ensure_seats(const char *civ, const char *ledger) {
    FILE *f = fopen(ledger, "r"); char ln[512]; int have = 0;
    if (f) { while (fgets(ln, sizeof ln, f)) if (!strncmp(ln, "SEAT |", 6)) { have = 1; break; } fclose(f); }
    if (have) return;
    { char *a1[] = { (char *)civ, "seat", (char *)ledger, "civ1", "human", NULL }; run_op(a1); }
    { char *a2[] = { (char *)civ, "seat", (char *)ledger, "civ2", "agent:basic", NULL }; run_op(a2); }
}

/* One queued command from a button (tearit_ctl) or the old "step n" line: play its page. */
static int act_one(const char *op, const char *ledger, const char *dir, const char *ln, char *msg, int msz) {
    char verb[130] = "", args[130] = "";
    if (!strncmp(ln, "step ", 5) && ln[5] && !ln[6]) { snprintf(verb, sizeof verb, "step"); snprintf(args, sizeof args, "%s", ln + 5); }
    else if (!strncmp(ln, "step_", 5) && ln[5] && !ln[6]) { snprintf(verb, sizeof verb, "step"); snprintf(args, sizeof args, "%s", ln + 5); }
    else if (!strcmp(ln, "dig") || !strcmp(ln, "found") || !strcmp(ln, "eat") || !strcmp(ln, "sleep") || !strcmp(ln, "hut") || !strcmp(ln, "fog") ||
             !strcmp(ln, "endturn") || !strcmp(ln, "place")) snprintf(verb, sizeof verb, "%s", ln);
    else if (!strcmp(ln, "life")) {
        char *av[] = { (char *)op, "life", (char *)ledger, NULL };
        if (run_op(av)) { snprintf(msg, msz, "refused"); return 1; }
        snprintf(msg, msz, "life toggled"); return 0;
    } else { snprintf(msg, msz, "unknown"); return 1; }
    return do_verb(verb, args, ledger, dir, msg, msz);
}

int main(int argc, char **argv) {
    char civ[1100], op[1100], sw[1100], feed[1100], ledger[1100], deskdir[1100], act[1100], lk[1100];
    const char *book, *edit;
    int lf;
    long cursor = 0;
    struct sigaction sa;
    struct stat st;
    if (argc < 4) { fprintf(stderr, "usage: tearit_driver <house> <book> <editor>\n"); return 2; }
    book = argv[2]; edit = argv[3];
    snprintf(HOUSE_, sizeof HOUSE_, "%s", argv[1]); snprintf(BOOK_, sizeof BOOK_, "%s", book);
    snprintf(CTL, sizeof CTL, "%s/ops/+x/tearit_ctl.+x", book); snprintf(PLAYD, sizeof PLAYD, "%s/state/games/tearit-map", edit);
    snprintf(op, sizeof op, "%s/ops/+x/tearit_map_op.+x", book);
    snprintf(civ, sizeof civ, "%s/ops/+x/tearit_civ_op.+x", book);
    snprintf(sw, sizeof sw, "%s/state/world/switches.txt", edit);
    snprintf(feed, sizeof feed, "%s/state/game_ui.txt", edit);
    snprintf(ledger, sizeof ledger, "%s/state/games/tearit-map/game/ledger.txt", edit);
    snprintf(deskdir, sizeof deskdir, "%s/tearit-map-pchq/pieces/system/maps/tearit-map/board", book);
    snprintf(act, sizeof act, "%s/state/games/tearit-map/action.txt", edit);
    snprintf(lk, sizeof lk, "%s/state/games/tearit-map/driver.lock", edit);
    mkdir(edit, 0755);
    { char d[1100]; snprintf(d, sizeof d, "%s/state/games/tearit-map/game", edit); 
      snprintf(d, sizeof d, "%s/state", edit); mkdir(d, 0755);
      snprintf(d, sizeof d, "%s/state/games", edit); mkdir(d, 0755);
      snprintf(d, sizeof d, "%s/state/games/tearit-map", edit); mkdir(d, 0755);
      snprintf(d, sizeof d, "%s/state/games/tearit-map/game", edit); mkdir(d, 0755); }
    if (access(ledger, R_OK) != 0) {
        char *av[] = { op, "gen", "1", "16", "16", ledger, NULL };
        if (run_op(av)) { fprintf(stderr, "tearit_driver: gen failed\n"); return 1; }
    }
    if (desk(op, ledger, deskdir)) { fprintf(stderr, "tearit_driver: desk failed\n"); return 1; }
    lf = open(lk, O_RDWR | O_CREAT, 0644);
    if (lf < 0) { fprintf(stderr, "tearit_driver: cannot open lock\n"); return 1; }
    if (flock(lf, LOCK_EX | LOCK_NB)) { fprintf(stderr, "tearit_driver: already running\n"); return 0; }
    memset(&sa, 0, sizeof sa); sa.sa_handler = on_term;
    sigaction(SIGTERM, &sa, NULL); sigaction(SIGINT, &sa, NULL);
    cursor = stat(act, &st) ? 0 : (long)st.st_size;
    ensure_seats(civ, ledger);
    refresh_status(civ, ledger);
    bars(argv[1], book, edit, "play");
    hotbar_read(edit);
    hotbar(edit, "0");
    stamps(edit, 1, op, ledger, deskdir);
    publish(feed, 1, MSG, op, ledger, deskdir, edit);
    { const char *ow = getenv("GAME_OWNER_PID"); long owner = ow ? atol(ow) : 0; int beat = 0;
    if (!stat(ledger, &st)) led_m_ = (long)st.st_size;
    while (!stop_) {
        if (owner > 1 && kill((pid_t)owner, 0) != 0 && errno == ESRCH) break;
        if (!stat(act, &st) && st.st_size > cursor && running(sw)) {
            FILE *f = fopen(act, "r");
            if (f) {
                char ln[128];
                if (fseek(f, cursor, SEEK_SET) == 0) while (fgets(ln, sizeof ln, f)) {
                    char *e = strpbrk(ln, "\r\n"); if (e) *e = 0;
                    if (ln[0]) { act_one(op, ledger, deskdir, ln, MSG, sizeof MSG); if (!stat(ledger, &st)) led_m_ = (long)st.st_size; refresh_status(civ, ledger); }
                }
                cursor = ftell(f); fclose(f);
            }
            if (cursor > 65536 && truncate(act, 0) == 0) cursor = 0;
        } else if (!stat(act, &st) && st.st_size < cursor) cursor = 0;
        if (roam_on(edit) && (beat++ % 4) == 0) {
            char *av[] = { op, "roam", ledger, ledger, NULL };
            run_op(av);
        }
        if (!stat(ledger, &st) && (long)st.st_size != led_m_) { led_m_ = (long)st.st_size; desk(op, ledger, deskdir); refresh_status(civ, ledger); }
        publish(feed, 1, MSG, op, ledger, deskdir, edit);
        stamps(edit, 1, op, ledger, deskdir);
        usleep(250000);
    }
    bars(argv[1], book, edit, "edit");
    hotbar(edit, hb_was);
    stamps(edit, 0, op, ledger, deskdir);
    }
    publish(feed, 0, "", op, ledger, deskdir, edit);
    return 0;
}
