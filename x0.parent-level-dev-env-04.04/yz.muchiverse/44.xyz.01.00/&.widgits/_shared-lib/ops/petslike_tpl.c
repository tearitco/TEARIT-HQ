/* petslike_tpl.c - fills the shared window templates of the pets-like games (PETSLIKE-FRAMEWORK-PLAN.md P2).
 *   petslike_tpl gen   <app_dir>   write <app>/board/pchq-board.xhtpm and <app>/<app>.xhtpm from &.widgits/_shared-lib/petslike/{board,standalone}.xhtpm.in
 *   petslike_tpl check <app_dir>   exit 1 (and name the file) when a committed window file differs from what gen would write
 * Tokens in a template: @APP@ = the app folder name, @OP@ = the op name (@APP@ with - as _), @CAST@ = toy.pdl "cast_label" (default "cast").
 * Markers: a line  <!--@@sN-->  or a block  <!--@@sN:--> ... <!--@@/sN-->  (the lines in between are the default) is replaced by the game's block sN of
 * <app>/xhtpm-extra.txt, which holds  "@@<kind> sN" ... "@@end"  sections (kind = board | standalone); no block = the default. The window files stay
 * checked in (the renderer reads them as they are); edit the template or the extra file, then gen. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <libgen.h>

#define MAXB 32
static char *blk[2][MAXB]; /* [kind][n] */
static char APP[96], OP[96], CAST[32] = "cast";

static char *slurp(const char *p) { FILE *f = fopen(p, "rb"); if (!f) return NULL; fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET); char *b = malloc((size_t)n + 1); if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(b); return NULL; } b[n] = 0; fclose(f); return b; }
static void load_cast(const char *appdir) {
    char p[PATH_MAX]; snprintf(p, sizeof p, "%s/toy.pdl", appdir); char *t = slurp(p); if (!t) return;
    for (char *l = strtok(t, "\n"); l; l = strtok(NULL, "\n")) { char *a = strstr(l, "| cast_label"); if (!a || strncmp(l, "SECTION", 7)) continue; char *b = strchr(a + 1, '|'); if (!b) continue; b++; while (*b == ' ') b++; char *e = b + strlen(b); while (e > b && (e[-1] == ' ' || e[-1] == '\r')) *--e = 0; snprintf(CAST, sizeof CAST, "%s", b); }
    free(t);
}
static void load_extra(const char *appdir) {
    char p[PATH_MAX]; snprintf(p, sizeof p, "%s/xhtpm-extra.txt", appdir); char *t = slurp(p); if (!t) return;
    char *cur = NULL; int kind = -1, n = -1; size_t cap = 0, len = 0; char *s = t;
    while (*s) { char *nl = strchr(s, '\n'); size_t ll = nl ? (size_t)(nl - s) : strlen(s); char line[1024]; snprintf(line, sizeof line, "%.*s", (int)(ll < 1023 ? ll : 1023), s);
        if (!strncmp(line, "@@end", 5)) { if (kind >= 0 && n >= 0 && n < MAXB) blk[kind][n] = cur ? cur : strdup(""); cur = NULL; kind = -1; len = cap = 0; }
        else if (!strncmp(line, "@@standalone s", 14) || !strncmp(line, "@@board s", 9)) { kind = line[2] == 's' ? 1 : 0; n = atoi(strchr(line, 's') + (kind ? strlen("tandalone s") + 1 : 1)); cur = NULL; len = cap = 0; }
        else if (kind >= 0) { size_t add = ll + 1; if (len + add + 1 > cap) { cap = (len + add + 1) * 2; cur = realloc(cur, cap); } memcpy(cur + len, s, ll); len += ll; cur[len++] = '\n'; cur[len] = 0; }
        s += ll + (nl ? 1 : 0); }
    free(t);
}
static void put(char **out, size_t *len, size_t *cap, const char *s, size_t n) { if (*len + n + 1 > *cap) { *cap = (*len + n + 1) * 2; *out = realloc(*out, *cap); } memcpy(*out + *len, s, n); *len += n; (*out)[*len] = 0; }
static char *expand(const char *tpl, int kind) {
    char *out = NULL; size_t len = 0, cap = 0; const char *s = tpl; int skip = -1, have = 0;
    while (*s) { const char *nl = strchr(s, '\n'); size_t ll = nl ? (size_t)(nl - s) : strlen(s); size_t full = ll + (nl ? 1 : 0); int n;
        if (!strncmp(s, "<!--@@s", 7) && sscanf(s + 7, "%d", &n) == 1 && n >= 0 && n < MAXB && !strncmp(s + 7 + (n >= 10 ? 2 : 1), "-->", 3)) { if (blk[kind][n]) put(&out, &len, &cap, blk[kind][n], strlen(blk[kind][n])); }
        else if (!strncmp(s, "<!--@@s", 7) && sscanf(s + 7, "%d", &n) == 1 && n >= 0 && n < MAXB && !strncmp(s + 7 + (n >= 10 ? 2 : 1), ":-->", 4)) { skip = n; have = blk[kind][n] != NULL; if (have) put(&out, &len, &cap, blk[kind][n], strlen(blk[kind][n])); }
        else if (!strncmp(s, "<!--@@/s", 8) && skip >= 0) { skip = -1; have = 0; }
        else if (skip < 0 || !have) put(&out, &len, &cap, s, full);
        s += full; }
    if (!out) out = strdup("");
    /* tokens */
    char *res = NULL; size_t rl = 0, rc = 0; for (const char *q = out; *q;) { const char *t = NULL; const char *v = NULL; size_t tl = 0;
        if (!strncmp(q, "@APP@", 5)) { t = q; tl = 5; v = APP; } else if (!strncmp(q, "@OP@", 4)) { t = q; tl = 4; v = OP; } else if (!strncmp(q, "@CAST@", 6)) { t = q; tl = 6; v = CAST; }
        if (t) { put(&res, &rl, &rc, v, strlen(v)); q += tl; } else { put(&res, &rl, &rc, q, 1); q++; } }
    free(out); return res ? res : strdup("");
}
int main(int argc, char **argv) {
    if (argc < 3 || (strcmp(argv[1], "gen") && strcmp(argv[1], "check"))) { fprintf(stderr, "usage: petslike_tpl gen|check <app_dir>\n"); return 2; }
    char app[PATH_MAX]; if (!realpath(argv[2], app)) { perror("app_dir"); return 2; } char cp[PATH_MAX]; snprintf(cp, sizeof cp, "%s", app); snprintf(APP, sizeof APP, "%s", basename(cp));
    snprintf(OP, sizeof OP, "%s", APP); for (char *c = OP; *c; c++) if (*c == '-') *c = '_';
    load_cast(app); load_extra(app);
    char tdir[PATH_MAX]; snprintf(tdir, sizeof tdir, "%s/../../&.widgits/_shared-lib/petslike", app);
    const char *kinds[2] = { "board", "standalone" }; int bad = 0;
    for (int k = 0; k < 2; k++) {
        char tp[PATH_MAX], dp[PATH_MAX]; snprintf(tp, sizeof tp, "%s/%s.xhtpm.in", tdir, kinds[k]);
        if (k == 0) snprintf(dp, sizeof dp, "%s/board/pchq-board.xhtpm", app); else snprintf(dp, sizeof dp, "%s/%s.xhtpm", app, APP);
        char *tpl = slurp(tp); if (!tpl) { fprintf(stderr, "petslike_tpl: no template %s\n", tp); return 2; }
        char *want = expand(tpl, k), *have = slurp(dp);
        if (!have || strcmp(want, have)) { if (!strcmp(argv[1], "check")) { printf("DIFFERS: %s\n", dp); bad = 1; } else { FILE *f = fopen(dp, "wb"); if (!f) { perror(dp); return 1; } fwrite(want, 1, strlen(want), f); fclose(f); printf("wrote %s\n", dp); } }
        else if (!strcmp(argv[1], "check")) printf("ok: %s\n", dp);
        free(tpl); free(want); free(have);
    }
    return bad;
}
