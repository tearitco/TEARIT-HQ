/* pc_touch_drv.c - test driver for the doom touch handler: includes pc_menu_input.c (main renamed) and calls check_player_touch_trigger(app_root, turn, x, y). Build: copy next to pc_menu_input.c, gcc -O2 -w -I . -o drv pc_touch_drv.c (see doom_touch_cases.pdl). Never run against the real app: it fires events. */
#define main pc_real_main
#include "pc_menu_input.c"
#undef main
int main(int argc, char **argv) { if (argc < 4) return 2; check_player_touch_trigger(argv[1], 1, atoi(argv[2]), atoi(argv[3])); return 0; }
