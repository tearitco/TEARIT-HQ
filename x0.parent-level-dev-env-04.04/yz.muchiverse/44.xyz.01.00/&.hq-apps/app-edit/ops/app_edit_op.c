/* app_edit_op.c - app-edit v1 (2026-11-15; design BOOK 16.game/PLUS-PHONE-DESIGN-2026-11-15.md sections 24-25). ONE op, two roles, like plugz (projector + act) folded together:
 *   MODULE (the layout's <module>, argv: <house> <package_dir>, env KHTPM_ARG3 = the instance dir): publishes <instance>/.hq_manager/ui.txt when something changed (marker size grew, or every 5 s).
 *   VERBS (the layout's action= strings and the launcher):
 *     open <house>                    start the window (instance instances/main) through the spawn funnel; prints APP-EDIT-OPENED <pid>
 *     select <index> <instance>       select a project
 *     check - <instance>              run app_check on the selected project, put its verdict in last.txt
 *     export - <instance>             phone_op install <house> <project>: app_check first, nothing copied on failure; the result line goes to last.txt
 *     phone - <instance>              phone_op new <house> (a phone to look at the exported app)
 *     icon <emoji> <instance>         the selected project's icon/sprite.csv from an emoji through the house pipeline (emoji_gen_atlas then emoji_xtract); icon/emoji.txt records which; the old icon stays unless the new one is complete
 *   --once <instance>                 (harness) one projection and exit. All state is files under <instance>/.hq_manager/ (sel.txt last.txt marker.txt ui.txt); every writer appends one byte to marker.txt.
 * Projects are folders <house>/&.hq-apps/app-edit/projects/<id>/ (an app PACKAGE, validated by plus-phone's app_check). The editor edits NO file yet (v1 = check, export, look); screens and the canvas come next.
 * Build: sh build_app_edit.sh */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <dirent.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#define P 4096
#define MAXPJ 48
static char HOUSE[P], HQ[P], INST[P];

static int run_out(char *const av[], const char *outf) { pid_t c = fork(); if (c < 0) return -1;
    if (c == 0) { int fd = open(outf, O_WRONLY | O_CREAT | O_TRUNC, 0644); if (fd >= 0) { dup2(fd, 1); dup2(fd, 2); } int nul = open("/dev/null", O_RDONLY); if (nul >= 0) dup2(nul, 0); execv(av[0], av); _exit(127); }
    int st; waitpid(c, &st, 0); return WIFEXITED(st) ? WEXITSTATUS(st) : -1; }
static void rd1(const char *name, char *out, size_t cap) { char f[P]; snprintf(f, sizeof f, "%s/%s", HQ, name); out[0] = 0; FILE *fp = fopen(f, "r"); if (!fp) return; if (fgets(out, (int)cap, fp)) out[strcspn(out, "\r\n")] = 0; fclose(fp); }
static void wr(const char *name, const char *text) { char f[P], t[P]; snprintf(f, sizeof f, "%s/%s", HQ, name); snprintf(t, sizeof t, "%s.tmp", f); FILE *w = fopen(t, "w"); if (!w) return; fputs(text, w); fputc('\n', w); fclose(w); rename(t, f); }
static void bump(void) { char f[P]; snprintf(f, sizeof f, "%s/marker.txt", HQ); struct stat sb; if (!stat(f, &sb) && sb.st_size > 4096) unlink(f); FILE *m = fopen(f, "a"); if (m) { fputc('x', m); fclose(m); } }
static long msize(void) { char f[P]; struct stat sb; snprintf(f, sizeof f, "%s/marker.txt", HQ); return stat(f, &sb) ? 0 : (long)sb.st_size; }
static void clean(char *s) { for (; *s; s++) if (*s == '\n' || *s == '\r' || *s == '\t') *s = ' '; }
static void stamp(char *out, size_t cap) { time_t t = time(NULL); struct tm tm; localtime_r(&t, &tm); strftime(out, cap, "%H:%M:%S", &tm); }

static int cmp(const void *a, const void *b) { return strcmp((const char *)a, (const char *)b); }
static int list_projects(char ids[][64]) { char d[P]; snprintf(d, sizeof d, "%s/&.hq-apps/app-edit/projects", HOUSE); DIR *dp = opendir(d); if (!dp) return 0; int n = 0; struct dirent *e;
    while ((e = readdir(dp)) && n < MAXPJ) { if (e->d_name[0] == '.' || strlen(e->d_name) > 63) continue; char m[P]; snprintf(m, sizeof m, "%s/%s/manifest.pdl", d, e->d_name); if (!access(m, R_OK)) snprintf(ids[n++], 64, "%s", e->d_name); }
    closedir(dp); qsort(ids, n, 64, cmp); return n; }
static int selected(char ids[][64], int n) { char s[16]; rd1("sel.txt", s, sizeof s); int i = atoi(s); return (i < 0 || i >= n) ? 0 : i; }
static void tool(const char *rel, char *out, size_t cap) { snprintf(out, cap, "%s/%s", HOUSE, rel); }
static void manifest_val(const char *pk, const char *key, char *out, size_t cap) { out[0] = 0; char f[P]; snprintf(f, sizeof f, "%s/manifest.pdl", pk); FILE *fp = fopen(f, "r"); char ln[400]; size_t kl = strlen(key); while (fp && fgets(ln, sizeof ln, fp)) { if (!strncmp(ln, key, kl) && !strncmp(ln + kl, " | ", 3)) { snprintf(out, cap, "%s", ln + kl + 3); out[strcspn(out, "\r\n")] = 0; break; } } if (fp) fclose(fp); }

static char last_ui[24 * 1024];
static void put(char *b, size_t cap, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
#include <stdarg.h>
static void put(char *b, size_t cap, const char *fmt, ...) { size_t l = strlen(b); va_list ap; va_start(ap, fmt); vsnprintf(b + l, cap - l, fmt, ap); va_end(ap); }
static void publish(void) {
    static char ids[MAXPJ][64]; int n = list_projects(ids); int sel = selected(ids, n); char last[400]; rd1("last.txt", last, sizeof last); clean(last);
    static char ui[sizeof last_ui]; ui[0] = 0; put(ui, sizeof ui, "window_title=app-edit  :  %d project%s\nrows_count=%d\nno_rows=%d\nempty_text=no projects yet (projects/ is empty)\n", n, n == 1 ? "" : "s", n, n == 0 ? 1 : 0);
    char ptool[P]; tool("&.hq-apps/plus-phone/apps", ptool, sizeof ptool);
    for (int i = 0; i < n; i++) { char inst_dir[P]; snprintf(inst_dir, sizeof inst_dir, "%s/apps/%s/manifest.pdl", ptool, ids[i]); char pk[P], ti[100]; snprintf(pk, sizeof pk, "%s/&.hq-apps/app-edit/projects/%s", HOUSE, ids[i]); manifest_val(pk, "TITLE", ti, sizeof ti);
        put(ui, sizeof ui, "row_%d_text=%s%s\nrow_%d_idx=%d\nrow_%d_cls=%s\n", i, ti[0] ? ti : ids[i], !access(inst_dir, R_OK) ? "  [on phone]" : "", i, i, i, i == sel ? "data-item active" : "data-item"); }
    if (n == 0) put(ui, sizeof ui, "detail_title=(no project)\ndetail_status=\nn_info=0\nlast_result=%s\n", last);
    else { char pk[P], ti[100], ord[24], outf[P], chk[P]; snprintf(pk, sizeof pk, "%s/&.hq-apps/app-edit/projects/%s", HOUSE, ids[sel]); manifest_val(pk, "TITLE", ti, sizeof ti); manifest_val(pk, "ORDER", ord, sizeof ord);
        tool("&.hq-apps/plus-phone/ops/+x/app_check.+x", chk, sizeof chk); snprintf(outf, sizeof outf, "%s/check.out", HQ); char *av[] = { chk, pk, HOUSE, NULL }; int rc = run_out(av, outf);
        static char info[16][260]; int ni = 0; FILE *fp = fopen(outf, "r"); char ln[400]; while (fp && ni < 16 && fgets(ln, sizeof ln, fp)) { ln[strcspn(ln, "\r\n")] = 0; clean(ln); if (ln[0]) snprintf(info[ni++], sizeof info[0], "%s", ln); } if (fp) fclose(fp); unlink(outf);
        char on[P]; snprintf(on, sizeof on, "%s/%s/manifest.pdl", ptool, ids[sel]);
        put(ui, sizeof ui, "detail_title=%s  (%s)\ndetail_status=%s  |  %s\nlast_result=%s\nn_info=%d\n", ti[0] ? ti : ids[sel], ids[sel], rc == 0 ? "package OK" : "package has problems", !access(on, R_OK) ? "installed on the phone" : "not on the phone yet", last, ni);
        for (int i = 0; i < ni; i++) put(ui, sizeof ui, "inf_%d_text=%s\n", i, info[i]); }
    if (strcmp(ui, last_ui)) { char f[P], t[P]; snprintf(f, sizeof f, "%s/ui.txt", HQ); snprintf(t, sizeof t, "%s/ui.txt.tmp", HQ); FILE *w = fopen(t, "w"); if (w) { fputs(ui, w); fclose(w); rename(t, f); } snprintf(last_ui, sizeof last_ui, "%s", ui); }
}

static int set_paths_from_instance(const char *inst) { const char *mark = "/&.hq-apps/app-edit/instances/"; const char *at = strstr(inst, mark); if (!at || (size_t)(at - inst) >= sizeof HOUSE) return 0; snprintf(HOUSE, sizeof HOUSE, "%.*s", (int)(at - inst), inst); snprintf(INST, sizeof INST, "%s", inst); snprintf(HQ, sizeof HQ, "%s/.hq_manager", inst); mkdir(HQ, 0755); return 1; }
static int verb_open(void) {
    char inst[P], hq[P], x[P], exe[P], funnel[P], tmp[P]; snprintf(inst, sizeof inst, "%s/&.hq-apps/app-edit/instances/main", HOUSE); snprintf(hq, sizeof hq, "%s/.hq_manager", inst);
    char cmd[P]; snprintf(cmd, sizeof cmd, "%s/&.hq-apps/app-edit/instances", HOUSE); mkdir(cmd, 0755); mkdir(inst, 0755); mkdir(hq, 0755);
    snprintf(x, sizeof x, "%s/&.hq-apps/app-edit/app-edit.xhtpm", HOUSE); snprintf(exe, sizeof exe, "%s/_.monads/_.livedesk-taskbar/ops/+x/khtpm_core_render.+x", HOUSE); snprintf(funnel, sizeof funnel, "%s/&.widgits/_shared-lib/kh_spawn.sh", HOUSE);
    if (access(x, R_OK) || access(exe, X_OK) || access(funnel, R_OK)) { printf("APP-EDIT-MISSING a file the window needs\n"); return 1; }
    char pidf[P]; snprintf(pidf, sizeof pidf, "%s/window.pid", inst); { FILE *f = fopen(pidf, "r"); long old = 0; if (f) { if (fscanf(f, "%ld", &old) != 1) old = 0; fclose(f); } if (old > 1 && kill((pid_t)old, 0) == 0) { printf("APP-EDIT-ALREADY-OPEN %ld\n", old); return 0; } }
    snprintf(tmp, sizeof tmp, "%s/spawn.out", inst); setenv("KH_ROLE", "tool", 1); setenv("KH_APP", "app-edit", 1); char *av[] = { "/bin/sh", funnel, HOUSE, "0", "app-edit", "--", exe, HOUSE, x, inst, NULL }; run_out(av, tmp);
    FILE *tp = fopen(tmp, "r"); char out[64] = ""; if (tp) { char *r = fgets(out, sizeof out, tp); (void)r; fclose(tp); } unlink(tmp); long pid = atol(out); if (pid <= 1) { printf("APP-EDIT-FAILED the window did not start\n"); return 1; }
    char num[32]; snprintf(num, sizeof num, "%ld\n", pid); FILE *w = fopen(pidf, "w"); if (w) { fputs(num, w); fclose(w); } printf("APP-EDIT-OPENED %ld\n", pid); return 0;
}
static void result(const char *what, const char *outf) { char line[300] = "", ln[400]; FILE *fp = fopen(outf, "r"); while (fp && fgets(ln, sizeof ln, fp)) { ln[strcspn(ln, "\r\n")] = 0; if (ln[0]) snprintf(line, sizeof line, "%s", ln); } if (fp) fclose(fp); unlink(outf); char ts[16], res[400]; stamp(ts, sizeof ts); snprintf(res, sizeof res, "%s  %s: %s", ts, what, line[0] ? line : "(no output)"); wr("last.txt", res); bump(); }
int main(int argc, char **argv) {
    if (argc >= 3 && !strcmp(argv[1], "open")) { snprintf(HOUSE, sizeof HOUSE, "%s", argv[2]); return verb_open(); }
    if (argc >= 4 && (!strcmp(argv[1], "select") || !strcmp(argv[1], "check") || !strcmp(argv[1], "export") || !strcmp(argv[1], "phone") || !strcmp(argv[1], "icon"))) {
        if (!set_paths_from_instance(argv[3])) { fprintf(stderr, "app_edit_op: not an app-edit instance dir: %s\n", argv[3]); return 2; }
        static char ids[MAXPJ][64]; int n = list_projects(ids); const char *v = argv[1];
        if (!strcmp(v, "select")) { for (const char *c = argv[2]; *c; c++) if (!isdigit((unsigned char)*c)) return 2; wr("sel.txt", argv[2]); wr("last.txt", ""); bump(); return 0; }
        char outf[P]; snprintf(outf, sizeof outf, "%s/act.out", HQ);
        if (!strcmp(v, "phone")) { char op[P]; tool("&.hq-apps/plus-phone/ops/+x/phone_op.+x", op, sizeof op); char *av[] = { op, "new", HOUSE, NULL }; run_out(av, outf); result("open a phone", outf); return 0; }
        if (n == 0) { wr("last.txt", "no project selected"); bump(); return 1; }
        char pk[P]; snprintf(pk, sizeof pk, "%s/&.hq-apps/app-edit/projects/%s", HOUSE, ids[selected(ids, n)]);
        if (!strcmp(v, "check")) { char chk[P]; tool("&.hq-apps/plus-phone/ops/+x/app_check.+x", chk, sizeof chk); char *av[] = { chk, pk, HOUSE, NULL }; int rc = run_out(av, outf); (void)rc; result("check", outf); return 0; }
        if (!strcmp(v, "icon")) {      /* icon <emoji> <instance>: the selected project's icon/sprite.csv made from an emoji by the HOUSE pipeline (emoji_gen_atlas: FreeType + NotoColorEmoji to a PNG, then emoji_xtract: PNG to the 64 px RGBA csv), the same two ops tp_place_desktop and hatch_egg use. No new icon form in manifest.pdl. The old icon is kept unless the new csv is complete. */
            const char *em = argv[2]; size_t el = strlen(em); int okc = el > 0 && el <= 16; for (size_t i = 0; okc && i < el; i++) if ((unsigned char)em[i] < 0x20) okc = 0;
            if (!okc) { wr("last.txt", "icon: give one emoji"); bump(); return 2; }
            char ga[P], xt[P], png[P], csv[P], fin[P], emf[P]; tool("_.monads/_.livedesk-taskbar/ops/+x/emoji_gen_atlas.+x", ga, sizeof ga); tool("_.monads/_.livedesk-taskbar/ops/+x/emoji_xtract.+x", xt, sizeof xt);
            snprintf(png, sizeof png, "%s/icon_tmp.png", HQ); snprintf(csv, sizeof csv, "%s/icon/sprite.csv.new", pk); snprintf(fin, sizeof fin, "%s/icon/sprite.csv", pk); snprintf(emf, sizeof emf, "%s/icon/emoji.txt", pk);
            if (access(ga, X_OK) || access(xt, X_OK)) { wr("last.txt", "icon: the emoji ops are not built"); bump(); return 1; }
            { char d[P]; snprintf(d, sizeof d, "%s/icon", pk); mkdir(d, 0755); }
            unlink(csv); char *a1[] = { ga, (char *)em, png, NULL }; int r1 = run_out(a1, outf); char *a2[] = { xt, png, "0", "64", csv, NULL }; int r2 = r1 == 0 ? run_out(a2, outf) : -1; unlink(png);
            struct stat sb; char h1[40] = ""; FILE *cf = fopen(csv, "r"); if (cf) { if (fgets(h1, sizeof h1, cf)) h1[strcspn(h1, "\r\n")] = 0; fclose(cf); }
            if (r1 != 0 || r2 != 0 || stat(csv, &sb) || sb.st_size < 20000 || strcmp(h1, "# resolution=64")) { unlink(csv); wr("last.txt", "icon: could not make that emoji (the old icon is kept)"); bump(); return 1; }
            if (rename(csv, fin) != 0) { unlink(csv); wr("last.txt", "icon: could not write the icon"); bump(); return 1; }
            { char t[P]; snprintf(t, sizeof t, "%s.tmp", emf); FILE *w = fopen(t, "w"); if (w) { fprintf(w, "%s\n", em); fclose(w); rename(t, emf); } }
            wr("last.txt", "icon set from the emoji (export to put it on the phone)"); bump(); return 0; }
        if (!strcmp(v, "export")) { char op[P]; tool("&.hq-apps/plus-phone/ops/+x/phone_op.+x", op, sizeof op); char *av[] = { op, "install", HOUSE, pk, NULL }; run_out(av, outf); result("export", outf); return 0; }
    }
    if (argc < 2) { fprintf(stderr, "usage: app_edit_op open <house> | select|check|export|phone|icon <arg> <instance> | <house> <pkg> (module)\n"); return 2; }
    snprintf(HOUSE, sizeof HOUSE, "%s", argv[1]); const char *inst = getenv("KHTPM_ARG3"); int once = argc >= 5 && !strcmp(argv[3], "--once"); if (once) inst = argv[4];
    if (!inst || !inst[0] || !set_paths_from_instance(inst)) { fprintf(stderr, "app_edit_op: no usable instance dir (KHTPM_ARG3)\n"); return 2; }
    if (once) { publish(); return 0; }
    long seen = -1; time_t lastfull = 0; for (;;) { long m = msize(); time_t now = time(NULL); if (m != seen || now - lastfull >= 5) { publish(); seen = m; lastfull = now; } if (getppid() == 1) return 0; usleep(400000); }
}
