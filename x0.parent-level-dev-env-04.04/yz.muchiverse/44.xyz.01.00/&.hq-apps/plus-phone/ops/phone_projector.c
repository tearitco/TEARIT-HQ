/* phone_projector.c - the <module> of plus-phone*.xhtpm: writes <instance>/.hq_manager/ui.txt ONLY when it changes (plugz_projector pattern: marker-size wake-up, 5 s heartbeat, exits when its parent is gone).
 * Reads phone.txt (PORT, ORIENT, PHONE), the apps list (apps/<id>/manifest.pdl, the ONE reader of the list) and last.txt. No logic of games lives here. argv: <house> <package_dir> [--once <instance>]; instance = $KHTPM_ARG3.
 * Build: gcc -std=gnu11 -Wall -O2 -o ops/+x/phone_projector.+x ops/phone_projector.c */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#define P 4096
static char HOUSE[P], INST[P], HQ[P], last_ui[16384];
static int is_word(const char *s); static int ent_names(char n[8][40]); static int ent_dir(const char *name, char *out, size_t cap);   /* phone_entity.inc, included below */
static int kv(const char *file, const char *key, char *out, size_t cap) { out[0] = 0; FILE *fp = fopen(file, "r"); if (!fp) return 0; char ln[400]; size_t kl = strlen(key); int hit = 0; while (fgets(ln, sizeof ln, fp)) { if (strncmp(ln, key, kl) || strncmp(ln + kl, " | ", 3)) continue; snprintf(out, cap, "%s", ln + kl + 3); out[strcspn(out, "\r\n")] = 0; hit = 1; break; } fclose(fp); return hit; }
static long msize(void) { char f[P]; struct stat sb; snprintf(f, sizeof f, "%s/marker.txt", HQ); return stat(f, &sb) ? 0 : (long)sb.st_size; }
static void publish(void) {
    char f[P], num[16], port[16], orient[16], last[300] = ""; snprintf(f, sizeof f, "%s/phone.txt", INST); kv(f, "PHONE", num, sizeof num); kv(f, "PORT", port, sizeof port); kv(f, "ORIENT", orient, sizeof orient);
    snprintf(f, sizeof f, "%s/last.txt", HQ); { FILE *fp = fopen(f, "r"); if (fp) { if (fgets(last, sizeof last, fp)) last[strcspn(last, "\r\n")] = 0; fclose(fp); } }
    static char ui[16384]; ui[0] = 0; size_t l;
    l = strlen(ui); snprintf(ui + l, sizeof ui - l, "phone_title=+%s :%s\nport_label=port %s\norient_label=%s\n", num, port, port, orient);
    char ad[P]; snprintf(ad, sizeof ad, "%s/&.hq-apps/plus-phone/apps", HOUSE); DIR *dp = opendir(ad); int n = 0; static char ids[16][64], titles[16][80], icons[16][P]; static int ord[16];
    if (dp) { struct dirent *e; while ((e = readdir(dp)) && n < 16) { if (e->d_name[0] == '.') continue; char mf[P]; snprintf(mf, sizeof mf, "%s/%s/manifest.pdl", ad, e->d_name); char id[64], title[80]; if (!kv(mf, "APP", id, sizeof id) && 0) continue;
            FILE *fp = fopen(mf, "r"); if (!fp) continue; char ln[300]; id[0] = title[0] = 0; char icon[200] = ""; int order = 50, pin = 0; while (fgets(ln, sizeof ln, fp)) { if (!strncmp(ln, "APP | id | ", 11)) { snprintf(id, sizeof id, "%s", ln + 11); id[strcspn(id, "\r\n")] = 0; } else if (!strncmp(ln, "TITLE | ", 8)) { snprintf(title, sizeof title, "%s", ln + 8); title[strcspn(title, "\r\n")] = 0; } else if (!strncmp(ln, "ICON | ", 7)) { snprintf(icon, sizeof icon, "%s", ln + 7); icon[strcspn(icon, "\r\n")] = 0; } else if (!strncmp(ln, "ORDER | ", 8)) order = atoi(ln + 8); else if (!strncmp(ln, "PIN | first", 11)) pin = 1; } fclose(fp);
            if (id[0]) { snprintf(ids[n], sizeof ids[n], "%s", id); snprintf(titles[n], sizeof titles[n], "%s", title[0] ? title : id); ord[n] = pin ? -1000 : order;      /* PIN | first = a system app that is always first in the index (settings) */ if (icon[0]) snprintf(icons[n], sizeof icons[n], "%s/&.hq-apps/plus-phone/apps/%s/%s", HOUSE, e->d_name, icon); else icons[n][0] = 0; n++; } } closedir(dp); }
    for (int a = 1; a < n; a++) for (int b = a; b > 0 && ord[b] < ord[b - 1]; b--) { char t1[64], t2[80], t3[P]; int to = ord[b]; ord[b] = ord[b - 1]; ord[b - 1] = to; memcpy(t1, ids[b], 64); memcpy(ids[b], ids[b - 1], 64); memcpy(ids[b - 1], t1, 64); memcpy(t2, titles[b], 80); memcpy(titles[b], titles[b - 1], 80); memcpy(titles[b - 1], t2, 80); memcpy(t3, icons[b], P); memcpy(icons[b], icons[b - 1], P); memcpy(icons[b - 1], t3, P); }      /* manifest ORDER rows decide the home order */
    char scr[64] = ""; { char sf[P]; snprintf(sf, sizeof sf, "%s/screen.txt", HQ); FILE *sp = fopen(sf, "r"); if (sp) { if (fgets(scr, sizeof scr, sp)) scr[strcspn(scr, "\r\n")] = 0; fclose(sp); } }
    int open_i = -1; for (int i = 0; i < n && scr[0]; i++) if (!strcmp(ids[i], scr)) open_i = i;      /* an open app hides the grid (count 0) and shows its own screen */
    { char b[80] = "", pg[80] = "", sf[P], ln[300]; snprintf(sf, sizeof sf, "%s/&.hq-apps/plus-phone/phone_prefs.pdl", HOUSE); FILE *pf = fopen(sf, "r"); if (pf) { while (fgets(ln, sizeof ln, pf)) if (!strncmp(ln, "BOOK | ", 7)) { ln[strcspn(ln, "\r\n")] = 0; snprintf(b, sizeof b, "%s", ln + 7); } fclose(pf); } if (!b[0]) snprintf(b, sizeof b, "phone");
      if (open_i >= 0) snprintf(pg, sizeof pg, "%s", ids[open_i]); else { snprintf(sf, sizeof sf, "%s/#.desktop/strip_ui.txt", HOUSE); pf = fopen(sf, "r"); if (pf) { while (fgets(ln, sizeof ln, pf)) if (!strncmp(ln, "username=", 9)) { ln[strcspn(ln, "\r\n")] = 0; snprintf(pg, sizeof pg, "%s", ln + 9); break; } fclose(pf); } if (!pg[0]) snprintf(pg, sizeof pg, "home"); }      /* the home page is named after the user (only the name line is read, nothing else of the user folder) */
      l = strlen(ui); snprintf(ui + l, sizeof ui - l, "book_label=%s\npage_label=%s\n", b, pg); }
    { char hp[P], inst[P]; snprintf(inst, sizeof inst, "%s", INST); l = strlen(ui); snprintf(ui + l, sizeof ui - l, "n_pages=%d\npg_0_label=%s\npg_0_act='%s/&.hq-apps/plus-phone/ops/+x/phone_op.+x' 'home' '%s'\n", n + 1, "home", HOUSE, inst); (void)hp;
      for (int i = 0; i < n; i++) { l = strlen(ui); snprintf(ui + l, sizeof ui - l, "pg_%d_label=%s\npg_%d_act='%s/&.hq-apps/plus-phone/ops/+x/phone_op.+x' 'open' '%s' '%s'\n", i + 1, ids[i], i + 1, HOUSE, inst, ids[i]); } }      /* the pages of the book: home + every installed app (the HQ/page dropdown rows) */
    l = strlen(ui); snprintf(ui + l, sizeof ui - l, "n_apps=%d\ntiles_show=%d\nno_apps=%d\napp_open=%d\napp_title=%s\napp_blank=1\n", n, open_i >= 0 ? 0 : 1, (n == 0 && open_i < 0) ? 1 : 0, open_i >= 0 ? 1 : 0, open_i >= 0 ? titles[open_i] : "");
    for (int i = 0; i < n; i++) { l = strlen(ui); snprintf(ui + l, sizeof ui - l, "app_%d_label=%s\napp_%d_id=%s\napp_%d_icon=%s\napp_%d_show=%d\n", i, titles[i], i, ids[i], i, icons[i], i, (open_i < 0 || i == open_i) ? 1 : 0); }
    { char en[8][40]; int ne = ent_names(en), k = 0; for (int i = 0; i < ne; i++) { char dir[P]; if (!ent_dir(en[i], dir, sizeof dir)) continue; l = strlen(ui); snprintf(ui + l, sizeof ui - l, "ent_%d_label=%s\nent_%d_id=%s\nent_%d_icon=%s/&.hq-apps/plus-phone/entity-icon\n", k, en[i], k, en[i], k, HOUSE); k++; } l = strlen(ui); snprintf(ui + l, sizeof ui - l, "n_ents=%d\n", k); }
    l = strlen(ui); snprintf(ui + l, sizeof ui - l, "last_result=%s\nshow_last=%d\n", last, last[0] ? 1 : 0);
    if (strcmp(ui, last_ui)) { char t[P]; snprintf(f, sizeof f, "%s/ui.txt", HQ); snprintf(t, sizeof t, "%s/ui.txt.tmp", HQ); FILE *w = fopen(t, "w"); if (w) { fputs(ui, w); fclose(w); rename(t, f); } snprintf(last_ui, sizeof last_ui, "%s", ui); }
}
/* ONE WINDOW (PLUS-PHONE-LAYOUT-SPEC): the phone window's module is always THIS projector; an open app's own projector (the <module src> its screen declares) runs as our CHILD while the app is open, and we stay silent
 * (one writer of ui.txt at a time); on home the child is stopped and we publish the picker again. A module is started once per window launch, so this is how a window can change app without being relaunched. */
static pid_t child = 0; static char child_app[64] = "";
static int word_ok(const char *s) { if (!*s) return 0; for (; *s; s++) if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') || *s == '-' || *s == '_')) return 0; return 1; }
static int is_word(const char *s) { return word_ok(s); }
#include "phone_entity.inc"
static int module_src(const char *id, char *out, size_t cap) {      /* the src of the <module> in the app's first SCREEN file */
    char mf[P], ln[600], rel[400] = ""; snprintf(mf, sizeof mf, "%s/&.hq-apps/plus-phone/apps/%s/manifest.pdl", HOUSE, id); FILE *fp = fopen(mf, "r"); if (!fp) return 0;
    while (fgets(ln, sizeof ln, fp)) { if (strncmp(ln, "SCREEN", 6)) continue; char *b1 = strchr(ln, '|'); if (!b1) continue; char *b2 = strchr(b1 + 1, '|'); if (!b2) continue; b2++; while (*b2 == ' ') b2++; snprintf(rel, sizeof rel, "%s", b2); rel[strcspn(rel, "\r\n ")] = 0; break; } fclose(fp); if (!rel[0]) return 0;
    char sp[P]; snprintf(sp, sizeof sp, "%s/&.hq-apps/plus-phone/apps/%s/%s", HOUSE, id, rel); fp = fopen(sp, "r"); if (!fp) return 0; static char buf[8192]; size_t n = fread(buf, 1, sizeof buf - 1, fp); fclose(fp); buf[n] = 0;
    char *m = strstr(buf, "<module"); if (!m) return 0; char *s = strstr(m, "src=\""); if (!s) return 0; s += 5; char *e = strchr(s, '"'); if (!e) return 0; size_t l = (size_t)(e - s); if (l + 1 > cap) return 0; memcpy(out, s, l); out[l] = 0; return 1;
}
static int app_child(void) {      /* reconcile the child with screen.txt; 1 = an app projector is running (it writes ui.txt) */
    char scr[64] = "", f[P]; snprintf(f, sizeof f, "%s/screen.txt", HQ); FILE *fp = fopen(f, "r"); if (fp) { if (fgets(scr, sizeof scr, fp)) scr[strcspn(scr, "\r\n")] = 0; fclose(fp); } if (!word_ok(scr)) scr[0] = 0;
    if (child > 0) { int st; pid_t r = waitpid(child, &st, WNOHANG); if (r == child) { child = 0; child_app[0] = 0; } else if (strcmp(child_app, scr)) { kill(child, SIGTERM); waitpid(child, &st, 0); child = 0; child_app[0] = 0; } }
    if (!child && scr[0]) { char src[P], exe[P], pkg[P]; if (module_src(scr, src, sizeof src)) { snprintf(exe, sizeof exe, "%s/%s", HOUSE, src); snprintf(pkg, sizeof pkg, "%s/&.hq-apps/plus-phone/apps/%s", HOUSE, scr);
        if (!access(exe, X_OK)) { pid_t c = fork(); if (c == 0) { execl(exe, exe, HOUSE, pkg, (char *)NULL); _exit(127); } if (c > 0) { child = c; snprintf(child_app, sizeof child_app, "%s", scr); } } } }
    return child > 0;
}
int main(int argc, char **argv) {
    if (argc < 2) return 2; snprintf(HOUSE, sizeof HOUSE, "%s", argv[1]); const char *inst = getenv("KHTPM_ARG3"); if (argc >= 5 && !strcmp(argv[3], "--once")) inst = argv[4]; if (!inst || !inst[0]) { fprintf(stderr, "phone_projector: no instance dir\n"); return 2; }
    snprintf(INST, sizeof INST, "%s", inst); snprintf(HQ, sizeof HQ, "%s/.hq_manager", inst); mkdir(HQ, 0755);
    if (argc >= 4 && !strcmp(argv[3], "--once")) { publish(); return 0; }
    long seen = -1; time_t lastfull = 0; int had = 0; for (;;) { int run = app_child(); if (run) had = 1; else { if (had) { seen = -1; had = 0; last_ui[0] = 0; }      /* the app projector overwrote ui.txt: forget what we last wrote so the picker is written again */ long m = msize(); time_t now = time(NULL); if (m != seen || now - lastfull >= 5) { publish(); seen = m; lastfull = now; } } if (getppid() == 1) { if (child > 0) kill(child, SIGTERM); return 0; } usleep(400000); }
}
