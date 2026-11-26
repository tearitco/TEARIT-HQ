/* rig_pose_op <rig.pdl> <anim|-> <t_ms>   the harness / AI face of the rig animation evaluator (header bv_rig_anim.h, the SAME file the renderer includes: D140 condition 1).
 * Reads a schema-2 rig file, evaluates every PART at time t_ms of animation <anim> (`-` = the rest pose) and prints, no writes anywhere:
 *   schema=<n> parts=<p> anims=<a> tracks=<k>
 *   part <name> pivot x y z aabb x0 y0 z0 x1 y1 z1 alpha <0..255>     (one row per part, file order, 3 decimals)
 * An old colour-only rig file (no SCHEMA row, no PART) prints schema=1 parts=0 and exits 0: nothing to pose, nothing wrong (forward compatibility).
 * Exit: 0 ok | 2 usage / unreadable / too big | 3 unknown animation | 4 bad row (the line number goes to stderr).
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o ops/+x/rig_pose_op.+x ops/rig_pose_op.c -lm */
#include "bv_rig_anim.h"
int main(int argc, char **argv) {
    if (argc != 4) { fprintf(stderr, "usage: rig_pose_op <rig.pdl> <anim|-> <t_ms>\n"); return 2; }
    FILE *f = fopen(argv[1], "rb"); if (!f) { fprintf(stderr, "rig_pose_op: cannot read %s\n", argv[1]); return 2; }
    static char buf[65536]; size_t n = fread(buf, 1, sizeof buf - 1, f); int big = !feof(f); fclose(f); if (big) { fprintf(stderr, "rig_pose_op: file over 64 KiB\n"); return 2; } buf[n] = 0;
    static BvaRig r; int bad = bva_parse(buf, &r); if (bad) { fprintf(stderr, "rig_pose_op: bad row at line %d\n", -bad); return 4; }
    int an = -1; if (strcmp(argv[2], "-")) { an = bva_find_anim(&r, argv[2]); if (an < 0) { fprintf(stderr, "rig_pose_op: no animation %s\n", argv[2]); return 3; } }
    static BvaPose po[BVA_MAX_PARTS]; bva_eval(&r, an, (float)atof(argv[3]), po);
    printf("schema=%d parts=%d anims=%d tracks=%d\n", r.schema, r.nparts, r.nanims, r.ntracks);
    for (int i = 0; i < r.nparts; i++) printf("part %s pivot %.3f %.3f %.3f aabb %.3f %.3f %.3f %.3f %.3f %.3f alpha %d\n", r.part[i].name, po[i].p[0], po[i].p[1], po[i].p[2], po[i].lo[0], po[i].lo[1], po[i].lo[2], po[i].hi[0], po[i].hi[1], po[i].hi[2], (int)(po[i].alpha + 0.5f));
    return 0;
}
