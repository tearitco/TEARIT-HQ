#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* pdl_read_manager: simple paginated document reader backend
 * Invocation: pdl_read_manager <house_root> <package_dir> <unused>
 * Reads document list from package_dir/docs.pdl, polls package_dir/pdl_read_action.txt
 * for commands (OPEN, NEXT, PREV), writes UI state to package_dir/pdl_read_ui.txt
 */

#define MAX_DOCS 200
#define MAX_TITLE_LEN 200
#define MAX_PATH_LEN 4096
#define MAX_CONTENT_SIZE 2000000
#define MAX_PAGES 2000
#define LINES_PER_PAGE 30
#define ACTION_FILE_BUF_SIZE 4096

typedef struct {
    char title[MAX_TITLE_LEN + 1];
    char path[MAX_PATH_LEN + 1];
} Document;

static Document docs[MAX_DOCS];
static int n_docs = 0;
static int current_doc = -1;
static int current_page = 0;
static char content_buffer[MAX_CONTENT_SIZE];
static int content_size = 0;
static int page_count = 0;
static char package_dir[MAX_PATH_LEN + 1];

/* Trim trailing whitespace from a string in place */
static void trim_trailing(char *s) {
    int len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' ||
                       s[len - 1] == '\r' || s[len - 1] == '\n')) {
        s[--len] = '\0';
    }
}

/* Count total lines in the content buffer */
static int count_total_lines(void) {
    if (content_size == 0) {
        return 0;
    }

    int lines = 1;
    for (int i = 0; i < content_size; i++) {
        if (content_buffer[i] == '\n') {
            lines++;
        }
    }

    /* If file ends with newline, that's just termination, not an extra line */
    if (content_buffer[content_size - 1] == '\n') {
        lines--;
    }

    return lines;
}

/* Calculate page count for the current document */
static int calculate_page_count(void) {
    int lines = count_total_lines();
    if (lines == 0) {
        return 1;  /* Empty file still has 1 empty page */
    }

    int pages = (lines + LINES_PER_PAGE - 1) / LINES_PER_PAGE;
    if (pages > MAX_PAGES) {
        pages = MAX_PAGES;
    }
    return pages;
}

/* Read and parse docs.pdl file */
static void read_docs_list(const char *pkg_dir) {
    char filepath[MAX_PATH_LEN + 1];
    snprintf(filepath, sizeof(filepath), "%s/docs.pdl", pkg_dir);

    FILE *f = fopen(filepath, "r");
    if (!f) {
        n_docs = 0;
        return;
    }

    char line[MAX_TITLE_LEN + MAX_PATH_LEN + 10];
    n_docs = 0;

    while (n_docs < MAX_DOCS && fgets(line, sizeof(line), f)) {
        int len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[--len] = '\0';
        }

        /* Skip empty and comment lines */
        if (len == 0 || line[0] == '#') {
            continue;
        }

        /* Find first pipe character */
        char *pipe = strchr(line, '|');
        if (!pipe) {
            continue;
        }

        /* Extract title (before pipe) */
        int title_len = pipe - line;
        if (title_len > MAX_TITLE_LEN) {
            title_len = MAX_TITLE_LEN;
        }
        strncpy(docs[n_docs].title, line, title_len);
        docs[n_docs].title[title_len] = '\0';
        trim_trailing(docs[n_docs].title);

        /* Extract path (after pipe) */
        const char *path_start = pipe + 1;
        int path_len = strlen(path_start);
        if (path_len > MAX_PATH_LEN) {
            path_len = MAX_PATH_LEN;
        }
        strncpy(docs[n_docs].path, path_start, path_len);
        docs[n_docs].path[path_len] = '\0';
        trim_trailing(docs[n_docs].path);

        n_docs++;
    }

    fclose(f);
}

/* Clear action file on startup */
static void clear_action_file(void) {
    char filepath[MAX_PATH_LEN + 1];
    snprintf(filepath, sizeof(filepath), "%s/pdl_read_action.txt", package_dir);

    FILE *f = fopen(filepath, "w");
    if (f) {
        fprintf(f, "seq=0\ncmd=\n");
        fclose(f);
    }
}

/* Write escaped page content to file handle */
static void write_escaped_page_content(FILE *f) {
    if (current_doc < 0 || page_count == 0) {
        return;  /* No document or no pages: write nothing */
    }

    int first_line = current_page * LINES_PER_PAGE;
    int last_line = first_line + LINES_PER_PAGE - 1;
    int current_line = 0;
    int should_write = (current_line >= first_line);

    for (int i = 0; i < content_size; i++) {
        if (current_line > last_line) {
            break;
        }

        unsigned char c = content_buffer[i];

        if (should_write) {
            if (c == '\n') {
                fprintf(f, "\\n");
            } else if (c == '\\') {
                fprintf(f, "\\\\");
            } else {
                fprintf(f, "%c", c);
            }
        }

        if (c == '\n') {
            current_line++;
            should_write = (current_line >= first_line && current_line <= last_line);
        }
    }
}

/* Write the UI file with current state */
static void write_ui_file(void) {
    char filepath[MAX_PATH_LEN + 1];
    snprintf(filepath, sizeof(filepath), "%s/pdl_read_ui.txt", package_dir);

    FILE *f = fopen(filepath, "w");
    if (!f) {
        return;
    }

    /* Write n_docs */
    fprintf(f, "n_docs=%d\n", n_docs);

    /* Write doc titles */
    for (int i = 0; i < n_docs; i++) {
        fprintf(f, "doc_%d_title=%s\n", i, docs[i].title);
    }

    /* Write doc_open */
    int is_open = (current_doc >= 0) ? 1 : 0;
    fprintf(f, "doc_open=%d\n", is_open);

    /* Write doc_title */
    if (is_open) {
        fprintf(f, "doc_title=%s\n", docs[current_doc].title);
    } else {
        fprintf(f, "doc_title=\n");
    }

    /* Write page_num (1-based for display, 0 if no document) */
    int page_num_display = is_open ? (current_page + 1) : 0;
    fprintf(f, "page_num=%d\n", page_num_display);

    /* Write page_count */
    fprintf(f, "page_count=%d\n", page_count);

    /* Write page_text with escaping */
    fprintf(f, "page_text=");
    write_escaped_page_content(f);
    fprintf(f, "\n");

    fclose(f);
}

/* Handle OPEN:<N> command */
static void handle_open(int doc_idx) {
    if (doc_idx < 0 || doc_idx >= n_docs) {
        return;  /* Out of range */
    }

    /* Try to read the document file */
    FILE *f = fopen(docs[doc_idx].path, "rb");
    if (!f) {
        /* File open failed: use error message */
        const char *errmsg = "(could not open this file)";
        int msglen = strlen(errmsg);
        if (msglen > MAX_CONTENT_SIZE) {
            msglen = MAX_CONTENT_SIZE;
        }
        strncpy(content_buffer, errmsg, msglen);
        content_size = msglen;
    } else {
        /* Read up to MAX_CONTENT_SIZE bytes */
        content_size = fread(content_buffer, 1, MAX_CONTENT_SIZE, f);
        fclose(f);
    }

    current_doc = doc_idx;
    current_page = 0;
    page_count = calculate_page_count();

    write_ui_file();
}

/* Handle NEXT command */
static void handle_next(void) {
    if (current_doc < 0) {
        return;  /* No document open */
    }

    if (current_page >= page_count - 1) {
        return;  /* Already at last page */
    }

    current_page++;
    write_ui_file();
}

/* Handle PREV command */
static void handle_prev(void) {
    if (current_doc < 0) {
        return;  /* No document open */
    }

    if (current_page <= 0) {
        return;  /* Already at first page */
    }

    current_page--;
    write_ui_file();
}

/* REAL, NEW 2026-09-05, direct live request ("the sidebar[should let]
 * the user still be able to change pages, like pdf") - jump straight
 * to a given page (a real page-nav sidebar, not just Prev/Next). */
static void handle_gotopage(int page_idx) {
    if (current_doc < 0) {
        return;  /* No document open */
    }
    if (page_idx < 0 || page_idx >= page_count) {
        return;  /* Out of range */
    }
    current_page = page_idx;
    write_ui_file();
}

/* Execute a command string */
static void execute_command(const char *cmd) {
    if (!cmd || cmd[0] == '\0') {
        return;
    }

    if (strncmp(cmd, "OPEN:", 5) == 0) {
        int doc_idx = atoi(cmd + 5);
        handle_open(doc_idx);
    } else if (strncmp(cmd, "GOTOPAGE:", 9) == 0) {
        int page_idx = atoi(cmd + 9);
        handle_gotopage(page_idx);
    } else if (strcmp(cmd, "NEXT") == 0) {
        handle_next();
    } else if (strcmp(cmd, "PREV") == 0) {
        handle_prev();
    }
    /* Unrecognized commands are silently ignored */
}

/* Poll action file for new commands */
static void poll_action_file(int *last_seq) {
    char filepath[MAX_PATH_LEN + 1];
    snprintf(filepath, sizeof(filepath), "%s/pdl_read_action.txt", package_dir);

    FILE *f = fopen(filepath, "r");
    if (!f) {
        return;
    }

    char buffer[ACTION_FILE_BUF_SIZE];
    int nread = fread(buffer, 1, ACTION_FILE_BUF_SIZE - 1, f);
    fclose(f);

    if (nread <= 0) {
        return;
    }
    buffer[nread] = '\0';

    /* Parse seq and cmd from buffer */
    int seq = 0;
    static char cmd_buf[1024];
    cmd_buf[0] = '\0';

    char *line_start = buffer;
    while (*line_start != '\0') {
        char *line_end = strchr(line_start, '\n');
        int line_len;

        if (line_end) {
            line_len = line_end - line_start;
        } else {
            line_len = strlen(line_start);
        }

        if (strncmp(line_start, "seq=", 4) == 0) {
            seq = atoi(line_start + 4);
        } else if (strncmp(line_start, "cmd=", 4) == 0) {
            int cmd_len = line_len - 4;
            if (cmd_len > (int)sizeof(cmd_buf) - 1) {
                cmd_len = sizeof(cmd_buf) - 1;
            }
            if (cmd_len > 0) {
                strncpy(cmd_buf, line_start + 4, cmd_len);
            }
            cmd_buf[cmd_len] = '\0';
        }

        if (line_end) {
            line_start = line_end + 1;
        } else {
            break;
        }
    }

    /* Execute command only if seq is newer and command is not empty */
    if (seq > *last_seq && cmd_buf[0] != '\0') {
        *last_seq = seq;
        execute_command(cmd_buf);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <house_root> <package_dir> <unused>\n", argv[0]);
        return 1;
    }

    strncpy(package_dir, argv[2], MAX_PATH_LEN);
    package_dir[MAX_PATH_LEN] = '\0';

    /* Startup sequence */
    read_docs_list(package_dir);
    clear_action_file();
    write_ui_file();

    int last_seq = 0;

    /* Main polling loop */
    while (1) {
        usleep(50000);  /* 50 milliseconds */
        poll_action_file(&last_seq);
    }

    return 0;  /* Never reached */
}
