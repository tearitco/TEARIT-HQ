/* tp_menu_input - real CHTPM input dispatcher for tile-picker's main
 * screen, modeled directly on fm_menu_input.c (read in full 2026-08-04).
 * Same real ASCII-shift un-shift fm_menu_input.c documents: chtpm_
 * parser_pal.c's send_command() relays onClick="KEY:n" as inject_raw_key
 * ('0'+n), i.e. the ASCII byte of the digit, not the raw integer n - a
 * real, confirmed house-wide behavior, not specific to file-menu.
 *
 * KEY:1..9 = pick that index's glyph from picker_items.txt, then reuse
 * tp_set_brush.+x + tp_place_desktop.+x EXACTLY as a human typing those
 * two commands in a terminal would (same ops already proven end-to-end
 * in scenarios/test_tile_desktop_place.sh) - this op does not duplicate
 * their logic, only shells out to them.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "self_exe.h" /* macOS leg: portable /proc/self/exe replacement */

#define MAX_LINE 2048
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_ITEMS 9

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void write_kv(const char *path, const char *key, const char *val) {
    char tmp[MAX_PATH];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *in = fopen(path, "r");
    FILE *out = fopen(tmp, "w");
    if (!out) return;
    int found = 0;
    if (in) {
        char line[MAX_LINE];
        size_t klen = strlen(key);
        while (fgets(line, sizeof(line), in)) {
            if (strncmp(line, key, klen) == 0 && line[klen] == '=') {
                fprintf(out, "%s=%s\n", key, val);
                found = 1;
            } else {
                fputs(line, out);
            }
        }
        fclose(in);
    }
    if (!found) fprintf(out, "%s=%s\n", key, val);
    fclose(out);
    rename(tmp, path);
}

/* REAL FIX 2026-08-04, direct instruction ("id like to see emojis
 * tho"): glyph widened from single ASCII char to a UTF-8 string - see
 * tp_compose_frame.c's own identical fix for why (chtpm_rgb_render's
 * real, already-working on-demand emoji rendering just needed the
 * clamp removed, not a new rendering feature). */
#define GLYPH_BUF 32
typedef struct { int index; char glyph[GLYPH_BUF]; } PickerItem;

static int load_items(const char *path, PickerItem *items, int max) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[256];
    int n = 0;
    while (n < max && fgets(line, sizeof(line), f)) {
        if (strncmp(line, "ITEM", 4) != 0) continue;
        char *p = strchr(line, '|');
        if (!p) continue;
        p++;
        int idx = atoi(p);
        p = strchr(p, '|');
        if (!p) continue;
        p++;
        while (*p == ' ') p++;
        char *end = p + strcspn(p, "\r\n");
        while (end > p && end[-1] == ' ') end--;
        size_t glen = (size_t)(end - p);
        if (glen == 0 || glen >= GLYPH_BUF) continue;
        if (idx > 0) {
            memcpy(items[n].glyph, p, glen);
            items[n].glyph[glen] = '\0';
            items[n].index = idx;
            n++;
        }
    }
    fclose(f);
    return n;
}

static void bump_screen(void) {
    char marker[PATH_BUF];
    snprintf(marker, sizeof(marker), "%s/pieces/display/tp_screen_changed.txt", project_root);
    FILE *mf = fopen(marker, "a");
    if (mf) { fprintf(mf, "X\n"); fclose(mf); }
}

/* REAL, NEW 2026-08-04, direct instruction ("^ mode... wherever they
 * click... the phymoji will appear" - supersedes the earlier immediate-
 * place behavior and the shelved cli_io-field idea): pressing Enter no
 * longer places anything itself. It spawns tp_arm_placer.+x (detached,
 * same setsid convention tp_place_desktop.c's own window-spawn uses),
 * which globally grabs the pointer+keyboard and resolves the real
 * destination on the next click or Escape - see tp_arm_placer.c's own
 * header comment and TILE_PICKER_DESIGN.md §5 for the full design.
 * wdir/desk resolution below is UNCHANGED from the old pick_and_place -
 * only what happens with them (arm instead of place immediately) is
 * new. */
static void arm_placer_for_glyph(const char *glyph, char *status_out, size_t status_sz) {
    char wdir[PATH_BUF];
    snprintf(wdir, sizeof(wdir), "%s/pieces/system", project_root);

    /* REAL FIX 2026-08-04, direct instruction ("pressing enter isn't
     * yet placing on desk"): a fixed "../../../#.desktop" relative path
     * assumed a specific nesting depth that doesn't match this widget's
     * real session dir (tile-picker/pieces/sessions/<id>/ - one level
     * deeper than a plain project root), so every placement silently
     * landed inside tile-picker's OWN directory instead of the real
     * house-wide #.desktop/. Fixed by reading pieces/system/
     * house_root.txt, the same real marker file button.sh's own
     * run_widget_session() already writes (copied from file-menu's own
     * button.sh, which uses this exact mechanism for the same reason -
     * resolving the house root should never be relative-path guesswork
     * once a project can run from a nested session dir). */
    char desk[PATH_BUF];
    const char *env_desk = getenv("TP_DESKTOP_ROOT");
    if (env_desk && env_desk[0]) {
        snprintf(desk, sizeof(desk), "%s", env_desk);
    } else {
        char house_root_path[PATH_BUF], house_root[PATH_BUF] = "";
        snprintf(house_root_path, sizeof(house_root_path), "%s/pieces/system/house_root.txt", project_root);
        FILE *hf = fopen(house_root_path, "r");
        if (hf) {
            if (fgets(house_root, sizeof(house_root), hf)) house_root[strcspn(house_root, "\r\n")] = '\0';
            fclose(hf);
        }
        if (house_root[0]) {
            snprintf(desk, sizeof(desk), "%s/#.desktop", house_root);
        } else {
            snprintf(desk, sizeof(desk), "%s/../../../#.desktop", project_root);
        }
    }

    char self_path[PATH_BUF], ops_dir[PATH_BUF];
    ssize_t len = self_exe_readlink(self_path, sizeof(self_path));
    if (len <= 0) {
        snprintf(status_out, status_sz, "error: cannot resolve own path");
        return;
    }
    self_path[len] = '\0';
    char *slash = strrchr(self_path, '/');
    if (slash) *slash = '\0';
    snprintf(ops_dir, sizeof(ops_dir), "%s", self_path);

    char cmd[PATH_BUF * 3];
    snprintf(cmd, sizeof(cmd), "'%s/tp_set_brush.+x' '%s' '%s' >/dev/null 2>&1",
             ops_dir, wdir, glyph);
    system(cmd);
    /* setsid: tp_arm_placer must outlive this short-lived tp_menu_input
     * invocation and hold its global grab independently, same "must
     * outlive its calling process" requirement tp_place_desktop.c's own
     * window-spawn already has. */
    snprintf(cmd, sizeof(cmd), "setsid '%s/tp_arm_placer.+x' '%s' '%s' '%s' '%s' >/dev/null 2>&1 < /dev/null &",
             ops_dir, project_root, wdir, desk, glyph);
    system(cmd);

    snprintf(status_out, status_sz, "^ arming %s...", glyph);
}

int main(int argc, char **argv) {
    resolve_root();
    int key = (argc > 1) ? atoi(argv[1]) : 0;
    /* Real, confirmed ASCII-shift un-shift - see this file's own header
     * comment and fm_menu_input.c's own identical fix. */
    if (key >= '0' && key <= '9') key -= '0';

    if (key >= 1 && key <= MAX_ITEMS) {
        PickerItem items[MAX_ITEMS];
        char items_path[PATH_BUF];
        snprintf(items_path, sizeof(items_path), "%s/pieces/system/picker_items.txt", project_root);
        int n = load_items(items_path, items, MAX_ITEMS);
        for (int i = 0; i < n; i++) {
            if (items[i].index == key) {
                char status[MAX_LINE];
                arm_placer_for_glyph(items[i].glyph, status, sizeof(status));
                char tp_path[PATH_BUF];
                snprintf(tp_path, sizeof(tp_path), "%s/pieces/system/tp_state.txt", project_root);
                /* tp_arm_placer.c owns status_line/armed from here on
                 * (it writes its own "^ ARMED..." message moments after
                 * this) - only a transient "arming..." placeholder is
                 * written here so the picker doesn't show stale text in
                 * the brief window before that process's own grab
                 * succeeds. */
                write_kv(tp_path, "status_line", status);
                break;
            }
        }
    }

    /* REAL BUG, found + fixed 2026-08-04 (direct instruction: "make sure
     * tile picker isn't running too fast" / high CPU report): this used
     * to call bump_screen() unconditionally, on every invocation -
     * including the two per-cycle key=0 no-op "pre-sync" calls
     * main_loop_chtpm.pal makes every single 16ms iteration (once before
     * the loop, once at the top of `loop:`). That meant
     * tp_screen_changed.txt grew every 16ms forever regardless of any
     * real change, so the PAL loop's own `beq x7, x8, no_render` check
     * NEVER matched - render_always fired on literally every iteration,
     * driving a full tp_compose_frame -> chtpm_parser_pal reparse ->
     * chtpm_rgb_render re-render chain continuously at ~60Hz even while
     * completely idle. Confirmed via direct measurement: chtpm_rgb_render
     * sustained ~40% CPU / chtpm_parser_pal ~500MB RES after being left
     * running idle for ~90 minutes. CONFIRMED the SAME bug exists in
     * file-menu's own fm_menu_input.c (this file's own real precedent,
     * copied faithfully, bug included) - a real, pre-existing, house-
     * wide issue, not unique to this file. Real fix: only bump the
     * screen-changed marker when a REAL key actually did something
     * (key != 0), never on the idle pre-sync calls. */
    if (key != 0) bump_screen();
    return 0;
}
