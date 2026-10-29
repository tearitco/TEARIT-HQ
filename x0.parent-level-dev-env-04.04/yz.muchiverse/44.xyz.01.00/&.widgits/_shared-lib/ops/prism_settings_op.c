/* prism_settings_op - one step of the settings > theme > Custom controls (owner 2026-10-28): changes ONE of the four prism variables (or the Custom on/off switch)
 * in <house>/#.desktop/livedesk_prism.pdl and re-renders the prism with the sibling op prism_op.+x into <house>/#.desktop/prism_theme.{rgba,png}.
 *   prism_settings_op <house> <TOKEN>
 *   TOKEN: PRISM_SHAPE_PREV|NEXT (0-3, wraps)  PRISM_ALGO_PREV|NEXT (0-3, wraps)  PRISM_HUE_MINUS|PLUS (step 30, wraps 0-359)  PRISM_LIGHT_MINUS|PLUS (step 10, clamps 0-100)
 *          PRISM_TOGGLE (Custom on/off)  PRISM_RESET (default rainbow: shape 1, algo 2, hue 0, light 60; Custom unchanged)  PRISM_SHOW (change nothing, re-render)
 * File format (same pdl shape as livedesk_theme.pdl):  SECTION | KEY | VALUE / ---- / PRISM | shape | 1 / PRISM | algo | 2 / PRISM | hue | 0 / PRISM | light | 60 / PRISM | custom | 0
 * A missing or unreadable file means the defaults. Output: `prism_settings: shape=<s> algo=<a> hue=<h> light=<l> custom=<c>`. It does NOT change the live window colours (that is the renderer's job).
 * Exit: 0 ok | 1 could not write | 2 usage / unknown token.   Build: sh build_prism_settings_op.sh (needs prism_op.+x next to it) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>

static int V[5] = {1, 2, 0, 60, 0};      /* shape algo hue light custom */
static const char *K[5] = {"shape", "algo", "hue", "light", "custom"};
static int is_dir(const char *p) { struct stat s; return stat(p, &s) == 0 && S_ISDIR(s.st_mode); }
static void load(const char *path) {
    FILE *f = fopen(path, "r"); char l[256]; if (!f) { return; }
    while (fgets(l, sizeof l, f)) {
        char *a = strchr(l, '|'); if (!a || strncmp(l, "PRISM", 5)) { continue; } a++; while (*a == ' ') { a++; }
        char *b = strchr(a, '|'); if (!b) { continue; } char *e = b - 1; while (e > a && *e == ' ') { *e-- = 0; } *b++ = 0; while (*b == ' ') { b++; }
        for (int i = 0; i < 5; i++) { if (!strcmp(a, K[i])) { V[i] = atoi(b); } }
    }
    fclose(f);
    int mx[5] = {3, 3, 359, 100, 1};
    for (int i = 0; i < 5; i++) { if (V[i] < 0 || V[i] > mx[i]) { V[i] = i == 0 ? 1 : i == 1 ? 2 : i == 3 ? 60 : 0; } }
}
static int save(const char *path) {      /* path is at most PATH_MAX + 64 */
    char t[PATH_MAX + 80]; snprintf(t, sizeof t, "%s.tmp", path); FILE *f = fopen(t, "w"); if (!f) { return 0; }
    fprintf(f, "SECTION      | KEY                | VALUE\n----------------------------------------\n");
    for (int i = 0; i < 5; i++) { fprintf(f, "PRISM        | %-18s | %d\n", K[i], V[i]); }
    int ok = fflush(f) == 0; if (fclose(f)) { ok = 0; } if (ok && rename(t, path) != 0) { ok = 0; } if (!ok) { remove(t); } return ok;
}
static int render(const char *house) {
    char self[PATH_MAX]; ssize_t n = readlink("/proc/self/exe", self, sizeof self - 1); if (n <= 0) { return 0; } self[n] = 0; char *sl = strrchr(self, '/'); if (!sl) { return 0; } strcpy(sl + 1, "prism_op.+x");
    char out[PATH_MAX + 32], s[8], a[8], h[8], l[8]; snprintf(out, sizeof out, "%.3000s/#.desktop/prism_theme", house);
    snprintf(s, sizeof s, "%d", V[0]); snprintf(a, sizeof a, "%d", V[1]); snprintf(h, sizeof h, "%d", V[2]); snprintf(l, sizeof l, "%d", V[3]);
    pid_t pid = fork(); if (pid < 0) { return 0; }
    if (pid == 0) { int dn = open("/dev/null", O_WRONLY); if (dn >= 0) { dup2(dn, 1); } execl(self, self, out, s, a, h, l, (char *)NULL); _exit(127); }
    int st = 0; waitpid(pid, &st, 0); return WIFEXITED(st) && WEXITSTATUS(st) == 0;
}
int main(int argc, char **argv) {
    if (argc != 3 || !is_dir(argv[1])) { fprintf(stderr, "Usage: prism_settings_op <house> <PRISM_TOKEN>\n"); return 2; }
    const char *t = argv[2]; char path[PATH_MAX + 64]; snprintf(path, sizeof path, "%.3000s/#.desktop/livedesk_prism.pdl", argv[1]); load(path);
    int dirty = 1;
    if (!strcmp(t, "PRISM_SHAPE_NEXT")) { V[0] = (V[0] + 1) % 4; } else if (!strcmp(t, "PRISM_SHAPE_PREV")) { V[0] = (V[0] + 3) % 4; }
    else if (!strcmp(t, "PRISM_ALGO_NEXT")) { V[1] = (V[1] + 1) % 4; } else if (!strcmp(t, "PRISM_ALGO_PREV")) { V[1] = (V[1] + 3) % 4; }
    else if (!strcmp(t, "PRISM_HUE_PLUS")) { V[2] = (V[2] + 30) % 360; } else if (!strcmp(t, "PRISM_HUE_MINUS")) { V[2] = (V[2] + 330) % 360; }
    else if (!strcmp(t, "PRISM_LIGHT_PLUS")) { V[3] = V[3] + 10 > 100 ? 100 : V[3] + 10; } else if (!strcmp(t, "PRISM_LIGHT_MINUS")) { V[3] = V[3] - 10 < 0 ? 0 : V[3] - 10; }
    else if (!strcmp(t, "PRISM_TOGGLE")) { V[4] = !V[4]; }
    else if (!strcmp(t, "PRISM_RESET")) { V[0] = 1; V[1] = 2; V[2] = 0; V[3] = 60; }
    else if (!strcmp(t, "PRISM_SHOW")) { dirty = 0; }
    else { fprintf(stderr, "prism_settings_op: unknown token\n"); return 2; }
    if (dirty && !save(path)) { return 1; }
    if (!render(argv[1])) { return 1; }
    printf("prism_settings: shape=%d algo=%d hue=%d light=%d custom=%d\n", V[0], V[1], V[2], V[3], V[4]);
    return 0;
}
