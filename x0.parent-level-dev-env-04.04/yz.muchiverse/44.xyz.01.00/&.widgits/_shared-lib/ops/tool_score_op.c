/* tool_score_op - the in-house ToolkenGPT: rank the ops / phases of the harness behavior bank for a goal in words, and FIRE the best one only when its score passes a threshold that is DATA.
 * Ranking is NOT re-implemented: it is `harness_bank_op find` (keyword 1.0, synonym weight, concept slot 0.6*w, sentence 0.25, summed over the goal words, times the entry's live Laplace weight), so one authority decides what a weight means.
 * Usage: tool_score_op <rules.pdl> <bank_dir> <goal word>... --bank-op <harness_bank_op> [--state <key=value file> --slots <slots.pdl>]
 *        tool_score_op --describe
 * Threshold: RULE tool_fire_min (concept-bank/tool_rules.pdl). A missing rule REFUSES (exit 2); there is no default.
 * Margin (2026-11-22): RULE tool_fire_margin = the least GAP between the best and the second score, in score units. A closer race is NOFIRE (a wrong FIRE costs more than a NOFIRE, which falls back to reading the route table). 0 turns it off; a missing rule REFUSES.
 *   Measured on 40 goals (harness cases work_router): without a margin 4 of 40 routes were wrong and ALL 40 fired; margin 0.40 blocked all 4 and also blocked 7 right ones. 0.40 is fitted to those 4 errors: re-fit it as the tool_log grows.
 * Slot filler (deterministic, no model): rows `SLOT | <entry id> | <arg> | from | <state key>` in the slots file; an argument is the value of that key in the state file (`key=value` lines, last one wins). Not the bank's own SLOT rows (those point at concepts).
 * Output: `RANK|<n>|<score>|<id>|<file>` for every hit, best first; then ONE of
 *   `FIRE|<id>|<file>|<arg=value ...>`   best score >= RULE tool_fire_min and every slot of that entry has a value (`-` when the entry has no slot rows)
 *   `NOFIRE|best=<score>|min=<rule>`     nothing reached the threshold (or no hits: best=0)
 *   `NOFIRE|best=<score>|min=<rule>|margin=<gap><<rule>|second=<id>`   the best passed but the runner-up is too close
 *   `NOARGS|<id>|<missing key>`          the best entry passed but a state key is missing: it does not fire half-armed
 * Exit 0 FIRE, 1 NOFIRE / NOARGS, 2 usage / refusal. Nothing is written anywhere; the caller runs the op and logs the attempt (tool_log_op) and judges it (tool_ab_op). */
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/wait.h>
#include "../tool_rules.c"
#define P 4096
#define MAXH 256
typedef struct { double score; char id[128], file[256]; } Hit;
static int run_capture(char *const argv[], char *out, size_t cap) {
    int fd[2]; if (pipe(fd)) return -1; pid_t p = fork(); if (p < 0) return -1;
    if (p == 0) { close(fd[0]); dup2(fd[1], 1); close(fd[1]); execv(argv[0], argv); _exit(127); }
    close(fd[1]); size_t n = 0; ssize_t r; while (n + 1 < cap && (r = read(fd[0], out + n, cap - 1 - n)) > 0) n += (size_t)r; out[n] = 0; close(fd[0]);
    int st = 0; waitpid(p, &st, 0); return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}
int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--describe")) { puts("tool_score_op: IN <rules.pdl> <bank_dir> goal words --bank-op P [--state key=value file --slots SLOT|entry|arg|from|key rows] | OUT RANK|n|score|id|file lines (harness_bank_op find) then FIRE|id|file|args, NOFIRE|best|min or NOARGS|id|key | RULE keys: tool_fire_min | exit 0 fire, 1 no fire, 2 refusal"); return 0; }
    if (argc < 5) { fprintf(stderr, "usage: tool_score_op <rules.pdl> <bank_dir> <goal word>... --bank-op P [--state F --slots F]   (or --describe)\n"); return 2; }
    const char *bankdir = argv[2], *bankop = NULL, *state = NULL, *slots = NULL; char *words[64]; int nw = 0;
    for (int i = 3; i < argc; i++) {
        if (!strncmp(argv[i], "--", 2)) { if (i + 1 >= argc) { fprintf(stderr, "tool_score_op: option %s needs a value\n", argv[i]); return 2; } if (!strcmp(argv[i], "--bank-op")) bankop = argv[i + 1]; else if (!strcmp(argv[i], "--state")) state = argv[i + 1]; else if (!strcmp(argv[i], "--slots")) slots = argv[i + 1]; else { fprintf(stderr, "tool_score_op: unknown option %s\n", argv[i]); return 2; } i++; }
        else if (nw < 64) words[nw++] = argv[i];
    }
    if (!bankop || !nw) { fprintf(stderr, "tool_score_op: need goal words and --bank-op\n"); return 2; }
    if ((state == NULL) != (slots == NULL)) { fprintf(stderr, "tool_score_op: --state and --slots go together\n"); return 2; }
    if (tr_load(argv[1]) != 0) { fprintf(stderr, "tool_score_op: refused: cannot read rules file\n"); return 2; }
    double min, margin; if (tr_num("tool_score_op", "tool_fire_min", &min)) return 2;
    if (tr_num("tool_score_op", "tool_fire_margin", &margin)) return 2;
    char *args[80]; int na = 0; args[na++] = (char *)bankop; args[na++] = "find"; args[na++] = (char *)bankdir; for (int i = 0; i < nw; i++) args[na++] = words[i]; args[na] = NULL;
    static char out[1 << 16]; if (run_capture(args, out, sizeof out) != 0) { fprintf(stderr, "tool_score_op: harness_bank_op find failed\n"); return 2; }
    static Hit hits[MAXH]; int nh = 0; for (char *ln = strtok(out, "\n"); ln && nh < MAXH; ln = strtok(NULL, "\n")) { char *a = strchr(ln, '|'); if (!a) continue; *a++ = 0; char *b = strchr(a, '|'); if (!b) continue; *b++ = 0; hits[nh].score = atof(ln); snprintf(hits[nh].id, sizeof hits[nh].id, "%s", a); snprintf(hits[nh].file, sizeof hits[nh].file, "%s", b); nh++; }
    for (int i = 0; i < nh; i++) printf("RANK|%d|%.3f|%s|%s\n", i + 1, hits[i].score, hits[i].id, hits[i].file);
    if (nh == 0 || hits[0].score < min) { printf("NOFIRE|best=%.3f|min=%s\n", nh ? hits[0].score : 0.0, tr_get("tool_fire_min")); return 1; }
    if (nh >= 2 && hits[0].score - hits[1].score < margin) {
        printf("NOFIRE|best=%.3f|min=%s|margin=%.3f<%s|second=%s\n", hits[0].score, tr_get("tool_fire_min"), hits[0].score - hits[1].score, tr_get("tool_fire_margin"), hits[1].id); return 1; }
    char fill[P] = "";
    if (slots) {
        FILE *sf = fopen(slots, "r"); if (!sf) { fprintf(stderr, "tool_score_op: cannot read slots file\n"); return 2; } char ln[1024];
        while (fgets(ln, sizeof ln, sf)) { char *c[6]; int k = 0; char *s = ln; while (k < 6) { c[k++] = s; char *b = strchr(s, '|'); if (!b) break; *b = 0; s = b + 1; } for (int i = 0; i < k; i++) c[i] = tr_trim(c[i]);
            if (k < 5 || strcmp(c[0], "SLOT") || strcmp(c[1], hits[0].id) || strcmp(c[3], "from")) continue;
            char val[512] = ""; int have = 0; FILE *st = fopen(state, "r"); if (!st) { fprintf(stderr, "tool_score_op: cannot read state file\n"); fclose(sf); return 2; } char sl[1024];
            while (fgets(sl, sizeof sl, st)) { char *eq = strchr(sl, '='); if (!eq) continue; *eq = 0; if (!strcmp(tr_trim(sl), c[4])) { snprintf(val, sizeof val, "%s", tr_trim(eq + 1)); have = 1; } } fclose(st);
            if (!have) { printf("NOARGS|%s|%s\n", hits[0].id, c[4]); fclose(sf); return 1; }
            size_t o = strlen(fill); snprintf(fill + o, sizeof fill - o, "%s%s=%s", o ? " " : "", c[2], val); }
        fclose(sf);
    }
    printf("FIRE|%s|%s|%s\n", hits[0].id, hits[0].file, fill[0] ? fill : "-"); return 0;
}
