/* generator_cast_op.c - a Tear-it cast: one row per character, built from parts.
 *
 *   generator_cast_op.+x cast <cast.pdl> <outdir>     write every character (sheet + sprite.csv) and RPG-Maker-layout cast sheets
 *   generator_cast_op.+x cast-check <cast.pdl>        parse only (exit 2 on a bad row)
 *
 * Cast file rows:
 *   CAST    | id | skin | hair_style | hair | garment | trim | body | mark | kind (optional: humanoid quad blob winged)
 *             hair_style: short long mohawk none     body: 0 slim 1 normal 2 broad     mark: 1 = gold chain and wrist
 *   GENERIC | prefix | count | seed                  count characters named prefix_00.. from a fixed Tear-it palette, by hash (no rand)
 * Colours are #rrggbb.
 *
 * The figure is OUR proportion, not RPG Maker's: a big round head, a short chunky body, a dark outline, a two-tone shade.
 * Layout is the same as the RPG Maker character sheet (3 walk frames across, down / left / right / up down) so a sheet drops
 * into an existing map. Right is the exact mirror of left. Same row, same bytes.
 * Candidates only: nothing here writes to a good/ folder.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../../&.widgits/_shared-lib/stb_image_write.h"

#define CW 48
#define CH 48
#define SW (CW * 3)
#define SH (CH * 4)
#define MAXCAST 256

typedef struct { int r, g, b; } Col;
typedef struct { char id[40]; Col skin, hair, garment, trim; int style, body, mark, kind; } Who; /* style: 0 short 1 long 2 mohawk 3 none; kind: 0 humanoid 1 quad 2 blob 3 winged */

static Who g_who[MAXCAST];
static int g_n;

static void mkp(const char *path) {
    char buf[1100];
    snprintf(buf, sizeof buf, "%s", path);
    for (char *p = buf + 1; *p; p++) if (*p == '/') { *p = 0; mkdir(buf, 0755); *p = '/'; }
    mkdir(buf, 0755);
}
static unsigned mix(unsigned h) { h ^= h >> 16; h *= 0x7feb352du; h ^= h >> 15; h *= 0x846ca68bu; h ^= h >> 16; return h; }
static int hexv(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
static int parse_col(const char *s, Col *c) {
    int i, v[6];
    if (*s == '#') s++;
    for (i = 0; i < 6; i++) { v[i] = hexv(s[i]); if (v[i] < 0) return -1; }
    if (s[6] && s[6] != ' ') return -1;
    c->r = v[0] * 16 + v[1]; c->g = v[2] * 16 + v[3]; c->b = v[4] * 16 + v[5];
    return 0;
}
static int style_of(const char *s) {
    if (!strcmp(s, "short")) return 0;
    if (!strcmp(s, "long")) return 1;
    if (!strcmp(s, "mohawk")) return 2;
    if (!strcmp(s, "none")) return 3;
    if (!strcmp(s, "masked")) return 4; /* a full-face mask in the hair colour, glowing eye slits in the trim colour (Sol) */
    if (!strcmp(s, "clown")) return 5;  /* white face paint, a red nose, a wig in the hair colour (the clowns) */
    return -1;
}
static Col shade(Col c, int pct) { Col o; o.r = c.r * pct / 100; o.g = c.g * pct / 100; o.b = c.b * pct / 100; if (o.r > 255) o.r = 255; if (o.g > 255) o.g = 255; if (o.b > 255) o.b = 255; return o; }

/* ---- drawing on one 48x48 cell ---- */
static void put(unsigned char *px, int x, int y, Col c) {
    unsigned char *p;
    if (x < 0 || y < 0 || x >= CW || y >= CH) return;
    p = px + (y * CW + x) * 4;
    p[0] = (unsigned char)c.r; p[1] = (unsigned char)c.g; p[2] = (unsigned char)c.b; p[3] = 255;
}
static void rect(unsigned char *px, int x0, int y0, int x1, int y1, Col c) { int x, y; for (y = y0; y <= y1; y++) for (x = x0; x <= x1; x++) put(px, x, y, c); }
static int in_ell(int x, int y, int cx, int cy, int rx, int ry) { int dx = x - cx, dy = y - cy; return dx * dx * ry * ry + dy * dy * rx * rx <= rx * rx * ry * ry; }
static void ell(unsigned char *px, int cx, int cy, int rx, int ry, Col c) { int x, y; for (y = cy - ry; y <= cy + ry; y++) for (x = cx - rx; x <= cx + rx; x++) if (in_ell(x, y, cx, cy, rx, ry)) put(px, x, y, c); }

/* ---- creatures: pets and monsters (kind 1 quad, 2 blob, 3 winged), same 3x4 sheet layout and the same outline.
 * Colours: skin = body, garment = belly and patches, hair = ears, mane, wings, tail, trim = eyes and glow. ---- */
static void draw_creature(unsigned char *px, const Who *w, int dir, int step) {
    Col body = w->skin, body_d = shade(w->skin, 72), belly = w->garment, ear = w->hair, glow = w->trim, eye = { 20, 20, 30 };
    int cx = 24, x, y, side = (dir == 1), back = (dir == 3), lift_a = (step == 0) ? 3 : 0, lift_b = (step == 2) ? 3 : 0, wob = (step == 1) ? 0 : 1;
    int sz = 8 + 2 * w->body; /* body 0 small, 1 normal, 2 large */
    if (w->kind == 1) { /* quad: a four-legged pet */
        if (side) {
            ell(px, cx + 2, 30, sz + 3, 7, body); ell(px, cx + 2, 33, sz, 4, belly);
            rect(px, cx - 7 - lift_a, 35, cx - 4 - lift_a, 43 - lift_a / 2, body_d); rect(px, cx - 1 + lift_b, 35, cx + 2 + lift_b, 43 - lift_b / 2, body);
            rect(px, cx + 5 + lift_b, 35, cx + 8 + lift_b, 43 - lift_b / 2, body_d); rect(px, cx + 9 - lift_a, 35, cx + 12 - lift_a, 43 - lift_a / 2, body);
            ell(px, cx - 9, 22, 7, 6, body); rect(px, cx - 14, 24, cx - 11, 26, belly); rect(px, cx - 9, 14, cx - 6, 18, ear);
            rect(px, cx - 11, 21, cx - 10, 22, eye); put(px, cx - 11, 21, glow); rect(px, cx + sz + 4, 26 - wob, cx + sz + 8, 29 - wob, ear);
        } else {
            ell(px, cx, 33 + wob, sz, 8, body); ell(px, cx, 36 + wob, sz - 3, 4, belly);
            rect(px, cx - 7, 38 - lift_a / 2, cx - 3, 43 - lift_a, body_d); rect(px, cx + 3, 38 - lift_b / 2, cx + 7, 43 - lift_b, body_d);
            ell(px, cx, 20 + wob, 9, 8, body); rect(px, cx - 9, 9 + wob, cx - 6, 14 + wob, ear); rect(px, cx + 6, 9 + wob, cx + 9, 14 + wob, ear);
            if (back) rect(px, cx - 1, 30, cx + 1, 40, ear);
            else { rect(px, cx - 5, 19 + wob, cx - 4, 20 + wob, glow); rect(px, cx + 3, 19 + wob, cx + 4, 20 + wob, glow); rect(px, cx - 1, 24 + wob, cx, 25 + wob, eye); ell(px, cx, 25 + wob, 4, 2, belly); rect(px, cx - 1, 24 + wob, cx, 24 + wob, eye); }
        }
    } else if (w->kind == 2) { /* blob: a slime that squashes as it moves */
        int rx = sz + 4 + (step == 1 ? -1 : 2), ry = sz + 1 + (step == 1 ? 2 : -1), cy = 42 - ry;
        ell(px, cx, cy, rx, ry, body);
        for (y = cy; y <= cy + ry; y++) for (x = cx - rx; x <= cx + rx; x++) if (in_ell(x, y, cx, cy, rx, ry) && y > cy + ry / 2) put(px, x, y, body_d);
        if (!back) {
            int ex = side ? -4 : 0;
            rect(px, cx - 5 + ex, cy - 2, cx - 4 + ex, cy, eye); rect(px, cx - 5 + ex, cy - 2, cx - 5 + ex, cy - 2, glow);
            if (!side) { rect(px, cx + 4, cy - 2, cx + 5, cy, eye); put(px, cx + 4, cy - 2, glow); }
            rect(px, cx - 1 + ex, cy + 3, cx + 1 + ex, cy + 3, body_d);
        }
        rect(px, cx - 5, cy - ry + 2, cx - 3, cy - ry + 3, belly); rect(px, cx - 4, cy - ry + 4, cx - 4, cy - ry + 4, belly);
    } else if (w->kind == 3) { /* winged: a bat or small dragon, wings beat across the three frames */
        int wy = (step == 0) ? 20 : (step == 1 ? 26 : 32);
        if (!side) { ell(px, cx - 13, wy, 9, 4, ear); ell(px, cx + 13, wy, 9, 4, ear); }
        else ell(px, cx + 4, wy, 9, 4, ear);
        ell(px, cx, 29, 6 + w->body, 8, body); ell(px, cx, 31, 4 + w->body, 5, belly);
        ell(px, cx + (side ? -3 : 0), 18, 7, 6, body);
        rect(px, cx - 6 + (side ? -3 : 0), 10, cx - 4 + (side ? -3 : 0), 13, ear); rect(px, cx + 4 + (side ? -3 : 0), 10, cx + 6 + (side ? -3 : 0), 13, ear);
        if (!back) { rect(px, cx - 4 + (side ? -3 : 0), 17, cx - 3 + (side ? -3 : 0), 18, glow); if (!side) rect(px, cx + 3, 17, cx + 4, 18, glow); }
        rect(px, cx - 3, 37, cx - 1, 41 - lift_a / 2, body_d); rect(px, cx + 1, 37, cx + 3, 41 - lift_b / 2, body_d);
    } else if (w->kind == 4) { /* plant: a fly-trap (the Venus Garden Mind): a bulb on a stem, the jaws open and close across the three frames */
        int open = (step == 0) ? 1 : (step == 1 ? 4 : 8);
        rect(px, cx - 1, 30, cx + 1, 43, belly); ell(px, cx - 7, 41, 5, 2, belly); ell(px, cx + 7, 41, 5, 2, belly);
        ell(px, cx, 22 + open / 2, 11, 7, body); ell(px, cx, 22 - open / 2, 11, 7, body_d);
        if (!back) { rect(px, cx - 8, 22 - open / 2 + 2, cx + 8, 22 + open / 2 - 2, ear); for (x = cx - 8; x <= cx + 8; x += 3) { put(px, x, 22 - open / 2 + 2, glow); put(px, x, 22 + open / 2 - 2, glow); } }
        rect(px, cx - 4, 14 - open / 2, cx - 3, 15 - open / 2, eye);
    } else { /* jelly: a dome with tentacles that sway (the Turritid, the Jelly Fish God) */
        int sway = (step == 0) ? -2 : (step == 2 ? 2 : 0);
        ell(px, cx, 20 + wob, 12, 10, body); rect(px, cx - 12, 22 + wob, cx + 12, 24 + wob, body); ell(px, cx, 17 + wob, 8, 5, belly);
        for (x = -9; x <= 9; x += 3) rect(px, cx + x + (x > 0 ? sway : -sway) / 2, 25, cx + x + 1 + (x > 0 ? sway : -sway) / 2, 38 + (x % 2 ? 4 : 0), ear);
        if (!back) { rect(px, cx - 5, 18 + wob, cx - 4, 19 + wob, glow); rect(px, cx + 4, 18 + wob, cx + 5, 19 + wob, glow); }
    }
}

/* dir: 0 down, 1 left, 2 (mirror of left), 3 up.  step: 0 left foot, 1 stand, 2 right foot */
static void draw_frame(unsigned char *px, const Who *w, int dir, int step) {
    const Col ink = { 14, 16, 21 };
    const Col eye = { 20, 20, 30 };
    Col skin = w->skin, skin_d = shade(w->skin, 78), gar = w->garment, gar_d = shade(w->garment, 72), leg = shade(w->garment, 85), shoe = { 12, 12, 16 };
    int cx = 24, bw = 6 + w->body, bob = (step == 1) ? 0 : -1, hy = 16 + bob, x, y, side = (dir == 1), back = (dir == 3);
    int lift_l = (step == 0) ? 3 : 0, lift_r = (step == 2) ? 3 : 0, sw_l = (step == 0) ? 1 : (step == 2 ? -1 : 0), sw_r = -sw_l;
    memset(px, 0, CW * CH * 4);
    if (w->kind) draw_creature(px, w, dir, step);
    else if (side) {
        /* legs: front and back, apart on a step */
        int fx = (step == 0) ? -2 : (step == 2 ? 2 : 0);
        rect(px, cx - 2 - fx, 36 + bob, cx + 2 - fx, 43, leg); rect(px, cx - 2 - fx, 42, cx + 2 - fx, 44, shoe);
        rect(px, cx - 2 + fx, 36 + bob, cx + 2 + fx, 43, shade(leg, 90)); rect(px, cx - 2 + fx, 42, cx + 2 + fx, 44, shoe);
        /* torso */
        rect(px, cx - 4, 26 + bob, cx + 4, 36 + bob, gar); rect(px, cx + 1, 26 + bob, cx + 4, 36 + bob, gar_d);
        /* arm swings fore and aft */
        rect(px, cx - 2 + fx, 27 + bob, cx + 1 + fx, 33 + bob, gar_d); rect(px, cx - 2 + fx, 34 + bob, cx + 1 + fx, 35 + bob, skin);
        if (w->mark == 1) { rect(px, cx - 2 + fx, 34 + bob, cx - 1 + fx, 35 + bob, w->trim); rect(px, cx - 4, 26 + bob, cx - 2, 26 + bob, w->trim); }
        /* head */
        ell(px, cx - 1, hy, 8, 8, skin);
        for (y = hy - 8; y <= hy + 8; y++) for (x = cx - 9; x <= cx + 7; x++) if (in_ell(x, y, cx - 1, hy, 8, 8) && x > cx + 2) put(px, x, y, skin_d);
        put(px, cx - 6, hy + 1, eye); put(px, cx - 6, hy + 2, eye); put(px, cx - 5, hy + 1, eye); put(px, cx - 5, hy + 2, eye);
        if (w->style == 0 || w->style == 1) for (y = hy - 8; y <= hy + 8; y++) for (x = cx - 9; x <= cx + 7; x++) if (in_ell(x, y, cx - 1, hy, 8, 8) && (y < hy - 1 || x > cx + 1)) put(px, x, y, w->hair);
        if (w->style == 1) rect(px, cx + 1, hy + 2, cx + 6, hy + 14, w->hair);
        if (w->style == 2) rect(px, cx - 2, hy - 12, cx + 3, hy - 4, w->hair);
        if (w->style == 4) { ell(px, cx - 1, hy, 8, 8, w->hair); rect(px, cx - 7, hy + 1, cx - 5, hy + 2, w->trim); }
        if (w->style == 5) { const Col white = { 244, 240, 232 }, red = { 214, 40, 40 }; ell(px, cx - 1, hy, 8, 8, white); put(px, cx - 6, hy + 1, eye); put(px, cx - 6, hy + 2, eye); rect(px, cx - 9, hy + 3, cx - 8, hy + 4, red); rect(px, cx + 2, hy - 2, cx + 7, hy + 6, w->hair); }
    } else {
        /* legs */
        rect(px, cx - 5, 36 + bob + lift_l / 2, cx - 1, 43 - lift_l, leg); rect(px, cx - 5, 42 - lift_l, cx - 1, 44 - lift_l, shoe);
        rect(px, cx + 1, 36 + bob + lift_r / 2, cx + 5, 43 - lift_r, shade(leg, 90)); rect(px, cx + 1, 42 - lift_r, cx + 5, 44 - lift_r, shoe);
        /* torso with a collar */
        rect(px, cx - bw, 26 + bob, cx + bw - 1, 36 + bob, gar); rect(px, cx + 1, 26 + bob, cx + bw - 1, 36 + bob, gar_d);
        rect(px, cx - bw, 26 + bob, cx + bw - 1, 27 + bob, gar_d);
        /* arms, swinging against the legs */
        rect(px, cx - bw - 3, 27 + bob + sw_l, cx - bw - 1, 33 + bob + sw_l, gar_d); rect(px, cx - bw - 3, 34 + bob + sw_l, cx - bw - 1, 35 + bob + sw_l, skin);
        rect(px, cx + bw, 27 + bob + sw_r, cx + bw + 2, 33 + bob + sw_r, gar_d); rect(px, cx + bw, 34 + bob + sw_r, cx + bw + 2, 35 + bob + sw_r, skin);
        if (w->mark == 1 && !back) { rect(px, cx - 4, 26 + bob, cx + 3, 26 + bob, w->trim); put(px, cx - 1, 27 + bob, w->trim); put(px, cx - 1, 28 + bob, w->trim); put(px, cx, 28 + bob, w->trim);
                                rect(px, cx + bw, 34 + bob + sw_r, cx + bw + 1, 35 + bob + sw_r, w->trim); }
        /* head */
        ell(px, cx, hy, 9, 8, skin);
        for (y = hy - 8; y <= hy + 8; y++) for (x = cx - 9; x <= cx + 9; x++) if (in_ell(x, y, cx, hy, 9, 8) && y > hy + 4) put(px, x, y, skin_d);
        if (!back) {
            rect(px, cx - 5, hy + 1, cx - 4, hy + 2, eye); rect(px, cx + 3, hy + 1, cx + 4, hy + 2, eye);
            rect(px, cx - 1, hy + 5, cx, hy + 5, skin_d);
        }
        if (w->style == 0 || w->style == 1) for (y = hy - 8; y <= hy + 8; y++) for (x = cx - 9; x <= cx + 9; x++)
            if (in_ell(x, y, cx, hy, 9, 8) && (back ? y < hy + 5 : (y < hy - 1 || ((x < cx - 7 || x > cx + 7) && y < hy + 3)))) put(px, x, y, w->hair);
        if (w->style == 1) { rect(px, cx - 10, hy + 1, cx - 7, hy + 13, w->hair); rect(px, cx + 7, hy + 1, cx + 10, hy + 13, w->hair); if (back) rect(px, cx - 8, hy + 3, cx + 8, hy + 14, w->hair); }
        if (w->style == 2) rect(px, cx - 1, hy - 13, cx + 1, hy - 4, w->hair);
        if (w->style == 4) { ell(px, cx, hy, 9, 8, w->hair); if (!back) { rect(px, cx - 6, hy + 1, cx - 3, hy + 2, w->trim); rect(px, cx + 3, hy + 1, cx + 6, hy + 2, w->trim); } }
        if (w->style == 5) {
            const Col white = { 244, 240, 232 }, red = { 214, 40, 40 };
            if (back) ell(px, cx, hy, 9, 8, w->hair);
            else { ell(px, cx, hy, 9, 8, white); rect(px, cx - 5, hy + 1, cx - 4, hy + 2, eye); rect(px, cx + 3, hy + 1, cx + 4, hy + 2, eye); rect(px, cx - 1, hy + 3, cx, hy + 4, red); rect(px, cx - 12, hy - 4, cx - 8, hy + 4, w->hair); rect(px, cx + 8, hy - 4, cx + 12, hy + 4, w->hair); rect(px, cx - 7, hy - 9, cx + 7, hy - 5, w->hair); }
        }
    }
    /* outline: any transparent pixel next to an opaque one becomes ink (a plain 4-neighbour pass) */
    {
        static unsigned char src[CW * CH * 4];
        memcpy(src, px, sizeof src);
        for (y = 0; y < CH; y++) for (x = 0; x < CW; x++) {
            if (src[(y * CW + x) * 4 + 3]) continue;
            if ((x > 0 && src[(y * CW + x - 1) * 4 + 3]) || (x < CW - 1 && src[(y * CW + x + 1) * 4 + 3]) ||
                (y > 0 && src[((y - 1) * CW + x) * 4 + 3]) || (y < CH - 1 && src[((y + 1) * CW + x) * 4 + 3])) put(px, x, y, ink);
        }
    }
    /* mark 2 = an echo: a translucent Time Twin (the same figure, 55 percent opaque) */
    if (w->mark == 2) for (x = 0; x < CW * CH; x++) px[x * 4 + 3] = (unsigned char)(px[x * 4 + 3] * 55 / 100);
}
static void mirror(unsigned char *dst, const unsigned char *src) { int x, y; for (y = 0; y < CH; y++) for (x = 0; x < CW; x++) memcpy(dst + (y * CW + x) * 4, src + (y * CW + (CW - 1 - x)) * 4, 4); }
static void blit(unsigned char *sheet, int sw, const unsigned char *cell, int ox, int oy) { int y; for (y = 0; y < CH; y++) memcpy(sheet + ((oy + y) * sw + ox) * 4, cell + y * CW * 4, CW * 4); }
static void draw_sheet(unsigned char *sheet, const Who *w) {
    unsigned char cell[CW * CH * 4], m[CW * CH * 4];
    int d, s;
    for (s = 0; s < 3; s++) {
        draw_frame(cell, w, 0, s); blit(sheet, SW, cell, s * CW, 0);
        draw_frame(cell, w, 1, s); blit(sheet, SW, cell, s * CW, CH);
        mirror(m, cell);           blit(sheet, SW, m, s * CW, 2 * CH);
        draw_frame(cell, w, 3, s); blit(sheet, SW, cell, s * CW, 3 * CH);
    }
    (void)d;
}
static int write_csv(const char *path, const unsigned char *px, int w, int h) {
    FILE *f = fopen(path, "w");
    int i;
    if (!f) return -1;
    fprintf(f, "# resolution=%dx%d\n# scale=1.0\n# transform=0,0,0\nr,g,b,a\n", w, h);
    for (i = 0; i < w * h; i++) fprintf(f, "%u,%u,%u,%u\n", px[i * 4], px[i * 4 + 1], px[i * 4 + 2], px[i * 4 + 3]);
    fclose(f);
    return 0;
}

/* ---- the cast file ---- */
static char *fld(char **p) {
    char *s = *p, *q;
    while (*s == ' ') s++;
    q = strchr(s, '|');
    if (q) { *q = 0; *p = q + 1; } else *p = s + strlen(s);
    { char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\n')) *--e = 0; }
    return s;
}
static int add_generic(const char *prefix, int count, unsigned seed) {
    static const Col skins[6] = { {240, 205, 170}, {222, 176, 138}, {190, 138, 100}, {150, 100, 70}, {112, 74, 52}, {84, 56, 40} };
    static const Col hairs[8] = { {20, 20, 26}, {58, 36, 24}, {110, 66, 34}, {168, 112, 52}, {196, 60, 40}, {200, 200, 210}, {60, 70, 120}, {120, 60, 140} };
    static const Col gars[8] = { {22, 24, 30}, {30, 52, 46}, {32, 44, 84}, {64, 34, 78}, {90, 38, 28}, {52, 52, 62}, {20, 20, 20}, {70, 60, 36} };
    int i;
    if (count < 1 || count > 200) return -1;
    for (i = 0; i < count; i++) {
        Who *w;
        unsigned h = mix(seed * 2654435761u + (unsigned)i * 40503u);
        if (g_n >= MAXCAST) return -1;
        w = &g_who[g_n++];
        memset(w, 0, sizeof *w);
        snprintf(w->id, sizeof w->id, "%.30s_%02d", prefix, i);
        w->skin = skins[h % 6]; w->hair = hairs[(h >> 4) % 8]; w->garment = gars[(h >> 8) % 8];
        w->style = (int)((h >> 12) % 4); w->body = (int)((h >> 16) % 3);
        w->trim.r = 255; w->trim.g = 210; w->trim.b = 74;
        w->mark = ((h >> 20) % 3) == 0;
    }
    return 0;
}
static int read_cast(const char *path) {
    FILE *f = fopen(path, "r");
    char line[600];
    int ln = 0;
    g_n = 0;
    if (!f) { fprintf(stderr, "no cast file\n"); return 2; }
    while (fgets(line, sizeof line, f)) {
        char *p = line, *k;
        ln++;
        if (!strncmp(line, "CAST |", 6)) {
            Who *w;
            char *id, *sk, *hs, *hc, *gc, *tc, *bd, *mk, *kd;
            p = line + 6;
            id = fld(&p); sk = fld(&p); hs = fld(&p); hc = fld(&p); gc = fld(&p); tc = fld(&p); bd = fld(&p); mk = fld(&p); kd = fld(&p);
            if (g_n >= MAXCAST || !id[0] || strlen(id) > 38) { fprintf(stderr, "line %d: bad id\n", ln); fclose(f); return 2; }
            w = &g_who[g_n];
            memset(w, 0, sizeof *w);
            snprintf(w->id, sizeof w->id, "%s", id);
            w->style = style_of(hs);
            if (w->style < 0 || parse_col(sk, &w->skin) || parse_col(hc, &w->hair) || parse_col(gc, &w->garment) || parse_col(tc, &w->trim)) {
                fprintf(stderr, "line %d: bad hair style or colour for %s\n", ln, id); fclose(f); return 2;
            }
            w->body = atoi(bd); w->mark = atoi(mk);
            w->kind = (!kd[0] || !strcmp(kd, "humanoid")) ? 0 : !strcmp(kd, "quad") ? 1 : !strcmp(kd, "blob") ? 2 : !strcmp(kd, "winged") ? 3 : !strcmp(kd, "plant") ? 4 : !strcmp(kd, "jelly") ? 5 : -1;
            if (w->kind < 0) { fprintf(stderr, "line %d: kind must be humanoid, quad, blob, winged, plant or jelly\n", ln); fclose(f); return 2; }
            if (w->body < 0 || w->body > 2) { fprintf(stderr, "line %d: body must be 0..2\n", ln); fclose(f); return 2; }
            g_n++;
        } else if (!strncmp(line, "GENERIC |", 9)) {
            char *pre, *cnt, *sd;
            p = line + 9;
            pre = fld(&p); cnt = fld(&p); sd = fld(&p);
            k = pre;
            if (!*k || add_generic(pre, atoi(cnt), (unsigned)strtoul(sd, NULL, 10)) != 0) { fprintf(stderr, "line %d: bad GENERIC row\n", ln); fclose(f); return 2; }
        }
    }
    fclose(f);
    if (g_n < 1) { fprintf(stderr, "cast has no rows\n"); return 2; }
    return 0;
}
/* A 144x144 face: the front standing frame's head and shoulders (x 8..40, y 0..36 of the cell), enlarged 4x with the same chunky pixels. */
static void face_from_sheet(const unsigned char *sheet, unsigned char *face) {
    int x, y, sx, sy;
    memset(face, 0, 144 * 144 * 4);
    for (y = 0; y < 36; y++) for (x = 0; x < 32; x++) {
        const unsigned char *s = sheet + (y * SW + (CW + 8 + x)) * 4;
        for (sy = 0; sy < 4; sy++) for (sx = 0; sx < 4; sx++) memcpy(face + ((y * 4 + sy) * 144 + (8 + x * 4 + sx)) * 4, s, 4);
    }
}
static int cast_run(const char *outdir) {
    unsigned char *sheet = malloc(SW * SH * 4), *big = NULL;
    int i, rc = 0, groups = (g_n + 7) / 8, g;
    if (!sheet) return 2;
    for (i = 0; i < g_n; i++) {
        char dir[900], path[1000];
        int k, opaque = 0;
        memset(sheet, 0, SW * SH * 4);
        draw_sheet(sheet, &g_who[i]);
        for (k = 0; k < SW * SH; k++) if (sheet[k * 4 + 3]) opaque++;
        snprintf(dir, sizeof dir, "%.500s/%.40s", outdir, g_who[i].id);
        mkp(dir);
        snprintf(path, sizeof path, "%.800s/sheet.png", dir);
        if (!stbi_write_png(path, SW, SH, 4, sheet, SW * 4)) { rc = 2; break; }
        snprintf(path, sizeof path, "%.800s/sprite.csv", dir);
        if (write_csv(path, sheet, SW, SH) != 0) { rc = 2; break; }
        { static unsigned char face[144 * 144 * 4]; face_from_sheet(sheet, face); snprintf(path, sizeof path, "%.800s/face.png", dir); if (!stbi_write_png(path, 144, 144, 4, face, 144 * 4)) { rc = 2; break; } }
        { static unsigned char bigs[288 * 384 * 4], bat[192 * 192 * 4]; int bx, by;
          for (by = 0; by < 384; by++) for (bx = 0; bx < 288; bx++) memcpy(bigs + (by * 288 + bx) * 4, sheet + ((by / 2) * SW + bx / 2) * 4, 4);
          for (by = 0; by < 192; by++) for (bx = 0; bx < 192; bx++) memcpy(bat + (by * 192 + bx) * 4, sheet + ((by / 4) * SW + CW + bx / 4) * 4, 4);
          snprintf(path, sizeof path, "%.800s/big.png", dir); if (!stbi_write_png(path, 288, 384, 4, bigs, 288 * 4)) { rc = 2; break; }
          snprintf(path, sizeof path, "%.800s/battler.png", dir); if (!stbi_write_png(path, 192, 192, 4, bat, 192 * 4)) { rc = 2; break; } }
        printf("CAST %s %dx%d frames=12 opaque=%d\n", g_who[i].id, SW, SH, opaque);
    }
    /* RPG Maker layout: 4 x 2 characters of 144x192 per 576x384 sheet, so it drops into an existing map */
    big = calloc((size_t)576 * 384, 4);
    { unsigned char *fs = calloc((size_t)576 * 288, 4);
      if (!fs) { free(sheet); free(big); return 2; }
      for (g = 0; g < groups && !rc; g++) { int j; char fp[900]; memset(fs, 0, (size_t)576 * 288 * 4);
        for (j = 0; j < 8 && g * 8 + j < g_n; j++) { int y; static unsigned char fc[144 * 144 * 4]; memset(sheet, 0, SW * SH * 4); draw_sheet(sheet, &g_who[g * 8 + j]); face_from_sheet(sheet, fc);
          for (y = 0; y < 144; y++) memcpy(fs + (((j / 4) * 144 + y) * 576 + (j % 4) * 144) * 4, fc + y * 144 * 4, 144 * 4); }
        snprintf(fp, sizeof fp, "%.800s/FaceSheet_%02d.png", outdir, g); if (!stbi_write_png(fp, 576, 288, 4, fs, 576 * 4)) rc = 2; }
      printf("FACES count=%d sheets=%d rc=%d\n", g_n, groups, rc); free(fs); }
    if (!big) { free(sheet); return 2; }
    for (g = 0; g < groups && !rc; g++) {
        int j;
        char path[900];
        memset(big, 0, (size_t)576 * 384 * 4);
        for (j = 0; j < 8 && g * 8 + j < g_n; j++) {
            int y;
            memset(sheet, 0, SW * SH * 4);
            draw_sheet(sheet, &g_who[g * 8 + j]);
            for (y = 0; y < SH; y++) memcpy(big + (((j / 4) * SH + y) * 576 + (j % 4) * SW) * 4, sheet + y * SW * 4, SW * 4);
        }
        snprintf(path, sizeof path, "%.800s/CastSheet_%02d.png", outdir, g);
        if (!stbi_write_png(path, 576, 384, 4, big, 576 * 4)) rc = 2;
    }
    printf("CASTSET characters=%d sheets=%d rc=%d\n", g_n, groups, rc);
    free(sheet); free(big);
    return rc;
}
/* slots <cast.pdl> <slots.pdl> <outroot>: compose the RPG Maker people and face sheets from the cast.
 *   SLOT | <RPG Maker sheet name, e.g. Actor1> | id1,id2,...,id8      (up to 8 ids; the rest of the sheet stays transparent)
 * Writes <outroot>/characters/<Name>.png (576x384) and <outroot>/faces/<Name>.png (576x288): the same names, sizes and 4 x 2 layout as the RPG Maker files, so the old levels load them unchanged.
 * Exit 2 on an unknown id, a row with more than 8 ids, or no rows. */
static int find_who(const char *id) { int i; for (i = 0; i < g_n; i++) if (!strcmp(g_who[i].id, id)) return i; return -1; }
static int slots_run(const char *slotf, const char *outroot) {
    FILE *f = fopen(slotf, "r");
    char line[900];
    int rows = 0, rc = 0;
    unsigned char *sheet = malloc(SW * SH * 4), *cs = malloc((size_t)576 * 384 * 4), *fs = malloc((size_t)576 * 288 * 4);
    if (!f || !sheet || !cs || !fs) { fprintf(stderr, "no slots file\n"); if (f) fclose(f); free(sheet); free(cs); free(fs); return 2; }
    while (fgets(line, sizeof line, f)) {
        char *p, *name, *ids, *tok, dir[900], path[1000];
        int j = 0, y;
        if (strncmp(line, "SLOT |", 6) != 0) continue;
        p = line + 6; name = fld(&p); ids = fld(&p);
        if (!name[0] || !ids[0] || strlen(name) > 40) { fprintf(stderr, "bad SLOT row\n"); rc = 2; break; }
        memset(cs, 0, (size_t)576 * 384 * 4); memset(fs, 0, (size_t)576 * 288 * 4);
        for (tok = strtok(ids, ","); tok; tok = strtok(NULL, ",")) {
            int wi = find_who(tok);
            static unsigned char fc[144 * 144 * 4];
            if (wi < 0 || j >= 8) { fprintf(stderr, "SLOT %s: unknown id or more than 8: %s\n", name, tok); rc = 2; break; }
            memset(sheet, 0, SW * SH * 4); draw_sheet(sheet, &g_who[wi]); face_from_sheet(sheet, fc);
            for (y = 0; y < SH; y++) memcpy(cs + (((j / 4) * SH + y) * 576 + (j % 4) * SW) * 4, sheet + y * SW * 4, SW * 4);
            for (y = 0; y < 144; y++) memcpy(fs + (((j / 4) * 144 + y) * 576 + (j % 4) * 144) * 4, fc + y * 144 * 4, 144 * 4);
            j++;
        }
        if (rc) break;
        snprintf(dir, sizeof dir, "%.600s/characters", outroot); mkp(dir);
        snprintf(path, sizeof path, "%.600s/%.40s.png", dir, name); if (!stbi_write_png(path, 576, 384, 4, cs, 576 * 4)) { rc = 2; break; }
        snprintf(dir, sizeof dir, "%.600s/faces", outroot); mkp(dir);
        snprintf(path, sizeof path, "%.600s/%.40s.png", dir, name); if (!stbi_write_png(path, 576, 288, 4, fs, 576 * 4)) { rc = 2; break; }
        rows++;
    }
    fclose(f); free(sheet); free(cs); free(fs);
    if (!rc && rows == 0) { fprintf(stderr, "slots file has no SLOT rows\n"); rc = 2; }
    if (!rc) printf("SLOTS sheets=%d characters_and_faces_written\n", rows);
    return rc;
}

int main(int argc, char **argv) {
    if (argc == 3 && !strcmp(argv[1], "cast-check")) { int r = read_cast(argv[2]); if (!r) printf("CASTCHECK characters=%d\n", g_n); return r; }
    if (argc == 4 && !strcmp(argv[1], "cast")) { int r = read_cast(argv[2]); return r ? r : cast_run(argv[3]); }
    if (argc == 5 && !strcmp(argv[1], "slots")) { int r = read_cast(argv[2]); return r ? r : slots_run(argv[3], argv[4]); }
    fprintf(stderr, "usage: cast <cast.pdl> <outdir> | cast-check <cast.pdl> | slots <cast.pdl> <slots.pdl> <outroot>\n");
    return 2;
}
