/* img_hai_job - one text-to-image draft on a remote host, started by img_hai_manager (never blocks the window).
 * Usage: img_hai_job <package_dir> <house_root> <ssh_target> <remote_dir> <python> <model> <steps> <size> <seed> <part> <prompt_file>
 * Steps: copy the prompt file to the host, run img_draft.py there under nice/ionice, copy the PNGs back into
 * <house>/&.widgits/concept-bank/data/img/draft/<day>/img/, append one LEDGER line, and append status lines to <package_dir>/state/job.txt
 * (append-only; the manager reads it by size growth). Children are fork+exec with a watchdog; there is no popen/system. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#define PL 4096
static char g_job[PL];
static void status(const char *m) { FILE *f = fopen(g_job, "a"); if (f) { fprintf(f, "%s\n", m); fclose(f); } }
/* run argv with stdout+stderr to outfile; returns exit code, -1 on timeout/fail */
static int run(char *const argv[], const char *outfile, int timeout_s) {
    pid_t p = fork(); if (p < 0) return -1;
    if (p == 0) { int fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644); if (fd >= 0) { dup2(fd, 1); dup2(fd, 2); close(fd); } int nul = open("/dev/null", O_RDONLY); if (nul >= 0) { dup2(nul, 0); close(nul); } execvp(argv[0], argv); _exit(127); }
    for (int t = 0; t < timeout_s * 5; t++) { int st; pid_t r = waitpid(p, &st, WNOHANG); if (r == p) return WIFEXITED(st) ? WEXITSTATUS(st) : -1; usleep(200000); }
    kill(p, SIGKILL); waitpid(p, NULL, 0); return -1;
}
int main(int argc, char **argv) {
    if (argc < 12) return 2;
    const char *pkg = argv[1], *house = argv[2], *host = argv[3], *rdir = argv[4], *py = argv[5], *model = argv[6], *steps = argv[7], *size = argv[8], *seed = argv[9], *part = argv[10], *pf = argv[11];
    snprintf(g_job, sizeof g_job, "%s/state/job.txt", pkg);
    const char *ms = strrchr(model, '/'); ms = ms ? ms + 1 : model;
    char id[64]; snprintf(id, sizeof id, "%s_%s_s%s", part, ms, seed);
    char msg[PL], tmpo[PL], rpath[PL], cmd[PL], day[16]; time_t now = time(NULL); strftime(day, sizeof day, "%Y-%m-%d", localtime(&now));
    snprintf(tmpo, sizeof tmpo, "%s/state/job_%s.out", pkg, id);
    snprintf(msg, sizeof msg, "running: %s on %s (about 2 min)", id, host); status(msg);
    snprintf(rpath, sizeof rpath, "%s:%s/jobs/%s.txt", host, rdir, id);
    { snprintf(cmd, sizeof cmd, "mkdir -p %s/jobs/out", rdir); char *a[] = { "ssh", "-o", "BatchMode=yes", "-o", "ConnectTimeout=10", (char *)host, cmd, NULL }; if (run(a, tmpo, 30) != 0) { status("failed: cannot reach host"); return 1; } }
    { char *a[] = { "scp", "-q", "-o", "BatchMode=yes", (char *)pf, rpath, NULL }; if (run(a, tmpo, 60) != 0) { status("failed: copying the prompt"); return 1; } }
    snprintf(cmd, sizeof cmd, "cd %s && nice -n 15 ionice -c3 env IMG_THREADS=6 %s img_draft.py --prompt-file jobs/%s.txt --part %s --model %s --out jobs/out --steps %s --size %s --seed %s", rdir, py, id, part, model, steps, size, seed);
    { char *a[] = { "ssh", "-o", "BatchMode=yes", "-o", "ServerAliveInterval=30", (char *)host, cmd, NULL }; int rc = run(a, tmpo, 1500); if (rc != 0) { snprintf(msg, sizeof msg, "failed: remote draft (exit %d) see state/job_%s.out", rc, id); status(msg); return 1; } }
    double secs = 0; { FILE *f = fopen(tmpo, "r"); char ln[PL]; if (f) { while (fgets(ln, sizeof ln, f)) if (!strncmp(ln, "done ", 5)) secs = atof(ln + 5); fclose(f); } }
    char dd[PL], dimg[PL]; snprintf(dd, sizeof dd, "%s/&.widgits/concept-bank/data/img/draft/%s", house, day); snprintf(dimg, sizeof dimg, "%s/img", dd); mkdir(dd, 0755); mkdir(dimg, 0755);
    status("copying the image back");
    char src[PL];
    snprintf(src, sizeof src, "%s:%s/jobs/out/%s.png", host, rdir, id); { char *a[] = { "scp", "-q", "-o", "BatchMode=yes", src, dimg, NULL }; if (run(a, tmpo, 120) != 0) { status("failed: copying the image back"); return 1; } }
    snprintf(src, sizeof src, "%s:%s/jobs/out/%s_32px.png", host, rdir, id); { char *a[] = { "scp", "-q", "-o", "BatchMode=yes", src, dimg, NULL }; run(a, tmpo, 120); }
    char prompt[2048] = ""; { FILE *f = fopen(pf, "r"); char ln[2300]; if (f) { while (fgets(ln, sizeof ln, f)) if (!strncmp(ln, "PROMPT:", 7)) { snprintf(prompt, sizeof prompt, "%s", ln + 8); char *n = strchr(prompt, '\n'); if (n) *n = 0; } fclose(f); } }
    char lp[PL]; snprintf(lp, sizeof lp, "%s/ledger.txt", dd); FILE *lf = fopen(lp, "a"); if (lf) { fprintf(lf, "LEDGER | %s | %s | %s | %s | %s | %.1f | %s.png | %s\n", part, model, seed, steps, size, secs, id, prompt); fclose(lf); }
    unlink(tmpo);
    snprintf(msg, sizeof msg, "done: %s in %.0f s", id, secs); status(msg);
    return 0;
}
