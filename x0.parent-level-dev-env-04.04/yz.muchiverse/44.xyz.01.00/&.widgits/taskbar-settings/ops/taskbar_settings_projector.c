/* taskbar_settings_projector.c - the <module> UI projector for
 * taskbar-settings-pal.xhtpm.
 *
 * WHAT THIS IS: mirrors db-hq-actors-pal/pal/actors_projector.pal and
 * events-hq/ops/evhq_projector.c - a small process the renderer forks
 * from a <module> tag. It READS what the UNMODIFIED
 * swatch_picker_manager.c already publishes and WRITES a key=value UI
 * file the static template consumes via vars=/${...}. It never touches
 * the manager, the renderer or the old taskbar_settings.chtpm.
 *
 * Compiled C (not .pal) purely for the content-gated write (keep the
 * last-written buffer, only rewrite on change) - the same reason
 * evhq_projector.c is C. The logic itself is trivial.
 *
 * INPUT   <house>/#.desktop/taskbar_settings_state.txt   (manager writes)
 *           phase=<0|1|2>   0=picking bg, 1=picking fg, 2=applied
 *           bg=<0..11 or -1> chosen background swatch index
 *           fg=<0..11 or -1> chosen foreground swatch index
 *           apply=<0|1>      1 once both are chosen (manager then execs
 *                            apply_theme_op and exits)
 *
 * OUTPUT  <house>/#.desktop/taskbar_settings_ui.txt   (this projector)
 *           prompt=<status string for the window title bar>
 *           phase=<0|1|2>
 *           bg_name=<palette name or ->     fg_name=<palette name or ->
 *           sw_0_ring .. sw_11_ring = "ring-bg" | "ring-fg" | ""
 *
 * argv (launch_module appends these after the <module src> tokens):
 *   argv[1] = house_root   argv[2] = package_dir
 * env: KHTPM_HOUSE / KHTPM_PKG also set by launch_module().
 */
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#define UIBUF 16384
#define MAX_SWATCHES 64

/* REAL, NEW 2026-09-04, direct live request ("can we add grey and
 * brown to swatch colors... that shouldn't be hardcoded, should be
 * from layout/module") - name/hex list read from the same swatches.pdl
 * swatch_picker_manager.c reads, instead of a compiled-in name array.
 * Published per-index (sw_<i>_name / sw_<i>_hex) plus n_swatches so
 * taskbar-settings-pal.xhtpm can drive a <repeat> instead of a fixed
 * set of 12 hardcoded <item> tags. */
static char g_name_buf[MAX_SWATCHES][32];
static char g_hex_buf[MAX_SWATCHES][8];
static int g_n_swatches = 0;

static void load_swatches(const char *house) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/&.widgits/taskbar-settings/swatches.pdl", house);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[256];
    while (g_n_swatches < MAX_SWATCHES && fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        char *bar = strchr(line, '|');
        if (!bar) continue;
        *bar = '\0';
        char *hex = bar + 1;
        hex[strcspn(hex, "\r\n")] = '\0';
        if (hex[0] != '#' || strlen(hex) != 7) continue; /* honest skip - malformed row */
        snprintf(g_name_buf[g_n_swatches], sizeof(g_name_buf[0]), "%s", line);
        snprintf(g_hex_buf[g_n_swatches], sizeof(g_hex_buf[0]), "%s", hex);
        g_n_swatches++;
    }
    fclose(f);
}

/* REAL, NEW 2026-09-04, direct live request ("single click vs double
 * click... was it added to settings yet") - reads the same house-wide
 * click_two_step key khtpm_core_render.c's own desktop_load_click_
 * two_step() reads, purely to publish a real, current-state label for
 * the new CLICK_TWOSTEP_TOGGLE toggle button - never writes it (the
 * renderer's own desktop_toggle_click_two_step() owns writing). */
static int read_click_two_step(const char *house) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/#.desktop/hq_ui.pdl", house);
    FILE *f = fopen(path, "r");
    if (!f) return 1; /* same real compile-time default the renderer itself uses */
    char line[128];
    int val = 1;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "click_two_step=", 15) == 0) { val = atoi(line + 15) != 0; break; }
    }
    fclose(f);
    return val;
}

/* REAL, NEW 2026-09-10, direct request ("when should we add font
 * picker to settings... lets do the build") - same real precedent as
 * read_click_two_step() just above: reads the house-wide font_family
 * key purely to publish the CURRENT choice as a label for the Font
 * </> stepper buttons, never writes it (khtpm_core_render.c's own
 * desktop_set_font_family() owns writing). */
static void read_font_family(const char *house, char *out, size_t out_sz) {
    snprintf(out, out_sz, "DejaVu Sans"); /* same compile-time default the renderer itself uses */
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/#.desktop/hq_ui.pdl", house);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "font_family=", 12) == 0) {
            char *v = line + 12;
            v[strcspn(v, "\r\n")] = '\0';
            if (v[0]) snprintf(out, out_sz, "%s", v);
            break;
        }
    }
    fclose(f);
}

/* 2026-10-08 (owner: settings window needs a visible, usable change):
 * publish the CURRENT opacity and UI size as text so the buttons can sit
 * next to the value they change. Read-only, same as the readers above. */
static double read_opacity(const char *house) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/#.desktop/livedesk_theme.pdl", house);
    FILE *f = fopen(path, "r");
    if (!f) return 1.0;
    char line[160];
    double v = 1.0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "COLOR", 5) != 0 || !strstr(line, "opacity")) continue;
        char *bar = strrchr(line, '|');
        if (bar) v = atof(bar + 1);
        break;
    }
    fclose(f);
    return v;
}

static double read_font_scale(const char *house) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/#.desktop/hq_ui.pdl", house);
    FILE *f = fopen(path, "r");
    if (!f) return 1.0;
    char line[128];
    double v = 1.0;
    while (fgets(line, sizeof(line), f))
        if (strncmp(line, "font_scale=", 11) == 0) { v = atof(line + 11); break; }
    fclose(f);
    return v;
}

/* readable label on any swatch: light text on dark fills, dark on light. */
static const char *tone_for_hex(const char *hex) {
    unsigned r = 0, g = 0, b = 0;
    if (sscanf(hex, "#%2x%2x%2x", &r, &g, &b) != 3) return "tone-light";
    return (r * 299 + g * 587 + b * 114) / 1000 < 140 ? "tone-dark" : "tone-light";
}

/* current bar skin id (hq_ui.pdl bar_skin=) and its catalog label (bar_skins.pdl); read-only. */
static void read_bar_skin_label(const char *house, char *out, size_t out_sz) {
    char path[PATH_MAX], line[1200], id[48] = "";
    snprintf(out, out_sz, "Bar skin: off");
    snprintf(path, sizeof(path), "%s/#.desktop/hq_ui.pdl", house);
    FILE *f = fopen(path, "r");
    if (!f) return;
    while (fgets(line, sizeof(line), f))
        if (strncmp(line, "bar_skin=", 9) == 0) {
            snprintf(id, sizeof(id), "%s", line + 9);
            id[strcspn(id, "\r\n")] = '\0';
            break;
        }
    fclose(f);
    if (!id[0]) return;
    snprintf(path, sizeof(path), "%s/&.widgits/taskbar-settings/bar_skins.pdl", house);
    f = fopen(path, "r");
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "SKIN", 4) != 0) continue;
        char *a = strchr(line, '|'); if (!a) continue;
        a++; while (*a == ' ') a++;
        char *b = strchr(a, '|'); if (!b) continue;
        *b = '\0';
        size_t l = strlen(a); while (l > 0 && a[l - 1] == ' ') a[--l] = '\0';
        if (strcmp(a, id) != 0) continue;
        char *lab = b + 1; while (*lab == ' ') lab++;
        char *c = strchr(lab, '|'); if (!c) continue;
        *c = '\0';
        l = strlen(lab); while (l > 0 && lab[l - 1] == ' ') lab[--l] = '\0';
        snprintf(out, out_sz, "Bar skin: %s", lab);
        break;
    }
    fclose(f);
}

static void read_state(const char *path, int *phase, int *bg, int *fg, int *apply) {
    *phase = 0; *bg = -1; *fg = -1; *apply = 0;
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        if      (strncmp(line, "phase=", 6) == 0) *phase = atoi(line + 6);
        else if (strncmp(line, "bg=",    3) == 0) *bg    = atoi(line + 3);
        else if (strncmp(line, "fg=",    3) == 0) *fg    = atoi(line + 3);
        else if (strncmp(line, "apply=", 6) == 0) *apply = atoi(line + 6);
    }
    fclose(f);
}

static void build_ui(char *ui, size_t cap, int phase, int bg, int fg, int click_two_step, const char *font_family,
                     double opacity, double font_scale, const char *bar_label) {
    const char *prompt =
        phase <= 0 ? "pick a background swatch" :
        phase == 1 ? "pick a text swatch"       :
                     "theme applied";
    size_t off = 0;
    off += (size_t)snprintf(ui + off, cap - off, "prompt=%s\n", prompt);
    off += (size_t)snprintf(ui + off, cap - off, "phase=%d\n", phase);
    off += (size_t)snprintf(ui + off, cap - off, "bg_name=%s\n",
                            (bg >= 0 && bg < g_n_swatches) ? g_name_buf[bg] : "-");
    off += (size_t)snprintf(ui + off, cap - off, "fg_name=%s\n",
                            (fg >= 0 && fg < g_n_swatches) ? g_name_buf[fg] : "-");
    off += (size_t)snprintf(ui + off, cap - off, "n_swatches=%d\n", g_n_swatches);
    for (int i = 0; i < g_n_swatches && off < cap; i++) {
        const char *ring = (i == bg) ? "ring-bg" : (i == fg) ? "ring-fg" : "";
        off += (size_t)snprintf(ui + off, cap - off, "sw_%d_ring=%s\n", i, ring);
        off += (size_t)snprintf(ui + off, cap - off, "sw_%d_name=%s\n", i, g_name_buf[i]);
        off += (size_t)snprintf(ui + off, cap - off, "sw_%d_hex=%s\n", i, g_hex_buf[i]);
        off += (size_t)snprintf(ui + off, cap - off, "sw_%d_tone=%s\n", i, tone_for_hex(g_hex_buf[i]));
    }
    off += (size_t)snprintf(ui + off, cap - off, "click_two_step_label=%s\n",
                            click_two_step ? "Click: 2-step" : "Click: 1-step");
    off += (size_t)snprintf(ui + off, cap - off, "font_family=%s\n", font_family);
    off += (size_t)snprintf(ui + off, cap - off, "font_label=Font: %s\n", font_family);
    off += (size_t)snprintf(ui + off, cap - off, "opacity_label=Opacity: %d%%\n", (int)(opacity * 100.0 + 0.5));
    off += (size_t)snprintf(ui + off, cap - off, "bar_skin_label=%s\n", bar_label);
    off += (size_t)snprintf(ui + off, cap - off, "size_label=Size: %d%%\n", (int)(font_scale * 100.0 + 0.5));
    off += (size_t)snprintf(ui + off, cap - off, "colors_label=Colors - background: %s, text: %s. Pick below (green ring = background, gold ring = text)\n",
                            (bg >= 0 && bg < g_n_swatches) ? g_name_buf[bg] : "-",
                            (fg >= 0 && fg < g_n_swatches) ? g_name_buf[fg] : "-");
}

int main(int argc, char **argv) {
    const char *house = (argc > 1 && argv[1][0]) ? argv[1]
                      : (getenv("KHTPM_HOUSE") ? getenv("KHTPM_HOUSE") : ".");
    load_swatches(house);

    char in_path[PATH_MAX], out_path[PATH_MAX], tmp_path[PATH_MAX];
    snprintf(in_path,  sizeof(in_path),  "%s/#.desktop/taskbar_settings_state.txt", house);
    snprintf(out_path, sizeof(out_path), "%s/#.desktop/taskbar_settings_ui.txt", house);
    snprintf(tmp_path, sizeof(tmp_path), "%s/#.desktop/taskbar_settings_ui.txt.tmp", house);

    char ui[UIBUF], last[UIBUF];
    last[0] = '\0';

    for (;;) {
        int phase, bg, fg, apply;
        read_state(in_path, &phase, &bg, &fg, &apply);
        int click_two_step = read_click_two_step(house);
        char font_family[64];
        read_font_family(house, font_family, sizeof(font_family));
        ui[0] = '\0';
        char bar_label[96];
        read_bar_skin_label(house, bar_label, sizeof(bar_label));
        build_ui(ui, sizeof(ui), phase, bg, fg, click_two_step, font_family,
                 read_opacity(house), read_font_scale(house), bar_label);

        if (strcmp(ui, last) != 0) {              /* content-gated write */
            FILE *f = fopen(tmp_path, "w");
            if (f) {
                fputs(ui, f);
                fclose(f);
                rename(tmp_path, out_path);
                snprintf(last, sizeof(last), "%s", ui);
            }
        }
        usleep(300000);
    }
    return 0;
}
