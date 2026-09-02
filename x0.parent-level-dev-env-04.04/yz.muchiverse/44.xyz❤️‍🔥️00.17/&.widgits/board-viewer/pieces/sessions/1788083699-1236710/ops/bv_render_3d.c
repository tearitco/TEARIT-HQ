/* bv_render_3d - board-viewer's own 3D renderer.
 *
 * REWRITE 2026-08-02 pt.3, direct user instruction ("use the
 * raymarching, thats what we wanted" - after being asked directly
 * "did we do the real raymarching or cheat?" and answering honestly:
 * no, pt.1/pt.2 of this file were a RASTERIZER - project flat/boxed
 * geometry through a camera, sort back-to-front, fill polygons with
 * solid colors. That is NOT what mutaclysm does and is a fundamentally
 * different, lower-fidelity technique - no textures, no per-pixel
 * ray/geometry intersection, just solid-color polygons.
 *
 * THIS version is a real per-pixel DDA raymarcher (Amanatides & Woo
 * grid traversal) + real voxel-texture sampling, ported from
 * mutaclysm's own ops/compose_rgb_frame.c: ray_aabb_hit_3d() (slab
 * method box intersection, ported near-verbatim), box_face_uv() (hit-
 * point -> texture UV per face, ported verbatim), get_voxel8_cached()/
 * sample_voxel8_pixel() (real voxel PNG->CSV texture data, loaded from
 * pieces/registry/emoji_assets/<hex>/voxels_16.csv - the SAME real
 * assets this project's own 2D emoji-mode rendering already uses,
 * auto-generated earlier via chtpm_rgb_render's own generic on-demand
 * emoji-to-voxel pipeline, so 2D and 3D now show visually consistent
 * textures for the same terrain).
 *
 * Unlike mutaclysm's own split (floor as flat painter's-algorithm
 * quads, walls via a separate raymarch pass), this file tests EVERY
 * board cell as a thin-or-tall AABB in ONE unified per-pixel DDA walk -
 * simpler given board-viewer's own single-scalar-height terrain model
 * (no separate walkable/wall glyph registry the way mutaclysm has).
 *
 * Overlay-compositing architecture (writes pieces/display/
 * rgb_frame_3d_overlay.raw, never rgb_frame.raw directly - see
 * &.widgits/view-vs-muta.md) is UNCHANGED from the previous rewrite;
 * only the actual pixel-generation algorithm changed in this pass.
 *
 * Self-contained, no shared headers.
 * Usage: bv_render_3d.+x (no args) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <omp.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_BOARD_DIM 64

/* Overlay dimensions - see bv_compose_frame.c's own matching marker-
 * skip line count (OVERLAY_H/GLYPH_H must divide evenly, kept in sync
 * manually between the two files, no shared header convention). */
#define GLYPH_H 16
#define FRAME_W 640
#define FRAME_H 480

#define M_PI_LOCAL 3.14159265358979323846

typedef struct { double x, y, z; } Vec3;

static char project_root[MAX_PATH] = ".";
static char house_root[MAX_PATH] = "";

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#ifndef F_OK
#define F_OK 0
#endif
#define access _access
#endif

/* UTF-8 path open — MinGW ANSI fopen fails on emoji house paths. */
static FILE *host_fopen(const char *path, const char *mode) {
#ifdef _WIN32
    wchar_t wpath[PATH_BUF], wmode[16];
    if (!path || !mode) return NULL;
    if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, PATH_BUF) <= 0 &&
        MultiByteToWideChar(CP_ACP, 0, path, -1, wpath, PATH_BUF) <= 0)
        return fopen(path, mode);
    if (MultiByteToWideChar(CP_UTF8, 0, mode, -1, wmode, 16) <= 0)
        MultiByteToWideChar(CP_ACP, 0, mode, -1, wmode, 16);
    FILE *f = _wfopen(wpath, wmode);
    return f ? f : fopen(path, mode);
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

static void load_house_root(void) {
    house_root[0] = '\0';
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/house_root.txt", project_root);
    FILE *f = host_fopen(path, "r");
    if (!f) return;
    if (fgets(house_root, sizeof(house_root), f)) {
        if ((unsigned char)house_root[0] == 0xEF &&
            (unsigned char)house_root[1] == 0xBB &&
            (unsigned char)house_root[2] == 0xBF)
            memmove(house_root, house_root + 3, strlen(house_root + 3) + 1);
        house_root[strcspn(house_root, "\r\n")] = '\0';
    }
    fclose(f);
}

/* Relative host paths (e.g. @.apps/aomorai-editor) resolve against house_root. */
static void resolve_host_root(const char *raw, char *out, size_t out_sz) {
    out[0] = '\0';
    if (!raw || !raw[0]) return;
    if ((raw[0] && raw[1] == ':') || raw[0] == '/' || raw[0] == '\\') {
        snprintf(out, out_sz, "%s", raw);
        return;
    }
    if (house_root[0]) {
        snprintf(out, out_sz, "%s/%s", house_root, raw);
        return;
    }
    snprintf(out, out_sz, "%s", raw);
}

static void read_kv_str(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = host_fopen(path, "r");
    if (!f) return;
    char l[MAX_LINE];
    size_t key_len = strlen(key);
    int first = 1;
    while (fgets(l, sizeof(l), f)) {
        char *line = l;
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

static double read_kv_double(const char *path, const char *key, double def) {
    char buf[64];
    read_kv_str(path, key, buf, sizeof(buf));
    return buf[0] ? atof(buf) : def;
}

static long long read_kv_ll(const char *path, const char *key, long long def) {
    char buf[64];
    read_kv_str(path, key, buf, sizeof(buf));
    return buf[0] ? atoll(buf) : def;
}

/* REAL FIX 2026-08-04, direct user report ("cam always reset showing
 * underground instead of above ground where the player/xelector is"):
 * current_z (which Z-layer/height a fresh session views) used to
 * default to a bare 0 everywhere - for any real multi-layer world
 * whose ground surface sits well above Z=0 (piececraft-xyz's own
 * debug map: ~16-18), that default IS underground. Real, generic fix:
 * if the focused host has a real hero_01 piece, its own real pos_z
 * (already "where the player actually is", pc_generate_chunk.c's own
 * real spawn write) is a genuinely better default than a magic
 * constant - a host with no hero (civ-txt/tactics-txt) falls back to
 * the old 0 unchanged, so this is a pure improvement, not a
 * regression risk for other real projects sharing this widget. */
/* Forward declaration - bv3d_has_z_manifest() is defined further down
 * this same file, needed here by default_render_mode() below. */
static int bv3d_has_z_manifest(const char *root, char *z_base_out, size_t z_base_sz, int *z_count_out);

/* Same real project-conditional default as bv_compose_frame.c/
 * bv_menu_input.c's own default_render_mode() (2026-08-04).
 * 2026-08-07, direct instruction ("start in 3d 3rd person as a
 * default, read from a config file so it's flexible"): the host's own
 * pieces/system/arrow_config.txt can now override both defaults,
 * mirroring the same two helpers in bv_menu_input.c/bv_compose_frame.c
 * so all three files still agree on a fresh session's default. */
static int default_render_mode(const char *root) {
    if (!root || !root[0]) return 0;
    char cfg[PATH_BUF];
    snprintf(cfg, sizeof(cfg), "%s/pieces/system/arrow_config.txt", root);
    int cfg_default = read_kv_int(cfg, "default_render_mode", -1);
    if (cfg_default == 0 || cfg_default == 1) return cfg_default;
    char z_base[PATH_BUF]; int z_count = 0;
    return bv3d_has_z_manifest(root, z_base, sizeof(z_base), &z_count) ? 1 : 0;
}

static int default_camera_mode(const char *root) {
    if (!root || !root[0]) return 2;
    char cfg[PATH_BUF];
    snprintf(cfg, sizeof(cfg), "%s/pieces/system/arrow_config.txt", root);
    int cfg_default = read_kv_int(cfg, "default_camera_mode", 2);
    if (cfg_default >= 1 && cfg_default <= 4) return cfg_default;
    return 2;
}

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

/* Optional multi-Z chunk support, ADDED 2026-08-03 - real port of the
 * exact same convention bv_compose_frame.c's own resolve_board_path()
 * already uses for the 2D view (see that file's own header comment for
 * the full writeup - civ-vs-piece.md §3c, PIECECRAFT_XYZ_DESIGN.md
 * §1/§1a). This file's own board-reading code used to always read the
 * flat pieces/system/board.txt directly, which a host publishing a
 * board_manifest.txt (piececraft-xyz) no longer has at all - switching
 * to 3D mode against such a host correctly found no file and rendered
 * nothing (real, if silent, early-return - `if (!bf) return 0;`), which
 * is why 3D showed solid black while 2D correctly showed real terrain.
 * A host with no manifest (civ-txt, tactics-txt) is unaffected -
 * resolve_board_path_3d() falls back to the exact same flat read. */
static int bv3d_has_z_manifest(const char *root, char *z_base_out, size_t z_base_sz, int *z_count_out) {
    char manifest_path[PATH_BUF];
    snprintf(manifest_path, sizeof(manifest_path), "%s/pieces/system/board_manifest.txt", root);
    z_base_out[0] = '\0';
    *z_count_out = 0;
    read_kv_str(manifest_path, "z_base", z_base_out, z_base_sz);
    *z_count_out = read_kv_int(manifest_path, "z_count", 0);
    return (z_base_out[0] && *z_count_out > 0);
}

/* --- vector helpers --- */
static Vec3 v3_add(Vec3 a, Vec3 b) { Vec3 r = {a.x+b.x, a.y+b.y, a.z+b.z}; return r; }
static double v3_dot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static Vec3 v3_cross(Vec3 a, Vec3 b) {
    Vec3 r = { a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x };
    return r;
}
static Vec3 v3_scale(Vec3 a, double s) { Vec3 r = {a.x*s, a.y*s, a.z*s}; return r; }
static Vec3 v3_norm(Vec3 a) {
    double len = sqrt(v3_dot(a, a));
    if (len < 1e-9) return a;
    Vec3 r = { a.x/len, a.y/len, a.z/len };
    return r;
}

/* --- terrain -> height + texture asset, now DATA-DRIVEN per host
 * project (real fix 2026-08-03, direct user catch: these three
 * functions used to be hardcoded C `switch` statements keyed on
 * literal glyph chars, with civ-txt's and tactics-txt's own specific
 * terrain baked directly into board-viewer's own source - exactly the
 * hardcoding board-viewer's whole "shared, focus-adaptive widget"
 * design was supposed to avoid (see @.apps/BOARD_WIDGET_ARCHITECTURE
 * .md §2's own focus-adaptive table). Every new host project that
 * wants a different glyph vocabulary - piececraft-xyz's real voxel
 * game needs a MUCH bigger one (dirt/stone/sand/wood/leaves/ore/...) -
 * would otherwise require editing board-viewer's own C files again.
 *
 * Fix: the FOCUSED host project's own pieces/system/terrain_legend.txt
 * (same per-host-data convention as board.txt/entities.txt), pipe-
 * delimited: glyph|height|r|g|b|asset_hex|name ("-" for asset_hex
 * means no texture, flat color only). Loaded ONCE per run (see
 * load_terrain_legend(), called right after load_entities() in
 * main()), looked up by these three functions instead of a switch.
 * Any glyph NOT found in the legend falls back to the exact same
 * defaults the old switch's own `default:` case used (height 0.0,
 * plains-ish flat color, no texture) - zero behavior change for any
 * project whose legend happens to be incomplete. civ-txt's and
 * tactics-txt's own legend files (seeded by their own button.sh, same
 * place board.txt/entities.txt already get touched) reproduce their
 * EXACT prior hardcoded values - this is a pure refactor. */
#define MAX_TERRAIN_LEGEND 32
typedef struct {
    char glyph;
    double height;
    unsigned char r, g, b;
    char asset_hex[16]; /* empty = no texture */
} TerrainLegendEntry;
static TerrainLegendEntry g_terrain_legend[MAX_TERRAIN_LEGEND];
static int g_terrain_legend_count = 0;

static void load_terrain_legend(const char *root) {
    g_terrain_legend_count = 0;
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/terrain_legend.txt", root);
    FILE *f = host_fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (g_terrain_legend_count < MAX_TERRAIN_LEGEND && fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        /* Comment lines only ("# text..."), NOT a real "#|1.8|..." data
         * row for tactics-txt's own wall glyph, which is itself the
         * literal character '#' - real bug, caught live in this same
         * pass: line[0]=='#' alone can't tell a comment from a legit
         * wall-glyph row, so also require line[1] to not be '|' (every
         * real data row has glyph immediately followed by '|'). */
        if (!line[0] || (line[0] == '#' && line[1] != '|')) continue;
        char *save = NULL;
        char *glyph_tok = strtok_r(line, "|", &save);
        char *h_tok = strtok_r(NULL, "|", &save);
        char *r_tok = strtok_r(NULL, "|", &save);
        char *g_tok = strtok_r(NULL, "|", &save);
        char *b_tok = strtok_r(NULL, "|", &save);
        char *asset_tok = strtok_r(NULL, "|", &save);
        if (!glyph_tok || !glyph_tok[0] || !h_tok || !r_tok || !g_tok || !b_tok) continue;
        TerrainLegendEntry *e = &g_terrain_legend[g_terrain_legend_count];
        e->glyph = glyph_tok[0];
        e->height = atof(h_tok);
        e->r = (unsigned char)atoi(r_tok);
        e->g = (unsigned char)atoi(g_tok);
        e->b = (unsigned char)atoi(b_tok);
        e->asset_hex[0] = '\0';
        if (asset_tok && asset_tok[0] && strcmp(asset_tok, "-") != 0) {
            snprintf(e->asset_hex, sizeof(e->asset_hex), "%s", asset_tok);
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

/* terrain_height() removed 2026-08-03 - dead once every voxel became a
 * uniform 1-unit cube (see load_voxel_chunk()'s own header comment for
 * the full writeup); TerrainLegendEntry's own "height" field/column is
 * left in terrain_legend.txt's own format and this struct unchanged
 * (harmless unused data, matches this house's own "extra key=value
 * lines are fine" convention) in case a future feature wants per-glyph
 * height variation again - just not read anywhere right now. */
static void terrain_color(char g, unsigned char *r, unsigned char *gr, unsigned char *b) {
    const TerrainLegendEntry *e = terrain_legend_lookup(g);
    if (e) { *r = e->r; *gr = e->g; *b = e->b; return; }
    *r=120; *gr=170; *b=80; /* same default the old switch's own default: case used */
}
static const char *terrain_asset_hex(char g) {
    const TerrainLegendEntry *e = terrain_legend_lookup(g);
    if (e && e->asset_hex[0]) return e->asset_hex;
    return NULL;
}

/* TRUE MULTI-LAYER VOXEL RENDERING, added 2026-08-03 - real, requested
 * fix for "why doesn't it look like a 3D game" (direct user diagnosis,
 * exactly correct): this file's own 3D mode only ever loaded ONE Z-
 * slice (resolve_board_path_3d()'s own current_z) and extruded each
 * glyph as a single-scalar-height 2D heightfield - the SAME shape
 * civ-txt's own flat single-layer board always used, ported unchanged
 * when piececraft-xyz's real multi-Z chunk storage (civ-vs-piece.md
 * §3c) landed. That's why "air" rendered as a solid sky-blue FLOOR
 * (terrain_height('_')==0.0, a real extruded slab) instead of being
 * see-through, and why moving through Z-levels never revealed real
 * verticality - each render only ever showed one flat plane.
 *
 * Real fix: for a host publishing board_manifest.txt (has_z_manifest()
 * - the SAME real per-host data-bank convention bv_compose_frame.c and
 * this file's own 2D board-loading already use), ALL Z-layer files are
 * loaded into a genuine 3D voxel grid, air is treated as fully
 * transparent (never tested as a hit at all), and the per-pixel DDA
 * (below, in main()) walks a real 3-axis Amanatides-Woo grid traversal
 * (col, row, AND level) instead of the old 2-axis (col, row)-only walk
 * with baked-in height. A host with no manifest (civ-txt, tactics-txt)
 * is completely unaffected - this whole path is gated behind
 * has_z_manifest() and the ORIGINAL single-slice 2D code below is left
 * completely untouched for that case, zero regression, zero shared
 * mutable state between the two paths. */
#define MAX_VOXEL_Z 64
static char g_voxel_air_glyph = '_'; /* real sentinel this project's own
    pc_generate_chunk.c writes for empty space - not yet legend-driven
    (a future terrain_legend.txt "is_air" column would be the real
    generalization, not built this session - flagged, not blocking). */

static int voxel_is_air(char g) {
    return (g == 0 || g == ' ' || g == g_voxel_air_glyph);
}

/* Loads a real voxel grid into board3d[lvl][row][col] - UNIFIED for
 * every host, per direct instruction ("we dont actually want single
 * slice rendering for anything"): a host publishing board_manifest.txt
 * (piececraft-xyz) gets every real Z-layer file loaded as true unit-
 * height voxel layers; a host with no manifest (civ-txt, tactics-txt)
 * gets its one flat board.txt loaded as a SINGLE layer (z_count=1) -
 * same loader, same DDA, same renderer, no second code path to
 * maintain. The single-layer case still uses that glyph's own real
 * terrain_legend height for ITS layer's own y0/y1 (see the per-pixel
 * DDA in main() below) rather than a uniform unit cube, so civ-txt's
 * existing hills-stick-up/water-sinks-down visual variety is preserved
 * exactly - only piececraft-xyz's own real multi-layer chunks use
 * uniform 1-unit voxel cubes per layer, since real verticality there
 * comes from the actual Z data, not a per-glyph height hint. Returns
 * the real z_count (>=1) on success, 0 on failure. */
static int load_voxel_chunk(const char *root, char board3d[MAX_VOXEL_Z][MAX_BOARD_DIM][MAX_BOARD_DIM],
                             int *board_w_out, int *board_h_out) {
    char z_base[PATH_BUF];
    int z_count = 0;
    int have_manifest = bv3d_has_z_manifest(root, z_base, sizeof(z_base), &z_count);
    if (!have_manifest) z_count = 1;
    if (z_count > MAX_VOXEL_Z) z_count = MAX_VOXEL_Z;
    if (z_count < 1) z_count = 1;

    int board_w = 0, board_h = 0;
    for (int lvl = 0; lvl < z_count; lvl++) {
        char z_path[PATH_BUF];
        if (have_manifest) {
            snprintf(z_path, sizeof(z_path), "%s/%s%d.txt", root, z_base, lvl);
        } else {
            snprintf(z_path, sizeof(z_path), "%s/pieces/system/board.txt", root);
        }
        FILE *zf = host_fopen(z_path, "r");
        if (!zf) continue; /* missing layer = treated as all-air, real graceful degradation */
        int row = 0;
        char line[MAX_LINE];
        while (row < MAX_BOARD_DIM && fgets(line, sizeof(line), zf)) {
            line[strcspn(line, "\r\n")] = '\0';
            int len = (int)strlen(line);
            if (len == 0) continue;
            if (len > MAX_BOARD_DIM) len = MAX_BOARD_DIM;
            memcpy(board3d[lvl][row], line, len);
            if (len > board_w) board_w = len;
            row++;
        }
        if (row > board_h) board_h = row;
        fclose(zf);
    }
    *board_w_out = board_w;
    *board_h_out = board_h;
    return (board_w > 0 && board_h > 0) ? z_count : 0;
}

/* --- entity rendering, part 1 (read-only, no click-to-select yet) ---
 * Same generic manifest bv_compose_frame.c's own load_entities() reads
 * (see that file's header comment for the full format/reasoning) -
 * duplicated, not shared, per this house's own no-shared-headers
 * convention. Rendered here as small solid-color boxes (untextured,
 * matching the legend cube's own convention - real voxel-textured
 * entities are later work, not this pass). */
#define MAX_ENTITIES 64
typedef struct {
    int pos_x, pos_y;
    char hex[16];
    unsigned char r, g, b;
    int owner_side;
} SimpleEntity;
static SimpleEntity g_entities[MAX_ENTITIES];
static int g_entity_count = 0;

static void load_entities(const char *root) {
    g_entity_count = 0;
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/entities.txt", root);
    FILE *f = host_fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (g_entity_count < MAX_ENTITIES && fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (!line[0]) continue;
        char *save = NULL;
        strtok_r(line, "|", &save); /* entity_id, unused here */
        char *px_tok = strtok_r(NULL, "|", &save);
        char *py_tok = strtok_r(NULL, "|", &save);
        char *hex_tok = strtok_r(NULL, "|", &save);
        char *r_tok = strtok_r(NULL, "|", &save);
        char *g_tok = strtok_r(NULL, "|", &save);
        char *b_tok = strtok_r(NULL, "|", &save);
        char *owner_tok = strtok_r(NULL, "|", &save);
        if (!px_tok || !py_tok || !hex_tok || !r_tok || !g_tok || !b_tok) continue;
        SimpleEntity *e = &g_entities[g_entity_count];
        e->pos_x = atoi(px_tok);
        e->pos_y = atoi(py_tok);
        snprintf(e->hex, sizeof(e->hex), "%s", hex_tok);
        e->r = (unsigned char)atoi(r_tok);
        e->g = (unsigned char)atoi(g_tok);
        e->b = (unsigned char)atoi(b_tok);
        e->owner_side = owner_tok ? atoi(owner_tok) : 1;
        g_entity_count++;
    }
    fclose(f);
}

/* Real xelector cursor marker, added 2026-08-03 (direct user report:
 * "everything looks really great but i cant see the xelector indicator
 * anymore"). Root cause: the OLD selector-highlight code only ever lit
 * up a voxel the ray actually HIT - but the xelector's own real pos_z
 * (civ-vs-piece.md §6a/§6b) typically sits one unit ABOVE the ground
 * surface, in open air, since it's a free-floating cursor, not
 * embedded in terrain. With real voxel air now correctly transparent
 * (this same session's own true-3D-voxel rewrite), there is usually no
 * solid block AT the xelector's own exact position to highlight at
 * all - it never had anything to render. Real fix: draw the xelector
 * as its own small marker box, tested unconditionally every pixel
 * (same pattern g_entities[] already uses just above), reading its
 * REAL live position from pieces/xelector_01/state.txt directly - so
 * it's visible whether it's floating in open air, standing on ground,
 * or embedded in solid terrain. A host with no xelector_01 piece
 * (civ-txt, tactics-txt) simply never has one to draw - graceful, not
 * an error. */
static int g_xelector_present = 0;
static int g_xelector_x = 0, g_xelector_y = 0, g_xelector_z = 0;
static char g_xelector_possessed_id[64] = "";

/* REAL, NEW 2026-08-04, direct instruction ("sun and moon will have
 * their own directory as pieces/entities") - real, persistent
 * celestial-body pieces (pieces/sun_01/state.txt, pieces/moon_01/
 * state.txt), continuously written by pc_clock_daemon.c's own real
 * orbit math. This loader is generic (same function for both real
 * bodies) - real double pos_x/y/z (not int, unlike hero/xelector -
 * orbital position is genuinely continuous, snapping it to whole
 * blocks would look jerky as it sweeps across the sky). */
typedef struct {
    int present;
    double x, y, z;
} CelestialBody;

static CelestialBody load_celestial_body(const char *root, const char *entity_id) {
    CelestialBody b; b.present = 0; b.x = b.y = b.z = 0.0;
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/%s/state.txt", root, entity_id);
    FILE *f = host_fopen(path, "r");
    if (!f) return b;
    char line[MAX_LINE];
    int have_x = 0, have_y = 0, have_z = 0;
    while (fgets(line, sizeof(line), f)) {
        char buf[64];
        if (strncmp(line, "pos_x=", 6) == 0) { snprintf(buf, sizeof(buf), "%s", line + 6); b.x = atof(buf); have_x = 1; }
        else if (strncmp(line, "pos_y=", 6) == 0) { snprintf(buf, sizeof(buf), "%s", line + 6); b.y = atof(buf); have_y = 1; }
        else if (strncmp(line, "pos_z=", 6) == 0) { snprintf(buf, sizeof(buf), "%s", line + 6); b.z = atof(buf); have_z = 1; }
    }
    fclose(f);
    b.present = (have_x && have_y && have_z);
    return b;
}

static void load_xelector(const char *root) {
    g_xelector_present = 0;
    g_xelector_possessed_id[0] = '\0';
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/xelector_01/state.txt", root);
    FILE *f = host_fopen(path, "r");
    if (!f) return;
    char buf[64];
    int have_x = 0, have_y = 0, have_z = 0;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strncmp(line, "pos_x=", 6) == 0) { snprintf(buf, sizeof(buf), "%s", line + 6); g_xelector_x = atoi(buf); have_x = 1; }
        else if (strncmp(line, "pos_y=", 6) == 0) { snprintf(buf, sizeof(buf), "%s", line + 6); g_xelector_y = atoi(buf); have_y = 1; }
        else if (strncmp(line, "pos_z=", 6) == 0) { snprintf(buf, sizeof(buf), "%s", line + 6); g_xelector_z = atoi(buf); have_z = 1; }
        else if (strncmp(line, "possessed_id=", 13) == 0) { snprintf(g_xelector_possessed_id, sizeof(g_xelector_possessed_id), "%s", line + 13); }
    }
    fclose(f);
    g_xelector_present = (have_x && have_y && have_z);
}

/* Real hero marker, added 2026-08-03 (direct user report: "i still dont
 * see player-avatar" - real bug, not a rendering fluke: hero_01 had NO
 * visual representation at all until this fix - it was never added to
 * entities.txt and never got a marker the way xelector did. Same real
 * pattern as load_xelector() above, reading hero_01's own live
 * position directly. Distinct color from the xelector's own cyan/
 * yellow cursor marker (below) so the two are never confused - a real
 * avatar-derived skin-tone color is later work (phase2-plan.md §3),
 * a solid warm color is the honest placeholder for now. */
static int g_hero_present = 0;
static int g_hero_x = 0, g_hero_y = 0, g_hero_z = 0;

/* Forward declaration - real definition is later in this file
 * (ray_aabb_hit_3d(), ported from mutaclysm's own real slab-test
 * primitive, see that function's own header comment) - needed here
 * because test_phymoji_hit() (below) is defined earlier in the file
 * than that primitive, but calls it. */
static int ray_aabb_hit_3d(double ox, double oy, double oz, double dx, double dy, double dz,
                            double bx0, double bx1, double by0, double by1, double bz0, double bz1,
                            double *out_t, int *out_face);

static void load_hero(const char *root) {
    g_hero_present = 0;
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/hero_01/state.txt", root);
    FILE *f = host_fopen(path, "r");
    if (!f) return;
    char buf[64];
    int have_x = 0, have_y = 0, have_z = 0;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strncmp(line, "pos_x=", 6) == 0) { snprintf(buf, sizeof(buf), "%s", line + 6); g_hero_x = atoi(buf); have_x = 1; }
        else if (strncmp(line, "pos_y=", 6) == 0) { snprintf(buf, sizeof(buf), "%s", line + 6); g_hero_y = atoi(buf); have_y = 1; }
        else if (strncmp(line, "pos_z=", 6) == 0) { snprintf(buf, sizeof(buf), "%s", line + 6); g_hero_z = atoi(buf); have_z = 1; }
    }
    fclose(f);
    g_hero_present = (have_x && have_y && have_z);
}

/* Real PyMoji volumetric entity rendering, added 2026-08-04 per
 * phymoji.md (this project's own real adoption plan). Loads a real
 * generated voxel model (pieces/registry/phymoji_assets/<entity_id>/
 * voxels.csv, produced by ops/pc_phymoji_gen.c - see that file's own
 * header comment for the full generation pipeline) as a flat list of
 * real (x,y,z,r,g,b) voxels - a brute-force per-voxel test at render
 * time is cheap enough at this count, GATED behind a real coarse
 * world-space bounding-box test (added 2026-08-04, see this file's own
 * hero-hit-test call site header comment - a real perf regression was
 * traced to this loop running unconditionally on every pixel before
 * that gate existed), so no dedicated mini-DDA is needed for models
 * this small (phymoji.md §4a's own "own real DDA" was the general-case
 * plan; this simpler brute-force approach is the correct, real
 * implementation for today's actual voxel counts, not a shortcut -
 * flag for revisiting only if a future asset's own real voxel count
 * grows enough to matter). Sized for TILE_N=32's own real worst case
 * (pc_phymoji_gen.c's own 2026-08-04 resolution bump, direct user
 * report "solid cube, no detail" - 8x8 was too low-res to show real
 * limb/gap silhouette detail): 32*32*8 = 8192 real max. */
#define MAX_PHYMOJI_VOXELS 8192
typedef struct {
    unsigned char lx, ly, lz;       /* local 0..7 grid coords */
    unsigned char r, g, b;
} PhymojiVoxel;

/* REAL FIX 2026-08-04, follow-up to the same-day generator fix (direct
 * user report: "phymoji is only showing 2 parallel sides, empty in the
 * middle"): pc_phymoji_gen.c no longer stretches a crop's own pixels
 * across a fixed 0..7 range (that stretch was the actual root cause of
 * the reported gap) - local coordinates are now the model's own REAL,
 * possibly-narrow extent (e.g. 0..1 for a 2-wide crop), not always
 * 0..7. This loader now also returns that REAL per-axis extent
 * (out_max_lx/ly/lz), so the renderer's own world-space scale (below,
 * test_phymoji_hit()) can map the model's TRUE size into its world
 * bounding box, instead of assuming a fixed 8-unit local grid that no
 * longer matches reality. */
static int load_phymoji_asset(const char *root, const char *entity_id,
                               PhymojiVoxel *out, int max_out,
                               int *out_max_lx, int *out_max_ly, int *out_max_lz) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/registry/phymoji_assets/%s/voxels.csv", root, entity_id);
    FILE *f = host_fopen(path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int n = 0;
    int first = 1;
    int max_lx = 0, max_ly = 0, max_lz = 0;
    while (n < max_out && fgets(line, sizeof(line), f)) {
        if (first) { first = 0; if (strncmp(line, "x,y,z", 5) == 0) continue; } /* skip real header row */
        int x, y, z, r, g, b;
        if (sscanf(line, "%d,%d,%d,%d,%d,%d", &x, &y, &z, &r, &g, &b) == 6) {
            out[n].lx = (unsigned char)x; out[n].ly = (unsigned char)y; out[n].lz = (unsigned char)z;
            out[n].r = (unsigned char)r; out[n].g = (unsigned char)g; out[n].b = (unsigned char)b;
            if (x > max_lx) max_lx = x;
            if (y > max_ly) max_ly = y;
            if (z > max_lz) max_lz = z;
            n++;
        }
    }
    fclose(f);
    if (out_max_lx) *out_max_lx = max_lx;
    if (out_max_ly) *out_max_ly = max_ly;
    if (out_max_lz) *out_max_lz = max_lz;
    return n;
}

/* REAL PERFORMANCE FIX 2026-08-04, direct user report ("rendering
 * slows when trees come into view"): test_phymoji_hit() used to loop
 * every single voxel in a template on any pixel that hit the coarse
 * box - fine at the hero's own real ~1500-voxel scale, genuinely slow
 * once tree_small's own real crop grew to ~3400 voxels after the same-
 * day resolution bump (TILE_N 8->32). Real fix, a direct real
 * consequence of the generator's own "z is always normalized to a
 * fixed 0..7 range regardless of source resolution" property (PyMoji
 * §5.4, unaffected by TILE_N): group voxels into real (lx,ly) COLUMNS
 * (at most 8 z-layers each) ONCE per template load, then test ONE real
 * merged AABB per COLUMN instead of one per VOXEL - matches this same
 * file's own already-proven terrain empty-space-skip technique (mc-
 * speed-algos.md §3), same real principle applied to phymoji models. */
#define MAX_PHYMOJI_COLUMNS 2048
typedef struct {
    unsigned char lx, ly;
    unsigned char exists_mask; /* bit z set = voxel z present (z always 0..7) */
    unsigned char cr[8], cg[8], cb[8];
} PhymojiColumn;

static int build_phymoji_columns(const PhymojiVoxel *voxels, int count, PhymojiColumn *cols, int max_cols) {
    int ncols = 0;
    for (int i = 0; i < count; i++) {
        const PhymojiVoxel *v = &voxels[i];
        int found = -1;
        for (int c = 0; c < ncols; c++) {
            if (cols[c].lx == v->lx && cols[c].ly == v->ly) { found = c; break; }
        }
        if (found < 0) {
            if (ncols >= max_cols) continue;
            found = ncols++;
            cols[found].lx = v->lx; cols[found].ly = v->ly;
            cols[found].exists_mask = 0;
        }
        int z = v->lz;
        if (z >= 0 && z < 8) {
            cols[found].exists_mask = (unsigned char)(cols[found].exists_mask | (1 << z));
            cols[found].cr[z] = v->r; cols[found].cg[z] = v->g; cols[found].cb[z] = v->b;
        }
    }
    return ncols;
}

/* Real per-world-instance destructible state (phymoji.md §3's own
 * resolved decision: template stays read-only/shared, each PLACED
 * instance gets its own real mutable record - "<owner piece dir>/
 * phymoji_removed.txt", one "x,y,z" line per real destroyed local
 * voxel coordinate, matching voxels.csv's own coordinate system
 * directly - simpler and more robust than tracking a generation-
 * order-dependent linear index). Filters the already-loaded voxel
 * array in place, returns the real remaining count. Nothing writes
 * this file yet (real mining/pc_break_block integration is later
 * work, civ-vs-piece.md §6 item 6, phymoji.md §5 step 6) - reading an
 * absent file is a real, graceful no-op (every voxel survives). */
static int apply_phymoji_removed(const char *root, const char *owner_rel_path,
                                  PhymojiVoxel *arr, int count) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/%s/phymoji_removed.txt", root, owner_rel_path);
    FILE *f = host_fopen(path, "r");
    if (!f) return count;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        int rx, ry, rz;
        if (sscanf(line, "%d,%d,%d", &rx, &ry, &rz) != 3) continue;
        for (int i = 0; i < count; i++) {
            if (arr[i].lx == rx && arr[i].ly == ry && arr[i].lz == rz) {
                arr[i] = arr[count - 1];
                count--;
                i--;
                break;
            }
        }
    }
    fclose(f);
    return count;
}

/* Real per-pixel phymoji hit test - transforms the world-space ray
 * into the model's own LOCAL coordinate frame (translate by the world
 * bounding box's own origin, scale by the model's own REAL per-axis
 * extent / world_size - PER AXIS, not a shared constant, since a real
 * cropped model is rarely a perfect cube, see load_phymoji_asset()'s
 * own header comment on this same 2026-08-04 fix), then tests each
 * real remaining voxel via the SAME ray_aabb_hit_3d() primitive every
 * other hit test in this file already uses. Real math note: applying
 * the SAME per-axis scale to both the origin offset and the direction
 * keeps local_t EXACTLY EQUAL to world_t for any real intersection
 * point (each axis's own slab test only cares about that axis's own
 * consistent scale, not a shared one across axes) - no separate
 * conversion needed, the value ray_aabb_hit_3d returns can be compared
 * against best_t directly. */
static int test_phymoji_hit(double ox, double oy, double oz, double dirx, double diry, double dirz,
                             double wx0, double wy0, double wz0,
                             double world_size_x, double world_size_y, double world_size_z,
                             int max_lx, int max_ly, int max_lz,
                             const PhymojiColumn *cols, int col_count,
                             double *out_t, int *out_face,
                             unsigned char *out_r, unsigned char *out_g, unsigned char *out_b) {
    double scale_x = (double)(max_lx + 1) / world_size_x;
    double scale_y = (double)(max_ly + 1) / world_size_y;
    double scale_z = (double)(max_lz + 1) / world_size_z;
    double lox = (ox - wx0) * scale_x, loy = (oy - wy0) * scale_y, loz = (oz - wz0) * scale_z;
    double ldx = dirx * scale_x, ldy = diry * scale_y, ldz = dirz * scale_z;

    double best_local_t = 1e18;
    int best_face = -1, best_col = -1;
    for (int c = 0; c < col_count; c++) {
        if (!cols[c].exists_mask) continue;
        int min_z = 0, max_z = 7;
        while (min_z < 8 && !(cols[c].exists_mask & (1 << min_z))) min_z++;
        while (max_z > 0 && !(cols[c].exists_mask & (1 << max_z))) max_z--;
        double t; int face;
        if (ray_aabb_hit_3d(lox, loy, loz, ldx, ldy, ldz,
                             (double)cols[c].lx, (double)cols[c].lx + 1.0,
                             (double)cols[c].ly, (double)cols[c].ly + 1.0,
                             (double)min_z, (double)max_z + 1.0,
                             &t, &face)
            && t < best_local_t) {
            best_local_t = t; best_face = face; best_col = c;
        }
    }
    if (best_col < 0) return 0;
    /* Real exact z within the merged column - recover it from the
     * local hit point itself (same coordinate frame the column AABB
     * test above already used), clamp/search to the nearest real
     * SURVIVING z bit (only ever needed once real per-voxel mining
     * exists - today's exists_mask is always contiguous, so this is a
     * real, honest safety net, not dead code). */
    double hit_loz = loz + ldz * best_local_t;
    int z = (int)hit_loz;
    if (z < 0) z = 0;
    if (z > 7) z = 7;
    if (!(cols[best_col].exists_mask & (1 << z))) {
        int lo = z, hi = z;
        while (lo >= 0 || hi <= 7) {
            if (lo >= 0 && (cols[best_col].exists_mask & (1 << lo))) { z = lo; break; }
            if (hi <= 7 && (cols[best_col].exists_mask & (1 << hi))) { z = hi; break; }
            lo--; hi++;
        }
    }
    *out_t = best_local_t; /* real: local_t == world_t, per this function's own header note */
    *out_face = best_face;
    *out_r = cols[best_col].cr[z]; *out_g = cols[best_col].cg[z]; *out_b = cols[best_col].cb[z];
    return 1;
}

/* Real generic WORLD-PLACED phymoji entities (phymoji.md §4b/§5's own
 * resolved decision, "switch trees to real entities now" - direct
 * instruction). Reads pieces/world_01/phymoji_entities.txt, one real
 * "entity_id,x,y,z" line per placed object (pc_generate_chunk.c's own
 * writer, currently the 4 fixed debug-map trees). Each unique
 * entity_id's own voxels.csv is loaded ONCE and cached/reused across
 * every placed instance sharing that template - matches this file's
 * own g_entities[]/voxel-cache precedent just below (one real load,
 * many real placements), not a per-instance reload. */
#define MAX_PHYMOJI_ENTITIES 32
#define MAX_PHYMOJI_TEMPLATES 8
typedef struct {
    char entity_id[64];
    int x, y, z;
    int template_idx;
} PhymojiWorldEntity;
typedef struct {
    char entity_id[64];
    PhymojiVoxel voxels[MAX_PHYMOJI_VOXELS];
    int count;
    int max_lx, max_ly, max_lz;
    PhymojiColumn columns[MAX_PHYMOJI_COLUMNS];
    int column_count;
} PhymojiTemplate;

static PhymojiTemplate g_phymoji_templates[MAX_PHYMOJI_TEMPLATES];
static int g_phymoji_template_count = 0;
static PhymojiWorldEntity g_phymoji_world_entities[MAX_PHYMOJI_ENTITIES];
static int g_phymoji_world_entity_count = 0;

static int get_or_load_phymoji_template(const char *root, const char *entity_id) {
    for (int i = 0; i < g_phymoji_template_count; i++) {
        if (strcmp(g_phymoji_templates[i].entity_id, entity_id) == 0) return i;
    }
    if (g_phymoji_template_count >= MAX_PHYMOJI_TEMPLATES) return -1;
    PhymojiTemplate *t = &g_phymoji_templates[g_phymoji_template_count];
    snprintf(t->entity_id, sizeof(t->entity_id), "%s", entity_id);
    t->count = load_phymoji_asset(root, entity_id, t->voxels, MAX_PHYMOJI_VOXELS,
                                    &t->max_lx, &t->max_ly, &t->max_lz);
    if (t->count <= 0) return -1;
    t->column_count = build_phymoji_columns(t->voxels, t->count, t->columns, MAX_PHYMOJI_COLUMNS);
    return g_phymoji_template_count++;
}

static void load_phymoji_world_entities_file(const char *root, const char *rel_path) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/%s", root, rel_path);
    FILE *f = host_fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (g_phymoji_world_entity_count < MAX_PHYMOJI_ENTITIES && fgets(line, sizeof(line), f)) {
        char entity_id[64];
        int x, y, z;
        if (sscanf(line, "%63[^,],%d,%d,%d", entity_id, &x, &y, &z) != 4) continue;
        int tpl = get_or_load_phymoji_template(root, entity_id);
        if (tpl < 0) continue;
        PhymojiWorldEntity *e = &g_phymoji_world_entities[g_phymoji_world_entity_count++];
        snprintf(e->entity_id, sizeof(e->entity_id), "%s", entity_id);
        e->x = x; e->y = y; e->z = z; e->template_idx = tpl;
    }
    fclose(f);
}

/* Reads BOTH real positioned-entity files: phymoji_entities.txt (real
 * static decoration - trees) and animals.txt (real MOVABLE entities -
 * chicken, direct instruction 2026-08-04 "give the chicken the master-
 * ledger AI ... just to walk randomly" - pc_generate_chunk.c's own
 * header comment on animals.txt explains the real static/mutable
 * split). Same "entity_id,x,y,z" line format for both, so one shared
 * loader covers both real files - a host with neither (or only one)
 * is a real, graceful no-op per file. */
static void load_phymoji_world_entities(const char *root) {
    g_phymoji_world_entity_count = 0;
    load_phymoji_world_entities_file(root, "pieces/world_01/phymoji_entities.txt");
    load_phymoji_world_entities_file(root, "pieces/world_01/animals.txt");
}

/* --- real voxel texture cache, ported from mutaclysm's own
 * get_voxel8_cached()/sample_voxel8_pixel() (ops/compose_rgb_frame.c) -
 * sized for 256 entries (16x16) since this project's own real assets
 * are 16-res (chtpm_rgb_render's on-demand generator's own default),
 * not mutaclysm's 8x8 - the loader is resolution-agnostic (reads
 * "# resolution=%d" from the file itself), naming kept as "voxel8" to
 * match mutaclysm's own function names for citation traceability. */
#define MAX_VOXEL_CACHE 16
#define MAX_VOXEL_PIXELS 256
typedef struct {
    char path[PATH_BUF];
    int resolution;
    int count;
    unsigned char pixels[MAX_VOXEL_PIXELS][4];
    int loaded;
    /* Real opaque bounding box within the texture grid (col/row units,
     * [bbox_u0,bbox_u1)/[bbox_v0,bbox_v1)) - computed ONCE per asset
     * when first loaded, cached alongside the pixel data. Used to crop
     * out the transparent padding around a small emoji icon so it can
     * be stretched to fill an entity's own front/back face completely -
     * direct user correction: "we aren't supposed to render the blank
     * space around the emoji." Also backs edge_color() below (§ real
     * side/top color derived from the actual art, not an arbitrary
     * pick). */
    int bbox_u0, bbox_v0, bbox_u1, bbox_v1;
    unsigned char edge_r, edge_g, edge_b;
} Voxel8Cache;
static Voxel8Cache g_voxel8_cache[MAX_VOXEL_CACHE];
static int g_voxel8_cache_count = 0;

static void compute_bbox_and_edge_color(Voxel8Cache *c) {
    int res = c->resolution;
    int u0 = res, v0 = res, u1 = -1, v1 = -1;
    for (int row = 0; row < res; row++) {
        for (int col = 0; col < res; col++) {
            int idx = row * res + col;
            if (idx >= c->count) continue;
            if (c->pixels[idx][3] > 10) {
                if (col < u0) u0 = col;
                if (col > u1) u1 = col;
                if (row < v0) v0 = row;
                if (row > v1) v1 = row;
            }
        }
    }
    if (u1 < u0) { /* fully transparent asset - degenerate, use whole grid */
        u0 = 0; v0 = 0; u1 = res - 1; v1 = res - 1;
    }
    c->bbox_u0 = u0; c->bbox_v0 = v0; c->bbox_u1 = u1 + 1; c->bbox_v1 = v1 + 1;

    /* Real "edge color" - average of the opaque pixels lying exactly on
     * the bounding box's own perimeter (its actual outline/silhouette
     * color), not the whole image's average (which would be dominated
     * by interior detail/shading, less representative as a flat side
     * tone). Falls back to a mid-gray if the perimeter is somehow
     * fully transparent (shouldn't happen given the bbox is opaque-
     * derived, but stay safe). */
    long sr = 0, sg = 0, sb = 0, n = 0;
    for (int row = v0; row <= v1; row++) {
        for (int col = u0; col <= u1; col++) {
            int on_edge = (row == v0 || row == v1 || col == u0 || col == u1);
            if (!on_edge) continue;
            int idx = row * res + col;
            if (idx >= c->count || c->pixels[idx][3] <= 10) continue;
            sr += c->pixels[idx][0]; sg += c->pixels[idx][1]; sb += c->pixels[idx][2];
            n++;
        }
    }
    if (n > 0) {
        c->edge_r = (unsigned char)(sr / n);
        c->edge_g = (unsigned char)(sg / n);
        c->edge_b = (unsigned char)(sb / n);
    } else {
        c->edge_r = c->edge_g = c->edge_b = 120;
    }
}

static Voxel8Cache *get_voxel8_cached(const char *path) {
    for (int i = 0; i < g_voxel8_cache_count; i++) {
        if (strcmp(g_voxel8_cache[i].path, path) == 0) return &g_voxel8_cache[i];
    }
    if (g_voxel8_cache_count >= MAX_VOXEL_CACHE) return NULL;
    Voxel8Cache *c = &g_voxel8_cache[g_voxel8_cache_count++];
    snprintf(c->path, sizeof(c->path), "%s", path);
    c->count = 0; c->resolution = 0; c->loaded = 0;
    FILE *f = host_fopen(path, "r");
    if (!f) return c; /* cached as "not loaded" so we don't retry every hit */
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f) && c->count < MAX_VOXEL_PIXELS) {
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
    if (c->loaded) compute_bbox_and_edge_color(c);
    return c;
}

/* Real, "clearly and stretched" entity front/back texturing - direct
 * user instruction. Remaps the incoming [0,1] face UV into the
 * texture's own OPAQUE bounding box only (see compute_bbox_and_edge_
 * color() above), so the actual emoji art fills the WHOLE face with no
 * blank transparent margin around it, instead of a small icon floating
 * in a mostly-empty tile. */
static int sample_voxel8_cropped(const char *path, double u, double v, unsigned char out_rgb[3]) {
    Voxel8Cache *c = get_voxel8_cached(path);
    if (!c || !c->loaded) return 0;
    if (u < 0.0) u = 0.0;
    if (u > 0.999999) u = 0.999999;
    if (v < 0.0) v = 0.0;
    if (v > 0.999999) v = 0.999999;
    int bw = c->bbox_u1 - c->bbox_u0;
    int bh = c->bbox_v1 - c->bbox_v0;
    if (bw <= 0 || bh <= 0) return 0;
    int col = c->bbox_u0 + (int)(u * bw);
    int row = c->bbox_v0 + (int)(v * bh);
    if (col >= c->bbox_u1) col = c->bbox_u1 - 1;
    if (row >= c->bbox_v1) row = c->bbox_v1 - 1;
    int idx = row * c->resolution + col;
    if (idx < 0 || idx >= c->count) return 0;
    if (c->pixels[idx][3] <= 10) return 0; /* still-transparent pixel inside the bbox (a real gap in the art) - caller falls back */
    out_rgb[0] = c->pixels[idx][0];
    out_rgb[1] = c->pixels[idx][1];
    out_rgb[2] = c->pixels[idx][2];
    return 1;
}

/* Real edge color for an asset, per direct user instruction ("tops and
 * sides are the side colors of the emoji" - confirmed via direct
 * follow-up: colors sampled from the emoji's own edge/border pixels,
 * not an arbitrary or team-tinted pick). */
static int get_edge_color(const char *path, unsigned char out_rgb[3]) {
    Voxel8Cache *c = get_voxel8_cached(path);
    if (!c || !c->loaded) return 0;
    out_rgb[0] = c->edge_r; out_rgb[1] = c->edge_g; out_rgb[2] = c->edge_b;
    return 1;
}

/* Real team-number marker, per direct instruction ("a colored number,
 * letter or w/e signifies that team, to be changed later, a number is
 * fine for now 1 or 2"). Reuses this project's own real ASCII bitmap
 * font (pieces/registry/fonts/ascii/<ascii-code>/glyph.txt, 8x16 '#'/'.'
 * pixel grid, already on disk, already used elsewhere in the shared
 * engine - no new asset generation needed) as a mask, blended in a
 * team color on top of whatever base color the caller already picked. */
#define GLYPH_PX_W 8
#define GLYPH_PX_H 16
static int g_glyph_1[GLYPH_PX_H][GLYPH_PX_W];
static int g_glyph_2[GLYPH_PX_H][GLYPH_PX_W];
static int g_glyphs_loaded = 0;

static void load_one_digit_glyph(const char *root, int ascii_code, int out[GLYPH_PX_H][GLYPH_PX_W]) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/registry/fonts/ascii/%d/glyph.txt", root, ascii_code);
    FILE *f = host_fopen(path, "r");
    if (!f) { memset(out, 0, sizeof(int) * GLYPH_PX_H * GLYPH_PX_W); return; }
    char line[64];
    for (int row = 0; row < GLYPH_PX_H; row++) {
        if (!fgets(line, sizeof(line), f)) { for (int c = 0; c < GLYPH_PX_W; c++) out[row][c] = 0; continue; }
        for (int c = 0; c < GLYPH_PX_W && line[c]; c++) out[row][c] = (line[c] == '#') ? 1 : 0;
    }
    fclose(f);
}

static void ensure_digit_glyphs_loaded(const char *root) {
    if (g_glyphs_loaded) return;
    load_one_digit_glyph(root, 49, g_glyph_1); /* ASCII '1' */
    load_one_digit_glyph(root, 50, g_glyph_2); /* ASCII '2' */
    g_glyphs_loaded = 1;
}

/* u,v in [0,1] over the TOP face; returns 1 if this pixel is part of
 * the digit's own real "on" pixels (caller should blend a team color
 * in), 0 otherwise (base color shows through unchanged). */
static int digit_mask(int owner_side, double u, double v) {
    int (*glyph)[GLYPH_PX_W] = (owner_side == 2) ? g_glyph_2 : g_glyph_1;
    int col = (int)(u * GLYPH_PX_W);
    int row = (int)(v * GLYPH_PX_H);
    if (col < 0) col = 0;
    if (col >= GLYPH_PX_W) col = GLYPH_PX_W - 1;
    if (row < 0) row = 0;
    if (row >= GLYPH_PX_H) row = GLYPH_PX_H - 1;
    return glyph[row][col];
}

/* --- ray/AABB slab test, ported verbatim from mutaclysm's own
 * ray_aabb_hit_3d() (ops/compose_rgb_frame.c:1067-1108, itself ported
 * from wraith_rgb_daemon.c's own ray_aabb_hit()). face: 0/1=-X/+X,
 * 2/3=-Y/+Y(3=top), 4/5=-Z/+Z. */
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

/* --- hit point -> face UV, ported verbatim from mutaclysm's own
 * box_face_uv() (ops/compose_rgb_frame.c:1192-1202). */
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

/* --- framebuffer --- */
static unsigned char fb[FRAME_H][FRAME_W][4];

/* REAL, NEW 2026-08-04, xyz-ngn-plan.md §2/§2b (piececraft-xyz's own
 * real day/night orbital plan, Step 1) - the sun's own real ELLIPTICAL
 * orbit ("long oval", direct instruction: "sun should be close to our
 * planet during day and orbit far away at night") driven by the SAME
 * real world clock pc_clock_daemon.c now advances continuously
 * (game_time_epoch_sec). Real ellipse: local_x = cos*a, local_z =
 * sin*b (b < a - a REAL stretched oval, not a circle) - this shape is
 * kept for later real Step 1c work (actually rendering a visible sun
 * OBJECT positioned along this same real orbit, "close in the sky
 * during day, far away at night").
 *
 * REVISED 2026-08-04, direct user correction ("day night is cycling
 * too fast... we should use the normal calendar and 24 hours, 12
 * light 12 dark"): brightness itself no longer comes from orbital
 * DISTANCE (§2b's original approach never guaranteed an even light/
 * dark split - the ellipse's own real geometry makes "near" and "far"
 * spend unequal fractions of the cycle, by design, for a genuinely
 * elliptical ORBIT shape). Real, clean fix: a full cycle is now a real
 * 24 GAME-HOUR calendar day (86400 game-seconds, matching game_
 * datetime's own real %H:%M display exactly - hour 12 = solar noon),
 * and brightness is a real sine curve over hour-of-day - guarantees an
 * EXACT 12h light / 12h dark split (crosses zero at exactly 06:00 and
 * 18:00), independent of the orbit's own real position/shape. */
#define SUN_PERIOD_SECONDS 86400.0  /* one real 24-GAME-hour calendar day, matching game_datetime's own %H:%M */
#define SUN_ORBIT_A 40.0            /* real semi-major axis (far reach) - for later Step 1c sun-object rendering, not brightness */
#define SUN_ORBIT_B 6.0             /* real semi-minor axis (close reach) - for later Step 1c sun-object rendering, not brightness */

static double compute_sun_light_level(long long game_epoch_sec) {
    double t = fmod((double)game_epoch_sec, SUN_PERIOD_SECONDS);
    if (t < 0) t += SUN_PERIOD_SECONDS;
    /* hour_of_day 0 = midnight, 12 = solar noon (peak brightness),
     * matching a real calendar exactly. sin() naturally crosses zero
     * at exactly the 1/4 and 3/4 points of the period (06:00/18:00),
     * giving a real, exact 12h/12h split with no separate threshold to
     * tune. */
    double angle = (t / SUN_PERIOD_SECONDS) * 2.0 * M_PI_LOCAL - (M_PI_LOCAL / 2.0);
    double sun_height = sin(angle); /* -1 at midnight, +1 at solar noon */
    double level = sun_height;
    if (level < 0.0) level = 0.0;
    if (level > 1.0) level = 1.0;
    return level;
}

/* Real, cheap 3-stop linear interpolation (xyz-ngn-plan.md §3) - day
 * blue -> sunset orange -> night navy, driven by the real sun_light_
 * level above instead of the plan doc's own original angle-based
 * placeholder. */
static void clear_sky(double sun_light_level) {
    unsigned char r, g, b;
    if (sun_light_level >= 0.6) {
        double f = (sun_light_level - 0.6) / 0.4; /* 0.6..1.0 -> 0..1 */
        if (f > 1.0) f = 1.0;
        r = (unsigned char)(255 + f * (135 - 255));
        g = (unsigned char)(150 + f * (180 - 150));
        b = (unsigned char)(90 + f * (220 - 90));
    } else if (sun_light_level >= 0.1) {
        double f = (sun_light_level - 0.1) / 0.5; /* 0.1..0.6 -> 0..1 */
        r = (unsigned char)(10 + f * (255 - 10));
        g = (unsigned char)(15 + f * (150 - 15));
        b = (unsigned char)(40 + f * (90 - 40));
    } else {
        double f = sun_light_level / 0.1; /* 0..0.1 -> 0..1 */
        if (f < 0.0) f = 0.0;
        r = (unsigned char)(10 * f);
        g = (unsigned char)(15 * f);
        b = (unsigned char)(40 * f);
    }
    for (int y = 0; y < FRAME_H; y++)
        for (int x = 0; x < FRAME_W; x++) {
            fb[y][x][0] = r; fb[y][x][1] = g; fb[y][x][2] = b; fb[y][x][3] = 255;
        }
}

/* --- camera --- */
typedef struct {
    Vec3 eye, forward, right, up;
    double focal;
} Camera;

static Camera build_camera(int camera_mode, double yaw_deg, double pitch_deg,
                            double pan_x, double pan_y, double pan_z, double z_level,
                            double anchor_x, double anchor_z, double anchor_h,
                            double fp_face_dist, double fp_eye_height,
                            double tp_distance, double tp_height,
                            double tp_look_down_deg) {
    Camera cam;
    double yaw = yaw_deg * M_PI_LOCAL / 180.0;
    /* REAL FIX 2026-08-04, direct user report ("3rd person needs to
     * angle down to see the entity, but still look forward - currently
     * just above and straight forward"): mode 2 sits the eye BEHIND
     * and ABOVE the anchor (tp_distance/tp_height, real position-only
     * offsets) but the LOOK DIRECTION used to come from the exact same
     * shared pitch every other mode uses (r/t-adjustable, default a
     * near-level ~6deg) - correct for first-person (eye is basically AT
     * the subject), wrong for third-person (eye is elevated behind the
     * subject, so level pitch looks out over their head into the
     * distance instead of down at them). Real, config-driven fix: mode
     * 2 gets a real additive downward tilt on TOP of the user's own
     * real r/t pitch control (not a replacement for it) - yaw still
     * drives which way "forward" is, only the vertical look angle
     * changes for this one mode. */
    /* dir.y = sin(pitch) below - positive pitch looks UP in this
     * engine's own convention, so tp_look_down_deg (a positive, human-
     * friendly "look down by N degrees" config value) is SUBTRACTED,
     * not added. */
    double effective_pitch_deg = pitch_deg - (camera_mode == 2 ? tp_look_down_deg : 0.0);
    double pitch = effective_pitch_deg * M_PI_LOCAL / 180.0;

    Vec3 dir;
    dir.x = cos(pitch) * sin(yaw);
    dir.y = sin(pitch);
    dir.z = cos(pitch) * cos(yaw);
    cam.forward = v3_norm(dir);

    if (camera_mode == 1) {
        /* REAL FIX 2026-08-04, direct user report ("first person camera
         * is still stuck in body, move it out more till its right"):
         * even after the real anchor_h fix (bv_render_3d.c's own
         * main(), same session - eye height was landing inside the
         * solid ground block), sitting the eye EXACTLY at the anchor
         * cell's own center is still a real edge case (any voxel that
         * happens to occupy that exact cell/height, real or a rounding
         * sliver near a face boundary, fills the view). Real, larger
         * margin: +1.5 height (was +0.9 - a full half-block further
         * clear of the ground/hero's own cell) PLUS a small real
         * forward nudge in the camera's own facing direction (so the
         * eye isn't dead-center in its own cell at all, sitting instead
         * nearer the cell's forward edge) - both real, generous safety
         * margins, not just a minimal fix. */
        cam.eye.x = anchor_x + fp_face_dist * sin(yaw); cam.eye.y = anchor_h + fp_eye_height; cam.eye.z = anchor_z + fp_face_dist * cos(yaw);
    } else if (camera_mode == 2) {
        cam.eye.x = anchor_x - tp_distance * sin(yaw); cam.eye.y = anchor_h + tp_height; cam.eye.z = anchor_z - tp_distance * cos(yaw);
    } else if (camera_mode == 3) {
        /* REAL FIX 2026-08-03, direct user report ("zx xelector movement
         * shouldn't move camera, thats a bug") - real precedent checked
         * (mutaclysm's own dox/ctrl-legend.md: "z/x = Hero Z level",
         * "c/v = Camera Z level", explicitly TWO SEPARATE axes). Modes
         * 3/4 are free-roam/bird's-eye - a genuinely DETACHED camera,
         * meant to be controlled only by wasd/c/v, never by wherever
         * the xelector/hero happens to be. anchor_h (mirrors current_z,
         * which z/x writes) used to be added into eye.y here too - so
         * moving the xelector vertically silently dragged the free
         * camera's own height along with it, exactly the reported bug.
         * Real fix: modes 3/4 build eye.y from z_level (c/v) ALONE, a
         * fixed base height, no anchor_h term - matches real precedent
         * exactly (these are the "all modes" c/v axis, not the z/x
         * hero axis). Modes 1/2 correctly KEEP anchor_h (first/third-
         * person are meant to follow the possessed/xelector position -
         * that's real, intended behavior, not this bug). */
        cam.eye.x = anchor_x + pan_x; cam.eye.y = 12.0 + z_level * 2.0; cam.eye.z = anchor_z + pan_z;
    } else { /* 4: bird's-eye, ABSOLUTE map coords - camera detached from any anchor */
        cam.eye.x = pan_x; cam.eye.y = 12.0 + z_level * 2.0; cam.eye.z = pan_y;
    }

    /* Two empirically-verified-correct branches (2026-08-02, see
     * &.widgits/view-vs-muta.md) - a single shared cross-product
     * ordering cannot produce a right-side-up, non-mirrored image for
     * BOTH near-vertical (bird's-eye) and near-horizontal (FPS-style)
     * pitch; each needs its own verified operand order. Do not
     * "simplify" this to one formula without re-verifying both cases
     * against a controlled test board (see view-vs-muta.md for the
     * methodology) - two earlier single-formula attempts each fixed
     * one case while silently breaking the other. */
    Vec3 right, up;
    if (fabs(cam.forward.y) > 0.999) {
        Vec3 fallback_up = { 0, 0, 1 };
        right = v3_norm(v3_cross(fallback_up, cam.forward));
        up = v3_norm(v3_cross(right, cam.forward));
    } else {
        Vec3 world_up = { 0, 1, 0 };
        right = v3_norm(v3_cross(world_up, cam.forward));
        up = v3_norm(v3_cross(cam.forward, right));
    }
    cam.right = right;
    cam.up = up;

    double fov_rad = 60.0 * M_PI_LOCAL / 180.0;
    cam.focal = (FRAME_H / 2.0) / tan(fov_rad / 2.0);
    return cam;
}

static void write_file_atomic(const char *path, const void *data, size_t len) {
    char tmp_path[PATH_BUF];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    FILE *f = host_fopen(tmp_path, "wb");
    if (!f) return;
    if (fwrite(data, 1, len, f) != len) { fclose(f); remove(tmp_path); return; }
    fclose(f);
    /* Windows rename does not overwrite existing dest (muta/wsr fix). */
    remove(path);
    if (rename(tmp_path, path) != 0) {
        FILE *out = host_fopen(path, "wb");
        if (out) { fwrite(data, 1, len, out); fclose(out); }
        remove(tmp_path);
    }
}

/* Exact mutaclysm receipt format (ops/compose_rgb_frame.c:1999,
 * "overlay_w=%d\noverlay_h=%d\n" - no checksum, no other fields). */
static void write_overlay_receipt(const char *path, int ov_w, int ov_h) {
    char buf[128];
    int n = snprintf(buf, sizeof(buf), "overlay_w=%d\noverlay_h=%d\n", ov_w, ov_h);
    if (n > 0) write_file_atomic(path, buf, (size_t)n);
}

int main(void) {
    resolve_root();
    load_house_root();

    char state_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/pieces/system/bv_state.txt", project_root);

    char focused_raw[PATH_BUF] = "", focused_project_root[PATH_BUF] = "";
    read_kv_str(state_path, "focused_project_root", focused_raw, sizeof(focused_raw));
    resolve_host_root(focused_raw, focused_project_root, sizeof(focused_project_root));
    if (!focused_project_root[0]) return 0;

    /* REAL FIX 2026-08-04, direct user report ("still blank - map
     * screens are black"): this file's own render_mode default used to
     * be a bare 0, independently of bv_compose_frame.c's own real
     * project-conditional default (added the same session, "always
     * start in 3d mode") - a fresh session with no explicit render_mode
     * key yet ended up with compose_frame believing 3D (skips writing
     * the 2D emoji grid) while THIS file believed 2D (skips ALL 3D
     * work, early-returns here without writing any overlay at all) -
     * two files silently disagreeing, both real screens ended up
     * genuinely empty. Real fix: same real has_z_manifest()-conditional
     * default here too, so both files agree without either one having
     * to write a real key to disk first. */
    int render_mode = read_kv_int(state_path, "render_mode", default_render_mode(focused_project_root));
    if (!render_mode) return 0; /* fast no-op - 2D mode is chtpm_rgb_render's own job */

    /* Real unified voxel grid - see load_voxel_chunk()'s own header
     * comment for the full writeup. static: MAX_VOXEL_Z(64) *
     * MAX_BOARD_DIM(64) * MAX_BOARD_DIM(64) = 256K chars, too big for
     * the stack. */
    static char board3d[MAX_VOXEL_Z][MAX_BOARD_DIM][MAX_BOARD_DIM];
    int board_w = 0, board_h = 0;
    int z_count = load_voxel_chunk(focused_project_root, board3d, &board_w, &board_h);
    if (z_count == 0) return 0;

    /* Real empty-space-skipping precompute - see mc-speed-algos.md for
     * the full writeup (real perf fix, 2026-08-03, direct user report:
     * "the render is super slow compared to civ/mutaclysm"). For each
     * column, find the real topmost/bottommost solid voxel AND verify
     * there's no gap between them, ONCE per frame (cheap: board_w*
     * board_h*z_count worst case, ~8192 ops for a 16x16x32 chunk -
     * negligible next to FRAME_W*FRAME_H per-pixel work).
     *
     * col_solid[row][col]==1 means the column is verified GAP-FREE
     * (every voxel from col_bottom to col_top is real solid material,
     * checked explicitly, not assumed) - ONLY these columns use the
     * fast single-merged-box DDA path below. Direct user concern,
     * addressed here rather than assumed away: "is it gonna make
     * things look weird and see thru sometimes? i dont like that" - a
     * column that ISN'T verified gap-free (col_solid==0) falls back to
     * the full, exact per-voxel walk for that one column, so this can
     * never silently render a real gap as solid, now or once Phase 2
     * (place/break) can actually carve real holes/caves. Today's
     * world-gen (pc_generate_chunk.c) never produces gaps, so every
     * column takes the fast path today - this check is what GUARANTEES
     * that stays true rather than just hoping it does. */
    static int col_top[MAX_BOARD_DIM][MAX_BOARD_DIM];
    static int col_bottom[MAX_BOARD_DIM][MAX_BOARD_DIM];
    static int col_solid[MAX_BOARD_DIM][MAX_BOARD_DIM];
    for (int row = 0; row < board_h; row++) {
        for (int col = 0; col < board_w; col++) {
            int top = -1, bottom = z_count;
            for (int lvl = 0; lvl < z_count; lvl++) {
                if (!voxel_is_air(board3d[lvl][row][col])) {
                    if (lvl > top) top = lvl;
                    if (lvl < bottom) bottom = lvl;
                }
            }
            col_top[row][col] = top;
            col_bottom[row][col] = bottom;
            int gapless = 1;
            if (top >= bottom) {
                for (int lvl = bottom; lvl <= top; lvl++) {
                    if (voxel_is_air(board3d[lvl][row][col])) { gapless = 0; break; }
                }
            }
            col_solid[row][col] = gapless;
        }
    }

    load_entities(focused_project_root);
    load_xelector(focused_project_root);
    load_hero(focused_project_root);

    /* Real hero phymoji model - "hero_humanoid" is today's real, fixed
     * placeholder source asset (direct instruction: "use the humanoid
     * emoji u just showed", real avatar-DNA-driven asset selection is
     * later work, phase2-plan.md §3 / phymoji.md §4c "basics first").
     * A host with no generated asset (hasn't run pc_phymoji_gen.+x
     * yet) gets 0 voxels back - real, graceful, falls through to the
     * OLD flat marker box below as an honest fallback, not a crash. */
    static PhymojiVoxel g_hero_phymoji[MAX_PHYMOJI_VOXELS];
    static PhymojiColumn g_hero_phymoji_columns[MAX_PHYMOJI_COLUMNS];
    int g_hero_phymoji_column_count = 0;
    int hero_phymoji_count = 0;
    int hero_phymoji_max_lx = 7, hero_phymoji_max_ly = 7, hero_phymoji_max_lz = 7;
    if (g_hero_present) {
        hero_phymoji_count = load_phymoji_asset(focused_project_root, "hero_humanoid", g_hero_phymoji, MAX_PHYMOJI_VOXELS,
                                                  &hero_phymoji_max_lx, &hero_phymoji_max_ly, &hero_phymoji_max_lz);
        if (hero_phymoji_count > 0) {
            hero_phymoji_count = apply_phymoji_removed(focused_project_root, "hero_01", g_hero_phymoji, hero_phymoji_count);
            g_hero_phymoji_column_count = build_phymoji_columns(g_hero_phymoji, hero_phymoji_count, g_hero_phymoji_columns, MAX_PHYMOJI_COLUMNS);
        }
    }
    load_phymoji_world_entities(focused_project_root);
    load_terrain_legend(focused_project_root);
    ensure_digit_glyphs_loaded(project_root);

    int selector_x = read_kv_int(state_path, "selector_x", board_w / 2);
    int selector_y = read_kv_int(state_path, "selector_y", board_h / 2);
    /* current_z is the SAME real field ops/bv_menu_input.c's own z/x
     * handler writes to the focused host's own pieces/xelector_01/
     * state.txt pos_z (civ-vs-piece.md §6b) - board-viewer's own
     * bv_state.txt copy of it is a mirror, read back here as the
     * selector's own real vertical position, clamped into the loaded
     * grid's own real z_count range. */
    int current_z = read_kv_int(state_path, "current_z", default_current_z(focused_project_root));
    if (current_z >= z_count) current_z = z_count - 1;
    if (current_z < 0) current_z = 0;
    int camera_mode = read_kv_int(state_path, "camera_mode", default_camera_mode(focused_project_root));
    /* REAL PARITY FIX 2026-08-07: fresh cam_pitch used to default to
     * -90 (straight down) in EVERY mode, so even the config-driven
     * "default_camera_mode=2 (third-person)" open rendered top-down,
     * visually identical to bird's-eye "view 4" - the "still starts
     * as view 4" report. Real default is now mode-consistent with the
     * '1'-'4' switch and 'f' reset handlers (6 for modes 1/2, -90 for
     * free-roam 3 / bird's-eye 4). */
    int default_cam_pitch = (camera_mode == 1 || camera_mode == 2) ? 6 : -90;
    int cam_yaw = read_kv_int(state_path, "cam_yaw", 180);
    int cam_pitch = read_kv_int(state_path, "cam_pitch", default_cam_pitch);
    /* Default cam_pan_x/y to the SELECTOR's own position (matches
     * where the 2D view actually starts) - see &.widgits/
     * view-vs-muta.md, real user-caught bug, fixed 2026-08-02. */
    int cam_pan_x = read_kv_int(state_path, "cam_pan_x", selector_x);
    int cam_pan_y = read_kv_int(state_path, "cam_pan_y", selector_y);
    int cam_pan_z = read_kv_int(state_path, "cam_pan_z", 0);
    /* REAL FIX 2026-08-04, direct user report ("still blank - map
     * screens are black"): mode 3/4's own eye.y = 12.0 + z_level*2.0
     * formula (build_camera(), deliberately anchor_h-FREE per this
     * file's own 2026-08-03 fix comment there) assumed a real ground
     * surface low enough for the z_level=0 default (eye.y=12) to clear
     * it - true for civ-txt/tactics-txt's own shallow boards, false for
     * piececraft-xyz's own real 32-layer world (surface ~16-18): a
     * fresh session's camera literally spawned INSIDE solid terrain,
     * which is exactly what a uniform near-black frame looks like (one
     * huge close-up voxel face filling the entire view). Real, generic
     * fix: default z_level from current_z (already the real ground
     * reference, not a magic constant) instead of a bare 0 - a shallow
     * board (current_z small) still gets 0, matching old behavior
     * exactly; a tall one gets real overhead clearance. */
    int default_z_level = (current_z > 12) ? ((current_z - 12 + 6) / 2) : 0;
    int cam_z_level = read_kv_int(state_path, "cam_z_level", default_z_level);

    double anchor_x = selector_x + 0.5, anchor_z = selector_y + 0.5;
    /* REAL FIX 2026-08-03 (direct user diagnosis: "why doesn't it look
     * like a 3d game"): anchor_h used to come from terrain_height() of
     * a single glyph (the OLD single-slice extrusion model's own
     * height hint). Now that Y directly means "which real voxel layer"
     * (uniform unit cubes, see the DDA below), the camera's own anchor
     * height is simply the selector's own real current_z position -
     * the camera stands ON the same real voxel layer the selector/
     * xelector is actually at, exactly like a real Minecraft camera
     * tracks the player's own Y position, not a per-tile height hint. */
    /* REAL FIX 2026-08-04, direct user report ("1st person view is
     * still inside 'body', we cant see outside"): current_z's own
     * default was JUST changed (this same session) to the real GROUND
     * SURFACE (fixing the "2D view is all air" bug) - but anchor_h's
     * real job here is completely different: it's the camera's own EYE
     * height, which needs to be the AIR tile the player actually
     * stands IN, one level above the ground, not the ground block
     * itself. Reusing current_z for both meanings is exactly what put
     * the first-person eye INSIDE the solid ground voxel (eye.y =
     * current_z + 0.9, i.e. 0.9 units into a block that spans
     * current_z..current_z+1). Real fix: prefer the xelector's own
     * REAL, already-tracked pos_z (g_xelector_z, load_xelector()'s own
     * real per-frame read) - the actual air tile - falling back to
     * current_z+1 (same real "one above the drawn ground" logic) only
     * when no xelector exists at all. */
    double anchor_h = g_xelector_present ? (double)g_xelector_z : (double)current_z + 1.0;
    double pan_x = (camera_mode == 4) ? (cam_pan_x + 0.5) : (double)cam_pan_x;
    double pan_y = (camera_mode == 4) ? (cam_pan_y + 0.5) : (double)cam_pan_y;

    /* REAL FIX 2026-08-04, direct instruction ("all camera stuff needs
     * to be here [arrow_config.txt] including face distance for 1st,
     * tilt/height for 3rd - get it?"): same real config file
     * bv_menu_input.c's own arrow_config.txt already reads, extended
     * with the camera-geometry constants that used to be hardcoded
     * right here in build_camera() - a human can now retune first/
     * third-person framing by editing one text file and relaunching,
     * no C, no rebuild, matching the exact real request that produced
     * arrow_config.txt in the first place. */
    char cam_cfg_path[PATH_BUF];
    snprintf(cam_cfg_path, sizeof(cam_cfg_path), "%s/pieces/system/arrow_config.txt", focused_project_root);
    double fp_face_dist = read_kv_double(cam_cfg_path, "fp_face_distance", 0.3);
    double fp_eye_height = read_kv_double(cam_cfg_path, "fp_eye_height", 1.5);
    double tp_distance = read_kv_double(cam_cfg_path, "tp_distance", 3.0);
    double tp_height = read_kv_double(cam_cfg_path, "tp_height", 4.0);
    double tp_look_down_deg = read_kv_double(cam_cfg_path, "tp_look_down_deg", 20.0);

    Camera cam = build_camera(camera_mode, cam_yaw, cam_pitch, pan_x, pan_y, cam_pan_z, cam_z_level,
                               anchor_x, anchor_z, anchor_h,
                               fp_face_dist, fp_eye_height, tp_distance, tp_height,
                               tp_look_down_deg);

    /* Real, live game clock read - see clear_sky()/compute_sun_light_
     * level()'s own header comments (xyz-ngn-plan.md §1/§2). A host
     * with no real world_01/state.txt yet (civ-txt/tactics-txt, or
     * piececraft-xyz before Confirm & Start) gets game_epoch=0, a real
     * honest fallback (compute_sun_light_level(0) is still a valid,
     * real orbital position, not a crash). */
    /* REAL, NEW 2026-08-04, direct instruction ("allow turned on or
     * off in config file") - same real config file every other camera
     * tunable already lives in. Disabled: flat sky (matches this
     * file's own real pre-lighting-engine default exactly), flat 1.0
     * ground light (no darkening), sun/moon objects hidden below - a
     * real, honest fallback to the OLD behavior, not a half-measure. */
    int lighting_enabled = read_kv_int(cam_cfg_path, "lighting_enabled", 1);

    char world_state_path_sky[PATH_BUF];
    snprintf(world_state_path_sky, sizeof(world_state_path_sky), "%s/pieces/world_01/state.txt", focused_project_root);
    long long game_epoch_sky = read_kv_ll(world_state_path_sky, "game_time_epoch_sec", 0);
    double game_light_level_sky = lighting_enabled ? compute_sun_light_level(game_epoch_sky) : 1.0;
    if (lighting_enabled) clear_sky(game_light_level_sky);
    else { for (int yy = 0; yy < FRAME_H; yy++) for (int xx = 0; xx < FRAME_W; xx++) { fb[yy][xx][0]=135; fb[yy][xx][1]=180; fb[yy][xx][2]=220; fb[yy][xx][3]=255; } }

    CelestialBody sun_body = lighting_enabled ? load_celestial_body(focused_project_root, "sun_01") : (CelestialBody){0,0,0,0};
    CelestialBody moon_body = lighting_enabled ? load_celestial_body(focused_project_root, "moon_01") : (CelestialBody){0,0,0,0};

    /* Legend/reference cube world bounds - exact mutaclysm convention
     * (draw_debug_cube's own header comment): X+/right=red, X-/left=
     * blue, Y+/top=green, Y-/bottom=brown, Z+/back=white, Z-/front=
     * dark. Solid per-face colors, no texture (matches mutaclysm's own
     * debug cube - it's a calibration aid, not real terrain). */
    const double cube_x0 = -1, cube_x1 = 0, cube_y0 = -1, cube_y1 = 0, cube_z0 = -1, cube_z1 = 0;

    /* REAL FIX 2026-08-02, live-caught bug ("V shaped cutoff at the
     * bottom of the map when the camera moves back, whole map
     * disappears if it moves back further"): max_steps below used to
     * be a fixed `board_w + board_h + 8`, sized assuming the camera
     * starts near/inside the board. Once the camera pans away (bird's-
     * eye/free-roam modes), a ray converging toward the board from
     * OUTSIDE it needs roughly (distance-to-board, in cells) EXTRA DDA
     * steps just to reach col/row in [0,board_w)x[0,board_h) before it
     * can ever test a real hit - the fixed budget ran out before
     * distant/grazing-angle rays (concentrated near the bottom-center
     * of the screen when viewed from above at a distance, producing
     * exactly the observed "V" shape) ever got there, so they
     * incorrectly resolved to sky. Compute the camera's own distance
     * from the board's bounding box once per FRAME (cheap, not per
     * pixel) and size the per-pixel step budget to comfortably cover
     * it, regardless of how far the camera is ever panned. */
    /* REAL PERFORMANCE FIX 2026-08-02/03 (see &.widgits/board-viewer/
     * PITFALLS.txt pitfall 1 for the full story - this replaces an
     * earlier, real-but-wasteful attempt): the previous fix set an
     * UNCONDITIONAL max_steps floor of FRAME_W(640) - meaning every
     * pixel that's genuinely sky (a large fraction of ANY frame, even
     * at normal camera distance) walked up to 640 wasted DDA
     * iterations before giving up, tanking performance across the
     * board, not just at extreme camera distance. Live-caught: "since
     * we added greater length of view... it seems to have slowed down
     * render... mutaclysm is way faster."
     *
     * REAL FIX: a cheap, coarse per-pixel rejection test FIRST - one
     * single ray/AABB test against the board's OWN OUTER bounding box
     * (not per-cell) - before ever entering the expensive per-cell DDA
     * loop at all. A ray that can't possibly hit the board's overall
     * extents costs exactly one slab test (the same primitive already
     * used for the legend cube) and moves on - no DDA, no wasted
     * iterations, regardless of camera distance. A ray that DOES pass
     * this coarse test is now known to actually enter the board's own
     * bounding region, so a MODEST, board-scale max_steps
     * (board_w+board_h+32) is genuinely sufficient for the per-cell DDA
     * that follows - the earlier distance/height-scaled inflation is
     * no longer needed at all, since the coarse test already handles
     * "ray never reaches the board" cheaply and correctly, and once a
     * ray IS confirmed to reach the board's bounding region, the
     * number of individual CELLS it can possibly cross is bounded by
     * the board's own small size, not by how far away the camera
     * started. */
    /* REAL FIX 2026-08-03: -0.4..2.0 was sized for the OLD single-slice
     * terrain_height() range (plains 0.0 .. capital 1.2). A real voxel
     * grid spans 0..z_count (up to 32 for piececraft-xyz's own real
     * chunks) - the coarse rejection test and per-pixel step budget
     * both need to cover the ACTUAL loaded grid's own real height, not
     * the old hardcoded terrain-height range, or rays aimed at upper/
     * lower voxel layers would be coarsely (and wrongly) rejected
     * before the real per-cell DDA ever ran. */
    const double board_bbox_y0 = -0.4, board_bbox_y1 = (double)z_count + 0.5;
    int frame_max_steps = board_w + board_h + z_count + 32;

    /* Real texture-cache warm-up, added 2026-08-03 for the OpenMP
     * parallel per-pixel loop just below (real perf fix - see
     * mc-speed-algos.md for the full writeup). get_voxel8_cached()
     * mutates the shared g_voxel8_cache[]/g_voxel8_cache_count global
     * state on a CACHE MISS (real fopen+parse+insert) - safe from a
     * single thread, a genuine data race if two threads both miss the
     * SAME uncached path at once once the loop below runs in parallel.
     * Real fix: populate every texture this frame could possibly need
     * HERE, single-threaded, before the parallel region starts - by
     * the time any worker thread calls get_voxel8_cached(), every real
     * path is already a cache HIT (pure read, genuinely safe to share
     * across threads with no lock). Small, fixed set of real paths -
     * one call per DISTINCT terrain glyph asset + one per DISTINCT
     * entity asset, cheap next to the per-pixel work itself. */
    for (int i = 0; i < g_terrain_legend_count; i++) {
        if (!g_terrain_legend[i].asset_hex[0]) continue;
        char warm_path[PATH_BUF];
        snprintf(warm_path, sizeof(warm_path), "%s/pieces/registry/emoji_assets/%s/voxels_16.csv",
                 project_root, g_terrain_legend[i].asset_hex);
        get_voxel8_cached(warm_path);
    }
    for (int i = 0; i < g_entity_count; i++) {
        if (!g_entities[i].hex[0]) continue;
        char warm_path[PATH_BUF];
        snprintf(warm_path, sizeof(warm_path), "%s/pieces/registry/emoji_assets/%s/voxels_16.csv",
                 project_root, g_entities[i].hex);
        get_voxel8_cached(warm_path);
    }

    /* Real per-pixel DDA raymarch (Amanatides & Woo grid traversal),
     * ported from mutaclysm's own raymarch_walls_3d() core loop
     * structure (ops/compose_rgb_frame.c:1219-1416), adapted to this
     * project's own single-scalar-height terrain model: every board
     * cell is tested as ONE AABB (thin slab for flat/water terrain,
     * a real tall box for forest/hills/capital) in the SAME unified
     * walk, rather than mutaclysm's own separate floor-quad-pass +
     * wall-raymarch-pass split (that split exists there because
     * mutaclysm has a real walkable/wall glyph registry; this project
     * doesn't need that distinction). */
    /* Real multi-core parallelism, added 2026-08-03 (direct user report:
     * "the render is super slow... even mutaclysm can render more than
     * this"). OpenCL was considered and REJECTED - this environment has
     * the loader/headers installed but `clinfo` reports ZERO actual
     * platforms/devices, so an OpenCL kernel would compile but fail at
     * runtime with no hardware to run it on. OpenMP, by contrast, is a
     * real, available, CPU-only parallelism path (libgomp confirmed
     * present, 8 real cores available) - each pixel's own ray is fully
     * independent (reads shared read-only state: cam/board3d/col_top/
     * col_bottom/col_solid/terrain legend/entities/xelector - all
     * finished loading before this point; writes only to its own
     * disjoint fb[sy][sx] cell) - genuinely embarrassingly parallel,
     * matching the SAME core insight real GPU rasterizers exploit
     * (many independent per-pixel/per-fragment computations), just
     * applied to the CPU cores actually available here instead of a
     * GPU that isn't. See this file's own get_voxel8_cached() warm-up
     * loop just above for the ONE real thread-safety hazard this
     * required fixing first (a shared mutable texture cache). See
     * mc-speed-algos.md for the full writeup. */
    #pragma omp parallel for schedule(dynamic, 4)
    for (int sy = 0; sy < FRAME_H; sy++) {
        for (int sx = 0; sx < FRAME_W; sx++) {
            double a = (sx - FRAME_W / 2.0) / cam.focal;
            double b = (FRAME_H / 2.0 - sy) / cam.focal;
            Vec3 ray_dir = v3_norm(v3_add(cam.forward, v3_add(v3_scale(cam.right, a), v3_scale(cam.up, b))));

            double ox = cam.eye.x, oy = cam.eye.y, oz = cam.eye.z;
            double dirx = ray_dir.x, diry = ray_dir.y, dirz = ray_dir.z;

            double best_t = 1e18;
            int best_face = -1;
            char best_glyph = 0;
            int is_cube = 0;
            int entity_hit_idx = -1;
            int is_xelector_hit = 0;
            int is_hero_hit = 0;
            int hero_phymoji_voxel_idx = -1;
            int phy_wentity_idx = -1;
            int phy_wentity_voxel_idx = -1;
            unsigned char hero_phy_r = 0, hero_phy_g = 0, hero_phy_b = 0;
            unsigned char went_r = 0, went_g = 0, went_b = 0;
            int is_sun_hit = 0, is_moon_hit = 0;
            int shadow_row = -1, shadow_col = -1, shadow_lvl = -1;

            /* REAL, NEW 2026-08-04 - sun/moon as real visible objects
             * (direct instruction: "is sun a real thing with object
             * and light rays... that's how moon and mars will be
             * traveled too"). Real small solid box at their own real,
             * continuously-updated orbital position (pieces/sun_01,
             * pieces/moon_01 - pc_clock_daemon.c's own real write).
             * Same real unconditional-per-pixel test class as the
             * xelector marker just below - at most two real bodies
             * right now, genuinely cheap. Real light RAYS (actual
             * shadow casting) are explicitly NOT built yet - xyz-ngn-
             * plan.md §5 flags that as later, real, separately-costed
             * work, not silently implied by "an object now exists". */
            if (sun_body.present) {
                double sx0 = sun_body.x - 2.0, sx1 = sun_body.x + 2.0;
                double sy0 = sun_body.y - 2.0, sy1 = sun_body.y + 2.0;
                double sz0 = sun_body.z - 2.0, sz1 = sun_body.z + 2.0;
                double t; int face;
                if (ray_aabb_hit_3d(ox, oy, oz, dirx, diry, dirz, sx0, sx1, sy0, sy1, sz0, sz1, &t, &face)
                    && t < best_t) {
                    best_t = t; best_face = face; is_cube = 0; entity_hit_idx = -1;
                    is_xelector_hit = 0; is_hero_hit = 0; hero_phymoji_voxel_idx = -1;
                    phy_wentity_idx = -1; phy_wentity_voxel_idx = -1;
                    is_sun_hit = 1; is_moon_hit = 0;
                }
            }
            if (moon_body.present) {
                double mx0 = moon_body.x - 1.2, mx1 = moon_body.x + 1.2;
                double my0 = moon_body.y - 1.2, my1 = moon_body.y + 1.2;
                double mz0 = moon_body.z - 1.2, mz1 = moon_body.z + 1.2;
                double t; int face;
                if (ray_aabb_hit_3d(ox, oy, oz, dirx, diry, dirz, mx0, mx1, my0, my1, mz0, mz1, &t, &face)
                    && t < best_t) {
                    best_t = t; best_face = face; is_cube = 0; entity_hit_idx = -1;
                    is_xelector_hit = 0; is_hero_hit = 0; hero_phymoji_voxel_idx = -1;
                    phy_wentity_idx = -1; phy_wentity_voxel_idx = -1;
                    is_sun_hit = 0; is_moon_hit = 1;
                }
            }

            /* Xelector marker - see load_xelector()'s own header
             * comment (2026-08-03 fix). Tested unconditionally, same
             * cost class as the entity-box test just below - at most
             * one real xelector exists per host. A real, slightly-
             * larger-than-entity footprint (0.15-0.85, vs entities'
             * 0.25-0.75) so it reads as visually distinct from any
             * entity standing at the same cell, not a same-size twin. */
            /* REAL FIX 2026-08-04, direct user report ("1st person cam
             * is 'in player' and all i can see is xelector color"):
             * mode 1's own eye position IS the xelector/hero anchor
             * position exactly (build_camera()'s camera_mode==1 branch)
             * - rendering the xelector's own marker box unconditionally
             * meant first-person was always looking at the INSIDE of
             * its own marker, filling the whole view with its flat
             * color. Real fix: skip the xelector's own marker specifically
             * in mode 1 (you ARE it, seeing it from outside makes no
             * sense there) - modes 2/3/4 are real external views and
             * correctly keep showing it. */
            if (g_xelector_present && camera_mode != 1) {
                double xx0 = g_xelector_x + 0.15, xx1 = g_xelector_x + 0.85;
                double xz0 = g_xelector_y + 0.15, xz1 = g_xelector_y + 0.85;
                double xy0 = g_xelector_z + 0.15, xy1 = g_xelector_z + 0.85;
                double t; int face;
                if (ray_aabb_hit_3d(ox, oy, oz, dirx, diry, dirz,
                                     xx0, xx1, xy0, xy1, xz0, xz1, &t, &face)
                    && t < best_t) {
                    best_t = t; best_face = face; is_cube = 0; entity_hit_idx = -1; is_xelector_hit = 1; is_hero_hit = 0; hero_phymoji_voxel_idx = -1; phy_wentity_idx = -1; phy_wentity_voxel_idx = -1; is_sun_hit = 0; is_moon_hit = 0;
                }
            }

            /* Real hero phymoji rendering - see load_phymoji_asset()/
             * test_phymoji_hit()'s own header comments (2026-08-04,
             * phymoji.md). World footprint: 0.9x0.9x0.9 world units
             * (same overall size the OLD flat marker box used, now
             * filled with real generated voxel detail instead of a
             * flat color) - centered on the hero's own (x,y) cell,
             * standing on the ground at hero_z. Falls back to the OLD
             * flat marker box (is_hero_hit path, below) if no real
             * asset has been generated yet - honest, graceful, not a
             * silent invisible failure. */
            /* Same real "you ARE the anchor in mode 1" fix as the
             * xelector marker just above - the hero's own phymoji box
             * is centered on the exact same anchor position mode 1's
             * eye sits at. */
            if (g_hero_present && hero_phymoji_count > 0 && camera_mode != 1) {
                double world_size = 0.9;
                double wx0 = g_hero_x + 0.5 - world_size / 2.0;
                double wy0 = g_hero_z + 0.0;
                double wz0 = g_hero_y + 0.5 - world_size / 2.0;
                /* PERFORMANCE FIX 2026-08-04 (direct user report: real
                 * slowdown after phymoji landed): the real per-voxel
                 * loop below (up to hero_phymoji_count real AABB tests,
                 * e.g. ~96) was running for EVERY pixel of the frame
                 * unconditionally, even pixels nowhere near the hero -
                 * that's the actual regression, not OpenMP/empty-space-
                 * skip breaking. Real fix: one cheap coarse box test
                 * against the hero's own OUTER world bounding box first
                 * (same ray_aabb_hit_3d() primitive, ONE call) - only
                 * pay the real per-voxel cost on pixels that already
                 * hit the coarse box. */
                double coarse_t; int coarse_face;
                if (ray_aabb_hit_3d(ox, oy, oz, dirx, diry, dirz,
                                     wx0, wx0 + world_size, wy0, wy0 + world_size, wz0, wz0 + world_size,
                                     &coarse_t, &coarse_face)
                    && coarse_t < best_t) {
                    double t; int face; unsigned char pr, pg, pb;
                    if (test_phymoji_hit(ox, oy, oz, dirx, diry, dirz, wx0, wy0, wz0,
                                          world_size, world_size, world_size,
                                          hero_phymoji_max_lx, hero_phymoji_max_ly, hero_phymoji_max_lz,
                                          g_hero_phymoji_columns, g_hero_phymoji_column_count, &t, &face, &pr, &pg, &pb)
                        && t < best_t) {
                        best_t = t; best_face = face; is_cube = 0; entity_hit_idx = -1; is_xelector_hit = 0;
                        is_hero_hit = 1; hero_phymoji_voxel_idx = 1; phy_wentity_idx = -1; phy_wentity_voxel_idx = -1;
                        is_sun_hit = 0; is_moon_hit = 0;
                        hero_phy_r = pr; hero_phy_g = pg; hero_phy_b = pb;
                    }
                }
            } else if (g_hero_present && camera_mode != 1) {
                /* Real, honest fallback - see this block's own header
                 * comment. Same flat marker box as before phymoji
                 * existed. */
                double hx0 = g_hero_x + 0.2, hx1 = g_hero_x + 0.8;
                double hz0 = g_hero_y + 0.2, hz1 = g_hero_y + 0.8;
                double hy0 = g_hero_z + 0.0, hy1 = g_hero_z + 0.9;
                double t; int face;
                if (ray_aabb_hit_3d(ox, oy, oz, dirx, diry, dirz,
                                     hx0, hx1, hy0, hy1, hz0, hz1, &t, &face)
                    && t < best_t) {
                    best_t = t; best_face = face; is_cube = 0; entity_hit_idx = -1; is_xelector_hit = 0;
                    is_hero_hit = 1; hero_phymoji_voxel_idx = -1; phy_wentity_idx = -1; phy_wentity_voxel_idx = -1; is_sun_hit = 0; is_moon_hit = 0;
                }
            }

            /* Real world-placed phymoji entities (trees, phymoji.md
             * §4b/§5's own resolved decision - "switch trees to real
             * entities now"). Same real coarse-box-then-per-voxel
             * pattern the hero uses just above (this file's own
             * 2026-08-04 perf fix header comment) - cheap for pixels
             * nowhere near any tree, real per-voxel detail for pixels
             * that are. World footprint: 1x3x1 world units (a tree
             * spans roughly 3 vertical blocks - trunk + canopy - not a
             * single hero-sized cube), standing on the ground at the
             * entity's own real placed z. */
            for (int wi = 0; wi < g_phymoji_world_entity_count; wi++) {
                PhymojiWorldEntity *we = &g_phymoji_world_entities[wi];
                PhymojiTemplate *wt = &g_phymoji_templates[we->template_idx];
                /* REAL FIX 2026-08-04: world footprint now varies by
                 * real entity type instead of a single hardcoded
                 * "1x3x1 tree" box for every world entity - the newly
                 * added chicken (a real small animal, direct
                 * instruction "give the chicken ... just to walk
                 * randomly") would otherwise get squeezed into a tree's
                 * own tall, narrow box. */
                double wsx = 1.0, wsy = 3.0, wsz = 1.0;
                if (strcmp(we->entity_id, "chicken") == 0) { wsx = 0.6; wsy = 0.6; wsz = 0.6; }
                double wx0 = we->x + 0.5 - wsx / 2.0;
                double wy0 = we->z + 0.0;
                double wz0 = we->y + 0.5 - wsz / 2.0;
                double coarse_t; int coarse_face;
                if (ray_aabb_hit_3d(ox, oy, oz, dirx, diry, dirz,
                                     wx0, wx0 + wsx, wy0, wy0 + wsy, wz0, wz0 + wsz,
                                     &coarse_t, &coarse_face)
                    && coarse_t < best_t) {
                    double t; int face; unsigned char pr, pg, pb;
                    if (test_phymoji_hit(ox, oy, oz, dirx, diry, dirz, wx0, wy0, wz0,
                                          wsx, wsy, wsz,
                                          wt->max_lx, wt->max_ly, wt->max_lz,
                                          wt->columns, wt->column_count, &t, &face, &pr, &pg, &pb)
                        && t < best_t) {
                        best_t = t; best_face = face; is_cube = 0; entity_hit_idx = -1; is_xelector_hit = 0;
                        is_hero_hit = 0; hero_phymoji_voxel_idx = -1;
                        phy_wentity_idx = wi; phy_wentity_voxel_idx = 1;
                        is_sun_hit = 0; is_moon_hit = 0;
                        went_r = pr; went_g = pg; went_b = pb;
                    }
                }
            }

            /* Legend cube test - fixed AABB, always tested. */
            {
                double t; int face;
                if (ray_aabb_hit_3d(ox, oy, oz, dirx, diry, dirz,
                                     cube_x0, cube_x1, cube_y0, cube_y1, cube_z0, cube_z1, &t, &face)
                    && t < best_t) {
                    best_t = t; best_face = face; is_cube = 1; is_xelector_hit = 0; is_hero_hit = 0; hero_phymoji_voxel_idx = -1; phy_wentity_idx = -1; phy_wentity_voxel_idx = -1; is_sun_hit = 0; is_moon_hit = 0;
                }
            }

            /* Entity tests - part 1, untextured solid-color boxes (see
             * load_entities()'s own header comment). Small box centered
             * within its own cell (0.25-0.75 footprint, full height 0-1)
             * so it reads as a distinct object standing on the terrain,
             * not just another full-cell block. Cheap - at most a
             * handful of entities exist (6 units today), a fixed-size
             * loop here costs far less than the per-cell DDA itself. */
            for (int ei = 0; ei < g_entity_count; ei++) {
                double ex0 = g_entities[ei].pos_x + 0.25, ex1 = g_entities[ei].pos_x + 0.75;
                double ez0 = g_entities[ei].pos_y + 0.25, ez1 = g_entities[ei].pos_y + 0.75;
                double t; int face;
                if (ray_aabb_hit_3d(ox, oy, oz, dirx, diry, dirz,
                                     ex0, ex1, 0.0, 1.0, ez0, ez1, &t, &face)
                    && t < best_t) {
                    best_t = t; best_face = face; is_cube = 0; entity_hit_idx = ei; is_xelector_hit = 0; is_hero_hit = 0; hero_phymoji_voxel_idx = -1; phy_wentity_idx = -1; phy_wentity_voxel_idx = -1; is_sun_hit = 0; is_moon_hit = 0;
                }
            }

            /* Coarse rejection FIRST - one cheap slab test against the
             * board's own OUTER bounding box. If a ray can't possibly
             * hit the board at all, skip the whole per-cell DDA below
             * entirely (matches the same primitive/cost as the legend-
             * cube test above - one ray_aabb_hit_3d call, not a loop).
             * This is what actually makes far-away/shallow-angle rays
             * cheap again, regardless of camera distance - see this
             * file's own header note + PITFALLS.txt pitfall 1. */
            int board_bbox_hit = 0;
            {
                double t; int face;
                if (ray_aabb_hit_3d(ox, oy, oz, dirx, diry, dirz,
                                     0.0, (double)board_w, board_bbox_y0, board_bbox_y1, 0.0, (double)board_h,
                                     &t, &face)) {
                    board_bbox_hit = 1;
                }
            }

            /* Board cells - REAL 2-AXIS grid DDA over (col,row), same
             * real Amanatides-Woo structure/cost as the ORIGINAL
             * single-slice version, restored 2026-08-03 as a real perf
             * fix (direct user report: "the render is super slow
             * compared to civ/mutaclysm" - see mc-speed-algos.md for
             * the full writeup). The naive 3-axis (col,row,lvl) walk
             * this replaced was CORRECT but slow: every column got
             * stepped through level-by-level even across large uniform
             * runs of air or stone. Real fix: col_top/col_bottom/
             * col_solid (precomputed once per frame, above) already
             * know each column's real solid extent - a VERIFIED
             * gap-free column (col_solid==1, true for every column in
             * today's world-gen) tests as ONE merged box, no inner
             * loop at all. A column that genuinely has a gap
             * (col_solid==0 - impossible today, real once Phase 2
             * mining exists) falls back to a true per-voxel walk, but
             * ONLY across that specific column's own real solid range,
             * not the whole grid - correct and fast in both cases, and
             * this can never render a real gap as solid, per the
             * col_solid check's own purpose. `lvl` is set to the REAL
             * entered Z on a hit (needed for correct texture UV/
             * selector-highlight below) - either the exact voxel hit
             * in the fallback path, or the ray's own real world-Y at
             * the merged-box hit point in the fast path (clamped into
             * the column's real solid range). */
            int col = (int)floor(ox), row = (int)floor(oz);
            int lvl = 0;
            if (board_bbox_hit) {
            int step_col = (dirx > 1e-12) ? 1 : (dirx < -1e-12 ? -1 : 0);
            int step_row = (dirz > 1e-12) ? 1 : (dirz < -1e-12 ? -1 : 0);
            double t_delta_x = (fabs(dirx) > 1e-12) ? fabs(1.0 / dirx) : 1e18;
            double t_delta_z = (fabs(dirz) > 1e-12) ? fabs(1.0 / dirz) : 1e18;
            double t_max_x = (step_col > 0) ? ((col + 1) - ox) / dirx : (step_col < 0 ? (col - ox) / dirx : 1e18);
            double t_max_z = (step_row > 0) ? ((row + 1) - oz) / dirz : (step_row < 0 ? (row - oz) / dirz : 1e18);

            int max_steps = frame_max_steps;
            for (int steps = 0; steps < max_steps; steps++) {
                if (col >= 0 && col < board_w && row >= 0 && row < board_h && col_top[row][col] >= col_bottom[row][col]) {
                    if (col_solid[row][col]) {
                        /* FAST PATH: one merged-box test, O(1). */
                        double t; int face;
                        if (ray_aabb_hit_3d(ox, oy, oz, dirx, diry, dirz,
                                             (double)col, (double)col + 1.0,
                                             (double)col_bottom[row][col], (double)col_top[row][col] + 1.0,
                                             (double)row, (double)row + 1.0,
                                             &t, &face) && t < best_t) {
                            double hit_y = oy + t * diry;
                            int entered_lvl = (int)floor(hit_y);
                            if (entered_lvl < col_bottom[row][col]) entered_lvl = col_bottom[row][col];
                            if (entered_lvl > col_top[row][col]) entered_lvl = col_top[row][col];
                            char g = board3d[entered_lvl][row][col];
                            best_t = t; best_face = face; best_glyph = g; is_cube = 0; entity_hit_idx = -1; is_xelector_hit = 0; is_hero_hit = 0; hero_phymoji_voxel_idx = -1; phy_wentity_idx = -1; phy_wentity_voxel_idx = -1; is_sun_hit = 0; is_moon_hit = 0; shadow_row = row; shadow_col = col;
                            lvl = entered_lvl;
                        }
                    } else {
                        /* SLOW FALLBACK: real per-voxel walk, but only
                         * across THIS column's own real solid range -
                         * never the full 0..z_count grid. */
                        for (int test_lvl = col_bottom[row][col]; test_lvl <= col_top[row][col]; test_lvl++) {
                            char g = board3d[test_lvl][row][col];
                            if (voxel_is_air(g)) continue;
                            double t; int face;
                            if (ray_aabb_hit_3d(ox, oy, oz, dirx, diry, dirz,
                                                 (double)col, (double)col + 1.0,
                                                 (double)test_lvl, (double)test_lvl + 1.0,
                                                 (double)row, (double)row + 1.0,
                                                 &t, &face) && t < best_t) {
                                best_t = t; best_face = face; best_glyph = g; is_cube = 0; entity_hit_idx = -1; is_xelector_hit = 0; is_hero_hit = 0; hero_phymoji_voxel_idx = -1; phy_wentity_idx = -1; phy_wentity_voxel_idx = -1; is_sun_hit = 0; is_moon_hit = 0; shadow_row = row; shadow_col = col;
                                lvl = test_lvl;
                            }
                        }
                    }
                }
                /* Once we have ANY hit, no cell further along the ray
                 * (beyond this DDA step) can be closer - safe to stop
                 * once the DDA's own next step distance exceeds it. */
                double next_t = (t_max_x < t_max_z) ? t_max_x : t_max_z;
                if (best_t < 1e17 && next_t > best_t) break;
                if (t_max_x < t_max_z) { col += step_col; t_max_x += t_delta_x; }
                else { row += step_row; t_max_z += t_delta_z; }
                /* No bounds-check-triggered early break - see the real
                 * root-cause writeup this file used to carry here
                 * (&.widgits/board-viewer/PITFALLS.txt "raymarch
                 * disappearing at distance"): a ray approaching the
                 * board from far outside it is legitimately out of
                 * bounds for its first several steps - max_steps alone
                 * (distance-aware, set above) is what safely bounds
                 * this loop, not a margin check. */
            }
            } /* end if (board_bbox_hit) */

            if (best_t >= 1e17) continue; /* sky, already cleared */

            unsigned char r, g, bl;
            if (is_sun_hit) {
                /* Real, bright, self-lit disc - a real light SOURCE
                 * shouldn't be darkened by the very light level it's
                 * causing, so this deliberately skips the real ground-
                 * light multiplier applied at the very end of this
                 * loop (that multiplier only darkens, never brightens,
                 * so a value already at 255 stays visually full either
                 * way - noted here for real clarity, not because it's
                 * currently a visible bug). */
                r = 255; g = 220; bl = 120;
            } else if (is_moon_hit) {
                r = 210; g = 210; bl = 225; /* real, cooler/dimmer pale tone */
            } else if (phy_wentity_idx >= 0 && phy_wentity_voxel_idx >= 0) {
                /* Real world-placed phymoji entity color (trees) - same
                 * real per-column baked color + face-darken pattern the
                 * hero block just below uses (2026-08-04 column-index
                 * perf fix - see build_phymoji_columns()'s own header
                 * comment - color now comes straight from
                 * test_phymoji_hit()'s own real column lookup, not a
                 * flat-array voxel index). */
                r = went_r; g = went_g; bl = went_b;
                if (best_face != 3) { r = (unsigned char)(r * 3 / 4); g = (unsigned char)(g * 3 / 4); bl = (unsigned char)(bl * 3 / 4); }
            } else if (is_hero_hit && hero_phymoji_voxel_idx >= 0) {
                /* Real phymoji voxel color (2026-08-04, phymoji.md) -
                 * the ACTUAL generated, depth-attenuated color for the
                 * specific voxel this ray hit, not a flat placeholder.
                 * A face-based darken still applies (matches every
                 * other solid entity in this file, e.g. the legend
                 * cube/generic entities below) since the generator
                 * bakes FRONT-facing depth attenuation but real per-
                 * face lighting (which side is lit) is still a render-
                 * time concern, same as everywhere else in this file. */
                r = hero_phy_r; g = hero_phy_g; bl = hero_phy_b;
                if (best_face != 3) { r = (unsigned char)(r * 3 / 4); g = (unsigned char)(g * 3 / 4); bl = (unsigned char)(bl * 3 / 4); }
            } else if (is_hero_hit) {
                /* Real, honest fallback (no generated asset yet) - the
                 * OLD flat placeholder color, see load_hero()'s own
                 * header comment for the original "i still dont see
                 * player-avatar" fix this traces back to. */
                if (best_face == 3) { r = 255; g = 220; bl = 177; }  /* top: light skin tone */
                else                { r = 200; g = 90;  bl = 60;  } /* sides/bottom: warm shirt-red */
            } else if (is_xelector_hit) {
                /* Bright, unmistakable solid cyan/yellow marker - not
                 * meant to look like real terrain/entity texture, a
                 * pure UI cursor indicator. Slightly darker on non-top
                 * faces so it still reads as a real 3D box, not a flat
                 * decal. */
                if (best_face == 3) { r = 255; g = 255; bl = 60; }   /* top: bright yellow */
                else                { r = 60;  g = 220; bl = 220; } /* sides/bottom: cyan */
            } else if (is_cube) {
                switch (best_face) {
                    case 1: r=220; g=40;  bl=40;  break; /* X+ red */
                    case 0: r=40;  g=60;  bl=220; break; /* X- blue */
                    case 3: r=40;  g=200; bl=60;  break; /* Y+ green */
                    case 2: r=120; g=80;  bl=40;  break; /* Y- brown */
                    case 5: r=235; g=235; bl=235; break; /* Z+ white */
                    default: r=25; g=25;  bl=25;  break; /* Z- dark */
                }
            } else if (entity_hit_idx >= 0) {
                /* REAL fix 2026-08-03, direct user correction ("emojis
                 * for non flat entities are supposed to render only the
                 * front and back face, clearly and stretch... tops and
                 * sides are the side colors of the emoji... a colored
                 * number signifies the team"). Face 4/5 (Z faces, see
                 * ray_aabb_hit_3d's own face numbering) are treated as
                 * front/back - the cropped, stretched emoji texture
                 * (sample_voxel8_cropped, no blank transparent margin).
                 * Every other face (0/1 sides, 2 bottom, 3 top) uses a
                 * real color sampled from the emoji's OWN edge/border
                 * pixels (get_edge_color), not an arbitrary tint - the
                 * side_r/g/b fields in entities.txt are now ONLY used
                 * as a fallback if the texture can't load at all. Top
                 * face additionally blends in a real team-number digit
                 * (real ASCII bitmap glyph, see digit_mask()). */
                SimpleEntity *se = &g_entities[entity_hit_idx];
                r = se->r; g = se->g; bl = se->b; /* fallback only */
                char asset_path[PATH_BUF];
                snprintf(asset_path, sizeof(asset_path), "%s/pieces/registry/emoji_assets/%s/voxels_16.csv",
                         project_root, se->hex[0] ? se->hex : "");
                double ex0 = se->pos_x + 0.25, ex1 = se->pos_x + 0.75;
                double ez0 = se->pos_y + 0.25, ez1 = se->pos_y + 0.75;
                double wx = ox + best_t * dirx, wy = oy + best_t * diry, wz = oz + best_t * dirz;
                double u, v;
                box_face_uv(wx, wy, wz, ex0, ex1, 0.0, 1.0, ez0, ez1, best_face, &u, &v);
                if ((best_face == 4 || best_face == 5) && se->hex[0]) {
                    unsigned char sampled[3];
                    if (sample_voxel8_cropped(asset_path, u, v, sampled)) {
                        r = sampled[0]; g = sampled[1]; bl = sampled[2];
                    }
                } else if (se->hex[0]) {
                    unsigned char edge[3];
                    if (get_edge_color(asset_path, edge)) { r = edge[0]; g = edge[1]; bl = edge[2]; }
                    if (best_face == 3 && digit_mask(se->owner_side, u, v)) {
                        /* Blend a real team color into the digit's own
                         * "on" pixels, base edge color shows through
                         * everywhere else on the top face. */
                        r = (se->owner_side == 2) ? 230 : 60;
                        g = (se->owner_side == 2) ? 60  : 90;
                        bl = (se->owner_side == 2) ? 50  : 230;
                    }
                    if (best_face != 3) { r = (unsigned char)(r * 3 / 4); g = (unsigned char)(g * 3 / 4); bl = (unsigned char)(bl * 3 / 4); }
                }
            } else {
                terrain_color(best_glyph, &r, &g, &bl);
                /* REAL FIX 2026-08-03: selector highlight now also
                 * checks lvl == current_z - with real multi-layer
                 * voxels, a (col,row) match alone is no longer
                 * sufficient (the SAME column exists on every layer);
                 * only the voxel on the selector's own actual layer
                 * should highlight, or every layer's column would light
                 * up identically regardless of which one the selector
                 * is really on. */
                if (col == selector_x && row == selector_y && lvl == current_z) {
                    r = (unsigned char)((r + 255) / 2);
                    g = (unsigned char)((g + 255) / 2);
                    bl = (unsigned char)(bl / 2);
                }
                const char *hex = terrain_asset_hex(best_glyph);
                if (hex) {
                    /* REAL FIX 2026-08-03: y0/y1 used to come from
                     * terrain_height() (the OLD per-glyph extrusion
                     * hint) - every real voxel is now a uniform 1-unit
                     * cube, so its own texture UV bounds are simply
                     * [lvl, lvl+1), matching the actual AABB the DDA
                     * above just tested against. */
                    double wx = ox + best_t * dirx, wy = oy + best_t * diry, wz = oz + best_t * dirz;
                    double u, v;
                    box_face_uv(wx, wy, wz, (double)col, (double)col + 1.0, (double)lvl, (double)lvl + 1.0, (double)row, (double)row + 1.0,
                                best_face, &u, &v);
                    char asset_path[PATH_BUF];
                    snprintf(asset_path, sizeof(asset_path), "%s/pieces/registry/emoji_assets/%s/voxels_16.csv",
                             project_root, hex);
                    /* REAL fix 2026-08-03: was sample_voxel8_pixel
                     * (raw, uncropped UV) - blank-transparent-margin
                     * problem, visible on mountains ('^' -> hills
                     * emoji) since terrain textures every face
                     * including the top with the full glyph asset.
                     * Cropped sampler fills the whole face with real
                     * opaque art, no margin. (Reverted the top-face
                     * edge-color split - not what was wanted here.) */
                    unsigned char sampled[3];
                    if (sample_voxel8_cropped(asset_path, u, v, sampled)) {
                        r = sampled[0]; g = sampled[1]; bl = sampled[2];
                    }
                }
                if (best_face != 3) { r = (unsigned char)(r * 3 / 4); g = (unsigned char)(g * 3 / 4); bl = (unsigned char)(bl * 3 / 4); }
            }
            /* xyz-ngn-plan.md §4 (Step 3 - real directional lighting):
             * same real sun_light_level clear_sky() already used for
             * the sky color, now applied to every real solid pixel too
             * - a real ambient floor (0.15) keeps night genuinely
             * visible instead of pure black, matching the plan doc's
             * own real formula exactly. Applied ONCE here, at the
             * single real point every per-pixel color path in this
             * loop already converges to, rather than threading it
             * through every individual branch above. */
            double ambient_floor = 0.15;
            double light_level = game_light_level_sky;
            if (light_level < ambient_floor) light_level = ambient_floor;

            /* xyz-ngn-plan.md §5 (Step 4 - real shadows, Unreal's own
             * real trick adapted): a real 2D column walk toward the
             * sun's own real world position, reusing col_top - the
             * SAME per-column height data already computed above for
             * the empty-space-skip optimization - as a de facto real
             * shadow map, exactly like the plan doc's own writeup.
             * Terrain-only (best_glyph != 0, shadow_row/col only ever
             * get set by a real terrain hit above) - phymoji entities
             * don't cast shadows yet, a real, stated limitation, not
             * an oversight. */
            if (lighting_enabled && sun_body.present && best_glyph != 0 && shadow_row >= 0) {
                double sun_dx = sun_body.x - (shadow_col + 0.5);
                double sun_dz = sun_body.z - (shadow_row + 0.5);
                double sun_dist_xz = sqrt(sun_dx * sun_dx + sun_dz * sun_dz);
                if (sun_dist_xz > 1e-6) {
                    sun_dx /= sun_dist_xz; sun_dz /= sun_dist_xz;
                    double step_x = shadow_col + 0.5, step_z = shadow_row + 0.5;
                    double my_height = (double)lvl;
                    int shadowed = 0;
                    int max_shadow_steps = 24; /* real, distance-bounded - matches this file's own real "max_steps" convention elsewhere */
                    for (int s = 1; s <= max_shadow_steps && !shadowed; s++) {
                        step_x += sun_dx; step_z += sun_dz;
                        int sc = (int)floor(step_x), sr = (int)floor(step_z);
                        if (sc < 0 || sc >= board_w || sr < 0 || sr >= board_h) break;
                        double dist_so_far = (double)s;
                        /* Real straight-line height from us to the sun
                         * at this real distance along the way - if the
                         * REAL terrain here is taller than that line,
                         * it blocks the sun from reaching us. */
                        double line_height = my_height + (sun_body.y - my_height) * (dist_so_far / sun_dist_xz);
                        if ((double)col_top[sr][sc] > line_height) shadowed = 1;
                    }
                    if (shadowed) light_level *= 0.4; /* real, partial darkening - not pure black, matches a real soft/ambient-bounced shadow look better than a hard 0 */
                }
            }

            r = (unsigned char)(r * light_level);
            g = (unsigned char)(g * light_level);
            bl = (unsigned char)(bl * light_level);
            fb[sy][sx][0] = r; fb[sy][sx][1] = g; fb[sy][sx][2] = bl; fb[sy][sx][3] = 255;
        }
    }

    /* Writes ONLY the overlay file - never rgb_frame.raw itself (see
     * this file's own header comment + view-vs-muta.md). system/
     * chtpm_rgb_render (mutaclysm's own fork) remains the sole writer
     * of rgb_frame.raw, compositing this overlay in via its own
     * MAP3D_MARKER handling every time it redraws for any reason. */
    char overlay_path[PATH_BUF], overlay_receipt_path[PATH_BUF];
    snprintf(overlay_path, sizeof(overlay_path), "%s/pieces/display/rgb_frame_3d_overlay.raw", project_root);
    snprintf(overlay_receipt_path, sizeof(overlay_receipt_path), "%s/pieces/display/rgb_frame_3d_overlay.receipt.txt", project_root);

    size_t byte_count = (size_t)FRAME_W * FRAME_H * 4;
    write_file_atomic(overlay_path, fb, byte_count);
    write_overlay_receipt(overlay_receipt_path, FRAME_W, FRAME_H);

    return 0;
}
