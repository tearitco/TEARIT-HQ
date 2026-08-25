/* renderer - standalone frame display, no ncurses.
 *
 * REAL BUG, FIXED (muchi-pals, 2026-07-19, direct user report - "why
 * isn't it rendering every frame in terminal in a way that will scroll
 * up like forum-pal... this is part of the refactor we wanted
 * deliberately"): ported directly from pal-forum's own real fix (same
 * family, same original complaint - "instead of refreshing the screen
 * each time it would simply show old frame in terminal and allow me to
 * scroll up... fix the way things are rendered to read from frame the
 * way fuzz-op does in 1.tpmos"), not guessed. Real TPMOS's own
 * pieces/display/renderer.c does NOT clear the screen by default - its
 * own is_history_on() defaults ON (the flag file doesn't even need to
 * exist - a missing file also means "on"), and in that mode it prints 5
 * blank lines + a "--- FRAME UPDATE at <ts> ---" marker, then the frame
 * content PLAIN - no ANSI clear, no cursor repositioning at all, relying
 * entirely on the terminal's own normal scrollback. The clear-screen
 * path only exists as an explicit OFF-mode opt-out.
 *
 * SUPERSEDES this file's own earlier alternate-screen-buffer fix
 * (\033[?1049h/l) - that was a real, considered fix for a DIFFERENT,
 * earlier complaint (terminal artifacts bleeding through on launch),
 * but the alternate screen buffer has no visible scrollback in most
 * terminal emulators, which directly conflicts with THIS deliberate
 * design goal (scroll up through frame history) - removed.
 *
 * MARKER SIMPLIFIED TO SINGLE (renderer_pulse.txt only) - mass-refactor
 * 2026-07-26, matching 101.mutaclsym (canonical reference per direct
 * user decision: "mutaclsym is most advanced, and is flicker free so
 * use that as reference"). This file used to also watch
 * frame_changed.txt as a workaround for href-navigation transitions not
 * reliably pulsing renderer_pulse.txt. Verified against real TPMOS's own
 * pieces/chtpm/plugins/chtpm_parser.c: compose_frame() unconditionally
 * appends to renderer_pulse.txt at its single write point, and is called
 * on every href transition (not just keypress-driven recomposes) - this
 * project's own system/chtpm_parser_pal.c already calls compose_frame()
 * explicitly at both href call sites (process_key(), the ACTIVATE-menu
 * Enter branch and the top-level nav branch) since the "REAL BUG,
 * LIVE-CAUGHT" href/process-leak fix landed 2026-07-19, so the dual-
 * marker workaround here was already redundant before this cleanup -
 * renderer_pulse.txt alone is authoritative.
 *
 * Mirrors the real TPMOS pattern (pieces/display/renderer.c): polls a
 * pulse marker file's SIZE (never mtime), and on growth, prints
 * pieces/display/current_frame.txt to stdout - a normal cooked-mode
 * terminal write, no raw mode needed here (only keyboard_input needs
 * raw mode).
 *
 * Every frame it draws is also appended, with a timestamp, to
 * pieces/display/frame_history.txt - an auditable, plain-text log of
 * every frame the game has ever shown, so a frame can be verified by
 * reading a file instead of driving a pty.
 *
 * Self-contained: own root resolution, own constants, no shared
 * headers. Exits when pieces/system/quit_flag.txt becomes non-empty
 * (written by keyboard_input on 'q'). */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>

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
 * whose content doesn't literally contain "off", both mean history mode
 * is on), matching that file's own real default exactly rather than
 * inventing a different one. */
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
     * file"): renderer_pulse.txt growing doesn't guarantee the actual
     * CONTENT of current_frame.txt is different from what was last
     * printed - a spurious/duplicate pulse with identical content would
     * otherwise still scroll-print 5 blank lines + a new FRAME UPDATE
     * block for nothing, reading as flicker on fast menu nav where many
     * keypresses land on the same visible state. Compare against the
     * last content actually printed and skip entirely (no print, no
     * frame_history.txt entry) when nothing really changed. */
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
         * distinguishable when scrolling back through them. */
        write_crlf("\n\n\n\n\n", stdout);
        char marker[128];
        snprintf(marker, sizeof(marker), "--- FRAME UPDATE at %s ---\n", timestamp);
        write_crlf(marker, stdout);
        write_crlf(content, stdout);
    } else {
        /* In-place overwrite mode (RGB double-buffer principle applied
         * to terminal): move cursor to home, write new frame over old
         * content, then clear trailing lines from the previous frame.
         * No clear-screen step = no blank flash = no flicker.
         * The old frame is visible until the new one overwrites it,
         * exactly like OpenGL's back-buffer draw + swap. */
        fputs("\033[H", stdout);
        write_crlf(content, stdout);
        fputs("\033[J", stdout);
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

    /* Watch ONLY renderer_pulse.txt - the authoritative post-compose marker.
     * frame_changed.txt is NOT watched: it fires before chtpm composes
     * current_frame.txt, causing a stale render (Pitfall 18 class race). */
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
