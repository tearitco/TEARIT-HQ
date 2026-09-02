/* renderer - standalone frame display, no ncurses.
 *
 * Polls renderer_pulse.txt SIZE (never mtime). On growth, shows
 * current_frame.txt. Always appends to frame_history.txt for offline
 * audit (K3). Exits when quit_flag.txt becomes non-empty.
 *
 * MARKER: renderer_pulse.txt only (TPMOS / mutaclsym precedent).
 *
 * SKIP-IF-UNCHANGED: skip live print + history append when content
 * matches last printed (CR-normalized).
 *
 * --- Windows terminal "scroll tear" fix (2026-08-06) ---
 * Real 1.TPMOS Windows path does NOT thrash the console with history
 * scroll for live view. In pieces/chtpm/plugins/orchestrator.c
 * render_thread_func() under _WIN32:
 *   - history off  -> system("cls"); then printf frame
 *   - history on   -> optional blank lines (also not a separate process)
 * And windows_renderer.c is built as renderer.+x with Console API
 * clear for static mode. Live play on Windows uses CLEAR + REDRAW so
 * the console does not "tear" through multi-page scroll; FILE history
 * still accumulates in frame_history.txt (and TPMOS session_frame_history).
 *
 * So on _WIN32 this process always clear_screen() for the live tty,
 * always appends frame_history.txt. Linux keeps history-on scroll vs
 * history-off ANSI clear (pieces/display/state.txt). */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#define usleep(us) Sleep((DWORD)((us) / 1000))
#define getcwd _getcwd
#define access _access
#define F_OK 0
#else
#include <unistd.h>
#endif

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif
#define PATH_BUF (MAX_PATH + 256)
#define FRAME_CAP 8192

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
#ifdef _WIN32
    if (access("pieces", F_OK) == 0) {
        snprintf(project_root, sizeof(project_root), ".");
        return;
    }
#endif
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) { snprintf(project_root, sizeof(project_root), "%s", env); return; }
    if (!getcwd(project_root, sizeof(project_root))) snprintf(project_root, sizeof(project_root), ".");
}

static long file_size(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (long)st.st_size;
}

static void strip_cr_inplace(char *s) {
    char *r = s, *w = s;
    while (*r) {
        if (*r != '\r') *w++ = *r;
        r++;
    }
    *w = '\0';
}

/* Emit logical newlines as \r\n. Never double-CR when input is already CRLF. */
static void write_crlf(const char *s, FILE *out) {
    for (const char *p = s; *p; p++) {
        if (*p == '\r') {
            if (p[1] == '\n') {
                fputc('\r', out);
                fputc('\n', out);
                p++;
            }
            continue;
        }
        if (*p == '\n') {
            fputc('\r', out);
            fputc('\n', out);
            continue;
        }
        fputc(*p, out);
    }
}

#ifdef _WIN32
/* TPMOS windows_renderer.c / orchestrator Win render path: cls equivalent. */
static void clear_screen(void) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD coordScreen = {0, 0};
    DWORD cCharsWritten;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    DWORD dwConSize;

    if (hConsole == INVALID_HANDLE_VALUE) return;
    if (!GetConsoleScreenBufferInfo(hConsole, &csbi)) return;

    dwConSize = (DWORD)csbi.dwSize.X * (DWORD)csbi.dwSize.Y;
    FillConsoleOutputCharacterA(hConsole, ' ', dwConSize, coordScreen, &cCharsWritten);
    FillConsoleOutputAttribute(hConsole, csbi.wAttributes, dwConSize, coordScreen, &cCharsWritten);
    SetConsoleCursorPosition(hConsole, coordScreen);
}
#endif

/* Linux only: state.txt history on/off (TPMOS renderer.c). */
#ifndef _WIN32
static int is_history_on(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/display/state.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return 1;
    char line[256];
    int on = 1;
    if (fgets(line, sizeof(line), f)) {
        if (strstr(line, "off")) on = 0;
    }
    fclose(f);
    return on;
}
#endif

static void render_frame(void) {
    char frame_path[PATH_BUF], history_path[PATH_BUF];
    snprintf(frame_path, sizeof(frame_path), "%s/pieces/display/current_frame.txt", project_root);
    snprintf(history_path, sizeof(history_path), "%s/pieces/display/frame_history.txt", project_root);

    FILE *f = fopen(frame_path, "r");
    if (!f) return;
    char content[FRAME_CAP];
    size_t n = fread(content, 1, sizeof(content) - 1, f);
    content[n] = '\0';
    fclose(f);

    char compare_buf[FRAME_CAP];
    memcpy(compare_buf, content, n + 1);
    strip_cr_inplace(compare_buf);

    static char last_printed[FRAME_CAP] = "";
    static int have_last = 0;
    if (have_last && strcmp(compare_buf, last_printed) == 0) {
        return;
    }
    memcpy(last_printed, compare_buf, sizeof(last_printed));
    have_last = 1;

    time_t rawtime = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&rawtime));

#ifdef _WIN32
    /*
     * Windows live console: always clear + redraw (TPMOS orchestrator
     * _WIN32 render_thread_func uses system("cls") for non-scroll path;
     * windows_renderer header: "proper screen clearing").
     * File history below still records every distinct frame for K3.
     */
    clear_screen();
    write_crlf(content, stdout);
#else
    if (is_history_on()) {
        write_crlf("\n\n\n\n\n", stdout);
        char marker[128];
        snprintf(marker, sizeof(marker), "--- FRAME UPDATE at %s ---\n", timestamp);
        write_crlf(marker, stdout);
        write_crlf(content, stdout);
    } else {
        fputs("\033[H\033[J", stdout);
        write_crlf(content, stdout);
    }
#endif
    fflush(stdout);

    /* File-backed frame history (always) - K3 / audit, same idea as
     * TPMOS pieces/debug/frames/session_frame_history.txt */
    FILE *hf = fopen(history_path, "a");
    if (hf) {
        fprintf(hf, "\n--- FRAME at %s ---\n%s\n", timestamp, content);
        fclose(hf);
    }
}

static int quit_requested(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/quit_flag.txt", project_root);
    return file_size(path) > 0;
}

int main(void) {
    resolve_root();

    char renderer_pulse_path[PATH_BUF], frame_path[PATH_BUF];
    snprintf(renderer_pulse_path, sizeof(renderer_pulse_path), "%s/pieces/display/renderer_pulse.txt", project_root);
    snprintf(frame_path, sizeof(frame_path), "%s/pieces/display/current_frame.txt", project_root);

    char history_path[PATH_BUF];
    snprintf(history_path, sizeof(history_path), "%s/pieces/display/frame_history.txt", project_root);
    FILE *clear = fopen(history_path, "w");
    if (clear) fclose(clear);

    render_frame();
    long last_renderer_pulse = file_size(renderer_pulse_path);

    while (!quit_requested()) {
        long rm = file_size(renderer_pulse_path);
        if (rm != last_renderer_pulse) {
            last_renderer_pulse = rm;
            long prev = -1;
            for (int i = 0; i < 20; i++) {
                long cur = file_size(frame_path);
                if (cur == prev) break;
                prev = cur;
                usleep(2000);
            }
            render_frame();
        }
        usleep(33333); /* 30 fps cap */
    }
    return 0;
}
