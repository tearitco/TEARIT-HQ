/* lc_clock.c — livedesk clock system: ONE self-contained binary, several
 * subcommands (daemon / control-plane), designed per au11-hq/15.clock-design.md.
 *
 * DESIGN CONTRACT (read before editing):
 *  - Files live under <house>/#.desktop/clocks/:
 *      clocks.pdl      one line per game clock:  CLOCK|<id>|scope=<s>|desc=<d>
 *      <id>.pdl        per-clock state (SECTION | key | value, read_key_value
 *                      compatible):
 *                        SECTION | game_time_epoch_ms | <ms>
 *                        SECTION | tick               | <n>
 *                        SECTION | running            | 0|1
 *                        SECTION | rate               | off|cent|sec|min|hour|day
 *      reminders.pdl   one line per reminder (pipe-delimited records, NOT
 *                      SECTION rows — a list of identical-shaped entities is
 *                      far easier to update record-at-a-time than multi-line
 *                      sections):
 *                        r<id>|clock=<id>|at=<ms>|text=<s>|event=<s>|note=<s>|enabled=1|fired=<ms>|repeat=<s>
 *      endturn.txt     daemon mailbox: one "endturn <id> [ms]" per line; the
 *                      daemon consumes (truncates) it every loop.
 *      daemon.pid      running daemon pid.
 *  - All state writers use the SAME flock() read-modify-write protocol as
 *    piececraft's pc_clock_daemon.c write_kv() (single fd, whole-file
 *    rewrite, ftruncate) — the manager menus, lc_clock ctl subcommands, and
 *    the daemon are all independent processes touching the same files.
 *  - The daemon is the ONLY writer of game_time_epoch_ms/tick/fired.
 *    Control-plane subcommands only write definition fields (rate/running/
 *    scope/desc/reminder records) + the endturn mailbox.
 *  - Game calendar: proleptic Gregorian anchored at epoch 0 = Year 0 A.D.,
 *    Month 1, Day 1, 00:00 (the telescope's "fake time starting 0 A.D.").
 *  - Usage: lc_clock.<ext> <house_root> <subcommand> [args]
 *    Explicit house_root arg (house lesson: explicit arg beats self-location
 *    inference, see EVENTS_RUNTIME.md).
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <dirent.h>

#define MAX_LINE 1024
#define MAX_PATH 4096
#define PBUF (MAX_PATH + 256)
#define MAX_CLOCKS 64
#define MAX_REMINDERS 128

/* ------------------------------------------------------------------ */
/* small helpers                                                       */
/* ------------------------------------------------------------------ */

static long long wall_ms_now(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000LL + tv.tv_usec / 1000LL;
}

static long long mono_ms_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

static void ensure_dir(const char *p) {
    char cmd[PBUF + 32];
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", p);
    int rc = system(cmd);
    (void)rc;
}

static void clocks_dir(char *out, size_t sz, const char *house) {
    snprintf(out, sz, "%s/#.desktop/clocks", house);
}

static void join3(char *out, size_t sz, const char *a, const char *b, const char *c) {
    if (!c[0])
        snprintf(out, sz, "%s/%s", a, b);
    else if (c[0] == '.')
        snprintf(out, sz, "%s/%s%s", a, b, c);
    else
        snprintf(out, sz, "%s/%s/%s", a, b, c);
}

/* read whole file into buf (NULL-terminated). Returns 0 on success. */
static int read_whole(const char *path, char *buf, size_t sz) {
    buf[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    size_t n = fread(buf, 1, sz - 1, f);
    fclose(f);
    buf[n] = '\0';
    return 0;
}

/* flock-protected whole-file read-modify-write (pc_clock_daemon.c port).
 * Writes key=value line preserving all other lines. */
static void write_kv(const char *path, const char *key, const char *value) {
    int fd = open(path, O_RDWR | O_CREAT, 0644);
    if (fd < 0) return;
    flock(fd, LOCK_EX);
    FILE *f = fdopen(fd, "r+");
    if (!f) { flock(fd, LOCK_UN); close(fd); return; }

    char lines[128][MAX_LINE];
    int nlines = 0;
    while (nlines < 128 && fgets(lines[nlines], MAX_LINE, f)) nlines++;

    size_t key_len = strlen(key);
    int found = 0;
    fseek(f, 0, SEEK_SET);
    for (int i = 0; i < nlines; i++) {
        if (strncmp(lines[i], key, key_len) == 0 && lines[i][key_len] == '=') {
            fprintf(f, "%s=%s\n", key, value);
            found = 1;
        } else {
            fputs(lines[i], f);
        }
    }
    if (!found) fprintf(f, "%s=%s\n", key, value);
    fflush(f);
    long endpos = ftell(f);
    if (endpos >= 0) { int _rc = ftruncate(fd, endpos); (void)_rc; }
    flock(fd, LOCK_UN);
    fclose(f);
}

static void write_kv_ll(const char *path, const char *key, long long value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%lld", value);
    write_kv(path, key, buf);
}

static void write_kv_int(const char *path, const char *key, int value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", value);
    write_kv(path, key, buf);
}

static void read_kv_str(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t key_len = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            char *v = line + key_len + 1;
            v[strcspn(v, "\r\n")] = '\0';
            snprintf(out, out_sz, "%s", v);
        }
    }
    fclose(f);
}

static long long read_kv_ll(const char *path, const char *key, long long def) {
    char buf[64];
    read_kv_str(path, key, buf, sizeof(buf));
    return buf[0] ? atoll(buf) : def;
}

static int read_kv_int(const char *path, const char *key, int def) {
    char buf[64];
    read_kv_str(path, key, buf, sizeof(buf));
    return buf[0] ? atoi(buf) : def;
}

/* ------------------------------------------------------------------ */
/* registry (clocks.pdl)                                               */
/* ------------------------------------------------------------------ */

static int clock_ids(const char *house, char ids[][128], int max) {
    char dir[PBUF], path[PBUF];
    clocks_dir(dir, sizeof(dir), house);
    join3(path, sizeof(path), dir, "clocks.pdl", "");
    char buf[64 * 1024];
    if (read_whole(path, buf, sizeof(buf)) != 0) return 0;
    int n = 0;
    char *save = NULL;
    for (char *line = strtok_r(buf, "\n", &save); line && n < max; line = strtok_r(NULL, "\n", &save)) {
        if (strncmp(line, "CLOCK|", 6) != 0) continue;
        char *id = line + 6;
        char *bar = strchr(id, '|');
        if (bar) *bar = '\0';
        if (id[0]) snprintf(ids[n++], 128, "%s", id);
    }
    return n;
}

static void next_clock_id(const char *house, char *out, size_t sz) {
    char ids[MAX_CLOCKS][128];
    int n = clock_ids(house, ids, MAX_CLOCKS);
    for (int idx = 0; idx < 10000; idx++) {
        snprintf(out, sz, "gameclock%04d", idx);
        int taken = 0;
        for (int i = 0; i < n; i++)
            if (strcmp(ids[i], out) == 0) { taken = 1; break; }
        if (!taken) return;
    }
    snprintf(out, sz, "gameclock_%d", (int)time(NULL));
}

static int create_clock(const char *house, const char *id, const char *scope, const char *desc) {
    char dir[PBUF], path[PBUF], state[PBUF];
    clocks_dir(dir, sizeof(dir), house);
    ensure_dir(dir);
    join3(path, sizeof(path), dir, "clocks.pdl", "");
    join3(state, sizeof(state), dir, id, ".pdl");
    if (access(state, F_OK) == 0) return -1; /* already exists */

    FILE *f = fopen(path, "a");
    if (!f) return -1;
    fprintf(f, "CLOCK|%s|scope=%s|desc=%s\n", id, scope[0] ? scope : "user", desc[0] ? desc : "");
    fclose(f);

    /* state file seeded via flock protocol so a racing daemon never sees a half file */
    write_kv(state, "scope", scope[0] ? scope : "user");
    write_kv(state, "desc", desc[0] ? desc : "");
    write_kv_ll(state, "game_time_epoch_ms", 0);
    write_kv_ll(state, "tick", 0);
    write_kv_int(state, "running", 1);
    write_kv(state, "rate", "off");
    return 0;
}

static int delete_clock(const char *house, const char *id) {
    char dir[PBUF], path[PBUF], state[PBUF];
    clocks_dir(dir, sizeof(dir), house);
    join3(path, sizeof(path), dir, "clocks.pdl", "");
    join3(state, sizeof(state), dir, id, ".pdl");

    char buf[64 * 1024];
    if (read_whole(path, buf, sizeof(buf)) != 0) return -1;
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    int removed = 0;
    char *save = NULL;
    for (char *line = strtok_r(buf, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        char idbuf[128] = "";
        if (sscanf(line, "CLOCK|%127[^|]", idbuf) == 1 && strcmp(idbuf, id) == 0) { removed = 1; continue; }
        fprintf(f, "%s\n", line);
    }
    fclose(f);
    unlink(state);
    return removed ? 0 : -1;
}

/* ------------------------------------------------------------------ */
/* game calendar (proleptic Gregorian, epoch 0 = Year 0 A.D.)          */
/* ------------------------------------------------------------------ */

/* Howard Hinnant's civil_from_days (public domain). days_from_civil is the
 * natural inverse but this build never needs the forward direction. */
static void civil_from_days(long long z, int *y, unsigned *m, unsigned *d) {
    z += 719468;
    long long era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = (unsigned)(z - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long long yy = (long long)yoe + era * 400;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned mp = (5 * doy + 2) / 153;
    unsigned dd = doy - (153 * mp + 2) / 5 + 1;
    unsigned mm = mp + (mp < 10 ? 3 : -9);
    yy += (mm <= 2);
    *y = (int)yy;
    *m = mm;
    *d = dd;
}

static const char *zh_wday[] = {"日", "一", "二", "三", "四", "五", "六"};

/* 0 A.D. anchor per design doc §6.5: epoch_ms = 0 -> year 0, month 1, day 1.
 * Hinnant day 0 = 1970-01-01; year 0 A.D. Jan 1 = day -719528.
 * weekday 0 = Sunday (matches zh_wday and C's tm_wday convention). */
#define DAYS_YEAR0_TO_1970 719528LL

static int game_weekday(long long epoch_ms) {
    long long absday = epoch_ms / 86400000LL - DAYS_YEAR0_TO_1970;
    return (int)(((absday + 4) % 7 + 7) % 7); /* day 0 (1970-01-01) = Thursday = 4 */
}

static void format_gamedate(long long epoch_ms, const char *lang, char *out, size_t sz) {
    if (epoch_ms < 0) epoch_ms = 0;
    long long days = epoch_ms / 86400000LL - DAYS_YEAR0_TO_1970;
    long long tod = epoch_ms % 86400000LL;
    int y; unsigned mo, d;
    civil_from_days(days, &y, &mo, &d);
    int h = (int)(tod / 3600000LL);
    int mi = (int)((tod % 3600000LL) / 60000LL);
    int se = (int)((tod % 60000LL) / 1000LL);
    if (lang && strcmp(lang, "zh") == 0) {
        snprintf(out, sz, "%04d年%02u月%02u日 周%s %02d:%02d:%02d",
                 y, mo, d, zh_wday[game_weekday(epoch_ms)], h, mi, se);
    } else {
        snprintf(out, sz, "%04d-%02u-%02u %02d:%02d:%02d", y, mo, d, h, mi, se);
    }
}

/* ------------------------------------------------------------------ */
/* reminders                                                           */
/* ------------------------------------------------------------------ */

typedef struct {
    char id[64];
    char clock[128];
    long long at_ms;
    char at_text[128];
    char event[512];
    char note[256];
    int enabled;
    long long fired_ms;
    char repeat[64];
} Reminder;

static void reminders_path(char *out, size_t sz, const char *house) {
    char dir[PBUF];
    clocks_dir(dir, sizeof(dir), house);
    join3(out, sz, dir, "reminders.pdl", "");
}

/* build a record line; values are | -free (sanitized on input). */
static void rem_to_line(const Reminder *r, char *out, size_t sz) {
    snprintf(out, sz,
             "%s|clock=%s|at=%lld|text=%s|event=%s|note=%s|enabled=%d|fired=%lld|repeat=%s",
             r->id, r->clock, r->at_ms, r->at_text, r->event, r->note,
             r->enabled, r->fired_ms, r->repeat);
}

static void sanitize_pipe(char *s) {
    for (; *s; s++) if (*s == '|') *s = '_';
}

static int rem_parse(const char *line, Reminder *r) {
    char tmp[MAX_LINE];
    snprintf(tmp, sizeof(tmp), "%s", line);
    char *save = NULL;
    char *tok = strtok_r(tmp, "|", &save);
    if (!tok) return -1;
    snprintf(r->id, sizeof(r->id), "%s", tok);
    r->clock[0] = r->at_text[0] = r->event[0] = r->note[0] = r->repeat[0] = '\0';
    r->at_ms = 0; r->enabled = 1; r->fired_ms = 0;
    while ((tok = strtok_r(NULL, "|", &save)) != NULL) {
        if (strncmp(tok, "clock=", 6) == 0) snprintf(r->clock, sizeof(r->clock), "%s", tok + 6);
        else if (strncmp(tok, "at=", 3) == 0) r->at_ms = atoll(tok + 3);
        else if (strncmp(tok, "text=", 5) == 0) snprintf(r->at_text, sizeof(r->at_text), "%s", tok + 5);
        else if (strncmp(tok, "event=", 6) == 0) snprintf(r->event, sizeof(r->event), "%s", tok + 6);
        else if (strncmp(tok, "note=", 5) == 0) snprintf(r->note, sizeof(r->note), "%s", tok + 5);
        else if (strncmp(tok, "enabled=", 8) == 0) r->enabled = atoi(tok + 8);
        else if (strncmp(tok, "fired=", 6) == 0) r->fired_ms = atoll(tok + 6);
        else if (strncmp(tok, "repeat=", 7) == 0) snprintf(r->repeat, sizeof(r->repeat), "%s", tok + 7);
    }
    return 0;
}

static int reminders_load(const char *house, Reminder *out, int max) {
    char path[PBUF];
    reminders_path(path, sizeof(path), house);
    char buf[128 * 1024];
    if (read_whole(path, buf, sizeof(buf)) != 0) return 0;
    int n = 0;
    char *save = NULL;
    for (char *line = strtok_r(buf, "\n", &save); line && n < max; line = strtok_r(NULL, "\n", &save)) {
        if (rem_parse(line, &out[n]) == 0) n++;
    }
    return n;
}

/* flock-protected full rewrite of reminders.pdl */
static int reminders_save(const char *house, const Reminder *list, int n) {
    char path[PBUF];
    reminders_path(path, sizeof(path), house);
    char dir[PBUF];
    clocks_dir(dir, sizeof(dir), house);
    ensure_dir(dir);
    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    flock(fd, LOCK_EX);
    FILE *f = fdopen(fd, "w");
    if (!f) { flock(fd, LOCK_UN); close(fd); return -1; }
    for (int i = 0; i < n; i++) {
        char line[MAX_LINE];
        rem_to_line(&list[i], line, sizeof(line));
        fprintf(f, "%s\n", line);
    }
    fflush(f);
    flock(fd, LOCK_UN);
    fclose(f);
    return 0;
}

/* parse "when" into at_ms for a given clock kind. Returns 0 on success. */
static int parse_when(int is_real, long long cur_ms, const char *when, long long *at_ms, char *at_text, size_t text_sz) {
    if (!when[0]) return -1;
    long long ms = 0;
    if (when[0] == '+') {
        long long n = atoll(when + 1);
        char unit = when[strlen(when) - 1];
        long long mult = 1000;
        if (unit == 's') mult = 1000;
        else if (unit == 'm') mult = 60000;
        else if (unit == 'h') mult = 3600000;
        else if (unit == 'd') mult = 86400000;
        ms = cur_ms + n * mult;
    } else if (!is_real && when[0] >= '0' && when[0] <= '9') {
        ms = atoll(when); /* absolute game epoch ms */
    } else if (strcmp(when, "now") == 0) {
        ms = cur_ms;
    } else if (is_real && when[0] >= '0' && when[0] <= '9' && strchr(when, ':') != NULL) {
        /* real wall clock HH:MM -> next occurrence */
        int hh = atoi(when), mm = 0;
        const char *colon = strchr(when, ':');
        mm = atoi(colon + 1);
        struct tm tm_info;
        time_t now = time(NULL);
        localtime_r(&now, &tm_info);
        tm_info.tm_hour = hh;
        tm_info.tm_min = mm;
        tm_info.tm_sec = 0;
        time_t t = mktime(&tm_info);
        if (t <= now) t += 86400; /* if already past today, next day */
        ms = (long long)t * 1000LL;
    } else {
        return -1;
    }
    *at_ms = ms;
    snprintf(at_text, text_sz, "%s", when);
    return 0;
}

static void next_rem_id(const char *house, char *out, size_t sz) {
    Reminder list[MAX_REMINDERS];
    int n = reminders_load(house, list, MAX_REMINDERS);
    for (int i = 1; i < 10000; i++) {
        snprintf(out, sz, "r%d", i);
        int taken = 0;
        for (int j = 0; j < n; j++)
            if (strcmp(list[j].id, out) == 0) { taken = 1; break; }
        if (!taken) return;
    }
    snprintf(out, sz, "r%d", (int)time(NULL));
}

static int add_reminder(const char *house, const char *clock, const char *when, const char *event, const char *note) {
    int is_real = (strcmp(clock, "realclock") == 0);
    char dir[PBUF], state[PBUF];
    clocks_dir(dir, sizeof(dir), house);
    long long cur_ms;
    if (is_real) {
        cur_ms = wall_ms_now();
    } else {
        join3(state, sizeof(state), dir, clock, ".pdl");
        if (access(state, F_OK) != 0) return -1; /* unknown game clock */
        cur_ms = read_kv_ll(state, "game_time_epoch_ms", 0);
    }
    long long at_ms;
    char at_text[128];
    if (parse_when(is_real, cur_ms, when, &at_ms, at_text, sizeof(at_text)) != 0) return -1;

    Reminder r;
    memset(&r, 0, sizeof(r));
    next_rem_id(house, r.id, sizeof(r.id));
    snprintf(r.clock, sizeof(r.clock), "%s", clock);
    r.at_ms = at_ms;
    snprintf(r.at_text, sizeof(r.at_text), "%s", at_text);
    snprintf(r.event, sizeof(r.event), "%s", event);
    snprintf(r.note, sizeof(r.note), "%s", note);
    r.enabled = 1;
    r.fired_ms = 0;
    sanitize_pipe(r.note);
    sanitize_pipe(r.event);
    sanitize_pipe(r.at_text);

    Reminder list[MAX_REMINDERS];
    int n = reminders_load(house, list, MAX_REMINDERS);
    if (n >= MAX_REMINDERS) return -1;
    list[n++] = r;
    return reminders_save(house, list, n);
}

static int del_reminder(const char *house, const char *rid) {
    Reminder list[MAX_REMINDERS];
    int n = reminders_load(house, list, MAX_REMINDERS);
    int out = 0;
    for (int i = 0; i < n; i++) {
        if (strcmp(list[i].id, rid) == 0) continue;
        list[out++] = list[i];
    }
    if (out == n) return -1;
    return reminders_save(house, list, out);
}

/* ------------------------------------------------------------------ */
/* endturn mailbox                                                     */
/* ------------------------------------------------------------------ */

static void mailbox_append(const char *house, const char *id, const char *ms_str) {
    char dir[PBUF], path[PBUF];
    clocks_dir(dir, sizeof(dir), house);
    ensure_dir(dir);
    join3(path, sizeof(path), dir, "endturn.txt", "");
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) return;
    flock(fd, LOCK_EX);
    dprintf(fd, "endturn %s %s\n", id, ms_str && ms_str[0] ? ms_str : "3600000");
    flock(fd, LOCK_UN);
    close(fd);
}

/* consume the whole mailbox, applying each endturn. Called by daemon. */
static void consume_mailbox(const char *house) {
    char dir[PBUF], path[PBUF];
    clocks_dir(dir, sizeof(dir), house);
    join3(path, sizeof(path), dir, "endturn.txt", "");
    int fd = open(path, O_RDWR | O_CREAT, 0644);
    if (fd < 0) return;
    flock(fd, LOCK_EX);
    char buf[64 * 1024];
    size_t n = read(fd, buf, sizeof(buf) - 1);
    buf[n] = '\0';
    if (n > 0) {
        char *save = NULL;
        for (char *line = strtok_r(buf, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
            char id[128] = ""; long long amt = 3600000LL;
            if (sscanf(line, "endturn %127s %lld", id, &amt) >= 1 && id[0]) {
                char state[PBUF];
                join3(state, sizeof(state), dir, id, ".pdl");
                if (access(state, F_OK) == 0) {
                    long long ms = read_kv_ll(state, "game_time_epoch_ms", 0);
                    int tick = read_kv_int(state, "tick", 0);
                    write_kv_ll(state, "game_time_epoch_ms", ms + amt);
                    write_kv_int(state, "tick", tick + 1);
                }
            }
        }
        if (ftruncate(fd, 0) != 0) {}
    }
    flock(fd, LOCK_UN);
    close(fd);
}

/* ------------------------------------------------------------------ */
/* daemon                                                              */
/* ------------------------------------------------------------------ */

static volatile sig_atomic_t g_exit = 0;
static void on_sig(int s) { (void)s; g_exit = 1; }

static double rate_mult(const char *rate) {
    if (strcmp(rate, "cent") == 0) return 36000.0;
    if (strcmp(rate, "sec") == 0) return 360.0;
    if (strcmp(rate, "min") == 0) return 6.0;
    if (strcmp(rate, "hour") == 0) return 0.1;
    if (strcmp(rate, "day") == 0) return 0.004166666666666667;
    return 0.0; /* off / unknown */
}

static int pid_alive(long pid) {
    if (pid <= 0) return 0;
    return kill((pid_t)pid, 0) == 0;
}

static int daemon_running(const char *house) {
    char dir[PBUF], path[PBUF];
    clocks_dir(dir, sizeof(dir), house);
    join3(path, sizeof(path), dir, "daemon.pid", "");
    char buf[32];
    if (read_whole(path, buf, sizeof(buf)) != 0) return 0;
    return pid_alive(atol(buf));
}

/* The daemon fires a reminder: launch popup + run the attached event. */
/* REAL, dynamic path discovery (2026-08-17, direct instruction: "we
 * dont hardcode, see how tpmos's button.sh does dynamic path
 * discovery" - live report after muchi-pet/livedesk-clock moved out of
 * xyzfs/bin/). Same real precedent as play_event.sh's own upward
 * landmark search / khtpm_taskbar_manager.c's own toys_scan_one_root().
 * Scans known real app-root directories under house_root for a
 * subdirectory whose name contains app_name. */
static int find_app_dir(const char *house_root, const char *app_name, char *out, size_t outsz) {
    static const char *roots[] = { "*.monads", "&.widgits", "&.hq-apps", "@.apps", NULL };
    for (int i = 0; roots[i]; i++) {
        char parent[PBUF];
        snprintf(parent, sizeof(parent), "%s/%s", house_root, roots[i]);
        DIR *d = opendir(parent);
        if (!d) continue;
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL) {
            if (strstr(ent->d_name, app_name)) {
                snprintf(out, outsz, "%s/%s", parent, ent->d_name);
                closedir(d);
                return 1;
            }
        }
        closedir(d);
    }
    out[0] = '\0';
    return 0;
}

static void fire_reminder(const char *house, const Reminder *r) {
    char dir[PBUF];
    clocks_dir(dir, sizeof(dir), house);
    ensure_dir(dir);

    char msgpath[PBUF];
    snprintf(msgpath, sizeof(msgpath), "%s/fired_%s.txt", dir, r->id);
    FILE *mf = fopen(msgpath, "w");
    if (mf) {
        fprintf(mf, "⏰ REMINDER\n");
        fprintf(mf, "clock: %s @ %s\n", r->clock, r->at_text);
        if (r->note[0]) fprintf(mf, "note: %s\n", r->note);
        if (r->event[0]) fprintf(mf, "event: %s\n", r->event);
        fclose(mf);
    }

    char lc_dir[PBUF];
    find_app_dir(house, "livedesk-clock", lc_dir, sizeof(lc_dir));
    char popup_bin[PBUF];
    snprintf(popup_bin, sizeof(popup_bin), "%s/ops/+x/lc_reminder_popup.+x", lc_dir);
    char sh[PBUF * 2];
    /* house window standard: X11 RGB window + CSS (khtpm_css_parser),
     * launched detached exactly like db-hq/events-hq/context-menu open
     * (setsid nohup <bin> <house> <payload>) — NOT a GL window. */
    snprintf(sh, sizeof(sh), "setsid nohup '%s' '%s' '%s' >/dev/null 2>&1 &",
             popup_bin, house, msgpath);
    int rc = system(sh);
    (void)rc;

    /* run the attached event (.pal/events only, per user decision R5) */
    if (r->event[0]) {
        char pkg[PBUF] = "";
        if (strncmp(r->event, "common:", 7) == 0) {
            snprintf(pkg, sizeof(pkg), "%s/common_events/%s", house, r->event + 7);
        } else if (strncmp(r->event, "clock:", 6) == 0) {
            snprintf(pkg, sizeof(pkg), "%s/#.desktop/clocks/%s", house, r->event + 6);
        } else {
            snprintf(pkg, sizeof(pkg), "%s", r->event);
        }
        char muchi_pet_dir[PBUF];
        find_app_dir(house, "muchi-pet", muchi_pet_dir, sizeof(muchi_pet_dir));
        char evsh[PBUF * 2];
        snprintf(evsh, sizeof(evsh),
                 "setsid nohup sh '%s/ops/play_event.sh' '%s' '%s' >/dev/null 2>&1 &",
                 muchi_pet_dir, pkg, house);
        int rc2 = system(evsh);
        (void)rc2;
    }
}

static void poll_reminders(const char *house) {
    Reminder list[MAX_REMINDERS];
    int n = reminders_load(house, list, MAX_REMINDERS);
    int changed = 0;
    long long wall = wall_ms_now();
    for (int i = 0; i < n; i++) {
        if (!list[i].enabled || list[i].fired_ms != 0) continue;
        long long cur = wall;
        if (strcmp(list[i].clock, "realclock") != 0) {
            char dir[PBUF], state[PBUF];
            clocks_dir(dir, sizeof(dir), house);
            join3(state, sizeof(state), dir, list[i].clock, ".pdl");
            if (access(state, F_OK) != 0) continue; /* clock deleted */
            cur = read_kv_ll(state, "game_time_epoch_ms", 0);
        }
        if (cur >= list[i].at_ms) {
            list[i].fired_ms = wall;
            fire_reminder(house, &list[i]);
            changed = 1;
        }
    }
    if (changed) reminders_save(house, list, n);
}

static int cmd_daemon(const char *house) {
    signal(SIGTERM, on_sig);
    signal(SIGINT, on_sig);
    char dir[PBUF], pidpath[PBUF];
    clocks_dir(dir, sizeof(dir), house);
    ensure_dir(dir);
    join3(pidpath, sizeof(pidpath), dir, "daemon.pid", "");

    /* seed a default gameclock0000 if no registry exists yet */
    char ids[MAX_CLOCKS][128];
    if (clock_ids(house, ids, MAX_CLOCKS) == 0) {
        create_clock(house, "gameclock0000", "user", "main campaign");
    }

    FILE *pf = fopen(pidpath, "w");
    if (pf) { fprintf(pf, "%d\n", (int)getpid()); fclose(pf); }

    long long last = mono_ms_now();
    while (!g_exit) {
        long long now = mono_ms_now();
        long long elapsed = now - last;
        last = now;

        /* ticker: advance every running clock with a real rate */
        int n = clock_ids(house, ids, MAX_CLOCKS);
        for (int i = 0; i < n; i++) {
            char state[PBUF];
            join3(state, sizeof(state), dir, ids[i], ".pdl");
            if (access(state, F_OK) != 0) continue;
            int running = read_kv_int(state, "running", 1);
            char rate[16] = "off";
            read_kv_str(state, "rate", rate, sizeof(rate));
            double mult = rate_mult(rate);
            if (running && mult > 0.0 && elapsed > 0) {
                long long ms = read_kv_ll(state, "game_time_epoch_ms", 0);
                long long old_min = ms / 60000LL;
                double delta_game_cs = (double)elapsed * mult;
                long long delta_game_ms = (long long)(delta_game_cs * 10.0);
                if (delta_game_ms > 0) {
                    ms += delta_game_ms;
                    write_kv_ll(state, "game_time_epoch_ms", ms);
                    long long new_min = ms / 60000LL;
                    if (new_min != old_min) {
                        int tick = read_kv_int(state, "tick", 0);
                        write_kv_int(state, "tick", tick + 1);
                    }
                }
            }
        }

        consume_mailbox(house);
        poll_reminders(house);

        usleep(300000);
    }
    unlink(pidpath);
    return 0;
}

static int cmd_daemon_start(const char *house) {
    if (daemon_running(house)) {
        printf("daemon already running\n");
        return 0;
    }
    char self[PBUF];
    if (!readlink("/proc/self/exe", self, sizeof(self) - 1)) return -1;
    self[PBUF - 1] = '\0';
    char sh[PBUF * 2];
    snprintf(sh, sizeof(sh), "setsid nohup '%s' '%s' daemon >/dev/null 2>&1 &", self, house);
    int rc = system(sh);
    (void)rc;
    usleep(200000);
    return daemon_running(house) ? 0 : -1;
}

static int cmd_daemon_stop(const char *house) {
    char dir[PBUF], path[PBUF];
    clocks_dir(dir, sizeof(dir), house);
    join3(path, sizeof(path), dir, "daemon.pid", "");
    char buf[32];
    if (read_whole(path, buf, sizeof(buf)) == 0 && buf[0]) {
        long pid = atol(buf);
        if (pid > 0) kill((pid_t)pid, SIGTERM);
        return 0;
    }
    return -1;
}

/* ------------------------------------------------------------------ */
/* main / dispatch                                                     */
/* ------------------------------------------------------------------ */

static void print_usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s <house_root> <subcommand> [args]\n"
        "  daemon                          run ticker + reminder poll (forever)\n"
        "  daemon-start | daemon-stop      spawn / stop the headless daemon\n"
        "  new <id|auto> [scope] [desc]    create a game clock (default scope=user)\n"
        "  del <id>                        delete a game clock\n"
        "  list                            list clocks (pipe: id|scope|rate|running|ms|tick)\n"
        "  gamedate <id> [zh|en]           print formatted game date\n"
        "  ticker <id> on|off              enable/disable continuous ticker\n"
        "  rate <id> <cent|sec|min|hour|day|off>\n"
        "  endturn <id> [ms]               queue one discrete advance (default 1 game hour)\n"
        "  reminder-add <clock> <when> <event> [note]   when: HH:MM | +N[smhd] | ms | now\n"
        "  reminder-del <rid>              delete a reminder\n"
        "  reminders                       list reminders\n",
        prog);
}

int main(int argc, char **argv) {
    if (argc < 3) { print_usage(argv[0]); return 1; }
    const char *house = argv[1];
    const char *cmd = argv[2];

    char dir[PBUF];
    clocks_dir(dir, sizeof(dir), house);
    ensure_dir(dir);

    if (strcmp(cmd, "daemon") == 0) return cmd_daemon(house);
    if (strcmp(cmd, "daemon-start") == 0) return cmd_daemon_start(house);
    if (strcmp(cmd, "daemon-stop") == 0) return cmd_daemon_stop(house);

    if (strcmp(cmd, "new") == 0) {
        char id[128];
        if (argc >= 4 && strcmp(argv[3], "auto") != 0) snprintf(id, sizeof(id), "%s", argv[3]);
        else next_clock_id(house, id, sizeof(id));
        const char *scope = argc >= 5 ? argv[4] : "user";
        const char *desc = argc >= 6 ? argv[5] : "";
        if (create_clock(house, id, scope, desc) != 0) { fprintf(stderr, "create failed (exists?)\n"); return 1; }
        printf("%s\n", id);
        return 0;
    }
    if (strcmp(cmd, "del") == 0) {
        if (argc < 4) return 1;
        return delete_clock(house, argv[3]) == 0 ? 0 : 1;
    }
    if (strcmp(cmd, "list") == 0) {
        char ids[MAX_CLOCKS][128];
        int n = clock_ids(house, ids, MAX_CLOCKS);
        for (int i = 0; i < n; i++) {
            char state[PBUF];
            join3(state, sizeof(state), dir, ids[i], ".pdl");
            char rate[16] = "off", scope[64] = "";
            int running = 1;
            if (access(state, F_OK) == 0) {
                read_kv_str(state, "rate", rate, sizeof(rate));
                read_kv_str(state, "scope", scope, sizeof(scope));
                running = read_kv_int(state, "running", 1);
            }
            long long ms = read_kv_ll(state, "game_time_epoch_ms", 0);
            long long tick = read_kv_ll(state, "tick", 0);
            printf("%s|%s|%s|%d|%lld|%lld\n", ids[i], scope, rate, running, ms, tick);
        }
        return 0;
    }
    if (strcmp(cmd, "gamedate") == 0) {
        if (argc < 4) return 1;
        char state[PBUF];
        join3(state, sizeof(state), dir, argv[3], ".pdl");
        if (access(state, F_OK) != 0) { fprintf(stderr, "no such clock: %s\n", argv[3]); return 1; }
        long long ms = read_kv_ll(state, "game_time_epoch_ms", 0);
        char out[256];
        format_gamedate(ms, argc >= 5 ? argv[4] : "zh", out, sizeof(out));
        printf("%s\n", out);
        return 0;
    }
    if (strcmp(cmd, "ticker") == 0) {
        if (argc < 5) return 1;
        char state[PBUF];
        join3(state, sizeof(state), dir, argv[3], ".pdl");
        if (access(state, F_OK) != 0) return 1;
        if (strcmp(argv[4], "on") == 0) write_kv_int(state, "running", 1);
        else write_kv_int(state, "running", 0);
        return 0;
    }
    if (strcmp(cmd, "rate") == 0) {
        if (argc < 5) return 1;
        char state[PBUF];
        join3(state, sizeof(state), dir, argv[3], ".pdl");
        if (access(state, F_OK) != 0) return 1;
        if (rate_mult(argv[4]) == 0.0 && strcmp(argv[4], "off") != 0) return 1;
        write_kv(state, "rate", argv[4]);
        return 0;
    }
    if (strcmp(cmd, "endturn") == 0) {
        if (argc < 4) return 1;
        mailbox_append(house, argv[3], argc >= 5 ? argv[4] : NULL);
        return 0;
    }
    if (strcmp(cmd, "reminder-add") == 0) {
        if (argc < 6) return 1;
        const char *note = argc >= 7 ? argv[6] : "";
        if (add_reminder(house, argv[3], argv[4], argv[5], note) != 0) {
            fprintf(stderr, "reminder-add failed\n");
            return 1;
        }
        return 0;
    }
    if (strcmp(cmd, "reminder-del") == 0) {
        if (argc < 4) return 1;
        return del_reminder(house, argv[3]) == 0 ? 0 : 1;
    }
    if (strcmp(cmd, "reminders") == 0) {
        Reminder list[MAX_REMINDERS];
        int n = reminders_load(house, list, MAX_REMINDERS);
        for (int i = 0; i < n; i++) {
            printf("%s|%s|%lld|%s|%s|%s|%d|%lld|%s\n",
                   list[i].id, list[i].clock, list[i].at_ms, list[i].at_text,
                   list[i].event, list[i].note, list[i].enabled, list[i].fired_ms, list[i].repeat);
        }
        return 0;
    }

    print_usage(argv[0]);
    return 1;
}
