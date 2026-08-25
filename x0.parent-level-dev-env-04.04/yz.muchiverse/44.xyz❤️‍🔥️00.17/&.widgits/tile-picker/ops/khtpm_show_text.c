/* khtpm_show_text - real, generic "Show Text" event command, 2026-08-05.
 * Real RPG Maker command (confirmed, #.ref/menu/event.commands.1.txt).
 * Direct instruction: "we should use this as a chance to copy 'show
 * text' rpg maker command, and do this entirely with khtpm."
 *
 * Fire-and-forget: writes a real SHOW_TEXT_FILE relay command into the
 * entity's own already-running tp_desktop_window.c process. Real RPG
 * Maker semantics block event processing until the player dismisses the
 * text - not implemented here (this op returns immediately after
 * queuing the display) since every real caller this pass uses Show Text
 * as the LAST command in its own branch, where blocking wouldn't change
 * anything observable. A real "wait for dismiss" version would need its
 * own result-file convention, same shape as khtpm_show_choices.c's -
 * add if a future real script needs commands AFTER a Show Text.
 *
 * Usage: khtpm_show_text.+x <entity_package_dir> <text_file>
 *   text_file: a real, already-line-wrapped plain text file (the
 *   caller is responsible for real line wrapping - this op does not
 *   reflow text, it displays each real line as its own popup row).
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>

#define PATH_BUF 4352

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: khtpm_show_text.+x <entity_package_dir> <text_file>\n");
        return 1;
    }
    const char *package_dir = argv[1];
    const char *text_file = argv[2];

    char relay_path[PATH_BUF];
    snprintf(relay_path, sizeof(relay_path), "%s/interact_relay.txt", package_dir);
    FILE *rf = fopen(relay_path, "w");
    if (!rf) {
        fprintf(stderr, "khtpm_show_text: cannot open %s\n", relay_path);
        return 1;
    }
    fprintf(rf, "SHOW_TEXT_FILE:%s\n", text_file);
    fclose(rf);
    return 0;
}
