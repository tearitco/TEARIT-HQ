/* khtpm_strip_render_ascii.c — terminal ASCII renderer for the taskbar,
 * matching TPMOS's real pieces/display/renderer.c AS CLOSELY AS POSSIBLE:
 * NO termios, NO raw mode, NO stdin reading at all - a plain cooked-mode
 * stdout printer, nothing else.
 *
 * REAL BUG FIX 2026-08-18, direct user report ("its still staggered, and
 * i dont see '>' nav in the staggered brackets" - a real terminal
 * screenshot showed every line progressively MORE indented than the last,
 * a classic staircase pattern): the PREVIOUS version of this feature
 * (khtpm_strip_renderer_ascii.c, now retired) combined rendering AND raw-
 * termios keyboard input in ONE process. enable_raw_mode() there disabled
 * OPOST (matching keyboard_input.c's own real flags) - and termios raw
 * mode is a property of the TTY ITSELF, not the calling process, so once
 * OPOST was cleared, EVERY printf("\n") to that same terminal (including
 * this same process's own frame output) stopped auto-translating to
 * "\r\n" - each bare \n just moved down a line WITHOUT returning to
 * column 0, producing exactly the observed staircase. Direct user
 * question that found the real root cause: "it should display from a
 * .txt frame file like tpmos. didn't u see that in tpmos render frame
 * pipeline?" - TPMOS's real renderer.c (confirmed via direct read) NEVER
 * touches termios at all; only keyboard_input.c does, and it never
 * prints. Splitting into two binaries here isn't a style choice, it's
 * what actually eliminates the bug's root cause: this file's stdout is
 * NEVER shared with a raw-mode-configured tty from its own code, and even
 * though it inherits the SAME tty keyboard_input.c puts into raw mode
 * when run alongside it (open_cli.sh launches both against one terminal,
 * same as mutaclysm's button.sh), TPMOS's own real precedent proves that
 * shape works correctly in practice - so this file additionally emits
 * "\r\n" explicitly (belt-and-suspenders, cheap insurance against the
 * shared-tty raw-mode case) rather than relying on OPOST at all.
 *
 * Reads:
 *   - #.desktop/strip_frame.cells.pdl          (per-cell frame data from
 *     the parser, the single source of truth for what to display)
 *   - #.desktop/strip_cells_changed.txt        (size-poll redraw trigger,
 *     dedicated to cells.pdl only — NOT the same marker the parser's own
 *     frame_changed_dirty() watches; that collision was a real bug, fixed
 *     2026-08-19, see khtpm_strip_parser.c's flush_cells_pdl())
 * Writes:
 *   - #.desktop/strip_ascii_current_frame.txt  (the composed frame TEXT,
 *     a real .txt frame file, matching TPMOS's own current_frame.txt
 *     convention)
 *   - #.desktop/strip_ascii_frame_history.txt  (auditable, timestamped
 *     history of every frame drawn, matching TPMOS's real renderer.c)
 *
 * Frame unification (2026-08-19): this renderer no longer composes its
 * own frame from livedesk_taskbar.pdl + strip_state.txt. It reads the
 * parser-produced strip_frame.cells.pdl and prints each cell's ch field.
 * The parser is the single compositor; this renderer is a dumb consumer. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_LINE 512
#define CELL_CH_MAX 192

static char house_root[MAX_PATH] = ".";

static void resolve_root(int argc, char **argv) {
    if (argc > 1 && argv[1][0]) snprintf(house_root, sizeof(house_root), "%s", argv[1]);
}

static void build_path(char *out, size_t out_sz, const char *rel) {
    snprintf(out, out_sz, "%s/%s", house_root, rel);
}

static int read_pipe_kv(const char *path, const char *want_tag, const char *want_key, char *out, size_t out_sz) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        char tag[64] = "", key[128] = "", val[MAX_LINE] = "";
        char *p = line;
        char *seg1 = strtok(p, "|");
        char *seg2 = strtok(NULL, "|");
        char *seg3 = strtok(NULL, "\n");
        if (!seg1 || !seg2) continue;
        snprintf(tag, sizeof(tag), "%s", seg1);
        snprintf(key, sizeof(key), "%s", seg2);
        if (seg3) snprintf(val, sizeof(val), "%s", seg3);
        char *t = tag; while (*t == ' ') t++; char *te = t + strlen(t); while (te > t && (te[-1] == ' ' || te[-1] == '\n')) *--te = '\0';
        char *k = key; while (*k == ' ') k++; char *ke = k + strlen(k); while (ke > k && (ke[-1] == ' ' || ke[-1] == '\n')) *--ke = '\0';
        char *v = val; while (*v == ' ') v++; char *ve = v + strlen(v); while (ve > v && (ve[-1] == ' ' || ve[-1] == '\n')) *--ve = '\0';
        if (strcmp(t, want_tag) == 0 && strcmp(k, want_key) == 0) { snprintf(out, out_sz, "%s", v); found = 1; }
    }
    fclose(f);
    return found;
}

static int read_pipe_kv_int(const char *path, const char *want_tag, const char *want_key, int def) {
    char buf[64];
    if (read_pipe_kv(path, want_tag, want_key, buf, sizeof(buf))) return atoi(buf);
    return def;
}

/* Composes into a growable buffer - explicit "\r\n" line endings
 * throughout (see this file's own header comment: cheap insurance against
 * a shared raw-mode tty, not relying on OPOST at all). */
static char g_buf[65536];
static size_t g_len = 0;

static void emit(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(g_buf + g_len, sizeof(g_buf) - g_len, fmt, ap);
    va_end(ap);
    if (n > 0) g_len += (size_t)n;
    if (g_len >= sizeof(g_buf)) g_len = sizeof(g_buf) - 1;
}

/* REAL BUG FIX 2026-08-18, direct user request ("i still dont see '>'"):
 * strip_focus_cell (KEY row in strip_state.txt, 0-based, KTB_STRIP_N_CELLS
 * = 15 wide) is the MANAGER's own real, published "which header cell
 * currently has focus" state - already correct and already updated by
 * this session's own KSC_FOCUS_LEFT/RIGHT relay-forwarding fix
 * (khtpm_strip_parser.c's dispatch_key_code(), same pass). Previously this
 * function never read it at all and every cell always rendered "[ ]" by
 * construction. */
/* Reads strip_frame.cells.pdl (produced by the parser) and prints each
 * cell's ch field. Regions are separated by visual dividers. Cells with
 * focused=1 are printed with [>] instead of [ ]. */
static void compose_frame(const char *ts) {
    g_len = 0;
    char fp[PATH_BUF];
    build_path(fp, sizeof(fp), "#.desktop/strip_ascii_current_frame.txt");

    /* 2026-09-06: the frame is now composed by khtpm_core_render.c
     * (dock_write_ascii_frame(), called from redraw()) - this binary is
     * just the terminal PRESENTER: read that readable text file and
     * re-emit every line with an explicit "\r\n" (raw-mode tty
     * insurance, this file's whole reason to exist), preceded by a
     * cursor-home + clear so the terminal doesn't scroll forever. */
    emit("\033[H\033[2J");
    FILE *f = fopen(fp, "r");
    if (!f) { emit("(waiting for the taskbar strip to paint...)\r\n"); (void)ts; return; }
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        size_t n = strlen(line);
        while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r')) line[--n] = '\0';
        emit("%s\r\n", line);
    }
    fclose(f);
    (void)ts;
}

static void write_frame_file(void) {
    /* no-op since 2026-09-06: khtpm_core_render.c is the sole writer of
     * strip_ascii_current_frame.txt now; this presenter must not race it. */
}

static void append_history(const char *ts) {
    (void)ts;
    char path[PATH_BUF];
    build_path(path, sizeof(path), "#.desktop/strip_ascii_frame_history.txt");
    FILE *f = fopen(path, "a");
    if (!f) return;
    fwrite(g_buf, 1, g_len, f);
    fclose(f);
}

static void clear_session_history(void) {
    char path[PATH_BUF];
    build_path(path, sizeof(path), "#.desktop/strip_ascii_frame_history.txt");
    FILE *f = fopen(path, "w");
    if (!f) return;
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char ts[64];
    if (tm_info) strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm_info);
    else snprintf(ts, sizeof(ts), "??");
    fprintf(f, "=== NEW SESSION at %s ===\r\n", ts);
    fclose(f);
}

static void render_display(void) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char ts[64];
    if (tm_info) strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm_info);
    else snprintf(ts, sizeof(ts), "??");

    compose_frame(ts);
    write_frame_file();
    fwrite(g_buf, 1, g_len, stdout);
    fflush(stdout);
    append_history(ts);
}

/* Matches TPMOS's real renderer.c main() exactly: draw once, THEN seed the
 * marker cursor to whatever the pulse file's size is AT THAT POINT - no
 * separate "first sight" branch, so the loop's own direct comparison
 * can't spuriously fire again right after. usleep(16667) (~60Hz) poll,
 * matching TPMOS's own real cadence, not an approximation. */
int main(int argc, char **argv) {
    resolve_root(argc, argv);
    clear_session_history();
    render_display();

    char pulse_path[PATH_BUF];
    /* BUG FIX 2026-08-19: renamed from strip_frame_changed.txt, which
     * collided with khtpm_strip_parser.c's own manager-state dirty signal
     * of the same name (see khtpm_strip_parser.c's flush_cells_pdl() for
     * the full story) — that collision was firing a doc-reload every tick
     * in the parser, breaking arrow-key submenu navigation. */
    build_path(pulse_path, sizeof(pulse_path), "#.desktop/strip_ascii_current_frame.txt");
    struct stat st;
    long last_sz = -1; long last_mt = 0;
    if (stat(pulse_path, &st) == 0) { last_sz = st.st_size; last_mt = (long)st.st_mtime; }

    while (1) {
        if (stat(pulse_path, &st) == 0) {
            if (st.st_size != last_sz || (long)st.st_mtime != last_mt) {
                render_display();
                last_sz = st.st_size; last_mt = (long)st.st_mtime;
            }
        }
        usleep(50000);
    }
    return 0;
}
