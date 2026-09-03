/* palettes_projector.c - UI projector for the static palettes-<cat>.xhtpm
 * windows (emojis / elements). Replaces khtpm_core_render.c's
 * g_is_palettes dbhq_inject_palette_tiles() C path for these two simple
 * sprite-grid categories.
 *
 * Reads what the UNMODIFIED palettes_manager.+x publishes:
 *   <palettes_dir>/palettes-<cat>_state.txt   TSV, one tile per line:
 *     <glyph> \t <label> \t <sprite_dir>
 * Writes:
 *   <palettes_dir>/state/palettes-<cat>_ui.txt   key=value:
 *     n_tiles=  t_<i>_glyph=  t_<i>_sprite=
 * Content-gated (only rewrites on change).
 *
 * argv (launch_module appends after the <module src> token):
 *   argv[1] = house_root   argv[2] = package_dir (= &.widgits/palettes)
 *   argv[3] = category ("emojis" | "elements")  -- from <module id="...">
 * env: KHTPM_PKG = package_dir, KHTPM_HOUSE = house_root
 *
 * NOT bash (HANDOFF-scope-nav-and-chtpm-port.md §5/§6). Idle loop
 * sleeps 400ms; never spins.
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
#define MAXTILES 4000

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
    (void)house;

    char in_path[PATH_MAX], out_path[PATH_MAX], tmp_path[PATH_MAX];
    snprintf(in_path,  sizeof(in_path),  "%s/palettes-%s_state.txt", pkg, cat);
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
        if (f) {
            char line[PATH_MAX + 256];
            while (n < MAXTILES && fgets(line, sizeof(line), f)) {
                char *t1 = strchr(line, '\t');
                if (!t1) continue;
                *t1 = 0;
                char *rest = t1 + 1;
                char *t2 = strchr(rest, '\t');
                char *glyph = line;
                char *sprite = "";
                if (t2) { *t2 = 0; sprite = t2 + 1; }   /* rest = label (unused), sprite after 2nd tab */
                sanitize(glyph);
                sanitize(sprite);
                if (!glyph[0]) continue;
                off += (size_t)snprintf(ui + off, (off < UIBUF) ? UIBUF - off : 0,
                                        "t_%d_glyph=%s\nt_%d_sprite=%s\n", n, glyph, n, sprite);
                n++;
            }
            fclose(f);
        }
        off += (size_t)snprintf(ui + off, (off < UIBUF) ? UIBUF - off : 0, "n_tiles=%d\n", n);
        off += (size_t)snprintf(ui + off, (off < UIBUF) ? UIBUF - off : 0,
                                "empty=%d\n", n == 0 ? 1 : 0);

        if (strcmp(ui, last) != 0) {
            FILE *w = fopen(tmp_path, "w");
            if (w) { fputs(ui, w); fclose(w); rename(tmp_path, out_path);
                     snprintf(last, sizeof(last), "%s", ui); }
        }
        usleep(400000);
    }
    return 0;
}
