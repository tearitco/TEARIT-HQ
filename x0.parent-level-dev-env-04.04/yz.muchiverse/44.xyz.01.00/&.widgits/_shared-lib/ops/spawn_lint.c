/* spawn_lint.c - the ratchet that keeps new unregistered process launches out of the house (2026-11-02).
 *   spawn_lint <house_root> <baseline.txt> [--list]     check against the baseline
 *   spawn_lint <house_root> --emit                      print every current site as baseline rows (how the baseline file is generated)
 * Finds every launch site that can leave a detached process and does NOT register it:
 *   sh-setsid-nohup : a shell line with setsid+nohup, or setsid ending in & , with no registry call (kh_spawn.sh, proc_reg_op, proc_ledger_add, kh_proc_register,
 *                     livedesk_proc_list) on that line or the next three
 *   c-setsid        : setsid( in a C file that mentions no registry call (kh_spawn, kh_proc_register, kh_proc_self_register, proc_reg_op, ktb_system_recorded, ...)
 * A site on a line (or the line before) carrying "proc-ok:" is allowed on purpose (and listed with --list). Excluded: tests/, harness/, pieces/sessions, +x, vendor, ascii_frames,
 * .hq_manager, xyzfs/users, test-harn, symlinks.
 * Baseline = PROC-REGISTRY-BASELINE-2026-11-02.txt ("<kind> <path>:<line>" rows). The comparison is PER FILE AND KIND (line numbers shift on any edit): a file may have FEWER
 * sites than the baseline says, never more, and a file the baseline does not list may have none. Output: NEW rows, then a summary; exit 0 clean, 3 when anything is new.
 * Plan: PROC-REGISTRY-STANDARD-AND-REFACTOR-PLAN-2026-11-02.md section 3.4 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <limits.h>
#include <ctype.h>
#include <sys/stat.h>

typedef struct { char kind[24]; char path[PATH_MAX]; int n; } Cnt;
static Cnt *base, *cur; static int nbase, ncur, capb, capc;
static char house[PATH_MAX]; static int list_all = 0, allowed = 0;
typedef struct { char kind[24]; char path[PATH_MAX]; int line; } Site;
static Site *sites; static int nsites, capsites;

static Cnt *find(Cnt **arr, int *n, int *cap, const char *kind, const char *path, int create) {
    for (int i = 0; i < *n; i++) if (!strcmp((*arr)[i].kind, kind) && !strcmp((*arr)[i].path, path)) return &(*arr)[i];
    if (!create) return NULL;
    if (*n == *cap) { *cap = *cap ? *cap * 2 : 256; *arr = realloc(*arr, (size_t)*cap * sizeof(Cnt)); }
    Cnt *c = &(*arr)[(*n)++]; snprintf(c->kind, sizeof c->kind, "%s", kind); snprintf(c->path, sizeof c->path, "%s", path); c->n = 0;
    return c;
}
static int has_word(const char *s, const char *w) {
    size_t l = strlen(w);
    for (const char *p = s; (p = strstr(p, w)); p += l) {
        int before = p == s || !(isalnum((unsigned char)p[-1]) || p[-1] == '_'), after = !(isalnum((unsigned char)p[l]) || p[l] == '_');
        if (before && after) return 1;
    }
    return 0;
}
static const char *reg_pats[] = {"kh_proc_register", "kh_spawn", "proc_ledger_add", "livedesk_proc_list", "kh_proc_self_register", "ktb_system_recorded", "proc_reg_op", NULL};
static int mentions_reg(const char *s) { for (int i = 0; reg_pats[i]; i++) if (strstr(s, reg_pats[i])) return 1; return 0; }
static int skip_path(const char *p) {
    static const char *k[] = {"/pieces/sessions/", "/.git/", "/+x/", "/ascii_frames/", "/vendor/", "/.hq_manager/", "/xyzfs/users/", "/tests/", "/harness/", "test-harn", NULL};
    char t[PATH_MAX + 2]; snprintf(t, sizeof t, "%s/", p);
    for (int i = 0; k[i]; i++) if (strstr(t, k[i])) return 1;
    return 0;
}
static void add_site(const char *kind, const char *rel, int line) {
    if (nsites == capsites) { capsites = capsites ? capsites * 2 : 256; sites = realloc(sites, (size_t)capsites * sizeof(Site)); }
    Site *s = &sites[nsites++]; snprintf(s->kind, sizeof s->kind, "%s", kind); snprintf(s->path, sizeof s->path, "%s", rel); s->line = line;
    find(&cur, &ncur, &capc, kind, rel, 1)->n++;
}
static void scan_file(const char *full, const char *rel, int is_c) {
    FILE *f = fopen(full, "r"); if (!f) return;
    char **lines = NULL; int n = 0, cap = 0; char buf[4096];
    while (fgets(buf, sizeof buf, f)) { if (n == cap) { cap = cap ? cap * 2 : 256; lines = realloc(lines, (size_t)cap * sizeof *lines); } lines[n++] = strdup(buf); }
    fclose(f);
    int file_reg = 0;
    if (is_c) for (int i = 0; i < n && !file_reg; i++) file_reg = mentions_reg(lines[i]);
    for (int i = 0; i < n; i++) {
        const char *l = lines[i]; const char *t = l; while (*t == ' ' || *t == '\t') t++;
        int comment = is_c ? (!strncmp(t, "//", 2) || *t == '*' || !strncmp(t, "/*", 2)) : *t == '#';
        if (comment) continue;
        int hit = 0;
        if (!is_c) {
            size_t len = strlen(l); while (len && (l[len - 1] == '\n' || l[len - 1] == '\r' || l[len - 1] == ' ')) len--;
            int amp = len && l[len - 1] == '&';
            if (has_word(l, "setsid") && (has_word(l, "nohup") || amp)) hit = 1;
            if (hit) for (int k = i; k < n && k < i + 4; k++) if (mentions_reg(lines[k])) { hit = 0; break; }
        } else if (!file_reg) {
            const char *p = l;
            while ((p = strstr(p, "setsid"))) { const char *q = p + 6; while (*q == ' ') q++; if (*q == '(' && (p == l || !(isalnum((unsigned char)p[-1]) || p[-1] == '_'))) { hit = 1; break; } p += 6; }
        }
        if (!hit) continue;
        if (strstr(l, "proc-ok:") || (i > 0 && strstr(lines[i - 1], "proc-ok:"))) { allowed++; if (list_all) printf("ALLOWED %s:%d\n", rel, i + 1); continue; }
        add_site(is_c ? "c-setsid" : "sh-setsid-nohup", rel, i + 1);
    }
    for (int i = 0; i < n; i++) free(lines[i]);
    free(lines);
}
static void walk(const char *dir) {
    DIR *d = opendir(dir); if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char p[PATH_MAX]; snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
        struct stat st; if (lstat(p, &st) != 0 || S_ISLNK(st.st_mode)) continue;
        if (skip_path(p)) continue;
        if (S_ISDIR(st.st_mode)) { walk(p); continue; }
        size_t l = strlen(e->d_name); int sh = l > 3 && !strcmp(e->d_name + l - 3, ".sh"), c = l > 2 && (!strcmp(e->d_name + l - 2, ".c") || !strcmp(e->d_name + l - 2, ".h"));
        if (!sh && !c) continue;
        scan_file(p, p + strlen(house) + 1, c);
    }
    closedir(d);
}
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: spawn_lint <house_root> <baseline.txt> [--list] | <house_root> --emit\n"); return 2; }
    if (!realpath(argv[1], house)) { fprintf(stderr, "spawn_lint: no such house\n"); return 2; }
    if (!strcmp(argv[2], "--emit")) {
        walk(house);
        for (int i = 0; i < nsites; i++) for (int j = i + 1; j < nsites; j++) {      /* stable order: kind, path, line */
            int c = strcmp(sites[i].kind, sites[j].kind); if (!c) c = strcmp(sites[i].path, sites[j].path); if (!c) c = sites[i].line - sites[j].line;
            if (c > 0) { Site t = sites[i]; sites[i] = sites[j]; sites[j] = t; } }
        for (int i = 0; i < nsites; i++) printf("%s %s:%d\n", sites[i].kind, sites[i].path, sites[i].line);
        return 0;
    }
    list_all = argc > 3 && !strcmp(argv[3], "--list");
    FILE *b = fopen(argv[2], "r");
    if (!b) { fprintf(stderr, "spawn_lint: cannot read baseline %s\n", argv[2]); return 2; }
    char line[PATH_MAX + 64]; int nb_sites = 0;
    while (fgets(line, sizeof line, b)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char kind[24], path[PATH_MAX];
        if (sscanf(line, "%23s %4095[^\n]", kind, path) != 2) continue;
        char *colon = strrchr(path, ':'); if (colon) *colon = 0;
        find(&base, &nbase, &capb, kind, path, 1)->n++; nb_sites++;
    }
    fclose(b);
    walk(house);
    int newc = 0, fixed = 0;
    for (int i = 0; i < ncur; i++) {
        Cnt *bc = find(&base, &nbase, &capb, cur[i].kind, cur[i].path, 0); int allow = bc ? bc->n : 0;
        if (cur[i].n > allow) {
            newc += cur[i].n - allow;
            for (int s = 0; s < nsites; s++) if (!strcmp(sites[s].kind, cur[i].kind) && !strcmp(sites[s].path, cur[i].path)) printf("NEW %s %s:%d\n", sites[s].kind, sites[s].path, sites[s].line);
        }
    }
    for (int i = 0; i < nbase; i++) { Cnt *c = find(&cur, &ncur, &capc, base[i].kind, base[i].path, 0); int now = c ? c->n : 0; if (now < base[i].n) fixed += base[i].n - now; }
    printf("sites=%d baseline=%d new=%d fixed=%d allowed=%d\n", nsites, nb_sites, newc, fixed, allowed);
    return newc ? 3 : 0;
}
