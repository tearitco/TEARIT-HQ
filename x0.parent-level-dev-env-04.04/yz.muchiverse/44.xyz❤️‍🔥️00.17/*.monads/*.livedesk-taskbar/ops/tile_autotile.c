/* tile_autotile.c - real, standalone port of RPG Maker MV's actual
 * autotile mechanism, ground-truthed against a real deployed game's
 * `rpg_core.js` (see TILE-SYSTEM-DESIGN.md sec.2 and RPG-CODE-INDEX-
 * REF.md in #.#.✅️.cal-user-sum/1.^V-hq/ for the full citation/derivation
 * - this file is the code-side landing spot for that design work, not a
 * fresh invention).
 *
 * STATUS (2026-08-27): tables + quadrant-compositing draw math are
 * REAL and VERIFIED (see test_tile_autotile() below, run standalone -
 * not yet wired into tp_desktop_window_rgb.c's own render path). The
 * neighbor-bitmask -> quadrant-piece SELECTION function
 * (autotile_pick_quadrant) uses the standard, widely-documented
 * per-corner "3-neighbor-bit -> 1-of-5-pieces" scheme every correct
 * autotile implementation (Tiled, Godot, etc.) uses under the hood -
 * but the SPECIFIC real correspondence between a quadrant's 5 possible
 * states and the exact [x,y] table values a given engine ships was NOT
 * independently re-derived from first principles here (that would risk
 * a subtle, only-visible-on-screen mismatch). Real, honest next step
 * before this is trusted for real gameplay: render a known neighbor
 * configuration, PNG-dump it, and visually compare against a real RPG
 * Maker MV screenshot of the same configuration - do not skip that
 * check just because this file compiles and its own structural test
 * passes.
 *
 * UPDATE (2026-08-27, same day) - the blit-math half of that check is
 * now DONE: rendered several real shape rows (0, 15, 23, 31, 39, 47)
 * against the real `World_A2.png` tileset copied in at
 * `&.widgits/palettes/tilesets/rmmv/` (see RMMV-ASSET-SOURCE-
 * LOCATION.pdl for where it came from) and visually inspected the
 * output: real, clean, correctly-aligned grass/dirt autotile edges,
 * no garbling, no seams - and shape 47 (the table's own last row)
 * rendered as a small ISOLATED dirt patch fully surrounded by grass,
 * exactly matching that row's expected "no real neighbor
 * connections" semantic. This confirms the ported table + blit
 * coordinate math are correct against real pixel data, not just
 * internally self-consistent. STILL NOT DONE: the shapes above were
 * picked by hand for the visual spot-check, not derived from a real
 * 8-neighbor bitmask via `autotile_pick_quadrant()` - the neighbor->
 * shape-index mapping itself remains the one real, flagged gap before
 * this can drive actual gameplay placement. */

#include <stdio.h>
#include <string.h>

typedef struct { int x, y; } QuadCoord;
typedef QuadCoord ShapeRow[4]; /* [0]=top-left [1]=top-right [2]=bottom-left [3]=bottom-right */

/* Real, verbatim port of Tilemap.FLOOR_AUTOTILE_TABLE (rpg_core.js,
 * ~line 5388) - 48 real entries, confirmed by direct byte-count of the
 * real source, not assumed. Used for A1 (water/waterfall)/A2 (ground)/
 * A5 tile categories. */
static const ShapeRow FLOOR_AUTOTILE_TABLE[48] = {
    {{2,4},{1,4},{2,3},{1,3}}, {{2,0},{1,4},{2,3},{1,3}},
    {{2,4},{3,0},{2,3},{1,3}}, {{2,0},{3,0},{2,3},{1,3}},
    {{2,4},{1,4},{2,3},{3,1}}, {{2,0},{1,4},{2,3},{3,1}},
    {{2,4},{3,0},{2,3},{3,1}}, {{2,0},{3,0},{2,3},{3,1}},
    {{2,4},{1,4},{2,1},{1,3}}, {{2,0},{1,4},{2,1},{1,3}},
    {{2,4},{3,0},{2,1},{1,3}}, {{2,0},{3,0},{2,1},{1,3}},
    {{2,4},{1,4},{2,1},{3,1}}, {{2,0},{1,4},{2,1},{3,1}},
    {{2,4},{3,0},{2,1},{3,1}}, {{2,0},{3,0},{2,1},{3,1}},
    {{0,4},{1,4},{0,3},{1,3}}, {{0,4},{3,0},{0,3},{1,3}},
    {{0,4},{1,4},{0,3},{3,1}}, {{0,4},{3,0},{0,3},{3,1}},
    {{2,2},{1,2},{2,3},{1,3}}, {{2,2},{1,2},{2,3},{3,1}},
    {{2,2},{1,2},{2,1},{1,3}}, {{2,2},{1,2},{2,1},{3,1}},
    {{2,4},{3,4},{2,3},{3,3}}, {{2,4},{3,4},{2,1},{3,3}},
    {{2,0},{3,4},{2,3},{3,3}}, {{2,0},{3,4},{2,1},{3,3}},
    {{2,4},{1,4},{2,5},{1,5}}, {{2,0},{1,4},{2,5},{1,5}},
    {{2,4},{3,0},{2,5},{1,5}}, {{2,0},{3,0},{2,5},{1,5}},
    {{0,4},{3,4},{0,3},{3,3}}, {{2,2},{1,2},{2,5},{1,5}},
    {{0,2},{1,2},{0,3},{1,3}}, {{0,2},{1,2},{0,3},{3,1}},
    {{2,2},{3,2},{2,3},{3,3}}, {{2,2},{3,2},{2,1},{3,3}},
    {{2,4},{3,4},{2,5},{3,5}}, {{2,0},{3,4},{2,5},{3,5}},
    {{0,4},{1,4},{0,5},{1,5}}, {{0,4},{3,0},{0,5},{1,5}},
    {{0,2},{3,2},{0,3},{3,3}}, {{0,2},{1,2},{0,5},{1,5}},
    {{0,4},{3,4},{0,5},{3,5}}, {{2,2},{3,2},{2,5},{3,5}},
    {{0,2},{3,2},{0,5},{3,5}}, {{0,0},{1,0},{0,1},{1,1}},
};

/* Real, verbatim port of Tilemap.WALL_AUTOTILE_TABLE (~line 5415) - 16
 * real entries. Used for A3 (roof/wall-top)/A4 (wall-side). */
static const ShapeRow WALL_AUTOTILE_TABLE[16] = {
    {{2,2},{1,2},{2,1},{1,1}}, {{0,2},{1,2},{0,1},{1,1}},
    {{2,0},{1,0},{2,1},{1,1}}, {{0,0},{1,0},{0,1},{1,1}},
    {{2,2},{3,2},{2,1},{3,1}}, {{0,2},{3,2},{0,1},{3,1}},
    {{2,0},{3,0},{2,1},{3,1}}, {{0,0},{3,0},{0,1},{3,1}},
    {{2,2},{1,2},{2,3},{1,3}}, {{0,2},{1,2},{0,3},{1,3}},
    {{2,0},{1,0},{2,3},{1,3}}, {{0,0},{1,0},{0,3},{1,3}},
    {{2,2},{3,2},{2,3},{3,3}}, {{0,2},{3,2},{0,3},{3,3}},
    {{2,0},{3,0},{2,3},{3,3}}, {{0,0},{3,0},{0,3},{3,3}},
};

/* Real, verbatim port of Tilemap.WATERFALL_AUTOTILE_TABLE (~line 5426)
 * - 4 real entries. */
static const ShapeRow WATERFALL_AUTOTILE_TABLE[4] = {
    {{2,0},{1,0},{2,1},{1,1}}, {{0,0},{1,0},{0,1},{1,1}},
    {{2,0},{3,0},{2,1},{3,1}}, {{0,0},{3,0},{0,1},{3,1}},
};

typedef enum { AUTOTILE_FLOOR, AUTOTILE_WALL, AUTOTILE_WATERFALL } AutotileKind;

/* Real, verbatim port of Tilemap.prototype._drawAutotile's core loop
 * (rpg_core.js ~line 5100-5124): given a shape's 4 quadrant source
 * coords + a base atlas offset (bx,by, in TILE units) + the real tile
 * pixel size, compute the 4 real source/dest blit rects a renderer
 * needs to actually draw this tile. w1/h1 = half-tile size, same as
 * the real engine's own w1/h1. Caller does the actual pixel blit (this
 * function has no rendering dependency, matching this session's own
 * "verify the math standalone before wiring into a live binary"
 * discipline). */
typedef struct { int sx, sy, dx_off, dy_off, w, h; } BlitRect;

static int autotile_compute_blits(AutotileKind kind, int shape, int bx, int by,
                                   int tile_px, BlitRect out[4]) {
    const ShapeRow *table; int n;
    switch (kind) {
        case AUTOTILE_FLOOR:     table = FLOOR_AUTOTILE_TABLE;     n = 48; break;
        case AUTOTILE_WALL:      table = WALL_AUTOTILE_TABLE;      n = 16; break;
        case AUTOTILE_WATERFALL: table = WATERFALL_AUTOTILE_TABLE; n = 4;  break;
        default: return 0;
    }
    if (shape < 0 || shape >= n) return 0;
    int w1 = tile_px / 2, h1 = tile_px / 2;
    for (int i = 0; i < 4; i++) {
        int qsx = table[shape][i].x, qsy = table[shape][i].y;
        out[i].sx = (bx * 2 + qsx) * w1;
        out[i].sy = (by * 2 + qsy) * h1;
        out[i].dx_off = (i % 2) * w1;
        out[i].dy_off = (i / 2) * h1;
        out[i].w = w1; out[i].h = h1;
    }
    return 1;
}

/* Real 8-neighbor edge+corner bitmask, per TILE-SYSTEM-DESIGN.md sec.
 * 2.1 - bit order: 0=N 1=E 2=S 3=W (edges), 4=NE 5=SE 6=SW 7=NW
 * (corners). A corner bit is only "effective" if both its adjacent
 * edge bits are also set (see the AND-reduction in the design doc) -
 * that reduction happens HERE, once, so callers never need to redo it. */
#define BIT_N 0
#define BIT_E 1
#define BIT_S 2
#define BIT_W 3
#define BIT_NE 4
#define BIT_SE 5
#define BIT_SW 6
#define BIT_NW 7

static unsigned char autotile_effective_mask(unsigned char raw8) {
    unsigned char edge_n = (raw8 >> BIT_N) & 1, edge_e = (raw8 >> BIT_E) & 1;
    unsigned char edge_s = (raw8 >> BIT_S) & 1, edge_w = (raw8 >> BIT_W) & 1;
    unsigned char eff = (edge_n << BIT_N) | (edge_e << BIT_E) | (edge_s << BIT_S) | (edge_w << BIT_W);
    if (((raw8 >> BIT_NE) & 1) && edge_n && edge_e) eff |= (1 << BIT_NE);
    if (((raw8 >> BIT_SE) & 1) && edge_s && edge_e) eff |= (1 << BIT_SE);
    if (((raw8 >> BIT_SW) & 1) && edge_s && edge_w) eff |= (1 << BIT_SW);
    if (((raw8 >> BIT_NW) & 1) && edge_n && edge_w) eff |= (1 << BIT_NW);
    return eff;
}

/* ⚠️ NOT YET VISUALLY VERIFIED (see file header) - a real, standard
 * per-corner "3 relevant bits -> 1 of 5 real quadrant states" scheme,
 * computed independently for each of the tile's 4 corners from just
 * the 2 adjacent edges + that corner's own effective corner bit. This
 * is the correct SHAPE of the real algorithm every autotile
 * implementation uses; the mapping from a corner's 5 states to this
 * house's own FLOOR_AUTOTILE_TABLE row/quadrant-index still needs a
 * real render+screenshot verification pass before being trusted. */
typedef enum {
    QSTATE_ISOLATED = 0,   /* neither adjacent edge matches */
    QSTATE_EDGE_A,         /* only the first adjacent edge matches */
    QSTATE_EDGE_B,         /* only the second adjacent edge matches */
    QSTATE_INNER_CORNER,   /* both edges match, corner does NOT */
    QSTATE_FLAT            /* both edges match AND corner matches */
} QuadrantState;

static QuadrantState autotile_pick_quadrant(unsigned char eff_mask, int edge_a_bit, int edge_b_bit, int corner_bit) {
    int a = (eff_mask >> edge_a_bit) & 1, b = (eff_mask >> edge_b_bit) & 1, c = (eff_mask >> corner_bit) & 1;
    if (a && b) return c ? QSTATE_FLAT : QSTATE_INNER_CORNER;
    if (a) return QSTATE_EDGE_A;
    if (b) return QSTATE_EDGE_B;
    return QSTATE_ISOLATED;
}

/* ---------------- standalone structural test (no rendering, no X11) ---------------- */
static int g_fail = 0;
#define CHECK(cond, msg) do { if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); g_fail = 1; } } while (0)

int main(void) {
    /* 1. Table sizes are exactly what RPG-CODE-INDEX-REF.md says. */
    CHECK(sizeof(FLOOR_AUTOTILE_TABLE) / sizeof(ShapeRow) == 48, "floor table must have 48 rows");
    CHECK(sizeof(WALL_AUTOTILE_TABLE) / sizeof(ShapeRow) == 16, "wall table must have 16 rows");
    CHECK(sizeof(WATERFALL_AUTOTILE_TABLE) / sizeof(ShapeRow) == 4, "waterfall table must have 4 rows");

    /* 2. Blit-rect math: 4 quadrants must exactly tile the full tile,
     * no gaps/overlaps, for a real tile_px (48, matching real RPG
     * Maker MV assets per RPG-CODE-INDEX-REF.md). */
    BlitRect r[4];
    int ok = autotile_compute_blits(AUTOTILE_FLOOR, 0, 0, 0, 48, r);
    CHECK(ok, "shape 0 (valid) must compute");
    CHECK(r[0].dx_off == 0 && r[0].dy_off == 0, "quadrant 0 = top-left");
    CHECK(r[1].dx_off == 24 && r[1].dy_off == 0, "quadrant 1 = top-right");
    CHECK(r[2].dx_off == 0 && r[2].dy_off == 24, "quadrant 2 = bottom-left");
    CHECK(r[3].dx_off == 24 && r[3].dy_off == 24, "quadrant 3 = bottom-right");
    for (int i = 0; i < 4; i++) CHECK(r[i].w == 24 && r[i].h == 24, "each quadrant is exactly half the tile");

    /* 3. Out-of-range shape index must fail cleanly, not read garbage. */
    ok = autotile_compute_blits(AUTOTILE_FLOOR, 48, 0, 0, 48, r);
    CHECK(!ok, "shape 48 (one past the real 48-row table) must be rejected");
    ok = autotile_compute_blits(AUTOTILE_WATERFALL, 4, 0, 0, 48, r);
    CHECK(!ok, "shape 4 (one past the real 4-row waterfall table) must be rejected");

    /* 4. Effective-mask AND-reduction: an isolated corner (no matching
     * edges) must NOT survive into the effective mask, even if its raw
     * bit was set - this is the real rule from TILE-SYSTEM-DESIGN.md
     * sec 2.1 that makes 48 shapes sufficient instead of needing 256. */
    unsigned char raw = (1 << BIT_NE); /* corner set, but NEITHER adjacent edge set */
    unsigned char eff = autotile_effective_mask(raw);
    CHECK(eff == 0, "an isolated corner bit must be zeroed by the AND-reduction");

    raw = (1 << BIT_N) | (1 << BIT_E) | (1 << BIT_NE); /* both edges + corner, all real */
    eff = autotile_effective_mask(raw);
    CHECK((eff & (1 << BIT_NE)) != 0, "a corner backed by both real adjacent edges must survive");

    /* 5. Per-quadrant state selection: sanity-check the 5 real states
     * are reachable and distinct. */
    CHECK(autotile_pick_quadrant(0, BIT_N, BIT_E, BIT_NE) == QSTATE_ISOLATED, "no edges -> isolated");
    CHECK(autotile_pick_quadrant((1<<BIT_N), BIT_N, BIT_E, BIT_NE) == QSTATE_EDGE_A, "one edge -> edge state");
    CHECK(autotile_pick_quadrant((1<<BIT_N)|(1<<BIT_E), BIT_N, BIT_E, BIT_NE) == QSTATE_INNER_CORNER,
          "both edges, no corner -> inner corner");
    CHECK(autotile_pick_quadrant((1<<BIT_N)|(1<<BIT_E)|(1<<BIT_NE), BIT_N, BIT_E, BIT_NE) == QSTATE_FLAT,
          "both edges + corner -> flat");

    if (g_fail) { fprintf(stderr, "\nSOME CHECKS FAILED\n"); return 1; }
    printf("All structural checks PASS (tables real, blit math real, AND-reduction real).\n");
    printf("REMAINING before trusting this for real gameplay: render a known neighbor\n");
    printf("configuration, PNG-dump it, and visually compare against real RPG Maker MV\n");
    printf("output of the same configuration - see this file's own header comment.\n");
    return 0;
}
