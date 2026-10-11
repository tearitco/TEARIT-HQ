#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdarg.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/types.h>

/* knowledge_manager: <module> for knowledge-hq.xhtpm - a VIEW of the labeled
 * chemistry knowledge in <bank>/data/chem, data/hidden and proposals/chem_assoc,
 * plus owner review actions.  Same shape as concept-bank-hq: this ONE process
 * owns state, polls <pkg>/knowledge_action.txt (seq=<n>\ncmd=<VERB[:arg]>) and
 * publishes <pkg>/knowledge_ui.txt.  No renderer C.
 *
 *   knowledge_manager <house_root> <pkg>            daemon (50 ms poll loop)
 *   knowledge_manager --once <house_root> <pkg>     read state, apply at most one
 *                                                   pending action, write the ui
 *                                                   file, exit 0 (harness mode)
 *
 * Verbs: SHOW:compounds|elements  FILTER:ALL|OK|BAD|APPROVED|REJECTED|UNREVIEWED
 *        SEL:<n> (compound, 1-based)  SELEL:<Z>  BACK  RELOAD
 *        ACCEPT  REJECT  ADJUST:<e>:<f>:<m>   (owner review of the selected compound)
 *
 * WRITES: the ONLY data file this binary ever opens for writing is
 * <bank>/proposals/chem_assoc/review_owner.txt (append-only, one REVIEW row per
 * owner action).  Everything else it writes is its own scratch under <pkg>
 * (knowledge_ui.txt, state/knowledge_state.txt, the daemon's action-file reset).
 * Owner rows outrank the manager's review.txt rows; a later owner row wins.
 * Change detection for the daemon = size growth of the review files, not mtime. */

#define PL 4096
#define MAXC 256
#define MAXE 130
#define MAXF 4096

typedef struct {
    int n, ok, atoms, mass;
    char id[32], emoji[32], name[96], formula[64], elements[64], kind[32], state[32];
    char mp[16], bp[16], density[16], hint[320];
    int has_assoc; char phrase[240], pe[16], pf[16], pm[16], proposer[64], pstatus[32];
    int mv; char mw[64], mwhy[200], mby[48];   /* manager review: 0 none 1 accept 2 adjust 3 reject */
    int ov; char ow[64];                       /* owner review, same coding */
    int has_appr; char ae[16], af[16], am[16], aby[48], abasis[32];
} Comp;
typedef struct { int z, p, nn, mass; char sym[8], name[48]; } Elem;
typedef struct { char concept[40], row[32]; int val; } Fb;

static char house[PL], pkg[PL], cfg_path[PL], bank[PL], state_path[PL], ui_path[PL], act_path[PL];
static Comp comps[MAXC]; static int ncomp = 0;
static Elem els[MAXE]; static int nel = 0;
static Fb fbs[MAXF]; static int nfb = 0;
static char view[12] = "compounds", filter[16] = "ALL";
static int sel = 0, selz = 0, level = 0, last_seq = 0;
static char msg[300] = "";
static long sig_prev = -1;

static const char *VERB[4] = { "", "accept", "adjust", "reject" };

static void sanitize(char *s) { for (char *p = s; *p; p++) if (*p == '|' || *p == '\n' || *p == '\r' || *p == '\t') *p = ' '; }
static void cp(char *d, size_t n, const char *s) { size_t l = strlen(s); if (l >= n) l = n - 1; memcpy(d, s, l); d[l] = '\0'; }
static void mkdir_p(const char *path) {
    char t[PL]; cp(t, sizeof(t), path);
    for (char *p = t + 1; *p; p++) if (*p == '/') { *p = '\0'; mkdir(t, 0755); *p = '/'; }
    mkdir(t, 0755);
}
static char *slurp(const char *path) {
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *b = malloc((size_t)n + 1); if (!b) { fclose(f); return NULL; }
    size_t r = fread(b, 1, (size_t)n, f); b[r] = '\0'; fclose(f); return b;
}
static long fsize(const char *p) { struct stat st; return stat(p, &st) == 0 ? (long)st.st_size : 0; }
static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    for (char *e = s + strlen(s); e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r'); ) *--e = '\0';
    return s;
}
/* split a " | " delimited row in place */
static int split(char *line, char **f, int max) {
    int nf = 0; char *s = line;
    while (nf < max) { f[nf++] = s; char *c = strstr(s, " | "); if (!c) break; *c = '\0'; s = c + 3; }
    for (int i = 0; i < nf; i++) f[i] = trim(f[i]);
    return nf;
}
static const char *kv(char **f, int nf, const char *key) {
    size_t kl = strlen(key);
    for (int i = 1; i < nf; i++) if (!strncmp(f[i], key, kl) && f[i][kl] == '=') return f[i] + kl + 1;
    return "";
}
static void path_of(char *out, size_t n, const char *rel) { snprintf(out, n, "%s/%s", bank, rel); }

static void load_config(void) {
    snprintf(bank, sizeof(bank), "%s/&.widgits/concept-bank", house);
    char *b = slurp(cfg_path); if (!b) return;
    for (char *ln = strtok(b, "\n"); ln; ln = strtok(NULL, "\n")) {
        if (ln[0] == '#') continue;
        char *c1 = strchr(ln, '|'); if (!c1) continue; char *c2 = strchr(c1 + 1, '|'); if (!c2) continue;
        *c2 = '\0'; if (strcmp(trim(c1 + 1), "bank_dir") != 0) continue;
        char *v = trim(c2 + 1); if (!*v) continue;
        if (v[0] == '/') cp(bank, sizeof(bank), v); else snprintf(bank, sizeof(bank), "%s/%s", house, v);
    }
    free(b);
}

/* ------------------------------------------------------------------ data */
static void load_compounds(const char *rel) {
    char p[PL]; path_of(p, sizeof(p), rel);
    char *b = slurp(p); if (!b) return;
    for (char *ln = strtok(b, "\n"); ln; ln = strtok(NULL, "\n")) {
        if (strncmp(ln, "COMPOUND |", 10) != 0 || ncomp >= MAXC) continue;
        char *f[24]; int nf = split(ln, f, 24); Comp *c = &comps[ncomp];
        memset(c, 0, sizeof(*c)); c->n = ++ncomp;
        if (nf > 1) cp(c->id, sizeof(c->id), f[1]);
        cp(c->emoji, sizeof(c->emoji), kv(f, nf, "emoji")); cp(c->name, sizeof(c->name), kv(f, nf, "name"));
        cp(c->formula, sizeof(c->formula), kv(f, nf, "formula")); c->ok = atoi(kv(f, nf, "formula_ok"));
        c->atoms = atoi(kv(f, nf, "atoms")); c->mass = atoi(kv(f, nf, "mass_number"));
        cp(c->elements, sizeof(c->elements), kv(f, nf, "elements")); cp(c->kind, sizeof(c->kind), kv(f, nf, "kind"));
        cp(c->state, sizeof(c->state), kv(f, nf, "state")); cp(c->mp, sizeof(c->mp), kv(f, nf, "mp"));
        cp(c->bp, sizeof(c->bp), kv(f, nf, "bp")); cp(c->density, sizeof(c->density), kv(f, nf, "density"));
        cp(c->hint, sizeof(c->hint), kv(f, nf, "hint"));
        sanitize(c->name); sanitize(c->hint); sanitize(c->formula);
    }
    free(b);
}
static void load_elements(void) {
    char p[PL]; path_of(p, sizeof(p), "data/chem/elements.pdl");
    char *b = slurp(p); if (!b) return;
    for (char *ln = strtok(b, "\n"); ln; ln = strtok(NULL, "\n")) {
        if (strncmp(ln, "ELEMENT |", 9) != 0 || nel >= MAXE) continue;
        char *f[8]; int nf = split(ln, f, 8); if (nf < 7) continue;
        Elem *e = &els[nel++]; e->z = atoi(f[1]); cp(e->sym, sizeof(e->sym), f[2]); cp(e->name, sizeof(e->name), f[3]);
        e->p = atoi(f[4]); e->nn = atoi(f[5]); e->mass = atoi(f[6]);
    }
    free(b);
}
static void load_assoc(void) {
    char p[PL]; path_of(p, sizeof(p), "proposals/chem_assoc/draft.txt");
    char *b = slurp(p); if (!b) return;
    for (char *ln = strtok(b, "\n"); ln; ln = strtok(NULL, "\n")) {
        if (strncmp(ln, "ASSOC |", 7) != 0) continue;
        char *f[10]; int nf = split(ln, f, 10); if (nf < 8) continue;
        int n = atoi(f[1]); if (n < 1 || n > ncomp) continue;
        Comp *c = &comps[n - 1]; c->has_assoc = 1; cp(c->phrase, sizeof(c->phrase), f[2]); sanitize(c->phrase);
        cp(c->pe, sizeof(c->pe), kv(f, nf, "energy")); cp(c->pf, sizeof(c->pf), kv(f, nf, "force")); cp(c->pm, sizeof(c->pm), kv(f, nf, "motion"));
        cp(c->proposer, sizeof(c->proposer), kv(f, nf, "proposer")); cp(c->pstatus, sizeof(c->pstatus), kv(f, nf, "status"));
    }
    free(b);
}
/* review rows: REVIEW | n | verdict | [energy=.. force=.. motion=..] | by=.. | why  (last row per n wins) */
static void load_reviews(const char *rel, int owner) {
    char p[PL]; path_of(p, sizeof(p), rel);
    char *b = slurp(p); if (!b) return;
    for (char *ln = strtok(b, "\n"); ln; ln = strtok(NULL, "\n")) {
        if (strncmp(ln, "REVIEW |", 8) != 0) continue;
        char *f[10]; int nf = split(ln, f, 10); if (nf < 3) continue;
        int n = atoi(f[1]); if (n < 1 || n > ncomp) continue;
        int v = !strcmp(f[2], "accept") ? 1 : !strcmp(f[2], "adjust") ? 2 : !strcmp(f[2], "reject") ? 3 : 0; if (!v) continue;
        char w[64] = "", by[48] = "", why[200] = "";
        for (int i = 3; i < nf; i++) {
            if (!strncmp(f[i], "energy=", 7)) cp(w, sizeof(w), f[i]);
            else if (!strncmp(f[i], "by=", 3)) cp(by, sizeof(by), f[i] + 3);
            else cp(why, sizeof(why), f[i]);
        }
        Comp *c = &comps[n - 1];
        if (owner) { c->ov = v; cp(c->ow, sizeof(c->ow), w); }
        else { c->mv = v; cp(c->mw, sizeof(c->mw), w); cp(c->mby, sizeof(c->mby), by); cp(c->mwhy, sizeof(c->mwhy), why); sanitize(c->mwhy); }
    }
    free(b);
}
static void load_approved(void) {
    char p[PL]; path_of(p, sizeof(p), "data/hidden/chem_assoc.approved.txt");
    char *b = slurp(p); if (!b) return;
    for (char *ln = strtok(b, "\n"); ln; ln = strtok(NULL, "\n")) {
        if (strncmp(ln, "APPROVED |", 10) != 0) continue;
        char *f[10]; int nf = split(ln, f, 10); if (nf < 8) continue;
        int n = atoi(f[1]); if (n < 1 || n > ncomp) continue;
        Comp *c = &comps[n - 1]; c->has_appr = 1;
        cp(c->ae, sizeof(c->ae), kv(f, nf, "energy")); cp(c->af, sizeof(c->af), kv(f, nf, "force")); cp(c->am, sizeof(c->am), kv(f, nf, "motion"));
        cp(c->aby, sizeof(c->aby), kv(f, nf, "by")); cp(c->abasis, sizeof(c->abasis), kv(f, nf, "basis"));
    }
    free(b);
}
static void load_feedback(void) {
    char p[PL]; path_of(p, sizeof(p), "data/chem/chem_feedback.txt"); nfb = 0;
    char *b = slurp(p); if (!b) return;
    for (char *ln = strtok(b, "\n"); ln && nfb < MAXF; ln = strtok(NULL, "\n")) {
        const char *c = strstr(ln, "concept="), *r = strstr(ln, "row="), *v = strstr(ln, "valence="); if (!c || !v) continue;
        Fb *x = &fbs[nfb++]; memset(x, 0, sizeof(*x));
        size_t n = strcspn(c + 8, " |"); if (n >= sizeof(x->concept)) n = sizeof(x->concept) - 1; memcpy(x->concept, c + 8, n);
        if (r) { n = strcspn(r + 4, " |"); if (n >= sizeof(x->row)) n = sizeof(x->row) - 1; memcpy(x->row, r + 4, n); }
        x->val = v[8] == '-' ? -1 : 1;
    }
    free(b);
}
static void load_all(void) {
    ncomp = nel = 0;
    load_compounds("data/chem/compounds_chem.pdl"); load_compounds("data/chem/compounds_mcat.pdl");
    load_elements(); load_assoc();
    load_reviews("proposals/chem_assoc/review.txt", 0); load_reviews("proposals/chem_assoc/review_owner.txt", 1);
    load_approved(); load_feedback();
}
static long review_sig(void) {
    char p[PL]; long s = 0; const char *r[3] = { "proposals/chem_assoc/review.txt", "proposals/chem_assoc/review_owner.txt", "data/hidden/chem_assoc.approved.txt" };
    for (int i = 0; i < 3; i++) { path_of(p, sizeof(p), r[i]); s = s * 7919 + fsize(p); }
    return s;
}

/* -------------------------------------------------------------- state file */
static void save_state(void) {
    FILE *f = fopen(state_path, "w"); if (!f) return;
    fprintf(f, "view=%s\nfilter=%s\nsel=%d\nselz=%d\nlevel=%d\nlast_seq=%d\n", view, filter, sel, selz, level, last_seq);
    fclose(f);
}
static void load_state(void) {
    char *b = slurp(state_path); if (!b) return;
    for (char *ln = strtok(b, "\n"); ln; ln = strtok(NULL, "\n")) {
        if (!strncmp(ln, "view=", 5)) cp(view, sizeof(view), ln + 5);
        else if (!strncmp(ln, "filter=", 7)) cp(filter, sizeof(filter), ln + 7);
        else if (!strncmp(ln, "sel=", 4)) sel = atoi(ln + 4);
        else if (!strncmp(ln, "selz=", 5)) selz = atoi(ln + 5);
        else if (!strncmp(ln, "level=", 6)) level = atoi(ln + 6);
        else if (!strncmp(ln, "last_seq=", 9)) last_seq = atoi(ln + 9);
    }
    free(b);
}

/* ------------------------------------------------------------- derivations */
static int eff(const Comp *c) { return c->ov ? c->ov : c->mv; }
static int passes(const Comp *c) {
    int e = eff(c);
    if (!strcmp(filter, "OK")) return c->ok == 1;
    if (!strcmp(filter, "BAD")) return c->ok != 1;
    if (!strcmp(filter, "APPROVED")) return e == 1 || e == 2;
    if (!strcmp(filter, "REJECTED")) return e == 3;
    if (!strcmp(filter, "UNREVIEWED")) return e == 0;
    return 1;
}
static void status_text(const Comp *c, char *o, size_t n) {
    if (c->ov) snprintf(o, n, "owner: %s", VERB[c->ov]);
    else if (c->mv) snprintf(o, n, "manager: %s", VERB[c->mv]);
    else snprintf(o, n, "unreviewed");
}
static double laplace(const char *concept, const char *row, int *r, int *p) {
    int rw = 0, pu = 0;
    for (int i = 0; i < nfb; i++) {
        if (strcmp(fbs[i].concept, concept) != 0) continue;
        if (row && strcmp(fbs[i].row, row) != 0) continue;
        if (fbs[i].val > 0) rw++; else pu++;
    }
    *r = rw; *p = pu; return (rw + 1.0) / (rw + pu + 2.0);
}
static int concepts(char out[][40], int max) {
    int n = 0;
    for (int i = 0; i < nfb; i++) { int k; for (k = 0; k < n; k++) if (!strcmp(out[k], fbs[i].concept)) break; if (k == n && n < max) cp(out[n++], 40, fbs[i].concept); }
    return n;
}
static void bar(double w, char *o) {
    int n = (int)floor(fabs(w) * 10 + 0.5); if (n > 10) n = 10;
    o[0] = '['; for (int i = 0; i < 10; i++) o[1 + i] = i < n ? '#' : '-'; o[11] = ']'; o[12] = '\0';
}
static int in_list(const int *a, size_t n, int z) { for (size_t i = 0; i < n; i++) if (a[i] == z) return 1; return 0; }
static const char *tile_color(int z) {
    static const int noble[] = { 2, 10, 18, 36, 54, 86, 118 }, alk[] = { 3, 11, 19, 37, 55, 87 }, ae[] = { 4, 12, 20, 38, 56, 88 },
        hal[] = { 9, 17, 35, 53, 85, 117 }, mtl[] = { 5, 14, 32, 33, 51, 52, 84 }, nm[] = { 1, 6, 7, 8, 15, 16, 34 };
#define IN(a) in_list(a, sizeof(a) / sizeof(a[0]), z)
    if (IN(noble)) return "#b48cd9";
    if (IN(alk)) return "#e07a5f";
    if (IN(ae)) return "#e9b44c";
    if (IN(hal)) return "#6fcf97";
    if (IN(mtl)) return "#8fb8a8";
    if (IN(nm)) return "#7fb5e0";
    if (z >= 57 && z <= 71) return "#d98cb0";
    if (z >= 89 && z <= 103) return "#c26a8c";
    return "#a9b0bd";
}

/* ------------------------------------------------------------------ ui */
typedef struct { char *buf; size_t sz; FILE *f; int n; } Rows;
static void row(Rows *r, const char *cls, int idx, const char *fmt, ...) __attribute__((format(printf, 4, 5)));
static void row(Rows *r, const char *cls, int idx, const char *fmt, ...) {
    char t[900]; va_list ap; va_start(ap, fmt); vsnprintf(t, sizeof(t), fmt, ap); va_end(ap); sanitize(t);
    fprintf(r->f, "l_%d_text=%s\nl_%d_cls=%s\nl_%d_idx=%d\n", r->n, t, r->n, cls, r->n, idx); r->n++;
}
static void wrow(Rows *r, const char *label, const char *w) {
    char b[16]; double v = atof(w); bar(v, b);
    if (w[0]) row(r, "row", -1, "  %-7s %s %+.2f", label, b, v); else row(r, "row", -1, "  %-7s (none)", label);
}
static int weights_field(const char *w, const char *key, char *o, size_t n) {
    const char *p = strstr(w, key); if (!p) { o[0] = '\0'; return 0; }
    p += strlen(key); size_t l = strcspn(p, " "); if (l >= n) l = n - 1; memcpy(o, p, l); o[l] = '\0'; return 1;
}
static void compound_card(Rows *r, const Comp *c) {
    char st[64]; status_text(c, st, sizeof(st));
    row(r, "head-row", -1, "%s %s   (%s, #%d)   %s", c->emoji, c->name, c->id, c->n, st);
    row(r, c->ok == 1 ? "ok-row" : "bad-row", -1, "  formula    %s   [%s]", c->formula, c->ok == 1 ? "formula OK" : "formula BAD");
    row(r, "row", -1, "  atoms      %d", c->atoms); row(r, "row", -1, "  mass no.   %d", c->mass);
    row(r, "row", -1, "  elements   %s", c->elements); row(r, "row", -1, "  kind       %s", c->kind);
    row(r, "row", -1, "  state      %s", c->state); row(r, "row", -1, "  mp / bp    %s / %s", c->mp, c->bp);
    row(r, "row", -1, "  density    %s", c->density); row(r, "row", -1, "  hint       %.200s", c->hint);
    row(r, "head-row", -1, "proposed association (%s, status=%s)", c->has_assoc ? c->proposer : "none", c->has_assoc ? c->pstatus : "-");
    if (c->has_assoc) { row(r, "row", -1, "  %.200s", c->phrase); wrow(r, "energy", c->pe); wrow(r, "force", c->pf); wrow(r, "motion", c->pm); }
    else row(r, "row", -1, "  (no ASSOC row for #%d in draft.txt)", c->n);
    row(r, "head-row", -1, "review");
    if (c->mv) row(r, "row", -1, "  manager review: %s %s (by=%s) %.100s", VERB[c->mv], c->mw, c->mby, c->mwhy); else row(r, "row", -1, "  manager review: none");
    if (c->ov) row(r, "row", -1, "  owner review: %s %s", VERB[c->ov], c->ow); else row(r, "row", -1, "  owner review: none");
    row(r, "row", -1, "  status: %s", st);
    if (c->ov == 2 || (!c->ov && c->mv == 2)) {
        const char *w = c->ov ? c->ow : c->mw; char e[16], f[16], m[16];
        weights_field(w, "energy=", e, sizeof(e)); weights_field(w, "force=", f, sizeof(f)); weights_field(w, "motion=", m, sizeof(m));
        row(r, "head-row", -1, "effective (adjusted by %s)", c->ov ? "owner" : "manager"); wrow(r, "energy", e); wrow(r, "force", f); wrow(r, "motion", m);
    }
    row(r, "head-row", -1, "approved vector (data/hidden/chem_assoc.approved.txt)");
    if (c->has_appr) { row(r, "row", -1, "  by=%s basis=%s", c->aby, c->abasis); wrow(r, "energy", c->ae); wrow(r, "force", c->af); wrow(r, "motion", c->am); }
    else row(r, "row", -1, "  (not approved)");
    row(r, "head-row", -1, "exam scores  Laplace (reward+1)/(reward+punish+2)");
    char cn[8][40]; int k = concepts(cn, 8);
    for (int i = 0; i < k; i++) {
        int rw, pu, drw, dpu; double rs = laplace(cn[i], c->id, &rw, &pu), ds = laplace(cn[i], NULL, &drw, &dpu);
        row(r, "row", -1, "  %-12s this row %.3f (+%d/-%d)   dataset %.3f (+%d/-%d)", cn[i], rs, rw, pu, ds, drw, dpu);
    }
}
static int has_sym(const char *list, const char *sym) {
    size_t sl = strlen(sym);
    for (const char *p = list; *p; ) { size_t l = strcspn(p, ","); if (l == sl && !strncmp(p, sym, sl)) return 1; p += l; if (*p == ',') p++; }
    return 0;
}
static void element_card(Rows *r, const Elem *e) {
    row(r, "head-row", -1, "%s  %s   (Z=%d)", e->sym, e->name, e->z);
    row(r, "row", -1, "  protons    %d", e->p); row(r, "row", -1, "  neutrons   %d", e->nn);
    row(r, "row", -1, "  mass no.   %d", e->mass); row(r, "row", -1, "  electrons  %d", e->z);
    int k = 0; for (int i = 0; i < ncomp; i++) if (has_sym(comps[i].elements, e->sym)) k++;
    row(r, "head-row", -1, "compounds containing %s: %d", e->sym, k);
    int shown = 0; for (int i = 0; i < ncomp && shown < 12; i++) if (has_sym(comps[i].elements, e->sym)) { row(r, "row", -1, "  #%d %s  %s", comps[i].n, comps[i].name, comps[i].formula); shown++; }
    if (k > shown) row(r, "row", -1, "  ... and %d more", k - shown);
}
static void write_ui(void) {
    char tmp[PL]; snprintf(tmp, sizeof(tmp), "%s.tmp", ui_path);
    FILE *f = fopen(tmp, "w"); if (!f) return;
    int ec = !strcmp(view, "elements");
    int nshown = 0; for (int i = 0; i < ncomp; i++) if (passes(&comps[i])) nshown++;
    int nrev = 0; for (int i = 0; i < ncomp; i++) if (eff(&comps[i])) nrev++;
    fprintf(f, "head=Knowledge HQ  -  %d compounds  %d elements  (read-only except owner review)\n", ncomp, nel);
    fprintf(f, "status=%s: %d shown of %d, filter %s, %d reviewed\n", ec ? "elements" : "compounds", ec ? nel : nshown, ec ? nel : ncomp, filter, nrev);
    fprintf(f, "msg=%s\n", msg);
    fprintf(f, "view=%s\nfilter=%s\nlevel=%d\nsel=%d\nselz=%d\nn_total=%d\nn_shown=%d\nn_elements=%d\n", view, filter, level, sel, selz, ncomp, nshown, nel);
    fprintf(f, "cls_comp=%s\ncls_elem=%s\n", ec ? "" : "tab-active", ec ? "tab-active" : "");
    const char *fn[6] = { "ALL", "OK", "BAD", "APPROVED", "REJECTED", "UNREVIEWED" };
    for (int i = 0; i < 6; i++) fprintf(f, "cls_f%d=%s\n", i, (!ec && !strcmp(filter, fn[i])) ? "tab-active" : "");
    fprintf(f, "show_back=%s\nshow_review=%s\n", (level > 0 && !ec) ? "1" : "", (level > 0 && !ec && sel >= 1) ? "1" : "");
    fprintf(f, "show_filters=%s\n", ec ? "" : "1");
    /* dataset exam scores in the notes */
    char cn[8][40]; int k = concepts(cn, 8);
    for (int i = 0; i < 3; i++) {
        char t[200] = "";
        if (i < k) { int rw, pu; double s = laplace(cn[i], NULL, &rw, &pu); snprintf(t, sizeof(t), "exam %s: %.3f  (reward %d, punish %d)", cn[i], s, rw, pu); }
        fprintf(f, "note%d=%s\n", i + 1, t);
    }
    Comp *cur = (sel >= 1 && sel <= ncomp) ? &comps[sel - 1] : NULL;
    char crumb[300] = "";
    if (ec && selz >= 1) { for (int i = 0; i < nel; i++) if (els[i].z == selz) snprintf(crumb, sizeof(crumb), "elements / %s", els[i].name); }
    else if (!ec && level > 0 && cur) snprintf(crumb, sizeof(crumb), "compounds / #%d %s", cur->n, cur->name);
    fprintf(f, "crumb=%s\n", crumb);
    /* element tiles: only in the elements view (same flex-wrap sidebar mechanism as palettes-elements) */
    fprintf(f, "cls_grid=%s\nn_tiles=%d\n", ec ? "on" : "off", ec ? nel : 0);
    if (ec) for (int i = 0; i < nel; i++)
        fprintf(f, "t_%d_sym=%s\nt_%d_z=%d\nt_%d_color=%s\nt_%d_cls=%s\n", i, els[i].sym, i, els[i].z, i, tile_color(els[i].z), i, els[i].z == selz ? "sel" : "");
    Rows r = { 0 }; r.f = open_memstream(&r.buf, &r.sz);
    if (ec) {
        const Elem *e = NULL; for (int i = 0; i < nel; i++) if (els[i].z == selz) e = &els[i];
        if (e) element_card(&r, e); else row(&r, "row", -1, "Pick an element tile to see its card.");
    } else if (level == 0) {
        for (int i = 0; i < ncomp; i++) {
            const Comp *c = &comps[i]; if (!passes(c)) continue;
            char st[64]; status_text(c, st, sizeof(st));
            row(&r, c->ok == 1 ? "ok-row" : "bad-row", c->n, "%02d %s %.24s  %.20s  %s  %s", c->n, c->emoji, c->name, c->formula, c->ok == 1 ? "[OK]" : "[BAD]", st);
        }
        if (!nshown) row(&r, "row", -1, "(no compounds match filter %s)", filter);
    } else if (cur) compound_card(&r, cur);
    fclose(r.f);
    fprintf(f, "n_list=%d\n", r.n); fputs(r.buf, f); free(r.buf); fclose(f); rename(tmp, ui_path);
}

/* ---------------------------------------------------------------- owner review */
static int weight_ok(const char *s, double *v) {
    /* [-]d[.d[d]] with magnitude <= 1 : "0", "-1", "0.5", "0.25", "1.00" ; no sign +, no ".5", no 3rd decimal */
    const char *p = s; if (*p == '-') p++;
    if (*p < '0' || *p > '9') return 0;
    if (p[1] != '\0' && p[1] != '.') return 0;
    if (p[1] == '.') { size_t fl = strlen(p + 2); if (fl < 1 || fl > 2) return 0; for (const char *q = p + 2; *q; q++) if (*q < '0' || *q > '9') return 0; }
    *v = atof(s); return fabs(*v) <= 1.0;
}
static void owner_append(const char *verdict, const char *weights) {
    char p[PL], d[PL]; path_of(p, sizeof(p), "proposals/chem_assoc/review_owner.txt");
    cp(d, sizeof(d), p); char *sl = strrchr(d, '/'); if (sl) { *sl = '\0'; mkdir_p(d); }
    FILE *f = fopen(p, "a"); if (!f) { snprintf(msg, sizeof(msg), "refused: cannot append %s", p); return; }
    if (weights[0]) fprintf(f, "REVIEW | %d | %s | %s | by=owner | via knowledge-hq\n", sel, verdict, weights);
    else fprintf(f, "REVIEW | %d | %s | by=owner | via knowledge-hq\n", sel, verdict);
    fclose(f);
    Comp *c = &comps[sel - 1]; c->ov = !strcmp(verdict, "accept") ? 1 : !strcmp(verdict, "adjust") ? 2 : 3; cp(c->ow, sizeof(c->ow), weights);
    snprintf(msg, sizeof(msg), "owner: %s recorded for #%d %s", verdict, sel, c->name);
}
static void do_review(const char *verdict, const char *arg) {
    if (strcmp(view, "compounds") != 0 || sel < 1 || sel > ncomp) { snprintf(msg, sizeof(msg), "refused: select a compound first"); return; }
    char w[64] = "";
    if (!strcmp(verdict, "adjust")) {
        char a[96]; cp(a, sizeof(a), arg); char *t[3]; int nt = 0; char *s = a;
        int extra = 0;
        while (nt < 3) { t[nt++] = s; char *c = strchr(s, ':'); if (!c) break; *c = '\0'; s = c + 1; if (nt == 3) extra = 1; }
        double v[3];
        if (extra) nt = 4;
        if (nt != 3) { snprintf(msg, sizeof(msg), "refused: ADJUST needs <energy>:<force>:<motion>"); return; }
        const char *nm[3] = { "energy", "force", "motion" };
        for (int i = 0; i < 3; i++) if (!weight_ok(t[i], &v[i])) { snprintf(msg, sizeof(msg), "refused: %s weight '%.20s' must be a decimal in [-1,1] with at most 2 decimals", nm[i], t[i]); return; }
        snprintf(w, sizeof(w), "energy=%.2f force=%.2f motion=%.2f", v[0], v[1], v[2]);
    }
    owner_append(verdict, w);
}

/* ------------------------------------------------------------------ commands */
static void do_cmd(const char *cmd) {
    msg[0] = '\0';
    if (!strncmp(cmd, "SHOW:", 5)) {
        if (!strcmp(cmd + 5, "elements") || !strcmp(cmd + 5, "compounds")) { cp(view, sizeof(view), cmd + 5); level = 0; }
        else snprintf(msg, sizeof(msg), "ignored: unknown view '%.40s'", cmd + 5);
    } else if (!strncmp(cmd, "FILTER:", 7)) {
        static const char *ok[6] = { "ALL", "OK", "BAD", "APPROVED", "REJECTED", "UNREVIEWED" }; int hit = 0;
        for (int i = 0; i < 6; i++) if (!strcmp(cmd + 7, ok[i])) hit = 1;
        if (hit) { cp(filter, sizeof(filter), cmd + 7); cp(view, sizeof(view), "compounds"); level = 0; sel = 0; }
        else snprintf(msg, sizeof(msg), "ignored: unknown filter '%.40s'", cmd + 7);
    } else if (!strncmp(cmd, "SEL:", 4)) {
        int n = atoi(cmd + 4);
        if (n >= 1 && n <= ncomp) { sel = n; level = 1; cp(view, sizeof(view), "compounds"); } else snprintf(msg, sizeof(msg), "ignored: no compound %.20s", cmd + 4);
    } else if (!strncmp(cmd, "SELEL:", 6)) {
        int z = atoi(cmd + 6);
        if (z >= 1 && z <= nel) { selz = z; cp(view, sizeof(view), "elements"); } else snprintf(msg, sizeof(msg), "ignored: no element %.20s", cmd + 6);
    } else if (!strcmp(cmd, "BACK")) { level = 0; }
    else if (!strcmp(cmd, "RELOAD")) { load_all(); snprintf(msg, sizeof(msg), "reloaded"); }
    else if (!strcmp(cmd, "ACCEPT")) do_review("accept", "");
    else if (!strcmp(cmd, "REJECT")) do_review("reject", "");
    else if (!strncmp(cmd, "ADJUST:", 7)) do_review("adjust", cmd + 7);
    else snprintf(msg, sizeof(msg), "ignored: unknown verb '%.60s'", cmd);
}
static int poll_action(void) {
    FILE *f = fopen(act_path, "r"); if (!f) return 0;
    char buf[2048]; size_t nr = fread(buf, 1, sizeof(buf) - 1, f); fclose(f); buf[nr] = '\0';
    int seq = 0; char cmd[512] = "";
    for (char *ls = buf; *ls; ) {
        char *le = strchr(ls, '\n'); size_t ll = le ? (size_t)(le - ls) : strlen(ls);
        if (!strncmp(ls, "seq=", 4)) seq = atoi(ls + 4);
        else if (!strncmp(ls, "cmd=", 4)) { size_t cl = ll - 4; if (cl >= sizeof(cmd)) cl = sizeof(cmd) - 1; memcpy(cmd, ls + 4, cl); cmd[cl] = '\0'; }
        if (!le) break;
        ls = le + 1;
    }
    char *c = trim(cmd);
    if (seq > last_seq && c[0]) { last_seq = seq; do_cmd(c); return 1; }
    return 0;
}
static void on_term(int sig) { (void)sig; _exit(0); }

int main(int argc, char **argv) {
    int once = 0, a = 1;
    if (argc > 1 && !strcmp(argv[1], "--once")) { once = 1; a = 2; }
    if (argc < a + 2) { fprintf(stderr, "Usage: %s [--once] <house_root> <package_dir>\n", argv[0]); return 1; }
    cp(house, sizeof(house), argv[a]); cp(pkg, sizeof(pkg), argv[a + 1]);
    snprintf(cfg_path, sizeof(cfg_path), "%s/knowledge_hq_config.pdl", pkg);
    snprintf(state_path, sizeof(state_path), "%s/state/knowledge_state.txt", pkg);
    snprintf(ui_path, sizeof(ui_path), "%s/knowledge_ui.txt", pkg);
    snprintf(act_path, sizeof(act_path), "%s/knowledge_action.txt", pkg);
    { char sd[PL]; snprintf(sd, sizeof(sd), "%s/state", pkg); mkdir_p(sd); }
    load_config(); load_state(); load_all();
    if (sel > ncomp) sel = 0;
    if (once) { poll_action(); save_state(); write_ui(); return 0; }
    signal(SIGTERM, on_term); signal(SIGINT, on_term); signal(SIGHUP, on_term);
    last_seq = 0;
    { FILE *f = fopen(act_path, "w"); if (f) { fprintf(f, "seq=0\ncmd=\n"); fclose(f); } }
    sig_prev = review_sig(); save_state(); write_ui();
    for (;;) {
        usleep(50000);
        int ch = poll_action();
        long s = review_sig();
        if (s != sig_prev) { sig_prev = s; load_all(); ch = 1; }
        if (ch) { save_state(); write_ui(); }
    }
}
