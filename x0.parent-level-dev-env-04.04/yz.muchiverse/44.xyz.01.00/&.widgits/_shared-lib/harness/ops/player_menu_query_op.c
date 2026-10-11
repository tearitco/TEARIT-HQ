/* player_menu_query_op - front end to the taskbar manager's Player menu and play-mode file helpers, for pal harness cases (it #includes khtpm_taskbar_manager.c, white-box).
 * Build (from this ops dir): gcc -std=c11 -O2 -w -I "../../../../_.monads/_.livedesk-taskbar/ops" -I .. -I ../.. -o +x/player_menu_query_op.+x player_menu_query_op.c
 *   (-I the taskbar ops dir for the manager source and header, -I the shared lib for khtpm_phone.c / khtpm_game_setup.c)
 * Usage: player_menu_query_op <house> row <n>        -> "<label>|<command>" of the Player menu's nth row (0-based), empty if none
 *        player_menu_query_op <house> count          -> number of rows
 *        player_menu_query_op <house> cmd <command>  -> the label of the row with that command (empty if none)
 *        player_menu_query_op <house> set-playtest <0|1>   -> khtpm_save_playtest   (what the play-test row does)
 *        player_menu_query_op <house> set-play <0|1>       -> khtpm_save_play_mode  (what the play row / stop do)
 *        player_menu_query_op <house> load-play | load-playtest   -> 0|1 */
#include "khtpm_taskbar_manager.c"
int main(int argc, char **argv) {
    static HQMenuItem m[64]; int n; const char *h, *v;
    if (argc < 3) return 2;
    h = argv[1]; v = argv[2];
    if (!strcmp(v, "set-playtest") && argc >= 4) { khtpm_save_playtest(h, atoi(argv[3])); return 0; }
    if (!strcmp(v, "set-play") && argc >= 4) { khtpm_save_play_mode(h, atoi(argv[3])); return 0; }
    if (!strcmp(v, "load-play")) { printf("%d\n", khtpm_load_play_mode(h)); return 0; }
    if (!strcmp(v, "load-playtest")) { printf("%d\n", khtpm_load_playtest(h)); return 0; }
    n = livedesk_build_player_menu(h, m, 64);
    if (!strcmp(v, "count")) printf("%d\n", n);
    else if (!strcmp(v, "row") && argc >= 4) { int i = atoi(argv[3]); if (i >= 0 && i < n) printf("%s|%s\n", m[i].label, m[i].command); else puts(""); }
    else if (!strcmp(v, "cmd") && argc >= 4) { for (int i = 0; i < n; i++) if (!strcmp(m[i].command, argv[3])) { puts(m[i].label); return 0; } puts(""); }
    else return 2;
    return 0;
}
