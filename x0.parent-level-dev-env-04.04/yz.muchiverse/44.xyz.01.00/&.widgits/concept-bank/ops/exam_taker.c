/* exam_taker - micro-task worker: answers a chem_label exam with the LAN Gemma (free, uncapped), one fixed answer line per question.
 * Usage: exam_taker <ai_backend.pdl> <in.exam> <out_answers.txt> <worker_name> [timeout_s=60]
 * Reads gemma_lan_url / gemma_lan_model from ai_backend.pdl. For each `Q | id | kind | text | form` row it asks the model for ONLY the answer,
 * extracts one token by the question's form (integer | symbol | yes/no) and writes `A | id | answer` (the answers file chem_label exam-grade reads).
 * Models never set numbers or decide anything: the key grades them. Unparseable replies are written as `?`, which grades wrong.
 * One call at a time, curl via fork+exec+waitpid watchdog (house rule: no popen). Prints a per-kind summary line `KIND kind asked=n parsed=n`.
 * Nothing is written to any bank or feedback log by this op. Needs curl on PATH. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <sys/wait.h>
#define L 4096
static char URL[256] = "http://10.0.0.144:11434", MODEL[96] = "gemma3:270m";
static void conf(const char *p) { char ln[L]; FILE *f = fopen(p, "r"); if (!f) return; while (fgets(ln, sizeof ln, f)) { ln[strcspn(ln, "\r\n")] = 0;
    if (!strncmp(ln, "gemma_lan_url=", 14)) snprintf(URL, sizeof URL, "%.255s", ln + 14); else if (!strncmp(ln, "gemma_lan_model=", 16)) snprintf(MODEL, sizeof MODEL, "%.95s", ln + 16); } fclose(f); }
static int run_curl(const char *reqf, const char *outf, int tmo) {
    pid_t c = fork(); if (c < 0) return -1;
    if (c == 0) { int fd = open(outf, O_WRONLY | O_CREAT | O_TRUNC, 0600); if (fd >= 0) { dup2(fd, 1); close(fd); } int nul = open("/dev/null", O_WRONLY); if (nul >= 0) { dup2(nul, 2); close(nul); }
        char u[400], d[400]; snprintf(u, sizeof u, "%.300s/api/generate", URL); snprintf(d, sizeof d, "@%.380s", reqf);
        execlp("nice", "nice", "-n", "15", "curl", "-s", "-m", "55", "-H", "Content-Type: application/json", "-d", d, u, (char *)NULL); _exit(127); }
    time_t t0 = time(NULL); int st;
    for (;;) { pid_t r = waitpid(c, &st, WNOHANG); if (r == c) return WIFEXITED(st) ? WEXITSTATUS(st) : -1; if (time(NULL) - t0 > tmo) { kill(c, SIGKILL); waitpid(c, &st, 0); return -2; } usleep(100000); }
}
static void esc(const char *s, char *o, size_t n) { size_t k = 0; for (; *s && k + 3 < n; s++) { if (*s == '"' || *s == '\\') o[k++] = '\\'; o[k++] = (*s == '\n') ? ' ' : *s; } o[k] = 0; }
static int reply_text(const char *outf, char *t, size_t n) {   /* "response":"..." field of the Ollama reply */
    char buf[L * 4]; FILE *f = fopen(outf, "r"); if (!f) return 0; size_t m = fread(buf, 1, sizeof buf - 1, f); fclose(f); buf[m] = 0;
    char *p = strstr(buf, "\"response\":\""); if (!p) return 0; p += 12; size_t k = 0;
    while (*p && *p != '"' && k + 1 < n) { if (*p == '\\' && p[1]) { p++; t[k++] = (*p == 'n') ? ' ' : *p; p++; } else t[k++] = *p++; }
    t[k] = 0; return 1; }
static int pick(const char *form, const char *t, char *a, size_t n) {
    if (!strcmp(form, "integer")) { for (const char *p = t; *p; p++) if (isdigit((unsigned char)*p)) { size_t k = 0; while (isdigit((unsigned char)*p) && k + 1 < n) a[k++] = *p++; a[k] = 0; return 1; } return 0; }
    if (!strcmp(form, "yes/no")) { for (const char *p = t; *p; p++) { if (!strncasecmp(p, "yes", 3)) { snprintf(a, n, "yes"); return 1; } if (!strncasecmp(p, "no", 2)) { snprintf(a, n, "no"); return 1; } } return 0; }
    for (const char *p = t; *p; p++) if (isupper((unsigned char)*p)) { size_t k = 0; a[k++] = *p++; if (islower((unsigned char)*p)) a[k++] = *p; a[k] = 0; return 1; }   /* symbol: capital + optional lowercase */
    return 0; }
int main(int argc, char **argv) {
    if (argc < 5) { fprintf(stderr, "usage: exam_taker <ai_backend.pdl> <in.exam> <out_answers.txt> <worker> [timeout_s]\n"); return 2; }
    int tmo = argc > 5 ? atoi(argv[5]) : 60; conf(argv[1]); FILE *in = fopen(argv[2], "r"), *out = fopen(argv[3], "w"); if (!in || !out) { fprintf(stderr, "cannot open files\n"); return 2; }
    char reqf[400], outf[400]; snprintf(reqf, sizeof reqf, "%.380s.req", argv[3]); snprintf(outf, sizeof outf, "%.380s.rsp", argv[3]);
    char ln[L]; char kinds[8][16]; int ask[8] = {0}, par[8] = {0}, nk = 0;
    while (fgets(ln, sizeof ln, in)) { char id[32], kind[16], text[1024], form[16]; if (strncmp(ln, "Q |", 3)) continue;
        if (sscanf(ln, "Q | %31s | %15s | %1023[^|] | %15[^\n]", id, kind, text, form) != 4) continue;
        int ki = -1; for (int i = 0; i < nk; i++) if (!strcmp(kinds[i], kind)) ki = i; if (ki < 0 && nk < 8) { snprintf(kinds[nk], 16, "%s", kind); ki = nk++; } if (ki < 0) continue; ask[ki]++;
        char e[2200], pr[2400]; const char *ins = !strcmp(form, "integer") ? "Reply with only a whole number." : !strcmp(form, "yes/no") ? "Reply with only yes or no." : "Reply with only the element symbol (one or two letters), not the name."; snprintf(pr, sizeof pr, "%s %s", text, ins); esc(pr, e, sizeof e);
        FILE *r = fopen(reqf, "w"); if (!r) continue; fprintf(r, "{\"model\":\"%s\",\"prompt\":\"%s\",\"stream\":false,\"options\":{\"temperature\":0,\"num_predict\":16}}", MODEL, e); fclose(r);
        char t[512] = "", a[64] = "?"; int rc = run_curl(reqf, outf, tmo);
        if (rc == 0 && reply_text(outf, t, sizeof t) && pick(form, t, a, sizeof a)) par[ki]++; else snprintf(a, sizeof a, "?");
        fprintf(out, "A | %s | %s\n", id, a); }
    fclose(in); fclose(out); unlink(reqf); unlink(outf);
    for (int i = 0; i < nk; i++) printf("KIND %s asked=%d parsed=%d worker=%s\n", kinds[i], ask[i], par[i], argv[4]);
    return 0; }
