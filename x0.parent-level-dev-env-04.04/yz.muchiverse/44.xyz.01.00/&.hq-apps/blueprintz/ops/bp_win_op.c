#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
/* bp_win_op - drive the blueprintz WINDOW LAYER headless: the same request lines a click writes, the same ui.txt the window draws (BOOK
 * 08-roadmap/design-docs/BLUEPRINTZ-DESIGN.md 12.22; the harness row is BPWIN in _shared-lib/harness/ops/playthrough_op.c).
 *
 *   bp_win_op <house> <app_dir> <step> [<step> ...]
 *     step  REQ:<request line>          append it to <app_dir>/state/request.txt (what bp_req does), then wait for the manager: an edit-type
 *                                       request (EDIT: ADD: CDEL UNDO PNEW TRIG:) runs in the background, so wait until the message no longer ends in "..."
 *           EXPECT:<ui key>~<text>      the value of that ui.txt key must contain <text>
 *           EXPECTNOT:<ui key>~<text>   ... must NOT contain <text>
 *   The real blueprintz_manager is started on <app_dir> with state/target.pdl pointing at <house>, and always killed and reaped. In a step, a literal bar is
 *   written \p (the table format uses bars). Exit 0 all steps ok, 1 a step failed (the key, the wanted text and what was there are printed), 2 usage/setup.
 *   It shows the window's logic (selection, command list, refusals, undo, load), not pixels: pixels are the relay screenshots in the design page. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>

static long now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000L + t.tv_nsec / 1000000L; }
static pid_t g_mgr = -1;
static void stop_mgr(void) { if (g_mgr > 0) { kill(g_mgr, SIGTERM); for (int i = 0; i < 20; i++) { int st; if (waitpid(g_mgr, &st, WNOHANG) == g_mgr) { g_mgr = -1; return; } usleep(50000); } kill(g_mgr, SIGKILL); waitpid(g_mgr, NULL, 0); g_mgr = -1; } }
static char g_ui[PATH_MAX];
/* value of one ui.txt key into out ("" when absent) */
static void ui_get(const char *key, char *out, size_t n) {
    out[0] = 0; FILE *f = fopen(g_ui, "r"); if (!f) return;
    char l[4096], pre[200]; snprintf(pre, sizeof pre, "%s=", key); size_t pl = strlen(pre);
    while (fgets(l, sizeof l, f)) if (!strncmp(l, pre, pl)) { l[strcspn(l, "\r\n")] = 0; snprintf(out, n, "%s", l + pl); break; }
    fclose(f);
}
static void unbar(char *s) { char *w = s; for (char *r = s; *r; r++) { if (r[0] == '\\' && r[1] == 'p') { *w++ = '|'; r++; } else *w++ = *r; } *w = 0; }
static int ends_dots(const char *s) { size_t n = strlen(s); return n >= 3 && !strcmp(s + n - 3, "..."); }

int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: bp_win_op <house> <app_dir> <step> [<step> ...]   step = REQ:<line> | EXPECT:<key>~<text> | EXPECTNOT:<key>~<text>\n"); return 2; }
    const char *house = argv[1], *app = argv[2];
    char mgr[PATH_MAX], st[PATH_MAX], req[PATH_MAX], tgt[PATH_MAX];
    snprintf(mgr, sizeof mgr, "%s/+x/blueprintz_manager.+x", app); snprintf(st, sizeof st, "%s/state", app);
    snprintf(g_ui, sizeof g_ui, "%s/ui.txt", st); snprintf(req, sizeof req, "%s/request.txt", st); snprintf(tgt, sizeof tgt, "%s/target.pdl", st);
    if (access(mgr, X_OK)) { fprintf(stderr, "bp_win: no manager at %s\n", mgr); return 2; }
    mkdir(st, 0755); unlink(g_ui);
    { FILE *f = fopen(tgt, "w"); if (!f) return 2; fprintf(f, "TARGET | house | %s\n", house); fclose(f); }
    { FILE *f = fopen(req, "w"); if (f) fclose(f); }       /* start from an empty request file: the manager only reads what arrives after it starts */
    pid_t pid = fork(); if (pid < 0) return 2;
    if (pid == 0) { int fd = open("/dev/null", O_RDWR); if (fd >= 0) { dup2(fd, 0); dup2(fd, 1); dup2(fd, 2); } execl(mgr, mgr, house, app, (char *)NULL); _exit(127); }
    g_mgr = pid; atexit(stop_mgr);
    { long t0 = now_ms(); while (now_ms() - t0 < 6000 && access(g_ui, F_OK)) usleep(50000); if (access(g_ui, F_OK)) { fprintf(stderr, "bp_win: the manager never wrote ui.txt\n"); return 1; } }
    usleep(500000);
    int bad = 0;
    for (int i = 3; i < argc && !bad; i++) {
        char step[2400]; snprintf(step, sizeof step, "%s", argv[i]); unbar(step);
        if (!strncmp(step, "REQ:", 4)) {
            char before[400]; ui_get("msg", before, sizeof before);
            FILE *f = fopen(req, "a"); if (!f) { fprintf(stderr, "bp_win: cannot write the request\n"); return 2; } fprintf(f, "%s\n", step + 4); fclose(f);
            int edit = !strncmp(step + 4, "EDIT:", 5) || !strncmp(step + 4, "ADD:", 4) || !strcmp(step + 4, "CDEL") || !strcmp(step + 4, "UNDO") || !strcmp(step + 4, "PNEW") || !strncmp(step + 4, "TRIG:", 5);
            long t0 = now_ms(); char m[400] = "";
            while (now_ms() - t0 < (edit ? 25000 : 3000)) {
                usleep(150000); ui_get("msg", m, sizeof m);
                if (strcmp(m, before) && !ends_dots(m)) break;       /* the message changed and is final */
                if (!edit && now_ms() - t0 > 900) break;             /* a plain request that leaves the message as it was (e.g. a selection) */
            }
            usleep(300000);
        } else if (!strncmp(step, "EXPECT:", 7) || !strncmp(step, "EXPECTNOT:", 10)) {
            int neg = step[6] == 'N'; char *k = step + (neg ? 10 : 7), *tl = strchr(k, '~'); if (!tl) { fprintf(stderr, "bp_win: bad step %s\n", step); return 2; }
            *tl = 0; char v[4096]; ui_get(k, v, sizeof v); int has = strstr(v, tl + 1) != NULL;
            if (has == neg) { printf("bp_win: FAIL step %d: %s %s '%s' in %s=%.200s\n", i - 2, neg ? "EXPECTNOT" : "EXPECT", neg ? "found" : "missing", tl + 1, k, v); bad = 1; }
            else printf("bp_win: ok step %d: %s%s %s '%s'\n", i - 2, neg ? "not " : "", k, neg ? "has no" : "has", tl + 1);
        } else { fprintf(stderr, "bp_win: unknown step %s\n", step); return 2; }
    }
    return bad ? 1 : 0;
}
