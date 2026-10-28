/* bv_eye_guard_test - bv_gpu_solid_at and bv_gpu_pull_in (the third-person spring arm), headless and WITHOUT a GL context: they only read the grid. Build:
 *   gcc -O2 -I. -o bv_eye_guard_test bv_eye_guard_test.c bv_gpu_raymarch.c -lm -lEGL -lGLESv2
 * Scene: 8 x 8 grid, 6 levels. Level 0 is floor ('s') everywhere; a tall wall ('w', levels 1..5) fills row 6; a one-level crate ('c') stands at column 2, row 2. The character is at (3.5, 1.5, 3.5).
 * Exit 0 = every check held, 1 = a check failed. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "bv_gpu_raymarch.h"
static int bad = 0;
static void check(int ok, const char *what) { printf("%s: %s\n", ok ? "ok  " : "FAIL", what); if (!ok) bad = 1; }
static BvGpuScene S;
static void scene(void) {
    static unsigned char g[8 * 8 * 6]; memset(&S, 0, sizeof S); memset(g, 0, sizeof g);
    for (int i = 0; i < 64; i++) g[i] = 's';
    for (int l = 1; l < 6; l++) for (int x = 0; x < 8; x++) g[x + 6 * 8 + l * 64] = 'w';
    g[2 + 2 * 8 + 64] = 'c';
    S.grid = g; S.board_w = 8; S.board_h = 8; S.z_count = 6; S.legend_n = 3; S.legend_glyph[0] = 's'; S.legend_glyph[1] = 'w'; S.legend_glyph[2] = 'c';
}
int main(void) {
    scene();
    check(bv_gpu_solid_at(&S, 3.5, 0.5, 3.5) == 1, "solid_at: the floor cell is solid");
    check(bv_gpu_solid_at(&S, 3.5, 1.5, 3.5) == 0, "solid_at: the air above the floor is not");
    check(bv_gpu_solid_at(&S, 4.2, 3.0, 6.9) == 1 && bv_gpu_solid_at(&S, 4.2, 5.99, 6.0) == 1, "solid_at: the wall, anywhere inside its row and levels 1..5");
    check(bv_gpu_solid_at(&S, 4.2, 6.0, 6.5) == 0, "solid_at: above the wall (level 6 is outside the grid) is air");
    check(bv_gpu_solid_at(&S, -0.1, 0.5, 3.0) == 0 && bv_gpu_solid_at(&S, 3.0, 0.5, 8.2) == 0 && bv_gpu_solid_at(&S, 3.0, -0.5, 3.0) == 0, "solid_at: outside the grid (negative column, past the last row, below level 0) is air");
    check(bv_gpu_solid_at(&S, 2.9, 1.1, 2.1) == 1 && bv_gpu_solid_at(&S, 3.1, 1.1, 2.1) == 0, "solid_at: the crate cell and its neighbour (cell boundaries at whole numbers)");
    check(bv_gpu_solid_at(NULL, 1, 1, 1) == 0, "solid_at: no scene is air");
    double tgt[3] = { 3.5, 1.5, 3.5 };
    double e1[3] = { 3.5, 3.0, 1.0 };
    check(fabs(bv_gpu_pull_in(&S, tgt, e1, 40) - 1.0) < 1e-12 && e1[0] == 3.5 && e1[1] == 3.0 && e1[2] == 1.0, "pull_in: an eye already in air is untouched (returns 1.0)");
    double e2[3] = { 3.5, 3.0, 6.5 };      /* inside the wall, behind the character */
    double k2 = bv_gpu_pull_in(&S, tgt, e2, 40);
    check(k2 > 0.0 && k2 < 1.0 && !bv_gpu_solid_at(&S, e2[0], e2[1], e2[2]), "pull_in: an eye inside the wall ends up in air");
    check(e2[2] < 6.0 && e2[2] > tgt[2], "pull_in: and it stopped on the character's side of the wall, not past the character");
    check(fabs(e2[0] - 3.5) < 1e-9, "pull_in: it moved along the line to the target (x unchanged here because the line is in the x = 3.5 plane)");
    double d0 = hypot(6.5 - 3.5, 3.0 - 1.5), d1 = hypot(e2[2] - 3.5, e2[1] - 1.5); check(fabs(k2 - d1 / d0) < 1e-6, "pull_in: the returned fraction equals the distance kept divided by the distance asked for");
    check(e2[2] <= 6.0 - 1e-9 && 6.0 - e2[2] >= 1.0 / 40 * d0 * 0.9, "pull_in: it keeps at least one step of slack from the wall face");
    double e3[3] = { 3.5, 1.5, 3.5 }; double t3[3] = { 3.5, 1.5, 3.5 };
    double e4[3] = { 3.5, 0.5, 3.5 }, t4[3] = { 3.5, 0.5, 3.5 };
    check(bv_gpu_pull_in(&S, t4, e4, 40) == 0.0 && e4[1] == 0.5, "pull_in: eye and target both inside solid: no clear point, returns 0.0, eye untouched");
    check(bv_gpu_pull_in(&S, t3, e3, 40) == 1.0, "pull_in: eye in air at the target returns 1.0");
    double e5[3] = { 3.5, 3.0, 6.5 }; double k5 = bv_gpu_pull_in(&S, tgt, e5, 1);
    check(!bv_gpu_solid_at(&S, e5[0], e5[1], e5[2]) || k5 == 0.0, "pull_in: with one step it still never leaves the eye in solid (steps 1 jumps straight to the target)");
    double e6[3] = { 3.5, 3.0, 6.5 }; double k6 = bv_gpu_pull_in(&S, tgt, e6, 0);
    check(k6 >= 0.0 && k6 <= 1.0, "pull_in: steps below 1 are treated as 1");
    check(bv_gpu_pull_in(NULL, tgt, e1, 40) == 1.0, "pull_in: no scene leaves the eye alone");
    double e7[3] = { 3.5, 1.5, 6.9 }, t7[3] = { 3.5, 1.5, 0.5 }; double k7 = bv_gpu_pull_in(&S, t7, e7, 40);
    check(k7 > 0.0 && e7[2] < 6.0 && !bv_gpu_solid_at(&S, e7[0], e7[1], e7[2]), "pull_in: a level line from behind the wall to the far side stops at the wall's near face");
    return bad;
}
