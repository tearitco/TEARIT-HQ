/*
 * fuzzpet_brain — LLM-driven pet brain (C op, house pattern).
 *
 * Reads pet state, constructs a context-aware prompt, calls Ollama
 * (or any OpenAI-compatible API), parses the JSON response, applies
 * need-based guardrails, and writes PET_ACTION to the relay event bus.
 *
 * Hybrid architecture:
 * - Relay pattern: PET_ACTION rows go to interact_relay.txt
 * - Book:Page pattern: state synced to behavior.pdl STATE section
 * - Events dispatch: brain_dispatch.asm reads relay, calls .asm scripts
 *
 * Usage: fuzzpet_brain.+x <project> <goal> <model>
 *
 *   project:  "." for auto-detect (uses $PRISC_PROJECT_ROOT)
 *   goal:     survive | explore | evolve
 *   model:    gemma3:1b, llama3.2, etc.
 *
 * Example:
 *   export PRISC_PROJECT_ROOT="$PWD"
 *   ops/+x/fuzzpet_brain.+x . survive gemma3:1b
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define MAX_LINE    1024
#define MAX_STATE   32
#define PATH_BUF    512
#define MAX_PROMPT  4096
#define MAX_RESP    2048

/* Pet state fields */
typedef struct {
    char name[64];
    int  hunger;
    int  happiness;
    int  energy;
    int  level;
    int  pos_x;
    int  pos_y;
} pet_state_t;

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

/* Read key=value from state file */
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

/* Load pet state from state.txt */
static int load_pet_state(const char *root, pet_state_t *state) {
    char path[PATH_BUF];
    char val[256];

    path_join(path, sizeof(path), root,
              "pieces/apps/fuzzpet_app/fuzzpet/state.txt");

    memset(state, 0, sizeof(*state));
    strcpy(state->name, "Fuzzball");
    state->hunger = 50;
    state->happiness = 55;
    state->energy = 100;
    state->level = 1;
    state->pos_x = 0;
    state->pos_y = 0;

    int ok = 1;

    if (read_kv(path, "name", val, sizeof(val)))
        strncpy(state->name, val, sizeof(state->name) - 1);

    if (read_kv(path, "hunger", val, sizeof(val)))
        state->hunger = atoi(val);

    if (read_kv(path, "happiness", val, sizeof(val)))
        state->happiness = atoi(val);

    if (read_kv(path, "energy", val, sizeof(val)))
        state->energy = atoi(val);

    if (read_kv(path, "level", val, sizeof(val)))
        state->level = atoi(val);

    if (read_kv(path, "pos_x", val, sizeof(val)))
        state->pos_x = atoi(val);

    if (read_kv(path, "pos_y", val, sizeof(val)))
        state->pos_y = atoi(val);

    return ok;
}

/* Read last few lines of history for context */
static void read_history(const char *root, char *buf, size_t sz) {
    char path[PATH_BUF];
    path_join(path, sizeof(path), root,
              "pieces/apps/fuzzpet_app/fuzzpet/history.txt");

    FILE *f = fopen(path, "r");
    if (!f) {
        strcpy(buf, "(no history)");
        return;
    }

    char line[MAX_LINE];
    buf[0] = '\0';
    size_t off = 0;
    int line_count = 0;

    /* Keep last 5 lines */
    char *lines[5] = {0};
    int idx = 0;

    while (fgets(line, sizeof(line), f) && line_count < 5) {
        char *p = strpbrk(line, "\r\n");
        if (p) *p = '\0';
        if (strlen(line) > 1) {
            lines[idx % 5] = strdup(line);
            idx++;
            line_count++;
        }
    }
    fclose(f);

    /* Write in chronological order (oldest of last 5 first) */
    for (int i = 0; i < line_count && i < 5; i++) {
        int real_idx = (idx - line_count + i + 5) % 5;
        if (lines[real_idx]) {
            off += snprintf(buf + off, sz - off, "%s\n", lines[real_idx]);
            free(lines[real_idx]);
        }
    }
}

/* Build the LLM prompt */
static void build_prompt(char *prompt, size_t sz,
                         pet_state_t *state,
                         const char *goal,
                         const char *history) {
    snprintf(prompt, sz,
        "You are %s, a digital pet in a text-based world. "
        "Your current status: hunger=%d/100, happiness=%d/100, "
        "energy=%d/100, level=%d, position=(%d,%d).\n\n"
        "Your goal right now: %s.\n\n"
        "Recent history:\n%s\n\n"
        "Available actions: feed (reduces hunger by ~20), "
        "play (+10 happiness), sleep (+30 energy), explore (roam map), "
        "heal (combined sleep+play), evolve (level up + trait mutation), "
        "toggle_emoji (change display).\n\n"
        "Choose the action that best serves your goal. "
        "Output valid JSON with fields: action, reason, confidence. "
        "Example: {\"action\":\"feed\",\"reason\":\"hunger is critical\",\"confidence\":0.9}\n",
        state->name,
        state->hunger, state->happiness, state->energy,
        state->level, state->pos_x, state->pos_y,
        goal,
        history);
}

/* Call Ollama API and get response */
static int call_ollama(const char *model, const char *prompt, char *response, size_t sz) {
    /* Build the JSON payload */
    char *escaped_prompt = malloc(strlen(prompt) * 4 + 1);
    if (!escaped_prompt) return 0;

    /* Escape quotes and backslashes for JSON */
    int j = 0;
    for (size_t i = 0; prompt[i] && j < (int)(strlen(prompt) * 4); i++) {
        if (prompt[i] == '"') { escaped_prompt[j++] = '\\'; escaped_prompt[j++] = '"'; }
        else if (prompt[i] == '\\') { escaped_prompt[j++] = '\\'; escaped_prompt[j++] = '\\'; }
        else if (prompt[i] == '\n') { escaped_prompt[j++] = '\\'; escaped_prompt[j++] = 'n'; }
        else escaped_prompt[j++] = prompt[i];
    }
    escaped_prompt[j] = '\0';

    /* Build payload */
    char payload[8192];
    snprintf(payload, sizeof(payload),
        "{\"model\":\"%s\",\"prompt\":\"%s\",\"stream\":false,"
        "\"options\":{\"temperature\":0.7,\"max_tokens\":200}}",
        model, escaped_prompt);

    free(escaped_prompt);

    /* Write payload to temp file, curl, read response */
    char tmp_path[L_tmpnam];
    strcpy(tmp_path, "/tmp/fuzzpet_payload_XXXXXX");
    int fd = mkstemp(tmp_path);
    if (fd < 0) return 0;
    write(fd, payload, strlen(payload));
    close(fd);

    char cmd[PATH_BUF];
    snprintf(cmd, sizeof(cmd),
        "curl -s -X POST http://localhost:11434/api/generate "
        "-H 'Content-Type: application/json' "
        "-d @%s 2>/dev/null", tmp_path);

    FILE *p = popen(cmd, "r");
    if (!p) {
        unlink(tmp_path);
        return 0;
    }

    size_t off = 0;
    char buf[256];
    while (fgets(buf, sizeof(buf), p) && off < sz - 1) {
        off += snprintf(response + off, sz - off, "%s", buf);
    }
    pclose(p);
    unlink(tmp_path);

    return off > 0;
}

/* Parse JSON response - extract action, reason, confidence */
static int parse_response(const char *json, char *action, size_t asz,
                          char *reason, size_t rsz,
                          float *confidence) {
    /* Simple pattern matching for JSON fields */
    const char *p;

    /* Extract action */
    p = strstr(json, "\"action\"");
    if (p) {
        p = strchr(p, ':');
        if (p) {
            p++; /* skip colon */
            while (*p && (*p == ' ' || *p == '"')) p++;
            const char *end = strchr(p, '"');
            if (!end) end = p + strcspn(p, ",\n}");
            size_t len = end - p;
            if (len >= asz) len = asz - 1;
            memcpy(action, p, len);
            action[len] = '\0';
            while (end > p && (isspace((unsigned char)*(end-1)) || *(end-1) == '"')) end--;
            /* trim */
            len = strcspn(action, "\",}\n");
            action[len] = '\0';
        }
    }

    /* Extract confidence */
    p = strstr(json, "\"confidence\"");
    if (p) {
        p = strchr(p, ':');
        if (p) *confidence = atof(p + 1);
    } else {
        *confidence = 0.5;
    }

    /* Extract reason */
    p = strstr(json, "\"reason\"");
    if (p) {
        p = strchr(p, ':');
        if (p) {
            p++;
            while (*p && (*p == ' ' || *p == '"')) p++;
            const char *end = strchr(p, '"');
            if (!end) end = p + strcspn(p, ",\n}");
            size_t len = end - p;
            if (len >= rsz) len = rsz - 1;
            memcpy(reason, p, len);
            reason[len] = '\0';
        }
    } else {
        strcpy(reason, "No reason given");
    }

    return strlen(action) > 0;
}

/* Need-based guardrails - override LLM decision if survival needs are critical */
static int apply_guardrails(pet_state_t *state,
                            char *action, size_t sz,
                            const char *goal) {
    /* Survival goal: must feed if hunger > 60, play if happiness < 20, sleep if energy < 20 */
    if (strcmp(goal, "survive") == 0 || strcmp(goal, "survive") == 0) {
        if (state->hunger > 60) {
            strcpy(action, "feed");
            return 1;
        }
        if (state->happiness < 20) {
            strcpy(action, "play");
            return 1;
        }
        if (state->energy < 20) {
            strcpy(action, "sleep");
            return 1;
        }
    }

    /* General survival check regardless of goal */
    if (state->hunger > 80) {
        strcpy(action, "feed");
        return 1;
    }
    if (state->energy < 10) {
        strcpy(action, "sleep");
        return 1;
    }

    return 0;
}

/* Write state values to behavior.pdl STATE section for hybrid sync */
static void sync_pdl_state(const char *root, pet_state_t *state) {
    char path[PATH_BUF];
    path_join(path, sizeof(path), root,
              "pieces/apps/fuzzpet_app/fuzzpet/behavior.pdl");

    FILE *f = fopen(path, "r");
    if (!f) return;

    char content[8192];
    size_t sz = fread(content, 1, sizeof(content) - 1, f);
    fclose(f);
    content[sz] = '\0';

    /* Simple in-place replacement for STATE values */
    char tmp_path[PATH_BUF];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    f = fopen(tmp_path, "w");
    if (!f) return;

    fprintf(f, "%s", content);  /* write original */
    fprintf(f, "\n# Synced by fuzzpet_brain at runtime\n");
    fprintf(f, "RUNTIME    | hunger      | %d\n", state->hunger);
    fprintf(f, "RUNTIME    | happiness   | %d\n", state->happiness);
    fprintf(f, "RUNTIME    | energy      | %d\n", state->energy);
    fprintf(f, "RUNTIME    | level       | %d\n", state->level);
    fclose(f);

    rename(tmp_path, path);
}

/* Append to history */
static void append_history(const char *root, const char *action,
                           const char *reason, float confidence,
                           pet_state_t *state) {
    char path[PATH_BUF];
    path_join(path, sizeof(path), root,
              "pieces/apps/fuzzpet_app/fuzzpet/history.txt");

    FILE *f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "%s|action=%s|reason=%s|conf=%.2f|hunger=%d|happiness=%d|energy=%d\n",
            state->name, action, reason, confidence,
            state->hunger, state->happiness, state->energy);
    fclose(f);
}

/* Save last response */
static void save_response(const char *root, const char *response) {
    char path[PATH_BUF];
    path_join(path, sizeof(path), root,
              "pieces/apps/fuzzpet_app/fuzzpet/last_response.txt");
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "%s\n", response);
    fclose(f);
}

/* Write PET_ACTION to the relay event bus */
static void write_relay(const char *root, const char *action,
                        float confidence, const char *reason,
                        pet_state_t *state) {
    char path[PATH_BUF];
    path_join(path, sizeof(path), root,
              "pieces/apps/player_app/interact_relay.txt");

    FILE *f = fopen(path, "a");
    if (!f) return;

    fprintf(f,
        "PET_ACTION|%s|%.2f|%s|pet=%s|hunger=%d|happiness=%d|energy=%d|level=%d|pos_x=%d|pos_y=%d\n",
        action, confidence, reason,
        state->name, state->hunger, state->happiness,
        state->energy, state->level, state->pos_x, state->pos_y);

    fclose(f);
}

int main(int argc, char **argv) {
    resolve_root();

    if (argc < 4) {
        fprintf(stderr, "Usage: %s <project> <goal> <model>\n", argv[0]);
        fprintf(stderr, "  project: path or \".\" for PRISC_PROJECT_ROOT\n");
        fprintf(stderr, "  goal: survive | explore | evolve\n");
        fprintf(stderr, "  model: gemma3:1b, llama3.2, etc.\n");
        return 1;
    }

    const char *project = argv[1];
    const char *goal = argv[2];
    const char *model = argv[3];

    /* Override project root if explicit path given */
    if (strcmp(project, ".") != 0) {
        strncpy(project_root, project, sizeof(project_root) - 1);
    }

    /* Load pet state */
    pet_state_t state;
    if (!load_pet_state(project_root, &state)) {
        fprintf(stderr, "Warning: could not load pet state, using defaults\n");
    }

    printf("[brain] %s: hunger=%d happiness=%d energy=%d level=%d\n",
           state.name, state.hunger, state.happiness, state.energy, state.level);

    /* Build prompt */
    char history[2048];
    read_history(project_root, history, sizeof(history));

    char prompt[MAX_PROMPT];
    build_prompt(prompt, sizeof(prompt), &state, goal, history);

    printf("[brain] Calling Ollama model: %s\n", model);

    /* Call Ollama */
    char response[MAX_RESP];
    if (!call_ollama(model, prompt, response, sizeof(response))) {
        fprintf(stderr, "[brain] Ollama call failed, using fallback\n");

        /* Fallback: need-based decision */
        char action[64];
        if (state.hunger > 60) strcpy(action, "feed");
        else if (state.happiness < 20) strcpy(action, "play");
        else if (state.energy < 20) strcpy(action, "sleep");
        else strcpy(action, "explore");

        write_relay(project_root, action, 0.5, "fallback_need_based", &state);
        append_history(project_root, action, "fallback_need_based", 0.5, &state);
        printf("[brain] Fallback action: %s\n", action);
        return 0;
    }

    save_response(project_root, response);

    /* Parse response */
    char action[64] = "", reason[256] = "";
    float confidence = 0.5;
    if (!parse_response(response, action, sizeof(action),
                        reason, sizeof(reason), &confidence)) {
        fprintf(stderr, "[brain] Could not parse response, using fallback\n");
        if (state.hunger > 60) strcpy(action, "feed");
        else if (state.happiness < 20) strcpy(action, "play");
        else if (state.energy < 20) strcpy(action, "sleep");
        else strcpy(action, "explore");
        write_relay(project_root, action, 0.5, "fallback_parse_failed", &state);
        append_history(project_root, action, "fallback_parse_failed", 0.5, &state);
        printf("[brain] Fallback action: %s\n", action);
        return 0;
    }

    /* Apply guardrails */
    int guardrail_hit = apply_guardrails(&state, action, sizeof(action), goal);
    if (guardrail_hit) {
        printf("[brain] Guardrail override: %s (was %s)\n", action, reason);
        reason[0] = '\0';
        snprintf(reason, sizeof(reason), "guardrail_override_%s", goal);
        confidence = 0.85;
    }

    /* Sync to behavior.pdl (hybrid book:page integration) */
    sync_pdl_state(project_root, &state);

    /* Write to relay */
    write_relay(project_root, action, confidence, reason, &state);
    append_history(project_root, action, reason, confidence, &state);

    printf("[brain] Decision: %s (conf=%.2f, %s)\n", action, confidence, reason);

    /* Output JSON for human readability */
    printf("{\"action\":\"%s\",\"reason\":\"%s\",\"confidence\":%.2f,\"guardrail\":%s}\n",
           action, reason, confidence,
           guardrail_hit ? "true" : "false");

    return 0;
}
