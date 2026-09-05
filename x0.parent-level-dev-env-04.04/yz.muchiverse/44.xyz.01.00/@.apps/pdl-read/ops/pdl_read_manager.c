#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* pdl_read_manager: simple paginated document reader backend
 * Invocation: pdl_read_manager <house_root> <package_dir> <unused>
 *
 * REAL, NEW 2026-09-05, direct live request ("pdl read should read dox
 * from 'dox' in 1.hq") - the doc list is now a live scan of
 * <house_root>/#.DOX (flat files only, alphabetical), not a hand-
 * curated docs.pdl - real, discoverable house content instead of a
 * fixed list that goes stale.
 *
 * REAL, NEW 2026-09-05, direct live request ("should have file
 * explorer 'file' button in its header to open other files") - also
 * polls the File Explorer widget's own real result state
 * (<house_root>/&.widgits/file-explorer/file_explorer_ui.txt, its
 * fixed real location) for a NEW result_action=LOAD - when the
 * "file" button (a plain shell command in the xhtpm, launches
 * file-explorer's own button.sh, no new dispatch verb needed) leads
 * to a real pick, that exact path is opened directly, ad-hoc,
 * bypassing the #.DOX index entirely (current_doc stays -1; a
 * separate adhoc_open flag + adhoc_title track this state instead).
 */
#define _POSIX_C_SOURCE 200809L
#include <dirent.h>

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
static char house_root[MAX_PATH_LEN + 1];
/* ad-hoc file opened via the File Explorer "file" button, NOT one of
 * the #.DOX-scanned docs[] entries - current_doc stays -1 while this
 * is set, so handle_open()'s own doc-index logic is never confused by
 * it; adhoc_open is the real "is one currently showing" flag. */
static int adhoc_open = 0;
static char adhoc_title[MAX_TITLE_LEN + 1];
/* last real result string seen from file-explorer's own ui.txt, so a
 * STALE result left over from a previous pick (the file never gets
 * cleared after this program reads it - that file belongs to a
 * different, independent process) is never re-opened as if new. */
static char last_fe_result[MAX_PATH_LEN + 1] = "";

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

/* qsort comparator - alphabetical by title, case-sensitive (matches
 * file_explorer_manager.c's own real sort convention). */
static int doc_cmp(const void *a, const void *b) {
    return strcmp(((const Document *)a)->title, ((const Document *)b)->title);
}

/* REAL, NEW 2026-09-05 - live scan of <house_root>/#.DOX (flat files
 * only - no recursion, matches what's actually there: a plain
 * directory of real house docs, not a nested tree). Replaces the
 * original hand-curated docs.pdl approach entirely - a real,
 * discoverable list instead of one that goes stale the moment a new
 * doc is added to #.DOX and nobody remembers to also edit a second
 * file. Skips dotfiles and subdirectories (a real, future "browse
 * subdirs too" enhancement is out of this pass's scope - #.DOX today
 * is flat). */
static void read_docs_list(const char *hroot) {
    char dox_dir[MAX_PATH_LEN + 1];
    snprintf(dox_dir, sizeof(dox_dir), "%s/#.DOX", hroot);

    n_docs = 0;
    DIR *d = opendir(dox_dir);
    if (!d) return;

    struct dirent *e;
    while (n_docs < MAX_DOCS && (e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue; /* dotfiles, "." and ".." */

        char full_path[MAX_PATH_LEN + 1];
        snprintf(full_path, sizeof(full_path), "%s/%s", dox_dir, e->d_name);
        struct stat st;
        if (stat(full_path, &st) != 0 || !S_ISREG(st.st_mode)) continue; /* files only, no subdirs */

        snprintf(docs[n_docs].title, sizeof(docs[n_docs].title), "%s", e->d_name);
        snprintf(docs[n_docs].path, sizeof(docs[n_docs].path), "%s", full_path);
        n_docs++;
    }
    closedir(d);

    qsort(docs, n_docs, sizeof(Document), doc_cmp);
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
    if ((current_doc < 0 && !adhoc_open) || page_count == 0) {
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

    /* Write doc_open - true for either a #.DOX-indexed pick OR a real
     * ad-hoc file opened via the "file" button (File Explorer). */
    int is_open = (current_doc >= 0 || adhoc_open) ? 1 : 0;
    fprintf(f, "doc_open=%d\n", is_open);

    /* Write doc_title */
    if (current_doc >= 0) {
        fprintf(f, "doc_title=%s\n", docs[current_doc].title);
    } else if (adhoc_open) {
        fprintf(f, "doc_title=%s\n", adhoc_title);
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

/* Real, shared file-load - both a #.DOX-indexed pick (handle_open)
 * and a real ad-hoc file (poll_file_explorer_pick, opened via the
 * "file" button) load through this one function, not two copies. */
static void load_content_from_path(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        const char *errmsg = "(could not open this file)";
        int msglen = strlen(errmsg);
        if (msglen > MAX_CONTENT_SIZE) msglen = MAX_CONTENT_SIZE;
        memcpy(content_buffer, errmsg, msglen);
        content_size = msglen;
    } else {
        content_size = fread(content_buffer, 1, MAX_CONTENT_SIZE, f);
        fclose(f);
    }
    current_page = 0;
    page_count = calculate_page_count();
}

/* Handle OPEN:<N> command - a real #.DOX-indexed pick. */
static void handle_open(int doc_idx) {
    if (doc_idx < 0 || doc_idx >= n_docs) {
        return;  /* Out of range */
    }
    adhoc_open = 0; /* a real indexed pick always supersedes any ad-hoc file that was open */
    current_doc = doc_idx;
    load_content_from_path(docs[doc_idx].path);
    write_ui_file();
}

/* Handle NEXT command */
static void handle_next(void) {
    if (current_doc < 0 && !adhoc_open) {
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
    if (current_doc < 0 && !adhoc_open) {
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
    if (current_doc < 0 && !adhoc_open) {
        return;  /* No document open */
    }
    if (page_idx < 0 || page_idx >= page_count) {
        return;  /* Out of range */
    }
    current_page = page_idx;
    write_ui_file();
}

/* Real, small key=value line reader - same generic shape used all
 * through this house's own real .pdl/state-file convention. `key`
 * must include the trailing '=' already. */
static void read_kv_line(const char *path, const char *key, char *out, size_t outsz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_PATH_LEN];
    size_t klen = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, klen) == 0) {
            line[strcspn(line, "\r\n")] = '\0';
            snprintf(out, outsz, "%s", line + klen);
            break;
        }
    }
    fclose(f);
}

/* REAL, NEW 2026-09-05, direct live request ("should have file
 * explorer 'file' button in its header to open other files") - polls
 * the File Explorer widget's own real, independent result state for a
 * NEW result_action=LOAD (the "file" button just launches its
 * button.sh, a plain shell command - no coordination beyond reading
 * its already-real, already-published result). last_fe_result guards
 * against re-opening the SAME stale result forever (that file is
 * never cleared by anyone once written - it's a different process's
 * own state, not this program's to reset). */
static void poll_file_explorer_pick(void) {
    char fe_ui_path[MAX_PATH_LEN + 1];
    snprintf(fe_ui_path, sizeof(fe_ui_path), "%s/&.widgits/file-explorer/file_explorer_ui.txt", house_root);

    char result_action[32], result[MAX_PATH_LEN];
    read_kv_line(fe_ui_path, "result_action=", result_action, sizeof(result_action));
    read_kv_line(fe_ui_path, "result=", result, sizeof(result));

    if (strcmp(result_action, "LOAD") != 0) return;
    if (result[0] == '\0') return;
    if (strcmp(result, last_fe_result) == 0) return; /* already opened this exact pick */

    snprintf(last_fe_result, sizeof(last_fe_result), "%s", result);

    /* basename(result), real display title for an ad-hoc file - no
     * #.DOX index entry exists for it, so there's no docs[].title to
     * borrow. */
    const char *base = strrchr(result, '/');
    base = base ? base + 1 : result;
    snprintf(adhoc_title, sizeof(adhoc_title), "%s", base);

    current_doc = -1; /* an ad-hoc pick always supersedes any #.DOX-indexed doc that was open */
    adhoc_open = 1;
    load_content_from_path(result);
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

    strncpy(house_root, argv[1], MAX_PATH_LEN);
    house_root[MAX_PATH_LEN] = '\0';
    strncpy(package_dir, argv[2], MAX_PATH_LEN);
    package_dir[MAX_PATH_LEN] = '\0';

    /* Startup sequence */
    read_docs_list(house_root);
    clear_action_file();
    write_ui_file();

    int last_seq = 0;

    /* Main polling loop */
    while (1) {
        usleep(50000);  /* 50 milliseconds */
        poll_action_file(&last_seq);
        poll_file_explorer_pick();
    }

    return 0;  /* Never reached */
}
