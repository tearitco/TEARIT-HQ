// keyboard_muscle.c for Exo-Bot v5
// REAL-TIME INJECTOR: Runs concurrently with TPMOS, unbuffered logging, minimal startup delay.
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdarg.h>

static char g_root[1024] = {0};
static volatile sig_atomic_t should_exit = 0;
void handle_sig(int s) { (void)s; should_exit = 1; }

// 🔒 Dual-channel logger: UNBUFFERED stderr + persistent file with fsync
void log_debug(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    // 1. Immediate terminal output (unbuffered)
    fprintf(stderr, "[MUSCLE] %s\n", buf);
    fflush(stderr);

    // 2. Persistent debug log
    char *log_path = NULL, *dir_path = NULL;
    if (asprintf(&log_path, "%s/xo/bot5/debug/muscle_log.txt", g_root) < 0 ||
        asprintf(&dir_path, "%s/xo/bot5/debug", g_root) < 0) {
        return;
    }

    mkdir(dir_path, 0755);
    FILE *f = fopen(log_path, "a");
    if (f) {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        char ts[64];
        strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", t);
        fprintf(f, "[%s] %s\n", ts, buf);
        fflush(f);
        fsync(fileno(f)); // ⚡ Force to disk immediately
        fclose(f);
    }
    free(log_path); free(dir_path);
}

void inject_key(int code) {
    char *hist_path = NULL;
    if (asprintf(&hist_path, "%s/pieces/keyboard/history.txt", g_root) < 0) return;

    FILE *f = fopen(hist_path, "a");
    if (!f) {
        log_debug("INJECT FAIL: Cannot open %s (%s)", hist_path, strerror(errno));
        free(hist_path);
        return;
    }

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", t);
    fprintf(f, "[%s] KEY_PRESSED: %d\n", ts, code);
    fflush(f);
    fsync(fileno(f)); // ⚡ Force to disk immediately
    fclose(f);
    log_debug("INJECT SUCCESS: Key %d -> %s", code, hist_path);
    free(hist_path);
}

int main(int argc, char *argv[]) {
    // Force stderr unbuffered so TPMOS stdout capture doesn't hide our logs
    setvbuf(stderr, NULL, _IONBF, 0);

    if (argc > 1) {
        strncpy(g_root, argv[1], sizeof(g_root)-1);
    } else {
        getcwd(g_root, sizeof(g_root));
    }
    if (chdir(g_root) != 0) return 1;

    log_debug("STARTUP: Root verified = %s", g_root);
    signal(SIGINT, handle_sig);
    signal(SIGTERM, handle_sig);

    typedef enum { S_START, S_WAIT, S_INJECT, S_HOLD } State;
    State st = S_START;

    while (!should_exit) {
        switch(st) {
            case S_START: log_debug("FSM: STARTUP"); st = S_WAIT; break;
            case S_WAIT:  log_debug("FSM: WAITING 2s for TPMOS parser init..."); sleep(2); st = S_INJECT; break;
            case S_INJECT:log_debug("FSM: INJECTING ENTER (13)"); inject_key(13); st = S_HOLD; break;
            case S_HOLD:  log_debug("FSM: HOLDING (Press Ctrl+C to exit)"); sleep(1); break;
        }
    }
    log_debug("SESSION END");
    return 0;
}
