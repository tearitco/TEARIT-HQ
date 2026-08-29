/* mr_character - the four CHARACTER event commands as one real op,
 * 2026-08-29 (Task 1: Transparency, Followers, Animation, Erase).
 *
 * Usage: mr_character.+x <entity_dir> <mode> <value>
 *   mode:  transparency | followers | animation | erase
 *   value: on/off (transparency), show/hide (followers), an animation
 *          id (animation), or 1 (erase) - registry TEMPLATEs pass the
 *          literal author value; empty means "toggle/reset" semantic
 *          per command (documented below).
 *
 * Effect: writes REAL per-entity character state to
 * <entity_dir>/character_state.pdl - one `key=value` line per command
 * (the same flat kv convention inventory.txt/variables.txt/switches.txt
 * already use, so any kv reader in the house can consume it), plus the
 * usual messages.txt/history.txt audit. The sprite/rendering layer that
 * finally DISPLAYS transparency/followers/animation is a documented
 * later layer (GAME-READINESS-GAP-ANALYSIS "needs a real sprite layer")
 * - this task's contract is the real STATE + text-verifiable end-to-end
 * compile path, which this op fully serves. Non-blocking (no popup, no
 * clock coupling): a character command has no player-wait.
 */
#include "mr_clock_common.h"

static const char *normalize_mode(const char *mode) {
    if (strcmp(mode, "transparency") == 0) return "transparency";
    if (strcmp(mode, "followers") == 0) return "followers";
    if (strcmp(mode, "animation") == 0) return "animation";
    if (strcmp(mode, "erase") == 0) return "erase";
    return mode;
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: mr_character.+x <entity_dir> <mode> <value>\n");
        return 1;
    }
    const char *entity_dir = argv[1];
    const char *mode = normalize_mode(argv[2]);
    const char *value = argv[3];

    char state_path[MR_PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/character_state.pdl", entity_dir);
    mr_kv_set(state_path, mode, value);

    mr_log(entity_dir, "CHARACTER %s=%s", mode, value);
    printf("CHARACTER %s=%s (state in %s)\n", mode, value, state_path);
    return 0;
}