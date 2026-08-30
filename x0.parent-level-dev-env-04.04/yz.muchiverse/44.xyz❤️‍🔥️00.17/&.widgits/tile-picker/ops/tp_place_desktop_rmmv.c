/* tp_place_desktop_rmmv - place the currently armed RMMV-tile brush
 * onto the house desktop as a real, PERSISTENT tile-entity
 * (TILE-SYSTEM-DESIGN.md §4b.3/§6 item 6). Real, deliberate parallel of
 * tp_place_desktop.c rather than a shared-code generalization of it:
 * the RMMV tile pipeline copies an already-rendered sprite.csv (the
 * palette manager's own per-kind thumbnail cache, see
 * palettes_manager.c's publish_rmmv()) instead of running the emoji
 * FreeType pipeline, and the meta.pdl fields it writes describe a
 * tileset/category/kind identity instead of a glyph. tp_desktop_
 * window_rgb.c needs ZERO changes for this - it already draws any
 * entity's sprite.csv generically (load_sprite_csv()), regardless of
 * what produced it.
 *
 * REAL FIX 2026-08-29, direct instruction ("when the tiles are placed
 * they should be saved in user/files/desks/ so they will be reloaded
 * on reset... that is a fundamental function of this house"): a placed
 * tile used to be written to #.desktop/tiles/<name>/ - a real, shared
 * "exchange tray" (#.desktop/README.txt's own documented "outside the
 * live world until imported" framing), with NO DESK-row registration
 * at all. Confirmed live: every tile placed this way vanished on the
 * next session load/taskbar restart - livedesk_spawn_desk() (khtpm_
 * taskbar_manager.c) only ever re-spawns entities it finds as DESK rows
 * in the active session's active desk .pdl. This op now makes a placed
 * tile a REAL pal: its own package dir under the active session's real
 * pals/ tree, plus a real `DESK | ...` row appended to the active
 * desk's .pdl - the exact same on-disk shape livedesk_place_pal()
 * produces for every other entity, so the existing, unmodified
 * livedesk_spawn_desk() reload path picks it up on the next session
 * load/reset with zero changes there. See TILE-PLACEMENT-DESK-
 * PERSISTENCE-GAP-2026-08-29.txt for the full investigation + the
 * user's own answers on the design fork (real DESK row / tiles-are-
 * pals / active session+desk / move storage into the session tree).
 *
 * Usage: tp_place_desktop_rmmv.+x <widget_state_dir> <desktop_root>
 * Reads <widget_state_dir>/brush_rmmv.txt (written by
 * tp_set_brush_rmmv.+x). Click position comes from the real, permanent
 * nav_master_ledger.txt (RMMV_CLICK rows) - <desktop_root> is still
 * needed for that, house_root (independently resolved) is needed for
 * the session/pals/desk tree.
 */
#define _GNU_SOURCE
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
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
 * taskbar_manager.c's own read_key_value() (this is a separate binary,
 * no shared header for it - same "each tile-picker op is self-
 * contained" convention every other op here already follows). */
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

/* Same house-root marker-walk every other tile-picker op already uses
 * (#.desktop/ + &.widgits/ both present). Returns a heap string via
 * strdup, or NULL. */
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

/* Resolves the active session's dir, its active desk's .pdl path, and
 * the user's real pals/ root. Returns 0 (does nothing else) if any real
 * piece is missing - callers must treat that as a hard failure, not a
 * silent fallback to the old, non-persistent #.desktop/tiles/ path
 * (that path is real, intentional scope now, not an accident to keep
 * papering over). */
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

/* Appends a real DESK row for this entity - same row shape/columns
 * khtpm_taskbar_manager.c's livedesk_place_pal() writes (DESK | name |
 * rel_pal_path | x | y | gx | gy | glyph | index), scanning the desk
 * .pdl once first for the current max index (own real DESK rows are
 * permanent history, never truncated - same convention every desk .pdl
 * already follows). No dedup-by-name check: unlike a pal (one fixed
 * identity, re-placed many times), each tile placement is a brand new,
 * uniquely-named entity (name includes a timestamp), so there is never
 * a real pre-existing row to collide with. */
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
        fprintf(stderr, "Usage: tp_place_desktop_rmmv.+x <widget_state_dir> <desktop_root>\n");
        return 1;
    }
    const char *wdir = argv[1];
    const char *desk_root = argv[2]; /* #.desktop/ - only used to read nav_master_ledger.txt */

    char brush_path[PATH_BUF];
    snprintf(brush_path, sizeof(brush_path), "%s/brush_rmmv.txt", wdir);
    char sprite_dir[PATH_BUF] = "", tileset[64] = "", category[16] = "", kind_label[128] = "";
    read_kv(brush_path, "sprite_dir", sprite_dir, sizeof(sprite_dir));
    read_kv(brush_path, "tileset", tileset, sizeof(tileset));
    read_kv(brush_path, "category", category, sizeof(category));
    read_kv(brush_path, "kind_label", kind_label, sizeof(kind_label));
    if (!sprite_dir[0] || !tileset[0]) {
        fprintf(stderr, "tp_place_desktop_rmmv: no armed rmmv brush\n");
        return 1;
    }

    char src_sprite[PATH_BUF];
    snprintf(src_sprite, sizeof(src_sprite), "%s/sprite.csv", sprite_dir);
    if (access(src_sprite, F_OK) != 0) {
        fprintf(stderr, "tp_place_desktop_rmmv: armed brush's sprite.csv missing at %s\n", src_sprite);
        return 1;
    }

    char *house_root = find_house_root();
    if (!house_root) {
        fprintf(stderr, "tp_place_desktop_rmmv: could not resolve house root\n");
        return 1;
    }

    char sess_dir[PATH_BUF], desk_pdl[PATH_BUF], pals_root[PATH_BUF];
    if (!resolve_active_session_desk(house_root, sess_dir, sizeof(sess_dir),
                                      desk_pdl, sizeof(desk_pdl), pals_root, sizeof(pals_root))) {
        fprintf(stderr, "tp_place_desktop_rmmv: could not resolve active session/desk - "
                        "no login? no active session/desk set?\n");
        free(house_root);
        return 1;
    }

    char name[128];
    snprintf(name, sizeof(name), "tile_rmmv_%s_%s_%ld", tileset, category, (long)time(NULL));

    char dir[PATH_BUF], cmd[PATH_BUF];
    snprintf(dir, sizeof(dir), "%s/%s", pals_root, name);
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", dir);
    if (system(cmd) != 0) { free(house_root); return 1; }

    /* Copy the already-rendered representative thumbnail (the palette
     * manager's own real per-kind cache) - not regenerated here, same
     * "reuse, don't reinvent" convention as everything else this pass
     * traced through. */
    char cp_cmd[PATH_BUF * 2];
    snprintf(cp_cmd, sizeof(cp_cmd), "cp '%s' '%s/sprite.csv'", src_sprite, dir);
    if (system(cp_cmd) != 0) {
        fprintf(stderr, "tp_place_desktop_rmmv: failed to copy sprite.csv\n");
        free(house_root);
        return 1;
    }

    const char *glyph = "\xF0\x9F\xA7\xB1"; /* 🧱 - cosmetic placeholder, an rmmv tile has no real single-glyph identity */
    {
        char glyph_path[PATH_BUF];
        snprintf(glyph_path, sizeof(glyph_path), "%s/glyph.txt", dir);
        FILE *gf = fopen(glyph_path, "w");
        if (gf) { fprintf(gf, "%s\n", glyph); fclose(gf); }
    }

    /* Real pal.pdl - same shape livedesk_ensure_pal() writes for every
     * other real pal (PAL | name | ... / PAL | glyph | ...), so this
     * entity shows up correctly anywhere the house lists real pals, not
     * just in the desk .pdl. No hash row - that's a content-integrity
     * check for user-authored pals; an auto-generated tile has nothing
     * to verify against. */
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

    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/meta.pdl", dir);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "SECTION      | KEY                | VALUE\n");
        fprintf(f, "----------------------------------------\n");
        fprintf(f, "META         | piece_id           | %s\n", name);
        fprintf(f, "STATE        | kind                 | tile_rmmv\n");
        fprintf(f, "STATE        | tileset              | %s\n", tileset);
        fprintf(f, "STATE        | category             | %s\n", category);
        fprintf(f, "STATE        | kind_label           | %s\n", kind_label);
        fprintf(f, "STATE        | created_at           | %ld\n", (long)time(NULL));
        /* Real, honest scope note (2026-08-29): no autotile_shape/
         * neighbor-recompute field yet - TILE-SYSTEM-DESIGN.md §6 item 8
         * (autotile recompute-on-neighbor-change) is separate, unbuilt
         * follow-up work. This entity places its single representative
         * kind-thumbnail exactly as picked, no edge-blending yet. */
        fprintf(f, "METHOD       | Close                | CLOSE\n");
        fclose(f);
    }

    /* Real menu.chtpm generation, same as tp_place_desktop.c's own
     * identical block (ENTITY-MENU-LEGACY-DEPRECATION-PLAN.md Phase 1) -
     * every new entity gets one the same moment it's created. */
    {
        char conv_path[PATH_BUF], conv_cmd[PATH_BUF * 2];
        snprintf(conv_path, sizeof(conv_path), "%s/*.monads/*.livedesk-taskbar/ops/meta_to_menu_chtpm.py", house_root);
        snprintf(conv_cmd, sizeof(conv_cmd), "python3 '%s' '%s' >/dev/null 2>&1", conv_path, dir);
        int rc = system(conv_cmd);
        (void)rc; /* honest no-op on failure, same fallback tp_place_desktop.c documents */
    }

    /* remember last place in widget state */
    char last[PATH_BUF];
    snprintf(last, sizeof(last), "%s/last_desktop_place_rmmv.txt", wdir);
    f = fopen(last, "w");
    if (f) {
        fprintf(f, "path=%s\n", dir);
        fprintf(f, "tileset=%s\n", tileset);
        fprintf(f, "category=%s\n", category);
        fclose(f);
    }

    printf("DESKTOP_TILE_RMMV %s tileset=%s category=%s\n", dir, tileset, category);

    /* Click position comes from the real, permanent master ledger
     * (nav_master_ledger.txt under desk_root/#.desktop - khtpm_entity_
     * menu_render.c writes a real "RMMV_CLICK pid=... x=... y=..." line
     * the instant it captures the click, synchronously, decoupled from
     * whether THIS op ever runs or succeeds). Scans the WHOLE ledger for
     * the LAST matching line, since it's real, permanent, append-only
     * history, not a single-purpose transient file. Falls back to (0,0)
     * if genuinely no click was ever logged - same fallback the DESK-row
     * append below always needs a real x/y for regardless. */
    int click_x = 0, click_y = 0;
    {
        char ix[32] = "", iy[32] = "";
        char ledger_path[PATH_BUF];
        snprintf(ledger_path, sizeof(ledger_path), "%s/nav_master_ledger.txt", desk_root);
        FILE *lf = fopen(ledger_path, "r");
        if (lf) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), lf)) {
                if (strncmp(line, "RMMV_CLICK ", 11) != 0) continue;
                char *xp = strstr(line, " x=");
                char *yp = strstr(line, " y=");
                if (xp) { sscanf(xp + 3, "%31s", ix); }
                if (yp) { sscanf(yp + 3, "%31s", iy); }
            }
            fclose(lf);
        }
        if (ix[0] && iy[0]) { click_x = atoi(ix); click_y = atoi(iy); }

        char pos_path[PATH_BUF];
        snprintf(pos_path, sizeof(pos_path), "%s/desktop_pos.txt", dir);
        FILE *pf = fopen(pos_path, "w");
        if (pf) { fprintf(pf, "x=%d\ny=%d\n", click_x, click_y); fclose(pf); }

        /* REAL, NEW 2026-08-29, direct instruction ("it should say what
         * coord was last placed on desktop if something was placed"):
         * reuses the same armed-note file/poll mechanism (khtpm_entity_
         * menu_render.c already watches this path and swaps the
         * picker's title text on change) - tp_arm_placer_rmmv.c unlinks
         * it on every real exit, this write here replaces that with a
         * real "placed" message instead of leaving it simply gone. */
        char note_path[PATH_BUF];
        snprintf(note_path, sizeof(note_path), "%s/rmmv_armed.txt", wdir);
        FILE *nf = fopen(note_path, "w");
        if (nf) {
            fprintf(nf, "Placed %s/%s \"%s\" at (%d,%d)\n", tileset, category, kind_label, click_x, click_y);
            fclose(nf);
        }
    }

    /* Real DESK-row append - THE actual fix. pals_rel is relative to
     * house_root, matching every other real DESK row's own path
     * convention (khtpm_taskbar_manager.c's livedesk_pals_rel()). */
    {
        char pals_rel[PATH_BUF];
        size_t hl = strlen(house_root);
        snprintf(pals_rel, sizeof(pals_rel), "%s/%s", pals_root + hl + 1, name);
        append_desk_row(desk_pdl, name, pals_rel, click_x, click_y, glyph);
    }

    /* Real duplicate-spawn guard - `pgrep -f`'s argument is EXTENDED
     * REGEX, so the literal '.'/'+' in the binary's own ".+x" filename
     * suffix must be escaped or this silently matches far more than
     * intended (found live 2026-08-29 testing this exact op's earlier
     * version). */
    {
        char pgrep_cmd[PATH_BUF * 2];
        snprintf(pgrep_cmd, sizeof(pgrep_cmd), "pgrep -f 'tp_desktop_window_rgb\\.\\+x %s' >/dev/null 2>&1", dir);
        if (system(pgrep_cmd) == 0) {
            printf("DESKTOP_TILE_RMMV %s: window already running, not spawning a duplicate\n", dir);
            free(house_root);
            return 0;
        }
    }

    /* Spawn the real, live GL entity window - same setsid-detach
     * convention tp_place_desktop.c uses (outlive the calling terminal). */
    {
        char exe_path[PATH_BUF], spawn_cmd[PATH_BUF * 2];
        snprintf(exe_path, sizeof(exe_path), "%s/*.monads/*.livedesk-taskbar/ops/+x/tp_desktop_window_rgb.+x", house_root);
        snprintf(spawn_cmd, sizeof(spawn_cmd), "setsid '%s' '%s' >/dev/null 2>&1 < /dev/null &", exe_path, dir);
        int rc = system(spawn_cmd);
        (void)rc;
    }
    free(house_root);

    return 0;
}
