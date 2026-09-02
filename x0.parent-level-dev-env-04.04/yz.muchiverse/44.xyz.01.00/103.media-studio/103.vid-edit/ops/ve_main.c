/* ve_main.c — Muchi Video Editor Phase-1 (iMovie-shaped)
 *
 * freeglut UI + ffmpeg for preview decode / export.
 * Layout: File menu | preview + inspector | multi-track timeline
 * Demo clips auto-loaded from media demo mp4s
 *
 * button.sh r   Esc quit
 */
#define _GNU_SOURCE
#include <GL/freeglut.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <pulse/error.h>
#include <pulse/simple.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "../../shared/media_drop_path.h"
#include "../../shared/chtpm_nav_mock.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int g_win_w = 1440, g_win_h = 860;
#define WIN_W (g_win_w)
#define WIN_H (g_win_h)

#define MENU_H 24
#define TOP_H 52
#define INSPECT_W 200
#define MAX_CLIPS 64
#define MAX_TRACKS 4
#define PREVIEW_W 640
#define PREVIEW_H 360

/* CPU budget — ffmpeg one-frame seek is expensive; never spam it.
 * Play: hold last frame + audio (no per-frame decode).
 * Scrub/pause: at most ~2 ffmpeg spawns per second.
 * UI: ≤20fps play / very low idle. */
#define UI_FPS_PLAY       20
#define UI_FPS_IDLE        8
#define UI_TIMER_PLAY_MS  (1000 / UI_FPS_PLAY)  /* 50ms */
#define UI_TIMER_IDLE_MS  200                   /* paused: wake rarely */
#define PREVIEW_DECODE_GAP_MS 500               /* min ms between system(ffmpeg) */
#define PREVIEW_CACHE_MS  400
#define CANVAS_WRITE_MS   2000
#define PLAY_SLEEP_US     5000                  /* 5ms yield while playing */
#define IDLE_SLEEP_US     20000                 /* 20ms idle yield */
#define PREVIEW_DEC_W     320                   /* half-res decode → ~4× less work */
#define PREVIEW_DEC_H     180

typedef struct {
    int used;
    int track;          /* 0=V1 1=V2 2=A1 3=A2 */
    char name[48];
    char path[MEDIA_PATH_MAX];
    int start_ms;       /* timeline position */
    int duration_ms;    /* on timeline (out-in) */
    int in_ms, out_ms;  /* source trim */
    float col[3];
    int is_audio;
} Clip;

static Clip g_clips[MAX_CLIPS];
static int g_n_clips = 0;
static int g_sel = -1;
static int g_playing = 0;
static int g_playhead_ms = 0;
static double g_play_acc = 0;
static int g_file_menu = 0;
static int g_snap_ms = 100;
static volatile int g_quit = 0;
/* timeline clip drag */
static int g_drag_clip = -1;
static int g_drag_grab_ms = 0; /* playhead offset within clip at grab */
static int g_drag_ox = 0, g_drag_oy = 0;
/* XDND */
static Display *g_xdpy = NULL;
static Window g_xwin = 0;
static Atom g_xa_XdndAware, g_xa_XdndEnter, g_xa_XdndPosition, g_xa_XdndStatus;
static Atom g_xa_XdndLeave, g_xa_XdndDrop, g_xa_XdndFinished, g_xa_XdndSelection;
static Atom g_xa_XdndTypeList, g_xa_XdndActionCopy, g_xa_text_uri_list, g_xa_XdndActionPrivate;
static Window g_xdnd_source = 0;
static int g_xdnd_ver = 5;
static int g_xdnd_setup = 0;
static char g_status[256] = "";
static char g_project_root[1024] = ".";
static char g_project_name[64] = "Untitled";
static char g_ffmpeg[256] = "ffmpeg";

/* preview texture */
static GLuint g_tex = 0;
static unsigned char g_preview_rgb[PREVIEW_W * PREVIEW_H * 3];
static int g_preview_valid = 0;
static int g_preview_src_ms = -1;
static int g_preview_clip = -1;
static int g_preview_tex_dirty = 1;   /* only re-upload GL when pixels change */
static double g_last_decode_sec = 0;  /* wall-clock throttle for ffmpeg */
static double g_last_canvas_sec = 0;
static int g_ui_dirty = 1;            /* skip redraw when nothing changed */
static double g_last_ui_sec = 0;
static int g_fps_count = 0;
static double g_fps_window = 0;
static float g_ui_fps = 0;
static int g_glut_ready = 0; /* never glutPostRedisplay before glutInit */
static volatile int g_decode_busy = 0; /* never stack concurrent system(ffmpeg) */
static int g_want_decode = 0;         /* 1 = scrub/stop wants a real frame */

static void mark_ui_dirty(void) {
    g_ui_dirty = 1;
    if (g_glut_ready)
        glutPostRedisplay();
}
static void invalidate_preview(void) {
    /* Request a new frame, but never clear the last good texture while playing
     * (that used to force ffmpeg every tick and melt the CPU). */
    g_want_decode = 1;
    mark_ui_dirty();
}

/* Audio: ffmpeg PCM pipe → PulseAudio (was missing — preview was video-only) */
#define AUD_SR 44100
#define AUD_CH 2
#define AUD_BUF 2048
static pa_simple *g_pa = NULL;
static int g_pa_ok = 0;
static int g_aud_pipe = -1;
static pid_t g_aud_pid = -1;
static int g_aud_clip = -1;
static int g_aud_src_ms = -1;
static pthread_t g_aud_th;
static volatile int g_aud_run = 1;

static void set_playing(int on);
static void audio_init(void);
static void audio_shutdown(void);
static void audio_sync_to_playhead(void);
static void audio_kill_pipe(void);
static void write_canvas_raw_throttled(int force);

static const char *track_names[] = { "V1", "V2", "A1", "A2" };
static const float track_cols[][3] = {
    {0.35f, 0.55f, 0.95f}, {0.55f, 0.40f, 0.90f},
    {0.30f, 0.75f, 0.45f}, {0.85f, 0.55f, 0.25f}
};

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}
static float clampf(float v, float a, float b) {
    return v < a ? a : (v > b ? b : v);
}
static int clampi(int v, int a, int b) {
    return v < a ? a : (v > b ? b : v);
}
static int snap_ms(int t) {
    if (g_snap_ms <= 0) return t < 0 ? 0 : t;
    if (t < 0) t = 0;
    return ((t + g_snap_ms / 2) / g_snap_ms) * g_snap_ms;
}

static int timeline_end_ms(void) {
    int mx = 5000;
    for (int i = 0; i < g_n_clips; i++) {
        if (!g_clips[i].used) continue;
        int e = g_clips[i].start_ms + g_clips[i].duration_ms;
        if (e > mx) mx = e;
    }
    return mx + 1000;
}

static int clip_at_playhead(int video_only) {
    int best = -1, best_track = 99;
    for (int i = 0; i < g_n_clips; i++) {
        Clip *c = &g_clips[i];
        if (!c->used) continue;
        if (video_only && c->is_audio) continue;
        if (g_playhead_ms < c->start_ms) continue;
        if (g_playhead_ms >= c->start_ms + c->duration_ms) continue;
        if (c->track < best_track) { best_track = c->track; best = i; }
    }
    return best;
}

/* ---- I/O ---- */
static void save_project(void) {
    char path[1200];
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/timeline.clips", g_project_root);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "project=%s\nplayhead_ms=%d\nn_clips=%d\n", g_project_name, g_playhead_ms, g_n_clips);
    for (int i = 0; i < g_n_clips; i++) {
        Clip *c = &g_clips[i];
        if (!c->used) continue;
        fprintf(f, "CLIP name=%s track=%d start=%d dur=%d in=%d out=%d audio=%d path=%s\n",
                c->name, c->track, c->start_ms, c->duration_ms, c->in_ms, c->out_ms,
                c->is_audio, c->path);
    }
    fclose(f);
    snprintf(g_status, sizeof(g_status), "Saved '%s'", g_project_name);
}

static void load_project(void) {
    char path[1200], line[1024];
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/timeline.clips", g_project_root);
    FILE *f = fopen(path, "r");
    if (!f) {
        snprintf(g_status, sizeof(g_status), "No timeline.clips to load");
        return;
    }
    g_n_clips = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "project=", 8) == 0) {
            snprintf(g_project_name, sizeof(g_project_name), "%s", line + 8);
            g_project_name[strcspn(g_project_name, "\r\n")] = 0;
        } else if (strncmp(line, "playhead_ms=", 12) == 0) {
            g_playhead_ms = atoi(line + 12);
        } else if (strncmp(line, "CLIP ", 5) == 0) {
            if (g_n_clips >= MAX_CLIPS) continue;
            Clip c = {0};
            c.used = 1;
            char *p = line + 5;
            char *tok;
            while ((tok = strsep(&p, " \n")) != NULL) {
                if (!*tok) continue;
                if (strncmp(tok, "name=", 5) == 0) snprintf(c.name, sizeof(c.name), "%s", tok + 5);
                else if (strncmp(tok, "track=", 6) == 0) c.track = atoi(tok + 6);
                else if (strncmp(tok, "start=", 6) == 0) c.start_ms = atoi(tok + 6);
                else if (strncmp(tok, "dur=", 4) == 0) c.duration_ms = atoi(tok + 4);
                else if (strncmp(tok, "in=", 3) == 0) c.in_ms = atoi(tok + 3);
                else if (strncmp(tok, "out=", 4) == 0) c.out_ms = atoi(tok + 4);
                else if (strncmp(tok, "audio=", 6) == 0) c.is_audio = atoi(tok + 6);
                else if (strncmp(tok, "path=", 5) == 0) snprintf(c.path, sizeof(c.path), "%s", tok + 5);
            }
            int t = clampi(c.track, 0, MAX_TRACKS - 1);
            c.col[0] = track_cols[t][0]; c.col[1] = track_cols[t][1]; c.col[2] = track_cols[t][2];
            g_clips[g_n_clips++] = c;
        }
    }
    fclose(f);
    g_sel = g_n_clips ? 0 : -1;
    invalidate_preview();
    snprintf(g_status, sizeof(g_status), "Loaded '%s' (%d clips)", g_project_name, g_n_clips);
}

/* Quote for /bin/sh — house paths have emoji, &, spaces */
static void shell_quote(const char *in, char *out, size_t out_sz) {
    size_t o = 0;
    if (out_sz < 3) { out[0] = 0; return; }
    out[o++] = '\'';
    for (const char *p = in; *p && o + 5 < out_sz; p++) {
        if (*p == '\'') {
            out[o++] = '\'';
            out[o++] = '\\';
            out[o++] = '\'';
            out[o++] = '\'';
        } else {
            out[o++] = *p;
        }
    }
    out[o++] = '\'';
    out[o] = 0;
}

static void probe_duration_ms(const char *path, int *out_ms) {
    char cmd[MEDIA_PATH_MAX + 256], line[256], q[MEDIA_PATH_MAX + 8];
    shell_quote(path, q, sizeof(q));
    snprintf(cmd, sizeof(cmd),
             "ffprobe -v error -show_entries format=duration -of default=nk=1:nw=1 %s 2>/dev/null",
             q);
    FILE *p = popen(cmd, "r");
    *out_ms = 3000;
    if (!p) return;
    if (fgets(line, sizeof(line), p)) {
        double d = atof(line);
        if (d > 0.1) *out_ms = (int)(d * 1000.0);
    }
    pclose(p);
}

/* media_kind_from_path → shared/media_drop_path.c */

static int default_track_for_kind(int kind) {
    if (kind == 2) return 2; /* A1 */
    return 0;               /* V1 for video/image */
}

static void add_clip_from_file(const char *path, int track, int start_ms) {
    if (g_n_clips >= MAX_CLIPS) {
        snprintf(g_status, sizeof(g_status), "Timeline full (%d clips)", MAX_CLIPS);
        return;
    }
    if (!media_path_is_readable_file(path)) {
        char dbg[320];
        snprintf(dbg, sizeof(dbg), "Not a readable file: %.240s", path ? path : "(null)");
        snprintf(g_status, sizeof(g_status), "%s", dbg);
        media_drop_debug_log(g_project_root, dbg);
        return;
    }
    int kind = media_kind_from_path(path);
    if (kind == 0) {
        snprintf(g_status, sizeof(g_status), "Unsupported type (need video/audio/image/webm): %.120s", path);
        return;
    }
    Clip *c = &g_clips[g_n_clips];
    memset(c, 0, sizeof(*c));
    c->used = 1;
    if (track < 0) track = default_track_for_kind(kind);
    /* keep audio on A*, video/image on V* if user dropped on wrong row we still honor drop track */
    if (kind == 2 && track < 2) track = 2;
    if (kind != 2 && track >= 2) track = 0;
    c->track = clampi(track, 0, MAX_TRACKS - 1);
    c->is_audio = (kind == 2) || (c->track >= 2);
    snprintf(c->path, sizeof(c->path), "%s", path);
    const char *base = strrchr(path, '/');
    snprintf(c->name, sizeof(c->name), "%s", base ? base + 1 : path);
    int dur = 3000;
    if (kind == 3) {
        dur = 3000; /* still image default 3s (ffmpeg still-frame) */
    } else {
        probe_duration_ms(path, &dur);
        if (dur < 100) dur = 1000;
    }
    c->in_ms = 0;
    c->out_ms = dur;
    c->duration_ms = dur;
    c->start_ms = snap_ms(start_ms);
    c->col[0] = track_cols[c->track][0];
    c->col[1] = track_cols[c->track][1];
    c->col[2] = track_cols[c->track][2];
    if (kind == 3) { /* tint images slightly */
        c->col[0] = 0.9f; c->col[1] = 0.75f; c->col[2] = 0.35f;
    }
    g_n_clips++;
    g_sel = g_n_clips - 1;
    invalidate_preview();
    const char *kname = kind == 1 ? "video" : (kind == 2 ? "audio" : "image");
    snprintf(g_status, sizeof(g_status), "Dropped %s → %s @ %dms  (%s)",
             kname, track_names[c->track], c->start_ms, c->name);
}

static void timeline_xy_to_track_ms(int mx, int my, int *out_track, int *out_ms) {
    int top = (int)(WIN_H * 0.52f);
    int end = timeline_end_ms();
    if (end < 10000) end = 10000;
    float lane_x0 = 70;
    float lane_w = WIN_W - lane_x0 - 8;
    int th = 48;
    int t = 0;
    if (my >= top + 22)
        t = (my - top - 22) / th;
    t = clampi(t, 0, MAX_TRACKS - 1);
    int ms = 0;
    if (mx >= lane_x0)
        ms = (int)((mx - lane_x0) / lane_w * end);
    if (ms < 0) ms = 0;
    *out_track = t;
    *out_ms = snap_ms(ms);
}

typedef struct { int track; int start_ms; } DropCtx;

static void on_drop_path(const char *path, void *user) {
    DropCtx *ctx = (DropCtx *)user;
    add_clip_from_file(path, ctx->track, ctx->start_ms);
    if (g_n_clips > 0)
        ctx->start_ms = g_clips[g_n_clips - 1].start_ms + g_clips[g_n_clips - 1].duration_ms;
}

static void import_uri_list(const char *data, int track, int start_ms) {
    DropCtx ctx = { track, start_ms };
    /* Shared resolver: emoji/ZWJ house paths, long percent-encoded file:// URIs */
    media_import_uri_list(data, g_project_root, on_drop_path, &ctx,
                          g_status, sizeof(g_status));
}

/* ---- X11 XDND (drag files from Nautilus/etc onto timeline) ---- */
static void xdnd_setup(void) {
    if (g_xdnd_setup) return;
    g_xdpy = glXGetCurrentDisplay();
    g_xwin = (Window)glXGetCurrentDrawable();
    if (!g_xdpy || !g_xwin) return;
    g_xa_XdndAware = XInternAtom(g_xdpy, "XdndAware", False);
    g_xa_XdndEnter = XInternAtom(g_xdpy, "XdndEnter", False);
    g_xa_XdndPosition = XInternAtom(g_xdpy, "XdndPosition", False);
    g_xa_XdndStatus = XInternAtom(g_xdpy, "XdndStatus", False);
    g_xa_XdndLeave = XInternAtom(g_xdpy, "XdndLeave", False);
    g_xa_XdndDrop = XInternAtom(g_xdpy, "XdndDrop", False);
    g_xa_XdndFinished = XInternAtom(g_xdpy, "XdndFinished", False);
    g_xa_XdndSelection = XInternAtom(g_xdpy, "XdndSelection", False);
    g_xa_XdndTypeList = XInternAtom(g_xdpy, "XdndTypeList", False);
    g_xa_XdndActionCopy = XInternAtom(g_xdpy, "XdndActionCopy", False);
    g_xa_text_uri_list = XInternAtom(g_xdpy, "text/uri-list", False);
    Atom version = 5;
    XChangeProperty(g_xdpy, g_xwin, g_xa_XdndAware, XA_ATOM, 32,
                    PropModeReplace, (unsigned char *)&version, 1);
    /* Keep freeglut's mask; add PropertyChange for SelectionNotify (drop data). */
    {
        XWindowAttributes attr;
        if (XGetWindowAttributes(g_xdpy, g_xwin, &attr))
            XSelectInput(g_xdpy, g_xwin,
                         attr.your_event_mask | PropertyChangeMask | StructureNotifyMask);
    }
    g_xdnd_setup = 1;
    snprintf(g_status, sizeof(g_status),
             "Drop files on window (mp4/webm/wav/png…) · drag clips on timeline");
}

static void xdnd_send_status(Window source, int accept) {
    XEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.xclient.type = ClientMessage;
    ev.xclient.display = g_xdpy;
    ev.xclient.window = source;
    ev.xclient.message_type = g_xa_XdndStatus;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = (long)g_xwin;
    ev.xclient.data.l[1] = accept ? 1 : 0;
    ev.xclient.data.l[2] = 0;
    ev.xclient.data.l[3] = 0;
    ev.xclient.data.l[4] = (long)g_xa_XdndActionCopy;
    XSendEvent(g_xdpy, source, False, NoEventMask, &ev);
}

static void xdnd_send_finished(Window source) {
    XEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.xclient.type = ClientMessage;
    ev.xclient.window = source;
    ev.xclient.message_type = g_xa_XdndFinished;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = (long)g_xwin;
    ev.xclient.data.l[1] = 1;
    ev.xclient.data.l[2] = (long)g_xa_XdndActionCopy;
    XSendEvent(g_xdpy, source, False, NoEventMask, &ev);
}

/* last position for drop placement */
static int g_drop_track = 0, g_drop_ms = 0;

static void xdnd_poll(void) {
    if (!g_xdnd_setup) {
        xdnd_setup();
        if (!g_xdnd_setup) return;
    }
    XEvent ev;
    /* Only pull Xdnd ClientMessages / SelectionNotify — never XNextEvent-all
     * (that steals freeglut's mouse/key events). Call this BEFORE
     * glutMainLoopEvent so freeglut does not discard unknown ClientMessages.
     *
     * IMPORTANT: must NOT drop WM_DELETE_WINDOW — that is the title-bar ✕ quit.
     * Earlier code discarded all non-Xdnd ClientMessages → X button did nothing. */
    Atom wm_protocols = XInternAtom(g_xdpy, "WM_PROTOCOLS", False);
    Atom wm_delete = XInternAtom(g_xdpy, "WM_DELETE_WINDOW", False);
    while (XCheckTypedEvent(g_xdpy, ClientMessage, &ev)) {
        Atom t = ev.xclient.message_type;
        if (t == g_xa_XdndEnter) {
            g_xdnd_source = (Window)ev.xclient.data.l[0];
            g_xdnd_ver = (int)((ev.xclient.data.l[1] >> 24) & 0xff);
            (void)g_xdnd_ver;
        } else if (t == g_xa_XdndPosition) {
            g_xdnd_source = (Window)ev.xclient.data.l[0];
            int root_x = (int)((ev.xclient.data.l[2] >> 16) & 0xffff);
            int root_y = (int)(ev.xclient.data.l[2] & 0xffff);
            Window child;
            int wx, wy;
            XTranslateCoordinates(g_xdpy, DefaultRootWindow(g_xdpy), g_xwin,
                                  root_x, root_y, &wx, &wy, &child);
            timeline_xy_to_track_ms(wx, wy, &g_drop_track, &g_drop_ms);
            xdnd_send_status(g_xdnd_source, 1);
            snprintf(g_status, sizeof(g_status), "Drop → %s @ %dms (release)",
                     track_names[g_drop_track], g_drop_ms);
        } else if (t == g_xa_XdndLeave) {
            g_xdnd_source = 0;
        } else if (t == g_xa_XdndDrop) {
            g_xdnd_source = (Window)ev.xclient.data.l[0];
            Atom prop = XInternAtom(g_xdpy, "VE_DROP_PROP", False);
            XConvertSelection(g_xdpy, g_xa_XdndSelection, g_xa_text_uri_list,
                              prop, g_xwin, CurrentTime);
        } else if (t == wm_protocols &&
                   (Atom)ev.xclient.data.l[0] == wm_delete) {
            /* title-bar close */
            g_quit = 1;
        } else {
            /* leave non-Xdnd ClientMessages for freeglut */
            XPutBackEvent(g_xdpy, &ev);
            break;
        }
    }
    while (XCheckTypedEvent(g_xdpy, SelectionNotify, &ev)) {
        if (ev.xselection.property != None) {
            Atom actual_type;
            int actual_format;
            unsigned long nitems, bytes_after;
            unsigned char *data = NULL;
            if (XGetWindowProperty(g_xdpy, g_xwin, ev.xselection.property, 0, 65536, True,
                                   AnyPropertyType, &actual_type, &actual_format,
                                   &nitems, &bytes_after, &data) == Success && data) {
                import_uri_list((char *)data, g_drop_track, g_drop_ms);
                XFree(data);
            }
        }
        if (g_xdnd_source)
            xdnd_send_finished(g_xdnd_source);
        g_xdnd_source = 0;
        glutPostRedisplay();
    }
}

static void load_demo_media(void) {
    char p[1024];
    const char *names[] = { "demo_blue.mp4", "demo_orange.mp4", "demo_green.mp4" };
    int starts[] = { 0, 2500, 5000 };
    int tracks[] = { 0, 0, 1 };
    g_n_clips = 0;
    for (int i = 0; i < 3; i++) {
        snprintf(p, sizeof(p), "%s/media/%s", g_project_root, names[i]);
        struct stat st;
        if (stat(p, &st) != 0) continue;
        add_clip_from_file(p, tracks[i], starts[i]);
    }
    /* simple audio placeholders as silent clips on A1 spanning timeline */
    if (g_n_clips > 0) {
        Clip *a = &g_clips[g_n_clips];
        memset(a, 0, sizeof(*a));
        a->used = 1;
        a->track = 2;
        a->is_audio = 1;
        snprintf(a->name, sizeof(a->name), "Audio bed");
        a->path[0] = 0;
        a->start_ms = 0;
        a->duration_ms = timeline_end_ms();
        a->in_ms = 0;
        a->out_ms = a->duration_ms;
        a->col[0] = track_cols[2][0];
        a->col[1] = track_cols[2][1];
        a->col[2] = track_cols[2][2];
        g_n_clips++;
    }
    g_sel = 0;
    snprintf(g_project_name, sizeof(g_project_name), "Demo Edit");
    snprintf(g_status, sizeof(g_status), "Demo: 3 video clips on V1/V2 + audio bed");
}

static void new_project(void) {
    g_n_clips = 0;
    g_sel = -1;
    g_playhead_ms = 0;
    set_playing(0);
    invalidate_preview();
    snprintf(g_project_name, sizeof(g_project_name), "Untitled");
    snprintf(g_status, sizeof(g_status), "New project — File→Import demo or add media");
}

/* ---- preview decode via ffmpeg (CPU-budgeted) ---- */
static void fill_placeholder_preview(float r, float g, float b, int ms) {
    /* Cheap solid fill + top bar (no per-pixel sin — that burned CPU) */
    unsigned char R = (unsigned char)(clampf(r, 0, 1) * 200);
    unsigned char G = (unsigned char)(clampf(g, 0, 1) * 200);
    unsigned char B = (unsigned char)(clampf(b, 0, 1) * 200);
    unsigned char row[PREVIEW_W * 3];
    for (int x = 0; x < PREVIEW_W; x++) {
        row[x * 3] = R; row[x * 3 + 1] = G; row[x * 3 + 2] = B;
    }
    for (int y = 0; y < PREVIEW_H; y++) {
        unsigned char *dst = g_preview_rgb + y * PREVIEW_W * 3;
        if (y < 8 || y > PREVIEW_H - 9) {
            memset(dst, 20, (size_t)PREVIEW_W * 3);
        } else {
            memcpy(dst, row, (size_t)PREVIEW_W * 3);
        }
    }
    int px = (ms % 3000) * PREVIEW_W / 3000;
    for (int y = 0; y < 20; y++) {
        int i = (y * PREVIEW_W + clampi(px, 0, PREVIEW_W - 1)) * 3;
        g_preview_rgb[i] = 255; g_preview_rgb[i + 1] = 80; g_preview_rgb[i + 2] = 60;
    }
    g_preview_valid = 1;
    g_preview_tex_dirty = 1;
}

static void write_canvas_raw_throttled(int force) {
    double t = now_sec();
    if (!force && (t - g_last_canvas_sec) * 1000.0 < CANVAS_WRITE_MS)
        return;
    g_last_canvas_sec = t;
    char cpath[1200];
    snprintf(cpath, sizeof(cpath), "%s/pieces/apps/player_app/canvas.raw", g_project_root);
    FILE *cf = fopen(cpath, "wb");
    if (cf) {
        /* row-major RGBA; write in chunks */
        unsigned char row[PREVIEW_W * 4];
        for (int y = 0; y < PREVIEW_H; y++) {
            const unsigned char *src = g_preview_rgb + (size_t)y * PREVIEW_W * 3;
            for (int x = 0; x < PREVIEW_W; x++) {
                row[x * 4] = src[x * 3];
                row[x * 4 + 1] = src[x * 3 + 1];
                row[x * 4 + 2] = src[x * 3 + 2];
                row[x * 4 + 3] = 255;
            }
            fwrite(row, 1, sizeof(row), cf);
        }
        fclose(cf);
    }
    snprintf(cpath, sizeof(cpath), "%s/pieces/apps/player_app/canvas.receipt.txt", g_project_root);
    cf = fopen(cpath, "w");
    if (cf) {
        fprintf(cf, "width=%d\nheight=%d\nbytes_per_pixel=4\nmode=preview\n", PREVIEW_W, PREVIEW_H);
        fclose(cf);
    }
}

/* Upscale tiny decode buffer into PREVIEW_W×H with nearest-neighbor (cheap). */
static void blit_upscale_rgb(const unsigned char *src, int sw, int sh) {
    for (int y = 0; y < PREVIEW_H; y++) {
        int sy = y * sh / PREVIEW_H;
        if (sy >= sh) sy = sh - 1;
        const unsigned char *srow = src + (size_t)sy * sw * 3;
        unsigned char *drow = g_preview_rgb + (size_t)y * PREVIEW_W * 3;
        for (int x = 0; x < PREVIEW_W; x++) {
            int sx = x * sw / PREVIEW_W;
            if (sx >= sw) sx = sw - 1;
            drow[x * 3]     = srow[sx * 3];
            drow[x * 3 + 1] = srow[sx * 3 + 1];
            drow[x * 3 + 2] = srow[sx * 3 + 2];
        }
    }
    g_preview_valid = 1;
    g_preview_tex_dirty = 1;
}

static void decode_preview_frame(void) {
    int ci = clip_at_playhead(1);
    int src_ms = g_playhead_ms;
    Clip *c = NULL;
    if (ci >= 0) {
        c = &g_clips[ci];
        int local = g_playhead_ms - c->start_ms;
        src_ms = c->in_ms + local;
        if (src_ms > c->out_ms) src_ms = c->out_ms;
    }

    /* PLAYING: never spawn ffmpeg. Hold last frame (or cheap solid if none). */
    if (g_playing) {
        if (!g_preview_valid) {
            if (ci >= 0 && c)
                fill_placeholder_preview(c->col[0], c->col[1], c->col[2], src_ms);
            else
                fill_placeholder_preview(0.12f, 0.12f, 0.14f, g_playhead_ms);
            g_preview_clip = ci;
            g_preview_src_ms = src_ms;
        }
        return;
    }

    /* Cache hit — same clip, close enough source time */
    if (g_preview_valid && !g_want_decode && g_preview_clip == ci &&
        abs(g_preview_src_ms - src_ms) < PREVIEW_CACHE_MS)
        return;

    /* Hard wall-clock gap: max ~2 ffmpeg/sec even on frantic scrubbing */
    double t = now_sec();
    if (g_decode_busy)
        return;
    if ((t - g_last_decode_sec) * 1000.0 < PREVIEW_DECODE_GAP_MS) {
        /* Still show something if we have nothing yet */
        if (!g_preview_valid) {
            if (ci >= 0 && c)
                fill_placeholder_preview(c->col[0], c->col[1], c->col[2], src_ms);
            else
                fill_placeholder_preview(0.12f, 0.12f, 0.14f, g_playhead_ms);
        }
        return;
    }

    if (ci < 0) {
        fill_placeholder_preview(0.12f, 0.12f, 0.14f, g_playhead_ms);
        g_preview_clip = -1;
        g_preview_src_ms = g_playhead_ms;
        g_want_decode = 0;
        return;
    }

    if (!c->path[0]) {
        fill_placeholder_preview(c->col[0], c->col[1], c->col[2], src_ms);
        g_preview_clip = ci;
        g_preview_src_ms = src_ms;
        g_want_decode = 0;
        return;
    }

    g_decode_busy = 1;
    g_last_decode_sec = t;

    char cmd[MEDIA_PATH_MAX + 512], qpath[MEDIA_PATH_MAX + 8];
    double sec = src_ms / 1000.0;
    shell_quote(c->path, qpath, sizeof(qpath));
    /* Half-res, single-thread, low nice, no audio — keep machine alive */
    snprintf(cmd, sizeof(cmd),
             "nice -n 15 %s -hide_banner -loglevel error -threads 1 "
             "-ss %.3f -i %s -frames:v 1 -f rawvideo -pix_fmt rgb24 "
             "-s %dx%d -an -y /tmp/ve_frame.rgb 2>/dev/null",
             g_ffmpeg, sec, qpath, PREVIEW_DEC_W, PREVIEW_DEC_H);
    int rc = system(cmd);

    static unsigned char small[PREVIEW_DEC_W * PREVIEW_DEC_H * 3];
    FILE *f = fopen("/tmp/ve_frame.rgb", "rb");
    if (rc != 0 || !f) {
        fill_placeholder_preview(c->col[0], c->col[1], c->col[2], src_ms);
    } else {
        size_t need = (size_t)PREVIEW_DEC_W * PREVIEW_DEC_H * 3;
        size_t n = fread(small, 1, need, f);
        fclose(f);
        if (n < need)
            fill_placeholder_preview(c->col[0], c->col[1], c->col[2], src_ms);
        else
            blit_upscale_rgb(small, PREVIEW_DEC_W, PREVIEW_DEC_H);
    }
    g_preview_clip = ci;
    g_preview_src_ms = src_ms;
    g_want_decode = 0;
    g_decode_busy = 0;
    /* canvas only when paused / scrubbed, not on a hot path */
    write_canvas_raw_throttled(0);
}

static void export_project(void) {
    /* concat demuxer from timeline order on V1 only */
    char list[1200], outp[1200], cmd[1600];
    snprintf(list, sizeof(list), "%s/pieces/apps/player_app/export_list.txt", g_project_root);
    snprintf(outp, sizeof(outp), "%s/pieces/apps/player_app/export.mp4", g_project_root);
    FILE *f = fopen(list, "w");
    if (!f) return;
    int n = 0;
    for (int i = 0; i < g_n_clips; i++) {
        Clip *c = &g_clips[i];
        if (!c->used || c->is_audio || !c->path[0]) continue;
        if (c->track != 0) continue; /* V1 only simple export */
        fprintf(f, "file '%s'\n", c->path);
        n++;
    }
    fclose(f);
    if (n == 0) {
        snprintf(g_status, sizeof(g_status), "Export: no V1 clips with paths");
        return;
    }
    snprintf(cmd, sizeof(cmd),
             "%s -y -f concat -safe 0 -i '%s' -c copy '%s' 2>/tmp/ve_export.log",
             g_ffmpeg, list, outp);
    int rc = system(cmd);
    if (rc == 0)
        snprintf(g_status, sizeof(g_status), "Exported %s (%d clips)", outp, n);
    else
        snprintf(g_status, sizeof(g_status), "Export failed — see /tmp/ve_export.log");
}

/* ---- draw ---- */
static void rect(float x, float y, float w, float h, float r, float g, float b, float a) {
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h);
    glEnd();
}
static void text(float x, float y, const char *s) {
    glColor3f(0.93f, 0.94f, 0.96f);
    glRasterPos2f(x, y);
    while (*s) glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *s++);
}
static void text_dim(float x, float y, const char *s) {
    glColor3f(0.55f, 0.58f, 0.62f);
    glRasterPos2f(x, y);
    while (*s) glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *s++);
}

static int tl_top(void) { return (int)(WIN_H * 0.52f); }
static int track_h(void) { return 48; }

static void draw_menu(void) {
    rect(0, 0, WIN_W, MENU_H, 0.20f, 0.21f, 0.23f, 1);
    rect(4, 2, 48, MENU_H - 4, g_file_menu ? 0.32f : 0.22f, g_file_menu ? 0.36f : 0.23f, 0.28f, 1);
    text(14, 16, "File");
    text_dim(64, 16, "Edit");
    text_dim(110, 16, "Clip");
    text_dim(160, 16, "View");
    char pn[96];
    snprintf(pn, sizeof(pn), "  %s  —  Muchi Video", g_project_name);
    text_dim(220, 16, pn);
    if (g_file_menu) {
        const char *items[] = {
            "New Project",
            "Load Project",
            "Save Project",
            "Import Demo Media",
            "Export MP4 (V1)",
            "Paste files (clipboard)",
            "Quit  Esc"
        };
        float mx = 4, my = MENU_H, mw = 190, mh = 22;
        rect(mx, my, mw, mh * 7 + 4, 0.18f, 0.19f, 0.21f, 0.98f);
        for (int i = 0; i < 7; i++) {
            float iy = my + 2 + i * mh;
            rect(mx + 2, iy, mw - 4, mh - 2, 0.24f, 0.26f, 0.30f, 1);
            text(mx + 10, iy + 14, items[i]);
        }
    }
}

static void draw_transport(void) {
    float ty = MENU_H;
    rect(0, ty, WIN_W, TOP_H, 0.16f, 0.17f, 0.19f, 1);
    float cx = 24, yb = ty + 12;
    rect(cx, yb, 36, 30, 0.28f, 0.29f, 0.32f, 1); text(cx + 8, yb + 20, "|<");
    cx += 42;
    rect(cx, yb, 36, 30, 0.28f, 0.29f, 0.32f, 1); text(cx + 10, yb + 20, "[]");
    cx += 42;
    rect(cx, yb - 2, 44, 34, g_playing ? 0.22f : 0.30f, g_playing ? 0.62f : 0.32f, 0.34f, 1);
    text(cx + 12, yb + 20, ">");
    cx += 52;
    rect(cx, yb, 40, 30, 0.28f, 0.30f, 0.35f, 1); text(cx + 8, yb + 20, "<"); /* step - */
    cx += 44;
    rect(cx, yb, 40, 30, 0.28f, 0.30f, 0.35f, 1); text(cx + 12, yb + 20, ">"); /* step + */

    cx += 60;
    rect(cx, ty + 8, 220, 38, 0.08f, 0.10f, 0.12f, 1);
    int sec = g_playhead_ms / 1000;
    int frm = (g_playhead_ms % 1000) / 33;
    char lcd[64];
    snprintf(lcd, sizeof(lcd), "  %02d:%02d:%02d.%02d", sec / 3600, (sec / 60) % 60, sec % 60, frm);
    text(cx + 16, ty + 24, lcd);
    text_dim(cx + 16, ty + 40, "  Space play  J/K/L  I/O trim");

    text_dim(WIN_W - 320, ty + 22, "iMovie-style: preview above, timeline below");
    text_dim(WIN_W - 320, ty + 40, "C split  R ripple  X export");
}

static void ensure_tex(void) {
    if (!g_tex) {
        glGenTextures(1, &g_tex);
        glBindTexture(GL_TEXTURE_2D, g_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
}

static void draw_preview(void) {
    int top = MENU_H + TOP_H + 8;
    int bot = tl_top() - 8;
    int area_h = bot - top;
    int area_w = WIN_W - INSPECT_W - 24;
    float scale = fminf((float)area_w / PREVIEW_W, (float)area_h / PREVIEW_H);
    float dw = PREVIEW_W * scale, dh = PREVIEW_H * scale;
    float dx = 12 + (area_w - dw) * 0.5f;
    float dy = top + (area_h - dh) * 0.5f;

    rect(0, top - 4, WIN_W - INSPECT_W, area_h + 12, 0.08f, 0.08f, 0.09f, 1);
    rect(dx - 2, dy - 2, dw + 4, dh + 4, 0.05f, 0.05f, 0.06f, 1);

    decode_preview_frame();
    ensure_tex();
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_tex);
    if (g_preview_tex_dirty) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, PREVIEW_W, PREVIEW_H, 0,
                     GL_RGB, GL_UNSIGNED_BYTE, g_preview_rgb);
        g_preview_tex_dirty = 0;
    }
    glColor3f(1, 1, 1);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(dx, dy);
    glTexCoord2f(1, 0); glVertex2f(dx + dw, dy);
    glTexCoord2f(1, 1); glVertex2f(dx + dw, dy + dh);
    glTexCoord2f(0, 1); glVertex2f(dx, dy + dh);
    glEnd();
    glDisable(GL_TEXTURE_2D);

    /* inspector */
    float ix = WIN_W - INSPECT_W;
    rect(ix, top - 4, INSPECT_W, area_h + 12, 0.14f, 0.15f, 0.17f, 1);
    text(ix + 10, top + 14, "INSPECTOR");
    if (g_sel >= 0 && g_sel < g_n_clips && g_clips[g_sel].used) {
        Clip *c = &g_clips[g_sel];
        char line[80];
        text(ix + 10, top + 36, c->name);
        snprintf(line, sizeof(line), "Track %s", track_names[c->track]);
        text_dim(ix + 10, top + 54, line);
        snprintf(line, sizeof(line), "Start  %d ms", c->start_ms);
        text_dim(ix + 10, top + 74, line);
        snprintf(line, sizeof(line), "Dur    %d ms", c->duration_ms);
        text_dim(ix + 10, top + 90, line);
        snprintf(line, sizeof(line), "In     %d ms", c->in_ms);
        text_dim(ix + 10, top + 106, line);
        snprintf(line, sizeof(line), "Out    %d ms", c->out_ms);
        text_dim(ix + 10, top + 122, line);
        text_dim(ix + 10, top + 150, "I / O  set in/out");
        text_dim(ix + 10, top + 166, "at playhead");
        text_dim(ix + 10, top + 190, "[ / ]  nudge clip");
        text_dim(ix + 10, top + 206, "Del    remove");
    } else {
        text_dim(ix + 10, top + 40, "No clip selected");
    }
}

static void draw_timeline(void) {
    int top = tl_top();
    rect(0, top, WIN_W, WIN_H - top - 24, 0.11f, 0.12f, 0.14f, 1);
    /* ruler */
    rect(0, top, WIN_W, 22, 0.14f, 0.15f, 0.17f, 1);
    int end = timeline_end_ms();
    if (end < 10000) end = 10000;
    float lane_x0 = 70;
    float lane_w = WIN_W - lane_x0 - 8;
    for (int s = 0; s <= end; s += 1000) {
        float x = lane_x0 + (float)s / end * lane_w;
        glColor4f(0.4f, 0.42f, 0.45f, 0.8f);
        glBegin(GL_LINES);
        glVertex2f(x, top); glVertex2f(x, top + 22);
        glEnd();
        char lb[16];
        snprintf(lb, sizeof(lb), "%ds", s / 1000);
        text_dim(x + 2, top + 15, lb);
    }
    text_dim(8, top + 15, "time");

    int th = track_h();
    for (int t = 0; t < MAX_TRACKS; t++) {
        float y = top + 22 + t * th;
        int is_a = t >= 2;
        rect(0, y, WIN_W, th, t % 2 ? 0.105f : 0.12f, 0.11f, 0.13f, 1);
        rect(0, y, 68, th, is_a ? 0.16f : 0.18f, 0.17f, 0.20f, 1);
        text(12, y + 28, track_names[t]);
        /* clips */
        for (int i = 0; i < g_n_clips; i++) {
            Clip *c = &g_clips[i];
            if (!c->used || c->track != t) continue;
            float x0 = lane_x0 + (float)c->start_ms / end * lane_w;
            float w = (float)c->duration_ms / end * lane_w;
            if (w < 6) w = 6;
            float r = c->col[0], g = c->col[1], b = c->col[2];
            if (i == g_sel) { r = 1; g = 0.85f; b = 0.35f; }
            rect(x0, y + 8, w, th - 16, r * 0.85f, g * 0.85f, b * 0.85f, 0.92f);
            if (w > 40) text(x0 + 6, y + 28, c->name);
        }
    }

    /* playhead */
    float phx = lane_x0 + (float)g_playhead_ms / end * lane_w;
    glColor4f(0.95f, 0.35f, 0.28f, 0.95f);
    glLineWidth(2);
    glBegin(GL_LINES);
    glVertex2f(phx, top); glVertex2f(phx, top + 22 + MAX_TRACKS * th);
    glEnd();
    glLineWidth(1);
}

static void draw_status(void) {
    rect(0, WIN_H - 24, WIN_W, 24, 0.12f, 0.13f, 0.15f, 1);
    char line[320];
    if (!g_status[0])
        snprintf(g_status, sizeof(g_status), "Drag-drop video/audio/images/webm · Space play · X export");
    snprintf(line, sizeof(line), "%s   ·  UI %.0ffps cap%d  decode=scrub-only",
             g_status, g_ui_fps, g_playing ? UI_FPS_PLAY : UI_FPS_IDLE);
    text_dim(10, WIN_H - 8, line);
}

static void draw_ui(void) {
    double t = now_sec();
    int cap = g_playing ? UI_FPS_PLAY : UI_FPS_IDLE;
    double min_frame = 1.0 / (double)cap;
    /* Hard fps cap */
    if (g_last_ui_sec > 0 && (t - g_last_ui_sec) < min_frame)
        return;
    /* Paused + nothing changed: ignore expose storms */
    if (!g_playing && !g_ui_dirty && g_last_ui_sec > 0 &&
        (t - g_last_ui_sec) < 0.5)
        return;
    g_last_ui_sec = t;
    g_fps_count++;
    if (g_fps_window <= 0) g_fps_window = t;
    if (t - g_fps_window >= 1.0) {
        g_ui_fps = (float)g_fps_count / (float)(t - g_fps_window);
        g_fps_count = 0;
        g_fps_window = t;
    }

    glClearColor(0.10f, 0.11f, 0.13f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, WIN_W, WIN_H, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    chtpm_nav_set_window(WIN_W, WIN_H);
    {
        int tl = tl_top();
        int prev_top = MENU_H + TOP_H;
        chtpm_nav_begin();
        chtpm_nav_add("Methods/File", 0, 0, (float)WIN_W, (float)MENU_H, 0);
        chtpm_nav_add("Transport", 0, (float)MENU_H, (float)WIN_W, (float)TOP_H, 1);
        chtpm_nav_add("PreviewCanvas", 0, (float)prev_top,
                      (float)(WIN_W - INSPECT_W), (float)(tl - prev_top), 2);
        chtpm_nav_add("Inspector", (float)(WIN_W - INSPECT_W), (float)prev_top,
                      (float)INSPECT_W, (float)(tl - prev_top), 3);
        chtpm_nav_add("TimelineTracks", 0, (float)tl, (float)WIN_W,
                      (float)(WIN_H - tl - 24), 4);
        chtpm_nav_add("Status", 0, (float)(WIN_H - 24), (float)WIN_W, 24, 5);
    }
    glPushMatrix();
    glTranslatef(0, (float)chtpm_nav_bar_h(), 0);
    draw_menu();
    draw_transport();
    draw_preview();
    draw_timeline();
    draw_status();
    if (g_file_menu) draw_menu();
    chtpm_nav_draw();
    glPopMatrix();

    glutSwapBuffers();
    g_ui_dirty = 0;
}

/* ---- input ---- */
static void import_clipboard_uris(void) {
    /* Fallback when XDND fails (some Wayland→X11 paths): clipboard uri-list */
    FILE *p = popen("xclip -o -selection clipboard -t text/uri-list 2>/dev/null || "
                    "xclip -o -selection clipboard 2>/dev/null || "
                    "wl-paste -t text/uri-list 2>/dev/null || true", "r");
    if (!p) {
        snprintf(g_status, sizeof(g_status), "Clipboard import failed (install xclip?)");
        return;
    }
    char buf[8192];
    size_t n = fread(buf, 1, sizeof(buf) - 1, p);
    pclose(p);
    buf[n] = 0;
    if (!n) {
        snprintf(g_status, sizeof(g_status), "Clipboard empty — copy files in Nautilus (Ctrl+C) then File→Paste import");
        return;
    }
    import_uri_list(buf, g_drop_track, g_playhead_ms > 0 ? g_playhead_ms : 0);
}

static void file_action(int item) {
    if (item == 0) new_project();
    else if (item == 1) load_project();
    else if (item == 2) save_project();
    else if (item == 3) load_demo_media();
    else if (item == 4) export_project();
    else if (item == 5) import_clipboard_uris(); /* was separator — remapped */
    else if (item == 6) { g_quit = 1; save_project(); exit(0); }
    g_file_menu = 0;
}

static void keyboard(unsigned char key, int x, int y) {
    (void)x; (void)y;
    if (key == 27 || key == 3) { g_quit = 1; save_project(); exit(0); }
    {
        int sh = glutGetModifiers() & GLUT_ACTIVE_SHIFT;
        if (chtpm_nav_on_key(key, sh)) {
            char m[160];
            chtpm_nav_status(m, sizeof(m));
            snprintf(g_status, sizeof(g_status), "%s", m);
            mark_ui_dirty();
            return;
        }
    }
    if (glutGetModifiers() & GLUT_ACTIVE_CTRL) {
        if (key == 's' || key == 'S' || key == 19) { save_project(); return; }
        if (key == 'o' || key == 'O' || key == 15) { load_project(); return; }
        if (key == 'n' || key == 'N' || key == 14) { new_project(); return; }
    }
    if (key == ' ') {
        set_playing(!g_playing);
        return;
    }
    if (key == 'j' || key == 'J') {
        g_playhead_ms = clampi(g_playhead_ms - 200, 0, timeline_end_ms());
        invalidate_preview();
        if (g_playing) audio_sync_to_playhead();
        return;
    }
    if (key == 'l' || key == 'L') {
        g_playhead_ms = clampi(g_playhead_ms + 200, 0, timeline_end_ms());
        invalidate_preview();
        if (g_playing) audio_sync_to_playhead();
        return;
    }
    if (key == 'k' || key == 'K') {
        set_playing(!g_playing);
        return;
    }
    if (key == ',' || key == '<') {
        g_playhead_ms = clampi(g_playhead_ms - 33, 0, timeline_end_ms());
        invalidate_preview();
        if (g_playing) audio_sync_to_playhead();
        return;
    }
    if (key == '.' || key == '>') {
        g_playhead_ms = clampi(g_playhead_ms + 33, 0, timeline_end_ms());
        invalidate_preview();
        if (g_playing) audio_sync_to_playhead();
        return;
    }
    if (key == 'i' || key == 'I') {
        if (g_sel >= 0) {
            Clip *c = &g_clips[g_sel];
            int local = g_playhead_ms - c->start_ms;
            if (local < 0) local = 0;
            c->in_ms = c->in_ms + local; /* rough: set source in */
            c->duration_ms = c->out_ms - c->in_ms;
            if (c->duration_ms < 100) c->duration_ms = 100;
            snprintf(g_status, sizeof(g_status), "In point set");
            invalidate_preview();
        }
        return;
    }
    if (key == 'o' || key == 'O') {
        if (g_sel >= 0) {
            Clip *c = &g_clips[g_sel];
            int local = g_playhead_ms - c->start_ms;
            if (local > 0) {
                c->out_ms = c->in_ms + local;
                c->duration_ms = c->out_ms - c->in_ms;
            }
            snprintf(g_status, sizeof(g_status), "Out point set");
            invalidate_preview();
        }
        return;
    }
    if (key == 'c' || key == 'C') {
        /* split selected at playhead */
        if (g_sel < 0 || g_n_clips >= MAX_CLIPS - 1) return;
        Clip *c = &g_clips[g_sel];
        if (g_playhead_ms <= c->start_ms || g_playhead_ms >= c->start_ms + c->duration_ms)
            return;
        int local = g_playhead_ms - c->start_ms;
        Clip right = *c;
        c->duration_ms = local;
        c->out_ms = c->in_ms + local;
        right.start_ms = g_playhead_ms;
        right.in_ms = c->out_ms;
        right.duration_ms = right.out_ms - right.in_ms;
        g_clips[g_n_clips++] = right;
        snprintf(g_status, sizeof(g_status), "Split clip");
        return;
    }
    if (key == 'r' || key == 'R') {
        if (g_sel < 0) return;
        int rem = g_clips[g_sel].duration_ms;
        int st = g_clips[g_sel].start_ms;
        g_clips[g_sel].used = 0;
        /* ripple: shift later clips on same track left */
        int tr = g_clips[g_sel].track;
        for (int i = 0; i < g_n_clips; i++) {
            if (!g_clips[i].used || g_clips[i].track != tr) continue;
            if (g_clips[i].start_ms >= st + rem)
                g_clips[i].start_ms -= rem;
        }
        /* compact */
        int w = 0;
        for (int i = 0; i < g_n_clips; i++)
            if (g_clips[i].used) g_clips[w++] = g_clips[i];
        g_n_clips = w;
        g_sel = -1;
        invalidate_preview();
        snprintf(g_status, sizeof(g_status), "Ripple delete");
        return;
    }
    if (key == 127 || key == 8) {
        if (g_sel < 0) return;
        g_clips[g_sel].used = 0;
        int w = 0;
        for (int i = 0; i < g_n_clips; i++)
            if (g_clips[i].used) g_clips[w++] = g_clips[i];
        g_n_clips = w;
        g_sel = -1;
        invalidate_preview();
        return;
    }
    if (key == 'x' || key == 'X') { export_project(); return; }
    if (key == 's' || key == 'S') { save_project(); return; }
    if (key == 'd' || key == 'D') { load_demo_media(); return; }
    if (key == 'p' || key == 'P') {
        set_playing(0);
        g_playhead_ms = 0;
        invalidate_preview();
        return;
    }
    if (key == '[') {
        if (g_sel >= 0) g_clips[g_sel].start_ms = snap_ms(g_clips[g_sel].start_ms - 100);
        return;
    }
    if (key == ']') {
        if (g_sel >= 0) g_clips[g_sel].start_ms = snap_ms(g_clips[g_sel].start_ms + 100);
        return;
    }
    if (key >= '1' && key <= '4') {
        /* select first clip on track */
        int tr = key - '1';
        for (int i = 0; i < g_n_clips; i++)
            if (g_clips[i].used && g_clips[i].track == tr) { g_sel = i; break; }
        return;
    }
}

static void special(int key, int x, int y) {
    (void)x; (void)y;
    if (key == GLUT_KEY_LEFT) {
        g_playhead_ms = clampi(g_playhead_ms - 33, 0, timeline_end_ms());
        invalidate_preview();
        if (g_playing) audio_sync_to_playhead();
    } else if (key == GLUT_KEY_RIGHT) {
        g_playhead_ms = clampi(g_playhead_ms + 33, 0, timeline_end_ms());
        invalidate_preview();
        if (g_playing) audio_sync_to_playhead();
    } else if (key == GLUT_KEY_HOME) {
        g_playhead_ms = 0; invalidate_preview();
        if (g_playing) audio_sync_to_playhead();
    }
}

static void mouse(int button, int state, int mx, int my) {
    my = chtpm_nav_mouse_y(my); /* content below mock methods bar */
    if (button == GLUT_LEFT_BUTTON && state == GLUT_UP) {
        g_drag_clip = -1;
        return;
    }
    if (button != GLUT_LEFT_BUTTON || state != GLUT_DOWN) return;

    if (my < MENU_H) {
        if (mx >= 4 && mx < 52) { g_file_menu = !g_file_menu; return; }
        g_file_menu = 0;
        return;
    }
    if (g_file_menu) {
        if (mx >= 4 && mx < 194 && my >= MENU_H && my < MENU_H + 22 * 7 + 4) {
            int item = (my - MENU_H - 2) / 22;
            if (item >= 0 && item < 7) file_action(item);
            else g_file_menu = 0;
            return;
        }
        g_file_menu = 0;
    }

    if (my >= MENU_H && my < MENU_H + TOP_H) {
        if (mx >= 24 && mx < 60) {
            set_playing(0);
            g_playhead_ms = 0;
            invalidate_preview();
            return;
        }
        if (mx >= 66 && mx < 102) { set_playing(0); return; }
        if (mx >= 108 && mx < 152) { set_playing(1); return; }
        if (mx >= 160 && mx < 200) {
            g_playhead_ms = clampi(g_playhead_ms - 33, 0, timeline_end_ms());
            invalidate_preview();
            if (g_playing) audio_sync_to_playhead();
            return;
        }
        if (mx >= 204 && mx < 244) {
            g_playhead_ms = clampi(g_playhead_ms + 33, 0, timeline_end_ms());
            invalidate_preview();
            if (g_playing) audio_sync_to_playhead();
            return;
        }
        return;
    }

    int top = tl_top();
    if (my >= top + 22) {
        int end = timeline_end_ms();
        if (end < 10000) end = 10000;
        float lane_x0 = 70;
        float lane_w = WIN_W - lane_x0 - 8;
        int th = track_h();
        int t = (my - top - 22) / th;
        if (t >= 0 && t < MAX_TRACKS && mx >= lane_x0) {
            int ms = (int)((mx - lane_x0) / lane_w * end);
            g_playhead_ms = clampi(ms, 0, end);
            invalidate_preview();
            g_sel = -1;
            g_drag_clip = -1;
            for (int i = 0; i < g_n_clips; i++) {
                Clip *c = &g_clips[i];
                if (!c->used || c->track != t) continue;
                if (ms >= c->start_ms && ms < c->start_ms + c->duration_ms) {
                    g_sel = i;
                    g_drag_clip = i;
                    g_drag_grab_ms = ms - c->start_ms;
                    g_drag_ox = mx;
                    g_drag_oy = my;
                    break;
                }
            }
        }
    }
}

static void motion(int mx, int my) {
    my = chtpm_nav_mouse_y(my);
    if (g_drag_clip < 0 || g_drag_clip >= g_n_clips) return;
    Clip *c = &g_clips[g_drag_clip];
    if (!c->used) return;
    int track, ms;
    timeline_xy_to_track_ms(mx, my, &track, &ms);
    int kind = media_kind_from_path(c->path);
    if (c->path[0] == 0) kind = c->is_audio ? 2 : 1;
    if (kind == 2 && track < 2) track = 2;
    if (kind != 2 && kind != 0 && track >= 2) track = 0;
    c->track = track;
    c->is_audio = (track >= 2) || (kind == 2);
    c->start_ms = snap_ms(ms - g_drag_grab_ms);
    if (c->start_ms < 0) c->start_ms = 0;
    c->col[0] = track_cols[c->track][0];
    c->col[1] = track_cols[c->track][1];
    c->col[2] = track_cols[c->track][2];
    snprintf(g_status, sizeof(g_status), "Move %s → %s @ %dms", c->name, track_names[c->track], c->start_ms);
    glutPostRedisplay();
}

/* ---- timeline audio (ffmpeg → Pulse) ---- */
static int clip_at_playhead_with_path(void) {
    /* Prefer lower track number (V1 over V2 over A*). Must have a real file. */
    int best = -1, best_tr = 99;
    for (int i = 0; i < g_n_clips; i++) {
        Clip *c = &g_clips[i];
        if (!c->used || !c->path[0]) continue;
        if (g_playhead_ms < c->start_ms) continue;
        if (g_playhead_ms >= c->start_ms + c->duration_ms) continue;
        /* images have no useful audio */
        if (media_kind_from_path(c->path) == 3) continue;
        if (c->track < best_tr) {
            best_tr = c->track;
            best = i;
        }
    }
    return best;
}

static void audio_kill_pipe(void) {
    int fd = g_aud_pipe;
    pid_t pid = g_aud_pid;
    g_aud_pipe = -1;
    g_aud_pid = -1;
    g_aud_clip = -1;
    g_aud_src_ms = -1;
    if (fd >= 0) close(fd);
    if (pid > 0) {
        kill(pid, SIGTERM);
        int st = 0;
        for (int i = 0; i < 10; i++) {
            pid_t r = waitpid(pid, &st, WNOHANG);
            if (r == pid || r < 0) break;
            usleep(5000);
        }
        if (waitpid(pid, &st, WNOHANG) == 0) {
            kill(pid, SIGKILL);
            waitpid(pid, &st, 0);
        }
    }
}

static void audio_open_pipe(const char *path, int src_ms) {
    audio_kill_pipe();
    if (!path || !path[0] || !g_pa_ok) return;

    int fds[2];
    if (pipe(fds) != 0) return;

    pid_t pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        return;
    }
    if (pid == 0) {
        /* child: ffmpeg → stdout */
        dup2(fds[1], STDOUT_FILENO);
        close(fds[0]);
        close(fds[1]);
        char ss[32];
        snprintf(ss, sizeof(ss), "%.3f", src_ms / 1000.0);
        /* -vn: audio only; PCM s16le stereo 44.1k for Pulse */
        execlp(g_ffmpeg, g_ffmpeg,
              "-ss", ss,
              "-i", path,
              "-vn",
              "-f", "s16le",
              "-acodec", "pcm_s16le",
              "-ac", "2",
              "-ar", "44100",
              "-v", "error",
              "pipe:1",
              (char *)NULL);
        _exit(127);
    }
    close(fds[1]);
    g_aud_pipe = fds[0];
    g_aud_pid = pid;
    /* non-blocking read so UI never stalls if ffmpeg lags */
    int fl = fcntl(g_aud_pipe, F_GETFL, 0);
    if (fl >= 0) fcntl(g_aud_pipe, F_SETFL, fl | O_NONBLOCK);
}

static void audio_sync_to_playhead(void) {
    if (!g_playing || !g_pa_ok) {
        if (g_aud_pipe >= 0) audio_kill_pipe();
        return;
    }
    int ci = clip_at_playhead_with_path();
    if (ci < 0) {
        audio_kill_pipe();
        return;
    }
    Clip *c = &g_clips[ci];
    int local = g_playhead_ms - c->start_ms;
    if (local < 0) local = 0;
    int src_ms = c->in_ms + local;
    if (src_ms > c->out_ms) src_ms = c->out_ms;

    /* Restart pipe when clip changes or seek drifts (>250ms) */
    if (ci != g_aud_clip || g_aud_pipe < 0 ||
        (g_aud_src_ms >= 0 && abs(src_ms - g_aud_src_ms) > 250)) {
        audio_open_pipe(c->path, src_ms);
        g_aud_clip = ci;
        g_aud_src_ms = src_ms;
    }
}

static void *audio_thread(void *arg) {
    (void)arg;
    int16_t buf[AUD_BUF * AUD_CH];
    while (g_aud_run && !g_quit) {
        if (!g_playing || g_aud_pipe < 0 || !g_pa) {
            usleep(8000);
            continue;
        }
        ssize_t n = read(g_aud_pipe, buf, sizeof(buf));
        if (n > 0) {
            int err = 0;
            if (pa_simple_write(g_pa, buf, (size_t)n, &err) < 0) {
                /* keep going */
            }
            /* advance estimated stream position ~ms */
            int frames = (int)(n / (AUD_CH * (int)sizeof(int16_t)));
            g_aud_src_ms += (frames * 1000) / AUD_SR;
        } else if (n == 0) {
            /* EOF — clip ended; clear so sync restarts next clip */
            audio_kill_pipe();
            usleep(5000);
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                usleep(2000);
            else {
                audio_kill_pipe();
                usleep(5000);
            }
        }
    }
    return NULL;
}

static void audio_init(void) {
    static const pa_sample_spec ss = {
        .format = PA_SAMPLE_S16LE,
        .rate = AUD_SR,
        .channels = AUD_CH
    };
    int err = 0;
    g_pa = pa_simple_new(NULL, "MuchiVideo", PA_STREAM_PLAYBACK, NULL, "timeline",
                         &ss, NULL, NULL, &err);
    g_pa_ok = (g_pa != NULL);
    if (!g_pa_ok) {
        snprintf(g_status, sizeof(g_status), "Audio offline (%s) — video only",
                 pa_strerror(err));
    }
    g_aud_run = 1;
    pthread_create(&g_aud_th, NULL, audio_thread, NULL);
}

static void audio_shutdown(void) {
    g_aud_run = 0;
    g_playing = 0;
    audio_kill_pipe();
    if (g_aud_th) pthread_join(g_aud_th, NULL);
    if (g_pa) {
        pa_simple_free(g_pa);
        g_pa = NULL;
    }
}

static void set_playing(int on) {
    g_playing = on ? 1 : 0;
    if (g_playing) {
        audio_sync_to_playhead();
        snprintf(g_status, sizeof(g_status),
                 g_pa_ok ? "Playing (audio + hold frame)" : "Playing (no Pulse)");
    } else {
        audio_kill_pipe();
        snprintf(g_status, sizeof(g_status), "Paused");
        /* One decode on stop — not during play */
        g_want_decode = 1;
        write_canvas_raw_throttled(1);
    }
    mark_ui_dirty();
}

static void timer_cb(int v) {
    (void)v;
    if (g_quit) {
        audio_shutdown();
        save_project();
        exit(0);
    }
    static double last = 0;
    double t = now_sec();
    if (last == 0) last = t;
    double dt = t - last;
    /* clamp huge stalls so playhead doesn't jump after a slow decode */
    if (dt > 0.15) dt = 0.15;
    last = t;
    if (g_playing) {
        g_play_acc += dt * 1000.0;
        int step = (int)g_play_acc;
        if (step > 0) {
            g_play_acc -= step;
            g_playhead_ms += step;
            if (g_playhead_ms > timeline_end_ms()) {
                g_playhead_ms = 0;
                audio_sync_to_playhead();
            }
            /* audio resync ~4Hz — clip edges only, cheap */
            static double last_aud = 0;
            if (last_aud == 0 || (t - last_aud) >= 0.25) {
                last_aud = t;
                audio_sync_to_playhead();
            }
            g_ui_dirty = 1; /* playhead / LCD only — no ffmpeg */
        }
    }
    xdnd_poll();
    if (g_ui_dirty || g_playing)
        glutPostRedisplay();
    glutTimerFunc(g_playing ? UI_TIMER_PLAY_MS : UI_TIMER_IDLE_MS, timer_cb, 0);
}

static void reshape(int w, int h) {
    if (w < 800) w = 800;
    if (h < 500) h = 500;
    g_win_w = w;
    g_win_h = h;
    glViewport(0, 0, g_win_w, g_win_h);
    mark_ui_dirty();
}

static void on_sig(int s) {
    (void)s;
    g_quit = 1;
    save_project();
}

/* freeglut path for window ✕ (also handled via WM_DELETE_WINDOW in xdnd_poll) */
static void on_window_close(void) {
    g_quit = 1;
}

int main(int argc, char **argv) {
    if (argc > 1) snprintf(g_project_root, sizeof(g_project_root), "%s", argv[1]);
    else {
        const char *e = getenv("PRISC_PROJECT_ROOT");
        if (e && e[0]) snprintf(g_project_root, sizeof(g_project_root), "%s", e);
    }
    signal(SIGINT, on_sig);
    signal(SIGTERM, on_sig);
    signal(SIGPIPE, SIG_IGN); /* ffmpeg pipe close must not kill UI */

    /* prefer system ffmpeg */
    if (system("command -v ffmpeg >/dev/null 2>&1") != 0)
        snprintf(g_status, sizeof(g_status), "WARN: ffmpeg missing — placeholder preview only");

    load_demo_media();
    audio_init();

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(1440, 860);
    glutCreateWindow("Muchi Video — Drop media on timeline");
    g_glut_ready = 1;
    /* ✕ must not abort process mid-audio; we drain via g_quit */
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_CONTINUE_EXECUTION);
    glutCloseFunc(on_window_close);
    glutDisplayFunc(draw_ui);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutIgnoreKeyRepeat(1);
    glutTimerFunc(UI_TIMER_IDLE_MS, timer_cb, 0);
    g_want_decode = 1; /* one initial poster frame */
    mark_ui_dirty();

    snprintf(g_status, sizeof(g_status),
             g_pa_ok
                 ? "Space play (audio) · decode on scrub only · CPU-safe"
                 : "Space play · AUDIO OFF · CPU-safe");

    /* Custom loop: XDND before freeglut. Always yield — never busy-spin. */
    while (!g_quit) {
        xdnd_poll();
        glutMainLoopEvent();
        usleep(g_playing ? PLAY_SLEEP_US : IDLE_SLEEP_US);
    }
    audio_shutdown();
    save_project();
    return 0;
}
