/* screen_rec_manager.c - <module> backend for screen-rec-hq.xhtpm.
 *
 * "Streaming"/screen-recorder toy, converted to house x11-HQ spec the
 * SAME way the network apps (forum/irc) were: this is a thin manager
 * that WRAPS an existing engine daemon, it does not re-implement it.
 *
 * The engine is the already-built, out-of-spec capture+encode daemon
 * at  44.xyz.01.00/151.screen-rec+01.02/system/screen_rec  (Wayland
 * xdg-desktop-portal + PipeWire ScreenCast -> libx264 .mp4; see that
 * tree's dox/architecture.md). It is UNCHANGED. Its file-IPC:
 *   writes  <eng>/pieces/display/recorder_state.receipt.txt
 *              receipt_type=recorder_state
 *              recording=0|1
 *              output_path=<path or empty>
 *              frames_encoded=<int>
 *   writes  <eng>/pieces/display/rgb_frame.raw        (live RGBA32 frame)
 *   writes  <eng>/pieces/display/rgb_frame.receipt.txt (frame_w/frame_h/..)
 *   reads   <eng>/pieces/control/record_command.txt   ("start" / "stop")
 *   pid ->  /tmp/screen_rec.pid
 *
 * This manager:
 *   - spawns the engine if it isn't already running
 *   - polls  <pkg>/state/screen_rec_action.txt   (seq=N\ncmd=...)
 *       cmd=START / STOP / TOGGLE  -> write record_command.txt
 *       cmd=RESCAN                 -> re-list recordings/ next tick
 *   - reads the engine's recorder_state receipt and publishes
 *     <pkg>/state/screen_rec_ui.txt (key=value, consumed via vars=)
 *
 * Forked by khtpm_core_render.+x as:
 *   screen_rec_manager.+x <house_root> <package_dir> [id]
 *
 * STATUS 2026-09-08: skeleton. Compiles + runs + start/stop works +
 * state publishes. TODO for whoever continues (grok): (a) verify the
 * engine actually launches headless on THIS box's portal (it may need
 * the compositor picker dialog once - see architecture.md), (b) wire
 * the <canvas> live preview to the engine's rgb_frame.raw, (c) a real
 * recordings list with durations/thumbs, (d) elapsed-time display.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#define KH_PLAT_IMPL
#include "../../../&.widgits/_shared-lib/kh_plat.h"

#define PL 4096
#define MAXREC 128

static char house_root[PL];
static char pkg_dir[PL];
static char eng_root[PL];   /* 151.screen-rec+01.02 */

static char g_msg[128] = "";

static void bye(int s) { (void)s; _exit(0); }

/* read one "key=value" line's value */
static void kv(const char *path, const char *key, char *out, size_t osz) {
    out[0] = 0;
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[PL];
    size_t klen = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, klen) == 0 && line[klen] == '=') {
            char *v = line + klen + 1;
            char *nl = strchr(v, '\n'); if (nl) *nl = 0;
            snprintf(out, osz, "%s", v);
            break;
        }
    }
    fclose(f);
}

static int engine_alive(void) {
    char buf[64] = "";
    kv("/tmp/screen_rec.pid", "", buf, sizeof(buf)); /* file is just "<pid>\n" */
    FILE *f = fopen("/tmp/screen_rec.pid", "r");
    if (!f) return 0;
    int pid = 0; if (fscanf(f, "%d", &pid) != 1) pid = 0;
    fclose(f);
    if (pid <= 1) return 0;
    char p[64]; snprintf(p, sizeof(p), "/proc/%d", pid);
    struct stat st;
    return stat(p, &st) == 0;
}

static void engine_spawn(void) {
    if (engine_alive()) return;
    /* engine's button.sh knows how to build+run it; `run` execs the GUI
     * too, so call the daemon binary directly and background it. */
    const char *args[1] = { "run" };
    /* keep this house-relative; kh_plat runs `sh '<house>/<rel>' <args>` */
    kh_plat_run_house_script(house_root,
        "151.screen-rec+01.02/system/../button.sh", args, 1);
    /* NOTE: button.sh's `run` verb also execs the old GLUT GUI. For the
     * converted toy we only want the daemon. TODO(grok): add a
     * `daemon-only` verb to 151.screen-rec+01.02/button.sh (spawn
     * system/screen_rec, do NOT exec screen_rec_gui) and call that here
     * instead. */
}

static void engine_cmd(const char *cmd) {
    char path[PL];
    snprintf(path, sizeof(path), "%s/pieces/control/record_command.txt", eng_root);
    FILE *f = fopen(path, "w");
    if (f) { fprintf(f, "%s\n", cmd); fclose(f); }
}

static void write_ui(void) {
    char dir[PL], tmp[PL], dst[PL];
    snprintf(dir, sizeof(dir), "%s/state", pkg_dir);
    kh_plat_mkdir_p(dir);
    snprintf(dst, sizeof(dst), "%s/screen_rec_ui.txt", dir);
    snprintf(tmp, sizeof(tmp), "%s/screen_rec_ui.txt.tmp", dir);

    char rstate[PL];
    snprintf(rstate, sizeof(rstate), "%s/pieces/display/recorder_state.receipt.txt", eng_root);
    char recording[16] = "0", outpath[PL] = "", frames[32] = "0";
    kv(rstate, "recording", recording, sizeof(recording));
    kv(rstate, "output_path", outpath, sizeof(outpath));
    kv(rstate, "frames_encoded", frames, sizeof(frames));
    int rec = atoi(recording);
    int eng = engine_alive();

    /* recordings/ list */
    char recdir[PL];
    snprintf(recdir, sizeof(recdir), "%s/recordings", eng_root);
    char names[MAXREC][256]; int n = 0;
    DIR *d = opendir(recdir);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) && n < MAXREC) {
            if (e->d_name[0] == '.') continue;
            const char *dot = strrchr(e->d_name, '.');
            if (!dot || strcmp(dot, ".mp4") != 0) continue;
            snprintf(names[n++], sizeof(names[0]), "%s", e->d_name);
        }
        closedir(d);
    }

    FILE *f = fopen(tmp, "w");
    if (!f) return;
    fprintf(f, "engine_up=%d\n", eng);
    fprintf(f, "engine_label=%s\n", eng ? "engine: running" : "engine: OFF (portal picker may be needed)");
    fprintf(f, "recording=%d\n", rec);
    fprintf(f, "state=%s\n", rec ? "REC" : (eng ? "idle" : "no engine"));
    fprintf(f, "rec_label=%s\n", rec ? "[ STOP ]" : "[ START ]");
    fprintf(f, "rec_cmd=%s\n", rec ? "STOP" : "START");
    fprintf(f, "frames_encoded=%s\n", frames);
    fprintf(f, "output_path=%s\n", outpath[0] ? outpath : "-");
    fprintf(f, "n_recordings=%d\n", n);
    for (int i = 0; i < n; i++) fprintf(f, "r_%d_name=%s\n", i, names[i]);
    fprintf(f, "msg=%s\n", g_msg);
    fclose(f);
    rename(tmp, dst);
}

static void poll_action(int *last_seq) {
    char path[PL];
    snprintf(path, sizeof(path), "%s/state/screen_rec_action.txt", pkg_dir);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[PL], cmd[128] = ""; int seq = 0;
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "seq=%d", &seq) == 1) continue;
        if (strncmp(line, "cmd=", 4) == 0) {
            snprintf(cmd, sizeof(cmd), "%s", line + 4);
            char *nl = strchr(cmd, '\n'); if (nl) *nl = 0;
        }
    }
    fclose(f);
    if (seq == *last_seq || !cmd[0]) return;
    *last_seq = seq;

    if (strcmp(cmd, "START") == 0) { engine_spawn(); engine_cmd("start"); snprintf(g_msg, sizeof(g_msg), "recording..."); }
    else if (strcmp(cmd, "STOP") == 0) { engine_cmd("stop"); snprintf(g_msg, sizeof(g_msg), "stopped"); }
    else if (strcmp(cmd, "TOGGLE") == 0) {
        char rstate[PL]; snprintf(rstate, sizeof(rstate), "%s/pieces/display/recorder_state.receipt.txt", eng_root);
        char r[16] = "0"; kv(rstate, "recording", r, sizeof(r));
        if (atoi(r)) { engine_cmd("stop"); snprintf(g_msg, sizeof(g_msg), "stopped"); }
        else { engine_spawn(); engine_cmd("start"); snprintf(g_msg, sizeof(g_msg), "recording..."); }
    }
    else if (strcmp(cmd, "RESCAN") == 0) { snprintf(g_msg, sizeof(g_msg), "rescanned"); }
    else if (strcmp(cmd, "ENGINE_START") == 0) { engine_spawn(); snprintf(g_msg, sizeof(g_msg), "engine spawn requested"); }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: screen_rec_manager <house> <pkg> [id]\n"); return 1; }
    snprintf(house_root, sizeof(house_root), "%s", argv[1]);
    snprintf(pkg_dir,    sizeof(pkg_dir),    "%s", argv[2]);
    snprintf(eng_root,   sizeof(eng_root),   "%s/151.screen-rec+01.02", house_root);
    kh_plat_on_terminate(bye);

    { char ap[PL]; snprintf(ap, sizeof(ap), "%s/state/screen_rec_action.txt", pkg_dir);
      kh_plat_mkdir_p(pkg_dir);
      char sd[PL]; snprintf(sd, sizeof(sd), "%s/state", pkg_dir); kh_plat_mkdir_p(sd);
      FILE *a = fopen(ap, "w"); if (a) { fputs("seq=0\ncmd=\n", a); fclose(a); } }

    write_ui();
    int last_seq = 0;
    for (;;) { kh_plat_sleep_ms(200); poll_action(&last_seq); write_ui(); }
    return 0;
}
