/* asset_root_op - find where a machine keeps a book's asset files, from a DATA file (book-stack reader phase R3a, 2026-11-02).
 * Replaces the same path hunt that was copied into three scripts (bible_text, bible_tts, tao): environment override, Windows paths, the Mac Desktop, the Linux share that
 * must be mounted first. The candidates are rows in a .pdl; the op only checks them in order and prints the first that really exists.
 *
 * Usage: asset_root_op <roots.pdl> <bible|share> [<mount_script>]
 *   bible  prints the bible folder (the one that contains the PROBE path)       share  prints its parent (the folder that also holds tao-te-ching-tty/ ...)
 *   mount_script  optional: a bash file defining ensure_book_mount (see book-stack pieces/_shared/ensure_book_mount.sh); sourced and run ONLY when no candidate
 *                 exists yet and a Linux row is marked mount=1, then the rows are tried once more.
 * roots.pdl rows (pipe separated, '#' comments, no pipe inside a field):
 *   PROBE    | <path that must exist under the bible folder>               e.g. bible.ch2en.ran
 *   BIBLEDIR | <name of the bible folder inside the share>                  e.g. bible-ench.twins+ai]b2   (used for the two environment overrides)
 *   ROOT     | <os> | <path of the BIBLE folder> | <mount 0|1>              os = linux | darwin | win | any ; a leading ~ is $HOME ; rows are tried in file order
 * Order of candidates: $BIBLE_ASSET_ROOT (a bible folder, OR a share folder that holds BIBLEDIR), $BOOK_SHARE_ROOT (a share folder), then the ROOT rows for this OS.
 * Output: the path on stdout, nothing else. Exit 0 found, 1 none found (every tried path is listed on stderr, so a missing drive is loud, never a silent no-op), 2 usage.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/wait.h>

#define PB 4352
#define MAXR 64
typedef struct { char os[16]; char path[PB]; int mount; } Root;

static char g_probe[256], g_bibledir[256];
static Root R[MAXR]; static int nR = 0;

static void trim(char *s) {
    size_t n = strlen(s); while (n && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' ' || s[n - 1] == '\t')) s[--n] = 0;
    char *p = s; while (*p == ' ' || *p == '\t') p++; if (p != s) memmove(s, p, strlen(p) + 1);
}
static int exists(const char *p) { struct stat st; return stat(p, &st) == 0; }
static int fit(char *out, size_t n, const char *fmt, const char *a, const char *b) { int w = snprintf(out, n, fmt, a, b); return w >= 0 && (size_t)w < n; }
static const char *this_os(void) {
    struct utsname u; if (uname(&u) != 0) return "linux";
    if (!strcmp(u.sysname, "Darwin")) return "darwin";
    if (!strncmp(u.sysname, "MINGW", 5) || !strncmp(u.sysname, "MSYS", 4) || !strncmp(u.sysname, "CYGWIN", 6)) return "win";
    return "linux";
}
static int load(const char *file) {
    FILE *f = fopen(file, "r"); if (!f) { fprintf(stderr, "asset_root_op: cannot read %s\n", file); return 0; }
    char line[PB + 128];
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#') continue;
        char *f1[4]; int k = 0; char *c = line;
        while (k < 4) { f1[k++] = c; char *bar = strstr(c, " | "); if (!bar) break; *bar = 0; c = bar + 3; }
        for (int i = 0; i < k; i++) trim(f1[i]);
        if (k >= 2 && !strcmp(f1[0], "PROBE")) snprintf(g_probe, sizeof g_probe, "%s", f1[1]);
        else if (k >= 2 && !strcmp(f1[0], "BIBLEDIR")) snprintf(g_bibledir, sizeof g_bibledir, "%s", f1[1]);
        else if (k >= 4 && !strcmp(f1[0], "ROOT") && nR < MAXR) {
            snprintf(R[nR].os, sizeof R[nR].os, "%s", f1[1]);
            const char *home = getenv("HOME");
            if (f1[2][0] == '~' && home) { if (!fit(R[nR].path, sizeof R[nR].path, "%s%s", home, f1[2] + 1)) continue; }
            else snprintf(R[nR].path, sizeof R[nR].path, "%s", f1[2]);
            R[nR].mount = f1[3][0] == '1'; nR++;
        }
    }
    fclose(f); return 1;
}
/* a bible folder is real when <dir>/<PROBE> exists */
static int is_bible(const char *dir) { char p[PB]; return fit(p, sizeof p, "%s/%s", dir, g_probe) && exists(p); }

static void run_mount(const char *script) {
    pid_t pid = fork(); if (pid < 0) return;
    if (pid == 0) {
        int nul = open("/dev/null", O_WRONLY); if (nul >= 0) { dup2(nul, 1); dup2(nul, 2); close(nul); }
        execlp("bash", "bash", "-c", ". \"$1\"; ensure_book_mount", "asset_root_op", script, (char *)NULL); _exit(127);
    }
    int st; waitpid(pid, &st, 0);
}

int main(int argc, char **argv) {
    if (argc < 3 || (strcmp(argv[2], "bible") && strcmp(argv[2], "share"))) { fprintf(stderr, "Usage: asset_root_op <roots.pdl> <bible|share> [<mount_script>]\n"); return 2; }
    if (!load(argv[1])) return 1;
    if (!g_probe[0]) { fprintf(stderr, "asset_root_op: no PROBE row in %s\n", argv[1]); return 1; }
    const char *os = this_os(), *mount_script = argc > 3 ? argv[3] : NULL;
    static char tried[8192]; tried[0] = 0;
    char found[PB] = "";
    for (int pass = 0; pass < 2 && !found[0]; pass++) {
        if (pass == 1) {                                        /* second pass only after a mount attempt */
            int want = 0; for (int i = 0; i < nR; i++) if (R[i].mount && (!strcmp(R[i].os, "linux") || !strcmp(R[i].os, "any")) && !strcmp(os, "linux")) want = 1;
            if (!want || !mount_script || access(mount_script, R_OK) != 0) break;
            run_mount(mount_script);
        }
        const char *e = getenv("BIBLE_ASSET_ROOT");
        if (e && *e) {
            char p[PB];
            if (is_bible(e)) snprintf(found, sizeof found, "%s", e);
            else if (g_bibledir[0] && fit(p, sizeof p, "%s/%s", e, g_bibledir) && is_bible(p)) snprintf(found, sizeof found, "%s", p);
            else if (pass == 0) { strncat(tried, "  $BIBLE_ASSET_ROOT=", sizeof tried - strlen(tried) - 1); strncat(tried, e, sizeof tried - strlen(tried) - 1); strncat(tried, "\n", sizeof tried - strlen(tried) - 1); }
        }
        e = getenv("BOOK_SHARE_ROOT");
        if (!found[0] && e && *e && g_bibledir[0]) {
            char p[PB];
            if (fit(p, sizeof p, "%s/%s", e, g_bibledir) && is_bible(p)) snprintf(found, sizeof found, "%s", p);
            else if (pass == 0) { strncat(tried, "  $BOOK_SHARE_ROOT=", sizeof tried - strlen(tried) - 1); strncat(tried, e, sizeof tried - strlen(tried) - 1); strncat(tried, "\n", sizeof tried - strlen(tried) - 1); }
        }
        for (int i = 0; i < nR && !found[0]; i++) {
            if (strcmp(R[i].os, "any") && strcmp(R[i].os, os)) continue;
            if (is_bible(R[i].path)) snprintf(found, sizeof found, "%s", R[i].path);
            else if (pass == 0) { strncat(tried, "  ", sizeof tried - strlen(tried) - 1); strncat(tried, R[i].path, sizeof tried - strlen(tried) - 1); strncat(tried, R[i].mount ? "  (needs the shared drive mounted)\n" : "\n", sizeof tried - strlen(tried) - 1); }
        }
    }
    if (!found[0]) { fprintf(stderr, "asset_root_op: no book assets found (looked for %s under each of):\n%s", g_probe, tried[0] ? tried : "  (no candidates for this OS)\n"); return 1; }
    if (!strcmp(argv[2], "share")) { char *sl = strrchr(found, '/'); if (sl && sl != found) *sl = 0; }
    printf("%s\n", found);
    return 0;
}
