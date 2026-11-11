#define _GNU_SOURCE
/* proof_all_op - the Harness menu "run all" row (2026-11-11).
 *   proof_all_op run <house> <menu.pdl> <outdir>
 * Stages, in order, each printed as STAGE|<name>:
 *   make      the case runner and video_op are on disk
 *   test      every CASE row in the menu except proof_all and proof_all_fix
 *   present   a silent slide video of those verdicts (video_op --no-tts)
 *   doc       <outdir>/0.proof-all.md
 *   continue  CONTINUE on the last line. Exit 0 only when every test passed.
 * A failed test still gets a doc and a video, then CONTINUE, and the exit is 1.
 * Does not call cpu_gate: the menu row is already inside one gate.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#define P 4096
#define MAXC 32

static void stage(const char *s) { printf("STAGE|%s\n", s); fflush(stdout); }
static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; return s; }

static int run_cwd(char *const argv[], const char *cwd) {
    pid_t pid = fork(); if (pid < 0) return -1;
    if (pid == 0) { if (cwd && chdir(cwd)) _exit(126); execv(argv[0], argv); _exit(127); }
    int st = 0; waitpid(pid, &st, 0); return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

int main(int argc, char **argv) {
    if (argc != 5 || strcmp(argv[1], "run")) { fprintf(stderr, "usage: proof_all_op run <house> <menu.pdl> <outdir>\n"); return 2; }
    const char *house = argv[2], *menu = argv[3], *out = argv[4];
    char harness[P], caseop[P], verdop[P], video[P];
    snprintf(harness, sizeof harness, "%s/&.widgits/_shared-lib/harness", house);
    snprintf(caseop, sizeof caseop, "%s/ops/+x/harness_case_op.+x", harness);
    snprintf(verdop, sizeof verdop, "%s/ops/+x/harness_verdict_op.+x", harness);
    snprintf(video, sizeof video, "%s/ops/+x/video_op.+x", harness);
    stage("make");
    if (access(caseop, X_OK) || access(verdop, X_OK) || access(video, X_OK) || access("/usr/bin/gcc", X_OK)) {
        fprintf(stderr, "proof_all_op: make missing a runner, video_op, or gcc\n"); return 1;
    }
    mkdir(out, 0755);
    char ids[MAXC][64]; int n = 0;
    FILE *mf = fopen(menu, "r"); if (!mf) { fprintf(stderr, "proof_all_op: no menu\n"); return 2; }
    char ln[1024];
    while (fgets(ln, sizeof ln, mf) && n < MAXC) {
        if (ln[0] == '#') continue;
        char *f[6]; int nf = 0; char *p = ln; f[nf++] = p;
        for (; *p && nf < 6; p++) if (*p == '|') { *p = 0; f[nf++] = p + 1; }
        if (nf < 5 || strcmp(trim(f[0]), "HARNESS")) continue;
        if (strcmp(trim(f[3]), "CASE")) continue;
        char *id = trim(f[1]);
        if (!strcmp(id, "proof_all") || !strcmp(id, "proof_all_fix")) continue;
        snprintf(ids[n], 64, "%s", id); n++;
    }
    fclose(mf);
    if (!n) { fprintf(stderr, "proof_all_op: no cases\n"); return 1; }
    stage("test");
    int failed = 0;
    char verdicts[MAXC][240];
    for (int i = 0; i < n; i++) {
        char cf[P]; snprintf(cf, sizeof cf, "cases/%s.pdl", ids[i]);
        char *a1[] = { caseop, cf, (char *)house, NULL };
        int rc = run_cwd(a1, harness);
        char *a2[] = { verdop, cf, NULL };
        run_cwd(a2, harness);
        char vp[P]; snprintf(vp, sizeof vp, "%s/results/%s.txt.verdict.txt", harness, ids[i]);
        FILE *vf = fopen(vp, "r"); char keep[240] = "VERDICT|NONE";
        if (vf) { while (fgets(ln, sizeof ln, vf)) { char *t = trim(ln); if (!strncmp(t, "VERDICT|", 8)) snprintf(keep, sizeof keep, "%s", t); } fclose(vf); }
        snprintf(verdicts[i], sizeof verdicts[i], "%s %s rc=%d", ids[i], keep, rc);
        printf("TEST|%s\n", verdicts[i]); fflush(stdout);
        if (rc != 0 || !strstr(keep, "VERDICT|PASS")) failed++;
    }
    stage("doc");
    char doc[P]; snprintf(doc, sizeof doc, "%s/0.proof-all.md", out);
    FILE *df = fopen(doc, "w");
    if (!df) { fprintf(stderr, "proof_all_op: cannot write doc\n"); return 1; }
    fprintf(df, "# Run all\n\nStages: make, test, present, doc, continue.\n\n");
    fprintf(df, "Failed cases: %d of %d.\n\n", failed, n);
    for (int i = 0; i < n; i++) fprintf(df, "- %s\n", verdicts[i]);
    fprintf(df, "\nThe video is video.mp4 in this folder (silent slides of these verdicts).\n");
    fclose(df);
    stage("present");
    char deck[P]; snprintf(deck, sizeof deck, "%s/deck.txt", out);
    FILE *deckf = fopen(deck, "w");
    if (!deckf) return 1;
    fprintf(deckf, "TITLE | Run all | make, test, present, doc, continue | The harness menu ran every confirmed-proof case\n");
    fprintf(deckf, "FOOTER | progress cut\n");
    for (int i = 0; i < n; i++) fprintf(deckf, "SLIDE | %s | %s | %s\n", ids[i], verdicts[i], verdicts[i]);
    fclose(deckf);
    char *ad[] = { video, "deck", deck, (char *)out, NULL };
    if (run_cwd(ad, out)) { fprintf(stderr, "proof_all_op: slides failed\n"); return 1; }
    char *ar[] = { video, "render", (char *)out, "--no-tts", NULL };
    if (run_cwd(ar, out)) { fprintf(stderr, "proof_all_op: video failed\n"); return 1; }
    stage("continue");
    printf("CONTINUE\n");
    return failed ? 1 : 0;
}
