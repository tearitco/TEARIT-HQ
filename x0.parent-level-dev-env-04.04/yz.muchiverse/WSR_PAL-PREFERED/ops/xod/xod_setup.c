/* xod_setup — Tournament Setup C op for XOD.
 *
 * Handles all configuration actions from wsr_xod_setup piece.pdl.
 * Writes agent_config.json per slot, manages tournament config.
 *
 * Actions (via argv[1]):
 *   SET_AGENT_TYPE|<slot>|<type>      -- type: llm|goap|human|scripted
 *   SET_AGENT_MODEL|<slot>|<model>    -- Ollama model name
 *   SET_AGENT_GOAL|<slot>|<goal>      -- survive|grow|accumulate|check_market
 *   SET_AGENT_TEMP|<slot>|<temp>      -- temperature 0.0-1.0
 *   SET_DURATION|<seconds>
 *   ADD_AGENT
 *   REMOVE_AGENT
 *   RESET_CONFIG
 *   START_TOURNAMENT
 *   CANCEL
 *
 * Outputs: writes agent_config.json files to session dir
 * Emits SETUP_* events to interact_relay.txt
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#define access _access
#define F_OK 0
#define mkdir(path, mode) _mkdir(path)
#else
#include <unistd.h>
#endif

#define MAX_LINE 4096
#define PATH_BUF (4096 + 512)
#define MAX_AGENTS 8

static char project_root[PATH_BUF] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void append_event(const char *event) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/interact_relay.txt", project_root);
    FILE *f = fopen(path, "a");
    if (f) { fprintf(f, "%s\n", event); fflush(f); fclose(f); }
}

static int read_file(const char *path, char *buf, size_t sz) {
    FILE *f = fopen(path, "r");
    if (!f) { buf[0] = '\0'; return 0; }
    size_t n = fread(buf, 1, sz - 1, f);
    buf[n] = '\0';
    fclose(f);
    return n > 0;
}

static void write_file(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    if (!f) return;
    fputs(content, f);
    fclose(f);
}

static int ensure_session_dirs(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/sessions", project_root);
    return mkdir(path, 0755);
}

/* Read current config from session state file. */
static int read_config(char agents[MAX_AGENTS][4][64], int *num_agents, int *duration) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/xod_setup_state.txt", project_root);
    
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    
    *num_agents = 0;
    *duration = 60;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strncmp(line, "num_agents=", 11) == 0) {
            *num_agents = atoi(line + 11);
        } else if (strncmp(line, "duration=", 9) == 0) {
            *duration = atoi(line + 9);
} else if (strncmp(line, "agent_", 6) == 0) {
                char *eq = strchr(line, '=');
                if (!eq) continue;
                *eq = '\0';
                char *key = line + 6;  /* skip "agent_" */
                char *slot_end = strchr(key, '_');
                if (!slot_end) continue;
                *slot_end = '\0';
                *eq = '\0';
                int slot = atoi(key);
                if (slot < 1 || slot > MAX_AGENTS) continue;
                char *field = slot_end + 1;
                char *value = eq + 1;
                
                int idx = -1;
                if (strcmp(field, "type") == 0) idx = 0;
                else if (strcmp(field, "model") == 0) idx = 1;
                else if (strcmp(field, "goal") == 0) idx = 2;
                else if (strcmp(field, "temp") == 0) idx = 3;
                if (idx >= 0 && slot >= 1 && slot <= MAX_AGENTS) {
                    snprintf(agents[slot-1][idx], 64, "%s", value);
                }
            }
    }
    fclose(f);
    return 1;
}

/* Write config to session state file. */
static void write_config(char agents[MAX_AGENTS][4][64], int num_agents, int duration) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/xod_setup_state.txt", project_root);
    
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "num_agents=%d\n", num_agents);
    fprintf(f, "duration=%d\n", duration);
    for (int i = 0; i < num_agents; i++) {
        fprintf(f, "agent_%d_type=%s\n", i+1, agents[i][0][0] ? agents[i][0] : "llm");
        fprintf(f, "agent_%d_model=%s\n", i+1, agents[i][1][0] ? agents[i][1] : "gemma3:1b");
        fprintf(f, "agent_%d_goal=%s\n", i+1, agents[i][2][0] ? agents[i][2] : "survive");
        fprintf(f, "agent_%d_temp=%s\n", i+1, agents[i][3][0] ? agents[i][3] : "0.3");
    }
    fclose(f);
}

/* Generate agent_slots display string for CHTPM. */
static void generate_agent_slots(char agents[MAX_AGENTS][4][64], int num_agents, char *out, size_t out_sz) {
    char *p = out;
    size_t rem = out_sz;
    for (int i = 0; i < num_agents; i++) {
        int n = snprintf(p, rem,
            "  Agent %d: Type=%s Model=%s Goal=%s Temp=%s\n",
            i+1,
            agents[i][0][0] ? agents[i][0] : "llm",
            agents[i][1][0] ? agents[i][1] : "gemma3:1b",
            agents[i][2][0] ? agents[i][2] : "survive",
            agents[i][3][0] ? agents[i][3] : "0.3");
        if (n >= 0) { p += n; rem = (rem > (size_t)n) ? rem - n : 0; }
    }
    if (num_agents == 0) {
        snprintf(out, out_sz, "  (no agents configured)");
    }
}

/* Generate piece_methods display for CHTPM. */
static void generate_piece_methods(int num_agents, char *out, size_t out_sz) {
    char *p = out;
    size_t rem = out_sz;
    for (int i = 0; i < num_agents; i++) {
        int base = i * 4 + 1;
        int n = snprintf(p, rem,
            "  [ ] Agent %d Type      [ ] Agent %d Model      [ ] Agent %d Goal      [ ] Agent %d Temp\n",
            i+1, i+1, i+1, i+1);
        if (n >= 0) { p += n; rem = (rem > (size_t)n) ? rem - n : 0; }
    }
    if (num_agents == 0) {
        snprintf(out, out_sz, "  [ ] Add Agent Slot");
    } else {
        int n = snprintf(p, rem,
            "  [ ] Duration (s)       [ ] Add Agent Slot       [ ] Remove Agent Slot\n");
        if (n >= 0) { p += n; rem = (rem > (size_t)n) ? rem - n : 0; }
    }
}

/* Write individual agent config JSON for the agent runner. */
static void write_agent_configs(char agents[MAX_AGENTS][4][64], int num_agents, int duration) {
    for (int i = 0; i < num_agents; i++) {
        char path[PATH_BUF];
        snprintf(path, sizeof(path), "%s/pieces/apps/player_app/agent_config_%d.json", project_root, i+1);
        
        char type[64], model[64], goal[64], temp[64];
        snprintf(type, sizeof(type), "%s", agents[i][0][0] ? agents[i][0] : "llm");
        snprintf(model, sizeof(model), "%s", agents[i][1][0] ? agents[i][1] : "gemma3:1b");
        snprintf(goal, sizeof(goal), "%s", agents[i][2][0] ? agents[i][2] : "survive");
        snprintf(temp, sizeof(temp), "%s", agents[i][3][0] ? agents[i][3] : "0.3");
        
        FILE *f = fopen(path, "w");
        if (f) {
            fprintf(f, "{\n");
            fprintf(f, "  \"type\": \"%s\",\n", type);
            fprintf(f, "  \"model\": \"%s\",\n", model);
            fprintf(f, "  \"goal\": \"%s\",\n", goal);
            fprintf(f, "  \"temperature\": %s,\n", temp);
            fprintf(f, "  \"slot\": %d\n", i+1);
            fprintf(f, "}\n");
            fclose(f);
        }
    }
    /* Write tournament config */
    char tourney_path[PATH_BUF];
    snprintf(tourney_path, sizeof(tourney_path), "%s/pieces/apps/player_app/tournament_config.json", project_root);
    FILE *tf = fopen(tourney_path, "w");
    if (tf) {
        fprintf(tf, "{\n");
        fprintf(tf, "  \"num_agents\": %d,\n", num_agents);
        fprintf(tf, "  \"duration_seconds\": %d,\n", duration);
        fprintf(tf, "  \"created\": %ld\n", (long)time(NULL));
        fprintf(tf, "}\n");
        fclose(tf);
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <action> [args...]\n", argv[0]);
        return 1;
    }
    resolve_root();
    ensure_session_dirs();

    char agents[MAX_AGENTS][4][64] = {0};
    int num_agents = 0, duration = 60;
    read_config(agents, &num_agents, &duration);

    char *action = argv[1];

    if (strcmp(action, "SET_AGENT_TYPE") == 0 && argc >= 4) {
        int slot = atoi(argv[2]);
        if (slot >= 1 && slot <= MAX_AGENTS && slot <= num_agents) {
            snprintf(agents[slot-1][0], 64, "%s", argv[3]);
            write_config(agents, num_agents, duration);
            append_event("SETUP_AGENT_TYPE|slot|type");
        }
    }
    else if (strcmp(action, "SET_AGENT_MODEL") == 0 && argc >= 4) {
        int slot = atoi(argv[2]);
        if (slot >= 1 && slot <= MAX_AGENTS && slot <= num_agents) {
            snprintf(agents[slot-1][1], 64, "%s", argv[3]);
            write_config(agents, num_agents, duration);
            append_event("SETUP_AGENT_MODEL|slot|model");
        }
    }
    else if (strcmp(action, "SET_AGENT_GOAL") == 0 && argc >= 4) {
        int slot = atoi(argv[2]);
        if (slot >= 1 && slot <= MAX_AGENTS && slot <= num_agents) {
            snprintf(agents[slot-1][2], 64, "%s", argv[3]);
            write_config(agents, num_agents, duration);
            append_event("SETUP_AGENT_GOAL|slot|goal");
        }
    }
    else if (strcmp(action, "SET_AGENT_TEMP") == 0 && argc >= 4) {
        int slot = atoi(argv[2]);
        if (slot >= 1 && slot <= MAX_AGENTS && slot <= num_agents) {
            double t = atof(argv[3]);
            if (t < 0) t = 0; if (t > 1) t = 1;
            snprintf(agents[slot-1][3], 64, "%.2f", t);
            write_config(agents, num_agents, duration);
            append_event("SETUP_AGENT_TEMP|slot|temp");
        }
    }
    else if (strcmp(action, "SET_DURATION") == 0 && argc >= 3) {
        duration = atoi(argv[2]);
        if (duration < 10) duration = 10;
        if (duration > 3600) duration = 3600;
        write_config(agents, num_agents, duration);
        append_event("SETUP_DURATION|seconds");
    }
    else if (strcmp(action, "ADD_AGENT") == 0) {
        if (num_agents < MAX_AGENTS) {
            num_agents++;
            snprintf(agents[num_agents-1][0], 64, "llm");
            snprintf(agents[num_agents-1][1], 64, "gemma3:1b");
            snprintf(agents[num_agents-1][2], 64, "survive");
            snprintf(agents[num_agents-1][3], 64, "0.3");
            write_config(agents, num_agents, duration);
            append_event("SETUP_ADD_AGENT|num_agents");
        }
    }
    else if (strcmp(action, "REMOVE_AGENT") == 0) {
        if (num_agents > 0) {
            num_agents--;
            write_config(agents, num_agents, duration);
            append_event("SETUP_REMOVE_AGENT|num_agents");
        }
    }
    else if (strcmp(action, "RESET_CONFIG") == 0) {
        num_agents = 1;
        snprintf(agents[0][0], 64, "llm");
        snprintf(agents[0][1], 64, "gemma3:1b");
        snprintf(agents[0][2], 64, "survive");
        snprintf(agents[0][3], 64, "0.3");
        duration = 60;
        write_config(agents, num_agents, duration);
        append_event("SETUP_RESET");
    }
    else if (strcmp(action, "START_TOURNAMENT") == 0) {
        if (num_agents >= 1) {
            write_agent_configs(agents, num_agents, duration);
            append_event("SETUP_START|num_agents|duration");
            printf("Tournament started with %d agents for %d seconds\n", num_agents, duration);
        } else {
            append_event("SETUP_START_FAILED|no_agents");
        }
    }
    else if (strcmp(action, "CANCEL") == 0) {
        append_event("SETUP_CANCEL");
        printf("Setup cancelled\n");
    }

    /* Always regenerate display strings for CHTPM */
    char agent_slots[2048] = "";
    char piece_methods[2048] = "";
    generate_agent_slots(agents, num_agents, agent_slots, sizeof(agent_slots));
    generate_piece_methods(num_agents, piece_methods, sizeof(piece_methods));

    char status[64];
    snprintf(status, sizeof(status), "%s", num_agents >= 1 ? "Ready" : "Need agents");

    char state_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/pieces/apps/player_app/xod_setup_display.txt", project_root);
    FILE *sf = fopen(state_path, "w");
    if (sf) {
        fprintf(sf, "num_agents=%d\n", num_agents);
        fprintf(sf, "duration=%d\n", duration);
        fprintf(sf, "status=%s\n", status);
        fprintf(sf, "ready_to_start=%d\n", num_agents >= 1);
        fprintf(sf, "can_add_agent=%d\n", num_agents < MAX_AGENTS);
        fprintf(sf, "can_remove_agent=%d\n", num_agents > 1);
        fprintf(sf, "agent_slots=%s\n", agent_slots);
        fprintf(sf, "piece_methods=%s\n", piece_methods);
        fclose(sf);
    }

    return 0;
}