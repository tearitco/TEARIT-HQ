/* pet_manager - the live loop behind the pet house window (a <module> child of the khtpm window).
 *
 * Every ~125 ms: read the window's position from #.desktop/livedesk_hq_windows_<parent pid>.txt (x=, y=, w=, h=, written by the renderer for sidebar+panel windows),
 * run pet_physics (gravity, inertia from the window being moved), pick the expression (falling/landing/walking override the mood), and draw the room + pet with pet_scene
 * into state/scene.raw (the window's <canvas sprite=...>). About once a second it refreshes ui.txt (pet_event.sh status); every `tick_s` seconds (default 30) it runs
 * the day tick (needs, self care, evolution). It runs until the window closes (the parent pid changes) or it is killed.
 * Every path is derived from argv[0]: <house>/@.apps/pet-house/ops/+x/pet_manager.+x. Env: PET_DIR (state dir), PET_TICK_S, PET_SCENE_W/H.
 * Build: gcc -std=c11 -O2 -Wall -Wextra -D_DEFAULT_SOURCE -o ops/+x/pet_manager.+x ops/pet_manager.c */
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static long long now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (long long)t.tv_sec * 1000 + t.tv_nsec / 1000000; }
static int sh(const char *cmd, char *out, size_t n) {
    FILE *p = popen(cmd, "r"); if (!p) return -1;
    size_t k = 0; if (out) { out[0] = 0; int c; while ((c = fgetc(p)) != EOF && k + 1 < n) out[k++] = (char)c; out[k] = 0; } else { while (fgetc(p) != EOF) {} }
    return pclose(p);
}
static int field(const char *line, const char *key) { char pat[32]; snprintf(pat, sizeof pat, "|%s=", key); const char *p = strstr(line, pat); return p ? atoi(p + strlen(pat)) : 0; }
static void kvs(const char *text, const char *key, char *out, size_t n) {
    out[0] = 0; char pat[64]; snprintf(pat, sizeof pat, "%s=", key); const char *p = text;
    while ((p = strstr(p, pat))) { if (p == text || p[-1] == '\n' || p[-1] == ' ') { p += strlen(pat); size_t k = 0; while (p[k] && p[k] != '\n' && p[k] != ' ' && k + 1 < n) { out[k] = p[k]; k++; } out[k] = 0; return; } p++; }
}

int main(int argc, char **argv) {
    (void)argc;
    char self[PATH_MAX]; if (!realpath(argv[0], self)) return 1;
    char app[PATH_MAX], house[PATH_MAX];
    snprintf(app, sizeof app, "%s", self); for (int i = 0; i < 3; i++) { char *s = strrchr(app, '/'); if (s) *s = 0; }      /* .../pet-house */
    snprintf(house, sizeof house, "%s", app); for (int i = 0; i < 2; i++) { char *s = strrchr(house, '/'); if (s) *s = 0; }
    const char *pd = getenv("PET_DIR"); char pet[PATH_MAX]; if (pd && pd[0]) snprintf(pet, sizeof pet, "%s", pd); else snprintf(pet, sizeof pet, "%s/state", app);
    int W = getenv("PET_SCENE_W") ? atoi(getenv("PET_SCENE_W")) : 360, H = getenv("PET_SCENE_H") ? atoi(getenv("PET_SCENE_H")) : 280;
    int tick_s = getenv("PET_TICK_S") ? atoi(getenv("PET_TICK_S")) : 30; if (tick_s < 1) tick_s = 30;
    pid_t parent = getppid();
    mkdir(pet, 0755);
    char cmd[4096], buf[4096];
    snprintf(cmd, sizeof cmd, "PET_DIR='%s' sh '%s/ops/pet_event.sh' status", pet, app); sh(cmd, buf, sizeof buf);       /* creates the pet if needed */
    long long t_last = now_ms(), t_status = 0, t_tick = now_ms();
    char anim_ui[32] = "idle";
    while (getppid() == parent) {
        long long t = now_ms(), dt = t - t_last; t_last = t;
        int wx = 0, wy = 0;
        char rp[PATH_MAX]; snprintf(rp, sizeof rp, "%s/#.desktop/livedesk_hq_windows_%d.txt", house, (int)parent);
        FILE *rf = fopen(rp, "r"); int got = rf != NULL; if (rf) { char l[512]; if (fgets(l, sizeof l, rf)) { char t2[600]; snprintf(t2, sizeof t2, "|%s", l); wx = field(t2, "x"); wy = field(t2, "y"); } fclose(rf); }
        int floor_h = H - 30;
        static int have_pos = 0, last_wx = 0, last_wy = 0;
        if (got) { have_pos = 1; last_wx = wx; last_wy = wy; }
        if (!have_pos) { usleep(100000); continue; }              /* no window position yet: do not let 0,0 -> real position look like a shove */
        wx = last_wx; wy = last_wy;
        snprintf(cmd, sizeof cmd, "'%s/ops/+x/pet_physics.+x' step '%s/physics.st' %d %d %d %d %lld '%s/physics.pdl'", app, pet, wx, wy, W, floor_h, dt, app);
        sh(cmd, buf, sizeof buf);
        char sx[16] = "180", sy[16] = "250", pa[16] = "rest"; kvs(buf, "pet_x", sx, sizeof sx); kvs(buf, "pet_y", sy, sizeof sy); kvs(buf, "pet_anim", pa, sizeof pa);
        if (t - t_status > 1000) {
            snprintf(cmd, sizeof cmd, "PET_DIR='%s' sh '%s/ops/pet_event.sh' status", pet, app); sh(cmd, buf, sizeof buf);
            kvs(buf, "anim", anim_ui, sizeof anim_ui); t_status = t;
        }
        if (t - t_tick > (long long)tick_s * 1000) {
            snprintf(cmd, sizeof cmd, "PET_DIR='%s' sh '%s/ops/pet_event.sh' tick", pet, app); sh(cmd, NULL, 0); t_tick = t;
        }
        const char *an = anim_ui;
        if (!strcmp(pa, "fall") || !strcmp(pa, "thud")) an = "surprised"; else if (!strcmp(pa, "walk")) an = "walk";
        snprintf(cmd, sizeof cmd, "'%s/ops/+x/pet_scene.+x' '%s' '%s/scene.raw' %d %d %s %s %s %lld", app, pet, pet, W, H, sx, sy, an, (t / 125) % 8);
        sh(cmd, NULL, 0);
        usleep(100000);
    }
    return 0;
}
