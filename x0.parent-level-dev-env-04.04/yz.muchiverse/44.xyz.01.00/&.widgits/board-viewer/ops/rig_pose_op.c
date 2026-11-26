/* rig_pose_op <rig.pdl> <anim|@pattern|-> <t_ms> [boxes <direction>]   the harness / AI face of the rig animation evaluator (headers bv_rig_anim.h and bv_rig_anim_draw.h, the SAME files the renderer includes: D140 condition 1).
 * Reads a schema-2 rig file and evaluates it at time t_ms. <anim> = an ANIM name, `@<n>` = the animation the STATE row of verb pattern n names (0 idle, 1/2 walk, 3 jump, 4 mine, 5 place), `-` = the rest pose. No writes anywhere.
 *   default mode prints:  schema=<n> parts=<p> anims=<a> tracks=<k>   then one row per part, file order, 3 decimals:  part <name> pivot x y z aabb x0 y0 z0 x1 y1 z1 alpha <0..255>
 *   `boxes <direction>`   prints the six stage 1 renderer boxes of that facing (2 south 4 west 6 east 8 north):  boxes 6   then   box <i> x0 y0 z0 x1 y1 z1   (head torso arm_r arm_l leg_r leg_l, cell relative, exactly what the renderer draws);
 *                         the pattern for the animation comes from `@<n>` (any other value = the rest pose). boxes 0 when the file is not a six-part rig.
 * An old colour-only rig file (no SCHEMA row, no PART) prints schema=1 parts=0 and exits 0: nothing to pose, nothing wrong (forward compatibility).
 * Exit: 0 ok | 2 usage / unreadable / too big | 3 unknown animation or no STATE for the pattern | 4 bad row (the line number goes to stderr).
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o ops/+x/rig_pose_op.+x ops/rig_pose_op.c -lm */
#include "bv_rig_anim_draw.h"
int main(int argc, char **argv) {
    int boxes = argc == 6 && !strcmp(argv[4], "boxes");
    if (argc != 4 && !boxes) { fprintf(stderr, "usage: rig_pose_op <rig.pdl> <anim|@pattern|-> <t_ms> [boxes <direction>]\n"); return 2; }
    FILE *f = fopen(argv[1], "rb"); if (!f) { fprintf(stderr, "rig_pose_op: cannot read %s\n", argv[1]); return 2; }
    static char buf[65536]; size_t n = fread(buf, 1, sizeof buf - 1, f); int big = !feof(f); fclose(f); if (big) { fprintf(stderr, "rig_pose_op: file over 64 KiB\n"); return 2; } buf[n] = 0;
    static BvaRig r; int bad = bva_parse(buf, &r); if (bad) { fprintf(stderr, "rig_pose_op: bad row at line %d\n", -bad); return 4; }
    int an = -1, pattern = -1; float t = (float)atof(argv[3]);
    if (argv[2][0] == '@') { pattern = atoi(argv[2] + 1); an = bva_anim_for_pattern(&r, pattern); if (an < 0 && r.nparts > 0 && !boxes) { fprintf(stderr, "rig_pose_op: no STATE for pattern %d\n", pattern); return 3; } }
    else if (strcmp(argv[2], "-")) { an = bva_find_anim(&r, argv[2]); if (an < 0) { fprintf(stderr, "rig_pose_op: no animation %s\n", argv[2]); return 3; } }
    if (boxes) { float b[6][6]; int ci[6]; int k = bv_rig2_boxes(&r, atoi(argv[5]), pattern, t, b, ci); printf("boxes %d\n", k);
        for (int i = 0; i < k; i++) printf("box %d %.3f %.3f %.3f %.3f %.3f %.3f\n", i, b[i][0], b[i][1], b[i][2], b[i][3], b[i][4], b[i][5]);
        return 0; }
    static BvaPose po[BVA_MAX_PARTS]; bva_eval(&r, an, t, po);
    printf("schema=%d parts=%d anims=%d tracks=%d\n", r.schema, r.nparts, r.nanims, r.ntracks);
    for (int i = 0; i < r.nparts; i++) printf("part %s pivot %.3f %.3f %.3f aabb %.3f %.3f %.3f %.3f %.3f %.3f alpha %d\n", r.part[i].name, po[i].p[0], po[i].p[1], po[i].p[2], po[i].lo[0], po[i].lo[1], po[i].lo[2], po[i].hi[0], po[i].hi[1], po[i].hi[2], (int)(po[i].alpha + 0.5f));
    return 0;
}
