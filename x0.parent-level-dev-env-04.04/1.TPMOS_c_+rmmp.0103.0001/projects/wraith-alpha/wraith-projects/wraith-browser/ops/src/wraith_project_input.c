#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define MAX_PATH_LEN 1024
#define MAX_LINE_LEN 512
#define MAX_DOC_LINES 32
#define MAX_HISTORY 32

typedef struct {
    char project_id[128];
    char title[64];
    char mode[32];
    char status[64];
    char current_page[64];
    char address[128];
    char address_input[128];
    char input_mode[32];
    int search_submit_armed;
    int js_enabled;
    int history_index;
    char last_action[128];
} BrowserState;

typedef struct {
    int counter;
    char last_script[64];
    char last_result[128];
} ScriptState;

static void trim_newline(char *s) {
    if (!s) return;
    s[strcspn(s, "\r\n")] = '\0';
}

static void copy_value(char *dst, size_t dst_sz, const char *src) {
    if (!dst || dst_sz == 0) return;
    if (!src) src = "";
    snprintf(dst, dst_sz, "%.*s", (int)dst_sz - 1, src);
}

static void path_join(char *out, size_t out_sz, const char *root, const char *rel) {
    snprintf(out, out_sz, "%s/%s", root, rel);
}

/* 2026-07-11: reads CHROME_CONTENT_START's live value, published by
 * wraith-alpha_manager.c (see that file's publish_chrome_reserved_nav_count(),
 * added same day) to pieces/display/chrome_reserved_nav_count.txt, instead
 * of hardcoding this project's own guess about where its nav range may
 * safely start. This ops binary is fork+exec'd by
 * wraith-alpha_manager.c's run_active_project_input_op() WITHOUT a chdir(),
 * so it inherits the manager's own cwd -- the relative path below is
 * correct without combining it with `root` (which is this PROJECT's own
 * dir, not the repo root). Falls back to the literal this file always used
 * before if the manager hasn't published yet. */
static int read_chrome_content_start(int fallback) {
    FILE *f = fopen("pieces/display/chrome_reserved_nav_count.txt", "r");
    int value;
    if (!f) return fallback;
    if (fscanf(f, "%d", &value) != 1 || value <= 0) {
        fclose(f);
        return fallback;
    }
    fclose(f);
    return value;
}

static void defaults(BrowserState *st) {
    memset(st, 0, sizeof(*st));
    snprintf(st->project_id, sizeof(st->project_id), "wraith-alpha/wraith-projects/wraith-browser");
    snprintf(st->title, sizeof(st->title), "WRAITH BROWSER");
    snprintf(st->mode, sizeof(st->mode), "browser");
    snprintf(st->status, sizeof(st->status), "ready");
    snprintf(st->current_page, sizeof(st->current_page), "index.html");
    snprintf(st->address, sizeof(st->address), "local://index.html");
    snprintf(st->address_input, sizeof(st->address_input), "local://index.html");
    snprintf(st->input_mode, sizeof(st->input_mode), "search");
    st->search_submit_armed = 0;
    st->js_enabled = 1;
    st->history_index = 0;
    snprintf(st->last_action, sizeof(st->last_action), "Initialized browser");
}

static void load_state(const char *root, BrowserState *st) {
    char path[MAX_PATH_LEN];
    char line[MAX_LINE_LEN];
    FILE *f;

    defaults(st);
    path_join(path, sizeof(path), root, "session/state.txt");
    f = fopen(path, "r");
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        trim_newline(line);
        if (strncmp(line, "project_id=", 11) == 0) copy_value(st->project_id, sizeof(st->project_id), line + 11);
        else if (strncmp(line, "title=", 6) == 0) copy_value(st->title, sizeof(st->title), line + 6);
        else if (strncmp(line, "mode=", 5) == 0) copy_value(st->mode, sizeof(st->mode), line + 5);
        else if (strncmp(line, "status=", 7) == 0) copy_value(st->status, sizeof(st->status), line + 7);
        else if (strncmp(line, "current_page=", 13) == 0) copy_value(st->current_page, sizeof(st->current_page), line + 13);
        else if (strncmp(line, "address=", 8) == 0) copy_value(st->address, sizeof(st->address), line + 8);
        else if (strncmp(line, "address_input=", 14) == 0) copy_value(st->address_input, sizeof(st->address_input), line + 14);
        else if (strncmp(line, "input_mode=", 11) == 0) copy_value(st->input_mode, sizeof(st->input_mode), line + 11);
        else if (strncmp(line, "search_submit_armed=", 20) == 0) st->search_submit_armed = atoi(line + 20);
        else if (strncmp(line, "js_enabled=", 11) == 0) st->js_enabled = atoi(line + 11);
        else if (strncmp(line, "history_index=", 14) == 0) st->history_index = atoi(line + 14);
        else if (strncmp(line, "last_action=", 12) == 0) copy_value(st->last_action, sizeof(st->last_action), line + 12);
    }
    fclose(f);
    if (!st->address_input[0]) copy_value(st->address_input, sizeof(st->address_input), st->address);
    if (!st->input_mode[0]) copy_value(st->input_mode, sizeof(st->input_mode), "search");
    if (strcmp(st->status, "editing") != 0) {
        copy_value(st->address_input, sizeof(st->address_input), st->address);
    }
}

static void save_state(const char *root, const BrowserState *st) {
    char path[MAX_PATH_LEN];
    FILE *f;

    path_join(path, sizeof(path), root, "session/state.txt");
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "project_id=%s\n", st->project_id);
    fprintf(f, "title=%s\n", st->title);
    fprintf(f, "mode=%s\n", st->mode);
    fprintf(f, "status=%s\n", st->status);
    fprintf(f, "current_page=%s\n", st->current_page);
    fprintf(f, "address=%s\n", st->address);
    fprintf(f, "address_input=%s\n", st->address_input);
    fprintf(f, "input_mode=%s\n", st->input_mode);
    fprintf(f, "search_submit_armed=%d\n", st->search_submit_armed);
    fprintf(f, "js_enabled=%d\n", st->js_enabled);
    fprintf(f, "history_index=%d\n", st->history_index);
    fprintf(f, "last_action=%s\n", st->last_action);
    fclose(f);
}

static void load_script_state(const char *root, ScriptState *st) {
    char path[MAX_PATH_LEN];
    char line[MAX_LINE_LEN];
    FILE *f;

    memset(st, 0, sizeof(*st));
    snprintf(st->last_script, sizeof(st->last_script), "none");
    snprintf(st->last_result, sizeof(st->last_result), "Script op ready");
    path_join(path, sizeof(path), root, "session/script_state.txt");
    f = fopen(path, "r");
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        trim_newline(line);
        if (strncmp(line, "counter=", 8) == 0) st->counter = atoi(line + 8);
        else if (strncmp(line, "last_script=", 12) == 0) copy_value(st->last_script, sizeof(st->last_script), line + 12);
        else if (strncmp(line, "last_result=", 12) == 0) copy_value(st->last_result, sizeof(st->last_result), line + 12);
    }
    fclose(f);
}

static void append_receipt(const char *root, const char *message) {
    char path[MAX_PATH_LEN];
    FILE *f;

    path_join(path, sizeof(path), root, "session/receipts.txt");
    f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "%s\n", message);
    fclose(f);
}

static long read_cursor(const char *root) {
    char path[MAX_PATH_LEN];
    FILE *f;
    long cursor = 0;

    path_join(path, sizeof(path), root, "session/history.cursor");
    f = fopen(path, "r");
    if (!f) return 0;
    if (fscanf(f, "%ld", &cursor) != 1) cursor = 0;
    fclose(f);
    return cursor;
}

static void write_cursor(const char *root, long cursor) {
    char path[MAX_PATH_LEN];
    FILE *f;

    path_join(path, sizeof(path), root, "session/history.cursor");
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "%ld\n", cursor);
    fclose(f);
}

static int key_from_history_line(const char *line) {
    const char *p = strstr(line, "KEY_PRESSED:");
    if (!p) return -1;
    p += strlen("KEY_PRESSED:");
    while (*p && isspace((unsigned char)*p)) p++;
    return atoi(p);
}

static const char *command_from_history_line(const char *line) {
    const char *p = strstr(line, "COMMAND:");
    if (!p) return NULL;
    p += strlen("COMMAND:");
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

static int strip_tag_text(const char *line, char *tag, size_t tag_sz, char *value, size_t value_sz) {
    const char *open = strchr(line, '<');
    const char *close = strchr(line, '>');
    const char *end;
    size_t n;
    if (!open || !close || close <= open + 1) return 0;
    n = (size_t)(close - open - 1);
    if (n >= tag_sz) n = tag_sz - 1;
    memcpy(tag, open + 1, n);
    tag[n] = '\0';
    end = strstr(close + 1, "</");
    if (!end) end = line + strlen(line);
    n = (size_t)(end - (close + 1));
    if (n >= value_sz) n = value_sz - 1;
    memcpy(value, close + 1, n);
    value[n] = '\0';
    return 1;
}

static void update_history_file(const char *root, const BrowserState *st, int push_new) {
    char path[MAX_PATH_LEN];
    char items[MAX_HISTORY][64];
    int count = 0;
    FILE *f;
    char line[MAX_LINE_LEN];
    int i;

    path_join(path, sizeof(path), root, "session/nav_history.txt");
    f = fopen(path, "r");
    if (f) {
        while (count < MAX_HISTORY && fgets(line, sizeof(line), f)) {
            trim_newline(line);
            if (line[0]) copy_value(items[count++], sizeof(items[count]), line);
        }
        fclose(f);
    }

    if (count == 0) {
        copy_value(items[count++], sizeof(items[0]), st->current_page);
    }

    if (push_new) {
        if (st->history_index < count - 1) count = st->history_index + 1;
        if (count >= MAX_HISTORY) {
            for (i = 1; i < count; i++) copy_value(items[i - 1], sizeof(items[0]), items[i]);
            count = MAX_HISTORY - 1;
        }
        copy_value(items[count++], sizeof(items[0]), st->current_page);
    }

    f = fopen(path, "w");
    if (!f) return;
    for (i = 0; i < count; i++) fprintf(f, "%s\n", items[i]);
    fclose(f);
}

static int load_history_file(const char *root, char items[MAX_HISTORY][64]) {
    char path[MAX_PATH_LEN];
    char line[MAX_LINE_LEN];
    FILE *f;
    int count = 0;

    path_join(path, sizeof(path), root, "session/nav_history.txt");
    f = fopen(path, "r");
    if (!f) return 0;
    while (count < MAX_HISTORY && fgets(line, sizeof(line), f)) {
        trim_newline(line);
        if (line[0]) copy_value(items[count++], sizeof(items[count]), line);
    }
    fclose(f);
    return count;
}

static void navigate_to(BrowserState *st, const char *page) {
    copy_value(st->current_page, sizeof(st->current_page), page);
    snprintf(st->address, sizeof(st->address), "local://%s", st->current_page);
    copy_value(st->address_input, sizeof(st->address_input), st->address);
    st->search_submit_armed = 0;
    snprintf(st->status, sizeof(st->status), "navigated");
    snprintf(st->last_action, sizeof(st->last_action), "Loaded %s", st->current_page);
}

static void navigate_to_address_input(BrowserState *st) {
    const char *addr = st->address_input;
    const char *page = addr;
    if (strncmp(addr, "local://", 8) == 0) page = addr + 8;
    while (*page == '/') page++;
    if (!*page) page = "index.html";
    navigate_to(st, page);
}

static void process_browser_command(const char *root, BrowserState *st, const char *command) {
    char trimmed[MAX_LINE_LEN];
    char history[MAX_HISTORY][64];
    int count;
    char op_path[MAX_PATH_LEN];
    char cmd[MAX_PATH_LEN * 2];
    int rc;

    if (!command || !command[0]) return;
    copy_value(trimmed, sizeof(trimmed), command);
    trim_newline(trimmed);

    if (strncmp(trimmed, "BROWSER_NAV:", 12) == 0) {
        navigate_to(st, trimmed + 12);
        st->history_index++;
        update_history_file(root, st, 1);
        append_receipt(root, trimmed);
        return;
    }
    if (strcmp(trimmed, "BROWSER_BACK") == 0) {
        count = load_history_file(root, history);
        if (count > 0 && st->history_index > 0 && st->history_index < count) {
            st->history_index--;
            navigate_to(st, history[st->history_index]);
        } else {
            snprintf(st->last_action, sizeof(st->last_action), "Back unavailable");
        }
        append_receipt(root, trimmed);
        return;
    }
    if (strcmp(trimmed, "BROWSER_FORWARD") == 0) {
        count = load_history_file(root, history);
        if (count > 0 && st->history_index + 1 < count) {
            st->history_index++;
            navigate_to(st, history[st->history_index]);
        } else {
            snprintf(st->last_action, sizeof(st->last_action), "Forward unavailable");
        }
        append_receipt(root, trimmed);
        return;
    }
    if (strcmp(trimmed, "BROWSER_RELOAD") == 0) {
        snprintf(st->status, sizeof(st->status), "reloaded");
        snprintf(st->last_action, sizeof(st->last_action), "Reloaded %s", st->current_page);
        copy_value(st->address_input, sizeof(st->address_input), st->address);
        append_receipt(root, trimmed);
        return;
    }
    if (strcmp(trimmed, "BROWSER_TOGGLE_JS") == 0) {
        st->js_enabled = st->js_enabled ? 0 : 1;
        snprintf(st->status, sizeof(st->status), "ready");
        snprintf(st->last_action, sizeof(st->last_action), "JS %s", st->js_enabled ? "enabled" : "disabled");
        append_receipt(root, trimmed);
        return;
    }
    if (strcmp(trimmed, "BROWSER_HOME") == 0) {
        navigate_to(st, "index.html");
        append_receipt(root, trimmed);
        return;
    }
    if (strcmp(trimmed, "BROWSER_AUDIT") == 0) {
        navigate_to(st, "audit.html");
        append_receipt(root, trimmed);
        return;
    }
    if (strcmp(trimmed, "BROWSER_JS_LAB") == 0) {
        navigate_to(st, "js-lab.html");
        append_receipt(root, trimmed);
        return;
    }
    if (strncmp(trimmed, "BROWSER_JS:", 11) == 0) {
        if (!st->js_enabled) {
            snprintf(st->last_action, sizeof(st->last_action), "JS disabled");
            append_receipt(root, "BROWSER_JS_BLOCKED");
            return;
        }
        path_join(op_path, sizeof(op_path), root, "ops/src/+x/browser_exec_js.+x");
        snprintf(cmd, sizeof(cmd), "'%s' '%s' '%s' >/dev/null 2>&1", op_path, root, trimmed + 11);
        rc = system(cmd);
        if (rc >= 0 && WIFEXITED(rc) && WEXITSTATUS(rc) == 0) {
            snprintf(st->last_action, sizeof(st->last_action), "JS %s", trimmed + 11);
            snprintf(st->status, sizeof(st->status), "scripted");
        } else {
            snprintf(st->last_action, sizeof(st->last_action), "JS op failed");
            snprintf(st->status, sizeof(st->status), "error");
        }
        append_receipt(root, trimmed);
        return;
    }
}

static void process_key(BrowserState *st, int key) {
    size_t len;

    if (strcmp(st->input_mode, "search") == 0) {
        len = strlen(st->address_input);
        if (key == 10 || key == 13) {
            if (st->search_submit_armed) {
                navigate_to_address_input(st);
            } else {
                st->search_submit_armed = 1;
                snprintf(st->status, sizeof(st->status), "editing");
                snprintf(st->last_action, sizeof(st->last_action), "Search armed");
            }
            return;
        }
        if (key == 127 || key == 8) {
            if (len > 0) st->address_input[len - 1] = '\0';
            st->search_submit_armed = 1;
            snprintf(st->last_action, sizeof(st->last_action), "Edited search");
            snprintf(st->status, sizeof(st->status), "editing");
            return;
        }
        if (key >= 32 && key <= 126 && len + 1 < sizeof(st->address_input)) {
            st->address_input[len] = (char)key;
            st->address_input[len + 1] = '\0';
            st->search_submit_armed = 1;
            snprintf(st->last_action, sizeof(st->last_action), "Edited search");
            snprintf(st->status, sizeof(st->status), "editing");
            return;
        }
    }

    if (key == '1') navigate_to(st, "index.html");
    else if (key == '2') navigate_to(st, "audit.html");
    else if (key == '3') navigate_to(st, "js-lab.html");
}

static void write_text_file(const char *path, const char *text) {
    FILE *f = fopen(path, "w");
    if (!f) return;
    fputs(text, f);
    fclose(f);
}

static void build_outputs(const char *root, const BrowserState *st, const ScriptState *script) {
    char page_path[MAX_PATH_LEN];
    char source_path[MAX_PATH_LEN];
    char dom_path[MAX_PATH_LEN];
    char layout_path[MAX_PATH_LEN];
    char body_path[MAX_PATH_LEN];
    char doc_text_path[MAX_PATH_LEN];
    char scene_path[MAX_PATH_LEN];
    char source_buf[8192] = "";
    char body_buf[16384] = "";
    char doc_buf[8192] = "";
    char dom_buf[8192] = "NODE root document\n";
    char layout_buf[4096] = "BOX browser_root x=0 y=0 w=1 h=1\n";
    char scene_buf[16384] = "";
    char line[MAX_LINE_LEN];
    char tag[64];
    char value[384];
    char doc_lines[MAX_DOC_LINES][384];
    int doc_count = 0;
    int nav_base = read_chrome_content_start(6);
    FILE *f;

    path_join(page_path, sizeof(page_path), root, st->current_page[0] ? "" : "");
    snprintf(page_path, sizeof(page_path), "%s/pages/%s", root, st->current_page);
    f = fopen(page_path, "r");
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            strncat(source_buf, line, sizeof(source_buf) - strlen(source_buf) - 1);
            trim_newline(line);
            if (strip_tag_text(line, tag, sizeof(tag), value, sizeof(value))) {
                if (doc_count < MAX_DOC_LINES && strcmp(tag, "title") != 0 && strcmp(tag, "script type=\"tpmos-js\"") != 0) {
                    if (strcmp(tag, "h1") == 0) snprintf(doc_lines[doc_count++], sizeof(doc_lines[0]), "[H1] %s", value);
                    else if (strcmp(tag, "h2") == 0) snprintf(doc_lines[doc_count++], sizeof(doc_lines[0]), "[H2] %s", value);
                    else if (strcmp(tag, "p") == 0) snprintf(doc_lines[doc_count++], sizeof(doc_lines[0]), "%s", value);
                    else if (strcmp(tag, "li") == 0) snprintf(doc_lines[doc_count++], sizeof(doc_lines[0]), "- %s", value);
                    else if (strncmp(tag, "a href=", 7) == 0) snprintf(doc_lines[doc_count++], sizeof(doc_lines[0]), "link: %s", value);
                    else snprintf(doc_lines[doc_count++], sizeof(doc_lines[0]), "%s", value);
                }
                strncat(dom_buf, "NODE ", sizeof(dom_buf) - strlen(dom_buf) - 1);
                strncat(dom_buf, tag, sizeof(dom_buf) - strlen(dom_buf) - 1);
                strncat(dom_buf, " ", sizeof(dom_buf) - strlen(dom_buf) - 1);
                strncat(dom_buf, value, sizeof(dom_buf) - strlen(dom_buf) - 1);
                strncat(dom_buf, "\n", sizeof(dom_buf) - strlen(dom_buf) - 1);
            }
        }
        fclose(f);
    } else {
        snprintf(source_buf, sizeof(source_buf), "<h1>Missing page</h1>\n<p>%s</p>\n", st->current_page);
        snprintf(doc_lines[doc_count++], sizeof(doc_lines[0]), "[H1] Missing page");
        snprintf(doc_lines[doc_count++], sizeof(doc_lines[0]), "%s", st->current_page);
    }

    snprintf(body_buf, sizeof(body_buf),
        "WRAITH BROWSER | TPMOS hybrid phase 1/2\n"
        "Address: %s\n"
        "Page: %s | Status: %s\n"
        "JS: %s | Last: %s\n"
        "Counter: %d | Script: %s | Result: %s\n"
        "%s 1. [%s]\n"
        "[ ] 2. [Home] [ ] 3. [Audit] [ ] 4. [JS_Lab]\n"
        "[ ] 5. [Back] [ ] 6. [Forward] [ ] 7. [Reload]\n"
        "[ ] 8. [Toggle_JS] [ ] 9. [JS+1] [ ] 10. [JS_Reset]\n"
        "------------------------------------------------------------\n",
        st->address,
        st->current_page,
        st->status,
        st->js_enabled ? "on" : "off",
        st->last_action,
        script->counter,
        script->last_script,
        script->last_result,
        strcmp(st->input_mode, "search") == 0 ? "[^]" : "[ ]",
        st->address_input);

    for (int i = 0; i < doc_count; i++) {
        strncat(body_buf, doc_lines[i], sizeof(body_buf) - strlen(body_buf) - 1);
        strncat(body_buf, "\n", sizeof(body_buf) - strlen(body_buf) - 1);
        strncat(doc_buf, doc_lines[i], sizeof(doc_buf) - strlen(doc_buf) - 1);
        strncat(doc_buf, "\n", sizeof(doc_buf) - strlen(doc_buf) - 1);
        snprintf(line, sizeof(line), "BOX line_%d x=0 y=%d w=1 h=1 text=%s\n", i, i + 1, doc_lines[i]);
        strncat(layout_buf, line, sizeof(layout_buf) - strlen(layout_buf) - 1);
    }

    path_join(source_path, sizeof(source_path), root, "session/source.html");
    path_join(dom_path, sizeof(dom_path), root, "session/dom_tree.pdl");
    path_join(layout_path, sizeof(layout_path), root, "session/layout_tree.pdl");
    path_join(body_path, sizeof(body_path), root, "session/wraith_body.txt");
    path_join(doc_text_path, sizeof(doc_text_path), root, "session/browser_gl_document.txt");
    path_join(scene_path, sizeof(scene_path), root, "session/scene.objects.pdl");

    write_text_file(source_path, source_buf);
    write_text_file(dom_path, dom_buf);
    write_text_file(layout_path, layout_buf);
    write_text_file(body_path, body_buf);
    write_text_file(doc_text_path, doc_buf);

    strncat(scene_buf, "OBJECT tag=panel id=browser_panel role=browser_panel x=16 y=4 w=98 h=30 z=24 nav=0 source=semantic:browser_panel fg=#E8F1F2 bg=#162534 border=#7EDFF2 action=- label=WRAITH_BROWSER\n", sizeof(scene_buf) - strlen(scene_buf) - 1);
    strncat(scene_buf, "OBJECT tag=text id=browser_title role=browser_header x=18 y=5 w=34 h=1 z=26 nav=0 source=semantic:browser_header fg=#E8F1F2 bg=#162534 border=#7EDFF2 action=- label=WRAITH_BROWSER\n", sizeof(scene_buf) - strlen(scene_buf) - 1);
    snprintf(line, sizeof(line), "OBJECT tag=control id=browser_search role=window_toolbar_item x=18 y=7 w=88 h=1 z=30 nav=%d source=semantic:browser_search fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=INTERACT label=%s src=\n", nav_base + 0, st->address_input);
    strncat(scene_buf, line, sizeof(scene_buf) - strlen(scene_buf) - 1);
    snprintf(line, sizeof(line), "OBJECT tag=control id=browser_home role=window_toolbar_item x=18 y=9 w=12 h=1 z=30 nav=%d source=semantic:browser_nav fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=PROJECT_ACTION:BROWSER_HOME label=Home src=\n", nav_base + 1);
    strncat(scene_buf, line, sizeof(scene_buf) - strlen(scene_buf) - 1);
    snprintf(line, sizeof(line), "OBJECT tag=control id=browser_audit role=window_toolbar_item x=31 y=9 w=12 h=1 z=30 nav=%d source=semantic:browser_nav fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=PROJECT_ACTION:BROWSER_AUDIT label=Audit src=\n", nav_base + 2);
    strncat(scene_buf, line, sizeof(scene_buf) - strlen(scene_buf) - 1);
    snprintf(line, sizeof(line), "OBJECT tag=control id=browser_js_lab role=window_toolbar_item x=44 y=9 w=14 h=1 z=30 nav=%d source=semantic:browser_nav fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=PROJECT_ACTION:BROWSER_JS_LAB label=JS_Lab src=\n", nav_base + 3);
    strncat(scene_buf, line, sizeof(scene_buf) - strlen(scene_buf) - 1);
    snprintf(line, sizeof(line), "OBJECT tag=control id=browser_back role=window_toolbar_item x=59 y=9 w=11 h=1 z=30 nav=%d source=semantic:browser_nav fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=PROJECT_ACTION:BROWSER_BACK label=Back src=\n", nav_base + 4);
    strncat(scene_buf, line, sizeof(scene_buf) - strlen(scene_buf) - 1);
    snprintf(line, sizeof(line), "OBJECT tag=control id=browser_forward role=window_toolbar_item x=71 y=9 w=14 h=1 z=30 nav=%d source=semantic:browser_nav fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=PROJECT_ACTION:BROWSER_FORWARD label=Forward src=\n", nav_base + 5);
    strncat(scene_buf, line, sizeof(scene_buf) - strlen(scene_buf) - 1);
    snprintf(line, sizeof(line), "OBJECT tag=control id=browser_reload role=window_toolbar_item x=86 y=9 w=12 h=1 z=30 nav=%d source=semantic:browser_nav fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=PROJECT_ACTION:BROWSER_RELOAD label=Reload src=\n", nav_base + 6);
    strncat(scene_buf, line, sizeof(scene_buf) - strlen(scene_buf) - 1);
    snprintf(line, sizeof(line), "OBJECT tag=control id=browser_toggle_js role=window_toolbar_item x=18 y=10 w=16 h=1 z=30 nav=%d source=semantic:browser_nav fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=PROJECT_ACTION:BROWSER_TOGGLE_JS label=Toggle_JS src=\n", nav_base + 7);
    strncat(scene_buf, line, sizeof(scene_buf) - strlen(scene_buf) - 1);
    snprintf(line, sizeof(line), "OBJECT tag=control id=browser_js_inc role=window_toolbar_item x=35 y=10 w=10 h=1 z=30 nav=%d source=semantic:browser_nav fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=PROJECT_ACTION:BROWSER_JS:counter_inc label=JS+1 src=\n", nav_base + 8);
    strncat(scene_buf, line, sizeof(scene_buf) - strlen(scene_buf) - 1);
    snprintf(line, sizeof(line), "OBJECT tag=control id=browser_js_reset role=window_toolbar_item x=46 y=10 w=14 h=1 z=30 nav=%d source=semantic:browser_nav fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=PROJECT_ACTION:BROWSER_JS:counter_reset label=JS_Reset src=\n", nav_base + 9);
    strncat(scene_buf, line, sizeof(scene_buf) - strlen(scene_buf) - 1);
    strncat(scene_buf, "OBJECT tag=panel id=browser_doc_panel role=browser_doc_panel x=18 y=12 w=88 h=20 z=24 nav=0 source=semantic:browser_doc_panel fg=#E8F1F2 bg=#1A2B3A border=#7EDFF2 action=- label=Document\n", sizeof(scene_buf) - strlen(scene_buf) - 1);
    snprintf(line, sizeof(line), "OBJECT tag=surface id=browser_doc_text role=text_asset x=20 y=13 w=84 h=17 z=28 nav=0 source_ref=%s fg=#E8F1F2 bg=#1A2B3A border=#1A2B3A action=- label=Browser_Document\n", doc_text_path);
    strncat(scene_buf, line, sizeof(scene_buf) - strlen(scene_buf) - 1);

    write_text_file(scene_path, scene_buf);
}

int main(int argc, char **argv) {
    char history_path[MAX_PATH_LEN];
    FILE *f;
    char line[MAX_LINE_LEN];
    long cursor;
    BrowserState st;
    ScriptState script;

    if (argc < 2) return 1;

    load_state(argv[1], &st);
    cursor = read_cursor(argv[1]);
    path_join(history_path, sizeof(history_path), argv[1], "session/history.txt");
    f = fopen(history_path, "r");
    if (f) {
        if (cursor > 0) fseek(f, cursor, SEEK_SET);
        while (fgets(line, sizeof(line), f)) {
            const char *command = command_from_history_line(line);
            int key = key_from_history_line(line);
            if (command) process_browser_command(argv[1], &st, command);
            else if (key > 0) process_key(&st, key);
        }
        cursor = ftell(f);
        fclose(f);
        write_cursor(argv[1], cursor);
    }

    load_script_state(argv[1], &script);
    if (strcmp(st.status, "editing") != 0) {
        copy_value(st.address_input, sizeof(st.address_input), st.address);
    }
    save_state(argv[1], &st);
    build_outputs(argv[1], &st, &script);
    return 0;
}
