/* music_daemon - endless seeded background music loop (MUSIC-DAEMON v0, EDEN-PLAYABLE-LOOP plan section 4). Nothing starts it automatically.
 *
 * Usage: music_daemon [--dir DIR] [--ops OPDIR] [--sink file:PREFIX] [--once N] [--pace-ms M] [--poll-ms P]
 *   DIR     holds music_state.txt (input), music_ledger.txt (append-only output), music_daemon.pid, music_stop.flag and the data (mood.pdl, scales.pdl, instruments.pdl). default "."
 *   --data D  where mood.pdl/scales.pdl/instruments.pdl are (default DIR).
 *   OPDIR   where music_seq.+x and music_synth.+x are (default: DIR/ops/+x). The daemon runs them by fork+exec+waitpid (no system(), no popen): op + fork + IPC-by-file.
 *   --sink  headless mode: each phrase is written to PREFIX.<n>.wav and NO player is started (tests). Without it a player is chosen from PATH: paplay, then aplay, then ffplay.
 *   --once N  render/play N phrases then exit (default: loop until stopped).   --pace-ms  sleep between phrases in sink mode (default 200, so a sink loop does not spin).
 * music_state.txt (key=value lines): on, mood, volume, seed, time_of_day, weather. Change detection = file SIZE or CONTENT hash, never mtime. Garbage/missing file or values
 *   fall back to safe defaults (on=1 mood=calm volume=0.6 seed=1 day clear) - Heal; music_seq itself falls back to a built-in safe pattern for an unknown mood or a bad data file.
 *   Settings are re-read before each phrase, so a change is heard from the NEXT phrase (v0: no mid-phrase cut). Phrase n uses seed + n via music_seq --phrase n.
 * Stops cleanly when music_stop.flag exists (the daemon removes it), when on=0, on SIGTERM/SIGINT, or after --once N. Flag-file kill switch, never pkill -f.
 * Ledger rows: START|pid|sink-or-player   PHRASE|n|mood|seed|bytes   (HEAL|n|reason)   STOP|reason   (state rows are ignorable, PHRASE rows are the contract).
 * Runs itself at nice 15. Self-contained: libc only. Pattern of the player child (fork/exec/WNOHANG poll with kill on stop) from @.apps/music-player-hq/ops/music_player_manager.c
 * (it forks an mpg123 child the same way); the play-while-rendering-the-next-phrase pipeline is new. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/wait.h>

static char DIR[3000] = ".", DATA[3000] = "", OPS[3200], SINK[3000] = "";
static volatile sig_atomic_t g_sig = 0;
static void on_sig(int s) { (void)s; g_sig = 1; }
static void msleep(int ms) { struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L }; nanosleep(&ts, NULL); }
static void path_of(char *out, size_t n, const char *name) { snprintf(out, n, "%s/%s", DIR, name); }
static int exists(const char *p) { struct stat st; return stat(p, &st) == 0; }

static void ledger(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
#include <stdarg.h>
static void ledger(const char *fmt, ...) {
    char p[3100], row[1024]; va_list ap; va_start(ap, fmt); vsnprintf(row, sizeof row, fmt, ap); va_end(ap);
    path_of(p, sizeof p, "music_ledger.txt"); FILE *f = fopen(p, "a"); if (!f) return; fprintf(f, "%s\n", row); fclose(f);   /* append-only */
}

/* ---------- state ---------- */
typedef struct { int on; char mood[48], tod[24], wx[24]; float volume; char seed[40]; } State;
static void state_defaults(State *s) { s->on = 1; snprintf(s->mood, sizeof s->mood, "calm"); snprintf(s->tod, sizeof s->tod, "day"); snprintf(s->wx, sizeof s->wx, "clear"); s->volume = 0.6f; snprintf(s->seed, sizeof s->seed, "1"); }
static int safe_word(const char *v) { if (!*v) return 0; for (; *v; v++) if (!isalnum((unsigned char)*v) && *v != '_' && *v != '-') return 0; return 1; }
/* reads the whole file (<=64 KB) into buf; returns length or -1 when missing */
static long slurp(const char *p, char *buf, long cap) { FILE *f = fopen(p, "r"); if (!f) return -1; long n = (long)fread(buf, 1, cap - 1, f); fclose(f); buf[n] = 0; return n; }
static void state_parse(const char *text, State *s) {
    state_defaults(s); char line[256]; const char *p = text;
    while (*p) {
        size_t k = 0; while (*p && *p != '\n' && k < sizeof line - 1) line[k++] = *p++; line[k] = 0; if (*p == '\n') p++; else while (*p && *p != '\n') p++;
        char *eq = strchr(line, '='); if (!eq || line[0] == '#') continue; *eq = 0; char *key = line, *val = eq + 1;
        while (*val == ' ') val++; size_t vl = strlen(val); while (vl && isspace((unsigned char)val[vl - 1])) val[--vl] = 0;
        if (!strcmp(key, "on")) { if (!strcmp(val, "0")) s->on = 0; else if (!strcmp(val, "1")) s->on = 1; }
        else if (!strcmp(key, "mood") && safe_word(val)) snprintf(s->mood, sizeof s->mood, "%s", val);
        else if (!strcmp(key, "time_of_day") && safe_word(val)) snprintf(s->tod, sizeof s->tod, "%s", val);
        else if (!strcmp(key, "weather") && safe_word(val)) snprintf(s->wx, sizeof s->wx, "%s", val);
        else if (!strcmp(key, "volume")) { char *e; float v = strtof(val, &e); if (*val && !*e && v == v && v >= 0.0f) s->volume = v > 1.0f ? 1.0f : v; }
        else if (!strcmp(key, "seed") && safe_word(val)) snprintf(s->seed, sizeof s->seed, "%s", val);
    }
}

/* ---------- children ---------- */
static pid_t spawn(char *const argv[], int quiet) {
    pid_t pid = fork(); if (pid < 0) return -1;
    if (pid == 0) {
        if (quiet) { int dn = open("/dev/null", O_RDWR); if (dn >= 0) { dup2(dn, 0); dup2(dn, 1); dup2(dn, 2); if (dn > 2) close(dn); } }
        signal(SIGTERM, SIG_DFL); signal(SIGINT, SIG_DFL); execv(argv[0], argv); _exit(127);
    }
    return pid;
}
/* run to completion with a watchdog; returns exit code or -1 */
static int run_child(char *const argv[], int timeout_ms) {
    pid_t pid = spawn(argv, 1); if (pid < 0) return -1; int st = 0;
    for (int waited = 0; ; waited += 10) {
        pid_t r = waitpid(pid, &st, WNOHANG);
        if (r == pid) return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
        if (r < 0 && errno != EINTR) return -1;
        if (waited >= timeout_ms) { kill(pid, SIGKILL); waitpid(pid, &st, 0); return -1; }
        msleep(10);
    }
}
static int which(const char *prog, char *out, size_t n) {
    const char *path = getenv("PATH"); if (!path) path = "/usr/bin:/bin"; char tmp[4096]; snprintf(tmp, sizeof tmp, "%s", path);
    for (char *sv, *d = strtok_r(tmp, ":", &sv); d; d = strtok_r(NULL, ":", &sv)) { snprintf(out, n, "%s/%s", d, prog); if (access(out, X_OK) == 0) return 1; }
    return 0;
}
static int stop_requested(void) {
    char p[3100]; path_of(p, sizeof p, "music_stop.flag");
    return g_sig || exists(p);
}
/* wait for a player child; on stop kill it. returns 1 if stopped early */
static int wait_player(pid_t *pid) {
    int st;
    while (*pid > 0) {
        pid_t r = waitpid(*pid, &st, WNOHANG);
        if (r == *pid || (r < 0 && errno != EINTR)) { *pid = -1; return 0; }
        if (stop_requested()) { kill(*pid, SIGTERM); msleep(50); if (waitpid(*pid, &st, WNOHANG) != *pid) { kill(*pid, SIGKILL); waitpid(*pid, &st, 0); } *pid = -1; return 1; }
        msleep(50);
    }
    return 0;
}

int main(int argc, char **argv) {
    int once = 0, pace = 200, poll = 200; (void)poll;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--dir") && i + 1 < argc) snprintf(DIR, sizeof DIR, "%s", argv[++i]);
        else if (!strcmp(argv[i], "--data") && i + 1 < argc) snprintf(DATA, sizeof DATA, "%s", argv[++i]);
        else if (!strcmp(argv[i], "--ops") && i + 1 < argc) snprintf(OPS, sizeof OPS, "%s", argv[++i]);
        else if (!strcmp(argv[i], "--sink") && i + 1 < argc) { const char *s = argv[++i]; if (strncmp(s, "file:", 5)) { fprintf(stderr, "music_daemon: --sink needs file:<prefix>\n"); return 2; } snprintf(SINK, sizeof SINK, "%s", s + 5); }
        else if (!strcmp(argv[i], "--once") && i + 1 < argc) once = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--pace-ms") && i + 1 < argc) pace = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--poll-ms") && i + 1 < argc) poll = atoi(argv[++i]);
        else { fprintf(stderr, "music_daemon: bad arg %s\n", argv[i]); return 2; }
    }
    if (!DATA[0]) snprintf(DATA, sizeof DATA, "%s", DIR);
    if (!OPS[0]) snprintf(OPS, sizeof OPS, "%s/ops/+x", DIR);
    char seq[3300], syn[3300], pidp[3100], statep[3100], flagp[3100], player[300] = "", pname[16] = "";
    snprintf(seq, sizeof seq, "%s/music_seq.+x", OPS); snprintf(syn, sizeof syn, "%s/music_synth.+x", OPS);
    path_of(pidp, sizeof pidp, "music_daemon.pid"); path_of(statep, sizeof statep, "music_state.txt"); path_of(flagp, sizeof flagp, "music_stop.flag");
    if (access(seq, X_OK) || access(syn, X_OK)) { fprintf(stderr, "music_daemon: ops not found in %s\n", OPS); return 1; }
    if (!SINK[0]) {
        static const char *cands[][4] = { {"paplay", NULL}, {"aplay", "-q", NULL}, {"ffplay", "-nodisp", NULL} };
        for (int i = 0; i < 3; i++) if (which(cands[i][0], player, sizeof player)) { snprintf(pname, sizeof pname, "%s", cands[i][0]); break; }
        if (!pname[0]) { fprintf(stderr, "music_daemon: no player (paplay/aplay/ffplay) on PATH\n"); ledger("STOP|no-player"); return 2; }
    }
    if (exists(pidp)) { char b[64]; if (slurp(pidp, b, sizeof b) > 0) { pid_t old = (pid_t)atoi(b); if (old > 1 && old != getpid() && kill(old, 0) == 0) { fprintf(stderr, "music_daemon: already running (pid %d)\n", (int)old); return 3; } } }
    unlink(flagp);   /* a stale stop flag from a previous run must not kill a fresh start; stopping = write the flag AFTER the pid file appears */
    if (nice(15) < 0) { /* best effort */ }
    struct sigaction sa; memset(&sa, 0, sizeof sa); sa.sa_handler = on_sig; sigaction(SIGTERM, &sa, NULL); sigaction(SIGINT, &sa, NULL);
    { FILE *pf = fopen(pidp, "w"); if (pf) { fprintf(pf, "%d\n", (int)getpid()); fclose(pf); } }
    ledger("START|%d|%s", (int)getpid(), SINK[0] ? "sink" : pname);

    char tmpbase[3100]; snprintf(tmpbase, sizeof tmpbase, "%s/.music_tmp_%d", DIR, (int)getpid());
    char stext[65536], last_text[65536] = ""; long last_len = -2; State st; state_defaults(&st);
    pid_t player_pid = -1; int n = 0; const char *reason = "flag";
    for (;;) {
        if (stop_requested()) { reason = g_sig ? "signal" : "flag"; break; }
        if (once > 0 && n >= once) { reason = "once-done"; break; }
        long len = slurp(statep, stext, sizeof stext);      /* size/content change detection, never mtime */
        if (len != last_len || (len > 0 && strcmp(stext, last_text))) { if (len > 0) snprintf(last_text, sizeof last_text, "%s", stext); else last_text[0] = 0; last_len = len; state_parse(len > 0 ? stext : "", &st); }
        if (!st.on) { reason = "on=0"; break; }
        n++;
        char pat[3200], wav[3300], nstr[16], vstr[24]; snprintf(pat, sizeof pat, "%s.pat", tmpbase);
        if (SINK[0]) snprintf(wav, sizeof wav, "%s.%d.wav", SINK, n); else snprintf(wav, sizeof wav, "%s.%d.wav", tmpbase, n % 2);
        snprintf(nstr, sizeof nstr, "%d", n); snprintf(vstr, sizeof vstr, "%.3f", st.volume);
        char *sargv[] = { seq, "--data", DATA, "--mood", st.mood, "--seed", st.seed, "--tod", st.tod, "--weather", st.wx, "--phrase", nstr, "-o", pat, NULL };
        int rc = run_child(sargv, 20000);
        if (rc != 0) { ledger("HEAL|%d|seq-rc-%d-using-safe-pattern", n, rc); char *safeargv[] = { seq, "--data", DATA, "--safe", "--seed", st.seed, "--phrase", nstr, "-o", pat, NULL }; rc = run_child(safeargv, 20000); }
        char *yargv[] = { syn, "--data", DATA, "--volume", vstr, "-o", wav, pat, NULL };
        if (rc == 0) rc = run_child(yargv, 30000);
        unlink(pat);
        struct stat ws; long bytes = (rc == 0 && stat(wav, &ws) == 0) ? (long)ws.st_size : -1;
        if (bytes <= 44) { ledger("HEAL|%d|render-failed", n); if (!SINK[0]) unlink(wav); msleep(500); if (n > 50 && bytes <= 44) { reason = "render-failing"; break; } continue; }
        ledger("PHRASE|%d|%s|%s|%ld", n, st.mood, st.seed, bytes);
        if (SINK[0]) { if (once <= 0 || n < once) msleep(pace); continue; }
        if (wait_player(&player_pid)) { unlink(wav); reason = g_sig ? "signal" : "flag"; break; }   /* previous phrase finished (or stop) */
        char *pargv[8]; int k = 0; pargv[k++] = player;
        if (!strcmp(pname, "aplay")) pargv[k++] = "-q"; else if (!strcmp(pname, "ffplay")) { pargv[k++] = "-nodisp"; pargv[k++] = "-autoexit"; pargv[k++] = "-loglevel"; pargv[k++] = "quiet"; }
        pargv[k++] = wav; pargv[k] = NULL;
        player_pid = spawn(pargv, 1);
        /* the player is now playing phrase n; the loop renders n+1 meanwhile (other wav slot) */
    }
    if (player_pid > 0) {
        if (!strcmp(reason, "once-done")) wait_player(&player_pid);        /* let the last phrase finish (a stop flag still kills it) */
        else { kill(player_pid, SIGTERM); msleep(50); if (waitpid(player_pid, NULL, WNOHANG) != player_pid) { kill(player_pid, SIGKILL); waitpid(player_pid, NULL, 0); } }
    }
    for (int i = 0; i < 2; i++) { char w[3300]; snprintf(w, sizeof w, "%s.%d.wav", tmpbase, i); unlink(w); }
    unlink(flagp); unlink(pidp);        /* pid file goes BEFORE the STOP row, so STOP in the ledger means "fully stopped" */
    ledger("STOP|%s", reason);
    return 0;
}
