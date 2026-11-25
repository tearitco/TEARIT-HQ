/* rmmc_director - the second camera of the rmmc video: a shot list in, a folder of PNG frames and a manifest out (owner 2026-11-25: "make a second camera entity ... that can dump pngs ... think like director";
 * "presentation proof of driven as well as autonomous behavior doing the same things").
 *   rmmc_director.+x <house> <shots.pdl> <outdir> [--only <name>] [--window-pid <pid>]
 * It never draws anything itself. It composes tools that already exist: bv_look (the engine's debug camera, cam_look.txt: absolute or orbiting the selector, which follows Asa once she is possessed),
 * rmmc_verb (the same event-page engine the harness calls), the per-window relay (the same file a human key lands in, read by rmmc_driver), and the engine's own raw frame (pieces/display/rgb_frame_3d_overlay.raw)
 * turned into PNG by raw_to_png. Capture waits for the engine to write a NEW frame after a wake-up (126 in interact_relay.txt), so a frame is never a repeat.
 * Shot list rows (pipe separated, # comments):
 *   SETUP | <verb> <arg>                          stage the scene with no seat check (reset, spawn 140 100)
 *   SEAT | human|harness                         run the seat verb (who may act); written to the manifest as a caption row
 *   SHOT | name | who | act | look | frames | gap_ms | caption
 *      who   : human = the action is a KEY into the window relay (the driver maps it exactly like a person's key); harness = the action is an rmmc_verb call as seat harness
 *      act   : click:<x>,<y> (a real mouse click at that window pixel) | idle (project --idle: a drop still showing is collected) | verb:<verb> <arg> (harness) e.g. verb:walk e | xkey:<xdotool key name> (human) e.g. xkey:Right xkey:space xkey:b | key:<decimal> (a line in the window relay, not the engine; rarely useful) | none (camera only, a hold)
 *      look  : sel <dist> <yaw> <pitch> (orbit Asa) | <x> <y> <z> <dist> <yaw> <pitch> (orbit a cell) | off
 *      frames: PNGs to take after the action; an optional 9th field `window` captures the whole editor window (the UI) instead of the raw 3D frame; gap_ms: pause between frames (0 = as fast as the engine renders)
 * Outputs: <outdir>/<name>_NNN.png, <outdir>/manifest.txt (png | hold seconds | caption), <outdir>/director.log. Raw copies are deleted once converted (3 MB each) and the folder is capped at 600 frames.
 * Footprint: runs only while called; waits on the engine (no spin, 100 ms sleeps); writes at most 600 PNGs per call into the folder you give it (a scratch folder, never the repo).
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/rmmc_director.+x rmmc_director.c */
#define _GNU_SOURCE
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static char HOUSE[PATH_MAX], SESS[PATH_MAX], RAW[PATH_MAX + 80], RCPT[PATH_MAX + 80], WRELAY[PATH_MAX + 80], OUT[PATH_MAX];
static FILE *LOG;
static int NOCALLER;   /* SETUP rows (reset, spawn) run with no seat check: they stage the scene, they are not a player action */
static long now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000L + t.tv_nsec / 1000000L; }
static long mtime_ns(const char *p) { struct stat s; return stat(p, &s) == 0 ? (long)s.st_mtim.tv_sec * 1000000000L + s.st_mtim.tv_nsec : -1; }
static int run(char *const av[], char *const envp[], char *out, size_t osz) {
    int p[2]; if (pipe(p)) return 2; pid_t c = fork(); if (c < 0) return 2;
    if (c == 0) { alarm(25); dup2(p[1], 1); dup2(p[1], 2); close(p[0]); close(p[1]); if (envp) for (; *envp; envp++) putenv(*envp); execv(av[0], av); _exit(127); }
    close(p[1]); size_t n = 0; ssize_t k; while (n < osz - 1 && (k = read(p[0], out + n, osz - 1 - n)) > 0) n += (size_t)k; out[n] = 0; close(p[0]);
    int st = 0; waitpid(c, &st, 0); char *e = strchr(out, '\n'); if (e) *e = 0; return WIFEXITED(st) ? WEXITSTATUS(st) : 99; }
static int kv(const char *path, const char *key, char *out, size_t n) {
    FILE *f = fopen(path, "r"); if (!f) return 0; char l[PATH_MAX + 80]; size_t kl = strlen(key); int ok = 0;
    while (fgets(l, sizeof l, f)) if (!strncmp(l, key, kl) && l[kl] == '=') { l[strcspn(l, "\r\n")] = 0; snprintf(out, n, "%s", l + kl + 1); ok = 1; }
    fclose(f); return ok; }
static void append(const char *path, const char *line) { FILE *f = fopen(path, "a"); if (f) { fputs(line, f); fclose(f); } }
static void wake(void) { char p[PATH_MAX + 80]; snprintf(p, sizeof p, "%s/pieces/apps/player_app/interact_relay.txt", SESS); append(p, "126\n"); }
static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0; return s; }
static int copy_file(const char *a, const char *b) { FILE *i = fopen(a, "rb"), *o = i ? fopen(b, "wb") : NULL; if (!o) { if (i) fclose(i); return 0; } char buf[65536]; size_t n; while ((n = fread(buf, 1, sizeof buf, i)) > 0) fwrite(buf, 1, n, o); fclose(i); fclose(o); return 1; }

/* one frame: wake, wait for a frame newer than `after`, copy it, convert to png; returns 1 on success */
static int WINCAP;   /* 1 = the whole editor window (bars, hotbar, Pals: the UI) instead of the engine's raw 3D frame */
static int grab_window(const char *png) {
    char cmd[PATH_MAX + 200], wid[64] = "", o[300]; FILE *p = popen("xwininfo -root -tree 2>/dev/null | grep ' 1365x934+' | head -1 | awk '{print $1}'", "r"); if (p) { if (fgets(wid, sizeof wid, p)) wid[strcspn(wid, "\r\n")] = 0; pclose(p); }
    if (!wid[0]) return 0; long t0 = mtime_ns(RAW); wake(); for (int i = 0; i < 60 && mtime_ns(RAW) == t0; i++) usleep(100000); usleep(150000);
    char bin[PATH_MAX + 80]; snprintf(bin, sizeof bin, "%s/&.widgits/_shared-lib/ops/+x/dump_frame_png_op.+x", HOUSE); snprintf(cmd, sizeof cmd, "%s", png); char *av[] = { bin, wid, cmd, NULL }; return run(av, NULL, o, sizeof o) == 0; }
static int grab(const char *png) {
    if (WINCAP) return grab_window(png);
    long t0 = mtime_ns(RAW), until = now_ms() + 12000; wake();
    while (now_ms() < until) { if (mtime_ns(RAW) != t0) break; usleep(100000); if (((until - now_ms()) / 1000) % 3 == 0) wake(); }
    if (mtime_ns(RAW) == t0) return 0;
    usleep(60000);   /* the writer closes the file right after the mtime moves */
    char raw[PATH_MAX + 100]; snprintf(raw, sizeof raw, "%s.raw", png); if (!copy_file(RAW, raw)) return 0;
    char bin[PATH_MAX + 80], rc[PATH_MAX + 100], o[256]; snprintf(bin, sizeof bin, "%s/&.widgits/_shared-lib/ops/+x/raw_to_png.+x", HOUSE); snprintf(rc, sizeof rc, "%s", RCPT);
    char *av[] = { bin, raw, (char *)png, "--receipt", rc, NULL }; int r = run(av, NULL, o, sizeof o); remove(raw); return r == 0; }

/* "asa <dist> <yaw> <pitch>" = orbit the actor where the LEDGER says she is: vox at=x,y,z becomes board column x, row z, level y minus the heightmap shift (META shift), re-read every frame so the camera follows a walk */
static int asa_look(const char *look, char *out, size_t n) {
    double d = 4, yw = 200, pt = -20; if (sscanf(look + 3, "%lf %lf %lf", &d, &yw, &pt) < 0) return 0;
    char vop[PATH_MAX + 80], data[PATH_MAX + 40], led[PATH_MAX + 100], hm[PATH_MAX + 80], o[200]; snprintf(vop, sizeof vop, "%s/@.apps/vox/ops/+x/vox_op.+x", HOUSE); snprintf(data, sizeof data, "%s/@.apps/rmmc/vox", HOUSE);
    snprintf(led, sizeof led, "%s/@.apps/rpg-edit-hq/state/games/rmmc/game/ledger.txt", HOUSE); snprintf(hm, sizeof hm, "%s/terrain.heightmap", data);
    char *av[] = { vop, "where", data, led, "asa", NULL }; int ax, ay, az; if (run(av, NULL, o, sizeof o) != 0 || sscanf(o, "at=%d,%d,%d", &ax, &ay, &az) != 3) return 0;
    int shift = 0; FILE *f = fopen(hm, "r"); if (f) { char l[200]; while (fgets(l, sizeof l, f)) if (!strncmp(l, "META | shift |", 14)) shift = atoi(l + 14); fclose(f); }
    snprintf(out, n, "%d %d %d %.1f %.0f %.0f", ax, az, ay - shift, d, yw, pt); return 1; }
static int do_look(const char *look) {
    char bin[PATH_MAX + 80], app[PATH_MAX + 40], o[300]; snprintf(bin, sizeof bin, "%s/&.widgits/board-viewer/tools/+x/bv_look.+x", HOUSE); snprintf(app, sizeof app, "%s/@.apps/rpg-edit-hq", HOUSE);
    char t[200]; if (!strncmp(look, "asa", 3)) { if (!asa_look(look, t, sizeof t)) return 1; } else snprintf(t, sizeof t, "%s", look);
    char *av[] = { bin, app, HOUSE, t, NULL }; int r = run(av, NULL, o, sizeof o); fprintf(LOG, "look '%s' -> %d %s\n", look, r, o); return r; }

/* "idle" = the driver's expiry done by hand: project the desk with --idle, so a drop that is still showing is gone (collected) and the pose is back to idle */
static void do_idle(void) {
    char pop[PATH_MAX + 80], seed[PATH_MAX + 160], desk[PATH_MAX + 160], led[PATH_MAX + 120], pose[PATH_MAX + 60], hm[PATH_MAX + 80], o[300];
    snprintf(pop, sizeof pop, "%s/@.apps/rmmc/ops/+x/rmmc_project_op.+x", HOUSE); snprintf(desk, sizeof desk, "%s/@.apps/rmmc/rmmc-pchq/pieces/system/maps/rmmc/rmmc-0001", HOUSE); snprintf(seed, sizeof seed, "%s/seed", desk);
    snprintf(led, sizeof led, "%s/@.apps/rpg-edit-hq/state/games/rmmc/game/ledger.txt", HOUSE); snprintf(pose, sizeof pose, "%s/@.apps/rmmc/pose.pdl", HOUSE); snprintf(hm, sizeof hm, "%s/@.apps/rmmc/vox/terrain.heightmap", HOUSE);
    char *av[] = { pop, "project", seed, desk, led, pose, hm, "--idle", NULL }; int r = run(av, NULL, o, sizeof o); fprintf(LOG, "idle -> %d %s\n", r, o); }
static int do_verb(const char *verb, const char *arg) {
    char bin[PATH_MAX + 80], o[300], g[200], c[64]; snprintf(bin, sizeof bin, "%s/@.apps/rmmc/ops/+x/rmmc_verb.+x", HOUSE);
    snprintf(g, sizeof g, "GAME_ARG=%s", arg); snprintf(c, sizeof c, "RMMC_CALLER=%s", (NOCALLER || !strcmp(verb, "seat")) ? "" : "harness");
    char *ev[] = { g, c, NULL }; if (!c[strlen("RMMC_CALLER=")]) ev[1] = NULL;
    char *av[] = { bin, (char *)verb, NULL }; int r = run(av, ev, o, sizeof o); fprintf(LOG, "verb %s '%s' -> %d %s\n", verb, arg, r, o); return r; }

int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "Usage: rmmc_director.+x <house> <shots.pdl> <outdir> [--only <name>] [--window-pid <pid>]\n"); return 2; }
    snprintf(HOUSE, sizeof HOUSE, "%s", argv[1]); snprintf(OUT, sizeof OUT, "%s", argv[3]); const char *only = NULL, *wpid = "";
    for (int i = 4; i + 1 < argc; i += 2) { if (!strcmp(argv[i], "--only")) only = argv[i + 1]; else if (!strcmp(argv[i], "--window-pid")) wpid = argv[i + 1]; }
    char ui[PATH_MAX + 80]; snprintf(ui, sizeof ui, "%s/@.apps/rpg-edit-hq/board/state/ui.txt", HOUSE);
    if (!kv(ui, "bv_session", SESS, sizeof SESS) || access(SESS, F_OK)) { fprintf(stderr, "no live engine session in %s\n", ui); return 2; }
    snprintf(RAW, sizeof RAW, "%s/pieces/display/rgb_frame_3d_overlay.raw", SESS); snprintf(RCPT, sizeof RCPT, "%s/pieces/display/rgb_frame_3d_overlay.receipt.txt", SESS);
    char wp[32]; snprintf(wp, sizeof wp, "%s", wpid);
    snprintf(WRELAY, sizeof WRELAY, "%s/#.desktop/entity_menu_history/%s.txt", HOUSE, wp);
    mkdir(OUT, 0775); char lp[PATH_MAX + 40], mp[PATH_MAX + 40]; snprintf(lp, sizeof lp, "%s/director.log", OUT); snprintf(mp, sizeof mp, "%s/manifest.txt", OUT); LOG = fopen(lp, "a"); if (!LOG) return 2;
    FILE *sf = fopen(argv[2], "r"); if (!sf) { fprintf(stderr, "cannot read %s\n", argv[2]); return 2; } FILE *mf = fopen(mp, "a"); if (!mf) return 2;
    fprintf(LOG, "session %s relay %s (window pid '%s')\n", SESS, WRELAY, wp);
    char line[1024]; int total = 0, bad = 0;
    while (fgets(line, sizeof line, sf)) {
        char *s = trim(line); if (!*s || *s == '#') continue; char *f[10]; int n = 0; for (char *t = strtok(s, "|"); t && n < 10; t = strtok(NULL, "|")) f[n++] = trim(t);
        if (n >= 2 && !strcmp(f[0], "SETUP")) { char v[40], a[120] = ""; if (sscanf(f[1], "%39s %119[^\n]", v, a) >= 1) { NOCALLER = 1; do_verb(v, a); NOCALLER = 0; } continue; }
        if (n >= 2 && !strcmp(f[0], "SEAT")) { int r = do_verb("seat", f[1]); fprintf(mf, "- | 0 | seat %s: %s\n", f[1], r == 0 ? "ok" : "refused"); continue; }
        if (n < 8 || strcmp(f[0], "SHOT")) { fprintf(LOG, "skip row: %s\n", f[0]); continue; }
        WINCAP = (n >= 9 && !strcmp(f[8], "window"));
        const char *name = f[1], *who = f[2], *act = f[3], *look = f[4]; int frames = atoi(f[5]), gap = atoi(f[6]); const char *cap = f[7];
        if (only && strcmp(only, name)) continue;
        if (frames < 1) frames = 1;
        if (frames > 120) frames = 120;
        if (strcmp(look, "keep")) do_look(look);
        if (!strncmp(act, "verb:", 5)) { char v[40], a[120] = ""; if (sscanf(act + 5, "%39s %119[^\n]", v, a) >= 1) do_verb(v, a); }
        else if (!strcmp(act, "idle")) do_idle();
        else if (!strncmp(act, "click:", 6)) {   /* a real mouse click at window pixel x,y (board_ctl click: pointer onto the spot, then the press through the window relay), as a person clicks a hotbar slot */
            int cx = 0, cy = 0; char bin[PATH_MAX + 80], app[PATH_MAX + 40], o[300], sx[16], sy[16]; sscanf(act + 6, "%d,%d", &cx, &cy); snprintf(sx, sizeof sx, "%d", cx); snprintf(sy, sizeof sy, "%d", cy);
            snprintf(bin, sizeof bin, "%s/&.widgits/board-viewer/tools/+x/board_ctl.+x", HOUSE); snprintf(app, sizeof app, "%s/@.apps/rpg-edit-hq", HOUSE); char *ev[] = { "BOARD_CTL_SIZE=1365x934", NULL }; char *av[] = { bin, "click", app, sx, sy, NULL };
            int r = run(av, ev, o, sizeof o); fprintf(LOG, "click %d,%d -> %d %s\n", cx, cy, r, o); }
        else if (!strncmp(act, "xkey:", 5)) {   /* a real X key into the board window, exactly what a person does (board_ctl key: pointer onto the window, focus, xdotool key, wait) */
            char bin[PATH_MAX + 80], app[PATH_MAX + 40], o[300], kn[64], ms[16] = "500"; snprintf(bin, sizeof bin, "%s/&.widgits/board-viewer/tools/+x/board_ctl.+x", HOUSE); snprintf(app, sizeof app, "%s/@.apps/rpg-edit-hq", HOUSE);
            char seq[160]; snprintf(seq, sizeof seq, "%s", act + 5); char *ev[] = { "BOARD_CTL_SIZE=1365x934", NULL };   /* xkey:<key> [<key> ...]: a sequence, as a person types a nav number then Enter (2 2 Return) */
            for (char *sv, *tk = strtok_r(seq, " ", &sv); tk; tk = strtok_r(NULL, " ", &sv)) { snprintf(kn, sizeof kn, "%s", tk); char *av[] = { bin, "key", app, kn, ms, NULL }; int r = run(av, ev, o, sizeof o); fprintf(LOG, "xkey %s -> %d %s\n", kn, r, o); } }
        else if (!strncmp(act, "key:", 4)) { char l[64]; snprintf(l, sizeof l, "KEY_PRESSED: %d\n", atoi(act + 4)); append(WRELAY, l); fprintf(LOG, "key %s -> %s\n", act + 4, WRELAY); }
        for (int i = 0; i < frames && total < 600; i++) {
            if (!strncmp(look, "asa", 3) && i > 0) do_look(look);
            char png[PATH_MAX + 120]; snprintf(png, sizeof png, "%s/%s_%03d.png", OUT, name, i); long t = now_ms(); int ok = grab(png);
            fprintf(LOG, "%s %s frame %d %s %ldms\n", name, who, i, ok ? "ok" : "MISSING", now_ms() - t); if (!ok) { bad++; continue; }
            total++; fprintf(mf, "%s_%03d.png | %.2f | %s (%s)\n", name, i, gap > 0 ? gap / 1000.0 : 0.12, cap, who); if (gap > 0) usleep((useconds_t)gap * 1000); }
    }
    fclose(sf); fclose(mf); fclose(LOG); printf("director: %d frames, %d missing, manifest %s\n", total, bad, mp); return bad ? 1 : 0; }
