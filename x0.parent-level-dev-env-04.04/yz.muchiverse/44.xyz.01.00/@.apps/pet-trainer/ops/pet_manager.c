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

static volatile int g_term = 0; static void on_term(int s) { (void)s; g_term = 1; }
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
    if (code == kb(app, "map_mode", 53)) pov = (pov == 5) ? 1 : 5;
    else if (code == kb(app, "render_mode_toggle", 48)) mode3d = !mode3d;
    else if (code == kb(app, "pov_mode_1", 49)) { pov = 1; mode3d = 1; } else if (code == kb(app, "pov_mode_2", 50)) { pov = 2; mode3d = 1; }
    else if (code == kb(app, "pov_mode_3", 51)) { pov = 3; mode3d = 1; } else if (code == kb(app, "pov_mode_4", 52)) { pov = 4; mode3d = 1; }
    else if (code == kb(app, "yaw_left", 113)) yaw = (yaw + 345) % 360; else if (code == kb(app, "yaw_right", 101)) yaw = (yaw + 15) % 360;
    else if (code == kb(app, "pitch_down", 114)) pitch -= 5; else if (code == kb(app, "pitch_up", 116)) pitch += 5;
    else if (code == kb(app, "cam_height_down", 99)) h -= 1; else if (code == kb(app, "cam_height_up", 118)) h += 1;
    else if (code == kb(app, "reset_view", 102)) { yaw = 0; pitch = 0; h = 0; } else chg = 0;
    if (!chg) return; f = fopen(p, "w"); if (f) { fprintf(f, "mode=%s\npov=%d\nyaw=%d\npitch=%d\nheight=%d\n", mode3d ? "3d" : "2d", pov, yaw, pitch, h); fclose(f); }
}
typedef struct { int x0, x1, top; char name[24]; } Plat;
/* rooms.pdl: PLATFORM | room | x0 | x1 | top y | name  (furniture the pet may jump on) */
static int load_plats(const char *app, const char *loc, Plat *p, int max) {
    char rp[PATH_MAX], l[200]; snprintf(rp, sizeof rp, "%s/rooms.pdl", app); FILE *f = fopen(rp, "r"); int n = 0; if (!f) return 0;
    while (n < max && fgets(l, sizeof l, f)) { char rm[32], nm[40]; int a, b, c; if (sscanf(l, "PLATFORM | %31s | %d | %d | %d | %39[^\n]", rm, &a, &b, &c, nm) == 5 && !strcmp(rm, loc)) { p[n].x0 = a; p[n].x1 = b; p[n].top = c; snprintf(p[n].name, sizeof p[n].name, "%s", nm); size_t k = strlen(p[n].name); while (k && p[n].name[k - 1] == ' ') p[n].name[--k] = 0; n++; } }
    fclose(f); return n;
}
static int room_scene(const char *app, const char *loc) {   /* rooms.pdl ROOM | id | scene id | name */
    char rp[PATH_MAX], l[200]; snprintf(rp, sizeof rp, "%s/rooms.pdl", app); FILE *f = fopen(rp, "r"); int sc = 0; if (!f) return 0;
    while (fgets(l, sizeof l, f)) { char id[32]; int s; if (sscanf(l, "ROOM | %31s | %d", id, &s) == 2 && !strcmp(id, loc)) { sc = s; break; } }
    fclose(f); return sc;
}
static int cam_pov(const char *dir) { char p[PATH_MAX], l[64]; int pv = 1; snprintf(p, sizeof p, "%s/camera.st", dir); FILE *f = fopen(p, "r"); if (!f) return 1; while (fgets(l, sizeof l, f)) if (!strncmp(l, "pov=", 4)) pv = atoi(l + 4); fclose(f); return pv; }
static long relay_off = -1, hist_off = -1;
static void esc_poll(const char *dir) {        /* Esc (27) is forwarded to keyboard/history.txt only: that is the way out of Interact mode (arrows and camera keys stop, nav numbers work again) */
    char p[PATH_MAX]; snprintf(p, sizeof p, "%s/keyboard/history.txt", dir); FILE *f = fopen(p, "r"); if (!f) return;
    fseek(f, 0, SEEK_END); long sz = ftell(f); if (hist_off < 0 || hist_off > sz) hist_off = sz; fseek(f, hist_off, SEEK_SET); char l[64]; int esc = 0;
    while (fgets(l, sizeof l, f)) if (strstr(l, "KEY_PRESSED: 27")) esc = 1;
    hist_off = ftell(f); fclose(f);
    if (esc) { { char cp[PATH_MAX], cl[256] = ""; snprintf(cp, sizeof cp, "%s/camera.st", dir); FILE *cf = fopen(cp, "r"); if (cf) { size_t n = fread(cl, 1, sizeof cl - 1, cf); cl[n] = 0; fclose(cf); char *q = strstr(cl, "pov=5"); if (q) { q[4] = '1'; cf = fopen(cp, "w"); if (cf) { fputs(cl, cf); fclose(cf); } } } }      /* leaving Interact also leaves the debug map (pov 5) */
      snprintf(p, sizeof p, "%s/interact_armed.txt", dir); FILE *a = fopen(p, "w"); if (a) { fputs("0\n", a); fclose(a); } }
}
static void relay_poll(const char *app, const char *dir, const char *view, long long t) {
    char p[PATH_MAX]; snprintf(p, sizeof p, "%s/interact_relay.txt", dir); FILE *f = fopen(p, "r"); if (!f) return;
    fseek(f, 0, SEEK_END); long sz = ftell(f); if (relay_off < 0 || relay_off > sz) relay_off = sz;      /* first look after arming: ignore what was there */
    fseek(f, relay_off, SEEK_SET); char l[128]; int moves = 0, cap = kb(app, "arrow_run_cap", 3); long long stale = kb(app, "stale_ms", 800);
    long line_start = ftell(f);
    while (fgets(l, sizeof l, f)) {
        int code = 0; long long ms = 0; if (sscanf(l, "%d %lld", &code, &ms) < 1) { line_start = ftell(f); continue; }
        if (ms > 0 && t - ms > stale) { line_start = ftell(f); continue; }                                                              /* backlog from a slow frame: drop it */
        const char *dirn = code == kb(app, "arrow_up", 1002) ? "up" : code == kb(app, "arrow_down", 1003) ? "down" : code == kb(app, "arrow_left", 1000) ? "left" : code == kb(app, "arrow_right", 1001) ? "right" : NULL;
        if (dirn) {
            if (strcmp(view, "world")) { line_start = ftell(f); continue; }
            if (moves >= cap) { relay_off = line_start; fclose(f); return; }                     /* run cap reached: keep the rest for the next tick, never drop it */
            char cmd[2048]; snprintf(cmd, sizeof cmd, "PET_DIR= PET_SHARED='%s' sh '%s/ops/pet_event.sh' world_move %s >/dev/null 2>&1", dir, app, dirn); sh(cmd, NULL, 0); moves++;
        } else camera_apply(app, dir, code);
        line_start = ftell(f);
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
    Display *xd = XOpenDisplay(NULL); srand((unsigned)time(NULL) ^ (unsigned)getpid()); unsigned long g_wid = 0;
    long long t_last = now_ms(), t_status = 0, t_tick = now_ms();
    char anim_ui[32] = "idle";
    signal(SIGTERM, on_term); signal(SIGINT, on_term);
    while (getppid() == parent && !g_term) {
        long long t = now_ms(), dt = t - t_last; t_last = t;
        int wx = 0, wy = 0;
        char rp[PATH_MAX]; snprintf(rp, sizeof rp, "%s/#.desktop/livedesk_hq_windows_%d.txt", house, (int)parent);
        FILE *rf = fopen(rp, "r"); int got = rf != NULL;
        if (rf) {
            char l[512]; unsigned long wid = 0;
            if (fgets(l, sizeof l, rf)) { char t2[600]; snprintf(t2, sizeof t2, "|%s", l); wx = field(t2, "x"); wy = field(t2, "y"); const char *w = strstr(l, "win=0x"); if (w) wid = strtoul(w + 4, NULL, 16); }
            fclose(rf);
            if (wid) g_wid = wid; if (xd && wid) { Window ch; int rx = 0, ry = 0; if (XTranslateCoordinates(xd, (Window)wid, DefaultRootWindow(xd), 0, 0, &rx, &ry, &ch)) { wx = rx; wy = ry; } }
        }
        int floor_h = H - 30;
        static int have_pos = 0, last_wx = 0, last_wy = 0;
        if (got) { have_pos = 1; last_wx = wx; last_wy = wy; }
        if (!have_pos) { usleep(100000); continue; }              /* no window position yet: do not let 0,0 -> real position look like a shove */
        wx = last_wx; wy = last_wy;
        static int pwx = -99999, pwy = -99999; static char psx[16] = "180", psy[16] = "250", ppa[16] = "rest";
        char sx[16], sy[16], pa[16];
        if (wx == pwx && wy == pwy && !strcmp(ppa, "rest")) { strcpy(sx, psx); strcpy(sy, psy); strcpy(pa, ppa); }          /* window still, pet at rest: no physics process this tick */
        else {
            snprintf(cmd, sizeof cmd, "'%s/ops/+x/pet_physics.+x' step '%s/physics.st' %d %d %d %d %lld '%s/physics.pdl'", app, pet, wx, wy, W, floor_h, dt, app);
            sh(cmd, buf, sizeof buf);
            strcpy(sx, "180"); strcpy(sy, "250"); strcpy(pa, "rest"); kvs(buf, "pet_x", sx, sizeof sx); kvs(buf, "pet_y", sy, sizeof sy); kvs(buf, "pet_anim", pa, sizeof pa);
            pwx = wx; pwy = wy; strcpy(psx, sx); strcpy(psy, sy); strcpy(ppa, pa);
        }
        if (t - t_status > 1000) {
            snprintf(cmd, sizeof cmd, "PET_DIR= PET_SHARED='%s' sh '%s/ops/pet_event.sh' status", pet, app); sh(cmd, buf, sizeof buf);
            kvs(buf, "anim", anim_ui, sizeof anim_ui); t_status = t;
        }
        char act[64] = "p1", view[16] = "room"; { char ap[PATH_MAX]; FILE *af; snprintf(ap, sizeof ap, "%s/active.txt", pet); if ((af = fopen(ap, "r"))) { if (fgets(act, sizeof act, af)) act[strcspn(act, "\r\n")] = 0; fclose(af); } snprintf(ap, sizeof ap, "%s/view.txt", pet); if ((af = fopen(ap, "r"))) { if (fgets(view, sizeof view, af)) view[strcspn(view, "\r\n")] = 0; fclose(af); } }
        { char ap[PATH_MAX]; snprintf(ap, sizeof ap, "%s/interact_armed.txt", pet); FILE *af = fopen(ap, "r"); int armed = af && fgetc(af) == '1'; if (af) fclose(af); if (armed) { relay_poll(app, pet, view, t); esc_poll(pet); } else { relay_off = -1; hist_off = -1; } }
        char runp[PATH_MAX]; snprintf(runp, sizeof runp, "%s/pets/%s/running.txt", pet, act); int running = 0; { FILE *rr = fopen(runp, "r"); if (rr) { running = fgetc(rr) == '1'; fclose(rr); } }
        if (!running) t_tick = t;                                                       /* stopped: no day tick, no self care */
        const char *an = anim_ui;
        if (!strcmp(pa, "fall") || !strcmp(pa, "thud")) an = "surprised"; else if (!strcmp(pa, "walk")) an = "walk";
        /* autonomy, only while the pet is started: wander, hop, talk, hum; a shaken window makes it react. Physics (a shove, a fall) owns the position; autonomy only moves a resting pet. */
        static double ax = -1; static int tgt = 180; static char door_ev[64] = ""; static long long nxt_move = 0, nxt_hop = 0, hop0 = -1, nxt_say = 0, nxt_hum = 0; static char ppa2[16] = "rest";
        int at_rest = !strcmp(pa, "rest"); double hopy = 0; char sxo[16], syo[16];
        char loc[32] = "bedroom"; long long arrive_seq = 0; int arrive_x = 150, rid = 0;
        { char lp[PATH_MAX]; FILE *lf; snprintf(lp, sizeof lp, "%s/pets/%s/loc.txt", pet, act); if ((lf = fopen(lp, "r"))) { if (fgets(loc, sizeof loc, lf)) loc[strcspn(loc, "\r\n")] = 0; fclose(lf); }
          snprintf(lp, sizeof lp, "%s/pets/%s/arrive_seq.txt", pet, act); if ((lf = fopen(lp, "r"))) { char q[40]; if (fgets(q, sizeof q, lf)) arrive_seq = atoll(q); fclose(lf); }
          snprintf(lp, sizeof lp, "%s/pets/%s/arrive_x.txt", pet, act); if ((lf = fopen(lp, "r"))) { char q[16]; if (fgets(q, sizeof q, lf)) arrive_x = atoi(q); fclose(lf); } }
        rid = room_scene(app, loc);
        static long long seen_seq = 0; static char seen_act[64] = "";
        if (strcmp(seen_act, act)) { snprintf(seen_act, sizeof seen_act, "%s", act); seen_seq = arrive_seq; ax = -1; }      /* another pet became the active one: start from its resting x */
        static Plat pl[8]; static int npl = 0, plat_on = -1, plat_goal = -1, lift_goal = 0; static double lift_from = 0, hop_amp = 36; static char pl_loc[32] = ""; static long long down_at = 0;
        if (strcmp(pl_loc, loc)) { snprintf(pl_loc, sizeof pl_loc, "%s", loc); npl = load_plats(app, loc, pl, 8); plat_on = plat_goal = -1; lift_goal = 0; lift_from = 0; hop0 = -1; }
        if (arrive_seq != seen_seq) { seen_seq = arrive_seq; ax = arrive_x; tgt = arrive_x; nxt_move = t + 2500; plat_on = plat_goal = -1; lift_goal = 0; lift_from = 0; hop0 = -1; }            /* a teleport event moved it: appear at the arrival door */
        if (!nxt_say) { nxt_say = t + 8000; nxt_hum = t + 12000; nxt_hop = t + 5000; }
        if (ax < 0 || (!at_rest && ax < 0)) ax = atof(sx); else if (!at_rest) ax = atof(sx);
        if (running && !strcmp(view, "room") && at_rest) {
            int hopping = (hop0 >= 0 && t - hop0 < 700);
            { char wp[PATH_MAX]; snprintf(wp, sizeof wp, "%s/pets/%s/want_platform.txt", pet, act); FILE *wf = fopen(wp, "r");      /* the taught verb: climb <name> / climb down */
              if (wf) { char nm[40] = ""; if (fgets(nm, sizeof nm, wf)) nm[strcspn(nm, "\r\n")] = 0; fclose(wf); remove(wp);
                  if (!strncmp(nm, "down", 4) || !strncmp(nm, "floor", 5)) { if (plat_on >= 0) down_at = t; plat_goal = -1; }
                  else for (int i = 0; i < npl; i++) if (strstr(pl[i].name, nm)) { plat_goal = i; break; } } }
            if (!hopping && plat_on < 0 && plat_goal < 0 && t > nxt_move) { door_ev[0] = 0; tgt = 90 + rand() % (W - 90 - 110); nxt_move = t + 3500 + rand() % 7000;
                if (npl > 0 && rand() % 100 < 22) plat_goal = rand() % npl;                                                                     /* now and then it wants to be up on the furniture */
                else if (rand() % 100 < 14) { char rp2[PATH_MAX]; snprintf(rp2, sizeof rp2, "%s/rooms.pdl", app); FILE *rf2 = fopen(rp2, "r"); if (rf2) { char l2[200]; while (fgets(l2, sizeof l2, rf2)) { char rm[32], ds[32], au[16]; int dx = 0, ar = 0; if (sscanf(l2, "DOOR | %31s | %d | %31s | %d | %15s", rm, &dx, ds, &ar, au) == 5 && !strcmp(rm, loc) && !strcmp(au, "auto=1")) { tgt = dx; snprintf(door_ev, sizeof door_ev, "door_%s_%s", rm, ds); nxt_move = t + 9000; break; } } fclose(rf2); } } }
            if (plat_goal >= 0 && plat_goal < npl) { tgt = (pl[plat_goal].x0 + pl[plat_goal].x1) / 2; if (!hopping && ax >= tgt - 6 && ax <= tgt + 6) {      /* under it: jump up */
                lift_from = lift_goal; lift_goal = (H - 30) - pl[plat_goal].top; hop_amp = 18; hop0 = t; plat_on = plat_goal; plat_goal = -1; down_at = t + 9000 + rand() % 8000; nxt_move = t + 3000; } }
            if (plat_on >= 0 && !hopping) {                                                                                                       /* up there: stroll along the top, then hop down */
                if (t > nxt_move) { tgt = pl[plat_on].x0 + 14 + rand() % (pl[plat_on].x1 - pl[plat_on].x0 - 28 > 1 ? pl[plat_on].x1 - pl[plat_on].x0 - 28 : 1); nxt_move = t + 2500 + rand() % 3000; }
                if (t > down_at) { lift_from = lift_goal; lift_goal = 0; hop_amp = 18; hop0 = t; plat_on = -1; nxt_move = t + 2000; } }
            if (door_ev[0] && plat_on < 0 && ax >= tgt - 6 && ax <= tgt + 6) { snprintf(cmd, sizeof cmd, "PET_DIR= PET_SHARED='%s' sh '%s/ops/pet_event.sh' fire %s >/dev/null 2>&1", pet, app, door_ev); sh(cmd, NULL, 0); door_ev[0] = 0; nxt_move = t + 3000; }
            double step = 45.0 * (double)dt / 1000.0;
            if (ax < tgt - 2) { ax += step; an = "walk"; } else if (ax > tgt + 2) { ax -= step; an = "walk"; }
            if (!hopping && t > nxt_hop) { lift_from = lift_goal; hop_amp = 36; hop0 = t; nxt_hop = t + 9000 + rand() % 16000; }
            if (hop0 >= 0 && t - hop0 < 700) { double u = (double)(t - hop0) / (hop_amp > 20 ? 600.0 : 700.0); if (u > 1) u = 1; hopy = lift_from + (lift_goal - lift_from) * u + hop_amp * 4.0 * u * (1.0 - u); an = "happy"; }
            else hopy = lift_goal;
        } else if (!strcmp(view, "room")) hopy = lift_goal;
        snprintf(sxo, sizeof sxo, "%d", (int)ax); snprintf(syo, sizeof syo, "%d", atoi(sy) - (int)hopy);
        { int visible = 0; if (xd && g_wid) { XWindowAttributes wa; if (XGetWindowAttributes(xd, (Window)g_wid, &wa)) visible = wa.map_state == IsViewable; }
          if (running) {
              if (t > nxt_say) { snprintf(cmd, sizeof cmd, "PET_DIR= PET_SHARED='%s' sh '%s/ops/pet_event.sh' speak >/dev/null 2>&1", pet, app); sh(cmd, NULL, 0); nxt_say = t + 25000 + rand() % 35000; }
              if (visible && !strcmp(view, "room") && t > nxt_hum) { snprintf(cmd, sizeof cmd, "PET_DIR= PET_SHARED='%s' sh '%s/ops/pet_event.sh' hum >/dev/null 2>&1", pet, app); sh(cmd, NULL, 0); nxt_hum = t + 15000 + rand() % 15000; }
              if (!strcmp(pa, "fall") && strcmp(ppa2, "fall")) { snprintf(cmd, sizeof cmd, "PET_DIR= PET_SHARED='%s' sh '%s/ops/pet_event.sh' react fall >/dev/null 2>&1", pet, app); sh(cmd, NULL, 0); }
              if (!strcmp(pa, "thud") && strcmp(ppa2, "thud")) { snprintf(cmd, sizeof cmd, "PET_DIR= PET_SHARED='%s' sh '%s/ops/pet_event.sh' react land >/dev/null 2>&1", pet, app); sh(cmd, NULL, 0); }
          }
          snprintf(ppa2, sizeof ppa2, "%s", pa); }
        if (!strcmp(view, "world")) {
            static long long t_npc = 0; if (t - t_npc > 700) { snprintf(cmd, sizeof cmd, "'%s/ops/+x/pet_world.+x' npcstep '%s' %s", app, pet, act); sh(cmd, NULL, 0); t_npc = t; }
            snprintf(cmd, sizeof cmd, "'%s/ops/+x/pet_scene.+x' world '%s' '%s/scene.raw' %d %d %s %lld", app, pet, pet, W, H, act, (t / 400) % 8);
        } else if (!strcmp(view, "manage")) snprintf(cmd, sizeof cmd, "'%s/ops/+x/pet_scene.+x' manage '%s' '%s/scene.raw' %d %d %s %lld", app, pet, pet, W, H, act, (t / 500) % 8);
        else if (cam_pov(pet) == 5) snprintf(cmd, sizeof cmd, "'%s/ops/+x/pet_scene.+x' map '%s' '%s/scene.raw' %d %d %d", app, pet, pet, W, H, rid);
        else snprintf(cmd, sizeof cmd, "'%s/ops/+x/pet_scene.+x' room '%s/pets/%s' '%s/scene.raw' %d %d %s %s %s %lld %d", app, pet, act, pet, W, H, sxo, syo, an, (t / 400) % 8, rid);
        sh(cmd, NULL, 0);
        usleep(200000);
    }
    { char c2[2048]; snprintf(c2, sizeof c2, "PET_SHARED='%s' sh '%s/ops/pet_clock.sh' stop >/dev/null 2>&1", pet, app); sh(c2, NULL, 0); }      /* the window closed: stop the pet clock daemon (no orphan, zero CPU) */
    return 0;
}
