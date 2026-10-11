/* var_cmp reference (sealed) - see prompt.txt for the spec */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
static char DIR_[4096];
static void op_error(const char *msg, const char *a) {
    char p[4200]; snprintf(p, sizeof p, "%s/op_errors.txt", DIR_); FILE *f = fopen(p, "a");
    if (f) { fprintf(f, "ERR|var_cmp|%s|%s\n", msg, a ? a : ""); fclose(f); }
    fprintf(stderr, "var_cmp: %s %s\n", msg, a ? a : "");
}
static int valid_name(const char *s) { if (!(isalpha((unsigned char)*s) || *s == '_')) return 0; for (; *s; s++) if (!(isalnum((unsigned char)*s) || *s == '_')) return 0; return 1; }
static int parse_ll(const char *s, long long *out) {
    const char *p = s; if (*p == '+' || *p == '-') p++; if (!*p) return 0; for (const char *q = p; *q; q++) if (!isdigit((unsigned char)*q)) return 0;
    errno = 0; char *e; long long v = strtoll(s, &e, 10); if (errno || *e) return 0; *out = v; return 1;
}
static long long var_get(const char *name) {
    char p[4200], ln[1024]; snprintf(p, sizeof p, "%s/variables.txt", DIR_); FILE *f = fopen(p, "r"); size_t kl = strlen(name); long long v = 0;
    if (!f) return 0;
    while (fgets(ln, sizeof ln, f)) if (!strncmp(ln, name, kl) && ln[kl] == '=') { char *nl = strpbrk(ln + kl + 1, "\r\n"); if (nl) *nl = 0; if (!parse_ll(ln + kl + 1, &v)) v = 0; break; }
    fclose(f); return v;
}
static int operand(const char *tok, long long *out) { if (parse_ll(tok, out)) return 1; if (valid_name(tok)) { *out = var_get(tok); return 1; } return 0; }
int main(int argc, char **argv) {
    struct stat sb;
    if (argc != 6 || stat(argv[1], &sb) || !S_ISDIR(sb.st_mode)) { fprintf(stderr, "usage: var_cmp <state_dir> <a> <op> <b> <out>\n"); if (argc == 6 || (argc > 1 && !stat(argv[1], &sb) && S_ISDIR(sb.st_mode))) { snprintf(DIR_, sizeof DIR_, "%s", argv[1]); op_error("usage", argv[0]); } return 2; }
    snprintf(DIR_, sizeof DIR_, "%s", argv[1]);
    const char *op = argv[3]; int k = -1;
    const char *sym[] = { "==", "!=", "<", "<=", ">", ">=" }, *wrd[] = { "eq", "ne", "lt", "le", "gt", "ge" };
    for (int i = 0; i < 6; i++) if (!strcmp(op, sym[i]) || !strcmp(op, wrd[i])) k = i;
    long long a, b;
    if (k < 0) { op_error("bad-operator", op); return 2; }
    if (!operand(argv[2], &a)) { op_error("bad-operand", argv[2]); return 2; }
    if (!operand(argv[4], &b)) { op_error("bad-operand", argv[4]); return 2; }
    if (!valid_name(argv[5])) { op_error("bad-out-name", argv[5]); return 2; }
    int r = k == 0 ? a == b : k == 1 ? a != b : k == 2 ? a < b : k == 3 ? a <= b : k == 4 ? a > b : a >= b;
    char p[4200], tp[4300], ln[1024]; snprintf(p, sizeof p, "%s/variables.txt", DIR_); snprintf(tp, sizeof tp, "%s.tmp", p);
    FILE *f = fopen(p, "r"), *o = fopen(tp, "w"); size_t kl = strlen(argv[5]); int done = 0;
    if (!o) { if (f) fclose(f); op_error("cannot-write", p); return 2; }
    while (f && fgets(ln, sizeof ln, f)) {
        size_t L = strlen(ln); int nl = L && ln[L - 1] == '\n';
        if (!done && !strncmp(ln, argv[5], kl) && ln[kl] == '=') { fprintf(o, "%s=%d\n", argv[5], r); done = 1; }
        else { fputs(ln, o); if (!nl) fputc('\n', o); }
    }
    if (f) fclose(f);
    if (!done) fprintf(o, "%s=%d\n", argv[5], r);
    fclose(o); if (rename(tp, p)) { op_error("cannot-rename", p); return 2; }
    printf("%d\n", r); return 0;
}
