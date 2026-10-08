/* ghost_fake_backend - test double for horn_chat_backend (harness ghost_run.pal). NO network. Behaviour comes from files next to the binary (<self>.X):
 *   .reply  text printed on stdout        .err  text printed on stderr (the provider line)      .rc  exit code (default 0)
 *   .hang   if present: sleep 30 s (to be killed by the watchdog)                                .hook  one argv entry per line: a command run (fork+exec, waited) before replying
 * It also records .pid, .prompt (argv[1]) and .env (HORN_TOOLS / HORN_CURL_TIMEOUT / PRISC_PROJECT_ROOT) so the harness can check how it was called. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
static char *rd(const char *self, const char *ext) {
    char p[4200]; snprintf(p, sizeof p, "%s.%s", self, ext);
    FILE *f = fopen(p, "rb"); if (!f) return NULL;
    char *b = malloc(1 << 20); size_t n = fread(b, 1, (1 << 20) - 1, f); b[n] = 0; fclose(f); return b;
}
static void wr(const char *self, const char *ext, const char *s) {
    char p[4200]; snprintf(p, sizeof p, "%s.%s", self, ext);
    FILE *f = fopen(p, "wb"); if (!f) return; fputs(s, f); fclose(f);
}
int main(int argc, char **argv) {
    const char *self = argv[0]; char t[8192], cw[4096]; char *s;
    snprintf(t, sizeof t, "%d\n", (int)getpid()); wr(self, "pid", t);
    wr(self, "prompt", argc > 1 ? argv[1] : "");
    if (!getcwd(cw, sizeof cw)) cw[0] = 0;
    snprintf(t, sizeof t, "HORN_TOOLS=%s\nHORN_CURL_TIMEOUT=%s\nPRISC_PROJECT_ROOT=%s\ncwd=%s\n", getenv("HORN_TOOLS") ? getenv("HORN_TOOLS") : "(unset)", getenv("HORN_CURL_TIMEOUT") ? getenv("HORN_CURL_TIMEOUT") : "(unset)", getenv("PRISC_PROJECT_ROOT") ? getenv("PRISC_PROJECT_ROOT") : "(unset)", cw);
    wr(self, "env", t);
    if ((s = rd(self, "hook"))) {
        char *av[64]; int n = 0; char *sv = NULL;
        for (char *ln = strtok_r(s, "\n", &sv); ln && n < 63; ln = strtok_r(NULL, "\n", &sv)) av[n++] = ln;
        av[n] = NULL;
        if (n) { pid_t p = fork(); if (p == 0) { execvp(av[0], av); _exit(127); } int st; waitpid(p, &st, 0); }
    }
    if (access((snprintf(t, sizeof t, "%s.hang", self), t), F_OK) == 0) for (;;) sleep(30);
    if ((s = rd(self, "reply"))) fputs(s, stdout);
    if ((s = rd(self, "err"))) fputs(s, stderr);
    fflush(stdout); fflush(stderr);
    s = rd(self, "rc"); return s ? atoi(s) : 0;
}
