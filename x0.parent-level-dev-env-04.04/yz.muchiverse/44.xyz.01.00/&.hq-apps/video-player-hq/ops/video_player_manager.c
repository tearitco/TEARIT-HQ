/* video_player_manager.c - backend of video-player-hq: a queue you drop video files onto, played by an ffplay child, with a file check.
 *   run:    video_player_manager <house_root> <package_dir>      publishes video_player_ui.txt, reads video_player_action.txt by cursor
 *   probe:  video_player_manager --probe <file>                  prints the file check (harness hook, no window)
 * Why a check: a presentation video that played nowhere turned out to hold 3 video frames over 23 s (variable frame rate); the check says so.
 * Playback is ffplay in its own window (fork+exec, no shell); Pause/Resume are SIGSTOP/SIGCONT; the child is stopped when the manager quits. */
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define MAXQ 64
static char pkg[1024], queue[MAXQ][1024]; static int nq, cur = -1;
static int status;                       /* 0 stopped, 1 playing, 2 paused */
static pid_t child; static time_t t_start; static double paused_acc; static time_t t_pause;
static char info[8][200]; static int ninfo; static char logline[200];
static volatile sig_atomic_t quit_flag;
static void on_term(int s) { (void)s; quit_flag = 1; }

static int capture(char *const argv[], char *out, size_t n) {   /* fork+exec, read stdout */
    int fd[2]; if (pipe(fd)) return -1; pid_t p = fork();
    if (p == 0) { dup2(fd[1], 1); int nul = open("/dev/null", O_WRONLY); dup2(nul, 2); close(fd[0]); close(fd[1]); execvp(argv[0], argv); _exit(127); }
    close(fd[1]); size_t k = 0; ssize_t r; while (k + 1 < n && (r = read(fd[0], out + k, n - 1 - k)) > 0) k += (size_t)r; out[k] = 0; close(fd[0]);
    int st; waitpid(p, &st, 0); return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}
static const char *kv(const char *blob, const char *key, char *v, size_t n, int nth) {
    size_t kl = strlen(key); const char *p = blob; int seen = 0; v[0] = 0;
    while (*p) { const char *e = strchr(p, '\n'); size_t len = e ? (size_t)(e - p) : strlen(p);
        if (len > kl && !strncmp(p, key, kl) && p[kl] == '=') { if (seen++ == nth) { size_t m = len - kl - 1; if (m >= n) m = n - 1; memcpy(v, p + kl + 1, m); v[m] = 0; return v; } }
        if (!e) break; p = e + 1; }
    return NULL;
}
static void check(const char *file) {                           /* fills info[] */
    ninfo = 0; char out[4096], v[128], w[64], h[64], fps[64], vc[64], ac[64], dur[64], nf[64];
    char *const a[] = { "ffprobe", "-v", "error", "-show_entries", "format=duration:stream=codec_type,codec_name,width,height,avg_frame_rate,nb_frames", "-of", "default=nw=1", (char *)file, NULL };
    if (capture(a, out, sizeof out) != 0 || !out[0]) { snprintf(info[ninfo++], 200, "cannot read this file (ffprobe failed): not a video, or damaged"); return; }
    /* stream blocks come in file order: the first codec_name belongs to the first stream; find which is video by codec_type order */
    char t0[32], t1[32]; kv(out, "codec_type", t0, sizeof t0, 0); kv(out, "codec_type", t1, sizeof t1, 1);
    int vi = !strcmp(t0, "video") ? 0 : 1, ai = vi ? 0 : 1;
    kv(out, "codec_name", vc, sizeof vc, vi); kv(out, "codec_name", ac, sizeof ac, ai); kv(out, "width", w, sizeof w, 0); kv(out, "height", h, sizeof h, 0);
    kv(out, "avg_frame_rate", fps, sizeof fps, vi); kv(out, "nb_frames", nf, sizeof nf, vi); kv(out, "duration", dur, sizeof dur, 0);
    double d = atof(dur); long frames = atol(nf);
    snprintf(info[ninfo++], 200, "video %s %sx%s   audio %s   %.1f s   avg fps %s   frames %s", vc[0] ? vc : "none", w, h, !strcmp(t0, "audio") || !strcmp(t1, "audio") ? ac : "none", d, fps, nf);
    (void)v;
    if (!vc[0]) snprintf(info[ninfo++], 200, "WARNING no video stream");
    else if (frames > 0 && d > 5 && (double)frames / d < 1.0) snprintf(info[ninfo++], 200, "WARNING only %ld video frames over %.0f s: sparse variable frame rate, some players stall or show nothing; re-encode at a constant frame rate", frames, d);
    else snprintf(info[ninfo++], 200, "check ok: frame rate looks regular");
}
static void stop_child(void) { if (child > 0) { kill(child, SIGCONT); kill(child, SIGTERM); int st; waitpid(child, &st, 0); child = 0; } status = 0; paused_acc = 0; }
static void play(int i) {
    if (i < 0 || i >= nq) return; stop_child(); cur = i; check(queue[i]);
    child = fork(); if (child == 0) { setpgid(0, 0); int nul = open("/dev/null", O_RDWR); dup2(nul, 0); dup2(nul, 1); dup2(nul, 2);
        execlp("ffplay", "ffplay", "-autoexit", "-loglevel", "error", "-window_title", "video-player-hq", queue[i], (char *)NULL); _exit(127); }
    status = 1; t_start = time(NULL); paused_acc = 0; snprintf(logline, sizeof logline, "playing %s", strrchr(queue[i], '/') ? strrchr(queue[i], '/') + 1 : queue[i]);
}
static void do_cmd(char *line) {
    char *sp = strchr(line, ' '); char *arg = sp ? sp + 1 : ""; if (sp) *sp = 0;
    if (!strcmp(line, "add")) { if (nq < MAXQ && arg[0]) { struct stat sb; if (stat(arg, &sb) || !S_ISREG(sb.st_mode)) { snprintf(logline, sizeof logline, "not a file: %s", arg); return; }
            snprintf(queue[nq], sizeof queue[0], "%s", arg); nq++; cur = nq - 1; check(queue[cur]); snprintf(logline, sizeof logline, "added %s", arg); if (status == 0) play(cur); } }
    else if (!strcmp(line, "play")) { int i = arg[0] ? atoi(arg) : (cur >= 0 ? cur : 0); play(i); }
    else if (!strcmp(line, "pause") && status == 1) { kill(child, SIGSTOP); status = 2; t_pause = time(NULL); }
    else if (!strcmp(line, "resume") && status == 2) { kill(child, SIGCONT); paused_acc += (double)(time(NULL) - t_pause); status = 1; }
    else if (!strcmp(line, "stop")) { stop_child(); snprintf(logline, sizeof logline, "stopped"); }
    else if (!strcmp(line, "next")) { if (cur + 1 < nq) play(cur + 1); }
    else if (!strcmp(line, "prev")) { if (cur > 0) play(cur - 1); }
    else if (!strcmp(line, "clear")) { stop_child(); nq = 0; cur = -1; ninfo = 0; snprintf(logline, sizeof logline, "queue cleared"); }
}
static void publish(void) {
    char dst[1536], tmp[1600]; snprintf(dst, sizeof dst, "%s/video_player_ui.txt", pkg); snprintf(tmp, sizeof tmp, "%s.tmp", dst);
    FILE *f = fopen(tmp, "w"); if (!f) return; const char *st[] = { "stopped", "playing", "paused" };
    double el = status == 1 ? difftime(time(NULL), t_start) - paused_acc : status == 2 ? difftime(t_pause, t_start) - paused_acc : 0;
    fprintf(f, "head=Video Player  ·  %s\n", st[status]); fprintf(f, "status=%s%s%s   %d:%02d elapsed\n", st[status], cur >= 0 ? "   " : "", cur >= 0 ? (strrchr(queue[cur], '/') ? strrchr(queue[cur], '/') + 1 : queue[cur]) : "", (int)el / 60, (int)el % 60);
    fprintf(f, "hint=Drop a video file onto this window to add and play it. Numbers are the row numbers to press.\nlog=%s\n", logline);
    fprintf(f, "n_info=%d\n", ninfo); for (int i = 0; i < ninfo; i++) { fprintf(f, "i_%d_text=%s\n", i, info[i]); fprintf(f, "i_%d_cls=%s\n", i, !strncmp(info[i], "WARNING", 7) || !strncmp(info[i], "cannot", 6) ? "info-warn" : "info-ok"); }
    fprintf(f, "n_q=%d\n", nq); for (int i = 0; i < nq; i++) { const char *b = strrchr(queue[i], '/') ? strrchr(queue[i], '/') + 1 : queue[i]; fprintf(f, "q_%d_text=%s%d  %s\n", i, i == cur ? "> " : "  ", i, b); fprintf(f, "q_%d_cls=%s\n", i, i == cur ? "q-cur" : "q-row"); fprintf(f, "q_%d_act=play %d\n", i, i); }
    fclose(f); rename(tmp, dst);
}
int main(int argc, char **argv) {
    if (argc >= 3 && !strcmp(argv[1], "--probe")) { check(argv[2]); for (int i = 0; i < ninfo; i++) puts(info[i]); return 0; }
    if (argc < 3) { fprintf(stderr, "usage: %s <house_root> <package_dir>\n", argv[0]); return 2; }
    snprintf(pkg, sizeof pkg, "%s", argv[2]); signal(SIGTERM, on_term); signal(SIGINT, on_term);
    char ap[1536]; snprintf(ap, sizeof ap, "%s/video_player_action.txt", pkg); struct stat sb; long cursor = stat(ap, &sb) ? 0 : (long)sb.st_size;   /* start after old rows */
    while (!quit_flag) {
        if (!stat(ap, &sb) && sb.st_size > cursor) { FILE *f = fopen(ap, "r"); if (f) { fseek(f, cursor, SEEK_SET); char ln[1200]; while (fgets(ln, sizeof ln, f)) { ln[strcspn(ln, "\n")] = 0; if (ln[0]) do_cmd(ln); } cursor = ftell(f); fclose(f); } }
        if (child > 0) { int st; if (waitpid(child, &st, WNOHANG) == child) { child = 0; status = 0; snprintf(logline, sizeof logline, "finished"); if (cur + 1 < nq) play(cur + 1); } }
        publish(); usleep(300000);
    }
    stop_child(); return 0;
}
