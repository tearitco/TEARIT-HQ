/* econ_calendar - wsr-pal's TIME, ported from the original WSR's
 * `wsr_clock.c` + `presets/schedule.txt`.
 *
 * PORT FIDELITY - THIS IS THE POINT
 *   The original (MSR-DEPRACATED/wsr_clock.c) is the golden standard for
 *   time in this simulation, and this op is a port of it, not a redesign.
 *   Concretely, everything below was carried across from the original:
 *
 *   1. The `GameTime` struct exactly as declared there:
 *      year, month, day, hour, minute, second, centisecond - with the
 *      full cascade carry from wsr_clock.c:108-131 (centisecond ->
 *      second -> minute -> hour -> day -> month, with `day > 30` rolling
 *      the month and `month > 12` rolling the year, i.e. the original's
 *      30-day months, kept rather than "corrected" to real months).
 *   2. The persistence format: `key:value` with a COLON, in
 *      `data/wsr_clock.txt`. This deliberately does NOT use the `key=value`
 *      of the surrounding state.txt files, because it is a port and the
 *      original's clock file format is part of the thing being ported.
 *   3. Wall-clock-driven time, not turn-counted. The original advances by
 *      real elapsed milliseconds multiplied by a rate read from
 *      `data/setting.txt:ticker_speed` (wsp_clock.c:74-104). Time flows
 *      whether or not the player does anything.
 *   4. The same ticker_speed vocabulary and the same rate constants:
 *      cent=36000, sec=360, min=6, hour=0.1, day=0.004166666666666667
 *      game-centiseconds per real millisecond. These constants are carried
 *      over AS-IS, including the fact that the "day" speed is not actually
 *      a game day - see the speed table printed by `show`, which reports
 *      the true duration of each setting instead of leaving the next agent
 *      to discover it.
 *   5. The original's SIGINT/SIGTERM handler that saves state and exits
 *      cleanly, and its PID file (`data/wsr_clock.pid`).
 *   6. `presets/schedule.txt` - the interval -> command file, ported to
 *      `data/schedule.txt` with its original six rows and its original
 *      interval vocabulary (1_hour/1_day/3_months/1_year). This is the
 *      mechanism the current docs kept alluding to as a `3_months`
 *      schedule and which had never actually been built; it is real now.
 *
 * WHAT IS NOT A PORT
 *   The original's `*_loop` consumers are stubs - `tax_loop.c` and
 *   `hour_loop.c` are 19-line "Hello, ..." placeholders, and
 *   `dividend_loop.c` / `payroll_loop.c` are byte-identical wrong copies
 *   of each other (confirmed against SOCIETY-ECONOMY-ARCHITECTURE.txt:18
 *   and :454). So there is no working tax/payroll/dividend code to
 *   recover. The clock and the schedule mechanism are real and are what
 *   this port restores; the ops those rows point at remain TODO and are
 *   reported loudly when they are missing rather than skipped silently.
 *
 * KNOWN SEQUENCING CONSEQUENCE, NAMED NOT HIDDEN
 *   The rest of wsr-pal's economy is sequenced on the End Turn tick
 *   (`scripts/tick_all` -> `corp_apply_finances`, `corp_update_price`).
 *   The original clock is wall-clock driven. Those two cadences are
 *   genuinely different, so a `1_day` or `3_months` event can fire on a
 *   turn boundary that has not been recomputed yet. Fidelity was chosen
 *   over convenience here, deliberately: re-sequencing the finance pass
 *   onto the clock is a real change to the economy and is tracked as
 *   ROADMAP Phase 2.6, not smuggled in with the clock port.
 *
 * USAGE
 *   econ_calendar.+x run
 *       the clock DAEMON. Loads the clock, advances it against real
 *       elapsed time at the configured ticker_speed, runs any schedule row
 *       whose interval has come due, saves, and repeats. Catches
 *       SIGINT/SIGTERM to save and exit. This is the normal mode.
 *   econ_calendar.+x show
 *       print the current date/time and the true duration of each
 *       ticker_speed setting (no advance)
 *   econ_calendar.+x step <real_ms>
 *       advance by an explicit number of REAL milliseconds, as if that much
 *       wall time had passed. Deterministic - this is the testable path
 *       and the one the verification used.
 *   econ_calendar.+x due
 *       run every schedule row whose interval is due RIGHT NOW, without
 *       advancing time
 *   econ_calendar.+x set-speed <cent|sec|min|hour|day>
 *       write data/setting.txt:ticker_speed
 *   econ_calendar.+x schedule
 *       print the schedule and what is due
 *
 * Self-contained, no shared headers (matching the rest of ops/). */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
/* windows.h defines MAX_PATH as 260. This op builds path buffers out of
 * an arbitrarily long PRISC_PROJECT_ROOT, exactly like every other op in
 * this family, so the generous value is required - hence the #undef rather
 * than a rename. */
#undef MAX_PATH
#else
#include <unistd.h>
#endif

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_LINE 512

/* The original's month length (wsr_clock.c:120 `if (g_time.day > 30)`).
 * Kept deliberately: changing it to a real 28/30/31 calendar would be a
 * "fix" nobody asked for, and every interval threshold below depends on
 * it. */
#define DAYS_PER_MONTH 30
#define MONTHS_PER_YEAR 12

#define CENTIS_PER_SECOND 100
#define SECONDS_PER_MINUTE 60
#define MINUTES_PER_HOUR 60
#define HOURS_PER_DAY 24

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void data_path(char *out, size_t out_sz, const char *name) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(out, out_sz, "%s/projects/wsr-pal/data/%s", project_root, name);
#pragma GCC diagnostic pop
}

static int file_exists(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    fclose(f);
    return 1;
}

/* ------------------------------------------------------------------ */
/* GameTime - verbatim field set and cascade from wsr_clock.c          */
/* ------------------------------------------------------------------ */

typedef struct {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    int centisecond;
} GameTime;

static GameTime g_time;

/* wsr_clock.c:36-44 - the original's load default when the file is
 * missing, preserved exactly (2025, not 2026, not the current year). */
static void default_time(void) {
    g_time.year = 2025;
    g_time.month = 1;
    g_time.day = 1;
    g_time.hour = 0;
    g_time.minute = 0;
    g_time.second = 0;
    g_time.centisecond = 0;
}

static void load_clock_state(void) {
    char path[PATH_BUF];
    data_path(path, sizeof(path), "wsr_clock.txt");
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        default_time();
        return;
    }
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), fp)) {
        char *key = strtok(line, ":");
        char *value = strtok(NULL, "\n");
        if (key && value) {
            if (strcmp(key, "year") == 0) g_time.year = atoi(value);
            else if (strcmp(key, "month") == 0) g_time.month = atoi(value);
            else if (strcmp(key, "day") == 0) g_time.day = atoi(value);
            else if (strcmp(key, "hour") == 0) g_time.hour = atoi(value);
            else if (strcmp(key, "minute") == 0) g_time.minute = atoi(value);
            else if (strcmp(key, "second") == 0) g_time.second = atoi(value);
            else if (strcmp(key, "centisecond") == 0) g_time.centisecond = atoi(value);
        }
    }
    fclose(fp);
}

/* wsr_clock.c:62-71 - same field order, same colon format. */
static int save_clock_state(void) {
    char path[PATH_BUF];
    data_path(path, sizeof(path), "wsr_clock.txt");
    FILE *fp = fopen(path, "w");
    if (fp == NULL) {
        fprintf(stderr, "Error: Could not open data/wsr_clock.txt for writing.\n");
        return 0;
    }
    fprintf(fp, "year:%d\nmonth:%d\nday:%d\nhour:%d\nminute:%d\nsecond:%d\ncentisecond:%d\n",
            g_time.year, g_time.month, g_time.day, g_time.hour, g_time.minute,
            g_time.second, g_time.centisecond);
    return fclose(fp) == 0;
}

/* wsr_clock.c:108-131, the cascade carry, transcribed. The nesting is
 * deliberately identical to the original so the two can be diffed by eye. */
static void cascade_time(void) {
    while (g_time.centisecond >= 100) {
        g_time.centisecond -= 100;
        g_time.second++;
        if (g_time.second >= 60) {
            g_time.second = 0;
            g_time.minute++;
            if (g_time.minute >= 60) {
                g_time.minute = 0;
                g_time.hour++;
                if (g_time.hour >= 24) {
                    g_time.hour = 0;
                    g_time.day++;
                    if (g_time.day > 30) {
                        g_time.day = 1;
                        g_time.month++;
                        if (g_time.month > 12) {
                            g_time.month = 1;
                            g_time.year++;
                        }
                    }
                }
            }
        }
    }
}

/* Total game-centiseconds since the epoch. This is the one genuinely new
 * helper, and it exists only so the schedule can compare two GameTimes by
 * subtraction without re-implementing the cascade. It is a pure function
 * of the original's own field semantics, using its own 30-day months. */
static long long total_centiseconds(const GameTime *t) {
    long long c = 0;
    c += (long long)t->centisecond;
    c += (long long)t->second * CENTIS_PER_SECOND;
    c += (long long)t->minute * SECONDS_PER_MINUTE * CENTIS_PER_SECOND;
    c += (long long)t->hour * MINUTES_PER_HOUR * SECONDS_PER_MINUTE * CENTIS_PER_SECOND;
    c += (long long)t->day * HOURS_PER_DAY * MINUTES_PER_HOUR * SECONDS_PER_MINUTE * CENTIS_PER_SECOND;
    c += (long long)t->month * DAYS_PER_MONTH * HOURS_PER_DAY * MINUTES_PER_HOUR * SECONDS_PER_MINUTE * CENTIS_PER_SECOND;
    c += (long long)t->year * MONTHS_PER_YEAR * DAYS_PER_MONTH * HOURS_PER_DAY * MINUTES_PER_HOUR * SECONDS_PER_MINUTE * CENTIS_PER_SECOND;
    return c;
}

/* ------------------------------------------------------------------ */
/* ticker_speed - wsr_clock.c:73-106, same rates, same vocabulary       */
/* ------------------------------------------------------------------ */

/* The original's constants, carried over verbatim. See the header: the
 * "day" label does not mean a game day, and `show` reports the real
 * durations so this is discoverable rather than a trap. */
static double rate_for_speed(const char *speed_str) {
    if (strcmp(speed_str, "cent") == 0) return 36000.0;
    if (strcmp(speed_str, "sec") == 0) return 360.0;
    if (strcmp(speed_str, "min") == 0) return 6.0;
    if (strcmp(speed_str, "hour") == 0) return 0.1;
    if (strcmp(speed_str, "day") == 0) return 0.004166666666666667;
    return 0.0;
}

static void read_setting(char *out, size_t out_sz) {
    char path[PATH_BUF];
    data_path(path, sizeof(path), "setting.txt");
    snprintf(out, out_sz, "N/A");   /* wsr_clock.c:75 default */
    FILE *fp = fopen(path, "r");
    if (!fp) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), fp)) {
        char *key = strtok(line, ":");
        char *val = strtok(NULL, "\n");
        if (key && val && strcmp(key, "ticker_speed") == 0) {
            while (val[0] == ' ' || val[0] == '\t') val++;
            snprintf(out, out_sz, "%s", val);
            break;
        }
    }
    fclose(fp);
}

static int write_setting_speed(const char *speed) {
    char path[PATH_BUF];
    data_path(path, sizeof(path), "setting.txt");
    FILE *fp = fopen(path, "w");
    if (!fp) return 0;
    fprintf(fp, "ticker_speed: %s\n", speed);
    return fclose(fp) == 0;
}

/* wsr_clock.c:73-106. Returns the number of game-centiseconds elapsed for
 * the given real milliseconds, or 0 when the speed is unset/unknown. */
static long long game_centiseconds_for(long long elapsed_ms, const char *speed) {
    double rate = rate_for_speed(speed);
    if (rate == 0.0) return 0;
    return (long long)(elapsed_ms * rate);
}

/* ------------------------------------------------------------------ */
/* schedule.txt - the original's interval -> command mechanism          */
/* ------------------------------------------------------------------ */

typedef struct {
    char interval[24];
    char command[256];
    long long threshold;   /* game-centiseconds; 0 = unrecognised */
    int recognised;
} ScheduleRow;

static const ScheduleRow SCHEDULE_DEFAULTS[] = {
    /* Transcribed from MSR-DEPRACATED/presets/schedule.txt, in order. */
    { "1_hour",   "./ops/+x/news_loop.+x",    1LL * HOURS_PER_DAY * MINUTES_PER_HOUR * SECONDS_PER_MINUTE * CENTIS_PER_SECOND * 0 + 1LL * MINUTES_PER_HOUR * SECONDS_PER_MINUTE * CENTIS_PER_SECOND, 1 },
    { "1_day",    "./ops/+x/day_loop.+x",     1LL * HOURS_PER_DAY * MINUTES_PER_HOUR * SECONDS_PER_MINUTE * CENTIS_PER_SECOND, 1 },
    { "3_months", "./ops/+x/tax_loop.+x",     3LL * DAYS_PER_MONTH * HOURS_PER_DAY * MINUTES_PER_HOUR * SECONDS_PER_MINUTE * CENTIS_PER_SECOND, 1 },
    { "3_months", "./ops/+x/dividend_loop.+x", 3LL * DAYS_PER_MONTH * HOURS_PER_DAY * MINUTES_PER_HOUR * SECONDS_PER_MINUTE * CENTIS_PER_SECOND, 1 },
    { "3_months", "./ops/+x/payroll_loop.+x",  3LL * DAYS_PER_MONTH * HOURS_PER_DAY * MINUTES_PER_HOUR * SECONDS_PER_MINUTE * CENTIS_PER_SECOND, 1 },
    { "1_year",   "./ops/+x/salary_loop.+x",  1LL * MONTHS_PER_YEAR * DAYS_PER_MONTH * HOURS_PER_DAY * MINUTES_PER_HOUR * SECONDS_PER_MINUTE * CENTIS_PER_SECOND, 1 },
};
static const int N_SCHEDULE_DEFAULTS = (int)(sizeof(SCHEDULE_DEFAULTS) / sizeof(SCHEDULE_DEFAULTS[0]));

/* Interval name -> game-centiseconds. 1_hour and 1_day are spelled out
 * rather than derived from the array so an unrecognised row in a
 * hand-edited schedule file is reported instead of silently skipped. */
static long long threshold_for_interval(const char *name) {
    long long hour = 1LL * MINUTES_PER_HOUR * SECONDS_PER_MINUTE * CENTIS_PER_SECOND;
    long long day = 1LL * HOURS_PER_DAY * MINUTES_PER_HOUR * SECONDS_PER_MINUTE * CENTIS_PER_SECOND;
    long long month = 1LL * DAYS_PER_MONTH * HOURS_PER_DAY * MINUTES_PER_HOUR * SECONDS_PER_MINUTE * CENTIS_PER_SECOND;
    if (strcmp(name, "1_hour") == 0) return hour;
    if (strcmp(name, "1_day") == 0) return day;
    if (strcmp(name, "1_month") == 0) return month;
    if (strcmp(name, "3_months") == 0) return 3 * month;
    if (strcmp(name, "6_months") == 0) return 6 * month;
    if (strcmp(name, "1_year") == 0) return MONTHS_PER_YEAR * month;
    return 0;
}

static int load_schedule(ScheduleRow *rows, int max) {
    char path[PATH_BUF];
    data_path(path, sizeof(path), "schedule.txt");
    int n = 0;
    FILE *fp = fopen(path, "r");
    if (!fp) {
        for (int i = 0; i < N_SCHEDULE_DEFAULTS && i < max; i++) {
            rows[n] = SCHEDULE_DEFAULTS[i];
            n++;
        }
        return n;
    }
    char line[MAX_LINE];
    while (n < max && fgets(line, sizeof(line), fp)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\r' || *p == '\0') continue;
        p[strcspn(p, "\r\n")] = '\0';
        char *sp = strchr(p, ' ');
        if (!sp) continue;
        *sp = '\0';
        char *cmd = sp + 1;
        while (*cmd == ' ') cmd++;
        snprintf(rows[n].command, sizeof(rows[n].command), "%s", cmd);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(rows[n].interval, sizeof(rows[n].interval), "%s", p);
#pragma GCC diagnostic pop
        rows[n].threshold = threshold_for_interval(rows[n].interval);
        rows[n].recognised = (rows[n].threshold != 0);
        n++;
    }
    fclose(fp);
    return n;
}

/* Per-row last-run stamp, so "due" means "its interval has elapsed since
 * THIS row last ran" rather than "its interval has elapsed since the
 * epoch" (which would fire every row on the first tick).
 *
 * `*existed` is out-param, not decoration. A row with no stamp file has
 * never run, and the correct handling is to ARM it (stamp it with the
 * current time) and not run it yet - one skipped first tick, then normal
 * behaviour. Returning "now" as the fallback instead, without arming,
 * computes elapsed = 0 against a threshold that can never be reached;
 * and because the stamp is only written after a row actually runs, no stamp
 * file is ever created, so every row stays permanently not-due and the whole
 * schedule silently never fires. `*existed` is what breaks that deadlock. */
static void last_run_path(char *out, size_t out_sz, int index) {
    char name[64];
    snprintf(name, sizeof(name), "schedule_run.%d.txt", index);
    data_path(out, out_sz, name);
}

static GameTime read_last_run(int index, int *existed) {
    char path[PATH_BUF];
    last_run_path(path, sizeof(path), index);
    *existed = 0;
    FILE *f = fopen(path, "r");
    if (!f) return g_time;
    *existed = 1;
    GameTime t = g_time;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        char *key = strtok(line, ":");
        char *val = strtok(NULL, "\n");
        if (key && val) {
            if (strcmp(key, "year") == 0) t.year = atoi(val);
            else if (strcmp(key, "month") == 0) t.month = atoi(val);
            else if (strcmp(key, "day") == 0) t.day = atoi(val);
            else if (strcmp(key, "hour") == 0) t.hour = atoi(val);
            else if (strcmp(key, "minute") == 0) t.minute = atoi(val);
            else if (strcmp(key, "second") == 0) t.second = atoi(val);
            else if (strcmp(key, "centisecond") == 0) t.centisecond = atoi(val);
        }
    }
    fclose(f);
    return t;
}

static void write_last_run(int index) {
    char path[PATH_BUF];
    last_run_path(path, sizeof(path), index);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "year:%d\nmonth:%d\nday:%d\nhour:%d\nminute:%d\nsecond:%d\ncentisecond:%d\n",
            g_time.year, g_time.month, g_time.day, g_time.hour, g_time.minute,
            g_time.second, g_time.centisecond);
    fclose(f);
}

/* Resolves the op path for the current platform, mirroring how the rest
 * of this family calls ops: Linux `./ops/+x/x.+x`, Windows `ops\+x\x.+x`
 * under `cmd /c`. Reports an op whose binary is absent instead of
 * pretending it ran - the schedule's whole consumer set is still stubs. */
static void run_scheduled(const char *command, const char *interval) {
    /* Extract the op name from the command path. */
    const char *base = strrchr(command, '/');
    base = base ? base + 1 : command;
    char op_name[64];
    snprintf(op_name, sizeof(op_name), "%s", base);

    char bin[PATH_BUF], bin_exe[PATH_BUF];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(bin, sizeof(bin), "%s/ops/+x/%s", project_root, op_name);
    snprintf(bin_exe, sizeof(bin_exe), "%s.exe", bin);
#pragma GCC diagnostic pop

    if (!file_exists(bin) && !file_exists(bin_exe)) {
        printf("  [%s] SKIPPED %s: not built (original WSR shipped this as a stub)\n",
               interval, op_name);
        return;
    }

    char cmd[PATH_BUF * 2];
    fflush(stdout);
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "cmd /c \"cd /d \"%s\" && %s 2>&1\"", project_root, command);
#else
    snprintf(cmd, sizeof(cmd), "cd '%s' && %s 2>&1", project_root, command);
#endif
    printf("  [%s] RUN %s\n", interval, command);
    system(cmd);
}

/* Runs every row whose interval has elapsed since that row last ran. */
static void run_due_schedule(void) {
    ScheduleRow rows[64];
    int n = load_schedule(rows, 64);
    long long now = total_centiseconds(&g_time);

    for (int i = 0; i < n; i++) {
        if (!rows[i].recognised) {
            printf("  [%s] UNRECOGNISED interval in schedule.txt row %d: '%s' - not run\n",
                   rows[i].interval, i + 1, rows[i].command);
            continue;
        }
        int existed = 0;
        GameTime last = read_last_run(i, &existed);
        if (!existed) {
            /* First time this row has been seen. Arm it at the current time
             * so the interval is measured from now, and skip this tick -
             * otherwise every row would fire at once on the very first tick
             * of a fresh schedule. */
            write_last_run(i);
            printf("  [%s] ARMED %s: first observation, not run yet\n",
                   rows[i].interval, rows[i].command);
            continue;
        }
        long long elapsed = now - total_centiseconds(&last);
        if (elapsed < rows[i].threshold) continue;
        run_scheduled(rows[i].command, rows[i].interval);
        write_last_run(i);
    }
}

static void print_schedule(void) {
    ScheduleRow rows[64];
    int n = load_schedule(rows, 64);
    printf("SCHEDULE (data/schedule.txt)\n");
    printf("  %-10s %-32s %s\n", "INTERVAL", "COMMAND", "STATUS");
    printf("  %-10s %-32s %s\n", "--------", "-------", "------");
    for (int i = 0; i < n; i++) {
        char status[128];
        if (!rows[i].recognised) {
            snprintf(status, sizeof(status), "unrecognised interval - NOT RUN");
        } else {
            char bin[PATH_BUF], bin_exe[PATH_BUF];
            const char *base = strrchr(rows[i].command, '/');
            base = base ? base + 1 : rows[i].command;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(bin, sizeof(bin), "%s/ops/+x/%s", project_root, base);
            snprintf(bin_exe, sizeof(bin_exe), "%s.exe", bin);
#pragma GCC diagnostic pop
            if (!file_exists(bin) && !file_exists(bin_exe))
                snprintf(status, sizeof(status), "not built (original was a stub)");
            else
                snprintf(status, sizeof(status), "ready");
        }
        printf("  %-10s %-32s %s\n", rows[i].interval, rows[i].command, status);
    }
}

/* ------------------------------------------------------------------ */
/* display                                                             */
/* ------------------------------------------------------------------ */

static void show(void) {
    printf("NOW: %04d-%02d-%02d %02d:%02d:%02d.%02d\n", g_time.year, g_time.month,
           g_time.day, g_time.hour, g_time.minute, g_time.second, g_time.centisecond);

    char speed[64];
    read_setting(speed, sizeof(speed));
    printf("ticker_speed: %s\n", speed);

    long long day_cs = 1LL * HOURS_PER_DAY * MINUTES_PER_HOUR * SECONDS_PER_MINUTE * CENTIS_PER_SECOND;
    printf("\nTRUE DURATIONS (the original's rate constants, carried over as-is).\n"
           "The rate is game-CENTISECONDS per real millisecond, so 1/rate is the real\n"
           "milliseconds one game centisecond takes. The labels are historical and do\n"
           "NOT name the span they sit against - this table is the honest reading:\n");
    const char *labels[5] = { "cent", "sec", "min", "hour", "day" };
    for (int i = 0; i < 5; i++) {
        double rate = rate_for_speed(labels[i]);
        double real_ms_per_cs = 1.0 / rate;
        double real_ms_per_day = (double)day_cs / rate;
        printf("  %-6s rate %-12.4f  1 game cs = %-12.4f real ms  |  1 game day = ", labels[i], rate, real_ms_per_cs);
        if (real_ms_per_day >= 86400000.0) printf("%.1f real days\n", real_ms_per_day / 86400000.0);
        else if (real_ms_per_day >= 1000.0) printf("%.1f real minutes\n", real_ms_per_day / 60000.0);
        else printf("%.1f real seconds\n", real_ms_per_day / 1000.0);
    }
    printf("\n  A game month is %d game days (the original's 30-day month). At the 'day'\n"
           "  setting one game day takes %.0f real SECONDS, so one game month takes %.0f\n"
           "  real days. That is the original's own arithmetic, reproduced rather than\n"
           "  corrected - see docs/PORT-FIDELITY.md for what that costs.\n",
           DAYS_PER_MONTH,
           (double)day_cs / rate_for_speed("day") / 1000.0,
           ((double)day_cs / rate_for_speed("day") * DAYS_PER_MONTH) / 86400000.0);
}

/* ------------------------------------------------------------------ */
/* daemon                                                              */
/* ------------------------------------------------------------------ */

static volatile sig_atomic_t g_stop = 0;

/* wsr_clock.c:26-31 */
static void signal_handler(int signo) {
    (void)signo;
    g_stop = 1;
}

int main(int argc, char *argv[]) {
    resolve_root();

    const char *mode = (argc > 1) ? argv[1] : "show";

    if (strcmp(mode, "show") == 0) {
        load_clock_state();
        show();
        return 0;
    }

    if (strcmp(mode, "schedule") == 0) {
        load_clock_state();
        print_schedule();
        return 0;
    }

    if (strcmp(mode, "set-speed") == 0) {
        if (argc < 3) { fprintf(stderr, "Usage: econ_calendar.+x set-speed <cent|sec|min|hour|day>\n"); return 1; }
        if (rate_for_speed(argv[2]) == 0.0) {
            fprintf(stderr, "unknown ticker_speed '%s' (want cent|sec|min|hour|day)\n", argv[2]);
            return 1;
        }
        if (!write_setting_speed(argv[2])) { fprintf(stderr, "cannot write setting.txt\n"); return 1; }
        printf("ticker_speed set to %s\n", argv[2]);
        return 0;
    }

    if (strcmp(mode, "due") == 0) {
        load_clock_state();
        run_due_schedule();
        return 0;
    }

    if (strcmp(mode, "step") == 0) {
        if (argc < 3) { fprintf(stderr, "Usage: econ_calendar.+x step <real_ms>\n"); return 1; }
        long long elapsed_ms = atoll(argv[2]);
        load_clock_state();

        char speed[64];
        read_setting(speed, sizeof(speed));
        long long add = game_centiseconds_for(elapsed_ms, speed);
        if (add == 0) {
            fprintf(stderr, "ticker_speed is '%s' - time does not advance at this setting.\n", speed);
            return 1;
        }
        g_time.centisecond += (int)add;
        cascade_time();
        if (!save_clock_state()) return 1;
        run_due_schedule();
        show();
        return 0;
    }

    if (strcmp(mode, "run") != 0) {
        fprintf(stderr,
                "Usage: econ_calendar.+x run\n"
                "                             show\n"
                "                             schedule\n"
                "                             due\n"
                "                             step <real_ms>\n"
                "                             set-speed <cent|sec|min|hour|day>\n");
        return 1;
    }

    /* ---- daemon: wsr_clock.c:134-164 ---- */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    char pid_path[PATH_BUF];
    data_path(pid_path, sizeof(pid_path), "wsr_clock.pid");
    FILE *pid_fp = fopen(pid_path, "w");
    if (pid_fp == NULL) {
        fprintf(stderr, "Error: Could not open data/wsr_clock.pid for writing.\n");
        exit(1);
    }
#ifdef _WIN32
    fprintf(pid_fp, "%lu\n", (unsigned long)GetCurrentProcessId());
#else
    fprintf(pid_fp, "%d\n", (int)getpid());
#endif
    fclose(pid_fp);

    load_clock_state();

    struct timespec last_time, current_time;
    clock_gettime(CLOCK_MONOTONIC, &last_time);

    while (!g_stop) {
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        long elapsed_ms = (current_time.tv_sec - last_time.tv_sec) * 1000 +
                          (current_time.tv_nsec - last_time.tv_nsec) / 1000000;
        last_time = current_time;

        if (elapsed_ms > 0) {
            char speed[64];
            read_setting(speed, sizeof(speed));
            long long add = game_centiseconds_for(elapsed_ms, speed);
            if (add > 0) {
                g_time.centisecond += (int)add;
                cascade_time();
                run_due_schedule();
                save_clock_state();
            }
        }

#ifdef _WIN32
        Sleep(300);            /* the original's usleep(300000) */
#else
        usleep(300000);
#endif
    }

    save_clock_state();
    remove(pid_path);
    printf("clock stopped at %04d-%02d-%02d %02d:%02d:%02d.%02d\n", g_time.year, g_time.month,
           g_time.day, g_time.hour, g_time.minute, g_time.second, g_time.centisecond);
    return 0;
}
