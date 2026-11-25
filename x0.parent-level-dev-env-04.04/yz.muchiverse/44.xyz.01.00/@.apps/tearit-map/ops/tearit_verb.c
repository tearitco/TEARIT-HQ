/* tearit_verb - the ENGINE of the tearit-map verb pages (BOOK 16.game/EVENT-VERB-PAGES-DESIGN.md; the registered command tearit_verb; same shape as tear-on/ops/tear_on_verb.c).
 *   tearit_verb.+x <verb>     (lower case, the page name) with the rest of the call in the environment word GAME_ARG (space separated: a direction for step; civ and who for seat)
 * An event page (@.apps/tearit-map/events/<verb>/, written by verbgen from verbs.pdl, played by play_event.sh) runs this; the page is the path, this program is the leaf:
 * it calls ONE rule op (tearit_map_op for the walk, found, eat, sleep, dig, place, fog and hut rules; tearit_civ_op for endturn and seat), so every rule stays in those ops,
 * and writes play/result.txt ("ok" or "refused: <reason>", a PROJECTION the driver shows on the bar; the ledger is the authority).
 * Which ledger: TEARIT_LEDGER (the driver sets it; harness cases point it into $T); unset = the editor's live ledger of this house. TEARIT_DESK, when set, is the desk folder
 * rewritten after an accepted verb (fog always needs a folder and falls back to desk/ beside the ledger). Exit 0 = accepted, the op's own code when it refuses (4), 2 = unknown verb or bad argument.
 * Only the verbs named below are accepted (verbgen check reads the verb comparisons below against verbs.pdl).
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/tearit_verb.+x tearit_verb.c */
#define _POSIX_C_SOURCE 200809L
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
static int known(const char *v) {
    return !strcmp(v, "step") || !strcmp(v, "found") || !strcmp(v, "eat") || !strcmp(v, "sleep") || !strcmp(v, "dig") || !strcmp(v, "place") ||
           !strcmp(v, "fog") || !strcmp(v, "hut") || !strcmp(v, "endturn") || !strcmp(v, "seat"); }
static int run(char *const av[], char *out, size_t osz) {
    int p[2]; if (pipe(p)) return 2; pid_t c = fork(); if (c < 0) return 2;
    if (c == 0) { alarm(20); dup2(p[1], 1); dup2(p[1], 2); close(p[0]); close(p[1]); execv(av[0], av); _exit(127); }
    close(p[1]); size_t n = 0; ssize_t k; while (n < osz - 1 && (k = read(p[0], out + n, osz - 1 - n)) > 0) n += (size_t)k; out[n] = 0; close(p[0]);
    int st = 0; waitpid(c, &st, 0); char *e = strchr(out, '\n'); if (e) *e = 0; return WIFEXITED(st) ? WEXITSTATUS(st) : 99; }
int main(int argc, char **argv) {
    if (argc < 2 || !known(argv[1])) { fprintf(stderr, "tearit_verb: unknown verb\n"); return 2; }
    const char *v = argv[1], *arg = getenv("GAME_ARG"); if (!arg) arg = "";
    for (const char *c = arg; *c; c++) if (*c == '\n' || *c == '\r' || *c == '\'' || *c == '|' || (unsigned char)*c < 32) { fprintf(stderr, "tearit_verb: bad argument\n"); return 2; }
    char a1[64] = "", a2[64] = ""; if (sscanf(arg, "%63s %63s", a1, a2) < 0) a1[0] = 0;
    if (!strcmp(v, "step") && (strlen(a1) != 1 || !strchr("nesw", a1[0]))) { fprintf(stderr, "tearit_verb: step needs n, e, s or w\n"); return 2; }
    if (!strcmp(v, "seat") && (!a1[0] || !a2[0])) { fprintf(stderr, "tearit_verb: seat needs civ and who\n"); return 2; }
    char self[PATH_MAX]; if (!realpath(argv[0], self)) return 2;
    char house[PATH_MAX]; snprintf(house, sizeof house, "%s", self); for (int i = 0; i < 5; i++) { char *s = strrchr(house, '/'); if (!s) return 2; *s = 0; }   /* <house>/@.apps/tearit-map/ops/+x/<this> */
    char xo[PATH_MAX + 16]; snprintf(xo, sizeof xo, "%s/xyzfs", house); struct stat st; if (stat(xo, &st) != 0) { fprintf(stderr, "tearit_verb: house not found above %s\n", self); return 2; }
    char book[PATH_MAX + 32], mop[PATH_MAX + 64], cop[PATH_MAX + 64], led[PATH_MAX + 128], desk[PATH_MAX + 128], dir[PATH_MAX + 64], rp[PATH_MAX + 80];
    snprintf(book, sizeof book, "%s/@.apps/tearit-map", house); snprintf(mop, sizeof mop, "%s/ops/+x/tearit_map_op.+x", book); snprintf(cop, sizeof cop, "%s/ops/+x/tearit_civ_op.+x", book);
    const char *e = getenv("TEARIT_LEDGER"); if (e && *e) snprintf(led, sizeof led, "%s", e); else snprintf(led, sizeof led, "%s/@.apps/rpg-edit-hq/state/games/tearit-map/game/ledger.txt", house);
    e = getenv("TEARIT_DESK"); desk[0] = 0; if (e && *e) snprintf(desk, sizeof desk, "%s", e);
    snprintf(dir, sizeof dir, "%s/play", book); mkdir(dir, 0755); snprintf(rp, sizeof rp, "%s/result.txt", dir);
    char out[600] = ""; int code;
    if (!strcmp(v, "step")) { char *av[] = { mop, "step", led, led, a1, NULL }; code = run(av, out, sizeof out); }
    else if (!strcmp(v, "fog")) { char fd[PATH_MAX + 160]; snprintf(fd, sizeof fd, "%s", desk[0] ? desk : led); if (!desk[0]) { char *s = strrchr(fd, '/'); if (s) strcpy(s + 1, "desk"); }
        char *av[] = { mop, "fog", led, fd, NULL }; code = run(av, out, sizeof out); }
    else if (!strcmp(v, "endturn")) { char *av[] = { cop, "endturn", led, NULL }; code = run(av, out, sizeof out); }
    else if (!strcmp(v, "seat")) { char *av[] = { cop, "seat", led, a1, a2, NULL }; code = run(av, out, sizeof out); }
    else { char *av[] = { mop, (char *)v, led, led, NULL }; code = run(av, out, sizeof out); }
    if (code == 0 && desk[0] && strcmp(v, "fog")) { char o2[300]; char *av[] = { mop, "desk", led, desk, led, NULL }; if (run(av, o2, sizeof o2)) { snprintf(out, sizeof out, "desk failed"); code = 5; } }
    FILE *f = fopen(rp, "w"); if (f) { if (code == 0) fputs("ok\n", f); else fprintf(f, "refused: %s\n", out[0] ? out : "no reason given"); fclose(f); }
    return code; }
