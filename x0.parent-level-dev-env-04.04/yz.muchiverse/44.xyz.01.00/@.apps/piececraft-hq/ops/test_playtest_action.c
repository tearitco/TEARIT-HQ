/* test_playtest_action.c - pc-hq `player` verbs (toggle / playtest / stop) against a SCRATCH house: the real pchq_board_action.sh is copied into a scratch tree
 * (it derives HOUSE from its own location), run with fork/exec, and the play-mode file is read back. The live house is never touched.
 * build+run (from this ops dir): gcc -Wall -o /tmp/tpa test_playtest_action.c && /tmp/tpa [ops dir, default cwd]
 * Harness is C on purpose (house direction: harnesses are not sh; later pal + events). */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
static int fails;
#define CK(name, cond) do { if (cond) printf("PASS|%s\n", name); else { printf("FAIL|%s\n", name); fails = 1; } } while (0)
static char H[256], PM[512], SCRIPT[512], BV[512];
static void run(const char *verb, const char *arg) {
    fflush(stdout);   /* the child inherits the stdio buffer: flush first or every line prints once per fork */
    pid_t p = fork();
    if (p == 0) { freopen("/dev/null", "w", stdout); freopen("/dev/null", "w", stderr); execlp("sh", "sh", SCRIPT, BV, verb, arg, (char *)NULL); _exit(127); }
    int st; waitpid(p, &st, 0);
}
static void rd(char *out, size_t n) { FILE *f = fopen(PM, "r"); size_t k = 0; out[0] = 0; if (f) { k = fread(out, 1, n - 1, f); out[k] = 0; fclose(f); } }
static void put(const char *s) { FILE *f = fopen(PM, "w"); if (f) { fputs(s, f); fclose(f); } }
int main(int argc, char **argv) {
    char b[200], cmd[1400]; char src[512];
    snprintf(H, sizeof H, "/tmp/pta_house_XXXXXX"); if (!mkdtemp(H)) return 2;
    snprintf(cmd, sizeof cmd, "mkdir -p '%s/@.apps/piececraft-hq/ops' '%s/#.desktop' '%s/bv'", H, H, H); if (system(cmd)) return 2;
    if (argc > 1) snprintf(src, sizeof src, "%s", argv[1]); else if (!getcwd(src, sizeof src)) return 2;   /* ops dir holding pchq_board_action.sh */
    snprintf(cmd, sizeof cmd, "cp '%s/pchq_board_action.sh' '%s/@.apps/piececraft-hq/ops/'", src, H); if (system(cmd)) { fprintf(stderr, "run from the ops dir\n"); return 2; }
    snprintf(SCRIPT, sizeof SCRIPT, "%s/@.apps/piececraft-hq/ops/pchq_board_action.sh", H);
    snprintf(PM, sizeof PM, "%s/#.desktop/khtpm_play_mode.state.txt", H);
    snprintf(BV, sizeof BV, "%s/bv", H);

    run("player", "playtest"); rd(b, sizeof b);
    CK("off -> playtest writes mode=on + playtest=1", !strcmp(b, "mode=on\nplaytest=1\n"));
    run("player", "playtest"); rd(b, sizeof b);
    CK("playtest again -> plain play (mode=on only)", !strcmp(b, "mode=on\n"));
    run("player", "playtest"); run("player", "stop"); rd(b, sizeof b);
    CK("stop clears play-test (mode=off only)", !strcmp(b, "mode=off\n"));
    run("player", "playtest"); run("player", "toggle"); rd(b, sizeof b);
    CK("toggle from play-test -> off (clears it)", !strcmp(b, "mode=off\n"));
    run("player", "toggle"); rd(b, sizeof b);
    CK("toggle from off -> plain play, no play-test", !strcmp(b, "mode=on\n"));
    put("mode=off\nplaytest=1\n"); run("player", "playtest"); rd(b, sizeof b);
    CK("stale playtest=1 under mode=off is not 'on': next playtest turns it ON", !strcmp(b, "mode=on\nplaytest=1\n"));
    snprintf(cmd, sizeof cmd, "rm -rf %s", H); if (system(cmd)) {}
    printf("VERDICT|%s\n", fails ? "FAIL" : "PASS");
    return fails;
}
