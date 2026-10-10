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
typedef struct { char where[8], label[96], verb[48], arg[96], tgt[48]; int n; } Btn;
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
/* the numbered buttons of one view. Real numbering order (read off live frames): tabs, bottom footer, the sidebar rows (+ the nav.pdl rows), cli_io fields, then - only while a dropdown is open -
 * that dropdown's rows, then the overlay row (hotbar). Dropdown rows are listed with n = 0 and their tab in tgt; press opens the tab, then picks the row. */
static int party_rows(Btn *out, int max, int *cnt, int *num) {
    char pf[PATH_MAX]; snprintf(pf, sizeof pf, "%s/state/party.txt", fdir); char *pt = slurp(pf); if (!pt) return 0; char *sv = NULL; char *l = strtok_r(pt, "\n", &sv); int k = 0;
    while (l && *cnt < max) { char id[32], nm[32]; if (sscanf(l, "%31s %31s", id, nm) == 2) { Btn *b = &out[(*cnt)++]; memset(b, 0, sizeof *b); snprintf(b->where, 8, "foot"); snprintf(b->label, 96, "%s", nm); snprintf(b->verb, 48, "select"); snprintf(b->arg, 96, "%s", id); b->n = ++(*num); k++; } l = strtok_r(NULL, "\n", &sv); }
    free(pt); return k;
}
static int navmap(const char *view, Btn *out, int max) {
    char *x = slurp(layout); if (!x) { fprintf(stderr, "layout_flow: cannot read %s\n", layout); return -1; }
    Btn nr[64]; int nn = 0; nav_rows(view, nr, 64, &nn);
    int cnt = 0, num = 0; static Btn drops[64]; int ndrop = 0;
    for (int pass = 0; pass < 4; pass++) {                       /* 0 tabs, 1 footer, 2 sidebar+cli_io (+drop rows listed), 3 overlay */
        const char *p = x; int in_footer = 0, in_overlay = 0, in_dyn = 0, in_navrep = 0, in_bkrep = 0;
        while ((p = strchr(p, '<'))) {
            const char *e = strchr(p, '>'); if (!e) break; size_t len = (size_t)(e - p); char tag[2048]; if (len >= sizeof tag) len = sizeof tag - 1; memcpy(tag, p, len); tag[len] = 0; p = e + 1;
            if (!strncmp(tag, "<!--", 4)) { const char *c = strstr(p - 1, "-->"); if (c) p = c + 3; continue; }
            if (!strncmp(tag, "<footer", 7)) { in_footer = 1; continue; } if (!strncmp(tag, "</footer", 8)) { in_footer = 0; continue; }
            if (!strncmp(tag, "<row", 4)) { char cls[256]; attr(tag, "class", cls, sizeof cls); if (strstr(cls, "canvas-overlay")) in_overlay = 1; continue; } if (!strncmp(tag, "</row", 5)) { in_overlay = 0; continue; }
            if (!strncmp(tag, "<repeat", 7)) { char bind[32]; attr(tag, "bind", bind, sizeof bind);
                if (!strcmp(bind, "nav")) { if (pass == 2) for (int i = 0; i < nn && cnt < max; i++) { out[cnt] = nr[i]; out[cnt].n = ++num; cnt++; } in_navrep = 1; }
                else if (!strcmp(bind, "bk") && in_footer) { if (pass == 1) party_rows(out, max, &cnt, &num); in_bkrep = 1; }
                else in_dyn = 1; continue; }
            if (!strncmp(tag, "</repeat", 8)) { in_navrep = in_dyn = in_bkrep = 0; continue; }
            if (in_navrep || in_dyn || in_bkrep) { if (!(in_bkrep && 0)) { /* dropdown-child rows inside a repeat (book list) are listed once, not numbered */
                    char cls[256]; if (in_bkrep && pass == 2 && !strncmp(tag, "<item ", 6) && (attr(tag, "class", cls, sizeof cls), strstr(cls, "dropdown-child")) && ndrop < 64) { Btn *d = &drops[ndrop++]; memset(d, 0, sizeof *d); char act[512], tg[48]; attr(tag, "target_id", tg, sizeof tg); attr(tag, "action", act, sizeof act); split_action(act, d->verb, d->arg); snprintf(d->where, 8, "drop"); snprintf(d->tgt, 48, "%s", tg); snprintf(d->label, 96, "(each pet)"); } }
                continue; }
            int is_tab = !strncmp(tag, "<tab ", 5), is_item = !strncmp(tag, "<item ", 6), is_cli = !strncmp(tag, "<cli_io ", 8); if (!(is_tab || is_item || is_cli)) continue;
            char cls[256]; attr(tag, "class", cls, sizeof cls); int is_drop = strstr(cls, "dropdown-child") != NULL;
            int want = is_tab ? (pass == 0) : in_footer ? (pass == 1) : in_overlay ? (pass == 3) : (pass == 2);
            if (is_drop) want = (pass == 2);
            if (!want) continue;
            Btn *b = (is_drop && ndrop < 64) ? &drops[ndrop++] : (cnt < max ? &out[cnt++] : NULL); if (!b) break; memset(b, 0, sizeof *b);
            char act[512]; attr(tag, "label", b->label, sizeof b->label); attr(tag, "action", act, sizeof act); if (!act[0]) attr(tag, "onclick", act, sizeof act); attr(tag, "target_id", b->tgt, sizeof b->tgt);
            split_action(act, b->verb, b->arg);
            if (is_drop) { snprintf(b->where, 8, "drop"); b->n = 0; }
            else { snprintf(b->where, 8, is_tab ? "tab" : is_cli ? "field" : in_footer ? "foot" : in_overlay ? "hot" : "side"); b->n = ++num; }
        }
        if (pass == 2) { /* dropdown rows go after the numbered base items, tagged with the tab that opens them */
            for (int i = 0; i < ndrop && cnt < max; i++) out[cnt++] = drops[i]; ndrop = 0; }
    }
    free(x); return cnt;
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
    if (!strcmp(argv[1], "navmap")) { for (int i = 0; i < n; i++) printf("%d %s %s%s%s%s -> %s %s\n", b[i].n, b[i].where, b[i].tgt[0] && !strcmp(b[i].where, "drop") ? "[" : "", b[i].tgt[0] && !strcmp(b[i].where, "drop") ? b[i].tgt : "", b[i].tgt[0] && !strcmp(b[i].where, "drop") ? "] " : "", b[i].label, b[i].verb, b[i].arg); return 0; }
    if (!strcmp(argv[1], "press") && argc >= 5) {
        char want[200] = ""; int pid = 0, esc = 0; for (int i = 4; i < argc; i++) { if (!strcmp(argv[i], "--pid") && i + 1 < argc) { pid = atoi(argv[++i]); continue; } if (!strcmp(argv[i], "--esc")) { esc = 1; continue; } if (want[0]) strcat(want, " "); strncat(want, argv[i], sizeof want - strlen(want) - 1); }
        int hit = -1; for (int i = 0; i < n; i++) if (strcasestr(b[i].label, want)) { hit = i; break; }
        if (hit < 0) { fprintf(stderr, "layout_flow: no button \"%s\" in view %s (try navmap)\n", want, argv[3]); return 1; }
        int steps[2], ns = 0;                                   /* the numbers to press, in order */
        if (!strcmp(b[hit].where, "drop")) {
            int tab = 0, base = 0, idx = 0;                     /* base = how many numbered items come before the open dropdown's rows */
            for (int i = 0; i < n; i++) { if (!strcmp(b[i].where, "tab") && !strcmp(b[i].tgt, b[hit].tgt)) tab = b[i].n; if (b[i].n > 0 && strcmp(b[i].where, "hot")) base = b[i].n > base ? b[i].n : base; }
            for (int i = 0; i < n; i++) if (!strcmp(b[i].where, "drop") && !strcmp(b[i].tgt, b[hit].tgt)) { idx++; if (i == hit) break; }
            if (!tab) { fprintf(stderr, "layout_flow: no tab opens dropdown %s\n", b[hit].tgt); return 1; }
            steps[ns++] = tab; steps[ns++] = -idx; (void)base;          /* negative = inside the open dropdown: digits do not jump there (scoped nav), so press Down (idx-1) times then Enter */
        } else steps[ns++] = b[hit].n;
        for (int s = 0; s < ns; s++) { if (steps[s] < 0) printf("Down x%d, Enter\n", -steps[s] - 1); else printf("%d%s", steps[s], s + 1 < ns ? " then " : "\n"); }
        if (pid) { char rp[PATH_MAX]; snprintf(rp, sizeof rp, "%s/#.desktop/entity_menu_history/%d.txt", house, pid); FILE *f = fopen(rp, "a"); if (!f) { perror(rp); return 3; }
            if (esc) { fprintf(f, "# layout_flow press: Esc first\nKEY_PRESSED: 27\n"); fflush(f); usleep(3000000); }
            for (int s = 0; s < ns; s++) { char d[16]; if (steps[s] < 0) { fprintf(f, "# layout_flow press %s: %s (Down x%d, Enter)\n", argv[3], b[hit].label, -steps[s] - 1); for (int k = 1; k < -steps[s]; k++) { fprintf(f, "KEY_PRESSED: 201\n"); fflush(f); usleep(600000); } fprintf(f, "KEY_PRESSED: 13\n"); fflush(f); usleep(300000); continue; }
                snprintf(d, sizeof d, "%d", steps[s]); fprintf(f, "# layout_flow press %s: %s (%s)\n", argv[3], b[hit].label, d);
                for (char *c = d; *c; c++) { fprintf(f, "KEY_PRESSED: %d\n", *c); fflush(f); usleep(450000); } fprintf(f, "KEY_PRESSED: 13\n"); fflush(f); usleep(s + 1 < ns ? 2500000 : 300000); }
            fclose(f); }
        return 0; }
    return 2;
}
