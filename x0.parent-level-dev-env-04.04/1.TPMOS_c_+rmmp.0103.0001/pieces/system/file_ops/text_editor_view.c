#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*
 * text_editor_view.+x -- all-purpose viewport renderer for a cursor-based
 * text buffer (see text_edit_key.c's own header for the REUSE RULE this
 * pair of Ops satisfies). Extracted from agy-text-editor's
 * build_editor_map(). Outputs PLAIN text lines only (no box-drawing, no
 * XML/markup) so each caller can wrap them in its own presentation style
 * -- agy-text-editor's box-drawing panel vs. wrai-text-editor's <text>/
 * <br/> markup are different callers of the identical view logic.
 *
 * Usage: text_editor_view.+x <document_path> <cursor_state_path> <view_width> <view_height> <output_path>
 *   Writes exactly view_height lines to output_path, each padded/truncated
 *   to view_width characters. The line containing the cursor has "[X]"
 *   inserted at the cursor column. Vertical camera keeps the cursor line
 *   roughly centered; horizontal camera keeps the cursor column roughly
 *   centered on long lines.
 *
 * Bug fixed during extraction (2026-07-11, reported live as "rendering
 * bugs on the map screen"): the original build_editor_map() copied each
 * document line into a fixed buffer with strncpy(buf, src, N-1) and never
 * explicitly appended '\0' afterward. strncpy does NOT null-terminate
 * when the source is >= N-1 bytes -- so any document line at or beyond
 * the buffer's capacity left that buffer unterminated, and the strlen()/
 * strcpy() calls that followed (splitting the line into "before cursor"/
 * "after cursor" pieces) read past the end of the buffer into whatever
 * stack memory came next, corrupting the rendered output (and, in the
 * worst case, other stack variables) for that frame. Every buffer here
 * is explicitly null-terminated after every copy.
 */

#define MAX_LINES 512
#define MAX_LINE_LEN 2048
#define MAX_VIEW_WIDTH 256

static char lines[MAX_LINES][MAX_LINE_LEN];
static int total_lines = 0;
static int cursor_x = 0, cursor_y = 0;

static char* trim_str(char *str) {
    char *end;
    if (!str) return str;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

static void load_document(const char *path) {
    FILE *f = fopen(path, "r");
    total_lines = 0;
    if (f) {
        while (total_lines < MAX_LINES && fgets(lines[total_lines], MAX_LINE_LEN, f)) {
            size_t len = strlen(lines[total_lines]);
            while (len > 0 && (lines[total_lines][len - 1] == '\n' || lines[total_lines][len - 1] == '\r')) {
                lines[total_lines][--len] = '\0';
            }
            total_lines++;
        }
        fclose(f);
    }
    if (total_lines == 0) {
        lines[0][0] = '\0';
        total_lines = 1;
    }
}

static void load_cursor(const char *path) {
    FILE *f = fopen(path, "r");
    char line[256];
    cursor_x = 0;
    cursor_y = 0;
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "cursor_x=", 9) == 0) cursor_x = atoi(trim_str(line + 9));
        else if (strncmp(line, "cursor_y=", 9) == 0) cursor_y = atoi(trim_str(line + 9));
    }
    fclose(f);
}

int main(int argc, char *argv[]) {
    const char *document_path, *cursor_path, *output_path;
    int view_width, view_height;
    FILE *out;

    if (argc < 6) {
        fprintf(stderr, "Usage: %s <document_path> <cursor_state_path> <view_width> <view_height> <output_path>\n", argv[0]);
        return 2;
    }
    document_path = argv[1];
    cursor_path = argv[2];
    view_width = atoi(argv[3]);
    view_height = atoi(argv[4]);
    output_path = argv[5];

    if (view_width <= 0 || view_width >= MAX_VIEW_WIDTH) view_width = 40;
    if (view_height <= 0 || view_height > MAX_LINES) view_height = 8;

    load_document(document_path);
    load_cursor(cursor_path);

    if (cursor_y >= total_lines) cursor_y = total_lines - 1;
    if (cursor_y < 0) cursor_y = 0;
    {
        int len = (int)strlen(lines[cursor_y]);
        if (cursor_x > len) cursor_x = len;
        if (cursor_x < 0) cursor_x = 0;
    }

    out = fopen(output_path, "w");
    if (!out) return 1;

    int view_start_y = cursor_y - (view_height / 2);
    if (view_start_y < 0) view_start_y = 0;
    if (view_start_y + view_height > total_lines && total_lines > view_height) {
        view_start_y = total_lines - view_height;
    }

    int view_start_x = cursor_x - (view_width / 2);
    if (view_start_x < 0) view_start_x = 0;

    for (int i = view_start_y; i < view_start_y + view_height; i++) {
        char line_buf[MAX_LINE_LEN];
        char final_line[MAX_LINE_LEN + 8];
        char scrolled_line[MAX_VIEW_WIDTH + 1];

        if (i < total_lines) {
            strncpy(line_buf, lines[i], sizeof(line_buf) - 1);
            line_buf[sizeof(line_buf) - 1] = '\0';
        } else {
            line_buf[0] = '\0';
        }

        int len = (int)strlen(line_buf);
        if (i == cursor_y) {
            int cx = cursor_x;
            if (cx > len) cx = len;
            if (cx < 0) cx = 0;
            /* Explicit termination on both halves -- see this file's own
               header comment for why the original code's bare strncpy
               here was the actual root cause being fixed. */
            char before[MAX_LINE_LEN];
            strncpy(before, line_buf, (size_t)cx);
            before[cx] = '\0';
            snprintf(final_line, sizeof(final_line), "%s[X]%s", before, line_buf + cx);
        } else {
            strncpy(final_line, line_buf, sizeof(final_line) - 1);
            final_line[sizeof(final_line) - 1] = '\0';
        }

        int final_len = (int)strlen(final_line);
        if (view_start_x < final_len) {
            strncpy(scrolled_line, final_line + view_start_x, (size_t)view_width);
            scrolled_line[view_width] = '\0';
        } else {
            scrolled_line[0] = '\0';
        }

        fprintf(out, "%-*.*s\n", view_width, view_width, scrolled_line);
    }

    fclose(out);
    return 0;
}
