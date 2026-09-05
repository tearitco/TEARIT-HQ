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

static void build_ui(char *ui, size_t cap, int phase, int bg, int fg, int click_two_step) {
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
    }
    off += (size_t)snprintf(ui + off, cap - off, "click_two_step_label=%s\n",
                            click_two_step ? "Click: 2-step" : "Click: 1-step");
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
        ui[0] = '\0';
        build_ui(ui, sizeof(ui), phase, bg, fg, click_two_step);

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
