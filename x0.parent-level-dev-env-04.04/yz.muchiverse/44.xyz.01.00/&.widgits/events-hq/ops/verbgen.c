#define _POSIX_C_SOURCE 200809L
/* verbgen.c - verb pages for a game (EVENT-VERB-PAGES-DESIGN.md, BOOK 16.game).
 *
 *   verbgen check <app_dir>   compare <app>/verbs.pdl with the engine source and with events/
 *   verbgen gen   <app_dir>   write events/<verb>/event_pkg/pages/page_1/ for every "page=yes" verb
 *
 * <app>/verbs.pdl (the one source of truth for which verbs are pages):
 *   GAME | <app name> | <registered command> | <engine path relative to the app> | <engine source relative to the app>
 *   VERB | <name> | yes|native|no | <trigger> | <args> | <summary>
 * page: yes = the UI plays events/<name>/ (engine "fire"), native = the engine already plays a page inside the verb
 * (play, stop, int), no = engine-internal or high frequency (a page run is a sh+bash+prisc launch: never per tick).
 *
 * check: every strcmp(v, "<name>") in the engine source must be a VERB row and every VERB row must be in the source
 * (exit 1 otherwise); every page=yes verb must have a package whose first NODE runs that verb.
 * gen: a package is written only when it does not exist or carries "META | generated | verbgen" and its NODE row is
 * unchanged since generation. A package that was edited (events-hq / blueprintz) or made by hand is skipped and
 * reported, never overwritten. Same four files as the hand-made events/pickup. Writes nothing else.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#define MAXV 200
typedef struct { char name[48], page[12], trig[24], args[48]; } Verb;
static Verb vb[MAXV]; static int nv;
static char gname[80], gcmd[80], geng[300], gsrc[300];

static int trim(char *s) { char *a = s; while (*a == ' ' || *a == '\t') a++; memmove(s, a, strlen(a) + 1); size_t n = strlen(s); while (n && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\n' || s[n-1] == '\r')) s[--n] = 0; return (int)n; }
static int split(char *line, char **f, int max) { int n = 0; char *p = line; while (n < max) { f[n++] = p; char *q = strchr(p, '|'); if (!q) break; *q = 0; p = q + 1; } for (int i = 0; i < n; i++) trim(f[i]); return n; }
static int mkdirs(const char *path) { char b[PATH_MAX]; snprintf(b, sizeof b, "%s", path); for (char *p = b + 1; *p; p++) if (*p == '/') { *p = 0; mkdir(b, 0755); *p = '/'; } return mkdir(b, 0755) == 0 || errno == EEXIST ? 0 : -1; }
static int read_all(const char *p, char *buf, size_t cap) { FILE *f = fopen(p, "r"); if (!f) return -1; size_t n = fread(buf, 1, cap - 1, f); buf[n] = 0; fclose(f); return (int)n; }

static int load_manifest(const char *app) {
    char p[PATH_MAX], line[512]; snprintf(p, sizeof p, "%s/verbs.pdl", app);
    FILE *f = fopen(p, "r"); if (!f) { fprintf(stderr, "verbgen: no %s\n", p); return -1; }
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#') continue; char *fl[8]; int n = split(line, fl, 8);
        if (n >= 5 && !strcmp(fl[0], "GAME")) { snprintf(gname, sizeof gname, "%s", fl[1]); snprintf(gcmd, sizeof gcmd, "%s", fl[2]); snprintf(geng, sizeof geng, "%s", fl[3]); snprintf(gsrc, sizeof gsrc, "%s", fl[4]); }
        else if (n >= 4 && !strcmp(fl[0], "VERB") && nv < MAXV) {
            Verb *v = &vb[nv++]; snprintf(v->name, sizeof v->name, "%s", fl[1]); snprintf(v->page, sizeof v->page, "%s", fl[2]);
            snprintf(v->trig, sizeof v->trig, "%s", fl[3]); snprintf(v->args, sizeof v->args, "%s", n > 4 ? fl[4] : "");
        }
    }
    fclose(f);
    if (!gname[0] || !gcmd[0] || !geng[0] || !gsrc[0]) { fprintf(stderr, "verbgen: verbs.pdl has no GAME row\n"); return -1; }
    for (int i = 0; i < nv; i++) {
        if (strcmp(vb[i].page, "yes") && strcmp(vb[i].page, "native") && strcmp(vb[i].page, "no")) { fprintf(stderr, "verbgen: verb %s: page must be yes|native|no, got '%s'\n", vb[i].name, vb[i].page); return -1; }
        for (const char *q = vb[i].name; *q; q++) if (!((*q >= 'a' && *q <= 'z') || (*q >= '0' && *q <= '9') || *q == '_')) { fprintf(stderr, "verbgen: bad verb name '%s'\n", vb[i].name); return -1; }
        for (int j = 0; j < i; j++) if (!strcmp(vb[i].name, vb[j].name)) { fprintf(stderr, "verbgen: duplicate verb %s\n", vb[i].name); return -1; }
    }
    return 0;
}

/* the node row exactly as the hand-made pickup package writes it */
static void node_row(char *o, size_t cap, const char *name) { snprintf(o, cap, "NODE         | id=1 type=%s | verb=%s|arg=", gcmd, name); }

static void pkg_paths(const char *app, const char *name, char *dir, size_t cap) { snprintf(dir, cap, "%s/events/%s/event_pkg/pages/page_1", app, name); }

/* 0 = missing, 1 = generated and unchanged, 2 = generated but edited, 3 = hand-made with the same verb, 4 = hand-made, other verb */
static int pkg_state(const char *app, const char *name) {
    char d[PATH_MAX], p[PATH_MAX], buf[8192], want[300]; pkg_paths(app, name, d, sizeof d); snprintf(p, sizeof p, "%s/event.ir.pdl", d);
    if (read_all(p, buf, sizeof buf) < 0) return 0;
    node_row(want, sizeof want, name);
    int stamp = strstr(buf, "META         | generated          | verbgen") != NULL;
    char *nd = strstr(buf, "NODE"); int same = 0;
    if (nd) { char *e = strchr(nd, '\n'); if (e) *e = 0; char a[300], b[300]; snprintf(a, sizeof a, "%s", nd); snprintf(b, sizeof b, "%s", want); trim(a); trim(b); same = !strcmp(a, b); }
    if (stamp) return same ? 1 : 2;
    return same ? 3 : 4;
}

static int write_file(const char *p, const char *txt, int exec) { char old[8192]; int n = read_all(p, old, sizeof old); if (n >= 0 && !strcmp(old, txt)) return 0;      /* identical: leave the file (and its mtime) alone */
    FILE *f = fopen(p, "w"); if (!f) return -1; fputs(txt, f); fclose(f); if (exec) chmod(p, 0755); return 0; }

static int gen_one(const char *app, const char *name) {
    char d[PATH_MAX], p[PATH_MAX], t[4096], node[300];
    pkg_paths(app, name, d, sizeof d); if (mkdirs(d)) { fprintf(stderr, "verbgen: cannot create %s\n", d); return -1; }
    node_row(node, sizeof node, name);
    snprintf(t, sizeof t, "SECTION      | KEY                | VALUE\n----------------------------------------\nMETA         | piece_id           | %s\nMETA         | generated          | verbgen\nSTATE        | source               | events-hq\n%s\n", name, node);
    snprintf(p, sizeof p, "%s/event.ir.pdl", d); if (write_file(p, t, 0)) return -1;
    snprintf(t, sizeof t, "SECTION      | KEY                | VALUE\n----------------------------------------\nMETA         | piece_id           | %s\nCOND | trigger | ui\n", name);
    snprintf(p, sizeof p, "%s/condition.pdl", d); if (write_file(p, t, 0)) return -1;
    snprintf(t, sizeof t, "# event.pal - generated by verbgen from verbs.pdl (EVENT-VERB-PAGES-DESIGN.md); edit event.ir.pdl, not this file\n# pkg=%s page=page_1\nexec cmd_1.sh\nhalt\n", name);
    snprintf(p, sizeof p, "%s/event.pal", d); if (write_file(p, t, 0)) return -1;
    snprintf(t, sizeof t, "#!/bin/sh\ncd \"$(dirname \"$0\")/../../..\" || exit 1\nENT=\"${MUCHI_TARGET_ENT:-$PWD}\"\nD=\"$ENT\"\nwhile [ \"$D\" != \"/\" ] && [ ! -d \"$D/xyzfs\" ]; do D=\"$(dirname \"$D\")\"; done\nexec \"$D/@.apps/%s/%s\" '%s'\n", gname, geng, name);
    snprintf(p, sizeof p, "%s/cmd_1.sh", d); if (write_file(p, t, 1)) return -1;
    return 0;
}

/* engine source: every strcmp(v, "name") */
static int source_verbs(const char *app, char names[][48], int max) {
    /* the GAME row's source field may list several files separated by "," (a game built from a shared core plus its own skin) */
    int n = 0; char list[300]; snprintf(list, sizeof list, "%s", gsrc);
    for (char *tok = strtok(list, ","); tok; tok = strtok(NULL, ",")) {
        char p[PATH_MAX]; if (tok[0] == '/') snprintf(p, sizeof p, "%s", tok); else snprintf(p, sizeof p, "%s/%s", app, tok);
        FILE *f = fopen(p, "r"); if (!f) { fprintf(stderr, "verbgen: cannot read %s\n", p); return -1; }
        char line[8192];
        while (fgets(line, sizeof line, f)) {
            char *s = line;
            while ((s = strstr(s, "strcmp(v,")) != NULL) {
                s += 9; while (*s == ' ') s++;
                if (*s == '"') { char *e = strchr(s + 1, '"'); if (e && e - s - 1 < 47) {
                    char nm[48]; memcpy(nm, s + 1, (size_t)(e - s - 1)); nm[e - s - 1] = 0; int dup = 0;
                    for (int i = 0; i < n; i++) if (!strcmp(names[i], nm)) dup = 1;
                    if (!dup && n < max) snprintf(names[n++], 48, "%s", nm); s = e; } }
            }
        }
        fclose(f);
    }
    return n;
}

/* a window file that calls a page=yes verb directly (not through "fire") would bypass the page: report it */
static int direct_calls(const char *app) {
    DIR *d = opendir(app); if (!d) return 0; struct dirent *e; int bad = 0; const char *base = strrchr(geng, '/'); base = base ? base + 1 : geng;
    while ((e = readdir(d))) {
        size_t n = strlen(e->d_name); if (n < 7 || strcmp(e->d_name + n - 6, ".xhtpm")) continue;
        char p[PATH_MAX]; static char buf[262144]; snprintf(p, sizeof p, "%s/%s", app, e->d_name); if (read_all(p, buf, sizeof buf) < 0) continue;
        char *s = buf;
        while ((s = strstr(s, base)) != NULL) {
            s += strlen(base); if (*s == '\'') s++; while (*s == ' ') s++;
            char w[48]; int k = 0; while (k < 47 && ((*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9') || *s == '_')) w[k++] = *s++; w[k] = 0;
            for (int j = 0; j < nv; j++) if (!strcmp(w, vb[j].name) && !strcmp(vb[j].page, "yes")) { printf("DIRECT CALL in %s: %s (use \"fire %s\")\n", e->d_name, w, w); bad++; }
        }
    }
    closedir(d); return bad;
}

int main(int argc, char **argv) {
    if (argc != 3 || (strcmp(argv[1], "check") && strcmp(argv[1], "gen"))) { fprintf(stderr, "usage: verbgen check|gen <app_dir>\n"); return 2; }
    const char *app = argv[2]; if (load_manifest(app)) return 2;
    int bad = 0;
    if (!strcmp(argv[1], "gen")) {
        int made = 0, same = 0, skip = 0;
        for (int i = 0; i < nv; i++) {
            if (strcmp(vb[i].page, "yes")) continue;
            int st = pkg_state(app, vb[i].name);
            if (st == 0 || st == 1) { if (gen_one(app, vb[i].name)) { bad++; continue; } if (st == 0) { made++; printf("made    %s\n", vb[i].name); } else { same++; } }
            else { skip++; printf("skipped %s (%s)\n", vb[i].name, st == 2 ? "generated, then edited" : st == 3 ? "hand-made, same verb" : "hand-made, runs another verb"); if (st == 4) bad++; }
        }
        printf("verbgen gen: made %d, unchanged %d, skipped %d, errors %d\n", made, same, skip, bad); return bad ? 1 : 0;
    }
    static char names[MAXV * 2][48]; int ns = source_verbs(app, names, MAXV * 2); if (ns < 0) return 2;
    int in_src_not_man = 0, in_man_not_src = 0, missing_pkg = 0, wrong_pkg = 0, ypages = 0, direct = direct_calls(app);
    for (int i = 0; i < ns; i++) { int f = 0; for (int j = 0; j < nv; j++) if (!strcmp(names[i], vb[j].name)) f = 1; if (!f) { printf("MISSING from verbs.pdl: %s\n", names[i]); in_src_not_man++; } }
    for (int j = 0; j < nv; j++) { int f = 0; for (int i = 0; i < ns; i++) if (!strcmp(names[i], vb[j].name)) f = 1; if (!f) { printf("NOT in the engine source: %s\n", vb[j].name); in_man_not_src++; } }
    for (int j = 0; j < nv; j++) {
        if (strcmp(vb[j].page, "yes")) continue; ypages++; int st = pkg_state(app, vb[j].name);
        if (st == 0) { printf("NO PACKAGE for page=yes verb: %s\n", vb[j].name); missing_pkg++; }
        else if (st == 4) { printf("PACKAGE runs another verb: %s\n", vb[j].name); wrong_pkg++; }
    }
    printf("verbgen check: engine verbs %d, manifest verbs %d (yes %d), missing-from-manifest %d, not-in-source %d, no-package %d, wrong-package %d, direct-calls %d\n", ns, nv, ypages, in_src_not_man, in_man_not_src, missing_pkg, wrong_pkg, direct);
    bad = in_src_not_man + in_man_not_src + missing_pkg + wrong_pkg + direct;
    printf("%s\n", bad ? "FAIL" : "PASS"); return bad ? 1 : 0;
}
