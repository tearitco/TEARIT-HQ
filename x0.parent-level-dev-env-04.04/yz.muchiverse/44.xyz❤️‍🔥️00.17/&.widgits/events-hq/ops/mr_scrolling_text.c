/* mr_scrolling_text - real "Scrolling Text" event command op, 2026-08-29
 * (Task 1). Text-family message command → NON-BLOCKING POPUP by design:
 * gap #0 (CURSWORD-SOUL-VISION.md §4) explicitly carves out the
 * non-blocking popup exception - fire the SHOW_TEXT_FILE relay and
 * return; the op cannot observe the popup's dismissal (no renderer
 * signal exists yet - a blocking variant would need exactly the
 * deferred-edit escape hatch), so blocking suspension is handled by the
 * other blocking message ops + common_events_manager's clock-pause gate.
 *
 * Usage: mr_scrolling_text.+x <entity_dir> <house_root> <text>
 *   entity_dir:  the real playing entity's dir ($ENT from cmd_N.sh)
 *   house_root:  house root ($D; accepted for signature symmetry, not
 *                used - non-blocking popup never touches the clock)
 *   text:        the text to show (a "|" is disallowed inside values
 *                by the IR param format; one popup row per \n would
 *                need a renderer multiline surface - v1 shows the line
 *                as-is, matching mr_show_text's own single-file rule)
 *
 * Effect: real SHOW_TEXT_FILE:<tmp> relay write to the player-visible
 * popup target (MUCHI_CALLER_PKG-aware like mr_show_choices.c), audit
 * to messages.txt/history.txt. Text file is written under entity_dir
 * so the common event (which owns its own pkg dir) can still share one.
 */
#include "mr_clock_common.h"

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: mr_scrolling_text.+x <entity_dir> <house_root> <text>\n");
        return 1;
    }
    const char *entity_dir = argv[1];
    const char *house_root = argv[2];
    (void)house_root;
    const char *text = argv[3];

    char txt_path[MR_PATH_BUF];
    snprintf(txt_path, sizeof(txt_path), "%s/.scrolling_text.tmp.txt", entity_dir);
    FILE *tf = fopen(txt_path, "w");
    if (!tf) {
        fprintf(stderr, "mr_scrolling_text: cannot write %s\n", txt_path);
        return 1;
    }
    fprintf(tf, "%s\n", text);
    fclose(tf);

    char relay_path[MR_PATH_BUF];
    snprintf(relay_path, sizeof(relay_path), "%s/interact_relay.txt", mr_popup_target(entity_dir));
    FILE *rf = fopen(relay_path, "w");
    if (!rf) {
        fprintf(stderr, "mr_scrolling_text: cannot write %s\n", relay_path);
        return 1;
    }
    fprintf(rf, "SHOW_TEXT_FILE:%s\n", txt_path);
    fclose(rf);

    mr_log(entity_dir, "SCROLLING_TEXT queued file=%s text=%s", txt_path, text);
    printf("SCROLLING_TEXT queued file=%s\n", txt_path);
    return 0;
}