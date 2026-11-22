/* tool_ab_op - the Toolformer-style CONTROL RUN (XO nov-22 doc 4, section 0.1): a tool call is credited only if the OUTCOME is better with it than without it. The harness verdict is the loss.
 * Usage: tool_ab_op <rules.pdl> <case.pdl> <call.pdl> <bankcase.pdl> --task <task> --op <op> --house <house root> [--baseline <rows.pdl>] [--case-op <harness_case_op>] [--bank-op <harness_bank_op>] [--log <log> --log-op <tool_log_op>]
 *        tool_ab_op --describe
 *   <case.pdl>  an ordinary harness_case_op case (RESULTS row optional, it is replaced) with ONE marker row `CALL_HERE |`. Avoid relative INCLUDE rows (the derived cases live in a /tmp scratch folder; use $HOUSE paths).
 *   <call.pdl>  the rows of the call (e.g. a RUN row of the op and its args); WITH = the marker replaced by these rows; WITHOUT = the marker replaced by --baseline rows, or dropped.
 *   <bankcase.pdl> a case file whose `RESULTS | <path>` ledger and bank/<name>.behavior.pdl sidecar harness_bank_op keeps (the reward/punish counts of the op): the verdict is appended there as ONE row
 *               (`PASS|tool_ab/<task>/<op>` when credited, `FAIL|tool_ab/<task>/<op>|<useless|harmful>` otherwise) and `harness_bank_op update` turns it into the Laplace weight. No other path writes the weight.
 * Rule: WITH passes - WITHOUT passes >= RULE tool_ab_margin -> CREDIT (reward). Fewer passes with the call -> HARMFUL (punish). Anything between -> USELESS (not credited, punished).
 * Output: `AB|<task>|<op>|with=<pass>/<fail>|without=<pass>/<fail>|<CREDIT|USELESS|HARMFUL>`. Exit 0 CREDIT, 1 USELESS or HARMFUL, 2 usage / refusal (missing RULE, no marker, unreadable file).
 * Both runs happen in their own /tmp/hc_* scratch dirs (the case's SCRATCH row); no network; no shell. */
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include "../tool_rules.c"
#define P 4096
static int run(char *const argv[]) { pid_t p = fork(); if (p < 0) return -1; if (p == 0) { int d = open("/dev/null", O_WRONLY); if (d >= 0) { dup2(d, 1); dup2(d, 2); } execv(argv[0], argv); _exit(127); } int st = 0; waitpid(p, &st, 0); return WIFEXITED(st) ? WEXITSTATUS(st) : -1; }
static int copy_rows(FILE *o, const char *path) { FILE *f = fopen(path, "r"); if (!f) return -1; char ln[8192]; while (fgets(ln, sizeof ln, f)) { size_t n = strlen(ln); fputs(ln, o); if (n && ln[n - 1] != '\n') fputc('\n', o); } fclose(f); return 0; }
/* derive a case: every row of src, the RESULTS row replaced, the CALL_HERE row replaced by the rows of rep (or dropped). returns 1 if the marker was seen, -1 on error */
static int derive(const char *src, const char *dst, const char *results, const char *rep) {
    FILE *f = fopen(src, "r"), *o = fopen(dst, "w"); if (!f || !o) { if (f) fclose(f); if (o) fclose(o); return -1; }
    fprintf(o, "RESULTS | %s\n", results); char ln[8192]; int seen = 0;
    while (fgets(ln, sizeof ln, f)) { char t[8192]; snprintf(t, sizeof t, "%s", ln); char *p = tr_trim(t);
        if (!strncmp(p, "RESULTS", 7)) continue;
        if (!strncmp(p, "CALL_HERE", 9)) { seen = 1; if (rep && copy_rows(o, rep) != 0) { fclose(f); fclose(o); return -1; } continue; }
        fputs(ln, o); }
    fclose(f); fclose(o); return seen;
}
/* passes and fails in a results ledger (rows after the last RUN row, which is all of them for a fresh ledger) */
static int count(const char *res, int *pass, int *fail) { FILE *f = fopen(res, "r"); if (!f) return -1; char ln[8192]; *pass = *fail = 0; while (fgets(ln, sizeof ln, f)) { if (!strncmp(ln, "RUN|", 4)) *pass = *fail = 0; else if (!strncmp(ln, "PASS|", 5)) (*pass)++; else if (!strncmp(ln, "FAIL|", 5)) (*fail)++; } fclose(f); return 0; }
int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--describe")) { puts("tool_ab_op: IN <rules.pdl> <case.pdl with a CALL_HERE row> <call rows> <bankcase.pdl> --task --op --house [--baseline] | OUT `AB|task|op|with=p/f|without=p/f|CREDIT|USELESS|HARMFUL`, one PASS/FAIL row into the bank case's ledger then harness_bank_op update (the weight) | RULE keys: tool_ab_margin | exit 0 credit, 1 not credited, 2 refusal"); return 0; }
    if (argc < 5) { fprintf(stderr, "usage: tool_ab_op <rules.pdl> <case.pdl> <call.pdl> <bankcase.pdl> --task T --op O --house H [...]   (or --describe)\n"); return 2; }
    const char *rules = argv[1], *cs = argv[2], *call = argv[3], *bank = argv[4], *task = NULL, *opn = NULL, *house = NULL, *base = NULL, *caseop = NULL, *bankop = NULL, *log = NULL, *logop = NULL;
    for (int i = 5; i + 1 < argc; i += 2) { if (!strcmp(argv[i], "--task")) task = argv[i + 1]; else if (!strcmp(argv[i], "--op")) opn = argv[i + 1]; else if (!strcmp(argv[i], "--house")) house = argv[i + 1]; else if (!strcmp(argv[i], "--baseline")) base = argv[i + 1];
        else if (!strcmp(argv[i], "--case-op")) caseop = argv[i + 1]; else if (!strcmp(argv[i], "--bank-op")) bankop = argv[i + 1]; else if (!strcmp(argv[i], "--log")) log = argv[i + 1]; else if (!strcmp(argv[i], "--log-op")) logop = argv[i + 1]; else { fprintf(stderr, "tool_ab_op: unknown option %s\n", argv[i]); return 2; } }
    if (!task || !opn || !house) { fprintf(stderr, "tool_ab_op: --task, --op and --house are required\n"); return 2; }
    if (!caseop || !bankop) { fprintf(stderr, "tool_ab_op: --case-op and --bank-op are required (paths of harness_case_op and harness_bank_op)\n"); return 2; }
    if (log && !logop) { fprintf(stderr, "tool_ab_op: --log needs --log-op\n"); return 2; }
    if (tr_load(rules) != 0) { fprintf(stderr, "tool_ab_op: refused: cannot read rules file\n"); return 2; }
    double margin; if (tr_num("tool_ab_op", "tool_ab_margin", &margin)) return 2;
    char dir[] = "/tmp/ab_XXXXXX"; if (!mkdtemp(dir)) { fprintf(stderr, "tool_ab_op: no scratch dir\n"); return 2; }
    char cw[P], cwo[P], rw[P], rwo[P]; snprintf(cw, sizeof cw, "%s/with.pdl", dir); snprintf(cwo, sizeof cwo, "%s/without.pdl", dir); snprintf(rw, sizeof rw, "%s/with.txt", dir); snprintf(rwo, sizeof rwo, "%s/without.txt", dir);
    int a = derive(cs, cw, "with.txt", call), b = derive(cs, cwo, "without.txt", base);   /* RESULTS is relative to the derived case folder */
    int rc = 2;
    if (a < 0 || b < 0) fprintf(stderr, "tool_ab_op: refused: cannot read case, call or baseline file\n");
    else if (a == 0) fprintf(stderr, "tool_ab_op: refused: the case has no CALL_HERE row\n");
    else {
        char *c1[] = { (char *)caseop, cw, (char *)house, NULL }, *c2[] = { (char *)caseop, cwo, (char *)house, NULL }; run(c1); run(c2);
        int wp, wf, op_, of;
        if (count(rw, &wp, &wf) || count(rwo, &op_, &of)) fprintf(stderr, "tool_ab_op: a run wrote no ledger\n");
        else {
            const char *v = (wp - op_ >= margin) ? "CREDIT" : (wp < op_) ? "HARMFUL" : "USELESS"; int ok = !strcmp(v, "CREDIT");
            printf("AB|%s|%s|with=%d/%d|without=%d/%d|%s\n", task, opn, wp, wf, op_, of, v);
            /* the verdict row into the bank case's ledger, then the bank op turns it into the weight */
            char bp[P], line[P], res[P] = "", bd[P]; snprintf(bp, sizeof bp, "%s", bank); FILE *bf = fopen(bp, "r");
            if (bf) { while (fgets(line, sizeof line, bf)) { char *p = tr_trim(line); if (!strncmp(p, "RESULTS", 7)) { char *bar = strchr(p, '|'); if (bar) { snprintf(bd, sizeof bd, "%s", bp); char *dn = dirname(bd); snprintf(res, sizeof res, "%s/%s", dn, tr_trim(bar + 1)); } break; } } fclose(bf); }
            if (!res[0]) fprintf(stderr, "tool_ab_op: refused: the bank case has no RESULTS row\n");
            else {
                FILE *rf = fopen(res, "a"); if (!rf) fprintf(stderr, "tool_ab_op: cannot append the verdict row\n");
                else {
                    if (ok) fprintf(rf, "PASS|tool_ab/%s/%s\n", task, opn); else fprintf(rf, "FAIL|tool_ab/%s/%s|%s\n", task, opn, !strcmp(v, "HARMFUL") ? "harmful" : "useless"); fclose(rf);
                    char *u[] = { (char *)bankop, "update", bp, NULL }; run(u);
                    if (log) { char cb[P]; snprintf(cb, sizeof cb, "%s", call); char *l[] = { (char *)logop, (char *)rules, "add", (char *)log, (char *)task, "ab-control", (char *)opn, basename(cb), ok ? "PASS" : "FAIL", "rules", "0", NULL }; run(l); }
                    rc = ok ? 0 : 1;
                }
            }
        }
    }
    { char *rm[] = { "/bin/rm", "-rf", dir, NULL }; run(rm); }
    return rc;
}
