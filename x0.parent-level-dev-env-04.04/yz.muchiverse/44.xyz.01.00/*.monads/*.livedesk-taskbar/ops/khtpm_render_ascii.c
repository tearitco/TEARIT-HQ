/* khtpm_render_ascii.c — GENERIC terminal presenter for any khtpm window.
 *
 * The strip's khtpm_strip_render_ascii.c, templated on a PID instead of
 * hardcoded strip_ascii_* filenames (TERMINAL-MIRROR-PARITY-all-
 * windows.md step 2). khtpm_core_render.c's kh_write_ascii_frame() drops
 *   #.desktop/ascii_frames/<pid>.frame.txt   - the readable frame
 *   #.desktop/ascii_frames/<pid>.pulse.txt   - DIAMOND marker (grows/frame)
 * for every non-dock window it renders. This process watches the pulse
 * marker's SIZE (append-only, monotonic - never mtime, no clock race)
 * and reprints the frame with explicit "\r\n" + screen-clear.
 *
 * NO termios, NO stdin - same renderer/keyboard split as the strip (raw
 * mode clears OPOST and would staircase this process's own output; see
 * khtpm_strip_render_ascii.c's header for the full incident).
 *
 * Usage: khtpm_render_ascii.+x <house_root> <pid>
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_LINE 1024

static char g_frame_path[PATH_BUF];
static char g_pulse_path[PATH_BUF];
static int  g_target_pid = 0;

static void draw(void) {
    FILE *f = fopen(g_frame_path, "r");
    fputs("\033[H\033[2J", stdout);
    if (!f) {
        fputs("(waiting for the window to paint...)\r\n", stdout);
        fflush(stdout);
        return;
    }
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        size_t n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = '\0';
        printf("%s\r\n", line);
    }
    fclose(f);
    fflush(stdout);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <house_root> <pid>\n", argv[0]);
        return 1;
    }
    signal(SIGPIPE, SIG_IGN);
    const char *house = argv[1];
    g_target_pid = atoi(argv[2]);
    snprintf(g_frame_path, sizeof(g_frame_path), "%s/#.desktop/ascii_frames/%d.frame.txt", house, g_target_pid);
    snprintf(g_pulse_path, sizeof(g_pulse_path), "%s/#.desktop/ascii_frames/%d.pulse.txt", house, g_target_pid);

    draw();
    struct stat st;
    long last_marker = -1;
    if (stat(g_pulse_path, &st) == 0) last_marker = st.st_size;

    int missing_ticks = 0;
    while (1) {
        if (stat(g_pulse_path, &st) == 0) {
            missing_ticks = 0;
            if (st.st_size < last_marker) {          /* rotated - resync */
                last_marker = st.st_size;
            } else if (st.st_size > last_marker) {
                last_marker = st.st_size;
                draw();
            }
        } else {
            /* window gone: its atexit unlinked the pair. Give it a
             * moment (a rotate races a stat), then exit cleanly so the
             * hosting terminal closes. */
            if (++missing_ticks > 120) {             /* ~2s */
                fputs("\033[H\033[2J(window closed)\r\n", stdout);
                fflush(stdout);
                return 0;
            }
        }
        usleep(16667);   /* 60Hz, TPMOS renderer.c cadence */
    }
    return 0;
}
