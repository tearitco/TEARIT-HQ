/* khtpm_taskbar_manager.h — SHARED toolbar/taskbar design logic
 * (renamed from khtpm_taskbar_core.h 2026-08-10 — this file plays the same
 * role as CHTPM's own "manager" (e.g. game_manager.c): the real business
 * logic a layout/parser will call into, not layout data itself).
 * ONE logic set for Linux + Windows (WIN-COMPAT-RULE). Plat only draws.
 */
#ifndef KHTPM_TASKBAR_MANAGER_H
#define KHTPM_TASKBAR_MANAGER_H

#include <stddef.h>


void read_key_value(const char *path, const char *key, char *out, size_t out_sz);
#ifdef __cplusplus
extern "C" {
#endif

#define KTB_PATH_BUF 4352
#define KTB_MAX_TABS 64
#define KTB_MAX_SHORTCUTS 16
#define KTB_BAR_H 36
#define KTB_TAB_W 160
#define KTB_CLOSE_W 36
#define KTB_SHORTCUT_W 32

/* livedesk_* registry logic constants (ported from tp_taskbar.c's own
 * LIVEDESK_GRID_PX / LIVEDESK_DYN_MAX / LIVEDESK_MAX_OPEN, KTB-prefixed
 * to match this file's naming convention). */
#define KTB_LIVEDESK_GRID_PX 80   /* matches GRID_CELL_PX in tp_desktop_window.c */
#define KTB_LIVEDESK_DYN_MAX 24
#define KTB_LIVEDESK_MAX_OPEN 64
/* REAL FIX 2026-08-12, direct report ("some showed up on bottom tb then
 * dissapeared. only 2 are registered"): this was OFF, so the manager's
 * own read-prune-write-rename cycles on livedesk_open.txt (load_tabs()
 * below, and any other mutator in this file) ran completely unlocked -
 * meanwhile tp_desktop_window.c/tp_desktop_window_rgb.c's own
 * LIVEDESK_USE_REGISTRY_LOCK was already 1. Two writers to the same
 * file, only one side actually taking the flock() - a real lost-update
 * race, not a flaky test harness. Traced live: 7 simultaneously-
 * launched entities, only 2 survived in the registry after the
 * manager's own unlocked rewrite stomped the other 5's freshly-locked
 * additions. Flipped to 1 - now BOTH sides take the same real
 * cross-process flock() on livedesk_registry.lock. */
#ifndef KTB_LIVEDESK_USE_REGISTRY_LOCK
#define KTB_LIVEDESK_USE_REGISTRY_LOCK 1
#endif

typedef struct {
    int pid;
    int nav;                 /* shared live nav number */
    char entity[128];
    char path[KTB_PATH_BUF];
} KtbTab;

typedef struct {
    char glyph[16];
    char command[KTB_PATH_BUF];
} KtbShortcut;

/* Popup menu row for the hq (session/desk/pals) popup menus built by the
 * livedesk_build_*_menu() functions below. Ported verbatim from
 * tp_taskbar.c's HQMenuItem. */
typedef struct {
    char label[64];
    char command[KTB_PATH_BUF];
    int nav; /* shared nav-claim number while this row's popup is open */
} HQMenuItem;

typedef struct {
    char house_root[KTB_PATH_BUF];
    char pid_path[KTB_PATH_BUF];
    KtbTab tabs[KTB_MAX_TABS];
    int n_tabs;
    KtbShortcut shortcuts[KTB_MAX_SHORTCUTS];
    int n_shortcuts;
    char theme_bg[32];
    char theme_fg[32];
    /* Nav digit buffer (terminal-style middle input) */
    char digit_buf[16];
    int digit_len;
    int tab_focus_idx; /* keyboard cursor among tabs */
    int nav_armed;

    /* --- Added for khtpm_strip_parser HQ popup port (2026-08-11): top-left
     * HQ/user/file/desks window state. hq_open is 0 (closed) or which
     * header menu is showing (1=session, 2=desk, 3=pals) - the 4th header
     * ("save-as") is a direct action with no row list, so it never sets
     * hq_open, it opens cli-io straight away. New fields only, nothing
     * above this comment was touched - see file header constraint. */
    int hq_open;
    HQMenuItem hq_menu[KTB_LIVEDESK_DYN_MAX];
    int hq_n_menu;
    int hq_focus;
    int hq_digit_accum;

    /* --- cli-io (save-as / rename-desk) text-input modal state, ported
     * from tp_taskbar.c's g_cliio_* globals (see cliio_open()/cliio_open_
     * save_as()/agent_relay_dispatch()'s cli-io branch). */
    int cliio_active;
    int cliio_typing;
    char cliio_buffer[256];
    char cliio_op[32];       /* "save-as" or "rename-desk" */
    char cliio_sroot[KTB_PATH_BUF];
    char cliio_id[128];
    char cliio_desk[64];
    int cliio_focus;         /* 0 = text field, 1 = cancel row */

    /* Set by ktb_hq_activate() when the real HQ menu's "X.quit" row (see
     * ktb_hq_open() which==5) is activated - mirrors tp_taskbar.c's own
     * agent_relay_dispatch() "quit" command branch (calls
     * quit_and_save_session() then exits main's event loop). ktb_hq_activate()
     * itself has no access to the caller's g_running flag, so it just raises
     * this flag and the caller (dispatch_code() in khtpm_taskbar_manager_main.c)
     * performs the actual ktb_quit_and_save()+exit, then clears it. */
    int hq_quit_requested;

    /* --- Full 12-cell top strip port, pass 2 (2026-08-11): closes the
     * remaining tp_taskbar.c draw_strip() header-cell gaps (USER cell,
     * file/player submenus, inert palettes/edit/db/plugins/store/network
     * cells, unified nav-focus across header+tabs). Cell order/indices
     * (0-based, "which" passed to ktb_hq_open() below is index+1) mirrors
     * load_strip_config()+main()'s cells[] assembly EXACTLY: 0=HQ,
     * 1=USER, 2=file, 3=desks, 4=pals, 5=palettes, 6=edit, 7=player,
     * 8=db, 9=plugins, 10=menus, 11=store, 12=network, 13=ai, 14=date/time.
     * hq_open/hq_menu/hq_focus/hq_digit_accum above are REUSED for
     * whichever of these 15 cells is currently showing a submenu (only
     * HQ/file/desks/pals/player ever populate hq_menu; the rest are inert
     * placeholders - see khtpm_taskbar_manager.c's ktb_hq_open()). */
    char strip_user_cmd[KTB_PATH_BUF]; /* USER cell's command, from livedesk_taskbar.pdl's strip_user_cmd key - empty by default, matching legacy's load_strip_config() default (no user-switcher wired in the legacy itself either) */
    int strip_focus_cell; /* unified header-cell cursor: 0..14 = a strip cell, -1 = focus is on a tab instead (see ktb_nav_focus_delta()) */
    /* REAL, NEW 2026-08-16, direct correction ("the cells aren't
     * supposed to be hardcoded... that's an oversight") - real
     * position(1-based)->id table, read once at startup from
     * #.desktop/livedesk_header_cell_ids.txt (written by the SEPARATE
     * strip_parser process, the only one with real access to
     * khtpm_strip_header.chtpm's own parsed button id= attributes - see
     * that file's own write_header_cell_ids() comment for the full
     * real cross-process reasoning). Lets ktb_hq_open() check a cell's
     * real, data-declared identity (e.g. "toys") before falling back to
     * the existing which==N chain - additive, doesn't change any
     * existing cell's own behavior. */
    int cell_id_pos[15]; /* real literal, matches KTB_STRIP_N_CELLS (defined just below - can't use the macro itself before its own definition) */
    char cell_id_str[15][64];
    int n_cell_ids;
} KtbState;

#define KTB_STRIP_N_CELLS 15

void ktb_init(KtbState *s, const char *house_root);
/* REAL, NEW 2026-08-16 - see KtbState's own cell_id_pos/cell_id_str field comment. */
void ktb_load_cell_ids(KtbState *s);
const char *ktb_cell_id(const KtbState *s, int which);
void ktb_write_pidfile(KtbState *s, int pid);
void ktb_unlink_pidfile(const KtbState *s);

/* Reload tabs from livedesk_open (prune dead), sync nav claims, shortcuts, theme */
void ktb_reload(KtbState *s);

/* Activate tab by index: write ACTIVATE + OPEN_CONTEXT to interact_relay */
void ktb_activate_tab(KtbState *s, int idx);

/* Jump by shared NAV number: tab raise/open menu, or ACTIVATE_NAV to package */
void ktb_jump_nav(KtbState *s, int nav_n);

/* Digit buffer */
void ktb_digit_clear(KtbState *s);
void ktb_digit_push(KtbState *s, char c);
void ktb_digit_backspace(KtbState *s);
void ktb_digit_enter(KtbState *s); /* parse buffer → jump_nav */

/* Focus move among tabs */
void ktb_focus_delta(KtbState *s, int delta);

/* Quit+save: rewrite autostart LAUNCH rows from open tabs (portable paths) */
void ktb_quit_and_save(KtbState *s);

/* Layout helpers for plat drawing */
int ktb_close_x0(int screen_w);
int ktb_shortcuts_x0(int screen_w, int n_shortcuts);
int ktb_tab_index_at_x(int x, int n_tabs, int tabs_right);
int ktb_shortcut_index_at_x(int x, int screen_w, int n_shortcuts);

/* Portable path strip for shortcut commands */
void ktb_action_portable(const char *in, char *out, size_t out_sz);

int ktb_pid_alive(int pid);

/* ---------------------------------------------------------------------
 * HQ popup menu + cli-io modal wrappers (added for the khtpm_strip_parser
 * port of tp_taskbar.c's top-left HQ/user/file/desks window, 2026-08-11).
 * These call the existing static livedesk_build_*_menu()/livedesk_switch_
 * desk()/livedesk_new_desk()/livedesk_rename_desk()/livedesk_save_as_
 * with_name()/livedesk_place_pal() logic already in khtpm_taskbar_
 * manager.c - none of that logic is reimplemented here, only exposed.
 * ------------------------------------------------------------------- */

/* which: 1=session menu, 2=desk menu, 3=pals menu, 4=save-as (direct
 * action - opens cli-io immediately, sets no menu), 5=the real HQ button's
 * own menu (tp_taskbar.c's load_hq_config()/cell[0] - $.restart / X.quit /
 * cancel, data-driven from livedesk_taskbar.pdl's hq_menu_N_label/cmd rows,
 * added 2026-08-11 to restore the legacy's only real quit path now that the
 * bottom bar's close-X - confirmed dead code in tp_taskbar.c's draw_bar(),
 * CLOSE_BTN_W is defined but never drawn there - has been removed). */
void ktb_hq_open(KtbState *s, int which);
void ktb_hq_close(KtbState *s);
void ktb_hq_focus_delta(KtbState *s, int delta);
void ktb_hq_digit(KtbState *s, int d);
/* Runs hq_menu[row].command (switch-desk / new-desk / edit-desk / open-
 * session / pal placement / cancel), mirroring run_popup_row()'s dispatch. */
void ktb_hq_activate(KtbState *s, int row);

void ktb_cliio_open_save_as(KtbState *s);
void ktb_cliio_open_rename_desk(KtbState *s); /* seeds buffer with the current active desk's name */
void ktb_cliio_open_new_user(KtbState *s); /* stage 1 of 2 (user_id) - see khtpm_taskbar_manager.c header comment */
void ktb_cliio_close(KtbState *s);
void ktb_cliio_focus_delta(KtbState *s, int delta); /* Up/Down row toggle in nav mode, see khtpm_taskbar_manager.c */
void ktb_cliio_start_typing(KtbState *s);
void ktb_cliio_stop_typing(KtbState *s);
void ktb_cliio_type(KtbState *s, char c);
void ktb_cliio_backspace(KtbState *s);
/* Enter while typing: dispatches rename-desk/save-as/new-user with the
 * buffer. new-user-id re-opens as new-user-name instead of closing (2-stage
 * sequential signup, see ktb_cliio_open_new_user()). */
void ktb_cliio_submit(KtbState *s);

/* ---------------------------------------------------------------------
 * Full 12-cell strip wrappers, pass 2 (2026-08-11) - see KtbState's own
 * header comment above for the cell/which mapping.
 * ------------------------------------------------------------------- */

/* Runs the USER cell's command (strip_user_cmd), mirroring tp_taskbar.c's
 * open_cell_popup() cmd-branch for a cell with no submenu: closes whatever
 * popup is open (cells[idx].n_menu==0 path always calls close_popups()
 * first, unconditionally), then shells out strip_user_cmd if non-empty. A
 * true no-op when strip_user_cmd is empty, exactly like the legacy - no
 * "switch user" command exists in tp_taskbar.c itself, only the same empty-
 * by-default config hook. */
void ktb_strip_user_activate(KtbState *s);

/* Real gap fix (2026-08-11, direct request: "the button vars for
 * user/file/desk") — live username / file / desks labels for the header's
 * USER/file/desks cells, published by khtpm_taskbar_manager_main.c as new
 * strip_var_* fragments the layout substitutes via ${username}/
 * ${file_label}/${desks_label}, same pattern as strip_tabs etc. */
void ktb_get_username(const KtbState *s, char *out, size_t sz);
void ktb_get_file_label(const KtbState *s, char *out, size_t sz);
void ktb_get_desks_label(const KtbState *s, char *out, size_t sz);
/* REAL FIX 2026-08-30 - see khtpm_taskbar_manager.c's own header comment
 * on this function: exported so publish_var_fragments() can resolve real
 * pals-dropdown sprite paths, same cross-file pattern as the three above. */
int livedesk_pals_root(const char *house_root, char *out, size_t sz);
void ktb_get_avatar_dir(const KtbState *s, char *out, size_t sz);

/* Unified header-cell + tab focus cursor, ported from tp_taskbar.c's
 * nav_focus_step()/nav_focus_apply(): steps through [15 strip cells][tabs]
 * as ONE list, wrapping at both ends. Replaces ktb_focus_delta() for
 * top-level (no popup open) Left/Right - ktb_focus_delta() itself is left
 * untouched per the file's own edit constraints. */
void ktb_nav_focus_delta(KtbState *s, int delta);
/* Right-click "arm nav" — ports tp_taskbar.c's button==3 handling on both
 * strip_win and win (main(), ~lines 3688/3725): close_popups() first
 * (mirrored here as ktb_hq_close(), the only "popup" this port has open at
 * top level), then nav_armed=1, unified cursor forced to index 0 (the
 * header cell), digit buffer cleared. See khtpm_strip_codes.h's
 * KSC_NAV_ARM for the wire code that reaches this. */
void ktb_nav_arm(KtbState *s);

/* Enter at top level (no popup, no cli-io): if a strip cell has focus,
 * opens it (ktb_hq_open/ktb_strip_user_activate as appropriate) - mirrors
 * open_cell_popup() being invoked from agent_relay_dispatch()'s armed-Enter
 * branch when strip_focus_cell >= 0. Falls back to the existing
 * ktb_digit_enter()'s tab-activation behavior otherwise (digit buffer /
 * focused tab), left completely untouched. */
void ktb_nav_enter(KtbState *s);


void read_key_value(const char *path, const char *key, char *out, size_t out_sz);
#ifdef __cplusplus
}
#endif
void read_key_value(const char *path, const char *key, char *out, size_t out_sz);
#endif
