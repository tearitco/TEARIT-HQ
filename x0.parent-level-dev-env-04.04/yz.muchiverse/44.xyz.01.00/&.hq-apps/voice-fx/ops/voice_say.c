/* voice_say - one spoken line, made off the window's thread: voice_say <package_dir> <house_root> <voice name> <label> <text> [speed= pitch= formant= bright= warm= vib_hz= vib_cents= breath= accent=]
 * 1. stand-in engine: @.apps/pet-trainer/ops/pet_tts_local.py (offline Kokoro; voice name -> Kokoro voice by the KMAP rows; speed -> --rate) writes a wav;
 * 2. our knobs: &.widgits/_shared-lib/ops/audio_fx (in-house C: speed, formant, pitch, vibrato, warm/bright shelves, breath; BOOK 19.audio/VOICE-FX.md); the accent goes to the engine (--accent) because it rewrites phonemes before speaking;
 * 3. plays it with paplay (one at a time: a lock), appends  SAY | ts | label | voice | speed | pitch_st | seconds-to-make | file  to state/ledger.txt, writes the wav path to state/last.txt.
 * A file is never overwritten: <label>.wav, then <label>_r2.wav ... Status lines (append-only) go to state/say.txt. Exit 0 = made, 1 = failed.
 * Docs: &.hq-apps/voice-fx/README.md; design BOOK 19.audio/. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/wait.h>
#define PL 4096
static char g_say[PL];
static void status(const char *m) { FILE *f = fopen(g_say, "a"); if (f) { fprintf(f, "%s\n", m); fclose(f); } }
static int run(char *const argv[], int timeout_s) {
    pid_t p = fork(); if (p < 0) return -1;
    if (p == 0) { int nul = open("/dev/null", O_RDWR); if (nul >= 0) { dup2(nul, 0); dup2(nul, 1); dup2(nul, 2); } execvp(argv[0], argv); _exit(127); }
    for (int t = 0; t < timeout_s * 5; t++) { int st; pid_t r = waitpid(p, &st, WNOHANG); if (r == p) return WIFEXITED(st) ? WEXITSTATUS(st) : -1; usleep(200000); }
    kill(p, SIGKILL); waitpid(p, NULL, 0); return -1;
}
int main(int argc, char **argv) {
    if (argc < 6) return 2; const char *pkg = argv[1], *house = argv[2], *voice = argv[3], *label = argv[4], *text = argv[5];      /* then key=value: speed pitch formant bright warm vib_hz vib_cents breath accent */
    snprintf(g_say, sizeof g_say, "%s/state/say.txt", pkg); char dir[PL]; snprintf(dir, sizeof dir, "%s/state/audio", pkg); mkdir(dir, 0755);
    time_t t0 = time(NULL); char out[PL], tmp[PL], msg[PL], tts[PL]; snprintf(out, sizeof out, "%s/%s.wav", dir, label); struct stat st;
    for (int r = 2; r < 100 && !stat(out, &st); r++) snprintf(out, sizeof out, "%s/%s_r%d.wav", dir, label, r);
    snprintf(tmp, sizeof tmp, "%s/tmp_%d.wav", dir, (int)getpid()); snprintf(tts, sizeof tts, "%s/@.apps/pet-trainer/ops/pet_tts_local.py", house);
    char accent[48] = "--accent=none", fxargs[10][48]; int nfx = 0, spd_ms = 0; (void)spd_ms; char speed[24] = "1.0", summary[300] = "";
    for (int i = 6; i < argc; i++) { if (!strncmp(argv[i], "accent=", 7)) snprintf(accent, sizeof accent, "--accent=%s", argv[i] + 7); else if (!strncmp(argv[i], "speed=", 6)) snprintf(speed, sizeof speed, "%s", argv[i] + 6); else if (nfx < 10) snprintf(fxargs[nfx++], 48, "%s", argv[i]); size_t l = strlen(summary); snprintf(summary + l, sizeof summary - l, "%s ", argv[i]); }
    snprintf(msg, sizeof msg, "making: %s", label); status(msg);
    char rate[24]; snprintf(rate, sizeof rate, "--rate=%+d%%", (int)((atof(speed) - 1.0) * 100.0 + (atof(speed) >= 1.0 ? 0.5 : -0.5)));
    { char *a[] = { "nice", "-n", "10", tts, "--voice", (char *)voice, rate, "--pitch=+0Hz", "--no-fx", accent, "--text", (char *)text, "--write-media", tmp, NULL }; if (run(a, 90) != 0 || stat(tmp, &st) || st.st_size < 100) { unlink(tmp); status("failed: the voice engine made no sound (voice has no local Kokoro mapping?)"); return 1; } }
    { char fxb[PL]; snprintf(fxb, sizeof fxb, "%s/&.widgits/_shared-lib/ops/+x/audio_fx.+x", house); char *a[20]; int n = 0; a[n++] = fxb; a[n++] = tmp; a[n++] = out; for (int i = 0; i < nfx; i++) a[n++] = fxargs[i]; a[n] = NULL;
      if (run(a, 90) != 0 || stat(out, &st)) { unlink(tmp); status("failed: audio_fx (our own voice effects) step"); return 1; } }
    unlink(tmp);
    { char lp[PL]; snprintf(lp, sizeof lp, "%s/state/ledger.txt", pkg); FILE *f = fopen(lp, "a"); if (f) { fprintf(f, "SAY | %ld | %s | %s | %s%s | %ld | %s\n", (long)t0, label, voice, summary, accent, (long)(time(NULL) - t0), out); fclose(f); } }
    { char lp[PL]; snprintf(lp, sizeof lp, "%s/state/last.txt", pkg); FILE *f = fopen(lp, "w"); if (f) { fprintf(f, "%s\n", out); fclose(f); } }
    snprintf(msg, sizeof msg, "done: %s in %ld s", out, (long)(time(NULL) - t0)); status(msg);
    { char lp[PL]; snprintf(lp, sizeof lp, "%s/state/play.lock", pkg); int fd = open(lp, O_CREAT | O_RDWR, 0644); if (fd >= 0 && flock(fd, LOCK_EX | LOCK_NB) == 0) { char *a[] = { "nice", "-n", "10", "paplay", "--volume=32000", out, NULL }; run(a, 30); } }
    return 0;
}
