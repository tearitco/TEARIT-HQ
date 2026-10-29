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
    snprintf(out, out_sz, "M+ 1m"); /* same compile-time default the renderer itself uses */
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

/* same cut as the swatch labels: luminance under 140 keeps white writing, lighter fills use the dark writing. */
static const char *skin_tone(const char *house, const char *reldir) {
    char path[PATH_MAX], line[128];
    snprintf(path, sizeof(path), "%s/%s/sprite.csv", house, reldir);
    FILE *f = fopen(path, "r");
    if (!f) return "tone-dark";
    long sum = 0; int n = 0, r, g, b, a;
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "%d,%d,%d,%d", &r, &g, &b, &a) != 4) continue;
        if (a < 16) continue;
        sum += (r * 299 + g * 587 + b * 114) / 1000;
        n++;
    }
    fclose(f);
    if (!n) return "tone-dark";
    return (sum / n) < 140 ? "tone-dark" : "tone-light";
}

/* skin catalog for the swatch-style picker: sk_<i>_id / _label / _ring / _tone (+ an Off entry first). */
static int append_skin_rows(const char *house, char *ui, size_t cap, size_t off, const char *cur) {
    char path[PATH_MAX], line[1200];
    int n = 0;
    off += (size_t)snprintf(ui + off, cap - off, "sk_0_id=\nsk_0_label=Off (solid)\nsk_0_bar=no-skin\nsk_0_ring=%s\nsk_0_tone=tone-dark\n", cur[0] ? "" : "ring-bg");
    n = 1;
    snprintf(path, sizeof(path), "%s/&.widgits/taskbar-settings/bar_skins.pdl", house);
    FILE *f = fopen(path, "r");
    if (f) {
        while (n < 24 && fgets(line, sizeof(line), f)) {
            if (strncmp(line, "SKIN", 4) != 0) continue;
            char *a = strchr(line, '|'); if (!a) continue;
            a++; while (*a == ' ') a++;
            char *b = strchr(a, '|'); if (!b) continue;
            *b = '\0';
            size_t l = strlen(a); while (l > 0 && a[l - 1] == ' ') a[--l] = '\0';
            char *lab = b + 1; while (*lab == ' ') lab++;
            char *c = strchr(lab, '|'); if (!c) continue;
            *c = '\0';
            l = strlen(lab); while (l > 0 && lab[l - 1] == ' ') lab[--l] = '\0';
            char *left = c + 1;
            char *left_end = strchr(left, '|'); if (!left_end) continue;
            char *mid = left_end + 1; while (*mid == ' ') mid++;
            char *mid_end = strchr(mid, '|'); if (!mid_end) continue;
            *mid_end = '\0';
            l = strlen(mid); while (l > 0 && mid[l - 1] == ' ') mid[--l] = '\0';
            const char *tone = skin_tone(house, mid);
            off += (size_t)snprintf(ui + off, cap - off, "sk_%d_id=%s\nsk_%d_label=%s\nsk_%d_bar=skin-bar skin-for-%s\nsk_%d_ring=%s\nsk_%d_tone=%s\n",
                                    n, a, n, lab, n, a, n, strcmp(a, cur) == 0 ? "ring-bg" : "", n, tone);
            n++;
        }
        fclose(f);
    }
    off += (size_t)snprintf(ui + off, cap - off, "n_skins=%d\n", n);
    return (int)off;
}

static const char *g_house_for_skins = ".";
static char g_cur_skin_id[48];

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

static int read_grid_cell(const char *house) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/#.desktop/desk_grid.pdl", house);
    FILE *f = fopen(path, "r");
    int v = 80;
    if (!f) return v;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "GRID", 4) == 0 && strstr(line, "cell_px")) {
            char *p = strrchr(line, '|');
            if (p) { int n = atoi(p + 1); if (n > 0) v = n; }
            break;
        }
    }
    fclose(f);
    return v;
}

/* UI sound settings labels (BOOK 19.audio/UI-SOUNDS-AND-STARTUP-DESIGN.md): read-only view of the sound_* keys of hq_ui.pdl and the THEME rows of ui_sounds.pdl;
 * the renderer's desktop_sound_action() owns writing. Defaults mirror the player: enabled 1, volume 40, ui 1, startup 1, theme = the first THEME row. */
static size_t append_sound_labels(const char *house, char *ui, size_t cap, size_t off) {
    char path[PATH_MAX], line[512], theme[64] = "";
    int enabled = 1, vol = 40, ui_on = 1, startup = 1;
    snprintf(path, sizeof(path), "%s/#.desktop/hq_ui.pdl", house);
    FILE *f = fopen(path, "r");
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\r\n")] = '\0';
            if (strncmp(line, "sound_enabled=", 14) == 0) enabled = atoi(line + 14) != 0;
            else if (strncmp(line, "sound_volume=", 13) == 0) { vol = atoi(line + 13); if (vol < 0) vol = 0; if (vol > 100) vol = 100; }
            else if (strncmp(line, "sound_ui=", 9) == 0) ui_on = atoi(line + 9) != 0;
            else if (strncmp(line, "sound_startup=", 14) == 0) startup = atoi(line + 14) != 0;
            else if (strncmp(line, "sound_theme=", 12) == 0) snprintf(theme, sizeof(theme), "%.63s", line + 12);
        }
        fclose(f);
    }
    char label[96] = "", first[96] = "", id[64];
    snprintf(path, sizeof(path), "%s/&.widgits/ui-sounds/ui_sounds.pdl", house);
    f = fopen(path, "r");
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "THEME", 5) != 0) continue;
            char *a = strchr(line, '|');
            char *b = a ? strchr(a + 1, '|') : NULL;
            char *c = b ? strchr(b + 1, '|') : NULL;
            if (!a || !b) continue;
            snprintf(id, sizeof(id), "%.*s", (int)(b - a - 1), a + 1);
            char *t = id; while (*t == ' ') t++;
            size_t il = strlen(t); while (il && t[il - 1] == ' ') t[--il] = '\0';
            char lab[96];
            snprintf(lab, sizeof(lab), "%.*s", (int)((c ? c : line + strlen(line)) - b - 1), b + 1);
            char *lt = lab; while (*lt == ' ') lt++;
            size_t ll = strlen(lt); while (ll && (lt[ll - 1] == ' ' || lt[ll - 1] == '\n' || lt[ll - 1] == '\r')) lt[--ll] = '\0';
            if (!first[0]) snprintf(first, sizeof(first), "%s", lt);
            if (theme[0] && strcmp(t, theme) == 0) snprintf(label, sizeof(label), "%s", lt);
        }
        fclose(f);
    }
    if (!label[0]) snprintf(label, sizeof(label), "%s", first[0] ? first : "none");
    char bar[16];
    int filled = (vol + 5) / 10;
    for (int i = 0; i < 10; i++) bar[i] = i < filled ? '#' : '-';
    bar[10] = '\0';
    off += (size_t)snprintf(ui + off, cap - off, "sound_volume_label=Volume: %d%%  [%s]\n", vol, bar);
    off += (size_t)snprintf(ui + off, cap - off, "sound_enabled_label=Sound: %s\n", enabled ? "ON" : "OFF");
    off += (size_t)snprintf(ui + off, cap - off, "sound_ui_label=Menu sounds: %s\n", ui_on ? "ON" : "OFF");
    off += (size_t)snprintf(ui + off, cap - off, "sound_startup_label=Startup sound: %s\n", startup ? "ON" : "OFF");
    off += (size_t)snprintf(ui + off, cap - off, "sound_theme_label=Sound theme: %s\n", label);
    return off;
}

/* Custom theme (prism) labels, read-only view of #.desktop/livedesk_prism.pdl (written by prism_settings_op; owner 2026-10-28): shape, algorithm, hue, brightness, on/off.
 * A missing or out-of-range value shows the default rainbow (shape 1 hexagon, algo 2 spiral, hue 0, light 60, off). */
static size_t append_prism_labels(const char *house, char *ui, size_t cap, size_t off) {
    static const char *shapes[] = {"triangle", "hexagon", "diamond", "circle"};
    static const char *algos[] = {"radial", "linear", "spiral", "noise"};
    int shape = 1, algo = 2, hue = 0, light = 60, custom = 0;
    char path[PATH_MAX], line[256];
    snprintf(path, sizeof(path), "%s/#.desktop/livedesk_prism.pdl", house);
    FILE *f = fopen(path, "r");
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            char *a = strchr(line, '|'); if (!a || strncmp(line, "PRISM", 5) != 0) continue;
            a++; while (*a == ' ') a++;
            char *b = strchr(a, '|'); if (!b) continue;
            char *e = b - 1; while (e > a && *e == ' ') *e-- = '\0';
            *b++ = '\0'; while (*b == ' ') b++;
            if (strcmp(a, "shape") == 0) shape = atoi(b); else if (strcmp(a, "algo") == 0) algo = atoi(b);
            else if (strcmp(a, "hue") == 0) hue = atoi(b); else if (strcmp(a, "light") == 0) light = atoi(b);
            else if (strcmp(a, "custom") == 0) custom = atoi(b);
        }
        fclose(f);
    }
    if (shape < 0 || shape > 3) shape = 1;
    if (algo < 0 || algo > 3) algo = 2;
    if (hue < 0 || hue > 359) hue = 0;
    if (light < 0 || light > 100) light = 60;
    custom = custom != 0;
    off += (size_t)snprintf(ui + off, cap - off, "prism_custom_label=Custom theme (prism): %s (press to switch)\n", custom ? "ON" : "OFF");
    off += (size_t)snprintf(ui + off, cap - off, "prism_shape_label=Prism shape: %s\n", shapes[shape]);
    off += (size_t)snprintf(ui + off, cap - off, "prism_algo_label=Prism light: %s\n", algos[algo]);
    off += (size_t)snprintf(ui + off, cap - off, "prism_hue_label=Prism hue: %d\n", hue);
    off += (size_t)snprintf(ui + off, cap - off, "prism_light_label=Prism brightness: %d%%\n", light);
    return off;
}

static void build_ui(char *ui, size_t cap, int phase, int bg, int fg, int click_two_step, const char *font_family,
                     double opacity, double font_scale, const char *bar_label, int grid_cell) {
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
    /* "M+ 1m" is GameFont (RPG Maker's font) and the house default; name it so */
    if (strcmp(font_family, "M+ 1m") == 0)
        off += (size_t)snprintf(ui + off, cap - off, "font_label=Font: GameFont (RPG Maker, M+ 1m) - the default\n");
    else
        off += (size_t)snprintf(ui + off, cap - off, "font_label=Font: %s\n", font_family);
    off += (size_t)snprintf(ui + off, cap - off, "opacity_label=Opacity: %d%%\n", (int)(opacity * 100.0 + 0.5));
    off += (size_t)snprintf(ui + off, cap - off, "bar_skin_label=%s\n", bar_label);
    off = (size_t)append_skin_rows(g_house_for_skins, ui, cap, off, g_cur_skin_id);
    off += (size_t)snprintf(ui + off, cap - off, "size_label=Size: %d%%\n", (int)(font_scale * 100.0 + 0.5));
    off += (size_t)snprintf(ui + off, cap - off, "grid_label=Grid: %d px\n", grid_cell);
    off = append_sound_labels(g_house_for_skins, ui, cap, off);
    off = append_prism_labels(g_house_for_skins, ui, cap, off);
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
        g_house_for_skins = house;
        g_cur_skin_id[0] = '\0';
        if (strncmp(bar_label, "Bar skin: off", 13) != 0) {
            /* id again, straight from hq_ui.pdl (label may differ from id) */
            char pth[PATH_MAX], ln[200];
            snprintf(pth, sizeof(pth), "%s/#.desktop/hq_ui.pdl", house);
            FILE *hf = fopen(pth, "r");
            if (hf) {
                while (fgets(ln, sizeof(ln), hf))
                    if (strncmp(ln, "bar_skin=", 9) == 0) {
                        snprintf(g_cur_skin_id, sizeof(g_cur_skin_id), "%s", ln + 9);
                        g_cur_skin_id[strcspn(g_cur_skin_id, "\r\n")] = '\0';
                        break;
                    }
                fclose(hf);
            }
        }
        build_ui(ui, sizeof(ui), phase, bg, fg, click_two_step, font_family,
                 read_opacity(house), read_font_scale(house), bar_label, read_grid_cell(house));

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
