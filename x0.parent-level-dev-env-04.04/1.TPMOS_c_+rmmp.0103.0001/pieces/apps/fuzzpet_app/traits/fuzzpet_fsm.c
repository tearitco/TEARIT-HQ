/* fuzzpet_fsm — FSM controller for FuzzPet (C op, house pattern).
 *
 * Reads PET_ACTION from the relay event bus, maps it to the
 * corresponding fuzzpet_action.+x invocation, and executes it.
 *
 * House pattern: relay → FSM → op execution → state update.
 *
 * Usage: fuzzpet_fsm.+x
 *   Reads last PET_ACTION from interact_relay.txt,
 *   calls fuzzpet_action.+x with the mapped action,
 *   writes ACTION_EXECUTED to relay.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define MAX_LINE 256
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

static int read_last_line(const char *path, char *buf, size_t sz) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "PET_ACTION|", 11) == 0) {
            size_t len = strlen(line);
            if (len > sz - 1) len = sz - 1;
            memcpy(buf, line, len);
            buf[len] = '\0';
            found = 1;
        }
    }
    fclose(f);
    return found;
}

static char *trim_str(char *str) {
    while (isspace((unsigned char)*str)) str++;
    if (*str == '\0') return str;
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

/* Map pet action to fuzzpet_action args.
 * Returns 0 on success, 1 if action is unknown. */
static int execute_pet_action(const char *relay_path,
                              const char *action,
                              const char *pet_id) {
    char cmd[PATH_BUF];
    char op_path[PATH_BUF];
    char action_op_path[PATH_BUF];

    snprintf(op_path, sizeof(op_path),
             "%s/pieces/apps/playrm/ops/+x/fuzzpet_action.+x",
             project_root);
    if (access(op_path, X_OK) != 0) {
        snprintf(op_path, sizeof(op_path),
                 "%s/ops/+x/fuzzpet_action.+x", project_root);
    }

    /* Known actions: feed, play, sleep, toggle_emoji, explore, heal, evolve */
    if (strcmp(action, "feed") == 0 ||
        strcmp(action, "play") == 0 ||
        strcmp(action, "sleep") == 0 ||
        strcmp(action, "toggle_emoji") == 0) {
        /* Direct mapping to existing fuzzpet_action handlers */
        snprintf(cmd, sizeof(cmd), "%s %s %s 2>&1",
                 op_path, pet_id, action);
        int rc = system(cmd);

        char event[PATH_BUF];
        snprintf(event, sizeof(event),
                 "ACTION_EXECUTED|%s|%s|rc=%d", action, pet_id, rc);
        append_line(relay_path, event);
        return rc == 0 ? 0 : 1;
    }
    else if (strcmp(action, "explore") == 0) {
        /* Explore: random walk via zombie_ai pathfinding
         * Target a random tile. Use existing zombie_ai for pathfinding. */
        char *zombie_op = NULL;
        snprintf(op_path, sizeof(op_path),
                 "%s/pieces/apps/fuzzpet_app/traits/+x/zombie_ai.+x",
                 project_root);
        if (access(op_path, X_OK) == 0)
            zombie_op = strdup(op_path);
        else {
            snprintf(op_path, sizeof(op_path),
                     "%s/pieces/apps/fuzzpet_app/traits/zombie_ai.c",
                     project_root);
            if (access(op_path, R_OK) == 0) {
                snprintf(cmd, sizeof(cmd),
                         "gcc -O2 -o %s/pieces/apps/fuzzpet_app/traits/+x/zombie_ai.+x "
                         "%s 2>/dev/null",
                         project_root, op_path);
                system(cmd);
                zombie_op = strdup(project_root);
                strncat(zombie_op, "/pieces/apps/fuzzpet_app/traits/+x/zombie_ai.+x",
                        PATH_BUF - strlen(zombie_op) - 1);
            }
        }

        if (zombie_op && access(zombie_op, X_OK) == 0) {
            /* Move zombie toward pet */
            snprintf(cmd, sizeof(cmd), "%s zombie_01 fuzzpet 2>&1", zombie_op);
            int rc = system(cmd);
            char event[PATH_BUF];
            snprintf(event, sizeof(event),
                     "ACTION_EXECUTED|explore|fuzzpet|rc=%d|method=zombie_ai", rc);
            append_line(relay_path, event);
            free(zombie_op);
            return rc == 0 ? 0 : 1;
        }
        return 1;
    }
    else if (strcmp(action, "heal") == 0) {
        /* Heal: restore energy via sleep, boost happiness via play */
        snprintf(cmd, sizeof(cmd), "%s %s sleep 2>&1", op_path, pet_id);
        system(cmd);
        snprintf(cmd, sizeof(cmd), "%s %s play 2>&1", op_path, pet_id);
        system(cmd);
        append_line(relay_path, "ACTION_EXECUTED|heal|combined_sleep_play");
        return 0;
    }
    else if (strcmp(action, "evolve") == 0) {
        /* Evolve: increase level, mutate traits
         * Calls wsr_evolve to mutate trait thresholds */
        char evolve_op[PATH_BUF];
        snprintf(evolve_op, sizeof(evolve_op),
                 "%s/ops/+x/wsr_evolve.+x", project_root);
        if (access(evolve_op, X_OK) == 0) {
            snprintf(cmd, sizeof(cmd),
                     "%s pieces/display/fitness.txt "
                     "pieces/apps/fuzzpet_app/traits/ "
                     "pieces/display/pet_population.txt 2>&1",
                     evolve_op);
            system(cmd);
            append_line(relay_path, "ACTION_EXECUTED|evolve|traits_mutated");
            return 0;
        }
        return 1;
    }
    else if (strcmp(action, "wait") == 0) {
        /* No action needed */
        append_line(relay_path, "ACTION_EXECUTED|wait|none");
        return 0;
    }

    /* Unknown action */
    char event[PATH_BUF];
    snprintf(event, sizeof(event),
             "ACTION_ERROR|unknown_action|%s", action);
    append_line(relay_path, event);
    return 1;
}

int main(int argc, char **argv) {
    resolve_root();

    char relay_path[PATH_BUF];
    char pet_id[64] = "fuzzpet";

    /* Allow optional pet_id override */
    if (argc >= 2) strncpy(pet_id, argv[1], sizeof(pet_id) - 1);

    snprintf(relay_path, sizeof(relay_path),
             "%s/pieces/apps/player_app/interact_relay.txt", project_root);

    char last_action_line[MAX_LINE] = "";
    if (!read_last_line(relay_path, last_action_line, sizeof(last_action_line))) {
        fprintf(stderr, "No PET_ACTION found in relay\n");
        return 1;
    }

    /* Parse: PET_ACTION|action|confidence|reason|pet=name|hunger=N|... */
    char *p = last_action_line + 11; /* skip "PET_ACTION|" */
    char action[64] = "";
    char *pipe = strchr(p, '|');
    if (pipe) {
        size_t len = pipe - p;
        if (len < sizeof(action)) {
            memcpy(action, p, len);
            action[len] = '\0';
        }
        /* Extract pet= value if present */
        char *pet_field = strstr(pipe + 1, "pet=");
        if (pet_field) {
            char *end = strchr(pet_field + 4, '|');
            if (end) {
                size_t plen = end - (pet_field + 4);
                if (plen < sizeof(pet_id)) {
                    memcpy(pet_id, pet_field + 4, plen);
                    pet_id[plen] = '\0';
                }
            }
        }
    }

    printf("Executing: %s on %s\n", action, pet_id);
    int rc = execute_pet_action(relay_path, action, pet_id);

    if (rc == 0) {
        printf("Action '%s' completed successfully\n", action);
    } else {
        printf("Action '%s' failed (rc=%d)\n", action, rc);
    }

    return rc;
}
