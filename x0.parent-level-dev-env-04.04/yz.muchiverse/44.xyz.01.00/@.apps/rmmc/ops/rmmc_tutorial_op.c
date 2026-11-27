/* rmmc_tutorial_op - the in-game tutorial of the rmmc book as an OP (owner 2026-11-26: "tutorials can be like tools harness", "do it"; plan 16.game/TUTORIAL-HARNESS-ONBOARDING-GAMIFICATION-AND-NEAR-TERM-DRIVE-PLAN-2026-11-26.md).
 * The lesson is DATA (<book>/tutorial.pdl). The PROGRESS is never stored: it is derived from the LEDGER every call, from the byte offset where the lesson was started (no ghost data). The only thing kept is that offset,
 * a PREFERENCE (<book>/play/tutorial.txt: lesson=<id> / offset=<bytes>), deleted any time = the tutorial is off. The op writes nothing else and never touches the ledger.
 *   rmmc_tutorial_op start  <book> <ledger> [lesson]   write play/tutorial.txt (offset = the ledger's size now). Prints  started <lesson>
 *   rmmc_tutorial_op stop   <book>                     delete play/tutorial.txt. Prints  stopped
 *   rmmc_tutorial_op status <book> <ledger> [actor]    prints one line:  off  |  step=<i>/<n> id=<id> short=<text> have=<k> need=<m> goal=<text>  |  done=<n>/<n> lesson=<id>
 *   rmmc_tutorial_op list   <book>                     the lessons: one line per LESSON row
 *   rmmc_tutorial_op check  <book>                     every STEP fits the left bar: the goal word-wraps into <= RULE goal_lines lines of RULE bar_width chars, the step line (as the driver draws it) fits one. Prints  ok: ...  or one  too long: ...  line per row; exit 1 when any is too long
 * Check kinds: seat_human, moves N, jump, mine N, mined <item> (a DROP of <item> after a MINE), place N, reset, picked <item>; see tutorial.pdl.
 * Steps are checked IN ORDER by one pass over the ledger rows after the offset: a row that matches the CURRENT step's check counts toward it, and when the step is full the next one starts with the NEXT row (a row never counts twice).
 *   seat_human is the exception: it is a STATE, not an event (the last SEAT row of the actor, from the whole ledger, says human), so a player who is already in control passes it at once.
 * Exit: 0 ok | 2 usage / unreadable / unknown lesson.   Build: gcc -std=gnu11 -Wall -Wextra -O2 -o ops/+x/rmmc_tutorial_op.+x ops/rmmc_tutorial_op.c */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#define MAXSTEP 24
typedef struct { char id[24], shortt[48], goal[200], kind[16], arg[40]; int need; } Step;
static Step ST[MAXSTEP]; static int nst; static char lesson_title[80];

static int split(char *l, char **x, int max) { int n = 0; char *p = l; while (n < max) { x[n++] = p; char *q = strchr(p, '|'); if (!q) break; *q = 0; p = q + 1; }
    for (int i = 0; i < n; i++) { while (*x[i] == ' ' || *x[i] == '\t') x[i]++; char *e = x[i] + strlen(x[i]); while (e > x[i] && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; } return n; }

static int load_lesson(const char *book, const char *lesson) {
    char p[4200]; snprintf(p, sizeof p, "%.4000s/tutorial.pdl", book); FILE *f = fopen(p, "r"); if (!f) return 2; char l[600], *x[8]; int on = 0; nst = 0; lesson_title[0] = 0;
    while (fgets(l, sizeof l, f)) { if (l[0] == '#') continue; int n = split(l, x, 8);
        if (n >= 3 && !strcmp(x[0], "LESSON")) { on = !strcmp(x[1], lesson); if (on) snprintf(lesson_title, sizeof lesson_title, "%s", x[2]); continue; }
        if (on && n >= 6 && !strcmp(x[0], "STEP") && nst < MAXSTEP) { Step *s = &ST[nst++]; snprintf(s->id, sizeof s->id, "%s", x[1]); snprintf(s->shortt, sizeof s->shortt, "%s", x[2]); snprintf(s->goal, sizeof s->goal, "%s", x[3]); snprintf(s->kind, sizeof s->kind, "%s", x[4]); snprintf(s->arg, sizeof s->arg, "%.39s", x[5]); s->need = n >= 6 ? atoi(x[5]) : 1; if (s->need < 1) s->need = 1; } }      /* an item argument (picked dirt, mined dirt) reads as need 1 */
    fclose(f); return nst ? 0 : 2; }

static int read_pref(const char *book, char *lesson, size_t ln, long *off, const char *ledger) {      /* offset_<ledger file name>= wins over the generic offset=: a lesson started with a ledger FOLDER remembers the start of every page ledger in it */
    char p[4200]; snprintf(p, sizeof p, "%.4000s/play/tutorial.txt", book); FILE *f = fopen(p, "r"); if (!f) return 0; char l[200], key[200]; lesson[0] = 0; *off = -1; long spec = -1; const char *bn = strrchr(ledger, '/'); bn = bn ? bn + 1 : ledger; snprintf(key, sizeof key, "offset_%.150s=", bn);
    while (fgets(l, sizeof l, f)) { l[strcspn(l, "\r\n")] = 0; if (!strncmp(l, "lesson=", 7)) snprintf(lesson, ln, "%.60s", l + 7); else if (!strncmp(l, key, strlen(key))) spec = atol(l + strlen(key)); else if (!strncmp(l, "offset=", 7)) *off = atol(l + 7); }
    if (spec >= 0) *off = spec;
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
        else if (!strcmp(x[0], "DROP") && !strcmp(k, "mined") && n >= 3 && !strcmp(x[1], actor) && !strcmp(x[2], ST[cur].arg)) hit = 1;      /* mined <item>: a broken block that put <item> in the bag (the DROP row vox_op writes after MINE). Stranger QA 2026-11-27: "mine any block" let a player break sand, then "pick dirt, place it" was refused with creative off */
        else if (!strcmp(x[0], "PLACE") && !strcmp(k, "place") && n >= 2 && !strcmp(x[1], actor)) hit = 1;
        else if (!strcmp(x[0], "RESET") && !strcmp(k, "reset") && n >= 2 && !strcmp(x[1], actor)) hit = 1;
        if (hit && ++*have >= ST[cur].need) { cur++; *have = 0; while (cur < nst && is_state(&ST[cur]) && state_ok(&ST[cur], book, ledger, actor)) cur++; } }
    fclose(f); return cur; }

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: rmmc_tutorial_op start|stop|status|list|check <book> [ledger] [lesson|actor]\n"); return 2; }
    const char *verb = argv[1], *book = argv[2];
    if (!strcmp(verb, "list")) { char p[4200]; snprintf(p, sizeof p, "%.4000s/tutorial.pdl", book); FILE *f = fopen(p, "r"); if (!f) return 2; char l[600], *x[8]; while (fgets(l, sizeof l, f)) { if (l[0] == '#') continue; int n = split(l, x, 8); if (n >= 3 && !strcmp(x[0], "LESSON")) printf("%s\t%s\n", x[1], x[2]); } fclose(f); return 0; }
    if (!strcmp(verb, "check")) {      /* every line must FIT the screen (STRANGER QA 2026-11-27, live photos): the editor's left bar is 190 px, about 20 characters a line, word-wrapped; the goal sentence gets two lines (a third is clipped:
                                         * "Press B to swing the pickaxe. The second swing..." lost its point) and the step line ONE (its wrapped count "0/5" hid under the party box). RULE rows of tutorial.pdl: bar_width, goal_lines.
                                         * The step line is checked exactly as rmmc_driver draws it: "<i>/<n> <short>" plus " <k> of <need>" when need > 1, at its widest. */
        char p[4200]; snprintf(p, sizeof p, "%.4000s/tutorial.pdl", book); FILE *f = fopen(p, "r"); if (!f) return 2; char l[600], *x[8]; int width = 20, glines = 2, rows = 0, bad = 0, per[32] = {0}, nl = -1;
        while (fgets(l, sizeof l, f)) { if (l[0] == '#') continue; int n = split(l, x, 8); if (n >= 3 && !strcmp(x[0], "RULE") && !strcmp(x[1], "bar_width")) width = atoi(x[2]); else if (n >= 3 && !strcmp(x[0], "RULE") && !strcmp(x[1], "goal_lines")) glines = atoi(x[2]);
            else if (n >= 3 && !strcmp(x[0], "LESSON") && nl < 31) nl++; else if (n >= 6 && !strcmp(x[0], "STEP") && nl >= 0) per[nl]++; }
        if (width < 8) width = 8;
        rewind(f); nl = -1; while (fgets(l, sizeof l, f)) { if (l[0] == '#') continue; int n = split(l, x, 8); if (n >= 3 && !strcmp(x[0], "LESSON") && nl < 31) { nl++; continue; } if (n < 6 || strcmp(x[0], "STEP") || nl < 0) continue; rows++;
            int lines = 1, cur = 0; char g[300]; snprintf(g, sizeof g, "%s", x[3]);      /* greedy word wrap, as the renderer does */
            for (char *w = strtok(g, " "); w; w = strtok(NULL, " ")) { int wl = (int)strlen(w); if (!cur) cur = wl; else if (cur + 1 + wl <= width) cur += 1 + wl; else { lines++; cur = wl; } while (cur > width) { lines++; cur -= width; } }
            if (lines > glines) { printf("too long: step %s goal takes %d lines of %d chars (max %d): %s\n", x[1], lines, width, glines, x[3]); bad++; }
            int need = atoi(x[5]); char sl[200]; int k = snprintf(sl, sizeof sl, "%d/%d %s", per[nl], per[nl], x[2]); if (need > 1) k += snprintf(sl + k, sizeof sl - (size_t)k, " %d of %d", need, need);
            if (k > width) { printf("too long: step %s step line %d > %d chars: %s\n", x[1], k, width, sl); bad++; } }
        fclose(f); if (!bad) printf("ok: %d steps fit (goal <= %d lines, step line <= 1 line, %d chars a line)\n", rows, glines, width); return bad ? 1 : 0; }
    char pp[4200]; snprintf(pp, sizeof pp, "%.4000s/play/tutorial.txt", book);
    if (!strcmp(verb, "stop")) { unlink(pp); puts("stopped"); return 0; }
    if (argc < 4) { fprintf(stderr, "rmmc_tutorial_op: %s needs a ledger\n", verb); return 2; }
    const char *ledger = argv[3];
    if (!strcmp(verb, "start")) { const char *lesson = argc > 4 ? argv[4] : "lesson1"; if (load_lesson(book, lesson)) { fprintf(stderr, "rmmc_tutorial_op: no lesson %s in %s/tutorial.pdl\n", lesson, book); return 2; }
        struct stat st; long off = stat(ledger, &st) ? 0 : (long)st.st_size; char d[4200]; snprintf(d, sizeof d, "%.4000s/play", book); mkdir(d, 0755);
        FILE *f = fopen(pp, "w"); if (!f) return 2; fprintf(f, "lesson=%s\n", lesson);
        if (!stat(ledger, &st) && S_ISDIR(st.st_mode)) { DIR *dr = opendir(ledger); struct dirent *de; while (dr && (de = readdir(dr))) { size_t nl = strlen(de->d_name); if (strncmp(de->d_name, "ledger", 6) || nl < 10 || strcmp(de->d_name + nl - 4, ".txt")) continue; char lp[4400]; snprintf(lp, sizeof lp, "%.4000s/%s", ledger, de->d_name); struct stat ls; fprintf(f, "offset_%s=%ld\n", de->d_name, stat(lp, &ls) ? 0L : (long)ls.st_size); } if (dr) closedir(dr); }
        else fprintf(f, "offset=%ld\n", off);
        fclose(f); printf("started %s\n", lesson); return 0; }
    if (!strcmp(verb, "status")) { const char *actor = argc > 4 ? argv[4] : "asa"; char lesson[64]; long off; if (!read_pref(book, lesson, sizeof lesson, &off, ledger)) { puts("off"); return 0; }
        if (load_lesson(book, lesson)) { puts("off"); return 0; } int have; int cur = progress(book, ledger, actor, off, &have);
        if (cur >= nst) { printf("done=%d/%d lesson=%s\n", nst, nst, lesson); return 0; }
        printf("step=%d/%d id=%s short=%s have=%d need=%d goal=%s\n", cur + 1, nst, ST[cur].id, ST[cur].shortt, have, ST[cur].need, ST[cur].goal); return 0; }
    fprintf(stderr, "rmmc_tutorial_op: unknown verb %s\n", verb); return 2; }
