/* renderer - standalone frame display, no ncurses.
 * Scrolls by default (history ON). Clear-screen only when state.txt says "off".
 * Polls marker file SIZE (never mtime). On growth, prints current_frame.txt.
 * Each frame appended to frame_history.txt for offline audit.
 * Exits when quit_flag.txt becomes non-empty. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#define MAX_PATH 4096
/* Room for MAX_PATH worth of project_root plus the longest relative
 * suffix this file appends, so gcc can prove snprintf can't truncate
 * (silences -Wformat-truncation instead of just being right by luck). */
#define PATH_BUF (MAX_PATH + 256)

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) { snprintf(project_root, sizeof(project_root), "%s", env); return; }
    if (!getcwd(project_root, sizeof(project_root))) snprintf(project_root, sizeof(project_root), ".");
}

static long file_size(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (long)st.st_size;
}

/* Write content to the terminal translating '\n' -> "\r\n" ourselves.
 * keyboard_input puts the shared tty into raw mode with OPOST cleared
 * (needed so it can read raw keys), and OPOST is a per-terminal-device
 * setting, not per-process - so this process cannot rely on the tty
 * auto-translating our bare newlines into carriage returns anymore.
 * Emitting \r\n explicitly makes rendering correct regardless of what
 * termios state any other process left the shared tty in. */
static void write_crlf(const char *s, FILE *out) {
    for (const char *p = s; *p; p++) {
        if (*p == '\n') fputc('\r', out);
        fputc(*p, out);
    }
}

/* Ported directly from real TPMOS's own pieces/display/renderer.c -
 * defaults ON (a missing/unreadable pieces/display/state.txt, or one
 * whose content doesn't literally contain "off", both mean history
 * mode is on), matching that file's own real default exactly rather
 * than inventing a different one. */
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

/* Real TPMOS's own renderer.c (1.TPMOS_c_+rmmp.0102.0028/pieces/
 * display/renderer.c) hardcodes exactly 5 blank lines between frames in
 * history mode - fine for its own short frames, but this project's
 * frames (e.g. the path-completion picker, 20-40+ lines) are routinely
 * taller than that, so 5 blank lines leaves the tail of the PREVIOUS
 * frame still visible above the new one in the same terminal viewport -
 * user-reported: "adding space so not to show 2 frames on one terminal
 * screen." Not a porting bug (the reference genuinely only does 5 fixed
 * lines - confirmed by direct read), just a gap the reference itself
 * never needed to close for its own shorter frames. Query the real
 * terminal height via TIOCGWINSZ and print that many blank lines
 * instead, so the gap alone is always enough to scroll the entire
 * previous frame out of the current viewport regardless of frame size -
 * a fixed fallback (50) covers the non-tty case (output redirected to a
 * file/pipe during testing), where ioctl has nothing to query. */
static int terminal_rows(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0) {
        return ws.ws_row;
    }
    return 50;
}

static void render_frame(void) {
    char frame_path[PATH_BUF], history_path[PATH_BUF];
    snprintf(frame_path, sizeof(frame_path), "%s/pieces/display/current_frame.txt", project_root);
    snprintf(history_path, sizeof(history_path), "%s/pieces/display/frame_history.txt", project_root);

    FILE *f = fopen(frame_path, "r");
    if (!f) return;
    char content[8192];
    size_t n = fread(content, 1, sizeof(content) - 1, f);
    content[n] = '\0';
    fclose(f);

    /* SKIP-IF-UNCHANGED (mass-refactor 2026-07-26, direct user request:
     * "keep scrolling. but only render new frame change from marker
     * file"). See 01.muchi-pals-🥚️-13.01/system/renderer.c's own,
     * more detailed comment for the full rationale. */
    static char last_printed[8192] = "";
    static int have_last = 0;
    if (have_last && strcmp(content, last_printed) == 0) {
        return;
    }
    memcpy(last_printed, content, sizeof(last_printed));
    have_last = 1;

    time_t rawtime = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&rawtime));

    if (is_history_on()) {
        /* Real TPMOS shape: no clear, no cursor reposition - each new
         * frame just prints into the terminal's own normal scrollback,
         * separated by blank lines + a marker so frames are visually
         * distinguishable when scrolling back through them. Gap sized to
         * the real terminal height (terminal_rows()), not a fixed 5
         * lines - see that function's own comment. */
        int rows = terminal_rows();
        for (int i = 0; i < rows; i++) write_crlf("\n", stdout);
        char marker[128];
        snprintf(marker, sizeof(marker), "--- FRAME UPDATE at %s ---\n", timestamp);
        write_crlf(marker, stdout);
        write_crlf(content, stdout);
    } else {
        /* Static UI mode - the OLD default, kept as an explicit opt-in
         * (pieces/display/state.txt containing "off") for anyone who
         * genuinely wants an in-place-redrawn screen instead of
         * scrollback. */
        fputs("\033[H\033[J", stdout);
        write_crlf(content, stdout);
    }
    fflush(stdout);

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

    /* Watch ONLY renderer_pulse.txt - the authoritative post-compose
     * marker (mass-refactor 2026-07-26, matching 101.mutaclsym). */
    char renderer_pulse_path[PATH_BUF], frame_path[PATH_BUF];
    snprintf(renderer_pulse_path, sizeof(renderer_pulse_path), "%s/pieces/display/renderer_pulse.txt", project_root);
    snprintf(frame_path, sizeof(frame_path), "%s/pieces/display/current_frame.txt", project_root);

    /* Clear the frame history log at the start of a new session, same
     * as TPMOS's renderer does - prevents old runs' frames piling up
     * under a fresh game's log. */
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
