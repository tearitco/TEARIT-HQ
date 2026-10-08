/* joint_tune - the WEIGHTS-MANAGER layer: every change to a tunable weight goes through here, is bounded, and leaves a labeled ledger row.
 *
 * Usage:  joint_tune <apply|dry> <joints.pdl> <values_file> <tuning_ledger> <key> <delta> <proposer> <reason...>
 *   joints.pdl    JOINT rows (spec section 0):  JOINT | id | area | key | value | step=N | min=N | max=N | autonomy=N | owner_seat=N | note="..."
 *                 The `value` column is NOT the live value: the live value stays in <values_file> (key=int lines), so a weight has ONE home.
 *   values_file   e.g. eden/conductor/weights.pdl  (comment lines and unknown lines are kept byte for byte)
 *   tuning_ledger append-only; one row per applied move:  TUNE | t<n> | layer=weights-manager | joint=<id> | key=<k> | before=<a> | after=<b> | delta=<d> | proposer=<p> | reason="<text>" | outcome=pending
 *                 `outcome=pending` is later answered by a FEEDBACK row (valence +1/-1, concept=<key>, layer=weights-manager): the tuner's own moves are scored.
 * Rules: autonomy=0 -> only proposer `human` may move it. autonomy>=1 -> any proposer, but |delta| <= step. A human move may be any size as long as the result stays in [min,max].
 *        delta must be a non-zero integer. A key with no JOINT row cannot be tuned at all (no joint = no move).
 * Exit 0 applied (or dry ok) | 2 usage/unreadable file | 3 out of [min,max] | 4 proposer/step rule refused | 5 no JOINT row / key not in values_file.
 * Prints `OK <key> <before> -> <after>` or `REFUSED <reason>`. Every path comes from argv (no absolute path in this file).
 * Build: gcc -std=gnu11 -Wall -Wextra -Werror -O2 -o joint_tune.+x joint_tune.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#define L 4096
static int field_num(const char *row, const char *name, long *out) {   /* ` name=123` inside a pipe-separated row */
    char pat[64]; snprintf(pat, sizeof pat, "%s=", name);
    const char *p = row; while ((p = strstr(p, pat))) { if (p == row || p[-1] == ' ' || p[-1] == '|') { char *e; errno = 0; long v = strtol(p + strlen(pat), &e, 10); if (e == p + strlen(pat) || errno) return 0; *out = v; return 1; } p++; }
    return 0;
}
static int col(const char *row, int n, char *out, size_t sz) {          /* trimmed n-th (0-based) pipe-separated column */
    const char *p = row; for (int i = 0; i < n; i++) { p = strchr(p, '|'); if (!p) return 0; p++; }
    while (*p == ' ') p++;
    size_t k = 0;
    while (p[k] && p[k] != '|' && p[k] != '\n') k++;
    while (k && p[k - 1] == ' ') k--;
    if (k >= sz) k = sz - 1;
    memcpy(out, p, k); out[k] = 0; return 1;
}
static long ledger_rows(const char *path) { FILE *f = fopen(path, "r"); char ln[L]; long n = 0; if (!f) return 0; while (fgets(ln, sizeof ln, f)) if (!strncmp(ln, "TUNE |", 6)) n++; fclose(f); return n; }
int main(int argc, char **argv) {
    if (argc < 9 || (strcmp(argv[1], "apply") && strcmp(argv[1], "dry"))) { fprintf(stderr, "usage: joint_tune <apply|dry> <joints.pdl> <values_file> <tuning_ledger> <key> <delta> <proposer> <reason...>\n"); return 2; }
    const char *joints = argv[2], *values = argv[3], *ledger = argv[4], *key = argv[5], *prop = argv[7]; int dry = !strcmp(argv[1], "dry");
    char *e; errno = 0; long delta = strtol(argv[6], &e, 10);
    if (*e || errno || delta == 0 || !*argv[6]) { fprintf(stderr, "delta must be a non-zero integer\n"); return 2; }
    char reason[L] = ""; for (int i = 8; i < argc; i++) { size_t u = strlen(reason); snprintf(reason + u, sizeof reason - u, "%s%s", i > 8 ? " " : "", argv[i]); }
    for (char *q = reason; *q; q++) if (*q == '"' || *q == '|' || *q == '\n') *q = '\'';
    FILE *jf = fopen(joints, "r"); if (!jf) { fprintf(stderr, "cannot read %s\n", joints); return 2; }
    char ln[L], jid[64] = ""; long step = 0, mn = 0, mx = 0, aut = 0; int found = 0;
    while (!found && fgets(ln, sizeof ln, jf)) {
        char k[128]; if (strncmp(ln, "JOINT |", 7) || !col(ln, 3, k, sizeof k) || strcmp(k, key)) continue;
        col(ln, 1, jid, sizeof jid); if (!field_num(ln, "step", &step) || !field_num(ln, "min", &mn) || !field_num(ln, "max", &mx) || !field_num(ln, "autonomy", &aut)) { printf("REFUSED joint row for %s lacks step/min/max/autonomy\n", key); fclose(jf); return 5; }
        found = 1;
    }
    fclose(jf);
    if (!found) { printf("REFUSED no JOINT row for %s (no joint = no move)\n", key); return 5; }
    int human = !strcmp(prop, "human");
    if (aut == 0 && !human) { printf("REFUSED %s has autonomy=0: only proposer=human may move it\n", key); return 4; }
    if (!human && labs(delta) > step) { printf("REFUSED |delta|=%ld exceeds step=%ld for a non-human proposer\n", labs(delta), step); return 4; }
    FILE *vf = fopen(values, "r"); if (!vf) { fprintf(stderr, "cannot read %s\n", values); return 2; }
    char tmp[L + 8]; snprintf(tmp, sizeof tmp, "%s.tmp", values); FILE *of = dry ? NULL : fopen(tmp, "w"); if (!dry && !of) { fclose(vf); fprintf(stderr, "cannot write %s\n", tmp); return 2; }
    size_t kl = strlen(key); long before = 0, after = 0; int hit = 0;
    while (fgets(ln, sizeof ln, vf)) {
        if (!hit && !strncmp(ln, key, kl) && ln[kl] == '=') {
            before = atol(ln + kl + 1); after = before + delta; hit = 1;
            if (after < mn || after > mx) { printf("REFUSED %s %ld -> %ld leaves [%ld,%ld]\n", key, before, after, mn, mx); fclose(vf); if (of) { fclose(of); remove(tmp); } return 3; }
            if (of) fprintf(of, "%s=%ld\n", key, after);
        } else if (of) { fputs(ln, of); if (!strchr(ln, '\n')) fputc('\n', of); }
    }
    fclose(vf);
    if (!hit) { if (of) { fclose(of); remove(tmp); } printf("REFUSED %s is not in %s\n", key, values); return 5; }
    if (!dry) {
        fclose(of); if (rename(tmp, values)) { fprintf(stderr, "cannot replace %s\n", values); return 2; }
        FILE *lf = fopen(ledger, "a"); if (!lf) { fprintf(stderr, "applied but cannot append %s\n", ledger); return 2; }
        fprintf(lf, "TUNE | t%ld | layer=weights-manager | joint=%s | key=%s | before=%ld | after=%ld | delta=%+ld | proposer=%s | reason=\"%s\" | outcome=pending\n", ledger_rows(ledger) + 1, jid, key, before, after, delta, prop, reason);
        fclose(lf);
    }
    printf("OK %s %ld -> %ld%s\n", key, before, after, dry ? " (dry, nothing written)" : "");
    return 0;
}
