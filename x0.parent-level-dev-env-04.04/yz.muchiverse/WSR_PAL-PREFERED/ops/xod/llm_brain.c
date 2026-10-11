/* llm_brain — LLM Brain for XOD (C op, house pattern).
 *
 * Observes WSR state, builds prompts, calls model_api (Ollama),
 * reads response via json_parser, emits LLM_DECISION to event bus.
 *
 * House pattern: state → prompt → model_api chat → json_parser → decision.
 *
 * Usage: llm_brain.+x <session_dir> <goal> [model] [tom_file]
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#define access _access
#define F_OK 0
#define POPEN _popen
#define PCLOSE _pclose
#else
#include <unistd.h>
#define POPEN popen
#define PCLOSE pclose
#endif

#include "model_api.h"

#define MAX_LINE 8192
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

static void json_escape(char *out, size_t out_sz, const char *in) {
    char *p = out;
    const char *end = out + out_sz - 1;
    for (; *in && p < end; in++) {
        switch (*in) {
            case '"': *p++ = '\\'; *p++ = '"'; break;
            case '\\': *p++ = '\\'; *p++ = '\\'; break;
            case '\n': *p++ = '\\'; *p++ = 'n'; break;
            case '\r': *p++ = '\\'; *p++ = 'r'; break;
            case '\t': *p++ = '\\'; *p++ = 't'; break;
            default: *p++ = *in; break;
        }
    }
    *p = '\0';
}

/* Build system + user prompts from current WSR state. */
static void build_prompts(const char *session_dir, const char *goal,
                          char *system_prompt, size_t sp_sz,
                          char *user_prompt, size_t up_sz) {
    char frame[4096] = "", layout[256] = "";
    char corp_state[2048] = "", fitness[1024] = "", events[2048] = "";
    char tom_context[2048] = "{}";

    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/display/current_frame.txt", session_dir);
    read_file(path, frame, sizeof(frame));
    snprintf(path, sizeof(path), "%s/pieces/display/current_layout.txt", session_dir);
    read_first_line(path, layout, sizeof(layout));
    snprintf(path, sizeof(path), "%s/projects/wsr-pal/pieces/corp_ORB/state.txt", session_dir);
    read_file(path, corp_state, sizeof(corp_state));
    snprintf(path, sizeof(path), "%s/pieces/display/fitness.txt", session_dir);
    read_file(path, fitness, sizeof(fitness));
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/interact_relay.txt", session_dir);
    read_file(path, events, sizeof(events));
    if (strlen(events) > 500) {
        memmove(events, events + strlen(events) - 500, 500);
        events[500] = '\0';
    }

    if (access("/dev/stdin", F_OK) == 0) {
        char tom_path[PATH_BUF];
        snprintf(tom_path, sizeof(tom_path), "%s/pieces/apps/player_app/tom_context.json", session_dir);
        if (access(tom_path, F_OK) == 0)
            read_file(tom_path, tom_context, sizeof(tom_context));
    }

    char esc_frame[8192], esc_corp[4096], esc_fit[2048], esc_evt[4096], esc_tom[4096];
    json_escape(esc_frame, sizeof(esc_frame), frame);
    json_escape(esc_corp, sizeof(esc_corp), corp_state);
    json_escape(esc_fit, sizeof(esc_fit), fitness);
    json_escape(esc_evt, sizeof(esc_evt), events);
    json_escape(esc_tom, sizeof(esc_tom), tom_context);

    snprintf(system_prompt, sp_sz,
        "You are the decision-making brain of an autonomous agent driving a "
        "stock-market simulation (WSR: Wall Street Raider).\n\n"
        "GOAL: %s\n\n"
        "You observe the current screen frame, active layout, corp state, "
        "fitness score, and recent events on the event bus.\n\n"
        "VALID ACTIONS: end_turn, buy_stock, sell_stock, buy_sell, new_game, "
        "cycle_corp, list_portfolio, check_market, back_to_main, wait\n\n"
        "GOAL STRATEGY:\n"
        "- \"survive\": Keep cash reserves > 20%%. Prefer end_turn when cash is low.\n"
        "- \"accumulate\": Buy aggressively when price < book_value.\n"
        "- \"grow\": Cycle corps, explore markets, diversify.\n\n"
        "IMPORTANT: Choose an action from the VALID ACTIONS list that "
        "matches your goal strategy, considering the CURRENT FRAME "
        "and recent events. Do not always default to buy_stock.\n\n"
        "OUTPUT FORMAT (strict JSON, no extra text):\n"
        "{\"action\":\"<action>\",\"reason\":\"<reason>\","
        "\"confidence\":<float>}\n\n"
        "Only output valid JSON. The confidence field is required.", goal);

    snprintf(user_prompt, up_sz,
        "GOAL: %s\n\n"
        "CURRENT LAYOUT: %s\n\n"
        "CURRENT FRAME:\n%s\n\n"
        "CORP STATE:\n%s\n\n"
        "FITNESS:\n%s\n\n"
        "RECENT EVENTS:\n%s\n\n"
        "TOM CONTEXT:\n%s\n\n"
        "Decide the single best action now. Output only valid JSON.",
        goal, layout, esc_frame, esc_corp, esc_fit, esc_evt, esc_tom);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <session_dir> <goal> [model] [tom_file]\n", argv[0]);
        return 1;
    }
    resolve_root();

    const char *session_dir = argv[1];
    const char *goal = argv[2];
    const char *model_name = argc >= 4 ? argv[3] : "gemma3:1b";

    /* Use model_api for provider-agnostic LLM access */
    const char *ollama_url = "http://10.0.0.144:11434";
    const char *api_key = getenv("OLLAMA_API_KEY");  /* usually unset for local */

    model_api_t *api = model_api_init(MODEL_API_OLLAMA, ollama_url, model_name, api_key);
    if (!api) {
        fprintf(stderr, "Failed to init model_api\n");
        return 1;
    }

    char system_prompt[4096];
    char user_prompt[8192];
    build_prompts(session_dir, goal, system_prompt, sizeof(system_prompt),
                  user_prompt, sizeof(user_prompt));

    model_response_t *resp = model_api_chat(api, system_prompt, user_prompt, 0.3, 2048);
    if (!resp || resp->error_code) {
        fprintf(stderr, "LLM call failed: %s\n", resp ? resp->error_msg : "no response");
        if (resp) model_response_free(resp);
        model_api_free(api);
        return 1;
    }

    char action[64] = "wait";
    char reason[256] = "";
    double conf = 0.5;

    /* model_extract_action uses json_parser.+x internally */
    model_extract_action(resp->content, action, sizeof(action), reason, sizeof(reason), &conf);

    /*
     * Fallback policy: small local models (gemma3:1b) tend to default
     * to buy_stock regardless of goal. Apply a lightweight rule-based
     * override when the LLM action conflicts with the stated goal or
     * when it's repeating the same action.
     */
    static const char *last_goal = NULL;
    static const char *last_action = NULL;

    int override = 0;
    char override_action[64] = "";
    char override_reason[256] = "";

    if (strcmp(goal, "survive") == 0) {
        /* Survive: prefer end_turn/wait/check_market over trading actions */
        if (strcmp(action, "end_turn") != 0 &&
            strcmp(action, "wait") != 0 &&
            strcmp(action, "check_market") != 0) {
            override = 1;
        }
    } else if (strcmp(goal, "accumulate") == 0) {
        /* Accumulate: prefer buy_stock/sell_stock over passive actions */
        if (strcmp(action, "buy_stock") != 0 &&
            strcmp(action, "sell_stock") != 0) {
            override = 1;
        }
    } else if (strcmp(goal, "grow") == 0) {
        /* Grow: prefer cycle_corp/new_game over buy_stock/end_turn */
        if (strcmp(action, "buy_stock") == 0 ||
            strcmp(action, "end_turn") == 0) {
            override = 1;
        }
    }

    /* If the model is stuck in a loop, diversify */
    if (last_goal && strcmp(last_goal, goal) == 0 &&
        last_action && strcmp(last_action, action) == 0) {
        override = 1;
    }

    if (override) {
        if (strcmp(goal, "survive") == 0) {
            snprintf(override_action, sizeof(override_action), "end_turn");
            snprintf(override_reason, sizeof(override_reason),
                "survive: preserving cash for market timing");
        } else if (strcmp(goal, "accumulate") == 0) {
            snprintf(override_action, sizeof(override_action), "buy_stock");
            snprintf(override_reason, sizeof(override_reason),
                "accumulate: buying at current opportunity");
        } else if (strcmp(goal, "grow") == 0) {
            snprintf(override_action, sizeof(override_action), "cycle_corp");
            snprintf(override_reason, sizeof(override_reason),
                "grow: exploring alternative corporations");
        } else {
            snprintf(override_action, sizeof(override_action), "list_portfolio");
            snprintf(override_reason, sizeof(override_reason),
                "gathering more information before acting");
        }
        snprintf(action, sizeof(action), "%s", override_action);
        snprintf(reason, sizeof(reason), "%s", override_reason);
        conf = 0.6;
    }

    last_goal = goal;
    last_action = action;

    model_response_free(resp);
    model_api_free(api);

    /* Emit LLM_DECISION to event bus */
    char event_path[PATH_BUF];
    snprintf(event_path, sizeof(event_path), "%s/pieces/apps/player_app/interact_relay.txt", project_root);
    char event[MAX_LINE];
    snprintf(event, sizeof(event), "LLM_DECISION|%s|%.2f|%s", action, conf, reason);
    append_line(event_path, event);

    printf("{\"action\":\"%s\",\"reason\":\"%s\",\"confidence\":%.2f}\n", action, reason, conf);
    return 0;
}
