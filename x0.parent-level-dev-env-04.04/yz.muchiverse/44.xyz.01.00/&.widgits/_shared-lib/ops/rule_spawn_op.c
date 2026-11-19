/* rule_spawn_op - evaluate the spawn rule (../rule_spawn.c) once over a scene and print the result. The CLI face of the rule (harness, event page / Blueprintz as registry command rule_spawn, FSM / AI via --describe); the game calls the SAME functions in-process.
 *
 * Usage: rule_spawn_op <scene.pdl>      |      rule_spawn_op --describe
 * Scene rows (pipe rows, # comments, integers, Q8 = 1/256 cell):
 *   RULE  | body_w | N      RULE | body_h | N      REQUIRED (1 to 256): the numbers are data
 *   KIND  | hero or walker   (default hero)
 *   CELL  | cx | cy          the cell to stand in (whole cells), REQUIRED
 *   DIRTY | vx | vy | grounded | coyote | jbuf     optional: the body's motion fields BEFORE the spawn (proves they are cleared)
 *   STATE | jp | pos | armed | facing | fire        optional: a hero's per-seat state BEFORE the spawn (default 1 7 1 -1 1, deliberately not the start state)
 *   WALKER | dir | alive      optional: a walker's state BEFORE the spawn (default 1 0)
 * Output (hero): `BODY x=.. y=.. w=.. h=.. vx=.. vy=.. field=.. grounded=.. coyote=.. jbuf=.. HERO jp=.. pos=.. armed=.. facing=.. fire=..`; (walker): `BODY ... WALKER dir=.. alive=..`; then `HASH <fnv1a-64 of that line>`. Exit 0 ok, 2 usage or a bad scene. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../rule_spawn.c"
static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; return s; }
int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--describe")) { puts("rule_spawn_op: IN scene rows RULE body_w/body_h, KIND hero|walker, CELL cx cy, DIRTY vx vy grounded coyote jbuf, STATE jp pos armed facing fire, WALKER dir alive | OUT `BODY x y w h vx vy field grounded coyote jbuf` + `HERO ...` or `WALKER ...` and HASH | RULE keys: body_w body_h | module: _shared-lib/rule_spawn.c"); return 0; }
    if (argc < 2) { fprintf(stderr, "usage: rule_spawn_op <scene.pdl>   (or --describe)\n"); return 2; }
    FILE *f = fopen(argv[1], "r"); if (!f) { fprintf(stderr, "rule_spawn_op: cannot open scene\n"); return 2; }
    SP_Rule r = { -1, -1 }; int hero = 1, cx = -1, cy = -1, have = 0; RP_Walker w; memset(&w, 0, sizeof w); w.dir = 1; w.alive = 0; int jp = 1, pos = 7, armed = 1, facing = -1, fire = 1; char ln[1024];
    while (fgets(ln, sizeof ln, f)) {
        char *p = trim(ln); if (!*p || *p == '#') continue;
        char *c[8]; int k = 0; char *s = p; while (k < 8) { c[k++] = s; char *bar = strchr(s, '|'); if (!bar) break; *bar = 0; s = bar + 1; } for (int i = 0; i < k; i++) c[i] = trim(c[i]);
        if (!strcmp(c[0], "RULE") && k >= 3) { long v = atol(c[2]); if (!strcmp(c[1], "body_w")) r.w = v; else if (!strcmp(c[1], "body_h")) r.h = v; else { fprintf(stderr, "unknown RULE %s\n", c[1]); return 2; } }
        else if (!strcmp(c[0], "KIND") && k >= 2) hero = strcmp(c[1], "walker") != 0;
        else if (!strcmp(c[0], "CELL") && k >= 3) { cx = atoi(c[1]); cy = atoi(c[2]); have = 1; }
        else if (!strcmp(c[0], "DIRTY") && k >= 6) { w.b.vx = atol(c[1]); w.b.vy = atol(c[2]); w.b.grounded = atoi(c[3]); w.b.coyote = atoi(c[4]); w.b.jbuf = atoi(c[5]); }
        else if (!strcmp(c[0], "STATE") && k >= 6) { jp = atoi(c[1]); pos = atoi(c[2]); armed = atoi(c[3]); facing = atoi(c[4]); fire = atoi(c[5]); }
        else if (!strcmp(c[0], "WALKER") && k >= 3) { w.dir = atoi(c[1]); w.alive = atoi(c[2]); }
    }
    fclose(f);
    if (r.w < 1 || r.w > 256 || r.h < 1 || r.h > 256) { fprintf(stderr, "rule_spawn_op: the scene needs RULE body_w and body_h (1 to 256): the numbers are data, there is no default\n"); return 2; }
    if (!have) { fprintf(stderr, "rule_spawn_op: the scene needs a CELL | cx | cy row\n"); return 2; }
    if (hero) sp_hero(&w.b, cx, cy, &r, &jp, &pos, &armed, &facing, &fire); else sp_walker(&w, cx, cy, &r);
    char out[400]; int m = snprintf(out, sizeof out, "BODY x=%ld y=%ld w=%ld h=%ld vx=%ld vy=%ld field=%d grounded=%d coyote=%d jbuf=%d ", w.b.x, w.b.y, w.b.w, w.b.h, w.b.vx, w.b.vy, w.b.field, w.b.grounded, w.b.coyote, w.b.jbuf);
    if (hero) m += snprintf(out + m, sizeof out - m, "HERO jp=%d pos=%d armed=%d facing=%d fire=%d\n", jp, pos, armed, facing, fire); else m += snprintf(out + m, sizeof out - m, "WALKER dir=%d alive=%d\n", w.dir, w.alive);
    fputs(out, stdout); uint64_t h = 1469598103934665603ULL; for (int i = 0; i < m; i++) { h ^= (unsigned char)out[i]; h *= 1099511628211ULL; }
    printf("HASH %016llx\n", (unsigned long long)h); return 0;
}
