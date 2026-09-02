/* ee_compose_frame - Event Editor → view.txt + gui_state (continuous nav)
 *
 * CHTPM digit_accum numbers (ONE stream, never restart per section):
 *   1-8   methods   (static in event_editor.chtpm)
 *   9-12  pages
 *   13-16 fields
 *   17-28 content   (dynamic: Commands OR Scratch rows)
 *   29-34 footer
 *
 * Usage: ee_compose_frame.+x
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define MAX_LINE 2048
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define BOX_W 62
#define CONTENT_START 17
#define MAX_CONTENT 12
#define N_FOOTER 6

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

static void ping(void) {
    char p[PATH_BUF];
    snprintf(p, sizeof(p), "%s/pieces/display/frame_changed.txt", project_root);
    FILE *f = fopen(p, "a");
    if (f) { fputc('.', f); fclose(f); }
    snprintf(p, sizeof(p), "%s/pieces/display/ee_screen_changed.txt", project_root);
    f = fopen(p, "a");
    if (f) { fputc('.', f); fclose(f); }
}

static const char *CMDS[MAX_CONTENT] = {
    "Show Text : \"Halt! Who goes there?\"",
    "Show Choices : Fight, Flee, Talk",
    "When [Talk]",
    "  Control Switches : door_open = ON",
    "  Show Text : \"You may pass.\"",
    "End",
    "Conditional Branch : Switch door_open is ON",
    "  Transfer Player : map_02 (3, 8)",
    "End",
    "Comment : dual-source IR",
    "Script (.pal) : call_op muta_map_io ...",
    "(empty — insert command)"
};

static const char *BLOCKS[MAX_CONTENT] = {
    "[ when action_button ]",
    "  { show_text \"Halt! Who goes there?\" }",
    "  { show_choices Fight | Flee | Talk }",
    "  [ if choice == Talk ]",
    "    { set_switch door_open ON }",
    "    { show_text \"You may pass.\" }",
    "  [ if switch door_open == ON ]",
    "    { transfer map_02 3 8 }",
    "  [ comment dual_source ]",
    "  { call_op muta_map_io ... }",
    "  + add block ...",
    "(empty block slot)"
};

static const char *FOOTER[N_FOOTER] = {
    "OK", "Cancel", "Apply", "Add Cmd", "Palette", "Desktop"
};

/* Pure ASCII chrome only (32-126). Unicode box-drawing has no glyph.txt
 * in pieces/registry/fonts/ascii/ — rgb would drop those cells to blank. */
static void box_line(FILE *o, const char *s) {
    fprintf(o, "| %-*.*s |\n", BOX_W - 2, BOX_W - 2, s);
}

/* Sanitize button label: no double-quotes */
static void sanitize(const char *in, char *out, size_t n) {
    size_t j = 0;
    for (size_t i = 0; in[i] && j + 1 < n; i++) {
        char c = in[i];
        if (c == '"') c = '\'';
        if (c == '\n' || c == '\r') c = ' ';
        out[j++] = c;
    }
    out[j] = '\0';
}

/* REAL event content, 2026-08-04 direct instruction ("i want to start
 * seeing the actual events in any of the event editors"): parses a
 * real package's own event.ir.pdl NODE rows (format:
 * "NODE         | id=N type=TYPE    | text=TEXT") into real content
 * lines, replacing the hardcoded door_guard CMDS[]/BLOCKS[] demo
 * arrays whenever a real pkg_dir (ee_state.txt's own pkg_dir= key,
 * set by button.sh's EE_PKG_DIR env, see methods.pdl's own Events row)
 * points at a package that actually has one. Falls back to the demo
 * arrays untouched if no pkg_dir or no event.ir.pdl - existing
 * door_guard behavior is unaffected. */
static int load_real_rows(const char *pkg_dir, char rows_buf[MAX_CONTENT][160]) {
    if (!pkg_dir || !pkg_dir[0]) return 0;
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/event.ir.pdl", pkg_dir);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int n = 0;
    while (n < MAX_CONTENT && fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strncmp(line, "NODE", 4) != 0) continue;
        char *type_p = strstr(line, "type=");
        char *text_p = strstr(line, "text=");
        if (!type_p) continue;
        char type_buf[48] = "";
        {
            char *t = type_p + 5, *sp = strchr(t, ' ');
            char *pipe = strchr(t, '|');
            size_t len = sp ? (size_t)(sp - t) : (pipe ? (size_t)(pipe - t) : strlen(t));
            if (len >= sizeof(type_buf)) len = sizeof(type_buf) - 1;
            memcpy(type_buf, t, len);
            type_buf[len] = '\0';
        }
        const char *text_val = text_p ? text_p + 5 : "";
        while (*text_val == ' ') text_val++;
        snprintf(rows_buf[n], 160, "%s: %s", type_buf, text_val);
        n++;
    }
    fclose(f);
    return n;
}

int main(void) {
    resolve_root();

    char state[PATH_BUF], view[PATH_BUF], gui[PATH_BUF];
    snprintf(state, sizeof(state), "%s/pieces/system/ee_state.txt", project_root);
    snprintf(view, sizeof(view), "%s/pieces/apps/player_app/view.txt", project_root);
    snprintf(gui, sizeof(gui), "%s/projects/event-editor/manager/gui_state.txt", project_root);

    char view_mode[64], pkg[128], page[32], msg[MAX_LINE], pkg_dir[PATH_BUF];
    read_kv(state, "view_mode", view_mode, sizeof(view_mode));
    read_kv(state, "pkg_name", pkg, sizeof(pkg));
    read_kv(state, "page", page, sizeof(page));
    read_kv(state, "last_message", msg, sizeof(msg));
    read_kv(state, "pkg_dir", pkg_dir, sizeof(pkg_dir));
    if (!view_mode[0]) snprintf(view_mode, sizeof(view_mode), "commands");
    if (!pkg[0]) snprintf(pkg, sizeof(pkg), "door_guard");
    if (!page[0]) snprintf(page, sizeof(page), "1");

    int scratch = (strcmp(view_mode, "scratch") == 0);
    static char real_rows_buf[MAX_CONTENT][160];
    static const char *real_rows_ptr[MAX_CONTENT];
    int n_real = load_real_rows(pkg_dir, real_rows_buf);
    for (int i = 0; i < n_real; i++) real_rows_ptr[i] = real_rows_buf[i];

    const char **rows = n_real > 0 ? real_rows_ptr : (scratch ? BLOCKS : CMDS);
    int n_content = n_real > 0 ? n_real : MAX_CONTENT;
    int footer_base = CONTENT_START + n_content; /* 29 */
    int last_nav = footer_base + N_FOOTER - 1;   /* 34 */

    FILE *o = fopen(view, "w");
    if (!o) return 1;
    fprintf(o, "+==============================================================+\n");
    fprintf(o, "|  EVENT  name=%-16.16s  page=%s  view=%-8.8s |\n",
            pkg, page, scratch ? "SCRATCH" : "COMMANDS");
    fprintf(o, "+==============================================================+\n");
    /* REAL sprite thumbnail, 2026-08-04 direct instruction ("dont we
     * create emoji atlas as jpg then convert to csv? cant we do the
     * same with an image"): reuses the EXACT same real mechanism as
     * map-tile emoji rendering (chtpm_rgb_render's own
     * load_emoji_assets_from() reverse lookup against
     * pieces/registry/items/items.txt) - the glyph below is not
     * decorative text, it's a registered Unicode marker
     * (items.txt: asa_portrait|...|🎤) that chtpm_rgb_render replaces
     * with a real 16x16 voxel block generated from asa's own
     * il-dj.png via tp_asset_to_sprite.+x (registry:
     * pieces/registry/emoji_assets/asa_portrait/voxels_16.csv). Only
     * wired for pkg=="asa"/"ava" so far (ava: items.txt ava_portrait|
     * ...|💃, from nyeoni8.png); other pkgs fall back to the plain
     * bracket placeholder until they get their own registered asset. */
    if (strcmp(pkg, "asa") == 0) {
        box_line(o, "Image \xF0\x9F\x8E\xA4  Trigger: Action Button  Priority: Same");
    } else if (strcmp(pkg, "ava") == 0) {
        box_line(o, "Image \xF0\x9F\x92\x83  Trigger: Action Button  Priority: Same");
    } else {
        box_line(o, "Image [@]  Trigger: Action Button  Priority: Same");
    }
    box_line(o, "Conditions: [x] Switch door_open is OFF");
    fprintf(o, "+==============================================================+\n");
    {
        char line[96];
        snprintf(line, sizeof(line), "NAV continuous 1..%d  (multi-digit: type 17)", last_nav);
        box_line(o, line);
    }
    box_line(o, "1-8 methods | 9-12 pages | 13-16 fields | 17-28 contents | 29-34 foot");
    for (int i = 0; i < 5 && i < n_content; i++) {
        char line[96], clean[80];
        sanitize(rows[i], clean, sizeof(clean));
        snprintf(line, sizeof(line), "%2d. %.52s", CONTENT_START + i, clean);
        box_line(o, line);
    }
    box_line(o, "... full list = navigable buttons below (same numbers)");
    fprintf(o, "+==============================================================+\n");
    if (msg[0]) {
        char line[96];
        snprintf(line, sizeof(line), "%.58s", msg);
        box_line(o, line);
    } else {
        box_line(o, "KEY:5 toggle Commands|Scratch | Enter commit | Esc BACK");
    }
    fprintf(o, "+==============================================================+\n");
    fclose(o);

    {
        char cmd[PATH_BUF];
        snprintf(cmd, sizeof(cmd), "mkdir -p '%s/projects/event-editor/manager'", project_root);
        system(cmd);
    }

    FILE *g = fopen(gui, "w");
    if (!g) return 1;

    /* game_map: multi-line from view — write pipe-joined for single gui line;
     * chtpm also often reloads view.txt; keep both. */
    {
        FILE *vf = fopen(view, "r");
        fprintf(g, "game_map=");
        if (vf) {
            char line[MAX_LINE];
            int first = 1;
            while (fgets(line, sizeof(line), vf)) {
                line[strcspn(line, "\n")] = '\0';
                if (!first) fputs("\\n", g);
                /* gui_state loaders: some want real newlines in value — use spaces */
                for (char *p = line; *p; p++)
                    fputc(*p == '\\' ? '/' : *p, g);
                first = 0;
            }
            fclose(vf);
        }
        fputc('\n', g);
    }

    fprintf(g, "view_mode=%s\n", scratch ? "SCRATCH" : "COMMANDS");
    fprintf(g, "pkg_name=%s\n", pkg);
    fprintf(g, "nav_status=Nav>_ 1..%d multi-digit\n", last_nav);

    /* Labels WITHOUT numbers — CHTPM assigns continuous 1..N (digit_accum).
     * Fixed layout has 16 buttons before content (8 methods+4 pages+4 fields),
     * so content interactive indices start at 17. EE:ROW not KEY:n>9. */
    fprintf(g, "event_content_rows=");
    for (int i = 0; i < n_content; i++) {
        int num = CONTENT_START + i;
        char clean[160];
        sanitize(rows[i], clean, sizeof(clean));
        fprintf(g, "<button label=\"%s\" onClick=\"EE:ROW:%d\" /><br/>",
                clean, num);
    }
    fputc('\n', g);

    fprintf(g, "event_footer_rows=");
    for (int i = 0; i < N_FOOTER; i++) {
        int num = footer_base + i;
        fprintf(g, "<button label=\"%s\" onClick=\"EE:FOOT:%d\" /><br/>",
                FOOTER[i], num);
    }
    fputc('\n', g);
    fclose(g);

    {
        char map[PATH_BUF];
        snprintf(map, sizeof(map), "%s/pieces/system/ee_nav_map.txt", project_root);
        FILE *mf = fopen(map, "w");
        if (mf) {
            fprintf(mf, "1-8 methods\n9-12 pages\n13-16 fields\n");
            fprintf(mf, "%d-%d content (%s)\n", CONTENT_START, CONTENT_START + n_content - 1,
                    scratch ? "scratch" : "commands");
            fprintf(mf, "%d-%d footer\n", footer_base, last_nav);
            fclose(mf);
        }
    }

    ping();
    return 0;
}
