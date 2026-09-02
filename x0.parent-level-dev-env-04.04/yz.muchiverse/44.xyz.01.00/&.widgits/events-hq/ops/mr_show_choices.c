/* mr_show_choices - "Show Choices" event command
 * Displays a REAL, VISIBLE, nav-accepting (arrow keys or digit-jump)
 * context-style popup on the player's own entity window, and blocks
 * until they pick one (or a timeout elapses).
 *
 * Usage: mr_show_choices.+x <package_dir> <choices_text> [default_index]
 *   choices_text: comma-separated list of options
 *   default_index: optional, used only if the popup times out with no
 *   real pick (0-based, defaults to 0)
 *
 * REAL FIX 2026-08-25 (direct instruction: "should be a context style,
 * nav accepting (index or arrow keys)"): this used to only ever WRITE a
 * line to messages.txt/history.txt - it never actually showed anything
 * on screen, and immediately "succeeded" with whatever default_index
 * was passed, without waiting for a real pick. Found while trying to
 * make a common event's Show Choices visibly distinguishable from an
 * entity's own Show Text in a live test.
 *
 * The real popup mechanism this now drives ALREADY EXISTED, built
 * 2026-08-05 in tp_desktop_window_rgb.c specifically with this command
 * in mind (see that file's own comments: "a real Show Choices pick -
 * write the chosen row's real action... to the result file the waiting
 * op is polling") - it had simply never been wired to any real caller
 * until now. Zero renderer changes needed: this reuses the SAME
 * SHOW_PAGE relay command / choice_mode / nav_claim_rows() machinery
 * an entity's own right-click context menu already uses, by writing a
 * plain flat "OBJECT | label=.. | action=.." file (load_flat_objects()'s
 * own documented shape) and a "SHOW_PAGE:<objects_file>|<result_file>"
 * line to interact_relay.txt.
 *
 * KNOWN, REAL LIMITATION carried over from common-events-into-Play
 * (see EVENTS_ROADMAP_NEXT_STEPS.md): package_dir for a COMMON event
 * is the common event's own directory (common_events/<name>/), not the
 * actual player entity being played - interact_relay.txt written there
 * would go nowhere, since no window process polls that directory. Fix:
 * play_event.sh exports MUCHI_CALLER_PKG=<the real playing entity's
 * dir> before running a common event's compiled script; this program
 * checks that env var and, if set, sends the VISIBLE popup to THAT
 * directory's interact_relay.txt instead of package_dir's - while still
 * logging/writing choice_result.txt into package_dir (the common
 * event's own directory), since that's where any future command in the
 * SAME script would look for it. For a normal (non-common) entity
 * event, MUCHI_CALLER_PKG is unset and package_dir is used for both,
 * unchanged from before.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>

#define PATH_BUF 4352
#define MAX_CHOICES 32
#define POLL_INTERVAL_US 250000
#define POLL_TIMEOUT_S 30

static int split_choices(const char *choices, char out[MAX_CHOICES][256]) {
    int n = 0;
    const char *start = choices;
    while (*start && n < MAX_CHOICES) {
        const char *comma = strchr(start, ',');
        size_t len = comma ? (size_t)(comma - start) : strlen(start);
        while (len > 0 && start[len - 1] == ' ') len--;
        while (*start == ' ') { start++; len = len > 0 ? len - 1 : 0; }
        if (len >= sizeof(out[0])) len = sizeof(out[0]) - 1;
        memcpy(out[n], start, len);
        out[n][len] = '\0';
        n++;
        if (!comma) break;
        start = comma + 1;
    }
    return n;
}

static void log_line(const char *package_dir, const char *fmt, ...) {
    char msg_path[PATH_BUF], hist_path[PATH_BUF], line[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);

    snprintf(msg_path, sizeof(msg_path), "%s/messages.txt", package_dir);
    FILE *mf = fopen(msg_path, "a");
    if (mf) { fprintf(mf, "[%ld] %s\n", (long)time(NULL), line); fclose(mf); }

    snprintf(hist_path, sizeof(hist_path), "%s/history.txt", package_dir);
    FILE *hf = fopen(hist_path, "a");
    if (hf) { fprintf(hf, "%s\n", line); fclose(hf); }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: mr_show_choices.+x <package_dir> <choices_text> [default_index]\n");
        return 1;
    }
    const char *package_dir = argv[1];
    const char *choices_text = argv[2];
    int default_idx = (argc > 3) ? atoi(argv[3]) : 0;

    const char *caller_pkg = getenv("MUCHI_CALLER_PKG");
    const char *popup_target_dir = (caller_pkg && caller_pkg[0]) ? caller_pkg : package_dir;

    char choices[MAX_CHOICES][256];
    int n_choices = split_choices(choices_text, choices);
    if (n_choices <= 0) {
        fprintf(stderr, "mr_show_choices: no choices parsed from %s\n", choices_text);
        return 1;
    }

    char objpath[PATH_BUF], respath[PATH_BUF];
    snprintf(objpath, sizeof(objpath), "%s/.show_choices_objects.tmp.pdl", package_dir);
    snprintf(respath, sizeof(respath), "%s/.show_choices_result.tmp.txt", package_dir);
    remove(respath);

    FILE *of = fopen(objpath, "w");
    if (!of) {
        fprintf(stderr, "mr_show_choices: cannot write %s\n", objpath);
        return 1;
    }
    for (int i = 0; i < n_choices; i++) {
        fprintf(of, "OBJECT | label=%s | action=%d\n", choices[i], i);
    }
    fclose(of);

    char relay_path[PATH_BUF];
    snprintf(relay_path, sizeof(relay_path), "%s/interact_relay.txt", popup_target_dir);
    FILE *rf = fopen(relay_path, "w");
    if (!rf) {
        fprintf(stderr, "mr_show_choices: cannot write %s\n", relay_path);
        return 1;
    }
    fprintf(rf, "SHOW_PAGE:%s|%s\n", objpath, respath);
    fclose(rf);

    log_line(package_dir, "SHOW_CHOICES popup opened on %s, choices=%s default=%d",
             popup_target_dir, choices_text, default_idx);

    int result_idx = -1;
    int waited_us = 0;
    while (waited_us < POLL_TIMEOUT_S * 1000000) {
        FILE *check = fopen(respath, "r");
        if (check) {
            char line[64];
            if (fgets(line, sizeof(line), check)) {
                result_idx = atoi(line);
            }
            fclose(check);
            break;
        }
        usleep(POLL_INTERVAL_US);
        waited_us += POLL_INTERVAL_US;
    }

    remove(objpath);
    remove(respath);

    int final_idx = (result_idx >= 0 && result_idx < n_choices) ? result_idx : default_idx;
    const char *timed_out = (result_idx < 0) ? " (TIMED OUT, used default)" : "";

    char result_path[PATH_BUF];
    snprintf(result_path, sizeof(result_path), "%s/choice_result.txt", package_dir);
    FILE *wf = fopen(result_path, "w");
    if (wf) {
        fprintf(wf, "choice_result=%d\n", final_idx);
        fprintf(wf, "choice_label=%s\n", choices[final_idx]);
        fclose(wf);
    }

    log_line(package_dir, "SHOW_CHOICES result=%d (%s)%s", final_idx, choices[final_idx], timed_out);
    printf("SHOW_CHOICES result=%d (%s)%s\n", final_idx, choices[final_idx], timed_out);
    return 0;
}
