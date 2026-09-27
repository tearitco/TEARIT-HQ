#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <glob.h>
#include <stdarg.h>

#define MAX_LINE 1024
#define MAX_TRIGGERS 16
#define MAX_PATH 2048

typedef struct {
    char trigger_name[256];
    char condition[256];
    char event_func[256];
} Trigger;

void log_debug(const char *log_file, const char *format, ...) {
    va_list args;
    va_start(args, format);

    FILE *fp = fopen(log_file, "a");
    if (fp) {
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char timestamp[32];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
        fprintf(fp, "[%s] ", timestamp);
        vfprintf(fp, format, args);
        fprintf(fp, "\n");
        fclose(fp);
    }
    va_end(args);
}

void read_cursors(const char *cursor_file, int *ent, int *ev, int *anim) {
    *ent = 0;
    *ev = 0;
    *anim = 0;

    FILE *fp = fopen(cursor_file, "r");
    if (!fp) return;

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "entities_live: %d", ent) == 1) continue;
        if (sscanf(line, "world_events: %d", ev) == 1) continue;
        if (sscanf(line, "animation_queue: %d", anim) == 1) continue;
    }
    fclose(fp);
}

void write_cursors(const char *cursor_file, int ent, int ev, int anim) {
    FILE *fp = fopen(cursor_file, "w");
    if (fp) {
        fprintf(fp, "entities_live: %d\n", ent);
        fprintf(fp, "world_events: %d\n", ev);
        fprintf(fp, "animation_queue: %d\n", anim);
        fclose(fp);
    }
}

int load_trigger_table(const char *trigger_file, Trigger *triggers) {
    FILE *fp = fopen(trigger_file, "r");
    if (!fp) return 0;

    int count = 0;
    char line[MAX_LINE];
    int line_num = 0;

    while (fgets(line, sizeof(line), fp) && count < MAX_TRIGGERS) {
        line_num++;
        if (line_num <= 1) continue;  // Skip header

        char *stripped = line;
        while (*stripped == ' ' || *stripped == '\t') stripped++;
        if (*stripped == '\0' || *stripped == '\n') continue;

        // Parse pipe-separated: trigger_name | condition | event_func
        char *pos = stripped;
        char *pipe1 = strchr(pos, '|');
        if (!pipe1) continue;

        char *pipe2 = strchr(pipe1 + 1, '|');
        if (!pipe2) continue;

        int name_len = pipe1 - pos;
        int cond_len = pipe2 - pipe1 - 1;
        int func_len = strlen(pipe2 + 1);

        strncpy(triggers[count].trigger_name, pos, name_len);
        triggers[count].trigger_name[name_len] = '\0';

        strncpy(triggers[count].condition, pipe1 + 1, cond_len);
        triggers[count].condition[cond_len] = '\0';

        strncpy(triggers[count].event_func, pipe2 + 1, func_len);
        triggers[count].event_func[func_len] = '\0';

        // Trim whitespace
        for (int i = strlen(triggers[count].trigger_name) - 1; i >= 0 &&
             (triggers[count].trigger_name[i] == ' ' || triggers[count].trigger_name[i] == '\t'); i--)
            triggers[count].trigger_name[i] = '\0';
        for (int i = strlen(triggers[count].condition) - 1; i >= 0 &&
             (triggers[count].condition[i] == ' ' || triggers[count].condition[i] == '\t' || triggers[count].condition[i] == '\n'); i--)
            triggers[count].condition[i] = '\0';
        for (int i = strlen(triggers[count].event_func) - 1; i >= 0 &&
             (triggers[count].event_func[i] == ' ' || triggers[count].event_func[i] == '\t' || triggers[count].event_func[i] == '\n'); i--)
            triggers[count].event_func[i] = '\0';

        count++;
    }
    fclose(fp);
    return count;
}

int poll_entities(const char *entities_file, const char *log_file,
                  const char *page_root, const char *house_root,
                  Trigger *triggers, int trigger_count, int start_line) {
    FILE *fp = fopen(entities_file, "r");
    if (!fp) return start_line;

    char line[MAX_LINE];
    int line_num = 0;
    int current_line = start_line;

    while (fgets(line, sizeof(line), fp)) {
        line_num++;
        if (line_num <= start_line) continue;

        char *stripped = line;
        while (*stripped == ' ' || *stripped == '\t') stripped++;
        if (*stripped == '\0' || *stripped == '\n') continue;

        // Parse entity line: "entity_id | x=N | y=N | ..."
        char entity_id[256] = {0};
        int x_val = 0, y_val = 0;

        char *pos = stripped;
        char *pipe = strchr(pos, '|');
        if (pipe) {
            strncpy(entity_id, pos, pipe - pos);
            entity_id[pipe - pos] = '\0';
            for (int i = strlen(entity_id) - 1; i >= 0 &&
                 (entity_id[i] == ' ' || entity_id[i] == '\t'); i--)
                entity_id[i] = '\0';
        }

        // Find x and y values
        pos = stripped;
        while ((pos = strstr(pos, "x=")) != NULL) {
            if (sscanf(pos, "x=%d", &x_val) == 1) break;
            pos++;
        }
        pos = stripped;
        while ((pos = strstr(pos, "y=")) != NULL) {
            if (sscanf(pos, "y=%d", &y_val) == 1) break;
            pos++;
        }

        log_debug(log_file, "Entity moved: %s at (%d, %d)", entity_id, x_val, y_val);

        // Check triggers
        for (int i = 0; i < trigger_count; i++) {
            if (strcmp(triggers[i].trigger_name, "entity_moved") == 0) {
                log_debug(log_file, "  -> Would call: %s(%s, %d, %d)",
                          triggers[i].event_func, entity_id, x_val, y_val);
            } else if (strcmp(triggers[i].trigger_name, "zone_enter") == 0) {
                // Zone bounds: x∈[140,160], y∈[190,210]
                if (x_val >= 140 && x_val <= 160 && y_val >= 190 && y_val <= 210) {
                    log_debug(log_file, "  -> Zone trigger! %s(%s, zone_1)",
                              triggers[i].event_func, entity_id);
                }
            }
        }

        current_line = line_num;
    }
    fclose(fp);
    return current_line;
}

void detect_position_changes(const char *entities_file, const char *anim_queue_file, const char *log_file) {
    FILE *fp = fopen(entities_file, "r");
    if (!fp) return;

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), fp)) {
        char *stripped = line;
        while (*stripped == ' ' || *stripped == '\t') stripped++;
        if (*stripped == '\0' || *stripped == '\n') continue;

        // Parse: entity_id | x=N | y=N | [prev_x=N | prev_y=N | ...]
        char entity_id[256] = {0};
        int current_x = 0, current_y = 0;
        int prev_x = 0, prev_y = 0;

        char *pos = stripped;
        char *pipe = strchr(pos, '|');
        if (pipe) {
            strncpy(entity_id, pos, pipe - pos);
            entity_id[pipe - pos] = '\0';
            for (int i = strlen(entity_id) - 1; i >= 0 &&
                 (entity_id[i] == ' ' || entity_id[i] == '\t'); i--)
                entity_id[i] = '\0';
        }

        // Find current position
        pos = stripped;
        while ((pos = strstr(pos, "x=")) != NULL) {
            if (sscanf(pos, "x=%d", &current_x) == 1) break;
            pos++;
        }
        pos = stripped;
        while ((pos = strstr(pos, "y=")) != NULL) {
            if (sscanf(pos, "y=%d", &current_y) == 1) break;
            pos++;
        }

        // Check if this entity has a prev_x/prev_y (indicates a position change detected)
        pos = stripped;
        while ((pos = strstr(pos, "prev_x=")) != NULL) {
            if (sscanf(pos, "prev_x=%d", &prev_x) == 1) break;
            pos++;
        }

        // If no prev_x, this is a new or unchanged entity - store current as previous for next tick
        if (prev_x == 0 && prev_y == 0) {
            // Create marker file to track this entity's last known position
            char pos_marker[MAX_PATH];
            snprintf(pos_marker, sizeof(pos_marker), "%s/../.positions/%s",
                     anim_queue_file, entity_id);

            // Don't create marker yet - just note that we saw this position
            continue;
        }

        // If prev_x/prev_y exist and differ from current, position changed!
        if (prev_x != current_x || prev_y != current_y) {
            log_debug(log_file, "Position change detected: %s (%d,%d) -> (%d,%d)",
                      entity_id, prev_x, prev_y, current_x, current_y);

            // Queue animation for this position change
            FILE *anim_fp = fopen(anim_queue_file, "a");
            if (anim_fp) {
                fprintf(anim_fp, "%s | x=%d | y=%d | target_x=%d | target_y=%d\n",
                        entity_id, prev_x, prev_y, current_x, current_y);
                fclose(anim_fp);
            }
        }
    }
    fclose(fp);
}

int poll_animations(const char *anim_file, const char *log_file, int start_line) {
    FILE *fp = fopen(anim_file, "r");
    if (!fp) return start_line;

    char line[MAX_LINE];
    int line_num = 0;
    int current_line = start_line;

    while (fgets(line, sizeof(line), fp)) {
        line_num++;
        if (line_num <= start_line) continue;

        char *stripped = line;
        while (*stripped == ' ' || *stripped == '\t') stripped++;
        if (*stripped == '\0' || *stripped == '\n') continue;

        // Parse animation line: "entity_id | x=N | y=N | target_x=N | target_y=N"
        char entity_id[256] = {0};
        int target_x = 0, target_y = 0;

        char *pos = stripped;
        char *pipe = strchr(pos, '|');
        if (pipe) {
            strncpy(entity_id, pos, pipe - pos);
            entity_id[pipe - pos] = '\0';
            for (int i = strlen(entity_id) - 1; i >= 0 &&
                 (entity_id[i] == ' ' || entity_id[i] == '\t'); i--)
                entity_id[i] = '\0';
        }

        // Find target positions
        pos = stripped;
        while ((pos = strstr(pos, "target_x=")) != NULL) {
            if (sscanf(pos, "target_x=%d", &target_x) == 1) break;
            pos++;
        }
        pos = stripped;
        while ((pos = strstr(pos, "target_y=")) != NULL) {
            if (sscanf(pos, "target_y=%d", &target_y) == 1) break;
            pos++;
        }

        log_debug(log_file, "Animation queued: %s -> (%d, %d)", entity_id, target_x, target_y);

        current_line = line_num;
    }
    fclose(fp);
    return current_line;
}

int main(int argc, char *argv[]) {
    // Derive page_root from binary path
    char script_path[MAX_PATH];
    if (argc > 0) {
        realpath(argv[0], script_path);
    } else {
        strcpy(script_path, "./ops/page_manager_tick");
    }

    char *ops_dir = dirname(script_path);
    char page_root[MAX_PATH]; char *last_slash, *second_last;
    snprintf(page_root, sizeof(page_root), "%s/..", ops_dir);

    char normalized[MAX_PATH];
    realpath(page_root, normalized);
    strcpy(page_root, normalized);

    // Derive house_root (parent of pages dir)
    char *pages_ptr = strstr(page_root, "/pages");
    char house_root[MAX_PATH];
    if (pages_ptr) {
        strncpy(house_root, page_root, pages_ptr - page_root);
        house_root[pages_ptr - page_root] = '\0';
    } else {
        strcpy(house_root, page_root);
    }

    char state_dir[MAX_PATH];
    snprintf(state_dir, sizeof(state_dir), "%s/state", page_root);

    char log_file[MAX_PATH];
    snprintf(log_file, sizeof(log_file), "%s/page_manager.log", state_dir);

    char cursor_file[MAX_PATH];
    snprintf(cursor_file, sizeof(cursor_file), "%s/page_manager.cursor", state_dir);

    char entities_file[MAX_PATH];
    snprintf(entities_file, sizeof(entities_file), "%s/entities_live.txt", state_dir);

    char anim_file[MAX_PATH];
    snprintf(anim_file, sizeof(anim_file), "%s/animation_queue.txt", state_dir);

    char prev_state_file[MAX_PATH];
    snprintf(prev_state_file, sizeof(prev_state_file), "%s/.prev_entity_state", state_dir);

    char trigger_file[MAX_PATH];
    snprintf(trigger_file, sizeof(trigger_file), "%s/event_pkg/event_triggers.pdl", page_root);

    // Sync entity positions from desktop_pos.txt to entities_live.txt (master ledger)
    char sync_cmd[MAX_PATH];
    snprintf(sync_cmd, sizeof(sync_cmd), "'%s/ops/sync_entity_positions' >/dev/null 2>&1", page_root);
    system(sync_cmd);

    // Auto-detect position changes in entities_live.txt
    typedef struct {
        char entity_id[256];
        int x, y;
    } EntityState;

    EntityState prev_states[MAX_TRIGGERS];
    int prev_count = 0;

    FILE *prev_fp = fopen(prev_state_file, "r");
    if (prev_fp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), prev_fp) && prev_count < MAX_TRIGGERS) {
            if (sscanf(line, "%255s %d %d", prev_states[prev_count].entity_id,
                      &prev_states[prev_count].x, &prev_states[prev_count].y) == 3) {
                prev_count++;
            }
        }
        fclose(prev_fp);
    }

    // Save current entities and detect changes
    FILE *new_state_fp = fopen(prev_state_file, "w");
    FILE *curr_ent_fp = fopen(entities_file, "r");
    if (curr_ent_fp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), curr_ent_fp)) {
            char *stripped = line;
            while (*stripped == ' ' || *stripped == '\t') stripped++;
            if (*stripped == '\0' || *stripped == '\n') continue;

            char entity_id[256] = {0};
            int current_x = 0, current_y = 0;

            char *pos = stripped;
            char *pipe = strchr(pos, '|');
            if (pipe) {
                strncpy(entity_id, pos, pipe - pos);
                entity_id[pipe - pos] = '\0';
                for (int i = strlen(entity_id) - 1; i >= 0 &&
                     (entity_id[i] == ' ' || entity_id[i] == '\t'); i--)
                    entity_id[i] = '\0';
            }

            pos = stripped;
            while ((pos = strstr(pos, "x=")) != NULL) {
                if (sscanf(pos, "x=%d", &current_x) == 1) break;
                pos++;
            }
            pos = stripped;
            while ((pos = strstr(pos, "y=")) != NULL) {
                if (sscanf(pos, "y=%d", &current_y) == 1) break;
                pos++;
            }

            // Save current state
            if (new_state_fp) {
                fprintf(new_state_fp, "%s %d %d\n", entity_id, current_x, current_y);
            }

            // Detect position change
            int prev_x = -1, prev_y = -1;
            for (int i = 0; i < prev_count; i++) {
                if (strcmp(prev_states[i].entity_id, entity_id) == 0) {
                    prev_x = prev_states[i].x;
                    prev_y = prev_states[i].y;
                    break;
                }
            }

            // Queue animation if position changed
            if (prev_x != -1 && (prev_x != current_x || prev_y != current_y)) {
                log_debug(log_file, "Position change: %s (%d,%d) -> (%d,%d)",
                          entity_id, prev_x, prev_y, current_x, current_y);

                FILE *anim_fp = fopen(anim_file, "a");
                if (anim_fp) {
                    fprintf(anim_fp, "%s | x=%d | y=%d | target_x=%d | target_y=%d\n",
                            entity_id, prev_x, prev_y, current_x, current_y);
                    fclose(anim_fp);
                }
            }
        }
        fclose(curr_ent_fp);
    }
    if (new_state_fp) fclose(new_state_fp);

    // Read current cursors
    int entities_cursor, events_cursor, anim_cursor;
    read_cursors(cursor_file, &entities_cursor, &events_cursor, &anim_cursor);

    // Load trigger table
    Trigger triggers[MAX_TRIGGERS];
    int trigger_count = load_trigger_table(trigger_file, triggers);

    // Poll entities
    entities_cursor = poll_entities(entities_file, log_file, page_root, house_root,
                                     triggers, trigger_count, entities_cursor);

    // Poll animations
    anim_cursor = poll_animations(anim_file, log_file, anim_cursor);

    // Write updated cursors
    write_cursors(cursor_file, entities_cursor, events_cursor, anim_cursor);

    return 0;
}
