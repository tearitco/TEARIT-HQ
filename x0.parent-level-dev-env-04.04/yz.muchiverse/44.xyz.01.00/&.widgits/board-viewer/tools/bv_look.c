/* bv_look - jump the board engine's camera to any place and angle (owner 2026-10-27: "you should be able to jump to whatever camera angle you need", and a cli_io for xyz coordinate jumping in the top bar).
 * Writes pieces/system/cam_look.txt of the focused project (look_on=1, eye_*, at_* in the engine's WORLD coordinates) and wakes the idle engine. bv_render_3d.c reads it every frame and replaces the mode camera
 * with it (picture, click pick and the see-through cutaway); `off` removes it. One generic op for every board; the rpg-space top bar "debug" dropdown and `board_ctl look` both call it.
 *   bv_look.+x <app dir> <house> "<text>"       (the argument order a khtpm cli_io action gets: package dir, house root, typed value)
 * <app dir>: a folder holding state/ui.txt or board/state/ui.txt with bv_session= (the rpg-space app dir or its board dir).
 * <text> (numbers in GAME order: column row level, like the HUD's `pos 7,7,1`; a cell is looked at by its centre; pitch in degrees, negative = looking down; yaw 180 looks toward lower rows):
 *   off                              back to the mode camera
 *   x y z                            look at that cell from 8 cells away, yaw 180, pitch -35
 *   x y z dist yaw pitch             orbit that cell
 *   eye ex ey ez at ax ay az         exact eye and target
 *   sel [dist yaw pitch]             orbit the SELECTOR (in rpg-space the character you control), defaults 8 180 -35: the engine places it, so no desk offsets to guess
 * Exit 0 done (prints one line), 1 usage / bad text, 2 no live session. Build: sh tools/build_bv_look.sh */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <unistd.h>

static int read_kv(const char *path, const char *key, char *out, size_t n) {
    FILE *f = fopen(path, "r"); if (!f) return 0; char line[PATH_MAX + 64]; size_t kl = strlen(key); int ok = 0;
    while (fgets(line, sizeof line, f)) if (!strncmp(line, key, kl) && line[kl] == '=') { line[strcspn(line, "\r\n")] = 0; snprintf(out, n, "%s", line + kl + 1); ok = 1; }
    fclose(f); return ok; }
static int num(const char *s, double *d) { char *e; *d = strtod(s, &e); return e != s && !*e; }

int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "Usage: bv_look.+x <app dir> <house> \"off | x y z | x y z dist yaw pitch | eye ex ey ez at ax ay az\"\n"); return 1; }
    char ui[PATH_MAX + 32], sess[PATH_MAX], bst[PATH_MAX + 64], proot[PATH_MAX];
    const char *rel[3] = { "%s/state/ui.txt", "%s/board/state/ui.txt", "%s/../state/ui.txt" }; sess[0] = 0;
    for (int i = 0; i < 3 && !sess[0]; i++) { snprintf(ui, sizeof ui, rel[i], argv[1]); read_kv(ui, "bv_session", sess, sizeof sess); }
    if (!sess[0] || access(sess, F_OK)) { fprintf(stderr, "No live engine session (bv_session= in state/ui.txt of %s).\n", argv[1]); return 2; }
    snprintf(bst, sizeof bst, "%s/pieces/system/bv_state.txt", sess);
    if (!read_kv(bst, "focused_project_root", proot, sizeof proot) || !proot[0]) { fprintf(stderr, "The engine has no focused project yet.\n"); return 2; }
    char lp[PATH_MAX + 64]; snprintf(lp, sizeof lp, "%s/pieces/system/cam_look.txt", proot);

    char text[512]; snprintf(text, sizeof text, "%s", argv[3]); char *w[16]; int n = 0; for (char *t = strtok(text, " ,\t\r\n"); t && n < 16; t = strtok(NULL, " ,\t\r\n")) w[n++] = t;
    double ex, ey, ez, ax, ay, az; char say[200];
    if (n == 0) { fprintf(stderr, "Empty. Try: off | 7 7 1 | 7 7 1 10 180 -40 | eye 7 3 12 at 7 7 1\n"); return 1; }
    if (!strcmp(w[0], "sel") && (n == 1 || n == 4)) {
        double d = 8, y = 180, p = -35; if (n == 4 && (!num(w[1], &d) || !num(w[2], &y) || !num(w[3], &p))) { fprintf(stderr, "sel needs numbers: sel dist yaw pitch\n"); return 1; }
        FILE *f = fopen(lp, "w"); if (!f) { fprintf(stderr, "Could not write %s\n", lp); return 2; }
        fprintf(f, "look_on=1\nlook_sel=1\nlook_dist=%.3f\nlook_yaw=%.3f\nlook_pitch=%.3f\n", d, y, p); fclose(f); snprintf(say, sizeof say, "camera orbits the selector: dist %.1f yaw %.0f pitch %.0f", d, y, p);
    } else if (n == 1 && !strcmp(w[0], "off")) { remove(lp); snprintf(say, sizeof say, "camera look off (mode camera back)"); }
    else {
        double v[8]; int k = 0, eyeform = (!strcmp(w[0], "eye") && n == 8 && !strcmp(w[4], "at"));
        if (eyeform) { for (int i = 1; i <= 7; i++) { if (i == 4) continue; if (!num(w[i], &v[k++])) { fprintf(stderr, "'%s' is not a number.\n", w[i]); return 1; } } }
        else { if (n != 3 && n != 6) { fprintf(stderr, "Need 3 numbers (x y z) or 6 (x y z dist yaw pitch), or `eye ex ey ez at ax ay az`, or `off`.\n"); return 1; }
            for (int i = 0; i < n; i++) if (!num(w[i], &v[i])) { fprintf(stderr, "'%s' is not a number.\n", w[i]); return 1; } }
        if (eyeform) { ex = v[0] + 0.5; ez = v[1] + 0.5; ey = v[2] + 0.5; ax = v[3] + 0.5; az = v[4] + 0.5; ay = v[5] + 0.5; }
        else { double dist = n == 6 ? v[3] : 8, yaw = n == 6 ? v[4] : 180, pit = n == 6 ? v[5] : -35; if (dist < 0.5) dist = 0.5;
            ax = v[0] + 0.5; az = v[1] + 0.5; ay = v[2] + 0.5; double y = yaw * M_PI / 180.0, p = pit * M_PI / 180.0;
            ex = ax - dist * cos(p) * sin(y); ey = ay - dist * sin(p); ez = az - dist * cos(p) * cos(y); }
        FILE *f = fopen(lp, "w"); if (!f) { fprintf(stderr, "Could not write %s\n", lp); return 2; }
        fprintf(f, "look_on=1\neye_x=%.3f\neye_y=%.3f\neye_z=%.3f\nat_x=%.3f\nat_y=%.3f\nat_z=%.3f\n", ex, ey, ez, ax, ay, az); fclose(f);
        snprintf(say, sizeof say, "camera at (%.1f %.1f %.1f) looking at (%.1f %.1f %.1f) world", ex, ey, ez, ax, ay, az);
    }
    char rp[PATH_MAX + 64]; snprintf(rp, sizeof rp, "%s/pieces/apps/player_app/interact_relay.txt", sess);   /* 126 = the unbound "render now" key: an idle engine only redraws when woken */
    for (int i = 0; i < 2; i++) { FILE *r = fopen(rp, "a"); if (r) { fputs("126\n", r); fclose(r); } usleep(300000); }
    puts(say); return 0;
}
