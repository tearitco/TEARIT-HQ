#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <glob.h>
#include <stdarg.h>

#define MAX_LINE 1024
#define MAX_TRIGGERS 16
// REAL FIX 2026-09-28 (direct instruction: "there will be much more
// entities in the future so we need some sort of master ledger
// trunking strategy early"): the entity-tracking table below was
// silently reusing MAX_TRIGGERS (16, a real cap on trigger DEFINITIONS
// in event_triggers.pdl, unrelated in meaning) as its own capacity -
// already too small the moment this was checked: entities_live.txt
// has 36 real rows live right now, meaning position-change detection
// was already silently dropping the tail past the 16th entity, today,
// not just "in the future." Separate, real, own constant.
#define MAX_ENTITIES 512
// REAL FIX 2026-09-28 (cpu_loop_analysis.txt): this was 2048. glibc's
// FORTIFY_SOURCE hardens realpath() with a compile-time check that the
// destination buffer is >= PATH_MAX (4096) - NOT based on the actual
// resolved path length, so any build with fortify enabled (Ubuntu's
// default hardening on many toolchains) made realpath(argv[0],
// script_path) below abort with "*** buffer overflow detected ***"
// on EVERY SINGLE invocation, regardless of how short the real path
// was (147 bytes, confirmed live). Combined with the separate
// world_manager.pal `sleep 16`-microseconds bug (same date), this
// binary was crashing and being apport-caught thousands of times a
// second - the real mechanism behind the sustained CPU/throttling
// reports, confirmed via `gdb -batch -ex run -ex bt` backtrace
// (__realpath_chk -> __chk_fail -> abort). Must be >= PATH_MAX.
#define MAX_PATH 4096

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

// REAL FIX 2026-09-28 (bug_bounty.md "world_manager sustained CPU
// throttling" entry, direct instruction: "we need some sort of master
// ledger trunking strategy early"). PUSH-model consumer half: reads
// only the NEW lines appended to entities_live.ledger since the given
// cursor (move_entity_tick.c is the producer, appending its own move
// the moment it happens) and merges them into entities_live.txt.
// O(moves since last tick) to read, O(tracked entities) only to
// rewrite - and does nothing at all, not even an fopen of
// entities_live.txt, when nothing moved. Replaces the old PULL model
// (fork sync_entity_positions.+x -> recursive `find` over the whole
// xyzfs/users tree + per-entity forks, EVERY time it ran) which got
// slower as entities were added regardless of how many actually moved
// - the real scaling problem the direct instruction flagged, distinct
// from the crash-loop bug fixed earlier the same day. Returns the new
// cursor (highest ledger line number consumed).
int apply_ledger_deltas(const char *ledger_file, const char *entities_file,
                         const char *log_file, int start_line) {
    FILE *lf = fopen(ledger_file, "r");
    if (!lf) return start_line;

    typedef struct { char id[256]; int x, y; } Delta;
    static Delta deltas[MAX_ENTITIES];
    int n_deltas = 0;

    char line[MAX_LINE];
    int line_num = 0;
    int current_line = start_line;
    while (fgets(line, sizeof(line), lf)) {
        line_num++;
        if (line_num <= start_line) continue;
        current_line = line_num;

        char *pipe1 = strchr(line, '|');
        if (!pipe1) continue;
        char id[256] = {0};
        int idlen = (int)(pipe1 - line);
        if (idlen >= (int)sizeof(id)) idlen = sizeof(id) - 1;
        if (idlen > 0) strncpy(id, line, idlen);
        id[idlen > 0 ? idlen : 0] = '\0';
        for (int i = (int)strlen(id) - 1; i >= 0 && (id[i] == ' ' || id[i] == '\t'); i--) id[i] = '\0';
        if (id[0] == '\0') continue;

        int x = 0, y = 0;
        char *xp = strstr(pipe1, "x=");
        if (xp) sscanf(xp, "x=%d", &x);
        char *yp = strstr(pipe1, "y=");
        if (yp) sscanf(yp, "y=%d", &y);

        int found = -1;
        for (int i = 0; i < n_deltas; i++) {
            if (strcmp(deltas[i].id, id) == 0) { found = i; break; }
        }
        if (found < 0 && n_deltas < MAX_ENTITIES) {
            found = n_deltas++;
            snprintf(deltas[found].id, sizeof(deltas[found].id), "%s", id);
        }
        if (found >= 0) { deltas[found].x = x; deltas[found].y = y; }
    }
    fclose(lf);

    if (n_deltas == 0) return current_line;

    // Merge: load existing entities_live.txt rows, apply matching
    // deltas in place, append any brand-new entity IDs.
    typedef struct { char id[256]; int x, y; } Row;
    static Row rows[MAX_ENTITIES];
    int n_rows = 0;

    FILE *ef = fopen(entities_file, "r");
    if (ef) {
        char eline[MAX_LINE];
        while (fgets(eline, sizeof(eline), ef) && n_rows < MAX_ENTITIES) {
            char *stripped = eline;
            while (*stripped == ' ' || *stripped == '\t') stripped++;
            if (*stripped == '\0' || *stripped == '\n') continue;
            char *pipe = strchr(stripped, '|');
            if (!pipe) continue;
            char id[256] = {0};
            int idlen = (int)(pipe - stripped);
            if (idlen >= (int)sizeof(id)) idlen = sizeof(id) - 1;
            if (idlen > 0) strncpy(id, stripped, idlen);
            id[idlen > 0 ? idlen : 0] = '\0';
            for (int i = (int)strlen(id) - 1; i >= 0 && (id[i] == ' ' || id[i] == '\t'); i--) id[i] = '\0';
            if (id[0] == '\0') continue;
            int x = 0, y = 0;
            char *xp = strstr(stripped, "x=");
            if (xp) sscanf(xp, "x=%d", &x);
            char *yp = strstr(stripped, "y=");
            if (yp) sscanf(yp, "y=%d", &y);
            snprintf(rows[n_rows].id, sizeof(rows[n_rows].id), "%s", id);
            rows[n_rows].x = x;
            rows[n_rows].y = y;
            n_rows++;
        }
        fclose(ef);
    }

    for (int d = 0; d < n_deltas; d++) {
        int found = -1;
        for (int i = 0; i < n_rows; i++) {
            if (strcmp(rows[i].id, deltas[d].id) == 0) { found = i; break; }
        }
        if (found < 0 && n_rows < MAX_ENTITIES) {
            found = n_rows++;
            snprintf(rows[found].id, sizeof(rows[found].id), "%s", deltas[d].id);
        }
        if (found >= 0) { rows[found].x = deltas[d].x; rows[found].y = deltas[d].y; }
    }

    FILE *wf = fopen(entities_file, "w");
    if (wf) {
        for (int i = 0; i < n_rows; i++) {
            fprintf(wf, "%s | x=%d | y=%d\n", rows[i].id, rows[i].x, rows[i].y);
        }
        fclose(wf);
        log_debug(log_file, "Ledger merge: %d delta(s) applied, %d total entities", n_deltas, n_rows);
    }

    return current_line;
}

int main(int argc, char *argv[]) {
    // Derive world_root from binary path
    // script_path: ...&.hq-apps/world-manager/ops/world_manager_tick
    // We need: ...&.hq-apps/world-manager/
    // REAL FIX 2026-09-28 (bug_bounty.md, direct instruction: "we
    // rather use malloc, and sizeof, but remember, linux automatically
    // frees memory, so double free could occur"): realpath(path, NULL)
    // is a glibc extension that MALLOCs exactly the resolved length
    // itself - no fixed destination buffer to guess a size for, ever
    // (a fixed buffer under PATH_MAX was the actual crash-loop bug
    // fixed this same date - see world_manager.pal's history and this
    // file's earlier FORTIFY_SOURCE fix note). Each resolved pointer
    // is read exactly once (the snprintf copy into the existing fixed
    // script_path/page_root buffers below - still safe at MAX_PATH,
    // every real house path is well under it) and freed immediately
    // after that one read, in the same scope, no branch - matches
    // TPMOS house precedent (`1.TPMOS.../#.docs/^.pmo.ld-faq+8/
    // PITFALLS_ACTIVE_2026-03-18.txt` #20: "never free path buffers
    // before I/O operations complete" - freeing right after the only
    // read, never before it, and never a second time on any path,
    // is what actually avoids the double-free/use-after-free class the
    // instruction flagged).
    char script_path[MAX_PATH];
    if (argc > 0) {
        char *resolved = realpath(argv[0], NULL);
        if (resolved) {
            snprintf(script_path, sizeof(script_path), "%s", resolved);
            free(resolved);
        } else {
            strcpy(script_path, "./ops/world_manager_tick");
        }
    } else {
        strcpy(script_path, "./ops/world_manager_tick");
    }

    char *last_slash = strrchr(script_path, '/');
    if (last_slash) {
        *last_slash = '\0';  // Remove /world_manager_tick
        char *second_last = strrchr(script_path, '/');
        if (second_last) {
            *second_last = '\0';  // Remove /ops
        }
    }

    char page_root[MAX_PATH];
    strcpy(page_root, script_path);  // page_root is world-manager dir

    {
        char *resolved = realpath(page_root, NULL);
        if (resolved) {
            snprintf(page_root, sizeof(page_root), "%s", resolved);
            free(resolved);
        }
    }

    // Derive house_root (parent of hq-apps dir)
    char *hqapps_ptr = strstr(page_root, "/&.hq-apps");
    char house_root[MAX_PATH];
    if (hqapps_ptr) {
        strncpy(house_root, page_root, hqapps_ptr - page_root);
        house_root[hqapps_ptr - page_root] = '\0';
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
    //
    // HISTORY: this used to fork sync_entity_positions.+x (recursive
    // `find` over the whole xyzfs/users tree + per-entity sed/grep
    // forks) every tick, throttled to 1/sec after a 2026-09-28 crash-
    // loop fix - see bug_bounty.md. That throttle was correct for the
    // COST-PER-RUN problem it targeted, but the underlying model still
    // did O(all entities) work on every run regardless of how many
    // actually moved, and got slower as more entities were added -
    // the SEPARATE scaling problem the same date's follow-up direct
    // instruction flagged: "there will be much more entities in the
    // future so we need some sort of master ledger trunking strategy
    // early."
    //
    // REAL FIX: PUSH model. move_entity_tick.c (entity-cli) now
    // appends its own move directly to entities_live.ledger the moment
    // it happens (producer owns the write - the same cursor-over-
    // append-only-ledger shape this file already uses for
    // entities_live.txt/world_events.txt/animation_queue.txt, extended
    // rather than reinvented). This block just reads forward from its
    // own cursor - O(moves since last tick), does literally nothing
    // (not even an fopen) on a tick where nothing moved, and never
    // gets slower as entity COUNT grows, only as entity ACTIVITY does.
    char ledger_file[MAX_PATH];
    snprintf(ledger_file, sizeof(ledger_file), "%s/entities_live.ledger", state_dir);
    char ledger_cursor_file[MAX_PATH];
    snprintf(ledger_cursor_file, sizeof(ledger_cursor_file), "%s/.ledger_cursor", state_dir);
    {
        int ledger_cursor = 0;
        FILE *lcf = fopen(ledger_cursor_file, "r");
        if (lcf) {
            if (fscanf(lcf, "%d", &ledger_cursor) != 1) ledger_cursor = 0;
            fclose(lcf);
        }

        int new_cursor = apply_ledger_deltas(ledger_file, entities_file, log_file, ledger_cursor);
        if (new_cursor != ledger_cursor) {
            FILE *wcf = fopen(ledger_cursor_file, "w");
            if (wcf) { fprintf(wcf, "%d\n", new_cursor); fclose(wcf); }
        }
    }

    // SELF-HEALING FALLBACK, deliberately slow (60s, not 1s): the
    // ledger above only catches moves that went through
    // move_entity_tick.c. Anything else that can change desktop_pos.txt
    // (a hand edit, a future mover that forgets to append, ledger
    // corruption) needs a periodic full reconciliation - this is that,
    // unchanged from the earlier fix except the interval, since the
    // ledger is now the fast path and this only needs to repair drift.
    // Truncates the ledger + resets its cursor right after each run,
    // since entities_live.txt is authoritative again at that point -
    // keeps entities_live.ledger from growing unbounded forever
    // (append-only files need a real reset point somewhere, same
    // principle as this file's own animation_queue.txt cursor).
    {
        #define SYNC_MIN_INTERVAL_SEC 60
        char sync_marker[MAX_PATH];
        snprintf(sync_marker, sizeof(sync_marker), "%s/.sync_last_run", state_dir);
        struct stat mst;
        time_t now = time(NULL);
        int due = (stat(sync_marker, &mst) != 0) || (now - mst.st_mtime >= SYNC_MIN_INTERVAL_SEC);
        if (due) {
            FILE *mf = fopen(sync_marker, "w");
            if (mf) fclose(mf);

            char sync_path[MAX_PATH];
            snprintf(sync_path, sizeof(sync_path), "%s/ops/sync_entity_positions", page_root);

            pid_t sync_pid = fork();
            if (sync_pid == 0) {
                // Child process
                execvp(sync_path, (char *[]) { sync_path, NULL });
                exit(1);  // execvp only returns on error
            } else if (sync_pid > 0) {
                // Parent process: wait for child
                int sync_status;
                waitpid(sync_pid, &sync_status, 0);
            }

            FILE *tf = fopen(ledger_file, "w");
            if (tf) fclose(tf);
            FILE *rcf = fopen(ledger_cursor_file, "w");
            if (rcf) { fprintf(rcf, "0\n"); fclose(rcf); }
        }
    }

    // Auto-detect position changes in entities_live.txt
    typedef struct {
        char entity_id[256];
        int x, y;
    } EntityState;

    EntityState prev_states[MAX_ENTITIES];
    int prev_count = 0;

    FILE *prev_fp = fopen(prev_state_file, "r");
    if (prev_fp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), prev_fp) && prev_count < MAX_ENTITIES) {
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
