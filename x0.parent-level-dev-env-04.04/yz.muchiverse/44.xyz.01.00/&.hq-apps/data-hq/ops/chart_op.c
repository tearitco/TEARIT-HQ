/* chart_op - the CHART RASTERISER of data-hq (phase P1, 2026-11-09; BOOK 08-roadmap/design-docs/DATA-HQ-DATA-STUDIO-PLAN-2026-11-09.md section 5).
 * C only, no library: a table (csv / tsv / pdl / ledger, read by the data_op loader that this file text-includes) + one spec row -> one PNG (8-bit RGB, stored deflate blocks, so a file is W*H*3 bytes + 1 per row).
 * Same input = same bytes: integer pixel work, no time, no random, no locale.
 *
 * Usage:
 *   chart_op <spec.pdl> <data> <out.png> [--id <chart id>] [--size WxH]       render the (first / named) CHART row; prints the facts a test needs (one row per line)
 * NOTE: an <out> ending in .raw writes RGBA + <base>.receipt.txt (frame_w= frame_h=) for the renderer's generic <canvas sprite="${canvas_raw}"/>.
 *   chart_op pixel <png> <x> <y>                                           `PIXEL x y rrggbb` read back with stb_image (an independent decoder of what the writer made)
 *   chart_op size <png>                                                    `SIZE w h`
 * Spec row (pipe separated, `-` or empty = none):   CHART | id | title | type | x | y | group | filter | agg
 *   type    line (x numeric or YYYY-MM-DD, one polyline per group) | scatter | bar (HORIZONTAL bars: x = category column, y = measure, optional group = clusters) | hist (x = the values, y unused, group = stacked)
 *   filter  col=val;col>=val;...   (data_op operators = != < > <= >=, AND of all)
 *   agg     comma list: sum mean count min max median none | top=N (bar: keep the N biggest, the rest become `other (K)`; default 12) | bins=N (hist: about N bins, default 10) | nosort (bar: keep first-seen order) | zero (line/scatter: y axis from 0)
 *           bar with no y column counts rows; line/scatter with an agg merge equal x values per group; an unknown token is REFUSED
 * Output facts (stdout): `CHART ok id= type= rows= points= groups= size=WxH out=`, `LAYOUT | l | t | r | b | x0 | x1 | y0 | y1`, `LEGEND | i | name | cx | cy | rrggbb`, `POINT | i | group | x | y | px | py | rrggbb` (the first 12 points / bars: where the mark was drawn)
 *   or `CHART nodata id= reason=` (exit 0, a grey "NO DATA" tile of the same size: no rows after the filter, or nothing numeric to draw).
 * Exit: 0 ok or nodata tile | 2 usage, a bad spec (unknown type / column / agg token / filter), a column that is not numeric where it must be, a refused size. Nothing is written on exit 2.
 * Palette: Okabe-Ito (colour-blind safe, the published standard for categorical colour), 8 colours + grey for `other`; marker shapes differ per series so colour is never the only cue; plain white ground, dark text.
 * Caps: image 160..2400 px a side; 512 distinct group values (refused beyond), 8 drawn series (the rest named in the legend as `+K more`), 40 categories drawn.
 * Build: gcc -std=gnu11 -Wall -Wextra -Wno-unused-function -O2 -o ops/+x/chart_op.+x ops/chart_op.c -lm   (one line in &.widgits/_shared-lib/harness/ops/build_harness_ops.sh) */
#define DATA_OP_AS_LIB
#include "data_op.c"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "../../../&.widgits/_shared-lib/stb_image.h"
#include "font57.inc"   /* F57[95][5]: byte for byte the table of @.apps/deskopoly/ops/deskopoly.c (the board labels); case data_hq_p1 compares the two texts so they cannot drift */

/* ---------------------------------------------------------------- canvas */
static int W = 800, H = 480; static unsigned char *IMG;
#define C_BG 0xFFFFFF
#define C_INK 0x222222
#define C_AXIS 0x555555
#define C_GRID 0xE2E2E2
#define C_MUTED 0x666666
static const unsigned PAL[9] = { 0x0072B2, 0xE69F00, 0x009E73, 0xD55E00, 0xCC79A7, 0x56B4E9, 0x8C6D31, 0x332288, 0x8A8A8A };   /* Okabe-Ito order (blue first) without black/yellow, brown + indigo added; the last = other */
static void put(int x, int y, unsigned c) { if (x < 0 || y < 0 || x >= W || y >= H) return; unsigned char *p = IMG + ((size_t)y * (size_t)W + (size_t)x) * 3; p[0] = (unsigned char)(c >> 16); p[1] = (unsigned char)(c >> 8); p[2] = (unsigned char)c; }
static unsigned getpx(int x, int y) { if (x < 0 || y < 0 || x >= W || y >= H) return 0; const unsigned char *p = IMG + ((size_t)y * (size_t)W + (size_t)x) * 3; return ((unsigned)p[0] << 16) | ((unsigned)p[1] << 8) | p[2]; }
static void rect(int x0, int y0, int x1, int y1, unsigned c) { for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) put(x, y, c); }   /* x1, y1 exclusive */
static void thick_line(int x0, int y0, int x1, int y1, int th, unsigned c) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1, err = dx + dy; int a = -(th / 2), b = a + th;
    for (;;) { rect(x0 + a, y0 + a, x0 + b, y0 + b, c); if (x0 == x1 && y0 == y1) break; int e2 = 2 * err; if (e2 >= dy) { err += dy; x0 += sx; } if (e2 <= dx) { err += dx; y0 += sy; } }
}
static void marker(int cx, int cy, int r, int shape, unsigned c, unsigned edge) {   /* 0 circle 1 square 2 diamond 3 triangle; an `edge` ring keeps it visible on any ground */
    for (int pass = 0; pass < 2; pass++) { int rr = pass ? r : r + 1; unsigned col = pass ? c : edge;
        for (int dy = -rr; dy <= rr; dy++) for (int dx = -rr; dx <= rr; dx++) { int in;
            switch (shape & 3) { case 0: in = dx * dx + dy * dy <= rr * rr + rr / 2; break; case 1: in = 1; break; case 2: in = abs(dx) + abs(dy) <= rr + 1; break; default: in = dy >= -rr && abs(dx) * 2 <= (dy + rr) + 1; break; }
            if (in) put(cx + dx, cy + dy, col); } }
}
static int text_w(const char *s, int sc) { int n = (int)strlen(s); return n ? n * 6 * sc - sc : 0; }
static void glyph(int x, int y, char ch, int sc, unsigned c, int rot) {
    if (ch < 32 || ch > 126) ch = '?'; const unsigned char *col = F57[ch - 32];
    for (int cx = 0; cx < 5; cx++) for (int ry = 0; ry < 7; ry++) if (col[cx] & (1 << ry)) for (int dy = 0; dy < sc; dy++) for (int dx = 0; dx < sc; dx++) {
        if (!rot) put(x + cx * sc + dx, y + ry * sc + dy, c); else put(x + ry * sc + dx, y - cx * sc - dy, c); }
}
static void text(int x, int y, const char *s, int sc, unsigned c) { for (; *s; s++, x += 6 * sc) glyph(x, y, *s, sc, c, 0); }
static void text_up(int x, int y, const char *s, int sc, unsigned c) { for (; *s; s++, y -= 6 * sc) glyph(x, y, *s, sc, c, 1); }   /* reads bottom to top, (x,y) = the bottom-left start */
static void fit(char *dst, size_t cap, const char *s, int maxpx, int sc) {   /* truncate with a final '.' so a label never runs into its neighbour */
    snprintf(dst, cap, "%s", s); while (text_w(dst, sc) > maxpx && strlen(dst) > 2) { size_t n = strlen(dst); dst[n - 2] = '.'; dst[n - 1] = 0; }
}

/* ---------------------------------------------------------------- png writer (stored blocks) */
static unsigned crc_t[256]; static void crc_init(void) { for (unsigned n = 0; n < 256; n++) { unsigned c = n; for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1; crc_t[n] = c; } }
static unsigned crc_up(unsigned c, const unsigned char *b, size_t n) { for (size_t i = 0; i < n; i++) c = crc_t[(c ^ b[i]) & 255] ^ (c >> 8); return c; }
static void be32(unsigned char *p, unsigned v) { p[0] = (unsigned char)(v >> 24); p[1] = (unsigned char)(v >> 16); p[2] = (unsigned char)(v >> 8); p[3] = (unsigned char)v; }
static void chunk(FILE *f, const char *type, const unsigned char *d, size_t n) {
    unsigned char h[8]; be32(h, (unsigned)n); memcpy(h + 4, type, 4); fwrite(h, 1, 8, f); if (n) fwrite(d, 1, n, f);
    unsigned c = crc_up(0xFFFFFFFFu, (const unsigned char *)type, 4); c = crc_up(c, d, n) ^ 0xFFFFFFFFu; unsigned char t[4]; be32(t, c); fwrite(t, 1, 4, f);
}
static int write_png(const char *path) {
    crc_init(); size_t rowb = (size_t)W * 3 + 1, raw = rowb * (size_t)H; unsigned char *z = malloc(raw + raw / 65535 * 5 + 64); if (!z) return -1; size_t zn = 0;
    z[zn++] = 0x78; z[zn++] = 0x01; unsigned a = 1, b = 0; size_t done = 0; unsigned char row[1];
    unsigned char *flat = malloc(raw); if (!flat) { free(z); return -1; }
    for (int y = 0; y < H; y++) { flat[(size_t)y * rowb] = 0; memcpy(flat + (size_t)y * rowb + 1, IMG + (size_t)y * (size_t)W * 3, (size_t)W * 3); } (void)row;
    while (done < raw) { size_t n = raw - done > 65535 ? 65535 : raw - done; z[zn++] = done + n == raw ? 1 : 0; z[zn++] = (unsigned char)n; z[zn++] = (unsigned char)(n >> 8); z[zn++] = (unsigned char)~n; z[zn++] = (unsigned char)(~n >> 8); memcpy(z + zn, flat + done, n); zn += n; done += n; }
    for (size_t i = 0; i < raw; i++) { a = (a + flat[i]) % 65521u; b = (b + a) % 65521u; } unsigned char ad[4]; be32(ad, (b << 16) | a); memcpy(z + zn, ad, 4); zn += 4; free(flat);
    char tmp[PATH_MAX + 8]; snprintf(tmp, sizeof tmp, "%s.tmp", path); FILE *f = fopen(tmp, "wb"); if (!f) { free(z); return -1; }
    static const unsigned char sig[8] = { 0x89, 'P', 'N', 'G', 13, 10, 26, 10 }; fwrite(sig, 1, 8, f);
    unsigned char ih[13]; be32(ih, (unsigned)W); be32(ih + 4, (unsigned)H); ih[8] = 8; ih[9] = 2; ih[10] = 0; ih[11] = 0; ih[12] = 0; chunk(f, "IHDR", ih, 13); chunk(f, "IDAT", z, zn); chunk(f, "IEND", NULL, 0);
    int bad = ferror(f); if (fclose(f)) bad = 1; free(z); if (bad || rename(tmp, path)) { unlink(tmp); return -1; } return 0;
}

/* out ending in .raw: RGBA bytes (alpha 255) + `<base>.receipt.txt` with frame_w= / frame_h= : exactly what the shared renderer's generic <canvas sprite="${canvas_raw}"/> blits (khtpm_draw_core.c kh_draw_canvas) */
static int write_raw(const char *path) {
    char tmp[PATH_MAX + 8]; snprintf(tmp, sizeof tmp, "%s.tmp", path); FILE *f = fopen(tmp, "wb"); if (!f) return -1; size_t n = (size_t)W * (size_t)H; unsigned char *o = malloc(n * 4); if (!o) { fclose(f); return -1; }
    for (size_t i = 0; i < n; i++) { o[i * 4] = IMG[i * 3]; o[i * 4 + 1] = IMG[i * 3 + 1]; o[i * 4 + 2] = IMG[i * 3 + 2]; o[i * 4 + 3] = 255; } fwrite(o, 1, n * 4, f); free(o); int bad = ferror(f); if (fclose(f)) bad = 1; if (bad || rename(tmp, path)) { unlink(tmp); return -1; }
    char rc[PATH_MAX + 32]; snprintf(rc, sizeof rc, "%.*s.receipt.txt", (int)(strlen(path) - 4), path); snprintf(tmp, sizeof tmp, "%s.tmp", rc); f = fopen(tmp, "w"); if (!f) return -1; fprintf(f, "frame_w=%d\nframe_h=%d\n", W, H); if (fclose(f) || rename(tmp, rc)) return -1; return 0;
}
static int write_out(const char *path) { size_t l = strlen(path); return l > 4 && !strcmp(path + l - 4, ".raw") ? write_raw(path) : write_png(path); }
/* ---------------------------------------------------------------- spec */
typedef struct { char id[48], title[160], type[16], x[48], y[48], group[48], filter[200], agg[96]; } Spec;
enum { A_NONE, A_SUM, A_MEAN, A_COUNT, A_MIN, A_MAX, A_MED };
typedef struct { int agg, agg_set, top, bins, sort, zero; } Opts;
static char *nodash(char *s) { return (!strcmp(s, "-")) ? s + strlen(s) : s; }
static int parse_spec(const char *path, const char *id, Spec *sp) {
    char err[300]; long len; char *b = slurp(path, &len, err, sizeof err); if (!b) die(2, "%s", err); char *save = NULL;
    for (char *ln = strtok_r(b, "\n", &save); ln; ln = strtok_r(NULL, "\n", &save)) {
        char *t = trims(ln); if (strncmp(t, "CHART", 5) || (t[5] != ' ' && t[5] != '|' && t[5] != '\t')) continue;
        char *f[9]; int n = 0; char *p = t; for (; n < 9; n++) { f[n] = p; char *bar = n < 8 ? strchr(p, '|') : NULL; if (!bar) { n++; break; } *bar = 0; p = bar + 1; }
        if (n != 9) die(2, "%s: a CHART row needs 9 fields (CHART | id | title | type | x | y | group | filter | agg), this one has %d", path, n);
        for (int i = 0; i < 9; i++) f[i] = nodash(trims(f[i])); if (strcmp(f[0], "CHART")) continue;
        if (id && strcmp(f[1], id)) continue;
        snprintf(sp->id, sizeof sp->id, "%s", f[1]); snprintf(sp->title, sizeof sp->title, "%s", f[2]); snprintf(sp->type, sizeof sp->type, "%s", f[3]); snprintf(sp->x, sizeof sp->x, "%s", f[4]); snprintf(sp->y, sizeof sp->y, "%s", f[5]);
        snprintf(sp->group, sizeof sp->group, "%s", f[6]); snprintf(sp->filter, sizeof sp->filter, "%s", f[7]); snprintf(sp->agg, sizeof sp->agg, "%s", f[8]); free(b); return 0;
    }
    free(b); die(2, "%s: no CHART row%s%s", path, id ? " with id " : "", id ? id : ""); return -1;
}
static void parse_opts(const char *agg, Opts *o) {
    memset(o, 0, sizeof *o); o->top = 12; o->bins = 10; o->sort = 1; char b[128]; snprintf(b, sizeof b, "%s", agg); char *sv = NULL;
    for (char *p = strtok_r(b, ", ", &sv); p; p = strtok_r(NULL, ", ", &sv)) {
        if (!strcmp(p, "sum")) { o->agg = A_SUM; o->agg_set = 1; } else if (!strcmp(p, "mean")) { o->agg = A_MEAN; o->agg_set = 1; } else if (!strcmp(p, "count")) { o->agg = A_COUNT; o->agg_set = 1; }
        else if (!strcmp(p, "min")) { o->agg = A_MIN; o->agg_set = 1; } else if (!strcmp(p, "max")) { o->agg = A_MAX; o->agg_set = 1; } else if (!strcmp(p, "median")) { o->agg = A_MED; o->agg_set = 1; } else if (!strcmp(p, "none")) { o->agg = A_NONE; }
        else if (!strncmp(p, "top=", 4) && atoi(p + 4) >= 1 && atoi(p + 4) <= 40) o->top = atoi(p + 4); else if (!strncmp(p, "bins=", 5) && atoi(p + 5) >= 2 && atoi(p + 5) <= 60) o->bins = atoi(p + 5);
        else if (!strcmp(p, "nosort")) o->sort = 0; else if (!strcmp(p, "zero")) o->zero = 1; else die(2, "unknown agg token \"%s\" (sum mean count min max median none top=N bins=N nosort zero)", p);
    }
}
static double aggv(double *v, int n, int a) {
    if (n <= 0) return 0; double s = 0, mn = v[0], mx = v[0]; for (int i = 0; i < n; i++) { s += v[i]; if (v[i] < mn) mn = v[i]; if (v[i] > mx) mx = v[i]; }
    switch (a) { case A_SUM: return s; case A_MEAN: return s / n; case A_COUNT: return n; case A_MIN: return mn; case A_MAX: return mx;
        case A_MED: { double *c = malloc(sizeof(double) * (size_t)n); memcpy(c, v, sizeof(double) * (size_t)n); qsort(c, (size_t)n, sizeof(double), cmp_dbl); double m = n & 1 ? c[n / 2] : (c[n / 2 - 1] + c[n / 2]) / 2; free(c); return m; } }
    return v[0];
}

/* ---------------------------------------------------------------- dates, ticks */
static long days_from_civil(int y, int m, int d) { y -= m <= 2; long era = (y >= 0 ? y : y - 399) / 400; unsigned yoe = (unsigned)(y - era * 400); unsigned doy = (unsigned)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1); unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy; return era * 146097 + (long)doe - 719468; }
static void civil_from_days(long z, int *y, int *m, int *d) { z += 719468; long era = (z >= 0 ? z : z - 146096) / 146097; unsigned doe = (unsigned)(z - era * 146097); unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365; long yy = (long)yoe + era * 400; unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100); unsigned mp = (5 * doy + 2) / 153; *d = (int)(doy - (153 * mp + 2) / 5 + 1); *m = (int)(mp < 10 ? mp + 3 : mp - 9); *y = (int)(yy + (*m <= 2)); }
static double parse_day(const char *s) { return (double)days_from_civil(atoi(s), atoi(s + 5), atoi(s + 8)); }
typedef struct { double lo, hi, step; int n; } Tk;
static double nicenum(double x, int rnd) { double e = floor(log10(x)), f = x / pow(10, e), nf; if (rnd) nf = f < 1.5 ? 1 : f < 3 ? 2 : f < 7 ? 5 : 10; else nf = f <= 1 ? 1 : f <= 2 ? 2 : f <= 5 ? 5 : 10; return nf * pow(10, e); }
static Tk make_ticks(double lo, double hi, int target, int integer) {
    if (hi <= lo) { lo -= 0.5; hi += 0.5; } double range = nicenum(hi - lo, 0), step = nicenum(range / (target < 2 ? 1 : target - 1), 1); if (integer && step < 1) step = 1;
    Tk t; t.step = step; t.lo = floor(lo / step + 1e-9) * step; t.hi = ceil(hi / step - 1e-9) * step; t.n = (int)llround((t.hi - t.lo) / step) + 1; return t;
}
static void tick_label(double v, int date, char *o, size_t cap) { if (date) { int y, m, d; civil_from_days((long)llround(v), &y, &m, &d); snprintf(o, cap, "%04d-%02d-%02d", y, m, d); } else { snprintf(o, cap, "%.10g", v == 0 ? 0.0 : v); } }
static Tk fit_ticks(double lo, double hi, int pix, int date, int integer, int horiz) {   /* the most ticks whose labels do not touch */
    for (int target = horiz ? 10 : 9; target >= 2; target--) { Tk t = make_ticks(lo, hi, target, integer); if (t.n < 2 && target > 2) continue; int ok = 1, wmax = 0; char lb[40];
        for (int i = 0; i < t.n; i++) { tick_label(t.lo + i * t.step, date, lb, sizeof lb); int w = text_w(lb, 2); if (w > wmax) wmax = w; }
        if (horiz) ok = t.n * (wmax + 14) <= pix + 14; else ok = t.n * 22 <= pix + 8; if (ok) return t; }
    return make_ticks(lo, hi, 2, integer);
}

/* ---------------------------------------------------------------- shared frame */
static int PL, PT, PR, PB;   /* plot rectangle, inclusive of the axis lines */
static void draw_title(const Spec *sp) {   /* 3x font; a title too long for the width drops to 2x (and only then loses its tail to a dot) */
    const char *s = sp->title[0] ? sp->title : sp->id; char tt[200]; int sc = text_w(s, 3) <= W - 24 ? 3 : 2; fit(tt, sizeof tt, s, W - 24, sc); text(12, sc == 3 ? 10 : 14, tt, sc, C_INK);
}
static void tile_nodata(const Spec *sp, const char *why) {
    rect(0, 0, W, H, 0xF2F2F2); rect(0, 0, W, 1, C_GRID); rect(0, H - 1, W, H, C_GRID); rect(0, 0, 1, H, C_GRID); rect(W - 1, 0, W, H, C_GRID);
    draw_title(sp);
    const char *m = "NO DATA"; text((W - text_w(m, 4)) / 2, H / 2 - 20, m, 4, C_MUTED); char r[200]; fit(r, sizeof r, why, W - 24, 2); text((W - text_w(r, 2)) / 2, H / 2 + 20, r, 2, C_MUTED);
}
typedef struct { char name[48]; unsigned col; int shape; } Leg;
static Leg LEG[10]; static int nleg; static char legtitle[48]; static int legmore;
static void layout_frame(const Spec *sp, int left, int legw) {
    PL = left; PT = 52; PR = W - 18 - legw; PB = H - 62;
    draw_title(sp);
}
static void draw_legend(int style /*0 swatch square, 1 line+marker*/) {
    if (!nleg) return; int x = PR + 22, y = PT; if (legtitle[0]) { char lt[60]; fit(lt, sizeof lt, legtitle, W - x - 8, 2); text(x, y, lt, 2, C_MUTED); y += 22; }
    for (int i = 0; i < nleg; i++) { int cy = y + 7; char nm[60]; fit(nm, sizeof nm, LEG[i].name, W - x - 30, 2);
        if (style) { thick_line(x, cy, x + 16, cy, 2, LEG[i].col); marker(x + 8, cy, 3, LEG[i].shape, LEG[i].col, C_BG); } else rect(x + 1, y, x + 15, y + 14, LEG[i].col);
        text(x + 24, y, nm, 2, C_INK); printf("LEGEND | %d | %s | %d | %d | %06x\n", i, LEG[i].name, x + 8, cy, LEG[i].col); y += 22; }
    if (legmore) { char b[40]; snprintf(b, sizeof b, "+%d more", legmore); text(x + 24, y, b, 2, C_MUTED); }
}
/* axes + grid for a numeric x and numeric y plot; X() and Y() map data to pixels */
static double AX0, AX1, AY0, AY1;
static int X(double v) { return (int)floor(PL + (v - AX0) / (AX1 - AX0) * (PR - PL) + 0.5); }
static int Y(double v) { return (int)floor(PB - (v - AY0) / (AY1 - AY0) * (PB - PT) + 0.5); }
static void draw_axes(Tk xt, Tk yt, int xdate, int ydate, const char *xtitle, const char *ytitle, int xgrid) {
    char lb[40];
    for (int i = 0; i < yt.n; i++) { double v = yt.lo + i * yt.step; int y = Y(v); rect(PL, y, PR + 1, y + 1, C_GRID); tick_label(v, ydate, lb, sizeof lb); text(PL - 8 - text_w(lb, 2), y - 7, lb, 2, C_MUTED); }
    if (xgrid) for (int i = 0; i < xt.n; i++) { double v = xt.lo + i * xt.step; rect(X(v), PT, X(v) + 1, PB + 1, C_GRID); }
    for (int i = 0; i < xt.n; i++) { double v = xt.lo + i * xt.step; int x = X(v); rect(x, PB, x + 1, PB + 5, C_AXIS); tick_label(v, xdate, lb, sizeof lb); int w = text_w(lb, 2); int tx = x - w / 2; if (tx + w > W - 4) tx = W - 4 - w; if (tx < 2) tx = 2; text(tx, PB + 9, lb, 2, C_MUTED); }
    rect(PL, PB, PR + 1, PB + 1, C_AXIS); rect(PL, PT, PL + 1, PB + 1, C_AXIS);
    char t[80]; fit(t, sizeof t, xtitle, PR - PL, 2); text(PL + (PR - PL - text_w(t, 2)) / 2, H - 22, t, 2, C_INK);
    fit(t, sizeof t, ytitle, PB - PT, 2); text_up(10, PB - (PB - PT - text_w(t, 2)) / 2, t, 2, C_INK);
}
static int ytick_width(Tk yt, int ydate) { int w = 0; char lb[40]; for (int i = 0; i < yt.n; i++) { tick_label(yt.lo + i * yt.step, ydate, lb, sizeof lb); int tw = text_w(lb, 2); if (tw > w) w = tw; } return w; }
static void print_layout(void) { printf("LAYOUT | %d | %d | %d | %d | %.10g | %.10g | %.10g | %.10g\n", PL, PT, PR, PB, AX0, AX1, AY0, AY1); }

/* ---------------------------------------------------------------- data selection */
static int select_rows(const Tab *t, const char *filter, int *keep) {
    Where wh[8]; int nw = 0; char b[200]; snprintf(b, sizeof b, "%s", filter); char *sv = NULL;
    for (char *p = strtok_r(b, ";", &sv); p; p = strtok_r(NULL, ";", &sv)) { if (nw >= 8) die(2, "filter: at most 8 conditions"); int rc = parse_where(t, trims(p), &wh[nw]); if (rc == -1) die(2, "filter \"%s\": use col=val (ops = != < > <= >=)", p); if (rc == -2) die(2, "filter \"%s\": no such column", p); nw++; }
    int n = 0; for (int r = 0; r < t->nrows; r++) { int ok = 1; for (int k = 0; k < nw; k++) if (!where_ok(t, r, &wh[k])) { ok = 0; break; } if (ok) keep[n++] = r; } return n;
}
#define MAXG 512
typedef struct { char name[MAXG][48]; int cnt[MAXG]; int n; int map[MAXG]; int nkeep, nmore; } Groups;
static int grp_id(Groups *g, const char *s) { for (int i = 0; i < g->n; i++) if (!strcmp(g->name[i], s)) return i; if (g->n >= MAXG) die(2, "the group column has more than %d distinct values", MAXG); snprintf(g->name[g->n], 48, "%s", s); g->cnt[g->n] = 0; return g->n++; }
static void grp_finish(Groups *g, int maxkeep) {   /* the maxkeep biggest series (ties: first seen), numbered in first-seen order */
    int kept[MAXG] = {0}; int nk = g->n < maxkeep ? g->n : maxkeep;
    for (int k = 0; k < nk; k++) { int best = -1; for (int i = 0; i < g->n; i++) if (!kept[i] && (best < 0 || g->cnt[i] > g->cnt[best])) best = i; kept[best] = 1; }
    int c = 0; for (int i = 0; i < g->n; i++) g->map[i] = kept[i] ? c++ : -1; g->nkeep = c; g->nmore = g->n - c;
}
static void legend_from(const Groups *g, const char *title, int ngr) {
    nleg = 0; legmore = 0; legtitle[0] = 0; if (ngr <= 0) return; snprintf(legtitle, sizeof legtitle, "%s", title);
    for (int i = 0; i < g->n; i++) if (g->map[i] >= 0 && nleg < 8) { int k = g->map[i]; snprintf(LEG[k].name, sizeof LEG[k].name, "%s", g->name[i]); LEG[k].col = PAL[k]; LEG[k].shape = k; nleg++; }
    legmore = g->nmore;
}
static int legend_width(int style) { (void)style; int w = 0; for (int i = 0; i < nleg; i++) { int tw = text_w(LEG[i].name, 2); if (tw > w) w = tw; } int tt = text_w(legtitle, 2); if (tt > w) w = tt; if (w > 170) w = 170; return nleg ? w + 24 + 22 + 8 : 0; }

typedef struct { double x, y; int g; long ord; } Pt;
static int cmp_pt(const void *a, const void *b) { const Pt *p = a, *q = b; if (p->g != q->g) return p->g - q->g; if (p->x != q->x) return p->x < q->x ? -1 : 1; return p->ord < q->ord ? -1 : p->ord > q->ord; }

static const char *col_must(const Tab *t, const char *name, const char *what, int *idx) { *idx = col_index(t, name); if (*idx < 0) die(2, "%s column \"%s\" is not in the table (columns: %s%s%s ...)", what, name, t->name[0], t->ncols > 1 ? ", " : "", t->ncols > 1 ? t->name[1] : ""); return name; }

/* ---------------------------------------------------------------- line and scatter */
static int render_xy(const Tab *t, const Spec *sp, const Opts *o, const int *rows, int nrows, int scatter, int *npts_out, int *ngr_out) {
    int xc, yc = -1, gc = -1, ycs[4], ny = 0; col_must(t, sp->x, "x", &xc); if (sp->group[0]) col_must(t, sp->group, "group", &gc);
    if (strchr(sp->y, '+')) { /* several measures of a WIDE table as one series each (y = a+b+c, at most 4); no group column then */
        if (gc >= 0) die(2, "a y list (%s) and a group column cannot be combined", sp->y); char yb[96]; snprintf(yb, sizeof yb, "%s", sp->y); char *sv = NULL;
        for (char *q = strtok_r(yb, "+", &sv); q; q = strtok_r(NULL, "+", &sv)) { if (ny >= 4) die(2, "a y list has at most 4 columns"); col_must(t, q, "y", &ycs[ny]); ny++; } }
    else { col_must(t, sp->y, "y", &yc); ycs[0] = yc; ny = 1; }
    int xdate = 1, nxv = 0, xtext = 0; for (int i = 0; i < nrows; i++) { const char *s = cellat(t, rows[i], xc); if (!*s) continue; nxv++; if (!is_date(s)) xdate = 0; if (!is_num(s, NULL) && !is_date(s)) xtext = 1; }
    if (nxv == 0) xdate = 0;
    Pt *p = malloc(sizeof(Pt) * (size_t)(nrows ? nrows : 1) * (size_t)ny); int np = 0; Groups *G = calloc(1, sizeof *G); int skipped = 0; int *rawg = NULL;
    if (ny > 1) { G->n = ny; for (int k = 0; k < ny; k++) snprintf(G->name[k], 48, "%s", t->name[ycs[k]]); }
    int ynum_bad = 0; for (int i = 0; i < nrows; i++) for (int k = 0; k < ny; k++) { const char *xs = cellat(t, rows[i], xc), *ys = cellat(t, rows[i], ycs[k]); double xv, yv;
        if (xdate && is_date(xs)) xv = parse_day(xs); else if (!is_num(xs, &xv)) { skipped++; continue; } if (!is_num(ys, &yv)) { if (*ys) ynum_bad = 1; skipped++; continue; }
        int g = ny > 1 ? k : gc >= 0 ? grp_id(G, cellat(t, rows[i], gc)) : 0; if (gc < 0 && ny == 1 && G->n == 0) { G->n = 1; snprintf(G->name[0], 48, "%s", sp->y); } G->cnt[g]++; p[np].x = xv; p[np].y = yv; p[np].g = g; p[np].ord = (long)i * 4 + k; np++; }
    (void)rawg; if (ny > 1) gc = -2;   /* -2: series are columns (the legend title is the unit below) */
    if (np == 0) {
        if (xtext) die(2, "x column \"%s\" is not numeric or a YYYY-MM-DD date (for %s use a bar chart)", sp->x, sp->type);
        if (ynum_bad) die(2, "y column \"%s\" is not numeric", sp->y);
        free(p); free(G); free(rawg); return -1; }
    if (gc != -1) grp_finish(G, 8); else { G->map[0] = 0; G->nkeep = 1; G->nmore = 0; }
    int n2 = 0; for (int i = 0; i < np; i++) { int m = G->map[p[i].g]; if (m < 0) continue; p[n2] = p[i]; p[n2].g = m; n2++; } np = n2;
    qsort(p, (size_t)np, sizeof(Pt), cmp_pt);
    if (o->agg_set && !scatter) {   /* merge equal x per group */
        int w = 0; for (int i = 0; i < np;) { int j = i; while (j < np && p[j].g == p[i].g && p[j].x == p[i].x) j++; double *v = malloc(sizeof(double) * (size_t)(j - i)); for (int k = i; k < j; k++) v[k - i] = p[k].y; p[w] = p[i]; p[w].y = aggv(v, j - i, o->agg); free(v); w++; i = j; } np = w; }
    double xmin = p[0].x, xmax = p[0].x, ymin = p[0].y, ymax = p[0].y; for (int i = 1; i < np; i++) { if (p[i].x < xmin) xmin = p[i].x; if (p[i].x > xmax) xmax = p[i].x; if (p[i].y < ymin) ymin = p[i].y; if (p[i].y > ymax) ymax = p[i].y; }
    if (o->zero || (ymin > 0 && ymax > 0 && ymin / ymax < 0.5)) { if (ymin > 0) ymin = 0; }
    legend_from(G, sp->group, gc != -1 ? G->nkeep : 0); int lw = legend_width(1);
    Tk yt = make_ticks(ymin, ymax, 7, 0); int left = 10 + 14 + 8 + ytick_width(yt, 0) + 10; layout_frame(sp, left, lw);
    Tk xt = fit_ticks(xmin, xmax, PR - PL, xdate, 0, 1); AX0 = xt.lo; AX1 = xt.hi; AY0 = yt.lo; AY1 = yt.hi;
    char ylab[80]; if (o->agg_set && !scatter) snprintf(ylab, sizeof ylab, "%s(%s)", o->agg == A_SUM ? "sum" : o->agg == A_MEAN ? "mean" : o->agg == A_COUNT ? "count" : o->agg == A_MIN ? "min" : o->agg == A_MAX ? "max" : "median", sp->y); else snprintf(ylab, sizeof ylab, "%s", ny > 1 ? "value" : sp->y);
    draw_axes(xt, yt, xdate, 0, sp->x, ylab, 1);
    int printed = 0; int *cnt = calloc((size_t)(G->nkeep + 1), sizeof(int)); for (int i = 0; i < np; i++) cnt[p[i].g]++;
    if (!scatter) for (int i = 0; i + 1 < np; i++) if (p[i].g == p[i + 1].g) thick_line(X(p[i].x), Y(p[i].y), X(p[i + 1].x), Y(p[i + 1].y), 2, PAL[p[i].g]);
    int rad = scatter ? (np > 400 ? 3 : 4) : 3; for (int i = 0; i < np; i++) { if (!scatter && cnt[p[i].g] > 60) continue; marker(X(p[i].x), Y(p[i].y), rad, p[i].g, PAL[p[i].g], C_BG); }
    draw_legend(!scatter ? 1 : 1);
    print_layout();
    for (int i = 0; i < np && printed < 12; i++, printed++) printf("POINT | %d | %d | %.10g | %.10g | %d | %d | %06x\n", i, p[i].g, p[i].x, p[i].y, X(p[i].x), Y(p[i].y), PAL[p[i].g]);
    *npts_out = np; *ngr_out = gc != -1 ? G->nkeep : 1; if (skipped) printf("NOTE | %d rows skipped (empty or non-numeric x/y)\n", skipped);
    free(p); free(G); free(rawg); free(cnt); return 0;
}

/* ---------------------------------------------------------------- horizontal bars */
typedef struct { int key; double v; } KV;
static int cmp_kv(const void *a, const void *b) { const KV *p = a, *q = b; return p->key - q->key; }
static int render_bar(const Tab *t, const Spec *sp, const Opts *o, const int *rows, int nrows, int *npts_out, int *ngr_out) {
    int xc, yc = -1, gc = -1; col_must(t, sp->x, "x", &xc); if (sp->y[0]) col_must(t, sp->y, "y", &yc); if (sp->group[0]) col_must(t, sp->group, "group", &gc);
    int agg = o->agg_set ? o->agg : (yc < 0 ? A_COUNT : A_SUM); if (yc < 0 && agg != A_COUNT) die(2, "a bar chart with no y column can only count");
    Groups *G = calloc(1, sizeof *G); Groups *C = calloc(1, sizeof *C); int *rc = malloc(sizeof(int) * (size_t)(nrows ? nrows : 1)), *rg = malloc(sizeof(int) * (size_t)(nrows ? nrows : 1)); double *rv = malloc(sizeof(double) * (size_t)(nrows ? nrows : 1)); int n = 0, ybad = 0;
    for (int i = 0; i < nrows; i++) { double v = 1; if (yc >= 0) { const char *ys = cellat(t, rows[i], yc); if (!is_num(ys, &v)) { if (*ys) ybad = 1; continue; } } else if (agg == A_COUNT) v = 1;
        const char *cs = cellat(t, rows[i], xc); int c = grp_id(C, cs); C->cnt[c]++; int g = gc >= 0 ? grp_id(G, cellat(t, rows[i], gc)) : 0; if (gc >= 0) G->cnt[g]++; rc[n] = c; rg[n] = g; rv[n] = v; n++; }
    if (n == 0) { if (ybad) die(2, "y column \"%s\" is not numeric", sp->y); free(G); free(C); free(rc); free(rg); free(rv); return -1; }
    if (gc >= 0) grp_finish(G, 8); else { G->map[0] = 0; G->nkeep = 1; G->n = 1; }
    int *keepg = malloc(sizeof(int) * (size_t)n); int m = 0; for (int i = 0; i < n; i++) { int gm = G->map[rg[i]]; if (gm < 0) continue; rc[m] = rc[i]; rg[m] = gm; rv[m] = rv[i]; m++; } n = m; (void)keepg; free(keepg);
    /* total per category, to sort and to pick the top N */
    KV *kv = malloc(sizeof(KV) * (size_t)n); for (int i = 0; i < n; i++) { kv[i].key = rc[i]; kv[i].v = rv[i]; }
    int nc = C->n; double *tot = calloc((size_t)nc, sizeof(double)); { double *buf = malloc(sizeof(double) * (size_t)n); for (int c = 0; c < nc; c++) { int k = 0; for (int i = 0; i < n; i++) if (kv[i].key == c) buf[k++] = kv[i].v; tot[c] = k ? aggv(buf, k, agg) : -1e300; } free(buf); }
    int *order = malloc(sizeof(int) * (size_t)nc); for (int i = 0; i < nc; i++) order[i] = i;
    if (o->sort) for (int i = 1; i < nc; i++) { int v = order[i], j = i - 1; while (j >= 0 && tot[order[j]] < tot[v]) { order[j + 1] = order[j]; j--; } order[j + 1] = v; }
    int ncat = nc, other = 0; if (nc > o->top) { ncat = o->top; other = 1; }
    int *slot = malloc(sizeof(int) * (size_t)nc); for (int i = 0; i < nc; i++) slot[i] = -1; for (int i = 0; i < ncat; i++) slot[order[i]] = i; int nrest = nc - ncat; int nslots = ncat + other;
    if (nslots > 40) nslots = 40;
    char (*cname)[48] = malloc(48 * (size_t)(nslots + 1)); for (int i = 0; i < ncat && i < nslots; i++) snprintf(cname[i], 48, "%s", C->name[order[i]]); if (other) snprintf(cname[ncat], 48, "other (%d)", nrest);
    int ng = G->nkeep; double *cell = calloc((size_t)(nslots * ng), sizeof(double)); int *cn = calloc((size_t)(nslots * ng), sizeof(int));
    /* gather values per cell */
    double **cv = calloc((size_t)(nslots * ng), sizeof(double *)); int *cc = calloc((size_t)(nslots * ng), sizeof(int)); int *capv = calloc((size_t)(nslots * ng), sizeof(int));
    for (int i = 0; i < n; i++) { int s = slot[rc[i]] >= 0 ? slot[rc[i]] : ncat; if (s >= nslots) continue; int k = s * ng + rg[i]; if (cc[k] >= capv[k]) { capv[k] = capv[k] ? capv[k] * 2 : 8; cv[k] = realloc(cv[k], sizeof(double) * (size_t)capv[k]); } cv[k][cc[k]++] = rv[i]; }
    double vmin = 0, vmax = 0; for (int k = 0; k < nslots * ng; k++) { cn[k] = cc[k]; if (cc[k]) { cell[k] = aggv(cv[k], cc[k], agg); if (cell[k] > vmax) vmax = cell[k]; if (cell[k] < vmin) vmin = cell[k]; } free(cv[k]); }
    legend_from(G, sp->group, gc >= 0 ? ng : 0); int lw = legend_width(0);
    int labw = 0; for (int i = 0; i < nslots; i++) { char f[60]; fit(f, sizeof f, cname[i], 17 * 12, 2); int w = text_w(f, 2); if (w > labw) labw = w; }
    layout_frame(sp, 10 + 14 + 8 + labw + 10, lw);
    if (vmax == vmin) vmax = vmin + 1; Tk vt = fit_ticks(vmin, vmax, PR - PL, 0, agg == A_COUNT, 1); AX0 = vt.lo; AX1 = vt.hi; AY0 = 0; AY1 = 1;
    char lb[40]; for (int i = 0; i < vt.n; i++) { double v = vt.lo + i * vt.step; int x = X(v); rect(x, PT, x + 1, PB + 1, C_GRID); rect(x, PB, x + 1, PB + 5, C_AXIS); tick_label(v, 0, lb, sizeof lb); int w = text_w(lb, 2); int tx = x - w / 2; if (tx + w > W - 4) tx = W - 4 - w; if (tx < 2) tx = 2; text(tx, PB + 9, lb, 2, C_MUTED); }
    rect(PL, PB, PR + 1, PB + 1, C_AXIS); int zx = X(0); rect(zx, PT, zx + 1, PB + 1, C_AXIS);
    int rowh = (PB - PT) / (nslots ? nslots : 1); if (rowh < 1) rowh = 1; int inner = rowh * 8 / 10; int bh = inner / ng; if (bh < 2) bh = 2; int printed = 0;
    for (int s = 0; s < nslots; s++) { int y0 = PT + s * rowh + (rowh - bh * ng) / 2; char f[60]; fit(f, sizeof f, cname[s], 17 * 12, 2); text(PL - 8 - text_w(f, 2), PT + s * rowh + rowh / 2 - 7, f, 2, C_INK);
        for (int g = 0; g < ng; g++) { int k = s * ng + g; if (!cn[k]) continue; int x1 = X(cell[k]); int xa = zx < x1 ? zx : x1, xb = zx < x1 ? x1 : zx; if (xb == xa) xb = xa + 1; unsigned col = (other && s == ncat && ng == 1) ? PAL[8] : PAL[g];
            rect(xa, y0 + g * bh, xb + (cell[k] < 0 ? 0 : 1), y0 + g * bh + bh - (bh > 4 ? 1 : 0), col);
            if (ng == 1 && bh >= 12) { char v[32]; tick_label(cell[k], 0, v, sizeof v); int w = text_w(v, 2); if (x1 + 6 + w < W - 4 && cell[k] >= 0) text(x1 + 6, y0 + g * bh + bh / 2 - 7, v, 2, C_INK); }
            if (printed < 12) { printf("POINT | %d | %d | %s | %.10g | %d | %d | %06x\n", printed, g, cname[s], cell[k], (xa + xb) / 2, y0 + g * bh + bh / 2, col); printed++; } } }
    draw_legend(0); print_layout();
    char xl[80]; snprintf(xl, sizeof xl, "%s(%s)", agg == A_SUM ? "sum" : agg == A_MEAN ? "mean" : agg == A_COUNT ? "count" : agg == A_MIN ? "min" : agg == A_MAX ? "max" : "median", sp->y[0] ? sp->y : sp->x);
    char t2[80]; fit(t2, sizeof t2, xl, PR - PL, 2); text(PL + (PR - PL - text_w(t2, 2)) / 2, H - 22, t2, 2, C_INK); fit(t2, sizeof t2, sp->x, PB - PT, 2); text_up(10, PB - (PB - PT - text_w(t2, 2)) / 2, t2, 2, C_INK);
    *npts_out = nslots * ng; *ngr_out = ng; free(G); free(C); free(rc); free(rg); free(rv); free(kv); free(tot); free(order); free(slot); free(cname); free(cell); free(cn); free(cv); free(cc); free(capv); return 0;
}

/* ---------------------------------------------------------------- histogram */
static int render_hist(const Tab *t, const Spec *sp, const Opts *o, const int *rows, int nrows, int *npts_out, int *ngr_out) {
    int xc, gc = -1; col_must(t, sp->x, "x", &xc); if (sp->group[0]) col_must(t, sp->group, "group", &gc);
    double *v = malloc(sizeof(double) * (size_t)(nrows ? nrows : 1)); int *vg = malloc(sizeof(int) * (size_t)(nrows ? nrows : 1)); int n = 0, bad = 0; Groups *G = calloc(1, sizeof *G);
    for (int i = 0; i < nrows; i++) { const char *s = cellat(t, rows[i], xc); double d; if (!is_num(s, &d)) { if (*s) bad = 1; continue; } int g = gc >= 0 ? grp_id(G, cellat(t, rows[i], gc)) : 0; if (gc >= 0) G->cnt[g]++; v[n] = d; vg[n] = g; n++; }
    if (n == 0) { if (bad) die(2, "x column \"%s\" is not numeric", sp->x); free(v); free(vg); free(G); return -1; }
    if (gc >= 0) grp_finish(G, 8); else { G->map[0] = 0; G->nkeep = 1; G->n = 1; }
    int m = 0; for (int i = 0; i < n; i++) { int g = G->map[vg[i]]; if (g < 0) continue; v[m] = v[i]; vg[m] = g; m++; } n = m; int ng = G->nkeep;
    double mn = v[0], mx = v[0], allint = 1; for (int i = 0; i < n; i++) { if (v[i] < mn) mn = v[i]; if (v[i] > mx) mx = v[i]; if (v[i] != floor(v[i])) allint = 0; }
    Tk bt = make_ticks(mn, mx, o->bins + 1, (int)allint); if (allint && mx >= bt.hi - 1e-9) { bt.hi += bt.step; bt.n++; }   /* whole numbers: the biggest value gets a bin of its own, not a share of the one before it */
    int sub = 1; while (bt.n - 1 < o->bins / 2 + 1 && sub < 4 && bt.step / (sub * 2) >= (allint ? 1 : 0) && (bt.step / (sub * 2)) > 0) sub *= 2;
    double bw = bt.step / sub; int nb = (bt.n - 1) * sub; if (nb < 1) nb = 1; int *cnt = calloc((size_t)(nb * ng), sizeof(int));
    for (int i = 0; i < n; i++) { int b = (int)floor((v[i] - bt.lo) / bw + 1e-9); if (b >= nb) b = nb - 1; if (b < 0) b = 0; cnt[b * ng + vg[i]]++; }
    int cmax = 1; for (int b = 0; b < nb; b++) { int tt = 0; for (int g = 0; g < ng; g++) tt += cnt[b * ng + g]; if (tt > cmax) cmax = tt; }
    legend_from(G, sp->group, gc >= 0 ? ng : 0); int lw = legend_width(0); Tk yt = make_ticks(0, cmax, 7, 1); layout_frame(sp, 10 + 14 + 8 + ytick_width(yt, 0) + 10, lw);
    AX0 = bt.lo; AX1 = bt.hi; AY0 = yt.lo; AY1 = yt.hi; Tk xt = bt; (void)xt; draw_axes(bt, yt, 0, 0, sp->x, "count", 0);
    int printed = 0; for (int b = 0; b < nb; b++) { int base = 0; int xa = X(bt.lo + b * bw) + 1, xb = X(bt.lo + (b + 1) * bw); if (xb - xa > 3) xb--;
        for (int g = 0; g < ng; g++) { int c = cnt[b * ng + g]; if (!c) continue; int ytop = Y(base + c), ybot = Y(base); rect(xa, ytop, xb, ybot, PAL[g]); if (ng > 1 && ybot - ytop > 2) rect(xa, ytop, xb, ytop + 1, C_BG);
            if (printed < 12) { printf("POINT | %d | %d | %.10g | %d | %d | %d | %06x\n", b, g, bt.lo + b * bw, c, (xa + xb) / 2, (ytop + ybot) / 2, PAL[g]); printed++; } base += c; } }
    rect(PL, PB, PR + 1, PB + 1, C_AXIS); draw_legend(0); print_layout(); *npts_out = n; *ngr_out = ng; free(v); free(vg); free(G); free(cnt); return 0;
}

/* ---------------------------------------------------------------- main */
static int parse_size(const char *s) { int w, h; if (sscanf(s, "%dx%d", &w, &h) != 2 || w < 160 || h < 160 || w > 2400 || h > 2400) return -1; W = w; H = h; return 0; }
int main(int argc, char **argv) {
    if (argc >= 2 && !strcmp(argv[1], "pixel")) { if (argc != 5) die(2, "usage: chart_op pixel <png> <x> <y>"); int w, h, c; unsigned char *d = stbi_load(argv[2], &w, &h, &c, 3); if (!d) die(2, "cannot read %s as a png", argv[2]); int x = atoi(argv[3]), y = atoi(argv[4]); if (x < 0 || y < 0 || x >= w || y >= h) die(2, "pixel %d,%d is outside %dx%d", x, y, w, h); const unsigned char *p = d + ((size_t)y * (size_t)w + (size_t)x) * 3; printf("PIXEL %d %d %02x%02x%02x\n", x, y, p[0], p[1], p[2]); return 0; }
    if (argc >= 2 && !strcmp(argv[1], "size")) { if (argc != 3) die(2, "usage: chart_op size <png>"); int w, h, c; if (!stbi_info(argv[2], &w, &h, &c)) die(2, "cannot read %s as a png", argv[2]); printf("SIZE %d %d\n", w, h); return 0; }
    const char *pos[3] = {0}; int np = 0; const char *id = NULL; const char *size = NULL;
    for (int i = 1; i < argc; i++) { if (!strcmp(argv[i], "--id") && i + 1 < argc) id = argv[++i]; else if (!strcmp(argv[i], "--size") && i + 1 < argc) size = argv[++i]; else if (np < 3) pos[np++] = argv[i]; else die(2, "unexpected %s", argv[i]); }
    if (np != 3) die(2, "usage: chart_op <spec.pdl> <data> <out.png> [--id X] [--size WxH]   |   chart_op pixel <png> <x> <y>   |   chart_op size <png>");
    if (size && parse_size(size)) die(2, "--size %s: use WxH between 160x160 and 2400x2400", size);
    Spec sp; memset(&sp, 0, sizeof sp); parse_spec(pos[0], id, &sp); Opts o; parse_opts(sp.agg, &o);
    int is_line = !strcmp(sp.type, "line"), is_sc = !strcmp(sp.type, "scatter"), is_bar = !strcmp(sp.type, "bar"), is_hist = !strcmp(sp.type, "hist");
    if (!is_line && !is_sc && !is_bar && !is_hist) die(2, "chart %s: unknown type \"%s\" (line scatter bar hist)", sp.id, sp.type);
    if (!sp.x[0]) die(2, "chart %s: x is required", sp.id); if ((is_line || is_sc) && !sp.y[0]) die(2, "chart %s: a %s chart needs y", sp.id, sp.type);
    Tab t; char err[300]; if (load_table(pos[1], &t, err, sizeof err)) die(2, "%s", err);
    IMG = malloc((size_t)W * (size_t)H * 3); if (!IMG) die(2, "out of memory"); rect(0, 0, W, H, C_BG);
    int *rows = malloc(sizeof(int) * (size_t)(t.nrows ? t.nrows : 1)); int nrows = select_rows(&t, sp.filter, rows); int npts = 0, ngr = 0, rc = -1; const char *why = "no rows";
    if (nrows > 0) { rc = is_line || is_sc ? render_xy(&t, &sp, &o, rows, nrows, is_sc, &npts, &ngr) : is_bar ? render_bar(&t, &sp, &o, rows, nrows, &npts, &ngr) : render_hist(&t, &sp, &o, rows, nrows, &npts, &ngr); why = "no numeric values to draw"; }
    else why = sp.filter[0] ? "no rows match the filter" : "the table has no rows";
    if (rc != 0) { rect(0, 0, W, H, C_BG); tile_nodata(&sp, why); if (write_out(pos[2])) die(2, "cannot write %s", pos[2]); printf("CHART nodata id=%s reason=%s\n", sp.id, why); return 0; }
    if (write_out(pos[2])) die(2, "cannot write %s", pos[2]);
    printf("CHART ok id=%s type=%s rows=%d points=%d groups=%d size=%dx%d out=%s\n", sp.id, sp.type, nrows, npts, ngr, W, H, pos[2]); return 0;
}
