/* relay_drive - drive a khtpm_core_render window like a human through its per-process input relay (#.desktop/entity_menu_history/<pid>.txt).
 * Usage: relay_drive <pid> key <name|char>        one key (enter esc bs tab up down left right pgup pgdn, or one printable character)
 *        relay_drive <pid> type <text>            each character as a key press, paced like typing
 *        relay_drive <pid> click <x> <y> [button] press + release at window pixel x,y (the house is two-step: the first click focuses a row, the second activates)
 *        relay_drive <pid> tap <x> <y>            focus click then activate click, as a person clicking a button
 *        relay_drive <pid> press <id|label:text>  find the element in a fresh frame dump and press it by its on-screen number (Esc, digits, Enter)
 *        relay_drive <pid> tapid <id|label:text>  same, but two mouse clicks at the middle of the element
 *        relay_drive <pid> comment <text>         a '#' audit line (no-op for the window)
 *        relay_drive findwin <w> <h>              prints the X window id of the one unnamed window of that size (+-8 px), exit 1 if not exactly one
 *        relay_drive shot <winid> <out.png>       dump_frame_png_op on that window
 *        relay_drive <pid> run <cases.pdl> [winid]  run a case file; prints PASS/FAIL per step and a summary; exit 1 on any FAIL
 * Case rows (pipe separated, '#' comments):  STEP | key | <name> ; STEP | type | <text> ; STEP | click | x y ; STEP | tap | x y ; STEP | press | id or label:text ; STEP | tapid | id or label:text ; STEP | sleep | ms ;
 *   STEP | expect | <file> | contains|absent | <text> | <timeout_s> ; STEP | shot | <out.png>
 * Relative file paths in cases resolve against the house root. Keys use the house relay format KEY_PRESSED: <decimal>. Never truncates the relay file. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>
#define PL 4096
static char g_house[PL];
static void ms(int n) { usleep((useconds_t)n * 1000); }
static void house_from_exe(void) {
    char exe[PL]; ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1); if (n < 0) { g_house[0] = 0; return; } exe[n] = 0;
    for (int i = 0; i < 5; i++) { char *s = strrchr(exe, '/'); if (!s) break; *s = 0; }   /* house/&.widgits/_shared-lib/ops/+x/relay_drive.+x */
    snprintf(g_house, sizeof g_house, "%s", exe);
}
static void relay_line(long pid, const char *line) {
    char p[PL]; snprintf(p, sizeof p, "%s/#.desktop/entity_menu_history/%ld.txt", g_house, pid);
    FILE *f = fopen(p, "a"); if (!f) { perror(p); exit(2); } fprintf(f, "%s\n", line); fclose(f);
}
static int keycode(const char *k) {
    static const struct { const char *n; int c; } T[] = { { "enter", 13 }, { "esc", 27 }, { "bs", 8 }, { "tab", 9 }, { "up", 200 }, { "down", 201 }, { "left", 202 }, { "right", 203 }, { "pgup", 204 }, { "pgdn", 205 }, { "space", 32 } };
    for (unsigned i = 0; i < sizeof T / sizeof T[0]; i++) if (!strcmp(k, T[i].n)) return T[i].c;
    if (strlen(k) == 1 && (unsigned char)k[0] >= 32 && (unsigned char)k[0] < 127) return (unsigned char)k[0];
    return -1;
}
static void send_key(long pid, int code) { char b[64]; snprintf(b, sizeof b, "KEY_PRESSED: %d", code); relay_line(pid, b); }
static void send_click(long pid, int x, int y, int btn) { char b[96]; snprintf(b, sizeof b, "MOUSE_EVENT: %d %d %d 1", btn, x, y); relay_line(pid, b); ms(90); snprintf(b, sizeof b, "MOUSE_EVENT: %d %d %d 0", btn, x, y); relay_line(pid, b); }
static int runcmd(char *const argv[], const char *outfile) {
    pid_t p = fork(); if (p < 0) return -1;
    if (p == 0) { if (outfile) { int fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644); if (fd >= 0) { dup2(fd, 1); dup2(fd, 2); close(fd); } } execvp(argv[0], argv); _exit(127); }
    for (int t = 0; t < 300; t++) { int st; if (waitpid(p, &st, WNOHANG) == p) return WIFEXITED(st) ? WEXITSTATUS(st) : -1; ms(100); }
    kill(p, SIGKILL); waitpid(p, NULL, 0); return -1;
}
static int findwin(int w, int h, char *out) {
    char tmp[PL]; snprintf(tmp, sizeof tmp, "/tmp/relay_drive_tree_%d.txt", (int)getpid()); char *a[] = { "xwininfo", "-root", "-tree", NULL }; if (runcmd(a, tmp) != 0) return 0;
    FILE *f = fopen(tmp, "r"); if (!f) return 0; char ln[512]; int n = 0; while (fgets(ln, sizeof ln, f)) { if (!strstr(ln, "has no name")) continue; char id[32]; int ww, hh;
        const char *par = strstr(ln, "()"); if (par && sscanf(ln, " %31s", id) == 1 && sscanf(par + 2, " %dx%d", &ww, &hh) == 2 && abs(ww - w) <= 8 && abs(hh - h) <= 8) { snprintf(out, 32, "%s", id); n++; } }
    fclose(f); unlink(tmp); return n == 1;
}
static int shot(const char *win, const char *png) {
    char op[PL]; snprintf(op, sizeof op, "%s/&.widgits/_shared-lib/ops/+x/dump_frame_png_op.+x", g_house); char *a[] = { op, (char *)win, (char *)png, NULL }; return runcmd(a, "/dev/null");
}
static void abspath(const char *p, char *out) { if (p[0] == '/') snprintf(out, PL, "%s", p); else snprintf(out, PL, "%s/%s", g_house, p); }
static char *slurp(const char *p) { FILE *f = fopen(p, "rb"); if (!f) return NULL; fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET); char *b = malloc(n + 1); if (!b) { fclose(f); return NULL; } if (fread(b, 1, n, f) != (size_t)n) n = 0; b[n] = 0; fclose(f); return b; }
static int split(char *s, char **f, int max) { int n = 0; f[n++] = s; for (char *p = s; *p && n < max; p++) if (*p == '|') { *p = 0; f[n++] = p + 1; } for (int i = 0; i < n; i++) { while (*f[i] == ' ') f[i]++; char *e = f[i] + strlen(f[i]); while (e > f[i] && (e[-1] == ' ' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; } return n; }
/* ---- find an element by id or label (so tests do not depend on numbers that shift) ----
 * The window writes /tmp/entity-menu-frame.png.frame.txt when it gets the key p while no field is armed (so Esc first). One line per element:
 *   tag|id|class|label|?|action|nav|?|x|y|w|h|...   nav 0 = not reachable (off screen or not numbered). The path is shared by every khtpm window: use it one window at a time. */
typedef struct { int nav, x, y, w, h; char id[80], label[200]; } Elem1;
static int frame_dump(long pid, const char *frame) {
    struct stat st; long long before = 0; if (!stat(frame, &st)) before = (long long)st.st_mtim.tv_sec * 1000000000LL + st.st_mtim.tv_nsec;
    send_key(pid, 27); ms(300); send_key(pid, 'p');
    for (int t = 0; t < 40; t++) { ms(150); if (!stat(frame, &st)) { long long now = (long long)st.st_mtim.tv_sec * 1000000000LL + st.st_mtim.tv_nsec; if (now > before) { ms(200); return 1; } } }
    return 0;
}
/* key: "label:<text>" matches the first element whose label contains text; anything else matches the element id exactly. Returns 1 and fills *e. */
static int find_elem(const char *frame, const char *key, Elem1 *e) {
    FILE *f = fopen(frame, "r"); if (!f) return 0; char ln[4096]; int found = 0; int bylabel = !strncmp(key, "label:", 6); const char *want = bylabel ? key + 6 : key;
    while (!found && fgets(ln, sizeof ln, f)) { char *fld[16]; int n = 0; char *p = ln; fld[n++] = p; while (*p && n < 16) { if (*p == '|') { *p = 0; fld[n++] = p + 1; } p++; } if (n < 12) continue;
        int hit = bylabel ? (strstr(fld[3], want) != NULL) : (strcmp(fld[1], want) == 0); if (!hit) continue;
        memset(e, 0, sizeof *e); snprintf(e->id, sizeof e->id, "%s", fld[1]); snprintf(e->label, sizeof e->label, "%s", fld[3]); e->nav = atoi(fld[6]); e->x = atoi(fld[8]); e->y = atoi(fld[9]); e->w = atoi(fld[10]); e->h = atoi(fld[11]); found = 1; }
    fclose(f); return found;
}
static int press_elem(long pid, const char *key, int by_mouse, char *what, size_t cap) {
    const char *frame = "/tmp/entity-menu-frame.png.frame.txt"; Elem1 e;
    if (!frame_dump(pid, frame)) { snprintf(what, cap, "no frame dump after key p (is a field armed, or is the window not responding?)"); return 0; }
    if (!find_elem(frame, key, &e)) { snprintf(what, cap, "no element %s in the frame", key); return 0; }
    if (by_mouse) { if (e.w <= 0 || e.h <= 0) { snprintf(what, cap, "element %s has no size", key); return 0; } int cx = e.x + e.w / 2, cy = e.y + e.h / 2; send_click(pid, cx, cy, 1); ms(350); send_click(pid, cx, cy, 1); ms(500); snprintf(what, cap, "tapid %s (%d,%d)", key, cx, cy); return 1; }
    if (e.nav <= 0) { snprintf(what, cap, "element %s is not numbered (off screen?)", key); return 0; }
    char num[16]; snprintf(num, sizeof num, "%d", e.nav); send_key(pid, 27); ms(250); for (const char *c = num; *c; c++) { send_key(pid, (unsigned char)*c); ms(70); } ms(250); send_key(pid, 13); ms(500); snprintf(what, cap, "press %s (nav %d)", key, e.nav); return 1;
}
static int run_cases(long pid, const char *cases, const char *win) {
    FILE *f = fopen(cases, "r"); if (!f) { perror(cases); return 2; } char ln[1024], *fd[8]; int no = 0, pass = 0, fail = 0;
    while (fgets(ln, sizeof ln, f)) { no++; if (ln[0] == '#' || ln[0] == '\n' || strncmp(ln, "STEP", 4)) continue; int n = split(ln, fd, 8); if (n < 3) continue; const char *verb = fd[1]; int ok = 1; char what[300] = "";
        if (!strcmp(verb, "key")) { int c = keycode(fd[2]); if (c < 0) ok = 0; else { send_key(pid, c); ms(350); } snprintf(what, sizeof what, "key %s", fd[2]); }
        else if (!strcmp(verb, "type")) { for (const char *c = fd[2]; *c; c++) { send_key(pid, (unsigned char)*c); ms(70); } ms(300); snprintf(what, sizeof what, "type \"%s\"", fd[2]); }
        else if (!strcmp(verb, "click")) { int x = 0, y = 0; if (sscanf(fd[2], "%d %d", &x, &y) != 2) ok = 0; else { send_click(pid, x, y, 1); ms(400); } snprintf(what, sizeof what, "click %s", fd[2]); }
        else if (!strcmp(verb, "tap")) { int x = 0, y = 0; if (sscanf(fd[2], "%d %d", &x, &y) != 2) ok = 0; else { send_click(pid, x, y, 1); ms(350); send_click(pid, x, y, 1); ms(500); } snprintf(what, sizeof what, "tap %s (focus click, then activate click)", fd[2]); }
        else if (!strcmp(verb, "press") || !strcmp(verb, "tapid")) { ok = press_elem(pid, fd[2], !strcmp(verb, "tapid"), what, sizeof what); }
        else if (!strcmp(verb, "sleep")) { ms(atoi(fd[2])); snprintf(what, sizeof what, "sleep %s", fd[2]); }
        else if (!strcmp(verb, "shot")) { if (!win) ok = 0; else { char p[PL]; abspath(fd[2], p); ok = shot(win, p) == 0; } snprintf(what, sizeof what, "shot %s", fd[2]); }
        else if (!strcmp(verb, "expect") && n >= 5) { char p[PL]; abspath(fd[2], p); int to = n >= 6 ? atoi(fd[5]) : 5; int want = !strcmp(fd[3], "contains"); ok = 0;
            for (int t = 0; t <= to * 4; t++) { char *b = slurp(p); int has = b && strstr(b, fd[4]); free(b); if (has == want) { ok = 1; break; } ms(250); }
            snprintf(what, sizeof what, "expect %s %s \"%s\"", fd[2], fd[3], fd[4]); }
        else { ok = 0; snprintf(what, sizeof what, "unknown step %s", verb); }
        printf("%s line %d: %s\n", ok ? "PASS" : "FAIL", no, what); if (ok) pass++; else fail++; fflush(stdout); }
    fclose(f); printf("%s: %d pass, %d fail\n", fail ? "FAILED" : "ALL PASS", pass, fail); return fail ? 1 : 0;
}
int main(int argc, char **argv) {
    house_from_exe();
    if (argc >= 4 && !strcmp(argv[1], "findwin")) { char id[32]; if (findwin(atoi(argv[2]), atoi(argv[3]), id)) { puts(id); return 0; } return 1; }
    if (argc >= 4 && !strcmp(argv[1], "shot")) return shot(argv[2], argv[3]) == 0 ? 0 : 1;
    if (argc < 4) { fprintf(stderr, "usage: relay_drive <pid> key|type|click|comment|run ... | findwin W H | shot WIN OUT\n"); return 2; }
    long pid = atol(argv[1]); const char *v = argv[2];
    if (!strcmp(v, "key")) { int c = keycode(argv[3]); if (c < 0) { fprintf(stderr, "bad key\n"); return 2; } send_key(pid, c); return 0; }
    if (!strcmp(v, "type")) { for (const char *c = argv[3]; *c; c++) { send_key(pid, (unsigned char)*c); ms(70); } return 0; }
    if (!strcmp(v, "click") && argc >= 5) { send_click(pid, atoi(argv[3]), atoi(argv[4]), argc > 5 ? atoi(argv[5]) : 1); return 0; }
    if (!strcmp(v, "tap") && argc >= 5) { send_click(pid, atoi(argv[3]), atoi(argv[4]), 1); ms(350); send_click(pid, atoi(argv[3]), atoi(argv[4]), 1); return 0; }
    if (!strcmp(v, "press") || !strcmp(v, "tapid")) { char w[300]; int ok = press_elem(pid, argv[3], !strcmp(v, "tapid"), w, sizeof w); puts(w); return ok ? 0 : 1; }
    if (!strcmp(v, "comment")) { char b[300]; snprintf(b, sizeof b, "# %s", argv[3]); relay_line(pid, b); return 0; }
    if (!strcmp(v, "run")) { char cp[PL]; abspath(argv[3], cp); return run_cases(pid, cp, argc > 4 ? argv[4] : NULL); }
    return 2;
}
