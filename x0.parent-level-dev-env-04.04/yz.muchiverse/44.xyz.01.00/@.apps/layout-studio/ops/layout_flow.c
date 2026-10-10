/* layout_flow - layout studio layer 4b: a GAME'S FLOW as data. One flow.pdl names the layout, the nav table (views -> buttons), the verb script and the keybinds; this op
 * audits them together and drives the live window by LABEL, so a test (or a weak agent) never guesses a nav number.
 *
 *   layout_flow check  <flow.pdl>                       layout_check on the layout + every button verb exists in the verb script + verbs the stopped-gate would drop (WARN)
 *                                                       + duplicate / missing keybind codes. Exit 1 on any ERROR.
 *   layout_flow navmap <flow.pdl> <view>                the numbered buttons the window shows in that view: `<n> <where> <label> -> <verb> <arg>`
 *   layout_flow press  <flow.pdl> <view> <label words> [--pid N] [--esc]   (--esc first leaves Interact mode: while it is on, digits go to the game, not to the nav)   prints the digits to press; with --pid appends them (+ Enter) to #.desktop/entity_menu_history/<pid>.txt
 *
 * flow.pdl rows (`KIND | key | value`, # comments):  FLOW|id|..  FLOW|layout|<xhtpm>  FLOW|nav|<nav.pdl>  FLOW|verbs|<script with a case per verb>  FLOW|keys|<keybinds.pdl>
 *   FLOW|house|<house root relative to the flow file dir>   VIEW|<id>|<description>
 * Numbering rule (verified against live frames): tabs in file order, then the sidebar's static items in file order, the <repeat bind="nav"> rows expanded from nav.pdl for the view
 * (that is where `{party}` becomes N rows), then cli_io. Dynamic menu/inventory repeats are not counted (they appear only while open).
 * Build: sh ops/build_layout_flow.sh */
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAXB 256
typedef struct { char where[8], label[96], verb[48], arg[96]; int n; } Btn;
static char fdir[PATH_MAX], layout[PATH_MAX], navf[PATH_MAX], verbsf[PATH_MAX], keysf[PATH_MAX], house[PATH_MAX], flowid[64];
static char views[16][48]; static int nviews;
static int nerr, nwarn;

static char *trim(char *s) { while (isspace((unsigned char)*s)) s++; char *e = s + strlen(s); while (e > s && isspace((unsigned char)e[-1])) *--e = 0; return s; }
static char *slurp(const char *p) { FILE *f = fopen(p, "rb"); if (!f) return NULL; fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET); char *b = malloc((size_t)n + 1); if (!b) { fclose(f); return NULL; } size_t r = fread(b, 1, (size_t)n, f); b[r] = 0; fclose(f); return b; }
static void rel(char *out, const char *p) { if (p[0] == '/') snprintf(out, PATH_MAX, "%s", p); else snprintf(out, PATH_MAX, "%s/%s", fdir, p); }

static int load_flow(const char *path) {
    char *b = slurp(path); if (!b) { fprintf(stderr, "layout_flow: cannot read %s\n", path); return -1; }
    snprintf(fdir, sizeof fdir, "%s", path); char *s = strrchr(fdir, '/'); if (s) *s = 0; else strcpy(fdir, ".");
    char *sv1 = NULL, *sv2 = NULL; char *line = strtok_r(b, "\n", &sv1);
    while (line) {
        char *l = trim(line); if (*l && *l != '#' && strncmp(l, "SECTION", 7) && l[0] != '-') {
            char *a = strtok_r(l, "|", &sv2), *k = strtok_r(NULL, "|", &sv2), *v = strtok_r(NULL, "", &sv2); if (a && k && v) { a = trim(a); k = trim(k); v = trim(v); char *h = strstr(v, "  #"); if (h) *h = 0; v = trim(v);
                if (!strcmp(a, "FLOW")) { char t[PATH_MAX]; rel(t, v);
                    if (!strcmp(k, "id")) snprintf(flowid, sizeof flowid, "%s", v); else if (!strcmp(k, "layout")) snprintf(layout, sizeof layout, "%s", t); else if (!strcmp(k, "nav")) snprintf(navf, sizeof navf, "%s", t);
                    else if (!strcmp(k, "verbs")) snprintf(verbsf, sizeof verbsf, "%s", t); else if (!strcmp(k, "keys")) snprintf(keysf, sizeof keysf, "%s", t); else if (!strcmp(k, "house")) snprintf(house, sizeof house, "%s", t); }
                else if (!strcmp(a, "VIEW") && nviews < 16) snprintf(views[nviews++], 48, "%s", k); } }
        line = strtok_r(NULL, "\n", &sv1);
    }
    free(b); return 0;
}
static const char *attr(const char *tag, const char *name, char *out, size_t n) {
    char pat[40]; snprintf(pat, sizeof pat, " %s=\"", name); const char *p = strstr(tag, pat); out[0] = 0; if (!p) return NULL; p += strlen(pat); size_t k = 0; while (p[k] && p[k] != '"' && k + 1 < n) { out[k] = p[k]; k++; } out[k] = 0; return out;
}
static void split_action(const char *act, char *verb, char *arg) { /* "... pet_event.sh' VERB ARG..." -> verb, arg */
    verb[0] = arg[0] = 0; const char *p = strstr(act, ".sh' "); if (!p) { p = strstr(act, ".sh\" "); } if (!p) return; p += 5; while (*p == ' ') p++;
    size_t k = 0; while (p[k] && p[k] != ' ' && k < 47) { verb[k] = p[k]; k++; } verb[k] = 0; p += k; while (*p == ' ') p++; k = 0; while (p[k] && k < 95) { arg[k] = p[k]; k++; } arg[k] = 0; while (k && arg[k - 1] == ' ') arg[--k] = 0;
}
/* nav.pdl rows for a view */
static int nav_rows(const char *view, Btn *out, int max, int *n) {
    char *b = slurp(navf); if (!b) return -1; char *sv1 = NULL, *sv2 = NULL; char *line = strtok_r(b, "\n", &sv1);
    while (line) { char *l = trim(line); if (!strncmp(l, "NAV", 3)) { char *f[6] = { 0 }; int i = 0; char *t = strtok_r(l, "|", &sv2); while (t && i < 6) { f[i++] = trim(t); t = strtok_r(NULL, "|", &sv2); }
            if (i >= 5 && !strcmp(f[1], view) && *n < max) { Btn *x = &out[(*n)++]; memset(x, 0, sizeof *x); snprintf(x->where, 8, "nav"); snprintf(x->label, 96, "%s", f[2]); snprintf(x->verb, 48, "%s", f[3]); snprintf(x->arg, 96, "%s", f[4]); } } line = strtok_r(NULL, "\n", &sv1); }
    free(b); return 0;
}
/* the numbered buttons of one view */
static int navmap(const char *view, Btn *out, int max) {
    char *x = slurp(layout); if (!x) { fprintf(stderr, "layout_flow: cannot read %s\n", layout); return -1; }
    Btn nr[64]; int nn = 0; nav_rows(view, nr, 64, &nn);
    int cnt = 0, num = 0; const char *p = x; int in_nav_repeat = 0, in_dyn = 0;
    /* pass 1: tabs */
    for (int pass = 0; pass < 2; pass++) {
        p = x; in_nav_repeat = 0; in_dyn = 0;
        while ((p = strchr(p, '<'))) {
            const char *e = strchr(p, '>'); if (!e) break; size_t len = (size_t)(e - p); char tag[2048]; if (len >= sizeof tag) len = sizeof tag - 1; memcpy(tag, p, len); tag[len] = 0; p = e + 1;
            if (!strncmp(tag, "<!--", 4)) { const char *c = strstr(p - 1, "-->"); if (c) p = c + 3; continue; }
            if (!strncmp(tag, "<repeat", 7)) { char bind[32]; attr(tag, "bind", bind, sizeof bind);
                if (!strcmp(bind, "nav") && pass == 1) { for (int i = 0; i < nn && cnt < max; i++) { out[cnt] = nr[i]; out[cnt].n = ++num; if (!strcmp(out[cnt].label, "{party}")) { snprintf(out[cnt].label, 96, "{party row}"); } cnt++; } in_nav_repeat = 1; } else in_dyn = 1; continue; }
            if (!strncmp(tag, "</repeat", 8)) { in_nav_repeat = in_dyn = 0; continue; }
            if (in_nav_repeat || in_dyn) continue;
            int is_tab = !strncmp(tag, "<tab ", 5), is_item = !strncmp(tag, "<item ", 6), is_cli = !strncmp(tag, "<cli_io ", 8);
            if ((pass == 0 && !is_tab) || (pass == 1 && !(is_item || is_cli))) continue;
            Btn *b = &out[cnt]; memset(b, 0, sizeof *b); char act[512]; attr(tag, "label", b->label, sizeof b->label); attr(tag, "action", act, sizeof act); if (!act[0]) attr(tag, "onclick", act, sizeof act);
            split_action(act, b->verb, b->arg); snprintf(b->where, 8, is_tab ? "tab" : is_cli ? "field" : "side"); b->n = ++num; cnt++; if (cnt >= max) break;
        }
        if (pass == 0) { /* party rows need the actual names: published by the manager in state/ui.txt; here we expand {party} to six generic rows if the flow has a party file */ }
    }
    free(x);
    /* expand {party} rows: one per line of <state>/party.txt (house flow keeps it at ../state/party.txt next to nav.pdl) */
    char pf[PATH_MAX]; snprintf(pf, sizeof pf, "%s/state/party.txt", fdir); char *pt = slurp(pf);
    if (pt) { Btn tmp[MAXB]; int tn = 0, shift = 0; for (int i = 0; i < cnt; i++) {
            if (!strcmp(out[i].label, "{party row}")) { char *copy = strdup(pt); char *l = strtok(copy, "\n"); int k = 0; while (l && tn < MAXB - 1) { char id[32], nm[32]; if (sscanf(l, "%31s %31s", id, nm) == 2) { Btn *t = &tmp[tn++]; *t = out[i]; snprintf(t->label, 96, "%d %s", ++k, nm); snprintf(t->arg, 96, "%s", id); } l = strtok(NULL, "\n"); } free(copy); shift += k - 1; }
            else if (tn < MAXB) tmp[tn++] = out[i]; }
        int nn2 = 0; for (int i = 0; i < tn && i < max; i++) { out[i] = tmp[i]; out[i].n = ++nn2; } cnt = tn < max ? tn : max; free(pt); (void)shift; }
    return cnt;
}
static int has_word(const char *list, const char *w) { /* list tokens separated by | or space or ) */ size_t n = strlen(w); const char *p = list; while ((p = strstr(p, w))) { char a = p == list ? '|' : p[-1], b = p[n]; if ((a == '|' || a == ' ' || a == '"' || a == '(') && (b == '|' || b == ')' || b == ' ' || b == 0)) return 1; p += n; } return 0; }

static void report(int err, const char *msg, const char *fix) { printf("%s: %s | fix: %s\n", err ? "ERROR" : "WARN", msg, fix); if (err) nerr++; else nwarn++; }

static int cmd_check(void) {
    char cmd[PATH_MAX * 2 + 64]; snprintf(cmd, sizeof cmd, "'%s/@.apps/layout-studio/ops/+x/layout_check.+x' '%s'", house, layout);
    printf("-- layout_check %s\n", layout); fflush(stdout); if (system(cmd) != 0) { report(1, "layout_check failed", "fix the layout errors above first"); }
    char *vs = slurp(verbsf); if (!vs) { report(1, "verb script unreadable", "set FLOW | verbs to the script that has a case per verb"); return 1; }
    /* verbs = case labels at 4-space indent; stopped-gate = the line that lists the always-allowed verbs */
    static char cases[16384]; cases[0] = 0; char *copy = strdup(vs); char *line = strtok(copy, "\n"); char gate[8192] = "";
    while (line) { if (!strncmp(line, "    ", 4) && line[4] != ' ' && (isalpha((unsigned char)line[4]) || line[4] == '"')) { const char *c = strchr(line + 4, ')'); if (c) { size_t k = (size_t)(c - (line + 4)); if (k < 200 && strlen(cases) + k + 2 < sizeof cases) { strncat(cases, line + 4, k); strcat(cases, "|"); } } }
        if (strstr(line, "start|stop|status") && strstr(line, ";;")) snprintf(gate, sizeof gate, "%s", line); line = strtok(NULL, "\n"); }
    free(copy); free(vs);
    int checked = 0;
    for (int v = 0; v < nviews; v++) { Btn b[MAXB]; int n = navmap(views[v], b, MAXB); for (int i = 0; i < n; i++) { if (!b[i].verb[0]) continue; checked++;
            if (!has_word(cases, b[i].verb)) { char m[300]; snprintf(m, sizeof m, "view %s button %d \"%s\": verb \"%s\" has no case in %s", views[v], b[i].n, b[i].label, b[i].verb, verbsf); report(1, m, "add the case or fix the verb in nav.pdl / the layout"); }
            else if (gate[0] && !has_word(gate, b[i].verb) && strcmp(b[i].verb, "fire")) { char m[300]; snprintf(m, sizeof m, "view %s button %d \"%s\": verb \"%s\" is dropped while the pet is stopped", views[v], b[i].n, b[i].label, b[i].verb); report(0, m, "add it to the always-allowed list if it is window navigation"); } } }
    char *ks = slurp(keysf);
    if (ks) { int codes[64], names = 0; char nm[64][40]; char *l = strtok(ks, "\n"); while (l) { char *t = trim(l); if (!strncmp(t, "KEY", 3)) { char k[64], v[32], a[8]; if (sscanf(t, "%7s | %63s | %31s", a, k, v) == 3 && names < 64) { int c = atoi(v); for (int j = 0; j < names; j++) if (codes[j] == c && c != 255) { char m[200]; snprintf(m, sizeof m, "keybinds: %s and %s both use code %d", nm[j], k, c); report(1, m, "give one of them another code"); } codes[names] = c; snprintf(nm[names], 40, "%s", k); names++; } } l = strtok(NULL, "\n"); } free(ks); }
    printf("layout_flow check %s: %d view(s), %d button verb(s) checked, %d error(s), %d warning(s)\n", flowid, nviews, checked, nerr, nwarn); return nerr ? 1 : 0;
}
static void digits_out(int n, char *s) { snprintf(s, 16, "%d", n); }
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: layout_flow check|navmap|press <flow.pdl> ...\n"); return 2; }
    if (load_flow(argv[2])) return 2; if (!house[0]) snprintf(house, sizeof house, "%s", fdir);
    char hr[PATH_MAX]; if (realpath(house, hr)) snprintf(house, sizeof house, "%s", hr);
    if (!strcmp(argv[1], "check")) return cmd_check();
    if (argc < 4) return 2; Btn b[MAXB]; int n = navmap(argv[3], b, MAXB); if (n < 0) return 2;
    if (!strcmp(argv[1], "navmap")) { for (int i = 0; i < n; i++) printf("%d %s %s -> %s %s\n", b[i].n, b[i].where, b[i].label, b[i].verb, b[i].arg); return 0; }
    if (!strcmp(argv[1], "press") && argc >= 5) {
        char want[200] = ""; int pid = 0, esc = 0; for (int i = 4; i < argc; i++) { if (!strcmp(argv[i], "--pid") && i + 1 < argc) { pid = atoi(argv[++i]); continue; } if (!strcmp(argv[i], "--esc")) { esc = 1; continue; } if (want[0]) strcat(want, " "); strncat(want, argv[i], sizeof want - strlen(want) - 1); }
        for (int i = 0; i < n; i++) { if (strcasestr(b[i].label, want)) { char d[16]; digits_out(b[i].n, d); printf("%s\n", d);
                if (pid) { char rp[PATH_MAX]; snprintf(rp, sizeof rp, "%s/#.desktop/entity_menu_history/%d.txt", house, pid); FILE *f = fopen(rp, "a"); if (!f) { perror(rp); return 3; } if (esc) { fprintf(f, "# layout_flow press: Esc first (leave Interact mode)\nKEY_PRESSED: 27\n"); fflush(f); usleep(3000000); } fprintf(f, "# layout_flow press %s: %s (%s)\n", argv[3], b[i].label, d); for (char *c = d; *c; c++) { fprintf(f, "KEY_PRESSED: %d\n", *c); fflush(f); usleep(300000); } fprintf(f, "KEY_PRESSED: 13\n"); fclose(f); }
                return 0; } }
        fprintf(stderr, "layout_flow: no button \"%s\" in view %s (try navmap)\n", want, argv[3]); return 1; }
    return 2;
}
