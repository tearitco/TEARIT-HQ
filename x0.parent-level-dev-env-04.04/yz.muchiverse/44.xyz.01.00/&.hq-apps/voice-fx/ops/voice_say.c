/* voice_say - one spoken line, made off the window's thread: voice_say <package_dir> <house_root> <voice name> <speed> <pitch_st> <label> <text>
 * 1. stand-in engine: @.apps/pet-trainer/ops/pet_tts_local.py (offline Kokoro; voice name -> Kokoro voice by the KMAP rows; speed -> --rate) writes a wav;
 * 2. our knobs: &.widgits/_shared-lib/ops/audio_shift (in-house C: WSOLA + resample, pitch in semitones, duration kept, formants move with pitch; roadmap slice S1) writes the final mono 16-bit wav at the engine's sample rate;
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
    if (argc < 8) return 2; const char *pkg = argv[1], *house = argv[2], *voice = argv[3], *speed = argv[4], *pitch = argv[5], *label = argv[6], *text = argv[7];
    snprintf(g_say, sizeof g_say, "%s/state/say.txt", pkg); char dir[PL]; snprintf(dir, sizeof dir, "%s/state/audio", pkg); mkdir(dir, 0755);
    time_t t0 = time(NULL); char out[PL], tmp[PL], msg[PL], tts[PL]; snprintf(out, sizeof out, "%s/%s.wav", dir, label); struct stat st;
    for (int r = 2; r < 100 && !stat(out, &st); r++) snprintf(out, sizeof out, "%s/%s_r%d.wav", dir, label, r);
    snprintf(tmp, sizeof tmp, "%s/tmp_%d.wav", dir, (int)getpid()); snprintf(tts, sizeof tts, "%s/@.apps/pet-trainer/ops/pet_tts_local.py", house);
    snprintf(msg, sizeof msg, "making: %s", label); status(msg);
    char rate[24]; snprintf(rate, sizeof rate, "--rate=%+d%%", (int)((atof(speed) - 1.0) * 100.0 + (atof(speed) >= 1.0 ? 0.5 : -0.5)));
    { char *a[] = { "nice", "-n", "10", tts, "--voice", (char *)voice, rate, "--pitch=+0Hz", "--text", (char *)text, "--write-media", tmp, NULL }; if (run(a, 90) != 0 || stat(tmp, &st) || st.st_size < 100) { unlink(tmp); status("failed: the voice engine made no sound (voice has no local Kokoro mapping?)"); return 1; } }
    { char shift[PL]; snprintf(shift, sizeof shift, "%s/&.widgits/_shared-lib/ops/+x/audio_shift.+x", house); char *a[] = { shift, tmp, out, (char *)pitch, "1.0", NULL }; if (run(a, 60) != 0 || stat(out, &st)) { unlink(tmp); status("failed: audio_shift (our own pitch DSP) step"); return 1; } }
    unlink(tmp);
    { char lp[PL]; snprintf(lp, sizeof lp, "%s/state/ledger.txt", pkg); FILE *f = fopen(lp, "a"); if (f) { fprintf(f, "SAY | %ld | %s | %s | %s | %s | %ld | %s\n", (long)t0, label, voice, speed, pitch, (long)(time(NULL) - t0), out); fclose(f); } }
    { char lp[PL]; snprintf(lp, sizeof lp, "%s/state/last.txt", pkg); FILE *f = fopen(lp, "w"); if (f) { fprintf(f, "%s\n", out); fclose(f); } }
    snprintf(msg, sizeof msg, "done: %s in %ld s", out, (long)(time(NULL) - t0)); status(msg);
    { char lp[PL]; snprintf(lp, sizeof lp, "%s/state/play.lock", pkg); int fd = open(lp, O_CREAT | O_RDWR, 0644); if (fd >= 0 && flock(fd, LOCK_EX | LOCK_NB) == 0) { char *a[] = { "nice", "-n", "10", "paplay", "--volume=32000", out, NULL }; run(a, 30); } }
    return 0;
}
