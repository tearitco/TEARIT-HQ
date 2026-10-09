/* pet_physics - gravity for the pet inside its window. The pet is a point (its feet) in window coordinates; the window itself moves on the screen.
 *
 *   pet_physics step <state_file> <win_x> <win_y> <width> <height> <dt_ms> [physics.pdl]
 *   pet_physics show <state_file>
 *
 * Every step: the pet is "left behind" when the window moves (inertia % of the window's own movement is added to its velocity, opposite to it), gravity
 * adds downward speed, the pet moves, then bounces off the floor and walls (restitution %), floor friction slows it, and below rest_speed it stops.
 * State file (key=value, rewritten each step): x y vx vy last_wx last_wy air bounces. Output (also printed): `pet_x= pet_y= pet_air= pet_anim=` where pet_anim is
 * one of rest | walk | fall | thud, to pick the expression frame (fall = surprised arms up, thud = the landing squash). Coordinates: y grows DOWN, floor = height.
 * Build: gcc -std=c11 -O2 -Wall -Wextra -o ops/+x/pet_physics.+x ops/pet_physics.c -lm */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { double x, y, vx, vy, lwx, lwy; int air, bounces, init; } St;

static double kv(const char *path, const char *key, double def) {
    FILE *f = fopen(path, "r"); if (!f) return def;
    char line[256]; double v = def; size_t kl = strlen(key);
    while (fgets(line, sizeof line, f)) if (!strncmp(line, key, kl) && line[kl] == '=') { v = atof(line + kl + 1); break; }
    fclose(f); return v;
}
static double pdl(const char *path, const char *name, double def) {
    FILE *f = fopen(path, "r"); if (!f) return def;
    char line[256]; double v = def;
    while (fgets(line, sizeof line, f)) {
        char *b1 = strchr(line, '|'); if (!b1) continue; char *b2 = strchr(b1 + 1, '|'); if (!b2) continue;
        char nm[64]; int n = 0; char *p = b1 + 1; while (*p == ' ') p++;
        while (p < b2 && *p != ' ' && n < 63) nm[n++] = *p++; nm[n] = 0;
        if (!strcmp(nm, name)) { v = atof(b2 + 1); break; }
    }
    fclose(f); return v;
}
static void load(const char *p, St *s) {
    memset(s, 0, sizeof *s);
    s->x = kv(p, "x", 0); s->y = kv(p, "y", 0); s->vx = kv(p, "vx", 0); s->vy = kv(p, "vy", 0);
    s->lwx = kv(p, "last_wx", 0); s->lwy = kv(p, "last_wy", 0); s->air = (int)kv(p, "air", 0); s->bounces = (int)kv(p, "bounces", 0); s->init = (int)kv(p, "init", 0);
}
static void save(const char *p, const St *s, const char *anim) {
    FILE *f = fopen(p, "w"); if (!f) return;
    fprintf(f, "x=%.2f\ny=%.2f\nvx=%.2f\nvy=%.2f\nlast_wx=%.0f\nlast_wy=%.0f\nair=%d\nbounces=%d\ninit=1\npet_x=%.0f\npet_y=%.0f\npet_air=%d\npet_anim=%s\n",
            s->x, s->y, s->vx, s->vy, s->lwx, s->lwy, s->air, s->bounces, s->x, s->y, s->air, anim);
    fclose(f);
}

int main(int argc, char **argv) {
    if (argc >= 3 && !strcmp(argv[1], "show")) { FILE *f = fopen(argv[2], "r"); if (!f) return 2; char l[256]; while (fgets(l, sizeof l, f)) fputs(l, stdout); fclose(f); return 0; }
    if (argc < 8 || strcmp(argv[1], "step")) { fprintf(stderr, "usage: pet_physics step <state> <win_x> <win_y> <w> <h> <dt_ms> [physics.pdl] | show <state>\n"); return 2; }
    const char *sf = argv[2]; double wx = atof(argv[3]), wy = atof(argv[4]), W = atof(argv[5]), H = atof(argv[6]), dt = atof(argv[7]) / 1000.0;
    const char *pp = argc > 8 ? argv[8] : "";
    if (dt <= 0 || dt > 0.5) dt = 0.05;
    double g = pdl(pp, "gravity", 2400), rest = pdl(pp, "restitution", 45) / 100.0, inert = pdl(pp, "inertia", 70) / 100.0,
           fr = pdl(pp, "friction", 12) / 100.0, vmax = pdl(pp, "max_speed", 2600), vrest = pdl(pp, "rest_speed", 40);
    St s; load(sf, &s);
    if (!s.init) { s.x = W / 2; s.y = H; s.lwx = wx; s.lwy = wy; s.air = 0; }
    /* window movement this step: the pet lags behind it (pseudo-force). dw is in px; as velocity: dw / dt */
    double dwx = wx - s.lwx, dwy = wy - s.lwy;
    s.lwx = wx; s.lwy = wy;
    s.vx -= inert * dwx / dt; s.vy -= inert * dwy / dt;
    s.vy += g * dt;
    if (s.vx > vmax) s.vx = vmax; if (s.vx < -vmax) s.vx = -vmax; if (s.vy > vmax) s.vy = vmax; if (s.vy < -vmax) s.vy = -vmax;
    s.x += s.vx * dt; s.y += s.vy * dt;
    const char *anim = "rest"; int landed = 0;
    if (s.x < 12) { s.x = 12; s.vx = -s.vx * rest; } if (s.x > W - 12) { s.x = W - 12; s.vx = -s.vx * rest; }
    if (s.y < 40) { s.y = 40; s.vy = -s.vy * rest; }                        /* ceiling: the head height */
    if (s.y >= H) {
        s.y = H;
        if (s.vy > vrest * 4) { s.vy = -s.vy * rest; s.bounces++; landed = 1; } else { s.vy = 0; }
        s.vx *= (1.0 - fr * dt);
        if (fabs(s.vx) < vrest) s.vx = 0;
    }
    int on_floor = (s.y >= H - 0.5 && fabs(s.vy) < vrest);
    if (on_floor) { if (s.air) { landed = 1; } s.air = 0; if (!landed) s.bounces = 0; } else s.air = 1;
    anim = landed ? "thud" : (s.air ? "fall" : (fabs(s.vx) > vrest ? "walk" : "rest"));
    save(sf, &s, anim);
    printf("pet_x=%.0f pet_y=%.0f pet_air=%d pet_anim=%s\n", s.x, s.y, s.air, anim);
    return 0;
}
