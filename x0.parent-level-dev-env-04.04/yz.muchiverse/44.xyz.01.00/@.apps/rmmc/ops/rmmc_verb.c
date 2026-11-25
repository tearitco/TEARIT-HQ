/* rmmc_verb - the ENGINE of the rmmc verb pages (BOOK 16.game/EVENT-VERB-PAGES-DESIGN.md; the registered command rmmc_verb; same shape as tearit-map/ops/tearit_verb.c, D137).
 *   rmmc_verb.+x <verb>     (lower case, the page name) with the rest of the call in the environment word GAME_ARG (space separated: x z for spawn, a direction for walk and face, who for seat)
 * A page (@.apps/rmmc/events/<verb>/, written by verbgen from verbs.pdl, played by play_event.sh) runs this. It calls ONE rule op, vox_op (walk, face, jump, reset, seat, move), on the rmmc LEDGER, then
 * rmmc_project_op to redraw the PROJECTION the engine reads, and writes play/result.txt ("ok" or "refused: <reason> [exit N]", a PROJECTION; the ledger is the authority).
 * Environment: RMMC_LEDGER (default <house>/@.apps/rpg-edit-hq/state/games/rmmc/game/ledger.txt: the editor instance, never the shared #.desktop/vox_ledger.txt), RMMC_DESK (the projection folder, default the island desk of this book),
 *   RMMC_SEED (default <desk>/seed), RMMC_ACTOR (default asa), RMMC_CALLER (human or harness: when set, it must equal the actor's seat row or the verb refuses with exit 24 and writes NOTHING; unset = a direct call, no check).
 * Exit: 0 accepted; vox_op's own code when it refuses (10 unknown actor, 11 off the island, 14 ledger not reproducible, 15 no ground, 18 blocked); 24 not your seat; 2 unknown verb or bad argument (mine place pick: not wired).
 * Only the verbs named below are accepted (verbgen check reads the verb comparisons below against verbs.pdl).
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/rmmc_verb.+x rmmc_verb.c */
#define _POSIX_C_SOURCE 200809L
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
static int known(const char *v) {
    return !strcmp(v, "spawn") || !strcmp(v, "walk") || !strcmp(v, "face") || !strcmp(v, "jump") || !strcmp(v, "reset") || !strcmp(v, "seat"); }
static int notwired(const char *v) { return !strcmp(v, "mine") || !strcmp(v, "place") || !strcmp(v, "pick"); }
static int run(char *const av[], char *out, size_t osz) {
    int p[2]; if (pipe(p)) return 2; pid_t c = fork(); if (c < 0) return 2;
    if (c == 0) { alarm(20); dup2(p[1], 1); dup2(p[1], 2); close(p[0]); close(p[1]); execv(av[0], av); _exit(127); }
    close(p[1]); size_t n = 0; ssize_t k; while (n < osz - 1 && (k = read(p[0], out + n, osz - 1 - n)) > 0) n += (size_t)k; out[n] = 0; close(p[0]);
    int st = 0; waitpid(c, &st, 0); char *e = strchr(out, '\n'); if (e) *e = 0; return WIFEXITED(st) ? WEXITSTATUS(st) : 99; }
static void result(const char *rp, int code, const char *out) { FILE *f = fopen(rp, "w"); if (f) { if (code == 0) fputs("ok\n", f); else fprintf(f, "refused: %s [exit %d]\n", out[0] ? out : "no reason given", code); fclose(f); } }
int main(int argc, char **argv) {
    if (argc < 2 || (!known(argv[1]) && !notwired(argv[1]))) { fprintf(stderr, "rmmc_verb: unknown verb\n"); return 2; }
    const char *v = argv[1], *arg = getenv("GAME_ARG"); if (!arg) arg = "";
    for (const char *c = arg; *c; c++) if (*c == '\n' || *c == '\r' || *c == '\'' || *c == '|' || (unsigned char)*c < 32) { fprintf(stderr, "rmmc_verb: bad argument\n"); return 2; }
    if (notwired(v)) { fprintf(stderr, "rmmc_verb: %s is not wired yet (NOT VERIFIED)\n", v); return 2; }
    char a1[64] = "", a2[64] = ""; { char t[256]; snprintf(t, sizeof t, "%s", arg); for (char *c = t; *c; c++) if (*c == ',') *c = ' '; if (sscanf(t, "%63s %63s", a1, a2) < 0) a1[0] = 0; }
    if ((!strcmp(v, "walk") || !strcmp(v, "face")) && (strlen(a1) != 1 || !strchr("nesw", a1[0]))) { fprintf(stderr, "rmmc_verb: %s needs n, e, s or w\n", v); return 2; }
    int sx = 0, sz = 0; if (!strcmp(v, "spawn") && (!a1[0] || !a2[0] || sscanf(a1, "%d", &sx) != 1 || sscanf(a2, "%d", &sz) != 1)) { fprintf(stderr, "rmmc_verb: spawn needs x z\n"); return 2; }
    if (!strcmp(v, "seat") && strcmp(a1, "human") && strcmp(a1, "harness")) { fprintf(stderr, "rmmc_verb: seat needs human or harness\n"); return 2; }
    char self[PATH_MAX]; if (!realpath(argv[0], self)) return 2;
    char house[PATH_MAX]; snprintf(house, sizeof house, "%s", self); for (int i = 0; i < 5; i++) { char *s = strrchr(house, '/'); if (!s) return 2; *s = 0; }   /* <house>/@.apps/rmmc/ops/+x/<this> */
    char xo[PATH_MAX + 16]; snprintf(xo, sizeof xo, "%s/xyzfs", house); struct stat st; if (stat(xo, &st) != 0) { fprintf(stderr, "rmmc_verb: house not found above %s\n", self); return 2; }
    static char book[PATH_MAX + 32], vop[PATH_MAX + 64], pop[PATH_MAX + 64], data[PATH_MAX + 64], led[PATH_MAX + 128], desk[PATH_MAX + 160], seed[PATH_MAX + 200], pose[PATH_MAX + 64], hm[PATH_MAX + 96], rp[PATH_MAX + 80], dir[PATH_MAX + 64];
    snprintf(book, sizeof book, "%s/@.apps/rmmc", house); snprintf(vop, sizeof vop, "%s/@.apps/vox/ops/+x/vox_op.+x", house); snprintf(pop, sizeof pop, "%s/ops/+x/rmmc_project_op.+x", book);
    snprintf(data, sizeof data, "%s/vox", book); snprintf(pose, sizeof pose, "%s/pose.pdl", book); snprintf(hm, sizeof hm, "%s/vox/terrain.heightmap", book);
    const char *e = getenv("RMMC_LEDGER"); if (e && *e) snprintf(led, sizeof led, "%s", e); else snprintf(led, sizeof led, "%s/@.apps/rpg-edit-hq/state/games/rmmc/game/ledger.txt", house);
    e = getenv("RMMC_DESK"); if (e && *e) snprintf(desk, sizeof desk, "%s", e); else snprintf(desk, sizeof desk, "%s/rmmc-pchq/pieces/system/maps/rmmc/rmmc-0001", book);
    e = getenv("RMMC_SEED"); if (e && *e) snprintf(seed, sizeof seed, "%s", e); else snprintf(seed, sizeof seed, "%s/seed", desk);
    const char *actor = getenv("RMMC_ACTOR"); if (!actor || !*actor) actor = "asa";
    { char d[PATH_MAX + 130]; snprintf(d, sizeof d, "%s", led); for (char *p = d + 1; *p; p++) if (*p == '/') { *p = 0; mkdir(d, 0755); *p = '/'; } char *sl = strrchr(d, '/'); if (sl) { *sl = 0; mkdir(d, 0755); } }   /* the ledger folder is made on the first verb (mkdir -p) */
    snprintf(dir, sizeof dir, "%s/play", book); mkdir(dir, 0755); snprintf(rp, sizeof rp, "%s/result.txt", dir);
    char out[600] = ""; int code = 0;
    { const char *who = getenv("RMMC_CALLER");   /* the seat check: nothing is written when it is not this caller's turn */
      if (who && *who && strcmp(v, "seat")) { char *av[] = { vop, "seat", data, led, (char *)actor, NULL }; char o2[200]; int c2 = run(av, o2, sizeof o2);
          if (c2 == 0 && strncmp(o2, "seat=", 5) == 0 && strcmp(o2 + 5, who)) { snprintf(out, sizeof out, "not your seat (the seat is %s)", o2 + 5); result(rp, 24, out); fprintf(stderr, "%s\n", out); return 24; } } }
    if (!strcmp(v, "spawn")) { char gx[32], gz[32], o2[200]; snprintf(gx, sizeof gx, "%d", sx); snprintf(gz, sizeof gz, "%d", sz); char *g[] = { vop, "ground", data, led, gx, gz, NULL }; int c2 = run(g, o2, sizeof o2);
        int top = -1; if (c2 == 0) sscanf(o2, "top=%d", &top);
        if (c2 != 0 || top < 0) { snprintf(out, sizeof out, "no ground at %d,%d", sx, sz); code = 11; }
        else { char my[32]; snprintf(my, sizeof my, "%d", top + 1); char *av[] = { vop, "move", data, led, (char *)actor, gx, my, gz, NULL }; code = run(av, out, sizeof out); } }
    else if (!strcmp(v, "walk")) { char *av[] = { vop, "walk", data, led, (char *)actor, a1, NULL }; code = run(av, out, sizeof out); }
    else if (!strcmp(v, "face")) { char *av[] = { vop, "face", data, led, (char *)actor, a1, NULL }; code = run(av, out, sizeof out); }
    else if (!strcmp(v, "jump")) { char *av[] = { vop, "jump", data, led, (char *)actor, NULL }; code = run(av, out, sizeof out); }
    else if (!strcmp(v, "reset")) { char *av[] = { vop, "reset", data, led, (char *)actor, NULL }; code = run(av, out, sizeof out); }
    else { char *av[] = { vop, "seat", data, led, (char *)actor, a1, NULL }; code = run(av, out, sizeof out); }
    if (code == 0 && access(pop, X_OK) == 0) { char o2[300]; char *av[] = { pop, "project", seed, desk, led, pose, hm, NULL }; if (run(av, o2, sizeof o2)) { snprintf(out, sizeof out, "projection failed: %s", o2); code = 5; } }
    result(rp, code, out); if (code) fprintf(stderr, "%s\n", out);
    return code; }
