/* bv_rig_op boxes <direction> <pattern> <frame> [charset]: prints `boxes N` then the 6 boxes of the box-limb rig (header bv_rig.h), one `box i x0 y0 z0 x1 y1 z1` row each.
 *   charset defaults to rig:asa; a name that does not start with `rig:` gives `boxes 0` (the board stays as it was).
 * bv_rig_op probe <what> <direction> <pattern> <frame>: one measured fact, for the harness.
 *   height (top of head minus feet) | torsomin (lowest y of the torso) | legfwd (right leg centre along the facing) | legsign (neg when the two legs are on opposite sides of the torso, pos when in phase)
 *   armfwd (right arm's rear face minus the torso's front face along the facing; > 0 = the arm is forward of the torso) | armtop (highest y of the arms) | axis (x or z: the axis the two legs are split on: use pattern 0, it is the lateral axis (south x, east z))
 */
#include "bv_rig.h"
static float along(const float *b, int fx, int fz, int hi) {      /* box extent along the facing: front face (hi) or rear face */
    float a = fx ? (fx > 0 ? (hi ? b[3] : b[0]) : (hi ? -b[0] : -b[3])) : (fz > 0 ? (hi ? b[5] : b[2]) : (hi ? -b[2] : -b[5]));
    return a;
}
static float cen(const float *b, int fx, int fz) { return (along(b, fx, fz, 0) + along(b, fx, fz, 1)) / 2.f; }
int main(int argc, char **argv) {
    float o[6][6];
    if (argc >= 5 && !strcmp(argv[1], "boxes")) {
        int n = bv_rig_boxes(bv_rig_is(argc > 5 ? argv[5] : "rig:asa"), atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), o);
        printf("boxes %d\n", n);
        for (int i = 0; i < n; i++) printf("box %d %.4f %.4f %.4f %.4f %.4f %.4f\n", i, o[i][0], o[i][1], o[i][2], o[i][3], o[i][4], o[i][5]);
        return 0;
    }
    if (argc == 6 && !strcmp(argv[1], "probe")) {
        int d = atoi(argv[3]), fx, fz; bv_rig_facing(d, &fx, &fz);
        if (bv_rig_boxes(1, d, atoi(argv[4]), atoi(argv[5]), o) != 6) return 2;
        const char *w = argv[2];
        if (!strcmp(w, "height")) { printf("%.4f\n", o[0][4] - o[4][1]); return 0; }
        if (!strcmp(w, "torsomin")) { printf("%.4f\n", o[1][1]); return 0; }
        if (!strcmp(w, "legfwd")) { printf("%.4f\n", cen(o[4], fx, fz)); return 0; }
        if (!strcmp(w, "legsign")) { printf("%s\n", cen(o[4], fx, fz) * cen(o[5], fx, fz) < 0 ? "neg" : "pos"); return 0; }
        if (!strcmp(w, "armfwd")) { printf("%.4f\n", along(o[2], fx, fz, 0) - along(o[1], fx, fz, 1)); return 0; }
        if (!strcmp(w, "armtop")) { printf("%.4f\n", o[2][4] > o[3][4] ? o[2][4] : o[3][4]); return 0; }
        if (!strcmp(w, "axis")) { float dx = (o[4][0] + o[4][3]) / 2 - (o[5][0] + o[5][3]) / 2, dz = (o[4][2] + o[4][5]) / 2 - (o[5][2] + o[5][5]) / 2; printf("%s\n", (dx < 0 ? -dx : dx) > (dz < 0 ? -dz : dz) ? "x" : "z"); return 0; }
    }
    fprintf(stderr, "usage: bv_rig_op boxes <direction> <pattern> <frame> [charset] | probe <what> <direction> <pattern> <frame>\n");
    return 2;
}
