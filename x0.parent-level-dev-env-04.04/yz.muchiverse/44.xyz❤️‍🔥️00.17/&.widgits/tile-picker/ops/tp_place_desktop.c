/* tp_place_desktop - place current brush (or glyph) onto house desktop tray
 * Usage: tp_place_desktop.+x <widget_state_dir> <desktop_root> [glyph] [name]
 * If glyph omitted, reads brush.txt from widget_state_dir.
 * Writes #.desktop/tiles/<name>/ with glyph.txt + meta.pdl
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <libgen.h>
#include <unistd.h>
#include <limits.h>
#include "self_exe.h" /* macOS leg: portable /proc/self/exe replacement */

#define PATH_BUF 4352
#define MAX_LINE 2048

static void read_kv(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t klen = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, klen) == 0 && line[klen] == '=') {
            char *v = line + klen + 1;
            v[strcspn(v, "\n")] = '\0';
            snprintf(out, out_sz, "%s", v);
            break;
        }
    }
    fclose(f);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr,
            "Usage: tp_place_desktop.+x <widget_state_dir> <desktop_root> [glyph] [name]\n");
        return 1;
    }
    const char *wdir = argv[1];
    const char *desk = argv[2];
    /* REAL FIX 2026-08-04, direct instruction ("id like to see emojis
     * tho"): glyph widened from a single ASCII char to a real UTF-8
     * string - see tp_set_brush.c's own identical fix. */
    char glyph[64] = "";
    if (argc >= 4 && argv[3][0]) {
        snprintf(glyph, sizeof(glyph), "%s", argv[3]);
    } else {
        char brush[PATH_BUF], line[MAX_LINE];
        snprintf(brush, sizeof(brush), "%s/brush.txt", wdir);
        FILE *bf = fopen(brush, "r");
        if (bf && fgets(line, sizeof(line), bf)) {
            line[strcspn(line, "\r\n")] = '\0';
            snprintf(glyph, sizeof(glyph), "%s", line);
            fclose(bf);
        } else {
            if (bf) fclose(bf);
            fprintf(stderr, "tp_place_desktop: no brush\n");
            return 1;
        }
    }
    if (!glyph[0]) return 1;

    char name[128];
    if (argc >= 5 && argv[4][0]) {
        snprintf(name, sizeof(name), "%s", argv[4]);
    } else {
        /* Emoji glyphs can't safely go in a directory name (multi-byte,
         * possibly containing characters unfriendly to some tools) - a
         * short hash of the glyph string keeps the name filesystem-safe
         * and still distinct per-glyph. Plain ASCII glyphs still get a
         * readable name, same as before this fix. */
        int printable_ascii = glyph[1] == '\0' && glyph[0] >= 'A' && glyph[0] <= 'z';
        if (printable_ascii) {
            snprintf(name, sizeof(name), "tile_%c_%ld", glyph[0], (long)time(NULL));
        } else {
            unsigned int h = 2166136261u;
            for (const char *p = glyph; *p; p++) h = (h ^ (unsigned char)*p) * 16777619u;
            snprintf(name, sizeof(name), "tile_%08x_%ld", h, (long)time(NULL));
        }
    }

    char dir[PATH_BUF], cmd[PATH_BUF];
    snprintf(dir, sizeof(dir), "%s/tiles/%s", desk, name);
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", dir);
    if (system(cmd) != 0) return 1;

    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/glyph.txt", dir);
    FILE *f = fopen(path, "w");
    if (!f) return 1;
    fprintf(f, "%s\n", glyph);
    fclose(f);

    snprintf(path, sizeof(path), "%s/meta.pdl", dir);
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "SECTION      | KEY                | VALUE\n");
        fprintf(f, "----------------------------------------\n");
        fprintf(f, "META         | piece_id           | %s\n", name);
        fprintf(f, "STATE        | kind                 | tile_stamp\n");
        fprintf(f, "STATE        | glyph                | %s\n", glyph);
        fprintf(f, "STATE        | created_at           | %ld\n", (long)time(NULL));
        /* REAL, NEW 2026-08-04, direct instruction ("add the context
         * menus that already exist from egg-pal to these by default"):
         * every desktop package gets a real, data-driven METHOD table
         * (same convention as picker_items.txt/file-menu's piece.pdl),
         * read by tp_desktop_window.c's own right-click popup - see
         * TILE_PICKER_DESIGN.md §4.5. Default is just "Close", matching
         * egg_window.c's own current default context menu exactly - not
         * inventing new default behavior, porting the existing one.
         * More methods (Open Event Editor, etc.) get appended here
         * later without touching the renderer that reads them. */
        fprintf(f, "METHOD       | Close                | CLOSE\n");
        fclose(f);
    }

    /* REAL, NEW 2026-08-28 (ENTITY-MENU-LEGACY-DEPRECATION-PLAN.md
     * Phase 1 - design decision A, a real durable generator instead of
     * the one-off "scratchpad meta_to_chtpm.py" that produced the
     * first 7 converted entities back on 2026-08-16/18 and then got
     * lost, stalling that rollout). Every new entity gets a real
     * menu.chtpm generated from the meta.pdl just written above, the
     * SAME MOMENT it's created - not a manual backfill step someone
     * has to remember to run. `tp_desktop_window_rgb.c`'s own
     * `launch_khtpm_menu()` already checks for `menu.chtpm` and routes
     * to the shared Elem/CSS renderer when one exists (confirmed real,
     * this session) - this is the other half of that bridge: make sure
     * one always exists going forward. Same house_root resolution the
     * spawn block below already does - not worth a second walk, but
     * kept inline here rather than factored out since this file
     * already duplicates that walk inline where needed (matching its
     * own existing style, not introducing a new one). */
    {
        char self_path[PATH_BUF];
        ssize_t len = self_exe_readlink(self_path, sizeof(self_path));
        if (len > 0) {
            self_path[len] = '\0';
            char step[PATH_BUF];
            snprintf(step, sizeof(step), "%s", self_path);
            char *house_root = NULL;
            for (;;) {
                char *slash = strrchr(step, '/');
                if (!slash || slash == step) break;
                *slash = '\0';
                char desk[PATH_BUF], widg[PATH_BUF];
                snprintf(desk, sizeof(desk), "%s/#.desktop", step);
                snprintf(widg, sizeof(widg), "%s/&.widgits", step);
                if (access(desk, F_OK) == 0 && access(widg, F_OK) == 0) {
                    house_root = step;
                    break;
                }
            }
            if (house_root) {
                char conv_path[PATH_BUF], conv_cmd[PATH_BUF * 2];
                snprintf(conv_path, sizeof(conv_path), "%s/*.monads/*.livedesk-taskbar/ops/meta_to_menu_chtpm.py", house_root);
                snprintf(conv_cmd, sizeof(conv_cmd), "python3 '%s' '%s' >/dev/null 2>&1", conv_path, dir);
                int rc = system(conv_cmd);
                (void)rc; /* real, honest no-op on failure - a missing menu.chtpm
                           * just means this entity falls back to the legacy
                           * popup engine, same as it always has, not a crash */
            }
        }
    }

    /* remember last place in widget state */
    char last[PATH_BUF];
    snprintf(last, sizeof(last), "%s/last_desktop_place.txt", wdir);
    f = fopen(last, "w");
    if (f) {
        fprintf(f, "path=%s\n", dir);
        fprintf(f, "glyph=%s\n", glyph);
        fclose(f);
    }

    printf("DESKTOP_TILE %s glyph=%s\n", dir, glyph);

    /* REAL, NEW 2026-08-04, direct instruction ("^ mode... wherever
     * they click... the phymoji will appear"): tp_arm_placer.c resolves
     * a real click point and, when it lands on bare desktop, passes it
     * through here via env vars (not new positional args, to avoid
     * breaking tp_menu_input.c's existing 3-arg call) so the spawned
     * tp_desktop_window.+x appears exactly where the user clicked
     * (snapped to the grid), instead of always the fixed default spot. */
    {
        const char *ix = getenv("TP_INITIAL_X");
        const char *iy = getenv("TP_INITIAL_Y");
        if (ix && ix[0] && iy && iy[0]) {
            char pos_path[PATH_BUF];
            snprintf(pos_path, sizeof(pos_path), "%s/desktop_pos.txt", dir);
            FILE *pf = fopen(pos_path, "w");
            if (pf) { fprintf(pf, "x=%s\ny=%s\n", ix, iy); fclose(pf); }
        }
    }

    /* REAL FIX 2026-08-04, direct instruction ("do u see how egg-pal
     * creates the same emoji that user picked?"): reuses the EXACT same
     * two-op pipeline 01.muchi-pals-🥚️-13.01/ops/hatch_egg.c uses to turn
     * a picked emoji into a real texture - emoji_gen_atlas.+x (FreeType +
     * NotoColorEmoji -> PNG) then emoji_xtract.+x (PNG -> NxN RGBA CSV,
     * "sprite.csv") - so tp_desktop_window.+x can load and draw a real
     * textured sprite the same way egg_window.c already does, instead of
     * only a glyph-hashed color square. Resolves ops_dir via the same
     * /proc/self/exe technique the window-spawn step below already
     * uses. Best-effort: if generation fails (missing emoji font glyph,
     * etc.), sprite.csv just won't exist and tp_desktop_window.+x falls
     * back to its existing color+title behavior, same graceful
     * degradation hatch_egg.c's own "Hatch warning: sprite generation
     * failed" comment describes for itself. */
    {
        /* 2026-08-14 consolidation: emoji tools moved to the livedesk-
         * taskbar runtime +x/ (same folder as the entity binary). Resolve
         * the house root via marker-walk, same as the spawn step below. */
        char self_path[PATH_BUF];
        ssize_t len = self_exe_readlink(self_path, sizeof(self_path));
        if (len > 0) {
            self_path[len] = '\0';
            char step[PATH_BUF];
            snprintf(step, sizeof(step), "%s", self_path);
            char *ops_dir = NULL;
            for (;;) {
                char *slash = strrchr(step, '/');
                if (!slash || slash == step) break;
                *slash = '\0';
                char desk[PATH_BUF], widg[PATH_BUF];
                snprintf(desk, sizeof(desk), "%s/#.desktop", step);
                snprintf(widg, sizeof(widg), "%s/&.widgits", step);
                if (access(desk, F_OK) == 0 && access(widg, F_OK) == 0) {
                    char ent_ops[PATH_BUF];
                    snprintf(ent_ops, sizeof(ent_ops), "%s/*.monads/*.livedesk-taskbar/ops/+x", step);
                    ops_dir = strdup(ent_ops);
                    break;
                }
            }
            if (ops_dir) {
                char png_path[PATH_BUF], csv_path[PATH_BUF], gen_cmd[PATH_BUF * 3];
                snprintf(png_path, sizeof(png_path), "%s/atlas.png", dir);
                snprintf(csv_path, sizeof(csv_path), "%s/sprite.csv", dir);
                snprintf(gen_cmd, sizeof(gen_cmd),
                         "'%s/emoji_gen_atlas.+x' '%s' '%s' >/dev/null 2>&1 && "
                         "'%s/emoji_xtract.+x' '%s' 0 64 '%s' >/dev/null 2>&1",
                         ops_dir, glyph, png_path, ops_dir, png_path, csv_path);
                int rc = system(gen_cmd);
                (void)rc;
                free(ops_dir);
            }
        }
    }

    /* REAL GUARD, added 2026-08-04, direct instruction ("make sure no
     * matter how many windows our cpu isn't taxed, add guards if
     * needed"): refuse to spawn a SECOND tp_desktop_window.+x for the
     * SAME package dir - caught live this session (a duplicate "dog"
     * window from re-running a manual test) accumulating redundant GL
     * windows/processes for no reason. `pgrep -f` against the exact
     * package path is enough since every real invocation passes the
     * package dir as its sole argument. */
    {
        char pgrep_cmd[PATH_BUF * 2];
        snprintf(pgrep_cmd, sizeof(pgrep_cmd), "pgrep -f 'tp_desktop_window_rgb.+x %s' >/dev/null 2>&1", dir);
        if (system(pgrep_cmd) == 0) {
            printf("DESKTOP_TILE %s: window already running, not spawning a duplicate\n", dir);
            return 0;
        }
    }

    /* Spawn the real, live GL window for this package - detached
     * background process, same "dispatch, not decision" fire-and-forget
     * shelling-out style egg_window.c uses for self_tick_pet(). Without
     * this, the package sits invisibly on disk (the original behavior) -
     * direct instruction 2026-08-04: tile-picker's desktop placer must be
     * visibly live, not just a file write. */
    {
        /* tp_desktop_window_rgb.+x was consolidated OUT of tile-picker
         * into the livedesk-taskbar runtime (2026-08-14) - it now lives at
         * <house_root>/*.monads/*.livedesk-taskbar/ops/+x/. Resolve the
         * house root by walking up from /proc/self/exe (this binary's own
         * install dir) until a dir holding BOTH #.desktop/ and &.widgits/
         * is found - same marker-walk khtpm_vars.sh uses. */
        char self_path[PATH_BUF];
        ssize_t len = self_exe_readlink(self_path, sizeof(self_path));
        if (len > 0) {
            self_path[len] = '\0';
            char step[PATH_BUF];
            snprintf(step, sizeof(step), "%s", self_path);
            char *house_root = NULL;
            for (;;) {
                char *slash = strrchr(step, '/');
                if (!slash || slash == step) break; /* reached /, give up */
                *slash = '\0';
                char desk[PATH_BUF], widg[PATH_BUF];
                snprintf(desk, sizeof(desk), "%s/#.desktop", step);
                snprintf(widg, sizeof(widg), "%s/&.widgits", step);
                if (access(desk, F_OK) == 0 && access(widg, F_OK) == 0) {
                    house_root = step;
                    break;
                }
            }
            if (house_root) {
                char exe_path[PATH_BUF], spawn_cmd[PATH_BUF * 2];
                snprintf(exe_path, sizeof(exe_path), "%s/*.monads/*.livedesk-taskbar/ops/+x/tp_desktop_window_rgb.+x", house_root);
                /* setsid detaches into its own session (not just backgrounded
                 * with &) so this window genuinely outlives the calling
                 * terminal/process group - same "meant to outlive its
                 * terminal session" requirement egg_window.c's own header
                 * states for itself. Confirmed necessary 2026-08-04: a plain
                 * "... &" child died the moment its parent shell session
                 * ended, even though POSIX orphan-reparenting should normally
                 * keep it alive - some process-group supervisors reap the
                 * whole group, not just the direct parent. setsid sidesteps
                 * that by removing it from that group entirely. */
                snprintf(spawn_cmd, sizeof(spawn_cmd), "setsid '%s' '%s' >/dev/null 2>&1 < /dev/null &", exe_path, dir);
                int rc = system(spawn_cmd);
                (void)rc;
            } else {
                fprintf(stderr, "tp_place_desktop: could not find house root, window not spawned\n");
            }
        } else {
            fprintf(stderr, "tp_place_desktop: could not resolve own path, window not spawned\n");
        }
    }

    return 0;
}
