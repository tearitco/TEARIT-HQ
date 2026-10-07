/* harness_verdict_op - turns a results ledger into a verdict file (the second op a pal harness execs, after harness_case_op).
 * Usage: harness_verdict_op <cases.pdl>      reads that case file's `RESULTS | <path>` row, finds the LAST `RUN|` row in the ledger (the run just made; the ledger is
 *        append-only across runs) and counts the PASS / FAIL rows after it. Writes <results>.verdict.txt (overwritten each run):
 *            VERDICT|PASS|passed=N|failed=0|cases=<file>      or      VERDICT|FAIL|passed=N|failed=M|cases=<file>   followed by every FAIL row of this run
 *        and also appends the same VERDICT line to the ledger. Exit 0 = pass, 1 = fail, 2 = usage (a pal ignores it; read the verdict file). */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#define P 4096
int main(int argc, char **argv) {
    char cpath[P], cdir[P], line[8192], results[P] = "", vpath[P + 24]; FILE *f; long run_off = 0, off = 0; int pass = 0, fail = 0;
    if (argc < 2 || !realpath(argv[1], cpath)) { fprintf(stderr, "usage: harness_verdict_op <cases.pdl>\n"); return 2; }
    snprintf(cdir, sizeof cdir, "%s", cpath); { char *d = dirname(cdir); memmove(cdir, d, strlen(d) + 1); }
    if (!(f = fopen(cpath, "r"))) return 2;
    while (fgets(line, sizeof line, f)) { char *p = line; while (*p == ' ') p++; if (!strncmp(p, "RESULTS", 7)) { char *b = strchr(p, '|'); if (b) { b++; while (*b == ' ') b++; b[strcspn(b, "\r\n")] = 0; char *e = b + strlen(b); while (e > b && e[-1] == ' ') *--e = 0; snprintf(results, sizeof results, "%s/%s", cdir, b); } break; } }
    fclose(f);
    if (!results[0] || !(f = fopen(results, "r"))) { fprintf(stderr, "harness_verdict_op: no results ledger\n"); return 2; }
    while (fgets(line, sizeof line, f)) { if (!strncmp(line, "RUN|", 4)) run_off = off; off = ftell(f); }
    fseek(f, run_off, SEEK_SET);
    snprintf(vpath, sizeof vpath, "%s.verdict.txt", results);
    {
        char fails[16384] = ""; size_t fl = 0;
        while (fgets(line, sizeof line, f)) {
            if (!strncmp(line, "PASS|", 5)) pass++;
            else if (!strncmp(line, "FAIL|", 5)) { fail++; if (fl + strlen(line) < sizeof fails) { memcpy(fails + fl, line, strlen(line)); fl += strlen(line); fails[fl] = 0; } }
        }
        fclose(f);
        {
            char v[P + 128]; FILE *o;
            snprintf(v, sizeof v, "VERDICT|%s|passed=%d|failed=%d|cases=%s", (fail == 0 && pass > 0) ? "PASS" : "FAIL", pass, fail, cpath);
            if ((o = fopen(vpath, "w"))) { fprintf(o, "%s\n%s", v, fails); fclose(o); }
            if ((o = fopen(results, "a"))) { fprintf(o, "%s\n", v); fclose(o); }
            puts(v);
        }
    }
    return (fail == 0 && pass > 0) ? 0 : 1;
}
