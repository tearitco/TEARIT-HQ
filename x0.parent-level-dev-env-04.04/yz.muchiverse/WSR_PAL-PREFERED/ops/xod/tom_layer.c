/* tom_layer - Theory of Mind layer for XOD multi-agent driving.
 *
 * Tracks what other agents know, believe, and intend.
 * Reads the shared event bus (interact_relay.txt) and infers
 * other agents' mental states from their actions.
 *
 * This is the TOM half of the stack: it lets each agent
 * predict what other agents will do, avoid conflicts, and
 * cooperate or compete strategically.
 *
 * Outputs TOM_* events to the event bus so the LLM brain
 * can incorporate other-agent awareness into its decisions.
 *
 * Usage: tom_layer.+x
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>
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

/* tom_agent_t represents one known agent's inferred mental state. */
typedef struct {
    char agent_id[64];
    char last_action[64];
    char believed_goal[128];
    char believed_layout[128];
    int  last_row;
    double confidence;
    time_t last_seen;
} tom_agent_t;

#define MAX_AGENTS 16
static tom_agent_t agents[MAX_AGENTS];
static int agent_count = 0;

/* Find or create an agent entry by agent_id prefix in an event line. */
static tom_agent_t *find_or_create_agent(const char *event_line) {
    /* Events are formatted as:
     *   KEY_INJECTED|17|end_turn
     *   BEHAVIOR_COMPLETE|end_turn|ok
     *   FSM_STATE|executing|action=end_turn|confidence=0.95
     *   LLM_DECISION|end_turn|0.95|reason text
     *
     * We infer agent identity from the event bus file path / session dir.
     * For now, use the event type + action hash as a proxy for agent identity.
     * A real deployment would tag events with agent_id in the relay filename.
     */
    const char *action = NULL;
    const char *etype = event_line;

    if (strncmp(event_line, "KEY_INJECTED|", 13) == 0) {
        etype = "KEY_INJECTED";
        action = event_line + 13;
        char *p = strchr(action, '|');
        if (p) *p = '\0';
    } else if (strncmp(event_line, "BEHAVIOR_", 9) == 0) {
        etype = event_line;
        char *p = strchr(etype, '|');
        if (p) *p = '\0';
        action = p ? p + 1 : "";
    } else if (strncmp(event_line, "FSM_STATE|", 10) == 0) {
        etype = "FSM_STATE";
        action = event_line + 10;
    } else if (strncmp(event_line, "LLM_DECISION|", 13) == 0) {
        etype = "LLM_DECISION";
        action = event_line + 13;
        char *p = strchr(action, '|');
        if (p) *p = '\0';
    }

    /* Create a synthetic agent id from action + event type. */
    char aid[128];
    snprintf(aid, sizeof(aid), "%s_%s", etype, action ? action : "unknown");

    for (int i = 0; i < agent_count; i++) {
        if (strcmp(agents[i].agent_id, aid) == 0)
            return &agents[i];
    }

    if (agent_count >= MAX_AGENTS) return NULL;

    tom_agent_t *a = &agents[agent_count++];
    snprintf(a->agent_id, sizeof(a->agent_id), "%s", aid);
    a->last_action[0] = '\0';
    a->believed_goal[0] = '\0';
    a->believed_layout[0] = '\0';
    a->last_row = 0;
    a->confidence = 0.0;
    a->last_seen = time(NULL);
    return a;
}

/* Update an agent's belief state from a new event. */
static void update_agent_from_event(tom_agent_t *a, const char *event_line) {
    time_t now = time(NULL);
    a->last_seen = now;

    if (strncmp(event_line, "KEY_INJECTED|", 13) == 0) {
        const char *p = event_line + 13;
        a->last_row = atoi(p);
        const char *action = strchr(p, '|');
        if (action) {
            snprintf(a->last_action, sizeof(a->last_action), "%s", action + 1);
        }
        a->confidence = 0.9;
    } else if (strncmp(event_line, "LLM_DECISION|", 13) == 0) {
        const char *p = event_line + 13;
        char action[64];
        action[0] = '\0';
        sscanf(p, "%63[^|]", action);
        if (action[0])
            snprintf(a->last_action, sizeof(a->last_action), "%s", action);
        a->confidence = 0.85;
    } else if (strncmp(event_line, "BEHAVIOR_START|", 15) == 0) {
        const char *p = event_line + 15;
        snprintf(a->last_action, sizeof(a->last_action), "%s", p);
        a->confidence = 0.8;
    } else if (strncmp(event_line, "FSM_STATE|", 10) == 0) {
        /* Parse FSM state to update beliefs */
        const char *p = event_line + 10;
        char state[64] = "";
        sscanf(p, "%63[^|]", state);
        if (strstr(p, "action=")) {
            const char *ap = strstr(p, "action=");
            ap += 7;
            char action[64];
            action[0] = '\0';
            sscanf(ap, "%63[^|]", action);
            if (action[0])
                snprintf(a->last_action, sizeof(a->last_action), "%s", action);
        }
        if (strstr(p, "confidence=")) {
            const char *cp = strstr(p, "confidence=");
            cp += 11;
            a->confidence = atof(cp);
        }
    }

    /* Infer goal from action */
    if (strcmp(a->last_action, "end_turn") == 0)
        snprintf(a->believed_goal, sizeof(a->believed_goal), "advance_turn");
    else if (strcmp(a->last_action, "buy_stock") == 0)
        snprintf(a->believed_goal, sizeof(a->believed_goal), "accumulate");
    else if (strcmp(a->last_action, "sell_stock") == 0)
        snprintf(a->believed_goal, sizeof(a->believed_goal), "liquidate");
    else if (strcmp(a->last_action, "new_game") == 0)
        snprintf(a->believed_goal, sizeof(a->believed_goal), "restart");
    else if (strcmp(a->last_action, "check_market") == 0)
        snprintf(a->believed_goal, sizeof(a->believed_goal), "gather_info");
    else if (strcmp(a->last_action, "cycle_corp") == 0)
        snprintf(a->believed_goal, sizeof(a->believed_goal), "switch_entity");
    else
        snprintf(a->believed_goal, sizeof(a->believed_goal), "unknown");
}

/* Prune stale agents (not seen in > 5 minutes). */
static void prune_stale_agents(void) {
    time_t now = time(NULL);
    int write = 0;
    for (int read = 0; read < agent_count; read++) {
        if (now - agents[read].last_seen < 300) {
            if (write != read)
                agents[write] = agents[read];
            write++;
        }
    }
    agent_count = write;
}

/* Emit a TOM summary event for the LLM brain to consume. */
static void emit_tom_summary(const char *relay_path) {
    char evt[MAX_LINE];
    snprintf(evt, sizeof(evt), "TOM_SUMMARY|agents=%d", agent_count);
    append_line(relay_path, evt);

    for (int i = 0; i < agent_count; i++) {
        char belief[MAX_LINE];
        snprintf(belief, sizeof(belief),
                 "TOM_BELIEF|%s|action=%s|goal=%s|confidence=%.2f|age=%ld",
                 agents[i].agent_id,
                 agents[i].last_action[0] ? agents[i].last_action : "unknown",
                 agents[i].believed_goal[0] ? agents[i].believed_goal : "unknown",
                 agents[i].confidence,
                 (long)(time(NULL) - agents[i].last_seen));
        append_line(relay_path, belief);
    }
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    resolve_root();

    char relay_path[PATH_BUF];
    snprintf(relay_path, sizeof(relay_path), "%s/pieces/apps/player_app/interact_relay.txt", project_root);

    /* Read the entire relay and process every event. */
    FILE *f = fopen(relay_path, "r");
    if (!f) return 0;

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (!line[0]) continue;

        tom_agent_t *a = find_or_create_agent(line);
        if (a) update_agent_from_event(a, line);
    }
    fclose(f);

    prune_stale_agents();
    emit_tom_summary(relay_path);

    return 0;
}
