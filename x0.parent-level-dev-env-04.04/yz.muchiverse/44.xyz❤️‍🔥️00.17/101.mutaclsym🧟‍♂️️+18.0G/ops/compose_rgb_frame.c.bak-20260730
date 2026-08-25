/* compose_rgb_frame - one verb, one binary, no shared headers.
 * The GL/RGB mirror's content half: reads the exact same on-disk state
 * ops/compose_frame.c already reads (map.txt/furniture.txt/hero/items/
 * monsters, same camera-clamp formula, byte-for-byte copied from there)
 * and writes a raw RGBA32 framebuffer instead of an ASCII viewport -
 * pieces/display/rgb_frame.raw. Per
 * 2.muchi-verse/GRAND-ARCHITECTURE.md's GOVERNING CONSTRAINT: this file
 * makes ZERO GL calls of any kind, direct or indirect - it is plain,
 * portable, RISC-V-compilable-in-principle C that computes pixel color
 * values and writes them to a file, exactly the shape real 1.TPMOS
 * wraith_rgb_daemon.c already established (that file does the same job
 * for piececraft-wraith via a CPU voxel raymarch; this one is far
 * simpler - flat per-tile top-face color as the base layer, no
 * raymarch, since mutaclsym has no z-axis yet - PLUS, in emoji_mode,
 * real rasterized emoji pixels blitted on top of that base color, see
 * blit_emoji_tile()'s own header comment for the actual pipeline, ported
 * from real wraith-alpha's own working mechanism). system/gl_mirror.c
 * (a separate, later file) is the only thing allowed to touch GL, and
 * its only job is to blit whatever this op wrote.
 *
 * Per direct user instruction ("u said u have no eyes on the screen...
 * do the same [as wraith-gl's receipts]"), this op writes its own
 * receipt - pieces/display/rgb_frame.receipt.txt - with dimensions,
 * byte count, and an FNV-1a-64 checksum (same algorithm wraith_gl.c's
 * checksum_buffer() uses) so correctness can be verified by reading a
 * text file, not by looking at a window.
 *
 * Font/text (per direct follow-up instruction: "wraith-gl also has a
 * way to render fonts/nav that we should also have"). wraith_gl.c
 * itself has NO text-drawing code - the actual font pipeline lives in
 * wraith_rgb_daemon.c (load_glyphs()/blit_char()/blit_text()/
 * draw_text_asset(), lines ~401-560), which is exactly the right place
 * per the GOVERNING CONSTRAINT: text gets stamped into the CPU-computed
 * RGB buffer as plain pixel writes, never drawn via a GL text API. The
 * glyphs themselves are pre-generated once offline (real 1.TPMOS's
 * ops/font-gen-op.c, using FreeType, extracts each ASCII 32-126
 * character from a TTF into an 8x16 grid of #/. - see
 * pieces/registry/fonts/ascii/<code>/glyph.txt, copied verbatim from
 * wraith-alpha/assets/fonts/ascii/ rather than regenerated, same
 * "reuse the pre-extracted asset" precedent piececraft-wraith's own
 * voxel CSVs already established). load_glyphs()/blit_char()/
 * blit_text() below are a direct port of that same shape. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <time.h>
#include <ctype.h>
#include <math.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
/* Same viewport size as compose_frame.c's VIEWPORT_W/H - the two
 * renderers show the same window onto the same map, one in text, one
 * in pixels, matching the README.md "mirror" definition exactly: one
 * state, multiple renderers. */
#define VIEWPORT_W 40
#define VIEWPORT_H 16
#define MAX_MAP_W 256
#define MAX_MAP_H 256
/* Each map tile becomes a TILE_PX x TILE_PX solid-color block in the
 * framebuffer - flat top-face color, no raymarch (mutaclsym has no
 * z-axis/voxels yet, unlike piececraft-wraith). 16 keeps the output a
 * reasonable 640x256 texture without pretending to be more detailed
 * than the source data actually is. */
#define TILE_PX 16
/* Same 8x16 monospace cell size as real 1.TPMOS's font pipeline
 * (wraith_rgb_daemon.c's GLYPH_W/GLYPH_H) - matches glyph.txt's own
 * row/column count exactly, so load_glyphs() below needs no scaling. */
#define GLYPH_W 8
#define GLYPH_H 16
/* One text row above the tile grid (map id + turn) and TWO below (HP/
 * hunger/thirst/stamina, then the action-bar choice footer) - now
 * matches compose_frame.c's own two-line HUD footer exactly (build_
 * action_footer() below is a direct copy of that file's own build_
 * choice_footer()). Was 1 footer row/no action-bar text in an earlier
 * pass ("proving the font pipeline works end-to-end... in this first
 * pass") - the real gap that left, and the reason bumping to 2 rows
 * here, not shared/reused constants: MAX_TEXT_COLS's own truncation and
 * this whole file's "no shared headers" convention (see
 * build_action_footer()'s own header comment). message log tail is
 * still NOT mirrored here - out of scope for this pass, same "one thing
 * at a time" reasoning as before. */
#define HEADER_ROWS 1
#define FOOTER_ROWS 2
#define FRAME_W (VIEWPORT_W * TILE_PX)
#define FRAME_H (HEADER_ROWS * GLYPH_H + VIEWPORT_H * TILE_PX + FOOTER_ROWS * GLYPH_H)
#define MAX_TEXT_COLS (FRAME_W / GLYPH_W)
#define MAX_RECIPES_DISPLAY 16
#define BOX_TEXT_W 48
/* Matches item_id/monster_type's own 64-byte buffers exactly (both
 * flow into this one via cell_asset[]) - gcc can't prove a shorter
 * bound is safe from their own declared sizes alone, and every real id
 * in this project's registries is well under 32 anyway, but matching
 * the source buffers' own size removes any real truncation risk margin
 * rather than just suppressing the warning. */
#define ASSET_ID_BUF 64

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static int read_kv_int(const char *path, const char *key, int def) {
    FILE *f = fopen(path, "r");
    if (!f) return def;
    char line[MAX_LINE];
    int val = def;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(line, key) == 0) { val = atoi(eq + 1); break; }
    }
    fclose(f);
    return val;
}

static double read_kv_double(const char *path, const char *key, double def) {
    FILE *f = fopen(path, "r");
    if (!f) return def;
    char line[MAX_LINE];
    double val = def;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(line, key) == 0) { val = atof(eq + 1); break; }
    }
    fclose(f);
    return val;
}

static void read_kv_str(const char *path, const char *key, char *out, size_t out_sz, const char *def) {
    snprintf(out, out_sz, "%s", def);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(line, key) == 0) { snprintf(out, out_sz, "%s", eq + 1); break; }
    }
    fclose(f);
}

static void item_registry_field(const char *item_id, int field_index, char *out, size_t out_sz, const char *def) {
    snprintf(out, out_sz, "%s", def);
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/registry/items/items.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        line[strcspn(line, "\n")] = '\0';
        char *p1 = strchr(line, '|');
        if (!p1) continue;
        if ((size_t)(p1 - line) != strlen(item_id) || strncmp(line, item_id, p1 - line) != 0) continue;
        char *field = p1 + 1;
        for (int i = 1; i < field_index && field; i++) {
            field = strchr(field, '|');
            if (field) field++;
        }
        if (!field) break;
        char *end = strchr(field, '|');
        if (end) *end = '\0';
        snprintf(out, out_sz, "%s", field);
        break;
    }
    fclose(f);
}

/* Direct copy of compose_frame.c's own count_in_inventory() - same
 * "this op only reads, doesn't consume" reasoning for the craft panel's
 * ready/missing status. */
static int count_in_inventory(const char *item_id) {
    char inventory_dir[PATH_BUF];
    snprintf(inventory_dir, sizeof(inventory_dir), "%s/pieces/world_01/map_start/hero/inventory", project_root);
    DIR *d = opendir(inventory_dir);
    if (!d) return 0;
    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        char state_path[PATH_BUF + 384];
        snprintf(state_path, sizeof(state_path), "%s/%s/state.txt", inventory_dir, entry->d_name);
        char id[64];
        read_kv_str(state_path, "item_id", id, sizeof(id), "?");
        if (strcmp(id, item_id) == 0) count++;
    }
    closedir(d);
    return count;
}

static void monster_registry_field(const char *monster_type, int field_index, char *out, size_t out_sz, const char *def) {
    snprintf(out, out_sz, "%s", def);
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/registry/monsters/monster_types.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        line[strcspn(line, "\n")] = '\0';
        char *p1 = strchr(line, '|');
        if (!p1) continue;
        if ((size_t)(p1 - line) != strlen(monster_type) || strncmp(line, monster_type, p1 - line) != 0) continue;
        char *field = p1 + 1;
        for (int i = 1; i < field_index && field; i++) {
            field = strchr(field, '|');
            if (field) field++;
        }
        if (!field) break;
        char *end = strchr(field, '|');
        if (end) *end = '\0';
        snprintf(out, out_sz, "%s", field);
        break;
    }
    fclose(f);
}

/* Looks up rgb_top(R,G,B) for a glyph in a terrain/furniture-shaped
 * registry (glyph|id|name|walkable|rgb_top|unicode|rgb_top_emoji).
 * Returns 1 and fills r/g/b if found, 0 otherwise - caller tries
 * terrain then furniture, since the two registries' glyphs are
 * disjoint by construction (the same assumption compose_frame.c's own
 * merged grid already relies on: one glyph per cell, furniture wins
 * over terrain when present).
 *
 * emoji_mode, when set, prefers the row's rgb_top_emoji field instead
 * (falling back to plain rgb_top if that row has none). This is now
 * only the BASE/background layer under the real rasterized emoji pixels
 * blitted on top by blit_emoji_tile() below (see that function's own
 * header comment for the real pipeline) - it still matters on its own
 * for two real reasons: it shows through any transparent pixels in the
 * emoji glyph itself (most emoji don't fill their full 16x16 cell), and
 * it's the correct fallback for any tile whose asset failed to
 * generate/load. Originally this field WAS the entire emoji-mode
 * representation (a themed flat-color swap, no real glyph) - corrected
 * after direct instruction to look at wraith-alpha specifically, which
 * has a genuine FreeType-based emoji-to-RGB pipeline (confirmed via
 * direct research, not assumed - see blit_emoji_tile()'s own citation). */
static int glyph_rgb_top(const char *registry_rel_path, char glyph, int emoji_mode, int *r, int *g, int *b) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/%s", project_root, registry_rel_path);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        /* See ops/move_player.c's glyph_walkable() for why line[1]=='|'
         * (not just line[0]=='#') is the real comment test - '#' is
         * itself a valid glyph (t_wall). This exact check, done wrong,
         * is what first surfaced as every wall rendering magenta in
         * the debug PNG - the bug existed in move_player.c/
         * tick_monsters.c first (harmless there by coincidence), this
         * file just made it visible. */
        if (line[0] == '\n' || (line[0] == '#' && line[1] != '|')) continue;
        if (line[0] != glyph) continue;
        line[strcspn(line, "\n")] = '\0';
        char *p = strchr(line, '|'); /* -> id */
        if (!p) continue;
        p = strchr(p + 1, '|'); /* -> name */
        if (!p) continue;
        p = strchr(p + 1, '|'); /* -> walkable */
        if (!p) continue;
        p = strchr(p + 1, '|'); /* -> rgb_top */
        if (!p) continue;
        int base_found = (sscanf(p + 1, "%d,%d,%d", r, g, b) == 3);
        if (emoji_mode) {
            char *pe = strchr(p + 1, '|'); /* -> unicode */
            if (pe) pe = strchr(pe + 1, '|'); /* -> rgb_top_emoji */
            if (pe && sscanf(pe + 1, "%d,%d,%d", r, g, b) == 3) { found = 1; break; }
        }
        found = base_found;
        break;
    }
    fclose(f);
    return found;
}

/* Glyphs not covered by either registry (hero, ground items, monsters -
 * none of which have an rgb_top field yet, a known v0 limitation
 * documented in GRAND-ARCHITECTURE.md) get an obvious fallback color
 * rather than silently defaulting to black/floor - magenta reads as
 * "unmapped" at a glance the same way missing-texture magenta/black
 * checkerboards do in real engines. Hero '@' and the xlector cursor 'X'
 * (real active-target pattern, see ops/choice.c's own header comment)
 * both get their own fixed colors since they're always-present,
 * always-interesting special cases, not genuinely "unmapped" content -
 * cyan for the cursor, distinct from both the hero's yellow and the
 * generic magenta fallback. */
static void glyph_fallback_rgb(char glyph, int *r, int *g, int *b) {
    if (glyph == '@') { *r = 255; *g = 255; *b = 0; return; }
    if (glyph == 'X') { *r = 0; *g = 255; *b = 255; return; }
    if (glyph == ' ' || glyph == '\0') { *r = 0; *g = 0; *b = 0; return; }
    *r = 255; *g = 0; *b = 255;
}

static void glyph_to_rgb(char glyph, int emoji_mode, int *r, int *g, int *b) {
    if (glyph_rgb_top("pieces/registry/terrain/terrain_types.txt", glyph, emoji_mode, r, g, b)) return;
    if (glyph_rgb_top("pieces/registry/furniture/furniture_types.txt", glyph, emoji_mode, r, g, b)) return;
    glyph_fallback_rgb(glyph, r, g, b);
}

/* Same registry-scan shape as glyph_rgb_top() above, but stops after
 * the FIRST pipe to extract the row's own `id` field (glyph|id|...)
 * instead of walking further for rgb_top - this is the asset-directory
 * name blit_emoji_tile() looks up on disk, e.g. '#' -> "t_wall" ->
 * pieces/registry/emoji_assets/t_wall/voxels_16.csv. Terrain then
 * furniture, same disjoint-glyph-sets reasoning as glyph_rgb_top(). */
static int glyph_asset_id(const char *registry_rel_path, char glyph, char *out, size_t out_sz) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/%s", project_root, registry_rel_path);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '\n' || (line[0] == '#' && line[1] != '|')) continue;
        if (line[0] != glyph) continue;
        char *p1 = strchr(line, '|'); /* -> id */
        if (!p1) continue;
        char *end = strchr(p1 + 1, '|');
        if (end) *end = '\0';
        else p1[strcspn(p1, "\n")] = '\0';
        snprintf(out, out_sz, "%s", p1 + 1);
        found = 1;
        break;
    }
    fclose(f);
    return found;
}

static void terrain_or_furniture_asset_id(char glyph, char *out, size_t out_sz) {
    if (glyph_asset_id("pieces/registry/terrain/terrain_types.txt", glyph, out, out_sz)) return;
    if (glyph_asset_id("pieces/registry/furniture/furniture_types.txt", glyph, out, out_sz)) return;
    out[0] = '\0';
}

/* The real ASCII<->emoji GL pipeline, ported from real 1.TPMOS
 * wraith-alpha's own working mechanism after direct instruction to look
 * at it specifically ("look at wraith-alpha for how it converts emojis
 * to rgb") - researched via wraith_rgb_daemon.c's blit_codepoint()/
 * get_emoji_bitmap()/load_emoji_bitmap_from_disk(), confirmed by direct
 * read, not assumed. The real mechanism is a two-stage, offline-first
 * pipeline, NOT a live font-rasterize-every-frame approach (that file's
 * own header comment documents a real prior bug from trying exactly
 * that): a one-shot tool (pieces/system/emoji_extract/emoji_gen_atlas.c)
 * uses genuine FreeType (FT_Load_Char with FT_LOAD_COLOR, decoding
 * NotoColorEmoji.ttf's real embedded color bitmap glyphs - confirmed on
 * this machine, not hypothetical) to rasterize one emoji into a PNG,
 * then emoji_xtract.c downsamples it into a plain-text RGBA CSV
 * ("# resolution=N" header, one "r,g,b,a" row per pixel, row-major).
 * The daemon then just reads that pre-generated CSV and blits real
 * pixels - no FreeType calls anywhere in the hot per-frame path.
 *
 * mutaclsym's own version: assets pre-generated ONCE (not on demand
 * like wraith-alpha's own lazy fork/exec cache) into
 * pieces/registry/emoji_assets/<asset_id>/voxels_16.csv, at N=16 -
 * chosen to match TILE_PX exactly, so no runtime scaling is needed,
 * unlike wraith-alpha's own 64->N downsample (its daemon targets a
 * variable emoji_glyph_size; mutaclsym's tile size is fixed). Keyed by
 * this project's own registry `id` (see glyph_asset_id() above) rather
 * than the real precedent's hex-codepoint directory naming - mutaclsym
 * doesn't need cross-content asset sharing at that granularity, every
 * registry row already has its own unique id, so reusing it avoids a
 * whole extra codepoint-decoding step for no real benefit here. */
static int load_emoji_voxels(const char *asset_id, unsigned char voxels[TILE_PX][TILE_PX][4]) {
    /* Single-entry cache - adjacent tiles in the same frame are very
     * often the same asset_id (a run of floor tiles, a wall row), and
     * this is popen/fopen-per-call registry-lookup-adjacent code
     * already, matching this project's existing "correctness over
     * micro-optimization, small file scale" convention rather than
     * building real caching infrastructure for it. */
    static char cached_id[ASSET_ID_BUF] = "";
    static unsigned char cached_voxels[TILE_PX][TILE_PX][4];
    static int cache_valid = 0;

    if (cache_valid && strcmp(cached_id, asset_id) == 0) {
        memcpy(voxels, cached_voxels, sizeof(cached_voxels));
        return 1;
    }

    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/registry/emoji_assets/%s/voxels_16.csv", project_root, asset_id);
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    char line[128];
    int idx = 0;
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        if (strncmp(line, "r,g,b,a", 7) == 0) continue;
        int r, g, b, a;
        if (sscanf(line, "%d,%d,%d,%d", &r, &g, &b, &a) != 4) continue;
        if (idx >= TILE_PX * TILE_PX) break;
        int vy = idx / TILE_PX, vx = idx % TILE_PX;
        voxels[vy][vx][0] = (unsigned char)r;
        voxels[vy][vx][1] = (unsigned char)g;
        voxels[vy][vx][2] = (unsigned char)b;
        voxels[vy][vx][3] = (unsigned char)a;
        idx++;
    }
    fclose(f);
    if (idx != TILE_PX * TILE_PX) return 0;

    snprintf(cached_id, sizeof(cached_id), "%s", asset_id);
    memcpy(cached_voxels, voxels, sizeof(cached_voxels));
    cache_valid = 1;
    return 1;
}

/* Alpha-composites one tile's real rasterized emoji pixels on top of
 * whatever's already in the framebuffer at that tile (the flat
 * rgb_top/rgb_top_emoji color, so transparent emoji pixels show the
 * themed background color through, not black) - same "blit with alpha
 * testing" shape confirmed in real wraith_rgb_daemon.c's own
 * blit_codepoint() path, just per-tile instead of per-glyph-cell. */
static void blit_emoji_tile(unsigned char fb[FRAME_H][FRAME_W][4], int tile_x0, int tile_y0,
                             unsigned char voxels[TILE_PX][TILE_PX][4]) {
    for (int y = 0; y < TILE_PX; y++) {
        int fy = tile_y0 + y;
        if (fy < 0 || fy >= FRAME_H) continue;
        for (int x = 0; x < TILE_PX; x++) {
            int fx = tile_x0 + x;
            if (fx < 0 || fx >= FRAME_W) continue;
            unsigned char a = voxels[y][x][3];
            if (a == 0) continue;
            if (a == 255) {
                fb[fy][fx][0] = voxels[y][x][0];
                fb[fy][fx][1] = voxels[y][x][1];
                fb[fy][fx][2] = voxels[y][x][2];
            } else {
                fb[fy][fx][0] = (unsigned char)((voxels[y][x][0] * a + fb[fy][fx][0] * (255 - a)) / 255);
                fb[fy][fx][1] = (unsigned char)((voxels[y][x][1] * a + fb[fy][fx][1] * (255 - a)) / 255);
                fb[fy][fx][2] = (unsigned char)((voxels[y][x][2] * a + fb[fy][fx][2] * (255 - a)) / 255);
            }
            fb[fy][fx][3] = 255;
        }
    }
}

/* glyphs[c] is an 8x16 on/off mask (1=foreground) for ASCII char c,
 * 32 <= c < 127 - loaded once from pieces/registry/fonts/ascii/<c>/
 * glyph.txt (a plain #/. text grid, ported unmodified from real
 * 1.TPMOS's wraith_rgb_daemon.c load_glyphs()/font-gen-op.c). Missing
 * files (shouldn't happen - all 95 printable-ASCII codes were copied)
 * leave that slot all-zero, same silent-skip behavior the original
 * has. */
static unsigned char glyphs[127][GLYPH_H][GLYPH_W];

static void load_glyphs(void) {
    memset(glyphs, 0, sizeof(glyphs));
    for (int c = 32; c < 127; c++) {
        char path[PATH_BUF];
        snprintf(path, sizeof(path), "%s/pieces/registry/fonts/ascii/%d/glyph.txt", project_root, c);
        FILE *f = fopen(path, "r");
        if (!f) continue;
        char line[64];
        int y = 0;
        while (y < GLYPH_H && fgets(line, sizeof(line), f)) {
            for (int x = 0; x < GLYPH_W && line[x] != '\0' && line[x] != '\n'; x++) {
                glyphs[c][y][x] = (line[x] == '#') ? 1 : 0;
            }
            y++;
        }
        fclose(f);
    }
}

/* Stamps one character's foreground pixels directly into the RGBA
 * framebuffer at pixel origin (px,py) - plain CPU pixel writes, same
 * as wraith_rgb_daemon.c's blit_char(), no GL text API of any kind
 * (there is no such thing anywhere in this codebase, by the GOVERNING
 * CONSTRAINT). Background pixels are left untouched (transparent over
 * whatever's already there), matching the original's behavior. */
static void blit_char(unsigned char fb[FRAME_H][FRAME_W][4], int px, int py, unsigned char c,
                       unsigned char r, unsigned char g, unsigned char b) {
    if (c < 32 || c > 126) return;
    for (int y = 0; y < GLYPH_H; y++) {
        int fy = py + y;
        if (fy < 0 || fy >= FRAME_H) continue;
        for (int x = 0; x < GLYPH_W; x++) {
            int fx = px + x;
            if (fx < 0 || fx >= FRAME_W) continue;
            if (!glyphs[c][y][x]) continue;
            fb[fy][fx][0] = r;
            fb[fy][fx][1] = g;
            fb[fy][fx][2] = b;
            fb[fy][fx][3] = 255;
        }
    }
}

/* One text row, left-aligned at pixel origin (px,py), clipped to
 * MAX_TEXT_COLS - plain ASCII only (no UTF-8 decoding; nothing this op
 * prints today needs it, unlike wraith_rgb_daemon.c's blit_text()
 * which also handles emoji codepoints via blit_codepoint() - out of
 * scope for mutaclsym's HUD text specifically, tracked separately from
 * the emoji/ascii tile parity already documented in
 * GRAND-ARCHITECTURE.md's §0a). */
static void blit_text(unsigned char fb[FRAME_H][FRAME_W][4], int px, int py, const char *text,
                       unsigned char r, unsigned char g, unsigned char b) {
    int col = 0;
    for (const char *p = text; *p && col < MAX_TEXT_COLS; p++, col++) {
        blit_char(fb, px + col * GLYPH_W, py, (unsigned char)*p, r, g, b);
    }
}

/* Direct copy of compose_frame.c's own build_choice_footer() - same
 * pdl_reader.+x hero list_methods call, same "row 0/1 (move/end_turn)
 * skipped, numbering starts at 2" convention, same one-shared-source-of-
 * truth reasoning (see that file's own header comment for the full
 * citation of the real 1.TPMOS bug class - get_piece_methods_op.c vs
 * pdl_reader.c/route_input() disagreeing - this avoids by construction).
 * This was the ACTUAL gap behind "I don't see the ASCII action-bar text
 * in the GL window" - both mutaclsym's dispatch (ops/choice.c) and the
 * ASCII footer (compose_frame.c) already read hero/piece.pdl's own
 * METHOD table dynamically; this GL-side renderer simply never got the
 * matching row. Duplicated here (not shared via a header) per this
 * project's own "one op = one binary, no shared headers" convention -
 * see MAX_TEXT_COLS's own clipping in blit_text() for what happens if
 * this line runs long: clipped at the frame edge, not wrapped, matching
 * the same bounded-buffer behavior the ASCII version already has. */
static void build_action_footer(const char *project_root_, char *out, size_t out_sz, int action_cursor, int interact_mode) {
    char buf[512];

    /* interact_mode replaces the whole footer with a cursor-mode hint,
     * same reasoning as compose_frame.c's own copy: digits are genuine
     * no-ops while controlling the xlector cursor (see ops/choice.c),
     * so showing the numbered action list here would be misleading. */
    if (interact_mode) {
        /* Camera key hints vary by camera_mode. POV switching is always
         * shown; movement/rotation keys depend on the active mode. */
        snprintf(out, out_sz, "[1-4] POV [wasd] Move [q/e] Turn [r/t] Look [c/v] Z [f] Reset  [enter] Examine  [t] Throw  [esc] Back");
        return;
    }

    snprintf(buf, sizeof(buf), "[wasd/arrows] Move");
    size_t len = strlen(buf);

    char cmd[PATH_BUF];
    snprintf(cmd, sizeof(cmd), "'%s/ops/+x/pdl_reader.+x' hero list_methods", project_root_);
    FILE *pf = popen(cmd, "r");
    int idx = 0;
    if (pf) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), pf)) {
            line[strcspn(line, "\r\n")] = '\0';
            if (idx >= 2 && line[0]) {
                char label[MAX_LINE];
                snprintf(label, sizeof(label), "%s", line);
                label[0] = (char)toupper((unsigned char)label[0]);
                const char *cur = (idx == action_cursor) ? "[>]" : "[ ]";
                int n = snprintf(buf + len, sizeof(buf) - len, "  %s %d. [%s]", cur, idx, label);
                if (n > 0 && (size_t)n < sizeof(buf) - len) len += (size_t)n;
            }
            idx++;
        }
        pclose(pf);
    }
    int n = snprintf(buf + len, sizeof(buf) - len, "  [i] Interact  [e] Emoji  [q] Quit");
    if (n > 0 && (size_t)n < sizeof(buf) - len) len += (size_t)n;

    snprintf(out, out_sz, "%s", buf);
}

/* GL-side equivalent of compose_frame.c's own draw_panel_box() - was the
 * real, concrete gap behind "id like to see menu in gl" (the flat
 * action-bar footer already got this treatment - overlay panels hadn't).
 * Same overlay shape (a titled, bordered box glued to the tile
 * viewport's top-left corner, drawn OVER the already-rasterized map,
 * screen-space not map-space so it stays put regardless of camera
 * scroll), same content (title + numbered/bracket-cursor rows + a
 * trailing Cancel/Close row) - just pixels instead of characters.
 * ASCII blanks the map underneath by overwriting grid characters with
 * spaces first; this fills a solid background rectangle first instead,
 * for the same reason (so old tile colors don't show through the box's
 * own "empty" areas). box_row/box_col below are TILE-grid coordinates
 * (matching compose_frame.c's own box_row=1/box_col=2 exactly, since a
 * viewport row/col here is one TILE_PX/GLYPH_H-ish text line - both
 * happen to be 16px, no separate conversion needed), converted to real
 * pixel offsets at the two call sites below. */
static void draw_panel_box_gl(unsigned char fb[FRAME_H][FRAME_W][4], int tile_y0,
                               const char *title, char panel_rows[][BOX_TEXT_W + 1],
                               int panel_row_count, const char *cancel_row) {
    int box_row = 1, box_col = 2;
    int box_h = panel_row_count + 3; /* border + rows + Cancel + border */
    if (box_row + box_h > VIEWPORT_H) box_h = VIEWPORT_H - box_row;

    int box_x = box_col * TILE_PX;
    int box_y = tile_y0 + box_row * TILE_PX;
    int box_w_px = BOX_TEXT_W * GLYPH_W;
    if (box_x + box_w_px > FRAME_W) box_w_px = FRAME_W - box_x;
    int box_h_px = box_h * GLYPH_H;

    /* Solid background first, so old tile colors underneath don't show
     * through - same reason ASCII pads short rows with spaces before
     * overwriting the grid. */
    for (int y = 0; y < box_h_px; y++) {
        int fy = box_y + y;
        if (fy < 0 || fy >= FRAME_H) continue;
        for (int x = 0; x < box_w_px; x++) {
            int fx = box_x + x;
            if (fx < 0 || fx >= FRAME_W) continue;
            fb[fy][fx][0] = 20; fb[fy][fx][1] = 20; fb[fy][fx][2] = 30; fb[fy][fx][3] = 255;
        }
    }

    char top[BOX_TEXT_W + 12];
    snprintf(top, sizeof(top), "+--- %s ------------------+", title);
    for (int r = 0; r < box_h; r++) {
        const char *line;
        if (r == 0) line = top;
        else if (r <= panel_row_count) line = panel_rows[r - 1];
        else if (r == panel_row_count + 1) line = cancel_row;
        else line = "+-----------------------------+";
        blit_text(fb, box_x, box_y + r * GLYPH_H, line, 255, 255, 0);
    }
}

/* Same FNV-1a-64 algorithm real 1.TPMOS wraith_gl.c's checksum_buffer()
 * uses, so a checksum computed here and one computed there (once
 * system/gl_mirror.c exists and reads this same file) are directly
 * comparable - the whole point of the receipt pattern the user asked
 * for: verify the pipeline via matching numbers in two text files,
 * never by looking at a window. */
static uint64_t checksum_buffer(const unsigned char *buf, size_t len) {
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < len; i++) {
        hash ^= buf[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

static void write_receipt(const char *path, size_t byte_count, uint64_t checksum,
                           int map_w, int map_h, int cam_x, int cam_y) {
    FILE *f = fopen(path, "w");
    if (!f) return;
    time_t now = time(NULL);
    fprintf(f, "op=compose_rgb_frame\n");
    fprintf(f, "frame_w=%d\n", FRAME_W);
    fprintf(f, "frame_h=%d\n", FRAME_H);
    fprintf(f, "tile_px=%d\n", TILE_PX);
    fprintf(f, "viewport_w=%d\n", VIEWPORT_W);
    fprintf(f, "viewport_h=%d\n", VIEWPORT_H);
    fprintf(f, "bytes_per_pixel=4\n");
    fprintf(f, "byte_count=%zu\n", byte_count);
    fprintf(f, "expected_byte_count=%d\n", FRAME_W * FRAME_H * 4);
    fprintf(f, "checksum_fnv1a64=%016llx\n", (unsigned long long)checksum);
    fprintf(f, "map_w=%d\n", map_w);
    fprintf(f, "map_h=%d\n", map_h);
    fprintf(f, "cam_x=%d\n", cam_x);
    fprintf(f, "cam_y=%d\n", cam_y);
    fprintf(f, "written_at=%ld\n", (long)now);
    fclose(f);
}

/* ========================================================================
 * 3D VIEW - real, direct instruction (2026-07-17), grounded against real
 * piececraft-wraith (`wraith-alpha/wraith-projects/piececraft-wraith/
 * manager/piececraft-wraith_manager.c`'s update_scene_objects() and
 * `wraith-alpha/plugins/wraith_rgb_daemon.c`'s draw_tile_zmap_preview_3d()/
 * project_world_point(), both read in full before writing this - NOT the
 * legacy gltpm-based `projects/piececraft-3d`, confirmed dead kruft
 * predating wraith, per direct correction).
 *
 * NAMED SIMPLIFICATIONS vs. the real reference (a genuinely bigger,
 * more sophisticated engine - not silently treated as equivalent):
 *   - Painter's algorithm (back-to-front row order), not a real per-
 *     pixel depth buffer - overlapping geometry can occasionally
 *     mis-order (e.g. a near wall's corner drawn under a far wall's
 *     edge). The reference's own history shows it started here too
 *     (ray marching + depth buffer was a LATER upgrade on top of this
 *     same box-per-tile stage) - a real, legitimate v1, not a shortcut.
 *   - Faces behind the near plane are REJECTED outright, not clipped -
 *     a wall extremely close to the camera can visibly vanish rather
 *     than clip smoothly. The reference's own draw_clipped_face() does
 *     real frustum clipping - a genuine, separate future upgrade.
 *   - No ray marching, no translucency pass, no mouse-driven orbit -
 *     one simple box per non-walkable tile (top + camera-facing sides),
 *     a flat ground quad per walkable tile.
 * The projection math itself (world->camera translate, pitch rotate,
 * perspective divide) IS the same real formula as project_world_point().
 * Yaw (facing direction) is applied as a separate rotation before that -
 * the reference does this too (see its own g_cam_yaw/unrotate_by_yaw()),
 * just for mouse-orbit rather than facing-direction here.
 */
#define VIEW_3D_RADIUS 12 /* tiles around the hero actually rendered - real perf/scope bound, not the whole map */
#define NEAR_PLANE_3D 0.15

/* Entity (hero/xlector/monster/item) extrusion box: inset horizontally
 * from the full tile footprint and shorter than a wall's full 1.0
 * height, so a standing entity reads as a distinct object on its tile
 * rather than a second wall block. */
#define ENTITY_MARGIN 0.15
#define ENTITY_HEIGHT 0.9

/* Decor objects: walkable tiles that are still a real, specific emoji
 * (not bare ground) - chair/bed (walkable furniture, unlike the
 * blocking crate/counter/sink/table already handled by the wall path
 * via terrain_or_furniture_walkable()) and stairs down. Sized like a
 * low piece of furniture: bigger footprint than a standing entity,
 * shorter than a wall block. */
#define DECOR_MARGIN 0.1
#define DECOR_HEIGHT 0.5

static int glyph_is_decor_object(char glyph) {
    return glyph == 'h' /* f_chair */ || glyph == 'b' /* f_bed */ || glyph == '>' /* t_stairs_down */;
}

/* Mode 2 (third-person) camera preset - tweak these to reposition the
 * third-person camera. Pitch is negative = looking down, positive = up. */
#define MODE2_PITCH   -45.0  /* degrees: negative = looking down at player */
#define MODE2_Y_OFF    4.0   /* height above player (world units) */
#define MODE2_Z_OFF   -3.0   /* behind player (negative = behind facing dir) */

/* Simple perspective projection: world (wx,wy,wz) -> screen (out_x,out_y),
 * real camera-space transform (translate by cam pos, rotate by pitch),
 * then perspective divide. wy is height (up), wz is depth (forward from
 * the camera's own unrotated axis - yaw is applied by the CALLER before
 * this, by rotating wx/wz around the yaw pivot; see render_3d_view()). */
static void project_3d(double wx, double wy, double wz,
                        double cam_x, double cam_y, double cam_z, double pitch_deg,
                        double focal, int screen_cx, int screen_cy, double scale,
                        int *out_x, int *out_y, double *out_z2) {
    double rx = wx - cam_x;
    double ry = wy - cam_y;
    double rz = wz - cam_z;
    double ax = pitch_deg * M_PI / 180.0;
    double cpitch = cos(ax), spitch = sin(ax);
    double y2 = ry * cpitch - rz * spitch;
    double z2 = ry * spitch + rz * cpitch;
    int rejected = (z2 <= NEAR_PLANE_3D);
    if (rejected) z2 = NEAR_PLANE_3D; /* clamp so callers doing simple math don't divide-by-near-zero; drawing code below still checks out_z2 itself to reject */
    double persp = focal / z2;
    /* REAL BUG FIX, GL-only (live-reported against tempt-island's own
     * port of this file: "the whole map seems inverted right and
     * left" - re-verified and confirmed fixed there before porting
     * here). World X (col) increases with yaw_rotate()'s own standard-
     * math-convention rotation, but this project's world uses row-
     * increases-DOWNWARD as its Z axis (a left-handed 2D plane, not the
     * right-handed one that rotation formula assumes) - screen_cx + rx
     * put east/west content on the opposite screen side from where the
     * ASCII/2D view (and every other renderer) shows it. Negating rx
     * here un-mirrors the X axis; raymarch_walls_3d()'s own dx_cam
     * computation is this formula's algebraic inverse and must flip the
     * same way for the wall pass to stay aligned with this (floor)
     * pass - see that function's own matching comment. */
    *out_x = screen_cx - (int)(rx * scale * persp);
    *out_y = screen_cy - (int)(y2 * scale * persp);
    if (out_z2) *out_z2 = z2;
}

/* Yaw-rotates a world (x,z) pair around (pivot_x,pivot_z) by yaw_deg -
 * applied before project_3d() so "forward" always means "into the
 * screen" regardless of which absolute map direction the hero (or the
 * free camera) currently faces. Real reference does the equivalent
 * rotation for mouse-orbit (g_cam_yaw/unrotate_by_yaw()) - this reuses
 * the same idea, driven by ops/move_player.c's own `facing` field
 * instead of a mouse drag. */
static void yaw_rotate(double x, double z, double pivot_x, double pivot_z, double yaw_deg,
                        double *out_x, double *out_z) {
    double rad = yaw_deg * M_PI / 180.0;
    double cy = cos(rad), sy = sin(rad);
    double dx = x - pivot_x, dz = z - pivot_z;
    *out_x = pivot_x + dx * cy - dz * sy;
    *out_z = pivot_z + dx * sy + dz * cy;
}

static double facing_to_yaw(int facing) {
    /* ARROW_* sentinel values (1000-1003, same convention every file in
     * this project uses) map to compass-style yaw degrees.
     *
     * REAL BUG FIX, ported from tempt-island's own port of this file
     * (live-tested and confirmed fixed there first): project_3d()
     * structurally treats +Z (rz = wz - cam_z > 0) as "in front of the
     * camera" (that's what NEAR_PLANE_3D rejects against) - that's
     * baked into its math, not a convention this function gets to
     * redefine by itself. With the ORIGINAL 0°/180° UP/DOWN mapping, a
     * point north of the hero (smaller row) had negative rz at yaw=0
     * and was wrongly treated as BEHIND the camera, while a point south
     * (larger row, genuinely behind a hero facing up) had positive rz
     * and wrongly rendered as "in front" - i.e. ARROW_UP showed what's
     * behind the hero, not ahead of it ("camera facing player's back").
     * ARROW_RIGHT/LEFT had the same class of inversion, confirmed by
     * the same live playtest after the UP/DOWN fix landed - swapped the
     * same way, 90°<->270°. */
    switch (facing) {
        case 1002: return 180.0; /* ARROW_UP */
        case 1003: return 0.0;   /* ARROW_DOWN */
        case 1001: return 270.0; /* ARROW_RIGHT */
        case 1000: return 90.0;  /* ARROW_LEFT */
        default:   return 180.0; /* unset - matches this project's own default facing=1002 (ARROW_UP) in hero/state.txt */
    }
}

/* Simple scanline fill of a convex quad (4 screen points, in order) -
 * real, standard technique, NOT the reference's own depth-interpolated
 * clipped-face fill (see this section's own header comment on why a
 * plain painter's-algorithm fill is a real, named simplification here). */
/* fb is a flat RGBA8888 buffer, fb_w*fb_h*4 bytes - explicit
 * width/height (not the fixed FRAME_W/FRAME_H) so this same code can
 * fill either the full frame (regular "run" mode) or a smaller,
 * map-sized overlay buffer (chtpm mode - see this section's own
 * MAP3D_MARKER/overlay-compositing comment on render_3d_view() below). */
static void fill_quad_3d(unsigned char *fb, int fb_w, int fb_h,
                          const int xs[4], const int ys[4],
                          int r, int g, int b) {
    int ymin = ys[0], ymax = ys[0];
    for (int i = 1; i < 4; i++) {
        if (ys[i] < ymin) ymin = ys[i];
        if (ys[i] > ymax) ymax = ys[i];
    }
    if (ymin < 0) ymin = 0;
    if (ymax >= fb_h) ymax = fb_h - 1;
    for (int y = ymin; y <= ymax; y++) {
        double xleft = 1e18, xright = -1e18;
        for (int i = 0; i < 4; i++) {
            int j = (i + 1) % 4;
            int ay = ys[i], by = ys[j];
            if (ay == by) continue;
            if ((y >= ay && y < by) || (y >= by && y < ay)) {
                double t = (double)(y - ay) / (double)(by - ay);
                double x = xs[i] + t * (xs[j] - xs[i]);
                if (x < xleft) xleft = x;
                if (x > xright) xright = x;
            }
        }
        if (xleft > xright) continue;
        int xi0 = (int)xleft, xi1 = (int)xright;
        if (xi0 < 0) xi0 = 0;
        if (xi1 >= fb_w) xi1 = fb_w - 1;
        for (int x = xi0; x <= xi1; x++) {
            unsigned char *px = &fb[(y * fb_w + x) * 4];
            px[0] = (unsigned char)r;
            px[1] = (unsigned char)g;
            px[2] = (unsigned char)b;
            px[3] = 255;
        }
    }
}

/* Projects a quad's 4 world-space corners and fills it, UNLESS any
 * corner falls behind the near plane (reject-not-clip, see this
 * section's own header comment). */
static void draw_quad_3d(unsigned char *fb, int fb_w, int fb_h,
                          double wx0, double wy0, double wz0,
                          double wx1, double wy1, double wz1,
                          double wx2, double wy2, double wz2,
                          double wx3, double wy3, double wz3,
                          double cam_x, double cam_y, double cam_z, double pitch, double focal,
                          int screen_cx, int screen_cy, double scale,
                          int r, int g, int b) {
    int xs[4], ys[4];
    double z2[4];
    project_3d(wx0, wy0, wz0, cam_x, cam_y, cam_z, pitch, focal, screen_cx, screen_cy, scale, &xs[0], &ys[0], &z2[0]);
    project_3d(wx1, wy1, wz1, cam_x, cam_y, cam_z, pitch, focal, screen_cx, screen_cy, scale, &xs[1], &ys[1], &z2[1]);
    project_3d(wx2, wy2, wz2, cam_x, cam_y, cam_z, pitch, focal, screen_cx, screen_cy, scale, &xs[2], &ys[2], &z2[2]);
    project_3d(wx3, wy3, wz3, cam_x, cam_y, cam_z, pitch, focal, screen_cx, screen_cy, scale, &xs[3], &ys[3], &z2[3]);
    for (int i = 0; i < 4; i++) if (z2[i] <= NEAR_PLANE_3D) return;
    fill_quad_3d(fb, fb_w, fb_h, xs, ys, r, g, b);
}

/* Registry `walkable` field (glyph|id|name|walkable|rgb_top|unicode|
 * rgb_top_emoji) - determines whether a tile is drawn as an extruded
 * wall box or a flat floor quad. Duplicated narrowly (matching this
 * project's own "no shared headers" convention, same shape as
 * glyph_rgb_top() just above) rather than shared. out_found (may be
 * NULL) reports whether the glyph actually matched a row in this one
 * registry, so callers checking MULTIPLE registries (see
 * terrain_or_furniture_walkable() below) can tell "genuinely walkable"
 * apart from "not in this file at all, walkable=1 is just this
 * function's own default." */
static int glyph_walkable_3d_ex(const char *registry_rel_path, char glyph, int *out_found) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/%s", project_root, registry_rel_path);
    FILE *f = fopen(path, "r");
    if (out_found) *out_found = 0;
    if (!f) return 1;
    char line[MAX_LINE];
    int walkable = 1;
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '\n' || (line[0] == '#' && line[1] != '|')) continue;
        if (line[0] != glyph) continue;
        line[strcspn(line, "\n")] = '\0';
        char *p = strchr(line, '|');
        if (!p) continue;
        p = strchr(p + 1, '|');
        if (!p) continue;
        p = strchr(p + 1, '|');
        if (!p) continue;
        walkable = atoi(p + 1);
        if (out_found) *out_found = 1;
        break;
    }
    fclose(f);
    return walkable;
}

/* REAL BUG FIX: the 3D ray marcher's wall/floor split used to call
 * glyph_walkable_3d() with ONLY the terrain registry hardcoded - so a
 * furniture glyph (table/counter/sink/crate, none of which are IN
 * terrain_types.txt at all) always fell through to this function's own
 * "not found -> walkable=1" default, regardless of furniture_types.txt
 * marking them walkable=0. Blocking furniture never reached the wall-
 * AABB extrusion path because of this - it just rendered as a flat
 * floor-color quad despite already having real voxels_8.csv data ready
 * (f_table/f_counter/f_sink/f_crate). Terrain and furniture glyphs
 * never overlap (see furniture_types.txt's own header comment), so
 * checking terrain first and falling back to furniture only when the
 * terrain registry doesn't recognize the glyph is unambiguous. */
static int terrain_or_furniture_walkable(char glyph) {
    int found;
    int w = glyph_walkable_3d_ex("pieces/registry/terrain/terrain_types.txt", glyph, &found);
    if (found) return w;
    return glyph_walkable_3d_ex("pieces/registry/furniture/furniture_types.txt", glyph, &found);
}

/* REAL RAY-MARCHED WALL RENDERING (2026-07-17, direct instruction after
 * live feedback: "'123' hotkeys in gl seem to be doing something other
 * than changing the pov" [root-caused as camera_mode 2 and 3 sharing
 * IDENTICAL pitch/cam_y_off/cam_z_off presets below - the hotkey/relay
 * chain itself was verified correct via a real playable-interface test
 * before concluding this] "and they are also not rendering all sides,
 * u should use the same ray marching formula wraith-alpha piececraft-
 * 3d-wraith is using"). Ported from wraith_rgb_daemon.c's
 * raymarch_tile_grid()/ray_aabb_hit()/sample_voxel_pixel() (confirmed
 * via direct, full read - not excerpted), replacing the v1 quad
 * renderer's painter's-algorithm + "one hardcoded camera-facing side"
 * simplification named in this file's own original header comment. A
 * DDA walk over the map's real (col,row) grid, testing a full 3D AABB
 * per occupied cell in near-to-far order, gives correct occlusion AND
 * correct per-camera-angle face visibility for free (the first hit
 * found along each pixel's ray is guaranteed nearest) - unlike the old
 * approach, which only ever drew ONE fixed side regardless of where
 * the camera actually was.
 *
 * Frame convention differs from the reference in one deliberate way:
 * mutaclsym has no mouse-orbit (the reference's g_cam_yaw/
 * unrotate_by_yaw(), rotating the CAMERA around a pivot) - instead this
 * project already yaw-rotates WORLD geometry by the hero's own `facing`
 * before projecting (see yaw_rotate()/facing_to_yaw() above), keeping
 * the camera itself always at its natural map-space position. That
 * means, unlike the reference (which must first unrotate the camera's
 * OWN position back into the map's original frame before it can DDA-
 * walk real (col,row) indices), this camera's (cam_x,cam_y,cam_z) is
 * ALREADY expressed in the real, unrotated map frame the DDA needs -
 * only the per-pixel RAY DIRECTION needs un-yawing (rotating by -yaw
 * instead of +yaw), which yaw_rotate() already does unchanged, called
 * here with pivot (0,0) since rotating a direction/difference vector
 * needs no pivot (the same reasoning that lets the reference's own
 * unrotate_by_yaw() cancel its pivot when subtracting two unrotated
 * points to get a direction). */

/* Ray/AABB slab test, ported verbatim from wraith_rgb_daemon.c's
 * ray_aabb_hit() (pure math, no project-specific dependencies to
 * adapt). Returns the nearest entry t>=0 and which face was hit: 0/1 =
 * -X/+X, 2/3 = -Y/+Y (3 = top), 4/5 = -Z/+Z. */
static int ray_aabb_hit_3d(double ox, double oy, double oz, double dx, double dy, double dz,
                            double bx0, double bx1, double by0, double by1, double bz0, double bz1,
                            double *out_t, int *out_face) {
    double tmin = -1e18, tmax = 1e18;
    int face = -1;

    if (fabs(dx) < 1e-12) {
        if (ox < bx0 || ox > bx1) return 0;
    } else {
        double t0 = (bx0 - ox) / dx, t1 = (bx1 - ox) / dx;
        int f0 = 0;
        if (t0 > t1) { double t = t0; t0 = t1; t1 = t; f0 = 1; }
        if (t0 > tmin) { tmin = t0; face = f0; }
        if (t1 < tmax) tmax = t1;
        if (tmin > tmax) return 0;
    }
    if (fabs(dy) < 1e-12) {
        if (oy < by0 || oy > by1) return 0;
    } else {
        double t0 = (by0 - oy) / dy, t1 = (by1 - oy) / dy;
        int f0 = 2;
        if (t0 > t1) { double t = t0; t0 = t1; t1 = t; f0 = 3; }
        if (t0 > tmin) { tmin = t0; face = f0; }
        if (t1 < tmax) tmax = t1;
        if (tmin > tmax) return 0;
    }
    if (fabs(dz) < 1e-12) {
        if (oz < bz0 || oz > bz1) return 0;
    } else {
        double t0 = (bz0 - oz) / dz, t1 = (bz1 - oz) / dz;
        int f0 = 4;
        if (t0 > t1) { double t = t0; t0 = t1; t1 = t; f0 = 5; }
        if (t0 > tmin) { tmin = t0; face = f0; }
        if (t1 < tmax) tmax = t1;
        if (tmin > tmax) return 0;
    }
    if (tmax < 0.0) return 0;
    if (tmin < 0.0) { tmin = 0.0; face = -1; }
    *out_t = tmin;
    if (out_face) *out_face = face;
    return 1;
}

/* Small persistent cache of loaded voxels_8.csv assets (resolution-8,
 * per-face emoji texture generated by pieces/system/emoji_extract's
 * own emoji_gen_atlas.+x/emoji_xtract.+x tools - per direct
 * instruction, "8x8x8" means an 8x8 relief texture reused on whichever
 * face the ray hits, matching the reference's own real voxel_source
 * convention exactly, confirmed via direct read of
 * sample_voxel_pixel() - NOT a literal solid NxNxN volumetric grid,
 * since that isn't what the reference itself does either). Separate
 * from load_emoji_voxels()'s own TILE_PX=16 cache above (2D top-down
 * emoji blitting) - deliberately not unified, to avoid any risk of
 * regressing that already-working, already-verified path. Keyed by
 * path, lives for this process's lifetime (one-shot op, so that's just
 * "this single frame's render"). */
#define MAX_VOXEL8_CACHE 64
typedef struct {
    char path[PATH_BUF];
    int resolution;
    int count;
    unsigned char pixels[64][4];
    int loaded;
} Voxel8Cache;
static Voxel8Cache g_voxel8_cache[MAX_VOXEL8_CACHE];
static int g_voxel8_cache_count = 0;

static Voxel8Cache *get_voxel8_cached(const char *path) {
    for (int i = 0; i < g_voxel8_cache_count; i++) {
        if (strcmp(g_voxel8_cache[i].path, path) == 0) return &g_voxel8_cache[i];
    }
    if (g_voxel8_cache_count >= MAX_VOXEL8_CACHE) return NULL;
    Voxel8Cache *c = &g_voxel8_cache[g_voxel8_cache_count++];
    snprintf(c->path, sizeof(c->path), "%s", path);
    c->count = 0; c->resolution = 0; c->loaded = 0;
    FILE *f = fopen(path, "r");
    if (!f) return c; /* cached as "not loaded" so we don't retry every hit */
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f) && c->count < 64) {
        int r, g, b, a;
        if (line[0] == '#') { sscanf(line, "# resolution=%d", &c->resolution); continue; }
        if (sscanf(line, "%d,%d,%d,%d", &r, &g, &b, &a) == 4) {
            c->pixels[c->count][0] = (unsigned char)r;
            c->pixels[c->count][1] = (unsigned char)g;
            c->pixels[c->count][2] = (unsigned char)b;
            c->pixels[c->count][3] = (unsigned char)a;
            c->count++;
        }
    }
    fclose(f);
    if (c->resolution <= 0) for (c->resolution = 1; c->resolution * c->resolution < c->count; c->resolution++) {}
    c->loaded = (c->resolution > 0 && c->count > 0);
    return c;
}

/* Samples a voxels_8.csv at normalized (u,v) in [0,1) - ported from
 * sample_voxel_pixel(). Returns 1 and fills out_rgb if that pixel is
 * occupied (alpha>10); 0 if transparent or the CSV couldn't be loaded
 * (caller falls back to the tile's own flat, darkened rgb_top). */
static int sample_voxel8_pixel(const char *path, double u, double v, unsigned char out_rgb[3]) {
    Voxel8Cache *c = get_voxel8_cached(path);
    if (!c || !c->loaded) return 0;
    if (u < 0.0) u = 0.0;
    if (u > 0.999999) u = 0.999999;
    if (v < 0.0) v = 0.0;
    if (v > 0.999999) v = 0.999999;
    int col = (int)(u * c->resolution), row = (int)(v * c->resolution);
    int idx = row * c->resolution + col;
    if (idx < 0 || idx >= c->count) return 0;
    if (c->pixels[idx][3] <= 10) return 0;
    out_rgb[0] = c->pixels[idx][0];
    out_rgb[1] = c->pixels[idx][1];
    out_rgb[2] = c->pixels[idx][2];
    return 1;
}

/* Maps a ray/AABB hit point to a normalized (u,v) on whichever face was
 * hit, given that box's own world-space bounds - shared by both wall
 * and entity extrusion below so both extrude/texture identically.
 * Side faces (0/1 = -X/+X, 4/5 = -Z/+Z) wrap the texture around the
 * box's vertical side, v=0 at the top; the top face (3) and bottom (2)
 * are looked at from directly above/below, u=x fraction, v=z fraction -
 * this is the "extrude forward like the other emojis" fix: previously
 * only the side faces sampled a texture at all, so tops/entities showed
 * only a flat fallback color. */
static void box_face_uv(double wx, double wy, double wz,
                         double bx0, double bx1, double by0, double by1, double bz0, double bz1,
                         int face, double *u, double *v) {
    if (face == 2 || face == 3) {
        *u = (wx - bx0) / (bx1 - bx0);
        *v = (wz - bz0) / (bz1 - bz0);
    } else {
        *u = (face == 4 || face == 5) ? (wx - bx0) / (bx1 - bx0) : (wz - bz0) / (bz1 - bz0);
        *v = 1.0 - (wy - by0) / (by1 - by0);
    }
}

/* Per-glyph metadata, cached ONCE per raymarch_walls_3d() call (same
 * shape as the reference's own meta_cache[16]/meta_glyph[16] in
 * raymarch_tile_grid() - a per-call cache, not a global one, so it
 * never goes stale across separate compose_rgb_frame.c invocations)
 * rather than re-reading the registry file from disk on every DDA
 * step of every pixel, which would be the naive/slow way to call
 * glyph_walkable_3d()/glyph_rgb_top() here. */
typedef struct {
    char glyph;
    int walkable;
    int r, g, b;
    char voxel_path[PATH_BUF];
    int has_voxel;
} WallGlyphMeta;

/* Per-pixel DDA ray marcher over the non-walkable ("wall") cells of
 * the map grid, PLUS entity tiles (hero/xlector/monster/item, resolved
 * via cell_asset - see that array's own declaration comment in main()
 * for why it's keyed separately from grid's glyph) - real occlusion,
 * real per-camera-angle face visibility, real emoji texture per visible
 * face, for both. Walls and entities share one DDA walk (rather than
 * two separate passes) so the ray's near-to-far cell order alone
 * decides which one occludes the other, with no separate depth-buffer
 * cross-check needed. Ported from wraith_rgb_daemon.c's
 * raymarch_tile_grid() (confirmed via direct, full read); see this
 * section's own header comment for the yaw-frame adaptation. Draws
 * directly into fb - called AFTER the floor pass in render_3d_view()
 * below, so a hit here always correctly overwrites/occludes whatever
 * floor color was drawn at the same pixel. */
static void raymarch_walls_3d(unsigned char *fb, int fb_w, int fb_h,
                               char grid[MAX_MAP_H][MAX_MAP_W + 1], int rows, int map_w,
                               char cell_asset[MAX_MAP_H][MAX_MAP_W][ASSET_ID_BUF],
                               char cell_is_entity[MAX_MAP_H][MAX_MAP_W],
                               double cam_x, double cam_y, double cam_z, double pitch, double yaw,
                               double focal, int screen_cx, int screen_cy, double scale,
                               int col_lo, int col_hi, int row_lo, int row_hi) {
    (void)map_w;
    (void)rows;
    WallGlyphMeta meta_cache[16];
    int meta_count = 0;
    static int row_len[MAX_MAP_H];
    for (int row = row_lo; row <= row_hi; row++) row_len[row] = (int)strlen(grid[row]);

    double cpitch = cos(pitch * M_PI / 180.0), spitch = sin(pitch * M_PI / 180.0);

    for (int sy = 0; sy < fb_h; sy++) {
        for (int sx = 0; sx < fb_w; sx++) {
            /* Algebraic inverse of project_3d()'s own negated out_x
             * (screen_cx - rx*scale*persp) - see that function's own
             * matching comment. Must flip the same way this pass's
             * rays are cast, or the ray-marched walls would un-mirror
             * independently of the floor pass above and the two would
             * no longer align. */
            double dx_cam = (screen_cx - sx) / (scale * focal);
            double dy_cam = (screen_cy - sy) / (scale * focal);
            double dy_p = dy_cam * cpitch + spitch;
            double dz_p = -dy_cam * spitch + cpitch;
            double dir_x, dir_z;
            yaw_rotate(dx_cam, dz_p, 0.0, 0.0, -yaw, &dir_x, &dir_z);
            double dir_y = dy_p;
            double ox = cam_x, oy = cam_y, oz = cam_z;

            int col = (int)floor(ox), row = (int)floor(oz);
            int step_col = (dir_x > 0.0) ? 1 : (dir_x < 0.0 ? -1 : 0);
            int step_row = (dir_z > 0.0) ? 1 : (dir_z < 0.0 ? -1 : 0);
            double t_delta_x = (dir_x != 0.0) ? fabs(1.0 / dir_x) : 1e18;
            double t_delta_z = (dir_z != 0.0) ? fabs(1.0 / dir_z) : 1e18;
            double t_max_x = (dir_x > 0.0) ? ((col + 1) - ox) / dir_x : (dir_x < 0.0 ? (col - ox) / dir_x : 1e18);
            double t_max_z = (dir_z > 0.0) ? ((row + 1) - oz) / dir_z : (dir_z < 0.0 ? (row - oz) / dir_z : 1e18);

            /* hit_kind: 0 = wall/blocking-furniture (full box, per-glyph
             * asset), 1 = entity (hero/xlector/monster/item - inset box,
             * per-instance asset via cell_asset), 2 = decor (walkable
             * chair/bed/stairs-down - medium box, per-glyph asset same as
             * walls). */
            int hit = 0, hit_face = -1, hit_col = -1, hit_row = -1, hit_kind = 0;
            double hit_t = 0.0;
            int max_steps = (col_hi - col_lo + row_hi - row_lo) * 2 + 8;

            for (int steps = 0; steps < max_steps; steps++) {
                if (col >= col_lo && col <= col_hi && row >= row_lo && row <= row_hi) {
                    int len = row_len[row];
                    char glyph = (col < len) ? grid[row][col] : ' ';
                    if (glyph != ' ' && glyph != '\0') {
                        WallGlyphMeta *m = NULL;
                        for (int mi = 0; mi < meta_count; mi++) {
                            if (meta_cache[mi].glyph == glyph) { m = &meta_cache[mi]; break; }
                        }
                        WallGlyphMeta overflow_meta;
                        if (!m) {
                            m = (meta_count < 16) ? &meta_cache[meta_count++] : &overflow_meta;
                            m->glyph = glyph;
                            m->walkable = terrain_or_furniture_walkable(glyph);
                            if (!glyph_rgb_top("pieces/registry/terrain/terrain_types.txt", glyph, 0, &m->r, &m->g, &m->b) &&
                                !glyph_rgb_top("pieces/registry/furniture/furniture_types.txt", glyph, 0, &m->r, &m->g, &m->b)) {
                                glyph_fallback_rgb(glyph, &m->r, &m->g, &m->b);
                            }
                            char asset_id[64];
                            terrain_or_furniture_asset_id(glyph, asset_id, sizeof(asset_id));
                            if (asset_id[0]) {
                                snprintf(m->voxel_path, sizeof(m->voxel_path), "%s/pieces/registry/emoji_assets/%s/voxels_8.csv", project_root, asset_id);
                                m->has_voxel = 1;
                            } else {
                                m->has_voxel = 0;
                            }
                        }
                        if (!m->walkable) {
                            double bx0 = (double)col, bx1 = bx0 + 1.0;
                            double bz0 = (double)row, bz1 = bz0 + 1.0;
                            double t; int face;
                            if (ray_aabb_hit_3d(ox, oy, oz, dir_x, dir_y, dir_z, bx0, bx1, 0.0, 1.0, bz0, bz1, &t, &face)) {
                                hit = 1; hit_t = t; hit_face = face; hit_col = col; hit_row = row; hit_kind = 0;
                                break;
                            }
                        } else if (cell_is_entity[row][col]) {
                            /* Entity tile (hero/xlector/monster/item) - a
                             * smaller, shorter box than a wall's so it
                             * reads as a standing object on its tile
                             * rather than a second wall block. */
                            double bx0 = (double)col + ENTITY_MARGIN, bx1 = (double)col + 1.0 - ENTITY_MARGIN;
                            double bz0 = (double)row + ENTITY_MARGIN, bz1 = (double)row + 1.0 - ENTITY_MARGIN;
                            double t; int face;
                            if (ray_aabb_hit_3d(ox, oy, oz, dir_x, dir_y, dir_z, bx0, bx1, 0.0, ENTITY_HEIGHT, bz0, bz1, &t, &face)) {
                                hit = 1; hit_t = t; hit_face = face; hit_col = col; hit_row = row; hit_kind = 1;
                                break;
                            }
                        } else if (glyph_is_decor_object(glyph)) {
                            /* Walkable furniture/feature (chair/bed/stairs
                             * down) - real, specific emoji, not bare
                             * ground, so it still gets its own box (see
                             * DECOR_MARGIN/HEIGHT's own comment). */
                            double bx0 = (double)col + DECOR_MARGIN, bx1 = (double)col + 1.0 - DECOR_MARGIN;
                            double bz0 = (double)row + DECOR_MARGIN, bz1 = (double)row + 1.0 - DECOR_MARGIN;
                            double t; int face;
                            if (ray_aabb_hit_3d(ox, oy, oz, dir_x, dir_y, dir_z, bx0, bx1, 0.0, DECOR_HEIGHT, bz0, bz1, &t, &face)) {
                                hit = 1; hit_t = t; hit_face = face; hit_col = col; hit_row = row; hit_kind = 2;
                                break;
                            }
                        }
                    }
                }
                if (t_max_x < t_max_z) { col += step_col; t_max_x += t_delta_x; }
                else { row += step_row; t_max_z += t_delta_z; }
                if (col < col_lo - 1 || col > col_hi + 1 || row < row_lo - 1 || row > row_hi + 1) break;
            }

            if (hit) {
                double wx = ox + hit_t * dir_x;
                double wy = oy + hit_t * dir_y;
                double wz = oz + hit_t * dir_z;
                int r, g, b, has_voxel;
                char voxel_path[PATH_BUF];
                double bx0, bx1, by0, by1, bz0, bz1;
                WallGlyphMeta *m = NULL;
                if (hit_kind != 1) {
                    for (int mi = 0; mi < meta_count; mi++) {
                        if (meta_cache[mi].glyph == grid[hit_row][hit_col]) { m = &meta_cache[mi]; break; }
                    }
                    if (!m) continue;
                }
                if (hit_kind == 1) {
                    glyph_fallback_rgb(grid[hit_row][hit_col], &r, &g, &b);
                    snprintf(voxel_path, sizeof(voxel_path), "%s/pieces/registry/emoji_assets/%s/voxels_8.csv",
                             project_root, cell_asset[hit_row][hit_col]);
                    {   Voxel8Cache *ev = get_voxel8_cached(voxel_path);
                        has_voxel = (ev && ev->loaded);
                        static int dbg_ent_cnt = 0;
                        if (dbg_ent_cnt < 40) {
                            fprintf(stderr, "DBG_ENTITY[%d] glyph=%c asset=%s voxel=%s has_voxel=%d cache_slots=%d/%d\n",
                                    dbg_ent_cnt, grid[hit_row][hit_col], cell_asset[hit_row][hit_col],
                                    voxel_path, has_voxel, g_voxel8_cache_count, MAX_VOXEL8_CACHE);
                            dbg_ent_cnt++;
                        }
                    }
                    bx0 = (double)hit_col + ENTITY_MARGIN; bx1 = (double)hit_col + 1.0 - ENTITY_MARGIN;
                    bz0 = (double)hit_row + ENTITY_MARGIN; bz1 = (double)hit_row + 1.0 - ENTITY_MARGIN;
                    by0 = 0.0; by1 = ENTITY_HEIGHT;
                } else if (hit_kind == 2) {
                    r = m->r; g = m->g; b = m->b;
                    has_voxel = m->has_voxel;
                    snprintf(voxel_path, sizeof(voxel_path), "%s", m->voxel_path);
                    bx0 = (double)hit_col + DECOR_MARGIN; bx1 = (double)hit_col + 1.0 - DECOR_MARGIN;
                    bz0 = (double)hit_row + DECOR_MARGIN; bz1 = (double)hit_row + 1.0 - DECOR_MARGIN;
                    by0 = 0.0; by1 = DECOR_HEIGHT;
                } else {
                    r = m->r; g = m->g; b = m->b;
                    has_voxel = m->has_voxel;
                    snprintf(voxel_path, sizeof(voxel_path), "%s", m->voxel_path);
                    bx0 = (double)hit_col; bx1 = bx0 + 1.0;
                    bz0 = (double)hit_row; bz1 = bz0 + 1.0;
                    by0 = 0.0; by1 = 1.0;
                }
                /* Side faces: darken as the flat fallback, then try a
                 * real emoji texture on top of it - per direct
                 * instruction, "all of the current 3d blocks should be
                 * emoji." Top face (3) now also samples the texture
                 * (looking straight down at it) instead of staying a
                 * flat, undarkened fallback color - REAL BUG FIX: wall
                 * tops previously fell through this block untextured. */
                if (hit_face != 3) { r = r * 3 / 4; g = g * 3 / 4; b = b * 3 / 4; }
                if (has_voxel) {
                    double u, v;
                    box_face_uv(wx, wy, wz, bx0, bx1, by0, by1, bz0, bz1, hit_face, &u, &v);
                    unsigned char sampled[3];
                    if (sample_voxel8_pixel(voxel_path, u, v, sampled)) {
                        r = sampled[0]; g = sampled[1]; b = sampled[2];
                    }
                }
                {
                    unsigned char *px = &fb[(sy * fb_w + sx) * 4];
                    px[0] = (unsigned char)r; px[1] = (unsigned char)g; px[2] = (unsigned char)b; px[3] = 255;
                }
            }
        }
    }
}

/* The 3D view itself. Takes an explicit target buffer/width/height
 * (not the fixed FRAME_W/FRAME_H) so the SAME rendering code serves
 * two real call sites:
 *   - regular "run" mode (main()'s own branch below): draws into the
 *     full framebuf, replacing the flat-tile loop entirely.
 *   - chtpm mode (see this file's own MAP3D_MARKER/overlay-compositing
 *     block in main()): draws into a SEPARATE, map-sized-only overlay
 *     buffer written to its own file, which shared-ops/
 *     chtpm_rgb_render.c (a genuinely project-agnostic daemon with no
 *     game-state awareness) composites into the exact screen
 *     rectangle where it would otherwise have font-rasterized the
 *     embedded ${game_map} text - real, direct instruction (2026-07-17)
 *     to wire 3D into chtpm mode specifically, since "the normal mode
 *     will be deprecated once this is proven".
 * World space: col->X, row->Z (depth), extrude height->Y (up) -
 * matches the reference's own tile_zmap convention. */

/* Debug reference cube: 1x1x1 cube at (-0.5,-0.5,-0.5) with face colors
 * from ^.cube-legend_v1.2.txt, shaded with directional light + dark edge
 * outlines so each face is clearly distinguishable (voxel look):
 *   X+ (right) = red,  X- (left) = blue
 *   Y+ (top) = green,   Y- (bottom) = brown
 *   Z+ (back) = white,  Z- (front) = dark
 * Cube corners span (-1,-1,-1) to (0,0,0). Yaw-rotated like floor tiles.
 *
 * Directional light from upper-right-front: light = normalize(1, 1.5, -1).
 * Per-face brightness = 0.35 + 0.65 * max(0, dot(normal, light)).
 * Edge strips drawn along all 12 cube edges using inset corners
 * (0.04wu toward center) so edges sit between faces. */
static void draw_debug_cube(unsigned char *fb, int fb_w, int fb_h,
                             double cam_x, double cam_y, double cam_z,
                             double pitch, double yaw, double focal,
                             int screen_cx, int screen_cy, double scale,
                             double pivot_x, double pivot_z) {
    double c[8][3] = {
        {-1,-1,-1}, { 0,-1,-1}, {-1, 0,-1}, { 0, 0,-1},
        {-1,-1, 0}, { 0,-1, 0}, {-1, 0, 0}, { 0, 0, 0},
    };
    double rx[8], rz[8];
    for (int i = 0; i < 8; i++)
        yaw_rotate(c[i][0], c[i][2], pivot_x, pivot_z, yaw, &rx[i], &rz[i]);
    double cy[8]; for (int i = 0; i < 8; i++) cy[i] = c[i][1];

    /* Face definitions: 4 corner indices + RGB color (pre-lit) */
    typedef struct { int ci[4]; int r, g, b; double nx, ny, nz; } face_t;
    face_t faces[6] = {
        {{2,3,7,6},   0,180,  0,  0, 1, 0},  /* Y+ top green */
        {{0,1,5,4}, 139, 90, 43,  0,-1, 0},  /* Y- bottom brown */
        {{0,1,3,2}, 240,240,240,  0, 0, 1},  /* Z+ back white */
        {{4,5,7,6},  30, 30, 30,  0, 0,-1},  /* Z- front dark */
        {{1,5,7,3}, 200,  0,  0,  1, 0, 0},  /* X+ right red */
        {{0,4,6,2},   0,  0,200, -1, 0, 0},  /* X- left blue */
    };
    /* Light direction: normalize(1, 1.5, -1) = (0.480, 0.720, -0.480) */
    double lx = 0.480, ly = 0.720, lz = -0.480;

    /* Compute face center distance from camera and sort back-to-front */
    double dist[6];
    for (int i = 0; i < 6; i++) {
        double fcx = 0, fcy = 0, fcz = 0;
        for (int j = 0; j < 4; j++) {
            fcx += rx[faces[i].ci[j]];
            fcy += cy[faces[i].ci[j]];
            fcz += rz[faces[i].ci[j]];
        }
        fcx /= 4.0; fcy /= 4.0; fcz /= 4.0;
        double dx = fcx - cam_x, dy = fcy - cam_y, dz = fcz - cam_z;
        dist[i] = dx*dx + dy*dy + dz*dz;
    }
    /* Simple insertion sort descending (only 6 elements) */
    int order[6] = {0,1,2,3,4,5};
    for (int i = 1; i < 6; i++) {
        int key = order[i];
        double kd = dist[key];
        int j = i - 1;
        while (j >= 0 && dist[order[j]] < kd) {
            order[j+1] = order[j];
            j--;
        }
        order[j+1] = key;
    }

    /* Draw farthest-first (painter's algorithm: last draw wins) */
    for (int fi = 0; fi < 6; fi++) {
        int i = order[fi];
        double dot = faces[i].nx*lx + faces[i].ny*ly + faces[i].nz*lz;
        double br = 0.35 + 0.65 * (dot > 0 ? dot : 0);
        int cr = (int)(faces[i].r * br);
        int cg = (int)(faces[i].g * br);
        int cb = (int)(faces[i].b * br);
        int *ci = faces[i].ci;
        draw_quad_3d(fb, fb_w, fb_h,
            rx[ci[0]], cy[ci[0]], rz[ci[0]],
            rx[ci[1]], cy[ci[1]], rz[ci[1]],
            rx[ci[2]], cy[ci[2]], rz[ci[2]],
            rx[ci[3]], cy[ci[3]], rz[ci[3]],
            cam_x, cam_y, cam_z, pitch, focal, screen_cx, screen_cy, scale,
            cr, cg, cb);
    }

}

static void render_3d_view(unsigned char *fb, int fb_w, int fb_h,
                            char grid[MAX_MAP_H][MAX_MAP_W + 1], int rows, int map_w,
                            char cell_asset[MAX_MAP_H][MAX_MAP_W][ASSET_ID_BUF],
                            char cell_is_entity[MAX_MAP_H][MAX_MAP_W],
                            int px, int py, int camera_mode, int facing,
                            double cam_pan_x, double cam_pan_y, double cam_pan_z,
                            double cam_yaw, double cam_pitch, int cam_z_level) {
    double focal = 1.0;
    double scale = 210.0;
    int screen_cx = fb_w / 2;
    int screen_cy = fb_h / 2;
    double pitch, cam_y_off, cam_z_off, cam_x = 0.0, cam_z = 0.0;
    double yaw = (camera_mode == 4) ? 180.0 : facing_to_yaw(facing);

    /* Camera presets - same shape as real piececraft-wraith's own
     * camera_mode switch (1/2/3), values chosen for mutaclsym's own
     * tile scale rather than copied verbatim (that reference's own
     * numbers are tuned to ITS OWN world-unit scale, not this
     * project's).
     *
     * REAL BUG FIX (2026-07-17, direct live feedback: "'123' hotkeys in
     * gl seem to be doing something other than changing the pov"): a
     * real playable-interface test (injecting real KEY_PRESSED events
     * through the actual chtpm_parser_pal/choice.c relay chain, not
     * just editing hero/state.txt directly) proved camera_mode DOES
     * correctly change on every '1'/'2'/'3' keypress - the relay chain
     * itself was never the bug. The real cause: case 2 and case 3
     * below used to be IDENTICAL values, so switching between 3rd-
     * person and free-camera produced no visible change at all until
     * the player also panned with wasd/xz - indistinguishable from
     * "the hotkey did nothing." Now genuinely distinct. Values stay
     * modest, conservative increases from the previously-verified
     * working POV1/2 numbers (not copied from the reference, which is
     * tuned to a different world-unit scale) - re-verified via
     * dump_rgb_png.+x, not just assumed safe (see the NAMED TUNING
     * NOTE below on why pitch/cam_y_off/scale are coupled and a real
     * black-frame bug already happened once from changing them
     * carelessly). 3 (free camera) starts at a neutral, closer-in
     * position (same as 1st person) precisely so a pan away from it
     * reads as "the camera is now free," not "point 3 looks like point
     * 2." */
    switch (camera_mode) {
        case 1: /* first person: at hero eye level, yaw/pitch user-controlled via q/e/r/t */
            pitch = cam_pitch;
            cam_y_off = 0.9;
            cam_z_off = 0.0;
            yaw = cam_yaw;
            break;
        case 2: /* third person: pulled back/higher behind hero, yaw/pitch user-adjustable.
                  * cam_z_off is negative so camera is behind hero (opposite facing dir). */
            pitch = MODE2_PITCH;
            cam_y_off = MODE2_Y_OFF;
            cam_z_off = MODE2_Z_OFF;
            yaw = cam_yaw;
            break;
        case 3: /* free roam: starts at bird's eye (same as mode 4),
                  * then user takes full control of yaw/pitch/pan. */
            pitch = cam_pitch;
            cam_y_off = 12.0 + cam_z_level * 2.0;
            cam_z_off = 0.0;
            yaw = cam_yaw;
            break;
        case 4: /* bird's eye / board game: high above, looking straight down
                  * COORDINATE SYSTEM (from ^.cube-legend_v1.2.txt):
                  *   X = col (left/right), Y = height (up/down), Z = row (forward/back)
                  * pitch = -90°: camera looks straight down the -Y axis.
                  * cos(-90°)=0, sin(-90°)=-1 → y2=rz, z2=-ry.
                  * Floor at y=0, camera at y=12 → ry=-12, z2=12 (in front) ✓
                  *
                  * YAW = 180° (NOT 0°): project_3d() negates rx (a fix for
                  * modes 1-3's left-handed Z-down world). At yaw=0 this
                  * flips X and Y relative to ASCII. At yaw=180°, yaw_rotate
                  * mirrors the world, and the negation double-cancels →
                  * correct orientation matching the 2D ASCII view. */
            pitch = -90.0;
            cam_y_off = 12.0 + cam_z_level * 2.0;
            cam_z_off = 0.0;
            yaw = 180.0;
            /* absolute position, not hero-relative */
            cam_x = cam_pan_x;
            cam_z = cam_pan_y;
            break;
        default: pitch = cam_pitch; cam_y_off = 0.9; cam_z_off = 0.0; yaw = cam_yaw; break;
    }

    /* Camera world position: hero's own (col,row) is the pivot/base
     * position for POV 1/2/3; POV 3 additionally offsets by the free-cam
     * pan the player has applied (see ops/camera_control.c). cam_z_off
     * is applied OPPOSITE the facing direction (a proper "behind the
     * player" 3rd-person offset) by rotating the offset vector by yaw
     * before adding it in. Negative cam_z_off places the camera behind
     * the hero regardless of yaw.
     * POV 4 (bird's eye) uses absolute map coordinates, not hero-relative. */
    if (camera_mode == 4) {
        cam_x = cam_pan_x;
        cam_z = cam_pan_y;
    } else {
        double base_x = (double)px;
        double base_z = (double)py;
        double off_x, off_z;
        yaw_rotate(0.0, cam_z_off, 0.0, 0.0, yaw, &off_x, &off_z);
        cam_x = base_x + off_x;
        cam_z = base_z + off_z;
        if (camera_mode == 3) {
            cam_x += cam_pan_x;
            cam_z += cam_pan_z;
        }
    }
    double cam_y = cam_y_off;

    int view_radius = (camera_mode == 4) ? VIEW_3D_RADIUS * 2 : VIEW_3D_RADIUS;
    int col_lo = px - view_radius, col_hi = px + view_radius;
    int row_lo = py - view_radius, row_hi = py + view_radius;
    if (col_lo < 0) col_lo = 0;
    if (row_lo < 0) row_lo = 0;
    if (col_hi >= map_w) col_hi = map_w - 1;
    if (row_hi >= rows) row_hi = rows - 1;

    /* Skybox gradient: vertical 5-stop gradient filling the whole
     * framebuffer before geometry is drawn. Lets the user tell up
     * from down and by how much from the color alone:
     *   top (zenith)  = deep indigo  (0, 0, 50)
     *   25%           = steel blue   (40, 80, 150)
     *   50% (horizon) = pale gray    (150, 165, 180)
     *   75%           = olive brown  (80, 65, 35)
     *   bottom (nadir)= dark earth   (25, 15, 8) */
    {
        static const int sky[5][3] = {
            {  0,  0,  50},
            { 40, 80, 150},
            {150, 165, 180},
            { 80, 65,  35},
            { 25, 15,   8},
        };
        for (int sy = 0; sy < fb_h; sy++) {
            double t = (double)sy / (double)(fb_h - 1);
            double seg = t * 4.0;
            int idx = (int)seg;
            if (idx >= 4) idx = 3;
            double frac = seg - idx;
            int r = (int)(sky[idx][0] + frac * (sky[idx+1][0] - sky[idx][0]));
            int g = (int)(sky[idx][1] + frac * (sky[idx+1][1] - sky[idx][1]));
            int b = (int)(sky[idx][2] + frac * (sky[idx+1][2] - sky[idx][2]));
            for (int sx = 0; sx < fb_w; sx++) {
                int off = (sy * fb_w + sx) * 4;
                fb[off+0] = (unsigned char)r;
                fb[off+1] = (unsigned char)g;
                fb[off+2] = (unsigned char)b;
                fb[off+3] = 255;
            }
        }
    }

    /* Pass 1: the floor. Walkable tiles only, drawn as flat y=0 quads -
     * painter's algorithm is fine here (a real, deliberate choice, not
     * an oversight): the ground is a single continuous plane with no
     * per-tile depth variation, so no draw order can make one floor
     * tile wrongly occlude another. Non-walkable ("wall") tiles are
     * fully handled by the real ray marcher below instead now - not
     * drawn here at all. */
    for (int row = row_hi; row >= row_lo; row--) {
        int len = (int)strlen(grid[row]);
        for (int col = col_lo; col <= col_hi; col++) {
            char glyph = (col < len) ? grid[row][col] : ' ';
            if (glyph == ' ' || glyph == '\0') continue;
            if (!terrain_or_furniture_walkable(glyph)) continue;

            int r, g, b;
            if (!glyph_rgb_top("pieces/registry/terrain/terrain_types.txt", glyph, 0, &r, &g, &b) &&
                !glyph_rgb_top("pieces/registry/furniture/furniture_types.txt", glyph, 0, &r, &g, &b)) {
                glyph_fallback_rgb(glyph, &r, &g, &b);
            }

            /* World-space corners for this tile's unit square, yaw-
     * rotated around the camera's own base position so
     * "forward" always renders into the screen regardless of
     * which absolute map direction the hero currently faces. */
            double wx0 = (double)col, wx1 = (double)col + 1.0;
            double wz0 = (double)row, wz1 = (double)row + 1.0;
            double rx0, rz0, rx1, rz1, rx2, rz2, rx3, rz3;
            double pivot_x = cam_x;
            double pivot_z = cam_z;
            yaw_rotate(wx0, wz0, pivot_x, pivot_z, yaw, &rx0, &rz0);
            yaw_rotate(wx1, wz0, pivot_x, pivot_z, yaw, &rx1, &rz1);
            yaw_rotate(wx1, wz1, pivot_x, pivot_z, yaw, &rx2, &rz2);
            yaw_rotate(wx0, wz1, pivot_x, pivot_z, yaw, &rx3, &rz3);

            draw_quad_3d(fb, fb_w, fb_h, rx0, 0.0, rz0, rx1, 0.0, rz1, rx2, 0.0, rz2, rx3, 0.0, rz3,
                         cam_x, cam_y, cam_z, pitch, focal, screen_cx, screen_cy, scale, r, g, b);
        }
    }

    /* Pass 2: walls, ray-marched - see this section's own header
     * comment above render_3d_view() for why cam_x/cam_y/cam_z (already
     * in the map's real, unrotated frame) need no extra unrotation
     * here, only the per-pixel ray direction does. */
    raymarch_walls_3d(fb, fb_w, fb_h, grid, rows, map_w, cell_asset, cell_is_entity, cam_x, cam_y, cam_z, pitch, yaw,
                       focal, screen_cx, screen_cy, scale, col_lo, col_hi, row_lo, row_hi);

    /* Pass 3: debug reference cube at (-0.5,-0.5,-0.5) - coordinate
     * axis verification per ^.cube-legend_v1.2.txt */
    {
        double dbg_pivot_x = cam_x;
        double dbg_pivot_z = cam_z;
        draw_debug_cube(fb, fb_w, fb_h, cam_x, cam_y, cam_z, pitch, yaw,
                        focal, screen_cx, screen_cy, scale,
                        dbg_pivot_x, dbg_pivot_z);
    }
}

int main(void) {
    resolve_root();
    load_glyphs();

    char hero_path[PATH_BUF], out_path[PATH_BUF], receipt_path[PATH_BUF], rgb_pulse_path[PATH_BUF];
    snprintf(hero_path, sizeof(hero_path), "%s/pieces/world_01/map_start/hero/state.txt", project_root);
    snprintf(out_path, sizeof(out_path), "%s/pieces/display/rgb_frame.raw", project_root);
    snprintf(receipt_path, sizeof(receipt_path), "%s/pieces/display/rgb_frame.receipt.txt", project_root);
    /* Real fix ported from shared-ops/chtpm_rgb_render.c's own
     * pulse_rgb_ready() - see that file's own header comment for the
     * full why (matches real wraith_rgb_daemon.c's own pulse_rgb()/
     * rgb_frame_changed.txt two-stage pattern). system/gl_mirror.c now
     * watches ONLY this downstream file, not pieces/display/
     * frame_changed.txt directly - this op growing it here (right
     * after a real, complete write below) keeps that working
     * consistently for regular "run" mode too, not just chtpm mode. */
    snprintf(rgb_pulse_path, sizeof(rgb_pulse_path), "%s/pieces/display/rgb_frame_changed.txt", project_root);

    int px = read_kv_int(hero_path, "pos_x", 0);
    int py = read_kv_int(hero_path, "pos_y", 0);
    int hp = read_kv_int(hero_path, "hp", 100);
    int hunger = read_kv_int(hero_path, "hunger", 0);
    int thirst = read_kv_int(hero_path, "thirst", 0);
    int stamina = read_kv_int(hero_path, "stamina", 100);
    int action_cursor = read_kv_int(hero_path, "action_cursor", -1);
    int interact_mode = read_kv_int(hero_path, "interact_mode", 0);
    int emoji_mode = read_kv_int(hero_path, "emoji_mode", 1);
    int xlector_x = read_kv_int(hero_path, "xlector_pos_x", px);
    int xlector_y = read_kv_int(hero_path, "xlector_pos_y", py);
    int render_mode = read_kv_int(hero_path, "render_mode", 0);
    int camera_mode = read_kv_int(hero_path, "camera_mode", 1);
    int facing = read_kv_int(hero_path, "facing", 1002);
    double cam_pan_x = read_kv_double(hero_path, "cam_pan_x", 0.0);
    double cam_pan_y = read_kv_double(hero_path, "cam_pan_y", 0.0);
    double cam_pan_z = read_kv_double(hero_path, "cam_pan_z", 0.0);
    double cam_yaw = read_kv_double(hero_path, "cam_yaw", 0.0);
    double cam_pitch = read_kv_double(hero_path, "cam_pitch", 6.0);
    int cam_z_level = read_kv_int(hero_path, "cam_z_level", 0);
    int hero_z = read_kv_int(hero_path, "hero_z", 0);
    (void)hero_z; /* used in 3D rendering once z-level visual support is added */

    /* CHTPM MODE 3D OVERLAY - real, direct instruction (2026-07-17):
     * chtpm mode's own GL rendering (shared-ops/chtpm_rgb_render.c) is
     * a genuinely project-agnostic daemon that font-rasterizes
     * current_frame.txt verbatim - it was NEVER called in chtpm mode
     * before this (main_loop_chtpm.pal's own header comment explains
     * why it was removed originally: it used to overwrite chtpm's own
     * chrome). Rather than have this op fight chtpm_rgb_render.c for
     * the SAME rgb_frame.raw file (a real, confirmed race class this
     * whole project family has hit before with the two-stage-pulse
     * bug), this op instead writes a SEPARATE, map-sized-only overlay
     * file when running under chtpm mode, and chtpm_rgb_render.c
     * composites it into the exact rectangle where it would otherwise
     * have font-rasterized ops/compose_frame.c's own MAP3D_MARKER
     * sentinel line + the embedded map rows that follow it (see that
     * file's own MAP3D_MARKER handling). */
    char player_app_state_path[PATH_BUF];
    snprintf(player_app_state_path, sizeof(player_app_state_path), "%s/pieces/apps/player_app/state.txt", project_root);
    char module_path_check[PATH_BUF];
    read_kv_str(player_app_state_path, "module_path", module_path_check, sizeof(module_path_check), "");
    int is_chtpm_mode = (strstr(module_path_check, "main_loop_chtpm") != NULL);

    if (is_chtpm_mode) {
        if (render_mode != 1) {
            /* 2D chtpm mode: chtpm_rgb_render.c's own text-rasterization
             * of current_frame.txt already shows the map correctly (no
             * MAP3D_MARKER line was emitted - see ops/compose_frame.c's
             * own render_mode check) - this op has nothing to do here
             * at all, matching its original (pre-3D) behavior of never
             * running in chtpm mode. */
            return 0;
        }

        char map_dir_early[PATH_BUF];
        {
            char map_id_early[64];
            read_kv_str(hero_path, "map_id", map_id_early, sizeof(map_id_early), "map_start");
            snprintf(map_dir_early, sizeof(map_dir_early), "%s/pieces/world_01/%s", project_root, map_id_early);
        }
        char map_path_early[PATH_BUF + 32], furniture_path_early[PATH_BUF + 32], turn_path_early[PATH_BUF + 32];
        snprintf(map_path_early, sizeof(map_path_early), "%s/map.txt", map_dir_early);
        snprintf(furniture_path_early, sizeof(furniture_path_early), "%s/furniture.txt", map_dir_early);
        snprintf(turn_path_early, sizeof(turn_path_early), "%s/state.txt", map_dir_early);
        int map_w_early = read_kv_int(turn_path_early, "width", VIEWPORT_W);
        int map_h_early = read_kv_int(turn_path_early, "height", VIEWPORT_H);
        if (map_w_early > MAX_MAP_W) map_w_early = MAX_MAP_W;
        if (map_h_early > MAX_MAP_H) map_h_early = MAX_MAP_H;

        static char grid_early[MAX_MAP_H][MAX_MAP_W + 1];
        static char cell_asset_early[MAX_MAP_H][MAX_MAP_W][ASSET_ID_BUF];
        static char cell_is_entity_early[MAX_MAP_H][MAX_MAP_W];
        memset(cell_is_entity_early, 0, sizeof(cell_is_entity_early));
        int rows_early = 0;
        FILE *mf2 = fopen(map_path_early, "r");
        FILE *ff2 = fopen(furniture_path_early, "r");
        if (mf2) {
            char terrain_line[MAX_MAP_W + 4], furniture_line[MAX_MAP_W + 4];
            while (rows_early < map_h_early && fgets(terrain_line, sizeof(terrain_line), mf2)) {
                terrain_line[strcspn(terrain_line, "\n")] = '\0';
                furniture_line[0] = '\0';
                if (ff2) {
                    if (!fgets(furniture_line, sizeof(furniture_line), ff2)) furniture_line[0] = '\0';
                    furniture_line[strcspn(furniture_line, "\n")] = '\0';
                }
                int len2 = (int)strlen(terrain_line);
                if (len2 > map_w_early) len2 = map_w_early;
                for (int col = 0; col < len2; col++) {
                    char fg = (col < (int)strlen(furniture_line)) ? furniture_line[col] : ' ';
                    grid_early[rows_early][col] = (fg != ' ') ? fg : terrain_line[col];
                    terrain_or_furniture_asset_id(grid_early[rows_early][col], cell_asset_early[rows_early][col], ASSET_ID_BUF);
                }
                grid_early[rows_early][len2] = '\0';
                rows_early++;
            }
            fclose(mf2);
        }
        if (ff2) fclose(ff2);

        /* Draw hero and xlector cursor on the grid so render_3d_view()
         * can render them as 3D entities (matching the non-chtpm path's
         * grid layering at lines 1552-1587). Without this, the3D view
         * shows only terrain/furniture - the xlector cursor and hero
         * sprite are invisible. cell_asset_early mirrors the non-chtpm
         * path's cell_asset (see that array's own declaration comment)
         * so the 3D ray marcher can extrude/texture hero and xlector
         * here too, not just draw them as flat floor-color quads. */
        if (py >= 0 && py < rows_early && px >= 0 && px < map_w_early) {
            grid_early[py][px] = '@';
            snprintf(cell_asset_early[py][px], ASSET_ID_BUF, "hero");
            cell_is_entity_early[py][px] = 1;
        }
        if (interact_mode == 1 && xlector_y >= 0 && xlector_y < rows_early && xlector_x >= 0 && xlector_x < map_w_early) {
            grid_early[xlector_y][xlector_x] = 'X';
            snprintf(cell_asset_early[xlector_y][xlector_x], ASSET_ID_BUF, "xlector");
            cell_is_entity_early[xlector_y][xlector_x] = 1;
        }

        /* REAL BUG FIX (2026-07-27, direct live testing: "rgb is currently
         * not rendering a lot of stuff like zombies logs cup toilet paper
         * etc" - confirmed this chtpm-mode overlay path is a SEPARATE,
         * incomplete copy of the non-chtpm grid-population code below -
         * it only ever drew terrain/furniture/hero/xlector. Items,
         * monsters, and imported pets were never added here at all, even
         * though the non-chtpm path (and cell_asset's own design) has
         * always supported them - this is why walls/floor render fine in
         * chtpm+3D mode but every entity/item is invisible. Byte-for-byte
         * mirrors of the non-chtpm path's own items/monsters/pets loops
         * below, just targeting the _early grid/map_dir_early. */
        char items_dir_early[PATH_BUF + 32];
        snprintf(items_dir_early, sizeof(items_dir_early), "%s/items", map_dir_early);
        DIR *d_early = opendir(items_dir_early);
        if (d_early) {
            struct dirent *entry;
            while ((entry = readdir(d_early)) != NULL) {
                if (entry->d_name[0] == '.') continue;
                char state_path[PATH_BUF + 320];
                snprintf(state_path, sizeof(state_path), "%s/%s/state.txt", items_dir_early, entry->d_name);
                int ix = read_kv_int(state_path, "pos_x", -1);
                int iy = read_kv_int(state_path, "pos_y", -1);
                if (ix < 0 || iy < 0 || ix >= map_w_early || iy >= rows_early) continue;
                char item_id[64];
                read_kv_str(state_path, "item_id", item_id, sizeof(item_id), "?");
                char glyph[4];
                item_registry_field(item_id, 3, glyph, sizeof(glyph), "?");
                if (glyph[0]) grid_early[iy][ix] = glyph[0];
                if (glyph[0]) { snprintf(cell_asset_early[iy][ix], ASSET_ID_BUF, "%s", item_id); cell_is_entity_early[iy][ix] = 1; }
            }
            closedir(d_early);
        }

        char monsters_dir_early[PATH_BUF + 32];
        snprintf(monsters_dir_early, sizeof(monsters_dir_early), "%s/monsters", map_dir_early);
        d_early = opendir(monsters_dir_early);
        if (d_early) {
            struct dirent *entry;
            while ((entry = readdir(d_early)) != NULL) {
                if (entry->d_name[0] == '.') continue;
                char state_path[PATH_BUF + 320];
                snprintf(state_path, sizeof(state_path), "%s/%s/state.txt", monsters_dir_early, entry->d_name);
                int mx = read_kv_int(state_path, "pos_x", -1);
                int my = read_kv_int(state_path, "pos_y", -1);
                if (mx < 0 || my < 0 || mx >= map_w_early || my >= rows_early) continue;
                char monster_type[64];
                read_kv_str(state_path, "monster_type", monster_type, sizeof(monster_type), "?");
                char glyph[4];
                monster_registry_field(monster_type, 2, glyph, sizeof(glyph), "?");
                if (glyph[0]) grid_early[my][mx] = glyph[0];
                if (glyph[0]) { snprintf(cell_asset_early[my][mx], ASSET_ID_BUF, "%s", monster_type); cell_is_entity_early[my][mx] = 1; }
            }
            closedir(d_early);
        }

        d_early = opendir(map_dir_early);
        if (d_early) {
            struct dirent *entry;
            while ((entry = readdir(d_early)) != NULL) {
                if (entry->d_name[0] == '.') continue;
                if (strcmp(entry->d_name, "hero") == 0 ||
                    strcmp(entry->d_name, "items") == 0 ||
                    strcmp(entry->d_name, "monsters") == 0) continue;
                char pet_state_path[PATH_BUF + 320];
                snprintf(pet_state_path, sizeof(pet_state_path), "%s/%s/state.txt", map_dir_early, entry->d_name);
                int pxp = read_kv_int(pet_state_path, "pos_x", -1);
                int pyp = read_kv_int(pet_state_path, "pos_y", -1);
                if (pxp < 0 || pyp < 0 || pxp >= map_w_early || pyp >= rows_early) continue;
                char pet_species[ASSET_ID_BUF];
                read_kv_str(pet_state_path, "species", pet_species, sizeof(pet_species), "hero");
                grid_early[pyp][pxp] = 'p';
                char asset_check[PATH_BUF];
                snprintf(asset_check, sizeof(asset_check), "%s/pieces/registry/emoji_assets/%s/voxels_16.csv", project_root, pet_species);
                FILE *af = fopen(asset_check, "r");
                if (af) { fclose(af); snprintf(cell_asset_early[pyp][pxp], ASSET_ID_BUF, "%s", pet_species); }
                else snprintf(cell_asset_early[pyp][pxp], ASSET_ID_BUF, "hero");
                cell_is_entity_early[pyp][pxp] = 1;
            }
            closedir(d_early);
        }

        int ov_w = VIEWPORT_W * TILE_PX;
        int ov_h = VIEWPORT_H * TILE_PX;
        static unsigned char overlay_buf[VIEWPORT_H * TILE_PX][VIEWPORT_W * TILE_PX][4];
        for (int oy = 0; oy < ov_h; oy++)
            for (int ox = 0; ox < ov_w; ox++)
                overlay_buf[oy][ox][3] = 255;
        /* Anchor camera on the live controlled entity (xlector cursor when
         * in interact mode, hero otherwise). Same logic as non-chtpm path
         * at line 1615. Without this, the 3D camera always faces the hero
         * position even when the xlector cursor is being moved. */
        int anchor_x = (interact_mode == 1) ? xlector_x : px;
        int anchor_y = (interact_mode == 1) ? xlector_y : py;
        render_3d_view((unsigned char *)overlay_buf, ov_w, ov_h, grid_early, rows_early, map_w_early,
                        cell_asset_early, cell_is_entity_early, anchor_x, anchor_y, camera_mode, facing, cam_pan_x, cam_pan_y, cam_pan_z,
                        cam_yaw, cam_pitch, cam_z_level);

        char overlay_path[PATH_BUF], overlay_receipt_path[PATH_BUF];
        snprintf(overlay_path, sizeof(overlay_path), "%s/pieces/display/rgb_frame_3d_overlay.raw", project_root);
        snprintf(overlay_receipt_path, sizeof(overlay_receipt_path), "%s/pieces/display/rgb_frame_3d_overlay.receipt.txt", project_root);
        {
            char ov_tmp[PATH_BUF + 4];
            snprintf(ov_tmp, sizeof(ov_tmp), "%s.tmp", overlay_path);
            FILE *ovf = fopen(ov_tmp, "wb");
            if (ovf) {
                fwrite(overlay_buf, 1, (size_t)ov_w * ov_h * 4, ovf);
                fclose(ovf);
                rename(ov_tmp, overlay_path);
            }
        }
        FILE *ovr = fopen(overlay_receipt_path, "w");
        if (ovr) {
            fprintf(ovr, "overlay_w=%d\noverlay_h=%d\n", ov_w, ov_h);
            fclose(ovr);
        }
        return 0;
    }
    char map_id[64];
    read_kv_str(hero_path, "map_id", map_id, sizeof(map_id), "map_start");

    char map_dir[PATH_BUF];
    snprintf(map_dir, sizeof(map_dir), "%s/pieces/world_01/%s", project_root, map_id);
    char map_path[PATH_BUF + 32], furniture_path[PATH_BUF + 32], turn_path[PATH_BUF + 32];
    snprintf(map_path, sizeof(map_path), "%s/map.txt", map_dir);
    snprintf(furniture_path, sizeof(furniture_path), "%s/furniture.txt", map_dir);
    snprintf(turn_path, sizeof(turn_path), "%s/state.txt", map_dir);

    int turn = read_kv_int(turn_path, "turn", 0);
    int map_w = read_kv_int(turn_path, "width", VIEWPORT_W);
    int map_h = read_kv_int(turn_path, "height", VIEWPORT_H);
    if (map_w > MAX_MAP_W) map_w = MAX_MAP_W;
    if (map_h > MAX_MAP_H) map_h = MAX_MAP_H;
    /* Same "one state, two renderers" mirror principle as
     * compose_frame.c - z is the same purely-informational multi-Z
     * floor field, kept consistent across both renderers. */
    int z = read_kv_int(turn_path, "z", 0);

    /* Grid-building (terrain -> furniture -> items -> monsters -> hero)
     * copied verbatim in shape from compose_frame.c - same absolute
     * map-space buffer, same layering order, so the two renderers can
     * never show a different world, only a different encoding of it. */
    char grid[MAX_MAP_H][MAX_MAP_W + 1];
    /* Parallel per-cell asset-id map, identity-resolved at the exact
     * point each layer is drawn (not re-derived from grid's flat char
     * afterward) - same reasoning as compose_frame.c's own cell_emoji
     * buffer: this project's registries have real glyph collisions
     * between terrain and items ('=', '~', '%'), so a naive shared-char
     * lookup would pick the wrong asset for whichever one is actually
     * at that tile. static, matching this file's own framebuf
     * convention for large buffers. Populated when emoji_mode=1 (2D
     * top-down emoji blit reads it) OR render_mode=1 (3D view's own
     * wall/entity ray marcher reads it too, regardless of emoji_mode -
     * 3D always textures, same as walls already did before entities
     * were added to this pass). */
    static char cell_asset[MAX_MAP_H][MAX_MAP_W][ASSET_ID_BUF];
    /* Parallel "is this cell a real entity (hero/xlector/item/monster),
     * not just ordinary terrain/furniture" flag - cell_asset alone is
     * NOT that signal, since terrain/furniture tiles get a resolved
     * asset id here too (every walkable floor tile has SOME asset,
     * e.g. "t_floor") - REAL BUG FIX: the 3D entity ray marcher below
     * originally used "cell_asset[row][col][0] non-empty" to mean
     * "there's an entity here," which was true for literally every
     * terrain/furniture tile as well, so it tried to extrude+texture
     * every floor tile as an "entity" - most terrain assets have no
     * voxels_8.csv, so those all fell back to glyph_fallback_rgb's
     * generic magenta "unmapped" color, filling the whole ground level
     * with pink squares. This flag is set ONLY at the exact points
     * below where an item/monster/hero/xlector claims a cell. */
    static char cell_is_entity[MAX_MAP_H][MAX_MAP_W];
    int rows = 0;
    FILE *mf = fopen(map_path, "r");
    FILE *ff = fopen(furniture_path, "r");
    if (mf) {
        char terrain_line[MAX_MAP_W + 4], furniture_line[MAX_MAP_W + 4];
        while (rows < map_h && fgets(terrain_line, sizeof(terrain_line), mf)) {
            terrain_line[strcspn(terrain_line, "\n")] = '\0';
            furniture_line[0] = '\0';
            if (ff) {
                if (!fgets(furniture_line, sizeof(furniture_line), ff)) furniture_line[0] = '\0';
                furniture_line[strcspn(furniture_line, "\n")] = '\0';
            }
            int len = (int)strlen(terrain_line);
            if (len > map_w) len = map_w;
            for (int col = 0; col < len; col++) {
                char fg = (col < (int)strlen(furniture_line)) ? furniture_line[col] : ' ';
                grid[rows][col] = (fg != ' ') ? fg : terrain_line[col];
                if (emoji_mode || render_mode == 1) terrain_or_furniture_asset_id(grid[rows][col], cell_asset[rows][col], ASSET_ID_BUF);
                cell_is_entity[rows][col] = 0;
            }
            grid[rows][len] = '\0';
            rows++;
        }
        fclose(mf);
    }
    if (ff) fclose(ff);

    char items_dir[PATH_BUF + 32];
    snprintf(items_dir, sizeof(items_dir), "%s/items", map_dir);
    DIR *d = opendir(items_dir);
    if (d) {
        struct dirent *entry;
        while ((entry = readdir(d)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            char state_path[PATH_BUF + 320];
            snprintf(state_path, sizeof(state_path), "%s/%s/state.txt", items_dir, entry->d_name);
            int ix = read_kv_int(state_path, "pos_x", -1);
            int iy = read_kv_int(state_path, "pos_y", -1);
            if (ix < 0 || iy < 0 || ix >= map_w || iy >= rows) continue;
            char item_id[64];
            read_kv_str(state_path, "item_id", item_id, sizeof(item_id), "?");
            char glyph[4];
            item_registry_field(item_id, 3, glyph, sizeof(glyph), "?");
            if (glyph[0]) grid[iy][ix] = glyph[0];
            /* Resolved by item_id directly (its own asset dir name),
             * not re-derived from glyph[0] - see cell_asset's own
             * declaration comment for the real terrain/item collisions
             * this avoids. */
            if ((emoji_mode || render_mode == 1) && glyph[0]) { snprintf(cell_asset[iy][ix], ASSET_ID_BUF, "%s", item_id); cell_is_entity[iy][ix] = 1; }
        }
        closedir(d);
    }

    char monsters_dir[PATH_BUF + 32];
    snprintf(monsters_dir, sizeof(monsters_dir), "%s/monsters", map_dir);
    d = opendir(monsters_dir);
    if (d) {
        struct dirent *entry;
        while ((entry = readdir(d)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            char state_path[PATH_BUF + 320];
            snprintf(state_path, sizeof(state_path), "%s/%s/state.txt", monsters_dir, entry->d_name);
            int mx = read_kv_int(state_path, "pos_x", -1);
            int my = read_kv_int(state_path, "pos_y", -1);
            if (mx < 0 || my < 0 || mx >= map_w || my >= rows) continue;
            char monster_type[64];
            read_kv_str(state_path, "monster_type", monster_type, sizeof(monster_type), "?");
            char glyph[4];
            monster_registry_field(monster_type, 2, glyph, sizeof(glyph), "?");
            if (glyph[0]) grid[my][mx] = glyph[0];
            /* Resolved by monster_type directly - same reasoning as the
             * item loop above. */
            if ((emoji_mode || render_mode == 1) && glyph[0]) { snprintf(cell_asset[my][mx], ASSET_ID_BUF, "%s", monster_type); cell_is_entity[my][mx] = 1; }
        }
        closedir(d);
    }

    /* Imported pets (see 0.a-z-pets-plan/a-z-fix-report.txt) - real
     * byte-for-byte copy of compose_frame.c's own pets pass, adapted to
     * this file's cell_asset/cell_is_entity convention instead of a
     * live emoji string. Unlike monsters/items (registry-keyed native
     * content), a pet's species_id doubles directly as its emoji-asset
     * directory name here - ops/pet_export.c's own species_emoji field
     * is what got turned into a real pieces/registry/emoji_assets/
     * <species_id>/voxels_16.csv via muchi-pals' emoji_gen_atlas.+x/
     * emoji_xtract.+x (this project's own equivalent tool is not
     * present - see extrude-emoji.md - so the asset has to be generated
     * from the origin game's own copy of that pipeline once, offline,
     * same one-shot-then-commit shape every other asset in this
     * registry was made with). Falls back to the fixed "hero" asset id
     * if no matching directory exists yet, so an unknown species still
     * renders as *something* rather than a blank/fallback-color tile. */
    d = opendir(map_dir);
    if (d) {
        struct dirent *entry;
        while ((entry = readdir(d)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            if (strcmp(entry->d_name, "hero") == 0 ||
                strcmp(entry->d_name, "items") == 0 ||
                strcmp(entry->d_name, "monsters") == 0) continue;
            char pet_state_path[PATH_BUF + 320];
            snprintf(pet_state_path, sizeof(pet_state_path), "%s/%s/state.txt", map_dir, entry->d_name);
            int pxp = read_kv_int(pet_state_path, "pos_x", -1);
            int pyp = read_kv_int(pet_state_path, "pos_y", -1);
            if (pxp < 0 || pyp < 0 || pxp >= map_w || pyp >= rows) continue;
            char pet_species[ASSET_ID_BUF];
            read_kv_str(pet_state_path, "species", pet_species, sizeof(pet_species), "hero");
            grid[pyp][pxp] = 'p';
            if (emoji_mode || render_mode == 1) {
                char asset_check[PATH_BUF];
                snprintf(asset_check, sizeof(asset_check), "%s/pieces/registry/emoji_assets/%s/voxels_16.csv", project_root, pet_species);
                FILE *af = fopen(asset_check, "r");
                if (af) { fclose(af); snprintf(cell_asset[pyp][pxp], ASSET_ID_BUF, "%s", pet_species); }
                else snprintf(cell_asset[pyp][pxp], ASSET_ID_BUF, "hero");
                cell_is_entity[pyp][pxp] = 1;
            }
        }
        closedir(d);
    }

    if (py >= 0 && py < rows) {
        int rowlen = (int)strlen(grid[py]);
        while (rowlen <= px && rowlen < map_w) {
            grid[py][rowlen] = ' ';
            if (emoji_mode || render_mode == 1) cell_asset[py][rowlen][0] = '\0';
            cell_is_entity[py][rowlen] = 0;
            rowlen++;
            grid[py][rowlen] = '\0';
        }
        if (px >= 0 && px < map_w) {
            grid[py][px] = '@';
            /* Hero/xlector are hardcoded chars, not registry-driven -
             * "hero"/"xlector" are the fixed asset ids generated for
             * them (see the emoji-asset generation this feature's own
             * writeup covers). */
            if (emoji_mode || render_mode == 1) snprintf(cell_asset[py][px], ASSET_ID_BUF, "hero");
            cell_is_entity[py][px] = 1;
        }
    }

    /* Xlector cursor - same real xlector active-target pattern as
     * compose_frame.c's own copy (see that file's header comment and
     * ops/choice.c's own writeup / dox/04-chtpm-parser-research-and-
     * interact-mode.txt for the full research). Drawn last, on top of
     * even the hero. */
    if (interact_mode == 1 && xlector_y >= 0 && xlector_y < rows) {
        int rowlen = (int)strlen(grid[xlector_y]);
        while (rowlen <= xlector_x && rowlen < map_w) {
            grid[xlector_y][rowlen] = ' ';
            if (emoji_mode || render_mode == 1) cell_asset[xlector_y][rowlen][0] = '\0';
            cell_is_entity[xlector_y][rowlen] = 0;
            rowlen++;
            grid[xlector_y][rowlen] = '\0';
        }
        if (xlector_x >= 0 && xlector_x < map_w) {
            grid[xlector_y][xlector_x] = 'X';
            if (emoji_mode || render_mode == 1) snprintf(cell_asset[xlector_y][xlector_x], ASSET_ID_BUF, "xlector");
            cell_is_entity[xlector_y][xlector_x] = 1;
        }
    }

    /* Camera - byte-for-byte the same clamp formula as compose_frame.c,
     * so the GL viewport and the ASCII viewport always show the exact
     * same rectangle of the map. REAL BUG FIX (2026-07-21): anchor on
     * the LIVE controlled entity, not always the hero - see
     * compose_frame.c's own matching comment on this exact fix for the
     * full writeup (same class of bug as the sibling $.4x family's
     * stale-anchor camera bug, adapted here since move_player.c's
     * xlector cursor is genuinely free-roaming across the whole map,
     * clamped only to map bounds, not to the hero or the viewport). */
    int anchor_x = (interact_mode == 1) ? xlector_x : px;
    int anchor_y = (interact_mode == 1) ? xlector_y : py;
    int cam_x = anchor_x - VIEWPORT_W / 2;
    int cam_x_max = map_w - VIEWPORT_W;
    if (cam_x_max < 0) cam_x_max = 0;
    if (cam_x < 0) cam_x = 0;
    if (cam_x > cam_x_max) cam_x = cam_x_max;
    int cam_y = anchor_y - VIEWPORT_H / 2;
    int cam_y_max = map_h - VIEWPORT_H;
    if (cam_y_max < 0) cam_y_max = 0;
    if (cam_y < 0) cam_y = 0;
    if (cam_y > cam_y_max) cam_y = cam_y_max;

    /* Rasterize: one glyph -> one TILE_PX x TILE_PX solid block, written
     * top-to-bottom, left-to-right, RGBA8888 - the exact flat layout
     * wraith_gl.c's load_texture()/glTexImage2D expects from its own
     * WRAITH_FRAME_SOURCE file, so system/gl_mirror.c's port of that
     * function can read this file with zero format translation. Tile
     * rows start HEADER_ROWS*GLYPH_H pixels down, leaving the top text
     * row untouched (stays black, matching blit_char()'s
     * leave-background-alone behavior) for the header text blitted
     * below. */
    static unsigned char framebuf[FRAME_H][FRAME_W][4];
    /* Alpha defined as fully opaque everywhere up front - the tile loop
     * below and blit_char() both only ever set alpha=255 on pixels they
     * actually touch, which would otherwise leave the header/footer
     * text rows' background at the static zero-init's alpha=0 (RGB 0
     * too, so visually identical black either way since neither
     * gl_mirror.c nor real wraith_gl.c enables GL_BLEND - this is about
     * this file being a well-defined RGBA32 buffer on its own terms,
     * not a rendering bug). */
    for (int fy = 0; fy < FRAME_H; fy++)
        for (int fx = 0; fx < FRAME_W; fx++)
            framebuf[fy][fx][3] = 255;
    int tile_y0 = HEADER_ROWS * GLYPH_H;
    if (render_mode == 1) {
        /* Real 3D perspective view - see render_3d_view()'s own header
         * comment above main() for the full writeup and named
         * simplifications vs. the real piececraft-wraith reference.
         * emoji_mode is irrelevant here (real terrain colors either
         * way, no flat-vs-emoji distinction in 3D) - and this whole
         * branch is a no-op in ops/compose_frame.c (ASCII), matching
         * real wraith's own convention that '0' only changes what the
         * GL view shows. */
        render_3d_view((unsigned char *)framebuf, FRAME_W, FRAME_H, grid, rows, map_w, cell_asset, cell_is_entity, px, py, camera_mode, facing, cam_pan_x, cam_pan_y, cam_pan_z,
                        cam_yaw, cam_pitch, cam_z_level);
    } else {
        for (int vr = 0; vr < VIEWPORT_H; vr++) {
            int src_row = cam_y + vr;
            int src_len = (src_row >= 0 && src_row < rows) ? (int)strlen(grid[src_row]) : 0;
            for (int vc = 0; vc < VIEWPORT_W; vc++) {
                int src_col = cam_x + vc;
                char glyph = (src_row >= 0 && src_row < rows && src_col < src_len) ? grid[src_row][src_col] : ' ';
                int r, g, b;
                glyph_to_rgb(glyph, emoji_mode, &r, &g, &b);
                int tile_x0 = vc * TILE_PX;
                int tile_y0_px = tile_y0 + vr * TILE_PX;
                for (int py2 = 0; py2 < TILE_PX; py2++) {
                    int fy = tile_y0_px + py2;
                    for (int px2 = 0; px2 < TILE_PX; px2++) {
                        int fx = tile_x0 + px2;
                        framebuf[fy][fx][0] = (unsigned char)r;
                        framebuf[fy][fx][1] = (unsigned char)g;
                        framebuf[fy][fx][2] = (unsigned char)b;
                        framebuf[fy][fx][3] = 255;
                    }
                }
                /* Real rasterized emoji pixels on top of the flat base
                 * color - see blit_emoji_tile()'s own header comment for
                 * the full wraith-alpha-derived pipeline. */
                if (emoji_mode && src_row >= 0 && src_row < rows && src_col < src_len && cell_asset[src_row][src_col][0]) {
                    unsigned char voxels[TILE_PX][TILE_PX][4];
                    if (load_emoji_voxels(cell_asset[src_row][src_col], voxels)) {
                        blit_emoji_tile(framebuf, tile_x0, tile_y0_px, voxels);
                    }
                }
            }
        }
    }

    /* Overlay panel (craft/inventory) - see draw_panel_box_gl()'s own
     * header comment. Content-building logic (recipe/inventory row
     * formatting) is a direct copy of compose_frame.c's own main() -
     * same registry files, same "ready"/"missing" check, same trailing
     * Cancel/Close row convention - only the final draw call differs
     * (pixels instead of characters). */
    char active_panel[32];
    read_kv_str(hero_path, "active_panel", active_panel, sizeof(active_panel), "none");
    if (strcmp(active_panel, "craft") == 0) {
        int panel_cursor = read_kv_int(hero_path, "panel_cursor", 1);
        char recipes_path[PATH_BUF];
        snprintf(recipes_path, sizeof(recipes_path), "%s/pieces/registry/recipes/recipes.txt", project_root);
        FILE *rf = fopen(recipes_path, "r");
        char panel_rows[MAX_RECIPES_DISPLAY][BOX_TEXT_W + 1];
        int panel_row_count = 0;
        if (rf) {
            char rline[MAX_LINE];
            while (panel_row_count < MAX_RECIPES_DISPLAY && fgets(rline, sizeof(rline), rf)) {
                if (rline[0] == '#' || rline[0] == '\n') continue;
                rline[strcspn(rline, "\n")] = '\0';
                char *p1 = strchr(rline, '|');
                if (!p1) continue;
                *p1 = '\0';
                char *rname = p1 + 1;
                char *p2 = strchr(rname, '|');
                if (!p2) continue;
                *p2 = '\0';
                char *presult = p2 + 1;
                char *p3 = strchr(presult, '|');
                if (!p3) continue;
                *p3 = '\0';
                char *reqs_str = p3 + 1;

                int satisfied = 1;
                char reqs_copy[MAX_LINE];
                snprintf(reqs_copy, sizeof(reqs_copy), "%s", reqs_str);
                char *tok = strtok(reqs_copy, ",");
                while (tok) {
                    char *colon = strchr(tok, ':');
                    if (colon) {
                        *colon = '\0';
                        if (count_in_inventory(tok) < atoi(colon + 1)) satisfied = 0;
                    }
                    tok = strtok(NULL, ",");
                }

                int idx = panel_row_count + 1;
                const char *cur = (idx == panel_cursor) ? "[>]" : "[ ]";
                snprintf(panel_rows[panel_row_count], sizeof(panel_rows[0]), "%s %d. [%s]%s",
                         cur, idx, rname, satisfied ? " (ready)" : " (missing)");
                panel_row_count++;
            }
            fclose(rf);
        }
        int cancel_idx = panel_row_count + 1;
        const char *cancel_cur = (cancel_idx == panel_cursor) ? "[>]" : "[ ]";
        char cancel_row[BOX_TEXT_W + 1];
        snprintf(cancel_row, sizeof(cancel_row), "%s %d. [Cancel]", cancel_cur, cancel_idx);

        draw_panel_box_gl(framebuf, tile_y0, "CRAFT", panel_rows, panel_row_count, cancel_row);
    } else if (strcmp(active_panel, "inventory") == 0) {
        int panel_cursor = read_kv_int(hero_path, "panel_cursor", 1);
        char inventory_dir_panel[PATH_BUF];
        snprintf(inventory_dir_panel, sizeof(inventory_dir_panel), "%s/pieces/world_01/map_start/hero/inventory", project_root);
        char panel_rows[MAX_RECIPES_DISPLAY][BOX_TEXT_W + 1];
        int panel_row_count = 0;
        DIR *pd = opendir(inventory_dir_panel);
        if (pd) {
            struct dirent *entry;
            while (panel_row_count < MAX_RECIPES_DISPLAY && (entry = readdir(pd)) != NULL) {
                if (entry->d_name[0] == '.') continue;
                char state_path[PATH_BUF + 384];
                snprintf(state_path, sizeof(state_path), "%s/%s/state.txt", inventory_dir_panel, entry->d_name);
                char item_id[64];
                read_kv_str(state_path, "item_id", item_id, sizeof(item_id), "?");

                char iname[64], category[16], power_str[16];
                item_registry_field(item_id, 1, iname, sizeof(iname), item_id);
                item_registry_field(item_id, 2, category, sizeof(category), "?");
                item_registry_field(item_id, 5, power_str, sizeof(power_str), "0");
                int power = atoi(power_str);

                char detail[24] = "";
                if (strcmp(category, "weapon") == 0) snprintf(detail, sizeof(detail), " dmg+%d", power);
                else if (strcmp(category, "food") == 0) snprintf(detail, sizeof(detail), " hunger-%d", power);
                else if (strcmp(category, "drink") == 0) snprintf(detail, sizeof(detail), " thirst-%d", power);
                else if (strcmp(category, "armor") == 0) snprintf(detail, sizeof(detail), " def+%d", power);

                int idx = panel_row_count + 1;
                const char *cur = (idx == panel_cursor) ? "[>]" : "[ ]";
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
                snprintf(panel_rows[panel_row_count], sizeof(panel_rows[0]), "%s %d. [%s] (%s)%s",
                         cur, idx, iname, category, detail);
#pragma GCC diagnostic pop
                panel_row_count++;
            }
            closedir(pd);
        }
        int cancel_idx = panel_row_count + 1;
        const char *cancel_cur = (cancel_idx == panel_cursor) ? "[>]" : "[ ]";
        char cancel_row[BOX_TEXT_W + 1];
        snprintf(cancel_row, sizeof(cancel_row), "%s %d. [Close]", cancel_cur, cancel_idx);
        if (panel_row_count == 0) {
            snprintf(panel_rows[0], sizeof(panel_rows[0]), "[ ] (nothing carried)");
            panel_row_count = 1;
        }

        draw_panel_box_gl(framebuf, tile_y0, "INVENTORY", panel_rows, panel_row_count, cancel_row);
    }

    /* Nav/HUD text - the font pipeline's whole point. White on the
     * framebuffer's default-black background, same as compose_frame.c's
     * ASCII header/stat lines carry the same information. */
    char header_text[160];
    if (z != 0) {
        snprintf(header_text, sizeof(header_text), "MUTACLSYM  map: %-10s turn: %d  Floor: %d", map_id, turn, z);
    } else {
        snprintf(header_text, sizeof(header_text), "MUTACLSYM  map: %-10s turn: %d", map_id, turn);
    }
    blit_text(framebuf, 0, 0, header_text, 255, 255, 255);

    char footer_text[96];
    snprintf(footer_text, sizeof(footer_text), "HP:%d Hunger:%d Thirst:%d Stamina:%d",
             hp, hunger, thirst, stamina);
    blit_text(framebuf, 0, tile_y0 + VIEWPORT_H * TILE_PX, footer_text, 255, 255, 255);

    char action_footer[512];
    build_action_footer(project_root, action_footer, sizeof(action_footer), action_cursor, interact_mode);
    blit_text(framebuf, 0, tile_y0 + VIEWPORT_H * TILE_PX + GLYPH_H, action_footer, 255, 255, 255);

    size_t byte_count = (size_t)FRAME_W * FRAME_H * 4;
    /* Atomic write: write to .tmp then rename() over the real path.
     * POSIX rename() onto the same filesystem is atomic - any reader
     * sees either the complete OLD file or the complete NEW one, never
     * a partial/empty one. Without this, fopen("wb") truncates to 0
     * bytes before writing - a reader (gl_mirror / chtpm_rgb_render)
     * that opens the file in that window sees a black frame = flicker.
     * Matches chtpm_rgb_render.c's own write_file_atomic() pattern. */
    {
        char tmp_path[PATH_BUF + 4];
        snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", out_path);
        FILE *out = fopen(tmp_path, "wb");
        if (!out) return 1;
        size_t written = fwrite(framebuf, 1, byte_count, out);
        fclose(out);
        if (written != byte_count) { remove(tmp_path); return 1; }
        rename(tmp_path, out_path);
    }

    uint64_t checksum = checksum_buffer((const unsigned char *)framebuf, byte_count);
    write_receipt(receipt_path, byte_count, checksum, map_w, map_h, cam_x, cam_y);

    {
        FILE *pf = fopen(rgb_pulse_path, "a");
        if (pf) { fputc('P', pf); fputc('\n', pf); fclose(pf); }
    }

    return 0;
}
