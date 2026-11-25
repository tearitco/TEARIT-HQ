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
    return !strcmp(v, "spawn") || !strcmp(v, "walk") || !strcmp(v, "face") || !strcmp(v, "jump") || !strcmp(v, "reset") || !strcmp(v, "seat") || !strcmp(v, "mine") || !strcmp(v, "place"); }
static int run(char *const av[], char *out, size_t osz) {
    int p[2]; if (pipe(p)) return 2; pid_t c = fork(); if (c < 0) return 2;
    if (c == 0) { alarm(20); dup2(p[1], 1); dup2(p[1], 2); close(p[0]); close(p[1]); execv(av[0], av); _exit(127); }
    close(p[1]); size_t n = 0; ssize_t k; while (n < osz - 1 && (k = read(p[0], out + n, osz - 1 - n)) > 0) n += (size_t)k; out[n] = 0; close(p[0]);
    int st = 0; waitpid(c, &st, 0); char *e = strchr(out, '\n'); if (e) *e = 0; return WIFEXITED(st) ? WEXITSTATUS(st) : 99; }
static void result(const char *rp, int code, const char *out) { FILE *f = fopen(rp, "w"); if (f) { if (code == 0) fputs("ok\n", f); else fprintf(f, "refused: %s [exit %d]\n", out[0] ? out : "no reason given", code); fclose(f); } }
/* facing from the LEDGER, by the rule the projection draws with (rmmc_project_op: the last horizontal MOVE delta or the last FACE row, whichever is later; a jump moves only in y and keeps it): so what she visibly faces is the column mine and place act on */
static char facing_of(const char *ledger, const char *actor) {
    FILE *f = fopen(ledger, "r"); if (!f) return 0; char l[256], dir = 0; int px = 0, pz = 0, have = 0;
    while (fgets(l, sizeof l, f)) { char k[16], a[64], b[32], c[32], d[32];
        if (sscanf(l, "%15s | %63s | %31s | %31s | %31s", k, a, b, c, d) >= 3 && !strcmp(a, actor)) {
            if (!strcmp(k, "FACE") && strlen(b) == 1 && strchr("nesw", b[0])) dir = b[0];
            else if (!strcmp(k, "MOVE")) { int x = atoi(b), z = atoi(d); if (have) { if (x > px) dir = 'e'; else if (x < px) dir = 'w'; else if (z > pz) dir = 's'; else if (z < pz) dir = 'n'; } px = x; pz = z; have = 1; } } }
    fclose(f); return dir; }
int main(int argc, char **argv) {
    if (argc < 2 || (!known(argv[1]))) { fprintf(stderr, "rmmc_verb: unknown verb\n"); return 2; }
    const char *v = argv[1], *arg = getenv("GAME_ARG"); if (!arg) arg = "";
    for (const char *c = arg; *c; c++) if (*c == '\n' || *c == '\r' || *c == '\'' || *c == '|' || (unsigned char)*c < 32) { fprintf(stderr, "rmmc_verb: bad argument\n"); return 2; }
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
    else if (!strcmp(v, "mine") || !strcmp(v, "place")) {   /* the column IN FRONT of her: where (her cell + facing) -> that column's top -> MINE it / PLACE on top of it (one ledger row each, vox_op) */
        char o2[200]; char *wa[] = { vop, "where", data, led, (char *)actor, NULL }; int ax = 0, ay = 0, az = 0; char fc = '-';
        if (run(wa, o2, sizeof o2) != 0 || sscanf(o2, "at=%d,%d,%d", &ax, &ay, &az) != 3) { snprintf(out, sizeof out, "no such actor"); code = 10; }
        else { char *fp = strstr(o2, "face="); if (fp) fc = fp[5]; if (!strchr("nesw", fc)) fc = facing_of(led, actor); if (!strchr("nesw", fc)) fc = 's';
            int fx = ax + (fc == 'e') - (fc == 'w'), fz = az + (fc == 's') - (fc == 'n'); char gx[32], gz[32], o3[200]; snprintf(gx, sizeof gx, "%d", fx); snprintf(gz, sizeof gz, "%d", fz);
            char *g[] = { vop, "ground", data, led, gx, gz, NULL }; int c2 = run(g, o3, sizeof o3); int top = -1; if (c2 == 0) sscanf(o3, "top=%d", &top);
            if (c2 != 0 || top < 0) { snprintf(out, sizeof out, "no ground in front at %d,%d", fx, fz); code = 11; }
            else if (!strcmp(v, "mine")) { char cell[96]; snprintf(cell, sizeof cell, "%d,%d,%d", fx, top, fz); char *av[] = { vop, "mine", data, led, (char *)actor, "hand", cell, NULL }; code = run(av, out, sizeof out); }
            else { char item[64] = "dirt", pp[PATH_MAX + 96]; snprintf(pp, sizeof pp, "%s/play/pick.txt", book); FILE *pf = fopen(pp, "r"); if (pf) { if (fgets(item, sizeof item, pf)) item[strcspn(item, "\r\n'| ")] = 0; fclose(pf); } if (!item[0]) snprintf(item, sizeof item, "dirt");
                char cell[96], one[8] = "1"; snprintf(cell, sizeof cell, "%d,%d,%d", fx, top + 1, fz); char *gv[] = { vop, "give", data, led, (char *)actor, item, one, "hotbar", NULL }; char og[200]; run(gv, og, sizeof og);   /* creative hotbar: the picked block is given first, as a GIVE row */
                char *av[] = { vop, "place", data, led, (char *)actor, item, cell, NULL }; code = run(av, out, sizeof out); } } }
    else { char *av[] = { vop, "seat", data, led, (char *)actor, a1, NULL }; code = run(av, out, sizeof out); }
    if (code == 0 && access(pop, X_OK) == 0) { char o2[300]; char *av[] = { pop, "project", seed, desk, led, pose, hm, NULL }; if (run(av, o2, sizeof o2)) { snprintf(out, sizeof out, "projection failed: %s", o2); code = 5; } }
    result(rp, code, out); if (code) fprintf(stderr, "%s\n", out);
    return code; }
