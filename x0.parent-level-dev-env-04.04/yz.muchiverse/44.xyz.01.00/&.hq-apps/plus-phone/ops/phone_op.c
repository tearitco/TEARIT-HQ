/* phone_op.c - the ONE writer of the plus phone (design: BOOK 16.game/PLUS-PHONE-DESIGN-2026-11-15.md; first slice 2026-11-15, owner: "basics nothing crazy").
 *   phone_op new   <house>              start a new phone: next free number, a free port, a window (one process group, registered through the spawn funnel)
 *   phone_op list  <house>              one line per phone: n port orient running|stopped
 *   phone_op close <house> <n>          end that phone's window by exact pid and remove its folder
 *   phone_op flip  <instance>           portrait <-> landscape: writes ORIENT and relaunches the window with the other layout (a detached worker, so the button's own process is not killed with its window)
 *   phone_op relaunch <instance>        (internal worker of flip)
 *   phone_op open  <instance> <app>     a home tile was picked (this slice: records a result line only; the apps are not wired yet)
 *   phone_op home  <instance>           clear the result line
 *   phone_op closeself <instance>       the chrome's close button
 * An instance is <house>/&.hq-apps/plus-phone/instances/<n>/ : phone.txt (config: PHONE, PORT, ORIENT, STARTED), window.pid, .hq_manager/{ui.txt (projection), marker.txt (append-only wake-up), last.txt}.
 * No game state lives here (no-ghost-data rule). At most 4 phones. Build: gcc -std=gnu11 -Wall -O2 -o ops/+x/phone_op.+x ops/phone_op.c */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>
#include <time.h>
#include <dirent.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#define P 4096
#define MAXPHONES 4
#define PORT_LO 41000
#define PORT_HI 41999
static char HOUSE[P];
static const char *MARK = "/&.hq-apps/plus-phone/instances/";

static void wr(const char *path, const char *text) { char t[P + 8]; snprintf(t, sizeof t, "%s.tmp", path); FILE *w = fopen(t, "w"); if (!w) return; fputs(text, w); fclose(w); rename(t, path); }
static void bump(const char *inst) { char f[P]; snprintf(f, sizeof f, "%s/.hq_manager/marker.txt", inst); struct stat sb; if (!stat(f, &sb) && sb.st_size > 4096) unlink(f); FILE *m = fopen(f, "a"); if (m) { fputc('x', m); fclose(m); } }      /* capped at 4 KB */
static int house_of(const char *inst) { const char *at = strstr(inst, MARK); if (!at || (size_t)(at - inst) >= sizeof HOUSE) return 0; snprintf(HOUSE, sizeof HOUSE, "%.*s", (int)(at - inst), inst); return 1; }
static int kv(const char *inst, const char *key, char *out, size_t cap) {      /* PHONE | value rows of phone.txt */
    char f[P]; snprintf(f, sizeof f, "%s/phone.txt", inst); FILE *fp = fopen(f, "r"); out[0] = 0; if (!fp) return 0; char ln[300]; int hit = 0; size_t kl = strlen(key);
    while (fgets(ln, sizeof ln, fp)) { if (strncmp(ln, key, kl) || strncmp(ln + kl, " | ", 3)) continue; snprintf(out, cap, "%s", ln + kl + 3); out[strcspn(out, "\r\n")] = 0; hit = 1; break; } fclose(fp); return hit; }
static long pid_of(const char *inst) { char f[P], b[32] = ""; snprintf(f, sizeof f, "%s/window.pid", inst); FILE *fp = fopen(f, "r"); if (!fp) return 0; if (fgets(b, sizeof b, fp)) {} fclose(fp); return atol(b); }
static int is_window(long pid) { if (pid <= 1 || kill((pid_t)pid, 0)) return 0; char f[64], c[600] = ""; snprintf(f, sizeof f, "/proc/%ld/cmdline", pid); FILE *fp = fopen(f, "r"); if (!fp) return 0; size_t n = fread(c, 1, sizeof c - 1, fp); fclose(fp); for (size_t i = 0; i + 1 < n; i++) if (!c[i]) c[i] = ' '; return strstr(c, "khtpm_core_render") && strstr(c, "plus-phone") ? 1 : 0; }
static int run_out(char *const av[], const char *outf) { pid_t c = fork(); if (c < 0) return -1; if (c == 0) { int fd = open(outf, O_WRONLY | O_CREAT | O_TRUNC, 0644); if (fd >= 0) dup2(fd, 1); int nul = open("/dev/null", O_RDWR); if (nul >= 0) { dup2(nul, 2); dup2(nul, 0); } execv(av[0], av); _exit(127); }
    for (int i = 0; i < 200; i++) { int st; if (waitpid(c, &st, WNOHANG) == c) return WIFEXITED(st) ? WEXITSTATUS(st) : -1; usleep(100000); } kill(c, SIGKILL); waitpid(c, NULL, 0); return -1; }
static int port_free(int port) { int s = socket(AF_INET, SOCK_STREAM, 0); if (s < 0) return 0; struct sockaddr_in a; memset(&a, 0, sizeof a); a.sin_family = AF_INET; a.sin_port = htons((unsigned short)port); a.sin_addr.s_addr = htonl(INADDR_LOOPBACK); int ok = bind(s, (struct sockaddr *)&a, sizeof a) == 0; close(s); return ok; }
static int used_port(int port) { char d[P]; snprintf(d, sizeof d, "%s/&.hq-apps/plus-phone/instances", HOUSE); DIR *dp = opendir(d); if (!dp) return 0; struct dirent *e; int hit = 0; while ((e = readdir(dp))) { if (e->d_name[0] == '.') continue; char in[P], v[16]; snprintf(in, sizeof in, "%s/%s", d, e->d_name); if (kv(in, "PORT", v, sizeof v) && atoi(v) == port) hit = 1; } closedir(dp); return hit; }

/* start (or restart) the window of an instance with the layout for its orientation; prints nothing, returns the pid or 0 */
static long launch(const char *inst) {
    char orient[16]; kv(inst, "ORIENT", orient, sizeof orient); const char *lay = !strcmp(orient, "landscape") ? "plus-phone-landscape.xhtpm" : "plus-phone.xhtpm";
    char x[P], exe[P], funnel[P], tmp[P], nm[48], n[16]; snprintf(x, sizeof x, "%s/&.hq-apps/plus-phone/%s", HOUSE, lay); snprintf(exe, sizeof exe, "%s/_.monads/_.livedesk-taskbar/ops/+x/khtpm_core_render.+x", HOUSE); snprintf(funnel, sizeof funnel, "%s/&.widgits/_shared-lib/kh_spawn.sh", HOUSE);
    if (access(x, R_OK) || access(exe, X_OK) || access(funnel, R_OK)) return 0;
    kv(inst, "PHONE", n, sizeof n); snprintf(nm, sizeof nm, "plus-phone-%s", n); snprintf(tmp, sizeof tmp, "%s/spawn.out", inst); setenv("KH_ROLE", "tool", 1); setenv("KH_APP", "plus-phone", 1);
    char *av[] = { "/bin/sh", funnel, HOUSE, "0", nm, "--", exe, HOUSE, x, (char *)inst, NULL }; run_out(av, tmp);
    FILE *tp = fopen(tmp, "r"); char out[64] = ""; if (tp) { if (fgets(out, sizeof out, tp)) {} fclose(tp); } unlink(tmp); long pid = atol(out); if (pid <= 1) return 0;
    char pf[P], num[32]; snprintf(pf, sizeof pf, "%s/window.pid", inst); snprintf(num, sizeof num, "%ld\n", pid); wr(pf, num); return pid;
}
static int count_running(void) { char d[P]; snprintf(d, sizeof d, "%s/&.hq-apps/plus-phone/instances", HOUSE); DIR *dp = opendir(d); if (!dp) return 0; struct dirent *e; int n = 0; while ((e = readdir(dp))) { if (e->d_name[0] == '.') continue; char in[P]; snprintf(in, sizeof in, "%s/%s", d, e->d_name); if (is_window(pid_of(in))) n++; } closedir(dp); return n; }

static int v_new(void) {
    if (count_running() >= MAXPHONES) { printf("PHONE-REFUSED at most %d phones are open\n", MAXPHONES); return 1; }
    char d[P]; snprintf(d, sizeof d, "%s/&.hq-apps/plus-phone/instances", HOUSE); mkdir(d, 0755);
    int num = 0; for (int i = 1; i < 100; i++) { char in[P]; snprintf(in, sizeof in, "%s/%d", d, i); if (access(in, F_OK)) { num = i; break; } if (!is_window(pid_of(in))) { /* a dead phone's folder: reuse it, clean */ char cmd[P + 16]; (void)cmd; num = i; break; } }
    if (!num) { printf("PHONE-REFUSED no free phone number\n"); return 1; }
    int port = 0; for (int p = PORT_LO; p <= PORT_HI; p++) if (!used_port(p) && port_free(p)) { port = p; break; }
    if (!port) { printf("PHONE-REFUSED no free port in %d-%d\n", PORT_LO, PORT_HI); return 1; }
    char in[P], hq[P], f[P], txt[400]; snprintf(in, sizeof in, "%s/%d", d, num); snprintf(hq, sizeof hq, "%s/.hq_manager", in); mkdir(in, 0755); mkdir(hq, 0755);
    const char *junk[] = { "ui.txt", "last.txt", "marker.txt", "window.pid" }; for (int i = 0; i < 4; i++) { snprintf(f, sizeof f, "%s/.hq_manager/%s", in, junk[i]); unlink(f); } snprintf(f, sizeof f, "%s/window.pid", in); unlink(f);
    snprintf(txt, sizeof txt, "# phone.txt - CONFIG of one plus phone, written by phone_op. No game state lives here.\nPHONE | %d\nPORT | %d\nORIENT | portrait\nSTARTED | %ld\n", num, port, (long)time(NULL)); snprintf(f, sizeof f, "%s/phone.txt", in); wr(f, txt); bump(in);
    long pid = launch(in); if (!pid) { printf("PHONE-FAILED could not start the window (layout, renderer or spawn funnel missing)\n"); return 1; }
    printf("PHONE-OPENED %d port=%d pid=%ld\n", num, port, pid); return 0;
}
static int v_list(void) { char d[P]; snprintf(d, sizeof d, "%s/&.hq-apps/plus-phone/instances", HOUSE); DIR *dp = opendir(d); if (!dp) { printf("PHONES 0\n"); return 0; } struct dirent *e; int n = 0;
    while ((e = readdir(dp))) { if (e->d_name[0] == '.') continue; char in[P], po[16] = "", or_[16] = ""; snprintf(in, sizeof in, "%s/%s", d, e->d_name); kv(in, "PORT", po, sizeof po); kv(in, "ORIENT", or_, sizeof or_); printf("%s %s %s %s\n", e->d_name, po, or_, is_window(pid_of(in)) ? "running" : "stopped"); n++; } closedir(dp); printf("PHONES %d\n", n); return 0; }
static void rmtree(const char *p) { struct stat sb; if (lstat(p, &sb)) return; if (S_ISDIR(sb.st_mode)) { DIR *dp = opendir(p); if (dp) { struct dirent *e; while ((e = readdir(dp))) { if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue; char c[P]; snprintf(c, sizeof c, "%s/%s", p, e->d_name); rmtree(c); } closedir(dp); } rmdir(p); } else unlink(p); }
static void kill_window(const char *inst) { long pid = pid_of(inst); if (is_window(pid)) { kill((pid_t)pid, SIGTERM); for (int i = 0; i < 30 && !kill((pid_t)pid, 0); i++) usleep(100000); } }
static int v_close(const char *n) { for (const char *c = n; *c; c++) if (!isdigit((unsigned char)*c)) { printf("PHONE-REFUSED bad number\n"); return 2; } char in[P]; snprintf(in, sizeof in, "%s/&.hq-apps/plus-phone/instances/%s", HOUSE, n); if (access(in, F_OK)) { printf("PHONE-REFUSED no phone %s\n", n); return 1; } kill_window(in); rmtree(in); printf("PHONE-CLOSED %s\n", n); return 0; }
static void spawn_self(const char *verb, const char *inst) { char self[P]; ssize_t k = readlink("/proc/self/exe", self, sizeof self - 1); if (k <= 0) return; self[k] = 0; char funnel[P]; snprintf(funnel, sizeof funnel, "%s/&.widgits/_shared-lib/kh_spawn.sh", HOUSE);
    setenv("KH_ROLE", "tool", 1); setenv("KH_APP", "plus-phone", 1); setenv("KH_LIFETIME", "keep", 1); pid_t c = fork(); if (c == 0) { int nul = open("/dev/null", O_RDWR); if (nul >= 0) { dup2(nul, 0); dup2(nul, 1); dup2(nul, 2); } char *av[] = { "sh", funnel, HOUSE, "0", "plus-phone-act", "--", self, (char *)verb, (char *)inst, NULL }; execv("/bin/sh", av); _exit(127); } if (c > 0) waitpid(c, NULL, 0); }
static int v_flip(const char *inst) { char o[16]; kv(inst, "ORIENT", o, sizeof o); if (!o[0]) return 1; char f[P], txt[600], num[16] = "", port[16] = "", st[24] = ""; kv(inst, "PHONE", num, sizeof num); kv(inst, "PORT", port, sizeof port); kv(inst, "STARTED", st, sizeof st);
    snprintf(txt, sizeof txt, "# phone.txt - CONFIG of one plus phone, written by phone_op. No game state lives here.\nPHONE | %s\nPORT | %s\nORIENT | %s\nSTARTED | %s\n", num, port, !strcmp(o, "portrait") ? "landscape" : "portrait", st); snprintf(f, sizeof f, "%s/phone.txt", inst); wr(f, txt); bump(inst); spawn_self("relaunch", inst); return 0; }
static int v_relaunch(const char *inst) { usleep(300000); kill_window(inst); long pid = launch(inst); return pid ? 0 : 1; }
static int v_open(const char *inst, const char *app) { for (const char *c = app; *c; c++) if (!(isalnum((unsigned char)*c) || *c == '-' || *c == '_')) return 2; char f[P], txt[200]; snprintf(f, sizeof f, "%s/.hq_manager/last.txt", inst);
    snprintf(txt, sizeof txt, "%s: not wired yet (build step 5). Nothing was started.\n", app); wr(f, txt); bump(inst); return 0; }
static int v_home(const char *inst) { char f[P]; snprintf(f, sizeof f, "%s/.hq_manager/last.txt", inst); unlink(f); bump(inst); return 0; }

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: phone_op new|list <house> | close <house> <n> | flip|relaunch|home|closeself <instance> | open <instance> <app>\n"); return 2; }
    const char *v = argv[1];
    if (!strcmp(v, "new") || !strcmp(v, "list") || !strcmp(v, "close")) { snprintf(HOUSE, sizeof HOUSE, "%s", argv[2]); if (!strcmp(v, "new")) return v_new(); if (!strcmp(v, "list")) return v_list(); if (argc < 4) return 2; return v_close(argv[3]); }
    if (!house_of(argv[2])) { fprintf(stderr, "phone_op: not a plus-phone instance: %s\n", argv[2]); return 2; }
    if (!strcmp(v, "flip")) return v_flip(argv[2]); if (!strcmp(v, "relaunch")) return v_relaunch(argv[2]); if (!strcmp(v, "home")) return v_home(argv[2]);
    if (!strcmp(v, "open") && argc >= 4) return v_open(argv[2], argv[3]);
    if (!strcmp(v, "closeself")) { const char *at = strstr(argv[2], MARK); return v_close(at + strlen(MARK)); }
    fprintf(stderr, "phone_op: unknown verb %s\n", v); return 2;
}
