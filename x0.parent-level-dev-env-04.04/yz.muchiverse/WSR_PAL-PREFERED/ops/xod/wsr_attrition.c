/* wsr_attrition - Attrition Model for XOD.
 *
 * Tracks resource degradation over time.
 * Resources: cash, stock_value, time, attention, morale.
 *
 * Input (env/args):
 *   argv[1] = current frame path (optional)
 *   argv[2] = attrition state path (optional, defaults to pieces/display/attrition.txt)
 *
 * Reads current attrition state from pieces/display/attrition.txt
 * (or initializes fresh).
 *
 * Updates:
 *   time += 1
 *   attention -= complexity_per_turn
 *   morale += successes - failures
 *   cash -= opportunity_cost_per_turn
 *
 * Emits ATTRITION_TICK to interact_relay.txt.
 * Writes updated state back to pieces/display/attrition.txt.
 *
 * Usage: wsr_attrition.+x [frame_path] [state_path]
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#define MAX_LINE 1024
#define PATH_BUF (4096 + 512)

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

/* Read a KV from the attrition state file. */
static double read_kv_double(const char *path, const char *key, double def) {
    FILE *f = fopen(path, "r");
    if (!f) return def;
    char line[MAX_LINE];
    double val = def;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(line, key) == 0) {
            val = atof(eq + 1);
            break;
        }
    }
    fclose(f);
    return val;
}

/* Write a KV to the attrition state file. */
static void write_kv_double(const char *path, const char *key, double val) {
    FILE *f = fopen(path, "a");
    if (f) { fprintf(f, "%s=%.4f\n", key, val); fclose(f); }
}

int main(int argc, char **argv) {
    resolve_root();

    const char *state_path = "pieces/display/attrition.txt";
    if (argc >= 3) state_path = argv[2];

    char full_state[PATH_BUF];
    snprintf(full_state, sizeof(full_state), "%s/%s", project_root, state_path);

    /* Read current state */
    double cash = read_kv_double(full_state, "cash", 0.0);
    double stock_value = read_kv_double(full_state, "stock_value", 0.0);
    double time_turn = read_kv_double(full_state, "time_turn", 0.0);
    double attention = read_kv_double(full_state, "attention", 100.0);
    double morale = read_kv_double(full_state, "morale", 50.0);

    /* Degradation rates per turn */
    double cash_deg = 0.5;        /* opportunity cost /turn */
    double stock_deg = 1.0;       /* market volatility /turn */
    double time_deg = 1.0;        /* -1 turn per action */
    double attn_deg = 2.0;        /* complexity per behavior */
    double morale_gain = 5.0;     /* base morale recovery */

    /* Apply degradation */
    cash -= cash_deg;
    if (cash < 0) cash = 0;
    stock_value -= stock_deg;
    if (stock_value < 0) stock_value = 0;
    time_turn += time_deg;
    attention -= attn_deg;
    if (attention < 0) attention = 0;
    morale += morale_gain;
    if (morale > 100) morale = 100;

    /* Compute attrition cost */
    double attrition_cost = (cash_deg + stock_deg + time_deg + attn_deg) * 1.0;

    /* Write updated state */
    FILE *f = fopen(full_state, "w");
    if (f) {
        fprintf(f, "cash=%.4f\n", cash);
        fprintf(f, "stock_value=%.4f\n", stock_value);
        fprintf(f, "time_turn=%.4f\n", time_turn);
        fprintf(f, "attention=%.4f\n", attention);
        fprintf(f, "morale=%.4f\n", morale);
        fprintf(f, "attrition_cost=%.4f\n", attrition_cost);
        fclose(f);
    }

    /* Emit event bus */
    char evt[MAX_LINE];
    snprintf(evt, sizeof(evt), "ATTRITION_TICK|cash=%.2f,stock=%.2f,time=%.1f,attn=%.1f,morale=%.1f,cost=%.2f",
             cash, stock_value, time_turn, attention, morale, attrition_cost);
    append_event(evt);

    return 0;
}