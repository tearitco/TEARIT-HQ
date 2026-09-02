/* editor_compose_frame - render AGY editor chrome + buffer canvas into
 * pieces/apps/player_app/view.txt (${game_map}).
 *
 * Buffer lives at pieces/system/editor_buffer.txt.
 * Cursor: pieces/system/editor_state.txt cursor_pos=N (-1 = end).
 * Cursor glyph in canvas: [X]  (matches editor-flow.txt)
 *
 * Writes ONLY view.txt + pings frame_changed.txt (ONE VISIBLE FRAME WRITER).
 * Usage: editor_compose_frame.+x
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define MAX_LINE 2048
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define BOX_W 45
#define MAX_BUF 65536

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void read_kv_str(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t key_len = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            char *v = line + key_len + 1;
            v[strcspn(v, "\n")] = '\0';
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(out, out_sz, "%s", v);
#pragma GCC diagnostic pop
            break;
        }
    }
    fclose(f);
}

static void ping_render_marker(void) {
    char marker_path[PATH_BUF];
    snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/frame_changed.txt", project_root);
    FILE *mf = fopen(marker_path, "a");
    if (mf) { fputc('.', mf); fclose(mf); }
}

/* Read whole buffer file into out (null-terminated). Returns length. */
static size_t read_buffer(char *out, size_t out_sz) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/editor_buffer.txt", project_root);
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    size_t n = fread(out, 1, out_sz - 1, f);
    out[n] = '\0';
    fclose(f);
    return n;
}

static int get_cursor_pos(size_t buflen) {
    char path[PATH_BUF], raw[64];
    snprintf(path, sizeof(path), "%s/pieces/system/editor_state.txt", project_root);
    read_kv_str(path, "cursor_pos", raw, sizeof(raw));
    if (!raw[0] || strcmp(raw, "-1") == 0) return (int)buflen;
    int p = atoi(raw);
    if (p < 0) p = 0;
    if ((size_t)p > buflen) p = (int)buflen;
    return p;
}

static void box_top(FILE *o, const char *title) {
    fprintf(o, "╔");
    int pad = BOX_W - 2;
    int tlen = (int)strlen(title);
    int left = (pad - tlen) / 2;
    if (left < 0) left = 0;
    for (int i = 0; i < left; i++) fputs("═", o);
    fputs(title, o);
    for (int i = left + tlen; i < pad; i++) fputs("═", o);
    fprintf(o, "╗\n");
}

static void box_sep(FILE *o) {
    fprintf(o, "╠");
    for (int i = 0; i < BOX_W; i++) fputs("═", o);
    fprintf(o, "╣\n");
}

static void box_bot(FILE *o) {
    fprintf(o, "╚");
    for (int i = 0; i < BOX_W; i++) fputs("═", o);
    fprintf(o, "╝\n");
}

/* One content row: ║ + content padded to BOX_W + ║ */
static void box_row(FILE *o, const char *content) {
    /* Approximate visible width: count non-continuation UTF-8 bytes crudely
     * by treating multi-byte sequences as width 1 for emoji-ish glyphs. */
    int vis = 0;
    for (const unsigned char *p = (const unsigned char *)content; *p; ) {
        if (*p < 0x80) { vis++; p++; }
        else if ((*p & 0xE0) == 0xC0) { vis++; p += 2; if (!p[-1]) break; }
        else if ((*p & 0xF0) == 0xE0) { vis++; p += 3; }
        else if ((*p & 0xF8) == 0xF0) { vis++; p += 4; }
        else { vis++; p++; }
    }
    fprintf(o, "║  %s", content);
    int pad = BOX_W - 2 - vis;
    if (pad < 0) pad = 0;
    for (int i = 0; i < pad; i++) fputc(' ', o);
    fprintf(o, "║\n");
}

int main(void) {
    resolve_root();

    char state_path[PATH_BUF], view_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/pieces/system/editor_state.txt", project_root);
    snprintf(view_path, sizeof(view_path), "%s/pieces/apps/player_app/view.txt", project_root);

    char file_path[256], last_message[MAX_LINE];
    read_kv_str(state_path, "file_path", file_path, sizeof(file_path));
    read_kv_str(state_path, "last_message", last_message, sizeof(last_message));
    if (!file_path[0]) snprintf(file_path, sizeof(file_path), "docs/untitled.txt");

    char buf[MAX_BUF];
    size_t blen = read_buffer(buf, sizeof(buf));
    int cursor = get_cursor_pos(blen);

    /* Build display string with [X] at cursor */
    char with_cursor[MAX_BUF + 8];
    if ((size_t)cursor > blen) cursor = (int)blen;
    memcpy(with_cursor, buf, (size_t)cursor);
    memcpy(with_cursor + cursor, "[X]", 3);
    memcpy(with_cursor + cursor + 3, buf + cursor, blen - (size_t)cursor);
    with_cursor[blen + 3] = '\0';

    FILE *o = fopen(view_path, "w");
    if (!o) return 1;

    box_top(o, " X Y Z   E D I T O R ");
    {
        char row[BOX_W + 32];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(row, sizeof(row), "FILE: %s", file_path);
#pragma GCC diagnostic pop
        box_row(o, row);
    }
    box_sep(o);

    /* Emit buffer lines (split on \n); empty buffer still shows cursor row */
    {
        const char *p = with_cursor;
        int rows = 0;
        while (*p || rows == 0) {
            char linebuf[512];
            size_t i = 0;
            while (*p && *p != '\n' && i + 1 < sizeof(linebuf)) {
                linebuf[i++] = *p++;
            }
            linebuf[i] = '\0';
            if (*p == '\n') p++;
            box_row(o, linebuf);
            rows++;
            if (!*p) break;
            if (rows >= 12) break; /* keep frame compact */
        }
        /* Pad a few empty rows so canvas feels like a page */
        while (rows < 6) {
            box_row(o, "");
            rows++;
        }
    }

    box_sep(o);
    if (last_message[0]) {
        char row[BOX_W + 32];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(row, sizeof(row), "%s", last_message);
#pragma GCC diagnostic pop
        /* trim long message */
        if ((int)strlen(row) > BOX_W - 2) row[BOX_W - 2] = '\0';
        box_row(o, row);
    }
    box_bot(o);

    fclose(o);
    ping_render_marker();
    return 0;
}
