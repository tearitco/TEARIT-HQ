/* civ_verb - the ENGINE of the civ-snes verb pages (BOOK 16.game/CIV-SNES-2D-AND-3D-PLAN-2026-11-27.md, decision D157; the registered command civ_verb, same shape as tearit_verb).
 *   civ_verb.+x <verb>   (lower case, the page name), the rest of the call in the environment word GAME_ARG (space separated: unit and direction for move; unit for found; city and kind for build; civ and who for seat)
 * An event page (@.apps/civ-snes/events/<verb>/, written by verbgen from verbs.pdl, played by play_event.sh) runs this; the page is the path, this program is the leaf: it calls ONE rule op (civ_op, whose verbs are the rules),
 * then re-projects the desk the 2D and 3D board read, and writes play/result.txt ("ok" or "refused: <reason>", a PROJECTION the driver shows on the bar; the ledger is the authority).
 * Which ledger: CIV_LEDGER (the driver sets it; harness cases point it into $T); unset = the editor's live ledger of this house. CIV_DESK = the desk folder rewritten after an accepted verb (unset = none);
 * CIV_SEE = whose fog the desk shows (civ1 default, "all" for the harness). Exit 0 = accepted, the op's own code when it refuses (4), 2 = unknown verb or bad argument.
 * Only the verbs named below are accepted (verbgen check reads the verb comparisons below against verbs.pdl).
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/civ_verb.+x civ_verb.c */
#define _POSIX_C_SOURCE 200809L
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
static int known(const char *v) { return !strcmp(v, "move") || !strcmp(v, "found") || !strcmp(v, "build") || !strcmp(v, "endturn") || !strcmp(v, "seat") || !strcmp(v, "start"); }
static int run(char *const av[], char *out, size_t osz) {
    int p[2]; if (pipe(p)) return 2; pid_t c = fork(); if (c < 0) return 2;
    if (c == 0) { alarm(20); dup2(p[1], 1); dup2(p[1], 2); close(p[0]); close(p[1]); execv(av[0], av); _exit(127); }
    close(p[1]); size_t n = 0; ssize_t k; while (n < osz - 1 && (k = read(p[0], out + n, osz - 1 - n)) > 0) n += (size_t)k; out[n] = 0; close(p[0]);
    int st = 0; waitpid(c, &st, 0); char *e = strchr(out, '\n'); if (e) *e = 0; return WIFEXITED(st) ? WEXITSTATUS(st) : 99; }
int main(int argc, char **argv) {
    if (argc < 2 || !known(argv[1])) { fprintf(stderr, "civ_verb: unknown verb\n"); return 2; }
    const char *v = argv[1], *arg = getenv("GAME_ARG"); if (!arg) arg = "";
    for (const char *c = arg; *c; c++) if (*c == '\n' || *c == '\r' || *c == '\'' || *c == '|' || (unsigned char)*c < 32) { fprintf(stderr, "civ_verb: bad argument\n"); return 2; }
    char a1[64] = "", a2[64] = ""; if (sscanf(arg, "%63s %63s", a1, a2) < 0) a1[0] = 0;
    if ((!strcmp(v, "move") || !strcmp(v, "build") || !strcmp(v, "seat")) && (!a1[0] || !a2[0])) { fprintf(stderr, "civ_verb: %s needs two arguments\n", v); return 2; }
    if (!strcmp(v, "found") && !a1[0]) { fprintf(stderr, "civ_verb: found needs a unit\n"); return 2; }
    char self[PATH_MAX]; if (!realpath(argv[0], self)) return 2;
    char house[PATH_MAX]; snprintf(house, sizeof house, "%s", self); for (int i = 0; i < 5; i++) { char *s = strrchr(house, '/'); if (!s) return 2; *s = 0; }   /* <house>/@.apps/civ-snes/ops/+x/<this> */
    char xo[PATH_MAX + 16]; snprintf(xo, sizeof xo, "%s/xyzfs", house); struct stat st; if (stat(xo, &st) != 0) { fprintf(stderr, "civ_verb: house not found above %s\n", self); return 2; }
    char book[PATH_MAX + 32], op[PATH_MAX + 64], led[PATH_MAX + 128], desk[PATH_MAX + 128], dir[PATH_MAX + 64], rp[PATH_MAX + 80];
    snprintf(book, sizeof book, "%s/@.apps/civ-snes", house); snprintf(op, sizeof op, "%s/ops/+x/civ_op.+x", book);
    const char *e = getenv("CIV_LEDGER"); if (e && *e) snprintf(led, sizeof led, "%s", e); else snprintf(led, sizeof led, "%s/@.apps/rpg-edit-hq/state/games/civ-snes/game/ledger.txt", house);
    e = getenv("CIV_DESK"); desk[0] = 0; if (e && *e) snprintf(desk, sizeof desk, "%s", e);
    const char *see = getenv("CIV_SEE"); if (!see || !*see) see = "civ1";
    snprintf(dir, sizeof dir, "%s/play", book); mkdir(dir, 0755); snprintf(rp, sizeof rp, "%s/result.txt", dir);
    char out[600] = ""; int code;
    if (!strcmp(v, "move")) { char *av[] = { op, led, "move", a1, a2, NULL }; code = run(av, out, sizeof out); }
    else if (!strcmp(v, "found")) { char *av[] = { op, led, "found", a1, NULL }; code = run(av, out, sizeof out); }
    else if (!strcmp(v, "build")) { char *av[] = { op, led, "build", a1, a2, NULL }; code = run(av, out, sizeof out); }
    else if (!strcmp(v, "endturn")) { char *av[] = { op, led, "endturn", NULL }; code = run(av, out, sizeof out); }
    else if (!strcmp(v, "start")) { char dr[PATH_MAX + 64]; snprintf(dr, sizeof dr, "%s/play/setup.txt", book); char *av[] = { op, led, "start", dr, NULL }; code = run(av, out, sizeof out); }
    else { char *av[] = { op, led, "seat", a1, a2, NULL }; code = run(av, out, sizeof out); }
    if (code == 0 && desk[0]) { char o2[300]; const char *sel = getenv("CIV_SEL"); char *av[] = { op, led, "project", (char *)see, desk, (char *)(sel && *sel ? sel : NULL), NULL }; if (run(av, o2, sizeof o2)) { snprintf(out, sizeof out, "desk failed"); code = 5; } }
    if (code == 4 && !strncmp(out, "refused: ", 9)) memmove(out, out + 9, strlen(out + 9) + 1);
    FILE *f = fopen(rp, "w"); if (f) { if (code == 0) fputs("ok\n", f); else fprintf(f, "refused: %s\n", out[0] ? out : "no reason given"); fclose(f); }
    return code; }
