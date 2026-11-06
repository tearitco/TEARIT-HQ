/* material_colour_op - the generator's material colour, D35 steps M0 and M1 (BOOK 16.game/MATERIAL-ATTENTION-AND-EXPERT-MIX-GENERATOR-DESIGN-2026-11-05.md).
 *   material_colour_op elements <elements.pdl>                          118 lines `t_<Z-1>_color=#rrggbb`, the same text the element palette publishes (the pin for the M0 lift)
 *   material_colour_op check    <materials.pdl> <elements.pdl>          MATERIALCHECK materials=N pigments=M ; exit 2 on any unknown element/pigment, bad fraction, duplicate name
 *   material_colour_op material <materials.pdl> <elements.pdl> <name>   MATERIAL name hex=#rrggbb hsl=H,S,L refl=0.xx
 *   material_colour_op compare  <materials.pdl> <elements.pdl> <ground.pdl>   per material that names a recipe: the hand HSL, the derived HSL and the differences (the honest calibration, not a claim of accuracy)
 * Mixing is by fraction in LINEAR light (gamma 2.2). The renderer appends two extra args (package dir, house root) to actions: extras are ignored. Exit 0 ok, 2 usage/unreadable/invalid.
 * Reads files only; writes stdout only. Build: gcc -std=gnu11 -O2 -Wall -Wextra -o material_colour_op.+x material_colour_op.c -lm */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../../../&.widgits/_shared-lib/material_colour.c"
#define MAXE 120
#define MAXP 64
#define MAXM 128
#define MAXC 8
typedef struct { char sym[4]; int p, n; } El;
typedef struct { char key[40]; int r, g, b; double refl; } Pig;
typedef struct { char name[40], cls[16], recipe[40]; char ck[MAXC][40]; double cf[MAXC]; int nc; } Mat;
static El E[MAXE]; static int nE; static Pig P[MAXP]; static int nP; static Mat M[MAXM]; static int nM;
static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; return s; }
static int split(char *line, char **f, int max) { int n = 0; char *p = line; while (n < max) { f[n++] = p; p = strchr(p, '|'); if (!p) break; *p++ = 0; } for (int i = 0; i < n; i++) f[i] = trim(f[i]); return n; }
static int load_elements(const char *path) {
    FILE *f = fopen(path, "r"); char ln[512], *fl[8]; if (!f) { fprintf(stderr, "cannot read %s\n", path); return 2; }
    while (fgets(ln, sizeof ln, f)) { if (strncmp(ln, "ELEMENT |", 9)) continue; if (split(ln, fl, 8) < 6 || nE >= MAXE) continue; snprintf(E[nE].sym, sizeof E[nE].sym, "%s", fl[2]); E[nE].p = atoi(fl[4]); E[nE].n = atoi(fl[5]); nE++; }
    fclose(f); if (nE != 118) { fprintf(stderr, "elements.pdl has %d ELEMENT rows, expected 118\n", nE); return 2; } return 0;
}
static int find_el(const char *s) { for (int i = 0; i < nE; i++) if (!strcmp(E[i].sym, s)) return i; return -1; }
static int find_pig(const char *k) { for (int i = 0; i < nP; i++) if (!strcmp(P[i].key, k)) return i; return -1; }
static int find_mat(const char *k) { for (int i = 0; i < nM; i++) if (!strcmp(M[i].name, k)) return i; return -1; }
static int load_materials(const char *path) {
    FILE *f = fopen(path, "r"); char ln[1024], *fl[8]; int rc = 0; if (!f) { fprintf(stderr, "cannot read %s\n", path); return 2; }
    while (fgets(ln, sizeof ln, f)) {
        int nf;
        if (!strncmp(ln, "PIGMENT |", 9)) {
            nf = split(ln, fl, 8); if (nf < 4 || nP >= MAXP || strlen(fl[1]) >= sizeof P[0].key || fl[2][0] != '#' || strlen(fl[2]) != 7) { fprintf(stderr, "bad PIGMENT row: %s\n", fl[1]); rc = 2; continue; }
            if (find_pig(fl[1]) >= 0) { fprintf(stderr, "duplicate PIGMENT %s\n", fl[1]); rc = 2; continue; }
            snprintf(P[nP].key, sizeof P[nP].key, "%s", fl[1]); { unsigned v = (unsigned)strtoul(fl[2] + 1, NULL, 16); P[nP].r = v >> 16 & 255; P[nP].g = v >> 8 & 255; P[nP].b = v & 255; }
            P[nP].refl = atof(fl[3]); if (P[nP].refl < 0 || P[nP].refl > 1) { fprintf(stderr, "reflectance outside 0..1 for %s\n", fl[1]); rc = 2; } nP++;
        } else if (!strncmp(ln, "MATERIAL |", 10)) {
            Mat *m; char *c, *sv = NULL; nf = split(ln, fl, 8); if (nf < 5 || nM >= MAXM || strlen(fl[1]) >= sizeof m->name) { fprintf(stderr, "bad MATERIAL row\n"); rc = 2; continue; }
            if (find_mat(fl[1]) >= 0) { fprintf(stderr, "duplicate MATERIAL %s\n", fl[1]); rc = 2; continue; }
            m = &M[nM]; memset(m, 0, sizeof *m); snprintf(m->name, sizeof m->name, "%s", fl[1]); snprintf(m->cls, sizeof m->cls, "%s", fl[2]); snprintf(m->recipe, sizeof m->recipe, "%s", fl[4]);
            for (c = strtok_r(fl[3], ",", &sv); c && m->nc < MAXC; c = strtok_r(NULL, ",", &sv)) { char *co = strchr(c, ':'); if (!co) { fprintf(stderr, "%s: component without a fraction\n", m->name); rc = 2; break; } *co = 0; snprintf(m->ck[m->nc], 40, "%s", trim(c)); m->cf[m->nc] = atof(co + 1); if (m->cf[m->nc] <= 0) { fprintf(stderr, "%s: fraction of %s must be above 0\n", m->name, m->ck[m->nc]); rc = 2; } m->nc++; }
            if (!m->nc) { fprintf(stderr, "%s: no components\n", m->name); rc = 2; } nM++;
        }
    }
    fclose(f); return rc;
}
/* one material -> linear-light mix; returns 0, or 2 with a message when a component has neither a PIGMENT row nor (for metal/alloy) an element symbol */
static int derive(const Mat *m, int *r, int *g, int *b, double *refl) {
    double lr = 0, lg = 0, lb = 0, rf = 0, tot = 0; int i;
    for (i = 0; i < m->nc; i++) {
        int cr, cg, cb; double c_rf, w = m->cf[i]; int pi = find_pig(m->ck[i]), ei;
        if (pi >= 0) { cr = P[pi].r; cg = P[pi].g; cb = P[pi].b; c_rf = P[pi].refl; }
        else if ((!strcmp(m->cls, "metal") || !strcmp(m->cls, "alloy")) && (ei = find_el(m->ck[i])) >= 0) mc_drude(ei + 1, E[ei].p, E[ei].n, &cr, &cg, &cb, &c_rf);
        else { fprintf(stderr, "%s: component %s has no PIGMENT row and is not a metal element\n", m->name, m->ck[i]); return 2; }
        lr += w * pow(cr / 255.0, 2.2); lg += w * pow(cg / 255.0, 2.2); lb += w * pow(cb / 255.0, 2.2); rf += w * c_rf; tot += w;
    }
    *r = mc_clampi(255.0 * pow(lr / tot, 1 / 2.2)); *g = mc_clampi(255.0 * pow(lg / tot, 1 / 2.2)); *b = mc_clampi(255.0 * pow(lb / tot, 1 / 2.2)); *refl = rf / tot; return 0;
}
static void to_hsl(int r, int g, int b, double *h, double *s, double *l) {
    double R = r / 255.0, G = g / 255.0, B = b / 255.0, mx = fmax(R, fmax(G, B)), mn = fmin(R, fmin(G, B)), d = mx - mn; *l = (mx + mn) / 2; *h = 0;
    *s = d < 1e-9 ? 0 : d / (1 - fabs(2 * *l - 1));
    if (d > 1e-9) { if (mx == R) *h = 60 * fmod((G - B) / d + 6, 6); else if (mx == G) *h = 60 * ((B - R) / d + 2); else *h = 60 * ((R - G) / d + 4); }
    *s *= 100; *l *= 100;
}
int main(int argc, char **argv) {
    int i, rc = 0;
    if (argc < 3) { fprintf(stderr, "usage: material_colour_op elements|check|material|compare ...\n"); return 2; }
    if (!strcmp(argv[1], "elements")) {
        if (load_elements(argv[2])) return 2;
        for (i = 0; i < nE; i++) { int r, g, b; mc_drude(i + 1, E[i].p, E[i].n, &r, &g, &b, NULL); printf("t_%d_color=#%02x%02x%02x\n", i, r, g, b); } return 0;
    }
    if (!strcmp(argv[1], "verify")) {    /* verify <elements.pdl> <pin>: the computed 118 colours against a pin file (the M0 lift proof; a wrong pin exits 1) */
        FILE *pf; char ln[128]; int match = 0, bad = 0, seen = 0;
        if (argc < 4 || load_elements(argv[2]) || !(pf = fopen(argv[3], "r"))) { fprintf(stderr, "cannot read inputs\n"); return 2; }
        while (fgets(ln, sizeof ln, pf)) {
            int z, r, g, b; double rf; char want[16], have[16];
            if (sscanf(ln, "t_%d_color=%15s", &z, want) != 2 || z < 0 || z >= nE) continue;
            seen++; mc_drude(z + 1, E[z].p, E[z].n, &r, &g, &b, &rf); snprintf(have, sizeof have, "#%02x%02x%02x", r, g, b);
            if (!strcmp(have, want)) match++; else bad++;
        }
        fclose(pf); printf("ELEMENTS pinned=%d match=%d mismatch=%d\n", seen, match, bad); return (bad || seen != 118) ? 1 : 0;
    }
    if (argc < 4) { fprintf(stderr, "usage: material_colour_op check|material|compare <materials.pdl> <elements.pdl> [name|ground.pdl]\n"); return 2; }
    if (load_elements(argv[3]) || load_materials(argv[2])) return 2;
    if (!strcmp(argv[1], "check")) { for (i = 0; i < nM; i++) { int r, g, b; double rf; if (derive(&M[i], &r, &g, &b, &rf)) rc = 2; } if (!rc) printf("MATERIALCHECK materials=%d pigments=%d\n", nM, nP); return rc; }
    if (!strcmp(argv[1], "material")) {
        int mi, r, g, b; double rf, h, s, l; if (argc < 5 || (mi = find_mat(argv[4])) < 0) { fprintf(stderr, "no such material\n"); return 2; }
        if (derive(&M[mi], &r, &g, &b, &rf)) return 2;
        to_hsl(r, g, b, &h, &s, &l); printf("MATERIAL %s hex=#%02x%02x%02x hsl=%.0f,%.0f,%.0f refl=%.2f\n", M[mi].name, r, g, b, h, s, l, rf); return 0;
    }
    if (!strcmp(argv[1], "compare")) {
        FILE *f; char ln[512], *fl[10]; if (argc < 5 || !(f = fopen(argv[4], "r"))) { fprintf(stderr, "cannot read ground.pdl\n"); return 2; }
        while (fgets(ln, sizeof ln, f)) {
            int nf, mi, r, g, b; double rf, h, s, l, hh, hs, hl; if (strncmp(ln, "RECIPE |", 8)) continue; nf = split(ln, fl, 10); if (nf < 5) continue;
            for (mi = 0; mi < nM; mi++) { if (!strcmp(M[mi].recipe, fl[1])) break; }
            if (mi == nM) continue;
            if (derive(&M[mi], &r, &g, &b, &rf)) { rc = 2; continue; } to_hsl(r, g, b, &h, &s, &l); hh = atof(fl[2]); hs = atof(fl[3]); hl = atof(fl[4]);
            { double dh = fabs(h - hh); if (dh > 180) dh = 360 - dh; printf("COMPARE %-12s hand=%.0f,%.0f,%.0f derived=%.0f,%.0f,%.0f dh=%.0f ds=%.0f dl=%.0f\n", M[mi].name, hh, hs, hl, h, s, l, dh, fabs(s - hs), fabs(l - hl)); }
        }
        fclose(f); return rc;
    }
    fprintf(stderr, "unknown verb %s\n", argv[1]); return 2;
}
