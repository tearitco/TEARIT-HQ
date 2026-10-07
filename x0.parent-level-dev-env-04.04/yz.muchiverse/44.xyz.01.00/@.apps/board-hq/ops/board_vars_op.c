/* board_vars_op - turns a small data-source file (board.pdl) into the vars file the ONE generic board layout reads (Q006, HAI-ROBOTS-PHONES-SERVER-DESIGN.md 3d).
 * The grave, the ghost roster, the server and the phone windows are all "list of rows + detail pane + actions"; this op is the only per-window piece, and it is data, not C.
 *
 * Usage: board_vars_op.+x <state_dir> [--select N] [--watch]
 *   <state_dir>/board.pdl     the data source (below)        <state_dir>/selected.txt  the selected row index (written by --select)
 *   <state_dir>/ui.txt        OUTPUT: key=value vars for board.xhtpm (written only when its content changed)
 *   --select N   save N as the selected row, re-publish once, exit (this is what a row click runs)
 *   --watch | watch   publish, then re-publish every second when the output would change (the window's <module> runs this)
 * board.pdl lines (fields separated by " | ", '#' comments):
 *   BOARD  | title    | <window title>            BOARD | subtitle | <text>
 *   SOURCE | md-table | <path from house root, or absolute> | cols=0,1,4        rows = the data rows of the first markdown table; a cell like [Q001](dir/QUEST.md) gives
 *                                                                  id "Q001" and a link; row text = the listed cells joined with two spaces (default cols=0,1)
 *   SOURCE | pipe-index | <path> | cols=0,2                  rows = lines of a pipe-delimited index (phones.index: number|uid|label|entity_dir|phone_dir); id = field 0, link = the last field (a folder)
 *   SOURCE | dirs     | <path from house root>                    rows = sub-folders (names not starting with '_' or '.'), sorted
 *   DETAIL | link-tail | <n>                    detail pane = last n lines of the selected row's linked file (md-table)
 *   DETAIL | dir-file-tail | <file> | <n>       detail pane = last n lines of <selected folder>/<file> (dirs)
 *   ACTION | <label> | <shell command>           a button; the command is run as given (it may use the placeholders {id} and {dir} = the selected row's id / folder)
 * Exit: 0 ok, 2 usage / unreadable board.pdl. No network, writes only inside <state_dir>. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>

#define MAXR 400
#define PB 4352
typedef struct { char id[256], text[300], link[PB]; } Row;
static Row R[MAXR]; static int NR;
static char HOUSE[PB], STATE[PB], OPPATH[PB];
static char title[200] = "Board", subtitle[200] = "";
static char src_kind[16], src_path[PB], src_cols[64] = "0,1";
static char det_kind[24], det_file[200]; static int det_n = 12;
static char act_label[16][120], act_cmd[16][1500]; static int NA;

static void trim(char *s) { char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0; char *b = s; while (*b == ' ' || *b == '\t') b++; if (b != s) memmove(s, b, strlen(b) + 1); }
static void clean(char *s) { for (; *s; s++) if (*s == '\n' || *s == '\r' || *s == '|' || *s == '=' ) *s = ' '; }
static int join(char *o, size_t n, const char *a, const char *b) { size_t la = strlen(a), lb = strlen(b); if (la + 1 + lb + 1 > n) { o[0] = 0; return 0; } memcpy(o, a, la); o[la] = '/'; memcpy(o + la + 1, b, lb + 1); return 1; }
static int cmp(const void *a, const void *b) { return strcmp(((const Row *)a)->id, ((const Row *)b)->id); }

static void load_pdl(void) {
    char p[PB], line[2200]; FILE *f; join(p, sizeof(p), STATE, "board.pdl");
    if (!(f = fopen(p, "r"))) { fprintf(stderr, "board_vars_op: cannot read %s\n", p); exit(2); }
    while (fgets(line, sizeof(line), f)) {
        char *fld[5]; int n = 0; char *t = line; line[strcspn(line, "\r\n")] = 0;
        if (line[0] == '#' || !line[0]) continue;
        while (n < 5) { fld[n++] = t; char *b = strstr(t, " | "); if (!b) break; *b = 0; t = b + 3; }
        for (int i = 0; i < n; i++) trim(fld[i]);
        if (!strcmp(fld[0], "BOARD") && n >= 3) { if (!strcmp(fld[1], "title")) snprintf(title, sizeof(title), "%s", fld[2]); else if (!strcmp(fld[1], "subtitle")) snprintf(subtitle, sizeof(subtitle), "%s", fld[2]); }
        else if (!strcmp(fld[0], "SOURCE") && n >= 3) { snprintf(src_kind, sizeof(src_kind), "%s", fld[1]); snprintf(src_path, sizeof(src_path), "%s", fld[2]); if (n >= 4 && !strncmp(fld[3], "cols=", 5)) snprintf(src_cols, sizeof(src_cols), "%s", fld[3] + 5); }
        else if (!strcmp(fld[0], "DETAIL") && n >= 3) { snprintf(det_kind, sizeof(det_kind), "%s", fld[1]); if (!strcmp(fld[1], "link-tail")) det_n = atoi(fld[2]); else if (n >= 4) { snprintf(det_file, sizeof(det_file), "%s", fld[2]); det_n = atoi(fld[3]); } }
        else if (!strcmp(fld[0], "ACTION") && n >= 3 && NA < 16) { snprintf(act_label[NA], sizeof(act_label[0]), "%s", fld[1]); snprintf(act_cmd[NA], sizeof(act_cmd[0]), "%s", fld[2]); NA++; }
    }
    fclose(f);
}

static void load_rows(void) {
    char p[PB], line[2200], base[PB]; FILE *f;
    if (!src_kind[0]) return;
    if (src_path[0] == '/') snprintf(p, sizeof(p), "%.4000s", src_path); else join(p, sizeof(p), HOUSE, src_path);   /* absolute path = as given (tests point at scratch data); else relative to the house root */
    if (!strcmp(src_kind, "md-table")) {
        int seen_header = 0; char *dirend;
        snprintf(base, sizeof(base), "%s", p); dirend = strrchr(base, '/'); if (dirend) *dirend = 0;
        if (!(f = fopen(p, "r"))) return;
        while (NR < MAXR && fgets(line, sizeof(line), f)) {
            char *cell[12]; int n = 0; char *t;
            if (line[0] != '|') { if (seen_header) break; else continue; }
            line[strcspn(line, "\r\n")] = 0;
            if (!seen_header) { seen_header = 1; continue; }                 /* header row */
            if (strstr(line, "|---") || strstr(line, "| ---")) continue;     /* separator */
            t = line + 1; while (n < 12) { cell[n++] = t; char *b = strstr(t, " | "); if (!b) { char *e = strrchr(t, '|'); if (e) *e = 0; break; } *b = 0; t = b + 3; }
            for (int i = 0; i < n; i++) trim(cell[i]);
            Row *r = &R[NR]; r->link[0] = 0;
            { char *lb = strchr(cell[0], '['), *rb = lb ? strchr(lb, ']') : NULL, *lp = rb ? strstr(rb, "](") : NULL;
              if (lb && rb && lp) { char tmp[64]; size_t l = (size_t)(rb - lb - 1); if (l >= sizeof(tmp)) l = sizeof(tmp) - 1; memcpy(tmp, lb + 1, l); tmp[l] = 0; snprintf(r->id, sizeof(r->id), "%s", tmp);
                  char *e = strchr(lp + 2, ')'); if (e) { *e = 0; char rel[PB]; snprintf(rel, sizeof(rel), "%s", lp + 2); join(r->link, sizeof(r->link), base, rel); } }
              else snprintf(r->id, sizeof(r->id), "%s", cell[0]); }
            r->text[0] = 0;
            { char cols[64], *c, *sv = NULL; snprintf(cols, sizeof(cols), "%s", src_cols);
              for (c = strtok_r(cols, ",", &sv); c; c = strtok_r(NULL, ",", &sv)) { int k = atoi(c); const char *v = (k == 0) ? r->id : (k < n ? cell[k] : ""); size_t l = strlen(r->text);
                  snprintf(r->text + l, sizeof(r->text) - l, "%s%.70s", l ? "  " : "", v); } }
            clean(r->text); clean(r->id); NR++;
        }
        fclose(f);
    } else if (!strcmp(src_kind, "pipe-index")) {
        /* a pipe-delimited index such as ^.hai-server/phones.index: number|uid|label|entity_dir|phone_dir. id = field 0, link = LAST field (a folder, so DETAIL dir-file-tail works),
         * text = the fields listed in cols= (default 0,2 = number + label) */
        if (!(f = fopen(p, "r"))) return;
        while (NR < MAXR && fgets(line, sizeof(line), f)) {
            char *fld[8]; int n = 0; char *t = line;
            line[strcspn(line, "\r\n")] = 0; if (line[0] == '#' || !line[0]) continue;
            while (n < 8) { fld[n++] = t; char *b = strchr(t, '|'); if (!b) break; *b = 0; t = b + 1; }
            if (n < 2) continue;
            Row *r = &R[NR]; snprintf(r->id, sizeof(r->id), "%.255s", fld[0]); snprintf(r->link, sizeof(r->link), "%.4000s", fld[n - 1]); r->text[0] = 0;
            { char cols[64], *c, *sv = NULL; snprintf(cols, sizeof(cols), "%s", strcmp(src_cols, "0,1") ? src_cols : "0,2");
              for (c = strtok_r(cols, ",", &sv); c; c = strtok_r(NULL, ",", &sv)) { int k = atoi(c); const char *v = k < n ? fld[k] : ""; size_t l = strlen(r->text);
                  snprintf(r->text + l, sizeof(r->text) - l, "%s%.70s", l ? "  " : "", v); } }
            clean(r->text); clean(r->id); NR++;
        }
        fclose(f);
    } else if (!strcmp(src_kind, "dirs")) {
        DIR *d = opendir(p); struct dirent *e; if (!d) return;
        while (NR < MAXR && (e = readdir(d))) {
            char dp[PB]; struct stat st; if (e->d_name[0] == '.' || e->d_name[0] == '_') continue;
            join(dp, sizeof(dp), p, e->d_name); if (stat(dp, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
            snprintf(R[NR].id, sizeof(R[NR].id), "%s", e->d_name); snprintf(R[NR].text, sizeof(R[NR].text), "%s", e->d_name); snprintf(R[NR].link, sizeof(R[NR].link), "%s", dp); NR++;
        }
        closedir(d); qsort(R, (size_t)NR, sizeof(Row), cmp);
    }
}

static void tail_lines(const char *path, int n, char out[][400], int *cnt) {
    char line[2000]; FILE *f = fopen(path, "r"); static char ring[64][400]; int total = 0, start, i;
    *cnt = 0; if (!f || n <= 0) { if (f) fclose(f); return; } if (n > 64) n = 64;
    while (fgets(line, sizeof(line), f)) { line[strcspn(line, "\r\n")] = 0; if (!line[0]) continue; snprintf(ring[total % 64], 400, "%.390s", line); total++; }
    fclose(f);
    start = total > n ? total - n : 0;
    for (i = start; i < total; i++) { snprintf(out[*cnt], 400, "%s", ring[i % 64]); clean(out[*cnt]); (*cnt)++; }
}

static int publish(int selected) {
    static char buf[200000], old[200000]; int off = 0, i; char p[PB]; FILE *f; static char dl[64][400]; int nd = 0;
    char selid[256] = "", seldir[PB] = "";
    if (selected < 0 || selected >= NR) selected = NR ? 0 : -1;
    if (selected >= 0) { snprintf(selid, sizeof(selid), "%.255s", R[selected].id); snprintf(seldir, sizeof(seldir), "%.4000s", R[selected].link); }
    if (selected >= 0 && !strcmp(det_kind, "link-tail") && R[selected].link[0]) tail_lines(R[selected].link, det_n, dl, &nd);
    else if (selected >= 0 && !strcmp(det_kind, "dir-file-tail")) { char fp[PB]; if (join(fp, sizeof(fp), seldir, det_file)) tail_lines(fp, det_n, dl, &nd); }
    off += snprintf(buf + off, sizeof(buf) - off, "title=%s\nsubtitle=%s\nsel_name=%s\nn_rows=%d\n", title, subtitle, selid[0] ? selid : "-", NR);
    for (i = 0; i < NR && off < (int)sizeof(buf) - 2000; i++)
        off += snprintf(buf + off, sizeof(buf) - off, "r_%d_text=%s\nr_%d_cls=%s\nr_%d_action='%s' '%s' --select %d\n", i, R[i].text, i, i == selected ? "board-row board-sel" : "board-row", i, OPPATH, STATE, i);
    off += snprintf(buf + off, sizeof(buf) - off, "n_detail=%d\n", nd);
    for (i = 0; i < nd; i++) off += snprintf(buf + off, sizeof(buf) - off, "d_%d_text=%s\n", i, dl[i]);
    off += snprintf(buf + off, sizeof(buf) - off, "n_actions=%d\n", NA);
    for (i = 0; i < NA; i++) {
        char cmd[1500], out[8000]; size_t ci = 0; snprintf(cmd, sizeof(cmd), "%.1499s", act_cmd[i]);
        out[0] = 0;
        for (const char *c = cmd; *c && ci < sizeof(out) - 1200; ) {
            if (!strncmp(c, "{id}", 4)) { ci += (size_t)snprintf(out + ci, sizeof(out) - ci, "%s", selid); c += 4; }
            else if (!strncmp(c, "{dir}", 5)) { ci += (size_t)snprintf(out + ci, sizeof(out) - ci, "%s", seldir); c += 5; }
            else out[ci++] = *c++;
            out[ci] = 0;
        }
        off += snprintf(buf + off, sizeof(buf) - off, "a_%d_label=%s\na_%d_action=%s\n", i, act_label[i], i, out);
    }
    old[0] = 0; join(p, sizeof(p), STATE, "ui.txt");
    if ((f = fopen(p, "r"))) { size_t n = fread(old, 1, sizeof(old) - 1, f); old[n] = 0; fclose(f); }
    if (!strcmp(old, buf)) return 0;                                      /* unchanged: do not rewrite (no spurious reparse) */
    { char tmp[PB + 8]; snprintf(tmp, sizeof(tmp), "%s.tmp", p); if (!(f = fopen(tmp, "w"))) return 2; fputs(buf, f); fclose(f); rename(tmp, p); }
    return 1;
}

int main(int argc, char **argv) {
    int sel = 0, watch = 0, do_select = 0; char *rp, here[PB], sp[PB]; FILE *f;
    if (argc < 2) { fprintf(stderr, "usage: board_vars_op.+x <state_dir> [--select N] [--watch]\n"); return 2; }
    snprintf(STATE, sizeof(STATE), "%s", argv[1]);
    for (int i = 2; i < argc; i++) { if (!strcmp(argv[i], "--select") && i + 1 < argc) { sel = atoi(argv[++i]); do_select = 1; } else if (!strcmp(argv[i], "--watch") || !strcmp(argv[i], "watch") || ((strrchr(argv[i], '/') ? strrchr(argv[i], '/') + 1 : argv[i])[0] == 'w' && !strcmp(strrchr(argv[i], '/') ? strrchr(argv[i], '/') + 1 : argv[i], "watch"))) watch = 1;   /* a window <module> launcher prefixes EVERY argument with the house path ("<house>/watch"), so match the last path component */
        else { struct stat sb; if (watch || (stat(argv[i], &sb) == 0 && S_ISDIR(sb.st_mode))) continue;   /* module contract: the renderer appends <house_root> <package_dir> <module id> after our own args; in module mode (watch seen) ignore them */
               fprintf(stderr, "unknown argument %s\n", argv[i]); return 2; } }
    rp = realpath(argv[0], NULL); snprintf(OPPATH, sizeof(OPPATH), "%s", rp ? rp : argv[0]); free(rp);
    snprintf(here, sizeof(here), "%s", OPPATH);                           /* house root = nearest ancestor of this binary that holds &.widgits/_shared-lib */
    for (;;) { char *sl = strrchr(here, '/'); char t[PB]; if (!sl || sl == here) { fprintf(stderr, "board_vars_op: house root not found\n"); return 2; } *sl = 0; join(t, sizeof(t), here, "&.widgits/_shared-lib"); if (access(t, F_OK) == 0) break; }
    snprintf(HOUSE, sizeof(HOUSE), "%s", getenv("BOARD_HOUSE_ROOT") ? getenv("BOARD_HOUSE_ROOT") : here);
    if (STATE[0] != '/') {   /* relative: as given if it exists from here, else house-relative (a window <module> passes "@.apps/board-hq/state/<name>") */
        char t[PB], *abs = realpath(STATE, NULL);
        if (abs) { snprintf(STATE, sizeof(STATE), "%.4000s", abs); free(abs); }
        else { snprintf(t, sizeof(t), "%.4000s", STATE); join(STATE, sizeof(STATE), HOUSE, t); }
    }
    load_pdl();
    join(sp, sizeof(sp), STATE, "selected.txt");
    if (do_select) { if ((f = fopen(sp, "w"))) { fprintf(f, "%d\n", sel); fclose(f); } }
    else if ((f = fopen(sp, "r"))) { if (fscanf(f, "%d", &sel) != 1) sel = 0; fclose(f); }
    load_rows(); publish(sel);
    if (do_select || !watch) return 0;
    {   /* orphan hygiene: a watch loop must not outlive its window - exit when the parent (the renderer) is gone or the data file disappears */
        pid_t pp = getppid(); char pdl[PB]; join(pdl, sizeof(pdl), STATE, "board.pdl");
        for (;;) { sleep(1); if (getppid() != pp || access(pdl, R_OK) != 0) return 0; NR = 0; if ((f = fopen(sp, "r"))) { if (fscanf(f, "%d", &sel) != 1) sel = 0; fclose(f); } load_rows(); publish(sel); }
    }
}
