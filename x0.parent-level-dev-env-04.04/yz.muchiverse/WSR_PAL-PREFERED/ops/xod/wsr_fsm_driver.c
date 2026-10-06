/* wsr_fsm_driver - XOD Behavior Bank: reads WSR frame, emits key presses.
 *
 * REAL FRAME PARSING (replaces mock data):
 *   - Reads pieces/display/current_frame.txt (rendered menu text)
 *   - Reads pieces/display/current_layout.txt (which piece.pdl is showing)
 *   - Parses the frame to find the highlighted menu item ([>] cursor)
 *   - Maps menu items to row numbers for key injection
 *
 * BEHAVIOR BANK (unified op):
 *   argv[1] = behavior name
 *   argv[2] = optional param
 *
 * Real behaviors mapped to piece.pdl rows:
 *   end_turn        -> row 17 (TICK_ALL:1)
 *   buy_stock       -> row 11 (RUN:player_trade @corp buy 10)
 *   sell_stock      -> row 14 (RUN:player_trade @corp sell 10)
 *   buy_sell        -> row 12 (GOTO:wsr_trade_menu)
 *   new_game        -> row 30 (START_WIZARD:new_game)
 *   cycle_corp      -> row 8  (CYCLE_CORP)
 *   list_portfolio  -> row 24 (RUN:player_list_portfolio)
 *   check_market    -> row 11 (Buy Stock opens trade view)
 *   navigate_to     -> type row number digits then Enter
 *   back_to_main    -> from trade menu, row 5 (Back to main menu)
 *
 * OUTPUT:
 *   writes KEY_PRESSED codes to pieces/keyboard/history.txt
 *   writes bare decimals to pieces/apps/player_app/history.txt
 *   emits KEY_INJECTED|row|behavior to interact_relay.txt
 *   emits BEHAVIOR_START|behavior to interact_relay.txt
 *   emits BEHAVIOR_COMPLETE|behavior|result to interact_relay.txt
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#define access _access
#define F_OK 0
#else
#include <unistd.h>
#endif

#define MAX_LINE 4096
#define PATH_BUF (4096 + 512)

static char project_root[PATH_BUF] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void append_line(const char *path, const char *line) {
    FILE *f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "%s\n", line);
    fflush(f);
    fclose(f);
}

/* Read the first line of a file into buf. Returns 1 on success. */
static int read_first_line(const char *path, char *buf, size_t sz) {
    FILE *f = fopen(path, "r");
    if (!f) { buf[0] = '\0'; return 0; }
    if (!fgets(buf, (int)sz, f)) buf[0] = '\0';
    fclose(f);
    buf[strcspn(buf, "\r\n")] = '\0';
    return buf[0] != '\0';
}

/* Read an entire file into buf (up to sz-1 bytes). */
static int read_file(const char *path, char *buf, size_t sz) {
    FILE *f = fopen(path, "r");
    if (!f) { buf[0] = '\0'; return 0; }
    size_t n = fread(buf, 1, sz - 1, f);
    buf[n] = '\0';
    fclose(f);
    return n > 0;
}

/* Check whether layout_path indicates the main menu. */
static int is_main_menu(const char *layout_path) {
    const char *base = strrchr(layout_path, '/');
    if (!base) base = layout_path;
    return strstr(base, "wsr_main_menu") != NULL;
}

static int is_trade_menu(const char *layout_path) {
    const char *base = strrchr(layout_path, '/');
    if (!base) base = layout_path;
    return strstr(base, "wsr_trade_menu") != NULL;
}

/* Parse the frame text to find which menu item is currently selected.
 * The frame uses [>] for the cursor and [ ] for unselected items.
 * Returns the 1-based row number, or 0 if not found. */
static int parse_cursor_row(const char *frame) {
    const char *p = frame;
    while ((p = strstr(p, "[>]")) != NULL) {
        /* Look backward for the row number preceding this [>] marker. */
        const char *num_start = p - 1;
        while (num_start > frame && (*(num_start - 1) >= '0' && *(num_start - 1) <= '9')) {
            num_start--;
        }
        if (num_start > frame && *(num_start - 1) == ' ') {
            int row = atoi(num_start);
            if (row >= 1 && row <= 99) return row;
        }
        p += 3;
    }
    return 0;
}

/* Find a menu item label in the frame text. Returns 1 if found. */
static int frame_has(const char *frame, const char *label) {
    return strstr(frame, label) != NULL;
}

/* Emit a key sequence: for row N, emit the row digits then Enter (13).
 * Rows 1-9: single ASCII digit '1'-'9' then Enter.
 * Rows 10-99: bare integer digits then Enter. */
static void emit_row(int row, const char *behavior) {
    char kpath[PATH_BUF];
    char hpath[PATH_BUF];
    char epath[PATH_BUF];

    snprintf(kpath, sizeof(kpath), "%s/pieces/keyboard/history.txt", project_root);
    snprintf(hpath, sizeof(hpath), "%s/pieces/apps/player_app/history.txt", project_root);
    snprintf(epath, sizeof(epath), "%s/pieces/apps/player_app/interact_relay.txt", project_root);

    /* Build the key sequence string */
    char seq[32];
    if (row >= 1 && row <= 9) {
        snprintf(seq, sizeof(seq), "%c\n%d", '0' + row, 13);
    } else if (row >= 10 && row <= 99) {
        snprintf(seq, sizeof(seq), "%d\n%d", row, 13);
    } else {
        return;
    }

    /* Append to keyboard history (KEY_PRESSED format for chtpm bridge) */
    {
        FILE *f = fopen(kpath, "a");
        if (f) {
            /* Emit each character as KEY_PRESSED: <code> */
            for (int i = 0; seq[i]; i++) {
                if (seq[i] == '\n') continue;
                fprintf(f, "KEY_PRESSED: %d\n", (unsigned char)seq[i]);
            }
            fclose(f);
        }
    }

    /* Append bare decimals to prisc+x read_history path */
    {
        FILE *f = fopen(hpath, "a");
        if (f) {
            for (int i = 0; seq[i]; i++) {
                if (seq[i] == '\n') continue;
                fprintf(f, "%d\n", (unsigned char)seq[i]);
            }
            fclose(f);
        }
    }

    /* Event bus */
    char evt[MAX_LINE];
    snprintf(evt, sizeof(evt), "KEY_INJECTED|%d|%s", row, behavior);
    append_line(epath, evt);
}

/* Emit a behavior: logs start/complete and delegates to emit_row. */
static void run_behavior(int row, const char *behavior) {
    char epath[PATH_BUF];
    snprintf(epath, sizeof(epath), "%s/pieces/apps/player_app/interact_relay.txt", project_root);

    char evt[MAX_LINE];
    snprintf(evt, sizeof(evt), "BEHAVIOR_START|%s", behavior);
    append_line(epath, evt);

    emit_row(row, behavior);

    snprintf(evt, sizeof(evt), "BEHAVIOR_COMPLETE|%s|ok", behavior);
    append_line(epath, evt);
}

/* Map behavior name to a row number in wsr_main_menu.
 * These match the actual piece.pdl row ordering. */
static int main_menu_row(const char *behavior) {
    if (strcmp(behavior, "new_game") == 0)       return 30;
    if (strcmp(behavior, "end_turn") == 0)       return 17;
    if (strcmp(behavior, "buy_stock") == 0)      return 11;
    if (strcmp(behavior, "sell_stock") == 0)     return 14;
    if (strcmp(behavior, "buy_sell") == 0)       return 12;
    if (strcmp(behavior, "cycle_corp") == 0)     return 8;
    if (strcmp(behavior, "list_portfolio") == 0) return 24;
    if (strcmp(behavior, "check_market") == 0)   return 11;
    return 0;
}

/* Map behavior name to a row number in wsr_trade_menu. */
static int trade_menu_row(const char *behavior) {
    if (strcmp(behavior, "buy_stock") == 0)    return 1;
    if (strcmp(behavior, "sell_stock") == 0)   return 2;
    if (strcmp(behavior, "short_stock") == 0)  return 3;
    if (strcmp(behavior, "cover_stock") == 0)  return 4;
    if (strcmp(behavior, "back_to_main") == 0) return 5;
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <behavior> [param]\n", argv[0]);
        return 1;
    }
    resolve_root();

    char *behavior = argv[1];
    char param[256] = "";
    if (argc >= 3) snprintf(param, sizeof(param), "%s", argv[2]);

    char frame_path[PATH_BUF];
    char layout_path[PATH_BUF];
    snprintf(frame_path, sizeof(frame_path), "%s/pieces/display/current_frame.txt", project_root);
    snprintf(layout_path, sizeof(layout_path), "%s/pieces/display/current_layout.txt", project_root);

    char frame[MAX_LINE] = "";
    char layout[MAX_LINE] = "";

    read_file(frame_path, frame, sizeof(frame));
    read_first_line(layout_path, layout, sizeof(layout));

    /* Event bus: BEHAVIOR_START */
    {
        char epath[PATH_BUF];
        snprintf(epath, sizeof(epath), "%s/pieces/apps/player_app/interact_relay.txt", project_root);
        char evt[MAX_LINE];
        if (param[0])
            snprintf(evt, sizeof(evt), "BEHAVIOR_START|%s|%s", behavior, param);
        else
            snprintf(evt, sizeof(evt), "BEHAVIOR_START|%s", behavior);
        append_line(epath, evt);
    }

    int row = 0;

    if (strcmp(behavior, "navigate_to") == 0) {
        /* navigate_to takes a target label; find it in the frame and
         * emit its row number. Falls back to param as a raw row number. */
        if (param[0]) {
            /* Try param as row number first */
            row = atoi(param);
            if (row <= 0) {
                /* Search frame for the label */
                /* Simple approach: scan for "N. [label]" pattern */
                char search[64];
                snprintf(search, sizeof(search), "[%s", param);
                const char *found = strstr(frame, search);
                if (found && found > frame) {
                    const char *num_start = found - 1;
                    while (num_start > frame && *(num_start - 1) >= '0' && *(num_start - 1) <= '9') {
                        num_start--;
                    }
                    row = atoi(num_start);
                }
            }
        }
        if (row <= 0) row = 1; /* fallback: first item */
    }
    else if (strcmp(behavior, "select_item") == 0) {
        /* Select the currently highlighted item (Enter = 13). */
        row = parse_cursor_row(frame);
        if (row <= 0) row = 1;
    }
    else if (strcmp(behavior, "new_game") == 0) {
        row = main_menu_row("new_game");
        if (row <= 0) row = 30;
    }
    else if (strcmp(behavior, "end_turn") == 0) {
        if (is_trade_menu(layout)) {
            row = trade_menu_row("back_to_main");
            if (row > 0) run_behavior(row, "back_to_main");
            row = 0;
        }
        row = main_menu_row("end_turn");
    }
    else if (strcmp(behavior, "buy_stock") == 0) {
        if (is_trade_menu(layout)) {
            row = trade_menu_row("buy_stock");
        } else if (is_main_menu(layout)) {
            row = main_menu_row("buy_stock");
        }
    }
    else if (strcmp(behavior, "sell_stock") == 0) {
        if (is_trade_menu(layout)) {
            row = trade_menu_row("sell_stock");
        } else if (is_main_menu(layout)) {
            row = main_menu_row("sell_stock");
        }
    }
    else if (strcmp(behavior, "short_stock") == 0) {
        if (is_trade_menu(layout)) {
            row = trade_menu_row("short_stock");
        }
    }
    else if (strcmp(behavior, "cover_stock") == 0) {
        if (is_trade_menu(layout)) {
            row = trade_menu_row("cover_stock");
        }
    }
    else if (strcmp(behavior, "buy_sell") == 0) {
        if (is_main_menu(layout)) {
            row = main_menu_row("buy_sell");
        } else if (is_trade_menu(layout)) {
            row = trade_menu_row("back_to_main");
            if (row > 0) run_behavior(row, "back_to_main");
            row = 0;
        }
    }
    else if (strcmp(behavior, "cycle_corp") == 0) {
        row = main_menu_row("cycle_corp");
    }
    else if (strcmp(behavior, "list_portfolio") == 0) {
        row = main_menu_row("list_portfolio");
    }
    else if (strcmp(behavior, "check_market") == 0) {
        row = main_menu_row("check_market");
    }
    else if (strcmp(behavior, "back_to_main") == 0) {
        if (is_trade_menu(layout)) {
            row = trade_menu_row("back_to_main");
        }
    }
    else {
        fprintf(stderr, "Unknown behavior: %s\n", behavior);
        return 1;
    }

    if (row > 0) run_behavior(row, behavior);

    return 0;
}