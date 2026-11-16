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

/* ICON SIZE (2026-11-15, owner: "icons make themselves as big as they can, snap up in size; just give 3 options small, md, large"): three tile sizes, and a phone uses the LARGEST of them (up to the user's cap,
 * phone_prefs.pdl ICON_MAX | small|medium|large, default large) that still leaves at least 2 columns in its window. Portrait 364 wide -> medium (2 columns); landscape 704 wide -> large (3 columns).
 * The shared layout is a TEMPLATE: each launch writes a per-instance copy of the layout and css with the chosen size filled in (and ${PKG} made absolute) and starts THAT (Android resource generation / Gradle idea,
 * BOOK 16.game/PLUS-PHONE-DESIGN-2026-11-15.md section 22.3). The columns formula mirrors the renderer's swatch grid: (window width - 50) / (tile + 8). */
static const int ICON_PX[3] = { 96, 120, 160 };
static const char *ICON_NAME[3] = { "small", "medium", "large" };
static int icon_cap(void) {      /* NOT kv(): that helper reads <dir>/phone.txt (found 2026-11-15: the cap was ignored until the case read the setting back) */
    char f[P], ln[200]; snprintf(f, sizeof f, "%s/&.hq-apps/plus-phone/phone_prefs.pdl", HOUSE); FILE *fp = fopen(f, "r"); if (!fp) return 2;
    while (fgets(ln, sizeof ln, fp)) { if (strncmp(ln, "ICON_MAX | ", 11)) continue; ln[strcspn(ln, "\r\n")] = 0; for (int i = 0; i < 3; i++) if (!strcmp(ln + 11, ICON_NAME[i])) { fclose(fp); return i; } }
    fclose(fp); return 2;
}
static int count_apps(void) { char d[P]; snprintf(d, sizeof d, "%s/&.hq-apps/plus-phone/apps", HOUSE); DIR *dp = opendir(d); if (!dp) return 1; int n = 0; struct dirent *e; while ((e = readdir(dp))) if (e->d_name[0] != '.') n++; closedir(dp); return n < 1 ? 1 : n; }
static int fill_px(int win_w, int ic) { int c = (win_w - 50) / (ICON_PX[ic] + 8); if (c < 1) c = 1; { int n = count_apps(); if (n < c) c = n; }      /* few apps: fewer columns, so they fill the width; more apps than columns: the step size, rows wrap */ int t = (win_w - 50) / c - 9; return t < ICON_PX[ic] ? ICON_PX[ic] : t; }      /* 2026-11-15 owner: "shouldnt the 2 app icons fill full screen size": same column count as the step, tile grown to fill the row (the renderer keeps the same count: (w-50)/(t+8)) */
static int pick_icon(int win_w) { for (int i = icon_cap(); i >= 0; i--) if ((win_w - 50) / (ICON_PX[i] + 8) >= 2) return i; return 0; }
static int slurp(const char *f, char *buf, size_t cap) { FILE *fp = fopen(f, "r"); if (!fp) return 0; size_t n = fread(buf, 1, cap - 1, fp); fclose(fp); buf[n] = 0; return 1; }
static void replace_all(char *buf, size_t cap, const char *from, const char *to) { char *out = malloc(cap); if (!out) return; size_t o = 0, fl = strlen(from), tl = strlen(to); for (char *p = buf; *p && o + tl + 2 < cap; ) { if (!strncmp(p, from, fl)) { memcpy(out + o, to, tl); o += tl; p += fl; } else out[o++] = *p++; } out[o] = 0; snprintf(buf, cap, "%s", out); free(out); }
/* writes <inst>/<lay> and <inst>/<same stem>.css from the templates; returns 1 on success and the icon index in *ic */
/* The phone's BOTTOM bar (PLUS-PHONE-LAYOUT-SPEC, owner 2026-11-16): ONLY names, the open apps now (entities, a later hotbar, none yet); no rotate or leave cells (rotate is in the HQ menu, home in the picker row).
 * Token ${PHONE_FOOTER} in a layout is replaced by this footer: one cell per open app (today the one named in screen.txt, its manifest TITLE), an empty bar when none is open. */
static int is_word(const char *s);
/* ---- ENTITIES on the phone (owner 2026-11-16; spec: BOOK 16.game/PLUS-PHONE-CURSWORD-ENTITY-IMPLEMENTATION-SPEC-2026-11-16.md). An entity (cursword first) is the SAME process the desktop runs; the phone is another VIEW of it:
 * a cell in the phone bottom bar (names only) and, when tapped, an ENTITY SCREEN in the same window: the rows of the entity's own menu.chtpm as buttons. Which entities: `ENTITY | <name>` rows of phone_prefs.pdl (default: cursword).
 * Where the entity lives: the taskbar projection #.desktop/strip_ui.txt (tab_N_label / tab_N_sprite = its folder), accepted ONLY when the folder resolves (realpath) to .../xyzfs/users/<id>/.../pals/<name>; nothing else is read or written there except the single word CLOSE in its interact_relay.txt (what a click on the Close row does on the desktop). ---- */
#include "phone_entity.inc"
static void unesc(char *s) { static const char *from[] = { "&quot;", "&amp;", "&lt;", "&gt;", "&apos;" }; static const char to[] = { '"', '&', '<', '>', '\'' }; char *w = s; while (*s) { int hit = 0; if (*s == '&') for (int i = 0; i < 5; i++) if (!strncmp(s, from[i], strlen(from[i]))) { *w++ = to[i]; s += strlen(from[i]); hit = 1; break; } if (!hit) *w++ = *s++; } *w = 0; }
typedef struct { char label[80]; char action[2400]; } EItem;
static int ent_items(const char *dir, EItem *it, int max) { char mf[P], ln[4096]; snprintf(mf, sizeof mf, "%s/menu.chtpm", dir); FILE *fp = fopen(mf, "r"); int n = 0; while (fp && fgets(ln, sizeof ln, fp) && n < max) { char *p = strstr(ln, "<item "); if (!p) continue; char *la = strstr(p, "label=\""); char *ac = strstr(p, "action=\""); if (!la || !ac) continue;
        la += 7; char *le = strchr(la, '"'); ac += 8; char *ae = strchr(ac, '"'); if (!le || !ae) continue; if ((size_t)(le - la) >= sizeof it[n].label || (size_t)(ae - ac) >= sizeof it[n].action) continue; snprintf(it[n].label, sizeof it[n].label, "%.*s", (int)(le - la), la); snprintf(it[n].action, sizeof it[n].action, "%.*s", (int)(ae - ac), ac); unesc(it[n].label); unesc(it[n].action); n++; }
    if (fp) fclose(fp); return n; }
static const char ENT_CSS[] = "/* structure only: colours follow the Settings 2 colors */\nwindow { font-family: DejaVu Serif; font-size: 11px; }\n.phone-portrait { width: 360px; height: 700px; }\n.btn { border: 1px solid #808080; }\n.dimbtn { border: 1px solid #404040; }\n.tab-bar { background-color: #1a1a1a; border: 1px solid #444444; }\n.tab { background-color: #1a1a1a; color: #aaaaaa; border: 1px solid #444444; padding: 2px 2px; width: 150px; }\n.phone-footer { height: 44px; background-color: #181818; border: 1px solid #2a2a2a; }\n.phone-ft { width: 168px; border: 1px solid #444444; }\n.dd-row { border: 1px solid #444444; background-color: #1a1a1a; width: 200px; }\n.hqwin-minimized { color: #666666; border: 1px dashed #555555; }\n";
/* writes <inst>/entity-screen.xhtpm (+ .css): the entity's menu rows as buttons, the phone's stack around them; out = its path */
static int entity_screen(const char *inst, const char *name, char *out, size_t cap) {
    char dir[P]; if (!ent_listed(name) || !ent_dir(name, dir, sizeof dir)) return 0; static EItem it[24]; int n = ent_items(dir, it, 24); if (n <= 0) return 0; static char x[16384]; int l = 0;
    l += snprintf(x + l, sizeof x - l, "<!-- entity-screen.xhtpm: GENERATED by phone_op from the entity's own menu.chtpm (a projection, rewritten on every open) -->\n<window label=\"%s\" class=\"entity database-window phone-portrait managed\">\n  <module id=\"projector\" src=\"&.hq-apps/plus-phone/ops/+x/phone_projector.+x\"/>\n  <page name=\"main\">\n    ${PHONE_BARS}\n    ${APP_CHROME}\n    <scrolllist>\n", name);
    for (int i = 0; i < n && l < (int)sizeof x - 700; i++) { char lb[80]; snprintf(lb, sizeof lb, "%s", it[i].label); for (char *c = lb; *c; c++) if (*c == '"' || *c == '<' || *c == '>' || *c == '&' || *c == '$') *c = ' ';
        l += snprintf(x + l, sizeof x - l, "      <item id=\"e%d\" class=\"btn\" label=\"  %s  \" action=\"'${HOUSE}/&.hq-apps/plus-phone/ops/+x/phone_op.+x' entity-run '${ARG3}' '%s' '%d'\"/>\n", i + 1, lb, name, i + 1); }
    l += snprintf(x + l, sizeof x - l, "    </scrolllist>\n    ${PHONE_FOOTER}\n  </page>\n</window>\n"); char f[P]; snprintf(f, sizeof f, "%s/entity-screen.xhtpm", inst); wr(f, x); snprintf(f, sizeof f, "%s/entity-screen.css", inst); wr(f, ENT_CSS); snprintf(out, cap, "%s/entity-screen.xhtpm", inst); return 1; }
/* bottom-bar cells of the entities (after the app cell): names only, like the LyfeDesk bar's entity cells; added to a finished <footer>...</footer> string */
static void footer_add_entities(const char *inst, char *out, size_t cap) {
    char n[8][40]; int c = ent_names(n); char cells[1200]; int l = 0; cells[0] = 0; for (int i = 0; i < c; i++) { char dir[P]; if (!ent_dir(n[i], dir, sizeof dir)) continue; l += snprintf(cells + l, sizeof cells - l, "      <item id=\"ft-ent%d\" class=\"phone-ft hqwin\" label=\"\xF0\x9F\x97\xA1 %s\" action=\"'%s/&.hq-apps/plus-phone/ops/+x/phone_op.+x' entity '%s' '%s'\"/>\n", i, n[i], HOUSE, inst, n[i]); }
    if (!l) return; char *e = strstr(out, "</footer>"); if (!e) return; char *tail = strdup(e); if (!tail) return; size_t pre = (size_t)(e - out); while (pre && (out[pre - 1] == ' ')) pre--; if (pre + strlen(cells) + strlen(tail) + 8 >= cap) { free(tail); return; } out[pre] = 0; strcat(out, cells); strcat(out, "    "); strcat(out, tail); free(tail); }
/* The APP's chrome row, directly under the phone's top bar (owner 2026-11-16: "I can't minimize TSC if it doesn't have its own chrome"): the open app's NAME plus its own controls, `_` = minimize (home keeps a dimmed cell) and `x` = close the app.
 * Token ${APP_CHROME} in an app screen is replaced by this row; empty when no app is open. Built from the manifest TITLE so no app draws it itself. */
static void app_chrome(const char *inst, char *out, size_t cap) {
    char f[P], id[64] = "", mf[P], ln[400], title[120] = ""; out[0] = 0; snprintf(f, sizeof f, "%s/.hq_manager/screen.txt", inst); FILE *fp = fopen(f, "r"); if (fp) { if (fgets(id, sizeof id, fp)) id[strcspn(id, "\r\n")] = 0; fclose(fp); } if (!strncmp(id, "@entity:", 8)) { const char *nm = id + 8; if (!is_word(nm)) return; snprintf(out, cap, "<tabbar id=\"appchrome\" class=\"tab-bar app-chrome\">\n      <tab id=\"ac-title\" class=\"tab no-nav\" label=\"%s\"/>\n      <tab id=\"ac-min\" class=\"tab\" label=\"_\" action=\"'${HOUSE}/&.hq-apps/plus-phone/ops/+x/phone_op.+x' home '${ARG3}'\"/>\n      <tab id=\"ac-close\" class=\"tab\" label=\"x\" action=\"'${HOUSE}/&.hq-apps/plus-phone/ops/+x/phone_op.+x' home '${ARG3}'\"/>\n    </tabbar>", nm); return; } if (!is_word(id)) return;
    snprintf(mf, sizeof mf, "%s/&.hq-apps/plus-phone/apps/%s/manifest.pdl", HOUSE, id); fp = fopen(mf, "r"); while (fp && fgets(ln, sizeof ln, fp)) { if (strncmp(ln, "TITLE", 5)) continue; char *b = strchr(ln, '|'); if (!b) continue; b++; while (*b == ' ') b++; snprintf(title, sizeof title, "%s", b); title[strcspn(title, "\r\n")] = 0; size_t l = strlen(title); while (l && title[l - 1] == ' ') title[--l] = 0; break; } if (fp) fclose(fp);
    if (!title[0]) snprintf(title, sizeof title, "%s", id); for (char *c = title; *c; c++) if (*c == '"' || *c == '<' || *c == '>' || *c == '&' || *c == '$') *c = ' ';
    snprintf(out, cap, "<tabbar id=\"appchrome\" class=\"tab-bar app-chrome\">\n      <tab id=\"ac-title\" class=\"tab no-nav\" label=\"%s\"/>\n      <tab id=\"ac-min\" class=\"tab\" label=\"_\" action=\"'${HOUSE}/&.hq-apps/plus-phone/ops/+x/phone_op.+x' home '${ARG3}'\"/>\n      <tab id=\"ac-close\" class=\"tab\" label=\"x\" action=\"'${HOUSE}/&.hq-apps/plus-phone/ops/+x/phone_op.+x' closeapp '${ARG3}'\"/>\n    </tabbar>", title);
}
static void phone_footer(const char *inst, char *out, size_t cap) {
    char f[P], id[64] = "", mf[P], ln[400], title[120] = ""; snprintf(f, sizeof f, "%s/.hq_manager/screen.txt", inst); FILE *fp = fopen(f, "r"); if (fp) { if (fgets(id, sizeof id, fp)) id[strcspn(id, "\r\n")] = 0; fclose(fp); }
    if (is_word(id)) { snprintf(mf, sizeof mf, "%s/&.hq-apps/plus-phone/apps/%s/manifest.pdl", HOUSE, id); fp = fopen(mf, "r"); while (fp && fgets(ln, sizeof ln, fp)) { if (strncmp(ln, "TITLE", 5)) continue; char *b = strchr(ln, '|'); if (!b) continue; b++; while (*b == ' ') b++; snprintf(title, sizeof title, "%s", b); title[strcspn(title, "\r\n")] = 0; size_t l = strlen(title); while (l && title[l - 1] == ' ') title[--l] = 0; break; } if (fp) fclose(fp); }
    int minimized = 0; if (!title[0]) {      /* no app in view: a MINIMIZED app (minimized.txt) is still a cell, dimmed */
        snprintf(f, sizeof f, "%s/.hq_manager/minimized.txt", inst); fp = fopen(f, "r"); if (fp) { if (fgets(id, sizeof id, fp)) id[strcspn(id, "\r\n")] = 0; fclose(fp); }
        if (is_word(id)) { snprintf(mf, sizeof mf, "%s/&.hq-apps/plus-phone/apps/%s/manifest.pdl", HOUSE, id); fp = fopen(mf, "r"); while (fp && fgets(ln, sizeof ln, fp)) { if (strncmp(ln, "TITLE", 5)) continue; char *b = strchr(ln, '|'); if (!b) continue; b++; while (*b == ' ') b++; snprintf(title, sizeof title, "%s", b); title[strcspn(title, "\r\n")] = 0; size_t l = strlen(title); while (l && title[l - 1] == ' ') title[--l] = 0; break; } if (fp) fclose(fp); minimized = title[0] != 0; } }
    for (char *c = title; *c; c++) if (*c == '"' || *c == '<' || *c == '>' || *c == '&' || *c == '$') *c = ' ';
    /* the same cell as the LyfeDesk bottom bar's window cells: class hqwin (+ hqwin-minimized when minimized), label = the window emoji and its NAME, click = activate (restore) the app */
    if (title[0]) snprintf(out, cap, "<footer id=\"statusbar\" class=\"phone-footer\">\n      <item id=\"ft-app1\" class=\"phone-ft hqwin%s\" label=\"\xF0\x9F\xAA\x9F %s\" action=\"'%s/&.hq-apps/plus-phone/ops/+x/phone_op.+x' open '%s' '%s'\"/>\n    </footer>", minimized ? " hqwin-minimized" : "", title, HOUSE, inst, id); else snprintf(out, cap, "<footer id=\"statusbar\" class=\"phone-footer\">\n    </footer>"); footer_add_entities(inst, out, cap);
}
static int generate_layout(const char *inst, const char *lay, int win_w, int *ic) {
    char app[P], src[P], dst[P]; static char xb[16384], cb[8192]; snprintf(app, sizeof app, "%s/&.hq-apps/plus-phone", HOUSE); *ic = pick_icon(win_w);
    snprintf(src, sizeof src, "%s/%s", app, lay); if (!slurp(src, xb, sizeof xb)) return 0; replace_all(xb, sizeof xb, "${PKG}", app); { char ft[2048]; phone_footer(inst, ft, sizeof ft); replace_all(xb, sizeof xb, "${PHONE_FOOTER}", ft); }
    char stem[96]; snprintf(stem, sizeof stem, "%s", lay); char *dot = strrchr(stem, '.'); if (dot) *dot = 0;
    snprintf(src, sizeof src, "%s/%s.css", app, stem); if (!slurp(src, cb, sizeof cb)) return 0; char to[64]; snprintf(to, sizeof to, "width: %dpx; height: %dpx;", fill_px(win_w, *ic), fill_px(win_w, *ic)); replace_all(cb, sizeof cb, "width: 120px; height: 120px;", to);
    snprintf(dst, sizeof dst, "%s/%s", inst, lay); wr(dst, xb); snprintf(dst, sizeof dst, "%s/%s.css", inst, stem); wr(dst, cb); { char it[64]; snprintf(it, sizeof it, "%s %d\n", ICON_NAME[*ic], fill_px(win_w, *ic)); snprintf(dst, sizeof dst, "%s/icon_size.txt", inst); wr(dst, it); } return 1;
}
/* APP SCREENS (2026-11-15): when screen.txt names an installed app whose manifest has a SCREEN row, the phone window runs THAT layout (the first SCREEN row, from the package, so ${PKG} is the package folder and ARG3 the phone instance); out = its absolute path or empty */
static int is_word(const char *s);
static int screen_layout(const char *inst, char *out, size_t cap) {      /* the first `SCREEN | home | path` of the app (portrait), or its `SCREEN | home-landscape | path` when the phone is rotated and the app has one */
    char f[P], id[64] = "", mf[P], ln[600], port[400] = "", land[400] = ""; out[0] = 0; snprintf(f, sizeof f, "%s/.hq_manager/screen.txt", inst); FILE *fp = fopen(f, "r"); if (!fp) return 0; if (fgets(id, sizeof id, fp)) id[strcspn(id, "\r\n")] = 0; fclose(fp); if (!strncmp(id, "@entity:", 8)) return entity_screen(inst, id + 8, out, cap); if (!is_word(id)) return 0;
    snprintf(mf, sizeof mf, "%s/&.hq-apps/plus-phone/apps/%s/manifest.pdl", HOUSE, id); fp = fopen(mf, "r"); if (!fp) return 0;
    while (fgets(ln, sizeof ln, fp)) { if (strncmp(ln, "SCREEN", 6)) continue; ln[strcspn(ln, "\r\n")] = 0; char *b1 = strchr(ln, '|'); if (!b1) continue; char *b2 = strchr(b1 + 1, '|'); if (!b2) continue; char nm[64]; snprintf(nm, sizeof nm, "%.*s", (int)(b2 - b1 - 1) > 60 ? 60 : (int)(b2 - b1 - 1), b1 + 1); char *n = nm; while (*n == ' ') n++; size_t nl = strlen(n); while (nl && n[nl - 1] == ' ') n[--nl] = 0;
        b2++; while (*b2 == ' ') b2++; char rel[400]; snprintf(rel, sizeof rel, "%s", b2); size_t l = strlen(rel); while (l && rel[l - 1] == ' ') rel[--l] = 0; if (!strcmp(n, "home-landscape")) snprintf(land, sizeof land, "%s", rel); else if (!port[0]) snprintf(port, sizeof port, "%s", rel); }
    fclose(fp); char orient[16] = ""; kv(inst, "ORIENT", orient, sizeof orient); const char *rel = (!strcmp(orient, "landscape") && land[0]) ? land : port[0] ? port : land; if (!rel[0]) return 0; snprintf(out, cap, "%s/&.hq-apps/plus-phone/apps/%s/%s", HOUSE, id, rel); return !access(out, R_OK); }
/* ONE WINDOW (owner 2026-11-16, PLUS-PHONE-LAYOUT-SPEC): the phone window always runs its OWN layout file <inst>/plus-phone[-landscape].xhtpm; this writes into it what the window shows now:
 * the open app's screen (css first, ${PKG} filled) or the picker. The renderer re-parses that file, so open and home need no relaunch (flip still does: the window size changes). */
/* The phone's own top-bar block, defined ONCE in the phone template between the PHONE-BARS-BEGIN / -END comments; an app screen asks for it with the token ${PHONE_BARS} (the app never re-draws phone bars).
 * Injected into an app the `show="${app_open}"` guards go (an app is open by definition). out = the block, or empty if the template has no markers. */
static int phone_bars(const char *lay, char *out, size_t cap) {
    static char tb[32768]; char src[P]; out[0] = 0; snprintf(src, sizeof src, "%s/&.hq-apps/plus-phone/%s", HOUSE, lay); if (!slurp(src, tb, sizeof tb)) return 0;
    char *b = strstr(tb, "PHONE-BARS-BEGIN"); char *e = strstr(tb, "<!-- PHONE-BARS-END"); if (!b || !e || e < b) return 0; b = strchr(b, '\n'); if (!b) return 0; b++;
    size_t n = (size_t)(e - b); if (n + 1 > cap) return 0; memcpy(out, b, n); out[n] = 0; replace_all(out, cap, " show=\"${app_open}\"", ""); return 1;
}
static int refresh_layout(const char *inst) {
    char orient[16]; kv(inst, "ORIENT", orient, sizeof orient); int land = !strcmp(orient, "landscape"); const char *lay = land ? "plus-phone-landscape.xhtpm" : "plus-phone.xhtpm"; int ic = 0; char sl[P];
    if (screen_layout(inst, sl, sizeof sl)) {
        static char xb[65536], cb[16384]; char id[64] = "", f[P], pkg[P], css[P], stem[96], dst[P]; snprintf(f, sizeof f, "%s/.hq_manager/screen.txt", inst); FILE *fp = fopen(f, "r"); if (fp) { if (fgets(id, sizeof id, fp)) id[strcspn(id, "\r\n")] = 0; fclose(fp); }
        snprintf(pkg, sizeof pkg, "%s", sl); { char *sd = strrchr(pkg, '/'); if (sd) *sd = 0; }      /* ${PKG} = the screen file's own folder (apps/<id>/screens), as when the window ran the package file: screens say ${PKG}/../ops (found 2026-11-16: it was the package root, so every TSC button pointed at a path that did not exist) */ snprintf(css, sizeof css, "%s", sl); char *d = strrchr(css, '.'); if (d) snprintf(d, 6, ".css");
        if (slurp(sl, xb, sizeof xb) && slurp(css, cb, sizeof cb)) { { char *m = strstr(xb, "<module"); char *e = m ? strstr(m, "/>") : NULL; if (e) { const char *pm = "<module id=\"projector\" src=\"&.hq-apps/plus-phone/ops/+x/phone_projector.+x\"/>"; memmove(m + strlen(pm), e + 2, strlen(e + 2) + 1); memcpy(m, pm, strlen(pm)); } }      /* the window's module is always the phone projector (it runs the app's own as its child) */
            if (strstr(xb, "${PHONE_BARS}")) { static char pb[16384]; if (phone_bars(lay, pb, sizeof pb)) replace_all(xb, sizeof xb, "${PHONE_BARS}", pb); }
            { char ft[2048]; phone_footer(inst, ft, sizeof ft); replace_all(xb, sizeof xb, "${PHONE_FOOTER}", ft); } { char ac[1536]; app_chrome(inst, ac, sizeof ac); replace_all(xb, sizeof xb, "${APP_CHROME}", ac); } replace_all(xb, sizeof xb, "${PKG}", pkg); snprintf(stem, sizeof stem, "%s", lay); char *dot = strrchr(stem, '.'); if (dot) *dot = 0; 
            snprintf(dst, sizeof dst, "%s/%s.css", inst, stem); wr(dst, cb); snprintf(dst, sizeof dst, "%s/%s", inst, lay); wr(dst, xb); return 1; }
    }
    return generate_layout(inst, lay, land ? 704 : 364, &ic);
}
static long launch(const char *inst) {
    char orient[16]; kv(inst, "ORIENT", orient, sizeof orient);
    { char op[P], app[P], tmp[P], ws[16], hs[16]; int land = !strcmp(orient, "landscape"); snprintf(op, sizeof op, "%s/&.widgits/_shared-lib/ops/+x/window_place_op.+x", HOUSE); snprintf(app, sizeof app, "%s/&.hq-apps/plus-phone", HOUSE); snprintf(tmp, sizeof tmp, "%s/place.out", inst);
      snprintf(ws, sizeof ws, "%d", land ? 704 : 364); snprintf(hs, sizeof hs, "%d", land ? 364 : 704);      /* window sizes with frame; the shared op picks a free slot beside the other phones (placement.pdl opt-in) */
      if (!access(op, X_OK)) { char *av[] = { op, "place", app, (char *)inst, ws, hs, NULL }; run_out(av, tmp); unlink(tmp); } } const char *lay = !strcmp(orient, "landscape") ? "plus-phone-landscape.xhtpm" : "plus-phone.xhtpm";
    char x[P], exe[P], funnel[P], tmp[P], nm[48], n[16]; if (!refresh_layout(inst)) return 0; snprintf(x, sizeof x, "%s/%s", inst, lay);      /* the per-instance layout (picker with tile size filled in, or the open app's screen) */ snprintf(exe, sizeof exe, "%s/_.monads/_.livedesk-taskbar/ops/+x/khtpm_core_render.+x", HOUSE); snprintf(funnel, sizeof funnel, "%s/&.widgits/_shared-lib/kh_spawn.sh", HOUSE);
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
/* phone_prefs.pdl is a PREFERENCE file (no game state), whole file rewritten atomically. Keys: icon_max (small|medium|large), hostlist_url (https://..., the master host list to fetch before joining; `-` clears it), net_advertise (the address other machines should dial, an IPv4 or host name; `-` clears it). */
static int pref_valid_word(const char *s, const char *extra, size_t hi) { size_t n = strlen(s); if (!n || n > hi) return 0; for (; *s; s++) if (!(isalnum((unsigned char)*s) || strchr(extra, *s))) return 0; return 1; }
static void pref_read(const char *f, const char *key, char *out, size_t cap) { out[0] = 0; FILE *fp = fopen(f, "r"); if (!fp) return; char ln[600]; size_t kl = strlen(key); while (fgets(ln, sizeof ln, fp)) { if (strncmp(ln, key, kl) || strncmp(ln + kl, " | ", 3)) continue; ln[strcspn(ln, "\r\n")] = 0; snprintf(out, cap, "%s", ln + kl + 3); break; } fclose(fp); }
static int v_prefs(const char *key, const char *val) {
    int is_icon = !strcmp(key, "icon_max"), is_url = !strcmp(key, "hostlist_url"), is_adv = !strcmp(key, "net_advertise"); if (!is_icon && !is_url && !is_adv) { printf("PREFS-REFUSED unknown setting %s (known: icon_max, hostlist_url, net_advertise)\n", key); return 1; }
    char f[P], url[400], adv[100]; snprintf(f, sizeof f, "%s/&.hq-apps/plus-phone/phone_prefs.pdl", HOUSE); pref_read(f, "HOSTLIST_URL", url, sizeof url); pref_read(f, "NET_ADVERTISE", adv, sizeof adv); const char *icon = ICON_NAME[icon_cap()];
    if (!val) { if (is_icon) printf("icon_max %s\n", icon); else printf("%s %s\n", key, is_url ? (url[0] ? url : "-") : (adv[0] ? adv : "-")); return 0; }
    if (is_icon) { int ok = 0; for (int i = 0; i < 3; i++) if (!strcmp(val, ICON_NAME[i])) ok = 1; if (!ok) { printf("PREFS-REFUSED icon_max must be small, medium or large (got %s); nothing changed\n", val); return 1; } icon = val; }
    else if (is_url) { if (!strcmp(val, "-")) url[0] = 0; else if (!strncmp(val, "https://", 8) && strlen(val) <= 300 && pref_valid_word(val + 8, "./-_:?=&%~", 290)) snprintf(url, sizeof url, "%s", val); else { printf("PREFS-REFUSED hostlist_url must be an https:// address of at most 300 plain characters (nothing changed)\n"); return 1; } }
    else { if (!strcmp(val, "-")) adv[0] = 0; else if (pref_valid_word(val, ".-", 80)) snprintf(adv, sizeof adv, "%s", val); else { printf("PREFS-REFUSED net_advertise must be an IPv4 address or a host name (nothing changed)\n"); return 1; } }
    char txt[900]; snprintf(txt, sizeof txt, "# phone_prefs.pdl - a PREFERENCE of the plus phone (never read as game input). Written by `phone_op prefs`.\nICON_MAX | %s\n", icon); if (url[0]) { size_t l = strlen(txt); snprintf(txt + l, sizeof txt - l, "HOSTLIST_URL | %s\n", url); } if (adv[0]) { size_t l = strlen(txt); snprintf(txt + l, sizeof txt - l, "NET_ADVERTISE | %s\n", adv); } wr(f, txt); printf("PREFS-SET %s %s\n", key, val); return 0;
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
    /* an INSTALLED app opens its screen: screen.txt names it, the projector hides the grid and shows the app (a package with no screens is a BLANK app, shown as blank). settings is show-only. */
    char mf[P], sf[P]; const char *h = strstr(inst, "/&.hq-apps/plus-phone/instances/"); if (h && strcmp(app, "settings")) { snprintf(mf, sizeof mf, "%.*s/&.hq-apps/plus-phone/apps/%s/manifest.pdl", (int)(h - inst), inst, app); if (!access(mf, R_OK)) { snprintf(sf, sizeof sf, "%s/.hq_manager/screen.txt", inst); wr(sf, app); { char mn[P]; snprintf(mn, sizeof mn, "%s/.hq_manager/minimized.txt", inst); unlink(mn); } unlink(f); bump(inst); refresh_layout(inst);      /* same window: its layout file is rewritten and re-parsed (no relaunch) */ return 0; } }
    snprintf(txt, sizeof txt, "%s: nothing to show yet. Nothing was started.\n", app); wr(f, txt); bump(inst); return 0; }
/* INSTALL / UNINSTALL (2026-11-15, design section 24.4): an app reaches the phone only as a checked package folder. `install <house> <package_dir>`: app_check first (refused = nothing copied, its lines are printed), then the folder
 * replaces apps/<id>/ (copied beside it first and renamed, so a failed copy never leaves half an app); every open phone is told to republish. `uninstall <house> <id>`: removes apps/<id>/ (the system app settings is refused). */
static int is_word(const char *s) { if (!*s) return 0; for (; *s; s++) if (!(isalnum((unsigned char)*s) || *s == '-' || *s == '_')) return 0; return 1; }
static void bump_all(void) { char d[P]; snprintf(d, sizeof d, "%s/&.hq-apps/plus-phone/instances", HOUSE); DIR *dp = opendir(d); if (!dp) return; struct dirent *e; while ((e = readdir(dp))) { if (e->d_name[0] == '.') continue; char inst[P]; snprintf(inst, sizeof inst, "%s/%s", d, e->d_name); bump(inst); } closedir(dp); }
static int v_install(const char *pkg) {
    char chk[P], tmp[P], tmp2[P]; snprintf(chk, sizeof chk, "%s/&.hq-apps/plus-phone/ops/+x/app_check.+x", HOUSE); if (access(chk, X_OK)) { printf("INSTALL-REFUSED app_check is not built\n"); return 1; }
    snprintf(tmp, sizeof tmp, "%s/&.hq-apps/plus-phone/install.chk", HOUSE); { char *av[] = { chk, (char *)pkg, HOUSE, NULL }; int rc = run_out(av, tmp); FILE *f = fopen(tmp, "r"); char ln[400]; while (f && fgets(ln, sizeof ln, f)) fputs(ln, stdout); if (f) fclose(f); unlink(tmp); if (rc != 0) { printf("INSTALL-REFUSED the package did not pass app_check; nothing was copied\n"); return 1; } }
    char id[64]; { char *p = strdup(pkg); size_t l = strlen(p); while (l > 1 && p[l - 1] == '/') p[--l] = 0; char *b = strrchr(p, '/'); snprintf(id, sizeof id, "%s", b ? b + 1 : p); free(p); } if (!is_word(id)) { printf("INSTALL-REFUSED bad id\n"); return 1; }
    char apps[P], dst[P], stage[P]; snprintf(apps, sizeof apps, "%s/&.hq-apps/plus-phone/apps", HOUSE); snprintf(dst, sizeof dst, "%s/%s", apps, id); snprintf(stage, sizeof stage, "%s/.stage-%s", apps, id); snprintf(tmp2, sizeof tmp2, "%s/&.hq-apps/plus-phone/install.out", HOUSE);
    { char *rm[] = { "/bin/rm", "-rf", stage, NULL }; run_out(rm, tmp2); } { char *cp[] = { "/bin/cp", "-R", (char *)pkg, stage, NULL }; if (run_out(cp, tmp2) != 0) { printf("INSTALL-FAILED the copy failed; nothing changed\n"); unlink(tmp2); return 1; } }
    { char *rm[] = { "/bin/rm", "-rf", dst, NULL }; run_out(rm, tmp2); } if (rename(stage, dst)) { printf("INSTALL-FAILED could not move the staged copy into place\n"); unlink(tmp2); return 1; } unlink(tmp2); bump_all(); printf("INSTALLED %s\n", id); return 0;
}
static int v_uninstall(const char *id) {
    if (!is_word(id)) { printf("UNINSTALL-REFUSED bad id\n"); return 1; } if (!strcmp(id, "settings")) { printf("UNINSTALL-REFUSED settings is a system app\n"); return 1; }
    char dst[P], tmp[P]; snprintf(dst, sizeof dst, "%s/&.hq-apps/plus-phone/apps/%s", HOUSE, id); if (access(dst, F_OK)) { printf("UNINSTALL-REFUSED %s is not installed\n", id); return 1; }
    snprintf(tmp, sizeof tmp, "%s/&.hq-apps/plus-phone/uninstall.out", HOUSE); char *rm[] = { "/bin/rm", "-rf", dst, NULL }; run_out(rm, tmp); unlink(tmp); bump_all(); printf("UNINSTALLED %s\n", id); return 0;
}
/* MINIMIZE (PLUS-PHONE-LAYOUT-SPEC rule 3): leaving an open app (the < button, the "app picker" row) goes to the picker and keeps the app as a DIMMED cell in the phone bottom bar (minimized.txt = its id; a UI preference, never game state);
 * tapping the cell opens it again. Today a minimized app is NOT running in the background (its projector stops with the view; its data and ledger are untouched). `closeapp` removes the cell for good. */
static int v_home(const char *inst) { char f[P], sl[P], id[64] = ""; int had = screen_layout(inst, sl, sizeof sl); snprintf(f, sizeof f, "%s/.hq_manager/screen.txt", inst); { FILE *fp = fopen(f, "r"); if (fp) { if (fgets(id, sizeof id, fp)) id[strcspn(id, "\r\n")] = 0; fclose(fp); } }
    if (had && is_word(id)) { char mf[P]; snprintf(mf, sizeof mf, "%s/.hq_manager/minimized.txt", inst); wr(mf, id); }
    snprintf(f, sizeof f, "%s/.hq_manager/last.txt", inst); unlink(f); snprintf(f, sizeof f, "%s/.hq_manager/screen.txt", inst); unlink(f); bump(inst); if (had) refresh_layout(inst); return 0; }
static int v_closeapp(const char *inst) { v_home(inst); char f[P]; snprintf(f, sizeof f, "%s/.hq_manager/minimized.txt", inst); unlink(f); bump(inst); refresh_layout(inst); return 0; }
/* ENTITY verbs (spec section 5): `entity <inst> <name>` shows the entity screen in the SAME window (no relaunch); `entity-run <inst> <name> <row>` runs row N (1-based, the numbers the screen shows) of the entity's own menu:
 * `void` = back to the picker, `CLOSE` = the one word CLOSE in the entity's interact_relay.txt (exactly what a click on Close does on the desktop: the entity ends and the manager respawns it), and the rows of the form
 * sh -c '<script>' run as /bin/sh -c <script> <entity folder> <house> by fork/exec (no outer shell string is ever built; env WIN_X/WIN_Y like the desktop menu), detached, output discarded. Any other form is refused. */
static int v_entity(const char *inst, const char *name) { char dir[P], sl[P]; if (!is_word(name) || !ent_listed(name) || !ent_dir(name, dir, sizeof dir)) { printf("ENTITY-REFUSED %s is not a listed entity of this desk\n", name); return 1; }
    char f[P], id[80]; snprintf(id, sizeof id, "@entity:%s", name); snprintf(f, sizeof f, "%s/.hq_manager/screen.txt", inst); wr(f, id); if (!entity_screen(inst, name, sl, sizeof sl)) { unlink(f); printf("ENTITY-REFUSED the menu of %s has no rows\n", name); return 1; } bump(inst); refresh_layout(inst); printf("ENTITY-OPENED %s\n", name); return 0; }
static int v_entity_run(const char *inst, const char *name, const char *rowtxt) {
    char dir[P]; if (!is_word(name) || !ent_listed(name) || !ent_dir(name, dir, sizeof dir)) { printf("ENTITY-REFUSED %s is not a listed entity of this desk\n", name); return 1; } char *e; long row = strtol(rowtxt, &e, 10); static EItem it[24]; int n = ent_items(dir, it, 24); if (*e || row < 1 || row > n) { printf("ENTITY-REFUSED no row %s\n", rowtxt); return 2; }
    const char *a = it[row - 1].action; if (!strcmp(a, "void")) { v_home(inst); printf("ENTITY-ROW %ld %s: back\n", row, it[row - 1].label); return 0; }
    if (!strcmp(a, "CLOSE")) { char rf[P]; snprintf(rf, sizeof rf, "%s/interact_relay.txt", dir); FILE *w = fopen(rf, "a"); if (!w) { printf("ENTITY-REFUSED cannot write the close request\n"); return 1; } fputs("CLOSE\n", w); fclose(w); v_home(inst); printf("ENTITY-ROW %ld %s: close requested\n", row, it[row - 1].label); return 0; }
    if (strncmp(a, "sh -c '", 7)) { printf("ENTITY-REFUSED row %ld has an action form the phone does not run\n", row); return 1; } char script[2400]; snprintf(script, sizeof script, "%s", a + 7); char *q = strrchr(script, '\''); if (!q) { printf("ENTITY-REFUSED row %ld action is malformed\n", row); return 1; } *q = 0;
    signal(SIGCHLD, SIG_IGN); pid_t c = fork(); if (c < 0) return 1; if (c == 0) { setsid(); int nul = open("/dev/null", O_RDWR); if (nul >= 0) { dup2(nul, 0); dup2(nul, 1); dup2(nul, 2); } setenv("WIN_X", "100", 1); setenv("WIN_Y", "100", 1); execl("/bin/sh", "sh", "-c", script, dir, HOUSE, (char *)NULL); _exit(127); }
    printf("ENTITY-ROW %ld %s: started\n", row, it[row - 1].label); return 0; }

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: phone_op new|list <house> | close <house> <n> | flip|relaunch|home|closeself <instance> | open <instance> <app>\n"); return 2; }
    const char *v = argv[1];
    if (!strcmp(v, "new") || !strcmp(v, "list") || !strcmp(v, "close") || !strcmp(v, "prefs") || !strcmp(v, "install") || !strcmp(v, "uninstall")) { snprintf(HOUSE, sizeof HOUSE, "%s", argv[2]); if (!strcmp(v, "install")) return argc >= 4 ? v_install(argv[3]) : 2; if (!strcmp(v, "uninstall")) return argc >= 4 ? v_uninstall(argv[3]) : 2; if (!strcmp(v, "prefs")) return argc >= 4 ? v_prefs(argv[3], argc >= 5 ? argv[4] : NULL) : 2; if (!strcmp(v, "new")) return v_new(); if (!strcmp(v, "list")) return v_list(); if (argc < 4) return 2; return v_close(argv[3]); }
    if (!house_of(argv[2])) { fprintf(stderr, "phone_op: not a plus-phone instance: %s\n", argv[2]); return 2; }
    if (!strcmp(v, "quit")) { usleep(300000); kill_window(argv[2]); printf("PHONE-QUIT window closed, the phone and its apps keep their data (phone_op relaunch brings it back)\n"); return 0; }      /* HQ > quit: close the WINDOW only; "close" deletes the whole instance (tests, clean-up) and would lose a match */
    if (!strcmp(v, "flip")) return v_flip(argv[2]); if (!strcmp(v, "relaunch")) return v_relaunch(argv[2]); if (!strcmp(v, "home")) return v_home(argv[2]); if (!strcmp(v, "closeapp")) return v_closeapp(argv[2]);
    if (!strcmp(v, "open") && argc >= 4) return v_open(argv[2], argv[3]);
    if (!strcmp(v, "entity") && argc >= 4) return v_entity(argv[2], argv[3]);
    if (!strcmp(v, "entity-run") && argc >= 5) return v_entity_run(argv[2], argv[3], argv[4]);
    if (!strcmp(v, "closeself")) { const char *at = strstr(argv[2], MARK); return v_close(at + strlen(MARK)); }
    fprintf(stderr, "phone_op: unknown verb %s\n", v); return 2;
}
