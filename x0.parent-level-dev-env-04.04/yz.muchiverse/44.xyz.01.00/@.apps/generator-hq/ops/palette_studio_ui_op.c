/* palette_studio_ui_op: the buttons of the palette-studio window (D34 step 4, v0). One short-lived process per click; nothing stays resident (idle cost = the window only).
 *   init                    reset the session, render the preview, write the window's vars file     (button-pal.sh studio runs this before it opens the window)
 *   pick <template id>      choose a template (a row of events/gen_studio/templates.pdl)
 *   nudge <knob> <delta>    add delta (the window uses +-0.05) to that knob of the current template, clamped to the knob's range, then re-render
 *   reset                   all knobs of the current template back to 0
 *   save                    palette_studio_op save ... sheet into state/studio_saves/slotN (N rotates over 20, so the folder never grows past 20 sheets + records)
 * Option state=<dir> (anywhere after the command) redirects every file below to that folder (harness and scratch runs; the renderer appends its own two arguments after the command, which are ignored).
 * Files (all under &.widgits/palettes/state/, git-ignored): studio_session.txt, palettes-studio_ui.txt (the window's vars), studio_preview.raw + studio_preview.receipt.txt (the canvas),
 * studio_saves/. Every file is rewritten in place (tmp + rename). The op draws nothing itself: it starts palette_studio_op (which starts the real generator) with fork + exec. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/wait.h>
#define STB_IMAGE_IMPLEMENTATION
#include "../../../&.widgits/_shared-lib/stb_image.h"
#define MAXT 16
#define MAXK 5
#define SAVE_SLOTS 20
#define PW 768
#define PH 576
typedef struct { char id[40], kind[16], label[100], knobs[80]; char kn[MAXK][16]; int nk; double v[MAXK]; } Tpl;
static Tpl T[MAXT]; static int nt, cur;
static int g_saves, g_left = 1; /* g_left: the knob panel is shown (1) or collapsed (0) */
static char g_house[2048], g_state[2300], g_status[200];
static char *fld(char **p) {
    char *s = *p, *e; while (*s == ' ') s++;
    e = strchr(s, '|'); if (e) { *e = 0; *p = e + 1; } else { e = s + strlen(s); *p = e; }
    while (e > s && (e[-1] == ' ' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0;
    return s;
}
static int find_house(void) { /* .../<house>/@.apps/generator-hq/ops/+x/palette_studio_ui_op.+x : five components up */
    char buf[PATH_MAX]; ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 1); int i;
    if (n <= 0 || n >= (ssize_t)sizeof g_house) return 2;
    buf[n] = 0;
    for (i = 0; i < 5; i++) { char *s = strrchr(buf, '/'); if (!s) return 2; *s = 0; }
    snprintf(g_house, sizeof g_house, "%.2000s", buf);
    snprintf(g_state, sizeof g_state, "%s/&.widgits/palettes/state", g_house); return 0;
}
static int read_templates(void) {
    char path[2300], line[1200]; FILE *f;
    snprintf(path, sizeof path, "%s/@.apps/generator-hq/events/gen_studio/templates.pdl", g_house);
    f = fopen(path, "r"); if (!f) { fprintf(stderr, "cannot open %s\n", path); return 2; }
    while (fgets(line, sizeof line, f) && nt < MAXT) {
        char *p = line, *k, *c; Tpl *t;
        if (line[0] == '#' || line[0] == '\n') continue;
        k = fld(&p); if (strcmp(k, "TEMPLATE")) continue;
        t = &T[nt]; memset(t, 0, sizeof *t);
        snprintf(t->id, sizeof t->id, "%s", fld(&p)); snprintf(t->kind, sizeof t->kind, "%s", fld(&p)); snprintf(t->label, sizeof t->label, "%s", fld(&p)); snprintf(t->knobs, sizeof t->knobs, "%s", fld(&p));
        for (c = strtok(t->knobs, ","); c && t->nk < MAXK; c = strtok(NULL, ",")) snprintf(t->kn[t->nk++], sizeof t->kn[0], "%s", c);
        nt++;
    }
    fclose(f); return nt ? 0 : 2;
}
static double knob_lo(const char *k) { return (!strcmp(k, "warm") || !strcmp(k, "big") || !strcmp(k, "metal")) ? -1.0 : 0.0; }
static void sess_path(char *o, size_t n) { snprintf(o, n, "%s/studio_session.txt", g_state); }
static void load_session(void) {
    char p[2400], line[200]; FILE *f; int i, k;
    sess_path(p, sizeof p); f = fopen(p, "r"); if (!f) return;
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (!strncmp(line, "template=", 9)) { for (i = 0; i < nt; i++) if (!strcmp(T[i].id, line + 9)) cur = i; }
        else if (!strncmp(line, "saves=", 6)) g_saves = atoi(line + 6);
        else if (!strncmp(line, "left=", 5)) g_left = atoi(line + 5) ? 1 : 0;
        else if (!strncmp(line, "k.", 2)) { char *eq = strchr(line, '='), *dot2; if (!eq) continue; *eq = 0; dot2 = strchr(line + 2, '.'); if (!dot2) continue; *dot2 = 0;
            for (i = 0; i < nt; i++) if (!strcmp(T[i].id, line + 2)) for (k = 0; k < T[i].nk; k++) if (!strcmp(T[i].kn[k], dot2 + 1)) T[i].v[k] = atof(eq + 1); }
    }
    fclose(f);
}
static int write_atomic(const char *path, const char *data, size_t n) {
    char tmp[2500]; FILE *f; snprintf(tmp, sizeof tmp, "%s.tmp", path);
    f = fopen(tmp, "wb"); if (!f) return 2;
    if (fwrite(data, 1, n, f) != n) { fclose(f); return 2; }
    fclose(f); return rename(tmp, path) ? 2 : 0;
}
static void save_session(void) {
    char p[2400], buf[4096]; int n, i, k;
    n = snprintf(buf, sizeof buf, "template=%s\nsaves=%d\nleft=%d\n", T[cur].id, g_saves, g_left);
    for (i = 0; i < nt; i++) for (k = 0; k < T[i].nk; k++) n += snprintf(buf + n, sizeof buf - (size_t)n, "k.%s.%s=%.4f\n", T[i].id, T[i].kn[k], T[i].v[k]);
    sess_path(p, sizeof p); write_atomic(p, buf, (size_t)n);
}
static int run_argv(char **av) {
    pid_t pid = fork(); int st;
    if (pid < 0) return 2;
    if (pid == 0) { int dn = open("/dev/null", O_WRONLY); if (dn >= 0) { dup2(dn, 1); dup2(dn, 2); } execv(av[0], av); _exit(127); }
    if (waitpid(pid, &st, 0) < 0) return 2;
    return WIFEXITED(st) ? WEXITSTATUS(st) : 2;
}
/* knob options of the current template as "knob=name:delta" strings (only non-zero ones) */
static int knob_opts(char store[][64], char **av, int n) {
    int k; for (k = 0; k < T[cur].nk; k++) if (fabs(T[cur].v[k]) > 1e-9 && n < 30) { snprintf(store[k], 64, "knob=%s:%.4f", T[cur].kn[k], T[cur].v[k]); av[n++] = store[k]; } return n;
}
static int studio_cmd(const char *verb, const char *outpng, const char *name, int crop_top) { /* palette_studio_op render|save ... */
    char *av[48]; char bin[2400], tpl[2400], outdir[2400], store[MAXK][64]; int n = 0;
    snprintf(bin, sizeof bin, "%s/@.apps/generator-hq/ops/+x/palette_studio_op.+x", g_house); snprintf(tpl, sizeof tpl, "%s/@.apps/generator-hq/events/gen_studio/templates.pdl", g_house);
    av[n++] = bin; av[n++] = (char *)verb; av[n++] = tpl; av[n++] = T[cur].id;
    if (!strcmp(verb, "render")) av[n++] = (char *)outpng;
    else { snprintf(outdir, sizeof outdir, "%s/studio_saves", g_state); av[n++] = outdir; av[n++] = (char *)name; av[n++] = "sheet"; }
    n = knob_opts(store, av, n);
    if (crop_top && !strcmp(verb, "render")) av[n++] = "crop=0,0,16,12"; /* object sheets are 768 x 768: the preview shows the top 576 px */
    av[n] = NULL; return run_argv(av);
}
static int render_preview(void) {
    char png[2400], raw[2400], rcp[2400], rc_[100]; unsigned char *px; int w = 0, h = 0, c = 0, rc;
    snprintf(png, sizeof png, "%s/studio_preview.png", g_state); snprintf(raw, sizeof raw, "%s/studio_preview.raw", g_state); snprintf(rcp, sizeof rcp, "%s/studio_preview.receipt.txt", g_state);
    rc = studio_cmd("render", png, NULL, !strcmp(T[cur].kind, "objects")); if (rc) { snprintf(g_status, sizeof g_status, "render failed (exit %d)", rc); return rc; }
    px = stbi_load(png, &w, &h, &c, 4); if (!px) { snprintf(g_status, sizeof g_status, "cannot read the preview"); return 2; }
    if (w != PW || h != PH) { snprintf(g_status, sizeof g_status, "preview is %dx%d, expected %dx%d", w, h, PW, PH); stbi_image_free(px); return 2; }
    rc = write_atomic(raw, (const char *)px, (size_t)w * h * 4); stbi_image_free(px); if (rc) return rc;
    snprintf(rc_, sizeof rc_, "overlay_w=%d\noverlay_h=%d\n", w, h); write_atomic(rcp, rc_, strlen(rc_));
    unlink(png); return 0;
}
static void write_ui(void) {
    char p[2400], buf[8192]; int n = 0, i, k;
    n += snprintf(buf + n, sizeof buf - (size_t)n, "title=palette-studio\ntemplate_id=%s\ntemplate_label=%s\nn_templates=%d\n", T[cur].id, T[cur].label, nt);
    for (i = 0; i < nt; i++) n += snprintf(buf + n, sizeof buf - (size_t)n, "tp_%d_id=%s\ntp_%d_label=%s\ntp_%d_active=%s\n", i, T[i].id, i, T[i].label, i, i == cur ? "active" : "");
    n += snprintf(buf + n, sizeof buf - (size_t)n, "n_knobs=%d\n", T[cur].nk);
    for (k = 0; k < T[cur].nk; k++) { int pc = (int)lround(T[cur].v[k] * 100.0); n += snprintf(buf + n, sizeof buf - (size_t)n, "kb_%d_name=%s\nkb_%d_val=%s%d%%\n", k, T[cur].kn[k], k, pc > 0 ? "+" : "", pc); }
    n += snprintf(buf + n, sizeof buf - (size_t)n, "status=%s\ncanvas_raw=%s/studio_preview.raw\nleft_visible=%s\nknobs_state=%s\ntpl_short=%s\n", g_status, g_state, g_left ? "1" : "", g_left ? "on" : "off", T[cur].id);
    snprintf(p, sizeof p, "%s/palettes-studio_ui.txt", g_state); write_atomic(p, buf, (size_t)n);
}
int main(int argc, char **argv) {
    char d[2500]; int i, k, rc = 0;
    if (argc < 2) { fprintf(stderr, "usage: init | pick <template> | nudge <knob> <delta> | reset | save | toggle left\n"); return 2; }
    if (find_house() || read_templates()) { fprintf(stderr, "cannot find the house or the templates\n"); return 2; }
    for (i = 1; i < argc; i++) if (!strncmp(argv[i], "state=", 6)) snprintf(g_state, sizeof g_state, "%.2200s", argv[i] + 6); /* test and scratch use: every file goes to that folder, never the live window state */
    snprintf(d, sizeof d, "%s/studio_saves", g_state); mkdir(g_state, 0755); mkdir(d, 0755);
    if (strcmp(argv[1], "init")) load_session();
    g_status[0] = 0;
    if (!strcmp(argv[1], "init")) { cur = 0; g_saves = 0; snprintf(g_status, sizeof g_status, "ready: pick a template, then press + or -"); }
    else if (!strcmp(argv[1], "pick") && argc >= 3) { /* the renderer appends two arguments (package dir, house root) to every action: accept extras */ int hit = 0; for (i = 0; i < nt; i++) if (!strcmp(T[i].id, argv[2])) { cur = i; hit = 1; } if (!hit) { fprintf(stderr, "unknown template %s\n", argv[2]); return 2; } snprintf(g_status, sizeof g_status, "template %s", T[cur].id); }
    else if (!strcmp(argv[1], "nudge") && argc >= 4) {
        int hit = 0; for (k = 0; k < T[cur].nk; k++) if (!strcmp(T[cur].kn[k], argv[2])) { double v = T[cur].v[k] + atof(argv[3]), lo = knob_lo(argv[2]); if (v < lo) v = lo; if (v > 1.0) v = 1.0; if (fabs(v) < 1e-9) v = 0; T[cur].v[k] = v; hit = 1; }
        if (!hit) { fprintf(stderr, "template %s has no knob %s\n", T[cur].id, argv[2]); return 2; }
        snprintf(g_status, sizeof g_status, "%s %s", argv[2], argv[3]);
    } else if (!strcmp(argv[1], "toggle") && argc >= 3 && !strcmp(argv[2], "left")) { g_left = !g_left; snprintf(g_status, sizeof g_status, "knob panel %s", g_left ? "shown" : "hidden"); save_session(); write_ui(); return 0;
    } else if (!strcmp(argv[1], "reset")) { for (k = 0; k < T[cur].nk; k++) T[cur].v[k] = 0; snprintf(g_status, sizeof g_status, "knobs reset"); }
    else if (!strcmp(argv[1], "save")) {
        char name[40]; snprintf(name, sizeof name, "slot%d", g_saves % SAVE_SLOTS);
        rc = studio_cmd("save", NULL, name, 0);
        if (rc) snprintf(g_status, sizeof g_status, "save failed (exit %d)", rc); else { snprintf(g_status, sizeof g_status, "saved studio_saves/%s (%s)", name, T[cur].id); g_saves++; }
        save_session(); write_ui(); return rc;
    } else { fprintf(stderr, "bad command\n"); return 2; }
    rc = render_preview();
    save_session(); write_ui(); return rc;
}
