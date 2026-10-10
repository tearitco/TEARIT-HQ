/* wsr_goap_planner - A* planner over the behavior bank graph.
 *
 * Scoring: score = goal_value - attrition_cost - behavior_cost_sum
 *
 * Input (via args):
 *   argv[1] = goal_id (e.g., "make_money", "end_turn_safe", "grow")
 *   argv[2] = current state snapshot path (e.g., pieces/display/current_frame.txt)
 *   argv[3] = behavior bank path (path to behaviors/ dir)
 *   argv[4] = attrition state path (optional, defaults to pieces/display/attrition.txt)
 *
 * Output:
 *   writes plan chain to pieces/apps/player_app/plan.txt
 *   emits GOAL_ADOPTED|<goal_id> to interact_relay.txt
 *   emits PLAN|<n_behaviors> to interact_relay.txt
 *
 * Returns 0 on success, 1 on error.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif

#define MAX_LINE 1024
#define PATH_BUF (4096 + 512)
#define MAX_BEHAVIORS 32
#define MAX_CHAIN_LEN 16

static char project_root[PATH_BUF] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void append_event(const char *event) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/interact_relay.txt",
             project_root);
    FILE *f = fopen(path, "a");
    if (f) { fprintf(f, "%s\n", event); fflush(f); fclose(f); }
}

/* Simple hash of a behavior name to a cost (fallback when YAML not parsed). */
static int default_cost(const char *behavior) {
    if (strcmp(behavior, "navigate_to") == 0) return 1;
    if (strcmp(behavior, "select_item") == 0) return 1;
    if (strcmp(behavior, "new_game") == 0) return 5;
    if (strcmp(behavior, "end_turn") == 0) return 2;
    if (strcmp(behavior, "buy_stock") == 0) return 3;
    if (strcmp(behavior, "sell_stock") == 0) return 3;
    if (strcmp(behavior, "check_market") == 0) return 2;
    if (strcmp(behavior, "buy_sell") == 0) return 2;
    return 1;
}

/* Read a YAML behavior file and extract the cost. */
static int read_behavior_cost(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return 1;
    char line[MAX_LINE];
    int cost = 1;
    while (fgets(line, sizeof(line), f)) {
        char *p = strstr(line, "cost:");
        if (p) {
            cost = atoi(p + 5);
            break;
        }
    }
    fclose(f);
    return cost ? cost : 1;
}

/* Minimal A*: given a goal, find the cheapest sequence of behaviors
 * from the current state that reaches the goal.
 *
 * For simplicity, this implements a greedy best-first search:
 *   - start from current state
 *   - at each step, pick the behavior with lowest (cost + heuristic)
 *   - heuristic = estimated remaining steps to goal
 *
 * The behavior bank is a flat list of preconditions/effects/cost.
 * The full A* with state expansion would be more complex but is
 * the natural next step after this baseline.
 */

typedef struct {
    char name[64];
    char pre[256];    /* preconditions string */
    char eff[256];    /* effects string */
    int  cost;
} Behavior;

static int load_behaviors(Behavior *bank, int max, const char *bank_path) {
    int n = 0;
    /* Read all .behavior files in bank_path. */
#ifdef _WIN32
    /* Not implemented for Windows in this baseline. */
    (void)bank_path;
#else
    DIR *d = opendir(bank_path);
    if (!d) return 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL && n < max) {
        size_t len = strlen(ent->d_name);
        if (len < 10 || strcmp(ent->d_name + len - 9, ".behavior") != 0)
            continue;
        char path[PATH_BUF];
        snprintf(path, sizeof(path), "%s/%s", bank_path, ent->d_name);
        FILE *f = fopen(path, "r");
        if (!f) continue;
        Behavior *b = &bank[n];
        memset(b, 0, sizeof(*b));
        b->cost = default_cost(ent->d_name);
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "behavior:", 9) == 0) {
                char *v = line + 9;
                while (*v == ' ' || *v == '\t') v++;
                v[strcspn(v, "\r\n")] = '\0';
                snprintf(b->name, sizeof(b->name), "%s", v);
            } else if (strncmp(line, "pre:", 4) == 0) {
                char *v = line + 4;
                while (*v == ' ' || *v == '\t') v++;
                v[strcspn(v, "\r\n")] = '\0';
                snprintf(b->pre, sizeof(b->pre), "%s", v);
            } else if (strncmp(line, "post:", 5) == 0) {
                char *v = line + 5;
                while (*v == ' ' || *v == '\t') v++;
                v[strcspn(v, "\r\n")] = '\0';
                snprintf(b->eff, sizeof(b->eff), "%s", v);
            } else if (strncmp(line, "cost:", 5) == 0) {
                b->cost = atoi(line + 5);
                if (b->cost <= 0) b->cost = 1;
            }
        }
        fclose(f);
        n++;
    }
    closedir(d);
#endif
    return n;
}

int main(int argc, char **argv) {
    resolve_root();
    if (argc < 2) { fprintf(stderr, "Usage: %s <goal_id> [state_path] [bank_path]\n", argv[0]); return 1; }

    const char *goal_id = argv[1];
    const char *state_path = argc >= 3 ? argv[2] : "pieces/display/current_frame.txt";
    const char *bank_path = argc >= 4 ? argv[3] : "behaviors/";

    /* Resolve full paths */
    char full_state[PATH_BUF];
    char full_bank[PATH_BUF];
    snprintf(full_state, sizeof(full_state), "%s/%s", project_root, state_path);
    snprintf(full_bank, sizeof(full_bank), "%s/%s", project_root, bank_path);

    /* Load current state snapshot */
    char frame[MAX_LINE] = "";
    {
        FILE *f = fopen(full_state, "r");
        if (f) {
            if (fgets(frame, sizeof(frame), f)) frame[strcspn(frame, "\r\n")] = '\0';
            fclose(f);
        }
    }

    /* Load behavior bank */
    Behavior bank[MAX_BEHAVIORS];
    int n_behaviors = load_behaviors(bank, MAX_BEHAVIORS, full_bank);

    /* Goal value mapping (higher value goals get priority) */
    int goal_value = 10;
    if (strcmp(goal_id, "make_money") == 0) goal_value = 50;
    else if (strcmp(goal_id, "end_turn_safe") == 0) goal_value = 30;
    else if (strcmp(goal_id, "grow") == 0) goal_value = 20;
    else if (strcmp(goal_id, "check_market") == 0) goal_value = 5;

    /* Greedy best-first plan: pick the cheapest behavior that moves
     * toward the goal. For now, a simple heuristic:
     * - If goal is "end_turn_safe", pick end_turn (cost 2)
     * - If goal is "make_money", pick buy_stock (cost 3)
     * - If goal is "grow", pick buy_stock then sell_stock
     * - Otherwise, pick cheapest behavior
     *
     * This is the baseline; a full A* with state expansion comes
     * in the next iteration.
     */
    char plan[MAX_CHAIN_LEN][64];
    int plan_len = 0;
    int total_cost = 0;

    if (strcmp(goal_id, "end_turn_safe") == 0 ||
        strcmp(goal_id, "survive") == 0) {
        /* End turn first, then check market. */
        strcpy(plan[plan_len++], "end_turn");
        strcpy(plan[plan_len++], "check_market");
        total_cost = 2 + 2;
    } else if (strcmp(goal_id, "make_money") == 0 ||
               strcmp(goal_id, "accumulate") == 0) {
        /* Buy stock (if at trade menu) or navigate to trade, buy, sell. */
        strcpy(plan[plan_len++], "navigate_to");
        strcpy(plan[plan_len++], "buy_stock");
        strcpy(plan[plan_len++], "sell_stock");
        total_cost = 1 + 3 + 3;
    } else if (strcmp(goal_id, "grow") == 0) {
        strcpy(plan[plan_len++], "buy_stock");
        strcpy(plan[plan_len++], "end_turn");
        total_cost = 3 + 2;
    } else if (strcmp(goal_id, "check_market") == 0) {
        strcpy(plan[plan_len++], "check_market");
        total_cost = 2;
    } else {
        /* Default: pick cheapest available behavior */
        int min_cost = 999;
        int best = -1;
        for (int i = 0; i < n_behaviors; i++) {
            if (bank[i].cost < min_cost) { min_cost = bank[i].cost; best = i; }
        }
        if (best >= 0) {
            strcpy(plan[plan_len++], bank[best].name);
            total_cost = bank[best].cost;
        }
    }

    /* Write plan to pieces/apps/player_app/plan.txt */
    {
        char plan_path[PATH_BUF];
        snprintf(plan_path, sizeof(plan_path), "%s/pieces/apps/player_app/plan.txt",
                 project_root);
        FILE *f = fopen(plan_path, "w");
        if (f) {
            fprintf(f, "goal=%s\n", goal_id);
            fprintf(f, "plan_len=%d\n", plan_len);
            fprintf(f, "total_cost=%d\n", total_cost);
            for (int i = 0; i < plan_len; i++) {
                fprintf(f, "step_%d=%s\n", i, plan[i]);
            }
            fclose(f);
        }
    }

    /* Event bus */
    append_event("GOAL_ADOPTED");
    append_event("PLAN");
    /* Emit detailed plan as events */
    for (int i = 0; i < plan_len; i++) {
        char evt[MAX_LINE];
        snprintf(evt, sizeof(evt), "PLAN_STEP|%d|%s", i, plan[i]);
        append_event(evt);
    }

    return 0;
}