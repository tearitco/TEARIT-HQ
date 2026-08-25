/* daw_main.c — Muchi DAW Phase-1 pass 2 (Logic/GB/ProTools-shaped UI)
 *
 * Critical pass-1 misses (fixed here):
 *  - Piano roll was THE main view; real DAWs put Tracks/Arrangement
 *    first (horizontal lanes + regions), piano roll BELOW as editor.
 *  - No track headers with M/S/R, color chip, or [+] add track.
 *  - No bar ruler, no region blocks, transport was text soup.
 *  - Plugin strip stole permanent right third of screen.
 *
 * Layout (Logic/Pro Tools family):
 *   [ Transport ]  [ Ruler ]
 *   [ Track hdr |======= arrangement lanes / regions =======]
 *   [ + Track  ]
 *   [ Piano Roll editor for selected track .............. ]
 *   [ status ]
 *
 * Build: button.sh compile | r   Quit: Esc
 */
#define _GNU_SOURCE
#include <GL/glut.h>
#include <math.h>
#include <pulse/error.h>
#include <pulse/simple.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "../../shared/chtpm_nav_mock.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Live window size — reshape updates these so UI stays top-left pinned
 * when the user resizes (old code locked glViewport to 1440x860 so a
 * bigger window left the menu floating mid-window). */
static int g_win_w = 1440;
static int g_win_h = 860;
#define WIN_W (g_win_w)
#define WIN_H (g_win_h)
#define MAX_TRACKS 16
#define MAX_NOTES 1024
#define MAX_LIVE 32
#define PPQN 480
#define SR 44100
#define AUDIO_BUF 512
#define TICKS_VIEW (PPQN * 16) /* 16 bars visible */

/* layout constants — pass 3: menu bar + left channel strip */
#define MENU_H 24
#define STRIP_W 176
#define HDR_W 200
#define TOP_H 54
#define TOOL_H 26
#define RULER_H 22
#define ARR_TOP (MENU_H + TOP_H + TOOL_H + RULER_H)
#define TRACK_H 56
#define ADD_H 28
#define STATUS_H 26
#define SPLIT_Y_MIN 280
#define LANE_X (STRIP_W + HDR_W)

typedef struct {
    char name[32];
    float vol, pan;
    int mute, solo, armed;
    float eq_low, eq_high, reverb, distortion;
    int plugin_eq, plugin_rev, plugin_dist;
    float col[3];
} Track;

typedef struct {
    int used, track, pitch, start, len, vel;
} Note;

typedef struct {
    int active, pitch, track, releasing;
    double phase;
    float env;
} LiveVoice;

static Track g_tr[MAX_TRACKS];
static Note g_notes[MAX_NOTES];
static LiveVoice g_live[MAX_LIVE];
static int g_n_notes = 0;
static int g_n_tracks = 4;

static int g_playing, g_recording, g_cycle = 1;
static int g_tempo = 120;
static int g_playhead = 0;
static double g_play_accum = 0;
static int g_track = 0;
static int g_octave = 4;
static int g_selected = -1;
static int g_loop_end = PPQN * 8;
static int g_quantize = PPQN / 4;
static int g_snap = 1;
static int g_view_scroll = 0; /* tick scroll */
static int g_show_mixer = 0;  /* bottom mixer strip toggle */
static int g_split_y = 0;     /* arrangement bottom / editor top */
static int g_file_menu = 0;   /* File dropdown open */
static char g_project_name[64] = "Untitled";
static volatile int g_quit = 0;
static char g_status[280] = "";
static char g_project_root[1024] = ".";
static pa_simple *g_pa = NULL;
static pthread_t g_audio_th;
static pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;
static int g_audio_ok = 0;

static const float kTrackCols[][3] = {
    {0.35f, 0.55f, 0.95f}, {0.95f, 0.45f, 0.30f}, {0.35f, 0.78f, 0.50f},
    {0.85f, 0.55f, 0.90f}, {0.95f, 0.75f, 0.25f}, {0.40f, 0.85f, 0.90f},
    {0.90f, 0.40f, 0.55f}, {0.55f, 0.70f, 0.40f}, {0.70f, 0.50f, 0.95f},
    {0.95f, 0.60f, 0.40f}, {0.45f, 0.65f, 0.85f}, {0.80f, 0.80f, 0.40f},
    {0.60f, 0.45f, 0.75f}, {0.40f, 0.75f, 0.65f}, {0.85f, 0.50f, 0.50f},
    {0.50f, 0.55f, 0.80f},
};

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}
static int snap_tick(int t) {
    if (!g_snap || g_quantize <= 0) return t < 0 ? 0 : t;
    if (t < 0) t = 0;
    return ((t + g_quantize / 2) / g_quantize) * g_quantize;
}
static float clampf(float v, float a, float b) {
    return v < a ? a : (v > b ? b : v);
}
static int arr_bottom(void) {
    int max_lane = ARR_TOP + g_n_tracks * TRACK_H + ADD_H + 4;
    int split = g_split_y > 0 ? g_split_y : (int)(WIN_H * 0.52f);
    if (split < SPLIT_Y_MIN) split = SPLIT_Y_MIN;
    if (split > WIN_H - 200) split = WIN_H - 200;
    if (max_lane < split) return max_lane;
    return split;
}
static int editor_top(void) { return arr_bottom() + 4; }
static int editor_h(void) {
    int t = editor_top();
    int bot = WIN_H - STATUS_H - (g_show_mixer ? 110 : 0);
    int h = bot - t;
    return h < 120 ? 120 : h;
}
static float lane_w(void) {
    float w = (float)(WIN_W - LANE_X - 8);
    return w < 1 ? 1 : w;
}
static float tick_to_x(int tick) {
    return LANE_X + (float)(tick - g_view_scroll) / (float)TICKS_VIEW * lane_w();
}
static int x_to_tick(int x) {
    int t = g_view_scroll + (int)((x - LANE_X) / lane_w() * TICKS_VIEW);
    return t < 0 ? 0 : t;
}

/* ---- I/O ---- */
static void new_project(void) {
    pthread_mutex_lock(&g_mu);
    g_n_notes = 0;
    g_playhead = 0;
    g_view_scroll = 0;
    g_playing = 0;
    g_recording = 0;
    g_selected = -1;
    g_n_tracks = 4;
    g_track = 0;
    g_tempo = 120;
    snprintf(g_project_name, sizeof(g_project_name), "Untitled");
    for (int i = 0; i < MAX_TRACKS; i++) {
        g_tr[i].mute = g_tr[i].solo = 0;
        g_tr[i].armed = (i == 0);
        g_tr[i].vol = 0.78f;
    }
    pthread_mutex_unlock(&g_mu);
    snprintf(g_status, sizeof(g_status), "New project");
}

static void save_state(void) {
    char path[1200];
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/daw_state.txt", g_project_root);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "project=%s\ntempo=%d\nplayhead=%d\ntrack=%d\noctave=%d\nloop_end=%d\nn_tracks=%d\nnotes=%d\n",
            g_project_name, g_tempo, g_playhead, g_track, g_octave, g_loop_end, g_n_tracks, g_n_notes);
    for (int i = 0; i < g_n_tracks; i++) {
        fprintf(f, "track%d name=%s vol=%.3f pan=%.3f mute=%d solo=%d armed=%d "
                   "eq=%.2f/%.2f rev=%.2f dist=%.2f peq=%d prev=%d pdist=%d\n",
                i, g_tr[i].name, g_tr[i].vol, g_tr[i].pan, g_tr[i].mute, g_tr[i].solo,
                g_tr[i].armed, g_tr[i].eq_low, g_tr[i].eq_high, g_tr[i].reverb,
                g_tr[i].distortion, g_tr[i].plugin_eq, g_tr[i].plugin_rev, g_tr[i].plugin_dist);
    }
    fclose(f);
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/sequence.txt", g_project_root);
    f = fopen(path, "w");
    if (!f) return;
    for (int i = 0; i < g_n_notes; i++) {
        if (!g_notes[i].used) continue;
        fprintf(f, "NOTE track=%d pitch=%d start=%d len=%d vel=%d\n",
                g_notes[i].track, g_notes[i].pitch, g_notes[i].start,
                g_notes[i].len, g_notes[i].vel);
    }
    fclose(f);
}
static void load_state(void) {
    char path[1200], line[256];
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/daw_state.txt", g_project_root);
    FILE *f = fopen(path, "r");
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "project=", 8) == 0) {
                snprintf(g_project_name, sizeof(g_project_name), "%s", line + 8);
                g_project_name[strcspn(g_project_name, "\r\n")] = 0;
            } else if (strncmp(line, "tempo=", 6) == 0) g_tempo = atoi(line + 6);
            else if (strncmp(line, "playhead=", 9) == 0) g_playhead = atoi(line + 9);
            else if (strncmp(line, "track=", 6) == 0) g_track = atoi(line + 6);
            else if (strncmp(line, "octave=", 7) == 0) g_octave = atoi(line + 7);
            else if (strncmp(line, "loop_end=", 9) == 0) g_loop_end = atoi(line + 9);
            else if (strncmp(line, "n_tracks=", 9) == 0) {
                int n = atoi(line + 9);
                if (n >= 1 && n <= MAX_TRACKS) g_n_tracks = n;
            } else if (strncmp(line, "track", 5) == 0) {
                int idx = -1;
                float vol, pan, elo, ehi, rev, dist;
                int mute, solo, armed, peq, prev, pdist;
                char name[32];
                if (sscanf(line, "track%d name=%31s vol=%f pan=%f mute=%d solo=%d armed=%d eq=%f/%f rev=%f dist=%f peq=%d prev=%d pdist=%d",
                           &idx, name, &vol, &pan, &mute, &solo, &armed, &elo, &ehi, &rev, &dist, &peq, &prev, &pdist) >= 7
                    && idx >= 0 && idx < MAX_TRACKS) {
                    snprintf(g_tr[idx].name, sizeof(g_tr[idx].name), "%s", name);
                    g_tr[idx].vol = vol; g_tr[idx].pan = pan;
                    g_tr[idx].mute = mute; g_tr[idx].solo = solo; g_tr[idx].armed = armed;
                    g_tr[idx].eq_low = elo; g_tr[idx].eq_high = ehi;
                    g_tr[idx].reverb = rev; g_tr[idx].distortion = dist;
                    g_tr[idx].plugin_eq = peq; g_tr[idx].plugin_rev = prev; g_tr[idx].plugin_dist = pdist;
                }
            }
        }
        fclose(f);
    }
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/sequence.txt", g_project_root);
    f = fopen(path, "r");
    if (f) {
        g_n_notes = 0;
        while (fgets(line, sizeof(line), f) && g_n_notes < MAX_NOTES) {
            Note n = {0};
            if (sscanf(line, "NOTE track=%d pitch=%d start=%d len=%d vel=%d",
                       &n.track, &n.pitch, &n.start, &n.len, &n.vel) == 5) {
                n.used = 1;
                if (n.track >= g_n_tracks && n.track < MAX_TRACKS)
                    g_n_tracks = n.track + 1;
                g_notes[g_n_notes++] = n;
            }
        }
        fclose(f);
    }
    if (g_track >= g_n_tracks) g_track = 0;
    snprintf(g_status, sizeof(g_status), "Loaded '%s' (%d notes, %d tracks)",
             g_project_name, g_n_notes, g_n_tracks);
}
static void init_tracks(void) {
    static const char *names[] = {
        "Inst 1", "Bass", "Pads", "Arp", "Drums", "FX", "Vocal", "Extra",
        "Inst 9", "Inst 10", "Inst 11", "Inst 12", "Inst 13", "Inst 14", "Inst 15", "Inst 16"
    };
    for (int i = 0; i < MAX_TRACKS; i++) {
        snprintf(g_tr[i].name, sizeof(g_tr[i].name), "%s", names[i]);
        g_tr[i].vol = 0.78f;
        g_tr[i].pan = 0.0f;
        g_tr[i].mute = g_tr[i].solo = 0;
        g_tr[i].armed = (i == 0);
        g_tr[i].eq_low = g_tr[i].eq_high = 0.5f;
        g_tr[i].reverb = (i == 2) ? 0.25f : 0.08f;
        g_tr[i].distortion = (i == 1) ? 0.15f : 0.0f;
        g_tr[i].plugin_eq = 1;
        g_tr[i].plugin_rev = (i == 2);
        g_tr[i].plugin_dist = (i == 1);
        g_tr[i].col[0] = kTrackCols[i][0];
        g_tr[i].col[1] = kTrackCols[i][1];
        g_tr[i].col[2] = kTrackCols[i][2];
    }
    g_n_tracks = 4;
    if (g_n_notes == 0) {
        int melody[] = {60, 62, 64, 65, 67, 65, 64, 62};
        for (int i = 0; i < 8; i++)
            g_notes[g_n_notes++] = (Note){1, 0, melody[i], i * (PPQN / 2), PPQN / 2, 100};
        g_notes[g_n_notes++] = (Note){1, 1, 36, 0, PPQN * 4, 90};
        g_notes[g_n_notes++] = (Note){1, 1, 43, PPQN * 4, PPQN * 4, 90};
        g_notes[g_n_notes++] = (Note){1, 2, 64, 0, PPQN * 8, 60};
        g_notes[g_n_notes++] = (Note){1, 2, 67, PPQN * 2, PPQN * 6, 55};
    }
}
static void add_track(void) {
    if (g_n_tracks >= MAX_TRACKS) {
        snprintf(g_status, sizeof(g_status), "Max %d tracks", MAX_TRACKS);
        return;
    }
    int i = g_n_tracks++;
    snprintf(g_tr[i].name, sizeof(g_tr[i].name), "Inst %d", i + 1);
    g_track = i;
    g_tr[i].armed = 1;
    for (int j = 0; j < g_n_tracks; j++) if (j != i) g_tr[j].armed = 0;
    snprintf(g_status, sizeof(g_status), "Added track %d — %s", i + 1, g_tr[i].name);
}

/* ---- audio (unchanged path) ---- */
static float midi_hz(int pitch) {
    return 440.0f * powf(2.0f, (pitch - 69) / 12.0f);
}
static int track_audible(int ti) {
    if (ti < 0 || ti >= g_n_tracks) return 0;
    int any_solo = 0;
    for (int i = 0; i < g_n_tracks; i++) if (g_tr[i].solo) any_solo = 1;
    if (g_tr[ti].mute) return 0;
    if (any_solo && !g_tr[ti].solo) return 0;
    return 1;
}
static float process_plugins(int ti, float s) {
    Track *t = &g_tr[ti];
    if (t->plugin_eq) {
        float low_g = 0.5f + (t->eq_low - 0.5f);
        float high_g = 0.5f + (t->eq_high - 0.5f);
        s *= (0.7f * low_g + 0.3f * high_g + 0.3f);
    }
    if (t->plugin_dist && t->distortion > 0.01f)
        s = tanhf(s * (1.0f + t->distortion * 8.0f));
    if (t->plugin_rev && t->reverb > 0.01f) {
        static float rev_mem[MAX_TRACKS];
        rev_mem[ti] = rev_mem[ti] * (0.7f + 0.25f * t->reverb) + s * t->reverb * 0.35f;
        s = s * (1.0f - t->reverb * 0.4f) + rev_mem[ti];
    }
    return s;
}
static float render_sample(void) {
    float mix = 0.0f;
    pthread_mutex_lock(&g_mu);
    for (int v = 0; v < MAX_LIVE; v++) {
        LiveVoice *lv = &g_live[v];
        if (!lv->active || !track_audible(lv->track)) continue;
        lv->phase += midi_hz(lv->pitch) / (float)SR;
        if (lv->phase > 1.0) lv->phase -= 1.0;
        float s = sinf((float)(lv->phase * 2.0 * M_PI));
        s = s * 0.65f + (s > 0 ? 0.25f : -0.25f);
        if (lv->releasing) {
            lv->env *= 0.994f;
            if (lv->env < 0.001f) { lv->active = 0; continue; }
        } else if (lv->env < 1.0f) lv->env += 0.03f;
        s *= lv->env * 0.22f * g_tr[lv->track].vol;
        mix += process_plugins(lv->track, s);
    }
    if (g_playing) {
        static double ph[MAX_NOTES];
        for (int i = 0; i < g_n_notes; i++) {
            Note *n = &g_notes[i];
            if (!n->used || !track_audible(n->track)) continue;
            if (g_playhead < n->start || g_playhead >= n->start + n->len) continue;
            ph[i] += midi_hz(n->pitch) / (float)SR;
            if (ph[i] > 1.0) ph[i] -= 1.0;
            float s = sinf((float)(ph[i] * 2.0 * M_PI)) * 0.16f
                      * (n->vel / 127.0f) * g_tr[n->track].vol;
            mix += process_plugins(n->track, s);
        }
    }
    pthread_mutex_unlock(&g_mu);
    return tanhf(mix);
}
static void *audio_thread(void *arg) {
    (void)arg;
    int16_t buf[AUDIO_BUF];
    while (!g_quit) {
        for (int i = 0; i < AUDIO_BUF; i++) {
            float s = g_audio_ok ? render_sample() : 0.0f;
            if (s > 1) s = 1; if (s < -1) s = -1;
            buf[i] = (int16_t)(s * 30000.0f);
        }
        if (g_pa) {
            int err;
            pa_simple_write(g_pa, buf, sizeof(buf), &err);
        } else usleep(10000);
    }
    return NULL;
}
static void audio_start(void) {
    static const pa_sample_spec ss = { .format = PA_SAMPLE_S16LE, .rate = SR, .channels = 1 };
    int err;
    g_pa = pa_simple_new(NULL, "MuchiDAW", PA_STREAM_PLAYBACK, NULL, "synth", &ss, NULL, NULL, &err);
    g_audio_ok = g_pa ? 1 : 0;
    if (!g_pa)
        snprintf(g_status, sizeof(g_status), "Audio offline — UI only (%s)", pa_strerror(err));
    pthread_create(&g_audio_th, NULL, audio_thread, NULL);
}

/* ---- notes ---- */
static int pitch_from_key(unsigned char key) {
    static const char *white = "asdfghjkl;";
    static const int white_off[] = {0, 2, 4, 5, 7, 9, 11, 12, 14, 16};
    static const char *black = "wetyuop";
    static const int black_off[] = {1, 3, 6, 8, 10, 13, 15};
    const char *p;
    int base = 12 * (g_octave + 1);
    if ((p = strchr(white, key))) {
        int i = (int)(p - white);
        if (i >= 0 && i < 10) return base + white_off[i];
    }
    if ((p = strchr(black, key))) {
        int i = (int)(p - black);
        if (i >= 0 && i < 7) return base + black_off[i];
    }
    return -1;
}
static void note_on(int pitch, int track) {
    pthread_mutex_lock(&g_mu);
    for (int v = 0; v < MAX_LIVE; v++) {
        if (!g_live[v].active) {
            g_live[v] = (LiveVoice){1, pitch, track, 0, 0, 0.0f};
            break;
        }
    }
    if (g_recording || (g_playing && g_tr[track].armed)) {
        if (g_n_notes < MAX_NOTES) {
            int st = snap_tick(g_playhead);
            g_notes[g_n_notes++] = (Note){1, track, pitch, st, g_quantize, 100};
            g_selected = g_n_notes - 1;
        }
    }
    pthread_mutex_unlock(&g_mu);
}
static void note_off(int pitch) {
    pthread_mutex_lock(&g_mu);
    for (int v = 0; v < MAX_LIVE; v++)
        if (g_live[v].active && g_live[v].pitch == pitch)
            g_live[v].releasing = 1;
    pthread_mutex_unlock(&g_mu);
}
static void delete_selected(void) {
    if (g_selected < 0 || g_selected >= g_n_notes) return;
    pthread_mutex_lock(&g_mu);
    g_notes[g_selected].used = 0;
    int w = 0;
    for (int i = 0; i < g_n_notes; i++)
        if (g_notes[i].used) g_notes[w++] = g_notes[i];
    g_n_notes = w;
    g_selected = -1;
    pthread_mutex_unlock(&g_mu);
}

/* region bounds per track for arrangement blocks */
static void region_bounds(int ti, int *out_start, int *out_end) {
    int mn = 1 << 30, mx = -1;
    for (int i = 0; i < g_n_notes; i++) {
        if (!g_notes[i].used || g_notes[i].track != ti) continue;
        if (g_notes[i].start < mn) mn = g_notes[i].start;
        int e = g_notes[i].start + g_notes[i].len;
        if (e > mx) mx = e;
    }
    if (mx < 0) { *out_start = -1; *out_end = -1; }
    else { *out_start = mn; *out_end = mx; }
}

/* ---- draw helpers ---- */
static void rect(float x, float y, float w, float h, float r, float g, float b, float a) {
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h);
    glEnd();
}
static void rect_roundish(float x, float y, float w, float h, float r, float g, float b, float a) {
    /* soft top highlight for region look */
    rect(x, y, w, h, r, g, b, a);
    rect(x, y, w, 3, r + 0.12f, g + 0.12f, b + 0.12f, a * 0.7f);
}
static void line_v(float x, float y0, float y1, float r, float g, float b, float a) {
    glColor4f(r, g, b, a);
    glBegin(GL_LINES);
    glVertex2f(x, y0); glVertex2f(x, y1);
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
static void text_dark(float x, float y, const char *s) {
    glColor3f(0.12f, 0.12f, 0.14f);
    glRasterPos2f(x, y);
    while (*s) glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *s++);
}

/* ---- File menu bar ---- */
static void draw_menu_bar(void) {
    rect(0, 0, WIN_W, MENU_H, 0.20f, 0.21f, 0.23f, 1);
    rect(0, MENU_H - 1, WIN_W, 1, 0.08f, 0.08f, 0.09f, 1);
    /* File */
    rect(4, 2, 48, MENU_H - 4, g_file_menu ? 0.30f : 0.22f, g_file_menu ? 0.34f : 0.23f, g_file_menu ? 0.42f : 0.26f, 1);
    text(14, 16, "File");
    text_dim(64, 16, "Edit");
    text_dim(110, 16, "Track");
    text_dim(164, 16, "Mix");
    text_dim(210, 16, "View");
    char pn[80];
    snprintf(pn, sizeof(pn), "  %s%s", g_project_name, "  —  Muchi DAW");
    text_dim(280, 16, pn);
    if (g_file_menu) {
        float mx = 4, my = MENU_H, mw = 160, mh = 22;
        const char *items[] = {
            "New Project     Ctrl+N",
            "Open / Load     Ctrl+O",
            "Save            Ctrl+S",
            "Save As…",
            "——————",
            "Quit            Esc"
        };
        int n = 6;
        rect(mx, my, mw, mh * n + 4, 0.18f, 0.19f, 0.21f, 0.98f);
        for (int i = 0; i < n; i++) {
            float iy = my + 2 + i * mh;
            if (i == 4) { text_dim(mx + 10, iy + 14, items[i]); continue; }
            rect(mx + 2, iy, mw - 4, mh - 2, 0.24f, 0.26f, 0.30f, 1);
            text(mx + 10, iy + 14, items[i]);
        }
    }
}

/* ---- Left channel strip (active track: fader + inserts) ---- */
static void draw_channel_strip(void) {
    int y0 = MENU_H + TOP_H;
    int h = WIN_H - STATUS_H - y0;
    Track *T = &g_tr[g_track];
    rect(0, y0, STRIP_W, h, 0.15f, 0.16f, 0.18f, 1);
    rect(STRIP_W - 1, y0, 1, h, 0.08f, 0.08f, 0.09f, 1);

    /* header */
    rect(0, y0, STRIP_W, 36, T->col[0] * 0.35f, T->col[1] * 0.35f, T->col[2] * 0.35f, 1);
    text(8, y0 + 14, "CHANNEL");
    char line[64];
    snprintf(line, sizeof(line), "%d  %s", g_track + 1, T->name);
    text(8, y0 + 30, line);

    float y = y0 + 48;
    /* M S R */
    rect(10, y, 28, 22, T->mute ? 0.7f : 0.28f, 0.30f, 0.30f, 1); text(18, y + 15, "M");
    rect(44, y, 28, 22, T->solo ? 0.75f : 0.28f, T->solo ? 0.7f : 0.30f, 0.25f, 1); text(52, y + 15, "S");
    rect(78, y, 28, 22, T->armed ? 0.85f : 0.28f, T->armed ? 0.2f : 0.30f, 0.22f, 1); text(86, y + 15, "R");
    y += 36;

    /* volume fader */
    text_dim(10, y + 10, "Volume");
    y += 16;
    float fh = 140;
    rect(28, y, 22, fh, 0.10f, 0.10f, 0.11f, 1);
    float fill = fh * T->vol;
    rect(28, y + fh - fill, 22, fill, T->col[0], T->col[1], T->col[2], 0.95f);
    /* cap */
    rect(24, y + fh - fill - 4, 30, 8, 0.75f, 0.75f, 0.78f, 1);
    snprintf(line, sizeof(line), "%.0f%%", T->vol * 100);
    text(58, y + fh / 2, line);
    y += fh + 16;

    /* pan */
    text_dim(10, y, "Pan");
    y += 12;
    rect(12, y, STRIP_W - 28, 12, 0.12f, 0.12f, 0.14f, 1);
    float px = 12 + (STRIP_W - 28) * 0.5f + T->pan * ((STRIP_W - 28) * 0.5f);
    rect(px - 5, y - 2, 10, 16, 0.85f, 0.85f, 0.4f, 1);
    y += 28;

    /* INSERTS */
    text(10, y, "INSERTS");
    y += 16;

    /* EQ block */
    rect(8, y, STRIP_W - 16, 78, T->plugin_eq ? 0.20f : 0.14f, 0.20f, 0.24f, 1);
    snprintf(line, sizeof(line), "[%c] EQ", T->plugin_eq ? 'x' : ' ');
    text(14, y + 14, line);
    text_dim(14, y + 30, "Low");
    rect(48, y + 22, 100, 10, 0.1f, 0.1f, 0.12f, 1);
    rect(48, y + 22, 100 * T->eq_low, 10, 0.4f, 0.6f, 0.9f, 1);
    text_dim(14, y + 50, "High");
    rect(48, y + 42, 100, 10, 0.1f, 0.1f, 0.12f, 1);
    rect(48, y + 42, 100 * T->eq_high, 10, 0.9f, 0.55f, 0.35f, 1);
    text_dim(14, y + 68, "click EQ / drag bars");
    y += 88;

    /* Reverb */
    rect(8, y, STRIP_W - 16, 48, T->plugin_rev ? 0.16f : 0.14f, T->plugin_rev ? 0.24f : 0.16f, 0.28f, 1);
    snprintf(line, sizeof(line), "[%c] REVERB  %.0f%%", T->plugin_rev ? 'x' : ' ', T->reverb * 100);
    text(14, y + 16, line);
    rect(14, y + 28, STRIP_W - 36, 10, 0.1f, 0.1f, 0.12f, 1);
    rect(14, y + 28, (STRIP_W - 36) * T->reverb, 10, 0.35f, 0.7f, 0.85f, 1);
    y += 56;

    /* Distortion */
    rect(8, y, STRIP_W - 16, 48, T->plugin_dist ? 0.28f : 0.14f, 0.16f, 0.16f, 1);
    snprintf(line, sizeof(line), "[%c] DISTORT %.0f%%", T->plugin_dist ? 'x' : ' ', T->distortion * 100);
    text(14, y + 16, line);
    rect(14, y + 28, STRIP_W - 36, 10, 0.1f, 0.1f, 0.12f, 1);
    rect(14, y + 28, (STRIP_W - 36) * T->distortion, 10, 0.9f, 0.4f, 0.3f, 1);
    y += 56;

    text_dim(10, y + 8, "Keys: e/u/i toggle");
    text_dim(10, y + 24, "v/V vol  9/0 rev");
}

static void draw_transport(void) {
    /* sits UNDER menu bar (MENU_H) — never y=0 or it paints over File */
    float ty = (float)MENU_H;
    rect(0, ty, WIN_W, TOP_H, 0.16f, 0.17f, 0.19f, 1);
    rect(0, ty + TOP_H - 1, WIN_W, 1, 0.08f, 0.08f, 0.09f, 1);

    float cx = 28;
    float yb = ty + 12;
    rect(cx, yb, 36, 30, 0.28f, 0.29f, 0.32f, 1);
    text(cx + 8, yb + 20, "|<");
    cx += 42;
    rect(cx, yb, 36, 30, 0.28f, 0.29f, 0.32f, 1);
    text(cx + 10, yb + 20, "[]");
    cx += 42;
    rect(cx, yb - 2, 44, 34, g_playing ? 0.22f : 0.30f, g_playing ? 0.62f : 0.32f, g_playing ? 0.35f : 0.34f, 1);
    text(cx + 12, yb + 20, ">");
    cx += 50;
    rect(cx, yb, 36, 30, g_recording ? 0.75f : 0.32f, g_recording ? 0.18f : 0.28f, g_recording ? 0.18f : 0.30f, 1);
    text(cx + 12, yb + 20, "o");
    cx += 42;
    rect(cx, yb, 40, 30, g_cycle ? 0.25f : 0.28f, g_cycle ? 0.40f : 0.29f, g_cycle ? 0.55f : 0.32f, 1);
    text(cx + 8, yb + 20, "<>");
    cx += 56;

    rect(cx, ty + 8, 200, 38, 0.08f, 0.10f, 0.12f, 1);
    int bars = g_playhead / PPQN + 1;
    int beat = (g_playhead % PPQN) / (PPQN / 4) + 1;
    int tick = g_playhead % (PPQN / 4);
    char lcd[64];
    snprintf(lcd, sizeof(lcd), "  %03d . %d . %03d", bars, beat, tick);
    text(cx + 12, ty + 24, lcd);
    snprintf(lcd, sizeof(lcd), "  %d BPM   4/4", g_tempo);
    text_dim(cx + 12, ty + 40, lcd);
    cx += 220;

    text(cx, ty + 28, g_project_name);
    text_dim(WIN_W - 280, ty + 22, g_audio_ok ? "Audio: Pulse" : "Audio: OFF");
    text_dim(WIN_W - 280, ty + 40, "File menu top-left");
}

static void draw_toolbar(void) {
    float y = MENU_H + TOP_H;
    rect(STRIP_W, y, WIN_W - STRIP_W, TOOL_H, 0.18f, 0.19f, 0.21f, 1);
    text_dim(STRIP_W + 12, y + 18, "Tracks");
    text_dim(STRIP_W + 80, y + 18, "Snap 1/16");
    char b[64];
    snprintf(b, sizeof(b), "scroll ,/.  bar@%d", g_view_scroll / PPQN);
    text_dim(STRIP_W + 180, y + 18, b);
    text_dim(STRIP_W + 400, y + 18, g_show_mixer ? "[Mixer ON B]" : "[Mixer B]");
}

static void draw_ruler(void) {
    int y = MENU_H + TOP_H + TOOL_H;
    rect(STRIP_W, y, WIN_W - STRIP_W, RULER_H, 0.14f, 0.15f, 0.17f, 1);
    rect(STRIP_W, y, HDR_W, RULER_H, 0.16f, 0.17f, 0.19f, 1);
    text_dim(STRIP_W + 8, y + 15, "bar");
    for (int bar = 0; bar <= 16; bar++) {
        int tick = bar * PPQN;
        float x = tick_to_x(tick);
        if (x < LANE_X || x > WIN_W - 4) continue;
        line_v(x, y, y + RULER_H, 0.45f, 0.48f, 0.52f, 0.9f);
        char lb[16];
        snprintf(lb, sizeof(lb), "%d", bar + 1);
        text_dim(x + 3, y + 15, lb);
        for (int be = 1; be < 4; be++) {
            float bx = tick_to_x(tick + be * (PPQN / 4));
            if (bx > LANE_X && bx < WIN_W)
                line_v(bx, y + RULER_H - 6, y + RULER_H, 0.3f, 0.32f, 0.35f, 0.6f);
        }
    }
    if (g_cycle) {
        float x0 = tick_to_x(0), x1 = tick_to_x(g_loop_end);
        if (x1 > LANE_X) {
            if (x0 < LANE_X) x0 = LANE_X;
            rect(x0, y + 2, x1 - x0, 4, 0.25f, 0.55f, 0.75f, 0.55f);
        }
    }
}

static void draw_arrangement(void) {
    int ab = arr_bottom();
    int lane_bottom = ARR_TOP + g_n_tracks * TRACK_H;

    /* background (to the right of channel strip) */
    rect(STRIP_W, ARR_TOP, WIN_W - STRIP_W, ab - ARR_TOP, 0.11f, 0.12f, 0.14f, 1);

    for (int bar = 0; bar <= 16; bar++) {
        float x = tick_to_x(bar * PPQN);
        if (x < LANE_X || x > WIN_W) continue;
        line_v(x, ARR_TOP, lane_bottom, 0.2f, 0.22f, 0.25f, bar % 4 == 0 ? 0.5f : 0.25f);
    }

    for (int ti = 0; ti < g_n_tracks; ti++) {
        float y = ARR_TOP + ti * TRACK_H;
        int sel = (ti == g_track);
        rect(LANE_X, y, WIN_W - LANE_X, TRACK_H,
             sel ? 0.14f : (ti % 2 ? 0.105f : 0.12f),
             sel ? 0.155f : (ti % 2 ? 0.11f : 0.125f),
             sel ? 0.19f : (ti % 2 ? 0.125f : 0.14f), 1);
        line_v(LANE_X, y + TRACK_H - 1, y + TRACK_H, 0.08f, 0.08f, 0.09f, 1);

        /* track header to the right of channel strip */
        float hx = STRIP_W;
        rect(hx, y, HDR_W, TRACK_H, sel ? 0.20f : 0.17f, sel ? 0.21f : 0.18f, sel ? 0.24f : 0.20f, 1);
        rect(hx, y, 6, TRACK_H, g_tr[ti].col[0], g_tr[ti].col[1], g_tr[ti].col[2], 1);
        rect(hx + 14, y + 10, 28, 28, g_tr[ti].col[0] * 0.7f, g_tr[ti].col[1] * 0.7f, g_tr[ti].col[2] * 0.7f, 1);
        text(hx + 18, y + 28, "M");
        char nm[40];
        snprintf(nm, sizeof(nm), "%s", g_tr[ti].name);
        text(hx + 48, y + 22, nm);
        float bx = hx + 48;
        float by = y + 32;
        rect(bx, by, 18, 16, g_tr[ti].mute ? 0.7f : 0.28f, g_tr[ti].mute ? 0.35f : 0.30f, 0.30f, 1);
        text(bx + 5, by + 12, "M");
        bx += 22;
        rect(bx, by, 18, 16, g_tr[ti].solo ? 0.75f : 0.28f, g_tr[ti].solo ? 0.70f : 0.30f, g_tr[ti].solo ? 0.2f : 0.30f, 1);
        text(bx + 5, by + 12, "S");
        bx += 22;
        rect(bx, by, 18, 16, g_tr[ti].armed ? 0.85f : 0.28f, g_tr[ti].armed ? 0.2f : 0.30f, g_tr[ti].armed ? 0.2f : 0.30f, 1);
        text(bx + 5, by + 12, "R");
        rect(hx + HDR_W - 22, y + 8, 8, TRACK_H - 16, 0.1f, 0.1f, 0.11f, 1);
        float fh = (TRACK_H - 16) * g_tr[ti].vol;
        rect(STRIP_W + HDR_W - 22, y + 8 + (TRACK_H - 16) - fh, 8, fh,
             g_tr[ti].col[0], g_tr[ti].col[1], g_tr[ti].col[2], 0.85f);

        /* ---- MIDI region block(s) on lane ---- */
        int rs, re;
        region_bounds(ti, &rs, &re);
        if (rs >= 0) {
            float x0 = tick_to_x(rs);
            float x1 = tick_to_x(re);
            if (x1 > LANE_X && x0 < WIN_W) {
                if (x0 < LANE_X) x0 = LANE_X;
                float rw = x1 - x0;
                if (rw < 8) rw = 8;
                float ry = y + 10, rh = TRACK_H - 20;
                rect_roundish(x0, ry, rw, rh,
                              g_tr[ti].col[0] * 0.85f, g_tr[ti].col[1] * 0.85f, g_tr[ti].col[2] * 0.85f, 0.92f);
                /* mini note sparks inside region */
                pthread_mutex_lock(&g_mu);
                for (int ni = 0; ni < g_n_notes; ni++) {
                    Note *n = &g_notes[ni];
                    if (!n->used || n->track != ti) continue;
                    float nx = tick_to_x(n->start);
                    float nw = (float)n->len / TICKS_VIEW * (WIN_W - HDR_W);
                    if (nw < 2) nw = 2;
                    if (nx + nw < LANE_X || nx > WIN_W) continue;
                    float ny = ry + rh - 4 - ((n->pitch % 24) / 24.0f) * (rh - 8);
                    rect(nx, ny, nw, 3, 0.1f, 0.1f, 0.12f, 0.7f);
                }
                pthread_mutex_unlock(&g_mu);
                char rl[48];
                snprintf(rl, sizeof(rl), " MIDI  %s", g_tr[ti].name);
                if (rw > 50) text_dark(x0 + 6, ry + 14, rl);
            }
        } else {
            text_dim(LANE_X + 12, y + 30, "(empty — R to record or type notes)");
        }
    }

    /* + Track button row (GarageBand / Logic) */
    float ay = ARR_TOP + g_n_tracks * TRACK_H;
    if (ay + ADD_H <= ab) {
        rect(STRIP_W, ay, WIN_W - STRIP_W, ADD_H, 0.13f, 0.14f, 0.16f, 1);
        rect(STRIP_W + 8, ay + 4, 100, ADD_H - 8, 0.25f, 0.42f, 0.55f, 1);
        text(STRIP_W + 22, ay + 18, "+ Track");
        text_dim(STRIP_W + 120, ay + 18, "or press  =");
    }

    /* playhead over arrangement */
    float phx = tick_to_x(g_playhead);
    if (phx >= LANE_X && phx <= WIN_W) {
        glColor4f(0.95f, 0.35f, 0.28f, 0.95f);
        glLineWidth(2);
        glBegin(GL_LINES);
        glVertex2f(phx, ARR_TOP); glVertex2f(phx, lane_bottom);
        glEnd();
        glLineWidth(1);
        float ry = MENU_H + TOP_H + TOOL_H + RULER_H;
        glBegin(GL_TRIANGLES);
        glVertex2f(phx - 6, ry);
        glVertex2f(phx + 6, ry);
        glVertex2f(phx, ry - 8);
        glEnd();
    }
}

static void draw_piano_roll(void) {
    int et = editor_top();
    int eh = editor_h();
    int key_w = 40;
    int key_h = 12;
    int rows = eh / key_h;
    if (rows < 8) rows = 8;

    /* editor chrome */
    rect(STRIP_W, et - 4, WIN_W - STRIP_W, 4, 0.35f, 0.36f, 0.40f, 1); /* splitter */
    rect(STRIP_W, et, WIN_W - STRIP_W, 22, 0.15f, 0.16f, 0.18f, 1);
    char title[80];
    snprintf(title, sizeof(title), "Piano Roll  —  %s   (musical typing a-l / wetyu)   z/x octave %d",
             g_tr[g_track].name, g_octave);
    text(STRIP_W + 10, et + 15, title);
    text_dim(WIN_W - 200, et + 15, "Del note  |  arrows nudge");

    int grid_y = et + 22;
    int grid_h = eh - 22;
    int kx = STRIP_W;
    rect(kx, grid_y, WIN_W - kx, grid_h, 0.09f, 0.10f, 0.12f, 1);

    int pitch_top = 12 * (g_octave + 2);

    for (int row = 0; row < rows; row++) {
        int pitch = pitch_top - row;
        int pc = ((pitch % 12) + 12) % 12;
        int black = (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10);
        float y = grid_y + row * key_h;
        if (y > grid_y + grid_h) break;
        rect(kx, y, key_w, key_h - 1,
             black ? 0.18f : 0.82f, black ? 0.18f : 0.82f, black ? 0.20f : 0.85f, 1);
        rect(kx + key_w, y, WIN_W - kx - key_w, key_h - 1,
             black ? 0.10f : 0.13f, black ? 0.11f : 0.14f, black ? 0.13f : 0.16f, 1);
    }
    for (int bar = 0; bar <= 16; bar++) {
        float x = kx + key_w + (float)(bar * PPQN - g_view_scroll) / TICKS_VIEW * (WIN_W - kx - key_w);
        if (x < kx + key_w || x > WIN_W) continue;
        line_v(x, grid_y, grid_y + grid_h, 0.25f, 0.28f, 0.32f, 0.4f);
    }

    /* notes for selected track */
    pthread_mutex_lock(&g_mu);
    for (int i = 0; i < g_n_notes; i++) {
        Note *n = &g_notes[i];
        if (!n->used || n->track != g_track) continue;
        int row = pitch_top - n->pitch;
        if (row < 0 || row >= rows) continue;
        float x = STRIP_W + key_w + (float)(n->start - g_view_scroll) / TICKS_VIEW * (WIN_W - STRIP_W - key_w);
        float w = (float)n->len / TICKS_VIEW * (WIN_W - STRIP_W - key_w);
        if (w < 4) w = 4;
        float y = grid_y + row * key_h + 1;
        float r = g_tr[g_track].col[0], g = g_tr[g_track].col[1], b = g_tr[g_track].col[2];
        if (i == g_selected) { r = 1; g = 0.85f; b = 0.3f; }
        if (x + w < STRIP_W + key_w || x > WIN_W) continue;
        rect_roundish(x, y, w, key_h - 3, r, g, b, 0.92f);
    }
    pthread_mutex_unlock(&g_mu);

    /* playhead in editor */
    float phx = STRIP_W + key_w + (float)(g_playhead - g_view_scroll) / TICKS_VIEW * (WIN_W - STRIP_W - key_w);
    if (phx >= STRIP_W + key_w && phx <= WIN_W) {
        glColor4f(0.95f, 0.35f, 0.28f, 0.9f);
        glLineWidth(2);
        glBegin(GL_LINES);
        glVertex2f(phx, grid_y); glVertex2f(phx, grid_y + grid_h);
        glEnd();
        glLineWidth(1);
    }
}

static void draw_mixer_strip(void) {
    if (!g_show_mixer) return;
    int y0 = WIN_H - STATUS_H - 110;
    rect(0, y0, WIN_W, 110, 0.14f, 0.15f, 0.17f, 1);
    text(10, y0 + 16, "Mixer / Inserts  (B hide)   —  selected channel plugins");
    Track *T = &g_tr[g_track];
    float x = 20;
    /* channel strip */
    rect(x, y0 + 28, 70, 72, 0.18f, 0.19f, 0.22f, 1);
    rect(x + 26, y0 + 36, 18, 50, 0.1f, 0.1f, 0.11f, 1);
    float fh = 50 * T->vol;
    rect(x + 26, y0 + 36 + 50 - fh, 18, fh, T->col[0], T->col[1], T->col[2], 1);
    text(x + 8, y0 + 96, T->name);

    x = 110;
    char line[80];
    snprintf(line, sizeof(line), "[%c] EQ   low %.2f high %.2f   (e , .)",
             T->plugin_eq ? 'x' : ' ', T->eq_low, T->eq_high);
    rect(x, y0 + 30, 280, 22, T->plugin_eq ? 0.22f : 0.16f, 0.22f, 0.28f, 1);
    text(x + 6, y0 + 45, line);
    snprintf(line, sizeof(line), "[%c] REVERB  wet %.2f   (u  9/0)",
             T->plugin_rev ? 'x' : ' ', T->reverb);
    rect(x, y0 + 56, 280, 22, T->plugin_rev ? 0.18f : 0.16f, 0.24f, 0.30f, 1);
    text(x + 6, y0 + 71, line);
    snprintf(line, sizeof(line), "[%c] DISTORT amount %.2f   (i  -/=)",
             T->plugin_dist ? 'x' : ' ', T->distortion);
    rect(x, y0 + 82, 280, 22, T->plugin_dist ? 0.30f : 0.16f, 0.18f, 0.18f, 1);
    text(x + 6, y0 + 97, line);

    text_dim(420, y0 + 50, "Pro-style: inserts on selected track, not a permanent right dock.");
    text_dim(420, y0 + 70, "v/V volume   m mute  o solo  n arm-record");
}

static void draw_status(void) {
    rect(0, WIN_H - STATUS_H, WIN_W, STATUS_H, 0.12f, 0.13f, 0.15f, 1);
    if (!g_status[0])
        snprintf(g_status, sizeof(g_status),
                 "Space play  R rec  = +track  1-8/click select  a-l type  s save");
    text_dim(10, WIN_H - 9, g_status);
}

static void draw_ui(void) {
    glClearColor(0.10f, 0.11f, 0.13f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    /* match live window so resize keeps (0,0) at top-left of the window */
    glOrtho(0, WIN_W, WIN_H, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* Content sits BELOW CHTPM mock methods bar (reserve top pixels). */
    chtpm_nav_set_window(WIN_W, WIN_H);
    {
        int arr_h = WIN_H - ARR_TOP - STATUS_H - (g_show_mixer ? 110 : 0);
        int pr_h = 160;
        if (arr_h < 120) arr_h = 120;
        chtpm_nav_begin();
        chtpm_nav_add("Methods/File", 0, 0, (float)WIN_W, (float)MENU_H, 0);
        chtpm_nav_add("Transport", 0, (float)MENU_H, (float)WIN_W, (float)TOP_H, 1);
        chtpm_nav_add("ChannelStrip", 0, (float)(MENU_H + TOP_H), (float)STRIP_W,
                      (float)(WIN_H - MENU_H - TOP_H - STATUS_H), 2);
        chtpm_nav_add("Arrangement", (float)STRIP_W, (float)ARR_TOP,
                      (float)(WIN_W - STRIP_W), (float)(arr_h - pr_h), 3);
        chtpm_nav_add("PianoRoll", (float)STRIP_W, (float)(ARR_TOP + arr_h - pr_h),
                      (float)(WIN_W - STRIP_W), (float)pr_h, 4);
        if (g_show_mixer)
            chtpm_nav_add("Mixer", 0, (float)(WIN_H - STATUS_H - 110),
                          (float)WIN_W, 110, 5);
        chtpm_nav_add("Status", 0, (float)(WIN_H - STATUS_H), (float)WIN_W, (float)STATUS_H, 6);
    }
    glPushMatrix();
    glTranslatef(0, (float)chtpm_nav_bar_h(), 0);
    draw_menu_bar();
    draw_transport();
    draw_channel_strip();
    draw_toolbar();
    draw_ruler();
    draw_arrangement();
    draw_piano_roll();
    draw_mixer_strip();
    draw_status();
    if (g_file_menu) draw_menu_bar();
    chtpm_nav_draw(); /* top bar in window space + focus outline in content space */
    glPopMatrix();

    glutSwapBuffers();
}

static void write_canvas_raw(void) {
    char path[1200], rec[1200];
    int W = 640, H = 304;
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/canvas.raw", g_project_root);
    unsigned char *buf = calloc((size_t)W * H * 4, 1);
    if (!buf) return;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            int i = (y * W + x) * 4;
            buf[i] = 30; buf[i + 1] = 32; buf[i + 2] = 38; buf[i + 3] = 255;
            int ph = (g_playhead % TICKS_VIEW) * W / TICKS_VIEW;
            if (abs(x - ph) < 2) { buf[i] = 240; buf[i + 1] = 90; buf[i + 2] = 70; }
        }
    FILE *f = fopen(path, "wb");
    if (f) { fwrite(buf, 1, (size_t)W * H * 4, f); fclose(f); }
    free(buf);
    snprintf(rec, sizeof(rec), "%s/pieces/apps/player_app/canvas.receipt.txt", g_project_root);
    f = fopen(rec, "w");
    if (f) {
        fprintf(f, "width=%d\nheight=%d\nbytes_per_pixel=4\nmode=arrangement\n", W, H);
        fclose(f);
    }
}

static void timer_cb(int v) {
    (void)v;
    if (g_quit) { save_state(); exit(0); }
    static double last = 0;
    double t = now_sec();
    if (last == 0) last = t;
    double dt = t - last;
    last = t;
    if (g_playing) {
        g_play_accum += dt * (g_tempo / 60.0) * PPQN;
        int step = (int)g_play_accum;
        if (step > 0) {
            g_play_accum -= step;
            pthread_mutex_lock(&g_mu);
            g_playhead += step;
            if (g_cycle && g_playhead >= g_loop_end) g_playhead = 0;
            pthread_mutex_unlock(&g_mu);
            /* auto-scroll catch */
            if (g_playhead > g_view_scroll + TICKS_VIEW * 3 / 4)
                g_view_scroll = g_playhead - TICKS_VIEW / 2;
            if (g_view_scroll < 0) g_view_scroll = 0;
        }
    }
    static int div;
    if (++div >= 12) { div = 0; write_canvas_raw(); }
    glutPostRedisplay();
    glutTimerFunc(33, timer_cb, 0);
}

/* hit testing for mouse */
static int hit_track_at(int y) {
    if (y < ARR_TOP) return -1;
    int ti = (y - ARR_TOP) / TRACK_H;
    if (ti >= 0 && ti < g_n_tracks) return ti;
    return -1;
}

static void keyboard_down(unsigned char key, int x, int y) {
    (void)x; (void)y;
    if (key == 27 || key == 3) { g_quit = 1; save_state(); exit(0); }
    /* CHTPM mock nav: Tab / ` only — never steals Space/R/a-l typing */
    {
        int sh = glutGetModifiers() & GLUT_ACTIVE_SHIFT;
        if (chtpm_nav_on_key(key, sh)) {
            char m[160];
            chtpm_nav_status(m, sizeof(m));
            snprintf(g_status, sizeof(g_status), "%s", m);
            glutPostRedisplay();
            return;
        }
    }
    /* Ctrl+N/O/S via control modifier */
    if (glutGetModifiers() & GLUT_ACTIVE_CTRL) {
        if (key == 'n' || key == 'N' || key == 14) { new_project(); return; }
        if (key == 'o' || key == 'O' || key == 15) { load_state(); return; }
        if (key == 's' || key == 'S' || key == 19) {
            save_state();
            snprintf(g_status, sizeof(g_status), "Saved '%s'", g_project_name);
            return;
        }
    }
    if (key == ' ') {
        g_playing = !g_playing;
        if (!g_playing) {
            pthread_mutex_lock(&g_mu);
            for (int v = 0; v < MAX_LIVE; v++)
                if (g_live[v].active) g_live[v].releasing = 1;
            pthread_mutex_unlock(&g_mu);
        }
        snprintf(g_status, sizeof(g_status), g_playing ? "Playing" : "Stopped");
        return;
    }
    if (key == 'r' || key == 'R') {
        g_recording = !g_recording;
        if (g_recording) {
            g_tr[g_track].armed = 1;
            for (int i = 0; i < g_n_tracks; i++) if (i != g_track) g_tr[i].armed = 0;
        }
        snprintf(g_status, sizeof(g_status), g_recording ? "Record armed on selected track" : "Record off");
        return;
    }
    if (key == 'p' || key == 'P') {
        /* Pro Tools-ish: stop + return zero — was steal for rewind only */
        g_playing = 0;
        g_playhead = 0;
        g_view_scroll = 0;
        snprintf(g_status, sizeof(g_status), "Return to zero");
        return;
    }
    if (key == '=' || key == '+') { add_track(); return; }
    if (key == 'b' || key == 'B') {
        g_show_mixer = !g_show_mixer;
        snprintf(g_status, sizeof(g_status), g_show_mixer ? "Mixer open" : "Mixer closed");
        return;
    }
    if (key == 'c' || key == 'C') {
        g_cycle = !g_cycle;
        snprintf(g_status, sizeof(g_status), g_cycle ? "Cycle on" : "Cycle off");
        return;
    }
    if (key >= '1' && key <= '8') {
        int t = key - '1';
        if (t < g_n_tracks) g_track = t;
        return;
    }
    if (key == 'z') { if (g_octave > 1) g_octave--; return; }
    if (key == 'x') { if (g_octave < 7) g_octave++; return; }
    if (key == '[') { if (g_tempo > 40) g_tempo -= 2; return; }
    if (key == ']') { if (g_tempo < 240) g_tempo += 2; return; }
    if (key == ',') {
        g_view_scroll -= PPQN;
        if (g_view_scroll < 0) g_view_scroll = 0;
        return;
    }
    if (key == '.') {
        g_view_scroll += PPQN;
        return;
    }
    if (key == 'm') { g_tr[g_track].mute = !g_tr[g_track].mute; return; }
    if (key == 'o') { g_tr[g_track].solo = !g_tr[g_track].solo; return; }
    if (key == 'n') {
        for (int i = 0; i < g_n_tracks; i++) g_tr[i].armed = 0;
        g_tr[g_track].armed = 1;
        return;
    }
    if (key == 's') { save_state(); snprintf(g_status, sizeof(g_status), "Saved"); return; }
    if (key == 127 || key == 8) { delete_selected(); return; }
    if (key == 'e') { g_tr[g_track].plugin_eq ^= 1; return; }
    if (key == 'u') { g_tr[g_track].plugin_rev ^= 1; return; }
    if (key == 'i') { g_tr[g_track].plugin_dist ^= 1; return; }
    if (key == '9') { g_tr[g_track].reverb = clampf(g_tr[g_track].reverb - 0.05f, 0, 1); return; }
    if (key == '0') { g_tr[g_track].reverb = clampf(g_tr[g_track].reverb + 0.05f, 0, 1); return; }
    if (key == '-') { g_tr[g_track].distortion = clampf(g_tr[g_track].distortion - 0.05f, 0, 1); return; }
    /* note: = is add track; use Shift+- region already used */
    if (key == 'v') { g_tr[g_track].vol = clampf(g_tr[g_track].vol - 0.05f, 0, 1); return; }
    if (key == 'V') { g_tr[g_track].vol = clampf(g_tr[g_track].vol + 0.05f, 0, 1); return; }

    int pitch = pitch_from_key(key);
    if (pitch >= 0) {
        note_on(pitch, g_track);
        snprintf(g_status, sizeof(g_status), "Note %d on %s", pitch, g_tr[g_track].name);
    }
}
static void keyboard_up(unsigned char key, int x, int y) {
    (void)x; (void)y;
    int pitch = pitch_from_key(key);
    if (pitch >= 0) note_off(pitch);
}
static void special(int key, int x, int y) {
    (void)x; (void)y;
    if (key == GLUT_KEY_LEFT) {
        g_playhead = snap_tick(g_playhead - g_quantize);
        if (g_playhead < 0) g_playhead = 0;
    } else if (key == GLUT_KEY_RIGHT) {
        g_playhead = snap_tick(g_playhead + g_quantize);
    } else if (key == GLUT_KEY_HOME) {
        g_playhead = 0; g_view_scroll = 0;
    } else if (key == GLUT_KEY_UP && g_selected >= 0) {
        pthread_mutex_lock(&g_mu);
        if (g_notes[g_selected].used && g_notes[g_selected].pitch < 120)
            g_notes[g_selected].pitch++;
        pthread_mutex_unlock(&g_mu);
    } else if (key == GLUT_KEY_DOWN && g_selected >= 0) {
        pthread_mutex_lock(&g_mu);
        if (g_notes[g_selected].used && g_notes[g_selected].pitch > 12)
            g_notes[g_selected].pitch--;
        pthread_mutex_unlock(&g_mu);
    } else if (key == GLUT_KEY_PAGE_UP && g_track > 0) {
        g_track--;
    } else if (key == GLUT_KEY_PAGE_DOWN && g_track < g_n_tracks - 1) {
        g_track++;
    }
}

static int file_menu_action(int item) {
    /* 0 New 1 Open 2 Save 3 Save As 5 Quit */
    if (item == 0) {
        new_project();
        snprintf(g_project_name, sizeof(g_project_name), "Untitled");
    } else if (item == 1) {
        load_state();
    } else if (item == 2) {
        save_state();
        snprintf(g_status, sizeof(g_status), "Saved '%s'", g_project_name);
    } else if (item == 3) {
        /* Save As — stamp name with -saved */
        if (strcmp(g_project_name, "Untitled") == 0)
            snprintf(g_project_name, sizeof(g_project_name), "Project-1");
        else {
            char tmp[64];
            snprintf(tmp, sizeof(tmp), "%s", g_project_name);
            snprintf(g_project_name, sizeof(g_project_name), "%s-copy", tmp);
        }
        save_state();
        snprintf(g_status, sizeof(g_status), "Saved As '%s'", g_project_name);
    } else if (item == 5) {
        g_quit = 1; save_state(); exit(0);
    }
    g_file_menu = 0;
    return 1;
}

static int hit_channel_strip(int mx, int my) {
    if (mx >= STRIP_W || my < MENU_H + TOP_H) return 0;
    Track *T = &g_tr[g_track];
    int y0 = MENU_H + TOP_H;
    float y = y0 + 48;
    /* M S R */
    if (my >= y && my < y + 22) {
        if (mx >= 10 && mx < 38) { T->mute ^= 1; return 1; }
        if (mx >= 44 && mx < 72) { T->solo ^= 1; return 1; }
        if (mx >= 78 && mx < 106) {
            for (int i = 0; i < g_n_tracks; i++) g_tr[i].armed = 0;
            T->armed = 1; return 1;
        }
    }
    y += 36 + 16;
    float fh = 140;
    if (mx >= 24 && mx < 58 && my >= y && my < y + fh) {
        float rel = 1.0f - (my - y) / fh;
        T->vol = clampf(rel, 0, 1);
        return 1;
    }
    y += fh + 16 + 12;
    /* pan */
    if (my >= y && my < y + 16 && mx >= 12 && mx < STRIP_W - 16) {
        float mid = 12 + (STRIP_W - 28) * 0.5f;
        T->pan = clampf((mx - mid) / ((STRIP_W - 28) * 0.5f), -1, 1);
        return 1;
    }
    y += 28 + 16;
    /* EQ box */
    if (my >= y && my < y + 78 && mx >= 8 && mx < STRIP_W - 8) {
        if (my < y + 20) { T->plugin_eq ^= 1; return 1; }
        if (my >= y + 22 && my < y + 34 && mx >= 48 && mx < 148) {
            T->eq_low = clampf((mx - 48) / 100.0f, 0, 1); return 1;
        }
        if (my >= y + 42 && my < y + 54 && mx >= 48 && mx < 148) {
            T->eq_high = clampf((mx - 48) / 100.0f, 0, 1); return 1;
        }
    }
    y += 88;
    if (my >= y && my < y + 48 && mx >= 8 && mx < STRIP_W - 8) {
        if (my < y + 22) { T->plugin_rev ^= 1; return 1; }
        if (my >= y + 28 && mx >= 14 && mx < STRIP_W - 22) {
            T->reverb = clampf((mx - 14) / (float)(STRIP_W - 36), 0, 1); return 1;
        }
    }
    y += 56;
    if (my >= y && my < y + 48 && mx >= 8 && mx < STRIP_W - 8) {
        if (my < y + 22) { T->plugin_dist ^= 1; return 1; }
        if (my >= y + 28 && mx >= 14 && mx < STRIP_W - 22) {
            T->distortion = clampf((mx - 14) / (float)(STRIP_W - 36), 0, 1); return 1;
        }
    }
    return 1; /* consume clicks in strip */
}

static void mouse(int button, int state, int mx, int my) {
    my = chtpm_nav_mouse_y(my); /* content below mock methods bar */
    if (button != GLUT_LEFT_BUTTON || state != GLUT_DOWN) return;

    /* File menu bar / dropdown */
    if (my < MENU_H) {
        if (mx >= 4 && mx < 52) {
            g_file_menu = !g_file_menu;
            return;
        }
        g_file_menu = 0;
        return;
    }
    if (g_file_menu) {
        if (mx >= 4 && mx < 164 && my >= MENU_H && my < MENU_H + 22 * 6 + 4) {
            int item = (my - MENU_H - 2) / 22;
            if (item >= 0 && item < 6 && item != 4) file_menu_action(item);
            else g_file_menu = 0;
            return;
        }
        g_file_menu = 0;
    }

    /* channel strip */
    if (mx < STRIP_W && my >= MENU_H + TOP_H) {
        hit_channel_strip(mx, my);
        return;
    }

    /* transport (below menu) */
    if (my >= MENU_H && my < MENU_H + TOP_H) {
        int ty = my; /* absolute */
        (void)ty;
        if (mx >= 28 && mx < 64) { g_playing = 0; g_playhead = 0; g_view_scroll = 0; return; }
        if (mx >= 70 && mx < 106) { g_playing = 0; return; }
        if (mx >= 112 && mx < 156) { g_playing = 1; return; }
        if (mx >= 162 && mx < 198) {
            g_recording = !g_recording;
            if (g_recording) g_tr[g_track].armed = 1;
            return;
        }
        if (mx >= 204 && mx < 244) { g_cycle = !g_cycle; return; }
        return;
    }

    /* + Track */
    int ay = ARR_TOP + g_n_tracks * TRACK_H;
    if (my >= ay && my < ay + ADD_H && mx >= STRIP_W && mx < STRIP_W + 120) {
        add_track();
        return;
    }

    /* track header / lane */
    int ti = hit_track_at(my);
    if (ti >= 0 && mx >= STRIP_W) {
        g_track = ti;
        float hx = STRIP_W;
        int rowy = ARR_TOP + ti * TRACK_H + 32;
        if (my >= rowy && my < rowy + 16) {
            if (mx >= hx + 48 && mx < hx + 66) { g_tr[ti].mute ^= 1; return; }
            if (mx >= hx + 70 && mx < hx + 88) { g_tr[ti].solo ^= 1; return; }
            if (mx >= hx + 92 && mx < hx + 110) {
                for (int i = 0; i < g_n_tracks; i++) g_tr[i].armed = 0;
                g_tr[ti].armed = 1;
                return;
            }
        }
        return;
    }

    /* piano roll */
    int et = editor_top() + 22;
    int eh = editor_h() - 22;
    int key_w = 40;
    int key_h = 12;
    int kx = STRIP_W;
    if (my >= et && my < et + eh && mx > kx + key_w) {
        int pitch_top = 12 * (g_octave + 2);
        int row = (my - et) / key_h;
        int pitch = pitch_top - row;
        int tick = g_view_scroll + (int)((float)(mx - kx - key_w) / (WIN_W - kx - key_w) * TICKS_VIEW);
        tick = snap_tick(tick);
        pthread_mutex_lock(&g_mu);
        g_selected = -1;
        for (int i = 0; i < g_n_notes; i++) {
            Note *n = &g_notes[i];
            if (!n->used || n->track != g_track) continue;
            if (n->pitch == pitch && tick >= n->start && tick < n->start + n->len) {
                g_selected = i;
                pthread_mutex_unlock(&g_mu);
                return;
            }
        }
        if (g_recording || g_tr[g_track].armed) {
            if (g_n_notes < MAX_NOTES) {
                g_notes[g_n_notes] = (Note){1, g_track, pitch, tick, g_quantize, 100};
                g_selected = g_n_notes++;
            }
        }
        pthread_mutex_unlock(&g_mu);
    }
}

static void reshape(int w, int h) {
    if (w < 640) w = 640;
    if (h < 480) h = 480;
    g_win_w = w;
    g_win_h = h;
    /* keep split proportional on first layout only if still default-ish */
    if (g_split_y <= 0 || g_split_y > h - 100)
        g_split_y = (int)(h * 0.50f);
    glViewport(0, 0, g_win_w, g_win_h);
}

static void on_signal(int s) {
    (void)s;
    g_quit = 1;
    save_state();
}

int main(int argc, char **argv) {
    if (argc > 1) snprintf(g_project_root, sizeof(g_project_root), "%s", argv[1]);
    else {
        const char *e = getenv("PRISC_PROJECT_ROOT");
        if (e && e[0]) snprintf(g_project_root, sizeof(g_project_root), "%s", e);
    }
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    g_split_y = (int)(WIN_H * 0.50f);
    init_tracks();
    load_state();
    audio_start();

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(WIN_W, WIN_H);
    glutCreateWindow("Muchi DAW — File · Channel Strip · Tracks");
    glutDisplayFunc(draw_ui);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard_down);
    glutKeyboardUpFunc(keyboard_up);
    glutSpecialFunc(special);
    glutMouseFunc(mouse);
    glutIgnoreKeyRepeat(1);
    glutTimerFunc(33, timer_cb, 0);

    snprintf(g_status, sizeof(g_status),
             "Ready — File menu · L strip = EQ/vol · Space play · = +track · R rec · Audio %s",
             g_audio_ok ? "ON" : "OFF");
    glutMainLoop();
    g_quit = 1;
    if (g_pa) {
        pthread_join(g_audio_th, NULL);
        pa_simple_free(g_pa);
    }
    save_state();
    return 0;
}
