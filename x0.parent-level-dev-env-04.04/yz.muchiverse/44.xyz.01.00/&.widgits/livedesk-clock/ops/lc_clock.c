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
 *                        r<id>|clock=<id>|at=<ms>|text=<s>|event=<s>|note=<s>|enabled=1|fired=<ms>|repeat=<s>[|until=<ms>]
 *                        repeat= empty = one-shot (fired= is its state); repeat=every:<n><min|hour|day|week|
 *                        month|year> recurs on the game calendar (month/year keep the anchor day, clamped).
 *      schedule_ledger.txt  append-only SCHED|<rid>|occurrence_ms|fired_wall_ms|ok|fail|timeout|caught up <N>.
 *                        The ledger IS the recurrence cursor (never mtime). More than 400 missed occurrences
 *                        in one poll: 400 fire in order, the rest collapse into one 'caught up N' row + a
 *                        row in schedule_flags.txt. Events run fork+exec+waitpid with a 30 s watchdog.
 *      Harness hooks (unset = unchanged): `step <real_ms>` subcommand, LC_CLOCK_NO_POPUP=1,
 *                        LC_CLOCK_EVENT_RUNNER=<bin>, LC_CLOCK_EVENT_TIMEOUT_S=<n>.
 *      endturn.txt     daemon mailbox, one command per line: "endturn <id> [ms]" or
 *                        rate <id> <off|cent|sec|min|hour|day|x<ratio>> | pause <id> | resume <id> |
 *                        advance <id> <ms|Ns|Nm|Nh|Nd> | settime <id> <ms>      each may end with " @<source>".
 *                      The daemon pass consumes (truncates) it, applies each command under the clock's limits
 *                      (state keys limit_min_rate, limit_max_rate, allow_settime_back=0, allow_commands=1) and
 *                      appends EVERY accepted or refused command to append-only clock_audit.txt
 *                      (ts|clock|cmd|args|result|source). Time never runs backward by command: advance/endturn
 *                      must be > 0 and settime earlier than now is refused unless allow_settime_back=1 (logged
 *                      as ok:backwards-allowed). Send with `lc_clock <root> cmd <clock> <verb> [arg] [--source S]`.
 *      Chaining        clocks.pdl rows may carry |parent=<id>|ratio=<n>|master=0|1 (legacy rows have none).
 *                      A chained child's game_time_epoch_ms = round(parent_epoch*ratio) + child_offset (state
 *                      key), recomputed on every pass by derive_children and never advanced on its own, so
 *                      pause/rate/advance of the parent move the child. Rate/pause on a child are refused;
 *                      advance/settime on a child move its child_offset. Exactly one master=1 (`master [id]`
 *                      get/set; setting clears the old one; clearing to zero is refused; two in the file read
 *                      as ambiguous). Cycles are refused by `chain`. `gamedate master` is the display read.
 *      daemon.pid      running daemon pid.
 *  - All state writers use the SAME flock() read-modify-write protocol as
 *    piececraft's pc_clock_daemon.c write_kv() (single fd, whole-file
 *    rewrite, ftruncate) — the manager menus, lc_clock ctl subcommands, and
 *    the daemon are all independent processes touching the same files.
 *  - The daemon is the ONLY writer of game_time_epoch_ms/tick/fired.
 *    Control-plane subcommands only write definition fields (rate/running/
 *    scope/desc/limits/chain rows/reminder records) + the endturn mailbox. The ctl `rate`/`ticker`
 *    subcommands (legacy, owner panel) write directly and bypass limits and audit; event commands
 *    go through `cmd` (mailbox, limits, audit).
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
#include <sys/wait.h>

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


static long long days_from_civil(long long y, unsigned m, unsigned d) {
    y -= (m <= 2);
    long long era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (long long)doe - 719468;
}

static int is_leap(long long y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }
static unsigned month_len(long long y, unsigned m) {
    static const unsigned ml[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    return (m == 2 && is_leap(y)) ? 29 : ml[m - 1];
}

/* ------------------------------------------------------------------ */
/* recurrence: repeat=every:<n><unit>  (CLOCK-AS-THE-PLAY-SPINE 3.3)   */
/* occurrence k is a pure function of (anchor at, n, unit, k): month and */
/* year steps follow the game calendar and keep the ANCHOR day-of-month  */
/* (Jan 31 -> Feb 28/29 -> Mar 31), clamped to the month length.         */
/* ------------------------------------------------------------------ */

#define CATCHUP_CAP 400
#define DAY_MS 86400000LL

typedef struct { int n; long long step_ms; int months; } Repeat; /* months>0 => calendar unit */

static int parse_repeat(const char *s, Repeat *rp) {
    int n = 0; char unit[16] = "";
    if (strncmp(s, "every:", 6) != 0) return -1;
    if (sscanf(s + 6, "%d%15s", &n, unit) != 2 || n < 1) return -1;
    rp->n = n; rp->step_ms = 0; rp->months = 0;
    if (!strcmp(unit, "min")) rp->step_ms = 60000LL * n;
    else if (!strcmp(unit, "hour")) rp->step_ms = 3600000LL * n;
    else if (!strcmp(unit, "day")) rp->step_ms = DAY_MS * n;
    else if (!strcmp(unit, "week")) rp->step_ms = 7 * DAY_MS * n;
    else if (!strcmp(unit, "month")) rp->months = n;
    else if (!strcmp(unit, "year")) rp->months = 12 * n;
    else return -1;
    return 0;
}

static void ms_to_ym(long long ms, long long *ym_total, unsigned *d, long long *tod) {
    int y; unsigned m, dd;
    civil_from_days(ms / DAY_MS - DAYS_YEAR0_TO_1970, &y, &m, &dd);
    *ym_total = (long long)y * 12 + (m - 1);
    *d = dd;
    *tod = ms % DAY_MS;
}

static long long rep_occ(long long at, const Repeat *rp, long long k) {
    if (!rp->months) return at + k * rp->step_ms;
    long long tot; unsigned d; long long tod;
    ms_to_ym(at, &tot, &d, &tod);
    tot += k * rp->months;
    long long y = tot >= 0 ? tot / 12 : -((-tot + 11) / 12);
    unsigned m = (unsigned)(tot - y * 12) + 1;
    unsigned ml = month_len(y, m);
    if (d > ml) d = ml;
    return (days_from_civil(y, m, d) + DAYS_YEAR0_TO_1970) * DAY_MS + tod;
}

/* largest k with occurrence(k) <= limit, or -1 if even occurrence 0 is later */
static long long rep_kmax(long long at, const Repeat *rp, long long limit) {
    if (limit < at) return -1;
    long long k;
    if (!rp->months) return (limit - at) / rp->step_ms;
    long long a, l; unsigned d; long long tod;
    ms_to_ym(at, &a, &d, &tod);
    ms_to_ym(limit, &l, &d, &tod);
    k = (l - a) / rp->months;
    while (k > 0 && rep_occ(at, rp, k) > limit) k--;
    while (rep_occ(at, rp, k + 1) <= limit) k++;
    return k;
}

/* ------------------------------------------------------------------ */
/* schedule ledger: append-only, the cursor (never mtime)              */
/*   SCHED|<reminder id>|occurrence_ms|fired_wall_ms|result            */
/* result: ok | fail | timeout | caught up <N>                         */
/* ------------------------------------------------------------------ */

typedef struct { char id[64]; long long last_occ; } LedgerCur;
static LedgerCur *g_cur = NULL; static int g_ncur = 0, g_capcur = 0;
static long long g_ledger_off = 0; /* bytes already folded into g_cur (size-growth cursor) */
static unsigned long long g_ledger_ino = 0, g_ledger_dev = 0; /* identity of the file g_cur was built from */

static void ledger_path(char *out, size_t sz, const char *house) {
    char dir[PBUF];
    clocks_dir(dir, sizeof(dir), house);
    join3(out, sz, dir, "schedule_ledger.txt", "");
}

static void cur_note(const char *id, long long occ) {
    for (int i = 0; i < g_ncur; i++)
        if (strcmp(g_cur[i].id, id) == 0) { if (occ > g_cur[i].last_occ) g_cur[i].last_occ = occ; return; }
    if (g_ncur == g_capcur) {
        g_capcur = g_capcur ? g_capcur * 2 : 64;
        g_cur = realloc(g_cur, (size_t)g_capcur * sizeof(LedgerCur));
        if (!g_cur) { g_ncur = g_capcur = 0; return; }
    }
    snprintf(g_cur[g_ncur].id, sizeof(g_cur[g_ncur].id), "%s", id);
    g_cur[g_ncur].last_occ = occ;
    g_ncur++;
}

/* fold any rows appended since the last call into the in-memory cursor map */
static void ledger_refresh(const char *house) {
    char path[PBUF];
    ledger_path(path, sizeof(path), house);
    FILE *f = fopen(path, "r");
    if (!f) return;
    /* A checkpoint restore replaces schedule_ledger.txt (atomic rename = new inode) or shrinks it. The in-memory
     * cursor then describes a ledger that no longer exists: drop it and re-fold from byte 0 (identity by inode +
     * size, never mtime). A long-lived daemon would otherwise keep a stale, too-high cursor and skip firings. */
    struct stat lst;
    if (fstat(fileno(f), &lst) == 0) {
        if ((g_ledger_ino && ((unsigned long long)lst.st_ino != g_ledger_ino || (unsigned long long)lst.st_dev != g_ledger_dev)) ||
            lst.st_size < g_ledger_off) {
            g_ncur = 0; g_ledger_off = 0;
        }
        g_ledger_ino = (unsigned long long)lst.st_ino; g_ledger_dev = (unsigned long long)lst.st_dev;
    }
    if (fseek(f, g_ledger_off, SEEK_SET) != 0) { fclose(f); return; }
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        if (len == 0 || line[len - 1] != '\n') break; /* partial row: re-read next time */
        g_ledger_off += (long long)len;
        char id[64]; long long occ;
        if (sscanf(line, "SCHED|%63[^|]|%lld|", id, &occ) == 2) cur_note(id, occ);
    }
    fclose(f);
}

static int ledger_cursor(const char *id, long long *occ) {
    for (int i = 0; i < g_ncur; i++)
        if (strcmp(g_cur[i].id, id) == 0) { *occ = g_cur[i].last_occ; return 1; }
    return 0;
}

static void ledger_append(const char *house, const char *id, long long occ, long long wall, const char *result) {
    char path[PBUF];
    ledger_path(path, sizeof(path), house);
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) return;
    flock(fd, LOCK_EX);
    dprintf(fd, "SCHED|%s|%lld|%lld|%s\n", id, occ, wall, result);
    flock(fd, LOCK_UN);
    close(fd);
    cur_note(id, occ); /* keep the in-memory cursor in step; refresh will skip nothing twice (max) */
}

static void flag_append(const char *house, const char *id, long long n, long long wall) {
    char dir[PBUF], path[PBUF];
    clocks_dir(dir, sizeof(dir), house);
    join3(path, sizeof(path), dir, "schedule_flags.txt", "");
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) return;
    flock(fd, LOCK_EX);
    dprintf(fd, "FLAG|%s|caught_up|%lld|%lld\n", id, n, wall);
    flock(fd, LOCK_UN);
    close(fd);
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
    long long until_ms;   /* 0 = no end; only meaningful with repeat=every:... */
} Reminder;

static void reminders_path(char *out, size_t sz, const char *house) {
    char dir[PBUF];
    clocks_dir(dir, sizeof(dir), house);
    join3(out, sz, dir, "reminders.pdl", "");
}

/* build a record line; values are | -free (sanitized on input). */
static void rem_to_line(const Reminder *r, char *out, size_t sz) {
    int w = snprintf(out, sz,
             "%s|clock=%s|at=%lld|text=%s|event=%s|note=%s|enabled=%d|fired=%lld|repeat=%s",
             r->id, r->clock, r->at_ms, r->at_text, r->event, r->note,
             r->enabled, r->fired_ms, r->repeat);
    /* until= is written only when set, so legacy lines stay byte-identical */
    if (r->until_ms > 0 && w > 0 && (size_t)w < sz)
        snprintf(out + w, sz - (size_t)w, "|until=%lld", r->until_ms);
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
    r->at_ms = 0; r->enabled = 1; r->fired_ms = 0; r->until_ms = 0;
    while ((tok = strtok_r(NULL, "|", &save)) != NULL) {
        if (strncmp(tok, "clock=", 6) == 0) snprintf(r->clock, sizeof(r->clock), "%s", tok + 6);
        else if (strncmp(tok, "at=", 3) == 0) r->at_ms = atoll(tok + 3);
        else if (strncmp(tok, "text=", 5) == 0) snprintf(r->at_text, sizeof(r->at_text), "%s", tok + 5);
        else if (strncmp(tok, "event=", 6) == 0) snprintf(r->event, sizeof(r->event), "%s", tok + 6);
        else if (strncmp(tok, "note=", 5) == 0) snprintf(r->note, sizeof(r->note), "%s", tok + 5);
        else if (strncmp(tok, "enabled=", 8) == 0) r->enabled = atoi(tok + 8);
        else if (strncmp(tok, "fired=", 6) == 0) r->fired_ms = atoll(tok + 6);
        else if (strncmp(tok, "repeat=", 7) == 0) snprintf(r->repeat, sizeof(r->repeat), "%s", tok + 7);
        else if (strncmp(tok, "until=", 6) == 0) r->until_ms = atoll(tok + 6);
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
    ledger_refresh(house);
    for (int i = 1; i < 10000; i++) {
        snprintf(out, sz, "r%d", i);
        int taken = 0;
        long long dummy;
        for (int j = 0; j < n; j++)
            if (strcmp(list[j].id, out) == 0) { taken = 1; break; }
        /* an id already in the schedule ledger is spent: reusing it would inherit its cursor */
        if (!taken && ledger_cursor(out, &dummy)) taken = 1;
        if (!taken) return;
    }
    snprintf(out, sz, "r%d", (int)time(NULL));
}

static int add_reminder(const char *house, const char *clock, const char *when, const char *event, const char *note,
                        const char *repeat, long long until_ms) {
    Repeat rp_chk;
    if (repeat[0] && parse_repeat(repeat, &rp_chk) != 0) return -1; /* refuse unknown repeat syntax */
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
    snprintf(r.repeat, sizeof(r.repeat), "%s", repeat);
    r.until_ms = until_ms;
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
/* rates (names and x<ratio>), clock rows (parent/ratio/master), audit  */
/* ------------------------------------------------------------------ */

/* game ms per real ms for a rate string. Names keep the legacy multipliers (cent 360000, sec 3600, min 60,
 * hour 1 = real time, day 1/24); x<ratio> is that number directly (x1 = real time, x2 = twice real time). */
static double rate_mult(const char *rate) {
    if (strcmp(rate, "cent") == 0) return 36000.0;
    if (strcmp(rate, "sec") == 0) return 360.0;
    if (strcmp(rate, "min") == 0) return 6.0;
    if (strcmp(rate, "hour") == 0) return 0.1;
    if (strcmp(rate, "day") == 0) return 0.004166666666666667;
    return 0.0; /* off / unknown; x<ratio> is handled by parse_rate */
}

/* 0 = valid, *x = game ms per real ms (0 for off). -1 = invalid. */
static int parse_rate(const char *s, double *x) {
    if (!s || !s[0] || strlen(s) > 30) return -1;
    if (strcmp(s, "off") == 0) { *x = 0.0; return 0; }
    if (s[0] == 'x') {
        char *end = NULL;
        double v = strtod(s + 1, &end);
        if (end == s + 1 || *end || !(v > 0.0) || v > 1e9) return -1; /* also rejects nan */
        *x = v;
        return 0;
    }
    double m = rate_mult(s);
    if (m <= 0.0) return -1;
    *x = m * 10.0;
    return 0;
}

/* a duration: <n> ms, or <n>s|m|h|d (seconds/minutes/hours/days). Returns 0 and *ms, or -1. */
static int parse_amount(const char *s, long long *ms) {
    char *end = NULL;
    long long n = strtoll(s, &end, 10);
    if (end == s) return -1;
    long long mult = 1;
    if (*end == 's') mult = 1000; else if (*end == 'm') mult = 60000; else if (*end == 'h') mult = 3600000; else if (*end == 'd') mult = DAY_MS;
    else if (*end) return -1;
    if (*end && end[1]) return -1;
    *ms = n * mult;
    return 0;
}

typedef struct {
    char id[128], scope[64], desc[256], parent[128], ratio[32];
    int master, extended; /* extended = row carries parent=/ratio=/master= fields (legacy rows do not) */
} ClockRow;

static void clocks_file(char *out, size_t sz, const char *house) {
    char dir[PBUF];
    clocks_dir(dir, sizeof(dir), house);
    join3(out, sz, dir, "clocks.pdl", "");
}

static void row_parse(const char *line, ClockRow *r) {
    char tmp[MAX_LINE];
    snprintf(tmp, sizeof(tmp), "%s", line);
    memset(r, 0, sizeof(*r));
    char *save = NULL;
    char *tok = strtok_r(tmp, "|", &save); /* "CLOCK" */
    tok = strtok_r(NULL, "|", &save);
    if (tok) snprintf(r->id, sizeof(r->id), "%s", tok);
    while ((tok = strtok_r(NULL, "|", &save)) != NULL) {
        if (strncmp(tok, "scope=", 6) == 0) snprintf(r->scope, sizeof(r->scope), "%s", tok + 6);
        else if (strncmp(tok, "desc=", 5) == 0) snprintf(r->desc, sizeof(r->desc), "%s", tok + 5);
        else if (strncmp(tok, "parent=", 7) == 0) { snprintf(r->parent, sizeof(r->parent), "%s", tok + 7); r->extended = 1; }
        else if (strncmp(tok, "ratio=", 6) == 0) { snprintf(r->ratio, sizeof(r->ratio), "%s", tok + 6); r->extended = 1; }
        else if (strncmp(tok, "master=", 7) == 0) { r->master = atoi(tok + 7) == 1; r->extended = 1; }
    }
}

static void row_format(const ClockRow *r, char *out, size_t sz) {
    if (!r->extended)
        snprintf(out, sz, "CLOCK|%s|scope=%s|desc=%s", r->id, r->scope, r->desc);
    else
        snprintf(out, sz, "CLOCK|%s|scope=%s|desc=%s|parent=%s|ratio=%s|master=%d",
                 r->id, r->scope, r->desc, r->parent, r->ratio, r->master);
}

static int rows_load(const char *house, ClockRow *rows, int max) {
    char path[PBUF];
    clocks_file(path, sizeof(path), house);
    char buf[64 * 1024];
    if (read_whole(path, buf, sizeof(buf)) != 0) return 0;
    int n = 0;
    char *save = NULL;
    for (char *line = strtok_r(buf, "\n", &save); line && n < max; line = strtok_r(NULL, "\n", &save)) {
        if (strncmp(line, "CLOCK|", 6) != 0) continue;
        row_parse(line, &rows[n]);
        if (rows[n].id[0]) n++;
    }
    return n;
}

static int row_find(const ClockRow *rows, int n, const char *id) {
    for (int i = 0; i < n; i++) if (strcmp(rows[i].id, id) == 0) return i;
    return -1;
}

/* chained = a row with a non-empty parent= */
static int clock_parent_of(const char *house, const char *id, char *parent, size_t psz, double *ratio) {
    ClockRow rows[MAX_CLOCKS];
    int n = rows_load(house, rows, MAX_CLOCKS);
    int i = row_find(rows, n, id);
    if (i < 0 || !rows[i].parent[0]) return 0;
    snprintf(parent, psz, "%s", rows[i].parent);
    *ratio = rows[i].ratio[0] ? atof(rows[i].ratio) : 1.0;
    if (!(*ratio > 0.0)) *ratio = 1.0;
    return 1;
}

/* Rewrite clocks.pdl under flock (definition fields only). op: 'm' master=a, 'c' chain a under b with ratio c, 'u' unchain a.
 * Returns 0 or -1 with a reason in err. */
static int rows_edit(const char *house, char op, const char *a, const char *b, const char *c, char *err, size_t esz) {
    char path[PBUF];
    clocks_file(path, sizeof(path), house);
    int fd = open(path, O_RDWR);
    if (fd < 0) { snprintf(err, esz, "no clock registry"); return -1; }
    flock(fd, LOCK_EX);
    char buf[64 * 1024];
    ssize_t len = read(fd, buf, sizeof(buf) - 1);
    if (len < 0) len = 0;
    buf[len] = '\0';
    ClockRow rows[MAX_CLOCKS];
    int n = 0, rc = 0;
    char *save = NULL;
    for (char *line = strtok_r(buf, "\n", &save); line && n < MAX_CLOCKS; line = strtok_r(NULL, "\n", &save)) {
        if (strncmp(line, "CLOCK|", 6) != 0) continue;
        row_parse(line, &rows[n]);
        if (rows[n].id[0]) n++;
    }
    int ia = row_find(rows, n, a);
    if (ia < 0) { snprintf(err, esz, "no such clock: %s", a); rc = -1; }
    else if (op == 'm') {
        for (int i = 0; i < n; i++) { if (rows[i].master) { rows[i].master = 0; rows[i].extended = 1; } }
        rows[ia].master = 1; rows[ia].extended = 1;
    } else if (op == 'c') {
        int ib = row_find(rows, n, b);
        double rt = atof(c);
        if (ib < 0) { snprintf(err, esz, "no such parent clock: %s", b); rc = -1; }
        else if (!(rt > 0.0) || rt > 1e9) { snprintf(err, esz, "bad ratio: %s", c); rc = -1; }
        else {
            int cyc = (ia == ib);
            for (int cur = ib, depth = 0; !cyc && cur >= 0 && rows[cur].parent[0] && depth < MAX_CLOCKS; depth++) {
                cur = row_find(rows, n, rows[cur].parent);
                if (cur == ia) cyc = 1;
            }
            if (cyc) { snprintf(err, esz, "cycle: %s -> %s would loop", a, b); rc = -1; }
            else {
                snprintf(rows[ia].parent, sizeof(rows[ia].parent), "%s", b);
                snprintf(rows[ia].ratio, sizeof(rows[ia].ratio), "%s", c);
                rows[ia].extended = 1;
            }
        }
    } else if (op == 'u') {
        rows[ia].parent[0] = '\0'; rows[ia].ratio[0] = '\0';
    }
    if (rc == 0) {
        if (ftruncate(fd, 0) != 0) {}
        lseek(fd, 0, SEEK_SET);
        for (int i = 0; i < n; i++) {
            char ln[MAX_LINE];
            row_format(&rows[i], ln, sizeof(ln));
            dprintf(fd, "%s\n", ln);
        }
    }
    flock(fd, LOCK_UN);
    close(fd);
    return rc;
}

/* append-only audit: ts|clock|cmd|args|result|source (never truncated, never read back by the daemon) */
static void audit_clean(char *s) { for (; *s; s++) if (*s == '|' || *s == '\n' || *s == '\r') *s = '_'; }
static void audit_append(const char *house, const char *clock, const char *cmd, const char *args, const char *result, const char *source) {
    char dir[PBUF], path[PBUF];
    clocks_dir(dir, sizeof(dir), house);
    join3(path, sizeof(path), dir, "clock_audit.txt", "");
    char c[160], k[64], a[256], r[160], s[256];
    snprintf(c, sizeof(c), "%s", clock); snprintf(k, sizeof(k), "%s", cmd); snprintf(a, sizeof(a), "%s", args ? args : "");
    snprintf(r, sizeof(r), "%s", result); snprintf(s, sizeof(s), "%s", source && source[0] ? source : "unknown");
    audit_clean(c); audit_clean(k); audit_clean(a); audit_clean(r); audit_clean(s);
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) return;
    flock(fd, LOCK_EX);
    dprintf(fd, "%lld|%s|%s|%s|%s|%s\n", wall_ms_now(), c, k, a, r, s);
    flock(fd, LOCK_UN);
    close(fd);
}

/* ------------------------------------------------------------------ */
/* endturn mailbox + clock commands                                    */
/*   <verb> <clock> [arg] [@source]   verbs: endturn rate pause resume  */
/*   advance settime. The daemon pass is the only applier.              */
/* ------------------------------------------------------------------ */

static void mailbox_put(const char *house, const char *line) {
    char dir[PBUF], path[PBUF];
    clocks_dir(dir, sizeof(dir), house);
    ensure_dir(dir);
    join3(path, sizeof(path), dir, "endturn.txt", "");
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) return;
    flock(fd, LOCK_EX);
    dprintf(fd, "%s\n", line);
    flock(fd, LOCK_UN);
    close(fd);
}

static void mailbox_append(const char *house, const char *id, const char *ms_str) {
    char line[512];
    snprintf(line, sizeof(line), "endturn %s %s", id, ms_str && ms_str[0] ? ms_str : "3600000");
    mailbox_put(house, line);
}

/* derive every chained clock from its parent: epoch = round(parent_epoch * ratio) + child_offset. A pure function,
 * recomputed every pass; a child is never advanced on its own. Cycles (hand edits) are cut by a depth cap. */
static long long round_ll(double v) { return (long long)(v >= 0 ? v + 0.5 : v - 0.5); }

static void derive_one(const char *house, const char *dir, ClockRow *rows, int n, int i, int depth) {
    if (depth > 32 || !rows[i].parent[0]) return;
    int ip = row_find(rows, n, rows[i].parent);
    if (ip < 0) return;
    if (rows[ip].parent[0]) derive_one(house, dir, rows, n, ip, depth + 1);
    char ps[PBUF], cs[PBUF];
    join3(ps, sizeof(ps), dir, rows[ip].id, ".pdl");
    join3(cs, sizeof(cs), dir, rows[i].id, ".pdl");
    if (access(ps, F_OK) != 0 || access(cs, F_OK) != 0) return;
    double ratio = rows[i].ratio[0] ? atof(rows[i].ratio) : 1.0;
    if (!(ratio > 0.0)) ratio = 1.0;
    long long pe = read_kv_ll(ps, "game_time_epoch_ms", 0);
    long long ce = round_ll((double)pe * ratio) + read_kv_ll(cs, "child_offset", 0);
    if (ce != read_kv_ll(cs, "game_time_epoch_ms", 0)) write_kv_ll(cs, "game_time_epoch_ms", ce);
    long long pt = read_kv_ll(ps, "tick", 0);
    if (pt != read_kv_ll(cs, "tick", 0)) write_kv_ll(cs, "tick", pt);
    (void)house;
}

static void derive_children(const char *house) {
    char dir[PBUF];
    clocks_dir(dir, sizeof(dir), house);
    ClockRow rows[MAX_CLOCKS];
    int n = rows_load(house, rows, MAX_CLOCKS);
    for (int i = 0; i < n; i++) if (rows[i].parent[0]) derive_one(house, dir, rows, n, i, 0);
}

/* limits live per clock in its state file; an unset limit means unbounded */
static int within_rate_limits(const char *state, double x, const char **why) {
    char b[64]; double lim;
    read_kv_str(state, "limit_min_rate", b, sizeof(b));
    if (b[0] && parse_rate(b, &lim) == 0 && x < lim) { *why = "refused:below-min-rate"; return 0; }
    read_kv_str(state, "limit_max_rate", b, sizeof(b));
    if (b[0] && parse_rate(b, &lim) == 0 && x > lim) { *why = "refused:above-max-rate"; return 0; }
    return 1;
}

static void apply_command(const char *house, const char *dir, const char *rawline) {
    char line[MAX_LINE];
    snprintf(line, sizeof(line), "%s", rawline);
    char source[256] = "mailbox";
    char *at = strstr(line, " @");
    if (at) { snprintf(source, sizeof(source), "%s", at + 2); *at = '\0'; }
    char verb[32] = "", id[128] = "", arg[128] = "";
    int nf = sscanf(line, "%31s %127s %127s", verb, id, arg);
    if (nf < 1) return;
    if (nf < 2) { audit_append(house, "-", verb, "", "refused:usage", source); return; }

    char state[PBUF];
    join3(state, sizeof(state), dir, id, ".pdl");
    if (access(state, F_OK) != 0) { audit_append(house, id, verb, arg, "refused:no-such-clock", source); return; }

    char par[128]; double pratio = 1.0;
    int chained = clock_parent_of(house, id, par, sizeof(par), &pratio);
    int is_end = strcmp(verb, "endturn") == 0;

    if (!is_end && !strcmp(verb, "rate") + !strcmp(verb, "pause") + !strcmp(verb, "resume") +
                   !strcmp(verb, "advance") + !strcmp(verb, "settime") == 0) {
        audit_append(house, id, verb, arg, "refused:unknown-command", source);
        return;
    }
    if (!is_end && read_kv_int(state, "allow_commands", 1) == 0) {
        audit_append(house, id, verb, arg, "refused:commands-disabled", source);
        return;
    }

    if (!strcmp(verb, "rate")) {
        double x;
        const char *why = "";
        if (nf < 3 || parse_rate(arg, &x) != 0) { audit_append(house, id, verb, arg, "refused:bad-rate", source); return; }
        if (chained) { audit_append(house, id, verb, arg, "refused:chained-child", source); return; }
        if (!within_rate_limits(state, x, &why)) { audit_append(house, id, verb, arg, why, source); return; }
        write_kv(state, "rate", arg);
        audit_append(house, id, verb, arg, "ok", source);
    } else if (!strcmp(verb, "pause") || !strcmp(verb, "resume")) {
        if (chained) { audit_append(house, id, verb, "", "refused:chained-child", source); return; }
        write_kv_int(state, "running", verb[0] == 'r' ? 1 : 0);
        audit_append(house, id, verb, "", "ok", source);
    } else if (!strcmp(verb, "advance") || is_end) {
        long long amt = 3600000LL;
        if (nf >= 3) {
            int bad = is_end ? (sscanf(arg, "%lld", &amt) != 1) : (parse_amount(arg, &amt) != 0);
            if (bad) { audit_append(house, id, verb, arg, "refused:bad-amount", source); return; }
        }
        if (amt <= 0) { audit_append(house, id, verb, arg, "refused:non-positive", source); return; } /* time never runs backward by command */
        if (chained) {
            write_kv_ll(state, "child_offset", read_kv_ll(state, "child_offset", 0) + amt); /* epoch follows on the derive pass */
        } else {
            write_kv_ll(state, "game_time_epoch_ms", read_kv_ll(state, "game_time_epoch_ms", 0) + amt);
            write_kv_int(state, "tick", read_kv_int(state, "tick", 0) + 1);
        }
        audit_append(house, id, verb, arg, "ok", source);
    } else { /* settime */
        long long target;
        if (nf < 3 || sscanf(arg, "%lld", &target) != 1 || target < 0) { audit_append(house, id, verb, arg, "refused:bad-time", source); return; }
        long long cur = read_kv_ll(state, "game_time_epoch_ms", 0);
        const char *res = "ok";
        if (target < cur) {
            if (read_kv_int(state, "allow_settime_back", 0) != 1) { audit_append(house, id, verb, arg, "refused:backwards", source); return; }
            res = "ok:backwards-allowed";
        }
        if (chained) {
            char ps[PBUF];
            join3(ps, sizeof(ps), dir, par, ".pdl");
            long long pe = read_kv_ll(ps, "game_time_epoch_ms", 0);
            write_kv_ll(state, "child_offset", target - round_ll((double)pe * pratio));
        } else {
            write_kv_ll(state, "game_time_epoch_ms", target);
            write_kv_int(state, "tick", read_kv_int(state, "tick", 0) + 1);
        }
        audit_append(house, id, verb, arg, res, source);
    }
}

/* consume the whole mailbox, applying each command in order. Called by daemon. */
static void consume_mailbox(const char *house) {
    char dir[PBUF], path[PBUF];
    clocks_dir(dir, sizeof(dir), house);
    join3(path, sizeof(path), dir, "endturn.txt", "");
    int fd = open(path, O_RDWR | O_CREAT, 0644);
    if (fd < 0) return;
    flock(fd, LOCK_EX);
    char buf[64 * 1024];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n < 0) n = 0;
    buf[n] = '\0';
    if (n > 0) {
        char *save = NULL;
        for (char *line = strtok_r(buf, "\n", &save); line; line = strtok_r(NULL, "\n", &save))
            apply_command(house, dir, line);
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
    static const char *roots[] = { "_.monads", "&.widgits", "&.hq-apps", "@.apps", NULL };
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

/* Run the attached event: fork + exec (no shell) + waitpid with a watchdog.
 * Returns "ok" | "fail" | "timeout". Timeout default 30 s (LC_CLOCK_EVENT_TIMEOUT_S
 * overrides, mainly for the harness); on timeout the whole process group is killed.
 * LC_CLOCK_EVENT_RUNNER=<bin> replaces `sh <muchi-pet>/ops/play_event.sh` and is
 * called as <bin> <pkg> <house> (harness hook; unset = production behavior). */
static const char *run_event(const char *house, const char *event) {
    char pkg[PBUF] = "";
    if (strncmp(event, "common:", 7) == 0) {
        snprintf(pkg, sizeof(pkg), "%s/common_events/%s", house, event + 7);
    } else if (strncmp(event, "clock:", 6) == 0) {
        snprintf(pkg, sizeof(pkg), "%s/#.desktop/clocks/%s", house, event + 6);
    } else {
        snprintf(pkg, sizeof(pkg), "%s", event);
    }
    char script[PBUF] = "";
    const char *runner = getenv("LC_CLOCK_EVENT_RUNNER");
    if (!runner || !runner[0]) {
        char muchi_pet_dir[PBUF];
        find_app_dir(house, "muchi-pet", muchi_pet_dir, sizeof(muchi_pet_dir));
        snprintf(script, sizeof(script), "%s/ops/play_event.sh", muchi_pet_dir);
    }
    long tmo = 30;
    const char *te = getenv("LC_CLOCK_EVENT_TIMEOUT_S");
    if (te && atol(te) > 0) tmo = atol(te);

    pid_t pid = fork();
    if (pid < 0) return "fail";
    if (pid == 0) {
        setpgid(0, 0);
        int dn = open("/dev/null", O_RDWR);
        if (dn >= 0) { dup2(dn, 0); dup2(dn, 1); dup2(dn, 2); if (dn > 2) close(dn); }
        if (script[0]) execl("/bin/sh", "sh", script, pkg, house, (char *)NULL);
        else execl(runner, runner, pkg, house, (char *)NULL);
        _exit(127);
    }
    setpgid(pid, pid); /* also from the parent, closing the exec race */
    long long deadline = mono_ms_now() + tmo * 1000LL;
    int st = 0;
    for (;;) {
        pid_t w = waitpid(pid, &st, WNOHANG);
        if (w == pid) return (WIFEXITED(st) && WEXITSTATUS(st) == 0) ? "ok" : "fail";
        if (w < 0) return "fail";
        if (mono_ms_now() >= deadline) {
            kill(-pid, SIGKILL);
            kill(pid, SIGKILL);
            waitpid(pid, &st, 0);
            return "timeout";
        }
        usleep(10000);
    }
}

/* The daemon fires a reminder: popup (detached, it is a UI) + the attached event.
 * LC_CLOCK_NO_POPUP=1 skips the popup (harness/scratch runs). Returns the event result. */
static const char *fire_reminder(const char *house, const Reminder *r, int show_popup) {
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

    const char *np = getenv("LC_CLOCK_NO_POPUP");
    char lc_dir[PBUF] = "", popup_bin[PBUF] = "";
    if (show_popup && !(np && np[0] == '1')) {
        find_app_dir(house, "livedesk-clock", lc_dir, sizeof(lc_dir));
        snprintf(popup_bin, sizeof(popup_bin), "%s/ops/+x/lc_reminder_popup.+x", lc_dir);
    }
    /* never spawn a shell for a popup binary that is not there (scratch roots, unbuilt tree) */
    if (popup_bin[0] && access(popup_bin, X_OK) == 0) {
        char sh[PBUF * 2];
        /* house window standard: X11 RGB window + CSS (khtpm_css_parser),
         * launched detached exactly like db-hq/events-hq/context-menu open
         * (setsid nohup <bin> <house> <payload>) — NOT a GL window. */
        snprintf(sh, sizeof(sh), "setsid nohup '%s' '%s' '%s' >/dev/null 2>&1 &",
                 popup_bin, house, msgpath);
        int rc = system(sh);
        (void)rc;
    }

    /* run the attached event (.pal/events only, per user decision R5) */
    if (r->event[0]) return run_event(house, r->event);
    return "ok";
}

static void poll_reminders(const char *house) {
    Reminder list[MAX_REMINDERS];
    int n = reminders_load(house, list, MAX_REMINDERS);
    int changed = 0;
    long long wall = wall_ms_now();
    ledger_refresh(house);
    for (int i = 0; i < n; i++) {
        Reminder *r = &list[i];
        if (!r->enabled) continue;
        Repeat rp;
        int repeating = r->repeat[0] && parse_repeat(r->repeat, &rp) == 0;
        if (!repeating && r->fired_ms != 0) continue;
        long long cur = wall;
        if (strcmp(r->clock, "realclock") != 0) {
            char dir[PBUF], state[PBUF];
            clocks_dir(dir, sizeof(dir), house);
            join3(state, sizeof(state), dir, r->clock, ".pdl");
            if (access(state, F_OK) != 0) continue; /* clock deleted */
            cur = read_kv_ll(state, "game_time_epoch_ms", 0);
        }
        long long last;
        int have = ledger_cursor(r->id, &last);

        if (!repeating) { /* legacy one-shot: fired= is the state; the ledger only guards replay */
            if (cur < r->at_ms) continue;
            r->fired_ms = wall;
            changed = 1;
            if (have && last >= r->at_ms) continue; /* already ledgered (stale reminders.pdl): never twice */
            const char *res = fire_reminder(house, r, 1);
            ledger_append(house, r->id, r->at_ms, wall, res);
            continue;
        }

        long long limit = cur;
        if (r->until_ms > 0 && r->until_ms < limit) limit = r->until_ms;
        long long kmax = rep_kmax(r->at_ms, &rp, limit);
        long long k0 = have ? rep_kmax(r->at_ms, &rp, last) + 1 : 0;
        long long missed = kmax - k0 + 1;
        if (missed <= 0) continue;
        long long fire_n = missed > CATCHUP_CAP ? CATCHUP_CAP : missed;
        for (long long j = 0; j < fire_n; j++) {
            long long occ = rep_occ(r->at_ms, &rp, k0 + j);
            /* one popup per batch (the last fired), events run for every occurrence */
            const char *res = fire_reminder(house, r, j == fire_n - 1);
            ledger_append(house, r->id, occ, wall_ms_now(), res);
        }
        if (missed > fire_n) {
            long long collapsed = missed - fire_n;
            char res[64];
            snprintf(res, sizeof(res), "caught up %lld", collapsed);
            ledger_append(house, r->id, rep_occ(r->at_ms, &rp, kmax), wall_ms_now(), res);
            flag_append(house, r->id, collapsed, wall_ms_now());
        }
        r->fired_ms = wall;
        changed = 1;
    }
    if (changed) reminders_save(house, list, n);
}

/* advance every running clock by `elapsed` real ms at its rate (daemon ticker body) */
static void advance_clocks(const char *house, long long elapsed) {
    char dir[PBUF];
    clocks_dir(dir, sizeof(dir), house);
    ClockRow rows[MAX_CLOCKS];
    int n = rows_load(house, rows, MAX_CLOCKS);
    for (int i = 0; i < n; i++) {
        if (rows[i].parent[0]) continue; /* chained: derived from its parent in derive_children, never advanced here */
        char state[PBUF];
        join3(state, sizeof(state), dir, rows[i].id, ".pdl");
        if (access(state, F_OK) != 0) continue;
        int running = read_kv_int(state, "running", 1);
        char rate[32] = "off";
        read_kv_str(state, "rate", rate, sizeof(rate));
        double xr = 0.0;
        double mult = parse_rate(rate, &xr) == 0 ? xr / 10.0 : 0.0;
        if (running && mult > 0.0 && elapsed > 0) {
            long long ms = read_kv_ll(state, "game_time_epoch_ms", 0);
            long long old_min = ms / 60000LL;
            double delta_game_cs = (double)elapsed * mult;
            long long delta_game_ms = rate[0] == 'x' ? round_ll((double)elapsed * xr) : (long long)(delta_game_cs * 10.0);
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
}

/* master_get: 0 = exactly one master (id in out), 1 = none, 2 = more than one (ids joined by ',' in all) */
static int master_get(const char *house, char *out, size_t sz, char *all, size_t asz) {
    ClockRow rows[MAX_CLOCKS];
    int n = rows_load(house, rows, MAX_CLOCKS), cnt = 0;
    if (all && asz) all[0] = '\0';
    for (int i = 0; i < n; i++) {
        if (!rows[i].master) continue;
        if (cnt == 0) snprintf(out, sz, "%s", rows[i].id);
        if (all && asz && strlen(all) + strlen(rows[i].id) + 2 < asz) { if (all[0]) strcat(all, ","); strcat(all, rows[i].id); }
        cnt++;
    }
    return cnt == 1 ? 0 : (cnt == 0 ? 1 : 2);
}

/* control plane: lc_clock <root> cmd <clock> <verb> [arg] [--source S]. Only queues the command in the mailbox; the daemon
 * pass applies it (limits checked there) and writes the audit row. A malformed request is refused here, also audited. */
static int cmd_clock_command(const char *house, int argc, char **argv) {
    if (argc < 5) return 1;
    const char *clock = argv[3], *verb = argv[4];
    const char *src = "ctl";
    const char *arg = "";
    for (int i = 5; i < argc; i++) {
        if (strcmp(argv[i], "--source") == 0 && i + 1 < argc) { src = argv[++i]; }
        else if (!arg[0]) arg = argv[i];
    }
    int need = !strcmp(verb, "rate") || !strcmp(verb, "advance") || !strcmp(verb, "settime") || !strcmp(verb, "endturn") ? 1 : 0;
    int known = need || !strcmp(verb, "pause") || !strcmp(verb, "resume");
    if (!known) { audit_append(house, clock, verb, arg, "refused:unknown-verb", src); fprintf(stderr, "unknown verb: %s\n", verb); return 1; }
    if (need && strcmp(verb, "endturn") != 0 && !arg[0]) { audit_append(house, clock, verb, "", "refused:usage", src); fprintf(stderr, "%s needs an argument\n", verb); return 1; }
    char s[256], line[600];
    snprintf(s, sizeof(s), "%s", src);
    for (char *p = s; *p; p++) if (*p == ' ' || *p == '|' || *p == '\n' || *p == '\r') *p = '_';
    char a[160];
    snprintf(a, sizeof(a), "%s", arg);
    for (char *p = a; *p; p++) if (*p == ' ' || *p == '@' || *p == '\n' || *p == '\r') *p = '_';
    snprintf(line, sizeof(line), "%s %s%s%s @%s", verb, clock, a[0] ? " " : "", a, s);
    mailbox_put(house, line);
    return 0;
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

        advance_clocks(house, elapsed);

        consume_mailbox(house);
        derive_children(house);
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
        "  cmd <id> rate <off|cent|sec|min|hour|day|x<ratio>> | pause | resume | advance <ms|Ns|Nm|Nh|Nd> | settime <ms>  [--source S]\n"
        "                                  queue a clock command (the daemon pass applies it under the clock's limits and audits it)\n"
        "  limit <id> min_rate|max_rate <rate|none> | allow_settime_back|allow_commands <0|1>\n"
        "  chain <child> <parent> <ratio> | unchain <child>   derive a clock from a parent (epoch = parent*ratio + offset)\n"
        "  master [id]                     print / set the one master clock (the displayed one); gamedate master reads it\n"
        "  info <id>                       parent|ratio|master|child_offset|limits\n"
        "  reminder-add <clock> <when> <event> [note] [repeat] [until_ms]\n"
        "                                  when: HH:MM | +N[smhd] | ms | now; repeat: every:<n><min|hour|day|week|month|year>\n"
        "  step <real_ms>                  one deterministic pass (ticker for real_ms, mailbox, reminders); no daemon, no sleep\n"
        "  sched-count <rid> [prefix]      count schedule-ledger rows of a reminder (result starting with prefix)\n"
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
            char rate[32] = "off", scope[64] = "";
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
        char state[PBUF], mid[128];
        const char *gid = argv[3];
        if (strcmp(gid, "master") == 0) { /* the display reads the master clock */
            if (master_get(house, mid, sizeof(mid), NULL, 0) != 0) { fprintf(stderr, "no single master clock\n"); return 1; }
            gid = mid;
        }
        join3(state, sizeof(state), dir, gid, ".pdl");
        if (access(state, F_OK) != 0) { fprintf(stderr, "no such clock: %s\n", gid); return 1; }
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
    if (strcmp(cmd, "cmd") == 0) return cmd_clock_command(house, argc, argv);
    if (strcmp(cmd, "limit") == 0) { /* limit <clock> min_rate|max_rate|allow_settime_back|allow_commands <value|none> */
        if (argc < 6) return 1;
        char state[PBUF], key[64], val[64];
        double xv;
        join3(state, sizeof(state), dir, argv[3], ".pdl");
        if (access(state, F_OK) != 0) return 1;
        snprintf(val, sizeof(val), "%s", strcmp(argv[5], "none") == 0 ? "" : argv[5]);
        if (!strcmp(argv[4], "min_rate") || !strcmp(argv[4], "max_rate")) {
            if (val[0] && parse_rate(val, &xv) != 0) { fprintf(stderr, "bad rate\n"); return 1; }
        } else if (!strcmp(argv[4], "allow_settime_back") || !strcmp(argv[4], "allow_commands")) {
            if (strcmp(val, "0") && strcmp(val, "1")) { fprintf(stderr, "value must be 0 or 1\n"); return 1; }
        } else { fprintf(stderr, "unknown limit key\n"); return 1; }
        snprintf(key, sizeof(key), "%s%s", (argv[4][0] == 'a') ? "" : "limit_", argv[4]);
        write_kv(state, key, val);
        audit_append(house, argv[3], "limit", argv[4], val[0] ? val : "none", "ctl");
        return 0;
    }
    if (strcmp(cmd, "chain") == 0 || strcmp(cmd, "unchain") == 0) { /* chain <child> <parent> <ratio> | unchain <child> */
        int is_chain = cmd[0] == 'c';
        if (argc < (is_chain ? 6 : 4)) return 1;
        char err[256] = "", cstate[PBUF], pstate[PBUF];
        join3(cstate, sizeof(cstate), dir, argv[3], ".pdl");
        if (access(cstate, F_OK) != 0) { fprintf(stderr, "no such clock: %s\n", argv[3]); return 1; }
        if (is_chain) {
            join3(pstate, sizeof(pstate), dir, argv[4], ".pdl");
            if (access(pstate, F_OK) != 0) { fprintf(stderr, "no such parent clock: %s\n", argv[4]); return 1; }
        }
        if (rows_edit(house, is_chain ? 'c' : 'u', argv[3], is_chain ? argv[4] : NULL, is_chain ? argv[5] : NULL, err, sizeof(err)) != 0) {
            fprintf(stderr, "%s\n", err);
            audit_append(house, argv[3], cmd, is_chain ? argv[4] : "", "refused", "ctl");
            return 1;
        }
        if (is_chain) { /* keep the child's current epoch: offset = epoch - parent*ratio, so chaining never jumps time */
            long long pe = read_kv_ll(pstate, "game_time_epoch_ms", 0);
            long long ce = read_kv_ll(cstate, "game_time_epoch_ms", 0);
            write_kv_ll(cstate, "child_offset", ce - round_ll((double)pe * atof(argv[5])));
        }
        audit_append(house, argv[3], cmd, is_chain ? argv[4] : "", "ok", "ctl");
        return 0;
    }
    if (strcmp(cmd, "master") == 0) { /* master [id]: print the master, or make <id> the one master */
        char mid[128], err[256] = "";
        if (argc >= 4) {
            if (!argv[3][0] || !strcmp(argv[3], "none")) { fprintf(stderr, "refused: there must always be exactly one master\n"); return 1; }
            if (rows_edit(house, 'm', argv[3], NULL, NULL, err, sizeof(err)) != 0) { fprintf(stderr, "%s\n", err); return 1; }
            audit_append(house, argv[3], "master", "", "ok", "ctl");
            printf("%s\n", argv[3]);
            return 0;
        }
        char all[256];
        int rc = master_get(house, mid, sizeof(mid), all, sizeof(all));
        if (rc == 1) { fprintf(stderr, "no master clock\n"); return 1; }
        if (rc == 2) { fprintf(stderr, "ambiguous: more than one master (%s)\n", all); return 2; }
        printf("%s\n", mid);
        return 0;
    }
    if (strcmp(cmd, "info") == 0) { /* info <clock>: id|parent|ratio|master|child_offset|limit_min|limit_max|allow_back|allow_commands */
        if (argc < 4) return 1;
        ClockRow rows[MAX_CLOCKS];
        int rn = rows_load(house, rows, MAX_CLOCKS);
        int ri = row_find(rows, rn, argv[3]);
        if (ri < 0) return 1;
        char state[PBUF], mn[64], mx[64];
        join3(state, sizeof(state), dir, argv[3], ".pdl");
        read_kv_str(state, "limit_min_rate", mn, sizeof(mn));
        read_kv_str(state, "limit_max_rate", mx, sizeof(mx));
        printf("%s|%s|%s|%d|%lld|%s|%s|%d|%d\n", rows[ri].id, rows[ri].parent, rows[ri].ratio, rows[ri].master,
               read_kv_ll(state, "child_offset", 0), mn, mx, read_kv_int(state, "allow_settime_back", 0),
               read_kv_int(state, "allow_commands", 1));
        return 0;
    }
    if (strcmp(cmd, "reminder-add") == 0) {
        if (argc < 6) return 1;
        const char *note = argc >= 7 ? argv[6] : "";
        const char *rep = argc >= 8 ? argv[7] : "";
        long long until = argc >= 9 ? atoll(argv[8]) : 0;
        if (add_reminder(house, argv[3], argv[4], argv[5], note, rep, until) != 0) {
            fprintf(stderr, "reminder-add failed\n");
            return 1;
        }
        return 0;
    }
    if (strcmp(cmd, "reminder-del") == 0) {
        if (argc < 4) return 1;
        return del_reminder(house, argv[3]) == 0 ? 0 : 1;
    }
    if (strcmp(cmd, "step") == 0) { /* deterministic one pass; use only with no daemon running on this root */
        if (argc < 4) return 1;
        advance_clocks(house, atoll(argv[3]));
        consume_mailbox(house);
        derive_children(house);
        poll_reminders(house);
        return 0;
    }
    if (strcmp(cmd, "sched-count") == 0) {
        if (argc < 4) return 1;
        const char *pre = argc >= 5 ? argv[4] : "";
        char lp[PBUF];
        ledger_path(lp, sizeof(lp), house);
        FILE *lf = fopen(lp, "r");
        long long cnt = 0;
        char ln[MAX_LINE];
        while (lf && fgets(ln, sizeof(ln), lf)) {
            char id[64], res[128]; long long a, b;
            ln[strcspn(ln, "\r\n")] = '\0';
            if (sscanf(ln, "SCHED|%63[^|]|%lld|%lld|%127[^\n]", id, &a, &b, res) == 4 &&
                strcmp(id, argv[3]) == 0 && strncmp(res, pre, strlen(pre)) == 0) cnt++;
        }
        if (lf) fclose(lf);
        printf("%lld\n", cnt);
        return 0;
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
