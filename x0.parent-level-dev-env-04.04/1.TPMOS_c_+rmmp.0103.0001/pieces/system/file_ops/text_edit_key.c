#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*
 * text_edit_key.+x -- all-purpose, reusable cursor-based text-editing Op
 * (Bible section 11's REUSE RULE: "If logic is shared across managers or
 * projects, prefer a reusable Op first"). Extracted from agy-text-editor's
 * manager.c, which had this exact editing logic hardcoded inline -- a
 * violation of section 11's own MANDATE ("NEVER hardcode functional
 * logic... directly into manager modules... Managers orchestrate; Ops
 * execute"). Lives under pieces/system/ (not any one project) specifically
 * so ANY project that needs cursor-based line editing of a text buffer --
 * not just text editors -- can fork+exec this instead of reimplementing
 * it. agy-text-editor and wrai-text-editor both call this same binary.
 *
 * Usage: text_edit_key.+x <document_path> <cursor_state_path> <key_code>
 *   document_path: text file, one document line per line (read + rewritten)
 *   cursor_state_path: text file with "cursor_x=N\ncursor_y=N\n" (read + rewritten)
 *   key_code: integer keycode -- matches the exact key semantics
 *     wraith-alpha_manager.c/chtpm_parser.c already use for INTERACT-mode
 *     elements (ARROW_LEFT/RIGHT/UP/DOWN remapped to 1000/1001/1002/1003,
 *     everything else passed through as its raw ASCII/control code), so
 *     callers on either side of the Wraith/legacy split can feed this Op
 *     the same numbers they already have without translation.
 *
 * Exit code: 0 if the document or cursor changed, 1 if the key was a no-op
 * (unrecognized, or a movement/edit that was already at a boundary).
 *
 * CLI-testable per Bible section 12: seed a document + cursor file, run
 * this with a single key code, inspect the rewritten files.
 */

#define MAX_LINES 512
#define MAX_LINE_LEN 2048

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
            /* fgets null-terminates on success; strip the trailing
               newline(s) explicitly. The original agy-text-editor code
               this was extracted from had several strncpy() calls into
               fixed buffers with no explicit '\0' afterward -- a line at
               or beyond a buffer's capacity left the string
               unterminated, and any later strlen()/strcpy() on it read
               past the buffer. Very likely the actual cause of the
               reported editor-map rendering corruption on long lines.
               Every buffer this file writes is explicitly terminated,
               not left to strncpy's short-copy behavior. */
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

static void save_document(const char *path) {
    char tmp_path[4096];
    FILE *f;
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    f = fopen(tmp_path, "w");
    if (!f) return;
    for (int i = 0; i < total_lines; i++) {
        fprintf(f, "%s\n", lines[i]);
    }
    fclose(f);
    rename(tmp_path, path);
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

static void save_cursor(const char *path) {
    char tmp_path[4096];
    FILE *f;
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    f = fopen(tmp_path, "w");
    if (!f) return;
    fprintf(f, "cursor_x=%d\ncursor_y=%d\n", cursor_x, cursor_y);
    fclose(f);
    rename(tmp_path, path);
}

/* Mirrors agy-text-editor's original handle_interact_key() exactly for
   movement/editing semantics (WASD or the INTERACT eff-remapped arrow
   codes 1000-1003 move; backspace merges lines; Enter splits; printable
   chars insert at cursor) -- same UX, now shared instead of duplicated.
   Returns 1 if anything changed, 0 for a no-op key. */
static int apply_key(int key) {
    int modified = 0;

    if (key == 'w' || key == 'W' || key == 1002) {
        if (cursor_y > 0) {
            cursor_y--;
            int len = (int)strlen(lines[cursor_y]);
            if (cursor_x > len) cursor_x = len;
        }
    } else if (key == 's' || key == 'S' || key == 1003) {
        if (cursor_y < MAX_LINES - 2) {
            if (cursor_y >= total_lines - 1) {
                lines[total_lines][0] = '\0';
                total_lines++;
                modified = 1;
            }
            cursor_y++;
            int len = (int)strlen(lines[cursor_y]);
            if (cursor_x > len) cursor_x = len;
        }
    } else if (key == 'a' || key == 'A' || key == 1000) {
        if (cursor_x > 0) {
            cursor_x--;
        }
    } else if (key == 'd' || key == 'D' || key == 1001) {
        int len = (int)strlen(lines[cursor_y]);
        if (cursor_x < len) {
            cursor_x++;
        } else if (len < MAX_LINE_LEN - 2) {
            lines[cursor_y][len] = ' ';
            lines[cursor_y][len + 1] = '\0';
            cursor_x++;
            modified = 1;
        }
    } else if (key == 127 || key == 8) {
        if (cursor_x > 0) {
            char *line = lines[cursor_y];
            memmove(line + cursor_x - 1, line + cursor_x, strlen(line + cursor_x) + 1);
            cursor_x--;
            modified = 1;
        } else if (cursor_y > 0) {
            int prev_len = (int)strlen(lines[cursor_y - 1]);
            if (prev_len + (int)strlen(lines[cursor_y]) < MAX_LINE_LEN - 1) {
                strcat(lines[cursor_y - 1], lines[cursor_y]);
                for (int i = cursor_y; i < total_lines - 1; i++) {
                    strcpy(lines[i], lines[i + 1]);
                }
                total_lines--;
                cursor_y--;
                cursor_x = prev_len;
                modified = 1;
            }
        }
    } else if (key == 10 || key == 13) {
        if (total_lines < MAX_LINES - 1) {
            for (int i = total_lines; i > cursor_y + 1; i--) {
                strcpy(lines[i], lines[i - 1]);
            }
            strcpy(lines[cursor_y + 1], lines[cursor_y] + cursor_x);
            lines[cursor_y][cursor_x] = '\0';
            total_lines++;
            cursor_y++;
            cursor_x = 0;
            modified = 1;
        }
    } else if (key >= 32 && key <= 126) {
        char *line = lines[cursor_y];
        int len = (int)strlen(line);
        if (len < MAX_LINE_LEN - 2) {
            if (cursor_x > len) {
                for (int i = len; i < cursor_x; i++) line[i] = ' ';
                line[cursor_x] = '\0';
                len = cursor_x;
            }
            memmove(line + cursor_x + 1, line + cursor_x, len - cursor_x + 1);
            line[cursor_x] = (char)key;
            cursor_x++;
            modified = 1;
        }
    }

    return modified;
}

/* Inserts an arbitrary multi-byte UTF-8 string at the cursor -- added
   2026-07-13 for the emoji picker (context-st8.txt). apply_key()'s own
   printable-char branch above only ever writes ONE byte
   (`line[cursor_x] = (char)key`), because every key_code it was ever
   designed for is a single ASCII byte (32-126) -- an emoji is a 4-byte
   UTF-8 sequence (e.g. the grinning-face emoji is F0 9F 98 80), which
   simply cannot go through that path at all. Reuses THIS file/tool
   rather than adding a new one (Bible section 11 REUSE RULE) since this
   is genuinely the same job (mutate the document+cursor files at a
   cursor position) with a different-shaped payload, not a different
   feature. cursor_x is already a BYTE offset throughout this file (see
   apply_key()'s ASCII-only insert path) -- advancing it by strlen(text)
   (byte count) after inserting is consistent with that existing
   convention, not a new one. Returns 1 (always "changed") if any bytes
   were actually inserted. */
static int insert_text(const char *text) {
    char *line;
    int len;
    size_t insert_len;

    if (!text || !text[0]) {
        return 0;
    }
    line = lines[cursor_y];
    len = (int)strlen(line);
    insert_len = strlen(text);

    if (cursor_x > len) {
        for (int i = len; i < cursor_x; i++) line[i] = ' ';
        line[cursor_x] = '\0';
        len = cursor_x;
    }
    if ((size_t)len + insert_len >= MAX_LINE_LEN - 1) {
        return 0; /* would overflow the line buffer -- no-op, matches
                     apply_key()'s own len < MAX_LINE_LEN - 2 guards */
    }

    memmove(line + cursor_x + insert_len, line + cursor_x, (size_t)(len - cursor_x) + 1);
    memcpy(line + cursor_x, text, insert_len);
    cursor_x += (int)insert_len;
    return 1;
}

int main(int argc, char *argv[]) {
    const char *document_path, *cursor_path;
    int key;
    int changed;

    if (argc < 4) {
        fprintf(stderr, "Usage: %s <document_path> <cursor_state_path> <key_code>\n", argv[0]);
        fprintf(stderr, "   or: %s <document_path> <cursor_state_path> insert <utf8_text>\n", argv[0]);
        return 2;
    }
    document_path = argv[1];
    cursor_path = argv[2];

    load_document(document_path);
    load_cursor(cursor_path);

    if (strcmp(argv[3], "insert") == 0 && argc >= 5) {
        changed = insert_text(argv[4]);
    } else {
        key = atoi(argv[3]);
        changed = apply_key(key);
    }

    /* Cursor position is always re-clamped and re-written even on a
       no-op key, so a caller never has to special-case "did the cursor
       move without the document changing" -- movement-only keys
       (WASD/arrows with no line-length change) still count as a real
       state change from the caller's point of view. */
    if (cursor_y >= total_lines) cursor_y = total_lines - 1;
    if (cursor_y < 0) cursor_y = 0;
    {
        int len = (int)strlen(lines[cursor_y]);
        if (cursor_x > len) cursor_x = len;
        if (cursor_x < 0) cursor_x = 0;
    }

    save_document(document_path);
    save_cursor(cursor_path);

    return changed ? 0 : 1;
}
