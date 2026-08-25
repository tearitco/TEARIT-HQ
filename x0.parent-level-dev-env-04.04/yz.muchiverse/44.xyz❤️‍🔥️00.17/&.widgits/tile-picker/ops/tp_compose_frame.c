/* tp_compose_frame - real CHTPM view composer for tile-picker's main
 * screen, modeled directly on &.widgits/file-menu/ops/fm_compose_frame.c
 * (read in full 2026-08-04 before writing this - see this house's own
 * feedback_chtpm_read_precedent_first.md memory: a first attempt at this
 * widget was a bespoke raw X11/GLX window with its own keyboard handling,
 * confirmed BROKEN because it never wrote into history.txt, so
 * chtpm_parser_pal.c - the real, sole owner of all nav/focus/digit-jump/
 * Enter-to-activate logic in this house - never saw a single keystroke).
 *
 * Writes ONLY pieces/apps/player_app/view.txt (never current_frame.txt
 * directly, matching every other project's compose_frame op) - the
 * chtpm parser re-substitutes ${game_map} in tile_picker_main.chtpm with
 * whatever this emits.
 *
 * One screen, one dynamic list: reads pieces/system/picker_items.txt
 * (SECTION|INDEX|GLYPH rows - same data-driven-menu convention as
 * aomorai-editor-blueprint.md §8 / pc_menu_input.c's own METHOD table)
 * and emits one real <button onClick="KEY:n"> per item - chtpm_parser_pal
 * owns arrow/digit-jump navigation over these automatically, same as
 * every other numbered menu in this house.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_LINE 2048
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MARKUP_BUF 8192
#define MAX_ITEMS 9

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

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
 * tho"): glyph widened from a single ASCII char to a UTF-8 string.
 * chtpm_rgb_render (copied wholesale from wsr-pal, unmodified) already
 * has real on-demand emoji rendering via emoji_gen_atlas.+x/
 * emoji_xtract.+x (FreeType + NotoColorEmoji.ttf) for any real UTF-8
 * emoji appearing in a <button>/<text> label - this widget just needed
 * to stop clamping glyphs to single-byte ASCII to let a real emoji
 * reach that already-working renderer. */
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
        while (end > p && (end[-1] == ' ')) end--;
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

/* pulse_state_changed - same real dual-pulse fm_compose_frame.c uses,
 * confirmed against the same TPMOS reference that file cites. */
static void pulse_state_changed(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/state_changed.txt", project_root);
    FILE *f = fopen(path, "a");
    if (f) { fprintf(f, "X\n"); fclose(f); }
    snprintf(path, sizeof(path), "%s/pieces/display/frame_changed.txt", project_root);
    f = fopen(path, "a");
    if (f) { fprintf(f, "X\n"); fclose(f); }
}

static void read_current_layout(char *out, size_t out_sz) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/display/current_layout.txt", project_root);
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    if (fgets(out, out_sz, f)) out[strcspn(out, "\r\n")] = '\0';
    fclose(f);
}

static void emit_text_line(FILE *f, const char *text) {
    char esc[MAX_LINE];
    size_t o = 0;
    for (const char *p = text; *p && o + 6 < sizeof(esc); p++) {
        if (*p == '<') { memcpy(esc + o, "&lt;", 4); o += 4; }
        else if (*p == '>') { memcpy(esc + o, "&gt;", 4); o += 4; }
        else if (*p == '&') { memcpy(esc + o, "&amp;", 5); o += 5; }
        else esc[o++] = *p;
    }
    esc[o] = '\0';
    fprintf(f, "<text label=\"%s\" /><br/>", esc);
}

static void build_picker_markup(char *out, size_t out_sz, int selected) {
    PickerItem items[MAX_ITEMS];
    char items_path[PATH_BUF];
    snprintf(items_path, sizeof(items_path), "%s/pieces/system/picker_items.txt", project_root);
    int n = load_items(items_path, items, MAX_ITEMS);
    size_t o = 0;
    for (int i = 0; i < n; i++) {
        /* REAL FIX 2026-08-04, direct instruction ("double indexed...
         * not necessary"): chtpm_parser_pal already numbers every
         * button itself for digit-jump nav (confirmed - same reason
         * fm_compose_frame.c's own build_main_menu_markup() never
         * includes a leading number in its labels either, e.g. just
         * "NEW FILE" not "1. NEW FILE"). This op's own "%d: " prefix was
         * a second, redundant index. */
        int m = snprintf(out + o, out_sz - o,
                          "<button label=\"%s%s\" onClick=\"KEY:%d\" /><br/>",
                          items[i].glyph,
                          items[i].index == selected ? " (last placed)" : "",
                          items[i].index);
        if (m > 0) o += (size_t)m;
    }
}

int main(void) {
    resolve_root();
    char tp_path[PATH_BUF];
    snprintf(tp_path, sizeof(tp_path), "%s/pieces/system/tp_state.txt", project_root);
    char last_layout[256] = "", status_line[MAX_LINE] = "", selected_str[16] = "";
    read_kv(tp_path, "last_layout", last_layout, sizeof(last_layout));
    read_kv(tp_path, "status_line", status_line, sizeof(status_line));
    read_kv(tp_path, "selected", selected_str, sizeof(selected_str));
    int selected = selected_str[0] ? atoi(selected_str) : 0;
    /* REAL, NEW 2026-08-04 ("^" activation mode, see tp_arm_placer.c):
     * armed=1 means tp_arm_placer.+x currently holds a global pointer/
     * keyboard grab waiting for a destination click or Escape - shown
     * distinctly so the picker's own screen reflects that real state. */
    char armed_str[16] = "", armed_glyph[32] = "";
    read_kv(tp_path, "armed", armed_str, sizeof(armed_str));
    read_kv(tp_path, "armed_glyph", armed_glyph, sizeof(armed_glyph));
    int armed = atoi(armed_str);

    char layout[256];
    read_current_layout(layout, sizeof(layout));

    char out_path[PATH_BUF];
    snprintf(out_path, sizeof(out_path), "%s/pieces/apps/player_app/view.txt", project_root);
    FILE *f = fopen(out_path, "w");
    if (!f) return 1;

    emit_text_line(f, "TILE PICKER");
    if (armed) {
        char armed_line[MAX_LINE];
        snprintf(armed_line, sizeof(armed_line), "^^^ ARMED: %s -- click anywhere (Esc cancels) ^^^", armed_glyph);
        emit_text_line(f, armed_line);
    }
    emit_text_line(f, "");
    char markup[MARKUP_BUF];
    build_picker_markup(markup, sizeof(markup), selected);
    fprintf(f, "%s", markup);
    emit_text_line(f, "");
    emit_text_line(f, status_line[0] ? status_line : "Pick a digit, Enter arms it (click anywhere to place, Esc cancels).");
    fclose(f);

    write_kv(tp_path, "last_layout", layout);
    pulse_state_changed();
    return 0;
}
