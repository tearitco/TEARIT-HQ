/* tp_place_desktop - place current brush (or glyph) onto the house
 * desktop as a real, PERSISTENT entity.
 * Usage: tp_place_desktop.+x <widget_state_dir> <desktop_root> [glyph] [name]
 * If glyph omitted, reads brush.txt from widget_state_dir.
 *
 * REAL FIX 2026-08-29, direct instruction ("when the tiles are placed
 * they should be saved in user/files/desks/ so they will be reloaded
 * on reset... that is a fundamental function of this house"): used to
 * write #.desktop/tiles/<name>/ - a real, shared "exchange tray"
 * (#.desktop/README.txt's own documented "outside the live world until
 * imported" framing), with NO DESK-row registration at all, so a
 * placed tile vanished on the next session load/taskbar restart. See
 * tp_place_desktop_rmmv.c's own header for the full real investigation
 * (TILE-PLACEMENT-DESK-PERSISTENCE-GAP-2026-08-29.txt) and the same
 * fix, mirrored here: this op now makes a placed tile a REAL pal (own
 * package dir under the active session's real pals/ tree) plus a real
 * DESK row appended to the active desk's .pdl - the exact shape
 * livedesk_place_pal() (khtpm_taskbar_manager.c) produces for every
 * other entity, so the existing, unmodified livedesk_spawn_desk()
 * reload path picks it up with zero changes there.
 */
#define _GNU_SOURCE
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <libgen.h>
#include <unistd.h>
#include <limits.h>
#include "self_exe.h" /* macOS leg: portable /proc/self/exe replacement */

#define PATH_BUF 4352
#define MAX_LINE 2048

static void read_kv(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t klen = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, klen) == 0 && line[klen] == '=') {
            char *v = line + klen + 1;
            v[strcspn(v, "\n")] = '\0';
            snprintf(out, out_sz, "%s", v);
            break;
        }
    }
    fclose(f);
}

/* Generic "KEY | VALUE" or "KEY=VALUE" row reader, ported from khtpm_
 * taskbar_manager.c's own read_key_value() (separate binary, no shared
 * header - same "each tile-picker op is self-contained" convention
 * every other op here already follows). */
static void read_pdl_kv(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "META", 4) == 0) continue;
        char *p = strstr(line, key);
        if (!p) continue;
        char *eq = strchr(p, '=');
        char *bar = strrchr(p, '|');
        char *v = NULL;
        if (eq && (!bar || eq < bar)) v = eq + 1;
        else if (bar) v = bar + 1;
        if (!v) continue;
        while (*v == ' ' || *v == '\t') v++;
        v[strcspn(v, "\r\n")] = '\0';
        size_t n = strlen(v);
        while (n > 0 && (v[n - 1] == ' ' || v[n - 1] == '\t')) v[--n] = '\0';
        if (v[0]) { snprintf(out, out_sz, "%s", v); break; }
    }
    fclose(f);
}

static char *find_house_root(void) {
    char self_path[PATH_BUF];
    ssize_t len = self_exe_readlink(self_path, sizeof(self_path));
    if (len <= 0) return NULL;
    self_path[len] = '\0';
    char step[PATH_BUF];
    snprintf(step, sizeof(step), "%s", self_path);
    for (;;) {
        char *slash = strrchr(step, '/');
        if (!slash || slash == step) return NULL;
        *slash = '\0';
        char desk[PATH_BUF], widg[PATH_BUF];
        snprintf(desk, sizeof(desk), "%s/#.desktop", step);
        snprintf(widg, sizeof(widg), "%s/&.widgits", step);
        if (access(desk, F_OK) == 0 && access(widg, F_OK) == 0) return strdup(step);
    }
}

/* Real session/desk/pals resolution, ported from khtpm_taskbar_
 * manager.c's own livedesk_login_root/livedesk_user_uuid/
 * livedesk_sessions_root/livedesk_default_session/livedesk_active_desk/
 * livedesk_pals_root - real, live house state, not invented. */
static int resolve_login_root(const char *house_root, char *out, size_t sz) {
    out[0] = '\0';
    DIR *d = opendir(house_root);
    if (!d) return 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (strncmp(e->d_name, "0.user-pal", 10) == 0) {
            snprintf(out, sz, "%s/%s/00.login-signup", house_root, e->d_name);
            break;
        }
    }
    closedir(d);
    return out[0] != '\0';
}

static int resolve_user_uuid(const char *house_root, char *out, size_t sz) {
    out[0] = '\0';
    char login_root[PATH_BUF];
    if (!resolve_login_root(house_root, login_root, sizeof(login_root))) return 0;
    char p[PATH_BUF];
    snprintf(p, sizeof(p), "%s/current_login.txt", login_root);
    read_pdl_kv(p, "current_user_uuid", out, sz);
    return out[0] != '\0';
}

static int resolve_active_session_desk(const char *house_root,
                                        char *sess_dir_out, size_t sess_sz,
                                        char *desk_pdl_out, size_t desk_sz,
                                        char *pals_root_out, size_t pals_sz) {
    char uuid[128];
    if (!resolve_user_uuid(house_root, uuid, sizeof(uuid))) return 0;
    char sroot[PATH_BUF];
    snprintf(sroot, sizeof(sroot), "%s/xyzfs/users/%s/home/livedesk/sessions", house_root, uuid);
    char root_pdl[PATH_BUF];
    snprintf(root_pdl, sizeof(root_pdl), "%s/session.pdl", sroot);
    char active[64] = "";
    read_pdl_kv(root_pdl, "active_session", active, sizeof(active));
    if (!active[0]) return 0;
    snprintf(sess_dir_out, sess_sz, "%s/%s", sroot, active);
    char sess_pdl[PATH_BUF];
    snprintf(sess_pdl, sizeof(sess_pdl), "%s/session.pdl", sess_dir_out);
    char desk[64] = "";
    read_pdl_kv(sess_pdl, "active_desk", desk, sizeof(desk));
    if (!desk[0]) return 0;
    snprintf(desk_pdl_out, desk_sz, "%s/desks/%s.pdl", sess_dir_out, desk);
    snprintf(pals_root_out, pals_sz, "%s/xyzfs/users/%s/home/livedesk/pals", house_root, uuid);
    return 1;
}

static void append_desk_row(const char *desk_pdl, const char *name, const char *pals_rel,
                             int x, int y, const char *glyph) {
    int max_idx = -1;
    FILE *rf = fopen(desk_pdl, "r");
    if (rf) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), rf)) {
            if (strncmp(line, "DESK", 4) != 0) continue;
            char *p = strrchr(line, '|');
            if (p) { int idx = atoi(p + 1); if (idx > max_idx) max_idx = idx; }
        }
        fclose(rf);
    }
    FILE *wf = fopen(desk_pdl, "a");
    if (!wf) return;
    fprintf(wf, "DESK | %s | %s | %d | %d | %d | %d | %s | %d\n",
            name, pals_rel, x, y, x / 80, y / 80, glyph, max_idx + 1);
    fclose(wf);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr,
            "Usage: tp_place_desktop.+x <widget_state_dir> <desktop_root> [glyph] [name]\n");
        return 1;
    }
    const char *wdir = argv[1];
    const char *desk_root = argv[2]; /* #.desktop/ - kept for interface compat, no longer the storage location */
    /* REAL FIX 2026-08-04, direct instruction ("id like to see emojis
     * tho"): glyph widened from a single ASCII char to a real UTF-8
     * string - see tp_set_brush.c's own identical fix. */
    char glyph[64] = "";
    if (argc >= 4 && argv[3][0]) {
        snprintf(glyph, sizeof(glyph), "%s", argv[3]);
    } else {
        char brush[PATH_BUF], line[MAX_LINE];
        snprintf(brush, sizeof(brush), "%s/brush.txt", wdir);
        FILE *bf = fopen(brush, "r");
        if (bf && fgets(line, sizeof(line), bf)) {
            line[strcspn(line, "\r\n")] = '\0';
            snprintf(glyph, sizeof(glyph), "%s", line);
            fclose(bf);
        } else {
            if (bf) fclose(bf);
            fprintf(stderr, "tp_place_desktop: no brush\n");
            return 1;
        }
    }
    if (!glyph[0]) return 1;

    char name[128];
    if (argc >= 5 && argv[4][0]) {
        snprintf(name, sizeof(name), "%s", argv[4]);
    } else {
        /* Emoji glyphs can't safely go in a directory name (multi-byte,
         * possibly containing characters unfriendly to some tools) - a
         * short hash of the glyph string keeps the name filesystem-safe
         * and still distinct per-glyph. Plain ASCII glyphs still get a
         * readable name, same as before this fix. */
        int printable_ascii = glyph[1] == '\0' && glyph[0] >= 'A' && glyph[0] <= 'z';
        if (printable_ascii) {
            snprintf(name, sizeof(name), "tile_%c_%ld", glyph[0], (long)time(NULL));
        } else {
            unsigned int h = 2166136261u;
            for (const char *p = glyph; *p; p++) h = (h ^ (unsigned char)*p) * 16777619u;
            snprintf(name, sizeof(name), "tile_%08x_%ld", h, (long)time(NULL));
        }
    }

    char *house_root = find_house_root();
    if (!house_root) {
        fprintf(stderr, "tp_place_desktop: could not resolve house root\n");
        return 1;
    }

    char sess_dir[PATH_BUF], desk_pdl[PATH_BUF], pals_root[PATH_BUF];
    if (!resolve_active_session_desk(house_root, sess_dir, sizeof(sess_dir),
                                      desk_pdl, sizeof(desk_pdl), pals_root, sizeof(pals_root))) {
        fprintf(stderr, "tp_place_desktop: could not resolve active session/desk - "
                        "no login? no active session/desk set?\n");
        free(house_root);
        return 1;
    }

    char dir[PATH_BUF], cmd[PATH_BUF];
    snprintf(dir, sizeof(dir), "%s/%s", pals_root, name);
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", dir);
    if (system(cmd) != 0) { free(house_root); return 1; }

    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/glyph.txt", dir);
    FILE *f = fopen(path, "w");
    if (!f) { free(house_root); return 1; }
    fprintf(f, "%s\n", glyph);
    fclose(f);

    /* Real pal.pdl, same shape livedesk_ensure_pal() writes for every
     * other real pal - see tp_place_desktop_rmmv.c's own identical
     * block for why no hash row. */
    {
        char pal_path[PATH_BUF];
        snprintf(pal_path, sizeof(pal_path), "%s/pal.pdl", dir);
        FILE *pf = fopen(pal_path, "w");
        if (pf) {
            fprintf(pf, "PAL | name | %s\n", name);
            fprintf(pf, "PAL | glyph | %s\n", glyph);
            fclose(pf);
        }
    }

    snprintf(path, sizeof(path), "%s/meta.pdl", dir);
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "SECTION      | KEY                | VALUE\n");
        fprintf(f, "----------------------------------------\n");
        fprintf(f, "META         | piece_id           | %s\n", name);
        fprintf(f, "STATE        | kind                 | tile_stamp\n");
        fprintf(f, "STATE        | glyph                | %s\n", glyph);
        fprintf(f, "STATE        | created_at           | %ld\n", (long)time(NULL));
        /* REAL, NEW 2026-08-04, direct instruction ("add the context
         * menus that already exist from egg-pal to these by default"):
         * every desktop package gets a real, data-driven METHOD table
         * (same convention as picker_items.txt/file-menu's piece.pdl),
         * read by tp_desktop_window.c's own right-click popup - see
         * TILE_PICKER_DESIGN.md §4.5. Default is just "Close", matching
         * egg_window.c's own current default context menu exactly - not
         * inventing new default behavior, porting the existing one.
         * More methods (Open Event Editor, etc.) get appended here
         * later without touching the renderer that reads them. */
        fprintf(f, "METHOD       | Close                | CLOSE\n");
        /* REAL, NEW 2026-09-01, direct instruction ("the placed tiles
         * dont have cancel/copy/paste/delete or events... events should
         * open entity events and allow editing" - dev-only placed
         * tiles, no retrofit needed for anything placed before this):
         * Cancel matches every hand-authored entity's own default
         * (dog/asa/ava/book-stack all ship both Close AND Cancel);
         * Copy/Paste/Delete dispatch to their own small real scripts
         * (tp_copy_tile.sh/tp_paste_tile.sh/tp_delete_tile.sh, this same
         * ops/ dir) rather than new C - real house_root/package_dir are
         * both already known here at write time, so the action strings
         * below are literal, fully-resolved paths, no runtime dir-
         * climbing needed (this meta.pdl is generated fresh per real
         * house install, unlike a template shipped under pieces/).
         * Events gives THIS tile its own real, per-instance event_pkg
         * (mkdir -p'd on first use) - unlike pets/asa/ava, which share
         * one event_pkg per species under their own pieces/ dir, a
         * generic placed tile has no such shared home. */
        fprintf(f, "METHOD       | Cancel               | void\n");
        fprintf(f, "METHOD       | Copy                 | sh -c 'exec \"$1/&.widgits/tile-picker/ops/tp_copy_tile.sh\" \"$0\" \"$1\"'\n");
        fprintf(f, "METHOD       | Paste                | sh -c 'exec \"$1/&.widgits/tile-picker/ops/tp_paste_tile.sh\" \"$0\" \"$1\"'\n");
        fprintf(f, "METHOD       | Delete               | sh -c 'exec \"$1/&.widgits/tile-picker/ops/tp_delete_tile.sh\" \"$0\" \"$1\"'\n");
        fprintf(f, "METHOD       | Events               | sh -c 'mkdir -p \"$0/event_pkg/pages/page_1\" && exec env EE_PKG_NAME=\"$(basename \"$0\")\" EE_PKG_DIR=\"$0/event_pkg\" sh \"$1/&.widgits/event-editor/button.sh\" run-widget'\n");
        fclose(f);
    }

    /* Real menu.chtpm generation, same as tp_place_desktop_rmmv.c's own
     * identical block (ENTITY-MENU-LEGACY-DEPRECATION-PLAN.md Phase 1) -
     * every new entity gets one the same moment it's created. */
    {
        char conv_path[PATH_BUF], conv_cmd[PATH_BUF * 2];
        snprintf(conv_path, sizeof(conv_path), "%s/*.monads/*.livedesk-taskbar/ops/meta_to_menu_chtpm.py", house_root);
        snprintf(conv_cmd, sizeof(conv_cmd), "python3 '%s' '%s' >/dev/null 2>&1", conv_path, dir);
        int rc = system(conv_cmd);
        (void)rc; /* real, honest no-op on failure - a missing menu.chtpm
                   * just means this entity falls back to the legacy
                   * popup engine, same as it always has, not a crash */
    }

    /* remember last place in widget state */
    char last[PATH_BUF];
    snprintf(last, sizeof(last), "%s/last_desktop_place.txt", wdir);
    f = fopen(last, "w");
    if (f) {
        fprintf(f, "path=%s\n", dir);
        fprintf(f, "glyph=%s\n", glyph);
        fclose(f);
    }

    printf("DESKTOP_TILE %s glyph=%s\n", dir, glyph);

    /* REAL, NEW 2026-08-04, direct instruction ("^ mode... wherever
     * they click... the phymoji will appear"): tp_arm_placer.c resolves
     * a real click point and, when it lands on bare desktop, passes it
     * through here via env vars so the spawned window appears exactly
     * where the user clicked (snapped to the grid), instead of always
     * the fixed default spot. */
    int click_x = 0, click_y = 0;
    {
        const char *ix = getenv("TP_INITIAL_X");
        const char *iy = getenv("TP_INITIAL_Y");
        if (ix && ix[0] && iy && iy[0]) { click_x = atoi(ix); click_y = atoi(iy); }
        /* REAL, NEW 2026-08-31, direct instruction ("if in 2d u will
         * place on current z level") - a freshly-placed entity gets
         * the real, current, shared desktop_active_z.txt (same real
         * file cursword's own c/v keys write - see
         * tp_desktop_window_rgb.c's own g_active_z declaration
         * comment), not always 0. Missing file = real, honest 0
         * default, same shape every other optional state file in this
         * house already uses. */
        int active_z = 0;
        {
            char az_path[PATH_BUF];
            snprintf(az_path, sizeof(az_path), "%s/#.desktop/desktop_active_z.txt", house_root);
            FILE *azf = fopen(az_path, "r");
            if (azf) {
                char azline[16];
                if (fgets(azline, sizeof(azline), azf)) active_z = atoi(azline);
                fclose(azf);
            }
        }
        char pos_path[PATH_BUF];
        snprintf(pos_path, sizeof(pos_path), "%s/desktop_pos.txt", dir);
        FILE *pf = fopen(pos_path, "w");
        if (pf) { fprintf(pf, "x=%d\ny=%d\nz=%d\n", click_x, click_y, active_z); fclose(pf); }
    }

    /* Real DESK-row append - THE actual fix, mirrors tp_place_desktop_
     * rmmv.c's own identical block. pals_rel is relative to house_root,
     * matching every other real DESK row's own path convention
     * (khtpm_taskbar_manager.c's livedesk_pals_rel()). */
    {
        char pals_rel[PATH_BUF];
        size_t hl = strlen(house_root);
        snprintf(pals_rel, sizeof(pals_rel), "%s/%s", pals_root + hl + 1, name);
        append_desk_row(desk_pdl, name, pals_rel, click_x, click_y, glyph);
    }
    (void)desk_root;

    /* REAL FIX 2026-08-04, direct instruction ("do u see how egg-pal
     * creates the same emoji that user picked?"): reuses the EXACT same
     * two-op pipeline 01.muchi-pals-🥚️-13.01/ops/hatch_egg.c uses to turn
     * a picked emoji into a real texture - emoji_gen_atlas.+x (FreeType +
     * NotoColorEmoji -> PNG) then emoji_xtract.+x (PNG -> NxN RGBA CSV,
     * "sprite.csv") - so tp_desktop_window_rgb.+x can load and draw a
     * real textured sprite the same way egg_window.c already does,
     * instead of only a glyph-hashed color square. Best-effort: if
     * generation fails (missing emoji font glyph, etc.), sprite.csv just
     * won't exist and the entity falls back to its existing color+title
     * behavior. */
    {
        char ent_ops[PATH_BUF];
        snprintf(ent_ops, sizeof(ent_ops), "%s/*.monads/*.livedesk-taskbar/ops/+x", house_root);
        char png_path[PATH_BUF], csv_path[PATH_BUF], gen_cmd[PATH_BUF * 3];
        snprintf(png_path, sizeof(png_path), "%s/atlas.png", dir);
        snprintf(csv_path, sizeof(csv_path), "%s/sprite.csv", dir);
        snprintf(gen_cmd, sizeof(gen_cmd),
                 "'%s/emoji_gen_atlas.+x' '%s' '%s' >/dev/null 2>&1 && "
                 "'%s/emoji_xtract.+x' '%s' 0 64 '%s' >/dev/null 2>&1",
                 ent_ops, glyph, png_path, ent_ops, png_path, csv_path);
        int rc = system(gen_cmd);
        (void)rc;
    }

    /* REAL GUARD, added 2026-08-04, direct instruction ("make sure no
     * matter how many windows our cpu isn't taxed, add guards if
     * needed"): refuse to spawn a SECOND entity-renderer process for
     * the SAME package dir. Escaped `pgrep -f` regex metacharacters -
     * see tp_place_desktop_rmmv.c's own identical fix/comment (this
     * placer had the exact same unescaped-regex bug, just never caught
     * live here since this path's dir names rarely repeat).
     * REAL FIX 2026-09-01 - tp_desktop_window_rgb.+x retired as a
     * separate binary, folded into khtpm_core_render.c's own tp_main()
     * mode (see that file's own big merged-block header comment) -
     * pgrep pattern updated to match the real, current process name;
     * `dir` (this package's own unique path) keeps this specific
     * enough to not false-match a strip/HQ-app instance of the same
     * shared binary. */
    {
        char pgrep_cmd[PATH_BUF * 2];
        snprintf(pgrep_cmd, sizeof(pgrep_cmd), "pgrep -f 'khtpm_core_render\\.\\+x %s' >/dev/null 2>&1", dir);
        if (system(pgrep_cmd) == 0) {
            printf("DESKTOP_TILE %s: window already running, not spawning a duplicate\n", dir);
            free(house_root);
            return 0;
        }
    }

    /* Spawn the real, live window for this package - detached
     * background process, same "dispatch, not decision" fire-and-forget
     * shelling-out style egg_window.c uses for self_tick_pet(). */
    {
        char exe_path[PATH_BUF], spawn_cmd[PATH_BUF * 2];
        snprintf(exe_path, sizeof(exe_path), "%s/*.monads/*.livedesk-taskbar/ops/+x/khtpm_core_render.+x", house_root);
        snprintf(spawn_cmd, sizeof(spawn_cmd), "setsid '%s' '%s' >/dev/null 2>&1 < /dev/null &", exe_path, dir);
        int rc = system(spawn_cmd);
        (void)rc;
    }
    free(house_root);

    return 0;
}
