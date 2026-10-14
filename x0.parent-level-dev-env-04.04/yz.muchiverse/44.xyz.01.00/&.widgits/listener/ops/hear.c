/* hear - speech to text, engine E0 (whisper.cpp tiny.en): hear <in.wav> [--data DIR] [--home DIR] [--prompt "hot words"] [--keep] [--threads N] [--consent owner|synthetic|none]   (slice L1, BOOK 19.audio/LISTENER-STT-DESIGN.md)
 * 1. converts the input to 16 kHz mono (ffmpeg) in a temp file;  2. speech gate: adsp_vad finds spans; NO speech = exit 3, nothing transcribed, nothing logged (whisper invents text such as "Thank you." on silence, so the gate is not optional);
 * 3. cuts [first start, last end] and runs <home>/bin/whisper-cli with LD_LIBRARY_PATH=<home>/lib and <home>/models/ggml-tiny.en.bin (fork + exec, 120 s watchdog, no popen);
 * 4. prints the transcript on stdout and appends  HEAR | ts | utterance file or - | whisper-tiny.en | transcript | seconds | ms to make | - | consent  to <data>/ledger.txt.
 * --keep stores the cut utterance as <data>/rec/<ts>.wav (default: NOT stored: recordings are private, see the design's privacy rails); --prompt passes whisper's initial prompt (hot words / vocabulary).
 * Defaults: home = ~/.local/share/house-listener/whisper (the install), data = ~/.local/share/house-listener (ledger, kept utterances), independent of each other and both OUTSIDE the repository on purpose. Exit 0 transcript made, 1 error, 2 usage, 3 no speech. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <math.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include "../../_shared-lib/audio_dsp.c"
#define PL 4096
static int run(char *const argv[], const char *outfile, int timeout_s, const char *ldpath) {
    pid_t p = fork(); if (p < 0) return -1;
    if (p == 0) { int nul = open("/dev/null", O_RDWR); int fd = outfile ? open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644) : nul; if (nul >= 0) dup2(nul, 0); if (fd >= 0) dup2(fd, 1); if (nul >= 0) dup2(nul, 2); if (ldpath) setenv("LD_LIBRARY_PATH", ldpath, 1); execvp(argv[0], argv); _exit(127); }
    for (int t = 0; t < timeout_s * 5; t++) { int st; pid_t r = waitpid(p, &st, WNOHANG); if (r == p) return WIFEXITED(st) ? WEXITSTATUS(st) : -1; usleep(200000); }
    kill(p, SIGKILL); waitpid(p, NULL, 0); return -1;
}
int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: hear in.wav [--data DIR] [--home DIR] [--prompt TEXT] [--keep] [--threads N] [--consent owner|synthetic|none]\n"); return 2; }
    const char *in = argv[1], *prompt = NULL, *consent = "none"; char data[PL], home[PL]; const char *hm = getenv("HOME"); snprintf(data, sizeof data, "%s/.local/share/house-listener", hm ? hm : "."); snprintf(home, sizeof home, "%s/.local/share/house-listener/whisper", hm ? hm : "."); int keep = 0, threads = 4;
    for (int i = 2; i < argc; i++) { if (!strcmp(argv[i], "--data") && i + 1 < argc) snprintf(data, sizeof data, "%s", argv[++i]); else if (!strcmp(argv[i], "--home") && i + 1 < argc) snprintf(home, sizeof home, "%s", argv[++i]); else if (!strcmp(argv[i], "--prompt") && i + 1 < argc) prompt = argv[++i]; else if (!strcmp(argv[i], "--keep")) keep = 1; else if (!strcmp(argv[i], "--threads") && i + 1 < argc) threads = atoi(argv[++i]); else if (!strcmp(argv[i], "--consent") && i + 1 < argc) consent = argv[++i]; else { fprintf(stderr, "unknown argument %s\n", argv[i]); return 2; } }
    if (strcmp(consent, "owner") && strcmp(consent, "synthetic") && strcmp(consent, "none")) { fprintf(stderr, "consent must be owner, synthetic or none\n"); return 2; } if (threads < 1 || threads > 16) threads = 4;
    char rec[PL], bin[PL], lib[PL], mdl[PL], tmp[PL], cut[PL], txt[PL]; snprintf(bin, sizeof bin, "%s/bin/whisper-cli", home); snprintf(lib, sizeof lib, "%s/lib", home); snprintf(mdl, sizeof mdl, "%s/models/ggml-tiny.en.bin", home); struct stat st;
    if (stat(bin, &st) || stat(mdl, &st)) { fprintf(stderr, "whisper not installed under %s (needs bin/whisper-cli, lib/*.so, models/ggml-tiny.en.bin)\n", home); return 1; }
    mkdir(data, 0700); snprintf(rec, sizeof rec, "%s/rec", data); if (keep) mkdir(rec, 0700); long pid = (long)getpid(); snprintf(tmp, sizeof tmp, "%s/tmp_%ld_16k.wav", data, pid); snprintf(cut, sizeof cut, "%s/tmp_%ld_cut.wav", data, pid); snprintf(txt, sizeof txt, "%s/tmp_%ld.txt", data, pid);
    struct timeval t0; gettimeofday(&t0, NULL);
    { char *a[] = { "ffmpeg", "-loglevel", "quiet", "-y", "-i", (char *)in, "-ar", "16000", "-ac", "1", "-c:a", "pcm_s16le", tmp, NULL }; if (run(a, NULL, 60, NULL) != 0 || stat(tmp, &st)) { fprintf(stderr, "cannot convert %s (ffmpeg)\n", in); unlink(tmp); return 1; } }
    short *s; long n; int sr; if (adsp_read_wav(tmp, &s, &n, &sr)) { unlink(tmp); fprintf(stderr, "cannot read converted audio\n"); return 1; }
    AdspSeg seg[64]; int k = adsp_vad(s, n, sr, 700, 150, seg, 64); if (!k) { free(s); unlink(tmp); fprintf(stderr, "no speech\n"); return 3; }
    long a0 = seg[0].start, a1 = seg[k - 1].end; adsp_write_wav(cut, s + a0, a1 - a0, sr); double secs = (double)(a1 - a0) / sr; free(s); unlink(tmp);
    char thr[16]; snprintf(thr, sizeof thr, "%d", threads); char *a[16]; int na = 0; a[na++] = bin; a[na++] = "-m"; a[na++] = mdl; a[na++] = "-f"; a[na++] = cut; a[na++] = "-nt"; a[na++] = "-np"; a[na++] = "-t"; a[na++] = thr; if (prompt) { a[na++] = "--prompt"; a[na++] = (char *)prompt; } a[na] = NULL;
    int rc = run(a, txt, 120, lib); char text[2048] = ""; FILE *f = fopen(txt, "r"); if (f) { char ln[1024]; while (fgets(ln, sizeof ln, f)) { ln[strcspn(ln, "\r\n")] = 0; char *p = ln; while (*p == ' ') p++; if (*p && strlen(text) + strlen(p) + 2 < sizeof text) { if (text[0]) strcat(text, " "); strcat(text, p); } } fclose(f); } unlink(txt);
    struct timeval t1; gettimeofday(&t1, NULL); long ms = (t1.tv_sec - t0.tv_sec) * 1000 + (t1.tv_usec - t0.tv_usec) / 1000;
    if (rc != 0) { unlink(cut); fprintf(stderr, "whisper failed (exit %d)\n", rc); return 1; }
    char kept[PL] = "-"; long ts = (long)time(NULL); if (keep) { snprintf(kept, sizeof kept, "%s/%ld.wav", rec, ts); if (rename(cut, kept)) { snprintf(kept, sizeof kept, "-"); unlink(cut); } } else unlink(cut);
    for (char *p = text; *p; p++) if (*p == '|') *p = '/';
    { char lp[PL]; snprintf(lp, sizeof lp, "%s/ledger.txt", data); FILE *lf = fopen(lp, "a"); if (lf) { fprintf(lf, "HEAR | %ld | %s | whisper-tiny.en | %s | %.2f | %ld | - | %s\n", ts, kept, text, secs, ms, consent); fclose(lf); } }
    printf("%s\n", text); return 0;
}
