/* fsm_controller - FSM Controller for XOD.
 *
 * Reads the LLM's structured decision from the event bus
 * (LLM_DECISION|action|confidence|reason) and translates it
 * into the correct key sequence for that action in the current
 * WSR layout.
 *
 * This is the bridge between LLM reasoning and WSR keyboard input.
 *
 * It also writes FSM_STATE transitions so TOM and the dashboard
 * can track what state each agent is in.
 *
 * Usage: fsm_controller.+x
 *   No args. Reads from:
 *     - pieces/apps/player_app/interact_relay.txt (LLM decisions)
 *     - pieces/display/current_layout.txt (which screen)
 *     - pieces/display/current_frame.txt (current frame text)
 *   Writes to:
 *     - pieces/keyboard/history.txt (KEY_PRESSED codes)
 *     - pieces/apps/player_app/history.txt (bare decimals)
 *     - pieces/apps/player_app/interact_relay.txt (FSM_STATE events)
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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

static int read_file(const char *path, char *buf, size_t sz) {
    FILE *f = fopen(path, "r");
    if (!f) { buf[0] = '\0'; return 0; }
    size_t n = fread(buf, 1, sz - 1, f);
    buf[n] = '\0';
    fclose(f);
    return n > 0;
}

static int read_first_line(const char *path, char *buf, size_t sz) {
    FILE *f = fopen(path, "r");
    if (!f) { buf[0] = '\0'; return 0; }
    if (!fgets(buf, (int)sz, f)) buf[0] = '\0';
    fclose(f);
    buf[strcspn(buf, "\r\n")] = '\0';
    return buf[0] != '\0';
}

/* Scan the event bus for the most recent LLM_DECISION event. */
static int read_latest_decision(const char *relay_path, char *action, size_t action_sz,
                                char *reason, size_t reason_sz, double *confidence) {
    FILE *f = fopen(relay_path, "r");
    if (!f) return 0;

    char line[MAX_LINE];
    char last[MAX_LINE] = "";
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strncmp(line, "LLM_DECISION|", 13) == 0) {
            snprintf(last, sizeof(last), "%s", line + 13);
        }
    }
    fclose(f);

    if (!last[0]) return 0;

    /* Format: action|confidence|reason */
    char *p1 = strchr(last, '|');
    if (!p1) return 0;
    *p1 = '\0';
    snprintf(action, action_sz, "%s", last);

    char *p2 = strchr(p1 + 1, '|');
    if (p2) {
        *p2 = '\0';
        *confidence = atof(p1 + 1);
        snprintf(reason, reason_sz, "%s", p2 + 1);
    } else {
        *confidence = 0.5;
        snprintf(reason, reason_sz, "%s", p1 + 1);
    }
    return 1;
}

static int is_main_menu(const char *layout) {
    const char *base = strrchr(layout, '/');
    if (!base) base = layout;
    return strstr(base, "wsr_main_menu") != NULL;
}

static int is_trade_menu(const char *layout) {
    const char *base = strrchr(layout, '/');
    if (!base) base = layout;
    return strstr(base, "wsr_trade_menu") != NULL;
}

/* Map action name to a WSR row number in the current layout. */
static int action_to_row(const char *action, const char *layout) {
    if (is_trade_menu(layout)) {
        if (strcmp(action, "buy_stock") == 0)    return 1;
        if (strcmp(action, "sell_stock") == 0)   return 2;
        if (strcmp(action, "short_stock") == 0)  return 3;
        if (strcmp(action, "cover_stock") == 0)  return 4;
        if (strcmp(action, "back_to_main") == 0) return 5;
        return 0;
    }

    /* Default: main menu rows */
    if (strcmp(action, "new_game") == 0)        return 30;
    if (strcmp(action, "end_turn") == 0)        return 17;
    if (strcmp(action, "buy_stock") == 0)       return 11;
    if (strcmp(action, "sell_stock") == 0)      return 14;
    if (strcmp(action, "buy_sell") == 0)        return 12;
    if (strcmp(action, "cycle_corp") == 0)      return 8;
    if (strcmp(action, "list_portfolio") == 0)  return 24;
    if (strcmp(action, "check_market") == 0)    return 11;
    if (strcmp(action, "navigate_to") == 0)     return 1;
    return 0;
}

/* Emit the resolved item number as a single keycode into the keyboard
 * history. wsr_menu_input.+x accepts BOTH ASCII digits (48-57 → items
 * 1-9 via subtraction) AND raw numbers >9 (→ items 10+ directly). So:
 *   row 8  → emit 56  (ASCII '8' → resolved_item 8)
 *   row 17 → emit 17  (raw → resolved_item 17)
 *   row 30 → emit 30  (raw → resolved_item 30)
 * This avoids the digit-accumulation bug where row 17 split into '1','7'
 * selected items 1 and 7 separately. No Enter key is emitted — wsr_menu_input
 * processes each key as a fully-resolved selection. */
static void emit_row(int row, const char *action) {
    char kpath[PATH_BUF], hpath[PATH_BUF], epath[PATH_BUF];
    snprintf(kpath, sizeof(kpath), "%s/pieces/keyboard/history.txt", project_root);
    snprintf(hpath, sizeof(hpath), "%s/pieces/apps/player_app/history.txt", project_root);
    snprintf(epath, sizeof(epath), "%s/pieces/apps/player_app/interact_relay.txt", project_root);

    FILE *kf = fopen(kpath, "a");
    FILE *hf = fopen(hpath, "a");
    if (!kf || !hf) {
        if (kf) fclose(kf);
        if (hf) fclose(hf);
        return;
    }

    if (row >= 1 && row <= 9) {
        int ascii_key = '0' + row;
        fprintf(hf, "%d\n", ascii_key);
        fprintf(kf, "KEY_PRESSED: %d\n", ascii_key);
    } else if (row >= 10 && row <= 9999) {
        fprintf(hf, "%d\n", row);
        fprintf(kf, "KEY_PRESSED: %d\n", row);
    }

    fclose(kf);
    fclose(hf);

    char evt[MAX_LINE];
    snprintf(evt, sizeof(evt), "KEY_INJECTED|%d|%s", row, action);
    append_line(epath, evt);
}

/* FSM state machine states */
typedef enum {
    FSM_IDLE = 0,
    FSM_AWAITING_LLM = 1,
    FSM_EXECUTING = 2,
    FSM_WAITING_RESULT = 3,
    FSM_ERROR = 4,
} fsm_state_t;

static const char *fsm_state_name(fsm_state_t s) {
    switch (s) {
        case FSM_IDLE: return "idle";
        case FSM_AWAITING_LLM: return "awaiting_llm";
        case FSM_EXECUTING: return "executing";
        case FSM_WAITING_RESULT: return "waiting_result";
        case FSM_ERROR: return "error";
    }
    return "unknown";
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    resolve_root();

    char relay_path[PATH_BUF], layout_path[PATH_BUF], frame_path[PATH_BUF];
    snprintf(relay_path, sizeof(relay_path), "%s/pieces/apps/player_app/interact_relay.txt", project_root);
    snprintf(layout_path, sizeof(layout_path), "%s/pieces/display/current_layout.txt", project_root);
    snprintf(frame_path, sizeof(frame_path), "%s/pieces/display/current_frame.txt", project_root);

    /* Main control loop — called repeatedly by run_xod_agent.sh.
     * Single-shot: each invocation reads the latest LLM decision from
     * the event bus, maps it to a row, and emits the key codes. */
    fsm_state_t state = FSM_IDLE;
    char current_action[64] = "";
    double last_confidence = 0.0;
    char last_reason[256] = "";

    /* Read the latest decision from the event bus */
    char action[64] = "", reason[256] = "";
    double conf = 0.0;
    if (read_latest_decision(relay_path, action, sizeof(action),
                             reason, sizeof(reason), &conf)) {
        snprintf(current_action, sizeof(current_action), "%s", action);
        last_confidence = conf;
        snprintf(last_reason, sizeof(last_reason), "%s", reason);
        state = FSM_EXECUTING;
    } else {
        state = FSM_AWAITING_LLM;
    }

    /* Execute: map action to row in current layout, emit key codes */
    if (state == FSM_EXECUTING) {
        char layout[MAX_LINE] = "";
        read_first_line(layout_path, layout, sizeof(layout));
        int row = action_to_row(current_action, layout);
        if (row > 0) {
            emit_row(row, current_action);
        }
    }

    /* Emit FSM state for TOM/dashboard consumers */
    {
        char layout_for_log[MAX_LINE] = "";
        read_first_line(layout_path, layout_for_log, sizeof(layout_for_log));
        char evt[MAX_LINE];
        if (state == FSM_EXECUTING && current_action[0]) {
            int row = action_to_row(current_action, layout_for_log);
            snprintf(evt, sizeof(evt), "FSM_STATE|%s|action=%s|row=%d|confidence=%.2f|reason=%s",
                     fsm_state_name(state), current_action, row, last_confidence, last_reason);
        } else {
            snprintf(evt, sizeof(evt), "FSM_STATE|%s|action=%s|confidence=%.2f",
                     fsm_state_name(state),
                     current_action[0] ? current_action : "none",
                     last_confidence);
        }
        append_line(relay_path, evt);
    }

    return 0;
}
