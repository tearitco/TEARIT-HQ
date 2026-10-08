/* ledger_to_feedback - turn a quest_ledger.txt into FEEDBACK rows the promotion ledger can replay (the delegation loop -> scoring bridge).
 *
 * Usage:  ledger_to_feedback <quest_ledger.txt> <worker_dir> <family> [layer]
 *   worker_dir  directory of ONE worker (must exist); rows are appended to <worker_dir>/obs_feedback_log.txt, the file promotion_ledger.+x replays.
 *   family      task family, written as concept=<family> (promotion_ledger matches concept against a candidate's slot).
 *   layer       optional label (default "delegation") written as layer=<layer>: which level of the learning recursion produced the row.
 * One row per `ITER|n|...` line of the ledger:  [<date>] FEEDBACK | id=<quest>#<n> | target=<worker> | valence=+1 (verdict PASS) / -1 (FAIL) | concept=<family>
 *   | intensity=1 | layer=<layer> | quest=<quest> | iter=<n> | tokens=<total, 0 when unknown>
 * Verdict is read from `verdict=PASS|FAIL` or `VERDICT|PASS|FAIL` (an iteration with neither is skipped and counted). Re-running is safe: a row whose
 * `id=<quest>#<n> |` is already in the log is skipped. The date is the ledger's own QUEST date, so no wall clock is read. Every path comes from argv.
 * Prints `wrote=N skipped=M unparsed=K`. Exit 0 ok | 2 usage / unreadable input / worker_dir missing | 3 no ITER line found.
 * Build: gcc -std=gnu11 -Wall -Wextra -Werror -O2 -o ledger_to_feedback.+x ledger_to_feedback.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#define L 4096
static int already(const char *log, const char *id) {          /* 1 when `id=<id> |` is already a row of the log */
    FILE *f = fopen(log, "r"); char ln[L], key[512]; int hit = 0;
    snprintf(key, sizeof key, "id=%s |", id);
    if (!f) return 0;
    while (!hit && fgets(ln, sizeof ln, f)) if (strstr(ln, key)) hit = 1;
    fclose(f); return hit;
}
static int verdict_of(const char *ln) {                          /* +1 PASS, -1 FAIL, 0 not found */
    const char *p = strstr(ln, "verdict=");
    if (p) p += 8; else { p = strstr(ln, "VERDICT|"); if (p) p += 8; }
    if (!p) return 0;
    if (!strncmp(p, "PASS", 4)) return 1;
    if (!strncmp(p, "FAIL", 4)) return -1;
    return 0;
}
static long tokens_of(const char *ln) { const char *p = strstr(ln, "total="); return p ? atol(p + 6) : 0; }
int main(int argc, char **argv) {
    struct stat sb;
    if (argc < 4 || argc > 5 || stat(argv[2], &sb) || !S_ISDIR(sb.st_mode)) { fprintf(stderr, "usage: ledger_to_feedback <quest_ledger.txt> <worker_dir> <family> [layer]\n"); return 2; }
    const char *layer = argc == 5 ? argv[4] : "delegation";
    FILE *in = fopen(argv[1], "r"); if (!in) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
    char log[L], ln[L], quest[128] = "quest", date[32] = "0000-00-00", worker[160]; int wrote = 0, skipped = 0, bad = 0, seen = 0;
    const char *wb = strrchr(argv[2], '/'); snprintf(worker, sizeof worker, "%.150s", wb ? wb + 1 : argv[2]);
    snprintf(log, sizeof log, "%s/obs_feedback_log.txt", argv[2]);
    while (fgets(ln, sizeof ln, in)) {
        ln[strcspn(ln, "\r\n")] = 0;
        if (!strncmp(ln, "QUEST|", 6)) { char *a = ln + 6, *b = strchr(a, '|'); if (b) { *b = 0; snprintf(quest, sizeof quest, "%.100s", a); a = b + 1; b = strchr(a, '|'); if (b) *b = 0; snprintf(date, sizeof date, "%.20s", a); } continue; }
        if (strncmp(ln, "ITER|", 5)) continue;
        seen++;
        int n = atoi(ln + 5), v = verdict_of(ln); char id[256];
        if (!v) { bad++; continue; }
        snprintf(id, sizeof id, "%.120s#%d", quest, n);
        if (already(log, id)) { skipped++; continue; }
        FILE *o = fopen(log, "a"); if (!o) { fprintf(stderr, "cannot append %s\n", log); fclose(in); return 2; }
        fprintf(o, "[%s] FEEDBACK | id=%s | target=%s | valence=%+d | concept=%s | intensity=1 | layer=%s | quest=%s | iter=%d | tokens=%ld\n", date, id, worker, v, argv[3], layer, quest, n, tokens_of(ln));
        fclose(o); wrote++;
    }
    fclose(in);
    printf("wrote=%d skipped=%d unparsed=%d\n", wrote, skipped, bad);
    return seen ? 0 : 3;
}
