/* cpu_lint.c - the static pre-check for code that would throttle the machine (owner 2026-11-03; HQ-IQ-BOOK 03-pitfalls/hq-cpu-safety.md section 10).
 *   cpu_lint <house_root> <baseline.txt> [--list]       check against the baseline (exit 3 when any file has MORE hits than the baseline says)
 *   cpu_lint <house_root> --emit                        print every current hit as baseline rows
 * Heuristics, not proofs. Each kind is a pattern that has caused (or would cause) a hot machine:
 *   c-busy-loop      : for (;;) / while (1) with no blocking call (sleep, usleep, nanosleep, poll, select, read, recv, accept, wait, waitpid, flock, pause, fgets, fread, getline,
 *                      XNextEvent, run_op, system, popen, fork, ...) in the next 50 lines: a loop that can never leave (no break / return / exit / goto) and never blocks: it can only spin
 *   c-tight-sleep    : usleep(N) with a literal N <= 5000 (5 ms): a poll that wakes 200+ times a second
 *   sh-heavy-no-nice : a shell line running gcc / cc / clang / make / ffmpeg / convert / montage with no `nice` or `cpu_gate` on it or the line before (heavy work must be niced, one at a time)
 *   sh-loop-no-sleep : while true / while : / until false with no sleep / read / wait / timeout / inotifywait in the next 15 lines
 * A hit on a line (or the line before) carrying "cpu-ok:" is allowed on purpose. Excluded like cpu_lint: tests/, harness/, pieces/sessions, +x, vendor, ascii_frames, .hq_manager,
 * xyzfs/users, test-harn, symlinks. The ratchet is PER FILE AND KIND, so a file may only ever have fewer hits than the baseline says. Companion of cpu_gate (the launch-time check). */
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
static int skip_path(const char *p) {
    static const char *k[] = {"/pieces/sessions/", "/.git/", "/+x/", "/ascii_frames/", "/vendor/", "/.hq_manager/", "/xyzfs/users/", "/tests/", "/harness/", "test-harn", "quickjs", "/stb_", "/cpu_lint.c", "/miniaudio", "/sqlite3", NULL};
    char t[PATH_MAX + 2]; snprintf(t, sizeof t, "%s/", p);
    for (int i = 0; k[i]; i++) if (strstr(t, k[i])) return 1;
    return 0;
}
static void add_site(const char *kind, const char *rel, int line) {
    if (nsites == capsites) { capsites = capsites ? capsites * 2 : 256; sites = realloc(sites, (size_t)capsites * sizeof(Site)); }
    Site *s = &sites[nsites++]; snprintf(s->kind, sizeof s->kind, "%s", kind); snprintf(s->path, sizeof s->path, "%s", rel); s->line = line;
    find(&cur, &ncur, &capc, kind, rel, 1)->n++;
}
static int has_any(char **lines, int from, int to, const char **words, int n) {
    for (int k = from; k < to && k < n; k++) for (int w = 0; words[w]; w++) if (strstr(lines[k], words[w])) return 1;
    return 0;
}
/* the lines of the loop body that starts at line i: from the first '{' (on this line or the next) to its matching '}' (string/char literals ignored); -1 when no body is found within 3 lines */
static int body_end(char **lines, int i, int n) {
    int depth = 0, started = 0;
    for (int k = i; k < n && k < i + 400; k++) {
        const char *p = lines[k]; int instr = 0; char q = 0;
        for (; *p; p++) {
            if (instr) { if (*p == '\\' && p[1]) p++; else if (*p == q) instr = 0; continue; }
            if (*p == '"' || *p == '\'') { instr = 1; q = *p; continue; }
            if (p[0] == '/' && p[1] == '/') break;
            if (*p == '{') { depth++; started = 1; } else if (*p == '}') { depth--; if (started && depth == 0) return k; }
        }
        if (!started && k >= i + 3) return -1;
    }
    return -1;
}
static void scan_file(const char *full, const char *rel, int is_c) {
    FILE *f = fopen(full, "r"); if (!f) return;
    char **lines = NULL; int n = 0, cap = 0; char buf[4096];
    while (fgets(buf, sizeof buf, f)) { if (n == cap) { cap = cap ? cap * 2 : 256; lines = realloc(lines, (size_t)cap * sizeof *lines); } lines[n++] = strdup(buf); }
    fclose(f);
    static const char *c_block[] = {"sleep(", "usleep(", "nanosleep(", "poll(", "select(", "read(", "recv(", "accept(", "wait(", "waitpid(", "flock(", "pause(", "fgets(", "fread(", "getline(",
        "XNextEvent", "run_op(", "system(", "popen(", "fork(", "sigsuspend(", "epoll_wait(", "sem_wait(", "pthread_cond_wait(", "msleep(", "ms_sleep(", "sleep_ms(", "sleep_us(", "break", "return", "exit(", "goto ", "_exit(", "BV_STOP", NULL};
    static const char *sh_block[] = {"sleep", "read ", "wait", "timeout", "inotifywait", NULL};
    for (int i = 0; i < n; i++) {
        const char *l = lines[i]; const char *t = l; while (*t == ' ' || *t == '\t') t++;
        int comment = is_c ? (!strncmp(t, "//", 2) || *t == '*' || !strncmp(t, "/*", 2)) : *t == '#';
        if (comment) continue;
        const char *kind = NULL;
        if (is_c) {
            if (strstr(l, "for (;;)") || strstr(l, "for(;;)") || strstr(l, "while (1)") || strstr(l, "while(1)")) { int be = body_end(lines, i, n); if (be >= 0 && !has_any(lines, i, be + 1, c_block, n)) kind = "c-busy-loop"; }
            const char *u = strstr(l, "usleep(");
            if (!kind && u && (u == l || !(isalnum((unsigned char)u[-1]) || u[-1] == '_'))) { const char *q = u + 7; while (*q == ' ') q++; if (*q >= '0' && *q <= '9') { long v = strtol(q, NULL, 10); const char *r = q; while (*r >= '0' && *r <= '9') r++; if (*r == ')' && v > 0 && v <= 5000) kind = "c-tight-sleep"; } }
        } else {
            static const char *heavy[] = {"gcc ", "cc ", "clang ", "make ", "ffmpeg ", "convert ", "montage ", NULL};
            for (int w = 0; heavy[w] && !kind; w++) { const char *p = l; size_t wl = strlen(heavy[w]);
                while ((p = strstr(p, heavy[w]))) { int before = p == l || p[-1] == ' ' || p[-1] == '\t' || p[-1] == '(' || p[-1] == ';' || p[-1] == '&' || p[-1] == '|' || p[-1] == '-' || p[-1] == '{' || p[-1] == '"';
                    if (before && p != l && p[-1] == '-' ) before = 0;
                    if (before && !strstr(l, "echo ") && !strstr(l, "command -v") && !strstr(l, "which ") && !strstr(l, "grep ")) { if (!strstr(l, "nice") && !strstr(l, "cpu_gate") && !(i > 0 && (strstr(lines[i - 1], "nice") || strstr(lines[i - 1], "cpu_gate")))) kind = "sh-heavy-no-nice"; break; }
                    p += wl; } }
            if (!kind && (strstr(l, "while true") || strstr(l, "while :") || strstr(l, "while [ 1 ]") || strstr(l, "until false"))) { if (!has_any(lines, i, i + 15, sh_block, n)) kind = "sh-loop-no-sleep"; }
        }
        if (!kind) continue;
        if (strstr(l, "cpu-ok:") || (i > 0 && strstr(lines[i - 1], "cpu-ok:"))) { allowed++; if (list_all) printf("ALLOWED %s:%d\n", rel, i + 1); continue; }
        add_site(kind, rel, i + 1);
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
    if (argc < 3) { fprintf(stderr, "usage: cpu_lint <house_root> <baseline.txt> [--list] | <house_root> --emit\n"); return 2; }
    if (!realpath(argv[1], house)) { fprintf(stderr, "cpu_lint: no such house\n"); return 2; }
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
    if (!b) { fprintf(stderr, "cpu_lint: cannot read baseline %s\n", argv[2]); return 2; }
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
