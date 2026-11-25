/* rmmc_play.inc - TEXT INCLUDE (pure, two consumers: rmmc_beat_op and rmmc_driver; house rule: no header + link). Plays ONE rmmc verb as the EVENT PAGE (events/<verb>/, written by verbgen) through the shared
 * play_event.sh, the page being the path (D92); the page runs rmmc_verb, which calls vox_op then rmmc_project_op. Visible fallback (as tear_on_driver do_row, tearit_driver do_verb): when the page is missing, or the owner paused
 * events in Blueprintz (paused=1 in #.desktop/blueprintz_debug.txt, or in the file BLUEPRINTZ_DEBUG_FILE names: a paused page would only QUEUE), the same engine rmmc_verb is run directly and the message SAYS so.
 * The caller sets env RMMC_LEDGER / RMMC_DESK / RMMC_SEED itself. Returns the verb's exit code (read back from `[exit N]` in play/result.txt on the page path; 0 = accepted). Needs <stdio.h> <stdlib.h> <string.h> <unistd.h> <sys/stat.h> <sys/wait.h>. */
static int rp_run(char *const av[]) { pid_t p = fork(); int st = 0; if (p < 0) return 99; if (p == 0) { execv(av[0], av); _exit(127); } if (waitpid(p, &st, 0) < 0) return 99; return WIFEXITED(st) ? WEXITSTATUS(st) : 99; }
static int rmmc_play_verb(const char *house, const char *book, const char *verb, const char *args, const char *caller, char *msg, int msz) {
    char pd[4400], pl[4400], rp[4400], wd[4400], eng[4400], bp[4400], bl[64], rb[400] = ""; const char *ov = getenv("BLUEPRINTZ_DEBUG_FILE"); int held = 0, ran = 0, code = 0; struct stat ps;
    snprintf(pd, sizeof pd, "%s/events/%s", book, verb); snprintf(pl, sizeof pl, "%s/&.widgits/events-hq/ops/play_event.sh", house); snprintf(rp, sizeof rp, "%s/play/result.txt", book);
    snprintf(wd, sizeof wd, "%s/play/world", book); snprintf(eng, sizeof eng, "%s/ops/+x/rmmc_verb.+x", book);
    snprintf(bp, sizeof bp, "%s", ov ? ov : ""); if (!ov) snprintf(bp, sizeof bp, "%s/#.desktop/blueprintz_debug.txt", house);
    { FILE *bf = fopen(bp, "r"); if (bf) { while (fgets(bl, sizeof bl, bf)) if (!strncmp(bl, "paused=1", 8)) held = 1; fclose(bf); } }
    setenv("GAME_ARG", args, 1); setenv("MUCHI_TARGET_ENT", wd, 1); setenv("RMMC_CALLER", caller, 1);
    if (!held && !stat(pd, &ps) && !access(pl, R_OK)) {
        char *av[] = { "/bin/bash", pl, pd, (char *)house, "ui", NULL }; char play[4400]; snprintf(play, sizeof play, "%s/play", book); mkdir(play, 0755); mkdir(wd, 0755); unlink(rp);
        rp_run(av); { FILE *rf = fopen(rp, "r"); if (rf) { if (fgets(rb, sizeof rb, rf)) rb[strcspn(rb, "\r\n")] = 0; fclose(rf); } } ran = rb[0] != 0; }
    if (!ran) { char *av[] = { eng, (char *)verb, NULL }; code = rp_run(av); FILE *rf = fopen(rp, "r"); if (rf) { if (fgets(rb, sizeof rb, rf)) rb[strcspn(rb, "\r\n")] = 0; fclose(rf); }
        if (!rb[0]) snprintf(rb, sizeof rb, "%s", code ? "refused" : "ok");
        if (!strcmp(rb, "ok")) snprintf(msg, (size_t)msz, "%s (direct: the %s page did not run%s)", verb, verb, held ? ", events are paused in Blueprintz" : ""); else snprintf(msg, (size_t)msz, "%s", rb); }
    else { if (!strcmp(rb, "ok")) { code = 0; snprintf(msg, (size_t)msz, "%s", verb); } else { const char *x = strstr(rb, "[exit "); code = x ? atoi(x + 6) : 1; snprintf(msg, (size_t)msz, "%s", rb); } }
    unsetenv("GAME_ARG"); unsetenv("MUCHI_TARGET_ENT"); unsetenv("RMMC_CALLER"); return code;
}
