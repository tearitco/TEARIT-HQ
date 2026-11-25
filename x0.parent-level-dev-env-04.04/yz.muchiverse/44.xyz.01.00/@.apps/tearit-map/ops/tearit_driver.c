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

static void publish(const char *feed, int on, const char *msg, const char *op, const char *ledger, const char *dir, const char *edit) {
    char tmp[1600];
    FILE *f;
    const char *fl = fog_on(edit) ? "Fog on" : "Fog off";
    snprintf(tmp, sizeof tmp, "%s.tmp", feed);
    f = fopen(tmp, "w");
    if (!f) return;
    if (!on) fputs("gm_on=\ngm_pop=\ngmbar_on=\n", f);
    else {
        /* The board already draws this feed: gm_pop empty hides the centre popup, gmbar_on shows the bottom strip in its place. */
        fprintf(f, "gm_on=1\ngm_pid=%ld\ngm_pop=\ngmbar_on=1\n", (long)getpid());
        fprintf(f, "gmbar_title=%s\ngmbar_n=6\n", msg && *msg ? msg : "Tearit map");
        fprintf(f, "gmbar_0_label=North\ngmbar_0_act='%s' step '%s' '%s' n\n", op, ledger, ledger);
        fprintf(f, "gmbar_1_label=East\ngmbar_1_act='%s' step '%s' '%s' e\n", op, ledger, ledger);
        fprintf(f, "gmbar_2_label=South\ngmbar_2_act='%s' step '%s' '%s' s\n", op, ledger, ledger);
        fprintf(f, "gmbar_3_label=West\ngmbar_3_act='%s' step '%s' '%s' w\n", op, ledger, ledger);
        fprintf(f, "gmbar_4_label=Dig\ngmbar_4_act='%s' dig '%s' '%s'\n", op, ledger, ledger);
        fprintf(f, "gmbar_5_label=%s\ngmbar_5_act='%s' fog '%s' '%s'\n", fl, op, ledger, dir);
    }
    fclose(f);
    rename(tmp, feed);
}

/* The left stamp row already exists. Its list is stamps_ui.txt. */
static void stamps(const char *edit, int on, const char *op, const char *ledger, const char *dir) {
    char path[1000], tmp[1100];
    FILE *f;
    const char *fl = fog_on(edit) ? "Fog on" : "Fog off";
    snprintf(path, sizeof path, "%s/state/stamps_ui.txt", edit);
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    f = fopen(tmp, "w");
    if (!f) return;
    if (!on) fputs("stamp_on=\nn_stamp=0\n", f);
    else {
        fputs("stamp_on=1\nn_stamp=6\n", f);
        fprintf(f, "stamp_0_label=North\nstamp_0_act='%s' step '%s' '%s' n\n", op, ledger, ledger);
        fprintf(f, "stamp_1_label=East\nstamp_1_act='%s' step '%s' '%s' e\n", op, ledger, ledger);
        fprintf(f, "stamp_2_label=South\nstamp_2_act='%s' step '%s' '%s' s\n", op, ledger, ledger);
        fprintf(f, "stamp_3_label=West\nstamp_3_act='%s' step '%s' '%s' w\n", op, ledger, ledger);
        fprintf(f, "stamp_4_label=Dig\nstamp_4_act='%s' dig '%s' '%s'\n", op, ledger, ledger);
        fprintf(f, "stamp_5_label=%s\nstamp_5_act='%s' fog '%s' '%s'\n", fl, op, ledger, dir);
    }
    fclose(f);
    rename(tmp, path);
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

static int act_one(const char *op, const char *ledger, const char *dir, const char *ln, char *msg, int msz) {
    char *av[6];
    av[0] = (char *)op;
    if (!strncmp(ln, "step ", 5) && ln[5] && !ln[6]) {
        av[1] = "step"; av[2] = (char *)ledger; av[3] = (char *)ledger; av[4] = (char *)(ln + 5); av[5] = NULL;
    } else if (!strcmp(ln, "dig") || !strcmp(ln, "found")) {
        av[1] = (char *)ln; av[2] = (char *)ledger; av[3] = (char *)ledger; av[4] = NULL;
    } else { snprintf(msg, msz, "unknown"); return 1; }
    if (run_op(av)) { snprintf(msg, msz, "refused"); return 1; }
    if (desk(op, ledger, dir)) { snprintf(msg, msz, "desk failed"); return 1; }
    snprintf(msg, msz, "%s", ln);
    return 0;
}

int main(int argc, char **argv) {
    char op[1100], sw[1100], feed[1100], ledger[1100], deskdir[1100], act[1100], lk[1100];
    char msg[160] = "seed 1 is on the board";
    const char *book, *edit;
    int lf;
    long cursor = 0;
    struct sigaction sa;
    struct stat st;
    if (argc < 4) { fprintf(stderr, "usage: tearit_driver <house> <book> <editor>\n"); return 2; }
    book = argv[2]; edit = argv[3];
    snprintf(op, sizeof op, "%s/ops/+x/tearit_map_op.+x", book);
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
    bars(argv[1], book, edit, "play");
    hotbar_read(edit);
    hotbar(edit, "0");
    stamps(edit, 1, op, ledger, deskdir);
    publish(feed, 1, "Tearit map", op, ledger, deskdir, edit);
    { const char *ow = getenv("GAME_OWNER_PID"); long owner = ow ? atol(ow) : 0; long led_m = 0;
    if (!stat(ledger, &st)) led_m = (long)st.st_mtime;
    while (!stop_) {
        if (owner > 1 && kill((pid_t)owner, 0) != 0 && errno == ESRCH) break;
        if (!stat(act, &st) && st.st_size > cursor && running(sw)) {
            FILE *f = fopen(act, "r");
            if (f) {
                char ln[128];
                if (fseek(f, cursor, SEEK_SET) == 0) while (fgets(ln, sizeof ln, f)) {
                    char *e = strpbrk(ln, "\r\n"); if (e) *e = 0;
                    if (ln[0]) act_one(op, ledger, deskdir, ln, msg, sizeof msg);
                }
                cursor = ftell(f); fclose(f);
            }
            if (cursor > 65536 && truncate(act, 0) == 0) cursor = 0;
        } else if (!stat(act, &st) && st.st_size < cursor) cursor = 0;
        if (!stat(ledger, &st) && (long)st.st_mtime != led_m) { led_m = (long)st.st_mtime; desk(op, ledger, deskdir); }
        publish(feed, 1, "Tearit map", op, ledger, deskdir, edit);
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
