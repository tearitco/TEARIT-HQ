#define _GNU_SOURCE
/* video_op - slides and narrated videos WITHOUT Python (owner 2026-11-04: no Python; replaces presentation/make_deck.py, remote_tts.py and the PIL part of make_presentation_video.py).
 *   video_op deck   <deck.txt> <outdir>              1280x720 PNG slides -> <outdir>/snapshots/NN_*.png and <outdir>/manifest.txt
 *   video_op render <outdir> [--captions] [--no-tts]  <outdir>/manifest.txt -> <outdir>/video.mp4 (one frame per row, held while its narration plays)
 * Deck file (fields split by '|', bullets by ';', '#' = comment):  TITLE | title | subtitle | narration      SLIDE | heading | bullet ; bullet ; ... | narration
 * Manifest (one row per snapshot, playback order):  file | seconds | caption      (the caption is the narration text; with --captions it is also drawn on the frame, for window shots)
 * Pictures and video are made by ffmpeg (drawtext, concat, apad) and the audio length by ffprobe: external programs run with fork/exec, no shell. Narration: the wav of each caption comes from the Kokoro
 * service on another machine over ssh (the machine is named by PRESENT_TTS_HOST, user@host: never hardcoded here; heavy work goes off the desktop). Kokoro is Python on THAT machine: an outside service, not our code.
 * Without PRESENT_TTS_HOST (or with --no-tts) the video is silent. Every ffmpeg runs under `nice -n 15`; start this op itself behind cpu_gate (harness case does).
 * Exit: 0 ok, 1 a step failed (stderr says which), 2 usage. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <limits.h>
#define P 4096
#define MAXR 64
#define FB "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
#define FR "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"

static int run(char *const argv[], const char *outfile) {
    pid_t pid = fork(); if (pid < 0) return -1;
    if (pid == 0) { if (outfile) { int fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644); if (fd < 0) _exit(126); dup2(fd, 1); close(fd); } else { int fd = open("/dev/null", O_WRONLY); dup2(fd, 1); close(fd); }
        execvp(argv[0], argv); _exit(127); }
    int st = 0; waitpid(pid, &st, 0); return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}
static void wfile(const char *p, const char *t) { FILE *f = fopen(p, "w"); if (f) { fputs(t, f); fclose(f); } }
static int wrap(const char *s, int width, char lines[][200], int max) {      /* word wrap into lines of at most `width` characters */
    int n = 0; const char *p = s; while (*p && n < max) { while (*p == ' ') p++; int len = (int)strlen(p); int take = len < width ? len : width;
        if (take < len) { int sp = take; while (sp > width / 2 && p[sp] != ' ') sp--; if (sp > width / 2) take = sp; }
        snprintf(lines[n], 200, "%.*s", take, p); n++; p += take; }
    return n;
}
static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; return s; }
static void mk(const char *d) { mkdir(d, 0755); }

/* one slide: drawtext rows (textfile per row, so no escaping of the content) over a dark background */
static int slide(int title, const char *head, const char *sub, char bullets[][200], int nb, int n, int total, const char *tmp, const char *out) {
    char flt[8192] = ""; size_t fl = 0; int k = 0; char tf[P];
#define DT(txt, font, size, col, x, y) do { snprintf(tf, sizeof tf, "%s/t%d.txt", tmp, k++); wfile(tf, txt); fl += (size_t)snprintf(flt + fl, sizeof flt - fl, ",drawtext=fontfile=%s:textfile=%s:fontsize=%d:fontcolor=%s:x=%d:y=%d", font, tf, size, col, x, y); } while (0)
    fl += (size_t)snprintf(flt + fl, sizeof flt - fl, "drawbox=x=0:y=0:w=1280:h=10:color=0x5aa0ff:t=fill");
    char foot[160]; snprintf(foot, sizeof foot, "LyfeDesk  -  proposed plan  -  %d / %d", n, total); DT(foot, FR, 18, "0x788296", 48, 680);
    if (title) { char L[6][200]; int nl = wrap(head, 26, L, 6); for (int i = 0; i < nl; i++) DT(L[i], FB, 66, "0xebf0ff", 70, 210 + i * 78); nl = wrap(sub, 60, L, 4); for (int i = 0; i < nl; i++) DT(L[i], FR, 30, "0x96beff", 74, 450 + i * 40); }
    else { DT(head, FB, 46, "0xebf0ff", 60, 44); fl += (size_t)snprintf(flt + fl, sizeof flt - fl, ",drawbox=x=60:y=112:w=1160:h=3:color=0x3c5078:t=fill"); int y = 140;
        for (int b = 0; b < nb; b++) { char L[6][200]; int nl = wrap(bullets[b], 66, L, 6); fl += (size_t)snprintf(flt + fl, sizeof flt - fl, ",drawbox=x=66:y=%d:w=14:h=14:color=0x5aa0ff:t=fill", y + 14); for (int i = 0; i < nl; i++) { DT(L[i], FR, 31, "0xdce2f0", 104, y); y += 42; } y += 16; } }
    char *av[] = { "nice", "-n", "15", "ffmpeg", "-y", "-loglevel", "error", "-f", "lavfi", "-i", "color=c=0x121622:s=1280x720", "-frames:v", "1", "-vf", flt, (char *)out, NULL };
    return run(av, NULL);
}
static int cmd_deck(const char *deck, const char *outdir) {
    char snaps[P], mpath[P], ln[8192]; snprintf(snaps, sizeof snaps, "%s/snapshots", outdir); mk(outdir); mk(snaps); snprintf(mpath, sizeof mpath, "%s/manifest.txt", outdir);
    FILE *f = fopen(deck, "r"); if (!f) { fprintf(stderr, "video_op: cannot read %s\n", deck); return 1; }
    static char rows[MAXR][8192]; int nr = 0; while (fgets(ln, sizeof ln, f) && nr < MAXR) { char *t = trim(ln); if (!*t || *t == '#') continue; snprintf(rows[nr++], sizeof rows[0], "%s", t); } fclose(f);
    FILE *m = fopen(mpath, "w"); if (!m) return 1; char tmp[P]; snprintf(tmp, sizeof tmp, "%s/.t", outdir); mk(tmp);
    for (int i = 0; i < nr; i++) { char *fld[4] = { 0 }, *p = rows[i]; int nf = 0; while (nf < 4) { fld[nf++] = p; char *b = strchr(p, '|'); if (!b || nf == 4) break; *b = 0; p = b + 1; }
        for (int k = 0; k < nf; k++) { fld[k] = trim(fld[k]); }
        if (nf < 4) { fprintf(stderr, "video_op: row %d needs 4 fields\n", i + 1); fclose(m); return 1; }
        char name[96]; int w = 0; for (const char *c = fld[1]; *c && w < 30; c++) name[w++] = (*c >= '0' && *c <= '9') || (*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') ? *c : '_'; name[w] = 0;
        char fn[160], out[P]; snprintf(fn, sizeof fn, "%02d_%s.png", i + 1, name); snprintf(out, sizeof out, "%s/%s", snaps, fn);
        static char B[8][200]; int nb = 0; int title = !strcmp(fld[0], "TITLE");
        if (!title) { char tmpb[4096]; snprintf(tmpb, sizeof tmpb, "%s", fld[2]); for (char *t = strtok(tmpb, ";"); t && nb < 8; t = strtok(NULL, ";")) { t = trim(t); if (*t) snprintf(B[nb++], 200, "%s", t); } }
        if (slide(title, fld[1], title ? fld[2] : "", B, nb, i + 1, nr, tmp, out) != 0) { fprintf(stderr, "video_op: ffmpeg failed on slide %d\n", i + 1); fclose(m); return 1; }
        for (char *c = fld[3]; *c; c++) { if (*c == '|') *c = '/'; }
        fprintf(m, "%s | 6 | %s\n", fn, fld[3]); }
    fclose(m); printf("slides=%d dir=%s\n", nr, outdir); return 0;
}
static void b64(const char *s, char *o) { static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"; size_t n = strlen(s), j = 0;
    for (size_t i = 0; i < n; i += 3) { unsigned v = (unsigned char)s[i] << 16 | (i + 1 < n ? (unsigned char)s[i + 1] << 8 : 0) | (i + 2 < n ? (unsigned char)s[i + 2] : 0); o[j++] = T[v >> 18 & 63]; o[j++] = T[v >> 12 & 63]; o[j++] = i + 1 < n ? T[v >> 6 & 63] : '='; o[j++] = i + 2 < n ? T[v & 63] : '='; } o[j] = 0; }
static double wav_len(const char *wav, const char *tmp) {
    char o[P]; snprintf(o, sizeof o, "%s/dur.txt", tmp); char *av[] = { "ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", (char *)wav, NULL };
    if (run(av, o) != 0) return 0; FILE *f = fopen(o, "r"); double d = 0; if (f) { if (fscanf(f, "%lf", &d) != 1) d = 0; fclose(f); } return d;
}
static int cmd_render(const char *outdir, int captions, int notts) {
    char mpath[P], tmp[P], ln[8192]; snprintf(mpath, sizeof mpath, "%s/manifest.txt", outdir); snprintf(tmp, sizeof tmp, "%s/.t", outdir); mk(tmp);
    FILE *f = fopen(mpath, "r"); if (!f) { fprintf(stderr, "video_op: no %s\n", mpath); return 1; }
    static char file[MAXR][160], cap[MAXR][2048]; static double secs[MAXR], hold[MAXR]; int nr = 0;
    while (fgets(ln, sizeof ln, f) && nr < MAXR) { char *t = trim(ln); if (!*t || *t == '#') continue; char *a = strchr(t, '|'); if (!a) continue; *a = 0; char *b = strchr(a + 1, '|'); if (!b) continue; *b = 0;
        snprintf(file[nr], sizeof file[0], "%s", trim(t)); secs[nr] = atof(a + 1); if (secs[nr] <= 0) secs[nr] = 6; snprintf(cap[nr], sizeof cap[0], "%s", trim(b + 1)); nr++; }
    fclose(f); if (!nr) { fprintf(stderr, "video_op: manifest has no rows\n"); return 1; }
    const char *host = getenv("PRESENT_TTS_HOST"); int tts = !notts && host && host[0];
    if (!notts && !tts) fprintf(stderr, "video_op: PRESENT_TTS_HOST is not set: making a SILENT video\n");
    for (int i = 0; i < nr; i++) {
        char src[P], dst[P], wav[P], tf[P]; snprintf(src, sizeof src, "%s/snapshots/%s", outdir, file[i]); snprintf(dst, sizeof dst, "%s/f%02d.png", tmp, i); snprintf(wav, sizeof wav, "%s/n%02d.wav", tmp, i);
        if (access(src, R_OK) != 0) { fprintf(stderr, "video_op: manifest names a missing snapshot: %s\n", src); return 1; }
        char flt[2048]; size_t fl = (size_t)snprintf(flt, sizeof flt, "scale=1280:-2");
        if (captions && cap[i][0]) { char L[8][200]; int nl = wrap(cap[i], 78, L, 8); char txt[2048] = ""; for (int k = 0; k < nl; k++) { strncat(txt, L[k], sizeof txt - strlen(txt) - 2); if (k + 1 < nl) strcat(txt, "\n"); }
            snprintf(tf, sizeof tf, "%s/c%02d.txt", tmp, i); wfile(tf, txt); snprintf(flt + fl, sizeof flt - fl, ",drawtext=fontfile=%s:textfile=%s:fontsize=22:fontcolor=white:box=1:boxcolor=black@0.65:boxborderw=10:x=(w-text_w)/2:y=h-text_h-24", FR, tf); }
        char *av[] = { "nice", "-n", "15", "ffmpeg", "-y", "-loglevel", "error", "-i", src, "-vf", flt, "-frames:v", "1", dst, NULL };
        if (run(av, NULL) != 0) { fprintf(stderr, "video_op: ffmpeg could not prepare frame %d\n", i + 1); return 1; }
        hold[i] = secs[i];
        if (tts && cap[i][0]) { static char e[4096]; b64(cap[i], e); char rc[4400]; snprintf(rc, sizeof rc, "cd ~/house-tts && PYTHONPATH=lib nice -n 10 python3 one_line.py %s af_heart 4", e);
            char *tv[] = { "ssh", "-n", "-o", "BatchMode=yes", "-o", "ConnectTimeout=6", (char *)host, rc, NULL };
            if (run(tv, wav) != 0) { fprintf(stderr, "video_op: narration failed for row %d (ssh to %s)\n", i + 1, host); return 1; }
            FILE *w = fopen(wav, "rb"); char h[4] = ""; if (w) { if (fread(h, 1, 4, w) != 4) h[0] = 0; fclose(w); } if (strncmp(h, "RIFF", 4)) { fprintf(stderr, "video_op: row %d: the narration machine returned no wav\n", i + 1); return 1; }
            double d = wav_len(wav, tmp); if (d + 0.6 > hold[i]) hold[i] = d + 0.6; }
        else wav[0] = 0; (void)wav; }
    char lst[P]; snprintf(lst, sizeof lst, "%s/list.txt", tmp); FILE *l = fopen(lst, "w"); if (!l) return 1;
    for (int i = 0; i < nr; i++) fprintf(l, "file '%s/f%02d.png'\nduration %.2f\n", tmp, i, hold[i]); fprintf(l, "file '%s/f%02d.png'\n", tmp, nr - 1); fclose(l);      /* the concat demuxer ignores the last duration: repeat the last file */
    char *av[200]; int ac = 0; char out[P], flc[8192] = ""; snprintf(out, sizeof out, "%s/video.mp4", outdir); size_t fl = 0; int na = 0;
    av[ac++] = "nice"; av[ac++] = "-n"; av[ac++] = "15"; av[ac++] = "ffmpeg"; av[ac++] = "-y"; av[ac++] = "-loglevel"; av[ac++] = "error"; av[ac++] = "-f"; av[ac++] = "concat"; av[ac++] = "-safe"; av[ac++] = "0"; av[ac++] = "-i"; av[ac++] = lst;
    static char wavs[MAXR][P]; char cat[2048] = "";
    if (tts) { for (int i = 0; i < nr; i++) { snprintf(wavs[i], sizeof wavs[0], "%s/n%02d.wav", tmp, i); if (access(wavs[i], R_OK) != 0 || !cap[i][0]) continue; av[ac++] = "-i"; av[ac++] = wavs[i]; na++;
            fl += (size_t)snprintf(flc + fl, sizeof flc - fl, "[%d:a]apad=whole_dur=%.2f[a%d];", na, hold[i], na - 1); char one[16]; snprintf(one, sizeof one, "[a%d]", na - 1); strncat(cat, one, sizeof cat - strlen(cat) - 1); }
        if (na) { snprintf(flc + fl, sizeof flc - fl, "%sconcat=n=%d:v=0:a=1[aout]", cat, na); av[ac++] = "-filter_complex"; av[ac++] = flc; av[ac++] = "-map"; av[ac++] = "0:v"; av[ac++] = "-map"; av[ac++] = "[aout]"; av[ac++] = "-c:a"; av[ac++] = "aac"; } }
    static char tot[24]; { double sum = 0; for (int i = 0; i < nr; i++) sum += hold[i]; snprintf(tot, sizeof tot, "%.2f", sum); }
    av[ac++] = "-t"; av[ac++] = tot; av[ac++] = "-r"; av[ac++] = "25";      /* the concat demuxer gives the repeated last file an unspecified length: cut at the exact sum of the holds */ av[ac++] = "-c:v"; av[ac++] = "libx264"; av[ac++] = "-pix_fmt"; av[ac++] = "yuv420p"; av[ac++] = out; av[ac] = NULL;      /* no -shortest: both streams are exactly the sum of the holds */
    if (run(av, NULL) != 0) { fprintf(stderr, "video_op: the final ffmpeg failed\n"); return 1; }
    printf("video=%s rows=%d narrated=%d\n", out, nr, na); return 0;
}
int main(int argc, char **argv) {
    if (argc >= 4 && !strcmp(argv[1], "deck")) return cmd_deck(argv[2], argv[3]);
    if (argc >= 3 && !strcmp(argv[1], "render")) { int c = 0, n = 0; for (int i = 3; i < argc; i++) { if (!strcmp(argv[i], "--captions")) c = 1; else if (!strcmp(argv[i], "--no-tts")) n = 1; } return cmd_render(argv[2], c, n); }
    fprintf(stderr, "usage: video_op deck <deck.txt> <outdir> | render <outdir> [--captions] [--no-tts]\n"); return 2;
}
