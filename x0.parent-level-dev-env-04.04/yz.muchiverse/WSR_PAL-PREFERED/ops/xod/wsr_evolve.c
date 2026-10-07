/* wsr_evolve - Behavior Evolution for XOD.
 *
 * Given a set of behavior chains and their fitness scores, selects
 * the highest-scoring chains and mutates them to create the next
 * generation.
 *
 * Input:
 *   argv[1] = fitness data path (pieces/display/fitness.txt)
 *   argv[2] = behavior bank path (behaviors/)
 *   argv[3] = output path (pieces/display/population.txt)
 *
 * Output:
 *   writes evolved population to the output path
 *   emits EVOLVE|<generation>|<best_score> to interact_relay.txt
 *
 * Selection strategy: keep top 50%, mutate 20%, crossover 30%.
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

#define MAX_LINE 1024
#define PATH_BUF (4096 + 512)
#define MAX_POPULATION 64
#define MAX_CHAIN 32

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

typedef struct {
    char chain[MAX_CHAIN][64];
    int  chain_len;
    double fitness;
} Individual;

static void mutate(Individual *ind) {
    if (ind->chain_len == 0) return;
    /* Pick a random step and replace it with a random behavior. */
    int idx = rand() % ind->chain_len;
    const char *behaviors[] = {
        "end_turn", "buy_stock", "sell_stock", "check_market",
        "new_game", "buy_sell", "back_to_main"
    };
    int n = sizeof(behaviors) / sizeof(behaviors[0]);
    int pick = rand() % n;
    snprintf(ind->chain[idx], 64, "%s", behaviors[pick]);
}

static void crossover(Individual *a, Individual *b) {
    if (a->chain_len < 2 || b->chain_len < 2) return;
    int mid_a = rand() % a->chain_len;
    int mid_b = rand() % b->chain_len;
    char tmp[64];
    snprintf(tmp, sizeof(tmp), "%s", a->chain[mid_a]);
    snprintf(a->chain[mid_a], 64, "%s", b->chain[mid_b]);
    snprintf(b->chain[mid_b], 64, "%s", tmp);
}

static int cmp_fitness_desc(const void *p1, const void *p2) {
    const Individual *a = (const Individual *)p1;
    const Individual *b = (const Individual *)p2;
    if (b->fitness > a->fitness) return 1;
    if (b->fitness < a->fitness) return -1;
    return 0;
}

int main(int argc, char **argv) {
    resolve_root();

    const char *fitness_path = argc >= 2 ? argv[1] : "pieces/display/fitness.txt";
    const char *output_path   = argc >= 3 ? argv[2] : "pieces/display/population.txt";

    char full_fitness[PATH_BUF];
    char full_output[PATH_BUF];
    snprintf(full_fitness, sizeof(full_fitness), "%s/%s", project_root, fitness_path);
    snprintf(full_output,  sizeof(full_output),  "%s/%s", project_root, output_path);

    /* Read fitness data */
    Individual population[MAX_POPULATION];
    int pop_size = 0;

    {
        FILE *f = fopen(full_fitness, "r");
        if (!f) {
            /* Generate a small seed population with random fitness */
            srand((unsigned)time(NULL));
            for (int i = 0; i < 10 && i < MAX_POPULATION; i++) {
                Individual *ind = &population[pop_size++];
                ind->chain_len = 1 + rand() % 4;
                const char *behaviors[] = {
                    "end_turn", "buy_stock", "sell_stock", "check_market"
                };
                int n = sizeof(behaviors) / sizeof(behaviors[0]);
                for (int j = 0; j < ind->chain_len; j++) {
                    int pick = rand() % n;
                    snprintf(ind->chain[j], 64, "%s", behaviors[pick]);
                }
                ind->fitness = (double)(rand() % 1000);
            }
        } else {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), f) && pop_size < MAX_POPULATION) {
                Individual *ind = &population[pop_size++];
                ind->chain_len = 0;
                ind->fitness = 0.0;
                /* Expected format: fitness=<n> chain=<b1>|<b2>|... */
                char *p = strstr(line, "fitness=");
                if (p) ind->fitness = atof(p + 8);
                p = strstr(line, "chain=");
                if (p) {
                    char *chain_str = p + 6;
                    char *tok = strtok(chain_str, "|");
                    while (tok && ind->chain_len < MAX_CHAIN) {
                        tok[strcspn(tok, "\r\n")] = '\0';
                        snprintf(ind->chain[ind->chain_len++], 64, "%s", tok);
                        tok = strtok(NULL, "|");
                    }
                }
            }
            fclose(f);
        }
    }

    /* Sort by fitness descending */
    qsort(population, (size_t)pop_size, sizeof(Individual), cmp_fitness_desc);

    /* Evolve: keep top 50%, mutate 20%, crossover 30% */
    int keep = pop_size / 2;
    int mutate_count = pop_size / 5;
    int crossover_count = pop_size - keep - mutate_count;
    if (keep < 1) keep = 1;

    srand((unsigned)time(NULL));

    /* Mutate some of the kept individuals */
    for (int i = 0; i < mutate_count && (keep + i) < pop_size; i++) {
        mutate(&population[keep + i]);
    }

    /* Crossover pairs */
    for (int i = 0; i < crossover_count; i += 2) {
        if (keep + mutate_count + i + 1 < pop_size) {
            crossover(&population[keep + mutate_count + i],
                      &population[keep + mutate_count + i + 1]);
        }
    }

    /* Write evolved population */
    {
        FILE *f = fopen(full_output, "w");
        if (f) {
            for (int i = 0; i < pop_size; i++) {
                fprintf(f, "ind_%d fitness=%.2f chain=", i, population[i].fitness);
                for (int j = 0; j < population[i].chain_len; j++) {
                    if (j) fputc('|', f);
                    fputs(population[i].chain[j], f);
                }
                fputc('\n', f);
            }
            fclose(f);
        }
    }

    double best = pop_size > 0 ? population[0].fitness : 0.0;
    append_event("EVOLVE");
    char evt[MAX_LINE];
    snprintf(evt, sizeof(evt), "EVOLVE|best_fitness=%.2f|population_size=%d",
             best, pop_size);
    append_event(evt);

    return 0;
}