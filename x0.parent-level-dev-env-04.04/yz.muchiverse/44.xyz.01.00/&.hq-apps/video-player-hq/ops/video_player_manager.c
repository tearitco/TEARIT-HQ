/* video_player_manager.c - backend of video-player-hq: a queue you drop video files onto, played by an ffplay child, with a file check.
 *   run:    video_player_manager <house_root> <package_dir>      publishes video_player_ui.txt, reads video_player_action.txt by cursor
 *   probe:  video_player_manager --probe <file>                  prints the file check (harness hook, no window)
 * Why a check: a presentation video that played nowhere turned out to hold 3 video frames over 23 s (variable frame rate); the check says so.
 * Playback is IN the window: ffmpeg decodes the picture to raw RGBA frames (10 fps, fitted into 1100x680, or 1420x690 with the left panel folded, keeping the aspect) that the manager writes atomically to
 * <package_dir>/frame.raw + frame.receipt.txt, which the generic <canvas> blits (canvas_raw var, ZERO renderer C); the sound is a separate
 * `ffplay -nodisp` child started with the picture. Pause stops both and remembers the second; Resume and Seek restart both at that second
 * (so picture and sound stay together). All fork+exec, no shell; the children are stopped when the manager quits. */
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/wait.h>
#include <poll.h>
#include <sys/stat.h>

#define MAXQ 64
static char pkg[1024], queue[MAXQ][1024]; static int nq, cur = -1;
static int status;                       /* 0 stopped, 1 playing, 2 paused */
#define NTH 12                       /* scene thumbnails along the timeline */
static double dur; static pid_t tchild; static char thumbs_for[1024];
static int side_on = 1;                  /* left panel shown; the footer button folds it away so the picture can use the width */
#define BOXW 1100          /* the centre panel's pixel box: the picture is fitted into it, aspect kept, so it fills the screen area */
#define BOXH 680
#define BIGW 1420         /* with the left panel folded away the picture gets the whole width */
#define BIGH 690
static int fw = BOXW, fh = BOXH, vid_w, vid_h;
static pid_t child, achild; static int vfd = -1; static unsigned char fbuf[BIGW * BIGH * 4]; static size_t fgot;
static time_t t_start; static double base_off, pos_at_pause;
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
    ninfo = 0; char out[4096], v[128], w[64], h[64], fps[64], vc[64], ac[64], durs[64], nf[64];
    char *const a[] = { "ffprobe", "-v", "error", "-show_entries", "format=duration:stream=codec_type,codec_name,width,height,avg_frame_rate,nb_frames", "-of", "default=nw=1", (char *)file, NULL };
    if (capture(a, out, sizeof out) != 0 || !out[0]) { snprintf(info[ninfo++], 200, "cannot read this file (ffprobe failed): not a video, or damaged"); return; }
    /* stream blocks come in file order: the first codec_name belongs to the first stream; find which is video by codec_type order */
    char t0[32], t1[32]; kv(out, "codec_type", t0, sizeof t0, 0); kv(out, "codec_type", t1, sizeof t1, 1);
    int vi = !strcmp(t0, "video") ? 0 : 1, ai = vi ? 0 : 1;
    kv(out, "codec_name", vc, sizeof vc, vi); kv(out, "codec_name", ac, sizeof ac, ai); kv(out, "width", w, sizeof w, 0); kv(out, "height", h, sizeof h, 0);
    kv(out, "avg_frame_rate", fps, sizeof fps, vi); kv(out, "nb_frames", nf, sizeof nf, vi); kv(out, "duration", durs, sizeof durs, 0);
    double d = atof(durs); long frames = atol(nf); vid_w = atoi(w); vid_h = atoi(h); dur = d;
    snprintf(info[ninfo++], 200, "video %s %sx%s   audio %s   %.1f s   avg fps %s   frames %s", vc[0] ? vc : "none", w, h, !strcmp(t0, "audio") || !strcmp(t1, "audio") ? ac : "none", d, fps, nf);
    (void)v;
    if (!vc[0]) snprintf(info[ninfo++], 200, "WARNING no video stream");
    else if (frames > 0 && d > 5 && (double)frames / d < 1.0) snprintf(info[ninfo++], 200, "WARNING only %ld video frames over %.0f s: sparse variable frame rate, some players stall or show nothing; re-encode at a constant frame rate", frames, d);
    else snprintf(info[ninfo++], 200, "check ok: frame rate looks regular");
}
static void kill_one(pid_t *p) {      /* never block: a decoder stuck writing to our pipe ignores SIGTERM until it is read or the pipe closes */
    if (*p <= 0) return; int st; kill(*p, SIGCONT); kill(*p, SIGTERM);
    for (int i = 0; i < 15; i++) { if (waitpid(*p, &st, WNOHANG) == *p) { *p = 0; return; } usleep(20000); }
    kill(*p, SIGKILL); waitpid(*p, &st, 0); *p = 0;
}
static void stop_child(void) { if (vfd >= 0) { close(vfd); vfd = -1; } kill_one(&child); kill_one(&achild); fgot = 0; status = 0; base_off = 0; }
static double elapsed(void) { return status == 1 ? base_off + difftime(time(NULL), t_start) : status == 2 ? pos_at_pause : 0; }
static void gen_thumbs(const char *file) {       /* NTH evenly spaced scene thumbnails as PNGs the generic items can draw; one low-priority ffmpeg per video */
    if (!strcmp(thumbs_for, file) || dur <= 0) return; snprintf(thumbs_for, sizeof thumbs_for, "%s", file);
    if (tchild > 0) { kill(tchild, SIGKILL); int st; waitpid(tchild, &st, 0); tchild = 0; }
    char dir[1536], pat[1600], vf[96]; snprintf(dir, sizeof dir, "%s/thumbs", pkg); mkdir(dir, 0755);
    for (int i = 1; i <= NTH + 2; i++) { snprintf(pat, sizeof pat, "%s/t_%02d.png", dir, i); unlink(pat); }
    snprintf(pat, sizeof pat, "%s/t_%%02d.png", dir); snprintf(vf, sizeof vf, "fps=%.6f,scale=100:56", NTH / dur);
    tchild = fork(); if (tchild == 0) { setpgid(0, 0); int nul = open("/dev/null", O_RDWR); dup2(nul, 0); dup2(nul, 1); dup2(nul, 2); nice(15);
        execlp("ffmpeg", "ffmpeg", "-v", "error", "-y", "-i", file, "-an", "-vf", vf, "-frames:v", "12", pat, (char *)NULL); _exit(127); }
}
static void write_receipt(void) { char p[1536]; snprintf(p, sizeof p, "%s/frame.receipt.txt", pkg); FILE *f = fopen(p, "w"); if (f) { fprintf(f, "frame_w=%d\nframe_h=%d\n", fw, fh); fclose(f); } }
static void play_at(int i, double off) {
    if (i < 0 || i >= nq) return; stop_child(); cur = i; if (off <= 0) check(queue[i]);
    gen_thumbs(queue[i]);
    if (vid_w > 0 && vid_h > 0) { int bw = side_on ? BOXW : BIGW, bh = side_on ? BOXH : BIGH; double k = (double)bw / vid_w, k2 = (double)bh / vid_h; if (k2 < k) k = k2; fw = (int)(vid_w * k) & ~1; fh = (int)(vid_h * k) & ~1; if (fw < 2) fw = 2; if (fh < 2) fh = 2; } else { fw = side_on ? BOXW : BIGW; fh = side_on ? BOXH : BIGH; }
    write_receipt();
    char ss[32], vf[160]; snprintf(ss, sizeof ss, "%.2f", off < 0 ? 0 : off);
    snprintf(vf, sizeof vf, "fps=10,scale=%d:%d", fw, fh);
    int fd[2]; if (pipe(fd)) return;
    child = fork(); if (child == 0) { setpgid(0, 0); int nul = open("/dev/null", O_RDWR); dup2(nul, 0); dup2(nul, 2); dup2(fd[1], 1); close(fd[0]); close(fd[1]);
        execlp("ffmpeg", "ffmpeg", "-v", "error", "-re", "-ss", ss, "-i", queue[i], "-an", "-vf", vf, "-f", "rawvideo", "-pix_fmt", "rgba", "pipe:1", (char *)NULL); _exit(127); }
    close(fd[1]); vfd = fd[0];
#ifdef F_SETPIPE_SZ
    fcntl(vfd, F_SETPIPE_SZ, 1 << 20);      /* the default 64 KB pipe holds a fraction of one frame; a bigger one lets the decoder run ahead */
#endif
    fcntl(vfd, F_SETFL, fcntl(vfd, F_GETFL) | O_NONBLOCK); fgot = 0;
    achild = fork(); if (achild == 0) { setpgid(0, 0); int nul = open("/dev/null", O_RDWR); dup2(nul, 0); dup2(nul, 1); dup2(nul, 2); close(vfd);
        execlp("ffplay", "ffplay", "-nodisp", "-vn", "-autoexit", "-loglevel", "error", "-ss", ss, queue[i], (char *)NULL); _exit(127); }
    status = 1; t_start = time(NULL); base_off = off < 0 ? 0 : off; snprintf(logline, sizeof logline, "playing %s", strrchr(queue[i], '/') ? strrchr(queue[i], '/') + 1 : queue[i]);
}
static void play(int i) { play_at(i, 0); }
static void pump_frames(void) {          /* read what the decoder has produced; publish each whole frame atomically */
    if (vfd < 0) return; ssize_t r;
    while ((r = read(vfd, fbuf + fgot, (size_t)fw * fh * 4 - fgot)) > 0) { fgot += (size_t)r;
        if (fgot == (size_t)fw * fh * 4) { char p[1536], tmp[1600]; snprintf(p, sizeof p, "%s/frame.raw", pkg); snprintf(tmp, sizeof tmp, "%s.tmp", p);
            FILE *f = fopen(tmp, "wb"); if (f) { fwrite(fbuf, 1, (size_t)fw * fh * 4, f); fclose(f); rename(tmp, p); } fgot = 0; } }
}
static void do_cmd(char *line) {
    char *sp = strchr(line, ' '); char *arg = sp ? sp + 1 : ""; if (sp) *sp = 0;
    if (!strcmp(line, "add")) { if (nq < MAXQ && arg[0]) { struct stat sb; if (stat(arg, &sb) || !S_ISREG(sb.st_mode)) { snprintf(logline, sizeof logline, "not a file: %s", arg); return; }
            snprintf(queue[nq], sizeof queue[0], "%s", arg); nq++; cur = nq - 1; check(queue[cur]); snprintf(logline, sizeof logline, "added %s", arg); if (status == 0) play(cur); } }
    else if (!strcmp(line, "play")) { int i = (arg[0] >= '0' && arg[0] <= '9') ? atoi(arg) : (cur >= 0 ? cur : 0); play(i); }
    else if (!strcmp(line, "pause") && status == 1) { double e = elapsed(); if (vfd >= 0) { close(vfd); vfd = -1; } kill_one(&child); kill_one(&achild); status = 2; pos_at_pause = e; }
    else if (!strcmp(line, "resume") && status == 2) { play_at(cur, pos_at_pause); }
    else if ((!strcmp(line, "fwd") || !strcmp(line, "back")) && cur >= 0) { double e = elapsed() + (line[0] == 'f' ? 10 : -10); if (e < 0) e = 0; play_at(cur, e); }
    else if (!strcmp(line, "stop")) { stop_child(); snprintf(logline, sizeof logline, "stopped"); }
    else if (!strcmp(line, "next")) { if (cur + 1 < nq) play(cur + 1); }
    else if (!strcmp(line, "prev")) { if (cur > 0) play(cur - 1); }
    else if (!strcmp(line, "seek") && cur >= 0) { double e = atof(arg); if (e < 0) e = 0; if (dur > 0 && e > dur - 1) e = dur - 1; play_at(cur, e); }
    else if ((!strcmp(line, "scene+") || !strcmp(line, "scene-")) && cur >= 0 && dur > 0) { double sl = dur / NTH; int k = (int)(elapsed() / sl) + (line[5] == '+' ? 1 : -1); if (k < 0) k = 0; if (k >= NTH) k = NTH - 1; play_at(cur, k * sl); }
    else if (!strcmp(line, "side")) { side_on = !side_on; if (status == 1 && cur >= 0) play_at(cur, elapsed()); }
    else if (!strcmp(line, "clear")) { stop_child(); nq = 0; cur = -1; ninfo = 0; snprintf(logline, sizeof logline, "queue cleared"); }
}
static void publish(void) {
    char dst[1536], tmp[1600]; snprintf(dst, sizeof dst, "%s/video_player_ui.txt", pkg); snprintf(tmp, sizeof tmp, "%s.tmp", dst);
    FILE *f = fopen(tmp, "w"); if (!f) return; const char *st[] = { "stopped", "playing", "paused" };
    double el = elapsed();
    fprintf(f, "head=Video Player  ·  %s\n", st[status]); fprintf(f, "status=%s%s%s   %d:%02d elapsed\n", st[status], cur >= 0 ? "   " : "", cur >= 0 ? (strrchr(queue[cur], '/') ? strrchr(queue[cur], '/') + 1 : queue[cur]) : "", (int)el / 60, (int)el % 60);
    fprintf(f, "canvas_raw=%s/frame.raw\nside_on=%s\nside_cls=%s\nside_label=%s\n", pkg, side_on ? "1" : "", side_on ? "vp-sidebar" : "vp-sidebar-min", side_on ? "hide panel" : "show panel"); fprintf(f, "hint=Drop a video file onto this window to add and play it. Numbers are the row numbers to press.\nlog=%s\n", logline);
    { char bar[48]; int fill = dur > 0 ? (int)(el / dur * 30) : 0; if (fill > 30) fill = 30; for (int k = 0; k < 30; k++) bar[k] = k < fill ? '#' : '-'; bar[30] = 0;
      fprintf(f, "prog=[%s] %d:%02d / %d:%02d\n", bar, (int)el / 60, (int)el % 60, (int)dur / 60, (int)dur % 60); }
    { int nth = dur > 0 && cur >= 0 ? NTH : 0; double sl = dur > 0 ? dur / NTH : 1; int now = (int)(el / sl); fprintf(f, "n_th=%d\n", nth);
      for (int k = 0; k < nth; k++) { int s = (int)(k * sl); fprintf(f, "th_%d_sprite=%s/thumbs/t_%02d.png\nth_%d_text=%d:%02d\nth_%d_cls=%s\nth_%d_act=seek %d\n", k, pkg, k + 1, k, s / 60, s % 60, k, k == now ? "th-cur" : "th", k, s); } }
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
        pump_frames();
        if (tchild > 0) { int st; if (waitpid(tchild, &st, WNOHANG) == tchild) tchild = 0; }
        if (child > 0) { int st; if (waitpid(child, &st, WNOHANG) == child) { child = 0; pump_frames(); int nxt = cur + 1 < nq; stop_child(); snprintf(logline, sizeof logline, "finished"); if (nxt) play(cur + 1); } }
        { static struct timespec lastpub; struct timespec nw; clock_gettime(CLOCK_MONOTONIC, &nw);
          if ((nw.tv_sec - lastpub.tv_sec) * 1000 + (nw.tv_nsec - lastpub.tv_nsec) / 1000000 >= 300) { publish(); lastpub = nw; } }
        if (status == 1 && vfd >= 0) { struct pollfd pf = { vfd, POLLIN, 0 }; poll(&pf, 1, 40); }   /* wake as soon as the decoder has data (frames were limited to ~1/s by a fixed sleep) */
        else usleep(status == 1 ? 40000 : 100000);
    }
    stop_child(); if (tchild > 0) { kill(tchild, SIGKILL); int st; waitpid(tchild, &st, 0); } return 0;
}
