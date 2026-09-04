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
 *  rmmv  (RPG Maker Tiles: sprite grid + dir/category/tileset choosers)
 *    Reads  <pkg>/palettes-rmmv_state.txt  TSV: <glyph>\t<label>\t<sprite_dir>
 *           <pkg>/rmmv_options.txt   pipe: ACTIVE_DIR|.. DIR|k|l TILESET|k|l
 *                                         TAB|letter|concrete ACTIVE_TILESET|k
 *                                         ACTIVE_CATEGORY|concrete
 *           <pkg>/rmmv_active.txt    tab=<letter> / tileset=<key> / dir=<key>
 *    Writes <pkg>/state/palettes-rmmv_ui.txt :
 *      n_tiles= t_<i>_glyph= t_<i>_sprite= empty=
 *      n_dirs= d_<i>_key= d_<i>_label= d_<i>_active=(pal-dir-active|"")
 *      n_tabs= c_<i>_letter= c_<i>_label= c_<i>_active=(pal-sheet-active|"")
 *      n_tilesets= s_<i>_key= s_<i>_label= s_<i>_active=(pal-tileset-active|"")
 *      active_tileset= active_category=
 *      truncated= (1 when the tile grid was capped to fit the renderer's
 *                  256-slot var table - see the CAP NOTE in the branch)
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
    int is_rmmv = (strcmp(cat, "rmmv") == 0);

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
        } else if (is_rmmv) {
            /* CAP NOTE: khtpm_core_render.c's var table is a fixed
             * KH_MAX_VARS=256 slots, silently truncating the rest (grep
             * kh_set_var). rmmv's "b"/"c" categories are 256 tiles x
             * 2 vars each = 512, which alone blows the table before the
             * chooser/count vars land -> every <repeat count="${n_*}">
             * resolves to 0 and the whole window renders empty. So the
             * choosers + counts are emitted FIRST (they always win the
             * slots) and the tile grid is capped to whatever budget is
             * left. truncated=1 signals the partial grid to the
             * template. Full 256-tile grids need KH_MAX_VARS raised in
             * the renderer (out of scope here) or a scrolllist-internal
             * grid layout that does not go through per-tile vars. */
            #define KH_VAR_CAP 2048  /* matches khtpm_core_render.c KH_MAX_VARS (raised 2026-09-04) */

            /* --- active tab letter from rmmv_active.txt --- */
            char active_tab[64] = "";
            { char p[PATH_MAX]; snprintf(p, sizeof(p), "%s/rmmv_active.txt", pkg);
              FILE *af = fopen(p, "r");
              if (af) { char l[256];
                  while (fgets(l, sizeof(l), af)) {
                      if (strncmp(l, "tab=", 4) == 0) {
                          snprintf(active_tab, sizeof(active_tab), "%s", l + 4);
                          sanitize(active_tab);
                      }
                  }
                  fclose(af);
              }
            }

            /* --- choosers from rmmv_options.txt --- */
            char active_dir[128] = "", active_tileset[128] = "", active_category[128] = "";
            static char dkey[64][128], dlab[64][128]; int nd = 0;
            static char skey[64][128], slab[64][128]; int ns = 0;
            static char cltr[64][32], clab[64][128]; int nc = 0;
            { char p[PATH_MAX]; snprintf(p, sizeof(p), "%s/rmmv_options.txt", pkg);
              FILE *of = fopen(p, "r");
              if (of) { char l[512];
                  while (fgets(l, sizeof(l), of)) {
                      char *nl = strpbrk(l, "\r\n"); if (nl) *nl = 0;
                      char *b1 = strchr(l, '|'); if (!b1) continue; *b1 = 0;
                      char *tag = l, *a = b1 + 1, *bpart = "";
                      char *b2 = strchr(a, '|'); if (b2) { *b2 = 0; bpart = b2 + 1; }
                      if (strcmp(tag, "ACTIVE_DIR") == 0)
                          snprintf(active_dir, sizeof(active_dir), "%s", a);
                      else if (strcmp(tag, "ACTIVE_TILESET") == 0)
                          snprintf(active_tileset, sizeof(active_tileset), "%s", a);
                      else if (strcmp(tag, "ACTIVE_CATEGORY") == 0)
                          snprintf(active_category, sizeof(active_category), "%s", a);
                      else if (strcmp(tag, "DIR") == 0 && nd < 64) {
                          snprintf(dkey[nd], 128, "%s", a);
                          snprintf(dlab[nd], 128, "%s", bpart[0] ? bpart : a);
                          sanitize(dkey[nd]); sanitize(dlab[nd]); nd++;
                      } else if (strcmp(tag, "TILESET") == 0 && ns < 64) {
                          snprintf(skey[ns], 128, "%s", a);
                          snprintf(slab[ns], 128, "%s", bpart[0] ? bpart : a);
                          sanitize(skey[ns]); sanitize(slab[ns]); ns++;
                      } else if (strcmp(tag, "TAB") == 0 && nc < 64) {
                          snprintf(cltr[nc], 32, "%s", a);
                          snprintf(clab[nc], 128, "%s", bpart[0] ? bpart : a);
                          sanitize(cltr[nc]); sanitize(clab[nc]); nc++;
                      }
                  }
                  fclose(of);
              }
            }

            /* scalars + choosers first - these must win the var slots */
            off += (size_t)snprintf(ui + off, (off < UIBUF) ? UIBUF - off : 0,
                                    "active_tileset=%s\nactive_category=%s\n",
                                    active_tileset, active_category);

            for (int i = 0; i < nd; i++)
                off += (size_t)snprintf(ui + off, (off < UIBUF) ? UIBUF - off : 0,
                        "d_%d_key=%s\nd_%d_label=%s\nd_%d_active=%s\n",
                        i, dkey[i], i, dlab[i],
                        i, strcmp(dkey[i], active_dir) == 0 ? "pal-dir-active" : "");
            off += (size_t)snprintf(ui + off, (off < UIBUF) ? UIBUF - off : 0, "n_dirs=%d\n", nd);

            for (int i = 0; i < nc; i++)
                off += (size_t)snprintf(ui + off, (off < UIBUF) ? UIBUF - off : 0,
                        "c_%d_letter=%s\nc_%d_label=%s\nc_%d_active=%s\n",
                        i, cltr[i], i, clab[i][0] ? clab[i] : cltr[i],
                        i, strcmp(cltr[i], active_tab) == 0 ? "pal-sheet-active" : "");
            off += (size_t)snprintf(ui + off, (off < UIBUF) ? UIBUF - off : 0, "n_tabs=%d\n", nc);

            for (int i = 0; i < ns; i++)
                off += (size_t)snprintf(ui + off, (off < UIBUF) ? UIBUF - off : 0,
                        "s_%d_key=%s\ns_%d_label=%s\ns_%d_active=%s\n",
                        i, skey[i], i, slab[i],
                        i, strcmp(skey[i], active_tileset) == 0 ? "pal-tileset-active" : "");
            off += (size_t)snprintf(ui + off, (off < UIBUF) ? UIBUF - off : 0, "n_tilesets=%d\n", ns);

            /* --- tile grid, capped to the leftover var budget --- */
            /* slots used so far: active_tileset, active_category,
             * n_dirs, n_tabs, n_tilesets  (=5)
             * + n_tiles, empty, truncated  (=3, emitted below)
             * + 3 per dir/tab/tileset row. */
            int used = 8 + 3 * (nd + nc + ns);
            int budget = KH_VAR_CAP - used - 2;   /* -2 slack */
            int max_tiles = budget > 0 ? budget / 2 : 0;

            int emitted = 0, actual = 0;
            if (f) {
                char line[PATH_MAX + 256];
                while (fgets(line, sizeof(line), f)) {
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
                    actual++;
                    if (emitted < max_tiles) {
                        off += (size_t)snprintf(ui + off, (off < UIBUF) ? UIBUF - off : 0,
                                                "t_%d_glyph=%s\nt_%d_sprite=%s\n",
                                                emitted, glyph, emitted, sprite);
                        emitted++;
                    }
                }
            }
            off += (size_t)snprintf(ui + off, (off < UIBUF) ? UIBUF - off : 0,
                                    "n_tiles=%d\nempty=%d\ntruncated=%d\n",
                                    emitted, actual == 0 ? 1 : 0,
                                    actual > emitted ? 1 : 0);
            #undef KH_VAR_CAP
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
