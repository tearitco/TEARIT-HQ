/* Player > Play for earth-moth. The four walk buttons are the same
 * stamp bar and bottom strip tearit-map uses. The rules stay in
 * earth_moth_op. A step is refused off the grass. */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static volatile sig_atomic_t stop_;
static void on_term(int s) { (void)s; stop_ = 1; }

static int run_op(char *const av[]) {
    pid_t p = fork();
    int st = 0;
    if (p < 0) return 1;
    if (p == 0) { execv(av[0], av); _exit(127); }
    if (waitpid(p, &st, 0) < 0) return 1;
    return WIFEXITED(st) && WEXITSTATUS(st) == 0 ? 0 : 1;
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

static void publish(const char *feed, int on, const char *op, const char *etopo, const char *ledger) {
    char *mb = NULL; size_t ml = 0;
    FILE *f = open_memstream(&mb, &ml);
    if (!f) return;
    if (!on) fputs("gm_on=\ngm_pop=\ngmbar_on=\n", f);
    else {
        fprintf(f, "gm_on=1\ngm_pid=%ld\ngm_pop=\ngmbar_on=1\n", (long)getpid());
        fputs("gmbar_title=Earth moth\ngmbar_n=4\n", f);
        fprintf(f, "gmbar_0_label=North\ngmbar_0_act='%s' step '%s' '%s' n\n", op, etopo, ledger);
        fprintf(f, "gmbar_1_label=East\ngmbar_1_act='%s' step '%s' '%s' e\n", op, etopo, ledger);
        fprintf(f, "gmbar_2_label=South\ngmbar_2_act='%s' step '%s' '%s' s\n", op, etopo, ledger);
        fprintf(f, "gmbar_3_label=West\ngmbar_3_act='%s' step '%s' '%s' w\n", op, etopo, ledger);
    }
    fclose(f);
    put_if_changed(feed, mb, ml);
    free(mb);
}

static void stamps(const char *edit, int on, const char *op, const char *etopo, const char *ledger) {
    char path[1000];
    char *mb = NULL; size_t ml = 0;
    FILE *f = open_memstream(&mb, &ml);
    snprintf(path, sizeof path, "%s/state/stamps_ui.txt", edit);
    if (!f) return;
    if (!on) fputs("stamp_on=\nn_stamp=0\n", f);
    else {
        fputs("stamp_on=1\nn_stamp=4\n", f);
        fprintf(f, "stamp_0_label=North\nstamp_0_act='%s' step '%s' '%s' n\n", op, etopo, ledger);
        fprintf(f, "stamp_1_label=East\nstamp_1_act='%s' step '%s' '%s' e\n", op, etopo, ledger);
        fprintf(f, "stamp_2_label=South\nstamp_2_act='%s' step '%s' '%s' s\n", op, etopo, ledger);
        fprintf(f, "stamp_3_label=West\nstamp_3_act='%s' step '%s' '%s' w\n", op, etopo, ledger);
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

static int desk(const char *op, const char *etopo, const char *mine, const char *ledger, const char *dir) {
    char *av[] = { (char *)op, "desk", (char *)etopo, (char *)mine, (char *)ledger, (char *)dir, NULL };
    return run_op(av);
}

int main(int argc, char **argv) {
    char op[1100], sw[1100], feed[1100], ledger[1100], deskdir[1100], act[1100], lk[1100];
    char etopo[1100], mine[1100];
    const char *house, *book, *edit;
    int lf;
    long cursor = 0;
    struct sigaction sa;
    struct stat st;
    if (argc < 4) { fprintf(stderr, "usage: earth_moth_driver <house> <book> <editor>\n"); return 2; }
    house = argv[1]; book = argv[2]; edit = argv[3];
    snprintf(op, sizeof op, "%s/ops/+x/earth_moth_op.+x", book);
    snprintf(etopo, sizeof etopo, "%s/../../../#.NNEST_ASSETS/rta-sources/earth-moth/ETOPO5.DAT", house);
    snprintf(mine, sizeof mine, "%s/../../../#.NNEST_ASSETS/mineclonia/mods", house);
    snprintf(sw, sizeof sw, "%s/state/world/switches.txt", edit);
    snprintf(feed, sizeof feed, "%s/state/game_ui.txt", edit);
    snprintf(ledger, sizeof ledger, "%s/state/ledger.txt", book);
    snprintf(deskdir, sizeof deskdir, "%s/earth-moth-pchq/pieces/system/maps/earth-moth/earth", book);
    snprintf(act, sizeof act, "%s/state/games/earth-moth/action.txt", edit);
    snprintf(lk, sizeof lk, "%s/state/games/earth-moth/driver.lock", edit);
    {
        char d[1100];
        snprintf(d, sizeof d, "%s/state", edit); mkdir(d, 0755);
        snprintf(d, sizeof d, "%s/state/games", edit); mkdir(d, 0755);
        snprintf(d, sizeof d, "%s/state/games/earth-moth", edit); mkdir(d, 0755);
        snprintf(d, sizeof d, "%s/state", book); mkdir(d, 0755);
    }
    lf = open(lk, O_RDWR | O_CREAT, 0644);
    if (lf < 0) { fprintf(stderr, "earth_moth_driver: cannot open lock\n"); return 1; }
    if (flock(lf, LOCK_EX | LOCK_NB)) { fprintf(stderr, "earth_moth_driver: already running\n"); return 0; }
    memset(&sa, 0, sizeof sa); sa.sa_handler = on_term;
    sigaction(SIGTERM, &sa, NULL); sigaction(SIGINT, &sa, NULL);
    cursor = stat(act, &st) ? 0 : (long)st.st_size;
    bars(house, book, edit, "play");
    hotbar_read(edit);
    hotbar(edit, "0");
    stamps(edit, 1, op, etopo, ledger);
    publish(feed, 1, op, etopo, ledger);
    {
        const char *ow = getenv("GAME_OWNER_PID");
        long owner = ow ? atol(ow) : 0;
        long led_m = 0;
        if (!stat(ledger, &st)) led_m = (long)st.st_size;
        while (!stop_) {
            if (owner > 1 && kill((pid_t)owner, 0) != 0 && errno == ESRCH) break;
            if (!stat(act, &st) && (long)st.st_size > cursor && running(sw)) {
                FILE *f = fopen(act, "r");
                if (f) {
                    char ln[128];
                    if (fseek(f, cursor, SEEK_SET) == 0) while (fgets(ln, sizeof ln, f)) {
                        char *e = strpbrk(ln, "\r\n");
                        char *av[6];
                        if (e) *e = 0;
                        if (ln[0] == 'n' || ln[0] == 'e' || ln[0] == 's' || ln[0] == 'w') {
                            av[0] = op; av[1] = "step"; av[2] = etopo; av[3] = ledger; av[4] = ln; av[5] = NULL;
                            run_op(av);
                        }
                    }
                    cursor = ftell(f);
                    fclose(f);
                }
                if (cursor > 65536 && truncate(act, 0) == 0) cursor = 0;
            } else if (!stat(act, &st) && (long)st.st_size < cursor) cursor = 0;
            if (!stat(ledger, &st) && (long)st.st_size != led_m) {
                led_m = (long)st.st_size;
                desk(op, etopo, mine, ledger, deskdir);
            }
            publish(feed, 1, op, etopo, ledger);
            stamps(edit, 1, op, etopo, ledger);
            usleep(250000);
        }
    }
    bars(house, book, edit, "edit");
    hotbar(edit, hb_was);
    stamps(edit, 0, op, etopo, ledger);
    publish(feed, 0, op, etopo, ledger);
    return 0;
}
