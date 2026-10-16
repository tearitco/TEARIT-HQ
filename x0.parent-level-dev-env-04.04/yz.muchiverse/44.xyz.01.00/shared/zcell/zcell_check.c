/* zcell_check - parse and validate a z-cell file (format: BOOK 16.game/Z-CELL-FORMAT.md). Usage: zcell_check <file.pdl> [brixels.pdl]
 * Prints per-world, per-z cell counts. Exit 0 = valid, 1 = at least one ERR line, 2 = cannot open. Unknown brixel ids are WARN, not ERR. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MW 32
#define MX 64
typedef struct { char id[24], kind[12]; int w, h; } World;
typedef struct { char w[24]; int z, span; long alt; char scale[32]; } Level;
typedef struct { char w[24]; char ch; char name[40]; char fl[40]; } Leg;
typedef struct { char w[24]; int z, y; char row[MX + 1]; } Row;
typedef struct { char w[24]; int z, x, y, tz, tx, ty; char kind[12]; } Link;
typedef struct { char t[8]; char w[24]; int z, x, y, n, ln; char a[32], b[12], c[12]; } Thing;
static Thing T[1024]; static int nT;
static World W[MW]; static Level L[256]; static Leg G[512]; static Row R[2048]; static Link K[128];
static int nW, nL, nG, nR, nK, errs, warns;
static void err(int ln, const char *m, const char *a) { printf("ERR line %d: %s %s\n", ln, m, a ? a : ""); errs++; }
static int split(char *s, char **f, int max) { int n = 0; f[n++] = s; for (char *p = s; *p && n < max; p++) if (*p == '|') { *p = 0; f[n++] = p + 1; } for (int i = 0; i < n; i++) { while (*f[i] == ' ') f[i]++; char *e = f[i] + strlen(f[i]); while (e > f[i] && (e[-1] == ' ' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; } return n; }
static World *fw(const char *id) { for (int i = 0; i < nW; i++) if (!strcmp(W[i].id, id)) return &W[i]; return 0; }
static Level *fl(const char *w, int z) { for (int i = 0; i < nL; i++) if (L[i].z == z && !strcmp(L[i].w, w)) return &L[i]; return 0; }
static Leg *fg(const char *w, char c) { for (int i = 0; i < nG; i++) if (G[i].ch == c && !strcmp(G[i].w, w)) return &G[i]; return 0; }
static Row *fr(const char *w, int z, int y) { for (int i = 0; i < nR; i++) if (R[i].z == z && R[i].y == y && !strcmp(R[i].w, w)) return &R[i]; return 0; }
static int hasflag(Leg *g, const char *f) { char b[40]; snprintf(b, sizeof b, ",%s,", g->fl); char t[40]; snprintf(t, sizeof t, ",%s,", f); return strstr(b, t) != 0; }
static Leg *cell(const char *w, int z, int x, int y) { Row *r = fr(w, z, y); if (!r || x < 0 || x >= (int)strlen(r->row)) return 0; return fg(w, r->row[x]); }
static int brix_known(const char *bf, const char *id) { if (!bf) return 1; FILE *f = fopen(bf, "r"); if (!f) return 1; char ln[512]; int ok = 0; while (fgets(ln, sizeof ln, f)) { char *x[4]; char c[512]; snprintf(c, sizeof c, "%s", ln); if (split(c, x, 4) >= 2 && !strcmp(x[0], "BRIXEL") && !strcmp(x[1], id)) { ok = 1; break; } } fclose(f); return ok; }
int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: zcell_check file.pdl [brixels.pdl]\n"); return 2; }
    FILE *fp = fopen(argv[1], "r"); if (!fp) { perror(argv[1]); return 2; }
    char line[1024], *f[12]; int ln = 0; struct { char w[24], v[24]; int z, x, y, rot; int ln; } P[32]; int nP = 0; struct { char w[24], reg[24]; int z, x, y, span, ln; } Z[32]; int nZ = 0; struct { char v[24]; int x, y, ln; } S[32]; int nS = 0;
    while (fgets(line, sizeof line, fp)) {
        ln++; if (line[0] == '#' || line[0] == '\n' || !strncmp(line, "SECTION", 7) || line[0] == '-') continue;
        int n = split(line, f, 12);
        if (!strcmp(f[0], "ZWORLD") && n >= 5) { if (fw(f[1])) err(ln, "duplicate world", f[1]); else if (nW < MW) { World *w = &W[nW++]; snprintf(w->id, 24, "%s", f[1]); snprintf(w->kind, 12, "%s", f[2]); w->w = atoi(f[3]); w->h = atoi(f[4]); if (w->w < 1 || w->h < 1 || w->w >= MX) err(ln, "bad size for", f[1]); if (strcmp(w->kind, "building") && strcmp(w->kind, "column") && strcmp(w->kind, "vehicle")) err(ln, "bad kind", w->kind); } }
        else if (!strcmp(f[0], "ZLEVEL") && n >= 7) { if (!fw(f[1])) { err(ln, "level for unknown world", f[1]); continue; } Level *l = &L[nL++]; snprintf(l->w, 24, "%s", f[1]); l->z = atoi(f[2]); l->span = atoi(f[3]); l->alt = atol(f[4]); snprintf(l->scale, 32, "%s", f[5]); if (l->span < 1 || (l->span & (l->span - 1))) err(ln, "span is not a power of 2:", f[3]); }
        else if (!strcmp(f[0], "ZLEGEND") && n >= 5) { if (!fw(f[1])) { err(ln, "legend for unknown world", f[1]); continue; } if (strlen(f[2]) != 1) { err(ln, "legend char must be 1 char:", f[2]); continue; } if (fg(f[1], f[2][0])) { err(ln, "duplicate legend char", f[2]); continue; } Leg *g = &G[nG++]; snprintf(g->w, 24, "%s", f[1]); g->ch = f[2][0]; snprintf(g->name, 40, "%s", f[3]); snprintf(g->fl, 40, "%s", f[4]); }
        else if (!strcmp(f[0], "ZGRID") && n >= 5) { World *w = fw(f[1]); if (!w) { err(ln, "grid for unknown world", f[1]); continue; } Row *r = &R[nR++]; snprintf(r->w, 24, "%s", f[1]); r->z = atoi(f[2]); r->y = atoi(f[3]); snprintf(r->row, MX + 1, "%s", f[4]); if ((int)strlen(r->row) != w->w) err(ln, "row width differs from world width in", f[1]); if (r->y < 0 || r->y >= w->h) err(ln, "row y out of range in", f[1]); if (!fl(f[1], r->z)) err(ln, "grid row for a z with no ZLEVEL in", f[1]); for (int i = 0; r->row[i]; i++) if (!fg(f[1], r->row[i])) { char b[8] = { r->row[i], 0 }; err(ln, "char not in legend:", b); break; } }
        else if (!strcmp(f[0], "ZLINK") && n >= 9) { Link *k = &K[nK++]; snprintf(k->w, 24, "%s", f[1]); k->z = atoi(f[2]); k->x = atoi(f[3]); k->y = atoi(f[4]); snprintf(k->kind, 12, "%s", f[5]); k->tz = atoi(f[6]); k->tx = atoi(f[7]); k->ty = atoi(f[8]); if (!fw(k->w)) err(ln, "link in unknown world", k->w); if (k->tz == k->z) err(ln, "link does not change z in", k->w); if (abs(k->tz - k->z) != 1) err(ln, "link must change z by exactly 1 in", k->w); if (strcmp(k->kind, "up") && strcmp(k->kind, "down") && strcmp(k->kind, "ladder") && strcmp(k->kind, "ramp")) err(ln, "bad link kind:", k->kind); if (!strcmp(k->kind, "up") && k->tz < k->z) err(ln, "link kind up goes down in", k->w); if (!strcmp(k->kind, "down") && k->tz > k->z) err(ln, "link kind down goes up in", k->w); if (strcmp(k->kind, "ramp") && (k->tx != k->x || k->ty != k->y)) err(ln, "only a ramp may change x,y in", k->w); if (abs(k->tx - k->x) > 1 || abs(k->ty - k->y) > 1) err(ln, "link offset larger than 1 cell in", k->w); }
        else if (!strcmp(f[0], "ZREGION") && n >= 7) { if (nZ < 32) { Z[nZ].ln = ln; snprintf(Z[nZ].w, 24, "%s", f[1]); Z[nZ].z = atoi(f[2]); Z[nZ].x = atoi(f[3]); Z[nZ].y = atoi(f[4]); Z[nZ].span = atoi(f[5]); snprintf(Z[nZ].reg, 24, "%s", f[6]); nZ++; } }
        else if (!strcmp(f[0], "ZPLACE") && n >= 7) { if (nP < 32) { P[nP].ln = ln; snprintf(P[nP].w, 24, "%s", f[1]); snprintf(P[nP].v, 24, "%s", f[2]); P[nP].z = atoi(f[3]); P[nP].x = atoi(f[4]); P[nP].y = atoi(f[5]); P[nP].rot = atoi(f[6]); nP++; } }
        else if (!strcmp(f[0], "ZSEAT") && n >= 5) { if (nS < 32) { S[nS].ln = ln; snprintf(S[nS].v, 24, "%s", f[1]); S[nS].x = atoi(f[2]); S[nS].y = atoi(f[3]); nS++; } }
        else if ((!strcmp(f[0], "ZFURN") && n >= 8) || (!strcmp(f[0], "ZITEM") && n >= 7) || (!strcmp(f[0], "ZFIELD") && n >= 7) || (!strcmp(f[0], "ZTRAP") && n >= 6) || (!strcmp(f[0], "ZJOB") && n >= 7)) {
            if (nT >= 1024) continue; Thing *t = &T[nT++]; memset(t, 0, sizeof *t); t->ln = ln; snprintf(t->t, 8, "%s", f[0] + 1); snprintf(t->w, 24, "%s", f[1]); t->z = atoi(f[2]); t->x = atoi(f[3]); t->y = atoi(f[4]); snprintf(t->a, 32, "%s", f[5]);
            if (!strcmp(f[0], "ZFURN")) { snprintf(t->b, 12, "%s", f[6]); snprintf(t->c, 12, "%s", f[7]); } else if (!strcmp(f[0], "ZITEM") || !strcmp(f[0], "ZFIELD")) t->n = atoi(f[6]); else if (!strcmp(f[0], "ZJOB")) { t->n = atoi(f[6]); } }
        else if (f[0][0] == 'Z') err(ln, "unknown or short row:", f[0]);
    }
    fclose(fp);
    for (int i = 0; i < nW; i++) {      /* every level has every row; z levels are contiguous; altitude rises with z */
        World *w = &W[i]; int zmin = 9999, zmax = -9999, c = 0; for (int j = 0; j < nL; j++) if (!strcmp(L[j].w, w->id)) { c++; if (L[j].z < zmin) zmin = L[j].z; if (L[j].z > zmax) zmax = L[j].z; }
        if (!c) { err(0, "world has no ZLEVEL:", w->id); continue; }
        if (zmax - zmin + 1 != c) err(0, "z levels are not contiguous in", w->id);
        for (int z = zmin; z <= zmax; z++) { Level *l = fl(w->id, z), *p = fl(w->id, z - 1); if (!l) continue; if (p && l->alt <= p->alt) err(0, "altitude does not rise with z in", w->id); if (p && l->span < p->span) err(0, "span shrinks going up in", w->id); for (int y = 0; y < w->h; y++) if (!fr(w->id, z, y)) { char b[64]; snprintf(b, sizeof b, "%s z=%d y=%d", w->id, z, y); err(0, "missing grid row", b); } }
    }
    for (int i = 0; i < nK; i++) {      /* stairs: both ends are stair cells, and the far end links back */
        Link *k = &K[i]; Leg *a = cell(k->w, k->z, k->x, k->y), *b = cell(k->w, k->tz, k->tx, k->ty); char m[96]; snprintf(m, sizeof m, "%s z=%d (%d,%d)->z=%d (%d,%d)", k->w, k->z, k->x, k->y, k->tz, k->tx, k->ty);
        if (!a || !hasflag(a, "stair")) { err(0, "link start is not a stair cell:", m); continue; } if (!b || !hasflag(b, "stair")) { err(0, "link end is not a stair cell:", m); continue; }
        int back = 0; for (int j = 0; j < nK; j++) if (!strcmp(K[j].w, k->w) && K[j].z == k->tz && K[j].x == k->tx && K[j].y == k->ty && K[j].tz == k->z && K[j].tx == k->x && K[j].ty == k->y) back = 1;
        if (!back) err(0, "link has no reciprocal:", m);
    }
    for (int i = 0; i < nZ; i++) { World *rw = fw(Z[i].reg); if (!rw) { err(Z[i].ln, "region refers to unknown world", Z[i].reg); continue; } Level *l = fl(Z[i].w, Z[i].z); if (!l || !cell(Z[i].w, Z[i].z, Z[i].x, Z[i].y)) err(Z[i].ln, "region cell is outside its grid in", Z[i].w); else if (Z[i].span < l->span) err(Z[i].ln, "region span is finer than its level in", Z[i].w); }
    for (int i = 0; i < nT; i++) {      /* per-cell rows: keyed by world,z,x,y; the cell must exist and suit the row */
        Thing *t = &T[i]; Leg *g = cell(t->w, t->z, t->x, t->y); char m[64]; snprintf(m, sizeof m, "%.20s z=%d (%d,%d)", t->w, t->z, t->x, t->y);
        if (!fw(t->w) || !g) { err(t->ln, "cell row points outside its grid:", m); continue; }
        if (!strcmp(t->t, "FURN")) { if (hasflag(g, "solid")) err(t->ln, "furniture on a solid cell:", m); if (strcmp(t->b, "-") && strcmp(t->b, "open") && strcmp(t->b, "closed")) err(t->ln, "bad furniture state:", t->b); if (strcmp(t->c, "-") && strcmp(t->c, "n") && strcmp(t->c, "e") && strcmp(t->c, "s") && strcmp(t->c, "w")) err(t->ln, "bad facing:", t->c); }
        else if (!strcmp(t->t, "ITEM") && t->n < 1) err(t->ln, "item count must be 1 or more:", m);
        else if (!strcmp(t->t, "FIELD") && (t->n < 1 || t->n > 10)) err(t->ln, "field strength must be 1..10:", m);
        else if (!strcmp(t->t, "JOB")) { if (strcmp(t->a, "dig") && strcmp(t->a, "channel") && strcmp(t->a, "smooth")) err(t->ln, "unknown job:", t->a); else if (!strcmp(t->a, "dig") && !hasflag(g, "solid")) err(t->ln, "dig order on a cell that is not solid:", m); else if (strcmp(t->a, "dig") && !hasflag(g, "walk")) err(t->ln, "job needs a walkable cell:", m); }
    }
    for (int i = 0; i < nP; i++) {      /* a vehicle's own grid keeps span 1; in a coarser level its footprint rounds up to whole cells */
        World *v = fw(P[i].v), *w = fw(P[i].w); if (!v || strcmp(v->kind, "vehicle")) { err(P[i].ln, "place: not a vehicle world:", P[i].v); continue; } if (!w) { err(P[i].ln, "place: unknown world", P[i].w); continue; }
        if (P[i].rot % 90) { char b[16]; snprintf(b, sizeof b, "%d", P[i].rot); err(P[i].ln, "rotation must be a multiple of 90 in v1:", b); continue; }
        Level *l = fl(P[i].w, P[i].z); if (!l) { err(P[i].ln, "place: no such z in", P[i].w); continue; }
        int quarter = (P[i].rot / 90) % 4; if (quarter < 0) quarter += 4; int fw_ = quarter & 1 ? v->h : v->w, fh = quarter & 1 ? v->w : v->h; int cx = (fw_ + l->span - 1) / l->span, cy = (fh + l->span - 1) / l->span;
        if (P[i].x < 0 || P[i].y < 0 || P[i].x + cx > w->w || P[i].y + cy > w->h) err(P[i].ln, "place: vehicle footprint leaves the grid in", P[i].w);
        else printf("place %s in %s z=%d rot=%d: footprint %dx%d vehicle cells -> %dx%d cells at span %d\n", P[i].v, P[i].w, P[i].z, P[i].rot, fw_, fh, cx, cy, l->span);
    }
    for (int i = 0; i < nS; i++) { Leg *g = cell(S[i].v, 0, S[i].x, S[i].y); if (!g || !hasflag(g, "seat")) err(S[i].ln, "seat cell has no seat flag in", S[i].v); }
    for (int i = 0; i < nW; i++) {
        World *w = &W[i]; printf("world %s (%s %dx%d)\n", w->id, w->kind, w->w, w->h);
        for (int j = 0; j < nL; j++) { if (strcmp(L[j].w, w->id)) continue; int cnt[6] = { 0 }; for (int y = 0; y < w->h; y++) { Row *r = fr(w->id, L[j].z, y); if (!r) continue; for (int x = 0; r->row[x]; x++) { Leg *g = fg(w->id, r->row[x]); if (!g) continue; cnt[0]++; if (hasflag(g, "walk")) cnt[1]++; if (hasflag(g, "solid")) cnt[2]++; if (hasflag(g, "stair")) cnt[3]++; if (hasflag(g, "air")) cnt[4]++; if (hasflag(g, "seat")) cnt[5]++; } }
            printf("  z=%-3d span=%-3d alt=%-7ld %-14s cells=%d walk=%d solid=%d stair=%d air=%d seat=%d\n", L[j].z, L[j].span, L[j].alt, L[j].scale, cnt[0], cnt[1], cnt[2], cnt[3], cnt[4], cnt[5]); }
        if (!strcmp(w->kind, "vehicle")) for (int y = 0; y < w->h; y++) { Row *r = fr(w->id, 0, y); for (int x = 0; r && r->row[x]; x++) { Leg *g = fg(w->id, r->row[x]); if (g && !brix_known(argc > 2 ? argv[2] : 0, g->name)) { printf("WARN part not in brixels.pdl: %s\n", g->name); warns++; } } }
    }
    { int c[5] = { 0 }; for (int i = 0; i < nT; i++) c[!strcmp(T[i].t, "FURN") ? 0 : !strcmp(T[i].t, "ITEM") ? 1 : !strcmp(T[i].t, "FIELD") ? 2 : !strcmp(T[i].t, "TRAP") ? 3 : 4]++; printf("cell rows: furn=%d item=%d field=%d trap=%d job=%d\n", c[0], c[1], c[2], c[3], c[4]); }
    printf("%s: %d worlds, %d levels, %d links, %d errors, %d warnings\n", errs ? "INVALID" : "VALID", nW, nL, nK, errs, warns);
    return errs ? 1 : 0;
}
