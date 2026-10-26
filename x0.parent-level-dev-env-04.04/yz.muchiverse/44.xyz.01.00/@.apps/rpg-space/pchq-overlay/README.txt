pchq-overlay: this game's own versions of pc-hq instance config files. board/open_board.sh copies every file here over <app>/rpg-space-pchq/pieces/system/ at each launch
(the instance folder is gitignored and pchq_instance_init.sh never overwrites it, so a tuned file there would be lost).
keybinds.pdl  = the shared pc-hq keys, ctx_menu moved to o (111) and the HOSTKEY row that forwards m (109) to this skin's state/interact_relay.txt (engine bv_menu_input.c pdl_hostkey).
view.pdl      = ent_bar_hero=0 (the hero is not in the bottom bar).
