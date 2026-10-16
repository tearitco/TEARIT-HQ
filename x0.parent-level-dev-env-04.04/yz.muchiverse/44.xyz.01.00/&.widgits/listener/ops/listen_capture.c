/* listen_capture - one armed listening session: listen_capture <out.wav> [--max-s 30] [--wait-s 15] [--hang-ms 700] [--file in.wav] [--data DIR] [--arm button|space|file]     (slice L0, BOOK 19.audio/LISTENER-STT-DESIGN.md)
 * Opens the audio source, watches the level in 100 ms chunks, and returns ONE utterance: waits for speech (up to --wait-s), records it, and stops after --hang-ms of quiet (a real pause, not a timer) or --max-s.
 * Source: default `parecord --raw --format=s16le --rate=16000 --channels=1` (THE MICROPHONE: only when this op is run, which only a person's arm action does); `--file x.wav` streams a file in real time through
 * `ffmpeg -re` instead (used for tests: no microphone involved). Streaming detector: noise floor = the quietest level seen (0.6 s of calibration at the start, then it rises 0.02 dB per chunk), threshold = max(floor+9 dB, -50 dBFS),
 * speech starts after 2 chunks above it, with 500 ms of pre-roll kept so the first word is not clipped; the file keeps pre-roll + speech + 100 ms.
 * INDICATOR: while the source is open the file <data>/listening.txt holds this op's pid (a window shows "listening" from it); it is removed on exit. AUDIT: one row per session in <data>/ledger.txt:
 *   LISTEN | ts | arm source | seconds open | utterances (0 or 1) | note
 * Exit 0 = utterance written (its path is printed), 3 = no speech heard, 1 = could not open the source, 2 = usage. Never stores audio unless it heard speech. Data default ~/.local/share/house-listener. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>
#include <math.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "../../_shared-lib/audio_dsp.c"
#define PL 4096
#define SR 16000
#define CH 1600       /* 100 ms of samples */
static pid_t g_child; static char g_mark[PL];
static void cleanup(int s) { (void)s; if (g_child > 0) { kill(g_child, SIGTERM); } if (g_mark[0]) unlink(g_mark); _exit(1); }
int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: listen_capture out.wav [--max-s N] [--wait-s N] [--hang-ms N] [--file in.wav] [--data DIR] [--arm button|space|file]\n"); return 2; }
    const char *out = argv[1], *file = NULL, *arm = "button"; char data[PL]; const char *hm = getenv("HOME"); snprintf(data, sizeof data, "%s/.local/share/house-listener", hm ? hm : "."); int max_s = 30, wait_s = 15, hang_ms = 700;
    for (int i = 2; i < argc; i++) { if (!strcmp(argv[i], "--max-s") && i + 1 < argc) max_s = atoi(argv[++i]); else if (!strcmp(argv[i], "--wait-s") && i + 1 < argc) wait_s = atoi(argv[++i]); else if (!strcmp(argv[i], "--hang-ms") && i + 1 < argc) hang_ms = atoi(argv[++i]); else if (!strcmp(argv[i], "--file") && i + 1 < argc) { file = argv[++i]; arm = "file"; } else if (!strcmp(argv[i], "--data") && i + 1 < argc) snprintf(data, sizeof data, "%s", argv[++i]); else if (!strcmp(argv[i], "--arm") && i + 1 < argc) arm = argv[++i]; else { fprintf(stderr, "unknown argument %s\n", argv[i]); return 2; } }
    if (max_s < 1 || max_s > 120 || wait_s < 1 || wait_s > 120 || hang_ms < 200 || hang_ms > 5000) { fprintf(stderr, "max-s 1..120, wait-s 1..120, hang-ms 200..5000\n"); return 2; }
    mkdir(data, 0700); snprintf(g_mark, sizeof g_mark, "%s/listening.txt", data); int pfd[2]; if (pipe(pfd)) return 1; time_t t0 = time(NULL);
    g_child = fork(); if (g_child < 0) return 1;
    if (g_child == 0) { dup2(pfd[1], 1); close(pfd[0]); close(pfd[1]); int nul = open("/dev/null", O_RDWR); if (nul >= 0) { dup2(nul, 0); dup2(nul, 2); }
        if (file) execlp("ffmpeg", "ffmpeg", "-loglevel", "quiet", "-re", "-i", file, "-f", "s16le", "-ar", "16000", "-ac", "1", "-", (char *)NULL); else execlp("parecord", "parecord", "--raw", "--format=s16le", "--rate=16000", "--channels=1", (char *)NULL); _exit(127); }
    close(pfd[1]); signal(SIGTERM, cleanup); signal(SIGINT, cleanup); { FILE *m = fopen(g_mark, "w"); if (m) { fprintf(m, "%d\n", (int)getpid()); fclose(m); } } fprintf(stderr, "LISTENING (%s)\n", file ? file : "microphone");
    long cap = (long)(max_s + wait_s + 2) * SR; short *buf = malloc((size_t)cap * 2); if (!buf) return 1; long n = 0; double floor_db = 0; int chunks = 0, above = 0, speaking = 0, quiet = 0; long start = -1, last_loud = -1; int status = 3; short chunk[CH]; size_t have = 0; unsigned char *raw = (unsigned char *)chunk;
    for (;;) { struct pollfd p = { pfd[0], POLLIN, 0 }; int pr = poll(&p, 1, 1000); if (pr < 0) break; long elapsed = (long)(time(NULL) - t0);
        if (!speaking && elapsed >= wait_s + 1) break; if (speaking && n - start >= (long)max_s * SR) { status = 0; break; }
        if (pr == 0) continue; ssize_t r = read(pfd[0], raw + have, sizeof chunk - have); if (r <= 0) break; have += (size_t)r; if (have < sizeof chunk) continue; have = 0;
        if (n + CH > cap) break; memcpy(buf + n, chunk, sizeof chunk); double db = adsp_frame_db(chunk, CH); chunks++;
        if (chunks <= 6) { if (chunks == 1 || db < floor_db) floor_db = db; } else { if (db < floor_db) floor_db = db; else floor_db += 0.02; }
        double thr = floor_db + 9.0; if (thr < -50.0) thr = -50.0; int loud = chunks > 6 && db > thr; long cs = n; n += CH;
        if (!speaking) { if (loud) { above++; if (above >= 2) { speaking = 1; start = cs - CH * (above - 1) - SR / 2; if (start < 0) start = 0; last_loud = n; quiet = 0; } } else above = 0; }
        else { if (loud) { last_loud = n; quiet = 0; } else if (++quiet >= hang_ms / 100) { status = 0; break; } } }
    if (g_child > 0) { kill(g_child, SIGTERM); waitpid(g_child, NULL, 0); } unlink(g_mark);
    if (speaking && status != 0 && last_loud > start) status = 0;      /* the source ended (file) while speech was being recorded: keep what was heard */
    long secs = (long)(time(NULL) - t0); int rc = 3;
    if (status == 0 && speaking) { long end = last_loud + SR / 10; if (end > n) end = n; if (adsp_write_wav(out, buf + start, end - start, SR) == 0) { printf("%s\n", out); rc = 0; } else rc = 1; }
    { char lp[PL]; snprintf(lp, sizeof lp, "%s/ledger.txt", data); FILE *f = fopen(lp, "a"); if (f) { fprintf(f, "LISTEN | %ld | %s | %ld | %d | %s\n", (long)t0, arm, secs, rc == 0 ? 1 : 0, rc == 0 ? "utterance" : rc == 3 ? "no speech" : "error"); fclose(f); } }
    free(buf); return rc;
}
