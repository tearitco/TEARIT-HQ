/* mr_clock_common.h - shared helpers for the Task 1 message ops
 * (2026-08-29). Implements the gap-#0 design decision from
 * CURSWORD-SOUL-VISION.md §4 / GAME-READINESS-GAP-ANALYSIS-2026-08-27.md:
 * a blocking message box in continuous mode pauses the EXISTING game
 * clock via the real mechanism (lc_clock `ticker <id> off|on`, which
 * flips `running` in <house>/#.desktop/clocks/<id>.pdl), explicitly NOT
 * a new message-box-specific pause flag. common_events_manager.c's own
 * tick loop reads the same `running` state and skips Parallel/Autorun
 * fires while a game clock is paused. Also carries the shared
 * STATE_DIR (variables.txt) resolution mirroring the compiler's
 * resolve_session_root(), and the key=value set helper (mr_change_gold
 * pattern). Standalone - each op compiles it in, like every other C
 * op in this directory. */
#ifndef MR_CLOCK_COMMON_H
#define MR_CLOCK_COMMON_H

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#define MR_PATH_BUF 4352
#define MR_MAX_CLOCKS 16
#define MR_MAX_KEEP 128
#define MR_POLL_INTERVAL_US 250000
#define MR_POLL_TIMEOUT_S 30

/* Shared-header helpers are static-per-op (kept in the header so each
 * op compiles standalone like every other C op here); an op that does
 * not use a given helper must not trip -Wall's unused-function. */
#if defined(__GNUC__)
#define MR_UNUSED __attribute__((unused))
#else
#define MR_UNUSED
#endif

/* Real relay + result-file popup machinery idents (tp_desktop_window_rgb.c). */
#define ROW_OBJECTS_TMP ".mr_objects.tmp.pdl"
#define ROW_RESULT_TMP ".mr_result.tmp.txt"

/* Audit to <package_dir>/messages.txt + history.txt, same ledger habit
 * as mr_show_text/mr_show_choices. */
static MR_UNUSED void mr_log(const char *package_dir, const char *fmt, ...) {
    char msg_path[MR_PATH_BUF], hist_path[MR_PATH_BUF], line[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    snprintf(msg_path, sizeof(msg_path), "%s/messages.txt", package_dir);
    FILE *mf = fopen(msg_path, "a");
    if (mf) { fprintf(mf, "[%ld] %s\n", (long)time(NULL), line); fclose(mf); }
    snprintf(hist_path, sizeof(hist_path), "%s/history.txt", package_dir);
    FILE *hf = fopen(hist_path, "a");
    if (hf) { fprintf(hf, "%s\n", line); fclose(hf); }
}

/* Where should the op send a PLAYER-VISIBLE popup? Mirrors
 * mr_show_choices.c: a common event's compiled cmd_N.sh runs with its
 * OWN directory as $ENT, but play_event.sh exports MUCHI_CALLER_PKG=<the
 * real playing entity's event_pkg> so relay writes land on the window
 * that is actually on the player's screen. */
static MR_UNUSED const char *mr_popup_target(const char *package_dir) {
    const char *caller = getenv("MUCHI_CALLER_PKG");
    return (caller && caller[0]) ? caller : package_dir;
}

/* STATE_DIR resolution mirroring the compiler's resolve_session_root():
 * walk up from `dir` for a sessions/<id>/ ancestor; else fall back to
 * `dir` itself (non-session entities get per-entity state - the A6 note
 * in the handoff doc names this an acceptable per-entity degradation). */
static MR_UNUSED void mr_state_root(const char *dir, char *out, size_t outsz) {
    char walk[MR_PATH_BUF];
    snprintf(walk, sizeof(walk), "%s", dir);
    while (walk[0] && strcmp(walk, "/") != 0) {
        char *bs = strrchr(walk, '/');
        if (!bs || bs == walk) break;
        char parent[MR_PATH_BUF];
        size_t plen = (size_t)(bs - walk);
        if (plen >= sizeof(parent)) break;
        memcpy(parent, walk, plen);
        parent[plen] = '\0';
        char *pp = strrchr(parent, '/');
        if (pp) {
            if (strcmp(pp + 1, "sessions") == 0) { snprintf(out, outsz, "%s", walk); return; }
        } else if (strcmp(parent, "sessions") == 0) {
            snprintf(out, outsz, "%s", walk); return;
        }
        *bs = '\0';
    }
    snprintf(out, outsz, "%s", dir);
}

/* Real key=value set: rewrite the file preserving every other line
 * (mr_change_gold.c pattern - state keeps unknown future lines). */
static MR_UNUSED void mr_kv_set(const char *path, const char *key, const char *value) {
    char keep[MR_MAX_KEEP][512];
    int n_keep = 0;
    FILE *rf = fopen(path, "r");
    if (rf) {
        char line[512];
        size_t key_len = strlen(key);
        while (n_keep < MR_MAX_KEEP && fgets(line, sizeof(line), rf)) {
            if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') continue;
            snprintf(keep[n_keep], sizeof(keep[0]), "%s", line);
            n_keep++;
        }
        fclose(rf);
    }
    char tmp[MR_PATH_BUF];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *wf = fopen(tmp, "w");
    if (!wf) return;
    fprintf(wf, "%s=%s\n", key, value);
    for (int i = 0; i < n_keep; i++) fputs(keep[i], wf);
    fclose(wf);
    rename(tmp, path);
}

/* Clock ids from <house>/#.desktop/clocks/clocks.pdl (CLOCK|<id>|...). */
static MR_UNUSED int mr_clock_ids(const char *house, char ids[][128], int max) {
    char p[MR_PATH_BUF];
    snprintf(p, sizeof(p), "%s/#.desktop/clocks/clocks.pdl", house);
    FILE *f = fopen(p, "r");
    if (!f) return 0;
    char line[512];
    int n = 0;
    while (fgets(line, sizeof(line), f) && n < max) {
        if (strncmp(line, "CLOCK|", 6) != 0) continue;
        char *id = line + 6;
        char *bar = strchr(id, '|');
        if (bar) *bar = '\0';
        if (id[0]) snprintf(ids[n++], 128, "%s", id);
    }
    fclose(f);
    return n;
}

/* The real `running` flag from a clock's state file <id>.pdl. */
static MR_UNUSED int mr_clock_running(const char *house, const char *id, int def) {
    char p[MR_PATH_BUF];
    snprintf(p, sizeof(p), "%s/#.desktop/clocks/%s.pdl", house, id);
    FILE *f = fopen(p, "r");
    if (!f) return def;
    char line[512];
    int r = def;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "running=", 8) == 0) { r = atoi(line + 8); break; }
    }
    fclose(f);
    return r;
}

/* shell-out helper (same flock-protected control plane the taskbar's
 * livedesk:clock: rows shell out to - khtpm_taskbar_manager.c). */
static MR_UNUSED void mr_clock_shell(const char *house, const char *id, int on) {
    char lcbin[MR_PATH_BUF];
    snprintf(lcbin, sizeof(lcbin), "%s/&.widgits/livedesk-clock/ops/+x/lc_clock.+x", house);
    if (access(lcbin, X_OK) != 0) return; /* no clock machinery - skip */
    char cmd[MR_PATH_BUF * 2];
    snprintf(cmd, sizeof(cmd), "\"%s\" \"%s\" ticker %s %s >/dev/null 2>&1",
             lcbin, house, id, on ? "on" : "off");
    (void)system(cmd);
}

/* Pause every currently-RUNNING game clock; records in paused[] which
 * ones THIS call flipped (resume must only restore those, never clocks
 * that were already paused). Returns count flipped. Text-verifiable:
 * prints CLOCK_PAUSED/<id> per clock. */
static MR_UNUSED int mr_clock_pause(const char *house, char ids[][128], int n, int paused[]) {
    int np = 0;
    for (int i = 0; i < n && i < MR_MAX_CLOCKS; i++) {
        paused[i] = 0;
        if (mr_clock_running(house, ids[i], 1) == 1) {
            mr_clock_shell(house, ids[i], 0);
            paused[i] = 1;
            np++;
            printf("CLOCK_PAUSED %s\n", ids[i]);
        }
    }
    return np;
}

static MR_UNUSED void mr_clock_resume(const char *house, char ids[][128], int n, const int paused[]) {
    for (int i = 0; i < n && i < MR_MAX_CLOCKS; i++) {
        if (!paused[i]) continue;
        mr_clock_shell(house, ids[i], 1);
        printf("CLOCK_RESUMED %s\n", ids[i]);
    }
}

/* Block-poll the popup result file for up to MR_POLL_TIMEOUT_S.
 * Writes a real flat OBJECT file (load_flat_objects() shape) of `rows`
 * (one showing entry per row, row index = the value a pick returns) to
 * package_dir and fires SHOW_PAGE:<objects>|<result> to the relay, then
 * blocks until the player picks (or a timeout sets picked=-1). */
static MR_UNUSED int mr_popup_pick(const char *package_dir, char rows[][256], int n_rows) {
    char objpath[MR_PATH_BUF], respath[MR_PATH_BUF], relay_path[MR_PATH_BUF];
    snprintf(objpath, sizeof(objpath), "%s/%s", package_dir, ROW_OBJECTS_TMP);
    snprintf(respath, sizeof(respath), "%s/%s", package_dir, ROW_RESULT_TMP);

    if (n_rows <= 0) return -1;
    FILE *of = fopen(objpath, "w");
    if (!of) {
        fprintf(stderr, "mr_popup_pick: cannot write %s\n", objpath);
        return -1;
    }
    for (int i = 0; i < n_rows; i++)
        fprintf(of, "OBJECT | label=%s | action=%d\n", rows[i], i);
    fclose(of);

    remove(respath);
    snprintf(relay_path, sizeof(relay_path), "%s/interact_relay.txt", mr_popup_target(package_dir));
    FILE *rf = fopen(relay_path, "w");
    if (!rf) {
        fprintf(stderr, "mr_popup_pick: cannot write %s\n", relay_path);
        return -1;
    }
    fprintf(rf, "SHOW_PAGE:%s|%s\n", objpath, respath);
    fclose(rf);

    int picked = -1;
    int waited_us = 0;
    while (waited_us < MR_POLL_TIMEOUT_S * 1000000) {
        FILE *check = fopen(respath, "r");
        if (check) {
            char line[64];
            if (fgets(line, sizeof(line), check)) picked = atoi(line);
            fclose(check);
            break;
        }
        usleep(MR_POLL_INTERVAL_US);
        waited_us += MR_POLL_INTERVAL_US;
    }
    remove(objpath);
    remove(respath);
    return picked;
}

#endif /* MR_CLOCK_COMMON_H */