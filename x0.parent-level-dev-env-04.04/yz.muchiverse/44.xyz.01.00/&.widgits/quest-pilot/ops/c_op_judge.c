/* c_op_judge - the locked JUDGE for "write one small C op" quests (ghost_run HARNESS command).
 * Usage: c_op_judge <worker.c> <cases.pdl> <verdict_file>
 * 1. compiles <worker.c> with gcc -std=gnu11 -O2 -Wall -Wextra into a private temp dir (compile failure = FAIL, the compiler text goes in the verdict file);
 * 2. runs every CASE of <cases.pdl> against the binary (fork+exec, no shell, 5 s watchdog, its own temp dir as cwd) and compares exit code and stdout;
 * 3. writes the verdict file: one line per case (PASS|label or FAIL|label|why) and as the LAST line "PASS passed=N" or "FAIL failed=K of N" (ghost_run wants PASS and no FAIL there).
 * cases.pdl rows ('#' comments; strings use \n \t \p (a literal '|') and $T = the temp dir; fields are trimmed; \xHH = a raw byte):
 *   FILE | <name> | <text>                          write <T>/<name>
 *   CASE | <label> | <exit> | <out> | <arg1> | <arg2> ...    out: "=text" exact stdout (trailing newlines trimmed), "~text" stdout contains, "*" anything
 * Exit 0 always when it wrote a verdict (the verdict decides); 2 usage. Cases never touch anything outside the temp dir. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#define PM 4096
static char T[PM];
static void trim(char *s) { char *p = s; while (*p == ' ' || *p == '\t') p++; memmove(s, p, strlen(p) + 1); size_t n = strlen(s); while (n && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\r' || s[n-1] == '\n')) s[--n] = 0; }
static void expand(const char *in, char *out, size_t cap) {
    size_t o = 0;
    for (const char *p = in; *p && o + 2 < cap; p++) {
        if (*p == '\\' && p[1] == 'x' && p[2] && p[3]) { char h[3] = { p[2], p[3], 0 }; out[o++] = (char)strtol(h, NULL, 16); p += 3; }
        else if (*p == '\\' && p[1]) { p++; out[o++] = (*p == 'n') ? '\n' : (*p == 't') ? '\t' : (*p == 'p') ? '|' : *p; }
        else if (*p == '$' && p[1] == 'T') { size_t k = strlen(T); if (o + k + 1 < cap) { memcpy(out + o, T, k); o += k; } p++; }
        else out[o++] = *p;
    }
    out[o] = 0;
}
static int run(char *const argv[], const char *cwd, char *outbuf, size_t cap, int tmo) {
    int po[2]; if (pipe(po)) return -1; pid_t pid = fork();
    if (pid == 0) { setpgid(0, 0); dup2(po[1], 1); dup2(po[1], 2); close(po[0]); close(po[1]); if (cwd) { if (chdir(cwd)) _exit(126); } execv(argv[0], argv); _exit(127); }
    close(po[1]); setpgid(pid, pid); size_t n = 0; time_t dl = time(NULL) + tmo; int st = 0, done = 0, rc = -1;
    fcntl(po[0], F_SETFL, O_NONBLOCK);
    while (!done) {
        char b[4096]; ssize_t k = read(po[0], b, sizeof b); if (k > 0 && n + (size_t)k < cap) { memcpy(outbuf + n, b, (size_t)k); n += (size_t)k; }
        pid_t w = waitpid(pid, &st, WNOHANG);
        if (w == pid) { done = 1; rc = WIFEXITED(st) ? WEXITSTATUS(st) : 128 + WTERMSIG(st); }
        else if (time(NULL) > dl) { kill(-pid, SIGKILL); kill(pid, SIGKILL); waitpid(pid, &st, 0); rc = 124; done = 1; }
        else if (k <= 0) usleep(5000);
    }
    for (;;) { char b[4096]; ssize_t k = read(po[0], b, sizeof b); if (k <= 0) break; if (n + (size_t)k < cap) { memcpy(outbuf + n, b, (size_t)k); n += (size_t)k; } }
    close(po[0]); outbuf[n] = 0; return rc;
}
int main(int argc, char **argv) {
    if (argc != 4) { fprintf(stderr, "usage: c_op_judge <worker.c> <cases.pdl> <verdict_file>\n"); return 2; }
    FILE *vf = fopen(argv[3], "w"); if (!vf) return 2;
    snprintf(T, sizeof T, "/tmp/cjudge_XXXXXX"); if (!mkdtemp(T)) { fprintf(vf, "FAIL no temp dir\n"); fclose(vf); return 0; }
    char bin[PM], cc[8192]; snprintf(bin, sizeof bin, "%s/op", T);
    char *cargv[] = { "/usr/bin/gcc", "-std=gnu11", "-O2", "-Wall", "-Wextra", "-o", bin, argv[1], "-lm", NULL };
    int crc = run(cargv, NULL, cc, sizeof cc - 1, 60);
    if (crc != 0) { char *p; while ((p = strchr(cc, '\n')) && p - cc < 600 && p[1]) *p = ' '; cc[700] = 0; fprintf(vf, "compile output: %s\nFAIL compile failed (exit %d)\n", cc, crc); fclose(vf); return 0; }
    FILE *cf = fopen(argv[2], "r"); if (!cf) { fprintf(vf, "FAIL cannot read cases\n"); fclose(vf); return 0; }
    char line[PM * 2]; int pass = 0, fail = 0, total = 0;
    while (fgets(line, sizeof line, cf)) {
        char *f[40]; int nf = 0; trim(line); if (!line[0] || line[0] == '#') continue;
        for (char *p = line; nf < 40;) { f[nf++] = p; char *bar = strchr(p, '|'); if (!bar) break; *bar = 0; p = bar + 1; }
        for (int i = 0; i < nf; i++) trim(f[i]);
        if (!strcmp(f[0], "FILE") && nf >= 3) { char path[PM], txt[PM * 2]; expand(f[2], txt, sizeof txt); snprintf(path, sizeof path, "%s/%s", T, f[1]); FILE *w = fopen(path, "w"); if (w) { fputs(txt, w); fclose(w); } }
        else if (!strcmp(f[0], "CASE") && nf >= 4) {
            char *av[40]; char args[40][PM]; int ac = 0; char out[PM * 4]; total++;
            av[ac++] = bin; for (int i = 4; i < nf && ac < 38; i++) { expand(f[i], args[ac], PM); av[ac] = args[ac]; ac++; } av[ac] = NULL;
            int rc = run(av, T, out, sizeof out - 1, 5); size_t n = strlen(out); while (n && (out[n-1] == '\n' || out[n-1] == '\r')) out[--n] = 0;
            int want = atoi(f[2]); char exp[PM]; expand(f[3][0] ? f[3] + 1 : "", exp, sizeof exp); int okout = 1;
            if (f[3][0] == '=') okout = !strcmp(out, exp); else if (f[3][0] == '~') okout = strstr(out, exp) != NULL;
            if (rc == want && okout) { pass++; fprintf(vf, "PASS|%s\n", f[1]); }
            else { fail++; out[300] = 0; for (char *p = out; *p; p++) if (*p == '\n') *p = ' '; fprintf(vf, "case %s: want exit %d got %d; expected out %s%s; got out: %s\nFAIL|%s\n", f[1], want, rc, f[3][0] ? f[3] : "-", "", out, f[1]); }
        }
    }
    fclose(cf);
    if (!total) fprintf(vf, "FAIL no cases ran\n"); else if (fail) fprintf(vf, "FAIL failed=%d of %d\n", fail, total); else fprintf(vf, "PASS passed=%d\n", pass);
    fclose(vf); return 0;
}
