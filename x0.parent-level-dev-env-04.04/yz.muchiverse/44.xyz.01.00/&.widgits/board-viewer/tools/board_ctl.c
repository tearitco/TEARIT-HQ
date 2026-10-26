/* board_ctl - drive and photograph a board window (rpg-space and the other petslike boards) the way a person does, and say so loudly when control is lost (owner 2026-10-24: "what could be improved in our pipeline to enable you?").
 * One tool, one app dir argument (<house>/@.apps/<app>), because every failure of the 2026-10-24 session was the environment, not the game: the engine session folder deleted (window keeps its last frame and ignores keys),
 * a snapshot taken before the engine rendered (a frame behind the key), the camera that only takes relative keys, clicks that do nothing.
 *   board_ctl status    <app>                       one line per check: session folder, engine process (cwd not deleted), bv_state lines, window, game daemon; exit 0 when all hold, 1 otherwise
 *   board_ctl preflight <app>                       status + the engine really renders (nudge, the frame file must change within 10 s); exit 0 only then. Run before ANY proof or recording.
 *   board_ctl recover   <app>                       end the app's window and engine by pid, relaunch (button.sh run, then the piececraft engine by hand when no engine appears), wait for a live session
 *   board_ctl key       <app> <xdotool key> [ms]    put the pointer inside the window (the desktop is focus-follows-mouse: a key goes to the window under the pointer), focus it, press one real key, wait ms (default 1500)
 *   board_ctl click     <app> <x> <y>               a click in window pixels: pointer moved onto the spot, then the press through the per-pid relay (a bare xdotool click did not reach the popup, and the relay click without the pointer there did nothing)
 *   board_ctl camera    <app> <preset>              absolute camera from tools/camera_presets.pdl (written into the session's bv_state.txt without touching the other lines), then a redraw nudge
 *   board_ctl camset    <app> key=value ... | show   exact camera coordinates: cam_yaw cam_pitch cam_pan_x cam_pan_y cam_pan_z cam_z_level camera_mode current_z are written into bv_state.txt, then a redraw nudge (show prints them)
 *   board_ctl snap      <app> <out.png> [--same-ok] [--state f1,f2,..]
 *                                                   nudge, wait until the engine has rendered AFTER the nudge, dump the window (dump_frame_png_op), write <out.png>.receipt.txt (hash, session, frame time,
 *                                                   the first line of each --state file, relative to <app>/state). Exit 3 when the picture is byte-identical to the previous snap (stale) unless --same-ok.
 *   board_ctl manifest  <dir> <steps.txt>           steps.txt rows: <snapshot.png> | <hold s> | <caption with {file} placeholders>; {name} is replaced by the first state line recorded in that snapshot's receipt
 *                                                   (state names as given to --state, e.g. trainer.txt:x). Writes <dir>/manifest.txt for make_presentation_video.py and fails when two consecutive snapshots are identical.
 * *   board_ctl expect    <app> <state[:key]> <value> [seconds]   precondition / postcondition check on <app>/state (flag_int.txt 1, flag_ask.txt 1, trainer.txt:x 7 ...); waits up to [seconds]; exit 4 when not true
 * Build: sh tools/build_board_ctl.sh -> tools/+x/board_ctl.+x. Skill: .claude/skills/rpg-space-board-driving/SKILL.md */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/file.h>
#include <sys/stat.h>

static char APP[PATH_MAX], HOUSE[PATH_MAX], SESS[PATH_MAX], HERE[PATH_MAX];
static long now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000L + t.tv_nsec / 1000000L; }
static int slurp(const char *p, char *b, size_t cap) { FILE *f = fopen(p, "r"); if (!f) { b[0] = 0; return 0; } size_t n = fread(b, 1, cap - 1, f); b[n] = 0; fclose(f); return (int)n; }
static int exists(const char *p) { struct stat s; return stat(p, &s) == 0; }
static long mtime_ms(const char *p) { struct stat s; return stat(p, &s) == 0 ? (long)s.st_mtim.tv_sec * 1000L + s.st_mtim.tv_nsec / 1000000L : -1; }
static unsigned long long fnv_file(const char *p) { FILE *f = fopen(p, "rb"); unsigned long long h = 1469598103934665603ULL; int c; if (!f) return 0; while ((c = fgetc(f)) != EOF) { h ^= (unsigned)c; h *= 1099511628211ULL; } fclose(f); return h; }
static int run(const char *cmd, char *out, size_t cap) { FILE *f = popen(cmd, "r"); if (!f) return -1; size_t n = 0; if (out) { while (n < cap - 1) { int c = fgetc(f); if (c == EOF) break; out[n++] = (char)c; } out[n] = 0; } else { while (fgetc(f) != EOF) {} } return pclose(f); }

static int load_session(void) {      /* bv_session= of <app>/board/state/ui.txt */
    char p[PATH_MAX + 40], b[4096]; snprintf(p, sizeof p, "%s/board/state/ui.txt", APP); slurp(p, b, sizeof b); SESS[0] = 0;
    for (char *l = strtok(b, "\n"); l; l = strtok(NULL, "\n")) if (!strncmp(l, "bv_session=", 11)) snprintf(SESS, sizeof SESS, "%s", l + 11);
    return SESS[0] != 0;
}
static int pid_cmd_has(int pid, const char *needle1, const char *needle2) {
    char p[64], b[8192]; snprintf(p, sizeof p, "/proc/%d/cmdline", pid); FILE *f = fopen(p, "r"); if (!f) return 0; size_t n = fread(b, 1, sizeof b - 1, f); fclose(f);
    for (size_t i = 0; i < n; i++) if (!b[i]) b[i] = ' '; b[n] = 0; return (!needle1 || strstr(b, needle1)) && (!needle2 || strstr(b, needle2));
}
static int find_pids(const char *n1, const char *n2, int *out, int max) {      /* every pid whose command line holds both strings (never this process) */
    DIR *d = opendir("/proc"); struct dirent *e; int k = 0; if (!d) return 0;
    while ((e = readdir(d)) && k < max) { int pid = atoi(e->d_name); if (pid > 1 && pid != getpid() && pid_cmd_has(pid, n1, n2)) out[k++] = pid; } closedir(d); return k;
}
static int engine_pid(int *deleted) {      /* the bv_render_3d daemon of THIS session: its cwd is the session folder (or "(deleted)") */
    int pids[16], n = find_pids("bv_render_3d", "--daemon", pids, 16); char want[PATH_MAX]; snprintf(want, sizeof want, "%s", SESS); *deleted = 0;
    for (int i = 0; i < n; i++) { char l[64], t[PATH_MAX + 32] = ""; snprintf(l, sizeof l, "/proc/%d/cwd", pids[i]); ssize_t r = readlink(l, t, sizeof t - 1); if (r > 0) t[r] = 0;
        if (strstr(t, "(deleted)")) { *deleted = 1; return pids[i]; } if (SESS[0] && strstr(t, strrchr(want, '/') + 1)) return pids[i]; }
    return 0;
}
static int window_pid(void) { int p[8], n = find_pids("khtpm_core_render", strrchr(APP, '/') + 1, p, 8); return n ? p[n - 1] : 0; }
static int window_id(char *out, size_t cap) {      /* the window of the app: size 1260x556 by default (windows carry no X name); env BOARD_CTL_SIZE overrides */
    const char *sz = getenv("BOARD_CTL_SIZE"); if (!sz) sz = "1260x556"; char cmd[256], b[16384]; snprintf(cmd, sizeof cmd, "xwininfo -root -tree 2>/dev/null | grep ' %s+'", sz);
    if (run(cmd, b, sizeof b) != 0 || !b[0]) return 0; char *s = b; while (*s == ' ') s++; char *e = strchr(s, ' '); if (!e) return 0; *e = 0; snprintf(out, cap, "%s", s); return 1;
}
static void nudge(void) { char p[PATH_MAX + 64]; snprintf(p, sizeof p, "%s/pieces/apps/player_app/interact_relay.txt", SESS); FILE *f = fopen(p, "a"); if (f) { fputs("126\n", f); fclose(f); } }
static long frame_time(void) { char p[PATH_MAX + 64]; snprintf(p, sizeof p, "%s/pieces/display/rgb_frame.raw", SESS); return mtime_ms(p); }

static void state_get(const char *spec, char *val, size_t cap);
static int do_status(int quiet) {
    int bad = 0; char b[4096], p[PATH_MAX + 80];
    if (!load_session()) { printf("FAIL session: no bv_session in board/state/ui.txt (no engine session)\n"); return 1; }
    if (!exists(SESS)) { printf("FAIL session: folder is gone: %s (the window shows its last frame and ignores keys: run recover)\n", SESS); bad = 1; } else if (!quiet) printf("ok   session: %s\n", SESS);
    int del = 0, ep = engine_pid(&del); if (!ep) { printf("FAIL engine: no bv_render_3d --daemon process\n"); bad = 1; } else if (del) { printf("FAIL engine: pid %d runs in a deleted session folder\n", ep); bad = 1; } else printf("ok   engine: pid %d\n", ep);
    snprintf(p, sizeof p, "%s/pieces/system/bv_state.txt", SESS); slurp(p, b, sizeof b);
    { const char *need[3] = { "focused_project_id=", "focused_project_root=", "emoji_mode=" }; for (int i = 0; i < 3; i++) if (!strstr(b, need[i])) { printf("FAIL bv_state: line %s missing (keys are ignored, 'No project focused yet')\n", need[i]); bad = 1; } }
    int wp = window_pid(); char wid[64]; if (!wp) { printf("FAIL window: no khtpm_core_render for %s\n", strrchr(APP, '/') + 1); bad = 1; } else if (!window_id(wid, sizeof wid)) { printf("FAIL window: pid %d but no X window of the expected size (BOARD_CTL_SIZE)\n", wp); bad = 1; } else { char mc[160], mb[256]; snprintf(mc, sizeof mc, "xwininfo -id %s 2>/dev/null | grep 'Map State'", wid); run(mc, mb, sizeof mb);
        if (!strstr(mb, "IsViewable")) { printf("FAIL window: pid %d id %s is not mapped (%s): keys cannot be focused and dumps are not live; show the window (xdotool windowmap %s)\n", wp, wid, mb[0] ? strchr(mb, 'I') ? strchr(mb, 'I') : mb : "no Map State", wid); bad = 1; }
        else printf("ok   window: pid %d id %s (mapped)\n", wp, wid); }
    snprintf(p, sizeof p, "%s/state/daemon.lock", APP); int fd = open(p, O_RDWR); if (fd < 0) { printf("FAIL daemon: no state/daemon.lock\n"); bad = 1; } else { if (flock(fd, LOCK_EX | LOCK_NB) == 0) { printf("FAIL daemon: not running (nobody holds state/daemon.lock)\n"); bad = 1; } else printf("ok   daemon: running\n"); close(fd); }
    { char iv[64], tb[32], tp[PATH_MAX + 80]; state_get("flag_int.txt", iv, sizeof iv); snprintf(tp, sizeof tp, "%s/pieces/display/active_gui_is_typing.txt", SESS); slurp(tp, tb, sizeof tb);      /* In: ON must mean the engine is in Interact mode, or the arrow keys go nowhere */
      if (iv[0] == '1' && atoi(tb) != 1) { printf("FAIL In: flag_int=1 but the engine is not in Interact mode (active_gui_is_typing=%d): arrows and Enter are not taken; toggle the int verb off and on\n", atoi(tb)); bad = 1; }
      else if (iv[0] == '1') printf("ok   In: ON and the engine is in Interact mode\n"); else printf("note In: is off (flag_int=%s)\n", iv); }
    return bad;
}
static int do_preflight(void) {
    int bad = do_status(0); if (bad) { printf("PREFLIGHT FAIL (control lost; run: board_ctl recover %s)\n", APP); return 1; }
    long t0 = frame_time(); nudge(); long until = now_ms() + 10000; while (now_ms() < until) { if (frame_time() != t0) { printf("ok   render: the engine rendered after a nudge\n"); printf("PREFLIGHT OK\n"); return 0; } usleep(300000); }
    printf("FAIL render: no new frame within 10 s of a nudge (engine hung or session stale)\nPREFLIGHT FAIL\n"); return 1;
}
static int do_recover(void) {
    char cmd[PATH_MAX * 3]; int pids[16], n; load_session();
    n = find_pids("khtpm_core_render", strrchr(APP, '/') + 1, pids, 16); for (int i = 0; i < n; i++) kill(pids[i], SIGTERM);
    n = find_pids("bv_render_3d", "--daemon", pids, 16); for (int i = 0; i < n; i++) kill(pids[i], SIGTERM);
    n = find_pids("pchq_board_projector", NULL, pids, 16); for (int i = 0; i < n; i++) kill(pids[i], SIGTERM);
    sleep(3); snprintf(cmd, sizeof cmd, "sh '%s/button.sh' run >/dev/null 2>&1", APP); run(cmd, NULL, 0);
    printf("recover: launched %s/button.sh, waiting for a session (up to 90 s)\n", APP); long until = now_ms() + 90000; int started_engine = 0;
    while (now_ms() < until) { sleep(3); if (load_session() && exists(SESS)) { int del; if (engine_pid(&del) && !del) { printf("recover: session %s is up\n", SESS); return do_preflight(); } }
        if (!started_engine && now_ms() > until - 45000) { int lp[8]; if (find_pids("piececraft-hq/button.sh", NULL, lp, 8) > 0 || find_pids("open_pchq_board", NULL, lp, 8) > 0) { printf("recover: an engine launch is still running, not starting a second one (two starts kill each other's session)\n"); started_engine = 1; continue; } started_engine = 1; snprintf(cmd, sizeof cmd, "cd '%s/@.apps/piececraft-hq' && (PCHQ_ENGINE_MODE=1 PCHQ_PKG='%s/rpg-space-pchq' setsid nohup sh button.sh run >/tmp/board_ctl_engine.log 2>&1 &)", HOUSE, APP); run(cmd, NULL, 0); printf("recover: no engine yet, started the piececraft engine by hand\n"); } }
    printf("recover: FAILED, no live session after 90 s (see /tmp/rpg-space-board.log, /tmp/board_ctl_engine.log)\n"); return 1;
}
static int do_key(const char *key, int ms) {
    char wid[64], cmd[256]; if (!window_id(wid, sizeof wid)) { printf("FAIL key: no window\n"); return 1; }
    snprintf(cmd, sizeof cmd, "xdotool mousemove --window %s 400 350 2>/dev/null; xdotool windowfocus %s 2>/dev/null && sleep 0.3 && xdotool key %s", wid, wid, key); if (run(cmd, NULL, 0) != 0) { printf("FAIL key: xdotool could not focus %s (window unmapped or gone: board_ctl status)\n", wid); return 1; } if (load_session()) for (int i = 0; i < 3; i++) { usleep(700000); nudge(); }      /* an idle engine only reads its relay when woken: 126 is the unbound "render now" key (without it a key took 30 s to show) */
    usleep((useconds_t)ms * 1000); printf("key %s sent to %s\n", key, wid); return 0;
}
static int do_click(int x, int y) {
    int wp = window_pid(); if (!wp) { printf("FAIL click: no window\n"); return 1; } char p[PATH_MAX + 64]; snprintf(p, sizeof p, "%s/#.desktop/entity_menu_history/%d.txt", HOUSE, wp);
    { char wid[64], mc[160]; if (window_id(wid, sizeof wid)) { snprintf(mc, sizeof mc, "xdotool mousemove --window %s %d %d 2>/dev/null; xdotool windowfocus %s 2>/dev/null", wid, x, y, wid); run(mc, NULL, 0); usleep(500000); } }      /* the pointer must be on the element too: a relay click with the pointer elsewhere did not press the popup */
    FILE *f = fopen(p, "a"); if (!f) { printf("FAIL click: cannot append to %s\n", p); return 1; } fprintf(f, "MOUSE_EVENT: 1 %d %d 1\n", x, y); fflush(f); usleep(300000); fprintf(f, "MOUSE_EVENT: 1 %d %d 0\n", x, y); fclose(f); printf("click %d,%d through %s\n", x, y, p); return 0;
}
static int kv_write(const char *path, const char *key, const char *val) {      /* set one key=value in a kv file, every other line kept, tmp + rename */
    char b[8192]; slurp(path, b, sizeof b); char tmp[PATH_MAX + 32]; snprintf(tmp, sizeof tmp, "%s.bc.%d", path, (int)getpid()); FILE *o = fopen(tmp, "w"); if (!o) return 0; int done = 0; size_t kl = strlen(key);
    for (char *l = strtok(b, "\n"); l; l = strtok(NULL, "\n")) { if (!strncmp(l, key, kl) && l[kl] == '=') { fprintf(o, "%s=%s\n", key, val); done = 1; } else fprintf(o, "%s\n", l); }
    if (!done) fprintf(o, "%s=%s\n", key, val); fclose(o); return rename(tmp, path) == 0;
}
static int do_camera(const char *name) {
    if (!load_session() || !exists(SESS)) { printf("FAIL camera: no live session\n"); return 1; }
    char p[PATH_MAX + 64], b[8192]; snprintf(p, sizeof p, "%s/camera_presets.pdl", HERE); slurp(p, b, sizeof b); int hit = 0, want_z = -1;
    char *sv1 = NULL; for (char *l = strtok_r(b, "\n", &sv1); l; l = strtok_r(NULL, "\n", &sv1)) { char *f[12]; int k = 0; if (l[0] == '#') continue; char *sv2 = NULL; for (char *t = strtok_r(l, "|", &sv2); t && k < 12; t = strtok_r(NULL, "|", &sv2)) { while (*t == ' ') t++; char *e = t + strlen(t); while (e > t && e[-1] == ' ') *--e = 0; f[k++] = t; }
        if (k < 10 || strcmp(f[0], "CAM") || strcmp(f[1], name)) continue; char sp[PATH_MAX + 64]; snprintf(sp, sizeof sp, "%s/pieces/system/bv_state.txt", SESS);
        const char *keys[8] = { "camera_mode", "cam_yaw", "cam_pitch", "cam_pan_x", "cam_pan_y", "cam_pan_z", "cam_z_level", "current_z" };
        for (int i = 0; i < 8; i++) kv_write(sp, keys[i], f[2 + i]); hit = 1; want_z = atoi(f[9]); }
    if (!hit) { printf("FAIL camera: no preset '%s' in %s\n", name, p); return 1; } nudge(); usleep(500000); nudge();
    if (want_z >= 0) {      /* the engine puts the xelector level back (to -7 after a book reload) and the camera follows it: bring the level to the preset's with real z / x keys */
        char fp[PATH_MAX + 80], fb[4096]; usleep(2500000); nudge(); usleep(2500000); snprintf(fp, sizeof fp, "%s/pieces/display/current_frame.txt", SESS); slurp(fp, fb, sizeof fb); char *zl = strstr(fb, "Z-level:"); int cur = zl ? atoi(zl + 8) : want_z;
        if (cur != want_z) { char rp[PATH_MAX + 80]; snprintf(rp, sizeof rp, "%s/pieces/apps/player_app/interact_relay.txt", SESS); FILE *rf = fopen(rp, "a"); if (rf) { int n = cur < want_z ? want_z - cur : cur - want_z; for (int i = 0; i < n; i++) { fprintf(rf, "%d\n", cur < want_z ? 'x' : 'z'); fflush(rf); usleep(120000); } fprintf(rf, "126\n"); fclose(rf); } printf("camera: level %d -> %d (%s)\n", cur, want_z, cur < want_z ? "x" : "z"); usleep(2000000); nudge(); } }
    printf("camera %s set (absolute) and nudged\n", name); return 0;
}
static void state_get(const char *spec, char *val, size_t cap) {      /* "ship.txt" = first line of <app>/state/ship.txt, "trainer.txt:x" = the value of the line x= */
    char fn[128], fp[PATH_MAX + 64], fb[2048]; snprintf(fn, sizeof fn, "%s", spec); char *key = strchr(fn, ':'); if (key) *key++ = 0; snprintf(fp, sizeof fp, "%s/state/%s", APP, fn); slurp(fp, fb, sizeof fb); val[0] = 0;
    if (key) { size_t kl = strlen(key); char *sv = NULL; for (char *l = strtok_r(fb, "\n", &sv); l; l = strtok_r(NULL, "\n", &sv)) if (!strncmp(l, key, kl) && l[kl] == '=') snprintf(val, cap, "%s", l + kl + 1); } else { char *nl = strchr(fb, '\n'); if (nl) *nl = 0; snprintf(val, cap, "%s", fb); }
}
static int do_expect(const char *spec, const char *want, int wait_s) {      /* exit 0 when the state equals want (within wait_s seconds), else 4 with what it is */
    char v[512]; long until = now_ms() + wait_s * 1000L; for (;;) { state_get(spec, v, sizeof v); if (!strcmp(v, want)) { printf("ok   %s = %s\n", spec, v); return 0; } if (now_ms() >= until) break; usleep(500000); }
    printf("FAIL expect: %s is '%s', wanted '%s'\n", spec, v, want); return 4;
}
static int do_snap(const char *out, int same_ok, const char *states) {
    if (!load_session() || !exists(SESS)) { printf("FAIL snap: no live session (run preflight)\n"); return 1; }
    long t0 = frame_time(); nudge(); long until = now_ms() + 12000; int fresh = 0; while (now_ms() < until) { if (frame_time() != t0) { fresh = 1; break; } usleep(300000); }
    if (!fresh) { printf("FAIL snap: the engine did not render after the nudge (stale): no picture written\n"); return 1; } usleep(1500000);
    char wid[64], cmd[PATH_MAX * 2]; if (!window_id(wid, sizeof wid)) { printf("FAIL snap: no window\n"); return 1; }
    snprintf(cmd, sizeof cmd, "'%s/&.widgits/_shared-lib/ops/+x/dump_frame_png_op.+x' %s '%s' 2>/dev/null", HOUSE, wid, out); if (run(cmd, NULL, 0) != 0 || !exists(out)) { printf("FAIL snap: dump_frame_png_op failed\n"); return 1; }
    unsigned long long h = fnv_file(out); char lp[PATH_MAX + 64], lb[64]; snprintf(lp, sizeof lp, "%s/state/.board_ctl_last_snap", APP); slurp(lp, lb, sizeof lb); int same = strtoull(lb, NULL, 16) == h;
    { FILE *f = fopen(lp, "w"); if (f) { fprintf(f, "%llx\n", h); fclose(f); } }
    char rp[PATH_MAX + 32]; snprintf(rp, sizeof rp, "%s.receipt.txt", out); FILE *r = fopen(rp, "w"); if (r) { fprintf(r, "snap=%s\nhash=%llx\nsession=%s\nframe_ms=%ld\nsame_as_previous=%d\n", out, h, SESS, frame_time(), same);
        if (states) { char sb[1024]; snprintf(sb, sizeof sb, "%s", states); char *sv = NULL; for (char *t = strtok_r(sb, ",", &sv); t; t = strtok_r(NULL, ",", &sv)) { char val[512]; state_get(t, val, sizeof val); fprintf(r, "state.%s=%s\n", t, val); } } fclose(r); }
    printf("snap %s hash=%llx%s\n", out, h, same ? " SAME AS PREVIOUS" : ""); if (same && !same_ok) { printf("FAIL snap: identical to the previous snapshot (stale or nothing changed); pass --same-ok if that is expected\n"); return 3; } return 0;
}
static int do_manifest(const char *dir, const char *steps) {
    char sp[PATH_MAX + 32], b[16384], mp[PATH_MAX + 32]; slurp(steps, b, sizeof b); snprintf(mp, sizeof mp, "%s/manifest.txt", dir); FILE *m = fopen(mp, "w"); if (!m) { printf("FAIL manifest: cannot write %s\n", mp); return 1; }
    fprintf(m, "# generated by board_ctl manifest from %s; every {state} value comes from the snapshot's receipt\n", steps); char prev[64] = ""; int bad = 0;
    for (char *l = strtok(b, "\n"); l; l = strtok(NULL, "\n")) { if (l[0] == '#' || !l[0]) continue; char *b1 = strchr(l, '|'); char *b2 = b1 ? strchr(b1 + 1, '|') : NULL; if (!b2) continue; *b1 = 0; *b2 = 0; char *snap = l, *hold = b1 + 1, *cap = b2 + 1;
        while (*snap == ' ') snap++; char *e = snap + strlen(snap); while (e > snap && e[-1] == ' ') *--e = 0; snprintf(sp, sizeof sp, "%s/snapshots/%s.receipt.txt", dir, snap); char rb[2048]; slurp(sp, rb, sizeof rb);
        if (!rb[0]) { printf("FAIL manifest: no receipt for %s (take it with board_ctl snap)\n", snap); bad = 1; continue; } char hash[64] = ""; char *hp = strstr(rb, "hash="); if (hp) sscanf(hp + 5, "%63[^\n]", hash);
        if (prev[0] && !strcmp(prev, hash)) { printf("FAIL manifest: %s is identical to the snapshot before it\n", snap); bad = 1; } snprintf(prev, sizeof prev, "%s", hash);
        char out[2048] = ""; size_t o = 0; for (const char *c = cap; *c && o < sizeof out - 80; ) { if (*c == '{') { const char *q = strchr(c, '}'); if (q) { char name[80]; size_t nl = (size_t)(q - c - 1); if (nl >= sizeof name) nl = sizeof name - 1; memcpy(name, c + 1, nl); name[nl] = 0; char key[96]; snprintf(key, sizeof key, "state.%s=", name); char *v = strstr(rb, key);
                    if (!v) { printf("FAIL manifest: %s has no state '%s' in its receipt\n", snap, name); bad = 1; } else { v += strlen(key); while (*v && *v != '\n' && o < sizeof out - 4) out[o++] = *v++; } c = q + 1; continue; } } out[o++] = *c++; } out[o] = 0;
        fprintf(m, "%s | %s | %s\n", snap, hold, out); }
    fclose(m); printf("%s manifest %s\n", bad ? "FAIL" : "ok", mp); return bad;
}
static int do_camset(int argc, char **argv) {      /* camset <app> key=value ...: write the camera lines of the session's bv_state.txt directly (cam_yaw, cam_pitch, cam_pan_x/y/z, cam_z_level, camera_mode, current_z), then a redraw nudge; camset <app> show prints them. The values are what the engine reads: pan is relative to the desk's home framing (pieces/system/cam_home.txt). */
    if (!load_session() || !exists(SESS)) { printf("FAIL camset: no live session\n"); return 1; }
    char sp[PATH_MAX + 64], b[8192]; snprintf(sp, sizeof sp, "%s/pieces/system/bv_state.txt", SESS); int shown = 0;
    for (int i = 3; i < argc; i++) { if (!strcmp(argv[i], "show")) { shown = 1; continue; } char *eq = strchr(argv[i], '='); if (!eq) { printf("FAIL camset: %s is not key=value\n", argv[i]); return 1; } char k[64]; snprintf(k, sizeof k, "%.*s", (int)(eq - argv[i]), argv[i]);
        if (strncmp(k, "cam_", 4) && strcmp(k, "camera_mode") && strcmp(k, "current_z")) { printf("FAIL camset: %s is not a camera key\n", k); return 1; } kv_write(sp, k, eq + 1); }
    if (argc > 3 && !shown) { nudge(); usleep(500000); nudge(); }
    slurp(sp, b, sizeof b); for (char *l = strtok(b, "\n"); l; l = strtok(NULL, "\n")) if (!strncmp(l, "cam_", 4) || !strncmp(l, "camera_mode", 11) || !strncmp(l, "current_z", 9)) printf("%s\n", l);
    return 0;
}
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: board_ctl status|preflight|recover|key|click|camera|snap <app dir> ...   |   manifest <dir> <steps.txt>   (see the header of tools/board_ctl.c)\n"); return 2; }
    char self[PATH_MAX]; ssize_t r = readlink("/proc/self/exe", self, sizeof self - 1); if (r > 0) { self[r] = 0; char *s = strrchr(self, '/'); if (s) *s = 0; s = strrchr(self, '/'); if (s) *s = 0; snprintf(HERE, sizeof HERE, "%s", self); }      /* tools/+x/board_ctl.+x -> .../tools */
    const char *cmd = argv[1]; if (!strcmp(cmd, "manifest")) return argc >= 4 ? do_manifest(argv[2], argv[3]) : 2;
    if (!realpath(argv[2], APP)) { fprintf(stderr, "board_ctl: no such app dir %s\n", argv[2]); return 2; }
    snprintf(HOUSE, sizeof HOUSE, "%s", APP); { char *s = strrchr(HOUSE, '/'); if (s) *s = 0; s = strrchr(HOUSE, '/'); if (s) *s = 0; }      /* <house>/@.apps/<app> */
    load_session();
    if (!strcmp(cmd, "status")) return do_status(0);
    if (!strcmp(cmd, "preflight")) return do_preflight();
    if (!strcmp(cmd, "recover")) return do_recover();
    if (!strcmp(cmd, "key") && argc >= 4) return do_key(argv[3], argc >= 5 ? atoi(argv[4]) : 1500);
    if (!strcmp(cmd, "click") && argc >= 5) return do_click(atoi(argv[3]), atoi(argv[4]));
    if (!strcmp(cmd, "camera") && argc >= 4) return do_camera(argv[3]);
    if (!strcmp(cmd, "camset") && argc >= 4) return do_camset(argc, argv);
    if (!strcmp(cmd, "expect") && argc >= 5) return do_expect(argv[3], argv[4], argc >= 6 ? atoi(argv[5]) : 0);
    if (!strcmp(cmd, "snap") && argc >= 4) { int same_ok = 0; const char *states = NULL; for (int i = 4; i < argc; i++) { if (!strcmp(argv[i], "--same-ok")) same_ok = 1; else if (!strcmp(argv[i], "--state") && i + 1 < argc) states = argv[++i]; } return do_snap(argv[3], same_ok, states); }
    fprintf(stderr, "board_ctl: bad command or arguments\n"); return 2;
}
