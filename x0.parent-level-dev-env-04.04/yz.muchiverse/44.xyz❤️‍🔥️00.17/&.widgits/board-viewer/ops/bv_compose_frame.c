/* bv_compose_frame - renders board-viewer's current screen into
 * pieces/apps/player_app/view.txt.
 *
 * P4+P5 (per &.widgits/BOARD_WIDGET_PROGRESS.txt, combined per direct
 * user priority to get real board+camera+selector testable soon):
 * reads the FOCUSED HOST's real board.txt directly (real-time sync via
 * direct file read, per @.apps/BOARD_WIDGET_ARCHITECTURE.md §4 - no
 * new IPC, just knowing which root to read), and renders a
 * camera-clamped viewport around the widget's own selector cursor
 * position - the exact clamp math from &.widgits/5-pov-widgit.md §7
 * (ported from mutaclysm's own ops/compose_frame.c camera-clamp,
 * confirmed working there): cam = clamp(selector - VIEWPORT/2, 0,
 * board_dim - VIEWPORT).
 *
 * render_mode/camera_mode/emoji_mode (5-pov-widgit.md §2) are NOT yet
 * implemented - this is the render_mode==0 (2D flat) path only, always
 * active for now. 3D raymarch extrusion is P7/P8, a separate later
 * phase.
 *
 * Self-contained, no shared headers.
 * Usage: bv_compose_frame.+x (no args) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define access _access
#define F_OK 0
#endif

#define MAX_LINE 512
#ifndef MAX_PATH
#define MAX_PATH 4096
#endif
#define PATH_BUF (MAX_PATH + 256)
#define BOX_W 60
#define VIEWPORT_W 16
#define VIEWPORT_H 10
#define MAX_BOARD_DIM 64

static char project_root[MAX_PATH] = ".";
static char house_root[MAX_PATH] = "";

/* UTF-8 path open (emoji house / assets) — MinGW ANSI fopen fails on Unicode. */
static FILE *host_fopen(const char *path, const char *mode) {
#ifdef _WIN32
    wchar_t wpath[PATH_BUF], wmode[16];
    if (!path || !mode) return NULL;
    if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, PATH_BUF) <= 0) {
        if (MultiByteToWideChar(CP_ACP, 0, path, -1, wpath, PATH_BUF) <= 0)
            return fopen(path, mode);
    }
    if (MultiByteToWideChar(CP_UTF8, 0, mode, -1, wmode, 16) <= 0)
        MultiByteToWideChar(CP_ACP, 0, mode, -1, wmode, 16);
    FILE *f = _wfopen(wpath, wmode);
    if (f) return f;
    return fopen(path, mode); /* last resort */
#else
    return fopen(path, mode);
#endif
}

static void resolve_root(void) {
#ifdef _WIN32
    if (access("pieces", F_OK) == 0) {
        snprintf(project_root, sizeof(project_root), ".");
        return;
    }
#endif
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

/* Join house_root + relative host path; leave absolute paths as-is. */
static void resolve_host_root(const char *raw, char *out, size_t out_sz) {
    out[0] = '\0';
    if (!raw || !raw[0]) return;
    /* absolute Win / Unix */
    if ((raw[0] && raw[1] == ':') || raw[0] == '/' || raw[0] == '\\') {
        snprintf(out, out_sz, "%s", raw);
        return;
    }
    /* relative to house (preferred for Win emoji paths) */
    if (house_root[0]) {
        snprintf(out, out_sz, "%s/%s", house_root, raw);
        return;
    }
    snprintf(out, out_sz, "%s", raw);
}

static void load_house_root(void) {
    house_root[0] = '\0';
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/house_root.txt", project_root);
    FILE *f = host_fopen(path, "r");
    if (!f) f = fopen(path, "r");
    if (!f) return;
    if (fgets(house_root, sizeof(house_root), f)) {
        /* strip UTF-8 BOM if present */
        if ((unsigned char)house_root[0] == 0xEF &&
            (unsigned char)house_root[1] == 0xBB &&
            (unsigned char)house_root[2] == 0xBF) {
            memmove(house_root, house_root + 3, strlen(house_root + 3) + 1);
        }
        house_root[strcspn(house_root, "\r\n")] = '\0';
    }
    fclose(f);
}

static void read_kv_str(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = host_fopen(path, "r");
    if (!f) f = fopen(path, "r");
    if (!f) return;
    char l[MAX_LINE];
    size_t key_len = strlen(key);
    int first = 1;
    while (fgets(l, sizeof(l), f)) {
        char *line = l;
        /* strip BOM on first line only */
        if (first) {
            first = 0;
            if ((unsigned char)line[0] == 0xEF &&
                (unsigned char)line[1] == 0xBB &&
                (unsigned char)line[2] == 0xBF)
                line += 3;
        }
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            char *v = line + key_len + 1;
            v[strcspn(v, "\r\n")] = '\0';
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(out, out_sz, "%s", v);
#pragma GCC diagnostic pop
        }
    }
    fclose(f);
}

static int read_kv_int(const char *path, const char *key, int def) {
    char buf[64];
    read_kv_str(path, key, buf, sizeof(buf));
    return buf[0] ? atoi(buf) : def;
}

/* Same real fix as bv_render_3d.c/bv_menu_input.c's own
 * default_current_z() (2026-08-04, direct user report: "2d emoji mode
 * starts in underground, should be above ground"). */
static int default_current_z(const char *root) {
    if (!root || !root[0]) return 0;
    char hero_state_path[PATH_BUF];
    snprintf(hero_state_path, sizeof(hero_state_path), "%s/pieces/hero_01/state.txt", root);
    int hero_pos_z = read_kv_int(hero_state_path, "pos_z", 0);
    if (!hero_pos_z) return 0;
    /* REAL FOLLOW-UP FIX 2026-08-04, direct user report ("broke the map
     * renders for 2d and 3d - blank/no emoji"): hero pos_z is the AIR
     * tile the hero stands IN (pc_generate_chunk.c's own real spawn
     * comment: "stands ON it, one level higher"), not the ground - a
     * fresh 2D session defaulting straight onto that tile shows a real,
     * correctly-rendered, entirely EMPTY Z-slice (every glyph really is
     * air there), which reads exactly like a broken/blank view even
     * though nothing crashed. Real fix: default one level BELOW the
     * hero, onto the real ground surface itself. */
    return hero_pos_z - 1;
}

/* --- entity rendering, part 1 (read-only, no click-to-select yet) ---
 * Generic, project-agnostic manifest a host project writes: this file
 * never knows about "units"/"civs"/professions, only positions + a
 * render glyph/color the host already computed. See @.apps/tactics-txt/
 * ops/tactics_menu_input.c's own CONFIRM_START comment for the real
 * format this reads: entity_id|pos_x|pos_y|unicode_hex|r|g|b|owner_id */
#define MAX_ENTITIES 64
typedef struct {
    int pos_x, pos_y;
    char utf8[8];
    unsigned char r, g, b;
} Entity;
static Entity g_entities[MAX_ENTITIES];
static int g_entity_count = 0;

/* Real UTF-8 encoder for a codepoint given as a hex string (e.g.
 * "1F5E1") - standard encoding, same byte-count rules chtpm_rgb_
 * render.c's own decode_utf8_codepoint() uses in reverse. */
static void hex_codepoint_to_utf8(const char *hex, char *out, size_t out_sz) {
    unsigned int cp = (unsigned int)strtoul(hex, NULL, 16);
    if (cp < 0x80) {
        if (out_sz >= 2) { out[0] = (char)cp; out[1] = '\0'; }
    } else if (cp < 0x800) {
        if (out_sz >= 3) {
            out[0] = (char)(0xC0 | (cp >> 6));
            out[1] = (char)(0x80 | (cp & 0x3F));
            out[2] = '\0';
        }
    } else if (cp < 0x10000) {
        if (out_sz >= 4) {
            out[0] = (char)(0xE0 | (cp >> 12));
            out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
            out[2] = (char)(0x80 | (cp & 0x3F));
            out[3] = '\0';
        }
    } else {
        if (out_sz >= 5) {
            out[0] = (char)(0xF0 | (cp >> 18));
            out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
            out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
            out[3] = (char)(0x80 | (cp & 0x3F));
            out[4] = '\0';
        }
    }
}

static void load_entities(const char *root) {
    g_entity_count = 0;
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/entities.txt", root);
    /* host_fopen: absolute host root may contain emoji (Win ANSI fopen fails). */
    FILE *f = host_fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (g_entity_count < MAX_ENTITIES && fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (!line[0]) continue;
        char *save = NULL;
        char *tok = strtok_r(line, "|", &save);
        char *id_tok = tok; (void)id_tok;
        char *px_tok = strtok_r(NULL, "|", &save);
        char *py_tok = strtok_r(NULL, "|", &save);
        char *hex_tok = strtok_r(NULL, "|", &save);
        char *r_tok = strtok_r(NULL, "|", &save);
        char *g_tok = strtok_r(NULL, "|", &save);
        char *b_tok = strtok_r(NULL, "|", &save);
        if (!px_tok || !py_tok || !hex_tok || !r_tok || !g_tok || !b_tok) continue;
        Entity *e = &g_entities[g_entity_count];
        e->pos_x = atoi(px_tok);
        e->pos_y = atoi(py_tok);
        hex_codepoint_to_utf8(hex_tok, e->utf8, sizeof(e->utf8));
        e->r = (unsigned char)atoi(r_tok);
        e->g = (unsigned char)atoi(g_tok);
        e->b = (unsigned char)atoi(b_tok);
        g_entity_count++;
    }
    fclose(f);
}

/* REAL, NEW 2026-08-04, direct instruction ("when u add 2d chicken..."
 * / earlier "no player/tree emoji in 2D" gap): feeds phymoji world
 * entities (trees, chicken, hero) into this SAME real g_entities[]
 * overlay every civ-txt/tactics-txt unit icon already uses - no new
 * 2D rendering path needed, just a second real loader that APPENDS
 * instead of resetting (called AFTER load_entities() below). Each
 * entity's own real display emoji comes from pc_phymoji_gen.c's own
 * NEW emoji.txt sidecar (raw UTF-8, written alongside voxels.csv) -
 * the 3D voxel data itself has no memory of the source emoji, this
 * sidecar is the one real place that's recorded. A host/entity with
 * no sidecar yet (asset predates this fix) is a real, graceful no-op -
 * falls through to terrain-only, same as before this existed. */
static void load_phymoji_entities_as_2d(const char *root, const char *rel_path, int current_z) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/%s", root, rel_path);
    FILE *f = host_fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (g_entity_count < MAX_ENTITIES && fgets(line, sizeof(line), f)) {
        char entity_id[64];
        int x, y, z;
        if (sscanf(line, "%63[^,],%d,%d,%d", entity_id, &x, &y, &z) != 4) continue;
        /* REAL FIX 2026-08-04, direct user report ("why do chicken and
         * tree show up on every z level"): this loader never checked z
         * at all - every entity got placed at its own (x,y) regardless
         * of which Z-SLICE the 2D view currently shows, so it appeared
         * on every layer. Real fix: only show an entity on the exact
         * real Z-layer it actually stands on (matches how the 3D view
         * already only shows it at its own real height). */
        if (z != current_z) continue;
        char emoji_path[PATH_BUF];
        snprintf(emoji_path, sizeof(emoji_path), "%s/pieces/registry/phymoji_assets/%s/emoji.txt", root, entity_id);
        FILE *ef = host_fopen(emoji_path, "r");
        if (!ef) continue;
        Entity *e = &g_entities[g_entity_count];
        e->utf8[0] = '\0';
        if (fgets(e->utf8, sizeof(e->utf8), ef)) {
            e->utf8[strcspn(e->utf8, "\r\n")] = '\0';
        }
        fclose(ef);
        if (!e->utf8[0]) continue;
        e->pos_x = x; e->pos_y = y;
        e->r = e->g = e->b = 90; /* real fallback tint only, matches load_entities()'s own convention */
        g_entity_count++;
    }
    fclose(f);
}

/* Same real sidecar mechanism, for the hero specifically (its own real
 * position lives in pieces/hero_01/state.txt, a different real shape
 * than the comma-list files above). */
static void load_hero_as_2d(const char *root, int current_z) {
    char state_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/pieces/hero_01/state.txt", root);
    int have_x = 0, have_y = 0;
    int x = 0, y = 0, z = 0;
    char buf[64];
    read_kv_str(state_path, "pos_x", buf, sizeof(buf)); if (buf[0]) { x = atoi(buf); have_x = 1; }
    read_kv_str(state_path, "pos_y", buf, sizeof(buf)); if (buf[0]) { y = atoi(buf); have_y = 1; }
    read_kv_str(state_path, "pos_z", buf, sizeof(buf)); if (buf[0]) { z = atoi(buf); }
    if (!have_x || !have_y || g_entity_count >= MAX_ENTITIES) return;
    if (z != current_z) return; /* same real per-Z-layer filter as load_phymoji_entities_as_2d() */
    char emoji_path[PATH_BUF];
    snprintf(emoji_path, sizeof(emoji_path), "%s/pieces/registry/phymoji_assets/hero_humanoid/emoji.txt", root);
    FILE *ef = host_fopen(emoji_path, "r");
    if (!ef) return;
    Entity *e = &g_entities[g_entity_count];
    e->utf8[0] = '\0';
    if (fgets(e->utf8, sizeof(e->utf8), ef)) e->utf8[strcspn(e->utf8, "\r\n")] = '\0';
    fclose(ef);
    if (!e->utf8[0]) return;
    e->pos_x = x; e->pos_y = y;
    e->r = e->g = e->b = 90;
    g_entity_count++;
}

static Entity *entity_at(int x, int y) {
    for (int i = 0; i < g_entity_count; i++) {
        if (g_entities[i].pos_x == x && g_entities[i].pos_y == y) return &g_entities[i];
    }
    return NULL;
}

static int clamp_int(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* Optional multi-Z chunk support, ADDED 2026-08-03 for piececraft-xyz's
 * real Phase 1 (world/chunk storage per PIECECRAFT_XYZ_DESIGN.md §1/§1a,
 * see civ-vs-piece.md §3c). A host that wants multiple Z-levels
 * publishes pieces/system/board_manifest.txt:
 *   z_base=pieces/system/chunks/chunk_0_0/chunk_0_0_z
 *   z_count=32
 * board-viewer then reads "{z_base}{current_z}.txt" for whichever
 * Z-level is currently selected (bv_state.txt's own "current_z" key -
 * matches the real field name fuzz-op's own project.pdl/xlector state.txt
 * use, and mutaclysm's own dox/ctrl-legend.md z/x key convention -
 * see bv_menu_input.c's own header comment on this same change for the
 * full precedent citation and its one flagged divergence). A host that
 * does NOT publish this manifest (civ-txt, tactics-txt) is completely
 * unaffected - resolve_board_path() falls back to the exact same flat
 * pieces/system/board.txt read that already existed, zero behavior
 * change for either project. */
static int has_z_manifest(const char *root, char *z_base_out, size_t z_base_sz, int *z_count_out) {
    char manifest_path[PATH_BUF];
    snprintf(manifest_path, sizeof(manifest_path), "%s/pieces/system/board_manifest.txt", root);
    z_base_out[0] = '\0';
    *z_count_out = 0;
    read_kv_str(manifest_path, "z_base", z_base_out, z_base_sz);
    *z_count_out = read_kv_int(manifest_path, "z_count", 0);
    return (z_base_out[0] && *z_count_out > 0);
}

static void resolve_board_path(const char *root, int current_z, char *out, size_t out_sz) {
    char z_base[PATH_BUF];
    int z_count = 0;
    if (has_z_manifest(root, z_base, sizeof(z_base), &z_count)) {
        int clamped_z = clamp_int(current_z, 0, z_count - 1);
        snprintf(out, out_sz, "%s/%s%d.txt", root, z_base, clamped_z);
    } else {
        snprintf(out, out_sz, "%s/pieces/system/board.txt", root);
    }
}

static FILE *g_view_out = NULL;
static void border(void) {
    if (g_view_out) { fputc('+', g_view_out); for (int i = 0; i < BOX_W; i++) fputc('=', g_view_out); fputc('+', g_view_out); fputc('\n', g_view_out); }
}
static void line(const char *content) {
    int len = (int)strlen(content);
    if (len > BOX_W) len = BOX_W;
    if (g_view_out) {
        fprintf(g_view_out, "|%.*s", len, content);
        for (int i = len; i < BOX_W; i++) fputc(' ', g_view_out);
        fputc('|', g_view_out);
        fputc('\n', g_view_out);
    }
}
static void blank(void) { line(""); }

static void ping_chtpm_render_marker(const char *root) {
    char marker_path[PATH_BUF];
    snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/frame_changed.txt", root);
    FILE *mf = host_fopen(marker_path, "a");
    if (mf) { fputc('.', mf); fclose(mf); }
}

/* Terrain glyph legend, now DATA-DRIVEN per host project (real fix
 * 2026-08-03 - see bv_render_3d.c's own load_terrain_legend() for the
 * full root-cause writeup: these were hardcoded C switch statements
 * duplicating, and in '#''s case diverging from, the FOCUSED host's
 * own real terrain - a new host project with a new glyph vocabulary
 * (piececraft-xyz's real voxel game needs dirt/stone/sand/wood/...)
 * would otherwise require editing board-viewer's own C source again.
 * Reads the SAME pieces/system/terrain_legend.txt file bv_render_3d.c
 * reads (glyph|height|r|g|b|asset_hex|name), duplicated not shared per
 * house no-shared-headers convention. Height/r/g/b columns are unused
 * here (2D has no extrusion) but the file format stays identical so
 * one legend file serves both the 2D and 3D renderer. Any glyph not
 * found falls back to the exact same defaults the old switches used
 * ("?" name, NULL emoji -> caller's own ASCII-glyph fallback). */
#define MAX_TERRAIN_LEGEND 32
typedef struct {
    char glyph;
    char name[32];
    char emoji_utf8[8]; /* empty = no emoji (ASCII glyph fallback) */
} TerrainLegendEntry;
static TerrainLegendEntry g_terrain_legend[MAX_TERRAIN_LEGEND];
static int g_terrain_legend_count = 0;

static void load_terrain_legend(const char *root) {
    g_terrain_legend_count = 0;
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/terrain_legend.txt", root);
    /* Must use host_fopen — host is often under emoji house path on Win. */
    FILE *f = host_fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (g_terrain_legend_count < MAX_TERRAIN_LEGEND && fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        /* Comment lines only ("# text..."), NOT a real "#|1.8|..." data
         * row for tactics-txt's own wall glyph, which is itself the
         * literal character '#' - real bug, caught live in this same
         * pass (see bv_render_3d.c's own identical fix/comment). */
        if (!line[0] || (line[0] == '#' && line[1] != '|')) continue;
        char *save = NULL;
        char *glyph_tok = strtok_r(line, "|", &save);
        strtok_r(NULL, "|", &save); /* height, unused in 2D */
        strtok_r(NULL, "|", &save); /* r, unused in 2D */
        strtok_r(NULL, "|", &save); /* g, unused in 2D */
        strtok_r(NULL, "|", &save); /* b, unused in 2D */
        char *asset_tok = strtok_r(NULL, "|", &save);
        char *name_tok = strtok_r(NULL, "|", &save);
        if (!glyph_tok || !glyph_tok[0]) continue;
        TerrainLegendEntry *e = &g_terrain_legend[g_terrain_legend_count];
        e->glyph = glyph_tok[0];
        snprintf(e->name, sizeof(e->name), "%s", name_tok ? name_tok : "?");
        e->emoji_utf8[0] = '\0';
        if (asset_tok && asset_tok[0] && strcmp(asset_tok, "-") != 0) {
            hex_codepoint_to_utf8(asset_tok, e->emoji_utf8, sizeof(e->emoji_utf8));
        }
        g_terrain_legend_count++;
    }
    fclose(f);
}

static const TerrainLegendEntry *terrain_legend_lookup(char g) {
    for (int i = 0; i < g_terrain_legend_count; i++) {
        if (g_terrain_legend[i].glyph == g) return &g_terrain_legend[i];
    }
    return NULL;
}

static const char *glyph_name(char g) {
    const TerrainLegendEntry *e = terrain_legend_lookup(g);
    return (e && e->name[0]) ? e->name : "?";
}

/* emoji_mode default-on (per direct instruction: "we dont show ascii
 * we go immediately to emoji mode for these game widgets... using a
 * config file flag" - see &.widgits/5-pov-widgit.md and
 * bv_state.txt's own emoji_mode key, pre-seeded =1 by button.sh at
 * launch, matching mutaclysm's own read_kv_int(...,"emoji_mode",1)
 * default). Real UTF-8 emoji, not a hand-picked palette - relies on
 * chtpm_rgb_render.c's own GENERIC ON-DEMAND EMOJI GENERATION path
 * (real FreeType + NotoColorEmoji.ttf, voxel-asset cache under
 * pieces/registry/emoji_assets/<hex>/) to actually rasterize whatever
 * codepoint is emitted here - no emoji_assets curation needed in this
 * project. */
static const char *glyph_emoji(char g) {
    const TerrainLegendEntry *e = terrain_legend_lookup(g);
    if (e && e->emoji_utf8[0]) return e->emoji_utf8;
    return NULL;
}

int main(void) {
    resolve_root();
    load_house_root();

    char view_path[PATH_BUF], state_path[PATH_BUF];
    snprintf(view_path, sizeof(view_path), "%s/pieces/apps/player_app/view.txt", project_root);
    snprintf(state_path, sizeof(state_path), "%s/pieces/system/bv_state.txt", project_root);

    char focused_project_id[128] = "", focused_raw[PATH_BUF] = "", focused_project_root[PATH_BUF] = "";
    read_kv_str(state_path, "focused_project_id", focused_project_id, sizeof(focused_project_id));
    read_kv_str(state_path, "focused_project_root", focused_raw, sizeof(focused_raw));
    resolve_host_root(focused_raw, focused_project_root, sizeof(focused_project_root));
    int selector_x = read_kv_int(state_path, "selector_x", -1);
    int selector_y = read_kv_int(state_path, "selector_y", -1);
    int emoji_mode = read_kv_int(state_path, "emoji_mode", 1);

    /* Overwrite piece.pdl with META only, no METHOD rows, every render
     * - real, live-caught bug: an OLDER version of this op used to
     * write a real "ENTER_NAV_MODE" METHOD row here every frame; that
     * generation code was removed (see interact-fix-widget.txt - nav
     * entry is a real static <button onClick="INTERACT"> in the
     * .chtpm now, never a METHOD row), but the FILE it had already
     * written was never cleaned up, so ${piece_methods} kept picking
     * up the stale leftover row forever, rendering as a second, dead
     * "2." item alongside the real INTERACT button. Rewriting the file
     * unconditionally every frame (same "regenerated every render"
     * convention already used everywhere else in this house) makes
     * this self-healing against any future stale content too. */
    char pdl_path[PATH_BUF];
    snprintf(pdl_path, sizeof(pdl_path), "%s/projects/board-viewer/pieces/board_viewer/piece.pdl", project_root);
    FILE *pdl_out = host_fopen(pdl_path, "w");
    if (pdl_out) {
        fprintf(pdl_out, "SECTION      | KEY                | VALUE\n");
        fprintf(pdl_out, "----------------------------------------\n");
        fprintf(pdl_out, "META         | piece_id           | board_viewer\n");
        fclose(pdl_out);
    }

    /* Real interact-mode signal (2026-08-02 fix, see &.widgits/
     * interact-fix-widget.txt for the full investigation): whether nav
     * mode is genuinely active is now owned ENTIRELY by chtpm_parser_
     * pal.c's own INTERACT machinery, driven by the real, hand-written
     * <button onClick="INTERACT"> in board_viewer.chtpm (mutaclysm's
     * own exact convention, game.chtpm:7) - not by any state this
     * project's own ops track. The engine exports its own active/
     * typing flag to pieces/display/active_gui_is_typing.txt on every
     * mode transition (export_active_index(), chtpm_parser_pal.c:2719
     * - "1" means active_index != -1, a real INTERACT element is
     * currently engaged) - read that back here purely for the status
     * line/instructions text, nothing gates on it. No piece.pdl
     * METHOD row is generated for nav entry anymore - INTERACT is
     * reachable only via the static chtpm button, never through
     * ${piece_methods} (which always becomes onClick="KEY:N", not the
     * reserved INTERACT string - confirmed via direct parser code
     * read). */
    char typing_path[PATH_BUF];
    snprintf(typing_path, sizeof(typing_path), "%s/pieces/display/active_gui_is_typing.txt", project_root);
    int nav_mode = 0;
    {
        FILE *tf = host_fopen(typing_path, "r");
        if (tf) {
            char buf[16] = "";
            if (fgets(buf, sizeof(buf), tf)) nav_mode = atoi(buf);
            fclose(tf);
        }
    }

    g_view_out = host_fopen(view_path, "w");
    if (!g_view_out) return 1;

    border();
    line("  B O A R D - V I E W E R   ( W I D G E T )");
    border();
    blank();

    if (!focused_project_id[0]) {
        line("  No project focused yet.");
        line("  (Launched standalone, or focus wiring is P3, not yet built.)");
        blank();
        border();
        fclose(g_view_out);
        ping_chtpm_render_marker(project_root);
        return 0;
    }

    int current_z = read_kv_int(state_path, "current_z", default_current_z(focused_project_root));
    char board_path[PATH_BUF];
    resolve_board_path(focused_project_root, current_z, board_path, sizeof(board_path));
    FILE *bf = host_fopen(board_path, "r");
    if (!bf) bf = fopen(board_path, "r");
    if (!bf) {
        char rowbuf[MAX_LINE];
        snprintf(rowbuf, sizeof(rowbuf), "  Focused on: %s", focused_project_id);
        line(rowbuf);
        snprintf(rowbuf, sizeof(rowbuf), "  root: %s", focused_project_root);
        line(rowbuf);
        snprintf(rowbuf, sizeof(rowbuf), "  board: %s", board_path);
        line(rowbuf);
        line("  No board/chunk file (Confirm & Start, or path unreadable).");
        blank();
        border();
        fclose(g_view_out);
        ping_chtpm_render_marker(project_root);
        return 0;
    }

    char board[MAX_BOARD_DIM][MAX_BOARD_DIM];
    int board_h = 0, board_w = 0;
    char boardline[MAX_LINE];
    while (board_h < MAX_BOARD_DIM && fgets(boardline, sizeof(boardline), bf)) {
        boardline[strcspn(boardline, "\r\n")] = '\0';
        int len = (int)strlen(boardline);
        if (len == 0) continue;
        if (len > MAX_BOARD_DIM) len = MAX_BOARD_DIM;
        memcpy(board[board_h], boardline, len);
        if (len > board_w) board_w = len;
        board_h++;
    }
    fclose(bf);

    /* Real entity manifest - see this file's own load_entities() header
     * comment. Missing/empty entities.txt is NOT an error (unlike
     * board.txt) - a host with no real entities yet just shows terrain
     * only, same as before this feature existed. */
    load_entities(focused_project_root);
    load_phymoji_entities_as_2d(focused_project_root, "pieces/world_01/phymoji_entities.txt", current_z);
    load_phymoji_entities_as_2d(focused_project_root, "pieces/world_01/animals.txt", current_z);
    load_hero_as_2d(focused_project_root, current_z);
    load_terrain_legend(focused_project_root);

    /* Real, live-caught gap (2026-08-02): board.txt EXISTING but being
     * EMPTY (fopen succeeds, but the host hasn't actually run
     * CONFIRM_START in the CURRENT session yet - board_h/board_w stay
     * 0) fell through silently below - no error message, no early
     * return, just a blank grid area over a black GL background,
     * easily mistaken for a real rendering bug ("text is there but no
     * 2d/3d"). The MISSING-file case above already handles this
     * correctly; empty-file needs the exact same treatment. */
    if (board_w == 0 || board_h == 0) {
        char rowbuf[MAX_LINE];
        snprintf(rowbuf, sizeof(rowbuf), "  Focused on: %s", focused_project_id);
        line(rowbuf);
        line("  board.txt is empty - host hasn't run Confirm & Start in");
        line("  THIS session yet (game_state is probably still 'setup').");
        blank();
        border();
        fclose(g_view_out);
        ping_chtpm_render_marker(project_root);
        return 0;
    }

    /* Selector defaults to board center on first-ever render (no prior
     * selector_x/y recorded yet). */
    if (selector_x < 0 || selector_y < 0) {
        selector_x = board_w / 2;
        selector_y = board_h / 2;
    }
    selector_x = clamp_int(selector_x, 0, board_w > 0 ? board_w - 1 : 0);
    selector_y = clamp_int(selector_y, 0, board_h > 0 ? board_h - 1 : 0);

    /* Camera clamp, exact formula from 5-pov-widgit.md §7 / mutaclysm's
     * own ops/compose_frame.c:829-838 - anchor is always the selector
     * in this widget (no hero to branch on). */
    int cam_x = selector_x - VIEWPORT_W / 2;
    int cam_x_max = board_w - VIEWPORT_W;
    if (cam_x_max < 0) cam_x_max = 0;
    cam_x = clamp_int(cam_x, 0, cam_x_max);

    int cam_y = selector_y - VIEWPORT_H / 2;
    int cam_y_max = board_h - VIEWPORT_H;
    if (cam_y_max < 0) cam_y_max = 0;
    cam_y = clamp_int(cam_y, 0, cam_y_max);

    char rowbuf[MAX_LINE];
    snprintf(rowbuf, sizeof(rowbuf), "  Focused on: %s  (board %dx%d, cam %d,%d)",
             focused_project_id, board_w, board_h, cam_x, cam_y);
    line(rowbuf);
    snprintf(rowbuf, sizeof(rowbuf), "  Interact mode: %s", nav_mode ? "ON (arrows move selector, ESC exits)" : "OFF (select Enter Interact Mode below)");
    line(rowbuf);
    {
        /* Real possession status, added 2026-08-03 (civ-vs-piece.md
         * §6a/§6b, this session's own '9' toggle in bv_menu_input.c) -
         * important UX feedback: whether xelector movement is a free
         * camera pan or actually driving the hero (and costing a real
         * tick) isn't otherwise visible anywhere. */
        char xelector_state_path[PATH_BUF];
        snprintf(xelector_state_path, sizeof(xelector_state_path), "%s/pieces/xelector_01/state.txt", focused_project_root);
        char possessed_id[64] = "";
        read_kv_str(xelector_state_path, "possessed_id", possessed_id, sizeof(possessed_id));
        if (possessed_id[0]) {
            snprintf(rowbuf, sizeof(rowbuf), "  Possessing: %s  (press 9 to %s)",
                     strcmp(possessed_id, "hero_01") == 0 ? "hero_01 (jump/mine/build active)" : "none (free camera)",
                     strcmp(possessed_id, "hero_01") == 0 ? "release" : "possess hero");
            line(rowbuf);
        }
    }
    snprintf(rowbuf, sizeof(rowbuf), "  Display: %s", emoji_mode ? "emoji (default)" : "ascii");
    line(rowbuf);
    {
        char z_base_check[PATH_BUF];
        int z_count_check = 0;
        if (has_z_manifest(focused_project_root, z_base_check, sizeof(z_base_check), &z_count_check)) {
            /* current_z itself is written unclamped by bv_menu_input.c
             * (repeated 'x' past the top layer just keeps counting up) -
             * resolve_board_path() already clamps for the actual file
             * read, so the rendered terrain is always correct, but the
             * STATUS NUMBER shown here needs the same clamp or it can
             * display a value like "3 / 2" that looks like a real bug
             * even though nothing is actually broken - caught live
             * 2026-08-03 testing this same change. */
            int shown_z = clamp_int(current_z, 0, z_count_check - 1);
            snprintf(rowbuf, sizeof(rowbuf), "  Z-level: %d / %d  (z / x to switch layers)", shown_z, z_count_check - 1);
            line(rowbuf);
        }
    }

    /* Camera status (5-pov-widgit.md §2e) - '0' toggles render_mode,
     * '5'-'8' switch camera_mode while render_mode==1 (moved from
     * '1'-'4' 2026-08-31, see bv_menu_input.c's own header comment on
     * that change - '1'-'4' now reserved for a future "one map"
     * perspective mode). 3D pixels
     * themselves are separate, later work (§2f) - this line makes the
     * real, already-working state dispatch visible/testable now. */
    /* REAL FIX 2026-08-04, direct instruction ("always start in 3d
     * mode"): default render_mode is now project-conditional, not a
     * bare 0 for every host - a project that publishes real Z-layer
     * data (has_z_manifest(), piececraft-xyz's own board_manifest.txt)
     * genuinely HAS a 3D world to show, so a fresh session defaults
     * straight into it. A host with no real Z-manifest (civ-txt,
     * tactics-txt - real 2D-only board games) is completely unaffected
     * - has_z_manifest() returns false for them, same real 0 default as
     * before, not a regression risk.
     * 2026-08-07, direct instruction ("start in 3d 3rd person as a
     * default, read from a config file so it's flexible"): the host's
     * own pieces/system/arrow_config.txt can now override BOTH fresh
     * defaults - default_render_mode=<0|1> (missing/invalid falls back
     * to the has_z_manifest() conditional below) and default_camera_mode
     * =<1-4> (missing/invalid = 2, third-person - the real "start in 3d
     * 3rd person" directive default, NOT the old bird's-eye 4). This file
     * must agree with bv_menu_input.c/bv_render_3d.c's own same
     * helpers. */
    char cfg_path[PATH_BUF];
    snprintf(cfg_path, sizeof(cfg_path), "%s/pieces/system/arrow_config.txt", focused_project_root);
    char z_base_dummy[PATH_BUF]; int z_count_dummy = 0;
    int cfg_default_render = read_kv_int(cfg_path, "default_render_mode", -1);
    int default_render_mode = (cfg_default_render == 0 || cfg_default_render == 1)
        ? cfg_default_render
        : (has_z_manifest(focused_project_root, z_base_dummy, sizeof(z_base_dummy), &z_count_dummy) ? 1 : 0);
    int cfg_default_camera = read_kv_int(cfg_path, "default_camera_mode", 2);
    int default_camera_mode = (cfg_default_camera >= 1 && cfg_default_camera <= 4) ? cfg_default_camera : 2;
    int render_mode = read_kv_int(state_path, "render_mode", default_render_mode);
    int camera_mode = read_kv_int(state_path, "camera_mode", default_camera_mode);
    if (!render_mode) {
        line("  Camera: 2D flat (press 0 for 3D raymarch - not yet rendered)");
    } else {
        /* Mode-consistent fresh pitch (same parity fix as
         * bv_render_3d.c: 6 for modes 1/2, -90 only for 3/4). */
        int default_cam_pitch = (camera_mode == 1 || camera_mode == 2) ? 6 : -90;
        int cam_yaw = read_kv_int(state_path, "cam_yaw", 180);
        int cam_pitch = read_kv_int(state_path, "cam_pitch", default_cam_pitch);
        int cam_pan_x = read_kv_int(state_path, "cam_pan_x", 0);
        int cam_pan_y = read_kv_int(state_path, "cam_pan_y", 0);
        int cam_pan_z = read_kv_int(state_path, "cam_pan_z", 0);
        int cam_z_level = read_kv_int(state_path, "cam_z_level", 0);
        const char *mode_name = camera_mode == 1 ? "first-person" :
                                 camera_mode == 2 ? "third-person" :
                                 camera_mode == 3 ? "free-roam" : "bird's-eye";
        snprintf(rowbuf, sizeof(rowbuf), "  Camera: 3D mode %d (%s) yaw=%d pitch=%d",
                 camera_mode, mode_name, cam_yaw, cam_pitch);
        line(rowbuf);
        snprintf(rowbuf, sizeof(rowbuf), "    pan=(%d,%d,%d) z_level=%d [q/e yaw][r/t pitch][wasd pan][c/v z][f reset]",
                 cam_pan_x, cam_pan_y, cam_pan_z, cam_z_level);
        line(rowbuf);
    }
    blank();

    /* If 3D overlay is missing, force 2D emoji grid so GL is not blank
     * (Win used to leave 30 empty lines when bv_render_3d could not open
     * the host root). Overlay appears after a successful bv_render_3d. */
    if (render_mode) {
        char ov_path[PATH_BUF];
        snprintf(ov_path, sizeof(ov_path), "%s/pieces/display/rgb_frame_3d_overlay.raw", project_root);
        FILE *ov = host_fopen(ov_path, "rb");
        if (!ov) ov = fopen(ov_path, "rb");
        if (!ov || fseek(ov, 0, SEEK_END) != 0 || ftell(ov) < 1000) {
            if (ov) fclose(ov);
            render_mode = 0; /* fallback 2D this frame */
            line("  (3D overlay not ready - showing 2D emoji map; press 0 when 3D loads)");
        } else {
            fclose(ov);
        }
    }
    if (render_mode) {
        /* Real MAP3D_MARKER sentinel, exact port of mutaclysm's own
         * ops/compose_frame.c:1088 (see &.widgits/view-vs-muta.md §2-3
         * for the full investigation): a single 0x01 (SOH) byte as the
         * FIRST byte of its own line, immediately before the would-be
         * viewport rows. system/chtpm_rgb_render (mutaclysm's own
         * fork, copied in build.sh) watches for this exact byte as it
         * walks current_frame.txt, and on finding it, composites
         * pieces/display/rgb_frame_3d_overlay.raw (bv_render_3d.c's
         * own output) at that screen row, then skips ahead
         * ceil(overlay_h/GLYPH_H) further source lines - kept in sync
         * with bv_render_3d.c's own FRAME_H(480)/GLYPH_H(16)=30 lines
         * here. Written directly to g_view_out, NOT through line()'s
         * own box-border wrapping (which would put a '|' before the
         * marker byte, breaking the required "first byte of the line"
         * check) - a real, deliberate, minor cosmetic tradeoff: the
         * bordered-box look breaks across these 30 lines in ASCII
         * terminal view. Acceptable since this widget is GL-primary,
         * not ASCII-primary, and the ASCII view never showed real 3D
         * content anyway (render_mode is GL-window-only everywhere in
         * this house's real convention - 5-pov-widgit.md §2a). */
        fputc(0x01, g_view_out);
        fputc('\n', g_view_out);
        for (int i = 0; i < 30; i++) fputc('\n', g_view_out);
    } else {
    for (int vr = 0; vr < VIEWPORT_H; vr++) {
        int src_row = cam_y + vr;
        char rendered[MAX_LINE];
        int p = 0;
        rendered[p++] = ' ';
        rendered[p++] = ' ';
        for (int vc = 0; vc < VIEWPORT_W; vc++) {
            int src_col = cam_x + vc;
            char g = ' ';
            if (src_row >= 0 && src_row < board_h && src_col >= 0 && src_col < board_w) {
                g = board[src_row][src_col];
            }
            int is_sel = (src_row == selector_y && src_col == selector_x);
            /* Cells are packed edge-to-edge with NO separator between
             * columns - matches mutaclysm's own real convention
             * exactly (ops/compose_frame.c:1091, `fputs(viewport_
             * emoji[r][col], out)` in a tight loop, no space char
             * emitted between cells). A real, user-reported bug: an
             * earlier version here wrapped every cell in a leading/
             * trailing ' ' unconditionally, which - combined with
             * chtpm_rgb_render.c's own per-glyph pixel-width advance
             * (VOXEL_PX=16 for a real emoji tile, GLYPH_W=8 for a
             * plain-ASCII space) - rendered as a visibly wide blank
             * gap after every single tile. Brackets are now the ONLY
             * extra characters ever emitted, and only for the
             * selected cell (still plain ASCII, so bracket-adjacent
             * spacing is a real column, not padding - matches the
             * numbered-row [>]/[ ] convention used elsewhere in this
             * house's CHTPM UI, just without the always-on padding). */
            /* Entity occupying this cell wins over terrain, if any -
             * real entities are more important to see than the ground
             * under them, matches every real precedent in this house
             * (mutaclysm's own entity-over-floor draw order). */
            Entity *occ = entity_at(src_col, src_row);
            const char *emoji = occ ? occ->utf8 : (emoji_mode ? glyph_emoji(g) : NULL);
            if (is_sel) rendered[p++] = '[';
            if (emoji) {
                size_t elen = strlen(emoji);
                if (p + (int)elen < MAX_LINE - 4) {
                    memcpy(rendered + p, emoji, elen);
                    p += (int)elen;
                }
            } else {
                rendered[p++] = g;
            }
            if (is_sel) rendered[p++] = ']';
        }
        rendered[p] = '\0';
        line(rendered);
    }
    }

    blank();
    if (selector_y >= 0 && selector_y < board_h && selector_x >= 0 && selector_x < board_w) {
        char sel_g = board[selector_y][selector_x];
        snprintf(rowbuf, sizeof(rowbuf), "  Selected (%d,%d): %s", selector_x, selector_y, glyph_name(sel_g));
        line(rowbuf);
    }
    line("  Arrow keys pan the selector. Camera follows, clamped to board edges.");

    blank();
    border();

    fclose(g_view_out);
    /* REVERTED 2026-08-02 (see &.widgits/view-vs-muta.md): the earlier
     * render_mode-conditional suppression here was solving the wrong
     * problem - it tried to stop chtpm_rgb_render's own re-render, but
     * that daemon's two watched triggers are grown UNCONDITIONALLY by
     * chtpm_parser_pal.c itself on every key, regardless of anything
     * this project's own code does or doesn't call - there was never
     * any way to suppress it from this side. The real fix (system/
     * chtpm_rgb_render now being mutaclysm's own fork, with real
     * MAP3D_MARKER/overlay-compositing support - see bv_render_3d.c's
     * own header comment) means it no longer NEEDS to be suppressed:
     * bv_render_3d.c no longer writes rgb_frame.raw at all, so there is
     * nothing left to race against. Unconditional bump, matching every
     * other screen in this house. */
    ping_chtpm_render_marker(project_root);
    return 0;
}
