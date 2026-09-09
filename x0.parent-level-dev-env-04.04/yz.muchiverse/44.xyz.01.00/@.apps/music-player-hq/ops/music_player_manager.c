#define _XOPEN_SOURCE 700
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <signal.h>
#include <ftw.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

/* PROC-LIFECYCLE-ORCHESTRATOR-TEARDOWN.md §5 5b: register the `mpg123 -R`
 * grandchild in the master-ledger (owned by this manager) so a taskbar
 * quit / prune reaches it — mpg123 -R detaches into its own process
 * group, so the taskbar's render-group kill would miss it. This .c is
 * its own binary; build_music_player_manager.sh passes -I "$SHARED". */
#define KH_PROC_REGISTRY_IMPL
#include "kh_proc_registry.h"

/* music_player_manager: music-player-hq's real backend.
 * Invocation: music_player_manager <house_root> <package_dir> <unused>
 *
 * - reads <package_dir>/library.pdl for "root=<abs path>" lines,
 *   nftw()-scans each for .mp3 files (AppleDouble ._* skipped),
 * - drives an `mpg123 -R` child process for actual playback,
 * - publishes <package_dir>/music_player_ui.txt (plain key=value),
 * - polls  <package_dir>/music_player_action.txt (seq/cmd) for MUS_*
 *   verbs written by dispatch() in khtpm_core_render.c.
 *
 * v1 scope (see 08-roadmap/design-docs/MUSIC-PLAYER-HQ-DESIGN.md):
 * .mp3 only, whole library published (cap MAX_TRACKS, no scroll
 * window yet), no <slider> (seek/volume are +/- verbs), visualizer is
 * a position-driven ASCII block line (placeholder for the v2 FFmpeg +
 * fft_op path). */

#define MAX_TRACKS      300
#define MAX_ROOTS       32
#define PATHLEN         4096
#define NAMELEN         256
#define ACT_BUF         4096
#define BAR_W           40
#define VIZ_N           28

typedef struct { char path[PATHLEN]; char name[NAMELEN]; } Track;

static Track  tracks[MAX_TRACKS];
static int    n_tracks = 0;

static char   house_root[PATHLEN];
static char   package_dir[PATHLEN];

/* ---- playback state ---- */
static int    cur_index   = -1;          /* loaded track, -1 = none   */
static int    st_playing  = 0;           /* @P 2                       */
static int    st_paused   = 0;           /* @P 1                       */
static double pos_sec     = 0.0;
static double dur_sec     = 0.0;
static int    volume      = 80;          /* 0..100                    */
static int    shuffle     = 0;
static int    user_stopped = 0;          /* last @P 0 was an explicit STOP */

/* ---- mpg123 -R child ---- */
static pid_t  mpg_pid  = -1;
static int    mpg_in   = -1;             /* we write commands here     */
static int    mpg_out  = -1;             /* we read @-lines here (O_NONBLOCK) */
static char   mpg_line[1024];
static int    mpg_line_len = 0;
static int    mpg_ok   = 0;              /* child started & usable     */

/* ------------------------------------------------------------------ */

static void mpg_send(const char *fmt, ...) {
    if (mpg_in < 0) return;
    char buf[PATHLEN + 64];
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if (n > (int)sizeof(buf) - 2) n = (int)sizeof(buf) - 2;
    buf[n] = '\n'; buf[n + 1] = '\0';
    ssize_t w = write(mpg_in, buf, (size_t)n + 1);
    (void)w;
}

static void mpg_start(void) {
    int to_child[2], from_child[2];
    if (pipe(to_child) != 0 || pipe(from_child) != 0) return;

    pid_t pid = fork();
    if (pid < 0) return;
    if (pid == 0) {
        dup2(to_child[0], 0);
        dup2(from_child[1], 1);
        dup2(from_child[1], 2);
        close(to_child[0]); close(to_child[1]);
        close(from_child[0]); close(from_child[1]);
        execlp("mpg123", "mpg123", "-R", (char *)NULL);
        _exit(127);
    }
    close(to_child[0]);
    close(from_child[1]);
    mpg_pid = pid;
    if (house_root[0])
        kh_proc_register_owned(house_root, (long)pid, (long)pid,
                               (long)getpid(), "mpg123");
    mpg_in  = to_child[1];
    mpg_out = from_child[0];
    fcntl(mpg_out, F_SETFL, O_NONBLOCK);
    mpg_ok = 1;
    /* NB: do NOT send "SILENCE" - it suppresses the per-frame "@F
     * <frame> <left> <sec> <sec_left>" status lines this manager needs
     * to track pos_sec/dur_sec (progress bar, mm:ss, visualizer). All
     * mpg123 output goes to our pipe, not a terminal, so there's no
     * spam to silence. */
}

/* ------------------------------------------------------------------ */
/* library scan                                                       */

static int has_ext_mp3(const char *name) {
    size_t l = strlen(name);
    return l > 4 && strcasecmp(name + l - 4, ".mp3") == 0;
}

static int scan_cb(const char *fpath, const struct stat *sb,
                   int typeflag, struct FTW *ftwbuf) {
    (void)sb;
    if (typeflag != FTW_F) return 0;
    const char *base = fpath + ftwbuf->base;
    if (base[0] == '.' && base[1] == '_') return 0;   /* AppleDouble  */
    if (!has_ext_mp3(base)) return 0;
    if (n_tracks >= MAX_TRACKS) return 1;   /* non-zero: stop the walk */
    snprintf(tracks[n_tracks].path, PATHLEN, "%s", fpath);
    /* display name = basename without .mp3 */
    size_t bl = strlen(base);
    size_t cp = (bl > 4) ? bl - 4 : bl;
    if (cp >= NAMELEN) cp = NAMELEN - 1;
    memcpy(tracks[n_tracks].name, base, cp);
    tracks[n_tracks].name[cp] = '\0';
    n_tracks++;
    return 0;
}

static int track_cmp(const void *a, const void *b) {
    return strcasecmp(((const Track *)a)->name, ((const Track *)b)->name);
}

static void scan_library(void) {
    char lib[PATHLEN];
    snprintf(lib, sizeof(lib), "%s/library.pdl", package_dir);

    n_tracks = 0;
    FILE *f = fopen(lib, "r");
    if (f) {
        char line[PATHLEN];
        while (fgets(line, sizeof(line), f) && n_tracks < MAX_TRACKS) {
            line[strcspn(line, "\r\n")] = '\0';
            if (line[0] == '#' || line[0] == '\0') continue;
            if (strncmp(line, "root=", 5) != 0) continue;
            const char *root = line + 5;
            while (*root == ' ' || *root == '\t') root++;
            struct stat st;
            if (stat(root, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
            nftw(root, scan_cb, 16, FTW_PHYS);
        }
        fclose(f);
    }
    qsort(tracks, (size_t)n_tracks, sizeof(Track), track_cmp);
    cur_index = -1;   /* indices are invalidated by a rescan */
}

/* ------------------------------------------------------------------ */
/* state publish                                                      */

static void mmss(double s, char *out, size_t n) {
    if (s < 0) s = 0;
    int t = (int)(s + 0.5);
    snprintf(out, n, "%d:%02d", t / 60, t % 60);
}

static void build_bar(char *out, size_t n) {
    int pct = (dur_sec > 0.5) ? (int)(pos_sec * 100.0 / dur_sec) : 0;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    int fill = pct * BAR_W / 100;
    if (fill > BAR_W - 1) fill = BAR_W - 1;
    size_t k = 0;
    for (int i = 0; i < fill && k + 1 < n; i++) out[k++] = '=';
    if (k + 1 < n) out[k++] = '>';
    for (int i = fill + 1; i < BAR_W && k + 1 < n; i++) out[k++] = '-';
    out[k] = '\0';
}

static void build_viz(char *out, size_t n) {
    static const char *blocks[] = { " ", "_", ".", "-", "=", "*", "#", "@" };
    /* position-driven placeholder sweep - only "moves" while playing */
    size_t k = 0;
    for (int i = 0; i < VIZ_N && k + 1 < n; i++) {
        int lvl;
        if (st_playing && !st_paused) {
            double ph = (double)i * 0.6 + pos_sec * 5.0;
            /* cheap triangle wave 0..7 */
            double t = ph - (double)((long)(ph / 8.0)) * 8.0;
            lvl = (int)(t < 4.0 ? t : 8.0 - t);
        } else {
            lvl = 0;
        }
        if (lvl < 0) lvl = 0;
        if (lvl > 7) lvl = 7;
        out[k++] = blocks[lvl][0];
    }
    out[k] = '\0';
}

static void write_ui(void) {
    char tmp[PATHLEN], dst[PATHLEN];
    snprintf(dst, sizeof(dst), "%s/music_player_ui.txt", package_dir);
    snprintf(tmp, sizeof(tmp), "%s/music_player_ui.txt.tmp", package_dir);
    FILE *f = fopen(tmp, "w");
    if (!f) return;

    fprintf(f, "n_tracks=%d\n", n_tracks);
    for (int i = 0; i < n_tracks; i++) {
        fprintf(f, "t_%d_name=%s\n", i, tracks[i].name);
        fprintf(f, "t_%d_cls=%s\n", i, (i == cur_index) ? "playing" : "");
    }

    const char *state = st_playing ? (st_paused ? "PAUSED" : "PLAYING") : "STOPPED";
    fprintf(f, "state=%s\n", state);

    if (cur_index >= 0 && cur_index < n_tracks)
        fprintf(f, "track_name=%s\n", tracks[cur_index].name);
    else
        fprintf(f, "track_name=(nothing playing)\n");

    int pct = (dur_sec > 0.5) ? (int)(pos_sec * 100.0 / dur_sec) : 0;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    fprintf(f, "pos_sec=%d\n", (int)(pos_sec + 0.5));
    fprintf(f, "dur_sec=%d\n", (int)(dur_sec + 0.5));
    fprintf(f, "pos_pct=%d\n", pct);

    char a[16], b[16]; mmss(pos_sec, a, sizeof(a)); mmss(dur_sec, b, sizeof(b));
    fprintf(f, "pos_mmss=%s\n", a);
    fprintf(f, "dur_mmss=%s\n", b);

    char bar[BAR_W + 4]; build_bar(bar, sizeof(bar));
    fprintf(f, "bar=%s\n", bar);
    char viz[VIZ_N + 4]; build_viz(viz, sizeof(viz));
    fprintf(f, "viz=%s\n", viz);

    fprintf(f, "playpause=%s\n", (st_playing && !st_paused) ? "||" : ">");
    fprintf(f, "volume=%d\n", volume);
    fprintf(f, "shuffle_label=%s\n", shuffle ? "on" : "off");
    fprintf(f, "shuffle_cls=%s\n", shuffle ? "interact-engaged" : "");

    fclose(f);
    rename(tmp, dst);
}

/* ------------------------------------------------------------------ */
/* playback control                                                   */

static void load_index(int idx) {
    if (idx < 0 || idx >= n_tracks) return;
    cur_index = idx;
    pos_sec = 0; dur_sec = 0;
    user_stopped = 0;
    st_playing = 1; st_paused = 0;
    if (mpg_ok) {
        mpg_send("LOAD %s", tracks[idx].path);
        mpg_send("VOLUME %d", volume);
    }
}

static int next_index(int dir) {
    if (n_tracks == 0) return -1;
    if (shuffle) return rand() % n_tracks;
    int base = (cur_index < 0) ? 0 : cur_index;
    int nx = base + dir;
    if (nx < 0) nx = n_tracks - 1;
    if (nx >= n_tracks) nx = 0;
    return nx;
}

static void do_cmd(const char *cmd) {
    if (strncmp(cmd, "PLAY_INDEX:", 11) == 0) {
        load_index(atoi(cmd + 11));
    } else if (strcmp(cmd, "PLAYPAUSE") == 0) {
        if (cur_index < 0) { if (n_tracks) load_index(0); }
        else if (mpg_ok) { mpg_send("PAUSE"); }        /* mpg123 toggles */
    } else if (strcmp(cmd, "STOP") == 0) {
        user_stopped = 1;
        if (mpg_ok) mpg_send("STOP");
        st_playing = 0; st_paused = 0; pos_sec = 0;
    } else if (strcmp(cmd, "NEXT") == 0) {
        int nx = next_index(1);  if (nx >= 0) load_index(nx);
    } else if (strcmp(cmd, "PREV") == 0) {
        int nx = next_index(-1); if (nx >= 0) load_index(nx);
    } else if (strncmp(cmd, "SEEK:", 5) == 0) {
        int d = atoi(cmd + 5);
        if (mpg_ok && cur_index >= 0) mpg_send("JUMP %+ds", d);
    } else if (strcmp(cmd, "VOL_UP") == 0) {
        volume += 5; if (volume > 100) volume = 100;
        if (mpg_ok) mpg_send("VOLUME %d", volume);
    } else if (strcmp(cmd, "VOL_DN") == 0) {
        volume -= 5; if (volume < 0) volume = 0;
        if (mpg_ok) mpg_send("VOLUME %d", volume);
    } else if (strcmp(cmd, "SHUFFLE_TOGGLE") == 0) {
        shuffle = !shuffle;
    } else if (strcmp(cmd, "RESCAN") == 0) {
        if (mpg_ok) { mpg_send("STOP"); }
        st_playing = 0; st_paused = 0; pos_sec = 0; dur_sec = 0;
        scan_library();
    }
    /* unknown verbs ignored */
}

/* ------------------------------------------------------------------ */
/* mpg123 output                                                      */

static void handle_mpg_line(const char *l) {
    if (l[0] != '@' || l[1] == '\0') return;
    char tag = l[1];
    const char *rest = (l[2] == ' ') ? l + 3 : l + 2;

    if (tag == 'F') {
        /* @F <frame> <frames_left> <sec> <sec_left> */
        double fr = 0, fl = 0, s = 0, sl = 0;
        if (sscanf(rest, "%lf %lf %lf %lf", &fr, &fl, &s, &sl) >= 4) {
            pos_sec = s;
            dur_sec = s + sl;
            if (!st_paused) st_playing = 1;
        }
    } else if (tag == 'P') {
        int p = atoi(rest);
        if (p == 0) {
            int was_playing = st_playing;
            double was_pos = pos_sec, was_dur = dur_sec;
            st_playing = 0; st_paused = 0;
            /* natural end -> advance, unless the user hit STOP */
            if (!user_stopped && was_playing && was_dur > 1.0 &&
                was_pos >= was_dur - 2.0) {
                int nx = next_index(1);
                if (nx >= 0) load_index(nx);
            }
            user_stopped = 0;
        } else if (p == 1) {
            st_playing = 1; st_paused = 1;
        } else if (p == 2) {
            st_playing = 1; st_paused = 0;
        }
    } else if (tag == 'E') {
        /* decode/open error - skip to next so one bad file can't wedge */
        int nx = next_index(1);
        if (nx >= 0 && nx != cur_index) load_index(nx);
    }
    /* @S @I @V @R etc. - ignored in v1 */
}

static void pump_mpg(void) {
    if (mpg_out < 0) return;
    char buf[512];
    for (;;) {
        ssize_t r = read(mpg_out, buf, sizeof(buf));
        if (r <= 0) break;
        for (ssize_t i = 0; i < r; i++) {
            char c = buf[i];
            if (c == '\n' || c == '\r') {
                mpg_line[mpg_line_len] = '\0';
                if (mpg_line_len > 0) handle_mpg_line(mpg_line);
                mpg_line_len = 0;
            } else if (mpg_line_len < (int)sizeof(mpg_line) - 1) {
                mpg_line[mpg_line_len++] = c;
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* action file                                                        */

static void clear_action_file(void) {
    char p[PATHLEN];
    snprintf(p, sizeof(p), "%s/music_player_action.txt", package_dir);
    FILE *f = fopen(p, "w");
    if (f) { fprintf(f, "seq=0\ncmd=\n"); fclose(f); }
}

static void poll_action(int *last_seq) {
    char p[PATHLEN];
    snprintf(p, sizeof(p), "%s/music_player_action.txt", package_dir);
    FILE *f = fopen(p, "r");
    if (!f) return;
    char buf[ACT_BUF];
    size_t nr = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[nr] = '\0';

    int seq = 0;
    char cmd[512]; cmd[0] = '\0';
    char *ls = buf;
    while (*ls) {
        char *le = strchr(ls, '\n');
        size_t ll = le ? (size_t)(le - ls) : strlen(ls);
        if (strncmp(ls, "seq=", 4) == 0) {
            seq = atoi(ls + 4);
        } else if (strncmp(ls, "cmd=", 4) == 0) {
            size_t cl = ll - 4;
            if (cl >= sizeof(cmd)) cl = sizeof(cmd) - 1;
            memcpy(cmd, ls + 4, cl);
            cmd[cl] = '\0';
        }
        if (!le) break;
        ls = le + 1;
    }
    if (seq > *last_seq && cmd[0]) {
        *last_seq = seq;
        do_cmd(cmd);
    }
}

/* ------------------------------------------------------------------ */

/* On SIGTERM/SIGINT (taskbar quit, <module> cleanup), take mpg123 down
 * with us and drop its ledger row — mpg123 -R is its own group leader,
 * so it does NOT die with the manager's group otherwise. */
static void mp_on_term(int sig) {
    (void)sig;
    if (mpg_pid > 0) {
        kill(mpg_pid, SIGKILL);
        if (house_root[0]) kh_proc_reap_one(house_root, (long)mpg_pid, 1);
    }
    _exit(0);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <house_root> <package_dir> <unused>\n", argv[0]);
        return 1;
    }
    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, mp_on_term);
    signal(SIGINT,  mp_on_term);
    snprintf(house_root, sizeof(house_root), "%s", argv[1]);
    snprintf(package_dir, sizeof(package_dir), "%s", argv[2]);
    srand((unsigned)time(NULL));

    mpg_start();
    clear_action_file();
    write_ui();          /* publish an empty shell immediately so the
                          * window renders while a slow root scans */
    scan_library();
    write_ui();

    int last_seq = 0;
    int tick = 0;
    for (;;) {
        usleep(50000);                 /* 50 ms */
        poll_action(&last_seq);
        pump_mpg();

        /* reap a dead mpg123 (installed? crashed?) */
        if (mpg_pid > 0) {
            int status;
            if (waitpid(mpg_pid, &status, WNOHANG) == mpg_pid) {
                if (house_root[0]) kh_proc_reap_one(house_root, (long)mpg_pid, 1);
                mpg_pid = -1; mpg_ok = 0;
                if (mpg_in >= 0)  { close(mpg_in);  mpg_in  = -1; }
                if (mpg_out >= 0) { close(mpg_out); mpg_out = -1; }
            }
        }

        if (++tick >= 6) {             /* ~0.3 s: refresh pos/bar/viz */
            tick = 0;
            write_ui();
        }
    }
    return 0;
}
