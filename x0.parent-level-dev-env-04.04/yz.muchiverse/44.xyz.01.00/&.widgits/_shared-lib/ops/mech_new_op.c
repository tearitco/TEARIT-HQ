/* mech_new_op - generate a MECHANIC from args (XO nov-22 doc 4, section 6 row 2; the missing sibling of game_new_op): the verb row, the event page skeleton, the RULE rows, a phase-table case that can FAIL and a concept-bank proposal row.
 * Usage: mech_new_op <house> <app> <mechanic> --kind <kind> [--kinds <mech_kinds.pdl>]      (kinds file default: <house>/&.widgits/concept-bank/mech_kinds.pdl; the kinds are DATA there: dig mine craft place plant sleep build-job today)
 *        mech_new_op --describe
 * Needs the app: <house>/@.apps/<app>/verbs.pdl must exist (the verb row is appended). Writes, all or nothing (every file is built in memory and checked first):
 *   <app>/verbs.pdl                     + `VERB | <mechanic> | yes | ui | <args> | <sentence>`
 *   <app>/events/<mechanic>/event_pkg/pages/page_1/condition.pdl, event.ir.pdl      the skeleton (shape of mcl-hq events/mine; `verbgen gen` makes event.pal and cmd_1.sh from the verb row)
 *   <app>/mechanics.pdl                 + MECH / COST / GAIN / LIMIT rows and the RULE rows (`RULE | <mechanic>_<suffix> | N`, `RULE | start_<resource> | N` once): numbers are data, mech_step_op carries none
 *   <house>/&.widgits/_shared-lib/harness/play/phases/<app>_<mechanic>.phases.pdl   the case: GAME <app> mech_step with the shared op source as its NEEDS row; phases are computed by replaying the rule over the RULE numbers
 *   <house>/&.widgits/concept-bank/proposals/mech_proposals.txt   + `PROPOSE | kind=mechanic | id=<app>.<mechanic> | status=candidate | body=...` (promotion stays MANUAL: a person reads it)
 * REFUSES (exit 2, nothing written): an existing mechanic name (verb row, MECH row, events folder, phase table or proposal id), a bad name, an unknown kind, a missing app, any `{{` left after the kind's text was filled in, a kind with no EFFECT rows.
 * Output: `MECH_NEW|<app>|<mechanic>|<kind>|files=5`. Exit 0 ok, 2 usage / refusal. No network, no shell. */
#define _GNU_SOURCE
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../tool_rules.c"
#define P 4096
#define MAXR 64
typedef struct { char type[16], res[64], suffix[64]; double def; } Ef;
static void mkdirs(const char *path) { char t[P]; snprintf(t, sizeof t, "%s", path); for (char *p = t + 1; *p; p++) if (*p == '/') { *p = 0; mkdir(t, 0755); *p = '/'; } mkdir(t, 0755); }
static int exists(const char *p) { return access(p, F_OK) == 0; }
static int has_text(const char *file, const char *needle) { FILE *f = fopen(file, "r"); if (!f) return 0; char ln[4096]; int hit = 0; while (fgets(ln, sizeof ln, f)) if (strstr(ln, needle)) hit = 1; fclose(f); return hit; }
static void fill(char *dst, size_t n, const char *src, const char *mech, const char *app) {   /* {{mech}} {{app}} */
    size_t o = 0; for (const char *p = src; *p && o + 1 < n; ) { if (!strncmp(p, "{{mech}}", 8)) { o += (size_t)snprintf(dst + o, n - o, "%s", mech); p += 8; } else if (!strncmp(p, "{{app}}", 7)) { o += (size_t)snprintf(dst + o, n - o, "%s", app); p += 7; } else dst[o++] = *p++; } dst[o] = 0;
}
static int bad_name(const char *s) { if (!*s || strlen(s) > 40) return 1; for (; *s; s++) if (!islower((unsigned char)*s) && !isdigit((unsigned char)*s) && *s != '_' && *s != '-') return 1; return 0; }
static int refuse(const char *why) { fprintf(stderr, "mech_new_op: refused: %s\n", why); return 2; }
int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--describe")) { puts("mech_new_op: IN house app mechanic --kind K [--kinds file] (kinds are KIND/EFFECT/START rows in concept-bank/mech_kinds.pdl) | OUT verbs.pdl row, events/<m>/event_pkg/pages/page_1/{condition,event.ir}.pdl, mechanics.pdl MECH/COST/GAIN/LIMIT + RULE rows, play/phases/<app>_<m>.phases.pdl (replays the rule, can fail), a proposals/mech_proposals.txt row | refuses an existing name, bad kind, unreplaced {{ | exit 0 ok, 2 refusal"); return 0; }
    if (argc < 6) { fprintf(stderr, "usage: mech_new_op <house> <app> <mechanic> --kind K [--kinds F]   (or --describe)\n"); return 2; }
    const char *house = argv[1], *app = argv[2], *mech = argv[3], *kind = NULL; char kinds[P]; snprintf(kinds, sizeof kinds, "%s/&.widgits/concept-bank/mech_kinds.pdl", house);
    for (int i = 4; i + 1 < argc; i += 2) { if (!strcmp(argv[i], "--kind")) kind = argv[i + 1]; else if (!strcmp(argv[i], "--kinds")) snprintf(kinds, sizeof kinds, "%s", argv[i + 1]); else return refuse("unknown option"); }
    if (!kind) return refuse("--kind is required");
    if (bad_name(app) || bad_name(mech)) return refuse("app and mechanic names are lowercase letters, digits, _ and - (at most 40)");
    char appd[P], vp[P]; snprintf(appd, sizeof appd, "%s/@.apps/%s", house, app); snprintf(vp, sizeof vp, "%s/verbs.pdl", appd);
    if (!exists(vp)) return refuse("the app has no verbs.pdl (create the app first)");
    /* the kind */
    char args[128] = "", sentence[512] = "", kw[256] = ""; int found = 0; Ef ef[MAXR]; int nef = 0; struct { char res[64]; double v; } st[MAXR]; int nst = 0;
    FILE *f = fopen(kinds, "r"); if (!f) return refuse("cannot read the kinds file"); char ln[4096];
    while (fgets(ln, sizeof ln, f)) { char *c[7]; int k = 0; char *s = ln; while (k < 7) { c[k++] = s; char *b = strchr(s, '|'); if (!b) break; *b = 0; s = b + 1; } for (int i = 0; i < k; i++) c[i] = tr_trim(c[i]); if (k < 3 || c[0][0] == '#' || strcmp(c[1], kind)) continue;
        if (!strcmp(c[0], "KIND") && k >= 5) { found = 1; snprintf(args, sizeof args, "%s", c[2]); snprintf(sentence, sizeof sentence, "%s", c[3]); snprintf(kw, sizeof kw, "%s", c[4]); }
        else if (!strcmp(c[0], "EFFECT") && k >= 6 && nef < MAXR) { snprintf(ef[nef].type, 16, "%s", c[2]); snprintf(ef[nef].res, 64, "%s", c[3]); snprintf(ef[nef].suffix, 64, "%s", c[4]); ef[nef].def = atof(c[5]); nef++; }
        else if (!strcmp(c[0], "START") && k >= 4 && nst < MAXR) { snprintf(st[nst].res, 64, "%s", c[2]); st[nst].v = atof(c[3]); nst++; } }
    fclose(f);
    if (!found) return refuse("unknown kind (not a KIND row in the kinds file)");
    if (!nef) return refuse("the kind has no EFFECT rows");
    /* existing names */
    char evd[P], phd[P], mp[P], pp[P], idrow[256]; snprintf(evd, sizeof evd, "%s/events/%s", appd, mech); snprintf(phd, sizeof phd, "%s/&.widgits/_shared-lib/harness/play/phases/%s_%s.phases.pdl", house, app, mech);
    snprintf(mp, sizeof mp, "%s/mechanics.pdl", appd); snprintf(pp, sizeof pp, "%s/&.widgits/concept-bank/proposals/mech_proposals.txt", house); snprintf(idrow, sizeof idrow, "id=%s.%s ", app, mech);
    char vrow[256]; snprintf(vrow, sizeof vrow, "VERB | %s ", mech); char mrow[256]; snprintf(mrow, sizeof mrow, "MECH | %s |", mech);
    if (has_text(vp, vrow)) return refuse("the app already has a verb row with that name");
    if (has_text(mp, mrow)) return refuse("mechanics.pdl already has that mechanic");
    if (exists(evd)) return refuse("an events folder with that name exists");
    if (exists(phd)) return refuse("a phase table with that name exists");
    if (has_text(pp, idrow)) return refuse("a proposal with that id exists");
    /* build everything in memory */
    char sent[512]; fill(sent, sizeof sent, sentence, mech, app); char appus[64]; snprintf(appus, sizeof appus, "%s", app); for (char *p = appus; *p; p++) if (*p == '-') *p = '_';
    static char verbline[1024], cond[512], ir[1024], mechs[8192], phases[16384], prop[2048]; char rulen[MAXR][64]; (void)rulen;
    snprintf(verbline, sizeof verbline, "VERB | %-9s | yes    | ui     | %s | %s\n", mech, args, sent);
    snprintf(cond, sizeof cond, "SECTION      | KEY                | VALUE\n----------------------------------------\nMETA         | piece_id           | %s\nCOND | trigger | ui\n", mech);
    snprintf(ir, sizeof ir, "SECTION      | KEY                | VALUE\n----------------------------------------\nMETA         | piece_id           | %s\nMETA         | generated          | mech_new_op\nSTATE        | source               | events-hq\nNODE         | id=1 type=%s_verb | verb=%s|arg=\n", mech, appus, mech);
    size_t o = 0; o += (size_t)snprintf(mechs + o, sizeof mechs - o, "MECH | %s | %s\n", mech, args);
    double startv[MAXR]; char startr[MAXR][64]; int nsr = 0;   /* stocks for the replay */
    for (int i = 0; i < nef; i++) { const char *T = !strcmp(ef[i].type, "cost") ? "COST" : !strcmp(ef[i].type, "gain") ? "GAIN" : !strcmp(ef[i].type, "limit") ? "LIMIT" : NULL; if (!T) return refuse("an EFFECT row has a type other than cost, gain, limit");
        o += (size_t)snprintf(mechs + o, sizeof mechs - o, "%s | %s | %s | %s_%s\nRULE | %s_%s | %g\n", T, mech, ef[i].res, mech, ef[i].suffix, mech, ef[i].suffix, ef[i].def); (void)T; }
    /* start rules: once per app (a resource another mechanic already started is left alone) */
    for (int i = 0; i < nst; i++) { char key[96]; snprintf(key, sizeof key, "| start_%s |", st[i].res); if (!has_text(mp, key)) o += (size_t)snprintf(mechs + o, sizeof mechs - o, "RULE | start_%s | %g\n", st[i].res, st[i].v);
        snprintf(startr[nsr], 64, "%s", st[i].res); startv[nsr] = st[i].v; if (has_text(mp, key)) { char kk[96]; snprintf(kk, sizeof kk, "start_%s", st[i].res); tr_load(mp); const char *ev = tr_get(kk); if (ev) startv[nsr] = atof(ev); } nsr++; }   /* an earlier mechanic already fixed this stock: the replay uses it */
    /* the existing start stocks of resources other mechanics declared are read from mechanics.pdl for the replay */
    for (int i = 0; i < nef; i++) { int have = 0; for (int k = 0; k < nsr; k++) if (!strcmp(startr[k], ef[i].res)) have = 1; if (!have && nsr < MAXR) { char key[96]; snprintf(key, sizeof key, "start_%s", ef[i].res); tr_load(mp); const char *v = tr_get(key); snprintf(startr[nsr], 64, "%s", ef[i].res); startv[nsr] = v ? atof(v) : 0; nsr++; } }
    /* replay the rule over the numbers just chosen: how many steps succeed, and what each prints (the case pins them, and a wrong op fails them) */
    double stock[MAXR]; for (int k = 0; k < nsr; k++) stock[k] = startv[k];
    #define RES(name) ({ int _r = -1; for (int _k = 0; _k < nsr; _k++) if (!strcmp(startr[_k], name)) _r = _k; _r; })
    char argv_fill[256] = ""; { int n = 0; if (strcmp(args, "-")) { char t[128]; snprintf(t, sizeof t, "%s", args); for (char *w = strtok(t, " "); w; w = strtok(NULL, " ")) { size_t q = strlen(argv_fill); snprintf(argv_fill + q, sizeof argv_fill - q, " | %d", ++n); } } }
    size_t po = 0; po += (size_t)snprintf(phases + po, sizeof phases - po, "# %s_%s.phases.pdl - GENERATED by mech_new_op (kind %s) for the mechanic %s of %s. The phase replays the rule over the RULE rows of mechanics.pdl; edit the numbers there, then regenerate the expectations.\nGAME | %s | mech_step\nNEEDS | &.widgits/_shared-lib/ops/mech_step_op.c | @.apps/%s/ops/mech_step.c\nNEEDS | &.widgits/_shared-lib/tool_rules.c | @.apps/%s/tool_rules.c\n\nPHASE | p1_%s | %s: use it until the rule refuses | %s\n", app, mech, kind, mech, app, app, app, app, mech, mech, kw);
    int steps = 0; char lastmsg[256] = "";
    for (; steps < 40; steps++) {
        int ok = 1; for (int i = 0; i < nef; i++) if (!strcmp(ef[i].type, "cost")) { int r = RES(ef[i].res); if (r < 0 || stock[r] < ef[i].def) { snprintf(lastmsg, sizeof lastmsg, "%s: refused: need %g %s, have %g", mech, ef[i].def, ef[i].res, r < 0 ? 0 : stock[r]); ok = 0; } }
        if (ok) for (int i = 0; i < nef; i++) if (!strcmp(ef[i].type, "limit")) for (int g = 0; g < nef; g++) if (!strcmp(ef[g].type, "gain") && !strcmp(ef[g].res, ef[i].res)) { int r = RES(ef[i].res); if (r >= 0 && stock[r] + ef[g].def > ef[i].def) { snprintf(lastmsg, sizeof lastmsg, "%s: refused: %s is full (%g)", mech, ef[i].res, ef[i].def); ok = 0; } }
        po += (size_t)snprintf(phases + po, sizeof phases - po, "DO | %s%s\n", mech, argv_fill);
        if (!ok) { po += (size_t)snprintf(phases + po, sizeof phases - po, "SAYIS | %s | step-%d-is-refused-by-the-rule\n", lastmsg, steps + 1); break; }
        char shown[512] = ""; for (int i = 0; i < nef; i++) if (!strcmp(ef[i].type, "cost")) { int r = RES(ef[i].res); stock[r] -= ef[i].def; }
        for (int i = 0; i < nef; i++) if (!strcmp(ef[i].type, "gain")) { int r = RES(ef[i].res); if (r < 0 && nsr < MAXR) { snprintf(startr[nsr], 64, "%s", ef[i].res); stock[nsr] = 0; r = nsr++; } stock[r] += ef[i].def; }
        for (int i = 0; i < nef; i++) if (!strcmp(ef[i].type, "cost")) { size_t q = strlen(shown); snprintf(shown + q, sizeof shown - q, " %s=%g", ef[i].res, stock[RES(ef[i].res)]); }
        for (int i = 0; i < nef; i++) if (!strcmp(ef[i].type, "gain")) { size_t q = strlen(shown); snprintf(shown + q, sizeof shown - q, " %s=%g", ef[i].res, stock[RES(ef[i].res)]); }
        po += (size_t)snprintf(phases + po, sizeof phases - po, "SAYIS | %s: ok%s | step-%d-succeeds\n", mech, shown, steps + 1);
    }
    if (steps >= 40) return refuse("the rule never refuses within 40 steps (give the kind a cost or a limit)");
    po += (size_t)snprintf(phases + po, sizeof phases - po, "COUNT | state/mech_ledger.txt | EVENT\\p | %d | the-ledger-holds-exactly-the-successful-steps-and-not-the-refused-one\nDO | status\nSAY | EVENTS=%d | status-replays-the-ledger\n", steps, steps);
    snprintf(prop, sizeof prop, "PROPOSE | kind=mechanic | id=%s.%s  | status=candidate | body=verb %s (%s) of kind %s: %s; rules in %s/mechanics.pdl, case play/phases/%s_%s.phases.pdl, rule op mech_step_op\n", app, mech, mech, args, kind, sent, app, app, mech);
    /* unreplaced placeholders: refuse before anything is written */
    const char *all[] = { verbline, cond, ir, mechs, phases, prop }; for (int i = 0; i < 6; i++) if (strstr(all[i], "{{")) return refuse("an unreplaced {{placeholder}} would be written");
    /* write */
    char pg[P]; snprintf(pg, sizeof pg, "%s/event_pkg/pages/page_1", evd); mkdirs(pg);
    char fp[P]; FILE *w;
    snprintf(fp, sizeof fp, "%s/condition.pdl", pg); if (!(w = fopen(fp, "w"))) return refuse("cannot write condition.pdl"); fputs(cond, w); fclose(w);
    snprintf(fp, sizeof fp, "%s/event.ir.pdl", pg); if (!(w = fopen(fp, "w"))) return refuse("cannot write event.ir.pdl"); fputs(ir, w); fclose(w);
    if (!(w = fopen(vp, "a"))) return refuse("cannot append verbs.pdl"); fputs(verbline, w); fclose(w);
    if (!(w = fopen(mp, "a"))) return refuse("cannot append mechanics.pdl"); fputs(mechs, w); fclose(w);
    { char d[P]; snprintf(d, sizeof d, "%s", phd); char *s = strrchr(d, '/'); if (s) { *s = 0; mkdirs(d); } }
    if (!(w = fopen(phd, "w"))) return refuse("cannot write the phase table"); fputs(phases, w); fclose(w);
    { char d[P]; snprintf(d, sizeof d, "%s", pp); char *s = strrchr(d, '/'); if (s) { *s = 0; mkdirs(d); } }
    if (!(w = fopen(pp, "a"))) return refuse("cannot append the proposal"); fputs(prop, w); fclose(w);
    printf("MECH_NEW|%s|%s|%s|files=5\n", app, mech, kind); return 0;
}
