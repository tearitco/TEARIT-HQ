/* tsc_input - host keyboard dispatch for True Swords Clash.
 * Role split mirrors mychara_menu_input.c: the PAL loop calls this with
 * a register arg - 0 = idle sync (Pitfall 48 pre-sync no-op here, the
 * host has no screen-switch state in P1), or a real keycode from
 * read_history on pieces/apps/player_app/interact_relay.txt (fed by the
 * keyboard -> keyboard/history.txt -> chtpm_parser_pal -> relay chain).
 *
 * The relayed key maps to a Mana-Challenge action and is handed to
 * tsc_deal via pieces/system/player_action.txt (single-line handoff
 * file; only this op writes it, only tsc_deal consumes it - ONE WRITER
 * RULE). Actual validation (is it the human's turn? is the match
 * playing?) happens in tsc_deal, so a stray key during a computer's
 * turn is harmlessly discarded.
 *
 *   '1' -> strike   (3 dmg, 0 mana)
 *   '2' -> heavy    (6 dmg, 2 mana)
 *   '3' -> heal     (+5 HP, 2 mana)
 *   '4' -> block    (self status=block, absorbs 3, 1 mana)
 *
 * Self-contained, no shared headers.
 * Usage: tsc_input.+x <key> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

int main(int argc, char **argv) {
    if (argc < 2) return 0;
    resolve_root();

    int key = atoi(argv[1]);
    if (key == 0) return 0;

    const char *action = NULL;
    if (key == '1') action = "strike";
    else if (key == '2') action = "heavy";
    else if (key == '3') action = "heal";
    else if (key == '4') action = "block";

    if (!action) return 0;

    char handoff_path[PATH_BUF];
    snprintf(handoff_path, sizeof(handoff_path), "%s/pieces/system/player_action.txt", project_root);
    FILE *f = fopen(handoff_path, "w");
    if (!f) return 0;
    fputs(action, f);
    fclose(f);
    return 0;
}
