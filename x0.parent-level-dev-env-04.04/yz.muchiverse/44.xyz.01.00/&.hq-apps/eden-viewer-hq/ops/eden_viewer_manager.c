/* eden_viewer_manager.c - backend of the eden-viewer-hq window (first slice, READ ONLY).
 * Reads the Eden game of the current desk user and publishes eden_viewer_ui.txt (vars=) for the generic renderer:
 *   <house>/<current_xyzfs>/home/livedesk/eden_game/game/conductor/{variables.txt,status.txt,eden_history.txt}
 * current_xyzfs comes from the login file the 0.user-pal… folder, 00.login-signup/current_login.txt (relative path, never absolute).
 * EDEN_GAME_DIR (a conductor folder) overrides it, for the harness. It never writes the game. Change detection = file size, not mtime.
 *   run:   eden_viewer_manager <house_root> <package_dir>
 *   dump:  eden_viewer_manager --dump <conductor_dir>        prints the rows it would publish (harness hook)
 * Day and running come from variables.txt (live); people and tiles come from status.txt (as of the last Status). */
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>

#define NL 10
static char house_root[1024], pkg_dir[1024], cond[1024];
static volatile sig_atomic_t quit_flag;
static void on_term(int s) { (void)s; quit_flag = 1; }

static char out[16384]; static size_t outn;
static void put(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
#include <stdarg.h>
static void put(const char *fmt, ...) { va_list ap; va_start(ap, fmt); outn += (size_t)vsnprintf(out + outn, sizeof out - outn, fmt, ap); va_end(ap); if (outn > sizeof out - 1) outn = sizeof out - 1; }

static int read_all(const char *p, char *b, size_t n) { FILE *f = fopen(p, "r"); if (!f) return 0; size_t k = fread(b, 1, n - 1, f); b[k] = 0; fclose(f); return 1; }
/* value of "key=" inside a '|' row or a k=v file line; returns 1 and copies it */
static int field(const char *row, const char *key, char *v, size_t n) {
    size_t kl = strlen(key); const char *p = row;
    while (*p) { const char *e = strchr(p, '|'); size_t len = e ? (size_t)(e - p) : strlen(p);
        if (len > kl && !strncmp(p, key, kl) && p[kl] == '=') { size_t m = len - kl - 1; if (m >= n) m = n - 1; memcpy(v, p + kl + 1, m); v[m] = 0; return 1; }
        if (!e) break; p = e + 1; }
    v[0] = 0; return 0;
}
static int var_of(const char *file, const char *key, char *v, size_t n) {
    static char b[4096]; v[0] = 0; if (!read_all(file, b, sizeof b)) return 0;
    char *ln = strtok(b, "\n"); size_t kl = strlen(key);
    while (ln) { if (!strncmp(ln, key, kl) && ln[kl] == '=') { snprintf(v, n, "%s", ln + kl + 1); return 1; } ln = strtok(NULL, "\n"); }
    return 0;
}

static void find_conductor(void) {
    const char *ov = getenv("EDEN_GAME_DIR"); if (ov && *ov) { snprintf(cond, sizeof cond, "%s", ov); return; }
    cond[0] = 0; DIR *d = opendir(house_root); if (!d) return; struct dirent *e;
    while ((e = readdir(d))) { if (strncmp(e->d_name, "0.user-pal", 10)) continue;
        char lp[1536], b[2048], rel[512];
        snprintf(lp, sizeof lp, "%s/%s/00.login-signup/current_login.txt", house_root, e->d_name);
        if (!read_all(lp, b, sizeof b)) continue;
        if (var_of(lp, "current_xyzfs", rel, sizeof rel) && rel[0]) { snprintf(cond, sizeof cond, "%s/%s/home/livedesk/eden_game/game/conductor", house_root, rel); break; } }
    closedir(d);
}

static void build(void) {
    outn = 0; out[0] = 0; char p[1536];
    snprintf(p, sizeof p, "%s/variables.txt", cond);
    if (access(p, R_OK)) { put("head=Eden World\nstate=No Eden game found for this user yet (start one from the Eden button).\nn_people=0\nn_tiles=0\nn_log=0\ntiles=\n"); return; }
    char day[64] = "?", run[16] = "0"; var_of(p, "day", day, sizeof day); var_of(p, "running", run, sizeof run);
    put("head=Eden World  ·  day %s  ·  %s\n", day, !strcmp(run, "1") ? "running" : "paused/over");
    put("state=day %s   (people and tiles are from the last Status; press Status on the Eden button to refresh)\n", day);
    static char st[8192]; snprintf(p, sizeof p, "%s/status.txt", cond); if (!read_all(p, st, sizeof st)) st[0] = 0;
    int np = 0, nt = 0; char tiles[2048] = "";
    char *save = NULL, *ln = strtok_r(st, "\n", &save);
    char people[8][200]; char cells[24][40];
    while (ln) {
        char nm[64], kit[64];
        if (!strncmp(ln, "PART|", 5) && np < 8) { char hu[16], hp[16], mhp[16], coin[16], grain[16]; sscanf(ln, "PART|%63[^|]|%63[^|]", nm, kit);
            field(ln, "hunger", hu, sizeof hu); field(ln, "hp", hp, sizeof hp); field(ln, "mhp", mhp, sizeof mhp); field(ln, "coin", coin, sizeof coin); field(ln, "grain", grain, sizeof grain);
            snprintf(people[np++], 200, "@%s (%s)   hp %s/%s   hunger %s   coin %s   grain %s", nm, kit, hp, mhp, hu, coin, grain); }
        else if (!strncmp(ln, "ENT|", 4) && nt < 24) { char kind[64], x[32]; sscanf(ln, "ENT|%63[^|]|%63[^|]", nm, kind);
            if (!strcmp(kind, "house")) { field(ln, "condition", x, sizeof x); snprintf(cells[nt++], 40, "[house %s]", x); }
            else if (!strcmp(kind, "plant")) { char g[32]; field(ln, "state", x, sizeof x); field(ln, "grown", g, sizeof g); snprintf(cells[nt++], 40, "[plant s%s g%s]", x, g); }
            else { field(ln, "kind", x, sizeof x); snprintf(cells[nt++], 40, "[%s]", x[0] ? x : kind); } }
        ln = strtok_r(NULL, "\n", &save); }
    for (int i = 0; i < nt; i++) { strncat(tiles, cells[i], sizeof tiles - strlen(tiles) - 2); strncat(tiles, " ", sizeof tiles - strlen(tiles) - 1); }
    put("tiles=%s\nn_tiles=%d\n", tiles, nt); put("n_people=%d\n", np);
    for (int i = 0; i < np; i++) put("p_%d_text=%s\n", i, people[i]);
    /* ticker: last NL history rows (newest at the bottom), read from the file tail only - the file is large */
    snprintf(p, sizeof p, "%s/eden_history.txt", cond); FILE *f = fopen(p, "r"); int nlog = 0;
    if (f) { static char tail[8192]; fseek(f, 0, SEEK_END); long sz = ftell(f); long from = sz > (long)sizeof tail - 1 ? sz - ((long)sizeof tail - 1) : 0; fseek(f, from, SEEK_SET);
        size_t k = fread(tail, 1, sizeof tail - 1, f); tail[k] = 0; fclose(f);
        char *rows[256]; int nr = 0; char *s2 = NULL, *r = strtok_r(from ? strchr(tail, '\n') : tail, "\n", &s2);
        while (r && nr < 256) { rows[nr++] = r; r = strtok_r(NULL, "\n", &s2); }
        int a = nr > NL ? nr - NL : 0; for (int i = a; i < nr; i++) { for (char *c = rows[i]; *c; c++) if (*c == '|') *c = ' '; put("g_%d_text=%s\n", nlog++, rows[i]); } }
    put("n_log=%d\n", nlog);
}

int main(int argc, char **argv) {
    if (argc >= 3 && !strcmp(argv[1], "--dump")) { snprintf(cond, sizeof cond, "%s", argv[2]); build(); fputs(out, stdout); return 0; }
    if (argc < 3) { fprintf(stderr, "usage: %s <house_root> <package_dir>\n", argv[0]); return 2; }
    snprintf(house_root, sizeof house_root, "%s", argv[1]); snprintf(pkg_dir, sizeof pkg_dir, "%s", argv[2]);
    signal(SIGTERM, on_term); signal(SIGINT, on_term);
    char dst[1536], tmp[1600], last[sizeof out] = ""; snprintf(dst, sizeof dst, "%s/eden_viewer_ui.txt", pkg_dir); snprintf(tmp, sizeof tmp, "%s.tmp", dst);
    while (!quit_flag) {
        find_conductor(); build();
        if (strcmp(out, last)) { FILE *f = fopen(tmp, "w"); if (f) { fputs(out, f); fclose(f); rename(tmp, dst); snprintf(last, sizeof last, "%s", out); } }
        for (int i = 0; i < 10 && !quit_flag; i++) usleep(100000);
    }
    return 0;
}
