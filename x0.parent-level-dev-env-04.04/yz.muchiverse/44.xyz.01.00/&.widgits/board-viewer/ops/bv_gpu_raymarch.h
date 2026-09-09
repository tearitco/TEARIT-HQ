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

typedef struct {
    float min_x, min_y, min_z;   /* world-space AABB */
    float max_x, max_y, max_z;
    float r, g, b;               /* 0..1 flat colour */
    int   self_lit;              /* 1 = skip the ground-light multiply (sun/moon) */
} BvGpuBox;

typedef struct {
    int   w, h;
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
    float legend_rgb[BV_GPU_MAX_LEGEND][3];   /* 0..1 */

    float light_level;           /* ground light 0..1 (ambient floor already applied) */
    float sky[3];                /* 0..1 */

    int   box_n;
    BvGpuBox box[BV_GPU_MAX_BOX];
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
