/* common_events_manager.c - persistent manager for Autorun/Parallel common events
 *
 * Task 4 implementation: a dedicated polling process that checks every common
 * event's configured trigger type and fires them at the appropriate time.
 *
 * Design:
 *   - One polling thread with ~60Hz tick rate (matching game_manager.c pattern)
 *   - Scans house_root/common_events/ for all event directories
 *   - For each event, reads condition.pdl to determine trigger type (None/on-click/Autorun/Parallel)
 *   - Autorun: fires exactly once when switch transitions OFF→ON (edge-triggered)
 *   - Parallel: fires repeatedly while switch is ON, gated by 1-second cooldown per event
 *   - Sole writer to common_events/.manager_ledger.txt (timestamps all firings)
 *   - Uses existing call_event_op.c mechanism to actually run events
 *
 * Usage: common_events_manager.+x <house_root>
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <signal.h>
#include <dirent.h>
#include <stdarg.h>

#define MAX_LINE 4096
#define MAX_EVENTS 32
#define MAX_PATH 4096
#define POLL_INTERVAL 16667  /* 16ms = ~60fps, x0.moke standard */
#define PARALLEL_COOLDOWN 1000000  /* 1 second between re-executions of same Parallel event */

volatile int running = 1;

void signal_handler(int sig) {
    (void)sig;
    running = 0;
}

void log_mgr(const char *fmt, ...) {
    FILE *f = fopen("/tmp/common_events_manager.log", "a");
    if (f) {
        va_list args;
        va_start(args, fmt);
        fprintf(f, "[%ld] ", time(NULL));
        vfprintf(f, fmt, args);
        fprintf(f, "\n");
        va_end(args);
        fclose(f);
    }
}

/* Event state tracking */
typedef struct {
    char name[128];                      /* common event directory name */
    char trigger[64];                    /* trigger type: None/on-click/Autorun/Parallel */
    char switch_name[128];               /* switch name to monitor (user-configured, or falls back to ce_<name>) */
    int last_switch_state;               /* for edge-detection on Autorun */
    time_t last_parallel_exec_time;      /* for cooldown gating on Parallel */
} EventState;

static EventState g_events[MAX_EVENTS];
static int g_n_events = 0;
static char g_house_root[MAX_PATH];

/* Record a common event firing in the ledger */
static void record_ledger_entry(const char *event_name, const char *trigger_type) {
    char ledger_path[MAX_PATH];
    snprintf(ledger_path, sizeof(ledger_path), "%s/common_events/.manager_ledger.txt", g_house_root);

    FILE *f = fopen(ledger_path, "a");
    if (f) {
        time_t now = time(NULL);
        fprintf(f, "[%ld] FIRED: %s (trigger=%s)\n", now, event_name, trigger_type);
        fclose(f);
    }
    log_mgr("Ledger: %s fired (trigger=%s)", event_name, trigger_type);
}

/* Read current switch value: flat key=value format at sessions/<session_id>/switches.txt */
static int read_switch_value(const char *switch_name) {
    /* Determine current session directory */
    char session_path[MAX_PATH];
    DIR *sessions_dir = opendir(g_house_root);
    char active_session[64] = "";

    if (sessions_dir) {
        struct dirent *de;
        /* Simple heuristic: look for sessions/ subdirectory, then read active marker */
        while ((de = readdir(sessions_dir))) {
            if (strcmp(de->d_name, "sessions") == 0) {
                snprintf(session_path, sizeof(session_path), "%s/sessions", g_house_root);
                break;
            }
        }
        closedir(sessions_dir);
    }

    /* If no sessions dir, try checking for a marker file indicating current session */
    if (active_session[0] == '\0') {
        FILE *marker = fopen("/tmp/common_events_active_session.txt", "r");
        if (marker) {
            if (fgets(active_session, sizeof(active_session), marker)) {
                active_session[strcspn(active_session, "\r\n")] = '\0';
            }
            fclose(marker);
        }
    }

    /* If we found a session, build the path to switches.txt */
    if (active_session[0] != '\0') {
        char switches_file[MAX_PATH];
        snprintf(switches_file, sizeof(switches_file), "%s/sessions/%s/switches.txt",
                 g_house_root, active_session);

        FILE *f = fopen(switches_file, "r");
        if (f) {
            char line[256];
            while (fgets(line, sizeof(line), f)) {
                char key[128], val_str[128];
                if (sscanf(line, "%[^=]=%s", key, val_str) == 2) {
                    if (strcmp(key, switch_name) == 0) {
                        fclose(f);
                        return atoi(val_str);
                    }
                }
            }
            fclose(f);
        }
    }

    /* Fallback: switches at house root level (simpler for testing) */
    char fallback_path[MAX_PATH];
    snprintf(fallback_path, sizeof(fallback_path), "%s/switches.txt", g_house_root);

    FILE *f = fopen(fallback_path, "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            char key[128], val_str[128];
            if (sscanf(line, "%[^=]=%s", key, val_str) == 2) {
                if (strcmp(key, switch_name) == 0) {
                    fclose(f);
                    return atoi(val_str);
                }
            }
        }
        fclose(f);
    }

    return 0;  /* default to OFF if switch doesn't exist */
}

/* Execute a common event via prisc+x */
static int execute_common_event(const char *event_name) {
    char event_pal[MAX_PATH];
    snprintf(event_pal, sizeof(event_pal), "%s/common_events/%s/event_pkg/pages/page_1/event.pal",
             g_house_root, event_name);

    if (access(event_pal, F_OK) != 0) {
        log_mgr("Event file not found: %s", event_pal);
        return -1;
    }

    /* Find prisc+x binary relative to house root */
    char prisc_path[MAX_PATH];
    snprintf(prisc_path, sizeof(prisc_path), "%s/101.mutaclsym🧟‍♂️️+18.0G/system/prisc+x",
             g_house_root);

    if (access(prisc_path, X_OK) != 0) {
        log_mgr("prisc+x not found at %s", prisc_path);
        return -1;
    }

    /* Fork and exec prisc+x with the event.pal as argument */
    pid_t pid = fork();
    if (pid == 0) {
        /* Child process */
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);

        /* Set MUCHI_CALLER_PKG to the common event's package directory */
        char ce_pkg[MAX_PATH];
        snprintf(ce_pkg, sizeof(ce_pkg), "%s/common_events/%s/event_pkg",
                 g_house_root, event_name);
        setenv("MUCHI_CALLER_PKG", ce_pkg, 1);

        execl(prisc_path, prisc_path, event_pal, NULL);
        _exit(1);
    } else if (pid > 0) {
        /* Parent: wait for child */
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }

    return -1;
}

/* Load all common events from disk and their current trigger states */
static void load_common_events(void) {
    char ce_root[MAX_PATH];
    snprintf(ce_root, sizeof(ce_root), "%s/common_events", g_house_root);

    g_n_events = 0;
    DIR *d = opendir(ce_root);
    if (!d) {
        log_mgr("Could not open common_events directory: %s", ce_root);
        return;
    }

    struct dirent *de;
    while ((de = readdir(d)) && g_n_events < MAX_EVENTS) {
        if (de->d_name[0] == '.') continue;  /* skip . and .. */

        /* Check if it's a directory */
        char ce_path[MAX_PATH];
        snprintf(ce_path, sizeof(ce_path), "%s/%s", ce_root, de->d_name);
        struct stat st;
        if (stat(ce_path, &st) != 0 || !S_ISDIR(st.st_mode)) continue;

        /* Found a common event directory - read its trigger type */
        EventState *e = &g_events[g_n_events];
        snprintf(e->name, sizeof(e->name), "%s", de->d_name);
        snprintf(e->trigger, sizeof(e->trigger), "on-click");  /* default */
        e->last_switch_state = 0;
        e->last_parallel_exec_time = 0;

        /* Read condition.pdl to get trigger type and switch name */
        char condition_path[MAX_PATH];
        snprintf(condition_path, sizeof(condition_path), "%s/%s/event_pkg/pages/page_1/condition.pdl",
                 ce_root, de->d_name);

        /* Default switch name: ce_<event_name> */
        snprintf(e->switch_name, sizeof(e->switch_name), "ce_%s", e->name);

        FILE *f = fopen(condition_path, "r");
        if (f) {
            char line[256];
            while (fgets(line, sizeof(line), f)) {
                /* Format: COND | key | <value> */
                char *pipe1 = strchr(line, '|');
                if (pipe1) {
                    char *pipe2 = strchr(pipe1 + 1, '|');
                    if (pipe2) {
                        char key[64], val[64];
                        /* Extract key (between pipes) */
                        sscanf(pipe1 + 1, "%[^|]", key);
                        /* Extract value (after second pipe), trim whitespace */
                        sscanf(pipe2 + 1, " %s", val);

                        if (strstr(key, "trigger")) {
                            snprintf(e->trigger, sizeof(e->trigger), "%s", val);
                        } else if (strstr(key, "switch")) {
                            /* User-configured switch name overrides the default ce_<name> */
                            snprintf(e->switch_name, sizeof(e->switch_name), "%s", val);
                        }
                    }
                }
            }
            fclose(f);
        }

        log_mgr("Loaded event: %s, trigger=%s, switch=%s", e->name, e->trigger, e->switch_name);
        g_n_events++;
    }
    closedir(d);
}

/* Main event-checking loop - called once per poll tick */
static void check_common_events(void) {
    /* Reload events list periodically (every ~10 seconds at 60Hz = ~600 ticks) */
    static int tick_count = 0;
    if (tick_count++ % 600 == 0) {
        load_common_events();
    }

    time_t now = time(NULL);

    for (int i = 0; i < g_n_events; i++) {
        EventState *e = &g_events[i];

        /* Skip events with no automatic trigger */
        if (strcmp(e->trigger, "on-click") == 0 || strcmp(e->trigger, "None") == 0) {
            continue;
        }

        /* Use the configured switch name (or default ce_<name> if not set) */
        int current_switch_state = read_switch_value(e->switch_name);

        if (strcmp(e->trigger, "Autorun") == 0) {
            /* Autorun: edge-triggered, fires exactly once when switch goes 0→1 */
            if (e->last_switch_state == 0 && current_switch_state == 1) {
                log_mgr("Autorun edge detected: %s switch %s went 0→1", e->name, e->switch_name);
                execute_common_event(e->name);
                record_ledger_entry(e->name, "Autorun");
            }
        } else if (strcmp(e->trigger, "Parallel") == 0) {
            /* Parallel: fires repeatedly while switch is ON, gated by cooldown */
            if (current_switch_state == 1) {
                /* Check if enough time has passed since last execution */
                time_t time_since_last = (now - e->last_parallel_exec_time) * 1000000;
                if (time_since_last >= PARALLEL_COOLDOWN || e->last_parallel_exec_time == 0) {
                    log_mgr("Parallel fire (cooldown gate passed): %s", e->name);
                    execute_common_event(e->name);
                    record_ledger_entry(e->name, "Parallel");
                    e->last_parallel_exec_time = now;
                }
            }
        }

        /* Update state for next tick */
        e->last_switch_state = current_switch_state;
    }
}

void *polling_thread(void *arg) {
    (void)arg;
    log_mgr("Polling thread started");
    while (running) {
        check_common_events();
        usleep(POLL_INTERVAL);
    }
    log_mgr("Polling thread exiting");
    return NULL;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: common_events_manager.+x <house_root>\n");
        return 1;
    }

    snprintf(g_house_root, sizeof(g_house_root), "%s", argv[1]);

    log_mgr("=== Common Events Manager Started (house_root=%s) ===", g_house_root);

    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    /* Ensure ledger directory exists */
    char ce_root[MAX_PATH];
    snprintf(ce_root, sizeof(ce_root), "%s/common_events", g_house_root);
    mkdir(ce_root, 0755);

    /* Initial load of events */
    load_common_events();

    pthread_t poll_tid;
    if (pthread_create(&poll_tid, NULL, polling_thread, NULL) != 0) {
        log_mgr("ERROR: pthread_create failed");
        return 1;
    }

    log_mgr("Polling thread created, %d events loaded", g_n_events);

    /* Main thread waits for signal */
    while (running) {
        sleep(1);
    }

    pthread_join(poll_tid, NULL);
    log_mgr("=== Common Events Manager Stopped ===");
    return 0;
}
