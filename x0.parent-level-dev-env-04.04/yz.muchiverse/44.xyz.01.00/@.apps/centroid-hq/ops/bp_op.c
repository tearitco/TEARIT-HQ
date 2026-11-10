/* bp_op — one blueprint, one job list (2026-11-10).
 * The skyland file is the authority. CELL rows and JOB rows are projections.
 * The same verbs serve every BUILD id. A chapel-only file with no shared
 * runner is not this op.
 *
 *   bp_op check  <blueprint.pdl>
 *   bp_op list   <blueprint.pdl>
 *   bp_op gen    <blueprint.pdl> <build-id|under:ISLAND> <out.pdl>
 *   bp_op verify <blueprint.pdl> <build-id|under:ISLAND> <out.pdl>
 *   bp_op break  <blueprint.pdl> <void|close|zone> <out.pdl>
 *
 * gen writes one slice. A BUILD id is the mass (floor, walls, door, roof).
 * under:<island> is the taper below that island's top, in the same z
 * unit, shrinking to a point over `depth` layers. Scale is not applied.
 * Neither slice is a second planner. df-hq `place` today is `item x y` with no z. JOB rows
 * carry z so the crew has one column to match. They are not yet fed to
 * df-hq.
 *
 * Exit 0 on a good check or a matching verify. Exit 2 on a broken plan,
 * a short file, or a projection that differs.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <unistd.h>

#define MAXI 32
#define MAXB 64
#define MAXW 64
#define MAXZ 64
#define MAXC 8000
#define LMAX 512

typedef struct { char id[24]; char kind[16]; double cx, cy, rx, ry, top, depth; } Island;
typedef struct { char id[32]; int x, y, w, d, base, wall; char roof[24]; } Build;
typedef struct { int id, x, y, z; char island[24]; } Wp;
typedef struct { int x, y, z; char job[16]; } Cell;

static Island is[MAXI]; static int ni;
static Build bd[MAXB]; static int nb;
static Wp wp[MAXW]; static int nw;
static char zones[MAXZ][8][48]; static int nz;
static int gridw, gridh, scale = 1;
static char msg[240];

static int split(char *s, char **f, int cap) {
    int n = 0; char *p = s;
    while (*p && n < cap) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p || *p == '\n' || *p == '#') break;
        f[n++] = p;
        char *bar = strstr(p, " | ");
        if (!bar) { size_t L = strlen(p); while (L && (p[L - 1] == '\n' || p[L - 1] == '\r')) p[--L] = 0; break; }
        *bar = 0; p = bar + 3;
    }
    return n;
}

static int on_ell(const Island *g, double x, double y) {
    if (g->rx <= 0 || g->ry <= 0) return 0;
    double dx = (x - g->cx) / g->rx, dy = (y - g->cy) / g->ry;
    return dx * dx + dy * dy <= 1.0;
}

static Island *find_i(const char *id) {
    for (int i = 0; i < ni; i++) if (!strcmp(is[i].id, id)) return &is[i];
    return NULL;
}

static int load(const char *path) {
    FILE *f = fopen(path, "r"); char line[LMAX];
    if (!f) { snprintf(msg, sizeof msg, "cannot read %s", path); return 2; }
    ni = nb = nw = nz = 0; gridw = gridh = 0; scale = 1;
    int ln = 0;
    while (fgets(line, sizeof line, f)) {
        ln++; if (line[0] == '#' || line[0] == '\n') continue;
        char *fld[12]; int n = split(line, fld, 12); if (n < 1) continue;
        if (!strcmp(fld[0], "SCALE") && n >= 3) scale = atoi(fld[2]);
        else if (!strcmp(fld[0], "GRID") && n >= 3) { gridw = atoi(fld[1]); gridh = atoi(fld[2]); }
        else if (!strcmp(fld[0], "ISLAND") && n >= 9) {
            if (ni >= MAXI) { snprintf(msg, sizeof msg, "line %d: too many islands", ln); fclose(f); return 2; }
            Island *g = &is[ni++];
            snprintf(g->id, sizeof g->id, "%s", fld[1]); snprintf(g->kind, sizeof g->kind, "%s", fld[2]);
            g->cx = atof(fld[3]); g->cy = atof(fld[4]); g->rx = atof(fld[5]); g->ry = atof(fld[6]);
            g->top = atof(fld[7]); g->depth = atof(fld[8]);
        } else if (!strcmp(fld[0], "BUILD") && n >= 10) {
            if (nb >= MAXB) { snprintf(msg, sizeof msg, "line %d: too many buildings", ln); fclose(f); return 2; }
            Build *b = &bd[nb++];
            snprintf(b->id, sizeof b->id, "%s", fld[1]);
            b->x = atoi(fld[3]); b->y = atoi(fld[4]); b->w = atoi(fld[5]); b->d = atoi(fld[6]);
            b->base = atoi(fld[7]); b->wall = atoi(fld[8]); snprintf(b->roof, sizeof b->roof, "%s", fld[9]);
        } else if (!strcmp(fld[0], "WAYPOINT") && n >= 7) {
            if (nw >= MAXW) { snprintf(msg, sizeof msg, "line %d: too many waypoints", ln); fclose(f); return 2; }
            wp[nw].id = atoi(fld[1]); wp[nw].x = atoi(fld[3]); wp[nw].y = atoi(fld[4]); wp[nw].z = atoi(fld[5]);
            snprintf(wp[nw].island, sizeof wp[nw].island, "%s", fld[6]); nw++;
        } else if (!strcmp(fld[0], "ZONE") && n >= 3) {
            if (nz >= MAXZ) { snprintf(msg, sizeof msg, "line %d: too many zones", ln); fclose(f); return 2; }
            snprintf(zones[nz][0], 48, "%s", fld[1]); snprintf(zones[nz][1], 48, "%s", fld[2]); nz++;
        }
    }
    fclose(f);
    if (gridw < 1 || gridh < 1 || scale < 1) { snprintf(msg, sizeof msg, "GRID and SCALE are required"); return 2; }
    return 0;
}

static int check(void) {
    if (nw < 16) { snprintf(msg, sizeof msg, "need waypoints 0 to 15, found %d", nw); return 2; }
    int seen[16] = {0};
    for (int i = 0; i < nw; i++) {
        if (wp[i].id < 0 || wp[i].id > 15) { snprintf(msg, sizeof msg, "waypoint %d out of 0..15", wp[i].id); return 2; }
        seen[wp[i].id] = 1;
        Island *g = find_i(wp[i].island);
        if (!g) { snprintf(msg, sizeof msg, "waypoint %d names a missing island", wp[i].id); return 2; }
        if (!on_ell(g, wp[i].x + 0.5, wp[i].y + 0.5)) { snprintf(msg, sizeof msg, "waypoint %d outside island %s", wp[i].id, g->id); return 2; }
    }
    for (int i = 0; i < 16; i++) if (!seen[i]) { snprintf(msg, sizeof msg, "waypoint %d missing", i); return 2; }
    for (int i = 0; i < nb; i++) {
        double cx = bd[i].x + bd[i].w / 2.0, cy = bd[i].y + bd[i].d / 2.0;
        Island *host = NULL;
        for (int k = 0; k < ni; k++) if (on_ell(&is[k], cx, cy)) { host = &is[k]; break; }
        if (!host) { snprintf(msg, sizeof msg, "building %s is off every island", bd[i].id); return 2; }
        if (!strcmp(host->kind, "small")) { snprintf(msg, sizeof msg, "building %s sits on small island %s", bd[i].id, host->id); return 2; }
        if (bd[i].w < 1 || bd[i].d < 1 || bd[i].wall < 1) { snprintf(msg, sizeof msg, "building %s has an empty mass", bd[i].id); return 2; }
    }
    for (int i = 0; i < nz; i++) {
        Island *g = find_i(zones[i][1]);
        if (!g) { snprintf(msg, sizeof msg, "zone %s names a missing island", zones[i][0]); return 2; }
        if (!strcmp(g->kind, "small")) { snprintf(msg, sizeof msg, "zone %s sits on small island %s", zones[i][0], g->id); return 2; }
    }
    for (int a = 0; a < ni; a++) if (!strcmp(is[a].kind, "small")) {
        for (int b = 0; b < ni; b++) if (strcmp(is[b].kind, "small")) {
            if (on_ell(&is[b], is[a].cx, is[a].cy)) {
                snprintf(msg, sizeof msg, "small island %s too close to %s", is[a].id, is[b].id); return 2;
            }
        }
    }
    return 0;
}

static int add_cell(Cell *c, int *n, int x, int y, int z, const char *job) {
    for (int i = 0; i < *n; i++) if (c[i].x == x && c[i].y == y && c[i].z == z) {
        snprintf(c[i].job, sizeof c[i].job, "%s", job); return 0;
    }
    if (*n >= MAXC) return 2;
    c[*n].x = x; c[*n].y = y; c[*n].z = z; snprintf(c[*n].job, 16, "%s", job); (*n)++;
    return 0;
}

/* The underside only. Layer k is a smaller ellipse; the last layer is the point. */
static int gen_under(const char *id, Cell *c, int *n) {
    Island *g = find_i(id);
    if (!g) { snprintf(msg, sizeof msg, "no island %s", id); return 2; }
    int depth = (int)g->depth; int top = (int)g->top;
    if (depth < 1) { snprintf(msg, sizeof msg, "island %s has no underside", id); return 2; }
    *n = 0;
    for (int k = 1; k <= depth; k++) {
        int z = top - k;
        double f = 1.0 - (double)k / (double)depth;
        if (f <= 0.0) {
            if (add_cell(c, n, (int)floor(g->cx), (int)floor(g->cy), z, "earth")) {
                snprintf(msg, sizeof msg, "too many underside cells"); return 2;
            }
            continue;
        }
        Island slice = *g; slice.rx = g->rx * f; slice.ry = g->ry * f;
        int x0 = (int)floor(g->cx - g->rx) - 1, x1 = (int)ceil(g->cx + g->rx) + 1;
        int y0 = (int)floor(g->cy - g->ry) - 1, y1 = (int)ceil(g->cy + g->ry) + 1;
        int before = *n;
        for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
            if (!on_ell(&slice, x + 0.5, y + 0.5)) continue;
            if (add_cell(c, n, x, y, z, "earth")) { snprintf(msg, sizeof msg, "too many underside cells"); return 2; }
        }
        if (*n == before && add_cell(c, n, (int)floor(g->cx), (int)floor(g->cy), z, "earth")) {
            snprintf(msg, sizeof msg, "too many underside cells"); return 2;
        }
    }
    if (!*n) { snprintf(msg, sizeof msg, "underside of %s is empty", id); return 2; }
    return 0;
}

static int gen_build(const char *id, Cell *c, int *n) {
    Build *b = NULL; for (int i = 0; i < nb; i++) if (!strcmp(bd[i].id, id)) b = &bd[i];
    if (!b) { snprintf(msg, sizeof msg, "no building %s", id); return 2; }
    *n = 0;
    int doorx = b->x + b->w / 2, doory = b->y + b->d - 1;
    for (int y = b->y; y < b->y + b->d; y++) for (int x = b->x; x < b->x + b->w; x++) {
        if (add_cell(c, n, x, y, b->base, "stone")) return 2;
        int edge = (x == b->x || y == b->y || x == b->x + b->w - 1 || y == b->y + b->d - 1);
        if (!edge) continue;
        for (int z = 1; z < b->wall; z++) {
            const char *job = (z == 1 && x == doorx && y == doory) ? "door" : "stone";
            if (add_cell(c, n, x, y, b->base + z, job)) return 2;
        }
        if (add_cell(c, n, x, y, b->base + b->wall, "roof")) return 2;
    }
    return 0;
}

static int gen_slice(const char *id, Cell *c, int *n) {
    if (!strncmp(id, "under:", 6)) return gen_under(id + 6, c, n);
    return gen_build(id, c, n);
}

static int cmp_cell(const void *a, const void *b) {
    const Cell *A = a, *B = b;
    if (A->z != B->z) return A->z - B->z;
    if (A->y != B->y) return A->y - B->y;
    return A->x - B->x;
}

static int write_proj(const char *path, const char *id, Cell *c, int n) {
    qsort(c, (size_t)n, sizeof *c, cmp_cell);
    FILE *f = fopen(path, "w"); if (!f) return 2;
    fprintf(f, "# projection of BUILD %s. Not an authority. Regenerate with bp_op gen.\n", id);
    fprintf(f, "# JOB rows are the crew list. df-hq place does not read z yet.\n");
    for (int i = 0; i < n; i++) fprintf(f, "CELL | %d | %d | %d | %s\n", c[i].x, c[i].y, c[i].z, c[i].job);
    for (int i = 0; i < n; i++) fprintf(f, "JOB | place | %d | %d | %d | %s\n", c[i].x, c[i].y, c[i].z, c[i].job);
    fclose(f);
    return 0;
}

static int files_match(const char *a, const char *b) {
    FILE *fa = fopen(a, "r"), *fb = fopen(b, "r");
    if (!fa || !fb) { if (fa) fclose(fa); if (fb) fclose(fb); return 0; }
    int ca, cb, ok = 1; while (ok && (ca = fgetc(fa)) != EOF) { cb = fgetc(fb); if (ca != cb) ok = 0; }
    if (ok && fgetc(fb) != EOF) ok = 0;
    fclose(fa); fclose(fb); return ok;
}

static int do_break(const char *kind, const char *in, const char *out) {
    FILE *fi = fopen(in, "r"), *fo = fopen(out, "w"); char line[LMAX];
    if (!fi || !fo) { snprintf(msg, sizeof msg, "break cannot copy"); return 2; }
    int punched = 0;
    while (fgets(line, sizeof line, fi)) {
        if (!punched && !strcmp(kind, "void") && !strncmp(line, "WAYPOINT | 9 |", 13)) {
            fprintf(fo, "WAYPOINT | 9 | moved | 0 | 0 | 10 | P | mutant outside the plateau\n"); punched = 1; continue;
        }
        if (!punched && !strcmp(kind, "close") && !strncmp(line, "ISLAND | S1 |", 12)) {
            fprintf(fo, "ISLAND | S1 | small | 22 | 35 | 5 | 3.5 | 0 | 7 | mutant on the forest lobe\n"); punched = 1; continue;
        }
        fputs(line, fo);
    }
    if (!strcmp(kind, "zone")) { fprintf(fo, "ZONE | mutant | S1 | ruin | ell | 10 | 12 | 1 | 1 | not allowed\n"); punched = 1; }
    fclose(fi); fclose(fo);
    if (!punched) { snprintf(msg, sizeof msg, "break kind %s changed nothing", kind); return 2; }
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: bp_op check|list <blueprint> | gen|verify <blueprint> <build-id> <out> | break <blueprint> <void|close|zone> <out>\n");
        return 2;
    }
    const char *cmd = argv[1];
    if (!strcmp(cmd, "break")) {
        if (argc != 5) return 2;
        int rc = do_break(argv[3], argv[2], argv[4]);
        if (rc) fprintf(stderr, "%s\n", msg);
        return rc;
    }
    int rc = load(argv[2]); if (rc) { fprintf(stderr, "%s\n", msg); return rc; }
    rc = check();
    if (!strcmp(cmd, "check")) {
        if (rc) { fprintf(stderr, "%s\n", msg); return rc; }
        printf("BLUEPRINT islands=%d buildings=%d waypoints=%d zones=%d scale=%d grid=%dx%d\n", ni, nb, nw, nz, scale, gridw, gridh);
        return 0;
    }
    if (rc) { fprintf(stderr, "%s\n", msg); return rc; }
    if (!strcmp(cmd, "count") && argc == 3) {
        int columns = 0, volume = 0, bcells = 0;
        for (int y = 0; y < gridh; y++) for (int x = 0; x < gridw; x++) {
            int host = -1;
            for (int k = 0; k < ni; k++) if (on_ell(&is[k], x + 0.5, y + 0.5)) { host = k; break; }
            if (host < 0) continue;
            columns++;
            volume += (int)is[host].depth;
        }
        Cell cells[MAXC];
        for (int i = 0; i < nb; i++) {
            int n = 0;
            if (gen_build(bd[i].id, cells, &n)) { fprintf(stderr, "%s\n", msg); return 2; }
            bcells += n;
        }
        printf("LAND columns=%d volume=%d buildings=%d scale_not_applied=%d\n", columns, volume, bcells, scale);
        return 0;
    }
    if (!strcmp(cmd, "list")) {
        for (int i = 0; i < nb; i++) printf("BUILD %s %d %d %dx%d base=%d wall=%d\n", bd[i].id, bd[i].x, bd[i].y, bd[i].w, bd[i].d, bd[i].base, bd[i].wall);
        printf("RUNNER one gen for every BUILD above\n");
        return 0;
    }
    if ((!strcmp(cmd, "gen") || !strcmp(cmd, "verify")) && argc == 5) {
        Cell cells[MAXC]; int n = 0;
        if (gen_slice(argv[3], cells, &n)) { fprintf(stderr, "%s\n", msg); return 2; }
        if (!strcmp(cmd, "gen")) {
            if (write_proj(argv[4], argv[3], cells, n)) return 2;
            printf("PROJ build=%s cells=%d jobs=%d\n", argv[3], n, n);
            return 0;
        }
        char tmp[] = "/tmp/bp-verify-XXXXXX"; int fd = mkstemp(tmp); if (fd < 0) return 2; close(fd);
        if (write_proj(tmp, argv[3], cells, n)) return 2;
        int same = files_match(tmp, argv[4]); remove(tmp);
        if (!same) { fprintf(stderr, "projection differs from %s\n", argv[4]); return 2; }
        printf("VERIFY build=%s cells=%d\n", argv[3], n);
        return 0;
    }
    fprintf(stderr, "usage: bp_op check|list|gen|verify|break ...\n");
    return 2;
}
