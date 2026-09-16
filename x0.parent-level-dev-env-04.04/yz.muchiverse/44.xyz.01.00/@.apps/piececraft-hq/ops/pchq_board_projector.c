/* pchq_board_projector - state publisher for pchq-board.xhtpm.
 *
 * Replaces the session-discovery / active-state reads that
 * run_pchq_board_mode() (khtpm_core_render.c) did inline. Ports:
 *   pchq_find_board_session()  - ledger_peers.+x widget lookup
 *   pchq_is_interact_on()      - active_gui_is_typing.txt
 *
 * argv: [1]=house_root  [2]=package_dir (@.apps/piececraft-hq)
 *       [3]=host_project_id (from <module args="...">, e.g. "piececraft-hq")
 * env fallbacks: KHTPM_HOUSE / KHTPM_PKG
 *
 * Writes <pkg>/state/ui.txt (atomic, only on change):
 *   bv_session=  canvas_raw=  no_session=1|""  interact_label=ON|off
 *   clock=HH:MM
 *   menu_open=file|desk|      file_menu_open=1|""  desk_menu_open=1|""
 *   n_file_opts=2  f_0_label=default-pdl  f_0_active=pchq-menu-active|""
 *                  f_1_label=default-legacy f_1_active=...
 *   n_desk_opts=1  d_0_label=<board>  d_0_active=pchq-menu-active
 *
 * The menu-open state + active_level/active_board come from
 * <pkg>/state/menu.txt (pchq_board_action.sh writes it) and
 * board-viewer's own config, same files run_pchq_board_mode read.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#define UIBUF 16384

static void sanitize(char *s) {
    for (char *p = s; *p; p++) if (*p == '\n' || *p == '\r' || *p == '\t') *p = ' ';
}

/* one OPT/KEY value out of a pipe-delimited .pdl:  "<any> | <name> | <val>" */
static int read_pdl_opt(const char *path, const char *name, int def) {
    FILE *f = fopen(path, "r");
    if (!f) return def;
    char l[256]; int v = def;
    while (fgets(l, sizeof(l), f)) {
        if (l[0] == '#') continue;
        char *p1 = strchr(l, '|'); if (!p1) continue;
        char *p2 = strchr(p1 + 1, '|'); if (!p2) continue;
        char nm[64]; int k = 0;
        for (char *q = p1 + 1; q < p2 && k < 63; q++) if (*q != ' ' && *q != '\t') nm[k++] = *q;
        nm[k] = '\0';
        if (strcmp(nm, name) != 0) continue;
        v = atoi(p2 + 1);
        break;
    }
    fclose(f);
    return v;
}

/* MILESTONE C - append entities-bar rows for the <footer> from
 * world_01/{animals,phymoji_entities}.txt + hero_01. host_app_root =
 * "<house>/@.apps/<host>". Capped at 16. */
static size_t emit_entities(char *ui, size_t off, const char *host_app_root, int on) {
    int n = 0;
    char hp[PATH_MAX];
    snprintf(hp, sizeof(hp), "%s/pieces/hero_01/state.txt", host_app_root);
    FILE *hf = fopen(hp, "r");
    if (hf) {
        char hx[16] = "", hy[16] = "", hz[16] = "", l[128];
        while (fgets(l, sizeof(l), hf)) {
            if (!strncmp(l, "pos_x=", 6)) sscanf(l + 6, "%15s", hx);
            else if (!strncmp(l, "pos_y=", 6)) sscanf(l + 6, "%15s", hy);
            else if (!strncmp(l, "pos_z=", 6)) sscanf(l + 6, "%15s", hz);
        }
        fclose(hf);
        if (hx[0] && hy[0] && hz[0]) {
            off += (size_t)snprintf(ui + off, UIBUF - off,
                "ent_%d_label=hero\nent_%d_id=hero_01\nent_%d_kind=hero\n"
                "ent_%d_x=%s\nent_%d_y=%s\nent_%d_z=%s\n",
                n, n, n, n, hx, n, hy, n, hz);
            n++;
        }
    }
    const char *files[2] = { "pieces/world_01/animals.txt",
                             "pieces/world_01/phymoji_entities.txt" };
    for (int fi = 0; fi < 2 && n < 16; fi++) {
        char p[PATH_MAX];
        snprintf(p, sizeof(p), "%s/%s", host_app_root, files[fi]);
        FILE *f = fopen(p, "r");
        if (!f) continue;
        char line[160];
        while (n < 16 && fgets(line, sizeof(line), f)) {
            char id[64]; int x, y, z;
            if (sscanf(line, "%63[^,],%d,%d,%d", id, &x, &y, &z) != 4) continue;
            const char *kind = strstr(id, "chicken") ? "chicken"
                             : strstr(id, "tree")    ? "tree" : "entity";
            off += (size_t)snprintf(ui + off, UIBUF - off,
                "ent_%d_label=%s\nent_%d_id=%s\nent_%d_kind=%s\n"
                "ent_%d_x=%d\nent_%d_y=%d\nent_%d_z=%d\n",
                n, id, n, id, n, kind, n, x, n, y, n, z);
            n++;
        }
        fclose(f);
    }
    off += (size_t)snprintf(ui + off, UIBUF - off,
        "n_ent=%d\nentities_bar_on=%s\n", n, on ? "1" : "");
    return off;
}

/* one "key=value" line out of a flat key=value file */
static void read_kv(const char *path, const char *key, char *out, size_t outsz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    size_t klen = strlen(key);
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, klen) == 0 && line[klen] == '=') {
            char *v = line + klen + 1;
            v[strcspn(v, "\r\n")] = '\0';
            snprintf(out, outsz, "%s", v);
            break;
        }
    }
    fclose(f);
}

/* game.pdl uses "SECTION | KEY | VALUE" pipe columns, not KEY=VALUE -
 * read_kv() above can't parse it (confirmed live: silently returned
 * empty for every key). Matches pc_generate_chunk.c's own
 * load_extrusion_table() column-split logic. */
static void read_pdl_kv(const char *path, const char *key, char *out, size_t outsz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *p1 = strchr(line, '|');
        if (!p1) continue;
        char *p2 = strchr(p1 + 1, '|');
        if (!p2) continue;
        char k[64]; int klen = (int)(p2 - (p1 + 1));
        while (*(p1 + 1) == ' ') p1++;
        klen = (int)(p2 - (p1 + 1));
        if (klen <= 0 || klen >= (int)sizeof(k)) continue;
        memcpy(k, p1 + 1, (size_t)klen);
        k[klen] = '\0';
        int kl = (int)strlen(k);
        while (kl > 0 && k[kl - 1] == ' ') k[--kl] = '\0';
        if (strcmp(k, key) != 0) continue;
        char *v = p2 + 1;
        while (*v == ' ') v++;
        v[strcspn(v, "\r\n")] = '\0';
        int vl = (int)strlen(v);
        while (vl > 0 && v[vl - 1] == ' ') v[--vl] = '\0';
        snprintf(out, outsz, "%s", v);
        break;
    }
    fclose(f);
}

static int file_has_nonzero(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char l[32] = "";
    int r = (fgets(l, sizeof(l), f) && atoi(l) != 0);
    fclose(f);
    return r;
}

/* scoped port of run_pchq_board_mode()'s pchq_find_board_session():
 * ledger_peers.+x needs PRISC_PROJECT_ROOT + that dir's
 * pieces/system/house_root.txt; write the latter once (idempotent). */
static int find_board_session(const char *house, const char *host_id, char *out, size_t outsz) {
    out[0] = '\0';
    char static_root[PATH_MAX], hr_path[PATH_MAX];
    snprintf(static_root, sizeof(static_root), "%s/@.apps/%s", house, host_id);
    snprintf(hr_path, sizeof(hr_path), "%s/pieces/system/house_root.txt", static_root);
    FILE *hrf = fopen(hr_path, "w");
    if (hrf) { fprintf(hrf, "%s\n", house); fclose(hrf); }

    char cmd[PATH_MAX * 2];
    snprintf(cmd, sizeof(cmd),
             "PRISC_PROJECT_ROOT='%s' '%s/&.widgits/board-viewer/ops/+x/ledger_peers.+x' widget 2>/dev/null",
             static_root, house);
    FILE *pf = popen(cmd, "r");
    if (!pf) return 0;

    char want[256];
    snprintf(want, sizeof(want), "board-viewer:%s", host_id);
    char line[1024];
    int found = 0;
    while (fgets(line, sizeof(line), pf)) {
        line[strcspn(line, "\r\n")] = '\0';
        char *save = NULL;
        char *sess_tok = strtok_r(line, "|", &save);
        strtok_r(NULL, "|", &save);
        strtok_r(NULL, "|", &save);
        char *proj_tok = strtok_r(NULL, "|", &save);
        if (proj_tok && sess_tok && strcmp(proj_tok, want) == 0) {
            /* ledger_peers keeps a row ONLINE as long as its PID lives,
             * but a hard-killed test can leave the wrapper bash alive
             * with its session dir already rm -rf'd. A session whose dir
             * is gone must NOT be reported - it makes canvas_raw point
             * at a missing .raw = blank window. Keep scanning for a
             * newer, real one. */
            struct stat sst;
            if (stat(sess_tok, &sst) == 0 && S_ISDIR(sst.st_mode)) {
                snprintf(out, outsz, "%s", sess_tok);
                found = 1;
                break;
            }
        }
    }
    pclose(pf);
    return found;
}

int main(int argc, char **argv) {
    const char *house = (argc > 1 && argv[1][0]) ? argv[1]
                      : (getenv("KHTPM_HOUSE") ? getenv("KHTPM_HOUSE") : ".");
    const char *pkg = (argc > 2 && argv[2][0]) ? argv[2]
                    : (getenv("KHTPM_PKG") ? getenv("KHTPM_PKG") : ".");
    const char *host_id = (argc > 3 && argv[3][0]) ? argv[3] : "piececraft-hq";

    char out_path[PATH_MAX], tmp_path[PATH_MAX], menu_path[PATH_MAX];
    snprintf(out_path, sizeof(out_path), "%s/state/ui.txt", pkg);
    snprintf(tmp_path, sizeof(tmp_path), "%s/state/ui.txt.tmp", pkg);
    snprintf(menu_path, sizeof(menu_path), "%s/state/menu.txt", pkg);
    { char cmd[PATH_MAX + 32]; snprintf(cmd, sizeof(cmd), "mkdir -p '%s/state'", pkg);
      int r = system(cmd); (void)r; }

    static char ui[UIBUF], last[UIBUF];
    last[0] = '\0';

    /* Session discovery is a popen() of a whole binary (ledger_peers.+x)
     * - it was run EVERY 300ms loop, forever, per board window. The
     * board-viewer session dir almost never changes: discover it once,
     * then only re-scan when we don't have one or the cached dir has
     * disappeared (session ended). */
    static char bv_cache[PATH_MAX] = "";

    for (;;) {
        char bv[PATH_MAX] = "";
        int have;
        {
            struct stat cst;
            if (bv_cache[0] && stat(bv_cache, &cst) == 0 && S_ISDIR(cst.st_mode)) {
                snprintf(bv, sizeof(bv), "%s", bv_cache);
                have = 1;
            } else {
                have = find_board_session(house, host_id, bv, sizeof(bv));
                snprintf(bv_cache, sizeof(bv_cache), "%s", have ? bv : "");
            }
        }

        char raw[PATH_MAX] = "", typing[PATH_MAX] = "", h1[PATH_MAX] = "", h2[PATH_MAX] = "";
        if (have) {
            /* Pick the canvas source by render_mode (bv_state.txt):
             *   render_mode==1 -> rgb_frame_3d_overlay.raw  (bv_render_3d
             *       raymarch, no chrome - the khtpm window draws its own
             *       toolbar)
             *   render_mode==0 -> rgb_frame_2d.raw  (bv_render_2d, the
             *       flat RPG-Maker-style tile grid - PCHQ-2D-TILE-VIEW.md.
             *       Chrome-free by construction; replaces the old
             *       rgb_frame.raw / chtpm_rgb_render text-chrome hack.)
             * Fallback to whichever exists so the canvas is never blank. */
            char st_path[PATH_MAX], overlay[PATH_MAX], flat2d[PATH_MAX];
            snprintf(st_path, sizeof(st_path), "%s/pieces/system/bv_state.txt", bv);
            snprintf(overlay, sizeof(overlay), "%s/pieces/display/rgb_frame_3d_overlay.raw", bv);
            snprintf(flat2d,  sizeof(flat2d),  "%s/pieces/display/rgb_frame_2d.raw", bv);
            char rm[8] = ""; read_kv(st_path, "render_mode", rm, sizeof(rm));
            int mode3d = (rm[0] == '\0' || atoi(rm) != 0);   /* default 3D */
            struct stat so, sc;
            int have_o = (stat(overlay, &so) == 0 && so.st_size > 0);
            int have_c = (stat(flat2d,  &sc) == 0 && sc.st_size > 0);
            if (mode3d && have_o)       snprintf(raw, sizeof(raw), "%s", overlay);
            else if (!mode3d && have_c) snprintf(raw, sizeof(raw), "%s", flat2d);
            else if (have_o)            snprintf(raw, sizeof(raw), "%s", overlay);
            else                       snprintf(raw, sizeof(raw), "%s", flat2d);
            snprintf(typing, sizeof(typing), "%s/pieces/display/active_gui_is_typing.txt", bv);
            /* REAL FIX 2026-09-04 (live debug: relayed keys landed in
             * player_app/history.txt - confirmed via direct byte-level
             * test - but the camera never moved). Root cause: THIS
             * session's actual live board-viewer engine is
             * pal/main_module.pal, interpreted by prisc+x (confirmed
             * via `ps` - a real process for every session, not the
             * older chtpm_parser.c poll loop pchq_append_key() was
             * modeled on). Its own camera loop
             * (`read_history pieces/apps/player_app/interact_relay.txt
             * x2, x1`) reads bare-decimal keys from a DIFFERENT file -
             * player_app/interact_relay.txt, not history.txt - see
             * &.widgits/board-viewer/pal/main_module.pal and
             * &.widgits/_shared-lib/system/prisc+x.c's own
             * OP_READ_HISTORY handler for the exact read format (same
             * bare-decimal-int convention khtpm already writes, just
             * the wrong filename). h2 (keyboard/history.txt) is left
             * unchanged - chtpm_parser_pal's own separate UI-menu
             * overlay process (board_viewer.chtpm's INTERACT toggle,
             * confirmed live via active_gui_is_typing.txt) still reads
             * that one; camera and UI-menu are two independent
             * consumers of two independent files. */
            snprintf(h1, sizeof(h1), "%s/pieces/apps/player_app/interact_relay.txt", bv);
            snprintf(h2, sizeof(h2), "%s/pieces/keyboard/history.txt", bv);
        }
        int interact = have && file_has_nonzero(typing);

        /* board-viewer's own config (same keys run_pchq_board_mode read
         * via pchq_read_config_kv) - live under @.apps/<host>/ */
        char cfg[PATH_MAX], active_level[64] = "", active_board[64] = "";
        snprintf(cfg, sizeof(cfg), "%s/@.apps/%s/pieces/system/board_config.txt", house, host_id);
        read_kv(cfg, "active_board", active_board, sizeof(active_board));
        /* REAL FIX 2026-09-15, direct live report ("where did the
         * default game map go... it should be a 'default' folder
         * available in File menu") - active_level used to read
         * board_config.txt's own active_level key, which this same
         * session's file-hq/load-map fix stopped writing (confirmed
         * dead - nothing in the engine ever read it either). Left
         * stale here, this row highlighting would silently drift from
         * reality. The real, live-updated source is world_01/state.txt's
         * own map_id (the same field CONFIRM_START_MAP/pc_generate_
         * chunk.c actually write/read) - empty map_id means the flat/
         * procedural "default" world, not a named map. */
        char world_state_al[PATH_MAX];
        snprintf(world_state_al, sizeof(world_state_al), "%s/@.apps/%s/pieces/world_01/state.txt", house, host_id);
        read_kv(world_state_al, "map_id", active_level, sizeof(active_level));
        /* REAL FIX 2026-09-15, direct live report ("Desk dropdown shows
         * 'mineclonia sample' regardless of what's actually loaded") -
         * active_board had the exact same staleness bug as active_level
         * above (same dead board_config.txt key, never migrated when
         * active_level was fixed). The real, live source is the
         * currently-loaded project's own game.pdl (file=dir, desk=map
         * hierarchy) - desk_1_label under pieces/system/maps/<project>/,
         * project = map_id if set, else the real "default" project
         * (never a bare literal "default" string - that project has its
         * own real game.pdl too). */
        {
            char proj_id[64];
            snprintf(proj_id, sizeof(proj_id), "%s", active_level[0] ? active_level : "default");
            char game_pdl[PATH_MAX];
            snprintf(game_pdl, sizeof(game_pdl), "%s/pieces/system/maps/%s/game.pdl", pkg, proj_id);
            char proj_label[64] = "", desk_label[64] = "";
            read_pdl_kv(game_pdl, "label", proj_label, sizeof(proj_label));
            read_pdl_kv(game_pdl, "desk_1_label", desk_label, sizeof(desk_label));
            if (proj_label[0] && desk_label[0])
                snprintf(active_board, sizeof(active_board), "%s - %s", proj_label, desk_label);
            else if (proj_label[0])
                snprintf(active_board, sizeof(active_board), "%s", proj_label);
            else
                snprintf(active_board, sizeof(active_board), "default-map-1");
        }
        int is_legacy = (strcmp(active_level, "default-legacy") == 0);
        sanitize(active_board);

        char menu_open[16] = "";
        read_kv(menu_path, "open", menu_open, sizeof(menu_open));

        /* 2026-09-09, direct instruction ("time on display should show
         * clock time, not real time"): the toolbar clock is the GAME
         * clock (world_01/state.txt game_time_epoch_sec, the same value
         * bv_render_3d.c drives the sun/sky from), not the wall clock.
         * Falls back to "--:--" if the world file has no epoch yet. */
        char world_state[PATH_MAX], epoch_s[32] = "";
        snprintf(world_state, sizeof(world_state),
                 "%s/@.apps/%s/pieces/world_01/state.txt", house, host_id);
        read_kv(world_state, "game_time_epoch_sec", epoch_s, sizeof(epoch_s));
        char clock_s[8] = "--:--";
        if (epoch_s[0]) {
            long long ep = atoll(epoch_s);
            long long tod = ep % 86400; if (tod < 0) tod += 86400;
            snprintf(clock_s, sizeof(clock_s), "%02lld:%02lld",
                     tod / 3600, (tod % 3600) / 60);
        }

        /* REAL FIX 2026-09-15, direct live correction ("i think it
         * thinks player means 'player of entity' it actually is player
         * control for game start stop... its the same yes [as PLAY-
         * MODE-ENTITY-HARNESS-DESIGN.md's own Play Mode]") - the
         * position-readback this comment used to describe was the
         * WRONG read of the original 2026-09-04 stub note; replaced
         * with the real thing: the same house-wide Play Mode flag the
         * desktop taskbar's own "8.player" menu already toggles
         * (#.desktop/khtpm_play_mode.state.txt, `mode=on|off`) -
         * that design doc's own §2 says pc-hq needs exactly this
         * button added. pchq_board_action.sh's own `player` verb
         * flips this same file; this just reads it back for display. */
        char pm_path[PATH_MAX];
        snprintf(pm_path, sizeof(pm_path), "%s/#.desktop/khtpm_play_mode.state.txt", house);
        char pm_mode[16] = "";
        read_kv(pm_path, "mode", pm_mode, sizeof(pm_mode));
        char player_label[32];
        snprintf(player_label, sizeof(player_label), "Player: %s",
                 strcmp(pm_mode, "on") == 0 ? "ON" : "OFF");

        size_t off = 0;
        off += (size_t)snprintf(ui + off, UIBUF - off,
            "bv_session=%s\ncanvas_raw=%s\nno_session=%s\n"
            "bv_h1=%s\nbv_h2=%s\ninteract_class=%s\ninteract_armed=%d\n"
            "interact_label=%s\nclock=%s\nplayer_label=%s\n"
            "menu_open=%s\nfile_menu_open=%s\ndesk_menu_open=%s\n",
            bv, raw, have ? "" : "1",
            h1, h2, interact ? "interact-active" : "", interact ? 1 : 0,
            interact ? "ON" : "off", clock_s, player_label,
            menu_open,
            strcmp(menu_open, "file") == 0 ? "1" : "",
            strcmp(menu_open, "desk") == 0 ? "1" : "");

        off += (size_t)snprintf(ui + off, UIBUF - off,
            "n_file_opts=7\n"
            /* REAL, NEW 2026-09-15, direct live report ("it should be a
             * 'default' folder availiable in File menu till user makes
             * new(palcraft) then can load palcraft from file menu") -
             * a real, always-available way back to the flat/procedural
             * starting world - now a real desk-backed project dir
             * (pieces/system/maps/default/desk1/), same load-map path
             * every other real project uses (see PALCRAFT-DESIGN.md's
             * own "file = dir, desk = map" convention, same date). */
            "f_0_label=\xF0\x9F\x95\xB9\xEF\xB8\x8F default\nf_0_verb=load-map\nf_0_arg=default\nf_0_active=%s\n"
            "f_1_label=Open File Explorer\nf_1_verb=file-hq\nf_1_arg=\nf_1_active=\n"
            "f_2_label=mineclonia_sample\nf_2_verb=load-map\nf_2_arg=mineclonia_sample\nf_2_active=%s\n"
            "f_3_label=cdda_sample\nf_3_verb=load-map\nf_3_arg=cdda_sample\nf_3_active=%s\n"
            /* REAL, NEW 2026-09-15 - two real test projects, same date,
             * proving the desk-backed load path + the generalized
             * multi-glyph extrusion table both work for genuinely new
             * projects, not just the pre-existing three. */
            "f_4_label=\xF0\x9F\x95\xB9\xEF\xB8\x8F test_walls\nf_4_verb=load-map\nf_4_arg=test_walls\nf_4_active=%s\n"
            "f_5_label=\xF0\x9F\x95\xB9\xEF\xB8\x8F test_terraces\nf_5_verb=load-map\nf_5_arg=test_terraces\nf_5_active=%s\n"
            "f_6_label=default-legacy\nf_6_verb=file\nf_6_arg=1\nf_6_active=%s\n",
            active_level[0] ? "" : "pchq-menu-active",
            strcmp(active_level, "mineclonia_sample") == 0 ? "pchq-menu-active" : "",
            strcmp(active_level, "cdda_sample") == 0 ? "pchq-menu-active" : "",
            strcmp(active_level, "test_walls") == 0 ? "pchq-menu-active" : "",
            strcmp(active_level, "test_terraces") == 0 ? "pchq-menu-active" : "",
            is_legacy ? "pchq-menu-active" : "");

        off += (size_t)snprintf(ui + off, UIBUF - off,
            "n_desk_opts=1\nd_0_label=%s\nd_0_active=pchq-menu-active\n",
            active_board);

        /* MILESTONE C - the <footer> entities bar */
        {
            char host_app[PATH_MAX], pdl[PATH_MAX];
            snprintf(host_app, sizeof(host_app), "%s/@.apps/%s", house, host_id);
            snprintf(pdl, sizeof(pdl), "%s/pieces/system/pchq.pdl", host_app);
            int ebar = read_pdl_opt(pdl, "entities_bar", 0);
            off = emit_entities(ui, off, host_app, ebar);
        }

        if (strcmp(ui, last) != 0) {
            FILE *w = fopen(tmp_path, "w");
            if (w) { fputs(ui, w); fclose(w); rename(tmp_path, out_path); }
            snprintf(last, sizeof(last), "%s", ui);
        }
        usleep(300000);
    }
    return 0;
}
