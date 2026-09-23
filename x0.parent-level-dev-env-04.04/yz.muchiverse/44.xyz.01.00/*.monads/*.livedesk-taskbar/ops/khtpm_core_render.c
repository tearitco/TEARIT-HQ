#define _POSIX_C_SOURCE 200809L /* CLOCK_MONOTONIC + getline() under -std=c11 strict mode - bumped from 199309L 2026-08-16 for chai_load_ledger()'s real getline() fix, see that function's own header comment */
#include <stdarg.h> /* 2026-09-11 - kh_focus_debug_log()'s va_list, TEMPORARY diagnostic logging */
/* khtpm_entity_menu_render.c — entity context menu, Stage 2c PROOF
 * (2026-08-16, direct instruction: "oh use chtpm. its standard" -
 * overriding the smaller module-only-bolt-on option initially
 * recommended). ONE-ENTITY TEST CASE ONLY - see local-2do-15.txt's own
 * entity-context-menu entry for the full reasoning/rollout plan. Every
 * OTHER entity still uses tp_desktop_window_rgb.c's own built-in popup
 * engine (objects.pdl/meta.pdl) until this is proven live on ava first.
 *
 * Real .chtpm tag vocabulary, 1:1 with objects.pdl's own real semantics
 * (same action-string convention dispatch_action() already uses - a
 * real shell command, "CLOSE", "void", plus objects.pdl's own "GOTO:
 * <page>"/"BACK" reserved forms for multi-page nav):
 *   <window class="entity-menu">
 *     <page name="main">
 *       <item label="..." action="..."/>
 *       ...
 *     </page>
 *     <page name="other-page"> ... </page>
 *   </window>
 * <page name="..."> uses e->id to hold the page name (reusing the
 * shared Elem struct's existing field, not a new one). <item>'s action
 * string lives in e->onclick (also an existing Elem field) - label
 * holds the visible text, onclick holds the command, matching that
 * field's own original purpose.
 *
 * Real entity decoding (2026-08-16 finding): this parser only supports
 * double-quote-delimited attribute values with NO entity decoding
 * anywhere else in this house's khtpm family - action strings need
 * literal " characters (for "$0"-style var quoting inside their own
 * sh -c '...' wrappers), so apply_attr() decodes &quot;/&amp; for the
 * "action" attribute specifically - real, minimal XML entity decoding,
 * only the 2 entities actually needed, not a general-purpose scheme.
 *
 * Shares khtpm_render_core.c (Elem struct + hit_test/find_by_tag/
 * find_by_id) with db-hq/events-hq/chat-hai - a REAL step toward Stage
 * 2c's eventual convergence, not just proximity - this is genuinely the
 * 4th consumer of that shared core.
 *
 * Usage: khtpm_entity_menu_render.+x <package_dir> <house_root>
 * (matches dispatch_action()'s own existing calling convention exactly -
 * package_dir first, house_root second - so tp_desktop_window_rgb.c's
 * eventual integration point doesn't need a different argv shape). */
#include "khtpm_css_parser.h"
#include "khtpm_render_core.c" /* real .c, not a header - see that file's own comment */
#include "khtpm_reparse_diff.c" /* 2026-09-11 - real keyed tree diff/patch, see 08-roadmap/design-docs/CHTPM-INCREMENTAL-REPARSE-DESIGN.md. Wired in behind g_use_incremental_reparse, OFF by default - see that flag's own declaration comment. */
/* khtpm_taskbar_manager.h/.c removed 2026-09-01 - real, confirmed dead
 * linkage: ktb_init()/ktb_quit_and_save() (the only reason db-hq mode
 * ever needed it) were already removed from this file in an earlier
 * pass this same session; this build was still linking the entire
 * ~4,300-line khtpm_taskbar_manager.c object for zero real symbol use
 * (verified: this file builds and links clean with khtpm_taskbar_
 * manager.c dropped from the link line entirely - see
 * build_core_render.sh's own updated comment). Also the real house
 * standard clarified directly this session: no cross-.c linking to
 * share behavior within one binary - either genuinely the same file,
 * or a separate process talking over fork/exec+file IPC (which is
 * exactly what khtpm_taskbar_manager_main.+x already does with its
 * own real, unrelated compile of khtpm_taskbar_manager.c - untouched,
 * still real, still needed there). */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h> /* INT_MIN - g_win_pos_applied_x/y sentinel (redraw() corrective-move guard) */
#include <string.h>
#include <ctype.h>
#include <dirent.h> /* REAL, chat-hai mode only - session-dir listing */
#include <fcntl.h> /* REAL, NEW 2026-09-01 - strip mode's own zorder toggle respawn (open("/dev/null", O_RDWR)) */
#include <unistd.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/time.h> /* REAL, NEW 2026-09-01 - tile mode's own real gettimeofday() frame-pacing/click-vs-drag timing */
#include <sys/wait.h> /* REAL, db-hq mode only - launch_module()/cleanup_module(), real fork()+execl() */
#include <errno.h>
#include <signal.h> /* REAL, db-hq mode only - handle_term_signal() */
extern char **environ;
#include <time.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h> /* 2026-08-24 - XA_WINDOW for the XdndAware property (XDND drop support) */
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>
#include <X11/extensions/shape.h> /* REAL, NEW 2026-09-01 - tile mode's own real per-pixel window shape (build_shape_mask()/cursword_update_shape()), folded in verbatim from tp_desktop_window_rgb.c */
#include <math.h> /* REAL, NEW 2026-09-01 - tile mode's own real raymarch camera math (fabs/sqrt/sin/cos/tan), folded in verbatim from tp_desktop_window_rgb.c */
#define M_PI_LOCAL 3.14159265358979323846 /* same real, portable local constant tp_desktop_window_rgb.c's own file already used, not relying on glibc's own optional M_PI */
#include <locale.h> /* REAL, NEW 2026-09-01 - tile mode's own setlocale()/XSetLocaleModifiers() for its popup fontset */
#include <libgen.h> /* REAL, NEW 2026-09-01 - tile mode's own dirname()/basename() (self_exe_path/piece_id) */
#include <sys/file.h> /* REAL, NEW 2026-09-01 - tile mode's own real flock() cross-process popup mutex */

/* Orchestrator-owned PID teardown — PROC-LIFECYCLE-ORCHESTRATOR-TEARDOWN.md.
 * This is its own binary (separate from khtpm_taskbar_manager_main.+x),
 * so it carries the IMPL. build_core_render.sh passes -I "$SHARED".
 * _POSIX_C_SOURCE is already set at line 1, so the header's own
 * _GNU_SOURCE fallback stays inert. */
#define KH_PROC_REGISTRY_IMPL
#include "kh_proc_registry.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "lib/stb_image_write.h"

#define PATH_BUF 4096
#include "khtpm_ui_common.c" /* shared with khtpm_entity.c - see its header */
#include "khtpm_grid_jump.c" /* pure <grid> jump/cursor logic, shared with overlay pickers - see its header */
/* REAL Stage 5 §5d.10 (2026-08-16) - bumped 256->512 to match db-hq's
 * own original headroom (khtpm_hq_render.c) now that db-hq mode's own
 * 15-tab/sidebar/panel tree shares this same pool. */
static void popup_handle_click(int px, int py);
static void handle_key(KeySym ks, char ch);
static void grid_col_to_letters(int col, char *out, size_t outsz); /* defined near default_grid_handle_key() - needed earlier by dispatch()'s own CSVH_GRIDCOMMIT-style handlers */
static void history_path(char *out, size_t outsz);
static void zero_nav_subtree(Elem *e); /* generic: recursively zero nav_index (see its definition) */
static void kh_grab_keyboard_retry(void);
static void kh_capture_click(int x, int y, int button);
static void kh_capture_key(KeySym ks, char ch);
static void redraw(void); /* REAL, forward declaration needed for dispatch()'s OPACITY_MINUS/OPACITY_PLUS handlers (NEW 2026-08-29 TASK 2) */
static void kh_raise_and_focus(Window w); /* fwd - dispatch()'s FOCUSWIN handler uses it, defined near hq_dispatch_xevent */
static void kh_open_cli_io_context_menu(Elem *target, int win_px, int win_py); /* fwd - hq_dispatch_xevent's ButtonPress (button 3) uses it, defined near close_context_menu */
static void kh_poll_cli_io_ctxmenu_action(void); /* fwd - hq_idle_tick() polls this; defined near kh_open_cli_io_context_menu */
static int kh_key_history_code(KeySym ks, char ch); /* fwd - handle_key()'s interact-relay forward uses it before its real definition, near kh_capture_key */
static void desktop_toggle_click_two_step(const char *house_root); /* fwd - dispatch()'s CLICK_TWOSTEP_TOGGLE handler uses it before its real definition, near desktop_load_click_two_step */
static void desktop_set_font_scale(const char *house_root, int pct); /* fwd - dispatch()'s UI_SCALE_MINUS/PLUS handlers */
static void desktop_set_font_family(const char *house_root, const char *name); /* fwd - dispatch()'s UI_FONT_FAMILY_NEXT/PREV handlers */
static void desktop_load_click_two_step(const char *house_root); /* fwd - hq_ui_pdl_reload_if_changed() (hq_idle_tick(), long-running dock strip) uses it before its real definition */
static void reload_font_ui(void); /* fwd - hq_ui_pdl_reload_if_changed() re-sizes the chrome font on a live font_scale change */
static void kh_text_areas_reload(Elem *root); /* fwd - reparse_chtpm_if_changed() re-hydrates <text_area> buffers, defined near default_text_area_save */
static void kh_ensure_dock_peer_window(void); /* fwd - hq_idle_tick()'s own per-tick self-heal call; real def + header comment near main() */
static void kh_cli_io_reload(Elem *root); /* fwd - reparse_chtpm_if_changed() re-hydrates <cli_io> buffers, defined near kh_text_areas_reload */
static void kh_drop_zone_unregister(void);
static Elem *kh_find_input_by_key(Elem *root, const char *key); /* fwd - reparse_chtpm_if_changed() re-arms a cli_io/text_area across a live reparse without releasing the keyboard grab it already holds */
static void kh_focus_debug_log(const char *fmt, ...); /* fwd - TEMPORARY diagnostic logging, see its own definition comment (network-browser recurring focus bug) */
static Elem *elem_new(const char *tag); /* fwd - kh_pool_alloc() (CHTPM-INCREMENTAL-REPARSE-DESIGN.md) calls this before its real definition */
#define MAX_ELEMS 1024  /* 2026-09-02: page projection + chrome, was 512 */
#define MAX_PAGE_STACK 8

static Elem g_pool[MAX_ELEMS];
static int g_n_elems = 0;
/* 2026-09-11 - CHTPM-INCREMENTAL-REPARSE-DESIGN.md §3/§1. OFF by
 * default (0) - reparse_chtpm_if_changed() keeps its existing,
 * unmodified, always-a-full-rebuild behavior unless this is
 * explicitly turned on. Read once at startup from #.desktop/hq_ui.pdl
 * (incremental_reparse=1) by hq_ui_pdl_reload_if_changed()'s own real
 * PDL-load site - a runtime toggle, not a recompile, so the design
 * doc's mandatory dock-first rollout order can be tested window by
 * window without rebuilding between steps. */
static int g_use_incremental_reparse = 0;
/* g_pool's own free-list (design doc §3) - ONLY used by the
 * incremental path (g_use_incremental_reparse=1). The existing
 * full-rebuild path (g_n_elems=0 on every reparse) never populates or
 * drains this - it's a no-op, unused array for every window that
 * doesn't opt in, zero behavior change for the default path. Slots
 * freed by a genuinely-removed Elem (kh_pool_free(), the
 * KhDiffAllocator's free_fn) get pushed here; kh_pool_alloc() pops one
 * before falling back to elem_new()'s own bump allocator. */
static int g_pool_free_idx[MAX_ELEMS];
static int g_pool_free_n = 0;
static Elem *kh_pool_alloc(void *ctx) {
    (void)ctx;
    if (g_pool_free_n > 0) {
        Elem *e = &g_pool[g_pool_free_idx[--g_pool_free_n]];
        memset(e, 0, sizeof(*e));
        return e;
    }
    return elem_new(""); /* falls through to the existing bump allocator against the SAME g_pool - real tag gets set by kh_diff_apply_template() right after this returns */
}
static void kh_pool_free(void *ctx, Elem *e) {
    (void)ctx;
    if (g_pool_free_n >= MAX_ELEMS) return; /* real cap, matches g_pool's own - should be unreachable (can't free more slots than exist) */
    int idx = (int)(e - g_pool);
    if (idx < 0 || idx >= MAX_ELEMS) return; /* not one of ours - defensive, should never happen */
    g_pool_free_idx[g_pool_free_n++] = idx;
}
/* Scratch pool for the NEW/candidate tree the incremental path parses
 * into (design doc §1) - completely separate from g_pool, discarded
 * (its own g_n_elems_next reset to 0) after every diff/patch attempt,
 * success or failure. parse_chtpm()/elem_new() are retargeted to bump
 * into THIS array for the duration of that one parse via
 * g_elem_pool_target/g_elem_n_target (see elem_new()'s own updated
 * body) - both default to &g_pool/&g_n_elems so every OTHER caller of
 * elem_new() (the existing full-rebuild path, popup/context-menu
 * parses, everything) is completely unaffected. */
static Elem g_pool_next[MAX_ELEMS];
static int g_n_elems_next = 0;
static Elem *g_elem_pool_target = g_pool;
static int *g_elem_n_target = &g_n_elems;
static char g_chtpm_path[PATH_BUF];  /* real, generic (2026-08-31) - the real .chtpm this process was launched against, kept for the generic live-reparse capability below */

/* Entity-menu identity strip for the title bar (2026-09-09, direct
 * report: "entities just say ^main; they used to have entity name +
 * uid & pid"). Composed once from g_chtpm_path when this process is
 * rendering an entity context menu (.../<entity>/menu.chtpm): the
 * fallback title becomes "<entity> <iid> <pid4>" instead of the bare
 * page name "main". Empty for every other window (HQ windows carry a
 * real <window label="...">, so they never hit this fallback). */
static char g_entity_ident[128] = "";
static void kh_compose_entity_ident(void) {
    if (g_entity_ident[0] || !g_chtpm_path[0]) return;
    const char *slash = strrchr(g_chtpm_path, '/');
    const char *base = slash ? slash + 1 : g_chtpm_path;
    if (strcmp(base, "menu.chtpm") != 0) return;   /* only entity menus */
    /* dir = g_chtpm_path without the trailing "/menu.chtpm" */
    char dir[PATH_BUF];
    size_t dl = (size_t)(slash - g_chtpm_path);
    if (dl >= sizeof(dir)) return;
    memcpy(dir, g_chtpm_path, dl); dir[dl] = '\0';
    const char *ds = strrchr(dir, '/');
    const char *ename = ds ? ds + 1 : dir;         /* the entity dir name */
    char iid[32] = "";
    char ipath[PATH_BUF];
    snprintf(ipath, sizeof(ipath), "%s/instance_id.txt", dir);
    FILE *f = fopen(ipath, "r");
    if (f) { if (fgets(iid, sizeof(iid), f)) iid[strcspn(iid, "\r\n")] = '\0'; fclose(f); }
    if (iid[0])
        snprintf(g_entity_ident, sizeof(g_entity_ident), "%s %s %04d",
                 ename, iid, (int)getpid() % 10000);
    else
        snprintf(g_entity_ident, sizeof(g_entity_ident), "%s %04d",
                 ename, (int)getpid() % 10000);
}
/* REAL FIX 2026-09-01 (live report: open-hai's own real projection
 * never got picked up after a fresh launch - the bootstrap-then-
 * manager-writes-real-content sequence happens fast enough, especially
 * right after button.sh's own bootstrap-restore cp, that both writes
 * can land within the SAME whole second - plain time_t/st_mtime has
 * only 1-second resolution, so `st.st_mtime == g_chtpm_mtime` can be
 * spuriously true even though the file's real content already changed
 * out from under it, silently skipping the reparse forever until some
 * LATER write finally crosses into the next second. Real fix: track
 * the full nanosecond-resolution struct timespec (st_mtim, real glibc/
 * POSIX field) instead - immune to this exact race by construction. */
static struct timespec g_chtpm_mtime = {0, 0};
static Elem *g_dock_peer;
static char g_dock_peer_path[PATH_BUF];
static struct timespec g_dock_peer_mtime;
static Window g_dock_peer_win;
static Pixmap g_dock_peer_buf;
static XftDraw *g_dock_peer_xft;
static GC g_dock_peer_gc;
static int g_dock_peer_buf_w, g_dock_peer_buf_h;
static int g_dock_peer_x, g_dock_peer_y, g_dock_peer_w, g_dock_peer_h;
static int g_dock_header_nav_hi;
static int g_dock_click_peer;
static int g_dock_click_menu;
static Window g_dock_kbd_win;
static int g_dock_visible_rows = 1;
static int g_dock_packed_rows = 1;
static Elem g_dock_plus_elem, g_dock_minus_elem;
/* MILESTONE B/C - generic <footer> row pager (same idea as the dock
 * +/- above, for any sidebar+panel window's footer). g_footer_vis_rows
 * survives redraws; clamped against g_footer_total_rows each layout. */
static Elem g_footer_more_elem, g_footer_less_elem;
static int g_footer_vis_rows = 1;
static int g_footer_total_rows = 1;
static int g_dock_in_peer_paint;
static int g_dock_in_menu_paint;
static Window g_dock_menu_win;
static Pixmap g_dock_menu_buf;
static XftDraw *g_dock_menu_xft;
static GC g_dock_menu_gc;
static int g_dock_menu_buf_w, g_dock_menu_buf_h;
static int g_dock_menu_sx, g_dock_menu_sy, g_dock_menu_w, g_dock_menu_h;
static int g_zorder_above = 0;
static void load_zorder_mode(const char *house_root) {
    g_zorder_above = 0;
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/khtpm_zorder_mode.state.txt", house_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[64];
    if (fgets(line, sizeof(line), f) && strstr(line, "mode=above")) g_zorder_above = 1;
    fclose(f);
}
static void save_zorder_mode(const char *house_root, int above) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/khtpm_zorder_mode.state.txt", house_root);
    FILE *f = fopen(path, "w");
    if (f) { fprintf(f, "mode=%s\n", above ? "above" : "normal"); fclose(f); }
    snprintf(path, sizeof(path), "%s/#.desktop/livedesk_override_redirect.pdl", house_root);
    f = fopen(path, "w");
    if (f) { fprintf(f, "override_redirect=%s\n", above ? "true" : "false"); fclose(f); }
}
/* REAL, NEW 2026-09-01 - when the pdl says managed, a window still needs
 * the WM's ordinary cooperation to LOOK like this house's own window
 * (undecorated, no shell chrome, sinkable): _MOTIF_WM_HINTS
 * decorations=0 removes the titlebar/frame, WM_DELETE_WINDOW keeps the
 * existing close path working, and _NET_WM_STATE SKIP_TASKBAR|SKIP_PAGER
 * keeps it out of the shell's dock/overview exactly like an
 * override_redirect surface already is. Shared by every window-creation
 * site in this file that can be toggled managed. */
static void render_managed_wm_hints(Display *dpy, Window win, int managed) {
    if (!managed) return;
    Atom mh = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
    long hints[5] = { 2, 0, 0, 0, 0 }; /* flags=MWM_HINTS_DECORATIONS, decorations=0 */
    XChangeProperty(dpy, win, mh, mh, 32, PropModeReplace, (unsigned char *)hints, 5);
    Atom wdel = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &wdel, 1);
    Atom _net_state = XInternAtom(dpy, "_NET_WM_STATE", False);
    Atom skip_tb = XInternAtom(dpy, "_NET_WM_STATE_SKIP_TASKBAR", False);
    Atom skip_pg = XInternAtom(dpy, "_NET_WM_STATE_SKIP_PAGER", False);
    if (_net_state != None && skip_tb != None && skip_pg != None) {
        Atom states[2] = { skip_tb, skip_pg };
        XChangeProperty(dpy, win, _net_state, XA_ATOM, 32, PropModeReplace, (unsigned char *)states, 2);
    }
}
/* REAL, NEW 2026-09-01 - the second half of "normal" mode, called AFTER
 * the window is mapped + its first XSync: respawning re-maps the window
 * on top (Mutter puts freshly-mapped windows there), which would have put
 * livedesk ABOVE the native apps the moment the toggle runs. One
 * XConfigureWindow stack-mode Below flips it to the bottom where it
 * belongs - the WM honors restack requests for managed windows (proven
 * live: _NET_WM_STATE_ABOVE sticks on the managed taskbar strips). */
static void render_managed_sink_below(Display *dpy, Window win) {
    if (g_override_redirect) return;
    XSync(dpy, False);
    XWindowChanges wc;
    wc.stack_mode = Below;
    XConfigureWindow(dpy, win, CWStackMode, &wc);
    XSync(dpy, False);
}

static void apply_dock_window_hints(Display *dpy, Window w, int x, int y) {
    Atom motif_hints = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
    long hints[5] = { 2, 0, 0, 0, 0 };
    XChangeProperty(dpy, w, motif_hints, motif_hints, 32, PropModeReplace, (unsigned char *)hints, 5);
    Atom wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
    Atom above = XInternAtom(dpy, "_NET_WM_STATE_ABOVE", False);
    Atom skip_taskbar = XInternAtom(dpy, "_NET_WM_STATE_SKIP_TASKBAR", False);
    Atom skip_pager = XInternAtom(dpy, "_NET_WM_STATE_SKIP_PAGER", False);
    Atom sticky = XInternAtom(dpy, "_NET_WM_STATE_STICKY", False);
    if (g_zorder_above) {
        Atom states[4] = { above, skip_taskbar, skip_pager, sticky };
        XChangeProperty(dpy, w, wm_state, XA_ATOM, 32, PropModeReplace, (unsigned char *)states, 4);
    } else {
        Atom states[3] = { skip_taskbar, skip_pager, sticky };
        XChangeProperty(dpy, w, wm_state, XA_ATOM, 32, PropModeReplace, (unsigned char *)states, 3);
    }
    Atom win_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    Atom dock = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
    XChangeProperty(dpy, w, win_type, XA_ATOM, 32, PropModeReplace, (unsigned char *)&dock, 1);
    XSizeHints *shints = XAllocSizeHints();
    if (shints) { shints->flags = PPosition | USPosition; shints->x = x; shints->y = y; XSetWMNormalHints(dpy, w, shints); XFree(shints); }
    XWMHints *wh = XAllocWMHints();
    if (wh) {
        wh->flags = InputHint | StateHint;
        wh->input = True;
        wh->initial_state = NormalState;
        XSetWMHints(dpy, w, wh);
        XFree(wh);
    }
}
/* REAL, NEW 2026-08-29, direct instruction ("the tb has a
 * transparency. but that should propagate to 'all entities' and menu
 * screens (including tb dropdowns... context/hq etc) so player can
 * still see thru their desktop a bit") - real, working opacity
 * ALREADY exists (khtpm_strip_parser.c's own set_window_opacity()/
 * load_theme_opacity(), the taskbar's own real _NET_WM_WINDOW_OPACITY
 * + #.desktop/livedesk_theme.pdl "COLOR|opacity|N" convention) but was
 * never ported into THIS file - the merged renderer that now handles
 * db-hq/events-hq/chat-hai/popups/context-menus, i.e. everything the
 * user is describing as "full opacity" today. Ported verbatim (same
 * real logic, adapted to this file's own PATH_BUF/snprintf convention
 * instead of khtpm_strip_parser.c's path_join2/SP_PATH_BUF) rather
 * than sharing code across files for two small, pure functions with
 * no other dependencies. */
/* REAL, NEW 2026-09-01 - forward decl so the shared every-mode tick
 * (~line 10234) can call this before its own real definition further
 * down (~line 11034, still named pchq_ for its original single call
 * site, but genuinely generic - a plain house_root string + a shared
 * dirty-marker file). */
static int pchq_theme_changed_dirty(const char *house_root);






static double load_theme_opacity(void) {
    double opacity = 1.0;
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/livedesk_theme.pdl", g_house_root);
    FILE *f = fopen(path, "r");
    if (!f) return opacity;
    char line[PATH_BUF];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "COLOR", 5) != 0) continue;
        char *p = strchr(line, '|');
        if (!p) continue;
        p++;
        while (*p == ' ') p++;
        char *end = strchr(p, '|');
        if (!end) continue;
        char *key_end = end;
        while (key_end > p && key_end[-1] == ' ') key_end--;
        char key[16];
        size_t klen = (size_t)(key_end - p);
        if (klen == 0 || klen >= sizeof(key)) continue;
        memcpy(key, p, klen);
        key[klen] = '\0';
        if (strcmp(key, "opacity") != 0) continue;
        char *v = end + 1;
        while (*v == ' ') v++;
        v[strcspn(v, "\r\n")] = '\0';
        if (v[0] == '\0') continue;
        double parsed = atof(v);
        if (parsed >= 0.0 && parsed <= 1.0) opacity = parsed;
    }
    fclose(f);
    return opacity;
}

/* REAL, NEW 2026-08-29 (TASK 2: opacity control) - write a new opacity value
 * to the livedesk_theme.pdl file. Reads the entire file, updates the COLOR|
 * opacity line, and rewrites the file (preserving all other lines intact). */
static void write_theme_opacity(double opacity) {
    if (opacity < 0.0) opacity = 0.0;
    if (opacity > 1.0) opacity = 1.0;

    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/livedesk_theme.pdl", g_house_root);

    /* Read existing file to preserve all lines */
    FILE *f = fopen(path, "r");
    if (!f) return;

    char lines[16][PATH_BUF];
    int n_lines = 0;
    char line[PATH_BUF];
    int opacity_line_idx = -1;

    while (fgets(line, sizeof(line), f) && n_lines < 16) {
        if (strncmp(line, "COLOR", 5) == 0) {
            char *p = strchr(line, '|');
            if (p) {
                p++;
                while (*p == ' ') p++;
                char *end = strchr(p, '|');
                if (end) {
                    char *key_end = end;
                    while (key_end > p && key_end[-1] == ' ') key_end--;
                    char key[16];
                    size_t klen = (size_t)(key_end - p);
                    if (klen > 0 && klen < sizeof(key)) {
                        memcpy(key, p, klen);
                        key[klen] = '\0';
                        if (strcmp(key, "opacity") == 0) {
                            opacity_line_idx = n_lines;
                        }
                    }
                }
            }
        }
        snprintf(lines[n_lines], sizeof(lines[n_lines]), "%s", line);
        n_lines++;
    }
    fclose(f);

    /* If no opacity line found, don't create one - only update existing */
    if (opacity_line_idx < 0) return;

    /* Write the file back with the updated opacity line */
    FILE *fw = fopen(path, "w");
    if (!fw) return;

    for (int i = 0; i < n_lines; i++) {
        if (i == opacity_line_idx) {
            fprintf(fw, "COLOR        | opacity              | %.2f\n", opacity);
        } else {
            fputs(lines[i], fw);
        }
    }
    fclose(fw);

    /* REAL, NEW 2026-08-30, direct instruction ("it only needs to
     * happen on status change. it doesn't have to continuously poll
     * if settings buttons aren't being pressed. what in house
     * architecture can be used to support this") - same real, cheap
     * "changed marker" convention this house already uses everywhere
     * (frame_changed.txt/strip_frame_changed.txt/pc_screen_changed.txt
     * - see frame_changed_dirty()'s own real shape in
     * khtpm_strip_parser.c): a real, tiny append-only file whose SIZE
     * a consumer's ALREADY-RUNNING event-select loop checks once per
     * tick via a single stat() - near-zero cost, no new timer, no
     * heavy poll, and it only does real work (reload+reapply opacity)
     * on an actual change, exactly matching the direct instruction.
     * Written here so BOTH direct opacity edits (this settings screen)
     * and any future write_theme_opacity() caller mark the change the
     * same real way, without each caller needing to remember to. */
    {
        char marker_path[PATH_BUF];
        snprintf(marker_path, sizeof(marker_path), "%s/#.desktop/livedesk_theme_changed.txt", g_house_root);
        FILE *mf = fopen(marker_path, "a");
        if (mf) { fprintf(mf, "%.2f\n", opacity); fclose(mf); }
    }
}
/* REAL, db-hq mode only (§5d.10) - module launch, ported VERBATIM from
 * khtpm_hq_render.c (real fork()+execl(), already TPMOS-compliant - see
 * that file's own header comment, "explain to me your plan and why its
 * different from the tpmos/wraith examples"). Harmless when g_is_db_hq
 * is 0 (never called). */

/* REAL, generic module launcher (xperiments/khtpm-generic-dispatch-
 * design.md §2a, 2026-08-31) - collapses what used to be 3 near-
 * identical per-mode fork+execl copies (dbhq_launch_module()/
 * evhq_launch_module()/chai_launch_module()) into one real function
 * with zero project knowledge: every argument comes from either the
 * already-parsed <module> Elem (src/extra_arg) or generic context
 * (house_root/package_dir), never a hardcoded path or class check.
 * First real use: dbhq_launch_module() below now delegates to this
 * instead of forking itself - a pure, verifiable substitution (same
 * exact argv, same exact behavior) - the real proof-of-mechanism test
 * before events-hq/chat-hai/network-browser are migrated onto it too.
 * Returns the child pid (or -1 on fork failure), same as a bare
 * fork() - caller owns the pid the same way it always did. */
/* Resolve one module token to an absolute path: absolute stays as-is;
 * a relative token is tried against package_dir then house_root, and
 * left unchanged if neither exists (execv will then error visibly). */
static void lm_resolve(const char *tok, const char *house_root,
                       const char *package_dir, char *out, size_t outsz) {
    if (tok[0] == '/') { snprintf(out, outsz, "%s", tok); return; }
    char cand[PATH_BUF];
    if (package_dir && package_dir[0]) {
        snprintf(cand, sizeof(cand), "%s/%s", package_dir, tok);
        if (access(cand, F_OK) == 0) { snprintf(out, outsz, "%s", cand); return; }
    }
    if (house_root && house_root[0]) {
        snprintf(cand, sizeof(cand), "%s/%s", house_root, tok);
        if (access(cand, F_OK) == 0) { snprintf(out, outsz, "%s", cand); return; }
    }
    snprintf(out, outsz, "%s/%s", house_root ? house_root : ".", tok);
}

static pid_t launch_module(const char *src, const char *house_root, const char *package_dir, const char *extra_arg) {
    if (!src || !src[0]) return -1;

    /* src may be a single path OR (tpmos convention) an interpreter +
     * its own args, space-separated - e.g.
     *   <module src="&.widgits/_shared-lib/system/+x/prisc+x.+x pal/foo.pal"/>
     * Every whitespace token before house_root/package_dir is a real
     * argv entry; a relative one is resolved via lm_resolve(). A plain
     * one-token src (the compiled-manager case) is unchanged. */
    char work[PATH_BUF * 2];
    snprintf(work, sizeof(work), "%s", src);

    char resolved[8][PATH_BUF];
    char *argv[16];
    int argc = 0;
    char *save = NULL;
    for (char *tok = strtok_r(work, " \t", &save);
         tok && argc < 8;
         tok = strtok_r(NULL, " \t", &save)) {
        lm_resolve(tok, house_root, package_dir, resolved[argc], PATH_BUF);
        argv[argc] = resolved[argc];
        argc++;
    }
    if (argc == 0) return -1;
    argv[argc++] = (char *)house_root;
    argv[argc++] = (char *)package_dir;
    if (extra_arg && extra_arg[0]) argv[argc++] = (char *)extra_arg;
    argv[argc] = NULL;

    pid_t pid = fork();
    if (pid == 0) {
        if (house_root)   setenv("KHTPM_HOUSE", house_root, 1);
        if (package_dir) { setenv("KHTPM_PKG", package_dir, 1);
                           setenv("PRISC_PROJECT_ROOT", package_dir, 1); }
        execv(argv[0], argv);
        _exit(1);
    } else if (pid < 0) {
        fprintf(stderr, "khtpm_entity_menu_render: launch_module: fork failed for %s\n", argv[0]);
    }
    return pid;
}

/* fork EVERY <module> in the tree (chtpm carries several, like an HTML
 * page carries several <script src>): one view/shell + one logic
 * module per tab. Each gets house_root + package_dir (+ its own id as
 * argv[3]). All pids tracked so cleanup kills them all. */
#define KH_MAX_MODULES 16
static pid_t g_module_pids[KH_MAX_MODULES];
static int g_n_module_pids = 0;

static void kh_cleanup_modules(void) {
    for (int i = 0; i < g_n_module_pids; i++)
        if (g_module_pids[i] > 0) {
            kill(g_module_pids[i], SIGTERM);
            waitpid(g_module_pids[i], NULL, WNOHANG);
        }
    g_n_module_pids = 0;
    /* PROC-LIFECYCLE: drop this render's owned rows from the canonical
     * ledger (the SIGTERM loop above already stopped them; this rewrites
     * the file without them so a later reap_all / prune has nothing
     * stale to chase). grace_ms 1 -> the already-dead modules are
     * skip-stale, so it's effectively just the rewrite. */
    if (g_house_root[0])
        kh_proc_reap_subtree(g_house_root, (long)getpid(), 1, 0);
}

/* Real def + full header comment near g_arg3_dir further down (events-
 * hq's real 4-arg launch shape needs this) - declared here, ABOVE its
 * first real use in kh_collect_and_launch_modules() just below, since
 * that function is itself defined long before g_arg3_dir's own section
 * of the file. */
static char g_arg4_entity_label[PATH_BUF] = "";
static void kh_collect_and_launch_modules(Elem *e, const char *house_root, const char *package_dir) {
    if (!e) return;
    for (int i = 0; i < e->n_children; i++) {
        Elem *c = e->children[i];
        if (strcmp(c->tag, "module") == 0 && c->label[0] &&
            g_n_module_pids < KH_MAX_MODULES) {
            /* REAL FIX 2026-09-13 - see g_arg4_entity_label's own decl
             * comment. A <module> tag's own explicit id= always wins
             * (unchanged, existing behavior for every other mode); only
             * events-hq's real launch (argv[4] captured into this
             * global) falls back here, and only when the module didn't
             * already specify its own id. Confirmed live via the fixed
             * run_visible_window_events_hq_demo.sh harness - without
             * this, khtpm_events_hq_manager.+x's own argc<4 check exits
             * immediately with a usage error on every fresh launch. */
            const char *extra = c->id[0] ? c->id : (g_arg4_entity_label[0] ? g_arg4_entity_label : NULL);
            pid_t p = launch_module(c->label, house_root, package_dir, extra);
            if (p > 0) {
                g_module_pids[g_n_module_pids++] = p;
                /* PROC-LIFECYCLE: track every <module> in the canonical
                 * ledger, owned by THIS render (master = getpid()). It's
                 * in the render's process group so a house quit reaches
                 * it via the group-kill anyway; registering also lets
                 * prune keep the ledger honest and a kill -9'd render's
                 * modules still get reaped house-wide. */
                if (house_root && house_root[0]) {
                    /* name must be a single whitespace-free token (the
                     * ledger line is space-delimited). c->label is the
                     * whole `src="..."` string, so use the module id, or
                     * a fixed label. */
                    const char *mn = (c->id[0]) ? c->id : "module";
                    kh_proc_register_owned(house_root, (long)p, (long)p,
                                           (long)getpid(), mn);
                }
            }
        }
        kh_collect_and_launch_modules(c, house_root, package_dir);
    }
}

/* launch all <module>s of a window: write module_parent.pid once, then
 * fork each. atexit cleanup registered on first use. */
static void kh_launch_window_modules(Elem *window, const char *house_root, const char *package_dir) {
    if (!window) return;
    char ppp[PATH_BUF];
    snprintf(ppp, sizeof(ppp), "%s/module_parent.pid", package_dir);
    FILE *pf = fopen(ppp, "w");
    if (pf) { fprintf(pf, "%d\n", (int)getpid()); fclose(pf); }
    int before = g_n_module_pids;
    kh_collect_and_launch_modules(window, house_root, package_dir);
    if (g_n_module_pids > before) {
        static int registered = 0;
        if (!registered) { atexit(kh_cleanup_modules); registered = 1; }
    }
}

static Elem *elem_new(const char *tag) {
    /* 2026-09-11 - bumps whichever pool g_elem_pool_target/
     * g_elem_n_target currently point at. Both default to g_pool/
     * &g_n_elems (set at their own declaration) - every existing call
     * site, and every window that never touches the incremental path,
     * sees IDENTICAL behavior to before this change. Only
     * kh_parse_into_scratch() (the incremental path's own candidate
     * parse) retargets these, briefly, around its one parse_chtpm()
     * call - see that function's own comment. */
    if (*g_elem_n_target >= MAX_ELEMS) return NULL;
    Elem *e = &g_elem_pool_target[(*g_elem_n_target)++];
    memset(e, 0, sizeof(*e));
    snprintf(e->tag, sizeof(e->tag), "%s", tag);
    return e;
}

/* ---------- tiny generic tag-tree parser (same shape as db-hq/events-hq's
 * own hand-rolled parser, not reinvented) ---------- */
static void skip_ws(const char **p) { while (**p && isspace((unsigned char)**p)) (*p)++; }

/* Count of malformed bytes the tag-tree parser had to skip in the
 * current parse_chtpm() call (see the forward-progress guard in
 * parse_element()'s child loop). >0 means a not-well-formed template -
 * almost always a bare `"` / `<` / `>` / `&` in a ${var} value that
 * kh_substitute_vars() couldn't escape (a value spliced OUTSIDE a
 * quoted attribute, e.g. free `>${x}<` between tags). The parser
 * degrades gracefully (skips the byte, keeps rendering) rather than
 * spinning or aborting - this counter just makes it non-silent. */
static long g_parse_skipped_bytes = 0;

/* Paranoia cap: even with the forward-progress guard, bound the total
 * loop iterations of one parse_chtpm() call so a future no-progress
 * regression in SOME OTHER parser loop can't hang the process either.
 * Set to ~4x the (post-substitution) template length + a floor; a
 * well-formed parse is well under 1x. 0 = disabled (no parse running). */
static long g_parse_budget = 0;
static int  g_parse_cap_hit = 0;
/* Set by parse_chtpm() for the MAIN window's template only: how many
 * bytes the last parse skipped, and a sticky "this window's template is
 * not well-formed" flag the redraw() title code shows as "⚠ malformed
 * template". Cleared when a later reparse of the same template is
 * clean. */
static long g_last_parse_skipped = 0;
static int  g_window_malformed = 0;
#define KH_PARSE_STEP() do { \
    if (g_parse_budget > 0 && --g_parse_budget <= 0) { \
        g_parse_cap_hit = 1; return p + strlen(p); \
    } } while (0)

static void parse_attr_value(const char **p, char *out, size_t outsz) {
    skip_ws(p);
    if (**p != '"') { out[0] = '\0'; return; }
    (*p)++;
    size_t n = 0;
    while (**p && **p != '"') { if (n + 1 < outsz) out[n++] = **p; (*p)++; }
    out[n] = '\0';
    if (**p == '"') (*p)++;
}

/* Real, minimal XML entity decode - ONLY &quot;/&amp;, the 2 this file's
 * own action= values actually need (see this file's own header comment
 * for why - real shell commands embed literal " for their own "$0"-style
 * var quoting). Decodes in place. */
static void decode_entities(char *s) {
    /* REAL BUG FIX 2026-08-18, direct live investigation (book-stack's
     * "Read" menu item did nothing, no error, no menu - see
     * bookstack-path-bug.txt): this function's own header comment
     * claimed only &quot;/&amp; needed support, but book-stack's own
     * real action= string (menu.chtpm) also uses &gt; (from its own
     * "2>/dev/null" shell redirects inside nested $(find ...) command
     * substitutions, HTML-attribute-encoded like everything else in
     * that string). Undecoded &gt; fell through to the else branch
     * UNCHANGED (literal 4-char text "&gt;", not ">"), corrupting
     * "2>/dev/null" into "2&gt;/dev/null" - which /bin/sh parses as
     * `find ... -type d 2` (extra literal arg "2", real find error) `&`
     * (background) `gt` (nonexistent command) `/dev/null` (its arg) -
     * a genuinely broken pipeline, not a cosmetic glitch. This silently
     * emptied out both $(find "$H" ...) substitutions in book-stack's
     * real Read action, so MUTA_ROOT/READER_PATH ended up empty and the
     * final `exec` failed with nothing visible (backgrounded, stdout/
     * stderr redirected to /dev/null by dispatch()'s own wrapper) -
     * exactly matching the live, reported symptom. &amp; MUST be
     * decoded LAST among the entities that start with '&' (matches the
     * standard HTML-entity-decode ordering rule) so a real "&amp;gt;"
     * sequence in source data isn't double-decoded into ">" - not a
     * concern for this file's own real, hand-authored action strings
     * today, but the safe, correct order regardless. */
    char *r = s, *w = s;
    while (*r) {
        if (strncmp(r, "&quot;", 6) == 0) { *w++ = '"'; r += 6; }
        else if (strncmp(r, "&gt;", 4) == 0) { *w++ = '>'; r += 4; }
        else if (strncmp(r, "&lt;", 4) == 0) { *w++ = '<'; r += 4; }
        else if (strncmp(r, "&amp;", 5) == 0) { *w++ = '&'; r += 5; }
        else *w++ = *r++;
    }
    *w = '\0';
}

/* 2026-08-24 - data-driven X11 XDND drop support (first consumer:
 * bookmarks' drag-a-dir-onto-the-window). A <window drop_action="...">
 * attribute opts THIS window into being a real XDND drop target: on a
 * drop of a text/uri-list selection, the first dropped path is
 * exported as $DROP_PATH and drop_action is run exactly like dispatch()
 * runs item actions (same "$0"=package_dir/"$1"=house_root positional
 * convention) - but WITHOUT setting g_quit, because a drop should not
 * end the window's session the way picking an item does. Windows
 * without the attribute never attach XdndAware and are byte-for-byte
 * unchanged (zero risk to the 7 existing menu.chtpm entities).
 *
 * House-history note (why real XDND is safe HERE when gl_mirror.c
 * removed it): gl_mirror's real-Xdnd block died for two documented
 * reasons - GLUT+WM-reparenting broke its own window self-lookup for
 * attaching XdndAware, and its check_xdnd_events() idle poll had no
 * CPU throttle (crashed the machine once). Neither hazard exists in
 * this renderer: we create and keep our own Window id directly (no
 * lookup), and the popup loop below is a blocking select()+XNextEvent
 * with a 150ms cap - attaching XDND costs zero idle CPU. */
static char g_drop_action[1024] = "";
static char g_drop_highlight_color[16] = "#88ff66";
static int g_drop_highlight = 0;
static char g_drop_hover_name[128] = "";

static void apply_attr(Elem *e, const char *name, const char *val) {
    if (strcmp(name, "id") == 0 || strcmp(name, "name") == 0) {
        snprintf(e->id, sizeof(e->id), "%s", val);
    } else if (strcmp(name, "class") == 0) {
        char tmp[128]; snprintf(tmp, sizeof(tmp), "%s", val);
        char *tok = strtok(tmp, " ");
        while (tok && e->n_classes < CSS_MAX_CLASSES) {
            snprintf(e->classes[e->n_classes], sizeof(e->classes[0]), "%s", tok);
            e->n_classes++;
            tok = strtok(NULL, " ");
        }
    } else if (strcmp(name, "label") == 0) {
        /* REAL FIX 2026-08-31 (found live testing open-hai's own real
         * projection: a real session snippet containing "&.widgits"
         * showed as the literal 5-char text "&amp;.widgits" on screen)
         * - decode_entities() already existed and was already applied
         * to action=/onclick= (see that branch's own 2026-08-25 fix
         * comment), just never ported to label= - the one attribute
         * every generic <text>/<item> projection actually displays. */
        char decoded[sizeof(e->label)];
        snprintf(decoded, sizeof(decoded), "%s", val);
        decode_entities(decoded);
        snprintf(e->label, sizeof(e->label), "%s", decoded);
    } else if (strcmp(name, "action") == 0 || strcmp(name, "onClick") == 0 || strcmp(name, "onclick") == 0) {
        /* REAL FIX 2026-08-25 (Stage 2 palettes migration, direct live
         * report: "no emojis just blank glyph... no navs"). This parser
         * only ever recognized the attribute NAME "action" - db-hq's own
         * dashboard.chtpm happens to use that name, so it always worked
         * there. Palettes' own .chtpm (composed by palettes_menu.sh) uses
         * the house's OTHER real onClick= convention (matching
         * khtpm_hq_render.c's own attr_ci_eq(name,"onclick") and every
         * tb-native dropdown row) - that attribute was being silently
         * ignored entirely, so e->onclick never got set, which explains
         * BOTH missing symptoms at once: no sprite (draw_elem() only
         * blits when e->sprite[0], covered separately below, but even
         * with that fixed nothing was numbered) AND no nav (assign_
         * palettes_nav()'s own `e->onclick[0]` check was always false). */
        char decoded[sizeof(e->onclick)];
        snprintf(decoded, sizeof(decoded), "%s", val);
        decode_entities(decoded);
        snprintf(e->onclick, sizeof(e->onclick), "%s", decoded);
    } else if (strcmp(name, "sprite") == 0) {
        /* REAL FIX 2026-08-25 (Stage 2 palettes migration) - ported from
         * khtpm_hq_render.c's own apply_attr() (attr_ci_eq(name,
         * "sprite")) - was entirely missing here, so e->sprite never got
         * set regardless of draw_elem()'s own sprite-blit support.
         * REAL FIX 2026-09-08 (live report: "palettes tile sets no
         * longer rendering"): sprite= is a filesystem path fed from a
         * projector's ${t.sprite}, and this house's own dirs are named
         * `&.widgits/` `&.hq-apps/`. Since experiment/xhtpm-attr-var-
         * escaping, kh_substitute_vars() XML-escapes a ${var} spliced
         * inside a quoted attribute, so that `&` arrives here as
         * `&amp;` - `.../&amp;.widgits/...`, a path that does not exist,
         * so every sprite blit silently failed. decode_entities() the
         * same as label=/onclick=/action= already do; the escaping was
         * artificial, undoing it restores the real path. */
        char decoded[sizeof(e->sprite)];
        snprintf(decoded, sizeof(decoded), "%s", val);
        decode_entities(decoded);
        snprintf(e->sprite, sizeof(e->sprite), "%s", decoded);
    } else if (strcmp(name, "src") == 0) {
        /* REAL Stage 5 §5d.10 (2026-08-16) - db-hq mode only, ported
         * from khtpm_hq_render.c's own apply_attr(): <module src="..."/>
         * real, wraith_parser_alpha.c convention. Reused e->label to
         * hold it - <module> elements are never drawn, safe reuse.
         * 2026-09-08 - decode_entities() for the same reason sprite=
         * does above (a ${var}-fed src path under &.widgits/ arrives
         * XML-escaped since experiment/xhtpm-attr-var-escaping). */
        char decoded[sizeof(e->label)];
        snprintf(decoded, sizeof(decoded), "%s", val);
        decode_entities(decoded);
        snprintf(e->label, sizeof(e->label), "%s", decoded);
    } else if (strcmp(name, "args") == 0) {
        /* REAL, NEW 2026-08-25 (palettes manager port) - optional extra
         * static argv for a <module>, e.g. <module src="palettes_
         * manager.+x" args="emojis"/> so ONE manager binary can serve
         * multiple category windows (palettes-emojis.chtpm/palettes-
         * elements.chtpm/...) and know which category it's publishing
         * for. Reused e->id - same "module elements are never drawn,
         * safe reuse" reasoning src= already uses for e->label. */
        snprintf(e->id, sizeof(e->id), "%s", val);
    } else if (strcmp(name, "target_id") == 0) {
        /* REAL, NEW 2026-08-31 (generic capability #2 - see Elem's own
         * target_id field comment in khtpm_render_core.c) - real,
         * generic <cli_io target_id="..."/> attribute, ported from
         * chtpm_parser.c's own UIElement.target_id. */
        snprintf(e->target_id, sizeof(e->target_id), "%s", val);
    } else if (strcmp(name, "backspace_action") == 0) {
        /* REAL, NEW 2026-09-01 - see Elem's own backspace_action field
         * comment in khtpm_render_core.c. Decoded exactly like action=/
         * onclick= (a real shell command, same &quot;/&amp;/&gt;/&lt;
         * entity set). */
        char decoded[sizeof(e->backspace_action)];
        snprintf(decoded, sizeof(decoded), "%s", val);
        decode_entities(decoded);
        snprintf(e->backspace_action, sizeof(e->backspace_action), "%s", decoded);
    } else if (strcmp(name, "relay") == 0) {
        /* REAL, NEW 2026-09-04 - generic "Interact Mode" capability,
         * ported from pc-hq's own hand-rolled run_pchq_board_mode()
         * key-forward (see g_interact_relay_on's own declaration
         * comment). Comma-separated target file path(s); this item's
         * own class="interact-engaged" (data-driven, from a projector's
         * ${var}, same convention every active/selected state in this
         * house already uses) is what arms/disarms it - not a click. */
        snprintf(e->relay, sizeof(e->relay), "%s", val);
    } else if (strcmp(name, "rows") == 0) {
        /* REAL, NEW 2026-09-01 - see Elem's own rows field comment in
         * khtpm_render_core.c. */
        e->rows = atoi(val);
    } else if (strcmp(name, "value") == 0) {
        /* REAL, NEW 2026-09-14 - generic `<bar>` progress/playhead value
         * (see Elem's own bar_value comment in khtpm_render_core.c).
         * Same arbitrary unit as max=; v1 consumers (network-browser's
         * video nav row) publish centiseconds. Zero effect on any tag
         * that isn't <bar>. */
        e->bar_value = atoi(val);
    } else if (strcmp(name, "max") == 0) {
        /* REAL, NEW 2026-09-14 - generic `<bar>` upper bound (see the
         * value= branch directly above; max=0 means "no fill"). */
        e->bar_max = atoi(val);
    } else if (strcmp(name, "content") == 0) {
        /* REAL, NEW 2026-09-05 (pdl-read's own real "display a data-
         * driven page of text" need, found live: putting multi-line
         * content in label= broke the frame round-trip - label is
         * never escaped for real newlines the way text_area_buffer
         * is). A real, generic way to set a <text_area>'s initial
         * buffer content from a template var - unlike live-typed
         * content, `val` here has ALREADY had kh_substitute_vars()'s
         * own "\n" -> real-newline conversion applied (this attribute
         * value is substituted before the XML re-parse that calls
         * apply_attr()), so a direct copy is correct. Only meaningful
         * for text_area (cli_io stays single-line, unused by
         * anything else) - harmless no-op attribute for any other tag. */
        /* EXPERIMENT 2026-09-05 (experiment/xhtpm-attr-var-escaping):
         * kh_substitute_vars() now XML-escapes a ${var} value spliced
         * inside a quoted attribute, so a `"`/`<`/`>`/`&` in the doc
         * page no longer breaks the re-parse. Undo that here, same as
         * label=/action= already do. */
        if (strcmp(e->tag, "text_area") == 0) {
            char decoded[sizeof(e->text_area_buffer)];
            snprintf(decoded, sizeof(decoded), "%s", val);
            decode_entities(decoded);
            snprintf(e->text_area_buffer, sizeof(e->text_area_buffer), "%s", decoded);
        } else if (strcmp(e->tag, "cli_io") == 0) {
            /* REAL, NEW 2026-09-11, direct live report ("if i use
             * existing tab, it doesn't let me backspace w/e data is in
             * there. is there no record of it or something") - real
             * root cause: nothing ever seeded a cli_io's input_buffer
             * (what typing/Backspace actually edit) from anything - a
             * manager-projected label= (network-browser's address bar:
             * the loaded URL) looked fully populated but the real,
             * editable input_buffer underneath was always empty. First
             * fix reused label= itself to seed input_buffer, but label=
             * is ALSO drawn as a literal prefix before input_buffer for
             * every cli_io (chat-hai's "&gt; " prompt is the same
             * mechanism) - for network-browser specifically, where
             * label WAS the whole URL, that doubled the text on screen
             * (label + input_buffer both showing the same URL back to
             * back). Real fix: cli_io gets the exact same content=
             * seed-attribute text_area already has, right above -
             * UNCONDITIONAL copy, no "only if empty" guard needed,
             * because reparse_chtpm_if_changed()'s reparse-diff engine
             * already protects input_buffer on any MATCHED element
             * (kh_diff_apply_template() never touches it) exactly the
             * same way it already protects text_area_buffer - proven
             * by the SAME unconditional pattern one branch up. label=
             * stays free to be a short, real prefix (or empty) for
             * every cli_io, including this one, with zero special
             * casing. */
            char decoded[sizeof(e->input_buffer)];
            snprintf(decoded, sizeof(decoded), "%s", val);
            decode_entities(decoded);
            snprintf(e->input_buffer, sizeof(e->input_buffer), "%s", decoded);
        }
    } else if (strcmp(name, "bg") == 0) {
        /* REAL, NEW 2026-09-04, direct live request ("can we add grey
         * and brown to swatch colors... that shouldn't be hardcoded,
         * should be from layout/module") - see Elem.bg's own field
         * comment (khtpm_render_core.c) and draw_elem()'s own
         * consuming comment (khtpm_draw_core.c) for the full generic-
         * capability rationale. `val` is typically a `${var}`-
         * substituted hex string from a real manager/projector, same
         * substitution pass every other attribute already gets. */
        snprintf(e->bg, sizeof(e->bg), "%s", val);
    } else if (strcmp(name, "drop_action") == 0) {
        /* 2026-08-24 - see the g_drop_action block comment above.
         * Window-level attr; decoded through the SAME entity decoder
         * action= uses, so &quot;/&amp;/&gt;/&lt; all behave
         * identically for shell quoting inside drop actions. */
        char decoded[sizeof(g_drop_action)];
        snprintf(decoded, sizeof(decoded), "%s", val);
        decode_entities(decoded);
        snprintf(g_drop_action, sizeof(g_drop_action), "%s", decoded);
    } else if (strcmp(name, "drop_highlight") == 0) {
        snprintf(g_drop_highlight_color, sizeof(g_drop_highlight_color), "%s", val[0] ? val : "#88ff66");
    }
}

static const char *parse_element(const char *p, Elem *parent) {
    skip_ws(&p);
    if (*p != '<') return p;
    p++;
    if (*p == '!') {
        const char *end = strstr(p, "-->");
        return end ? end + 3 : p + strlen(p);
    }
    char tag[32]; size_t tn = 0;
    while (*p && !isspace((unsigned char)*p) && *p != '>' && *p != '/') {
        if (tn + 1 < sizeof(tag)) tag[tn++] = *p;
        p++;
    }
    tag[tn] = '\0';
    Elem *e = elem_new(tag);
    /* REAL BUG FIX 2026-09-11 (gdb-traced crash: a real, large loaded
     * page - 4chan's own /b/ front page, ~250+ real content rows,
     * each one 4 real Elems per the network-browser template's own
     * <repeat> body even though only one of the four ever shows -
     * pushed g_pool past MAX_ELEMS=1024, elem_new() returned NULL, and
     * this line dereferenced it unconditionally: `e->parent = parent`
     * on a NULL e). elem_new()'s own documented contract (see its
     * declaration's 2026-08-26 comment, "elem_new() returns NULL,
     * guarded call sites just skip adding content") was never actually
     * honored HERE, the one call site every other element in the house
     * ultimately funnels through - a real, root-cause gap, not a
     * network-browser-specific one (any window's page, given enough
     * content, was one large-enough load away from this same crash).
     * Real fix: once the pool is genuinely exhausted, there is no
     * recovering a partial parse - stop parsing HERE, at the exact
     * point of exhaustion, same "run out of string" contract this
     * function's own closing-tag branch a few lines down already uses
     * (`return end ? end + 1 : p + strlen(p);`) - the tree is silently
     * truncated at MAX_ELEMS rather than crashing; every already-parsed
     * element up to this point is real and rendered normally. */
    if (!e) return p + strlen(p);
    e->parent = parent;
    if (parent && parent->n_children < MAX_CHILDREN) parent->children[parent->n_children++] = e;

    /* generic show="..." (CHTPM-ARCHITECTURE-FIX.md): with ${var} now
     * resolved at buffer level, a static template can gate a whole
     * element on a state value - show="" / "0" / "false" drops it from
     * the tree, anything else keeps it. Missing show= => always shown,
     * so every existing template is unchanged. Handles the common
     * self-closing case; a dropped element with children still parses
     * its subtree (detached) - fine for leaf widgets like the ones
     * signup-hq gates. */
    int drop_elem = 0;
    for (;;) {
        KH_PARSE_STEP();
        skip_ws(&p);
        if (*p == '/' && p[1] == '>') {
            p += 2;
            if (drop_elem && parent && parent->n_children > 0 &&
                parent->children[parent->n_children - 1] == e)
                parent->n_children--;
            return p;
        }
        if (*p == '>') { p++; break; }
        if (!*p) return p;
        char attr[32]; size_t an = 0;
        while (*p && *p != '=' && !isspace((unsigned char)*p) && *p != '>' && *p != '/') {
            if (an + 1 < sizeof(attr)) attr[an++] = *p;
            p++;
        }
        attr[an] = '\0';
        skip_ws(&p);
        char val[1024] = "";
        if (*p == '=') { p++; parse_attr_value(&p, val, sizeof(val)); }
        if (attr[0]) {
            if (strcmp(attr, "show") == 0)
                drop_elem = (val[0] == '\0' || strcmp(val, "0") == 0 || strcmp(val, "false") == 0);
            else
                apply_attr(e, attr, val);
        }
    }
    if (drop_elem && parent && parent->n_children > 0 &&
        parent->children[parent->n_children - 1] == e)
        parent->n_children--;

    for (;;) {
        KH_PARSE_STEP();
        skip_ws(&p);
        if (!*p) return p;
        if (p[0] == '<' && p[1] == '/') {
            const char *end = strchr(p, '>');
            return end ? end + 1 : p + strlen(p);
        }
        /* REAL FIX 2026-09-05, direct live report ("pdl-read window
         * won't open from the tb-sub menu anymore ... WM related?").
         * gdb-traced to a hard 100%-CPU infinite loop right here,
         * BEFORE the window ever maps: parse_element()'s own first line
         * is `if (*p != '<') return p;` - it returns p UNCHANGED for
         * any byte that isn't a tag opener. This loop then calls it
         * again on the same byte, forever.
         *
         * How stray non-'<' text ends up mid-stream: kh_substitute_vars()
         * splices a ${var} value straight into the raw template text
         * BEFORE parsing. pdl-read's manager publishes a doc page into
         * `content="${page_text}"`, and that page text contains a bare
         * `"` (e.g. a Markdown  "(CORRECT)"  quote). parse_attr_value()
         * stops at that inner `"`, the rest of the page body spills out
         * as raw inter-element text, and the first `>` in it (a Markdown
         * `> ` blockquote) closes the mangled tag - leaving prose where
         * a child element should be. Short pages happened not to contain
         * a `>` after the break, which is why it looked intermittent.
         *
         * Guaranteeing forward progress here makes the parser robust to
         * ANY malformed / not-well-formed input instead of spinning:
         * a byte parse_element() can't consume is skipped. (The deeper
         * correctness fix - escaping ${var} values spliced into quoted
         * attributes so the content isn't truncated at the first `"` -
         * is a separate change.) */
        const char *before = p;
        p = parse_element(p, e);
        if (p == before) { g_parse_skipped_bytes++; p++; }
    }
}

/* ------------------------------------------------------------------ *
 * ${var} substitution - restores the tpmos layout/data separation
 * (see 08-roadmap/design-docs/CHTPM-ARCHITECTURE-FIX.md). A .chtpm is
 * a STATIC template; a manager writes plain key=value lines to a state
 * file; at parse time every ${key} token in the template is replaced
 * with that key's value. Ported from 101.ledger-player-npc-simple+3/
 * system/chtpm_parser.c (substitute_vars/load_vars/get_var).
 *
 * Backward compatible: if a template contains no "${" at all, none of
 * this runs and parsing is byte-for-byte as before. The state file is
 * named by a `vars="<path>"` attribute on any tag (resolved against
 * g_house_root, or absolute if it starts with '/'); a missing file
 * makes every ${key} resolve to the empty string, matching tpmos
 * get_var()'s default. Only khtpm_core_render.+x consumes this - the
 * ~25 legacy chtpm_parser_pal.c apps keep their own substituter. */
#define KH_MAX_VARS   2048  /* was 256 - a <repeat> grid emits 2+ vars/row; a
                             * 256-tile rmmv tileset alone is 512, and silent
                             * truncation in kh_set_var() made every count var
                             * that landed after the overflow resolve to 0 */
#define KH_VAR_NAME   64
#define KH_VAR_VALUE  2048
typedef struct { char name[KH_VAR_NAME]; char value[KH_VAR_VALUE]; } KhVar;
static KhVar g_kh_vars[KH_MAX_VARS];
static int g_kh_nvars = 0;
static char g_vars_path[PATH_BUF] = "";
/* REAL, NEW 2026-09-03 - ONE generic optional-argv hook (not a per-app
 * mode). If argv[3] is an existing directory, it is an "instance dir":
 *   - ${ARG3} resolves to it (kh_get_var builtin, like ${PID}),
 *   - <instance>/.hq_manager/ui.txt is appended to whatever vars= the
 *     template declares, so a per-entity projection (events-hq, and
 *     later db-hq Common Events) can be multi-instance without a
 *     house-global state file,
 *   - every <module> fork gets KHTPM_ARG3=<instance dir> in its env.
 * The pre-existing "argc>=5 -> g_win_x=atoi(argv[3])" popup path is
 * guarded to only run when argv[3] is NOT a directory. */
static char g_arg3_dir[PATH_BUF] = "";
/* REAL FIX 2026-09-13, direct live report ("run the harness or is it
 * stale?" - it was NOT stale, it found a real, current regression):
 * events-hq's own real launch shape is <house> <chtpm> <pkg_dir>
 * <entity_label> (argv[3]=pkg_dir dir, argv[4]=entity_label - see
 * §5d.11's comment on the argv[3]/argv[4] reinterpretation just below
 * in main()). g_arg3_dir already captures argv[3] for the generic
 * instance-dir hook, but argv[4] (the entity_label khtpm_events_hq_
 * manager.c's own main() requires - "usage: <house_root> <pkg_dir>
 * <entity_label>", argc<4 is a hard exit) was never captured anywhere
 * - kh_launch_window_modules()'s own module-launch call passed neither
 * this nor g_arg3_dir (it passed g_package_dir, dirname of the .chtpm
 * itself, e.g. ".../events-hq/pieces" - wrong directory entirely) to
 * launch_module(), so the manager it self-spawns via <module> always
 * exited immediately on a fresh launch: confirmed live via
 * run_visible_window_events_hq_demo.sh (fixed to be safe to run
 * against a live desktop, see that harness's own PROC_PATTERN comment)
 * - "khtpm_events_hq_manager: usage: <house_root> <pkg_dir>
 * <entity_label>" in the launch log, zero PNG produced. The long-lived
 * asa/ava/greet_player managers already running elsewhere on this
 * house predate whatever refactor broke this (this file's own <module>
 * self-launch consolidation, per launch_module()'s header comment,
 * folded events-hq onto the same generic path db-hq/chat-hai use -
 * neither of which need a THIRD manager argv the way events-hq does) -
 * a fresh events-hq launch has been silently broken since, unnoticed
 * because nothing relaunched it until this investigation.
 *
 * (g_arg4_entity_label itself is declared earlier in this file, right
 * before kh_collect_and_launch_modules() - its first real use - since
 * that function is defined long before this section.) */
static char g_extra_vars_path[PATH_BUF] = "";
/* vars-file change is content-hashed (g_vars_hash), not mtime-tracked */
/* hash of the vars-file bytes at the last reparse - so a projector that
 * rewrites state/ui.txt every tick (rather than only on change, the
 * tpmos frame_changed.txt convention) does NOT force a reparse/redraw
 * when the content is byte-identical. Cheap: the state file is small. */
static unsigned long g_vars_hash = 0;
/* Provisional hash awaiting confirmation - see reparse_chtpm_if_changed().
 * The prisc .pal projectors rewrite their state file IN PLACE (sfopen
 * truncates, then many swrites over several ms), so a poll can catch it
 * half-written. Requiring the same new hash on two consecutive polls
 * before reparsing rejects that transient (tpmos ONE WRITER RULE: never
 * act on a half-written frame). 0 = nothing pending. */
static unsigned long g_vars_hash_pending = 0;
static unsigned long kh_file_hash(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    unsigned long h = 1469598103934665603UL; /* FNV-1a 64 */
    int c;
    while ((c = fgetc(f)) != EOF) { h ^= (unsigned char)c; h *= 1099511628211UL; }
    fclose(f);
    return h ? h : 1;
}

static const char *kh_get_var(const char *name) {
    /* built-ins so a static template can name house-relative paths in
     * an action= without the manager having to bake in an absolute
     * path: ${HOUSE} = house_root, ${PKG} = this .chtpm's own dir
     * (what the renderer passes as $1 to every action anyway). */
    if (strcmp(name, "HOUSE") == 0) return g_house_root;
    if (strcmp(name, "PKG") == 0)   return g_package_dir;
    /* ${PID} = this renderer process's own pid - the id the frame dump
     * (entity_menu_frame_<pid>.txt) and the agent history injector
     * (entity_menu_history/<pid>.txt) are keyed by. Lets a static
     * template surface it (e.g. the strip's cell after the clock)
     * without any manager involvement, since the manager is a
     * different process. */
    if (strcmp(name, "PID") == 0) {
        static char kh_pidbuf[16];
        snprintf(kh_pidbuf, sizeof(kh_pidbuf), "%d", (int)getpid());
        return kh_pidbuf;
    }
    if (strcmp(name, "ARG3") == 0) return g_arg3_dir;
    for (int i = 0; i < g_kh_nvars; i++)
        if (strcmp(g_kh_vars[i].name, name) == 0) return g_kh_vars[i].value;
    return "";
}

static void kh_set_var(const char *name, const char *value) {
    for (int i = 0; i < g_kh_nvars; i++) {
        if (strcmp(g_kh_vars[i].name, name) == 0) {
            snprintf(g_kh_vars[i].value, KH_VAR_VALUE, "%s", value);
            return;
        }
    }
    if (g_kh_nvars >= KH_MAX_VARS) return;
    snprintf(g_kh_vars[g_kh_nvars].name, KH_VAR_NAME, "%s", name);
    snprintf(g_kh_vars[g_kh_nvars].value, KH_VAR_VALUE, "%s", value);
    g_kh_nvars++;
}

/* key=value lines; '#' comments and blank lines skipped; key and
 * surrounding space trimmed, interior spaces of the value kept. */
static void kh_load_vars(const char *path) {
    if (!path || !path[0]) return;
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[KH_VAR_VALUE + KH_VAR_NAME + 8];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        char *s = line;
        while (*s == ' ' || *s == '\t') s++;
        if (*s == '\0' || *s == '#') continue;
        char *eq = strchr(s, '=');
        if (!eq) continue;
        *eq = '\0';
        char *ke = eq;
        while (ke > s && (ke[-1] == ' ' || ke[-1] == '\t')) ke--;
        *ke = '\0';
        char *val = eq + 1;
        while (*val == ' ' || *val == '\t') val++;
        if (s[0]) kh_set_var(s, val);
    }
    fclose(f);
}

/* load one OR MANY space-separated state files (vars="a.txt b.txt c.txt")
 * - one <module> per file, later files add/override. Clears the table
 * once, then appends each. */
static void kh_load_vars_multi(const char *paths) {
    g_kh_nvars = 0;
    if (!paths || !paths[0]) return;
    char work[PATH_BUF * 4];
    snprintf(work, sizeof(work), "%s", paths);
    char *save = NULL;
    for (char *tok = strtok_r(work, " \t", &save); tok; tok = strtok_r(NULL, " \t", &save))
        kh_load_vars(tok);
}

/* combined FNV-1a hash of every space-separated file in `paths` - a
 * content change in ANY of them changes the result. */
static unsigned long kh_files_hash(const char *paths) {
    if (!paths || !paths[0]) return 0;
    char work[PATH_BUF * 4];
    snprintf(work, sizeof(work), "%s", paths);
    unsigned long h = 1469598103934665603UL;
    char *save = NULL;
    for (char *tok = strtok_r(work, " \t", &save); tok; tok = strtok_r(NULL, " \t", &save)) {
        unsigned long fh = kh_file_hash(tok);
        h ^= fh; h *= 1099511628211UL;
    }
    return h ? h : 1;
}

/* the `vars="..."` attribute, with EACH space-separated token resolved
 * against g_package_dir (or house_root), space-joined into out. */
static int kh_find_vars_attr(const char *buf, char *out, size_t outsz) {
    const char *p = strstr(buf, "vars=");
    while (p) {
        if (p == buf || isspace((unsigned char)p[-1]) || p[-1] == '<') {
            const char *q = p + 5;
            if (*q == '"') {
                q++;
                char rel[PATH_BUF]; size_t n = 0;
                while (*q && *q != '"' && n + 1 < sizeof(rel)) rel[n++] = *q++;
                rel[n] = '\0';
                /* rel may be one path OR several space-separated (one
                 * <module> per file). Resolve EACH token: absolute
                 * wins; relative resolves against g_package_dir (a
                 * per-instance copy under a custom --data-root finds
                 * its own state), else house_root. Space-join into out. */
                const char *base = g_package_dir[0] ? g_package_dir
                                 : (g_house_root[0] ? g_house_root : ".");
                out[0] = '\0';
                char work[PATH_BUF]; snprintf(work, sizeof(work), "%s", rel);
                char *save = NULL; int first = 1;
                for (char *tok = strtok_r(work, " \t", &save); tok;
                     tok = strtok_r(NULL, " \t", &save)) {
                    char one[PATH_BUF];
                    if (tok[0] == '/') snprintf(one, sizeof(one), "%s", tok);
                    else if (tok[0] == '#' && g_house_root[0])
                        snprintf(one, sizeof(one), "%s/%s", g_house_root, tok);
                    else snprintf(one, sizeof(one), "%s/%s", base, tok);
                    size_t cur = strlen(out);
                    snprintf(out + cur, outsz - cur, "%s%s", first ? "" : " ", one);
                    first = 0;
                }
                return out[0] ? 1 : 0;
            }
        }
        p = strstr(p + 5, "vars=");
    }
    return 0;
}

/* ${name} -> value; \$ \{ \\ pass the next char literally; a "\n"
 * sequence inside a value becomes a real newline (tpmos convention).
 * An unknown ${name} expands to nothing.
 *
 * EXPERIMENT 2026-09-05 (branch experiment/xhtpm-attr-var-escaping,
 * follow-up to fix ea384825): substitution splices the value straight
 * into the raw template text that parse_element() then re-parses. If
 * the splice lands inside a double-quoted attribute value (`content=
 * "${page_text}"`, `label="${x}"`, ...) and the value contains a
 * literal `"` (or `<` `>` `&`), it breaks parse_attr_value() and the
 * rest of the value corrupts the tag stream (the infinite loop
 * ea384825 now merely survives, at the cost of truncating the value
 * at the first `"`). Fix: track whether `o` is currently inside a
 * template-level `"..."` and, when so, emit each char of a ${var}
 * value XML-escaped - `decode_entities()` on the apply_attr() side
 * (already run for label=/action=/...; content= gets it here too)
 * restores the real text. Template-literal `"` chars still toggle the
 * state; a ${var} value's own emitted `"` does not. */
static void kh_sv_emit(char **o, char *end, char c, int esc) {
    if (!esc) { if (*o < end) *(*o)++ = c; return; }
    const char *rep = NULL;
    switch (c) {
        case '&': rep = "&amp;"; break;
        case '"': rep = "&quot;"; break;
        case '<': rep = "&lt;"; break;
        case '>': rep = "&gt;"; break;
        default:  if (*o < end) *(*o)++ = c; return;
    }
    while (*rep && *o < end) *(*o)++ = *rep++;
}

static void kh_substitute_vars(const char *src, char *dst, size_t max_len) {
    const char *p = src;
    char *o = dst;
    char *end = dst + max_len - 1;
    int in_attr_quote = 0; /* o is inside a template-level "..." */
    while (*p && o < end) {
        if (strncmp(p, "<!--", 4) == 0) {
            const char *e = strstr(p, "-->");
            size_t span = e ? (size_t)(e + 3 - p) : strlen(p);
            for (size_t i = 0; i < span && o < end; i++) *o++ = p[i];
            p += span;
            continue;
        }
        if (*p == '\\' && (p[1] == '$' || p[1] == '{' || p[1] == '\\')) {
            *o++ = p[1]; p += 2; continue;
        }
        if (p[0] == '$' && p[1] == '{') {
            const char *close = strchr(p, '}');
            if (close) {
                char name[KH_VAR_NAME];
                size_t n = (size_t)(close - (p + 2));
                if (n >= sizeof(name)) n = sizeof(name) - 1;
                memcpy(name, p + 2, n); name[n] = '\0';
                const char *v = kh_get_var(name);
                while (*v && o < end) {
                    if (v[0] == '\\' && v[1] == 'n') { kh_sv_emit(&o, end, '\n', 0); v += 2; }
                    else kh_sv_emit(&o, end, *v++, in_attr_quote);
                }
                p = close + 1;
                continue;
            }
        }
        if (*p == '"') in_attr_quote = !in_attr_quote;
        *o++ = *p++;
    }
    *o = '\0';
}

/* <repeat count="${n}" bind="row"> ... ${row.field} ... </repeat>
 * (CHTPM-ARCHITECTURE-FIX.md 3.3, the dynamic-list half). Runs BEFORE
 * kh_substitute_vars: `count` is resolved now (a bare int, or one
 * ${var}); the body is emitted `count` times with ${row.field}
 * rewritten to ${row_<i>_field} and ${row.#} to the literal index, so
 * the value pass then fills row_0_field / row_1_field / ... from the
 * state file. No nesting in v1 (first </repeat> closes). Missing/empty
 * count => zero copies, so an empty list just vanishes. */
#define KH_REPEAT_MAX 4096
static size_t kh_emit_repeat_body(const char *body, size_t blen,
                                  const char *bind, int idx,
                                  char *o, char *oend) {
    char *o0 = o;
    size_t bl = strlen(bind);
    for (size_t i = 0; i < blen && o < oend; ) {
        if (body[i] == '$' && i + 1 < blen && body[i + 1] == '{' &&
            i + 2 + bl < blen && strncmp(body + i + 2, bind, bl) == 0 &&
            body[i + 2 + bl] == '.') {
            size_t j = i + 3 + bl;
            char field[64]; size_t fn = 0;
            while (j < blen && body[j] != '}' && fn + 1 < sizeof(field) &&
                   (isalnum((unsigned char)body[j]) || body[j] == '_' || body[j] == '#'))
                field[fn++] = body[j++];
            field[fn] = '\0';
            if (j < blen && body[j] == '}') {
                j++;
                int n;
                if (strcmp(field, "#") == 0)
                    n = snprintf(o, (size_t)(oend - o), "%d", idx);
                else if (strcmp(field, "##") == 0)
                    /* REAL, NEW 2026-09-05 - 1-based index, for
                     * human-facing counters (line numbers, "Page N")
                     * where ${bind.#}'s 0-based value reads wrong. */
                    n = snprintf(o, (size_t)(oend - o), "%d", idx + 1);
                else
                    n = snprintf(o, (size_t)(oend - o), "${%s_%d_%s}", bind, idx, field);
                if (n > 0) o += (n < (int)(oend - o) ? n : (int)(oend - o));
                i = j;
                continue;
            }
        }
        *o++ = body[i++];
    }
    return (size_t)(o - o0);
}

static void kh_expand_repeats(const char *src, char *dst, size_t cap) {
    const char *p = src;
    char *o = dst;
    char *oend = dst + cap - 1;
    while (*p && o < oend) {
        if (strncmp(p, "<!--", 4) == 0) {
            const char *e = strstr(p, "-->");
            size_t span = e ? (size_t)(e + 3 - p) : strlen(p);
            for (size_t i = 0; i < span && o < oend; i++) *o++ = p[i];
            p += span;
            continue;
        }
        if (strncmp(p, "<repeat", 7) == 0 &&
            (isspace((unsigned char)p[7]) || p[7] == '>')) {
            const char *gt = strchr(p, '>');
            /* depth-matched close: skip past any NESTED <repeat>...
             * </repeat> so the outer body is captured whole. The inner
             * ones are expanded on the next pass of the outer loop
             * around this function (kh_expand_repeats_all). */
            const char *close = NULL;
            if (gt) {
                int depth = 1;
                const char *q = gt + 1;
                while (*q) {
                    if (strncmp(q, "<repeat", 7) == 0 &&
                        (isspace((unsigned char)q[7]) || q[7] == '>')) { depth++; q += 7; continue; }
                    if (strncmp(q, "</repeat>", 9) == 0) {
                        depth--;
                        if (depth == 0) { close = q; break; }
                        q += 9; continue;
                    }
                    q++;
                }
            }
            if (gt && close) {
                /* attrs live in [p+7, gt) */
                char attrs[512]; size_t an = (size_t)(gt - (p + 7));
                if (an >= sizeof(attrs)) an = sizeof(attrs) - 1;
                memcpy(attrs, p + 7, an); attrs[an] = '\0';
                char bind[64] = "item";
                int count = 0;
                const char *bp = strstr(attrs, "bind=\"");
                if (bp) { bp += 6; size_t k = 0;
                    while (bp[k] && bp[k] != '"' && k + 1 < sizeof(bind)) { bind[k] = bp[k]; k++; }
                    bind[k] = '\0'; }
                const char *cp = strstr(attrs, "count=\"");
                if (cp) {
                    cp += 7;
                    char cv[64]; size_t k = 0;
                    while (cp[k] && cp[k] != '"' && k + 1 < sizeof(cv)) { cv[k] = cp[k]; k++; }
                    cv[k] = '\0';
                    if (cv[0] == '$' && cv[1] == '{') {
                        char nm[64]; size_t m = 0;
                        for (const char *q = cv + 2; *q && *q != '}' && m + 1 < sizeof(nm); q++) nm[m++] = *q;
                        nm[m] = '\0';
                        count = atoi(kh_get_var(nm));
                    } else {
                        count = atoi(cv);
                    }
                }
                if (count < 0) count = 0;
                if (count > KH_REPEAT_MAX) count = KH_REPEAT_MAX;
                const char *body = gt + 1;
                size_t blen = (size_t)(close - body);
                for (int i = 0; i < count && o < oend; i++)
                    o += kh_emit_repeat_body(body, blen, bind, i, o, oend);
                p = close + 9; /* strlen("</repeat>") */
                continue;
            }
        }
        *o++ = *p++;
    }
    *o = '\0';
}

/* expand nested <repeat> (pages -> commands, ...): run kh_expand_repeats
 * repeatedly - each pass expands the outermost layer, exposing the
 * inner <repeat>s (whose count="${x.n}" is now a concrete
 * ${x_0_n} the var pass will read). Bounded at 5 levels. Ping-pongs
 * between two buffers. Returns the buffer holding the final result
 * (either `a` or `b`), NUL-terminated. */
static char *kh_expand_repeats_all(char *a, char *b, size_t cap) {
    char *src = a, *dst = b;
    for (int pass = 0; pass < 5; pass++) {
        if (!strstr(src, "<repeat")) return src;
        kh_expand_repeats(src, dst, cap);
        char *t = src; src = dst; dst = t;
    }
    return src;
}

static Elem *parse_chtpm(const char *path) {
    long skipped_before = g_parse_skipped_bytes;
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    buf[rd] = '\0';
    fclose(f);

    /* static-template pipeline (CHTPM-ARCHITECTURE-FIX.md): load the
     * state file, expand <repeat> blocks, then substitute ${var}. All
     * skipped when the template uses neither ${...} nor <repeat> - a
     * plain markup .chtpm is parsed byte-for-byte as before. */
    if (strstr(buf, "${") || strstr(buf, "<repeat")) {
        char vpath[PATH_BUF] = "";
        if (kh_find_vars_attr(buf, vpath, sizeof(vpath))) {
            snprintf(g_vars_path, sizeof(g_vars_path), "%s", vpath);
        }
        /* append the instance-dir ui.txt (argv[3] hook) as an extra
         * space-separated vars source - kh_load_vars_multi and
         * kh_files_hash both split g_vars_path on whitespace. */
        if (g_extra_vars_path[0]) {
            size_t l = strlen(g_vars_path);
            snprintf(g_vars_path + l, sizeof(g_vars_path) - l,
                     "%s%s", l ? " " : "", g_extra_vars_path);
        }
        if (g_vars_path[0]) g_vars_hash = kh_files_hash(g_vars_path);
        kh_load_vars_multi(g_vars_path);

        if (strstr(buf, "<repeat")) {
            /* Big enough for a full 256-row tile grid whose <repeat> body
             * carries long ${…} paths (rmmv/emoji): each expanded row is
             * ~250B pre-sub, ~600B post-sub. */
            size_t rcap = (size_t)sz * 48 + (size_t)512 * 1024;
            char *a = malloc(rcap), *b = malloc(rcap);
            if (a && b) {
                snprintf(a, rcap, "%s", buf);
                char *fin = kh_expand_repeats_all(a, b, rcap);
                free(buf);
                buf = strdup(fin);
                free(a); free(b);
                if (!buf) return NULL;
            } else { free(a); free(b); }
        }

        size_t cap = strlen(buf) * 4 + 65536;
        char *subbed = malloc(cap);
        if (subbed) {
            kh_substitute_vars(buf, subbed, cap);
            free(buf);
            buf = subbed;
        }
    }

    /* paranoia cap: ~4x the post-substitution length + a floor. A
     * well-formed parse stays under 1x; this only trips on a genuine
     * runaway (a future no-progress bug somewhere in parse_element). */
    g_parse_budget = (long)strlen(buf) * 4 + 100000;
    g_parse_cap_hit = 0;

    const char *p = buf;
    Elem *root = NULL;
    while (*p) {
        if (--g_parse_budget <= 0) { g_parse_cap_hit = 1; break; }
        skip_ws(&p);
        if (!*p) break;
        if (*p == '<' && p[1] == '!') { p = parse_element(p, NULL); continue; }
        if (*p != '<') break;
        if (!root) {
            root = elem_new("__root");
            const char *after = parse_element(p, root);
            p = after;
        } else {
            p = parse_element(p, root);
        }
    }
    free(buf);
    g_parse_budget = 0;   /* parse over - disable the step guard */
    long this_skipped = g_parse_skipped_bytes - skipped_before;
    if (this_skipped > 0 || g_parse_cap_hit) {
        /* Non-silent, non-fatal: a not-well-formed template rendered
         * with garbled bytes dropped. Almost always a bare " / < / > /
         * & in a ${var} value that landed outside a quoted attribute
         * (kh_substitute_vars escapes the in-attribute case). The
         * window still opens; this line is the breadcrumb. */
        fprintf(stderr,
            "khtpm parse_chtpm(%s): NOT WELL-FORMED - skipped %ld stray byte(s)%s "
            "(a bare \" / < / > / & in a ${var} value between tags?). "
            "Window still rendered, content may be garbled.\n",
            path, this_skipped, g_parse_cap_hit ? " AND HIT THE ITERATION CAP" : "");
    }
    /* Sticky visible marker - only for THIS window's own template (not
     * the dock-peer strip or an unrelated reparse); cleared by a later
     * clean reparse of the same file. */
    if (!g_chtpm_path[0] || (path && strcmp(path, g_chtpm_path) == 0)) {
        g_last_parse_skipped = this_skipped;
        g_window_malformed = (this_skipped > 0 || g_parse_cap_hit);
    }
    if (root && root->n_children > 0) return root->children[0];
    return root;
}

/* ---------- page navigation (real, matches objects.pdl's own GOTO:/
 * BACK semantics exactly - not reinvented) ---------- */
static Elem *g_window;
static char g_page_stack[MAX_PAGE_STACK][32];
static int g_page_stack_n = 0;
static char g_current_page[32] = "main";

static Elem *find_page(const char *name) {
    for (int i = 0; i < g_window->n_children; i++) {
        Elem *c = g_window->children[i];
        if (strcmp(c->tag, "page") == 0 && strcmp(c->id, name) == 0) return c;
    }
    return NULL;
}

/* REAL, generic capability #1 (2026-08-31, xperiments/khtpm-generic-
 * dispatch-design.md §5 - see its own header comment for the direct
 * instruction this answers): re-reads g_chtpm_path from disk whenever
 * its mtime changes, replacing g_window wholesale. Lets a real manager
 * keep a live-updating, generic .chtpm as its own real projection (the
 * SAME real "manager owns projection, renderer just re-parses/renders
 * it" philosophy fo-menu-sys.md already documents for the chtpm_
 * parser.c/ASCII family - this is the khtpm/X11 side finally getting
 * the equivalent capability) - without this renderer needing ANY
 * project-specific C code to show that manager's real, changing state.
 * g_n_elems is reset to 0 first - the ENTIRE tree is rebuilt fresh,
 * same real "checkpoint and rewind" discipline chat-hai's own
 * chai_n_elems_static already uses, just for the whole tree instead of
 * a sub-list. Real, deliberate scope: does NOT re-detect g_is_X mode
 * flags - a window's real MODE never changes mid-session, only its
 * CONTENT does; callers gate this off entirely for the 3 modes that
 * manage their own cached Elem pointers (db-hq/events-hq/chat-hai -
 * see this function's own call site). */
/* Forward declaration - the real definition (with its own full header
 * comment) lives further down this file, right after g_focus_nav/
 * g_n_nav/g_nav[] - needed here because this function must NULL it out
 * on every reparse (elem_new()'s shared g_pool[] is reused in place, so
 * a stale armed-field pointer from the old tree is not just wrong, it
 * aliases whatever the new parse happens to write at that pool slot). */
static Elem *g_default_input_elem;
/* REAL, NEW 2026-09-14, direct live report ("cli-io can switch if
 * cli-io is active or not active. get it? that can be checked by
 * pdl... could that be handled thru layout/manager?") - g_default_
 * input_elem (armed/not-armed) is real, current state that only ever
 * existed inside this process; a manager has no way to see it, so it
 * can't use it as a real switch to decide what layout/menu content to
 * publish. Publishes a real, tiny state file, same convention as the
 * existing .hq_manager/ui.txt vars publish (g_extra_vars_path) -
 * `active=1`/`active=0`, written only on a real ARM<->UNARM transition
 * (not every redundant re-arm of the same already-armed field), so a
 * manager can poll it and use it exactly like any other switch this
 * house's event/condition system already checks. Routed through ONE
 * real setter (below) rather than patched into each of the several
 * real g_default_input_elem = ... assignment sites by hand - single
 * source of truth, no drift risk if a future call site gets added and
 * someone forgets the publish. */
static void kh_publish_cli_io_active(int active) {
    if (!g_package_dir[0]) return;
    char dir[PATH_BUF], path[PATH_BUF];
    snprintf(dir, sizeof(dir), "%s/.hq_manager", g_package_dir);
    mkdir(dir, 0777);
    snprintf(path, sizeof(path), "%s/cli_io_active.txt", dir);
    FILE *f = fopen(path, "w");
    if (f) { fprintf(f, "active=%d\n", active); fclose(f); }
}
static void kh_set_default_input_elem(Elem *e) {
    int was_active = (g_default_input_elem != NULL);
    int now_active = (e != NULL);
    g_default_input_elem = e;
    if (was_active != now_active) kh_publish_cli_io_active(now_active);
}
/* REAL, NEW 2026-09-05 (TEXT_AREA-SCROLL-GUTTER-SELECTION-DESIGN.md) -
 * Shift-held state for the CURRENT key being dispatched. Set from
 * ev->xkey.state at the physical KeyPress site and per-code in
 * dispatch_relay_code() (relay codes 220-225 = shifted arrows/Home/
 * End). default_cli_io_handle_key() reads it: Shift+move extends the
 * text selection, an unshifted move collapses it. */
static int g_key_shift = 0;
/* REAL, NEW 2026-09-14, direct live report ("clicking text-edit in tb
 * didn't bring it to top in always on top off") - the real X server
 * timestamp of the most recent real input event this process has seen
 * (ButtonPress/KeyPress both carry a real `.time`), updated once at the
 * top of hq_dispatch_xevent(). kh_raise_and_focus() used to pass
 * CurrentTime to its _NET_ACTIVE_WINDOW request and XSetInputFocus -
 * the EWMH spec explicitly calls CurrentTime unreliable there (a real
 * WM uses this timestamp to arbitrate focus-stealing prevention against
 * its own last-seen user activity time; Mutter can silently ignore/
 * deprioritize a CurrentTime request for a WM-managed window instead of
 * actually raising it - matches the report exactly: it worked while
 * override_redirect bypassed the WM entirely, broke once windows became
 * WM-managed). 0 (CurrentTime) until the first real event arrives. */
static Time g_last_event_time = 0;
static int g_key_ctrl = 0;
/* PDL-configurable window-close combo. ESC never closes a real app
 * window (accident risk - direct instruction). This is the deliberate
 * close gesture, from #.desktop/hq_ui.pdl:  close_combo=ctrl+q
 * (the user can set ctrl+w, ctrl+shift+w, ...).
 *
 * REAL FIX 2026-09-14, direct live report ("i tried selecting it
 * didn't work" -> "is it competing with ctrl+c 2 kill?" -> "just
 * change ctrl+c to kill to off for now. we will change kill to
 * ctrl+q"): this default WAS ctrl+c, and handle_key()'s close-combo
 * check below runs completely unconditionally, before any armed-field
 * check - so a real Ctrl+C pressed to copy selected text out of an
 * armed cli_io/text_area closed the WHOLE WINDOW first, every time,
 * before the copy could ever register. Confirmed live: this is what
 * silently killed a test window mid-copy-test this same session.
 * Real fix for now: move the default off Ctrl+C entirely (ctrl+q) -
 * a real fix that lets Ctrl+C mean copy while a field is armed,
 * without touching close_combo, is real, separate follow-up work, not
 * done here. */
static char g_close_combo[24] = "ctrl+q";
/* Top of the desktop work area - a new HQ window spawns here, fullscreen
 * starts here, and window drag can't go above it (clears the GNOME
 * panel + livedesk top strip). #.desktop/hq_ui.pdl: win_top_y=90 -
 * bump it if the board opens too high / into the top taskbar. */
static int g_win_top_y = 90;   /* == WM_MANAGED_DRAG_MIN_Y (defined below) */
/* REAL FIX 2026-09-05, direct live report ("tb and x11-hq windows no
 * longer do double digit accumulation jump, ie 15 jumps to 5") -
 * multi-digit nav-jump accumulator for the generic default-mode
 * digit-jump (handle_key()'s own `ch >= '1'` branch), ported from
 * tpmos chtpm_parser.c.bak's `digit_accum` (~line 1996): a pressed
 * digit is `accum*10 + d`; take it if 1..g_n_nav, else restart the
 * accumulator with just `d`. Reset to 0 on any arrow-nav, Enter, or
 * any non-digit key (that last one is done once, at the top of the
 * post-armed-field section of handle_key). Popup/entity-menu mode
 * already has its own `popup_digit_accum`; the taskbar's own
 * digit-jump is in khtpm_strip_parser.c / khtpm_taskbar_manager.c -
 * a separate fix if it regressed there too. */
static int g_nav_digit_accum = 0;
/* REAL, NEW 2026-09-03 (direct request: "make menu dropdown... work for
 * all layouts", after live-checking that piececraft-hq's own File/Desk
 * dropdown is hand-built, mode-specific C predating CENTROID_GOLD_STD,
 * and the real ACTIVATE/DEACTIVATE scope mechanism is currently wired
 * only into db-hq's own dispatch) - the SAME real concept (one active
 * "open" trigger at a time), generic to default mode instead of
 * db-hq-only, so any default-mode app (network-browser, open-hai,
 * chat-hai, co-lab-hai) gets a real dropdown for free just by using
 * onclick="ACTIVATE" + class="dropdown-child" target_id="<trigger id>"
 * in its own generated .chtpm - zero new C per app. Same dangling-
 * pointer risk as g_default_input_elem above (elem_new()'s pool reuse
 * on reparse) - reset alongside it in reparse_chtpm_if_changed(). */
static Elem *g_default_active_scope_root;
static char g_default_active_scope_id[64];
/* 1 when the active scope was entered on a CONTAINER (an ACTIVATE
 * trigger with target_id=) - then nav is confined to that subtree,
 * interact-mode style. 0 for a plain dropdown ACTIVATE (children
 * show/hide only, nav not confined) so every existing dropdown is
 * unchanged. */
static int g_default_scope_confine = 0;
/* generic <tabbar>/<tab> support (2026-09-03) - the id of the tab
 * currently marked active, so its highlight and the "you came from
 * here" Esc target survive the projector's every-tick reparse. Ported
 * in spirit from db-hq's g_dbhq_current_tab. */
static char g_default_active_tab_id[64] = "";
static int g_dock_drop_lo, g_dock_drop_hi;
/* REAL, NEW 2026-09-22 (task 1 - "pals" dock dropdown scroll) - the
 * scroll cursor for the dock/taskbar dropdown popup (layout_dock_bar()'s
 * dropdown-child stacking pass + dock_paint_menu()). Owned exactly like
 * every other scroll cursor in this file per the house standards skill's
 * own rule ("scroll cursor owned by generic_sbar_register() + the
 * generic Page_Up/Page_Down handler, no new key-handling code") - this
 * is just the *int storage cell* generic_sbar_register() is given a
 * pointer to; SCROLLUP:/SCROLLDOWN: onclick dispatch (already generic,
 * see dispatch()'s "SCROLLUP:"/"SCROLLDOWN:" branch) mutates it via that
 * pointer, same as every sidebar/scrolllist's own scroll int does.
 * g_dock_dropdown_scroll_owner resets it to 0 whenever a DIFFERENT
 * trigger's dropdown opens, so switching from "pals" to "hq" doesn't
 * keep a stale scroll offset. */
static int g_dock_dropdown_scroll = 0;
static char g_dock_dropdown_scroll_owner[64] = "";
/* Forward declaration - real definition (with its own X11/Xft section
 * header comment) lives further down this file. Needed here because a
 * reparse that disarms a cli_io field mid-type must also release any
 * real XGrabKeyboard that field's own arm took (see activate_focused()/
 * default_cli_io_handle_key()'s own real grab-keyboard fix) - leaving
 * an exclusive keyboard grab held after silently disarming would lock
 * ALL keyboard input house-wide to this one (now non-typing) window
 * until it closes, a real, much worse bug than the one being fixed. */
static Display *dpy;
static int g_headless;  /* fwd (real def near g_dump_and_exit) - referenced by the guards below */
/* Every XUngrabKeyboard(dpy,...) in this file goes through here so a
 * --headless run (dpy == NULL) can't segfault Xlib on a NULL Display,
 * and so the call is a clean no-op before any display is open. */
/* >0: an armed field's XGrabKeyboard lost a race to another client (a dock's
 * stale display-wide grab, a closing popup) and keeps retrying from
 * hq_idle_tick() until this CLOCK_MONOTONIC ms deadline - the old 5x5ms burst
 * gave up before anything could release it, leaving csv-hq's grid armed but
 * deaf (kh_focus_debug.log: "GRAB key=cell_ attempts=6 rc=1"). */
static long long g_kbd_grab_retry_deadline_ms;
static long long kh_mono_ms(void) {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
static void kh_ungrab_kbd(void) { g_kbd_grab_retry_deadline_ms = 0; if (dpy) XUngrabKeyboard(dpy, CurrentTime); }
/* 2026-09-11, CHTPM-INCREMENTAL-REPARSE-DESIGN.md §1 - parses `path`
 * into the SCRATCH pool (g_pool_next) instead of the live g_pool,
 * leaving g_window/every existing live pointer completely untouched.
 * Works by briefly retargeting elem_new()'s own bump allocator
 * (g_elem_pool_target/g_elem_n_target) around this one parse_chtpm()
 * call, then restoring it - every OTHER elem_new() call site in the
 * house (the existing full-rebuild path included) is unaffected, since
 * those globals default to &g_pool/&g_n_elems and nothing else ever
 * retargets them. Returns the scratch tree's root (still non-NULL only
 * as long as it's used before the NEXT call to this function or the
 * next full rebuild - it lives in g_pool_next, which reparse_chtpm_
 * if_changed() resets to empty right after every diff attempt). */
static Elem *kh_parse_into_scratch(const char *path) {
    g_n_elems_next = 0;
    Elem *saved_pool = g_elem_pool_target;
    int *saved_n = g_elem_n_target;
    g_elem_pool_target = g_pool_next;
    g_elem_n_target = &g_n_elems_next;
    Elem *result = parse_chtpm(path);
    g_elem_pool_target = saved_pool;
    g_elem_n_target = saved_n;
    return result;
}
/* Forward declaration - real definition (with its own header comment)
 * lives in the generic sidebar+panel scroll section further down.
 * Needed here so a reparse (new manager content) can auto-scroll the
 * message list to the newest content, same real "always show the
 * latest" convention any chat UI needs - see this function's own body
 * below for where it's used. */
static int g_default_scrolllist_scroll;
static int window_is_dock(void); /* real def + header comment further down; forward-declared for the dock heartbeat below */
static int reparse_chtpm_if_changed(void) {
    if (!g_chtpm_path[0]) return 0;
    struct stat st;
    if (stat(g_chtpm_path, &st) != 0) return 0;
    /* REAL FIX 2026-09-13, direct live report ("all options in tb and
     * bottom bar disappear... top reappeared, but not bottom for a
     * while... there was no change so why did this happen?... even if
     * it is idle or slow cpu, it shouldn't do that"): hoisted out of the
     * block below (was scoped there, invisible to the two real re-parse
     * sites further down) so both of them can gate on it - see those
     * sites' own updated comments for the actual fix this enables. */
    int peer_changed = 0;
    {
        if (g_dock_peer_path[0]) {
            struct stat pst;
            if (stat(g_dock_peer_path, &pst) == 0 &&
                (pst.st_mtim.tv_sec != g_dock_peer_mtime.tv_sec ||
                 pst.st_mtim.tv_nsec != g_dock_peer_mtime.tv_nsec))
                peer_changed = 1;
        }
        /* ${var} state file (CHTPM-ARCHITECTURE-FIX.md): the template
         * .chtpm is now static, so its mtime never moves - a data
         * change shows up as a new mtime on g_vars_path instead. Treat
         * that exactly like a template change: re-parse (which re-runs
         * kh_load_vars + kh_substitute_vars) + re-layout + re-draw.
         *
         * REAL FIX 2026-09-13, direct live report: "bottom tb is missing
         * entities again... no matter what this cant happen" - the
         * FOURTH live occurrence of this exact symptom class this week
         * (bug_bounty.md), each time a DIFFERENT specific mechanism (a
         * dead pid, a double-registration kill, an ENOENT race, and this
         * time: a content-hash that was live-confirmed STABLE for 5+
         * full seconds with the dock still blank - a fourth, distinct
         * gap in this exact mtime/hash/debounce chain). Direct follow-up
         * instruction, after an earlier pass here tried a periodic
         * forced-reparse timer as a band-aid over that same chain: "we
         * dont use mtime or hash for render, ideally we use marker
         * filesize change only, or stay with last render... do u see
         * this instruction in golden rules?" - CENTROID_GOLD_STD.md
         * rule 8 ("Repaint discipline - marker/dirty model... not on
         * mtime, not on a hash, not per input event"), yes, and this
         * exact file already has the real, proven, ALREADY-WIRED
         * instance of it one function away: the manager's own
         * publish_state() (khtpm_taskbar_manager_main.c) writes
         * strip_ui.txt/strip_state.txt THEN appends one byte to
         * #.desktop/strip_frame_changed.txt (touch_frame_changed()) -
         * dock_poll_strip_state() below already watches that marker's
         * SIZE growth, never mtime, for its own (narrower) focus-sync
         * job. The real, final fix: for a dock window, THIS reparse
         * gate watches the exact same marker for content changes too,
         * replacing the hash/debounce chain entirely - no timer, no
         * hash, no possible instability window; growth is the one and
         * only real signal, exactly as rule 8 states. Non-dock windows
         * (every other mode) keep the existing hash/debounce path
         * unchanged - they have no such marker to watch (most have no
         * single "the" writer process the way the strip's manager is
         * one), so the earlier mechanism stays the correct one there. */
        int vars_changed = 0;
        if (window_is_dock()) {
            static long s_dock_vars_marker = -1;
            char mp[PATH_BUF];
            snprintf(mp, sizeof(mp), "%s/#.desktop/strip_frame_changed.txt", g_house_root);
            struct stat mst;
            if (stat(mp, &mst) == 0) {
                if (s_dock_vars_marker < 0)              s_dock_vars_marker = mst.st_size;      /* first sight */
                else if (mst.st_size < s_dock_vars_marker) s_dock_vars_marker = mst.st_size;      /* truncated/rotated - resync */
                else if (mst.st_size > s_dock_vars_marker) { s_dock_vars_marker = mst.st_size; vars_changed = 1; }
            }
            /* REAL FIX 2026-09-14, direct live report ("sword and castle
             * aren't on bottom toolbar... they were there for last 15
             * minutes. then vanished... same bug we had before, vanishing
             * bottom tb entities"): live-checked at report time - the
             * real data was already fully correct (both processes alive,
             * both registered in livedesk_open.txt, strip_ui.txt already
             * published n_tabs=2 with both labels) - this was NEVER a
             * process-death or registry-prune bug, purely a frozen render:
             * the marker-growth gate just above only forces a reparse
             * when something NEW changes; if a reparse is ever missed or
             * silently lost (a transient read hiccup, a race with the
             * manager's own rewrite, anything) there was no second chance
             * to self-correct - the dock peer just sits on whatever was
             * last (wrongly) drawn, indefinitely, even once the
             * underlying data is fully correct again. The house's own
             * previous fix for this SYMPTOM CLASS (ktb_self_heal_active_
             * desk_registry(), khtpm_taskbar_manager.c) was a periodic
             * ~10s timer that re-decided PROCESS LIFECYCLE (spawn/kill)
             * against fast-moving state, and caused three real "entities
             * die/flicker/vanish" incidents itself before being disabled
             * entirely (see ktb_reload()'s own 2026-09-14 header comment)
             * - that shape is exactly what NOT to repeat here. This is
             * deliberately narrower and cannot touch process lifecycle at
             * all: a bounded-interval FORCE of the exact same reparse
             * path a real vars change already takes, reading the exact
             * same on-disk ground truth (strip_ui.txt/livedesk_open.txt
             * via the manager's own publish) - if the data is already
             * right, this is a no-op repaint; if a prior reparse was
             * ever missed, this closes the gap within one interval
             * instead of leaving it wrong until the next real change
             * happens to land. 20s chosen to comfortably beat "sat idle
             * 15 minutes" while staying cheap (one parse_chtpm() of a
             * small template, not a process operation). */
            {
                static struct timespec s_dock_force_last = {0, 0};
                struct timespec now_ts;
                clock_gettime(CLOCK_MONOTONIC, &now_ts);
                if (s_dock_force_last.tv_sec == 0) {
                    s_dock_force_last = now_ts;
                } else if (now_ts.tv_sec - s_dock_force_last.tv_sec >= 20) {
                    s_dock_force_last = now_ts;
                    vars_changed = 1;
                }
                /* REAL FIX 2026-09-15, direct live report ("i do see a
                 * flicker on bottom tb every once in a while... never
                 * on top bar... once every 5-10 min"). Root cause: the
                 * 300s blind destroy+rebuild that used to live here
                 * (7th bounty occurrence's own "safe, KISS" answer,
                 * 2026-09-14) unconditionally tore down and rebuilt
                 * the peer's real Pixmap/GC/XftDraw/Window on a plain
                 * elapsed-time timer, whether or not anything was
                 * actually wrong - exactly a blind timer, the opposite
                 * of this house's own DIAMOND/marker-driven-render
                 * standard, and exactly why it only ever hit the
                 * bottom bar (the only window with this destructive
                 * timer at all) and never the header or any entity
                 * (neither has one). That 7th occurrence's own root
                 * cause was explicitly "not pinned down with
                 * certainty" at the time - a speculative safety net,
                 * not a real fix. Since then, this same session found
                 * and fixed several concrete, confirmed root causes of
                 * real dock staleness (the incremental-reparse element-
                 * pool leak, the manager's stale KTB_STRIP_N_CELLS
                 * focus round-trip, the pager focus-echo bug) - real
                 * condition-based fixes, not timers. The remaining,
                 * real, condition-based self-heal for "the window
                 * itself is genuinely gone" is
                 * kh_ensure_dock_peer_window()'s own per-tick
                 * XGetWindowAttributes liveness check (already live,
                 * already proven, the 6th occurrence's own fix) - a
                 * real state check, not a blind timer, exactly the
                 * DIAMOND spirit: react to what's ACTUALLY wrong, not
                 * to elapsed time. Removed the blind rebuild rather
                 * than replace it with a marker file, since a marker
                 * can only prove "a reparse ran," not "the paint
                 * surface itself is corrupt" - the one failure mode
                 * this timer was guessing at - and no live evidence
                 * this session (or since) has shown that failure mode
                 * recurring. If it ever does resurface, root-cause it
                 * for real with the kh_focus_debug_log probe technique
                 * this session proved out repeatedly, not another
                 * blind timer. */
            }
        } else if (g_vars_path[0]) {
            /* content-hash ALL the state files (one per <module>) - a
             * reparse fires only on a real byte change in any of them,
             * not on a projector's identical every-tick rewrite
             * (marker-driven-render spirit). Cheap: small files. */
            unsigned long h = kh_files_hash(g_vars_path);
            if (h != g_vars_hash) {
                /* Debounce a non-atomic in-place projector rewrite (see
                 * g_vars_hash_pending): only treat the change as real once
                 * the SAME new hash shows up on two consecutive polls. A
                 * partial/empty mid-write read hashes to a one-off value
                 * that never repeats, so it is ignored instead of causing
                 * a blank<->populated reparse flip every projector tick. */
                if (h == g_vars_hash_pending) {
                    g_vars_hash = h;
                    g_vars_hash_pending = 0;
                    vars_changed = 1;
                } else {
                    g_vars_hash_pending = h;
                }
            } else {
                g_vars_hash_pending = 0;
            }
        }
        /* REAL FIX 2026-09-14, direct live report ("navs wont go past 2
         * as if 2 are on tb, but its the old tb items(7) not the 2") -
         * root cause: peer_changed (just above) only fires on the
         * bottom dock peer's OWN template file mtime, which per this
         * function's 2026-09-13 comment a few lines up is deliberately
         * treated as static and never touched again after boot. That
         * assumption is wrong for khtpm_strip_bottom.xhtpm specifically
         * - its whole content is a <repeat count="${n_tabs}"> driven by
         * the SAME per-tick vars data the header's own vars_changed
         * check already watches (n_tabs/tab_N_* - real, live proof: a
         * desk switch changed n_tabs from 7 to 2 in strip_ui.txt and the
         * marker grew, so the header side re-rendered its own nav count
         * correctly, but the peer's tab list never got a fresh
         * parse_chtpm() at all, so it kept showing whatever 7 stale
         * <tab> elements were parsed at boot - nav numbering (driven by
         * live vars) and the visibly rendered tree (driven by the stale
         * peer parse) silently diverged from that point on. Fix: a real
         * vars change is exactly as real a reason to refresh the peer as
         * a template edit - fold it in here rather than re-deriving a
         * second, parallel "did the tab list change" signal. */
        if (vars_changed) peer_changed = 1;
        if (st.st_mtim.tv_sec == g_chtpm_mtime.tv_sec && st.st_mtim.tv_nsec == g_chtpm_mtime.tv_nsec && !peer_changed && !vars_changed)
            return 0;
    }
    g_chtpm_mtime = st.st_mtim;
    /* 2026-09-11 - CHTPM-INCREMENTAL-REPARSE-DESIGN.md. Tried FIRST,
     * before any of the existing full-rebuild logic below runs - on
     * success this returns immediately, the full-rebuild code never
     * executes. OFF by default (g_use_incremental_reparse); requires a
     * real existing g_window to diff against (the very first parse of
     * a window's life has nothing to compare to - falls through to the
     * existing path below, same as always). On any failure (parse
     * failure, diff failure/allocator exhaustion, root tag changed)
     * this deliberately does NOT return - it falls through to the
     * unmodified existing code below, which does its own fresh
     * g_n_elems=0/parse_chtpm() rebuild regardless of whatever partial
     * state the failed diff attempt may have left g_window in (kh_
     * reparse_diff_patch()'s own documented contract: on failure the
     * old tree's state is undefined, the caller must discard it
     * entirely - which is exactly what falling through to the existing
     * rebuild does, for free, no special-casing needed). */
    if (g_use_incremental_reparse && g_window) {
        Elem *candidate = kh_parse_into_scratch(g_chtpm_path);
        if (candidate) {
            KhDiffAllocator alloc = { kh_pool_alloc, kh_pool_free, NULL };
            Elem *removed[MAX_ELEMS];
            KhDiffRemovedList rl = { removed, MAX_ELEMS, 0 };
            int ok = kh_reparse_diff_patch(g_window, candidate, &alloc, &rl);
            g_n_elems_next = 0; /* scratch pool always discarded here, success or failure - candidate's own Elems are never adopted into the live tree */
            if (ok) {
                /* The diff preserved identity for everything else - the
                 * ONLY place a cross-reparse pointer can still go stale
                 * is a genuine removal, reported here. This replaces
                 * EVERY per-consumer find-by-key bolt-on (kh_text_areas_
                 * reload/kh_cli_io_reload/kh_find_input_by_key) for any
                 * window running this path - none of those run below. */
                for (int i = 0; i < rl.n; i++) {
                    if (removed[i] == g_default_input_elem) {
                        kh_ungrab_kbd();
                        kh_set_default_input_elem(NULL);
                        kh_focus_debug_log("INCREMENTAL_REPARSE armed field genuinely removed - disarmed");
                    }
                    if (removed[i] == g_default_active_scope_root) {
                        g_default_active_scope_root = NULL;
                        g_default_scope_confine = 0;
                    }
                }
                if (g_dock_peer_path[0] && peer_changed) {
                    /* REAL FIX 2026-09-13, direct live report ("bottom bar
                     * disappears... there was no change so why did this
                     * happen?"): this used to re-parse the peer template
                     * from disk unconditionally on EVERY successful header
                     * reparse (i.e. on every single vars_changed tick -
                     * basically any tab/pal state update, which can be
                     * frequent) even though the peer is a STATIC template
                     * that (per this file's own long-standing comment
                     * elsewhere) never changes again after boot. That's
                     * real, repeated, unnecessary disk I/O + a fresh
                     * parse_chtpm() call for a file that essentially never
                     * changes - and every one of those was a fresh, brand
                     * new chance to hit the same class of transient read
                     * hiccup (ENOENT-style race, slow/contended disk under
                     * this house's own documented weak-CPU machine) this
                     * exact symptom has already been chased and patched
                     * four separate times this week (see this file's
                     * "FOURTH live occurrence" comment above). peer_changed
                     * (computed once, above, from the peer's own real
                     * mtime) was already sitting right there the whole
                     * time - simplification, not another patch: only
                     * touch the peer file when it ACTUALLY changed. Keeps
                     * the fresh-parse-only-adopts-on-success guard just
                     * below unchanged (still real, still needed for the
                     * rare case the template genuinely is edited live). */
                    Elem *np = parse_chtpm(g_dock_peer_path);
                    if (np) g_dock_peer = np;
                    { struct stat pst; if (g_dock_peer && stat(g_dock_peer_path, &pst) == 0) g_dock_peer_mtime = pst.st_mtim; }
                }
                kh_focus_debug_log("INCREMENTAL_REPARSE ok removed=%d", rl.n);
                return 1;
            }
            kh_focus_debug_log("INCREMENTAL_REPARSE diff failed rc=%d - falling back to full rebuild this tick", ok);
        } else {
            kh_focus_debug_log("INCREMENTAL_REPARSE candidate parse failed - falling back to full rebuild this tick");
        }
    }
    /* REAL FIX 2026-08-31 (found live testing open-hai's own projection
     * with a real, live-typing manager behind it: clicks/Enter appeared
     * to silently stop arming a cli_io field for no visible reason) -
     * elem_new()'s own g_pool[MAX_ELEMS] never frees, it's reused IN
     * PLACE from index 0 on every reparse (see this file's own g_n_elems
     * reset just below) - a g_default_input_elem left pointing into the
     * OLD tree becomes a dangling/aliased pointer into WHATEVER the new
     * parse happens to write at that same pool slot the instant this
     * function rebuilds. A real .chtpm this house's own generic
     * capability #1 is FOR (a manager regenerating live content) can
     * reparse mid-arm at any moment - this isn't a rare edge case for
     * that real use, it's the normal case. Same real "drop transient
     * UI state tied to the old tree" reasoning this function already
     * applies to g_current_page/g_page_stack_n just below, extended to
     * the one other piece of state that can reference the old tree.
     * REAL, NEW 2026-08-31 - also release any real XGrabKeyboard that
     * field's own arm took (activate_focused()'s own real grab-keyboard
     * fix) - see this function's own forward-declaration comment for
     * `dpy` above for why leaving it held would be a much worse bug
     * than the one this whole block fixes. A harmless no-op when
     * nothing was actually armed/grabbed.
     *
     * REAL FIX 2026-09-11 (direct instruction: "in text-edit-hq its
     * working perfect. can we make [browser, h-ai cli_io] just work
     * like text-edit?" - text-edit-hq's <text_area> already got a real
     * reparse-survival fix on 2026-09-08 (kh_text_areas_reload, right
     * below) that <cli_io> never got - this is that same parity applied
     * to cli_io, nothing else bundled in this time. Root cause this
     * closes: this block unconditionally disarmed+ungrabbed on EVERY
     * reparse; a manager that reparses often while a cli_io is armed
     * (open-hai streaming a response, network-browser's ~300ms tick)
     * loses the field constantly, while text-edit-hq's manager barely
     * reparses after load so its text_area rarely hit this at all - not
     * because text_area is special, just because it's rarely exercised.
     * The X keyboard grab is a Window-level resource, not tied to our
     * Elem* - if the SAME field (by saved key) still exists after
     * reparse, the grab already held for it is still valid; nothing to
     * release. Capture which field was armed, by key, before the
     * pointer is cleared - restored (without retaking the grab) once
     * the new tree exists, a few lines down. */
    char saved_input_key[128] = "";
    /* REAL FIX 2026-09-14, direct live report ("i tried selecting it
     * didn't work"... "theres some finnicky focus issues with the
     * text edit hq space") - the restore code a few lines down used to
     * unconditionally collapse the selection (cursor -> end of buffer,
     * sel_anchor = cursor) every single time a reparse landed on an
     * already-armed field, whether or not the buffer actually changed.
     * text-edit-hq's own debug log shows it re-arming (activate_
     * focused()) far more often than a single click-in explains ("the
     * finnicky focus issues") - each of THOSE re-arms also collapses
     * selection (separate, real mechanism, activate_focused() itself)
     * - but every ordinary reparse tick was ALSO wiping any live
     * Shift+Arrow selection the user had built up, even when nothing
     * about the field's own content changed. Capture the pre-reparse
     * buffer content here too, so the restore below can tell "content
     * genuinely changed under us" (cursor-to-end is still correct)
     * apart from "unrelated reparse, this field's own text is
     * identical" (the real selection should survive). */
    char saved_input_buf[4096] = "";
    int saved_cursor = 0, saved_sel_anchor = 0;
    /* A <grid> armed across a full rebuild (csv-hq's manager republishes
     * csv_hq_ui.txt on every SETCELL/status change) must come back exactly as
     * it was: same cell cursor, same jump buffer, same nested cell-edit state.
     * Before this, kh_find_input_by_key() never matched a <grid>, so every
     * full reparse silently disarmed it and released the keyboard grab. */
    int saved_is_grid = 0, saved_grid_row = 0, saved_grid_col = 0, saved_grid_mode = 0;
    char saved_grid_jump[sizeof(((Elem *)0)->grid_jump_buffer)] = "";
    char saved_grid_cell[sizeof(((Elem *)0)->grid_cell_buffer)] = "";
    if (g_default_input_elem && strcmp(g_default_input_elem->tag, "grid") == 0) {
        saved_is_grid = 1;
        saved_grid_row = g_default_input_elem->grid_cur_row;
        saved_grid_col = g_default_input_elem->grid_cur_col;
        saved_grid_mode = g_default_input_elem->grid_edit_mode;
        snprintf(saved_grid_jump, sizeof(saved_grid_jump), "%s", g_default_input_elem->grid_jump_buffer);
        snprintf(saved_grid_cell, sizeof(saved_grid_cell), "%s", g_default_input_elem->grid_cell_buffer);
    }
    if (g_default_input_elem) {
        const char *k = g_default_input_elem->target_id[0] ? g_default_input_elem->target_id : g_default_input_elem->id;
        snprintf(saved_input_key, sizeof(saved_input_key), "%s", k);
        const char *ob = strcmp(g_default_input_elem->tag, "text_area") == 0 ?
            g_default_input_elem->text_area_buffer : g_default_input_elem->input_buffer;
        snprintf(saved_input_buf, sizeof(saved_input_buf), "%s", ob);
        saved_cursor = g_default_input_elem->cursor;
        saved_sel_anchor = g_default_input_elem->sel_anchor;
    }
    kh_set_default_input_elem(NULL);
    /* Same real dangling-pointer reasoning as g_default_input_elem just
     * above - a stale dropdown-open pointer into a freed/reused pool
     * slot is a real, live crash risk, not a cosmetic one. */
    g_default_active_scope_root = NULL;
    g_default_scope_confine = 0;
    g_n_elems = 0;
    Elem *new_window = parse_chtpm(g_chtpm_path);
    if (!new_window) return 0;
    g_window = new_window;
    /* REAL, NEW 2026-09-08 (sql-hq) - a <text_area> auto-saves its
     * buffer to <pkg>/text_area_<id>.txt (default_text_area_save), but
     * the fresh parse above resets every buffer to its content="" attr.
     * For an app whose projector republishes state/ui.txt on Run
     * (sql-hq: results grid changes -> reparse), that would wipe the
     * user's in-progress SQL every time they hit Run. Re-hydrate any
     * text_area from its saved file here, exactly the way the scope
     * re-resolve just below restores nav state across the same reparse.
     * No-op for any text_area whose save file doesn't exist yet. */
    kh_text_areas_reload(new_window);
    /* cli_io's own half of the same fix, real parity with text_area
     * above - see kh_cli_io_reload()'s own header comment. Restore
     * buffer content for every cli_io, then - if a field was actually
     * armed when this reparse started - re-find that SAME field in the
     * new tree by its saved key and re-arm it: cursor to the end of
     * the restored buffer, no stale selection. The grab is NOT retaken
     * here - it was never released above, so it's still held. Only the
     * else branch touches the grab, for the one real case that needs
     * it: the field genuinely disappeared from the new tree. */
    kh_cli_io_reload(new_window);
    if (saved_input_key[0]) {
        Elem *reelem = kh_find_input_by_key(new_window, saved_input_key);
        if (reelem && saved_is_grid && strcmp(reelem->tag, "grid") == 0) {
            kh_set_default_input_elem(reelem);
            reelem->grid_cur_row = saved_grid_row;
            reelem->grid_cur_col = saved_grid_col;
            reelem->grid_edit_mode = saved_grid_mode;
            snprintf(reelem->grid_jump_buffer, sizeof(reelem->grid_jump_buffer), "%s", saved_grid_jump);
            snprintf(reelem->grid_cell_buffer, sizeof(reelem->grid_cell_buffer), "%s", saved_grid_cell);
            reelem->cursor = saved_cursor;
            kh_focus_debug_log("REPARSE key=%s FOUND grid row=%d col=%d edit=%d", saved_input_key, saved_grid_row, saved_grid_col, saved_grid_mode);
        } else if (reelem) {
            kh_set_default_input_elem(reelem);
            char *rbuf = strcmp(reelem->tag, "text_area") == 0 ? reelem->text_area_buffer : reelem->input_buffer;
            int rlen = (int)strlen(rbuf);
            /* REAL FIX 2026-09-14 - see saved_input_buf's own header
             * comment: only force cursor-to-end/collapse-selection when
             * the field's own content genuinely changed under this
             * reparse (a real save/load/regen did happen - cursor
             * position from before may not even be valid). If it's
             * byte-identical, this was an unrelated reparse tick - keep
             * whatever selection the user had built up, clamped only
             * for safety against a length that can't have grown past
             * what it already was. */
            if (strcmp(rbuf, saved_input_buf) == 0) {
                reelem->cursor = saved_cursor > rlen ? rlen : (saved_cursor < 0 ? 0 : saved_cursor);
                reelem->sel_anchor = saved_sel_anchor > rlen ? rlen : (saved_sel_anchor < 0 ? 0 : saved_sel_anchor);
            } else {
                reelem->cursor = rlen;
                reelem->sel_anchor = reelem->cursor;
            }
            kh_focus_debug_log("REPARSE key=%s FOUND buf_len=%d content_same=%d", saved_input_key, rlen, strcmp(rbuf, saved_input_buf) == 0);
        } else {
            kh_ungrab_kbd(); /* field really is gone - the grab held for it is now meaningless */
            kh_focus_debug_log("REPARSE key=%s NOT_FOUND - ungrabbed", saved_input_key);
        }
    }
    if (g_dock_peer_path[0] && peer_changed) {
        /* REAL FIX 2026-09-13 - see the incremental-reparse branch's own
         * identical comment above (peer_changed gate, real reason, real
         * simplification); this is just the full-rebuild fallback's copy
         * of the same fix. */
        Elem *np = parse_chtpm(g_dock_peer_path);
        if (np) g_dock_peer = np;
        { struct stat pst; if (g_dock_peer && stat(g_dock_peer_path, &pst) == 0) g_dock_peer_mtime = pst.st_mtim; }
    }
    if (g_default_active_scope_id[0]) {
        /* re-resolve the scope across the reparse the projector's
         * every-tick state write triggers - by the trigger's id, then
         * (interact-style container scope) to its target_id container,
         * restoring the confine flag so nav stays confined.
         *
         * REAL FIX 2026-09-13 (direct live report: "tb has an issue
         * now, its stuck on 1.hq no matter what is pressed") -
         * g_default_scope_confine used to be left AS-IS here (only
         * ever set to 1 in the two branches below, never reset to 0)
         * - a real, latent bug, not something this session's other
         * fixes touched: a header cell like strip-cell-1 ("HQ") has a
         * bare onclick="ACTIVATE" with NO target_id, and isn't a
         * <tab>, so neither branch below matches for it - but if
         * g_default_scope_confine was already 1 from ANY earlier real
         * scoped interaction in this same process's lifetime (a
         * <tab>, or a target_id'd ACTIVATE elsewhere), it stayed 1
         * forever after, on every single reparse (this dock's own
         * projector rewrites its state every ~400ms tick, so this
         * runs constantly) - permanently confining nav to just the
         * trigger itself (kh_elem_in_scope() has no other match for
         * it), matching the exact reported symptom. activate_focused()
         * itself already gets this right (unconditional reset to 0
         * before conditionally setting 1, see its own ACTIVATE branch
         * above) - this re-resolve path must do the same. */
        Elem *trig = find_by_id(g_window, g_default_active_scope_id);
        g_default_active_scope_root = trig;
        g_default_scope_confine = 0;
        if (trig && trig->target_id[0]) {
            Elem *c = find_by_id(g_window, trig->target_id);
            if (c) { g_default_active_scope_root = c; g_default_scope_confine = 1; }
        } else if (trig && strcmp(trig->tag, "tab") == 0) {
            /* a <tab> scope always locks onto the page <sidebar> */
            Elem *pg = find_page("main");
            Elem *sb = pg ? find_by_tag(pg, "sidebar") : NULL;
            if (sb) { g_default_active_scope_root = sb; g_default_scope_confine = 1; }
        }
    }
    g_current_page[0] = '\0';
    snprintf(g_current_page, sizeof(g_current_page), "main");
    g_page_stack_n = 0;
    /* REAL, NEW 2026-08-31 - a real, generic <scrolllist> (see the
     * dual-region sidebar+panel section further down) auto-follows new
     * content by default, same real "always show the newest message"
     * convention any chat UI needs - a huge sentinel here gets clamped
     * to the real max_scroll the very next layout_scroll_region() call
     * makes, so this doesn't need to know the real row count itself.
     * Deliberately does NOT touch g_default_sidebar_scroll - a session
     * list has no "newest is at the bottom" convention to auto-follow. */
    /* REAL FIX 2026-09-02 - auto-follow-bottom is the chat convention
     * (open-hai newest message). A document-shaped consumer (network
     * browser, any page that should start at the top) marks its panel
     * <scrolllist class="from-top"/>. Data-driven, no per-app flag:
     * missing class keeps the old bottom-follow so chat is unchanged. */
    g_default_scrolllist_scroll = 1000000;
    {
        Elem *panel = find_by_tag(new_window, "panel");
        if (panel) {
            int i, j;
            for (i = 0; i < panel->n_children; i++) {
                Elem *c = panel->children[i];
                if (strcmp(c->tag, "scrolllist") != 0) continue;
                for (j = 0; j < c->n_classes; j++) {
                    if (strcmp(c->classes[j], "from-top") == 0)
                        g_default_scrolllist_scroll = 0;
                }
            }
        }
    }
    return 1;
}

/* ---------- X11/Xft ---------- */
/* dpy itself is forward-declared earlier, right after g_default_input_elem
 * (see that comment for why) - defining it again here would conflict. */
static Window win;
static int screen;
/* Always-on-top ("@") toggle, renderer half. The process-management half (scan
 * /proc, SIGTERM, re-exec entity windows so their create-time-only
 * override_redirect picks up the new mode, raise/lower tile:* entities) moved out
 * 2026-09-20 to the standalone op ktb_zorder_op.+x (ktb_zorder_op.c) - see
 * 08-roadmap/design-docs/DOCK-UNFACTOR-AUDIT.md sections 1/4/6. What stays here is
 * only what needs THIS process's X connection and window ids: raising/lowering the
 * strip's own dock windows, and spawning the op detached (fork+setsid, stdio to
 * /dev/null, no waiting - the popen-freeze lesson in prisc-x-popen-custom-op-freeze). */
static void kh_zorder_raise_dock(int raise) {
    if (!dpy) return;
    if (raise) {
        XRaiseWindow(dpy, win);
        if (g_dock_peer_win) XRaiseWindow(dpy, g_dock_peer_win);
        if (g_dock_menu_win) XRaiseWindow(dpy, g_dock_menu_win);
    } else {
        XLowerWindow(dpy, win);
        if (g_dock_peer_win) XLowerWindow(dpy, g_dock_peer_win);
        if (g_dock_menu_win) XLowerWindow(dpy, g_dock_menu_win);
    }
    XFlush(dpy);
}
static void kh_spawn_zorder_op(int above) {
    char bin[PATH_BUF];
    pid_t p;
    snprintf(bin, sizeof(bin), "%s/*.monads/*.livedesk-taskbar/ops/+x/ktb_zorder_op.+x", g_house_root);
    if (access(bin, X_OK) != 0) {
        kh_focus_debug_log("ZORDER_TOGGLE: %s missing - build_core_render.sh builds it; entities not respawned", bin);
        return;
    }
    p = fork();
    if (p < 0) return;
    if (p == 0) {
        pid_t g = fork();
        int devnull;
        if (g != 0) _exit(0);
        setsid();
        devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            dup2(devnull, 0); dup2(devnull, 1); dup2(devnull, 2);
            if (devnull > 2) close(devnull);
        }
        execl(bin, bin, g_house_root, above ? "above" : "normal", (char *)NULL);
        _exit(127);
    }
    (void)waitpid(p, NULL, 0);
}
static GC gc;
static Pixmap buf;
/* Real fix 2026-08-28 (live crash: BadMatch on X_GetImage) - buf/win
 * used to be created ONCE at their initial g_window->w/h and never
 * resized, but content can genuinely grow taller AFTER window creation
 * (g_pal_forced_h, set by dbhq_inject_palette_tiles() once real rows
 * like the rmmv tab bar / tileset chooser exist) - redraw()'s own
 * XGetImage always requested the CURRENT (grown) g_window->h against
 * the ORIGINAL (smaller) Pixmap, which X rejects outright. These track
 * the Pixmap's actual real allocated size so redraw() can detect
 * "content grew past what's backing it" and recreate buf (+ the real
 * X11 window itself, via XResizeWindow) to match, instead of assuming
 * a window's size is fixed for its whole lifetime like every OTHER
 * khtpm/-hq window in this file does (chat-hai/events-hq's own escape
 * from this bug is simply never growing post-creation - palettes is
 * the first mode where the content height is genuinely dynamic). */
static int g_buf_w = 0, g_buf_h = 0;
static XftDraw *xftdraw_buf;
static Colormap cmap;
static XftFont *font_ui;
static int g_win_x = 300, g_win_y = 300;
static int g_win_w = 260, g_win_h = 200;
/* Last on-screen position we actually asked the server for via XMoveWindow.
 * The redraw() corrective-move block (2026-09-03) must only touch the server
 * when our INTENDED position (g_win_x/g_win_y) has genuinely changed since we
 * last applied it - comparing against raw XGetWindowAttributes wa.x/wa.y is a
 * coordinate-space bug (a reparenting WM reports frame-relative coords, so the
 * "mismatch" is almost always spuriously true), which fired an XMoveWindow +
 * XSync + XSetInputFocus storm on every idle redraw = the flicker regression.
 * INT_MIN = "never applied yet". */
static int g_win_pos_applied_x = INT_MIN, g_win_pos_applied_y = INT_MIN;
/* Last "^"/"." focus-owned state actually painted into the title bar, so a
 * genuine but repeated FocusIn/FocusOut (mode NotifyNormal) only repaints
 * when the indicator would truly change. -1 = nothing painted yet. */
static int g_focus_owned_painted = -1;
/* Set at window-create for a <window class="managed"> non-dock window
 * (pchq-board). hq_idle_tick() re-asserts XSetInputFocus while the
 * pointer is over the window (legacy run_pchq_board_mode pchq_focus_ok).
 * Not set for override_redirect windows (2026-09-03 flicker). */
static int g_win_managed_focus = 0;
static int g_has_canvas = 0;  /* set by assign_nav_and_layout when a <canvas> is present -> event loop ticks ~30fps for a live feed */
/* REAL, NEW 2026-09-04 (direct instruction: "we should still use
 * 'interact' mode... it is the same in old chtpm-parser for tpmos...
 * should try to stick as close to that as possible") - generic port of
 * tpmos chtpm_parser_pal.c's own onClick="INTERACT"/active_index dual-
 * mode key routing: while a real, externally-owned process (piececraft-
 * hq's board-viewer, first real consumer) has genuinely engaged, EVERY
 * keypress here is raw game input, not this window's own nav - same
 * real shape run_pchq_board_mode() hand-rolled per-app, now generic.
 * Armed/disarmed by a template <item relay="path1,path2"> carrying
 * class="interact-active" - a LIVE, projector-published state (not a
 * local click guess), scanned once per layout pass. While armed, keys
 * are appended verbatim to relay path(s) as bare-decimal (matches
 * house convention: pieces/apps/player_app/history.txt) and "KEY_
 * PRESSED: N" (pieces/keyboard/history.txt) - the SAME two-file dual
 * write pchq_append_key() always did, just no longer app-specific C. */
static int g_interact_relay_on = 0;
static char g_interact_relay_paths[2][PATH_BUF];
static int g_interact_relay_n = 0;
static char g_interact_relay_raw[300] = "";  /* last-armed relay= string, for the re-arm check only */
/* PC-HQ-FOCUS-AND-INTERACT-ACTIVATE.md: X11 focus vs Interact arm.
 * handle_key forwards only when both are 1. Init 1 so first In works
 * before the first FocusIn. */
static int g_x11_window_focused = 1;
static int g_interact_disengage_sent = 0;
static int g_quit = 0;
/* --dump-and-exit (any argv position): paint one frame, write the PNG +
 * .txt receipt via dump_frame_png(), then quit. Set once at startup,
 * honoured at the top of hq_run_event_loop() so every mode is covered
 * from one place. g_dumped guards against a mode that also calls
 * dump_frame_png() itself before entering the loop. */
static int g_dump_and_exit = 0;
static int g_dumped = 0;
/* --headless (any argv position): run with NO X connection at all -
 * parse, ${var}-sub, the real assign_nav_and_layout() (text metrics
 * estimated, screen size a constant), then loop writing the per-PID
 * ascii frame + consuming the per-PID input relay. For agents / CI /
 * driving many IRC+chain windows from different ports with no display
 * and no Xvfb. See TERMINAL-MIRROR-PARITY-all-windows.md step 4. */
static int g_headless = 0;
/* REAL FIX 2026-08-16, direct live report ("it breaks on events or just
 * when right clicking sometimes" - intermittent): the stale-event drain
 * right after XMapRaised only discards events already sitting in the X
 * server's queue AT THAT INSTANT - it does not cover a trailing event
 * from the initiating right-click that the server hasn't delivered yet
 * (real race, not fully closed by the drain alone). Add a short
 * time-based debounce on top: ignore ButtonPress entirely until this
 * many ms after the window mapped, closing the race regardless of
 * exact event-arrival timing. */
static struct timespec g_map_time;
#define PHANTOM_CLICK_GUARD_MS 150
static int g_focus_nav = 1;
static int g_n_nav = 0;
static Elem *g_nav[MAX_ELEMS];

/* Right-click hit-test shared by the real ButtonPress path and the relayed
 * `MOUSE_EVENT: 3 x y 1` path, so a human and an agent open the same menu. */
static Elem *kh_ctx_hit(int x, int y) {
    for (int i = 0; i < g_n_nav; i++) {
        Elem *it = g_nav[i];
        if (!it || it->w <= 0) continue;
        if (strncmp(it->id, "chrome-", 7) == 0) continue;
        if (x >= it->x && x < it->x + it->w && y >= it->y && y < it->y + it->h) return it;
    }
    return NULL;
}
/* REAL, NEW 2026-09-14 (generic `<bar>` click-to-seek) - the raw
 * window-local click X a human hit a <bar> with, relative to the bar's
 * own x so the fraction is (click_x - e->x)/e->w. Set by popup_handle_
 * click()'s own bar hit-test immediately before activate_focused() runs
 * the bar's onclick (that generic dispatcher sees g_focus_nav, not the
 * pointer, so the coordinate is carried here instead). -1 = no pending
 * bar click, which activate_focused()'s bar branch uses to run the SAME
 * command the other generic dispatch paths use (Enter on a focused bar
 * seeks to the current playhead, e.g. half when no bar click is pending). */
static int g_bar_click_x = -1;
/* REAL, NEW 2026-08-31 - moved up here (from its own real definition
 * site right before activate_focused(), further down this file) so
 * khtpm_draw_core.c's own #include below can see it: a cli_io element
 * currently ARMED (accepting real keystrokes) needs its own distinct
 * visual from a merely-focused-but-not-yet-armed one, matching the
 * reference 1.TPMOS_c_+rmmp.0103.0001 chtpm_parser.c family's own
 * real "^" (armed) vs ">" (focused only) cursor convention - direct
 * instruction. See draw_elem()'s own real cli_io branch.
 * (Moved even further up, above reparse_chtpm_if_changed(), 2026-08-31 -
 * that function needs to NULL this out on every reparse - see its own
 * comment.) */
/* REAL, NEW 2026-08-29 (direct instruction: "i dont want u to just do
 * button as soon as its clicked, first nav should move and wait for
 * second click") - replaces the old "auto" default (single click both
 * focuses AND activates in one step) everywhere a real nav-numbered
 * element is clicked. Shared, not duplicated per mode - every mode's
 * own handle_click() calls this instead of inlining the check.
 * Returns 1 when the caller should go ahead and activate `hit` (it was
 * ALREADY the focused element - this is a real second click on the
 * same target); returns 0 when this click's only real effect was
 * moving focus onto `hit` for the first time, and the caller must
 * stop there without activating - the caller is responsible for a
 * redraw so the moved focus ring is visible immediately. Elems with no
 * real nav_index (e.g. a scrollbar drag track/arrow - a repeat
 * control, not a menu selection) keep the old immediate-activate
 * behavior; this only changes real nav-numbered targets. */
/* Curated font-family picker list (Settings' own "Font -/+ " cycle) -
 * real fontconfig names, real TrueType fonts already covered by this
 * house's own font stack (fontconfig substitutes a close match for any
 * of these not literally installed, same as any other Xft app - never
 * a hard failure, matches XftFontOpenName's own fallback behavior
 * already relied on elsewhere in this file). DSEG7/DSEG14 Classic
 * added 2026-09-10 (direct request: "is there a digital style font you
 * know? thats what i want to use") - real, installed (`fonts-dseg`
 * apt package, added this session), the actual open-source 7-/14-
 * segment LCD-style typeface family, not an approximation. DSEG7 is
 * the classic digital-clock look but only really covers digits +
 * a handful of symbols (most letters render as a rough segment
 * approximation, not their real shape) - DSEG14 covers the full
 * alphabet properly (14-segment displays can spell real words) at the
 * same digital aesthetic, so it's the safer pick if DSEG7 turns out
 * hard to read in nav labels/buttons. Both in the cycle either way. */
static const char *g_font_family_choices[] = {
    "DejaVu Sans", "Times New Roman", "Comic Sans MS", "Helvetica",
    "Ubuntu", "Noto Sans", "DSEG7 Classic", "DSEG14 Classic",
};
#define N_FONT_FAMILY_CHOICES (int)(sizeof(g_font_family_choices) / sizeof(g_font_family_choices[0]))
static int window_is_dock(void);
static int elem_has_class(Elem *e, const char *cls);
static int kh_elem_in_scope(Elem *e);
/* Append one decimal code to the strip's input relay (the same file the
 * terminal keyboard binary and agents write, that the manager's
 * poll_strip_history() consumes). Used to tell the manager "the on-screen
 * highlight just moved to nav N" (6000+N) so its strip_focus_cell - and
 * with it the ASCII mirror - stays in exact lockstep with what the X11
 * strip is showing. Dock only. 2026-09-06. */
static void dock_relay_focus_code(int code) {
    if (!window_is_dock() || code <= 0) return;
    char hist[PATH_BUF];
    snprintf(hist, sizeof(hist), "%s/#.desktop/strip_history.txt", g_house_root);
    FILE *hf = fopen(hist, "a");
    if (hf) { fprintf(hf, "%d\n", code); fclose(hf); }
}
static int click_focus_then_activate(Elem *hit) {
    if (!hit) return 0;
    /* Out-of-scope rows stay numbered and drawn, but a click must not
     * steal focus or fire — same as chtpm_parser.c is_navigable(). */
    if (!kh_elem_in_scope(hit)) return 0;
    /* Dock: bottom-strip HQ window cells (class hqwin / onclick
     * FOCUSWIN:) are the same shape as a taskbar button — first click
     * must raise/restore, not merely focus (Enter already activated;
     * click_two_step made pc-hq look like it "only opens from nav").
     * ACTIVATE (HQ/File-style trigger) on dock cells honors #.desktop/
     * hq_ui.pdl click_two_step: when click_two_step=0, ACTIVATE fires
     * on first click; when click_two_step=1, it requires two-step
     * (first click sets focus, second click activates).
     *
     * REAL FIX 2026-09-10 (direct report: "even tho we are on '2
     * clicks' it only takes one click to open apps from dropdown
     * menus... an agent's false assumption to leave these out of the
     * boolean switch") - `dropdown-child` used to be unconditionally
     * lumped into this immediate-fire group ("a mouse hit on a
     * dropdown-child row opens/runs immediately"), bypassing
     * g_click_two_step entirely regardless of the house-wide setting.
     * That was never an intentional exception, just an old false
     * assumption baked into the comment - removed. A dropdown row is
     * shaped exactly like any other clickable item once its menu is
     * open, so it now falls through to the same click_two_step check
     * every other row uses below. */
    if (window_is_dock() &&
        (elem_has_class(hit, "hqwin") ||
         (hit->onclick[0] && strncmp(hit->onclick, "FOCUSWIN:", 9) == 0) ||
         (!g_click_two_step && strcmp(hit->onclick, "ACTIVATE") == 0))) {
        g_focus_nav = hit->nav_index;
        return 1;
    }
    /* A window can opt every click into single-click-activate with
     * class="single-click" on <window> - regardless of the house-wide
     * click_two_step. For pickers where one click == pick (the periodic
     * table: click a tile -> inspect/place, no "focus first" step). */
    if (!g_click_two_step || (g_window && elem_has_class(g_window, "single-click"))) {
        g_focus_nav = hit->nav_index;
        return 1;
    }
    if (hit->nav_index <= 0) return 1;
    if (g_focus_nav != hit->nav_index) {
        g_focus_nav = hit->nav_index;
        /* first click of a two-step: focus moved but nothing fired yet -
         * tell the manager so the ASCII mirror + strip_focus_cell follow
         * the click, not just arrow keys (dock, header/tab cells only). */
        if (window_is_dock() && !elem_has_class(hit, "dropdown-child"))
            dock_relay_focus_code(6000 + hit->nav_index);
        return 0;
    }
    return 1;
}
/* REAL Stage 5 §5d.10 (2026-08-16) - scaled() is now mode-aware: db-hq
 * mode has a real, user-adjustable DPI scale (g_dbhq_font_scale, read
 * from #.desktop/hq_ui.pdl, ported verbatim from khtpm_hq_render.c's
 * own scaled()); popup modes (entity-menu/taskbar-settings) still have
 * no real DPI-scale source, so stay identity. Must be declared BEFORE
 * khtpm_draw_core.c's own font_for() (below) references it, and before
 * g_dbhq_font_scale's own declaration below uses it transitively via
 * db-hq's ported layout code - forward-declare the flag/scale here. */
/* LayDoc Gap 2: NULL = no ACTIVATE scope. Declared before draw_core
 * include so elem_cursor_prefix can show [^] on the scope root. */
static Elem *g_dbhq_active_scope_root = NULL;
/* 2026-09-04: g_is_stats_hq/g_is_palettes/g_is_bookmarks (and
 * g_is_db_hq before them) removed - stats-hq/palettes/bookmarks all
 * have real live button-pal.sh launchers pointed at their own
 * .xhtpm templates + projectors now; every branch these flags gated
 * was already dead by construction (const 0). See handoff
 * Rev 15 for the audit that found this. */
/* 2026-09-04: piececraft-hq's board window (formerly its own isolated
 * raw-pixel-blit mode, run_pchq_board_mode()/g_is_pchq_board/<window
 * class="pchq-board">) is retired - pchq-board.xhtpm (static template
 * + <canvas> primitive + generic Interact Mode relay) replaced it,
 * live-verified end to end this session. */

/* Screen dimensions - a constant under --headless (no dpy to ask), the
 * real X value otherwise. Every layout-path DisplayWidth/Height call
 * goes through these two so headless layout has a sane viewport. */
static int kh_screen_w(void) { return g_headless ? 1920 : DisplayWidth(dpy, screen); }
static int kh_screen_h(void) { return g_headless ? 1080 : DisplayHeight(dpy, screen); }
/* REAL Stage 5 (2026-08-16, khtpm-merge-how2.md §5d) - shared, generic
 * draw_elem()/render_tree()/font_for() (was hand-rolled, per-app pixel
 * drawing - see khtpm_draw_core.c's own header comment). Included here
 * (after dpy/screen/cmap/gc/buf/xftdraw_buf/g_focus_nav are all
 * already declared above, which it needs).
 * REAL, NEW 2026-09-02 - draw_elem() may skip the numbered nav badge
 * when the element has class=quiet; elem_has_class is defined later
 * in this file, so it is forward-declared here before the include. */
static int elem_has_class(Elem *e, const char *cls);
static int window_is_dock(void);
/* chtpm interact-mode gate: g_nav[] stays the full numbered list;
 * this predicate is the only thing that decides who can take focus.
 * Port of chtpm_parser.c is_navigable()'s active_index branch. */
static int kh_elem_in_scope(Elem *e) {
    if (!e) return 0;
    if (!(g_default_scope_confine && g_default_active_scope_root)) return 1;
    if (e == g_default_active_scope_root) return 1;
    {
        Elem *p;
        for (p = e->parent; p; p = p->parent)
            if (p == g_default_active_scope_root) return 1;
    }
    if (e->id[0] && g_default_active_scope_id[0] &&
        strcmp(e->id, g_default_active_scope_id) == 0) return 1;
    if (g_default_active_scope_id[0] && e->target_id[0] &&
        elem_has_class(e, "dropdown-child") &&
        strcmp(e->target_id, g_default_active_scope_id) == 0) return 1;
    if (e->id[0] && strncmp(e->id, "chrome-", 7) == 0) return 1;
    return 0;
}
#include "khtpm_draw_core.c"
/* ROW_H / CHROME_H are UI-scaled (LIVEDESK-UI-SCALE.md): scaled() is a
 * pure int fn of g_ui_scale_pct (100 = identity), so these stay a
 * single consistent value within any one expression / layout pass and
 * grow the row + titlebar height with the Settings font_scale. Base:
 * 24 / 24. */
#define ROW_H    scaled(24)
#define CHROME_H scaled(24)
#define KH_WIN_FRAME 2

static CssSheet g_sheet;

/* REAL Stage 5 §5d.3 step 6 (2026-08-16, khtpm-merge-how2.md) - the
 * actual literal binary merge. This binary now serves BOTH the real
 * generic menu shape (entity-menu's own original job). 2026-09-04:
 * taskbar-settings' own real swatch-picker shape (`<window class=
 * "swatch-picker">`) was retired - taskbar-settings-pal.xhtpm (the
 * real, live template since 2026-09-03) is a normal generic-mode
 * window and never trips this. */
#define SWATCH 34
#define SWATCH_GAP 8
#define SWATCH_COLS 6
static char g_palette_name_buf[12][32];
static const char *g_palette_name[12];
/* "PICK:<n>" is a generic, always-on dispatch() action token (not
 * gated by any window-class flag) - taskbar-settings-pal.xhtpm's own
 * swatch items use it, consumed by swatch_picker_manager.+x via
 * #.desktop/taskbar_settings_action.txt. See dispatch()'s own PICK:
 * handler below. */
static unsigned g_swatch_action_seq = 0;

/* Cli-io typed commands, e.g. `mv 17 21` = move the thing shown as [ ]17 into
 * the thing shown as [ ]21. Arrives as `STRING: mv 17 21` in the per-pid relay
 * file (same text a human types into the Cli-io field). This window owns the
 * nav list, so it resolves numbers here and forwards a resolved command to the
 * window's manager over its action file; the manager does the file work.
 * Sources: an entry of this window's list (ids entryN/gentryN), or a desk pal
 * tab from the live nav-claim pool. Destination: a directory entry of this
 * window, or any other element of this window (= its current dir).
 * Add verbs by extending the strcmp chain in kh_cliio_exec(). */
static Elem *kh_nav_elem(int n) {
    for (int i = 0; i < g_n_nav; i++)
        if (g_nav[i] && g_nav[i]->nav_index == n) return g_nav[i];
    return NULL;
}

static int kh_entry_idx_of(const Elem *e) {
    if (!e) return -1;
    if (!strncmp(e->id, "gentry", 6)) return atoi(e->id + 6);
    if (!strncmp(e->id, "entry", 5)) return atoi(e->id + 5);
    return -1;
}

static int kh_claimed_tab_path(int nav, char *out, size_t outsz) {
    char cp[PATH_BUF];
    snprintf(cp, sizeof(cp), "%s/#.desktop/livedesk-nav-claims/livedesk_nav_claims.txt", g_house_root);
    FILE *f = fopen(cp, "r");
    if (!f) return 0;
    char line[PATH_BUF + 256];
    int found = 0;
    while (!found && fgets(line, sizeof(line), f)) {
        if (strncmp(line, "KIND=tab|", 9) != 0) continue;
        char key[32];
        snprintf(key, sizeof(key), "|NAV=%d|", nav);
        if (!strstr(line, key)) continue;
        char *pp = strstr(line, "|PATH=");
        if (!pp) continue;
        pp += 6;
        pp[strcspn(pp, "\r\n")] = '\0';
        snprintf(out, outsz, "%s", pp);
        found = 1;
    }
    fclose(f);
    return found;
}

static void kh_cliio_result(const char *msg) {
    if (!g_package_dir[0]) return;
    char rp[PATH_BUF];
    snprintf(rp, sizeof(rp), "%s/cliio_result.txt", g_package_dir);
    FILE *f = fopen(rp, "w");
    if (f) { fprintf(f, "%s\n", msg); fclose(f); }
}

static void kh_cliio_exec(const char *text) {
    char verb[16] = "";
    int a = 0, b = 0;
    if (sscanf(text, "%15s %d %d", verb, &a, &b) < 1) return;
    if (strcmp(verb, "mv") != 0) { kh_cliio_result("error: unknown verb (only mv <nav#> <nav#>)"); return; }
    char af_path[PATH_BUF];
    snprintf(af_path, sizeof(af_path), "%s/file_explorer_action.txt", g_package_dir);
    if (!g_package_dir[0]) return;
    char probe[PATH_BUF];
    snprintf(probe, sizeof(probe), "%s/file_explorer_ui.txt", g_package_dir);
    if (access(probe, F_OK) != 0) { kh_cliio_result("error: this window has no mv handler yet"); return; }
    char src[PATH_BUF + 2], dst[8];
    Elem *ea = kh_nav_elem(a), *eb = kh_nav_elem(b);
    int ia = kh_entry_idx_of(ea), ib = kh_entry_idx_of(eb);
    char tab_path[PATH_BUF];
    if (ia >= 0) snprintf(src, sizeof(src), "e%d", ia);
    else if (kh_claimed_tab_path(a, tab_path, sizeof(tab_path))) snprintf(src, sizeof(src), "p%s", tab_path);
    else { kh_cliio_result("error: source nav# not found"); return; }
    if (ib >= 0) snprintf(dst, sizeof(dst), "e%d", ib);
    else if (eb) snprintf(dst, sizeof(dst), "w");
    else { kh_cliio_result("error: destination nav# not found"); return; }
    FILE *f = fopen(af_path, "w");
    if (!f) return;
    fprintf(f, "seq=%u\ncmd=CLIIO_MV:%s|%s\n", ++g_swatch_action_seq, src, dst);
    fclose(f);
}

static int elem_has_class(Elem *e, const char *cls) {
    for (int i = 0; i < e->n_classes; i++)
        if (strcmp(e->classes[i], cls) == 0) return 1;
    return 0;
}

#define WM_MANAGED_DRAG_MIN_Y 90
/* Fullscreen ("!" / TOGGLE_FULLSCREEN) fills only the desktop WORK AREA
 * - the band between the livedesk top strip (WM_MANAGED_DRAG_MIN_Y) and
 * the bottom dock strip - not the whole display (direct instruction
 * 2026-09-10: "full screen shouldn't go above the size within bottom/
 * top tb of desk"). Follow-up: "too far right off the screen ... when
 * in doubt dont go far, stop short" - so inset a clear margin on every
 * edge (WM frame + being safely inside, matching the ~60px right
 * reserve user-resizable windows already use). */
#define WM_MANAGED_BOTTOM_RESERVE 46
#define WM_FS_MARGIN_X 16
#define WM_FS_MARGIN_RIGHT 140   /* generous - kept overshooting the right; stop short */
#define WM_FS_MARGIN_BOTTOM 32
/* absolute safety cap: fullscreen never exceeds this fraction of the
 * reported display, whatever the WM / HiDPI coord weirdness (direct
 * instruction: "when in doubt dont go far, stop short"). */
#define WM_FS_MAX_PCT 90


/* Real, single-slot font cache for text measurement, ported verbatim
 * (khtpm-merge-how2.md §3.2's own cache pattern, already proven). */
static int kh_measure_text_px(const CssStyle *st, const char *text) {
    if (g_headless) {
        /* no Xft without a display - estimate: ~0.55em advance per
         * UTF-8 codepoint (bytes with (b & 0xC0) != 0x80). Good enough
         * for layout width decisions; the text mirror doesn't render
         * pixels anyway. */
        int size = scaled(st->has_font_size ? st->font_size : 12);
        int adv = (size * 55) / 100; if (adv < 5) adv = 5;
        int n = 0;
        for (const char *p = text ? text : ""; *p; p++)
            if (((unsigned char)*p & 0xC0) != 0x80) n++;
        return n * adv;
    }
    char spec[128];
    const char *fam = st->has_font_family ? st->font_family : g_ui_font_family;
    int size = scaled(st->has_font_size ? st->font_size : 12);
    snprintf(spec, sizeof(spec), "%s:pixelsize=%d%s", fam, size, (st->has_font_weight && st->font_weight_bold) ? ":bold" : "");

    static char cached_spec[128] = "";
    static XftFont *cached_font = NULL;
    XftFont *f;
    if (cached_font && strcmp(cached_spec, spec) == 0) {
        f = cached_font;
    } else {
        if (cached_font) XftFontClose(dpy, cached_font);
        f = XftFontOpenName(dpy, screen, spec);
        if (!f) f = XftFontOpenName(dpy, screen, "DejaVu Sans:pixelsize=10");
        cached_font = f;
        snprintf(cached_spec, sizeof(cached_spec), "%s", spec);
    }
    if (!f) return (int)strlen(text) * 8;
    XGlyphInfo ext;
    XftTextExtentsUtf8(dpy, f, (const FcChar8 *)text, (int)strlen(text), &ext);
    return ext.width;
}

/* Real db-hq layout pass, ported verbatim (already Stage-3-complete -
 * calls the shared css_layout_pass() 3x: tabbar/sidebar/panel). */
/* REAL FIX 2026-08-25 (Stage 2 palettes migration, direct live report:
 * "no longer showing emojis or navs") - css_layout_pass() (shared,
 * khtpm_render_core.c) is recursive but does NOT itself apply CSS to
 * children - it only uses whatever e->style each Elem already has,
 * meaning every Elem in the tree needs dbhq_apply_css() run on it
 * BEFORE layout, not just direct children of whatever loop happens to
 * touch them. dbhq_layout_pass()'s own panel loop only ever CSS'd
 * panel's DIRECT children (rows), never grandchildren (palette tile
 * buttons nested inside each row) - same real bug class already found
 * and fixed once in khtpm_hq_render.c as "apply_css_deep()" (nested
 * elements got zero style before), never ported to this shared/merged
 * binary until now. Scoped to g_is_palettes to avoid changing db-hq's
 * own already-working flat title/text/button behavior. */
/* REAL, NEW 2026-08-25 (live report: "thumb moves but doesn't change
 * display") - css_layout_pass() (shared, khtpm_render_core.c) assigns
 * every element's own ABSOLUTE x/y during its recursion - a tile inside
 * a <row> gets its final on-screen y computed once, right there, not
 * derived from its parent row's y at draw time (draw_elem() reads e->y
 * directly). Shifting only the row container's own y after the fact
 * (the ported khtpm_hq_render.c snippet's own approach) left every tile
 * exactly where it started - only the row's own (invisible) box moved.
 * This walks the whole subtree and shifts every descendant's y by the
 * same delta, so the tiles actually move with their row. */
static void kh_shift_subtree(Elem *e, int dy) {
    if (!e) return;
    e->y += dy;
    for (int i = 0; i < e->n_children; i++) kh_shift_subtree(e->children[i], dy);
}

static void kh_shift_subtree_xy(Elem *e, int dx, int dy) {
    if (!e) return;
    e->x += dx; e->y += dy;
    for (int i = 0; i < e->n_children; i++) kh_shift_subtree_xy(e->children[i], dx, dy);
}



/* ============================================================
 * REAL FRAME-HISTORY-DERIVED PAINT (2026-08-28, Phase 2 of
 * RENDER-FRAME-HISTORY-DRIFT-ASSESSMENT.md - see RENDER-REFACTOR-2DO-
 * PROGRESS.md for the live status of this effort). First real, scoped
 * proof: palettes' panel content (title/hint/tabs/tiles/tileset
 * chooser) is serialized to a real flat file BEFORE painting, and a
 * genuinely separate paint function reads ONLY that file (zero live
 * Elem-tree pointer access) to draw pixels - matching the house-
 * standard wraith-alpha pattern (chtpm_parser.c writes current_
 * frame.txt, renderer.c draws ONLY from it) instead of this file's
 * own prior drift (paint reading directly from a live, mutable Elem
 * tree in the same process). Deliberately scoped to the PANEL subtree
 * only (not the window chrome/close-button/scrollbar-track, which are
 * either already-generic or raw-pixel affordances outside the Elem
 * tree entirely) - see the progress doc for why this is a real,
 * honest first slice and not the whole file done at once.
 * ============================================================ */

/* One frame-file line = one real Elem's worth of drawable state, in
 * the EXACT SAME field order draw_elem() actually reads: tag, id,
 * classes (comma-joined, since Elem itself stores them as an array),
 * label, sprite, onclick, nav_index, active, x, y, w, h. Pipe-
 * delimited (matches this house's own PDL convention elsewhere) -
 * real, current onclick strings never contain a literal '|', but if a
 * future one ever needs to, this format would need real escaping,
 * not silently break (fields are read via strchr('|'), a literal pipe
 * inside a field would misparse loudly, not corrupt quietly). */
/* REAL, NEW 2026-08-31 (generic capability #2 follow-up - found live
 * testing open-hai's own real .chtpm projection: a real, armed cli_io
 * field's own live-typed input_buffer never showed on screen, because
 * this exact frame-file round trip never carried it) - '|' is this
 * format's own field delimiter, so a real input_buffer/target_id value
 * containing a literal '|' (a real shell pipe is a plausible thing to
 * type into a composer) must not reach fprintf() unescaped, or it would
 * misparse exactly like onclick's own pipes once did (see this file's
 * own 2026-08-28 book-stack fix comment above kh_paint_frame_line()).
 * Onclick solves this by anchoring from BOTH ends of the line; these
 * two fields are simpler (no other data depends on their exact byte
 * count) - a real, byte-safe substitution (0x01, a control byte that
 * can never appear in real typed text) round-trips perfectly. */
static void frame_field_escape_pipe(const char *in, char *out, size_t outsz) {
    size_t o = 0;
    for (const unsigned char *p = (const unsigned char *)in; *p && o + 1 < outsz; p++)
        out[o++] = (*p == '|') ? '\x01' : (char)*p;
    out[o] = '\0';
}
static void frame_field_unescape_pipe(const char *in, char *out, size_t outsz) {
    size_t o = 0;
    for (const unsigned char *p = (const unsigned char *)in; *p && o + 1 < outsz; p++)
        out[o++] = (*p == '\x01') ? '|' : (char)*p;
    out[o] = '\0';
}
/* REAL, NEW 2026-09-05 (CLI_IO-CURSOR-AND-TEXT_AREA-MULTILINE-EDITING-
 * DESIGN.md) - text_area_buffer needs the SAME pipe-escape as every
 * other frame field above, PLUS a real newline escape: this frame
 * format is fundamentally line-oriented (kh_paint_frame_line() is
 * called once per fgets()-read line), so a real, literal `\n` a user
 * actually typed into a text_area would otherwise split ONE element's
 * serialized line into two - one truncated, one garbage with no
 * leading fields - the exact "malformed line" case this file's own
 * honest-skip convention exists for, except here it would happen on
 * EVERY real multi-line document instead of never. 0x02 is the same
 * real "control byte that can never appear in real typed text"
 * substitution trick 0x01 already uses for '|' above. */
static void frame_field_escape_text(const char *in, char *out, size_t outsz) {
    size_t o = 0;
    for (const unsigned char *p = (const unsigned char *)in; *p && o + 1 < outsz; p++)
        out[o++] = (*p == '|') ? '\x01' : (*p == '\n') ? '\x02' : (char)*p;
    out[o] = '\0';
}
static void frame_field_unescape_text(const char *in, char *out, size_t outsz) {
    size_t o = 0;
    for (const unsigned char *p = (const unsigned char *)in; *p && o + 1 < outsz; p++)
        out[o++] = (*p == '\x01') ? '|' : (*p == '\x02') ? '\n' : (char)*p;
    out[o] = '\0';
}

static void kh_serialize_frame_elem(FILE *f, Elem *e) {
    char classes_joined[CSS_MAX_CLASSES * 33] = "";
    for (int i = 0; i < e->n_classes; i++) {
        if (i > 0) strcat(classes_joined, ",");
        strcat(classes_joined, e->classes[i]);
    }
    /* REAL, NEW 2026-08-31 - target_id/input_buffer appended as two
     * more trailing fields (see this function's own escape-helper
     * comment just above for why they're pipe-escaped first). Any
     * consumer of this frame-file format from before this change simply
     * never had a cli_io element to serialize (the tag didn't exist
     * yet) - not a compatibility break for anything real. */
    char target_id_esc[64 * 2], input_buffer_esc[256 * 2];
    frame_field_escape_pipe(e->target_id, target_id_esc, sizeof(target_id_esc));
    frame_field_escape_pipe(e->input_buffer, input_buffer_esc, sizeof(input_buffer_esc));
    /* REAL FIX 2026-09-05, live bug (csv-hq's own real grid: a row
     * label like "1  |       |       |..." genuinely needs literal '|'
     * characters to look like spreadsheet columns) - label is one of
     * the FRONT-anchored fields (kh_paint_frame_line() splits fields
     * 0-4 sequentially via strchr('|'), on the documented assumption
     * "they never contain a real pipe in practice" - exactly the same
     * class of bug book-stack's onclick hit in 2026-08-28, just in a
     * front-anchored field instead of the middle one). Escaping label
     * the same proven way target_id/input_buffer/relay/bg already are
     * is simpler than re-anchoring the front split (unlike onclick,
     * label has other real fields BOTH before and after it, so it
     * can't just become "whatever's left in the middle"). */
    char label_esc[256 * 2];
    frame_field_escape_pipe(e->label, label_esc, sizeof(label_esc));
    /* REAL FIX 2026-09-04 (pc-hq-bugs.md Bug 3 - "^" badge never shows
     * for an Interact Mode trigger even though its class/label are
     * current) - e->relay was never serialized here, same gap
     * target_id/input_buffer had before 2026-08-31 fixed it for THEM.
     * Every default/popup-mode window draws through THIS frame-file
     * round trip (see redraw()'s own header comment - render_tree()
     * itself is never called for this mode), so kh_paint_frame_line()'s
     * tmp Elem always had relay[0]=='\0', and draw_core.c's own
     * is_scope check (`g_interact_relay_on && e->relay[0]`) could
     * never see it - not a bug in that check itself, a bug in what it
     * was ever handed. Same escape convention (relay's own value is a
     * comma-joined absolute-path list - could theoretically collide
     * with '|' in a wild house_root, escape defensively same as the
     * other two). */
    char relay_esc[300 * 2];
    frame_field_escape_pipe(e->relay, relay_esc, sizeof(relay_esc));
    /* REAL, NEW 2026-09-04 (same class of gap as relay's own fix just
     * above, caught proactively this time before it became a repeat
     * of that same debugging saga) - e->bg (the new generic bg=
     * attribute, see its own field/apply_attr/draw_elem comments) is
     * ALSO consumed by draw_elem() on the tmp Elem this round trip
     * hands it, so it needs to survive the trip too, same as every
     * other field draw_elem() reads. */
    char bg_esc[16 * 2];
    frame_field_escape_pipe(e->bg, bg_esc, sizeof(bg_esc));
    /* REAL, NEW 2026-09-05 (CLI_IO-CURSOR-AND-TEXT_AREA-MULTILINE-
     * EDITING-DESIGN.md) - e->cursor is a plain int, no escaping
     * needed, but it hits the EXACT same trap relay/bg already did:
     * draw_elem() only ever sees the tmp Elem this round trip hands
     * it, so a cursor that isn't serialized here is a cursor
     * kh_paint_frame_line() can never reconstruct, no matter how
     * correct the key-handling logic is. */
    /* REAL, NEW 2026-09-05 - text_area_buffer, same exact trap as
     * every field above (draw_elem() only ever sees this round trip's
     * own tmp Elem) - see frame_field_escape_text()'s own comment for
     * why this one needs a REAL newline escape too, not just pipe. */
    char text_area_esc[4096 * 2];
    frame_field_escape_text(e->text_area_buffer, text_area_esc, sizeof(text_area_esc));
    /* REAL, NEW 2026-09-05 (GRID-ELEMENT-DESIGN.md) - grid_jump_buffer/
     * grid_cell_buffer get the same pipe escape as label/target_id/etc
     * (either could plausibly contain a literal '|' - a cell's own real
     * content, mid-edit). grid_cur_row/col/edit_mode are plain ints,
     * same as e->cursor above - no escaping needed, but still must be
     * serialized or kh_paint_frame_line()'s tmp Elem can never see them. */
    char grid_jump_esc[16 * 2], grid_cell_esc[256 * 2];
    frame_field_escape_pipe(e->grid_jump_buffer, grid_jump_esc, sizeof(grid_jump_esc));
    frame_field_escape_pipe(e->grid_cell_buffer, grid_cell_esc, sizeof(grid_cell_esc));
    fprintf(f, "%s|%s|%s|%s|%s|%s|%d|%d|%d|%d|%d|%d|%s|%s|%s|%s|%d|%s|%d|%d|%d|%s|%s|%d\n",
            e->tag, e->id, classes_joined, label_esc, e->sprite, e->onclick,
            e->nav_index, e->active, e->x, e->y, e->w, e->h,
            target_id_esc, input_buffer_esc, relay_esc, bg_esc, e->cursor, text_area_esc,
            e->grid_cur_row, e->grid_cur_col, e->grid_edit_mode, grid_jump_esc, grid_cell_esc,
            e->sel_anchor);
}

/* Real recursive serializer, same traversal order render_tree() itself
 * uses (non-title children first, in order, title deferred to last at
 * each level) - PRESERVING draw order matters for real visual parity
 * (a later-drawn element can visually overlap an earlier one). */
/* MILESTONE A polish (direct report: "desk dropdown renders UNDER the
 * game map"). The per-level second-pass defer below only reorders
 * dropdown-child WITHIN one container's subtree - a dropdown nested in
 * an early sibling (the board's <sidebar id="rail">) still paints
 * before a later sibling (<panel> with the <canvas>). Collect every
 * non-dock dropdown-child tree-wide during the first pass and flush it
 * AFTER everything (canvas, footer, chrome) via
 * kh_serialize_frame_deferred(). */
static Elem *g_ser_dd[32];
static int g_ser_dd_n = 0;
static void kh_serialize_frame_subtree(FILE *f, Elem *e);
static void kh_serialize_frame_deferred(FILE *f) {
    for (int i = 0; i < g_ser_dd_n; i++) {
        kh_serialize_frame_elem(f, g_ser_dd[i]);
        kh_serialize_frame_subtree(f, g_ser_dd[i]);
    }
    g_ser_dd_n = 0;
}

static void kh_serialize_frame_subtree(FILE *f, Elem *e) {
    for (int i = 0; i < e->n_children; i++) {
        Elem *c = e->children[i];
        if (strcmp(c->tag, "title") == 0 || strcmp(c->tag, "module") == 0) continue;
        /* REAL FIX 2026-09-03 - this default/popup-mode redraw() never
         * calls the shared render_tree() at all (it goes through this
         * serialize-to-file/paint-from-file path instead, see redraw()'s
         * own header comment); render_tree()'s own class="dropdown-child"
         * defer (added for the "Menu" dropdown, "work for all layouts")
         * has zero effect here, root cause of the first live test
         * drawing nothing visible despite draw_elem() genuinely being
         * called with the right x/y/w/h/bg - it was painted, then
         * immediately painted-over by whatever line came after it in
         * this file, same real reason <title> is deferred below. Same
         * fix, same place, mirrored. */
        if (elem_has_class(c, "dropdown-child")) {
            /* dock keeps its own in-place menu paint; every other window
             * defers tree-wide (flushed last by the caller). */
            if (!window_is_dock() && g_ser_dd_n < 32) g_ser_dd[g_ser_dd_n++] = c;
            continue;
        }
        kh_serialize_frame_elem(f, c);
        kh_serialize_frame_subtree(f, c);
    }
    for (int i = 0; i < e->n_children; i++) {
        Elem *c = e->children[i];
        if (strcmp(c->tag, "title") == 0) kh_serialize_frame_elem(f, c);
        else if (elem_has_class(c, "dropdown-child") && window_is_dock()) {
            if (!g_dock_in_menu_paint) continue;
            kh_serialize_frame_elem(f, c);
            kh_serialize_frame_subtree(f, c);
        }
    }
}

/* Real, genuinely separate paint step - takes a PARSED LINE STRUCT,
 * never an Elem*, proving by construction that this function cannot
 * read anything except what the frame file itself said. Builds a real
 * temporary Elem populated ONLY from the parsed fields, resolves its
 * CSS style the exact same generic way the live tree does (css_
 * compute_style() against tag/classes/active - reused, not
 * reimplemented), then calls the SAME real draw_elem() every other
 * path uses - zero duplicated drawing logic, zero new visual bugs
 * possible from this function's own code (the only thing it does
 * beyond "call the real, already-correct drawing code" is the file
 * parse itself). */
static void kh_paint_frame_line(const char *line) {
    /* REAL, NEW 2026-09-05 - bumped from 2048: text_area_buffer alone
     * can be up to 4096 bytes, escaped (worst case ~2x if every byte
     * needs escaping) - 2048 silently truncated any real document
     * anywhere near that size, corrupting the parse (not just for
     * text_area's own field - a truncated line desyncs every FIELD
     * after it too, tail-anchored or not). Sized with real headroom,
     * not just barely enough. */
    char buf2[9000];
    snprintf(buf2, sizeof(buf2), "%s", line);

    /* REAL FIX 2026-08-28, live bug (book-stack's entity-menu: first
     * item invisible, jumbled into the header). Root cause, confirmed
     * via a real PNG dump (relay 'p'/112), not guessed: book-stack's
     * "Read" item's real onclick shell command contains literal "|"
     * pipe characters (`find ... 2>/dev/null | head -1`, twice) - the
     * OLD sequential from-the-front splitter below treated those as
     * real field delimiters too, shifting nav_index/active/x/y/w/h to
     * consume fragments of the onclick TEXT instead of the real
     * numbers, so that one item painted at garbage coordinates
     * (landing in the header band). Every other converted entity's
     * menu.chtpm (ava/asa/self/3 monsters) happens to have zero pipe
     * characters in any action string, which is why only book-stack
     * ever hit this. Real fix: fields 0-4 (tag/id/classes/label/
     * sprite) are still split from the FRONT (they never contain a
     * real pipe in practice); fields 6-11 (nav_index/active/x/y/w/h)
     * are always-numeric, so they're now peeled from the END instead.
     * Field 5 (onclick) is "whatever's left in the middle" - safe to
     * contain any number of real pipes, since neither anchor searches
     * inside it anymore. */
    char *front[5];
    char *p = buf2;
    for (int i = 0; i < 5; i++) {
        front[i] = p;
        char *bar = strchr(p, '|');
        if (!bar) return; /* malformed line - honest skip, not a crash */
        *bar = '\0';
        p = bar + 1;
    }
    /* [0]=nav_index [1]=active [2]=x [3]=y [4]=w [5]=h [6]=target_id
     * (pipe-escaped) [7]=input_buffer (pipe-escaped) [8]=relay (pipe-
     * escaped, pc-hq-bugs.md Bug 3) [9]=bg (pipe-escaped, REAL, NEW
     * 2026-09-04 - the generic bg= attribute, see Elem.bg's own field
     * comment) [10]=cursor (plain int, REAL, NEW 2026-09-05 - see
     * Elem.cursor's own field comment) [11]=text_area_buffer
     * (pipe-AND-newline-escaped, REAL, NEW 2026-09-05 - see
     * frame_field_escape_text()'s own comment) [12]=grid_cur_row
     * [13]=grid_cur_col [14]=grid_edit_mode (plain ints, REAL, NEW
     * 2026-09-05, GRID-ELEMENT-DESIGN.md) [15]=grid_jump_buffer
     * [16]=grid_cell_buffer (pipe-escaped) [17]=sel_anchor (plain int,
     * REAL, NEW 2026-09-05, TEXT_AREA-SCROLL-GUTTER-SELECTION-DESIGN.md)
     * - a frame file written by an older binary (before these fields
     * existed) simply has fewer tail fields - the loop below returns
     * (honest skip) rather than misparse it, matching this function's
     * existing "malformed line" convention exactly. */
    char *tail[18];
    /* REAL FIX 2026-08-28, same-day self-correction (first attempt at
     * this fix broke EVERY entity menu, not just book-stack's - see
     * git blame if this comment ever needs re-deriving why): the front
     * loop above already wrote NUL bytes earlier in buf2, so
     * `strlen(buf2)` here would measure only up to the FIRST of those
     * (basically just tag's length), not the real end of line. `p`
     * itself still points at an intact, correctly-NUL-terminated
     * remainder (the front loop never touched anything from `p`
     * onward), so `p + strlen(p)` is the real end - `buf2 +
     * strlen(buf2)` is not. */
    char *scan_end = p + strlen(p);
    for (int i = 17; i >= 0; i--) {
        char *bar = NULL;
        for (char *q = scan_end - 1; q >= p; q--) { if (*q == '|') { bar = q; break; } }
        if (!bar) return; /* malformed line - honest skip, not a crash */
        tail[i] = bar + 1;
        *bar = '\0';
        scan_end = bar;
    }
    char *onclick_field = p; /* everything between front[4] and tail[0], pipes and all */

    Elem tmp;
    memset(&tmp, 0, sizeof(tmp));
    snprintf(tmp.tag, sizeof(tmp.tag), "%s", front[0]);
    snprintf(tmp.id, sizeof(tmp.id), "%s", front[1]);
    tmp.n_classes = 0;
    if (front[2][0]) {
        char classbuf[CSS_MAX_CLASSES * 33];
        snprintf(classbuf, sizeof(classbuf), "%s", front[2]);
        char *cp = classbuf;
        while (cp && *cp && tmp.n_classes < CSS_MAX_CLASSES) {
            char *comma = strchr(cp, ',');
            if (comma) *comma = '\0';
            snprintf(tmp.classes[tmp.n_classes++], sizeof(tmp.classes[0]), "%s", cp);
            cp = comma ? comma + 1 : NULL;
        }
    }
    { char label_unesc[256]; frame_field_unescape_pipe(front[3], label_unesc, sizeof(label_unesc));
      snprintf(tmp.label, sizeof(tmp.label), "%s", label_unesc); }
    snprintf(tmp.sprite, sizeof(tmp.sprite), "%s", front[4]);
    snprintf(tmp.onclick, sizeof(tmp.onclick), "%s", onclick_field);
    tmp.nav_index = atoi(tail[0]);
    tmp.active = atoi(tail[1]);
    tmp.x = atoi(tail[2]);
    tmp.y = atoi(tail[3]);
    tmp.w = atoi(tail[4]);
    tmp.h = atoi(tail[5]);
    /* REAL, NEW 2026-08-31 - see kh_serialize_frame_elem()'s own
     * comment. Without this, a cli_io element painted through THIS path
     * (the default/popup mode's real content draw, see redraw()'s own
     * "now the shared, generic render_tree()" comment) always saw an
     * empty input_buffer regardless of what was really typed - `tmp` is
     * a fresh, memset-zeroed local on every call, never the live Elem
     * a human is actually typing into. */
    frame_field_unescape_pipe(tail[6], tmp.target_id, sizeof(tmp.target_id));
    frame_field_unescape_pipe(tail[7], tmp.input_buffer, sizeof(tmp.input_buffer));
    frame_field_unescape_pipe(tail[8], tmp.relay, sizeof(tmp.relay));
    frame_field_unescape_pipe(tail[9], tmp.bg, sizeof(tmp.bg));
    tmp.cursor = atoi(tail[10]); /* plain int, no escaping - see Elem.cursor's own field comment */
    frame_field_unescape_text(tail[11], tmp.text_area_buffer, sizeof(tmp.text_area_buffer));
    /* REAL, NEW 2026-09-05, GRID-ELEMENT-DESIGN.md - same trap every
     * field above already hit: draw_elem() only ever sees this round
     * trip's own tmp Elem, so a grid element's live cursor/edit-mode/
     * jump-buffer/cell-buffer that isn't serialized+parsed here can
     * never actually reach the screen, no matter how correct the
     * key-handling logic is. */
    tmp.grid_cur_row = atoi(tail[12]);
    tmp.grid_cur_col = atoi(tail[13]);
    tmp.grid_edit_mode = atoi(tail[14]);
    frame_field_unescape_pipe(tail[15], tmp.grid_jump_buffer, sizeof(tmp.grid_jump_buffer));
    frame_field_unescape_pipe(tail[16], tmp.grid_cell_buffer, sizeof(tmp.grid_cell_buffer));
    tmp.sel_anchor = atoi(tail[17]); /* plain int - see Elem.sel_anchor's own field comment */

    css_compute_style(&g_sheet, tmp.tag, tmp.id[0] ? tmp.id : NULL, tmp.classes, tmp.n_classes, tmp.active, &tmp.style);
    if (window_is_dock()) {
        snprintf(tmp.style.fg_color, sizeof(tmp.style.fg_color), "%s", g_theme_fg);
        tmp.style.has_fg_color = 1;
        if (elem_has_class(&tmp, "dropdown-child")) {
            snprintf(tmp.style.bg_color, sizeof(tmp.style.bg_color), "%s", g_theme_bg);
            tmp.style.has_bg_color = 1;
        }
    }
    draw_elem(&tmp, 0);
}




/* Write rmmv_active.txt in-process (all three fields) and update
 * g_pal_active_* so A-E / tileset highlight moves on press 1.
 * Detached set_rmmv + 1s manager poll is why it took 2-3 presses. */

/* Rebuild the sidebar+panel for whatever g_dbhq_current_tab now is - the
 * non-CE remainder of the old dbhq_restore_tab_content() (the Common
 * Events INLINE editor is gone; db-hq-pal opens the ported events-hq
 * window for that instead). */
/* ====================== end db-hq mode block ========================= */

/* events-hq's C mode (its own evhq_-prefixed draw/render copies and the
 * g_is_events_hq flag) was deleted 2026-09-03 - events-hq is now
 * events-hq.xhtpm + a projector on this generic renderer. The dead flag was
 * removed 2026-09-20 in the dock-unfactor dead-code pass
 * (08-roadmap/design-docs/DOCK-UNFACTOR-AUDIT.md §2). */
/* g_is_chat_hai removed 2026-09-01 - chat-hai's own hardcoded mode is
 * gone (migrated onto the generic sidebar/panel/scrolllist/cli_io
 * path, see chat_hai_projector.sh's own header comment); it now
 * carries no class= at all and is indistinguishable from any other
 * generic default-mode window. */
static void dump_frame_png(void); /* forward decl - the 'p' dump shortcut calls the shared one, defined later */
static void zero_nav_subtree(Elem *e) {
    if (!e) return;
    e->nav_index = 0;
    for (int i = 0; i < e->n_children; i++) zero_nav_subtree(e->children[i]);
}

/* Recursively resolve CSS for every Elem in a subtree. css_layout_pass()
 * reads e->style but never fills it, and the classic layout helpers only
 * CSS their direct children - a flex subtree needs every node styled
 * first. */
static void kh_css_deep(Elem *e) {
    if (!e) return;
    css_compute_style(&g_sheet, e->tag, e->id[0] ? e->id : NULL,
                      e->classes, e->n_classes, 0, &e->style);
    for (int i = 0; i < e->n_children; i++) kh_css_deep(e->children[i]);
}

/* Number every actionable <item>/<cli_io> in a subtree in document
 * order (g_nav[]/g_n_nav). Used for a region laid out by css_layout_pass
 * (flex), which sizes/positions boxes but assigns no nav. */
static void kh_assign_nav_subtree(Elem *e) {
    if (!e) return;
    int actionable = (strcmp(e->tag, "item") == 0 || strcmp(e->tag, "cli_io") == 0) &&
                     (e->onclick[0] || e->label[0]);
    /* Renumber unconditionally every call, like every other layout pass
     * (++g_n_nav). A stale nav_index left on the Elem from the previous
     * frame must NOT be kept - assign_nav_and_layout() resets g_n_nav to
     * 0 but not the per-Elem field, so keeping it produced two items
     * sharing an index (a tile stuck at 1, chrome also at 1). */
    if (actionable && g_n_nav < MAX_ELEMS) {
        e->nav_index = ++g_n_nav;
        g_nav[g_n_nav - 1] = e;
    } else if (!actionable) {
        e->nav_index = 0;
    }
    for (int i = 0; i < e->n_children; i++) kh_assign_nav_subtree(e->children[i]);
}

/* REAL FIX 2026-08-29 (EVENTS-HQ-RENDER-UNIFICATION-PLAN.md Part B) -
 * extracted from events-hq's own view_mode==1 branch (was hardcoded
 * to that one caller's "window"/"viewmode_stub" globals) so Common
 * Events (db-hq) can build the SAME real Scratch block-palette/
 * placement view into its OWN target stub Elem, instead of a second,
 * duplicated copy of this logic - the whole point of Part A unifying
 * the paint layer first. Nav-based click-to-place, NOT drag/drop:
 * left = block palette (onclick "BLOCK:SEL:<i>", selected gets
 * .selected), right = the SCRATCHBLOCK rows + a "[].<#>" place slot
 * (onclick "BLOCK:PLACE" -> evhq_request_append_node(), the same
 * action.txt boundary append both modes already share). Parameterized
 * on the target stub + real content geometry + window width - no
 * caller-specific globals reached into directly. */
static int kh_nonfatal_x_error(Display *d, XErrorEvent *e) {
    char ebuf[128]; XGetErrorText(d, e->error_code, ebuf, sizeof(ebuf));
    fprintf(stderr, "khtpm_entity_menu_render: events-hq: X error (non-fatal): %s (request %d.%d)\n", ebuf, e->request_code, e->minor_code);
    return 0;
}
/* ==================== end events-hq mode block ======================== */

/* REAL, NEW 2026-09-01 - moved out of the (now-deleted) chat-hai
 * mode block below: still genuinely used by the shared is_popup
 * drag-to-move handling (~line 10555 area), not chat-hai-specific
 * despite living inside that block historically. */
static int g_popup_dragging = 0;
static int g_popup_drag_last_x = 0, g_popup_drag_last_y = 0;
/* REAL, NEW 2026-09-14, direct live report ("i see shift arrow working
 * now. but not mouse drag. can u fix that?") - real mouse drag-select
 * for cli_io/text_area. g_text_drag_elem is the armed field currently
 * being dragged (NULL when not dragging) - checked instead of just a
 * bare bool so a stray MotionNotify after the field somehow changed
 * out from under a drag (a reparse mid-drag, a real edge case) can't
 * write into a dangling pointer. */
static Elem *g_text_drag_elem = NULL;

/* User drag-resize (2026-09-09, direct request: "we used to have window
 * grab stretch resize - definitely want that for this window, but
 * toggleable by project layout or pdl"). Opt-in per window via
 * <window class="user-resizable">. When on: a ⌟ glyph is drawn in the
 * bottom-right, and a button-1 drag started in that KH_RESIZE_GRIP hot
 * corner resizes the window (XResizeWindow + relayout). Zero effect on
 * every other window. */
static int g_user_resizable = 0;
static int g_win_resizing = 0;
static int g_resize_start_xr = 0, g_resize_start_yr = 0, g_resize_start_w = 0, g_resize_start_h = 0;
#define KH_RESIZE_GRIP 20
#define KH_WIN_MIN_W   220
#define KH_WIN_MIN_H   140

/* REAL, NEW 2026-09-01 - the old chat-hai mode block (~2,500 lines,
 * chai_-prefixed: its own draw_elem/render_tree/CSS apply/layout/
 * handle_key/click handling) was fully deleted here, along with every
 * scattered `if (g_is_chat_hai)` check across every shared function
 * (click/key/focus handlers, window creation, CSS/history path
 * selection, etc.) and the `g_is_chat_hai` variable itself - see that
 * removed declaration's own comment. Real replacement: chat-hai's
 * `<window>` tag now carries no class= at all, so it's genuinely
 * indistinguishable from any other generic default-mode window - see
 * chat_hai_projector.sh's own header comment for the full migration. */
/* REAL BUG FOUND 2026-08-15 (direct report: "clicking ON the message in
 * window crashed window"): g_n_elems is a bump-allocator index that
 * elem_new() NEVER rewinds. chai_inject_sessions()/chai_inject_panel_feed() (see
 * their own header comments) call elem_new() fresh every single
 * chai_redraw() with no NULL-check before dereferencing the result - so
 * after ~MAX_ELEMS cumulative allocations across the session's whole
 * chai_redraw history (not tied to any one click, just whichever chai_redraw
 * happens to be the one that finally exhausts the pool), elem_new()
 * starts returning NULL and the very next `item->parent = ...` write
 * segfaults. chai_n_elems_static is the fix: captured once, right after
 * parse_chtpm() in main(), as the count of REAL .chtpm-declared
 * elements; chai_layout_pass() rewinds g_n_elems to this baseline every
 * frame before any dynamic injection runs, so the pool never grows
 * across frames - bounded and deterministic, not merely "big enough for
 * now." (Same underlying flaw existed in the file's original
 * inject_sidebar_items(), not something this session's edits
 * introduced - just newly triggered by feed items now living in the
 * panel instead of a shorter-lived sidebar-only list.) */

/* ============ end chat-hai mode content ============ */

/* ============ generic sidebar+panel scroll (default/popup mode) ============
 * REAL, NEW 2026-08-31 (open-hai's own real conversion, direct
 * instruction: real growing-window bug + "we actually didn't number
 * every message, but we did number a sidebar with different chat
 * sessions to resume" - full sidebar redesign, not a quick scroll cap).
 * Purely additive to the default/popup mode's own list-layout branch -
 * a page with no <sidebar>/<panel> tags gets the EXACT SAME flat
 * behavior as before (swatch-picker, choice-picker, taskbar-settings,
 * network-browser's own current .chtpm - none use these tags, none
 * regress). Zero project knowledge: tag-based only (item/text/cli_io/
 * scrolllist), same discipline as launch_module()/reparse_chtpm_if_
 * changed()/the generic <cli_io> element itself - any future khtpm
 * consumer with a long list + a composer can use this, not just
 * open-hai. */
#define SIDEBAR_W 220
#define DEFAULT_WIN_W 700
#define DEFAULT_WIN_H 520

static int g_default_sidebar_scroll = 0;
/* g_default_scrolllist_scroll itself is forward-declared earlier, right
 * after g_default_input_elem - see that comment for why. */
/* Real nav-index ranges each scrollable region owns this frame - set by
 * layout_scroll_region() below, read by the generic Page_Up/Page_Down
 * handler (handle_key()'s own new branch) to know WHICH region's own
 * scroll variable a page-key should adjust (whichever range g_focus_nav
 * currently falls inside). [lo,hi] inclusive; [0,0] means "no items,
 * nothing to scroll" (a fresh page/an empty sidebar). */
static int g_default_sidebar_nav_lo = 0, g_default_sidebar_nav_hi = 0;
static int g_default_scrolllist_nav_lo = 0, g_default_scrolllist_nav_hi = 0;
/* extra <panel> regions (flex-row page, e.g. Canvas-Craft's inventory
 * column). Shared scroll cursor across all extras. */
static int g_default_panel2_scroll = 0;
static int g_default_panel2_nav_lo = 0, g_default_panel2_nav_hi = 0;

/* REAL, NEW 2026-09-02 - generic visible scrollbar for ANY <scrolllist>
 * that overflows its viewport. Palette-grid already had its own track
 * (g_pal_track_*); this is the shared sidebar/panel scrolllist path
 * (layout_scroll_region), not a second renderer and not per-app.
 * Track + thumb only. Mouse-drag is NOT wired: default/popup click
 * capture ignores wheel/drag on a dedicated thumb (MOUSE_EVENT skips
 * buttons 4/5; MotionNotify has no generic-thumb drag). Wheel on the
 * viewport is handled in poll_agent_history. Page_Up/Page_Down already
 * moved *scroll. Thumb position follows that offset so the user can
 * SEE where they are. */
#define GENERIC_SCROLLBAR_W 8
typedef struct {
    int vx, vy, vw, vh;
    int track_x, track_y, track_w, track_h;
    int thumb_y, thumb_h;
    int *scroll;
    int total, visible, max_scroll;
} GenericScrollBar;
static GenericScrollBar g_generic_sbars[8];
static int g_n_generic_sbars;
/* REAL, NEW 2026-09-03 (direct live report: "chat hai doesn't have the
 * thumb navs that some other projects (like palletes) have yet" -
 * palettes' own real up/down arrow buttons, "REAL, NEW 2026-08-25 -
 * they need to be numbered (1 and 2), with nav feature for
 * accessibility/disabled", ported here generically instead of being
 * palette-grid-only forever). Real, static-storage Elems, the SAME
 * "outside the parsed tree" pattern g_default_close_elem already uses -
 * one up/down pair per registered scrollbar slot, index-matched to
 * g_generic_sbars[]. Real nav_index, real onclick ("SCROLLUP:<i>"/
 * "SCROLLDOWN:<i>"), so an AI/keyboard-only session (this house's own
 * digit-jump nav convention) can actually reach and use them - the
 * existing thumb/track was mouse-only. */
static Elem g_sbar_up_elem[8], g_sbar_down_elem[8];

static void generic_sbar_reset(void) { g_n_generic_sbars = 0; }

/* REAL, NEW 2026-09-04 (direct instruction: "this should be guarded
 * against in code, till it works right") - force any synthetic edge
 * affordance (chrome X/!/_, scrollbar ^/v arrows) fully inside the
 * window, INSET past the 2px theme frame. Shrinks it if it is wider/
 * taller than the window, then pulls it in from any edge it crosses. */
static void kh_clamp_elem_onscreen(Elem *e) {
    const int M = 6;  /* frame (2px) + a little air */
    if (!e || e->w <= 0) return;
    if (e->w > g_win_w - 2 * M) e->w = g_win_w - 2 * M;
    if (e->x + e->w > g_win_w - M) e->x = g_win_w - M - e->w;
    if (e->x < M) e->x = M;
    if (e->h > 0) {
        if (e->h > g_win_h - 2 * M) e->h = g_win_h - 2 * M;
        if (e->y + e->h > g_win_h - M) e->y = g_win_h - M - e->h;
        if (e->y < 2) e->y = 2;
    }
}

#define GENERIC_SBAR_ARROW_H 14
static void generic_sbar_register(int x, int y, int w, int h, int *scroll,
                                  int total, int visible, int max_scroll) {
    GenericScrollBar *b;
    int th, usable, sc, slot;
    if (g_n_generic_sbars >= 8) return;
    if (w < GENERIC_SCROLLBAR_W + 8 || h < 8) return;
    if (max_scroll < 1) return;
    slot = g_n_generic_sbars;
    b = &g_generic_sbars[g_n_generic_sbars++];
    b->vx = x; b->vy = y; b->vw = w; b->vh = h;
    b->track_w = GENERIC_SCROLLBAR_W;
    b->track_x = x + w - GENERIC_SCROLLBAR_W - 12;  /* clear of the 2px window frame + screen-edge margin */
    b->scroll = scroll;
    b->total = total;
    b->visible = visible;
    b->max_scroll = max_scroll;
    /* REAL, NEW 2026-09-03 - real up/down arrow buttons reserve their
     * own ARROW_H at each end of the track, same real classic-scrollbar
     * shape palettes' own g_pal_arrow_up/g_pal_arrow_down already use
     * (track shrinks to make room, not drawn on top of it) - see this
     * struct's own header comment for the real "why" (AI/keyboard-only
     * reachability, not decoration). Falls back to the old full-height
     * track when there's no real room for both arrows plus a usable
     * thumb (a very short scrolllist), so nothing regresses there. */
    int have_arrows = (h > GENERIC_SBAR_ARROW_H * 2 + 12);
    b->track_y = y + (have_arrows ? GENERIC_SBAR_ARROW_H : 0);
    b->track_h = h - (have_arrows ? GENERIC_SBAR_ARROW_H * 2 : 0);
    th = (total > 0) ? (b->track_h * visible) / total : b->track_h;
    if (th < 12) th = 12;
    if (th > b->track_h) th = b->track_h;
    usable = b->track_h - th;
    sc = scroll ? *scroll : 0;
    if (sc < 0) sc = 0;
    if (sc > max_scroll) sc = max_scroll;
    b->thumb_h = th;
    b->thumb_y = b->track_y + ((max_scroll > 0 && usable > 0) ? (sc * usable) / max_scroll : 0);

    if (!have_arrows) { g_sbar_up_elem[slot].w = 0; g_sbar_down_elem[slot].w = 0; return; }
    Elem *up = &g_sbar_up_elem[slot], *dn = &g_sbar_down_elem[slot];
    memset(up, 0, sizeof(*up)); memset(dn, 0, sizeof(*dn));
    snprintf(up->tag, sizeof(up->tag), "item"); snprintf(dn->tag, sizeof(dn->tag), "item");
    snprintf(up->id, sizeof(up->id), "sbar-up-%d", slot);
    snprintf(dn->id, sizeof(dn->id), "sbar-down-%d", slot);
    snprintf(up->classes[0], sizeof(up->classes[0]), "sbar-arrow"); up->n_classes = 1;
    snprintf(dn->classes[0], sizeof(dn->classes[0]), "sbar-arrow"); dn->n_classes = 1;
    snprintf(up->label, sizeof(up->label), "^");
    snprintf(dn->label, sizeof(dn->label), "v");
    if (sc > 0) snprintf(up->onclick, sizeof(up->onclick), "SCROLLUP:%d", slot);
    if (sc < max_scroll) snprintf(dn->onclick, sizeof(dn->onclick), "SCROLLDOWN:%d", slot);
    /* draw_elem forces a "[ ]NN. " nav badge on any nav_index>0 element,
     * so a track-width (8px) box clipped the badge + glyph off the edge
     * ("cant see the number for the thumb navs"). Give them a real
     * badge-width box, right-aligned to the track, then hard-clamp fully
     * inside the window (see kh_clamp_elem_onscreen). */
    int aw = 54, ah = GENERIC_SBAR_ARROW_H;
    up->w = aw; up->h = ah; up->y = y;
    up->x = b->track_x + b->track_w - aw;
    dn->w = aw; dn->h = ah; dn->y = y + h - ah;
    dn->x = b->track_x + b->track_w - aw;
    kh_clamp_elem_onscreen(up);
    kh_clamp_elem_onscreen(dn);
    css_compute_style(&g_sheet, up->tag, up->id, up->classes, up->n_classes, 0, &up->style);
    css_compute_style(&g_sheet, dn->tag, dn->id, dn->classes, dn->n_classes, 0, &dn->style);
    up->nav_index = ++g_n_nav; g_nav[g_n_nav - 1] = up;
    dn->nav_index = ++g_n_nav; g_nav[g_n_nav - 1] = dn;
}

static void draw_generic_scrollbars(void) {
    int i;
    for (i = 0; i < g_n_generic_sbars; i++) {
        GenericScrollBar *b = &g_generic_sbars[i];
        XSetForeground(dpy, gc, alloc_pixel("#2a2a2a"));
        XFillRectangle(dpy, buf, gc, b->track_x, b->track_y,
                       (unsigned)b->track_w, (unsigned)b->track_h);
        XSetForeground(dpy, gc, alloc_pixel("#888888"));
        XFillRectangle(dpy, buf, gc, b->track_x + 1, b->thumb_y,
                       (unsigned)(b->track_w > 2 ? b->track_w - 2 : b->track_w),
                       (unsigned)b->thumb_h);
    }
}

static int generic_sbar_wheel(int mx, int my, int dir) {
    int i;
    for (i = 0; i < g_n_generic_sbars; i++) {
        GenericScrollBar *b = &g_generic_sbars[i];
        if (!b->scroll) continue;
        if (mx >= b->vx && mx < b->vx + b->vw &&
            my >= b->vy && my < b->vy + b->vh) {
            *b->scroll += dir;
            return 1;
        }
    }
    return 0;
}

/* REAL, NEW 2026-09-01 (live report: "still missing x and !" - real
 * chrome affordances, same real "X" (close) / "!" (fullscreen) pair
 * piececraft-hq's own real board-mode chrome already uses, direct
 * instruction to reuse that convention) - two real, generic, ALWAYS-
 * present chrome buttons for the sidebar+panel layout, same "outside
 * the parsed tree" static-storage pattern db-hq's own g_dbhq_close_elem
 * already uses (NOT allocated from elem_new()'s pool every frame - that
 * would leak a pool slot every single redraw, since only a real
 * reparse resets g_n_elems). Scoped to layout_sidebar_panel() only -
 * every OTHER default-mode consumer (swatch-picker/choice-picker/
 * taskbar-settings) already has its own real, data-driven close
 * convention (a `<item id="close">`/`class="close-btn"`), so adding
 * this unconditionally to every default-mode window would double up
 * on those, not fix a real gap - sidebar+panel is the one real shape
 * that currently has none. */
/* REAL, NEW 2026-09-01 - set once a page has real been laid out via
 * layout_sidebar_panel() (see assign_nav_and_layout()'s own call site
 * comment) - dispatch()'s own tail reads this to skip its default
 * "menus close after a real action fires" behavior for a genuinely
 * persistent window. */
static int g_default_has_sidebar_panel = 0;
/* REAL, NEW 2026-09-04 - a persistent tile/grid window (palettes: a
 * <page> of <repeat> tiles, class="palettes-pal"/"database-window")
 * is NOT a transient context menu: firing a tile's action= must not
 * close it, same reasoning as g_default_has_sidebar_panel but without
 * needing the <sidebar>+<panel> structure that flag is tied to. Set
 * once from the window class in main(). */
static int g_default_persistent = 0;
/* REAL, NEW 2026-09-09 - leftmost x of the has_canvas layout's template
 * chrome trio (x / ! / _), captured during layout (post window-frame
 * shift). The ButtonPress drag-start zone uses it to know which part of
 * the top strip is real chrome (must take a click) vs draggable margin -
 * the sidebar+panel layout has g_default_*_elem for this, the flat
 * toolbar/canvas layout did not, so a mouse click on "!" or "_" was
 * eaten as a window drag ("worked from nav, not mouse"). 0 = no canvas
 * chrome this frame (fall back to the old g_win_w-60 constant). */
static int g_canvas_chrome_left_x = 0;
static Elem g_default_close_elem_storage;
static Elem *g_default_close_elem = &g_default_close_elem_storage;
static Elem g_default_fullscreen_elem_storage;
static Elem *g_default_fullscreen_elem = &g_default_fullscreen_elem_storage;
static Elem g_default_minimize_elem_storage;
static Elem *g_default_minimize_elem = &g_default_minimize_elem_storage;
static int g_hq_minimized = 0;
static int g_default_is_fullscreen = 0;
static int g_default_pre_fullscreen_x = 0, g_default_pre_fullscreen_y = 0;

static int window_is_entity_menu(void) {
    int i;
    if (!g_window) return 0;
    for (i = 0; i < g_window->n_classes; i++)
        if (strcmp(g_window->classes[i], "entity-menu") == 0) return 1;
    return 0;
}

/* Auto chrome trio for swatch-grid / flat-page (2026-09-18): same
 * g_default_*_elem as sidebar+panel. Place right-to-left from *chrome_x
 * (X then ! then _ → visual _ ! X). No template items.
 * Entity menus: chrome on the TOP row; nametag/focus one row below
 * (user 2026-09-18). */
static void kh_place_chrome_btn(Elem *e, const char *id, const char *label,
                                const char *onclick, int *chrome_x, int chrome_y) {
    memset(e, 0, sizeof(*e));
    snprintf(e->tag, sizeof(e->tag), "item");
    snprintf(e->id, sizeof(e->id), "%s", id);
    snprintf(e->label, sizeof(e->label), "%s", label);
    snprintf(e->onclick, sizeof(e->onclick), "%s", onclick);
    css_compute_style(&g_sheet, "item", id, NULL, 0, 0, &e->style);
    int cw = kh_measure_text_px(&e->style, label) + 52;
    if (cw < 48) cw = 48;
    *chrome_x -= cw;
    e->x = *chrome_x; e->y = chrome_y; e->w = cw; e->h = CHROME_H - 4;
    kh_clamp_elem_onscreen(e);
    *chrome_x = e->x - 4;
    e->nav_index = ++g_n_nav;
    g_nav[g_n_nav - 1] = e;
}

/* Wrap <tab> children like palette chips (2026-09-18 breadcrumbs).
 * Returns pixel height of the tabbar box. Does not grow g_win_w —
 * extra crumbs take new rows; user-resizable windows keep their width. */
static int kh_layout_tabbar_wrap(Elem *tabbar, int x0, int y0) {
    int row_h = scaled(28);
    int th = row_h - scaled(4);
    int tx = x0, ty = y0 + scaled(2);
    int rows = 1;
    int i;
    if (!tabbar || tabbar->n_children <= 0) return 0;
    for (i = 0; i < tabbar->n_children; i++) {
        Elem *tab = tabbar->children[i];
        if (strcmp(tab->tag, "tab") != 0) continue;
        css_compute_style(&g_sheet, tab->tag, tab->id, tab->classes, tab->n_classes, 0, &tab->style);
        int tw = scaled(52);
        if (font_ui && tab->label[0]) {
            XGlyphInfo gi;
            XftTextExtentsUtf8(dpy, font_ui, (const FcChar8 *)tab->label, (int)strlen(tab->label), &gi);
            tw += gi.xOff;
        }
        if (tx > x0 && tx + tw > g_win_w - 8) {
            tx = x0;
            ty += row_h;
            rows++;
        }
        tab->x = tx; tab->y = ty; tab->w = tw; tab->h = th;
        tab->nav_index = ++g_n_nav;
        g_nav[g_n_nav - 1] = tab;
        tx += tw + scaled(3);
    }
    {
        int h = rows * row_h;
        tabbar->x = 0; tabbar->y = y0; tabbar->w = g_win_w; tabbar->h = h;
        css_compute_style(&g_sheet, tabbar->tag, tabbar->id, tabbar->classes, tabbar->n_classes, 0, &tabbar->style);
        return h;
    }
}

/* Lays out `container`'s own direct item/text children as a real,
 * generic scrollable list clipped to the given box - only `visible_rows`
 * of them (h/ROW_H) are ever given a real position/nav_index; the rest
 * are pushed off-canvas (never drawn, never focusable) until a real
 * Page_Up/Page_Down (see handle_key()'s own new branch) moves `*scroll`
 * and brings them into view on the next redraw. text children take a
 * row like item children (real vertical space) but never get a
 * nav_index (not interactive) - same real convention the flat-list
 * branch's own 2026-08-31 text-row fix already established. Returns the
 * [lo,hi] real nav_index range this call assigned, via *out_lo/*out_hi
 * (both 0 if the container had zero item children). */
/* REAL FIX 2026-09-02 - generic scrolllist rows are ROW_H (24px). An
 * item with sprite= still blits via hq_sprite() but its box was 24px,
 * so the tile crushed to ~16px and the nav badge painted into the
 * previous row. Sprite items occupy enough ROW_H slots for a 64px
 * blit (HQ_SPRITE_PX_MAX) plus padding. Scroll math stays row-based.
 * Zero per-app flags - any consumer of sprite= in a scrolllist gets it.
 *
 * REAL, NEW 2026-09-02 - a <row class="sprite-grid-row"> (or a <row>
 * whose children are all sprite items) lays those children horizontally
 * inside one scroll slot whose height is one tall sprite tile. Palettes
 * pal-grid-row is a different mechanism (generic_scroll_layout_pass) and
 * is explicitly skipped here. */
/* Min cell width for sprite-grid-row children. 64px blit is unchanged
 * (draw_core caps at HQ_SPRITE_PX_MAX). Wider cell = one-line labels
 * can show ~40 chars instead of clipping to "KPO...". Remainder of the
 * row is split evenly so 4 columns still fill ~760px content. */
#define SPRITE_GRID_TILE_W 168
#define SPRITE_GRID_TILE_H 96

static int scroll_is_sprite_grid_row(const Elem *c) {
    int i, n_items = 0, n_sprites = 0;
    if (!c || strcmp(c->tag, "row") != 0) return 0;
    if (elem_has_class((Elem *)c, "pal-grid-row")) return 0;
    if (elem_has_class((Elem *)c, "sprite-grid-row")) return 1;
    for (i = 0; i < c->n_children; i++) {
        const Elem *ch = c->children[i];
        if (strcmp(ch->tag, "item") != 0 && strcmp(ch->tag, "text") != 0) continue;
        n_items++;
        if (ch->sprite[0]) n_sprites++;
    }
    return n_items > 0 && n_sprites == n_items;
}

static int sprite_grid_cols(int w) {
    int cols = w / SPRITE_GRID_TILE_W;
    return cols < 1 ? 1 : cols;
}

static int sprite_grid_n_tiles(const Elem *c) {
    int i, n = 0;
    if (!c) return 0;
    for (i = 0; i < c->n_children; i++) {
        const Elem *ch = c->children[i];
        if (strcmp(ch->tag, "item") == 0 || strcmp(ch->tag, "text") == 0) n++;
    }
    return n;
}

static int sprite_grid_visual_lines(const Elem *c, int w) {
    int n = sprite_grid_n_tiles(c);
    int cols = sprite_grid_cols(w);
    int lines = n > 0 ? (n + cols - 1) / cols : 1;
    return lines < 1 ? 1 : lines;
}

/* REAL, NEW 2026-09-12 (NETWORK-BROWSER-VIDEO-V3-DESIGN.md §2/§2.3) - a
 * canvas INSIDE a <scrolllist> gets a real multi-row span so the live
 * video surface is scrollable content, not a 1-row sliver. Frame height
 * comes from the SAME sibling receipt kh_draw_canvas() reads
 * (<base>.receipt.txt, frame_h=/overlay_h= priority - matching build
 * convention so one contract sizes both layout AND paint), squashed to
 * ROW_H units. No file open on the hot path: NaN-proof, and a missing
 * receipt (producer still writing) yields a 1-row placeholder at worst. */
static int scroll_canvas_frame_h(const Elem *c) {
    if (!c || !c->sprite[0]) return 0;
    char rc[512];
    { const char *dot = strrchr(c->sprite, '.');
      if (dot && strcmp(dot, ".raw") == 0)
          snprintf(rc, sizeof(rc), "%.*s.receipt.txt", (int)(dot - c->sprite), c->sprite);
      else
          snprintf(rc, sizeof(rc), "%s.receipt.txt", c->sprite);
    }
    int h = 0;
    FILE *rf = fopen(rc, "r");
    if (rf) {
        char l[128];
        while (fgets(l, sizeof(l), rf)) {
            if (!strncmp(l, "overlay_h=", 10)) h = atoi(l + 10);
            else if (!h && !strncmp(l, "frame_h=", 8)) h = atoi(l + 8);
        }
        fclose(rf);
    }
    return h;
}

static int scroll_row_span(const Elem *c, int w) {
    if (scroll_is_sprite_grid_row(c)) {
        int line_span = (SPRITE_GRID_TILE_H + ROW_H - 1) / ROW_H;
        return sprite_grid_visual_lines(c, w) * line_span;
    }
    if (c && strcmp(c->tag, "canvas") == 0) {
        int fh = scroll_canvas_frame_h(c);
        if (fh < ROW_H) fh = ROW_H;
        return (fh + ROW_H - 1) / ROW_H;
    }
    /* class="sprite-inline": keep a single ROW_H slot; draw_core's short-bar
     * path then blits a 24px sprite left of the label (taskbar look), so a
     * list of mixed sprite/non-sprite rows stays uniform. */
    if (c && c->sprite[0] && !elem_has_class((Elem *)c, "sprite-inline"))
        return (64 + ROW_H + 8 + ROW_H - 1) / ROW_H; /* 64px blit + one ROW_H for the nav chip */
    /* REAL FIX 2026-09-03 (direct live report: co-lab-hai's own long
     * agent messages clipped with "..." instead of wrapping, unlike
     * chat-hai's own rows - traced to this exact function always
     * returning 1, so no plain <text> row was ever given more than one
     * ROW_H of real box height to wrap into, even after khtpm_draw_
     * core.c's own wrap path was generalized past cli_io-only - see
     * that file's wrap_line_count()/is_multiline_box comments for the
     * other half of this fix). A real, generic capability: any plain
     * <text> row (no sprite, not a grid) whose label needs more than
     * one wrapped line at this row's own real width gets that many
     * ROW_H units, computed via the SAME real CSS style/font this
     * element will actually draw with (not a guessed default), so the
     * layout and the eventual draw agree. Every existing single-line
     * label is completely unaffected - wrap_line_count() returns 1 for
     * anything that already fits, same as before this fix existed. */
    /* REAL FIX 2026-09-03 (direct live report: chat-hai's "Speed: 6s
     * (click to cycle)" row visibly overlapped the transcript below
     * it) - this function only ever handled tag=="text", leaving every
     * <item> (chat-hai's Pause/Speed controls, laid out through this
     * SAME fixed-rows path) permanently pinned at span=1 regardless of
     * real label length. Same real wrap-span logic as the <text> case
     * right below, reused rather than duplicated - the one real
     * difference is an item ALSO carries a "[ ]NN. " nav badge prefix
     * (see draw_elem()'s own badge_label_x) that eats into the same
     * row's real available width; a fixed, generous estimate ("[ ]99. "
     * at this font) is subtracted here so this stays a real, honest
     * upper-bound measurement, not an undercount that reintroduces the
     * exact same overlap for a differently-worded label later. */
    if (g_headless) return 1;   /* no Xft to measure wrapping; the text mirror ignores visual row spans */
    if (c && (strcmp(c->tag, "text") == 0 || strcmp(c->tag, "item") == 0) && c->label[0]) {
        CssStyle tmp_style;
        css_compute_style(&g_sheet, c->tag, c->id, (char (*)[32])(void *)c->classes, c->n_classes, 0, &tmp_style);
        XftFont *font = font_for(&tmp_style);
        int pad = tmp_style.has_padding ? tmp_style.padding : 4;
        int avail_w = w - pad * 2;
        if (strcmp(c->tag, "item") == 0) {
            XGlyphInfo ext;
            XftTextExtentsUtf8(dpy, font, (const FcChar8 *)"[ ]99. ", 7, &ext);
            avail_w -= ext.xOff;
        }
        int lines = wrap_line_count(font, c->label, avail_w);
        int line_h = font->ascent - font->descent > 0 ? font->ascent - font->descent : 12;
        line_h += 4;
        int span = (lines * line_h + ROW_H - 1) / ROW_H;
        return span < 1 ? 1 : span;
    }
    return 1;
}

static void layout_scroll_sprite_grid_row(Elem *row, int x, int y, int w, int h_box, int visible, int *out_lo, int *out_hi) {
    int j, col = 0, line = 0;
    int tile_h = SPRITE_GRID_TILE_H;
    int cols = sprite_grid_cols(w);
    int tile_w = (cols > 0) ? (w / cols) : SPRITE_GRID_TILE_W;
    if (tile_w < SPRITE_GRID_TILE_W) tile_w = SPRITE_GRID_TILE_W;
    (void)h_box;
    row->x = x;
    row->y = visible ? y : -100000;
    row->w = w;
    row->h = sprite_grid_visual_lines(row, w) * tile_h;
    row->nav_index = 0;
    css_compute_style(&g_sheet, row->tag, row->id, row->classes, row->n_classes, 0, &row->style);
    for (j = 0; j < row->n_children; j++) {
        Elem *t = row->children[j];
        if (strcmp(t->tag, "item") != 0 && strcmp(t->tag, "text") != 0) {
            t->x = x; t->y = -100000; t->w = 0; t->h = 0; t->nav_index = 0;
            continue;
        }
        t->w = tile_w;
        t->h = tile_h;
        t->x = x + col * tile_w;
        t->y = visible ? (y + line * tile_h) : -100000;
        css_compute_style(&g_sheet, t->tag, t->id, t->classes, t->n_classes, 0, &t->style);
        if (visible && strcmp(t->tag, "item") == 0) {
            t->nav_index = ++g_n_nav;
            g_nav[g_n_nav - 1] = t;
            if (*out_lo == 0) *out_lo = t->nav_index;
            *out_hi = t->nav_index;
        } else {
            t->nav_index = 0;
        }
        col++;
        if (col >= cols) { col = 0; line++; }
    }
}

static void layout_scroll_region(Elem *container, int x, int y, int w, int h, int *scroll, int *out_lo, int *out_hi) {
    *out_lo = 0; *out_hi = 0;
    if (!container || h <= 0) return;
    int visible_rows = h / ROW_H;
    if (visible_rows < 1) visible_rows = 1;
    int total = 0;
    int inner_w = w;
    for (int i = 0; i < container->n_children; i++) {
        Elem *c = container->children[i];
        if (strcmp(c->tag, "item") == 0 || strcmp(c->tag, "text") == 0 ||
            strcmp(c->tag, "cli_io") == 0 || strcmp(c->tag, "text_area") == 0 ||
            strcmp(c->tag, "canvas") == 0 || strcmp(c->tag, "bar") == 0 ||
            scroll_is_sprite_grid_row(c))
            total += scroll_row_span(c, w);
    }
    int max_scroll = total > visible_rows ? total - visible_rows : 0;
    if (*scroll > max_scroll) *scroll = max_scroll;
    if (*scroll < 0) *scroll = 0;
    /* REAL FIX 2026-09-03 (direct live report: "focus issue... never
     * there before", traced live via a direct diff review - the
     * generic scrollbar's own new up/down arrow buttons only ever
     * shrank the TRACK/THUMB drawn inside generic_sbar_register(); the
     * real content band real rows get positioned into, right below,
     * was never shrunk to match - the arrows sat drawn on TOP of the
     * first/last real visible row instead of in real reserved space of
     * their own, stealing clicks/focus from whatever content used to
     * be there). Decided ONCE, here, using the exact same real gates
     * generic_sbar_register() uses internally (h > ARROW_H*2+12, w wide
     * enough, max_scroll>=1) so the two can never disagree about where
     * the real content band starts/ends - recomputes visible_rows/
     * max_scroll against the REAL reduced band before a single row is
     * positioned, not after. */
    int have_arrows = max_scroll > 0 && w >= GENERIC_SCROLLBAR_W + 8 && h > GENERIC_SBAR_ARROW_H * 2 + 12;
    int content_y = y + (have_arrows ? GENERIC_SBAR_ARROW_H : 0);
    if (have_arrows) {
        visible_rows = (h - GENERIC_SBAR_ARROW_H * 2) / ROW_H;
        if (visible_rows < 1) visible_rows = 1;
        max_scroll = total > visible_rows ? total - visible_rows : 0;
        if (*scroll > max_scroll) *scroll = max_scroll;
    }
    generic_sbar_register(x, y, w, h, scroll, total, visible_rows, max_scroll);
    if (max_scroll > 0 && w > GENERIC_SCROLLBAR_W + 8)
        inner_w = w - GENERIC_SCROLLBAR_W;

    int row = 0;
    for (int i = 0; i < container->n_children; i++) {
        Elem *c = container->children[i];
        int is_grid = scroll_is_sprite_grid_row(c);
        /* a canvas inside a scroll window is a real content row too
         * (V3 video): same clip rules + own span from the receipt */
        if (!is_grid && strcmp(c->tag, "item") != 0 && strcmp(c->tag, "text") != 0 &&
            strcmp(c->tag, "cli_io") != 0 && strcmp(c->tag, "text_area") != 0 &&
            strcmp(c->tag, "canvas") != 0 && strcmp(c->tag, "bar") != 0) continue;
        if (strcmp(c->tag, "canvas") == 0) g_has_canvas = 1; /* live surface -> 30fps tick */
        int span = scroll_row_span(c, inner_w);
        int visible = (row + span > *scroll && row < *scroll + visible_rows);
        if (is_grid) {
            layout_scroll_sprite_grid_row(c, x, content_y + (row - *scroll) * ROW_H, inner_w, span * ROW_H, visible, out_lo, out_hi);
        } else if (visible) {
            c->x = x; c->y = content_y + (row - *scroll) * ROW_H; c->w = inner_w; c->h = span * ROW_H;
            css_compute_style(&g_sheet, c->tag, c->id, c->classes, c->n_classes, 0, &c->style);
            if (strcmp(c->tag, "item") == 0 || strcmp(c->tag, "cli_io") == 0 ||
                strcmp(c->tag, "text_area") == 0 || strcmp(c->tag, "bar") == 0) {
                c->nav_index = ++g_n_nav;
                g_nav[g_n_nav - 1] = c;
                if (*out_lo == 0) *out_lo = c->nav_index;
                *out_hi = c->nav_index;
            } else {
                c->nav_index = 0;
            }
        } else {
            c->x = x; c->y = -100000; c->w = w; c->h = span * ROW_H;
            c->nav_index = 0;
        }
        row += span;
    }
}

/* Real, generic "fixed rows above (or around) a nested <scrolllist>"
 * layout - shared by BOTH <sidebar> and <panel> (2026-09-01, direct
 * instruction: sidebar needed the exact same real capability panel
 * already had, "giving the scroll window to session and chat if they
 * need" - one real function, not two near-duplicate copies). Any
 * direct item/text child of `container` NOT inside its own nested
 * <scrolllist> is a fixed, always-visible row (real controls belong
 * here - they must stay reachable regardless of how long the scrolling
 * content grows); a nested <scrolllist> gets whatever vertical space
 * is left after those fixed rows AND a pinned <cli_io> composer (if
 * one exists as a direct child - real, tag-based, never part of any
 * scroll flow, always the container's own last ROW_H).
 *
 * REAL, NEW 2026-09-02 - a direct <row class="toolbar"> is one fixed
 * ROW_H at y_cursor (item children laid horizontally, equal widths,
 * each with a nav_index). Data-driven: any window can emit it.
 * pal-grid-row is a different mechanism and is skipped here.
 * A <cli_io class="top"> pins at y_cursor like a fixed row (address
 * bar); unclassed cli_io stays the bottom composer so chat/open-hai
 * is unchanged. composer_h for list_h is BOTTOM cli_io only, so a
 * top field is not subtracted twice. class "from-top" is unrelated
 * (scrolllist starts at 0) and must not be used for this pin. */
static void layout_toolbar_row(Elem *row, int x, int y, int w) {
    int j, n_items = 0, col = 0, iw;
    row->x = x; row->y = y; row->w = w; row->h = ROW_H; row->nav_index = 0;
    css_compute_style(&g_sheet, row->tag, row->id, row->classes, row->n_classes, 0, &row->style);
    for (j = 0; j < row->n_children; j++)
        if (strcmp(row->children[j]->tag, "item") == 0) n_items++;
    iw = n_items > 0 ? w / n_items : w;
    for (j = 0; j < row->n_children; j++) {
        Elem *t = row->children[j];
        if (strcmp(t->tag, "item") != 0) {
            t->x = x; t->y = -100000; t->w = 0; t->h = 0; t->nav_index = 0;
            continue;
        }
        t->x = x + col * iw;
        t->y = y;
        t->w = iw;
        t->h = ROW_H;
        css_compute_style(&g_sheet, t->tag, t->id, t->classes, t->n_classes, 0, &t->style);
        t->nav_index = ++g_n_nav;
        g_nav[g_n_nav - 1] = t;
        col++;
    }
}

static void layout_fixed_rows_and_scrolllist(Elem *container, int x, int y, int w, int h, int *scroll, int *out_lo, int *out_hi) {
    int composer_rows = 0;
    Elem *scrolllist = NULL;
    for (int i = 0; i < container->n_children; i++) {
        Elem *c = container->children[i];
        /* REAL, NEW 2026-09-01 (direct instruction: "build word-wrap/
         * multi-line/emoji into the generic cli_io first") - a real
         * <cli_io rows="N"/> reserves N real text rows instead of the
         * old, always-1-row assumption (rows defaults to 0/unset,
         * meaning "1" - every existing single-line consumer, open-hai's
         * own real composer included, is completely unaffected).
         * REAL FIX 2026-09-02 - only BOTTOM (unclassed) cli_io reserves
         * the trailing strip. class=top is laid at y_cursor below. */
        if ((strcmp(c->tag, "cli_io") == 0 || strcmp(c->tag, "text_area") == 0) && !elem_has_class(c, "top"))
            composer_rows = c->rows > 0 ? c->rows : 1;
        if (strcmp(c->tag, "scrolllist") == 0) scrolllist = c;
    }
    int composer_h = composer_rows * ROW_H;
    int y_cursor = y;
    for (int i = 0; i < container->n_children; i++) {
        Elem *c = container->children[i];
        /* REAL, NEW 2026-09-03 - a dropdown-child never consumes fixed-
         * row space of its own; it's positioned as a real overlay,
         * relative to its trigger, in the dedicated pass right after
         * this loop (which runs once every OTHER row already has its
         * own real, final x/y/h - a trigger living inside a toolbar
         * row, not just a bare <item>, needs that same guarantee). */
        if (elem_has_class(c, "dropdown-child")) continue;
        if (strcmp(c->tag, "item") == 0 || strcmp(c->tag, "text") == 0 || strcmp(c->tag, "bar") == 0) {
            /* REAL FIX 2026-09-03 (direct live report: co-lab-hai's own
             * PENDING banner still didn't wrap after scroll_row_span()'s
             * own fix - root cause: that banner is a direct <text> child
             * of <panel>, laid out HERE, in the fixed-rows path, not
             * inside the nested <scrolllist> scroll_row_span() actually
             * covers. Same real fix, same helper, applied here too - a
             * fixed row can need more than one ROW_H exactly the same
             * way a scrolled one can. */
            int span = scroll_row_span(c, w);
            c->x = x; c->y = y_cursor; c->w = w; c->h = span * ROW_H;
            css_compute_style(&g_sheet, c->tag, c->id, c->classes, c->n_classes, 0, &c->style);
            if (strcmp(c->tag, "item") == 0 || strcmp(c->tag, "bar") == 0) { c->nav_index = ++g_n_nav; g_nav[g_n_nav - 1] = c; }
            else c->nav_index = 0;
            y_cursor += span * ROW_H;
        } else if (strcmp(c->tag, "row") == 0 && elem_has_class(c, "toolbar") && !elem_has_class(c, "pal-grid-row")) {
            layout_toolbar_row(c, x, y_cursor, w);
            y_cursor += ROW_H;
        } else if (strcmp(c->tag, "cli_io") == 0 || strcmp(c->tag, "text_area") == 0) {
            int this_h = (c->rows > 0 ? c->rows : 1) * ROW_H;
            if (elem_has_class(c, "top")) {
                c->x = x; c->y = y_cursor; c->w = w; c->h = this_h;
                y_cursor += this_h;
            } else if (strcmp(c->tag, "text_area") == 0 && !scrolllist) {
                /* REAL FIX 2026-09-11, direct live report ("5/highlight
                 * is way too high... shouldn't be higher than line
                 * counter or tabbar but it is") - the glue-to-bottom-
                 * strip-sized-by-rows math right below is built for
                 * chat's shape (a <scrolllist> of history ABOVE a small
                 * rows="3" composer - chat-hai's real layout), correct
                 * there. text-edit-hq's panel has no <scrolllist> at
                 * all - its rows="20" <text_area> IS the entire panel
                 * content, meant to FILL it, not glue a fixed
                 * composer_h=(20*ROW_H) strip to the bottom, which once
                 * that computed height exceeded the real panel height
                 * put c->y ABOVE y itself (above the tabbar/gutter -
                 * exactly the report). Deliberately scoped to tag ==
                 * "text_area" only (not cli_io) - direct instruction
                 * 2026-09-11 ("roll back anything u change that wasn't
                 * about text area") after the same-shaped cli_io fix
                 * broke open-hai; a bare <text_area> with no sibling
                 * <scrolllist> has nothing to leave room for above it,
                 * so filling the whole container is safe and correct
                 * for it specifically. */
                c->x = x; c->y = y; c->w = w; c->h = h;
            } else {
                c->x = x; c->y = y + h - composer_h; c->w = w; c->h = composer_h;
            }
            css_compute_style(&g_sheet, c->tag, c->id, c->classes, c->n_classes, 0, &c->style);
            c->nav_index = ++g_n_nav; g_nav[g_n_nav - 1] = c;
        } else if (strcmp(c->tag, "grid") == 0) {
            /* REAL, NEW 2026-09-05 (GRID-ELEMENT-DESIGN.md). Always a
             * fixed, always-visible row (like cli_io/text_area's own
             * class="top" case) - a grid doesn't glue to the bottom the
             * way a single composer field does. rows= reused verbatim
             * from cli_io/text_area's own attribute (defaults to 8 real
             * visible rows if unset, an arbitrary but reasonable v1
             * placeholder height).
             * REAL FIX 2026-09-05, direct live report ("the orange
             * highligh for grid goes offscreen. nothing should go
             * offscreen") - this height MUST exactly match what
             * khtpm_draw_core.c's own grid branch actually consumes
             * (its STATUS_H_PX + (rows+1)*CELL_H_PX), or the generic
             * nav-focus rectangle (drawn earlier in draw_elem(), sized
             * from THIS c->h) ends up taller than the real drawn table,
             * leaving a floating, disconnected-looking orange line past
             * the last real row. This was using the unrelated generic
             * ROW_H (24px, ~every other element's row height) instead -
             * a real, deliberately duplicated copy of draw_core.c's own
             * two constants (same reason grid_col_to_letters() is also
             * duplicated there: the two files can't share a single
             * #define across their real translation-unit split at this
             * house's own build boundary). Keep these three numbers
             * (18, 22, and the +1 for the header row) in exact sync
             * with khtpm_draw_core.c's STATUS_H_PX/CELL_H_PX if either
             * ever changes. */
            enum { GRID_STATUS_H_PX = 18, GRID_CELL_H_PX = 22 };
            int rows = c->rows > 0 ? c->rows : 8;
            int this_h = GRID_STATUS_H_PX + (rows + 1) * GRID_CELL_H_PX;
            c->x = x; c->y = y_cursor; c->w = w; c->h = this_h;
            css_compute_style(&g_sheet, c->tag, c->id, c->classes, c->n_classes, 0, &c->style);
            c->nav_index = ++g_n_nav; g_nav[g_n_nav - 1] = c;
            y_cursor += this_h;
        }
        /* scrolllist itself is positioned in its own real pass below,
         * once the fixed-row total (y_cursor's own final advance) is
         * known - skipped here on purpose. */
    }
    /* REAL, NEW 2026-09-03 - real, generic dropdown-child overlay pass.
     * Runs after every other row/toolbar in this container has its own
     * final x/y/h (a trigger can live inside a toolbar row, laid out
     * above, not just as a bare <item>) - find_by_id() searches the
     * WHOLE window, not just this container, so a trigger elsewhere on
     * the page still works ("for all layouts", not sidebar/panel-
     * specific). Multiple dropdown-children sharing one target_id
     * stack vertically below the trigger, in tree order. Closed (not
     * the active scope): positioned off-screen, same -100000 sentinel
     * this file's own hidden-element convention already uses
     * everywhere else - real, not just invisible-but-still-clickable. */
    {
        char last_target[64] = "";
        int stack_n = 0;
        for (int i = 0; i < container->n_children; i++) {
            Elem *c = container->children[i];
            if (!elem_has_class(c, "dropdown-child")) continue;
            Elem *trigger = c->target_id[0] ? find_by_id(g_window, c->target_id) : NULL;
            int open = trigger && (g_default_active_scope_root == trigger ||
                (g_default_active_scope_id[0] && trigger->id[0] &&
                 strcmp(g_default_active_scope_id, trigger->id) == 0));
            if (strcmp(last_target, c->target_id) != 0) { stack_n = 0; snprintf(last_target, sizeof(last_target), "%s", c->target_id); }
            css_compute_style(&g_sheet, c->tag, c->id, c->classes, c->n_classes, 0, &c->style);
            if (open) {
                int dw = trigger->w > 0 ? trigger->w : w;
                c->x = trigger->x; c->y = trigger->y + trigger->h + stack_n * ROW_H; c->w = dw; c->h = ROW_H;
                c->nav_index = ++g_n_nav; g_nav[g_n_nav - 1] = c;
                if (!g_dock_drop_lo) g_dock_drop_lo = c->nav_index;
                g_dock_drop_hi = c->nav_index;
            } else {
                c->x = x; c->y = -100000; c->w = 0; c->h = 0; c->nav_index = 0;
            }
            stack_n++;
        }
    }
    if (scrolllist) {
        int list_h = (y + h) - y_cursor - composer_h;
        scrolllist->x = x; scrolllist->y = y_cursor; scrolllist->w = w; scrolllist->h = list_h > 0 ? list_h : 0;
        css_compute_style(&g_sheet, scrolllist->tag, scrolllist->id, scrolllist->classes, scrolllist->n_classes, 0, &scrolllist->style);
        layout_scroll_region(scrolllist, x, y_cursor, w, list_h, scroll, out_lo, out_hi);
    } else if (out_lo && out_hi) {
        /* No nested scrolllist - container has no scrollable region of
         * its own (e.g. sidebar with no sessions yet, or a page that
         * only ever needed fixed rows). [0,0] matches layout_scroll_
         * region()'s own "nothing to scroll" convention. */
        *out_lo = 0; *out_hi = 0;
    }
}

/* Real, generic dual-region layout - fires only when `page` declares
 * BOTH a <sidebar> and a <panel> (see this section's own header
 * comment for why a page with neither is completely unaffected).
 * <sidebar>'s own children are ALWAYS the left scroll region.
 * <panel>'s own DIRECT item/text children (NOT inside its own
 * <scrolllist>, if any) flow as fixed, always-visible rows top-down
 * (real controls like "New session"/model-cycle/sound-toggle belong
 * here - they must stay reachable without scrolling past a long
 * transcript); a nested <scrolllist> gets whatever vertical space is
 * left after those fixed rows and a pinned <cli_io> composer (a real
 * generic composer field is NEVER part of any scroll flow - always
 * the last ROW_H of its own parent panel, real convention any future
 * consumer can rely on, tag-based, not open-hai-specific). Returns 1
 * if it actually ran (caller should skip the old flat-list path),
 * 0 if `page` has no sidebar+panel pair (old path still owns it). */
/* MILESTONE A (PCHQ-ENTITY-MENU-AND-TASKBAR-DESIGN.md §6a) - lay out a
 * <canvas> child of a sidebar+panel region so the board window can be a
 * normal HQ window (chrome, dropdowns, taskbar entry, minimize) with the
 * 2D/3D view as an in-panel canvas instead of the flat has_canvas
 * layout. The canvas fills the region box. Sets g_has_canvas (drives
 * the live-feed ~30fps tick), wires the projector's canvas_raw sprite,
 * and writes the producer-size handoff (#.desktop/pchq_board_view.txt)
 * so bv_render_3d renders the exact pixel box kh_draw_canvas blits 1:1.
 * The shared draw_elem() already dispatches <canvas> -> kh_draw_canvas
 * regardless of layout, so no paint-side change is needed. Returns 1 if
 * a canvas was found and placed. */
static int kh_layout_canvas_in_region(Elem *region, int rx, int ry, int rw, int rh) {
    Elem *cv = NULL;
    for (int i = 0; i < region->n_children; i++)
        if (strcmp(region->children[i]->tag, "canvas") == 0) { cv = region->children[i]; break; }
    if (!cv) return 0;

    css_compute_style(&g_sheet, cv->tag, cv->id, cv->classes, cv->n_classes, 0, &cv->style);
    { const char *cr = kh_get_var("canvas_raw");
      if (cr && cr[0]) snprintf(cv->sprite, sizeof(cv->sprite), "%s", cr); }

    int pad = 6;
    cv->x = rx + pad; cv->y = ry + pad;
    cv->w = rw - 2 * pad; cv->h = rh - 2 * pad;
    if (cv->w < 64) cv->w = 64;
    if (cv->h < 64) cv->h = 64;
    cv->nav_index = 0;
    g_has_canvas = 1;

    char vsz[PATH_BUF];
    snprintf(vsz, sizeof(vsz), "%s/#.desktop/pchq_board_view.txt", g_house_root);
    FILE *vf = fopen(vsz, "w");
    if (vf) { fprintf(vf, "%d %d\n", cv->w, cv->h); fclose(vf); }
    return 1;
}

static int layout_sidebar_panel(Elem *page) {
    Elem *sidebar = find_by_tag(page, "sidebar");
    /* The main content panel is a DIRECT child of <page> and is not a
     * dropdown overlay. find_by_tag()'s depth-first first-match would
     * otherwise return a `<panel class="dropdown-child">` nested inside
     * the sidebar (milestone A: the board's Desk/Menu dropdowns) - so
     * pick deliberately, then fall back to the old behavior. */
    Elem *panel = NULL;
    for (int i = 0; i < page->n_children; i++) {
        Elem *c = page->children[i];
        if (strcmp(c->tag, "panel") == 0 && !elem_has_class(c, "dropdown-child")) {
            panel = c; break;
        }
    }
    if (!panel) panel = find_by_tag(page, "panel");
    if (!sidebar || !panel) return 0;
    generic_sbar_reset();

    css_compute_style(&g_sheet, g_window->tag, g_window->id[0] ? g_window->id : NULL,
                      g_window->classes, g_window->n_classes, 0, &g_window->style);
    css_compute_style(&g_sheet, sidebar->tag, sidebar->id, sidebar->classes, sidebar->n_classes, 0, &sidebar->style);
    css_compute_style(&g_sheet, panel->tag, panel->id, panel->classes, panel->n_classes, 0, &panel->style);

    if (g_default_is_fullscreen) {
        /* work area only, and stop short of every edge (see the
         * WM_FS_MARGIN_* declaration comment) */
        int sw = kh_screen_w(), sh = kh_screen_h();
        g_win_w = sw - WM_FS_MARGIN_X - WM_FS_MARGIN_RIGHT;
        g_win_h = sh - WM_MANAGED_DRAG_MIN_Y - WM_MANAGED_BOTTOM_RESERVE - WM_FS_MARGIN_BOTTOM;
        if (g_win_w > sw * WM_FS_MAX_PCT / 100) g_win_w = sw * WM_FS_MAX_PCT / 100;
        if (g_win_h > sh * WM_FS_MAX_PCT / 100) g_win_h = sh * WM_FS_MAX_PCT / 100;
        if (g_win_w < 320) g_win_w = sw - WM_FS_MARGIN_X;
        if (g_win_h < 240) g_win_h = sh - WM_MANAGED_DRAG_MIN_Y;
        if (g_win_h < 240) g_win_h = sh;
    } else if (g_user_resizable && g_win_w > 0 && g_win_h > 0) {
        /* a user-resizable window OWNS its own size after main()'s
         * initial value - the ⌟ drag updates g_win_w/g_win_h and this
         * relayout must NOT snap it back to the CSS/default ("resize
         * wont grow at all" report 2026-09-10). */
    } else {
        g_win_w = g_window->style.has_width ? g_window->style.width : DEFAULT_WIN_W;
        g_win_h = g_window->style.has_height ? g_window->style.height : DEFAULT_WIN_H;
    }
    g_window->w = g_win_w;
    g_window->h = g_win_h;

    /* REAL, NEW 2026-09-03 (direct live report: "X always hangs a bit
     * off screen on new hq-x11 window project creations") - root cause:
     * g_win_x/g_win_y default to a flat, content-blind 300,300 (see
     * their own declaration) whenever a brand-new app has no saved
     * position yet in hq_ui.pdl; g_win_w/g_win_h just above is real,
     * content-driven, and can genuinely exceed what 300+w leaves on a
     * real monitor - the chrome "X"/"!" pair (right after this
     * function, computed FROM g_win_w) then sits past the actual
     * screen edge, unreachable. Generic, real fix here rather than a
     * per-app tweak: clamp so the window's own real right/bottom edge
     * never lands past the real display, same real DisplayWidth/
     * DisplayHeight this same function already uses for fullscreen
     * above - every future new app gets this for free, not just the
     * one this was found on. Only pulls IN from an off-screen edge,
     * never re-centers a window the human deliberately dragged
     * partway off - a real position clamp, not a real recenter. */
    if (!g_default_is_fullscreen) {
        int sw = kh_screen_w(), sh = kh_screen_h();
        /* REAL, NEW 2026-09-04 (live report: chat-hai's session-list
         * scrollbar sat flush against / past the screen's right edge,
         * unusable). A window WIDER than the screen leaves g_win_x
         * negative below and the right strip (where the scrollbar
         * lives) permanently off-screen - shrink it to fit first. Then
         * keep a small margin off the right/bottom edges so a
         * right-edge affordance (scrollbar, chrome X) is never flush
         * against the physical screen edge. */
        const int EDGE_MARGIN = 14;
        if (g_win_w > sw - EDGE_MARGIN) g_win_w = sw - EDGE_MARGIN;
        if (g_win_h > sh - EDGE_MARGIN) g_win_h = sh - EDGE_MARGIN;
        g_window->w = g_win_w; g_window->h = g_win_h;
        if (g_win_x + g_win_w > sw - EDGE_MARGIN) g_win_x = sw - EDGE_MARGIN - g_win_w;
        if (g_win_y + g_win_h > sh - EDGE_MARGIN) g_win_y = sh - EDGE_MARGIN - g_win_h;
        if (g_win_x < 0) g_win_x = 0;
        if (g_win_y < 0) g_win_y = 0;
    }

    /* generic <tabbar> (spirit of db-hq's dbhq_layout_pass tab strip):
     * a horizontal row of <tab> just under the chrome; the sidebar +
     * panel start below it. Each <tab> is nav-numbered FIRST (before
     * sidebar/panel), like the old db-hq order. The active tab (id ==
     * g_default_active_tab_id) gets an "active" class so app CSS can
     * style it (.tab / .tab.active - see db-hq's dashboard.css). */
    /* Lay out EVERY <tabbar> child of the page, stacked (view-mode row
     * on top, page row under it - EVENTS-HQ-XHTPM-PORT.md §5). Each is
     * scaled(28) tall; content starts below the last one. Within one
     * tabbar, if a sibling tab is the globally-clicked tab
     * (g_default_active_tab_id) that one wins; if the clicked tab
     * belongs to a DIFFERENT tabbar, this group falls back to its
     * template / projector-supplied class="active". */
    int tabbar_h = 0;
    {
        int row_h = scaled(28);
        int th = row_h - scaled(4);
        for (int ci = 0; ci < page->n_children; ci++) {
            Elem *tabbar = page->children[ci];
            if (strcmp(tabbar->tag, "tabbar") != 0 || tabbar->n_children <= 0) continue;
            int group_has_click = 0;
            if (g_default_active_tab_id[0])
                for (int i = 0; i < tabbar->n_children; i++) {
                    Elem *t = tabbar->children[i];
                    if (strcmp(t->tag, "tab") == 0 && t->id[0] &&
                        strcmp(t->id, g_default_active_tab_id) == 0) { group_has_click = 1; break; }
                }
            int ty = CHROME_H + tabbar_h + scaled(2);
            int tx = scaled(6);
            for (int i = 0; i < tabbar->n_children; i++) {
                Elem *tab = tabbar->children[i];
                if (strcmp(tab->tag, "tab") != 0) continue;
                int tmpl_active = 0;
                for (int k = 0; k < tab->n_classes; k++)
                    if (strcmp(tab->classes[k], "active") == 0) tmpl_active = 1;
                tab->active = group_has_click
                    ? (tab->id[0] && strcmp(tab->id, g_default_active_tab_id) == 0)
                    : tmpl_active;
                int has_active_cls = tmpl_active;
                if (tab->active && !has_active_cls && tab->n_classes < CSS_MAX_CLASSES)
                    snprintf(tab->classes[tab->n_classes++], sizeof(tab->classes[0]), "active");
                else if (!tab->active && has_active_cls) {
                    int w2 = 0;
                    for (int k = 0; k < tab->n_classes; k++)
                        if (strcmp(tab->classes[k], "active") != 0)
                            snprintf(tab->classes[w2++], sizeof(tab->classes[0]), "%s", tab->classes[k]);
                    tab->n_classes = w2;
                }
                css_compute_style(&g_sheet, tab->tag, tab->id, tab->classes, tab->n_classes, 0, &tab->style);
                /* [ ]N. badge is ~46px (playbook); 34 left labels clipped. */
                int tw = scaled(52);
                if (font_ui && tab->label[0]) {
                    XGlyphInfo gi;
                    XftTextExtentsUtf8(dpy, font_ui, (const FcChar8 *)tab->label, (int)strlen(tab->label), &gi);
                    tw += gi.xOff;
                }
                tab->x = tx; tab->y = ty; tab->w = tw; tab->h = th;
                tab->nav_index = ++g_n_nav; g_nav[g_n_nav - 1] = tab;
                tx += tw + scaled(3);
            }
            if (tx + scaled(6) > g_win_w) { g_win_w = tx + scaled(6); g_window->w = g_win_w; }
            tabbar->x = 0; tabbar->y = CHROME_H + tabbar_h; tabbar->w = g_win_w; tabbar->h = row_h;
            css_compute_style(&g_sheet, tabbar->tag, tabbar->id, tabbar->classes, tabbar->n_classes, 0, &tabbar->style);
            tabbar_h += row_h;
        }
    }
    int content_top = CHROME_H + tabbar_h;

    /* Region boxes. Two ways, HTML-shaped:
     *  - default: the classic fixed sidebar + one panel takes the rest.
     *  - if <page style="display:flex; flex-direction:row"> the shared
     *    flexbox engine (css_layout_pass, khtpm_render_core.c) sizes
     *    every region child (sidebar + N panels) from its own
     *    width / flex-grow - exactly like a CSS flex row. This is how a
     *    3-column (recipes | bench | inventory) window is authored, no
     *    per-app renderer code. */
    css_compute_style(&g_sheet, page->tag, page->id, page->classes, page->n_classes, 0, &page->style);
    int page_is_flex = page->style.has_display && page->style.display_flex;
    if (page_is_flex) {
        for (int i = 0; i < page->n_children; i++) {
            Elem *c = page->children[i];
            css_compute_style(&g_sheet, c->tag, c->id, c->classes, c->n_classes, 0, &c->style);
        }
        css_layout_pass(page, 0, content_top, g_win_w, g_win_h - content_top);
    } else {
        int sidebar_w = SIDEBAR_W;
        if (sidebar->style.has_width && !sidebar->style.width_is_pct) sidebar_w = sidebar->style.width;
        sidebar->x = 0; sidebar->y = content_top; sidebar->w = sidebar_w; sidebar->h = g_win_h - content_top;
        panel->x = sidebar_w; panel->y = content_top; panel->w = g_win_w - sidebar_w; panel->h = g_win_h - content_top;
    }

    /* MILESTONE B/C (PCHQ-ENTITY-MENU-AND-TASKBAR-DESIGN.md §6a) - a
     * generic bottom-dock <footer>: a horizontal strip along the
     * window's bottom edge, laid out AFTER sidebar/panel (their heights
     * trimmed so nothing paints under it). Cells WRAP into rows; a
     * synthesized +/- pager (like the dock strip's) shows/hides deeper
     * rows via FOOTER_ROWS:+1/-1. g_footer_vis_rows of g_footer_total_
     * rows are visible; off-page cells park at -100000. */
    g_footer_more_elem.w = g_footer_less_elem.w = 0;
    {
        Elem *footer = NULL;
        for (int i = 0; i < page->n_children; i++)
            if (strcmp(page->children[i]->tag, "footer") == 0) { footer = page->children[i]; break; }
        if (footer) {
            css_compute_style(&g_sheet, footer->tag, footer->id, footer->classes, footer->n_classes, 0, &footer->style);
            int row_h = footer->style.has_height ? footer->style.height : scaled(24);
            if (row_h < 12) row_h = 12;
            int pad = scaled(6);
            int pager_w = scaled(46);   /* room for +/- at the right edge */
            /* always stop short of the ⌟ drag-resize grip in the
             * bottom-right corner (direct instruction) */
            int grip = g_user_resizable ? KH_RESIZE_GRIP + scaled(4) : 0;
            int right_edge = g_win_w - pad - grip;

            /* pass 1: assign each cell a (row, x) by wrapping */
            int row = 0, fx = pad, max_row = 0;
            for (int i = 0; i < footer->n_children; i++) {
                Elem *fi = footer->children[i];
                if (strcmp(fi->tag, "item") != 0 && strcmp(fi->tag, "text") != 0) continue;
                css_compute_style(&g_sheet, fi->tag, fi->id, fi->classes, fi->n_classes, 0, &fi->style);
                int fw = fi->style.has_width ? fi->style.width
                       : (kh_measure_text_px(&fi->style, fi->label) + scaled(18));
                if (fx > pad && fx + fw > right_edge - pager_w) { row++; fx = pad; }
                fi->x = fx; fi->w = fw; fi->h = row_h - scaled(4);
                fi->y = row;                       /* stash the row index in y for pass 2 */
                if (row > max_row) max_row = row;
                fx += fw + scaled(4);
            }
            g_footer_total_rows = max_row + 1;
            if (g_footer_vis_rows < 1) g_footer_vis_rows = 1;
            if (g_footer_vis_rows > g_footer_total_rows) g_footer_vis_rows = g_footer_total_rows;

            int footer_h = g_footer_vis_rows * row_h + scaled(4);
            footer->x = 0; footer->y = g_win_h - footer_h;
            /* the footer NEVER climbs into the header/toolbar band -
             * they don't compete. Keep a sliver of panel between them. */
            int min_footer_y = content_top + scaled(6);
            if (footer->y < min_footer_y) {
                footer->y = min_footer_y;
                footer_h = g_win_h - footer->y;
                if (footer_h < row_h) footer_h = row_h;
            }
            footer->w = g_win_w; footer->h = footer_h;
            /* sidebar/panel stop exactly at the footer top */
            panel->h  = footer->y - panel->y;   if (panel->h  < 0) panel->h  = 0;
            sidebar->h = footer->y - sidebar->y; if (sidebar->h < 0) sidebar->h = 0;

            /* pass 2: real y from the stashed row, park off-page rows */
            for (int i = 0; i < footer->n_children; i++) {
                Elem *fi = footer->children[i];
                if (strcmp(fi->tag, "item") != 0 && strcmp(fi->tag, "text") != 0) continue;
                int r = fi->y;
                if (r >= g_footer_vis_rows) {
                    fi->x = 0; fi->y = -100000; fi->w = 0; fi->h = 0; fi->nav_index = 0;
                    continue;
                }
                fi->y = footer->y + scaled(2) + r * row_h;
                if (strcmp(fi->tag, "item") == 0 && (fi->onclick[0] || fi->label[0])) {
                    fi->nav_index = ++g_n_nav; g_nav[g_n_nav - 1] = fi;
                } else {
                    fi->nav_index = 0;
                }
            }

            /* synthesize the +/- pager on the footer's first row when
             * there is more than one row of cells. */
            if (g_footer_total_rows > 1) {
                int ay = footer->y + scaled(2);
                int aw = scaled(20), ah = row_h - scaled(4);
                memset(&g_footer_less_elem, 0, sizeof(g_footer_less_elem));
                snprintf(g_footer_less_elem.tag, sizeof(g_footer_less_elem.tag), "item");
                snprintf(g_footer_less_elem.id, sizeof(g_footer_less_elem.id), "footer-rows-less");
                snprintf(g_footer_less_elem.label, sizeof(g_footer_less_elem.label), "-");
                snprintf(g_footer_less_elem.onclick, sizeof(g_footer_less_elem.onclick), "FOOTER_ROWS:-1");
                g_footer_less_elem.x = right_edge - 2 * aw - scaled(3);
                g_footer_less_elem.y = ay; g_footer_less_elem.w = aw; g_footer_less_elem.h = ah;
                css_compute_style(&g_sheet, "item", "footer-rows-less", NULL, 0, 0, &g_footer_less_elem.style);
                g_footer_less_elem.nav_index = ++g_n_nav; g_nav[g_n_nav - 1] = &g_footer_less_elem;

                memset(&g_footer_more_elem, 0, sizeof(g_footer_more_elem));
                snprintf(g_footer_more_elem.tag, sizeof(g_footer_more_elem.tag), "item");
                snprintf(g_footer_more_elem.id, sizeof(g_footer_more_elem.id), "footer-rows-more");
                snprintf(g_footer_more_elem.label, sizeof(g_footer_more_elem.label), "+");
                snprintf(g_footer_more_elem.onclick, sizeof(g_footer_more_elem.onclick), "FOOTER_ROWS:+1");
                g_footer_more_elem.x = right_edge - aw;
                g_footer_more_elem.y = ay; g_footer_more_elem.w = aw; g_footer_more_elem.h = ah;
                css_compute_style(&g_sheet, "item", "footer-rows-more", NULL, 0, 0, &g_footer_more_elem.style);
                g_footer_more_elem.nav_index = ++g_n_nav; g_nav[g_n_nav - 1] = &g_footer_more_elem;
            }
        }
    }

    /* REAL FIX 2026-09-21 (piececraft-hq board bug, BUG-LOG.md
     * "renders only a thin .main tab, no board content" - root-caused
     * by direct code read, no piececraft-specific change needed here):
     * a bare `<text show="${cond}">` direct child of `<page>` (a
     * fallback/hint message that isn't inside sidebar/panel/footer/
     * tabbar - e.g. pchq-board.xhtpm's own "No live board-viewer
     * session..." line) was never laid out anywhere in this function -
     * only a <footer>'s OWN <text> children get real x/y/w/h above.
     * draw_elem() (khtpm_draw_core.c) already skips any element with
     * w<=0||h<=0 - painting is fully generic, gated only on real
     * geometry existing, so the only gap was this missing layout pass.
     * A `show=` that evaluates false already drops the element from
     * the tree entirely at PARSE time (see the `drop_elem` handling
     * above `apply_attr()`), so every text reaching here is meant to
     * be visible right now - no visibility check needed, only geometry.
     * Placed inside the panel's own content area (single line, near
     * the top, padded) so it reads as "why the panel is empty/whatever
     * it's explaining" rather than floating over the sidebar or footer. */
    for (int i = 0; i < page->n_children; i++) {
        Elem *t = page->children[i];
        if (strcmp(t->tag, "text") != 0) continue;
        css_compute_style(&g_sheet, t->tag, t->id, t->classes, t->n_classes, 0, &t->style);
        int pad = scaled(10);
        int th = t->style.has_height ? t->style.height : scaled(20);
        t->x = panel->x + pad;
        t->y = panel->y + pad;
        t->w = panel->w - 2 * pad; if (t->w < 0) t->w = 0;
        t->h = th;
        t->nav_index = 0; /* a hint, not a focusable/clickable item */
    }

    /* REAL, NEW 2026-08-31 (live report: "no separation elements") -
     * a real visible divider between the two regions belongs in CSS
     * (entity_menu_default.css's own generic `sidebar`/`cli_io` rules),
     * NOT set programmatically here - this default/popup mode's own
     * real content draw round-trips every frame through a text frame
     * file (kh_serialize_frame_subtree()/kh_paint_frame_line(),
     * see reparse_chtpm_if_changed()'s own sibling fix for the same
     * class of bug with input_buffer/target_id) which does NOT carry
     * style fields at all - the paint side always recomputes style
     * fresh from CSS (tag/id/classes), so anything set directly on
     * these live Elem objects would be silently discarded before ever
     * reaching the screen. Found live: this exact code used to set
     * has_bg_color/has_border_color right here and never once painted -
     * see entity_menu_default.css's own new `sidebar { ... }` rule for
     * the real fix (kh_paint_frame_line()'s own temp Elem calls
     * css_compute_style() itself, using the SAME real g_sheet, so a
     * real CSS rule DOES survive the round trip - only a programmatic
     * style assignment made directly on the live tree does not). */

    /* REAL, NEW 2026-09-01 (direct instruction: "re add [the controls]
     * by making another panel in sessions and giving the scroll window
     * to session and chat if they need" - after they scrolled out of
     * view once the real session list grew past sidebar's own visible
     * height) - <sidebar> now gets the SAME real "fixed rows above a
     * nested <scrolllist>" capability <panel> already had, via one
     * shared helper (layout_fixed_rows_and_scrolllist() below) instead
     * of two near-duplicate copies. Sidebar's own direct item/text
     * children (New/Model/Sound) stay fixed and always visible; a
     * nested <scrolllist> (the real session list) gets whatever's left
     * and scrolls independently - same real per-region scroll variable
     * as before, just no longer required to be sidebar's OWN direct
     * children. */
    /* A region that opts into flexbox (display:flex, incl.
     * flex-wrap:wrap for a tile grid / periodic table) is laid out by
     * the shared HTML flex engine; css_layout_pass assigns no nav, so
     * number its items after. Otherwise the classic vertical
     * fixed-rows + scroll pass. */
    if (sidebar->style.has_display && sidebar->style.display_flex) {
        /* Flex-wrap tile grid (e.g. the periodic-table element picker).
         * The shared HTML flex engine (css_layout_pass) lays every tile
         * out fresh from sidebar->y every frame; it assigns no nav.
         * SCROLL exactly the way every other region in this file does
         * (layout_scroll_region / the swatch-grid path): do NOT
         * translate the whole subtree - off-fold rows then paint up
         * into the pinned chrome band, nothing clips a region to its
         * box on the paint side - instead HIDE off-fold tiles at
         * y=-100000 and nav-number only the visible ones. Scroll input
         * is the generic Page_Up/Down + the generic_sbar ^/v arrows +
         * wheel; no bespoke key handling. See HOUSE_CODE_PITFALLS.md #14. */
        zero_nav_subtree(sidebar);
        kh_css_deep(sidebar);
        css_layout_pass(sidebar, sidebar->x, sidebar->y, sidebar->w, sidebar->h);

        int grid_bottom = 0, first_h = 0, first_y = sidebar->y;
        for (int i = 0; i < sidebar->n_children; i++) {
            Elem *c = sidebar->children[i];
            if (strcmp(c->tag, "item") != 0) continue;
            if (!first_h) { first_h = c->h; first_y = c->y; }
            if (c->y + c->h > grid_bottom) grid_bottom = c->y + c->h;
        }
        int cgap = sidebar->style.has_gap ? sidebar->style.gap : 0;
        int cpad = sidebar->style.has_padding ? sidebar->style.padding : 0;
        int row_h  = (first_h > 0 ? first_h : 40) + cgap;
        int grid_h = grid_bottom - first_y + cpad;
        int total_rows   = row_h > 0 ? (grid_h + row_h - 1) / row_h : 1;
        int visible_rows = row_h > 0 ? sidebar->h / row_h : 1;
        if (visible_rows < 1) visible_rows = 1;
        int max_scroll   = total_rows > visible_rows ? total_rows - visible_rows : 0;
        if (g_default_sidebar_scroll < 0) g_default_sidebar_scroll = 0;
        if (g_default_sidebar_scroll > max_scroll) g_default_sidebar_scroll = max_scroll;

        int shift = g_default_sidebar_scroll * row_h;
        int lo = g_n_nav + 1;
        for (int i = 0; i < sidebar->n_children; i++) {
            Elem *c = sidebar->children[i];
            if (strcmp(c->tag, "item") != 0) {
                if (shift) kh_shift_subtree(c, -shift);   /* headers etc. scroll with the grid */
                continue;
            }
            int r = row_h > 0 ? (c->y - first_y + row_h / 2) / row_h : 0;
            if (r < g_default_sidebar_scroll || r >= g_default_sidebar_scroll + visible_rows) {
                kh_shift_subtree(c, -100000 - c->y);      /* park the whole tile subtree off-screen */
                c->nav_index = 0;
            } else {
                if (shift) kh_shift_subtree(c, -shift);
                if (c->onclick[0] || c->label[0]) {
                    c->nav_index = ++g_n_nav;
                    g_nav[g_n_nav - 1] = c;
                } else {
                    c->nav_index = 0;
                }
            }
        }
        g_default_sidebar_nav_lo = (g_n_nav >= lo) ? lo : 0;
        g_default_sidebar_nav_hi = g_n_nav;

        /* same call shape as layout_fixed_rows_and_scrolllist()'s own:
         * pass the FULL region width/height - the helper carves the
         * thumb + arrows off the right edge itself. */
        generic_sbar_register(sidebar->x, sidebar->y, sidebar->w, sidebar->h,
                              &g_default_sidebar_scroll, total_rows, visible_rows, max_scroll);
    } else {
        layout_fixed_rows_and_scrolllist(sidebar, sidebar->x, sidebar->y, sidebar->w, sidebar->h,
                                          &g_default_sidebar_scroll, &g_default_sidebar_nav_lo, &g_default_sidebar_nav_hi);
    }
    if (kh_layout_canvas_in_region(panel, panel->x, panel->y, panel->w, panel->h)) {
        /* MILESTONE A - the panel is a live canvas (pc-hq board); it
         * filled the box, no fixed-rows / flex pass for it. */
    } else if (panel->style.has_display && panel->style.display_flex) {
        zero_nav_subtree(panel);
        kh_css_deep(panel);
        css_layout_pass(panel, panel->x, panel->y, panel->w, panel->h);
        kh_assign_nav_subtree(panel);       /* detail-panel buttons: few, after the grid, before chrome */
    } else {
        layout_fixed_rows_and_scrolllist(panel, panel->x, panel->y, panel->w, panel->h,
                                          &g_default_scrolllist_scroll, &g_default_scrolllist_nav_lo, &g_default_scrolllist_nav_hi);
    }
    /* flex mode: css_layout_pass() sized every region above; run the
     * classic vertical fixed-rows+scroll+nav pass inside EACH extra
     * <panel> too (its box is already set). One shared scroll cursor
     * for all extras - fine while there's one; give it its own if a
     * window ever needs two independently-scrolling right columns. */
    if (page_is_flex) {
        for (int i = 0; i < page->n_children; i++) {
            Elem *c = page->children[i];
            if (c == panel || strcmp(c->tag, "panel") != 0) continue;
            layout_fixed_rows_and_scrolllist(c, c->x, c->y, c->w, c->h,
                                              &g_default_panel2_scroll,
                                              &g_default_panel2_nav_lo, &g_default_panel2_nav_hi);
        }
    }

    /* REAL, NEW 2026-09-01 - the real "X"/"!" chrome pair (see these
     * statics' own header comment). Positioned in the chrome strip's
     * own top-right corner, nav-numbered LAST (matches db-hq's own
     * g_dbhq_close_elem convention - a fresh window never opens with
     * Close already focused). */
    {
        /* REAL FIX 2026-09-01 (live report: "smushed to the right side...
         * can have more space and be more visible dont be too far
         * right") - wider buttons, a real gap between them, and a real
         * margin off the true right edge (not flush against it).
         * REAL FIX 2026-09-03 (direct live report: "the ! and x aren't
         * inside the box created as button, they are to the right of")
         * - 32px fit a bare "X" label alone, but draw_elem() always
         * prepends the real "[ ]NN." nav badge before ANY label (see its
         * own 2026-09-02 comment, "always draw [ ]N. when nav_index>0 -
         * digit-jump and AI control need the visible brackets"), so the
         * label spilled out past the new border's own right edge instead
         * of sitting inside it. db-hq's own real close button
         * (g_dbhq_close_w) is already scaled(56) for exactly this
         * reason - matched here, not invented. */
        int btn_w = 56, btn_h = CHROME_H - 4, gap = 8, right_margin = 10;

        memset(g_default_minimize_elem, 0, sizeof(*g_default_minimize_elem));
        snprintf(g_default_minimize_elem->tag, sizeof(g_default_minimize_elem->tag), "item");
        snprintf(g_default_minimize_elem->id, sizeof(g_default_minimize_elem->id), "chrome-minimize");
        snprintf(g_default_minimize_elem->label, sizeof(g_default_minimize_elem->label), "_");
        snprintf(g_default_minimize_elem->onclick, sizeof(g_default_minimize_elem->onclick), "MINIMIZE");
        g_default_minimize_elem->x = g_win_w - btn_w * 3 - gap * 2 - right_margin;
        g_default_minimize_elem->y = 2;
        g_default_minimize_elem->w = btn_w;
        g_default_minimize_elem->h = btn_h;
        css_compute_style(&g_sheet, g_default_minimize_elem->tag, g_default_minimize_elem->id, NULL, 0, 0, &g_default_minimize_elem->style);
        g_default_minimize_elem->nav_index = ++g_n_nav;
        g_nav[g_n_nav - 1] = g_default_minimize_elem;

        memset(g_default_fullscreen_elem, 0, sizeof(*g_default_fullscreen_elem));
        snprintf(g_default_fullscreen_elem->tag, sizeof(g_default_fullscreen_elem->tag), "item");
        snprintf(g_default_fullscreen_elem->id, sizeof(g_default_fullscreen_elem->id), "chrome-fullscreen");
        snprintf(g_default_fullscreen_elem->label, sizeof(g_default_fullscreen_elem->label), "!");
        snprintf(g_default_fullscreen_elem->onclick, sizeof(g_default_fullscreen_elem->onclick), "TOGGLE_FULLSCREEN");
        g_default_fullscreen_elem->x = g_win_w - btn_w * 2 - gap - right_margin; g_default_fullscreen_elem->y = 2;
        g_default_fullscreen_elem->w = btn_w; g_default_fullscreen_elem->h = btn_h;
        /* REAL, NEW 2026-09-03 (direct live report: "db-hq uses
         * completely different render for x[] button" - a real, visible
         * bordered box, not plain floating text, same real look
         * g_dbhq_close_elem already has) - id="chrome-fullscreen"/
         * "chrome-close" real CSS rules in entity_menu_default.css do
         * this, NOT a direct struct assignment here - same real lesson
         * as the dropdown-child bug earlier this session: this element
         * gets serialized to a frame file and repainted from THAT via
         * kh_paint_frame_line() (see redraw()'s own header comment),
         * which recomputes style fresh from css_compute_style() against
         * tag/id/classes only - any style field set directly on the
         * live struct here never survives that round trip. */
        css_compute_style(&g_sheet, g_default_fullscreen_elem->tag, g_default_fullscreen_elem->id, NULL, 0, 0, &g_default_fullscreen_elem->style);
        g_default_fullscreen_elem->nav_index = ++g_n_nav; g_nav[g_n_nav - 1] = g_default_fullscreen_elem;

        memset(g_default_close_elem, 0, sizeof(*g_default_close_elem));
        snprintf(g_default_close_elem->tag, sizeof(g_default_close_elem->tag), "item");
        snprintf(g_default_close_elem->id, sizeof(g_default_close_elem->id), "chrome-close");
        snprintf(g_default_close_elem->label, sizeof(g_default_close_elem->label), "X");
        snprintf(g_default_close_elem->onclick, sizeof(g_default_close_elem->onclick), "CLOSE");
        g_default_close_elem->x = g_win_w - btn_w - right_margin; g_default_close_elem->y = 2;
        g_default_close_elem->w = btn_w; g_default_close_elem->h = btn_h;
        /* REAL, NEW 2026-09-03 - same real CSS-not-struct fix as
         * fullscreen right above, same real reason. */
        css_compute_style(&g_sheet, g_default_close_elem->tag, g_default_close_elem->id, NULL, 0, 0, &g_default_close_elem->style);
        g_default_close_elem->nav_index = ++g_n_nav; g_nav[g_n_nav - 1] = g_default_close_elem;
    }

    if (g_focus_nav > g_n_nav) g_focus_nav = g_n_nav > 0 ? g_n_nav : 1;
    if (g_focus_nav < 1) g_focus_nav = 1;
    return 1;
}
/* ============ end generic sidebar+panel scroll ============ */

#define DOCK_BAR_H scaled(36)  /* UI-scaled, LIVEDESK-UI-SCALE.md (base 36) */
#define DOCK_SPRITE_PX kh_auto_px(24) /* screen-relative only (not font_scale), see kh_ui_apply_scale() */
#define DOCK_CELL_GAP kh_auto_px(16) /* screen-relative only (not font_scale), see kh_ui_apply_scale() */
#define DOCK_NAV_BADGE_PX kh_auto_px(36) /* screen-relative only (not font_scale), see kh_ui_apply_scale() */
#define DOCK_FOCUS_BOX_W kh_auto_px(64) /* screen-relative only (not font_scale), see kh_ui_apply_scale() */
/* REAL FIX 2026-09-14, direct live report ("the +- is not centered in
 * the space for it either... u may want to make the space for it
 * wider to accomodate"): 80px was sized back when nav badges on this
 * pair were always single-digit; a desk with 20+ entities (dsr's own
 * 17 live buildings) pushes the pager's own nav numbers into the
 * 30s - two digits - and the old margin left the pair cramped against
 * the last cell. Widened; dock_place_pager() below now centers the
 * pair within this margin instead of hugging the right edge. */
#define DOCK_PAGER_W kh_auto_px(110) /* screen-relative only (not font_scale), see kh_ui_apply_scale() */
/* DOCK_MAX_PACK removed 2026-09-14 (DOCK-BAR-GENERIC-LAYOUT-MIGRATION.md
 * phase 1) - was the fixed-size bound for the bottom bar's own
 * hand-packed pack[] array, deleted along with it now that
 * css_layout_pass's flex-wrap engine owns that job. */

static int kh_tree_has_text_input(Elem *e) {
    if (!e) return 0;
    if (!strcmp(e->tag, "cli_io") || !strcmp(e->tag, "text_area") || !strcmp(e->tag, "grid")) return 1;
    for (int i = 0; i < e->n_children; i++)
        if (kh_tree_has_text_input(e->children[i])) return 1;
    return 0;
}

static int window_is_dock(void) {
    return g_window && (elem_has_class(g_window, "dock-header") || elem_has_class(g_window, "dock-bottom"));
}

static int dock_is_our_win(Window w) {
    return w && (w == win || (g_dock_peer_win && w == g_dock_peer_win) ||
                 (g_dock_menu_win && w == g_dock_menu_win));
}

static void dock_grab_keyboard(Window cw) {
    if (!cw || !dpy) return;
    int rc = XGrabKeyboard(dpy, cw, True, GrabModeAsync, GrabModeAsync, CurrentTime);
    if (rc != GrabSuccess)
        kh_focus_debug_log("DOCK GRAB win=0x%lx rc=%d(0=success; 1=another client holds the keyboard)", (unsigned long)cw, rc);
    g_dock_kbd_win = cw;
}

/* Keep the dock grab until focus really leaves header/bottom/menu.
 * XGrabKeyboard itself emits FocusOut mode=NotifyGrab; ungrabbing on
 * that event dropped the grab in the same XPending loop as the click. */
static void dock_release_keyboard_if_left(void) {
    Window fw = None;
    int rev = 0;
    if (!dpy) return;
    XGetInputFocus(dpy, &fw, &rev);
    if (dock_is_our_win(fw)) {
        if (fw != g_dock_kbd_win) dock_grab_keyboard(fw);
        return;
    }
    if (g_dock_kbd_win) {
        kh_ungrab_kbd();
        g_dock_kbd_win = None;
    }
}

static void load_dock_strip_offset(int *out_x, int *out_y) {
    *out_x = 0;
    *out_y = 40;
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/livedesk_taskbar.pdl", g_house_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[PATH_BUF];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "SECTION", 7) != 0) continue;
        char *p = strchr(line, '|');
        if (!p) continue;
        p++;
        while (*p == ' ') p++;
        char *end = strchr(p, '|');
        if (!end) continue;
        char *key_end = end;
        while (key_end > p && key_end[-1] == ' ') key_end--;
        char key[48];
        size_t klen = (size_t)(key_end - p);
        if (klen == 0 || klen >= sizeof(key)) continue;
        memcpy(key, p, klen);
        key[klen] = '\0';
        char *v = end + 1;
        while (*v == ' ') v++;
        v[strcspn(v, "\r\n")] = '\0';
        if (strcmp(key, "strip_x_offset") == 0) *out_x = atoi(v);
        else if (strcmp(key, "strip_y_offset") == 0) *out_y = atoi(v);
    }
    fclose(f);
}

static int dock_text_px(const char *s) {
    if (!s || !s[0]) return 0;
    {
        int w = 0;
        const unsigned char *p = (const unsigned char *)s;
        const unsigned char *run = p;
        while (*p) {
            unsigned int cp;
            int clen = khtpm_utf8_decode(p, &cp);
            const EmojiTile *t = (cp == 0xFE0F || cp == 0x200D) ? NULL : khtpm_emoji_for_cp(cp);
            if (t) {
                if (p > run) {
                    if (dpy && font_ui) {
                        XGlyphInfo ext;
                        XftTextExtentsUtf8(dpy, font_ui, (const FcChar8 *)run, (int)(p - run), &ext);
                        w += ext.xOff;
                    } else w += (int)(p - run) * 7;
                }
                w += EMOJI_ADV;
                p += clen;
                run = p;
            } else p += clen;
        }
        if (p > run) {
            if (dpy && font_ui) {
                XGlyphInfo ext;
                XftTextExtentsUtf8(dpy, font_ui, (const FcChar8 *)run, (int)(p - run), &ext);
                w += ext.xOff;
            } else w += (int)(p - run) * 7;
        }
        return w;
    }
}

/* A cell's natural (unshrunk) width; the two shapes layout_dock_toolbar_row()
 * used inline before. */
static int dock_cell_natural_w(Elem *t) {
    int cw;
    if (elem_has_class(t, "no-nav")) {
        cw = 6 + dock_text_px(t->label) + 10;
        if (cw < 40) cw = 40;
        return cw;
    }
    cw = 6 + DOCK_NAV_BADGE_PX;
    if (t->sprite[0]) cw += DOCK_SPRITE_PX + 4;
    cw += dock_text_px(t->label) + 10;
    if (cw < 52) cw = 52;
    if (cw > 180) cw = 180;
    return cw;
}

/* Compact left-packed cells (old strip), not equal-split across the screen.
 * If the natural widths + gaps would run past max_w (a smaller screen than
 * the layouts were tuned on), every cell is shrunk by the same ratio so the
 * whole row fits instead of being cut off at the screen edge. Recomputed
 * from the natural widths every pass, so it is idempotent. */
static int layout_dock_toolbar_row(Elem *row, int x, int y, int max_w) {
    int j, col_x = x, used = 0;
    int nat_total = 0, n_cells = 0, shr_num = 1, shr_den = 1;
    row->x = x; row->y = y; row->h = DOCK_BAR_H; row->nav_index = 0;
    css_compute_style(&g_sheet, row->tag, row->id, row->classes, row->n_classes, 0, &row->style);
    for (j = 0; j < row->n_children; j++) {
        Elem *t = row->children[j];
        if (strcmp(t->tag, "item") != 0) continue;
        nat_total += dock_cell_natural_w(t);
        n_cells++;
    }
    if (n_cells > 0 && max_w > 0) {
        int avail = max_w - n_cells * DOCK_CELL_GAP;
        if (avail < n_cells * 24) avail = n_cells * 24;
        if (nat_total > avail) { shr_num = avail; shr_den = nat_total; }
    }
    for (j = 0; j < row->n_children; j++) {
        Elem *t = row->children[j];
        int cw;
        if (strcmp(t->tag, "item") != 0) {
            t->x = x; t->y = -100000; t->w = 0; t->h = 0; t->nav_index = 0;
            continue;
        }
        /* class="no-nav" - a plain status cell (e.g. the strip's pid
         * readout after the clock): laid out and drawn, but no nav
         * index, so it gets no "[ ]N." badge and arrows/digits skip it. */
        if (elem_has_class(t, "no-nav")) {
            cw = dock_cell_natural_w(t);
            if (shr_den > 1) cw = cw * shr_num / shr_den;
            t->x = col_x; t->y = y; t->w = cw; t->h = DOCK_BAR_H; t->nav_index = 0;
            css_compute_style(&g_sheet, t->tag, t->id, t->classes, t->n_classes, 0, &t->style);
            col_x += cw + DOCK_CELL_GAP;
            used = col_x - x;
            if (used > max_w) used = max_w;
            continue;
        }
        cw = dock_cell_natural_w(t);
        if (shr_den > 1) cw = cw * shr_num / shr_den;
        t->x = col_x;
        t->y = y;
        t->w = cw;
        t->h = DOCK_BAR_H;
        css_compute_style(&g_sheet, t->tag, t->id, t->classes, t->n_classes, 0, &t->style);
        t->nav_index = ++g_n_nav;
        g_nav[g_n_nav - 1] = t;
        col_x += cw + DOCK_CELL_GAP;
        used = col_x - x;
        if (used > max_w) used = max_w;
    }
    row->w = used > 0 ? used : max_w;
    return row->w;
}

static int dock_item_cw(Elem *t) {
    int cw = 6 + DOCK_NAV_BADGE_PX;
    if (t->sprite[0]) cw += DOCK_SPRITE_PX + 4;
    cw += dock_text_px(t->label) + 10;
    if (cw < 52) cw = 52;
    if (elem_has_class(t, "hqwin")) {
        if (cw > 240) cw = 240;
    } else if (cw > 180) cw = 180;
    return cw;
}

/* Horizontal "- +" pager. REAL FIX 2026-09-14, direct live report ("the
 * +- [for] viewing lower rows of entities... are on left instead of
 * right, where there is literally a designated space fore them" -
 * pc-hq's own footer pager, `pchq-board.css`'s own "appears bottom-
 * right" convention, cited as the real working reference). Right-
 * aligned again now, INSIDE the DOCK_PAGER_W margin the row-packer
 * already reserves and never lays a cell into (`max_w = g_win_w -
 * DOCK_FOCUS_BOX_W - DOCK_PAGER_W` above) - confirmed live: that
 * margin was sitting empty on the right the whole time the pager
 * rendered left-flowing instead, exactly the "designated space" this
 * report points at. This reverses an EARLIER direct instruction
 * ("why isn't it justified left like the [cells]?", 2026-08-something)
 * that moved it left-flowing in the first place - recorded here since
 * that comment is now wrong, not silently deleted: this NEWER, more
 * specific instruction (with a real working reference to match) is
 * the one to keep going forward if the two ever seem to conflict
 * again. `after_x` (the x just past the last laid-out cell) is no
 * longer used for positioning, only for the `need` check's own
 * context - kept as a parameter so every call site stays unchanged. */
static void dock_place_pager(int win_w, int after_x) {
    (void)after_x;
    /* REAL, NEW 2026-09-15, direct live report ("could be a bit more
     * 'left' and spaced between the 2") - widened the -/+ gap and
     * biased the centered position a bit left of dead-center in the
     * DOCK_PAGER_W margin, both real, cosmetic pixel tweaks only. */
    int aw = scaled(22), gap = scaled(10);
    int left_bias = scaled(10);
    int need = (g_dock_packed_rows > 1) || (g_dock_visible_rows > 1);

    memset(&g_dock_minus_elem, 0, sizeof(g_dock_minus_elem));
    memset(&g_dock_plus_elem,  0, sizeof(g_dock_plus_elem));
    if (!need) {
        g_dock_minus_elem.y = g_dock_plus_elem.y = -100000;
        g_dock_minus_elem.nav_index = g_dock_plus_elem.nav_index = 0;
        return;
    }

    /* Centered within the reserved DOCK_PAGER_W margin (not hugging the
     * right edge) - direct live report ("the +- is not centered in the
     * space for it either"). margin_start is where the row-packer's
     * own max_w cap (g_win_w - DOCK_FOCUS_BOX_W - DOCK_PAGER_W) stops
     * laying out cells, i.e. the real left edge of this reserved zone. */
    int margin_start = win_w - DOCK_PAGER_W;
    int content_w = 2 * aw + gap;
    int mx = margin_start + (DOCK_PAGER_W - content_w) / 2 - left_bias;
    if (mx < DOCK_FOCUS_BOX_W) mx = DOCK_FOCUS_BOX_W;
    if (mx + content_w > win_w - 4) mx = win_w - 4 - content_w;

    snprintf(g_dock_minus_elem.tag, sizeof(g_dock_minus_elem.tag), "item");
    snprintf(g_dock_minus_elem.id, sizeof(g_dock_minus_elem.id), "dock-page-minus");
    snprintf(g_dock_minus_elem.label, sizeof(g_dock_minus_elem.label), "-");
    snprintf(g_dock_minus_elem.onclick, sizeof(g_dock_minus_elem.onclick), "PAGEROW:-1");
    g_dock_minus_elem.x = mx; g_dock_minus_elem.y = 0;
    g_dock_minus_elem.w = aw; g_dock_minus_elem.h = DOCK_BAR_H;
    css_compute_style(&g_sheet, "item", "dock-page-minus", NULL, 0, 0, &g_dock_minus_elem.style);
    g_dock_minus_elem.nav_index = ++g_n_nav;
    g_nav[g_n_nav - 1] = &g_dock_minus_elem;

    snprintf(g_dock_plus_elem.tag, sizeof(g_dock_plus_elem.tag), "item");
    snprintf(g_dock_plus_elem.id, sizeof(g_dock_plus_elem.id), "dock-page-plus");
    snprintf(g_dock_plus_elem.label, sizeof(g_dock_plus_elem.label), "+");
    snprintf(g_dock_plus_elem.onclick, sizeof(g_dock_plus_elem.onclick), "PAGEROW:+1");
    g_dock_plus_elem.x = mx + aw + gap; g_dock_plus_elem.y = 0;
    g_dock_plus_elem.w = aw; g_dock_plus_elem.h = DOCK_BAR_H;
    css_compute_style(&g_sheet, "item", "dock-page-plus", NULL, 0, 0, &g_dock_plus_elem.style);
    g_dock_plus_elem.nav_index = ++g_n_nav;
    g_nav[g_n_nav - 1] = &g_dock_plus_elem;
}

static void dock_draw_separators(Elem *page) {
    int i, j;
    if (!page || !font_ui) return;
    for (i = 0; i < page->n_children; i++) {
        Elem *row = page->children[i];
        if (strcmp(row->tag, "row") != 0 || !elem_has_class(row, "toolbar")) continue;
        Elem *prev = NULL;
        for (j = 0; j < row->n_children; j++) {
            Elem *t = row->children[j];
            if (strcmp(t->tag, "item") != 0 || t->w <= 0) continue;
            if (prev) {
                int mx = (prev->x + prev->w + t->x) / 2 - 2;
                int ty = t->y + DOCK_BAR_H / 2 + font_ui->ascent / 2 - 2;
                XftColor col = xft_color(g_theme_fg);
                XftDrawStringUtf8(xftdraw_buf, &col, font_ui, mx, ty, (const FcChar8 *)"|", 1);
                XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &col);
            }
            prev = t;
        }
    }
}

/* Screen-edge taskbar bars: packed cells, pdl offset, no X/! chrome. */
static int layout_dock_bar(Elem *page) {
    int is_bottom, sw, sh, y, i, ox, oy, row_w, content_w = 0;
    if (!page || !window_is_dock()) return 0;
    is_bottom = elem_has_class(g_window, "dock-bottom");
    g_default_has_sidebar_panel = 1; /* persistent: dispatch must not quit */
    generic_sbar_reset();
    sw = kh_screen_w();
    sh = kh_screen_h();
    load_theme_colors();
    load_dock_strip_offset(&ox, &oy);
    ox = kh_auto_px(ox); /* left margin follows the screen-relative scale (oy stays: it clears the desktop's own top panel) */
    if (is_bottom) {
        int inset = ox;
        if (inset < 8) inset = 8;
        g_win_x = inset;
        g_win_w = sw - inset * 2;
        if (g_win_w < 80) g_win_w = 80;
    }
    y = 0;
    if (is_bottom) {
        /* REAL FIX 2026-09-14 (DOCK-BAR-GENERIC-LAYOUT-MIGRATION.md,
         * phase 1) - the hand-written column/row-advance pack loop
         * (real source of today's own pager-position/centering/row-
         * overlap bugs, all pixel-math mistakes) is replaced by the
         * generic flex-wrap engine (css_layout_pass, khtpm_render_
         * core.c - same real mechanism canvas-craft.xhtpm's own proven
         * tile grid already uses). This C code now only does what a
         * template author can't express in CSS: MEASURE each cell's
         * own content-driven width (dock_item_cw() kept for exactly
         * that, no longer for positioning) and set css_layout_pass's
         * one real input it can't infer (the wrap-line height). The
         * actual row-wrap/column-advance math is gone from this file -
         * it lives in the shared engine now, where a bug in it gets
         * fixed once for every window, not re-found per dock feature. */
        Elem *row_elem = NULL;
        int max_w = g_win_w - DOCK_FOCUS_BOX_W - DOCK_PAGER_W;
        if (max_w < 40) max_w = 40;
        for (i = 0; i < page->n_children; i++) {
            Elem *c = page->children[i];
            if (strcmp(c->tag, "row") == 0 && elem_has_class(c, "toolbar")) { row_elem = c; break; }
        }
        int n_pack = 0, r_max = 0;
        if (row_elem) {
            int k;
            css_compute_style(&g_sheet, row_elem->tag, row_elem->id, row_elem->classes, row_elem->n_classes, 0, &row_elem->style);
            for (k = 0; k < row_elem->n_children; k++) {
                Elem *t = row_elem->children[k];
                if (strcmp(t->tag, "item") != 0) {
                    t->x = 0; t->y = -100000; t->w = 0; t->h = 0; t->nav_index = 0;
                    continue;
                }
                css_compute_style(&g_sheet, t->tag, t->id, t->classes, t->n_classes, 0, &t->style);
                t->w = dock_item_cw(t);
                t->h = DOCK_BAR_H;
            }
            /* 100000: tall enough that flex-wrap never runs out of
             * vertical room to lay out every real row - rows beyond
             * g_dock_visible_rows are hidden below, after layout, the
             * same real "compute everything, then hide what's off-
             * page" pattern the footer pager already uses. */
            css_layout_pass(row_elem, DOCK_FOCUS_BOX_W, 0, max_w, 100000);
            for (k = 0; k < row_elem->n_children; k++) {
                Elem *t = row_elem->children[k];
                if (strcmp(t->tag, "item") != 0 || t->w <= 0) continue;
                n_pack++;
                int r = t->y / DOCK_BAR_H;
                if (r > r_max) r_max = r;
            }
        }
        g_dock_packed_rows = (n_pack > 0) ? (r_max + 1) : 1;
        if (g_dock_visible_rows > g_dock_packed_rows) g_dock_visible_rows = g_dock_packed_rows;
        if (g_dock_visible_rows < 1) g_dock_visible_rows = 1;
        if (row_elem) {
            int k;
            for (k = 0; k < row_elem->n_children; k++) {
                Elem *t = row_elem->children[k];
                if (strcmp(t->tag, "item") != 0 || t->w <= 0) continue;
                int r = t->y / DOCK_BAR_H;
                if (r < g_dock_visible_rows) {
                    t->nav_index = ++g_n_nav;
                    g_nav[g_n_nav - 1] = t;
                } else {
                    t->x = 0; t->y = -100000; t->w = 0; t->h = 0; t->nav_index = 0;
                }
            }
        }
        y = g_dock_visible_rows * DOCK_BAR_H;
        dock_place_pager(g_win_w, 0);
        for (i = 0; i < page->n_children; i++) {
            Elem *c = page->children[i];
            if (strcmp(c->tag, "cli_io") == 0) {
                c->x = 0; c->y = y; c->w = (g_win_w > 120 ? g_win_w : 240); c->h = DOCK_BAR_H;
                css_compute_style(&g_sheet, c->tag, c->id, c->classes, c->n_classes, 0, &c->style);
                c->nav_index = ++g_n_nav;
                g_nav[g_n_nav - 1] = c;
                y += DOCK_BAR_H;
            } else if (elem_has_class(c, "dropdown-child")) {
                continue;
            } else if (!(strcmp(c->tag, "row") == 0 && elem_has_class(c, "toolbar"))) {
                c->x = 0; c->y = -100000; c->w = 0; c->h = 0; c->nav_index = 0;
            }
        }
    } else
    for (i = 0; i < page->n_children; i++) {
        Elem *c = page->children[i];
        if (strcmp(c->tag, "row") == 0 && elem_has_class(c, "toolbar")) {
            int n_items = 0, k;
            int row_max;
            for (k = 0; k < c->n_children; k++)
                if (strcmp(c->children[k]->tag, "item") == 0) n_items++;
            if (n_items == 0) {
                c->x = 0; c->y = -100000; c->w = 0; c->h = 0; c->nav_index = 0;
                continue;
            }
            row_max = (is_bottom ? g_win_w : (sw - ox * 2)) - DOCK_FOCUS_BOX_W;
            if (row_max < 40) row_max = 40;
            row_w = layout_dock_toolbar_row(c, DOCK_FOCUS_BOX_W, y, row_max);
            row_w += DOCK_FOCUS_BOX_W;
            if (row_w > content_w) content_w = row_w;
            y += DOCK_BAR_H;
        } else if (strcmp(c->tag, "cli_io") == 0) {
            c->x = 0; c->y = y; c->w = (content_w > 120 ? content_w : 240); c->h = DOCK_BAR_H;
            css_compute_style(&g_sheet, c->tag, c->id, c->classes, c->n_classes, 0, &c->style);
            c->nav_index = ++g_n_nav;
            g_nav[g_n_nav - 1] = c;
            y += DOCK_BAR_H;
        } else if (elem_has_class(c, "dropdown-child")) {
            continue;
        } else {
            c->x = 0; c->y = -100000; c->w = 0; c->h = 0; c->nav_index = 0;
        }
    }
    /* REAL FIX 2026-09-22 (task 1 - "pals" dock dropdown, direct user
     * correction mid-pass: "if its 'no limit' it will go off screen" -
     * the popup's own ON-SCREEN HEIGHT must stay bounded to a small,
     * fixed number of visible rows regardless of how many real entries
     * the data array holds; a scrollbar, not a taller window, is how the
     * rest get reached). Two SEPARATE caps, not to be conflated:
     *   1. Data capacity: KTB_LIVEDESK_DYN_MAX 24->256 (khtpm_taskbar_
     *      manager.h) - the menu-item array itself, real capacity.
     *   2. Visible window height: DOCK_DROPDOWN_MAX_VISIBLE_ROWS below -
     *      a fixed, named, small constant. Window height is ALWAYS
     *      visible_rows * DOCK_BAR_H, never n_open_total * DOCK_BAR_H.
     * Real scroll, reusing the house's own generic mechanism per the
     * standards skill (no second scroll mechanism, no new key handling):
     * g_dock_dropdown_scroll is the plain int cell generic_sbar_register()
     * is given a pointer to; its own SCROLLUP:/SCROLLDOWN: onclick +
     * generic_sbar_wheel() + the generic Page_Up/Page_Down handler are
     * ALL that ever mutate it - nothing new added here. The one real gap
     * closed here beyond that: this dropdown paints into its own SEPARATE
     * override-redirect popup window (dock_paint_menu(), not the normal
     * per-frame Elem paint loop), so draw_generic_scrollbars() - the
     * same function every other scrolled list's track/thumb pixels come
     * from - is called a second time from inside dock_paint_menu()'s own
     * g_window-swapped block below, against that popup's buf/gc. */
#define DOCK_DROPDOWN_MAX_VISIBLE_ROWS 12
    {
        char last_target[64] = "";
        int stack_n = 0, col_w = 0, n_open_total = 0, visible_rows, max_scroll;
        Elem *trig0 = NULL;
        for (i = 0; i < page->n_children; i++) {
            Elem *c = page->children[i];
            Elem *trigger;
            int open, dw;
            if (!elem_has_class(c, "dropdown-child")) continue;
            trigger = c->target_id[0] ? find_by_id(g_window, c->target_id) : NULL;
            open = trigger && (g_default_active_scope_root == trigger ||
                (g_default_active_scope_id[0] && trigger->id[0] &&
                 strcmp(g_default_active_scope_id, trigger->id) == 0));
            if (!open || !trigger) continue;
            n_open_total++;
            dw = dock_text_px(c->label) + DOCK_NAV_BADGE_PX + 16;
            if (c->sprite[0]) dw += DOCK_SPRITE_PX + 4;
            if (dw < trigger->w) dw = trigger->w;
            if (dw > col_w) col_w = dw;
            trig0 = trigger;
        }
        if (col_w < 48) col_w = 48;
        if (trig0 && trig0->x + col_w > sw - 8) {
            col_w = sw - 8 - trig0->x;
            if (col_w < 40) col_w = 40;
        }
        /* Reserve room for the scrollbar track's own width when there's
         * more than one screenful, same real "shrink the row, don't
         * overlap the thumb" shape every other generic_sbar_register()
         * call site in this file already uses (see its own GENERIC_
         * SCROLLBAR_W header comment) - checked BEFORE computing
         * visible_rows so a wide pal-name column doesn't get the thumb
         * drawn on top of its label text. */
        if (n_open_total > DOCK_DROPDOWN_MAX_VISIBLE_ROWS)
            col_w += GENERIC_SCROLLBAR_W + 4;
        /* Reset the scroll cursor to 0 whenever a DIFFERENT trigger's
         * dropdown becomes the open one - a stale offset from "pals"
         * must not leak into "hq" opened right after. */
        if (trig0 && trig0->id[0]) {
            if (strcmp(g_dock_dropdown_scroll_owner, trig0->id) != 0) {
                g_dock_dropdown_scroll = 0;
                snprintf(g_dock_dropdown_scroll_owner, sizeof(g_dock_dropdown_scroll_owner), "%s", trig0->id);
            }
        } else {
            g_dock_dropdown_scroll = 0;
            g_dock_dropdown_scroll_owner[0] = '\0';
        }
        visible_rows = (n_open_total > 0 && n_open_total < DOCK_DROPDOWN_MAX_VISIBLE_ROWS)
            ? n_open_total : DOCK_DROPDOWN_MAX_VISIBLE_ROWS;
        if (visible_rows < 1) visible_rows = 1;
        max_scroll = n_open_total - visible_rows;
        if (max_scroll < 0) max_scroll = 0;
        if (g_dock_dropdown_scroll > max_scroll) g_dock_dropdown_scroll = max_scroll;
        if (g_dock_dropdown_scroll < 0) g_dock_dropdown_scroll = 0;
        for (i = 0; i < page->n_children; i++) {
            Elem *c = page->children[i];
            Elem *trigger;
            int open, vis_idx;
            if (!elem_has_class(c, "dropdown-child")) continue;
            trigger = c->target_id[0] ? find_by_id(g_window, c->target_id) : NULL;
            open = trigger && (g_default_active_scope_root == trigger ||
                (g_default_active_scope_id[0] && trigger->id[0] &&
                 strcmp(g_default_active_scope_id, trigger->id) == 0));
            if (strcmp(last_target, c->target_id) != 0) {
                stack_n = 0;
                snprintf(last_target, sizeof(last_target), "%s", c->target_id);
            }
            css_compute_style(&g_sheet, c->tag, c->id, c->classes, c->n_classes, 0, &c->style);
            vis_idx = stack_n - g_dock_dropdown_scroll;
            if (open && trigger && vis_idx >= 0 && vis_idx < visible_rows) {
                c->x = 0;
                c->y = vis_idx * DOCK_BAR_H;
                c->w = col_w;
                c->h = DOCK_BAR_H;
                c->nav_index = ++g_n_nav;
                g_nav[g_n_nav - 1] = c;
                if (!g_dock_drop_lo) g_dock_drop_lo = c->nav_index;
                g_dock_drop_hi = c->nav_index;
            } else {
                /* clip, never translate (house standards skill,
                 * 2026-09-06 incident) - park fully off-screen, no nav
                 * number, whether it's above OR below the visible
                 * window. This is the exact fix for the "first row
                 * shows two nav buttons" bug: previously an off-window
                 * row one slot above the fold could still land inside
                 * the popup's paint range; the strict vis_idx bounds
                 * check above means a row is either fully inside the
                 * visible block with its own single nav number, or
                 * fully parked - no boundary row can double up. */
                c->x = 0; c->y = -100000; c->w = 0; c->h = 0; c->nav_index = 0;
            }
            stack_n++;
        }
        if (!is_bottom) {
            if (trig0 && g_dock_drop_lo) {
                int n_rows = g_dock_drop_hi - g_dock_drop_lo + 1; /* == visible_rows shown, by construction */
                g_dock_menu_w = col_w;
                g_dock_menu_h = n_rows * DOCK_BAR_H;
                if (g_dock_menu_w < 40) g_dock_menu_w = 40;
                g_dock_menu_sx = trig0->x;
                g_dock_menu_sy = trig0->y + trig0->h;
                if (max_scroll > 0)
                    generic_sbar_register(0, 0, g_dock_menu_w, g_dock_menu_h,
                                          &g_dock_dropdown_scroll, n_open_total, visible_rows, max_scroll);
            } else {
                g_dock_menu_w = 0;
                g_dock_menu_h = 0;
            }
        }
    }
    if (y < DOCK_BAR_H) y = DOCK_BAR_H;
    /* REAL, NEW 2026-09-03 (HQ-WINDOW-TASKBAR-ENTRIES-AND-MINIMIZE-2026-09-
     * 03.md §2.1/§4) - the bottom dock used to be hard-fixed at exactly one
     * DOCK_BAR_H tall, so the FIRST (entities/tabs) and SECOND (shortcuts)
     * rows already sat fine but adding a THIRD row (this design doc's
     * per-live-HQ-window taskbar entries, each in their own real section)
     * would have been silently clipped off-window - every real count of
     * stacked toolbar rows lives in `y` by this point (each row increments
     * it by DOCK_BAR_H above). For the bottom dock, grow the window to the
     * full stacked height instead of clamping at one row: since it's
     * anchored to the screen bottom (g_win_y = sh - g_win_h below, using
     * this same value), growing it makes it rise UP off the bottom edge -
     * exactly the "show more rows by adding another layer" shape the design
     * doc's own §4.2 asks for, without any + / - paging yet (that's the
     * separate Slice C capacity mechanism, untouched here). Header dock
     * (is_bottom==0) keeps its existing one-row fixed height unchanged. */
    if (is_bottom)
        g_win_h = y;
    else
        g_win_h = DOCK_BAR_H;
    if (is_bottom) {
        int inset = ox;
        int bar_max;
        if (inset < 8) inset = 8;
        bar_max = sw - inset * 2;
        if (bar_max < 80) bar_max = 80;
        g_win_x = inset;
        g_win_y = sh - g_win_h;
        g_win_w = bar_max;
    } else {
        g_win_x = ox;
        g_win_y = oy;
        g_win_w = content_w + 8;
        if (g_win_w < 80) g_win_w = 80;
        if (g_win_x + g_win_w > sw - ox) g_win_w = sw - ox - g_win_x;
        if (g_win_w < 40) g_win_w = 40;
        if (g_dock_menu_w > 0) {
            g_dock_menu_sx += g_win_x;
            g_dock_menu_sy += g_win_y;
        }
    }
    g_window->w = g_win_w;
    g_window->h = g_win_h;
    return 1;
}

static void dock_paint_peer(void) {
    Elem *hold, *page;
    Window winh;
    Pixmap bufh;
    XftDraw *xh;
    GC gch;
    int x, y, w, h, bw, bh;
    if (!g_dock_peer || !g_dock_peer_win || g_dock_in_peer_paint) return;
    g_dock_in_peer_paint = 1;
    hold = g_window; winh = win; bufh = buf; xh = xftdraw_buf; gch = gc;
    x = g_win_x; y = g_win_y; w = g_win_w; h = g_win_h; bw = g_buf_w; bh = g_buf_h;
    g_window = g_dock_peer;
    win = g_dock_peer_win;
    buf = g_dock_peer_buf;
    xftdraw_buf = g_dock_peer_xft;
    gc = g_dock_peer_gc;
    g_win_x = g_dock_peer_x; g_win_y = g_dock_peer_y;
    g_win_w = g_dock_peer_w; g_win_h = g_dock_peer_h;
    g_buf_w = g_dock_peer_buf_w; g_buf_h = g_dock_peer_buf_h;
    if (g_window) { g_window->w = g_win_w; g_window->h = g_win_h; }
    if (g_win_w > g_buf_w || g_win_h > g_buf_h) {
        int nw = g_win_w > g_buf_w ? g_win_w : g_buf_w;
        int nh = g_win_h > g_buf_h ? g_win_h : g_buf_h;
        if (xftdraw_buf) { XftDrawDestroy(xftdraw_buf); xftdraw_buf = NULL; }
        if (buf) XFreePixmap(dpy, buf);
        buf = XCreatePixmap(dpy, win, (unsigned)nw, (unsigned)nh, (unsigned)DefaultDepth(dpy, screen));
        xftdraw_buf = XftDrawCreate(dpy, buf, DefaultVisual(dpy, screen), cmap);
        g_buf_w = nw; g_buf_h = nh;
        g_dock_peer_buf = buf; g_dock_peer_xft = xftdraw_buf;
        g_dock_peer_buf_w = nw; g_dock_peer_buf_h = nh;
    }
    XSetForeground(dpy, gc, alloc_pixel(g_theme_bg));
    XFillRectangle(dpy, buf, gc, 0, 0, (unsigned)g_win_w, (unsigned)g_win_h);
    {
        Window focus_win; int focus_revert;
        XGetInputFocus(dpy, &focus_win, &focus_revert);
        const char *mark = (focus_win == win) ? "^" : ".";
        int ty = DOCK_BAR_H / 2 + (font_ui ? font_ui->ascent / 2 : 6);
        XftColor mark_col = xft_color(window_is_dock() ? g_theme_fg : "#eeeeee");
        XftDrawStringUtf8(xftdraw_buf, &mark_col, font_ui, 18, ty, (const FcChar8 *)mark, 1);
        XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &mark_col);
        XSetForeground(dpy, gc, alloc_pixel("#4a4a4a"));
        XDrawLine(dpy, buf, gc, DOCK_FOCUS_BOX_W, 0, DOCK_FOCUS_BOX_W, g_win_h);
    }
    page = find_page(g_current_page);
    if (page) {
        char fpath[PATH_BUF], tmpp[PATH_BUF];
        snprintf(fpath, sizeof(fpath), "%s/#.desktop/entity_menu_frame_%d_bot.txt", g_house_root, (int)getpid());
        snprintf(tmpp, sizeof(tmpp), "%s.tmp", fpath);
        FILE *ff = fopen(tmpp, "w");
        if (ff) {
            kh_serialize_frame_subtree(ff, page);
            if (g_dock_plus_elem.w > 0) kh_serialize_frame_elem(ff, &g_dock_plus_elem);
            if (g_dock_minus_elem.w > 0) kh_serialize_frame_elem(ff, &g_dock_minus_elem);
            fclose(ff);
            rename(tmpp, fpath);
        }
        FILE *rf = fopen(fpath, "r");
        if (rf) {
            char line[9000]; /* REAL, NEW 2026-09-05 - see kh_paint_frame_line()'s own buf2 comment: must be big enough for a real text_area line */
            while (fgets(line, sizeof(line), rf)) {
                size_t len = strlen(line);
                while (len > 0 && (line[len-1]=='\n' || line[len-1]=='\r')) line[--len] = '\0';
                if (len) kh_paint_frame_line(line);
            }
            fclose(rf);
        }
        dock_draw_separators(page);
    }
    {
        int rr;
        XSetForeground(dpy, gc, alloc_pixel(g_theme_fg[0] ? g_theme_fg : "#888888"));
        for (rr = 1; rr < g_dock_visible_rows; rr++)
            XDrawLine(dpy, buf, gc, 0, rr * DOCK_BAR_H, g_win_w, rr * DOCK_BAR_H);
        if (g_dock_plus_elem.w > 0)
            XDrawLine(dpy, buf, gc, g_win_w - DOCK_PAGER_W, 0, g_win_w - DOCK_PAGER_W, g_win_h);
    }
    {
        XWindowAttributes wa;
        if (XGetWindowAttributes(dpy, win, &wa) &&
            (wa.width != g_win_w || wa.height != g_win_h || wa.x != g_win_x || wa.y != g_win_y))
            XMoveResizeWindow(dpy, win, g_win_x, g_win_y, (unsigned)g_win_w, (unsigned)g_win_h);
    }
    /* 2px theme-secondary window frame, same as every other khtpm
     * window (redraw() / run_pchq_board_mode()) - drawn LAST, right
     * before the buffer->window present so no content paint can
     * overpaint it. The bottom taskbar strip (this peer) was the last
     * window still missing it. g_win_w/g_win_h are the peer's own
     * dimensions here (swapped in above). */
    {
        XSetForeground(dpy, gc, alloc_pixel(g_theme_fg[0] ? g_theme_fg : "#888888"));
        XDrawRectangle(dpy, buf, gc, 0, 0,
                       (unsigned)(g_win_w - 1), (unsigned)(g_win_h - 1));
    }
    {
        XImage *frame = XGetImage(dpy, buf, 0, 0, (unsigned)g_win_w, (unsigned)g_win_h, AllPlanes, ZPixmap);
        if (frame) {
            XPutImage(dpy, win, gc, frame, 0, 0, 0, 0, (unsigned)g_win_w, (unsigned)g_win_h);
            XDestroyImage(frame);
        } else {
            XCopyArea(dpy, buf, win, gc, 0, 0, (unsigned)g_win_w, (unsigned)g_win_h, 0, 0);
        }
    }
    g_window = hold; win = winh; buf = bufh; xftdraw_buf = xh; gc = gch;
    g_win_x = x; g_win_y = y; g_win_w = w; g_win_h = h; g_buf_w = bw; g_buf_h = bh;
    g_dock_in_peer_paint = 0;
}

static void dock_paint_menu(void) {
    int i;
    if (g_dock_menu_w <= 0 || g_dock_menu_h <= 0 || g_dock_drop_lo < 1) {
        if (g_dock_menu_win) XUnmapWindow(dpy, g_dock_menu_win);
        return;
    }
    if (!g_dock_menu_win) {
        XSetWindowAttributes swa;
        swa.background_pixel = alloc_pixel(g_theme_bg);
        swa.override_redirect = True;
        swa.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | KeyPressMask | FocusChangeMask;
        g_dock_menu_win = XCreateWindow(dpy, RootWindow(dpy, screen),
            g_dock_menu_sx, g_dock_menu_sy, (unsigned)g_dock_menu_w, (unsigned)g_dock_menu_h,
            0, CopyFromParent, InputOutput, CopyFromParent,
            CWBackPixel | CWOverrideRedirect | CWEventMask, &swa);
        apply_dock_window_hints(dpy, g_dock_menu_win, g_dock_menu_sx, g_dock_menu_sy);
        g_dock_menu_gc = XCreateGC(dpy, g_dock_menu_win, 0, NULL);
        g_dock_menu_buf_w = g_dock_menu_w;
        g_dock_menu_buf_h = g_dock_menu_h;
        g_dock_menu_buf = XCreatePixmap(dpy, g_dock_menu_win,
            (unsigned)g_dock_menu_buf_w, (unsigned)g_dock_menu_buf_h,
            (unsigned)DefaultDepth(dpy, screen));
        g_dock_menu_xft = XftDrawCreate(dpy, g_dock_menu_buf, DefaultVisual(dpy, screen), cmap);
    }
    XMoveResizeWindow(dpy, g_dock_menu_win, g_dock_menu_sx, g_dock_menu_sy,
                      (unsigned)g_dock_menu_w, (unsigned)g_dock_menu_h);
    XMapRaised(dpy, g_dock_menu_win);
    if (g_dock_menu_w > g_dock_menu_buf_w || g_dock_menu_h > g_dock_menu_buf_h) {
        int nw = g_dock_menu_w > g_dock_menu_buf_w ? g_dock_menu_w : g_dock_menu_buf_w;
        int nh = g_dock_menu_h > g_dock_menu_buf_h ? g_dock_menu_h : g_dock_menu_buf_h;
        if (g_dock_menu_xft) { XftDrawDestroy(g_dock_menu_xft); g_dock_menu_xft = NULL; }
        if (g_dock_menu_buf) XFreePixmap(dpy, g_dock_menu_buf);
        g_dock_menu_buf = XCreatePixmap(dpy, g_dock_menu_win, (unsigned)nw, (unsigned)nh,
            (unsigned)DefaultDepth(dpy, screen));
        g_dock_menu_xft = XftDrawCreate(dpy, g_dock_menu_buf, DefaultVisual(dpy, screen), cmap);
        g_dock_menu_buf_w = nw; g_dock_menu_buf_h = nh;
    }
    {
        Window winh = win;
        Pixmap bufh = buf;
        XftDraw *xh = xftdraw_buf;
        GC gch = gc;
        int ww = g_win_w, hh = g_win_h;
        g_dock_in_menu_paint = 1;
        win = g_dock_menu_win;
        buf = g_dock_menu_buf;
        xftdraw_buf = g_dock_menu_xft;
        gc = g_dock_menu_gc;
        g_win_w = g_dock_menu_w;
        g_win_h = g_dock_menu_h;
        XSetForeground(dpy, gc, alloc_pixel(g_theme_bg));
        XFillRectangle(dpy, buf, gc, 0, 0, (unsigned)g_win_w, (unsigned)g_win_h);
        {
            char fpath[PATH_BUF], tmpp[PATH_BUF];
            snprintf(fpath, sizeof(fpath), "%s/#.desktop/entity_menu_frame_%d_menu.txt",
                     g_house_root, (int)getpid());
            snprintf(tmpp, sizeof(tmpp), "%s.tmp", fpath);
            FILE *ff = fopen(tmpp, "w");
            if (ff) {
                for (i = g_dock_drop_lo; i <= g_dock_drop_hi; i++) {
                    if (i >= 1 && i <= g_n_nav && g_nav[i - 1])
                        kh_serialize_frame_elem(ff, g_nav[i - 1]);
                }
                fclose(ff);
                rename(tmpp, fpath);
            }
            FILE *rf = fopen(fpath, "r");
            if (rf) {
                char line[9000]; /* REAL, NEW 2026-09-05 - see kh_paint_frame_line()'s own buf2 comment: must be big enough for a real text_area line */
                while (fgets(line, sizeof(line), rf)) {
                    size_t len = strlen(line);
                    while (len > 0 && (line[len-1]=='\n' || line[len-1]=='\r')) line[--len] = '\0';
                    if (len) kh_paint_frame_line(line);
                }
                fclose(rf);
            }
        }
        /* REAL, NEW 2026-09-22 (task 1) - same generic track/thumb draw
         * every other scrolled list's scrollbar pixels come from
         * (registered above in layout_dock_bar()'s dropdown-child pass
         * via generic_sbar_register(), same call this file's every
         * other scroll site uses) - this popup paints through its own
         * separate g_window-swapped block, so it needs its own call to
         * the SAME shared draw function, not a new one. */
        draw_generic_scrollbars();
        /* 2px theme-secondary window frame in a dedicated margin,
         * drawn LAST, right before the present. */
        {
            {
                const char *fc = (g_drop_highlight && g_drop_highlight_color[0])
                    ? g_drop_highlight_color
                    : (g_theme_fg[0] ? g_theme_fg : "#888888");
                XSetForeground(dpy, gc, alloc_pixel(fc));
                if (g_drop_highlight) {
                    int i;
                    XFillRectangle(dpy, buf, gc, 0, 0, (unsigned)g_win_w, 28);
                    XSetLineAttributes(dpy, gc, 3, LineOnOffDash, CapButt, JoinMiter);
                    for (i = 4; i <= 10; i += 3)
                        XDrawRectangle(dpy, buf, gc, i, i,
                                       (unsigned)(g_win_w - 1 - 2 * i), (unsigned)(g_win_h - 1 - 2 * i));
                    XSetLineAttributes(dpy, gc, 1, LineSolid, CapButt, JoinMiter);
                    if (font_ui && xftdraw_buf) {
                        char banner[160];
                        XftColor tcol;
                        XRenderColor xr;
                        xr.red = 0; xr.green = 0; xr.blue = 0; xr.alpha = 0xffff;
                        XftColorAllocValue(dpy, DefaultVisual(dpy, DefaultScreen(dpy)),
                                           DefaultColormap(dpy, DefaultScreen(dpy)), &xr, &tcol);
                        snprintf(banner, sizeof(banner), "[ drop: %s ]",
                                 g_drop_hover_name[0] ? g_drop_hover_name : "...");
                        XftDrawStringUtf8(xftdraw_buf, &tcol, font_ui, 12, 20,
                                          (const FcChar8 *)banner, (int)strlen(banner));
                        XftColorFree(dpy, DefaultVisual(dpy, DefaultScreen(dpy)),
                                     DefaultColormap(dpy, DefaultScreen(dpy)), &tcol);
                    }
                } else {
                    int _fb;
                    for (_fb = 0; _fb < KH_WIN_FRAME; _fb++)
                        XDrawRectangle(dpy, buf, gc, _fb, _fb,
                                       (unsigned)(g_win_w - 1 - 2 * _fb), (unsigned)(g_win_h - 1 - 2 * _fb));
                }
            }
        }
        {
            XImage *frame = XGetImage(dpy, buf, 0, 0, (unsigned)g_win_w, (unsigned)g_win_h, AllPlanes, ZPixmap);
            if (frame) {
                XPutImage(dpy, win, gc, frame, 0, 0, 0, 0, (unsigned)g_win_w, (unsigned)g_win_h);
                XDestroyImage(frame);
            }
        }
        win = winh; buf = bufh; xftdraw_buf = xh; gc = gch;
        g_win_w = ww; g_win_h = hh;
        g_dock_in_menu_paint = 0;
    }
}

/* interact-mode-style scope confinement. g_nav[] / nav_index are
 * immutable render data (every interactive row keeps its number).
 * Out-of-scope rows stay on screen with [ ]N. — inert until Esc.
 * Port of chtpm_parser.c: skip-scan + is_navigable(), not a rebuild. */
static void kh_focus_first_in_scope(void) {
    int i;
    if (!(g_default_scope_confine && g_n_nav > 0)) return;
    if (g_focus_nav >= 1 && g_focus_nav <= g_n_nav &&
        kh_elem_in_scope(g_nav[g_focus_nav - 1])) return;
    for (i = 0; i < g_n_nav; i++) {
        if (kh_elem_in_scope(g_nav[i])) { g_focus_nav = i + 1; return; }
    }
}
static void kh_focus_first_child_in_scope(void) {
    int i;
    if (!(g_default_scope_confine && g_n_nav > 0)) return;
    for (i = 0; i < g_n_nav; i++) {
        Elem *e = g_nav[i];
        if (!kh_elem_in_scope(e)) continue;
        if (e == g_default_active_scope_root) continue;
        if (e->id[0] && g_default_active_scope_id[0] &&
            strcmp(e->id, g_default_active_scope_id) == 0) continue;
        g_focus_nav = i + 1;
        return;
    }
    kh_focus_first_in_scope();
}
/* TPMOS is_navigable() counts the ACTIVATE root so it stays numbered,
 * but wrapping arrows onto that root is the parser bug where [>]
 * vanishes from the submenu. Arrow stops are descendants + chrome
 * (chrome is numbered as part of the submenu so X/!/_ stay reachable).
 * The [^] trigger itself is not an arrow stop. */
static int kh_elem_arrow_stop(Elem *e) {
    if (!e || !kh_elem_in_scope(e)) return 0;
    if (!g_default_scope_confine) return 1;
    if (e == g_default_active_scope_root) return 0;
    if (e->id[0] && g_default_active_scope_id[0] &&
        strcmp(e->id, g_default_active_scope_id) == 0) return 0;
    return 1;
}
static void kh_nav_step(int dir) {
    int prev, n;
    if (g_n_nav < 1) return;
    if (!g_default_scope_confine) {
        int nv = g_focus_nav + dir;
        if (nv >= 1 && nv <= g_n_nav) g_focus_nav = nv;
        return;
    }
    prev = g_focus_nav;
    n = g_n_nav;
    do {
        g_focus_nav += dir;
        if (g_focus_nav < 1) g_focus_nav = n;
        if (g_focus_nav > n) g_focus_nav = 1;
    } while (g_focus_nav != prev && !kh_elem_arrow_stop(g_nav[g_focus_nav - 1]));
}
static void kh_apply_scope_confine(void) {
    /* Do not shrink g_nav[] or zero nav_index. Snap focus if the
     * current row fell out of the live predicate. */
    if (!(g_default_scope_confine && g_default_active_scope_root && !window_is_dock()))
        return;
    if (g_focus_nav >= 1 && g_focus_nav <= g_n_nav &&
        kh_elem_arrow_stop(g_nav[g_focus_nav - 1])) return;
    kh_focus_first_child_in_scope();
}

/* REAL, NEW 2026-09-04 - see g_interact_relay_on's own declaration
 * comment. Scans the current page's direct <item> children (never the
 * whole tree - a relay trigger is always a real, top-level toolbar-ish
 * item, same shape the chrome/canvas scans right above already use)
 * for one carrying BOTH a non-empty relay= and class="interact-active".
 * Arms if found and not already armed; disarms if not found and
 * currently armed - so simply not publishing that class on the next
 * reparse (the projector's own real signal that the external process
 * disengaged) is what turns keyboard ownership back over to this
 * window, never a local guess. */
static void kh_scan_interact_relay(void) {
    Elem *pg = find_page(g_current_page);
    Elem *found = NULL;
    Elem *any_relay = NULL;
    if (pg) {
        /* direct page children, AND one level into a layout container
         * (sidebar / tabbar / footer) - milestone A moved the board's
         * relay trigger from a flat page <item> into a <sidebar>/
         * <tabbar> toolbar. Accept <item> or <tab>. */
        for (int i = 0; i < pg->n_children && !found; i++) {
            Elem *c = pg->children[i];
            int is_container = (strcmp(c->tag, "sidebar") == 0 ||
                                strcmp(c->tag, "tabbar") == 0 ||
                                strcmp(c->tag, "footer") == 0);
            int lo = is_container ? 0 : -1;
            int hi = is_container ? c->n_children : 0;
            for (int j = lo; j < hi; j++) {
                Elem *it = (j < 0) ? c : c->children[j];
                if ((strcmp(it->tag, "item") != 0 && strcmp(it->tag, "tab") != 0) || !it->relay[0]) continue;
                if (!any_relay) any_relay = it;
                if (elem_has_class(it, "interact-active")) { found = it; break; }
            }
        }
    }
    if (!found) found = any_relay;
    /* Arm from projector vars even when the Elem class is stale
     * (vars-hash reparse not firing — pc-hq-leg-vs-nu-fix.md §6b). */
    const char *ic = kh_get_var("interact_class");
    const char *ia = kh_get_var("interact_armed");
    int var_armed = (ic && strstr(ic, "interact-active")) ||
                    (ia && ia[0] == '1');
    int class_armed = found && elem_has_class(found, "interact-active");
    const char *h1 = kh_get_var("bv_h1");
    const char *h2 = kh_get_var("bv_h2");
    if (class_armed || var_armed) {
        char paths[PATH_BUF * 2];
        if (h1 && h1[0]) {
            if (h2 && h2[0]) snprintf(paths, sizeof(paths), "%s,%s", h1, h2);
            else snprintf(paths, sizeof(paths), "%s", h1);
        } else if (found) {
            snprintf(paths, sizeof(paths), "%s", found->relay);
        } else {
            g_interact_relay_on = 0;
            g_interact_relay_raw[0] = '\0';
            return;
        }
        if (!g_interact_relay_on || strcmp(g_interact_relay_raw, paths) != 0) {
            snprintf(g_interact_relay_raw, sizeof(g_interact_relay_raw), "%s", paths);
            g_interact_relay_n = 0;
            char work[PATH_BUF * 2];
            snprintf(work, sizeof(work), "%s", paths);
            char *save = NULL, *tok = strtok_r(work, ",", &save);
            while (tok && g_interact_relay_n < 2) {
                snprintf(g_interact_relay_paths[g_interact_relay_n], sizeof(g_interact_relay_paths[0]), "%s", tok);
                g_interact_relay_n++;
                tok = strtok_r(NULL, ",", &save);
            }
        }
        /* REVERTED 2026-09-04 - see pc-hq-bugs.md Bug 2 update. The
         * XGrabKeyboard this comment used to describe was a real,
         * confirmed regression: a keyboard grab is DISPLAY-WIDE (this
         * file's own line ~9192 comment already warns about exactly
         * this class of problem) and only ever released by an explicit
         * XUngrabKeyboard on the on->off transition below - which
         * depends on the SAME projector/reparse cycle already known
         * fragile (Bug 3's root cause). Live-confirmed: once armed,
         * EVERY other window in the house (including the taskbar
         * itself) lost real keyboard input until the pc-hq process was
         * killed outright, severing its X connection (the only other
         * way an X grab releases). Killing the one window broke
         * keyboard input for the entire desktop - far worse than the
         * bug it was meant to fix. Do not re-add this without solving
         * the disarm-reliability problem FIRST, and prefer a per-
         * window `XSetInputFocus` reassertion over a display-wide
         * grab if it needs revisiting at all. */
        g_interact_relay_on = 1;
    } else {
        g_interact_relay_on = 0;
        g_interact_relay_raw[0] = '\0';
        g_interact_disengage_sent = 0;
    }
}

static int kh_interact_vars_on(void) {
    const char *ic = kh_get_var("interact_class");
    const char *ia = kh_get_var("interact_armed");
    return (ic && strstr(ic, "interact-active")) || (ia && ia[0] == '1');
}

static void kh_interact_append_13(void) {
    int n = g_interact_relay_n;
    const char *paths[2];
    char h1buf[PATH_BUF], h2buf[PATH_BUF];
    if (n > 0) {
        for (int i = 0; i < n; i++) paths[i] = g_interact_relay_paths[i];
    } else {
        const char *h1 = kh_get_var("bv_h1");
        const char *h2 = kh_get_var("bv_h2");
        n = 0;
        if (h1 && h1[0]) { snprintf(h1buf, sizeof(h1buf), "%s", h1); paths[n++] = h1buf; }
        if (h2 && h2[0]) { snprintf(h2buf, sizeof(h2buf), "%s", h2); paths[n++] = h2buf; }
    }
    for (int i = 0; i < n; i++) {
        if (!paths[i] || !paths[i][0]) continue;
        FILE *f = fopen(paths[i], "a");
        if (!f) continue;
        /* pchq-vs-tpmos.md D2: keyboard/history.txt needs the
         * KEY_PRESSED: prefix or the board_viewer.chtpm parser ignores
         * it (so grok's FocusOut auto-disengage silently did nothing). */
        if (strstr(paths[i], "keyboard/history.txt")) fprintf(f, "KEY_PRESSED: 13\n");
        else                                          fprintf(f, "13\n");
        fclose(f);
    }
}

static void kh_interact_disengage_engine_if_on(void) {
    if (g_interact_disengage_sent) return;
    if (!kh_interact_vars_on() && !g_interact_relay_on) return;
    kh_interact_append_13();
    g_interact_disengage_sent = 1;
}

static void kh_interact_engage_if_needed(void) {
    if (kh_interact_vars_on() || g_interact_relay_on) return;
    kh_interact_append_13();
}

static int kh_page_has_relay_item(void) {
    Elem *pg = find_page(g_current_page);
    if (!pg) return 0;
    for (int i = 0; i < pg->n_children; i++) {
        Elem *c = pg->children[i];
        if ((strcmp(c->tag, "item") == 0 || strcmp(c->tag, "tab") == 0) && c->relay[0])
            return 1;
        /* milestone A: the board's relay trigger moved into a <tabbar>/
         * <sidebar>/<footer> toolbar - look one level in. */
        if (strcmp(c->tag, "tabbar") == 0 || strcmp(c->tag, "sidebar") == 0 ||
            strcmp(c->tag, "footer") == 0) {
            for (int j = 0; j < c->n_children; j++) {
                Elem *it = c->children[j];
                if ((strcmp(it->tag, "item") == 0 || strcmp(it->tag, "tab") == 0) && it->relay[0])
                    return 1;
            }
        }
    }
    return 0;
}

static int kh_canvas_hit(int px, int py) {
    Elem *pg = find_page(g_current_page);
    if (!pg) return 0;
    for (int i = 0; i < pg->n_children; i++) {
        Elem *it = pg->children[i];
        if (strcmp(it->tag, "canvas") != 0) continue;
        if (px >= it->x && px < it->x + it->w && py >= it->y && py < it->y + it->h)
            return 1;
    }
    return 0;
}

static void assign_nav_and_layout(void) {
    g_has_canvas = 0;
    g_canvas_chrome_left_x = 0;
    /* REAL FIX 2026-09-04 - the window-frame block at the end of this
     * function does `g_win_w/h += 2*KH_WIN_FRAME` every call. Branches
     * that recompute g_win_w/h from content (sidebar+panel, persistent
     * swatch grid) are fine, but the flat-list / entity-menu popup
     * branch leaves g_win_w fixed, so the +frame COMPOUNDED every
     * redraw (context menu marched off-screen + pegged CPU on the
     * resize->expose->relayout loop). Undo the previous pass's frame
     * grow up front so it's idempotent regardless of which branch runs.
     * g_win_frame_applied guards against undoing what was never added
     * (dock, first call). */
    static int g_win_frame_applied = 0;
    if (g_win_frame_applied) {
        g_win_w -= 2 * KH_WIN_FRAME;
        g_win_h -= 2 * KH_WIN_FRAME;
        g_win_frame_applied = 0;
    }
    /* REAL Stage 5 §5d.10 (2026-08-16) - db-hq mode branch, real WM-
     * managed window shape, own layout/nav functions (ported verbatim,
     * not forced into the popup modes' page/item shape below). */
    g_n_nav = 0;
    g_dock_header_nav_hi = 0;
    g_dock_drop_lo = 0;
    g_dock_drop_hi = 0;
    Elem *page = find_page(g_current_page);
    if (page && layout_dock_bar(page)) {
        g_dock_header_nav_hi = g_n_nav;
        if (g_dock_peer) {
            int hx = g_win_x, hy = g_win_y, hw = g_win_w, hh = g_win_h;
            Elem *hold = g_window;
            g_window = g_dock_peer;
            {
                Elem *pp = find_page(g_current_page);
                if (pp) layout_dock_bar(pp);
            }
            g_dock_peer_x = g_win_x; g_dock_peer_y = g_win_y;
            g_dock_peer_w = g_win_w; g_dock_peer_h = g_win_h;
            g_window = hold;
            g_win_x = hx; g_win_y = hy; g_win_w = hw; g_win_h = hh;
            if (g_window) { g_window->w = hw; g_window->h = hh; }
        }
        if (g_focus_nav < 1 || g_focus_nav > g_n_nav)
            g_focus_nav = g_n_nav > 0 ? 1 : 0;
        if (g_dock_drop_lo && g_default_active_scope_id[0] &&
            (g_focus_nav < g_dock_drop_lo || g_focus_nav > g_dock_drop_hi))
            g_focus_nav = g_dock_drop_lo;
        return;
    }
    if (!page) { g_win_h = CHROME_H + 8; return; }
    /* REAL FIX 2026-09-01 (found live, chat-hai's own migration onto
     * this path - a plain <item action=...>'s Pause/Speed control
     * silently closed the WHOLE window, and open-hai's own Sound/Model
     * toggle items were found independently dead with the same
     * signature): dispatch()'s own g_quit=1 tail ("real menus close
     * after a real action fires") is correct for this mode's original
     * context-menu use case, but wrong for a genuinely persistent
     * sidebar+panel window, where a plain control button firing should
     * never close the app. g_default_has_sidebar_panel latches true the
     * first time this real dual-region layout is used, and dispatch()
     * checks it before quitting - see that function's own tail. */
    if (layout_sidebar_panel(page)) {
        /* REAL FIX 2026-09-03 (direct live report: "do this for the
         * other windows" - open-hai/chat-hai stayed at the shared
         * hq_ui.pdl 100,160 despite main()'s own find_by_tag(sidebar)/
         * (panel) check, co-lab-hai got the real 80,80 fine) - root
         * cause: that main()-time check runs against the renderer's
         * ONE-TIME initial parse, which can still be a bootstrap
         * placeholder skeleton with no real <sidebar>/<panel> yet
         * (open-hai/chat-hai's own manager writes its real content on
         * its first tick, a real race the button.sh "lost its <module>
         * tag - restoring from bootstrap" message makes worse, not
         * better). This runs here instead - the first time this
         * function EVER actually detects real sidebar+panel content,
         * on ANY tick, live reparse included - no race possible, this
         * IS the real detection. g_default_has_sidebar_panel latching
         * true is the correct "first time" signal already used one
         * line below for a different real fix, same real idea reused. */
        if (!g_default_has_sidebar_panel) { g_win_x = 80; g_win_y = g_win_top_y; }
        g_default_has_sidebar_panel = 1;
        if (g_dock_drop_lo && g_default_active_scope_id[0] &&
            (g_focus_nav < g_dock_drop_lo || g_focus_nav > g_dock_drop_hi))
            g_focus_nav = g_dock_drop_lo;
        /* REAL FIX 2026-09-03 - the sidebar+panel window shape (db-hq-pal
         * <tab> scope) needs the same interact-mode nav confinement the
         * fallthrough branch below gets; without this the [^] tab lock
         * never took (all 15 tabs stayed navigable after activating one). */
        kh_apply_scope_confine();
        if (g_focus_nav > g_n_nav) g_focus_nav = g_n_nav > 0 ? g_n_nav : 1;
        if (g_focus_nav < 1) g_focus_nav = 1;
        return;
    }
    generic_sbar_reset();
    {
        int i, grid = 0;
        for (i = 0; i < page->n_children; i++) {
            Elem *item = page->children[i];
            int c;
            if (strcmp(item->tag, "item") != 0 && strcmp(item->tag, "cli_io") != 0) continue;
            for (c = 0; c < item->n_classes; c++)
                if (strcmp(item->classes[c], "swatch") == 0) { grid = 1; break; }
            if (grid) break;
        }
        if (grid) {
        /* Grid is data: any <item class="swatch">. Not g_is_swatch_picker.
         * Matches the pre-port rmmv picker layout (see git @94d12680
         * dbhq_layout_pass): a WIDE window whose column count is derived
         * from its width; folder + sheet choosers wrap in a strip ABOVE
         * the grid, tileset choosers pinned as a footer BELOW it; the
         * grid clips to a fixed row count and gets a generic_sbar
         * (draggable thumb + nav-numbered ^/v arrows) at its right edge.
         * All keyed on the class STRING (pal-tileset vs. the rest) - no
         * app names in here. */
        int x0 = 12;
        int pitch = SWATCH + SWATCH_GAP;
        /* wide, screen-relative, like the old window - only for a real
         * persistent palette/db window, not a transient swatch popup */
        if (g_default_persistent) {
            int scr_w = kh_screen_w();
            int want = (scr_w * 5) / 8;
            if (want > 1180) want = 1180;
            if (want < 460) want = 460;
            /* never run off the right of the screen from wherever the
             * window currently sits (no clamp elsewhere on this path) */
            if (g_win_x < 0) g_win_x = 0;
            if (g_win_x + want > scr_w - 16) {
                g_win_x = scr_w - 16 - want;
                if (g_win_x < 0) { g_win_x = 0; want = scr_w - 16; }
            }
            g_win_w = want;
            if (g_window) g_window->w = g_win_w;
        }
        /* scrollbar region right edge kept a comfortable 20px inside the
         * window so the track + thumb never crowd the frame/screen edge */
        int sbar_right = g_win_w - 20;
        int cols = (sbar_right - x0 - GENERIC_SCROLLBAR_W - 4) / pitch;
        if (cols < 1) cols = 1;
        int grid_w = cols * pitch;
        int tile_w = SWATCH, tile_h = SWATCH;
        int pitch_x = pitch, pitch_y = pitch;
        int n_sw = 0;
        int chrome_x = g_win_w - 8;   /* right-to-left cursor for chrome buttons */
        int found_close = 0;
        Elem *sw_items[MAX_CHILDREN];
        for (i = 0; i < page->n_children; i++) {
            Elem *item = page->children[i];
            int is_sw = 0, is_close = 0, c;
            /* cli_io joins the chip row like an item (nav-numbered, styled) so a
             * toolbar text field (File Explorer Search) survives grid mode. */
            if (strcmp(item->tag, "item") != 0 && strcmp(item->tag, "cli_io") != 0) continue;
            for (c = 0; c < item->n_classes; c++) {
                if (strcmp(item->classes[c], "swatch") == 0) is_sw = 1;
                if (strcmp(item->classes[c], "close-btn") == 0 ||
                    strcmp(item->classes[c], "chrome-btn") == 0) is_close = 1;
            }
            if (strcmp(item->id, "close") == 0) is_close = 1;
            if (is_close) found_close = 1;
            css_compute_style(&g_sheet, item->tag, item->id, item->classes, item->n_classes, 0, &item->style);
            item->nav_index = ++g_n_nav;
            g_nav[g_n_nav - 1] = item;
            if (is_close) {
                /* draw_elem prepends "[ ]NN. " before the label, so the
                 * box must be wide enough for the badge + the label or
                 * the glyph spills off the right edge. Then hard-clamp
                 * fully inside the window so it can never fall off. */
                int cw = kh_measure_text_px(&item->style, item->label) + 52;
                if (cw < 48) cw = 48;
                chrome_x -= cw;
                item->y = 2; item->w = cw; item->h = CHROME_H - 4;
                item->x = chrome_x;
                kh_clamp_elem_onscreen(item);
                chrome_x = item->x - 4;
            } else if (is_sw) {
                if (n_sw < 12) {
                    snprintf(g_palette_name_buf[n_sw], sizeof(g_palette_name_buf[n_sw]), "%s", item->label);
                    g_palette_name[n_sw] = g_palette_name_buf[n_sw];
                }
                /* Palettes: sprite IS the cell; wiping label is
                 * intentional. File-explorer grid (2026-09-18): no
                 * sprite, label is icon+name+size same as list mode.
                 * Blanking those made a field of empty (often yellow)
                 * 34px tiles. Keep the label when there is no sprite. */
                if (item->sprite[0])
                    item->label[0] = '\0';
                if (n_sw < MAX_CHILDREN) sw_items[n_sw] = item;
                n_sw++;
            }
        }
        {
            int labeled_sw = 0;
            int s;
            for (s = 0; s < n_sw && s < MAX_CHILDREN; s++) {
                if (sw_items[s]->label[0] && !sw_items[s]->sprite[0]) {
                    labeled_sw = 1;
                    break;
                }
            }
            if (labeled_sw) {
                tile_w = 176;
                tile_h = 40;
                pitch_x = tile_w + SWATCH_GAP;
                pitch_y = tile_h + SWATCH_GAP;
                cols = (sbar_right - x0 - GENERIC_SCROLLBAR_W - 4) / pitch_x;
                if (cols < 1) cols = 1;
                grid_w = cols * pitch_x;
            }
        }
        /* Every real HQ/picker window MUST have a way out (direct)
         * instruction: "make sure x11-hq windows all have a default x
         * button so they don't get stuck on screen"). If the template
         * declared none, synthesise the same g_default_close_elem the
         * sidebar+panel path uses - drawn/clicked/serialised through the
         * exact machinery that already exists for it. */
        {
            int cy = 2; /* chrome on the nametag's former row; entity-menu draws nametag below */
            if (!found_close)
                kh_place_chrome_btn(g_default_close_elem, "chrome-close", "X", "CLOSE", &chrome_x, cy);
            else
                g_default_close_elem->w = 0;
            kh_place_chrome_btn(g_default_fullscreen_elem, "chrome-fullscreen", "!", "TOGGLE_FULLSCREEN", &chrome_x, cy);
            kh_place_chrome_btn(g_default_minimize_elem, "chrome-minimize", "_", "MINIMIZE", &chrome_x, cy);
        }
        /* helper: does this <item> carry class="pal-dir" (the long folder
         * list - pinned to the FOOTER; sheet A/B/C + tileset choosers go
         * up top, next to the grid, since they're picked far more often) */
        #define KH_IS_FOOTER_CHIP(it_) ({ int _t = 0; for (int _c = 0; _c < (it_)->n_classes; _c++) \
            if (strcmp((it_)->classes[_c], "pal-dir") == 0) { _t = 1; break; } _t; })
        const int KH_CHIP_ROW_GAP = 10;   /* breathing room between wrapped chooser rows */
        /* pass 2a: sheet (A/B/C) + tileset choosers, above the grid,
         * each family on its own row */
        int chips_top = CHROME_H + 6;
        {
            int crumb_h = 0;
            for (i = 0; i < page->n_children; i++) {
                Elem *tb = page->children[i];
                if (strcmp(tb->tag, "tabbar") != 0 || tb->n_children <= 0) continue;
                crumb_h += kh_layout_tabbar_wrap(tb, x0, CHROME_H + crumb_h);
            }
            if (crumb_h) chips_top = CHROME_H + crumb_h + 6;
        }
        {
            int cx = x0, cy = chips_top;
            const char *prev_fam = NULL;
            for (i = 0; i < page->n_children; i++) {
                Elem *item = page->children[i];
                int is_sw = 0, is_close = 0, c;
                if (strcmp(item->tag, "item") != 0 && strcmp(item->tag, "cli_io") != 0) continue;
                for (c = 0; c < item->n_classes; c++) {
                    if (strcmp(item->classes[c], "swatch") == 0) is_sw = 1;
                    if (strcmp(item->classes[c], "close-btn") == 0 ||
                        strcmp(item->classes[c], "chrome-btn") == 0) is_close = 1;
                }
                if (strcmp(item->id, "close") == 0) is_close = 1;
                if (is_sw || is_close || KH_IS_FOOTER_CHIP(item)) continue;
                const char *fam = item->n_classes ? item->classes[0] : "";
                /* room for draw_elem's "[ ]NN. " nav badge (~46px) PLUS
                 * the whole label, or the CSS width if it asks for more -
                 * a too-narrow chip clipped "A"/"B" to just the badge. */
                int w = kh_measure_text_px(&item->style, item->label) + 46;
                if (item->style.has_width && item->style.width > w) w = item->style.width;
                if (w < 44) w = 44;
                if (strcmp(item->tag, "cli_io") == 0 && w < 280) w = 280; /* room to type */
                int rh = item->style.has_height ? item->style.height : ROW_H;
                if (prev_fam && strcmp(prev_fam, fam) != 0) { cx = x0; cy += rh + KH_CHIP_ROW_GAP; }
                else if (cx > x0) cx += 6;
                if (cx > x0 && cx + w > g_win_w - 8) { cx = x0; cy += rh + KH_CHIP_ROW_GAP; }
                item->x = cx; item->y = cy; item->w = w; item->h = rh;
                cx += w;
                prev_fam = fam;
            }
            if (cx > x0 || cy > chips_top) chips_top = cy + ROW_H + KH_CHIP_ROW_GAP;
        }
        /* pass 3: scrolled swatch grid, below the header strip */
        /* REAL FIX 2026-09-04, direct live report ("settings buttons
         * overlap the color buttons") - confirmed via frame-dump
         * coordinates, not guessed: a header chip row (e.g. taskbar-
         * settings-pal's own Opacity -/+ buttons) sits at y=[32,56),
         * chips_top pushed the grid's own tile boxes down to y=66 -
         * the TILE rects themselves never touched (10px clear) - but
         * each swatch tile's own nav badge is drawn ABOVE the tile
         * (khtpm_draw_core.c's `is_swatch_tile` branch: `numy_above =
         * e->y - 2 - descent`, chip spanning roughly 16-18px above
         * e->y), landing at y=[50,65] - a real ~6px visual collision
         * with the chip row's own bottom edge that chips_top's own
         * ROW_H+KH_CHIP_ROW_GAP spacing never accounted for. Real fix:
         * reserve that same badge headroom before the grid starts,
         * not just row-height + gap - same constant the badge draw
         * itself effectively uses (ascent+descent+padding), rounded
         * up. Swatch grids with no header chips are unaffected
         * (chips_top == CHROME_H+6 either way; this headroom is
         * needed either way since row 1's own badges always draw
         * above the first tile row too - matches the pre-existing
         * fixed 12px top margin that already worked fine when nothing
         * shared this space, just wasn't enough once a header row
         * pushed the grid down to a tighter starting position). */
        #define KH_SWATCH_BADGE_HEADROOM 18
        int y0 = chips_top + KH_SWATCH_BADGE_HEADROOM;
        static int g_swatch_grid_scroll = 0;
        int total_rows = (n_sw + cols - 1) / cols;
        int visible_rows = 12;
        if (visible_rows > total_rows) visible_rows = total_rows;
        if (visible_rows < 1) visible_rows = 1;
        int max_scroll = total_rows - visible_rows;
        if (max_scroll < 0) max_scroll = 0;
        if (g_swatch_grid_scroll > max_scroll) g_swatch_grid_scroll = max_scroll;
        if (g_swatch_grid_scroll < 0) g_swatch_grid_scroll = 0;
        int view_h = visible_rows * pitch_y;
        for (int s = 0; s < n_sw && s < MAX_CHILDREN; s++) {
            Elem *it = sw_items[s];
            int col = s % cols, row = s / cols - g_swatch_grid_scroll;
            it->w = tile_w; it->h = tile_h;
            if (row < 0 || row >= visible_rows) { it->w = 0; it->h = 0; it->x = 0; it->y = -100000; }
            else { it->x = x0 + col * pitch_x; it->y = y0 + row * pitch_y; }
        }
        if (max_scroll > 0)
            generic_sbar_register(x0, y0, sbar_right - x0, view_h,
                                  &g_swatch_grid_scroll, total_rows, visible_rows, max_scroll);
        int max_y = y0 + view_h;
        /* pass 2b: the folder chooser (long list) pinned as a footer below the grid */
        {
            int foot0 = max_y + KH_CHIP_ROW_GAP + 4;
            int cx = x0, cy = foot0;
            for (i = 0; i < page->n_children; i++) {
                Elem *item = page->children[i];
                if (strcmp(item->tag, "item") != 0) continue;
                if (!KH_IS_FOOTER_CHIP(item)) continue;
                /* room for draw_elem's "[ ]NN. " nav badge (~46px) PLUS
                 * the whole label, or the CSS width if it asks for more -
                 * a too-narrow chip clipped "A"/"B" to just the badge. */
                int w = kh_measure_text_px(&item->style, item->label) + 46;
                if (item->style.has_width && item->style.width > w) w = item->style.width;
                if (w < 44) w = 44;
                int rh = item->style.has_height ? item->style.height : ROW_H;
                if (cx > x0) cx += 6;
                if (cx > x0 && cx + w > g_win_w - 8) { cx = x0; cy += rh + KH_CHIP_ROW_GAP; }
                item->x = cx; item->y = cy; item->w = w; item->h = rh;
                cx += w;
            }
            if (cx > x0 || cy > foot0) max_y = cy + ROW_H;
        }
        #undef KH_IS_FOOTER_CHIP
        g_win_h = max_y + 8;
        } else {
        /* A page carrying a <canvas> is a live-framebuffer window (the
         * piececraft-hq board). It gets a horizontal toolbar row, top-
         * strip chrome-close placement, and a canvas sized to the
         * framebuffer's own dims. A plain flat list - context menus,
         * bootstrap skeletons, chat-hai's fallback - has NO <canvas> and
         * takes the ORIGINAL stacked-row path below BYTE-FOR-BYTE (this
         * branch is shared by every windowless-list window; see the
         * 2026-09-04 context-menu runaway/regression). */
        int has_canvas = 0;
        for (int _i = 0; _i < page->n_children; _i++)
            if (strcmp(page->children[_i]->tag, "canvas") == 0) { has_canvas = 1; break; }

        /* Same <tabbar> strip layout_sidebar_panel() uses (db-hq /
         * csv-hq). Canvas pages used to skip it, so a skeleton-3 toy
         * could not put New/Demo/tools on a horizontal tab row.
         *
         * REAL FIX 2026-09-15, direct live report ("we dont need a
         * sidbar at all" / "thats a different legacy bug that we can
         * remove") - this block used to be gated on has_canvas only;
         * generalized to run for ANY flat page with a real <tabbar>
         * child, canvas or not - a plain flat list (file-explorer's
         * own real breadcrumb row) needs the exact same horizontal tab
         * strip a canvas page already gets, no real reason to keep it
         * canvas-only. Safe, backward-compatible: the inner loop only
         * ever acts on children whose tag=="tabbar", so any page with
         * no real <tabbar> at all still gets canvas_tabbar_h=0, a
         * true no-op, unchanged from before. */
        int canvas_tabbar_h = 0;
        {
            for (int ci = 0; ci < page->n_children; ci++) {
                Elem *tabbar = page->children[ci];
                if (strcmp(tabbar->tag, "tabbar") != 0 || tabbar->n_children <= 0) continue;
                canvas_tabbar_h += kh_layout_tabbar_wrap(tabbar, scaled(6), CHROME_H + canvas_tabbar_h);
            }
        }

        int y = CHROME_H + canvas_tabbar_h;
        if (window_is_entity_menu()) y = CHROME_H * 2; /* chrome row, nametag row, then items */
        int chrome_x = g_win_w - 8;      /* has_canvas only */
        int row_x = 0, row_h = 0;        /* has_canvas horizontal-row cursor */
        int found_close = 0;
        for (int i = 0; i < page->n_children; i++) {
            Elem *item = page->children[i];
            /* REAL FIX 2026-09-15, direct live report ("we dont need a
             * sidbar at all" - file-explorer's own list needed real
             * scrolling, which this flat-page path never had at all
             * before now: a <scrolllist> here previously matched none
             * of this loop's own tag cases and was silently dropped -
             * its own children never positioned, never nav-numbered,
             * genuinely invisible, not just unscrolled). Delegates to
             * layout_scroll_region() - the same real, generic scroll
             * primitive layout_sidebar_panel()'s own panel-side
             * scrolllist already uses, reusing its exact scroll state
             * (g_default_scrolllist_scroll/nav_lo/nav_hi) rather than
             * new globals - safe because a window is either sidebar+
             * panel-shaped or flat-page-shaped, never both at once, so
             * there's no real risk of the two ever colliding. Fills
             * every real pixel left below the tabbar/chrome down to
             * the window's own bottom edge - a flat page's own list IS
             * the whole window, not one region sharing it with a
             * sidebar. */
            if (strcmp(item->tag, "scrolllist") == 0) {
                int room = g_win_h - y - 8;
                if (room < ROW_H) room = ROW_H;
                css_compute_style(&g_sheet, item->tag, item->id, item->classes, item->n_classes, 0, &item->style);
                layout_scroll_region(item, 0, y, g_win_w, room,
                                      &g_default_scrolllist_scroll,
                                      &g_default_scrolllist_nav_lo, &g_default_scrolllist_nav_hi);
                y += room;
                continue;
            }
            /* 2026-08-31 - a plain <text> row advances y like an item row
             * (real vertical space) but is never nav-numbered. */
            int is_text = strcmp(item->tag, "text") == 0;
            if (strcmp(item->tag, "canvas") == 0) {
                /* live pixel framebuffer element (kh_draw_canvas): sized
                 * from CSS w/h, else the framebuffer's own receipt dims,
                 * else window-width x 360. Never nav-numbered. */
                g_has_canvas = 1;
                if (row_x) { y += row_h + 4; row_x = 0; row_h = 0; }
                css_compute_style(&g_sheet, item->tag, item->id, item->classes, item->n_classes, 0, &item->style);
                /* Live canvas_raw from projector vars — first parse can
                 * happen before ui.txt exists (click-open vs Enter after
                 * 1.5s). Without this the Elem sprite stays empty and
                 * kh_draw_canvas fills dark every tick. */
                {
                    const char *cr = kh_get_var("canvas_raw");
                    if (cr && cr[0])
                        snprintf(item->sprite, sizeof(item->sprite), "%s", cr);
                }
                int cw = item->style.has_width  ? item->style.width  : 0;
                int ch = item->style.has_height ? item->style.height : 0;
                if ((!cw || !ch) && item->sprite[0]) {
                    /* sibling <base>.receipt.txt, not <base>.raw.receipt.txt
                     * (matches kh_draw_canvas's own fix, same real bug -
                     * this is why the map never rendered: both guesses
                     * used the wrong receipt path so w/h stayed 0). */
                    char rp[512]; const char *dot = strrchr(item->sprite, '.');
                    if (dot && strcmp(dot, ".raw") == 0)
                        snprintf(rp, sizeof(rp), "%.*s.receipt.txt", (int)(dot - item->sprite), item->sprite);
                    else
                        snprintf(rp, sizeof(rp), "%s.receipt.txt", item->sprite);
                    FILE *rf = fopen(rp, "r");
                    if (rf) { char l[128];
                        while (fgets(l, sizeof(l), rf)) {
                            /* bv_render_3d overlay receipt: overlay_w/h.
                             * chtpm_rgb_render composited receipt
                             * (pchq-vs-muta.md B1): frame_w/h. */
                            if (!cw && !strncmp(l, "overlay_w=", 10)) cw = atoi(l + 10);
                            if (!ch && !strncmp(l, "overlay_h=", 10)) ch = atoi(l + 10);
                            if (!cw && !strncmp(l, "frame_w=", 8))    cw = atoi(l + 8);
                            if (!ch && !strncmp(l, "frame_h=", 8))    ch = atoi(l + 8);
                        }
                        fclose(rf);
                    }
                }
                if (!cw) cw = g_win_w - 12;
                if (!ch) ch = 360;
                item->x = 6; item->y = y; item->w = cw; item->h = ch;
                if (g_user_resizable) {
                    /* the canvas FILLS the window (below the toolbar) -
                     * resize the window, the map view resizes with it.
                     * The producing renderer is told this pixel size via
                     * #.desktop/pchq_board_view.txt (below) and renders
                     * exactly that, so kh_draw_canvas blits 1:1. */
                    item->x = 6;
                    item->w = g_win_w - 12;
                    item->h = g_win_h - item->y - 8;
                    if (item->w < 64) item->w = 64;
                    if (item->h < 64) item->h = 64;
                    char vsz[PATH_BUF];
                    snprintf(vsz, sizeof(vsz), "%s/#.desktop/pchq_board_view.txt", g_house_root);
                    FILE *vf = fopen(vsz, "w");
                    if (vf) { fprintf(vf, "%d %d\n", item->w, item->h); fclose(vf); }
                } else {
                    /* not user-owned: grow the window to the framebuffer */
                    if (cw + 12 > g_win_w) { g_win_w = cw + 12; g_window->w = g_win_w; }
                }
                y += item->h + 4;
                continue;
            }
            if (strcmp(item->tag, "item") != 0 && strcmp(item->tag, "cli_io") != 0 && strcmp(item->tag, "text_area") != 0 && !is_text) continue;
            css_compute_style(&g_sheet, item->tag, item->id, item->classes, item->n_classes, 0, &item->style);

            if (!is_text) {
                /* REAL FIX 2026-09-15, direct live report ("this one is
                 * missing chrome buttons" / "still no chrome standards")
                 * - this whole is_close block used to be has_canvas-only,
                 * so a flat, canvas-less page's own explicit chrome item
                 * (id="chrome-close"/class="chrome-btn"/"close-btn") was
                 * never detected as chrome at all - it just fell through
                 * into the plain full-width stacked-row path below like
                 * any other <item>. Generalized: chrome_x/top-right
                 * placement is real and correct for ANY flat page, not
                 * just canvas ones (chrome_x is initialized to g_win_w-8
                 * unconditionally above already); only the horizontal
                 * toolbar-ROW packing right below (has_width items)
                 * stays canvas-only, since that's a real canvas-toolbar-
                 * specific behavior a plain list never asked for. */
                int is_close = strcmp(item->id, "close") == 0 ||
                                strcmp(item->id, "chrome-close") == 0;
                for (int c = 0; c < item->n_classes && !is_close; c++)
                    if (strcmp(item->classes[c], "chrome-btn") == 0 ||
                        strcmp(item->classes[c], "close-btn") == 0) is_close = 1;
                if (is_close) {
                    found_close = 1;
                    int cw = kh_measure_text_px(&item->style, item->label) + 52;
                    if (cw < 48) cw = 48;
                    chrome_x -= cw;
                    item->y = 2; item->w = cw; item->h = CHROME_H - 4; item->x = chrome_x;
                    kh_clamp_elem_onscreen(item);
                    chrome_x = item->x - 4;
                    if (!g_canvas_chrome_left_x || item->x < g_canvas_chrome_left_x)
                        g_canvas_chrome_left_x = item->x;
                    item->nav_index = ++g_n_nav; g_nav[g_n_nav - 1] = item;
                    continue;
                }
            }
            /* REAL FIX 2026-09-15, direct live report ("can the back
             * button and grid view be on same row, instead of back
             * being tied to other files? get it?") - this CSS-width-
             * driven horizontal row packing used to be has_canvas-only,
             * same real gap as the is_close block above: a flat,
             * canvas-less page's own two toolbar items (Back/grid-
             * toggle, both declared width in CSS) had no way to share a
             * row at all - every plain <item> fell to the single-per-
             * row fallback further below. Generalized, same reasoning
             * as the chrome fix above. */
            if (!is_text) {
                if (item->style.has_width && item->style.width > 0) {
                    int iw = item->style.width;
                    int ih = item->style.has_height ? item->style.height : ROW_H;
                    if (row_x == 0) row_x = 6;
                    if (row_x + iw > g_win_w - 6) { y += row_h + 4; row_x = 6; row_h = 0; }
                    item->x = row_x; item->y = y; item->w = iw; item->h = ih;
                    row_x += iw + 5;
                    if (ih > row_h) row_h = ih;
                    item->nav_index = ++g_n_nav; g_nav[g_n_nav - 1] = item;
                    continue;
                }
                if (row_x) { y += row_h + 4; row_x = 0; row_h = 0; }
            }

            /* REAL FIX 2026-09-05 (found live testing <text_area>'s
             * own first real window) - this fallback ignored rows="N"
             * entirely for BOTH cli_io and text_area, always giving a
             * single ROW_H regardless - a pre-existing gap that never
             * surfaced for cli_io because every real cli_io consumer so
             * far (open-hai's composer) happens to live inside a
             * sidebar+panel window, which goes through layout_fixed_
             * rows_and_scrolllist()'s OWN, correct rows= handling
             * instead of this flat-list fallback path. */
            int is_multirow_field = (strcmp(item->tag, "cli_io") == 0 || strcmp(item->tag, "text_area") == 0);
            int item_h = is_multirow_field ? (item->rows > 0 ? item->rows : 1) * ROW_H : ROW_H;
            item->x = 0; item->y = y; item->w = g_win_w; item->h = item_h;
            if (!is_text) { item->nav_index = ++g_n_nav; g_nav[g_n_nav - 1] = item; }
            y += item_h;
        }
        if (row_x) y += row_h + 4;
        /* Same real "every window MUST have a way out" fallback the
         * swatch-grid path already has (~line 6026, "make sure x11-hq
         * windows all have a default x button") - a flat page that
         * declares no explicit chrome item at all now still gets one,
         * synthesized through the exact same g_default_close_elem
         * machinery, real id="chrome-close" CSS look included. */
        {
            int cy = 2; /* chrome on the nametag's former row; entity-menu draws nametag below */
            if (!found_close)
                kh_place_chrome_btn(g_default_close_elem, "chrome-close", "X", "CLOSE", &chrome_x, cy);
            else
                g_default_close_elem->w = 0;
            kh_place_chrome_btn(g_default_fullscreen_elem, "chrome-fullscreen", "!", "TOGGLE_FULLSCREEN", &chrome_x, cy);
            kh_place_chrome_btn(g_default_minimize_elem, "chrome-minimize", "_", "MINIMIZE", &chrome_x, cy);
        }
        /* user owns the height when class="user-resizable". No
         * "never clip content" fallback here: the canvas is sized to
         * fill exactly the space left below the toolbar
         * (item->h = g_win_h - item->y - 8), so content height always
         * ~= g_win_h and a `g_win_h < y+8 -> g_win_h = y+8` guard is a
         * +4/pass feedback loop (window crept to the screen edge). */
        if (!g_user_resizable) g_win_h = y + 8;
        }
    }
    if (!window_is_dock() && g_window) {
        int fb = KH_WIN_FRAME;
        Elem *pg = find_page(g_current_page);
        if (pg) for (int i = 0; i < pg->n_children; i++) kh_shift_subtree_xy(pg->children[i], fb, fb);
        /* synthetic chrome elems (not in the tree) */
        if (g_default_close_elem && g_default_close_elem->w > 0)      { g_default_close_elem->x += fb;      g_default_close_elem->y += fb; }
        if (g_default_minimize_elem && g_default_minimize_elem->w > 0) { g_default_minimize_elem->x += fb;   g_default_minimize_elem->y += fb; }
        if (g_default_fullscreen_elem && g_default_fullscreen_elem->w > 0) { g_default_fullscreen_elem->x += fb; g_default_fullscreen_elem->y += fb; }
        if (g_canvas_chrome_left_x) g_canvas_chrome_left_x += fb;  /* keep it in the same post-shift coords the ButtonPress handler sees */
        /* generic scrollbar geometry + its ^/v arrow elems */
        for (int i = 0; i < g_n_generic_sbars; i++) {
            g_generic_sbars[i].vx += fb; g_generic_sbars[i].vy += fb;
            g_generic_sbars[i].track_x += fb; g_generic_sbars[i].track_y += fb;
            g_generic_sbars[i].thumb_y += fb;
            if (g_sbar_up_elem[i].w > 0)   { g_sbar_up_elem[i].x += fb;   g_sbar_up_elem[i].y += fb; }
            if (g_sbar_down_elem[i].w > 0) { g_sbar_down_elem[i].x += fb; g_sbar_down_elem[i].y += fb; }
        }
        g_win_w += 2 * fb; g_win_h += 2 * fb;
        g_window->w = g_win_w; g_window->h = g_win_h;
        g_win_frame_applied = 1;   /* the undo at the top of the next pass reads this */
    }
    kh_scan_interact_relay();
    kh_apply_scope_confine();
    if (g_focus_nav > g_n_nav) g_focus_nav = g_n_nav > 0 ? g_n_nav : 1;
    if (g_focus_nav < 1) g_focus_nav = 1;
}

static void switch_page(const char *name) {
    if (!find_page(name)) return;
    snprintf(g_current_page, sizeof(g_current_page), "%s", name);
    g_focus_nav = 1;
}

/* Real dispatch - same shape as tp_desktop_window_rgb.c's own
 * dispatch_action(), ported not reinvented (this is a DIFFERENT process
 * so it can't call that function directly, but the semantics must match
 * exactly - CLOSE/void/GOTO:/BACK are handled here, everything else is a
 * real shell command run with package_dir/house_root as args, same
 * "%s '%s' '%s'" shape). */
static void dispatch(const char *action) {
    /* REAL, NEW 2026-09-03 (HQ-WINDOW-TASKBAR-ENTRIES-AND-MINIMIZE-2026-09-
     * 03.md §2.2) - HQ window taskbar entry click, handled LOCALLY in the
     * strip renderer (this process owns g_dpy; the taskbar manager is X-free
     * by design, per Grok's review on 2026-09-03: "click-to-focus belongs in
     * the STRIP RENDERER... dispatch() in the renderer does XRaiseWindow +
     * XSetInputFocus on that id"). Format: FOCUSWIN:0x<win>:<pid>. For a
     * NOT-minimized window: real XRaiseWindow (bring to top - focus alone
     * doesn't restack) + XSetInputFocus, both on the target window's own real
     * X id, which works cross-process. For a MINIMIZED one, the TARGET
     * process must map its own window (house rule: a process only touches
     * its own X resources), so read that window's registry file, and if
     * minimized, write the design doc's §3.2 restore-request file
     * (#.desktop/livedesk_hq_restore_<pid>.txt) the target renderer polls
     * and responds to by XMapWindow+XSetInputFocus on ITS OWN window. */
    if (strcmp(action, "PAGEROW:+1") == 0) {
        if (g_dock_visible_rows < g_dock_packed_rows) g_dock_visible_rows++;
        return;
    }
    if (strcmp(action, "PAGEROW:-1") == 0) {
        if (g_dock_visible_rows > 1) g_dock_visible_rows--;
        return;
    }
    if (strcmp(action, "FOOTER_ROWS:+1") == 0) {
        if (g_footer_vis_rows < g_footer_total_rows) g_footer_vis_rows++;
        return;
    }
    if (strcmp(action, "FOOTER_ROWS:-1") == 0) {
        if (g_footer_vis_rows > 1) g_footer_vis_rows--;
        return;
    }
    if (strncmp(action, "FOCUSWIN:", 9) == 0) {
        unsigned long w = 0; int pid = 0;
        if (sscanf(action + 9, "0x%lx:%d", &w, &pid) != 2) return;
        int minimized = 0;
        if (pid > 0) {
            char reg_path[PATH_BUF];
            snprintf(reg_path, sizeof(reg_path), "%s/#.desktop/livedesk_hq_windows_%d.txt", g_house_root, pid);
            FILE *rf = fopen(reg_path, "r");
            if (rf) {
                char rline[PATH_BUF];
                if (fgets(rline, sizeof(rline), rf) && strstr(rline, "minimized=1")) minimized = 1;
                fclose(rf);
            }
        }
        if (minimized) {
            char restore_path[PATH_BUF];
            snprintf(restore_path, sizeof(restore_path), "%s/#.desktop/livedesk_hq_restore_%d.txt", g_house_root, pid);
            FILE *wf = fopen(restore_path, "w");
            if (wf) { fprintf(wf, "restore=1\n"); fclose(wf); }
        } else {
            kh_raise_and_focus((Window)w);
        }
        return;
    }
    if (strcmp(action, "ZORDER_TOGGLE") == 0) {
        load_zorder_mode(g_house_root);
        g_zorder_above = !g_zorder_above;
        save_zorder_mode(g_house_root, g_zorder_above);
        g_override_redirect = g_zorder_above ? 1 : 0;
        kh_zorder_raise_dock(g_zorder_above);
        kh_spawn_zorder_op(g_zorder_above);
        /* REAL FIX 2026-09-13, direct live report ("we dont even need
         * to respawn the bottom toolbar tho for ontop... we dont need
         * to respawn top tb either. just entities") - correct: always-
         * on-top is real per-ENTITY window state (swa.override_redirect,
         * tp_main() only) - the strip's own two windows are
         * unconditionally WM-managed regardless of this setting (see
         * dock_managed in main()), so ktb_zorder_op.+x (spawned above)
         * now skips them entirely (real /proc cmdline identity check,
         * same one khtpm_taskbar_manager.c's own
         * livedesk_kill_strip_renderers() already uses). This @ button
         * only ever lives on the strip itself (khtpm_strip_header.chtpm,
         * grepped - no other window ever wires ZORDER_TOGGLE), so the
         * process running THIS handler is always the strip - it must
         * NOT g_quit anymore: nothing above it in
         * ktb_zorder_op.+x forks a replacement (the strip is excluded
         * from its respawn list), so setting g_quit here
         * would just kill the taskbar with nothing left to bring it
         * back. Re-apply the strip's own persistent EWMH ABOVE hint live
         * instead (XChangeProperty, no window recreation needed - unlike
         * override_redirect, _NET_WM_STATE is a plain property any
         * window can update after the fact) so its own always-above
         * preference stays in sync with the new g_zorder_above value
         * without needing to restart the process that just changed it. */
        if (window_is_dock()) {
            apply_dock_window_hints(dpy, win, g_win_x, g_win_y);
            if (g_dock_peer_win) apply_dock_window_hints(dpy, g_dock_peer_win, g_dock_peer_x, g_dock_peer_y);
            return;
        }
        g_quit = 1;
        return;
    }
    if (strncmp(action, "PICK:", 5) == 0) {
        char ap[PATH_BUF];
        snprintf(ap, sizeof(ap), "%s/#.desktop/taskbar_settings_action.txt", g_house_root);
        FILE *af = fopen(ap, "w");
        if (af) { fprintf(af, "seq=%u\n%s\n", ++g_swatch_action_seq, action); fclose(af); }
        return;
    }
    /* REAL, NEW 2026-09-05 (11.brainstorm/2026-09-05/PDL-READER-AND-
     * FILE-EXPLORER-WIDGET.md) - the File Explorer widget's own
     * dispatch verbs, same exact real "renderer writes an
     * incrementing-seq action file in-process, manager polls it"
     * convention PICK:<n> just above already uses - not a shell-
     * command relay (a static xhtpm template has no reliable way to
     * embed its own absolute install path for a relay script to call
     * with plain argv, which is why PICK:<n> is handled here instead
     * of via a launched process too). FE_ENTRY:<n> keeps browsing
     * (window stays open); FE_SAVEAS:<name> and FE_CANCEL both finish
     * the dialog, so the window closes the same way CLOSE already
     * does elsewhere - the manager (a separate, independent process)
     * keeps running just long enough on its own to notice the write,
     * commit result/result_action, and exit itself; closing this
     * window early never kills it. See file_explorer_manager.c's own
     * header comment for the full real command contract. */
    if (strncmp(action, "FE_", 3) == 0) {
        char ap[PATH_BUF];
        /* Each explorer instance owns its own package dir (default instance =
         * the widget dir itself; named instances = widget/instances/<name>/),
         * so two explorer windows never share one action file. */
        if (g_package_dir[0])
            snprintf(ap, sizeof(ap), "%s/file_explorer_action.txt", g_package_dir);
        else
            snprintf(ap, sizeof(ap), "%s/&.widgits/file-explorer/file_explorer_action.txt", g_house_root);
        char cmd_buf[600];
        if (strcmp(action, "FE_SAVEAS") == 0) {
            /* REAL, NEW 2026-09-05 - the filename field is a real,
             * live <cli_io> (id="filename") the shared renderer itself
             * already owns the typed value for - read it directly off
             * the live tree here rather than trusting a `${filename}`
             * template substitution, which only ever reflects the
             * MANAGER's last-published value (updated on an ENTRY pick,
             * not on every keystroke) and would go stale the moment a
             * human types something different afterward. */
            Elem *fld = find_by_id(g_window, "filename");
            snprintf(cmd_buf, sizeof(cmd_buf), "SAVEAS:%s", fld ? fld->input_buffer : "");
        } else {
            snprintf(cmd_buf, sizeof(cmd_buf), "%s", action + 3);
        }
        FILE *af = fopen(ap, "w");
        if (af) { fprintf(af, "seq=%u\ncmd=%s\n", ++g_swatch_action_seq, cmd_buf); fclose(af); }
        if (strcmp(action, "FE_SAVEAS") == 0 || strcmp(action, "FE_CANCEL") == 0) g_quit = 1;
        return;
    }
    /* REAL, NEW 2026-09-05 (same brainstorm doc, pdl-read - the second
     * real app off today's cli_io/text_area/File-Explorer work) - same
     * in-process action-file-write convention as FE_ above, but
     * written to g_package_dir directly (pdl-read's own manager lives
     * in the SAME package_dir as this window's xhtpm, unlike the File
     * Explorer widget's fixed &.widgits/ location) - see pdl_read_
     * manager.c's own header for the full OPEN:<n>/NEXT/PREV command
     * contract. Page navigation never quits the window - only real
     * page-switching (GOTO:/BACK, already generic, unrelated to this
     * prefix) leaves the reader view. */
    /* REAL, NEW 2026-09-05, direct live request ("should have file
     * explorer 'file' button in its header to open other files") -
     * launches the File Explorer widget's own real button.sh (a plain
     * background process - no new IPC needed, pdl_read_manager.c
     * already independently polls that widget's own real, published
     * result state once it's running) AND switches to the "read" page
     * right away, same real reason PDL_OPEN:<n> below also needs a
     * direct switch_page() call - one xhtpm action= string can't run
     * two dispatch verbs. The "read" page shows its own real "no
     * document open" state until the manager notices a pick and
     * republishes - the same live-reparse-on-vars-change mechanism
     * every other page_text update already relies on, nothing new. */
    if (strcmp(action, "PDL_OPENFILE") == 0) {
        /* REAL FIX 2026-09-05, direct live report ("opened a mess
         * within same window, didn't see any files") - button.sh was
         * rewritten to the real toy.pdl launch convention (`sh
         * button.sh run`, deriving house_root itself) so it could be
         * added to the "toys" menu; this call site still passed
         * house_root as argv[1], which is no longer "run", so the
         * script silently exited 0 without launching anything - the
         * switch_page("read") below still fired regardless, which is
         * exactly the "mess in the same window, no files" symptom. */
        char cmd[PATH_BUF * 2];
        snprintf(cmd, sizeof(cmd), "sh '%s/&.widgits/file-explorer/button.sh' run >/dev/null 2>&1 &",
                 g_house_root);
        int rc = system(cmd);
        (void)rc;
        switch_page("read");
        return;
    }
    if (strncmp(action, "PDL_", 4) == 0) {
        char ap[PATH_BUF];
        snprintf(ap, sizeof(ap), "%s/pdl_read_action.txt", g_package_dir);
        FILE *af = fopen(ap, "w");
        if (af) { fprintf(af, "seq=%u\ncmd=%s\n", ++g_swatch_action_seq, action + 4); fclose(af); }
        /* REAL, NEW 2026-09-05 - a single dispatch() call only ever
         * runs ONE action string; PDL_OPEN:<n> needs BOTH the manager
         * notified (above) AND the window to actually switch to the
         * "read" page - done directly here rather than trying to
         * encode two actions into one xhtpm action= string (dispatch()
         * has no such combinator, and inventing one for just this case
         * isn't worth it when a real generic switch_page() call is
         * right here already). */
        if (strncmp(action + 4, "OPEN:", 5) == 0) switch_page("read");
        return;
    }
    /* REAL, NEW 2026-09-05 - music-player-hq (@.apps/music-player-hq),
     * same shape as PDL_* / CSVH_*: just relay the verb (everything
     * after "MUS_") to the app's own action file. The backend
     * (music_player_manager.+x) owns all playback state and drives an
     * mpg123 -R child; it's a single <page> app so no switch_page()
     * pairing is needed. Verbs: PLAY_INDEX:<n>, PLAYPAUSE, STOP,
     * NEXT, PREV, SEEK:<+/-secs>, VOL_UP, VOL_DN, SHUFFLE_TOGGLE,
     * RESCAN. None need a live element value (unlike FE_SAVEAS /
     * CSVH_SETCELL), so a plain seq/cmd write is the whole job. */
    if (strncmp(action, "MUS_", 4) == 0) {
        char ap[PATH_BUF];
        snprintf(ap, sizeof(ap), "%s/music_player_action.txt", g_package_dir);
        FILE *af = fopen(ap, "w");
        if (af) { fprintf(af, "seq=%u\ncmd=%s\n", ++g_swatch_action_seq, action + 4); fclose(af); }
        return;
    }
    /* REAL, NEW 2026-09-05 - text-edit-hq (@.apps/text-edit-hq), same
     * real toys-menu batch as csv-hq below. TXT_OPENFILE launches the
     * shared File Explorer widget exactly like PDL_OPENFILE (own real
     * window, LOAD mode, no page switch needed - text-edit-hq is a
     * single-page app); the manager polls File Explorer's own
     * published result itself, same poll_file_explorer_pick() pattern
     * pdl_read_manager.c already uses - no new IPC. TXT_SAVE/
     * TXT_SAVEAS both need the LIVE <text_area id="editor"> buffer,
     * which only this render process holds (the manager's own last-
     * published `content` var is stale the instant a human types
     * anything) - dumped RAW (this file's own private scratch buffer,
     * both ends agree on the format, no escaping needed) to
     * text_edit_save_buffer.txt, same real "dump live element, THEN
     * write the actual action" two-step FE_SAVEAS already established
     * for its own live "filename" cli_io. TXT_SAVEAS also reads the
     * live "pathfield" cli_io the same way. */
    if (strcmp(action, "TXT_OPENFILE") == 0) {
        char cmd[PATH_BUF * 2];
        snprintf(cmd, sizeof(cmd), "sh '%s/&.widgits/file-explorer/button.sh' run >/dev/null 2>&1 &",
                 g_house_root);
        int rc = system(cmd);
        (void)rc;
        return;
    }
    if (strcmp(action, "TXT_NEW") == 0) {
        char ap[PATH_BUF];
        snprintf(ap, sizeof(ap), "%s/text_edit_action.txt", g_package_dir);
        FILE *af = fopen(ap, "w");
        if (af) { fprintf(af, "seq=%u\ncmd=NEW\n", ++g_swatch_action_seq); fclose(af); }
        return;
    }
    if (strcmp(action, "TXT_SAVE") == 0 || strcmp(action, "TXT_SAVEAS") == 0) {
        Elem *ed = find_by_id(g_window, "editor");
        char bufpath[PATH_BUF];
        snprintf(bufpath, sizeof(bufpath), "%s/text_edit_save_buffer.txt", g_package_dir);
        FILE *bf = fopen(bufpath, "wb");
        if (bf) { if (ed) fputs(ed->text_area_buffer, bf); fclose(bf); }
        char ap[PATH_BUF];
        snprintf(ap, sizeof(ap), "%s/text_edit_action.txt", g_package_dir);
        FILE *af = fopen(ap, "w");
        if (strcmp(action, "TXT_SAVE") == 0) {
            if (af) { fprintf(af, "seq=%u\ncmd=SAVE\n", ++g_swatch_action_seq); fclose(af); }
        } else {
            /* REAL, NEW 2026-09-05, direct live report ("we dont need
             * that save as path cli_io thing. we have seperate save
             * menu 2 handle that") - Save As no longer reads a typed
             * <cli_io> path from the sidebar (removed). It arms the
             * manager for a save-as (buffer already dumped above) and
             * launches the shared File Explorer widget - the manager
             * then treats the NEXT File Explorer pick as the save
             * target, not a file to load (SAVEAS_ARM). */
            if (af) { fprintf(af, "seq=%u\ncmd=SAVEAS_ARM\n", ++g_swatch_action_seq); fclose(af); }
            char cmd[PATH_BUF * 2];
            snprintf(cmd, sizeof(cmd), "sh '%s/&.widgits/file-explorer/button.sh' run >/dev/null 2>&1 &", g_house_root);
            int rc = system(cmd); (void)rc;
        }
        return;
    }
    /* REAL, NEW 2026-09-05 - csv-hq (@.apps/csv-hq). Same real shape as
     * text-edit-hq just above - CSVH_OPENFILE launches the shared File
     * Explorer widget; CSVH_SETCELL/CSVH_FUNC:<name> need a live
     * <cli_io> value (cellval/colref) the manager's own last-published
     * state can't reflect mid-keystroke, dumped/embedded the same real
     * way FE_SAVEAS and TXT_SAVE already do. Column refs and function
     * names are short, house-controlled tokens (typed into a small
     * cli_io, or a fixed action= suffix) - safe to embed directly in
     * the single-line cmd=... the manager parses, no scratch-file
     * needed there (unlike cellval, which could contain arbitrary
     * text). CSVH_NEW/CSVH_SAVE fall through to the generic CSVH_
     * prefix at the end - the grid itself is entirely manager-owned
     * (edited only via real SETCELL calls, never a giant live text
     * buffer), so no live-dump is needed for either. */
    if (strcmp(action, "CSVH_OPENFILE") == 0) {
        char cmd[PATH_BUF * 2];
        snprintf(cmd, sizeof(cmd), "sh '%s/&.widgits/file-explorer/button.sh' run >/dev/null 2>&1 &",
                 g_house_root);
        int rc = system(cmd);
        (void)rc;
        return;
    }
    /* REAL, NEW 2026-09-05 (GRID-ELEMENT-DESIGN.md, step 5 - the real
     * <grid> element replacing csv-hq's old cellref/cellval/Set-Cell
     * trio). Fired from default_grid_handle_key()'s own Escape-commit
     * branch via the grid's own action=/onclick= attribute - reads the
     * LIVE grid element's own grid_cur_row/col + grid_cell_buffer
     * (the manager's last-published state can't reflect a mid-edit
     * keystroke), dumps the edited value to the SAME scratch file
     * CSVH_SETCELL already uses, and issues the SAME SETCELL command -
     * zero manager-side protocol change, per the design doc's own
     * "the grid is purely a richer input surface" decision. */
    if (strcmp(action, "CSVH_GRIDCOMMIT") == 0) {
        Elem *g = find_by_id(g_window, "sheet");
        if (g) {
            char bufpath[PATH_BUF];
            snprintf(bufpath, sizeof(bufpath), "%s/csv_setcell_buffer.txt", g_package_dir);
            FILE *bf = fopen(bufpath, "wb");
            if (bf) { fputs(g->grid_cell_buffer, bf); fclose(bf); }
            char colletters[8];
            grid_col_to_letters(g->grid_cur_col, colletters, sizeof(colletters));
            char ap[PATH_BUF];
            snprintf(ap, sizeof(ap), "%s/csv_hq_action.txt", g_package_dir);
            FILE *af = fopen(ap, "w");
            if (af) { fprintf(af, "seq=%u\ncmd=SETCELL:%s%d\n", ++g_swatch_action_seq, colletters, g->grid_cur_row + 1); fclose(af); }
        }
        return;
    }
    if (strcmp(action, "CSVH_SETCELL") == 0) {
        Elem *rf = find_by_id(g_window, "cellref");
        Elem *vf = find_by_id(g_window, "cellval");
        char bufpath[PATH_BUF];
        snprintf(bufpath, sizeof(bufpath), "%s/csv_setcell_buffer.txt", g_package_dir);
        FILE *bf = fopen(bufpath, "wb");
        if (bf) { if (vf) fputs(vf->input_buffer, bf); fclose(bf); }
        char ap[PATH_BUF];
        snprintf(ap, sizeof(ap), "%s/csv_hq_action.txt", g_package_dir);
        FILE *af = fopen(ap, "w");
        if (af) { fprintf(af, "seq=%u\ncmd=SETCELL:%s\n", ++g_swatch_action_seq, rf ? rf->input_buffer : ""); fclose(af); }
        return;
    }
    if (strncmp(action, "CSVH_FUNC:", 10) == 0) {
        Elem *cf = find_by_id(g_window, "colref");
        char ap[PATH_BUF];
        snprintf(ap, sizeof(ap), "%s/csv_hq_action.txt", g_package_dir);
        FILE *af = fopen(ap, "w");
        if (af) { fprintf(af, "seq=%u\ncmd=FUNC:%s:%s\n", ++g_swatch_action_seq, action + 10, cf ? cf->input_buffer : ""); fclose(af); }
        return;
    }
    if (strncmp(action, "CSVH_", 5) == 0) {
        char ap[PATH_BUF];
        snprintf(ap, sizeof(ap), "%s/csv_hq_action.txt", g_package_dir);
        FILE *af = fopen(ap, "w");
        if (af) { fprintf(af, "seq=%u\ncmd=%s\n", ++g_swatch_action_seq, action + 5); fclose(af); }
        return;
    }
    /* REAL, NEW 2026-08-29 (TASK 2: opacity control) - OPACITY_MINUS/OPACITY_PLUS
     * handlers. Read current opacity from theme, adjust by ±0.05, write back,
     * and apply to the window immediately for live visual feedback. */
    if (strcmp(action, "OPACITY_MINUS") == 0) {
        double opacity = load_theme_opacity();
        opacity -= 0.05;
        if (opacity < 0.0) opacity = 0.0;
        write_theme_opacity(opacity);
        set_window_opacity(dpy, win, opacity);
        redraw();
        return;
    }
    if (strcmp(action, "OPACITY_PLUS") == 0) {
        double opacity = load_theme_opacity();
        opacity += 0.05;
        if (opacity > 1.0) opacity = 1.0;
        write_theme_opacity(opacity);
        set_window_opacity(dpy, win, opacity);
        redraw();
        return;
    }
    if (strcmp(action, "UI_SCALE_MINUS") == 0 || strcmp(action, "UI_SCALE_PLUS") == 0) {
        /* LIVEDESK-UI-SCALE.md - step font_scale in hq_ui.pdl by 0.25,
         * clamp 0.75..2.0, re-size the chrome font, relayout, repaint.
         * Other open windows follow via hq_ui_pdl_reload_if_changed(). */
        int s = g_ui_user_pct + (action[9] == 'P' ? 25 : -25); /* the user's font_scale, not the screen-auto-scaled effective pct */
        if (s < 75) s = 75;
        if (s > 200) s = 200;
        desktop_set_font_scale(g_house_root, s);
        reload_font_ui();
        if (!g_quit) { assign_nav_and_layout(); redraw(); }
        return;
    }
    if (strcmp(action, "UI_FONT_FAMILY_NEXT") == 0 || strcmp(action, "UI_FONT_FAMILY_PREV") == 0) {
        /* direct instruction 2026-09-10 ("when should we add font
         * picker to settings... lets do the build") - same real
         * step-button UX UI_SCALE_MINUS/PLUS already uses, cycling
         * g_font_family_choices instead of a number. Any window with
         * its own CSS font-family is unaffected - only the house-wide
         * default this picker controls. */
        int idx = 0;
        for (int i = 0; i < N_FONT_FAMILY_CHOICES; i++)
            if (strcmp(g_ui_font_family, g_font_family_choices[i]) == 0) { idx = i; break; }
        idx += (action[15] == 'N') ? 1 : -1;   /* "UI_FONT_FAMILY_" is 15 chars - index 15 is N(ext)/P(rev) */
        if (idx < 0) idx = N_FONT_FAMILY_CHOICES - 1;
        if (idx >= N_FONT_FAMILY_CHOICES) idx = 0;
        desktop_set_font_family(g_house_root, g_font_family_choices[idx]);
        reload_font_ui();
        if (!g_quit) { assign_nav_and_layout(); redraw(); }
        return;
    }
    if (strcmp(action, "CLICK_TWOSTEP_TOGGLE") == 0) {
        desktop_toggle_click_two_step(g_house_root);
        redraw();
        return;
    }
    if (strcmp(action, "MINIMIZE") == 0) {
        if (!window_is_dock() && g_default_has_sidebar_panel) {
            g_hq_minimized = 1;
            {
                char reg_path[PATH_BUF], reg_tmp[PATH_BUF];
                const char *title_raw = (g_window && g_window->label[0] ? g_window->label : g_current_page);
                snprintf(reg_path, sizeof(reg_path), "%s/#.desktop/livedesk_hq_windows_%d.txt",
                         g_house_root, (int)getpid());
                snprintf(reg_tmp, sizeof(reg_tmp), "%s.tmp", reg_path);
                FILE *rf = fopen(reg_tmp, "w");
                if (rf) {
                    fprintf(rf, "win=0x%lx|pid=%d|title=%s|x=%d|y=%d|w=%d|h=%d|minimized=1|focused=0\n",
                            (unsigned long)win, (int)getpid(), title_raw,
                            g_win_x, g_win_y, g_win_w, g_win_h);
                    fclose(rf);
                    rename(reg_tmp, reg_path);
                }
            }
            XUnmapWindow(dpy, win);
            XFlush(dpy);
        }
        return;
    }
    if (strcmp(action, "CLOSE") == 0) { g_quit = 1; return; }
    /* REAL, NEW 2026-09-01 - the sidebar+panel chrome "!" button (see
     * g_default_is_fullscreen's own declaration comment) - a real,
     * generic toggle, not open-hai-specific: any sidebar+panel window
     * gets this for free. Real window resize handled by redraw()'s own
     * existing g_win_w/g_win_h vs g_buf_w/g_buf_h grow-check and real
     * XResizeWindow call, already proven safe for a live-reparse-driven
     * size change (capability #1's own resize-safety fix) - this is
     * the same real mechanism, just toggled by a click instead of new
     * content. */
    if (strcmp(action, "TOGGLE_FULLSCREEN") == 0) {
        g_default_is_fullscreen = !g_default_is_fullscreen;
        if (g_default_is_fullscreen) {
            g_default_pre_fullscreen_x = g_win_x; g_default_pre_fullscreen_y = g_win_y;
            /* inside the work area, stop short of the edges */
            g_win_x = WM_FS_MARGIN_X; g_win_y = g_win_top_y;
        } else {
            g_win_x = g_default_pre_fullscreen_x; g_win_y = g_default_pre_fullscreen_y;
        }
        XMoveWindow(dpy, win, g_win_x, g_win_y);
        /* REAL FIX 2026-09-05 - force a real relayout+repaint now (this
         * handler previously relied on some later redraw() that never
         * came for a plain app window). redraw() itself does the
         * grow-buf + XResizeWindow to the new fullscreen size before it
         * draws (moved early this same fix). */
        if (!g_quit) { assign_nav_and_layout(); redraw(); }
        return;
    }
    /* REAL FIX 2026-08-16, direct live report ("cancel doesn't work
     * yet"): the legacy dispatch (tp_desktop_window_rgb.c line ~2026)
     * ALWAYS calls close_context_menu() before even looking at the
     * action - "void" only skips running a shell command, it still
     * closes the menu. This copy returned without setting g_quit, so
     * Cancel/Stop silently left the window open. */
    if (strcmp(action, "void") == 0) {
        /* A context-menu Cancel/Stop row is `action="void"` and SHOULD
         * close its (transient) menu - the legacy tp_desktop_window_
         * rgb.c behavior. But a persistent window (class="database-
         * window"/"palettes-pal"), a sidebar+panel app, or a live
         * canvas window (the pchq board) uses `action="void"` for
         * genuine no-op chrome (the board's clock / Menu / Player
         * stubs) - there, "void" must mean nothing, not "close the
         * whole window". REAL FIX 2026-09-09, direct live report
         * ("clicking toolbar clock ... actually closes the window").
         * Same guard the shell-command fallthrough at the end of this
         * function already uses. */
        if (!g_default_has_sidebar_panel && !g_default_persistent && !g_has_canvas)
            g_quit = 1;
        return;
    }
    if (strncmp(action, "GOTO:", 5) == 0) { switch_page(action + 5); return; }
    if (strcmp(action, "BACK") == 0) {
        if (g_page_stack_n > 0) { switch_page(g_page_stack[--g_page_stack_n]); }
        return;
    }
    char cmd[PATH_BUF * 3];
    snprintf(cmd, sizeof(cmd), "%s '%s' '%s' >/dev/null 2>&1 &", action, g_package_dir, g_house_root);
    int rc = system(cmd);
    (void)rc;
    /* real menus close after a real action fires, matching
     * tp_desktop_window_rgb.c's own UX - but NOT for a genuinely
     * persistent sidebar+panel window (open-hai/chat-hai/network-
     * browser's own New-session/Sound-toggle/Model-cycle/Pause/Speed
     * controls, all plain <item action=...>). REAL FIX 2026-09-01
     * (found live: chat-hai's own Pause button - and, independently,
     * open-hai's real running window - silently closed the WHOLE app
     * on a single click, confirmed via a real relay-driven repro: the
     * dispatched shell command ran and wrote its state file correctly,
     * then the process exited cleanly right after). See
     * g_default_has_sidebar_panel's own declaration comment. */
    if (!g_default_has_sidebar_panel && !g_default_persistent) g_quit = 1;
}

/* REAL, NEW 2026-09-01 - same real shell-command dispatch as dispatch()
 * itself, minus its own "menus close after a real action fires"
 * g_quit=1 (see that function's own comment for why it's there) - a
 * persistent window's own backspace_action (see Elem's own field
 * comment) must not close the whole window just because it deleted one
 * row, same real reasoning default_cli_io_run_action() already applies
 * to a composer's own action=. */
static void dispatch_no_quit(const char *action) {
    char cmd[PATH_BUF * 3];
    snprintf(cmd, sizeof(cmd), "%s '%s' '%s' >/dev/null 2>&1 &", action, g_package_dir, g_house_root);
    int rc = system(cmd);
    (void)rc;
}

/* REAL, generic capability #2 (2026-08-31, xperiments/khtpm-generic-
 * dispatch-design.md §5) - a real, generic `<cli_io>` text-input
 * element for the default/popup mode, ported directly from
 * 1.TPMOS_c_+rmmp.0103.0001/pieces/chtpm/plugins/chtpm_parser.c's own
 * real UIElement.input_buffer/target_id design (read in full before
 * writing this - direct instruction: "see existing chtpm parser std
 * format... can khtpm parser be more similar?"). Zero per-app C: any
 * `.chtpm` can declare `<cli_io id="..." target_id="..." action="...">`
 * and get real armed text-input, live-synced to a real, generic
 * per-window `cli_io_state.txt` (same real "target_id-keyed state
 * file" shape the reference uses, just this house's own plain
 * key=value line format instead of gui_state.txt's own).
 * (g_default_input_elem itself now lives further up this file, near
 * g_focus_nav - see its own comment there for why.) */

static void default_cli_io_state_path(char *out, size_t outsz) {
    snprintf(out, outsz, "%s/cli_io_state.txt", g_package_dir);
}

/* Real, generic read-modify-write - same real shape as the reference's
 * own save_to_gui_state_impl(): rewrite every real line, updating (or
 * adding) the one this element owns. Small, bounded real file (one
 * line per real armed field a window ever has), safe to rewrite whole
 * on every keystroke, matching the reference's own real "live sync on
 * every keystroke" behavior. */
static void default_cli_io_save(Elem *e) {
    const char *key = e->target_id[0] ? e->target_id : e->id;
    if (!key[0]) return;
    char path[PATH_BUF];
    default_cli_io_state_path(path, sizeof(path));
    char lines[64][128];
    int n = 0;
    FILE *f = fopen(path, "r");
    if (f) {
        char line[256];
        while (n < 64 && fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\r\n")] = '\0';
            char *eq = strchr(line, '=');
            if (!eq) continue;
            *eq = '\0';
            if (strcmp(line, key) == 0) continue; /* real value replaced below */
            snprintf(lines[n], sizeof(lines[n]), "%s=%s", line, eq + 1);
            n++;
        }
        fclose(f);
    }
    f = fopen(path, "w");
    if (!f) return;
    for (int i = 0; i < n; i++) fprintf(f, "%s\n", lines[i]);
    fprintf(f, "%s=%s\n", key, e->input_buffer);
    fclose(f);
}

/* Real, generic "run this real action without also quitting" -
 * SAME argv/quoting convention as dispatch()'s own real shell-command
 * branch, minus its real "menus close after a real action fires"
 * g_quit=1 - a persistent composer/chat field submitting a message
 * must NOT close its own window, unlike a one-shot menu item.
 *
 * REAL, NEW 2026-08-31 - a 3rd argv, the field's own live typed value
 * at the moment Enter was pressed. Without this, a consumer's only way
 * to read what was typed is cli_io_state.txt - but this same function's
 * caller clears and re-saves the buffer (empty) right after spawning
 * this backgrounded command, so a script that instead re-reads that
 * file races its own clear (real, if rare, TOCTOU - the background
 * child may not have opened the file yet). Passing the value directly
 * as an argv is immune to that race by construction. */
static void default_cli_io_run_action(const char *action, const char *value) {
    if (!action || !action[0]) return;
    char val_esc[600];
    { size_t o = 0; for (const unsigned char *p = (const unsigned char *)value; *p && o + 5 < sizeof(val_esc); p++) {
        if (*p == '\'') { memcpy(val_esc + o, "'\\''", 4); o += 4; } else val_esc[o++] = (char)*p;
    } val_esc[o] = '\0'; }
    char cmd[PATH_BUF * 3 + 700];
    snprintf(cmd, sizeof(cmd), "%s '%s' '%s' '%s' >/dev/null 2>&1 &", action, g_package_dir, g_house_root, val_esc);
    int rc = system(cmd);
    (void)rc;
}

/* REAL, NEW 2026-09-05 (CLI_IO-CURSOR-AND-TEXT_AREA-MULTILINE-EDITING-
 * DESIGN.md) - text_area's own live-save. cli_io_state.txt's key=value-
 * per-line format can't hold a value containing real newlines, so this
 * is a real, separate, dedicated file per field (named by its own
 * target_id/id) holding the raw buffer as the WHOLE file content - no
 * escaping needed here (unlike the frame round-trip file below, this
 * one is read as one big blob by any real consumer, never line-by-
 * line). */
static void default_text_area_state_path(const char *key, char *out, size_t outsz) {
    snprintf(out, outsz, "%s/text_area_%s.txt", g_package_dir, key);
}
static void default_text_area_save(Elem *e) {
    const char *key = e->target_id[0] ? e->target_id : e->id;
    if (!key[0]) return;
    char path[PATH_BUF];
    default_text_area_state_path(key, path, sizeof(path));
    FILE *f = fopen(path, "w");
    if (!f) return;
    fputs(e->text_area_buffer, f);
    fclose(f);
}

/* Re-hydrate a <text_area> from its own text_area_<id>.txt (written by
 * default_text_area_save). Called once per reparse so a projector's
 * every-Run state rewrite doesn't wipe the user's in-progress buffer.
 * If the save file is absent the elem keeps its content="" attr value. */
static void kh_text_areas_reload(Elem *root) {
    if (!root || !g_package_dir[0]) return;
    if (strcmp(root->tag, "text_area") == 0) {
        const char *key = root->target_id[0] ? root->target_id : root->id;
        if (key[0]) {
            char path[PATH_BUF];
            default_text_area_state_path(key, path, sizeof(path));
            FILE *f = fopen(path, "rb");
            if (f) {
                size_t n = fread(root->text_area_buffer, 1,
                                 sizeof(root->text_area_buffer) - 1, f);
                root->text_area_buffer[n] = '\0';
                fclose(f);
            }
        }
    }
    for (int i = 0; i < root->n_children; i++)
        kh_text_areas_reload(root->children[i]);
}

/* cli_io's own half of the same real fix text_area got 2026-09-08 -
 * see reparse_chtpm_if_changed()'s own 2026-09-11 comment (direct
 * instruction: "make [browser, h-ai cli_io] just work like text-edit"). */
static void kh_cli_io_reload(Elem *root) {
    if (!root || !g_package_dir[0]) return;
    if (strcmp(root->tag, "cli_io") == 0) {
        const char *key = root->target_id[0] ? root->target_id : root->id;
        if (key[0]) {
            char path[PATH_BUF];
            default_cli_io_state_path(path, sizeof(path));
            FILE *f = fopen(path, "r");
            if (f) {
                char line[256];
                while (fgets(line, sizeof(line), f)) {
                    line[strcspn(line, "\r\n")] = '\0';
                    char *eq = strchr(line, '=');
                    if (!eq) continue;
                    *eq = '\0';
                    if (strcmp(line, key) == 0) {
                        snprintf(root->input_buffer, sizeof(root->input_buffer), "%s", eq + 1);
                        break;
                    }
                }
                fclose(f);
            }
        }
    }
    for (int i = 0; i < root->n_children; i++)
        kh_cli_io_reload(root->children[i]);
}

/* Find the SAME cli_io/text_area a saved key= identifies, in a freshly
 * rebuilt tree - same key derivation (target_id-or-id) the save/reload
 * functions above already use, checked against BOTH fields (not just
 * find_by_id()'s plain e->id match) since target_id and id can differ. */
static Elem *kh_find_input_by_key(Elem *root, const char *key) {
    if (!root || !key || !key[0]) return NULL;
    if (strcmp(root->tag, "cli_io") == 0 || strcmp(root->tag, "text_area") == 0 ||
        strcmp(root->tag, "grid") == 0) {
        const char *k = root->target_id[0] ? root->target_id : root->id;
        if (k[0] && strcmp(k, key) == 0) return root;
    }
    for (int i = 0; i < root->n_children; i++) {
        Elem *r = kh_find_input_by_key(root->children[i], key);
        if (r) return r;
    }
    return NULL;
}

/* REAL, NEW 2026-09-05 - real logical-line (delimited by actual `\n`
 * bytes the user typed, NOT the renderer's own display-only word-wrap
 * breaks) boundary helpers, shared by Home/End/Up/Down below. For
 * cli_io (never contains a real `\n`) these are always a no-op past
 * "the whole buffer is one line" - correctly reducing to cli_io's
 * original whole-buffer Home/End behavior for free. */
static int kh_text_line_start(const char *buf, int pos) {
    while (pos > 0 && buf[pos - 1] != '\n') pos--;
    return pos;
}
static int kh_text_line_end(const char *buf, int pos) {
    int len = (int)strlen(buf);
    while (pos < len && buf[pos] != '\n') pos++;
    return pos;
}

/* REAL, NEW 2026-09-05 (CLI_IO-CURSOR-AND-TEXT_AREA-MULTILINE-EDITING-
 * DESIGN.md, direct instruction: "i think cli-io should still have a
 * cursor") - real cursor-aware editing, replacing the old append/
 * backspace-at-the-end-only behavior. `e->cursor` is a plain byte
 * offset (matching the existing printable-char branch below, which is
 * ASCII-only already - no UTF-8 codepoint-width awareness attempted
 * here, consistent with the scope this function already had). This is
 * the SAME primitive text_area's own multi-line cursor logic will
 * generalize from (crossing embedded \n and visual post-wrap rows,
 *08-roadmap/design-docs/CLI_IO-CURSOR-AND-TEXT_AREA-MULTILINE-EDITING-
 * DESIGN.md) - keep insert/delete-at-offset shaped so that reuse stays
 * possible, don't fold assumptions here that only make sense for a
 * single line with no real newlines. */
/* REAL, NEW 2026-09-05 - generalized to (buffer, capacity, *cursor)
 * instead of hardcoding e->input_buffer, so <text_area> (its own much
 * bigger text_area_buffer, same shared cursor field) reuses these
 * verbatim instead of a second, duplicate implementation - exactly
 * the reuse this plan's own design doc called for. default_cli_io_
 * handle_key() below picks buf/cap based on e->tag and calls these
 * directly - no separate cli_io-named wrapper layer anymore. */
static void kh_text_clamp_cursor(char *buf, int *cursor) {
    int len = (int)strlen(buf);
    if (*cursor < 0) *cursor = 0;
    if (*cursor > len) *cursor = len;
}
static void kh_text_insert_at_cursor(char *buf, size_t cap, int *cursor, char ch) {
    int len = (int)strlen(buf);
    if (len + 1 >= (int)cap) return; /* real cap, matches the old append path's own bound */
    kh_text_clamp_cursor(buf, cursor);
    for (int i = len; i >= *cursor; i--) buf[i + 1] = buf[i];
    buf[*cursor] = ch;
    (*cursor)++;
}
static void kh_text_delete_before_cursor(char *buf, int *cursor) {
    kh_text_clamp_cursor(buf, cursor);
    if (*cursor <= 0) return;
    int len = (int)strlen(buf);
    for (int i = *cursor - 1; i < len; i++) buf[i] = buf[i + 1];
    (*cursor)--;
}
static void kh_text_delete_at_cursor(char *buf, int *cursor) {
    kh_text_clamp_cursor(buf, cursor);
    int len = (int)strlen(buf);
    if (*cursor >= len) return;
    for (int i = *cursor; i < len; i++) buf[i] = buf[i + 1];
}

/* REAL, NEW 2026-09-05 (TEXT_AREA-SCROLL-GUTTER-SELECTION-DESIGN.md) -
 * if [sel_anchor,cursor) is a real (non-collapsed) range, delete it,
 * leave the cursor at the range start with the selection collapsed.
 * Returns 1 if it deleted something. This is the "typing / paste /
 * Backspace replaces the selection" primitive. */
static int kh_text_delete_selection(Elem *e, char *buf) {
    if (e->sel_anchor == e->cursor) return 0;
    int lo = e->sel_anchor < e->cursor ? e->sel_anchor : e->cursor;
    int hi = e->sel_anchor < e->cursor ? e->cursor : e->sel_anchor;
    int len = (int)strlen(buf);
    if (lo < 0) lo = 0;
    if (hi > len) hi = len;
    memmove(buf + lo, buf + hi, (size_t)(len - hi) + 1);
    e->cursor = lo;
    e->sel_anchor = lo;
    return 1;
}
/* Selection bounds into *lo/*hi (clamped to the real buffer). Returns
 * 1 if there is a real selection, 0 if collapsed. */
static int kh_text_selection_range(const Elem *e, const char *buf, int *lo, int *hi) {
    if (e->sel_anchor == e->cursor) return 0;
    int a = e->sel_anchor < e->cursor ? e->sel_anchor : e->cursor;
    int b = e->sel_anchor < e->cursor ? e->cursor : e->sel_anchor;
    int len = (int)strlen(buf);
    if (a < 0) a = 0;
    if (b > len) b = len;
    *lo = a; *hi = b;
    return b > a;
}

/* ---------- REAL, NEW 2026-09-05 (08-roadmap/design-docs/
 * CLIPBOARD-COPY-PASTE-DESIGN.md, direct instruction "lets build it in
 * text editor") - real X11 CLIPBOARD selection copy/paste. Works
 * window <-> window (any two khtpm processes) AND window <-> any other
 * real X11 app (terminal, browser), because it speaks the actual X11
 * selection protocol rather than a house-private channel. v1 scope:
 * whole-buffer copy, paste-at-cursor - no text-range SELECTION yet
 * (that's a real, separate, larger feature, see the design doc's own
 * v2 note).
 *
 * Copy = XSetSelectionOwner(CLIPBOARD, win); we then answer
 * SelectionRequest events (hq_dispatch_xevent()) with g_clipboard_text
 * for as long as we own it. Paste = XConvertSelection request, then
 * the text arrives asynchronously via a SelectionNotify event, which
 * inserts it at g_default_input_elem's cursor. ---------- */
static char g_clipboard_text[65536];
static int  g_paste_pending = 0;
static Atom g_atom_clipboard = 0, g_atom_utf8 = 0, g_atom_targets = 0, g_atom_paste_prop = 0;
/* REAL, NEW 2026-09-05, direct live request ("is there a way, in top
 * right to show var 'copied' when ctrl+c is pressed?") - set on every
 * copy/cut; redraw()'s own chrome-strip block draws a small "copied"
 * tag top-right while this is within ~2s of now, and hq_idle_tick()
 * clears it (one final repaint) once it ages out. */
static time_t g_clip_copied_at = 0;

static void kh_clipboard_init_atoms(void) {
    if (g_atom_clipboard) return;
    g_atom_clipboard  = XInternAtom(dpy, "CLIPBOARD", False);
    g_atom_utf8       = XInternAtom(dpy, "UTF8_STRING", False);
    g_atom_targets    = XInternAtom(dpy, "TARGETS", False);
    g_atom_paste_prop = XInternAtom(dpy, "KH_CLIP_PASTE", False);
}

static void kh_clipboard_copy(const char *text) {
    kh_clipboard_init_atoms();
    snprintf(g_clipboard_text, sizeof(g_clipboard_text), "%s", text ? text : "");
    XSetSelectionOwner(dpy, g_atom_clipboard, win, CurrentTime);
    /* also seed PRIMARY - some real X apps (xterm middle-click) only
     * read that one; costs nothing to own both. */
    XSetSelectionOwner(dpy, XA_PRIMARY, win, CurrentTime);
    XFlush(dpy);
    g_clip_copied_at = time(NULL);
}

static void kh_clipboard_request_paste(void) {
    kh_clipboard_init_atoms();
    g_paste_pending = 1;
    /* Ask the current CLIPBOARD owner to write UTF8_STRING into our own
     * KH_CLIP_PASTE property - answered async by a SelectionNotify. */
    XConvertSelection(dpy, g_atom_clipboard, g_atom_utf8, g_atom_paste_prop, win, CurrentTime);
    XFlush(dpy);
}

/* Insert an arbitrary run of bytes at an armed field's cursor, reusing
 * the same one-char insert primitive typing already goes through.
 * cli_io (single line, and its input_buffer is NOT newline-escaped
 * through the frame round trip) drops '\n' -> ' '; text_area keeps
 * real newlines (its buffer IS newline-safe). '\r' is always stripped
 * (CRLF normalisation for text pasted from other platforms/apps). */
static void kh_clipboard_insert_text(Elem *e, const char *text) {
    if (!e || !text) return;
    int is_area = (strcmp(e->tag, "text_area") == 0);
    char *buf = is_area ? e->text_area_buffer : e->input_buffer;
    size_t cap = is_area ? sizeof(e->text_area_buffer) : sizeof(e->input_buffer);
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        char c = (char)*p;
        if (c == '\r') continue;
        if (c == '\n' && !is_area) c = ' ';
        if (c == '\n') { kh_text_insert_at_cursor(buf, cap, &e->cursor, '\n'); continue; }
        if (c < 32 || c > 126) continue; /* v1: ASCII-only, same scope the typed-char path already has */
        kh_text_insert_at_cursor(buf, cap, &e->cursor, c);
    }
    e->sel_anchor = e->cursor; /* a paste lands as a plain cursor, no selection */
    if (is_area) default_text_area_save(e); else default_cli_io_save(e);
}

/* REAL, NEW 2026-09-05 - this function now handles BOTH cli_io and
 * text_area (kept the cli_io name - every existing call site already
 * says "cli_io" and this IS still that same function, just widened,
 * not a rename-everything exercise). Shared: Escape/Left/Right/Home/
 * End/Delete/Backspace/printable-insert, all through the generalized
 * kh_text_*() primitives above. Different: Enter (submit-and-clear for
 * cli_io, insert-a-real-newline for text_area) and Up/Down (text_area
 * only - cli_io is single-line, nothing to move to). */
static void default_cli_io_handle_key(KeySym ks, char ch) {
    Elem *e = g_default_input_elem;
    if (!e) return;
    /* TEMPORARY diagnostic (see kh_focus_debug_log's own comment) -
     * proves the key genuinely REACHED this process/function at all -
     * if the log shows real KEYPRESS lines missing for a real
     * keystroke the user typed, the key never arrived here (lost to
     * another window/the grab, an X-level problem, not this function's
     * own logic); if every key shows up here but the visible on-screen
     * result is still wrong, the bug is downstream of this point. */
    kh_focus_debug_log("KEYPRESS key=%s ks=%lu ch=%d(%c)",
                        e->target_id[0] ? e->target_id : e->id, (unsigned long)ks, (int)ch,
                        (ch >= 32 && ch < 127) ? ch : '?');
    int is_area = (strcmp(e->tag, "text_area") == 0);
    char *buf = is_area ? e->text_area_buffer : e->input_buffer;
    size_t cap = is_area ? sizeof(e->text_area_buffer) : sizeof(e->input_buffer);
    if (ks == XK_Return || ks == XK_KP_Enter) {
        if (is_area) {
            kh_text_delete_selection(e, buf); /* Enter over a selection replaces it, same as typing */
            kh_text_insert_at_cursor(buf, cap, &e->cursor, '\n');
            e->sel_anchor = e->cursor;
            default_text_area_save(e);
        } else {
            default_cli_io_save(e);
            default_cli_io_run_action(e->onclick, e->input_buffer);
            e->input_buffer[0] = '\0';
            e->cursor = 0; /* real, matching the reference's own "clear after submit, stay active" behavior - cursor resets with the now-empty buffer */
            e->sel_anchor = 0;
            default_cli_io_save(e);
        }
        return;
    }
    /* REAL FIX 2026-08-31 (live report: armed via a real double-click,
     * "^" showed correctly, but real physical keys typed nothing - root
     * cause confirmed live: real X input focus was 0x0/None with the
     * mouse pointer far from the window, i.e. this WM's focus-follows-
     * mouse policy silently took keyboard focus away the instant the
     * human's hand left the mouse to reach the keyboard - override_
     * redirect + a plain XSetInputFocus retry at map time, this default
     * mode's existing mechanism, is mouse-position-dependent by
     * construction). Real, already-proven fix, not invented here:
     * kh_grab_keyboard_retry() (db-hq's own real XGrabKeyboard retry,
     * currently gated behind its own g_dbhq_focus_grab_enabled .pdl
     * flag for THAT mode) - reused verbatim, unconditionally, scoped to
     * exactly an armed field's own lifetime (cli_io originally, now
     * text_area too). An exclusive keyboard grab routes KeyPress to
     * `win` regardless of pointer position or window-manager focus
     * policy, so this is immune to the exact failure just diagnosed.
     * Safe to make unconditional here (no existing popup uses cli_io
     * yet, so this can't regress any of them) - see the matching
     * XUngrabKeyboard on every real disarm path (Escape here, reparse_
     * chtpm_if_changed()'s own real safety net). */
    if (ks == XK_Escape) {
        kh_focus_debug_log("ESCAPE key=%s - explicit user disarm", e->target_id[0] ? e->target_id : e->id);
        kh_set_default_input_elem(NULL); kh_ungrab_kbd(); return;
    }
    /* REAL, NEW 2026-09-05 (CLIPBOARD-COPY-PASTE-DESIGN.md +
     * TEXT_AREA-SCROLL-GUTTER-SELECTION-DESIGN.md) - Ctrl+C / Ctrl+V /
     * Ctrl+X arrive as plain control characters through XLookupString
     * (ASCII 3 / 22 / 24) on a standard X keyboard; the relay sends the
     * same bare codes. When a real text SELECTION exists, copy/cut act
     * on just the selected substring ("partial copy out" - direct
     * request); with no selection they fall back to the whole buffer
     * (still a genuinely useful "copy the whole doc" shortcut). Paste
     * replaces the selection if any, then inserts at the cursor once
     * the async SelectionNotify comes back. */
    if (ch == 3 || ch == 24) { /* Ctrl+C / Ctrl+X */
        int lo, hi;
        if (kh_text_selection_range(e, buf, &lo, &hi)) {
            char saved = buf[hi];
            buf[hi] = '\0';
            kh_clipboard_copy(buf + lo);
            buf[hi] = saved;
            if (ch == 24) { /* cut: remove the selection */
                kh_text_delete_selection(e, buf);
                if (is_area) default_text_area_save(e); else default_cli_io_save(e);
            }
        } else {
            kh_clipboard_copy(buf);
            if (ch == 24) {
                buf[0] = '\0'; e->cursor = 0; e->sel_anchor = 0;
                if (is_area) default_text_area_save(e); else default_cli_io_save(e);
            }
        }
        return;
    }
    if (ch == 22) { /* Ctrl+V */
        if (kh_text_delete_selection(e, buf)) {
            if (is_area) default_text_area_save(e); else default_cli_io_save(e);
        }
        kh_clipboard_request_paste();
        return;
    }
    /* REAL, NEW 2026-09-11, direct live report ("unlike a real browser,
     * i cant clear the search bar. i have to open a new tab") - root
     * cause: activate_focused() arms a cli_io with the cursor at the
     * END of the buffer and the selection COLLAPSED (sel_anchor ==
     * cursor), never select-all-on-focus like a real browser's address
     * bar; and this function had no key that selects the whole buffer
     * at all - Ctrl+A simply did nothing. A real, generic gap (every
     * cli_io/text_area in the house, not network-browser-specific),
     * fixed the same generic way Ctrl+C/X/V already are: Ctrl+A
     * (ASCII 1, matches the existing 3/22/24 pattern just above)
     * selects [0, strlen(buf)) - the user can then just start typing
     * (kh_text_delete_selection() already replaces a live selection on
     * any keystroke, same as every other selection-aware path here) or
     * hit Backspace/Delete to clear it in one motion, no new-tab
     * workaround needed. */
    if (ch == 1) { /* Ctrl+A - select all */
        e->sel_anchor = 0;
        e->cursor = (int)strlen(buf);
        return;
    }
    /* --- cursor movement, selection-aware. Shift+move keeps
     * sel_anchor where it is (so [sel_anchor,cursor) grows/shrinks); an
     * unshifted move collapses the selection to the new cursor. --- */
    if (ks == XK_Left) {
        kh_text_clamp_cursor(buf, &e->cursor);
        if (e->cursor > 0) e->cursor--;
        if (!g_key_shift) e->sel_anchor = e->cursor;
        return;
    }
    if (ks == XK_Right) {
        kh_text_clamp_cursor(buf, &e->cursor);
        if (e->cursor < (int)strlen(buf)) e->cursor++;
        if (!g_key_shift) e->sel_anchor = e->cursor;
        return;
    }
    /* REAL, NEW 2026-09-05 - line-aware (real \n, not display-wrap)
     * Home/End via kh_text_line_start/end() - for cli_io (never
     * contains a real \n) these reduce to the exact same whole-buffer
     * Home/End behavior it always had; for text_area this is the real,
     * expected "jump to start/end of the current line" behavior. */
    if (ks == XK_Home) { e->cursor = kh_text_line_start(buf, e->cursor); if (!g_key_shift) e->sel_anchor = e->cursor; return; }
    if (ks == XK_End) { e->cursor = kh_text_line_end(buf, e->cursor); if (!g_key_shift) e->sel_anchor = e->cursor; return; }
    /* REAL, NEW 2026-09-05 - text_area only: real LOGICAL-line Up/Down
     * (crosses actual \n boundaries, preserving column position best-
     * effort). Real VISUAL (word-wrapped) row Up/Down - i.e. moving
     * within one long logical line that wraps across several on-screen
     * rows - is explicitly NOT attempted here; see this plan's own
     * design doc for why that's real, separate, harder work, not
     * silently guessed at in this first pass. cli_io has no real \n
     * ever, so Up/Down simply does nothing for it - correct, since a
     * single-line composer has nowhere to move to. */
    if (is_area && (ks == XK_Up || ks == XK_Down)) {
        int line_start = kh_text_line_start(buf, e->cursor);
        int col = e->cursor - line_start;
        if (ks == XK_Up) {
            if (line_start > 0) {
                int prev_end = line_start - 1; /* the real \n immediately before this line */
                int prev_start = kh_text_line_start(buf, prev_end);
                int prev_len = prev_end - prev_start;
                e->cursor = prev_start + (col < prev_len ? col : prev_len);
            }
        } else {
            int line_end = kh_text_line_end(buf, e->cursor);
            if (buf[line_end] != '\0') { /* not already the last real line */
                int next_start = line_end + 1;
                int next_end = kh_text_line_end(buf, next_start);
                int next_len = next_end - next_start;
                e->cursor = next_start + (col < next_len ? col : next_len);
            }
        }
        if (!g_key_shift) e->sel_anchor = e->cursor;
        return;
    }
    if (ks == XK_Delete) {
        if (!kh_text_delete_selection(e, buf)) kh_text_delete_at_cursor(buf, &e->cursor);
        if (is_area) default_text_area_save(e); else default_cli_io_save(e);
        return;
    }
    if (ks == XK_BackSpace) {
        if (!kh_text_delete_selection(e, buf)) kh_text_delete_before_cursor(buf, &e->cursor);
        if (is_area) default_text_area_save(e); else default_cli_io_save(e);
        return;
    }
    if (ch >= 32 && ch < 127) {
        kh_text_delete_selection(e, buf); /* typing over a selection replaces it */
        kh_text_insert_at_cursor(buf, cap, &e->cursor, ch);
        e->sel_anchor = e->cursor;
        if (is_area) default_text_area_save(e); else default_cli_io_save(e);
    }
}

/* REAL, NEW 2026-09-05 (GRID-ELEMENT-DESIGN.md) - base-26 multi-letter
 * column codec (A=0..Z=25, AA=26..AZ=51, ...), the same "bijective
 * base-26" scheme spreadsheet column letters always use. Shared by the
 * jump-buffer parser below and by draw_elem()'s own grid header-row
 * rendering (step 3, not yet built). */
static void grid_col_to_letters(int col, char *out, size_t outsz) {
    gj_col_to_letters(col, out, outsz); /* codec lives in _shared-lib/khtpm_grid_jump.c */
}
/* REAL, NEW 2026-09-05 (GRID-ELEMENT-DESIGN.md) - the grid's own key
 * handler, routed to from the same armed-field call site cli_io/
 * text_area use, but with genuinely different semantics (a 2D cursor
 * plus a nested cell-edit sub-state, not one text cursor). Two real
 * sub-states, both scoped to grid_edit_mode:
 *   0 = navigating (# badge) - arrows move the cursor; letters/digits
 *       accumulate into grid_jump_buffer; Enter resolves a pending
 *       jump, or (buffer already empty) ENTERS the cell under the
 *       cursor; Escape disarms the whole grid.
 *   1 = editing one cell (^ badge) - grid_cell_buffer is a real,
 *       live-typed single-line buffer, reusing cli_io's own text-
 *       editing primitives (kh_text_insert_at_cursor/kh_text_delete_*)
 *       directly rather than re-implementing them; Escape COMMITS
 *       (fires the grid's own action=/onclick=, the same generic
 *       dispatch() verb mechanism <tab>/<item> already use - NOT
 *       default_cli_io_run_action()'s raw-shell-command path, since a
 *       commit needs the full in-process dispatch() table, e.g.
 *       CSVH_GRIDCOMMIT) and returns to state 0 - a SECOND Escape from
 *       state 0 is what actually disarms the grid, matching the
 *       design doc's "activating that cell '^' till esc is pressed"
 *       (one Escape ends the cell, a second ends the grid). */
static void default_grid_handle_key(KeySym ks, char ch) {
    Elem *e = g_default_input_elem;
    if (!e) return;
    if (e->grid_edit_mode) {
        /* State 1: editing one cell - same single-line primitives
         * cli_io's own input_buffer editing already uses, pointed at
         * grid_cell_buffer instead. */
        if (ks == XK_Escape) {
            if (e->onclick[0]) dispatch(e->onclick);
            e->grid_edit_mode = 0;
            return;
        }
        if (ks == XK_Return || ks == XK_KP_Enter) return; /* single-line cell - Enter does nothing, same as cli_io has no multi-line concept */
        if (ks == XK_Left) { kh_text_clamp_cursor(e->grid_cell_buffer, &e->cursor); if (e->cursor > 0) e->cursor--; return; }
        if (ks == XK_Right) { kh_text_clamp_cursor(e->grid_cell_buffer, &e->cursor); if (e->cursor < (int)strlen(e->grid_cell_buffer)) e->cursor++; return; }
        if (ks == XK_Home) { e->cursor = 0; return; }
        if (ks == XK_End) { e->cursor = (int)strlen(e->grid_cell_buffer); return; }
        if (ks == XK_Delete) { kh_text_delete_at_cursor(e->grid_cell_buffer, &e->cursor); return; }
        if (ks == XK_BackSpace) { kh_text_delete_before_cursor(e->grid_cell_buffer, &e->cursor); return; }
        if (ch >= 32 && ch < 127) kh_text_insert_at_cursor(e->grid_cell_buffer, sizeof(e->grid_cell_buffer), &e->cursor, ch);
        return;
    }
    /* State 0: navigating - the pure cursor/jump-buffer logic is the shared
     * khtpm_grid_jump.c step function (also usable by overlay pickers). No hard
     * upper cap on the cursor here - the MANAGER is the real bounds authority
     * (SETCELL already rejects out-of-range refs), so rows/cols stay 0. */
    GjState gs;
    memset(&gs, 0, sizeof(gs));
    snprintf(gs.jump, sizeof(gs.jump), "%s", e->grid_jump_buffer);
    gs.row = e->grid_cur_row;
    gs.col = e->grid_cur_col;
    GjKey gk = GJ_KEY_CHAR;
    if (ks == XK_Escape) gk = GJ_KEY_ESC;
    else if (ks == XK_Up) gk = GJ_KEY_UP;
    else if (ks == XK_Down) gk = GJ_KEY_DOWN;
    else if (ks == XK_Left) gk = GJ_KEY_LEFT;
    else if (ks == XK_Right) gk = GJ_KEY_RIGHT;
    else if (ks == XK_BackSpace) gk = GJ_KEY_BACKSPACE;
    else if (ks == XK_Return || ks == XK_KP_Enter) gk = GJ_KEY_ENTER;
    else if (!(ch >= 32 && ch < 127)) return;
    GjAction ga = gj_step(&gs, gk, ch);
    snprintf(e->grid_jump_buffer, sizeof(e->grid_jump_buffer), "%s", gs.jump);
    e->grid_cur_row = gs.row;
    e->grid_cur_col = gs.col;
    if (ga == GJ_DISARM) { kh_set_default_input_elem(NULL); kh_ungrab_kbd(); return; }
    if (ga == GJ_ENTER_CELL) {
        /* Nothing pending - a second real Enter ENTERS the cell under the
         * cursor (state 0 -> state 1), matching the design doc's own
         * disambiguation (two different "Enter does something" cases, not
         * two different keys). grid_cell_buffer is SEEDED with the cursor's
         * current real cell value (published by the app's own manager as a
         * per-cell var, e.g. "cell_2_1"); the var-name PREFIX is read from
         * this grid's own target_id= (defaults to "cell_"), keeping the
         * element generic, same as commit_action reusing onclick=. */
        char varname[80];
        snprintf(varname, sizeof(varname), "%s%d_%d", e->target_id[0] ? e->target_id : "cell_", e->grid_cur_row, e->grid_cur_col);
        snprintf(e->grid_cell_buffer, sizeof(e->grid_cell_buffer), "%s", kh_get_var(varname));
        e->grid_edit_mode = 1;
        e->cursor = (int)strlen(e->grid_cell_buffer);
    }
}

static void activate_focused(void) {
    if (g_focus_nav < 1 || g_focus_nav > g_n_nav) return;
    Elem *item = g_nav[g_focus_nav - 1];
    if (!kh_elem_in_scope(item)) return;
    /* REAL FIX 2026-08-31 - see default_cli_io_handle_key()'s own
     * Escape-branch comment for the full real diagnosis. Grab taken
     * HERE (arm time), released on every real disarm path. */
    if (strcmp(item->tag, "cli_io") == 0 || strcmp(item->tag, "text_area") == 0) {
        /* REAL FIX 2026-09-14, direct live report ("i tried selecting
         * it didn't work"... "theres some finnicky focus issues with
         * the text edit hq space") - this field can already be armed
         * AND already be the live g_default_input_elem when
         * activate_focused() fires again (text-edit-hq's own debug log
         * shows repeated re-arms far more often than a single click-in
         * explains) - the arm-at-end-of-buffer/collapse-selection logic
         * below used to run every single time regardless, silently
         * wiping any in-progress Shift+Arrow selection between the
         * user's own keystrokes. kh_grab_keyboard_retry() still runs
         * unconditionally below (cheap, idempotent if the grab is
         * already held - and the real fix if THIS is what's
         * re-establishing a grab that keeps genuinely dying, a
         * separate, not-yet-root-caused question). Only the cursor/
         * selection reset is now skipped for a redundant re-trigger on
         * the SAME already-armed field; a genuinely NEW arm (a
         * different field, or this one after really being disarmed)
         * still gets the real re-focus convention below unchanged. */
        int already_armed = (g_default_input_elem == item);
        kh_set_default_input_elem(item);
        if (!already_armed) {
            /* REAL, NEW 2026-09-05 - arm at the end of whatever's already
             * typed, matching every normal editor's own re-focus
             * convention, not wherever a stale cursor happened to be left.
             * text_area added alongside cli_io here (same real armed-field
             * mechanism, same ^ indicator, same Escape-disarm path). */
            item->cursor = (int)strlen(strcmp(item->tag, "text_area") == 0 ? item->text_area_buffer : item->input_buffer);
            item->sel_anchor = item->cursor; /* arm with no stale selection */
        }
        kh_grab_keyboard_retry();
        return;
    }
    /* REAL, NEW 2026-09-05 (GRID-ELEMENT-DESIGN.md) - <grid> arms into
     * its own state 0 (navigating, # badge) - grid_cur_row/col are
     * deliberately NOT reset here, so re-entering an already-used grid
     * resumes where the cursor last was, same "pick up where you left
     * off" spirit cli_io/text_area's own end-of-buffer cursor seed
     * has. grid_jump_buffer/grid_edit_mode DO reset - a stale pending
     * jump or a still-"editing" flag from a previous arm would be a
     * real, confusing leftover state to resume into. */
    if (strcmp(item->tag, "grid") == 0) {
        kh_set_default_input_elem(item);
        item->grid_jump_buffer[0] = '\0';
        item->grid_edit_mode = 0;
        kh_grab_keyboard_retry();
        return;
    }
    /* generic <tab>: run the tab's own action=, mark it active for CSS.
     *
     * REAL FIX 2026-09-05, direct live report ("nav has to work for
     * entire window... [text_area at nav 6] unreachable by mouse or
     * nav arrows"): a <tab> only scope-confines nav into a container
     * when it has an EXPLICIT target_id= (db-hq/events-hq's real
     * content-switching tabs - "pick a tab, then navigate its record
     * list"). A <tab> with NO target_id= is a plain menu-bar action
     * button (text-edit-hq/csv-hq/pdl-read/media-* New/Open/Save/etc) -
     * it must NOT trap nav anywhere, or the app's MAIN content (a
     * <text_area> or <grid> in the <panel>, not the <sidebar>) becomes
     * permanently unreachable the moment you press one. The old
     * fallback (`if (!sb) sb = find_by_tag(pg,"sidebar")`) confined
     * every no-target_id tab to whatever sidebar the page happened to
     * have - the exact bug. Every real content-switching tab in the
     * house already sets target_id= explicitly, so dropping the
     * fallback changes behavior only for the plain action tabs, which
     * is the fix. */
    if (strcmp(item->tag, "tab") == 0) {
        if (item->id[0])
            snprintf(g_default_active_tab_id, sizeof(g_default_active_tab_id), "%s", item->id);
        if (item->onclick[0]) dispatch(item->onclick);
        if (item->target_id[0]) {
            Elem *sb = find_by_id(g_window, item->target_id);
            if (sb) {
                g_default_active_scope_root = sb;
                g_default_scope_confine = 1;
                snprintf(g_default_active_scope_id, sizeof(g_default_active_scope_id), "%s", item->id);
                kh_focus_first_child_in_scope();
            }
        }
        return;
    }
    /* REAL, NEW 2026-09-03 - real, generic dropdown trigger, checked
     * BEFORE the generic dispatch() fallback (same real "an element
     * with its own recognized onclick verb handles itself" order
     * dbhq_activate_elem() already uses for its own ACTIVATE check).
     * Clicking the trigger again while already open closes it (a real
     * toggle, not just an open-only action) - matches ordinary
     * dropdown/menu-button behavior everywhere else, not invented here. */
    if (strncmp(item->onclick, "ACTIVATE", 8) == 0 &&
        (item->onclick[8] == '\0' || item->onclick[8] == ' ')) {
        int same = (g_default_active_scope_root == item) ||
            (item->id[0] && g_default_active_scope_id[0] &&
             strcmp(item->id, g_default_active_scope_id) == 0);
        if (same) {
            g_default_active_scope_root = NULL;
            g_default_active_scope_id[0] = '\0';
            g_default_scope_confine = 0;
            if (strncmp(item->id, "strip-cell-", 11) == 0) {
                char hist[PATH_BUF];
                snprintf(hist, sizeof(hist), "%s/#.desktop/strip_history.txt", g_house_root);
                FILE *hf = fopen(hist, "a");
                if (hf) { fprintf(hf, "27\n"); fclose(hf); }
            }
        } else {
            /* Scope into the CONTAINER named by target_id (its subtree
             * becomes the nav scope - spirit of dbhq_activate_scope() /
             * INTERACT), or the trigger itself if no target_id. The
             * trigger id is what Esc + the toggle above key off. */
            Elem *scope = item;
            g_default_scope_confine = 0;
            if (item->target_id[0]) {
                Elem *c = find_by_id(g_window, item->target_id);
                if (c) { scope = c; g_default_scope_confine = 1; }
            }
            g_default_active_scope_root = scope;
            snprintf(g_default_active_scope_id, sizeof(g_default_active_scope_id), "%s", item->id);
            if (g_default_scope_confine) kh_focus_first_child_in_scope();
            /* `onclick="ACTIVATE '<script>' '<verb>'"` - run the trailing
             * command too, so one click can both switch content and
             * scope into it (e.g. a db-hq-pal tab). */
            {
                const char *rest = item->onclick + 8;
                while (*rest == ' ') rest++;
                if (*rest) dispatch(rest);
            }
            if (strncmp(item->id, "strip-cell-", 11) == 0) {
                int n = atoi(item->id + 11);
                char hist[PATH_BUF];
                snprintf(hist, sizeof(hist), "%s/#.desktop/strip_history.txt", g_house_root);
                FILE *hf = fopen(hist, "a");
                if (hf) { fprintf(hf, "%d\n", 4000 + n); fclose(hf); }
            }
        }
        return;
    }
    /* A dropdown-child's own real action (its "action=" attribute, run
     * via dispatch() below through the normal fallthrough) should also
     * close the menu it came from - a selected option leaving its own
     * dropdown open is a real, if minor, real-world UX bug this avoids
     * for free, not a separate feature to build later. */
    if (elem_has_class(item, "dropdown-child") && strcmp(item->onclick, "ZORDER_TOGGLE") != 0) {
        g_default_active_scope_root = NULL;
        g_default_active_scope_id[0] = '\0';
        g_default_scope_confine = 0;
    }
    /* REAL, NEW 2026-09-03 - generic scrollbar up/down arrows
     * (generic_sbar_register()'s own header comment), same real
     * "recognized onclick verb, checked before the generic dispatch()
     * fallback" order as ACTIVATE right above - "SCROLLUP:<i>"/
     * "SCROLLDOWN:<i>" are never real shell commands, dispatch() must
     * never see them. Index-matched into g_generic_sbars[], the SAME
     * array this frame's own layout pass just populated - clamped
     * against its own real max_scroll, not a guessed bound. */
    if (strncmp(item->onclick, "SCROLLUP:", 9) == 0 || strncmp(item->onclick, "SCROLLDOWN:", 11) == 0) {
        int up = item->onclick[6] == 'U';
        int i = atoi(item->onclick + (up ? 9 : 11));
        if (i >= 0 && i < g_n_generic_sbars && g_generic_sbars[i].scroll) {
            int *sc = g_generic_sbars[i].scroll;
            *sc += up ? -1 : 1;
            if (*sc < 0) *sc = 0;
            if (*sc > g_generic_sbars[i].max_scroll) *sc = g_generic_sbars[i].max_scroll;
        }
        return;
    }
    /* REAL, NEW 2026-09-14 (generic `<bar>` click-to-seek, same
     * "recognized element handles itself before generic dispatch()"
     * order as ACTIVATE/SCROLLUP right above) - a <bar> onClick carries a
     * literal `%FRAC` placeholder (set by the projector) that must hold
     * the 0.0..1.0 fraction of the exact click point: Substitute the
     * real value from g_bar_click_x (popup_handle_click()'s own bar
     * hit-test), then dispatch the finished command. With no pending
     * click (Enter on a focused bar) the placeholder resolves to the
     * playhead's own current fraction when max>0, else 0.5 - a same-spot
     * Enter is then a harmless no-op seek for the common video consumer. */
    if (strcmp(item->tag, "bar") == 0 && item->onclick[0]) {
        double frac;
        if (g_bar_click_x >= item->x) {
            frac = (item->w > 0) ? (double)(g_bar_click_x - item->x) / item->w : 0.0;
        } else if (item->bar_max > 0) {
            frac = (double)item->bar_value / item->bar_max;
        } else {
            frac = 0.5;
        }
        if (frac < 0.0) frac = 0.0;
        if (frac > 1.0) frac = 1.0;
        char cmd[sizeof(item->onclick) + 24];
        const char *ph = strstr(item->onclick, "%FRAC");
        if (ph) {
            char frac_s[16];
            snprintf(frac_s, sizeof(frac_s), "%.4f", frac);
            size_t pre = (size_t)(ph - item->onclick);
            size_t post = strlen(ph + 5);
            if (pre + strlen(frac_s) + post < sizeof(cmd)) {
                memcpy(cmd, item->onclick, pre);
                memcpy(cmd + pre, frac_s, strlen(frac_s));
                memcpy(cmd + pre + strlen(frac_s), ph + 5, post + 1);
            } else {
                snprintf(cmd, sizeof(cmd), "%s", item->onclick);
            }
        } else {
            snprintf(cmd, sizeof(cmd), "%s", item->onclick);
        }
        g_bar_click_x = -1; /* one pending bar click, one substitution */
        dispatch(cmd);
        return;
    }
    if (item->onclick[0]) dispatch(item->onclick);
}

/* REAL, NEW 2026-09-06 - restore the HQ-menu "cli" terminal mirror.
 * khtpm_strip_render_ascii.+x read #.desktop/strip_frame.cells.pdl, which
 * the retired khtpm_strip_parser.+x wrote; both went away 2026-09-01 when
 * the strip folded into this binary, so the mirror froze (last cells.pdl
 * write Sep 3). This walks the LIVE laid-out Elem tree and writes the
 * same readable frame file straight from redraw(), so a terminal
 * `tail -F #.desktop/strip_ascii_current_frame.txt` IS the live strip.
 * Dock mode only; a no-op for every other window. Cheap: two small
 * fopen("w")+tree-walk per repaint, and the strip only repaints on a
 * real state change. */
/* Header-cell / bottom-tab focus lives in the MANAGER's strip_state.txt
 * (strip_focus_cell: 0-based cell, or -1 meaning focus is on a bottom
 * tab at index tab_focus_idx). Read once per frame; used by the walk so
 * arrow-key nav is actually visible in the terminal mirror. */
static int g_dock_focus_cell = -1;   /* 0-based, -1 = on a tab */
static int g_dock_tab_focus  = 0;

/* REAL, NEW 2026-09-06 - the terminal-input half of the mirror.
 * khtpm_strip_keyboard_ascii.+x relays arrow/digit/Enter keys to
 * #.desktop/strip_history.txt; the manager consumes them and republishes
 * strip_state.txt (strip_focus_cell / tab_focus_idx / hq_open / ...).
 * The X11 strip repaints on its own X events, but a state change driven
 * purely from the terminal produces NO X event here, so redraw() (and
 * with it dock_write_ascii_frame()) never ran and the ASCII mirror
 * lagged - it only caught up on the next unrelated repaint.
 *
 * MARKER-DRIVEN (DIAMOND standard, 2026-09-06 - see
 * 02-architecture/reference/TPMOS-DIAMOND-render-chain.md): the manager
 * already appends one byte to #.desktop/strip_frame_changed.txt on every
 * publish_state() (touch_frame_changed(), modelled on chtpm_parser.c's
 * frame_changed.txt). We watch that marker's SIZE growth - never mtime -
 * so there is no wall-clock-resolution race and no "same second"
 * dropped republish. On growth: re-read strip_state.txt, map the
 * manager's focus cursor onto g_focus_nav, redraw once. */
static void dock_poll_strip_state(void) {
    if (!window_is_dock()) return;
    static long s_marker = -1;
    char mp[PATH_BUF];
    snprintf(mp, sizeof(mp), "%s/#.desktop/strip_frame_changed.txt", g_house_root);
    struct stat mst;
    if (stat(mp, &mst) != 0) return;
    if (s_marker < 0)               { s_marker = mst.st_size; return; }  /* first sight */
    if (mst.st_size < s_marker)     { s_marker = mst.st_size; return; }  /* truncated/rotated - resync */
    if (mst.st_size == s_marker)      return;                            /* nothing new */
    s_marker = mst.st_size;

    char sp[PATH_BUF];
    snprintf(sp, sizeof(sp), "%s/#.desktop/strip_state.txt", g_house_root);

    /* The manager just republished. Pull its focus cursor
     * (strip_focus_cell: 0-based header cell, or -1 = a bottom tab at
     * tab_focus_idx) and map it onto THIS renderer's g_focus_nav so a
     * key driven from the terminal moves the real on-screen highlight,
     * not just the ASCII mirror. Header cells are g_focus_nav
     * 1..g_dock_header_nav_hi; bottom-bar tabs continue above that. */
    int sfc = -2, tfi = 0;
    FILE *s = fopen(sp, "r");
    if (s) {
        char line[512];
        while (fgets(line, sizeof(line), s)) {
            if (strncmp(line, "KEY | strip_focus_cell | ", 25) == 0) sfc = atoi(line + 25);
            else if (strncmp(line, "KEY | tab_focus_idx | ", 22) == 0) tfi = atoi(line + 22);
        }
        fclose(s);
    }
    if (sfc >= 0) {
        int nv = sfc + 1;
        if (nv >= 1 && nv <= g_n_nav) g_focus_nav = nv;
    } else if (sfc == -1) {
        int nv = g_dock_header_nav_hi + 1 + tfi;
        if (nv >= 1 && nv <= g_n_nav) g_focus_nav = nv;
    }
    if (!g_quit) redraw();
}

static void dock_ascii_walk(FILE *f, Elem *e, int depth) {
    if (!e) return;
    if (e->y < -1000) { return; }   /* parked offscreen (a closed menu's rows etc.) - not on screen, not in the mirror */
    int has_label = e->label[0] != '\0';
    int is_container = (strcmp(e->tag, "window") == 0 || strcmp(e->tag, "page") == 0 ||
                        strcmp(e->tag, "sidebar") == 0 || strcmp(e->tag, "panel") == 0 ||
                        strcmp(e->tag, "row") == 0 || strcmp(e->tag, "tabbar") == 0 ||
                        strcmp(e->tag, "scrolllist") == 0 || strcmp(e->tag, "module") == 0);
    if ((has_label || (!is_container && e->nav_index > 0)) && strcmp(e->tag, "module") != 0) {
        for (int i = 0; i < depth; i++) fputs("  ", f);
        if (e->nav_index > 0) {
            /* Mark the ONE cell this renderer is actually highlighting on
             * screen: g_focus_nav (1-based, unified across header cells
             * 1..g_dock_header_nav_hi then bottom-bar tabs above that).
             * 2026-09-06 fix for the split-brain the user hit: this used
             * to read the MANAGER's strip_focus_cell, which is a separate
             * cursor the manager only moves for terminal-relay input - so
             * driving the X11 strip with the mouse/keys moved g_focus_nav
             * (what you see) but not strip_focus_cell (what the mirror
             * drew), and the two disagreed. Now the mirror always shows
             * the real highlight; terminal input reaches g_focus_nav via
             * dock_poll_strip_state() below, so both stay in lockstep. */
            fprintf(f, "%s%d. ", (e->nav_index == g_focus_nav) ? "[>] " : "[ ] ", e->nav_index);
        } else if (!is_container)
            fputs("    ", f);
        int active = 0;
        for (int i = 0; i < e->n_classes; i++)
            if (strcmp(e->classes[i], "active") == 0 || strcmp(e->classes[i], "tab-active") == 0 ||
                strcmp(e->classes[i], "room-active") == 0) active = 1;
        fprintf(f, "%s%s%s\n", active ? "* " : "",
                has_label ? e->label : e->tag,
                (e->y < -1000) ? "  (hidden)" : "");
    }
    for (int i = 0; i < e->n_children; i++)
        dock_ascii_walk(f, e->children[i], depth + (is_container ? 1 : 0));
}

/* The dynamic strip bits (which header cell is focused, whether the HQ
 * popup menu is open + its rows + focus) live in the MANAGER's
 * strip_state.txt, not this renderer's g_window tree - the popup is a
 * separate window. Fold them into the mirror by reading that file. */
static void dock_ascii_append_state(FILE *f) {
    char sp[PATH_BUF];
    snprintf(sp, sizeof(sp), "%s/#.desktop/strip_state.txt", g_house_root);
    FILE *s = fopen(sp, "r");
    if (!s) return;
    char line[1024];
    int hq_open = 0, hq_focus = -1, tab_focus = -1;
    int n_hqitems = 0;
    char hqitems[24][160];
    while (fgets(line, sizeof(line), s)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strncmp(line, "KEY | hq_open | ", 16) == 0)      hq_open  = atoi(line + 16);
        else if (strncmp(line, "KEY | hq_focus | ", 17) == 0) hq_focus = atoi(line + 17);
        else if (strncmp(line, "KEY | tab_focus_idx | ", 22) == 0) tab_focus = atoi(line + 22);
        else if (strncmp(line, "HQITEM | ", 9) == 0 && n_hqitems < 24) {
            const char *p = line + 9;
            const char *bar = strchr(p, '|');
            size_t len = bar ? (size_t)(bar - p) : strlen(p);
            while (len > 0 && p[len - 1] == ' ') len--;
            if (len >= sizeof(hqitems[0])) len = sizeof(hqitems[0]) - 1;
            memcpy(hqitems[n_hqitems], p, len);
            hqitems[n_hqitems][len] = '\0';
            n_hqitems++;
        }
    }
    fclose(s);
    (void)tab_focus;
    if (hq_open && n_hqitems > 0) {
        fprintf(f, "--- HQ menu (open) ---\n");
        for (int i = 0; i < n_hqitems; i++)
            fprintf(f, "  %s%d. %s\n", (i == hq_focus) ? "[>] " : "[ ] ", i + 1, hqitems[i]);
    }
}

static void dock_write_ascii_frame(void) {
    if (!window_is_dock() || !g_window) return;

    /* pull the manager's focus cursor before walking the tree */
    g_dock_focus_cell = -1;
    g_dock_tab_focus  = 0;
    {
        char sp[PATH_BUF];
        snprintf(sp, sizeof(sp), "%s/#.desktop/strip_state.txt", g_house_root);
        FILE *s = fopen(sp, "r");
        if (s) {
            char line[512];
            while (fgets(line, sizeof(line), s)) {
                if (strncmp(line, "KEY | strip_focus_cell | ", 25) == 0) g_dock_focus_cell = atoi(line + 25);
                else if (strncmp(line, "KEY | tab_focus_idx | ", 22) == 0) g_dock_tab_focus = atoi(line + 22);
            }
            fclose(s);
        }
    }

    char path[PATH_BUF], hpath[PATH_BUF];
    snprintf(path,  sizeof(path),  "%s/#.desktop/strip_ascii_current_frame.txt", g_house_root);
    snprintf(hpath, sizeof(hpath), "%s/#.desktop/strip_ascii_frame_history.txt", g_house_root);
    char ts[32];
    time_t now = time(NULL);
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&now));

    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "--- TASKBAR FRAME  %s ---\n", ts);
        dock_ascii_walk(f, g_window, 0);
        if (g_dock_peer) { fprintf(f, "--- bottom bar ---\n"); dock_ascii_walk(f, g_dock_peer, 0); }
        dock_ascii_append_state(f);
        fclose(f);
        /* DIAMOND: the compose->present marker, exactly like
         * chtpm_parser.c appends "P\n" to renderer_pulse.txt after
         * writing current_frame.txt. khtpm_strip_render_ascii.+x (the
         * terminal presenter) watches THIS file's size growth - not the
         * frame file's mtime - so a frame that is byte-identical in
         * length (a single-digit nav number moving) still triggers a
         * repaint. */
        char pp[PATH_BUF];
        snprintf(pp, sizeof(pp), "%s/#.desktop/strip_ascii_pulse.txt", g_house_root);
        struct stat pst;
        /* size IS the signal, so rotate (not append) once it gets big -
         * the presenter's `size < last` branch treats the shrink as a
         * resync, never a missed frame. */
        const char *mode = (stat(pp, &pst) == 0 && pst.st_size > 64 * 1024) ? "w" : "a";
        FILE *pf = fopen(pp, mode);
        if (pf) { fputc('P', pf); fclose(pf); }
    }
    FILE *h = fopen(hpath, "a");
    if (h) {
        struct stat hst;
        if (fstat(fileno(h), &hst) == 0 && hst.st_size > 512 * 1024) { fclose(h); h = fopen(hpath, "w"); }
    }
    if (h) {
        fprintf(h, "\n=== %s ===\n", ts);
        dock_ascii_walk(h, g_window, 0);
        fclose(h);
    }
}

/* Mode-agnostic sibling of dock_write_ascii_frame() (TERMINAL-MIRROR-
 * PARITY-all-windows.md step 1). Every NON-dock window this binary
 * renders drops a live text mirror of its laid-out Elem tree, keyed by
 * PID, next to the per-PID input relay that poll_agent_history() already
 * consumes:
 *   #.desktop/ascii_frames/<pid>.frame.txt   - the readable frame
 *   #.desktop/ascii_frames/<pid>.pulse.txt   - DIAMOND marker (size grows
 *                                              per frame, rotates >64KB)
 * A generic presenter (khtpm_render_ascii.+x <house> <pid>) tails the
 * pulse and prints the frame; a generic keyboard relay writes
 * KEY_PRESSED lines into #.desktop/entity_menu_history/<pid>.txt. The
 * dock keeps its own strip_ascii_* filenames (the cli launcher + docs
 * point at them) - it's the already-shipped special case. */
/* REAL, NEW 2026-09-10 - frame-history receipts design doc,
 * kh_write_ascii_frame()'s own helpers. FNV1a over the canonical frame
 * text: the "did the frame really change" test (only_on_change, the
 * receipts.conf policy key the design maps 1:1) - byte-identical
 * re-renders append nothing to history and mint no receipts. */
static unsigned long long kh_fnv1a64(const unsigned char *p, size_t n) {
    unsigned long long h = 1469598103934665603ULL;
    for (size_t i = 0; i < n; i++) { h ^= (unsigned char)p[i]; h *= 1099511628211ULL; }
    return h;
}

/* L1 - the durable, append-only per-window frame history, mirroring the
 * dock's own strip_ascii_frame_history.txt discipline (timestamped block
 * header, rotate past 512KB) on the per-PID non-dock mirror. This is an
 * OUTPUT audit trail, not an input relay (no byte-cursor consumer), so
 * plain append + rotate is the whole story; the caller gates it on real
 * change so idle repaints never bloat it. */
static void kh_append_ascii_history(const char *dir, int pid, const char *base, const char *ts,
                                    const char *fbuf, size_t flen) {
    char hpath[PATH_BUF];
    snprintf(hpath, sizeof(hpath), "%s/%d.frame_history.txt", dir, pid);
    FILE *h = fopen(hpath, "a");
    if (h) {
        struct stat st;
        if (fstat(fileno(h), &st) == 0 && st.st_size > 512 * 1024) { fclose(h); h = fopen(hpath, "w"); }
    }
    if (!h) return;
    fprintf(h, "\n=== %s pid %d  %s ===\n", base, pid, ts);
    fwrite(fbuf, 1, flen, h);
    fclose(h);
}

/* L2 - OBJECT-lines pdl: one machine-readable row per element the
 * readable mirror actually shows (dock_ascii_walk's own row predicate:
 * has a label or a real nav number, and is not a bare container), with
 * the real laid-out geometry + nav + active state. This is the TMOS
 * scene.objects.pdl shape, so two snapshots diff as data, not pixels. */
static void kh_receipt_objects_walk(FILE *f, Elem *e) {
    if (!e) return;
    int is_container = (strcmp(e->tag, "window") == 0 || strcmp(e->tag, "page") == 0 ||
                        strcmp(e->tag, "sidebar") == 0 || strcmp(e->tag, "panel") == 0 ||
                        strcmp(e->tag, "row") == 0 || strcmp(e->tag, "tabbar") == 0 ||
                        strcmp(e->tag, "scrolllist") == 0 || strcmp(e->tag, "module") == 0);
    if (e->y >= -1000 && !is_container && (e->label[0] != '\0' || e->nav_index > 0)) {
        char classes_joined[CSS_MAX_CLASSES * 33] = "";
        for (int i = 0; i < e->n_classes; i++) {
            if (i > 0) strcat(classes_joined, ",");
            strcat(classes_joined, e->classes[i]);
        }
        fprintf(f, "OBJECT tag=%s id=%s class=%s x=%d y=%d w=%d h=%d nav=%d active=%d label=%s\n",
                e->tag[0] ? e->tag : "-", e->id[0] ? e->id : "-",
                classes_joined[0] ? classes_joined : "-",
                e->x, e->y, e->w, e->h, e->nav_index, e->active,
                e->label[0] ? e->label : "-");
    }
    for (int i = 0; i < e->n_children; i++) kh_receipt_objects_walk(f, e->children[i]);
}

static int s_receipt_seq = 0;

/* L2 - the TMOS/wraith-shaped receipt PDL for one real rendered frame:
 * serials, the source frame/history files, a FNV1a checksum of the
 * canonical frame text, geometry + focus, and the linked OBJECTs pdl.
 * <pid>.receipt.pdl is the always-current mirror; <pid>.<seq>.* are
 * locked snapshots. L3 - the one-line-per-snapshot index ledger, bounded
 * to the last 50 entries (receipts.conf max_entries, built-in default -
 * no config file required, per the design doc). */
static void kh_write_frame_receipt(const char *dir, int pid, const char *base, unsigned long long dig) {
    int seq = ++s_receipt_seq;
    char rlat[PATH_BUF], rsnap[PATH_BUF], op[PATH_BUF];
    snprintf(rlat,  sizeof(rlat),  "%s/%d.receipt.pdl", dir, pid);
    snprintf(rsnap, sizeof(rsnap), "%s/%d.%d.receipt.pdl", dir, pid, seq);
    snprintf(op,    sizeof(op),    "%s/%d.%d.objects.pdl", dir, pid, seq);

    time_t now = time(NULL);
    char iso[40];
    strftime(iso, sizeof(iso), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
    const char *label = (g_window && g_window->label[0]) ? g_window->label : base;

    FILE *of = fopen(op, "w");
    if (of) { kh_receipt_objects_walk(of, g_window); fclose(of); }

    char *rbuf = NULL; size_t rlen = 0;
    FILE *r = open_memstream(&rbuf, &rlen);
    if (!r) return;
    fprintf(r, "receipt_type=kh_x11_framesnapshot\n");
    fprintf(r, "generated_by=khtpm_core_render\n");
    fprintf(r, "generated_at_epoch=%ld\n", (long)now);
    fprintf(r, "generated_at_iso_utc=%s\n", iso);
    fprintf(r, "receipt_generation_key=%d@%ld\n", pid, (long)now);
    fprintf(r, "window=%s\n", label);
    fprintf(r, "chtpm=%s\n", base);
    fprintf(r, "page=%s\n", g_current_page[0] ? g_current_page : "-");
    fprintf(r, "vars=%s\n", g_vars_path[0] ? g_vars_path : "-");
    fprintf(r, "source_frame_txt=ascii_frames/%d.frame.txt\n", pid);
    fprintf(r, "source_history_txt=ascii_frames/%d.frame_history.txt\n", pid);
    fprintf(r, "frame_checksum_fnv1a64=0x%016llx\n", dig);
    fprintf(r, "viewport_w=%d\n", g_win_w);
    fprintf(r, "viewport_h=%d\n", g_win_h);
    fprintf(r, "focus_nav=%d\n", g_focus_nav);
    fprintf(r, "n_nav=%d\n", g_n_nav);
    fprintf(r, "objects_pdl=ascii_frames/%d.%d.objects.pdl\n", pid, seq);
    fprintf(r, "png=-\n");
    fclose(r);

    FILE *fa = fopen(rsnap, "w");
    if (fa) { fwrite(rbuf, 1, rlen, fa); fclose(fa); }
    FILE *fl = fopen(rlat, "w");
    if (fl) { fwrite(rbuf, 1, rlen, fl); fclose(fl); }
    free(rbuf);

    char ipath[PATH_BUF];
    snprintf(ipath, sizeof(ipath), "%s/index.txt", dir);
    char all[1024 * 512] = "";
    size_t alln = 0;
    { FILE *fi = fopen(ipath, "r");
      if (fi) {
          long sz; fseek(fi, 0, SEEK_END); sz = ftell(fi); fseek(fi, 0, SEEK_SET);
          if (sz > 0 && sz < (long)sizeof(all) - 1) { alln = (size_t)fread(all, 1, (size_t)sz, fi); all[alln] = '\0'; }
          fclose(fi); } }
    int nl = 0;
    for (char *p = all; (p = strchr(p, '\n')) != NULL; p++) nl++;
    if (nl >= 50) {
        char *p = all;
        for (int i = 0; i < nl - 49; i++) { char *n = strchr(p, '\n'); if (!n) break; p = n + 1; }
        FILE *fo = fopen(ipath, "w");
        if (fo) { fputs(p, fo); fclose(fo); }
    }
    FILE *fa2 = fopen(ipath, "a");
    if (fa2) {
        /* label may carry spaces - index uses the chtpm basename (safe
         * whitespace-splittable third field) + the checksum for the diff */
        fprintf(fa2, "%s %d %s %d 0x%016llx\n", iso, pid, base, seq, dig);
        fclose(fa2);
    }
}

static void kh_write_ascii_frame(void) {
    if (!g_window || window_is_dock()) return;

    char dir[PATH_BUF], fpath[PATH_BUF], ppath[PATH_BUF];
    snprintf(dir, sizeof(dir), "%s/#.desktop/ascii_frames", g_house_root);
    mkdir(dir, 0777);
    snprintf(fpath, sizeof(fpath), "%s/%d.frame.txt", dir, (int)getpid());
    snprintf(ppath, sizeof(ppath), "%s/%d.pulse.txt", dir, (int)getpid());

    const char *base = g_chtpm_path;
    { const char *sl = strrchr(base, '/'); if (sl) base = sl + 1; }
    char ts[32];
    time_t now = time(NULL);
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&now));

    /* Serialize the readable frame into memory ONCE - the SAME bytes
     * feed (a) the live <pid>.frame.txt overwrite, (b) the DIAMOND
     * pulse, and (c) the on-change history + receipt writers, so the
     * presenter, the live file, the durable history block and the
     * receipt all describe one canonical serialization - no drift. */
    char *fbuf = NULL; size_t flen = 0;
    FILE *ms = open_memstream(&fbuf, &flen);
    if (!ms) return;
    fprintf(ms, "--- %s  pid %d  %s ---\n", base, (int)getpid(), ts);
    if (g_current_page[0]) fprintf(ms, "--- page: %s ---\n", g_current_page);
    dock_ascii_walk(ms, g_window, 0);
    fclose(ms);

    FILE *f = fopen(fpath, "w");
    if (f) { fwrite(fbuf, 1, flen, f); fclose(f); }

    struct stat pst;
    const char *mode = (stat(ppath, &pst) == 0 && pst.st_size > 64 * 1024) ? "w" : "a";
    FILE *pf = fopen(ppath, mode);
    if (pf) { fputc('P', pf); fclose(pf); }

    /* only_on_change: byte-identical re-renders (idle mouse-move
     * repaints) append no history block and mint no receipts */
    static unsigned long long s_last_dig = 0;
    static int s_init = 0;
    unsigned long long dig = kh_fnv1a64((const unsigned char *)fbuf, flen);
    if (!s_init || dig != s_last_dig) {
        s_init = 1; s_last_dig = dig;
        kh_append_ascii_history(dir, (int)getpid(), base, ts, fbuf, flen);
        kh_write_frame_receipt(dir, (int)getpid(), base, dig);
    }
    free(fbuf);
}

/* Real cleanup counterpart - a closed window's live frame/pulse pair
 * shouldn't linger. The frame_history.txt receipt/history stream is a
 * durable audit trail (frame-history receipts design doc), so it stays
 * AFTER the window closes. Called from the same quit paths as
 * history_unregister(). */
static void kh_ascii_frame_unregister(void) {
    char p[PATH_BUF];
    snprintf(p, sizeof(p), "%s/#.desktop/ascii_frames/%d.frame.txt", g_house_root, (int)getpid());
    unlink(p);
    snprintf(p, sizeof(p), "%s/#.desktop/ascii_frames/%d.pulse.txt", g_house_root, (int)getpid());
    unlink(p);
    kh_drop_zone_unregister();
}

static void redraw(void) {
    /* --headless: there is no window to blit to. redraw() is the one
     * choke point every "something changed, repaint" path funnels
     * through, so intercept it here - run the real layout, then write
     * the text frame + DIAMOND pulse instead of drawing pixels. Every
     * caller (idle tick, reparse, dispatch verbs, dock nav) then just
     * works headless with no per-caller guard. */
    if (g_headless) {
        assign_nav_and_layout();
        if (window_is_dock()) dock_write_ascii_frame();
        else kh_write_ascii_frame();
        return;
    }
    if (getenv("KH_REDRAW_TRACE")) {
        static int rc = 0; struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
        void *ra = __builtin_return_address(0);
        fprintf(stderr, "[REDRAW %d] t=%ld.%03ld ra=%p dirty=%d scope=%d tab=%s\n",
                ++rc, (long)ts.tv_sec, ts.tv_nsec/1000000, ra,
                g_frame_dirty, g_default_scope_confine, g_default_active_tab_id);
        fflush(stderr);
    }
    /* REAL §5d.12 (2026-08-16) - chat-hai mode: chai_redraw() is
     * self-contained (own layout, own present via XGetImage->XPutImage,
     * own frame-history append) - ported verbatim, not split into a
     * content-only half like db-hq/events-hq, since its own real
     * redraw() already did its own blit. Early return, no generic
     * present needed. */
    /* REAL Stage 5 §5d.10 (2026-08-16) - db-hq mode: own real content
     * draw (chrome/tabbar/sidebar/panel), same shared present
     * (XGetImage->XPutImage) below every mode already uses. */
    assign_nav_and_layout();
    /* REAL FIX 2026-09-05, direct live report ("fullscreen breaks
     * pdl-read layout - content stays squished bottom-left, most of
     * the window black"). Root cause (found by a parallel
     * investigation): the grow-buf + XResizeWindow blocks near the end
     * of redraw() ran AFTER this frame was already drawn into the
     * still-SMALL Pixmap - everything past the old bounds got clipped,
     * then the buffer/window grew, and that comment's promised "NEXT
     * redraw() repaints it" never came (TOGGLE_FULLSCREEN's handler
     * only did XMoveWindow + a single popup_handle_click redraw, no
     * hq_request_redraw()). Fix: do the essential grow+resize HERE,
     * right after assign_nav_and_layout() has set g_win_w/g_win_h to
     * the fullscreen size, BEFORE the first XFillRectangle - so this
     * very frame draws into a correctly-sized buffer and window. The
     * every-frame safety-net block near the end stays (it's a cheap
     * no-op read when nothing changed). Position-move logic is left
     * where it is - only the size half needs to be early. */
    if (g_win_w > g_buf_w || g_win_h > g_buf_h) {
        int nw = g_win_w > g_buf_w ? g_win_w : g_buf_w;
        int nh = g_win_h > g_buf_h ? g_win_h : g_buf_h;
        if (xftdraw_buf) { XftDrawDestroy(xftdraw_buf); xftdraw_buf = NULL; }
        if (buf) XFreePixmap(dpy, buf);
        buf = XCreatePixmap(dpy, win, (unsigned)nw, (unsigned)nh, (unsigned)DefaultDepth(dpy, screen));
        xftdraw_buf = XftDrawCreate(dpy, buf, DefaultVisual(dpy, screen), cmap);
        g_buf_w = nw; g_buf_h = nh;
        XSync(dpy, False);
    }
    {
        XWindowAttributes wa;
        if (XGetWindowAttributes(dpy, win, &wa) && (wa.width != g_win_w || wa.height != g_win_h)) {
            XResizeWindow(dpy, win, (unsigned)g_win_w, (unsigned)g_win_h);
            XSync(dpy, False);
        }
    }
    /* 2026-09-09, direct report ("bookstack doesn't take on the settings
     * colors from the verse popup"): the base window fill + chrome strip
     * were hardcoded #1c1c1c/#2a2a2a for every non-dock window, so an
     * entity window like book-stack (minimal CSS - nothing repaints over
     * this base) ignored #.desktop/livedesk_theme.pdl entirely.
     * g_theme_bg/fg are already loaded (load_theme_colors() in main() +
     * live on the theme-changed marker) and always hold a valid value
     * (static #1c1c1c/#cccccc defaults), so just use them. */
    XSetForeground(dpy, gc, alloc_pixel(g_theme_bg));
    XFillRectangle(dpy, buf, gc, 0, 0, (unsigned)g_win_w, (unsigned)g_win_h);
    if (!window_is_dock()) {
        /* Header bar: slightly lifted theme. File/body area: duller /
         * washed so the list is visibly not the same slab as chrome. */
        XSetForeground(dpy, gc, alloc_pixel(kh_shade_hex(g_theme_bg, 18)));
        XFillRectangle(dpy, buf, gc, 0, 0, (unsigned)g_win_w, CHROME_H);
        {
            const char *body = (g_drop_highlight && g_drop_highlight_color[0])
                ? g_drop_highlight_color
                : kh_shade_hex(g_theme_bg, 48);
            XSetForeground(dpy, gc, alloc_pixel(body));
            XFillRectangle(dpy, buf, gc, 0, CHROME_H, (unsigned)g_win_w,
                           (unsigned)(g_win_h > CHROME_H ? g_win_h - CHROME_H : 0));
        }
    }

    /* REAL Stage 5 §5d.3 step 6 (2026-08-16) - real, data-selected
     * chrome text. Swatch-picker mode's own real title/status text,
     * ported verbatim from taskbar-settings' own redraw(); menu mode's
     * own real page-name title, unchanged. */
    {
        /* REAL, NEW 2026-09-03 (direct live request, straight out of
         * today's own real X input-focus regression: "in toolbar of
         * windows there is supposed to be a '^' signal if window is
         * active. maybe we could add '.' if its not, for sanity") - a
         * real, generic, always-on focus sanity indicator: queries the
         * real X server (XGetInputFocus), not this app's own belief
         * about itself, so a repeat of today's own "focus silently
         * dropped" class of bug is visible at a glance in every future
         * default-mode window, without needing xdotool/screenshots to
         * diagnose. "^" reuses the SAME glyph the armed-scope/dropdown
         * indicator already uses elsewhere in this file (a real,
         * established "this is the active thing" convention in this
         * house), "." is the real, deliberately unarmed/inert
         * counterpart - not a new visual language, the same one. */
        Window focus_win; int focus_revert;
        XGetInputFocus(dpy, &focus_win, &focus_revert);
        g_focus_owned_painted = (focus_win == win) ? 1 : 0; /* what the "^"/"." below reflects - the FocusIn/FocusOut redraw guard reads this */
        /* REAL, NEW 2026-09-15, direct live report ("the joy keys
         * should be consumed and interpreted from same file as arrow
         * keys, just like in tpmos. is that std?") - yes: this window
         * already polls its OWN per-pid relay file
         * (#.desktop/entity_menu_history/<pid>.txt) every tick via
         * poll_agent_history() - the exact same file real arrow-key
         * relay/agent testing already uses, proven reliable. Rather
         * than have khtpm_joystick_daemon.+x guess which window is
         * focused (no _NET_WM_PID is set anywhere in this codebase,
         * and the existing livedesk_hq_windows_<pid>.txt registry is
         * scoped to sidebar+panel app windows only, ~line 8487 below -
         * it would miss a flat-page window like file-explorer), this
         * window publishes the one fact it ALREADY knows reliably
         * right here (focus_win == win, the same check the "^"/"."
         * title indicator uses) as a tiny, universal, one-line marker
         * ANY window type produces - the daemon reads it once per
         * event to pick which pid's relay file to append into. Only
         * written while actually focused (never clears to a stale 0)
         * - the daemon simply targets whichever pid wrote here most
         * recently, same "eventually consistent, cheap" convention the
         * frame-file writes already use. */
        if (focus_win == win) {
            char fp_path[PATH_BUF], fp_tmp[PATH_BUF];
            snprintf(fp_path, sizeof(fp_path), "%s/#.desktop/joystick_focused_pid.txt", g_house_root);
            snprintf(fp_tmp, sizeof(fp_tmp), "%s.tmp.%d", fp_path, (int)getpid());
            FILE *fpf = fopen(fp_tmp, "w");
            if (fpf) { fprintf(fpf, "%d\n", (int)getpid()); fclose(fpf); rename(fp_tmp, fp_path); }
        }
        char title_buf[192];
        /* fallback chain: explicit <window label> -> entity identity
         * strip (entity menus) -> bare page name. */
        kh_compose_entity_ident();
        const char *title_raw = g_window->label[0] ? g_window->label
                              : (g_entity_ident[0] ? g_entity_ident : g_current_page);
        snprintf(title_buf, sizeof(title_buf), "%s %s%s%s",
                 (focus_win == win) ? "^" : ".", title_raw,
                 g_default_scope_confine ? "  Active [^]: (ESC to exit)" : "",
                 g_window_malformed ? "  \xE2\x9A\xA0 malformed template" : "");
        const char *title = title_buf;
        if (window_is_dock()) {
            const char *mark = (focus_win == win) ? "^" : ".";
            XftFont *df = font_ui;
            int ty = DOCK_BAR_H / 2 + (df ? df->ascent / 2 : 6);
            XftColor mark_col = xft_color(window_is_dock() ? g_theme_fg : "#eeeeee");
            XftDrawStringUtf8(xftdraw_buf, &mark_col, df, 18, ty,
                               (const FcChar8 *)mark, 1);
            XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &mark_col);
            XSetForeground(dpy, gc, alloc_pixel("#4a4a4a"));
            XDrawLine(dpy, buf, gc, DOCK_FOCUS_BOX_W, 0, DOCK_FOCUS_BOX_W, g_win_h);
        } else {
        XftColor title_col = xft_color(g_theme_fg);  /* themed (was hardcoded #eeeeee) - see the base-fill comment above */
        XftDrawStringUtf8(xftdraw_buf, &title_col, font_ui, 8,
                           window_is_entity_menu() ? (CHROME_H + 16) : 16,
                           (const FcChar8 *)title, (int)strlen(title));
        XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &title_col);
        /* REAL, NEW 2026-09-05 - "copied" tag top-right (left of the
         * chrome minimize/fullscreen/close buttons, which start around
         * g_win_w-190) for ~2s after any Ctrl+C/Ctrl+X. Event-driven:
         * the copy keypress already redraws once to show it;
         * hq_idle_tick() redraws once more to clear it when it ages. */
        if (g_clip_copied_at && (time(NULL) - g_clip_copied_at) <= 2) {
            XftColor cc = xft_color(g_theme_accent);
            const char *tag = "copied";
            XGlyphInfo ge;
            XftTextExtentsUtf8(dpy, font_ui, (const FcChar8 *)tag, (int)strlen(tag), &ge);
            XftDrawStringUtf8(xftdraw_buf, &cc, font_ui,
                               g_win_w - 200 - ge.width, 16,
                               (const FcChar8 *)tag, (int)strlen(tag));
            XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &cc);
        }
        }
        /* REAL, NEW 2026-09-03 (HQ-WINDOW-TASKBAR-ENTRIES-AND-MINIMIZE-
         * 2026-09-03.md §2.1) - real per-window taskbar registry, one
         * line, written by THIS window's own renderer (never another
         * process touching resources it doesn't own), PID-scoped same
         * real lesson as today's frame-file race fix - two windows can
         * never collide on this path. Scoped to real sidebar+panel app
         * windows only (g_default_has_sidebar_panel) - a transient
         * entity-menu popup or the swatch-picker is not a real "app"
         * worth its own taskbar entry, same real distinction db-hq/
         * events-hq's own separate window class already makes. Written
         * every redraw tick (cheap - this whole block already runs
         * every tick for the title bar right above), so the taskbar's
         * own poll (not yet built, §2.1) always sees fresh focus/
         * position state with zero new IPC beyond a plain text file. */
        if (g_default_has_sidebar_panel && !window_is_dock()) {
            char reg_path[PATH_BUF], reg_tmp[PATH_BUF];
            snprintf(reg_path, sizeof(reg_path), "%s/#.desktop/livedesk_hq_windows_%d.txt", g_house_root, (int)getpid());
            snprintf(reg_tmp, sizeof(reg_tmp), "%s.tmp", reg_path);
            FILE *rf = fopen(reg_tmp, "w");
            if (rf) {
                fprintf(rf, "win=0x%lx|pid=%d|title=%s|x=%d|y=%d|w=%d|h=%d|minimized=%d|focused=%d\n",
                        (unsigned long)win, (int)getpid(), title_raw, g_win_x, g_win_y, g_win_w, g_win_h,
                        g_hq_minimized ? 1 : 0, (focus_win == win) ? 1 : 0);
                fclose(rf);
                rename(reg_tmp, reg_path);
            }
        }
    }

    /* REAL Stage 5 (2026-08-16, khtpm-merge-how2.md §5d) - was a manual
     * per-item draw loop (background fill on focus, hand-picked colors);
     * now the shared, generic render_tree() (same real focus-ring
     * convention every other khtpm app already uses, khtpm_draw_core.c).
     * Real, deliberate visual change: focus indicator is now a ring, not
     * a full-row background fill - consistent with the house standard,
     * not a regression. */
    Elem *page = find_page(g_current_page);
    if (page) {
        char fpath[PATH_BUF], tmpp[PATH_BUF];
        /* REAL FIX 2026-09-03 (direct live report: chat-hai's "Speed"
         * row and its own first transcript message rendered as
         * character-level interleaved garbage - "moxieSpeed..." - even
         * though a live debug print proved this function's own layout
         * math was correct on every tick). Root cause: this path was
         * ONE flat, house-wide name, `#.desktop/entity_menu_frame.txt`
         * (or taskbar_settings_frame.txt for swatch-picker), shared by
         * EVERY default-mode window in the whole house - chat-hai and
         * open-hai (or any two default-mode apps open at once) were
         * concurrently writing to and reading from the exact same file,
         * every redraw tick, from separate processes with no locking.
         * The write side's own tmp+rename is only atomic PER WRITER -
         * two processes racing to write the SAME tmp path can still
         * interleave, and a reader can catch the file mid-rename. Real
         * fix: scope this path per-process, same real PID-keyed
         * convention `#.desktop/entity_menu_history/<pid>.txt` already
         * uses for other per-window state - getpid() is unique per
         * renderer, so two windows can never collide on this path
         * again, house-wide, permanently, not just for chat-hai/open-
         * hai's own current pairing. */
        snprintf(fpath, sizeof(fpath), "%s/#.desktop/entity_menu_frame_%d.txt", g_house_root,
                 (int)getpid());
        snprintf(tmpp, sizeof(tmpp), "%s.tmp", fpath);
        FILE *ff = fopen(tmpp, "w");
        if (ff) {
            g_ser_dd_n = 0;
            kh_serialize_frame_subtree(ff, page);
            /* REAL, NEW 2026-09-01 - the sidebar+panel chrome "X"/"!"
             * pair (see their own static-storage declaration comment)
             * live OUTSIDE `page`'s own tree (same real reason db-hq's
             * own g_dbhq_close_elem does), so dbhq_serialize_frame_
             * subtree()'s page-rooted recursion never reaches them on
             * its own - serialized explicitly here, real nav_index
             * already assigned by layout_sidebar_panel() above. A
             * harmless no-op (both real-elem's own w/h stay 0) for any
             * OTHER default-mode page, which never touches these two
             * statics at all. */
            if (g_default_minimize_elem->w > 0) kh_serialize_frame_elem(ff, g_default_minimize_elem);
            if (g_default_close_elem->w > 0) kh_serialize_frame_elem(ff, g_default_close_elem);
            if (g_default_fullscreen_elem->w > 0) kh_serialize_frame_elem(ff, g_default_fullscreen_elem);
            /* REAL, NEW 2026-09-03 - generic scrollbar up/down arrows
             * (generic_sbar_register()'s own header comment) live
             * outside the parsed tree too, same real reason/pattern as
             * the close/fullscreen pair right above - serialized
             * explicitly here, once per registered scrollbar slot. */
            for (int sbi = 0; sbi < g_n_generic_sbars; sbi++) {
                if (g_sbar_up_elem[sbi].w > 0) kh_serialize_frame_elem(ff, &g_sbar_up_elem[sbi]);
                if (g_sbar_down_elem[sbi].w > 0) kh_serialize_frame_elem(ff, &g_sbar_down_elem[sbi]);
            }
            /* MILESTONE B/C - the synthesized footer row pager, same
             * "lives outside the parsed tree" pattern as the chrome
             * trio above. */
            if (g_footer_more_elem.w > 0) kh_serialize_frame_elem(ff, &g_footer_more_elem);
            if (g_footer_less_elem.w > 0) kh_serialize_frame_elem(ff, &g_footer_less_elem);
            /* MILESTONE A polish - open dropdown-child last so it paints
             * OVER the <canvas> / <footer> (direct report). */
            kh_serialize_frame_deferred(ff);
            fclose(ff); rename(tmpp, fpath);
        }
        {
            FILE *rf = fopen(fpath, "r");
            if (rf) {
                char line[9000]; /* REAL, NEW 2026-09-05 - see kh_paint_frame_line()'s own buf2 comment: must be big enough for a real text_area line */
                while (fgets(line, sizeof(line), rf)) {
                    size_t len = strlen(line);
                    while (len > 0 && (line[len-1]=='\n' || line[len-1]=='\r')) line[--len] = '\0';
                    if (len) kh_paint_frame_line(line);
                }
                fclose(rf);
            }
        }
        draw_generic_scrollbars();
        if (window_is_dock()) dock_draw_separators(page);
        /* Drop-hover UX: fill already used drop color for the file
         * area; paint a dashed frame + name on TOP of the list so it
         * cannot hide under swatch/scrolllist cells. */
        if (g_drop_highlight && !window_is_dock()) {
            int i;
            const char *fc = g_drop_highlight_color[0] ? g_drop_highlight_color : "#88ff66";
            XSetForeground(dpy, gc, alloc_pixel(fc));
            XFillRectangle(dpy, buf, gc, 8, CHROME_H + 8, (unsigned)(g_win_w > 16 ? g_win_w - 16 : 0), 36);
            XSetLineAttributes(dpy, gc, 4, LineOnOffDash, CapButt, JoinMiter);
            for (i = 2; i <= 8; i += 3)
                XDrawRectangle(dpy, buf, gc, i, i,
                               (unsigned)(g_win_w - 1 - 2 * i), (unsigned)(g_win_h - 1 - 2 * i));
            XSetLineAttributes(dpy, gc, 1, LineSolid, CapButt, JoinMiter);
            if (font_ui && xftdraw_buf) {
                char banner[192];
                XftColor tcol = xft_color("#111111");
                snprintf(banner, sizeof(banner), "[ drop into inventory: %s ]",
                         g_drop_hover_name[0] ? g_drop_hover_name : "...");
                XftDrawStringUtf8(xftdraw_buf, &tcol, font_ui, 16, CHROME_H + 32,
                                  (const FcChar8 *)banner, (int)strlen(banner));
                XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &tcol);
            }
            /* Dotted cell where the file will land (next grid/list slot). */
            {
                int sx = 16, sy = CHROME_H + 88, sw = 176, sh = 40;
                int n = 0, maxx = 0, maxy = 0, tw = 176, th = 40, minx = 24, miny = 0, grid = 0, ni;
                for (ni = 0; ni < g_n_nav; ni++) {
                    Elem *e = g_nav[ni];
                    int ge, en;
                    if (!e || e->w <= 0) continue;
                    ge = !strncmp(e->id, "gentry", 6);
                    en = !strncmp(e->id, "entry", 5) && e->id[5] >= '0' && e->id[5] <= '9';
                    if (!ge && !en) continue;
                    if (ge) grid = 1;
                    n++;
                    tw = e->w; th = e->h;
                    if (e->x < minx) minx = e->x;
                    if (!miny || e->y < miny) miny = e->y;
                    if (e->y > maxy || (e->y == maxy && e->x >= maxx)) {
                        maxx = e->x; maxy = e->y;
                    }
                }
                if (n > 0) {
                    if (grid) {
                        sx = maxx + tw + 8; sy = maxy;
                        if (sx + tw > g_win_w - 20) { sx = minx; sy = maxy + th + 8; }
                        sw = tw; sh = th;
                    } else {
                        sx = minx; sy = maxy + th + 4; sw = tw; sh = th;
                    }
                } else if (!grid) {
                    sw = g_win_w > 48 ? g_win_w - 48 : 120;
                    sh = 28;
                }
                XSetForeground(dpy, gc, alloc_pixel("#111111"));
                XSetLineAttributes(dpy, gc, 2, LineOnOffDash, CapButt, JoinMiter);
                XDrawRectangle(dpy, buf, gc, sx, sy, (unsigned)sw, (unsigned)sh);
                XSetLineAttributes(dpy, gc, 1, LineOnOffDash, CapButt, JoinMiter);
                XDrawRectangle(dpy, buf, gc, sx + 3, sy + 3,
                               (unsigned)(sw > 6 ? sw - 6 : 1), (unsigned)(sh > 6 ? sh - 6 : 1));
                XSetLineAttributes(dpy, gc, 1, LineSolid, CapButt, JoinMiter);
                if (font_ui && xftdraw_buf && g_drop_hover_name[0]) {
                    XftColor tcol = xft_color("#111111");
                    XftDrawStringUtf8(xftdraw_buf, &tcol, font_ui, sx + 8, sy + sh / 2 + 4,
                                      (const FcChar8 *)g_drop_hover_name,
                                      (int)strlen(g_drop_hover_name));
                    XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &tcol);
                }
            }
        }
    }
    if (g_dock_peer && !g_dock_in_peer_paint) dock_paint_peer();
    if (window_is_dock() && !g_dock_in_peer_paint && !g_dock_in_menu_paint)
        dock_paint_menu();

    /* REAL FIX 2026-08-31 (found live, testing generic capability #1 -
     * the .chtpm live-reparse this default/popup mode now also gets,
     * see reparse_chtpm_if_changed()'s own header comment): this real
     * present path never needed a Pixmap/window resize check before -
     * g_win_w/g_win_h and buf were both set ONCE at real launch and
     * never changed afterward. Live reparse is the first real case
     * where content (and so g_win_w/g_win_h, computed inside
     * assign_nav_and_layout()'s own default-mode branch) can GROW
     * after buf already exists, and XGetImage past a Pixmap's real
     * allocated size throws a fatal, unhandled BadMatch (confirmed
     * live: a real crash reproduced by growing a picker's own item
     * count via a live-edited .chtpm). Same real fix already proven
     * for db-hq/events-hq/open-hai above - recreate buf/xftdraw_buf if
     * grown, real-resize the X11 window to match, checked every frame
     * (cheap - a no-op read when nothing changed). */
    if (g_win_w > g_buf_w || g_win_h > g_buf_h) {
        int new_w = g_win_w > g_buf_w ? g_win_w : g_buf_w;
        int new_h = g_win_h > g_buf_h ? g_win_h : g_buf_h;
        if (xftdraw_buf) { XftDrawDestroy(xftdraw_buf); xftdraw_buf = NULL; }
        if (buf) XFreePixmap(dpy, buf);
        buf = XCreatePixmap(dpy, win, (unsigned)new_w, (unsigned)new_h, (unsigned)DefaultDepth(dpy, screen));
        xftdraw_buf = XftDrawCreate(dpy, buf, DefaultVisual(dpy, screen), cmap);
        g_buf_w = new_w; g_buf_h = new_h;
        XSync(dpy, False);
        /* the just-resized Pixmap is undefined content - this frame's
         * real drawing above ran against the OLD buf, so it's lost;
         * the NEXT redraw() (already scheduled by every real caller of
         * this generic capability) repaints it for real - a single,
         * harmless blank frame, not a crash. */
    }
    {
        XWindowAttributes wa;
        if (XGetWindowAttributes(dpy, win, &wa) && (wa.width != g_win_w || wa.height != g_win_h)) {
            XResizeWindow(dpy, win, (unsigned)g_win_w, (unsigned)g_win_h);
            XSync(dpy, False);
        }
        /* REAL, NEW 2026-09-03 - same real off-screen-chrome fix as
         * layout_sidebar_panel()'s own g_win_x/g_win_y clamp (see its
         * header comment); that clamp only updates the in-memory
         * variables, this is what actually MOVES the real X11 window to
         * match, same real "check real XGetWindowAttributes, only act
         * on a genuine mismatch" pattern the resize check right above
         * already uses.
         * REAL FIX 2026-09-03 (direct live report: "arrow keys aren't
         * moving the '>' in the windows... when did this happen?") -
         * this corrective move can land moments AFTER main()'s own
         * post-map XSetInputFocus retry loop (2026-08-28, "popups are no
         * longer getting nav/index focus... i have to manually click")
         * already succeeded - moving a window that JUST received real
         * input focus is a known, real way for a WM/compositor to
         * silently drop it again, and nothing EVER moved these windows
         * after creation before this session's own new position-anchor
         * work, so this real regression could not have existed before
         * it. Real fix: check who really has focus before the move: if
         * it was genuinely this window, put it back immediately after -
         * same retry idea as that original 2026-08-28 fix, applied at
         * the one new real place a focused window can now move. */
        /* FLICKER FIX 2026-09-03 - the prior version compared raw wa.x/wa.y
         * (which a reparenting WM reports RELATIVE TO THE WM FRAME, not the
         * root) against g_win_x/g_win_y (root-relative), so the mismatch was
         * almost always spuriously true and this block fired an XMoveWindow +
         * up to 3 XSync roundtrips + an XSetInputFocus on EVERY redraw,
         * including idle repaints - a corrective-move feedback storm the
         * compositor rendered as intermittent flicker (cf. tpmos PITFALLS #17
         * "renderer prints only, no side effects" and #59 split-brain coords).
         * Now: (1) do nothing at all unless our INTENDED position actually
         * changed since we last applied it - the idle case is zero roundtrips;
         * (2) even then, translate the window origin to real root coords
         * before deciding the server is genuinely out of position. */
        if (g_win_x != g_win_pos_applied_x || g_win_y != g_win_pos_applied_y) {
            Window root_child; int root_x = 0, root_y = 0;
            if (XTranslateCoordinates(dpy, win, DefaultRootWindow(dpy), 0, 0,
                                      &root_x, &root_y, &root_child)
                && (root_x != g_win_x || root_y != g_win_y)) {
                Window had_focus; int had_revert;
                XGetInputFocus(dpy, &had_focus, &had_revert);
                XMoveWindow(dpy, win, g_win_x, g_win_y);
                XSync(dpy, False);
                if (had_focus == win) {
                    XSetInputFocus(dpy, win, RevertToParent, CurrentTime);
                    XSync(dpy, False);
                }
            }
            g_win_pos_applied_x = g_win_x;
            g_win_pos_applied_y = g_win_y;
        }
    }
    /* REAL, NEW 2026-09-04 (direct request) - a 2px frame in the theme
     * SECONDARY colour (livedesk_theme.pdl fg) around the WHOLE window,
     * taskbar included. Drawn LAST, after every content paint (sidebar/
     * panel bg fills would otherwise overpaint the side/bottom edges -
     * the live "chat-hai only frames the chrome" report). Also a visual
     * proof that any edge affordance stopping short of it is on-window. */
    {
        XSetForeground(dpy, gc, alloc_pixel(g_theme_fg[0] ? g_theme_fg : "#888888"));
        XDrawRectangle(dpy, buf, gc, 0, 0,
                       (unsigned)(g_win_w - 1), (unsigned)(g_win_h - 1));
    }
    /* user drag-resize affordance: ⌟ in the bottom-right corner when the
     * window opted in (class="user-resizable"). The KH_RESIZE_GRIP hot
     * corner in the ButtonPress handler is anchored to the same spot. */
    if (g_user_resizable && xftdraw_buf && font_ui) {
        XftColor gcol = xft_color(g_theme_fg[0] ? g_theme_fg : "#888888");
        XftDrawStringUtf8(xftdraw_buf, &gcol, font_ui,
                          g_win_w - 15, g_win_h - 5,
                          (const FcChar8 *)"\xE2\x8C\x9F", 3);   /* U+231F ⌟ */
        XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &gcol);
    }
    XSync(dpy, False);
    XImage *frame = XGetImage(dpy, buf, 0, 0, (unsigned)g_win_w, (unsigned)g_win_h, AllPlanes, ZPixmap);
    if (frame) {
        XPutImage(dpy, win, gc, frame, 0, 0, 0, 0, (unsigned)g_win_w, (unsigned)g_win_h);
        XDestroyImage(frame);
    } else {
        XCopyArea(dpy, buf, win, gc, 0, 0, (unsigned)g_win_w, (unsigned)g_win_h, 0, 0);
    }
    XFlush(dpy);
    dock_write_ascii_frame();   /* strip: the "cli" mirror (dock only, else no-op) */
    kh_write_ascii_frame();     /* every other window: per-PID text mirror */
}

/* on-demand debug PNG dump, same real convention every other khtpm app
 * uses (own separate capture, not the hot redraw path). */
/* REAL Stage 1 follow-up (2026-08-16, khtpm-merge-how2.md "HOUSE
 * STANDARD" section) - was a locally-duplicated XImage->RGB unpack
 * loop (same shape as db-hq/taskbar-settings' own real duplicates, see
 * that section's own header comment for the full real correction).
 * Now the same real, standalone, cross-app op binary those already use
 * (&.widgits/_shared-lib/ops/dump_frame_png_op.c), invoked via
 * system() - captures the real, already-blitted WINDOW directly (own
 * X connection), not this process's own `buf` back-buffer. */
/* REAL Stage 5 §5d.3 step 6 (2026-08-16) - mode-aware output path,
 * same real backward-compatibility reasoning as history_path() above.
 * Swatch-picker mode also writes the real receipt.txt taskbar-
 * settings' own testing convention already relied on (nav/phase/
 * bg_idx/fg_idx), ported verbatim. */
/* REAL FIX (2026-08-27, direct instruction: "we need 2 fix this once
 * and for all" - dump_frame_png_op.+x's own header comment ASSUMED "the
 * caller has already flushed by the time this fires off a relay-
 * triggered 'p' keypress" - false. A relay code is dispatched the
 * instant it's read (dispatch_relay_code() -> handle_key()/
 * evhq_handle_key() -> dump_frame_png(), all synchronous, all within
 * ONE poll_agent_history() call) - the main loop's own redraw() for
 * THIS SAME TICK has NOT run yet, so dump_frame_png_op.+x's XGetImage
 * on the live window captured whatever the PREVIOUS tick's redraw()
 * left on screen, one full action behind every single time. Root
 * cause confirmed live: after sending Enter then 112 (dump) with real
 * sleeps between them, the text-state dump (code 210, which reads the
 * live Elem tree directly, no window/pixmap involved) already showed
 * the correct post-Enter state, while the PNG consistently showed the
 * pre-Enter layout - not a one-off race, the SAME stale frame came
 * back byte-identical on a second dump 2s later, ruling out "hasn't
 * caught up yet." Fix: force the SAME real redraw() the main loop
 * would eventually call anyway, synchronously, right here, before
 * ever invoking the external dump op - by construction the window
 * always holds the current frame at capture time now, no sleep/poll
 * needed by any caller ever again for this family. */
static void redraw(void);
static void dump_frame_png(void) {
    char png[PATH_BUF];
    redraw(); /* REAL FIX above - guarantees `win`'s real on-screen pixels reflect the state as of THIS tick's input, not the previous tick's */
    snprintf(png, sizeof(png), "/tmp/entity-menu-frame.png");
    char cmd[PATH_BUF * 2];
    snprintf(cmd, sizeof(cmd), "'%s/&.widgits/_shared-lib/ops/+x/dump_frame_png_op.+x' 0x%lx '%s'",
             g_house_root, (unsigned long)win, png);
    int ok = (system(cmd) == 0);
    {
        /* Generic window: alongside the PNG write (a) a .receipt.txt and
         * (b) a .frame.txt ASCII serialization of the laid-out Elem
         * tree - the tpmos "if it's not in current_frame.txt it's not
         * in the pixels" check, reusing db-hq's own serializer. */
        char receipt[PATH_BUF], framef[PATH_BUF];
        snprintf(receipt, sizeof(receipt), "%s.receipt.txt", png);
        snprintf(framef, sizeof(framef), "%s.frame.txt", png);
        FILE *rf = fopen(receipt, "w");
        if (rf) {
            fprintf(rf, "ok=%d png=%s w=%d h=%d t=%ld nav=%d n_nav=%d page=%s vars=%s\n",
                    ok, png, g_win_w, g_win_h, (long)time(NULL),
                    g_focus_nav, g_n_nav,
                    g_current_page[0] ? g_current_page : "-",
                    g_vars_path[0] ? g_vars_path : "-");
            fclose(rf);
        }
        FILE *ff = fopen(framef, "w");
        if (ff) {
            if (g_window) {
                kh_serialize_frame_elem(ff, g_window);
                kh_serialize_frame_subtree(ff, g_window);
            }
            fclose(ff);
        }
    }
}

static void handle_key(KeySym ks, char ch) {
    /* PDL-configurable window close (#.desktop/hq_ui.pdl close_combo,
     * default ctrl+c). The deliberate close gesture for a focused
     * window - ESC deliberately does NOT close a real app window
     * (accident risk). Compares the base keysym (Ctrl+C delivers ch
     * 0x03, so ch is useless here) against the letter after the last
     * '+', case-insensitively, with the ctrl/shift requirement. */
    if (g_close_combo[0] && ((ks >= 'a' && ks <= 'z') || (ks >= 'A' && ks <= 'Z'))) {
        const char *plus = strrchr(g_close_combo, '+');
        int want_ch = plus ? (unsigned char)plus[1] : (unsigned char)g_close_combo[0];
        int want_ctrl  = (strstr(g_close_combo, "ctrl")  != NULL);
        int want_shift = (strstr(g_close_combo, "shift") != NULL);
        int got = (int)ks; if (got >= 'A' && got <= 'Z') got += 32;
        if (want_ch >= 'A' && want_ch <= 'Z') want_ch += 32;
        if (want_ch && got == want_ch &&
            (!want_ctrl  || g_key_ctrl) &&
            (!want_shift || g_key_shift)) {
            g_quit = 1;
            return;
        }
    }
    /* REAL FIX 2026-09-04 (pc-hq-bugs.md Bug 2 - "tb top gets focus
     * (steals it) and wont ever give it back" the instant the user
     * presses an arrow key after clicking a different window). Root
     * cause, confirmed by direct read: dock_grab_keyboard()'s matching
     * release (dock_release_keyboard_if_left()) only fires on a real
     * FocusOut event for the dock's OWN window - if that release is
     * ever missed (this desktop's real Mutter/XWayland focus-
     * notification quirks are already documented multiple times
     * elsewhere in this file), the dock keeps an ACTIVE, DISPLAY-WIDE
     * XGrabKeyboard indefinitely. A key display-routed to the dock
     * ONLY because of that stale grab (never because the user is
     * actually navigating the taskbar) still reaches THIS function
     * with `window_is_dock()` true, and the dock's own arrow-key nav
     * handler below (`XRaiseWindow`+`XSetInputFocus`+`dock_grab_
     * keyboard()`) then RE-ASSERTS AND RE-GRABS focus for itself as an
     * ordinary side effect of what it thinks is legitimate taskbar
     * navigation - reinforcing the exact steal, forever, on every
     * subsequent keypress. Real fix: before acting on ANY key as the
     * dock's own input, verify via a live XGetInputFocus (not the
     * cached/one-tick-stale g_focus_owned_painted) that the dock
     * genuinely holds real focus right now. If it doesn't, this key
     * was never meant for it - release the stale grab immediately and
     * do not process it as dock nav, so whichever window the user
     * actually clicked keeps its real focus and gets the NEXT key. */
    if (window_is_dock() && g_dock_kbd_win && dpy) {
        Window fw = None; int rev = 0;
        XGetInputFocus(dpy, &fw, &rev);
        if (!dock_is_our_win(fw)) {
            kh_ungrab_kbd();
            g_dock_kbd_win = None;
            return;
        }
    }
    /* REAL, NEW 2026-09-04 - see g_interact_relay_on's own declaration
     * comment. Checked BEFORE every other key-order exception in this
     * function (events-hq's picker, db-hq's armed field, 'p' dump) -
     * matches run_pchq_board_mode()'s own real behavior: "keyboard is
     * 100% game input now", forwarded unconditionally, INCLUDING Escape
     * (the external process's own native ESC-exit consumes it there,
     * never intercepted locally) and 'p' (never a local dump shortcut
     * while engaged). kh_key_history_code() is the SAME decimal-code
     * resolver history capture already uses - reused, not reinvented. */
    /* Escape bypasses the g_x11_window_focused half of the gate
     * (pchq-vs-tpmos.md D5): a real KeyPress here proves this window has
     * X focus, and Escape is the unambiguous "exit Interact" - forward
     * it (format-correct, D2) so the board_viewer.chtpm parser's own
     * process_key(27) ESC-exit runs even if a spurious Mutter FocusOut
     * left the flag at 0. */
    if (g_interact_relay_on && ks == XK_Escape) {
        /* 27 goes ONLY to keyboard/history.txt - see the double-arrow
         * comment in the general branch below. */
        g_x11_window_focused = 1;
        for (int i = 0; i < g_interact_relay_n; i++) {
            const char *p = g_interact_relay_paths[i];
            if (p[0] && strstr(p, "keyboard/history.txt")) {
                FILE *f = fopen(p, "a");
                if (f) { fprintf(f, "KEY_PRESSED: 27\n"); fclose(f); }
            }
        }
        return;
    }
    if (g_interact_relay_on && g_x11_window_focused) {
        int code = kh_key_history_code(ks, ch);
        /* REAL FIX 2026-09-04 (see PLAN-pchq-interact-camera-pov.md
         * Part A for the full citation trail) - tpmos/board-viewer's
         * own ARROW_LEFT/RIGHT/UP/DOWN convention (chtpm_parser.c's
         * enum, bv_menu_input.c's own #defines) is 1000/1001/1002/1003,
         * NOT khtpm's house-wide 200-203 nav-capture convention that
         * kh_key_history_code() correctly still returns for every
         * OTHER caller. Interact Mode is the one relay path that must
         * speak the game engine's own dialect - remap ONLY here. */
        if      (code == 200) code = 1002; /* Up    -> ARROW_UP    */
        else if (code == 201) code = 1003; /* Down  -> ARROW_DOWN  */
        else if (code == 202) code = 1000; /* Left  -> ARROW_LEFT  */
        else if (code == 203) code = 1001; /* Right -> ARROW_RIGHT */
        /* REAL FIX 2026-09-09 (pchq-vs-muta.md - the "double arrow"
         * bug). The two relay targets have DIFFERENT consumers:
         *   - keyboard/history.txt -> the board_viewer.chtpm PARSER's
         *     process_key() (KEY_PRESSED: <n> format only). While the
         *     parser is engaged in INTERACT it ALSO re-injects every key
         *     it reads there into interact_relay.txt (inject_raw_key) -
         *     so a key written to BOTH files reaches the pal-VM camera
         *     TWICE = one keypress moves the xelector two cells.
         *   - interact_relay.txt -> the pal-VM camera loop (bare <n>).
         * Fix: 13/27 (engage-toggle / ESC-exit - the parser's state
         * machine) go ONLY to keyboard/history.txt; every other key
         * (arrows 1000-1003, wasd, 0, 1-4, q/e/r/t/c/v, digits) goes
         * ONLY to interact_relay.txt. The parser stays engaged (its
         * active_index never changes on a camera key) and never
         * double-injects. */
        int to_parser = (code == 13 || code == 27);
        for (int i = 0; i < g_interact_relay_n; i++) {
            const char *p = g_interact_relay_paths[i];
            if (!p[0]) continue;
            int is_kbd = (strstr(p, "keyboard/history.txt") != NULL);
            if (to_parser != is_kbd) continue;      /* route by consumer */
            FILE *f = fopen(p, "a");
            if (!f) continue;
            if (is_kbd) {
                fprintf(f, "KEY_PRESSED: %d\n", code);
            } else {
                /* "<code> <monotonic_ms>" - bv_dispatch drops a queued
                 * key older than its stale threshold, so a held key
                 * that piles up behind a slow render stops promptly on
                 * release instead of coasting. A bare "<code>" (this
                 * line without the timestamp) still parses - reverse
                 * compatible. */
                struct timespec rts; clock_gettime(CLOCK_MONOTONIC, &rts);
                long long rms = (long long)rts.tv_sec * 1000 + rts.tv_nsec / 1000000;
                fprintf(f, "%d %lld\n", code, rms);
            }
            fclose(f);
        }
        /* REAL, NEW 2026-09-04, direct request ("add p frame dump to
         * game then") - 'p' is not on the documented camera/POV key
         * table (PLAN-pchq-interact-camera-pov.md), so it's safe to
         * ALSO fire the normal local PNG dump here, in addition to
         * forwarding it same as every other key above - lets whoever
         * is debugging a live Interact Mode session get a real frame
         * (proving reparse/badge/canvas state at that instant)
         * without ever needing to disarm first. Forward always
         * happens above regardless, in case some future game module
         * genuinely does bind 'p'. */
        if (ch == 'p') dump_frame_png();
        return;
    }
    /* REAL, events-hq mode only - routed BEFORE the shared 'p' dump
     * check, matching its own real key-order exactly: when its picker
     * overlay is open, 'p' must be swallowed as a literal typed
     * character in the active field, not intercepted as a dump
     * shortcut (its own original handle_key() checked g_picker_open
     * first, 'p' only afterward). */
    /* REAL 2026-08-25 (Stage 3 bookmarks port) - db-hq mode now has its
     * own armed input field (g_input_elem, bookmarks' New+ path entry)
     * and needs the SAME key-order exception as events-hq/chat-hai
     * above: 'p' must type into an armed field, not trigger a dump. */
    /* REAL, NEW 2026-09-05 (GRID-ELEMENT-DESIGN.md) - <grid> shares the
     * same g_default_input_elem armed-field slot (same 'p'-swallowed-
     * as-a-literal-character exception, same single active-field-at-a-
     * time invariant every other armed element already relies on), but
     * its own key semantics (2D cursor / jump buffer / nested cell-edit
     * sub-state) are different enough from cli_io/text_area's single
     * text-cursor model to need a dedicated handler rather than another
     * is_area-style branch inside default_cli_io_handle_key(). */
    if (g_default_input_elem && strcmp(g_default_input_elem->tag, "grid") == 0) { default_grid_handle_key(ks, ch); return; }
    if (g_default_input_elem) { default_cli_io_handle_key(ks, ch); return; } /* same real key-order exception - a real cli_io field needs 'p' as a literal typed character */
    if (ch == 'p') { dump_frame_png(); return; }
    /* Anything past this point that isn't a bare digit ends a
     * pending multi-digit nav jump (tpmos digit_accum "reset on
     * non-digit keys"). */
    if (!(ch >= '0' && ch <= '9')) g_nav_digit_accum = 0;
    /* Space: on the dock/strip it is Enter (a focused pal cell opens its context
     * menu, a header cell opens its dropdown, same path as Enter below); in any
     * other HQ window it opens the same context menu a mouse button-3 click on the
     * focused nav item opens. Armed fields returned above (Space stays a literal
     * there); an entity-menu popup ignores Space. */
    if (ks == XK_space || ch == ' ') {
        if (window_is_dock()) {
            ks = XK_Return;
            ch = 0;
        } else if (g_focus_nav >= 1 && g_focus_nav <= g_n_nav && !window_is_entity_menu()) {
            Elem *f = g_nav[g_focus_nav - 1];
            if (f && f->w > 0 && strncmp(f->id, "chrome-", 7) != 0) {
                kh_open_cli_io_context_menu(f, f->x + f->w / 2, f->y + f->h / 2);
                return;
            }
        }
    }
    if (ks == XK_Return || ks == XK_KP_Enter) {
        activate_focused();
        /* activate_focused() may have just entered/left a scope (<tab>,
         * ACTIVATE) - relayout+repaint NOW so [^] and the confined nav
         * show immediately, instead of only on the next projector tick. */
        if (!g_quit) { assign_nav_and_layout(); redraw(); }
        return;
    }
    /* REAL, NEW 2026-09-03 (direct instruction: "esc closes drop down
     * and deactivates", matching the taskbar's own separate ktb_hq_
     * close() on Escape) - checked BEFORE the plain g_quit=1 fallback
     * right below, same real key-order class as every other "something
     * is armed/open, Escape closes THAT first" exception in this
     * function (default_cli_io_handle_key()'s own Escape branch is the
     * proven precedent - this is the same real idea for the generic
     * dropdown instead of a cli_io field). */
    if (ks == XK_Escape && (g_default_active_scope_root || g_default_active_scope_id[0])) {
        /* Pop ONE level: nearest ACTIVATE ancestor, else full clear.
         * Matches chtpm_parser.c ESC (lines 2745-2754). */
        Elem *old = g_default_active_scope_root;
        Elem *p = old ? old->parent : NULL;
        Elem *next = NULL;
        while (p) {
            if (strncmp(p->onclick, "ACTIVATE", 8) == 0) { next = p; break; }
            p = p->parent;
        }
        if (next && next != old) {
            g_default_active_scope_root = next;
            if (next->id[0])
                snprintf(g_default_active_scope_id, sizeof(g_default_active_scope_id), "%s", next->id);
            g_default_scope_confine = 1;
            if (old && old->nav_index > 0) g_focus_nav = old->nav_index;
            else kh_focus_first_in_scope();
        } else {
            Elem *trig = g_default_active_scope_id[0] ? find_by_id(g_window, g_default_active_scope_id) : NULL;
            if (trig && trig->nav_index > 0)
                g_focus_nav = trig->nav_index;
            else if (g_default_active_scope_root && g_default_active_scope_root->nav_index > 0)
                g_focus_nav = g_default_active_scope_root->nav_index;
            g_default_active_scope_root = NULL;
            g_default_active_scope_id[0] = '\0';
            g_default_scope_confine = 0;
        }
        if (window_is_dock()) {
            char hist[PATH_BUF];
            snprintf(hist, sizeof(hist), "%s/#.desktop/strip_history.txt", g_house_root);
            FILE *hf = fopen(hist, "a");
            if (hf) { fprintf(hf, "27\n"); fclose(hf); }
        }
        return;
    }
    if (ks == XK_Escape && window_is_dock()) {
        char hist[PATH_BUF];
        snprintf(hist, sizeof(hist), "%s/#.desktop/strip_history.txt", g_house_root);
        FILE *hf = fopen(hist, "a");
        if (hf) { fprintf(hf, "27\n"); fclose(hf); }
        return;
    }
    if (ks == XK_Escape) {
        /* Direct instruction 2026-09-10: "we dont wanna close any
         * window on esc cuz it could be accident. ctrl+c is ok, but
         * nothing else but the x button". A real app window
         * (persistent / sidebar+panel / canvas - db-hq, events-hq, the
         * pc-hq board) is NEVER closed by a bare Escape; it has an [X]
         * chrome button. Bare ESC here is a no-op (earlier handlers
         * already gave ESC its useful jobs: exit a scope, forward to an
         * armed interact relay). Transient popups - the entity context
         * menu, pickers - keep their own ESC-to-dismiss below. */
        if (g_default_persistent || g_default_has_sidebar_panel || g_has_canvas)
            return;
        g_quit = 1; return;
    }
    /* REAL, NEW 2026-09-01 - a real, generic second action any focused
     * <item> can carry (see Elem's own backspace_action field comment) -
     * checked BEFORE the plain Up/Down/digit nav below, same real key-
     * order class as every other "armed field eats this key first"
     * exception in this function, even though nothing here is armed -
     * a focused item with backspace_action set simply always wins over
     * default_cli_io_handle_key()'s own absence at this point (already
     * routed away above if a field WAS actually armed). */
    if (ks == XK_BackSpace && g_focus_nav >= 1 && g_focus_nav <= g_n_nav) {
        Elem *focused = g_nav[g_focus_nav - 1];
        if (focused->backspace_action[0]) { dispatch_no_quit(focused->backspace_action); return; }
    }
    if (ks == XK_Up || ks == XK_Left) {
        if (g_dock_drop_lo && g_default_active_scope_id[0]) {
            if (g_focus_nav > g_dock_drop_lo) g_focus_nav--;
            return;
        }
        kh_nav_step(-1);
        if (window_is_dock()) dock_relay_focus_code(6000 + g_focus_nav);  /* snap the manager's strip_focus_cell to the new highlight (absolute, no drift) */
        if (window_is_dock() && g_dock_peer_win && g_focus_nav >= 1 && g_focus_nav <= g_n_nav) {
            Window want = (g_focus_nav > g_dock_header_nav_hi) ? g_dock_peer_win : win;
            XRaiseWindow(dpy, want);
            XSetInputFocus(dpy, want, RevertToParent, CurrentTime);
            dock_grab_keyboard(want);
        }
        return;
    }
    if (ks == XK_Down || ks == XK_Right) {
        if (g_dock_drop_lo && g_default_active_scope_id[0]) {
            if (g_focus_nav < g_dock_drop_hi) g_focus_nav++;
            return;
        }
        kh_nav_step(1);
        if (window_is_dock()) dock_relay_focus_code(6000 + g_focus_nav);
        if (window_is_dock() && g_dock_peer_win && g_focus_nav >= 1 && g_focus_nav <= g_n_nav) {
            Window want = (g_focus_nav > g_dock_header_nav_hi) ? g_dock_peer_win : win;
            XRaiseWindow(dpy, want);
            XSetInputFocus(dpy, want, RevertToParent, CurrentTime);
            dock_grab_keyboard(want);
        }
        return;
    }
    /* REAL, NEW 2026-08-31 - generic sidebar+panel scroll (see that
     * section's own header comment). Page_Up/Down scroll whichever
     * scrollable region g_focus_nav currently sits inside - a no-op
     * for a page with no <sidebar>/<panel> (both nav_lo/nav_hi stay
     * [0,0], never matching a real g_focus_nav >= 1). */
    if (ks == XK_Page_Up || ks == XK_Page_Down) {
        int dir = (ks == XK_Page_Down) ? 1 : -1;
        if (g_focus_nav >= g_default_sidebar_nav_lo && g_focus_nav <= g_default_sidebar_nav_hi)
            g_default_sidebar_scroll += dir;
        else if (g_focus_nav >= g_default_scrolllist_nav_lo && g_focus_nav <= g_default_scrolllist_nav_hi)
            g_default_scrolllist_scroll += dir;
        return;
    }
    if (ch >= '0' && ch <= '9') {
        int d = ch - '0';
        if (g_dock_drop_lo && g_default_active_scope_id[0]) {
            /* dock drop-zone: single-digit only, unchanged */
            g_nav_digit_accum = 0;
            int idx = g_dock_drop_lo + d - 1;
            if (idx >= g_dock_drop_lo && idx <= g_dock_drop_hi) g_focus_nav = idx;
            return;
        }
        /* multi-digit accumulate: "15" jumps to 15, not 5 (tpmos
         * chtpm_parser.c.bak digit_accum). Take accum*10+d when it's a
         * real nav index; else restart the accumulator with just d. */
        int nv = g_nav_digit_accum * 10 + d;
        if (nv >= 1 && nv <= g_n_nav && kh_elem_in_scope(g_nav[nv - 1])) {
            g_focus_nav = nv;
            g_nav_digit_accum = nv;
        } else if (d >= 1 && d <= g_n_nav && kh_elem_in_scope(g_nav[d - 1])) {
            g_focus_nav = d;
            g_nav_digit_accum = d;
        } else {
            g_nav_digit_accum = 0;
        }
        return;
    }
}

/* ---------- history (renamed 2026-08-25 from "relay" - this was already a
 * real, append-only, cursor-based reader, never truncating; the name was
 * the only thing left over from before this house settled on TPMOS
 * history.txt parity language. Same file family every other khtpm app
 * uses - #.desktop/entity_menu_history.txt. A line starting with '#' is
 * a human/agent audit comment: atoi() on it yields 0 so it is consumed
 * (cursor advances past it) but never dispatched - use this to leave a
 * "why" note inline in the file without a separate build. ---------- */
static long g_history_cursor = -1;
/* REAL FIX 2026-08-29 (live incident: my own test relay input to
 * db_hq_history.txt was ALSO delivered to the user's real, separately-
 * open db-hq window, corrupting its live nav state - "why isn't
 * arrow/index nav working in db-hq anymore?"). Root cause: this path
 * was keyed by MODE NAME ONLY, so every window of the same mode - real
 * user window, a test window, a second agent's window - read the exact
 * same file. Real fix, mirrors nav_tab's own existing per-pid
 * convention of the (since removed, 2026-09-20) nav_tab registry EXACTLY:
 * one real file per PROCESS, not per mode. Every consumer (a real
 * human's own X11 input via kh_capture_key()/kh_capture_click(),
 * or an external agent's relay write) now only ever reaches the ONE
 * window it actually targets - no possible cross-window bleed
 * regardless of how many windows of the same mode are open at once.
 * Discovery for an external writer that needs to find "the db-hq
 * window showing X": the old nav_master_current.txt / nav_tab/<pid>
 * registries (nav_ledger_publish()/nav_tab_register()) were dead code and
 * were removed 2026-09-20 (DOCK-UNFACTOR-AUDIT.md §2). Today an agent
 * resolves the PID from `ps`/the window's module_parent.pid or
 * livedesk_hq_windows_<pid>.txt - see 08-roadmap/design-docs/
 * RELAY-WINDOW-TARGETING-DESIGN.md §1/§3b. */
static void history_dir(char *out, size_t outsz) {
    snprintf(out, outsz, "%s/#.desktop/entity_menu_history", g_house_root);
}
static void history_path(char *out, size_t outsz) {
    char dir[PATH_BUF];
    history_dir(dir, sizeof(dir));
    mkdir(dir, 0777);
    snprintf(out, outsz, "%s/%d.txt", dir, (int)getpid());
}
/* Create this process's relay file empty at startup and seed the cursor
 * to 0, so the FIRST keystroke a generic terminal keyboard
 * (khtpm_kbd_ascii.+x) sends into a brand-new relay isn't mistaken for
 * pre-existing backlog and swallowed by poll_agent_history()'s
 * first-sight seed (g_history_cursor < 0 -> = st.st_size). Truncates a
 * stale file left by a crashed same-PID predecessor, which is correct -
 * nothing should replay across process lifetimes. */
static void history_init_empty(void) {
    char path[PATH_BUF];
    history_path(path, sizeof(path));
    FILE *f = fopen(path, "w");
    if (f) fclose(f);
    g_history_cursor = 0;
}

/* Phase 3a: capture-only. House format from pieces/keyboard/history.txt:
 *   MOUSE_EVENT: <button> <x> <y> <is_press>
 * Zero interpretation. Consume is poll_agent_history(). */
/* Phase 3b: capture-only. House format KEY_PRESSED: <decimal>.
 * Printable ASCII as-is; Tab=9; Return/Esc/BS same as existing relay;
 * arrows/page 200-205 (already in dispatch_relay_code). Other keys
 * write the raw X11 KeySym so consume can handle_key(ks,0). */
static void dispatch_relay_code(int code) {
    /* REAL, NEW 2026-09-05 - a relay-driven key is Shift-held only if
     * it's one of the explicit shifted-selection codes (220-225 below).
     * Reset first so a stale g_key_shift left by a prior PHYSICAL
     * Shift+key (the KeyPress site sets it from ev->xkey.state) can't
     * leak into an unshifted relay arrow and silently extend a
     * selection instead of collapsing it. */
    if (code < 220 || code > 225) g_key_shift = 0;
    if (code == 13) handle_key(XK_Return, 0);
    else if (code == 27) handle_key(XK_Escape, 0);
    else if (code == 8) handle_key(XK_BackSpace, 0); /* real, db-hq's own extra code - harmless no-op for other modes */
    else if (code == 9) handle_key(XK_Tab, 0); /* Phase 3b: Tab is a real key, not a printable */
    /* REAL, NEW 2026-08-25 (debug-only) - relay codes 200-203 for arrow
     * keysyms, which have no ASCII code and so were unreachable through
     * this text-file relay before now. Needed to reproduce a live report
     * ("up/down arrows don't move nav in bookmarks") headlessly instead
     * of guessing - outside the 0-126 real-keypress range so it can
     * never collide with an actual typed character. */
    else if (code == 200) handle_key(XK_Up, 0);
    else if (code == 201) handle_key(XK_Down, 0);
    else if (code == 202) handle_key(XK_Left, 0);
    else if (code == 203) handle_key(XK_Right, 0);
    else if (code == 204) handle_key(XK_Page_Up, 0);
    else if (code == 205) handle_key(XK_Page_Down, 0);
    /* REAL, NEW 2026-09-05 (CLIPBOARD-COPY-PASTE-DESIGN.md) - Ctrl+C/
     * Ctrl+X/Ctrl+V as their real ASCII control codes (3/24/22), so
     * the relay can drive clipboard copy/paste headlessly for testing,
     * exactly the same bytes a physical Ctrl+letter produces through
     * XLookupString. Handled as `ch`, not a keysym - default_cli_io_
     * handle_key()'s own `ch == 3/22/24` checks pick them up. */
    else if (code == 3 || code == 22 || code == 24) handle_key(0, (char)code);
    /* REAL, NEW 2026-09-05 (TEXT_AREA-SCROLL-GUTTER-SELECTION-DESIGN.md)
     * - Shift+arrow / Shift+Home / Shift+End for text selection, which
     * the relay had no way to express (Shift is a modifier, not a
     * key). 220-225, same reserved-band idea as the 200-205 plain
     * arrows. g_key_shift is set for exactly the one dispatched key. */
    else if (code >= 220 && code <= 225) {
        KeySym sk = (code == 220) ? XK_Left : (code == 221) ? XK_Right :
                    (code == 222) ? XK_Up   : (code == 223) ? XK_Down  :
                    (code == 224) ? XK_Home : XK_End;
        g_key_shift = 1;
        handle_key(sk, 0);
        g_key_shift = 0;
    }
    /* Task 6/7 (2026-08-26) - db-hq-only cheap text state dump for
     * agent testing, see dbhq_dump_debug_state()'s own header comment.
     * Code 210 (not a real keypress; 206-209 left free for any future
     * debug-only codes in this same reserved band). */
    /* REAL, NEW 2026-08-28 (Phase C testing) - dbhq_dump_debug_state()'s
     * own g_n_nav/g_nav[] loop (the part that actually matters for
     * verifying the generic scroll wiring) already reads only the
     * SHARED globals every mode populates, not db-hq-specific state - the
     * db-hq-only and events-hq-only fields it also prints are simply
     * irrelevant (harmless stale/zero) noise for chat-hai. Extended here
     * instead of writing a second, chai-only dump, since chat-hai had NO
     * text-state dump at all before this (only chai_dump_frame_png(),
     * PNG-only). */
    else if (code >= 32 && code <= 126) handle_key(0, (char)code);
    else if (code > 255 && code != 200 && code != 201 && code != 202 &&
             code != 203 && code != 204 && code != 205 && code != 210)
        handle_key((KeySym)code, 0);
}
/* REAL, NEW 2026-09-11 - TEMPORARY diagnostic-only logging (direct
 * instruction: network-browser's cli_io focus/backspace bug has been
 * "fixed" 3 times now, every fix passing this house's own relay-driven
 * test methodology cleanly, and the user reports real physical
 * hardware is unchanged every time - the relay test itself may be
 * masking the real bug, same class of problem as the already-
 * documented override_redirect/xdotool masking (09-appendix/
 * pc-hq-bugs.md Bug 2). Rather than ask for a synchronous side-by-side
 * test session, this appends one real, timestamped line per grab
 * attempt/outcome to <package_dir>/kh_focus_debug.log so the user can
 * just use the browser normally and the log gets read back afterward -
 * real evidence from real hardware, no live monitoring needed. Safe to
 * remove once this bug's real root cause is found - see BUG-LOG.md's
 * "network-browser address bar" open entry. */
static void kh_focus_debug_log(const char *fmt, ...) {
    if (!g_package_dir[0]) return;
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/kh_focus_debug.log", g_package_dir);
    FILE *f = fopen(path, "a");
    if (!f) return;
    struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tmv; localtime_r(&ts.tv_sec, &tmv);
    fprintf(f, "%02d:%02d:%02d.%03ld ", tmv.tm_hour, tmv.tm_min, tmv.tm_sec, ts.tv_nsec / 1000000);
    va_list ap; va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fprintf(f, "\n");
    fclose(f);
}

/* generic history-relay + keyboard-grab helpers (were dbhq_*; the
 * popup/entity-menu + cli_io paths still need them after the dbhq_*
 * deletion). */
static void kh_grab_keyboard_retry(void) {
    if (!dpy) return;   /* --headless: no display, nothing to grab */
    int a, rc = -1;
    for (a = 0; a < 5; a++) {
        rc = XGrabKeyboard(dpy, win, True, GrabModeAsync, GrabModeAsync, CurrentTime);
        if (rc == GrabSuccess) break;
        XSync(dpy, False); usleep(5000);
    }
    Window fw = None; int rev = 0;
    XGetInputFocus(dpy, &fw, &rev);
    kh_focus_debug_log("GRAB key=%s attempts=%d rc=%d(0=success) real_focus_is_us=%d",
                        g_default_input_elem ? (g_default_input_elem->target_id[0] ? g_default_input_elem->target_id : g_default_input_elem->id) : "?",
                        a + 1, rc, fw == win);
    g_kbd_grab_retry_deadline_ms = (rc == GrabSuccess) ? 0 : kh_mono_ms() + 3000;
}
/* Finish a grab that lost the race in kh_grab_keyboard_retry(): one attempt
 * every ~30ms for up to 3s while the field is still armed. */
static void kh_grab_retry_tick(void) {
    static long long last_ms;
    if (!g_kbd_grab_retry_deadline_ms || !dpy) return;
    long long now = kh_mono_ms();
    if (!g_default_input_elem || now > g_kbd_grab_retry_deadline_ms) {
        if (g_default_input_elem)
            kh_focus_debug_log("GRAB late-retry gave up (another client still holds the keyboard)");
        g_kbd_grab_retry_deadline_ms = 0;
        return;
    }
    if (now - last_ms < 30) return;
    last_ms = now;
    if (XGrabKeyboard(dpy, win, True, GrabModeAsync, GrabModeAsync, CurrentTime) == GrabSuccess) {
        g_kbd_grab_retry_deadline_ms = 0;
        kh_focus_debug_log("GRAB late-retry succeeded key=%s",
                           g_default_input_elem->target_id[0] ? g_default_input_elem->target_id : g_default_input_elem->id);
    }
}
static int kh_key_history_code(KeySym ks, char ch) {
    if (ch >= 32 && ch <= 126) return (unsigned char)ch;
    if (ks == XK_Tab || ks == XK_ISO_Left_Tab) return 9;
    if (ks == XK_Return || ks == XK_KP_Enter) return 13;
    if (ks == XK_Escape) return 27;
    if (ks == XK_BackSpace) return 8;
    if (ks == XK_Up) return 200; if (ks == XK_Down) return 201;
    if (ks == XK_Left) return 202; if (ks == XK_Right) return 203;
    if (ks == XK_Page_Up) return 204; if (ks == XK_Page_Down) return 205;
    return (int)ks;
}
static void kh_capture_click(int x, int y, int button) {
    char path[PATH_BUF]; history_path(path, sizeof(path));
    if (g_history_cursor < 0) { struct stat st; g_history_cursor = (stat(path,&st)==0)?st.st_size:0; }
    FILE *f = fopen(path, "a"); if (!f) return;
    fprintf(f, "MOUSE_EVENT: %d %d %d 1\n", button, x, y); fclose(f);
}
static void kh_capture_key(KeySym ks, char ch) {
    char path[PATH_BUF]; history_path(path, sizeof(path));
    if (g_history_cursor < 0) { struct stat st; g_history_cursor = (stat(path,&st)==0)?st.st_size:0; }
    FILE *f = fopen(path, "a"); if (!f) return;
    fprintf(f, "KEY_PRESSED: %d\n", kh_key_history_code(ks, ch)); fclose(f);
}

static int poll_agent_history(void) {
    char path[PATH_BUF];
    history_path(path, sizeof(path));
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    if (g_history_cursor < 0) { g_history_cursor = st.st_size; return 0; }
    if (st.st_size < g_history_cursor) { g_history_cursor = st.st_size; return 0; }
    if (st.st_size == g_history_cursor) return 0;
    /* Consume this process's own history mailbox even when another
     * window has X focus. Requiring hq_window_has_x_focus() forced
     * agents onto xdotool/XTest, which steals the human's browser
     * (k9: file relay exists so a human can use the SAME display).
     * Dual-consume of one file by two processes is a different bug
     * (one history file per mode/process); do not "fix" it by
     * ignoring the mailbox. Cursor still skips leftover on first
     * sight (g_history_cursor < 0 above). */
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    fseek(f, g_history_cursor, SEEK_SET);
    int n = 0;
    char line[64];
    long consumed = g_history_cursor;
    while (fgets(line, sizeof(line), f)) {
        char *nl = strchr(line, '\n');
        if (!nl) break;
        *nl = '\0';
        long here = ftell(f);
        if (line[0] != '#') { /* '#'-prefixed lines are audit comments, not commands */
            if (strncmp(line, "MOUSE_EVENT: ", 13) == 0) {
                int button = 0, mx = 0, my = 0, is_press = 1;
                int nf = sscanf(line + 13, "%d %d %d %d", &button, &mx, &my, &is_press);
                if (nf >= 3 && is_press && (button == 4 || button == 5)) {
                    if (generic_sbar_wheel(mx, my, (button == 5) ? 1 : -1))
                        n++;
                    else {
                        g_default_scrolllist_scroll += (button == 5) ? 1 : -1;
                        n++;
                    }
                } else if (nf >= 3 && is_press && button == 3) {
                    kh_open_cli_io_context_menu(kh_ctx_hit(mx, my), mx, my);
                    n++;
                } else if (nf >= 3 && is_press && button != 3 && button != 4 && button != 5) {
                    popup_handle_click(mx, my);
                    n++;
                }
                /* PITFALL #52 (tpmos: MOUSE MOVE REDRAW SPAM) - do NOT count
                 * a bare relayed pointer-move (button 0 / not a press / a
                 * plain release) as consumed input. Only real clicks and
                 * wheel notches (handled above, each n++'d there) dirty the
                 * frame. Counting every move here made a focused generic
                 * window repaint on every mouse twitch over it = flicker. */
            } else if (strncmp(line, "STRING: ", 8) == 0) {
                kh_cliio_exec(line + 8);
                n++;
            } else if (strncmp(line, "KEY_PRESSED: ", 13) == 0) {
                int code = atoi(line + 13);
                if (code > 0) { dispatch_relay_code(code); n++; }
            } else {
                int code = atoi(line);
                if (code > 0) { dispatch_relay_code(code); n++; }
            }
        }
        consumed = here;
    }
    fclose(f);
    g_history_cursor = consumed;
    return n;
}

/* ---- XDND drop target (see the g_drop_action block comment) ---- */
static Atom ga_xdnd_aware, ga_xdnd_enter, ga_xdnd_position, ga_xdnd_leave,
            ga_xdnd_drop, ga_xdnd_selection, ga_xdnd_status, ga_xdnd_finished,
            ga_xdnd_action_copy, ga_uri_list;
static Window g_xdnd_source = None;
static int g_xdnd_awaiting = 0;

static void xdnd_init_atoms(Display *dpy) {
    ga_xdnd_aware      = XInternAtom(dpy, "XdndAware", False);
    ga_xdnd_enter      = XInternAtom(dpy, "XdndEnter", False);
    ga_xdnd_position   = XInternAtom(dpy, "XdndPosition", False);
    ga_xdnd_leave      = XInternAtom(dpy, "XdndLeave", False);
    ga_xdnd_drop       = XInternAtom(dpy, "XdndDrop", False);
    ga_xdnd_selection  = XInternAtom(dpy, "XdndSelection", False);
    ga_xdnd_status     = XInternAtom(dpy, "XdndStatus", False);
    ga_xdnd_finished   = XInternAtom(dpy, "XdndFinished", False);
    ga_xdnd_action_copy = XInternAtom(dpy, "XdndActionCopy", False);
    ga_uri_list        = XInternAtom(dpy, "text/uri-list", False);
}

/* Advertise XDND v5 support - only when the loaded .chtpm actually
 * declared a drop_action. Called right after the popup window maps. */
static void xdnd_attach_if_needed(Display *dpy, Window w) {
    if (!g_drop_action[0]) return;
    long ver = 5;
    XChangeProperty(dpy, w, ga_xdnd_aware, XA_WINDOW, 32, PropModeReplace,
                    (unsigned char *)&ver, 1);
    XSync(dpy, False);
}

/* In-place %XX decode for file:// URIs (spaces etc arrive escaped). */
static void uri_decode_inplace(char *s) {
    char *r = s, *w = s;
    while (*r) {
        if (r[0] == '%' && isxdigit((unsigned char)r[1]) && isxdigit((unsigned char)r[2])) {
            char hex[3] = { r[1], r[2], 0 };
            *w++ = (char)strtol(hex, NULL, 16);
            r += 3;
        } else {
            *w++ = *r++;
        }
    }
    *w = '\0';
}

/* SelectionNotify arrived: read the uri-list property, take the first
 * entry that names an EXISTING DIRECTORY (falling back to the first
 * existing path of any kind), export it as $DROP_PATH and run
 * g_drop_action with dispatch()'s exact positional convention. Does
 * NOT quit the window. Always answers XdndFinished so the source's
 * drag cursor doesn't stick. */
static void xdnd_handle_selection(Display *dpy, Window win) {
    Atom actual = None; int fmt = 0; unsigned long n = 0, left = 0;
    unsigned char *data = NULL;
    char path[PATH_BUF] = "";
    char first_any[PATH_BUF] = "";
    if (XGetWindowProperty(dpy, win, ga_uri_list, 0, 65536, True /*delete*/,
                           AnyPropertyType, &actual, &fmt, &n, &left, &data) == Success && data && n > 0) {
        char *line = (char *)data, *end = (char *)data + n;
        while (line < end && !path[0]) {
            char *nl = memchr(line, '\n', (size_t)(end - line));
            size_t len = nl ? (size_t)(nl - line) : (size_t)(end - line);
            char item[PATH_BUF];
            if (len >= sizeof(item)) len = sizeof(item) - 1;
            memcpy(item, line, len); item[len] = '\0';
            size_t L = strlen(item);
            while (L > 0 && (item[L-1] == '\r' || item[L-1] == ' ')) item[--L] = '\0';
            if (L > 0) {
                char *p = item;
                if (strncmp(p, "file://", 7) == 0) {
                    p += 7;
                    char *slash = strchr(p, '/');          /* skip host part */
                    p = slash ? slash : p + strlen(p);
                }
                uri_decode_inplace(p);
                struct stat st;
                if (p[0] && stat(p, &st) == 0) {
                    /* prefer the first dropped DIRECTORY; remember the
                     * first existing path of any kind as a fallback so
                     * a stray-file drop still lands somewhere useful
                     * (the handler script decides what's valid). */
                    if (S_ISDIR(st.st_mode)) snprintf(path, sizeof(path), "%s", p);
                    else if (!first_any[0]) snprintf(first_any, sizeof(first_any), "%s", p);
                }
            }
            line = nl ? nl + 1 : end;
        }
        XFree(data);
    }
    if (!path[0] && first_any[0]) snprintf(path, sizeof(path), "%s", first_any);
    if (path[0]) {
        setenv("DROP_PATH", path, 1);
        char cmd[PATH_BUF * 3];
        snprintf(cmd, sizeof(cmd), "%s '%s' '%s' >/dev/null 2>&1 &",
                 g_drop_action, g_package_dir, g_house_root);
        int rc = system(cmd);
        (void)rc;
    } else {
        unsetenv("DROP_PATH");
    }
    if (g_xdnd_source != None) {
        XEvent fin;
        memset(&fin, 0, sizeof(fin));
        fin.xclient.type = ClientMessage;
        fin.xclient.window = g_xdnd_source;
        fin.xclient.message_type = ga_xdnd_finished;
        fin.xclient.format = 32;
        fin.xclient.data.l[0] = (long)win;
        XSendEvent(dpy, g_xdnd_source, False, NoEventMask, &fin);
    }
    g_xdnd_source = None;
    g_drop_highlight = 0;
}

static int kh_is_drop_target_window(void) {
    if (g_drop_action[0]) return 1;
    return g_window && elem_has_class(g_window, "file-explorer-pal");
}

static void kh_write_drop_zone(void) {
    char dirp[PATH_BUF], path[PATH_BUF], dest[PATH_BUF];
    FILE *f;
    if (!kh_is_drop_target_window() || !g_house_root[0]) return;
    snprintf(dirp, sizeof(dirp), "%s/#.desktop/khtpm_drop_zones", g_house_root);
    mkdir(dirp, 0777);
    dest[0] = 0;
    if (g_package_dir[0]) {
        char ui[PATH_BUF];
        snprintf(ui, sizeof(ui), "%s/file_explorer_ui.txt", g_package_dir);
        FILE *uf = fopen(ui, "r");
        if (uf) {
            char line[PATH_BUF];
            while (fgets(line, sizeof(line), uf)) {
                if (!strncmp(line, "dir=", 4)) {
                    char *nl = strchr(line, '\n'); if (nl) *nl = 0;
                    snprintf(dest, sizeof(dest), "%s", line + 4);
                }
            }
            fclose(uf);
        }
    }
    snprintf(path, sizeof(path), "%s/%d.txt", dirp, (int)getpid());
    f = fopen(path, "w");
    if (!f) return;
    {
        int zx = g_win_x, zy = g_win_y, zw = g_win_w, zh = g_win_h;
        if (dpy && win) {
            Window child = None;
            int rx = 0, ry = 0;
            XTranslateCoordinates(dpy, win, RootWindow(dpy, DefaultScreen(dpy)),
                                  0, 0, &rx, &ry, &child);
            zx = rx; zy = ry;
        }
        fprintf(f, "pid=%d\nwin=0x%lx\nx=%d\ny=%d\nw=%d\nh=%d\ndest=%s\ncolor=%s\n",
                (int)getpid(), (unsigned long)win, zx, zy, zw, zh, dest, g_drop_highlight_color);
    }
    fclose(f);
}

static void kh_drop_zone_unregister(void) {
    char path[PATH_BUF];
    if (!g_house_root[0]) return;
    snprintf(path, sizeof(path), "%s/#.desktop/khtpm_drop_zones/%d.txt", g_house_root, (int)getpid());
    unlink(path);
}


static int kh_read_drag_hover(void) {
    char path[PATH_BUF], line[PATH_BUF];
    FILE *f;
    int pid = 0;
    g_drop_hover_name[0] = 0;
    if (!g_house_root[0]) return 0;
    snprintf(path, sizeof(path), "%s/#.desktop/drag_hover_pid.txt", g_house_root);
    f = fopen(path, "r");
    if (!f) return 0;
    while (fgets(line, sizeof(line), f)) {
        char *nl = strchr(line, '\n'); if (nl) *nl = 0;
        if (!strncmp(line, "pid=", 4)) pid = atoi(line + 4);
        else if (!strncmp(line, "name=", 5))
            snprintf(g_drop_hover_name, sizeof(g_drop_hover_name), "%s", line + 5);
        else if (line[0] >= '0' && line[0] <= '9')
            pid = atoi(line); /* old one-line format */
    }
    fclose(f);
    return pid;
}


static void hq_request_redraw(void) {
    if (!g_quit) g_frame_dirty = 1;
}

/* Same real cheap "changed marker" convention as khtpm_strip_parser.c/
 * tp_desktop_window_rgb.c's own theme_changed_dirty() (dc759f3c) -
 * fully generic (plain house_root string + a shared dirty-marker
 * file) despite its pchq-sounding name; used every-mode, every-tick
 * (see hq_idle_tick()'s own call site for why). */
static long g_pchq_theme_changed_cursor = 0;
static int pchq_theme_changed_dirty(const char *house_root) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/livedesk_theme_changed.txt", house_root);
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    if (st.st_size != g_pchq_theme_changed_cursor) { g_pchq_theme_changed_cursor = st.st_size; return 1; }
    return 0;
}

/* REAL FIX 2026-09-04, direct live report ("clicking changing to
 * 2step doesn't change tb yet") - desktop_load_click_two_step() was
 * only ever called once, at startup (main()/tp_main()). Every OTHER
 * window is short-lived (a fresh launch already sees a just-toggled
 * setting), but the taskbar/dock strip is long-running - it never
 * saw a toggle made in Settings without a manual restart. Same real
 * mtime-gate convention as pchq_theme_changed_dirty() above (avoid a
 * needless fopen+read every tick for every window in the house);
 * called every tick in hq_idle_tick(), same shared every-mode spot. */
/* REAL FIX 2026-09-10, DIAMOND standard (see hq_ui_pdl_touch_marker()'s
 * own header comment) - hq_ui.pdl itself is rewritten in place, not
 * appended, so it can never be the marker; #.desktop/hq_ui_pdl_changed
 * .txt is the real append-only marker every writer touches. The
 * standard's own rule 1: "a render happens iff an append-only marker
 * file's size grew - strictly greater, monotonic. Never mtime, never a
 * hash." -1 = not yet initialized (the FIRST call in a process just
 * establishes the baseline without reloading - the real settings were
 * already read once at startup via desktop_load_click_two_step()). */
static long g_hq_ui_pdl_marker_sz = -1;
static void hq_ui_pdl_reload_if_changed(const char *house_root) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/hq_ui_pdl_changed.txt", house_root);
    struct stat st;
    if (stat(path, &st) != 0) return;
    if (g_hq_ui_pdl_marker_sz < 0) { g_hq_ui_pdl_marker_sz = (long)st.st_size; return; }
    if ((long)st.st_size > g_hq_ui_pdl_marker_sz) {
        g_hq_ui_pdl_marker_sz = (long)st.st_size;
        int old_scale = g_ui_scale_pct;
        desktop_load_click_two_step(house_root);
        if (g_ui_scale_pct != old_scale) {
            /* font_scale changed in Settings while this window is open:
             * re-size the chrome font, relayout (box metrics changed,
             * not just a colour), repaint. */
            reload_font_ui();
            assign_nav_and_layout();
            hq_request_redraw();
        }
    }
}

static void hq_idle_tick(void) {
    kh_grab_retry_tick();
    /* A dock whose grab release was missed (FocusOut swallowed, mode ==
     * NotifyGrab) used to keep its display-wide XGrabKeyboard until the next
     * KEY reached it - so an armed field in another window (csv-hq's grid)
     * kept losing the grab. Check live focus every ~100ms instead. */
    if (window_is_dock() && g_dock_kbd_win) {
        static long long last_dock_chk_ms;
        long long nowm = kh_mono_ms();
        if (nowm - last_dock_chk_ms >= 100) { last_dock_chk_ms = nowm; dock_release_keyboard_if_left(); }
    }
    if (kh_is_drop_target_window()) {
        int hp = kh_read_drag_hover();
        int want = (hp == (int)getpid()) || (g_xdnd_source != None);
        if (want != g_drop_highlight) {
            g_drop_highlight = want;
            hq_request_redraw();
        } else if (want) {
            hq_request_redraw(); /* keep the hover fill live every tick */
        }
        kh_write_drop_zone();
    }
    /* PC-HQ-FOCUS-AND-INTERACT-ACTIVATE.md: idle pointer-over
     * XSetInputFocus deleted (that was the focus hog). One-shot
     * take-focus is ButtonPress / post-map only.
     * pc-hq-leg-vs-nu-fix.md §6b: Interact Mode arm must not wait on
     * a vars-hash reparse. Reload projector vars and rescan every tick
     * (legacy read active_gui_is_typing.txt once per frame). */
    if (g_vars_path[0]) kh_load_vars_multi(g_vars_path);
    kh_scan_interact_relay();
    /* REAL, NEW 2026-09-14 - real cross-process CUT/COPY/PASTE bridge
     * for the cli_io/text_area right-click menu (kh_open_cli_io_
     * context_menu()'s own header comment) - cheap, mtime-gated. */
    kh_poll_cli_io_ctxmenu_action();
    /* REAL, NEW 2026-09-14 - reap the menu-fork child (launch_khtpm_
     * menu()) whenever it actually exits. The kill-then-relaunch guard
     * at the top of launch_khtpm_menu() also calls waitpid(WNOHANG)
     * right after SIGTERM, but that's almost never fast enough to
     * catch a still-running child - an entity that opens exactly one
     * menu and never opens a second left that child as a permanent
     * <defunct> zombie for the rest of its own life (confirmed live,
     * 2026-09-14 - ava/asa's own real menu-fork PIDs). This is the
     * only other place g_khtpm_menu_pid's child can exit from (user
     * closes the menu, or its own action fires and it self-quits). */
    if (g_khtpm_menu_pid > 0) {
        int wstatus;
        if (waitpid(g_khtpm_menu_pid, &wstatus, WNOHANG) == g_khtpm_menu_pid) {
            g_khtpm_menu_pid = -1;
        }
    }
    /* REAL, NEW 2026-09-05 - age out the top-right "copied" tag: one
     * last repaint the moment it crosses ~2s old, then it stays cleared
     * (this block is a no-op once g_clip_copied_at is back to 0). */
    if (g_clip_copied_at && (time(NULL) - g_clip_copied_at) > 2) {
        g_clip_copied_at = 0;
        if (!g_quit) redraw();
    }
    /* REAL, NEW 2026-08-31 (xperiments/khtpm-generic-dispatch-design.md
     * §5 - direct instruction: "the renderer/parser should have no
     * need to know the difference [between projects]... why are there
     * different parsing standards for different apps... they should
     * all use the same layout tags and standards"). Generic capability
     * #1: the plain default page/item mode (the SAME one taskbar-
     * settings/entity-menus/choice-picker/the open-hai sessions proof
     * already use) now re-reads its own .chtpm file whenever it
     * changes on disk, not just once at startup - lets a real manager
     * keep regenerating real, generic markup (same real philosophy
     * #.haiku+/tpmos-re-dox/fo-menu-sys.md already documents for the
     * ASCII/chtpm_parser.c family) without this renderer needing ANY
     * project-specific C code. Scoped OFF for db-hq/events-hq/chat-hai
     * (each owns its own real content-refresh mechanism against its
     * own cached Elem pointers already - reparsing their window from
     * under them would invalidate those, real, deliberate exclusion,
     * not an oversight). */
    {
        /* REAL, NEW 2026-09-13, direct live report ("all options in tb
         * and bottom bar disappear... top reappeared, but not bottom
         * for a while... there was no change so why did this happen?
         * even if it is idle or slow cpu, it shouldn't do that") - the
         * strip's own kh_focus_debug.log already proves the periodic
         * (once-a-minute, clock-driven) reparse ITSELF always reports
         * success ("INCREMENTAL_REPARSE ok removed=0", every single
         * time, for hours) - so whatever actually blanked the bars that
         * time isn't a parse/diff failure. Real permanent, always-on
         * (not env-gated - this is cheap, one clock_gettime + one log
         * line per reparse, not per frame) timing around this exact
         * tick, so the NEXT occurrence leaves hard evidence (a real
         * slow assign_nav_and_layout()/redraw() call logged with its
         * own duration) instead of another guess. */
        struct timespec _rp_t0, _rp_t1;
        int _rp_dock = window_is_dock();
        /* REAL FIX 2026-09-14, direct live report ("the bottom toolbar
         * is completely gone. i think it died again"): live-confirmed
         * via a raw X window-tree dump (not guessed) that this was NOT
         * the render-staleness bug this same symptom class usually is
         * (§bug_bounty.md) - the underlying data was fine, the process
         * was alive and ticking normally (DOCK_TICK firing every ~10s),
         * but `g_dock_peer_win` (the bottom bar's own real X Window)
         * genuinely did not exist anywhere on the X server - a
         * different, more severe failure than "stale," and NOT a
         * self-heal killing anything (self-heal is fully disabled, see
         * ktb_reload()'s own 2026-09-14 header comment in
         * khtpm_taskbar_manager.c - and grepped: nothing anywhere in
         * this file ever XUnmapWindow/XDestroyWindow's g_dock_peer_win
         * while the process is alive). Root cause not pinned down with
         * certainty (the startup parse-retry loop a few hundred lines
         * up logged no failure that run), but the SHAPE is the same one
         * this exact bug class keeps taking: a one-time startup
         * decision (`if (g_dock_peer) { XCreateWindow(...) }`, runs
         * ONCE before the event loop starts) with zero later recovery
         * if it's ever skipped or the window is ever lost by any other
         * means. Real, structural, narrow fix matching the house's own
         * post-mortem on the OLD self-heal (khtpm_taskbar_manager.c's
         * ktb_reload() comment): this touches ONLY this process's own
         * window creation, never another process's lifecycle, so it
         * cannot repeat that mechanism's own "died/flickered/vanished"
         * incident history. If the peer's DATA exists (`g_dock_peer`
         * non-NULL, real - the marker-gated reparse above already
         * guarantees this stays correct) but its WINDOW doesn't
         * (`g_dock_peer_win == 0`, or the window id it holds is no
         * longer valid on the server), recreate it right here using the
         * exact same real creation code the startup path uses (kept
         * in its own function, `kh_ensure_dock_peer_window()`, so
         * there is exactly one real place this window ever gets built,
         * not two independently-maintained copies). Cheap: one
         * `XGetWindowAttributes` liveness probe per tick, real window
         * creation only on the rare tick it's actually needed. */
        if (_rp_dock) kh_ensure_dock_peer_window();
        if (_rp_dock) clock_gettime(CLOCK_MONOTONIC, &_rp_t0);
        int _rp_changed = reparse_chtpm_if_changed();
        if (_rp_dock) {
            clock_gettime(CLOCK_MONOTONIC, &_rp_t1);
            double _rp_ms = (_rp_t1.tv_sec - _rp_t0.tv_sec) * 1000.0 + (_rp_t1.tv_nsec - _rp_t0.tv_nsec) / 1e6;
            if (_rp_changed || _rp_ms > 5.0)
                kh_focus_debug_log("DOCK_TICK reparse_changed=%d reparse_ms=%.2f", _rp_changed, _rp_ms);
        }
        if (_rp_changed) {
            if (_rp_dock) clock_gettime(CLOCK_MONOTONIC, &_rp_t0);
            assign_nav_and_layout(); redraw();
            if (_rp_dock) {
                clock_gettime(CLOCK_MONOTONIC, &_rp_t1);
                double _rp_ms = (_rp_t1.tv_sec - _rp_t0.tv_sec) * 1000.0 + (_rp_t1.tv_nsec - _rp_t0.tv_nsec) / 1e6;
                kh_focus_debug_log("DOCK_TICK layout+redraw_ms=%.2f g_n_elems=%d", _rp_ms, g_n_elems);
            }
            /* REAL FIX 2026-09-04 (pc-hq-bugs.md Bug 2, same root cause
             * as handle_key()'s own new guard above) - this used to
             * blindly re-grab whenever g_dock_kbd_win was still set,
             * regardless of whether the dock actually still holds real
             * focus - one of the two paths that could reinforce a
             * stale grab forever. Same live-focus check first. */
            if (g_dock_kbd_win) {
                Window fw = None; int rev = 0;
                if (dpy) XGetInputFocus(dpy, &fw, &rev);
                if (dock_is_our_win(fw)) dock_grab_keyboard(g_dock_kbd_win);
                else { kh_ungrab_kbd(); g_dock_kbd_win = None; }
            }
            /* real content growth may have just recreated buf as a
             * blank Pixmap (see redraw()'s own resize-safety comment) -
             * a second real redraw() repaints it for real THIS tick,
             * instead of leaving a blank window until the next
             * unrelated event. Cheap - a no-op second call whenever no
             * resize was needed. */
            redraw();
        }
    }
    /* REAL FIX 2026-09-01 (live report: "entities changes opacity
     * automatically... but windows have to be reset to change. they
     * didn't used to be like this") - live, event-driven opacity
     * reapply already existed in this exact binary
     * (pchq_theme_changed_dirty()/set_window_opacity(), ported from
     * tp_desktop_window_rgb.c's own theme_changed_dirty()), but was
     * only ever wired up for pchq_board mode (~line 11708) - every
     * OTHER window this binary draws (open-hai/chat-hai/network-
     * browser/db-hq/events-hq/taskbar-settings/entity-menus) had NO
     * live reapply at all, so a theme opacity change only ever took
     * effect on the NEXT fresh launch. Wired into this same shared,
     * every-mode, every-tick spot (pchq_theme_changed_dirty() itself
     * is already fully generic despite its name - a plain house_root
     * string + a shared dirty-marker file, nothing pchq-specific) so
     * every window gets the same live behavior entities already have,
     * with zero per-mode duplication. */
    hq_ui_pdl_reload_if_changed(g_house_root);
    if (pchq_theme_changed_dirty(g_house_root)) {
        set_window_opacity(dpy, win, load_theme_opacity());
        /* REAL FIX 2026-09-04 (live incident: bg/fg picked the same
         * colour, corrected in the pdl, but every already-open
         * non-dock window - palette/chat-hai/db-hq-pal/entity-menu -
         * kept showing the broken theme until manually restarted).
         * load_theme_colors()/redraw() was dock-only here; every
         * window needs the SAME live reload, not just the taskbar. */
        load_theme_colors();
        hq_request_redraw();
        if (window_is_dock() && g_dock_peer_win) set_window_opacity(dpy, g_dock_peer_win, load_theme_opacity());
    }
    if (poll_agent_history() > 0 && !g_quit) hq_request_redraw();
    dock_poll_strip_state();   /* mirror terminal-driven nav into strip_ascii_current_frame.txt promptly */
    if (g_default_has_sidebar_panel && !window_is_dock() && dpy && win) {
        char restore_path[PATH_BUF];
        snprintf(restore_path, sizeof(restore_path),
                 "%s/#.desktop/livedesk_hq_restore_%d.txt", g_house_root, (int)getpid());
        if (access(restore_path, F_OK) == 0) {
            unlink(restore_path);
            g_hq_minimized = 0;
            XMapWindow(dpy, win);
            XRaiseWindow(dpy, win);
            XSetInputFocus(dpy, win, RevertToParent, CurrentTime);
            XFlush(dpy);
            hq_request_redraw();
        }
    }
    if (g_quit) return;
}

/* REAL, NEW 2026-09-14 - see g_text_drag_elem's own decl comment.
 * Converts a real click/drag point (px,py, window-relative) on an
 * ARMED cli_io/text_area into a byte offset into its own buffer, using
 * kh_elem_badge_label_x()/kh_text_offset_at_x() (khtpm_draw_core.c) so
 * this stays consistent with the real draw position rather than a
 * second, drifting guess. cli_io is treated as single-line (the real
 * common case - address-bar-style fields) - a wrapped cli_io
 * approximates to its own first visual row, an accepted v1 limit.
 * text_area does real row-aware hit-testing, mirroring draw_elem()'s
 * own word-wrap walk closely enough for the common case; a heavily
 * wrapped long line may land a few bytes off in a rare edge case -
 * still real, working drag-select for ordinary editing. */
static int kh_input_offset_at_click(Elem *e, int px, int py) {
    int is_area = (strcmp(e->tag, "text_area") == 0);
    XftFont *font = font_for(&e->style);
    if (!font) return is_area ? (int)strlen(e->text_area_buffer) : (int)strlen(e->input_buffer);
    if (!is_area) {
        int text_x = kh_elem_badge_label_x(e);
        char shown[256 + 64];
        snprintf(shown, sizeof(shown), "%s%s", e->label, e->input_buffer);
        int label_len = (int)strlen(e->label);
        int off = kh_text_offset_at_x(font, shown, px - text_x) - label_len;
        int blen = (int)strlen(e->input_buffer);
        if (off < 0) off = 0;
        if (off > blen) off = blen;
        return off;
    }
    /* text_area - real row-aware hit test, mirrors draw_elem()'s own
     * word-wrap walk (khtpm_draw_core.c, text_area branch) closely. */
    static char shown_buf[4096 + 300];
    snprintf(shown_buf, sizeof(shown_buf), "%s%s", e->label, e->text_area_buffer);
    const char *shown_label = shown_buf;
    int label_len = (int)strlen(e->label);
    int text_x = e->x + 4;
    int avail_w = e->w > 0 ? (e->x + e->w) - text_x : -1;
    int line_h = font->ascent - font->descent > 0 ? font->ascent - font->descent : 12;
    line_h += 4;
    int badge_reserve = (e->nav_index > 0) ? line_h : 0;
    int max_lines = (e->h - badge_reserve) / line_h;
    if (max_lines < 1) max_lines = 1;
    int rel_x = px - text_x;
    int target_row = (py - e->y - badge_reserve) / line_h;
    if (target_row < 0) target_row = 0;
    if (target_row >= max_lines) target_row = max_lines - 1;

    const char *lp = shown_label;
    int line_no = 0;
    int found_off = -1;
    while (*lp && line_no < max_lines && found_off < 0) {
        const char *nl = strchr(lp, '\n');
        int logical_len = nl ? (int)(nl - lp) : (int)strlen(lp);
        const char *sp = lp;
        int remaining = logical_len;
        do {
            int last_good_space = -1, i = 0;
            XGlyphInfo lw;
            for (;;) {
                if (i >= remaining) break;
                if (sp[i] == ' ') last_good_space = i;
                XftTextExtentsUtf8(dpy, font, (const FcChar8 *)sp, i + 1, &lw);
                if (avail_w > 0 && lw.width > avail_w) break;
                i++;
            }
            int cut = i;
            int more_in_logical = (i < remaining);
            if (more_in_logical && last_good_space >= 0) cut = last_good_space;
            if (cut == 0 && more_in_logical) cut = 1;
            if (line_no == target_row) {
                char row_buf[600];
                snprintf(row_buf, sizeof(row_buf), "%.*s", cut, sp);
                int row_start = (int)(sp - shown_label);
                found_off = row_start + kh_text_offset_at_x(font, row_buf, rel_x);
            }
            sp += cut;
            remaining -= cut;
            if (more_in_logical && last_good_space >= 0 && remaining > 0 && *sp == ' ') { sp++; remaining--; }
            line_no++;
        } while (remaining > 0 && line_no < max_lines && found_off < 0);
        if (found_off >= 0 || !nl) break;
        lp = nl + 1;
    }
    if (found_off < 0) found_off = (int)strlen(shown_label); /* past all real rows - end of text */
    int buf_off = found_off - label_len;
    int blen = (int)strlen(e->text_area_buffer);
    if (buf_off < 0) buf_off = 0;
    if (buf_off > blen) buf_off = blen;
    return buf_off;
}

static void popup_handle_click(int px, int py) {
    g_bar_click_x = -1; /* fresh frame: no pending bar click until the
                         * bar-specific hit-test below arms one */
    /* REAL, NEW 2026-08-29 (direct instruction: "i think whole house
     * should have the same single|doubleclick rule or it could be
     * confusing... it should be house wide if possible/ez") - same
     * click_focus_then_activate() every other mode's click handler now
     * uses, applied here too for consistency. Real, honest trade-off
     * this house's own click_two_step=0 escape hatch in hq_ui.pdl
     * exists for: a right-click context menu is a different UX shape
     * than a persistent window (it's about to close either way), but
     * direct instruction was for uniformity over that distinction, so
     * this follows it rather than silently keeping an exception. */
    int i0 = 0, i1 = g_n_nav;
    if (g_dock_click_menu && g_dock_drop_lo) {
        i0 = g_dock_drop_lo - 1;
        i1 = g_dock_drop_hi;
    } else if (g_dock_peer && g_dock_header_nav_hi > 0) {
        if (g_dock_click_peer) { i0 = g_dock_header_nav_hi; i1 = g_n_nav; }
        else { i0 = 0; i1 = g_dock_header_nav_hi; }
    }
    for (int i = i0; i < i1; i++) {
        Elem *it = g_nav[i];
        if (px >= it->x && px < it->x + it->w && py >= it->y && py < it->y + it->h) {
            /* generic <bar>: remember the exact click X (relative-to-x
             * fraction is computed in activate_focused()'s bar branch =
             * the ONLY place a bar click can actually reach the onClick,
             * since Click is a pointer form of the exact same Enter). */
            g_bar_click_x = (strcmp(it->tag, "bar") == 0) ? px : -1;
            if (!click_focus_then_activate(it)) { redraw(); return; }
            activate_focused();
            /* REAL, NEW 2026-09-14, direct live report ("shift arrow
             * working now. but not mouse drag. can u fix that?") - a
             * real click on an ARMED cli_io/text_area (this is only
             * reached once click_focus_then_activate() has genuinely
             * armed it, not the first half of a click_two_step pair)
             * positions the cursor at the real click point instead of
             * activate_focused()'s own default end-of-buffer arm, and
             * starts a real drag-select: MotionNotify (button 1 held)
             * extends [sel_anchor,cursor) as the mouse moves, same
             * shape every other text editor's own click-drag gives. */
            if (g_default_input_elem == it &&
                (strcmp(it->tag, "cli_io") == 0 || strcmp(it->tag, "text_area") == 0)) {
                it->cursor = kh_input_offset_at_click(it, px, py);
                it->sel_anchor = it->cursor;
                g_text_drag_elem = it;
            }
            if (!g_quit) assign_nav_and_layout();
            redraw();
            return;
        }
    }
    if (window_is_dock() && g_dock_peer && g_dock_header_nav_hi > 0 && !g_dock_click_menu) {
        if (g_dock_click_peer) {
            int first_bot = g_dock_header_nav_hi + 1;
            if (first_bot <= g_n_nav) g_focus_nav = first_bot;
        } else if (g_focus_nav > g_dock_header_nav_hi) {
            g_focus_nav = 1;
        }
        redraw();
    }
}

/* REAL, NEW 2026-08-29, direct instruction ("they should be separate
 * functions when possible and not affect other functionality") -
 * pulled out of hq_dispatch_xevent's own ButtonPress handling so the
 * SAME real logic (bounds-check against the picker's own window vs.
 * real desktop, ledger write, place-op invocation) can be called from
 * two real callers: the event-based path (still works for synthetic/
 * XTest clicks) and dbhq_rmmv_poll_pointer() below (the real fix for
 * actual human mouse input - see RMMV-CLICK-CAPTURE-INVESTIGATION-
 * 2026-08-29.txt for the full root-cause trail: real hardware pointer
 * events are never delivered to an XGrabPointer-holding XWayland
 * client under this Mutter version, a real, known, still-open upstream
 * bug, not something fixable in this house's own code). Takes real
 * root-relative coordinates; does not care which caller resolved them. */
/* REAL, NEW 2026-08-29 - the actual, human-usable fix for real mouse
 * clicks (see dbhq_rmmv_handle_desktop_click's own header for the
 * root-cause). XQueryPointer is a synchronous request/reply, not an
 * asynchronously delivered event, so it sidesteps Mutter's Wayland-
 * surface-focus input routing gap entirely - polls real button state
 * directly rather than waiting for an event that real hardware clicks
 * never generate for a grabbing XWayland client. Called once per
 * event-loop tick (~150ms, see hq_run_event_loop) only while armed;
 * detects a real 0->1 edge on Button1 so a single physical click
 * triggers exactly once, not once per poll tick while held down. */

/* REAL, NEW 2026-09-04 (direct instruction: a taskbar-nav click, or a
 * click on a buried window, should bring it to the top even with
 * always-on-top=false). XRaiseWindow alone is only a hint the WM may
 * ignore under focus-stealing prevention in WM-managed mode; the
 * EWMH-sanctioned "activate" is a _NET_ACTIVE_WINDOW ClientMessage to
 * the root with source indication 2 (direct user action), which Mutter
 * honours. Harmless (redundant with the raise) when override_redirect.
 * Also focuses. */
static void kh_raise_and_focus(Window w) {
    if (!dpy || !w) return;
    XRaiseWindow(dpy, w);
    /* REAL FIX 2026-09-14 - see g_last_event_time's own decl comment.
     * CurrentTime here (both the ClientMessage below and
     * XSetInputFocus) is the real, documented reason this could
     * silently fail to raise a WM-managed window: Mutter's focus-
     * stealing-prevention arbitrates on this timestamp against its own
     * last-seen user-activity time, and a CurrentTime request carries
     * no real evidence of "this really is a direct, fresh user action"
     * - a real timestamp from the click that triggered this call does.
     * Falls back to CurrentTime only if no real event has been seen
     * yet (startup edge case). */
    Time ts = g_last_event_time ? g_last_event_time : CurrentTime;
    Atom naw = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
    if (naw != None) {
        XEvent e; memset(&e, 0, sizeof(e));
        e.xclient.type = ClientMessage;
        e.xclient.window = w;
        e.xclient.message_type = naw;
        e.xclient.format = 32;
        e.xclient.data.l[0] = 2;            /* source: pager / direct user action */
        e.xclient.data.l[1] = ts;
        XSendEvent(dpy, RootWindow(dpy, DefaultScreen(dpy)), False,
                   SubstructureNotifyMask | SubstructureRedirectMask, &e);
    }
    XSetInputFocus(dpy, w, RevertToParent, ts);
    XFlush(dpy);
    /* Evidence for the next hardware attempt (no user action needed): what the
     * WM considers active and who has X focus right after a click. If keys still
     * never arrive, compare with `xrecord`-style delivery in bug_bounty.md. */
    {
        Window fw = None; int rev = 0; XGetInputFocus(dpy, &fw, &rev);
        Atom na = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False), at; int af; unsigned long nit, ba;
        unsigned char *pp = NULL; Window act = 0;
        XSync(dpy, False);
        if (XGetWindowProperty(dpy, RootWindow(dpy, DefaultScreen(dpy)), na, 0, 1, False, XA_WINDOW, &at, &af, &nit, &ba, &pp) == Success && pp) {
            if (nit > 0) act = *(Window *)pp;
            XFree(pp);
        }
        kh_focus_debug_log("CLICK-FOCUS target=0x%lx x_focus=0x%lx wm_active=0x%lx", (unsigned long)w, (unsigned long)fw, (unsigned long)act);
    }
}


static void hq_dispatch_xevent(XEvent *ev, Atom wm_delete, int is_popup) {
    /* real, current server timestamp - see g_last_event_time's own decl
     * comment. Every event type that carries one uses the same struct
     * offset in Xlib's XEvent union (KeyPress/Release, ButtonPress/
     * Release, MotionNotify all agree), so ev->xkey.time reads it
     * correctly regardless of which of those this actually is. */
    if (ev->type == KeyPress || ev->type == KeyRelease ||
        ev->type == ButtonPress || ev->type == ButtonRelease ||
        ev->type == MotionNotify)
        g_last_event_time = ev->xkey.time;

    if (ev->type == Expose) {
        /* Coalesce the damage burst. X delivers one Expose per rectangle of
         * a single damaged region (and a fresh burst every time another
         * window churns stacking above this override-redirect window) -
         * calling redraw() on each = N full-window XGetImage+XPutImage blits
         * for one logical repaint, which reads on screen as flicker of the
         * busy regions (sidebar list, panel fields). Drain every Expose
         * already queued for this window, then repaint exactly once.
         * (tpmos PITFALLS #52 spirit: don't full-redraw per raw event.) */
        XEvent drain;
        while (XCheckTypedWindowEvent(dpy, ev->xexpose.window, Expose, &drain)) { }
        redraw();
        return;
    }
    if (ev->type == ClientMessage && (Atom)ev->xclient.data.l[0] == wm_delete) {
        g_quit = 1;
        return;
    }
    /* REAL FIX 2026-08-29 - in-process rmmv armed-brush click capture,
     * see g_pal_rmmv_armed's own header comment. Must run BEFORE any
     * other ButtonPress handling below - while armed, this process
     * holds a real root-window grab, so the NEXT ButtonPress anywhere
     * on screen (x_root/y_root are absolute regardless of which window
     * the grab reports as the event window) is this click, not
     * whatever this window's own normal click logic would do with it.
     * NOTE, same day, follow-up finding (RMMV-CLICK-CAPTURE-
     * INVESTIGATION-2026-08-29.txt, root-caused by a delegated
     * subagent): this ButtonPress path only ever fires for SYNTHETIC
     * (XTest-injected) clicks - real hardware mouse clicks are never
     * delivered here at all under this Mutter/XWayland setup (a real,
     * known, still-open Mutter bug: gitlab.gnome.org/GNOME/mutter/-/
     * issues/642 - XGrabPointer() succeeds at the X-protocol level but
     * Mutter never routes real hardware pointer events to the grabbing
     * client, only to whichever surface has Wayland-level focus;
     * XTestFakeButtonEvent bypasses this by injecting directly into
     * the X server's own protocol layer). The REAL, human-usable path
     * is dbhq_rmmv_poll_pointer() below (XQueryPointer polling,
     * unaffected by this Wayland routing gap) - this event-based path
     * is kept only because it still works for synthetic/XTest testing
     * and costs nothing to leave in. */
    if (ev->type == ButtonPress) {
        /* drag-resize grip - works for ANY user-resizable window, not
         * just popups (the pc-hq board is now a managed sidebar+panel
         * window, so the popup-gated grip block below never fired for
         * it - "resize wont grow at all" report 2026-09-10). Very small
         * hot corner (KH_RESIZE_GRIP px), checked before any element
         * hit-test so a footer cell can't eat it. */
        if (g_user_resizable && !window_is_dock() && !g_win_resizing &&
            ev->xbutton.button == 1 &&
            ev->xbutton.x >= g_win_w - KH_RESIZE_GRIP && ev->xbutton.x < g_win_w &&
            ev->xbutton.y >= g_win_h - KH_RESIZE_GRIP && ev->xbutton.y < g_win_h) {
            g_win_resizing = 1;
            g_resize_start_xr = ev->xbutton.x_root;
            g_resize_start_yr = ev->xbutton.y_root;
            g_resize_start_w = g_win_w;
            g_resize_start_h = g_win_h;
            return;
        }
        /* REAL FIX 2026-09-03 (direct live report: "its way to hard to
         * get window focus. i tap click window and it still doesn't
         * have focus") - root cause, confirmed live via the new "^"/"."
         * title-bar indicator: override_redirect=true (this house's
         * default for these windows, #.desktop/livedesk_override_
         * redirect.pdl) means the window manager never manages these
         * windows AT ALL, focus included - the ONLY thing that ever
         * gave one of these windows real X input focus was main()'s own
         * post-map XSetInputFocus retry loop (2026-08-29), which fires
         * exactly ONCE, at launch. Nothing ever re-asserted it again -
         * clicking back into an already-open window after focus moved
         * elsewhere (another app, another one of these same windows)
         * did real internal nav/dispatch work (via ev->xbutton.x/y hit-
         * testing below) but never told the X server this window should
         * now own the keyboard, so a human's very next keypress kept
         * going to whichever window last called XSetInputFocus - a
         * completely ordinary "click anywhere in a window focuses it"
         * expectation this house never actually implemented for this
         * mode. A plain, unconditional call here matches what every
         * normal desktop app gets for free from its WM - these windows
         * have no WM to do it for them. ev->xbutton.window is checked
         * against win (not is_popup/mode-specific) so this is real,
         * generic, and safe for every consumer of this shared
         * dispatcher, not just chat-hai/open-hai. */
        if (ev->xbutton.window == win || (g_dock_peer_win && ev->xbutton.window == g_dock_peer_win)
            || (g_dock_menu_win && ev->xbutton.window == g_dock_menu_win)) {
            Window cw = ev->xbutton.window;
            g_dock_click_peer = (g_dock_peer_win && cw == g_dock_peer_win);
            g_dock_click_menu = (g_dock_menu_win && cw == g_dock_menu_win);
            if (window_is_dock() && g_dock_peer && g_dock_header_nav_hi > 0 && !g_dock_click_menu) {
                if (g_dock_click_peer) {
                    int first_bot = g_dock_header_nav_hi + 1;
                    if (first_bot <= g_n_nav &&
                        (g_focus_nav < first_bot || g_focus_nav > g_n_nav))
                        g_focus_nav = first_bot;
                } else if (g_focus_nav > g_dock_header_nav_hi) {
                    g_focus_nav = 1;
                }
            }
            if (window_is_dock()) {
                XRaiseWindow(dpy, cw);
                dock_grab_keyboard(cw);
                XSetInputFocus(dpy, cw, RevertToParent, CurrentTime);
            } else {
                /* click anywhere in an HQ window -> bring it (and its
                 * keyboard focus) to the top, WM-managed mode included */
                kh_raise_and_focus(cw);
                g_x11_window_focused = 1;
                /* Play-screen engage: canvas bbox, not g_nav. Never
                 * verb interact (toggle-off). */
                if (g_win_managed_focus && kh_page_has_relay_item() &&
                    kh_canvas_hit(ev->xbutton.x, ev->xbutton.y))
                    kh_interact_engage_if_needed();
            }
            if (window_is_dock() && g_dock_menu_win && cw == g_dock_menu_win &&
                ev->xbutton.button == 1 && g_dock_drop_lo >= 1) {
                /* Hit-test laid-out row boxes. y/DOCK_BAR_H was off-by-one
                 * on the long toys list (Piececraft-HQ vs Piececraft). */
                int px = ev->xbutton.x, py = ev->xbutton.y;
                int i0 = g_dock_drop_lo - 1, i1 = g_dock_drop_hi;
                if (i1 > g_n_nav) i1 = g_n_nav;
                for (int i = i0; i < i1; i++) {
                    Elem *it = g_nav[i];
                    if (!it) continue;
                    if (px >= it->x && px < it->x + it->w &&
                        py >= it->y && py < it->y + it->h) {
                        /* REAL FIX 2026-09-10 (direct follow-up report:
                         * "no. there still firing from one" - this is
                         * a SECOND, separate dropdown click path from
                         * click_focus_then_activate()'s own dropdown-
                         * child branch just fixed: g_dock_menu_win is
                         * the standalone HQ popup window (toys/pals/
                         * session/etc - built from the manager's own
                         * hq_menu[] rows, not the xhtpm dropdown-child
                         * pattern), and its click hit-test unconditionally
                         * fired on the first click, same bypass, just a
                         * different code path. Route through the shared
                         * two-step gate instead of hand-rolling the
                         * unconditional fire again. */
                        if (!click_focus_then_activate(it)) { redraw(); return; }
                        activate_focused();
                        if (!g_quit) redraw();
                        return;
                    }
                }
            }
        }
        if (is_popup) {
            /* user drag-resize: button-1 in the bottom-right KH_RESIZE_GRIP
             * hot corner starts a resize (opt-in, <window class="user-
             * resizable">). Checked before the title drag-start below. */
            if (g_user_resizable && !window_is_dock() && ev->xbutton.button == 1 &&
                ev->xbutton.x >= g_win_w - KH_RESIZE_GRIP && ev->xbutton.x < g_win_w &&
                ev->xbutton.y >= g_win_h - KH_RESIZE_GRIP && ev->xbutton.y < g_win_h) {
                g_win_resizing = 1;
                g_resize_start_xr = ev->xbutton.x_root;
                g_resize_start_yr = ev->xbutton.y_root;
                g_resize_start_w = g_win_w;
                g_resize_start_h = g_win_h;
                return;
            }
            /* REAL, NEW 2026-08-29 (TASK 1: popup drag support) - check for
             * drag-start on chrome area (y < CHROME_H), same pattern as
             * db-hq/events-hq/chat-hai. Button 1 only, top CHROME_H pixels.
             *
             * REAL FIX 2026-08-29 (live report: "why isn't x quit button
             * working for settings anymore?") - this window's own close
             * button lives INSIDE that same top strip (dbhq_layout_pass's
             * is_close block: x = g_win_w-60..g_win_w, y = 0..CHROME_H).
             * Without an exclusion this unconditionally ate every click
             * there as a drag-start before kh_capture_click() ever got a
             * chance to hit-test the close element - exactly db-hq/events-
             * hq's own already-solved problem (see their g_dbhq_close_elem/
             * g_evhq_close_elem exclusion just below), never ported here
             * since this popup path has no such named close-element global
             * to check against; excluded the same top-right 60px rect by
             * its own known real coordinates instead. */
            /* REAL FIX 2026-09-01 (live report: "! chrome doesn't work
             * via mouse click, only nav/Enter") - the exclusion zone
             * below used to be a hardcoded 60px, sized when "X" was the
             * ONLY chrome button on this popup path (2026-08-29). Once
             * "!" and "_" were added to its LEFT this same session,
             * most of their real clickable area fell outside that
             * fixed 60px, so a click there was eaten as a drag-start
             * before kh_capture_click() ever ran - keyboard Enter on
             * the focused nav item bypassed this entirely, which is
             * why it "worked from nav but not mouse". Real fix: use
             * the chrome trio's own real leftmost x (g_default_full_
             * screen_elem, already computed by layout_sidebar_panel())
             * as the exclusion boundary instead of a stale constant -
             * only when this window actually has real chrome
             * (g_default_has_sidebar_panel), else keep the original
             * 60px for any other popup that has just a plain close
             * corner and no chrome trio of its own. */
            /* 2026-09-09: the has_canvas / flat-toolbar layout (pchq
             * board) has its own template chrome trio, not the
             * g_default_*_elem synth ones - use its captured leftmost x
             * so a mouse click on "!" / "_" hit-tests instead of being
             * swallowed as a window drag. */
            int chrome_zone_x = (g_default_minimize_elem->w > 0)
                                 ? g_default_minimize_elem->x
                                 : ((g_default_fullscreen_elem->w > 0)
                                    ? g_default_fullscreen_elem->x
                                    : ((g_default_close_elem->w > 0)
                                       ? g_default_close_elem->x
                                       : (g_canvas_chrome_left_x ? g_canvas_chrome_left_x - 4
                                                                 : g_win_w - 60)));
            if (!window_is_dock() && ev->xbutton.button == 1 && ev->xbutton.y >= KH_WIN_FRAME && ev->xbutton.y < CHROME_H + KH_WIN_FRAME &&
                !(ev->xbutton.x >= chrome_zone_x && ev->xbutton.x < g_win_w)) {
                g_popup_dragging = 1;
                g_popup_drag_last_x = ev->xbutton.x_root;
                g_popup_drag_last_y = ev->xbutton.y_root;
                return;
            }
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            long ms_since_map = (now.tv_sec - g_map_time.tv_sec) * 1000L
                               + (now.tv_nsec - g_map_time.tv_nsec) / 1000000L;
            if (!window_is_dock() && ms_since_map < PHANTOM_CLICK_GUARD_MS) return;
            if (window_is_dock()) {
                if (ev->xbutton.button != 3 && ev->xbutton.button != 4 && ev->xbutton.button != 5)
                    popup_handle_click(ev->xbutton.x, ev->xbutton.y);
                if (!g_quit) redraw();
                return;
            }
            /* REAL FIX 2026-09-01 (live report: "backspace didn't work
             * for me" - clicked a row, then Backspace never reached
             * the window) - same real focus-follows-mouse root cause
             * already diagnosed for the generic <cli_io> composer
             * (xperiments/khtpm-generic-dispatch-design.md's own
             * writeup), but this is the safer, non-exclusive half of
             * that fix: an XGrabKeyboard held for this window's WHOLE
             * life (tried first, reverted - would make the rest of the
             * desktop keyboard-dead while this window is merely open,
             * a much worse regression than the bug it fixes) is wrong
             * here. A plain re-assertion of real X input focus on every
             * real click is the normal, expected "click to focus"
             * behavior any desktop app has - if the human's mouse is
             * still near/over the row they just clicked (the common
             * case for a single Backspace right after), this alone
             * closes the gap without grabbing anything exclusively. */
            /* REAL, NEW 2026-09-14, direct live report across several
             * turns - real right-click context menu, "just like entity
             * context menus... for the window (or subwindow)": a real
             * right-click ANYWHERE in the window opens THIS window's
             * own CTXMENU-driven menu (kh_open_cli_io_context_menu()'s
             * own header comment) if its manager ever wrote any real
             * CTXMENU rows to meta.pdl - a real, deliberate no-op
             * otherwise (never a house-wide assumed default). If the
             * click happens to land on a cli_io/text_area, that element
             * becomes the real target (so a CUT/COPY/PASTE row, if the
             * app's own meta.pdl declares one, has real buffer/
             * selection state to act on) - target is NULL for a
             * right-click on plain window space, which is fine for any
             * OTHER app-declared action (dispatch()-routed, no target
             * Elem needed) and simply means CUT/COPY/PASTE would be a
             * no-op if chosen there. poll_agent_history()'s own
             * MOUSE_EVENT dispatch deliberately never routes button 3
             * to popup_handle_click() (that's a left-click activation
             * path) - real right-click hit-testing lives here instead. */
            if (ev->xbutton.button == 3) {
                kh_open_cli_io_context_menu(kh_ctx_hit(ev->xbutton.x, ev->xbutton.y),
                                            ev->xbutton.x, ev->xbutton.y);
                return;
            }
            XSetInputFocus(dpy, win, RevertToParent, CurrentTime);
            kh_capture_click(ev->xbutton.x, ev->xbutton.y, (int)ev->xbutton.button);
            poll_agent_history();
            if (!g_quit) redraw();
        }
        return;
    }
    if (ev->type == ButtonRelease && ev->xbutton.button == 1) {
        g_popup_dragging = 0;  /* REAL, NEW 2026-08-29 (TASK 1) */
        g_text_drag_elem = NULL; /* REAL, NEW 2026-09-14 - end any real text drag-select */
        if (g_win_resizing) {
            /* commit: ONE relayout + redraw now that the drag is done.
             * Doing it per-MotionNotify feeds back through the
             * has_canvas/toolbar layout (which only ever grows g_win_w,
             * plus a per-pass +2*KH_WIN_FRAME) and the window grows
             * without bound while you drag - the "infinite grow" bug. */
            g_win_resizing = 0;
            if (!g_quit) { assign_nav_and_layout(); redraw(); }
        }
        return;
    }
    if (ev->type == MotionNotify) {
        /* REAL, NEW 2026-09-14, direct live report ("shift arrow
         * working now. but not mouse drag. can u fix that?") - real
         * drag-select: while button 1 is genuinely still held (checked
         * via the event's own state, not just "we started a drag" -
         * the button could have been released outside this window and
         * we'd miss the ButtonRelease) and a text field is the live
         * drag target, extend [sel_anchor,cursor) to the current mouse
         * position every motion tick. Coalesced the same way the
         * resize-drag branch just below already does - only the final
         * position in a motion burst matters for where the cursor ends
         * up. Checked BEFORE g_win_resizing/g_popup_dragging so a text
         * drag can never be shadowed by an unrelated window-level drag
         * state left set from something else. */
        if (g_text_drag_elem && (ev->xmotion.state & Button1Mask)) {
            XEvent mdrain;
            while (XCheckTypedWindowEvent(dpy, win, MotionNotify, &mdrain)) *ev = mdrain;
            if (g_default_input_elem == g_text_drag_elem) {
                g_text_drag_elem->cursor = kh_input_offset_at_click(g_text_drag_elem, ev->xmotion.x, ev->xmotion.y);
                if (!g_quit) redraw();
            } else {
                g_text_drag_elem = NULL; /* disarmed out from under the drag - stop cleanly */
            }
            return;
        }
        if (g_text_drag_elem && !(ev->xmotion.state & Button1Mask)) g_text_drag_elem = NULL; /* missed the real ButtonRelease - stop here instead */
        if (g_win_resizing) {   /* any user-resizable window, not just popups */
            /* coalesce the motion burst - only the final position matters */
            XEvent mdrain;
            while (XCheckTypedWindowEvent(dpy, win, MotionNotify, &mdrain)) *ev = mdrain;
            int scr = DefaultScreen(dpy);
            int maxw = DisplayWidth(dpy, scr), maxh = DisplayHeight(dpy, scr);
            int nw = g_resize_start_w + (ev->xmotion.x_root - g_resize_start_xr);
            int nh = g_resize_start_h + (ev->xmotion.y_root - g_resize_start_yr);
            if (nw < KH_WIN_MIN_W) nw = KH_WIN_MIN_W;
            if (nh < KH_WIN_MIN_H) nh = KH_WIN_MIN_H;
            if (nw > maxw) nw = maxw;
            if (nh > maxh) nh = maxh;
            if (nw != g_win_w || nh != g_win_h) {
                g_win_w = nw; g_win_h = nh;
                if (g_window) { g_window->w = g_win_w; g_window->h = g_win_h; }
                /* X window only + cheap same-buffer repaint. NO
                 * assign_nav_and_layout() here - see the ButtonRelease
                 * comment. The Pixmap may be smaller than the new size
                 * for a beat; redraw() on release rebuilds it. */
                XResizeWindow(dpy, win, (unsigned)g_win_w, (unsigned)g_win_h);
            }
            return;
        }
        if (is_popup && g_popup_dragging) {
            /* REAL, NEW 2026-08-29 (TASK 1: popup drag-move) - same pattern
             * as other modes: compute delta from last recorded x_root/y_root,
             * update g_win_x/g_win_y, call XMoveWindow, clamp to WM_MANAGED_
             * DRAG_MIN_Y to avoid overlap with taskbar header. */
            int dx = ev->xmotion.x_root - g_popup_drag_last_x;
            int dy = ev->xmotion.y_root - g_popup_drag_last_y;
            g_win_x += dx; g_win_y += dy;
            if (g_win_y < g_win_top_y) g_win_y = g_win_top_y;
            XMoveWindow(dpy, win, g_win_x, g_win_y);
            g_popup_drag_last_x = ev->xmotion.x_root;
            g_popup_drag_last_y = ev->xmotion.y_root;
        }
        return;
    }
    if (ev->type == KeyPress) {
        char buf8[8]; KeySym ks;
        int n = XLookupString(&ev->xkey, buf8, sizeof(buf8) - 1, &ks, NULL);
        buf8[n > 0 ? n : 0] = '\0';
        const char *kname = XKeysymToString(ks);
        /* REAL, NEW 2026-09-05 - real Shift state for this key, read by
         * default_cli_io_handle_key()'s selection logic. */
        g_key_shift = (ev->xkey.state & ShiftMask) ? 1 : 0;
        g_key_ctrl  = (ev->xkey.state & ControlMask) ? 1 : 0;
        if (is_popup) {
            /* Physical keys must move dock/popup nav in-process.
             * Capture+poll is for agent replay; idle tick still consumes
             * KEY_PRESSED from the history file. Pin the cursor past this
             * capture so the same key is not dispatched twice. */
            handle_key(ks, buf8[0]);
            kh_capture_key(ks, buf8[0]);
            {
                char hp[PATH_BUF];
                struct stat st;
                history_path(hp, sizeof(hp));
                if (stat(hp, &st) == 0) g_history_cursor = st.st_size;
            }
            if (!g_quit) redraw();
        }
        return;
    }
    if (ev->type == FocusIn || ev->type == FocusOut) {
        /* FLICKER FIX 2026-09-03 - the new "^"/"." title indicator wired an
         * unconditional redraw() onto every FocusIn/FocusOut. But a focused
         * X11 window gets a FocusOut(NotifyGrab)+FocusIn(NotifyUngrab) pair
         * EVERY time any process takes a pointer/keyboard grab anywhere on
         * the desktop (passive button grabs, the taskbar's rmmv root grab,
         * entity drags, menus, alt-tab...) - none of which change what this
         * window shows. That was two full XGetImage+XPutImage blits per
         * grab = the "redraws with no change" flicker, worst on db-hq-pal
         * because it's the window that holds real focus while the user
         * mouses around triggering grabs. Ignore grab-synthetic and
         * pointer-derived notifications (the dock branch below already did
         * this for its own case), and only repaint when the actual
         * focus-owned state flips. */
        if (ev->xfocus.mode == NotifyGrab || ev->xfocus.mode == NotifyUngrab ||
            ev->xfocus.mode == NotifyWhileGrabbed ||
            ev->xfocus.detail == NotifyPointer || ev->xfocus.detail == NotifyPointerRoot ||
            ev->xfocus.detail == NotifyInferior) {
            return;
        }
    }
    if (ev->type == FocusIn) {
        {
            /* REAL FIX 2026-09-03 (direct live report/question: "is
             * there a way to tell if another window gets focus and
             * remove focus from both? should be the same if another
             * x11-hq window gets focus") - default/popup mode
             * (chat-hai/open-hai/co-lab-hai/entity-menu alike) already
             * requests FocusChangeMask (see this window's own
             * XCreateWindow event_mask) and this handler already existed
             * for db-hq/events-hq - it simply had no default-mode branch
             * at all, so a real FocusIn/FocusOut here was silently
             * swallowed (both blocks `return` unconditionally). This is
             * what makes the new "^"/"." title-bar indicator live and
             * immediate the instant real focus changes for ANY reason -
             * another x11-hq window, an unrelated app, alt-tab - not
             * just whenever this window happens to redraw for some
             * other reason anyway. */
            if (g_focus_owned_painted != 1) redraw();
        }
        g_x11_window_focused = 1;
        g_interact_disengage_sent = 0;
        return;
    }
    if (ev->type == FocusOut) {
        if (window_is_dock() &&
            ev->xfocus.mode != NotifyGrab && ev->xfocus.mode != NotifyUngrab)
            dock_release_keyboard_if_left();
        if (g_win_managed_focus) {
            g_x11_window_focused = 0;
            kh_interact_disengage_engine_if_on();
        }
        if (g_focus_owned_painted != 0) redraw();
        return;
    }
    /* REAL, NEW 2026-09-05 (CLIPBOARD-COPY-PASTE-DESIGN.md) - real X11
     * CLIPBOARD selection protocol. SelectionRequest: another client
     * (another khtpm window, a terminal, a browser) asked us for the
     * clipboard content we own - answer with g_clipboard_text.
     * SelectionClear: we lost ownership (someone else copied) - nothing
     * to do, the new owner answers from here on. SelectionNotify (with
     * g_paste_pending, NOT the XDND g_xdnd_awaiting path below): a paste
     * we requested has arrived - read the property, insert at the armed
     * field's cursor. These three event types are delivered regardless
     * of this window's event mask, so no XSelectInput change is needed. */
    if (ev->type == SelectionRequest) {
        XSelectionRequestEvent *rq = &ev->xselectionrequest;
        XSelectionEvent nt;
        nt.type = SelectionNotify; nt.display = rq->display; nt.requestor = rq->requestor;
        nt.selection = rq->selection; nt.target = rq->target; nt.time = rq->time;
        nt.property = None;
        if (!g_atom_targets) kh_clipboard_init_atoms();
        if (rq->target == g_atom_targets) {
            Atom offer[] = { g_atom_targets, g_atom_utf8, XA_STRING };
            XChangeProperty(dpy, rq->requestor, rq->property, XA_ATOM, 32,
                            PropModeReplace, (unsigned char *)offer, 3);
            nt.property = rq->property;
        } else if (rq->target == g_atom_utf8 || rq->target == XA_STRING) {
            XChangeProperty(dpy, rq->requestor, rq->property, rq->target, 8,
                            PropModeReplace, (unsigned char *)g_clipboard_text,
                            (int)strlen(g_clipboard_text));
            nt.property = rq->property;
        }
        XSendEvent(dpy, rq->requestor, True, NoEventMask, (XEvent *)&nt);
        XFlush(dpy);
        return;
    }
    if (ev->type == SelectionClear) {
        return; /* lost CLIPBOARD/PRIMARY ownership - real, expected, nothing to do */
    }
    if (ev->type == SelectionNotify && g_paste_pending) {
        g_paste_pending = 0;
        XSelectionEvent *se = &ev->xselection;
        if (se->property != None && g_default_input_elem) {
            Atom got_type; int got_fmt; unsigned long nitems, after; unsigned char *data = NULL;
            if (XGetWindowProperty(dpy, win, g_atom_paste_prop, 0, (long)(sizeof(g_clipboard_text) / 4),
                                   True, AnyPropertyType, &got_type, &got_fmt,
                                   &nitems, &after, &data) == Success && data) {
                kh_clipboard_insert_text(g_default_input_elem, (const char *)data);
                XFree(data);
                if (!g_quit) redraw();
            }
        }
        return;
    }
    /* XDND was popup-only because is_popup returned before HQ handlers.
     * Same drop_action contract for every mode that declared it. */
    if (ev->type == SelectionNotify && g_xdnd_awaiting) {
        g_xdnd_awaiting = 0;
        xdnd_handle_selection(dpy, win);
        if (!g_quit) redraw();
        return;
    }
    if (ev->type == ClientMessage && g_drop_action[0] &&
        (Atom)ev->xclient.message_type == ga_xdnd_enter) {
        g_xdnd_source = (Window)ev->xclient.data.l[0];
        if (!g_drop_highlight) { g_drop_highlight = 1; if (!g_quit) redraw(); }
        return;
    }
    if (ev->type == ClientMessage && g_drop_action[0] &&
        (Atom)ev->xclient.message_type == ga_xdnd_position &&
        g_xdnd_source != None) {
        XEvent st;
        memset(&st, 0, sizeof(st));
        st.xclient.type = ClientMessage;
        st.xclient.window = g_xdnd_source;
        st.xclient.message_type = ga_xdnd_status;
        st.xclient.format = 32;
        st.xclient.data.l[0] = (long)win;
        st.xclient.data.l[1] = 1;
        st.xclient.data.l[2] = 0;
        st.xclient.data.l[3] = (long)ga_xdnd_action_copy;
        st.xclient.data.l[4] = (long)ga_xdnd_action_copy;
        XSendEvent(dpy, g_xdnd_source, False, NoEventMask, &st);
        return;
    }
    if (ev->type == ClientMessage && g_drop_action[0] &&
        (Atom)ev->xclient.message_type == ga_xdnd_leave) {
        g_xdnd_source = None;
        if (g_drop_highlight) { g_drop_highlight = 0; if (!g_quit) redraw(); }
        return;
    }
    if (ev->type == ClientMessage && g_drop_action[0] &&
        (Atom)ev->xclient.message_type == ga_xdnd_drop &&
        g_xdnd_source != None) {
        XConvertSelection(dpy, ga_xdnd_selection, ga_uri_list,
                          ga_uri_list, win, (Time)ev->xclient.data.l[2]);
        g_xdnd_awaiting = 1;
    }
}


static void hq_run_event_loop(Atom wm_delete, int is_popup) {
    /* headless snapshot: --dump-and-exit on any mode. Paint one real
     * frame (twice, with a beat between - same "opacity/first-paint
     * settles on the 2nd" reasoning the db-hq path already relies on),
     * write the PNG + receipt via dump_frame_png(), then quit before
     * the loop proper. Covers every hq_run_event_loop() caller from
     * one place. */
    if (g_dump_and_exit) {
        if (!g_dumped) {
            g_dumped = 1;
            redraw();
            XFlush(dpy);
            usleep(250000);
            /* a <module> projector just started - give it a beat to
             * write its first state file, then pick it up before the
             * snapshot (the event loop that normally does this is
             * skipped in dump mode). */
            reparse_chtpm_if_changed();
            redraw();
            XFlush(dpy);
            dump_frame_png();
        }
        return;
    }
    while (!g_quit) {
        /* REAL, NEW 2026-09-03 - honor the SIGTERM/SIGINT hook installed
         * in main(): the select() below wakes on signal or every
         * ~150ms, so this breaks the loop promptly and falls through to
         * the shared cleanup path (atexit's cleanup_hq_window_registry,
         * nav_tab_unregister, history_unregister) which the default/
         * HQ-window modes previously skipped entirely on kill -TERM. */
        if (g_shutdown_requested) { g_quit = 1; break; }
        /* REAL FIX 2026-09-15 (bug_bounty.md, "mouse click jumps to next
         * nav" - root cause traced by direct read, not guessed: dock
         * mode numbers bottom-bar items RELATIVELY, off
         * g_dock_header_nav_hi (assign_nav_and_layout(), ~line 5892) -
         * every bottom Elem's nav_index is header_count + position, not
         * an absolute id. hq_idle_tick() (below) is what can trigger a
         * reparse/relayout that recomputes that header count (a vars
         * change - the clock label, username, any n_tabs-driven count).
         * This loop used to run hq_idle_tick() BEFORE draining XPending,
         * so a real ButtonPress the X server had already queued (the
         * click landed on the numbering painted on screen) could be
         * dispatched AFTER a same-tick reparse silently shifted every
         * bottom nav_index out from under it - the click's pixel hit
         * was always correct, but the Elem it hit had already been
         * renumbered by the time click_focus_then_activate() read its
         * nav_index. Fix, once and for all, by construction rather than
         * by patching the race narrower: drain and fully dispatch every
         * event the server already delivered to us FIRST, every single
         * loop iteration, before hq_idle_tick() is allowed to touch
         * g_nav/g_dock_header_nav_hi at all. Any event already in hand
         * is now always evaluated against the numbering that was live
         * when it was queued - a reparse can still change numbering,
         * but only ever for the NEXT event, never retroactively for one
         * already in flight. */
        while (XPending(dpy)) {
            XEvent ev; XNextEvent(dpy, &ev);
            hq_dispatch_xevent(&ev, wm_delete, is_popup);
        }
        if (g_quit) break;
        hq_idle_tick();
        if (g_quit) break;
        fd_set fds; FD_ZERO(&fds);
        int xfd = ConnectionNumber(dpy); FD_SET(xfd, &fds);
        /* REAL FIX 2026-08-29, found live-testing tp_debug_click_
         * watcher.c (a standalone tool built to isolate this exact
         * problem): a 150ms poll tick can genuinely miss a real click
         * entirely - a synthetic XTest click's own button-down window
         * is only ~50ms, and a real human click can be shorter still,
         * so a 150ms sample interval has a real chance of landing
         * entirely between press and release. Only shortened while
         * g_pal_rmmv_armed (costs nothing otherwise - every other
         * window/mode never sets this flag at all). */
        /* Dock strip: tick at ~30Hz like a canvas, not 150ms. hq_idle_tick()
         * is where dock_poll_strip_state() notices the manager's
         * strip_state.txt republish and mirrors terminal-driven focus into
         * the ASCII frame - a 150ms tick there was the visible "cli" lag
         * the user reported. TPMOS's own reference renderer.c polls its
         * pulse marker at 60Hz (usleep(16667)); 33ms here is the same
         * marker/dirty idea, one cheap stat() per tick, no extra file. */
        struct timeval tv = (g_has_canvas || window_is_dock() || g_drop_highlight
                             || kh_is_drop_target_window())
                                ? (struct timeval){ 0, 16667 }
                                : (struct timeval){ 0, 150000 };
        select(xfd + 1, &fds, NULL, NULL, &tv);
        /* Events that arrive during THIS select() wait are deliberately
         * left queued - they'll be the very first thing drained at the
         * top of the NEXT iteration, before that iteration's own
         * hq_idle_tick(), so the same guarantee holds every cycle. */
        /* REAL, NEW 2026-08-29 - see dbhq_rmmv_poll_pointer's own
         * header comment. A real, non-event, XQueryPointer-based
         * fallback for real human mouse clicks, which a real Mutter/
         * XWayland bug never delivers as ButtonPress events to this
         * grabbing process. No-op (returns immediately) whenever not
         * armed, so this costs nothing on every other tick of every
         * other window's own event loop. */
        /* P-7 (pchq-vs-tpmos.md): a <canvas> window used to force
         * g_frame_dirty=1 EVERY tick -> ~33 redraw()s/s (re-read the
         * .raw + XPutImage) whether or not the framebuffer changed.
         * TPMOS's renderer.c only repaints on its pulse marker growing.
         * Marker-drive it: stat the live canvas_raw file and only repaint
         * when its size/mtime moved, plus a slow ~2Hz safety repaint
         * (late-appearing var, window resize, receipt swap). */
        if (g_has_canvas && !g_quit) {
            static off_t  s_last_sz = -1;
            static time_t s_last_mt = 0;
            static time_t s_last_force = 0;
            const char *cr = kh_get_var("canvas_raw");
            struct stat cst;
            if (cr && cr[0] && stat(cr, &cst) == 0) {
                if (cst.st_size != s_last_sz || cst.st_mtime != s_last_mt) {
                    s_last_sz = cst.st_size;
                    s_last_mt = cst.st_mtime;
                    g_frame_dirty = 1;
                }
            } else {
                g_frame_dirty = 1;  /* no file/var yet: keep painting the dark bootstrap */
            }
            time_t nowt = time(NULL);
            if (nowt - s_last_force >= 1) { s_last_force = nowt; g_frame_dirty = 1; }
        }
        if (g_frame_dirty && !g_quit) { g_frame_dirty = 0; redraw(); }
    }
}


/* REAL, NEW 2026-09-04, direct live request ("single click vs double
 * click. it was added to settings yet[?]") - real, generic toggle for
 * the same house-wide click_two_step key desktop_load_click_two_step()
 * reads, exposed via a reserved onclick verb (CLICK_TWOSTEP_TOGGLE,
 * same real shape as OPACITY_MINUS/PLUS above: read current value,
 * flip, write back, apply locally, redraw) so it's settable from a
 * real Settings UI item instead of hand-editing hq_ui.pdl. Multi-key
 * file (comments + emoji_sprite_view live in the same file) - can't
 * just overwrite the whole thing like the single-key zorder-mode
 * file; rewrites in place, replacing the one matching line (or
 * appending it if somehow absent), leaving every other line
 * untouched. Only takes effect for THIS process immediately
 * (g_click_two_step updated in place) and any future launch that
 * reads the file fresh - does NOT live-propagate to other already-
 * open windows (that would need the same respawn-all-processes
 * mechanism ktb_zorder_op.+x (ktb_zorder_op.c) uses for override_redirect;
 * deliberately not built here, scope kept to "settable at all"). */
/* REAL FIX 2026-09-10 (direct instruction: "diamond standard isn't
 * mtime, its fsize by appending to a marker file... can u find and do"
 * - see 02-architecture/reference/TPMOS-DIAMOND-render-chain.md).
 * hq_ui.pdl itself is rewritten in place, not appended (an equal-length
 * rewrite is invisible to a size cursor on the FILE ITSELF - the exact
 * reason a past pass here used mtime instead) - the real DIAMOND fix
 * isn't "watch the content file's own size," it's "every writer also
 * appends one byte to a dedicated, real append-only marker," same
 * shape as TPMOS's own frame_changed.txt. Called by all three real
 * writers below (desktop_toggle_click_two_step/desktop_set_font_scale/
 * desktop_set_font_family) right after they rewrite hq_ui.pdl. */
static void hq_ui_pdl_touch_marker(const char *house_root) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/hq_ui_pdl_changed.txt", house_root);
    FILE *f = fopen(path, "a");
    if (f) { fputc('K', f); fclose(f); }
}

static void desktop_toggle_click_two_step(const char *house_root) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/hq_ui.pdl", house_root);
    char lines[128][256];
    int n = 0;
    FILE *f = fopen(path, "r");
    if (f) {
        while (n < 128 && fgets(lines[n], sizeof(lines[n]), f)) n++;
        fclose(f);
    }
    int new_val = !g_click_two_step;
    int replaced = 0;
    for (int i = 0; i < n; i++) {
        if (strncmp(lines[i], "click_two_step=", 15) == 0) {
            snprintf(lines[i], sizeof(lines[i]), "click_two_step=%d\n", new_val);
            replaced = 1;
        }
    }
    FILE *wf = fopen(path, "w");
    if (!wf) return;
    for (int i = 0; i < n; i++) fputs(lines[i], wf);
    if (!replaced) fprintf(wf, "click_two_step=%d\n", new_val);
    fclose(wf);
    g_click_two_step = new_val;
    hq_ui_pdl_touch_marker(house_root);
}

/* Rewrite hq_ui.pdl's font_scale row in place (same shape as
 * desktop_toggle_click_two_step). pct is 75..200; stored as a decimal
 * multiplier. Updates g_ui_scale_pct in this process; other open
 * windows pick it up via hq_ui_pdl_reload_if_changed()'s mtime check. */
static void desktop_set_font_scale(const char *house_root, int pct) {
    if (pct < 75) pct = 75;
    if (pct > 200) pct = 200;
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/hq_ui.pdl", house_root);
    char lines[128][256];
    int n = 0;
    FILE *f = fopen(path, "r");
    if (f) { while (n < 128 && fgets(lines[n], sizeof(lines[n]), f)) n++; fclose(f); }
    int replaced = 0;
    for (int i = 0; i < n; i++) {
        if (strncmp(lines[i], "font_scale=", 11) == 0) {
            snprintf(lines[i], sizeof(lines[i]), "font_scale=%.2f\n", pct / 100.0);
            replaced = 1;
        }
    }
    FILE *wf = fopen(path, "w");
    if (!wf) return;
    for (int i = 0; i < n; i++) fputs(lines[i], wf);
    if (!replaced) fprintf(wf, "font_scale=%.2f\n", pct / 100.0);
    fclose(wf);
    g_ui_user_pct = pct;
    kh_ui_apply_scale();
    hq_ui_pdl_touch_marker(house_root);
}

static void desktop_set_font_family(const char *house_root, const char *name) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/hq_ui.pdl", house_root);
    char lines[128][256];
    int n = 0;
    FILE *f = fopen(path, "r");
    if (f) { while (n < 128 && fgets(lines[n], sizeof(lines[n]), f)) n++; fclose(f); }
    int replaced = 0;
    for (int i = 0; i < n; i++) {
        if (strncmp(lines[i], "font_family=", 12) == 0) {
            snprintf(lines[i], sizeof(lines[i]), "font_family=%s\n", name);
            replaced = 1;
        }
    }
    FILE *wf = fopen(path, "w");
    if (!wf) return;
    for (int i = 0; i < n; i++) fputs(lines[i], wf);
    if (!replaced) fprintf(wf, "font_family=%s\n", name);
    fclose(wf);
    snprintf(g_ui_font_family, sizeof(g_ui_font_family), "%s", name);
    hq_ui_pdl_touch_marker(house_root);
}


/* Reopen font_ui at the current UI scale. font_ui is the shared chrome/
 * title/dock/tab font (drawn directly, not via font_for()), loaded once
 * in main(); this lets a live scale change re-size it too. Safe to call
 * whenever dpy/screen are valid. */
static void reload_font_ui(void) {
    if (!dpy) return;
    XftFont *old = font_ui;
    char spec[64];
    snprintf(spec, sizeof(spec), "Noto Sans CJK SC:pixelsize=%d", scaled(13));
    XftFont *nf = XftFontOpenName(dpy, screen, spec);
    if (!nf) {
        snprintf(spec, sizeof(spec), "DejaVu Sans:pixelsize=%d", scaled(12));
        nf = XftFontOpenName(dpy, screen, spec);
    }
    if (nf) { font_ui = nf; if (old && old != nf) XftFontClose(dpy, old); }
}

/* HQ-only hq_ui.pdl keys, handed to kh_ui_pdl_load() as its `extra` hook. */
static int hq_ui_pdl_extra(const char *line, const char *val) {
    if (0) {}
        /* 2026-09-11, CHTPM-INCREMENTAL-REPARSE-DESIGN.md - runtime
         * toggle for the incremental reparse path, OFF (0) unless this
         * key is present and non-zero. A real PDL key, not a
         * recompile, so the design doc's mandatory dock-first rollout
         * order can be tested window by window (this house-wide
         * setting is read by every khtpm_core_render.c process, same
         * as click_two_step) without rebuilding between steps. */
    else if (strcmp(line, "incremental_reparse") == 0) g_use_incremental_reparse = atoi(val) != 0;

        else if (strcmp(line, "close_combo") == 0) {
            snprintf(g_close_combo, sizeof(g_close_combo), "%s", val);
            for (char *p = g_close_combo; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
        }

        else if (strcmp(line, "win_top_y") == 0) {
            g_win_top_y = atoi(val);
            if (g_win_top_y < 0) g_win_top_y = 0;
            if (g_win_top_y > 400) g_win_top_y = 400;
        }
    else return 0;
    return 1;
}

static void desktop_load_click_two_step(const char *house_root) {
    kh_ui_pdl_load(house_root, hq_ui_pdl_extra);
}

static void handle_shutdown_signal(int sig) {
    (void)sig;
    g_shutdown_requested = 1;
}

/* ---------- REAL, NEW 2026-09-14, direct live report across several
 * turns ("what about a context menu for right click with cut/copy/
 * paste/ (nav ready?)" -> "read from .pdl like others" -> "each app
 * may have its own uses for right click, so we shouldn't assume menu
 * entries or opps" -> "can u do it? just like entity context menus?
 * ...can be handled by manager to an extent" -> "it still looks and
 * functions different than entity menus. it should function exactly
 * the same. (the navs, the ./^ focus visual, the entity name/pid etc.
 * why are u using different thing for window context menu. they are
 * both basically entities get it? a window is an entity, and vice
 * versa thats how u should think of it, get it?") - FINAL real design:
 * this is NOT a hand-rolled in-process popup. It writes a real
 * menu.chtpm (the exact same format cursword's own real, converted
 * menu.chtpm already uses - <window class="entity-menu"><page
 * name="main"><item label=... action=.../>...</page></window>) and
 * launches it via launch_khtpm_menu() VERBATIM - the exact same real,
 * separate-process, theme/CSS-rendered mechanism every entity's own
 * right-click menu already uses. Real nav numbering, real "[^]"/"[>]"
 * focus prefix, real header row - all for free, because this genuinely
 * IS the same mechanism, not a lookalike built to resemble it.
 *
 * CUT/COPY/PASTE are the only built-in verbs, and they need real,
 * live, in-process access to THIS window's own armed-field buffer/
 * selection - which the forked popup process has no way to reach
 * directly. Bridged the same real way this house already bridges any
 * cross-process action: the generated item's action= is a real shell
 * command (confirmed via dispatch()'s own generic fallthrough, which
 * runs any unrecognized action as a real `sh -c` shell command with
 * package_dir as $0/house_root as $1, then closes the popup - exactly
 * cursword's own menu.chtpm convention) that writes the chosen verb
 * into a real, tiny action file
 * (<package_dir>/.hq_manager/cli_io_ctxmenu_action.txt) - THIS process
 * polls that file (kh_poll_cli_io_ctxmenu_action(), called from
 * hq_idle_tick()) and runs the real action against g_cliio_ctx_target
 * once it appears. Any OTHER, custom CTXMENU-declared action (a
 * manager's own real shell command/house verb) is NOT bridged at all -
 * it's written straight into the generated item's own action=, so it
 * runs directly in the popup process, exactly like any real entity
 * menu item already does - no polling, no bridge needed for those. */
static Elem *g_cliio_ctx_target = NULL;

static void kh_cli_io_ctx_action_file(char *out, size_t outsz) {
    snprintf(out, outsz, "%s/.hq_manager/cli_io_ctxmenu_action.txt", g_package_dir);
}

/* Real `CTXMENU | Label | action` rows from THIS window's own
 * `<package_dir>/meta.pdl` - same file, same pipe-delimited shape,
 * same parse loop as load_methods()'s own real `METHOD` rows above, a
 * different row keyword so the two lists don't collide in one shared
 * file. Zero rows (file missing, or an app that simply never writes
 * any) -> n=0, real, deliberate "use the sane built-in default"
 * fallback in kh_open_cli_io_context_menu() below - never a house-wide
 * assumed default when a real CTXMENU row DOES exist, but also never a
 * blank menu when none do, matching load_methods()'s own real caller
 * convention (zero real METHOD rows still gets a real "Close" - see
 * that pattern's own comment a few hundred lines up). */
static int kh_load_cli_io_context_menu(MethodItem *items, int max) {
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/meta.pdl", g_package_dir);
    FILE *f = pdl_open(path);
    if (!f) return 0;
    char line[TP_PATH_BUF];
    int n = 0;
    while (n < max && fgets(line, sizeof(line), f)) {
        if (strncmp(line, "CTXMENU", 7) != 0) continue;
        char *p = strchr(line, '|');
        if (!p) continue;
        p++;
        while (*p == ' ') p++;
        char *end = strchr(p, '|');
        if (!end) continue;
        char *label_end = end;
        while (label_end > p && label_end[-1] == ' ') label_end--;
        size_t llen = (size_t)(label_end - p);
        if (llen == 0 || llen >= sizeof(items[0].label)) continue;
        memcpy(items[n].label, p, llen);
        items[n].label[llen] = '\0';

        char *a = end + 1;
        while (*a == ' ') a++;
        char *a_end = a + strcspn(a, "\r\n");
        while (a_end > a && a_end[-1] == ' ') a_end--;
        size_t alen = (size_t)(a_end - a);
        if (alen == 0 || alen >= sizeof(items[0].action)) continue;
        memcpy(items[n].action, a, alen);
        items[n].action[alen] = '\0';
        n++;
    }
    fclose(f);
    return n;
}

/* REAL, NEW 2026-09-21 (item-2 vertical slice: a robot/puzzle-piece
 * entity dropped into another entity's Inventory needs its own event
 * to act on the HOST, not on itself). A robot living in someone's
 * Inventory always has the shape <host_dir>/inventory/<robot_name> -
 * this house's own real convention (same one fe_place_on_desk.sh's
 * "pals = dirname(dirname(dirname(SRC)))" already uses for the mirror
 * case, moving OUT of inventory). Pure string math, no cross-process
 * state needed: if ent_dir's parent's basename is "inventory", its
 * grandparent is the host. Returns 1 and fills host_out if ent_dir is
 * inside an inventory; 0 (host_out untouched) otherwise - the normal
 * "this entity, not in anyone's inventory" case, unaffected. */
static int kh_inventory_host_dir(const char *ent_dir, char *host_out, size_t host_sz) {
    if (!ent_dir || !ent_dir[0]) return 0;
    char buf[TP_PATH_BUF];
    snprintf(buf, sizeof(buf), "%s", ent_dir);
    char *slash1 = strrchr(buf, '/');
    if (!slash1 || slash1 == buf) return 0;
    *slash1 = '\0'; /* buf = dirname(ent_dir) */
    char *slash2 = strrchr(buf, '/');
    if (!slash2) return 0;
    const char *parent_base = slash2 + 1; /* basename(dirname(ent_dir)) */
    if (strcmp(parent_base, "inventory") != 0) return 0;
    *slash2 = '\0'; /* buf = dirname(dirname(ent_dir)) = host */
    if (!buf[0]) return 0;
    snprintf(host_out, host_sz, "%s", buf);
    return 1;
}

static void kh_open_cli_io_context_menu(Elem *target, int win_px, int win_py) {
    if (!dpy) return;
    MethodItem items[12 + MAX_METHODS];
    char ent_dir[TP_PATH_BUF];
    int is_ent[12 + MAX_METHODS];
    ent_dir[0] = 0;
    memset(is_ent, 0, sizeof(is_ent));
    int n = kh_load_cli_io_context_menu(items, 12);
    if (n == 0) {
        int i = 0;
        int is_text = target && (strcmp(target->tag, "cli_io") == 0 || strcmp(target->tag, "text_area") == 0);
        if (target) {
            snprintf(items[i].label, sizeof(items[i].label), "Cut");   snprintf(items[i].action, sizeof(items[i].action), "CUT");   i++;
            snprintf(items[i].label, sizeof(items[i].label), "Copy");  snprintf(items[i].action, sizeof(items[i].action), "COPY");  i++;
            snprintf(items[i].label, sizeof(items[i].label), "Paste"); snprintf(items[i].action, sizeof(items[i].action), "PASTE"); i++;
            if (!is_text) {
                snprintf(items[i].label, sizeof(items[i].label), "Delete"); snprintf(items[i].action, sizeof(items[i].action), "DELETE"); i++;
                snprintf(items[i].label, sizeof(items[i].label), "Place");  snprintf(items[i].action, sizeof(items[i].action), "PLACE");  i++;
            }
        }
        snprintf(items[i].label, sizeof(items[i].label), "Cancel"); snprintf(items[i].action, sizeof(items[i].action), "void"); i++;
        n = i;
    }
    /* A tile that carries an entity dir in sprite= (a pal shown in an Inventory)
     * also offers that entity's own METHOD rows, inserted before the trailing
     * Cancel. Only real shell actions are offered; built-in keywords the desktop
     * handles in-process (CLOSE/void/OPEN_USER/CLI_IO ...) are skipped. */
    if (target && target->sprite[0] && n > 0) {
        char mp[TP_PATH_BUF];
        snprintf(mp, sizeof(mp), "%s/meta.pdl", target->sprite);
        if (access(mp, R_OK) == 0) {
            MethodItem em[MAX_METHODS];
            int en = load_methods(target->sprite, em, MAX_METHODS);
            int keep_cancel = (n > 0 && strcmp(items[n - 1].action, "void") == 0);
            int base = keep_cancel ? n - 1 : n;
            int w = base;
            for (int k = 0; k < en && w < 12 + MAX_METHODS - 1; k++) {
                const char *a = em[k].action;
                int keyword = 1;
                for (const char *c = a; *c; c++)
                    if (!((*c >= 'A' && *c <= 'Z') || *c == '_' || *c == ':')) { keyword = 0; break; }
                if (keyword || strcmp(a, "void") == 0 || strchr(em[k].label, '\'')) continue;
                items[w] = em[k];
                is_ent[w] = 1;
                w++;
            }
            if (keep_cancel) { snprintf(items[w].label, sizeof(items[w].label), "Cancel"); snprintf(items[w].action, sizeof(items[w].action), "void"); w++; }
            n = w;
            snprintf(ent_dir, sizeof(ent_dir), "%s", target->sprite);
        }
    }
    g_cliio_ctx_target = target;
    if (g_package_dir[0] && target) {
        char tp[TP_PATH_BUF];
        snprintf(tp, sizeof(tp), "%s/fe_ctx_target.txt", g_package_dir);
        FILE *tf = fopen(tp, "w");
        if (tf) {
            fprintf(tf, "id=%s\nlabel=%s\ntag=%s\n", target->id, target->label, target->tag);
            fclose(tf);
        }
    }

    /* real .hq_manager/ subdir, same convention this file's other
     * per-window state (ui.txt/cli_io_active.txt) already lives in. */
    char subdir[TP_PATH_BUF];
    snprintf(subdir, sizeof(subdir), "%s/.hq_manager", g_package_dir);
    mkdir(subdir, 0777);
    /* clear any stale action from a previous right-click before this
     * one's own popup can possibly write a fresh one. */
    char actfile[TP_PATH_BUF];
    kh_cli_io_ctx_action_file(actfile, sizeof(actfile));
    unlink(actfile);

    char menu_path[TP_PATH_BUF];
    snprintf(menu_path, sizeof(menu_path), "%s/menu.chtpm", subdir);
    FILE *cf = fopen(menu_path, "w");
    if (!cf) return;
    {
        char wlab[96];
        wlab[0] = 0;
        if (target && target->label[0]) {
            const char *s = target->label;
            while (*s && !isalnum((unsigned char)*s) && *s != '_' && *s != '.') s++;
            snprintf(wlab, sizeof(wlab), "%s", s[0] ? s : target->label);
            char *sp = strchr(wlab, ' ');
            if (sp) *sp = 0;
        }
        if (!wlab[0]) snprintf(wlab, sizeof(wlab), "item");
        fprintf(cf, "<window class=\"entity-menu\" label=\"%s\">\n  <page name=\"main\">\n", wlab);
    }
    for (int i = 0; i < n; i++) {
        const char *act = items[i].action;
        if (is_ent[i]) {
            char raw[TP_PATH_BUF * 2], esc[TP_PATH_BUF * 3];
            char host_dir[TP_PATH_BUF];
            /* If this item is a robot/entity sitting in someone else's
             * Inventory, export MUCHI_TARGET_ENT=<host> so an event this
             * entity's own METHOD row runs (e.g. the real "Play" ->
             * play_event.sh precedent) can act on the host instead of
             * the robot itself - see kh_inventory_host_dir()'s own
             * header comment and khtpm_events_hq_manager.c's matching
             * ENT="${MUCHI_TARGET_ENT:-$PWD}" opt-in override. */
            if (kh_inventory_host_dir(ent_dir, host_dir, sizeof(host_dir))) {
                snprintf(raw, sizeof(raw),
                         "MUCHI_TARGET_ENT='%s' sh '%s/&.widgits/file-explorer/ops/fe_entity_method.sh' '%s' '%s'",
                         host_dir, g_house_root, ent_dir, items[i].label);
            } else {
                snprintf(raw, sizeof(raw), "sh '%s/&.widgits/file-explorer/ops/fe_entity_method.sh' '%s' '%s'",
                         g_house_root, ent_dir, items[i].label);
            }
            size_t ew = 0;
            for (const char *r = raw; *r && ew + 8 < sizeof(esc); r++) {
                if (*r == '&') { memcpy(esc + ew, "&amp;", 5); ew += 5; }
                else if (*r == '"') { memcpy(esc + ew, "&quot;", 6); ew += 6; }
                else esc[ew++] = *r;
            }
            esc[ew] = '\0';
            fprintf(cf, "    <item label=\"%s\" action=\"%s\"/>\n", items[i].label, esc);
            continue;
        }
        if (strcmp(act, "CUT") == 0 || strcmp(act, "COPY") == 0 || strcmp(act, "PASTE") == 0 ||
            strcmp(act, "DELETE") == 0 || strcmp(act, "PLACE") == 0) {
            /* real, house-standard cross-process bridge (see this
             * block's own header comment). The popup is launched with the
             * <house_root> <chtpm_path> contract, so inside its items
             * "$0" is just "sh" - NOT package_dir - and a "$0/..." path
             * silently never reached this window. Emit the absolute
             * action-file path instead (& XML-escaped). */
            char eact[TP_PATH_BUF * 2];
            size_t ew = 0;
            for (const char *r = actfile; *r && ew + 6 < sizeof(eact); r++) {
                if (*r == '&') { memcpy(eact + ew, "&amp;", 5); ew += 5; }
                else eact[ew++] = *r;
            }
            eact[ew] = '\0';
            fprintf(cf, "    <item label=\"%s\" action=\"sh -c &apos;echo %s &gt; &quot;%s&quot;&apos;\"/>\n",
                    items[i].label, act, eact);
        } else if (strcmp(act, "CANCEL") == 0) {
            fprintf(cf, "    <item label=\"%s\" action=\"void\"/>\n", items[i].label);
        } else {
            /* a manager's own real, custom action - runs straight in
             * the popup process, exactly like any real entity menu
             * item, no bridge needed. */
            fprintf(cf, "    <item label=\"%s\" action=\"%s\"/>\n", items[i].label, act);
        }
    }
    fprintf(cf, "  </page>\n</window>\n");
    fclose(cf);

    /* launch_khtpm_menu() always reads "<pkg_dir>/menu.chtpm" - point
     * it at subdir (where the generated file above actually lives),
     * not g_package_dir's own top level (which might, in principle,
     * hold a real, unrelated menu.chtpm of its own some day). */
    snprintf(g_khtpm_menu_pkg_dir, sizeof(g_khtpm_menu_pkg_dir), "%s", subdir);
    snprintf(g_khtpm_menu_house_root, sizeof(g_khtpm_menu_house_root), "%s", g_house_root);
    int scr = DefaultScreen(dpy);
    Window child_ret; int rx = 0, ry = 0;
    XTranslateCoordinates(dpy, win, RootWindow(dpy, scr), win_px, win_py, &rx, &ry, &child_ret);
    launch_khtpm_menu(rx, ry);
}

/* Real CUT/COPY/PASTE execution against g_cliio_ctx_target - the ONLY
 * three verbs the popup process bridges back here (see this block's
 * own header comment); CANCEL/void and any custom action never reach
 * this function at all, they run straight in the popup process. */
static void kh_run_cli_io_context_action(const char *action) {
    Elem *e = g_cliio_ctx_target;
    int is_text = e && (strcmp(e->tag, "cli_io") == 0 || strcmp(e->tag, "text_area") == 0);
    if (is_text && (strcmp(action, "COPY") == 0 || strcmp(action, "CUT") == 0)) {
        int is_area = (strcmp(e->tag, "text_area") == 0);
        char *buf = is_area ? e->text_area_buffer : e->input_buffer;
        int lo, hi;
        if (kh_text_selection_range(e, buf, &lo, &hi)) {
            char tmp[4096];
            int nlen = hi - lo; if (nlen > (int)sizeof(tmp) - 1) nlen = (int)sizeof(tmp) - 1;
            memcpy(tmp, buf + lo, (size_t)nlen); tmp[nlen] = '\0';
            kh_clipboard_copy(tmp);
            if (strcmp(action, "CUT") == 0) kh_text_delete_selection(e, buf);
        }
    } else if (is_text && strcmp(action, "PASTE") == 0) {
        if (e) kh_set_default_input_elem(e);
        kh_clipboard_request_paste();
    } else if (is_text && strcmp(action, "DELETE") == 0) {
        int is_area = (strcmp(e->tag, "text_area") == 0);
        char *buf = is_area ? e->text_area_buffer : e->input_buffer;
        kh_text_delete_selection(e, buf);
    } else if (!is_text && g_package_dir[0]) {
        /* File-explorer (and any other non-text right-click): same
         * verbs, manager owns the FS. Piececraft CTX_* sibling. */
        char ap[TP_PATH_BUF];
        snprintf(ap, sizeof(ap), "%s/file_explorer_action.txt", g_package_dir);
        FILE *af = fopen(ap, "w");
        if (af) {
            fprintf(af, "seq=%u\ncmd=CTX_%s\n", ++g_swatch_action_seq, action);
            fclose(af);
        }
    }
    g_cliio_ctx_target = NULL;
    if (!g_quit) redraw();
}

/* Called from hq_idle_tick() every real tick - cheap (one stat/fopen
 * only when the file's own mtime moved, matching this file's own
 * marker-driven-not-per-frame discipline elsewhere) real poll for the
 * bridged CUT/COPY/PASTE verb the popup process (if any) wrote. */
static void kh_poll_cli_io_ctxmenu_action(void) {
    static time_t last_mtime = 0;
    if (!g_package_dir[0]) return;
    char path[TP_PATH_BUF];
    kh_cli_io_ctx_action_file(path, sizeof(path));
    struct stat st;
    if (stat(path, &st) != 0) { last_mtime = 0; return; }
    if (st.st_mtime == last_mtime) return;
    last_mtime = st.st_mtime;
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[64];
    int got = fgets(line, sizeof(line), f) != NULL;
    fclose(f);
    unlink(path); /* consumed - a stale re-read next tick must never re-fire the same action */
    if (!got) return;
    line[strcspn(line, "\r\n")] = '\0';
    if (line[0]) kh_run_cli_io_context_action(line);
}


/* REAL, NEW 2026-09-03 (HQ-WINDOW-TASKBAR-ENTRIES-AND-MINIMIZE-2026-09-
 * 03.md §2.1) - see main()'s own atexit() registration comment. */
static void cleanup_hq_window_registry(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/livedesk_hq_windows_%d.txt", g_house_root, (int)getpid());
    unlink(path);
}

/* --headless main loop. Entered from main() just before it would
 * XOpenDisplay(). g_window is parsed, CSS loaded, g_current_page set,
 * the per-PID relay + atexit hooks are in place. No X, ever. redraw()
 * itself is intercepted for headless (writes the text frame), so this
 * loop just drives the same change signals hq_idle_tick() does. */
static int headless_run(void) {
    fprintf(stderr, "[khtpm --headless] %s  pid %d\n",
            g_chtpm_path[0] ? g_chtpm_path : "(dock)", (int)getpid());
    g_win_x = 0; g_win_y = 0;
    g_win_w = window_is_dock() ? kh_screen_w() : 960;
    g_win_h = window_is_dock() ? 40 : 640;

    /* If this window has <module>s that publish its vars= file, give
     * them a beat to write it before the first layout. The windowed
     * path gets this for free from its slower event loop; without it a
     * headless run can race a missing -> fully-populated vars file into
     * its very first reparse and (with a large generated list) trip a
     * layout crash. Bounded ~1.5s; a window with no vars= file or a
     * fast module just falls straight through. */
    if (g_vars_path[0]) {
        for (int w = 0; w < 90; w++) {
            struct stat vst;
            if (stat(g_vars_path, &vst) == 0 && vst.st_size > 0) {
                unsigned long h1 = kh_files_hash(g_vars_path);
                usleep(16667);
                if (kh_files_hash(g_vars_path) == h1) break;   /* stable */
            }
            usleep(16667);
        }
        reparse_chtpm_if_changed();   /* pick the file up before the first paint */
    }

    redraw();   /* first frame (headless redraw() == layout + write text frame) */

    while (!g_quit && !g_shutdown_requested) {
        int dirty = 0;
        if (reparse_chtpm_if_changed()) dirty = 1;
        hq_ui_pdl_reload_if_changed(g_house_root);
        if (poll_agent_history() > 0) dirty = 1;
        if (dirty && !g_quit) redraw();
        usleep(16667);   /* 60Hz, DIAMOND cadence */
    }
    return 0;
}

/* REAL, NEW 2026-09-14, direct live report ("the bottom toolbar is
 * completely gone. i think it died again. we have to prevent this
 * bug"). The one real place the dock peer's (bottom tab bar's) own X
 * Window gets built - factored out of main()'s own startup code (which
 * now just calls this once) so the SAME real creation logic can also
 * be called defensively from the main loop's own per-tick self-check,
 * with zero risk of two independently-maintained copies drifting apart.
 * Idempotent/safe to call every tick: a real `XGetWindowAttributes`
 * liveness probe first - if the window already exists and the server
 * still knows about it, this is a cheap no-op single round-trip, not a
 * window recreated every tick. Only actually rebuilds
 * (GC/Pixmap/XftDraw included, matching the original startup code
 * exactly) when the window is genuinely missing or the server reports
 * it invalid (BadWindow), which is real evidence the window is gone,
 * not a guess. Requires `dpy`/`screen`/`cmap`/`g_dock_peer_path[0]` -
 * all already resolved by the time this can usefully run (main loop,
 * after startup), so no extra readiness checks needed here. */
static void kh_ensure_dock_peer_window(void) {
    if (!g_dock_peer_path[0]) return;   /* this window isn't a dock at all */
    if (!g_dock_peer) return;            /* peer data not parsed (yet) - nothing to show, nothing to build a window for */
    if (g_dock_peer_win) {
        XWindowAttributes wa;
        /* A BadWindow error here would otherwise reach the process's
         * own default X error handler (fatal by default) - main()
         * installs `kh_nonfatal_x_error` globally at the earliest
         * possible point specifically so every best-effort X call in
         * every mode survives exactly this class of stale-id error
         * instead of crashing the whole process - so a plain
         * XGetWindowAttributes call here is safe: it returns 0
         * (failure) and the already-installed non-fatal handler logs
         * and continues instead of aborting. */
        if (XGetWindowAttributes(dpy, g_dock_peer_win, &wa)) return; /* still real and alive - nothing to do */
        /* Window id is stale/invalid - fall through and rebuild for
         * real, same as "never created" below. */
        g_dock_peer_win = 0;
    }

    XSetWindowAttributes pswa;
    pswa.background_pixel = alloc_pixel(g_theme_bg);
    pswa.override_redirect = False;
    pswa.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | ButtonMotionMask | KeyPressMask | StructureNotifyMask | FocusChangeMask;
    g_dock_peer_win = XCreateWindow(dpy, RootWindow(dpy, screen),
        g_dock_peer_x, g_dock_peer_y,
        (unsigned)(g_dock_peer_w > 0 ? g_dock_peer_w : 64),
        (unsigned)(g_dock_peer_h > 0 ? g_dock_peer_h : DOCK_BAR_H),
        0, CopyFromParent, InputOutput, CopyFromParent,
        CWBackPixel | CWOverrideRedirect | CWEventMask, &pswa);
    apply_dock_window_hints(dpy, g_dock_peer_win, g_dock_peer_x, g_dock_peer_y);
    render_managed_wm_hints(dpy, g_dock_peer_win, 1);
    XMapRaised(dpy, g_dock_peer_win);
    set_window_opacity(dpy, g_dock_peer_win, load_theme_opacity());
    XMoveWindow(dpy, g_dock_peer_win, g_dock_peer_x, g_dock_peer_y);
    g_dock_peer_gc = XCreateGC(dpy, g_dock_peer_win, 0, NULL);
    g_dock_peer_buf_w = g_dock_peer_w > 0 ? g_dock_peer_w : 64;
    g_dock_peer_buf_h = g_dock_peer_h > 0 ? g_dock_peer_h : DOCK_BAR_H;
    if (g_dock_peer_buf) { XFreePixmap(dpy, g_dock_peer_buf); g_dock_peer_buf = 0; }
    if (g_dock_peer_xft) { XftDrawDestroy(g_dock_peer_xft); g_dock_peer_xft = NULL; }
    g_dock_peer_buf = XCreatePixmap(dpy, g_dock_peer_win,
        (unsigned)g_dock_peer_buf_w, (unsigned)g_dock_peer_buf_h,
        (unsigned)DefaultDepth(dpy, screen));
    g_dock_peer_xft = XftDrawCreate(dpy, g_dock_peer_buf, DefaultVisual(dpy, screen), cmap);
    kh_focus_debug_log("DOCK_PEER_WINDOW (re)created id=0x%lx", (unsigned long)g_dock_peer_win);
}

int main(int argc, char **argv) {
    /* Scan + strip the flag tokens so the positional parsing below sees
     * a clean <house_root> <chtpm_path> [x] [y] regardless of where the
     * flag was placed. */
    {
        int w = 1;
        for (int ai = 1; ai < argc; ai++) {
            if (strcmp(argv[ai], "--dump-and-exit") == 0) { g_dump_and_exit = 1; continue; }
            if (strcmp(argv[ai], "--headless") == 0)      { g_headless = 1;      continue; }
            argv[w++] = argv[ai];
        }
        argc = w;
        argv[argc] = NULL;
    }
    /* REAL, NEW 2026-09-01 - taskbar strip mode AND tile mode dispatch,
     * checked FIRST, before any of the shared .chtpm-parsing setup below
     * - NEITHER mode takes a .chtpm path at all, so both share the same
     * real argc==2 invocation shape, unlike every other mode which
     * requires at least a chtpm_path too. Disambiguated by a real
     * filesystem fact: argv[1] is always a genuine house_root for strip
     * mode (which always has its own real "#.desktop" directory), and
     * always a tile/pal's own package_dir for tile mode (which never
     * does) - confirmed via direct filesystem tests before this merge,
     * zero changes needed to any of this house's 11 real launcher
     * scripts. See strip_main()'s own big merged-block header comment
     * above, and tp_main()'s own (BEGIN TILE MODE), for the full
     * consolidation rationale (khtpm_strip_parser.c/.../
     * tp_desktop_window_rgb.c folded in verbatim, zero linking). */
    if (argc == 2) {
        fprintf(stderr,
                "khtpm_core_render: pal/tile process is khtpm_entity.+x <package_dir>\n"
                "usage: %s <house_root> <chtpm_path> [x] [y]\n", argv[0]);
        return 1;
    }
    /* REAL Stage 5 step 3/4 (2026-08-16, khtpm-merge-how2.md §5d.3) -
     * was <package_dir> <house_root> [x] [y] (house_root NOT first,
     * unlike every other khtpm app - a real, confirmed argv drift).
     * Now the real, unified <house_root> <chtpm_path> [x] [y] contract
     * - package_dir is ALWAYS dirname(chtpm_path) (every entity's own
     * package dir IS where its menu.chtpm lives), so it's derived
     * rather than passed separately - real, elegant simplification,
     * not just a reorder. */
    if (argc < 3) { fprintf(stderr, "usage: %s <house_root> <chtpm_path> [x] [y]\n", argv[0]); return 1; }
    snprintf(g_house_root, sizeof(g_house_root), "%s", argv[1]);
    /* REAL, NEW 2026-09-03 (HQ-WINDOW-TASKBAR-ENTRIES-AND-MINIMIZE-
     * 2026-09-03.md §2.1) - real cleanup for this window's own real
     * taskbar-registry file (written every redraw tick once real
     * sidebar+panel content exists, see redraw()'s own title-bar block).
     * Registered here, unconditionally, harmless no-op unlink for any
     * mode that never writes one (db-hq/events-hq/tile/etc.) - real,
     * clean-exit removal so a closed window's taskbar entry doesn't
     * linger; a crashed process still leaves a stale file, caught by
     * the taskbar's own real liveness check (kill(pid,0), same
     * convention ktb_pid_alive() already uses) when it polls, same
     * real second-safety-net shape as this design doc's own §2.1. */
    atexit(cleanup_hq_window_registry);
    atexit(kh_ascii_frame_unregister);   /* per-PID text-mirror frame/pulse pair */
    history_init_empty();                /* per-PID input relay exists from tick 0 - no swallowed first keystroke */
    /* REAL, NEW 2026-09-03 - default/HQ-window SIGTERM cleanup hook.
     * tp_main() (entity/tile path) installs handle_shutdown_signal() and
     * honors g_shutdown_requested, but the shared default/HQ path that
     * hosts chat-hai/open-hai/palettes/bookmarks/stats-hq/etc. did NOT -
     * a plain `kill -TERM` (what kill_hq_windows.sh and most session
     * shutdowns send first) default-terminated without running atexit(),
     * leaving the failed-livedesk_hq_windows_<pid>.txt registry file
     * behind (real live case). Install the same handler here so the
     * atexit cleanup runs on SIGTERM/SIGINT; hq_run_event_loop() now
     * checks g_shutdown_requested to break its loop. db-hq/events-hq
     * override with their own handlers a little later (their loops are
     * separate), which is fine. */
    signal(SIGTERM, handle_shutdown_signal);
    signal(SIGINT, handle_shutdown_signal);
    /* REAL, NEW 2026-09-01 - @ toggle: every real window this binary can
     * open (db-hq/events-hq/chat-hai/open-hai) obeys the shared pdl; the
     * popup/settings branches stay pinned below. Loaded before any
     * window creation - override_redirect is creation-time-only. */
    load_override_redirect(g_house_root);
    load_zorder_mode(g_house_root);
    /* REAL FIX 2026-09-04 (RENDERER-MODULARITY-AND-PERF-AUDIT.md-
     * adjacent finding, direct live report "single click vs double
     * click... was it added to settings yet") - this comment's own
     * claim ("dbhq_load_font_scale() also reads... click_two_step")
     * was already stale: dbhq_load_font_scale() no longer exists
     * anywhere in this file (deleted in an earlier dbhq_* cleanup
     * pass), and desktop_load_click_two_step() itself was ONLY ever
     * called from tp_main() (tile mode) - meaning the generic default-
     * mode path this comment claims to cover (db-hq/events-hq/chat-
     * hai/open-hai/settings/entity-menus/everything else) NEVER
     * actually loaded click_two_step at all, silently staying at its
     * compile-time default (1, two-step) regardless of what
     * #.desktop/hq_ui.pdl says - a real, previously-undiscovered gap
     * this comment's own words already promised was fixed. Restored
     * for real, at the exact spot this comment already said it
     * belonged. */
    desktop_load_click_two_step(g_house_root);
    snprintf(g_chtpm_path, sizeof(g_chtpm_path), "%s", argv[2]);
    snprintf(g_package_dir, sizeof(g_package_dir), "%s", g_chtpm_path);
    { char *slash = strrchr(g_package_dir, '/'); if (slash) *slash = '\0'; }

    /* generic argv[3] instance-dir hook (see g_arg3_dir decl). Must run
     * BEFORE parse_chtpm so g_extra_vars_path is picked up by the
     * static-template vars pass. */
    if (argc >= 4 && argv[3][0]) {
        struct stat a3st;
        if (stat(argv[3], &a3st) == 0 && S_ISDIR(a3st.st_mode)) {
            snprintf(g_arg3_dir, sizeof(g_arg3_dir), "%s", argv[3]);
            snprintf(g_extra_vars_path, sizeof(g_extra_vars_path),
                     "%s/.hq_manager/ui.txt", g_arg3_dir);
            setenv("KHTPM_ARG3", g_arg3_dir, 1);
            /* REAL FIX 2026-09-13 - see g_arg4_entity_label's own decl
             * comment: events-hq's real launch shape is <house> <chtpm>
             * <pkg_dir> <entity_label> - capture argv[4] here too, right
             * alongside g_arg3_dir, so kh_launch_window_modules() below
             * can pass it through to the manager it self-spawns. */
            if (argc >= 5 && argv[4][0])
                snprintf(g_arg4_entity_label, sizeof(g_arg4_entity_label), "%s", argv[4]);
        }
    }

    g_window = parse_chtpm(g_chtpm_path);
    if (!g_window) { fprintf(stderr, "khtpm_core_render: failed to parse %s\n", g_chtpm_path); return 1; }
    { struct stat gcst; if (stat(g_chtpm_path, &gcst) == 0) g_chtpm_mtime = gcst.st_mtim; }
    kh_text_areas_reload(g_window); /* 2026-09-08 - restore a persisted <text_area> (sql-hq editor) on launch too, not only across reparse */
    if (elem_has_class(g_window, "dock-header")) {
        /* REAL FIX 2026-09-13, direct live report ("nav is stuck at 1
         * again") - live-confirmed on a GENUINELY FRESH strip process
         * (a brand-new PID from an always-on-top respawn, never sent a
         * real click in its own lifetime by the time it was checked):
         * nav was already snapped into a "dropdown open" scope from
         * frame one (kh_elem_in_scope()'s own g_default_active_scope_id/
         * g_dock_drop_lo pair already non-empty/non-zero). Every real
         * setter of g_default_active_scope_id is click-driven (grepped,
         * confirmed - activate_focused()'s own ACTIVATE/tab branches,
         * the Escape-pop handler) - none of them can fire before this
         * process's own event loop even starts, so the exact mechanism
         * that pre-seeds it remains unconfirmed. Rather than chase a
         * process-local global's mystery initial state further, this is
         * the direct, robust answer: a freshly-started dock window
         * cannot possibly have a real, current scope yet (nothing has
         * been clicked in ITS OWN lifetime) - explicitly zero every
         * piece of that state right here, once, before the first real
         * layout pass ever runs, so whatever value these globals
         * happened to hold is irrelevant. Belt-and-suspenders on top of
         * the two earlier related fixes this same week (the reparse-
         * restore confine reset, and the dup-registry-line race) - this
         * one guarantees a clean slate unconditionally, not just in the
         * specific paths already found. */
        g_default_active_scope_root = NULL;
        g_default_active_scope_id[0] = '\0';
        g_default_scope_confine = 0;
        g_dock_drop_lo = 0;
        g_dock_drop_hi = 0;
        /* peer is the static bottom template beside the header, never
         * a generated #.desktop/strip_bottom.chtpm (layout-update). */
        snprintf(g_dock_peer_path, sizeof(g_dock_peer_path),
                 "%s/khtpm_strip_bottom.xhtpm", g_package_dir);
        /* REAL FIX 2026-09-13 (direct live report: "task bar has an
         * issue doing that that needs to be fixed" - the bottom bar
         * missing on SOME boots, not others). Same real ENOENT-style
         * transient-read-failure class already root-caused and fixed
         * for the RUNTIME reparse call sites (74debf38) - but THIS
         * one, the one-time STARTUP parse, never got the same
         * treatment: a single failed parse_chtpm() here left
         * g_dock_peer NULL for the entire life of the process, with
         * no later retry ever able to recover it (peer_changed only
         * fires on a real mtime change to a STATIC template file that
         * never changes again after boot). Unlike a runtime reparse,
         * this runs once before the event loop starts, so a short,
         * bounded blocking retry here is real and safe, not a
         * responsiveness regression. */
        for (int _dp_try = 0; _dp_try < 40 && !g_dock_peer; _dp_try++) {
            g_dock_peer = parse_chtpm(g_dock_peer_path);
            if (!g_dock_peer) {
                struct timespec _dp_ts = {0, 100 * 1000 * 1000L};
                nanosleep(&_dp_ts, NULL);
            }
        }
        if (!g_dock_peer)
            fprintf(stderr, "khtpm_core_render: dock peer parse failed after retries: %s\n", g_dock_peer_path);
        { struct stat pst; if (g_dock_peer && stat(g_dock_peer_path, &pst) == 0) g_dock_peer_mtime = pst.st_mtim; }
    }

    /* REAL Stage 5 §5d.3 step 6 (2026-08-16, khtpm-merge-how2.md §5d) -
     * real, data-driven mode detection - `<window class="swatch-
     * picker">` (matches wraith-alpha's own real "one binary, behavior
     * selected by loaded data" shape, not a new attribute/parser
     * change - class= was already fully generic). */
    for (int i = 0; i < g_window->n_classes; i++) {
        if (strcmp(g_window->classes[i], "database-window") == 0 ||
            strcmp(g_window->classes[i], "palettes-pal") == 0) g_default_persistent = 1;
        if (strcmp(g_window->classes[i], "user-resizable") == 0) g_user_resizable = 1;
        /* REAL Stage 5 §5d.10 (2026-08-16) - db-hq mode, real, data-
         * driven detection (`<window class="db-hq">`, same convention
         * as swatch-picker's own). */
        /* REAL §5d.11 (2026-08-16) - events-hq mode, same real
         * convention, matching its own real existing class attribute
         * (`<window class="events-hq-window">`, unchanged - no new
         * class token needed, this app's own class already existed). */
        /* REAL, NEW 2026-08-25 (au11-hq/TPMOS-COMPLIANCE-DEBT.md - full
         * compliant rebuild, direct instruction: "do this completely
         * tpmos compliant"). stats-hq reuses db-hq's ENTIRE proven
         * sidebar+panel+dispatch+module-launch machinery (real, live
         * code, not a second copy) - g_is_db_hq=1 too. g_is_stats_hq
         * exists ONLY to give it its own state-file/relay-file names
         * (see g_dbhq_events_state_path below and history_path()) so a
         * real db-hq window and a real stats-hq window can run
         * simultaneously without colliding on the same files. The
         * OLD stats-hq (open_stats_hq.sh's own bash regex-scrape +
         * printf-XML <tabbar>, TPMOS-COMPLIANCE-DEBT.md's worst finding
         * - tabs that render but never respond to clicks) is replaced
         * entirely: a real, new, testable stats_hq_manager.c (matching
         * khtpm_hq_manager.c's own real shape) now owns the session-
         * stats scan and publishes into the SAME simple state-file
         * format dbhq_load_common_events() already parses - sidebar
         * items work for real because they ride the exact same generic
         * item-click path db-hq's own Common Events already prove out
         * live, not a new one. */
        /* REAL, NEW 2026-08-25 (Stage 2 palettes migration) - see
         * g_is_palettes's own declaration comment. */
        /* REAL, NEW 2026-08-25 (Stage 3 bookmarks migration off the
         * deprecated standalone khtpm_hq_render.c) - bm_menu.sh
         * composes <window class="database-window bookmarks">. */
    }

    /* REAL FIX 2026-08-16, direct live report ("doesn't open by her
     * actual position like old context menu does") - launch_khtpm_menu()
     * now passes the caller's real, screen-clamped popup x/y (the same
     * px/py open_context_menu() itself computes via
     * clamp_popup_to_screen()) as argv[3]/argv[4]. Optional so a
     * standalone/relay-testing launch (2-arg) still works with the old
     * 300,300 default. REAL §5d.11 (2026-08-16) - events-hq mode
     * reinterprets argv[3]/argv[4] as its own real <pkg_dir>
     * <entity_label> (it's legitimately multi-instance, scoped by
     * pkg_dir, and never supported explicit x/y anyway - always starts
     * at its own real 120,120 default) - moved this block to AFTER mode
     * detection since it now needs to know which interpretation applies. */
    if (argc >= 5 && !g_arg3_dir[0]) {
        /* not the instance-dir hook (g_arg3_dir) -> the legacy popup
         * launch shape where argv[3]/argv[4] are x/y ints. */
        g_win_x = atoi(argv[3]); g_win_y = atoi(argv[4]);
    }

    /* REAL Stage 5 (2026-08-16, khtpm-merge-how2.md §5d) - real, mode-
     * selected CSS (was never loaded at all before this port for menu
     * mode; swatch-picker mode keeps its own real taskbar_settings.css,
     * unchanged content). db-hq/events-hq modes keep their own real
     * convention - css_path derived by extension-swap from the .chtpm
     * path itself (dashboard.chtpm -> dashboard.css), ported verbatim,
     * not a fixed ops-dir filename like the other 2 modes. */
    {
        char css_path[PATH_BUF];
        snprintf(css_path, sizeof(css_path), "%s/*.monads/*.livedesk-taskbar/ops/entity_menu_default.css",
                 g_house_root);
        memset(&g_sheet, 0, sizeof(g_sheet));
        css_load(css_path, &g_sheet);
        /* REAL, NEW 2026-09-02 - merge the .css sitting next to the
         * loaded .chtpm (same stem). Data-driven: any app can ship
         * window size and colors without a mode flag. Missing file
         * is a no-op (css_load returns 0). */
        {
            char app_css[PATH_BUF];
            snprintf(app_css, sizeof(app_css), "%s", g_chtpm_path);
            char *dot = strrchr(app_css, '.');
            if (dot) snprintf(dot, sizeof(app_css) - (size_t)(dot - app_css), ".css");
            if (strcmp(app_css, css_path) != 0) css_load(app_css, &g_sheet);
        }
    }

    /* REAL, NEW 2026-09-03 (direct live report: "look at how db hq has
     * its x button, can u just do that? rpg maker tiles do same, they
     * never go off screen") - same real convention as db-hq's own
     * g_win_x=100/events-hq's own g_win_x=120 right above, applied here
     * for the last remaining default-mode case: a real sidebar+panel
     * window (co-lab-hai/open-hai/chat-hai - the wide, ~900px-class
     * windows, not a small transient popup, which legitimately wants to
     * appear wherever it was invoked from). These never got their own
     * explicit anchor - they fell through to the generic popup default,
     * which is really the SHARED, drag-tracked hq_ui.pdl window_x/
     * window_y (or 300,300 if never saved) - a value ANY other popup
     * may have left far from a safe corner, then a genuinely wide
     * window inherits it verbatim. db-hq/events-hq never had this
     * problem for the simple reason they never read that shared value
     * at all - they stomp it with their own fixed, safe corner anchor
     * right after the one shared load call (dbhq_load_font_scale()) at
     * line ~17761, same real pattern applied here. */
    if (find_by_tag(g_window, "sidebar") && find_by_tag(g_window, "panel")) {
        g_win_x = 80; g_win_y = g_win_top_y; /* sidebar+panel default (hq_ui.pdl win_top_y) */
    }

    /* REAL FIX (found live, first standalone test): g_win_h is DATA-
     * DRIVEN (item count) but the window/pixmap used to be created at a
     * fixed default height BEFORE this ever ran - redraw()'s first
     * layout pass would then XGetImage a LARGER area than the actual
     * Pixmap, a real geometry mismatch (X_GetImage BadMatch, confirmed
     * live). Real fix: compute the real height once, up front, before
     * creating anything X11-side - this menu's content is static per
     * page switch, no need for ConfigureNotify-driven runtime resize.
     * REAL Stage 5 §5d.10 (2026-08-16) - dpy/screen/cmap now open
     * BEFORE this call, not after (moved up) - db-hq mode's own real
     * layout pass needs a live X connection to measure font metrics
     * (kh_measure_text_px()), unlike the popup modes' fixed-height
     * rows which never needed dpy this early. Harmless reorder for
     * popup modes - dpy/screen/cmap weren't used before this point
     * either way. */
    if (g_headless) {
        /* the normal path forks the window's <module>s ~250 lines below,
         * after XOpenDisplay - headless returns before that, so do it
         * here. Same generic call, same cleanup (atexit kh_cleanup_
         * modules, registered inside). Without this a headless window
         * shows only its static skeleton, its manager never running. */
        kh_launch_window_modules(g_window, g_house_root, g_arg3_dir[0] ? g_arg3_dir : g_package_dir);
        return headless_run();
    }

    dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "khtpm_entity_menu_render: cannot open display\n"); return 1; }
    /* REAL FIX 2026-09-01 (found live: open-hai/chat-hai/network-browser
     * all completely failed to launch - real repro via direct binary
     * run showed a fatal, unhandled "BadMatch (invalid parameter
     * attributes)" on X_SetInputFocus, killing the whole process before
     * a single frame ever drew). Root cause: the @ z-order work (this
     * session, oc) makes windows optionally real WM-managed
     * (override_redirect=false per #.desktop/livedesk_override_
     * redirect.pdl) - a managed window's map is ASYNCHRONOUS (the WM
     * must reparent it), unlike override_redirect (always instant, no
     * WM involved) - so the existing post-map XSetInputFocus retry loop
     * (~line 12824, written back when every window here WAS always
     * override_redirect) can now genuinely fire before the window is
     * viewable, and X's default error handler calls exit() on any
     * unhandled error. kh_nonfatal_x_error() already existed for
     * exactly this class of "best-effort X call, never worth crashing
     * the whole app over" case, but was only ever installed for
     * events-hq mode - installing it globally here, at the earliest
     * possible point, so EVERY mode gets the same real safety net
     * (this fixes the crash without needing to chase every individual
     * best-effort X call across every mode one at a time). */
    XSetErrorHandler(kh_nonfatal_x_error);
    screen = DefaultScreen(dpy);
    cmap = DefaultColormap(dpy, screen);
    g_ui_screen_w = DisplayWidth(dpy, screen);
    g_ui_screen_h = DisplayHeight(dpy, screen);
    kh_ui_apply_scale(); /* screen-relative UI scale, see kh_ui_apply_scale() */
    /* class="user-resizable" opens at a modest fixed size, offset from
     * the corner so the chrome (x / ! / _) is always reachable. NOT
     * derived from DisplayWidth/Height - those read the framebuffer,
     * which under HiDPI / a virtual desktop can be much larger than the
     * visible monitor (direct report 2026-09-09: full-screen put the
     * close button off the right edge). Only clamped DOWN to the
     * display. The ⌟ drag + the canvas take it from here. */
    if (g_user_resizable) {
        int sw = DisplayWidth(dpy, screen), sh = DisplayHeight(dpy, screen);
        g_win_x = 90;
        g_win_y = WM_MANAGED_DRAG_MIN_Y;
        g_win_w = kh_auto_px(1120);
        g_win_h = kh_auto_px(720);
        if (g_win_w > sw - g_win_x - 60)  g_win_w = sw - g_win_x - 60;
        if (g_win_h > sh - g_win_y - 40)  g_win_h = sh - g_win_y - 40;
        if (g_win_w < KH_WIN_MIN_W) g_win_w = KH_WIN_MIN_W;
        if (g_win_h < KH_WIN_MIN_H) g_win_h = KH_WIN_MIN_H;
    }
    reload_font_ui();  /* "Noto Sans CJK SC" / "DejaVu Sans" at pixelsize scaled(13)/scaled(12) - honours hq_ui.pdl font_scale, loaded just above */
    /* REAL, NEW 2026-08-25 (live report: bookmarks' own path labels
     * carry real emoji dir names, rendered as tofu boxes - "open-hai
     * has an implementation for this we can steal") - loads once, here,
     * for every mode (not just db-hq/bookmarks): any label text in any
     * consumer can legitimately contain emoji, this house's own
     * directory names prove that. */
    khtpm_load_emoji_tiles(g_house_root);

    assign_nav_and_layout();

    /* REAL §5d.11 (2026-08-16) - events-hq mode: real WM-managed window
     * creation + own event loop, kept as its own separate branch (not
     * interleaved into db-hq's or the popup modes' code) since its real
     * drag/focus/poll logic, while similar in shape to db-hq's, is a
     * genuinely separate real implementation (own globals, own close
     * elem, own picker-overlay-aware click gating) - same real
     * per-mode-exception precedent as everywhere else in this file. */

    XSetWindowAttributes swa;
    /* REAL FIX 2026-09-03 (direct live report, still reproducing after
     * layout_sidebar_panel()'s own clamp - "x for exit is still hanging
     * off visible window"): root-caused further - that first clamp only
     * ever runs for the sidebar+panel dual-region layout; a plain
     * entity-menu/popup window (g_win_w/g_win_h fixed once, well before
     * this real XCreateWindow call, never revisited by that function at
     * all) never went through it. This is the one real choke point
     * EVERY default-mode window - flat popup, swatch-picker, sidebar+
     * panel alike - passes through with its own final g_win_x/g_win_y/
     * g_win_w/g_win_h already decided, so the clamp belongs here, once,
     * not duplicated per layout mode. Same real DisplayWidth/
     * DisplayHeight this file already uses for fullscreen; only pulls
     * IN from an off-screen edge, never re-centers. */
    {
        int sw = kh_screen_w(), sh = kh_screen_h();
        if (g_win_x + g_win_w > sw) g_win_x = sw - g_win_w;
        if (g_win_y + g_win_h > sh) g_win_y = sh - g_win_h;
        if (g_win_x < 0) g_win_x = 0;
        if (g_win_y < 0) g_win_y = 0;
    }
    load_theme_colors();  /* every window, not just dock - the 2px window frame + theme-aware bg need g_theme_fg/bg live */
    swa.background_pixel = alloc_pixel(g_theme_bg); /* themed base (load_theme_colors() ran just above); static #1c1c1c default keeps the no-white-flash guarantee */
    /* REAL FIX 2026-08-16, direct live report ("none of the buttons seem
     * 2 work yet"): this window was a normal WM-managed window, unlike
     * the legacy popup (override_redirect=True, open_context_menu() near
     * line 1414). Most WMs (Mutter included) use click-to-focus - the
     * FIRST click on a just-mapped, unfocused window only focuses it and
     * never reaches the app as a real ButtonPress, and since this
     * process launches fresh on every open, EVERY click was a first
     * click. override_redirect bypasses window-manager management
     * entirely (same as any real popup/menu), so clicks are delivered
     * immediately - matches the legacy popup's own real behavior. */
    /* REAL, NEW 2026-09-01 - @ toggle: this branch is open-hai (a real
     * panel window) AND the transient entity-menu popup/taskbar-Settings
     * swatch-picker. swatch-picker stays pinned always (the human is the
     * foreground actor there); open-hai and the transient menus follow
     * the shared pdl's managed=false when "normal" sinks below native
     * apps. Same data-driven rule, same shared #.desktop pdl. */
    /* !!! NEVER REMOVE THIS COMMENT OR THE `dock_managed` LOGIC BELOW
     * WITHOUT READING IT FIRST !!! (direct user instruction, 2026-09-05)
     * Full incident writeup, alternatives considered, and standing
     * rule: 03-pitfalls/X11-AND-SESSION-PITFALLS.md, 2026-09-05 entry.
     * One-line version: this exact bug (dock arrow-key nav dying) was
     * ALREADY fixed once (eb74b733, 2026-08-30) in the taskbar's THEN
     * implementation, khtpm_strip_parser.c - that file was later fully
     * replaced by this file's own dock mode, which never inherited the
     * fix and just followed the shared `g_override_redirect` PDL like
     * every other window. The very next unrelated revert of that PDL
     * (fixing a DIFFERENT bug, the toys-dropdown regression) silently
     * undid this fix as a side effect. If you are refactoring dock
     * window creation, or ever tempted to make this follow
     * `g_override_redirect` "for consistency" - don't. That is
     * literally the regression. The dock strip windows (this one and
     * g_dock_peer_win below) must stay unconditionally WM-managed,
     * independent of any global override_redirect setting, forever. */
    /* REAL FIX 2026-09-05, direct live report ("when i click tb, i
     * expect arrows to control nav, this broke again... same problem
     * as last time") - this is the SAME real bug class eb74b733
     * (2026-08-30, "Fix taskbar's own intermittent click failures:
     * WM-managed, not override_redirect") already found and fixed
     * once, for the taskbar's OLD implementation (khtpm_strip_parser.c,
     * since replaced by this file's own dock mode) - Mutter's real
     * XWayland focus/key routing never reaches an override_redirect
     * surface at all (XSetInputFocus/XGetInputFocus report success at
     * the raw X11-protocol level regardless - why every xdotool/
     * synthetic-event test this session looked fine while real
     * hardware kept failing). That old fix's target file no longer
     * exists; this file's own dock windows inherited the house-wide
     * `g_override_redirect` PDL setting instead, which the 217d97eb
     * revert (same day, different bug - the toys dropdown menu breaking
     * once EVERY window went WM-managed) put back to `true` - silently
     * undoing eb74b733's fix for the dock strip specifically as a side
     * effect of fixing something else. Scoped fix this time so a
     * future revert of the global PDL can't do this again: the
     * persistent dock strip windows (this one, window_is_dock(), and
     * g_dock_peer_win below) are unconditionally WM-managed, completely
     * independent of the global `g_override_redirect` PDL - matching
     * apply_dock_window_hints()'s own already-unconditional EWMH dock
     * hints just below. g_dock_menu_win (the toys dropdown, ~line 3802)
     * deliberately keeps its own hardcoded `override_redirect = True` -
     * that IS the 217d97eb regression's real fix already in place
     * (short-lived popups/submenus correctly stay override_redirect,
     * per 03-pitfalls/X11-AND-SESSION-PITFALLS.md) - not touched here. */
    int dock_managed = window_is_dock();
    /* pc-hq-leg-vs-nu-fix.md §5-A-ii: <window class="managed"> is
     * WM-managed like the dock. Only pchq-board sets the class. */
    /* Any window holding a text field / grid must be WM-managed regardless of
     * the always-on-top PDL: Mutter/XWayland never routes keyboard focus to
     * override_redirect windows (pc-hq-leg-vs-nu-fix.md, csv-hq bug_bounty).
     * class="unmanaged" opts out. */
    int win_managed = dock_managed || elem_has_class(g_window, "managed") ||
                      (!elem_has_class(g_window, "unmanaged") && kh_tree_has_text_input(g_window));
    g_win_managed_focus = win_managed && !dock_managed;
    swa.override_redirect = win_managed ? False : (Bool)g_override_redirect;
    /* REAL FIX 2026-08-29 (live report: "toolbar doesn't allow drag
     * repositioning") - this generic popup window (entity-menu popup AND
     * swatch-picker/Settings) never requested ButtonReleaseMask or
     * ButtonMotionMask, unlike db-hq/events-hq/chat-hai's own event masks
     * just above, which all three DO include. TASK 1's drag code
     * (g_popup_dragging, hq_dispatch_xevent's is_popup MotionNotify/
     * ButtonRelease branches) was real and correctly wired, but X11 was
     * never asked to deliver those event types to this window at all, so
     * ButtonPress armed g_popup_dragging and then nothing ever moved or
     * cleared it - same class of bug as the missing CWOverrideRedirect
     * mask entry found earlier this session (a struct field set but the
     * corresponding mask bit missing, so X11 silently ignores it). */
    swa.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | ButtonMotionMask | KeyPressMask | StructureNotifyMask | FocusChangeMask;
    win = XCreateWindow(dpy, RootWindow(dpy, screen), g_win_x, g_win_y, (unsigned)g_win_w, (unsigned)g_win_h, 0,
                         CopyFromParent, InputOutput, CopyFromParent, CWBackPixel | CWOverrideRedirect | CWEventMask, &swa);
    if (window_is_dock()) apply_dock_window_hints(dpy, win, g_win_x, g_win_y);
    render_managed_wm_hints(dpy, win, win_managed || !g_override_redirect); /* REAL, NEW 2026-09-01 - managed branch; win_managed adds class="managed" 2026-09-08 */
    Atom motif_hints = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
    long hints[5] = { 2, 0, 0, 0, 0 };
    XChangeProperty(dpy, win, motif_hints, motif_hints, 32, PropModeReplace, (unsigned char *)hints, 5);
    Atom wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &wm_delete, 1);
    /* PPosition - same real fix db-hq/events-hq/chat-hai already needed
     * (khtpm-merge-how2.md's own white-flash/position entries) - without
     * this the WM ignores the requested x/y. */
    XSizeHints *shints = XAllocSizeHints();
    if (shints) { shints->flags = PPosition; shints->x = g_win_x; shints->y = g_win_y; XSetWMNormalHints(dpy, win, shints); XFree(shints); }

    XMapRaised(dpy, win);
    set_window_opacity(dpy, win, load_theme_opacity());
    XSync(dpy, False);
    if (window_is_dock()) XMoveWindow(dpy, win, g_win_x, g_win_y);
    if (!window_is_dock())
    render_managed_sink_below(dpy, win); /* REAL, NEW 2026-09-01 - @ "normal" mode: drop this managed window below native apps (undoes the MapRaised) */
    /* 2026-08-24 - XDND drop-target opt-in (no-op unless this .chtpm
     * declared a window-level drop_action= attribute). */
    xdnd_init_atoms(dpy);
    xdnd_attach_if_needed(dpy, win);
    /* REAL FIX 2026-08-28, direct live report ("popups are no longer
     * getting nav/index focus use like they used to. i have to
     * manually click with mouse"): the "no XSetInputFocus on map"
     * rule above was written for the AGENT-steals-the-browser case
     * (an unattended process silently mapping a window while the human
     * is doing something else, e.g. typing) - real, correct guidance
     * for THAT case, per HQ-WINDOW-MAP-AND-AGENT-INPUT.md's own §1
     * table entry: "raise-then-focus ONLY when the human needs keys in
     * that popup." An entity-menu / taskbar-settings popup is NOT that
     * case - it only ever exists because the human JUST right-clicked
     * (or otherwise directly triggered) it, same real moment their
     * mouse is already there, same normal-desktop-context-menu
     * expectation every other app on this OS gives for free. Removing
     * SetInputFocus here fixed a real problem for AGENT-launched HQ
     * windows but broke real keyboard nav for HUMAN-launched popups -
     * this restores it, scoped to popups only (HQ windows keep the
     * XMapWindow/no-focus fix from earlier tonight, unchanged). A
     * short retry (F-19: a bare call can silently fail once under
     * XWayland/Mutter) - not a full XGrabKeyboard (that's the
     * heavier, house-wide-flock-guarded exclusive resource reserved
     * for grab_keyboard=1 entities specifically, a separate, real,
     * not-yet-wired STATE flag - see ENTITY-MENU-LEGACY-DEPRECATION-
     * PLAN.md) - just enough for normal KeyPress delivery to this
     * window like any other popup on this desktop. */
    for (int attempt = 0; attempt < 5; attempt++) {
        XSetInputFocus(dpy, win, RevertToParent, CurrentTime);
        XSync(dpy, False);
        Window focused; int revert;
        XGetInputFocus(dpy, &focused, &revert);
        if (focused == win) break;
        usleep(5000);
    }
    clock_gettime(CLOCK_MONOTONIC, &g_map_time);
    /* REAL FIX 2026-08-16, direct live report ("it also pops up instead
     * of context menu when i rightclick ava" - Chat fired immediately):
     * same real cause class as tp_desktop_window_rgb.c's own documented
     * window-ID-recycle phantom click fix (open_context_menu()) - the
     * right-click that triggered this whole launch can still have a
     * trailing Button event sitting in this window's queue the instant
     * it maps. Drain it before the real event loop starts, so only
     * input that arrives after this window genuinely existed can select
     * a row. */
    {
        XEvent stale_ev;
        while (XCheckWindowEvent(dpy, win, ButtonPressMask | KeyPressMask, &stale_ev)) {
            /* discard - see comment above */
        }
    }

    /* REAL, NEW 2026-08-31 (open-hai's own real conversion, xperiments/
     * khtpm-generic-dispatch-design.md) - the default/popup mode never
     * had ANY <module> launch support (db-hq/events-hq/chat-hai each
     * have their own copy, gated behind their own g_is_X flags). Reuses
     * the already-generic launch_module() (§2a, zero project knowledge)
     * and db-hq's own g_dbhq_module_pid/dbhq_cleanup_module() - despite
     * the db-hq-prefixed name, neither has any g_is_db_hq check inside,
     * they're already mode-agnostic "the module THIS process launched"
     * bookkeeping, just never wired up for this mode before. Checked
     * ONLY here (once, at initial parse) - NOT inside reparse_chtpm_
     * if_changed(), which fires repeatedly for this mode's whole real
     * reason for existing (a live-regenerating manager) - re-checking
     * there would fork a NEW manager on every single content change.
     * A manager's own regenerated .chtpm simply omits the <module> tag
     * once running (nothing re-checks it after this one-time launch,
     * so its presence or absence in later reparses doesn't matter
     * either way - omitted for clarity, not because it's required). */
    {
        /* REAL FIX 2026-09-01 (live report: relaunching open-hai via
         * button.sh left an orphaned khtpm_open_hai_manager.+x behind
         * EVERY time, piling up - confirmed live, multiple stale
         * managers found still running after repeated relaunches).
         * Root cause: button.sh kills this process with a plain
         * `kill -TERM`, and SIGTERM's default action terminates a
         * process WITHOUT running its atexit() handlers - dbhq_
         * cleanup_module() was registered via atexit() only, which
         * only ever fires on a NORMAL exit()/return from main(), never
         * on an external signal. db-hq/events-hq/chat-hai already
         * solved this exact problem for their own managers with a real
         * SIGTERM/SIGINT handler (dbhq_handle_term_signal(), which
         * explicitly calls dbhq_cleanup_module() before _exit()) - this
         * mode just never installed it, since it never had a module to
         * clean up before now. A real SIGTERM/SIGINT handler here was
         * tried and REVERTED (2026-09-01, live-confirmed via repeated
         * isolated testing): installing it made the freshly-forked
         * child die shortly after its own first write, root cause not
         * isolated. Real fix instead, borrowed from chat-hai's OWN
         * already-proven, different real mechanism for this exact
         * problem (chai_launch_module()'s own real chat_hai_renderer.
         * pid file + chat_hai_loop.sh's own liveness poll) - made
         * generic here rather than copied a second time: this renderer
         * writes ITS OWN pid to a real, predictable, generic path next
         * to the .chtpm (module_parent.pid in package_dir) before
         * launching ANY module; a module binary that wants this real
         * "exit if my own parent is gone" safety net can poll that same
         * file itself (see khtpm_open_hai_manager.c's own real use of
         * it) - opt-in, not required, zero effect on a module that
         * never reads it. Fork EVERY <module> (shell + one per tab). */
        kh_launch_window_modules(g_window, g_house_root, g_arg3_dir[0] ? g_arg3_dir : g_package_dir);
    }

    {
        /* graphics_exposures=False: the XCopyArea fallback in redraw()
         * (Pixmap->window blit) otherwise emits a GraphicsExpose/NoExpose
         * per call with a default GC - noise this loop has no handler for. */
        XGCValues gcv; gcv.graphics_exposures = False;
        gc = XCreateGC(dpy, win, GCGraphicsExposures, &gcv);
    }
    buf = XCreatePixmap(dpy, win, (unsigned)g_win_w, (unsigned)g_win_h, (unsigned)DefaultDepth(dpy, screen));
    xftdraw_buf = XftDrawCreate(dpy, buf, DefaultVisual(dpy, screen), cmap);
    /* REAL FIX 2026-08-31, direct live report ("blank black screen that
     * flashes before load" on every entity context menu since today's
     * work): g_buf_w/g_buf_h (the real allocated-Pixmap-size tracker
     * redraw()'s own resize-safety check now uses for this mode too -
     * see that check's own header comment) were never set here, unlike
     * every other mode's own window-creation code (db-hq/events-hq/
     * chat-hai all set them right after their own XCreatePixmap). Left
     * at their static 0/0 default, redraw()'s check saw g_win_w/h > 0/0
     * as "grown" on literally the FIRST real frame of every popup ever
     * opened, recreating the Pixmap and, per that check's own honest
     * "next redraw() repaints it for real" contract, silently
     * discarding that first frame's real content - exactly the blank
     * flash reported live. Real fix: record the REAL size this Pixmap
     * was actually just created at, matching every other mode's own
     * convention. */
    g_buf_w = g_win_w; g_buf_h = g_win_h;

    /* REAL FIX 2026-09-14 - startup creation folded into the same real
     * function (`kh_ensure_dock_peer_window()`, defined above main())
     * the main loop's own per-tick self-heal calls, so this window is
     * built in exactly one place regardless of whether it's the first
     * time or a live recovery. See that function's own header comment
     * for the full "bottom toolbar completely gone" incident this
     * closes. */
    kh_ensure_dock_peer_window();

    redraw();
    /* REAL FIX 2026-08-29 part 3 - see db-hq branch's own identical
     * comment above (OPACITY-PIPELINE-INVESTIGATION-2026-08-29-part3.txt)
     * - same real "opacity-on-reset" quirk, same fix, ported from
     * khtpm_strip_parser.c's own already-documented pattern. */
    XFlush(dpy);
    usleep(200000);
    set_window_opacity(dpy, win, load_theme_opacity());
    XFlush(dpy);

    hq_run_event_loop(wm_delete, 1);

    XftDrawDestroy(xftdraw_buf);
    XFreeGC(dpy, gc);
    XFreePixmap(dpy, buf);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}

