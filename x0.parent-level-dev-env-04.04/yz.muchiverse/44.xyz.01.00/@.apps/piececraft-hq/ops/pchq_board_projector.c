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
#define UIBUF 8192

static void sanitize(char *s) {
    for (char *p = s; *p; p++) if (*p == '\n' || *p == '\r' || *p == '\t') *p = ' ';
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
            snprintf(out, outsz, "%s", sess_tok);
            found = 1;
            break;
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

    for (;;) {
        char bv[PATH_MAX] = "";
        int have = find_board_session(house, host_id, bv, sizeof(bv));

        char raw[PATH_MAX] = "", typing[PATH_MAX] = "", h1[PATH_MAX] = "", h2[PATH_MAX] = "";
        if (have) {
            snprintf(raw, sizeof(raw), "%s/pieces/display/rgb_frame_3d_overlay.raw", bv);
            snprintf(typing, sizeof(typing), "%s/pieces/display/active_gui_is_typing.txt", bv);
            /* the SAME two files pchq_append_key() always dual-wrote to
             * (bv_history1/bv_history2 in the old run_pchq_board_mode) -
             * published so the template's generic relay= capability can
             * forward keys here without any renderer-side app knowledge. */
            snprintf(h1, sizeof(h1), "%s/pieces/apps/player_app/history.txt", bv);
            snprintf(h2, sizeof(h2), "%s/pieces/keyboard/history.txt", bv);
        }
        int interact = have && file_has_nonzero(typing);

        /* board-viewer's own config (same keys run_pchq_board_mode read
         * via pchq_read_config_kv) - live under @.apps/<host>/ */
        char cfg[PATH_MAX], active_level[64] = "", active_board[64] = "";
        snprintf(cfg, sizeof(cfg), "%s/@.apps/%s/pieces/system/board_config.txt", house, host_id);
        read_kv(cfg, "active_level", active_level, sizeof(active_level));
        read_kv(cfg, "active_board", active_board, sizeof(active_board));
        if (!active_board[0]) snprintf(active_board, sizeof(active_board), "default");
        int is_legacy = (strcmp(active_level, "default-legacy") == 0);
        sanitize(active_board);

        char menu_open[16] = "";
        read_kv(menu_path, "open", menu_open, sizeof(menu_open));

        time_t now = time(NULL);
        struct tm *tmv = localtime(&now);
        char clock_s[8] = "--:--";
        if (tmv) strftime(clock_s, sizeof(clock_s), "%H:%M", tmv);

        size_t off = 0;
        off += (size_t)snprintf(ui + off, UIBUF - off,
            "bv_session=%s\ncanvas_raw=%s\nno_session=%s\n"
            "bv_h1=%s\nbv_h2=%s\ninteract_class=%s\n"
            "interact_label=%s\nclock=%s\n"
            "menu_open=%s\nfile_menu_open=%s\ndesk_menu_open=%s\n",
            bv, raw, have ? "" : "1",
            h1, h2, interact ? "interact-active" : "",
            interact ? "ON" : "off", clock_s,
            menu_open,
            strcmp(menu_open, "file") == 0 ? "1" : "",
            strcmp(menu_open, "desk") == 0 ? "1" : "");

        off += (size_t)snprintf(ui + off, UIBUF - off,
            "n_file_opts=2\n"
            "f_0_label=default-pdl\nf_0_active=%s\n"
            "f_1_label=default-legacy\nf_1_active=%s\n",
            is_legacy ? "" : "pchq-menu-active",
            is_legacy ? "pchq-menu-active" : "");

        off += (size_t)snprintf(ui + off, UIBUF - off,
            "n_desk_opts=1\nd_0_label=%s\nd_0_active=pchq-menu-active\n",
            active_board);

        if (strcmp(ui, last) != 0) {
            FILE *w = fopen(tmp_path, "w");
            if (w) { fputs(ui, w); fclose(w); rename(tmp_path, out_path); }
            snprintf(last, sizeof(last), "%s", ui);
        }
        usleep(300000);
    }
    return 0;
}
