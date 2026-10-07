/* game_setup_query_op - command-line front end to khtpm_game_setup.c, so a pal harness (the case files) can call the parser's functions and read the answer from stdout.
 * Usage: game_setup_query_op <game.pdl> info                      -> loaded=<0|1> title=<..> start=<..> body=<..> maps=N cells=N rows=N edits=N
 *        game_setup_query_op <game.pdl> map <mode> <desk>        -> 1 | 0        (gs_map_available)
 *        game_setup_query_op <game.pdl> cell <mode> <place> <id> -> 1 | 0        (gs_cell_visible)
 *        game_setup_query_op <game.pdl> edit <mode> <id>         -> 1 | 0        (gs_edit_allowed)
 *        game_setup_query_op <game.pdl> cellget <id> <label|modes|where|cmd|order>   -> the value (empty if none)
 *        game_setup_query_op <game.pdl> rowget <cell> <n> <label|cmd>               -> the value
 *        game_setup_query_op - mode <house>                                   -> build | play | playtest   (gs_current_mode)
 *        game_setup_query_op - switch <house> <session_dir> <target>          -> 1 | 0:<reason>            (gs_check_map_switch; appends the ledger on refusal) */
#include "../../khtpm_game_setup.c"
int main(int argc, char **argv) {
    static GameSetup g; const char *v;
    if (argc < 3) { fprintf(stderr, "usage: game_setup_query_op <game.pdl|-> <verb> ...\n"); return 2; }
    v = argv[2];
    if (!strcmp(v, "mode") && argc >= 4) { puts(gs_current_mode(argv[3])); return 0; }
    if (!strcmp(v, "switch") && argc >= 6) { char why[256]; int ok = gs_check_map_switch(argv[3], argv[4], argv[5], why, sizeof why); if (ok) puts("1"); else printf("0:%s\n", why); return 0; }
    gs_load(argv[1], &g);
    if (!strcmp(v, "info")) printf("loaded=%d title=%s start=%s body=%s maps=%d cells=%d rows=%d edits=%d\n", g.loaded, g.title, g.start, g.body, g.n_maps, g.n_cells, g.n_rows, g.n_edits);
    else if (!strcmp(v, "map") && argc >= 5) printf("%d\n", gs_map_available(&g, argv[3], argv[4]));
    else if (!strcmp(v, "cell") && argc >= 6) printf("%d\n", gs_cell_visible(&g, argv[3], argv[4], argv[5]));
    else if (!strcmp(v, "edit") && argc >= 5) printf("%d\n", gs_edit_allowed(&g, argv[3], argv[4]));
    else if (!strcmp(v, "cellget") && argc >= 5) {
        for (int i = 0; i < g.n_cells; i++) if (!strcmp(g.cells[i].id, argv[3])) {
            const GsCell *c = &g.cells[i]; const char *f = argv[4];
            if (!strcmp(f, "label")) puts(c->label); else if (!strcmp(f, "modes")) puts(c->modes); else if (!strcmp(f, "where")) puts(c->where);
            else if (!strcmp(f, "cmd")) puts(c->cmd); else if (!strcmp(f, "order")) printf("%d\n", c->order);
            return 0;
        }
        puts("");
    } else if (!strcmp(v, "rowget") && argc >= 6) {
        for (int i = 0; i < g.n_rows; i++) if (!strcmp(g.rows[i].cell, argv[3]) && g.rows[i].n == atoi(argv[4])) { puts(!strcmp(argv[5], "label") ? g.rows[i].label : g.rows[i].cmd); return 0; }
        puts("");
    } else return 2;
    return 0;
}
