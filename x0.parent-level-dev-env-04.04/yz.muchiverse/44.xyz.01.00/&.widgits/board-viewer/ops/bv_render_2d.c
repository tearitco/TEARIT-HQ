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
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <dirent.h>
#include <setjmp.h>
#include <png.h>

#include "bv_cjk_glyph.h"   /* view_2d_style=ascii: coloured CJK glyph per cell */
#include "bv_move_range.c"   /* shared Move range finder file helpers */
#include "../../_shared-lib/khtpm_locations.c"   /* loc_resolve_saved(): saved paths resolved against the live house root */
#include "../../_shared-lib/khtpm_pin_pick.c"   /* pin_path_pick(): a board obeys its OWN open_book_page.txt before the house-wide one (S3b-1) */

#define MAX_LINE     1024
#define PATH_BUF     4096
#define MAX_DIM      256          /* board cells per side, hard ceiling */
#define MAX_FRAME_PX 2400         /* per side; CELL shrinks to fit P1's whole-board view */
#define MAX_LEGEND   64
#define MAX_ENT      256

static char project_root[PATH_BUF] = ".";

/* pc-hq sessions (PCHQ-SESSIONS-DESIGN.md): prefer the file the board window wrote INSIDE this engine session (pieces/display/<name>) over the house-wide #.desktop one, so two boards do not read
 * each other's canvas size or click. A window launched before this change only writes the house-wide file: the path is then left alone (fallback). */
static void bv_prefer_session_file(char *path, size_t cap, const char *name) {
    char sp[4400]; snprintf(sp, sizeof(sp), "%s/pieces/display/%s", project_root, name);
    struct stat sst; if (stat(sp, &sst) == 0) snprintf(path, cap, "%s", sp);
}
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
    loc_house_root(project_root, house_root, sizeof(house_root));   /* shared resolver (_shared-lib/khtpm_locations.c): locations.pdl / house_root.txt / walk up */
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
static int pin_readable(const char *p) {
    FILE *f = host_fopen(p, "r");
    if (!f) return 0;
    fclose(f);
    return 1;
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
    pin_path_pick(house, focused_root, pin_readable, ob, sizeof(ob));      /* the board's own pin first, then the house-wide one (S3b-1) */
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
    /* the saved pdl= may be house-relative or a stale absolute path (checkout moved): resolve against the LIVE house
     * root - see _shared-lib/khtpm_locations.c */
    char resolved[PATH_BUF];
    loc_resolve_saved(house, stored, resolved, sizeof(resolved));
    FILE *t = host_fopen(resolved, "r");
    if (!t) return 0;
    fclose(t);
    snprintf(out, n, "%s", resolved);
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
static int desk_dir(char *out, size_t osz);
static void load_board(int current_z) {
    {   /* a book with its own ASCII legend (maps/<book>/<desk>/ascii_legend.txt) draws its own map.txt in the ` view */
        char dd[PATH_BUF], lg[PATH_BUF], mt[PATH_BUF];
        if (desk_dir(dd, sizeof(dd))) {
            snprintf(lg, sizeof(lg), "%s/ascii_legend.txt", dd); snprintf(mt, sizeof(mt), "%s/map.txt", dd);
            FILE *lf = host_fopen(lg, "r");
            if (lf) { fclose(lf);
                FILE *mf = host_fopen(mt, "r");
                if (mf) { char line[MAX_LINE]; int row = 0;
                    while (row < MAX_DIM && fgets(line, sizeof(line), mf)) { line[strcspn(line, "\r\n")] = 0; int len = (int)strlen(line); if (!len) continue; if (len > MAX_DIM) len = MAX_DIM; memcpy(g_board[row], line, len); if (len > g_bw) g_bw = len; row++; }
                    if (row > g_bh) g_bh = row; fclose(mf); return; } }
        }
    }
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

/* Decode one RGBA PNG. Returns 0 on any failure and leaves *out NULL.
 * Used only for a desk map.png. Desks with no file stay on the glyph
 * colour path. */
static int load_rgba_png(const char *path, unsigned char **out, int *w, int *h) {
    *out = NULL; *w = 0; *h = 0;
    FILE *fp = host_fopen(path, "rb");
    if (!fp) return 0;
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) { fclose(fp); return 0; }
    png_infop info = png_create_info_struct(png);
    if (!info) { png_destroy_read_struct(&png, NULL, NULL); fclose(fp); return 0; }
    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return 0;
    }
    png_init_io(png, fp);
    png_read_info(png, info);
    int width = (int)png_get_image_width(png, info);
    int height = (int)png_get_image_height(png, info);
    if (width < 1 || height < 1 || width > 8192 || height > 8192) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return 0;
    }
    png_byte color = png_get_color_type(png, info);
    png_byte depth = png_get_bit_depth(png, info);
    if (depth == 16) png_set_strip_16(png);
    if (color == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (color == PNG_COLOR_TYPE_GRAY && depth < 8) png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
    if (color == PNG_COLOR_TYPE_RGB || color == PNG_COLOR_TYPE_GRAY || color == PNG_COLOR_TYPE_PALETTE)
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    if (color == PNG_COLOR_TYPE_GRAY || color == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);
    png_read_update_info(png, info);
    if (png_get_rowbytes(png, info) != (size_t)width * 4) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return 0;
    }
    unsigned char *buf = malloc((size_t)width * (size_t)height * 4);
    png_bytep *rows = buf ? malloc(sizeof(png_bytep) * (size_t)height) : NULL;
    if (!buf || !rows) {
        free(buf); free(rows);
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return 0;
    }
    for (int y = 0; y < height; y++) rows[y] = buf + (size_t)y * (size_t)width * 4;
    png_read_image(png, rows);
    free(rows);
    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);
    *out = buf; *w = width; *h = height;
    return 1;
}

/* One charset frame, nearest-neighbour, skipping clear pixels. */
static void blit_frame(unsigned char *dst, int W, int H,
                       int dx, int dy, int dw, int dh,
                       const unsigned char *src, int sw, int sh,
                       int sx, int sy, int fw, int fh) {
    if (fw < 1 || fh < 1 || dw < 1 || dh < 1) return;
    for (int y = 0; y < dh; y++) {
        int yy = dy + y;
        if (yy < 0 || yy >= H) continue;
        int syy = sy + (y * fh) / dh;
        if (syy < 0 || syy >= sh) continue;
        for (int x = 0; x < dw; x++) {
            int xx = dx + x;
            if (xx < 0 || xx >= W) continue;
            int sxx = sx + (x * fw) / dw;
            if (sxx < 0 || sxx >= sw) continue;
            const unsigned char *s = src + ((size_t)syy * (size_t)sw + (size_t)sxx) * 4;
            if (s[3] < 16) continue;
            unsigned char *p = dst + ((size_t)yy * (size_t)W + (size_t)xx) * 4;
            p[0] = s[0]; p[1] = s[1]; p[2] = s[2]; p[3] = 255;
        }
    }
}

/* events.txt beside map.png. A line is one visible map event:
 *   x y r g b charset index direction pattern
 * Feet sit on the bottom of the cell. A '!' sheet is one cell tall.
 * Any other sheet is two cells tall. This binary is a new process
 * each frame, so the sheet cache lives only for this call. */
static void draw_map_events(unsigned char *px, int W, int H,
                            const char *map_png, int map_tile,
                            int map_px0, int map_py0, int draw) {
    if (!map_png || map_tile < 1 || draw < 1) return;
    char path[PATH_BUF];
    size_t n = strlen(map_png);
    if (n < 8 || n >= sizeof(path)) return;
    snprintf(path, sizeof(path), "%.*s/events.txt", (int)(n - 8), map_png);
    FILE *f = host_fopen(path, "r");
    if (!f) return;
    typedef struct { char name[64]; unsigned char *px; int w, h; } Sheet;
    Sheet sheets[32];
    int ns = 0;
    memset(sheets, 0, sizeof(sheets));
    char line[256];
    int drawn = 0;
    while (drawn < 256 && fgets(line, sizeof(line), f)) {
        int ex, ey, r, g, b, index, dir, pat;
        char name[64];
        if (sscanf(line, "%d %d %d %d %d %63s %d %d %d",
                   &ex, &ey, &r, &g, &b, name, &index, &dir, &pat) != 9)
            continue;
        (void)r; (void)g; (void)b;
        int si = -1;
        for (int i = 0; i < ns; i++) if (strcmp(sheets[i].name, name) == 0) si = i;
        if (si < 0 && ns < 32) {
            char dirn[PATH_BUF], sheet[PATH_BUF];
            snprintf(dirn, sizeof(dirn), "%s", map_png);
            /* Sheets live in the repo #.NNEST_ASSETS, eleven
             * directories above a desk map.png. Eight stops inside
             * 44.xyz.01.00, which has no assets dir. */
            for (int up = 0; up < 16 && si < 0; up++) {
                char *sl = strrchr(dirn, '/');
                if (!sl) break;
                *sl = 0;
                snprintf(sheet, sizeof(sheet),
                         "%s/#.NNEST_ASSETS/tsots-characters/%s.png", dirn, name);
                if (load_rgba_png(sheet, &sheets[ns].px, &sheets[ns].w, &sheets[ns].h)) {
                    snprintf(sheets[ns].name, sizeof(sheets[ns].name), "%s", name);
                    si = ns++;
                }
            }
        }
        if (si < 0) continue;
        int sw = sheets[si].w, sh = sheets[si].h;
        int row = dir == 4 ? 1 : dir == 6 ? 2 : dir == 8 ? 3 : 0;
        if (pat < 0) pat = 0;
        if (pat > 2) pat = 2;
        int pw, ph, sx, sy;
        if (name[0] == '$') {
            pw = sw / 3; ph = sh / 4;
            sx = pat * pw; sy = row * ph;
        } else {
            pw = sw / 12; ph = sh / 8;
            if (index < 0) index = 0;
            if (index > 7) index = 7;
            sx = ((index % 4) * 3 + pat) * pw;
            sy = ((index / 4) * 4 + row) * ph;
        }
        if (pw < 1 || ph < 1) continue;
        int dh = (name[0] == '!') ? draw : draw * 2;
        int dw = dh * pw / ph;
        if (dw < 1) dw = 1;
        int left = (ex * map_tile - map_px0) * draw / map_tile;
        int bottom = ((ey + 1) * map_tile - map_py0) * draw / map_tile;
        blit_frame(px, W, H, left + (draw - dw) / 2, bottom - dh, dw, dh,
                   sheets[si].px, sw, sh, sx, sy, pw, ph);
        drawn++;
    }
    fclose(f);
    for (int i = 0; i < ns; i++) free(sheets[i].px);
}

/* parallax.pdl sits beside map.png. show=0 means this desk has no sky.
 * loop_x / loop_y tile with the map scroll. sx/sy are stored for a
 * later scroll tick and are not applied here. */
/* The sky of a painted desk as raw RGBA8: parallax.rgba next to map.png, `PARALLAX | width | N` and `PARALLAX | height | N` in parallax.pdl - the SAME files the 3D engine reads (bv_render_3d.c), so 2D shows the same space parallax
 * (owner 2026-10-27: "2d isn't using space parallax?": it only looked for a parallax.png, which the games never write). */
static void load_parallax_rgba(const char *map_png, unsigned char **out, int *w, int *h) {
    *out = NULL; *w = *h = 0; size_t n = strlen(map_png); if (n < 8 || n > PATH_BUF - 32) return; char dir[PATH_BUF], pdl[PATH_BUF], raw[PATH_BUF];
    snprintf(dir, sizeof(dir), "%.*s", (int)(n - 8), map_png); snprintf(pdl, sizeof(pdl), "%s/parallax.pdl", dir); snprintf(raw, sizeof(raw), "%s/parallax.rgba", dir);
    FILE *f = host_fopen(pdl, "r"); if (!f) return; char line[256]; int pw = 0, ph = 0;
    while (fgets(line, sizeof(line), f)) { char *bar = strrchr(line, '|'); if (!bar || !strstr(line, "PARALLAX")) continue; if (strstr(line, "width")) pw = atoi(bar + 1); else if (strstr(line, "height")) ph = atoi(bar + 1); }
    fclose(f); if (pw < 1 || ph < 1 || pw > 4096 || ph > 4096) return;
    unsigned char *buf = (unsigned char *)malloc((size_t)pw * ph * 4); if (!buf) return; FILE *rf = host_fopen(raw, "rb"); if (!rf) { free(buf); return; }
    size_t got = fread(buf, 1, (size_t)pw * ph * 4, rf); fclose(rf); if (got != (size_t)pw * ph * 4) { free(buf); return; } *out = buf; *w = pw; *h = ph; }
static int desk_parallax(const char *map_png, int *loop_x, int *loop_y) {
    *loop_x = 0;
    *loop_y = 0;
    char pdl[PATH_BUF];
    size_t n = strlen(map_png);
    if (n < 8 || n >= sizeof(pdl)) return 0;
    snprintf(pdl, sizeof(pdl), "%.*s/parallax.pdl", (int)(n - 8), map_png);
    FILE *f = host_fopen(pdl, "r");
    if (!f) return 0;
    int show = 0;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *p = strstr(line, "show");
        if (p && strstr(line, "PARALLAX")) {
            char *bar = strrchr(p, '|');
            if (bar) show = atoi(bar + 1);
        }
        if ((p = strstr(line, "loop_x"))) {
            char *bar = strrchr(p, '|');
            if (bar) *loop_x = atoi(bar + 1);
        }
        if ((p = strstr(line, "loop_y"))) {
            char *bar = strrchr(p, '|');
            if (bar) *loop_y = atoi(bar + 1);
        }
    }
    fclose(f);
    return show == 1;
}

/* The current desk folder (maps/<book>/<desk>), or "" when the world names none. */
static int desk_dir(char *out, size_t osz) {
    out[0] = '\0';
    const char *root = focused_root[0] ? focused_root : project_root;
    char wst[PATH_BUF], desk[64] = "", map_id[64] = "";
    snprintf(wst, sizeof(wst), "%s/pieces/world_01/state.txt", root);
    read_kv_str(wst, "desk_id", desk, sizeof(desk));
    read_kv_str(wst, "map_id", map_id, sizeof(map_id));
    if (!desk[0] || !map_id[0] || strchr(desk, '/') || strchr(map_id, '/')) return 0;
    snprintf(out, osz, "%s/pieces/system/maps/%s/%s", root, map_id, desk);
    return 1;
}
/* ASCII view (` key) of a book that has its own glyphs: <desk>/ascii_legend.txt, same rows as terrain_legend.txt
 * (glyph|height|r|g|b|asset|name|char). Its rows come first so they win over the host legend. The char column may be
 * any single character (an ASCII letter or a CJK one). */
static int g_own_ascii = 0;   /* this desk has its own ASCII legend: its events are the entities, the host pieces (hero, trees) are not drawn */
static void load_desk_ascii_legend(void) {
    char dd[PATH_BUF], path[PATH_BUF];
    if (!desk_dir(dd, sizeof(dd))) return;
    snprintf(path, sizeof(path), "%s/ascii_legend.txt", dd);
    FILE *f = host_fopen(path, "r");
    if (!f) return;
    g_own_ascii = 1;
    char line[MAX_LINE];
    while (g_nleg < MAX_LEGEND && fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (!line[0] || (line[0] == '#' && line[1] != '|')) continue;
        char *sv = NULL, *g = strtok_r(line, "|", &sv);
        strtok_r(NULL, "|", &sv);
        char *rt = strtok_r(NULL, "|", &sv), *gt = strtok_r(NULL, "|", &sv), *bt = strtok_r(NULL, "|", &sv);
        strtok_r(NULL, "|", &sv); strtok_r(NULL, "|", &sv);
        char *ct = strtok_r(NULL, "|", &sv);
        if (!g || !g[0] || !rt || !gt || !bt) continue;
        int i = g_nleg;
        for (int k = 0; k < g_nleg; k++) if (g_leg[k].glyph == g[0]) { i = k; break; }   /* replace a host row of the same glyph */
        g_leg[i].glyph = g[0]; g_leg[i].r = (unsigned char)atoi(rt); g_leg[i].g = (unsigned char)atoi(gt); g_leg[i].b = (unsigned char)atoi(bt);
        g_leg_hex[i][0] = '\0'; g_leg_cjk[i][0] = '\0';
        if (ct && ct[0] && strcmp(ct, "-") != 0) snprintf(g_leg_cjk[i], sizeof(g_leg_cjk[0]), "%s", ct);
        if (i == g_nleg) g_nleg++;
    }
    fclose(f);
}
/* ASCII view of events.txt (x y r g b charset index dir pattern [glyph]): one glyph per event in the event's own colour,
 * brightened; no glyph column = the first letter of the sheet name. Replaces the sprites while view_2d_style=ascii. */
static void draw_map_event_glyphs(unsigned char *px, int W, int ox, int oy, int cols, int rows, int cell) {
    char dd[PATH_BUF], path[PATH_BUF];
    if (!desk_dir(dd, sizeof(dd))) return;
    snprintf(path, sizeof(path), "%s/events.txt", dd);
    FILE *f = host_fopen(path, "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        int ex, ey, r, g, b, index, dir, pat, n = 0;
        char name[64], gl[8] = "";
        if (sscanf(line, "%d %d %d %d %d %63s %d %d %d %7s%n", &ex, &ey, &r, &g, &b, name, &index, &dir, &pat, gl, &n) < 9) continue;
        int scx = ex - ox, scy = ey - oy;
        if (scx < 0 || scy < 0 || scx >= cols || scy >= rows) continue;
        if (!gl[0]) { gl[0] = name[0] == '!' || name[0] == '$' ? name[1] : name[0]; gl[1] = '\0'; }
        unsigned int cp = bv_cjk_utf8_first(gl);
        if (!cp) continue;
        unsigned char gr = (unsigned char)(r + (255 - r) * 3 / 5), gg = (unsigned char)(g + (255 - g) * 3 / 5), gb = (unsigned char)(b + (255 - b) * 3 / 5);
        blit_cjk(px, W, scx * cell, scy * cell, cell, cp, gr, gg, gb);
    }
    fclose(f);
}

/* map.png sits beside map.txt. Tile step is png width / map.txt columns.
 * The 24 px exports and a future 48 px render both fit this. */
static int desk_map_png(char *path, size_t psz, int *tile_px) {
    path[0] = '\0';
    *tile_px = 0;
    const char *root = focused_root[0] ? focused_root : project_root;
    char wst[PATH_BUF], desk[64] = "", map_id[64] = "";
    snprintf(wst, sizeof(wst), "%s/pieces/world_01/state.txt", root);
    read_kv_str(wst, "desk_id", desk, sizeof(desk));
    read_kv_str(wst, "map_id", map_id, sizeof(map_id));
    if (!desk[0] || !map_id[0]) return 0;
    if (strchr(desk, '/') || strchr(map_id, '/')) return 0;
    snprintf(path, psz, "%s/pieces/system/maps/%s/%s/map.png", root, map_id, desk);
    char mt[PATH_BUF];
    snprintf(mt, sizeof(mt), "%s/pieces/system/maps/%s/%s/map.txt", root, map_id, desk);
    FILE *f = host_fopen(mt, "r");
    if (!f) return 0;
    char line[MAX_DIM + 8];
    if (!fgets(line, sizeof(line), f)) { fclose(f); return 0; }
    fclose(f);
    line[strcspn(line, "\r\n")] = '\0';
    int cols = (int)strlen(line);
    if (cols < 1) return 0;
    /* tile size filled after the PNG width is known; stash cols in *tile_px
     * as a negative sentinel and let the caller divide. */
    *tile_px = cols;
    return 1;
}

/* view_2d_style=scene: show a picture a GAME paints itself instead of drawing the book's tiles (rpg-pet: its own 660x430 frame with its sprite scaling and look, the one its standalone window shows).
 * Everything comes from the book's view.pdl: "VIEW | scene_raw | <path to a raw RGBA file>", "VIEW | scene_w | N", "VIEW | scene_h | N", and "VIEW | default_style | scene" (the style used while bv_state.txt
 * view_2d_style is empty, so a book can choose its own default 2D look; '\\' toggles scene <-> tiles, '0' back to 2D lands on the default). */
static int view_pdl_get(const char *key, char *out, size_t osz) {
    char dd[PATH_BUF], vp[PATH_BUF]; out[0] = 0; if (!desk_dir(dd, sizeof(dd))) return 0; char *sl = strrchr(dd, '/'); if (!sl) return 0; *sl = 0; snprintf(vp, sizeof(vp), "%s/view.pdl", dd);
    FILE *vf = host_fopen(vp, "r"); if (!vf) return 0; char l[MAX_LINE]; size_t kl = strlen(key); int got = 0;
    while (!got && fgets(l, sizeof(l), vf)) { char *p = strchr(l, '|'); if (!p) continue; p++; while (*p == ' ') p++; if (strncmp(p, key, kl) != 0) continue; p += kl; while (*p == ' ') p++; if (*p != '|') continue; p++; while (*p == ' ') p++;
        size_t n = strcspn(p, "\r\n"); while (n && p[n - 1] == ' ') n--; if (n >= osz) n = osz - 1; memcpy(out, p, n); out[n] = 0; got = 1; }
    fclose(vf); return got;
}
static int blit_scene(unsigned char *px, int W, int H) {
    char path[PATH_BUF], b[32]; int sw = 0, sh = 0;
    if (!view_pdl_get("scene_raw", path, sizeof(path)) || !path[0]) return 0;
    if (view_pdl_get("scene_w", b, sizeof(b))) sw = atoi(b);
    if (view_pdl_get("scene_h", b, sizeof(b))) sh = atoi(b);
    if (sw < 16 || sh < 16 || sw > 4096 || sh > 4096) return 0;
    size_t n = (size_t)sw * sh * 4; unsigned char *src = (unsigned char *)malloc(n); if (!src) return 0;
    FILE *f = host_fopen(path, "rb"); if (!f) { free(src); return 0; }
    size_t got = fread(src, 1, n, f); fclose(f); if (got != n) { free(src); return 0; }
    int x0 = (W - sw) / 2, y0 = (H - sh) / 2;                  /* centred; a smaller window crops */
    for (int y = 0; y < sh; y++) { int dy = y0 + y; if (dy < 0 || dy >= H) continue;
        for (int x = 0; x < sw; x++) { int dx = x0 + x; if (dx < 0 || dx >= W) continue; const unsigned char *s = src + ((size_t)y * sw + x) * 4; unsigned char *d = px + ((size_t)dy * W + dx) * 4; d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = 255; } }
    free(src); return 1;
}


/* ---- ENTITY PIECES IN 2D (entity frames spec 2026-10-27, 08-roadmap/design-docs/2026-10-27-ENTITY-FRAMES-AND-VIEWS-SPEC.md section 4): the 3D engine draws every pieces/<name>/ with a state.txt + model.txt
 * (the skiff, the asteroids, loot, ...); the 2D view used to know none of them, so a thing could be in 3D and missing here. Same files, same rules as bv_render_3d.c load_vehicle: names ship_01 + veh_1..24,
 * pos_x/pos_y (both required), size_x/size_y (footprint in cells), desk= (empty or * = every desk, else must equal world_01's desk_id), model.txt `dim X Y Z` + `x y z r g b` (X = grid x, Y = up, Z = grid y),
 * optional tile_model.txt (`x y z top side` cells) + tiles.rgba (24x24 tiles). Each piece becomes one small top-down picture (the highest voxel of each column, or the top tile of the highest cell), drawn over
 * the footprint. The world coordinates are the desk page's own cells (page cell c,r = world x c, y r). */
typedef struct { int x, y, z; unsigned char c[3]; } VoxTop;
#define P2D_MAX 25
typedef struct { float x, y, sx, sy; int iw, ih; unsigned char *img; } P2D;
static P2D g_p2d[P2D_MAX]; static int g_np2d = 0;
static void p2d_load(const char *root, const char *desk) {
    g_np2d = 0;
    for (int k = 0; k < P2D_MAX; k++) {
        char nm[16], path[PATH_BUF], line[MAX_LINE], vdesk[128] = ""; if (k == 0) snprintf(nm, sizeof nm, "ship_01"); else snprintf(nm, sizeof nm, "veh_%d", k);
        snprintf(path, sizeof(path), "%s/pieces/%s/state.txt", root, nm); FILE *f = host_fopen(path, "r"); if (!f) continue;
        P2D P; memset(&P, 0, sizeof P); P.sx = P.sy = 1; int have = 0;
        while (fgets(line, sizeof(line), f)) {
            if (!strncmp(line, "pos_x=", 6)) { P.x = (float)atof(line + 6); have |= 1; } else if (!strncmp(line, "pos_y=", 6)) { P.y = (float)atof(line + 6); have |= 2; }
            else if (!strncmp(line, "size_x=", 7)) P.sx = (float)atof(line + 7); else if (!strncmp(line, "size_y=", 7)) P.sy = (float)atof(line + 7);
            else if (!strncmp(line, "desk=", 5)) { line[strcspn(line, "\r\n")] = 0; snprintf(vdesk, sizeof vdesk, "%s", line + 5); } }
        fclose(f); if (have != 3 || P.sx <= 0 || P.sy <= 0 || (vdesk[0] && strcmp(vdesk, "*") && strcmp(desk, vdesk))) continue;
        int dim[3] = {0, 0, 0}, tdim[3] = {0, 0, 0};
        /* tile-faced model first: top tile of the highest cell of each column, the whole 24x24 picture per cell */
        snprintf(path, sizeof(path), "%s/pieces/%s/tile_model.txt", root, nm); FILE *tf = host_fopen(path, "r");
        if (tf) {
            snprintf(path, sizeof(path), "%s/pieces/%s/tiles.rgba", root, nm); FILE *xf = host_fopen(path, "rb"); unsigned char *tex = (unsigned char *)malloc(32 * 24 * 24 * 4); int tex_n = 0;
            if (xf && tex) { tex_n = (int)(fread(tex, 1, 32 * 24 * 24 * 4, xf) / (24 * 24 * 4)); } if (xf) fclose(xf);
            int cx[1024], cy[1024], cz[1024], ct[1024], n = 0;
            while (fgets(line, sizeof(line), tf)) { int a, b, c, tt, ts; if (!strncmp(line, "dim ", 4)) { sscanf(line + 4, "%d %d %d", &tdim[0], &tdim[1], &tdim[2]); continue; }
                if (n < 1024 && sscanf(line, "%d %d %d %d %d", &a, &b, &c, &tt, &ts) == 5) { cx[n] = a; cy[n] = b; cz[n] = c; ct[n] = tt; n++; } }
            fclose(tf);
            if (tex && tex_n > 0 && n > 0 && tdim[0] > 0 && tdim[2] > 0 && tdim[0] <= 32 && tdim[2] <= 32) {
                P.iw = tdim[0] * 24; P.ih = tdim[2] * 24; P.img = (unsigned char *)calloc((size_t)P.iw * P.ih, 4); static int hy[32][32];
                if (P.img) { for (int a = 0; a < 32; a++) for (int b = 0; b < 32; b++) hy[a][b] = -1;
                    for (int q = 0; q < n; q++) { if (cx[q] < 0 || cx[q] >= tdim[0] || cz[q] < 0 || cz[q] >= tdim[2] || ct[q] < 0 || ct[q] >= tex_n || cy[q] < hy[cx[q]][cz[q]]) continue;
                        hy[cx[q]][cz[q]] = cy[q]; for (int yy = 0; yy < 24; yy++) memcpy(P.img + ((size_t)(cz[q] * 24 + yy) * P.iw + cx[q] * 24) * 4, tex + ((size_t)ct[q] * 24 * 24 + (size_t)yy * 24) * 4, 24 * 4); } }
            }
            free(tex);
        }
        if (!P.img) {   /* plain voxel model: one texel per column, the colour of the highest voxel (a little brighter the higher it is) */
            snprintf(path, sizeof(path), "%s/pieces/%s/model.txt", root, nm); FILE *mf = host_fopen(path, "r"); if (!mf) continue;
            static VoxTop vt[4096]; int n = 0;
            while (fgets(line, sizeof(line), mf)) { int a, b, c, r, g, bl; if (!strncmp(line, "dim ", 4)) { sscanf(line + 4, "%d %d %d", &dim[0], &dim[1], &dim[2]); continue; }
                if (n < 4096 && sscanf(line, "%d %d %d %d %d %d", &a, &b, &c, &r, &g, &bl) == 6) { vt[n].x = a; vt[n].y = b; vt[n].z = c; vt[n].c[0] = (unsigned char)r; vt[n].c[1] = (unsigned char)g; vt[n].c[2] = (unsigned char)bl; n++; } }
            fclose(mf); if (n == 0 || dim[0] <= 0 || dim[2] <= 0 || dim[0] > 64 || dim[2] > 64) continue;
            P.iw = dim[0]; P.ih = dim[2]; P.img = (unsigned char *)calloc((size_t)P.iw * P.ih, 4); if (!P.img) continue;
            static int hy[64][64]; for (int a = 0; a < 64; a++) for (int b = 0; b < 64; b++) hy[a][b] = -1;
            for (int q = 0; q < n; q++) { if (vt[q].x < 0 || vt[q].x >= dim[0] || vt[q].z < 0 || vt[q].z >= dim[2] || vt[q].y < hy[vt[q].x][vt[q].z]) continue;
                hy[vt[q].x][vt[q].z] = vt[q].y; int sh = 85 + (dim[1] > 1 ? 15 * vt[q].y / (dim[1] - 1) : 15); unsigned char *d = P.img + ((size_t)vt[q].z * P.iw + vt[q].x) * 4;
                for (int ch = 0; ch < 3; ch++) { int v = vt[q].c[ch] * sh / 100; d[ch] = (unsigned char)(v > 255 ? 255 : v); } d[3] = 255; }
        }
        g_p2d[g_np2d++] = P;
    }
}

/* the scene = the page (0,0 .. cols,rows) and every piece's footprint, in cells */
static int g_xel_on = 0, g_xel_x = 0, g_xel_y = 0;      /* the xelector cell, counted into the scene only while it lies OUTSIDE the page (owner 2026-10-27: the 2D xelector was "stuck in the ship": the cursor moved on, the view stayed on the page) */
static void p2d_scene(int cols, int rows, float *x0, float *y0, float *x1, float *y1) {
    *x0 = 0; *y0 = 0; *x1 = (float)cols; *y1 = (float)rows;
    if (g_xel_on) { if (g_xel_x < *x0) *x0 = (float)g_xel_x; if (g_xel_y < *y0) *y0 = (float)g_xel_y; if (g_xel_x + 1 > *x1) *x1 = (float)(g_xel_x + 1); if (g_xel_y + 1 > *y1) *y1 = (float)(g_xel_y + 1); }
    for (int k = 0; k < g_np2d; k++) { if (g_p2d[k].x < *x0) *x0 = g_p2d[k].x; if (g_p2d[k].y < *y0) *y0 = g_p2d[k].y; if (g_p2d[k].x + g_p2d[k].sx > *x1) *x1 = g_p2d[k].x + g_p2d[k].sx; if (g_p2d[k].y + g_p2d[k].sy > *y1) *y1 = g_p2d[k].y + g_p2d[k].sy; } }
/* MINIMAP in 2D (owner 2026-10-27: "there should be a minimap in 2d if the map is bigger than the view"): the whole scene at a few pixels per cell in the top-right corner - the page as its own picture, every piece as its average colour, the
 * visible part as a yellow outline, the followed cell (xelector) as a red dot. Drawn only when part of the scene is outside the view. view.pdl `VIEW | view_2d_minimap | 0` turns it off. (vx0,vy0,vx1,vy1) = the visible rectangle in cells. */
static void p2d_minimap(unsigned char *px, int W, int H, int cols, int rows, const unsigned char *map_px, int map_w, int map_h, int map_tile, float vx0, float vy0, float vx1, float vy1, int selx, int sely) {
    float sx0, sy0, sx1, sy1; p2d_scene(cols, rows, &sx0, &sy0, &sx1, &sy1);
    if (vx0 <= sx0 + 0.01f && vy0 <= sy0 + 0.01f && vx1 >= sx1 - 0.01f && vy1 >= sy1 - 0.01f) return;      /* the whole scene is on screen: no minimap */
    float sw = sx1 - sx0, sh = sy1 - sy0; int mw, mh; float sc = 200.0f / sw; if (150.0f / sh < sc) sc = 150.0f / sh; if (sc > 8.0f) sc = 8.0f; if (sc < 1.0f) sc = 1.0f; mw = (int)(sw * sc); mh = (int)(sh * sc);
    if (mw + 12 > W || mh + 12 > H) return; int ox = W - mw - 8, oy = 8;
    for (int y = -2; y < mh + 2; y++) for (int x = -2; x < mw + 2; x++) { unsigned char *d = px + ((size_t)(oy + y) * W + (ox + x)) * 4; int edge = x < 0 || y < 0 || x >= mw || y >= mh; d[0] = edge ? 200 : 10; d[1] = edge ? 200 : 10; d[2] = edge ? 205 : 16; d[3] = 255; }
    for (int y = 0; y < mh; y++) for (int x = 0; x < mw; x++) { float wx = sx0 + (x + 0.5f) / sc, wy = sy0 + (y + 0.5f) / sc; unsigned char *d = px + ((size_t)(oy + y) * W + (ox + x)) * 4;
        if (wx >= 0 && wy >= 0 && wx < cols && wy < rows && map_px && map_tile > 0) { int mx = (int)(wx * map_tile), my = (int)(wy * map_tile); if (mx < map_w && my < map_h) { const unsigned char *s = map_px + ((size_t)my * map_w + mx) * 4; if (s[3]) { d[0] = s[0] / 2 + 20; d[1] = s[1] / 2 + 20; d[2] = s[2] / 2 + 20; } } }
        for (int k = 0; k < g_np2d; k++) { P2D *P = &g_p2d[k]; if (wx < P->x || wy < P->y || wx >= P->x + P->sx || wy >= P->y + P->sy) continue; long r = 0, g = 0, b = 0, n = 0;
            for (int i = 0; i < P->iw * P->ih; i += 3) { const unsigned char *s = P->img + (size_t)i * 4; if (s[3]) { r += s[0]; g += s[1]; b += s[2]; n++; } } if (n) { d[0] = (unsigned char)(r / n); d[1] = (unsigned char)(g / n); d[2] = (unsigned char)(b / n); } } }
    int rx0 = ox + (int)((vx0 - sx0) * sc), ry0 = oy + (int)((vy0 - sy0) * sc), rx1 = ox + (int)((vx1 - sx0) * sc), ry1 = oy + (int)((vy1 - sy0) * sc);
    for (int x = rx0; x <= rx1; x++) for (int e = 0; e < 2; e++) { int y = e ? ry1 : ry0; if (x >= ox && x < ox + mw && y >= oy && y < oy + mh) { unsigned char *d = px + ((size_t)y * W + x) * 4; d[0] = 70; d[1] = 255; d[2] = 255; d[3] = 255; } }
    for (int y = ry0; y <= ry1; y++) for (int e = 0; e < 2; e++) { int x = e ? rx1 : rx0; if (x >= ox && x < ox + mw && y >= oy && y < oy + mh) { unsigned char *d = px + ((size_t)y * W + x) * 4; d[0] = 70; d[1] = 255; d[2] = 255; d[3] = 255; } }
    if (selx >= 0 && sely >= 0) { int dx = ox + (int)((selx + 0.5f - sx0) * sc), dy = oy + (int)((sely + 0.5f - sy0) * sc); for (int y = dy - 1; y <= dy + 1; y++) for (int x = dx - 1; x <= dx + 1; x++) if (x >= ox && x < ox + mw && y >= oy && y < oy + mh) { unsigned char *d = px + ((size_t)y * W + x) * 4; d[0] = 240; d[1] = 40; d[2] = 40; d[3] = 255; } }
}

/* draw every piece over the footprint: screen = ax * world + bx (per axis), clipped to the frame */
static void p2d_draw(unsigned char *px, int W, int H, float axx, float bxx, float ayy, float byy) {
    for (int k = 0; k < g_np2d; k++) { P2D *P = &g_p2d[k];
        int x0 = (int)floorf(axx * P->x + bxx), x1 = (int)ceilf(axx * (P->x + P->sx) + bxx), y0 = (int)floorf(ayy * P->y + byy), y1 = (int)ceilf(ayy * (P->y + P->sy) + byy);
        if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 > W) x1 = W; if (y1 > H) y1 = H;
        for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) {
            float fx = (((float)x + 0.5f - bxx) / axx - P->x) / P->sx, fy = (((float)y + 0.5f - byy) / ayy - P->y) / P->sy; if (fx < 0 || fx >= 1 || fy < 0 || fy >= 1) continue;
            const unsigned char *s = P->img + ((size_t)((int)(fy * P->ih)) * P->iw + (int)(fx * P->iw)) * 4; if (s[3] == 0) continue;
            unsigned char *d = px + ((size_t)y * W + x) * 4; d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = 255; } }
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
    /* The 2D picture follows the xelector. selector_x/y is only the
     * fallback when that piece has no position yet. */
    {
        char xp[PATH_BUF];
        snprintf(xp, sizeof(xp), "%s/pieces/xelector_01/state.txt", focused_root[0] ? focused_root : fpr);
        int xx = read_kv_int(xp, "pos_x", -1);
        int xy = read_kv_int(xp, "pos_y", -1);
        if (xx >= 0 && xy >= 0) { sel_x = xx; sel_y = xy; }
    }
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
    load_desk_ascii_legend();
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
        {   /* the floor covers the page (pgr_extent), not a fixed 16x16 */
            int mx = 0, my = 0;
            if (pgr_extent(house_root, &mx, &my)) {
                if (mx + 1 > g_bw) g_bw = mx + 1;
                if (my + 1 > g_bh) g_bh = my + 1;
            }
            if (g_bw > MAX_DIM) g_bw = MAX_DIM;
            if (g_bh > MAX_DIM) g_bh = MAX_DIM;
        }
        for (int y = 0; y < g_bh; y++)
            for (int x = 0; x < g_bw; x++)
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
    {   /* a book may set its own tile size: maps/<book>/view.pdl "VIEW | cell_px | N" (rpg-pet: 48, its room is drawn 1:1 like its standalone window); the house value above stays for every other book */
        char dd[PATH_BUF], vp[PATH_BUF];
        if (desk_dir(dd, sizeof(dd))) { char *sl = strrchr(dd, '/'); if (sl) { *sl = 0; snprintf(vp, sizeof(vp), "%s/view.pdl", dd);
            FILE *vf = host_fopen(vp, "r");
            if (vf) { char l[MAX_LINE]; while (fgets(l, sizeof(l), vf)) { char *p = strstr(l, "cell_px"); if (!p) continue; p = strchr(p, '|'); if (!p) continue; int v = atoi(p + 1); if (v >= 8 && v <= 256) cell = v; break; } fclose(vf); } } }
    }

    /* Viewport size = the board window's live canvas pixel size, which
     * the khtpm renderer writes to #.desktop/pchq_board_view.txt every
     * layout (class="user-resizable"). Resize the window -> the map view
     * resizes with it. Falls back to 640x480 (== the 3D overlay) if the
     * file isn't there yet. */
    int W = 640, H = 480;
    { char vsz[PATH_BUF]; snprintf(vsz, sizeof(vsz), "%s/#.desktop/pchq_board_view.txt", house_root); bv_prefer_session_file(vsz, sizeof(vsz), "board_view.txt");
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

    {   /* view_2d_style=scene (the book's default while the style is unset, see view_pdl_get): a game-painted frame instead of the engine's tiles */
        char sty[16] = ""; read_kv_str(st, "view_2d_style", sty, sizeof(sty));
        if (!sty[0]) { char dsty[16]; if (view_pdl_get("default_style", dsty, sizeof(dsty))) snprintf(sty, sizeof(sty), "%s", dsty); }
        if (!side_mode && strcmp(sty, "scene") == 0 && blit_scene(px, W, H)) {
            char out[PATH_BUF], rec[PATH_BUF];
            snprintf(out, sizeof(out), "%s/pieces/display/rgb_frame_2d.raw", project_root);
            snprintf(rec, sizeof(rec), "%s/pieces/display/rgb_frame_2d.receipt.txt", project_root);
            write_atomic(out, px, (size_t)W * H * 4);
            { char rb[128]; int rn = snprintf(rb, sizeof(rb), "frame_w=%d\nframe_h=%d\n", W, H); if (rn > 0) write_atomic(rec, rb, (size_t)rn); }
            free(px);
            return 0;
        }
    }

    #define VP_PXR(SX,SY) (px + ((size_t)(SY) * W + (SX)) * 4)

    /* Phase 2: a desk with map.png is blitted 1:1. Scroll is the selector
     * in tile pixels, not the 80 px house cell. Side view (render_mode 2)
     * and every desk with no file keep the glyph path below. */
    {   /* entity pieces (skiff, asteroids, ...): the same files the 3D engine draws */
        const char *proot = focused_root[0] ? focused_root : project_root; char wst[PATH_BUF], cdesk[64] = "";
        snprintf(wst, sizeof(wst), "%s/pieces/world_01/state.txt", proot); read_kv_str(wst, "desk_id", cdesk, sizeof(cdesk));
#ifndef BV_2D_NO_PIECES      /* -DBV_2D_NO_PIECES builds the renderer as it was before the piece code: the harness uses it as the NEGATIVE fixture of the parity gate */
        p2d_load(proot, cdesk);
#endif
    }
    unsigned char *map_px = NULL;
    int map_w = 0, map_h = 0, map_tile = 0, map_ok = 0;
    int map_px0 = 0, map_py0 = 0;
    char style0[16] = ""; read_kv_str(st, "view_2d_style", style0, sizeof(style0));
    if (!side_mode && strcmp(style0, "ascii") != 0) {   /* ` = the glyph view: a desk picture is not blitted */
        char mp[PATH_BUF];
        int map_cols = 0;
        if (desk_map_png(mp, sizeof(mp), &map_cols) &&
            load_rgba_png(mp, &map_px, &map_w, &map_h) &&
            map_cols > 0 && map_w % map_cols == 0) {
            map_tile = map_w / map_cols;
            /* On-screen tile is desk_grid.pdl cell_px (the settings
             * grid step). 24 draws the export 1:1. 80 draws the same
             * pixels larger. Source art stays the PNG's own tile. */
            /* Default zoom = fit: the whole page on screen (whole-pixel tile step, never below 4 px). '+' / '-' (bv_menu_input) set view_2d_zoom 0..5 =
             * 100, 125, 150, 200, 300, 400 percent of the fit size; above fit the view centres on the selector and scrolls. The selector box below uses
             * the same size, so `cell` takes it. */
            int draw = cell, world_fit = 0; float wcx = 0, wcy = 0;
            { int mrows = map_tile > 0 ? map_h / map_tile : 0;
              g_xel_on = (sel_x >= 0 && sel_y >= 0 && map_cols > 0 && mrows > 0 && (sel_x >= map_cols || sel_y >= mrows)); g_xel_x = sel_x; g_xel_y = sel_y;
              if (map_cols > 0 && mrows > 0) {
                  static const int zpct[6] = {100, 125, 150, 200, 300, 400};
                  char zs[16] = ""; read_kv_str(st, "view_2d_zoom", zs, sizeof(zs)); int z = atoi(zs); if (z < -1) z = -1; if (z > 5) z = 5;
                  int fit = W / map_cols, fh = H / mrows; if (fh < fit) fit = fh; if (fit < 4) fit = 4;
                  if (z == -1 && g_np2d > 0) {      /* -1 = the whole scene: the page and every piece (entity frames spec, 2D camera = the parent frame) */
                      float ux0, uy0, ux1, uy1; p2d_scene(map_cols, mrows, &ux0, &uy0, &ux1, &uy1);
                      int f2 = (int)((float)W / (ux1 - ux0)), f3 = (int)((float)H / (uy1 - uy0)); if (f3 < f2) f2 = f3; if (f2 < 2) f2 = 2; fit = f2; world_fit = 1; wcx = (ux0 + ux1) / 2; wcy = (uy0 + uy1) / 2; z = 0; }
                  if (z < 0) z = 0;      /* -1 with no piece to show: just the fit-page size (never index zpct[-1]) */
                  draw = fit * zpct[z] / 100;
              } }
            if (draw < (world_fit ? 2 : 4)) draw = world_fit ? 2 : 4;
            if (map_tile >= 8 && map_tile <= 96 && (map_h % map_tile) == 0) {
                int cx = sel_x >= 0 ? sel_x : (map_cols / 2);
                int cy = sel_y >= 0 ? sel_y : ((map_h / map_tile) / 2);
                int view_w = W * map_tile / draw;
                int view_h = H * map_tile / draw;
                if (view_w < 1) view_w = 1;
                if (view_h < 1) view_h = 1;
                map_px0 = cx * map_tile - view_w / 2;
                map_py0 = cy * map_tile - view_h / 2;
                if (world_fit) { map_px0 = (int)(wcx * map_tile) - view_w / 2; map_py0 = (int)(wcy * map_tile) - view_h / 2; }
                else if (map_w > view_w) {
                    if (map_px0 < 0) map_px0 = 0;
                    if (map_px0 > map_w - view_w) map_px0 = map_w - view_w;
                } else map_px0 = -(view_w - map_w) / 2;
                if (world_fit) { /* centred above */ } else if (map_h > view_h) {
                    if (map_py0 < 0) map_py0 = 0;
                    if (map_py0 > map_h - view_h) map_py0 = map_h - view_h;
                } else map_py0 = -(view_h - map_h) / 2;
                if (!world_fit) {   /* PERSISTENT CAMERA (owner 2026-10-27: "arrows move the xelector, not the camera ... it should pan"): the view keeps its own position (pieces/display/view_2d_cam.txt, in cells) and is NOT re-centred on the xelector
                     * every frame. It scrolls by the least amount only when the xelector would leave the screen, and is clamped to the whole scene (page + pieces + the xelector). Shift+W/A/S/D (bv_menu_input.c) move it directly,
                     * ';' deletes the file = the default view again (centred on the xelector, clamped to the page, exactly as before). The first frame, and any change of window size or zoom, starts from that default. */
                    char cpath[PATH_BUF]; snprintf(cpath, sizeof(cpath), "%s/pieces/display/view_2d_cam.txt", project_root);
                    float vwc = (float)view_w / map_tile, vhc = (float)view_h / map_tile, camx = (float)map_px0 / map_tile, camy = (float)map_py0 / map_tile, lx = camx, ly = camy; int had = 0, fsx = -1, fsy = -1;
                    { FILE *cf = host_fopen(cpath, "r"); if (cf) { char l[96]; float fx = 0, fy = 0, fw = 0, fh = 0; int got = 0;
                        while (fgets(l, sizeof(l), cf)) { if (!strncmp(l, "x=", 2)) { fx = (float)atof(l + 2); got |= 1; } else if (!strncmp(l, "y=", 2)) { fy = (float)atof(l + 2); got |= 2; } else if (!strncmp(l, "vw=", 3)) { fw = (float)atof(l + 3); got |= 4; } else if (!strncmp(l, "vh=", 3)) { fh = (float)atof(l + 3); got |= 8; } else if (!strncmp(l, "sx=", 3)) { fsx = atoi(l + 3); got |= 16; } else if (!strncmp(l, "sy=", 3)) { fsy = atoi(l + 3); got |= 32; } }
                        fclose(cf); if (got == 63 && fabsf(fw - vwc) < 0.01f && fabsf(fh - vhc) < 0.01f) { camx = fx; camy = fy; had = 1; } } }
                    if (sel_x >= 0 && sel_y >= 0 && (!had || sel_x != fsx || sel_y != fsy)) {      /* only when the XELECTOR moved: keep it on screen with the least movement (a camera pan is never undone by this) */
                        if ((float)sel_x < camx) camx = (float)sel_x; else if ((float)sel_x + 1.0f > camx + vwc) camx = (float)sel_x + 1.0f - vwc;
                        if ((float)sel_y < camy) camy = (float)sel_y; else if ((float)sel_y + 1.0f > camy + vhc) camy = (float)sel_y + 1.0f - vhc; }
                    { float sx0, sy0, sx1, sy1; p2d_scene(map_cols, map_h / map_tile, &sx0, &sy0, &sx1, &sy1);
                      if (sx1 - sx0 <= vwc) camx = (sx0 + sx1) / 2 - vwc / 2; else { if (camx < sx0) camx = sx0; if (camx > sx1 - vwc) camx = sx1 - vwc; }
                      if (sy1 - sy0 <= vhc) camy = (sy0 + sy1) / 2 - vhc / 2; else { if (camy < sy0) camy = sy0; if (camy > sy1 - vhc) camy = sy1 - vhc; } }
                    map_px0 = (int)floorf(camx * map_tile + 0.5f); map_py0 = (int)floorf(camy * map_tile + 0.5f);
                    if (!had || fabsf(camx - lx) > 0.0005f || fabsf(camy - ly) > 0.0005f || sel_x != fsx || sel_y != fsy) { char cb[200]; int cn = snprintf(cb, sizeof(cb), "x=%.3f\ny=%.3f\nvw=%.3f\nvh=%.3f\nsx=%d\nsy=%d\n", camx, camy, vwc, vhc, sel_x, sel_y); if (cn > 0) write_atomic(cpath, cb, (size_t)cn); }
                }
                /* Sky behind the tiles. A clear map pixel keeps the sky.
                 * An opaque export covers it, which is the baked picture. */
                unsigned char *para = NULL;
                int para_w = 0, para_h = 0, loop_x = 0, loop_y = 0;
                if (desk_parallax(mp, &loop_x, &loop_y)) {
                    char pp[PATH_BUF];
                    size_t n = strlen(mp);
                    if (n > 8 && n < sizeof(pp)) {
                        snprintf(pp, sizeof(pp), "%.*s/parallax.png", (int)(n - 8), mp);
                        load_rgba_png(pp, &para, &para_w, &para_h);
                        if (!para) load_parallax_rgba(mp, &para, &para_w, &para_h);      /* the sky the 3D engine reads (parallax.rgba, size in parallax.pdl) when there is no png */
                    }
                }
                /* One image pixel tracks one map pixel, so the picture
                 * zooms with cell_px. Loop scrolls with the map. No loop
                 * stays pinned to the window, which is what MV does. */
                if (para && para_w > 0 && para_h > 0) {
                    for (int y = 0; y < H; y++) {
                        int my = y * map_tile / draw;
                        for (int x = 0; x < W; x++) {
                            int mx = x * map_tile / draw;
                            int sx = loop_x ? (map_px0 + mx) : mx;
                            int sy = loop_y ? (map_py0 + my) : my;
                            if (loop_x) { sx %= para_w; if (sx < 0) sx += para_w; }
                            if (loop_y) { sy %= para_h; if (sy < 0) sy += para_h; }
                            if (sx < 0 || sy < 0 || sx >= para_w || sy >= para_h) continue;
                            const unsigned char *s = para + ((size_t)sy * (size_t)para_w + (size_t)sx) * 4;
                            if (s[3] == 0) continue;
                            unsigned char *p = VP_PXR(x, y);
                            p[0] = s[0]; p[1] = s[1]; p[2] = s[2]; p[3] = 255;
                        }
                    }
                }
                for (int y = 0; y < H; y++) {
                    int sy = map_py0 + y * map_tile / draw;
                    for (int x = 0; x < W; x++) {
                        int sx = map_px0 + x * map_tile / draw;
                        unsigned char *p = VP_PXR(x, y);
                        if (sx < 0 || sy < 0 || sx >= map_w || sy >= map_h) continue;
                        const unsigned char *s = map_px + ((size_t)sy * (size_t)map_w + (size_t)sx) * 4;
                        if (s[3] == 0) continue;
                        p[0] = s[0]; p[1] = s[1]; p[2] = s[2]; p[3] = 255;
                    }
                }
                free(para);
                map_ok = 1; cell = draw;
                draw_map_events(px, W, H, mp, map_tile, map_px0, map_py0, draw);
                p2d_draw(px, W, H, (float)draw, -(float)map_px0 * draw / map_tile, (float)draw, -(float)map_py0 * draw / map_tile);      /* the pieces over their footprints (page cell c,r = world c,r) */
                {   char mm[16]; if (!view_pdl_get("view_2d_minimap", mm, sizeof(mm)) || atoi(mm) != 0)      /* on by default, VIEW | view_2d_minimap | 0 = off */
                        p2d_minimap(px, W, H, map_cols, map_h / map_tile, map_px, map_w, map_h, map_tile, (float)map_px0 / map_tile, (float)map_py0 / map_tile, (float)(map_px0 + view_w) / map_tile, (float)(map_py0 + view_h) / map_tile, sel_x, sel_y); }
                /* map_px0/map_py0 stay in source pixels. map_tile stays
                 * the PNG tile. The selector uses `cell` for the box. */
            }
        }
        if (!map_ok) { free(map_px); map_px = NULL; }
    }
    /* view_2d_style (` toggle): "ascii" -> DF/CDDA-style: one coloured
     * CJK glyph per cell (bg dimmed to a terrain hint); "emoji" -> the
     * emoji sprite over the terrain colour; "tiles" (default) -> the
     * flat colour grid (P1). Real palette tilesets are P2b. */
    char style[16] = ""; read_kv_str(st, "view_2d_style", style, sizeof(style));
    int want_emoji = (strcmp(style, "emoji") == 0);
    int want_ascii = (strcmp(style, "ascii") == 0);

    /* --- ground tiles (skipped when the desk picture was blitted) --- */
    if (!map_ok)
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
    for (int i = 0; !map_ok && !(g_own_ascii && want_ascii) && i < g_nent; i++) {
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

    if (!map_ok && !side_mode && !(g_own_ascii && want_ascii)) p2d_draw(px, W, H, (float)cell, -(float)ox * cell, (float)cell, -(float)oy * cell);      /* glyph/tile path: the same pieces */
    if (want_ascii && !side_mode) draw_map_event_glyphs(px, W, ox, oy, cols, rows, cell);

    /* --- the manual matrix grid (viewport-relative cell boundaries).
     * A grid is not part of the Maker picture, so it stays off when
     * map.png was blitted. --- */
    if (!map_ok) {
    for (int c = 0; c <= cols; c++) {
        int x = c * cell; if (x >= W) x = W - 1;
        for (int y = 0; y < H; y++) { unsigned char *p = VP_PXR(x, y); p[0]=gl_r; p[1]=gl_g; p[2]=gl_b; p[3]=255; }
    }
    for (int c = 0; c <= rows; c++) {
        int y = c * cell; if (y >= H) y = H - 1;
        for (int x = 0; x < W; x++) { unsigned char *p = VP_PXR(x, y); p[0]=gl_r; p[1]=gl_g; p[2]=gl_b; p[3]=255; }
    }
    }

    /* --- xelector: 2px inset border, bright accent ---
     * side_mode: honestly skipped for v1 - the xelector has no real
     * height of its own (only x/row), so there's no correct screen
     * position to draw its highlight at on this view's vertical
     * (height) axis. Ground/entities above already draw fully. */
    if (!side_mode && map_ok && sel_x >= 0 && sel_y >= 0 && map_tile > 0) {
        int box = cell > 0 ? cell : map_tile;
        int x0 = (sel_x * map_tile - map_px0) * box / map_tile;
        int y0 = (sel_y * map_tile - map_py0) * box / map_tile;
        if (x0 < W && y0 < H && x0 + box > 0 && y0 + box > 0) {
            unsigned char xr = 40, xg = 255, xb = 80;
            /* 6px, not 2. A 2px line disappeared into the painted tiles. */
            for (int t = 0; t < 6; t++) {
                for (int x = x0; x < x0 + box; x++) {
                    if (x < 0 || x >= W) continue;
                    int yA = y0 + t, yB = y0 + box - 1 - t;
                    if (yA >= 0 && yA < H) { unsigned char *a = VP_PXR(x, yA); a[0]=xr; a[1]=xg; a[2]=xb; a[3]=255; }
                    if (yB >= 0 && yB < H) { unsigned char *a = VP_PXR(x, yB); a[0]=xr; a[1]=xg; a[2]=xb; a[3]=255; }
                }
                for (int y = y0; y < y0 + box; y++) {
                    if (y < 0 || y >= H) continue;
                    int xA = x0 + t, xB = x0 + box - 1 - t;
                    if (xA >= 0 && xA < W) { unsigned char *a = VP_PXR(xA, y); a[0]=xr; a[1]=xg; a[2]=xb; a[3]=255; }
                    if (xB >= 0 && xB < W) { unsigned char *a = VP_PXR(xB, y); a[0]=xr; a[1]=xg; a[2]=xb; a[3]=255; }
                }
            }
        }
    } else if (!side_mode) {
        int scx = sel_x - ox, scy = sel_y - oy;
        if (sel_x >= 0 && sel_y >= 0 && scx >= 0 && scy >= 0 && scx < cols && scy < rows) {
            unsigned char xr = 40, xg = 255, xb = 80;
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
        if (!map_ok && hx >= 0 && hy >= 0) {
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
        if (!map_ok && hx >= 0 && hy >= 0) {   /* grok: not over a drawn map page */
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
        snprintf(cpath, sizeof(cpath), "%s/#.desktop/pchq_canvas_click.txt", house_root); bv_prefer_session_file(cpath, sizeof(cpath), "canvas_click.txt");
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
                    /* Painted (maker) maps have no placer: an armed placer takes the arrow keys, so rpg-pet's INT / the xelector would stop answering the arrows after any canvas click (same rule as the 3D click path). */
                    char mk_dir[PATH_BUF + 160], mk_map[128] = "", mk_desk[128] = "";
                    { char wp[PATH_BUF + 64]; snprintf(wp, sizeof(wp), "%s/pieces/world_01/state.txt", focused_root);
                      FILE *wf = host_fopen(wp, "r");
                      if (wf) { char ln[256]; while (fgets(ln, sizeof ln, wf)) { ln[strcspn(ln, "\r\n")] = 0; if (!strncmp(ln, "map_id=", 7)) snprintf(mk_map, sizeof mk_map, "%s", ln + 7); else if (!strncmp(ln, "desk_id=", 8)) snprintf(mk_desk, sizeof mk_desk, "%s", ln + 8); } fclose(wf); } }
                    snprintf(mk_dir, sizeof(mk_dir), "%s/pieces/system/maps/%s/%s", focused_root, mk_map, mk_desk);
                    int mk_painted = mk_map[0] && mk_desk[0] && access(mk_dir, F_OK) == 0;
                    if (!bvr_click(focused_root, project_root, bx, by, 0, 0) && !mk_painted)   /* Move range open: select/place */
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
            if (armed && !map_ok) {
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
    free(map_px);
    return 0;
}
