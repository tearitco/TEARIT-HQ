/* renderer - standalone frame display, no ncurses.
 *
 * REAL BUG, FIXED 2026-07-19 (direct user report/request - "instead of
 * refreshing the screen each time it would simply show old frame in
 * terminal and allow me to scroll up... fix the way things are
 * rendered to read from frame the way fuzz-op does in 1.tpmos"):
 * every local copy of this file across the whole family (checked
 * directly - wsr-pal/pal-forum/pal-chain/zoo_0000 all had the same
 * shape) UNCONDITIONALLY cleared the screen (`\033[H\033[J`) on every
 * frame - real TPMOS's own `pieces/display/renderer.c` does NOT do
 * this by default. Its own `is_history_on()` defaults ON (the flag
 * file, `pieces/display/state.txt`, doesn't even need to exist - a
 * missing file also means "on"), and in that (default) mode it prints
 * 5 blank lines + a `--- FRAME UPDATE at <ts> ---` marker, then the
 * frame content PLAIN - no ANSI clear, no cursor repositioning at all,
 * relying entirely on the terminal's own normal scrollback. The
 * clear-screen path only exists as the OFF-mode fallback. Ported that
 * real shape here directly rather than guessing at one.
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

    char pulse_path[PATH_BUF];
    snprintf(pulse_path, sizeof(pulse_path), "%s/pieces/display/frame_changed.txt", project_root);

    /* REAL BUG FIX (user-reported "had to press Enter twice" - a
     * screen transition looked blank/stale until an UNRELATED
     * keypress fixed it): chtpm_parser_pal.c's own process_key()
     * grows pieces/display/frame_changed.txt on every KEYPRESS-driven
     * recompose - the ONLY marker this renderer used to watch. But a
     * module-driven recompose (chtpm noticing active_target_id changed
     * via a relayed GOTO: command, reloading ${piece_methods} for the
     * new screen on its own state_changed.txt-triggered reload path)
     * does NOT also grow frame_changed.txt - so the CORRECTED
     * current_frame.txt sat on disk unseen until an unrelated later
     * keypress happened to grow frame_changed.txt too.
     *
     * CORRECTION (found live via a brief black-flash regression in
     * mutaclsym after xyzos-standards.txt sec. 0's own synchronous-
     * dispatch fix - pieces/apps/player_app/state_changed.txt was the
     * WRONG marker to add here): that file is grown by a PROJECT'S OWN
     * compose op BEFORE chtpm has actually reloaded/recomposed anything
     * - a pre-trigger, not a completion signal. chtpm_parser_pal.c's
     * own internal compose_frame() (shared-ops/chtpm_parser_pal.c
     * ~line 2575) grows pieces/display/renderer_pulse.txt
     * UNCONDITIONALLY every time it runs, regardless of what triggered
     * it - THIS is the authoritative "the correct, chrome-composed
     * current_frame.txt is ready" signal chtpm itself already emits.
     * Watch this instead: it never fires early, and never fires for a
     * project's own intermediate/direct write (mutaclsym's own
     * compose_frame.c dual-writes current_frame.txt directly as part
     * of its own turn dispatch - sec. 0.1/16.3 - which used to get
     * caught by the state_changed.txt watch and rendered as a visible
     * flash before chtpm's own correction landed). */
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
    long last_marker = file_size(pulse_path);
    long last_renderer_pulse = file_size(renderer_pulse_path);

    while (!quit_requested()) {
        long m = file_size(pulse_path);
        long rm = file_size(renderer_pulse_path);
        if (m != last_marker) {
            last_marker = m;
            render_frame();
        } else if (rm != last_renderer_pulse) {
            last_renderer_pulse = rm;
            /* Settle: chtpm's own recompose may still be in progress the
             * instant this marker grows - wait for current_frame.txt's
             * own size to read the same twice in a row (capped, so a
             * genuinely stuck write can't hang this loop forever). */
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
