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
#define UIBUF 8192

/* palette names in swatch_picker_manager.c g_hex[] order */
static const char *g_name[12] = {
    "black","white","charcoal","silver","red","orange",
    "yellow","green","cyan","blue","purple","pink"
};

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

static void build_ui(char *ui, size_t cap, int phase, int bg, int fg) {
    const char *prompt =
        phase <= 0 ? "pick a background swatch" :
        phase == 1 ? "pick a text swatch"       :
                     "theme applied";
    size_t off = 0;
    off += (size_t)snprintf(ui + off, cap - off, "prompt=%s\n", prompt);
    off += (size_t)snprintf(ui + off, cap - off, "phase=%d\n", phase);
    off += (size_t)snprintf(ui + off, cap - off, "bg_name=%s\n",
                            (bg >= 0 && bg < 12) ? g_name[bg] : "-");
    off += (size_t)snprintf(ui + off, cap - off, "fg_name=%s\n",
                            (fg >= 0 && fg < 12) ? g_name[fg] : "-");
    for (int i = 0; i < 12 && off < cap; i++) {
        const char *ring = (i == bg) ? "ring-bg" : (i == fg) ? "ring-fg" : "";
        off += (size_t)snprintf(ui + off, cap - off, "sw_%d_ring=%s\n", i, ring);
    }
}

int main(int argc, char **argv) {
    const char *house = (argc > 1 && argv[1][0]) ? argv[1]
                      : (getenv("KHTPM_HOUSE") ? getenv("KHTPM_HOUSE") : ".");

    char in_path[PATH_MAX], out_path[PATH_MAX], tmp_path[PATH_MAX];
    snprintf(in_path,  sizeof(in_path),  "%s/#.desktop/taskbar_settings_state.txt", house);
    snprintf(out_path, sizeof(out_path), "%s/#.desktop/taskbar_settings_ui.txt", house);
    snprintf(tmp_path, sizeof(tmp_path), "%s/#.desktop/taskbar_settings_ui.txt.tmp", house);

    char ui[UIBUF], last[UIBUF];
    last[0] = '\0';

    for (;;) {
        int phase, bg, fg, apply;
        read_state(in_path, &phase, &bg, &fg, &apply);
        ui[0] = '\0';
        build_ui(ui, sizeof(ui), phase, bg, fg);

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
