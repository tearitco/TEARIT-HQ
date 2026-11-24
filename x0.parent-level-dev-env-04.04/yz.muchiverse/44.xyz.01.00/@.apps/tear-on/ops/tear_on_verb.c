/* tear_on_verb - the ENGINE of the tear-on verb pages (BOOK 16.game/EVENT-VERB-PAGES-DESIGN.md; the registered command tear_on_verb).
 *   tear_on_verb.+x <verb>        (lower case, the page name; the chain row is upper case) with the rest of the row in the environment word GAME_ARG (pipe separated, owner and ts last)
 * An event page (@.apps/tear-on/events/<verb>/, written by verbgen from verbs.pdl, played by play_event.sh) runs this; the page is the path, this program is the leaf:
 * it builds the row "<VERB>|<GAME_ARG>", asks tear_on_op to mine it onto the chain (every rule stays in tear_chain_op) and writes play/result.txt
 * ("ok" or "refused: <reason>", a PROJECTION the driver shows on the bar; the ledger is the authority). Exit 0 = the row was accepted.
 * Only the verbs named below are accepted (verbgen check reads the verb comparisons below against verbs.pdl). Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/tear_on_verb.+x tear_on_verb.c */
#define _POSIX_C_SOURCE 200809L
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
static int known(const char *v) {
    return !strcmp(v, "pet") || !strcmp(v, "faucet") || !strcmp(v, "feed") || !strcmp(v, "train") || !strcmp(v, "rest") || !strcmp(v, "compete") ||
           !strcmp(v, "move") || !strcmp(v, "arena") || !strcmp(v, "join") || !strcmp(v, "leave") || !strcmp(v, "fill") || !strcmp(v, "fight") || !strcmp(v, "wopt"); }
int main(int argc, char **argv) {
    if (argc < 2 || !known(argv[1])) { fprintf(stderr, "tear_on_verb: unknown verb\n"); return 2; }
    const char *v = argv[1], *arg = getenv("GAME_ARG"); if (!arg) arg = "";
    for (const char *c = arg; *c; c++) if (*c == '\n' || *c == '\r' || *c == '\'' || (unsigned char)*c < 32) { fprintf(stderr, "tear_on_verb: bad argument\n"); return 2; }
    char self[PATH_MAX]; if (!realpath(argv[0], self)) return 2;
    char house[PATH_MAX]; snprintf(house, sizeof house, "%s", self); for (int i = 0; i < 5; i++) { char *s = strrchr(house, '/'); if (!s) return 2; *s = 0; }   /* <house>/@.apps/tear-on/ops/+x/<this> */
    char xo[PATH_MAX + 16]; snprintf(xo, sizeof xo, "%s/xyzfs", house); struct stat st; if (stat(xo, &st) != 0) { fprintf(stderr, "tear_on_verb: house not found above %s\n", self); return 2; }
    char op[PATH_MAX + 64], row[1024], rp[PATH_MAX + 64], dir[PATH_MAX + 64]; snprintf(op, sizeof op, "%s/&.hq-apps/tear-on/ops/+x/tear_on_op.+x", house); char up[24]; snprintf(up, sizeof up, "%s", v); for (char *q = up; *q; q++) if (*q >= 'a' && *q <= 'z') *q = (char)(*q - 32); snprintf(row, sizeof row, "%s|%s", up, arg);
    snprintf(dir, sizeof dir, "%s/@.apps/tear-on/play", house); mkdir(dir, 0755); snprintf(rp, sizeof rp, "%s/result.txt", dir);
    int p[2]; if (pipe(p)) return 2; pid_t c = fork(); if (c < 0) return 2;
    if (c == 0) { alarm(20); dup2(p[1], 1); dup2(p[1], 2); close(p[0]); close(p[1]); execl(op, op, "do", house, row, (char *)NULL); _exit(127); }
    close(p[1]); char out[600]; size_t n = 0; ssize_t k; while (n < sizeof out - 1 && (k = read(p[0], out + n, sizeof out - 1 - n)) > 0) n += (size_t)k; out[n] = 0; close(p[0]);
    int status = 0; waitpid(c, &status, 0); int code = WIFEXITED(status) ? WEXITSTATUS(status) : 99; char *e = strchr(out, '\n'); if (e) *e = 0;
    FILE *f = fopen(rp, "w"); if (f) { if (code == 0) fputs("ok\n", f); else fprintf(f, "%s\n", out[0] ? out : "refused"); fclose(f); }
    return code; }
