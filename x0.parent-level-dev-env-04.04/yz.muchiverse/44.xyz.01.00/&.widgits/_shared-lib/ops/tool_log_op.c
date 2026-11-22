/* tool_log_op - the tool-use ATTEMPT LOG: append-only DATA rows `DATA|task|input|op|args|verdict|tier|tokens|ts`, the raw material for the Toolformer-style filter (tool_ab_op), the scorer (tool_score_op)
 * and any later distiller (XO nov-22 doc 4, section 0). ONLY harness-generated rows go here (a verdict from a case, a phase table or tool_ab_op): never a private transcript, a prompt or a path of the owner.
 * Usage: tool_log_op <rules.pdl> add <log> <task> <input> <op> <args> <verdict> <tier> <tokens>    append one row (ts = now); refuses a field with a bar or newline, a field longer than RULE tool_log_max_field,
 *                                                                                              a verdict not listed as RULE tool_verdict, a tier not listed as RULE tool_tier, a non-integer tokens
 *        tool_log_op <rules.pdl> tail <log> <n>        the last n rows
 *        tool_log_op <rules.pdl> stats <log>           `OP|<op>|<rows>`, `VERDICT|<verdict>|<rows>`, `TIER|<tier>|<rows>`, `TOTAL|<rows>`
 *        tool_log_op --describe
 * CAP (RUNTIME-FILE-GROWTH-STANDARD.md): after every add the file keeps only the LAST RULE tool_log_max_rows rows (streamed, so a file of any size is trimmed, never skipped); second guard: cpu_guard trim_tail.
 * Exit 0 ok, 2 usage / refusal. A missing RULE refuses. */
#define _GNU_SOURCE
#include <time.h>
#include <ctype.h>
#include "../tool_rules.c"
#define MAXK 256
typedef struct { char k[TR_LEN]; long n; } Cnt;
static int bump(Cnt *c, int *nc, const char *k) { for (int i = 0; i < *nc; i++) if (!strcmp(c[i].k, k)) { c[i].n++; return 0; } if (*nc >= MAXK) return -1; snprintf(c[*nc].k, TR_LEN, "%s", k); c[*nc].n = 1; (*nc)++; return 0; }
static int bad(const char *s, double maxf) { if ((double)strlen(s) > maxf) return 2; for (; *s; s++) if (*s == '|' || *s == '\n' || *s == '\r') return 1; return 0; }
static int split(char *ln, char **f, int max) { int n = 0; char *s = ln; while (n < max) { f[n++] = s; char *b = strchr(s, '|'); if (!b) break; *b = 0; s = b + 1; } return n; }
static long count_rows(const char *log) { FILE *f = fopen(log, "r"); if (!f) return 0; long n = 0; int c, any = 0; while ((c = fgetc(f)) != EOF) { any = 1; if (c == '\n') { n++; any = 0; } } if (any) n++; fclose(f); return n; }
static int trim_log(const char *log, long keep) {
    long n = count_rows(log); if (n <= keep) return 0; long drop = n - keep; char tmp[4200]; snprintf(tmp, sizeof tmp, "%s.tmp", log);
    FILE *in = fopen(log, "r"), *o = fopen(tmp, "w"); if (!in || !o) { if (in) fclose(in); if (o) fclose(o); return -1; }
    char ln[8192]; long i = 0; while (fgets(ln, sizeof ln, in)) { if (i++ >= drop) fputs(ln, o); } fclose(in); fclose(o); return rename(tmp, log);
}
int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--describe")) { puts("tool_log_op: IN <rules.pdl> add|tail|stats <log> ... | OUT append-only DATA|task|input|op|args|verdict|tier|tokens|ts rows (harness-generated only), tail n rows, stats per op/verdict/tier | RULE keys: tool_log_max_rows tool_log_max_field tool_tier tool_verdict | module: _shared-lib/tool_rules.c"); return 0; }
    if (argc < 4) { fprintf(stderr, "usage: tool_log_op <rules.pdl> add|tail|stats <log> ...   (or --describe)\n"); return 2; }
    if (tr_load(argv[1]) != 0) { fprintf(stderr, "tool_log_op: refused: cannot read rules file\n"); return 2; }
    const char *cmd = argv[2], *log = argv[3]; double maxr, maxf;
    if (!strcmp(cmd, "add")) {
        if (argc != 11) { fprintf(stderr, "tool_log_op add: need task input op args verdict tier tokens\n"); return 2; }
        if (tr_num("tool_log_op", "tool_log_max_rows", &maxr) || tr_num("tool_log_op", "tool_log_max_field", &maxf)) return 2;
        static const char *nm[] = { "task", "input", "op", "args", "verdict", "tier", "tokens" };
        for (int i = 0; i < 7; i++) { int b = bad(argv[4 + i], maxf); if (b) { fprintf(stderr, "tool_log_op: refused: field %s %s\n", nm[i], b == 1 ? "holds a bar or newline" : "is longer than RULE tool_log_max_field"); return 2; } if (!*argv[4 + i]) { fprintf(stderr, "tool_log_op: refused: field %s is empty\n", nm[i]); return 2; } }
        if (!tr_in("tool_verdict", argv[8])) { fprintf(stderr, "tool_log_op: refused: verdict %s is not a RULE tool_verdict\n", argv[8]); return 2; }
        if (!tr_in("tool_tier", argv[9])) { fprintf(stderr, "tool_log_op: refused: tier %s is not a RULE tool_tier\n", argv[9]); return 2; }
        for (const char *t = argv[10]; *t; t++) if (!isdigit((unsigned char)*t)) { fprintf(stderr, "tool_log_op: refused: tokens must be a whole number\n"); return 2; }
        FILE *f = fopen(log, "a"); if (!f) { fprintf(stderr, "tool_log_op: cannot open log\n"); return 2; }
        fprintf(f, "DATA|%s|%s|%s|%s|%s|%s|%s|%ld\n", argv[4], argv[5], argv[6], argv[7], argv[8], argv[9], argv[10], (long)time(NULL)); fclose(f);
        if (trim_log(log, (long)maxr) != 0) { fprintf(stderr, "tool_log_op: trim failed\n"); return 2; }
        return 0;
    }
    if (!strcmp(cmd, "tail")) {
        if (argc != 5) { fprintf(stderr, "tool_log_op tail <log> <n>\n"); return 2; }
        long n = atol(argv[4]), tot = count_rows(log), skip = tot > n ? tot - n : 0, i = 0; FILE *f = fopen(log, "r"); if (!f) return 0;
        char ln[8192]; while (fgets(ln, sizeof ln, f)) if (i++ >= skip) fputs(ln, stdout); fclose(f); return 0;
    }
    if (!strcmp(cmd, "stats")) {
        static Cnt ops[MAXK], vs[MAXK], ts[MAXK]; int no = 0, nv = 0, nt = 0; long tot = 0; FILE *f = fopen(log, "r"); char ln[8192];
        if (f) { while (fgets(ln, sizeof ln, f)) { ln[strcspn(ln, "\r\n")] = 0; char *c[10]; int k = split(ln, c, 10); if (k < 9 || strcmp(c[0], "DATA")) continue; tot++;
            if (bump(ops, &no, c[3]) || bump(vs, &nv, c[5]) || bump(ts, &nt, c[6])) { fprintf(stderr, "tool_log_op: too many distinct values\n"); fclose(f); return 2; } } fclose(f); }
        for (int i = 0; i < no; i++) printf("OP|%s|%ld\n", ops[i].k, ops[i].n);
        for (int i = 0; i < nv; i++) printf("VERDICT|%s|%ld\n", vs[i].k, vs[i].n);
        for (int i = 0; i < nt; i++) printf("TIER|%s|%ld\n", ts[i].k, ts[i].n);
        printf("TOTAL|%ld\n", tot); return 0;
    }
    fprintf(stderr, "tool_log_op: unknown verb %s\n", cmd); return 2;
}
