/* pop_update - applies ONE tick of population growth/decline, the
 * "not in original WSR" metric named directly (WSR has zero population
 * simulation, per SOCIETY-ECONOMY-ARCHITECTURE.txt §0.5). Reuses
 * EVO-DESIGN.txt §2's own ecosystem-tier population-dynamics idea
 * directly - same mechanism, applied to a city's citizens instead of
 * an ecosystem's species.
 *
 * decision_mode=1 (weighted) is REAL, not a stub: effective_growth =
 * (birth_rate - death_rate) x food_adequacy_factor, where
 * food_adequacy = food_supply/food_demand (capped) - a food shortage
 * measurably slows or reverses growth, matching how real population
 * dynamics actually work, not an arbitrary formula. decision_mode=0
 * (preset) ignores food entirely - fixed birth_rate-death_rate every
 * tick, the simpler/classic baseline.
 *
 * NOTE (per direct instruction "add a note to broaden new features
 * later"): weather is NOT yet wired into this formula (a real future
 * extension - extreme weather should plausibly raise death_rate
 * temporarily) - named here, not built, so it isn't lost.
 *
 * Self-contained, no shared headers.
 * Usage: pop_update.+x <piece_id> */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_LINE 512
#define MAX_FIELD 256

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void read_state_field(const char *state_path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(state_path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t key_len = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            char *v = line + key_len + 1;
            v[strcspn(v, "\n")] = '\0';
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(out, out_sz, "%s", v);
#pragma GCC diagnostic pop
            break;
        }
    }
    fclose(f);
}

static void write_state_field(const char *state_path, const char *key, const char *value) {
    FILE *f = fopen(state_path, "r");
    char lines[64][MAX_LINE];
    int nlines = 0;
    if (f) {
        while (nlines < 64 && fgets(lines[nlines], MAX_LINE, f)) nlines++;
        fclose(f);
    }
    size_t key_len = strlen(key);
    f = fopen(state_path, "w");
    if (!f) return;
    int found = 0;
    for (int i = 0; i < nlines; i++) {
        if (strncmp(lines[i], key, key_len) == 0 && lines[i][key_len] == '=') {
            fprintf(f, "%s=%s\n", key, value);
            found = 1;
        } else {
            fputs(lines[i], f);
        }
    }
    if (!found) fprintf(f, "%s=%s\n", key, value);
    fclose(f);
}

/* per-capita daily food need - a named constant so it's easy to find/
 * tune later, matching this whole family's own convention. */
static const float FOOD_NEED_PER_CAPITA = 0.001f;

int main(int argc, char *argv[]) {
    resolve_root();
    if (argc < 2) return 1;
    char state_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/projects/wsr-pal/pieces/%s/state.txt", project_root, argv[1]);

    char mode_str[MAX_FIELD], pop_str[MAX_FIELD], birth_str[MAX_FIELD], death_str[MAX_FIELD];
    char food_supply_str[MAX_FIELD];
    read_state_field(state_path, "decision_mode", mode_str, sizeof(mode_str));
    read_state_field(state_path, "total_population", pop_str, sizeof(pop_str));
    read_state_field(state_path, "birth_rate", birth_str, sizeof(birth_str));
    read_state_field(state_path, "death_rate", death_str, sizeof(death_str));
    read_state_field(state_path, "food_supply", food_supply_str, sizeof(food_supply_str));

    int mode = mode_str[0] ? atoi(mode_str) : 0;
    float population = pop_str[0] ? atof(pop_str) : 0;
    float birth_rate = birth_str[0] ? atof(birth_str) : 0;
    float death_rate = death_str[0] ? atof(death_str) : 0;
    float food_supply = food_supply_str[0] ? atof(food_supply_str) : 0;

    float food_demand = population * FOOD_NEED_PER_CAPITA;
    float effective_growth;
    const char *mode_name;

    if (mode == 0) {
        effective_growth = birth_rate - death_rate;
        mode_name = "preset";
    } else {
        /* weighted (and rl-stub/llm-fallback, matching the same
         * "falls back to weighted's real logic" pattern already used
         * in corp_decide.c/gov_decide.c) - real food-adequacy factor.
         * CORRECTED during live testing: the first version scaled net
         * (birth-death) by food_adequacy, which only suppressed growth
         * toward zero under a shortage - starvation should cause real
         * DECLINE, not just stagnation. Fixed: births scale down with
         * adequacy (capped at the base rate - a surplus doesn't
         * runaway-boost births), deaths scale UP as adequacy drops
         * below 1, so a full famine (food_adequacy=0) produces a real
         * negative growth rate, not a flat 0%. */
        float food_adequacy = (food_demand > 0) ? food_supply / food_demand : 1.0f;
        float effective_births = birth_rate * (food_adequacy < 1.0f ? food_adequacy : 1.0f);
        float shortage = 1.0f - food_adequacy;
        if (shortage < 0) shortage = 0;
        float effective_deaths = death_rate + shortage * death_rate * 2.0f;
        effective_growth = effective_births - effective_deaths;
        mode_name = "weighted(real)";
    }

    float new_population = population * (1.0f + effective_growth / 100.0f);
    if (new_population < 0) new_population = 0;

    char new_pop_str[MAX_FIELD], new_food_demand_str[MAX_FIELD];
    snprintf(new_pop_str, sizeof(new_pop_str), "%.1f", new_population);
    snprintf(new_food_demand_str, sizeof(new_food_demand_str), "%.2f", food_demand);
    write_state_field(state_path, "total_population", new_pop_str);
    write_state_field(state_path, "food_demand", new_food_demand_str);
    write_state_field(state_path, "last_action", effective_growth >= 0 ? "grew" : "declined");
    write_state_field(state_path, "current_state", "0");
    printf("[%s] population %.1f -> %.1f (growth=%.3f%%, food_demand=%.2f, food_supply=%.2f)\n",
           mode_name, population, new_population, effective_growth, food_demand, food_supply);
    return 0;
}
