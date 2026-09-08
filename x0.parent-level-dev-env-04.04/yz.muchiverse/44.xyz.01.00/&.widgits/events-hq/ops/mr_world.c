/* mr_world - remaining RMMV-shaped event commands as real STATE writes.
 * 2026-09-08. Same contract as mr_character.c: verifiable kv files,
 * no fake renderer. Map/screen/shop/battle consumers can read later.
 *
 * Usage: mr_world.+x <entity_dir> <mode> <k1> [k2] [k3]
 */
#include "mr_clock_common.h"

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: mr_world.+x <entity_dir> <mode> <arg> [arg2] [arg3]\n");
        return 1;
    }
    const char *ent = argv[1];
    const char *mode = argv[2];
    const char *a1 = argv[3];
    const char *a2 = (argc > 4) ? argv[4] : "";
    const char *a3 = (argc > 5) ? argv[5] : "";

    char root[MR_PATH_BUF];
    mr_state_root(ent, root, sizeof(root));

    if (!strcmp(mode, "self_switch")) {
        char p[MR_PATH_BUF];
        snprintf(p, sizeof(p), "%s/self_switches.txt", ent);
        mr_kv_set(p, a1[0] ? a1 : "A", a2[0] ? a2 : "1");
        mr_log(ent, "SELF_SWITCH %s=%s", a1, a2);
    } else if (!strcmp(mode, "timer")) {
        char p[MR_PATH_BUF];
        snprintf(p, sizeof(p), "%s/timer.txt", root);
        mr_kv_set(p, "running", a1);
        if (a2[0]) mr_kv_set(p, "seconds", a2);
        mr_log(ent, "TIMER running=%s seconds=%s", a1, a2);
    } else if (!strcmp(mode, "fadeout") || !strcmp(mode, "fadein")
            || !strcmp(mode, "tint") || !strcmp(mode, "flash") || !strcmp(mode, "shake")) {
        char p[MR_PATH_BUF];
        snprintf(p, sizeof(p), "%s/screen_state.pdl", root);
        mr_kv_set(p, "mode", mode);
        mr_kv_set(p, "value", a1);
        if (a2[0]) mr_kv_set(p, "frames", a2);
        mr_log(ent, "SCREEN %s=%s frames=%s", mode, a1, a2);
    } else if (!strcmp(mode, "transfer")) {
        char p[MR_PATH_BUF];
        snprintf(p, sizeof(p), "%s/map_state.pdl", root);
        char xs[32] = "0", ys[32] = "0";
        snprintf(xs, sizeof(xs), "%s", a2);
        snprintf(ys, sizeof(ys), "%s", a3);
        char *comma = strchr(xs, ',');
        if (comma) { *comma = 0; snprintf(ys, sizeof(ys), "%s", comma + 1); }
        mr_kv_set(p, "map", a1);
        mr_kv_set(p, "x", xs);
        mr_kv_set(p, "y", ys);
        mr_log(ent, "TRANSFER map=%s x=%s y=%s", a1, xs, ys);
    } else if (!strcmp(mode, "scroll")) {
        char p[MR_PATH_BUF];
        snprintf(p, sizeof(p), "%s/map_state.pdl", root);
        mr_kv_set(p, "scroll_dx", a1);
        mr_kv_set(p, "scroll_dy", a2);
        mr_log(ent, "SCROLL dx=%s dy=%s", a1, a2);
    } else if (!strcmp(mode, "move_route")) {
        char p[MR_PATH_BUF];
        snprintf(p, sizeof(p), "%s/map_state.pdl", root);
        mr_kv_set(p, "move_target", a1);
        mr_kv_set(p, "move_route", a2);
        mr_log(ent, "MOVE_ROUTE target=%s route=%s", a1, a2);
    } else if (!strcmp(mode, "shop")) {
        char p[MR_PATH_BUF];
        snprintf(p, sizeof(p), "%s/shop_state.pdl", root);
        mr_kv_set(p, "goods", a1);
        mr_kv_set(p, "open", "1");
        mr_log(ent, "SHOP goods=%s", a1);
    } else if (!strcmp(mode, "battle")) {
        char p[MR_PATH_BUF];
        snprintf(p, sizeof(p), "%s/battle_state.pdl", root);
        mr_kv_set(p, "troop", a1);
        mr_kv_set(p, "can_escape", a2[0] ? a2 : "1");
        mr_log(ent, "BATTLE troop=%s can_escape=%s", a1, a2);
    } else if (!strcmp(mode, "play_se")) {
        char p[MR_PATH_BUF];
        snprintf(p, sizeof(p), "%s/audio_state.pdl", root);
        mr_kv_set(p, "last_se", a1);
        mr_log(ent, "PLAY_SE %s", a1);
        if (a1[0] && access(a1, R_OK) == 0) {
            char cmd[MR_PATH_BUF + 64];
            snprintf(cmd, sizeof(cmd), "ffplay -nodisp -autoexit -loglevel quiet '%s' >/dev/null 2>&1 &", a1);
            int r = system(cmd); (void)r;
        }
    } else {
        fprintf(stderr, "mr_world: unknown mode %s\n", mode);
        return 1;
    }
    printf("WORLD %s ok\n", mode);
    return 0;
}
