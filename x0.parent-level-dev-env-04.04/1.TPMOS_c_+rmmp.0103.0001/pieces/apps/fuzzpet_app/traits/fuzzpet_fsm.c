/*
 * fuzzpet_fsm — FSM controller for FuzzPet (C op, house pattern).
 *
 * Reads PET_ACTION from the relay event bus, maps it to the
 * corresponding state mutation or op invocation, and executes it.
 *
 * Hybrid architecture:
 * - Relay pattern: brain.c writes PET_ACTION to interact_relay.txt
 * - Book:Page: behavior.pdl declares METHOD -> .asm script mappings
 * - brain_dispatch.asm reads relay and dispatches to .asm scripts
 *   that call this FSM directly
 *
 * This FSM reads/writes state at the fuzzpet_app path:
 *   pieces/apps/fuzzpet_app/fuzzpet/state.txt
 *
 * Usage: fuzzpet_fsm.+x <project_root>
 *   project_root defaults to $PRISC_PROJECT_ROOT or "."
 *
 *   Reads last PET_ACTION from interact_relay.txt,
 *   updates fuzzpet state.txt directly,
 *   writes ACTION_EXECUTED to relay.
 *
 * Actions: feed, play, sleep, explore, heal, evolve, toggle_emoji, wait
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

#define MAX_LINE    1024
#define PATH_BUF    4096
#define MAX_FIELDS  32

static char project_root[PATH_BUF] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) {
        snprintf(project_root, sizeof(project_root), "%s", env);
    }
}

static char *path_join(char *buf, size_t sz, const char *base, const char *rel) {
    snprintf(buf, sz, "%s/%s", base, rel);
    return buf;
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

static int read_kv(const char *path, const char *key, char *val, size_t sz) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int found = 0;
    size_t keylen = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, keylen) == 0 && line[keylen] == '=') {
            char *p = line + keylen + 1;
            char *end = strpbrk(p, "\r\n");
            if (end) *end = '\0';
            strncpy(val, p, sz - 1);
            val[sz - 1] = '\0';
            found = 1;
            break;
        }
    }
    fclose(f);
    return found;
}

/* Write/update a key=value pair in state.txt */
static void set_state_kv(const char *path, const char *key, int val) {
    char lines[100][MAX_LINE];
    int line_count = 0;
    int found = 0;
    char kvb[MAX_LINE];

    FILE *f = fopen(path, "r");
    if (f) {
        while (fgets(lines[line_count], sizeof(lines[0]), f) && line_count < 99) {
            char *eq = strchr(lines[line_count], '=');
            if (eq) {
                *eq = '\0';
                if (strcmp(trim_str(lines[line_count]), key) == 0) {
                    snprintf(lines[line_count], sizeof(lines[0]), "%s=%d\n", key, val);
                    found = 1;
                } else {
                    *eq = '=';
                }
            }
            line_count++;
        }
        fclose(f);
    }

    if (!found && line_count < 100) {
        snprintf(lines[line_count++], sizeof(lines[0]), "%s=%d\n", key, val);
    }

    f = fopen(path, "w");
    if (f) {
        for (int i = 0; i < line_count; i++) {
            fputs(lines[i], f);
        }
        fclose(f);
    }
}

static void hit_frame(const char *root) {
    char path[PATH_BUF];
    path_join(path, sizeof(path), root, "pieces/display/frame_changed.txt");
    FILE *f = fopen(path, "a");
    if (f) { fprintf(f, "X\n"); fclose(f); }
}

static void log_master_ledger(const char *root, const char *piece_id,
                               const char *action, const char *msg) {
    char path[PATH_BUF];
    path_join(path, sizeof(path), root, "pieces/master_ledger/master_ledger.txt");
    FILE *f = fopen(path, "a");
    if (f) {
        time_t now = time(NULL);
        fprintf(f, "[%ld] %s | %s: %s\n", now, piece_id, action, msg);
        fclose(f);
    }
}

/* Execute a pet action — reads and writes state directly */
static int execute_action(const char *root, const char *relay_path,
                          const char *action, const char *pet_id) {
    char state_path[PATH_BUF];
    path_join(state_path, sizeof(state_path), root,
              "pieces/apps/fuzzpet_app/fuzzpet/state.txt");

    int hunger = 0, happiness = 0, energy = 0, level = 0;
    char name[64] = "Fuzzball";

    char val[256];
    if (read_kv(state_path, "name", val, sizeof(val)))
        strncpy(name, val, sizeof(name) - 1);
    if (read_kv(state_path, "hunger", val, sizeof(val)))   hunger = atoi(val);
    if (read_kv(state_path, "happiness", val, sizeof(val))) happiness = atoi(val);
    if (read_kv(state_path, "energy", val, sizeof(val)))   energy = atoi(val);
    if (read_kv(state_path, "level", val, sizeof(val)))    level = atoi(val);

    /* Apply action effects */
    int guardrail = 0;
    char msg[256] = "";

    if (strcmp(action, "feed") == 0) {
        hunger = (hunger > 20) ? hunger - 20 : 0;
        snprintf(msg, sizeof(msg), "Yum! Thanks for the food.");
        log_master_ledger(root, pet_id, action, msg);
    }
    else if (strcmp(action, "play") == 0) {
        happiness = (happiness < 90) ? happiness + 10 : 100;
        snprintf(msg, sizeof(msg), "Yay! That was fun.");
        log_master_ledger(root, pet_id, action, msg);
    }
    else if (strcmp(action, "sleep") == 0) {
        energy = (energy < 70) ? energy + 30 : 100;
        snprintf(msg, sizeof(msg), "Zzz... feeling refreshed.");
        log_master_ledger(root, pet_id, action, msg);
    }
    else if (strcmp(action, "toggle_emoji") == 0) {
        int emoji = 0;
        if (read_kv(state_path, "emoji_mode", val, sizeof(val)))
            emoji = atoi(val);
        emoji = emoji ? 0 : 1;
        set_state_kv(state_path, "emoji_mode", emoji);
        snprintf(msg, sizeof(msg), emoji ? "ASCII Mode" : "Emoji Mode");
        log_master_ledger(root, pet_id, "emoji", msg);
    }
    else if (strcmp(action, "heal") == 0) {
        energy = (energy < 70) ? energy + 30 : 100;
        set_state_kv(state_path, "energy", energy);
        happiness = (happiness < 90) ? happiness + 10 : 100;
        set_state_kv(state_path, "happiness", happiness);
        snprintf(msg, sizeof(msg), "Purrr... restored!");
        log_master_ledger(root, pet_id, "heal", msg);
    }
    else if (strcmp(action, "explore") == 0) {
        /* Use zombie_ai for pathfinding */
        char cmd[PATH_BUF];
        char zombie_path[PATH_BUF];
        path_join(zombie_path, sizeof(zombie_path), root,
                  "pieces/apps/fuzzpet_app/traits/+x/zombie_ai.+x");
        if (access(zombie_path, X_OK) == 0) {
            snprintf(cmd, sizeof(cmd), "%s zombie_01 fuzzball 2>&1", zombie_path);
            int rc = system(cmd);
            snprintf(msg, sizeof(msg), "explored (rc=%d)", rc);
        } else {
            snprintf(msg, sizeof(msg), "explore: zombie_ai not found, no movement");
        }
        log_master_ledger(root, pet_id, "explore", msg);
    }
    else if (strcmp(action, "evolve") == 0) {
        level++;
        snprintf(msg, sizeof(msg), "Evolved to level %d!", level);
        log_master_ledger(root, pet_id, "evolve", msg);
    }
    else if (strcmp(action, "wait") == 0 || strcmp(action, "idle") == 0) {
        snprintf(msg, sizeof(msg), "Waiting...");
        log_master_ledger(root, pet_id, "wait", msg);
    }
    else {
        char event[PATH_BUF];
        snprintf(event, sizeof(event), "ACTION_ERROR|unknown_action|%s", action);
        append_line(relay_path, event);
        return 1;
    }

    /* Update state file for actions that change stats */
    if (strcmp(action, "feed") == 0)
        set_state_kv(state_path, "hunger", hunger);
    else if (strcmp(action, "play") == 0)
        set_state_kv(state_path, "happiness", happiness);
    else if (strcmp(action, "sleep") == 0)
        set_state_kv(state_path, "energy", energy);
    else if (strcmp(action, "heal") == 0)
        set_state_kv(state_path, "level", level);
    else if (strcmp(action, "evolve") == 0)
        set_state_kv(state_path, "level", level);

    /* Trigger frame update */
    hit_frame(root);

    /* Write completion to relay */
    char event[PATH_BUF];
    snprintf(event, sizeof(event),
             "ACTION_EXECUTED|%s|%s|hunger=%d|happiness=%d|energy=%d|level=%d",
             action, pet_id, hunger, happiness, energy, level);
    append_line(relay_path, event);

    printf("Action '%s' executed: hunger=%d happiness=%d energy=%d level=%d\n",
           action, hunger, happiness, energy, level);
    return 0;
}

int main(int argc, char **argv) {
    resolve_root();

    char relay_path[PATH_BUF];
    char pet_id[64] = "fuzzball";

    if (argc >= 2) {
        strncpy(pet_id, argv[1], sizeof(pet_id) - 1);
    }

    path_join(relay_path, sizeof(relay_path), project_root,
              "pieces/apps/player_app/interact_relay.txt");

    char last_action_line[MAX_LINE] = "";
    if (!read_last_line(relay_path, last_action_line, sizeof(last_action_line))) {
        fprintf(stderr, "No PET_ACTION found in relay\n");
        return 1;
    }

    /* Parse: PET_ACTION|action|confidence|reason|pet=name|hunger=N|... */
    char *p = last_action_line + 11; /* skip "PET_ACTION|" */
    char action[64] = "";
    char *pipe_sep = strchr(p, '|');
    if (pipe_sep) {
        size_t len = pipe_sep - p;
        if (len < sizeof(action)) {
            memcpy(action, p, len);
            action[len] = '\0';
        }
    }

    printf("[fsm] Executing: %s on %s\n", action, pet_id);
    int rc = execute_action(project_root, relay_path, action, pet_id);

    return rc;
}
