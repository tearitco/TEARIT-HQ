/* chtpm_rgb_render - SHARED, PERSISTENT process (yz.muchiverse/
 * 2.muchi-verse/shared-ops/, see !.handoff-doc-j17.txt and
 * chtpm-to-pal-layout-plan.txt §8/§2 in that handoff doc for the full
 * why). Same category as system/renderer.c/system/gl_mirror.c already
 * are in this family - a long-running daemon, NOT a one-shot pal op
 * invoked from a .pal script.
 *
 * WHAT THIS IS: real 1.TPMOS's own wraith_rgb_daemon.c
 * (projects/wraith-alpha/plugins/), confirmed via direct code read,
 * does something genuinely simple: it watches pieces/display/
 * frame_changed.txt, reads pieces/display/current_frame.txt (the FULL
 * ASCII text chtpm_parser.c composed - chrome AND embedded content,
 * everything), and font-rasterizes EVERY CHARACTER OF IT into an
 * RGBA32 buffer - confirmed explicitly by that project's own
 * architecture doc: "Does NOT parse .chtpm... does not recognize
 * <cli_io>, <button>, <text> tags." Zero semantic awareness of
 * buttons/panels - it just blits whatever characters are already
 * there. This is the ENTIRE reason real 1.TPMOS's own GL window shows
 * chtpm's own menu chrome: the chrome is literally IN the text buffer
 * already, and every character gets rasterized, full stop.
 *
 * THIS FILE is the pal-native equivalent - same shape, same job.
 * `ops/compose_rgb_frame.c` (per-project, already exists in mutaclsym/
 * zoo_0000) does something DIFFERENT: it draws real 2D TILE graphics
 * (walls/furniture as actual colored tiles) directly from game-state
 * files, never touching current_frame.txt at all - which is exactly
 * why chtpm's own chrome never appeared in the GL window before this
 * file existed. `load_glyphs()`/`blit_char()`/`blit_text()` below are
 * a DIRECT PORT of the identically-named, identically-shaped functions
 * already in `ops/compose_rgb_frame.c` (which itself already ported
 * them from real wraith_rgb_daemon.c - see that file's own header
 * comment) - NOT reinvented, just re-purposed to rasterize the WHOLE
 * buffer instead of one footer line.
 *
 * WHY 640x768, NOT 640x304 (real, direct user report, not guessed):
 * this file used to match system/gl_mirror.c's own then-HARDCODED
 * `#define WIDTH 640` / `#define HEIGHT 304`, reasoning that reusing
 * its existing fixed expectation was lower-risk than changing it. That
 * reasoning turned out wrong in practice - a real chtpm frame
 * (chrome + embedded ${game_map} + status/inventory/message-log +
 * footer) is genuinely ~35 lines tall (confirmed via direct count of a
 * live current_frame.txt), while 304px/GLYPH_H(16) only ever fit 19 -
 * the bottom of the map and most of the footer were being silently
 * clipped every single frame, not just at the edges. gl_mirror.c now
 * reads frame_w/frame_h from rgb_frame.receipt.txt DYNAMICALLY (same
 * pattern shared-ops/dump_rgb_png.c already used successfully) instead
 * of hardcoding them, so this file is free to size itself for its own
 * real content instead of an unrelated fixed constant. 48 rows (768px)
 * gives real headroom above the observed ~35-line maximum (message log
 * can grow to LOG_TAIL=4 lines, footers vary) without being wasteful.
 * Width stays 640 (80 GLYPH_W(8) columns) - already confirmed generous
 * against the longest real chrome line (~72 chars).
 *
 * Writes pieces/display/rgb_frame.raw + rgb_frame.receipt.txt - the
 * SAME paths system/gl_mirror.c and shared-ops/dump_rgb_png.c already
 * read. "One state, multiple renderers": whichever process last wrote
 * rgb_frame.raw wins - ops/compose_rgb_frame.c's own real tile
 * graphics in regular "run" mode (still exactly 640x304, its own
 * unrelated game-viewport-derived size, untouched by this change), or
 * this daemon's text rasterization in "chtpm" mode - gl_mirror.c reads
 * whichever size the CURRENT receipt says, correctly handling either. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdint.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

#define GLYPH_W 8
#define GLYPH_H 16
#define FRAME_W 640
#define FRAME_H 768
#define MAX_TEXT_COLS (FRAME_W / GLYPH_W)
#define MAX_TEXT_ROWS (FRAME_H / GLYPH_H)

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) { snprintf(project_root, sizeof(project_root), "%s", env); return; }
    if (!getcwd(project_root, sizeof(project_root))) snprintf(project_root, sizeof(project_root), ".");
}

/* Direct port of ops/compose_rgb_frame.c's own load_glyphs()/
 * blit_char()/blit_text() - see this file's own top-of-file comment.
 * Reads the SAME pieces/registry/fonts/ascii/<code>/glyph.txt registry
 * every project's own compose_rgb_frame.c already reads - genuinely
 * project-agnostic already (confirmed: mutaclsym's and zoo_0000's own
 * copies of this registry are the same shape), which is exactly why
 * this whole file belongs in shared-ops/ rather than being a 3rd
 * per-project duplicate. */
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

/* REAL emoji rendering, NOT a hash fallback - direct user correction
 * ("wraith-alpha renders rgb emojis just fine. why can we do what that
 * is doing? you should copy wraith-alpha as much as possible"). An
 * earlier version of this file stopped at flat colors only
 * (utf8_hash_color(), still below, now truly a last resort), reasoning
 * that wraith_rgb_daemon.c's own architecture doc saying it "does not
 * recognize <cli_io>/<button>/<text> tags" meant it didn't render real
 * emoji pixels either - WRONG, those are different claims. Real
 * wraith_rgb_daemon.c has zero .chtpm/tag awareness AND still renders
 * real emoji pixel art, via blit_codepoint()/get_emoji_bitmap()/
 * load_emoji_bitmap_from_disk() - a genuine 2-stage, offline-first
 * pipeline (real FreeType rasterization done ONCE by a separate tool,
 * the daemon just blits the pre-generated bitmap at runtime) - already
 * documented in ops/compose_rgb_frame.c's own header comment (which
 * ported the SAME mechanism for regular "run" mode), just never
 * applied here.
 *
 * This file now does the same, adapted the same way compose_rgb_frame.c
 * already adapted it: reuses THIS project's own pre-generated
 * pieces/registry/emoji_assets/<asset_id>/voxels_16.csv files (real
 * 16x16 RGBA pixel data, FreeType-rasterized once, not per-frame) -
 * see load_emoji_voxels()/blit_emoji_tile() below, ported directly
 * from compose_rgb_frame.c's own identically-named functions.
 *
 * THE ONE REAL DIFFERENCE FROM compose_rgb_frame.c's OWN LOOKUP: that
 * file keys emoji_assets/<asset_id>/ off its own INTERNAL single-char
 * terrain/furniture type code (read straight from game state), which
 * this file doesn't have - it only ever sees the ALREADY-RENDERED
 * emoji BYTES in current_frame.txt. Since every registry row already
 * maps id<->unicode 1:1, this builds the REVERSE lookup (unicode bytes
 * -> asset_id) once at startup by reading the SAME registries, instead
 * of receiving asset_id directly - a real, necessary adaptation for a
 * text-only-input daemon, not a shortcut. Covers all four registries
 * that have real emoji_assets/ entries (confirmed via directory
 * listing): terrain, furniture, items, monsters - the first two also
 * carry a real curated `rgb_top_emoji` flat color (used as the tile's
 * base color the voxel art is blitted on top of, matching
 * compose_rgb_frame.c's own exact layering); items/monsters have no
 * such field (per those registries' own header comments - "items/
 * monsters already fall back to the same flat glyph_fallback_rgb()
 * regardless of this toggle - a pre-existing limitation"), so they get
 * a neutral dark backdrop instead, real voxel art still on top. */
#define MAX_EMOJI_COLORS 128
#define EMOJI_BYTES_BUF 16
#define ASSET_ID_BUF 64
typedef struct {
    char bytes[EMOJI_BYTES_BUF]; int len;
    unsigned char r, g, b;
    char asset_id[ASSET_ID_BUF];
} EmojiColor;
static EmojiColor emoji_colors[MAX_EMOJI_COLORS];
static int emoji_color_count = 0;

/* terrain_types.txt/furniture_types.txt: glyph|id|name|walkable|
 * rgb_top|unicode|rgb_top_emoji - id is field[1], unicode is field[5],
 * rgb_top_emoji is field[6]. */
static void load_emoji_colors_from(const char *registry_rel_path) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/%s", project_root, registry_rel_path);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[512];
    while (fgets(line, sizeof(line), f) && emoji_color_count < MAX_EMOJI_COLORS) {
        /* A real comment line always has a space right after '#' (see
         * every header line in terrain_types.txt/furniture_types.txt
         * itself); a real DATA row can also legitimately start with
         * '#' when that's literally the glyph (terrain_types.txt's own
         * wall row: "#|t_wall|Wall|0|90,90,100|..."). The old
         * line[0]=='#' check treated that wall row as a comment and
         * silently dropped it, so walls never got a registry color and
         * fell back to whatever this daemon's own default/hash color
         * is - confirmed live as the real cause of "walls green instead
         * of brick". line[1]=='|' is what actually distinguishes a
         * pipe-delimited data row from a prose comment here. */
        if (line[0] == '\n' || (line[0] == '#' && line[1] != '|')) continue;
        line[strcspn(line, "\r\n")] = '\0';
        char *fields[7] = {0};
        int nf = 0;
        char *save = NULL;
        char *tok = strtok_r(line, "|", &save);
        while (tok && nf < 7) { fields[nf++] = tok; tok = strtok_r(NULL, "|", &save); }
        if (nf < 7) continue;
        const char *asset_id = fields[1];
        const char *unicode_str = fields[5];
        int r, g, b;
        if (sscanf(fields[6], "%d,%d,%d", &r, &g, &b) != 3) continue;
        size_t ulen = strlen(unicode_str);
        if (ulen == 0 || ulen >= EMOJI_BYTES_BUF) continue;
        EmojiColor *e = &emoji_colors[emoji_color_count++];
        snprintf(e->bytes, sizeof(e->bytes), "%s", unicode_str);
        e->len = (int)ulen;
        e->r = (unsigned char)r; e->g = (unsigned char)g; e->b = (unsigned char)b;
        snprintf(e->asset_id, sizeof(e->asset_id), "%s", asset_id);
    }
    fclose(f);
}

/* items.txt (id|name|category|glyph|weight|power|unicode, 7 fields,
 * id=field[0], unicode=field[6]) and monster_types.txt (id|name|glyph|
 * hp|damage|unicode, 6 fields, id=field[0], unicode=field[5]) - real,
 * different field counts/order than terrain/furniture (checked via
 * direct read of both files' own header comments, not assumed), and
 * neither has an rgb_top_emoji column at all - a fixed neutral
 * backdrop color is used instead, real voxel art still blitted on top
 * where an emoji_assets/<id>/ directory exists (confirmed via listing:
 * every item id and "zombie" both have one). */
static void load_emoji_assets_from(const char *registry_rel_path, int field_count, int unicode_field) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/%s", project_root, registry_rel_path);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[512];
    while (fgets(line, sizeof(line), f) && emoji_color_count < MAX_EMOJI_COLORS) {
        /* Same real comment-vs-data-row fix as load_emoji_colors_from()
         * above - see that function's own comment. */
        if (line[0] == '\n' || (line[0] == '#' && line[1] != '|')) continue;
        line[strcspn(line, "\r\n")] = '\0';
        char *fields[8] = {0};
        int nf = 0;
        char *save = NULL;
        char *tok = strtok_r(line, "|", &save);
        while (tok && nf < field_count) { fields[nf++] = tok; tok = strtok_r(NULL, "|", &save); }
        if (nf < field_count) continue;
        const char *asset_id = fields[0];
        const char *unicode_str = fields[unicode_field];
        size_t ulen = strlen(unicode_str);
        if (ulen == 0 || ulen >= EMOJI_BYTES_BUF) continue;
        EmojiColor *e = &emoji_colors[emoji_color_count++];
        snprintf(e->bytes, sizeof(e->bytes), "%s", unicode_str);
        e->len = (int)ulen;
        e->r = 60; e->g = 60; e->b = 60; /* neutral backdrop - see comment above */
        snprintf(e->asset_id, sizeof(e->asset_id), "%s", asset_id);
    }
    fclose(f);
}

/* Hero/xlector are hardcoded chars in every project's own compose_frame.c
 * (never registry-driven, per that file's own comment on
 * glyph_fallback_rgb()'s matching special-case), so they have no
 * terrain/furniture/items/monsters registry row for
 * load_emoji_colors_from() to find - added directly here instead, real
 * curated colors matching glyph_fallback_rgb()'s own yellow/cyan
 * exactly (255,255,0 / 0,255,255), asset_id matching the real,
 * confirmed-present pieces/registry/emoji_assets/hero/ and .../xlector/
 * voxel directories. Safe to hardcode (not a chrome-text collision risk
 * like plain ASCII '@'/'X' would be - see this daemon's earlier,
 * mistaken reasoning on that point in project memory): 🧑/🎯 are real
 * multi-byte emoji compose_frame.c only ever emits for the hero/
 * xlector cell specifically, never appearing in ordinary chrome text. */
static void add_hardcoded_emoji_color(const char *bytes, const char *asset_id,
                                      unsigned char r, unsigned char g, unsigned char b) {
    if (emoji_color_count >= MAX_EMOJI_COLORS) return;
    EmojiColor *e = &emoji_colors[emoji_color_count++];
    snprintf(e->bytes, sizeof(e->bytes), "%s", bytes);
    e->len = (int)strlen(bytes);
    e->r = r; e->g = g; e->b = b;
    snprintf(e->asset_id, sizeof(e->asset_id), "%s", asset_id);
}

static void load_emoji_colors(void) {
    emoji_color_count = 0;
    load_emoji_colors_from("pieces/registry/terrain/terrain_types.txt");
    load_emoji_colors_from("pieces/registry/furniture/furniture_types.txt");
    load_emoji_assets_from("pieces/registry/items/items.txt", 7, 6);
    load_emoji_assets_from("pieces/registry/monsters/monster_types.txt", 6, 5);
    add_hardcoded_emoji_color("🧑", "hero", 255, 255, 0);
    add_hardcoded_emoji_color("🎯", "xlector", 0, 255, 255);
}

/* Longest-match-first lookup at text position p - some registry
 * entries are TWO codepoints (e.g. stairs "⬇️"/"⬆️" = base arrow +
 * VS16 variation selector, 6 bytes total) that display as ONE visual
 * character in the game's own text grid. Matching the registry's own
 * full known string first (rather than decoding one codepoint at a
 * time) keeps column alignment correct for those - a naive per-
 * codepoint decode would consume the VS16 as its OWN separate column,
 * shifting everything after it. Returns byte length consumed on match
 * (0 = no match, caller falls back to plain UTF-8 decode); *asset_id_out
 * is set to the matched entry's own asset_id (empty string if the
 * caller shouldn't attempt a voxel-art blit, though every match here
 * always has one - kept as an explicit out-param rather than assuming
 * so a future registry addition without emoji_assets/ coverage fails
 * safe, falling back to the flat color only). */
static int match_emoji_color(const char *p, unsigned char *r, unsigned char *g, unsigned char *b, const char **asset_id_out) {
    int best_len = 0;
    const EmojiColor *best = NULL;
    for (int i = 0; i < emoji_color_count; i++) {
        int elen = emoji_colors[i].len;
        if (elen <= best_len) continue;
        if (strncmp(p, emoji_colors[i].bytes, (size_t)elen) == 0) { best_len = elen; best = &emoji_colors[i]; }
    }
    if (!best) return 0;
    *r = best->r; *g = best->g; *b = best->b;
    *asset_id_out = best->asset_id;
    return best_len;
}

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

/* Solid `w` x GLYPH_H block, no font shape - the fallback used for any
 * non-ASCII (multi-byte UTF-8) codepoint below. Deliberately NOT a
 * font glyph: this file has no emoji bitmap font, and reusing ops/
 * compose_rgb_frame.c's own glyph_to_rgb() isn't possible here - that
 * function keys off compose_rgb_frame.c's OWN internal single-char
 * terrain/furniture type codes (read straight from game state files),
 * not the already-rendered emoji bytes this file reads out of
 * current_frame.txt, which by this point have no type info left.
 * Width is a parameter, not the fixed GLYPH_W - see blit_text()'s own
 * comment on why tile-glyph cells and plain-text cells now use
 * DIFFERENT widths (VOXEL_PX=16 vs GLYPH_W=8). */
static void blit_solid_block(unsigned char fb[FRAME_H][FRAME_W][4], int px, int py, int w,
                             unsigned char r, unsigned char g, unsigned char b) {
    for (int y = 0; y < GLYPH_H; y++) {
        int fy = py + y;
        if (fy < 0 || fy >= FRAME_H) continue;
        for (int x = 0; x < w; x++) {
            int fx = px + x;
            if (fx < 0 || fx >= FRAME_W) continue;
            fb[fy][fx][0] = r;
            fb[fy][fx][1] = g;
            fb[fy][fx][2] = b;
            fb[fy][fx][3] = 255;
        }
    }
}

/* Direct port of ops/compose_rgb_frame.c's own load_emoji_voxels() -
 * reads a real, pre-generated 16x16 RGBA voxel CSV (see this file's
 * own "REAL emoji rendering" comment above for the full FreeType-
 * offline-pipeline background). Single-entry cache, same reasoning as
 * the original: adjacent map cells are very often the same asset_id
 * (a run of floor tiles), and this is already fopen-per-call registry-
 * lookup-adjacent code, matching the family's "correctness over
 * micro-optimization at this file scale" convention. */
#define VOXEL_PX 16
static int load_emoji_voxels(const char *asset_id, unsigned char voxels[VOXEL_PX][VOXEL_PX][4]) {
    static char cached_id[ASSET_ID_BUF] = "";
    static unsigned char cached_voxels[VOXEL_PX][VOXEL_PX][4];
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
        if (idx >= VOXEL_PX * VOXEL_PX) break;
        int vy = idx / VOXEL_PX, vx = idx % VOXEL_PX;
        voxels[vy][vx][0] = (unsigned char)r;
        voxels[vy][vx][1] = (unsigned char)g;
        voxels[vy][vx][2] = (unsigned char)b;
        voxels[vy][vx][3] = (unsigned char)a;
        idx++;
    }
    fclose(f);
    if (idx != VOXEL_PX * VOXEL_PX) return 0;

    snprintf(cached_id, sizeof(cached_id), "%s", asset_id);
    memcpy(cached_voxels, voxels, sizeof(cached_voxels));
    cache_valid = 1;
    return 1;
}

/* Direct port of compose_rgb_frame.c's own blit_emoji_tile() - blits
 * 1:1, NO scaling, into a SQUARE VOXEL_PX(16)x16 cell, exactly like
 * the original. A PREVIOUS version of this function downscaled 2:1 to
 * fit this file's own then-uniform GLYPH_W(8)-wide text columns -
 * WRONG, per direct user comparison against a real screenshot: it
 * squished every icon to half width and, since VIEWPORT_W(40) *
 * VOXEL_PX(16) = 640 = FRAME_W exactly, was also structurally
 * unnecessary - tile rows already fit the full frame width perfectly
 * at NATIVE size with zero scaling needed. blit_text() below now
 * tracks a running PIXEL x-position instead of a fixed column count,
 * advancing by VOXEL_PX(16) for a matched tile glyph and GLYPH_W(8)
 * for plain text - real wraith-alpha's own precedent for this
 * ("its daemon targets a variable emoji_glyph_size", per
 * compose_rgb_frame.c's own citation), not a from-scratch invention.
 * Same alpha-composite-onto-flat-color shape as the original
 * (transparent voxel pixels show the flat base color underneath, not
 * black) - confirmed real wraith_rgb_daemon.c's own blit_codepoint()
 * does the same "blit with alpha testing" shape, just per-tile there. */
static void blit_emoji_tile(unsigned char fb[FRAME_H][FRAME_W][4], int px, int py,
                             unsigned char voxels[VOXEL_PX][VOXEL_PX][4]) {
    for (int y = 0; y < VOXEL_PX; y++) {
        int fy = py + y;
        if (fy < 0 || fy >= FRAME_H) continue;
        for (int x = 0; x < VOXEL_PX; x++) {
            int fx = px + x;
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

/* How many bytes a UTF-8 sequence starting with `lead` occupies - 1 for
 * plain ASCII or an invalid/continuation lead byte (treated as a lone
 * byte so a corrupt stream can't desync column counting forever), 2-4
 * for real multi-byte lead bytes (mutaclsym/zoo_0000's own tile emoji,
 * e.g. \xf0\x9f\xa7\xb1 = "wall", are all in the 4-byte range). */
static int utf8_seq_len(unsigned char lead) {
    if ((lead & 0x80) == 0x00) return 1;
    if ((lead & 0xE0) == 0xC0) return 2;
    if ((lead & 0xF0) == 0xE0) return 3;
    if ((lead & 0xF8) == 0xF0) return 4;
    return 1;
}

/* Deterministic color from a UTF-8 sequence's own bytes (same FNV-1a
 * shape as checksum_buffer() below) - not a real per-tile color chart
 * (this file has none), but two different emoji reliably get two
 * different colors, so distinct tile types stay visually distinct
 * rather than all collapsing into identical gray blocks. */
static void utf8_hash_color(const char *bytes, int len, unsigned char *r, unsigned char *g, unsigned char *b) {
    uint64_t h = 14695981039346656037ULL;
    for (int i = 0; i < len; i++) { h ^= (unsigned char)bytes[i]; h *= 1099511628211ULL; }
    *r = (unsigned char)(80 + (h & 0x7F));
    *g = (unsigned char)(80 + ((h >> 8) & 0x7F));
    *b = (unsigned char)(80 + ((h >> 16) & 0x7F));
}

/* Tracks a running PIXEL x-position, not a fixed-width column count -
 * REAL FIX for a real, user-reported regression ("emojis look bad
 * compared to how they used to look... getting cut off and jacked
 * up"). A previous version advanced by a fixed GLYPH_W(8) per glyph
 * REGARDLESS of type, forcing every matched tile glyph's own native
 * 16x16 art to be downscaled 2:1 to fit - visibly squished, AND (since
 * VIEWPORT_W(40) * VOXEL_PX(16) = 640 = FRAME_W exactly) completely
 * unnecessary: a full map row only needs 40 native-width tile cells to
 * exactly fill the frame, not 80 half-width ones. Plain ASCII text
 * still advances by GLYPH_W(8) (chrome/footer lines need the full ~80-
 * column budget that width gives - confirmed against game.chtpm's own
 * longest footer line, ~72 chars, which would truncate at a uniform
 * 16px/col width). Real wraith-alpha's own precedent for mixing widths
 * within one daemon, not invented here: "its daemon targets a variable
 * emoji_glyph_size" (compose_rgb_frame.c's own citation). Stops on
 * PIXEL width (x_px < FRAME_W), not a column count, since the two
 * widths make a fixed max-column number meaningless for a mixed line. */
static void blit_text(unsigned char fb[FRAME_H][FRAME_W][4], int px, int py, const char *text,
                      unsigned char r, unsigned char g, unsigned char b) {
    int x_px = px;
    const char *p = text;
    while (*p && x_px < FRAME_W) {
        unsigned char lead = (unsigned char)*p;
        if (lead >= 0x80) {
            unsigned char tr, tg, tb;
            const char *asset_id = NULL;
            int matched_len = match_emoji_color(p, &tr, &tg, &tb, &asset_id);
            if (matched_len > 0) {
                blit_solid_block(fb, x_px, py, VOXEL_PX, tr, tg, tb);
                unsigned char voxels[VOXEL_PX][VOXEL_PX][4];
                if (asset_id[0] && load_emoji_voxels(asset_id, voxels)) {
                    blit_emoji_tile(fb, x_px, py, voxels);
                }
                p += matched_len;
                x_px += VOXEL_PX;
                continue;
            }
            int seqlen = utf8_seq_len(lead);
            utf8_hash_color(p, seqlen, &tr, &tg, &tb);
            blit_solid_block(fb, x_px, py, VOXEL_PX, tr, tg, tb);
            p += seqlen;
            x_px += VOXEL_PX;
            continue;
        }
        blit_char(fb, x_px, py, lead, r, g, b);
        p++;
        x_px += GLYPH_W;
    }
}

/* Same FNV-1a-64 algorithm every other RGB-writing op in this family
 * already uses (ops/compose_rgb_frame.c's own copy, cited there as
 * matching real wraith_gl.c's checksum_buffer()) - so a checksum
 * written here is directly comparable to what dump_rgb_png.c or a
 * human reads elsewhere. */
static uint64_t checksum_buffer(const unsigned char *buf, size_t len) {
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < len; i++) {
        hash ^= buf[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

/* Write to <path>.tmp then rename() over <path> - REAL FIX for a real,
 * user-reported bug ("the gl screen randomly disappears when enter is
 * pressed"). This daemon polls and rewrites rgb_frame.raw far more
 * often (~60/sec) than ops/compose_rgb_frame.c ever did (only on a
 * keypress, gated behind the module's own .pal loop), so a pre-
 * existing latent pattern in this family - writing the real output
 * path directly via a plain fopen(path,"w"/"wb"), which TRUNCATES to
 * 0 bytes before any new content is written - became visibly
 * reachable here. Confirmed via direct read of system/gl_mirror.c's
 * own load_texture(): it does a plain fread() with no retry, and on a
 * short read (exactly what happens if it opens the file in the window
 * between this daemon's own truncate and the completion of its write)
 * zero-fills the REST of the buffer and uploads that as the frame -
 * a real, textbook torn-read race, not a guess. POSIX rename() onto
 * the same filesystem is atomic - any reader sees either the complete
 * OLD file or the complete NEW one, never a partial one in between. */
static void write_file_atomic(const char *path, const void *data, size_t len) {
    char tmp_path[PATH_BUF];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    FILE *f = fopen(tmp_path, "wb");
    if (!f) return;
    fwrite(data, 1, len, f);
    fclose(f);
    rename(tmp_path, path);
}

/* Grows the DOWNSTREAM "a real new rgb_frame.raw is ready" pulse -
 * see this file's own main()'s header comment on why this is a
 * genuinely separate file from pieces/display/frame_changed.txt (the
 * UPSTREAM trigger this daemon itself watches), matching real
 * wraith_rgb_daemon.c's own pulse_rgb()/rgb_frame_changed.txt
 * two-stage pattern exactly. Called only AFTER rgb_frame.raw and its
 * receipt have both been written - a reader (system/gl_mirror.c) that
 * only reacts to THIS file is guaranteed the real output is complete,
 * not just "the upstream parser did something." */
static void pulse_rgb_ready(const char *path) {
    FILE *f = fopen(path, "a");
    if (f) { fputc('P', f); fputc('\n', f); fclose(f); }
}

static void write_receipt(const char *path, size_t byte_count, uint64_t checksum) {
    char buf[512];
    int n = snprintf(buf, sizeof(buf),
        "op=chtpm_rgb_render\n"
        "frame_w=%d\n"
        "frame_h=%d\n"
        "bytes_per_pixel=4\n"
        "byte_count=%zu\n"
        "expected_byte_count=%d\n"
        "checksum_fnv1a64=%016llx\n"
        "max_text_cols=%d\n"
        "max_text_rows=%d\n",
        FRAME_W, FRAME_H, byte_count, FRAME_W * FRAME_H * 4,
        (unsigned long long)checksum, MAX_TEXT_COLS, MAX_TEXT_ROWS);
    if (n > 0) write_file_atomic(path, buf, (size_t)n);
}

static long file_size(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (long)st.st_size;
}

static int read_kv_int_generic(const char *path, const char *key, int def) {
    FILE *f = fopen(path, "r");
    if (!f) return def;
    char line[256];
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

/* MAP3D OVERLAY COMPOSITING - real, direct instruction (2026-07-17):
 * lets a project with its own real game-state-aware 3D renderer (this
 * file has none by design - it's a genuinely shared, project-agnostic
 * text rasterizer) composite a separately-rendered region into this
 * daemon's own output, WITHOUT this file needing to know anything
 * about that project's own game state, camera, or map format.
 *
 * Protocol (deliberately generic, not mutaclsym-specific): a project's
 * own text-composing op (e.g. ops/compose_frame.c) may emit a single
 * 0x01 (SOH) byte as the entire content of one line, anywhere in
 * current_frame.txt. When this daemon encounters that byte as a
 * line's first character, it does NOT rasterize that line as text -
 * instead it reads pieces/display/rgb_frame_3d_overlay.raw (a flat
 * RGBA8888 buffer) + pieces/display/rgb_frame_3d_overlay.receipt.txt
 * (`overlay_w=`/`overlay_h=` in pixels), blits it verbatim at
 * (x=0, y=current_row*GLYPH_H), and skips ahead in its own line
 * counter by however many text-rows-worth of vertical space the
 * overlay occupies (round up, so a following line - like an HP/footer
 * row - still lands on a real row boundary) - the marker's own
 * producer is expected to have also skipped writing that many map
 * rows as text (see ops/compose_frame.c's own MAP3D_MARKER emission),
 * so nothing is double-counted. Any project that never emits this
 * marker byte (the common case) sees ZERO behavior change - this is a
 * pure opt-in. */
static void blit_overlay(unsigned char fb[FRAME_H][FRAME_W][4], int y0) {
    char overlay_path[PATH_BUF], receipt_path[PATH_BUF];
    snprintf(overlay_path, sizeof(overlay_path), "%s/pieces/display/rgb_frame_3d_overlay.raw", project_root);
    snprintf(receipt_path, sizeof(receipt_path), "%s/pieces/display/rgb_frame_3d_overlay.receipt.txt", project_root);
    int ov_w = read_kv_int_generic(receipt_path, "overlay_w", 0);
    int ov_h = read_kv_int_generic(receipt_path, "overlay_h", 0);
    if (ov_w <= 0 || ov_h <= 0) return;

    FILE *f = fopen(overlay_path, "rb");
    if (!f) return;
    unsigned char *buf = malloc((size_t)ov_w * ov_h * 4);
    if (!buf) { fclose(f); return; }
    size_t got = fread(buf, 1, (size_t)ov_w * ov_h * 4, f);
    fclose(f);
    if (got != (size_t)ov_w * ov_h * 4) { free(buf); return; }

    for (int oy = 0; oy < ov_h; oy++) {
        int fy = y0 + oy;
        if (fy < 0 || fy >= FRAME_H) continue;
        for (int ox = 0; ox < ov_w && ox < FRAME_W; ox++) {
            unsigned char *src = &buf[(oy * ov_w + ox) * 4];
            fb[fy][ox][0] = src[0];
            fb[fy][ox][1] = src[1];
            fb[fy][ox][2] = src[2];
            fb[fy][ox][3] = 255;
        }
    }
    free(buf);
}

/* Reads pieces/display/current_frame.txt (whatever chtpm_parser_pal.c
 * most recently wrote - chrome AND embedded ${game_map}, all of it,
 * exactly as real wraith_rgb_daemon.c reads its own project's
 * current_frame.txt) and rasterizes every line, black background,
 * plain white text - no color-coding of chrome vs content, matching
 * wraith_rgb_daemon.c's own "no semantic awareness of tags" property
 * exactly: it doesn't know or care which characters are a button
 * label versus game content, it just blits them all uniformly. See
 * blit_overlay()'s own header comment for the one real, opt-in
 * exception to that (a 0x01 marker line). */
static void render_once(unsigned char fb[FRAME_H][FRAME_W][4], const char *frame_path) {
    memset(fb, 0, (size_t)FRAME_H * FRAME_W * 4);
    /* Alpha defined opaque everywhere up front, matching ops/
     * compose_rgb_frame.c's own established convention exactly (see
     * that file's own comment at its identical memset+alpha-fill site)
     * - neither gl_mirror.c nor real wraith_gl.c enables GL_BLEND, so
     * this doesn't change what the real GL window shows (RGB is what
     * matters there, alpha is ignored) - but leaving alpha=0 on
     * background pixels makes any PNG dumped via dump_rgb_png.+x for
     * human/agent debugging render as if transparent (most viewers
     * show alpha=0 as white), making opaque white text pixels
     * genuinely indistinguishable from the "transparent" background
     * in that debug view even though the real GL window is fine. */
    for (int y = 0; y < FRAME_H; y++)
        for (int x = 0; x < FRAME_W; x++)
            fb[y][x][3] = 255;

    FILE *f = fopen(frame_path, "r");
    if (!f) return;

    char line[512];
    int row = 0;
    while (row < MAX_TEXT_ROWS && fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if ((unsigned char)line[0] == 0x01) {
            /* MAP3D_MARKER - see blit_overlay()'s own header comment.
             * Don't rasterize the marker itself. */
            char receipt_path[PATH_BUF];
            snprintf(receipt_path, sizeof(receipt_path), "%s/pieces/display/rgb_frame_3d_overlay.receipt.txt", project_root);
            int ov_h = read_kv_int_generic(receipt_path, "overlay_h", 0);
            int skip_rows = (ov_h > 0) ? (ov_h + GLYPH_H - 1) / GLYPH_H : 0;

            blit_overlay(fb, row * GLYPH_H);

            /* Consume (without rasterizing) whatever the producer's own
             * marker-emission convention put in place of the map rows
             * this overlay replaces, then continue from a real row
             * boundary. */
            for (int i = 0; i < skip_rows; i++) {
                if (!fgets(line, sizeof(line), f)) break;
            }
            row += skip_rows;
            continue;
        }
        blit_text(fb, 0, row * GLYPH_H, line, 255, 255, 255);
        row++;
    }
    fclose(f);
}

int main(void) {
    resolve_root();
    load_glyphs();
    load_emoji_colors();

    char frame_path[PATH_BUF], pulse_path[PATH_BUF];
    char out_path[PATH_BUF], receipt_path[PATH_BUF], rgb_pulse_path[PATH_BUF];
    snprintf(frame_path, sizeof(frame_path), "%s/pieces/display/current_frame.txt", project_root);
    snprintf(pulse_path, sizeof(pulse_path), "%s/pieces/display/frame_changed.txt", project_root);
    snprintf(out_path, sizeof(out_path), "%s/pieces/display/rgb_frame.raw", project_root);
    snprintf(receipt_path, sizeof(receipt_path), "%s/pieces/display/rgb_frame.receipt.txt", project_root);
    /* REAL FIX (direct user redirect: "wraith creates a rgb file that
     * rgb parser simply reads. aren't we doing the same? if so, check
     * the receipts" - checking them found this): real wraith_gl.c
     * does NOT watch pieces/display/frame_changed.txt (the UPSTREAM
     * trigger this daemon itself watches for input) - it watches its
     * own SEPARATE, DOWNSTREAM `rgb_frame_changed.txt`
     * (WRAITH_FRAME_TRIGGER, confirmed via direct read), grown ONLY by
     * wraith_rgb_daemon.c's own pulse_rgb(), AFTER it finishes writing
     * its output. gl_mirror.c here used to watch the SAME upstream
     * `frame_changed.txt` this daemon ALSO watches - a real,
     * confirmed bug, not a guess: measured via a live checksum trace
     * (rgb_frame.receipt.txt's own checksum vs. gl_display.receipt.txt's
     * loaded checksum, sampled every 30ms through a burst of
     * keypresses) that gl_mirror can "consume" an upstream trigger
     * BEFORE this daemon finishes its own settle-and-render, then
     * never notice the real output is ready - not a brief lag, a
     * PERMANENT stall until some unrelated later trigger happens to
     * fire again. Fixed by growing this SEPARATE file, matching
     * wraith's own naming, only once a real new rgb_frame.raw is
     * actually complete - gl_mirror.c now watches THIS instead. */
    snprintf(rgb_pulse_path, sizeof(rgb_pulse_path), "%s/pieces/display/rgb_frame_changed.txt", project_root);

    /* REAL BUG FIX (user-reported "GL window lagged behind one frame for
     * transition" - wsr-pal): a module-driven screen change (chtpm
     * noticing active_target_id changed via a relayed GOTO: command,
     * reloading ${piece_methods} for the new screen on its own
     * state_changed.txt-triggered reload path) does NOT grow
     * frame_changed.txt - only chtpm_parser_pal.c's own KEYPRESS-driven
     * process_key() does that. This daemon, like system/renderer.c
     * before its own fix, only ever watched frame_changed.txt - so the
     * CORRECTED current_frame.txt (chtpm's own module-triggered
     * recompose) sat on disk unseen until an unrelated later keypress
     * happened to grow frame_changed.txt too.
     *
     * CORRECTION (found live via a NEW brief-black-flash regression in
     * mutaclsym after sec. 0's own synchronous-dispatch fix - state_
     * changed.txt was the WRONG marker to add here): state_changed.txt
     * is grown by a PROJECT'S OWN compose op BEFORE chtpm has reloaded/
     * recomposed anything - it is a pre-trigger, not a completion
     * signal. For a project whose own module ALSO writes
     * current_frame.txt directly as part of the same op (mutaclsym's
     * own compose_frame.c, sec. 16.3/0.1's own dual-write pattern -
     * NEEDED there, not a bug), that op grows state_changed.txt too,
     * so watching it here caught the module's own INTERMEDIATE,
     * chrome-less write and rendered THAT into rgb_frame.raw, before
     * chtpm's own subsequent correction landed - a real, visible flash,
     * confirmed live once synchronous dispatch made the two writes
     * happen close enough together to actually be caught.
     *
     * THE REAL FIX: chtpm_parser_pal.c's own internal compose_frame()
     * (shared-ops/chtpm_parser_pal.c ~line 2575) grows
     * pieces/display/renderer_pulse.txt UNCONDITIONALLY, every single
     * time it runs - regardless of whether a keypress or a
     * state_changed.txt-triggered reload caused it. This is the
     * authoritative "the CORRECT, chrome-composed current_frame.txt is
     * ready" signal chtpm itself already emits - watch THIS instead of
     * state_changed.txt: it never fires early, and never fires for a
     * project's own intermediate/direct write, only for chtpm's own
     * finished composition. The existing settle-loop below still
     * handles the "is the write actually finished" concern for either
     * trigger identically. */
    char renderer_pulse_path[PATH_BUF];
    snprintf(renderer_pulse_path, sizeof(renderer_pulse_path), "%s/pieces/display/renderer_pulse.txt", project_root);

    static unsigned char fb[FRAME_H][FRAME_W][4];

    /* Render once unconditionally at startup, same reasoning every
     * other renderer/daemon in this family already uses - so
     * rgb_frame.raw exists and reflects the real starting state before
     * the first marker-file growth. */
    render_once(fb, frame_path);
    size_t byte_count = (size_t)FRAME_W * FRAME_H * 4;
    write_file_atomic(out_path, fb, byte_count);
    write_receipt(receipt_path, byte_count, checksum_buffer((unsigned char *)fb, byte_count));
    pulse_rgb_ready(rgb_pulse_path);

    long last_pulse = file_size(pulse_path);
    long last_renderer_pulse = file_size(renderer_pulse_path);

    /* Same "poll a marker file's SIZE, never mtime" shape system/
     * renderer.c and system/gl_mirror.c already use in this family -
     * not a new invention, the already-proven convention.
     *
     * REAL FIX (direct user report: GL screen shows stale/mixed
     * content, specifically right after pressing Enter - reproduced
     * twice via direct testing, not a guess): chtpm_parser_pal.c grows
     * frame_changed.txt (this daemon's own "go re-render" signal)
     * and writes current_frame.txt (the actual content) as two
     * SEPARATE, unsynchronized writes - real 1.TPMOS code this project
     * keeps a deliberately minimal-diff fork of (see shared-ops/
     * chtpm_parser_pal.c's own header comment on its "exactly 2
     * patches" policy), not something to patch further for this. The
     * old code here rendered the INSTANT it saw the pulse grow, which
     * could catch current_frame.txt mid-write (confirmed live: a
     * render right after an Enter-triggered pulse growth showed a
     * MIX of the old and new footer text). A first fix used a fixed
     * 30ms delay after the pulse changed - a real improvement (direct
     * user confirmation: "no where near as bad"), but not fully
     * robust, since it's a blind delay tied to an UNRELATED file
     * (frame_changed.txt), not an actual check on current_frame.txt
     * itself. Checked real wraith_rgb_daemon.c directly, per explicit
     * instruction, for a better pattern to copy - it has NEITHER an
     * atomic write NOR any settle delay at all (`main()`'s own loop
     * renders the instant its trigger file changes, no grace period)
     * - this exact race is LATENT in real wraith-alpha too, not
     * something to copy from there; its own upstream writer likely
     * just finishes fast enough in practice to rarely expose it.
     *
     * REAL FIX: wait for current_frame.txt ITSELF to stop changing
     * size, not a fixed delay on a proxy file - poll its own size
     * repeatedly until it reads the same value twice in a row (capped
     * at 20 attempts / ~200ms so a genuinely stuck writer can't hang
     * this daemon forever) before reading its content. Directly
     * verifies the actual thing being read is done changing, rather
     * than guessing how long that takes. */
    for (;;) {
        long m = file_size(pulse_path);
        long sm = file_size(renderer_pulse_path);
        if (m != last_pulse || sm != last_renderer_pulse) {
            last_pulse = m;
            last_renderer_pulse = sm;
            long prev_frame_size = -2, cur_frame_size = file_size(frame_path);
            for (int attempt = 0; attempt < 20 && cur_frame_size != prev_frame_size; attempt++) {
                prev_frame_size = cur_frame_size;
                usleep(10000);
                cur_frame_size = file_size(frame_path);
            }
            render_once(fb, frame_path);
            write_file_atomic(out_path, fb, byte_count);
            write_receipt(receipt_path, byte_count, checksum_buffer((unsigned char *)fb, byte_count));
            pulse_rgb_ready(rgb_pulse_path);
        }
        usleep(33333); /* 30 fps cap */
    }
    return 0;
}
