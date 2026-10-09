/* pet_manager - the live loop behind the pet house window (a <module> child of the khtpm window).
 *
 * Every ~125 ms: read the window's position from #.desktop/livedesk_hq_windows_<parent pid>.txt (x=, y=, w=, h=, written by the renderer for sidebar+panel windows),
 * run pet_physics (gravity, inertia from the window being moved), pick the expression (falling/landing/walking override the mood), and draw the room + pet with pet_scene
 * into state/scene.raw (the window's <canvas sprite=...>). About once a second it refreshes ui.txt (pet_event.sh status); every `tick_s` seconds (default 30) it runs
 * the day tick (needs, self care, evolution). It runs until the window closes (the parent pid changes) or it is killed.
 * Every path is derived from argv[0]: <house>/@.apps/pet-trainer/ops/+x/pet_manager.+x. Env: PET_DIR (state dir), PET_TICK_S, PET_SCENE_W/H.
 * The registry file gives the window id (win=0x...); its LIVE root position comes from X (XTranslateCoordinates), because the registry's x/y are only written at layout time.
 * Build: gcc -std=c11 -O2 -Wall -Wextra -D_DEFAULT_SOURCE -o ops/+x/pet_manager.+x ops/pet_manager.c -lX11 */
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <X11/Xlib.h>

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


/* ---- Interact mode: the same input path pc-hq uses. The khtpm window (an item with relay= and class interact-active) appends "<code> <monotonic_ms>" lines to state/interact_relay.txt;
 * we read the new lines, drop stale ones, and map codes through keybinds.pdl (pc-hq's names). Arrows walk the trainer in the village; camera / POV keys update camera.st (mode 2d|3d, pov 1-4, yaw, pitch, height)
 * for the 3D view to read later. */
static int kb(const char *app, const char *key, int def) {
    char p[PATH_MAX]; snprintf(p, sizeof p, "%s/keybinds.pdl", app); FILE *f = fopen(p, "r"); if (!f) return def; char l[256]; int v = def;
    while (fgets(l, sizeof l, f)) { if (l[0] != 'K' && l[0] != 'O') continue; char a[32], k[64], val[32]; if (sscanf(l, "%31s | %63s | %31s", a, k, val) == 3 && !strcmp(k, key)) { v = atoi(val); break; } }
    fclose(f); return v;
}
static void camera_apply(const char *app, const char *dir, int code) {
    char p[PATH_MAX]; snprintf(p, sizeof p, "%s/camera.st", dir); int mode3d = 0, pov = 1, yaw = 0, pitch = 0, h = 0; FILE *f = fopen(p, "r");
    if (f) { char l[64]; while (fgets(l, sizeof l, f)) { if (!strncmp(l, "mode=3d", 7)) mode3d = 1; else if (!strncmp(l, "pov=", 4)) pov = atoi(l + 4); else if (!strncmp(l, "yaw=", 4)) yaw = atoi(l + 4); else if (!strncmp(l, "pitch=", 6)) pitch = atoi(l + 6); else if (!strncmp(l, "height=", 7)) h = atoi(l + 7); } fclose(f); }
    int chg = 1;
    if (code == kb(app, "render_mode_toggle", 48)) mode3d = !mode3d;
    else if (code == kb(app, "pov_mode_1", 49)) { pov = 1; mode3d = 1; } else if (code == kb(app, "pov_mode_2", 50)) { pov = 2; mode3d = 1; }
    else if (code == kb(app, "pov_mode_3", 51)) { pov = 3; mode3d = 1; } else if (code == kb(app, "pov_mode_4", 52)) { pov = 4; mode3d = 1; }
    else if (code == kb(app, "yaw_left", 113)) yaw = (yaw + 345) % 360; else if (code == kb(app, "yaw_right", 101)) yaw = (yaw + 15) % 360;
    else if (code == kb(app, "pitch_down", 114)) pitch -= 5; else if (code == kb(app, "pitch_up", 116)) pitch += 5;
    else if (code == kb(app, "cam_height_down", 99)) h -= 1; else if (code == kb(app, "cam_height_up", 118)) h += 1;
    else if (code == kb(app, "reset_view", 102)) { yaw = 0; pitch = 0; h = 0; } else chg = 0;
    if (!chg) return; f = fopen(p, "w"); if (f) { fprintf(f, "mode=%s\npov=%d\nyaw=%d\npitch=%d\nheight=%d\n", mode3d ? "3d" : "2d", pov, yaw, pitch, h); fclose(f); }
}
static long relay_off = -1;
static void relay_poll(const char *app, const char *dir, const char *view, long long t) {
    char p[PATH_MAX]; snprintf(p, sizeof p, "%s/interact_relay.txt", dir); FILE *f = fopen(p, "r"); if (!f) return;
    fseek(f, 0, SEEK_END); long sz = ftell(f); if (relay_off < 0 || relay_off > sz) relay_off = sz;      /* first look after arming: ignore what was there */
    fseek(f, relay_off, SEEK_SET); char l[128]; int moves = 0, cap = kb(app, "arrow_run_cap", 3); long long stale = kb(app, "stale_ms", 800);
    while (fgets(l, sizeof l, f)) {
        int code = 0; long long ms = 0; if (sscanf(l, "%d %lld", &code, &ms) < 1) continue;
        if (ms > 0 && t - ms > stale) continue;                                                              /* backlog from a slow frame: drop it */
        const char *dirn = code == kb(app, "arrow_up", 1002) ? "up" : code == kb(app, "arrow_down", 1003) ? "down" : code == kb(app, "arrow_left", 1000) ? "left" : code == kb(app, "arrow_right", 1001) ? "right" : NULL;
        if (dirn) {
            if (strcmp(view, "world") || moves >= cap) continue;
            char cmd[2048]; snprintf(cmd, sizeof cmd, "PET_DIR= PET_SHARED='%s' sh '%s/ops/pet_event.sh' world_move %s >/dev/null 2>&1", dir, app, dirn); sh(cmd, NULL, 0); moves++;
        } else camera_apply(app, dir, code);
    }
    relay_off = ftell(f); fclose(f);
}

int main(int argc, char **argv) {
    (void)argc;
    char self[PATH_MAX]; if (!realpath(argv[0], self)) return 1;
    char app[PATH_MAX], house[PATH_MAX];
    snprintf(app, sizeof app, "%s", self); for (int i = 0; i < 3; i++) { char *s = strrchr(app, '/'); if (s) *s = 0; }      /* .../pet-trainer */
    snprintf(house, sizeof house, "%s", app); for (int i = 0; i < 2; i++) { char *s = strrchr(house, '/'); if (s) *s = 0; }
    const char *pd = getenv("PET_DIR"); char pet[PATH_MAX]; if (pd && pd[0]) snprintf(pet, sizeof pet, "%s", pd); else snprintf(pet, sizeof pet, "%s/state", app);
    int W = getenv("PET_SCENE_W") ? atoi(getenv("PET_SCENE_W")) : 360, H = getenv("PET_SCENE_H") ? atoi(getenv("PET_SCENE_H")) : 280;
    int tick_s = getenv("PET_TICK_S") ? atoi(getenv("PET_TICK_S")) : 30; if (tick_s < 1) tick_s = 30;
    pid_t parent = getppid();
    mkdir(pet, 0755);
    char cmd[4096], buf[4096];
    snprintf(cmd, sizeof cmd, "PET_DIR= PET_SHARED='%s' sh '%s/ops/pet_event.sh' status", pet, app); sh(cmd, buf, sizeof buf);       /* creates the pet if needed */
    Display *xd = XOpenDisplay(NULL);
    long long t_last = now_ms(), t_status = 0, t_tick = now_ms();
    char anim_ui[32] = "idle";
    while (getppid() == parent) {
        long long t = now_ms(), dt = t - t_last; t_last = t;
        int wx = 0, wy = 0;
        char rp[PATH_MAX]; snprintf(rp, sizeof rp, "%s/#.desktop/livedesk_hq_windows_%d.txt", house, (int)parent);
        FILE *rf = fopen(rp, "r"); int got = rf != NULL;
        if (rf) {
            char l[512]; unsigned long wid = 0;
            if (fgets(l, sizeof l, rf)) { char t2[600]; snprintf(t2, sizeof t2, "|%s", l); wx = field(t2, "x"); wy = field(t2, "y"); const char *w = strstr(l, "win=0x"); if (w) wid = strtoul(w + 4, NULL, 16); }
            fclose(rf);
            if (xd && wid) { Window ch; int rx = 0, ry = 0; if (XTranslateCoordinates(xd, (Window)wid, DefaultRootWindow(xd), 0, 0, &rx, &ry, &ch)) { wx = rx; wy = ry; } }
        }
        int floor_h = H - 30;
        static int have_pos = 0, last_wx = 0, last_wy = 0;
        if (got) { have_pos = 1; last_wx = wx; last_wy = wy; }
        if (!have_pos) { usleep(100000); continue; }              /* no window position yet: do not let 0,0 -> real position look like a shove */
        wx = last_wx; wy = last_wy;
        snprintf(cmd, sizeof cmd, "'%s/ops/+x/pet_physics.+x' step '%s/physics.st' %d %d %d %d %lld '%s/physics.pdl'", app, pet, wx, wy, W, floor_h, dt, app);
        sh(cmd, buf, sizeof buf);
        char sx[16] = "180", sy[16] = "250", pa[16] = "rest"; kvs(buf, "pet_x", sx, sizeof sx); kvs(buf, "pet_y", sy, sizeof sy); kvs(buf, "pet_anim", pa, sizeof pa);
        if (t - t_status > 1000) {
            snprintf(cmd, sizeof cmd, "PET_DIR= PET_SHARED='%s' sh '%s/ops/pet_event.sh' status", pet, app); sh(cmd, buf, sizeof buf);
            kvs(buf, "anim", anim_ui, sizeof anim_ui); t_status = t;
        }
        char act[64] = "p1", view[16] = "room"; { char ap[PATH_MAX]; FILE *af; snprintf(ap, sizeof ap, "%s/active.txt", pet); if ((af = fopen(ap, "r"))) { if (fgets(act, sizeof act, af)) act[strcspn(act, "\r\n")] = 0; fclose(af); } snprintf(ap, sizeof ap, "%s/view.txt", pet); if ((af = fopen(ap, "r"))) { if (fgets(view, sizeof view, af)) view[strcspn(view, "\r\n")] = 0; fclose(af); } }
        { char ap[PATH_MAX]; snprintf(ap, sizeof ap, "%s/interact_armed.txt", pet); FILE *af = fopen(ap, "r"); int armed = af && fgetc(af) == '1'; if (af) fclose(af); if (armed) relay_poll(app, pet, view, t); else relay_off = -1; }
        char runp[PATH_MAX]; snprintf(runp, sizeof runp, "%s/pets/%s/running.txt", pet, act); int running = 0; { FILE *rr = fopen(runp, "r"); if (rr) { running = fgetc(rr) == '1'; fclose(rr); } }
        if (!running) t_tick = t;                                                       /* stopped: no day tick, no self care */
        if (t - t_tick > (long long)tick_s * 1000) {
            snprintf(cmd, sizeof cmd, "PET_DIR= PET_SHARED='%s' sh '%s/ops/pet_event.sh' tick", pet, app); sh(cmd, NULL, 0); t_tick = t;
        }
        const char *an = anim_ui;
        if (!strcmp(pa, "fall") || !strcmp(pa, "thud")) an = "surprised"; else if (!strcmp(pa, "walk")) an = "walk";
        if (!strcmp(view, "world")) {
            static long long t_npc = 0; if (t - t_npc > 700) { snprintf(cmd, sizeof cmd, "'%s/ops/+x/pet_world.+x' npcstep '%s' %s", app, pet, act); sh(cmd, NULL, 0); t_npc = t; }
            snprintf(cmd, sizeof cmd, "'%s/ops/+x/pet_scene.+x' world '%s' '%s/scene.raw' %d %d %s %lld", app, pet, pet, W, H, act, (t / 125) % 8);
        } else if (!strcmp(view, "manage")) snprintf(cmd, sizeof cmd, "'%s/ops/+x/pet_scene.+x' manage '%s' '%s/scene.raw' %d %d %s %lld", app, pet, pet, W, H, act, (t / 250) % 8);
        else snprintf(cmd, sizeof cmd, "'%s/ops/+x/pet_scene.+x' room '%s/pets/%s' '%s/scene.raw' %d %d %s %s %s %lld", app, pet, act, pet, W, H, sx, sy, an, (t / 125) % 8);
        sh(cmd, NULL, 0);
        usleep(100000);
    }
    return 0;
}
