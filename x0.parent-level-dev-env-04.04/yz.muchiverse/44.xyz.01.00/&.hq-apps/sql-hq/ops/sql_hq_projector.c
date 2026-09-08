/* sql_hq_projector - state publisher for sql-hq.xhtpm.
 *
 * argv: [1]=house_root  [2]=package_dir (&.hq-apps/sql-hq)  [3]=id
 * env fallbacks: KHTPM_HOUSE / KHTPM_PKG
 *
 * Polls, and on any change atomically rewrites <pkg>/state/ui.txt:
 *   A=<house>/&.hq-apps/sql-hq/ops/sql_hq_action.sh
 *   active_table=<name>
 *   status=<one line>
 *   n_tables=N  no_tables=1|""
 *     tb_<i>_name=  tb_<i>_label=  tb_<i>_active=sql-active|""
 *   n_macros=M
 *     m_<i>_name=  m_<i>_label=
 *   n_grid=R
 *     g_<i>_text=  g_<i>_class=grid-head|grid-sep|grid-row|grid-err
 *
 * Inputs it reads from <pkg>/state/ :
 *   manifest.pdl   SECTION | table:<name> | <path>|<kind>
 *   active_table.txt   one line
 *   status.txt         one line
 *   result.grid.txt    the aligned grid from `sql_hq run` (or an error)
 * and <pkg>/sql_hq_macros.pdl  (SECTION | macro:<name> | <label> | <snippet>)
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <stdarg.h>

#define PB 2048
#define MAXROW 4000

static char g_house[PB], g_pkg[PB];

static void rstrip(char *s){ size_t n=strlen(s); while(n&&(s[n-1]=='\n'||s[n-1]=='\r'||s[n-1]==' '||s[n-1]=='\t')) s[--n]=0; }

/* append escaped-for-one-line into a growing buffer */
static void ap(char **buf, size_t *len, size_t *cap, const char *fmt, ...) {
    va_list a; va_start(a, fmt);
    for (;;) {
        int n = vsnprintf(*buf + *len, *cap - *len, fmt, a);
        va_end(a);
        if (n >= 0 && (size_t)n < *cap - *len) { *len += (size_t)n; return; }
        *cap = (*cap ? *cap * 2 : 8192);
        *buf = realloc(*buf, *cap);
        va_start(a, fmt);
    }
}

/* strip newlines/CR from a value so it's safe as one ui.txt line */
static void oneline(const char *in, char *out, size_t n) {
    size_t o = 0;
    for (const char *p = in; *p && o + 1 < n; p++)
        out[o++] = (*p == '\n' || *p == '\r') ? ' ' : *p;
    out[o] = 0;
}

static long fhash(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    long h = 1469598103934665603L; int c;
    while ((c = fgetc(f)) != EOF) { h ^= (unsigned char)c; h *= 1099511628211L; }
    fclose(f);
    return h ? h : 1;
}

static int read1(const char *path, char *out, size_t n) {
    out[0] = 0;
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    if (fgets(out, (int)n, f)) rstrip(out);
    fclose(f);
    return 1;
}

int main(int argc, char **argv) {
    const char *h = (argc > 1 && argv[1][0]) ? argv[1] : getenv("KHTPM_HOUSE");
    const char *p = (argc > 2 && argv[2][0]) ? argv[2] : getenv("KHTPM_PKG");
    if (!h || !p) { fprintf(stderr, "sql_hq_projector: need house + pkg\n"); return 1; }
    snprintf(g_house, sizeof(g_house), "%s", h);
    snprintf(g_pkg, sizeof(g_pkg), "%s", p);

    char mpath[PB], apath[PB], spath[PB], gpath[PB], macpath[PB], uipath[PB], uitmp[PB];
    snprintf(mpath,   sizeof(mpath),   "%s/state/manifest.pdl",     g_pkg);
    snprintf(apath,   sizeof(apath),   "%s/state/active_table.txt", g_pkg);
    snprintf(spath,   sizeof(spath),   "%s/state/status.txt",       g_pkg);
    snprintf(gpath,   sizeof(gpath),   "%s/state/result.grid.txt",  g_pkg);
    snprintf(macpath, sizeof(macpath), "%s/sql_hq_macros.pdl",      g_pkg);
    snprintf(uipath,  sizeof(uipath),  "%s/state/ui.txt",           g_pkg);
    snprintf(uitmp,   sizeof(uitmp),   "%s/state/.ui.txt.tmp",      g_pkg);

    { char d[PB]; snprintf(d, sizeof(d), "%s/state", g_pkg); mkdir(d, 0755);
      char d2[PB]; snprintf(d2, sizeof(d2), "%s/state/history", g_pkg); mkdir(d2, 0755); }

    long last = -1;
    for (;;) {
        long sig = fhash(mpath) ^ (fhash(apath) * 3) ^ (fhash(spath) * 7)
                 ^ (fhash(gpath) * 11) ^ (fhash(macpath) * 13);
        if (sig != last) {
            last = sig;

            char *buf = NULL; size_t len = 0, cap = 0;
            ap(&buf, &len, &cap, "A=%s/&.hq-apps/sql-hq/ops/sql_hq_action.sh\n", g_house);

            char active[512]; read1(apath, active, sizeof(active));
            ap(&buf, &len, &cap, "active_table=%s\n", active);

            char status[1024] = ""; read1(spath, status, sizeof(status));
            char st1[1024]; oneline(status, st1, sizeof(st1));
            ap(&buf, &len, &cap, "status=%s\n", st1[0] ? st1 : "ready");

            /* tables from manifest.pdl */
            int nt = 0;
            FILE *mf = fopen(mpath, "r");
            if (mf) {
                char line[PB];
                while (fgets(line, sizeof(line), mf)) {
                    if (line[0] == '#' || line[0] == '\n') continue;
                    char *f1 = strchr(line, '|'); if (!f1) continue; f1++;
                    char *f2 = strchr(f1, '|');   if (!f2) continue; *f2 = 0; f2++;
                    char key[512]; sscanf(f1, " %511[^ |\n]", key);
                    if (strncmp(key, "table:", 6) != 0) continue;
                    const char *nm = key + 6;
                    char pathv[PB]; sscanf(f2, " %2047[^|\n]", pathv);
                    /* short label: name + trailing basename hint */
                    ap(&buf, &len, &cap, "tb_%d_name=%s\n", nt, nm);
                    ap(&buf, &len, &cap, "tb_%d_label=%s\n", nt, nm);
                    ap(&buf, &len, &cap, "tb_%d_active=%s\n", nt,
                       strcmp(nm, active) == 0 ? "sql-active" : "");
                    nt++;
                }
                fclose(mf);
            }
            ap(&buf, &len, &cap, "n_tables=%d\n", nt);
            ap(&buf, &len, &cap, "no_tables=%s\n", nt ? "" : "1");

            /* macros from sql_hq_macros.pdl */
            int nm2 = 0;
            FILE *cf = fopen(macpath, "r");
            if (cf) {
                char line[PB];
                while (fgets(line, sizeof(line), cf)) {
                    if (line[0] == '#' || line[0] == '\n') continue;
                    char *f1 = strchr(line, '|'); if (!f1) continue; f1++;
                    char *f2 = strchr(f1, '|');   if (!f2) continue; *f2 = 0; f2++;
                    char key[256]; sscanf(f1, " %255[^ |\n]", key);
                    if (strncmp(key, "macro:", 6) != 0) continue;
                    char label[256]; char *f3 = strchr(f2, '|');
                    if (f3) *f3 = 0;
                    sscanf(f2, " %255[^|\n]", label);
                    ap(&buf, &len, &cap, "m_%d_name=%s\n", nm2, key + 6);
                    ap(&buf, &len, &cap, "m_%d_label=%s\n", nm2, label[0] ? label : key + 6);
                    nm2++;
                }
                fclose(cf);
            }
            ap(&buf, &len, &cap, "n_macros=%d\n", nm2);

            /* result grid: one ui var per line, classed by shape */
            int ng = 0;
            FILE *gf = fopen(gpath, "r");
            if (gf) {
                char line[PB];
                while (ng < MAXROW && fgets(line, sizeof(line), gf)) {
                    rstrip(line);
                    const char *cls = "grid-row";
                    if (ng == 0)                          cls = "grid-head";
                    else if (line[0] == '-' )             cls = "grid-sep";
                    if (strncmp(line, "-- error", 8) == 0) cls = "grid-err";
                    char one[PB]; oneline(line, one, sizeof(one));
                    ap(&buf, &len, &cap, "g_%d_text=%s\n", ng, one);
                    ap(&buf, &len, &cap, "g_%d_class=%s\n", ng, cls);
                    ng++;
                }
                fclose(gf);
            }
            ap(&buf, &len, &cap, "n_grid=%d\n", ng);

            FILE *uf = fopen(uitmp, "w");
            if (uf) { fwrite(buf, 1, len, uf); fclose(uf); rename(uitmp, uipath); }
            free(buf);
        }
        struct timespec ts = { 0, 250 * 1000 * 1000 };
        nanosleep(&ts, NULL);
    }
    return 0;
}
