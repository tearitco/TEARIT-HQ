/* sfx_make - render one sound effect and check it: sfx_make <package_dir> <label> <wave> <f0> <f1> <dur> <attack> <decay> <vol> <lp>     (SCAFFOLD, 2026-10-14)
 * Stand-in renderer: sox synth (the in-house C renderer is roadmap slice S3, BOOK 19.audio/SFX-TOY-DESIGN.md). Writes state/audio/<label>.wav (22.05 kHz mono 16 bit; never overwrites: _r2 ...),
 * then the first two NUMERIC CHECKS from the design, read from `sox ... stat`: length within 5% of dur (+0.02 s), peak amplitude between 0.05 and 0.98. Result line to state/say.txt,
 * ledger row  SFX | ts | label | wave f0 f1 dur | seconds-to-make | checks | file  to state/ledger.txt, path to state/last.txt, then plays with paplay (a lock: one at a time).
 * Exit 0 = made (checks may still say FAIL in the line), 1 = could not make. A recipe that is silent or clipped is made anyway and reported, so a human can hear what failed. */
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
static int run(char *const argv[], const char *outfile, int timeout_s) {
    pid_t p = fork(); if (p < 0) return -1;
    if (p == 0) { int nul = open("/dev/null", O_RDWR); int fd = outfile ? open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644) : nul; if (nul >= 0) dup2(nul, 0); if (fd >= 0) { dup2(fd, 1); dup2(fd, 2); } execvp(argv[0], argv); _exit(127); }
    for (int t = 0; t < timeout_s * 5; t++) { int st; pid_t r = waitpid(p, &st, WNOHANG); if (r == p) return WIFEXITED(st) ? WEXITSTATUS(st) : -1; usleep(200000); }
    kill(p, SIGKILL); waitpid(p, NULL, 0); return -1;
}
int main(int argc, char **argv) {
    if (argc < 11) return 2; const char *pkg = argv[1], *label = argv[2], *wave = argv[3], *f0 = argv[4], *f1 = argv[5], *dur = argv[6], *att = argv[7], *dec = argv[8], *vol = argv[9], *lp = argv[10];
    snprintf(g_say, sizeof g_say, "%s/state/say.txt", pkg); char dir[PL]; snprintf(dir, sizeof dir, "%s/state/audio", pkg); mkdir(dir, 0755);
    time_t t0 = time(NULL); char out[PL], msg[PL], st_out[PL]; struct stat st; snprintf(out, sizeof out, "%s/%s.wav", dir, label);
    for (int r = 2; r < 100 && !stat(out, &st); r++) snprintf(out, sizeof out, "%s/%s_r%d.wav", dir, label, r);
    snprintf(msg, sizeof msg, "making: %s", label); status(msg);
    const char *sw = !strcmp(wave, "noise") ? "whitenoise" : wave; char freq[40]; if (!strcmp(wave, "noise")) freq[0] = 0; else if (atof(f1) > 0) snprintf(freq, sizeof freq, "%s:%s", f0, f1); else snprintf(freq, sizeof freq, "%s", f0);
    char fade[80]; snprintf(fade, sizeof fade, "%s", att); char *a[24]; int n = 0; a[n++] = "sox"; a[n++] = "-n"; a[n++] = "-r"; a[n++] = "22050"; a[n++] = "-c"; a[n++] = "1"; a[n++] = "-b"; a[n++] = "16"; a[n++] = out; a[n++] = "synth"; a[n++] = (char *)dur; a[n++] = (char *)sw; if (freq[0]) a[n++] = freq;
    a[n++] = "fade"; a[n++] = "t"; a[n++] = fade; a[n++] = (char *)dur; a[n++] = (char *)dec; a[n++] = "vol"; a[n++] = (char *)vol; if (atof(lp) > 0) { a[n++] = "lowpass"; a[n++] = (char *)lp; } a[n] = NULL;
    if (run(a, NULL, 30) != 0 || stat(out, &st)) { status("failed: sox could not render this recipe"); return 1; }
    snprintf(st_out, sizeof st_out, "%s/stat_%d.txt", dir, (int)getpid()); { char *b[] = { "sox", out, "-n", "stat", NULL }; run(b, st_out, 15); }
    double len = -1, peak = -1; { FILE *f = fopen(st_out, "r"); char ln[256]; if (f) { while (fgets(ln, sizeof ln, f)) { if (!strncmp(ln, "Length (seconds):", 17)) len = atof(ln + 17); else if (!strncmp(ln, "Maximum amplitude:", 18)) peak = atof(ln + 18); } fclose(f); } unlink(st_out); }
    double want = atof(dur); int dur_ok = len >= 0 && len >= want * 0.95 - 0.02 && len <= want * 1.05 + 0.02, peak_ok = peak >= 0.05 && peak <= 0.98;
    char chk[200]; snprintf(chk, sizeof chk, "duration %.3f s (want %.3f) %s; peak %.2f %s", len, want, dur_ok ? "ok" : "FAIL", peak, peak_ok ? "ok" : (peak < 0.05 ? "FAIL silent" : "FAIL clipped"));
    { char lp2[PL]; snprintf(lp2, sizeof lp2, "%s/state/ledger.txt", pkg); FILE *f = fopen(lp2, "a"); if (f) { fprintf(f, "SFX | %ld | %s | %s %s %s %s | %ld | %s | %s\n", (long)t0, label, wave, f0, f1, dur, (long)(time(NULL) - t0), chk, out); fclose(f); } }
    { char lp2[PL]; snprintf(lp2, sizeof lp2, "%s/state/last.txt", pkg); FILE *f = fopen(lp2, "w"); if (f) { fprintf(f, "%s\n", out); fclose(f); } }
    snprintf(msg, sizeof msg, "done: %s | %s", strrchr(out, '/') + 1, chk); status(msg);
    { char lp2[PL]; snprintf(lp2, sizeof lp2, "%s/state/play.lock", pkg); int fd = open(lp2, O_CREAT | O_RDWR, 0644); if (fd >= 0 && flock(fd, LOCK_EX | LOCK_NB) == 0) { char *p[] = { "nice", "-n", "10", "paplay", "--volume=32000", out, NULL }; run(p, NULL, 30); } }
    return 0;
}
