/* dust_orbit - where the planets are on the space desk at a given time (pure: reads system.pdl, prints; nothing is written).
 *   dust_orbit <system_dir> <real_seconds>        one line per body: `name parent x y angle_deg period_s` (x, y in desk cells, y down)
 * A planet's angle = phase + 360 * (real_seconds * time_scale / period), its place = the parent's desk cell + a * cells_per_au * (cos, sin). The PERIOD is asked of the house's physics node (&.widgits/physics-nodes/ops/+x/phys_node_eval.+x, kepler), once per planet: laws are nodes, not private code (BOOK RPG-DUST-DESIGN.md 6.1, SOLAR-SANDBOX-AND-PLANET-PHYSICS-DESIGN.md).
 * Exit: 0 ok, 1 bad file or a period that did not come back, 2 usage. Build: sh ops/build_dust_orbit.sh */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
typedef struct { char name[24], parent[24]; double a, period, phase; double sx, sy; } Body;
static Body B[16]; static int nb; static double cells_per_au = 12, time_scale = 1;
static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; return s; }
static int split(char *s, char **f, int max) { int n = 0; f[n++] = s; for (char *p = s; *p && n < max; p++) if (*p == '|') { *p = 0; f[n++] = trim(p + 1); } for (int i = 0; i < n; i++) f[i] = trim(f[i]); return n; }
static Body *find(const char *n) { for (int i = 0; i < nb; i++) if (!strcmp(B[i].name, n)) return &B[i]; return NULL; }
/* the period in seconds from the physics node: fork + exec + read, no shell, no popen */
static double period_of(const char *dir, const char *body) {
    char exe[PATH_MAX + 80]; snprintf(exe, sizeof exe, "%s/../../../&.widgits/physics-nodes/ops/+x/phys_node_eval.+x", dir);
    int pf[2]; if (pipe(pf)) return -1; pid_t pid = fork(); if (pid < 0) return -1;
    if (!pid) { dup2(pf[1], 1); close(pf[0]); close(pf[1]); { int dn = open("/dev/null", O_WRONLY); if (dn >= 0) dup2(dn, 2); } execl(exe, exe, dir, body, "orbit", (char *)NULL); _exit(127); }
    close(pf[1]); char buf[2048]; size_t n = 0; ssize_t r; while (n < sizeof buf - 1 && (r = read(pf[0], buf + n, sizeof buf - 1 - n)) > 0) n += (size_t)r; buf[n] = 0; close(pf[0]); int st; waitpid(pid, &st, 0);
    for (char *l = strtok(buf, "\n"); l; l = strtok(NULL, "\n")) { char *f[8]; char c[256]; snprintf(c, sizeof c, "%s", l); int k = split(c, f, 8); if (k >= 4 && !strcmp(f[0], "OUT") && !strcmp(f[2], "period")) return atof(f[3]); }
    return -1;
}
int main(int argc, char **argv) {
    if (argc != 3) { fprintf(stderr, "usage: dust_orbit <system_dir> <real_seconds>\n"); return 2; }
    char path[PATH_MAX + 16], real[PATH_MAX]; snprintf(path, sizeof path, "%s/system.pdl", argv[1]); FILE *fp = fopen(path, "r"); if (!fp) { fprintf(stderr, "dust_orbit: cannot read %s\n", path); return 1; }
    if (!realpath(argv[1], real)) return 1;
    double t = atof(argv[2]); char ln[512];
    while (fgets(ln, sizeof ln, fp)) { char *s = trim(ln); if (!*s || *s == '#') continue; char *f[8]; int n = split(s, f, 8);
        if (!strcmp(f[0], "BODY") && n >= 7 && nb < 16) { Body *b = &B[nb++]; memset(b, 0, sizeof *b); snprintf(b->name, sizeof b->name, "%s", f[1]); snprintf(b->parent, sizeof b->parent, "%s", f[2]); b->a = atof(f[5]); }
        else if (!strcmp(f[0], "SPACE") && n >= 3) { if (!strcmp(f[1], "cells_per_au")) cells_per_au = atof(f[2]); else if (!strcmp(f[1], "time_scale")) time_scale = atof(f[2]);
            else if (!strcmp(f[1], "phase") && n >= 4) { Body *b = find(f[2]); if (b) b->phase = atof(f[3]); } else if (n >= 4 && find(f[1])) { Body *b = find(f[1]); b->sx = atof(f[2]); b->sy = atof(f[3]); } } }
    fclose(fp);
    for (int i = 0; i < nb; i++) { Body *b = &B[i]; double x, y, ang = 0, per = 0;
        if (!strcmp(b->parent, "-")) { x = b->sx; y = b->sy; }
        else { Body *p = find(b->parent); if (!p) { fprintf(stderr, "dust_orbit: %s has parent %s which is not a body\n", b->name, b->parent); return 1; }
            per = period_of(real, b->name); if (per <= 0) { fprintf(stderr, "dust_orbit: no period for %s from the physics node\n", b->name); return 1; }
            ang = fmod(b->phase + 360.0 * (t * time_scale / per), 360.0); double r = b->a / 1.495978707e11 * cells_per_au; x = p->sx + r * cos(ang * M_PI / 180); y = p->sy + r * sin(ang * M_PI / 180); }
        b->sx = x; b->sy = y; printf("%s %s %.2f %.2f %.2f %.6g\n", b->name, b->parent, x, y, ang, per); }
    return 0;
}
