/* palettes_projector.c - UI projector for the static palettes-<cat>.xhtpm
 * windows. Replaces khtpm_core_render.c's g_is_palettes
 * dbhq_inject_palette_tiles() C path for the ported categories.
 *
 * Modes (selected by argv[3] / <module id="...">):
 *
 *  emojis | elements  (sprite-grid)
 *    Reads  <pkg>/palettes-<cat>_state.txt   TSV: <glyph>\t<label>\t<sprite_dir>
 *    Writes <pkg>/state/palettes-<cat>_ui.txt :
 *      n_tiles=  t_<i>_glyph=  t_<i>_sprite=  empty=
 *
 *  piececraft  (read-only status readout)
 *    Reads  <pkg>/palettes-piececraft-hq_state.txt  TSV: noop\t<text>\t
 *      (published by the piececraft-hq app itself - palettes_manager.c
 *       has no "piececraft" branch, so this template carries NO
 *       palettes_manager <module>, only this projector.)
 *    Writes <pkg>/state/palettes-piececraft_ui.txt :
 *      n_rows=  r_<i>_text=  empty=
 *
 *  debug  (console: mixed action buttons + log lines)
 *    Reads  <pkg>/palettes-debug_state.txt  TSV: <verb>\t<label>\t
 *      verbs: toggle:<N> and clear -> action rows; noop -> log rows
 *    Writes <pkg>/state/palettes-debug_ui.txt :
 *      n_rows=  r_<i>_label=  r_<i>_is_btn=  r_<i>_is_log=  r_<i>_action=  empty=
 *
 * All modes content-gated (only rewrites on change). Idle loop sleeps
 * 400ms; never spins. NOT bash (HANDOFF-scope-nav-and-chtpm-port.md
 * §5/§6).
 *
 * argv: argv[1] = house_root   argv[2] = package_dir (= &.widgits/palettes)
 *       argv[3] = category      env: KHTPM_PKG / KHTPM_HOUSE fallbacks
 */
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#define UIBUF (256 * 1024)
#define MAXROWS 4000

static void sanitize(char *s) {
    for (char *p = s; *p; p++) {
        if (*p == '\r' || *p == '\n') { *p = 0; break; }
        if (*p == '|') *p = '/';   /* frame-dump field separator */
    }
}

int main(int argc, char **argv) {
    const char *house = (argc > 1 && argv[1][0]) ? argv[1]
                      : (getenv("KHTPM_HOUSE") ? getenv("KHTPM_HOUSE") : ".");
    const char *pkg = (argc > 2 && argv[2][0]) ? argv[2]
                    : (getenv("KHTPM_PKG") ? getenv("KHTPM_PKG") : ".");
    const char *cat = (argc > 3 && argv[3][0]) ? argv[3] : "emojis";

    int is_piececraft = (strcmp(cat, "piececraft") == 0);
    int is_debug = (strcmp(cat, "debug") == 0);

    char in_path[PATH_MAX], out_path[PATH_MAX], tmp_path[PATH_MAX];
    if (is_piececraft)
        snprintf(in_path, sizeof(in_path), "%s/palettes-piececraft-hq_state.txt", pkg);
    else
        snprintf(in_path, sizeof(in_path), "%s/palettes-%s_state.txt", pkg, cat);
    snprintf(out_path, sizeof(out_path), "%s/state/palettes-%s_ui.txt", pkg, cat);
    snprintf(tmp_path, sizeof(tmp_path), "%s/state/palettes-%s_ui.txt.tmp", pkg, cat);
    { char d[PATH_MAX]; snprintf(d, sizeof(d), "%s/state", pkg);
      char cmd[PATH_MAX + 16]; snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", d); int r = system(cmd); (void)r; }

    static char ui[UIBUF], last[UIBUF];
    last[0] = 0;

    for (;;) {
        size_t off = 0;
        int n = 0;
        FILE *f = fopen(in_path, "r");

        if (is_piececraft) {
            if (f) {
                char line[PATH_MAX + 256];
                while (n < MAXROWS && fgets(line, sizeof(line), f)) {
                    char *t1 = strchr(line, '\t');
                    if (!t1) continue;
                    char *rest = t1 + 1;                 /* line = verb (noop) */
                    char *t2 = strchr(rest, '\t');
                    if (t2) *t2 = 0;
                    sanitize(rest);
                    if (!rest[0]) continue;
                    off += (size_t)snprintf(ui + off, (off < UIBUF) ? UIBUF - off : 0,
                                            "r_%d_text=%s\n", n, rest);
                    n++;
                }
            }
            off += (size_t)snprintf(ui + off, (off < UIBUF) ? UIBUF - off : 0, "n_rows=%d\n", n);
            off += (size_t)snprintf(ui + off, (off < UIBUF) ? UIBUF - off : 0,
                                    "empty=%d\n", n == 0 ? 1 : 0);
        } else if (is_debug) {
            if (f) {
                char line[PATH_MAX + 256];
                while (n < MAXROWS && fgets(line, sizeof(line), f)) {
                    char *t1 = strchr(line, '\t');
                    if (!t1) continue;
                    *t1 = 0;
                    char *verb = line;
                    char *label = t1 + 1;
                    char *t2 = strchr(label, '\t');
                    if (t2) *t2 = 0;
                    sanitize(verb);
                    sanitize(label);

                    int is_btn = 0;
                    char action[PATH_MAX + 128];
                    action[0] = 0;
                    if (strncmp(verb, "toggle:", 7) == 0) {
                        is_btn = 1;
                        snprintf(action, sizeof(action),
                                 "'%s/&.widgits/palettes/palettes_menu.sh' 'debug-toggle' '%s'",
                                 house, verb + 7);
                    } else if (strcmp(verb, "clear") == 0) {
                        is_btn = 1;
                        snprintf(action, sizeof(action),
                                 "'%s/&.widgits/palettes/palettes_menu.sh' 'debug-clear'",
                                 house);
                    }
                    off += (size_t)snprintf(ui + off, (off < UIBUF) ? UIBUF - off : 0,
                                            "r_%d_label=%s\n"
                                            "r_%d_is_btn=%d\n"
                                            "r_%d_is_log=%d\n"
                                            "r_%d_action=%s\n",
                                            n, label,
                                            n, is_btn ? 1 : 0,
                                            n, is_btn ? 0 : 1,
                                            n, action);
                    n++;
                }
            }
            off += (size_t)snprintf(ui + off, (off < UIBUF) ? UIBUF - off : 0, "n_rows=%d\n", n);
            off += (size_t)snprintf(ui + off, (off < UIBUF) ? UIBUF - off : 0,
                                    "empty=%d\n", n == 0 ? 1 : 0);
        } else {
            if (f) {
                char line[PATH_MAX + 256];
                while (n < MAXROWS && fgets(line, sizeof(line), f)) {
                    char *t1 = strchr(line, '\t');
                    if (!t1) continue;
                    *t1 = 0;
                    char *rest = t1 + 1;
                    char *t2 = strchr(rest, '\t');
                    char *glyph = line;
                    char *sprite = "";
                    if (t2) { *t2 = 0; sprite = t2 + 1; }
                    sanitize(glyph);
                    sanitize(sprite);
                    if (!glyph[0]) continue;
                    off += (size_t)snprintf(ui + off, (off < UIBUF) ? UIBUF - off : 0,
                                            "t_%d_glyph=%s\nt_%d_sprite=%s\n", n, glyph, n, sprite);
                    n++;
                }
            }
            off += (size_t)snprintf(ui + off, (off < UIBUF) ? UIBUF - off : 0, "n_tiles=%d\n", n);
            off += (size_t)snprintf(ui + off, (off < UIBUF) ? UIBUF - off : 0,
                                    "empty=%d\n", n == 0 ? 1 : 0);
        }

        if (f) fclose(f);

        if (strcmp(ui, last) != 0) {
            FILE *w = fopen(tmp_path, "w");
            if (w) { fputs(ui, w); fclose(w); rename(tmp_path, out_path);
                     snprintf(last, sizeof(last), "%s", ui); }
        }
        usleep(400000);
    }
    return 0;
}
