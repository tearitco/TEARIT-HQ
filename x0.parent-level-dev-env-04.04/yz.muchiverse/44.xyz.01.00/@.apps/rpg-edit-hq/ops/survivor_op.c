#define _GNU_SOURCE
/* survivor_op - step 1 of the early-survivor slice (16.game/EARLY-SURVIVOR-AND-COMMON-EVENTS.md "Order to build" 1), QUARANTINED in the editor's own book (2026-11-04): smash furniture into items, needs that tick, eat, drink, wield, light a fire.
 * Runs only in a folder that holds QUARANTINE (same rule as item_ledger_op; exit 4 otherwise) and writes items ONLY through item_ledger_op (mint from source `smash`, burn with a reason): it never edits a count itself.
 *   survivor_op <book_dir> place <furniture_id> <x> <y>        put a furniture piece on a cell (hp from furniture.pdl)
 *   survivor_op <book_dir> smash <actor> <x> <y>               hp down by 1; at 0 the piece is consumed and EXACTLY its drops are minted to the actor
 *   survivor_op <book_dir> tick <actor> <minutes>              needs rise by needs.pdl rates (hunger, thirst, fatigue), clamped 0..100
 *   survivor_op <book_dir> eat|drink <actor> <item>            burns 1 of the item (reason eaten/drunk), lowers the need named in itemdata.pdl; refused if the actor holds none
 *   survivor_op <book_dir> wield <actor> <item>                the one held slot; refused unless the actor holds the item
 *   survivor_op <book_dir> light <actor> <x> <y>               burns the inputs of the LIGHT row (e.g. 1 rag + 1 plank), writes a FIRE row; refused with NOTHING burned when an input is missing
 *   survivor_op <book_dir> read <actor> <book_item>            step 2: spends the book's minutes (needs rise) and sets the CAP of its skill; the level is unchanged; the book is not consumed
 *   survivor_op <book_dir> practice <actor> <skill> [n]        step 2: xp up; a level is gained only while level < cap (xp keeps counting at the cap)
 *   survivor_op <book_dir> recipes <actor>                     step 2: the recipes this actor can see (skill level AND cap both clear the recipe's level); the others stay hidden
 *   survivor_op <book_dir> craft <actor> <recipe>              step 2: checks skill, tool (held, not used up), inputs; then burns the inputs (reason craft:<id>), mints the output (source craft), spends the minutes, practises the skill
 *   survivor_op <book_dir> status <actor>
 * Step 2 files: books.pdl `BOOK|item|skill|cap|minutes`; recipes.pdl `RECIPE|id|output|count|tool|skill|level|minutes|inputs` (tool - = none); skills.pdl `XPLEVEL|per|<xp per level>`; state/skills.txt (actor|skill|level|cap|xp).
 * Files in <book_dir>: furniture.pdl `FURNITURE|id|hp|item:n,item:n`; needs.pdl `RATE|need|points|per_minutes`; itemdata.pdl `USE|item|eat|hunger|-40` and `LIGHT|item:n,item:n|fuel|20`;
 * state/placed.txt (id|x|y|hp), state/needs.txt (actor|hunger|thirst|fatigue), state/wield.txt (actor|item), state/fire.txt (x|y|fuel), state/events.txt (readable log).
 * Exit: 0 ok, 3 refused (reason on stdout), 2 usage, 4 not quarantined. Env SURVIVOR_LEDGER_OP overrides the ledger op path. COST: one short process per call, no resident part. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>
#include <sys/wait.h>
#include <sys/stat.h>
static char BOOK[PATH_MAX], LOP[PATH_MAX];
static void P(char *o, const char *rel) { snprintf(o, PATH_MAX, "%s/%s", BOOK, rel); }
static long now(void) { const char *e = getenv("ITEM_LEDGER_TS"); return e ? atol(e) : (long)time(NULL); }
/* item_ledger_op <book> a b c d e f (NULL-terminated list); stdout into out; returns its exit code */
static int ledger(char *out, size_t cap, const char *a, const char *b, const char *c, const char *d, const char *e, const char *f) {
    const char *av[10]; int n = 0; av[n++] = LOP; av[n++] = BOOK; const char *x[6] = { a, b, c, d, e, f }; for (int i = 0; i < 6 && x[i]; i++) av[n++] = x[i]; av[n] = NULL;
    int pf[2]; if (pipe(pf) != 0) return -1; pid_t k = fork(); if (k < 0) return -1;
    if (k == 0) { dup2(pf[1], 1); close(pf[0]); execv(LOP, (char *const *)av); _exit(127); }
    close(pf[1]); size_t got = 0; ssize_t r; while (got + 1 < cap && (r = read(pf[0], out + got, cap - 1 - got)) > 0) got += (size_t)r; out[got] = 0; close(pf[0]);
    while (got > 0 && (out[got - 1] == '\n' || out[got - 1] == '\r')) out[--got] = 0; int st = 0; waitpid(k, &st, 0); return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}
static long held(const char *actor, const char *item) {      /* the ledger is the only inventory: ask it */
    char o[4096]; if (ledger(o, sizeof o, "balance", actor, NULL, NULL, NULL, NULL) != 0) return 0; long n = 0;
    for (char *sv = NULL, *l = strtok_r(o, "\n", &sv); l; l = strtok_r(NULL, "\n", &sv)) { char *a = strchr(l, '|'); if (!a) continue; *a++ = 0; char *b = strchr(a, '|'); if (!b) continue; *b++ = 0; if (!strcmp(l, actor) && !strcmp(a, item)) n = atol(b); }
    return n;
}
static void logev(const char *actor, const char *verb, const char *detail) { char p[PATH_MAX]; P(p, "state/events.txt"); FILE *f = fopen(p, "a"); if (f) { fprintf(f, "%ld|%s|%s|%s\n", now(), actor, verb, detail); fclose(f); } }
/* read a .pdl row by key: returns the text after the first | following the key's tag, matching field 1 */
static int pdl_row(const char *file, const char *tag, const char *key, char *rest, size_t cap) {
    char p[PATH_MAX]; P(p, file); FILE *f = fopen(p, "r"); if (!f) return 0; char ln[512];
    while (fgets(ln, sizeof ln, f)) { ln[strcspn(ln, "\r\n")] = 0; if (strncmp(ln, tag, strlen(tag)) || ln[strlen(tag)] != '|') continue; char *a = ln + strlen(tag) + 1; char *b = strchr(a, '|'); if (!b) continue; *b = 0; if (strcmp(a, key)) continue; snprintf(rest, cap, "%s", b + 1); fclose(f); return 1; }
    fclose(f); return 0;
}
static void sanit(char *s) { for (; *s; s++) if (*s == '|' || *s == '\n' || *s == '\r' || *s == ' ') *s = '_'; }
/* generic key/value line files: state/<name> rows "k1|k2|..." ; load all rows */
typedef struct { char f[5][64]; } Row;
static int load(const char *name, Row *rows, int max) { char p[PATH_MAX]; P(p, name); FILE *f = fopen(p, "r"); if (!f) return 0; char ln[400]; int n = 0; while (n < max && fgets(ln, sizeof ln, f)) { ln[strcspn(ln, "\r\n")] = 0; if (!ln[0]) continue; memset(&rows[n], 0, sizeof rows[n]); char *q = ln; int k = 0; while (k < 5) { char *b = strchr(q, '|'); if (b) *b = 0; snprintf(rows[n].f[k++], 64, "%s", q); if (!b) break; q = b + 1; } n++; } fclose(f); return n; }
static void save(const char *name, Row *rows, int n, int cols) { char p[PATH_MAX], t[PATH_MAX + 8]; P(p, name); snprintf(t, sizeof t, "%s.tmp", p); FILE *f = fopen(t, "w"); if (!f) return; for (int i = 0; i < n; i++) { for (int c = 0; c < cols; c++) fprintf(f, c ? "|%s" : "%s", rows[i].f[c]); fputc('\n', f); } fclose(f); rename(t, p); }
static int clamp(long v) { return v < 0 ? 0 : v > 100 ? 100 : (int)v; }
static int need_ix(const char *n) { return !strcmp(n, "hunger") ? 1 : !strcmp(n, "thirst") ? 2 : !strcmp(n, "fatigue") ? 3 : 0; }
static int needs_row(Row *rows, int *n, const char *actor) { for (int i = 0; i < *n; i++) if (!strcmp(rows[i].f[0], actor)) return i; snprintf(rows[*n].f[0], 64, "%s", actor); snprintf(rows[*n].f[1], 64, "0"); snprintf(rows[*n].f[2], 64, "0"); snprintf(rows[*n].f[3], 64, "0"); return (*n)++; }


/* ---- step 2 helpers ---- */
static void apply_tick(const char *actor, long mins) {      /* the same rule as the tick verb, callable from read/craft */
    static Row nr[200]; int n = load("state/needs.txt", nr, 200); int r = needs_row(nr, &n, actor); static const char *nd[3] = { "hunger", "thirst", "fatigue" };
    for (int k = 0; k < 3; k++) { char rr[200]; long pts = 0, per = 60; if (pdl_row("needs.pdl", "RATE", nd[k], rr, sizeof rr)) { pts = atol(rr); char *b = strchr(rr, '|'); if (b) per = atol(b + 1); } if (per <= 0) per = 60; snprintf(nr[r].f[k + 1], 64, "%d", clamp(atol(nr[r].f[k + 1]) + mins * pts / per)); }
    save("state/needs.txt", nr, n, 4);
}
static Row srows[200]; static int nsk;
static int skill_ix(const char *actor, const char *skill, int create) { for (int i = 0; i < nsk; i++) if (!strcmp(srows[i].f[0], actor) && !strcmp(srows[i].f[1], skill)) return i; if (!create || nsk >= 199) return -1; memset(&srows[nsk], 0, sizeof srows[nsk]); snprintf(srows[nsk].f[0], 64, "%s", actor); snprintf(srows[nsk].f[1], 64, "%s", skill); snprintf(srows[nsk].f[2], 64, "0"); snprintf(srows[nsk].f[3], 64, "0"); snprintf(srows[nsk].f[4], 64, "0"); return nsk++; }
static void practise(const char *actor, const char *skill, long pts) {
    nsk = load("state/skills.txt", srows, 200); int i = skill_ix(actor, skill, 1); if (i < 0) return; char rr[64]; long per = 3; if (pdl_row("skills.pdl", "XPLEVEL", "per", rr, sizeof rr)) per = atol(rr); if (per < 1) per = 3;
    long lvl = atol(srows[i].f[2]), cap = atol(srows[i].f[3]), xp = atol(srows[i].f[4]) + pts; while (lvl < cap && xp >= per) { xp -= per; lvl++; }      /* a level only while level < cap; xp keeps counting at the cap */
    snprintf(srows[i].f[2], 64, "%ld", lvl); snprintf(srows[i].f[4], 64, "%ld", xp); save("state/skills.txt", srows, nsk, 5);
}
static int recipe_visible(const char *actor, const char *skill, long need) {      /* hidden until the skill's LEVEL and CAP both clear the recipe */
    if (!strcmp(skill, "-") || need <= 0) return 1; nsk = load("state/skills.txt", srows, 200); int i = skill_ix(actor, skill, 0); if (i < 0) return 0; return atol(srows[i].f[2]) >= need && atol(srows[i].f[3]) >= need;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: survivor_op <book_dir> place|smash|tick|eat|drink|wield|light|status ...\n"); return 2; }
    snprintf(BOOK, sizeof BOOK, "%s", argv[1]); char q[PATH_MAX]; P(q, "QUARANTINE"); if (access(q, F_OK) != 0) { printf("NOT A QUARANTINED BOOK: %s has no QUARANTINE file\n", BOOK); return 4; }
    { const char *e = getenv("SURVIVOR_LEDGER_OP"); if (e && e[0]) snprintf(LOP, sizeof LOP, "%s", e); else { char self[PATH_MAX]; ssize_t n = readlink("/proc/self/exe", self, sizeof self - 1); if (n < 0) n = 0; self[n] = 0; char *s = strrchr(self, '/'); if (s) *s = 0; snprintf(LOP, sizeof LOP, "%s/item_ledger_op.+x", self); } }
    { char d[PATH_MAX]; P(d, "state"); mkdir(d, 0755); }
    const char *v = argv[2]; static Row rows[200]; char out[4096], rest[512];
    if (!strcmp(v, "place") && argc == 6) { char id[64]; snprintf(id, 64, "%s", argv[3]); sanit(id); if (!pdl_row("furniture.pdl", "FURNITURE", id, rest, sizeof rest)) { printf("REFUSED: no furniture '%s' in furniture.pdl\n", id); return 3; }
        int n = load("state/placed.txt", rows, 200); for (int i = 0; i < n; i++) if (!strcmp(rows[i].f[1], argv[4]) && !strcmp(rows[i].f[2], argv[5])) { printf("REFUSED: cell %s,%s already holds %s\n", argv[4], argv[5], rows[i].f[0]); return 3; }
        if (n >= 199) { printf("REFUSED: too many pieces\n"); return 3; } snprintf(rows[n].f[0], 64, "%s", id); snprintf(rows[n].f[1], 64, "%s", argv[4]); snprintf(rows[n].f[2], 64, "%s", argv[5]); snprintf(rows[n].f[3], 64, "%d", atoi(rest)); save("state/placed.txt", rows, n + 1, 4); logev("-", "place", id); printf("PLACED %s at %s,%s hp %d\n", id, argv[4], argv[5], atoi(rest)); return 0; }
    if (!strcmp(v, "smash") && argc == 6) { char actor[64]; snprintf(actor, 64, "%s", argv[3]); sanit(actor); int n = load("state/placed.txt", rows, 200), at = -1; for (int i = 0; i < n; i++) if (!strcmp(rows[i].f[1], argv[4]) && !strcmp(rows[i].f[2], argv[5])) at = i;
        if (at < 0) { printf("REFUSED: nothing to smash at %s,%s\n", argv[4], argv[5]); return 3; } int hp = atoi(rows[at].f[3]) - 1; char fid[64]; snprintf(fid, 64, "%s", rows[at].f[0]);
        if (hp > 0) { snprintf(rows[at].f[3], 64, "%d", hp); save("state/placed.txt", rows, n, 4); logev(actor, "smash", fid); printf("HIT %s hp %d\n", fid, hp); return 0; }
        if (!pdl_row("furniture.pdl", "FURNITURE", fid, rest, sizeof rest)) { printf("REFUSED: %s lost its furniture.pdl row\n", fid); return 3; } char *drops = strchr(rest, '|'); drops = drops ? drops + 1 : rest;
        char dcopy[400]; snprintf(dcopy, sizeof dcopy, "%s", drops); { char *sv = NULL; for (char *t = strtok_r(dcopy, ",", &sv); t; t = strtok_r(NULL, ",", &sv)) { char *c = strchr(t, ':'); if (!c) { printf("REFUSED: bad drop '%s'\n", t); return 3; } *c = 0; if (ledger(out, sizeof out, "total", t, NULL, NULL, NULL, NULL) != 0) { printf("REFUSED: drop item %s is not defined in the ledger (nothing changed)\n", t); return 3; } } }      /* validate every drop first: all or nothing */
        for (int i = at; i + 1 < n; i++) rows[i] = rows[i + 1]; save("state/placed.txt", rows, n - 1, 4);      /* the piece is consumed once, before its drops are minted */
        snprintf(dcopy, sizeof dcopy, "%s", drops); char summary[300] = ""; { char *sv = NULL; for (char *t = strtok_r(dcopy, ",", &sv); t; t = strtok_r(NULL, ",", &sv)) { char *c = strchr(t, ':'); *c = 0; if (ledger(out, sizeof out, "mint", actor, t, c + 1, "smash", NULL) != 0) { printf("ERROR: mint of %s failed: %s\n", t, out); return 3; } strncat(summary, t, sizeof summary - strlen(summary) - 1); strncat(summary, ":", sizeof summary - strlen(summary) - 1); strncat(summary, c + 1, sizeof summary - strlen(summary) - 1); strncat(summary, " ", sizeof summary - strlen(summary) - 1); } }
        logev(actor, "smash", summary); printf("SMASHED %s: %s\n", fid, summary); return 0; }
    if (!strcmp(v, "tick") && argc == 5) { char actor[64]; snprintf(actor, 64, "%s", argv[3]); sanit(actor); long mins = atol(argv[4]); if (mins <= 0 || mins > 100000) { printf("REFUSED: minutes must be 1..100000\n"); return 3; }
        int n = load("state/needs.txt", rows, 200); int r = needs_row(rows, &n, actor); static const char *nd[3] = { "hunger", "thirst", "fatigue" };
        for (int k = 0; k < 3; k++) { char rr[200]; long pts = 0, per = 60; if (pdl_row("needs.pdl", "RATE", nd[k], rr, sizeof rr)) { pts = atol(rr); char *b = strchr(rr, '|'); if (b) per = atol(b + 1); } if (per <= 0) per = 60; long add = mins * pts / per; snprintf(rows[r].f[k + 1], 64, "%d", clamp(atol(rows[r].f[k + 1]) + add)); }
        save("state/needs.txt", rows, n, 4); logev(actor, "tick", argv[4]); printf("NEEDS %s hunger=%s thirst=%s fatigue=%s\n", actor, rows[r].f[1], rows[r].f[2], rows[r].f[3]); return 0; }
    if ((!strcmp(v, "eat") || !strcmp(v, "drink")) && argc == 5) { char actor[64], item[64]; snprintf(actor, 64, "%s", argv[3]); sanit(actor); snprintf(item, 64, "%s", argv[4]); sanit(item);
        if (!pdl_row("itemdata.pdl", "USE", item, rest, sizeof rest)) { printf("REFUSED: %s has no USE row in itemdata.pdl\n", item); return 3; } char verb[16], need[16]; long delta = 0; if (sscanf(rest, "%15[^|]|%15[^|]|%ld", verb, need, &delta) != 3 || strcmp(verb, v)) { printf("REFUSED: %s cannot be used to %s\n", item, v); return 3; }
        if (held(actor, item) < 1) { printf("REFUSED: %s holds no %s\n", actor, item); return 3; } if (ledger(out, sizeof out, "burn", actor, item, "1", !strcmp(v, "eat") ? "eaten" : "drunk", NULL) != 0) { printf("REFUSED: %s\n", out); return 3; }
        int n = load("state/needs.txt", rows, 200); int r = needs_row(rows, &n, actor); int ix = need_ix(need); if (!ix) { printf("REFUSED: unknown need %s\n", need); return 3; } snprintf(rows[r].f[ix], 64, "%d", clamp(atol(rows[r].f[ix]) + delta)); save("state/needs.txt", rows, n, 4); logev(actor, v, item); printf("%s %s: %s now %s\n", !strcmp(v, "eat") ? "ATE" : "DRANK", item, need, rows[r].f[ix]); return 0; }
    if (!strcmp(v, "wield") && argc == 5) { char actor[64], item[64]; snprintf(actor, 64, "%s", argv[3]); sanit(actor); snprintf(item, 64, "%s", argv[4]); sanit(item); if (held(actor, item) < 1) { printf("REFUSED: %s holds no %s\n", actor, item); return 3; }
        int n = load("state/wield.txt", rows, 200), at = -1; for (int i = 0; i < n; i++) if (!strcmp(rows[i].f[0], actor)) at = i; if (at < 0) { at = n++; snprintf(rows[at].f[0], 64, "%s", actor); } snprintf(rows[at].f[1], 64, "%s", item); save("state/wield.txt", rows, n, 2); logev(actor, "wield", item); printf("WIELDING %s\n", item); return 0; }
    if (!strcmp(v, "light") && argc == 6) { char actor[64]; snprintf(actor, 64, "%s", argv[3]); sanit(actor); char lr[400] = ""; { char p[PATH_MAX]; P(p, "itemdata.pdl"); FILE *f = fopen(p, "r"); if (f) { char ln[400]; while (fgets(ln, sizeof ln, f)) if (!strncmp(ln, "LIGHT|", 6)) { ln[strcspn(ln, "\r\n")] = 0; snprintf(lr, sizeof lr, "%s", ln + 6); break; } fclose(f); } } if (!lr[0]) { printf("REFUSED: no LIGHT row in itemdata.pdl\n"); return 3; }
        char *inputs = lr, *r2 = strchr(lr, '|'); if (!r2) { printf("REFUSED: bad LIGHT row\n"); return 3; } *r2++ = 0; char *r3 = strchr(r2, '|'); long fuel = r3 ? atol(r3 + 1) : 10;
        char c1[400]; snprintf(c1, sizeof c1, "%s", inputs); { char *sv = NULL; for (char *t = strtok_r(c1, ",", &sv); t; t = strtok_r(NULL, ",", &sv)) { char *c = strchr(t, ':'); if (!c) continue; *c = 0; if (held(actor, t) < atol(c + 1)) { printf("REFUSED: %s needs %s:%s for a fire (nothing burned)\n", actor, t, c + 1); return 3; } } }      /* check every input before burning any */
        char c2[400]; snprintf(c2, sizeof c2, "%s", inputs); { char *sv = NULL; for (char *t = strtok_r(c2, ",", &sv); t; t = strtok_r(NULL, ",", &sv)) { char *c = strchr(t, ':'); if (!c) continue; *c = 0; if (ledger(out, sizeof out, "burn", actor, t, c + 1, "fire", NULL) != 0) { printf("ERROR: burn of %s failed: %s\n", t, out); return 3; } } }
        int n = load("state/fire.txt", rows, 200); snprintf(rows[n].f[0], 64, "%s", argv[4]); snprintf(rows[n].f[1], 64, "%s", argv[5]); snprintf(rows[n].f[2], 64, "%ld", fuel); save("state/fire.txt", rows, n + 1, 3); logev(actor, "light", inputs); printf("FIRE at %s,%s fuel %ld (burned %s)\n", argv[4], argv[5], fuel, inputs); return 0; }
    if (!strcmp(v, "read") && argc == 5) { char actor[64], item[64]; snprintf(actor, 64, "%s", argv[3]); sanit(actor); snprintf(item, 64, "%s", argv[4]); sanit(item);
        if (!pdl_row("books.pdl", "BOOK", item, rest, sizeof rest)) { printf("REFUSED: %s is not a book (no BOOK row)\n", item); return 3; } char skill[48]; long cap = 0, mins = 0; if (sscanf(rest, "%47[^|]|%ld|%ld", skill, &cap, &mins) != 3) { printf("REFUSED: bad BOOK row\n"); return 3; }
        if (held(actor, item) < 1) { printf("REFUSED: %s holds no %s\n", actor, item); return 3; }
        nsk = load("state/skills.txt", srows, 200); int i = skill_ix(actor, skill, 1); if (atol(srows[i].f[3]) < cap) snprintf(srows[i].f[3], 64, "%ld", cap); save("state/skills.txt", srows, nsk, 5); apply_tick(actor, mins); logev(actor, "read", item);
        printf("READ %s: %s cap %s (level %s unchanged, %ld minutes)\n", item, skill, srows[i].f[3], srows[i].f[2], mins); return 0; }
    if (!strcmp(v, "practice") && (argc == 5 || argc == 6)) { char actor[64]; snprintf(actor, 64, "%s", argv[3]); sanit(actor); long pts = argc == 6 ? atol(argv[5]) : 1; if (pts < 1 || pts > 1000) { printf("REFUSED: points must be 1..1000\n"); return 3; }
        practise(actor, argv[4], pts); nsk = load("state/skills.txt", srows, 200); int i = skill_ix(actor, argv[4], 0); logev(actor, "practice", argv[4]); printf("PRACTISED %s: level %s cap %s xp %s\n", argv[4], srows[i].f[2], srows[i].f[3], srows[i].f[4]); return 0; }
    if (!strcmp(v, "recipes") && argc == 4) { char actor[64]; snprintf(actor, 64, "%s", argv[3]); sanit(actor); char p[PATH_MAX]; P(p, "recipes.pdl"); FILE *f = fopen(p, "r"); int shown = 0; if (f) { char ln[500]; while (fgets(ln, sizeof ln, f)) { if (strncmp(ln, "RECIPE|", 7)) continue; ln[strcspn(ln, "\r\n")] = 0; char id[48], out2[48], tool[48], sk[48], in[300]; long cnt = 0, lv = 0, mn = 0; if (sscanf(ln + 7, "%47[^|]|%47[^|]|%ld|%47[^|]|%47[^|]|%ld|%ld|%299s", id, out2, &cnt, tool, sk, &lv, &mn, in) != 8) continue; if (recipe_visible(actor, sk, lv)) { printf("RECIPE %s -> %s x%ld (%ld min)\n", id, out2, cnt, mn); shown++; } } fclose(f); } if (!shown) printf("NO RECIPES VISIBLE\n"); return 0; }
    if (!strcmp(v, "craft") && argc == 5) { char actor[64], rid[64]; snprintf(actor, 64, "%s", argv[3]); sanit(actor); snprintf(rid, 64, "%s", argv[4]); sanit(rid);
        if (!pdl_row("recipes.pdl", "RECIPE", rid, rest, sizeof rest)) { printf("REFUSED: no recipe '%s'\n", rid); return 3; } char o2[48], tool[48], sk[48], in[300]; long cnt = 0, lv = 0, mn = 0; if (sscanf(rest, "%47[^|]|%ld|%47[^|]|%47[^|]|%ld|%ld|%299s", o2, &cnt, tool, sk, &lv, &mn, in) != 7 || cnt < 1) { printf("REFUSED: bad RECIPE row\n"); return 3; }
        if (!recipe_visible(actor, sk, lv)) { printf("REFUSED: recipe '%s' is hidden: %s needs level and cap %ld (nothing changed)\n", rid, sk, lv); return 3; }
        if (strcmp(tool, "-") && held(actor, tool) < 1) { printf("REFUSED: %s needs the tool %s in hand (nothing changed)\n", rid, tool); return 3; }
        if (ledger(out, sizeof out, "total", o2, NULL, NULL, NULL, NULL) != 0) { printf("REFUSED: output item %s is not defined (nothing changed)\n", o2); return 3; }
        char c1[300]; snprintf(c1, sizeof c1, "%s", in); { char *sv = NULL; for (char *t = strtok_r(c1, ",", &sv); t; t = strtok_r(NULL, ",", &sv)) { char *c = strchr(t, ':'); if (!c) continue; *c = 0; if (held(actor, t) < atol(c + 1)) { printf("REFUSED: %s needs %s:%s (nothing burned)\n", rid, t, c + 1); return 3; } } }
        char c2[300], reason[80]; snprintf(c2, sizeof c2, "%s", in); snprintf(reason, sizeof reason, "craft:%s", rid); { char *sv = NULL; for (char *t = strtok_r(c2, ",", &sv); t; t = strtok_r(NULL, ",", &sv)) { char *c = strchr(t, ':'); if (!c) continue; *c = 0; if (ledger(out, sizeof out, "burn", actor, t, c + 1, reason, NULL) != 0) { printf("ERROR: burn of %s failed: %s\n", t, out); return 3; } } }
        char cn[24]; snprintf(cn, sizeof cn, "%ld", cnt); if (ledger(out, sizeof out, "mint", actor, o2, cn, "craft", NULL) != 0) { printf("ERROR: mint of %s failed: %s\n", o2, out); return 3; }
        apply_tick(actor, mn); if (strcmp(sk, "-")) practise(actor, sk, 1); logev(actor, "craft", rid); printf("CRAFTED %s x%ld (%ld min, burned %s)\n", o2, cnt, mn, in); return 0; }
    if (!strcmp(v, "status") && argc == 4) { int n = load("state/needs.txt", rows, 200); for (int i = 0; i < n; i++) if (!strcmp(rows[i].f[0], argv[3])) { printf("NEEDS %s hunger=%s thirst=%s fatigue=%s\n", rows[i].f[0], rows[i].f[1], rows[i].f[2], rows[i].f[3]); return 0; } printf("NEEDS %s hunger=0 thirst=0 fatigue=0\n", argv[3]); return 0; }
    fprintf(stderr, "usage: survivor_op <book_dir> place|smash|tick|eat|drink|wield|light|read|practice|recipes|craft|status ...\n"); return 2;
}
