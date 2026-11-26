/* rmmc_tutorial_op - the in-game tutorial of the rmmc book as an OP (owner 2026-11-26: "tutorials can be like tools harness", "do it"; plan 16.game/TUTORIAL-HARNESS-ONBOARDING-GAMIFICATION-AND-NEAR-TERM-DRIVE-PLAN-2026-11-26.md).
 * The lesson is DATA (<book>/tutorial.pdl). The PROGRESS is never stored: it is derived from the LEDGER every call, from the byte offset where the lesson was started (no ghost data). The only thing kept is that offset,
 * a PREFERENCE (<book>/play/tutorial.txt: lesson=<id> / offset=<bytes>), deleted any time = the tutorial is off. The op writes nothing else and never touches the ledger.
 *   rmmc_tutorial_op start  <book> <ledger> [lesson]   write play/tutorial.txt (offset = the ledger's size now). Prints  started <lesson>
 *   rmmc_tutorial_op stop   <book>                     delete play/tutorial.txt. Prints  stopped
 *   rmmc_tutorial_op status <book> <ledger> [actor]    prints one line:  off  |  step=<i>/<n> id=<id> short=<text> have=<k> need=<m> goal=<text>  |  done=<n>/<n> lesson=<id>
 *   rmmc_tutorial_op list   <book>                     the lessons: one line per LESSON row
 * Steps are checked IN ORDER by one pass over the ledger rows after the offset: a row that matches the CURRENT step's check counts toward it, and when the step is full the next one starts with the NEXT row (a row never counts twice).
 *   seat_human is the exception: it is a STATE, not an event (the last SEAT row of the actor, from the whole ledger, says human), so a player who is already in control passes it at once.
 * Exit: 0 ok | 2 usage / unreadable / unknown lesson.   Build: gcc -std=gnu11 -Wall -Wextra -O2 -o ops/+x/rmmc_tutorial_op.+x ops/rmmc_tutorial_op.c */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAXSTEP 24
typedef struct { char id[24], shortt[48], goal[200], kind[16], arg[40]; int need; } Step;
static Step ST[MAXSTEP]; static int nst; static char lesson_title[80];

static int split(char *l, char **x, int max) { int n = 0; char *p = l; while (n < max) { x[n++] = p; char *q = strchr(p, '|'); if (!q) break; *q = 0; p = q + 1; }
    for (int i = 0; i < n; i++) { while (*x[i] == ' ' || *x[i] == '\t') x[i]++; char *e = x[i] + strlen(x[i]); while (e > x[i] && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; } return n; }

static int load_lesson(const char *book, const char *lesson) {
    char p[4200]; snprintf(p, sizeof p, "%.4000s/tutorial.pdl", book); FILE *f = fopen(p, "r"); if (!f) return 2; char l[600], *x[8]; int on = 0; nst = 0; lesson_title[0] = 0;
    while (fgets(l, sizeof l, f)) { if (l[0] == '#') continue; int n = split(l, x, 8);
        if (n >= 3 && !strcmp(x[0], "LESSON")) { on = !strcmp(x[1], lesson); if (on) snprintf(lesson_title, sizeof lesson_title, "%s", x[2]); continue; }
        if (on && n >= 6 && !strcmp(x[0], "STEP") && nst < MAXSTEP) { Step *s = &ST[nst++]; snprintf(s->id, sizeof s->id, "%s", x[1]); snprintf(s->shortt, sizeof s->shortt, "%s", x[2]); snprintf(s->goal, sizeof s->goal, "%s", x[3]); snprintf(s->kind, sizeof s->kind, "%s", x[4]); snprintf(s->arg, sizeof s->arg, "%.39s", x[5]); s->need = n >= 6 ? atoi(x[5]) : 1; if (s->need < 1) s->need = 1; } }
    fclose(f); return nst ? 0 : 2; }

static int read_pref(const char *book, char *lesson, size_t ln, long *off) {
    char p[4200]; snprintf(p, sizeof p, "%.4000s/play/tutorial.txt", book); FILE *f = fopen(p, "r"); if (!f) return 0; char l[200]; lesson[0] = 0; *off = -1;
    while (fgets(l, sizeof l, f)) { l[strcspn(l, "\r\n")] = 0; if (!strncmp(l, "lesson=", 7)) snprintf(lesson, ln, "%.60s", l + 7); else if (!strncmp(l, "offset=", 7)) *off = atol(l + 7); }
    fclose(f); return lesson[0] && *off >= 0; }

static int seat_is_human(const char *ledger, const char *actor) {      /* the last SEAT row of the actor, whole ledger */
    FILE *f = fopen(ledger, "r"); if (!f) return 0; char l[400], *x[8]; int human = 0;
    while (fgets(l, sizeof l, f)) { if (strncmp(l, "SEAT", 4)) continue; int n = split(l, x, 8); if (n >= 3 && !strcmp(x[1], actor)) human = !strcmp(x[2], "human"); }
    fclose(f); return human; }

static int is_state(const Step *s) { return !strcmp(s->kind, "seat_human") || !strcmp(s->kind, "picked"); }      /* a STATE step (not an event): passed as soon as the state is true, even before the player does anything */
static int state_ok(const Step *s, const char *book, const char *ledger, const char *actor) {
    if (!strcmp(s->kind, "seat_human")) return seat_is_human(ledger, actor);
    if (!strcmp(s->kind, "picked")) { char p[4200], l[100] = ""; snprintf(p, sizeof p, "%.4000s/play/pick.txt", book); FILE *f = fopen(p, "r"); if (!f) return 0; if (fgets(l, sizeof l, f)) l[strcspn(l, "\r\n")] = 0; fclose(f); return !strcmp(l, s->arg); }
    return 0; }
/* one pass: returns the index of the current step (nst = all done) and how many rows it has so far */
static int progress(const char *book, const char *ledger, const char *actor, long off, int *have) {
    int cur = 0; *have = 0;
    while (cur < nst && is_state(&ST[cur]) && state_ok(&ST[cur], book, ledger, actor)) cur++;
    FILE *f = fopen(ledger, "r"); if (!f) return cur;
    char l[400], *x[10]; int px = -99999, py = -99999, pz = -99999; long pos = 0;
    while (cur < nst && fgets(l, sizeof l, f)) { long at = pos; pos += (long)strlen(l); char cp[400]; snprintf(cp, sizeof cp, "%s", l); int n = split(cp, x, 10); if (n < 2) continue;
        if (at < off) { if (!strcmp(x[0], "MOVE") && n >= 5 && !strcmp(x[1], actor)) { px = atoi(x[2]); py = atoi(x[3]); pz = atoi(x[4]); } continue; }   /* before the start point: only the player's last position, so the first step after it counts */
        int hit = 0; const char *k = ST[cur].kind;
        if (!strcmp(x[0], "MOVE") && n >= 5 && !strcmp(x[1], actor)) { int mx = atoi(x[2]), my = atoi(x[3]), mz = atoi(x[4]);
            if (!strcmp(k, "moves") && (mx != px || mz != pz) && px != -99999) hit = 1;
            if (!strcmp(k, "jump") && mx == px && mz == pz && my == py + 1) hit = 1;
            px = mx; py = my; pz = mz; }
        else if (!strcmp(x[0], "MINE") && !strcmp(k, "mine") && n >= 2 && !strcmp(x[1], actor)) hit = 1;
        else if (!strcmp(x[0], "PLACE") && !strcmp(k, "place") && n >= 2 && !strcmp(x[1], actor)) hit = 1;
        else if (!strcmp(x[0], "RESET") && !strcmp(k, "reset") && n >= 2 && !strcmp(x[1], actor)) hit = 1;
        if (hit && ++*have >= ST[cur].need) { cur++; *have = 0; while (cur < nst && is_state(&ST[cur]) && state_ok(&ST[cur], book, ledger, actor)) cur++; } }
    fclose(f); return cur; }

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: rmmc_tutorial_op start|stop|status|list <book> [ledger] [lesson|actor]\n"); return 2; }
    const char *verb = argv[1], *book = argv[2];
    if (!strcmp(verb, "list")) { char p[4200]; snprintf(p, sizeof p, "%.4000s/tutorial.pdl", book); FILE *f = fopen(p, "r"); if (!f) return 2; char l[600], *x[8]; while (fgets(l, sizeof l, f)) { if (l[0] == '#') continue; int n = split(l, x, 8); if (n >= 3 && !strcmp(x[0], "LESSON")) printf("%s\t%s\n", x[1], x[2]); } fclose(f); return 0; }
    char pp[4200]; snprintf(pp, sizeof pp, "%.4000s/play/tutorial.txt", book);
    if (!strcmp(verb, "stop")) { unlink(pp); puts("stopped"); return 0; }
    if (argc < 4) { fprintf(stderr, "rmmc_tutorial_op: %s needs a ledger\n", verb); return 2; }
    const char *ledger = argv[3];
    if (!strcmp(verb, "start")) { const char *lesson = argc > 4 ? argv[4] : "lesson1"; if (load_lesson(book, lesson)) { fprintf(stderr, "rmmc_tutorial_op: no lesson %s in %s/tutorial.pdl\n", lesson, book); return 2; }
        struct stat st; long off = stat(ledger, &st) ? 0 : (long)st.st_size; char d[4200]; snprintf(d, sizeof d, "%.4000s/play", book); mkdir(d, 0755);
        FILE *f = fopen(pp, "w"); if (!f) return 2; fprintf(f, "lesson=%s\noffset=%ld\n", lesson, off); fclose(f); printf("started %s\n", lesson); return 0; }
    if (!strcmp(verb, "status")) { const char *actor = argc > 4 ? argv[4] : "asa"; char lesson[64]; long off; if (!read_pref(book, lesson, sizeof lesson, &off)) { puts("off"); return 0; }
        if (load_lesson(book, lesson)) { puts("off"); return 0; } int have; int cur = progress(book, ledger, actor, off, &have);
        if (cur >= nst) { printf("done=%d/%d lesson=%s\n", nst, nst, lesson); return 0; }
        printf("step=%d/%d id=%s short=%s have=%d need=%d goal=%s\n", cur + 1, nst, ST[cur].id, ST[cur].shortt, have, ST[cur].need, ST[cur].goal); return 0; }
    fprintf(stderr, "rmmc_tutorial_op: unknown verb %s\n", verb); return 2; }
