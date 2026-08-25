/* compose_title_frame - one verb, one binary, no shared headers.
 * Renders the title screen (New Game + one row per existing save) into
 * pieces/display/current_frame.txt - the same output file
 * compose_frame.c writes during real gameplay, so the renderer process
 * doesn't need to know or care which phase (title vs playing) is
 * currently active; it just prints whatever's in that file. Same
 * bracket-cursor + numbered-list convention as egg-pals' compose_menu.c
 * and mutaclsym's own craft/inventory overlay panels (see
 * ops/compose_frame.c's draw_panel_box() and its citation of real
 * 1.TPMOS's chtmp_parser.c). */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_SAVES 32
#define BOX_W 60

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static int read_kv_int(const char *path, const char *key, int def) {
    FILE *f = fopen(path, "r");
    if (!f) return def;
    char line[MAX_LINE];
    int val = def;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(line, key) == 0) { val = atoi(eq + 1); break; }
    }
    fclose(f);
    return val;
}

static void border(FILE *out) {
    fputc('+', out);
    for (int i = 0; i < BOX_W; i++) fputc('=', out);
    fputc('+', out);
    fputc('\n', out);
}

static void line(FILE *out, const char *content) {
    int len = (int)strlen(content);
    if (len > BOX_W) len = BOX_W;
    fprintf(out, "|%.*s", len, content);
    for (int i = len; i < BOX_W; i++) fputc(' ', out);
    fputc('|', out);
    fputc('\n', out);
}

static void blank(FILE *out) { line(out, ""); }

static void title(FILE *out, const char *text) {
    char spaced[BOX_W + 1] = "";
    int p = 0;
    for (const char *c = text; *c && p < BOX_W - 1; c++) {
        spaced[p++] = *c;
        if (c[1]) spaced[p++] = ' ';
    }
    spaced[p] = '\0';
    int pad = (BOX_W - (int)strlen(spaced)) / 2;
    char padded[BOX_W + 1] = "";
    for (int i = 0; i < pad && i < BOX_W; i++) padded[i] = ' ';
    snprintf(padded + (pad > 0 ? pad : 0), sizeof(padded) - (pad > 0 ? pad : 0), "%s", spaced);
    line(out, padded);
}

/* Same "[cursor] N. [label]" convention as every other numbered list in
 * this project family - see draw_panel_box()/option()'s own citations. */
static void option(FILE *out, int index, int cursor, const char *label) {
    char buf[BOX_W + 1];
    const char *cur = index == cursor ? "[>]" : "[ ]";
    /* label is genuinely short (built from a fixed save name + turn
     * number just below) despite gcc only being able to prove it no
     * longer than its own declared size - same class of warning
     * suppressed narrowly elsewhere in this project. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(buf, sizeof(buf), "  %s %d. [%s]", cur, index, label);
#pragma GCC diagnostic pop
    line(out, buf);
}

int main(void) {
    resolve_root();

    char state_path[PATH_BUF], out_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/pieces/system/title_state.txt", project_root);
    snprintf(out_path, sizeof(out_path), "%s/pieces/display/current_frame.txt", project_root);
    int cursor = read_kv_int(state_path, "cursor", 1);

    char saves_dir[PATH_BUF];
    snprintf(saves_dir, sizeof(saves_dir), "%s/pieces/saves", project_root);
    char save_names[MAX_SAVES][64];
    int save_count = 0;
    DIR *d = opendir(saves_dir);
    if (d) {
        struct dirent *entry;
        while (save_count < MAX_SAVES && (entry = readdir(d)) != NULL) {
            if (entry->d_name[0] == '.') continue;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(save_names[save_count], 64, "%s", entry->d_name);
#pragma GCC diagnostic pop
            save_count++;
        }
        closedir(d);
    }

    FILE *out = fopen(out_path, "w");
    if (!out) return 1;

    border(out);
    blank(out);
    title(out, "MUTACLSYM");
    blank(out);
    option(out, 1, cursor, "New Game");
    for (int i = 0; i < save_count; i++) {
        char meta_path[PATH_BUF + 96];
        /* save_names[i] is genuinely short (see title_input.c's own
         * identical suppression at the point it's populated). */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(meta_path, sizeof(meta_path), "%s/%s/save_meta.txt", saves_dir, save_names[i]);
        int turn = read_kv_int(meta_path, "turn", 0);
        char label[96];
        snprintf(label, sizeof(label), "Continue: %s (turn %d)", save_names[i], turn);
#pragma GCC diagnostic pop
        option(out, i + 2, cursor, label);
    }
    blank(out);
    border(out);
    fprintf(out, "[0-9] jump  [up/down] move  [enter] select  [ctrl-c] quit\n");

    fclose(out);
    return 0;
}
