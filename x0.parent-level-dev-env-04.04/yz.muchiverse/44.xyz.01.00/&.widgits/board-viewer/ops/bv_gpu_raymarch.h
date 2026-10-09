/* bv_gpu_raymarch - Path A (BV-GPU-RENDER-DESIGN.md): the 3D voxel
 * raymarch on the GPU via headless EGL + GLES3, instead of the OpenMP
 * CPU loop in bv_render_3d.c.
 *
 * bv_render_3d.c fills a BvGpuScene from the same locals its CPU loop
 * uses and calls bv_gpu_raymarch(). Return 0 = the RGBA frame is in
 * `out` (w*h*4, top-left origin, same as the CPU buffer). Non-zero =
 * GL/EGL unavailable or failed -> caller runs the CPU loop.
 *
 * Persistence: by default every call builds and tears down the EGL
 * context (fine for the one-shot CPU-replacement mode, but ~0.25s of
 * driver init dominates). bv_gpu_set_persistent(1) keeps the context +
 * program + textures resident across calls - the daemon (bv_render_3d
 * --daemon) does this once, then each frame is ~1-5ms.
 *
 * v1 scope: terrain DDA + flat-colour AABBs for entities / hero /
 * trees / xelector / sun / moon + one ground light level + one sky
 * colour. No phymoji voxel detail, no per-column shadow rays (v3). */
#ifndef BV_GPU_RAYMARCH_H
#define BV_GPU_RAYMARCH_H

#include <stddef.h>

#define BV_GPU_MAX_LEGEND 64
#define BV_GPU_MAX_BOX    128
#define BV_GPU_MAX_MODEL  80     /* charset extrusions share this list with hero/trees; 24 left classic-board people as flat boxes */
#define BV_GPU_MDL_DIM    32     /* max local grid side */
#define BV_GPU_MDL_DEPTH  8      /* phymoji lz is always 0..7 */

typedef struct {
    float min_x, min_y, min_z;   /* world-space AABB */
    float max_x, max_y, max_z;
    float r, g, b;               /* 0..1 flat colour (used when model < 0) */
    int   self_lit;              /* 1 = skip the ground-light multiply (sun/moon) */
    int   wire;                  /* 0 = solid, 1 = edges only (wire_edge wide), 2 = thin edges (wire_thin wide) */
    int   model;                 /* >=0 -> raymarch phymoji model[model] inside the box instead of a flat fill */
} BvGpuBox;

typedef struct {
    int   w, h;                  /* output frame size (always written full) */
    int   lod_step;              /* 1 = full res; >1 = raymarch at w/step x h/step and nearest-upscale (motion frames) */
    float focal;                 /* cam.focal (already fov/height-derived) */

    float eye[3], fwd[3], right[3], up[3];   /* camera, world space */

    /* voxel grid: board3d[lvl][row][col], glyph byte per cell.
     * dims: cols = board_w, rows = board_h, levels = z_count.
     * world extents: X in [0,board_w], Y in [0,z_count], Z in [0,board_h]. */
    int   board_w, board_h, z_count;
    const unsigned char *grid;   /* board_w*board_h*z_count bytes, index (col + row*board_w + lvl*board_w*board_h) */

    /* legend: glyph byte -> colour + solidity. Bytes not listed = air. */
    int   legend_n;
    unsigned char legend_glyph[BV_GPU_MAX_LEGEND];
    float legend_rgb[BV_GPU_MAX_LEGEND][3];   /* 0..1 flat fallback colour */
    /* per-legend terrain texture: a 16x16 RGBA slice (from the glyph's
     * emoji_assets/<hex>/voxels_16.csv) + the opaque bounding box to
     * crop to (0..1). has_tex=0 -> slice is ignored, flat colour used. */
    int   legend_has_tex[BV_GPU_MAX_LEGEND];
    unsigned char legend_tex[BV_GPU_MAX_LEGEND][16 * 16 * 4];
    float legend_bbox[BV_GPU_MAX_LEGEND][4];  /* u0,v0,u1,v1 in 0..1 */

    float light_level;           /* ground light 0..1 (ambient floor already applied) */
    float sky[3];                /* 0..1 */

    /* maker_view (RPG Maker tile terrain): per-cell atlas. When maker != 0 the grid holds solid voxels (legend glyph 1) and each hit
     * voxel is textured from the atlas: level 0 uses the cell's floor slot, levels above use its wall slot. cells = cells_w*cells_h
     * pairs of uint16 {floor_slot, wall_slot} (slot 0 = blank); atlas = RGBA8 atlas_w x atlas_h, atlas_cols tiles per row of tile_px px.
     * maker_id changes whenever the atlas/cell data changes (the GL textures are re-uploaded only then). */
    int   maker, maker_id, atlas_w, atlas_h, atlas_cols, tile_px, cells_w, cells_h;
    const unsigned char  *atlas;
    const unsigned short *cells;
    /* Far-plane sky. Sampled only when a ray misses. para == NULL keeps u_sky.
     * loop tiles across the frame. The camera vectors are not involved. */
    const unsigned char *para;
    int para_w, para_h, para_loop_x, para_loop_y;
    int see_through;             /* view.pdl see_through=1: no sky, no parallax, miss alpha 0 */
    float fog_start, fog_end;    /* distance fog in cells: hits fade to the sky colour between start and end; fog_end <= fog_start = off */
    float wire_edge, wire_thin;  /* edge half-widths in cells for wire 1 / wire 2; 0 = defaults (0.10 / 0.03) */
    int   box_n;
    BvGpuBox box[BV_GPU_MAX_BOX];

    /* phymoji models: dense local occupancy+colour grids. model_dim[m]
     * = {lx_count, ly_count, lz_count}; voxel (x,y,z) at
     * model_vox[m][((z*BV_GPU_MDL_DIM + y)*BV_GPU_MDL_DIM + x)*4],
     * RGBA, a=255 solid. */
    int   model_n;
    int   model_dim[BV_GPU_MAX_MODEL][3];
    unsigned char model_vox[BV_GPU_MAX_MODEL][BV_GPU_MDL_DIM * BV_GPU_MDL_DIM * BV_GPU_MDL_DEPTH * 4];
} BvGpuScene;

/* 0 = success (out filled), non-zero = fall back to CPU. */
int  bv_gpu_raymarch(const BvGpuScene *s, unsigned char *out);

/* Keep the EGL context + GL objects alive across bv_gpu_raymarch()
 * calls (daemon mode). Passing 0 tears them down. */
void bv_gpu_set_persistent(int on);

/* Explicit teardown (called on daemon exit; also implied by
 * set_persistent(0)). Safe to call when nothing is initialised. */
void bv_gpu_shutdown(void);

#endif
