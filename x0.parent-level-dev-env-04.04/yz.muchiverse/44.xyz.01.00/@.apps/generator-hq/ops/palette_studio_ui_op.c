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
#define MAXT 64
#define MAXK 6
#define SAVE_SLOTS 20
#define PW 768
#define PH 576
typedef struct { char id[40], kind[16], label[100], knobs[80]; char kn[MAXK][16]; int nk; double v[MAXK]; } Tpl;
static Tpl T[MAXT]; static int nt, cur;
static int g_variant = 1; /* reseed number for ground templates: 0 = the recipe look (old), N = every block gets its own seed (D34); Roll steps it */
static int g_view = 1; /* 1 = Picker view (default), 0 = the full sheet */
static int g_grid = 1; /* faint 48 px tile gridlines drawn into the PREVIEW only (never into a saved sheet) */
static int g_saves, g_left = 1, g_cmp, g_knobs_on = 1; /* g_cmp: the preview shows the ORIGINAL (no knobs); g_knobs_on: whether knob options are passed to the render */ /* g_left: the knob panel is shown (1) or collapsed (0) */
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
static int write_atomic(const char *path, const char *data, size_t n) {
    char tmp[2500]; FILE *f; snprintf(tmp, sizeof tmp, "%s.tmp", path);
    f = fopen(tmp, "wb"); if (!f) return 2;
    if (fwrite(data, 1, n, f) != n) { fclose(f); return 2; }
    fclose(f); return rename(tmp, path) ? 2 : 0;
}
/* ---- selection, session snapshots, presets and undo (D34 Player and Select tabs) ---- */
static int g_sel_on, g_sel[4]; /* selection rectangle in the sheet's own units: cells (objects, A5) or blocks (A2) */
static int g_save_crop; /* save writes the selection as a crop instead of the whole sheet */
/* sheet size in selection units: objects 16 x 16 cells, A5 floors 8 x 16 cells, A2 ground 8 x 4 blocks of 96 x 144 px; 0 = no selection for this template (icons, cast) */
static int sel_dims(int *cols, int *rows, int *uw, int *uh) {
    const char *id = T[cur].id; size_t l = strlen(id);
    if (!strcmp(T[cur].kind, "objects")) { *cols = 16; *rows = 16; *uw = 48; *uh = 48; return 1; }
    if (!strcmp(T[cur].kind, "ground") && l >= 2 && !strcmp(id + l - 2, "a4")) return 0; /* A4 has tops and sides of different sizes: no selection */
    if (!strcmp(T[cur].kind, "ground") && l >= 2 && !strcmp(id + l - 2, "a3")) { *cols = 8; *rows = 4; *uw = 96; *uh = 96; return 1; }
    if (!strcmp(T[cur].kind, "ground")) { if (l >= 2 && !strcmp(id + l - 2, "a5")) { *cols = 8; *rows = 16; *uw = 48; *uh = 48; } else { *cols = 8; *rows = 4; *uw = 96; *uh = 144; } return 1; }
    return 0;
}
/* one session line; only_state = true for undo and presets: they restore the template, knobs, variant and selection but never the window toggles */
static void apply_line(char *line, int only_state) {
    int i, k;
    if (!strncmp(line, "template=", 9)) { for (i = 0; i < nt; i++) if (!strcmp(T[i].id, line + 9)) cur = i; }
    else if (!strncmp(line, "variant=", 8)) { g_variant = atoi(line + 8); if (g_variant < 0 || g_variant > 999999) g_variant = 1; }
    else if (!strncmp(line, "sel=", 4)) { g_sel_on = sscanf(line + 4, "%d,%d,%d,%d", &g_sel[0], &g_sel[1], &g_sel[2], &g_sel[3]) == 4; }
    else if (!strncmp(line, "k.", 2)) { char *eq = strchr(line, '='), *dot2; if (!eq) return; *eq = 0; dot2 = strchr(line + 2, '.'); if (!dot2) return; *dot2 = 0;
        for (i = 0; i < nt; i++) if (!strcmp(T[i].id, line + 2)) for (k = 0; k < T[i].nk; k++) if (!strcmp(T[i].kn[k], dot2 + 1)) T[i].v[k] = atof(eq + 1); }
    else if (only_state) return;
    else if (!strncmp(line, "saves=", 6)) g_saves = atoi(line + 6);
    else if (!strncmp(line, "left=", 5)) g_left = atoi(line + 5) ? 1 : 0;
    else if (!strncmp(line, "cmp=", 4)) g_cmp = atoi(line + 4) ? 1 : 0;
    else if (!strncmp(line, "grid=", 5)) g_grid = atoi(line + 5) ? 1 : 0;
    else if (!strncmp(line, "view=", 5)) g_view = atoi(line + 5) ? 1 : 0;
}
static void apply_text(char *text, int only_state) { char *sp = NULL, *ln; for (ln = strtok_r(text, "\n", &sp); ln; ln = strtok_r(NULL, "\n", &sp)) { ln[strcspn(ln, "\r")] = 0; apply_line(ln, only_state); } }
static void load_session(void) {
    char p[2400], line[200]; FILE *f;
    sess_path(p, sizeof p); f = fopen(p, "r"); if (!f) return;
    while (fgets(line, sizeof line, f)) { line[strcspn(line, "\r\n")] = 0; apply_line(line, 0); }
    fclose(f);
}
/* the session as text; bounded (the old fixed 4096 buffer would overflow once the template list grew) */
static size_t session_text(char *buf, size_t sz) {
    size_t n; int i, k;
    n = (size_t)snprintf(buf, sz, "template=%s\nsaves=%d\nleft=%d\ncmp=%d\ngrid=%d\nview=%d\nvariant=%d\n", T[cur].id, g_saves, g_left, g_cmp, g_grid, g_view, g_variant);
    if (n >= sz) return sz - 1;
    if (g_sel_on) { int m = snprintf(buf + n, sz - n, "sel=%d,%d,%d,%d\n", g_sel[0], g_sel[1], g_sel[2], g_sel[3]); if (m > 0 && (size_t)m < sz - n) n += (size_t)m; } else { int m = snprintf(buf + n, sz - n, "sel=none\n"); if (m > 0 && (size_t)m < sz - n) n += (size_t)m; }
    for (i = 0; i < nt; i++) for (k = 0; k < T[i].nk; k++) { int m = snprintf(buf + n, sz - n, "k.%s.%s=%.4f\n", T[i].id, T[i].kn[k], T[i].v[k]); if (m < 0 || (size_t)m >= sz - n) return n; n += (size_t)m; }
    return n;
}
static void save_session(void) { char p[2400], buf[16384]; size_t n = session_text(buf, sizeof buf); sess_path(p, sizeof p); write_atomic(p, buf, n); }
#define UNDO_MAX 20      /* snapshots kept: the file is rewritten with at most this many records */
#define UNDO_FILE_CAP 400000
static void undo_path(char *o, size_t n) { snprintf(o, n, "%s/studio_undo.txt", g_state); }
static char *slurp(const char *p, size_t cap, size_t *len) { FILE *f = fopen(p, "r"); char *b; size_t n; if (!f) return NULL; b = malloc(cap + 1); if (!b) { fclose(f); return NULL; } n = fread(b, 1, cap, f); fclose(f); b[n] = 0; *len = n; return b; }
static void undo_push(void) {
    char p[2400], snap[16384], *old, *out; size_t on = 0, sn = session_text(snap, sizeof snap), outn, skip = 0; int recs = 0, i;
    undo_path(p, sizeof p); old = slurp(p, UNDO_FILE_CAP, &on); if (!old) { old = calloc(1, 1); on = 0; }
    for (i = 0; (size_t)i + 4 <= on; i++) if (!strncmp(old + i, "---\n", 4) && (i == 0 || old[i - 1] == '\n')) recs++;
    while (recs >= UNDO_MAX) { char *q = strstr(old + skip, "---\n"); if (!q) { skip = on; break; } skip = (size_t)(q - old) + 4; recs--; }
    out = malloc(on - skip + sn + 8); if (!out) { free(old); return; }
    memcpy(out, old + skip, on - skip); outn = on - skip; memcpy(out + outn, snap, sn); outn += sn; memcpy(out + outn, "---\n", 4); outn += 4;
    write_atomic(p, out, outn); free(out); free(old);
}
/* pop the newest snapshot and restore it; returns 0 when there was one */
static int undo_pop(void) {
    char p[2400], *old, *end, *start, *rec; size_t on = 0;
    undo_path(p, sizeof p); old = slurp(p, UNDO_FILE_CAP, &on); if (!old || on < 4) { free(old); return 1; }
    end = old + on - 4; if (strncmp(end, "---\n", 4)) { free(old); return 1; }
    start = end; while (start > old && !(start[-1] == '\n' && start >= old + 4 && !strncmp(start - 4, "---\n", 4))) start--;
    if (start > old && start - 4 >= old && !strncmp(start - 4, "---\n", 4)) { /* start sits right after a separator */ } else start = old;
    rec = malloc((size_t)(end - start) + 1); if (!rec) { free(old); return 1; }
    memcpy(rec, start, (size_t)(end - start)); rec[end - start] = 0;
    write_atomic(p, old, (size_t)(start - old)); free(old);
    apply_text(rec, 1); free(rec); return 0;
}
static void preset_path(char *o, size_t n, int slot) { snprintf(o, n, "%s/studio_presets/slot%d.txt", g_state, slot); }
static int run_argv(char **av) {
    pid_t pid = fork(); int st;
    if (pid < 0) return 2;
    if (pid == 0) { int dn = open("/dev/null", O_WRONLY); if (dn >= 0) { dup2(dn, 1); dup2(dn, 2); } execv(av[0], av); _exit(127); }
    if (waitpid(pid, &st, 0) < 0) return 2;
    return WIFEXITED(st) ? WEXITSTATUS(st) : 2;
}
/* knob options of the current template as "knob=name:delta" strings (only non-zero ones) */
static int knob_opts(char store[][64], char **av, int n) {
    if (!g_knobs_on) return n; /* compare mode: render the original */
    static char sbuf[40]; if (g_sel_on && (!strcmp(T[cur].kind, "objects") || !strcmp(T[cur].kind, "ground")) && n < 30) { snprintf(sbuf, sizeof sbuf, "select=%d,%d,%d,%d", g_sel[0], g_sel[1], g_sel[2], g_sel[3]); av[n++] = sbuf; }
    static char vbuf[32]; if (g_variant && (!strcmp(T[cur].kind, "ground") || !strcmp(T[cur].kind, "cast")) && n < 30) { snprintf(vbuf, sizeof vbuf, "variant=%d", g_variant); av[n++] = vbuf; }
    int k; for (k = 0; k < T[cur].nk; k++) if (fabs(T[cur].v[k]) > 1e-9 && n < 30) { if (!strcmp(T[cur].kn[k], "scramble")) snprintf(store[k], 64, "scramble=%.4f", T[cur].v[k]); else snprintf(store[k], 64, "knob=%s:%.4f", T[cur].kn[k], T[cur].v[k]); av[n++] = store[k]; /* scramble is an option of both generators, not a bank knob */ } return n;
}
static int studio_cmd(const char *verb, const char *outpng, const char *name, int crop_top) { /* palette_studio_op render|save ... */
    char *av[48]; char bin[2400], tpl[2400], outdir[2400], store[MAXK][64]; int n = 0;
    snprintf(bin, sizeof bin, "%s/@.apps/generator-hq/ops/+x/palette_studio_op.+x", g_house); snprintf(tpl, sizeof tpl, "%s/@.apps/generator-hq/events/gen_studio/templates.pdl", g_house);
    av[n++] = bin; av[n++] = (char *)verb; av[n++] = tpl; av[n++] = T[cur].id;
    if (!strcmp(verb, "render")) av[n++] = (char *)outpng;
    else { snprintf(outdir, sizeof outdir, "%s/studio_saves", g_state); av[n++] = outdir; av[n++] = (char *)name; av[n++] = g_save_crop ? "crop" : "sheet"; }
    n = knob_opts(store, av, n);
    if (!strcmp(verb, "save") && g_save_crop) { static char cb[48]; snprintf(cb, sizeof cb, "crop=%d,%d,%d,%d", g_sel[0], g_sel[1], g_sel[2], g_sel[3]); av[n++] = cb; }
    if (crop_top && !strcmp(verb, "render")) av[n++] = "crop=0,0,16,12"; /* object sheets are 768 x 768: the preview shows the top 576 px */
    av[n] = NULL; return run_argv(av);
}
/* Picker view (owner 2026-11-05: "just show it as it looks in the rpg maker picker"). Same rule as the house picker in _.monads/_.livedesk-taskbar/ops/palettes_manager.c:
 * ONE representative per autotile kind = the block's top-left 48 px cell (A1/A2 blocks are 2x3 cells; A3/A4 2x2; A5 and B-E are 1x1, every cell is a tile). Empty
 * representatives are skipped, so the grid is packed. Scale 2x when 48 or fewer tiles, else 1x. Preview only: saved sheets are never touched. *step* = the grid step in px. */
static int g_step = 48;
static unsigned char *picker_compose(const unsigned char *src, int w, int h, int ground) {
    unsigned char *o = calloc((size_t)PW * PH * 4, 1); int a5 = (w == 384 && h == 768), a34 = (w == 768 && (h == 384 || h == 720)), bc = ground && !a5 ? 2 : 1, br = ground && !a5 ? (a34 ? 2 : 3) : 1, kx = (w / 48) / bc, ky = (h / 48) / br, n = 0, s, per, i, x, y, a, b;
    int *cx, *cy; if (!o) return NULL;
    cx = malloc(sizeof(int) * (size_t)(kx * ky + 1)); cy = malloc(sizeof(int) * (size_t)(kx * ky + 1)); if (!cx || !cy) { free(o); free(cx); free(cy); return NULL; }
    for (y = 0; y < ky; y++) for (x = 0; x < kx; x++) {
        int any = 0; for (b = 0; b < 48 && !any; b++) for (a = 0; a < 48; a++) if (src[((size_t)(y * br * 48 + b) * w + x * bc * 48 + a) * 4 + 3]) { any = 1; break; }
        if (any) { cx[n] = x * bc * 48; cy[n] = y * br * 48; n++; }
    }
    s = n <= 48 ? 2 : 1; per = PW / (48 * s); g_step = 48 * s;
    for (i = 0; i < n; i++) {
        int dx = (i % per) * 48 * s, dy = (i / per) * 48 * s; if (dy + 48 * s > PH) break;
        for (b = 0; b < 48 * s; b++) for (a = 0; a < 48 * s; a++) memcpy(o + ((size_t)(dy + b) * PW + dx + a) * 4, src + ((size_t)(cy[i] + b / s) * w + cx[i] + a / s) * 4, 4);
    }
    free(cx); free(cy); return o;
}
static int render_preview(void) {
    char png[2400], raw[2400], rcp[2400], rc_[100]; unsigned char *px; int w = 0, h = 0, c = 0, rc, objects = !strcmp(T[cur].kind, "objects");
    snprintf(png, sizeof png, "%s/studio_preview.png", g_state); snprintf(raw, sizeof raw, "%s/studio_preview.raw", g_state); snprintf(rcp, sizeof rcp, "%s/studio_preview.receipt.txt", g_state);
    g_knobs_on = !g_cmp; rc = studio_cmd("render", png, NULL, objects && !g_view); g_knobs_on = 1; if (rc) { snprintf(g_status, sizeof g_status, "render failed (exit %d)", rc); return rc; }
    px = stbi_load(png, &w, &h, &c, 4); if (!px) { snprintf(g_status, sizeof g_status, "cannot read the preview"); return 2; }
    g_step = 48;
    if (g_view && strcmp(T[cur].kind, "cast")) { unsigned char *o = picker_compose(px, w, h, !strcmp(T[cur].kind, "ground")); stbi_image_free(px); if (!o) { snprintf(g_status, sizeof g_status, "picker view failed"); return 2; } px = o; w = PW; h = PH; }
    if ((!g_view || !strcmp(T[cur].kind, "cast")) && (w != PW || h != PH) && w <= 4 * PW && h <= 4 * PH) { unsigned char *o = calloc((size_t)PW * PH * 4, 1); int yy; if (!o) { stbi_image_free(px); return 2; } for (yy = 0; yy < PH && yy < h; yy++) memcpy(o + (size_t)yy * PW * 4, px + (size_t)yy * w * 4, (size_t)(w < PW ? w : PW) * 4); stbi_image_free(px); px = o; w = PW; h = PH; } /* A5 floor sheets are 384 x 768: the full-sheet view shows the top 576 px at the left of the canvas */
    if (w != PW || h != PH) { snprintf(g_status, sizeof g_status, "preview is %dx%d, expected %dx%d", w, h, PW, PH); stbi_image_free(px); return 2; }
    if (g_grid) { int x, y, k; for (y = 0; y < h; y++) for (x = 0; x < w; x++) if (x % g_step == 0 || y % g_step == 0) { unsigned char *q = px + ((size_t)y * w + x) * 4; for (k = 0; k < 3; k++) q[k] = (unsigned char)(q[k] + (255 - q[k]) * 22 / 100); q[3] = 255; } } /* 22%% toward white on every 48 px tile edge */
    if (g_sel_on && !g_view) { int cols, rows, uw, uh; if (sel_dims(&cols, &rows, &uw, &uh)) { int x0 = g_sel[0] * uw, y0 = g_sel[1] * uh, x1 = (g_sel[0] + g_sel[2]) * uw - 1, y1 = (g_sel[1] + g_sel[3]) * uh - 1, x, y, t; for (t = 0; t < 3; t++) for (y = y0; y <= y1; y++) for (x = x0; x <= x1; x++) { if (!(x - x0 == t || x1 - x == t || y - y0 == t || y1 - y == t)) continue; if (x >= 0 && x < w && y >= 0 && y < h) { unsigned char *q = px + ((size_t)y * w + x) * 4; q[0] = 255; q[1] = 220; q[2] = 40; q[3] = 255; } } } } /* a 3 px yellow frame around the selection, preview only */
    rc = write_atomic(raw, (const char *)px, (size_t)w * h * 4); stbi_image_free(px); if (rc) return rc;
    snprintf(rc_, sizeof rc_, "overlay_w=%d\noverlay_h=%d\n", w, h); write_atomic(rcp, rc_, strlen(rc_));
    unlink(png); return 0;
}
static const char *sel_text(char *b, size_t n) { if (!g_sel_on) snprintf(b, n, "selection: whole sheet"); else snprintf(b, n, "selection: %d,%d size %dx%d", g_sel[0], g_sel[1], g_sel[2], g_sel[3]); return b; }
static void write_ui(void) {
    char sel_label[80]; sel_text(sel_label, sizeof sel_label);
    char p[2400], buf[8192]; int n = 0, i, k;
    n += snprintf(buf + n, sizeof buf - (size_t)n, "title=palette-studio\ntemplate_id=%s\ntemplate_label=%s\nn_templates=%d\n", T[cur].id, T[cur].label, nt);
    for (i = 0; i < nt; i++) n += snprintf(buf + n, sizeof buf - (size_t)n, "tp_%d_id=%s\ntp_%d_label=%s\ntp_%d_active=%s\n", i, T[i].id, i, T[i].label, i, i == cur ? "active" : "");
    n += snprintf(buf + n, sizeof buf - (size_t)n, "n_knobs=%d\n", T[cur].nk);
    for (k = 0; k < T[cur].nk; k++) { int pc = (int)lround(T[cur].v[k] * 100.0); n += snprintf(buf + n, sizeof buf - (size_t)n, "kb_%d_name=%s\nkb_%d_val=%s%d%%\n", k, T[cur].kn[k], k, pc > 0 ? "+" : "", pc); }
    n += snprintf(buf + n, sizeof buf - (size_t)n, "status=%s\ncanvas_raw=%s/studio_preview.raw\nleft_visible=%s\nknobs_state=%s\ntpl_short=%s\ncmp_state=%s\ngrid_state=%s\nview_state=%s\nvariant_val=%d\nsel_label=%s\n", g_status, g_state, g_left ? "1" : "", g_left ? "on" : "off", T[cur].id, g_cmp ? "on" : "off", g_grid ? "on" : "off", g_view ? "picker" : "sheet", g_variant, sel_label);
    snprintf(p, sizeof p, "%s/palettes-studio_ui.txt", g_state); write_atomic(p, buf, (size_t)n);
}
int main(int argc, char **argv) {
    char d[2500]; int i, k, rc = 0;
    if (argc < 2) { fprintf(stderr, "usage: init | pick <template> | nudge <knob> <delta> | reset | save | toggle left | compare | grid | view | roll [n] | undo | preset save|load N | select whole|top|bottom|left|right|rect c,r,w,h | savecrop\n"); return 2; }
    if (find_house() || read_templates()) { fprintf(stderr, "cannot find the house or the templates\n"); return 2; }
    for (i = 1; i < argc; i++) if (!strncmp(argv[i], "state=", 6)) snprintf(g_state, sizeof g_state, "%.2200s", argv[i] + 6); /* test and scratch use: every file goes to that folder, never the live window state */
    snprintf(d, sizeof d, "%s/studio_saves", g_state); mkdir(g_state, 0755); mkdir(d, 0755); snprintf(d, sizeof d, "%s/studio_presets", g_state); mkdir(d, 0755);
    if (strcmp(argv[1], "init")) load_session();
    g_status[0] = 0;
    if (!strcmp(argv[1], "nudge") || !strcmp(argv[1], "pick") || !strcmp(argv[1], "reset") || !strcmp(argv[1], "roll") || !strcmp(argv[1], "select") || (!strcmp(argv[1], "preset") && argc >= 4 && !strcmp(argv[2], "load"))) undo_push(); /* one snapshot before every change of state: Player > Undo steps back */
    if (!strcmp(argv[1], "init")) { cur = 0; g_saves = 0; snprintf(g_status, sizeof g_status, "ready: pick a template, then press + or -"); }
    else if (!strcmp(argv[1], "roll")) { int v = argc >= 3 && argv[2][0] >= 0x30 && argv[2][0] <= 0x39 ? atoi(argv[2]) : g_variant + 1; g_variant = v > 999999 ? 1 : v; g_cmp = 0; if (strcmp(T[cur].kind, "ground") && strcmp(T[cur].kind, "cast")) snprintf(g_status, sizeof g_status, "variant %d saved, but reseeding only changes ground sheets and cast crowds", g_variant); else snprintf(g_status, sizeof g_status, g_variant ? "variant %d (each block reseeded)" : "variant 0 (recipe look)", g_variant); }
    else if (!strcmp(argv[1], "undo")) { if (undo_pop()) snprintf(g_status, sizeof g_status, "nothing to undo"); else snprintf(g_status, sizeof g_status, "undone: %s", T[cur].id); }
    else if (!strcmp(argv[1], "preset") && argc >= 4 && (!strcmp(argv[2], "save") || !strcmp(argv[2], "load")) && atoi(argv[3]) >= 1 && atoi(argv[3]) <= 9) {
        char pp[2400]; int slot = atoi(argv[3]); preset_path(pp, sizeof pp, slot);
        if (!strcmp(argv[2], "save")) { char sb[16384]; size_t sn = session_text(sb, sizeof sb); if (write_atomic(pp, sb, sn)) { snprintf(g_status, sizeof g_status, "preset %d not saved", slot); } else snprintf(g_status, sizeof g_status, "preset %d saved (%s, variant %d)", slot, T[cur].id, g_variant); }
        else { size_t ln = 0; char *tx = slurp(pp, 20000, &ln); if (!tx) { snprintf(g_status, sizeof g_status, "preset %d is empty", slot); } else { g_cmp = 0; apply_text(tx, 1); free(tx); snprintf(g_status, sizeof g_status, "preset %d loaded (%s, variant %d)", slot, T[cur].id, g_variant); } }
    }
    else if (!strcmp(argv[1], "select") && argc >= 3) {
        int cols, rows, uw, uh;
        if (!sel_dims(&cols, &rows, &uw, &uh)) snprintf(g_status, sizeof g_status, "selection works on object and ground sheets");
        else if (!strcmp(argv[2], "whole")) { g_sel_on = 0; snprintf(g_status, sizeof g_status, "selection: whole sheet"); }
        else {
            int c = 0, r = 0, w = cols, h = rows, ok = 1;
            if (!strcmp(argv[2], "top")) h = rows / 2; else if (!strcmp(argv[2], "bottom")) { r = rows / 2; h = rows - rows / 2; } else if (!strcmp(argv[2], "left")) w = cols / 2; else if (!strcmp(argv[2], "right")) { c = cols / 2; w = cols - cols / 2; }
            else if (!strcmp(argv[2], "rect") && argc >= 4) ok = sscanf(argv[3], "%d,%d,%d,%d", &c, &r, &w, &h) == 4 && c >= 0 && r >= 0 && w >= 1 && h >= 1 && c + w <= cols && r + h <= rows;
            else ok = 0;
            if (!ok) { fprintf(stderr, "bad selection (whole|top|bottom|left|right|rect c,r,w,h inside %d x %d)\n", cols, rows); return 2; }
            g_sel_on = 1; g_sel[0] = c; g_sel[1] = r; g_sel[2] = w; g_sel[3] = h; g_view = 0; /* the frame is drawn on the full sheet */
            snprintf(g_status, sizeof g_status, "selected %d,%d size %dx%d", c, r, w, h);
        }
    }
    else if (!strcmp(argv[1], "savecrop")) {
        char name[40]; int cols, rows, uw, uh; if (!sel_dims(&cols, &rows, &uw, &uh) || !g_sel_on) { snprintf(g_status, sizeof g_status, "savecrop needs a selection on an object or ground sheet"); save_session(); write_ui(); return 2; }
        snprintf(name, sizeof name, "slot%d", g_saves % SAVE_SLOTS); g_save_crop = 1; rc = studio_cmd("save", NULL, name, 0); g_save_crop = 0;
        if (rc) snprintf(g_status, sizeof g_status, "crop save failed (exit %d)", rc); else { snprintf(g_status, sizeof g_status, "saved crop studio_saves/%s (%d,%d size %dx%d)", name, g_sel[0], g_sel[1], g_sel[2], g_sel[3]); g_saves++; }
        save_session(); write_ui(); return rc;
    }
    else if (!strcmp(argv[1], "view")) { g_view = !g_view; snprintf(g_status, sizeof g_status, g_view ? "picker view (one tile per kind, packed)" : "full sheet view"); }
    else if (!strcmp(argv[1], "grid")) { g_grid = !g_grid; snprintf(g_status, sizeof g_status, g_grid ? "gridlines on" : "gridlines off"); }
    else if (!strcmp(argv[1], "compare")) { g_cmp = !g_cmp; snprintf(g_status, sizeof g_status, g_cmp ? "showing the ORIGINAL (knobs off)" : "showing your TUNED sheet"); }
    else if (!strcmp(argv[1], "pick") && argc >= 3) { g_cmp = 0; g_sel_on = 0; /* the renderer appends two arguments (package dir, house root) to every action: accept extras */ int hit = 0; for (i = 0; i < nt; i++) if (!strcmp(T[i].id, argv[2])) { cur = i; hit = 1; } if (!hit) { fprintf(stderr, "unknown template %s\n", argv[2]); return 2; } snprintf(g_status, sizeof g_status, "template %s", T[cur].id); }
    else if (!strcmp(argv[1], "nudge") && argc >= 4) { g_cmp = 0;
        int hit = 0; for (k = 0; k < T[cur].nk; k++) if (!strcmp(T[cur].kn[k], argv[2])) { double v = T[cur].v[k] + atof(argv[3]), lo = knob_lo(argv[2]); if (v < lo) v = lo; if (v > 1.0) v = 1.0; if (fabs(v) < 1e-9) v = 0; T[cur].v[k] = v; hit = 1; }
        if (!hit) { fprintf(stderr, "template %s has no knob %s\n", T[cur].id, argv[2]); return 2; }
        snprintf(g_status, sizeof g_status, "%s %s", argv[2], argv[3]);
    } else if (!strcmp(argv[1], "toggle") && argc >= 3 && !strcmp(argv[2], "left")) { g_left = !g_left; snprintf(g_status, sizeof g_status, "knob panel %s", g_left ? "shown" : "hidden"); save_session(); write_ui(); return 0;
    } else if (!strcmp(argv[1], "reset")) { g_cmp = 0; for (k = 0; k < T[cur].nk; k++) T[cur].v[k] = 0; snprintf(g_status, sizeof g_status, "knobs reset"); }
    else if (!strcmp(argv[1], "save")) {
        char name[40]; snprintf(name, sizeof name, "slot%d", g_saves % SAVE_SLOTS);
        rc = studio_cmd("save", NULL, name, 0);
        if (rc) snprintf(g_status, sizeof g_status, "save failed (exit %d)", rc); else { snprintf(g_status, sizeof g_status, "saved studio_saves/%s (%s)", name, T[cur].id); g_saves++; }
        save_session(); write_ui(); return rc;
    } else { fprintf(stderr, "bad command\n"); return 2; }
    rc = render_preview();
    save_session(); write_ui(); return rc;
}
