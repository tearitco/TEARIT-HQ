/* mr_run_menu - "Run Menu" event command (2026-11-02, book-stack reader phase R2b).
 * Shows a data-driven menu in the shared picker and runs the branch script of the row that was picked. The menu is DATA (a .pdl file), so a book's menu
 * order, labels and actions are edited as rows, never in a script.
 *
 * Usage: mr_run_menu.+x <entity_dir> <house_root> <menu_file>
 *   menu_file  path RELATIVE TO THE HOUSE ROOT (no leading '/', no ".."), e.g. _.monads/_.book-stack/pieces/reader/event_pkg/pages/page_1/data/sources.pdl
 *   rows       SOURCE | <order> | <action> | <label>      (order is a number; rows are shown in ascending order; '#' lines and other rows are ignored)
 *   branches   <dir of menu_file>/../branches/<action>/run.sh   (the action must be a plain identifier: letters, digits, _ ; a row with no script is skipped with a note)
 *
 * Popup folder: $PACKAGE_DIR (the Read method exports it), else $MUCHI_TARGET_ENT, else <entity_dir> - the same precedence dispatch.sh had.
 * The picker is the house's khtpm_show_choices.+x (stdout = the picked action; nothing on cancel / timeout, then this op does nothing and exits 0).
 * Only an action that is listed in the menu AND has a script is run; anything else the picker returns (CLOSE, junk) is ignored. No shell is used to parse anything:
 * plain fork/exec. The branch script is run with `bash <script>` and the environment unchanged (PACKAGE_DIR is set if it was not), and its exit status is returned.
 *
 * Exit: 0 done (including cancel), 1 setup problem (message on stderr), 2 usage, else the branch script's status.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/wait.h>

#define MAXROWS 64
#define PB 4352
typedef struct { long order; char action[64]; char label[200]; } Row;

static int ident(const char *s) {
    if (!*s) return 0;
    for (; *s; s++) if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') || *s == '_')) return 0;
    return 1;
}
static void trim(char *s) {
    size_t n = strlen(s); while (n && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' ')) s[--n] = 0;
    char *p = s; while (*p == ' ') p++; if (p != s) memmove(s, p, strlen(p) + 1);
}
/* split "SOURCE | order | action | label" on " | " into order, action and label; the label keeps any later " | " */
static int parse_row(char *line, Row *r) {
    if (strncmp(line, "SOURCE | ", 9) != 0) return 0;
    char *f[2]; char *c = line + 9; int k = 0;          /* two fields (order, action) precede the label, which is the rest of the line */
    while (k < 2) { f[k++] = c; char *bar = strstr(c, " | "); if (!bar) return 0; *bar = 0; c = bar + 3; }
    trim(f[0]); trim(f[1]); trim(c);
    char *end; long o = strtol(f[0], &end, 10); if (end == f[0] || *end) return 0;
    if (!ident(f[1]) || strlen(f[1]) >= sizeof r->action) return 0;
    if (!*c || strlen(c) >= sizeof r->label || strchr(c, '\n')) return 0;
    r->order = o; snprintf(r->action, sizeof r->action, "%s", f[1]); snprintf(r->label, sizeof r->label, "%s", c);
    return 1;
}
/* snprintf that reports truncation: 1 = fit, 0 = would have been cut (a cut path would name a different file, so every path build is checked) */
static int fit(char *out, size_t n, const char *fmt, const char *a, const char *b) { int w = snprintf(out, n, fmt, a, b); return w >= 0 && (size_t)w < n; }
static int cmp_row(const void *a, const void *b) { long x = ((const Row *)a)->order, y = ((const Row *)b)->order; return x < y ? -1 : x > y; }

int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "Usage: mr_run_menu.+x <entity_dir> <house_root> <menu_file relative to the house>\n"); return 2; }
    const char *ent = argv[1], *house = argv[2], *rel = argv[3];
    if (!*rel || rel[0] == '/' || strstr(rel, "..")) { fprintf(stderr, "mr_run_menu: the menu file must be relative to the house root, without '..': %s\n", rel); return 1; }

    char menu[PB]; if (!fit(menu, sizeof menu, "%s/%s", house, rel)) { fprintf(stderr, "mr_run_menu: menu path too long\n"); return 1; }
    FILE *f = fopen(menu, "r");
    if (!f) { fprintf(stderr, "mr_run_menu: cannot read %s\n", menu); return 1; }
    static Row rows[MAXROWS]; int n = 0; char line[1024];
    char base[PB]; snprintf(base, sizeof base, "%s", menu);
    char *sl = strrchr(base, '/'); if (sl) *sl = 0;            /* dir of the menu file */
    char *sl2 = strrchr(base, '/'); if (sl2) *sl2 = 0;         /* its parent: branches/ lives here */
    while (n < MAXROWS && fgets(line, sizeof line, f)) {
        if (line[0] == '#') continue;
        Row r; if (!parse_row(line, &r)) continue;
        char sp[PB], bp[PB]; if (!fit(bp, sizeof bp, "%s/branches/%s", base, r.action) || !fit(sp, sizeof sp, "%s/run.sh", bp, "")) { fprintf(stderr, "mr_run_menu: skipping '%s': path too long\n", r.action); continue; }
        if (access(sp, R_OK) != 0) { fprintf(stderr, "mr_run_menu: skipping '%s': no %s\n", r.action, sp); continue; }
        rows[n++] = r;
    }
    fclose(f);
    if (n == 0) { fprintf(stderr, "mr_run_menu: no usable SOURCE rows in %s\n", menu); return 1; }
    qsort(rows, (size_t)n, sizeof rows[0], cmp_row);

    const char *pkg = getenv("PACKAGE_DIR"); if (!pkg || !*pkg) pkg = getenv("MUCHI_TARGET_ENT"); if (!pkg || !*pkg) pkg = ent;
    char choices[PB]; snprintf(choices, sizeof choices, "%s/.run_menu_choices.tmp.pdl", pkg);
    FILE *cf = fopen(choices, "w");
    if (!cf) { fprintf(stderr, "mr_run_menu: cannot write %s\n", choices); return 1; }
    for (int i = 0; i < n; i++) fprintf(cf, "OBJECT | label=%s | action=%s\n", rows[i].label, rows[i].action);
    fclose(cf);

    char picker[PB]; snprintf(picker, sizeof picker, "%s/&.widgits/tile-picker/ops/+x/khtpm_show_choices.+x", house);
    if (access(picker, X_OK) != 0) { fprintf(stderr, "mr_run_menu: picker missing: %s\n", picker); remove(choices); return 1; }
    int pfd[2]; if (pipe(pfd) != 0) { remove(choices); return 1; }
    pid_t pid = fork();
    if (pid < 0) { remove(choices); return 1; }
    if (pid == 0) { dup2(pfd[1], 1); close(pfd[0]); close(pfd[1]); execl(picker, picker, pkg, choices, (char *)NULL); _exit(127); }
    close(pfd[1]);
    char picked[256] = ""; size_t used = 0; ssize_t got;
    while (used + 1 < sizeof picked && (got = read(pfd[0], picked + used, sizeof picked - 1 - used)) > 0) used += (size_t)got;
    picked[used] = 0; close(pfd[0]);
    int st = 0; waitpid(pid, &st, 0);
    remove(choices);
    trim(picked);
    char *nl = strchr(picked, '\n'); if (nl) *nl = 0;
    if (!*picked) return 0;                                    /* cancelled or timed out: nothing to run */

    int at = -1; for (int i = 0; i < n; i++) if (!strcmp(rows[i].action, picked)) at = i;
    if (at < 0) return 0;                                      /* CLOSE or anything not in the menu */

    char script[PB], bdir[PB];
    if (!fit(bdir, sizeof bdir, "%s/branches/%s", base, rows[at].action) || !fit(script, sizeof script, "%s/run.sh", bdir, "")) { fprintf(stderr, "mr_run_menu: script path too long\n"); return 1; }
    if (!getenv("PACKAGE_DIR")) setenv("PACKAGE_DIR", pkg, 1);
    pid = fork();
    if (pid < 0) return 1;
    if (pid == 0) { execlp("bash", "bash", script, (char *)NULL); _exit(127); }
    waitpid(pid, &st, 0);
    return WIFEXITED(st) ? WEXITSTATUS(st) : 1;
}
