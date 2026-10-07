/* phys_node_eval - evaluate one physics NODE for one BODY (see ../README.md; design: SOLAR-SANDBOX-AND-PLANET-PHYSICS-DESIGN.md sections 3 and 8).
 * Usage: phys_node_eval <dir_with_system.pdl> <body> <node_id> [page]
 * Reads <dir>/physics_tunables.pdl (defaults) then <dir>/system.pdl (CONST, BODY, NODE, LINK, TUNABLE, OVERRIDE).
 * Pull model: an input comes from a LINK (the upstream node is evaluated first, recursively) else from a body property
 * (mass, radius, a, rotation_period, star_mass = parent's mass). So changing a TUNABLE / swapping an impl upstream changes
 * the hash and value of every DOWNSTREAM node evaluated through the links: nothing derived is stored stale.
 * Output rows go to <dir>/data/phys_out.txt (append-only):  OUT | body.node | key | value | impl=<impl> | h=<hex>
 * A row is NOT appended when the latest row for that (body.node, key) already carries the same input hash h
 * (FNV-1a over impl, input values, tunables used, G): cache by input change, never mtime.
 * Everything is evaluated in memory first; any error (unknown impl, missing body/node/input) exits 2 and appends nothing.
 * Exit: 0 ok, 1 usage, 2 error.  Stdout: the OUT rows of every node evaluated, then  STATUS | appended=N cached=M */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAXL 256
#define MAXF 16
#define FLEN 512
typedef struct { int n; char f[MAXF][FLEN]; } Row;
static Row BODY[MAXL], NODE[MAXL], LINK[MAXL], TUN[MAXL], OVR[MAXL];
static int nBODY, nNODE, nLINK, nTUN, nOVR;
static double G; static int haveG;
static char DIR[1024], BODYN[128], PAGE[128];

static void die(const char *fmt, const char *a, const char *b) {
    fprintf(stderr, "error: "); fprintf(stderr, fmt, a, b); fprintf(stderr, "\n"); exit(2);
}
static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0;
    return s;
}
static void load(const char *path, int required) {
    FILE *fp = fopen(path, "r");
    if (!fp) { if (required) die("cannot open %s%s", path, ""); return; }
    char line[4096];
    while (fgets(line, sizeof line, fp)) {
        char *t = trim(line);
        if (!*t || *t == '#') continue;
        Row r; r.n = 0;
        char *p = t;
        while (r.n < MAXF) {
            char *bar = strchr(p, '|');
            if (bar) *bar = 0;
            snprintf(r.f[r.n++], FLEN, "%s", trim(p));
            if (!bar) break;
            p = bar + 1;
        }
        const char *v = r.f[0];
        if (!strcmp(v, "CONST") && r.n >= 3 && !strcmp(r.f[1], "G")) { G = atof(r.f[2]); haveG = 1; }
        else if (!strcmp(v, "BODY") && nBODY < MAXL) BODY[nBODY++] = r;
        else if (!strcmp(v, "NODE") && nNODE < MAXL) NODE[nNODE++] = r;
        else if (!strcmp(v, "LINK") && nLINK < MAXL) LINK[nLINK++] = r;
        else if (!strcmp(v, "TUNABLE") && nTUN < MAXL) TUN[nTUN++] = r;
        else if (!strcmp(v, "OVERRIDE") && nOVR < MAXL) OVR[nOVR++] = r;
    }
    fclose(fp);
}
static const char *kv(const Row *r, int from, const char *key) {   /* "key=value" field lookup */
    size_t k = strlen(key);
    for (int i = from; i < r->n; i++)
        if (!strncmp(r->f[i], key, k) && r->f[i][k] == '=') return r->f[i] + k + 1;
    return NULL;
}
static const Row *find_body(const char *name) {
    for (int i = 0; i < nBODY; i++) if (!strcmp(BODY[i].f[1], name)) return &BODY[i];
    return NULL;
}
static const Row *find_node(const char *id) {
    for (int i = 0; i < nNODE; i++) if (!strcmp(NODE[i].f[1], id)) return &NODE[i];
    return NULL;
}

/* per-run evaluated nodes (memo) */
typedef struct { char id[64]; char impl[64]; int nout; char key[4][64]; double val[4]; unsigned long long h; } Ev;
static Ev EV[64]; static int nEV, depth;

static unsigned long long fnv(unsigned long long h, const char *s) {
    for (; *s; s++) { h ^= (unsigned char)*s; h *= 1099511628211ULL; }
    return h;
}
static unsigned long long HH;        /* hash being accumulated for the node under evaluation */
static void hmix(const char *tag, double v) {
    char b[160]; snprintf(b, sizeof b, "%s=%.17g;", tag, v); HH = fnv(HH, b);
}

/* the override (if any) in force for a node: page > planet > system */
static const Row *pick_override(const char *node) {
    const Row *best = NULL; int bs = 0;
    char pg[160], pl[160];
    snprintf(pg, sizeof pg, "scope=page:%s", PAGE); snprintf(pl, sizeof pl, "scope=planet:%s", BODYN);
    for (int i = 0; i < nOVR; i++) {
        const Row *o = &OVR[i];
        if (o->n < 4 || strcmp(o->f[2], node)) continue;
        int s = 0;
        if (PAGE[0] && !strcmp(o->f[1], pg)) s = 3;
        else if (!strcmp(o->f[1], pl)) s = 2;
        else if (!strcmp(o->f[1], "scope=system")) s = 1;
        if (s && s >= bs) { bs = s; best = o; }   /* equal scope: later row wins */
    }
    return best;
}
static int tunable(const char *node, const Row *ov, const char *key, double *out) {
    const char *s = ov ? kv(ov, 3, key) : NULL;
    if (s) { *out = atof(s); hmix(key, *out); return 1; }
    for (int i = nTUN - 1; i >= 0; i--)      /* later rows win: system.pdl after the defaults file */
        if (TUN[i].n >= 4 && !strcmp(TUN[i].f[1], node) && !strcmp(TUN[i].f[2], key)) {
            *out = atof(TUN[i].f[3]); hmix(key, *out); return 1;
        }
    return 0;
}
static double body_prop(const Row *b, const char *name, int *ok) {
    *ok = 1;
    if (!strcmp(name, "mass")) return atof(b->f[3]);
    if (!strcmp(name, "radius")) return atof(b->f[4]);
    if (!strcmp(name, "a")) return atof(b->f[5]);
    if (!strcmp(name, "rotation_period")) return atof(b->f[6]);
    if (!strcmp(name, "star_mass")) {
        const Row *p = find_body(b->f[2]);
        if (!p) { *ok = 0; return 0; }
        return atof(p->f[3]);
    }
    *ok = 0; return 0;
}
static int eval_node(const char *id);
static double input(const Row *node, const char *name) {
    for (int i = 0; i < nLINK; i++) {          /* LINK | from.out -> to.in */
        char from[128], to[128];
        if (LINK[i].n < 2 || sscanf(LINK[i].f[1], "%127s -> %127s", from, to) != 2) continue;
        char *td = strchr(to, '.'), *fd = strchr(from, '.');
        if (!td || !fd) continue;
        *td = 0; *fd = 0;
        if (strcmp(to, node->f[1]) || strcmp(td + 1, name)) continue;
        int e = eval_node(from);
        for (int k = 0; k < EV[e].nout; k++)
            if (!strcmp(EV[e].key[k], fd + 1)) return EV[e].val[k];
        die("link source %s has no output '%s'", from, fd + 1);
    }
    int ok; const Row *b = find_body(BODYN);
    double v = body_prop(b, name, &ok);
    if (!ok) die("node %s: input '%s' has no link and is not a body property", node->f[1], name);
    return v;
}
static int eval_node(const char *id) {
    for (int i = 0; i < nEV; i++) if (!strcmp(EV[i].id, id)) return i;
    if (++depth > 16) die("link cycle at node %s%s", id, "");
    const Row *n = find_node(id);
    if (!n || n->n < 6) die("no NODE row '%s'%s", id, "");
    const char *kind = n->f[2], *ins = kv(n, 3, "in"), *outs = kv(n, 3, "out");
    const char *impl = kv(n, 3, "impl");
    if (!ins || !outs || !impl) die("NODE %s needs in=, out=, impl=%s", id, "");
    const Row *ov = pick_override(id);
    if (ov && kv(ov, 3, "impl")) impl = kv(ov, 3, "impl");
    /* inputs first (this evaluates upstream nodes; their effect enters our hash through their values) */
    double in[4] = {0, 0, 0, 0}; int nin = 0; char inbuf[256]; snprintf(inbuf, sizeof inbuf, "%s", ins);
    char names[4][64];
    char *sv; for (char *tok = strtok_r(inbuf, ",", &sv); tok && nin < 4; tok = strtok_r(NULL, ",", &sv)) {
        snprintf(names[nin], 64, "%s", tok); in[nin] = input(n, names[nin]); nin++;
    }
    HH = 1469598103934665603ULL;
    HH = fnv(HH, BODYN); HH = fnv(HH, id); HH = fnv(HH, impl); HH = fnv(HH, "v1;");
    hmix("G", G);
    for (int i = 0; i < nin; i++) hmix(names[i], in[i]);
    double p = 0, t;
    if (!strcmp(impl, "constant")) {
        if (!tunable(id, ov, "value", &p)) die("node %s: impl constant needs tunable 'value'%s", id, "");
    } else if (!strcmp(kind, "gravity") && (!strcmp(impl, "newton") || !strcmp(impl, "scaled"))) {
        if (in[1] <= 0) die("node %s: radius must be > 0%s", id, "");
        p = G * in[0] / (in[1] * in[1]);
        if (!strcmp(impl, "scaled")) {
            if (!tunable(id, ov, "factor", &t)) die("node %s: impl scaled needs tunable 'factor'%s", id, "");
            p *= t;
        }
    } else if (!strcmp(kind, "orbit") && !strcmp(impl, "kepler")) {
        if (in[0] <= 0 || in[1] <= 0) die("node %s: body has no orbit (a and star_mass must be > 0)%s", id, "");
        p = 2 * M_PI * sqrt(in[0] * in[0] * in[0] / (G * in[1]));
    } else if (!strcmp(kind, "escape_velocity") && !strcmp(impl, "standard")) {
        p = sqrt(2 * in[0] * in[1]);     /* v = sqrt(2 g R) = sqrt(2 G M / R) when g is newton */
    } else {
        die("unknown impl '%s' for node kind '%s'", impl, kind);
    }
    Ev *e = &EV[nEV];
    snprintf(e->id, 64, "%s", id); snprintf(e->impl, 64, "%s", impl); e->h = HH; e->nout = 0;
    char ob[256]; snprintf(ob, sizeof ob, "%s", outs);
    char *sv2; for (char *tok = strtok_r(ob, ",", &sv2); tok && e->nout < 4; tok = strtok_r(NULL, ",", &sv2)) {
        snprintf(e->key[e->nout], 64, "%s", tok);
        double v = p;
        if (e->nout == 1) v = !strcmp(kind, "orbit") ? p / 86400.0 : !strcmp(kind, "escape_velocity") ? p / 1000.0 : p;
        e->val[e->nout++] = v;
    }
    depth--;
    return nEV++;
}

/* latest recorded hash for (body.node, key) in the ledger, "" if none */
static void last_hash(const char *path, const char *bn, const char *key, char *out) {
    out[0] = 0;
    FILE *fp = fopen(path, "r"); if (!fp) return;
    char line[2048];
    while (fgets(line, sizeof line, fp)) {
        Row r; r.n = 0; char *p = line;
        while (r.n < 6) {
            char *bar = strchr(p, '|'); if (bar) *bar = 0;
            snprintf(r.f[r.n++], FLEN, "%s", trim(p));
            if (!bar) break;
            p = bar + 1;
        }
        if (r.n >= 6 && !strcmp(r.f[0], "OUT") && !strcmp(r.f[1], bn) && !strcmp(r.f[2], key))
            snprintf(out, 64, "%.60s", r.f[5]);
    }
    fclose(fp);
}

int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: phys_node_eval <dir_with_system.pdl> <body> <node_id> [page]\n"); return 1; }
    snprintf(DIR, sizeof DIR, "%s", argv[1]); snprintf(BODYN, sizeof BODYN, "%s", argv[2]);
    snprintf(PAGE, sizeof PAGE, "%s", argc > 4 ? argv[4] : "");
    char path[1200];
    snprintf(path, sizeof path, "%s/physics_tunables.pdl", DIR); load(path, 0);
    snprintf(path, sizeof path, "%s/system.pdl", DIR); load(path, 1);
    if (!haveG) die("system.pdl has no CONST | G row%s%s", "", "");
    const Row *b = find_body(BODYN);
    if (!b || b->n < 7) die("no BODY row '%s'%s", BODYN, "");
    eval_node(argv[3]);
    char data[1100], led[1200];
    snprintf(data, sizeof data, "%s/data", DIR); snprintf(led, sizeof led, "%s/data/phys_out.txt", DIR);
    mkdir(data, 0755);
    FILE *out = NULL; int app = 0, cached = 0;
    for (int i = 0; i < nEV; i++) for (int k = 0; k < EV[i].nout; k++) {
        char bn[200], hx[64], last[64], row[600];
        snprintf(bn, sizeof bn, "%s.%s", BODYN, EV[i].id);
        snprintf(hx, sizeof hx, "h=%016llx", EV[i].h);
        snprintf(row, sizeof row, "OUT | %s | %s | %.6g | impl=%s | %s", bn, EV[i].key[k], EV[i].val[k], EV[i].impl, hx);
        puts(row);
        last_hash(led, bn, EV[i].key[k], last);
        if (!strcmp(last, hx)) { cached++; continue; }
        if (!out && !(out = fopen(led, "a"))) die("cannot append %s%s", led, "");
        fprintf(out, "%s\n", row); app++;
    }
    if (out) fclose(out);
    printf("STATUS | appended=%d cached=%d\n", app, cached);
    return 0;
}
