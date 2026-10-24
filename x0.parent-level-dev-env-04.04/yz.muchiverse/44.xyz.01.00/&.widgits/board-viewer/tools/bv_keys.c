/* bv_keys - press keys in a board-viewer engine session, like a person at the board (owner 2026-10-23: "move camera etc, get comfortable and make skills inside these worlds").
 *   bv_keys <session_dir> <text>        every character of <text> is one key press (its ASCII code appended to <session>/pieces/apps/player_app/interact_relay.txt, the relay the engine drains),
 *                                       then one redraw nudge (126, bound to nothing). Escapes in <text>: \n = Enter (13), \e = Escape (27), \t = Tab (9), \\ = backslash, ~ is the nudge itself (ignored).
 * Camera keys of the default config (pieces/system/arrow_config.txt): q e yaw, r t pitch down / up, c v free camera height down / up, w a s d pan, f reset, 1-4 camera mode (the 3D modes), 0 2D / 3D, z x xelector level down / up.
 * The session dir is bv_session= of the window's board/state/ui.txt. A key is processed when the engine next wakes (the nudge wakes it); a camera still moving renders at low detail, the settled frame is crisp.
 * Exit 0 ok, 1 the relay is not writable, 2 usage. Build: sh tools/build_bv_keys.sh */
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
int main(int argc, char **argv) {
    if (argc != 3) { fprintf(stderr, "usage: bv_keys <session_dir> <text>\n"); return 2; }
    char p[PATH_MAX + 64]; snprintf(p, sizeof p, "%s/pieces/apps/player_app/interact_relay.txt", argv[1]);
    FILE *f = fopen(p, "a"); if (!f) { fprintf(stderr, "bv_keys: cannot append to %s\n", p); return 1; }
    int n = 0;
    for (const char *s = argv[2]; *s; s++) {
        int c = (unsigned char)*s;
        if (c == '\\' && s[1]) { s++; c = *s == 'n' ? 13 : *s == 'e' ? 27 : *s == 't' ? 9 : (unsigned char)*s; }
        if (c == '~') continue;
        fprintf(f, "%d\n", c); fflush(f); n++; usleep(120000);
    }
    fprintf(f, "126\n"); fclose(f); printf("sent %d key(s)\n", n); return 0;
}
