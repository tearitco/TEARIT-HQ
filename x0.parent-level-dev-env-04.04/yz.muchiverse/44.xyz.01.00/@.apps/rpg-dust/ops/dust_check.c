/* dust_check - R0 of rpg-dust (BOOK 16.game/RPG-DUST-DESIGN.md section 9): the checks that exist BEFORE any generator does. Two modes, both pure (read files, print, exit code; nothing is written).
 *
 *   dust_check ladder <scale.pdl>                 the scale ladder, the seed and the tweakable parameters
 *   dust_check react  <elements.pdl> <reactions.pdl>   every reaction row conserves every element
 *
 * ladder rows (pipe-delimited, '#' comments, a leading `SECTION | KEY | VALUE` line and a `---` line are skipped):
 *   SEED  | universe | <unsigned 64-bit decimal>             exactly one
 *   PARAM | <name [a-z0-9_]+> | <number> [| <level id>]       names unique; the optional level (default: the root) is where the value enters the seed chain (dust_seed lseed)
 *   SCALE | id | log2_m | cell_about | parent | gen | view | tick | note
 *       id [a-z0-9_]+ unique; log2_m integer in -80..+80 (metres per cell = 2^log2_m); parent '-' for exactly ONE root, otherwise the id of another SCALE row;
 *       a child is SMALLER than its parent (log2_m strictly lower), so no cycles; gen = dust_gen_<name>; view = one or more of 2d-tiles 3d-voxel field joined by '+';
 *       tick = us ms second minute 10min hour day; cell_about and note are free text (cell_about not empty).
 *   any other first field is an error (a typo must not pass silently).
 * Exit: 0 all rows valid (prints `ladder ok: <n> levels, root <id>, seed <n>, <p> params`), 1 one or more problems (each printed as `LINE n: <what>`), 2 usage or unreadable file.
 *
 * react rows: REACTION | id | reactants | products | dH_kJ_per_mol | conditions | note      (reactants / products like `2H2 + O2`, coefficient then formula, joined by '+')
 *   prints `OK id` or `IMBALANCE id <El>:<left>-><right> ...` or `BAD id <what>` per reaction and a summary `reactions=N balanced=B bad=K`. Exit 0 all balanced, 1 any imbalance or bad row, 2 usage / no elements.
 *   Formula parsing is _shared-lib/chem_formula.c (per-element atom counts against the locked elements.pdl).
 * Build: sh ops/build_dust_check.sh   (gcc -std=gnu11 -Wall -Wextra -Werror -O2 -I<house>/&.widgits/_shared-lib) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "dust_pdl.c"
#include "chem_formula.c"      /* found with -I <house>/&.widgits/_shared-lib (build script and harness both pass it) */
#define MAXL 64
typedef struct { char id[48]; int log2m; char parent[48]; int line; int ok; } Level;
static int is_int(const char *s, long *v) { char *e; if (!*s) return 0; errno = 0; long x = strtol(s, &e, 10); if (*e || errno) return 0; *v = x; return 1; }
static int is_num(const char *s) { char *e; if (!*s) return 0; strtod(s, &e); return !*e; }
static int in_set(const char *s, const char *const *set) { for (; *set; set++) if (!strcmp(s, *set)) return 1; return 0; }
static int check_view(const char *v) { static const char *ok[] = { "2d-tiles", "3d-voxel", "field", 0 }; char b[96]; snprintf(b, sizeof b, "%s", v); int n = 0; for (char *t = strtok(b, "+"); t; t = strtok(NULL, "+")) { while (*t == ' ') t++; char *e = t + strlen(t); while (e > t && e[-1] == ' ') *--e = 0; if (!in_set(t, ok)) return 0; n++; } return n > 0; }
static int ladder(const char *path) {
    FILE *fp = fopen(path, "r"); if (!fp) { fprintf(stderr, "dust_check: cannot read %s\n", path); return 2; }
    Level L[MAXL]; int nl = 0, bad = 0, seeds = 0, params = 0; char seed[32] = "", pn[MAXL][48], pl[MAXL][48]; int pline[MAXL]; char ln[1024]; int lineno = 0;
#define ERR(...) do { printf("LINE %d: ", lineno); printf(__VA_ARGS__); printf("\n"); bad++; } while (0)
    while (fgets(ln, sizeof ln, fp)) {
        lineno++; char *s = ln; while (*s == ' ' || *s == '\t') s++; if (!*s || *s == '#' || *s == '\n' || *s == '\r' || !strncmp(s, "---", 3) || !strncmp(s, "SECTION", 7)) continue;
        char *f[10]; int n = split_bar(s, f, 10);
        if (!strcmp(f[0], "SEED")) {
            if (n < 3 || strcmp(f[1], "universe")) { ERR("SEED row must be SEED | universe | <number>"); continue; }
            int digits = (int)strlen(f[2]); int okd = digits > 0 && digits <= 20; for (int i = 0; okd && i < digits; i++) if (!isdigit((unsigned char)f[2][i])) okd = 0;
            if (okd && digits == 20 && strcmp(f[2], "18446744073709551615") > 0) okd = 0;      /* above 2^64 - 1 */
            if (!okd) { ERR("seed '%s' is not an unsigned 64-bit decimal number", f[2]); continue; }
            seeds++; if (seeds > 1) { ERR("more than one SEED row"); }
            cp(seed, sizeof seed, f[2]);
        } else if (!strcmp(f[0], "PARAM")) {
            if (n < 3 || !ident(f[1]) || !is_num(f[2])) { ERR("PARAM row must be PARAM | <name a-z0-9_> | <number>"); continue; }
            int dup = 0; for (int i = 0; i < params; i++) if (!strcmp(pn[i], f[1])) dup = 1; if (dup) { ERR("duplicate PARAM '%s'", f[1]); continue; }
            if (params < MAXL) { cp(pn[params], sizeof pn[0], f[1]); cp(pl[params], sizeof pl[0], n > 3 ? f[3] : ""); pline[params] = lineno; }
            params++;
        } else if (!strcmp(f[0], "SCALE")) {
            if (n < 8) { ERR("SCALE row needs 8 fields (id | log2_m | cell_about | parent | gen | view | tick | note)"); continue; }
            Level *l = &L[nl < MAXL ? nl : MAXL - 1]; if (nl >= MAXL) { ERR("more than %d levels", MAXL); continue; } l->line = lineno; l->ok = 1; long v;
            if (!ident(f[1])) { ERR("id '%s' must be lower case letters, digits, underscore", f[1]); l->ok = 0; } cp(l->id, sizeof l->id, f[1]);
            for (int i = 0; i < nl; i++) if (!strcmp(L[i].id, f[1])) { ERR("duplicate level id '%s' (first on line %d)", f[1], L[i].line); l->ok = 0; }
            if (!is_int(f[2], &v) || v < -80 || v > 80) { ERR("log2_m '%s' must be an integer in -80..80", f[2]); l->ok = 0; v = 0; } l->log2m = (int)v;
            if (!f[3][0]) { ERR("cell_about is empty"); l->ok = 0; }
            if (strcmp(f[4], "-") && !ident(f[4])) { ERR("parent '%s' must be '-' or a level id", f[4]); l->ok = 0; } cp(l->parent, sizeof l->parent, f[4]);
            if (strncmp(f[5], "dust_gen_", 9) || !ident(f[5])) { ERR("gen '%s' must be dust_gen_<name>", f[5]); l->ok = 0; }
            if (!check_view(f[6])) { ERR("view '%s' must be 2d-tiles, 3d-voxel, field (joined by +)", f[6]); l->ok = 0; }
            { static const char *tk[] = { "us", "ms", "second", "minute", "10min", "hour", "day", 0 }; if (!in_set(f[7], tk)) { ERR("tick '%s' must be one of us ms second minute 10min hour day", f[7]); l->ok = 0; } }
            nl++;
        } else ERR("unknown row kind '%s' (SEED, PARAM, SCALE)", f[0]);
    }
    fclose(fp); char root[48] = ""; int roots = 0;
    for (int i = 0; i < nl; i++) {
        if (!strcmp(L[i].parent, "-")) { roots++; cp(root, sizeof root, L[i].id); continue; }
        int pi = -1; for (int j = 0; j < nl; j++) if (!strcmp(L[j].id, L[i].parent)) pi = j;
        if (pi < 0) { lineno = L[i].line; ERR("level '%s' names parent '%s' which is not a level", L[i].id, L[i].parent); }
        else if (L[i].ok && L[pi].ok && L[i].log2m >= L[pi].log2m) { lineno = L[i].line; ERR("level '%s' (log2_m %d) is not smaller than its parent '%s' (log2_m %d)", L[i].id, L[i].log2m, L[pi].id, L[pi].log2m); }
    }
    for (int i = 0; i < params && i < MAXL; i++) { if (!pl[i][0]) continue; int found = 0; for (int j = 0; j < nl; j++) if (!strcmp(L[j].id, pl[i])) found = 1; if (!found) { lineno = pline[i]; ERR("PARAM '%s' names level '%s' which is not a level", pn[i], pl[i]); } }
    lineno = 0; if (nl == 0) ERR("no SCALE rows"); else if (roots != 1) ERR("%d root levels (parent '-'); exactly one is required", roots);
    if (seeds == 0) ERR("no SEED row");
    if (bad) { printf("ladder FAILED: %d problem%s\n", bad, bad == 1 ? "" : "s"); return 1; }
    printf("ladder ok: %d levels, root %s, seed %s, %d params\n", nl, root, seed, params); return 0;
}
static int react(const char *elpath, const char *rpath) {
    if (cf_load_elements(elpath) < 100) { fprintf(stderr, "dust_check: elements.pdl %s unreadable or has fewer than 100 ELEMENT rows\n", elpath); return 2; }
    FILE *fp = fopen(rpath, "r"); if (!fp) { fprintf(stderr, "dust_check: cannot read %s\n", rpath); return 2; }
    char ln[1024]; int total = 0, good = 0, badn = 0;
    while (fgets(ln, sizeof ln, fp)) {
        char *s = ln; while (*s == ' ') s++; if (strncmp(s, "REACTION", 8)) continue; char *f[8]; int n = split_bar(s, f, 8); total++;
        if (n < 4 || !ident(f[1])) { printf("BAD ? REACTION needs id | reactants | products\n"); badn++; continue; }
        CfCount l, r; if (!cf_side(f[2], &l)) { printf("BAD %s reactants '%s' do not parse (unknown element, bad count or bad formula)\n", f[1], f[2]); badn++; continue; }
        if (!cf_side(f[3], &r)) { printf("BAD %s products '%s' do not parse (unknown element, bad count or bad formula)\n", f[1], f[3]); badn++; continue; }
        char diff[512] = ""; size_t o = 0; int any = 0; for (int i = 0; i < CF_MAXEL; i++) if (l.n[i] != r.n[i]) { any = 1; o += (size_t)snprintf(diff + o, sizeof diff - o, " %s:%ld->%ld", cf_el[i].sym, l.n[i], r.n[i]); }
        if (any) { printf("IMBALANCE %s%s\n", f[1], diff); badn++; } else { printf("OK %s\n", f[1]); good++; }
    }
    fclose(fp); printf("reactions=%d balanced=%d bad=%d\n", total, good, badn); return (badn || total == 0) ? 1 : 0;
}
int main(int argc, char **argv) {
    if (argc == 3 && !strcmp(argv[1], "ladder")) return ladder(argv[2]);
    if (argc == 4 && !strcmp(argv[1], "react")) return react(argv[2], argv[3]);
    fprintf(stderr, "usage: dust_check ladder <scale.pdl> | dust_check react <elements.pdl> <reactions.pdl>\n"); return 2;
}
