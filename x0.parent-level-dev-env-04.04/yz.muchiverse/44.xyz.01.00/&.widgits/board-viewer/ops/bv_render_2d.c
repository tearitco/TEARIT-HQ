/* bv_render_2d.c - the flat top-down tile view for pc-hq's 2D
 * (render_mode == 0) board. PCHQ-2D-TILE-VIEW.md P1.
 *
 * A plain painter, NO raymarch, NO chrome, NO legend/status text:
 *   - each board cell filled with its terrain-legend colour,
 *   - a manually drawn "matrix" grid on every cell boundary,
 *   - the xelector as an inset highlight box,
 *   - entities as solid colour squares.
 * Emoji glyphs and real palette tilesets are P2 (view_2d_style, the
 * `` ` `` toggle) - this pass is colour-only so the pipeline (bv_dispatch
 * routing render_mode==0 here, projector publishing rgb_frame_2d.raw,
 * no bv_compose_frame chrome) can be verified end to end.
 *
 * Reads (cwd = the board-viewer session dir, or $PRISC_PROJECT_ROOT):
 *   pieces/system/bv_state.txt        focused_project_root, selector_x/y, current_z
 *   pieces/system/house_root.txt      -> house root (for desk_grid.pdl)
 *   <focused>/pieces/system/board.txt            flat glyph grid (one row per line)
 *   <focused>/<z_base><current_z>.txt            if a z-manifest exists
 *   <focused>/pieces/system/terrain_legend.txt   glyph|height|r|g|b|asset|name
 *   <focused>/pieces/system/entities.txt         pos_x=,pos_y=,hex= (or r/g/b)
 *   <house>/#.desktop/desk_grid.pdl              GRID | cell_px | N   (default 80)
 * Writes:
 *   pieces/display/rgb_frame_2d.raw              RGBA, w*h*4
 *   pieces/display/rgb_frame_2d.receipt.txt      frame_w=%d\nframe_h=%d\n
 *
 * Self-contained, no shared headers (board-viewer house convention).
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <dirent.h>

#include "bv_cjk_glyph.h"   /* view_2d_style=ascii: coloured CJK glyph per cell */
#include "bv_move_range.c"   /* shared Move range finder file helpers */

#define MAX_LINE     1024
#define PATH_BUF     4096
#define MAX_DIM      256          /* board cells per side, hard ceiling */
#define MAX_FRAME_PX 2400         /* per side; CELL shrinks to fit P1's whole-board view */
#define MAX_LEGEND   64
#define MAX_ENT      256

static char project_root[PATH_BUF] = ".";
static char house_root[PATH_BUF]   = "";
static char focused_root[PATH_BUF] = "";

static FILE *host_fopen(const char *p, const char *m) { return fopen(p, m); }

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) { snprintf(project_root, sizeof(project_root), "%s", env); return; }
    if (!getcwd(project_root, sizeof(project_root))) snprintf(project_root, sizeof(project_root), ".");
}

static void read_kv_str(const char *path, const char *key, char *out, size_t osz) {
    out[0] = '\0';
    FILE *f = host_fopen(path, "r");
    if (!f) return;
    size_t kl = strlen(key);
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, kl) == 0 && line[kl] == '=') {
            char *v = line + kl + 1;
            v[strcspn(v, "\r\n")] = '\0';
            snprintf(out, osz, "%s", v);
            break;
        }
    }
    fclose(f);
}
static int read_kv_int(const char *path, const char *key, int def) {
    char b[64]; read_kv_str(path, key, b, sizeof(b));
    return b[0] ? atoi(b) : def;
}

static void load_house_root(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/house_root.txt", project_root);
    FILE *f = host_fopen(path, "r");
    if (!f) return;
    if (fgets(house_root, sizeof(house_root), f)) {
        if ((unsigned char)house_root[0] == 0xEF && (unsigned char)house_root[1] == 0xBB &&
            (unsigned char)house_root[2] == 0xBF)
            memmove(house_root, house_root + 3, strlen(house_root + 3) + 1);
        house_root[strcspn(house_root, "\r\n")] = '\0';
    }
    fclose(f);
}

/* focused_project_root is usually absolute; if relative, resolve vs house_root */
static void resolve_focused(const char *raw) {
    if (!raw || !raw[0]) { focused_root[0] = '\0'; return; }
    if (raw[0] == '/' || (raw[0] && raw[1] == ':')) { snprintf(focused_root, sizeof(focused_root), "%s", raw); return; }
    if (house_root[0]) snprintf(focused_root, sizeof(focused_root), "%s/%s", house_root, raw);
    else snprintf(focused_root, sizeof(focused_root), "%s", raw);
}

/* ---- terrain legend: glyph -> rgb ---- */
typedef struct { char glyph; unsigned char r, g, b; } Leg;
static Leg  g_leg[MAX_LEGEND];
static int  g_nleg = 0;
static char g_leg_hex[MAX_LEGEND][16];   /* asset_hex column, parallel to g_leg */
static char g_leg_cjk[MAX_LEGEND][8];    /* optional cjk glyph column (view_2d_style=ascii) */
static void load_legend(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/terrain_legend.txt", focused_root);
    FILE *f = host_fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (g_nleg < MAX_LEGEND && fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (!line[0] || (line[0] == '#' && line[1] != '|')) continue;   /* comment, not the '#' wall glyph */
        char *sv = NULL;
        char *g  = strtok_r(line, "|", &sv);
        strtok_r(NULL, "|", &sv);                    /* height (unused in 2D) */
        char *rt = strtok_r(NULL, "|", &sv);
        char *gt = strtok_r(NULL, "|", &sv);
        char *bt = strtok_r(NULL, "|", &sv);
        char *at = strtok_r(NULL, "|", &sv);        /* asset_hex ("-" = none) */
        char *nt = strtok_r(NULL, "|", &sv);        /* name (unused in 2D) */
        char *ct = strtok_r(NULL, "|", &sv);        /* cjk glyph (optional, "-"/empty = none) */
        (void)nt;
        if (!g || !g[0] || !rt || !gt || !bt) continue;
        g_leg[g_nleg].glyph = g[0];
        g_leg[g_nleg].r = (unsigned char)atoi(rt);
        g_leg[g_nleg].g = (unsigned char)atoi(gt);
        g_leg[g_nleg].b = (unsigned char)atoi(bt);
        g_leg_hex[g_nleg][0] = '\0';
        if (at && at[0] && strcmp(at, "-") != 0)
            snprintf(g_leg_hex[g_nleg], sizeof(g_leg_hex[0]), "%s", at);
        g_leg_cjk[g_nleg][0] = '\0';
        if (ct && ct[0] && strcmp(ct, "-") != 0)
            snprintf(g_leg_cjk[g_nleg], sizeof(g_leg_cjk[0]), "%s", ct);
        g_nleg++;
    }
    fclose(f);
}
static int legend_idx(char glyph) {
    for (int i = 0; i < g_nleg; i++) if (g_leg[i].glyph == glyph) return i;
    return -1;
}
static int legend_rgb(char glyph, unsigned char *r, unsigned char *g, unsigned char *b) {
    int i = legend_idx(glyph);
    if (i < 0) return 0;
    *r = g_leg[i].r; *g = g_leg[i].g; *b = g_leg[i].b; return 1;
}

/* ---- emoji sprites (pieces/registry/emoji_assets/<HEX>/voxels_16.csv,
 * a flat 16x16 RGBA sheet - the SAME asset bv_render_3d textures with).
 * Loaded once per hex, scaled per cell, alpha-composited. ---- */
#define EMO_N 64
#define EMO_RES 16
static struct { char hex[16]; int ok; unsigned char px[EMO_RES*EMO_RES*4]; } g_emo[EMO_N];
static int g_nemo = 0;
static const unsigned char *load_emoji16(const char *hex) {
    if (!hex || !hex[0]) return NULL;
    for (int i = 0; i < g_nemo; i++)
        if (strcmp(g_emo[i].hex, hex) == 0) return g_emo[i].ok ? g_emo[i].px : NULL;
    if (g_nemo >= EMO_N) return NULL;
    int slot = g_nemo++;
    snprintf(g_emo[slot].hex, sizeof(g_emo[slot].hex), "%s", hex);
    g_emo[slot].ok = 0;
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/registry/emoji_assets/%s/voxels_16.csv", project_root, hex);
    FILE *f = host_fopen(path, "r");
    if (!f && focused_root[0]) {   /* session may not have copied this asset - fall back to the host project */
        snprintf(path, sizeof(path), "%s/pieces/registry/emoji_assets/%s/voxels_16.csv", focused_root, hex);
        f = host_fopen(path, "r");
    }
    if (!f) return NULL;
    char line[MAX_LINE];
    int n = 0;
    while (n < EMO_RES*EMO_RES && fgets(line, sizeof(line), f)) {
        int r, g, b, a;
        if (line[0] == '#') continue;
        if (sscanf(line, "%d,%d,%d,%d", &r, &g, &b, &a) == 4) {
            g_emo[slot].px[n*4+0] = (unsigned char)r;
            g_emo[slot].px[n*4+1] = (unsigned char)g;
            g_emo[slot].px[n*4+2] = (unsigned char)b;
            g_emo[slot].px[n*4+3] = (unsigned char)a;
            n++;
        }
    }
    fclose(f);
    g_emo[slot].ok = (n == EMO_RES*EMO_RES);
    return g_emo[slot].ok ? g_emo[slot].px : NULL;
}
/* blit a 16x16 RGBA sprite scaled (nearest) into cellxcell at (dx,dy),
 * alpha over whatever is already in the frame. */
static void blit_emoji(unsigned char *frame, int W, int dx, int dy, int cell, const unsigned char *e16) {
    for (int yy = 0; yy < cell; yy++) {
        int sy = yy * EMO_RES / cell; if (sy >= EMO_RES) sy = EMO_RES - 1;
        for (int xx = 0; xx < cell; xx++) {
            int sx = xx * EMO_RES / cell; if (sx >= EMO_RES) sx = EMO_RES - 1;
            const unsigned char *s = e16 + (sy*EMO_RES + sx)*4;
            int a = s[3];
            if (a == 0) continue;
            unsigned char *d = frame + ((size_t)(dy+yy) * W + (dx+xx)) * 4;
            if (a >= 255) { d[0]=s[0]; d[1]=s[1]; d[2]=s[2]; d[3]=255; }
            else {
                d[0] = (unsigned char)((s[0]*a + d[0]*(255-a)) / 255);
                d[1] = (unsigned char)((s[1]*a + d[1]*(255-a)) / 255);
                d[2] = (unsigned char)((s[2]*a + d[2]*(255-a)) / 255);
                d[3] = 255;
            }
        }
    }
}

/* Pal picture. sprite.csv is "r,g,b,a" after a # resolution line.
 * Samples an 8x8 of the opaque pixels into the cell. Returns 1 when
 * any pixel was drawn. */
static int blit_sprite_csv(unsigned char *frame, int W, int dx, int dy, int cell, const char *path) {
    FILE *f = host_fopen(path, "r");
    if (!f) return 0;
    int res = 64, data = 0, i = 0, any = 0;
    unsigned char tile[8][8][4];
    memset(tile, 0, sizeof(tile));
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#') {
            int r = 0;
            if (sscanf(line, "# resolution=%d", &r) == 1 && r > 0) res = r;
            continue;
        }
        if (!data) { if (strncmp(line, "r,g,b", 5) == 0) data = 1; continue; }
        int r, g, b, a;
        if (sscanf(line, "%d,%d,%d,%d", &r, &g, &b, &a) != 4) continue;
        int x = i % res, y = i / res;
        i++;
        if (y >= res) break;
        int sx = x * 8 / res; if (sx > 7) sx = 7;
        int sy = y * 8 / res; if (sy > 7) sy = 7;
        if (a > tile[sy][sx][3]) {
            tile[sy][sx][0] = (unsigned char)r; tile[sy][sx][1] = (unsigned char)g;
            tile[sy][sx][2] = (unsigned char)b; tile[sy][sx][3] = (unsigned char)a;
            if (a) any = 1;
        }
    }
    fclose(f);
    if (!any) return 0;
    for (int yy = 0; yy < cell; yy++) {
        int sy = yy * 8 / cell; if (sy > 7) sy = 7;
        for (int xx = 0; xx < cell; xx++) {
            int sx = xx * 8 / cell; if (sx > 7) sx = 7;
            unsigned char *s = tile[sy][sx];
            if (s[3] == 0) continue;
            unsigned char *d = frame + ((size_t)(dy + yy) * W + (dx + xx)) * 4;
            d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = 255;
        }
    }
    return 1;
}

/* ---- ascii/CJK view: one tinted coverage glyph filling the cell ---- */
static void blit_cjk(unsigned char *frame, int W, int dx, int dy, int cell,
                     unsigned int cp, unsigned char r, unsigned char g, unsigned char b) {
    const unsigned char *cov = bv_cjk_coverage(cp, cell);
    if (!cov) return;
    for (int yy = 0; yy < cell; yy++) {
        for (int xx = 0; xx < cell; xx++) {
            unsigned int a = cov[yy * cell + xx];
            if (!a) continue;
            unsigned char *d = frame + ((size_t)(dy + yy) * W + (dx + xx)) * 4;
            d[0] = (unsigned char)((r * a + d[0] * (255 - a)) / 255);
            d[1] = (unsigned char)((g * a + d[1] * (255 - a)) / 255);
            d[2] = (unsigned char)((b * a + d[2] * (255 - a)) / 255);
            d[3] = 255;
        }
    }
}

/* ---- entities: pos + colour ---- */
typedef struct { int x, y, z; unsigned char r, g, b; char hex[16]; char cjk[8]; char spr[180]; } Ent;
static Ent g_ent[MAX_ENT];
static int g_nent = 0;

/* first UTF-8 scalar of s -> uppercase hex codepoint string (for
 * phymoji emoji.txt sidecars, which store the raw emoji char). */
static void utf8_first_hex(const char *s, char *out, size_t osz) {
    out[0] = '\0';
    const unsigned char *p = (const unsigned char *)s;
    unsigned cp = 0;
    if (p[0] < 0x80) cp = p[0];
    else if ((p[0] & 0xE0) == 0xC0 && p[1]) cp = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);
    else if ((p[0] & 0xF0) == 0xE0 && p[1] && p[2]) cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
    else if ((p[0] & 0xF8) == 0xF0 && p[1] && p[2] && p[3])
        cp = ((p[0] & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
    if (cp) snprintf(out, osz, "%X", cp);
}
static void hex_to_rgb(const char *hx, unsigned char *r, unsigned char *g, unsigned char *b) {
    if (hx[0] == '#') hx++;
    unsigned v = (unsigned)strtoul(hx, NULL, 16);
    *r = (v >> 16) & 0xFF; *g = (v >> 8) & 0xFF; *b = v & 0xFF;
}
static void load_entities(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/entities.txt", focused_root);
    FILE *f = host_fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    Ent cur; int have = 0;
    memset(&cur, 0, sizeof(cur));
    while (g_nent < MAX_ENT && fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (!line[0]) {                              /* blank line = record separator */
            if (have) g_ent[g_nent++] = cur;
            memset(&cur, 0, sizeof(cur)); have = 0; continue;
        }
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0'; char *k = line, *v = eq + 1;
        if      (!strcmp(k, "pos_x")) { cur.x = atoi(v); have = 1; }
        else if (!strcmp(k, "pos_y")) { cur.y = atoi(v); have = 1; }
        else if (!strcmp(k, "hex") || !strcmp(k, "color") || !strcmp(k, "colour")) {
            if (v[0]) { hex_to_rgb(v, &cur.r, &cur.g, &cur.b); have = 1; }
        }
    }
    if (have) g_ent[g_nent++] = cur;
    fclose(f);
}

/* hero (pieces/hero_01/state.txt) + world animals
 * (pieces/world_01/animals.txt: "name,x,y,z"), each shown only on its
 * own z-slice - same as bv_compose_frame.c's load_hero_as_2d /
 * load_phymoji_entities_as_2d. Emoji from the phymoji_assets/<id>/
 * emoji.txt sidecar. */
static void read_first_line(const char *path, char *out, size_t osz) {
    out[0] = '\0';
    FILE *f = host_fopen(path, "r");
    if (!f) return;
    if (fgets(out, osz, f)) out[strcspn(out, "\r\n")] = '\0';
    fclose(f);
}
/* An actor shows on the z-slice being viewed AND on the slice one
 * below it (its feet stand on that surface - a plain z==cur_z match
 * hides every surface-standing piece whenever you look at the ground
 * layer, which is exactly "why don't I see the player"). */
/* REAL, NEW 2026-09-15 - render_mode==2 (side view, see load_side_
 * board()'s own header comment) wants EVERY actor regardless of
 * height (it filters by ROW instead, in main()), not the normal
 * single-height-slice match every other render_mode uses. */
static int g_any_z = 0;
static int actor_on_z(int actor_z, int cur_z) {
    if (g_any_z) return 1;
    return actor_z == cur_z || actor_z == cur_z + 1;
}
static void add_actor(const char *asset_id, int x, int y, int z) {
    if (g_nent >= MAX_ENT) return;
    char emo[PATH_BUF], glyph[16];
    Ent *e = &g_ent[g_nent];
    memset(e, 0, sizeof(*e));
    e->x = x; e->y = y; e->z = z; e->r = e->g = e->b = 90;
    snprintf(emo, sizeof(emo), "%s/pieces/registry/phymoji_assets/%s/emoji.txt", focused_root, asset_id);
    read_first_line(emo, glyph, sizeof(glyph));
    utf8_first_hex(glyph, e->hex, sizeof(e->hex));
    snprintf(emo, sizeof(emo), "%s/pieces/registry/phymoji_assets/%s/cjk.txt", focused_root, asset_id);
    read_first_line(emo, e->cjk, sizeof(e->cjk));
    if (e->hex[0] || e->cjk[0]) g_nent++;   /* skip if the asset has neither sidecar */
}
/* "asset_id,x,y,z" list file (world_01/animals.txt, phymoji_entities.txt) */
static void load_actor_list(const char *rel_path, int cur_z) {
    char p[PATH_BUF];
    snprintf(p, sizeof(p), "%s/%s", focused_root, rel_path);
    FILE *f = host_fopen(p, "r");
    if (!f) return;
    char line[MAX_LINE], name[64];
    int x, y, z;
    while (g_nent < MAX_ENT && fgets(line, sizeof(line), f)) {
        if (sscanf(line, "%63[^,],%d,%d,%d", name, &x, &y, &z) != 4) continue;
        if (actor_on_z(z, cur_z)) add_actor(name, x, y, z);
    }
    fclose(f);
}
/* Active livedesk page: sessions/<id>/session.pdl names the desk,
 * desks/<desk>.pdl holds DESK rows. Cell columns are the 6th and 7th
 * fields. A pixel field at or above 40 is cells times 80. */
static void field_trim(char *s) {
    char *a = s;
    while (*a == ' ' || *a == '\t') a++;
    if (a != s) memmove(s, a, strlen(a) + 1);
    int n = (int)strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n')) s[--n] = '\0';
}
static int read_pdl_value(const char *path, const char *key, char *out, int n) {
    FILE *f = host_fopen(path, "r");
    out[0] = '\0';
    if (!f) return 0;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        char *p1 = strchr(line, '|');
        if (!p1) continue;
        char *p2 = strchr(p1 + 1, '|');
        if (!p2) continue;
        *p2 = '\0';
        field_trim(p1 + 1);
        if (strcmp(p1 + 1, key) != 0) continue;
        char *val = p2 + 1;
        field_trim(val);
        char *nl = strchr(val, '|');
        if (nl) *nl = '\0';
        field_trim(val);
        snprintf(out, n, "%s", val);
        fclose(f);
        return out[0] != '\0';
    }
    fclose(f);
    return 0;
}
/* open_book_page.txt:
 *   source=desk  — the pdl= path Synch wrote. A later desk switch
 *                   does not move this view until the next Synch.
 *   source=board — the board's own map; do not read a desk file
 *   no source, but pdl= — older Synch pin; same as source=desk
 * Returns 1 and writes the desk path, -1 when the board owns the
 * page, 0 when this file does not decide (caller may use active_desk). */
static int page_bound_pdl(const char *house, char *out, int n) {
    char ob[PATH_BUF], line[PATH_BUF], source[32] = "", stored[PATH_BUF] = "";
    snprintf(ob, sizeof(ob), "%s/@.apps/piececraft-hq/pieces/display/open_book_page.txt", house);
    FILE *f = host_fopen(ob, "r");
    if (!f) return 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "source=", 7) == 0) {
            snprintf(source, sizeof(source), "%s", line + 7);
            source[strcspn(source, "\r\n")] = '\0';
        } else if (strncmp(line, "pdl=", 4) == 0) {
            snprintf(stored, sizeof(stored), "%s", line + 4);
            stored[strcspn(stored, "\r\n")] = '\0';
        }
    }
    fclose(f);
    if (strcmp(source, "board") == 0) {
        if (n > 0) out[0] = '\0';
        return -1;
    }
    if (strcmp(source, "desk") != 0 && !stored[0]) return 0;
    if (!stored[0]) return 0;
    FILE *t = host_fopen(stored, "r");
    if (!t) return 0;
    fclose(t);
    snprintf(out, n, "%s", stored);
    return 1;
}
static int page_file(const char *house, char *out, int n) {
    int bound = page_bound_pdl(house, out, n);
    if (bound < 0) return 0;
    if (bound > 0) return 1;
    char users[PATH_BUF];
    snprintf(users, sizeof(users), "%s/xyzfs/users", house);
    DIR *d = opendir(users);
    if (!d) return 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char sess[PATH_BUF], rootpdl[PATH_BUF], active[128];
        snprintf(sess, sizeof(sess), "%s/%s/home/livedesk/sessions", users, e->d_name);
        snprintf(rootpdl, sizeof(rootpdl), "%s/session.pdl", sess);
        if (!read_pdl_value(rootpdl, "active_session", active, sizeof(active))) continue;
        char sp[PATH_BUF], desk[128];
        snprintf(sp, sizeof(sp), "%s/%s/session.pdl", sess, active);
        if (!read_pdl_value(sp, "active_desk", desk, sizeof(desk))) continue;
        snprintf(out, n, "%s/%s/desks/%s.pdl", sess, active, desk);
        closedir(d);
        return 1;
    }
    closedir(d);
    return 0;
}
static void read_page_rows(const char *pdl, int cur_z) {
    FILE *f = host_fopen(pdl, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (g_nent < MAX_ENT && fgets(line, sizeof(line), f)) {
        if (strncmp(line, "DESK", 4) != 0) continue;
        char *fld[8];
        int nf = 0;
        char *p = line;
        while (nf < 8 && (p = strchr(p, '|'))) {
            p++;
            fld[nf++] = p;
        }
        if (nf < 6) continue;
        for (int i = 0; i < nf; i++) {
            char *bar = strchr(fld[i], '|');
            if (bar) *bar = '\0';
            field_trim(fld[i]);
        }
        if (!strcmp(fld[0], "hero_01") || !strcmp(fld[0], "tree_small") || !strcmp(fld[0], "chicken")
            || !strcmp(fld[0], "xelector_01") || !strcmp(fld[0], "camera_01"))
            continue;
        int cx = atoi(fld[4]);
        int cy = atoi(fld[5]);
        int px = atoi(fld[2]);
        int py = atoi(fld[3]);
        if (cx == 0 && cy == 0 && (px >= 40 || py >= 40 || px <= -40 || py <= -40)) {
            cx = px / 80; cy = py / 80;
        }
        Ent *e = &g_ent[g_nent++];
        memset(e, 0, sizeof(*e));
        e->x = cx; e->y = cy; e->z = cur_z;
        e->r = 80; e->g = 200; e->b = 255;
        if (nf > 6 && fld[6][0] && strcmp(fld[6], ".") != 0)
            utf8_first_hex(fld[6], e->hex, sizeof(e->hex));
        if (nf > 1 && fld[1][0] && house_root[0])
            snprintf(e->spr, sizeof(e->spr), "%s/%s/sprite.csv", house_root, fld[1]);
    }
    fclose(f);
}
static int page_has_name(const char *pdl, const char *want) {
    FILE *f = host_fopen(pdl, "r");
    if (!f) return 0;
    char line[MAX_LINE], name[64];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "DESK", 4) != 0) continue;
        char *bar = strchr(line, '|');
        if (!bar) continue;
        snprintf(name, sizeof(name), "%s", bar + 1);
        char *bar2 = strchr(name, '|');
        if (bar2) *bar2 = '\0';
        field_trim(name);
        if (strcmp(name, want) == 0) { found = 1; break; }
    }
    fclose(f);
    return found;
}
static void page_append_row(const char *pdl, const char *name, const char *path, int cx, int cy,
                            const char *glyph, int tail) {
    FILE *f = host_fopen(pdl, "a");
    if (!f) return;
    fprintf(f, "DESK | %s | %s | %d | %d | %d | %d | %s | %d\n",
            name, path, cx * 80, cy * 80, cx, cy, glyph && glyph[0] ? glyph : ".", tail);
    fclose(f);
}
/* One pass over the old lists. Writes a row only when that name is absent. */
static void page_seed_sprites(const char *pdl) {
    char path[PATH_BUF], b[32];
    int hx = 0, hy = 0;
    if (page_has_name(pdl, "hero_01")) goto trees;
    snprintf(path, sizeof(path), "%s/pieces/hero_01/state.txt", focused_root);
    read_kv_str(path, "pos_x", b, sizeof(b)); if (b[0]) hx = atoi(b);
    read_kv_str(path, "pos_y", b, sizeof(b)); if (b[0]) hy = atoi(b);
    page_append_row(pdl, "hero_01", "@.apps/piececraft-hq/pieces/hero_01", hx, hy, ".", 0);
trees:
    if (!page_has_name(pdl, "tree_small")) {
        snprintf(path, sizeof(path), "%s/pieces/world_01/phymoji_entities.txt", focused_root);
        FILE *f = host_fopen(path, "r");
        if (f) {
            char line[128];
            while (fgets(line, sizeof(line), f)) {
                char id[64]; int x, y, z;
                if (sscanf(line, "%63[^,],%d,%d,%d", id, &x, &y, &z) != 4) continue;
                page_append_row(pdl, id, "@.apps/piececraft-hq/pieces/world_01", x, y, ".", 0);
            }
            fclose(f);
        }
    }
    if (!page_has_name(pdl, "chicken")) {
        snprintf(path, sizeof(path), "%s/pieces/world_01/animals.txt", focused_root);
        FILE *f = host_fopen(path, "r");
        if (f) {
            char line[128];
            while (fgets(line, sizeof(line), f)) {
                char id[64]; int x, y, z;
                if (sscanf(line, "%63[^,],%d,%d,%d", id, &x, &y, &z) != 4) continue;
                page_append_row(pdl, id, "@.apps/piececraft-hq/pieces/world_01", x, y, ".", z);
            }
            fclose(f);
        }
    }
    if (!page_has_name(pdl, "xelector_01")) {
        int xx = 0, xy = 0, xz = 0;
        char poss[64] = ".";
        snprintf(path, sizeof(path), "%s/pieces/xelector_01/state.txt", focused_root);
        read_kv_str(path, "pos_x", b, sizeof(b)); if (b[0]) xx = atoi(b);
        read_kv_str(path, "pos_y", b, sizeof(b)); if (b[0]) xy = atoi(b);
        read_kv_str(path, "pos_z", b, sizeof(b)); if (b[0]) xz = atoi(b);
        read_kv_str(path, "possessed_id", poss, sizeof(poss));
        if (!poss[0]) snprintf(poss, sizeof(poss), ".");
        page_append_row(pdl, "xelector_01", "@.apps/piececraft-hq/pieces/xelector_01", xx, xy, poss, xz);
    }
    if (!page_has_name(pdl, "camera_01")) {
        int mode = 2, yaw = 180, pitch = 6, panx = 0, pany = 0, panz = 0, zl = 0;
        char g[96];
        snprintf(path, sizeof(path), "%s/pieces/system/bv_state.txt", project_root);
        read_kv_str(path, "camera_mode", b, sizeof(b)); if (b[0]) mode = atoi(b);
        read_kv_str(path, "cam_yaw", b, sizeof(b)); if (b[0]) yaw = atoi(b);
        read_kv_str(path, "cam_pitch", b, sizeof(b)); if (b[0]) pitch = atoi(b);
        read_kv_str(path, "cam_pan_x", b, sizeof(b)); if (b[0]) panx = atoi(b);
        read_kv_str(path, "cam_pan_y", b, sizeof(b)); if (b[0]) pany = atoi(b);
        read_kv_str(path, "cam_pan_z", b, sizeof(b)); if (b[0]) panz = atoi(b);
        read_kv_str(path, "cam_z_level", b, sizeof(b)); if (b[0]) zl = atoi(b);
        snprintf(g, sizeof(g), "m=%d,y=%d,p=%d,z=%d,h=%d", mode, yaw, pitch, panz, zl);
        page_append_row(pdl, "camera_01", "@.apps/piececraft-hq/pieces/display", panx, pany, g, 0);
    }
}
static int page_named_cells(const char *house, const char *want, int *xs, int *ys, int max) {
    char pdl[PATH_BUF];
    int n = 0;
    if (!house || !house[0] || !page_file(house, pdl, sizeof(pdl))) return 0;
    FILE *f = host_fopen(pdl, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    while (n < max && fgets(line, sizeof(line), f)) {
        if (strncmp(line, "DESK", 4) != 0) continue;
        char *fld[8];
        int nf = 0;
        char *p = line;
        while (nf < 8 && (p = strchr(p, '|'))) { p++; fld[nf++] = p; }
        if (nf < 6) continue;
        for (int i = 0; i < nf; i++) {
            char *bar = strchr(fld[i], '|');
            if (bar) *bar = '\0';
            field_trim(fld[i]);
        }
        if (strcmp(fld[0], want) != 0) continue;
        int cx = atoi(fld[4]), cy = atoi(fld[5]);
        int px = atoi(fld[2]), py = atoi(fld[3]);
        if (cx == 0 && cy == 0 && (px >= 40 || py >= 40 || px <= -40 || py <= -40)) {
            cx = px / 80; cy = py / 80;
        }
        xs[n] = cx; ys[n] = cy; n++;
    }
    fclose(f);
    return n;
}
/* glyph is field 7 (possessed id, or the camera's m,y,p,z,h pack).
 * tail is field 8 (xelector z). */
static int page_row_meta(const char *house, const char *want, int *cx, int *cy,
                         char *glyph, int glen, int *tail) {
    char pdl[PATH_BUF];
    if (!house || !house[0] || !page_file(house, pdl, sizeof(pdl))) return 0;
    FILE *f = host_fopen(pdl, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "DESK", 4) != 0) continue;
        char *fld[8];
        int nf = 0;
        char *p = line;
        while (nf < 8 && (p = strchr(p, '|'))) { p++; fld[nf++] = p; }
        if (nf < 6) continue;
        for (int i = 0; i < nf; i++) {
            char *bar = strchr(fld[i], '|');
            if (bar) *bar = '\0';
            field_trim(fld[i]);
        }
        if (strcmp(fld[0], want) != 0) continue;
        *cx = atoi(fld[4]); *cy = atoi(fld[5]);
        if (glyph && glen > 0) snprintf(glyph, glen, "%s", nf > 6 ? fld[6] : ".");
        if (tail) *tail = nf > 7 ? atoi(fld[7]) : 0;
        found = 1;
        break;
    }
    fclose(f);
    return found;
}
static void load_actors(int cur_z) {
    char pdl[PATH_BUF], bound[PATH_BUF];
    /* A desk page is the whole list. Missing tree_small / chicken
     * means they left with the old book. Do not seed them back in,
     * and do not read the private txt files. */
    int desk_page = house_root[0] && page_bound_pdl(house_root, bound, sizeof(bound)) > 0;
    if (!desk_page && house_root[0] && page_file(house_root, pdl, sizeof(pdl)))
        page_seed_sprites(pdl);
    int xs[16], ys[16], n = 0;
    if (!desk_page) n = page_named_cells(house_root, "hero_01", xs, ys, 16);
    if (n > 0) add_actor("hero_humanoid", xs[0], ys[0], cur_z);
    else if (!desk_page) {
        char p[PATH_BUF], b[32];
        snprintf(p, sizeof(p), "%s/pieces/hero_01/state.txt", focused_root);
        int hx = -1, hy = -1, hz = -999;
        read_kv_str(p, "pos_x", b, sizeof(b)); if (b[0]) hx = atoi(b);
        read_kv_str(p, "pos_y", b, sizeof(b)); if (b[0]) hy = atoi(b);
        read_kv_str(p, "pos_z", b, sizeof(b)); if (b[0]) hz = atoi(b);
        if (hx >= 0 && hy >= 0 && actor_on_z(hz, cur_z)) add_actor("hero_humanoid", hx, hy, hz);
    }
    n = 0;
    if (!desk_page) n = page_named_cells(house_root, "tree_small", xs, ys, 16);
    if (n > 0) { for (int i = 0; i < n; i++) add_actor("tree_small", xs[i], ys[i], cur_z); }
    else if (!desk_page) load_actor_list("pieces/world_01/phymoji_entities.txt", cur_z);
    n = 0;
    if (!desk_page) n = page_named_cells(house_root, "chicken", xs, ys, 16);
    if (n > 0) { for (int i = 0; i < n; i++) add_actor("chicken", xs[i], ys[i], cur_z); }
    else if (!desk_page) load_actor_list("pieces/world_01/animals.txt", cur_z);
    /* Livedesk page file, read every frame. The desk writes the row
     * when a pal moves. Cyan square. hero_01, tree_small, and chicken
     * are rows too, drawn above as their own sprites. */
    if (house_root[0] && page_file(house_root, pdl, sizeof(pdl)))
        read_page_rows(pdl, cur_z);
}

/* ---- board glyphs (one z-slice) ---- */
static char g_board[MAX_DIM][MAX_DIM];
static int  g_bw = 0, g_bh = 0;
static void load_board(int current_z) {
    /* z-manifest? board_manifest.txt: "z_base=<prefix>" + "z_count=N" */
    char man[PATH_BUF], zbase[256] = "";
    int zcount = 0;
    snprintf(man, sizeof(man), "%s/pieces/system/board_manifest.txt", focused_root);
    read_kv_str(man, "z_base", zbase, sizeof(zbase));
    zcount = read_kv_int(man, "z_count", 0);

    char path[PATH_BUF];
    if (zbase[0] && zcount > 0) {
        int z = current_z; if (z < 0) z = 0; if (z >= zcount) z = zcount - 1;
        snprintf(path, sizeof(path), "%s/%s%d.txt", focused_root, zbase, z);
    } else {
        snprintf(path, sizeof(path), "%s/pieces/system/board.txt", focused_root);
    }
    FILE *f = host_fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    int row = 0;
    while (row < MAX_DIM && fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        int len = (int)strlen(line);
        if (len == 0) continue;
        if (len > MAX_DIM) len = MAX_DIM;
        memcpy(g_board[row], line, len);
        if (len > g_bw) g_bw = len;
        row++;
    }
    if (row > g_bh) g_bh = row;
    fclose(f);
}

/* REAL, NEW 2026-09-15, direct live request ("we wanted to add a 5th
 * [camera mode] for 'side scroll' (mario) type mode... its supposed to
 * use 'emoji mode' like camera option '0'") - render_mode==2, a real
 * side-on slice of the SAME flat/2D data this file already draws
 * top-down for render_mode==0. Reuses g_board[MAX_DIM][MAX_DIM], but
 * here the first index is a Z-LAYER (height), not a board row: for one
 * fixed board row (the "depth" the view is sliced through - the
 * xelector's own current row, so the slice always shows exactly what
 * you're standing in front of), read that same row's glyph from EVERY
 * real z-layer file (board_manifest.txt's z_base/z_count, the same
 * manifest load_board() already reads for a single z) - g_board[z][col]
 * instead of g_board[row][col]. No manifest = no side view possible
 * (the flat, non-extruded board.txt case has no per-height data to
 * slice at all) - caller falls back to render_mode 0's normal draw. */
static void load_side_board(int fixed_row, int *zcount_out) {
    *zcount_out = 0;
    char man[PATH_BUF], zbase[256] = "";
    snprintf(man, sizeof(man), "%s/pieces/system/board_manifest.txt", focused_root);
    read_kv_str(man, "z_base", zbase, sizeof(zbase));
    int zcount = read_kv_int(man, "z_count", 0);
    if (!zbase[0] || zcount <= 0) return;
    if (zcount > MAX_DIM) zcount = MAX_DIM;
    if (fixed_row < 0) fixed_row = 0;
    for (int z = 0; z < zcount; z++) {
        char path[PATH_BUF];
        snprintf(path, sizeof(path), "%s/%s%d.txt", focused_root, zbase, z);
        FILE *f = host_fopen(path, "r");
        if (!f) continue;
        char line[MAX_LINE];
        int row = 0;
        while (row <= fixed_row && fgets(line, sizeof(line), f)) {
            if (row == fixed_row) {
                line[strcspn(line, "\r\n")] = '\0';
                int len = (int)strlen(line);
                if (len > MAX_DIM) len = MAX_DIM;
                memcpy(g_board[z], line, (size_t)len);
                if (len > g_bw) g_bw = len;
            }
            row++;
        }
        fclose(f);
    }
    g_bh = zcount;   /* reused by main()'s existing "by < g_bh" bounds check below */
    *zcount_out = zcount;
}

static void write_atomic(const char *path, const void *data, size_t len) {
    char tmp[PATH_BUF];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *f = host_fopen(tmp, "wb");
    if (!f) return;
    if (fwrite(data, 1, len, f) != len) { fclose(f); remove(tmp); return; }
    fclose(f);
    remove(path);
    if (rename(tmp, path) != 0) {
        FILE *o = host_fopen(path, "wb");
        if (o) { fwrite(data, 1, len, o); fclose(o); }
        remove(tmp);
    }
}

int main(void) {
    resolve_root();
    load_house_root();

    char st[PATH_BUF];
    snprintf(st, sizeof(st), "%s/pieces/system/bv_state.txt", project_root);
    char fpr[PATH_BUF] = "";
    read_kv_str(st, "focused_project_root", fpr, sizeof(fpr));
    resolve_focused(fpr);
    int sel_x    = read_kv_int(st, "selector_x", -1);
    int sel_y    = read_kv_int(st, "selector_y", -1);
    int cur_z    = read_kv_int(st, "current_z", 0);
    /* render_mode==2: real side-scroll/Mario slice (see load_side_
     * board()'s own header comment) - same flat-render philosophy as
     * render_mode==0 (this whole file), sliced from the side instead
     * of top-down. Any other value (0, or an unrecognized future one)
     * falls back to the normal top-down draw this file always did. */
    int render_mode = read_kv_int(st, "render_mode", 0);
    int side_mode = (render_mode == 2);
    int fixed_row = (sel_y >= 0) ? sel_y : 0;   /* the "depth" this slice is through */

    load_legend();
    load_entities();
    g_any_z = side_mode;
    load_actors(cur_z);          /* hero_01 + world_01 animals - all heights in side_mode, this z-slice otherwise */
    int side_zcount = 0;
    char bound_pdl[PATH_BUF];
    int desk_page = house_root[0] && page_bound_pdl(house_root, bound_pdl, sizeof(bound_pdl)) > 0;
    if (desk_page) {
        /* The desk page is the map. The piececraft chunk (grass, rock)
         * stays out. A 16x16 floor keeps pals off the void without
         * filling the view. source=board brings the chunk back. */
        memset(g_board, 0, sizeof(g_board));
        g_bw = 16;
        g_bh = 16;
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < 16; x++)
                g_board[y][x] = '.';
        side_zcount = 1;
    } else if (side_mode) {
        load_side_board(fixed_row, &side_zcount);
    } else {
        load_board(cur_z);
    }

    /* Empty / not-yet-generated board -> still show a grid so `0` isn't blank. */
    int bw = g_bw > 0 ? g_bw : 20;
    int bh = side_mode ? (side_zcount > 0 ? side_zcount : 15) : (g_bh > 0 ? g_bh : 15);

    /* cell size: the house desktop grid (desk_grid.pdl GRID|cell_px|N,
     * default 80) - a board cell is the same on-screen size as a desk
     * cell. */
    char grid_pdl[PATH_BUF];
    snprintf(grid_pdl, sizeof(grid_pdl), "%s/#.desktop/desk_grid.pdl", house_root);
    int cell = 80;
    { FILE *gf = host_fopen(grid_pdl, "r");
      if (gf) { char l[MAX_LINE];
        while (fgets(l, sizeof(l), gf)) {
            char *p = strstr(l, "cell_px");            /* "GRID | cell_px | N" */
            if (!p) continue;
            p = strchr(p, '|'); if (!p) continue;
            int v = atoi(p + 1); if (v >= 8 && v <= 256) cell = v;
            break;
        }
        fclose(gf);
      } }

    /* Viewport size = the board window's live canvas pixel size, which
     * the khtpm renderer writes to #.desktop/pchq_board_view.txt every
     * layout (class="user-resizable"). Resize the window -> the map view
     * resizes with it. Falls back to 640x480 (== the 3D overlay) if the
     * file isn't there yet. */
    int W = 640, H = 480;
    { char vsz[PATH_BUF]; snprintf(vsz, sizeof(vsz), "%s/#.desktop/pchq_board_view.txt", house_root);
      FILE *vf = host_fopen(vsz, "r");
      if (vf) { int a = 0, b = 0; if (fscanf(vf, "%d %d", &a, &b) == 2) {
                    if (a >= 160 && a <= 3840) W = a;
                    if (b >= 120 && b <= 2160) H = b;
                } fclose(vf); } }
    int cols = W / cell, rows = H / cell;
    if (cols < 1) cols = 1;
    if (rows < 1) rows = 1;

    /* viewport origin cell (top-left), centred on the xelector, clamped.
     * side_mode: the vertical axis is height, not board row, and the
     * xelector has no height of its own - center on the mid-height
     * (same as this file's own long-standing "no selector position"
     * fallback, reused deliberately rather than inventing a new rule). */
    int ox = (sel_x >= 0 ? sel_x : bw / 2) - cols / 2;
    int oy = (side_mode || sel_y < 0) ? (bh / 2 - rows / 2) : (sel_y - rows / 2);
    if (bw > cols) { if (ox < 0) ox = 0; if (ox > bw - cols) ox = bw - cols; } else ox = -(cols - bw) / 2;
    if (bh > rows) { if (oy < 0) oy = 0; if (oy > bh - rows) oy = bh - rows; } else oy = -(rows - bh) / 2;
    {
        char vp[PATH_BUF];
        snprintf(vp, sizeof(vp), "%s/pieces/display/view_map.txt", project_root);
        FILE *vf = host_fopen(vp, "w");
        if (vf) { fprintf(vf, "ox=%d\noy=%d\ncell=%d\nW=%d\nH=%d\n", ox, oy, cell, W, H); fclose(vf); }
    }

    unsigned char *px = calloc((size_t)W * H, 4);
    if (!px) return 1;

    unsigned char air_r = 24, air_g = 24, air_b = 28;   /* desk-ish dark, NOT sky blue */
    unsigned char gl_r = 60, gl_g = 90, gl_b = 70;      /* faint "matrix" grid line */

    /* fill: whole frame starts as air (covers the sub-cell remainder on
     * the right/bottom edges too) */
    for (size_t i = 0; i < (size_t)W * H; i++) {
        px[i*4+0] = air_r; px[i*4+1] = air_g; px[i*4+2] = air_b; px[i*4+3] = 255;
    }

    #define VP_PXR(SX,SY) (px + ((size_t)(SY) * W + (SX)) * 4)
    /* view_2d_style (` toggle): "ascii" -> DF/CDDA-style: one coloured
     * CJK glyph per cell (bg dimmed to a terrain hint); "emoji" -> the
     * emoji sprite over the terrain colour; "tiles" (default) -> the
     * flat colour grid (P1). Real palette tilesets are P2b. */
    char style[16] = ""; read_kv_str(st, "view_2d_style", style, sizeof(style));
    int want_emoji = (strcmp(style, "emoji") == 0);
    int want_ascii = (strcmp(style, "ascii") == 0);

    /* --- ground tiles --- */
    for (int scy = 0; scy < rows; scy++) {
        for (int scx = 0; scx < cols; scx++) {
            int bx = ox + scx;
            /* side_mode: 'by' here means a Z-LAYER index, not a board
             * row - inverted so higher z (taller) draws nearer the TOP
             * of the screen and z=0 (ground) draws nearer the BOTTOM,
             * the real visual convention a side view needs (g_board[]
             * itself was already filled indexed by z, not row, in
             * load_side_board() above). */
            int by = side_mode ? (side_zcount - 1 - (oy + scy)) : (oy + scy);
            if (bx < 0 || by < 0 || bx >= bw || by >= bh) continue;
            unsigned char r = air_r, g = air_g, b = air_b;
            const unsigned char *e16 = NULL;
            unsigned int cjk_cp = 0;
            unsigned char gr = 0, gg = 0, gb = 0;   /* ascii glyph colour */
            if (by < g_bh && bx < g_bw) {
                char gch = g_board[by][bx];
                if (gch && gch != '_' && gch != ' ') {
                    int li = legend_idx(gch);
                    if (li >= 0) { r = g_leg[li].r; g = g_leg[li].g; b = g_leg[li].b;
                                   if (want_emoji) e16 = load_emoji16(g_leg_hex[li]);
                                   if (want_ascii && g_leg_cjk[li][0]) cjk_cp = bv_cjk_utf8_first(g_leg_cjk[li]); }
                    else { r = 90; g = 90; b = 96; }
                }
            }
            if (cjk_cp) {
                /* bright glyph in the terrain hue, cell bg dimmed to a hint */
                gr = (unsigned char)(r + (255 - r) * 3 / 5);
                gg = (unsigned char)(g + (255 - g) * 3 / 5);
                gb = (unsigned char)(b + (255 - b) * 3 / 5);
                r /= 4; g /= 4; b /= 4;
            }
            int dx = scx*cell, dy = scy*cell;
            for (int yy = 0; yy < cell; yy++)
                for (int xx = 0; xx < cell; xx++) {
                    unsigned char *p = VP_PXR(dx + xx, dy + yy);
                    p[0]=r; p[1]=g; p[2]=b; p[3]=255;
                }
            if (e16) blit_emoji(px, W, dx, dy, cell, e16);
            if (cjk_cp) blit_cjk(px, W, dx, dy, cell, cjk_cp, gr, gg, gb);
        }
    }

    /* --- entities / hero / animals: emoji sprite if we have one, else
     * a solid colour square (inner 60%) --- */
    for (int i = 0; i < g_nent; i++) {
        /* side_mode: only entities standing in THIS depth slice (same
         * row the terrain slice is through) are visible at all - real
         * depth culling, not a simplification, matches what a real
         * side-scroll camera would actually show. Screen height comes
         * from the entity's own real z (g_any_z above loaded every
         * height, not just one slice), inverted the same way ground
         * tiles are just above. */
        int scx, scy;
        if (side_mode) {
            if (g_ent[i].y != fixed_row) continue;
            scx = g_ent[i].x - ox;
            scy = (side_zcount - 1 - g_ent[i].z) - oy;
        } else {
            scx = g_ent[i].x - ox; scy = g_ent[i].y - oy;
        }
        if (scx < 0 || scy < 0 || scx >= cols || scy >= rows) continue;
        if (want_ascii && g_ent[i].cjk[0]) {
            unsigned int cp = bv_cjk_utf8_first(g_ent[i].cjk);
            if (cp) { blit_cjk(px, W, scx*cell, scy*cell, cell, cp, 245, 240, 210); continue; }
        }
        const unsigned char *e16 = g_ent[i].hex[0] ? load_emoji16(g_ent[i].hex) : NULL;
        if (e16) {
            blit_emoji(px, W, scx*cell, scy*cell, cell, e16);
        } else if (g_ent[i].spr[0] && blit_sprite_csv(px, W, scx*cell, scy*cell, cell, g_ent[i].spr)) {
            /* pal sprite.csv, the picture the desk already shows */
        } else {
            int m = cell / 5;
            for (int yy = m; yy < cell - m; yy++)
                for (int xx = m; xx < cell - m; xx++) {
                    unsigned char *p = VP_PXR(scx*cell + xx, scy*cell + yy);
                    p[0]=g_ent[i].r; p[1]=g_ent[i].g; p[2]=g_ent[i].b; p[3]=255;
                }
        }
    }

    /* --- the manual matrix grid (viewport-relative cell boundaries) --- */
    for (int c = 0; c <= cols; c++) {
        int x = c * cell; if (x >= W) x = W - 1;
        for (int y = 0; y < H; y++) { unsigned char *p = VP_PXR(x, y); p[0]=gl_r; p[1]=gl_g; p[2]=gl_b; p[3]=255; }
    }
    for (int c = 0; c <= rows; c++) {
        int y = c * cell; if (y >= H) y = H - 1;
        for (int x = 0; x < W; x++) { unsigned char *p = VP_PXR(x, y); p[0]=gl_r; p[1]=gl_g; p[2]=gl_b; p[3]=255; }
    }

    /* --- xelector: 2px inset border, bright accent ---
     * side_mode: honestly skipped for v1 - the xelector has no real
     * height of its own (only x/row), so there's no correct screen
     * position to draw its highlight at on this view's vertical
     * (height) axis. Ground/entities above already draw fully. */
    if (!side_mode) {
        int scx = sel_x - ox, scy = sel_y - oy;
        if (sel_x >= 0 && sel_y >= 0 && scx >= 0 && scy >= 0 && scx < cols && scy < rows) {
            unsigned char xr = 255, xg = 204, xb = 0;
            int x0 = scx * cell, y0 = scy * cell;
            for (int t = 1; t <= 2; t++) {
                for (int x = x0 + t; x < x0 + cell - t; x++)
                    for (int yy = 0; yy < 2; yy++) {
                        unsigned char *a = VP_PXR(x, y0 + t + yy);
                        unsigned char *cc = VP_PXR(x, y0 + cell - 1 - t - yy);
                        a[0]=xr; a[1]=xg; a[2]=xb; a[3]=255; cc[0]=xr; cc[1]=xg; cc[2]=xb; cc[3]=255;
                    }
                for (int y = y0 + t; y < y0 + cell - t; y++)
                    for (int xx = 0; xx < 2; xx++) {
                        unsigned char *a = VP_PXR(x0 + t + xx, y);
                        unsigned char *cc = VP_PXR(x0 + cell - 1 - t - xx, y);
                        a[0]=xr; a[1]=xg; a[2]=xb; a[3]=255; cc[0]=xr; cc[1]=xg; cc[2]=xb; cc[3]=255;
                    }
            }
        }
    }

    /* Yellow wire on the xelector's cell. Possessing an entity
     * puts the xelector on that entity's cell. */
    {
        int hx = -1, hy = -1, hz = 0;
        char glyph[64] = "";
        if (!page_row_meta(house_root, "xelector_01", &hx, &hy, glyph, sizeof(glyph), &hz)) {
            char hpath[PATH_BUF], b[32];
            snprintf(hpath, sizeof(hpath), "%s/pieces/xelector_01/state.txt", focused_root);
            read_kv_str(hpath, "pos_x", b, sizeof(b)); if (b[0]) hx = atoi(b);
            read_kv_str(hpath, "pos_y", b, sizeof(b)); if (b[0]) hy = atoi(b);
            read_kv_str(hpath, "pos_z", b, sizeof(b)); if (b[0]) hz = atoi(b);
            read_kv_str(hpath, "possessed_id", glyph, sizeof(glyph));
        }
        if (glyph[0] && strcmp(glyph, ".") != 0) {
            int pxs[4], pys[4];
            if (page_named_cells(house_root, glyph, pxs, pys, 4) > 0) {
                hx = pxs[0]; hy = pys[0];
            }
        }
        if (hx >= 0 && hy >= 0) {
            int scx, scy;
            if (side_mode) {
                scx = hx - ox;
                scy = (side_zcount - 1 - hz) - oy;
            } else {
                scx = hx - ox;
                scy = hy - oy;
            }
            if (scx >= 0 && scy >= 0 && scx < cols && scy < rows) {
                int x0 = scx * cell, y0 = scy * cell;
                for (int t = 0; t < 3; t++) {
                    for (int x = x0; x < x0 + cell && x < W; x++) {
                        unsigned char *a = VP_PXR(x, y0 + t);
                        unsigned char *b = VP_PXR(x, y0 + cell - 1 - t);
                        a[0]=255; a[1]=220; a[2]=40; a[3]=255;
                        b[0]=255; b[1]=220; b[2]=40; b[3]=255;
                    }
                    for (int y = y0; y < y0 + cell && y < H; y++) {
                        unsigned char *a = VP_PXR(x0 + t, y);
                        unsigned char *b = VP_PXR(x0 + cell - 1 - t, y);
                        a[0]=255; a[1]=220; a[2]=40; a[3]=255;
                        b[0]=255; b[1]=220; b[2]=40; b[3]=255;
                    }
                }
            }
        }
        /* Desk diamond: the same '#' file, one tile per '#', on the hero. */
        if (hx >= 0 && hy >= 0) {
            /* Range finder: open only while move_range_matrix.txt exists
             * (see bv_move_range.c). Same origin + same test the 3D
             * render and the Enter-confirm use. */
            BvRange rng;
            if (bvr_load(focused_root, &rng)) {
                int nr = rng.nr, nc = rng.nc;
                int cx0 = nc / 2, cy0 = nr / 2;
                { int ex, ey, ez; if (bvr_origin(focused_root, &ex, &ey, &ez)) { hx = ex; hy = ey; hz = ez; } }   /* origin = the entity being moved, same as 3D */
                for (int row = 0; row < nr; row++) {
                    for (int col = 0; col < nc && rng.rows[row][col]; col++) {
                        if (rng.rows[row][col] != '#') continue;
                        int scx = (hx + col - cx0) - ox;
                        int scy = side_mode ? ((side_zcount - 1 - hz) - oy) : ((hy + row - cy0) - oy);
                        if (scx < 0 || scy < 0 || scx >= cols || scy >= rows) continue;
                        int x0 = scx * cell, y0 = scy * cell;
                        for (int t = 0; t < 2; t++) {
                            for (int x = x0; x < x0 + cell && x < W; x++) {
                                unsigned char *a = VP_PXR(x, y0 + t);
                                unsigned char *b = VP_PXR(x, y0 + cell - 1 - t);
                                a[0]=255; a[1]=220; a[2]=40; a[3]=255;
                                b[0]=255; b[1]=220; b[2]=40; b[3]=255;
                            }
                            for (int y = y0; y < y0 + cell && y < H; y++) {
                                unsigned char *a = VP_PXR(x0 + t, y);
                                unsigned char *b = VP_PXR(x0 + cell - 1 - t, y);
                                a[0]=255; a[1]=220; a[2]=40; a[3]=255;
                                b[0]=255; b[1]=220; b[2]=40; b[3]=255;
                            }
                        }
                    }
                }
            }
        }
        char cpath[PATH_BUF], seenp[PATH_BUF];
        snprintf(cpath, sizeof(cpath), "%s/#.desktop/pchq_canvas_click.txt", house_root);
        snprintf(seenp, sizeof(seenp), "%s/pieces/display/click_2d.seen", project_root);
        FILE *cf = host_fopen(cpath, "r");
        int cx = 0, cy = 0, cw = 0, ch = 0;
        if (cf && fscanf(cf, "%d %d %d %d", &cx, &cy, &cw, &ch) == 4 && cw > 0 && ch > 0) {
            char stamp[64], prev[64] = "";
            snprintf(stamp, sizeof(stamp), "%d %d %d %d", cx, cy, cw, ch);
            FILE *sf = host_fopen(seenp, "r");
            if (sf) { if (fgets(prev, sizeof(prev), sf)) prev[strcspn(prev, "\r\n")] = 0; fclose(sf); }
            if (strcmp(prev, stamp) != 0) {
                int px = cx * W / cw, py = cy * H / ch;
                int scx = px / cell, scy = py / cell;
                if (scx >= 0 && scy >= 0 && scx < cols && scy < rows) {
                    int bx = ox + scx;
                    int by = side_mode ? hy : (oy + scy);
                    int bz = side_mode ? (side_zcount - 1 - (oy + scy)) : cur_z;
                    char pp[PATH_BUF];
                    snprintf(pp, sizeof(pp), "%s/pieces/display/placer.txt", project_root);
                    FILE *pf = NULL;
                    if (!bvr_click(focused_root, project_root, bx, by, 0, 0))   /* Move range open: select/place */
                        pf = host_fopen(pp, "w");
                    if (pf) { fprintf(pf, "armed=1\nx=%d\ny=%d\nz=%d\n", bx, by, bz); fclose(pf); }
                    time_t now = time(NULL);
                    struct tm tmv; localtime_r(&now, &tmv);
                    snprintf(pp, sizeof(pp), "%s/pieces/display/click_hud.txt", focused_root);
                    pf = host_fopen(pp, "w");
                    if (pf) {
                        fprintf(pf, "pos=%d,%d,%d\ntime=%02d:%02d:%02d\n",
                                bx, by, bz, tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
                        fclose(pf);
                    }
                }
                sf = host_fopen(seenp, "w");
                if (sf) { fputs(stamp, sf); fclose(sf); }
            }
        }
        if (cf) fclose(cf);
        /* Green selector tile, one cell, same file the 3D view reads. */
        {
            char pp[PATH_BUF];
            snprintf(pp, sizeof(pp), "%s/pieces/display/placer.txt", project_root);
            FILE *pf = host_fopen(pp, "r");
            int armed = 0, sx = 0, sy = 0, sz = 0;
            if (pf) {
                char line[64];
                while (fgets(line, sizeof(line), pf)) {
                    if (strncmp(line, "armed=", 6) == 0) armed = atoi(line + 6);
                    else if (strncmp(line, "x=", 2) == 0) sx = atoi(line + 2);
                    else if (strncmp(line, "y=", 2) == 0) sy = atoi(line + 2);
                    else if (strncmp(line, "z=", 2) == 0) sz = atoi(line + 2);
                }
                fclose(pf);
            }
            if (armed) {
                int scx = sx - ox;
                int scy = side_mode ? ((side_zcount - 1 - sz) - oy) : (sy - oy);
                if (scx >= 0 && scy >= 0 && scx < cols && scy < rows) {
                    int bx0 = scx * cell, by0 = scy * cell;
                    for (int t = 0; t < 3; t++) {
                        for (int x = bx0; x < bx0 + cell && x < W; x++) {
                            unsigned char *a = VP_PXR(x, by0 + t);
                            unsigned char *b = VP_PXR(x, by0 + cell - 1 - t);
                            a[0]=40; a[1]=255; a[2]=80; a[3]=255;
                            b[0]=40; b[1]=255; b[2]=80; b[3]=255;
                        }
                        for (int y = by0; y < by0 + cell && y < H; y++) {
                            unsigned char *a = VP_PXR(bx0 + t, y);
                            unsigned char *b = VP_PXR(bx0 + cell - 1 - t, y);
                            a[0]=40; a[1]=255; a[2]=80; a[3]=255;
                            b[0]=40; b[1]=255; b[2]=80; b[3]=255;
                        }
                    }
                }
            }
        }
    }
    #undef VP_PXR

    char out[PATH_BUF], rec[PATH_BUF];
    snprintf(out, sizeof(out), "%s/pieces/display/rgb_frame_2d.raw", project_root);
    snprintf(rec, sizeof(rec), "%s/pieces/display/rgb_frame_2d.receipt.txt", project_root);
    write_atomic(out, px, (size_t)W * H * 4);
    { char rb[128]; int n = snprintf(rb, sizeof(rb), "frame_w=%d\nframe_h=%d\n", W, H);
      if (n > 0) write_atomic(rec, rb, (size_t)n); }
    free(px);
    return 0;
}
