/* tree_lint - one linter for every "tree of things" the engine will have: creature EVOLUTION / shapeshift trees (parent = the form it comes from), TECH trees (needs = what must be known first),
 * ANATOMY part trees (parent = part_of). One verb family instead of three (compaction rule, design doc section 21); this is the generic form of the roadmap's `part_tree_lint`.
 * Designs: #.#.calendar-dox/!.HQ-IQ-BOOK/16.game/TERUMON-SHAPESHIFT-AND-EVOLUTION-TREES-DESIGN.md and TECH-TREES-AND-RENDER-RESEARCH-PIPELINE-DESIGN.md.
 *
 * Input: a .pdl file; every row `NODE | <id> | <title> | key=value | key=value ...` is a node (other rows are ignored, `#` starts a comment line).
 *   parent=<id>   the single parent ("-" or absent = a root)        needs=<id>,<id>   prerequisites (a DAG; any number)
 *   cost=<n>      a non-negative whole number (MP, EXP, research points: the caller decides)        tier=<n>   a non-negative whole number        any other key=value is carried and ignored
 * Usage:  tree_lint <file.pdl> [--single-root] [--max-depth N] [--tier-monotone] [--require key[,key]] [--path <id>] [--closure <id>]
 * Checks (each failure is one line `FAIL <code> <id> <detail>`; the run goes on so ALL problems are listed):
 *   bad_row (NODE with fewer than 3 fields) | bad_id (not [A-Za-z0-9_.-]+, at most 64) | dup_id | bad_number (cost / tier) | missing_key (--require) | unknown_parent | unknown_need | self_parent | self_need
 *   parent_cycle | needs_cycle | multi_root / no_root (--single-root, or no node without a parent at all) | too_deep (--max-depth, a root has depth 0) | tier_order (--tier-monotone: a child's tier must exceed
 *   its parent's and a node's tier must exceed every prerequisite's tier; nodes without a tier are skipped)
 * Report (stdout, key=value): nodes roots edges needs_edges max_depth failures. --path <id> prints `path=<root>/<..>/<id>` (the parent chain); --closure <id> prints `closure=<id>,<id>..` (every prerequisite
 * reachable through needs and parent, the node itself last) and `closure_cost=<n>` (the sum of cost= over the closure; nodes without a cost count 0): "what does it take to reach this".
 * Exit: 0 clean | 1 at least one failure | 2 usage or unreadable file.   Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/tree_lint.+x tree_lint.c */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAXN 4096
#define MAXNEED 24
typedef struct { char id[72], parent[72], needs[MAXNEED][72]; int nneeds; long cost, tier; int has_cost, has_tier; int pi; int ni[MAXNEED]; char keys[512]; } Node;
static Node N[MAXN]; static int nN, failures;

static void fail(const char *code, const char *id, const char *detail) { printf("FAIL %s %s %s\n", code, id[0] ? id : "-", detail); failures++; }
static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0; return s; }
static int id_ok(const char *s) { size_t n = strlen(s); if (!n || n > 64) return 0; for (size_t i = 0; i < n; i++) if (!isalnum((unsigned char)s[i]) && s[i] != '_' && s[i] != '.' && s[i] != '-') return 0; return 1; }
static int find(const char *id) { for (int i = 0; i < nN; i++) if (!strcmp(N[i].id, id)) return i; return -1; }
static int whole(const char *s, long *out) { if (!*s) return 0; for (const char *p = s; *p; p++) if (!isdigit((unsigned char)*p)) return 0; if (strlen(s) > 12) return 0; *out = atol(s); return 1; }
static int has_key(const Node *n, const char *k) { char pat[80]; snprintf(pat, sizeof pat, ",%s,", k); char hay[560]; snprintf(hay, sizeof hay, ",%s,", n->keys); return strstr(hay, pat) != NULL; }

static int depth_of(int i, int limit) { int d = 0; while (N[i].pi >= 0 && d <= limit) { i = N[i].pi; d++; } return d; }
static int g_color[MAXN];
static int needs_dfs(int i, char *cyc, size_t cap) {      /* 1 = a cycle through needs, naming the node where it closes */
    g_color[i] = 1;
    for (int k = 0; k < N[i].nneeds; k++) { int j = N[i].ni[k]; if (j < 0) continue;
        if (g_color[j] == 1) { snprintf(cyc, cap, "%s -> %s", N[i].id, N[j].id); return 1; }
        if (g_color[j] == 0 && needs_dfs(j, cyc, cap)) return 1; }
    g_color[i] = 2; return 0;
}
static int g_seen[MAXN];
static void closure_walk(int i, int *order, int *n) {
    if (g_seen[i]) return;
    g_seen[i] = 1;
    if (N[i].pi >= 0) closure_walk(N[i].pi, order, n);
    for (int k = 0; k < N[i].nneeds; k++) if (N[i].ni[k] >= 0) closure_walk(N[i].ni[k], order, n);
    order[(*n)++] = i;
}

int main(int argc, char **argv) {
    if (argc < 2) goto usage;
    int single_root = 0, tier_mono = 0, max_depth = -1; const char *require = NULL, *path_id = NULL, *clo_id = NULL;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--single-root")) single_root = 1;
        else if (!strcmp(argv[i], "--tier-monotone")) tier_mono = 1;
        else if (!strcmp(argv[i], "--max-depth") && i + 1 < argc) max_depth = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--require") && i + 1 < argc) require = argv[++i];
        else if (!strcmp(argv[i], "--path") && i + 1 < argc) path_id = argv[++i];
        else if (!strcmp(argv[i], "--closure") && i + 1 < argc) clo_id = argv[++i];
        else goto usage;
    }
    FILE *f = fopen(argv[1], "r"); if (!f) { fprintf(stderr, "tree_lint: cannot read %s\n", argv[1]); return 2; }
    static char line[4096];
    while (fgets(line, sizeof line, f)) {
        char *t = trim(line); if (!*t || *t == '#') continue;
        char *fld[40]; int nf = 0; char *p = t; while (nf < 40) { fld[nf++] = p; char *b = strchr(p, '|'); if (!b) break; *b = 0; p = b + 1; }
        for (int i = 0; i < nf; i++) fld[i] = trim(fld[i]);
        if (strcmp(fld[0], "NODE")) continue;
        if (nf < 3) { fail("bad_row", nf > 1 ? fld[1] : "", "NODE needs at least | id | title |"); continue; }
        if (nN >= MAXN) { fprintf(stderr, "tree_lint: more than %d nodes\n", MAXN); return 2; }
        Node *n = &N[nN]; memset(n, 0, sizeof *n); n->pi = -1; snprintf(n->id, sizeof n->id, "%.71s", fld[1]);
        if (!id_ok(fld[1])) { fail("bad_id", fld[1], "ids are [A-Za-z0-9_.-], 1..64 characters"); continue; }
        if (find(n->id) >= 0) { fail("dup_id", n->id, "defined twice"); continue; }
        int bad = 0;
        for (int i = 3; i < nf; i++) { char *eq = strchr(fld[i], '='); if (!eq) continue; *eq = 0; const char *k = fld[i], *v = trim(eq + 1);
            if (strlen(n->keys) + strlen(k) + 2 < sizeof n->keys) { if (n->keys[0]) strcat(n->keys, ","); strcat(n->keys, k); }
            if (!strcmp(k, "parent")) { if (strcmp(v, "-")) snprintf(n->parent, sizeof n->parent, "%.71s", v); }
            else if (!strcmp(k, "needs")) { char tmp[512]; snprintf(tmp, sizeof tmp, "%s", v); for (char *q = strtok(tmp, ","); q; q = strtok(NULL, ",")) { q = trim(q); if (!*q) continue; if (n->nneeds < MAXNEED) snprintf(n->needs[n->nneeds++], 72, "%.71s", q); else { fail("bad_row", n->id, "more than 24 needs"); bad = 1; } } }
            else if (!strcmp(k, "cost")) { if (!whole(v, &n->cost)) { fail("bad_number", n->id, "cost must be a non-negative whole number"); bad = 1; } else n->has_cost = 1; }
            else if (!strcmp(k, "tier")) { if (!whole(v, &n->tier)) { fail("bad_number", n->id, "tier must be a non-negative whole number"); bad = 1; } else n->has_tier = 1; } }
        if (bad) { /* keep the node so later checks still see its id */ }
        nN++;
    }
    fclose(f);
    if (require) { char tmp[256]; snprintf(tmp, sizeof tmp, "%s", require); for (char *k = strtok(tmp, ","); k; k = strtok(NULL, ",")) for (int i = 0; i < nN; i++) if (!has_key(&N[i], k)) { char d[96]; snprintf(d, sizeof d, "missing required key %s", k); fail("missing_key", N[i].id, d); } }
    long edges = 0, nedges = 0;
    for (int i = 0; i < nN; i++) {
        if (N[i].parent[0]) { if (!strcmp(N[i].parent, N[i].id)) fail("self_parent", N[i].id, "its own parent"); else { int j = find(N[i].parent); if (j < 0) fail("unknown_parent", N[i].id, N[i].parent); else { N[i].pi = j; edges++; } } }
        for (int k = 0; k < N[i].nneeds; k++) { N[i].ni[k] = -1; if (!strcmp(N[i].needs[k], N[i].id)) { fail("self_need", N[i].id, "needs itself"); continue; } int j = find(N[i].needs[k]); if (j < 0) fail("unknown_need", N[i].id, N[i].needs[k]); else { N[i].ni[k] = j; nedges++; } }
    }
    int cyc_p = 0;                                                       /* parent cycles: following parents must reach a root within nN steps */
    for (int i = 0; i < nN && !cyc_p; i++) { int j = i, steps = 0; while (N[j].pi >= 0 && steps <= nN) { j = N[j].pi; steps++; } if (steps > nN) { fail("parent_cycle", N[i].id, "following parent= never reaches a root"); cyc_p = 1; } }
    { char cyc[160]; memset(g_color, 0, sizeof g_color); for (int i = 0; i < nN; i++) if (g_color[i] == 0 && needs_dfs(i, cyc, sizeof cyc)) { fail("needs_cycle", N[i].id, cyc); break; } }
    int roots = 0, maxd = 0; for (int i = 0; i < nN; i++) if (!N[i].parent[0]) roots++;
    if (!cyc_p) for (int i = 0; i < nN; i++) { int d = depth_of(i, nN); if (d > maxd) maxd = d; if (max_depth >= 0 && d > max_depth) { char dd[64]; snprintf(dd, sizeof dd, "depth %d > %d", d, max_depth); fail("too_deep", N[i].id, dd); } }
    if (nN > 0 && roots == 0) fail("no_root", "-", "every node has a parent: a cycle or an empty forest");
    if (single_root && roots > 1) { char d[64]; snprintf(d, sizeof d, "%d roots, expected 1", roots); fail("multi_root", "-", d); }
    if (tier_mono) for (int i = 0; i < nN; i++) {
        if (!N[i].has_tier) continue;
        if (N[i].pi >= 0 && N[N[i].pi].has_tier && N[i].tier <= N[N[i].pi].tier) fail("tier_order", N[i].id, "tier must exceed its parent's");
        for (int k = 0; k < N[i].nneeds; k++) { int j = N[i].ni[k]; if (j >= 0 && N[j].has_tier && N[i].tier <= N[j].tier) { fail("tier_order", N[i].id, "tier must exceed every prerequisite's"); break; } } }
    printf("nodes=%d\nroots=%d\nedges=%ld\nneeds_edges=%ld\nmax_depth=%d\nfailures=%d\n", nN, roots, edges, nedges, maxd, failures);
    if (path_id && !failures) { int i = find(path_id); if (i < 0) { fprintf(stderr, "tree_lint: no node %s\n", path_id); return 2; } char chain[600] = ""; int stack[256], d = 0; while (i >= 0 && d < 256) { stack[d++] = i; i = N[i].pi; } for (int k = d - 1; k >= 0; k--) { strncat(chain, N[stack[k]].id, sizeof chain - strlen(chain) - 2); if (k) strcat(chain, "/"); } printf("path=%s\n", chain); }
    if (clo_id && !failures) { int i = find(clo_id); if (i < 0) { fprintf(stderr, "tree_lint: no node %s\n", clo_id); return 2; } static int order[MAXN]; int n = 0; memset(g_seen, 0, sizeof g_seen); closure_walk(i, order, &n); long total = 0;
        printf("closure="); for (int k = 0; k < n; k++) { printf("%s%s", k ? "," : "", N[order[k]].id); total += N[order[k]].cost; } printf("\nclosure_cost=%ld\n", total); }
    return failures ? 1 : 0;
usage:
    fprintf(stderr, "Usage: tree_lint <file.pdl> [--single-root] [--max-depth N] [--tier-monotone] [--require key[,key]] [--path <id>] [--closure <id>]\n"); return 2;
}
