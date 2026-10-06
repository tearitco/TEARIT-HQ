/* wsr_fitness - Real fitness scoring for XOD.
 *
 * Reads the actual corp_ORB state file and computes a fitness score
 * based on real portfolio metrics.
 *
 * Input:
 *   argv[1] = corp state path (default: projects/wsr-pal/pieces/corp_ORB/state.txt)
 *   argv[2] = price history path (default: projects/wsr-pal/pieces/corp_ORB/price_history.txt)
 *
 * Output:
 *   writes fitness.txt to pieces/display/fitness.txt
 *   emits FITNESS|score|components to interact_relay.txt
 *
 * Fitness formula:
 *   fitness = portfolio_value + cash + (shares_held * stock_price) + market_cap
 *   normalized against initial values
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
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

static int read_kv_int(const char *path, const char *key, int def) {
    return (int)read_kv_double(path, key, (double)def);
}

int main(int argc, char **argv) {
    resolve_root();

    const char *state_path = argc >= 2 ? argv[1] :
        "projects/wsr-pal/pieces/corp_ORB/state.txt";
    const char *price_path = argc >= 3 ? argv[2] :
        "projects/wsr-pal/pieces/corp_ORB/price_history.txt";

    char full_state[PATH_BUF];
    char full_price[PATH_BUF];
    snprintf(full_state, sizeof(full_state), "%s/%s", project_root, state_path);
    snprintf(full_price, sizeof(full_price), "%s/%s", project_root, price_path);

    /* Read corp state */
    double cash = read_kv_double(full_state, "cash", 0.0);
    double stock_price = read_kv_double(full_state, "stock_price", 0.0);
    double book_value = read_kv_double(full_state, "book_value", 0.0);
    double shares_outstanding = read_kv_double(full_state, "shares_outstanding", 0.0);
    double market_cap = read_kv_double(full_state, "market_cap", 0.0);
    double debt_to_equity = read_kv_double(full_state, "debt_to_equity", 0.0);
    int risk_bias = read_kv_int(full_state, "risk_bias", 5);
    int shares_held = read_kv_int(full_state, "shares_held", 0);
    int current_state = read_kv_int(full_state, "current_state", 0);
    int decision_mode = read_kv_int(full_state, "decision_mode", 1);

    /* Read price history for trend */
    double initial_price = stock_price;
    double price_change_pct = 0.0;
    {
        FILE *f = fopen(full_price, "r");
        if (f) {
            char line[MAX_LINE];
            if (fgets(line, sizeof(line), f)) {
                /* Format: turn_index,price_at_turn_start,price_after,price_change_pct */
                char *p4 = strrchr(line, ',');
                if (p4) price_change_pct = atof(p4 + 1);
                char *p3 = p4 ? strrchr(line, ',') : NULL;
                (void)p3;
            }
            fclose(f);
        }
    }

    /* Fitness components */
    double portfolio_value = cash + (shares_held * stock_price);
    double corp_health = book_value > 0 ? market_cap / book_value : 0.0;
    double risk_penalty = debt_to_equity > 0.5 ? debt_to_equity * 10.0 : 0.0;
    double trend_bonus = price_change_pct > 0 ? price_change_pct * 2.0 : 0.0;

    /* Base fitness */
    double fitness = portfolio_value + corp_health * 100.0 + trend_bonus - risk_penalty;

    /* Normalize to 0-1000 range */
    if (fitness < 0) fitness = 0;
    if (fitness > 10000) fitness = 10000;
    double normalized = fitness / 10.0;

    /* Write fitness file */
    char fitness_path[PATH_BUF];
    snprintf(fitness_path, sizeof(fitness_path), "%s/pieces/display/fitness.txt",
             project_root);
    FILE *ff = fopen(fitness_path, "w");
    if (ff) {
        fprintf(ff, "fitness=%.2f\n", normalized);
        fprintf(ff, "portfolio_value=%.2f\n", portfolio_value);
        fprintf(ff, "cash=%.2f\n", cash);
        fprintf(ff, "stock_price=%.2f\n", stock_price);
        fprintf(ff, "shares_held=%d\n", shares_held);
        fprintf(ff, "market_cap=%.2f\n", market_cap);
        fprintf(ff, "book_value=%.2f\n", book_value);
        fprintf(ff, "corp_health=%.2f\n", corp_health);
        fprintf(ff, "debt_to_equity=%.2f\n", debt_to_equity);
        fprintf(ff, "risk_bias=%d\n", risk_bias);
        fprintf(ff, "price_change_pct=%.2f\n", price_change_pct);
        fprintf(ff, "decision_mode=%d\n", decision_mode);
        fprintf(ff, "current_state=%d\n", current_state);
        fclose(ff);
    }

    /* Event bus */
    append_event("FITNESS");
    char evt[MAX_LINE];
    snprintf(evt, sizeof(evt),
             "FITNESS|%.2f|portfolio=%.2f,cash=%.2f,price=%.2f,held=%d,health=%.2f,de=%.2f",
             normalized, portfolio_value, cash, stock_price, shares_held,
             corp_health, debt_to_equity);
    append_event(evt);

    return 0;
}