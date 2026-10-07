/* test_game_setup.c - build+run: gcc -Wall -o /tmp/tgs tests/test_game_setup.c && /tmp/tgs   (scratch files only) */
#include "../khtpm_game_setup.c"
#include <unistd.h>
#include <sys/stat.h>
static int fails;
#define CK(name, cond) do { if (cond) printf("PASS|%s\n", name); else { printf("FAIL|%s\n", name); fails = 1; } } while (0)
int main(void) {
    char path[] = "/tmp/gs_test_XXXXXX"; int fd = mkstemp(path); FILE *f; GameSetup g;
    if (fd < 0) return 2;
    f = fdopen(fd, "w");
    fputs("# a game\n"
          "GAME | title | My Game\nGAME | start | start_01\nGAME | body | hero_01\n"
          "MAP | town | available | 1\nMAP | cave | available | 1\nMAP | secret | available | 0\n"
          "CELL | db | db | modes=build | where=livedesk,pchq | order=3 | cmd=livedesk:open-db\n"
          "CELL | plugins | plugins | modes=build,playtest | cmd=sh a/b.sh\n"
          "CELL | gametitle | My Game | modes=play,playtest | where=pchq | order=1 | cmd=event:title_screen\n"
          "ROW | gametitle | 1 | Continue | cmd=event:continue\nROW | gametitle | 2 | Quit | cmd=quit\n"
          "EDIT | door_civ | playtest | 1\nEDIT | locked | playtest | 0\n"
          "WHATEVER | unknown | row ignored\n\n   # indented comment\n", f);
    fclose(f);
    CK("missing file reports 1", gs_load("/tmp/gs_does_not_exist_zz", &g) == 1 && !g.loaded);
    CK("missing file = build defaults everywhere", gs_map_available(&g, "play", "x") && gs_cell_visible(&g, "play", "pchq", "db") && gs_edit_allowed(&g, "build", "x"));
    CK("load ok", gs_load(path, &g) == 0 && g.loaded);
    CK("GAME rows", !strcmp(g.title, "My Game") && !strcmp(g.start, "start_01") && !strcmp(g.body, "hero_01"));
    CK("MAP: only available=1 rows kept (2)", g.n_maps == 2);
    CK("CELL count 3, ROW count 2, EDIT count 1 (playtest|0 skipped)", g.n_cells == 3 && g.n_rows == 2 && g.n_edits == 1);
    CK("cell options parsed", !strcmp(g.cells[0].cmd, "livedesk:open-db") && g.cells[0].order == 3 && !strcmp(g.cells[2].cmd, "event:title_screen"));
    CK("cell defaults (no where= -> both places, modes default)", gs_list_has(g.cells[1].where, "pchq") && gs_list_has(g.cells[1].where, "livedesk"));
    CK("row parsed", g.rows[0].n == 1 && !strcmp(g.rows[0].label, "Continue") && !strcmp(g.rows[1].cmd, "quit"));
    CK("build: every map allowed", gs_map_available(&g, "build", "secret") && gs_map_available(&g, "build", "anything"));
    CK("play: listed map allowed, unlisted refused", gs_map_available(&g, "play", "town") && !gs_map_available(&g, "play", "secret") && !gs_map_available(&g, "playtest", "hall"));
    CK("build: every cell visible", gs_cell_visible(&g, "build", "pchq", "db") && gs_cell_visible(&g, "build", "pchq", "gametitle"));
    CK("play: db hidden (modes=build)", !gs_cell_visible(&g, "play", "livedesk", "db"));
    CK("playtest: plugins visible, play: hidden", gs_cell_visible(&g, "playtest", "pchq", "plugins") && !gs_cell_visible(&g, "play", "pchq", "plugins"));
    CK("where= combines with modes=: gametitle only in pchq", gs_cell_visible(&g, "play", "pchq", "gametitle") && !gs_cell_visible(&g, "play", "livedesk", "gametitle"));
    CK("unlisted built-in cell stays visible in play", gs_cell_visible(&g, "play", "pchq", "book"));
    CK("edit: build yes, play no, playtest only marked", gs_edit_allowed(&g, "build", "zzz") && !gs_edit_allowed(&g, "play", "door_civ") && gs_edit_allowed(&g, "playtest", "door_civ") && !gs_edit_allowed(&g, "playtest", "locked"));
    /* file present but no MAP rows = unrestricted (legacy-safe) */
    f = fopen(path, "w"); fputs("GAME | title | t\n", f); fclose(f);
    gs_load(path, &g);
    CK("no MAP rows -> maps unrestricted in play", gs_map_available(&g, "play", "anything"));
    /* limits: more rows than slots must not overflow */
    f = fopen(path, "w"); for (int i = 0; i < 100; i++) fprintf(f, "MAP | m%d | available | 1\n", i); for (int i = 0; i < 100; i++) fprintf(f, "CELL | c%d | c | modes=play\n", i); fclose(f);
    gs_load(path, &g);
    CK("capacity clamp (64 maps, 64 cells), no crash", g.n_maps == 64 && g.n_cells == 64);
    /* one-call helpers on a scratch house */
    {
        char house[] = "/tmp/gs_house_XXXXXX"; char buf[1200], reason[200]; int ok;
        if (!mkdtemp(house)) return 2;
        snprintf(buf, sizeof buf, "%s/#.desktop", house); mkdir(buf, 0755);
        snprintf(buf, sizeof buf, "%s/sess", house); mkdir(buf, 0755);
        snprintf(buf, sizeof buf, "%s/sess/game.pdl", house); f = fopen(buf, "w"); fputs("MAP | town | available | 1\n", f); fclose(f);
        snprintf(buf, sizeof buf, "%s/sess", house);
        CK("no mode file = build: any map allowed", gs_check_map_switch(house, buf, "cave", reason, sizeof reason) == 1);
        { char mp[1200]; snprintf(mp, sizeof mp, "%s/#.desktop/khtpm_play_mode.state.txt", house); f = fopen(mp, "w"); fputs("mode=off\n", f); fclose(f);
          CK("mode=off = build: allowed", gs_check_map_switch(house, buf, "cave", reason, sizeof reason) == 1 && !strcmp(gs_current_mode(house), "build"));
          f = fopen(mp, "w"); fputs("mode=on\n", f); fclose(f); }
        CK("mode=on reads as play", !strcmp(gs_current_mode(house), "play"));
        CK("play: listed map allowed, no ledger line", gs_check_map_switch(house, buf, "town", reason, sizeof reason) == 1);
        { char lp[1200]; snprintf(lp, sizeof lp, "%s/#.desktop/game_access_ledger.txt", house); CK("no ledger yet", access(lp, F_OK) != 0);
          ok = gs_check_map_switch(house, buf, "cave", reason, sizeof reason);
          CK("play: unlisted map refused with a reason", ok == 0 && strstr(reason, "cave") && strstr(reason, "play"));
          { char l[400] = ""; FILE *lf = fopen(lp, "r"); if (lf) { if (!fgets(l, sizeof l, lf)) l[0] = 0; fclose(lf); }
            CK("refusal appended one ledger line", strstr(l, "|refused-map|play|cave|") != NULL); }
          gs_check_map_switch(house, buf, "hall", reason, sizeof reason);
          { int n = 0; char l[400]; FILE *lf = fopen(lp, "r"); while (lf && fgets(l, sizeof l, lf)) n++; if (lf) fclose(lf); CK("ledger is append-only (2 lines)", n == 2); } }
        snprintf(buf, sizeof buf, "%s/nogame", house); mkdir(buf, 0755);
        CK("play but no game.pdl: allowed (existing houses unchanged)", gs_check_map_switch(house, buf, "cave", reason, sizeof reason) == 1);
        snprintf(buf, sizeof buf, "rm -rf %s", house); if (system(buf)) {}
    }
    unlink(path);
    printf("VERDICT|%s\n", fails ? "FAIL" : "PASS");
    return fails;
}
