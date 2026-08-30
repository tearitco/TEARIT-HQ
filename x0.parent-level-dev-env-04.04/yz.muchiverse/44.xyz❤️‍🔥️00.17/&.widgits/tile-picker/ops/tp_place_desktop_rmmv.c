/* tp_place_desktop_rmmv - place the currently armed RMMV-tile brush
 * onto the house desktop tray as a real tile-entity (TILE-SYSTEM-
 * DESIGN.md §4b.3/§6 item 6). Real, deliberate parallel of
 * tp_place_desktop.c rather than a shared-code generalization of it:
 * the RMMV tile pipeline copies an already-rendered sprite.csv (the
 * palette manager's own per-kind thumbnail cache, see
 * palettes_manager.c's publish_rmmv()) instead of running the emoji
 * FreeType pipeline, and the meta.pdl fields it writes describe a
 * tileset/category/kind identity instead of a glyph. tp_desktop_
 * window_rgb.c needs ZERO changes for this - it already draws any
 * entity's sprite.csv generically (load_sprite_csv()), regardless of
 * what produced it.
 *
 * Usage: tp_place_desktop_rmmv.+x <widget_state_dir> <desktop_root>
 * Reads <widget_state_dir>/brush_rmmv.txt (written by
 * tp_set_brush_rmmv.+x). Writes #.desktop/tiles/<name>/ with
 * sprite.csv (copied) + meta.pdl. Honors TP_INITIAL_X/TP_INITIAL_Y env
 * vars for click-position placement, same convention tp_place_desktop.c
 * already established.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
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

/* Same house-root marker-walk every other tile-picker op already uses
 * (#.desktop/ + &.widgits/ both present). Returns a heap string via
 * strdup, or NULL. */
static char *find_house_root(void) {
    char self_path[PATH_BUF];
    ssize_t len = self_exe_readlink(self_path, sizeof(self_path));
    if (len <= 0) return NULL;
    self_path[len] = '\0';
    char step[PATH_BUF];
    snprintf(step, sizeof(step), "%s", self_path);
    for (;;) {
        char *slash = strrchr(step, '/');
        if (!slash || slash == step) return NULL;
        *slash = '\0';
        char desk[PATH_BUF], widg[PATH_BUF];
        snprintf(desk, sizeof(desk), "%s/#.desktop", step);
        snprintf(widg, sizeof(widg), "%s/&.widgits", step);
        if (access(desk, F_OK) == 0 && access(widg, F_OK) == 0) return strdup(step);
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: tp_place_desktop_rmmv.+x <widget_state_dir> <desktop_root>\n");
        return 1;
    }
    const char *wdir = argv[1];
    const char *desk = argv[2];

    char brush_path[PATH_BUF];
    snprintf(brush_path, sizeof(brush_path), "%s/brush_rmmv.txt", wdir);
    char sprite_dir[PATH_BUF] = "", tileset[64] = "", category[16] = "", kind_label[128] = "";
    read_kv(brush_path, "sprite_dir", sprite_dir, sizeof(sprite_dir));
    read_kv(brush_path, "tileset", tileset, sizeof(tileset));
    read_kv(brush_path, "category", category, sizeof(category));
    read_kv(brush_path, "kind_label", kind_label, sizeof(kind_label));
    if (!sprite_dir[0] || !tileset[0]) {
        fprintf(stderr, "tp_place_desktop_rmmv: no armed rmmv brush\n");
        return 1;
    }

    char src_sprite[PATH_BUF];
    snprintf(src_sprite, sizeof(src_sprite), "%s/sprite.csv", sprite_dir);
    if (access(src_sprite, F_OK) != 0) {
        fprintf(stderr, "tp_place_desktop_rmmv: armed brush's sprite.csv missing at %s\n", src_sprite);
        return 1;
    }

    char name[128];
    snprintf(name, sizeof(name), "tile_rmmv_%s_%s_%ld", tileset, category, (long)time(NULL));

    char dir[PATH_BUF], cmd[PATH_BUF];
    snprintf(dir, sizeof(dir), "%s/tiles/%s", desk, name);
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", dir);
    if (system(cmd) != 0) return 1;

    /* Copy the already-rendered representative thumbnail (the palette
     * manager's own real per-kind cache) - not regenerated here, same
     * "reuse, don't reinvent" convention as everything else this pass
     * traced through. */
    char cp_cmd[PATH_BUF * 2];
    snprintf(cp_cmd, sizeof(cp_cmd), "cp '%s' '%s/sprite.csv'", src_sprite, dir);
    if (system(cp_cmd) != 0) {
        fprintf(stderr, "tp_place_desktop_rmmv: failed to copy sprite.csv\n");
        return 1;
    }

    /* REAL, REQUIRED (found live 2026-08-29, testing this exact op):
     * tp_desktop_window_rgb.c silently exits at startup - clean exit
     * code 0, zero output - if glyph.txt does not exist in the package
     * dir, even though sprite.csv (the thing that actually gets drawn)
     * is present and correct. Root-caused via strace: it's an
     * undocumented dependency in existing, unmodified code, not
     * something visible from tp_desktop_window_rgb.c's own comments.
     * Every other real spawner (tp_place_desktop.c) always writes one;
     * this one didn't, since an rmmv tile has no real single-glyph
     * identity. Write a placeholder - its CONTENT is cosmetic (window
     * title fallback only, see tp_desktop_window_rgb.c's own glyph_str
     * use), its EXISTENCE is what's load-bearing. */
    {
        char glyph_path[PATH_BUF];
        snprintf(glyph_path, sizeof(glyph_path), "%s/glyph.txt", dir);
        FILE *gf = fopen(glyph_path, "w");
        if (gf) { fprintf(gf, "\xF0\x9F\xA7\xB1\n" /* 🧱 */); fclose(gf); }
    }

    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/meta.pdl", dir);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "SECTION      | KEY                | VALUE\n");
        fprintf(f, "----------------------------------------\n");
        fprintf(f, "META         | piece_id           | %s\n", name);
        fprintf(f, "STATE        | kind                 | tile_rmmv\n");
        fprintf(f, "STATE        | tileset              | %s\n", tileset);
        fprintf(f, "STATE        | category             | %s\n", category);
        fprintf(f, "STATE        | kind_label           | %s\n", kind_label);
        fprintf(f, "STATE        | created_at           | %ld\n", (long)time(NULL));
        /* Real, honest scope note (2026-08-29): no autotile_shape/
         * neighbor-recompute field yet - TILE-SYSTEM-DESIGN.md §6 item 8
         * (autotile recompute-on-neighbor-change) is separate, unbuilt
         * follow-up work. This entity places its single representative
         * kind-thumbnail exactly as picked, no edge-blending yet. */
        fprintf(f, "METHOD       | Close                | CLOSE\n");
        fclose(f);
    }

    /* Real menu.chtpm generation, same as tp_place_desktop.c's own
     * identical block (ENTITY-MENU-LEGACY-DEPRECATION-PLAN.md Phase 1) -
     * every new entity gets one the same moment it's created. */
    char *house_root = find_house_root();
    if (house_root) {
        char conv_path[PATH_BUF], conv_cmd[PATH_BUF * 2];
        snprintf(conv_path, sizeof(conv_path), "%s/*.monads/*.livedesk-taskbar/ops/meta_to_menu_chtpm.py", house_root);
        snprintf(conv_cmd, sizeof(conv_cmd), "python3 '%s' '%s' >/dev/null 2>&1", conv_path, dir);
        int rc = system(conv_cmd);
        (void)rc; /* honest no-op on failure, same fallback tp_place_desktop.c documents */
    }

    /* remember last place in widget state */
    char last[PATH_BUF];
    snprintf(last, sizeof(last), "%s/last_desktop_place_rmmv.txt", wdir);
    f = fopen(last, "w");
    if (f) {
        fprintf(f, "path=%s\n", dir);
        fprintf(f, "tileset=%s\n", tileset);
        fprintf(f, "category=%s\n", category);
        fclose(f);
    }

    printf("DESKTOP_TILE_RMMV %s tileset=%s category=%s\n", dir, tileset, category);

    /* REAL, NEW 2026-08-29, direct instruction ("parallel/forked
     * placing logic op can read from ledger, place (having no
     * knowledge of the picker window, just a reusable op)"): reads the
     * click position from the real, permanent master ledger
     * (nav_master_ledger.txt - khtpm_entity_menu_render.c writes a
     * real "RMMV_CLICK pid=... x=... y=..." line the instant it
     * captures the click, synchronously, decoupled from whether THIS
     * op ever runs or succeeds) instead of TP_INITIAL_X/Y env vars.
     * This op now has zero dependency on how it was invoked or by
     * what - a real "does one thing, reads real state, no caller-
     * specific plumbing" op, matching this house's own convention
     * elsewhere (compare tp_desktop_window_rgb.c reading desktop_pos.
     * txt rather than being told its own position by whatever spawned
     * it). Scans the WHOLE ledger for the LAST matching line, since
     * it's real, permanent, append-only history, not a single-purpose
     * transient file. */
    {
        char ix[32] = "", iy[32] = "";
        char ledger_path[PATH_BUF];
        snprintf(ledger_path, sizeof(ledger_path), "%s/nav_master_ledger.txt", desk);
        FILE *lf = fopen(ledger_path, "r");
        if (lf) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), lf)) {
                if (strncmp(line, "RMMV_CLICK ", 11) != 0) continue;
                char *xp = strstr(line, " x=");
                char *yp = strstr(line, " y=");
                if (xp) { sscanf(xp + 3, "%31s", ix); }
                if (yp) { sscanf(yp + 3, "%31s", iy); }
            }
            fclose(lf);
        }
        if (ix[0] && iy[0]) {
            char pos_path[PATH_BUF];
            snprintf(pos_path, sizeof(pos_path), "%s/desktop_pos.txt", dir);
            FILE *pf = fopen(pos_path, "w");
            if (pf) { fprintf(pf, "x=%s\ny=%s\n", ix, iy); fclose(pf); }

            /* REAL, NEW 2026-08-29, direct instruction ("it should say
             * what coord was last placed on desktop if something was
             * placed"): reuses the same armed-note file/poll mechanism
             * (khtpm_entity_menu_render.c already watches this path and
             * swaps the picker's title text on change) - tp_arm_placer_
             * rmmv.c unlinks it on every real exit, this write here
             * replaces that with a real "placed" message instead of
             * leaving it simply gone, so the picker shows real feedback
             * either way (armed, placed, or idle - never silent). */
            char note_path[PATH_BUF];
            snprintf(note_path, sizeof(note_path), "%s/rmmv_armed.txt", wdir);
            FILE *nf = fopen(note_path, "w");
            if (nf) {
                fprintf(nf, "Placed %s/%s \"%s\" at (%s,%s)\n", tileset, category, kind_label, ix, iy);
                fclose(nf);
            }
        }
    }

    /* Real duplicate-spawn guard - REAL FIX 2026-08-29 vs. tp_place_
     * desktop.c's own identical original pattern: 'tp_desktop_window_
     * rgb.+x' is the literal FILENAME (this house's real ".+x" binary-
     * suffix convention), but `pgrep -f` treats its argument as
     * EXTENDED REGEX - the unescaped '.' (any char) and '+' (1-or-more
     * of preceding) in "rgb.+x" mean this was never actually matching
     * the literal filename, it was matching "rgb" + any-chars-greedy +
     * literal "x" + the dir path, ANYWHERE in ANY process's cmdline -
     * found live while testing this exact op (false "already running"
     * on demonstrably fresh, never-before-spawned directory names).
     * Real fix: escape the regex metacharacters so this actually means
     * what it says. */
    {
        char pgrep_cmd[PATH_BUF * 2];
        snprintf(pgrep_cmd, sizeof(pgrep_cmd), "pgrep -f 'tp_desktop_window_rgb\\.\\+x %s' >/dev/null 2>&1", dir);
        if (system(pgrep_cmd) == 0) {
            printf("DESKTOP_TILE_RMMV %s: window already running, not spawning a duplicate\n", dir);
            free(house_root);
            return 0;
        }
    }

    /* Spawn the real, live GL entity window - same setsid-detach
     * convention tp_place_desktop.c uses (outlive the calling terminal). */
    if (house_root) {
        char exe_path[PATH_BUF], spawn_cmd[PATH_BUF * 2];
        snprintf(exe_path, sizeof(exe_path), "%s/*.monads/*.livedesk-taskbar/ops/+x/tp_desktop_window_rgb.+x", house_root);
        snprintf(spawn_cmd, sizeof(spawn_cmd), "setsid '%s' '%s' >/dev/null 2>&1 < /dev/null &", exe_path, dir);
        int rc = system(spawn_cmd);
        (void)rc;
        free(house_root);
    } else {
        fprintf(stderr, "tp_place_desktop_rmmv: could not find house root, window not spawned\n");
    }

    return 0;
}
