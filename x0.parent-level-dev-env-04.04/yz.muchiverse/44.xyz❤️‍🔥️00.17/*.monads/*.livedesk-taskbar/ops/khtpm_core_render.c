#define _POSIX_C_SOURCE 200809L /* CLOCK_MONOTONIC + getline() under -std=c11 strict mode - bumped from 199309L 2026-08-16 for chai_load_ledger()'s real getline() fix, see that function's own header comment */
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

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "lib/stb_image_write.h"

#define PATH_BUF 4096
/* REAL Stage 5 §5d.10 (2026-08-16) - bumped 256->512 to match db-hq's
 * own original headroom (khtpm_hq_render.c) now that db-hq mode's own
 * 15-tab/sidebar/panel tree shares this same pool. */
static void nav_tab_unregister(void);
static void nav_ledger_publish(void);
static void popup_handle_click(int px, int py);
static void history_unregister(void); /* REAL, NEW 2026-08-29 - see its own real definition/comment near history_path() */
static void zero_nav_subtree(Elem *e); /* REAL, NEW 2026-08-29 - see its own real definition/comment near evhq_zero_subtree() */
static void redraw(void); /* REAL, forward declaration needed for dispatch()'s OPACITY_MINUS/OPACITY_PLUS handlers (NEW 2026-08-29 TASK 2) */
#define MAX_ELEMS 512
#define MAX_PAGE_STACK 8

static Elem g_pool[MAX_ELEMS];
static int g_n_elems = 0;
static char g_package_dir[PATH_BUF];
static char g_house_root[PATH_BUF];
static char g_chtpm_path[PATH_BUF];  /* real, generic (2026-08-31) - the real .chtpm this process was launched against, kept for the generic live-reparse capability below */
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
/* REAL, NEW 2026-09-01 - the @ z-order toggle's managed half. House rule:
 * behavior comes from #.desktop/livedesk_override_redirect.pdl (true =
 * always-on-top override_redirect window, false = WM-managed so the WM
 * decides z-order and the "normal" mode can sink these below native
 * apps). Absent file / unrecognized value = default true (existing
 * behavior, every real window unaffected). override_redirect is fixed at
 * XCreateWindow time, so the taskbar's toggle kills + relaunches each
 * living window via /proc (ktb_toggle_zorder_respawn in
 * khtpm_strip_parser.c) - this loader only has to be right at startup. */
static int g_override_redirect = 1;
static void load_override_redirect(const char *house_root) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/livedesk_override_redirect.pdl", house_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[64];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *val = eq + 1;
        val[strcspn(val, "\r\n")] = '\0';
        if (strcmp(line, "override_redirect") == 0)
            g_override_redirect = (strcmp(val, "true") == 0);
    }
    fclose(f);
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
/* REAL, NEW 2026-09-01 - remember which modes are real "windows" the
 * user toggles (open-hai, db-hq, events-hq, chat-hai) vs the transient
 * always-on-top chrome (entity-menu popup / taskbar-Settings swatch
 * picker). swatch-picker stays pinned regardless of the pdl - the one
 * dialog where the human IS the foreground actor, same reason the tile
 * popup menu keeps override_redirect=True. */
static int g_is_swatch_picker = 0;

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
static void set_window_opacity(Display *d, Window w, double opacity) {
    if (opacity < 0.0) opacity = 0.0;
    if (opacity > 1.0) opacity = 1.0;
    Atom opacity_atom = XInternAtom(d, "_NET_WM_WINDOW_OPACITY", False);
    unsigned long val = (unsigned long)(opacity * (double)0xFFFFFFFFUL);
    XChangeProperty(d, w, opacity_atom, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&val, 1);
}

static double load_theme_opacity(void) {
    double opacity = 0.5;
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
static pid_t g_dbhq_module_pid = -1;

static void dbhq_cleanup_module(void) {
    if (g_dbhq_module_pid > 0) {
        kill(g_dbhq_module_pid, SIGTERM);
        waitpid(g_dbhq_module_pid, NULL, WNOHANG);
        g_dbhq_module_pid = -1;
    }
}

static void dbhq_handle_term_signal(int sig) {
    (void)sig;
    nav_tab_unregister();
    history_unregister();
    dbhq_cleanup_module();
    _exit(0);
}

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
static pid_t launch_module(const char *src, const char *house_root, const char *package_dir, const char *extra_arg) {
    if (!src || !src[0]) return -1;
    char full_path[PATH_BUF];
    if (src[0] == '/') snprintf(full_path, sizeof(full_path), "%s", src);
    else snprintf(full_path, sizeof(full_path), "%s/%s", house_root, src);

    pid_t pid = fork();
    if (pid == 0) {
        if (extra_arg && extra_arg[0])
            execl(full_path, full_path, house_root, package_dir, extra_arg, (char *)NULL);
        else
            execl(full_path, full_path, house_root, package_dir, (char *)NULL);
        _exit(1);
    } else if (pid < 0) {
        fprintf(stderr, "khtpm_entity_menu_render: launch_module: fork failed for %s\n", full_path);
    }
    return pid;
}

static void dbhq_launch_module(const char *src, const char *extra_arg) {
    /* REAL, NEW 2026-08-25 (bookmarks manager port) - modules now also
     * get the package dir (chtpm's own dirname, i.e. the pal dir for a
     * per-pal consumer like bookmarks) as argv[2]. Backward compatible:
     * every existing manager (khtpm_hq_manager.c, stats_hq_manager.c)
     * only reads argv[1] and ignores the extra arg.
     * REAL, NEW 2026-08-25 (palettes manager port) - optional argv[3]
     * from <module args="..."/> (see apply_attr()'s own "args" branch),
     * for a manager that serves multiple category windows off one
     * binary (palettes_manager.c) and needs to know which one. NULL/
     * empty is the common case (bookmarks/stats-hq don't use it) -
     * execl() just gets a shorter argv, no behavior change for them.
     *
     * CORRECTED 2026-08-31 - was its own real fork()+execl() here; now
     * delegates to the generic launch_module() above (xperiments/
     * khtpm-generic-dispatch-design.md §2a) - same exact argv, same
     * exact behavior, zero fork()/execl() duplication. */
    g_dbhq_module_pid = launch_module(src, g_house_root, g_package_dir, extra_arg);
}

static Elem *elem_new(const char *tag) {
    if (g_n_elems >= MAX_ELEMS) return NULL;
    Elem *e = &g_pool[g_n_elems++];
    memset(e, 0, sizeof(*e));
    snprintf(e->tag, sizeof(e->tag), "%s", tag);
    return e;
}

/* REAL BUG FIX 2026-08-26 (live user report: "all visual from common-
 * events disappears... after show-choices has been open a while") -
 * elem_new()'s single shared g_pool[MAX_ELEMS] never recycles slots.
 * Dynamic, frequently-rebuilt UI content (command lists, sidebar items)
 * must NOT consume that shared pool on every rebuild, or a long enough
 * real session exhausts it and the affected panel silently goes blank
 * (elem_new() returns NULL, guarded call sites just skip adding
 * content). Fix: give each frequently-rebuilt list its OWN small,
 * fixed, NEVER-freed array of real Elem structs (declared separately
 * from g_pool, sized generously for realistic use), and reuse the SAME
 * struct instances every rebuild instead of allocating fresh ones. */
static Elem *reusable_slot(Elem *slots, int max_slots, int index, const char *tag) {
    if (index < 0 || index >= max_slots) return NULL;
    Elem *e = &slots[index];
    memset(e, 0, sizeof(*e));
    snprintf(e->tag, sizeof(e->tag), "%s", tag);
    return e;
}

/* ---------- tiny generic tag-tree parser (same shape as db-hq/events-hq's
 * own hand-rolled parser, not reinvented) ---------- */
static void skip_ws(const char **p) { while (**p && isspace((unsigned char)**p)) (*p)++; }

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
         * set regardless of draw_elem()'s own sprite-blit support. */
        snprintf(e->sprite, sizeof(e->sprite), "%s", val);
    } else if (strcmp(name, "src") == 0) {
        /* REAL Stage 5 §5d.10 (2026-08-16) - db-hq mode only, ported
         * from khtpm_hq_render.c's own apply_attr(): <module src="..."/>
         * real, wraith_parser_alpha.c convention. Reused e->label to
         * hold it - <module> elements are never drawn, safe reuse. */
        snprintf(e->label, sizeof(e->label), "%s", val);
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
    } else if (strcmp(name, "rows") == 0) {
        /* REAL, NEW 2026-09-01 - see Elem's own rows field comment in
         * khtpm_render_core.c. */
        e->rows = atoi(val);
    } else if (strcmp(name, "drop_action") == 0) {
        /* 2026-08-24 - see the g_drop_action block comment above.
         * Window-level attr; decoded through the SAME entity decoder
         * action= uses, so &quot;/&amp;/&gt;/&lt; all behave
         * identically for shell quoting inside drop actions. */
        char decoded[sizeof(g_drop_action)];
        snprintf(decoded, sizeof(decoded), "%s", val);
        decode_entities(decoded);
        snprintf(g_drop_action, sizeof(g_drop_action), "%s", decoded);
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
    e->parent = parent;
    if (parent && parent->n_children < MAX_CHILDREN) parent->children[parent->n_children++] = e;

    for (;;) {
        skip_ws(&p);
        if (*p == '/' && p[1] == '>') { p += 2; return p; }
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
        if (attr[0]) apply_attr(e, attr, val);
    }

    for (;;) {
        skip_ws(&p);
        if (!*p) return p;
        if (p[0] == '<' && p[1] == '/') {
            const char *end = strchr(p, '>');
            return end ? end + 1 : p + strlen(p);
        }
        p = parse_element(p, e);
    }
}

static Elem *parse_chtpm(const char *path) {
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
    const char *p = buf;
    Elem *root = NULL;
    while (*p) {
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
/* Forward declaration - real definition (with its own X11/Xft section
 * header comment) lives further down this file. Needed here because a
 * reparse that disarms a cli_io field mid-type must also release any
 * real XGrabKeyboard that field's own arm took (see activate_focused()/
 * default_cli_io_handle_key()'s own real grab-keyboard fix) - leaving
 * an exclusive keyboard grab held after silently disarming would lock
 * ALL keyboard input house-wide to this one (now non-typing) window
 * until it closes, a real, much worse bug than the one being fixed. */
static Display *dpy;
/* Forward declaration - real definition (with its own header comment)
 * lives in the generic sidebar+panel scroll section further down.
 * Needed here so a reparse (new manager content) can auto-scroll the
 * message list to the newest content, same real "always show the
 * latest" convention any chat UI needs - see this function's own body
 * below for where it's used. */
static int g_default_scrolllist_scroll;
static int reparse_chtpm_if_changed(void) {
    if (!g_chtpm_path[0]) return 0;
    struct stat st;
    if (stat(g_chtpm_path, &st) != 0) return 0;
    if (st.st_mtim.tv_sec == g_chtpm_mtime.tv_sec && st.st_mtim.tv_nsec == g_chtpm_mtime.tv_nsec) return 0;
    g_chtpm_mtime = st.st_mtim;
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
     * nothing was actually armed/grabbed. */
    if (g_default_input_elem) XUngrabKeyboard(dpy, CurrentTime);
    g_default_input_elem = NULL;
    g_n_elems = 0;
    Elem *new_window = parse_chtpm(g_chtpm_path);
    if (!new_window) return 0;
    g_window = new_window;
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
    g_default_scrolllist_scroll = 1000000;
    return 1;
}

/* ---------- X11/Xft ---------- */
/* dpy itself is forward-declared earlier, right after g_default_input_elem
 * (see that comment for why) - defining it again here would conflict. */
static Window win;
static int screen;
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
static int g_quit = 0;
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
/* REAL, NEW 2026-08-29 (direct instruction: "make it optional from
 * .pdl, can open immediately, or wait for second click") - runtime-
 * configurable, not hardcoded, per this house's own standing rule
 * (real PDL config beats a baked-in constant - see hq_ui.pdl's own
 * font_scale/focus_grab keys, same file, same real key=value parser
 * shape, reused not reinvented). Default is the new two-step behavior
 * (1); set `click_two_step=0` in #.desktop/hq_ui.pdl to restore the
 * old single-click-activates "auto" behavior house-wide. */
static int g_click_two_step = 1;
static int click_focus_then_activate(Elem *hit) {
    if (!hit) return 0;
    if (!g_click_two_step) return 1;
    if (hit->nav_index <= 0) return 1;
    if (g_focus_nav != hit->nav_index) { g_focus_nav = hit->nav_index; return 0; }
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
static int g_is_db_hq = 0;
/* LayDoc Gap 2: NULL = no ACTIVATE scope. Declared before draw_core
 * include so elem_cursor_prefix can show [^] on the scope root. */
static Elem *g_dbhq_active_scope_root = NULL;
/* PAUSED 2026-08-25 mid-migration - see the direct finding that stopped
 * this: stats-hq's real dashboard.chtpm DOES have a <tabbar> (real
 * session-timestamp tabs, e.g. "2026-08-13 22:53:37"), contradicting
 * this comment's own first-draft claim that it was tab-free generic
 * content. db-hq's own dbhq_*() tab-switching code matches tab clicks
 * against a FIXED TAB_LABELS[] array specific to db-hq's own Common
 * Events tabs - stats-hq's timestamp tabs would never match those
 * labels, so blindly aliasing g_is_stats_hq into g_is_db_hq's exact
 * path (the original plan here) would likely render fine but leave tab
 * click-switching silently broken. Flagged for the user before writing
 * any more of this - not resumed yet. */
static int g_is_stats_hq = 0;
/* REAL, NEW 2026-08-25 (Stage 2 palettes migration off the deprecated
 * standalone khtpm_hq_render.c - au11-hq/TPMOS-COMPLIANCE-DEBT.md /
 * khtpm-merge-how2.md). Palettes' own .chtpm is fully static content
 * (composed once by palettes_menu.sh, no live state/manager needed,
 * unlike db-hq/stats-hq) - rides g_is_db_hq=1 too for the shared
 * chrome/dispatch machinery, but g_is_palettes gates its own generic,
 * UNCONDITIONAL nav pass (see dbhq_assign_nav_indices()'s own
 * g_is_palettes branch) - deliberately NOT the nav_index==0-guarded
 * assign_generic_onclick_nav() pattern khtpm_hq_render.c used, since
 * that pattern needed clear_nav_indices() to avoid a real, live bug
 * found+fixed there this same session (stale nav_index staying non-zero
 * after frame 1, silently skipping every element on frame 2+). Palettes
 * has no tabbar/sidebar/panel-button structure to avoid double-counting
 * against, so unconditional reassignment is both simpler and immune to
 * that whole bug class by construction. */
static int g_is_palettes = 0;
/* REAL, NEW 2026-08-25 (Stage 3 bookmarks migration off khtpm_hq_render.c,
 * same debt entry as palettes above) - bookmarks is also a single
 * static panel of onClick-carrying <button> rows, no tabbar/sidebar,
 * so it needs the exact same layout-gate/sidebar_w/apply_css_deep/
 * generic-nav exceptions g_is_palettes already added - see every
 * `g_is_palettes` site below, now OR'd with this flag rather than
 * duplicated. Kept as its own flag (not folded into g_is_palettes)
 * since bookmarks also needs the chtpm-live-reload + armed-input
 * mechanism palettes has no use for. */
static int g_is_bookmarks = 0;
/* REAL, NEW 2026-08-30 - piececraft-hq board-view khtpm conversion,
 * direct instruction ("u should do it the same way the legacy chtpm
 * parser does it. if possible steal code/ops w/e u have to"). Real,
 * deliberate ISOLATION choice: unlike every other g_is_* mode flag
 * above, this one is handled by its own fully separate function
 * (run_pchq_board_mode(), see its own header comment near main()) that
 * returns before any of this file's shared X11-window/Elem/CSS setup
 * runs - zero shared state with the other 8 real modes, since this
 * mode is fundamentally a raw-pixel blit (bv_render_3d.c's own 3D
 * raymarch RGBA output), not an Elem/CSS-rendered window at all. Kept
 * as its own real, low-risk addition rather than threaded through the
 * existing giant shared main() - see PIECECRAFT-HQ-BOARD-KHTPM-
 * CONVERSION-2026-08-30.md for the real proof-of-concept this ports
 * (pchq_board_view_poc.c, already live-verified with a real
 * screenshot before this port). */
static int g_is_pchq_board = 0;
static double g_dbhq_font_scale = 1.0;
static int scaled(int base_px) {
    if (g_is_db_hq) return (int)(base_px * g_dbhq_font_scale + 0.5);
    return base_px;
}
/* REAL Stage 5 (2026-08-16, khtpm-merge-how2.md §5d) - shared, generic
 * draw_elem()/render_tree()/font_for() (was hand-rolled, per-app pixel
 * drawing - see khtpm_draw_core.c's own header comment). Included here
 * (after dpy/screen/cmap/gc/buf/xftdraw_buf/g_focus_nav are all
 * already declared above, which it needs). */
#include "khtpm_draw_core.c"
#define ROW_H 24
#define CHROME_H 24

static CssSheet g_sheet;

/* REAL Stage 5 §5d.3 step 6 (2026-08-16, khtpm-merge-how2.md) - the
 * actual literal binary merge. This binary now serves BOTH the real
 * generic menu shape (entity-menu's own original job) AND taskbar-
 * settings' own real swatch-picker shape, selected by a real, data-
 * driven signal (`<window class="swatch-picker">`), matching wraith-
 * alpha's own real "one binary, behavior selected by loaded data"
 * shape - not zero-app-C, but genuinely ONE compiled binary, no
 * dlopen/plugin indirection. Set once in main() after parse_chtpm(). */
static pid_t g_swatch_mgr_pid = -1;
static unsigned g_swatch_action_seq = 0;

/* REAL, swatch-picker-only state - ported verbatim from taskbar-
 * settings' own real g_phase/g_chosen_bg_idx/g_chosen_fg_idx/
 * g_palette_hex/g_palette_name (khtpm_taskbar_settings_render.c,
 * kept as a real, documented per-mode exception - the 2-phase pick
 * is genuinely stateful UI interaction, not something the shared
 * dispatch()/assign_nav_and_layout() can express generically, same
 * real precedent as chat-hai's panel exception in Stage 3). Unused,
 * harmless, when g_is_swatch_picker is 0. */
#define SWATCH 34
#define SWATCH_GAP 8
#define SWATCH_COLS 6
static int g_phase = 0;
static int g_chosen_bg_idx = -1;
static int g_chosen_fg_idx = -1;
static const char *g_palette_hex[12];
static char g_palette_name_buf[12][32];
static const char *g_palette_name[12];

/* ======================================================================
 * REAL, db-hq-mode-only state + functions (§5d.10, 2026-08-16) - ported
 * from khtpm_hq_render.c, kept as its own real, documented mode branch
 * per the same precedent as the swatch-picker's own 2-phase pick state
 * above (a genuinely different window shape/interaction model, not
 * forced into the popup modes' shared shape). Harmless, unused, when
 * g_is_db_hq is 0. Reuses this file's own dpy/screen/cmap/gc/buf/
 * xftdraw_buf/win/g_win_x/g_win_y/g_house_root/g_window/g_nav/g_n_nav/
 * g_focus_nav/g_quit globals directly (same real names, same real
 * purpose, no duplication needed).
 * ====================================================================== */
#define DB_HQ_MAX_EVENTS 128
static char g_dbhq_events[DB_HQ_MAX_EVENTS][64];
static int g_dbhq_n_events = 0;
static int g_dbhq_selected_event = -1;
/* Task 6 (2026-08-26, direct instruction: Common Events needs a real
 * inline editor, "same as how entities events works... modeled off
 * rpgmaker mv/mz" - one dialog, sidebar list + editor panel together,
 * NOT a separate spawned window). True once a real common event has
 * been selected and its own khtpm_events_hq_manager.+x instance is
 * live, retargeting the SAME g_evhq_* globals/functions events-hq
 * already uses for entities - see dbhq_ce_open() below. */
static int g_evhq_picker_open; /* real definition (with initializer) is later in the file, near g_evhq_picker_type - this tentative redeclaration just makes it visible to dbhq_handle_click(), which is defined earlier */
static int g_evhq_picker_type; /* REAL, NEW 2026-08-29 - same tentative-redeclaration pattern as g_evhq_picker_open just above, needed for dbhq_handle_click()'s own real fix (mouse click focus-sync) */
static int g_evhq_picker_focus;
static int g_evhq_active_field;
static int g_dbhq_ce_editing = 0;
static char g_dbhq_ce_name[128] = "";
static int g_dbhq_ce_needs_rebuild = 1; /* see dbhq_ce_inject_panel()'s own header comment */
static char g_dbhq_events_state_path[PATH_BUF];
static time_t g_dbhq_events_state_mtime = 0;
static char g_dbhq_action_path[PATH_BUF];

/* REAL, NEW 2026-08-25 (bookmarks manager port) - bookmarks' own state
 * is name+path PAIRS, not single strings, and paths in this house run
 * well past g_dbhq_events[][64]'s buffer - a separate, correctly-sized
 * pair array, not a reuse of db-hq/stats-hq's own. Per-pal (unlike
 * g_dbhq_events_state_path, which is house-wide), derived from
 * g_package_dir at init. */
#define BM_MAX_ROWS 64
static char g_bm_names[BM_MAX_ROWS][256];
static char g_bm_paths[BM_MAX_ROWS][PATH_BUF];
static int g_bm_n_rows = 0;
static char g_bm_state_path[PATH_BUF];
static time_t g_bm_state_mtime = 0;
/* the panel's 4 static children (title, hint, New+ button, Open Pal
 * Folder button), captured once at init so dbhq_inject_bookmark_items()
 * can rebuild panel->children[] as [title, hint, ...rows, new+, open]
 * on every reload without losing them - see that function's own header. */
static Elem *g_bm_static_title = NULL;
static Elem *g_bm_static_hint = NULL;
static Elem *g_bm_static_newplus = NULL;
static Elem *g_bm_static_openfolder = NULL;

/* REAL, NEW 2026-08-25 (palettes manager port, same shape as bookmarks'
 * own g_bm_* block just above) - palettes_manager.c publishes
 * `emoji<TAB>label<TAB>sprite_dir_or_empty` rows; the renderer chunks
 * them into <row class="pal-grid-row"> blocks of PAL_COLS tiles each,
 * same visual shape palettes_menu.sh's own emit_tiles_matrix() used.
 * Column count/wide-class stay a renderer-side presentation constant
 * (not published data) - genuinely a layout decision, not business
 * logic the manager needs to own. */
#define PAL_MAX_TILES 512
static char g_pal_emoji[PAL_MAX_TILES][32];
static char g_pal_label[PAL_MAX_TILES][256];
static char g_pal_sprite[PAL_MAX_TILES][PATH_BUF];
static int g_pal_n_tiles = 0;
static char g_pal_state_path[PATH_BUF];
static time_t g_pal_state_mtime = 0;
static off_t g_pal_state_size = -1;
/* REAL FIX 2026-08-28 (upgrade over the size-only check above) - two
 * different real rmmv tabs (e.g. B and C) can publish the SAME total
 * byte count (256 lines each, "b kind 7,3" and "c kind 7,3" are the
 * identical length) - size alone can miss a real B<->C switch the
 * exact same way raw mtime already missed same-second switches. A
 * real FNV-1a content checksum catches any actual byte difference
 * regardless of length coincidence, still cheap for a file this small
 * (a few KB at most). */
static unsigned long dbhq_file_checksum(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    unsigned long h = 2166136261UL;
    int c;
    while ((c = fgetc(f)) != EOF) { h ^= (unsigned long)c; h *= 16777619UL; }
    fclose(f);
    return h;
}
static unsigned long g_pal_state_checksum = 0;
static char g_pal_category[64];
/* REAL FIX 2026-08-27 (direct instruction: "flag hardcoded things in
 * parser... they are supposed to use generic .pdl read functions" -
 * this used to be `strcmp(g_pal_category, "elements") == 0` at the
 * single real call site below. Now a real value read from the
 * manager's own published `palettes-<category>_layout.txt` (a real
 * "wide=0|1" line, sourced from pallets.pdl's own real WIDE column -
 * see palettes_manager.c's publish_layout_flag()) - zero hardcoded
 * category names anywhere in this file for this decision now. Read
 * once per category load (layout rarely changes; not worth mtime-
 * gating on every redraw like the tile content itself). */
static int g_pal_layout_wide = 0;
static Elem *g_pal_static_title = NULL;
static Elem *g_pal_static_hint = NULL;
static int g_pal_forced_h = 0;

/* REAL, NEW 2026-08-29, direct live report ("nothing happened when i
 * tried it" - the armed-brush flow had zero visible feedback, so a
 * click that only moved nav focus (this house's real two-step click
 * convention, see click_focus_then_activate) looked identical to one
 * that silently did nothing). Polls rmmv_armed.txt (written by
 * palettes_menu.sh's arm_rmmv(), cleared by tp_arm_placer_rmmv.c on
 * exit) and swaps the picker's own hint text between this and the
 * chtpm's real default, so arming state is always visibly true, not
 * assumed. g_pal_default_hint captured once from the real chtpm-parsed
 * label the first time the hint Elem is found - not hardcoded here,
 * so a future wording change to palettes-rmmv.chtpm's own <text> still
 * restores correctly. */
static char g_pal_default_hint[256] = "";
static char g_pal_armed_path[PATH_BUF] = "";
static unsigned long g_pal_armed_checksum = 0;
/* DEAD CODE, kept inert intentionally 2026-08-29 - this in-process
 * XGrabPointer/XQueryPointer-polling click-capture design was tried
 * and superseded same day (see tp_arm_placer_rmmv.c's own header for
 * the real reason: real hardware clicks were never visible to this
 * process either way, only to a real mapped XWayland surface - the
 * real fix is that file's own full-screen InputOnly window instead).
 * g_pal_rmmv_armed is never set to 1 anywhere anymore, so every branch
 * below gated on it (hq_dispatch_xevent's ButtonPress/KeyPress
 * handling, dbhq_rmmv_poll_pointer(), the shortened select() timeout
 * in hq_run_event_loop()) is real but permanently unreachable - left
 * in place rather than surgically removed under time pressure; safe
 * to delete in a future pass, not load-bearing for anything. */
static int g_pal_rmmv_armed = 0;

/* Real, generic tab/chooser options for the rmmv tile picker
 * (2026-08-27) - published by palettes_manager.c's own publish_rmmv_
 * options() from the SAME real tileset_registry.pdl, never hardcoded
 * here. Empty (n==0) for every other category - stat() on a path that
 * only rmmv ever writes just fails, no-op, same pattern g_pal_state_path
 * already uses. */
#define PAL_MAX_OPTS 32
static char g_pal_opt_tileset_key[PAL_MAX_OPTS][64];
static char g_pal_opt_tileset_label[PAL_MAX_OPTS][128];
static int g_pal_n_tilesets = 0;
/* Real A/B/C/D/E sheet-letter tabs (2026-08-28, per external review
 * correction) - published by the manager's own rmmv_tab_letter_for()
 * as "TAB|<letter>|<default category to switch to>" - the renderer
 * never groups a1..a5 itself, it only shows whatever real letters the
 * manager's own registry scan actually found. */
static char g_pal_opt_tab_letter[PAL_MAX_OPTS];
static char g_pal_opt_tab_cat[PAL_MAX_OPTS][16];
static int g_pal_n_tabs = 0;
static char g_pal_opt_dir_key[PAL_MAX_OPTS][32];
static char g_pal_opt_dir_label[PAL_MAX_OPTS][32];
static int g_pal_n_dirs = 0;
static char g_pal_active_dir[32] = "tilesets";
static Elem g_pal_dir_slots[PAL_MAX_OPTS];
static char g_pal_active_tileset[64] = "";
static char g_pal_active_category[16] = "";
static char g_pal_options_path[PATH_BUF];
static time_t g_pal_options_mtime = 0;
static off_t g_pal_options_size = -1;
static unsigned long g_pal_options_checksum = 0;

/* REAL, ported 2026-08-25 (live request: figure out scrolling for the
 * palette grid) - verbatim port of khtpm_hq_render.c's own real,
 * live-verified scroll mechanism (that file is deleted now, recovered
 * from git history at commit 0dbcfcc^ for this port - NOT reinvented).
 * Checked chat-hai's own "scroll" first per direct question - that's a
 * different mechanism entirely (auto-scroll-to-latest-message, no user-
 * controlled position/thumb), not reusable here. This one is a real,
 * user-controlled row-scroll: Page_Up/Page_Down, mouse wheel, and a
 * drawn scrollbar thumb - full-row steps only (no partial-row clipping
 * engine exists), rows outside the visible window get zeroed w/h so
 * they're simply not drawn AND not nav-numbered (assign_palettes_nav()
 * already skips w==0/h==0 elements - no change needed there). */
static int g_pal_scroll = 0;
static int g_pal_has_grid = 0;
static int g_pal_total_rows = 0, g_pal_visible_rows = 1;
static int g_pal_track_x, g_pal_track_y, g_pal_track_w, g_pal_track_h;
static int g_pal_thumb_y, g_pal_thumb_h;
/* REAL, NEW 2026-08-25 (live report: "the thumb for mouse isn't working
 * yet") - the ported khtpm_hq_render.c mechanism only ever DREW the
 * thumb and scrolled via wheel/Page keys; clicking/dragging the thumb
 * itself was never wired to anything in that file either (confirmed by
 * reading its own recovered source before writing this) - a real,
 * separate gap this house never had a fix for, not a porting mistake. */
static int g_pal_thumb_dragging = 0;
/* REAL, NEW 2026-08-25 (live report: "no up down nav yet (near thumb)")
 * - real up/down arrow buttons at the track's own two ends. */
static int g_pal_arrow_h = 0;
/* REAL, NEW 2026-08-25 (live instruction: "they need to be numbered
 * (1 and 2), with nav feature for accessibility / disabled") - the
 * up/down scroll arrows are now real Elems (synthetic storage, same
 * "outside the parsed tree" pattern g_dbhq_close_elem_storage already
 * uses), not raw pixel draws - they get a real nav_index (and so a real
 * keyboard Enter/digit-jump path) via the SAME draw_elem()/dbhq_
 * activate_elem() machinery every other tile uses, and go through a
 * real disabled state (excluded from nav, dimmed) at the scroll min/max
 * instead of silently doing nothing. */
static Elem g_pal_arrow_up_storage;
static Elem g_pal_arrow_down_storage;
static Elem *g_pal_arrow_up = &g_pal_arrow_up_storage;
static Elem *g_pal_arrow_down = &g_pal_arrow_down_storage;
static int g_pal_arrow_up_disabled = 0, g_pal_arrow_down_disabled = 0;

static int elem_has_class(Elem *e, const char *cls) {
    for (int i = 0; i < e->n_classes; i++)
        if (strcmp(e->classes[i], cls) == 0) return 1;
    return 0;
}

static const char *DB_HQ_TAB_LABELS[] = {
    "Actors", "Classes", "Skills", "Items", "Weapons", "Armors",
    "Enemies", "Troops", "States", "Animations", "Tilesets",
    "Common Events", "System", "Types", "Terms"
};
#define DB_HQ_N_TABS 15
#define DB_HQ_COMMON_EVENTS_TAB 11
#define DB_HQ_TERMS_TAB 14
#define DB_HQ_ACTORS_TAB 0
#define DB_HQ_CLASSES_TAB 1
#define DB_HQ_SKILLS_TAB 2
#define DB_HQ_ITEMS_TAB 3
#define DB_HQ_WEAPONS_TAB 4
#define DB_HQ_ARMORS_TAB 5
#define DB_HQ_ENEMIES_TAB 6
#define DB_HQ_TROOPS_TAB 7
#define DB_HQ_STATES_TAB 8
#define DB_HQ_ANIMATIONS_TAB 9
#define DB_HQ_TILESETS_TAB 10
#define DB_HQ_SYSTEM_TAB 12
#define DB_HQ_TYPES_TAB 13
static int g_dbhq_current_tab = 0; /* Actors — must match nav [1]; CE was a lie */
static char g_dbhq_terms_state_path[PATH_BUF];
static char g_dbhq_actors_state_path[PATH_BUF];
#define DB_HQ_MAX_ACTORS 64
typedef struct {
    int id;
    char name[64];
    char nickname[64];
    char class_name[64];
    int init_lv, max_lv;
    char profile[160];
    char face[64], character[64], battler[64];
    char weapon[64], shield[64], head[64], body[64], accessory[64];
    int mhp, mmp, atk, defn, mat, mdf, agi, luk;
    char note[160];
} DbhqActor;
static DbhqActor g_dbhq_actors[DB_HQ_MAX_ACTORS];
static int g_dbhq_n_actors;
static int g_dbhq_selected_actor;
static time_t g_dbhq_actors_mtime;
static Elem g_dbhq_actor_panel_slots[MAX_CHILDREN];
/* Real, generic "which db-hq tabs actually have real backing data"
 * check (2026-08-28) - replaces 3 separate hardcoded `== DB_HQ_COMMON_
 * EVENTS_TAB` gates (layout, sidebar population, placeholder-vs-real
 * dispatch) with one real registry. Common Events and Terms are both
 * real today (real managers publishing real state files); the other
 * 13 tabs still correctly fall through to the generic "(coming soon)"
 * placeholder. Adding a NEW real tab later (per the events/db/
 * networking delegation doc's own Task 2) means adding ONE line here,
 * not re-finding and editing 3 separate gate sites again. */
static int dbhq_tab_is_real(int tab) {
    return tab == DB_HQ_COMMON_EVENTS_TAB || tab == DB_HQ_TERMS_TAB || tab == DB_HQ_ACTORS_TAB
        || tab == DB_HQ_CLASSES_TAB || tab == DB_HQ_SKILLS_TAB
        || tab == DB_HQ_WEAPONS_TAB || tab == DB_HQ_ARMORS_TAB
        || tab == DB_HQ_ENEMIES_TAB || tab == DB_HQ_TROOPS_TAB
        || tab == DB_HQ_STATES_TAB || tab == DB_HQ_ANIMATIONS_TAB
        || tab == DB_HQ_TILESETS_TAB || tab == DB_HQ_ITEMS_TAB
        || tab == DB_HQ_SYSTEM_TAB || tab == DB_HQ_TYPES_TAB;
}

static int g_dbhq_focus_grab_enabled = 0;
static int g_dbhq_chrome_h = 26;
static Elem g_dbhq_close_elem_storage;
static Elem *g_dbhq_close_elem = &g_dbhq_close_elem_storage;
static int g_dbhq_close_x, g_dbhq_close_y, g_dbhq_close_w, g_dbhq_close_h;
static int g_dbhq_digit_accum = 0;
static char g_dbhq_last_key_label[32] = "";
static int g_dbhq_has_real_focus = 0;
/* REAL FIX 2026-08-16, direct live report ("moved it up 2 high and one
 * is stuck" - a WM-managed window dragged above the taskbar header strip
 * can end up under/behind it, effectively unreachable/stuck). Clamp
 * drag's y to never go above this, for db-hq/events-hq/chat-hai alike -
 * matches the header strip's own real height + a small margin. */
/* REAL FIX (2026-08-17, live report: "mutaclysm still not moved 50
 * down (overlaps header still)"): the real taskbar header strip
 * occupies y=50 to y=86 (36px tall, confirmed live via xwininfo) - it
 * STARTS at y=50, it doesn't END there. A floor of 50 put windows
 * right at the header's own top edge, still fully overlapping it. Real
 * floor is the header's own bottom edge + a small margin. */
#define WM_MANAGED_DRAG_MIN_Y 90
static int g_dbhq_dragging = 0;
static int g_dbhq_drag_last_x = 0, g_dbhq_drag_last_y = 0;

static void dbhq_load_font_scale(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/hq_ui.pdl", g_house_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *val = eq + 1;
        char *nl = strchr(val, '\n');
        if (nl) *nl = '\0';
        if (strcmp(line, "font_scale") == 0) {
            double v = atof(val);
            if (v >= 0.5 && v <= 3.0) g_dbhq_font_scale = v;
        } else if (strcmp(line, "focus_grab") == 0) {
            g_dbhq_focus_grab_enabled = atoi(val) != 0;
        } else if (strcmp(line, "window_x") == 0) {
            g_win_x = atoi(val);
        } else if (strcmp(line, "window_y") == 0) {
            g_win_y = atoi(val);
        } else if (strcmp(line, "click_two_step") == 0) {
            g_click_two_step = atoi(val) != 0;
        }
    }
    fclose(f);
}

/* Returns 1 if the common-events list actually changed (caller should
 * re-inject sidebar items + redraw), 0 if unchanged - real, mtime-gated,
 * ported verbatim. The manager binary (khtpm_hq_manager.c, launched via
 * dbhq_launch_module() from the <module> tag) owns the real directory
 * scan; this only reads its published state file. */
/* REAL FIX 2026-08-28 (Terms tab wiring, part of the same dbhq_tab_is_
 * real() generalization) - this loader used to always read the ONE
 * hardcoded g_dbhq_events_state_path, correct only while Common Events
 * was the sole real tab. Now picks the real state file for whichever
 * REAL tab is currently active - Terms reuses this exact same generic
 * "one label per line" loader/g_dbhq_events[] array (it was already
 * generic, just never fed a second real source). A tracked "last
 * loaded path" forces one real reload on tab switch even if the two
 * files' mtimes happen to coincide - the mtime-gate alone can't detect
 * "same timestamp, different file". */
static char g_dbhq_events_last_path[PATH_BUF];
static int dbhq_load_common_events(void) {
    const char *path = (g_dbhq_current_tab == DB_HQ_TERMS_TAB) ? g_dbhq_terms_state_path : g_dbhq_events_state_path;
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    int path_changed = strcmp(path, g_dbhq_events_last_path) != 0;
    if (!path_changed && st.st_mtime == g_dbhq_events_state_mtime) return 0;
    snprintf(g_dbhq_events_last_path, sizeof(g_dbhq_events_last_path), "%s", path);
    g_dbhq_events_state_mtime = st.st_mtime;

    char tmp[DB_HQ_MAX_EVENTS][64];
    int n = 0;
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[128];
    while (n < DB_HQ_MAX_EVENTS && fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = '\0';
        if (len == 0) continue;
        snprintf(tmp[n], sizeof(tmp[0]), "%s", line);
        n++;
    }
    fclose(f);
    if (n == g_dbhq_n_events) {
        int same = 1;
        for (int i = 0; i < n; i++) {
            if (strcmp(tmp[i], g_dbhq_events[i]) != 0) { same = 0; break; }
        }
        if (same) return 0;
    }
    g_dbhq_n_events = n;
    for (int i = 0; i < n; i++)
        snprintf(g_dbhq_events[i], sizeof(g_dbhq_events[0]), "%s", tmp[i]);
    return 1;
}

/* REAL FIX 2026-08-25 (direct live report: "i was hoping it was more
 * human readable like before") - parses just the date/name portion
 * (before the first "|") out of a stats-hq raw data line, for a clean
 * sidebar label. Real db-hq's own g_dbhq_events[] never contains "|" in
 * a common-event NAME, so this is a no-op passthrough for real db-hq. */
static void dbhq_sidebar_label_for(int i, char *out, size_t outsz) {
    const char *src = g_dbhq_events[i];
    const char *bar = strchr(src, '|');
    size_t len = bar ? (size_t)(bar - src) : strlen(src);
    if (len >= outsz) len = outsz - 1;
    memcpy(out, src, len);
    out[len] = '\0';
}

/* REAL, NEW 2026-08-25 (bookmarks manager port) - mtime-gated read of
 * bookmarks_manager.c's own published `name<TAB>path` state file, same
 * convention as dbhq_load_common_events() above. Returns 1 if rows
 * actually changed (caller should re-inject + redraw), 0 if unchanged. */
static int dbhq_load_bookmark_state(void) {
    struct stat st;
    if (stat(g_bm_state_path, &st) != 0) return 0;
    if (st.st_mtime == g_bm_state_mtime) return 0;
    g_bm_state_mtime = st.st_mtime;

    g_bm_n_rows = 0;
    FILE *f = fopen(g_bm_state_path, "r");
    if (!f) return 1;
    char line[PATH_BUF];
    while (g_bm_n_rows < BM_MAX_ROWS && fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = '\0';
        if (len == 0) continue;
        char *tab = strchr(line, '\t');
        if (!tab) continue;
        *tab = '\0';
        snprintf(g_bm_names[g_bm_n_rows], sizeof(g_bm_names[0]), "%s", line);
        snprintf(g_bm_paths[g_bm_n_rows], sizeof(g_bm_paths[0]), "%s", tab + 1);
        g_bm_n_rows++;
    }
    fclose(f);
    return 1;
}

/* REAL, NEW 2026-08-25 (bookmarks manager port) - rebuilds panel-
 * >children[] as [title, hint, ...bookmark rows..., New+, Open Folder]
 * from the 4 captured static elems + g_bm_names/g_bm_paths. Same
 * elem_new()-per-row shape dbhq_inject_sidebar_items() already uses for
 * db-hq/stats-hq's own dynamic sidebar - not a new pattern. */
static Elem *dbhq_bm_row_factory(void *row, void *ctx) {
    int i = (int)(intptr_t)row;
    Elem *e;
    (void)ctx;
    if (i < 0 || i >= g_bm_n_rows) return NULL;
    e = elem_new("button");
    snprintf(e->classes[0], sizeof(e->classes[0]), "bm-bookmark");
    e->n_classes = 1;
    snprintf(e->label, sizeof(e->label), "%s  -  %s", g_bm_names[i], g_bm_paths[i]);
    snprintf(e->onclick, sizeof(e->onclick), "open:%s", g_bm_paths[i]);
    return e;
}

static void dbhq_inject_bookmark_items(Elem *panel) {
    if (!panel) return;
    panel->n_children = 0;
    if (g_bm_static_title && panel->n_children < MAX_CHILDREN) panel->children[panel->n_children++] = g_bm_static_title;
    if (g_bm_static_hint && panel->n_children < MAX_CHILDREN) panel->children[panel->n_children++] = g_bm_static_hint;
    {
        void *rows[BM_MAX_ROWS];
        int i;
        for (i = 0; i < g_bm_n_rows; i++) rows[i] = (void *)(intptr_t)i;
        elem_inject_loop(panel, rows, g_bm_n_rows, dbhq_bm_row_factory, NULL);
    }
    if (g_bm_static_newplus && panel->n_children < MAX_CHILDREN) panel->children[panel->n_children++] = g_bm_static_newplus;
    if (g_bm_static_openfolder && panel->n_children < MAX_CHILDREN) panel->children[panel->n_children++] = g_bm_static_openfolder;
}

/* REAL, NEW 2026-08-25 (palettes manager port) - mtime-gated read of
 * palettes_manager.c's own published `emoji<TAB>label<TAB>sprite_dir`
 * state file, same convention as dbhq_load_bookmark_state() above. */
static int dbhq_load_palette_state(void) {
    struct stat st;
    if (stat(g_pal_state_path, &st) != 0) return 0;
    /* REAL FIX 2026-08-28 (live report: "have to press abc tab multiple
     * (2/3 times) to get it to change") - st_mtime has only ONE-SECOND
     * resolution on this filesystem. Clicking through tabs faster than
     * a real second apart makes the manager's rewrite land on the SAME
     * mtime as the previous one, so this gate silently treated a real
     * content change as "nothing changed" - the renderer only actually
     * caught up once enough real wall-clock time (or one more click,
     * landing in a later second) had passed. Real fix: also compare
     * file SIZE, which almost always differs between two genuinely
     * different real publishes even within the same second - cheap
     * (already have the stat() result), no manager-side change needed. */
    if (st.st_mtime == g_pal_state_mtime && st.st_size == g_pal_state_size) {
        unsigned long cksum = dbhq_file_checksum(g_pal_state_path);
        if (cksum == g_pal_state_checksum) return 0;
        g_pal_state_checksum = cksum;
    } else {
        g_pal_state_checksum = dbhq_file_checksum(g_pal_state_path);
    }
    g_pal_state_mtime = st.st_mtime;
    g_pal_state_size = st.st_size;

    g_pal_n_tiles = 0;
    FILE *f = fopen(g_pal_state_path, "r");
    if (!f) return 1;
    char line[PATH_BUF];
    while (g_pal_n_tiles < PAL_MAX_TILES && fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = '\0';
        if (len == 0) continue;
        char *tab1 = strchr(line, '\t');
        if (!tab1) continue;
        *tab1 = '\0';
        char *tab2 = strchr(tab1 + 1, '\t');
        if (!tab2) continue;
        *tab2 = '\0';
        int i = g_pal_n_tiles;
        snprintf(g_pal_emoji[i], sizeof(g_pal_emoji[0]), "%s", line);
        snprintf(g_pal_label[i], sizeof(g_pal_label[0]), "%s", tab1 + 1);
        snprintf(g_pal_sprite[i], sizeof(g_pal_sprite[0]), "%s", tab2 + 1);
        g_pal_n_tiles++;
    }
    fclose(f);
    return 1;
}

/* Real, generic loader for rmmv_options.txt (2026-08-27, tile-picker UI
 * pass) - same mtime-gate shape as dbhq_load_palette_state() above.
 * Populates zero hardcoded tilesets/categories - whatever the manager
 * actually published from the real registry, nothing more. */
static int dbhq_load_palette_options(void) {
    struct stat st;
    if (!g_pal_options_path[0] || stat(g_pal_options_path, &st) != 0) return 0;
    /* REAL FIX 2026-08-28 - same real same-second-mtime staleness bug
     * (+ same-size coincidence risk) as dbhq_load_palette_state()'s own
     * header comment describes - same real content-checksum fix. */
    if (st.st_mtime == g_pal_options_mtime && st.st_size == g_pal_options_size) {
        unsigned long cksum = dbhq_file_checksum(g_pal_options_path);
        if (cksum == g_pal_options_checksum) return 0;
        g_pal_options_checksum = cksum;
    } else {
        g_pal_options_checksum = dbhq_file_checksum(g_pal_options_path);
    }
    g_pal_options_mtime = st.st_mtime;
    g_pal_options_size = st.st_size;

    g_pal_n_tilesets = 0;
    g_pal_n_tabs = 0;
    g_pal_n_dirs = 0;
    g_pal_active_tileset[0] = '\0';
    g_pal_active_category[0] = '\0';
    snprintf(g_pal_active_dir, sizeof(g_pal_active_dir), "tilesets");
    FILE *f = fopen(g_pal_options_path, "r");
    if (!f) return 1;
    char line[PATH_BUF];
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = '\0';
        char *p1 = strchr(line, '|');
        if (!p1) continue;
        *p1 = '\0';
        char *rest = p1 + 1;
        if (strcmp(line, "ACTIVE_TILESET") == 0) {
            snprintf(g_pal_active_tileset, sizeof(g_pal_active_tileset), "%s", rest);
        } else if (strcmp(line, "ACTIVE_CATEGORY") == 0) {
            snprintf(g_pal_active_category, sizeof(g_pal_active_category), "%s", rest);
        } else if (strcmp(line, "ACTIVE_DIR") == 0) {
            snprintf(g_pal_active_dir, sizeof(g_pal_active_dir), "%s", rest);
        } else if (strcmp(line, "DIR") == 0) {
            char *p2 = strchr(rest, '|');
            if (!p2) continue;
            *p2 = '\0';
            if (g_pal_n_dirs < PAL_MAX_OPTS) {
                snprintf(g_pal_opt_dir_key[g_pal_n_dirs], sizeof(g_pal_opt_dir_key[0]), "%s", rest);
                snprintf(g_pal_opt_dir_label[g_pal_n_dirs], sizeof(g_pal_opt_dir_label[0]), "%s", p2 + 1);
                g_pal_n_dirs++;
            }
        } else if (strcmp(line, "TAB") == 0) {
            char *p2 = strchr(rest, '|');
            if (!p2 || p2 != rest + 1) continue; /* letter is always exactly 1 char */
            char letter = rest[0];
            char *defcat = p2 + 1;
            if (g_pal_n_tabs < PAL_MAX_OPTS) {
                g_pal_opt_tab_letter[g_pal_n_tabs] = letter;
                snprintf(g_pal_opt_tab_cat[g_pal_n_tabs], sizeof(g_pal_opt_tab_cat[0]), "%s", defcat);
                g_pal_n_tabs++;
            }
        } else if (strcmp(line, "TILESET") == 0) {
            char *p2 = strchr(rest, '|');
            if (!p2) continue;
            *p2 = '\0';
            if (g_pal_n_tilesets < PAL_MAX_OPTS) {
                snprintf(g_pal_opt_tileset_key[g_pal_n_tilesets], sizeof(g_pal_opt_tileset_key[0]), "%s", rest);
                snprintf(g_pal_opt_tileset_label[g_pal_n_tilesets], sizeof(g_pal_opt_tileset_label[0]), "%s", p2 + 1);
                g_pal_n_tilesets++;
            }
        }
    }
    fclose(f);
    return 1;
}

/* REAL, NEW 2026-08-25 (live request: "update chemistry view thru
 * layout, not hardcoded") - column count used to be a literal 4/10
 * picked by hand to roughly match what the old bash emit_tiles_matrix()
 * used. Now genuinely layout-driven: derived from the REAL CSS tile
 * width (.pal-tile / .pal-tile.pal-wide, whichever this category uses -
 * so editing palettes-*.css's own width alone reflows the grid, no code
 * change needed) and the real window content width (window's own CSS
 * width if set, else the same default_w formula dbhq_layout_pass() uses
 * - kept in sync with that function on purpose, see its own comment).
 * If a future category's CSS gives tiles a different width, this
 * recomputes cols on its own instead of needing a new hardcoded number
 * added here. */
static int dbhq_pal_cols_for(int wide) {
    CssStyle win_st; css_style_init(&win_st);
    css_compute_style(&g_sheet, "window", NULL, NULL, 0, 0, &win_st);
    int window_w = win_st.has_width ? win_st.width : scaled(900);

    char classes[2][32];
    int n_classes = 0;
    snprintf(classes[n_classes++], sizeof(classes[0]), "pal-tile");
    if (wide) snprintf(classes[n_classes++], sizeof(classes[0]), "pal-wide");
    CssStyle tile_st; css_style_init(&tile_st);
    css_compute_style(&g_sheet, "button", NULL, classes, n_classes, 0, &tile_st);
    int tile_w = tile_st.has_width ? tile_st.width : scaled(48);

    CssStyle row_st; css_style_init(&row_st);
    char row_cls[1][32]; snprintf(row_cls[0], sizeof(row_cls[0]), "pal-grid-row");
    css_compute_style(&g_sheet, "row", NULL, row_cls, 1, 0, &row_st);
    int gap = row_st.has_gap ? row_st.gap : scaled(4);

    int margin = scaled(8), padding = scaled(12);
    int content_w = window_w - 2 * margin - 2 * padding;
    int cols = (content_w + gap) / (tile_w + gap);
    return cols > 0 ? cols : 1;
}

/* REAL BUG FIX 2026-08-28 (live crash: SIGSEGV in dbhq_inject_palette_
 * tiles(), confirmed via gdb backtrace after 2-3 real tab/tileset
 * switches) - this function used the SHARED, never-recycled elem_new()/
 * g_pool[MAX_ELEMS=512] for every row AND every tile, exactly the
 * failure mode reusable_slot()'s own header comment already documents
 * ("a long enough real session exhausts it... elem_new() returns NULL,
 * guarded call sites just skip adding content" - except THIS function's
 * call sites were NOT guarded against a NULL return, so it crashed
 * instead of silently going blank). A single real switch to a non-
 * autotile sheet (e.g. World_B.png = 256 real 1x1 tiles) already uses
 * ~290 pool slots in ONE inject; a second switch exhausted the whole
 * 512-slot pool outright. Real fix, same pattern already proven for
 * db-hq's sidebar/panel/event-list (g_dbhq_sidebar_slots/g_dbhq_panel_
 * slots/g_evhq_cmd_slots): dedicated, generously-sized, NEVER-freed
 * arrays reused via reusable_slot() every rebuild instead of
 * allocating fresh Elems from the shared pool each time. */
static Elem g_pal_row_slots[64];
static Elem g_pal_tile_slots[PAL_MAX_TILES];
static Elem g_pal_tab_slots[PAL_MAX_OPTS];
static Elem g_pal_tileset_slots[PAL_MAX_OPTS];

static void dbhq_inject_palette_tiles(Elem *panel) {
    if (!panel) return;
    int wide = g_pal_layout_wide; /* REAL FIX 2026-08-27 - was hardcoded strcmp(g_pal_category, "elements"), see g_pal_layout_wide's own header comment */
    int cols = dbhq_pal_cols_for(wide);
    g_pal_scroll = 0; /* new content, new scroll - avoids a stale offset past the new max (same habit khtpm_hq_render.c's own reload path used) */
    int next_row_slot = 0, next_tile_slot = 0, next_tab_slot = 0, next_tileset_slot = 0;

    panel->n_children = 0;
    if (g_pal_static_title && panel->n_children < MAX_CHILDREN) panel->children[panel->n_children++] = g_pal_static_title;
    if (g_pal_static_hint && panel->n_children < MAX_CHILDREN) panel->children[panel->n_children++] = g_pal_static_hint;

    /* www/img directory tabs (db-hq-shaped names), two rows wrap. */
    if (g_pal_n_dirs > 0) {
        const int per = 6;
        Elem *drow = NULL;
        int dslot = 0;
        for (int i = 0; i < g_pal_n_dirs && panel->n_children < MAX_CHILDREN; i++) {
            if (i % per == 0) {
                drow = reusable_slot(g_pal_row_slots, 64, next_row_slot++, "row");
                if (!drow) break;
                drow->parent = panel;
                snprintf(drow->classes[0], sizeof(drow->classes[0]), "pal-tab-row");
                drow->n_classes = 1;
                panel->children[panel->n_children++] = drow;
            }
            if (!drow || drow->n_children >= MAX_CHILDREN) continue;
            Elem *tab = reusable_slot(g_pal_dir_slots, PAL_MAX_OPTS, dslot++, "button");
            if (!tab) break;
            tab->parent = drow;
            snprintf(tab->classes[0], sizeof(tab->classes[0]), "pal-tab");
            tab->n_classes = 1;
            if (strcmp(g_pal_opt_dir_key[i], g_pal_active_dir) == 0) {
                snprintf(tab->classes[1], sizeof(tab->classes[1]), "pal-tab-active");
                tab->n_classes = 2;
            }
            snprintf(tab->label, sizeof(tab->label), "%s", g_pal_opt_dir_label[i]);
            snprintf(tab->onclick, sizeof(tab->onclick),
                     "exec:'%s/&.widgits/palettes/palettes_menu.sh' set-rmmv-dir '%s' '%s'",
                     g_house_root, g_package_dir, g_pal_opt_dir_key[i]);
            drow->children[drow->n_children++] = tab;
        }
    }

    /* Real A/B/C/D/E sheet-letter tab row (2026-08-27/28, per the
     * user's own rmmv-tiles mockup + external review correction that
     * tabs are real sheet LETTERS, not raw a1..a5 sub-category keys) -
     * built fresh each redraw from g_pal_opt_tab_letter/_cat (whatever
     * the manager's own rmmv_tab_letter_for() grouping actually found
     * in the real registry for the active tileset). Every other
     * category has n==0 here (options file never written), so this is
     * a no-op for them. Active highlight compares the CURRENT active
     * category's own letter-group, not a raw string match, so any
     * a1..a5 category active still lights up the single "A" tab. */
    if (g_pal_n_tabs > 0 && panel->n_children < MAX_CHILDREN &&
        (g_pal_active_dir[0] == '\0' || strcmp(g_pal_active_dir, "tilesets") == 0)) {
        char active_letter = g_pal_active_category[0] ? (g_pal_active_category[0] == 'a' ? 'A' : (char)toupper((unsigned char)g_pal_active_category[0])) : '\0';
        Elem *tabrow = reusable_slot(g_pal_row_slots, 64, next_row_slot++, "row");
        if (tabrow) {
        tabrow->parent = panel;
        snprintf(tabrow->classes[0], sizeof(tabrow->classes[0]), "pal-tab-row");
        tabrow->n_classes = 1;
        for (int i = 0; i < g_pal_n_tabs && tabrow->n_children < MAX_CHILDREN; i++) {
            Elem *tab = reusable_slot(g_pal_tab_slots, PAL_MAX_OPTS, next_tab_slot++, "button");
            if (!tab) break;
            tab->parent = tabrow;
            snprintf(tab->classes[0], sizeof(tab->classes[0]), "pal-tab");
            tab->n_classes = 1;
            if (g_pal_opt_tab_letter[i] == active_letter) {
                snprintf(tab->classes[1], sizeof(tab->classes[1]), "pal-tab-active");
                tab->n_classes = 2;
            }
            snprintf(tab->label, sizeof(tab->label), "%c", g_pal_opt_tab_letter[i]);
            /* Real 2026-08-28 fix - sends the TAB LETTER, not a resolved
             * category (g_pal_opt_tab_cat[i] is kept only for the
             * active-highlight comparison below; the manager itself now
             * resolves letter -> concrete a1/a2/... category from a
             * real directory scan, since which suffix backs a letter
             * can change independently of which letter is active). */
            snprintf(tab->onclick, sizeof(tab->onclick),
                     "exec:'%s/&.widgits/palettes/palettes_menu.sh' set-rmmv-tab '%s' '%c'",
                     g_house_root, g_package_dir, g_pal_opt_tab_letter[i]);
            tabrow->children[tabrow->n_children++] = tab;
        }
        panel->children[panel->n_children++] = tabrow;
        }
    }

    Elem *row = NULL;
    for (int i = 0; i < g_pal_n_tiles && panel->n_children < MAX_CHILDREN; i++) {
        if (i % cols == 0) {
            row = reusable_slot(g_pal_row_slots, 64, next_row_slot++, "row");
            if (!row) break;
            row->parent = panel;
            snprintf(row->classes[0], sizeof(row->classes[0]), "pal-grid-row");
            row->n_classes = 1;
            panel->children[panel->n_children++] = row;
        }
        if (!row || row->n_children >= MAX_CHILDREN) continue;
        Elem *tile = reusable_slot(g_pal_tile_slots, PAL_MAX_TILES, next_tile_slot++, "button");
        if (!tile) break;
        tile->parent = row;
        snprintf(tile->classes[0], sizeof(tile->classes[0]), "pal-tile");
        tile->n_classes = 1;
        if (wide) { snprintf(tile->classes[1], sizeof(tile->classes[1]), "pal-wide"); tile->n_classes = 2; }
        snprintf(tile->label, sizeof(tile->label), "%s", g_pal_label[i]);
        if (g_pal_sprite[i][0]) snprintf(tile->sprite, sizeof(tile->sprite), "%s", g_pal_sprite[i]);
        /* REAL FIX 2026-08-29 (TILE-SYSTEM-DESIGN.md §6 item 6, the
         * doc-audit pass's identified real gap): before this fix, EVERY
         * category's tile click - including rmmv - went through place()
         * with g_pal_emoji[i], which for rmmv holds a label string like
         * "a2 kind 3,1", not a real glyph. That sent garbage into the
         * FreeType emoji_gen_atlas pipeline, which is why "sets a real
         * current brush state on tile click" was still flagged pending
         * in TILE-SYSTEM-DESIGN.md's own §6 item 5 note. rmmv now arms
         * a real tileset/category/kind brush instead - g_pal_sprite[i]
         * is already the manager's own real per-kind sprite.csv cache
         * dir (publish_rmmv()), so no new rendering/compositing code is
         * needed here, only correct routing. */
        if (strcmp(g_pal_category, "rmmv") == 0) {
            /* REAL, NEW 2026-08-29 - the armed-click-capture window's
             * own rect is passed through so it can tile AROUND this
             * picker window (not over it) - see tp_arm_placer_rmmv.c's
             * own header for the full real design history/why. */
            snprintf(tile->onclick, sizeof(tile->onclick),
                     "exec:'%s/&.widgits/palettes/palettes_menu.sh' arm-rmmv '%s' '%s' '%s' '%s' '%d' '%d' '%d' '%d'",
                     g_house_root, g_pal_sprite[i], g_pal_active_tileset, g_pal_active_category, g_pal_label[i],
                     g_win_x, g_win_y, g_window->w, g_window->h);
        } else if (strcmp(g_pal_category, "debug") == 0) {
            /* REAL, NEW 2026-08-29 - debug_hq's own rows (publish_
             * debug() in palettes_manager.c) carry a real action string
             * in g_pal_emoji[i]: "toggle:<idx>", "clear", or "noop" for
             * plain debug.txt content-display rows. "noop" gets no
             * onclick at all - a real read-only row, not a dead button. */
            if (strncmp(g_pal_emoji[i], "toggle:", 7) == 0) {
                snprintf(tile->onclick, sizeof(tile->onclick),
                         "exec:'%s/&.widgits/palettes/palettes_menu.sh' debug-toggle '%s'",
                         g_house_root, g_pal_emoji[i] + 7);
            } else if (strcmp(g_pal_emoji[i], "clear") == 0) {
                snprintf(tile->onclick, sizeof(tile->onclick),
                         "exec:'%s/&.widgits/palettes/palettes_menu.sh' debug-clear", g_house_root);
            } else {
                tile->onclick[0] = '\0';
            }
        } else {
            snprintf(tile->onclick, sizeof(tile->onclick), "exec:'%s/&.widgits/palettes/palettes_menu.sh' place '%s'", g_house_root, g_pal_emoji[i]);
        }
        row->children[row->n_children++] = tile;
    }

    /* Real tileset chooser row (2026-08-27, "instead of opposite menu"
     * per direct correction - chooser sits at the BOTTOM of the panel,
     * after the tile grid, not top). Built from g_pal_opt_tileset_*
     * (whatever real "<key>.name" rows publish_rmmv_options() found),
     * same no-hardcoding shape as the tab row above. */
    /* Wrap tileset chooser at 4 per row (live: 6 prefixes, one row
     * only showed ~4). Same wrap we'll use for img-dir tabs. */
    if (g_pal_n_tilesets > 0 &&
        (g_pal_active_dir[0] == '\0' || strcmp(g_pal_active_dir, "tilesets") == 0)) {
        const int per = 4;
        Elem *chooserrow = NULL;
        for (int i = 0; i < g_pal_n_tilesets && panel->n_children < MAX_CHILDREN; i++) {
            if (i % per == 0) {
                chooserrow = reusable_slot(g_pal_row_slots, 64, next_row_slot++, "row");
                if (!chooserrow) break;
                chooserrow->parent = panel;
                snprintf(chooserrow->classes[0], sizeof(chooserrow->classes[0]), "pal-tileset-row");
                chooserrow->n_classes = 1;
                panel->children[panel->n_children++] = chooserrow;
            }
            if (!chooserrow || chooserrow->n_children >= MAX_CHILDREN) continue;
            Elem *opt = reusable_slot(g_pal_tileset_slots, PAL_MAX_OPTS, next_tileset_slot++, "button");
            if (!opt) break;
            opt->parent = chooserrow;
            snprintf(opt->classes[0], sizeof(opt->classes[0]), "pal-tileset-opt");
            opt->n_classes = 1;
            if (strcmp(g_pal_opt_tileset_key[i], g_pal_active_tileset) == 0) {
                snprintf(opt->classes[1], sizeof(opt->classes[1]), "pal-tileset-active");
                opt->n_classes = 2;
            }
            snprintf(opt->label, sizeof(opt->label), "%s", g_pal_opt_tileset_label[i]);
            snprintf(opt->onclick, sizeof(opt->onclick),
                     "exec:'%s/&.widgits/palettes/palettes_menu.sh' set-rmmv-tileset '%s' '%s'",
                     g_house_root, g_package_dir, g_pal_opt_tileset_key[i]);
            chooserrow->children[chooserrow->n_children++] = opt;
        }
    }

    /* REAL, NEW 2026-08-25 (same "thru layout, not hardcoded" request) -
     * window content height used to sit at db-hq's own fixed 600px
     * default regardless of how many rows this category's real cols
     * count produces - harmless for the old hardcoded 10/4-col grid
     * (roughly filled it), but a real dead-space regression once cols
     * itself became layout-derived (a wider window fits more per row,
     * so fewer rows, so a big empty gap below the last one). Computed
     * from the real row height/gap this category's own CSS declares,
     * not a second hardcoded number. */
    CssStyle row_st2; css_style_init(&row_st2);
    char row_cls2[1][32]; snprintf(row_cls2[0], sizeof(row_cls2[0]), "pal-grid-row");
    css_compute_style(&g_sheet, "row", NULL, row_cls2, 1, 0, &row_st2);
    int row_h = row_st2.has_height ? row_st2.height : scaled(56);
    int gap = row_st2.has_gap ? row_st2.gap : scaled(4);
    int rows = cols > 0 ? (g_pal_n_tiles + cols - 1) / cols : 0;
    /* Real tab/chooser rows add their own row heights - counted as
     * extra "rows" here rather than a second hardcoded height constant,
     * since they share the exact same row_h/gap CSS shape. */
    int extra_rows = (g_pal_n_dirs > 0 ? (g_pal_n_dirs + 5) / 6 : 0) + (g_pal_n_tabs > 0 ? 1 : 0) + (g_pal_n_tilesets > 0 ? (g_pal_n_tilesets + 3) / 4 : 0);
    rows += extra_rows;
    int hint_h = scaled(24);
    /* dbhq_layout_pass() always does `content_h = window->style.height -
     * tabbar_h`, unconditionally reserving tabbar_h even with no real
     * <tabbar> present (a pre-existing db-hq-mode quirk, not introduced
     * here) - style.height has to include that same amount back, or the
     * window ends up tabbar_h short of what was actually computed. */
    int tabbar_h = scaled(30);
    int margin = scaled(8), padding = scaled(12);
    int content_h = hint_h + rows * row_h + (rows > 0 ? (rows - 1) * gap : 0) + 2 * padding + margin + tabbar_h;
    /* REAL FIX 2026-08-25 (same pass that added scrolling) - fitting the
     * window to EVERY row defeats scrolling entirely (g_pal_visible_rows
     * would always equal g_pal_total_rows, max_scroll always 0). Cap at
     * a real, reasonable on-screen height instead - content taller than
     * that scrolls via the newly-ported g_pal_scroll mechanism, content
     * shorter than that still gets the real shrink-to-fit from the fix
     * above (chemistry's own 917px vs the old fixed-600 default). */
    int max_h = scaled(600);
    if (content_h > max_h) content_h = max_h;
    g_pal_forced_h = content_h > scaled(150) ? content_h : scaled(150);
}

static Elem g_dbhq_sidebar_slots[MAX_CHILDREN]; /* see reusable_slot()'s own header comment */

static void dbhq_inject_sidebar_items(Elem *sidebar) {
    if (!sidebar) return;
    sidebar->n_children = 0;
    int next_slot_index = 0;
    for (int i = 0; i < g_dbhq_n_events; i++) {
        Elem *item = reusable_slot(g_dbhq_sidebar_slots, MAX_CHILDREN, next_slot_index++, "item");
        if (!item) break; /* pool exhausted - stop, don't crash (see addbtn's own comment below) */
        item->parent = sidebar;
        snprintf(item->classes[0], sizeof(item->classes[0]), "data-item");
        item->n_classes = 1;
        if (g_is_stats_hq) {
            dbhq_sidebar_label_for(i, item->label, sizeof(item->label));
            /* REAL FIX - id holds the real index so click-matching
             * doesn't depend on the label (now just the date, not the
             * full raw line anymore) staying unique/stable - see the
             * "item" click branch below. */
            snprintf(item->id, sizeof(item->id), "%d", i);
        } else {
            snprintf(item->label, sizeof(item->label), "%s", g_dbhq_events[i]);
        }
        item->active = (i == g_dbhq_selected_event);
        if (sidebar->n_children < MAX_CHILDREN) sidebar->children[sidebar->n_children++] = item;
    }
    if (g_dbhq_n_events == 0 && !g_is_stats_hq) {
        Elem *item = reusable_slot(g_dbhq_sidebar_slots, MAX_CHILDREN, next_slot_index++, "item");
        if (!item) return;
        item->parent = sidebar;
        snprintf(item->label, sizeof(item->label), "(none yet)");
        if (sidebar->n_children < MAX_CHILDREN) sidebar->children[sidebar->n_children++] = item;
    }
    /* Task 6 (2026-08-26, direct instruction: Common Events had no way
     * to create a new one, unlike entity events' own +Add Command) -
     * reuses the SAME generic input:<file>|<postcmd> mechanism
     * bookmarks' own "New+" button already uses (dbhq_handle_key's
     * g_input_elem branch), no new popup/input machinery. The post
     * command only mkdir -p's a bare event_pkg dir - dbhq_load_common_
     * events()'s existing mtime-gated rescan (already proven live,
     * zero-recompile, this session) picks up the new directory on its
     * own; the manager itself creates page_1 with the real template the
     * first time the user clicks "+ New Page" inside it, same as an
     * entity's own first page - no scaffold format duplicated here. */
    if (!g_is_stats_hq && !g_is_palettes && !g_is_bookmarks) {
        Elem *addbtn = reusable_slot(g_dbhq_sidebar_slots, MAX_CHILDREN, next_slot_index++, "item");
        /* REAL BUG FIX 2026-08-26 (found via gdb, real SIGSEGV) -
         * elem_new() returns NULL when the shared MAX_ELEMS pool is
         * exhausted (it never recycles - a house-wide structural limit,
         * not new to this function). Task 6/7's own per-tick panel
         * rebuilds raised real pool pressure enough to hit this in
         * practice; guard here defensively rather than pretend the pool
         * is infinite. */
        if (!addbtn) return;
        addbtn->parent = sidebar;
        snprintf(addbtn->classes[0], sizeof(addbtn->classes[0]), "data-item"); addbtn->n_classes = 1;
        snprintf(addbtn->id, sizeof(addbtn->id), "ce-add-event");
        snprintf(addbtn->label, sizeof(addbtn->label), "+ Add Common Event");
        char target[PATH_BUF]; snprintf(target, sizeof(target), "%s/#.desktop/.dbhq_new_ce_name.txt", g_house_root);
        char post[900];
        snprintf(post, sizeof(post),
            "sh -c 'N=$(tail -1 \"%s\" | tr -d \"/\\r\\n\"); [ -n \"$N\" ] && mkdir -p \"%s/common_events/$N/event_pkg\"'",
            target, g_house_root);
        snprintf(addbtn->onclick, sizeof(addbtn->onclick), "input:%s|%s", target, post);
        if (sidebar->n_children < MAX_CHILDREN) sidebar->children[sidebar->n_children++] = addbtn;
    }
}

/* REAL FIX 2026-08-25 (direct live report: "i was hoping it was more
 * human readable like before") - stats-hq's own panel-population,
 * parallel to db-hq's single-text-field update, since stats-hq needs 5
 * separate itemized lines (matching the OLD template exactly: Session/
 * User Messages/AI Responses/Total Turns/Tool Calls+Delegation), not
 * one combined summary. Parses stats_hq_manager.c's own raw pipe-
 * delimited publish format (date|turns|user_msgs|ai_msgs|tools|pct). */

static void dbhq_actor_clear(DbhqActor *a) {
    memset(a, 0, sizeof(*a));
    a->init_lv = 1;
    a->max_lv = 99;
}

static void dbhq_actor_set_key(DbhqActor *a, const char *key, const char *val) {
    if (strcmp(key, "id") == 0) a->id = atoi(val);
    else if (strcmp(key, "name") == 0) snprintf(a->name, sizeof(a->name), "%s", val);
    else if (strcmp(key, "nickname") == 0) snprintf(a->nickname, sizeof(a->nickname), "%s", val);
    else if (strcmp(key, "class") == 0) snprintf(a->class_name, sizeof(a->class_name), "%s", val);
    else if (strcmp(key, "init_lv") == 0) a->init_lv = atoi(val);
    else if (strcmp(key, "max_lv") == 0) a->max_lv = atoi(val);
    else if (strcmp(key, "profile") == 0) snprintf(a->profile, sizeof(a->profile), "%s", val);
    else if (strcmp(key, "face") == 0) snprintf(a->face, sizeof(a->face), "%s", val);
    else if (strcmp(key, "character") == 0) snprintf(a->character, sizeof(a->character), "%s", val);
    else if (strcmp(key, "battler") == 0) snprintf(a->battler, sizeof(a->battler), "%s", val);
    else if (strcmp(key, "weapon") == 0) snprintf(a->weapon, sizeof(a->weapon), "%s", val);
    else if (strcmp(key, "shield") == 0) snprintf(a->shield, sizeof(a->shield), "%s", val);
    else if (strcmp(key, "head") == 0) snprintf(a->head, sizeof(a->head), "%s", val);
    else if (strcmp(key, "body") == 0) snprintf(a->body, sizeof(a->body), "%s", val);
    else if (strcmp(key, "accessory") == 0) snprintf(a->accessory, sizeof(a->accessory), "%s", val);
    else if (strcmp(key, "mhp") == 0) a->mhp = atoi(val);
    else if (strcmp(key, "mmp") == 0) a->mmp = atoi(val);
    else if (strcmp(key, "atk") == 0) a->atk = atoi(val);
    else if (strcmp(key, "def") == 0) a->defn = atoi(val);
    else if (strcmp(key, "mat") == 0) a->mat = atoi(val);
    else if (strcmp(key, "mdf") == 0) a->mdf = atoi(val);
    else if (strcmp(key, "agi") == 0) a->agi = atoi(val);
    else if (strcmp(key, "luk") == 0) a->luk = atoi(val);
    else if (strcmp(key, "note") == 0) snprintf(a->note, sizeof(a->note), "%s", val);
}

/* Parse house PDL: SECTION | KEY | VALUE  — ACTOR rows. No JSON. */
static int dbhq_load_actors(void) {
    const char *path = g_dbhq_actors_state_path;
    struct stat st;
    char fallback[PATH_BUF];
    if (stat(path, &st) != 0) {
        snprintf(fallback, sizeof(fallback), "%s/&.widgits/db-hq/data/actors.pdl", g_house_root);
        path = fallback;
        if (stat(path, &st) != 0) return 0;
    }
    if (st.st_mtime == g_dbhq_actors_mtime && g_dbhq_n_actors > 0) return 0;
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    DbhqActor tmp[DB_HQ_MAX_ACTORS];
    int n = 0;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len-1]=='\n' || line[len-1]=='\r')) line[--len] = '\0';
        if (len == 0 || line[0] == '-' || strncmp(line, "SECTION", 7) == 0) continue;
        char sec[64] = "", key[64] = "", val[256] = "";
        char *p = line;
        char *bar = strstr(p, "|");
        if (!bar) continue;
        *bar = '\0';
        snprintf(sec, sizeof(sec), "%s", p);
        p = bar + 1;
        while (*p == ' ') p++;
        bar = strstr(p, "|");
        if (!bar) continue;
        *bar = '\0';
        /* trim key */
        char *ke = bar - 1;
        while (ke > p && (*ke == ' ' || *ke == '\t')) { *ke = '\0'; ke--; }
        snprintf(key, sizeof(key), "%s", p);
        p = bar + 1;
        while (*p == ' ') p++;
        snprintf(val, sizeof(val), "%s", p);
        /* trim trailing space on sec */
        for (int i = (int)strlen(sec)-1; i>=0 && (sec[i]==' '||sec[i]=='\t'); i--) sec[i]='\0';
        if (strcmp(sec, "ACTOR") != 0) continue;
        if (strcmp(key, "id") == 0) {
            if (n >= DB_HQ_MAX_ACTORS) break;
            dbhq_actor_clear(&tmp[n]);
            dbhq_actor_set_key(&tmp[n], key, val);
            n++;
        } else if (n > 0) {
            dbhq_actor_set_key(&tmp[n-1], key, val);
        }
    }
    fclose(f);
    g_dbhq_actors_mtime = st.st_mtime;
    int same = (n == g_dbhq_n_actors);
    if (same) {
        for (int i = 0; i < n; i++) {
            if (memcmp(&tmp[i], &g_dbhq_actors[i], sizeof(DbhqActor)) != 0) { same = 0; break; }
        }
    }
    if (same) return 0;
    g_dbhq_n_actors = n;
    memcpy(g_dbhq_actors, tmp, sizeof(DbhqActor) * (size_t)n);
    if (g_dbhq_selected_actor < 0 && n > 0) g_dbhq_selected_actor = 0;
    if (g_dbhq_selected_actor >= n) g_dbhq_selected_actor = n > 0 ? n - 1 : -1;
    return 1;
}

static void dbhq_actor_sidebar_label(int i, char *out, size_t outsz) {
    snprintf(out, outsz, "%04d: %s", g_dbhq_actors[i].id, g_dbhq_actors[i].name);
}

static Elem *dbhq_actor_panel_row(Elem *panel, int *slot, const char *label) {
    Elem *e = reusable_slot(g_dbhq_actor_panel_slots, MAX_CHILDREN, (*slot)++, "button");
    if (!e) return NULL;
    e->parent = panel;
    snprintf(e->classes[0], sizeof(e->classes[0]), "data-item");
    e->n_classes = 1;
    snprintf(e->label, sizeof(e->label), "%s", label);
    snprintf(e->onclick, sizeof(e->onclick), "ACTIVATE");
    if (panel->n_children < MAX_CHILDREN) panel->children[panel->n_children++] = e;
    return e;
}

static void dbhq_inject_actors_panel(Elem *panel) {
    if (!panel) return;
    panel->n_children = 0;
    int slot = 0;
    if (g_dbhq_selected_actor < 0 || g_dbhq_selected_actor >= g_dbhq_n_actors) {
        Elem *t = reusable_slot(g_dbhq_actor_panel_slots, MAX_CHILDREN, slot++, "title");
        if (!t) return;
        t->parent = panel;
        snprintf(t->classes[0], sizeof(t->classes[0]), "block-title"); t->n_classes = 1;
        snprintf(t->label, sizeof(t->label), "Actor");
        if (panel->n_children < MAX_CHILDREN) panel->children[panel->n_children++] = t;
        dbhq_actor_panel_row(panel, &slot, "(select an actor)");
        return;
    }
    DbhqActor *a = &g_dbhq_actors[g_dbhq_selected_actor];
    char buf[256];
    Elem *t = reusable_slot(g_dbhq_actor_panel_slots, MAX_CHILDREN, slot++, "title");
    if (t) {
        t->parent = panel;
        snprintf(t->classes[0], sizeof(t->classes[0]), "block-title"); t->n_classes = 1;
        snprintf(t->label, sizeof(t->label), "Actor %04d", a->id);
        if (panel->n_children < MAX_CHILDREN) panel->children[panel->n_children++] = t;
    }
    snprintf(buf, sizeof(buf), "Name          %s", a->name); dbhq_actor_panel_row(panel, &slot, buf);
    snprintf(buf, sizeof(buf), "Nickname      %s", a->nickname); dbhq_actor_panel_row(panel, &slot, buf);
    snprintf(buf, sizeof(buf), "Class         %s", a->class_name); dbhq_actor_panel_row(panel, &slot, buf);
    snprintf(buf, sizeof(buf), "Initial Level %d", a->init_lv); dbhq_actor_panel_row(panel, &slot, buf);
    snprintf(buf, sizeof(buf), "Max Level     %d", a->max_lv); dbhq_actor_panel_row(panel, &slot, buf);
    snprintf(buf, sizeof(buf), "Profile       %s", a->profile); dbhq_actor_panel_row(panel, &slot, buf);
    snprintf(buf, sizeof(buf), "Face          %s", a->face[0] ? a->face : "(none)"); dbhq_actor_panel_row(panel, &slot, buf);
    snprintf(buf, sizeof(buf), "Character     %s", a->character[0] ? a->character : "(none)"); dbhq_actor_panel_row(panel, &slot, buf);
    snprintf(buf, sizeof(buf), "Battler       %s", a->battler[0] ? a->battler : "(none)"); dbhq_actor_panel_row(panel, &slot, buf);
    snprintf(buf, sizeof(buf), "Weapon        %s", a->weapon[0] ? a->weapon : "None"); dbhq_actor_panel_row(panel, &slot, buf);
    snprintf(buf, sizeof(buf), "Shield        %s", a->shield[0] ? a->shield : "None"); dbhq_actor_panel_row(panel, &slot, buf);
    snprintf(buf, sizeof(buf), "Head          %s", a->head[0] ? a->head : "None"); dbhq_actor_panel_row(panel, &slot, buf);
    snprintf(buf, sizeof(buf), "Body          %s", a->body[0] ? a->body : "None"); dbhq_actor_panel_row(panel, &slot, buf);
    snprintf(buf, sizeof(buf), "Accessory     %s", a->accessory[0] ? a->accessory : "None"); dbhq_actor_panel_row(panel, &slot, buf);
    snprintf(buf, sizeof(buf), "MHP  %d", a->mhp); dbhq_actor_panel_row(panel, &slot, buf);
    snprintf(buf, sizeof(buf), "MMP  %d", a->mmp); dbhq_actor_panel_row(panel, &slot, buf);
    snprintf(buf, sizeof(buf), "ATK  %d", a->atk); dbhq_actor_panel_row(panel, &slot, buf);
    snprintf(buf, sizeof(buf), "DEF  %d", a->defn); dbhq_actor_panel_row(panel, &slot, buf);
    snprintf(buf, sizeof(buf), "MAT  %d", a->mat); dbhq_actor_panel_row(panel, &slot, buf);
    snprintf(buf, sizeof(buf), "MDF  %d", a->mdf); dbhq_actor_panel_row(panel, &slot, buf);
    snprintf(buf, sizeof(buf), "AGI  %d", a->agi); dbhq_actor_panel_row(panel, &slot, buf);
    snprintf(buf, sizeof(buf), "LUK  %d", a->luk); dbhq_actor_panel_row(panel, &slot, buf);
    snprintf(buf, sizeof(buf), "Note          %s", a->note); dbhq_actor_panel_row(panel, &slot, buf);
}

static void dbhq_inject_actors_sidebar(Elem *sidebar) {
    if (!sidebar) return;
    sidebar->n_children = 0;
    int next = 0;
    for (int i = 0; i < g_dbhq_n_actors; i++) {
        Elem *item = reusable_slot(g_dbhq_sidebar_slots, MAX_CHILDREN, next++, "item");
        if (!item) break;
        item->parent = sidebar;
        snprintf(item->classes[0], sizeof(item->classes[0]), "data-item");
        item->n_classes = 1;
        dbhq_actor_sidebar_label(i, item->label, sizeof(item->label));
        snprintf(item->id, sizeof(item->id), "%d", i);
        item->onclick[0] = '\0';
        item->active = (i == g_dbhq_selected_actor);
        if (sidebar->n_children < MAX_CHILDREN) sidebar->children[sidebar->n_children++] = item;
    }
}

static void dbhq_show_actors(void) {
    dbhq_load_actors();
    Elem *sidebar = find_by_tag(g_window, "sidebar");
    dbhq_inject_actors_sidebar(sidebar);
    Elem *panel = find_by_tag(g_window, "panel");
    dbhq_inject_actors_panel(panel);
}

#define DBHQ_LIST_MAX 64
#define DBHQ_KV_MAX 24
#define DBHQ_N_LIST_TABS 12
typedef struct {
    int id;
    char name[64];
    char kv_key[DBHQ_KV_MAX][32];
    char kv_val[DBHQ_KV_MAX][160];
    int n_kv;
} DbhqListRec;
static const struct {
    int tab;
    const char *section;
    const char *title;
    const char *pdl_name;
    const char *state_name;
} g_dbhq_list_cfg[DBHQ_N_LIST_TABS] = {
    { DB_HQ_CLASSES_TAB, "CLASS",  "Class",  "classes.pdl", "db_hq_classes.state.txt" },
    { DB_HQ_SKILLS_TAB,  "SKILL",  "Skill",  "skills.pdl",  "db_hq_skills.state.txt" },
    { DB_HQ_ITEMS_TAB,   "ITEM",   "Item",   "items.pdl",   "db_hq_items.state.txt" },
    { DB_HQ_WEAPONS_TAB, "WEAPON", "Weapon", "weapons.pdl", "db_hq_weapons.state.txt" },
    { DB_HQ_ARMORS_TAB,  "ARMOR",  "Armor",  "armors.pdl",  "db_hq_armors.state.txt" },
    { DB_HQ_ENEMIES_TAB, "ENEMY", "Enemy", "enemies.pdl", "db_hq_enemies.state.txt" },
    { DB_HQ_TROOPS_TAB, "TROOP", "Troop", "troops.pdl", "db_hq_troops.state.txt" },
    { DB_HQ_STATES_TAB, "STATE", "State", "states.pdl", "db_hq_states.state.txt" },
    { DB_HQ_ANIMATIONS_TAB, "ANIMATION", "Animation", "animations.pdl", "db_hq_animations.state.txt" },
    { DB_HQ_TILESETS_TAB, "TILESET", "Tileset", "tilesets.pdl", "db_hq_tilesets.state.txt" },
    { DB_HQ_SYSTEM_TAB, "SYSTEM", "System", "system.pdl", "db_hq_system.state.txt" },
    { DB_HQ_TYPES_TAB, "TYPE", "Type", "types.pdl", "db_hq_types.state.txt" },
};
static DbhqListRec g_dbhq_list_recs[DBHQ_N_LIST_TABS][DBHQ_LIST_MAX];
static int g_dbhq_list_n[DBHQ_N_LIST_TABS];
static int g_dbhq_list_sel[DBHQ_N_LIST_TABS];
static time_t g_dbhq_list_mtime[DBHQ_N_LIST_TABS];
static char g_dbhq_list_state_path[DBHQ_N_LIST_TABS][PATH_BUF];

static int dbhq_list_idx_for_tab(int tab) {
    for (int i = 0; i < DBHQ_N_LIST_TABS; i++)
        if (g_dbhq_list_cfg[i].tab == tab) return i;
    return -1;
}

static void dbhq_list_rec_clear(DbhqListRec *r) {
    memset(r, 0, sizeof(*r));
}

static void dbhq_list_rec_set(DbhqListRec *r, const char *key, const char *val) {
    if (strcmp(key, "id") == 0) { r->id = atoi(val); return; }
    if (strcmp(key, "name") == 0) { snprintf(r->name, sizeof(r->name), "%s", val); return; }
    if (r->n_kv >= DBHQ_KV_MAX) return;
    snprintf(r->kv_key[r->n_kv], sizeof(r->kv_key[0]), "%s", key);
    snprintf(r->kv_val[r->n_kv], sizeof(r->kv_val[0]), "%s", val);
    r->n_kv++;
}

static int dbhq_load_list_tab(int li) {
    if (li < 0 || li >= DBHQ_N_LIST_TABS) return 0;
    const char *path = g_dbhq_list_state_path[li];
    struct stat st;
    char fallback[PATH_BUF];
    if (stat(path, &st) != 0) {
        snprintf(fallback, sizeof(fallback), "%s/&.widgits/db-hq/data/%s",
                 g_house_root, g_dbhq_list_cfg[li].pdl_name);
        path = fallback;
        if (stat(path, &st) != 0) return 0;
    }
    if (st.st_mtime == g_dbhq_list_mtime[li] && g_dbhq_list_n[li] > 0) return 0;
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    DbhqListRec tmp[DBHQ_LIST_MAX];
    int n = 0;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len-1]=='\n' || line[len-1]=='\r')) line[--len] = '\0';
        if (len == 0 || line[0] == '-' || strncmp(line, "SECTION", 7) == 0) continue;
        char sec[64] = "", key[64] = "", val[256] = "";
        char *pcur = line;
        char *bar = strstr(pcur, "|");
        if (!bar) continue;
        *bar = '\0';
        snprintf(sec, sizeof(sec), "%s", pcur);
        pcur = bar + 1;
        while (*pcur == ' ') pcur++;
        bar = strstr(pcur, "|");
        if (!bar) continue;
        *bar = '\0';
        char *ke = bar - 1;
        while (ke > pcur && (*ke == ' ' || *ke == '\t')) { *ke = '\0'; ke--; }
        snprintf(key, sizeof(key), "%s", pcur);
        pcur = bar + 1;
        while (*pcur == ' ') pcur++;
        snprintf(val, sizeof(val), "%s", pcur);
        for (int i = (int)strlen(sec)-1; i>=0 && (sec[i]==' '||sec[i]=='\t'); i--) sec[i]='\0';
        if (strcmp(sec, g_dbhq_list_cfg[li].section) != 0) continue;
        if (strcmp(key, "id") == 0) {
            if (n >= DBHQ_LIST_MAX) break;
            dbhq_list_rec_clear(&tmp[n]);
            dbhq_list_rec_set(&tmp[n], key, val);
            n++;
        } else if (n > 0) {
            dbhq_list_rec_set(&tmp[n-1], key, val);
        }
    }
    fclose(f);
    g_dbhq_list_mtime[li] = st.st_mtime;
    int same = (n == g_dbhq_list_n[li]);
    if (same) {
        for (int i = 0; i < n; i++)
            if (memcmp(&tmp[i], &g_dbhq_list_recs[li][i], sizeof(DbhqListRec)) != 0) { same = 0; break; }
    }
    if (same) return 0;
    g_dbhq_list_n[li] = n;
    memcpy(g_dbhq_list_recs[li], tmp, sizeof(DbhqListRec) * (size_t)n);
    if (g_dbhq_list_sel[li] < 0 && n > 0) g_dbhq_list_sel[li] = 0;
    if (g_dbhq_list_sel[li] >= n) g_dbhq_list_sel[li] = n > 0 ? n - 1 : -1;
    return 1;
}

static void dbhq_inject_list_sidebar(int li, Elem *sidebar) {
    if (!sidebar) return;
    sidebar->n_children = 0;
    int next = 0;
    for (int i = 0; i < g_dbhq_list_n[li]; i++) {
        Elem *item = reusable_slot(g_dbhq_sidebar_slots, MAX_CHILDREN, next++, "item");
        if (!item) break;
        item->parent = sidebar;
        snprintf(item->classes[0], sizeof(item->classes[0]), "data-item");
        item->n_classes = 1;
        snprintf(item->label, sizeof(item->label), "%04d: %s",
                 g_dbhq_list_recs[li][i].id, g_dbhq_list_recs[li][i].name);
        snprintf(item->id, sizeof(item->id), "%d", i);
        item->onclick[0] = '\0';
        item->active = (i == g_dbhq_list_sel[li]);
        if (sidebar->n_children < MAX_CHILDREN) sidebar->children[sidebar->n_children++] = item;
    }
}

static void dbhq_inject_list_panel(int li, Elem *panel) {
    if (!panel) return;
    panel->n_children = 0;
    int slot = 0;
    const char *title = g_dbhq_list_cfg[li].title;
    if (g_dbhq_list_sel[li] < 0 || g_dbhq_list_sel[li] >= g_dbhq_list_n[li]) {
        Elem *t = reusable_slot(g_dbhq_actor_panel_slots, MAX_CHILDREN, slot++, "title");
        if (!t) return;
        t->parent = panel;
        snprintf(t->classes[0], sizeof(t->classes[0]), "block-title"); t->n_classes = 1;
        snprintf(t->label, sizeof(t->label), "%s", title);
        if (panel->n_children < MAX_CHILDREN) panel->children[panel->n_children++] = t;
        dbhq_actor_panel_row(panel, &slot, "(select a row)");
        return;
    }
    DbhqListRec *r = &g_dbhq_list_recs[li][g_dbhq_list_sel[li]];
    Elem *t = reusable_slot(g_dbhq_actor_panel_slots, MAX_CHILDREN, slot++, "title");
    if (t) {
        t->parent = panel;
        snprintf(t->classes[0], sizeof(t->classes[0]), "block-title"); t->n_classes = 1;
        snprintf(t->label, sizeof(t->label), "%s %04d", title, r->id);
        if (panel->n_children < MAX_CHILDREN) panel->children[panel->n_children++] = t;
    }
    char buf[256];
    snprintf(buf, sizeof(buf), "Name          %s", r->name);
    dbhq_actor_panel_row(panel, &slot, buf);
    for (int i = 0; i < r->n_kv; i++) {
        char pretty[48];
        snprintf(pretty, sizeof(pretty), "%s", r->kv_key[i]);
        for (char *c = pretty; *c; c++) if (*c == '_') *c = ' ';
        if (pretty[0] >= 'a' && pretty[0] <= 'z') pretty[0] = (char)(pretty[0] - 32);
        snprintf(buf, sizeof(buf), "%-13s %s", pretty, r->kv_val[i]);
        dbhq_actor_panel_row(panel, &slot, buf);
    }
}

static void dbhq_show_list_tab(void) {
    int li = dbhq_list_idx_for_tab(g_dbhq_current_tab);
    if (li < 0) return;
    dbhq_load_list_tab(li);
    dbhq_inject_list_sidebar(li, find_by_tag(g_window, "sidebar"));
    dbhq_inject_list_panel(li, find_by_tag(g_window, "panel"));
}


static void stats_populate_panel(int idx) {
    if (idx < 0 || idx >= g_dbhq_n_events) return;
    char buf[128];
    snprintf(buf, sizeof(buf), "%s", g_dbhq_events[idx]);
    char *date = buf;
    char *turns_s = strchr(buf, '|');
    if (!turns_s) return;
    *turns_s++ = '\0';
    char *umsg_s = strchr(turns_s, '|');
    if (!umsg_s) return;
    *umsg_s++ = '\0';
    char *amsg_s = strchr(umsg_s, '|');
    if (!amsg_s) return;
    *amsg_s++ = '\0';
    char *tools_s = strchr(amsg_s, '|');
    if (!tools_s) return;
    *tools_s++ = '\0';
    char *pct_s = strchr(tools_s, '|');
    if (!pct_s) return;
    *pct_s++ = '\0';

    Elem *title = find_by_id(g_window, "stat-title");
    Elem *msgs = find_by_id(g_window, "stat-msgs");
    Elem *ai = find_by_id(g_window, "stat-ai");
    Elem *turns = find_by_id(g_window, "stat-turns");
    Elem *tools = find_by_id(g_window, "stat-tools");
    /* REAL FIX 2026-08-25 (direct live report: "it used to show how
     * much money was saved from token calls") - "Overall Stats" (sidebar
     * entry 0, written by stats_hq_manager.c's own write_overall_line())
     * reuses this exact same 6-field record shape but with DIFFERENT
     * real meaning per field (delegation rate/model calls/passes/tokens
     * saved/$ saved, not a session's turns/messages/tools) - same
     * parsing above, just different labels here. */
    if (strcmp(date, "Overall Stats") == 0) {
        if (title) snprintf(title->label, sizeof(title->label), "Overall Stats (all sessions)");
        if (msgs) snprintf(msgs->label, sizeof(msgs->label), "Delegation Rate: %s%%", turns_s);
        if (ai) snprintf(ai->label, sizeof(ai->label), "Model Calls: %s   Passed: %s", umsg_s, amsg_s);
        if (turns) snprintf(turns->label, sizeof(turns->label), "Tokens Saved: ~%s", tools_s);
        if (tools) snprintf(tools->label, sizeof(tools->label), "$ Saved (Claude API): ~$%s", pct_s);
        return;
    }
    if (title) snprintf(title->label, sizeof(title->label), "Session: %s", date);
    if (msgs) snprintf(msgs->label, sizeof(msgs->label), "User Messages: %s", umsg_s);
    if (ai) snprintf(ai->label, sizeof(ai->label), "AI Responses: %s", amsg_s);
    if (turns) snprintf(turns->label, sizeof(turns->label), "Total Turns: %s", turns_s);
    if (tools) snprintf(tools->label, sizeof(tools->label), "Tool Calls: %s   Delegation: %s%%", tools_s, pct_s);
}

static void dbhq_apply_css(Elem *e, int hover) {
    css_compute_style(&g_sheet, e->tag, e->id[0] ? e->id : NULL, e->classes, e->n_classes, hover, &e->style);
}

/* Real, single-slot font cache for text measurement, ported verbatim
 * (khtpm-merge-how2.md §3.2's own cache pattern, already proven). */
static int dbhq_measure_text_px(const CssStyle *st, const char *text) {
    char spec[128];
    const char *fam = st->has_font_family ? st->font_family : "DejaVu Sans";
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
static void dbhq_apply_css_deep(Elem *e) {
    if (!e) return;
    dbhq_apply_css(e, 0);
    for (int i = 0; i < e->n_children; i++) dbhq_apply_css_deep(e->children[i]);
}

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
static void dbhq_pal_shift_subtree(Elem *e, int dy) {
    if (!e) return;
    e->y += dy;
    for (int i = 0; i < e->n_children; i++) dbhq_pal_shift_subtree(e->children[i], dy);
}

/* REAL, GENERALIZED 2026-08-28 (RENDER-FRAME-HISTORY-DRIFT-ASSESSMENT.md
 * Phase C) - this mechanism (scroll clipping + track/thumb/arrow
 * geometry) used to be palettes-only, gated on g_is_palettes, using
 * ONLY "pal-grid-row" as its row selector. A real Phase-A inventory
 * confirmed this is the ONLY scroll mechanism anywhere in this file -
 * db-hq's sidebar (Common Events/Terms/stats-hq), bookmarks, chat-hai's
 * session sidebar, and events-hq's command list all have ZERO scroll
 * support today - long lists silently overflow off-screen with no
 * clipping at all. Since each window MODE runs as its own separate
 * process of this same binary (g_is_palettes etc are set ONCE at
 * startup and never change for that process's lifetime), there is
 * only ever ONE scrollable region active per process - reusing the
 * SAME g_pal_* globals for whichever mode's content is active is
 * completely safe, no cross-mode collision possible. What changes
 * here is WHO calls this and how rows are selected, not the mechanism
 * itself (already real, live-verified for palettes).
 *
 * `container`: the Elem whose children are candidate rows (panel for
 * palettes/bookmarks, sidebar for db-hq/chat-hai).
 * `row_class`: NULL means "every direct child of container is a real
 * row" (db-hq sidebar, chat-hai session sidebar - these containers
 * hold nothing else); a real class name means "only children carrying
 * this class count as rows, others (title/hint/tab-row/static rows)
 * are left alone" (palettes' "pal-grid-row", bookmarks' "bm-bookmark").
 * `panel_y`/`panel_h`: the real bounding box scrolling clips against -
 * passed explicitly since callers differ on whether that's the panel
 * or the sidebar's own box. */
static void generic_scroll_layout_pass(Elem *container, const char *row_class, int box_y, int box_h) {
    g_pal_has_grid = 0;
    g_pal_total_rows = 0;
    if (!container) return;
    Elem *grid_rows[MAX_CHILDREN];
    for (int i = 0; i < container->n_children && i < MAX_CHILDREN; i++) {
        Elem *c = container->children[i];
        if (!row_class || elem_has_class(c, row_class))
            grid_rows[g_pal_total_rows++] = c;
    }
    if (g_pal_total_rows == 0) {
        g_pal_arrow_up->w = 0; g_pal_arrow_up->h = 0; g_pal_arrow_up->onclick[0] = '\0';
        g_pal_arrow_down->w = 0; g_pal_arrow_down->h = 0; g_pal_arrow_down->onclick[0] = '\0';
        return;
    }
    g_pal_has_grid = 1;
    int pad12 = scaled(12);
    int top = box_y + pad12;
    int bot = box_y + box_h - pad12;
    int pitch = (g_pal_total_rows > 1)
        ? grid_rows[1]->y - grid_rows[0]->y
        : grid_rows[0]->h + scaled(6);
    if (pitch <= 0) pitch = 1;
    g_pal_visible_rows = (bot - top) / pitch;
    if (g_pal_visible_rows < 1) g_pal_visible_rows = 1;
    int max_scroll = g_pal_total_rows - g_pal_visible_rows;
    if (max_scroll < 0) max_scroll = 0;
    if (g_pal_scroll > max_scroll) g_pal_scroll = max_scroll;
    if (g_pal_scroll < 0) g_pal_scroll = 0;
    for (int i = 0; i < g_pal_total_rows; i++) {
        Elem *r = grid_rows[i];
        int dy = -(g_pal_scroll * pitch);
        dbhq_pal_shift_subtree(r, dy);
        if (r->y < top || r->y + r->h > bot) { r->w = 0; r->h = 0; }
    }
    g_pal_arrow_h = scaled(14);
    g_pal_track_w = scaled(8);
    g_pal_track_x = container->x + container->w - g_pal_track_w - scaled(2);
    g_pal_track_y = top + g_pal_arrow_h;
    g_pal_track_h = (bot - top) - 2 * g_pal_arrow_h;
    if (g_pal_track_h < 0) g_pal_track_h = 0;
    if (max_scroll == 0) {
        g_pal_thumb_y = g_pal_track_y; g_pal_thumb_h = g_pal_track_h;
    } else {
        int th = (g_pal_track_h * g_pal_visible_rows) / g_pal_total_rows;
        if (th < scaled(14)) th = scaled(14);
        int ty = g_pal_track_y + ((g_pal_track_h - th) * g_pal_scroll) / max_scroll;
        g_pal_thumb_y = ty; g_pal_thumb_h = th;
    }
    memset(g_pal_arrow_up, 0, sizeof(*g_pal_arrow_up));
    snprintf(g_pal_arrow_up->tag, sizeof(g_pal_arrow_up->tag), "button");
    g_pal_arrow_up->x = g_pal_track_x; g_pal_arrow_up->y = g_pal_track_y - g_pal_arrow_h;
    g_pal_arrow_up->w = g_pal_track_w; g_pal_arrow_up->h = g_pal_arrow_h;
    snprintf(g_pal_arrow_up->onclick, sizeof(g_pal_arrow_up->onclick), "scroll:up");
    g_pal_arrow_up->badge_align_left = 1;
    g_pal_arrow_up_disabled = (g_pal_scroll <= 0);

    memset(g_pal_arrow_down, 0, sizeof(*g_pal_arrow_down));
    snprintf(g_pal_arrow_down->tag, sizeof(g_pal_arrow_down->tag), "button");
    g_pal_arrow_down->x = g_pal_track_x; g_pal_arrow_down->y = g_pal_track_y + g_pal_track_h;
    g_pal_arrow_down->w = g_pal_track_w; g_pal_arrow_down->h = g_pal_arrow_h;
    snprintf(g_pal_arrow_down->onclick, sizeof(g_pal_arrow_down->onclick), "scroll:down");
    g_pal_arrow_down->badge_align_left = 1;
    g_pal_arrow_down_disabled = (g_pal_scroll >= max_scroll);
}

static void dbhq_layout_pass(Elem *window) {
    dbhq_apply_css(window, 0);
    /* REAL FIX 2026-08-25 (live report: window height didn't shrink to
     * match the real row count once palette grid columns became layout-
     * derived) - dbhq_apply_css() above re-reads window{}'s own CSS on
     * EVERY call, which has no height declared for palettes, so it
     * always resets has_height back to 0 - a one-time override written
     * into window->style at injection time got silently wiped the very
     * next redraw. Same "re-apply every frame, not just once" fix
     * chat_layout_pass() already uses for its own forced window size
     * (chai_forced_win_w/h) - g_pal_forced_h is set once by
     * dbhq_inject_palette_tiles() and re-applied here every pass. */
    if (g_is_palettes && g_pal_forced_h > 0) { window->style.has_height = 1; window->style.height = g_pal_forced_h; }
    window->x = 0; window->y = 0;
    int default_w = scaled(900);
    int content_total_h = window->style.has_height ? window->style.height : scaled(600);

    Elem *tabbar = find_by_tag(window, "tabbar");
    Elem *sidebar = find_by_tag(window, "sidebar");
    Elem *panel = find_by_tag(window, "panel");

    int tabbar_h = scaled(30);
    int tab_widths[MAX_CHILDREN];
    int tabbar_natural_w = scaled(4);
    if (tabbar) {
        dbhq_apply_css(tabbar, 0);
        for (int i = 0; i < tabbar->n_children; i++) {
            Elem *tab = tabbar->children[i];
            tab->active = (i == g_dbhq_current_tab);
            dbhq_apply_css(tab, 0);
            tab_widths[i] = dbhq_measure_text_px(&tab->style, tab->label) + scaled(34);
            tab->w = tab_widths[i];
            tabbar_natural_w += tab_widths[i] + 1;
        }
    }
    window->w = window->style.has_width ? window->style.width : (tabbar_natural_w > default_w ? tabbar_natural_w : default_w);
    window->h = content_total_h + g_dbhq_chrome_h;

    g_dbhq_close_w = scaled(56); g_dbhq_close_h = g_dbhq_chrome_h - scaled(6);
    g_dbhq_close_x = window->w - g_dbhq_close_w - scaled(4);
    g_dbhq_close_y = scaled(3);

    if (tabbar) {
        tabbar->style.has_display = 1; tabbar->style.display_flex = 1;
        tabbar->style.has_flex_direction = 1; tabbar->style.flex_row = 1;
        css_layout_pass(tabbar, 0, g_dbhq_chrome_h, window->w, tabbar_h);
        for (int i = 0; i < tabbar->n_children; i++) {
            Elem *tab = tabbar->children[i];
            tab->x += scaled(4) + i;
            tab->y = g_dbhq_chrome_h + scaled(2); tab->h = tabbar_h - scaled(4);
        }
    }

    int content_y = g_dbhq_chrome_h + tabbar_h;
    int content_h = content_total_h - tabbar_h;
    /* REAL FIX 2026-08-25 (Stage 2 palettes migration) - palettes has no
     * <sidebar> at all (find_by_tag returns NULL below), but this
     * default was applied unconditionally, wasting 210px of panel width
     * that no sidebar was actually using. */
    int sidebar_w = (g_is_palettes || g_is_bookmarks) ? 0 : scaled(210);

    /* REAL FIX 2026-08-25 (Stage 2 palettes migration, direct live
     * report: "no longer showing emojis or navs") - this gate assumed
     * every g_is_db_hq consumer has a real tabbar/tab-state concept.
     * Palettes has no tabbar at all (g_dbhq_current_tab stays at its
     * default, never equals DB_HQ_COMMON_EVENTS_TAB), so this early
     * return skipped ALL panel/content layout below - every element
     * stayed at its zero-initialized x/y/w/h, which is also why
     * assign_palettes_nav()'s own `e->w > 0 && e->h > 0` check numbered
     * nothing. */
    if (!(g_is_palettes || g_is_bookmarks) && !dbhq_tab_is_real(g_dbhq_current_tab)) return;

    if (sidebar) {
        dbhq_apply_css(sidebar, 0);
        if (sidebar->style.has_width && !sidebar->style.width_is_pct) sidebar_w = sidebar->style.width;
        sidebar->style.has_display = 1; sidebar->style.display_flex = 1;
        sidebar->style.has_flex_direction = 1; sidebar->style.flex_row = 0;
        sidebar->style.has_padding = 1; sidebar->style.padding = scaled(4);
        int item_h = scaled(22);
        for (int i = 0; i < sidebar->n_children; i++) {
            Elem *item = sidebar->children[i];
            dbhq_apply_css(item, 0);
            item->style.has_height = 1; item->style.height = item_h;
        }
        css_layout_pass(sidebar, 0, content_y, sidebar_w, content_h);
    }

    if (panel) {
        dbhq_apply_css(panel, 0);
        int margin = scaled(8);
        panel->x = sidebar_w + margin;
        panel->y = content_y + margin;
        panel->w = window->w - sidebar_w - margin * 2;
        panel->h = content_h - margin * 2;
        panel->style.has_display = 1; panel->style.display_flex = 1;
        panel->style.has_flex_direction = 1; panel->style.flex_row = 0;
        panel->style.has_padding = 1; panel->style.padding = scaled(12);
        panel->style.has_gap = 1; panel->style.gap = scaled(6);
        for (int i = 0; i < panel->n_children; i++) {
            Elem *c = panel->children[i];
            dbhq_apply_css(c, 0);
            if (strcmp(c->tag, "title") == 0) {
                c->w = dbhq_measure_text_px(&c->style, c->label) + scaled(10);
                c->h = scaled(14);
                continue;
            }
            /* REAL FIX 2026-08-25 (Stage 2 palettes migration) - this
             * unconditional 22px override was stomping palette rows'
             * real CSS-driven height (.pal-grid-row's own 56px, needed
             * for 48px sprite tiles) right after dbhq_apply_css() just
             * computed it correctly two lines above. db-hq's own flat
             * text/button panel children still want this default - only
             * palettes skips it, since its rows size themselves from
             * CSS. Also runs the deep CSS-apply here (not once for the
             * whole panel up front) so it lands after this same loop's
             * own per-row apply, not stomped by anything after. */
            if (g_is_palettes) {
                dbhq_apply_css_deep(c);
                continue;
            }
            c->style.has_height = 1; c->style.height = scaled(22);
        }
        css_layout_pass(panel, panel->x, panel->y, panel->w, panel->h);
        /* REAL FIX 2026-08-29 (Part B, Common Events view-mode tabs) -
         * dbhq_ce_inject_panel() injects a real "tabbar" child (view-
         * mode tabs) whose OWN tab children were positioned relative
         * to panel->y at injection time, before this pass just moved
         * the tabbar itself down in the flex column (same real class
         * of bug the window's own top tabbar already avoids by being
         * repositioned HERE, after its own css_layout_pass() call
         * above - the injector can't know its final y in advance).
         * Real fix, same pattern: reposition each tab relative to the
         * tabbar's own NOW-correct x/y. */
        for (int i = 0; i < panel->n_children; i++) {
            Elem *c = panel->children[i];
            if (strcmp(c->tag, "tabbar") != 0) continue;
            int tx = c->x;
            for (int j = 0; j < c->n_children; j++) {
                Elem *tab = c->children[j];
                /* REAL FIX 2026-08-29 (live report: "tabs are a bit too
                 * close together, overlapping eachother") - the
                 * css_layout_pass(panel, ...) call just above already
                 * recursed into this tabbar's own children (it has no
                 * display:flex declared, so css_layout_pass's generic
                 * block algorithm stomped each tab's carefully-measured
                 * injection-time width with its own default), so
                 * trusting tab->w here was trusting a value this same
                 * function had already clobbered one line earlier.
                 * Recompute it fresh, same formula dbhq_ce_inject_
                 * panel() used at injection. */
                int tw = dbhq_measure_text_px(&tab->style, tab->label) + scaled(34);
                tab->x = tx; tab->y = c->y + 2; tab->w = tw; tab->h = c->h - 4;
                tx += tw + scaled(4);
            }
        }

        /* REAL, GENERALIZED 2026-08-28 (was palettes-only inline code,
         * see generic_scroll_layout_pass()'s own header comment for the
         * full "why"). Palettes' own tile grid is the only mode with a
         * class-filtered row selector (title/hint/tab-row/chooser-row
         * share the panel with real scrollable tile rows) - every other
         * mode below scrolls a container whose children are ALL real
         * rows (row_class=NULL). */
        if (g_is_palettes) {
            generic_scroll_layout_pass(panel, "pal-grid-row", panel->y, panel->h);
        } else if (dbhq_tab_is_real(g_dbhq_current_tab) && sidebar && sidebar->n_children > 0) {
            /* REAL FIX 2026-08-28 (Phase C, fixes a real, previously-
             * silent bug: db-hq's sidebar - Common Events, Terms, and
             * stats-hq's session list, all the SAME function - had zero
             * scroll support; a long enough list simply ran off the
             * bottom of the window with no way to reach it). */
            generic_scroll_layout_pass(sidebar, NULL, sidebar->y, content_h);
        } else if (g_is_bookmarks) {
            generic_scroll_layout_pass(panel, "bm-bookmark", panel->y, panel->h);
        } else {
            g_pal_has_grid = 0;
        }
    }
}

/* REAL, NEW 2026-08-25 (Stage 2 palettes migration) - any element
 * carrying its own onClick= becomes a numbered row, tree-walk order.
 * Unconditional (no nav_index==0 guard) - see g_is_palettes's own
 * declaration comment for why that's the deliberate, safer choice here
 * (no earlier pass in this mode to avoid double-counting against). */
static int dbhq_cli_io_navigable(Elem *e) {
    if (strcmp(e->tag, "cli_io") != 0) return 1;
    return (e == g_dbhq_active_scope_root);
}
static int dbhq_elem_is_navigable(Elem *e) {
    if (!e) return 0;
    if (!dbhq_cli_io_navigable(e)) return 0;
    if (!g_dbhq_active_scope_root) return 1;
    { Elem *p = e; while (p) { if (p == g_dbhq_active_scope_root) return 1; p = p->parent; } }
    return 0;
}
static void dbhq_activate_scope(Elem *e) { g_dbhq_active_scope_root = e; }
static void dbhq_back_scope(void) {
    Elem *p = g_dbhq_active_scope_root ? g_dbhq_active_scope_root->parent : NULL;
    while (p && strncmp(p->onclick, "ACTIVATE", 8) != 0) p = p->parent;
    g_dbhq_active_scope_root = p;
}
static void dbhq_nav_take(Elem *e) {
    if (!e || g_n_nav >= MAX_ELEMS) return;
    if (!dbhq_elem_is_navigable(e)) { e->nav_index = 0; return; }
    e->nav_index = ++g_n_nav;
    g_nav[g_n_nav - 1] = e;
}

static void assign_palettes_nav(Elem *e) {
    if (!e || g_n_nav >= MAX_ELEMS) return;
    if (e->onclick[0] && e != g_dbhq_close_elem && e->w > 0 && e->h > 0) {
        if (dbhq_elem_is_navigable(e)) {
            e->nav_index = ++g_n_nav;
            g_nav[g_n_nav - 1] = e;
        } else e->nav_index = 0;
    }
    for (int i = 0; i < e->n_children && g_n_nav < MAX_ELEMS; i++)
        assign_palettes_nav(e->children[i]);
}

static void dbhq_assign_nav_indices(Elem *window) {
    g_n_nav = 0;
    /* REAL FIX 2026-08-29 - same real fix as evhq_assign_nav_indices()'s
     * own matching comment (nav-index collision between the modal
     * picker and the background window). Gated on the exact same
     * condition db-hq's own picker ownership uses everywhere else
     * (g_dbhq_ce_editing && g_evhq_picker_open) - plain db-hq/
     * palettes/bookmarks never open this picker at all. */
    if (g_dbhq_ce_editing && g_evhq_picker_open) { zero_nav_subtree(window); return; }
    Elem *tabbar = find_by_tag(window, "tabbar");
    if (tabbar) {
        for (int i = 0; i < tabbar->n_children && g_n_nav < MAX_ELEMS; i++) {
            Elem *tab = tabbar->children[i];
            dbhq_nav_take(tab);
        }
    }
    /* REAL FIX 2026-08-25 (live report: "it never puts a default '>' in
     * bookmarks in 4 or any" + arrows not moving anything visibly) -
     * g_dbhq_current_tab defaults to DB_HQ_COMMON_EVENTS_TAB for EVERY
     * db-hq window, bookmarks included (nothing ever sets it otherwise
     * for a tabbar-less window). This block's own panel loop was
     * numbering bookmarks' <button> rows 1,2,3 - then the generic
     * assign_palettes_nav() pass below ran unconditionally on the SAME
     * tree and re-numbered those same buttons a second time (4,5,6,
     * confirmed live via a debug dump: real content sat at nav_index
     * 4-6 while g_focus_nav defaulted to/jumped to 1-3, so NOTHING ever
     * matched - the ring/badge never had a valid target). Two passes
     * fighting over one tree. Scoped out here exactly like
     * dbhq_layout_pass()'s own tab-gate already excludes palettes. */
    if (!(g_is_palettes || g_is_bookmarks) && dbhq_tab_is_real(g_dbhq_current_tab)) {
        Elem *sidebar = find_by_tag(window, "sidebar");
        if (sidebar) {
            for (int i = 0; i < sidebar->n_children && g_n_nav < MAX_ELEMS; i++) {
                Elem *item = sidebar->children[i];
                dbhq_nav_take(item);
            }
        }
        Elem *panel = find_by_tag(window, "panel");
        if (panel) {
            for (int i = 0; i < panel->n_children && g_n_nav < MAX_ELEMS; i++) {
                Elem *c = panel->children[i];
                /* REAL FIX 2026-08-29 (Part B, live report: "scripting
                 * scratch and blueprints dont have nav. that violates
                 * house") - Common Events' own view-mode tabbar
                 * (dbhq_ce_inject_panel()'s "CE:VIEWTAB:" tabs) was
                 * falling into the generic "not a button, zero it"
                 * branch below, same real bug class events-hq's own
                 * viewtabs nav-assignment already solves - mirrored
                 * here, not reinvented (see evhq_assign_nav_indices()'s
                 * own real "viewtabs nav-reachable first" comment). */
                if (strcmp(c->tag, "tabbar") == 0) {
                    for (int j = 0; j < c->n_children && g_n_nav < MAX_ELEMS; j++) {
                        dbhq_nav_take(c->children[j]);
                    }
                    continue;
                }
                /* REAL FIX 2026-08-29 (live report: "in the 'scratch'
                 * visual scripting setup, all blocks are supposed to be
                 * nav numbered") - same real gap as events-hq's own
                 * evhq_assign_nav_indices() had (see that function's own
                 * matching comment, fixed in the same pass): the
                 * dbhq_ce_inject_panel() Scratch stub (tag="panel",
                 * built by the SHARED evhq_build_scratch_view()) fell
                 * into the generic "not a button, zero it" branch below
                 * and its real clickable children (palette items, the
                 * place-slot) were never walked at all. Gate on
                 * onclick[0], same as events-hq's own fix, since the
                 * stub also carries inert "text"/"block-clue" children
                 * that correctly stay non-nav. */
                if (strcmp(c->tag, "panel") == 0) {
                    for (int j = 0; j < c->n_children && g_n_nav < MAX_ELEMS; j++) {
                        Elem *bc = c->children[j];
                        if (!bc->onclick[0]) continue;
                        dbhq_nav_take(bc);
                    }
                    continue;
                }
                if (strcmp(c->tag, "button") != 0) { c->nav_index = 0; continue; }
                dbhq_nav_take(c);
            }
        }
    }
    /* REAL, NEW 2026-08-25 (Stage 2 palettes migration) - generic,
     * UNCONDITIONAL nav pass for palettes' own grid-of-tiles content
     * (no tabbar/sidebar/panel-button structure above to conflict
     * with). See g_is_palettes's own declaration comment for why this
     * is deliberately unconditional, not the nav_index==0-guarded
     * pattern khtpm_hq_render.c used (that pattern needs
     * clear_nav_indices() every pass to stay correct - a real bug
     * found+fixed there this session when that call was missing;
     * unconditional reassignment sidesteps the whole bug class here). */
    /* REAL 2026-08-25 (Stage 3 bookmarks port) - bookmarks' own flat
     * button-per-row panel is the exact same "no tabbar/sidebar
     * structure, every onclick-carrying element numbered" shape
     * palettes already uses this generic pass for - reused, not
     * duplicated. */
    /* REAL, NEW 2026-08-25 (live instruction: "they need to be numbered
     * (1 and 2), with nav feature for accessibility / disabled") - the
     * scroll arrows get numbered FIRST (1/2), tiles after - matches the
     * literal instruction, and reads naturally as "the controls for
     * this grid, then the grid". A disabled arrow's onclick was already
     * cleared in dbhq_layout_pass(), so it fails the same onclick[0]
     * check every other numbered element uses - excluded from nav
     * without a separate disabled-specific branch here. */
    if (g_pal_has_grid) {
        if (g_pal_arrow_up->onclick[0] && g_n_nav < MAX_ELEMS) {
            dbhq_nav_take(g_pal_arrow_up);
        }
        if (g_pal_arrow_down->onclick[0] && g_n_nav < MAX_ELEMS) {
            dbhq_nav_take(g_pal_arrow_down);
        }
    }
    if (g_is_palettes || g_is_bookmarks) {
        assign_palettes_nav(g_window);
    }
    if (g_n_nav < MAX_ELEMS) {
        dbhq_nav_take(g_dbhq_close_elem);
    }
    if (g_focus_nav < 1) g_focus_nav = 1;
    if (g_focus_nav > g_n_nav) g_focus_nav = g_n_nav > 0 ? g_n_nav : 1;
    nav_ledger_publish();
}

static void dbhq_render_placeholder_tab(Elem *window) {
    char pspec[48];
    snprintf(pspec, sizeof(pspec), "DejaVu Sans:pixelsize=%d", scaled(12));
    XftFont *font = XftFontOpenName(dpy, screen, pspec);
    XftColor col = xft_color("#888888");
    char msg[64];
    snprintf(msg, sizeof(msg), "%s \xe2\x80\x94 (coming soon)", DB_HQ_TAB_LABELS[g_dbhq_current_tab]);
    XGlyphInfo extents;
    XftTextExtentsUtf8(dpy, font, (const FcChar8 *)msg, (int)strlen(msg), &extents);
    int tx = (window->w - extents.width) / 2;
    int ty = window->h / 2;
    XftDrawStringUtf8(xftdraw_buf, &col, font, tx, ty, (const FcChar8 *)msg, (int)strlen(msg));
    XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &col);
    XftFontClose(dpy, font);
}

static void dbhq_soft_focus(void) {
    XRaiseWindow(dpy, win);
    XSetInputFocus(dpy, win, RevertToParent, CurrentTime);
    XFlush(dpy);
}

static void dbhq_grab_keyboard_retry(void) {
    for (int attempt = 0; attempt < 5; attempt++) {
        int rc = XGrabKeyboard(dpy, win, True, GrabModeAsync, GrabModeAsync, CurrentTime);
        if (rc == GrabSuccess) break;
        XSync(dpy, False);
        usleep(5000);
    }
}

static void dbhq_draw_chrome_bar(void) {
    XSetForeground(dpy, gc, alloc_pixel("#2b2b2b"));
    XFillRectangle(dpy, buf, gc, 0, 0, g_window->w, g_dbhq_chrome_h);

    char tspec[48];
    snprintf(tspec, sizeof(tspec), "DejaVu Sans:pixelsize=%d:bold", scaled(10));
    XftFont *titlefont = XftFontOpenName(dpy, screen, tspec);
    if (!titlefont) { snprintf(tspec, sizeof(tspec), "DejaVu Sans:pixelsize=%d", scaled(10)); titlefont = XftFontOpenName(dpy, screen, tspec); }
    XftColor titlecol = xft_color("#eeeeee");
    char title[16];
    snprintf(title, sizeof(title), "db-hq %s", g_dbhq_has_real_focus ? "^" : " ");
    int ty = (g_dbhq_chrome_h + titlefont->ascent - titlefont->descent) / 2;
    XftDrawStringUtf8(xftdraw_buf, &titlecol, titlefont, scaled(8), ty, (const FcChar8 *)title, (int)strlen(title));
    XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &titlecol);
    XftFontClose(dpy, titlefont);

    g_dbhq_close_elem->x = g_dbhq_close_x; g_dbhq_close_elem->y = g_dbhq_close_y;
    g_dbhq_close_elem->w = g_dbhq_close_w; g_dbhq_close_elem->h = g_dbhq_close_h;
    snprintf(g_dbhq_close_elem->label, sizeof(g_dbhq_close_elem->label), "x");
    css_style_init(&g_dbhq_close_elem->style);
    g_dbhq_close_elem->style.has_border_color = 1;
    snprintf(g_dbhq_close_elem->style.border_color, sizeof(g_dbhq_close_elem->style.border_color), "%s",
             g_dbhq_close_elem->nav_index == g_focus_nav ? "#ff8c00" : "#888888");
    g_dbhq_close_elem->style.has_border_width = 1; g_dbhq_close_elem->style.border_width = 1;
    g_dbhq_close_elem->style.has_fg_color = 1;
    snprintf(g_dbhq_close_elem->style.fg_color, sizeof(g_dbhq_close_elem->style.fg_color), "#eeeeee");
    draw_elem(g_dbhq_close_elem, 0);
}

/* Real db-hq redraw content (called from the shared redraw()'s
 * g_is_db_hq branch) - chrome fill/tabbar/sidebar/panel/placeholder,
 * ported verbatim. Present (XGetImage->XPutImage) stays in the shared
 * redraw(), not duplicated here. */
/* REAL, NEW 2026-08-25 (live report: "the thumb for mouse isn't working
 * yet... it needs to start at the top and show lower content as its
 * pulled downwards") - maps a mouse Y coordinate (anywhere in the track)
 * directly to a scroll row, the standard "click/drag jumps the thumb to
 * the cursor" scrollbar behavior - top of track = scroll 0 (first rows
 * visible), dragging down increases scroll (later rows come into view).
 * max_scroll is recomputed the same way dbhq_layout_pass()'s own post-
 * pass does, since this runs from a raw pointer event, before layout. */
static void dbhq_pal_scroll_to_y(int mouse_y) {
    if (!g_pal_has_grid || g_pal_track_h <= 0) return;
    int max_scroll = g_pal_total_rows - g_pal_visible_rows;
    if (max_scroll < 0) max_scroll = 0;
    if (max_scroll == 0) { g_pal_scroll = 0; return; }
    int th = (g_pal_track_h * g_pal_visible_rows) / g_pal_total_rows;
    if (th < scaled(14)) th = scaled(14);
    int usable = g_pal_track_h - th;
    int rel = mouse_y - g_pal_track_y - th / 2;
    if (rel < 0) rel = 0;
    if (rel > usable) rel = usable;
    g_pal_scroll = usable > 0 ? (rel * max_scroll) / usable : 0;
}

/* Forward decls - real definitions live after the g_evhq_* globals
 * they share with events-hq's own entity editing (Task 6, 2026-08-26). */
static void dbhq_ce_open(const char *ce_name);
/* Forward decls, real definitions come later (evhq_* section) -
 * dbhq_ce_inject_panel() (Part B, 2026-08-29) needs these before its
 * own definition to build the shared view-mode tabs/Scratch content. */
static int evhq_measure_text_px(const CssStyle *st, const char *text);
static void evhq_build_scratch_view(Elem *viewmode_stub, int content_x, int content_y, int content_h, int window_w);
static int evhq_handle_block_onclick(const char *onclick);
static int dbhq_ce_inject_panel(Elem *panel);
static void dbhq_restore_tab_content(void);
static void dbhq_ce_handle_onclick(const char *onclick);
static void evhq_dispatch_picker_onclick(const char *onclick);
static void evhq_redraw_content(void); /* REAL, NEW 2026-08-29 - evhq_dispatch_picker_onclick()'s own new PICKER:DELETE case needs this before its real definition */
static void nav_tab_register(const char *type, const char *title);
static void nav_tab_unregister(void);
static void nav_tab_cycle(void);
static void nav_tab_poll_active(void);
static void nav_ledger_publish(void);
 /* Task 7 follow-up (2026-08-26) - shared mouse-click handler for the picker's real Elems, used by both dbhq_activate_elem() and evhq_activate_elem() */
static void dbhq_ce_draw_overlay_if_needed(void);
static void dbhq_ce_handle_key_if_needed(KeySym ks, char ch, int *consumed);
static void evhq_open_edit_picker(int cmd_index); /* Task 7 (2026-08-26) - defined after g_evhq_cmds/registry helpers; EvhqCmdNode itself declared just below */
static void evhq_load_command_registry(void); /* Task 7 (2026-08-26) - dbhq_ce_inject_panel() needs to call this before its own definition */
/* Real events-hq functions this Task 6 code reuses verbatim, but which
 * are themselves defined later in the file than db-hq's own redraw/key
 * functions - forward-declared here so the wrappers above can call
 * them regardless of definition order. */
static void evhq_draw_picker_overlay(void);
static void evhq_handle_key(KeySym ks, char ch);

/* REAL, requested "once and for all" fix (2026-08-27) - same real
 * frame-history convention as evhq_append_frame_history() (see its own
 * header comment for the full "why"), ported to db-hq too - db-hq mode
 * covers palettes/bookmarks/stats-hq/Common-Events-editor as well since
 * they all share this one dispatch, not just the plain entity-menu
 * view. */
static long g_dbhq_frame_seq = 0;
static void dbhq_append_frame_history(void) {
    g_dbhq_frame_seq++;
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/db_hq_frame_history.txt", g_house_root);
    FILE *f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "seq=%ld focus_nav=%d/%d tab=%d selected_event=%d\n",
            g_dbhq_frame_seq, g_focus_nav, g_n_nav, g_dbhq_current_tab, g_dbhq_selected_event);
    fclose(f);
    /* REAL, NEW (2026-08-27, HARNESS-AUTHORING-GUIDE.md §3a) - the SAME
     * single-key flat-file sibling convention evhq_append_frame_history()
     * already uses (its own header comment explains the full "why"): a
     * real PAL/prisc+x script can inject relay codes but SYS_GET_KV_INT
     * only matches a key at the very START of a line, so it cannot read
     * the multi-key db_hq_frame_history.txt line above. Small, cheap,
     * zero-VM-change fix: also write single-key flat files a PAL script
     * CAN poll today via SYS_GET_KV_INT. First real consumer: the db-hq
     * tab-switch PAL harness (cursword/harnesses/pal/db_hq_tab_switch_demo.pal). */
    char tabpath[PATH_BUF];
    snprintf(tabpath, sizeof(tabpath), "%s/#.desktop/db_hq_current_tab.txt", g_house_root);
    FILE *tf = fopen(tabpath, "w");
    if (tf) { fprintf(tf, "current_tab=%d\n", g_dbhq_current_tab); fclose(tf); }
    char seqpath[PATH_BUF];
    snprintf(seqpath, sizeof(seqpath), "%s/#.desktop/db_hq_seq.txt", g_house_root);
    FILE *qf = fopen(seqpath, "w");
    if (qf) { fprintf(qf, "seq=%ld\n", g_dbhq_frame_seq); fclose(qf); }
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
 * own 2026-08-28 book-stack fix comment above dbhq_paint_frame_line()).
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

static void dbhq_serialize_frame_elem(FILE *f, Elem *e) {
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
    fprintf(f, "%s|%s|%s|%s|%s|%s|%d|%d|%d|%d|%d|%d|%s|%s\n",
            e->tag, e->id, classes_joined, e->label, e->sprite, e->onclick,
            e->nav_index, e->active, e->x, e->y, e->w, e->h,
            target_id_esc, input_buffer_esc);
}

/* Real recursive serializer, same traversal order render_tree() itself
 * uses (non-title children first, in order, title deferred to last at
 * each level) - PRESERVING draw order matters for real visual parity
 * (a later-drawn element can visually overlap an earlier one). */
static void dbhq_serialize_frame_subtree(FILE *f, Elem *e) {
    for (int i = 0; i < e->n_children; i++) {
        Elem *c = e->children[i];
        if (strcmp(c->tag, "title") == 0 || strcmp(c->tag, "module") == 0) continue;
        dbhq_serialize_frame_elem(f, c);
        dbhq_serialize_frame_subtree(f, c);
    }
    for (int i = 0; i < e->n_children; i++) {
        Elem *c = e->children[i];
        if (strcmp(c->tag, "title") == 0) dbhq_serialize_frame_elem(f, c);
    }
}

static void dbhq_write_palette_frame_file(Elem *panel) {
    if (!panel) return;
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/palettes_frame.txt", g_house_root);
    char tmp[PATH_BUF];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *f = fopen(tmp, "w");
    if (!f) return;
    dbhq_serialize_frame_subtree(f, panel);
    fclose(f);
    rename(tmp, path);
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
static void dbhq_paint_frame_line(const char *line) {
    char buf2[2048];
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
     * (pipe-escaped) [7]=input_buffer (pipe-escaped) - the last two are
     * REAL, NEW 2026-08-31, see dbhq_serialize_frame_elem()'s own
     * comment; a frame file written by an older binary (before these
     * two fields existed) simply has 6 tail fields, not 8 - the loop
     * below returns (honest skip) rather than misparse it, matching
     * this function's existing "malformed line" convention exactly. */
    char *tail[8];
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
    for (int i = 7; i >= 0; i--) {
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
    snprintf(tmp.label, sizeof(tmp.label), "%s", front[3]);
    snprintf(tmp.sprite, sizeof(tmp.sprite), "%s", front[4]);
    snprintf(tmp.onclick, sizeof(tmp.onclick), "%s", onclick_field);
    tmp.nav_index = atoi(tail[0]);
    tmp.active = atoi(tail[1]);
    tmp.x = atoi(tail[2]);
    tmp.y = atoi(tail[3]);
    tmp.w = atoi(tail[4]);
    tmp.h = atoi(tail[5]);
    /* REAL, NEW 2026-08-31 - see dbhq_serialize_frame_elem()'s own
     * comment. Without this, a cli_io element painted through THIS path
     * (the default/popup mode's real content draw, see redraw()'s own
     * "now the shared, generic render_tree()" comment) always saw an
     * empty input_buffer regardless of what was really typed - `tmp` is
     * a fresh, memset-zeroed local on every call, never the live Elem
     * a human is actually typing into. */
    frame_field_unescape_pipe(tail[6], tmp.target_id, sizeof(tmp.target_id));
    frame_field_unescape_pipe(tail[7], tmp.input_buffer, sizeof(tmp.input_buffer));

    css_compute_style(&g_sheet, tmp.tag, tmp.id[0] ? tmp.id : NULL, tmp.classes, tmp.n_classes, tmp.active, &tmp.style);
    draw_elem(&tmp, 0);
}

static void dbhq_paint_palette_frame_file(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/palettes_frame.txt", g_house_root);
    FILE *f = fopen(path, "r");
    if (!f) return; /* honest: no frame file yet, nothing to paint - not a fabricated fallback */
    char line[2048];
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = '\0';
        if (len == 0) continue;
        dbhq_paint_frame_line(line);
    }
    fclose(f);
}

static void dbhq_redraw_content(void) {
    dbhq_layout_pass(g_window);
    dbhq_assign_nav_indices(g_window);
    XSetForeground(dpy, gc, alloc_pixel(g_window->style.has_bg_color ? g_window->style.bg_color : "#141414"));
    /* REAL FIX 2026-08-28 (live corruption found testing Phase 2's
     * frame-file paint) - clearing only g_window->w/h leaves stale
     * pixels visible whenever content SHRINKS between redraws (a
     * taller previous session's leftover rows) - the backing Pixmap
     * only ever GROWS (see g_buf_w/g_buf_h's own header comment),
     * it never shrinks back down, so clearing less than the real
     * allocated buffer leaves old content sitting below the new,
     * smaller content. Clear the FULL allocated buffer every time. */
    XFillRectangle(dpy, buf, gc, 0, 0, (unsigned)(g_buf_w > g_window->w ? g_buf_w : g_window->w), (unsigned)(g_buf_h > g_window->h ? g_buf_h : g_window->h));
    if (!dbhq_tab_is_real(g_dbhq_current_tab)) {
        Elem *tabbar = find_by_tag(g_window, "tabbar");
        if (tabbar) { draw_elem(tabbar, 0); render_tree(tabbar, 1); }
        dbhq_render_placeholder_tab(g_window);
    } else if (g_is_palettes) {
        /* REAL, Phase 2 first proof (2026-08-28) - window chrome draws
         * normally (draw_elem() reads the live g_window directly, same
         * as always - only PANEL CONTENT is frame-derived for this
         * first scoped slice, see the big comment above dbhq_
         * serialize_frame_elem()). Panel content is: (1) serialize the
         * just-computed real layout to a real file, (2) paint ONLY
         * from that file - two genuinely separate steps with the file
         * as the real boundary between them, not a cosmetic detour
         * that still secretly reads the live tree. */
        draw_elem(g_window, 0);
        Elem *panel = find_by_tag(g_window, "panel");
        dbhq_write_palette_frame_file(panel);
        dbhq_paint_palette_frame_file();
    } else {
        render_tree(g_window, 0);
        /* Task 6 (2026-08-26) - embedded Common Event editor's Add
         * Command picker overlay, same real popup events-hq's own
         * entity editing already uses (evhq_draw_picker_overlay() is
         * already generic against g_window's own w/h, no changes
         * needed there). */
        dbhq_ce_draw_overlay_if_needed();
    }
    /* REAL, ported 2026-08-25 - palette matrix scroll thumb, verbatim
     * from khtpm_hq_render.c's own draw_chrome_bar-adjacent draw call
     * (see g_pal_scroll's own declaration comment). Drawn only when this
     * window's panel actually carries grid rows. */
    if (g_pal_has_grid && g_pal_track_h > 0) {
        XSetForeground(dpy, gc, alloc_pixel("#2a2a2a"));
        XFillRectangle(dpy, buf, gc, g_pal_track_x, g_pal_track_y, (unsigned)g_pal_track_w, (unsigned)g_pal_track_h);
        XSetForeground(dpy, gc, alloc_pixel("#888888"));
        XFillRectangle(dpy, buf, gc, g_pal_track_x + scaled(1), g_pal_thumb_y,
                       (unsigned)(g_pal_track_w - scaled(2)), (unsigned)g_pal_thumb_h);
        /* REAL, NEW 2026-08-25 (live instruction: "they need to be
         * numbered (1 and 2), with nav feature for accessibility /
         * disabled") - real up/down arrow buttons at the track's own
         * two ends, drawn as filled triangles (standard scrollbar
         * shape). Disabled (onclick[0]=='\0', already cleared in
         * dbhq_layout_pass() at the scroll min/max) dims to a flatter
         * gray instead of the enabled #cccccc, a real visual "this is
         * inert" signal matching the real disabled-from-nav state, not
         * just a missing badge. draw_elem() is called on each AFTER the
         * triangle so the real nav badge/focus-ring paints on top,
         * using the SAME code path (and so the SAME visual language)
         * every other numbered tile's badge already uses - not a
         * second, bespoke badge drawn here. */
        int ax = g_pal_track_x, aw = g_pal_track_w;
        int up_y0 = g_pal_track_y - g_pal_arrow_h;
        int down_y0 = g_pal_track_y + g_pal_track_h;
        int up_enabled = !g_pal_arrow_up_disabled;
        int down_enabled = !g_pal_arrow_down_disabled;
        XSetForeground(dpy, gc, alloc_pixel("#3a3a3a"));
        XFillRectangle(dpy, buf, gc, ax, up_y0, (unsigned)aw, (unsigned)g_pal_arrow_h);
        XFillRectangle(dpy, buf, gc, ax, down_y0, (unsigned)aw, (unsigned)g_pal_arrow_h);
        XSetForeground(dpy, gc, alloc_pixel(up_enabled ? "#cccccc" : "#555555"));
        XPoint up_tri[3] = {
            { (short)(ax + aw / 2), (short)(up_y0 + scaled(3)) },
            { (short)(ax + scaled(2)), (short)(up_y0 + g_pal_arrow_h - scaled(3)) },
            { (short)(ax + aw - scaled(2)), (short)(up_y0 + g_pal_arrow_h - scaled(3)) },
        };
        XFillPolygon(dpy, buf, gc, up_tri, 3, Convex, CoordModeOrigin);
        XSetForeground(dpy, gc, alloc_pixel(down_enabled ? "#cccccc" : "#555555"));
        XPoint down_tri[3] = {
            { (short)(ax + aw / 2), (short)(down_y0 + g_pal_arrow_h - scaled(3)) },
            { (short)(ax + scaled(2)), (short)(down_y0 + scaled(3)) },
            { (short)(ax + aw - scaled(2)), (short)(down_y0 + scaled(3)) },
        };
        XFillPolygon(dpy, buf, gc, down_tri, 3, Convex, CoordModeOrigin);
        draw_elem(g_pal_arrow_up, 0);
        draw_elem(g_pal_arrow_down, 0);
    }
    dbhq_draw_chrome_bar();
    dbhq_append_frame_history();
    /* REAL BUG FIX 2026-08-29, direct live report ("it detects your
     * clicks, but only updates when i reclick the window! just need
     * the window to update from when new entry to debug is read, not
     * wait for my click") - this whole function only ever drew into
     * the offscreen buffer (buf), never presented it to the real
     * window (win) - it relied on some OTHER later redraw path (one
     * triggered by the next real click) to actually push the buffer
     * to screen. That's exactly the observed symptom: content was
     * always correctly composed (a screenshot tool reading the buffer
     * directly showed it), but nothing reached the screen until an
     * unrelated event forced a real present. Every other real redraw
     * path in this file (search XCopyArea) already does this - this
     * one just never did. */
    XCopyArea(dpy, buf, win, gc, 0, 0, (unsigned)g_window->w, (unsigned)g_window->h, 0, 0);
    XFlush(dpy);
}

/* REAL, ported verbatim 2026-08-25 (Stage 3 bookmarks port) from
 * khtpm_hq_render.c's own hq_run_detached()/g_input_elem mechanism -
 * bookmarks' own onClick="open:<path>" row-open and
 * onClick="input:<file>|<postcmd>" New+ field both depend on this;
 * neither existed anywhere in this binary before. Kept generic (not
 * db-hq-specific) since any future database-window consumer gets it
 * for free, same reasoning khtpm_hq_render.c's own header used. */
static void hq_run_detached(int is_open, const char *arg) {
    pid_t mid = fork();
    if (mid < 0) return;
    if (mid == 0) {
        pid_t gc = fork();
        if (gc < 0) _exit(127);
        if (gc == 0) {
            setsid();
            if (is_open) {
                setenv("GDK_BACKEND", "x11", 1);
                execlp("xdg-open", "xdg-open", arg, (char *)NULL);
            } else {
                execl("/bin/sh", "sh", "-c", arg, (char *)NULL);
            }
            _exit(127);
        }
        _exit(0);
    }
    waitpid(mid, NULL, 0);
}

static Elem *g_input_elem = NULL;
static char g_input_buf[256];

static void input_disarm(void) {
    g_input_elem = NULL;
    g_input_buf[0] = '\0';
}



/* Write rmmv_active.txt in-process (all three fields) and update
 * g_pal_active_* so A-E / tileset highlight moves on press 1.
 * Detached set_rmmv + 1s manager poll is why it took 2-3 presses. */
static void dbhq_rmmv_write_active(const char *field, const char *val) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/rmmv_active.txt", g_package_dir);
    char tab[8] = "", tileset[64] = "", dir[64] = "tilesets";
    FILE *in = fopen(path, "r");
    if (in) {
        char line[128];
        while (fgets(line, sizeof(line), in)) {
            size_t n = strlen(line);
            while (n > 0 && (line[n-1]=='\n' || line[n-1]=='\r')) line[--n] = '\0';
            if (!strncmp(line, "tab=", 4)) snprintf(tab, sizeof(tab), "%s", line + 4);
            else if (!strncmp(line, "tileset=", 8)) snprintf(tileset, sizeof(tileset), "%s", line + 8);
            else if (!strncmp(line, "dir=", 4)) snprintf(dir, sizeof(dir), "%s", line + 4);
        }
        fclose(in);
    }
    if (strcmp(field, "tab") == 0) snprintf(tab, sizeof(tab), "%s", val);
    else if (strcmp(field, "tileset") == 0) snprintf(tileset, sizeof(tileset), "%s", val);
    else if (strcmp(field, "dir") == 0) snprintf(dir, sizeof(dir), "%s", val);
    FILE *out = fopen(path, "w");
    if (!out) return;
    if (tab[0]) fprintf(out, "tab=%s\n", tab);
    if (tileset[0]) fprintf(out, "tileset=%s\n", tileset);
    fprintf(out, "dir=%s\n", dir);
    fclose(out);
}


static int dbhq_rmmv_wait_publish(const char *want_cat, const char *want_set, const char *want_dir) {
    /* One click must wait until the manager actually rewrote options
     * (not until the next human click). Cache miss can take seconds. */
    for (int n = 0; n < 80; n++) {
        int och = dbhq_load_palette_options();
        int sch = dbhq_load_palette_state();
        if (och || sch) {
            int ok = 1;
            if (want_cat && want_cat[0] && strcmp(g_pal_active_category, want_cat) != 0) ok = 0;
            if (want_set && want_set[0] && strcmp(g_pal_active_tileset, want_set) != 0) ok = 0;
            if (want_dir && want_dir[0] && strcmp(g_pal_active_dir, want_dir) != 0) ok = 0;
            if (ok) return 1;
        }
        usleep(100000);
    }
    return 0;
}

static int dbhq_rmmv_apply_onclick(const char *onclick) {
    const char *p;
    if ((p = strstr(onclick, "set-rmmv-tab "))) {
        const char *q = strrchr(p, '\'');
        if (!q || q == p) return 0;
        const char *s = q - 1;
        while (s > p && *s != '\'') s--;
        if (*s != '\'' || q - s < 2) return 0;
        char letter[4] = {0};
        letter[0] = s[1];
        dbhq_rmmv_write_active("tab", letter);
        char want = letter[0];
        if (want >= 'a' && want <= 'z') want = (char)(want - 32);
        for (int i = 0; i < g_pal_n_tabs; i++) {
            if (g_pal_opt_tab_letter[i] == want) {
                snprintf(g_pal_active_category, sizeof(g_pal_active_category), "%s", g_pal_opt_tab_cat[i]);
                break;
            }
        }
        dbhq_rmmv_wait_publish(g_pal_active_category, NULL, NULL);
        return 1;
    }
    if ((p = strstr(onclick, "set-rmmv-tileset "))) {
        const char *q = strrchr(p, '\'');
        if (!q || q == p) return 0;
        const char *s = q - 1;
        while (s > p && *s != '\'') s--;
        if (*s != '\'') return 0;
        char key[64];
        size_t klen = (size_t)(q - s - 1);
        if (klen >= sizeof(key)) klen = sizeof(key) - 1;
        memcpy(key, s + 1, klen);
        key[klen] = '\0';
        dbhq_rmmv_write_active("tileset", key);
        snprintf(g_pal_active_tileset, sizeof(g_pal_active_tileset), "%s", key);
        dbhq_rmmv_wait_publish(NULL, key, NULL);
        return 1;
    }
    if ((p = strstr(onclick, "set-rmmv-dir "))) {
        const char *q = strrchr(p, 39);
        if (!q || q == p) return 0;
        const char *s = q - 1;
        while (s > p && *s != 39) s--;
        if (*s != 39) return 0;
        char key[32];
        size_t klen = (size_t)(q - s - 1);
        if (klen >= sizeof(key)) klen = sizeof(key) - 1;
        memcpy(key, s + 1, klen);
        key[klen] = 0;
        dbhq_rmmv_write_active("dir", key);
        snprintf(g_pal_active_dir, sizeof(g_pal_active_dir), "%s", key);
        dbhq_rmmv_wait_publish(NULL, NULL, key);
        return 1;
    }
    return 0;
}

static void dbhq_activate_elem(Elem *hit) {
    if (!hit) return;
    if (strcmp(hit->tag, "closebtn") == 0) { g_quit = 1; return; }
    /* REAL, ported 2026-08-25 (Stage 3 bookmarks port) - generic
     * data-driven onClick dispatch, checked BEFORE the domain-specific
     * tag branches below (same order khtpm_hq_render.c's own
     * activate_elem() used: an element with its own onclick handles
     * itself, domain branches only run for elements WITHOUT one). */
    if (hit->onclick[0]) {
        /* REAL, NEW 2026-08-25 (live instruction: real nav for the
         * scroll arrows) - same generic dispatch every other onclick
         * verb uses, so Enter-on-focused-nav and a mouse click share
         * this ONE code path, not two. */
        if (strcmp(hit->onclick, "ACTIVATE") == 0 || strncmp(hit->onclick, "ACTIVATE:", 9) == 0) {
            dbhq_activate_scope(hit);
            dbhq_assign_nav_indices(g_window);
            dbhq_redraw_content();
            return;
        }
        /* Not "BACK": that is chtpm page-stack (popup dispatch /
         * menu.chtpm action="BACK"). Scope-pop pairs with ACTIVATE. */
        if (strcmp(hit->onclick, "DEACTIVATE") == 0) {
            dbhq_back_scope();
            dbhq_assign_nav_indices(g_window);
            dbhq_redraw_content();
            return;
        }
        if (strcmp(hit->onclick, "scroll:up") == 0 || strcmp(hit->onclick, "scroll:down") == 0) {
            g_pal_scroll += (strcmp(hit->onclick, "scroll:down") == 0) ? 1 : -1;
            dbhq_redraw_content();
            return;
        }
        if (strncmp(hit->onclick, "input:", 6) == 0) {
            g_input_elem = hit;
            g_input_buf[0] = '\0';
            dbhq_redraw_content();
            return;
        }
        if (strncmp(hit->onclick, "open:", 5) == 0)
            hq_run_detached(1, hit->onclick + 5);
        else if (strncmp(hit->onclick, "exec:", 5) == 0) {
            /* rmmv tab/chooser: write active file HERE and re-inject so
             * highlight moves on press 1. Still exec the script (now
             * preserves all fields) so the manager's 100ms poll agrees. */
            if (dbhq_rmmv_apply_onclick(hit->onclick)) {
                Elem *panel = find_by_tag(g_window, "panel");
                dbhq_inject_palette_tiles(panel);
            }
            hq_run_detached(0, hit->onclick + 5);
            /* REAL DESIGN HISTORY 2026-08-29 - in-process XGrabPointer/
             * XQueryPointer-polling click-capture (g_pal_rmmv_armed)
             * was tried here and REMOVED again same day: fixed
             * synthetic clicks, confirmed via a real standalone
             * diagnostic tool (tp_debug_click_watcher.c) that it did
             * NOT fix real ones either - every real click ever
             * captured fell inside an already-open khtpm window, never
             * on bare desktop (this Mutter/XWayland setup only makes
             * real click state visible to X11 when the click lands on
             * a real XWayland surface). Real fix, direct instruction
             * ("maybe we do need a screen wide transparent click
             * capture surface?"): tp_arm_placer_rmmv.+x (spawned via
             * the exec above, palettes_menu.sh's own arm_rmmv())
             * creates a real full-screen InputOnly window tiled AROUND
             * this picker window and waits for a normal ButtonPress on
             * it - no grab, no polling, in this process or any other.
             * See that file's own header for the full history. */
        }
        /* Task 6 (2026-08-26) - the embedded Common Event editor's own
         * buttons (dbhq_ce_inject_panel()), dispatched the same generic
         * onclick[0] way as every other real verb here. Delegated to a
         * function defined later in the file (after the g_evhq_* globals
         * it needs) - see dbhq_ce_handle_onclick()'s own definition. */
        else if (strncmp(hit->onclick, "CE:", 3) == 0)
            dbhq_ce_handle_onclick(hit->onclick);
        /* REAL BUG FIX (2026-08-26, direct live report: "cancel doesn't
         * work") - db-hq's own embedded editor shows the SAME picker
         * overlay events-hq does (dbhq_ce_draw_overlay_if_needed() ->
         * evhq_draw_picker_overlay()), so its real onclick-carrying
         * Elems need the same PICKER: dispatch here too, or a real mouse
         * click on them in db-hq mode falls through to nothing, same
         * bug as events-hq had. Shared with evhq_activate_elem() via
         * evhq_dispatch_picker_onclick() so the logic isn't duplicated. */
        else if (strncmp(hit->onclick, "PICKER:", 7) == 0)
            evhq_dispatch_picker_onclick(hit->onclick);
        dbhq_redraw_content();
        return;
    }
    if (strcmp(hit->tag, "tab") == 0) {
        for (int i = 0; i < DB_HQ_N_TABS; i++) if (strcmp(hit->label, DB_HQ_TAB_LABELS[i]) == 0) { g_dbhq_current_tab = i; break; }
        g_dbhq_ce_editing = 0;
        dbhq_restore_tab_content();
        dbhq_redraw_content();
        return;
    }
    if (strcmp(hit->tag, "item") == 0) {
        if (g_dbhq_current_tab == DB_HQ_ACTORS_TAB) {
            int idx = atoi(hit->id);
            if (idx >= 0 && idx < g_dbhq_n_actors) g_dbhq_selected_actor = idx;
            dbhq_show_actors();
            dbhq_redraw_content();
            return;
        }
        {
            int li = dbhq_list_idx_for_tab(g_dbhq_current_tab);
            if (li >= 0) {
                int idx = atoi(hit->id);
                if (idx >= 0 && idx < g_dbhq_list_n[li]) g_dbhq_list_sel[li] = idx;
                dbhq_show_list_tab();
                dbhq_redraw_content();
                return;
            }
        }
        if (g_is_stats_hq) {
            /* REAL FIX 2026-08-25 - match by the real index stashed in
             * item->id (dbhq_inject_sidebar_items()'s own g_is_stats_hq
             * branch), not by label - the label is now just the date,
             * not the full raw data line, so it can't be matched back
             * against g_dbhq_events[] by string equality anymore. */
            int idx = atoi(hit->id);
            if (idx >= 0 && idx < g_dbhq_n_events) g_dbhq_selected_event = idx;
        } else {
            for (int i = 0; i < g_dbhq_n_events; i++) if (strcmp(g_dbhq_events[i], hit->label) == 0) { g_dbhq_selected_event = i; break; }
        }
        Elem *sidebar = find_by_tag(g_window, "sidebar");
        dbhq_inject_sidebar_items(sidebar);
        if (g_is_stats_hq) {
            stats_populate_panel(g_dbhq_selected_event);
        } else if (g_dbhq_selected_event >= 0) {
            /* Task 6 (2026-08-26) - real, embedded RPG-Maker-style
             * Common Events editor: selecting a sidebar item opens it
             * inline (same window, same panel), no separate app. */
            dbhq_ce_open(g_dbhq_events[g_dbhq_selected_event]);
        }
        dbhq_redraw_content();
        return;
    }
}

static void dbhq_handle_click(int px, int py) {
    /* REAL BUG FIX (2026-08-26, direct live report: "cancel doesn't
     * work") - same real fix as evhq_handle_click()'s own copy of this
     * comment: the picker's Elems aren't children of g_window, so
     * hit_test(g_window,...) below could never find them. Hit-test
     * directly against g_nav[] (which the picker owns exclusively while
     * open) instead, checked first, same as the other modes do for
     * their own synthetic/off-tree Elems (close button, scroll arrows). */
    if (g_dbhq_ce_editing && g_evhq_picker_open) {
        for (int i = 0; i < g_n_nav; i++) {
            Elem *e = g_nav[i];
            if (px >= e->x && px < e->x + e->w && py >= e->y && py < e->y + e->h) {
                if (!click_focus_then_activate(e)) {
                    /* REAL FIX 2026-08-29 - same root cause as
                     * evhq_handle_click()'s own matching fix (see its
                     * comment): evhq_draw_picker_overlay() stomps
                     * g_focus_nav from g_evhq_picker_focus/
                     * g_evhq_active_field on every redraw; sync
                     * whichever is live before redrawing or the click
                     * that just moved focus gets silently undone. */
                    if (g_evhq_picker_type < 0) g_evhq_picker_focus = e->nav_index;
                    else g_evhq_active_field = e->nav_index - 1;
                    dbhq_redraw_content();
                    return;
                }
                dbhq_activate_elem(e);
                return;
            }
        }
        return;
    }
    if (px >= g_dbhq_close_elem->x && px < g_dbhq_close_elem->x + g_dbhq_close_elem->w &&
        py >= g_dbhq_close_elem->y && py < g_dbhq_close_elem->y + g_dbhq_close_elem->h) {
        g_focus_nav = g_dbhq_close_elem->nav_index;
        dbhq_activate_elem(g_dbhq_close_elem);
        return;
    }
    /* REAL, NEW 2026-08-25 (live instruction: real nav for the scroll
     * arrows) - same "synthetic elem outside the parsed tree, checked
     * explicitly before hit_test()" pattern the close button above
     * already uses. A disabled arrow's onclick[0]=='\0' (cleared in
     * dbhq_layout_pass()) makes dbhq_activate_elem() a safe no-op for
     * it - no separate disabled check needed here either. */
    if (g_pal_has_grid) {
        if (px >= g_pal_arrow_up->x && px < g_pal_arrow_up->x + g_pal_arrow_up->w &&
            py >= g_pal_arrow_up->y && py < g_pal_arrow_up->y + g_pal_arrow_up->h) {
            if (g_pal_arrow_up->nav_index > 0) g_focus_nav = g_pal_arrow_up->nav_index;
            dbhq_activate_elem(g_pal_arrow_up);
            return;
        }
        if (px >= g_pal_arrow_down->x && px < g_pal_arrow_down->x + g_pal_arrow_down->w &&
            py >= g_pal_arrow_down->y && py < g_pal_arrow_down->y + g_pal_arrow_down->h) {
            if (g_pal_arrow_down->nav_index > 0) g_focus_nav = g_pal_arrow_down->nav_index;
            dbhq_activate_elem(g_pal_arrow_down);
            return;
        }
    }
    Elem *hit = hit_test(g_window, px, py);
    if (!hit) return;
    if (!click_focus_then_activate(hit)) { dbhq_redraw_content(); return; }
    dbhq_activate_elem(hit);
}

static void dbhq_handle_key(KeySym ks, char ch) {
    /* Task 6 (2026-08-26) - the embedded Common Event Add Command
     * picker owns keys next, same priority order as events-hq's own
     * top-level key dispatch gives its picker (checked before the
     * input-field/nav-digit handling below, since a picker being open
     * should always win). */
    int ce_consumed = 0;
    dbhq_ce_handle_key_if_needed(ks, ch, &ce_consumed);
    if (ce_consumed) return;
    /* REAL, ported 2026-08-25 (Stage 3 bookmarks port) - armed input
     * field owns every key first, same order/behavior as
     * khtpm_hq_render.c's own handle_key(). First (only) consumer:
     * bookmarks' New+ path entry. */
    if (g_input_elem) {
        if (ks == XK_Escape) { input_disarm(); dbhq_redraw_content(); return; }
        if (ks == XK_Return || ks == XK_KP_Enter) {
            const char *spec = g_input_elem->onclick + 6;
            char target[PATH_BUF] = "";
            char post[sizeof(g_input_elem->onclick)] = ""; /* real fix 2026-08-25 - onclick itself was just bumped 512->1536 after a real truncation bug; keep this in lockstep by deriving from it, not a second guessed constant */
            const char *bar = strchr(spec, '|');
            if (bar) {
                size_t tl = (size_t)(bar - spec);
                if (tl >= sizeof(target)) tl = sizeof(target) - 1;
                memcpy(target, spec, tl);
                snprintf(post, sizeof(post), "%s", bar + 1);
            } else {
                snprintf(target, sizeof(target), "%s", spec);
            }
            if (target[0]) {
                FILE *f = fopen(target, "a");
                if (f) { fprintf(f, "%s\n", g_input_buf); fclose(f); }
            }
            input_disarm();
            g_dbhq_digit_accum = 0;
            if (post[0]) hq_run_detached(0, post);
            dbhq_redraw_content();
            return;
        }
        if (ks == XK_BackSpace) {
            size_t L = strlen(g_input_buf);
            if (L > 0) {
                L--;
                while (L > 0 && (g_input_buf[L] & 0xC0) == 0x80) L--;
                g_input_buf[L] = '\0';
            }
            dbhq_redraw_content();
            return;
        }
        if (ch >= 32 && ch <= 126 && strlen(g_input_buf) < sizeof(g_input_buf) - 2) {
            size_t L = strlen(g_input_buf);
            g_input_buf[L] = ch;
            g_input_buf[L + 1] = '\0';
            dbhq_redraw_content();
            return;
        }
        return;
    }
    if (ks == XK_Return || ks == XK_KP_Enter) {
        if (g_dbhq_digit_accum > 0 && g_dbhq_digit_accum <= g_n_nav) g_focus_nav = g_dbhq_digit_accum;
        g_dbhq_digit_accum = 0;
        if (g_focus_nav >= 1 && g_focus_nav <= g_n_nav) dbhq_activate_elem(g_nav[g_focus_nav - 1]);
        return;
    }
    if (ks == XK_Escape) {
        if (g_dbhq_digit_accum > 0) { g_dbhq_digit_accum = 0; return; }
        g_quit = 1;
        return;
    }
    if (ch >= '0' && ch <= '9') {
        int d = ch - '0';
        int new_val = g_dbhq_digit_accum * 10 + d;
        if (new_val > 0 && new_val <= g_n_nav) {
            g_dbhq_digit_accum = new_val;
            g_focus_nav = new_val;
        } else if (d > 0 && d <= g_n_nav) {
            g_dbhq_digit_accum = d;
            g_focus_nav = d;
        } else {
            g_dbhq_digit_accum = 0;
        }
        return;
    }
    /* REAL FIX 2026-08-25 (live report: "i want emoji up down arrows to
     * move down entire row, instead of sideways to skip to next row") -
     * Up/Down used to be aliased straight onto Left/Right (+-1 linear
     * index), so on a 10-wide grid, Up/Down behaved identically to
     * Left/Right and only ever felt like it "skipped to next row" once
     * it crossed a row boundary. Real fix, palettes only: Up/Down step
     * by the grid's own column count (detected at runtime from how many
     * leading g_nav[] entries share the first tile's y - no hardcoded
     * column count to keep in sync with palettes_menu.sh's own
     * emit_tiles_matrix call), landing on the tile directly above/below.
     * Left/Right keep the plain +-1 linear step within a row. db-hq's
     * own sidebar/panel (not a grid) keeps the original Up/Down==Left/
     * Right behavior unchanged. */
    if (g_pal_has_grid && g_n_nav > 0 && (ks == XK_Up || ks == XK_Down)) {
        int cols = 1;
        int y0 = g_nav[0]->y;
        for (int i = 1; i < g_n_nav; i++) {
            if (g_nav[i]->y == y0) cols++; else break;
        }
        int step = (ks == XK_Up) ? -cols : cols;
        int nv = g_focus_nav + step;
        if (nv >= 1 && nv <= g_n_nav) {
            g_focus_nav = nv;
        } else if (g_pal_has_grid) {
            /* REAL FIX 2026-08-25, direct instruction ("no UI element
             * without mirror kbd accessibility... like in open-hai") -
             * off-screen rows are excluded from nav numbering entirely
             * (by design, see assign_palettes_nav()'s own w>0/h>0 check),
             * so hitting the top/bottom edge with plain arrow keys used
             * to just do nothing - a keyboard-only user could never
             * reach anything below/above the fold at all, only Page_Up/
             * Down (still keyboard, but a separate, less discoverable
             * key) could scroll. Real fix: arrow keys now auto-scroll
             * into view at the edge too, same as any accessible list -
             * scroll one row, re-layout/re-number (same tiles-shift-out-
             * of-nav mechanism Page_Up/Down already uses), then land on
             * the newly-revealed row at the SAME column, not just
             * wherever nav index arithmetic happens to point. */
            int col_offset = cols > 0 ? (g_focus_nav - 1) % cols : 0;
            g_pal_scroll += (ks == XK_Down) ? 1 : -1;
            dbhq_layout_pass(g_window);
            dbhq_assign_nav_indices(g_window);
            if (ks == XK_Down) {
                int last_row_start = g_n_nav - cols;
                if (last_row_start < 0) last_row_start = 0;
                int target = last_row_start + col_offset + 1;
                g_focus_nav = (target >= 1 && target <= g_n_nav) ? target : g_n_nav;
            } else {
                int target = col_offset + 1;
                g_focus_nav = (target >= 1 && target <= g_n_nav) ? target : 1;
            }
        }
        g_dbhq_digit_accum = 0;
        return;
    }
    if (ks == XK_Up || ks == XK_Left) {
        if (g_focus_nav > 1) g_focus_nav--;
        g_dbhq_digit_accum = 0;
        return;
    }
    if (ks == XK_Tab || ks == XK_ISO_Left_Tab) {
        /* Two db-hq share one history file; only the focused window
         * may cycle, or both processes Tab-cycle and fight. */
        if (g_dbhq_has_real_focus) nav_tab_cycle();
        g_dbhq_digit_accum = 0;
        return;
    }
    if (ks == XK_Down || ks == XK_Right) {
        if (g_focus_nav < g_n_nav) g_focus_nav++;
        g_dbhq_digit_accum = 0;
        return;
    }
    /* REAL, ported 2026-08-25 - palette matrix paging, verbatim from
     * khtpm_hq_render.c's own real, live-verified mechanism. One page =
     * visible-1 rows so the top row stays for context. Re-layout +
     * re-number immediately so the scroll clamp/thumb/visible-tile-only
     * nav numbering all reflect the new position before the next redraw. */
    if (ks == XK_Page_Up || ks == XK_Page_Down) {
        if (g_pal_has_grid) {
            int step = g_pal_visible_rows > 1 ? g_pal_visible_rows - 1 : 1;
            g_pal_scroll += (ks == XK_Page_Down) ? step : -step;
            dbhq_layout_pass(g_window);
            dbhq_assign_nav_indices(g_window);
        }
        g_dbhq_digit_accum = 0;
        return;
    }
    g_dbhq_digit_accum = 0;
}
/* ====================== end db-hq mode block ========================= */

/* ======================================================================
 * REAL, events-hq-mode-only state + functions (§5d.11, 2026-08-16) -
 * ported from khtpm_events_hq_render.c. UNLIKE db-hq's own port, this
 * app's own draw_elem()/render_tree()/font_for()/alloc_pixel()/
 * xft_color() turned out NOT to be behaviorally identical to the
 * shared khtpm_draw_core.c versions (single-arg signatures, no hover
 * state, inline tab-active-fill special case) - kept here as real,
 * evhq_-prefixed per-mode copies rather than silently reusing the
 * shared ones, a real, documented exception to the "already shared via
 * khtpm_draw_core.c" assumption that held for db-hq. Also real,
 * genuinely different from db-hq: events-hq is legitimately
 * MULTI-INSTANCE (one window per entity's event_pkg, scoped by
 * pkg_dir - see button.sh's own same_entity_pids()), takes 2 extra
 * real argv params (pkg_dir/entity_label) db-hq doesn't have, and its
 * own module launch passes 3 args not 1. Harmless, unused, when
 * g_is_events_hq is 0.
 * ====================================================================== */
static int g_is_events_hq = 0;
/* g_is_chat_hai removed 2026-09-01 - chat-hai's own hardcoded mode is
 * gone (migrated onto the generic sidebar/panel/scrolllist/cli_io
 * path, see chat_hai_projector.sh's own header comment); it now
 * carries no class= at all and is indistinguishable from any other
 * generic default-mode window. */
#define EVHQ_CHROME_H 26
static void dump_frame_png(void); /* forward decl - evhq_handle_key()'s own real 'p' case calls the shared one, defined later in this file */

static char g_evhq_pkg_dir[PATH_BUF];
static char g_evhq_entity_label[128];

static pid_t g_evhq_module_pid = -1;
static void evhq_cleanup_module(void) {
    if (g_evhq_module_pid > 0) {
        kill(g_evhq_module_pid, SIGTERM);
        waitpid(g_evhq_module_pid, NULL, WNOHANG);
        g_evhq_module_pid = -1;
    }
}
static void evhq_handle_term_signal(int sig) {
    (void)sig;
    evhq_cleanup_module();
    _exit(0);
}
static void evhq_launch_module(const char *src) {
    if (!src || !src[0]) return;
    char full_path[PATH_BUF];
    if (src[0] == '/') snprintf(full_path, sizeof(full_path), "%s", src);
    else snprintf(full_path, sizeof(full_path), "%s/%s", g_house_root, src);
    g_evhq_module_pid = fork();
    if (g_evhq_module_pid == 0) {
        execl(full_path, full_path, g_house_root, g_evhq_pkg_dir, g_evhq_entity_label, (char *)NULL);
        _exit(1);
    } else if (g_evhq_module_pid < 0) {
        fprintf(stderr, "khtpm_entity_menu_render: events-hq: launch_module: fork failed for %s\n", full_path);
        g_evhq_module_pid = -1;
    }
}

static unsigned char *g_evhq_sprite_pixels = NULL;
static int g_evhq_sprite_res = 0;
static void evhq_load_entity_sprite(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/../sprite.csv", g_evhq_pkg_dir);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[256];
    int res = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "# resolution=", 13) == 0) { res = atoi(line + 13); break; }
    }
    if (res <= 0) { fclose(f); return; }
    unsigned char *pixels = malloc((size_t)res * (size_t)res * 4);
    if (!pixels) { fclose(f); return; }
    int count = 0;
    while (count < res * res && fgets(line, sizeof(line), f)) {
        int r, g, b, a;
        if (sscanf(line, "%d,%d,%d,%d", &r, &g, &b, &a) == 4) {
            pixels[count * 4 + 0] = (unsigned char)r; pixels[count * 4 + 1] = (unsigned char)g;
            pixels[count * 4 + 2] = (unsigned char)b; pixels[count * 4 + 3] = (unsigned char)a;
            count++;
        }
    }
    fclose(f);
    if (count != res * res) { free(pixels); return; }
    g_evhq_sprite_pixels = pixels;
    g_evhq_sprite_res = res;
}

typedef struct {
    int id;
    char type[32];
    char params[512];
} EvhqCmdNode;
static void evhq_describe_command(const EvhqCmdNode *cmd, char *out, size_t outsz); /* Task 7 (2026-08-26) - real definition after g_evhq_cmd_defs/registry helpers */
#define EVHQ_MAX_CMDS 64
#define EVHQ_MAX_PAGES 16
static char g_evhq_pages[EVHQ_MAX_PAGES][64];
static int g_evhq_n_pages = 0;
static int g_evhq_current_page = 0;
/* Task 5 (2026-08-27) - Scripting|Scratch|Blueprints view-mode toolbar.
 * 0=Scripting (today's real command list, default/unchanged behavior),
 * 1=Scratch, 2=Blueprints (both real, clickable, nav-reachable STUBS
 * only - see PAL-VISUAL-SCRIPTING-PLAN.md, no block/node rendering
 * built yet). Shared by both events-hq mode and db-hq's embedded
 * Common Events editor (g_is_db_hq/g_is_events_hq are mutually
 * exclusive per process, so one variable is safe for both). */
static int g_evhq_view_mode = 0;
static const char *EVHQ_VIEW_STUB_LABELS[3] = { "", "Scratch view - coming soon", "Blueprints view - coming soon" };
static EvhqCmdNode g_evhq_cmds[EVHQ_MAX_CMDS];
static int g_evhq_n_cmds = 0;
/* VS task #2 (2026-08-28) - Scratch view blocks, populated from
 * SCRATCHBLOCK|<key>|<status> rows the manager publishes (switch =
 * ON/OFF, Change Gold/exec-shim op = the real value, e.g. 10/-10).
 * Rendered only in view mode 1 (Scratch). */
typedef struct { char key[128]; char status[16]; } EvhqBlockNode;
#define EVHQ_MAX_BLOCKS 16
static EvhqBlockNode g_evhq_blocks[EVHQ_MAX_BLOCKS];
static int g_evhq_n_blocks = 0;
static Elem g_evhq_block_slots[MAX_CHILDREN];
/* Visual block editor (2026-08-29) - click-to-place, nav-based, no
 * drag/drop: left sidebar of block pieces (click to pick, highlights),
 * then click the "[].<#>" slot to append via evhq_request_append_node().
 * All ops below are real registry commands (see DESIGN NOTE in
 * !.OPEN-2do-events-db-networking-2026-08-28.md); cls1/cls2 are the
 * two class tokens that make the piece look scratch-colored. */
typedef struct {
    const char *label;
    const char *type;
    const char *params;
    const char *cls1;
    const char *cls2;
} EvhqPaletteItem;
static const EvhqPaletteItem g_evhq_palette[] = {
    { "Change Gold",  "change_gold",     "amount=10",                          "scratch-block", "gold"   },
    { "Take Gold",    "take_gold",       "amount=10",                          "scratch-block", "green"  },
    { "Switch ON",    "control_switch",  "switch_name=flag_0|switch_value=1",  "scratch-block", "orange" },
    { "Show Text",    "show_text",       "text=Hello!",                        "scratch-block", "purple" },
    { "Wait",         "wait",            "ms=100",                             "scratch-block", "pink"   },
};
#define EVHQ_PALETTE_MAX 8
#define EVHQ_PALETTE_N ((int)(sizeof(g_evhq_palette) / sizeof(g_evhq_palette[0])))
static Elem g_evhq_palette_slots[EVHQ_PALETTE_MAX];
static Elem g_evhq_place_slots[2];
static int g_evhq_selected_palette = -1;
static char g_evhq_selected_type[32] = "";
static char g_evhq_selected_params[128] = "";
static const char *evhq_palette_cls_for_type(const char *type) {
    if (!type || !type[0]) return NULL;
    for (int i = 0; i < EVHQ_PALETTE_N; i++)
        if (strcmp(type, g_evhq_palette[i].type) == 0) return g_evhq_palette[i].cls2;
    return NULL;
}
static char g_evhq_trigger[64] = "(unknown)";
static char g_evhq_switch_name[128] = "";  /* for Common Events: configured switch to watch */
static char g_evhq_mgr_pages_state_path[PATH_BUF];
static char g_evhq_mgr_selected_page_path[PATH_BUF];
static char g_evhq_mgr_page_state_path[PATH_BUF];
static char g_evhq_mgr_action_path[PATH_BUF];
static time_t g_evhq_pages_state_mtime = 0;
static time_t g_evhq_page_state_mtime = 0;

static void evhq_init_manager_paths(void) {
    char mgr_dir[PATH_BUF];
    snprintf(mgr_dir, sizeof(mgr_dir), "%s/.hq_manager", g_evhq_pkg_dir);
    snprintf(g_evhq_mgr_pages_state_path, sizeof(g_evhq_mgr_pages_state_path), "%s/pages.state.txt", mgr_dir);
    snprintf(g_evhq_mgr_selected_page_path, sizeof(g_evhq_mgr_selected_page_path), "%s/selected_page.txt", mgr_dir);
    snprintf(g_evhq_mgr_page_state_path, sizeof(g_evhq_mgr_page_state_path), "%s/page.state.txt", mgr_dir);
    snprintf(g_evhq_mgr_action_path, sizeof(g_evhq_mgr_action_path), "%s/action.txt", mgr_dir);
}
/* REAL FIX 2026-08-25 (found live while capturing an H6 proof
 * presentation, not caught by any agent's own self-report): creating a
 * page via "+ New" only ever asked the MANAGER to create it - it never
 * selected the new page on the RENDER side. The new tab looked focused/
 * active in the UI (keyboard nav cursor landed there), but
 * g_evhq_current_page never advanced past whatever was selected before,
 * so evhq_write_selected_page() kept re-confirming the OLD page to the
 * manager every poll tick - Trigger/Commands silently kept showing the
 * old page's real content under the new page's tab. Set by the
 * new-page-btn activate handler; consumed here the first time the page
 * COUNT actually grows, auto-selecting the newest (highest-numbered)
 * page - matches "+ New" always appending, never inserting. */
static int g_evhq_pending_select_new_page = 0;
static unsigned long g_evhq_pages_cksum;
static unsigned long g_evhq_page_state_cksum;

static unsigned long evhq_file_cksum(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    unsigned long h = 5381;
    int c;
    while ((c = fgetc(f)) != EOF) h = ((h << 5) + h) + (unsigned char)c;
    fclose(f);
    return h;
}

static int evhq_load_pages(void) {
    struct stat st;
    if (stat(g_evhq_mgr_pages_state_path, &st) != 0) return 0;
    if (st.st_mtime == g_evhq_pages_state_mtime) return 0;
    unsigned long ck = evhq_file_cksum(g_evhq_mgr_pages_state_path);
    g_evhq_pages_state_mtime = st.st_mtime;
    if (ck == g_evhq_pages_cksum && g_evhq_n_pages >= 0) {
        /* manager rewrote identical bytes — not a new frame */
        return 0;
    }
    g_evhq_pages_cksum = ck;
    int prev_n_pages = g_evhq_n_pages;
    g_evhq_n_pages = 0;
    FILE *f = fopen(g_evhq_mgr_pages_state_path, "r");
    if (!f) return 0;
    char line[128];
    while (g_evhq_n_pages < EVHQ_MAX_PAGES && fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0') continue;
        snprintf(g_evhq_pages[g_evhq_n_pages], sizeof(g_evhq_pages[0]), "%s", line);
        g_evhq_n_pages++;
    }
    fclose(f);
    if (g_evhq_pending_select_new_page && g_evhq_n_pages > prev_n_pages) {
        g_evhq_current_page = g_evhq_n_pages - 1;
        g_evhq_pending_select_new_page = 0;
    }
    if (g_evhq_current_page >= g_evhq_n_pages) g_evhq_current_page = 0;
    return 1;
}
static void evhq_write_selected_page(void) {
    if (g_evhq_current_page < 0 || g_evhq_current_page >= g_evhq_n_pages) return;
    FILE *f = fopen(g_evhq_mgr_selected_page_path, "w");
    if (!f) return;
    fprintf(f, "%s\n", g_evhq_pages[g_evhq_current_page]);
    fclose(f);
}
static int evhq_load_page_state(void) {
    struct stat st;
    if (stat(g_evhq_mgr_page_state_path, &st) != 0) return 0;
    if (st.st_mtime == g_evhq_page_state_mtime) return 0;
    unsigned long ck = evhq_file_cksum(g_evhq_mgr_page_state_path);
    g_evhq_page_state_mtime = st.st_mtime;
    if (ck == g_evhq_page_state_cksum) return 0;
    g_evhq_page_state_cksum = ck;
    g_evhq_n_cmds = 0;
    g_evhq_n_blocks = 0;
    snprintf(g_evhq_trigger, sizeof(g_evhq_trigger), "(unset)");
    snprintf(g_evhq_switch_name, sizeof(g_evhq_switch_name), "");  /* clear previous switch name */
    FILE *f = fopen(g_evhq_mgr_page_state_path, "r");
    if (!f) return 1;
    char line[600];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strncmp(line, "TRIGGER|", 8) == 0) {
            snprintf(g_evhq_trigger, sizeof(g_evhq_trigger), "%s", line + 8);
        } else if (strncmp(line, "SWITCH|", 7) == 0) {
            snprintf(g_evhq_switch_name, sizeof(g_evhq_switch_name), "%s", line + 7);
        } else if (strncmp(line, "CMD|", 4) == 0 && g_evhq_n_cmds < EVHQ_MAX_CMDS) {
            char *p = line + 4;
            char *bar1 = strchr(p, '|');
            if (!bar1) continue;
            *bar1 = '\0';
            g_evhq_cmds[g_evhq_n_cmds].id = atoi(p);
            char *type_start = bar1 + 1;
            char *bar2 = strchr(type_start, '|');
            if (!bar2) continue;
            *bar2 = '\0';
            snprintf(g_evhq_cmds[g_evhq_n_cmds].type, sizeof(g_evhq_cmds[0].type), "%s", type_start);
            snprintf(g_evhq_cmds[g_evhq_n_cmds].params, sizeof(g_evhq_cmds[0].params), "%s", bar2 + 1);
            g_evhq_n_cmds++;
        } else if (strncmp(line, "SCRATCHBLOCK|", 13) == 0 && g_evhq_n_blocks < EVHQ_MAX_BLOCKS) {
            char *p = line + 13;
            char *bar = strchr(p, '|');
            if (!bar) continue;
            *bar = '\0';
            EvhqBlockNode *b = &g_evhq_blocks[g_evhq_n_blocks];
            memset(b, 0, sizeof(*b));
            snprintf(b->key, sizeof(b->key), "%s", p);
            snprintf(b->status, sizeof(b->status), "%s", bar + 1);
            g_evhq_n_blocks++;
        }
    }
    fclose(f);
    return 1;
}

static void evhq_request_append_node(const char *type, const char *params_line) {
    FILE *f = fopen(g_evhq_mgr_action_path, "w");
    if (!f) return;
    fprintf(f, "append:%s|%s\n", type, params_line);
    fclose(f);
}
/* Task 7 (2026-08-26) - real command editing, sibling to append above.
 * See khtpm_events_hq_manager.c's own "edit:" action handler. */
static void evhq_request_edit_node(int id, const char *type, const char *params_line) {
    FILE *f = fopen(g_evhq_mgr_action_path, "w");
    if (!f) return;
    fprintf(f, "edit:%d|%s|%s\n", id, type, params_line);
    fclose(f);
}

/* REAL, NEW 2026-08-29 (direct instruction: "trigger able from visual
 * nav / index, as usual... just a nav for delete") - sibling to
 * evhq_request_edit_node() above, same real action.txt boundary.
 * See khtpm_events_hq_manager.c's own new "delete:" action handler. */
static void evhq_request_delete_node(int id) {
    FILE *f = fopen(g_evhq_mgr_action_path, "w");
    if (!f) return;
    fprintf(f, "delete:%d\n", id);
    fclose(f);
}

static void evhq_request_trigger_update(const char *new_trigger) {
    /* Task H7 (2026-08-25) - request the manager rewrite condition.pdl's trigger */
    FILE *f = fopen(g_evhq_mgr_action_path, "w");
    if (!f) return;
    fprintf(f, "trigger:%s\n", new_trigger);
    fclose(f);
}

static int g_evhq_has_real_focus = 0;
static char g_evhq_last_key_label[32] = "";
static int g_evhq_dragging = 0;
static int g_evhq_drag_last_x = 0, g_evhq_drag_last_y = 0;
static int g_evhq_toolbar_y = 0, g_evhq_toolbar_h = 0;
static Elem g_evhq_close_elem_storage;
static Elem *g_evhq_close_elem = &g_evhq_close_elem_storage;
static int g_evhq_close_x, g_evhq_close_y, g_evhq_close_w, g_evhq_close_h;
static int g_evhq_digit_accum = 0;

static int g_evhq_picker_open = 0;
static int g_evhq_picker_type = -1;
static int g_evhq_picker_focus = 1;
/* REAL, NEW 2026-08-29 (live report: "why don't i see those events in
 * the event editor?") - the Add-Command type list was hardcoded to a
 * flat `i<16` cap with no scroll, from back when the registry had
 * exactly ~16 commands - the registry now has 22 (Task 1 added Select
 * Item/Scrolling Text/all 4 Character commands) and the picker box
 * itself (280px default from picker.chtpm) physically can't show more
 * than ~9 rows at 22px each regardless of any array-size fix, so the
 * last several commands were silently unreachable even by digit-jump.
 * g_evhq_picker_scroll is which real g_evhq_cmd_defs[] index is at the
 * top of the visible window; g_evhq_picker_visible_rows is how many
 * rows the box actually has room for this frame (computed in
 * evhq_draw_picker_overlay(), read back in evhq_handle_key() - same
 * "compute once at draw time, key handling reads the cached value"
 * shape used for other overlay state throughout this file). Digits/
 * arrows still move within the current visible window (same real
 * on-screen-position semantic used everywhere else in this house);
 * Page_Up/Page_Down scroll the window itself, same real keys the
 * command list/palette grid already use for the identical reason. */
static int g_evhq_picker_scroll = 0;
static int g_evhq_picker_visible_rows = 9;
static char g_evhq_field1[256] = "", g_evhq_field2[256] = "";
static int g_evhq_active_field = 0;
static int g_evhq_edit_cmd_id = -1; /* Task 7 (2026-08-26): -1 = Add Command flow, >=0 = editing that existing command's real id */

/* Task 6 (2026-08-26) - open a real common event in the SAME embedded
 * db-hq panel (RPG Maker MV/MZ shape: one dialog, sidebar list of
 * event slots + the real command editor together - NOT a separate
 * spawned window, direct instruction). Retargets the exact same
 * g_evhq_pkg_dir/g_evhq_entity_label globals events-hq already uses
 * for entities, then launches a real khtpm_events_hq_manager.+x
 * instance scoped to this common event's own event_pkg dir - the
 * manager doesn't care whether pkg_dir is under an entity or
 * common_events/, it's already generic. */
static void dbhq_ce_open(const char *ce_name) {
    if (!ce_name || !ce_name[0]) return;
    snprintf(g_evhq_pkg_dir, sizeof(g_evhq_pkg_dir), "%s/common_events/%s/event_pkg", g_house_root, ce_name);
    snprintf(g_evhq_entity_label, sizeof(g_evhq_entity_label), "%s", ce_name);
    snprintf(g_dbhq_ce_name, sizeof(g_dbhq_ce_name), "%s", ce_name);
    evhq_init_manager_paths();
    g_evhq_n_pages = 0; g_evhq_pages_state_mtime = 0;
    g_evhq_n_cmds = 0; g_evhq_page_state_mtime = 0;
    g_evhq_current_page = 0; g_evhq_pending_select_new_page = 0;
    g_evhq_picker_open = 0;
    snprintf(g_evhq_trigger, sizeof(g_evhq_trigger), "(loading)");
    evhq_launch_module("&.widgits/events-hq/ops/+x/khtpm_events_hq_manager.+x");
    g_dbhq_ce_editing = 1;
    g_dbhq_ce_needs_rebuild = 1;
}

/* Rebuilds the visible content of db-hq's own "panel" Elem to show the
 * selected common event's real trigger + command list, using the SAME
 * generic title/text/button tags db-hq's own panel layout pass
 * (dbhq_layout_pass, ~line 1120) already knows how to flex-stack - no
 * new layout code needed, only new children. Buttons get a real
 * onclick="CE:..." string, dispatched by the generic onclick branch in
 * dbhq_activate_elem() (added alongside this function) - the SAME
 * dispatch mechanism bookmarks' onClick="open:..." already uses. */
static Elem g_dbhq_panel_slots[MAX_CHILDREN]; /* see reusable_slot()'s own header comment */
/* REAL FIX 2026-08-29 (Part B) - real slot storage for Common Events'
 * own view-mode tabbar children + its Scratch/Blueprints stub content,
 * same reusable_slot() pattern every other injector in this file uses -
 * NOT part of g_dbhq_panel_slots since that pool is sized/indexed for
 * the flat Scripting-mode child list and this content replaces it
 * entirely in non-Scripting modes. */
static Elem g_dbhq_ce_viewtab_slots[3];
static Elem g_dbhq_ce_scratch_stub;

static int dbhq_ce_inject_panel(Elem *panel) {
    if (!panel) return 0;
    int pages_changed = evhq_load_pages();
    int state_changed = evhq_load_page_state();
    /* REAL BUG FIX 2026-08-26 (found via gdb backtrace, real SIGSEGV,
     * not guessed): elem_new() allocates from a FIXED-SIZE static pool
     * with no free/recycle mechanism (see khtpm_render_core.c). This
     * function used to rebuild panel->children - calling elem_new() for
     * every title/text/button - on EVERY ~150ms periodic tick
     * unconditionally, unlike dbhq_inject_sidebar_items()'s own much
     * rarer mtime-gated refresh. That leaked ~7 pool slots per tick
     * forever, exhausting the pool and crashing (SIGSEGV in
     * __vsnprintf_internal, confirmed live via `gdb -batch -ex run -ex
     * bt`) a few seconds into any real session. Real fix: only rebuild
     * when the underlying data actually changed (evhq_load_pages()/
     * evhq_load_page_state() are already self-mtime-gated and report
     * this), or on the first inject after dbhq_ce_open(). */
    if (!g_dbhq_ce_needs_rebuild && !pages_changed && !state_changed) return 0;
    g_dbhq_ce_needs_rebuild = 0;
    evhq_write_selected_page();
    /* Task 7 (2026-08-26) - real bug fix: descriptions came out empty
     * ("change_gold" with no params) because g_evhq_cmd_defs[] was only
     * ever loaded by the picker overlay's own draw call - if a common
     * event is opened and never has "+ Add Command" clicked, the
     * registry was never loaded and evhq_find_cmd_def() always returned
     * NULL. Load it here unconditionally (self-mtime-gated internally,
     * cheap to call every rebuild). */
    evhq_load_command_registry();
    panel->n_children = 0;
    int next_slot_index = 0;
    Elem *title = reusable_slot(g_dbhq_panel_slots, MAX_CHILDREN, next_slot_index++, "title");
    if (!title) return 0; /* pool exhausted - see elem_new()'s own NULL contract; nothing more we can safely build this pass */
    snprintf(title->classes[0], sizeof(title->classes[0]), "block-title"); title->n_classes = 1;
    snprintf(title->label, sizeof(title->label), "Common Event: %s", g_dbhq_ce_name);
    panel->children[panel->n_children++] = title;

    /* REAL FIX 2026-08-29 (EVENTS-HQ-RENDER-UNIFICATION-PLAN.md Part B)
     * - the same real Scripting/Scratch/Blueprints view-mode tabs
     * events-hq has, sharing the SAME g_evhq_view_mode global (already
     * shared between the two modes - see g_evhq_n_cmds/g_evhq_cmds
     * reuse just below). Real tabbar Elem, real "tab" children, same
     * onclick-prefix convention (dbhq_activate_elem()'s existing
     * generic dispatch already forwards unrecognized onclicks to
     * dbhq_ce_handle_onclick() while g_dbhq_ce_editing is set - see
     * that function's own new "CE:VIEWTAB:" case). */
    Elem *vtabs = reusable_slot(g_dbhq_panel_slots, MAX_CHILDREN, next_slot_index++, "tabbar");
    if (vtabs) {
        snprintf(vtabs->classes[0], sizeof(vtabs->classes[0]), "view-tabs"); vtabs->n_classes = 1;
        static const char *ce_view_labels[3] = { "Scripting", "Scratch", "Blueprints" };
        int tw = 0;
        for (int i = 0; i < 3; i++) {
            Elem *tab = reusable_slot(g_dbhq_ce_viewtab_slots, 3, i, "tab");
            if (!tab) break;
            snprintf(tab->classes[0], sizeof(tab->classes[0]), "view-tab"); tab->n_classes = 1;
            snprintf(tab->label, sizeof(tab->label), "%s", ce_view_labels[i]);
            char oc[16]; snprintf(oc, sizeof(oc), "CE:VIEWTAB:%d", i);
            snprintf(tab->onclick, sizeof(tab->onclick), "%s", oc);
            tab->active = (i == g_evhq_view_mode);
            css_compute_style(&g_sheet, tab->tag, NULL, tab->classes, tab->n_classes, tab->active, &tab->style);
            int this_w = evhq_measure_text_px(&tab->style, tab->label) + 34;
            tab->x = panel->x + tw; tab->y = panel->y + 4; tab->w = this_w; tab->h = 20;
            tw += this_w + 4;
            vtabs->children[vtabs->n_children++] = tab;
        }
        vtabs->x = panel->x; vtabs->y = panel->y; vtabs->w = tw; vtabs->h = 26;
        panel->children[panel->n_children++] = vtabs;
    }
    if (g_evhq_view_mode == 1) {
        /* Scratch mode - the real content Trigger/Switch/command-list/
         * +Add-Command below is Scripting-only; give the Scratch view
         * its own real stub Elem to build into, same shape events-hq's
         * own "viewmode-stub" panel gives it. */
        Elem *stub = reusable_slot(&g_dbhq_ce_scratch_stub, 1, 0, "panel");
        if (stub) {
            evhq_build_scratch_view(stub, panel->x, panel->y + 30, panel->h - 30, panel->w > 0 ? panel->w : g_window->w);
            panel->children[panel->n_children++] = stub;
        }
        return 1;
    }
    if (g_evhq_view_mode == 2) {
        Elem *stub = reusable_slot(&g_dbhq_ce_scratch_stub, 1, 0, "text");
        if (stub) {
            snprintf(stub->classes[0], sizeof(stub->classes[0]), "empty-msg"); stub->n_classes = 1;
            snprintf(stub->label, sizeof(stub->label), "Blueprints view - coming soon");
            stub->x = panel->x + 8; stub->y = panel->y + 40; stub->w = (panel->w > 0 ? panel->w : g_window->w) - 16; stub->h = 20;
            css_compute_style(&g_sheet, stub->tag, NULL, stub->classes, stub->n_classes, 0, &stub->style);
            panel->children[panel->n_children++] = stub;
        }
        return 1;
    }

    /* Direct instruction (2026-08-26): a real Trigger field, RPG Maker
     * MV/MZ shape (Common Events' own "General Settings" - None/Autorun/
     * Parallel, Switch only when a trigger needs one), positioned above
     * the command list ("Scripting"). Real, nav-reachable button (not
     * static text) - activating it cycles None -> Autorun -> Parallel ->
     * None, writing the new value via the SAME evhq_request_trigger_
     * update() entity events already use (reused, not reinvented). */
    Elem *trig = reusable_slot(g_dbhq_panel_slots, MAX_CHILDREN, next_slot_index++, "button");
    if (trig) {
        snprintf(trig->classes[0], sizeof(trig->classes[0]), "prop-value"); trig->n_classes = 1;
        snprintf(trig->id, sizeof(trig->id), "ce-trigger");
        snprintf(trig->onclick, sizeof(trig->onclick), "CE:TRIGGER");
        snprintf(trig->label, sizeof(trig->label), "Trigger: %s", g_evhq_trigger);
        if (panel->n_children < MAX_CHILDREN) panel->children[panel->n_children++] = trig;
    }

    /* REAL FIX (2026-08-27): a Switch field for Autorun/Parallel common events.
     * Uses the same cli-io mechanism bookmarks' "New+" button already uses.
     * Only relevant when trigger is Autorun or Parallel; greyed/hidden otherwise.
     * Stores switch name in condition.pdl via khtpm_events_hq_manager.c handler. */
    int show_switch_field = (strcasecmp(g_evhq_trigger, "Autorun") == 0 ||
                             strcasecmp(g_evhq_trigger, "Parallel") == 0);
    if (show_switch_field) {
        Elem *sw = reusable_slot(g_dbhq_panel_slots, MAX_CHILDREN, next_slot_index++, "button");
        if (sw) {
            snprintf(sw->classes[0], sizeof(sw->classes[0]), "prop-value"); sw->n_classes = 1;
            snprintf(sw->id, sizeof(sw->id), "ce-switch");

            /* cli-io pattern: input:<file>|<post cmd> - reuses existing mechanism */
            char target[PATH_BUF];
            snprintf(target, sizeof(target), "%s/#.desktop/.dbhq_ce_switch_name.txt", g_house_root);
            char post[900];
            snprintf(post, sizeof(post),
                "sh -c 'N=$(tail -1 \"%s\" | tr -d \"\\r\\n\"); [ -n \"$N\" ] && echo \"switch:$N\" >> \"%s/#.desktop/events_hq_history.txt\"'",
                target, g_house_root);
            snprintf(sw->onclick, sizeof(sw->onclick), "input:%s|%s", target, post);

            /* Display current switch name (read from condition.pdl by the manager) */
            if (g_evhq_switch_name[0]) {
                snprintf(sw->label, sizeof(sw->label), "Switch: %s", g_evhq_switch_name);
            } else {
                snprintf(sw->label, sizeof(sw->label), "Switch: (unset, using ce_%s)", g_dbhq_ce_name);
            }
            if (panel->n_children < MAX_CHILDREN) panel->children[panel->n_children++] = sw;
        }
    }

    if (g_evhq_n_cmds == 0) {
        Elem *e = reusable_slot(g_dbhq_panel_slots, MAX_CHILDREN, next_slot_index++, "text");
        if (e) {
            snprintf(e->classes[0], sizeof(e->classes[0]), "empty-msg"); e->n_classes = 1;
            snprintf(e->label, sizeof(e->label), "(no commands yet)");
            if (panel->n_children < MAX_CHILDREN) panel->children[panel->n_children++] = e;
        }
    } else {
        for (int i = 0; i < g_evhq_n_cmds && panel->n_children < MAX_CHILDREN; i++) {
            /* Task 7 (2026-08-26) - real, nav-reachable, editable row:
             * button tag (so dbhq_assign_nav_indices()'s existing
             * button-only panel loop numbers it for free, no separate
             * nav-assignment change needed), onclick delegates to the
             * SAME edit-picker events-hq uses, description generated
             * generically from the registry (never hand-write per-type
             * strings here). */
            Elem *e = reusable_slot(g_dbhq_panel_slots, MAX_CHILDREN, next_slot_index++, "button");
            if (!e) break; /* pool exhausted - stop, don't crash */
            char cls[48]; snprintf(cls, sizeof(cls), "cmd-%s", g_evhq_cmds[i].type);
            snprintf(e->classes[0], sizeof(e->classes[0]), "%s", cls); e->n_classes = 1;
            snprintf(e->id, sizeof(e->id), "cmd-row-%d", g_evhq_cmds[i].id);
            snprintf(e->onclick, sizeof(e->onclick), "CE:EDITCMD:%d", g_evhq_cmds[i].id);
            char desc[300]; evhq_describe_command(&g_evhq_cmds[i], desc, sizeof(desc));
            snprintf(e->label, sizeof(e->label), "%d. %s", g_evhq_cmds[i].id, desc);
            panel->children[panel->n_children++] = e;
        }
    }
    if (panel->n_children < MAX_CHILDREN) {
        Elem *add = reusable_slot(g_dbhq_panel_slots, MAX_CHILDREN, next_slot_index++, "button");
        if (add) {
            snprintf(add->classes[0], sizeof(add->classes[0]), "btn-primary"); add->n_classes = 1;
            snprintf(add->id, sizeof(add->id), "ce-add-command");
            snprintf(add->onclick, sizeof(add->onclick), "CE:ADDCMD");
            snprintf(add->label, sizeof(add->label), "+ Add Command");
            panel->children[panel->n_children++] = add;
        }
    }
    /* Direct instruction (2026-08-26): no Play button here either -
     * removed alongside "Back to list". */
    /* Direct instruction (2026-08-26): no "Back to list" button - the
     * sidebar list is always visible alongside this panel (RPG Maker MV/
     * MZ shape), so there's nothing to "go back" to. */
    return 1;
}


static void dbhq_restore_tab_content(void) {
    /* Switching away from Actors must rebuild CE/Terms chrome. Actors
     * rewrote sidebar+panel in place; without this, nav [12] Common
     * Events highlighted the tab but left Harold's panel on screen. */
    if (g_is_palettes || g_is_bookmarks || g_is_stats_hq) return;
    if (g_dbhq_current_tab == DB_HQ_ACTORS_TAB) {
        dbhq_show_actors();
        return;
    }
    if (dbhq_list_idx_for_tab(g_dbhq_current_tab) >= 0) {
        dbhq_show_list_tab();
        return;
    }
    if (g_dbhq_current_tab == DB_HQ_COMMON_EVENTS_TAB) {
        dbhq_load_common_events();
        Elem *sidebar = find_by_tag(g_window, "sidebar");
        dbhq_inject_sidebar_items(sidebar);
        if (g_dbhq_selected_event < 0 && g_dbhq_n_events > 0)
            g_dbhq_selected_event = 0;
        Elem *panel = find_by_tag(g_window, "panel");
        if (g_dbhq_selected_event >= 0 && g_dbhq_selected_event < g_dbhq_n_events) {
            dbhq_ce_open(g_dbhq_events[g_dbhq_selected_event]);
            dbhq_ce_inject_panel(panel);
        } else if (panel) {
            panel->n_children = 0;
        }
        return;
    }
    if (g_dbhq_current_tab == DB_HQ_TERMS_TAB) {
        dbhq_load_common_events();
        Elem *sidebar = find_by_tag(g_window, "sidebar");
        dbhq_inject_sidebar_items(sidebar);
        return;
    }
    /* placeholder tabs: drop actor/CE children so gray message is honest */
    {
        Elem *sidebar = find_by_tag(g_window, "sidebar");
        if (sidebar) sidebar->n_children = 0;
        Elem *panel = find_by_tag(g_window, "panel");
        if (panel) panel->n_children = 0;
    }
}

/* Real definition of the forward-declared dispatcher (see the prototype
 * above dbhq_activate_elem()) - the embedded Common Event editor's own
 * verbs, reusing the EXACT same picker/action-file state and mechanism
 * events-hq already uses for entities (g_evhq_picker_open/g_evhq_mgr_
 * action_path/etc.), just with g_evhq_pkg_dir retargeted by dbhq_ce_
 * open() to the selected common event instead of an entity. */
static void dbhq_ce_handle_onclick(const char *onclick) {
    if (strcmp(onclick, "CE:ADDCMD") == 0) {
        g_evhq_picker_open = 1; g_evhq_picker_type = -1; g_evhq_picker_focus = 1; g_evhq_picker_scroll = 0;
        g_evhq_field1[0] = '\0'; g_evhq_field2[0] = '\0'; g_evhq_active_field = 0;
        g_evhq_edit_cmd_id = -1;
    } else if (strncmp(onclick, "CE:EDITCMD:", 11) == 0) {
        /* Task 7 (2026-08-26) - same real edit flow as events-hq's own
         * "cmd-edit-<id>" rows, just reached via db-hq's onclick-prefix
         * dispatch convention instead of an id check. */
        int target_id = atoi(onclick + 11);
        for (int i = 0; i < g_evhq_n_cmds; i++) if (g_evhq_cmds[i].id == target_id) { evhq_open_edit_picker(i); break; }
    } else if (strcmp(onclick, "CE:NEWPAGE") == 0) {
        FILE *af = fopen(g_evhq_mgr_action_path, "w");
        if (af) { fprintf(af, "new_page"); fclose(af); }
        g_evhq_pending_select_new_page = 1;
    } else if (strcmp(onclick, "CE:TRIGGER") == 0) {
        /* Direct instruction (2026-08-26): RPG Maker MV/MZ-style trigger
         * field, cycled None -> Autorun -> Parallel -> None on activate
         * (no free-text typing needed for this closed set). Switch-
         * condition field is a real, separate follow-up, not built yet -
         * see dbhq_ce_inject_panel()'s own comment on this field. */
        const char *next = "None";
        if (strcasecmp(g_evhq_trigger, "None") == 0) next = "Autorun";
        else if (strcasecmp(g_evhq_trigger, "Autorun") == 0) next = "Parallel";
        evhq_request_trigger_update(next);
    } else if (strncmp(onclick, "CE:VIEWTAB:", 11) == 0) {
        /* REAL FIX 2026-08-29 (Part B) - Common Events gets the SAME
         * Scripting/Scratch/Blueprints view modes events-hq has, via
         * the SAME shared g_evhq_view_mode global (already shared
         * between the two modes, see dbhq_ce_inject_panel()'s own real
         * reuse of g_evhq_n_cmds/g_evhq_cmds/etc) - not a second,
         * db-hq-only view-mode concept. */
        g_evhq_view_mode = atoi(onclick + 11);
        g_dbhq_ce_needs_rebuild = 1;
    } else if (evhq_handle_block_onclick(onclick)) {
        g_dbhq_ce_needs_rebuild = 1;
    }
}

/* Real definitions of the other two forward-declared hooks (see the
 * prototypes above dbhq_activate_elem()) - db-hq's own redraw/key-dispatch
 * functions are defined earlier in the file than these g_evhq_* globals,
 * so they call through these thin, always-safe-to-call wrappers instead
 * of touching the globals directly. */
static void dbhq_ce_draw_overlay_if_needed(void) {
    if (g_dbhq_ce_editing && g_evhq_picker_open) evhq_draw_picker_overlay();
}
static void dbhq_ce_handle_key_if_needed(KeySym ks, char ch, int *consumed) {
    *consumed = 0;
    if (!g_dbhq_ce_editing || !g_evhq_picker_open) return;
    evhq_handle_key(ks, ch);
    dbhq_redraw_content();
    *consumed = 1;
}

/* REAL, 2026-08-26 (direct instruction: "we never hardcode stuff,
 * always keeping things super modular and abstract" - full rationale
 * in #.ref/menu/EVENT-COMMAND-REGISTRY-ARCHITECTURE.md) - the Add
 * Command picker's type list, field prompts, and field count are now
 * loaded from the SAME registry file khtpm_events_hq_manager.c's
 * compile_page() reads (#.ref/menu/event_commands.registry.pdl), not
 * hardcoded arrays. Adding a new SIMPLE command needs zero changes
 * here - just a new COMMAND block in the registry. */
#define EVHQ_MAX_CMD_DEFS 48
typedef struct {
    char type[48];
    char label[64];
    char field1[64];
    char field2[64];
    char param_names[4][32];
    int n_params;
    char select2_options[8][32];
    int n_select2;
} EvhqCommandDef;
static EvhqCommandDef g_evhq_cmd_defs[EVHQ_MAX_CMD_DEFS];
static int g_evhq_n_cmd_defs = 0;
static time_t g_evhq_registry_mtime = 0;

static void evhq_load_command_registry(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.ref/menu/event_commands.registry.pdl", g_house_root);
    struct stat st;
    if (stat(path, &st) != 0) return;
    if (st.st_mtime == g_evhq_registry_mtime && g_evhq_n_cmd_defs > 0) return;
    g_evhq_registry_mtime = st.st_mtime;
    g_evhq_n_cmd_defs = 0;
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[600];
    EvhqCommandDef *cur = NULL;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        char *p = line;
        while (*p == ' ') p++;
        if (strncmp(p, "COMMAND ", 8) == 0) {
            if (g_evhq_n_cmd_defs >= EVHQ_MAX_CMD_DEFS) break;
            cur = &g_evhq_cmd_defs[g_evhq_n_cmd_defs++];
            memset(cur, 0, sizeof(*cur));
            snprintf(cur->type, sizeof(cur->type), "%s", p + 8);
        } else if (!cur) {
            continue;
        } else if (strncmp(p, "LABEL ", 6) == 0) {
            snprintf(cur->label, sizeof(cur->label), "%s", p + 6);
        } else if (strncmp(p, "FIELD1 ", 7) == 0) {
            snprintf(cur->field1, sizeof(cur->field1), "%s", p + 7);
        } else if (strncmp(p, "FIELD2 ", 7) == 0) {
            snprintf(cur->field2, sizeof(cur->field2), "%s", p + 7);
        } else if (strncmp(p, "PARAMS ", 7) == 0) {
            cur->n_params = 0;
            char *tok = p + 7, *comma;
            while (tok && *tok && cur->n_params < 4) {
                comma = strchr(tok, ',');
                size_t l = comma ? (size_t)(comma - tok) : strlen(tok);
                if (l >= sizeof(cur->param_names[0])) l = sizeof(cur->param_names[0]) - 1;
                memcpy(cur->param_names[cur->n_params], tok, l);
                cur->param_names[cur->n_params][l] = '\0';
                cur->n_params++;
                tok = comma ? comma + 1 : NULL;
            }
        } else if (strncmp(p, "SELECT2 ", 8) == 0) {
            cur->n_select2 = 0;
            char *tok = p + 8, *colon;
            while (tok && *tok && cur->n_select2 < 8) {
                colon = strchr(tok, ':');
                size_t l = colon ? (size_t)(colon - tok) : strlen(tok);
                if (l >= sizeof(cur->select2_options[0])) l = sizeof(cur->select2_options[0]) - 1;
                memcpy(cur->select2_options[cur->n_select2], tok, l);
                cur->select2_options[cur->n_select2][l] = '\0';
                cur->n_select2++;
                tok = colon ? colon + 1 : NULL;
            }
        } else if (strcmp(p, "END") == 0) {
            cur = NULL;
        }
        /* TEMPLATE lines are deliberately ignored here - only the
         * manager's compile_page() needs the template text; the
         * render side only needs enough to draw the picker and build
         * the params_line to send. */
    }
    fclose(f);
}

/* Task 7 (2026-08-26, direct instruction: "text description underneath
 * events of how much gold changes, what message is sent etc... when
 * user clicks that nav, they can change it") - a real, human-readable
 * one-line description for a command row, generated GENERICALLY from
 * the registry's own label/param_names + the command's real current
 * values (event_commands.registry.pdl already has everything needed -
 * per this house's own standing rule, never hand-write a per-command-
 * type description string in C, see EVENT-COMMAND-REGISTRY-
 * ARCHITECTURE.md). Params are stored as "key=val|key=val" - split on
 * '|' first, match each segment's key against def->param_names to
 * preserve the registry's own declared field order rather than
 * whatever order the params happened to be stored in. */
static EvhqCommandDef *evhq_find_cmd_def(const char *type) {
    for (int i = 0; i < g_evhq_n_cmd_defs; i++)
        if (strcmp(g_evhq_cmd_defs[i].type, type) == 0) return &g_evhq_cmd_defs[i];
    return NULL;
}
static void evhq_parse_params_line(const char *params_line, char keys[4][32], char vals[4][256], int *n) {
    *n = 0;
    char buf[512]; snprintf(buf, sizeof(buf), "%s", params_line ? params_line : "");
    char *seg = buf;
    while (seg && *n < 4) {
        char *bar = strchr(seg, '|');
        if (bar) *bar = '\0';
        char *eq = strchr(seg, '=');
        if (eq) {
            *eq = '\0';
            snprintf(keys[*n], sizeof(keys[0]), "%s", seg);
            snprintf(vals[*n], sizeof(vals[0]), "%s", eq + 1);
            (*n)++;
        }
        seg = bar ? bar + 1 : NULL;
    }
}
static void evhq_describe_command(const EvhqCmdNode *cmd, char *out, size_t outsz) {
    EvhqCommandDef *def = evhq_find_cmd_def(cmd->type);
    char keys[4][32], vals[4][256]; int n = 0;
    evhq_parse_params_line(cmd->params, keys, vals, &n);
    char body[400] = "";
    if (def) {
        for (int i = 0; i < def->n_params; i++) {
            const char *v = "";
            for (int j = 0; j < n; j++) if (strcmp(keys[j], def->param_names[i]) == 0) { v = vals[j]; break; }
            char seg[280]; snprintf(seg, sizeof(seg), "%s%s: %s", i > 0 ? ", " : "", def->param_names[i], (v && v[0]) ? v : "(empty)");
            strncat(body, seg, sizeof(body) - strlen(body) - 1);
        }
        snprintf(out, outsz, "%s (%s)", def->label, body);
    } else {
        snprintf(out, outsz, "%s %s", cmd->type, cmd->params);
    }
}
/* Task 7 (2026-08-26) - shared by events-hq (entity editing) and db-hq's
 * embedded common-event editor: arm the SAME Add-Command picker overlay,
 * but pre-filled with an EXISTING command's real current values and
 * jumped straight to its field view (not the type-picker list, since the
 * type is already known and fixed for an edit). g_evhq_edit_cmd_id being
 * >=0 is what evhq_submit_picker() below checks to send "edit:" instead
 * of "append:". */
static void evhq_open_edit_picker(int cmd_index) {
    if (cmd_index < 0 || cmd_index >= g_evhq_n_cmds) return;
    EvhqCmdNode *cmd = &g_evhq_cmds[cmd_index];
    EvhqCommandDef *def = evhq_find_cmd_def(cmd->type);
    if (!def) return; /* unknown/legacy type - nothing to edit against */
    int type_idx = -1;
    for (int i = 0; i < g_evhq_n_cmd_defs; i++) if (&g_evhq_cmd_defs[i] == def) { type_idx = i; break; }
    char keys[4][32], vals[4][256]; int n = 0;
    evhq_parse_params_line(cmd->params, keys, vals, &n);
    g_evhq_field1[0] = '\0'; g_evhq_field2[0] = '\0';
    for (int j = 0; j < n; j++) {
        if (def->n_params >= 1 && strcmp(keys[j], def->param_names[0]) == 0) snprintf(g_evhq_field1, sizeof(g_evhq_field1), "%s", vals[j]);
        if (def->n_params >= 2 && strcmp(keys[j], def->param_names[1]) == 0) snprintf(g_evhq_field2, sizeof(g_evhq_field2), "%s", vals[j]);
    }
    g_evhq_picker_open = 1;
    g_evhq_picker_type = type_idx;
    g_evhq_active_field = 0;
    g_evhq_edit_cmd_id = cmd->id;
}

/* REAL BUG FIX (2026-08-26, direct live report: "cancel doesn't work.
 * doesn't seem like any of the input does") - real definition of the
 * shared mouse-click handler forward-declared above dbhq_activate_elem().
 * A real click on any picker row (type option, field, or Cancel) used to
 * fall through both dbhq_activate_elem() and evhq_activate_elem() with
 * no matching onclick/id/tag branch at all and silently do nothing -
 * keyboard-driven interaction (already verified live via the real relay)
 * was unaffected, this only fixes the mouse path. Mirrors exactly what
 * evhq_handle_key()'s own g_evhq_picker_open branch already does for the
 * same actions from the keyboard, so behavior is consistent regardless
 * of input method. */
static void evhq_dispatch_picker_onclick(const char *onclick) {
    if (strncmp(onclick, "PICKER:FIELD:", 13) == 0) { g_evhq_active_field = atoi(onclick + 13); return; }
    if (strncmp(onclick, "PICKER:TYPE:", 12) == 0) { g_evhq_picker_type = atoi(onclick + 12); return; }
    if (strcmp(onclick, "PICKER:CANCEL") == 0) { g_evhq_picker_open = 0; g_evhq_edit_cmd_id = -1; return; }
    /* REAL, NEW 2026-08-29 - mouse-click parity for the new Delete row,
     * same real "onclick-first dispatch" reason this whole function
     * exists (see its own header comment) - keyboard path is in
     * evhq_handle_key()'s own matching PICKER:DELETE-shaped branch. */
    if (strcmp(onclick, "PICKER:DELETE") == 0) {
        if (g_evhq_edit_cmd_id >= 0) evhq_request_delete_node(g_evhq_edit_cmd_id);
        g_evhq_picker_open = 0; g_evhq_edit_cmd_id = -1;
        evhq_redraw_content();
        return;
    }
}

/* Trigger editing state (Task H7, 2026-08-25) - reuses the keystroke accumulation pattern */
static int g_evhq_trigger_edit_mode = 0;
static char g_evhq_trigger_buffer[64] = "";

static void evhq_apply_css(Elem *e) {
    css_compute_style(&g_sheet, e->tag, e->id[0] ? e->id : NULL, e->classes, e->n_classes, 0, &e->style);
}
static int evhq_measure_text_px(const CssStyle *st, const char *text) {
    char spec[128];
    const char *fam = st->has_font_family ? st->font_family : "DejaVu Sans";
    int size = st->has_font_size ? st->font_size : 11;
    snprintf(spec, sizeof(spec), "%s:pixelsize=%d%s", fam, size, (st->has_font_weight && st->font_weight_bold) ? ":bold" : "");
    static char cached_spec[128] = "";
    static XftFont *cached_font = NULL;
    XftFont *f;
    if (cached_font && strcmp(cached_spec, spec) == 0) f = cached_font;
    else {
        if (cached_font) XftFontClose(dpy, cached_font);
        f = XftFontOpenName(dpy, screen, spec);
        if (!f) f = XftFontOpenName(dpy, screen, "DejaVu Sans:pixelsize=11");
        cached_font = f;
        snprintf(cached_spec, sizeof(cached_spec), "%s", spec);
    }
    if (!f) return (int)strlen(text) * 7;
    XGlyphInfo ext;
    XftTextExtentsUtf8(dpy, f, (const FcChar8 *)text, (int)strlen(text), &ext);
    return ext.width;
}

/* Task 5 (2026-08-27) - evhq_render_tree()/hit_test() both recurse into
 * EVERY child regardless of the parent's own w/h (confirmed by reading
 * both directly) - zeroing only a panel's own w/h leaves its children
 * at their last real (nonzero) position, still drawn AND still
 * clickable underneath the stub panel. Recursively zero the whole
 * subtree instead so a hidden panel is genuinely inert, not just
 * invisible-looking at the top level. */
static void evhq_zero_subtree(Elem *e) {
    if (!e) return;
    e->w = 0; e->h = 0;
    for (int i = 0; i < e->n_children; i++) evhq_zero_subtree(e->children[i]);
}

/* REAL FIX 2026-08-29 (live report: "nav arrows are still driving both
 * sub menu and parent menu (bad)") - live-reproduced and confirmed:
 * the Add-Command picker's own rows are numbered 1..N (see
 * evhq_draw_picker_overlay()), the SAME low range the background
 * window's own tabbar/sidebar/panel elements use for THEIR nav_index -
 * draw_elem() draws a focus ring purely on `nav_index == g_focus_nav`
 * with no concept of "which modal this belongs to," so a background
 * element and a picker row with the same number both light up at
 * once. `*_assign_nav_indices()` re-running while the picker is
 * closed-then-reopened doesn't help - the STALE nav_index values from
 * before the picker opened are still baked into the background Elems'
 * own structs. Real fix, mirrors evhq_zero_subtree()'s own pattern
 * (same real technique already used to hide Scripting-mode content
 * behind Scratch mode) - recursively zero every background Elem's
 * nav_index while the picker owns g_focus_nav/g_nav[] exclusively, so
 * it can never coincidentally match a picker row's own number. */
static void zero_nav_subtree(Elem *e) {
    if (!e) return;
    e->nav_index = 0;
    for (int i = 0; i < e->n_children; i++) zero_nav_subtree(e->children[i]);
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
static void evhq_build_scratch_view(Elem *viewmode_stub, int content_x, int content_y, int content_h, int window_w) {
    if (g_evhq_selected_palette < 0) {
        g_evhq_selected_palette = 0;
        snprintf(g_evhq_selected_type, sizeof(g_evhq_selected_type), "%s", g_evhq_palette[0].type);
        snprintf(g_evhq_selected_params, sizeof(g_evhq_selected_params), "%s", g_evhq_palette[0].params);
    }
    evhq_zero_subtree(viewmode_stub);
    /* REAL FIX 2026-08-29 (Part B live report - palette overlapping
     * db-hq's own real, persistent sidebar): this used to hardcode
     * x=0, correct for events-hq (nothing real occupies that space in
     * its own window shape) but WRONG for Common Events, where x=0 is
     * underneath the real event-list sidebar. Real content_x param so
     * each caller passes its own real left edge. */
    viewmode_stub->x = content_x; viewmode_stub->y = content_y; viewmode_stub->w = window_w; viewmode_stub->h = content_h;
    int pslot = 0, bslot = 0;
    for (int i = 0; i < EVHQ_PALETTE_N && pslot < EVHQ_PALETTE_MAX; i++) {
        Elem *it = reusable_slot(g_evhq_palette_slots, EVHQ_PALETTE_MAX, pslot++, "block-item");
        if (!it) break;
        snprintf(it->classes[0], sizeof(it->classes[0]), "block-item");
        it->n_classes = 1;
        if (g_evhq_palette[i].cls1[0]) {
            snprintf(it->classes[it->n_classes], sizeof(it->classes[it->n_classes]), "%s", g_evhq_palette[i].cls1);
            it->n_classes++;
        }
        if (g_evhq_palette[i].cls2[0]) {
            snprintf(it->classes[it->n_classes], sizeof(it->classes[it->n_classes]), "%s", g_evhq_palette[i].cls2);
            it->n_classes++;
        }
        if (i == g_evhq_selected_palette) {
            snprintf(it->classes[it->n_classes], sizeof(it->classes[it->n_classes]), "selected");
            it->n_classes++;
        }
        snprintf(it->label, sizeof(it->label), "%s", g_evhq_palette[i].label);
        snprintf(it->onclick, sizeof(it->onclick), "BLOCK:SEL:%d", i);
        it->x = viewmode_stub->x + 8; it->y = viewmode_stub->y + 8 + i * 26;
        it->w = 196; it->h = 22;
        css_compute_style(&g_sheet, it->tag, NULL, it->classes, it->n_classes, 0, &it->style);
        viewmode_stub->children[viewmode_stub->n_children++] = it;
    }
    int bx = viewmode_stub->x + 220;
    for (int i = 0; i < g_evhq_n_blocks && bslot < MAX_CHILDREN; i++) {
        Elem *b = reusable_slot(g_evhq_block_slots, MAX_CHILDREN, bslot++, "text");
        if (!b) break;
        snprintf(b->classes[0], sizeof(b->classes[0]), "scratch-block"); b->n_classes = 1;
        const char *cls2 = evhq_palette_cls_for_type(g_evhq_blocks[i].key);
        if (cls2) { snprintf(b->classes[b->n_classes], sizeof(b->classes[b->n_classes]), "%s", cls2); b->n_classes++; }
        snprintf(b->label, sizeof(b->label), "%s  [%s]", g_evhq_blocks[i].key, g_evhq_blocks[i].status);
        b->x = bx; b->y = viewmode_stub->y + 8 + i * 26;
        b->w = window_w - bx - 12; b->h = 22;
        css_compute_style(&g_sheet, b->tag, NULL, b->classes, b->n_classes, 0, &b->style);
        viewmode_stub->children[viewmode_stub->n_children++] = b;
    }
    Elem *pl = reusable_slot(g_evhq_place_slots, 2, 0, "block-place");
    if (pl) {
        snprintf(pl->classes[0], sizeof(pl->classes[0]), "block-place");
        pl->n_classes = 1;
        if (g_evhq_palette[g_evhq_selected_palette].cls1[0]) {
            snprintf(pl->classes[pl->n_classes], sizeof(pl->classes[pl->n_classes]), "%s", g_evhq_palette[g_evhq_selected_palette].cls1);
            pl->n_classes++;
        }
        if (g_evhq_palette[g_evhq_selected_palette].cls2[0]) {
            snprintf(pl->classes[pl->n_classes], sizeof(pl->classes[pl->n_classes]), "%s", g_evhq_palette[g_evhq_selected_palette].cls2);
            pl->n_classes++;
        }
        snprintf(pl->label, sizeof(pl->label), "[].%d  new block", g_evhq_n_cmds + 1);
        snprintf(pl->onclick, sizeof(pl->onclick), "BLOCK:PLACE");
        pl->x = bx; pl->y = viewmode_stub->y + 8 + g_evhq_n_blocks * 26;
        pl->w = window_w - bx - 12; pl->h = 22;
        css_compute_style(&g_sheet, pl->tag, NULL, pl->classes, pl->n_classes, 0, &pl->style);
        viewmode_stub->children[viewmode_stub->n_children++] = pl;
    }
    Elem *cl = reusable_slot(g_evhq_place_slots, 2, 1, "block-clue");
    if (cl) {
        snprintf(cl->classes[0], sizeof(cl->classes[0]), "block-clue"); cl->n_classes = 1;
        snprintf(cl->label, sizeof(cl->label), "sel: %s  ::  %s", g_evhq_selected_type, g_evhq_selected_params);
        cl->x = viewmode_stub->x + 8; cl->y = viewmode_stub->y + 8 + EVHQ_PALETTE_N * 26 + 4;
        cl->w = 196; cl->h = 18;
        css_compute_style(&g_sheet, cl->tag, NULL, cl->classes, cl->n_classes, 0, &cl->style);
        viewmode_stub->children[viewmode_stub->n_children++] = cl;
    }
}

/* REAL FIX 2026-08-29 (Part B) - extracted from events-hq's own click
 * dispatch so Common Events (db-hq) can reuse the SAME real "click-
 * to-place Scratch" onclick handling, not a second copy. Returns 1 if
 * this was a real BLOCK: onclick (caller then does its own mode-
 * appropriate redraw/rebuild), 0 otherwise. */
static int evhq_handle_block_onclick(const char *onclick) {
    if (strncmp(onclick, "BLOCK:SEL:", 10) == 0) {
        int idx = atoi(onclick + 10);
        if (idx >= 0 && idx < EVHQ_PALETTE_N) {
            g_evhq_selected_palette = idx;
            snprintf(g_evhq_selected_type, sizeof(g_evhq_selected_type), "%s", g_evhq_palette[idx].type);
            snprintf(g_evhq_selected_params, sizeof(g_evhq_selected_params), "%s", g_evhq_palette[idx].params);
        }
        return 1;
    }
    if (strcmp(onclick, "BLOCK:PLACE") == 0) {
        if (g_evhq_selected_palette >= 0 && g_evhq_selected_type[0])
            evhq_request_append_node(g_evhq_selected_type, g_evhq_selected_params);
        return 1;
    }
    return 0;
}

static void evhq_layout_pass(Elem *window) {
    evhq_apply_css(window);
    window->x = 0; window->y = 0;
    window->w = 720; window->h = 480;
    g_evhq_close_w = 56; g_evhq_close_h = EVHQ_CHROME_H - 6;
    g_evhq_close_x = window->w - g_evhq_close_w - 4;
    g_evhq_close_y = 3;
    Elem *toolbar = find_by_id(window, "toolbar");
    Elem *pagetabs = find_by_id(window, "pagetabs");
    Elem *left = find_by_id(window, "left");
    Elem *right = find_by_id(window, "right");
    Elem *footer = find_by_id(window, "footer");
    int toolbar_h = 46, tabs_h = 26, footer_h = 34;
    int y = EVHQ_CHROME_H;
    if (toolbar) {
        evhq_apply_css(toolbar);
        toolbar->x = 0; toolbar->y = y; toolbar->w = window->w; toolbar->h = toolbar_h;
        g_evhq_toolbar_y = toolbar->y; g_evhq_toolbar_h = toolbar->h;
        for (int i = 0; i < toolbar->n_children; i++) {
            Elem *c = toolbar->children[i]; evhq_apply_css(c);
            /* Task 5 (2026-08-27) - toolbar now has 2 real children
             * (event-name, viewtabs), not 1 - the old "one child, full
             * width" layout would stack them on top of each other.
             * event-name stays left as before; viewtabs (real tag
             * "tabbar") gets laid out to the right of it, same tab-
             * measuring shape pagetabs already uses below. */
            if (strcmp(c->tag, "tabbar") == 0) {
                for (int j = 0; j < c->n_children; j++) {
                    Elem *tab = c->children[j]; evhq_apply_css(tab);
                    /* Direct live report (2026-08-27): "the highlight
                     * square for scripting selector is slightly not as
                     * big as some of the wording" - real cause: e->w
                     * only measured the plain label text, but
                     * evhq_draw_elem() ALSO draws a "[>]N." nav badge
                     * BEFORE the label (own 9px mono font, ~5 chars +
                     * 5px gap) that was never counted here, so the
                     * focus-ring border (sized to e->w) came out
                     * narrower than the actual visible content. +24 ->
                     * +34 to cover the badge+gap for these single-digit
                     * (1/2/3) viewtab indices. */
                    tab->w = evhq_measure_text_px(&tab->style, tab->label) + 34;
                }
                int total_w = 0;
                for (int j = 0; j < c->n_children; j++) total_w += c->children[j]->w + 4;
                c->x = window->w - 56 - total_w; c->y = toolbar->y + toolbar_h / 2 - 11; c->w = total_w; c->h = 22;
                int tx = c->x;
                for (int j = 0; j < c->n_children; j++) {
                    Elem *tab = c->children[j];
                    tab->x = tx; tab->y = c->y; tab->h = c->h;
                    tx += tab->w + 4;
                }
                continue;
            }
            c->x = 46; c->y = toolbar->y + toolbar_h / 2 - 9; c->w = window->w - 56 - 260; c->h = 18;
        }
        y += toolbar_h;
    }
    if (pagetabs) {
        evhq_apply_css(pagetabs);
        for (int i = 0; i < pagetabs->n_children; i++) {
            Elem *tab = pagetabs->children[i]; evhq_apply_css(tab);
            tab->w = evhq_measure_text_px(&tab->style, tab->label) + 30;
        }
        pagetabs->style.has_display = 1; pagetabs->style.display_flex = 1;
        pagetabs->style.has_flex_direction = 1; pagetabs->style.flex_row = 1;
        css_layout_pass(pagetabs, 0, y, window->w, tabs_h);
        for (int i = 0; i < pagetabs->n_children; i++) {
            Elem *tab = pagetabs->children[i];
            tab->x += 4 + i;
            tab->y = y + 2; tab->h = tabs_h - 4;
        }
        y += tabs_h;
    }
    int content_y = y, content_h = window->h - y - footer_h;
    int left_w = 220;
    /* Task 5 (2026-08-27) - Scratch/Blueprints view modes: zero-size
     * left/right/footer entirely (never drawn, never hit-testable -
     * draw_elem()/hit_test() both already skip w<=0||h<=0 Elems
     * elsewhere in this file) instead of touching their real content,
     * so Scripting's own behavior is provably unchanged when active. */
    if (left) {
        if (g_evhq_view_mode != 0) { evhq_zero_subtree(left); }
        else {
        evhq_apply_css(left);
        for (int i = 0; i < left->n_children; i++) {
            Elem *c = left->children[i]; evhq_apply_css(c);
            if (strcmp(c->tag, "title") == 0) { c->w = evhq_measure_text_px(&c->style, c->label) + 10; c->h = 14; continue; }
            c->style.has_height = 1; c->style.height = 18;
        }
        left->style.has_display = 1; left->style.display_flex = 1;
        left->style.has_flex_direction = 1; left->style.flex_row = 0;
        left->style.has_padding = 1; left->style.padding = 10;
        left->style.has_gap = 1; left->style.gap = 6;
        css_layout_pass(left, 4, content_y + 8, left_w, content_h - 12);
        }
    }
    if (right) {
        if (g_evhq_view_mode != 0) {
            evhq_zero_subtree(right);
            /* REAL, NEW 2026-08-28 - a stub view mode hides "right"
             * entirely (see evhq_zero_subtree() above); without this,
             * g_pal_has_grid would keep whatever it was left as by the
             * last real Scripting-mode pass, drawing a scroll track over
             * a stub view that has no scrollable content at all. */
            g_pal_has_grid = 0;
        }
        else {
        evhq_apply_css(right);
        for (int i = 0; i < right->n_children; i++) {
            Elem *c = right->children[i]; evhq_apply_css(c);
            if (strcmp(c->tag, "title") == 0) { c->w = evhq_measure_text_px(&c->style, c->label) + 10; c->h = 14; continue; }
            c->style.has_height = 1; c->style.height = 18;
        }
        right->style.has_display = 1; right->style.display_flex = 1;
        right->style.has_flex_direction = 1; right->style.flex_row = 0;
        right->style.has_padding = 1; right->style.padding = 12;
        right->style.has_gap = 1; right->style.gap = 4;
        css_layout_pass(right, left_w + 8, content_y + 8, window->w - left_w - 16, content_h - 12);
        /* REAL, GENERALIZED 2026-08-28 (Phase C target #3) - "right"'s
         * command rows had zero scroll support before this; a long
         * enough command list ran off the bottom of the panel with no
         * way to reach it. "cmd-row" is the second class every
         * evhq_inject_commands() row now carries (see that function's
         * own comment) - title/empty-msg are left alone. */
        generic_scroll_layout_pass(right, "cmd-row", content_y + 8, content_h - 12);
        }
    }
    Elem *viewmode_stub = find_by_id(window, "viewmode-stub");
    if (viewmode_stub) {
        if (g_evhq_view_mode == 0) { evhq_zero_subtree(viewmode_stub); }
        else if (g_evhq_view_mode == 1) {
            evhq_build_scratch_view(viewmode_stub, 0, content_y, content_h, window->w);
        }
        else {
            viewmode_stub->x = 0; viewmode_stub->y = content_y; viewmode_stub->w = window->w; viewmode_stub->h = content_h;
            for (int i = 0; i < viewmode_stub->n_children; i++) {
                Elem *c = viewmode_stub->children[i];
                snprintf(c->label, sizeof(c->label), "%s", EVHQ_VIEW_STUB_LABELS[g_evhq_view_mode]);
                c->x = viewmode_stub->x + 20; c->y = viewmode_stub->y + 20;
                c->w = window->w - 40; c->h = 20;
            }
        }
    }
    if (footer) {
        if (g_evhq_view_mode != 0) { evhq_zero_subtree(footer); }
        else {
        evhq_apply_css(footer);
        for (int i = 0; i < footer->n_children; i++) {
            Elem *c = footer->children[i]; evhq_apply_css(c);
            /* REAL FIX 2026-08-29 (live report: "the colors on the
             * buttons aren't completely covering the buttons... they
             * need to stretch to fit the text") - same real bug class
             * already found+fixed for view-tabs on 2026-08-27 (see that
             * fix's own comment above, toolbar section): width was
             * measured from the plain label alone, but draw_elem()
             * ALSO draws a real "[ ]N." nav badge INSIDE the same box,
             * before the label - +20 never accounted for that, so the
             * label text ran past the button's own background/border
             * on every footer button once real nav numbering reached
             * them. Same +34 constant that fix established, not a new
             * number. */
            c->w = evhq_measure_text_px(&c->style, c->label) + 34;
        }
        footer->style.has_display = 1; footer->style.display_flex = 1;
        footer->style.has_flex_direction = 1; footer->style.flex_row = 1;
        footer->style.has_gap = 1; footer->style.gap = 8;
        css_layout_pass(footer, 0, window->h - footer_h, window->w, footer_h);
        for (int i = 0; i < footer->n_children; i++) {
            Elem *c = footer->children[i];
            c->x += 10;
            c->y = footer->y + 6; c->h = footer_h - 12;
        }
        }
    }
}

static Elem g_evhq_cmd_slots[MAX_CHILDREN]; /* see reusable_slot()'s own header comment */

static void evhq_inject_commands(Elem *window) {
    Elem *right = find_by_id(window, "right");
    if (!right) return;
    evhq_load_command_registry(); /* Task 7 (2026-08-26) - see dbhq_ce_inject_panel()'s own comment on this same fix */
    Elem *title = NULL;
    for (int i = 0; i < right->n_children; i++) if (strcmp(right->children[i]->tag, "title") == 0) title = right->children[i];
    right->n_children = 0;
    if (title) right->children[right->n_children++] = title;
    int next_slot_index = 0;
    if (g_evhq_n_cmds == 0) {
        Elem *e = reusable_slot(g_evhq_cmd_slots, MAX_CHILDREN, next_slot_index++, "text");
        if (!e) return;
        snprintf(e->classes[0], sizeof(e->classes[0]), "empty-msg"); e->n_classes = 1;
        snprintf(e->label, sizeof(e->label), "(no commands yet)");
        right->children[right->n_children++] = e;
        return;
    }
    for (int i = 0; i < g_evhq_n_cmds && right->n_children < MAX_CHILDREN; i++) {
        /* Task 7 (2026-08-26, direct live report: "did u accidentally
         * remove the nav from scripted commands list?" - checked: this
         * was pre-existing, command rows were NEVER nav-reachable/
         * editable before this fix, in events-hq OR db-hq). Real button
         * tag (evhq_assign_nav_indices()'s own new "right" panel pass,
         * added alongside this) + id="cmd-edit-<id>" (evhq_activate_
         * elem()'s own new handler) + a generic, registry-driven
         * description (evhq_describe_command() - never hand-write a
         * per-command-type string here). */
        Elem *e = reusable_slot(g_evhq_cmd_slots, MAX_CHILDREN, next_slot_index++, "button");
        if (!e) break; /* pool exhausted - stop, don't crash */
        char cls[48]; snprintf(cls, sizeof(cls), "cmd-%s", g_evhq_cmds[i].type);
        snprintf(e->classes[0], sizeof(e->classes[0]), "%s", cls);
        /* REAL, NEW 2026-08-28 (Phase C, generic scroll wiring) - "right"
         * also holds a real "title" child (and an "empty-msg" text row
         * when g_evhq_n_cmds==0), so row_class=NULL would wrongly treat
         * those as scrollable rows too. Every cmd-<type> variant is
         * distinct (cmd-say/cmd-wait/...), so there is no existing SHARED
         * class across all of them for generic_scroll_layout_pass() to
         * filter on - this second class is added purely so that filter
         * has something real to match, same role "pal-grid-row"/
         * "bm-bookmark" already play for their own modes. */
        snprintf(e->classes[1], sizeof(e->classes[1]), "cmd-row");
        e->n_classes = 2;
        snprintf(e->id, sizeof(e->id), "cmd-edit-%d", g_evhq_cmds[i].id);
        char desc[300]; evhq_describe_command(&g_evhq_cmds[i], desc, sizeof(desc));
        snprintf(e->label, sizeof(e->label), "%d. %s", g_evhq_cmds[i].id, desc);
        right->children[right->n_children++] = e;
    }
}
static void evhq_refresh_page_data(Elem *window) {
    evhq_write_selected_page();
    evhq_load_page_state();
    Elem *tv = find_by_id(window, "trigger-value");
    if (tv) {
        if (g_evhq_trigger_edit_mode) {
            snprintf(tv->label, sizeof(tv->label), "%s_", g_evhq_trigger_buffer);
        } else {
            snprintf(tv->label, sizeof(tv->label), "%s", g_evhq_trigger);
        }
    }
    evhq_inject_commands(window);
    Elem *pagetabs = find_by_id(window, "pagetabs");
    if (pagetabs) {
        pagetabs->n_children = 0;
        for (int i = 0; i < g_evhq_n_pages && pagetabs->n_children < MAX_CHILDREN; i++) {
            Elem *t = elem_new("tab");
            snprintf(t->label, sizeof(t->label), "%s", g_evhq_pages[i]);
            t->active = (i == g_evhq_current_page);
            pagetabs->children[pagetabs->n_children++] = t;
        }
        /* Task H6 (2026-08-25) - "New Page" row for creating new pages */
        if (pagetabs->n_children < MAX_CHILDREN) {
            Elem *newpage = elem_new("tab");
            snprintf(newpage->label, sizeof(newpage->label), "+ New");
            newpage->id[0] = '\0'; snprintf(newpage->id, sizeof(newpage->id), "new-page-btn");
            newpage->active = 0;
            pagetabs->children[pagetabs->n_children++] = newpage;
        }
    }
    /* Task 5 (2026-08-27) - viewtabs are statically declared in
     * dashboard.chtpm (3 fixed tabs, real ids viewtab-0/1/2) - just
     * sync the active flag here, no elem_new() needed. */
    Elem *viewtabs = find_by_id(window, "viewtabs");
    if (viewtabs) for (int i = 0; i < viewtabs->n_children; i++) {
        viewtabs->children[i]->active = (i == g_evhq_view_mode);
    }
    Elem *en = find_by_id(window, "event-name");
    if (en) snprintf(en->label, sizeof(en->label), "%s", g_evhq_entity_label);
}
static void evhq_assign_nav_indices(Elem *window) {
    g_n_nav = 0;
    /* REAL FIX 2026-08-29 (live report: "nav arrows are still driving
     * both sub menu and parent menu (bad)... selecting 8 in the
     * subwindow will select 8 in parent window") - live-confirmed:
     * the picker's own rows are numbered 1..N, the SAME low range the
     * background window's own tabbar/sidebar/panel elements use, and
     * draw_elem() draws a focus ring purely on `nav_index ==
     * g_focus_nav` with no concept of which modal/window an element
     * belongs to - a background element and a picker row with the
     * same number both light up (and both become the real destination
     * of nav_index-driven digit-jump) at once. This function runs
     * BEFORE evhq_draw_picker_overlay() in the redraw sequence
     * (evhq_redraw_content()), which rebuilds g_n_nav/g_nav[] with the
     * picker's own real numbers - so zeroing every background
     * element's nav_index here and returning early, while the picker
     * is open, guarantees no background element can ever coincide
     * with whatever number the picker is currently using. */
    if (g_evhq_picker_open) { zero_nav_subtree(window); return; }
    /* Task 5 (2026-08-27) - viewtabs nav-reachable first (top of window,
     * always visible regardless of view mode). */
    Elem *viewtabs = find_by_id(window, "viewtabs");
    if (viewtabs) for (int i = 0; i < viewtabs->n_children && g_n_nav < MAX_ELEMS; i++) {
        viewtabs->children[i]->nav_index = ++g_n_nav; g_nav[g_n_nav - 1] = viewtabs->children[i];
    }
    Elem *pagetabs = find_by_id(window, "pagetabs");
    if (pagetabs) for (int i = 0; i < pagetabs->n_children && g_n_nav < MAX_ELEMS; i++) {
        pagetabs->children[i]->nav_index = ++g_n_nav; g_nav[g_n_nav - 1] = pagetabs->children[i];
    }
    /* REAL FIX 2026-08-29 (live report: "in the 'scratch' visual
     * scripting setup, all blocks are supposed to be nav numbered")
     * - evhq_build_scratch_view()'s real, clickable Elems (the palette
     * items, onclick "BLOCK:SEL:<i>", and the "[].<#> new block"
     * place-slot, onclick "BLOCK:PLACE") were never walked here at
     * all - Scratch mode had zero nav coverage of its own real
     * interactive content, same class of gap Task 7 already fixed for
     * events-hq's own "right" command rows. Gate on onclick[0] rather
     * than tag (viewmode_stub mixes "block-item"/"text"/"block-place"/
     * "block-clue" tags; only the first and third are real actions -
     * the placed-block "text" rows and the "sel: ..." clue label have
     * no onclick and correctly stay non-nav, same as any other
     * inert-text Elem elsewhere in this file). */
    if (g_evhq_view_mode == 1) {
        Elem *stub = find_by_id(window, "viewmode-stub");
        if (stub) for (int i = 0; i < stub->n_children && g_n_nav < MAX_ELEMS; i++) {
            Elem *c = stub->children[i];
            if (!c->onclick[0]) continue;
            c->nav_index = ++g_n_nav; g_nav[g_n_nav - 1] = c;
        }
    }
    /* Task 5 (2026-08-27) - everything below here is Scripting-mode-only
     * content (trigger/commands/footer) - skip granting nav when a stub
     * view is showing instead, matching evhq_layout_pass()'s own
     * evhq_zero_subtree() hiding of the exact same Elems, so nav can
     * never reach something invisible. */
    if (g_evhq_view_mode == 0) {
    /* Task H7 (2026-08-25) - trigger-value nav-reachable for editing */
    Elem *trigger_val = find_by_id(window, "trigger-value");
    if (trigger_val && g_n_nav < MAX_ELEMS) {
        trigger_val->nav_index = ++g_n_nav; g_nav[g_n_nav - 1] = trigger_val;
    }
    /* Task 7 (2026-08-26) - command rows (evhq_inject_commands()'s own
     * "right" panel, now real `button`-tagged Elems) were never walked
     * here at all before this fix - confirmed via direct live report,
     * the real root cause of "commands aren't nav-reachable/editable". */
    /* REAL, NEW 2026-08-28 (Phase C target #3) - scroll arrows numbered
     * BEFORE the rows they control, same order dbhq_assign_nav_indices()
     * already uses for palettes/db-hq/bookmarks. A disabled arrow's
     * onclick[0]=='\0' (cleared in generic_scroll_layout_pass()) excludes
     * it here automatically. */
    if (g_pal_has_grid) {
        if (g_pal_arrow_up->onclick[0] && g_n_nav < MAX_ELEMS) {
            g_pal_arrow_up->nav_index = ++g_n_nav; g_nav[g_n_nav - 1] = g_pal_arrow_up;
        }
        if (g_pal_arrow_down->onclick[0] && g_n_nav < MAX_ELEMS) {
            g_pal_arrow_down->nav_index = ++g_n_nav; g_nav[g_n_nav - 1] = g_pal_arrow_down;
        }
    }
    Elem *right = find_by_id(window, "right");
    if (right) for (int i = 0; i < right->n_children && g_n_nav < MAX_ELEMS; i++) {
        Elem *c = right->children[i];
        if (strcmp(c->tag, "button") != 0) continue;
        c->nav_index = ++g_n_nav; g_nav[g_n_nav - 1] = c;
    }
    Elem *footer = find_by_id(window, "footer");
    if (footer) for (int i = 0; i < footer->n_children && g_n_nav < MAX_ELEMS; i++) {
        footer->children[i]->nav_index = ++g_n_nav; g_nav[g_n_nav - 1] = footer->children[i];
    }
    }
    if (g_n_nav < MAX_ELEMS) { g_evhq_close_elem->nav_index = ++g_n_nav; g_nav[g_n_nav - 1] = g_evhq_close_elem; }
    if (g_focus_nav < 1) g_focus_nav = 1;
    if (g_focus_nav > g_n_nav) g_focus_nav = g_n_nav > 0 ? g_n_nav : 1;
}

static unsigned long evhq_alloc_pixel(const char *spec) {
    if (!spec || !spec[0]) return BlackPixel(dpy, screen);
    XColor c;
    if (spec[0] == '#') { if (XParseColor(dpy, cmap, spec, &c) && XAllocColor(dpy, cmap, &c)) return c.pixel; }
    else if (XAllocNamedColor(dpy, cmap, spec, &c, &c)) return c.pixel;
    return BlackPixel(dpy, screen);
}
static XftColor evhq_xft_color(const char *spec) {
    XftColor xc; XRenderColor rc = {0, 0, 0, 0xffff};
    if (spec && spec[0] == '#' && strlen(spec) >= 7) {
        unsigned int r, g, b; sscanf(spec + 1, "%02x%02x%02x", &r, &g, &b);
        rc.red = (unsigned short)(r * 257); rc.green = (unsigned short)(g * 257); rc.blue = (unsigned short)(b * 257);
    }
    XftColorAllocValue(dpy, DefaultVisual(dpy, screen), cmap, &rc, &xc); return xc;
}
static XftFont *evhq_font_for(const CssStyle *st) {
    char spec[128];
    const char *fam = st->has_font_family ? st->font_family : "DejaVu Sans";
    int size = st->has_font_size ? st->font_size : 11;
    snprintf(spec, sizeof(spec), "%s:pixelsize=%d%s", fam, size, (st->has_font_weight && st->font_weight_bold) ? ":bold" : "");
    static char cached_spec[128] = "";
    static XftFont *cached_font = NULL;
    if (cached_font && strcmp(cached_spec, spec) == 0) return cached_font;
    if (cached_font) XftFontClose(dpy, cached_font);
    XftFont *f = XftFontOpenName(dpy, screen, spec);
    if (!f) f = XftFontOpenName(dpy, screen, "DejaVu Sans:pixelsize=11");
    cached_font = f;
    snprintf(cached_spec, sizeof(cached_spec), "%s", spec);
    return f;
}
/* REAL FIX 2026-08-29 (EVENTS-HQ-RENDER-UNIFICATION-PLAN.md Part A) -
 * evhq_draw_elem()/evhq_render_tree() were a real, hand-copied twin of
 * the shared draw_elem()/render_tree() (khtpm_draw_core.c), drifted
 * since the original binary-merge - missing sprite support, missing
 * the badge-font cache (the exact perf bug already fixed in the
 * shared version, still live here), missing elem_cursor_prefix()/
 * ACTIVATE-scope support (tonight's Gap 5), missing border-width/
 * padding-aware layout, missing item-active highlight, missing
 * contrast-aware badge color, missing badge_align_left. All real
 * call sites (the tree walk + every direct draw_elem-style call for
 * chrome/overlay/scrollbar Elems) now call the shared draw_elem()/
 * render_tree() directly - see this plan doc for the full real diff
 * that justified this, not a guess. */
static void evhq_draw_entity_glyph(void) {
    if (!g_evhq_sprite_pixels || g_evhq_sprite_res <= 0) return;
    int size = 36;
    int ox = 6, oy = g_evhq_toolbar_y + (g_evhq_toolbar_h - size) / 2;
    int bg_r = 0x2f, bg_g = 0x2f, bg_b = 0x2f;
    for (int y = 0; y < size; y++) {
        int sy = y * g_evhq_sprite_res / size;
        for (int x = 0; x < size; x++) {
            int sx = x * g_evhq_sprite_res / size;
            const unsigned char *px = &g_evhq_sprite_pixels[(sy * g_evhq_sprite_res + sx) * 4];
            int a = px[3];
            if (a == 0) continue;
            int r = (px[0] * a + bg_r * (255 - a)) / 255;
            int g = (px[1] * a + bg_g * (255 - a)) / 255;
            int b = (px[2] * a + bg_b * (255 - a)) / 255;
            char spec[8]; snprintf(spec, sizeof(spec), "#%02x%02x%02x", r, g, b);
            XSetForeground(dpy, gc, evhq_alloc_pixel(spec));
            XDrawPoint(dpy, buf, gc, ox + x, oy + y);
        }
    }
}
static void evhq_draw_chrome_bar(void) {
    XSetForeground(dpy, gc, evhq_alloc_pixel("#1c1c1c"));
    XFillRectangle(dpy, buf, gc, 0, 0, g_window->w, EVHQ_CHROME_H);
    char tspec[48]; snprintf(tspec, sizeof(tspec), "DejaVu Sans:pixelsize=10:bold");
    XftFont *titlefont = XftFontOpenName(dpy, screen, tspec);
    if (titlefont) {
        XftColor titlecol = evhq_xft_color("#eeeeee");
        char title[48]; snprintf(title, sizeof(title), "events-hq %s", g_evhq_has_real_focus ? "^" : " ");
        int ty = (EVHQ_CHROME_H + titlefont->ascent - titlefont->descent) / 2;
        XftDrawStringUtf8(xftdraw_buf, &titlecol, titlefont, 8, ty, (const FcChar8 *)title, (int)strlen(title));
        XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &titlecol);
        XftFontClose(dpy, titlefont);
    }
    g_evhq_close_elem->x = g_evhq_close_x; g_evhq_close_elem->y = g_evhq_close_y;
    g_evhq_close_elem->w = g_evhq_close_w; g_evhq_close_elem->h = g_evhq_close_h;
    snprintf(g_evhq_close_elem->label, sizeof(g_evhq_close_elem->label), "x");
    css_style_init(&g_evhq_close_elem->style);
    g_evhq_close_elem->style.has_border_color = 1;
    snprintf(g_evhq_close_elem->style.border_color, sizeof(g_evhq_close_elem->style.border_color), "%s", g_evhq_close_elem->nav_index == g_focus_nav ? "#ff8c00" : "#888888");
    g_evhq_close_elem->style.has_fg_color = 1;
    snprintf(g_evhq_close_elem->style.fg_color, sizeof(g_evhq_close_elem->style.fg_color), "#eeeeee");
    draw_elem(g_evhq_close_elem, 0);
}
/* Task 7 follow-up (2026-08-26, direct live re-test: "still dont see
 * nav on the subs (show choices, change gold? etc)... should be
 * driving by layouts"). Real fix: the picker's rows are now real Elems
 * with a real nav_index, drawn via the SAME generic evhq_draw_elem()
 * every other button in this file already uses - which already knows
 * how to draw a "[>N]" badge + orange focus outline for any Elem with
 * nav_index>0 (see evhq_draw_elem()'s own nav_index handling, ~line
 * 2727-2748), for free, no new drawing code needed. This REPLACES raw
 * XftDrawStringUtf8 line-by-line drawing for the interactive rows only
 * (the header/hint text stay plain drawn text - not interactive,
 * nothing to navigate to). Deliberately does NOT touch the existing,
 * proven-working key-handling logic in evhq_handle_key()'s own
 * g_evhq_picker_open branch (Enter/Backspace/typed-char field editing,
 * digit-jump in the type list) - only g_focus_nav is kept in sync with
 * whichever field/type-option that existing logic already considers
 * "active" (g_evhq_active_field / g_evhq_picker_focus), purely so the
 * SAME visual nav language (numbered brackets, orange outline) used
 * everywhere else in this house also appears here, and so an
 * agent driving this via db_hq_history.txt/events_hq_history.txt can
 * read real nav_index/g_focus_nav state from the debug dump exactly
 * like it already can for every other window in this binary. */
/* Picker layout: parsed once from picker.chtpm, positions cached for
 * the drawing function. Follows the fo-menu-sys.md pattern: chtpm
 * defines the structural frame (panel + row slots + cancel), C code
 * fills in dynamic content from the in-memory registry. */
typedef struct {
    int px, py, pw, ph;
    int row_x, row_w, row_h, row_spacing;
    int cancel_nav_index;
} PickerLayout;
static PickerLayout g_picker_layout;
static int g_picker_layout_loaded = 0;
static void picker_chtpm_load(void) {
    if (g_picker_layout_loaded) return;
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/&.widgits/events-hq/pieces/picker.chtpm", g_house_root);
    Elem *root = parse_chtpm(path);
    if (root) {
        g_picker_layout.pw = root->w > 0 ? root->w : 360;
        g_picker_layout.ph = root->h > 0 ? root->h : 280;
    } else {
        g_picker_layout.pw = 360; g_picker_layout.ph = 280;
    }
    g_picker_layout.px = (g_window->w - g_picker_layout.pw) / 2;
    g_picker_layout.py = (g_window->h - g_picker_layout.ph) / 2;
    g_picker_layout.row_x = g_picker_layout.px + 16;
    g_picker_layout.row_w = g_picker_layout.pw - 32;
    g_picker_layout.row_h = 20;
    g_picker_layout.row_spacing = 22;
    g_picker_layout_loaded = 1;
}
static Elem g_picker_slots[16];
/* REAL, NEW 2026-08-29 (direct instruction: "no hand drawn c tho. we
 * need to fix that... it should be a new created chtpm element or
 * something, like how +- works") - the overlay's own chrome (panel
 * background/border, title, hint text) used to be painted with raw
 * XSetForeground/XFillRectangle/XDrawRectangle/XftDrawStringUtf8 calls
 * carrying hardcoded hex colors, instead of real Elems drawn through
 * the shared draw_elem() + real CSS classes (picker.chtpm already
 * declared "picker-overlay" as this panel's real class - it was just
 * never matched against any actual CSS rule before now, see
 * dashboard.css's own new rules). Same real class of fix as the row/
 * Cancel/Delete/field Elems just below, which already used draw_elem()
 * but still set colors via inline snprintf(style.fg_color,...) instead
 * of css_compute_style() - all of that inline styling is gone now too. */
static Elem g_picker_chrome_slots[2]; /* 0=overlay panel, 1=title/hint (reused per screen, never both drawn at once) */
static void evhq_draw_picker_overlay(void) {
    picker_chtpm_load();
    PickerLayout *L = &g_picker_layout;
    Elem *overlay = reusable_slot(g_picker_chrome_slots, 2, 0, "panel");
    if (overlay) {
        snprintf(overlay->classes[0], sizeof(overlay->classes[0]), "picker-overlay"); overlay->n_classes = 1;
        overlay->x = L->px; overlay->y = L->py; overlay->w = L->pw; overlay->h = L->ph;
        css_compute_style(&g_sheet, overlay->tag, NULL, overlay->classes, overlay->n_classes, 0, &overlay->style);
        draw_elem(overlay, 0);
    }
    int ty = L->py + 20;
    evhq_load_command_registry();
    g_n_nav = 0;
    if (g_evhq_picker_type < 0) {
        g_focus_nav = g_evhq_picker_focus;
        Elem *title = reusable_slot(g_picker_chrome_slots, 2, 1, "title");
        if (title) {
            snprintf(title->classes[0], sizeof(title->classes[0]), "picker-title"); title->n_classes = 1;
            snprintf(title->label, sizeof(title->label), "Add Command");
            title->x = L->row_x; title->y = ty - 12; title->w = L->row_w; title->h = 16;
            css_compute_style(&g_sheet, title->tag, NULL, title->classes, title->n_classes, 0, &title->style);
            draw_elem(title, 0);
        }
        ty += 26;
        /* Real visible-row budget for THIS frame - box bottom minus the
         * hint line minus one row reserved for Cancel, divided by row
         * pitch. Recomputed every draw so a resize/different picker.
         * chtpm geometry is never stale. */
        int content_bottom = L->py + L->ph - 14 - L->row_spacing;
        g_evhq_picker_visible_rows = (content_bottom - ty) / L->row_spacing;
        if (g_evhq_picker_visible_rows < 1) g_evhq_picker_visible_rows = 1;
        if (g_evhq_picker_visible_rows > 14) g_evhq_picker_visible_rows = 14; /* g_picker_slots pool safety margin, slot 15 reserved for Cancel */
        int max_scroll = g_evhq_n_cmd_defs - g_evhq_picker_visible_rows;
        if (max_scroll < 0) max_scroll = 0;
        if (g_evhq_picker_scroll > max_scroll) g_evhq_picker_scroll = max_scroll;
        if (g_evhq_picker_scroll < 0) g_evhq_picker_scroll = 0;
        int shown = 0;
        for (int i = 0; i < g_evhq_picker_visible_rows; i++) {
            int cmd_idx = g_evhq_picker_scroll + i;
            if (cmd_idx >= g_evhq_n_cmd_defs) break;
            Elem *row = reusable_slot(g_picker_slots, 16, i, "button");
            if (!row) break;
            snprintf(row->label, sizeof(row->label), "%s", g_evhq_cmd_defs[cmd_idx].label);
            row->x = L->row_x; row->y = ty - 15; row->w = L->row_w; row->h = L->row_h;
            snprintf(row->classes[0], sizeof(row->classes[0]), "picker-row"); row->n_classes = 1;
            css_compute_style(&g_sheet, row->tag, NULL, row->classes, row->n_classes, 0, &row->style);
            row->nav_index = i + 1;
            snprintf(row->onclick, sizeof(row->onclick), "PICKER:TYPE:%d", cmd_idx);
            g_nav[g_n_nav++] = row;
            draw_elem(row, 0);
            ty += L->row_spacing;
            shown++;
        }
        {
            Elem *cancel = reusable_slot(g_picker_slots, 16, 15, "button");
            if (cancel) {
                snprintf(cancel->label, sizeof(cancel->label), "Cancel");
                cancel->x = L->row_x; cancel->y = ty - 15; cancel->w = L->row_w; cancel->h = L->row_h;
                snprintf(cancel->classes[0], sizeof(cancel->classes[0]), "picker-cancel"); cancel->n_classes = 1;
                css_compute_style(&g_sheet, cancel->tag, NULL, cancel->classes, cancel->n_classes, 0, &cancel->style);
                snprintf(cancel->onclick, sizeof(cancel->onclick), "PICKER:CANCEL");
                cancel->nav_index = shown + 1;
                g_nav[g_n_nav++] = cancel;
                draw_elem(cancel, 0);
                ty += L->row_spacing;
            }
        }
        Elem *hint = reusable_slot(g_picker_chrome_slots, 2, 1, "text");
        if (hint) {
            snprintf(hint->classes[0], sizeof(hint->classes[0]), "picker-hint"); hint->n_classes = 1;
            snprintf(hint->label, sizeof(hint->label), "%s",
                (max_scroll > 0)
                    ? "Digits/arrows + Enter select, PageUp/PageDown scroll, Escape cancels"
                    : "Digits/arrows + Enter select, Escape cancels");
            hint->x = L->row_x; hint->y = L->py + L->ph - 14 - 11; hint->w = L->row_w; hint->h = 14;
            css_compute_style(&g_sheet, hint->tag, NULL, hint->classes, hint->n_classes, 0, &hint->style);
            draw_elem(hint, 0);
        }
    } else if (g_evhq_picker_type < g_evhq_n_cmd_defs) {
        EvhqCommandDef *def = &g_evhq_cmd_defs[g_evhq_picker_type];
        Elem *title = reusable_slot(g_picker_chrome_slots, 2, 1, "title");
        if (title) {
            snprintf(title->classes[0], sizeof(title->classes[0]), "picker-title"); title->n_classes = 1;
            snprintf(title->label, sizeof(title->label), "%s", def->label);
            title->x = L->row_x; title->y = ty - 12; title->w = L->row_w; title->h = 16;
            css_compute_style(&g_sheet, title->tag, NULL, title->classes, title->n_classes, 0, &title->style);
            draw_elem(title, 0);
        }
        ty += 30;
        g_focus_nav = g_evhq_active_field + 1;
        int has_field2 = (def->n_params > 1 && strcmp(def->field2, "-") != 0);
        Elem *f1 = reusable_slot(g_picker_slots, 16, 0, "button");
        if (f1) {
            snprintf(f1->label, sizeof(f1->label), "%s %s%s", def->field1, g_evhq_field1, g_evhq_active_field == 0 ? "_" : "");
            f1->x = L->row_x; f1->y = ty - 15; f1->w = L->row_w; f1->h = L->row_h;
            snprintf(f1->classes[0], sizeof(f1->classes[0]), "picker-row"); f1->n_classes = 1;
            css_compute_style(&g_sheet, f1->tag, NULL, f1->classes, f1->n_classes, 0, &f1->style);
            f1->nav_index = 1;
            snprintf(f1->onclick, sizeof(f1->onclick), "PICKER:FIELD:0");
            g_nav[g_n_nav++] = f1;
            draw_elem(f1, 0);
            ty += L->row_spacing + 2;
        }
        if (has_field2) {
            Elem *f2 = reusable_slot(g_picker_slots, 16, 1, "button");
            if (f2) {
                if (def->n_select2 > 0 && g_evhq_active_field == 1)
                    snprintf(f2->label, sizeof(f2->label), "%s [%s] < >", def->field2, g_evhq_field2);
                else if (def->n_select2 > 0)
                    snprintf(f2->label, sizeof(f2->label), "%s %s", def->field2, g_evhq_field2);
                else
                    snprintf(f2->label, sizeof(f2->label), "%s %s%s", def->field2, g_evhq_field2, g_evhq_active_field == 1 ? "_" : "");
                f2->x = L->row_x; f2->y = ty - 15; f2->w = L->row_w; f2->h = L->row_h;
                snprintf(f2->classes[0], sizeof(f2->classes[0]), "picker-row"); f2->n_classes = 1;
                css_compute_style(&g_sheet, f2->tag, NULL, f2->classes, f2->n_classes, 0, &f2->style);
                f2->nav_index = 2;
                snprintf(f2->onclick, sizeof(f2->onclick), "PICKER:FIELD:1");
                g_nav[g_n_nav++] = f2;
                draw_elem(f2, 0);
                ty += L->row_spacing + 2;
            }
        }
        {
            Elem *cancel = reusable_slot(g_picker_slots, 16, 15, "button");
            if (cancel) {
                snprintf(cancel->label, sizeof(cancel->label), "Cancel");
                cancel->x = L->row_x; cancel->y = ty - 15; cancel->w = L->row_w; cancel->h = L->row_h;
                snprintf(cancel->classes[0], sizeof(cancel->classes[0]), "picker-cancel"); cancel->n_classes = 1;
                css_compute_style(&g_sheet, cancel->tag, NULL, cancel->classes, cancel->n_classes, 0, &cancel->style);
                snprintf(cancel->onclick, sizeof(cancel->onclick), "PICKER:CANCEL");
                cancel->nav_index = def->n_params + 1;
                g_nav[g_n_nav++] = cancel;
                draw_elem(cancel, 0);
                ty += L->row_spacing + 2;
            }
        }
        /* REAL, NEW 2026-08-29 (see evhq_handle_key()'s own matching
         * comment on this same feature) - a real "Delete" row, only
         * when editing an existing command (g_evhq_edit_cmd_id >= 0),
         * right after Cancel. */
        if (g_evhq_edit_cmd_id >= 0) {
            Elem *del = reusable_slot(g_picker_slots, 16, 14, "button");
            if (del) {
                snprintf(del->label, sizeof(del->label), "Delete");
                del->x = L->row_x; del->y = ty - 15; del->w = L->row_w; del->h = L->row_h;
                snprintf(del->classes[0], sizeof(del->classes[0]), "picker-delete"); del->n_classes = 1;
                css_compute_style(&g_sheet, del->tag, NULL, del->classes, del->n_classes, 0, &del->style);
                snprintf(del->onclick, sizeof(del->onclick), "PICKER:DELETE");
                del->nav_index = def->n_params + 2;
                g_nav[g_n_nav++] = del;
                draw_elem(del, 0);
                ty += L->row_spacing + 2;
            }
        }
        Elem *hint2 = reusable_slot(g_picker_chrome_slots, 2, 0, "text");
        /* REAL: slot 0 is normally the overlay panel, but the panel has
         * already been drawn for this frame by the time we get here -
         * safe, deliberate reuse, same "one slot pool, sequenced by
         * draw order within a single frame" pattern reusable_slot()'s
         * own header comment documents. */
        if (hint2) {
            snprintf(hint2->classes[0], sizeof(hint2->classes[0]), "picker-hint"); hint2->n_classes = 1;
            snprintf(hint2->label, sizeof(hint2->label), "%s",
                def->n_select2 > 0 ? "Enter: next/submit  ←→: select  Esc: cancel"
                                    : "Enter: next/submit  Escape: cancel");
            hint2->x = L->row_x; hint2->y = L->py + L->ph - 14 - 11; hint2->w = L->row_w; hint2->h = 14;
            css_compute_style(&g_sheet, hint2->tag, NULL, hint2->classes, hint2->n_classes, 0, &hint2->style);
            draw_elem(hint2, 0);
        }
    }
}
/* REAL, requested "once and for all" fix (2026-08-27, direct
 * instruction: "is there a way view can send a signal when it has
 * changed via frame history and is ready to be dumped... we need 2 fix
 * this once and for all") - same real, already-proven convention
 * chai_append_frame_history() uses for chat-hai (2026-08-15, "you
 * should check it with injection and framehistory.txt (we dont need a
 * png dump to see if frames are updating)"), ported to events-hq/db-hq
 * which never had it: one line appended to a real frame-history file
 * EVERY completed redraw, with a monotonic seq number. A harness should
 * now: read the file's last seq, send its input, then POLL this file
 * until seq increases (real signal, not a sleep guess) before sending
 * the PNG-dump relay code - eliminates the whole class of "is the frame
 * actually ready yet" bug this session hit (which turned out to be a
 * separate real draw-guard bug, see evhq_draw_elem()'s own w<=0/h<=0
 * fix above, but this signal is real, general prevention against the
 * NEXT such bug looking the same from a harness's point of view). */
static long g_evhq_frame_seq = 0;
static void evhq_append_frame_history(void) {
    g_evhq_frame_seq++;
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/events_hq_frame_history.txt", g_house_root);
    FILE *f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "seq=%ld focus_nav=%d/%d view_mode=%d page=%d n_cmds=%d entity=%s\n",
            g_evhq_frame_seq, g_focus_nav, g_n_nav, g_evhq_view_mode, g_evhq_current_page,
            g_evhq_n_cmds, g_evhq_entity_label);
    fclose(f);
    /* REAL, NEW (2026-08-27, HARNESS-AUTHORING-GUIDE.md §3a) - a real
     * PAL/prisc+x script can already inject relay codes (SYS_OPEN
     * append + SYS_WRITE_LINE) but SYS_GET_KV_INT only matches a key at
     * the very START of a line, so it cannot read the multi-key line
     * above. Small, cheap, zero-VM-change fix: also write single-key
     * flat files a PAL script CAN poll today via SYS_GET_KV_INT, one
     * real file per field worth polling. First real consumer: the
     * proof-of-concept PAL harness for Task 5's view-tab switch. */
    char vmpath[PATH_BUF];
    snprintf(vmpath, sizeof(vmpath), "%s/#.desktop/events_hq_view_mode.txt", g_house_root);
    FILE *vf = fopen(vmpath, "w");
    if (vf) { fprintf(vf, "view_mode=%d\n", g_evhq_view_mode); fclose(vf); }
    char seqpath[PATH_BUF];
    snprintf(seqpath, sizeof(seqpath), "%s/#.desktop/events_hq_seq.txt", g_house_root);
    FILE *sf = fopen(seqpath, "w");
    if (sf) { fprintf(sf, "seq=%ld\n", g_evhq_frame_seq); fclose(sf); }
}
static void evhq_redraw_content(void) {
    evhq_layout_pass(g_window);
    evhq_assign_nav_indices(g_window);
    XSetForeground(dpy, gc, evhq_alloc_pixel("#252525"));
    /* REAL FIX 2026-08-28 (live corruption found testing Phase 2's
     * frame-file paint) - clearing only g_window->w/h leaves stale
     * pixels visible whenever content SHRINKS between redraws (a
     * taller previous session's leftover rows) - the backing Pixmap
     * only ever GROWS (see g_buf_w/g_buf_h's own header comment),
     * it never shrinks back down, so clearing less than the real
     * allocated buffer leaves old content sitting below the new,
     * smaller content. Clear the FULL allocated buffer every time. */
    XFillRectangle(dpy, buf, gc, 0, 0, (unsigned)(g_buf_w > g_window->w ? g_buf_w : g_window->w), (unsigned)(g_buf_h > g_window->h ? g_buf_h : g_window->h));
    /* REAL FIX 2026-08-29 (Part A) - was evhq_render_tree(g_window), a
     * hand-copied twin missing tonight's Gap 5/badge-cache/sprite
     * fixes (see this function's own draw_elem() replacement comment
     * above). The shared render_tree() also draws the ROOT element
     * itself (depth==0), which the old evhq_render_tree() never did -
     * live-verified harmless: g_window's own root Elem carries no
     * real bg/border style here, so this is a no-op paint, not a new
     * visible layer. */
    render_tree(g_window, 0);
    /* REAL, NEW 2026-08-28 (Phase C target #3) - events-hq has its OWN
     * redraw path (evhq_render_tree()/evhq_draw_elem()), entirely
     * separate from db-hq's dbhq_redraw_content() - the scroll track/
     * thumb/arrow drawing dbhq_redraw_content() already does for
     * g_pal_has_grid is NEVER reached from here, so it's replicated here
     * (same geometry fields generic_scroll_layout_pass() already
     * computed, same visual shape) rather than assumed shared. */
    if (g_pal_has_grid && g_pal_track_h > 0) {
        XSetForeground(dpy, gc, evhq_alloc_pixel("#2a2a2a"));
        XFillRectangle(dpy, buf, gc, g_pal_track_x, g_pal_track_y, (unsigned)g_pal_track_w, (unsigned)g_pal_track_h);
        XSetForeground(dpy, gc, evhq_alloc_pixel("#888888"));
        XFillRectangle(dpy, buf, gc, g_pal_track_x + 1, g_pal_thumb_y,
                       (unsigned)(g_pal_track_w - 2), (unsigned)g_pal_thumb_h);
        int ax = g_pal_track_x, aw = g_pal_track_w;
        int up_y0 = g_pal_track_y - g_pal_arrow_h;
        int down_y0 = g_pal_track_y + g_pal_track_h;
        int up_enabled = !g_pal_arrow_up_disabled;
        int down_enabled = !g_pal_arrow_down_disabled;
        XSetForeground(dpy, gc, evhq_alloc_pixel("#3a3a3a"));
        XFillRectangle(dpy, buf, gc, ax, up_y0, (unsigned)aw, (unsigned)g_pal_arrow_h);
        XFillRectangle(dpy, buf, gc, ax, down_y0, (unsigned)aw, (unsigned)g_pal_arrow_h);
        XSetForeground(dpy, gc, evhq_alloc_pixel(up_enabled ? "#cccccc" : "#555555"));
        XPoint up_tri[3] = {
            { (short)(ax + aw / 2), (short)(up_y0 + 3) },
            { (short)(ax + 2), (short)(up_y0 + g_pal_arrow_h - 3) },
            { (short)(ax + aw - 2), (short)(up_y0 + g_pal_arrow_h - 3) },
        };
        XFillPolygon(dpy, buf, gc, up_tri, 3, Convex, CoordModeOrigin);
        XSetForeground(dpy, gc, evhq_alloc_pixel(down_enabled ? "#cccccc" : "#555555"));
        XPoint down_tri[3] = {
            { (short)(ax + aw / 2), (short)(down_y0 + g_pal_arrow_h - 3) },
            { (short)(ax + 2), (short)(down_y0 + 3) },
            { (short)(ax + aw - 2), (short)(down_y0 + 3) },
        };
        XFillPolygon(dpy, buf, gc, down_tri, 3, Convex, CoordModeOrigin);
        draw_elem(g_pal_arrow_up, 0);
        draw_elem(g_pal_arrow_down, 0);
    }
    evhq_draw_entity_glyph();
    evhq_draw_chrome_bar();
    if (g_evhq_picker_open) evhq_draw_picker_overlay();
    evhq_append_frame_history();
}
static void evhq_activate_elem(Elem *hit) {
    if (!hit) return;
    /* REAL BUG FIX (2026-08-26, direct live report: "cancel doesn't
     * work. doesn't seem like any of the input does") - the picker's
     * real Elems (added earlier the same day for real nav) had a
     * nav_index but no `onclick`, and this function has never had a
     * generic onclick-first dispatch the way dbhq_activate_elem() does
     * (see !.HOUSE_STDS.md §K.3 item 4/5 - the real house convention is
     * onClick-driven: "EVERY element carrying onClick= is auto-numbered
     * into the keyboard nav"). A real mouse click on any picker row -
     * Cancel included - fell through every tag/id check below and did
     * nothing. Keyboard-driven interaction (Enter/Backspace/typed chars,
     * handled separately in evhq_handle_key()'s own g_evhq_picker_open
     * branch) was NOT affected by this bug and was already verified
     * live via the real relay - this fixes the MOUSE-click path only,
     * following the same real onclick convention every other real verb
     * in this house's dispatch chain already uses. */
    if (hit->onclick[0]) {
        if (strncmp(hit->onclick, "PICKER:", 7) == 0) evhq_dispatch_picker_onclick(hit->onclick);
        /* REAL, NEW 2026-08-28 (Phase C target #3) - same generic
         * scroll:up/down dispatch dbhq_activate_elem() already uses. */
        else if (strcmp(hit->onclick, "scroll:up") == 0 || strcmp(hit->onclick, "scroll:down") == 0) {
            g_pal_scroll += (strcmp(hit->onclick, "scroll:down") == 0) ? 1 : -1;
            evhq_redraw_content();
        }
        /* Visual block editor (2026-08-29, Part B: shared with Common
         * Events via evhq_handle_block_onclick() - see its own header
         * comment) - click-to-place Scratch: BLOCK:SEL:<i> picks a
         * palette piece, BLOCK:PLACE appends the chosen op via the
         * same action.txt boundary append: uses. */
        else if (evhq_handle_block_onclick(hit->onclick)) {
            evhq_redraw_content();
        }
        return;
    }
    if (strcmp(hit->tag, "closebtn") == 0) { g_quit = 1; return; }
    if (strcmp(hit->id, "new-page-btn") == 0) {
        /* Task H6 (2026-08-25) - request a new page from the manager.
         * REAL FIX (same day) - arm the pending-select flag so
         * evhq_load_pages() actually selects the new page once the
         * manager republishes it, instead of leaving the old page
         * silently selected under the new page's tab (see that
         * function's own header comment for the full bug). */
        FILE *af = fopen(g_evhq_mgr_action_path, "w");
        if (af) {
            fprintf(af, "new_page");
            fclose(af);
        }
        g_evhq_pending_select_new_page = 1;
        return;
    }
    if (strcmp(hit->id, "trigger-value") == 0) {
        /* Task H7 (2026-08-25) - arm trigger editing, same text-entry pattern as Add Command */
        g_evhq_trigger_edit_mode = 1;
        g_evhq_trigger_buffer[0] = '\0';
        return;
    }
    if (strcmp(hit->id, "play-test") == 0) {
        /* Task H8 (2026-08-25) - run the current event now via play_event.sh,
         * same real runtime path an entity's own Play METHOD row uses */
        FILE *af = fopen(g_evhq_mgr_action_path, "w");
        if (af) {
            fprintf(af, "play");
            fclose(af);
        }
        return;
    }
    /* Task 5 (2026-08-27) - viewtabs (id="viewtab-0/1/2") are also
     * tag="tab", same as page tabs - MUST be checked first by id, or
     * the generic page-tab branch below (matches by LABEL against
     * g_evhq_pages[]) could coincidentally match if a real page is ever
     * named "Scripting"/"Scratch"/"Blueprints". */
    if (strncmp(hit->id, "viewtab-", 8) == 0) {
        g_evhq_view_mode = atoi(hit->id + 8);
        evhq_refresh_page_data(g_window);
        return;
    }
    if (strcmp(hit->tag, "tab") == 0) {
        for (int i = 0; i < g_evhq_n_pages; i++) if (strcmp(hit->label, g_evhq_pages[i]) == 0) { g_evhq_current_page = i; break; }
        evhq_refresh_page_data(g_window);
        return;
    }
    if (strcmp(hit->id, "add-command") == 0) {
        g_evhq_picker_open = 1; g_evhq_picker_type = -1; g_evhq_picker_focus = 1; g_evhq_picker_scroll = 0;
        g_evhq_field1[0] = '\0'; g_evhq_field2[0] = '\0'; g_evhq_active_field = 0;
        g_evhq_edit_cmd_id = -1;
        return;
    }
    /* Task 7 (2026-08-26) - command rows are now real, nav-reachable,
     * editable Elems (id="cmd-edit-<real node id>"), same real click-to-
     * edit events-hq was missing entirely before this. */
    if (strncmp(hit->id, "cmd-edit-", 9) == 0) {
        int target_id = atoi(hit->id + 9);
        for (int i = 0; i < g_evhq_n_cmds; i++) if (g_evhq_cmds[i].id == target_id) { evhq_open_edit_picker(i); break; }
        return;
    }
}
static void evhq_handle_click(int px, int py) {
    /* REAL BUG FIX (2026-08-26, direct live report: "cancel doesn't
     * work. doesn't seem like any of the input does") - a SECOND real
     * bug alongside the missing onclick one: the picker's Elems
     * (g_picker_slots, built fresh in evhq_draw_picker_overlay()) are
     * NOT children of g_window at all - hit_test(g_window, ...) below
     * could never find them no matter what onclick they carry. While
     * the picker is open it's modal and owns g_nav[]/g_n_nav exclusively
     * (see evhq_draw_picker_overlay()'s own comment), so hit-test against
     * THAT array directly instead of the window tree, checked first. */
    if (g_evhq_picker_open) {
        for (int i = 0; i < g_n_nav; i++) {
            Elem *e = g_nav[i];
            if (px >= e->x && px < e->x + e->w && py >= e->y && py < e->y + e->h) {
                if (!click_focus_then_activate(e)) {
                    /* REAL FIX 2026-08-29 (live report: "mouse click
                     * not working... still no mouse click pickup" -
                     * root-caused via a temporary debug trace, live-
                     * reproduced): evhq_draw_picker_overlay() sets
                     * `g_focus_nav = g_evhq_picker_focus` (type list)
                     * unconditionally at the top of every redraw -
                     * click_focus_then_activate() above only updates
                     * g_focus_nav directly, never g_evhq_picker_focus
                     * (or g_evhq_active_field, the field-entry
                     * screen's own equivalent), so the redraw this
                     * same click triggers immediately stomped the
                     * mouse's own focus move back to whatever stale
                     * value those variables still held - keyboard nav
                     * worked because it updates the real variable
                     * directly; mouse never did. Sync whichever one
                     * is live for the current screen before redrawing. */
                    if (g_evhq_picker_type < 0) g_evhq_picker_focus = e->nav_index;
                    else g_evhq_active_field = e->nav_index - 1;
                    evhq_redraw_content();
                    return;
                }
                evhq_activate_elem(e);
                return;
            }
        }
        return;
    }
    if (px >= g_evhq_close_elem->x && px < g_evhq_close_elem->x + g_evhq_close_elem->w &&
        py >= g_evhq_close_elem->y && py < g_evhq_close_elem->y + g_evhq_close_elem->h) {
        g_focus_nav = g_evhq_close_elem->nav_index; evhq_activate_elem(g_evhq_close_elem); return;
    }
    /* REAL, NEW 2026-08-28 (Phase C target #3) - same synthetic-elem
     * coordinate check dbhq_handle_click() already uses for the scroll
     * arrows (they're drawn Elems but not children of g_window's parsed
     * tree, so hit_test() below would never find them). */
    if (g_pal_has_grid) {
        if (px >= g_pal_arrow_up->x && px < g_pal_arrow_up->x + g_pal_arrow_up->w &&
            py >= g_pal_arrow_up->y && py < g_pal_arrow_up->y + g_pal_arrow_up->h) {
            if (g_pal_arrow_up->nav_index > 0) g_focus_nav = g_pal_arrow_up->nav_index;
            evhq_activate_elem(g_pal_arrow_up);
            return;
        }
        if (px >= g_pal_arrow_down->x && px < g_pal_arrow_down->x + g_pal_arrow_down->w &&
            py >= g_pal_arrow_down->y && py < g_pal_arrow_down->y + g_pal_arrow_down->h) {
            if (g_pal_arrow_down->nav_index > 0) g_focus_nav = g_pal_arrow_down->nav_index;
            evhq_activate_elem(g_pal_arrow_down);
            return;
        }
    }
    Elem *hit = hit_test(g_window, px, py);
    if (!hit) return;
    if (!click_focus_then_activate(hit)) { evhq_redraw_content(); return; }
    evhq_activate_elem(hit);
}
static void evhq_submit_picker(void) {
    if (g_evhq_picker_type < 0 || g_evhq_picker_type >= g_evhq_n_cmd_defs) { g_evhq_picker_open = 0; return; }
    EvhqCommandDef *def = &g_evhq_cmd_defs[g_evhq_picker_type];
    /* REAL, 2026-08-26 - generic params_line build, "key=val|key=val"
     * (pipe-separated - see event_commands.registry.pdl's own header
     * comment), replacing the old per-type snprintf chain. field1 maps
     * to param_names[0], field2 (if this command has one) to
     * param_names[1] - positional, matching the picker's own two-field
     * UI exactly. An empty field2 still gets its own pipe segment (an
     * empty value, not an omitted one) so compile_page()'s generic
     * parser always finds a fixed number of segments per command type.
     *
     * Normalizations (control_switch ON/OFF, select2 None) run BEFORE
     * building the params_line so the normalized values reach the manager. */
    if (strcmp(def->type, "control_switch") == 0 &&
        def->n_params >= 2 && g_evhq_field2[0]) {
        if (strcasecmp(g_evhq_field2, "ON") == 0)
            snprintf(g_evhq_field2, sizeof(g_evhq_field2), "1");
        else if (strcasecmp(g_evhq_field2, "OFF") == 0)
            snprintf(g_evhq_field2, sizeof(g_evhq_field2), "0");
    }
    if (def->n_select2 > 0 && g_evhq_field2[0] &&
        strcasecmp(g_evhq_field2, "None") == 0)
        g_evhq_field2[0] = '\0';
    char params[512] = "";
    if (def->n_params >= 1) snprintf(params, sizeof(params), "%s=%s", def->param_names[0], g_evhq_field1);
    if (def->n_params >= 2) {
        char seg[300]; snprintf(seg, sizeof(seg), "|%s=%s", def->param_names[1], g_evhq_field2);
        strncat(params, seg, sizeof(params) - strlen(params) - 1);
    }
    /* Task 7 (2026-08-26) - editing an existing row sends "edit:", not
     * "append:". g_evhq_edit_cmd_id is armed by evhq_open_edit_picker()
     * and must always be reset here so the NEXT Add Command (a fresh
     * -1 picker_type) doesn't accidentally edit the last-edited row. */
    if (g_evhq_edit_cmd_id >= 0) evhq_request_edit_node(g_evhq_edit_cmd_id, def->type, params);
    else evhq_request_append_node(def->type, params);
    g_evhq_edit_cmd_id = -1;
    g_evhq_picker_open = 0;
}
static void evhq_handle_key(KeySym ks, char ch) {
    /* Task H7 (2026-08-25) - trigger editing, reuses the Add Command picker's
     * own keystroke-accumulation pattern rather than a second mechanism */
    if (g_evhq_trigger_edit_mode) {
        if (ks == XK_Escape) { g_evhq_trigger_edit_mode = 0; return; }
        if (ks == XK_Return || ks == XK_KP_Enter) {
            evhq_request_trigger_update(g_evhq_trigger_buffer);
            g_evhq_trigger_edit_mode = 0;
            return;
        }
        if (ks == XK_BackSpace) { size_t l = strlen(g_evhq_trigger_buffer); if (l > 0) g_evhq_trigger_buffer[l - 1] = '\0'; return; }
        if (ch >= 32 && ch <= 126) {
            size_t l = strlen(g_evhq_trigger_buffer);
            if (l + 1 < sizeof(g_evhq_trigger_buffer)) { g_evhq_trigger_buffer[l] = ch; g_evhq_trigger_buffer[l + 1] = '\0'; }
            return;
        }
        return;
    }

    if (g_evhq_picker_open) {
        if (ks == XK_Escape) { g_evhq_picker_open = 0; g_evhq_edit_cmd_id = -1; return; }
        if (g_evhq_picker_type < 0) {
            /* Direct instruction (2026-08-26): "they need a cancel" - a
             * real, nav-reachable Cancel option alongside Escape, not a
             * replacement for it. Cancel occupies one extra focus
             * position past the last real VISIBLE row (not past the
             * full command count - see g_evhq_picker_scroll's own
             * header comment, the registry now has more commands than
             * the box can show at once). Digits/arrows move within the
             * current visible window; Page_Up/Page_Down scroll it. */
            int last_row_focus = g_evhq_picker_visible_rows;
            if (g_evhq_picker_scroll + last_row_focus > g_evhq_n_cmd_defs)
                last_row_focus = g_evhq_n_cmd_defs - g_evhq_picker_scroll;
            if (ch >= '1' && ch <= '9' && (ch - '0') <= last_row_focus) g_evhq_picker_focus = ch - '0';
            else if (ks == XK_Up || ks == XK_Left) { if (g_evhq_picker_focus > 1) g_evhq_picker_focus--; }
            else if (ks == XK_Down || ks == XK_Right || ks == XK_Tab) { if (g_evhq_picker_focus < last_row_focus + 1) g_evhq_picker_focus++; }
            else if (ks == XK_Page_Up) {
                if (g_evhq_picker_scroll > 0) g_evhq_picker_scroll -= g_evhq_picker_visible_rows;
                if (g_evhq_picker_scroll < 0) g_evhq_picker_scroll = 0;
            }
            else if (ks == XK_Page_Down) {
                int max_scroll = g_evhq_n_cmd_defs - g_evhq_picker_visible_rows;
                if (max_scroll < 0) max_scroll = 0;
                g_evhq_picker_scroll += g_evhq_picker_visible_rows;
                if (g_evhq_picker_scroll > max_scroll) g_evhq_picker_scroll = max_scroll;
            }
            else if (ks == XK_Return || ks == XK_KP_Enter) {
                if (g_evhq_picker_focus == last_row_focus + 1) { g_evhq_picker_open = 0; g_evhq_edit_cmd_id = -1; return; }
                g_evhq_picker_type = g_evhq_picker_scroll + g_evhq_picker_focus - 1;
                /* Initialize select2 field to first option if empty */
                if (g_evhq_picker_type >= 0 && g_evhq_picker_type < g_evhq_n_cmd_defs) {
                    EvhqCommandDef *sel_def = &g_evhq_cmd_defs[g_evhq_picker_type];
                    if (sel_def->n_select2 > 0 && g_evhq_field2[0] == '\0')
                        snprintf(g_evhq_field2, sizeof(g_evhq_field2), "%s", sel_def->select2_options[0]);
                }
            }
            return;
        }
        if (g_evhq_picker_type >= g_evhq_n_cmd_defs) return;
        int n_params = g_evhq_cmd_defs[g_evhq_picker_type].n_params;
        int single_field = (n_params <= 1);
        /* Check if the active field is a SELECT2 cycle field */
        EvhqCommandDef *cur_def = &g_evhq_cmd_defs[g_evhq_picker_type];
        int active_is_select = 0;
        int active_select_idx = -1;
        if (g_evhq_active_field == 1 && cur_def->n_select2 > 0) {
            active_is_select = 1;
            char *active_val = g_evhq_field2;
            for (int si = 0; si < cur_def->n_select2; si++) {
                if (strcmp(active_val, cur_def->select2_options[si]) == 0) { active_select_idx = si; break; }
            }
            if (active_select_idx < 0 && active_val[0] == '\0') active_select_idx = 0;
        }
        if (active_is_select && (ks == XK_Left || ks == XK_Right)) {
            if (active_select_idx >= 0) {
                if (ks == XK_Left) active_select_idx = (active_select_idx - 1 + cur_def->n_select2) % cur_def->n_select2;
                else active_select_idx = (active_select_idx + 1) % cur_def->n_select2;
                snprintf(g_evhq_field2, sizeof(g_evhq_field2), "%s", cur_def->select2_options[active_select_idx]);
            }
            return;
        }
        /* Same real Cancel addition as the type-list above - one extra
         * focus position past the last real field (index == n_params),
         * reachable via Left/Right (Tab has no ASCII code so isn't
         * usable from the plain text relay, but Right/Left already are
         * via relay codes 202/203 - see dispatch_relay_code()). REAL,
         * NEW 2026-08-29 (live report: "the placed scratch blocks and
         * or events may need a 'delete' input button... trigger able
         * from visual nav / index, as usual") - a SECOND extra slot,
         * Delete, only when g_evhq_edit_cmd_id >= 0 (editing a real,
         * existing command - "Add Command" has nothing yet to delete).
         * Reuses this exact same nav-driven picker flow instead of a
         * separate focus-tracking mechanism (an earlier attempt at a
         * standalone "delete whatever's currently focused" footer
         * button was real but flawed - focus moves TO that button
         * before Enter, so by the time it activates, focus no longer
         * points at the row at all; this approach never has that
         * problem since Delete lives inside the SAME picker session
         * the row's own Enter already opened). */
        int last_slot = n_params + (g_evhq_edit_cmd_id >= 0 ? 1 : 0);
        if (ks == XK_Left) { if (g_evhq_active_field > 0) g_evhq_active_field--; return; }
        if (ks == XK_Right) { if (g_evhq_active_field < last_slot) g_evhq_active_field++; return; }
        if (g_evhq_active_field > n_params) {
            /* Focus is on the Delete slot. */
            if (ks == XK_Return || ks == XK_KP_Enter) {
                evhq_request_delete_node(g_evhq_edit_cmd_id);
                g_evhq_picker_open = 0; g_evhq_edit_cmd_id = -1;
                evhq_redraw_content();
            }
            return;
        }
        if (g_evhq_active_field == n_params) {
            /* Focus is on the Cancel slot - only Enter (handled here) and
             * Escape (handled above) do anything; typing/backspace are
             * no-ops here since there's no field buffer at this position. */
            if (ks == XK_Return || ks == XK_KP_Enter) { g_evhq_picker_open = 0; g_evhq_edit_cmd_id = -1; }
            return;
        }
        char *active = g_evhq_active_field == 0 ? g_evhq_field1 : g_evhq_field2;
        size_t asz = g_evhq_active_field == 0 ? sizeof(g_evhq_field1) : sizeof(g_evhq_field2);
        if (ks == XK_Return || ks == XK_KP_Enter) {
            if (!single_field && g_evhq_active_field == 0) { g_evhq_active_field = 1; return; }
            evhq_submit_picker();
            return;
        }
        if (ks == XK_BackSpace) { size_t l = strlen(active); if (l > 0) active[l - 1] = '\0'; return; }
        if (ch >= 32 && ch <= 126) {
            size_t l = strlen(active);
            if (l + 1 < asz) { active[l] = ch; active[l + 1] = '\0'; }
            return;
        }
        return;
    }
    if (ch == 'p') { dump_frame_png(); return; }
    if (ks == XK_Return || ks == XK_KP_Enter) {
        if (g_evhq_digit_accum > 0 && g_evhq_digit_accum <= g_n_nav) g_focus_nav = g_evhq_digit_accum;
        g_evhq_digit_accum = 0;
        if (g_focus_nav >= 1 && g_focus_nav <= g_n_nav) evhq_activate_elem(g_nav[g_focus_nav - 1]);
        return;
    }
    if (ks == XK_Escape) { if (g_evhq_digit_accum > 0) { g_evhq_digit_accum = 0; return; } g_quit = 1; return; }
    if (ch >= '0' && ch <= '9') {
        int d = ch - '0';
        int new_val = g_evhq_digit_accum * 10 + d;
        if (new_val > 0 && new_val <= g_n_nav) { g_evhq_digit_accum = new_val; g_focus_nav = new_val; }
        else if (d > 0 && d <= g_n_nav) { g_evhq_digit_accum = d; g_focus_nav = d; }
        else g_evhq_digit_accum = 0;
        return;
    }
    if (ks == XK_Up || ks == XK_Left) { if (g_focus_nav > 1) g_focus_nav--; g_evhq_digit_accum = 0; return; }
    if (ks == XK_Tab || ks == XK_ISO_Left_Tab) { if (g_evhq_has_real_focus) nav_tab_cycle(); g_evhq_digit_accum = 0; return; }
    if (ks == XK_Down || ks == XK_Right) { if (g_focus_nav < g_n_nav) g_focus_nav++; g_evhq_digit_accum = 0; return; }
    /* REAL, NEW 2026-08-28 (Phase C target #3) - same real Page_Up/
     * Page_Down paging dbhq_handle_key() already uses for any
     * g_pal_has_grid mode; events-hq's own command list had no keyboard
     * scroll path at all before this. */
    if (ks == XK_Page_Up || ks == XK_Page_Down) {
        if (g_pal_has_grid) {
            int step = g_pal_visible_rows > 1 ? g_pal_visible_rows - 1 : 1;
            g_pal_scroll += (ks == XK_Page_Down) ? step : -step;
            evhq_layout_pass(g_window);
            evhq_assign_nav_indices(g_window);
        }
        g_evhq_digit_accum = 0;
        return;
    }
    g_evhq_digit_accum = 0;
}
static int evhq_nonfatal_x_error(Display *d, XErrorEvent *e) {
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
static void dbhq_dump_debug_state(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "/tmp/db-hq-state.txt");
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "g_focus_nav=%d\n", g_focus_nav);
    fprintf(f, "g_dbhq_digit_accum=%d\n", g_dbhq_digit_accum);
    fprintf(f, "g_dbhq_current_tab=%d (%s)\n", g_dbhq_current_tab,
            (g_dbhq_current_tab >= 0 && g_dbhq_current_tab < DB_HQ_N_TABS) ? DB_HQ_TAB_LABELS[g_dbhq_current_tab] : "?");
    fprintf(f, "g_dbhq_selected_event=%d\n", g_dbhq_selected_event);
    fprintf(f, "g_dbhq_ce_editing=%d name=%s\n", g_dbhq_ce_editing, g_dbhq_ce_name);
    fprintf(f, "g_evhq_picker_open=%d trigger=%s n_cmds=%d\n", g_evhq_picker_open, g_evhq_trigger, g_evhq_n_cmds);
    fprintf(f, "g_evhq_picker_type=%d g_evhq_active_field=%d\n", g_evhq_picker_type, g_evhq_active_field);
    fprintf(f, "g_evhq_field1=[%s]\n", g_evhq_field1);
    fprintf(f, "g_evhq_field2=[%s]\n", g_evhq_field2);
    fprintf(f, "g_input_elem=%s\n", g_input_elem ? g_input_elem->id : "(null)");
    {
        Elem *panel = find_by_tag(g_window, "panel");
        if (panel) {
            fprintf(f, "panel->n_children=%d\n", panel->n_children);
            for (int i = 0; i < panel->n_children; i++) {
                Elem *c = panel->children[i];
                fprintf(f, "  panel_child[%d] tag=%s id=%s nav_index=%d label=%s\n", i, c->tag, c->id, c->nav_index, c->label);
            }
        } else {
            fprintf(f, "panel=NULL\n");
        }
    }
    fprintf(f, "DEBUG g_buf_w=%d g_buf_h=%d g_window_w=%d g_window_h=%d\n", g_buf_w, g_buf_h, g_window->w, g_window->h);
    fprintf(f, "g_pal_track_x=%d g_pal_track_y=%d g_pal_track_w=%d g_pal_track_h=%d g_pal_thumb_y=%d g_pal_thumb_h=%d g_pal_total_rows=%d g_pal_visible_rows=%d g_pal_scroll=%d\n",
            g_pal_track_x, g_pal_track_y, g_pal_track_w, g_pal_track_h, g_pal_thumb_y, g_pal_thumb_h, g_pal_total_rows, g_pal_visible_rows, g_pal_scroll);
    fprintf(f, "g_n_nav=%d\n", g_n_nav);
    for (int i = 0; i < g_n_nav; i++) {
        Elem *e = g_nav[i];
        /* REAL FIX 2026-08-28 (live investigation: a visible pixel-level
         * duplicate control couldn't be confirmed/denied from this dump
         * alone - it only ever printed tag/id/label, never real screen
         * geometry, so a pure draw-position bug is invisible here even
         * though render and this dump both read the exact same live
         * Elem tree). Real x/y/w/h added so a geometry bug is provable
         * via the cheap text dump instead of falling back to a PNG +
         * manual pixel inspection every time. */
        fprintf(f, "  nav[%d] tag=%s id=%s label=%s x=%d y=%d w=%d h=%d%s\n", i + 1, e->tag, e->id, e->label,
                e->x, e->y, e->w, e->h, (i + 1 == g_focus_nav) ? "  <-- FOCUS" : "");
    }
    fprintf(f, "scope_root=%s\n", g_dbhq_active_scope_root ? g_dbhq_active_scope_root->id : "(none)");
    if (g_window) {
        ElemFlatEntry flat[MAX_ELEMS];
        int nf = elem_flatten(g_window, flat, MAX_ELEMS);
        fprintf(f, "flatten_n=%d\n", nf);
        for (int i = 0; i < nf && i < 40; i++) {
            Elem *e = flat[i].elem;
            fprintf(f, "  flat[%d] parent=%d tag=%s id=%s\n", flat[i].index, flat[i].parent_index, e->tag, e->id);
        }
    }
    fclose(f);
}


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
static Elem g_default_close_elem_storage;
static Elem *g_default_close_elem = &g_default_close_elem_storage;
static Elem g_default_fullscreen_elem_storage;
static Elem *g_default_fullscreen_elem = &g_default_fullscreen_elem_storage;
static int g_default_is_fullscreen = 0;
static int g_default_pre_fullscreen_x = 0, g_default_pre_fullscreen_y = 0;

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
static void layout_scroll_region(Elem *container, int x, int y, int w, int h, int *scroll, int *out_lo, int *out_hi) {
    *out_lo = 0; *out_hi = 0;
    if (!container || h <= 0) return;
    int visible_rows = h / ROW_H;
    if (visible_rows < 1) visible_rows = 1;
    int total = 0;
    for (int i = 0; i < container->n_children; i++) {
        Elem *c = container->children[i];
        if (strcmp(c->tag, "item") == 0 || strcmp(c->tag, "text") == 0) total++;
    }
    int max_scroll = total > visible_rows ? total - visible_rows : 0;
    if (*scroll > max_scroll) *scroll = max_scroll;
    if (*scroll < 0) *scroll = 0;

    int row = 0;
    for (int i = 0; i < container->n_children; i++) {
        Elem *c = container->children[i];
        if (strcmp(c->tag, "item") != 0 && strcmp(c->tag, "text") != 0) continue;
        int visible = (row >= *scroll && row < *scroll + visible_rows);
        if (visible) {
            c->x = x; c->y = y + (row - *scroll) * ROW_H; c->w = w; c->h = ROW_H;
            css_compute_style(&g_sheet, c->tag, c->id, c->classes, c->n_classes, 0, &c->style);
            if (strcmp(c->tag, "item") == 0) {
                c->nav_index = ++g_n_nav;
                g_nav[g_n_nav - 1] = c;
                if (*out_lo == 0) *out_lo = c->nav_index;
                *out_hi = c->nav_index;
            } else {
                c->nav_index = 0;
            }
        } else {
            c->x = x; c->y = -100000; c->w = w; c->h = ROW_H;
            c->nav_index = 0;
        }
        row++;
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
 * scroll flow, always the container's own last ROW_H). */
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
         * own real composer included, is completely unaffected). */
        if (strcmp(c->tag, "cli_io") == 0) composer_rows = c->rows > 0 ? c->rows : 1;
        if (strcmp(c->tag, "scrolllist") == 0) scrolllist = c;
    }
    int composer_h = composer_rows * ROW_H;
    int y_cursor = y;
    for (int i = 0; i < container->n_children; i++) {
        Elem *c = container->children[i];
        if (strcmp(c->tag, "item") == 0 || strcmp(c->tag, "text") == 0) {
            c->x = x; c->y = y_cursor; c->w = w; c->h = ROW_H;
            css_compute_style(&g_sheet, c->tag, c->id, c->classes, c->n_classes, 0, &c->style);
            if (strcmp(c->tag, "item") == 0) { c->nav_index = ++g_n_nav; g_nav[g_n_nav - 1] = c; }
            else c->nav_index = 0;
            y_cursor += ROW_H;
        } else if (strcmp(c->tag, "cli_io") == 0) {
            c->x = x; c->y = y + h - composer_h; c->w = w; c->h = composer_h;
            css_compute_style(&g_sheet, c->tag, c->id, c->classes, c->n_classes, 0, &c->style);
            c->nav_index = ++g_n_nav; g_nav[g_n_nav - 1] = c;
        }
        /* scrolllist itself is positioned in its own real pass below,
         * once the fixed-row total (y_cursor's own final advance) is
         * known - skipped here on purpose. */
    }
    if (scrolllist) {
        int list_h = (y + h) - y_cursor - composer_h;
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
static int layout_sidebar_panel(Elem *page) {
    Elem *sidebar = find_by_tag(page, "sidebar");
    Elem *panel = find_by_tag(page, "panel");
    if (!sidebar || !panel) return 0;

    if (g_default_is_fullscreen) {
        g_win_w = DisplayWidth(dpy, screen);
        g_win_h = DisplayHeight(dpy, screen);
    } else {
        g_win_w = g_window->style.has_width ? g_window->style.width : DEFAULT_WIN_W;
        g_win_h = g_window->style.has_height ? g_window->style.height : DEFAULT_WIN_H;
    }

    sidebar->x = 0; sidebar->y = CHROME_H; sidebar->w = SIDEBAR_W; sidebar->h = g_win_h - CHROME_H;
    panel->x = SIDEBAR_W; panel->y = CHROME_H; panel->w = g_win_w - SIDEBAR_W; panel->h = g_win_h - CHROME_H;
    /* REAL, NEW 2026-08-31 (live report: "no separation elements") -
     * a real visible divider between the two regions belongs in CSS
     * (entity_menu_default.css's own generic `sidebar`/`cli_io` rules),
     * NOT set programmatically here - this default/popup mode's own
     * real content draw round-trips every frame through a text frame
     * file (dbhq_serialize_frame_subtree()/dbhq_paint_frame_line(),
     * see reparse_chtpm_if_changed()'s own sibling fix for the same
     * class of bug with input_buffer/target_id) which does NOT carry
     * style fields at all - the paint side always recomputes style
     * fresh from CSS (tag/id/classes), so anything set directly on
     * these live Elem objects would be silently discarded before ever
     * reaching the screen. Found live: this exact code used to set
     * has_bg_color/has_border_color right here and never once painted -
     * see entity_menu_default.css's own new `sidebar { ... }` rule for
     * the real fix (dbhq_paint_frame_line()'s own temp Elem calls
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
    layout_fixed_rows_and_scrolllist(sidebar, sidebar->x, sidebar->y, sidebar->w, sidebar->h,
                                      &g_default_sidebar_scroll, &g_default_sidebar_nav_lo, &g_default_sidebar_nav_hi);
    layout_fixed_rows_and_scrolllist(panel, panel->x, panel->y, panel->w, panel->h,
                                      &g_default_scrolllist_scroll, &g_default_scrolllist_nav_lo, &g_default_scrolllist_nav_hi);

    /* REAL, NEW 2026-09-01 - the real "X"/"!" chrome pair (see these
     * statics' own header comment). Positioned in the chrome strip's
     * own top-right corner, nav-numbered LAST (matches db-hq's own
     * g_dbhq_close_elem convention - a fresh window never opens with
     * Close already focused). */
    {
        /* REAL FIX 2026-09-01 (live report: "smushed to the right side...
         * can have more space and be more visible dont be too far
         * right") - wider buttons, a real gap between them, and a real
         * margin off the true right edge (not flush against it). */
        int btn_w = 32, btn_h = CHROME_H - 4, gap = 8, right_margin = 10;

        memset(g_default_fullscreen_elem, 0, sizeof(*g_default_fullscreen_elem));
        snprintf(g_default_fullscreen_elem->tag, sizeof(g_default_fullscreen_elem->tag), "item");
        snprintf(g_default_fullscreen_elem->id, sizeof(g_default_fullscreen_elem->id), "chrome-fullscreen");
        snprintf(g_default_fullscreen_elem->label, sizeof(g_default_fullscreen_elem->label), "!");
        snprintf(g_default_fullscreen_elem->onclick, sizeof(g_default_fullscreen_elem->onclick), "TOGGLE_FULLSCREEN");
        g_default_fullscreen_elem->x = g_win_w - btn_w * 2 - gap - right_margin; g_default_fullscreen_elem->y = 2;
        g_default_fullscreen_elem->w = btn_w; g_default_fullscreen_elem->h = btn_h;
        css_compute_style(&g_sheet, g_default_fullscreen_elem->tag, NULL, NULL, 0, 0, &g_default_fullscreen_elem->style);
        g_default_fullscreen_elem->nav_index = ++g_n_nav; g_nav[g_n_nav - 1] = g_default_fullscreen_elem;

        memset(g_default_close_elem, 0, sizeof(*g_default_close_elem));
        snprintf(g_default_close_elem->tag, sizeof(g_default_close_elem->tag), "item");
        snprintf(g_default_close_elem->id, sizeof(g_default_close_elem->id), "chrome-close");
        snprintf(g_default_close_elem->label, sizeof(g_default_close_elem->label), "X");
        snprintf(g_default_close_elem->onclick, sizeof(g_default_close_elem->onclick), "CLOSE");
        g_default_close_elem->x = g_win_w - btn_w - right_margin; g_default_close_elem->y = 2;
        g_default_close_elem->w = btn_w; g_default_close_elem->h = btn_h;
        css_compute_style(&g_sheet, g_default_close_elem->tag, NULL, NULL, 0, 0, &g_default_close_elem->style);
        g_default_close_elem->nav_index = ++g_n_nav; g_nav[g_n_nav - 1] = g_default_close_elem;
    }

    if (g_focus_nav > g_n_nav) g_focus_nav = g_n_nav > 0 ? g_n_nav : 1;
    if (g_focus_nav < 1) g_focus_nav = 1;
    return 1;
}
/* ============ end generic sidebar+panel scroll ============ */

static void assign_nav_and_layout(void) {
    /* REAL Stage 5 §5d.10 (2026-08-16) - db-hq mode branch, real WM-
     * managed window shape, own layout/nav functions (ported verbatim,
     * not forced into the popup modes' page/item shape below). */
    if (g_is_db_hq) { dbhq_layout_pass(g_window); dbhq_assign_nav_indices(g_window); return; }
    if (g_is_events_hq) { evhq_layout_pass(g_window); evhq_assign_nav_indices(g_window); return; }
    g_n_nav = 0;
    Elem *page = find_page(g_current_page);
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
    if (layout_sidebar_panel(page)) { g_default_has_sidebar_panel = 1; return; }
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
         * REAL, NEW 2026-08-29 (TASK 2) - opacity control buttons (non-swatch
         * items) are positioned below the grid, with dynamic height calculation. */
        int x0 = 16, y0 = CHROME_H + 44;
        int sw_i = 0;
        int max_y = y0;
        int other_y = CHROME_H + 180;  /* Start position for non-swatch items */
        for (i = 0; i < page->n_children; i++) {
            Elem *item = page->children[i];
            int is_sw = 0, is_close = 0, c;
            if (strcmp(item->tag, "item") != 0) continue;
            for (c = 0; c < item->n_classes; c++) {
                if (strcmp(item->classes[c], "swatch") == 0) is_sw = 1;
                if (strcmp(item->classes[c], "close-btn") == 0) is_close = 1;
            }
            if (strcmp(item->id, "close") == 0) is_close = 1;
            if (is_close) {
                item->x = g_win_w - 60; item->y = 0; item->w = 60; item->h = CHROME_H;
            } else if (is_sw) {
                int col = sw_i % SWATCH_COLS, row = sw_i / SWATCH_COLS;
                item->x = x0 + col * (SWATCH + SWATCH_GAP);
                item->y = y0 + row * (SWATCH + SWATCH_GAP);
                item->w = SWATCH; item->h = SWATCH;
                if (sw_i < 12) {
                    snprintf(g_palette_name_buf[sw_i], sizeof(g_palette_name_buf[sw_i]), "%s", item->label);
                    g_palette_name[sw_i] = g_palette_name_buf[sw_i];
                }
                item->label[0] = '\0';
                sw_i++;
                if (item->y + item->h > max_y) max_y = item->y + item->h;
            } else {
                item->x = 0; item->y = other_y; item->w = g_win_w; item->h = ROW_H;
                if (item->y + item->h > max_y) max_y = item->y + item->h;
                other_y += ROW_H;
            }
            item->nav_index = ++g_n_nav;
            g_nav[g_n_nav - 1] = item;
            css_compute_style(&g_sheet, item->tag, item->id, item->classes, item->n_classes, 0, &item->style);
        }
        g_win_h = max_y + 8;  /* Dynamic height to fit swatches + any other items */
        } else {
        int y = CHROME_H;
        for (int i = 0; i < page->n_children; i++) {
            Elem *item = page->children[i];
            /* REAL, NEW 2026-08-31 (found live testing open-hai's own
             * .chtpm projection: "looks nothing like the old one" /
             * "not able to enter keys") - real bug, not a guess: this
             * loop only ever laid out "item"/"cli_io" rows, so any
             * plain <text> row (a status line, a transcript message, a
             * tool-approval banner - ordinary non-interactive content
             * ANY khtpm consumer's own .chtpm might mix in) was left at
             * its real parse-time default x/y/w/h (0,0,0,0) - never
             * positioned, garbled on top of row 0, and worse, silently
             * shifting every item/cli_io AFTER it up by one full row
             * from where its own document position visually implies.
             * Fixed generically: a "text" row now advances y exactly
             * like an item row (real vertical space, real row height),
             * it's simply never added to g_nav (it isn't interactive -
             * no real nav_index, can't be focused/clicked/armed). */
            int is_text = strcmp(item->tag, "text") == 0;
            if (strcmp(item->tag, "item") != 0 && strcmp(item->tag, "cli_io") != 0 && !is_text) continue;
            item->x = 0; item->y = y; item->w = g_win_w; item->h = ROW_H;
            if (!is_text) { item->nav_index = ++g_n_nav; g_nav[g_n_nav - 1] = item; }
            css_compute_style(&g_sheet, item->tag, item->id, item->classes, item->n_classes, 0, &item->style);
            y += ROW_H;
        }
        g_win_h = y + 8;
        }
    }
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
static void apply_theme(const char *bg_hex, const char *fg_hex);
static void dispatch(const char *action) {
    if (strncmp(action, "PICK:", 5) == 0) {
        char ap[PATH_BUF];
        snprintf(ap, sizeof(ap), "%s/#.desktop/taskbar_settings_action.txt", g_house_root);
        FILE *af = fopen(ap, "w");
        if (af) { fprintf(af, "seq=%u\n%s\n", ++g_swatch_action_seq, action); fclose(af); }
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
            g_win_x = 0; g_win_y = 0;
        } else {
            g_win_x = g_default_pre_fullscreen_x; g_win_y = g_default_pre_fullscreen_y;
        }
        XMoveWindow(dpy, win, g_win_x, g_win_y);
        return;
    }
    /* REAL FIX 2026-08-16, direct live report ("cancel doesn't work
     * yet"): the legacy dispatch (tp_desktop_window_rgb.c line ~2026)
     * ALWAYS calls close_context_menu() before even looking at the
     * action - "void" only skips running a shell command, it still
     * closes the menu. This copy returned without setting g_quit, so
     * Cancel/Stop silently left the window open. */
    if (strcmp(action, "void") == 0) { g_quit = 1; return; }
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
    if (!g_default_has_sidebar_panel) g_quit = 1;
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

/* REAL, ported verbatim from taskbar-settings' own real apply_theme()
 * - builds the full apply_theme_op command string (bg/fg baked in)
 * and fires it through the SAME shared dispatch() every mode uses. */
static void apply_theme(const char *bg_hex, const char *fg_hex) {
    char cmd[PATH_BUF * 3];
    snprintf(cmd, sizeof(cmd), "'%s/*.monads/*.livedesk-taskbar/ops/+x/apply_theme_op.+x' '%s' '%s' '%s'",
             g_house_root, g_house_root, bg_hex, fg_hex);
    dispatch(cmd);
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

static void default_cli_io_handle_key(KeySym ks, char ch) {
    Elem *e = g_default_input_elem;
    if (!e) return;
    if (ks == XK_Return || ks == XK_KP_Enter) {
        default_cli_io_save(e);
        default_cli_io_run_action(e->onclick, e->input_buffer);
        e->input_buffer[0] = '\0';
        default_cli_io_save(e); /* real, empty value, matching the reference's own "clear after submit, stay active" behavior */
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
     * dbhq_grab_keyboard_retry() (db-hq's own real XGrabKeyboard retry,
     * currently gated behind its own g_dbhq_focus_grab_enabled .pdl
     * flag for THAT mode) - reused verbatim, unconditionally, scoped to
     * exactly a cli_io field's own armed lifetime. An exclusive
     * keyboard grab routes KeyPress to `win` regardless of pointer
     * position or window-manager focus policy, so this is immune to
     * the exact failure just diagnosed. Safe to make unconditional
     * here (no existing popup uses cli_io yet, so this can't regress
     * any of them) - see the matching XUngrabKeyboard on every real
     * disarm path (Escape here, reparse_chtpm_if_changed()'s own real
     * safety net). */
    if (ks == XK_Escape) { g_default_input_elem = NULL; XUngrabKeyboard(dpy, CurrentTime); return; }
    if (ks == XK_BackSpace) {
        size_t len = strlen(e->input_buffer);
        if (len > 0) { e->input_buffer[len - 1] = '\0'; default_cli_io_save(e); }
        return;
    }
    if (ch >= 32 && ch < 127) {
        size_t len = strlen(e->input_buffer);
        if (len + 1 < sizeof(e->input_buffer)) {
            e->input_buffer[len] = ch; e->input_buffer[len + 1] = '\0';
            default_cli_io_save(e);
        }
    }
}

static void activate_focused(void) {
    if (g_focus_nav < 1 || g_focus_nav > g_n_nav) return;
    Elem *item = g_nav[g_focus_nav - 1];
    /* REAL FIX 2026-08-31 - see default_cli_io_handle_key()'s own
     * Escape-branch comment for the full real diagnosis. Grab taken
     * HERE (arm time), released on every real disarm path. */
    if (strcmp(item->tag, "cli_io") == 0) { g_default_input_elem = item; dbhq_grab_keyboard_retry(); return; }
    if (item->onclick[0]) dispatch(item->onclick);
}

static void redraw(void) {
    /* REAL §5d.12 (2026-08-16) - chat-hai mode: chai_redraw() is
     * self-contained (own layout, own present via XGetImage->XPutImage,
     * own frame-history append) - ported verbatim, not split into a
     * content-only half like db-hq/events-hq, since its own real
     * redraw() already did its own blit. Early return, no generic
     * present needed. */
    /* REAL Stage 5 §5d.10 (2026-08-16) - db-hq mode: own real content
     * draw (chrome/tabbar/sidebar/panel), same shared present
     * (XGetImage->XPutImage) below every mode already uses. */
    if (g_is_db_hq || g_is_events_hq) {
        if (g_is_db_hq) dbhq_redraw_content(); else evhq_redraw_content();
        /* Real fix 2026-08-28 (see g_buf_w/g_buf_h's own header comment)
         * - content just drawn above may have grown g_window->w/h past
         * the Pixmap's real allocated size (palettes' rmmv tab bar +
         * tileset chooser rows are the first real case of this). Detect
         * and recreate BEFORE the XGetImage below, which otherwise
         * requests a rectangle larger than the real Pixmap and X
         * rejects the whole request with BadMatch (a fatal, unhandled
         * default Xlib error handler - the process dies, not just that
         * one draw call). The Pixmap itself only ever grows (a smaller
         * frame is harmless to read from an oversized buffer - real
         * savings, not correctness). */
        if (g_window->w > g_buf_w || g_window->h > g_buf_h) {
            int new_w = g_window->w > g_buf_w ? g_window->w : g_buf_w;
            int new_h = g_window->h > g_buf_h ? g_window->h : g_buf_h;
            if (xftdraw_buf) { XftDrawDestroy(xftdraw_buf); xftdraw_buf = NULL; }
            if (buf) XFreePixmap(dpy, buf);
            buf = XCreatePixmap(dpy, win, (unsigned)new_w, (unsigned)new_h, (unsigned)DefaultDepth(dpy, screen));
            xftdraw_buf = XftDrawCreate(dpy, buf, DefaultVisual(dpy, screen), cmap);
            g_buf_w = new_w; g_buf_h = new_h;
            XSync(dpy, False);
            /* The just-resized Pixmap is undefined content (fresh
             * XCreatePixmap, not a copy of the old one) - the content
             * draw above already ran against the OLD buf, so re-run it
             * now that buf is the right size, or this frame would blit
             * garbage/black instead of the real content. */
            if (g_is_db_hq) dbhq_redraw_content(); else evhq_redraw_content();
        }
        /* REAL FIX 2026-08-28 (live report + real screenshot: switching
         * between rmmv tabs/tilesets with very different real content
         * sizes left an old, larger session's tiles visibly showing as
         * a "second layer" below the new, smaller content) - the REAL
         * on-screen X11 window was never resized DOWN to match shrunk
         * content, only ever grown (the block above only grows the
         * backing Pixmap, which is a different, legitimately-one-way
         * concern - reading less than an oversized Pixmap is harmless).
         * But XPutImage below only ever writes the TOP g_window->w x
         * g_window->h pixels of the real window - if the real window is
         * physically TALLER than that (never shrunk from an earlier,
         * bigger session), the excess strip below is simply never
         * touched again and keeps showing whatever was drawn there
         * last, indefinitely. The real window's own SIZE (unlike the
         * Pixmap's capacity) must track content exactly, both growing
         * AND shrinking, every time it changes - checked via real
         * XGetWindowAttributes rather than trusting a locally-tracked
         * variable, since this is real, occasionally-stale-prone state
         * (the WM can also resize/moves this override-redirect window). */
        {
            XWindowAttributes wa;
            if (XGetWindowAttributes(dpy, win, &wa) &&
                (wa.width != g_window->w || wa.height != g_window->h)) {
                XResizeWindow(dpy, win, (unsigned)g_window->w, (unsigned)g_window->h);
                XSync(dpy, False);
            }
        }
        XSync(dpy, False);
        XImage *frame = XGetImage(dpy, buf, 0, 0, (unsigned)g_window->w, (unsigned)g_window->h, AllPlanes, ZPixmap);
        if (frame) {
            XPutImage(dpy, win, gc, frame, 0, 0, 0, 0, (unsigned)g_window->w, (unsigned)g_window->h);
            XDestroyImage(frame);
        } else {
            XCopyArea(dpy, buf, win, gc, 0, 0, (unsigned)g_window->w, (unsigned)g_window->h, 0, 0);
        }
        XFlush(dpy);
        return;
    }
    assign_nav_and_layout();
    XSetForeground(dpy, gc, alloc_pixel("#1c1c1c"));
    XFillRectangle(dpy, buf, gc, 0, 0, (unsigned)g_win_w, (unsigned)g_win_h);
    XSetForeground(dpy, gc, alloc_pixel("#2a2a2a"));
    XFillRectangle(dpy, buf, gc, 0, 0, (unsigned)g_win_w, CHROME_H);

    /* REAL Stage 5 §5d.3 step 6 (2026-08-16) - real, data-selected
     * chrome text. Swatch-picker mode's own real title/status text,
     * ported verbatim from taskbar-settings' own redraw(); menu mode's
     * own real page-name title, unchanged. */
    {
        const char *title = (g_window->label[0] ? g_window->label : g_current_page);
        XftColor title_col = xft_color("#eeeeee");
        XftDrawStringUtf8(xftdraw_buf, &title_col, font_ui, 8, 16,
                           (const FcChar8 *)title, (int)strlen(title));
        XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &title_col);
        if (g_is_swatch_picker) {
            const char *status = g_phase == 0 ? "Pick PRIMARY, then Enter"
                                : g_phase == 1 ? "Pick SECONDARY, then Enter"
                                : "Applied - closing...";
            XftColor status_col = xft_color("#ffffff");
            XftDrawStringUtf8(xftdraw_buf, &status_col, font_ui, 16, CHROME_H + 26, (const FcChar8 *)status, (int)strlen(status));
            XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &status_col);
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
        snprintf(fpath, sizeof(fpath), "%s/#.desktop/%s", g_house_root,
                 g_is_swatch_picker ? "taskbar_settings_frame.txt" : "entity_menu_frame.txt");
        snprintf(tmpp, sizeof(tmpp), "%s.tmp", fpath);
        FILE *ff = fopen(tmpp, "w");
        if (ff) {
            dbhq_serialize_frame_subtree(ff, page);
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
            if (g_default_close_elem->w > 0) dbhq_serialize_frame_elem(ff, g_default_close_elem);
            if (g_default_fullscreen_elem->w > 0) dbhq_serialize_frame_elem(ff, g_default_fullscreen_elem);
            fclose(ff); rename(tmpp, fpath);
        }
        {
            FILE *rf = fopen(fpath, "r");
            if (rf) {
                char line[2048];
                while (fgets(line, sizeof(line), rf)) {
                    size_t len = strlen(line);
                    while (len > 0 && (line[len-1]=='\n' || line[len-1]=='\r')) line[--len] = '\0';
                    if (len) dbhq_paint_frame_line(line);
                }
                fclose(rf);
            }
        }
    }

    /* REAL, swatch-picker-only overlay (ported verbatim from taskbar-
     * settings' own redraw()) - the "chosen" bg/fg ring + primary/
     * secondary status lines, real, documented per-mode exceptions
     * (khtpm_draw_core.c's own draw_elem() has no generic 3rd-state
     * concept). */
    if (g_is_swatch_picker && page) {
        int sw_i = 0;
        for (int i = 0; i < page->n_children; i++) {
            Elem *item = page->children[i];
            if (strcmp(item->tag, "item") != 0 || strcmp(item->id, "close") == 0) continue;
            int chosen = (sw_i == g_chosen_bg_idx) || (sw_i == g_chosen_fg_idx);
            if (chosen && item->nav_index != g_focus_nav) {
                XSetForeground(dpy, gc, 0x22c55e);
                XDrawRectangle(dpy, buf, gc, item->x - 2, item->y - 2, (unsigned)item->w + 4, (unsigned)item->h + 4);
            }
            sw_i++;
        }
        int x0 = 16, y0 = CHROME_H + 44;
        XftColor accent = xft_color("#22c55e");
        if (g_chosen_bg_idx >= 0) {
            char line[64];
            snprintf(line, sizeof(line), "primary: %s", g_palette_name[g_chosen_bg_idx]);
            XftDrawStringUtf8(xftdraw_buf, &accent, font_ui, x0, y0 + 2 * (SWATCH + SWATCH_GAP) + 20, (const FcChar8 *)line, (int)strlen(line));
        }
        if (g_chosen_fg_idx >= 0) {
            char line[64];
            snprintf(line, sizeof(line), "secondary: %s", g_palette_name[g_chosen_fg_idx]);
            XftDrawStringUtf8(xftdraw_buf, &accent, font_ui, x0, y0 + 2 * (SWATCH + SWATCH_GAP) + 38, (const FcChar8 *)line, (int)strlen(line));
        }
        XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &accent);
    }

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
    if (g_is_events_hq) {
        snprintf(png, sizeof(png), "/tmp/events-hq-frame.png"); /* real, preserves khtpm_events_hq_render.c's own external contract */
        char cmd[PATH_BUF * 2];
        snprintf(cmd, sizeof(cmd), "'%s/&.widgits/_shared-lib/ops/+x/dump_frame_png_op.+x' 0x%lx '%s'",
                 g_house_root, (unsigned long)win, png);
        system(cmd); /* REAL, existing house-standard op-binary dispatch, reused verbatim - not new dispatch code */
        return;
    }
    if (g_is_db_hq) {
        snprintf(png, sizeof(png), "/tmp/db-hq-frame.png"); /* real, preserves khtpm_hq_render.c's own external contract */
        char cmd[PATH_BUF * 2];
        snprintf(cmd, sizeof(cmd), "'%s/&.widgits/_shared-lib/ops/+x/dump_frame_png_op.+x' 0x%lx '%s'",
                 g_house_root, (unsigned long)win, png);
        system(cmd); /* REAL, existing house-standard op-binary dispatch, reused verbatim - not new dispatch code */
        return;
    }
    if (g_is_swatch_picker) {
        char audit_dir[PATH_BUF];
        snprintf(audit_dir, sizeof(audit_dir), "%s/#.desktop/taskbar-settings-audit", g_house_root);
        mkdir(audit_dir, 0755);
        snprintf(png, sizeof(png), "%s/settings-frame.png", audit_dir);
    } else {
        snprintf(png, sizeof(png), "/tmp/entity-menu-frame.png");
    }
    char cmd[PATH_BUF * 2];
    snprintf(cmd, sizeof(cmd), "'%s/&.widgits/_shared-lib/ops/+x/dump_frame_png_op.+x' 0x%lx '%s'",
             g_house_root, (unsigned long)win, png);
    int ok = (system(cmd) == 0);
    if (g_is_swatch_picker) {
        char audit_dir[PATH_BUF], receipt[PATH_BUF];
        snprintf(audit_dir, sizeof(audit_dir), "%s/#.desktop/taskbar-settings-audit", g_house_root);
        snprintf(receipt, sizeof(receipt), "%s/settings-frame.png.receipt.txt", audit_dir);
        FILE *rf = fopen(receipt, "w");
        if (rf) {
            fprintf(rf, "ok=%d w=%d h=%d t=%ld nav=%d n_nav=%d phase=%d bg_idx=%d fg_idx=%d\n",
                    ok, g_win_w, g_win_h, (long)time(NULL), g_focus_nav, g_n_nav, g_phase, g_chosen_bg_idx, g_chosen_fg_idx);
            fclose(rf);
        }
    }
}

static void handle_key(KeySym ks, char ch) {
    /* REAL, events-hq mode only - routed BEFORE the shared 'p' dump
     * check, matching its own real key-order exactly: when its picker
     * overlay is open, 'p' must be swallowed as a literal typed
     * character in the active field, not intercepted as a dump
     * shortcut (its own original handle_key() checked g_picker_open
     * first, 'p' only afterward). */
    if (g_is_events_hq) { evhq_handle_key(ks, ch); return; }
    /* REAL 2026-08-25 (Stage 3 bookmarks port) - db-hq mode now has its
     * own armed input field (g_input_elem, bookmarks' New+ path entry)
     * and needs the SAME key-order exception as events-hq/chat-hai
     * above: 'p' must type into an armed field, not trigger a dump. */
    if (g_is_db_hq && g_input_elem) { dbhq_handle_key(ks, ch); return; }
    if (g_default_input_elem) { default_cli_io_handle_key(ks, ch); return; } /* same real key-order exception - a real cli_io field needs 'p' as a literal typed character */
    if (ch == 'p') { dump_frame_png(); return; }
    if (g_is_db_hq) { dbhq_handle_key(ks, ch); return; }
    if (ks == XK_Return || ks == XK_KP_Enter) { activate_focused(); return; }
    if (ks == XK_Escape) { g_quit = 1; return; }
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
    if (ks == XK_Up) { if (g_focus_nav > 1) g_focus_nav--; return; }
    if (ks == XK_Down) { if (g_focus_nav < g_n_nav) g_focus_nav++; return; }
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
    if (ch >= '1' && ch <= '9') { int d = ch - '0'; if (d <= g_n_nav) g_focus_nav = d; return; }
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
 * convention EXACTLY (nav_tab_dir()/nav_tab_register(), same file):
 * one real file per PROCESS, not per mode. Every consumer (a real
 * human's own X11 input via dbhq_capture_key()/dbhq_capture_click(),
 * or an external agent's relay write) now only ever reaches the ONE
 * window it actually targets - no possible cross-window bleed
 * regardless of how many windows of the same mode are open at once.
 * Discovery for an external writer that needs to find "the db-hq
 * window showing X": nav_master_current.txt already publishes
 * "<pid> <tab_ordinal> <nav_index> <id>" rows (see nav_ledger_
 * publish()), and nav_tab/<pid> holds that pid's real window title -
 * cross-reference the two, no new registry needed. */
static void history_dir(char *out, size_t outsz) {
    snprintf(out, outsz, "%s/#.desktop/%s", g_house_root,
             g_is_stats_hq ? "stats_hq_history" :
             g_is_db_hq ? "db_hq_history" :
             g_is_events_hq ? "events_hq_history" :
             g_is_swatch_picker ? "taskbar_settings_history" : "entity_menu_history");
}
static void history_path(char *out, size_t outsz) {
    char dir[PATH_BUF];
    history_dir(dir, sizeof(dir));
    mkdir(dir, 0777);
    snprintf(out, outsz, "%s/%d.txt", dir, (int)getpid());
}
/* Real cleanup counterpart to nav_tab_unregister() - called from the
 * same 4 real quit paths that call it, so a closed window's history
 * file doesn't sit around forever. Harmless if never opened. */
static void history_unregister(void) {
    char path[PATH_BUF];
    history_path(path, sizeof(path));
    unlink(path);
}

/* Phase 3a: capture-only. House format from pieces/keyboard/history.txt:
 *   MOUSE_EVENT: <button> <x> <y> <is_press>
 * Zero interpretation. Consume is poll_agent_history(). */
static void dbhq_capture_click(int x, int y, int button) {
    char path[PATH_BUF];
    history_path(path, sizeof(path));
    /* poll_agent_history() on first sight of a file sets cursor to EOF
     * and returns without reading (skip leftover agent lines at window
     * open). If the file did not exist yet, cursor is still -1 here, and
     * a same-tick poll after this append would skip the click we just
     * wrote. Pin cursor to pre-append size so only this new line is
     * consumed. */
    if (g_history_cursor < 0) {
        struct stat st;
        if (stat(path, &st) == 0) g_history_cursor = st.st_size;
        else g_history_cursor = 0;
    }
    FILE *f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "MOUSE_EVENT: %d %d %d 1\n", button, x, y);
    fclose(f);
}

/* Phase 3b: capture-only. House format KEY_PRESSED: <decimal>.
 * Printable ASCII as-is; Tab=9; Return/Esc/BS same as existing relay;
 * arrows/page 200-205 (already in dispatch_relay_code). Other keys
 * write the raw X11 KeySym so consume can handle_key(ks,0). */
static int dbhq_key_history_code(KeySym ks, char ch) {
    if (ch >= 32 && ch <= 126) return (unsigned char)ch;
    if (ks == XK_Tab || ks == XK_ISO_Left_Tab) return 9;
    if (ks == XK_Return || ks == XK_KP_Enter) return 13;
    if (ks == XK_Escape) return 27;
    if (ks == XK_BackSpace) return 8;
    if (ks == XK_Up) return 200;
    if (ks == XK_Down) return 201;
    if (ks == XK_Left) return 202;
    if (ks == XK_Right) return 203;
    if (ks == XK_Page_Up) return 204;
    if (ks == XK_Page_Down) return 205;
    return (int)ks;
}

static void dbhq_capture_key(KeySym ks, char ch) {
    char path[PATH_BUF];
    history_path(path, sizeof(path));
    if (g_history_cursor < 0) {
        struct stat st;
        if (stat(path, &st) == 0) g_history_cursor = st.st_size;
        else g_history_cursor = 0;
    }
    FILE *f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "KEY_PRESSED: %d\n", dbhq_key_history_code(ks, ch));
    fclose(f);
}

/* Tab-cycle: live registry is per-pid files (so two processes cannot
 * clobber one rewrite). Ledger is append-only audit. */
static int g_nav_tab_ordinal;

static void nav_tab_dir(char *out, size_t n) {
    snprintf(out, n, "%s/#.desktop/nav_tab", g_house_root);
}

static void nav_tab_register(const char *type, const char *title) {
    char dir[PATH_BUF], path[PATH_BUF], ledger[PATH_BUF];
    nav_tab_dir(dir, sizeof(dir));
    mkdir(dir, 0777);
    int max_ord = 0;
    DIR *d = opendir(dir);
    if (d) {
        struct dirent *de;
        while ((de = readdir(d))) {
            if (de->d_name[0] == '.') continue;
            char fp[PATH_BUF];
            snprintf(fp, sizeof(fp), "%s/%s", dir, de->d_name);
            pid_t pid = (pid_t)atoi(de->d_name);
            if (pid > 1 && kill(pid, 0) != 0 && errno == ESRCH) {
                unlink(fp);
                continue;
            }
            FILE *rf = fopen(fp, "r");
            if (!rf) continue;
            int ord = 0;
            unsigned long xid = 0;
            if (fscanf(rf, "%d %lx", &ord, &xid) >= 1 && ord > max_ord) max_ord = ord;
            fclose(rf);
        }
        closedir(d);
    }
    g_nav_tab_ordinal = max_ord + 1;
    snprintf(path, sizeof(path), "%s/%d", dir, (int)getpid());
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "%d %lx %s %s\n", g_nav_tab_ordinal, (unsigned long)win,
                type && type[0] ? type : "hq",
                title ? title : "hq");
        fclose(f);
    }
    snprintf(ledger, sizeof(ledger), "%s/#.desktop/nav_master_ledger.txt", g_house_root);
    FILE *lf = fopen(ledger, "a");
    if (lf) {
        fprintf(lf, "REG pid=%d tab=%d xid=%lx type=%s %s\n",
                (int)getpid(), g_nav_tab_ordinal, (unsigned long)win,
                type && type[0] ? type : "hq",
                title ? title : "hq");
        fclose(lf);
    }
}

static void nav_tab_unregister(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/nav_tab/%d", g_house_root, (int)getpid());
    unlink(path);
}

static void nav_tab_cycle(void) {
    char dir[PATH_BUF];
    nav_tab_dir(dir, sizeof(dir));
    typedef struct { int ord; unsigned long xid; pid_t pid; } Ent;
    Ent ents[64];
    int n = 0;
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *de;
    while ((de = readdir(d)) && n < 64) {
        if (de->d_name[0] == '.') continue;
        pid_t pid = (pid_t)atoi(de->d_name);
        char fp[PATH_BUF];
        snprintf(fp, sizeof(fp), "%s/%s", dir, de->d_name);
        if (pid > 1 && kill(pid, 0) != 0 && errno == ESRCH) {
            unlink(fp);
            continue;
        }
        FILE *rf = fopen(fp, "r");
        if (!rf) continue;
        int ord = 0;
        unsigned long xid = 0;
        if (fscanf(rf, "%d %lx", &ord, &xid) >= 2 && xid) {
            ents[n].ord = ord;
            ents[n].xid = xid;
            ents[n].pid = pid;
            n++;
        }
        fclose(rf);
    }
    closedir(d);
    if (n < 1) return;
    /* insertion sort by ordinal */
    for (int i = 1; i < n; i++) {
        Ent t = ents[i];
        int j = i;
        while (j > 0 && ents[j - 1].ord > t.ord) { ents[j] = ents[j - 1]; j--; }
        ents[j] = t;
    }
    int me = -1;
    pid_t selfpid = getpid();
    for (int i = 0; i < n; i++) if (ents[i].pid == selfpid) { me = i; break; }
    int nxt = (me >= 0) ? (me + 1) % n : 0;
    char want[PATH_BUF];
    snprintf(want, sizeof(want), "%s/#.desktop/nav_tab_active.txt", g_house_root);
    unsigned long seq = 1;
    FILE *rf2 = fopen(want, "r");
    if (rf2) {
        int t=0,p=0; unsigned long s=0;
        if (fscanf(rf2, "tab=%d pid=%d seq=%lu", &t, &p, &s) >= 3) seq = s + 1;
        fclose(rf2);
    }
    FILE *wf = fopen(want, "w");
    if (!wf) return;
    fprintf(wf, "tab=%d pid=%d seq=%lu\n", ents[nxt].ord, (int)ents[nxt].pid, seq);
    fclose(wf);
    /* Self-claim is handled by nav_tab_poll_active() in the loop so
     * the TARGET process focuses its OWN window (X11 won't let us
     * reliably activate a foreign client). */
    if (ents[nxt].pid == selfpid)
        nav_tab_poll_active();
}

static void nav_tab_poll_active(void) {
    char want[PATH_BUF];
    snprintf(want, sizeof(want), "%s/#.desktop/nav_tab_active.txt", g_house_root);
    FILE *f = fopen(want, "r");
    if (!f) return;
    int tab = 0, pid = 0;
    unsigned long seq = 0;
    static unsigned long last_seq = 0;
    if (fscanf(f, "tab=%d pid=%d seq=%lu", &tab, &pid, &seq) < 2) { fclose(f); return; }
    fclose(f);
    if (seq && seq == last_seq) return;
    last_seq = seq;
    if (tab != g_nav_tab_ordinal && pid != (int)getpid()) return;
    XUngrabKeyboard(dpy, CurrentTime);
    XRaiseWindow(dpy, win);
    XSetInputFocus(dpy, win, RevertToParent, CurrentTime);
    if (g_is_db_hq && g_dbhq_focus_grab_enabled) dbhq_grab_keyboard_retry();
    XFlush(dpy);
}



static unsigned long g_nav_ledger_ck;

static void nav_ledger_publish(void) {
    unsigned long ck = 5381;
    ck = ((ck << 5) + ck) + (unsigned)g_n_nav;
    ck = ((ck << 5) + ck) + (unsigned)g_nav_tab_ordinal;
    for (int i = 0; i < g_n_nav; i++) {
        Elem *e = g_nav[i];
        if (!e) continue;
        const char *s = e->id[0] ? e->id : (e->onclick[0] ? e->onclick : e->tag);
        ck = ((ck << 5) + ck) + (unsigned)e->nav_index;
        for (const char *p = s; *p; p++) ck = ((ck << 5) + ck) + (unsigned char)*p;
    }
    if (ck == g_nav_ledger_ck) return;
    g_nav_ledger_ck = ck;

    char cur[PATH_BUF], led[PATH_BUF];
    snprintf(cur, sizeof(cur), "%s/#.desktop/nav_master_current.txt", g_house_root);
    snprintf(led, sizeof(led), "%s/#.desktop/nav_master_ledger.txt", g_house_root);
    FILE *cf = fopen(cur, "w");
    FILE *lf = fopen(led, "a");
    if (lf) fprintf(lf, "SNAP pid=%d tab=%d n=%d\n", (int)getpid(), g_nav_tab_ordinal, g_n_nav);
    for (int i = 0; i < g_n_nav; i++) {
        Elem *e = g_nav[i];
        if (!e) continue;
        const char *s = e->id[0] ? e->id : (e->onclick[0] ? e->onclick : e->tag);
        char line[512];
        snprintf(line, sizeof(line), "%d %d %d %s\n",
                 (int)getpid(), g_nav_tab_ordinal, e->nav_index, s);
        if (cf) fputs(line, cf);
        if (lf) fputs(line, lf);
    }
    if (cf) fclose(cf);
    if (lf) fclose(lf);
}

/* Phase 4: wraith-alpha frame_changed.txt — FILE marker, size-only.
 * Helpers are mode-agnostic (path table, same shape as history_path()).
 * Pilot WIRING is db-hq's loop only; other loops still call redraw()
 * directly. Do not bake g_is_db_hq into mark/consume. */
static long g_frame_changed_last_size = -1;

static void frame_changed_path(char *out, size_t outsz) {
    snprintf(out, outsz, "%s/#.desktop/%s", g_house_root,
             g_is_palettes ? "palettes_frame_changed.txt" :
             g_is_bookmarks ? "bookmarks_frame_changed.txt" :
             g_is_stats_hq ? "stats_hq_frame_changed.txt" :
             g_is_db_hq ? "db_hq_frame_changed.txt" :
             g_is_events_hq ? "events_hq_frame_changed.txt" :
             g_is_swatch_picker ? "taskbar_settings_frame_changed.txt" :
             "entity_menu_frame_changed.txt");
}

static void mark_frame_changed(void) {
    char path[PATH_BUF];
    frame_changed_path(path, sizeof(path));
    FILE *f = fopen(path, "a");
    if (!f) return;
    fputc('.', f);
    fclose(f);
}

static int consume_frame_changed(void) {
    char path[PATH_BUF];
    struct stat st;
    frame_changed_path(path, sizeof(path));
    if (stat(path, &st) != 0) {
        g_frame_changed_last_size = 0;
        return 0;
    }
    if (g_frame_changed_last_size < 0) {
        g_frame_changed_last_size = st.st_size;
        return 0;
    }
    if (st.st_size < g_frame_changed_last_size) {
        g_frame_changed_last_size = st.st_size;
        return 0;
    }
    if (st.st_size > g_frame_changed_last_size) {
        g_frame_changed_last_size = st.st_size;
        return 1;
    }
    return 0;
}

/* Pilot: only the true db-hq window uses marker wiring this pass.
 * Palettes/bookmarks/stats-hq share this loop via g_is_db_hq=1. */
static int dbhq_marker_pilot(void) {
    return g_is_db_hq && !g_is_palettes && !g_is_bookmarks && !g_is_stats_hq;
}

static void dbhq_loop_request_redraw(void) {
    if (dbhq_marker_pilot()) mark_frame_changed();
    else redraw();
}

static void dbhq_loop_paint_if_dirty(void) {
    if (!dbhq_marker_pilot()) return;
    if (consume_frame_changed() && !g_quit) redraw();
}

static void dispatch_relay_code(int code) {
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
    else if (code == 210 && (g_is_db_hq || g_is_events_hq)) dbhq_dump_debug_state();
    else if (code >= 32 && code <= 126) handle_key(0, (char)code);
    else if (code > 255 && code != 200 && code != 201 && code != 202 &&
             code != 203 && code != 204 && code != 205 && code != 210)
        handle_key((KeySym)code, 0);
}
static int hq_window_has_x_focus(void) {
    if (g_is_events_hq) return g_evhq_has_real_focus;
    if (g_is_db_hq) return g_dbhq_has_real_focus;
    return 1;
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
                if (nf >= 3 && is_press && button != 3 && button != 4 && button != 5) {
                    if (g_is_db_hq) dbhq_handle_click(mx, my);
                    else if (g_is_events_hq) evhq_handle_click(mx, my);
                    else popup_handle_click(mx, my);
                }
                if (nf >= 3) n++;
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
}

static void hq_request_redraw(void) {
    if (dbhq_marker_pilot()) dbhq_loop_request_redraw();
    else if (!g_quit) redraw();
}

static void hq_idle_tick(void) {
    if (g_is_swatch_picker) {
        char sp[PATH_BUF];
        snprintf(sp, sizeof(sp), "%s/#.desktop/taskbar_settings_state.txt", g_house_root);
        FILE *sf = fopen(sp, "r");
        if (sf) {
            char line[64];
            int phase=g_phase, bg=g_chosen_bg_idx, fg=g_chosen_fg_idx, apply=0;
            while (fgets(line, sizeof(line), sf)) {
                if (strncmp(line, "phase=", 6)==0) phase=atoi(line+6);
                else if (strncmp(line, "bg=", 3)==0) bg=atoi(line+3);
                else if (strncmp(line, "fg=", 3)==0) fg=atoi(line+3);
                else if (strncmp(line, "apply=", 6)==0) apply=atoi(line+6);
            }
            fclose(sf);
            if (phase != g_phase || bg != g_chosen_bg_idx || fg != g_chosen_fg_idx) {
                g_phase = phase; g_chosen_bg_idx = bg; g_chosen_fg_idx = fg;
                redraw();
            }
            /* Only a completed 2-phase pick may close the picker.
             * Leftover apply=1 from a prior instance must not quit on launch. */
            if (apply && phase >= 2 && fg >= 0) g_quit = 1;
        }
    }
    if (g_is_events_hq) {
        if (evhq_load_pages() || evhq_load_page_state()) {
            evhq_refresh_page_data(g_window);
            redraw();
        }
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
    if (!g_is_db_hq && !g_is_events_hq) {
        if (reparse_chtpm_if_changed()) {
            assign_nav_and_layout(); redraw();
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
    if (pchq_theme_changed_dirty(g_house_root)) {
        set_window_opacity(dpy, win, load_theme_opacity());
    }
    if (poll_agent_history() > 0 && !g_quit) hq_request_redraw();
    if (g_is_db_hq || g_is_events_hq) nav_tab_poll_active();
    if (g_quit) return;
    if (g_is_db_hq) {
        if (g_is_bookmarks && dbhq_load_bookmark_state()) {
            Elem *panel = find_by_tag(g_window, "panel");
            dbhq_inject_bookmark_items(panel);
            dbhq_redraw_content();
        }
        if (g_is_palettes && g_pal_state_path[0]) {
            int changed = dbhq_load_palette_state();
            changed |= dbhq_load_palette_options();
            if (changed) {
                Elem *panel = find_by_tag(g_window, "panel");
                dbhq_inject_palette_tiles(panel);
                dbhq_redraw_content();
            }
        }
        /* REAL, NEW 2026-08-29 - visible "armed" feedback for the rmmv
         * brush, see g_pal_default_hint's own header comment for why.
         * Same mtime-checksum-gated poll shape g_pal_state_path already
         * uses (dbhq_file_checksum), not a fresh redraw every tick. */
        if (g_is_palettes && g_pal_armed_path[0] && g_pal_static_title) {
            struct stat ast;
            unsigned long cksum = (stat(g_pal_armed_path, &ast) == 0) ? dbhq_file_checksum(g_pal_armed_path) : 0;
            if (cksum != g_pal_armed_checksum) {
                g_pal_armed_checksum = cksum;
                char line[256] = "";
                if (cksum) {
                    FILE *af = fopen(g_pal_armed_path, "r");
                    if (af) { if (fgets(line, sizeof(line), af)) line[strcspn(line, "\r\n")] = '\0'; fclose(af); }
                }
                snprintf(g_pal_static_title->label, sizeof(g_pal_static_title->label), "%s",
                         line[0] ? line : g_pal_default_hint);
                /* Adds a second class (doesn't replace block-title, which
                 * other windows' titles also use) so armed reads as
                 * unmistakably different - see .pal-hint-armed's own
                 * header comment in palettes-rmmv.css. */
                if (line[0]) {
                    snprintf(g_pal_static_title->classes[1], sizeof(g_pal_static_title->classes[1]), "pal-hint-armed");
                    g_pal_static_title->n_classes = 2;
                } else {
                    g_pal_static_title->n_classes = 1;
                }
                dbhq_redraw_content();
            }
        }
        if (!g_is_palettes && !g_is_bookmarks && g_dbhq_current_tab == DB_HQ_ACTORS_TAB) {
            if (dbhq_load_actors()) {
                dbhq_show_actors();
                dbhq_loop_request_redraw();
            }
        } else if (!g_is_palettes && !g_is_bookmarks && dbhq_list_idx_for_tab(g_dbhq_current_tab) >= 0) {
            int li = dbhq_list_idx_for_tab(g_dbhq_current_tab);
            if (dbhq_load_list_tab(li)) {
                dbhq_show_list_tab();
                dbhq_loop_request_redraw();
            }
        } else if (!g_is_palettes && !g_is_bookmarks && dbhq_load_common_events()) {
            Elem *sidebar = find_by_tag(g_window, "sidebar");
            dbhq_inject_sidebar_items(sidebar);
            if (g_dbhq_selected_event < 0 && g_dbhq_n_events > 0) g_dbhq_selected_event = 0;
            if (g_is_stats_hq) {
                stats_populate_panel(g_dbhq_selected_event);
            } else {
                Elem *panel_text = find_by_tag(g_window, "text");
                if (panel_text && g_dbhq_selected_event >= 0 && g_dbhq_selected_event < g_dbhq_n_events)
                    snprintf(panel_text->label, sizeof(panel_text->label), "%s", g_dbhq_events[g_dbhq_selected_event]);
            }
            dbhq_loop_request_redraw();
        }
        if (g_dbhq_ce_editing) {
            Elem *panel = find_by_tag(g_window, "panel");
            if (dbhq_ce_inject_panel(panel))
                dbhq_loop_request_redraw();
        }
        dbhq_loop_paint_if_dirty();
    }
}

static void popup_handle_click(int px, int py) {
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
    for (int i = 0; i < g_n_nav; i++) {
        Elem *it = g_nav[i];
        if (px >= it->x && px < it->x + it->w && py >= it->y && py < it->y + it->h) {
            if (!click_focus_then_activate(it)) { redraw(); return; }
            activate_focused();
            return;
        }
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
static void dbhq_rmmv_handle_desktop_click(int x_root, int y_root) {
    if (x_root >= g_win_x && x_root < g_win_x + g_window->w &&
        y_root >= g_win_y && y_root < g_win_y + g_window->h) {
        /* Click landed back inside the picker's own window (e.g.
         * picking a different tile to re-arm with) - ungrab and let
         * the picker's own normal click handling take it from here
         * (re-arms via the same onclick path if it lands on a tile). */
        XUngrabPointer(dpy, CurrentTime);
        XUngrabKeyboard(dpy, CurrentTime);
        g_pal_rmmv_armed = 0;
        return;
    }

    XUngrabPointer(dpy, CurrentTime);
    XUngrabKeyboard(dpy, CurrentTime);
    g_pal_rmmv_armed = 0;
    char envx[32], envy[32];
    snprintf(envx, sizeof(envx), "%d", x_root);
    snprintf(envy, sizeof(envy), "%d", y_root);

    /* Real master ledger (nav_master_ledger.txt), same real append-
     * only convention nav_tab_register()/livedesk_registry_add()
     * already use for this exact file - written synchronously, in
     * this process, the instant the real click is resolved, decoupled
     * from whether the placement subprocess call below ever succeeds. */
    {
        char led[PATH_BUF];
        snprintf(led, sizeof(led), "%s/#.desktop/nav_master_ledger.txt", g_house_root);
        FILE *lf = fopen(led, "a");
        if (lf) {
            fprintf(lf, "RMMV_CLICK pid=%d x=%s y=%s\n", (int)getpid(), envx, envy);
            fclose(lf);
        }
        if (g_pal_static_title) {
            snprintf(g_pal_static_title->label, sizeof(g_pal_static_title->label),
                     "Clicked desktop at (%s,%s) - placing...", envx, envy);
            snprintf(g_pal_static_title->classes[1], sizeof(g_pal_static_title->classes[1]), "pal-hint-armed");
            g_pal_static_title->n_classes = 2;
            dbhq_redraw_content();
        }
    }

    /* tp_place_desktop_rmmv.+x reads its own click position straight
     * from nav_master_ledger.txt (just written above) - a real,
     * reusable, caller-agnostic op, not handed argv/env state here. */
    char cmd[PATH_BUF * 3];
    snprintf(cmd, sizeof(cmd),
             "'%s/&.widgits/tile-picker/ops/+x/tp_place_desktop_rmmv.+x' '%s/&.widgits/palettes/state' '%s/#.desktop' >/dev/null 2>&1",
             g_house_root, g_house_root, g_house_root);
    int rc = system(cmd);
    (void)rc;
}

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
static int g_pal_rmmv_button1_was_down = 0;

static void dbhq_rmmv_poll_pointer(void) {
    if (!g_pal_rmmv_armed) { g_pal_rmmv_button1_was_down = 0; return; }
    Window root_ret, child_ret;
    int root_x, root_y, win_x, win_y;
    unsigned int mask;
    if (!XQueryPointer(dpy, RootWindow(dpy, screen), &root_ret, &child_ret,
                        &root_x, &root_y, &win_x, &win_y, &mask)) {
        return;
    }
    int down = (mask & Button1Mask) ? 1 : 0;
    if (down && !g_pal_rmmv_button1_was_down) {
        g_pal_rmmv_button1_was_down = 1;
        dbhq_rmmv_handle_desktop_click(root_x, root_y);
    } else if (!down) {
        g_pal_rmmv_button1_was_down = 0;
    }
}

static void hq_dispatch_xevent(XEvent *ev, Atom wm_delete, int is_popup) {

    if (ev->type == Expose) {
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
    if (g_pal_rmmv_armed && ev->type == ButtonPress) {
        dbhq_rmmv_handle_desktop_click(ev->xbutton.x_root, ev->xbutton.y_root);
    }
    if (g_pal_rmmv_armed && ev->type == KeyPress) {
        KeySym ks = XLookupKeysym(&ev->xkey, 0);
        if (ks == XK_Escape) {
            XUngrabPointer(dpy, CurrentTime);
            XUngrabKeyboard(dpy, CurrentTime);
            g_pal_rmmv_armed = 0;
            /* Same file the arm/place C ops already use for visible
             * feedback - clear it so the picker's title reverts. */
            char armed_path[PATH_BUF];
            snprintf(armed_path, sizeof(armed_path), "%s/&.widgits/palettes/state/rmmv_armed.txt", g_house_root);
            unlink(armed_path);
            return;
        }
    }
    if (ev->type == ButtonPress) {
        if (is_popup) {
            /* REAL, NEW 2026-08-29 (TASK 1: popup drag support) - check for
             * drag-start on chrome area (y < CHROME_H), same pattern as
             * db-hq/events-hq/chat-hai. Button 1 only, top CHROME_H pixels.
             *
             * REAL FIX 2026-08-29 (live report: "why isn't x quit button
             * working for settings anymore?") - this window's own close
             * button lives INSIDE that same top strip (dbhq_layout_pass's
             * is_close block: x = g_win_w-60..g_win_w, y = 0..CHROME_H).
             * Without an exclusion this unconditionally ate every click
             * there as a drag-start before dbhq_capture_click() ever got a
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
             * before dbhq_capture_click() ever ran - keyboard Enter on
             * the focused nav item bypassed this entirely, which is
             * why it "worked from nav but not mouse". Real fix: use
             * the chrome trio's own real leftmost x (g_default_full_
             * screen_elem, already computed by layout_sidebar_panel())
             * as the exclusion boundary instead of a stale constant -
             * only when this window actually has real chrome
             * (g_default_has_sidebar_panel), else keep the original
             * 60px for any other popup that has just a plain close
             * corner and no chrome trio of its own. */
            int chrome_zone_x = (g_default_has_sidebar_panel && g_default_fullscreen_elem->w > 0)
                                 ? g_default_fullscreen_elem->x : g_win_w - 60;
            if (ev->xbutton.button == 1 && ev->xbutton.y < CHROME_H &&
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
            if (ms_since_map < PHANTOM_CLICK_GUARD_MS) return;
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
            XSetInputFocus(dpy, win, RevertToParent, CurrentTime);
            dbhq_capture_click(ev->xbutton.x, ev->xbutton.y, (int)ev->xbutton.button);
            poll_agent_history();
            if (!g_quit) redraw();
        } else if (g_is_db_hq) {
            if (g_dbhq_focus_grab_enabled) { dbhq_grab_keyboard_retry(); dbhq_soft_focus(); }
            if (ev->xbutton.button == 1 && g_pal_has_grid &&
                ev->xbutton.x >= g_pal_track_x && ev->xbutton.x < g_pal_track_x + g_pal_track_w &&
                ev->xbutton.y >= g_pal_track_y && ev->xbutton.y < g_pal_track_y + g_pal_track_h) {
                g_pal_thumb_dragging = 1;
                dbhq_pal_scroll_to_y(ev->xbutton.y);
                dbhq_layout_pass(g_window);
                dbhq_assign_nav_indices(g_window);
                dbhq_loop_request_redraw();
                return;
            }
            /* REAL FIX 2026-08-29 (live report: Common Events' Add-
             * Command picker had "all very weird behavior" - this
             * check was missing the same !g_evhq_picker_open guard
             * events-hq's own analogous drag-start check already has
             * (see that one's own comment) - a real click meant for
             * the modal picker, landing in the window's own top chrome
             * strip by coincidence, could arm a background window-drag
             * underneath the modal. */
            if (!g_evhq_picker_open && ev->xbutton.button == 1 && ev->xbutton.y < g_dbhq_chrome_h &&
                !(ev->xbutton.x >= g_dbhq_close_elem->x && ev->xbutton.x < g_dbhq_close_elem->x + g_dbhq_close_elem->w &&
                  ev->xbutton.y >= g_dbhq_close_elem->y && ev->xbutton.y < g_dbhq_close_elem->y + g_dbhq_close_elem->h)) {
                g_dbhq_dragging = 1;
                g_dbhq_drag_last_x = ev->xbutton.x_root;
                g_dbhq_drag_last_y = ev->xbutton.y_root;
            }
            if (!g_evhq_picker_open && g_pal_has_grid && (ev->xbutton.button == 4 || ev->xbutton.button == 5)) {
                g_pal_scroll += (ev->xbutton.button == 5) ? 2 : -2;
            } else if (ev->xbutton.button != 3 && ev->xbutton.button != 4 && ev->xbutton.button != 5) {
                dbhq_capture_click(ev->xbutton.x, ev->xbutton.y, (int)ev->xbutton.button);
                poll_agent_history();
            }
            if (!g_quit) dbhq_loop_request_redraw();
        } else if (g_is_events_hq) {
            if (!g_evhq_picker_open && ev->xbutton.button == 1 && ev->xbutton.y < EVHQ_CHROME_H &&
                !(ev->xbutton.x >= g_evhq_close_elem->x && ev->xbutton.x < g_evhq_close_elem->x + g_evhq_close_elem->w &&
                  ev->xbutton.y >= g_evhq_close_elem->y && ev->xbutton.y < g_evhq_close_elem->y + g_evhq_close_elem->h)) {
                g_evhq_dragging = 1;
                g_evhq_drag_last_x = ev->xbutton.x_root;
                g_evhq_drag_last_y = ev->xbutton.y_root;
            }
            if (!g_evhq_picker_open && g_pal_has_grid && (ev->xbutton.button == 4 || ev->xbutton.button == 5)) {
                g_pal_scroll += (ev->xbutton.button == 5) ? 2 : -2;
                evhq_layout_pass(g_window);
                evhq_assign_nav_indices(g_window);
            } else if (ev->xbutton.button != 3) {
                /* REAL FIX 2026-08-29 (live report: "mouse click not
                 * working in add commands sub window... instead it
                 * actually changing the tabs") - this used to also
                 * require `!g_evhq_picker_open`, so a REAL physical
                 * click while the picker overlay was open never even
                 * reached dbhq_capture_click()/poll_agent_history() at
                 * all - silently dropped before evhq_handle_click()'s
                 * own correct picker-aware hit-test (which checks
                 * g_evhq_picker_open FIRST and hit-tests g_nav[]
                 * instead of the main window tree) ever got a chance to
                 * run. This exact class of bug was invisible to every
                 * relay-driven test this session, since the file-relay
                 * MOUSE_EVENT path calls dbhq_capture_click() directly
                 * and was never subject to this gate - only a REAL
                 * physical click could ever trigger it. The drag-start
                 * and scroll-wheel guards just above stay gated (those
                 * really should be suppressed while modal); only the
                 * actual click-capture call was wrongly gated too. */
                g_evhq_has_real_focus = 1;
                dbhq_capture_click(ev->xbutton.x, ev->xbutton.y, (int)ev->xbutton.button);
                poll_agent_history();
            }
            if (!g_quit) redraw();
        }
        return;
    }
    if (ev->type == ButtonRelease && ev->xbutton.button == 1) {
        g_dbhq_dragging = 0;
        g_pal_thumb_dragging = 0;
        g_evhq_dragging = 0;
        g_popup_dragging = 0;  /* REAL, NEW 2026-08-29 (TASK 1) */
        return;
    }
    if (ev->type == MotionNotify) {
        if (g_is_db_hq && g_pal_thumb_dragging) {
            dbhq_pal_scroll_to_y(ev->xmotion.y);
            dbhq_layout_pass(g_window);
            dbhq_assign_nav_indices(g_window);
            dbhq_loop_request_redraw();
        } else if (g_is_db_hq && g_dbhq_dragging) {
            int dx = ev->xmotion.x_root - g_dbhq_drag_last_x;
            int dy = ev->xmotion.y_root - g_dbhq_drag_last_y;
            g_win_x += dx; g_win_y += dy;
            if (g_win_y < WM_MANAGED_DRAG_MIN_Y) g_win_y = WM_MANAGED_DRAG_MIN_Y;
            XMoveWindow(dpy, win, g_win_x, g_win_y);
            g_dbhq_drag_last_x = ev->xmotion.x_root;
            g_dbhq_drag_last_y = ev->xmotion.y_root;
        } else if (g_is_events_hq && g_evhq_dragging) {
            int dx = ev->xmotion.x_root - g_evhq_drag_last_x;
            int dy = ev->xmotion.y_root - g_evhq_drag_last_y;
            g_win_x += dx; g_win_y += dy;
            if (g_win_y < WM_MANAGED_DRAG_MIN_Y) g_win_y = WM_MANAGED_DRAG_MIN_Y;
            XMoveWindow(dpy, win, g_win_x, g_win_y);
            g_evhq_drag_last_x = ev->xmotion.x_root;
            g_evhq_drag_last_y = ev->xmotion.y_root;
        } else if (is_popup && g_popup_dragging) {
            /* REAL, NEW 2026-08-29 (TASK 1: popup drag-move) - same pattern
             * as other modes: compute delta from last recorded x_root/y_root,
             * update g_win_x/g_win_y, call XMoveWindow, clamp to WM_MANAGED_
             * DRAG_MIN_Y to avoid overlap with taskbar header. */
            int dx = ev->xmotion.x_root - g_popup_drag_last_x;
            int dy = ev->xmotion.y_root - g_popup_drag_last_y;
            g_win_x += dx; g_win_y += dy;
            if (g_win_y < WM_MANAGED_DRAG_MIN_Y) g_win_y = WM_MANAGED_DRAG_MIN_Y;
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
        if (is_popup) {
            dbhq_capture_key(ks, buf8[0]);
            poll_agent_history();
            if (!g_quit) redraw();
        } else if (g_is_db_hq) {
            snprintf(g_dbhq_last_key_label, sizeof(g_dbhq_last_key_label), "%s", kname ? kname : (buf8[0] ? buf8 : "?"));
            dbhq_capture_key(ks, buf8[0]);
            poll_agent_history();
            if (!g_quit) dbhq_loop_request_redraw();
        } else if (g_is_events_hq) {
            snprintf(g_evhq_last_key_label, sizeof(g_evhq_last_key_label), "%s", kname ? kname : (buf8[0] ? buf8 : "?"));
            g_evhq_has_real_focus = 1;
            dbhq_capture_key(ks, buf8[0]);
            poll_agent_history();
            if (!g_quit) redraw();
        }
        return;
    }
    if (ev->type == FocusIn) {
        if (g_is_db_hq && !g_dbhq_has_real_focus) {
            g_dbhq_has_real_focus = 1;
            { struct stat st; char hp[PATH_BUF]; history_path(hp, sizeof(hp));
              if (stat(hp, &st) == 0) g_history_cursor = st.st_size; }
            dbhq_loop_request_redraw();
        } else if (g_is_events_hq) {
            g_evhq_has_real_focus = 1;
            redraw();
        }
        return;
    }
    if (ev->type == FocusOut) {
        if (g_is_db_hq && g_dbhq_has_real_focus) {
            g_dbhq_has_real_focus = 0;
            dbhq_loop_request_redraw();
        } else if (g_is_events_hq) {
            g_evhq_has_real_focus = 0;
            redraw();
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
    while (!g_quit) {
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
        struct timeval tv = g_pal_rmmv_armed ? (struct timeval){ 0, 15000 } : (struct timeval){ 0, 150000 };
        select(xfd + 1, &fds, NULL, NULL, &tv);
        while (XPending(dpy)) {
            XEvent ev; XNextEvent(dpy, &ev);
            hq_dispatch_xevent(&ev, wm_delete, is_popup);
        }
        /* REAL, NEW 2026-08-29 - see dbhq_rmmv_poll_pointer's own
         * header comment. A real, non-event, XQueryPointer-based
         * fallback for real human mouse clicks, which a real Mutter/
         * XWayland bug never delivers as ButtonPress events to this
         * grabbing process. No-op (returns immediately) whenever not
         * armed, so this costs nothing on every other tick of every
         * other window's own event loop. */
        dbhq_rmmv_poll_pointer();
        if (dbhq_marker_pilot()) dbhq_loop_paint_if_dirty();
    }
}


/* REAL, NEW 2026-08-30 - piececraft-hq board-view khtpm conversion. See
 * g_is_pchq_board's own declaration comment for the real "why isolated"
 * reasoning, and PIECECRAFT-HQ-BOARD-KHTPM-CONVERSION-2026-08-30.md for
 * the full real writeup + the proven proof-of-concept
 * (pchq_board_view_poc.c) this whole block ports, verbatim in spirit,
 * into a real khtpm-family window (real chrome: title + close [X],
 * matching every other khtpm window's own visual convention - itself a
 * real, deliberate port of x11_mirror.c's own draw_chrome(), same
 * "steal code, don't reinvent" instruction this whole feature was built
 * under). */
static unsigned long pchq_alloc_pixel(Display *dpy, Colormap cmap, const char *spec) {
    XColor c;
    if (XParseColor(dpy, cmap, spec, &c) && XAllocColor(dpy, cmap, &c)) return c.pixel;
    return BlackPixel(dpy, DefaultScreen(dpy));
}

static int pchq_read_kv_int(const char *path, const char *key, int def) {
    FILE *f = fopen(path, "r");
    if (!f) return def;
    char line[128];
    size_t klen = strlen(key);
    int val = def;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, klen) == 0 && line[klen] == '=') { val = atoi(line + klen + 1); break; }
    }
    fclose(f);
    return val;
}

/* REAL, NEW 2026-08-30 (!.HOUSE_STDS.md §A.9) - resolves whether the
 * legacy engine's own real INTERACT element is genuinely engaged right
 * now, by reading the SAME real state its own onClick="INTERACT" click
 * handler persists (pieces/hero_01/state.txt's interact_mode flag,
 * under the HOST project - resolved via board-viewer's own real
 * bv_state.txt focused_project_root, not board-viewer's own session).
 * This is the one real signal this window uses to decide which side of
 * the engine's own active_index==-1 boundary it's on - see §A.9 for
 * the full model this mirrors. */
/* REAL FIX (2026-08-30, found live debugging "interact isn't yet
 * activating"): set_interact_mode() in chtpm_parser_pal.c writes
 * hero_01/state.txt under THIS board-viewer session's OWN
 * project_root_path - which has no hero_01 dir at all (silent no-op,
 * confirmed live: "No such file or directory"). The real,
 * unconditionally-written signal for "active_index != -1" (genuinely
 * engaged in an INTERACT/cli_io element right now) is
 * export_active_index()'s own pieces/display/active_gui_is_typing.txt
 * - a bare "1"/"0" (not key=value), confirmed live to read "1"
 * immediately after a real Enter-activation of the Interact button.
 * Same real file &.widgits/interact-fix-widget.txt already documented
 * using for this exact purpose - should have started here. */
static int pchq_is_interact_on(const char *bv_session) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/display/active_gui_is_typing.txt", bv_session);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char l[32] = "";
    if (!fgets(l, sizeof(l), f)) { fclose(f); return 0; }
    fclose(f);
    return atoi(l) != 0;
}

/* Real session discovery - a scoped-down port of pc_menu_input.c's own
 * open_board_widget() peer lookup (ledger_peers.+x, real, live, already
 * proven - not reinvented). Only finds the session dir; does NOT spawn
 * a new board-viewer widget if none is running (this mode is a real
 * DISPLAY for an already-live board-viewer session, launched
 * separately by piececraft-hq's own real "View Board" - a genuinely
 * separate concern from finding it). */
static int pchq_find_board_session(const char *house_root, const char *host_project_id, char *out, size_t outsz) {
    /* REAL FIX, found live testing this exact function - ledger_peers.+x
     * hard-requires PRISC_PROJECT_ROOT (confirmed: "Error: PRISC_
     * PROJECT_ROOT not set" running it bare) AND that dir's own real
     * pieces/system/house_root.txt (ledger_peers.c's own
     * resolve_house_root(), reads THAT file, not the env var directly).
     * button.sh only ever writes house_root.txt into piececraft-hq's
     * EPHEMERAL per-launch session dir (pieces/sessions/<id>/pieces/
     * system/house_root.txt), never the static project root - confirmed
     * live (real file only found under sessions/, real "no such file"
     * at the static path). Since this khtpm process is launched
     * independently of any one game session and has no real way to
     * know which session is "the" current one just from house_root/
     * host_project_id, write a real house_root.txt at the STATIC
     * project root once (same real content button.sh's own session
     * copy already has) so ledger_peers.+x can resolve it regardless of
     * which session is live - harmless, idempotent, matches this
     * file's own real content exactly. */
    char static_root[PATH_BUF], hr_path[PATH_BUF];
    snprintf(static_root, sizeof(static_root), "%s/@.apps/%s", house_root, host_project_id);
    snprintf(hr_path, sizeof(hr_path), "%s/pieces/system/house_root.txt", static_root);
    FILE *hrf = fopen(hr_path, "w");
    if (hrf) { fprintf(hrf, "%s\n", house_root); fclose(hrf); }

    char cmd[PATH_BUF * 2];
    snprintf(cmd, sizeof(cmd),
             "PRISC_PROJECT_ROOT='%s' '%s/&.widgits/board-viewer/ops/+x/ledger_peers.+x' widget 2>/dev/null",
             static_root, house_root);
    FILE *pf = popen(cmd, "r");
    if (!pf) return 0;
    char want[256];
    snprintf(want, sizeof(want), "board-viewer:%s", host_project_id);
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

/* REAL FIX 2026-08-30, direct live report ("i opened. killed from
 * close. and tried 2 open again. its not opening (pc-hq)") - the
 * pchq-board Close Elem only ever did `running = 0`, tearing down
 * THIS window's own X11 resources - it never touched the real,
 * underlying piececraft-hq session (its orchestrator, board-viewer
 * widget, everything button.sh's own `run` spawned). That whole real
 * game session kept running silently in the background - the next
 * taskbar click's own `run` invocation would find and kill that
 * lingering orchestrator via kill_own_orchestrator() (real, correct
 * cleanup, confirmed by direct code read), then start a genuinely NEW
 * session/orchestrator/board-viewer/chrome - so a real fresh window
 * SHOULD still have appeared... but "Close" leaving the OLD, real
 * game session alive for however long the user waits between close
 * and reopen is still real, wrong behavior (matches how every other
 * hq window's own Close - db-hq/chat-hai/events-hq - actually ends
 * the thing it's a chrome for, not just its own drawing surface).
 * Real fix: before tearing down this window, write the SAME real
 * pieces/system/quit_flag.txt orchestrator.c already polls for on
 * every tick (confirmed via direct read: "Exits when
 * pieces/system/quit_flag.txt becomes non-empty") - the exact same
 * signal Ctrl+C's own real quit path uses (keyboard_input.c's
 * write_quit_flag()) - so Close now triggers the REAL, full,
 * clean button.sh EXIT trap (kill_own_module, kill_own_board_widget,
 * persist_session_state, rm -rf SESSION_DIR) instead of just hiding
 * this one window over a still-live session. */
/* REAL, NEW 2026-08-30, direct instruction ("fullscreen... we will put
 * '!' for fullscreen next to 'x'") - the standard, real EWMH way to
 * toggle fullscreen on an ALREADY-MAPPED window: a real
 * _NET_WM_STATE ClientMessage sent to the root window (per the EWMH
 * spec - a direct XChangeProperty from the client itself is only
 * honored BEFORE the initial map, which this window already is well
 * past by the time a user clicks "!"). _NET_WM_STATE_TOGGLE (2) lets
 * the WM own the actual on/off bookkeeping - this function only
 * tracks *this window's own* believed state locally for the toolbar's
 * own badge/highlight, same as pchq_interact_on's own local mirror of
 * real engine state elsewhere in this function. */
static void pchq_toggle_fullscreen(Display *dpy, Window win) {
    Atom wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
    Atom fullscreen = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
    XEvent xev;
    memset(&xev, 0, sizeof(xev));
    xev.type = ClientMessage;
    xev.xclient.window = win;
    xev.xclient.message_type = wm_state;
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = 2; /* _NET_WM_STATE_TOGGLE */
    xev.xclient.data.l[1] = (long)fullscreen;
    xev.xclient.data.l[2] = 0;
    xev.xclient.data.l[3] = 1; /* source indication: normal application */
    XSendEvent(dpy, RootWindow(dpy, DefaultScreen(dpy)), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &xev);
    XFlush(dpy);
}

/* Same real cheap "changed marker" convention as khtpm_strip_parser.c/
 * tp_desktop_window_rgb.c's own theme_changed_dirty() (dc759f3c) -
 * kept as a local static cursor here since this mode is its own real
 * long-running loop, same shape as those two files' own. */
static long g_pchq_theme_changed_cursor = 0;
static int pchq_theme_changed_dirty(const char *house_root) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/livedesk_theme_changed.txt", house_root);
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    if (st.st_size != g_pchq_theme_changed_cursor) { g_pchq_theme_changed_cursor = st.st_size; return 1; }
    return 0;
}

/* REAL, NEW 2026-08-30, direct instruction ("lets look into dropdown
 * for pc taskbar (file and desk menu etc) cuz thats how we will prove
 * save load projects [note this as well, i haven't seen save load
 * from file in pc yet]") - File's own two real states
 * (default-pdl/default-legacy) are tracked in the HOST's own real,
 * static config.txt (pc_menu_input.c's FILE_MENU/DESK_MENU handlers
 * write active_level/active_board there via resolve_real_root() -
 * confirmed by direct read: that resolves to the STATIC project root,
 * not the ephemeral session dir, since real_project_root.txt always
 * points back to it). Reading the SAME static path directly - no
 * session resolution needed, this file is written once per real
 * FILE_MENU/DESK_MENU action regardless of which session triggered
 * it. */
static void pchq_read_config_kv(const char *house_root, const char *host_project_id, const char *key, char *out, size_t outsz) {
    out[0] = '\0';
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/@.apps/%s/pieces/system/config.txt", house_root, host_project_id);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[PATH_BUF];
    size_t klen = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, klen) == 0 && line[klen] == '=') {
            snprintf(out, outsz, "%s", line + klen + 1);
            out[strcspn(out, "\r\n")] = '\0';
            break;
        }
    }
    fclose(f);
}

static void pchq_quit_host_session(const char *house_root, const char *host_project_id) {
    char sessions_dir[PATH_BUF];
    snprintf(sessions_dir, sizeof(sessions_dir), "%s/@.apps/%s/pieces/sessions", house_root, host_project_id);
    DIR *d = opendir(sessions_dir);
    if (!d) return;
    char latest[256] = "";
    long latest_ts = -1;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        long ts = atol(ent->d_name);
        if (ts > latest_ts) { latest_ts = ts; snprintf(latest, sizeof(latest), "%s", ent->d_name); }
    }
    closedir(d);
    if (!latest[0]) return;
    char quit_path[PATH_BUF];
    snprintf(quit_path, sizeof(quit_path), "%s/%s/pieces/system/quit_flag.txt", sessions_dir, latest);
    FILE *f = fopen(quit_path, "w");
    if (f) { fprintf(f, "1\n"); fclose(f); }
}

/* REAL, NEW 2026-08-30, direct live report ("there are 2 renders on
 * screen") - confirmed via real xwininfo output: this mode's own
 * window and the legacy x11_mirror.+x-based board-viewer widget window
 * were both real, both mapped, at the EXACT SAME screen position -
 * this mode never replaced the legacy display, it just sat alongside
 * it. Direct instruction from earlier in this same session ("we are
 * meant to get rid of 'board-view widget'... its the board view widget
 * that needs to be converted to khtpm") - once this mode successfully
 * finds and attaches to a live board-viewer session, kill THAT
 * session's own x11_mirror.+x process (cwd-scoped, same real technique
 * board-viewer's own button.sh already uses for its bv_set_wm_pid
 * targeting - see that file's own real cwd-match pgrep loop). Leaves
 * every OTHER real board-viewer process for that same session alone
 * (chtpm_parser_pal/prisc+x/bv_render_3d.c/bv_compose_frame.c) - those
 * are what actually GENERATE the real rgb_frame_3d_overlay.raw this
 * mode reads, killing them would break the real data source, not just
 * the redundant legacy display. */
static void pchq_kill_legacy_display(const char *bv_session) {
    char cmd[PATH_BUF * 2];
    snprintf(cmd, sizeof(cmd),
             "for p in $(pgrep -f 'x11_mirror\\.\\+x'); do "
             "cwd=$(readlink -f /proc/$p/cwd 2>/dev/null); "
             "if [ \"$cwd\" = '%s' ]; then kill $p; fi; done",
             bv_session);
    int rc = system(cmd);
    (void)rc;
}

/* REAL, NEW 2026-08-30, direct instruction: "thats not what the legacy
 * chtpm peice board-view did u need to stick as closely to that model
 * as possible. absolute parity. research it and see where u went
 * wrong." Real research finding (see PIECECRAFT-HQ-BOARD-KHTPM-
 * CONVERSION-2026-08-30.md for the full writeup):
 *   1. board_viewer.chtpm has a real, declarative <interact src="..."/>
 *      + a reserved onClick="INTERACT" button - the ENGINE
 *      (chtpm_parser_pal.c) handles ALL real nav/focus/arrow-relay/ESC
 *      natively, with zero app-side code - this is the real "for free"
 *      system, and it lives entirely in the legacy engine, not
 *      anything this file can reimplement locally with real parity.
 *   2. system/chtpm_rgb_render.c (a real, shared compositor daemon,
 *      NOT the same thing as the window-display step) already reads
 *      BOTH the real text chrome chtpm_parser_pal renders into
 *      current_frame.txt AND the real 3D overlay bv_render_3d.c
 *      writes, and blits them into ONE real, fully-composited
 *      rgb_frame.raw (see that file's own blit_overlay()/MAP3D_MARKER
 *      header comment) - x11_mirror.c only ever needed to blit THAT
 *      one file and forward every real key/click into board-viewer's
 *      own real relay files, letting the real engine do everything
 *      else. The earlier version of this function read rgb_frame_3d_
 *      overlay.raw DIRECTLY (skipping the real compositor's own output
 *      entirely) and hand-drew its own separate chrome/nav on top -
 *      real parity means NOT doing either of those things.
 * Real fix: blit rgb_frame.raw (the same file x11_mirror.c blits,
 * already containing the real "[>] N. Interact Mode..." chrome text),
 * and forward EVERY real key/click into board-viewer's own real
 * relay files (keyboard/history.txt, player_app/history.txt,
 * player_app/state.txt's last_click_x/y) via a direct, deliberate port
 * of x11_mirror.c's own append_key()/write_click_kv()/map_special_key()
 * - zero local nav logic of this file's own, matching x11_mirror.c's
 * own real "steal everything, reimplement nothing" shape exactly.
 * board-viewer/button.sh's own NO_RGB_COMPOSITOR/NO_GL split (same
 * date) is what makes rgb_frame.raw available here with no real GL
 * window of board-viewer's own ever needing to map. */
#define PCHQ_ARROW_LEFT  1000
#define PCHQ_ARROW_RIGHT 1001
#define PCHQ_ARROW_UP    1002
#define PCHQ_ARROW_DOWN  1003

static int pchq_map_special_key(KeySym ks) {
    if (ks == XK_Left) return PCHQ_ARROW_LEFT;
    if (ks == XK_Right) return PCHQ_ARROW_RIGHT;
    if (ks == XK_Up) return PCHQ_ARROW_UP;
    if (ks == XK_Down) return PCHQ_ARROW_DOWN;
    return 0;
}

/* Direct port of x11_mirror.c's own append_key() - dual-write, same
 * real target files, same real format. */
static void pchq_append_key(const char *history1, const char *history2, int key) {
    FILE *f = fopen(history1, "a");
    if (f) { fprintf(f, "%d\n", key); fclose(f); }
    FILE *cf = fopen(history2, "a");
    if (cf) { fprintf(cf, "KEY_PRESSED: %d\n", key); fclose(cf); }
}

/* Direct port of x11_mirror.c's own write_click_kv() - real read-
 * modify-write of board-viewer's own real player_app/state.txt. */
static void pchq_write_click_kv(const char *bv_session, const char *key, int value) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/state.txt", bv_session);
    char lines[128][512];
    int n = 0, replaced = 0;
    FILE *rf = fopen(path, "r");
    if (rf) {
        char line[512];
        size_t klen = strlen(key);
        while (n < 128 && fgets(line, sizeof(line), rf)) {
            if (strncmp(line, key, klen) == 0 && line[klen] == '=') {
                snprintf(lines[n], sizeof(lines[0]), "%s=%d\n", key, value);
                replaced = 1;
            } else {
                snprintf(lines[n], sizeof(lines[0]), "%s", line);
            }
            n++;
        }
        fclose(rf);
    }
    FILE *wf = fopen(path, "w");
    if (!wf) return;
    for (int i = 0; i < n; i++) fputs(lines[i], wf);
    if (!replaced) fprintf(wf, "%s=%d\n", key, value);
    fclose(wf);
}

static int run_pchq_board_mode(const char *house_root, const char *host_project_id) {
    char bv_session[PATH_BUF] = "";
    if (!pchq_find_board_session(house_root, host_project_id, bv_session, sizeof(bv_session))) {
        fprintf(stderr, "run_pchq_board_mode: no live board-viewer session found for %s "
                        "(open View Board from the game first)\n", host_project_id);
        return 1;
    }
    pchq_kill_legacy_display(bv_session);

    /* REAL ARCHITECTURE REWRITE (2026-08-30, direct instruction: "isn't
     * it only that actual 2d/3d screen {interact screen} needs to be
     * blitted? ... everything else can be just a typical hq window...
     * thats the architecture we should have been using not some hybrid
     * disorganized blitted legacy newfangled setup").
     *
     * Previous shape (restyle pass, commit 562eb172 onward) blitted the
     * legacy engine's ENTIRE current_frame.txt (chrome text, toolbar
     * buttons, status info, AND the 3D view) as one classified text
     * stream, then tried to bolt real khtpm-style nav-badge highlight
     * on top of it. That's the direct cause of two real, reported bugs:
     * multiple buttons packed onto one text LINE all shared a single
     * whole-line highlight (no per-badge granularity), and a real
     * numbered close button living inside blitted content could never
     * be unified with this window's own separately hand-drawn chrome
     * [X] the way db-hq/chat-hai/events-hq keep ONE real close Elem in
     * their own title strip.
     *
     * New shape: ONLY the real 2D/3D view (pieces/display/
     * rgb_frame_3d_overlay.raw - a real, project-agnostic RGBA canvas,
     * see &.widgits/board-viewer/ops/bv_render_3d.c's own header) is a
     * blit, treated exactly like an <img>/<canvas> element would be.
     * Everything else - title bar, Close, File, Desk, the Interact Mode
     * toggle - is a real local Elem with its own nav_index, drawn with
     * the SAME real "#ff8c00 focused / #888888 unfocused" bordered-box
     * convention db-hq's own g_dbhq_close_elem uses (see
     * dbhq_draw_chrome_bar() for the reference this was modeled on).
     * current_frame.txt is no longer read AT ALL - board_viewer.chtpm
     * itself went back to being ONLY the real "Interact Mode" button
     * (see that file's own header comment), since camera/selector
     * navigation while genuinely inside real Interact Mode is the ONE
     * piece of UI that must stay legacy-engine-owned (that's what
     * "absolute parity" was about) - everything else here is real,
     * local, khtpm-native UI, not a hybrid.
     *
     * The one real subtlety: this window's own "Interact Mode" Elem
     * can't just write hero_01/state.txt's interact_mode flag directly
     * - chtpm_parser_pal.c's own onClick="INTERACT" handling
     * (set_interact_mode() + export_active_index()) also updates that
     * RUNNING process's own in-memory active_index/focus_index, which
     * is what actually gates real arrow-key-to-camera relay - a raw
     * file write from a separate process would get silently
     * overwritten by the engine's own next render pass. Real, zero-
     * reimplementation fix: forward a synthetic click at the fixed
     * real screen position board_viewer.chtpm's own (now sole) button
     * always renders at (row 0) via the SAME pchq_write_click_kv()
     * mechanism already used for real in-canvas clicks - the legacy
     * engine's own native click-hit-testing does the real toggle, this
     * file does zero reimplementation of it. */
    char overlay_path[PATH_BUF], overlay_receipt_path[PATH_BUF];
    snprintf(overlay_path, sizeof(overlay_path), "%s/pieces/display/rgb_frame_3d_overlay.raw", bv_session);
    snprintf(overlay_receipt_path, sizeof(overlay_receipt_path), "%s/pieces/display/rgb_frame_3d_overlay.receipt.txt", bv_session);

    char bv_history1[PATH_BUF], bv_history2[PATH_BUF];
    snprintf(bv_history1, sizeof(bv_history1), "%s/pieces/apps/player_app/history.txt", bv_session);
    snprintf(bv_history2, sizeof(bv_history2), "%s/pieces/keyboard/history.txt", bv_session);

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "run_pchq_board_mode: cannot open display\n"); return 1; }
    /* REAL FIX 2026-08-30 - this mode returns before main()'s own
     * XSetErrorHandler(evhq_nonfatal_x_error) call, so an
     * XSetInputFocus() landing before the WM finishes reparenting a
     * freshly WM-managed window throws an uncaught BadMatch and crashes
     * the whole process (confirmed live). Same real non-fatal handler
     * already used elsewhere in this file. */
    XSetErrorHandler(evhq_nonfatal_x_error);
    int screen = DefaultScreen(dpy);
    Visual *visual = DefaultVisual(dpy, screen);
    int depth = DefaultDepth(dpy, screen);
    Colormap cmap = DefaultColormap(dpy, screen);

#define PCHQ_TOOLBAR_H 28
#define PCHQ_CLOSE_W 74
#define PCHQ_FULLSCREEN_W 56
#define PCHQ_DROPDOWN_ROW_H 22
    int canvas_w = 640, canvas_h = 480; /* real defaults, resized from the overlay's own receipt below */
    int win_x = 140, win_y = 90;
    int dragging = 0, drag_last_x = 0, drag_last_y = 0;
    int win_w = canvas_w, win_h = CHROME_H + PCHQ_TOOLBAR_H + canvas_h;

    /* REAL FIX 2026-08-30, direct live report ("its not geting mouse /
     * kbd input") - override_redirect windows never get real keyboard/
     * mouse focus routed by Mutter (synthetic XTest input worked,
     * masking the bug) - normal WM-managed window, decorations
     * stripped via _MOTIF_WM_HINTS instead, same real shape
     * x11_mirror.c itself uses. */
    Window win = XCreateSimpleWindow(dpy, RootWindow(dpy, screen), win_x, win_y,
                                      (unsigned)win_w, (unsigned)win_h, 0,
                                      BlackPixel(dpy, screen), pchq_alloc_pixel(dpy, cmap, "#1c1c1c"));
    {
        Atom motif_hints = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
        long hints[5] = { 2, 0, 0, 0, 0 }; /* flags=MWM_HINTS_DECORATIONS, decorations=0 */
        XChangeProperty(dpy, win, motif_hints, motif_hints, 32, PropModeReplace, (unsigned char *)hints, 5);
    }
    XStoreName(dpy, win, "Piececraft-HQ Board (khtpm)");
    Atom pchq_wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &pchq_wm_delete, 1);
    XSelectInput(dpy, win, ExposureMask | ButtonPressMask | ButtonReleaseMask | ButtonMotionMask | KeyPressMask | StructureNotifyMask);
    {
        XSizeHints *shints = XAllocSizeHints();
        /* REAL FIX 2026-08-30, direct live report (fullscreen toggled
         * off per _NET_WM_STATE, confirmed via xprop, but the window
         * never actually shrank back down) - PPosition alone gives the
         * WM no real "normal" size to restore to after leaving
         * fullscreen. Real fix: also advertise PSize with this
         * window's own actual current size. */
        if (shints) { shints->flags = PPosition | PSize; shints->x = win_x; shints->y = win_y; shints->width = win_w; shints->height = win_h; XSetWMNormalHints(dpy, win, shints); XFree(shints); }
    }
    XMapRaised(dpy, win);
    set_window_opacity(dpy, win, load_theme_opacity());
    GC gc = XCreateGC(dpy, win, 0, NULL);
    for (int attempt = 0; attempt < 5; attempt++) {
        XSetInputFocus(dpy, win, RevertToParent, CurrentTime);
        XSync(dpy, False);
        Window focused; int revert;
        XGetInputFocus(dpy, &focused, &revert);
        if (focused == win) break;
        usleep(5000);
    }

    Pixmap buf = XCreatePixmap(dpy, win, (unsigned)win_w, (unsigned)win_h, (unsigned)depth);
    XftDraw *xftdraw = XftDrawCreate(dpy, buf, visual, cmap);
    XftFont *ui_font = XftFontOpenName(dpy, screen, "Ubuntu-10");
    XftFont *pchq_body_font = XftFontOpenName(dpy, screen, "DejaVu Sans Mono:pixelsize=13");
    if (!pchq_body_font) pchq_body_font = ui_font;

    XImage *ov_img = NULL;
    unsigned char *ov_buf = NULL;
    int ov_w_cur = 0, ov_h_cur = 0;

    /* Real, cached colors/pixels - allocated once, not per-frame (same
     * real perf fix as before - a colormap round trip per element per
     * frame at 30fps was measurably laggy). */
    unsigned long pix_chrome = pchq_alloc_pixel(dpy, cmap, "#2a2a2a");
    /* REAL FIX 2026-08-30, direct live report ("x doesn't need to be
     * 'red' its distracting") - matches Fullscreen's own neutral
     * chrome-strip-icon treatment now (pix_chrome), not a special
     * warning color - the orange focus border is still real feedback
     * when it's actually focused, same as every other elem. */
    unsigned long pix_close = pchq_alloc_pixel(dpy, cmap, "#2a2a2a");
    unsigned long pix_bg = pchq_alloc_pixel(dpy, cmap, "#111111");
    unsigned long pix_focus_fill = pchq_alloc_pixel(dpy, cmap, "#3a2a10");
    unsigned long pix_focus_border = pchq_alloc_pixel(dpy, cmap, "#ff8c00");
    unsigned long pix_unfocus_fill = pchq_alloc_pixel(dpy, cmap, "#2a2a2a");
    unsigned long pix_unfocus_border = pchq_alloc_pixel(dpy, cmap, "#555555");
    XftColor col_title, col_focus, col_unfocus;
    { XRenderColor rc = {0xeeee, 0xeeee, 0xeeee, 0xffff}; XftColorAllocValue(dpy, visual, cmap, &rc, &col_title); }
    { XRenderColor rc = {0xffff, 0x8c8c, 0x0000, 0xffff}; XftColorAllocValue(dpy, visual, cmap, &rc, &col_focus); }
    { XRenderColor rc = {0xaaaa, 0xaaaa, 0xaaaa, 0xffff}; XftColorAllocValue(dpy, visual, cmap, &rc, &col_unfocus); }

    /* REAL, local Elem set - File/Desk/Interact Mode/Close, same real
     * "#ff8c00 focused / #888888 unfocused" bordered-box convention
     * every other khtpm window uses. nav_index order matches on-screen
     * left-to-right/chrome-position order; Close is last on purpose
     * (matches db-hq's own convention - a fresh window never opens
     * with Close already focused). Positions recomputed each frame
     * below (win_w can change with the overlay's own real size). */
    /* REAL REORDER 2026-08-30, direct instruction ("interact mode
     * should be #1, then menu, then file, then desk" -> clarified to
     * "1.in 2.file 3.desk 4.menu 5.db 6.x" -> "hold off on db, put it
     * as a sub under menu") - real order is now In, File, Desk, Menu,
     * X. "Menu" is a real, new, currently-stub toolbar Elem (a general
     * game menu; Db will live as a sub-item under it later, not its own
     * top-level slot) - present now for layout/nav parity, no
     * dispatch yet. Later: Menu should open as a real dropdown (same
     * real pattern the taskbar's own menus already use), with Db as
     * one of its rows - not implemented yet, this is just the stub
     * slot reserved for it. */
    /* REAL, NEW 2026-08-30, direct instruction ("fullscreen, player and
     * clock" - roadmap items from aug-30-retro.md's own "Next-steps"
     * section) - Player/Clock join the toolbar row right after Menu
     * (per direct instruction: "we will probably just add player and
     * clock tb after menu"); Fullscreen ("!") joins Close in the chrome
     * strip (per direct instruction: "we will put '!' for fullscreen
     * next to 'x'"). Clock shows the real current time (cheap,
     * deterministic, no reason to stub it); Player is a real stub for
     * now (hero HP/position readback is a separate, later pass - not
     * blocking this layout work). */
    typedef struct { char label[24]; int x, y, w, h; int action; } PchqElem;
    enum { PCHQ_ACT_INTERACT = 0, PCHQ_ACT_FILE, PCHQ_ACT_DESK, PCHQ_ACT_MENU, PCHQ_ACT_PLAYER, PCHQ_ACT_CLOCK, PCHQ_ACT_FULLSCREEN, PCHQ_ACT_CLOSE, PCHQ_N_ELEMS };
    PchqElem elems[PCHQ_N_ELEMS];
    snprintf(elems[PCHQ_ACT_INTERACT].label, sizeof(elems[0].label), "In");
    snprintf(elems[PCHQ_ACT_FILE].label, sizeof(elems[0].label), "File");
    snprintf(elems[PCHQ_ACT_DESK].label, sizeof(elems[0].label), "Desk");
    snprintf(elems[PCHQ_ACT_MENU].label, sizeof(elems[0].label), "Menu");
    snprintf(elems[PCHQ_ACT_PLAYER].label, sizeof(elems[0].label), "Player");
    snprintf(elems[PCHQ_ACT_CLOCK].label, sizeof(elems[0].label), "Clock");
    snprintf(elems[PCHQ_ACT_FULLSCREEN].label, sizeof(elems[0].label), "!");
    snprintf(elems[PCHQ_ACT_CLOSE].label, sizeof(elems[0].label), "X");
    for (int i = 0; i < PCHQ_N_ELEMS; i++) elems[i].action = i;
    int pchq_focus = PCHQ_ACT_INTERACT;
    int pchq_is_fullscreen = 0;
    int pchq_opacity_reapplied = 0;

    /* REAL, NEW 2026-08-30 - File/Desk real dropdowns, so switching
     * levels/boards is a real, visible pick instead of a blind cycle -
     * direct instruction: "thats how we will prove save load
     * projects... i haven't seen save load from file in pc yet".
     * pchq_dropdown: 0=closed, 1=File open, 2=Desk open.
     * File has exactly 2 real states today (default-pdl/default-
     * legacy, per pc_menu_input.c's own FILE_MENU handler) - picking
     * the non-current one sends the same real cycle key that already
     * works, just through a real visible list instead of blind
     * toggling. Desk has exactly 1 real board today (confirmed by
     * direct read of defaults/default-pdl/default.pdl) - still real
     * infrastructure, ready for when more boards exist, not
     * fabricated content. */
    int pchq_dropdown = 0;
    int pchq_dropdown_focus = 0;
    char pchq_active_level[64] = "";
    char pchq_active_board[64] = "";

    int running = 1;
    int pchq_focus_ok = 0;
    while (running) {
        /* REAL FIX 2026-08-30, direct live report ("screen flashes and
         * is throttling. is the renderer cpu safe?") - this loop had NO
         * real frame cap at all (confirmed live: ps aux showed it
         * pinned at ~75-80% CPU, state Rs - genuinely spinning, not
         * blocked/idle waiting on anything). Every other khtpm loop in
         * this file (db-hq's, the strip's) has a real usleep() per
         * iteration; this one was missed in the architecture rewrite.
         * 16ms ~= 60fps, same real target the overlay's own raymarch
         * producer runs at - matches, doesn't starve, doesn't spin. */
        usleep(16000);
        if (!pchq_focus_ok) {
            XSetInputFocus(dpy, win, RevertToParent, CurrentTime);
            XSync(dpy, False);
            Window focused; int revert;
            XGetInputFocus(dpy, &focused, &revert);
            if (focused == win) pchq_focus_ok = 1;
        }

        int ov_w = pchq_read_kv_int(overlay_receipt_path, "overlay_w", 0);
        int ov_h = pchq_read_kv_int(overlay_receipt_path, "overlay_h", 0);
        if (ov_w > 0 && ov_h > 0 && (ov_w != ov_w_cur || ov_h != ov_h_cur || !ov_img)) {
            ov_w_cur = ov_w; ov_h_cur = ov_h;
            canvas_w = ov_w; canvas_h = ov_h;
            free(ov_buf);
            ov_buf = malloc((size_t)ov_w * ov_h * 4);
            if (ov_img) { XDestroyImage(ov_img); ov_img = NULL; }
            char *data = malloc((size_t)ov_w * ov_h * 4);
            ov_img = XCreateImage(dpy, visual, (unsigned)depth, ZPixmap, 0, data,
                                   (unsigned)ov_w, (unsigned)ov_h, 32, 0);
            int new_win_w = canvas_w, new_win_h = CHROME_H + PCHQ_TOOLBAR_H + canvas_h;
            if (new_win_w != win_w || new_win_h != win_h) {
                win_w = new_win_w; win_h = new_win_h;
                XResizeWindow(dpy, win, (unsigned)win_w, (unsigned)win_h);
                XFreePixmap(dpy, buf);
                buf = XCreatePixmap(dpy, win, (unsigned)win_w, (unsigned)win_h, (unsigned)depth);
                XftDrawDestroy(xftdraw);
                xftdraw = XftDrawCreate(dpy, buf, visual, cmap);
            }
        }
        if (ov_img && ov_buf) {
            FILE *of = fopen(overlay_path, "rb");
            if (of) {
                size_t got = fread(ov_buf, 1, (size_t)ov_w_cur * ov_h_cur * 4, of);
                fclose(of);
                if (got == (size_t)ov_w_cur * ov_h_cur * 4) {
                    for (int y = 0; y < ov_h_cur; y++)
                        for (int x = 0; x < ov_w_cur; x++) {
                            size_t o = ((size_t)y * ov_w_cur + x) * 4;
                            unsigned long px = ((unsigned long)ov_buf[o] << 16)
                                              | ((unsigned long)ov_buf[o + 1] << 8)
                                              | (unsigned long)ov_buf[o + 2];
                            XPutPixel(ov_img, x, y, px);
                        }
                }
            }
        }

        /* REAL, NEW 2026-08-30 (!.HOUSE_STDS.md §A.9) - the one real
         * signal that decides which side of the engine's own
         * active_index==-1 boundary this window is on right now. Read
         * ONCE per frame - used both for the status label below AND
         * for routing keyboard input in this same iteration's event
         * loop (a real live report confirmed arrows/digits must move
         * THIS toolbar's own focus while off, and forward unconditionally
         * to the game once on - "everything is normal till in interact"). */
        int pchq_interact_on = pchq_is_interact_on(bv_session);
        /* Real, live active_level/active_board readback for the File/
         * Desk dropdowns - only bothered with while a dropdown is
         * actually open, to avoid a pointless file read every frame
         * the rest of the time. */
        if (pchq_dropdown) {
            pchq_read_config_kv(house_root, host_project_id, "active_level", pchq_active_level, sizeof(pchq_active_level));
            pchq_read_config_kv(house_root, host_project_id, "active_board", pchq_active_board, sizeof(pchq_active_board));
        }

        /* Real title chrome - just a title, Close now lives here as a
         * real Elem (see below), not a separate hand-drawn duplicate. */
        XSetForeground(dpy, gc, pix_chrome);
        XFillRectangle(dpy, buf, gc, 0, 0, (unsigned)win_w, CHROME_H);
        if (ui_font) {
            const char *title = "Piececraft-HQ Board (khtpm)";
            XftDrawStringUtf8(xftdraw, &col_title, ui_font, 8, 18, (const FcChar8 *)title, (int)strlen(title));
        }

        /* Real toolbar row - File/Desk/Interact Mode, left to right. */
        XSetForeground(dpy, gc, pix_chrome);
        XFillRectangle(dpy, buf, gc, 0, CHROME_H, (unsigned)win_w, PCHQ_TOOLBAR_H);

        /* Real order left to right, per direct instruction: In, File,
         * Desk, Menu, then X in the chrome strip far right - spaced out
         * (6px gaps) same as before. "In" is shortened from "Interact
         * Mode" so its own box doesn't have to be the widest one. */
        /* REAL FIX 2026-08-30, direct live report ("4.x is going off
         * the right of the header a bit") - flush against win_w left
         * zero margin for text to render into; a real gap keeps the
         * badge text fully inside the visible window. */
        /* REAL, NEW 2026-08-30 - Fullscreen ("!") joins Close in the
         * chrome strip, immediately to its left (direct instruction:
         * "we will put '!' for fullscreen next to 'x'"). Player/Clock
         * join the toolbar row after Menu (direct instruction: "we
         * will probably just add player and clock tb after menu") -
         * widths trimmed slightly across the board so all six toolbar
         * boxes still fit inside the real canvas width without
         * overflowing/clipping. */
        elems[PCHQ_ACT_CLOSE].x = win_w - PCHQ_CLOSE_W - 6; elems[PCHQ_ACT_CLOSE].y = 0;
        elems[PCHQ_ACT_CLOSE].w = PCHQ_CLOSE_W; elems[PCHQ_ACT_CLOSE].h = CHROME_H;
        elems[PCHQ_ACT_FULLSCREEN].x = elems[PCHQ_ACT_CLOSE].x - PCHQ_FULLSCREEN_W - 4; elems[PCHQ_ACT_FULLSCREEN].y = 0;
        elems[PCHQ_ACT_FULLSCREEN].w = PCHQ_FULLSCREEN_W; elems[PCHQ_ACT_FULLSCREEN].h = CHROME_H;
        elems[PCHQ_ACT_INTERACT].x = 6; elems[PCHQ_ACT_INTERACT].y = CHROME_H + 2;
        elems[PCHQ_ACT_INTERACT].w = 95; elems[PCHQ_ACT_INTERACT].h = PCHQ_TOOLBAR_H - 4;
        elems[PCHQ_ACT_FILE].x = elems[PCHQ_ACT_INTERACT].x + elems[PCHQ_ACT_INTERACT].w + 5; elems[PCHQ_ACT_FILE].y = CHROME_H + 2;
        elems[PCHQ_ACT_FILE].w = 78; elems[PCHQ_ACT_FILE].h = PCHQ_TOOLBAR_H - 4;
        elems[PCHQ_ACT_DESK].x = elems[PCHQ_ACT_FILE].x + elems[PCHQ_ACT_FILE].w + 5; elems[PCHQ_ACT_DESK].y = CHROME_H + 2;
        elems[PCHQ_ACT_DESK].w = 78; elems[PCHQ_ACT_DESK].h = PCHQ_TOOLBAR_H - 4;
        elems[PCHQ_ACT_MENU].x = elems[PCHQ_ACT_DESK].x + elems[PCHQ_ACT_DESK].w + 5; elems[PCHQ_ACT_MENU].y = CHROME_H + 2;
        elems[PCHQ_ACT_MENU].w = 82; elems[PCHQ_ACT_MENU].h = PCHQ_TOOLBAR_H - 4;
        elems[PCHQ_ACT_PLAYER].x = elems[PCHQ_ACT_MENU].x + elems[PCHQ_ACT_MENU].w + 5; elems[PCHQ_ACT_PLAYER].y = CHROME_H + 2;
        elems[PCHQ_ACT_PLAYER].w = 92; elems[PCHQ_ACT_PLAYER].h = PCHQ_TOOLBAR_H - 4;
        elems[PCHQ_ACT_CLOCK].x = elems[PCHQ_ACT_PLAYER].x + elems[PCHQ_ACT_PLAYER].w + 5; elems[PCHQ_ACT_CLOCK].y = CHROME_H + 2;
        elems[PCHQ_ACT_CLOCK].w = 84; elems[PCHQ_ACT_CLOCK].h = PCHQ_TOOLBAR_H - 4;

        for (int i = 0; i < PCHQ_N_ELEMS; i++) {
            int focused = (i == pchq_focus);
            if (i == PCHQ_ACT_CLOSE) {
                /* REAL FIX 2026-08-30, direct live report ("x still
                 * dont' have index nav (close) why? isn't it using a
                 * similar layout system now?") - Close is a real Elem
                 * in the SAME elems[] array/nav_index sequence as
                 * File/Desk/Interact (arrow/digit-nav already reaches
                 * it, confirmed), but its own draw branch never got the
                 * same real "[>]N."/"[ ]N." badge the other three
                 * elems' draw branch has - a real omission, not a
                 * structural difference. Widened PCHQ_CLOSE_W to fit
                 * the badge text. */
                XSetForeground(dpy, gc, pix_close);
                XFillRectangle(dpy, buf, gc, elems[i].x, elems[i].y, (unsigned)elems[i].w, (unsigned)elems[i].h);
                if (focused) {
                    XSetForeground(dpy, gc, pix_focus_border);
                    XDrawRectangle(dpy, buf, gc, elems[i].x, elems[i].y, (unsigned)elems[i].w - 1, (unsigned)elems[i].h - 1);
                }
                if (ui_font) {
                    char close_label[16];
                    snprintf(close_label, sizeof(close_label), "%s%d. X", focused ? "[>]" : "[ ]", PCHQ_ACT_CLOSE + 1);
                    XftDrawStringUtf8(xftdraw, &col_title, ui_font, elems[i].x + 4, 18, (const FcChar8 *)close_label, (int)strlen(close_label));
                }
                continue;
            }
            if (i == PCHQ_ACT_FULLSCREEN) {
                /* Same real chrome-strip-icon treatment as Close - a
                 * short glyph, not a normal toolbar box, matching
                 * direct instruction ("'!' for fullscreen next to
                 * 'x'"). */
                XSetForeground(dpy, gc, pix_chrome);
                XFillRectangle(dpy, buf, gc, elems[i].x, elems[i].y, (unsigned)elems[i].w, (unsigned)elems[i].h);
                if (focused) {
                    XSetForeground(dpy, gc, pix_focus_border);
                    XDrawRectangle(dpy, buf, gc, elems[i].x, elems[i].y, (unsigned)elems[i].w - 1, (unsigned)elems[i].h - 1);
                }
                if (ui_font) {
                    char fs_label[16];
                    snprintf(fs_label, sizeof(fs_label), "%s%d.!", focused ? "[>]" : "[ ]", PCHQ_ACT_FULLSCREEN + 1);
                    XftDrawStringUtf8(xftdraw, pchq_is_fullscreen ? &col_focus : &col_title, ui_font,
                                       elems[i].x + 4, 18, (const FcChar8 *)fs_label, (int)strlen(fs_label));
                }
                continue;
            }
            XSetForeground(dpy, gc, focused ? pix_focus_fill : pix_unfocus_fill);
            XFillRectangle(dpy, buf, gc, elems[i].x, elems[i].y, (unsigned)elems[i].w, (unsigned)elems[i].h);
            XSetForeground(dpy, gc, focused ? pix_focus_border : pix_unfocus_border);
            XDrawRectangle(dpy, buf, gc, elems[i].x, elems[i].y, (unsigned)elems[i].w - 1, (unsigned)elems[i].h - 1);
            if (pchq_body_font) {
                /* REAL FIX 2026-08-30, direct live report ("its missing
                 * all the index, nav bracket [] features why?") - the
                 * bordered-box highlight alone dropped the real "[>]N."/
                 * "[ ]N." nav-badge convention every other numbered row
                 * in this house shows. Added back as a real visual
                 * prefix - genuinely can't be digit-jump-activated
                 * (1/2/3 are legitimately reserved for real camera-mode
                 * switching, checked directly in bv_menu_input.c), but
                 * the badge itself is still real, informational, and
                 * consistent - Tab/click remain the real activation
                 * path, same as this window's own documented reason for
                 * not overloading those digits. */
                char label[48];
                char badge[8];
                /* REAL FIX 2026-08-30, direct live report ("its not
                 * changing '>' to '^' to signal its using interact
                 * mode") - the house-wide [>]/[^]/[ ] glyph convention
                 * (!.HOUSE_STDS.md §A.5) means [^] = genuinely ACTIVE/
                 * ENGAGED, which takes priority over plain focus - the
                 * real engine's own render_element() does exactly this
                 * glyph swap for its own active_index element. Mirror
                 * it here using the same real pchq_interact_on signal
                 * already resolved once this frame. */
                int is_engaged = (i == PCHQ_ACT_INTERACT) && pchq_interact_on;
                snprintf(badge, sizeof(badge), "%s%d.", is_engaged ? "[^]" : (focused ? "[>]" : "[ ]"), i + 1);
                if (i == PCHQ_ACT_INTERACT) {
                    /* Real status readback - not a separate blitted text
                     * dump, a real, small local label reading the SAME
                     * real active_gui_is_typing.txt flag the legacy
                     * engine itself writes on real activation
                     * (pchq_interact_on, already resolved once this
                     * frame above). */
                    snprintf(label, sizeof(label), "%s In: %s", badge, pchq_interact_on ? "ON" : "off");
                } else if (i == PCHQ_ACT_CLOCK) {
                    /* Real, live current time - cheap, deterministic,
                     * no reason to leave it a stub like Menu/Player. */
                    time_t now = time(NULL);
                    struct tm *tmv = localtime(&now);
                    char tbuf[16];
                    if (tmv) strftime(tbuf, sizeof(tbuf), "%H:%M", tmv); else snprintf(tbuf, sizeof(tbuf), "--:--");
                    snprintf(label, sizeof(label), "%s %s", badge, tbuf);
                } else {
                    snprintf(label, sizeof(label), "%s %s", badge, elems[i].label);
                }
                XftDrawStringUtf8(xftdraw, focused ? &col_focus : &col_unfocus, pchq_body_font,
                                   elems[i].x + 6, elems[i].y + elems[i].h - 8, (const FcChar8 *)label, (int)strlen(label));
            }
        }

        /* Real content background + the ONLY real blit left - the pure
         * 2D/3D view itself, treated exactly like a canvas element. */
        XSetForeground(dpy, gc, pix_bg);
        XFillRectangle(dpy, buf, gc, 0, CHROME_H + PCHQ_TOOLBAR_H, (unsigned)win_w, (unsigned)canvas_h);
        if (ov_img)
            XPutImage(dpy, buf, gc, ov_img, 0, 0, 0, CHROME_H + PCHQ_TOOLBAR_H, (unsigned)ov_w_cur, (unsigned)ov_h_cur);

        /* Real File/Desk dropdown - drawn AFTER the content blit above
         * (real bug, caught live: drawing it BEFORE meant the content
         * canvas fill/blit - which starts at the SAME y as the
         * dropdown - painted straight over it every frame; state was
         * always correct, confirmed via debug print, only the paint
         * order was wrong) so it actually renders on top, real rows,
         * real current-state marker (see pchq_dropdown's own
         * declaration comment for the full real behavior). */
        if (pchq_dropdown) {
            int n_rows = (pchq_dropdown == 1) ? 2 : 1;
            int dropdown_x = elems[pchq_dropdown == 1 ? PCHQ_ACT_FILE : PCHQ_ACT_DESK].x;
            int dropdown_y = CHROME_H + PCHQ_TOOLBAR_H;
            int dropdown_w = 150;
            XSetForeground(dpy, gc, pix_unfocus_fill);
            XFillRectangle(dpy, buf, gc, dropdown_x, dropdown_y, (unsigned)dropdown_w, (unsigned)(n_rows * PCHQ_DROPDOWN_ROW_H));
            XSetForeground(dpy, gc, pix_unfocus_border);
            XDrawRectangle(dpy, buf, gc, dropdown_x, dropdown_y, (unsigned)dropdown_w - 1, (unsigned)(n_rows * PCHQ_DROPDOWN_ROW_H) - 1);
            for (int r = 0; r < n_rows; r++) {
                int row_y = dropdown_y + r * PCHQ_DROPDOWN_ROW_H;
                int row_focused = (r == pchq_dropdown_focus);
                int is_current;
                const char *row_label;
                if (pchq_dropdown == 1) {
                    int is_legacy = (strcmp(pchq_active_level, "default-legacy") == 0);
                    is_current = (r == (is_legacy ? 1 : 0));
                    row_label = (r == 0) ? "default-pdl" : "default-legacy";
                } else {
                    is_current = 1; /* the one real board is always the active one today */
                    row_label = pchq_active_board[0] ? pchq_active_board : "default";
                }
                if (row_focused) {
                    XSetForeground(dpy, gc, pix_focus_fill);
                    XFillRectangle(dpy, buf, gc, dropdown_x + 1, row_y + 1, (unsigned)dropdown_w - 2, (unsigned)PCHQ_DROPDOWN_ROW_H - 2);
                }
                if (pchq_body_font) {
                    char row_text[64];
                    snprintf(row_text, sizeof(row_text), "%s%s", is_current ? "* " : "  ", row_label);
                    XftDrawStringUtf8(xftdraw, row_focused ? &col_focus : &col_unfocus, pchq_body_font,
                                       dropdown_x + 6, row_y + PCHQ_DROPDOWN_ROW_H - 6, (const FcChar8 *)row_text, (int)strlen(row_text));
                }
            }
        }

        XCopyArea(dpy, buf, win, gc, 0, 0, (unsigned)win_w, (unsigned)win_h, 0, 0);
        XFlush(dpy);

        /* REAL FIX 2026-08-30, direct live report ("piececraft hq
         * window doesn't have the opacity at all yet") - the SAME real
         * bug 9ab1c199 already found+fixed for db-hq/events-hq/chat-
         * hai/popup/every desktop entity: Mutter/XWayland does not
         * reliably honor _NET_WM_WINDOW_OPACITY set at map-time, before
         * a real first paint - it must be re-applied once after the
         * window has actually been painted at least one real frame.
         * This mode's own set_window_opacity() call (right after
         * XMapRaised, above) never got this follow-up - confirmed live
         * via xprop that the property WAS set correctly but visually
         * never applied. Same real one-time-after-first-paint pattern
         * every other branch already uses. */
        if (!pchq_opacity_reapplied) {
            pchq_opacity_reapplied = 1;
            usleep(200000);
            set_window_opacity(dpy, win, load_theme_opacity());
            XFlush(dpy);
        }
        /* Real, event-driven live opacity reload - same cheap marker
         * convention as khtpm_strip_parser.c/tp_desktop_window_rgb.c's
         * own theme_changed_dirty() (dc759f3c). */
        if (pchq_theme_changed_dirty(house_root)) {
            set_window_opacity(dpy, win, load_theme_opacity());
        }

        struct timeval tv = {0, 33333}; /* same 30fps cap as x11_mirror.c's own real poll */
        fd_set fds; FD_ZERO(&fds); int xfd = ConnectionNumber(dpy); FD_SET(xfd, &fds);
        select(xfd + 1, &fds, NULL, NULL, &tv);
        while (XPending(dpy)) {
            XEvent ev; XNextEvent(dpy, &ev);
            if (ev.type == Expose) {
                XCopyArea(dpy, buf, win, gc, 0, 0, (unsigned)win_w, (unsigned)win_h, 0, 0);
                XFlush(dpy);
            } else if (ev.type == KeyPress) {
                char kbuf[8]; KeySym ks;
                int n = XLookupString(&ev.xkey, kbuf, sizeof(kbuf) - 1, &ks, NULL);
                /* REAL, NEW 2026-08-30 (!.HOUSE_STDS.md §A.9, direct
                 * live correction: "arrow keys work for the external
                 * nav items... when not in interact right. they still
                 * get nav index numbers. everything is normal till in
                 * interact. its very elegant") - this window's own
                 * mirror of the legacy engine's real active_index==-1
                 * dual-mode model. pchq_interact_on (resolved once this
                 * frame, above) is the one real signal deciding which
                 * side of that boundary we're on. */
                if (pchq_dropdown) {
                    /* Real dropdown mode - takes priority over normal
                     * toolbar nav while open (matches the taskbar's
                     * own popup-vs-header nav priority). Row count is
                     * fixed per dropdown kind: File=2 (default-pdl/
                     * default-legacy), Desk=1 (the one real board
                     * today). */
                    int n_rows = (pchq_dropdown == 1) ? 2 : 1;
                    if (ks == XK_Escape) {
                        pchq_dropdown = 0;
                    } else if (ks == XK_Up || ks == XK_Left) {
                        pchq_dropdown_focus = (pchq_dropdown_focus - 1 + n_rows) % n_rows;
                    } else if (ks == XK_Down || ks == XK_Right || ks == XK_Tab) {
                        pchq_dropdown_focus = (pchq_dropdown_focus + 1) % n_rows;
                    } else if (ks == XK_Return || ks == XK_KP_Enter) {
                        /* REAL FIX 2026-08-30, direct instruction ("can
                         * we just do w/e tb does? even reuse as op or
                         * something?") closing the real, confirmed gap
                         * (traced live): chtpm_parser_pal.c's own real
                         * process_key() only ever forwards a relayed key
                         * into interact_relay.txt (inject_raw_key())
                         * from its `else if (strcmp(el->onClick,
                         * "INTERACT") == 0)` branch - i.e. ONLY while
                         * active_index is genuinely on the INTERACT
                         * element (pchq_is_interact_on() true). This
                         * whole dropdown is only ever reachable from the
                         * `!pchq_interact_on` normal-nav branch above, so
                         * '5'/'6' landed on ordinary chtpm nav dispatch
                         * instead and never reached bv_menu_input.c at
                         * all - confirmed by direct trace, not assumed.
                         * Real, zero-reimplementation fix: reuse the
                         * EXACT SAME mechanism PCHQ_ACT_INTERACT's own
                         * activation already uses below (a plain Enter/
                         * ASCII 13 through this same real relay) to
                         * engage Interact Mode first, THEN send the real
                         * File/Desk key - same "op" the legacy engine
                         * already runs for every other key, nothing new
                         * built. */
                        if (!pchq_is_interact_on(bv_session))
                            pchq_append_key(bv_history1, bv_history2, 13);
                        if (pchq_dropdown == 1) {
                            /* Row 0 = default-pdl, row 1 = default-legacy
                             * (matches pc_menu_input.c's own FILE_MENU
                             * cycle order). Picking whichever ISN'T
                             * already active sends the same real cycle
                             * key that already works - two states, one
                             * cycle key, a real visible pick instead of
                             * a blind toggle. Picking the ALREADY-active
                             * one is a real no-op (matches "you're
                             * already here"). */
                            int is_legacy = (strcmp(pchq_active_level, "default-legacy") == 0);
                            int current_row = is_legacy ? 1 : 0;
                            if (pchq_dropdown_focus != current_row) pchq_append_key(bv_history1, bv_history2, '5');
                        } else {
                            /* Desk - the one real board, reload it
                             * (matches DESK_MENU's own real behavior). */
                            pchq_append_key(bv_history1, bv_history2, '6');
                        }
                        pchq_dropdown = 0;
                    }
                } else if (!pchq_interact_on) {
                    /* Normal nav mode - arrows move THIS toolbar's own
                     * focus, digits 1-4 jump-select the SAME real way
                     * the legacy engine's own numbered rows do
                     * (honoring the real house-wide g_click_two_step
                     * setting - already loaded before this mode's
                     * dispatch, see main()'s own dbhq_load_font_scale()
                     * call), Enter activates whatever's focused. None
                     * of this ever reaches the legacy relay - it can't
                     * collide with real gameplay keys because those
                     * only mean anything once genuinely engaged. */
                    int activate = 0;
                    if (ks == XK_Left || ks == XK_Up) {
                        pchq_focus = (pchq_focus - 1 + PCHQ_N_ELEMS) % PCHQ_N_ELEMS;
                    } else if (ks == XK_Right || ks == XK_Down || ks == XK_Tab) {
                        pchq_focus = (pchq_focus + 1) % PCHQ_N_ELEMS;
                    } else if (n > 0 && kbuf[0] >= '1' && kbuf[0] < '1' + PCHQ_N_ELEMS) {
                        int target = kbuf[0] - '1';
                        if (g_click_two_step && target != pchq_focus) pchq_focus = target;
                        else { pchq_focus = target; activate = 1; }
                    } else if (ks == XK_Return || ks == XK_KP_Enter) {
                        activate = 1;
                    }
                    if (activate) {
                        if (pchq_focus == PCHQ_ACT_FILE) {
                            /* REAL, NEW 2026-08-30 - open a real
                             * dropdown instead of blind-cycling (see
                             * pchq_dropdown's own declaration comment
                             * above). */
                            pchq_dropdown = 1; pchq_dropdown_focus = 0;
                        } else if (pchq_focus == PCHQ_ACT_DESK) {
                            pchq_dropdown = 2; pchq_dropdown_focus = 0;
                        } else if (pchq_focus == PCHQ_ACT_MENU) {
                            /* Real stub - Menu has no dispatch yet (see
                             * its own declaration comment above). */
                        } else if (pchq_focus == PCHQ_ACT_PLAYER) {
                            /* Real stub - hero HP/position readback is
                             * a separate, later pass. */
                        } else if (pchq_focus == PCHQ_ACT_CLOCK) {
                            /* Clock is a real, live, read-only display -
                             * nothing to activate. */
                        } else if (pchq_focus == PCHQ_ACT_FULLSCREEN) {
                            pchq_toggle_fullscreen(dpy, win);
                            pchq_is_fullscreen = !pchq_is_fullscreen;
                        } else if (pchq_focus == PCHQ_ACT_INTERACT) {
                            /* REAL BUG FOUND + FIXED LIVE (2026-08-30) -
                             * a synthetic click via last_click_x/y does
                             * NOT reach the legacy engine's own button
                             * click-hit-testing at all - that convention
                             * (ported from x11_mirror.c) is consumed by
                             * board-viewer's own GAME logic (xelector/
                             * possess clicks), a completely separate
                             * real mechanism from chtpm_parser_pal's own
                             * UI activation. The REAL, zero-
                             * reimplementation way to activate a focused
                             * onClick="INTERACT" button from outside the
                             * engine's own process is a plain Enter
                             * keypress (ASCII 13) through the SAME real
                             * relay File/Desk already use - confirmed
                             * directly against the reference process_
                             * key()'s own Enter branch (!.HOUSE_STDS.md
                             * §A.9): "if (key==10||key==13...) { ...
                             * el=&elements[focus_index]; ... else if
                             * (onClick=='INTERACT') active_index=
                             * focus_index; ... }". board_viewer.chtpm's
                             * ONLY remaining element IS this button, so
                             * it's always the default focus - no digit-
                             * jump needed first. */
                            pchq_append_key(bv_history1, bv_history2, 13);
                        } else if (pchq_focus == PCHQ_ACT_CLOSE) {
                            pchq_quit_host_session(house_root, host_project_id);
                            running = 0;
                        }
                    }
                } else {
                    /* Engaged mode - keyboard is 100% game input now
                     * (direct confirmed answer: "Mouse click only while
                     * in interact mode"), forwarded unconditionally,
                     * same real shape x11_mirror.c's own KeyPress branch
                     * uses - includes Escape, which the legacy engine's
                     * own native ESC-exit consumes BEFORE this
                     * project's own ops ever see it (§A.9) - zero local
                     * interception needed here. */
                    if (n > 0) {
                        pchq_append_key(bv_history1, bv_history2, (int)(unsigned char)kbuf[0]);
                    } else {
                        int mapped = pchq_map_special_key(ks);
                        if (mapped > 0) pchq_append_key(bv_history1, bv_history2, mapped);
                    }
                }
            } else if (pchq_dropdown && ev.type == ButtonPress && ev.xbutton.button == Button1) {
                /* Real dropdown row click - see the KeyPress dropdown
                 * branch above for the real row-count/action shape;
                 * geometry mirrors the draw code below exactly
                 * (dropdown_x/y/w, PCHQ_DROPDOWN_ROW_H). */
                int n_rows = (pchq_dropdown == 1) ? 2 : 1;
                int dropdown_x = elems[pchq_dropdown == 1 ? PCHQ_ACT_FILE : PCHQ_ACT_DESK].x;
                int dropdown_y = CHROME_H + PCHQ_TOOLBAR_H;
                int dropdown_w = 150;
                int row = (ev.xbutton.x >= dropdown_x && ev.xbutton.x < dropdown_x + dropdown_w &&
                           ev.xbutton.y >= dropdown_y) ? (ev.xbutton.y - dropdown_y) / PCHQ_DROPDOWN_ROW_H : -1;
                if (row >= 0 && row < n_rows) {
                    /* Same real fix as the KeyPress dropdown branch above -
                     * engage Interact Mode first (reusing PCHQ_ACT_INTERACT's
                     * own real activation, a plain Enter/13 through this
                     * same relay) so chtpm_parser_pal.c's own real
                     * onClick=="INTERACT" gate is actually open before the
                     * File/Desk key is sent, or it never reaches
                     * bv_menu_input.c at all. */
                    if (!pchq_is_interact_on(bv_session))
                        pchq_append_key(bv_history1, bv_history2, 13);
                    if (pchq_dropdown == 1) {
                        int is_legacy = (strcmp(pchq_active_level, "default-legacy") == 0);
                        int current_row = is_legacy ? 1 : 0;
                        if (row != current_row) pchq_append_key(bv_history1, bv_history2, '5');
                    } else {
                        pchq_append_key(bv_history1, bv_history2, '6');
                    }
                }
                pchq_dropdown = 0;
            } else if (ev.type == ButtonPress && ev.xbutton.button == Button1) {
                int hit = -1;
                for (int i = 0; i < PCHQ_N_ELEMS; i++) {
                    if (ev.xbutton.x >= elems[i].x && ev.xbutton.x < elems[i].x + elems[i].w &&
                        ev.xbutton.y >= elems[i].y && ev.xbutton.y < elems[i].y + elems[i].h) { hit = i; break; }
                }
                if (hit >= 0) {
                    /* Real mouse click_two_step - same real convention
                     * click_focus_then_activate() uses house-wide (a
                     * click on an unfocused item selects it; a second
                     * click, or click_two_step=0, activates). Real
                     * mouse access to these elems ALWAYS works, even
                     * while genuinely engaged (direct confirmed answer:
                     * "Mouse click only while in interact mode"). */
                    int activate = (!g_click_two_step) || (pchq_focus == hit);
                    pchq_focus = hit;
                    if (activate) {
                        if (hit == PCHQ_ACT_FILE) {
                            pchq_dropdown = 1; pchq_dropdown_focus = 0;
                        } else if (hit == PCHQ_ACT_DESK) {
                            pchq_dropdown = 2; pchq_dropdown_focus = 0;
                        } else if (hit == PCHQ_ACT_MENU) {
                            /* Real stub - see declaration comment above. */
                        } else if (hit == PCHQ_ACT_PLAYER) {
                            /* Real stub - see declaration comment above. */
                        } else if (hit == PCHQ_ACT_CLOCK) {
                            /* Real, live, read-only display - nothing to
                             * activate. */
                        } else if (hit == PCHQ_ACT_FULLSCREEN) {
                            pchq_toggle_fullscreen(dpy, win);
                            pchq_is_fullscreen = !pchq_is_fullscreen;
                        } else if (hit == PCHQ_ACT_INTERACT) {
                            /* Same real fix as the keyboard path above -
                             * a plain Enter keypress through the relay,
                             * not a synthetic click. */
                            pchq_append_key(bv_history1, bv_history2, 13);
                        } else if (hit == PCHQ_ACT_CLOSE) {
                            /* REAL FIX 2026-08-30 - this mouse-click
                             * branch uses `hit`, not `pchq_focus` (the
                             * keyboard branch's own variable) - the
                             * earlier pchq_quit_host_session() fix
                             * (b1ef2cf0) only matched `pchq_focus ==
                             * PCHQ_ACT_CLOSE` text and silently never
                             * touched THIS branch at all, so a real
                             * mouse click on Close - confirmed live,
                             * the actual way this was being used - kept
                             * leaving the real game session running
                             * even after that fix. */
                            pchq_quit_host_session(house_root, host_project_id);
                            running = 0;
                        }
                    }
                } else if (ev.xbutton.y < CHROME_H) {
                    dragging = 1;
                    drag_last_x = ev.xbutton.x_root;
                    drag_last_y = ev.xbutton.y_root;
                } else if (ev.xbutton.y >= CHROME_H + PCHQ_TOOLBAR_H) {
                    /* Real click forwarded into the canvas - same real
                     * mechanism, offset now by chrome+toolbar height
                     * instead of just chrome. */
                    pchq_write_click_kv(bv_session, "last_click_x", ev.xbutton.x);
                    pchq_write_click_kv(bv_session, "last_click_y", ev.xbutton.y - CHROME_H - PCHQ_TOOLBAR_H);
                }
            } else if (ev.type == ButtonRelease && ev.xbutton.button == 1) {
                dragging = 0;
            } else if (ev.type == MotionNotify) {
                if (dragging) {
                    int dx = ev.xmotion.x_root - drag_last_x;
                    int dy = ev.xmotion.y_root - drag_last_y;
                    win_x += dx; win_y += dy;
                    XMoveWindow(dpy, win, win_x, win_y);
                    drag_last_x = ev.xmotion.x_root;
                    drag_last_y = ev.xmotion.y_root;
                }
            } else if (ev.type == ClientMessage && (Atom)ev.xclient.data.l[0] == pchq_wm_delete) {
                /* Real window-manager [X]/Alt+F4 close - same real quit
                 * signal as the in-toolbar Close Elem, so a WM-level
                 * close doesn't leave the underlying game session
                 * silently alive either. */
                pchq_quit_host_session(house_root, host_project_id);
                running = 0;
            }
        }
    }

    XDestroyWindow(dpy, win);
    XSync(dpy, False);
    XCloseDisplay(dpy);
    return 0;
}

/* ============================================================
 * REAL, NEW 2026-09-01 - taskbar strip mode, folded in verbatim from
 * khtpm_strip_parser.c + khtpm_strip_layout.c/.h + khtpm_strip_codes.h
 * (all three real, working, extensively live-debugged files - every
 * comment/fix-history below is preserved from the originals, not
 * rewritten). Consolidation per direct house-standard restatement this
 * session: no cross-.c linking OR #include-splicing to share behavior
 * within one binary - genuinely the same file, or a separate fork/
 * exec+file-IPC process. The strip's own two child concerns
 * (khtpm_taskbar_manager_main.+x, the pure-logic manager; the tile/pal
 * entity renderer) stay real, separate, fork/exec'd processes, exactly
 * as before - only the PARSER half (owns the only X11 Display in the
 * strip's own process pair) moves in here, as a new mode dispatched on
 * argc==2 (this mode's own real, exactly-2-arg invocation shape -
 * <house_root> only, no .chtpm path at all, unlike every other mode)
 * BEFORE the shared argc<3 check further down. See strip_main()'s own
 * renamed-from-main() comment.
 *
 * 5 real name collisions with the rest of this file, all resolved by
 * dropping the strip's own duplicate and reusing what already exists
 * here (confirmed equivalent in real meaning, not just by name):
 *   - g_house_root, g_click_two_step (both already read from the same
 *     real files this file's own copies already load from)
 *   - set_window_opacity()/load_theme_opacity() (byte-for-byte the
 *     same real _NET_WM_WINDOW_OPACITY/livedesk_theme.pdl mechanism)
 *   - main() -> renamed strip_main(), called from THIS file's own
 *     real main() when argc==2.
 * KTB_PATH_BUF/KTB_MAX_TABS/KTB_MAX_SHORTCUTS/KTB_BAR_H/
 * KTB_LIVEDESK_DYN_MAX below are real values copied from
 * khtpm_taskbar_manager.h (18352/64/16/36/24) - that header is no
 * longer included here per the same no-linking rule (it would drag in
 * the whole KtbState/tab/shortcut API this mode never uses), so the
 * 5 real constants this mode's own code actually needs are defined
 * directly instead.
 * ============================================================ */
#define KTB_PATH_BUF 4352
#define KTB_MAX_TABS 64
#define KTB_MAX_SHORTCUTS 16
#define KTB_BAR_H 36
#define KTB_LIVEDESK_DYN_MAX 24

/* ---- khtpm_strip_codes.h, verbatim ---- */
/* khtpm_strip_codes.h — shared decimal action-code protocol between
 * khtpm_strip_parser.c (writer, resolves clicks/keys locally) and
 * khtpm_taskbar_manager_main.c (reader, polls strip_history.txt and
 * dispatches into the existing ktb_* manager API).
 *
 * Per the design doc (khtpm-strip-parser-design.md §2 "Keys in"), the wire
 * format on strip_history.txt is CHTPM's own real convention: one bare
 * decimal integer per line ("%d\n"), no prefix. This header is the single
 * place both new binaries agree on what each integer means — not part of
 * either the manager's or the parser's "done" surface, just the small glue
 * contract between the two NEW files.
 *
 * Judgment call (not specified field-by-field in the design doc): CHTPM's
 * own history.txt carries raw X keycodes/ASCII, so low values (0-127) are
 * reserved for literal ASCII (digit chars '0'-'9' = 48-57, BackSpace = 8,
 * Return = 13, Escape = 27 — the same codes XLookupString/XLookupKeysym
 * already normalize to in khtpm_taskbar_plat_x11.c). Values >= 1000 are
 * strip-specific resolved actions (arrow-equivalents, tab clicks, shortcut
 * clicks, close) that have no natural single-byte ASCII code, using the
 * same "resolved action, not raw coordinates" shape the design's decisions
 * section calls for.
 */
#define KSC_BACKSPACE   8
#define KSC_ENTER       13
#define KSC_ESCAPE      27
/* digit codes: ASCII '0'..'9' == 48..57, pushed as-is via ktb_digit_push */

#define KSC_FOCUS_LEFT    1001
#define KSC_FOCUS_RIGHT   1002
#define KSC_CLOSE_QUIT    1003
/* Right-click "arm nav" (bug 1, 2026-08-11 live-test fix): mirrors
 * tp_taskbar.c's button==3 ButtonPress handling on both strip_win/win —
 * see ktb_nav_arm() in khtpm_taskbar_manager.h/.c. Sent instead of
 * KSC_TAB_BASE/KSC_HQ_HEADER_BASE when the click was a right-click. */
#define KSC_NAV_ARM       1004

/* Tab activate:   2000 + tab_idx   (idx in [0, KTB_MAX_TABS) ) */
#define KSC_TAB_BASE      2000
/* Shortcut run:   3000 + shortcut_idx (idx in [0, KTB_MAX_SHORTCUTS) ) */
#define KSC_SHORTCUT_BASE 3000
#define KSC_HQ_HEADER_BASE 4000
#define KSC_HQ_ITEM_BASE   5000

/* ---- khtpm_strip_layout.h, verbatim (types + prototypes) ---- */
#define LAY_MAX_ELEMENTS   256
#define LAY_MAX_CHILDREN   64
#define LAY_MAX_TOKENS     (LAY_MAX_ELEMENTS * 4)
#define LAY_TYPE_LEN       16
#define LAY_LABEL_LEN      512
#define LAY_ONCLICK_LEN    64
#define LAY_ATTR_LEN       64
#define LAY_TAGNAME_LEN    32
#define LAY_ATTRSTR_LEN    768
#define LAY_FILE_MAX       (64 * 1024)
#define LAY_SPRITE_LEN     512
#define LAY_ID_LEN         64

typedef struct {
    char type[LAY_TYPE_LEN];        /* "panel" | "text" | "button" | "row" | "cli_io" */
    char label[LAY_LABEL_LEN];      /* RAW, pre-substitution (may contain literal ${var}) */
    char onClick[LAY_ONCLICK_LEN];  /* RAW attribute, e.g. "ACTIVATE:3", "TAB:0", "STRIP:2", "HQITEM:1", "BACK" */
    char target_id[LAY_ATTR_LEN];   /* cli_io only */
    char sprite[LAY_SPRITE_LEN];    /* button only, optional — entity dir for tab_sprite(), "" = none */
    char id[LAY_ID_LEN];            /* button only, optional real stable identity — "" if absent */
    int  parent_index;              /* -1 = root */
    int  children[LAY_MAX_CHILDREN];
    int  num_children;
} LayElement;

typedef struct {
    LayElement elements[LAY_MAX_ELEMENTS];
    int element_count;
    int active_index;   /* -1 = no ACTIVATE scope open (matches chtpm_parser.c's active_index) */
    int focus_index;    /* cursor position among navigable elements */
} LayDoc;

typedef const char *(*LayVarLookupFn)(const char *name, void *ctx);

/* Forward decls - khtpm_strip_layout.h's own real prototypes (lay_load()
 * etc. call these before their definitions appear later in this same
 * merged block, exactly as they did across the header/.c split before
 * this consolidation). */
static int lay_load(LayDoc *doc, const char *path, LayVarLookupFn get_var, void *ctx);
static int lay_reload_preserving_scope(LayDoc *doc, const char *path, LayVarLookupFn get_var, void *ctx);
static int lay_is_interactive(const LayElement *el);
static int lay_is_navigable(const LayDoc *doc, int idx);
static int lay_is_descendant(const LayDoc *doc, int child_idx, int parent_idx);
static int lay_is_activate_marker(const char *onClick);
static void lay_activate(LayDoc *doc, int idx);
static void lay_back(LayDoc *doc);
static void lay_focus_delta(LayDoc *doc, int delta);
static const char *lay_cursor_prefix(const LayDoc *doc, int idx);
static void lay_get_label(const LayDoc *doc, int idx, LayVarLookupFn get_var, void *ctx, char *out, size_t outsz);
static void lay_get_sprite(const LayDoc *doc, int idx, LayVarLookupFn get_var, void *ctx, char *out, size_t outsz);
static const char *lay_get_id(const LayDoc *doc, int idx);

/* ---- khtpm_strip_layout.c, verbatim ---- */
typedef enum { LTOK_TEXT, LTOK_OPEN, LTOK_CLOSE, LTOK_SELFCLOSE } LayTokenType;

typedef struct {
    LayTokenType type;
    char content[LAY_LABEL_LEN];   /* TEXT token body */
    char tag_name[LAY_TAGNAME_LEN];
    char attributes[LAY_ATTRSTR_LEN];
} LayToken;

static LayToken *lay_tokenize(const char *content, int *token_count) {
    LayToken *tokens = (LayToken *)calloc((size_t)LAY_MAX_TOKENS, sizeof(LayToken));
    *token_count = 0;
    if (!tokens) return NULL;
    const char *cursor = content;
    while (*cursor && *token_count < LAY_MAX_TOKENS) {
        if (strncmp(cursor, "<!--", 4) == 0) {
            const char *comment_end = strstr(cursor + 4, "-->");
            cursor = comment_end ? comment_end + 3 : cursor + strlen(cursor);
            continue;
        }
        const char *tag_start = strchr(cursor, '<');
        if (tag_start && strncmp(tag_start, "<!--", 4) == 0) {
            if (tag_start > cursor) {
                LayToken *t = &tokens[(*token_count)++];
                t->type = LTOK_TEXT;
                int len = (int)(tag_start - cursor);
                if (len > LAY_LABEL_LEN - 1) len = LAY_LABEL_LEN - 1;
                strncpy(t->content, cursor, (size_t)len);
                t->content[len] = '\0';
            }
            const char *comment_end = strstr(tag_start + 4, "-->");
            cursor = comment_end ? comment_end + 3 : tag_start + strlen(tag_start);
            continue;
        }
        if (!tag_start) {
            LayToken *t = &tokens[(*token_count)++];
            t->type = LTOK_TEXT;
            strncpy(t->content, cursor, LAY_LABEL_LEN - 1);
            t->content[LAY_LABEL_LEN - 1] = '\0';
            break;
        }
        if (tag_start > cursor) {
            LayToken *t = &tokens[(*token_count)++];
            t->type = LTOK_TEXT;
            int len = (int)(tag_start - cursor);
            if (len > LAY_LABEL_LEN - 1) len = LAY_LABEL_LEN - 1;
            strncpy(t->content, cursor, (size_t)len);
            t->content[len] = '\0';
        }
        const char *tag_end = strchr(tag_start, '>');
        if (!tag_end) {
            LayToken *t = &tokens[(*token_count)++];
            t->type = LTOK_TEXT;
            t->content[0] = '<';
            t->content[1] = '\0';
            cursor = tag_start + 1;
            continue;
        }
        LayToken *t = &tokens[(*token_count)++];
        char tag_body[LAY_ATTRSTR_LEN];
        int body_len = (int)(tag_end - tag_start - 1);
        if (body_len > (int)sizeof(tag_body) - 1) body_len = (int)sizeof(tag_body) - 1;
        if (body_len < 0) body_len = 0;
        strncpy(tag_body, tag_start + 1, (size_t)body_len);
        tag_body[body_len] = '\0';
        if (tag_body[0] == '/') {
            t->type = LTOK_CLOSE;
            strncpy(t->tag_name, tag_body + 1, LAY_TAGNAME_LEN - 1);
        } else {
            int self_closing = 0;
            if (body_len > 0 && tag_body[body_len - 1] == '/') {
                self_closing = 1;
                tag_body[body_len - 1] = '\0';
            }
            t->type = self_closing ? LTOK_SELFCLOSE : LTOK_OPEN;
            char *space = strchr(tag_body, ' ');
            if (space) {
                *space = '\0';
                strncpy(t->tag_name, tag_body, LAY_TAGNAME_LEN - 1);
                strncpy(t->attributes, space + 1, LAY_ATTRSTR_LEN - 1);
            } else {
                strncpy(t->tag_name, tag_body, LAY_TAGNAME_LEN - 1);
                t->attributes[0] = '\0';
            }
        }
        cursor = tag_end + 1;
    }
    return tokens;
}

static void lay_parse_attributes(LayElement *el, const char *attr_str) {
    if (!attr_str || attr_str[0] == '\0') return;
    char *attrs = strdup(attr_str);
    if (!attrs) return;
    char *pos = attrs;
    while (*pos) {
        while (*pos && isspace((unsigned char)*pos)) pos++;
        if (!*pos) break;
        char *name_start = pos;
        while (*pos && *pos != '=' && !isspace((unsigned char)*pos)) pos++;
        char saved = *pos;
        *pos = '\0';
        while (*(++pos) && isspace((unsigned char)*pos)) { /* skip */ }
        if (*pos == '=') {
            pos++;
            while (*pos && isspace((unsigned char)*pos)) pos++;
        }
        char *val_start = pos;
        if (*pos == '"' || *pos == '\'') {
            char quote = *pos++;
            val_start = pos;
            while (*pos && *pos != quote) pos++;
            if (*pos) *pos++ = '\0';
        } else {
            while (*pos && !isspace((unsigned char)*pos) && *pos != '/') pos++;
            if (*pos) *pos++ = '\0';
        }

        if (strcmp(name_start, "label") == 0) {
            strncpy(el->label, val_start, LAY_LABEL_LEN - 1);
        } else if (strcmp(name_start, "onClick") == 0) {
            strncpy(el->onClick, val_start, LAY_ONCLICK_LEN - 1);
        } else if (strcmp(name_start, "target_id") == 0) {
            strncpy(el->target_id, val_start, LAY_ATTR_LEN - 1);
        } else if (strcmp(name_start, "sprite") == 0) {
            strncpy(el->sprite, val_start, LAY_SPRITE_LEN - 1);
        } else if (strcmp(name_start, "id") == 0) {
            strncpy(el->id, val_start, LAY_ID_LEN - 1);
        }
        if (pos > attrs) *(pos - 1) = saved;
    }
    free(attrs);
}

static void lay_substitute_vars_naked(const char *src, char *dst, size_t max_len,
                                       LayVarLookupFn get_var, void *ctx) {
    const char *p_src = src;
    char *p_dst = dst;
    int in_tag = 0;
    while (*p_src && (size_t)(p_dst - dst) < max_len - 1) {
        if (*p_src == '<') in_tag = 1;
        else if (*p_src == '>') in_tag = 0;

        if (!in_tag && *p_src == '$' && *(p_src + 1) == '{') {
            const char *end = strchr(p_src, '}');
            if (end) {
                char var_name[64];
                int len = (int)(end - (p_src + 2));
                if (len > 63) len = 63;
                if (len < 0) len = 0;
                strncpy(var_name, p_src + 2, (size_t)len);
                var_name[len] = '\0';
                const char *val = get_var ? get_var(var_name, ctx) : "";
                if (!val) val = "";
                while (*val && (size_t)(p_dst - dst) < max_len - 1) {
                    if (*val == '\\' && *(val + 1) == 'n') { *p_dst++ = '\n'; val += 2; }
                    else *p_dst++ = *val++;
                }
                p_src = end + 1;
                continue;
            }
        }
        *p_dst++ = *p_src++;
    }
    *p_dst = '\0';
}

static void lay_substitute_vars(const char *src, char *dst, size_t max_len,
                                 LayVarLookupFn get_var, void *ctx) {
    const char *p_src = src;
    char *p_dst = dst;
    while (*p_src && (size_t)(p_dst - dst) < max_len - 1) {
        if (*p_src == '\\' && (*(p_src + 1) == '$' || *(p_src + 1) == '{' ||
                                *(p_src + 1) == '<' || *(p_src + 1) == '\\')) {
            *p_dst++ = *(p_src + 1);
            p_src += 2;
            continue;
        }
        if (*p_src == '$' && *(p_src + 1) == '{') {
            const char *end = strchr(p_src, '}');
            if (end) {
                char var_name[64];
                int len = (int)(end - (p_src + 2));
                if (len > 63) len = 63;
                if (len < 0) len = 0;
                strncpy(var_name, p_src + 2, (size_t)len);
                var_name[len] = '\0';
                const char *val = get_var ? get_var(var_name, ctx) : "";
                if (!val) val = "";
                while (*val && (size_t)(p_dst - dst) < max_len - 1) {
                    if (*val == '\\' && *(val + 1) == 'n') { *p_dst++ = '\n'; val += 2; }
                    else *p_dst++ = *val++;
                }
                p_src = end + 1;
                continue;
            }
        }
        *p_dst++ = *p_src++;
    }
    *p_dst = '\0';
}

static char *lay_read_file_to_string(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (length < 0) { fclose(f); return NULL; }
    if (length > LAY_FILE_MAX - 1) length = LAY_FILE_MAX - 1;
    char *buffer = (char *)malloc((size_t)length + 1);
    if (!buffer) { fclose(f); return NULL; }
    size_t n = fread(buffer, 1, (size_t)length, f);
    buffer[n] = '\0';
    fclose(f);
    return buffer;
}

static void lay_build_tree(LayDoc *doc, LayToken *tokens, int tc) {
    doc->element_count = 0;
    int stack[64];
    int top = -1;
    for (int i = 0; i < tc && doc->element_count < LAY_MAX_ELEMENTS; i++) {
        LayToken *t = &tokens[i];
        if (t->type == LTOK_TEXT) {
            char *trim = strdup(t->content);
            if (!trim) continue;
            char *p = trim;
            while (*p && isspace((unsigned char)*p)) p++;
            if (*p) {
                char *end = p + strlen(p) - 1;
                while (end > p && isspace((unsigned char)*end)) *end-- = '\0';
            }
            if (!*p) { free(trim); continue; }
            LayElement *el = &doc->elements[doc->element_count++];
            memset(el, 0, sizeof(*el));
            strcpy(el->type, "text");
            strncpy(el->label, p, LAY_LABEL_LEN - 1);
            el->parent_index = (top >= 0) ? stack[top] : -1;
            if (el->parent_index >= 0) {
                LayElement *pa = &doc->elements[el->parent_index];
                if (pa->num_children < LAY_MAX_CHILDREN)
                    pa->children[pa->num_children++] = doc->element_count - 1;
            }
            free(trim);
        } else if (t->type == LTOK_OPEN || t->type == LTOK_SELFCLOSE) {
            if (strcmp(t->tag_name, "panel") == 0) {
                if (t->type == LTOK_OPEN && top < 63) stack[++top] = -1;
                continue;
            }
            int known = (strcmp(t->tag_name, "text") == 0 ||
                         strcmp(t->tag_name, "button") == 0 ||
                         strcmp(t->tag_name, "row") == 0 ||
                         strcmp(t->tag_name, "cli_io") == 0);
            if (!known) {
                if (t->type == LTOK_OPEN && top < 63) {
                    int parent_of_unknown = (top >= 0) ? stack[top] : -1;
                    stack[++top] = parent_of_unknown;
                }
                continue;
            }
            LayElement *el = &doc->elements[doc->element_count++];
            memset(el, 0, sizeof(*el));
            strncpy(el->type, t->tag_name, LAY_TYPE_LEN - 1);
            lay_parse_attributes(el, t->attributes);
            el->parent_index = (top >= 0) ? stack[top] : -1;
            int my_index = doc->element_count - 1;
            if (el->parent_index >= 0) {
                LayElement *pa = &doc->elements[el->parent_index];
                if (pa->num_children < LAY_MAX_CHILDREN)
                    pa->children[pa->num_children++] = my_index;
            }
            if (t->type == LTOK_OPEN && top < 63) stack[++top] = my_index;
        } else if (t->type == LTOK_CLOSE) {
            if (strcmp(t->tag_name, "panel") == 0) {
                if (top >= 0) top--;
                continue;
            }
            if (top >= 0) top--;
        }
    }
}

static int lay_load(LayDoc *doc, const char *path, LayVarLookupFn get_var, void *ctx) {
    memset(doc, 0, sizeof(*doc));
    doc->active_index = -1;
    doc->focus_index = -1;

    char *content = lay_read_file_to_string(path);
    if (!content) return 0;

    char *substituted = (char *)malloc(LAY_FILE_MAX);
    if (!substituted) { free(content); return 0; }
    lay_substitute_vars_naked(content, substituted, LAY_FILE_MAX, get_var, ctx);
    free(content);

    int tc = 0;
    LayToken *tokens = lay_tokenize(substituted, &tc);
    free(substituted);
    if (!tokens) return 0;

    lay_build_tree(doc, tokens, tc);
    free(tokens);

    for (int i = 0; i < doc->element_count; i++) {
        if (lay_is_navigable(doc, i)) { doc->focus_index = i; break; }
    }
    if (doc->focus_index < 0) doc->focus_index = 0;
    return 1;
}

static int lay_reload_preserving_scope(LayDoc *doc, const char *path, LayVarLookupFn get_var, void *ctx) {
    char saved_onclick[LAY_ONCLICK_LEN] = "";
    char saved_focus_onclick[LAY_ONCLICK_LEN] = "";
    if (doc->active_index >= 0 && doc->active_index < doc->element_count)
        strncpy(saved_onclick, doc->elements[doc->active_index].onClick, LAY_ONCLICK_LEN - 1);
    if (doc->focus_index >= 0 && doc->focus_index < doc->element_count)
        strncpy(saved_focus_onclick, doc->elements[doc->focus_index].onClick, LAY_ONCLICK_LEN - 1);

    if (!lay_load(doc, path, get_var, ctx)) return 0;

    if (saved_onclick[0]) {
        for (int i = 0; i < doc->element_count; i++) {
            if (lay_is_interactive(&doc->elements[i]) &&
                strcmp(doc->elements[i].onClick, saved_onclick) == 0) {
                doc->active_index = i;
                break;
            }
        }
    }
    if (saved_focus_onclick[0]) {
        for (int i = 0; i < doc->element_count; i++) {
            if (lay_is_interactive(&doc->elements[i]) &&
                strcmp(doc->elements[i].onClick, saved_focus_onclick) == 0) {
                doc->focus_index = i;
                break;
            }
        }
    }
    if (doc->focus_index < 0 || doc->focus_index >= doc->element_count ||
        !lay_is_navigable(doc, doc->focus_index)) {
        doc->focus_index = -1;
        for (int i = 0; i < doc->element_count; i++) {
            if (lay_is_navigable(doc, i)) { doc->focus_index = i; break; }
        }
        if (doc->focus_index < 0) doc->focus_index = 0;
    }
    return 1;
}

static int lay_is_interactive(const LayElement *el) {
    return strcmp(el->type, "button") == 0 || strcmp(el->type, "cli_io") == 0;
}

static int lay_is_activate_marker(const char *onClick) {
    return onClick && strncmp(onClick, "ACTIVATE", 8) == 0;
}

static int lay_is_descendant(const LayDoc *doc, int child_idx, int parent_idx) {
    if (child_idx < 0 || parent_idx < 0) return 0;
    if (child_idx >= doc->element_count || parent_idx >= doc->element_count) return 0;
    int p = doc->elements[child_idx].parent_index;
    while (p != -1) {
        if (p == parent_idx) return 1;
        p = doc->elements[p].parent_index;
    }
    return 0;
}

static int lay_is_navigable(const LayDoc *doc, int idx) {
    if (idx < 0 || idx >= doc->element_count) return 0;
    const LayElement *el = &doc->elements[idx];
    if (!lay_is_interactive(el)) return 0;

    if (strcmp(el->type, "cli_io") == 0 && doc->active_index != idx) return 0;

    if (doc->active_index != -1) {
        const LayElement *active_el = &doc->elements[doc->active_index];
        if (active_el->num_children > 0 && lay_is_activate_marker(active_el->onClick)) {
            if (idx == doc->active_index) return 1;
            return lay_is_descendant(doc, idx, doc->active_index);
        }
        return idx == doc->active_index;
    }

    int p = el->parent_index;
    while (p != -1) {
        if (lay_is_activate_marker(doc->elements[p].onClick)) return 0;
        p = doc->elements[p].parent_index;
    }
    return 1;
}

static void lay_activate(LayDoc *doc, int idx) {
    if (idx < 0 || idx >= doc->element_count) return;
    doc->active_index = idx;
    doc->focus_index = idx;
    for (int i = 0; i < doc->element_count; i++) {
        if (i != idx && lay_is_descendant(doc, i, idx) && lay_is_navigable(doc, i)) {
            doc->focus_index = i;
            break;
        }
    }
}

static void lay_back(LayDoc *doc) {
    if (doc->active_index == -1) return;
    int old_active = doc->active_index;
    int p = doc->elements[doc->active_index].parent_index;
    while (p != -1 && !lay_is_activate_marker(doc->elements[p].onClick))
        p = doc->elements[p].parent_index;
    doc->active_index = p;
    doc->focus_index = old_active;
    if (doc->active_index != -1 && !lay_is_navigable(doc, doc->focus_index)) {
        doc->focus_index = -1;
        for (int i = 0; i < doc->element_count; i++) {
            if (lay_is_navigable(doc, i)) { doc->focus_index = i; break; }
        }
    }
}

static void lay_focus_delta(LayDoc *doc, int delta) {
    if (doc->element_count == 0) return;
    int nav_count = 0;
    int nav_list[LAY_MAX_ELEMENTS];
    int cur_pos = -1;
    for (int i = 0; i < doc->element_count; i++) {
        if (lay_is_navigable(doc, i)) {
            if (i == doc->focus_index) cur_pos = nav_count;
            nav_list[nav_count++] = i;
        }
    }
    if (nav_count == 0) return;
    if (cur_pos < 0) { doc->focus_index = nav_list[0]; return; }
    int next = (cur_pos + delta) % nav_count;
    if (next < 0) next += nav_count;
    doc->focus_index = nav_list[next];
}

static const char *lay_cursor_prefix(const LayDoc *doc, int idx) {
    if (idx < 0 || idx >= doc->element_count) return "[ ]";
    int is_active = (idx == doc->active_index);
    int is_focused = (idx == doc->focus_index);
    int navigable = lay_is_navigable(doc, idx);
    if (is_active) return "[^]";
    if (is_focused && (doc->active_index == -1 || navigable)) return "[>]";
    return "[ ]";
}

static void lay_get_label(const LayDoc *doc, int idx, LayVarLookupFn get_var, void *ctx,
                    char *out, size_t outsz) {
    if (idx < 0 || idx >= doc->element_count) { if (outsz) out[0] = '\0'; return; }
    lay_substitute_vars(doc->elements[idx].label, out, outsz, get_var, ctx);
}

static void lay_get_sprite(const LayDoc *doc, int idx, LayVarLookupFn get_var, void *ctx,
                     char *out, size_t outsz) {
    if (idx < 0 || idx >= doc->element_count) { if (outsz) out[0] = '\0'; return; }
    lay_substitute_vars(doc->elements[idx].sprite, out, outsz, get_var, ctx);
}

static const char *lay_get_id(const LayDoc *doc, int idx) {
    if (idx < 0 || idx >= doc->element_count) return "";
    return doc->elements[idx].id;
}

/* ---- khtpm_strip_parser.c, verbatim from here (minus g_house_root/
 * g_click_two_step/set_window_opacity/load_theme_opacity dups and the
 * renamed main()) ---- */
#define POLL_INTERVAL_ACTIVE_USEC 16667
#define POLL_INTERVAL_IDLE_USEC   100000
#define ACTIVE_HOLD_TICKS 30

typedef struct {
    char theme_bg[32];
    char theme_fg[32];
    char digit_buf[16];
    int  nav_armed;
    int  hq_focus;
    int  hq_open;
    int  cliio_active;
    int  cliio_typing;
    char cliio_op[32];
    char cliio_buffer[256];
    char cliio_label[64];

    char var_tabs[48 * 1024];
    char var_shortcuts[48 * 1024];
    char var_hqitems[48 * 1024];

    char var_username[128];
    char var_file_label[256];
    char var_desks_label[64];
    char var_avatar_dir[KTB_PATH_BUF];
    char var_datetime[128];
} SpState;

static pid_t g_manager_pid = -1;
static SpState g_st;

static Display *g_dpy = NULL;
static int g_zorder_above = 0;

static void strip_load_click_two_step(void) {
    char path[KTB_PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/hq_ui.pdl", g_house_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *val = eq + 1;
        char *nl = strchr(val, '\n');
        if (nl) *nl = '\0';
        if (strcmp(line, "click_two_step") == 0) g_click_two_step = atoi(val) != 0;
    }
    fclose(f);
}

static int g_has_real_focus = 0;
static Window g_focused_win = 0;

static void path_join2(char *out, size_t n, const char *root, const char *rel) {
    size_t rl = strlen(root);
    if (rl > 0 && (root[rl - 1] == '/' || root[rl - 1] == '\\'))
        snprintf(out, n, "%s%s", root, rel);
    else
        snprintf(out, n, "%s/%s", root, rel);
}

static void read_small_file(const char *rel_path, char *out, size_t outsz) {
    out[0] = '\0';
    char path[KTB_PATH_BUF];
    path_join2(path, sizeof(path), g_house_root, rel_path);
    FILE *f = fopen(path, "r");
    if (!f) return;
    size_t n = fread(out, 1, outsz - 1, f);
    out[n] = '\0';
    fclose(f);
    while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r' || out[n - 1] == ' ' || out[n - 1] == '\t'))
        out[--n] = '\0';
}

static char *trim_field(char *s) {
    while (*s == ' ') s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\r' || s[n - 1] == '\n')) s[--n] = '\0';
    return s;
}

static void load_state(SpState *st) {
    memset(st, 0, sizeof(*st));
    snprintf(st->theme_bg, sizeof(st->theme_bg), "white");
    snprintf(st->theme_fg, sizeof(st->theme_fg), "black");

    char path[KTB_PATH_BUF];
    path_join2(path, sizeof(path), g_house_root, "#.desktop/strip_state.txt");
    FILE *f = fopen(path, "r");
    if (f) {
        char line[KTB_PATH_BUF];
        while (fgets(line, sizeof(line), f)) {
            char *save = NULL;
            char *col0 = strtok_r(line, "|", &save);
            if (!col0) continue;
            col0 = trim_field(col0);
            if (strcmp(col0, "KEY") != 0) continue;
            char *keyf = strtok_r(NULL, "|", &save);
            char *valf = strtok_r(NULL, "\n", &save);
            if (!keyf || !valf) continue;
            char *key = trim_field(keyf);
            char *val = trim_field(valf);
            if (strcmp(key, "theme_bg") == 0) snprintf(st->theme_bg, sizeof(st->theme_bg), "%s", val);
            else if (strcmp(key, "theme_fg") == 0) snprintf(st->theme_fg, sizeof(st->theme_fg), "%s", val);
            else if (strcmp(key, "digit_buf") == 0) snprintf(st->digit_buf, sizeof(st->digit_buf), "%s", val);
            else if (strcmp(key, "nav_armed") == 0) st->nav_armed = atoi(val);
            else if (strcmp(key, "hq_focus") == 0) st->hq_focus = atoi(val);
            else if (strcmp(key, "hq_open") == 0) st->hq_open = atoi(val);
            else if (strcmp(key, "cliio_active") == 0) st->cliio_active = atoi(val);
            else if (strcmp(key, "cliio_typing") == 0) st->cliio_typing = atoi(val);
            else if (strcmp(key, "cliio_op") == 0) snprintf(st->cliio_op, sizeof(st->cliio_op), "%s", val);
            else if (strcmp(key, "cliio_buffer") == 0) snprintf(st->cliio_buffer, sizeof(st->cliio_buffer), "%s", val);
            else if (strcmp(key, "cliio_label") == 0) snprintf(st->cliio_label, sizeof(st->cliio_label), "%s", val);
        }
        fclose(f);
    }

    read_small_file("#.desktop/strip_var_tabs.txt", st->var_tabs, sizeof(st->var_tabs));
    read_small_file("#.desktop/strip_var_shortcuts.txt", st->var_shortcuts, sizeof(st->var_shortcuts));
    read_small_file("#.desktop/strip_var_hqitems.txt", st->var_hqitems, sizeof(st->var_hqitems));
    read_small_file("#.desktop/strip_var_username.txt", st->var_username, sizeof(st->var_username));
    read_small_file("#.desktop/strip_var_file_label.txt", st->var_file_label, sizeof(st->var_file_label));
    read_small_file("#.desktop/strip_var_desks_label.txt", st->var_desks_label, sizeof(st->var_desks_label));
    read_small_file("#.desktop/strip_var_avatar_dir.txt", st->var_avatar_dir, sizeof(st->var_avatar_dir));
    read_small_file("#.desktop/strip_var_datetime.txt", st->var_datetime, sizeof(st->var_datetime));
}

static char g_build_uid[64] = "";
static char g_build_uid_sprite_dir[KTB_PATH_BUF] = "";
static Window g_hq_win_handle = 0;

static const char *sp_get_var(const char *name, void *ctx) {
    SpState *st = (SpState *)ctx;
    if (strcmp(name, "strip_tabs") == 0) return st->var_tabs;
    if (strcmp(name, "strip_shortcuts") == 0) return st->var_shortcuts;
    if (strcmp(name, "strip_hq_items") == 0) return st->var_hqitems;
    if (strcmp(name, "cliio_label") == 0) return st->cliio_label;
    if (strcmp(name, "username") == 0) return st->var_username;
    if (strcmp(name, "file_label") == 0) return st->var_file_label;
    if (strcmp(name, "desks_label") == 0) return st->var_desks_label;
    if (strcmp(name, "avatar_dir") == 0) return st->var_avatar_dir;
    if (strcmp(name, "datetime") == 0) return st->var_datetime;
    if (strcmp(name, "build_uid") == 0) return g_build_uid;
    return "";
}

static void build_uid_init(char *out, size_t outsz, const char *house_root) {
    snprintf(out, outsz, "%d", (int)getpid());

    time_t now = time(NULL);
    struct tm tmv;
    localtime_r(&now, &tmv);
    int h = tmv.tm_hour % 12; if (h == 0) h = 12;
    int half = tmv.tm_min >= 30;
    unsigned cp = 0x1F550 + (unsigned)(h - 1) + (half ? 12 : 0);
    char glyph[8];
    int goff = 0;
    glyph[goff++] = (char)(0xF0 | ((cp >> 18) & 0x07));
    glyph[goff++] = (char)(0x80 | ((cp >> 12) & 0x3F));
    glyph[goff++] = (char)(0x80 | ((cp >> 6) & 0x3F));
    glyph[goff++] = (char)(0x80 | (cp & 0x3F));
    glyph[goff] = '\0';

    char dir[KTB_PATH_BUF];
    snprintf(dir, sizeof(dir), "%s/#.desktop/build_uid_sprite", house_root);
    mkdir(dir, 0755);
    char atlas[KTB_PATH_BUF], csv[KTB_PATH_BUF];
    snprintf(atlas, sizeof(atlas), "%s/atlas.png", dir);
    snprintf(csv, sizeof(csv), "%s/sprite.csv", dir);
    char gen_atlas[KTB_PATH_BUF * 2], gen_xtract[KTB_PATH_BUF * 2];
    snprintf(gen_atlas, sizeof(gen_atlas), "'%s/*.monads/*.livedesk-taskbar/ops/+x/emoji_gen_atlas.+x' '%s' '%s' >/dev/null 2>&1",
             house_root, glyph, atlas);
    snprintf(gen_xtract, sizeof(gen_xtract), "'%s/*.monads/*.livedesk-taskbar/ops/+x/emoji_xtract.+x' '%s' 0 64 '%s' >/dev/null 2>&1",
             house_root, atlas, csv);
    if (system(gen_atlas) == 0 && system(gen_xtract) == 0)
        snprintf(g_build_uid_sprite_dir, sizeof(g_build_uid_sprite_dir), "%s", dir);
}

static void launch_manager(void) {
    char exe[KTB_PATH_BUF];
    path_join2(exe, sizeof(exe), g_house_root,
               "*.monads/*.livedesk-taskbar/ops/+x/khtpm_taskbar_manager_main.+x");

    pid_t pid = fork();
    if (pid == 0) {
        execl(exe, exe, g_house_root, (char *)NULL);
        _exit(1);
    } else if (pid > 0) {
        g_manager_pid = pid;
    } else {
        fprintf(stderr, "strip_parser: fork() failed: %s\n", strerror(errno));
    }
}

static int ensure_manager_running(void) {
    if (g_manager_pid <= 0) { launch_manager(); return 1; }
    return 0;
}

static void reap_manager_nonblocking(void) {
    if (g_manager_pid <= 0) return;
    int status;
    pid_t r = waitpid(g_manager_pid, &status, WNOHANG);
    if (r == g_manager_pid) {
        g_manager_pid = -1;
    }
}

static void wait_for_manager_first_publish(void) {
    char path[KTB_PATH_BUF];
    path_join2(path, sizeof(path), g_house_root, "#.desktop/strip_state.txt");
    for (int i = 0; i < 40; i++) {
        struct stat st;
        if (stat(path, &st) == 0 && st.st_size > 0) return;
        struct timespec ts = { 0, 25 * 1000 * 1000 };
        nanosleep(&ts, NULL);
    }
}

static void cleanup_manager(void) {
    if (g_manager_pid > 0) {
        kill(g_manager_pid, SIGTERM);
        waitpid(g_manager_pid, NULL, WNOHANG);
        g_manager_pid = -1;
    }
}

static volatile sig_atomic_t g_strip_running = 1;
static void strip_on_sigterm(int sig) { (void)sig; g_strip_running = 0; }

static long g_frame_changed_cursor = -1;

static int frame_changed_dirty(void) {
    char path[KTB_PATH_BUF];
    path_join2(path, sizeof(path), g_house_root, "#.desktop/strip_frame_changed.txt");
    struct stat st;
    if (stat(path, &st) != 0) return g_frame_changed_cursor != 0 ? (g_frame_changed_cursor = 0, 1) : 0;
    if (g_frame_changed_cursor < 0) { g_frame_changed_cursor = st.st_size; return 1; }
    if (st.st_size != g_frame_changed_cursor) { g_frame_changed_cursor = st.st_size; return 1; }
    return 0;
}

static long g_strip_theme_changed_cursor = 0;

static int strip_theme_changed_dirty(void) {
    char path[KTB_PATH_BUF];
    path_join2(path, sizeof(path), g_house_root, "#.desktop/livedesk_theme_changed.txt");
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    if (st.st_size != g_strip_theme_changed_cursor) { g_strip_theme_changed_cursor = st.st_size; return 1; }
    return 0;
}

static void send_code(int code) {
    if (code <= 0) return;
    int just_launched = ensure_manager_running();
    char path[KTB_PATH_BUF];
    path_join2(path, sizeof(path), g_house_root, "#.desktop/strip_history.txt");
    if (just_launched) usleep(150000);
    FILE *f = fopen(path, "a");
    if (f) { fprintf(f, "%d\n", code); fclose(f); }
}

static void mirror_key_history(int code) {
    if (code <= 0) return;
    char path[KTB_PATH_BUF];
    path_join2(path, sizeof(path), g_house_root, "#.desktop/strip_input_history.txt");
    FILE *f = fopen(path, "a");
    if (f) { fprintf(f, "KEY_PRESSED: %d\n", code); fclose(f); }
}

static void mirror_mouse_history(const char *window_name, int button, int x, int y) {
    char path[KTB_PATH_BUF];
    path_join2(path, sizeof(path), g_house_root, "#.desktop/strip_input_history.txt");
    FILE *f = fopen(path, "a");
    if (f) { fprintf(f, "MOUSE_EVENT: %d %d %d 1 %s\n", button, x, y, window_name); fclose(f); }
}

static unsigned long parse_color(Display *dpy, const char *name, unsigned long fallback) {
    Colormap cmap = DefaultColormap(dpy, DefaultScreen(dpy));
    XColor c;
    if (XAllocNamedColor(dpy, cmap, name, &c, &c)) return c.pixel;
    return fallback;
}

static void load_strip_offset(int *out_x, int *out_y) {
    *out_x = 0;
    *out_y = 40;
    char path[KTB_PATH_BUF];
    path_join2(path, sizeof(path), g_house_root, "#.desktop/livedesk_taskbar.pdl");
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[KTB_PATH_BUF];
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
        char *v_end = v + strlen(v);
        while (v_end > v && v_end[-1] == ' ') v_end--;
        *v_end = '\0';
        if (v[0] == '\0') continue;
        if (strcmp(key, "strip_x_offset") == 0) *out_x = atoi(v);
        else if (strcmp(key, "strip_y_offset") == 0) *out_y = atoi(v);
    }
    fclose(f);
}

static int ktb_strip_nonfatal_x_error(Display *d, XErrorEvent *e) {
    char ebuf[128]; XGetErrorText(d, e->error_code, ebuf, sizeof(ebuf));
    fprintf(stderr, "khtpm_strip_parser: X error (non-fatal): %s (request %d.%d)\n", ebuf, e->request_code, e->minor_code);
    return 0;
}

static void taskbar_set_wm_class(Display *dpy, Window w) {
    XClassHint *ch = XAllocClassHint();
    if (!ch) return;
    ch->res_name = (char *)"MuchiverseLivedesk";
    ch->res_class = (char *)"MuchiverseLivedesk";
    XSetClassHint(dpy, w, ch);
    XFree(ch);
}

static void taskbar_make_wm_managed_dock(Display *dpy, Window w, int x, int y) {
    Atom motif_hints = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
    long hints[5] = { 2, 0, 0, 0, 0 };
    XChangeProperty(dpy, w, motif_hints, motif_hints, 32, PropModeReplace, (unsigned char *)hints, 5);

    Atom wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
    Atom above = XInternAtom(dpy, "_NET_WM_STATE_ABOVE", False);
    Atom skip_taskbar = XInternAtom(dpy, "_NET_WM_STATE_SKIP_TASKBAR", False);
    Atom skip_pager = XInternAtom(dpy, "_NET_WM_STATE_SKIP_PAGER", False);
    Atom sticky = XInternAtom(dpy, "_NET_WM_STATE_STICKY", False);
    Atom states[4] = { above, skip_taskbar, skip_pager, sticky };
    XChangeProperty(dpy, w, wm_state, XA_ATOM, 32, PropModeReplace, (unsigned char *)states, 4);

    Atom win_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    Atom dock = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
    XChangeProperty(dpy, w, win_type, XA_ATOM, 32, PropModeReplace, (unsigned char *)&dock, 1);

    XSizeHints *shints = XAllocSizeHints();
    if (shints) { shints->flags = PPosition; shints->x = x; shints->y = y; XSetWMNormalHints(dpy, w, shints); XFree(shints); }
}

#define STRIP_FRAME_HISTORY_MAX_BYTES 262144
static char g_frame_history_path[KTB_PATH_BUF];

static void frame_history_init(void) {
    path_join2(g_frame_history_path, sizeof(g_frame_history_path), g_house_root,
               "#.desktop/khtpm_strip_frame_history.txt");
    FILE *f = fopen(g_frame_history_path, "w");
    if (f) fclose(f);
}

static void append_frame_history(const LayDoc *header_doc, const LayDoc *bottom_doc, int nav_focus) {
    if (!g_frame_history_path[0]) return;
    struct stat st;
    if (stat(g_frame_history_path, &st) == 0 && st.st_size > STRIP_FRAME_HISTORY_MAX_BYTES)
        truncate(g_frame_history_path, 0);
    FILE *f = fopen(g_frame_history_path, "a");
    if (!f) return;
    const char *hf_type = "-", *hf_label = "-", *hf_onclick = "-";
    if (header_doc->focus_index >= 0 && header_doc->focus_index < header_doc->element_count) {
        hf_type = header_doc->elements[header_doc->focus_index].type;
        hf_label = header_doc->elements[header_doc->focus_index].label;
        hf_onclick = header_doc->elements[header_doc->focus_index].onClick;
    }
    fprintf(f, "header.focus=%d[type=%s label=%s onClick=%s] header.active=%d "
               "bottom.focus=%d unified_nav_focus=%d has_real_x11_focus=%d "
               "cliio_active=%d nav_armed=%d digit_buf=%s hq_focus=%d element_count=%d\n",
            header_doc->focus_index, hf_type, hf_label, hf_onclick, header_doc->active_index,
            bottom_doc->focus_index, nav_focus, g_has_real_focus,
            g_st.cliio_active, g_st.nav_armed, g_st.digit_buf, g_st.hq_focus, header_doc->element_count);
    fclose(f);
}

static void taskbar_soft_focus(Display *dpy, Window w) {
    if (!w) return;
    XRaiseWindow(dpy, w);
    if (g_focused_win != w) XSetInputFocus(dpy, w, RevertToParent, CurrentTime);
    XFlush(dpy);
}

static int g_nav_focus = 0;

static int root_nav_count(const LayDoc *doc) {
    int n = 0;
    for (int i = 0; i < doc->element_count; i++) {
        if (!lay_is_navigable(doc, i)) continue;
        n++;
    }
    return n;
}

static int root_nav_element_at(const LayDoc *doc, int pos) {
    int n = 0;
    for (int i = 0; i < doc->element_count; i++) {
        if (!lay_is_navigable(doc, i)) continue;
        if (n == pos) return i;
        n++;
    }
    return -1;
}

static int root_nav_pos_of(const LayDoc *doc, int elidx) {
    int n = 0;
    for (int i = 0; i < doc->element_count; i++) {
        if (!lay_is_navigable(doc, i)) continue;
        if (i == elidx) return n;
        n++;
    }
    return -1;
}

static void unified_apply(LayDoc *header_doc, LayDoc *bottom_doc) {
    int hc = root_nav_count(header_doc);
    if (g_nav_focus >= 0 && g_nav_focus < hc) {
        header_doc->focus_index = root_nav_element_at(header_doc, g_nav_focus);
        bottom_doc->focus_index = -1;
    } else {
        header_doc->focus_index = -1;
        int bc = root_nav_count(bottom_doc);
        int bpos = g_nav_focus - hc;
        bottom_doc->focus_index = (bpos >= 0 && bpos < bc) ? root_nav_element_at(bottom_doc, bpos) : -1;
    }
}

static void unified_step(LayDoc *header_doc, LayDoc *bottom_doc, int delta) {
    int total = root_nav_count(header_doc) + root_nav_count(bottom_doc);
    if (total <= 0) return;
    g_nav_focus = ((g_nav_focus + delta) % total + total) % total;
    unified_apply(header_doc, bottom_doc);
}

static void lay_focus_first(LayDoc *doc) {
    for (int i = 0; i < doc->element_count; i++) {
        if (lay_is_navigable(doc, i)) { doc->focus_index = i; return; }
    }
}

static void sync_focus_to_digit_buf(LayDoc *header_doc, LayDoc *bottom_doc, const SpState *st) {
    if (!st->digit_buf[0]) return;
    if (header_doc->active_index != -1) return;
    int nav_n = atoi(st->digit_buf);
    if (nav_n <= 0) return;
    int total = root_nav_count(header_doc) + root_nav_count(bottom_doc);
    if (nav_n > total) return;
    g_nav_focus = nav_n - 1;
    unified_apply(header_doc, bottom_doc);
}

static void ktb_zorder_apply_tree(int raise) {
    if (!g_dpy) return;
    Window root = RootWindow(g_dpy, DefaultScreen(g_dpy));
    Window root_ret, parent_ret;
    Window *children = NULL;
    unsigned int n = 0;
    if (!XQueryTree(g_dpy, root, &root_ret, &parent_ret, &children, &n) || !children) return;
    for (unsigned int i = 0; i < n; i++) {
        char *nm = NULL;
        if (!XFetchName(g_dpy, children[i], &nm) || !nm) continue;
        int is_entity = (strncmp(nm, "tile:", 5) == 0);
        XFree(nm);
        if (!is_entity) continue;
        if (raise) XRaiseWindow(g_dpy, children[i]);
        else XLowerWindow(g_dpy, children[i]);
    }
    XFree(children);
}

static void ktb_toggle_zorder_apply(int raise) {
    if (!g_dpy) return;
    char dir[KTB_PATH_BUF];
    snprintf(dir, sizeof(dir), "%s/#.desktop/nav_tab", g_house_root);
    DIR *d = opendir(dir);
    if (d) {
        struct dirent *de;
        while ((de = readdir(d))) {
            if (de->d_name[0] == '.') continue;
            char fp[KTB_PATH_BUF];
            snprintf(fp, sizeof(fp), "%s/%s", dir, de->d_name);
            pid_t pid = (pid_t)atoi(de->d_name);
            if (pid > 1 && kill(pid, 0) != 0 && errno == ESRCH) {
                unlink(fp);
                continue;
            }
            FILE *f = fopen(fp, "r");
            if (!f) continue;
            int ord = 0;
            unsigned long xid = 0;
            if (fscanf(f, "%d %lx", &ord, &xid) >= 2 && xid) {
                if (raise) XRaiseWindow(g_dpy, (Window)xid);
                else XLowerWindow(g_dpy, (Window)xid);
            }
            fclose(f);
        }
        closedir(d);
    }
    ktb_zorder_apply_tree(raise);
    XFlush(g_dpy);
}

static void ktb_toggle_zorder_respawn(void) {
    char bin0[KTB_PATH_BUF], bin1[KTB_PATH_BUF], bin2[KTB_PATH_BUF];
    snprintf(bin0, sizeof(bin0), "%s/*.monads/*.livedesk-taskbar/ops/+x/tp_desktop_window_rgb.+x", g_house_root);
    snprintf(bin1, sizeof(bin1), "%s/*.monads/*.livedesk-taskbar/ops/+x/khtpm_core_render.+x", g_house_root);
    snprintf(bin2, sizeof(bin2), "%s/&.hq-apps/network/+x/network_browser_render.+x", g_house_root);
    const char *bins[3] = { bin0, bin1, bin2 };
    const char *needles[3] = { "tp_desktop_window_rgb", "khtpm_core_render", "network_browser_render" };
    struct { pid_t pid; int which; char arg[8][KTB_PATH_BUF]; int argc; } found[64];
    int n_found = 0;
    DIR *pd = opendir("/proc");
    if (!pd) return;
    struct dirent *ent;
    while ((ent = readdir(pd)) != NULL) {
        if (ent->d_name[0] < '0' || ent->d_name[0] > '9') continue;
        char cpath[64];
        snprintf(cpath, sizeof(cpath), "/proc/%s/cmdline", ent->d_name);
        FILE *cf = fopen(cpath, "r");
        if (!cf) continue;
        char cmdbuf[KTB_PATH_BUF * 24];
        size_t got = fread(cmdbuf, 1, sizeof(cmdbuf) - 1, cf);
        fclose(cf);
        if (got == 0) continue;
        cmdbuf[got] = '\0';
        const char *a0 = cmdbuf;
        size_t a0len = strlen(a0);
        if (a0len == 0) continue;
        int which = -1;
        for (int k = 0; k < 3; k++) if (strstr(a0, needles[k])) { which = k; break; }
        if (which < 0) continue;
        if (n_found >= (int)(sizeof(found) / sizeof(found[0]))) break;
        found[n_found].pid = (pid_t)atoi(ent->d_name);
        found[n_found].which = which;
        found[n_found].argc = 0;
        const char *p = cmdbuf;
        for (int i = 0; i < 8; i++) {
            size_t l = strlen(p);
            if (l == 0) break;
            snprintf(found[n_found].arg[i], KTB_PATH_BUF, "%s", p);
            found[n_found].argc++;
            p += l + 1;
            if (p >= cmdbuf + got) break;
        }
        n_found++;
    }
    closedir(pd);
    for (int i = 0; i < n_found; i++) kill(found[i].pid, SIGTERM);
    usleep(300000);
    extern char **environ;
    for (int i = 0; i < n_found; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            setsid();
            int devnull = open("/dev/null", O_RDWR);
            if (devnull >= 0) { dup2(devnull, 0); dup2(devnull, 1); dup2(devnull, 2); if (devnull > 2) close(devnull); }
            char *av[9];
            av[0] = (char *)bins[found[i].which];
            int n = found[i].argc < 8 ? found[i].argc : 8;
            for (int j = 1; j < n; j++) av[j] = found[i].arg[j];
            av[n] = NULL;
            execve(av[0], av, environ);
            _exit(1);
        }
    }
}

static void ktb_toggle_zorder(void) {
    g_zorder_above = 0;
    char path[KTB_PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/khtpm_zorder_mode.state.txt", g_house_root);
    FILE *rf = fopen(path, "r");
    if (rf) {
        char line[32];
        if (fgets(line, sizeof(line), rf) && strstr(line, "mode=above")) g_zorder_above = 1;
        fclose(rf);
    }
    g_zorder_above = !g_zorder_above;
    FILE *f = fopen(path, "w");
    if (f) { fprintf(f, "mode=%s\n", g_zorder_above ? "above" : "normal"); fclose(f); }
    char or_path[KTB_PATH_BUF];
    snprintf(or_path, sizeof(or_path), "%s/#.desktop/livedesk_override_redirect.pdl", g_house_root);
    FILE *orf = fopen(or_path, "w");
    if (orf) { fprintf(orf, "override_redirect=%s\n", g_zorder_above ? "true" : "false"); fclose(orf); }
    ktb_toggle_zorder_respawn();
    ktb_toggle_zorder_apply(g_zorder_above);
}

static void dispatch_onclick(LayDoc *doc, int idx) {
    if (idx < 0 || idx >= doc->element_count) return;
    const char *oc = doc->elements[idx].onClick;
    if (!oc[0]) return;

    if (lay_is_activate_marker(oc)) {
        lay_activate(doc, idx);
        const char *colon = strchr(oc, ':');
        if (colon) {
            int n = atoi(colon + 1);
            if (n > 0) send_code(KSC_HQ_HEADER_BASE + n);
        }
        return;
    }
    if (strcmp(oc, "BACK") == 0) {
        lay_back(doc);
        send_code(KSC_ESCAPE);
        return;
    }
    if (strncmp(oc, "STRIP:", 6) == 0) {
        int n = atoi(oc + 6);
        if (n > 0) send_code(KSC_HQ_HEADER_BASE + n);
        return;
    }
    if (strncmp(oc, "TAB:", 4) == 0) {
        int i = atoi(oc + 4);
        send_code(KSC_TAB_BASE + i);
        return;
    }
    if (strncmp(oc, "SHORTCUT:", 9) == 0) {
        int i = atoi(oc + 9);
        if (i >= 0 && i < KTB_MAX_SHORTCUTS) send_code(KSC_SHORTCUT_BASE + i);
        return;
    }
    if (strcmp(oc, "ZORDER_TOGGLE") == 0) {
        ktb_toggle_zorder();
        return;
    }
    if (strncmp(oc, "HQITEM:", 7) == 0) {
        int i = atoi(oc + 7);
        if (i >= 0 && i < KTB_LIVEDESK_DYN_MAX) send_code(KSC_HQ_ITEM_BASE + i);
        return;
    }
}

static void dispatch_key_code(LayDoc *header_doc, LayDoc *bottom_doc, const SpState *st, int code) {
    if (code == KSC_ENTER) {
        if (st->hq_open && header_doc->active_index == -1) {
            send_code(KSC_ENTER);
            return;
        }
        if (st->cliio_active) {
            send_code(KSC_ENTER);
        } else if (header_doc->active_index != -1) {
            dispatch_onclick(header_doc, header_doc->focus_index);
        } else if (bottom_doc->focus_index >= 0 && lay_is_navigable(bottom_doc, bottom_doc->focus_index)) {
            dispatch_onclick(bottom_doc, bottom_doc->focus_index);
        } else if (lay_is_navigable(header_doc, header_doc->focus_index)) {
            dispatch_onclick(header_doc, header_doc->focus_index);
        } else {
            send_code(KSC_ENTER);
        }
    } else if (code == KSC_ESCAPE) {
        send_code(KSC_ESCAPE);
        if (header_doc->active_index != -1 &&
            strcmp(header_doc->elements[header_doc->active_index].type, "cli_io") != 0) {
            lay_back(header_doc);
        }
    } else if (code == KSC_BACKSPACE) {
        send_code(KSC_BACKSPACE);
    } else if (code == KSC_FOCUS_LEFT || code == KSC_FOCUS_RIGHT) {
        send_code(code);
        unified_step(header_doc, bottom_doc, code == KSC_FOCUS_LEFT ? -1 : 1);
    } else if (code >= KSC_HQ_HEADER_BASE && code < KSC_HQ_HEADER_BASE + 16) {
        send_code(code);
    } else if (code >= 0x20 && code < 0x7f) {
        send_code(code);
    }
}

static long g_relay_cursor = -1;

static void livedesk_relay_path(const char *house_root, char *out, size_t sz) {
    snprintf(out, sz, "%s/#.desktop/livedesk_agent_relay.txt", house_root);
}

static int poll_agent_relay(const char *house_root, LayDoc *header_doc, LayDoc *bottom_doc, const SpState *st) {
    char path[KTB_PATH_BUF];
    livedesk_relay_path(house_root, path, sizeof(path));
    struct stat stt;
    if (stat(path, &stt) != 0) return 0;
    if (g_relay_cursor < 0) { g_relay_cursor = stt.st_size; return 0; }
    if (stt.st_size < g_relay_cursor) { g_relay_cursor = stt.st_size; return 0; }
    if (stt.st_size == g_relay_cursor) return 0;
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    fseek(f, g_relay_cursor, SEEK_SET);
    char line[32];
    long consumed = g_relay_cursor;
    int n_dispatched = 0;
    while (fgets(line, sizeof(line), f)) {
        char *nl = strchr(line, '\n');
        if (!nl) break;
        *nl = '\0';
        long here = ftell(f);
        int code = atoi(line);
        if (code > 0) { dispatch_key_code(header_doc, bottom_doc, st, code); n_dispatched++; }
        consumed = here;
    }
    fclose(f);
    g_relay_cursor = consumed;
    return n_dispatched;
}

#define SP_MAX_HIT 96
typedef struct {
    int x0, y0, x1, y1;
    int elidx;
} SpHitRect;

static SpHitRect g_header_hits[SP_MAX_HIT];
static int g_header_hit_n = 0;
static int g_header_row_w = 0;
static int g_hq_win_w = 0;
static int g_header_natural_w = 0;

static SpHitRect g_popup_hits[SP_MAX_HIT];
static int g_popup_hit_n = 0;

static SpHitRect g_bottom_hits[SP_MAX_HIT];
static int g_bottom_hit_n = 0;

#define STRIP_NAV_BOX_W 64

static Colormap g_strip_cmap = 0;
static XftFont *g_strip_font = NULL;
static XftDraw *g_hq_xft = NULL, *g_popup_xft = NULL, *g_strip_xft = NULL;

static XftFont *strip_font(Display *dpy, int screen) {
    if (g_strip_font) return g_strip_font;
    g_strip_font = XftFontOpenName(dpy, screen, "Noto Sans CJK SC:pixelsize=13");
    if (!g_strip_font) g_strip_font = XftFontOpenName(dpy, screen, "DejaVu Sans:pixelsize=13");
    return g_strip_font;
}

static XftColor strip_xft_color(Display *dpy, int screen, unsigned long pixel) {
    XftColor xc;
    XColor qc; qc.pixel = pixel;
    XQueryColor(dpy, g_strip_cmap ? g_strip_cmap : DefaultColormap(dpy, screen), &qc);
    XRenderColor rc = { qc.red, qc.green, qc.blue, 0xffff };
    XftColorAllocValue(dpy, DefaultVisual(dpy, screen), g_strip_cmap ? g_strip_cmap : DefaultColormap(dpy, screen), &rc, &xc);
    return xc;
}

static void strip_draw_utf8(Display *dpy, int screen, XftDraw *xftd, unsigned long fg_pixel,
                             int x, int y, const char *s, int len) {
    if (!xftd || !s || len <= 0) return;
    XftFont *f = strip_font(dpy, screen);
    if (!f) return;
    XftColor col = strip_xft_color(dpy, screen, fg_pixel);
    XftDrawStringUtf8(xftd, &col, f, x, y, (const FcChar8 *)s, len);
    XftColorFree(dpy, DefaultVisual(dpy, screen), g_strip_cmap ? g_strip_cmap : DefaultColormap(dpy, screen), &col);
}

static Pixmap g_hq_buf = 0, g_popup_buf = 0, g_strip_buf = 0;
static GC g_hq_buf_gc = 0, g_popup_buf_gc = 0, g_strip_buf_gc = 0;
static int g_hq_buf_h = 0, g_hq_buf_w = 0;
static int g_popup_buf_h = 0, g_popup_buf_w = 0;

static void present_rgb(Display *dpy, Pixmap buf, Window win, GC gc, int w, int h) {
    XSync(dpy, False);
    XImage *img = XGetImage(dpy, buf, 0, 0, (unsigned)w, (unsigned)h, AllPlanes, ZPixmap);
    if (!img) { XCopyArea(dpy, buf, win, gc, 0, 0, (unsigned)w, (unsigned)h, 0, 0); return; }
    XPutImage(dpy, win, gc, img, 0, 0, 0, 0, (unsigned)w, (unsigned)h);
    XDestroyImage(img);
}

static void present_rgb_fit(Display *dpy, Pixmap buf, Window win, GC gc, int bw, int bh, int ww) {
    if (ww >= bw || ww < 1 || bw < 1 || bh < 1) {
        present_rgb(dpy, buf, win, gc, bw, bh);
        return;
    }
    XSync(dpy, False);
    XImage *src = XGetImage(dpy, buf, 0, 0, (unsigned)bw, (unsigned)bh, AllPlanes, ZPixmap);
    if (!src) { XCopyArea(dpy, buf, win, gc, 0, 0, (unsigned)bw, (unsigned)bh, 0, 0); return; }
    Visual *vis = DefaultVisual(dpy, DefaultScreen(dpy));
    int depth = DefaultDepth(dpy, DefaultScreen(dpy));
    XImage *dst = XCreateImage(dpy, vis, depth, ZPixmap, 0, NULL,
                               (unsigned)ww, (unsigned)bh, 32, 0);
    if (!dst) { XPutImage(dpy, win, gc, src, 0, 0, 0, 0, (unsigned)bw, (unsigned)bh); XDestroyImage(src); return; }
    dst->data = malloc((size_t)dst->bytes_per_line * (size_t)bh);
    if (!dst->data) { XDestroyImage(dst); XPutImage(dpy, win, gc, src, 0, 0, 0, 0, (unsigned)bw, (unsigned)bh); XDestroyImage(src); return; }
    for (int y = 0; y < bh; y++) {
        const char *srow = src->data + (size_t)y * src->bytes_per_line;
        char *drow = dst->data + (size_t)y * dst->bytes_per_line;
        for (int x = 0; x < ww; x++) {
            int sx = (int)((long)x * (long)bw / (long)ww);
            memcpy(drow + (size_t)x * dst->bits_per_pixel / 8,
                   srow + (size_t)sx * src->bits_per_pixel / 8,
                   (size_t)dst->bits_per_pixel / 8);
        }
    }
    XPutImage(dpy, win, gc, dst, 0, 0, 0, 0, (unsigned)ww, (unsigned)bh);
    XDestroyImage(dst);
    XDestroyImage(src);
}

#define TAB_SPRITE_PX 24
typedef struct {
    char path[KTB_PATH_BUF];
    unsigned char *rgba;
    int res;
    time_t mtime;
} TabSprite;
static TabSprite g_sprite_cache[KTB_MAX_TABS];

static TabSprite *tab_sprite(const char *path) {
    if (!path || !path[0]) return NULL;
    char pth[KTB_PATH_BUF];
    snprintf(pth, sizeof(pth), "%s", path);
    size_t pl = strlen(pth);
    while (pl > 0 && (pth[pl - 1] == '\n' || pth[pl - 1] == '\r' || pth[pl - 1] == ' ' || pth[pl - 1] == '\t'))
        pth[--pl] = 0;
    if (!pth[0]) return NULL;
    char csv_path[KTB_PATH_BUF];
    snprintf(csv_path, sizeof(csv_path), "%s/sprite.csv", pth);
    struct stat st;
    time_t mt = 0;
    if (stat(csv_path, &st) == 0) mt = st.st_mtime;
    for (int i = 0; i < KTB_MAX_TABS; i++) {
        if (g_sprite_cache[i].rgba && strcmp(g_sprite_cache[i].path, pth) == 0) {
            if (mt != g_sprite_cache[i].mtime) {
                free(g_sprite_cache[i].rgba);
                memset(&g_sprite_cache[i], 0, sizeof(TabSprite));
                break;
            }
            return &g_sprite_cache[i];
        }
    }
    FILE *f = fopen(csv_path, "r");
    if (!f) return NULL;
    char line[256];
    int res = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "# resolution=", 13) == 0) { res = atoi(line + 13); break; }
    }
    if (res <= 0) { fclose(f); return NULL; }
    unsigned char *pixels = malloc((size_t)res * (size_t)res * 4);
    if (!pixels) { fclose(f); return NULL; }
    int count = 0;
    while (count < res * res && fgets(line, sizeof(line), f)) {
        int rr, gg, bb, aa;
        if (sscanf(line, "%d,%d,%d,%d", &rr, &gg, &bb, &aa) == 4) {
            pixels[count * 4 + 0] = (unsigned char)rr;
            pixels[count * 4 + 1] = (unsigned char)gg;
            pixels[count * 4 + 2] = (unsigned char)bb;
            pixels[count * 4 + 3] = (unsigned char)aa;
            count++;
        }
    }
    fclose(f);
    if (count != res * res) { free(pixels); return NULL; }
    for (int i = 0; i < KTB_MAX_TABS; i++) {
        if (!g_sprite_cache[i].rgba) {
            snprintf(g_sprite_cache[i].path, sizeof(g_sprite_cache[i].path), "%s", pth);
            g_sprite_cache[i].rgba = pixels;
            g_sprite_cache[i].res = res;
            g_sprite_cache[i].mtime = mt;
            return &g_sprite_cache[i];
        }
    }
    free(pixels);
    return NULL;
}

static void blit_tab_sprite(Display *dpy, Drawable d, GC gc, TabSprite *sp,
                            int x0, int y0, int px, unsigned long bg_pixel) {
    Visual *vis = DefaultVisual(dpy, DefaultScreen(dpy));
    int depth = DefaultDepth(dpy, DefaultScreen(dpy));
    unsigned long rmask = vis->red_mask, gmask = vis->green_mask, bmask = vis->blue_mask;
    int rshift = 0, gshift = 0, bshift = 0;
    while (rmask && !(rmask & (1UL << rshift))) rshift++;
    while (gmask && !(gmask & (1UL << gshift))) gshift++;
    while (bmask && !(bmask & (1UL << bshift))) bshift++;
    unsigned long br = (bg_pixel >> rshift) & 0xff;
    unsigned long bg2 = (bg_pixel >> gshift) & 0xff;
    unsigned long bb = (bg_pixel >> bshift) & 0xff;
    int res = sp->res;
    unsigned char *buf = calloc((size_t)px * px, 4);
    if (!buf) return;
    for (int y = 0; y < px; y++) {
        int sy = (y * res) / px;
        if (sy >= res) sy = res - 1;
        for (int x = 0; x < px; x++) {
            int sx = (x * res) / px;
            if (sx >= res) sx = res - 1;
            const unsigned char *pix = &sp->rgba[(sy * res + sx) * 4];
            int a = pix[3];
            int r = (pix[0] * a + (int)br * (255 - a)) / 255;
            int g = (pix[1] * a + (int)bg2 * (255 - a)) / 255;
            int b = (pix[2] * a + (int)bb * (255 - a)) / 255;
            unsigned long word = ((unsigned long)r << rshift) | ((unsigned long)g << gshift) | ((unsigned long)b << bshift);
            buf[(y * px + x) * 4 + 0] = (unsigned char)(word & 0xff);
            buf[(y * px + x) * 4 + 1] = (unsigned char)((word >> 8) & 0xff);
            buf[(y * px + x) * 4 + 2] = (unsigned char)((word >> 16) & 0xff);
            buf[(y * px + x) * 4 + 3] = (unsigned char)((word >> 24) & 0xff);
        }
    }
    XImage *img = XCreateImage(dpy, vis, depth, ZPixmap, 0, (char *)buf, px, px, 32, 0);
    if (img) {
        img->byte_order = LSBFirst;
        XPutImage(dpy, d, gc, img, 0, 0, x0, y0, px, px);
        XDestroyImage(img);
    } else {
        free(buf);
    }
}

static int format_cell(LayDoc *doc, int idx, int nav_n, char *out, size_t outsz) {
    char label[LAY_LABEL_LEN];
    lay_get_label(doc, idx, sp_get_var, &g_st, label, sizeof(label));
    if (nav_n > 0)
        snprintf(out, outsz, "%s %d. %s", lay_cursor_prefix(doc, idx), nav_n, label);
    else
        snprintf(out, outsz, "%s %s", lay_cursor_prefix(doc, idx), label);
    int w = (int)strlen(out) * 8 + 20;
    char sprite_path[LAY_SPRITE_LEN];
    lay_get_sprite(doc, idx, sp_get_var, &g_st, sprite_path, sizeof(sprite_path));
    if (sprite_path[0]) w += TAB_SPRITE_PX + 6;
    if (w < 40) w = 40;
    return w;
}

#define CELLS_MAX 128
#define CELL_CH_MAX 192
typedef struct { int idx; int focused; char region[8]; char ch[CELL_CH_MAX]; } CellRec;
static CellRec g_cells[CELLS_MAX];
static int g_cells_n = 0;

static void cell_append(int idx, const char *region, const char *ch, int focused) {
    if (g_cells_n >= CELLS_MAX) return;
    CellRec *c = &g_cells[g_cells_n++];
    c->idx = idx;
    c->focused = focused;
    snprintf(c->region, sizeof(c->region), "%s", region);
    snprintf(c->ch, sizeof(c->ch), "%s", ch);
}

static void flush_cells_pdl(const char *house_root) {
    char tmp[KTB_PATH_BUF], dst[KTB_PATH_BUF];
    path_join2(tmp, sizeof(tmp), house_root, "#.desktop/strip_frame.cells.pdl.tmp");
    path_join2(dst, sizeof(dst), house_root, "#.desktop/strip_frame.cells.pdl");
    FILE *f = fopen(tmp, "w");
    if (!f) return;
    for (int i = 0; i < g_cells_n; i++) {
        CellRec *c = &g_cells[i];
        fprintf(f, "CELL | idx=%d region=%s | ch=%s fg=default bg=default focused=%d\n",
                c->idx, c->region, c->ch, c->focused);
    }
    fclose(f);
    rename(tmp, dst);
    char sig[KTB_PATH_BUF];
    path_join2(sig, sizeof(sig), house_root, "#.desktop/strip_cells_changed.txt");
    FILE *sf = fopen(sig, "a");
    if (sf) { fputc('x', sf); fclose(sf); }
}

static int header_total_width(LayDoc *doc) {
    int w = STRIP_NAV_BOX_W;
    int n = 0;
    for (int i = 0; i < doc->element_count; i++) {
        if (doc->elements[i].parent_index != -1) continue;
        if (strcmp(doc->elements[i].type, "button") != 0) continue;
        n++;
        char disp[96];
        w += format_cell(doc, i, n, disp, sizeof(disp)) + 6;
    }
    for (int i = 0; i < doc->element_count; i++) {
        if (doc->elements[i].parent_index != -1) continue;
        if (strcmp(doc->elements[i].type, "text") != 0) continue;
        char label[LAY_LABEL_LEN];
        lay_get_label(doc, i, sp_get_var, &g_st, label, sizeof(label));
        w += (int)strlen(label) * 8 + 20 + TAB_SPRITE_PX + 4;
        break;
    }
    return w;
}

static void draw_header_win(Display *dpy, Window win, GC gc, LayDoc *doc, SpState *st) {
    int h = KTB_BAR_H;
    int header_w = STRIP_NAV_BOX_W;
    g_header_hit_n = 0;

    Window win_real = win;
    int w_guess = header_total_width(doc);
    if (w_guess < 1) w_guess = 1;
    if (!g_hq_buf || g_hq_buf_h < h || g_hq_buf_w < w_guess) {
        if (g_hq_buf) { XFreePixmap(dpy, g_hq_buf); XFreeGC(dpy, g_hq_buf_gc); }
        if (g_hq_xft) { XftDrawDestroy(g_hq_xft); g_hq_xft = NULL; }
        g_hq_buf = XCreatePixmap(dpy, win, w_guess, h, DefaultDepth(dpy, DefaultScreen(dpy)));
        g_hq_buf_gc = XCreateGC(dpy, g_hq_buf, 0, NULL);
        XCopyGC(dpy, gc, GCForeground | GCBackground | GCFont, g_hq_buf_gc);
        g_hq_buf_h = h;
        g_hq_buf_w = w_guess;
        g_hq_xft = XftDrawCreate(dpy, g_hq_buf, DefaultVisual(dpy, DefaultScreen(dpy)), g_strip_cmap ? g_strip_cmap : DefaultColormap(dpy, DefaultScreen(dpy)));
    }
    unsigned long fg, bg_pixel;
    { XGCValues gv; XGetGCValues(dpy, gc, GCForeground, &gv); fg = gv.foreground; }
    { XGCValues gv; XGetGCValues(dpy, gc, GCBackground, &gv); bg_pixel = gv.background; }
    GC bgc = g_hq_buf_gc;
    XSetForeground(dpy, bgc, bg_pixel);
    XFillRectangle(dpy, g_hq_buf, bgc, 0, 0, w_guess, h);
    XSetForeground(dpy, bgc, fg);
    XDrawRectangle(dpy, g_hq_buf, bgc, 0, 0, w_guess - 1, h - 1);

    {
        char arm_lab[72];
        const char *focus_mark = g_has_real_focus ? "^" : " ";
        if (st->nav_armed || st->digit_buf[0]) {
            const char *arm = st->digit_buf[0] ? st->digit_buf : "NAV";
            snprintf(arm_lab, sizeof(arm_lab), "%s[%s]", focus_mark, arm);
        } else {
            snprintf(arm_lab, sizeof(arm_lab), "%s", focus_mark);
        }
        strip_draw_utf8(dpy, DefaultScreen(dpy), g_hq_xft, fg, 4, KTB_BAR_H / 2 + 4, arm_lab, (int)strlen(arm_lab));
    }
    XDrawLine(dpy, g_hq_buf, bgc, STRIP_NAV_BOX_W, 0, STRIP_NAV_BOX_W, KTB_BAR_H);

    int first = 1;
    int cell_n = 0;
    for (int i = 0; i < doc->element_count; i++) {
        if (doc->elements[i].parent_index != -1) continue;
        if (strcmp(doc->elements[i].type, "button") != 0) continue;
        cell_n++;
        if (!first) XDrawLine(dpy, g_hq_buf, bgc, header_w, 0, header_w, KTB_BAR_H);
        first = 0;
        char disp[96];
        int cw = format_cell(doc, i, cell_n, disp, sizeof(disp));
        char sprite_path[LAY_SPRITE_LEN];
        lay_get_sprite(doc, i, sp_get_var, &g_st, sprite_path, sizeof(sprite_path));
        TabSprite *sp = sprite_path[0] ? tab_sprite(sprite_path) : NULL;
        int text_x = header_w + 6;
        if (sp) {
            blit_tab_sprite(dpy, g_hq_buf, bgc, sp, header_w + 4, (KTB_BAR_H - TAB_SPRITE_PX) / 2, TAB_SPRITE_PX, bg_pixel);
            text_x = header_w + TAB_SPRITE_PX + 10;
        }
        strip_draw_utf8(dpy, DefaultScreen(dpy), g_hq_xft, fg, text_x, KTB_BAR_H / 2 + 4, disp, (int)strlen(disp));
        cell_append(cell_n - 1, "header", disp, (i == doc->focus_index));
        if (g_header_hit_n < SP_MAX_HIT) {
            SpHitRect *r = &g_header_hits[g_header_hit_n++];
            r->x0 = header_w; r->x1 = header_w + cw; r->y0 = 0; r->y1 = KTB_BAR_H; r->elidx = i;
        }
        header_w += cw + 6;
    }
    for (int i = 0; i < doc->element_count; i++) {
        if (doc->elements[i].parent_index != -1) continue;
        if (strcmp(doc->elements[i].type, "text") != 0) continue;
        char label[LAY_LABEL_LEN];
        lay_get_label(doc, i, sp_get_var, &g_st, label, sizeof(label));
        int text_x = header_w - 24;
        if (text_x < header_w - (int)strlen(label) * 8 - 20) text_x = header_w - (int)strlen(label) * 8 - 20;
        strip_draw_utf8(dpy, DefaultScreen(dpy), g_hq_xft, fg, text_x, KTB_BAR_H / 2 + 4, label, (int)strlen(label));
        int adv = (int)strlen(label) * 8 + 20 - 24;
        header_w += adv;
        if (header_w < STRIP_NAV_BOX_W) header_w = STRIP_NAV_BOX_W;
        TabSprite *usp = g_build_uid_sprite_dir[0] ? tab_sprite(g_build_uid_sprite_dir) : NULL;
        if (usp) {
            blit_tab_sprite(dpy, g_hq_buf, bgc, usp, header_w, (KTB_BAR_H - TAB_SPRITE_PX) / 2, TAB_SPRITE_PX, bg_pixel);
            header_w += TAB_SPRITE_PX + 4;
        }
        break;
    }
    g_header_row_w = header_w;
    g_header_natural_w = w_guess;

    present_rgb_fit(dpy, g_hq_buf, win_real, g_hq_buf_gc, w_guess, h, g_hq_win_w);
    XFlush(dpy);
}

static int draw_popup_win(Display *dpy, Window win, GC gc, LayDoc *doc, SpState *st,
                           int hq_win_x, int hq_win_y) {
    g_popup_hit_n = 0;
    if (doc->active_index < 0) return 0;

    int is_cliio = (strcmp(doc->elements[doc->active_index].type, "cli_io") == 0);
    int rows = 0;
    int row_elidx[KTB_LIVEDESK_DYN_MAX];
    if (is_cliio) {
        if (!st->cliio_active) return 0;
        rows = 1;
    } else {
        for (int i = 0; i < doc->element_count && rows < KTB_LIVEDESK_DYN_MAX; i++) {
            if (i == doc->active_index) continue;
            if (!lay_is_navigable(doc, i)) continue;
            if (!lay_is_descendant(doc, i, doc->active_index)) continue;
            row_elidx[rows++] = i;
        }
        if (rows == 0) return 0;
    }

    int h = KTB_BAR_H * rows;
    int w = 40, anchor_x0 = 0;
    for (int i = 0; i < g_header_hit_n; i++) {
        if (g_header_hits[i].elidx == doc->active_index) { anchor_x0 = g_header_hits[i].x0; break; }
    }
    if (is_cliio) {
        w = 300;
    } else {
        for (int r = 0; r < rows; r++) {
            char lab[192];
            char label[LAY_LABEL_LEN];
            lay_get_label(doc, row_elidx[r], sp_get_var, &g_st, label, sizeof(label));
            snprintf(lab, sizeof(lab), "%s %d. %s", lay_cursor_prefix(doc, row_elidx[r]), r + 1, label);
            int cw = (int)strlen(lab) * 8 + 24;
            char sprite_path[LAY_SPRITE_LEN];
            lay_get_sprite(doc, row_elidx[r], sp_get_var, &g_st, sprite_path, sizeof(sprite_path));
            if (sprite_path[0]) cw += TAB_SPRITE_PX + 4;
            if (cw > w) w = cw;
        }
    }
    int win_x, win_y;
    if (is_cliio) {
        int sw = DisplayWidth(dpy, DefaultScreen(dpy));
        win_x = (sw - w) / 2;
        win_y = 140;
    } else {
        win_x = hq_win_x + anchor_x0;
        win_y = hq_win_y + KTB_BAR_H;
        if (g_hq_win_handle) {
            Window dummy = 0;
            int hx = 0, hy = 0;
            unsigned hw = 0, hh = 0, bw = 0, depth = 0;
            if (XGetGeometry(dpy, g_hq_win_handle, &dummy, &hx, &hy, &hw, &hh, &bw, &depth) && hw > 0) {
                int layout_w = g_header_row_w > 0 ? g_header_row_w : (int)hw;
                win_x = hx + (int)((long)anchor_x0 * (long)hw / (long)layout_w);
                win_y = hy + (int)hh;
            }
        }
    }
    if (w < 1) w = 1;
    if (h < 1) h = 1;

    Window win_real = win;
    if (!g_popup_buf || g_popup_buf_h < h || g_popup_buf_w < w) {
        if (g_popup_buf) { XFreePixmap(dpy, g_popup_buf); XFreeGC(dpy, g_popup_buf_gc); }
        if (g_popup_xft) { XftDrawDestroy(g_popup_xft); g_popup_xft = NULL; }
        g_popup_buf = XCreatePixmap(dpy, win, w, h, DefaultDepth(dpy, DefaultScreen(dpy)));
        g_popup_buf_gc = XCreateGC(dpy, g_popup_buf, 0, NULL);
        XCopyGC(dpy, gc, GCForeground | GCBackground | GCFont, g_popup_buf_gc);
        g_popup_buf_h = h;
        g_popup_buf_w = w;
        g_popup_xft = XftDrawCreate(dpy, g_popup_buf, DefaultVisual(dpy, DefaultScreen(dpy)), g_strip_cmap ? g_strip_cmap : DefaultColormap(dpy, DefaultScreen(dpy)));
    }
    unsigned long fg, bg_pixel;
    { XGCValues gv; XGetGCValues(dpy, gc, GCForeground, &gv); fg = gv.foreground; }
    { XGCValues gv; XGetGCValues(dpy, gc, GCBackground, &gv); bg_pixel = gv.background; }
    GC bgc = g_popup_buf_gc;
    XSetForeground(dpy, bgc, bg_pixel);
    XFillRectangle(dpy, g_popup_buf, bgc, 0, 0, w, h);
    XSetForeground(dpy, bgc, fg);
    XDrawRectangle(dpy, g_popup_buf, bgc, 0, 0, w - 1, h - 1);

    if (is_cliio) {
        const char *pref = st->cliio_typing ? "[^]" : "[>]";
        char row0[320];
        if (st->cliio_typing)
            snprintf(row0, sizeof(row0), "%s %s: [%s_]  (Enter=save, Esc=cancel)", pref, st->cliio_label, st->cliio_buffer);
        else
            snprintf(row0, sizeof(row0), "%s %s: [%s]  (Enter=edit, Esc=cancel)", pref, st->cliio_label, st->cliio_buffer);
        strip_draw_utf8(dpy, DefaultScreen(dpy), g_popup_xft, fg, 8, KTB_BAR_H / 2 + 4, row0, (int)strlen(row0));
        cell_append(doc->active_index, "popup", row0, 1);
        if (g_popup_hit_n < SP_MAX_HIT) {
            SpHitRect *r = &g_popup_hits[g_popup_hit_n++];
            r->x0 = 0; r->x1 = w; r->y0 = 0; r->y1 = KTB_BAR_H; r->elidx = doc->active_index;
        }
    } else {
        for (int r = 0; r < rows; r++) {
            int row_y = KTB_BAR_H * r;
            if (r > 0) XDrawLine(dpy, g_popup_buf, bgc, 0, row_y, w, row_y);
            char label[LAY_LABEL_LEN];
            lay_get_label(doc, row_elidx[r], sp_get_var, &g_st, label, sizeof(label));
            char lab[192];
            snprintf(lab, sizeof(lab), "%s %d. %s", lay_cursor_prefix(doc, row_elidx[r]), r + 1, label);
            char sprite_path[LAY_SPRITE_LEN];
            lay_get_sprite(doc, row_elidx[r], sp_get_var, &g_st, sprite_path, sizeof(sprite_path));
            TabSprite *rsp = sprite_path[0] ? tab_sprite(sprite_path) : NULL;
            int text_x = 12;
            if (rsp) {
                int sy = row_y + (KTB_BAR_H - TAB_SPRITE_PX) / 2;
                blit_tab_sprite(dpy, g_popup_buf, bgc, rsp, 8, sy, TAB_SPRITE_PX, bg_pixel);
                text_x = 8 + TAB_SPRITE_PX + 6;
            }
            strip_draw_utf8(dpy, DefaultScreen(dpy), g_popup_xft, fg, text_x, row_y + KTB_BAR_H / 2 + 4, lab, (int)strlen(lab));
            cell_append(row_elidx[r], "popup", lab, (row_elidx[r] == doc->focus_index));
            if (g_popup_hit_n < SP_MAX_HIT) {
                SpHitRect *hr = &g_popup_hits[g_popup_hit_n++];
                hr->x0 = 0; hr->x1 = w; hr->y0 = row_y; hr->y1 = row_y + KTB_BAR_H; hr->elidx = row_elidx[r];
            }
        }
    }

    XMoveResizeWindow(dpy, win_real, win_x, win_y, w, h);
    present_rgb(dpy, g_popup_buf, win_real, g_popup_buf_gc, w, h);
    XFlush(dpy);
    return 1;
}

static void draw_bottom(Display *dpy, Window win, GC gc, int sw, unsigned long bg_pixel, LayDoc *doc) {
    g_bottom_hit_n = 0;
    Window win_real = win;
    if (!g_strip_buf) {
        g_strip_buf = XCreatePixmap(dpy, win, sw, KTB_BAR_H, DefaultDepth(dpy, DefaultScreen(dpy)));
        g_strip_buf_gc = XCreateGC(dpy, g_strip_buf, 0, NULL);
        XCopyGC(dpy, gc, GCForeground | GCBackground | GCFont, g_strip_buf_gc);
        g_strip_xft = XftDrawCreate(dpy, g_strip_buf, DefaultVisual(dpy, DefaultScreen(dpy)), g_strip_cmap ? g_strip_cmap : DefaultColormap(dpy, DefaultScreen(dpy)));
    }
    unsigned long fg;
    { XGCValues gv; XGetGCValues(dpy, gc, GCForeground, &gv); fg = gv.foreground; }
    GC bgc = g_strip_buf_gc;
    XSetForeground(dpy, bgc, bg_pixel);
    XFillRectangle(dpy, g_strip_buf, bgc, 0, 0, sw, KTB_BAR_H);
    XSetForeground(dpy, bgc, fg);
    win = g_strip_buf;

    XDrawLine(dpy, win, bgc, 0, 0, sw, 0);

    int x = 8;
    for (int i = 0; i < doc->element_count; i++) {
        if (doc->elements[i].parent_index != -1) continue;
        if (strcmp(doc->elements[i].type, "row") != 0) continue;
        for (int c = 0; c < doc->elements[i].num_children; c++) {
            int ci = doc->elements[i].children[c];
            if (strcmp(doc->elements[ci].type, "button") != 0) continue;
            char label[LAY_LABEL_LEN];
            lay_get_label(doc, ci, sp_get_var, &g_st, label, sizeof(label));
            char lab[192];
            snprintf(lab, sizeof(lab), "%s %s", lay_cursor_prefix(doc, ci), label);
            char sprite_path[LAY_SPRITE_LEN];
            lay_get_sprite(doc, ci, sp_get_var, &g_st, sprite_path, sizeof(sprite_path));
            TabSprite *sp = tab_sprite(sprite_path);
            int w = (int)strlen(lab) * 8 + 16 + (sp ? TAB_SPRITE_PX : 0);
            if (x + w >= sw - 8) break;
            XDrawLine(dpy, win, bgc, x, 0, x, KTB_BAR_H);
            int text_x = x + 8;
            if (sp) {
                int sy = (KTB_BAR_H - TAB_SPRITE_PX) / 2;
                blit_tab_sprite(dpy, win, bgc, sp, x + 2, sy, TAB_SPRITE_PX, bg_pixel);
                text_x = x + 8 + TAB_SPRITE_PX;
            }
            strip_draw_utf8(dpy, DefaultScreen(dpy), g_strip_xft, fg, text_x, KTB_BAR_H / 2 + 4, lab, (int)strlen(lab));
            cell_append(ci, "bottom", lab, (ci == doc->focus_index));
            if (g_bottom_hit_n < SP_MAX_HIT) {
                SpHitRect *r = &g_bottom_hits[g_bottom_hit_n++];
                r->x0 = x; r->x1 = x + w; r->y0 = 0; r->y1 = KTB_BAR_H; r->elidx = ci;
            }
            x += w;
        }
    }

    present_rgb(dpy, g_strip_buf, win_real, g_strip_buf_gc, sw, KTB_BAR_H);
    XFlush(dpy);
}

static int sp_hit_test(SpHitRect *hits, int n, int x, int y) {
    for (int i = 0; i < n; i++) {
        if (x >= hits[i].x0 && x < hits[i].x1 && y >= hits[i].y0 && y < hits[i].y1)
            return hits[i].elidx;
    }
    return -1;
}

static void layout_path(char *out, size_t n, const char *filename) {
    char rel[256];
    snprintf(rel, sizeof(rel), "*.monads/*.livedesk-taskbar/%s", filename);
    path_join2(out, n, g_house_root, rel);
}

static void write_header_cell_ids(const LayDoc *header_doc) {
    char path[KTB_PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/livedesk_header_cell_ids.txt", g_house_root);
    FILE *f = fopen(path, "w");
    if (!f) return;
    int pos = 0;
    for (int i = 0; i < header_doc->element_count; i++) {
        if (!lay_is_navigable(header_doc, i)) continue;
        pos++;
        const char *id = lay_get_id(header_doc, i);
        if (id && id[0]) fprintf(f, "%d|%s\n", pos, id);
    }
    fclose(f);
}

static void sync_cliio_scope(LayDoc *header_doc, SpState *st) {
    int cliio_idx = -1;
    for (int i = 0; i < header_doc->element_count; i++) {
        if (strcmp(header_doc->elements[i].type, "cli_io") == 0) { cliio_idx = i; break; }
    }
    if (cliio_idx < 0) return;
    if (st->cliio_active && header_doc->active_index != cliio_idx) {
        lay_activate(header_doc, cliio_idx);
    } else if (!st->cliio_active && header_doc->active_index == cliio_idx) {
        lay_back(header_doc);
    }
}

static char g_popup_focus_last_onclick[LAY_ONCLICK_LEN] = "";

static void sync_popup_focus(LayDoc *header_doc, const SpState *st) {
    if (header_doc->active_index < 0) { g_popup_focus_last_onclick[0] = '\0'; return; }
    if (strcmp(header_doc->elements[header_doc->active_index].type, "cli_io") == 0) return;
    const char *cur_onclick = header_doc->elements[header_doc->active_index].onClick;
    if (!st->hq_open) {
        if (strcmp(cur_onclick, g_popup_focus_last_onclick) == 0) {
            lay_back(header_doc);
            g_popup_focus_last_onclick[0] = '\0';
            return;
        }
    }
    snprintf(g_popup_focus_last_onclick, sizeof(g_popup_focus_last_onclick), "%s", cur_onclick);
    int n = 0, target = -1;
    for (int i = 0; i < header_doc->element_count; i++) {
        if (i == header_doc->active_index) continue;
        if (!lay_is_navigable(header_doc, i)) continue;
        if (!lay_is_descendant(header_doc, i, header_doc->active_index)) continue;
        if (n == st->hq_focus) { target = i; break; }
        n++;
    }
    if (target >= 0) header_doc->focus_index = target;
}

static void apply_captured_key(LayDoc *header_doc, LayDoc *bottom_doc, SpState *st, int code) {
    if (code == KSC_FOCUS_LEFT) {
        if (header_doc->active_index != -1) lay_focus_delta(header_doc, -1);
        else unified_step(header_doc, bottom_doc, -1);
    } else if (code == KSC_FOCUS_RIGHT) {
        if (header_doc->active_index != -1) lay_focus_delta(header_doc, 1);
        else unified_step(header_doc, bottom_doc, 1);
    } else if (code == KSC_ENTER || code == KSC_ESCAPE || code == KSC_BACKSPACE ||
               (code >= 0x20 && code < 0x7f)) {
        dispatch_key_code(header_doc, bottom_doc, st, code);
    }
}

static void apply_captured_mouse(LayDoc *header_doc, LayDoc *bottom_doc, SpState *st,
                                  Display *dpy, Window hq_win, Window popup_win, Window win,
                                  const char *window_name, int button, int x, int y) {
    if (strcmp(window_name, "hq_win") == 0) {
        if (button == 3) {
            if (!st->cliio_active) { send_code(KSC_NAV_ARM); g_nav_focus = 0; unified_apply(header_doc, bottom_doc); taskbar_soft_focus(dpy, hq_win); }
        } else {
            int hit_x = x;
            if (g_header_natural_w > 0 && g_hq_win_w > 0 && g_hq_win_w < g_header_natural_w)
                hit_x = (int)((long)x * (long)g_header_natural_w / (long)g_hq_win_w);
            int elidx = sp_hit_test(g_header_hits, g_header_hit_n, hit_x, y);
            if (elidx >= 0) {
                int activate = (!g_click_two_step) || (header_doc->focus_index == elidx);
                header_doc->focus_index = elidx;
                g_nav_focus = root_nav_pos_of(header_doc, elidx);
                if (activate) dispatch_onclick(header_doc, elidx);
                taskbar_soft_focus(dpy, hq_win);
            }
        }
    } else if (strcmp(window_name, "popup_win") == 0) {
        if (button == 3) {
            if (!st->cliio_active) { send_code(KSC_NAV_ARM); g_nav_focus = 0; unified_apply(header_doc, bottom_doc); taskbar_soft_focus(dpy, popup_win); }
        } else {
            int elidx = sp_hit_test(g_popup_hits, g_popup_hit_n, x, y);
            if (elidx >= 0) {
                if (strcmp(header_doc->elements[elidx].type, "cli_io") == 0) {
                    send_code(KSC_ENTER);
                } else {
                    int activate = (!g_click_two_step) || (header_doc->focus_index == elidx);
                    header_doc->focus_index = elidx;
                    if (activate) dispatch_onclick(header_doc, elidx);
                }
                taskbar_soft_focus(dpy, popup_win);
            }
        }
    } else if (strcmp(window_name, "win") == 0) {
        if (button == 3) {
            send_code(KSC_NAV_ARM);
            g_nav_focus = 0;
            unified_apply(header_doc, bottom_doc);
            taskbar_soft_focus(dpy, win);
        } else {
            int elidx = sp_hit_test(g_bottom_hits, g_bottom_hit_n, x, y);
            if (elidx >= 0) {
                int activate = (!g_click_two_step) || (bottom_doc->focus_index == elidx);
                bottom_doc->focus_index = elidx;
                g_nav_focus = root_nav_count(header_doc) + root_nav_pos_of(bottom_doc, elidx);
                if (activate) dispatch_onclick(bottom_doc, elidx);
            }
        }
    }
}

static long g_capture_cursor = -1;

static int poll_captured_input(LayDoc *header_doc, LayDoc *bottom_doc, SpState *st,
                                Display *dpy, Window hq_win, Window popup_win, Window win) {
    char path[KTB_PATH_BUF];
    path_join2(path, sizeof(path), g_house_root, "#.desktop/strip_input_history.txt");
    struct stat stt;
    if (stat(path, &stt) != 0) return 0;
    if (g_capture_cursor < 0) { g_capture_cursor = stt.st_size; return 0; }
    if (stt.st_size < g_capture_cursor) { g_capture_cursor = stt.st_size; return 0; }
    if (stt.st_size == g_capture_cursor) return 0;

    FILE *f = fopen(path, "r");
    if (!f) return 0;
    fseek(f, g_capture_cursor, SEEK_SET);
    char line[KTB_PATH_BUF];
    long consumed = g_capture_cursor;
    int n_applied = 0;
    while (fgets(line, sizeof(line), f)) {
        char *nl = strchr(line, '\n');
        if (!nl) break;
        *nl = '\0';
        long here = ftell(f);
        char *kp = strstr(line, "KEY_PRESSED: ");
        char *me = strstr(line, "MOUSE_EVENT: ");
        if (kp) {
            int code = atoi(kp + 13);
            if (code > 0) { apply_captured_key(header_doc, bottom_doc, st, code); n_applied++; }
        } else if (me) {
            int button = 0, x = 0, y = 0, is_press = 0;
            char wname[64] = "";
            if (sscanf(me + 13, "%d %d %d %d %63s", &button, &x, &y, &is_press, wname) == 5 && is_press) {
                apply_captured_mouse(header_doc, bottom_doc, st, dpy, hq_win, popup_win, win, wname, button, x, y);
                n_applied++;
            }
        }
        consumed = here;
    }
    fclose(f);
    g_capture_cursor = consumed;
    return n_applied;
}

/* Renamed from main() - see this whole merged block's own header
 * comment for the argc==2 dispatch that calls this from the real
 * main() below. */
static int strip_main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: khtpm_strip_parser <house_root>\n");
        return 1;
    }
    snprintf(g_house_root, sizeof(g_house_root), "%s", argv[1]);
    strip_load_click_two_step();
    frame_history_init();
    int hq_win_x = 0, hq_win_y = 40;
    load_strip_offset(&hq_win_x, &hq_win_y);

    signal(SIGTERM, strip_on_sigterm);
    signal(SIGINT, strip_on_sigterm);

    {
        char early_header_path[KTB_PATH_BUF];
        layout_path(early_header_path, sizeof(early_header_path), "khtpm_strip_header.chtpm");
        LayDoc early_doc;
        if (lay_load(&early_doc, early_header_path, NULL, NULL))
            write_header_cell_ids(&early_doc);
    }

    launch_manager();
    wait_for_manager_first_publish();

    Display *dpy = XOpenDisplay(NULL);
    g_dpy = dpy;
    if (dpy) XSetErrorHandler(ktb_strip_nonfatal_x_error);
    if (!dpy) {
        fprintf(stderr, "strip_parser: no display\n");
        cleanup_manager();
        return 1;
    }
    int sw = DisplayWidth(dpy, DefaultScreen(dpy));
    int sh = DisplayHeight(dpy, DefaultScreen(dpy));
    g_strip_cmap = DefaultColormap(dpy, DefaultScreen(dpy));

    load_state(&g_st);
    build_uid_init(g_build_uid, sizeof(g_build_uid), g_house_root);

    char header_path[KTB_PATH_BUF], bottom_path[KTB_PATH_BUF];
    layout_path(header_path, sizeof(header_path), "khtpm_strip_header.chtpm");
    layout_path(bottom_path, sizeof(bottom_path), "khtpm_strip_bottom.chtpm");

    static LayDoc header_doc, bottom_doc;
    if (!lay_load(&header_doc, header_path, sp_get_var, &g_st))
        fprintf(stderr, "strip_parser: failed to load %s\n", header_path);
    else
        write_header_cell_ids(&header_doc);
    if (!lay_load(&bottom_doc, bottom_path, sp_get_var, &g_st))
        fprintf(stderr, "strip_parser: failed to load %s\n", bottom_path);
    bottom_doc.focus_index = -1;
    unified_apply(&header_doc, &bottom_doc);
    sync_cliio_scope(&header_doc, &g_st);

    unsigned long bg = parse_color(dpy, g_st.theme_bg, BlackPixel(dpy, DefaultScreen(dpy)));
    unsigned long fg = parse_color(dpy, g_st.theme_fg, WhitePixel(dpy, DefaultScreen(dpy)));

    XSetWindowAttributes swa;
    swa.background_pixel = bg;
    swa.event_mask = ExposureMask | ButtonPressMask | KeyPressMask | FocusChangeMask;
    Window win = XCreateWindow(dpy, RootWindow(dpy, DefaultScreen(dpy)),
                               0, sh - KTB_BAR_H, sw, KTB_BAR_H, 0,
                               CopyFromParent, InputOutput, CopyFromParent,
                               CWBackPixel | CWEventMask, &swa);
    taskbar_set_wm_class(dpy, win);
    taskbar_make_wm_managed_dock(dpy, win, 0, sh - KTB_BAR_H);
    XMapRaised(dpy, win);
    set_window_opacity(dpy, win, load_theme_opacity());
    GC gc = XCreateGC(dpy, win, 0, NULL);
    XSetForeground(dpy, gc, fg);
    XSetBackground(dpy, gc, bg);

    int hq_real_w = header_total_width(&header_doc);
    if (hq_real_w > sw - hq_win_x) hq_real_w = sw - hq_win_x;
    XSetWindowAttributes hq_swa;
    hq_swa.background_pixel = bg;
    hq_swa.event_mask = ExposureMask | ButtonPressMask | KeyPressMask | FocusChangeMask;
    Window hq_win = XCreateWindow(dpy, RootWindow(dpy, DefaultScreen(dpy)),
                                  hq_win_x, hq_win_y, hq_real_w, KTB_BAR_H, 0,
                                  CopyFromParent, InputOutput, CopyFromParent,
                                  CWBackPixel | CWEventMask, &hq_swa);
    g_hq_win_handle = hq_win;
    g_hq_win_w = hq_real_w;
    taskbar_set_wm_class(dpy, hq_win);
    taskbar_make_wm_managed_dock(dpy, hq_win, hq_win_x, hq_win_y);
    XMapRaised(dpy, hq_win);
    set_window_opacity(dpy, hq_win, load_theme_opacity());

    XSetWindowAttributes popup_swa;
    popup_swa.background_pixel = bg;
    popup_swa.event_mask = ExposureMask | ButtonPressMask | KeyPressMask | FocusChangeMask;
    Window popup_win = XCreateWindow(dpy, RootWindow(dpy, DefaultScreen(dpy)),
                                     hq_win_x, hq_win_y + KTB_BAR_H, 200, KTB_BAR_H, 0,
                                     CopyFromParent, InputOutput, CopyFromParent,
                                     CWBackPixel | CWEventMask, &popup_swa);
    taskbar_set_wm_class(dpy, popup_win);
    taskbar_make_wm_managed_dock(dpy, popup_win, hq_win_x, hq_win_y + KTB_BAR_H);
    set_window_opacity(dpy, popup_win, load_theme_opacity());
    int popup_mapped = 0;

    g_cells_n = 0;
    draw_bottom(dpy, win, gc, sw, bg, &bottom_doc);
    draw_header_win(dpy, hq_win, gc, &header_doc, &g_st);
    if (draw_popup_win(dpy, popup_win, gc, &header_doc, &g_st, hq_win_x, hq_win_y)) {
        XMapRaised(dpy, popup_win);
        popup_mapped = 1;
        draw_popup_win(dpy, popup_win, gc, &header_doc, &g_st, hq_win_x, hq_win_y);
    }
    if (g_cells_n > 0) flush_cells_pdl(g_house_root);

    taskbar_soft_focus(dpy, win);

    XFlush(dpy);
    usleep(200000);
    double relaunch_opacity = load_theme_opacity();
    set_window_opacity(dpy, win, relaunch_opacity);
    set_window_opacity(dpy, hq_win, relaunch_opacity);
    set_window_opacity(dpy, popup_win, relaunch_opacity);
    XFlush(dpy);

    int active_ticks = ACTIVE_HOLD_TICKS;
    while (g_strip_running) {
        g_cells_n = 0;
        reap_manager_nonblocking();

        if (strip_theme_changed_dirty()) {
            double new_opacity = load_theme_opacity();
            set_window_opacity(dpy, win, new_opacity);
            set_window_opacity(dpy, hq_win, new_opacity);
            if (popup_win) set_window_opacity(dpy, popup_win, new_opacity);
        }

        int was_dirty = frame_changed_dirty();
        if (was_dirty) {
            load_state(&g_st);
            lay_reload_preserving_scope(&header_doc, header_path, sp_get_var, &g_st);
            lay_reload_preserving_scope(&bottom_doc, bottom_path, sp_get_var, &g_st);
            if (header_doc.active_index == -1) unified_apply(&header_doc, &bottom_doc);
            sync_cliio_scope(&header_doc, &g_st);
            sync_popup_focus(&header_doc, &g_st);
            sync_focus_to_digit_buf(&header_doc, &bottom_doc, &g_st);

            append_frame_history(&header_doc, &bottom_doc, g_nav_focus);

            bg = parse_color(dpy, g_st.theme_bg, bg);
            fg = parse_color(dpy, g_st.theme_fg, fg);
            XSetWindowBackground(dpy, win, bg);
            XSetForeground(dpy, gc, fg);
            XSetBackground(dpy, gc, bg);
            draw_bottom(dpy, win, gc, sw, bg, &bottom_doc);
            XSetWindowBackground(dpy, hq_win, bg);
            draw_header_win(dpy, hq_win, gc, &header_doc, &g_st);
            XSetWindowBackground(dpy, popup_win, bg);
            if (draw_popup_win(dpy, popup_win, gc, &header_doc, &g_st, hq_win_x, hq_win_y)) {
                if (!popup_mapped) {
                    XMapRaised(dpy, popup_win);
                    popup_mapped = 1;
                    set_window_opacity(dpy, popup_win, load_theme_opacity());
                    draw_popup_win(dpy, popup_win, gc, &header_doc, &g_st, hq_win_x, hq_win_y);
                }
            } else if (popup_mapped) {
                XUnmapWindow(dpy, popup_win);
                popup_mapped = 0;
            }
        }

        int n_relay_dispatched = poll_agent_relay(g_house_root, &header_doc, &bottom_doc, &g_st);
        if (n_relay_dispatched > 0) {
            draw_bottom(dpy, win, gc, sw, bg, &bottom_doc);
            draw_header_win(dpy, hq_win, gc, &header_doc, &g_st);
            if (draw_popup_win(dpy, popup_win, gc, &header_doc, &g_st, hq_win_x, hq_win_y)) {
                if (!popup_mapped) {
                    XMapRaised(dpy, popup_win);
                    popup_mapped = 1;
                    set_window_opacity(dpy, popup_win, load_theme_opacity());
                    draw_popup_win(dpy, popup_win, gc, &header_doc, &g_st, hq_win_x, hq_win_y);
                }
            } else if (popup_mapped) {
                XUnmapWindow(dpy, popup_win);
                popup_mapped = 0;
            }
        }

        if (was_dirty || n_relay_dispatched > 0) active_ticks = ACTIVE_HOLD_TICKS;
        else if (active_ticks > 0) active_ticks--;
        int poll_interval = active_ticks > 0 ? POLL_INTERVAL_ACTIVE_USEC : POLL_INTERVAL_IDLE_USEC;

        if (g_cells_n > 0) flush_cells_pdl(g_house_root);
        {
            fd_set fds;
            FD_ZERO(&fds);
            int xfd = ConnectionNumber(dpy);
            FD_SET(xfd, &fds);
            struct timeval tv = { 0, poll_interval };
            select(xfd + 1, &fds, NULL, NULL, &tv);
        }

        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            if (ev.type == Expose) {
                if (ev.xexpose.count == 0) {
                    if (ev.xexpose.window == win) draw_bottom(dpy, win, gc, sw, bg, &bottom_doc);
                    else if (ev.xexpose.window == hq_win) draw_header_win(dpy, hq_win, gc, &header_doc, &g_st);
                    else if (ev.xexpose.window == popup_win) draw_popup_win(dpy, popup_win, gc, &header_doc, &g_st, hq_win_x, hq_win_y);
                }
            } else if (ev.type == ButtonPress && ev.xbutton.window == hq_win) {
                mirror_mouse_history("hq_win", ev.xbutton.button, ev.xbutton.x, ev.xbutton.y);
            } else if (ev.type == ButtonPress && ev.xbutton.window == popup_win) {
                mirror_mouse_history("popup_win", ev.xbutton.button, ev.xbutton.x, ev.xbutton.y);
            } else if (ev.type == ButtonPress && ev.xbutton.window == win) {
                mirror_mouse_history("win", ev.xbutton.button, ev.xbutton.x, ev.xbutton.y);
            } else if (ev.type == FocusIn) {
                g_has_real_focus = 1;
                g_focused_win = ev.xfocus.window;
            } else if (ev.type == FocusOut) {
                g_has_real_focus = 0;
                if (g_focused_win == ev.xfocus.window) g_focused_win = 0;
            } else if (ev.type == KeyPress) {
                KeySym ks = XLookupKeysym(&ev.xkey, 0);
                if (ks == XK_Left || ks == XK_Up) {
                    mirror_key_history(KSC_FOCUS_LEFT);
                } else if (ks == XK_Right || ks == XK_Down) {
                    mirror_key_history(KSC_FOCUS_RIGHT);
                } else if (ks == XK_Return) {
                    mirror_key_history(KSC_ENTER);
                } else if (ks == XK_Escape) {
                    mirror_key_history(KSC_ESCAPE);
                } else if (ks == XK_BackSpace) {
                    mirror_key_history(KSC_BACKSPACE);
                } else {
                    char buf[8] = {0};
                    KeySym ks2;
                    if (XLookupString(&ev.xkey, buf, sizeof(buf), &ks2, NULL) > 0) {
                        unsigned char c = (unsigned char)buf[0];
                        if (c >= 0x20 && c < 0x7f) mirror_key_history((int)c);
                    }
                }
            }
        }

        {
            int n_captured = poll_captured_input(&header_doc, &bottom_doc, &g_st, dpy, hq_win, popup_win, win);
            if (n_captured > 0) {
                draw_bottom(dpy, win, gc, sw, bg, &bottom_doc);
                draw_header_win(dpy, hq_win, gc, &header_doc, &g_st);
                if (draw_popup_win(dpy, popup_win, gc, &header_doc, &g_st, hq_win_x, hq_win_y)) {
                    if (!popup_mapped) {
                        XMapRaised(dpy, popup_win);
                        popup_mapped = 1;
                        set_window_opacity(dpy, popup_win, load_theme_opacity());
                        draw_popup_win(dpy, popup_win, gc, &header_doc, &g_st, hq_win_x, hq_win_y);
                    }
                } else if (popup_mapped) {
                    XUnmapWindow(dpy, popup_win);
                    popup_mapped = 0;
                }
            }
        }
    }

    XFreeGC(dpy, gc);
    XDestroyWindow(dpy, win);
    XDestroyWindow(dpy, hq_win);
    XDestroyWindow(dpy, popup_win);
    XCloseDisplay(dpy);
    cleanup_manager();
    return 0;
}
/* ============ end taskbar strip mode ============ */

/* ============================================================
 * BEGIN TILE MODE (real entity-window renderer), folded in verbatim
 * from tp_desktop_window_rgb.c, 2026-09-01 (real house-standard
 * consolidation, phase 2 - see strip_main()'s own big header comment
 * above for the phase-1 precedent and the exact "no linkers" rule this
 * follows: khtpm_strip_parser.c/.../tp_desktop_window_rgb.c relocated
 * VERBATIM into this one file/binary, dispatched by argv shape, never
 * cross-.c #include/linking).
 *
 * This mode's own real invocation shape is exactly <package_dir>,
 * argc==2 - SAME argc as strip mode (<house_root>), disambiguated in
 * the real main() below by checking for a real "#.desktop" directory
 * under argv[1] (present only for a real house_root, never for a tile/
 * pal's own package_dir) - see main()'s own dispatch comment.
 *
 * Real collisions resolved during this relocation (found via the same
 * comm -12 static-scan + build-verify technique phase 1 used):
 *   - main()              -> tp_main() (this mode's own entry point)
 *   - load_override_redirect(), set_window_opacity(), the globals
 *     g_override_redirect and g_click_two_step: BYTE-FOR-BYTE identical
 *     to this file's own pre-existing copies (confirmed by direct
 *     comparison) - reused as-is, tp_desktop_window_rgb.c's own
 *     duplicate copies dropped, not renamed.
 *   - load_theme_opacity(): NOT reused - this file's own existing
 *     load_theme_opacity() takes no args (reads the shared global
 *     g_house_root directly), while tp_desktop_window_rgb.c's own
 *     version took an explicit house_root parameter. Renamed to
 *     tp_load_theme_opacity() (kept as its own real function, same
 *     body) rather than rewriting every call site's calling
 *     convention - a real, deliberate, minimal-risk choice, not an
 *     oversight.
 *   - PATH_BUF (4352 here vs this file's own 4096): renamed to
 *     TP_PATH_BUF throughout this block, kept at its own original
 *     4352 value - reusing the smaller existing constant would have
 *     silently shrunk every one of this mode's own real path buffers.
 *   - tp_desktop_window_rgb.c's own local `char g_house_root[PATH_BUF]`
 *     (declared inside its own main()) dropped - tp_main() now writes
 *     directly into this file's shared global g_house_root, same as
 *     every other mode.
 * ============================================================ */
/* ========================================================================
 * 2026-08-06 FOCUS-RECOVERY — option C (user chose):
 *   locks OFF; grabs ON; soft focus ON (this popup only).
 * Flip any flag to experiment. Nav [N] UI + NAV_KEY file relay stay ON.
 * ======================================================================== */
#ifndef LIVEDESK_USE_REGISTRY_LOCK
#define LIVEDESK_USE_REGISTRY_LOCK 1
#endif
#ifndef LIVEDESK_USE_POPUP_LOCK
#define LIVEDESK_USE_POPUP_LOCK 0
#endif
#ifndef LIVEDESK_USE_XGRAB_POINTER
#define LIVEDESK_USE_XGRAB_POINTER 1
#endif
#ifndef LIVEDESK_USE_XGRAB_KEYBOARD
#define LIVEDESK_USE_XGRAB_KEYBOARD 1
#endif
/* REAL 2026-08-07, direct instruction ("make them configurable via
 * config / .pdl file so i can easily experiment with them"): the
 * context-menu behavior guards are now RUNTIME-configurable, read from
 * the package's own meta.pdl by read_menu_config() below:
 *   STATE | menu_stay_open | 1    outside/repeat clicks keep menu open
 *   STATE | grab_pointer   | 1    pointer grab while a menu is open
 *   STATE | grab_keyboard  | 1    keyboard grab while a menu is open
 * menu_stay_open=1 also makes the menu NON-modal (no pointer grab), so
 * the toolbar and other windows stay clickable while it's open - see
 * open_context_menu()'s grab block. Missing rows keep these compile-time
 * defaults. Edit meta.pdl and the next right-click re-reads it - no
 * rebuild, no restart. */
static int g_menu_stay_open = 1;
/* REAL, NEW 2026-08-30, direct instruction ("it only needs to happen
 * on status change... what in house architecture can be used to
 * support this") - same real cheap-marker convention this house
 * already uses everywhere (frame_changed.txt et al) - a single
 * stat() per already-running tick against
 * #.desktop/livedesk_theme_changed.txt (written by
 * write_theme_opacity() in khtpm_core_render.c), real work
 * (reload+reapply opacity to this entity's own window) only runs on
 * an actual change. */
static long g_theme_changed_cursor = 0;
static int theme_changed_dirty(const char *house_root) {
    char path[4352];
    snprintf(path, sizeof(path), "%s/#.desktop/livedesk_theme_changed.txt", house_root);
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    /* REAL BUG FIX 2026-08-30 - same fix as khtpm_strip_parser.c's own
     * theme_changed_dirty() - cursor starts at 0, not -1, so the
     * marker's first-ever real append (this file usually doesn't
     * exist yet at process startup) counts as a real change. */
    if (st.st_size != g_theme_changed_cursor) { g_theme_changed_cursor = st.st_size; return 1; }
    return 0;
}

/* REAL, NEW 2026-08-30, found live: an entity nobody is interacting
 * with never sets need_redraw, so the whole draw block (later in the
 * loop, gated `if (!need_redraw) continue;`) never runs - meaning a
 * desktop-wide camera pan/tilt/mode CHANGE, written by cursword alone,
 * was silently invisible on every OTHER idle entity until something
 * else happened to poke it. Same real cheap-marker convention as
 * theme_changed_dirty() just above (one stat() per already-running
 * idle tick, real work only on an actual change) - cursword's own
 * camera writers (below) touch this marker; every entity's own idle
 * tick checks it and sets need_redraw itself when it moves. */
static long g_camera_changed_cursor = 0;
static int camera_changed_dirty(const char *house_root) {
    char path[4352];
    snprintf(path, sizeof(path), "%s/#.desktop/desktop_camera_changed.txt", house_root);
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    if (st.st_size != g_camera_changed_cursor) { g_camera_changed_cursor = st.st_size; return 1; }
    return 0;
}
static void bump_camera_changed(const char *house_root) {
    char path[4352];
    snprintf(path, sizeof(path), "%s/#.desktop/desktop_camera_changed.txt", house_root);
    FILE *f = fopen(path, "a");
    if (f) { fputc('.', f); fclose(f); }
}

/* REAL, NEW 2026-08-30, direct instruction ("we actually want to have
 * a .pdl file that decides how to render emoji sprits in top down.
 * (from top or front as usual) lets view from front for now but later
 * will change when doing more camera stuff") - a real, live-editable
 * `emoji_sprite_view` key in this same shared hq_ui.pdl (same real
 * home as click_two_step/cursword_move_mode - a house-wide UI toggle,
 * not buried in cursword's own pal-scoped config). "front" (default)
 * is a straight-on yaw=0 camera - the classic real "topdown map, but
 * sprites/objects render front-facing" convention most real top-down
 * games actually use, and directly answers the earlier live report
 * that the previous fixed yaw=45 diagonal corner view looked "melted"/
 * unreasonable. "top" is the original diagonal corner view, kept as a
 * real, named alternative for later camera work, not deleted. */
static int g_emoji_sprite_view_top = 0; /* 0 = front (default), 1 = top */


static void desktop_load_click_two_step(const char *house_root) {
    char path[4352]; /* matches this file's own later TP_PATH_BUF (not yet declared at this point) */
    snprintf(path, sizeof(path), "%s/#.desktop/hq_ui.pdl", house_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *val = eq + 1;
        char *nl = strchr(val, '\n');
        if (nl) *nl = '\0';
        if (strcmp(line, "click_two_step") == 0) g_click_two_step = atoi(val) != 0;
        else if (strcmp(line, "emoji_sprite_view") == 0) g_emoji_sprite_view_top = (strcmp(val, "top") == 0);
    }
    fclose(f);
}
static int g_grab_pointer = LIVEDESK_USE_XGRAB_POINTER;
static int g_grab_keyboard = LIVEDESK_USE_XGRAB_KEYBOARD;
/* Soft focus fallback (option C 2026-08-06): set input focus on THIS
 * popup only — not a house-wide raise-fight across entity tiles.
 * Used on open + header click so keys still work if grab fails. */
#ifndef LIVEDESK_POPUP_SOFT_FOCUS
#define LIVEDESK_POPUP_SOFT_FOCUS 1
#endif

/* REAL FIX 2026-08-05, direct instruction (MUCHI_RANCHER's own real
 * monsters need a 2x2-desktop-grid-cell footprint - "these monsters
 * should take up 4 tiles instead of the previous 1 tile"): WIN_PX was
 * a compile-time #define, meaning every desktop entity was hardcoded
 * to the exact same 64px size. Converted to a real runtime variable,
 * set once early in main() from a real, optional "footprint_tiles"
 * STATE row in the package's own meta.pdl (same real SECTION|KEY|VALUE
 * parse convention every other real field there already uses) -
 * defaults to 1 (this exact same 64px value) when absent, so every
 * existing pet/asa/ava package is completely unaffected. Every one of
 * this file's own real call sites below still just reads "WIN_PX" -
 * only its OWN declaration changed, not the 9 real places it's used. */
static int WIN_PX = 64;
#define POLL_INTERVAL_USEC 300000
#define TP_PATH_BUF 4352
/* REAL FIX 2026-08-04, direct instruction ("desk has a grid... egg-pets
 * snap to grid... make sure windows/procs are killed and don't render
 * more than 30fps, cpu is getting hot"): same GRID_CELL_PX egg_window.c
 * uses (01.muchi-pals-🥚️-13.01/system/egg_window.c) - tile stamps should
 * snap to the SAME desktop grid egg-pals already use, not a separate
 * one.
 *
 * REAL FIX 2026-08-27 (TILE-SYSTEM-DESIGN.md §0a, direct instruction:
 * "this size should be set/read from a .pdl which can be changed"):
 * converted from a compile-time #define to a real runtime variable, set
 * once early in main() from an optional "GRID | cell_px | N" row in
 * #.desktop/desk_grid.pdl (see read_grid_cell_px() below) - same real
 * "compile-time constant -> runtime variable read from a real file,
 * safe default preserves existing behavior" pattern this file's own
 * WIN_PX/footprint_tiles conversion already established above. Defaults
 * to 80 (this file's own original hardcoded value) when the file/row is
 * absent, so every existing desktop is completely unaffected until
 * someone actually writes a real desk_grid.pdl. */
static int GRID_CELL_PX = 80;
#define MAX_FPS 30
#define MIN_FRAME_USEC (1000000 / MAX_FPS)

/* REAL, NEW 2026-08-30 (CURSWORD-DESKTOP-3D-AND-PIECECRAFT-INSCENE-
 * DESKS-DESIGN.md §3a/§9/§10, direct instruction confirmed across
 * several rounds of Q&A - see that doc's own real decision record
 * before touching any of this) - real arm-on-click for cursword
 * specifically, NOT every desktop entity: a plain click (not a drag)
 * arms it, showing a real glowing halo, same real visible-state
 * principle as this house's other "armed" conventions
 * (rmmv_armed.txt/.pal-hint-armed). This is step 1 of that doc's own
 * scoped rollout (arm+halo only) - arrow-key movement, click-to-place,
 * and the 2D/3D camera switch are explicitly deferred to a later pass,
 * per the doc's own §8/§10 sequencing. */
static int g_is_cursword = 0;
static int g_cursword_armed = 0;

/* Confirmed default (§9 item 1, confirmed as-is in §10): 5px movement
 * AND under 300ms between ButtonPress and ButtonRelease counts as a
 * real click (arm), not a drag. */
#define CURSWORD_CLICK_MAX_PX 5
/* REAL FIX 2026-08-30, direct live report ("clicking it with mouse
 * moves it to fast can it wait a bit longer?") - 300ms was too tight
 * for a real, physical mouse click (press+release), misclassifying it
 * as a drag (moving cursword) instead of a real click (arming it).
 * Raised to 600ms - still well under "held down and dragged" territory
 * (CURSWORD_CLICK_MAX_PX's own 5px cap still guards against an actual
 * drag being misread as a click, this only loosens the TIME side). */
#define CURSWORD_CLICK_MAX_MS 600

/* Real, house-standard "small state file under #.desktop/" convention
 * (§9 item 5's own cited precedent, rmmv_armed.txt) - the one real,
 * visible-elsewhere signal for "is cursword currently armed right
 * now," same shape khtpm_core_render.c's own
 * pchq_is_interact_on()/etc. already use for cross-process real state. */
static void cursword_write_armed(const char *house_root, int armed) {
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/cursword_armed.txt", house_root);
    FILE *f = fopen(path, "w");
    if (f) { fprintf(f, "%d\n", armed ? 1 : 0); fclose(f); }
}

/* REAL, NEW 2026-08-30, direct instruction ("i still dont have arrow
 * control. would it help if we did a text display under cursword with
 * pressed key history?"): a real, live-visible readout of the last
 * few keys this window's own event loop actually received while armed
 * - the direct, fastest way to tell "key never reached this window at
 * all" apart from "key reached it but the move logic didn't fire,"
 * without any indirect file/log inspection. Extends the window taller
 * by CURSWORD_LOG_H (only while armed - see cursword_update_shape()'s
 * own real shape-mask union for the matching visible-region rectangle)
 * and draws the last CURSWORD_LOG_N short labels on one line via the
 * existing popup fontset (load_popup_fontset(), already loaded
 * unconditionally in main() - not new state). */
/* REAL, NEW 2026-08-30, direct instruction ("can we do another debug
 * below sword, that shows camera angle?") - grown from 20 to 38 to
 * fit a real second line (the existing key-log line, plus a new
 * camera pitch/tilt readout right below it), then to 56 (direct
 * instruction, 2026-08-31: "zx cy aren't changing z level... can u
 * add another debug row for cursword that show xyz position") for a
 * real third line - see the real draw site near the end of the main
 * render block for what actually gets printed on each line. */
#define CURSWORD_LOG_H 56
#define CURSWORD_LOG_N 5
/* REAL, NEW 2026-08-31, direct live report ("its too far off the
 * label 2 read, widen label for text?") - the debug strip's own
 * visible-region rectangle and backing pixmap were always exactly
 * WIN_PX (64px) wide, same as the sprite square above it, so the
 * posline/camline/logline text (up to ~30 chars) ran straight off
 * the right edge of the strip's own clip region and got silently
 * clipped by the window shape - not a font/color bug, a real width
 * bug. Strip-only width, wider than WIN_PX; every WIN_PX x WIN_PX
 * square (sprite mask, disc mask, halo ring) is completely
 * unaffected - only the strip's own mask/pixmap/window-width/present-
 * width below use this. */
#define CURSWORD_LOG_W 220
static char g_cursword_log[CURSWORD_LOG_N][12];
static int g_cursword_log_n = 0;
static void cursword_log_key(const char *label) {
    if (g_cursword_log_n < CURSWORD_LOG_N) {
        snprintf(g_cursword_log[g_cursword_log_n], sizeof(g_cursword_log[0]), "%s", label);
        g_cursword_log_n++;
    } else {
        for (int i = 1; i < CURSWORD_LOG_N; i++)
            snprintf(g_cursword_log[i - 1], sizeof(g_cursword_log[0]), "%s", g_cursword_log[i]);
        snprintf(g_cursword_log[CURSWORD_LOG_N - 1], sizeof(g_cursword_log[0]), "%s", label);
    }
}

/* REAL, NEW 2026-08-30, step 2 of the design doc's own §8/§10
 * sequencing (arrow-key movement + click-to-place, both real code,
 * house-wide PDL toggle decides which is ACTIVE while armed - direct
 * instruction: "we could add it in a pdl as optionally changeable
 * till we figure out what actually works best in practice"). Same
 * real home as click_two_step/opacity/cursword_move_mode itself -
 * #.desktop/hq_ui.pdl, loaded once at startup, same shape every other
 * real loader in this house uses. 0 = click_place (default), 1 =
 * arrow_only. Arrow-key nudge is real, always-on baseline movement in
 * EITHER mode (§3a's own core spec never made arrows conditional) -
 * this toggle only decides whether click-to-place is ALSO active. */
static int g_cursword_click_place = 1;
static void cursword_load_move_mode(const char *house_root) {
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/hq_ui.pdl", house_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *val = eq + 1;
        val[strcspn(val, "\r\n")] = '\0';
        if (strcmp(line, "cursword_move_mode") == 0)
            g_cursword_click_place = (strcmp(val, "arrow_only") != 0);
    }
    fclose(f);
}

/* Real, one-shot "waiting for the placement click" state - set right
 * after a successful real XGrabPointer on arm (click_place mode only),
 * cleared on the next real ButtonPress (the placement click itself) or
 * on Escape (real abort/disarm, must also ungrab). */
static int g_cursword_awaiting_place = 0;

/* REAL FIX 2026-08-05, direct instruction ("this is where we will
 * refactor the xwindow to be chtpm/master ledger compliant" -
 * MUCHI_RANCHER's own work item 2, see MUCHI_RANCHER_DESIGN.md §5 and
 * TILE_PICKER_DESIGN.md §10's own 3-step parity plan): every other
 * CHTPM app in this house gets a real, plain-text history file
 * (auditability) and a real file an external writer - human, script,
 * or AI - can inject a command into, that this window's own event
 * loop actually polls (AI-injection power). Raw override_redirect
 * windows like this one had NEITHER until now. Scoped per-package
 * (own history.txt/interact_relay.txt inside package_dir, not a
 * shared house-wide file like pieces/keyboard/history.txt - this
 * window only ever represents ONE entity, so its own real audit trail
 * belongs right next to that entity's own other real state). */
static char g_history_path[TP_PATH_BUF];
static char g_relay_path[TP_PATH_BUF];

/* Stage 2c PROOF (2026-08-16, direct instruction: "we wanna wire that
 * new context to toolbar and right clik entity and get rid of legacy,
 * so i can check it") - ONE-ENTITY test, see local-2do-15.txt's own
 * entity-context-menu entry. Real integration point: every one of this
 * file's ~20 open_context_menu() call sites already funnels through
 * that ONE function - rather than touch all of them (real risk, this
 * popup engine is deeply coupled to lock/lifecycle bookkeeping every
 * caller assumes), open_context_menu() itself now HIDES the legacy
 * popup (XUnmapWindow, right after its own real creation/lock/grab
 * logic runs completely unchanged) and launches the new khtpm .chtpm-
 * based renderer as the VISIBLE replacement, only when this entity's
 * own <package_dir>/menu.chtpm exists. Every entity without a
 * menu.chtpm keeps the exact original behavior, zero risk. */
static int g_use_khtpm_menu = 0;
static char g_khtpm_menu_pkg_dir[TP_PATH_BUF] = "";
static char g_khtpm_menu_house_root[TP_PATH_BUF] = "";
static pid_t g_khtpm_menu_pid = -1;

static void launch_khtpm_menu(int px, int py) {
    /* kill-then-relaunch, same real single-instance convention every
     * khtpm app's own button.sh already uses - a page-nav GOTO could
     * call open_context_menu() again while a prior instance is still
     * up (real for objects.pdl-style multi-page menus, not exercised
     * by ava's own single-page menu.chtpm yet, but correct to guard
     * for now rather than after it's hit live). */
    if (g_khtpm_menu_pid > 0) {
        kill(g_khtpm_menu_pid, SIGTERM);
        waitpid(g_khtpm_menu_pid, NULL, WNOHANG);
        g_khtpm_menu_pid = -1;
    }
    char bin_path[TP_PATH_BUF];
    snprintf(bin_path, sizeof(bin_path), "%s/*.monads/*.livedesk-taskbar/ops/+x/khtpm_core_render.+x", g_khtpm_menu_house_root);
    /* REAL Stage 5 step 3/4 (2026-08-16, khtpm-merge-how2.md §5d.3) -
     * real, unified <house_root> <chtpm_path> [x] [y] contract (was
     * <package_dir> <house_root> [x] [y]) - khtpm_core_render's
     * own main() now derives package_dir from dirname(chtpm_path)
     * itself, so this caller just needs to build the real chtpm path
     * once instead of passing the bare dir. */
    char chtpm_path[TP_PATH_BUF];
    snprintf(chtpm_path, sizeof(chtpm_path), "%s/menu.chtpm", g_khtpm_menu_pkg_dir);
    char px_str[16], py_str[16];
    snprintf(px_str, sizeof(px_str), "%d", px);
    snprintf(py_str, sizeof(py_str), "%d", py);
#ifndef _WIN32
    pid_t pid = fork();
    if (pid == 0) {
        execl(bin_path, bin_path, g_khtpm_menu_house_root, chtpm_path, px_str, py_str, (char *)NULL);
        _exit(1);
    } else if (pid > 0) {
        g_khtpm_menu_pid = pid;
    }
#else
    (void)bin_path;
    (void)chtpm_path;
    (void)px_str;
    (void)py_str;
    /* Entity-menu CHTPM renderer is a later Win pass; keep legacy popup. */
    g_khtpm_menu_pid = -1;
#endif
}

static void append_history(const char *fmt, ...) {
    FILE *f = fopen(g_history_path, "a");
    if (!f) return;
    time_t t = time(NULL);
    struct tm tmv;
    localtime_r(&t, &tmv);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmv);
    fprintf(f, "[%s] ", ts);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fprintf(f, "\n");
    fclose(f);
}

/* Real, generic method dispatch - same three real cases the live
 * click-handling ButtonPress branch already handles (CLOSE/void/real
 * command), factored out here so interact_relay.txt injection (below,
 * in main()'s own poll loop) can dispatch a method exactly the same
 * way a real click does. OPEN_USER is deliberately NOT handled here -
 * it needs live popup-position context only the real click site has;
 * a relay-injected OPEN_USER is a real, small future gap, not silently
 * faked here. */
static void dispatch_action(const char *action, const char *package_dir, const char *house_root, int *running_ptr) {
    /* Real bug fix (2026-08-11, direct live report: "clicking enter on
     * book menu doesn't work but mouse click does" — turned out to be
     * about "Read" specifically, not "Dir"). This function used to only
     * ever pass ONE argument (package_dir) to the command — but the
     * mouse ButtonPress handler's own separate, DUPLICATED inline copy
     * of this same dispatch logic (added 2026-08-10, "REAL FIX 2026-08-10
     * ... pass it as a real second argument") passes TWO (package_dir,
     * house_root). Any METHOD line updated to rely on the second argument
     * (e.g. book-stack's real "Read" — `sh -c 'H="$1" && ... exec
     * "$H/.../prisc+x" ...'`) worked via mouse (real click path) but
     * silently failed via Enter/RUN_METHOD/ACTIVATE_NAV (all funnel
     * through this ONE function) — $1/$H came up empty, the exec target
     * became a malformed path, sh -c's own exec failed, stderr redirected
     * to /dev/null, zero visible symptom beyond "nothing happened".
     * Fixed: this function now ALWAYS passes both arguments, matching the
     * mouse handler's own (already-correct) convention — the single
     * source of truth for the calling contract, instead of two
     * independently-diverging copies. Every METHOD line's script must
     * tolerate BOTH being present regardless of trigger path now (already
     * true for "Read" and, after the sibling fix wrapping bare system
     * binaries like "Dir" in `sh -c 'exec CMD "$0"'`, true for those too —
     * a bare command that ONLY reads argv[1] and ignores extras, like
     * xdg-open, needs that wrapper; a real script using $0/$1 already
     * works either way). */
    if (strcmp(action, "CLOSE") == 0) {
        *running_ptr = 0;
    } else if (strcmp(action, "void") == 0) {
        /* intentional no-op */
    } else if (strcmp(action, "OPEN_USER") == 0) {
        /* not supported via relay injection - see comment above */
    } else {
#ifdef _WIN32
        /* Do not system() bash. Dir → Explorer. Read → prisc+x.exe + event.pal
         * (Choose-Read/Hear/Tao lives in that pal, not meta.pdl). */
        if (strstr(action, "xdg-open") || strstr(action, "explorer")) {
            ShellExecuteA(NULL, "open", package_dir, NULL, NULL, SW_SHOWNORMAL);
            return;
        }
        if (strstr(action, "prisc") || strstr(action, "pieces/reader")) {
            const char *hr = (house_root && house_root[0]) ? house_root : ".";
            wchar_t wpat[TP_PATH_BUF], wprisc[TP_PATH_BUF], wev[TP_PATH_BUF];
            char ev_a[TP_PATH_BUF];
            snprintf(ev_a, sizeof(ev_a), "%s\\_.monads\\_.book-stack\\pieces\\reader\\event_pkg\\pages\\page_1\\event.pal", hr);
            for (char *q = ev_a; *q; q++) if (*q == '/') *q = '\\';
            MultiByteToWideChar(CP_UTF8, 0, ev_a, -1, wev, TP_PATH_BUF);
            _snwprintf(wpat, TP_PATH_BUF, L"%hs\\101.mutaclsym*", hr);
            WIN32_FIND_DATAW fd;
            HANDLE h = FindFirstFileW(wpat, &fd);
            wprisc[0] = 0;
            if (h != INVALID_HANDLE_VALUE) {
                do {
                    if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
                    wchar_t cand[TP_PATH_BUF];
                    _snwprintf(cand, TP_PATH_BUF, L"%hs\\%s\\system\\prisc+x.exe", hr, fd.cFileName);
                    if (GetFileAttributesW(cand) != INVALID_FILE_ATTRIBUTES) {
                        wcsncpy(wprisc, cand, TP_PATH_BUF - 1);
                        wprisc[TP_PATH_BUF - 1] = 0;
                    }
                } while (FindNextFileW(h, &fd));
                FindClose(h);
            }
            if (wprisc[0] && GetFileAttributesW(wev) != INVALID_FILE_ATTRIBUTES) {
                wchar_t cmd[TP_PATH_BUF * 2];
                _snwprintf(cmd, TP_PATH_BUF * 2 - 1, L"\"%s\" \"%s\"", wprisc, wev);
                STARTUPINFOW si; PROCESS_INFORMATION pi;
                ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
                ZeroMemory(&pi, sizeof(pi));
                CreateProcessW(NULL, cmd, NULL, NULL, FALSE,
                               CREATE_NEW_PROCESS_GROUP | CREATE_BREAKAWAY_FROM_JOB | DETACHED_PROCESS,
                               NULL, NULL, &si, &pi);
                if (pi.hThread) CloseHandle(pi.hThread);
                if (pi.hProcess) CloseHandle(pi.hProcess);
            }
            return;
        }
        (void)house_root;
        return;
#else
        char cmd[TP_PATH_BUF * 3];
        /* macOS leg (2026-08-22): METHOD actions are canonical Linux
         * shell strings (book-stack's Dir row is literally
         * `sh -c 'exec xdg-open "$0"'`); macOS has no xdg-open. Rewrite
         * occurrences to the native `open` at runtime — the same
         * translate-at-runtime-not-PDL shape as run_shortcut()'s
         * ktb_portable_darwin() in the manager driver. */
#ifdef __APPLE__
        {
            char fixed[TP_PATH_BUF * 3];
            const char *rd = action;
            char *wr = fixed;
            size_t n = 0;
            while (*rd && n < sizeof(fixed) - 6) {
                if (strncmp(rd, "xdg-open", 8) == 0 &&
                    (rd == action || rd[-1] == ' ' || rd[-1] == '\'' || rd[-1] == '"')) {
                    memcpy((char *)wr, "open", 4); wr += 4; rd += 8; n += 4;
                } else { *wr++ = *rd++; n++; }
            }
            *wr = '\0';
            snprintf(cmd, sizeof(cmd), "%s '%s' '%s' >/dev/null 2>&1 &", fixed, package_dir, house_root);
        }
#else
        snprintf(cmd, sizeof(cmd), "%s '%s' '%s' >/dev/null 2>&1 &", action, package_dir, house_root);
#endif
        int rc = system(cmd);
        (void)rc;
#endif
    }
}

/* REAL, 2026-08-05, direct instruction ("we should be thinking about
 * adding open desk procs to a livedesk master ledger just for
 * practice... a taskbar-widget at bottom of the screen"): a house-wide
 * "livedesk" master ledger under #.desktop/ (the confirmed real
 * house-wide file-desktop root - @.apps/hikikomorai/hikikomorai-design.md
 * §0 coined "livedesk" for this exact concept) giving every generated
 * entity a real, STABLE index nav number the first time its window
 * ever opens - reused, not reassigned, on every later relaunch (so a
 * future "jump to entity N" convention has a real, permanent address to
 * jump to, matching the digit-jump shape cli_io's own field convention
 * already uses). A separate, live "currently open" registry
 * (#.desktop/livedesk_open.txt) is what the new taskbar widget (below,
 * ensure_taskbar_running()/tp_taskbar.c) actually polls - entries added
 * on window-open, removed on clean window-close, so its tabs track
 * which entities are ACTUALLY live right now, not full history. */
static void dirname_step(const char *in, char *out, size_t out_sz) {
    char tmp[TP_PATH_BUF];
    snprintf(tmp, sizeof(tmp), "%s", in);
    char *d = dirname(tmp);
    snprintf(out, out_sz, "%s", d);
}

/* Portable self-binary path (macOS leg 2026-08-22): /proc/self/exe does
 * not exist on macOS; _NSGetExecutablePath() is the Apple equivalent and
 * may return a relative path when the binary was launched by bare name,
 * so resolve through realpath(). Linux/other keep readlink(/proc/self/exe)
 * byte-for-byte as before. Returns 1 on success. */
static int self_exe_path(char *out, size_t out_sz) {
#ifdef __APPLE__
    uint32_t sz = (uint32_t)out_sz;
    if (_NSGetExecutablePath(out, &sz) != 0) return 0;
    char resolved[TP_PATH_BUF];
    if (realpath(out, resolved)) snprintf(out, out_sz, "%s", resolved);
    return 1;
#else
    ssize_t slen = readlink("/proc/self/exe", out, out_sz - 1);
    if (slen <= 0) return 0;
    out[slen] = '\0';
    return 1;
#endif
}

/* House-root discovery: walk UP from this binary's own real install dir
 * (found via /proc/self/exe) until a directory containing BOTH #.desktop/
 * and &.widgits/ is found - the same marker-walk khtpm_vars.sh uses, so it
 * survives any relocation (the binary used to live at
 * <house>/&.widgits/tile-picker/ops/+x/ and was consolidated into
 * *.monads/*.livedesk-taskbar/ops/+x/ - the fixed dirname-step climb is
 * gone, position no longer matters). */
static void resolve_livedesk_paths(char *ops_dir_out, size_t ops_sz, char *house_root_out, size_t house_sz) {
    ops_dir_out[0] = '\0';
    house_root_out[0] = '\0';
    char self_path[TP_PATH_BUF];
    if (!self_exe_path(self_path, sizeof(self_path))) return;
    char step[TP_PATH_BUF];
    dirname_step(self_path, step, sizeof(step)); /* .../ops/+x */
    snprintf(ops_dir_out, ops_sz, "%s", step);
    /* Walk up from ops_dir until a dir holds both #.desktop/ and &.widgits/. */
    for (;;) {
        char desk[TP_PATH_BUF], widg[TP_PATH_BUF];
        snprintf(desk, sizeof(desk), "%s/#.desktop", step);
        snprintf(widg, sizeof(widg), "%s/&.widgits", step);
#ifdef _WIN32
        for (char *p = desk; *p; p++) if (*p == '/') *p = '\\';
        for (char *p = widg; *p; p++) if (*p == '/') *p = '\\';
#endif
        if (access(desk, F_OK) == 0 && access(widg, F_OK) == 0) {
            snprintf(house_root_out, house_sz, "%s", step);
            return;
        }
        char *slash = strrchr(step, '/');
#ifdef _WIN32
        char *bslash = strrchr(step, '\\');
        if (bslash && (!slash || bslash > slash)) slash = bslash;
#endif
        if (!slash || slash == step) break; /* reached root, give up */
        *slash = '\0';
    }
#ifdef _WIN32
    if (!house_root_out[0] && access("#.desktop", F_OK) == 0 && access("&.widgits", F_OK) == 0)
        snprintf(house_root_out, house_sz, ".");
#endif
}

/* Real, stable index: scans the ledger for a prior ASSIGN row whose
 * PATH= matches this exact package_dir and reuses that INDEX if found;
 * only assigns a brand-new one (from a real, persisted counter file)
 * the first time this package_dir has ever opened. Also writes
 * <package_dir>/livedesk_index.txt so any tool can read "this entity's
 * own index" directly without re-scanning the whole ledger. */
static int ensure_livedesk_index(const char *package_dir, const char *house_root) {
    char ledger_path[TP_PATH_BUF], counter_path[TP_PATH_BUF], idx_path[TP_PATH_BUF];
    snprintf(ledger_path, sizeof(ledger_path), "%s/#.desktop/livedesk_master_ledger.txt", house_root);
    snprintf(counter_path, sizeof(counter_path), "%s/#.desktop/livedesk_next_index.txt", house_root);
    snprintf(idx_path, sizeof(idx_path), "%s/livedesk_index.txt", package_dir);

    FILE *lf = fopen(ledger_path, "r");
    if (lf) {
        char line[TP_PATH_BUF];
        while (fgets(line, sizeof(line), lf)) {
            char *pathmark = strstr(line, "PATH=");
            if (!pathmark) continue;
            char pval[TP_PATH_BUF];
            snprintf(pval, sizeof(pval), "%s", pathmark + 5);
            pval[strcspn(pval, "\r\n")] = '\0';
            if (strcmp(pval, package_dir) == 0) {
                char *idxmark = strstr(line, "INDEX=");
                if (idxmark) {
                    int idx = atoi(idxmark + 6);
                    fclose(lf);
                    FILE *out = fopen(idx_path, "w");
                    if (out) { fprintf(out, "%d\n", idx); fclose(out); }
                    return idx;
                }
            }
        }
        fclose(lf);
    }

    int next = 1;
    FILE *cf = fopen(counter_path, "r");
    if (cf) { if (fscanf(cf, "%d", &next) != 1) next = 1; fclose(cf); }
    FILE *cw = fopen(counter_path, "w");
    if (cw) { fprintf(cw, "%d\n", next + 1); fclose(cw); }

    char pkgcopy[TP_PATH_BUF];
    snprintf(pkgcopy, sizeof(pkgcopy), "%s", package_dir);
    char *ent_name = basename(pkgcopy);
    time_t t = time(NULL);
    struct tm tmv;
    localtime_r(&t, &tmv);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmv);
    FILE *la = fopen(ledger_path, "a");
    if (la) {
        fprintf(la, "[%s] ASSIGN INDEX=%d ENTITY=%s PATH=%s\n", ts, next, ent_name, package_dir);
        fclose(la);
    }
    FILE *out = fopen(idx_path, "w");
    if (out) { fprintf(out, "%d\n", next); fclose(out); }
    return next;
}

/* Real liveness probe (kill(pid,0)) - forward-declared here, defined
 * alongside nav_claim_rows() below, shared by both self-healing prune
 * passes (see that function's own header comment for the full 2026-08-06
 * diagnosis this exists to fix). */
static int pid_is_alive(int pid);

/* REAL FIX 2026-08-06, direct-caught regression during the SAME session
 * ("book-stack missing from livedesk_open.txt after a simultaneous
 * multi-launch"): the self-healing prune above turned a single atomic
 * O_APPEND write (safe under concurrent writers - POSIX guarantees
 * small appends don't interleave) into a read-prune-write-rename cycle,
 * which is NOT atomic across processes - two entities launching at once
 * can both read the same "before" state, and whichever renames last
 * silently discards the other's own addition. Real fix: a real
 * cross-process mutex (flock(), same real mechanism as the popup lock
 * above) around the whole read+write critical section in every
 * function that does this read-modify-write-rename dance. */
static int g_registry_lock_fd = -1;

static void registry_lock_acquire(const char *house_root) {
    if (!LIVEDESK_USE_REGISTRY_LOCK) return;
    if (g_registry_lock_fd < 0) {
        char lock_path[TP_PATH_BUF];
        snprintf(lock_path, sizeof(lock_path), "%s/#.desktop/livedesk_registry.lock", house_root);
        g_registry_lock_fd = open(lock_path, O_CREAT | O_RDWR, 0666);
    }
    if (g_registry_lock_fd >= 0) flock(g_registry_lock_fd, LOCK_EX);
}

static void registry_lock_release(void) {
    if (!LIVEDESK_USE_REGISTRY_LOCK) return;
    if (g_registry_lock_fd >= 0) flock(g_registry_lock_fd, LOCK_UN);
}

/* REAL FIX 2026-08-06, direct report ("toolbar nav is at 7, but context
 * opened at 13") - see nav_claim_rows()'s own header comment above for
 * the full diagnosis. Same self-healing shape here: prune any dead-PID
 * or malformed line as part of adding the new one, rather than trusting
 * append-only history that never gets checked against reality. */
static void livedesk_registry_add(const char *house_root, const char *package_dir, int index, pid_t pid) {
    char reg_path[TP_PATH_BUF], tmp_path[TP_PATH_BUF];
    snprintf(reg_path, sizeof(reg_path), "%s/#.desktop/livedesk_open.txt", house_root);
    snprintf(tmp_path, sizeof(tmp_path), "%s/#.desktop/livedesk_open.txt.tmp", house_root);
    char pkgcopy[TP_PATH_BUF];
    snprintf(pkgcopy, sizeof(pkgcopy), "%s", package_dir);
    char *ent_name = basename(pkgcopy);

    registry_lock_acquire(house_root);
#ifdef _WIN32
    {
        FILE *w = fopen(reg_path, "a");
        if (w) {
            fprintf(w, "PID=%d|INDEX=%d|ENTITY=%s|PATH=%s\n", (int)pid, index, ent_name, package_dir);
            fclose(w);
        }
    }
#else
    FILE *f = fopen(reg_path, "r");
    FILE *w = fopen(tmp_path, "w");
    if (f && w) {
        char line[TP_PATH_BUF];
        while (fgets(line, sizeof(line), f)) {
            char *pp = strstr(line, "PID=");
            int line_pid = pp ? atoi(pp + 4) : 0;
            if (!pp || !pid_is_alive(line_pid)) continue;
            fputs(line, w);
        }
    }
    if (f) fclose(f);
    if (w) {
        fprintf(w, "PID=%d|INDEX=%d|ENTITY=%s|PATH=%s\n", (int)pid, index, ent_name, package_dir);
        fclose(w);
        rename(tmp_path, reg_path);
    }
#endif
    registry_lock_release();
}

static void livedesk_registry_remove(const char *house_root, pid_t pid) {
    char reg_path[TP_PATH_BUF], tmp_path[TP_PATH_BUF];
    snprintf(reg_path, sizeof(reg_path), "%s/#.desktop/livedesk_open.txt", house_root);
    snprintf(tmp_path, sizeof(tmp_path), "%s/#.desktop/livedesk_open.txt.tmp", house_root);
    registry_lock_acquire(house_root);
    FILE *f = fopen(reg_path, "r");
    if (!f) { registry_lock_release(); return; }
    FILE *w = fopen(tmp_path, "w");
    if (!w) { fclose(f); registry_lock_release(); return; }
    char marker[32];
    snprintf(marker, sizeof(marker), "PID=%d|", (int)pid);
    char line[TP_PATH_BUF];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, marker, strlen(marker)) == 0) continue;
        fputs(line, w);
    }
    fclose(f);
    fclose(w);
    rename(tmp_path, reg_path);
    registry_lock_release();
}

/* Direct instruction: "it can open when livedesk using app is open, and
 * if one is already open just add the tab of that app to the taskbar" -
 * a real PID-file singleton check (kill(pid,0), same liveness-probe
 * convention this codebase already uses in spirit elsewhere), not a
 * naive "spawn every time" that would pile up duplicate bars.
 *
 * REAL FIX 2026-08-05, direct correction ("why dont i see task bar in
 * &.widgits dir? thats where its ment to be... its not a member of
 * tile-picker"): the taskbar is its own real widget
 * (*.monads/*.livedesk-taskbar/), matching every other real widget's own
 * top-level layout (event-editor/, event-ez/, tile-picker/ itself) -
 * NOT nested inside tile-picker/ops/ just because tp_desktop_window.c
 * happens to be the one that launches it. Located via house_root
 * (already resolved above) plus this fixed, real house-relative path,
 * not via ops_dir (which would have wrongly implied "lives next to
 * tp_desktop_window.c"). */
static void ensure_taskbar_running(const char *house_root) {
#ifdef _WIN32
    if (x11_process_running("khtpm_strip_parser")) return;
    char exe[TP_PATH_BUF];
    DWORD n = GetModuleFileNameA(NULL, exe, TP_PATH_BUF);
    if (!n) return;
    char *slash = strrchr(exe, '\\');
    if (!slash) return;
    snprintf(slash + 1, TP_PATH_BUF - (size_t)(slash + 1 - exe), "khtpm_strip_parser.exe");
    x11_spawn_cwd(exe, house_root && house_root[0] ? house_root : ".");
    return;
#else
    char pid_path[TP_PATH_BUF];
    snprintf(pid_path, sizeof(pid_path), "%s/#.desktop/livedesk_taskbar.pid", house_root);
    int alive = 0;
    FILE *f = fopen(pid_path, "r");
    if (f) {
        int pid = 0;
        if (fscanf(f, "%d", &pid) == 1 && pid > 0 && kill((pid_t)pid, 0) == 0) alive = 1;
        fclose(f);
    }
    /* REAL FIX 2026-08-06: pid-file race with $.crypts concurrent launch
     * spawned DUPLICATE taskbars (two processes, grab/focus chaos). Also
     * scan /proc for an already-running tp_taskbar for this house. */
    if (!alive) {
        DIR *pd = opendir("/proc");
        if (pd) {
            struct dirent *ent;
            while ((ent = readdir(pd)) != NULL) {
                if (ent->d_name[0] < '0' || ent->d_name[0] > '9') continue;
                char cpath[64];
                snprintf(cpath, sizeof(cpath), "/proc/%s/cmdline", ent->d_name);
                FILE *cf = fopen(cpath, "r");
                if (!cf) continue;
                char cmdbuf[TP_PATH_BUF * 2];
                size_t n = fread(cmdbuf, 1, sizeof(cmdbuf) - 1, cf);
                fclose(cf);
                if (n == 0) continue;
                cmdbuf[n] = '\0';
                for (size_t i = 0; i < n; i++) if (cmdbuf[i] == '\0') cmdbuf[i] = ' ';
                /* REAL FIX 2026-08-11, direct live report "it opened both
                 * toolbars" (entities each relaunched legacy tp_taskbar
                 * even with the new khtpm strip taskbar already up): this
                 * scan only ever matched the literal substring
                 * "tp_taskbar", so it never recognized khtpm_strip_parser
                 * as "a taskbar is already running for this house" —
                 * every entity independently concluded none was running
                 * and launched legacy on top of it. Broadened to also
                 * match "khtpm_strip_parser", the taskbar's own process
                 * name (parser is the long-lived, user-visible half; its
                 * forked manager child living or dying tracks it 1:1).
                 * "tp_taskbar" kept as a harmless no-op safety net.
                 *
                 * REAL UPDATE 2026-08-11, same session, later: legacy
                 * tp_taskbar.c retired (archived to
                 * *.monads/*.livedesk-taskbar/ops/LEGACY-ARCHIVE-20260811.zip,
                 * originals deleted) — khtpm_strip_parser.+x is the real,
                 * only taskbar now. The fallback launch command below
                 * used to hardcode tp_taskbar.+x's path, which no longer
                 * exists on disk at all; updated to launch khtpm's own
                 * (renamed, no "_test" suffix) binary instead. */
                /* REAL FIX 2026-09-01 - khtpm_strip_parser.+x retired as a
                 * separate binary (folded verbatim into this very file as
                 * strip_main(), see that function's own big header
                 * comment) - the real, live process name is now
                 * khtpm_core_render, matched alongside the old name (still
                 * checked, harmless, in case an old build is somehow still
                 * running mid-transition). */
                if ((strstr(cmdbuf, "tp_taskbar") || strstr(cmdbuf, "khtpm_strip_parser") || strstr(cmdbuf, "khtpm_core_render"))
                    && strstr(cmdbuf, house_root)) {
                    alive = 1;
                    break;
                }
            }
            closedir(pd);
        }
    }
    if (!alive) {
        char cmd[TP_PATH_BUF * 2];
        snprintf(cmd, sizeof(cmd), "'%s/*.monads/*.livedesk-taskbar/ops/+x/khtpm_core_render.+x' '%s' >/dev/null 2>&1 &",
                 house_root, house_root);
        int rc = system(cmd);
        (void)rc;
    }
#endif /* !_WIN32 */
}

/* REAL, 2026-08-05, direct correction (see TILE_PICKER_DESIGN.md §13 -
 * "brackets are ment for focuz not holding numbers... it should look
 * up first and increment indexes"): a SHARED, LIVE claim pool for
 * every real "[N]" shown on screen right now, house-wide -
 * `#.desktop/livedesk-nav-claims/livedesk_nav_claims.txt` - deliberately separate from
 * ensure_livedesk_index()'s own PERMANENT ledger above (that one never
 * changes across relaunches; this one is pure live/ephemeral, numbers
 * free up and get reused the moment whatever held them closes). A
 * context menu claims one contiguous NAV range for its own rows the
 * moment it opens (nav_claim_rows()), releases that same range the
 * moment it closes (nav_release_pid()) - the taskbar (a separate real
 * process, *.monads/*.livedesk-taskbar/ops/tp_taskbar.c) claims its own
 * tab numbers from this exact same pool, so a tab and a menu row can
 * never show the same live number at once. */
/* action widened 2026-08-04, direct instruction (fo-menu-sys.md's real
 * convention: VALUE is a real, directly-executable command, e.g. a full
 * "gnome-terminal -- \"<long absolute house path>/chat.sh\"" line, not a
 * short keyword) - 64 bytes silently truncated/dropped real rows built
 * from this house's own long, emoji-heavy absolute paths.
 *
 * Moved up here (2026-08-05) from its original spot right before
 * load_methods() further down - nav_claim_rows() below needs the real
 * type, not just a forward declaration. */
typedef struct { char label[64]; char action[TP_PATH_BUF]; } MethodItem;

/* REAL FIX 2026-08-06, direct report ("index of navs maybe off, because
 * its not reindexing if one context closes and another opens"): base
 * used to be max_nav+1 - a real, confirmed bug, monotonically growing
 * FOREVER across a whole session, never reclaiming numbers
 * nav_release_pid() already freed on close. Real fix: scan every
 * currently-claimed NAV into a used[] set, then take the lowest
 * contiguous run of n free slots starting from 1 - the same real
 * "smallest available" convention wraith-alpha's own focus_index
 * validation (initialize_focus()/is_navigable()) uses to never trust a
 * stale index, applied here as smallest-free-range allocation instead. */
#define NAV_TRACK_MAX 4096
/* REAL FIX 2026-08-06, direct report ("toolbar nav is at 7, but context
 * opened at 13" - traced live: livedesk_nav_claims.txt/livedesk_open.txt
 * were full of entries for PIDs that no longer exist - some from
 * ordinary process churn, one line even structurally corrupted
 * (no "PID=" at all), proof two processes raced an unsynchronized
 * write to the same shared file at once). Neither file was ever
 * self-healing - a stale/corrupt line just sat there inflating every
 * future nav_claim_rows() count forever, exactly the "phantom index"
 * bug the report describes. Real fix: verify every claim's PID is
 * ACTUALLY alive (kill(pid,0)) as part of the same read pass, and
 * rewrite the file with only live, well-formed entries kept - so this
 * self-heals on every single popup open, regardless of how a stale
 * entry got there in the first place. */
static int pid_is_alive(int pid) {
    if (pid <= 0) return 0;
    return kill((pid_t)pid, 0) == 0 || errno != ESRCH;
}

static int nav_claim_rows(const char *house_root, pid_t pid, const char *package_dir, MethodItem *items, int n) {
    char claims_path[TP_PATH_BUF], tmp_path[TP_PATH_BUF];
    snprintf(claims_path, sizeof(claims_path), "%s/#.desktop/livedesk-nav-claims/livedesk_nav_claims.txt", house_root);
    snprintf(tmp_path, sizeof(tmp_path), "%s/#.desktop/livedesk-nav-claims/livedesk_nav_claims.txt.tmp", house_root);
    static char used[NAV_TRACK_MAX];
    memset(used, 0, sizeof(used));
    registry_lock_acquire(house_root); /* covers BOTH writes below - the prune-rename and the append are one critical section */
    FILE *f = fopen(claims_path, "r");
    FILE *w = fopen(tmp_path, "w");
    if (f) {
        char line[TP_PATH_BUF];
        while (fgets(line, sizeof(line), f)) {
            char *pp = strstr(line, "PID=");
            int line_pid = pp ? atoi(pp + 4) : 0;
            if (!pp || !pid_is_alive(line_pid)) continue; /* drop malformed/dead - self-heal */
            if (w) fputs(line, w);
            char *p = strstr(line, "NAV=");
            if (p) {
                int v = atoi(p + 4);
                if (v > 0 && v < NAV_TRACK_MAX) used[v] = 1;
            }
        }
        fclose(f);
    }
    if (w) { fclose(w); rename(tmp_path, claims_path); }
    int base = 1;
    for (int cand = 1; cand < NAV_TRACK_MAX - n; cand++) {
        int fits = 1;
        for (int i = 0; i < n; i++) {
            if (used[cand + i]) { fits = 0; break; }
        }
        if (fits) { base = cand; break; }
    }
    FILE *a = fopen(claims_path, "a");
    if (a) {
        for (int i = 0; i < n; i++) {
            fprintf(a, "KIND=row|PID=%d|NAV=%d|ROW=%d|LABEL=%s|PATH=%s\n",
                    (int)pid, base + i, i, items[i].label, package_dir);
        }
        fclose(a);
    }
    registry_lock_release();
    return base;
}

static void nav_release_pid(const char *house_root, pid_t pid) {
    char claims_path[TP_PATH_BUF], tmp_path[TP_PATH_BUF];
    snprintf(claims_path, sizeof(claims_path), "%s/#.desktop/livedesk-nav-claims/livedesk_nav_claims.txt", house_root);
    snprintf(tmp_path, sizeof(tmp_path), "%s/#.desktop/livedesk-nav-claims/livedesk_nav_claims.txt.tmp", house_root);
    registry_lock_acquire(house_root);
    FILE *f = fopen(claims_path, "r");
    if (!f) { registry_lock_release(); return; }
    FILE *w = fopen(tmp_path, "w");
    if (!w) { fclose(f); registry_lock_release(); return; }
    char marker[32];
    snprintf(marker, sizeof(marker), "|PID=%d|", (int)pid);
    char line[TP_PATH_BUF];
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, marker)) continue;
        fputs(line, w);
    }
    fclose(f);
    fclose(w);
    rename(tmp_path, claims_path);
    registry_lock_release();
}

static void read_glyph(const char *package_dir, char *out, size_t out_sz) {
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/glyph.txt", package_dir);
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    if (fgets(out, out_sz, f)) out[strcspn(out, "\r\n")] = '\0';
    fclose(f);
}

/* REAL FIX 2026-08-05, direct instruction (MUCHI_RANCHER monsters need
 * a real 2x2-tile footprint): reads a real, optional
 * "STATE | footprint_tiles | N" row from the package's own meta.pdl -
 * same SECTION|KEY|VALUE parse shape load_methods() below already
 * uses. Defaults to 1 (this file's own original, unconditional 64px
 * size) when the row is absent, so every existing pet/asa/ava package
 * is completely unaffected. */
/* REAL 2026-08-07, direct-caught bug ("muchi 4TSG has no Close
 * button"): m8's objects.pdl began with a UTF-8 BOM (EF BB BF), so its
 * first line "PAGE | main" failed strncmp(line,"PAGE",4) and the whole
 * main page (the one carrying Feed/Menu/Play/Close/Cancel) silently
 * vanished - the menu instead showed the NEXT page (activities), which
 * has no Close. Every PDL reader here opens package data files that are
 * often saved by Windows editors (which attach a BOM). This helper eats
 * a leading BOM right after fopen so the very first line parses like
 * any other. */
static FILE *pdl_open(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    int c1 = fgetc(f), c2 = fgetc(f), c3 = fgetc(f);
    if (!(c1 == 0xEF && c2 == 0xBB && c3 == 0xBF)) {
        if (c3 != EOF) ungetc(c3, f);
        if (c2 != EOF) ungetc(c2, f);
        if (c1 != EOF) ungetc(c1, f);
    }
    return f;
}

static int read_footprint_tiles(const char *package_dir) {
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/meta.pdl", package_dir);
    FILE *f = pdl_open(path);
    if (!f) return 1;
    char line[TP_PATH_BUF];
    int result = 1;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "STATE", 5) != 0) continue;
        char *p = strchr(line, '|');
        if (!p) continue;
        p++;
        while (*p == ' ') p++;
        char *end = strchr(p, '|');
        if (!end) continue;
        char *label_end = end;
        while (label_end > p && label_end[-1] == ' ') label_end--;
        const char *key = "footprint_tiles";
        size_t klen = strlen(key);
        if ((size_t)(label_end - p) != klen || strncmp(p, key, klen) != 0) continue;
        int v = atoi(end + 1);
        if (v > 0) result = v;
        break;
    }
    fclose(f);
    return result;
}

/* REAL FIX 2026-08-27 (TILE-SYSTEM-DESIGN.md §0a) - reads an optional
 * "GRID | cell_px | N" row from #.desktop/desk_grid.pdl (real,
 * house-wide - NOT per-package, since the desktop grid is one shared
 * thing every entity snaps to, unlike footprint_tiles which is
 * per-entity). Same SECTION|KEY|VALUE parse shape as
 * read_footprint_tiles() just above, adapted for a "GRID" section
 * instead of "STATE". Defaults to 80 (this file's own original
 * hardcoded GRID_CELL_PX value) when the file/row is absent. */
static int read_grid_cell_px(const char *house_root) {
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/desk_grid.pdl", house_root);
    FILE *f = pdl_open(path);
    if (!f) return 80;
    char line[TP_PATH_BUF];
    int result = 80;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "GRID", 4) != 0) continue;
        char *p = strchr(line, '|');
        if (!p) continue;
        p++;
        while (*p == ' ') p++;
        char *end = strchr(p, '|');
        if (!end) continue;
        char *label_end = end;
        while (label_end > p && label_end[-1] == ' ') label_end--;
        const char *key = "cell_px";
        size_t klen = strlen(key);
        if ((size_t)(label_end - p) != klen || strncmp(p, key, klen) != 0) continue;
        int v = atoi(end + 1);
        if (v > 0) result = v;
        break;
    }
    fclose(f);
    return result;
}

/* REAL, NEW 2026-08-31, direct instruction ("we are going to make a
 * 'map size' so players cant lose the map moving stuff around too
 * much (will hit 'wall' of movement)"), specified PDL-editable per
 * direct instruction ("something in a pdl file we can edit if we need
 * w/o changing hardcode") - same file, same GRID section, same
 * SECTION|KEY|VALUE shape as read_grid_cell_px() just above. Reads
 * two new optional keys:
 *   GRID | map_cols | N   real desktop-wide movement-wall width, in
 *                         grid cells (GRID_CELL_PX each)
 *   GRID | map_rows | N   real desktop-wide movement-wall height,
 *                         same units
 * Missing/absent/<=0 (the default, matching desk_grid.pdl not having
 * these keys yet) leaves *out_cols/*out_rows at 0 - callers treat 0 as
 * "no configured map size," falling back to the screen-resolution-
 * derived bound this file already used before this feature existed
 * (zero behavior change until someone actually sets these keys). */
static void read_map_size(const char *house_root, int *out_cols, int *out_rows) {
    *out_cols = 0;
    *out_rows = 0;
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/desk_grid.pdl", house_root);
    FILE *f = pdl_open(path);
    if (!f) return;
    char line[TP_PATH_BUF];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "GRID", 4) != 0) continue;
        char *p = strchr(line, '|');
        if (!p) continue;
        p++;
        while (*p == ' ') p++;
        char *end = strchr(p, '|');
        if (!end) continue;
        char *label_end = end;
        while (label_end > p && label_end[-1] == ' ') label_end--;
        size_t klen = (size_t)(label_end - p);
        int v = atoi(end + 1);
        if (v > 0 && klen == 8 && strncmp(p, "map_cols", 8) == 0) *out_cols = v;
        else if (v > 0 && klen == 8 && strncmp(p, "map_rows", 8) == 0) *out_rows = v;
    }
    fclose(f);
}

/* REAL 2026-08-07, direct instruction ("make them configurable via
 * config / .pdl file so i can easily experiment with them"): reads the
 * context-menu guard rows from the package's own meta.pdl (same
 * SECTION|KEY|VALUE shape load_methods()/read_footprint_tiles() use).
 * Keys (STATE section):
 *   STATE | menu_stay_open | 1   outside/repeat clicks keep the menu open
 *   STATE | grab_pointer   | 1   modal pointer grab while a menu is open
 *   STATE | grab_keyboard  | 1   modal keyboard grab while a menu is open
 *   STATE | grab_pointer_while_stay_open | 0   see open_context_menu()'s
 *                                  own comment on this key — lets a user
 *                                  opt back INTO a pointer grab even with
 *                                  menu_stay_open=1, for entities whose
 *                                  row clicks aren't reliably reaching a
 *                                  non-grabbed override-redirect popup
 *                                  under this house's Wayland/XWayland
 *                                  setup (direct report 2026-08-11: "it
 *                                  works clicking enter, but not mouse
 *                                  clicking"). Trades away "rest of the
 *                                  desk stays clickable while this menu's
 *                                  open" (the ORIGINAL reason grabbing was
 *                                  disabled for stay-open menus,
 *                                  2026-08-07) — real tradeoff, exposed as
 *                                  a knob instead of picking one hardcoded
 *                                  answer for every entity.
 * Missing rows keep the compile-time defaults. Called on startup AND on
 * every right-click reload, so a human edits meta.pdl and the very next
 * menu open picks it up - no rebuild, no restart.
 *
 * REAL BUG FIX (2026-08-11, found while adding the new key above, same
 * off-by-one class hit repeatedly elsewhere this session): the
 * "menu_stay_open" length check was klen==13 — the string is genuinely
 * 14 characters (verified: `printf '%s' "menu_stay_open" | wc -c`). This
 * meant STATE|menu_stay_open|... NEVER matched, at all — the key was
 * permanently stuck at its compile-time default (1) no matter what a
 * human set in meta.pdl, exactly the "looks configurable but silently
 * isn't" trap this whole config system exists to avoid. Fixed to 14. */
static int g_grab_pointer_while_stay_open = 0;

static void read_menu_config(const char *package_dir) {
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/meta.pdl", package_dir);
    FILE *f = pdl_open(path);
    if (!f) return;
    char line[TP_PATH_BUF];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "STATE", 5) != 0) continue;
        char *p = strchr(line, '|');
        if (!p) continue;
        p++;
        while (*p == ' ') p++;
        char *end = strchr(p, '|');
        if (!end) continue;
        char *label_end = end;
        while (label_end > p && label_end[-1] == ' ') label_end--;
        int v = atoi(end + 1);
        size_t klen = (size_t)(label_end - p);
        if (klen == 14 && strncmp(p, "menu_stay_open", 14) == 0)
            g_menu_stay_open = v ? 1 : 0;
        else if (klen == 12 && strncmp(p, "grab_pointer", 12) == 0)
            g_grab_pointer = v ? 1 : 0;
        else if (klen == 13 && strncmp(p, "grab_keyboard", 13) == 0)
            g_grab_keyboard = v ? 1 : 0;
        else if (klen == 28 && strncmp(p, "grab_pointer_while_stay_open", 28) == 0)
            g_grab_pointer_while_stay_open = v ? 1 : 0;
    }
    fclose(f);
}

/* REAL FIX 2026-08-05, direct instruction ("they should have a unique
 * alpha numerica 4digit combo"): piece_id alone (the package dir's own
 * basename) isn't guaranteed unique - two entities could share a name.
 * button.sh's own ensure_package() now generates a real, persistent
 * 4-char alphanumeric instance_id once (instance_id.txt, seed-once-
 * don't-clobber, same convention as glyph/created_at) - read it back
 * here the same way read_glyph() already does. */
static void read_instance_id(const char *package_dir, char *out, size_t out_sz) {
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/instance_id.txt", package_dir);
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    if (fgets(out, out_sz, f)) out[strcspn(out, "\r\n")] = '\0';
    fclose(f);
}

static int package_still_exists(const char *package_dir) {
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/glyph.txt", package_dir);
    struct stat st;
    return stat(path, &st) == 0;
}

/* Cheap, deterministic glyph->color hash so different tile stamps are
 * visually distinct even before/without a loaded font. */
static void glyph_color(char g, float *r, float *gg, float *b) {
    unsigned int h = (unsigned int)(unsigned char)g * 2654435761u;
    *r = ((h >> 0) & 0xFF) / 255.0f * 0.7f + 0.2f;
    *gg = ((h >> 8) & 0xFF) / 255.0f * 0.7f + 0.2f;
    *b = ((h >> 16) & 0xFF) / 255.0f * 0.7f + 0.2f;
}

/* REAL, NEW 2026-08-04, direct instruction ("next test, it should be a
 * real tile" - i.e. the window must show the actual glyph, not just a
 * colored square). No sprite/texture pipeline exists for tile-picker yet
 * (that's the still-open glyph-widening/rendering question in both
 * design docs), so this uses the same glXUseXFont technique GLUT's own
 * bitmap-font helpers are built on: load a real X core font, convert its
 * glyphs into GL display lists, draw with glCallLists. "fixed"-family X
 * core fonts are always present on any X11 install (confirmed via
 * xlsfonts on this machine) - no extra font file dependency. Returns 0
 * and leaves *base_list unset if no such font can be loaded (caller falls
 * back to the plain color square, same as before this fix). */
static int g_font_loaded = 0;
static XFontStruct *g_font_info = NULL;

/* REAL FIX 2026-08-05, direct report ("its having problems with the
 * chinese"): all popup text (context menu rows, Show Text) was drawn
 * with plain XDrawString - Latin-1 only, real X core-font limitation,
 * so the Chinese half of book-stack's Bible verses rendered as boxes/
 * garbage. Real fix: Xutf8DrawString against a real multi-byte XFontSet
 * that includes a CJK-capable base font (confirmed present on this
 * house's system via `fc-list :lang=zh` / `xlsfonts` - GNU unifont's
 * own "-misc-fixed-*-iso10646-1" covers CJK, used here alongside a
 * plain Latin fallback so ASCII stays crisp). Built once in main(),
 * used by every popup draw site below instead of XDrawString. */
static XFontSet g_popup_fontset = NULL;

static void load_popup_fontset(Display *dpy) {
    char **missing = NULL;
    int n_missing = 0;
    char *def_str = NULL;
    const char *base =
        "-misc-fixed-medium-r-normal--18-120-100-100-c-90-iso10646-1,"
        "-*-fixed-medium-r-normal--18-*-*-*-*-*-iso10646-1,"
        "-*-*-medium-r-normal--*-*-*-*-*-*-iso10646-1";
    g_popup_fontset = XCreateFontSet(dpy, base, &missing, &n_missing, &def_str);
    if (missing) XFreeStringList(missing);
    if (!g_popup_fontset) {
        g_popup_fontset = XCreateFontSet(dpy, "fixed", &missing, &n_missing, &def_str);
        if (missing) XFreeStringList(missing);
    }
}

static void popup_draw_text(Display *dpy, Drawable d, GC gc, int x, int y, const char *s) {
    if (g_popup_fontset) {
        Xutf8DrawString(dpy, d, g_popup_fontset, gc, x, y, s, (int)strlen(s));
    } else {
        XDrawString(dpy, d, gc, x, y, s, (int)strlen(s));
    }
}

/* Pixel width of UTF-8 popup label text (same fontset as popup_draw_text). */
static int popup_text_px(Display *dpy, const char *s) {
    (void)dpy;
    if (!s || !*s) return 0;
    if (g_popup_fontset) {
        XRectangle ink, logical;
        Xutf8TextExtents(g_popup_fontset, s, (int)strlen(s), &ink, &logical);
        return logical.width > 0 ? logical.width : ink.width;
    }
    /* Fallback: ~9px/glyph for the 18px fixed face we load. */
    return (int)strlen(s) * 9;
}

static int load_glyph_font(Display *dpy) {
    g_font_info = XLoadQueryFont(dpy, "-sony-fixed-medium-r-normal--24-170-100-100-c-120-iso8859-1");
    if (!g_font_info) g_font_info = XLoadQueryFont(dpy, "fixed");
    if (!g_font_info) return 0;
    return 1;
}

/* Draws directly into the compose buffer via plain XDrawString (no GL
 * display lists needed once glXUseXFont is gone - XLoadQueryFont's own
 * XFontStruct is enough for a GC-based draw). */
static void draw_glyph_rgb(Display *dpy, Drawable buf, GC gc, char g) {
    if (!g_font_info) return;
    XSetFont(dpy, gc, g_font_info->fid);
    /* Real, new 2026-08-30 - BlackPixel() alone has no real alpha byte
     * (0 in the high byte), which would draw fully TRANSPARENT text on
     * cursword's own new ARGB32 window - see draw_sprite_rgb()'s own
     * matching comment. Harmless no-op high byte on every other
     * entity's plain 24-bit window. */
    XSetForeground(dpy, gc, 0xFF000000UL | BlackPixel(dpy, DefaultScreen(dpy)));
    int cw = WIN_PX / 2, ch = (g_font_info->ascent + g_font_info->descent);
    int x = (WIN_PX - cw) / 2;
    int y = (WIN_PX + g_font_info->ascent - g_font_info->descent) / 2;
    (void)ch;
    XDrawString(dpy, buf, gc, x, y, &g, 1);
}

/* REAL FIX 2026-08-04, direct instruction ("do u see how egg-pal creates
 * the same emoji that user picked?"): load_sprite/upload_texture/
 * draw_sprite below are the same real technique 01.muchi-pals's
 * egg_window.c already uses to render a picked emoji as a real textured
 * quad, not text. tp_place_desktop.c now generates <package_dir>/
 * sprite.csv via the same emoji_gen_atlas.+x -> emoji_xtract.+x pipeline
 * hatch_egg.c uses; this window loads that CSV if present and draws the
 * real emoji texture instead of the glyph-hashed color square + font
 * fallback (which remains the fallback if sprite.csv is missing/failed
 * to generate). */
static int g_has_sprite = 0;
/* REAL FIX 2026-08-04, direct instruction ("asa's transparency isn't
 * fixed, just background is now red"): GL_BLEND alone only blends
 * within the GL scene against whatever this window itself already drew
 * (the glClearColor fill) - it does NOT make the real X11 WINDOW
 * transparent to the desktop behind it. Plain X11 windows are opaque
 * rectangles by default; real per-pixel desktop transparency needs the
 * X11 Shape Extension to cut the window's actual SHAPE to match the
 * sprite's alpha - exactly what egg_window.c's own build_shape_mask()
 * already does (ported verbatim below, POSIX branch only, matching this
 * file's own X11/GLX-only scope). Sprite pixels are now kept around
 * (not freed after texture upload) so the mask can be built from real
 * alpha data - same "kept around... so the shape mask can be rebuilt"
 * comment egg_window.c's own header already has for the identical
 * reason. */
static unsigned char *g_sprite_pixels = NULL;
static int g_sprite_res = 0;

static int load_sprite_csv(const char *csv_path) {
    FILE *f = fopen(csv_path, "r");
    if (!f) return 0;
    char line[256];
    int res = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "# resolution=", 13) == 0) { res = atoi(line + 13); break; }
    }
    if (res <= 0) { fclose(f); return 0; }

    unsigned char *pixels = malloc((size_t)res * (size_t)res * 4);
    if (!pixels) { fclose(f); return 0; }

    int count = 0;
    while (count < res * res && fgets(line, sizeof(line), f)) {
        int rr, gg, bb, aa;
        if (sscanf(line, "%d,%d,%d,%d", &rr, &gg, &bb, &aa) == 4) {
            pixels[count * 4 + 0] = (unsigned char)rr;
            pixels[count * 4 + 1] = (unsigned char)gg;
            pixels[count * 4 + 2] = (unsigned char)bb;
            pixels[count * 4 + 3] = (unsigned char)aa;
            count++;
        }
    }
    fclose(f);
    if (count != res * res) { free(pixels); return 0; }

    g_sprite_pixels = pixels; /* kept - shape mask AND the RGB draw below both read straight from this, no GL texture upload needed */
    g_sprite_res = res;
    return 1;
}

/* Verbatim port of egg_window.c's own build_shape_mask() (POSIX/X11
 * branch) - builds the window's real clip shape from the sprite's own
 * alpha channel (upscaled nearest-neighbor to the window's pixel size),
 * so the desktop genuinely shows through transparent pixels instead of
 * this window's own opaque background fill. */
static void build_shape_mask(Display *dpy, Window win, GC mask_gc, Pixmap mask) {
    XSetForeground(dpy, mask_gc, 0);
    XFillRectangle(dpy, mask, mask_gc, 0, 0, WIN_PX, WIN_PX);
    XSetForeground(dpy, mask_gc, 1);
    if (g_sprite_pixels) {
        for (int y = 0; y < WIN_PX; y++) {
            int sy = (y * g_sprite_res) / WIN_PX;
            if (sy >= g_sprite_res) sy = g_sprite_res - 1;
            for (int x = 0; x < WIN_PX; x++) {
                int sx = (x * g_sprite_res) / WIN_PX;
                if (sx >= g_sprite_res) sx = g_sprite_res - 1;
                if (g_sprite_pixels[(sy * g_sprite_res + sx) * 4 + 3] > 127) {
                    XFillRectangle(dpy, mask, mask_gc, x, y, 1, 1);
                }
            }
        }
    } else {
        XFillArc(dpy, mask, mask_gc, 0, 0, WIN_PX, WIN_PX, 0, 360 * 64);
    }
    XShapeCombineMask(dpy, win, ShapeBounding, 0, 0, mask, ShapeSet);
}

/* REAL, NEW 2026-08-31, direct live report ("some entities (not
 * cursword) but all others when rotated, leave a 'red shadow' of
 * their 2d shape") - real root cause: only cursword gets a real
 * 32-bit ARGB visual (see have_argb_visual's own "g_is_cursword"
 * gate near main()'s window creation) - every other entity's window
 * has no real per-pixel alpha at all, and build_shape_mask() above is
 * only ever called ONCE, from the flat 2D sprite's own silhouette, at
 * startup. In 3D mode the raymarched content's real footprint moves
 * as the entity rotates, but the window's real clickable/visible
 * region stays frozen at that original flat outline - so whenever the
 * rotated 3D shape covers LESS of that frozen outline than the flat
 * sprite did, the gap reveals this frame's always-opaque background
 * fill (the entity's own theme color) confined exactly to the old 2D
 * silhouette's shape - the reported "shadow."
 *
 * Real fix: after drawing 3D content into g_buf, read it back
 * (XGetImage - WIN_PX is small, ~80px, cheap at this file's own
 * MAX_FPS cap) and rebuild the window's real ShapeBounding mask from
 * whatever's ACTUALLY drawn this frame (any pixel that isn't exactly
 * the background fill color counts as "in") - the exact same real
 * "server clips what's not shaped in" mechanism build_shape_mask()
 * already uses, just driven by this frame's real raymarch result
 * instead of a one-time flat sprite. Cursword is exempt (g_is_cursword
 * check at the call site) - it already has its own real, working
 * shape-refresh path (cursword_update_shape()) for a different reason
 * (the halo's wider click surface) and real ARGB alpha for its own
 * background, so this generic path would just be redundant/
 * conflicting there. */
static void update_entity_shape_from_3d(Display *dpy, Window win, Drawable buf,
                                         int bg_r, int bg_g, int bg_b) {
    XImage *img = XGetImage(dpy, buf, 0, 0, WIN_PX, WIN_PX, AllPlanes, ZPixmap);
    if (!img) return;
    Pixmap mask = XCreatePixmap(dpy, win, WIN_PX, WIN_PX, 1);
    GC mask_gc = XCreateGC(dpy, mask, 0, NULL);
    XSetForeground(dpy, mask_gc, 0);
    XFillRectangle(dpy, mask, mask_gc, 0, 0, WIN_PX, WIN_PX);
    XSetForeground(dpy, mask_gc, 1);
    unsigned long bg_pixel = ((unsigned long)bg_r << 16) | ((unsigned long)bg_g << 8) | (unsigned long)bg_b;
    for (int y = 0; y < WIN_PX; y++) {
        for (int x = 0; x < WIN_PX; x++) {
            unsigned long px = XGetPixel(img, x, y) & 0xFFFFFFUL; /* real, deliberate - ignore the alpha byte, meaningless on this non-ARGB visual */
            if (px != bg_pixel) XFillRectangle(dpy, mask, mask_gc, x, y, 1, 1);
        }
    }
    XDestroyImage(img);
    XShapeCombineMask(dpy, win, ShapeBounding, 0, 0, mask, ShapeSet);
    XFreeGC(dpy, mask_gc);
    XFreePixmap(dpy, mask);
}

/* REAL FIX 2026-08-30, found live: the halo drawn to g_buf/win was
 * completely invisible no matter what - traced to THIS real
 * mechanism, build_shape_mask()'s own XShapeCombineMask(ShapeSet)
 * above, which clips the window's real, server-enforced visible
 * region down to just the sprite's own opaque silhouette. Anything
 * drawn to the backing pixmap OUTSIDE that shape is real X11 protocol
 * data that the server never composites - not a client-side bug, a
 * real window-shape boundary. Real fix: when cursword arms, UNION a
 * real ring-shaped mask onto the EXISTING sprite shape (ShapeUnion,
 * not ShapeSet - adds to it rather than replacing it) so the halo's
 * own pixels are inside the window's real visible region too; when it
 * disarms, rebuild the shape from scratch (build_shape_mask() again,
 * a real ShapeSet) to drop the ring and return to sprite-only. */
static void cursword_update_shape(Display *dpy, Window win) {
    if (!g_has_sprite) return;
    /* REAL, NEW 2026-08-30 - the real key-log debug strip (see
     * cursword_log_key()'s own header comment) needs the WINDOW
     * itself taller while armed, or its own visible-region rectangle
     * (unioned below) would just be empty space outside the window's
     * real bounds. Resized back down to exactly WIN_PX on disarm. */
    if (g_is_cursword)
        XResizeWindow(dpy, win, (unsigned)(g_cursword_armed ? CURSWORD_LOG_W : WIN_PX),
                      (unsigned)(WIN_PX + (g_cursword_armed ? CURSWORD_LOG_H : 0)));
    Pixmap mask = XCreatePixmap(dpy, win, (unsigned)WIN_PX, (unsigned)WIN_PX, 1);
    GC mask_gc = XCreateGC(dpy, mask, 0, NULL);
    build_shape_mask(dpy, win, mask_gc, mask); /* real ShapeSet baseline - sprite only */

    /* REAL FIX 2026-08-30, direct live report ("im still having to
     * click right on the image") - the earlier ShapeInput-only attempt
     * (a real, independent input-hitbox mask, wider than the visible
     * shape, zero visual change) turned out NOT to be honored by the
     * real compositor for genuine mouse clicks, even though it worked
     * in synthetic testing here - a known real-world gap for
     * ShapeInput specifically on override-redirect windows. The only
     * mechanism actually proven reliable for real click routing is
     * ShapeBounding itself (that's what already correctly gates every
     * other click today), so the grab surface now has to be real
     * ShapeBounding, not just Input - meaning it has to be visible.
     * Direct instruction on how: "solid disc but very low
     * transparency". This window has no true per-pixel alpha (binary
     * Shape mask only, not an ARGB32 visual) - a real transparency
     * blend isn't available, so this fills the disc with a real,
     * dim, near-black color instead (0x141414 - the same dim neutral
     * backdrop open-hai's own khtpm_open_hai_render.c already uses,
     * not an arbitrary pick) as the closest honest approximation:
     * reads as a faint shadow/backdrop, not a jarring solid block.
     * ALWAYS unioned now (moved out of the `if (g_cursword_armed)`
     * gate below) - the whole point is a wider grab surface even when
     * unarmed. */
    {
        Pixmap disc_mask = XCreatePixmap(dpy, win, (unsigned)WIN_PX, (unsigned)WIN_PX, 1);
        GC disc_gc = XCreateGC(dpy, disc_mask, 0, NULL);
        XSetForeground(dpy, disc_gc, 0);
        XFillRectangle(dpy, disc_mask, disc_gc, 0, 0, WIN_PX, WIN_PX);
        XSetForeground(dpy, disc_gc, 1);
        int dcx = WIN_PX / 2, dcy = WIN_PX / 2;
        int dradius = WIN_PX / 2 - 5;
        XFillArc(dpy, disc_mask, disc_gc, dcx - dradius, dcy - dradius,
                 (unsigned)(dradius * 2), (unsigned)(dradius * 2), 0, 360 * 64);
        XShapeCombineMask(dpy, win, ShapeBounding, 0, 0, disc_mask, ShapeUnion);
        XFreeGC(dpy, disc_gc);
        XFreePixmap(dpy, disc_mask);
    }

    if (g_cursword_armed) {
        XSetForeground(dpy, mask_gc, 0);
        XFillRectangle(dpy, mask, mask_gc, 0, 0, WIN_PX, WIN_PX);
        XSetForeground(dpy, mask_gc, 1);
        int cx = WIN_PX / 2, cy = WIN_PX / 2;
        int radius = WIN_PX / 2 - 5;
        if (radius > 3) {
            XSetLineAttributes(dpy, mask_gc, 9, LineSolid, CapButt, JoinMiter);
            XDrawArc(dpy, mask, mask_gc, cx - radius, cy - radius, (unsigned)(radius * 2), (unsigned)(radius * 2), 0, 360 * 64);
        }
        XShapeCombineMask(dpy, win, ShapeBounding, 0, 0, mask, ShapeUnion);

        /* Real key-log debug strip's own visible-region rectangle - a
         * SEPARATE mask pixmap offset by (0, WIN_PX), unioned the same
         * way as the ring just above. Deliberately separate from
         * `mask` (WIN_PX x WIN_PX, build_shape_mask()'s own real
         * contract) rather than resizing it, so that shared function's
         * existing behavior for every other entity stays completely
         * untouched. */
        Pixmap strip_mask = XCreatePixmap(dpy, win, (unsigned)CURSWORD_LOG_W, (unsigned)CURSWORD_LOG_H, 1);
        GC strip_gc = XCreateGC(dpy, strip_mask, 0, NULL);
        XSetForeground(dpy, strip_gc, 1);
        XFillRectangle(dpy, strip_mask, strip_gc, 0, 0, CURSWORD_LOG_W, CURSWORD_LOG_H);
        XShapeCombineMask(dpy, win, ShapeBounding, 0, WIN_PX, strip_mask, ShapeUnion);
        XFreeGC(dpy, strip_gc);
        XFreePixmap(dpy, strip_mask);
    }
    XFreeGC(dpy, mask_gc);
    XFreePixmap(dpy, mask);
}

/* Direct alpha-blended pixel blit into the compose buffer, nearest-
 * neighbor scaled sprite->window exactly like build_shape_mask()'s own
 * loop above (same sx/sy math, so the drawn pixels and the window's
 * real clip shape can never drift apart) - replaces the GL texture-
 * quad upload/draw entirely. Ported technique: events-hq's own
 * draw_entity_glyph() (khtpm_events_hq_render.c) already does this
 * exact alpha-over-background blit for a real sprite.csv. bg_r/g/b are
 * 0-255 ints (glyph_color()'s own 0..1 floats *255, already computed by
 * the caller) - blended UNDER the sprite's own alpha, same as GL_BLEND/
 * GL_SRC_ALPHA did before. */
static void draw_sprite_rgb(Display *dpy, Drawable buf, GC gc, int bg_r, int bg_g, int bg_b) {
    if (!g_sprite_pixels || g_sprite_res <= 0) return;
    for (int y = 0; y < WIN_PX; y++) {
        int sy = (y * g_sprite_res) / WIN_PX;
        if (sy >= g_sprite_res) sy = g_sprite_res - 1;
        for (int x = 0; x < WIN_PX; x++) {
            int sx = (x * g_sprite_res) / WIN_PX;
            if (sx >= g_sprite_res) sx = g_sprite_res - 1;
            unsigned char *p = &g_sprite_pixels[(sy * g_sprite_res + sx) * 4];
            int a = p[3];
            if (a <= 0) continue;
            int r = (p[0] * a + bg_r * (255 - a)) / 255;
            int g = (p[1] * a + bg_g * (255 - a)) / 255;
            int b = (p[2] * a + bg_b * (255 - a)) / 255;
            /* Real, new 2026-08-30 - the top byte is a real alpha
             * channel on cursword's own new ARGB32 window (see the
             * XCreateWindow ARGB-visual comment near main()'s window
             * setup) and a harmless no-op high byte on every other
             * entity's plain 24-bit window (silently masked off by the
             * X server, never stored) - always opaque here, since this
             * function already does its own real alpha blend against
             * bg_r/g/b above. */
            XSetForeground(dpy, gc, 0xFF000000UL | ((unsigned long)r << 16) | ((unsigned long)g << 8) | (unsigned long)b);
            XDrawPoint(dpy, buf, gc, x, y);
        }
    }
}

/* REAL, NEW 2026-08-30 - real "desktop 3D" render, design doc §3a/§9-12,
 * direct instruction ("we need to do the big important bulk of this
 * now... we can start with camera 3/4 topdown only, if that would
 * make it easier"). Desktop-wide by construction (§9 item #2's own
 * resolution - EVERY desktop entity's own window, this exact shared
 * binary/file, polls the SAME real #.desktop/desktop_camera_mode.txt;
 * not gated to cursword or to armed state - cursword is only ever the
 * CONTROLLER that writes this file, per the 1-4 key wiring above).
 *
 * Real scope note, matching the direct instruction above: modes 1/2
 * (true first/third-person perspective) and mode 3's own real free-
 * roam camera movement stay deferred - this first pass renders 3
 * and 4 identically, as a real, extruded "block viewed from above"
 * (matches board-viewer's own real mode-4 "bird's eye" framing
 * exactly; mode 3 is simplified down to the same topdown case for
 * now, not yet its own true free-roam perspective).
 *
 * No separate voxel-asset generation needed (unlike board-viewer's
 * own board-scale raymarcher, which reads a project-wide registry of
 * <hex>/voxels_16.csv files) - every desktop entity already has its
 * own real per-pixel RGBA texture loaded right here as g_sprite_pixels/
 * g_sprite_res (the exact same sprite.csv data draw_sprite_rgb() just
 * used above), so THAT is the real texture this reuses directly. */
static int g_camera_mode = 1;
static void load_camera_mode(const char *house_root) {
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/desktop_camera_mode.txt", house_root);
    FILE *f = fopen(path, "r");
    if (!f) { g_camera_mode = 1; return; }
    char line[16];
    if (!fgets(line, sizeof(line), f)) g_camera_mode = 1;
    else g_camera_mode = atoi(line);
    fclose(f);
    if (g_camera_mode < 1 || g_camera_mode > 4) g_camera_mode = 1;
}

/* REAL, NEW 2026-08-30, direct follow-up ("do u understand how it
 * looks depends on the camera?" -> "both" [tilt changes the block's
 * own look, AND pan/zoom moves the whole desktop]) - a real, second
 * shared state file (same real "small state file under #.desktop/"
 * convention as desktop_camera_mode.txt) carrying actual camera
 * PARAMETERS, not just a mode selector: cam_pan_x/cam_pan_y (a real
 * screen-pixel offset applied to every entity's own displayed
 * position, desktop-wide, while in 3D mode) and cam_tilt (0-100, how
 * much of each entity's own extruded side face shows - 0 is pure
 * straight-down/no side visible, 100 is maximally tilted/lots of side
 * visible). Real, honest scope note: ZOOM is NOT built this pass -
 * every entity's own window is a fixed WIN_PX size used throughout
 * this file's own shape-mask/grid/pixmap math; dynamically resizing
 * that per-frame is a real, separate, riskier change (pixmap/GC
 * recreation, shape-mask rebuild at new sizes) deliberately deferred
 * rather than rushed alongside pan+tilt in the same pass. */
static int g_cam_pan_x = 0, g_cam_pan_y = 0, g_cam_tilt = 0;
/* Real, new 2026-08-31, direct live report ("not all the camera
 * controls were fully taken from piececraft yet") - yaw (q/e),
 * board-viewer's own real key convention, was the real gap. Degrees,
 * added directly onto build_raymarch_cam()'s own fixed front/top
 * base yaw (see that function's own comment). */
static int g_cam_yaw = 0;
/* Real, tentative forward declarations - g_entity_z/g_active_z's own
 * real definitions (with header comments) sit further down this file
 * next to their real load/write functions, but cursword_handle_
 * camera_key() (right below) needs them in scope earlier - same real
 * C tentative-definition merge every other forward-declared global in
 * this file already relies on. */
static int g_entity_z;
static int g_active_z;
static void load_camera_state(const char *house_root) {
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/desktop_camera_state.txt", house_root);
    FILE *f = fopen(path, "r");
    if (!f) { g_cam_pan_x = 0; g_cam_pan_y = 0; g_cam_tilt = 0; g_cam_yaw = 0; return; }
    int pan_x = 0, pan_y = 0, tilt = 0, yaw = 0;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        int val = atoi(eq + 1);
        if (strcmp(line, "cam_pan_x") == 0) pan_x = val;
        else if (strcmp(line, "cam_pan_y") == 0) pan_y = val;
        else if (strcmp(line, "cam_tilt") == 0) tilt = val;
        else if (strcmp(line, "cam_yaw") == 0) yaw = val;
    }
    fclose(f);
    if (tilt < 0) tilt = 0;
    if (tilt > 100) tilt = 100;
    g_cam_pan_x = pan_x; g_cam_pan_y = pan_y; g_cam_tilt = tilt; g_cam_yaw = yaw;
}

/* Real write side of load_camera_state() above - cursword's own
 * camera-control keys (w/a/s/d pan, r/t tilt, q/e yaw, board-viewer's
 * own real key convention reused verbatim, zero collision with
 * cursword's own arrow-key entity movement or 1-4 mode keys) call
 * this after updating g_cam_pan_x/g_cam_pan_y/g_cam_tilt/g_cam_yaw in
 * memory. */
static void write_camera_state(const char *house_root) {
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/desktop_camera_state.txt", house_root);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "cam_pan_x=%d\ncam_pan_y=%d\ncam_tilt=%d\ncam_yaw=%d\n", g_cam_pan_x, g_cam_pan_y, g_cam_tilt, g_cam_yaw);
    fclose(f);
}

/* ============================================================
 * REAL, CONSOLIDATED CAMERA CONTROLS - direct instruction 2026-08-30
 * ("lets get all teh camera controls in 1 place first"). One real
 * function, one real per-key dispatch, called from exactly one site
 * in the main KeyPress handler (armed + g_is_cursword) instead of
 * three separate elif branches scattered through that chain. Add any
 * FUTURE camera key here too (yaw, zoom, etc. - direct instruction:
 * "then we will add the other camera controls") - this is the one
 * real place all of it belongs, not a per-key precedent to copy.
 * Returns 1 if the key was a real camera key (caller should treat it
 * as handled - set need_redraw etc.), 0 if it wasn't (caller keeps
 * checking its own other real branches, e.g. Escape/arrows).
 * ============================================================ */
static int cursword_handle_camera_key(const char *house_root, const char *package_dir, KeySym ks2) {
    if (ks2 == XK_0) {
        /* REAL, REPLACED 2026-08-31, direct instruction ("i just wanna
         * use 0 to change between 2d and 3d desk entity mode since
         * theres only 1 camera mode for desk") - the previous 1-4
         * (moved to 5-8) four-way first-person/third-person/free-roam/
         * bird's-eye split was board-viewer's own real convention,
         * reused verbatim back when a future "one map" shared-scene
         * mode was still going to need 1-4 reserved. One-map is now
         * abandoned (see ^.ONE-MAP-ATTEMPT.md - real reasons this
         * won't work here) and the direct-instruction framing above is
         * simpler and correct for THIS desk: only a real 2D/3D
         * distinction matters day to day, not which of the 4 sub-modes
         * - both 3/4 render identically here anyway (every ==3||==4
         * gate in this file treats them the same). Single real toggle:
         * mode 1 (flat/2D) <-> mode 4 (the real 3D render, picked as
         * the one representative value - bird's-eye, matches what this
         * whole session's own live testing actually used). Also
         * zeroes real cam_pan/tilt/yaw on every toggle - direct
         * instruction ("when sword view is reset, the other entities
         * views should be reset") - every other entity already polls
         * this same shared desktop_camera_state.txt (camera_changed_
         * dirty()'s own real poll, unchanged), so a clean reset here
         * is a real, already-working reset for the whole desk, not
         * just cursword's own view. */
        g_camera_mode = (g_camera_mode == 1) ? 4 : 1;
        char camp[TP_PATH_BUF];
        snprintf(camp, sizeof(camp), "%s/#.desktop/desktop_camera_mode.txt", house_root);
        FILE *cf = fopen(camp, "w");
        if (cf) { fprintf(cf, "%d\n", g_camera_mode); fclose(cf); }
        g_cam_pan_x = 0; g_cam_pan_y = 0; g_cam_tilt = 0; g_cam_yaw = 0;
        write_camera_state(house_root);
        bump_camera_changed(house_root);
        append_history(g_camera_mode == 4 ? "CURSWORD_CAMERA_3D_ON" : "CURSWORD_CAMERA_3D_OFF");
        return 1;
    }
    if (ks2 == XK_c || ks2 == XK_v) {
        /* REAL, NEW 2026-08-31, direct instruction ("do we have z
         * layers yet? ... using c & v the xelector/cursword moves up
         * and down z levels but the rest of the entities should
         * remain on their own z level unless some event is otherwise
         * moving them") - board-viewer's own real c/v key convention
         * (camera_control.c: "c/v = Camera Z level"), but real,
         * direct instruction maps it here to CURSWORD's OWN entity z
         * (cursword is the real xelector/selector role) rather than a
         * separate camera-only parameter - moving it is what defines
         * the shared desktop_active_z.txt every OTHER entity's own
         * window polls to decide whether to show or hide itself (see
         * that file's own header comment and the real map/unmap logic
         * in the main render loop). g_house_root/package_dir close
         * over this function's own real parameters, not globals.
         * REAL BUG FIX 2026-08-31, direct live report ("zx cy aren't
         * changing z level in 2d or 3d mode yet") - this branch used
         * to sit AFTER the 3D-only gate below, so c/v silently did
         * nothing outside camera_mode 3/4 - real z is an always-on
         * entity property, not a 3D-camera-only parameter, so this
         * check now runs BEFORE that gate, matching the direct
         * instruction's own "in 2d or 3d mode" framing exactly. */
        g_entity_z += (ks2 == XK_v) ? 1 : -1;
        char zpath[TP_PATH_BUF];
        snprintf(zpath, sizeof(zpath), "%s/desktop_pos.txt", package_dir);
        /* Re-read x/y (write_pos()'s own real "preserve what's not
         * changing" shape isn't reusable here - this call site has no
         * real win_x/win_y in scope, only package_dir) so the z change
         * doesn't clobber the entity's own real saved position. */
        int px = 0, py = 0;
        FILE *rf = fopen(zpath, "r");
        if (rf) {
            char line[128];
            while (fgets(line, sizeof(line), rf)) {
                if (strncmp(line, "x=", 2) == 0) px = atoi(line + 2);
                else if (strncmp(line, "y=", 2) == 0) py = atoi(line + 2);
            }
            fclose(rf);
        }
        FILE *wf = fopen(zpath, "w");
        if (wf) { fprintf(wf, "x=%d\ny=%d\nz=%d\n", px, py, g_entity_z); fclose(wf); }
        char azpath[TP_PATH_BUF];
        snprintf(azpath, sizeof(azpath), "%s/#.desktop/desktop_active_z.txt", house_root);
        FILE *azf = fopen(azpath, "w");
        if (azf) { fprintf(azf, "%d\n", g_entity_z); fclose(azf); }
        g_active_z = g_entity_z;
        bump_camera_changed(house_root);
        append_history(ks2 == XK_v ? "CURSWORD_Z_UP" : "CURSWORD_Z_DOWN");
        return 1;
    }
    if (g_camera_mode != 3 && g_camera_mode != 4) return 0; /* real, shared gate - every OTHER camera key below only means something once in a 3D mode (c/v above is deliberately exempt - real z is not camera-only), matches board-viewer's own real camera_control.c "render_mode != 1 -> no-op" precedent */
    if (ks2 == XK_f) {
        /* Real reset - board-viewer's own real key (camera_control.c:
         * "f reset to default facing"/"f center on hero"), reused
         * verbatim, direct instruction. */
        g_cam_pan_x = 0; g_cam_pan_y = 0; g_cam_tilt = 0; g_cam_yaw = 0;
        write_camera_state(house_root);
        bump_camera_changed(house_root);
        append_history("CURSWORD_CAMERA_RESET");
        return 1;
    }
    if (ks2 == XK_w || ks2 == XK_a || ks2 == XK_s || ks2 == XK_d || ks2 == XK_r || ks2 == XK_t ||
        ks2 == XK_q || ks2 == XK_e) {
        /* Real pan (w/a/s/d) + tilt (r/t) + yaw (q/e) - same real
         * letters board-viewer's own camera_control.c already uses,
         * reused verbatim, zero collision with cursword's own arrow-
         * key ENTITY movement or 1-4 mode keys. Real, new 2026-08-31,
         * direct live report ("not all the camera controls were
         * fully taken from piececraft yet") - q/e (yaw) was the real
         * gap this closes; c/v is handled separately below (real
         * per-entity Z, not a camera-only parameter, see g_entity_z's
         * own declaration comment for why). Desktop-wide effect - see
         * load_camera_state()'s own header comment. */
        int step = GRID_CELL_PX / 4;
        if (ks2 == XK_w) g_cam_pan_y += step;
        else if (ks2 == XK_s) g_cam_pan_y -= step;
        else if (ks2 == XK_a) g_cam_pan_x += step;
        else if (ks2 == XK_d) g_cam_pan_x -= step;
        else if (ks2 == XK_r) { g_cam_tilt += 10; if (g_cam_tilt > 100) g_cam_tilt = 100; }
        else if (ks2 == XK_t) { g_cam_tilt -= 10; if (g_cam_tilt < 0) g_cam_tilt = 0; }
        else if (ks2 == XK_q) g_cam_yaw -= 15;
        else if (ks2 == XK_e) g_cam_yaw += 15;
        write_camera_state(house_root);
        bump_camera_changed(house_root);
        append_history("CURSWORD_CAMERA_PAN_TILT");
        return 1;
    }
    return 0;
}

/* Real, art-derived shaded "wall" strip - same real bbox-crop +
 * edge-color-averaging technique as draw_topdown_block_rgb()'s own
 * header comment, factored out here so cursword's own camera-state
 * write helper (below) and the render path share one real definition
 * of "how big can the wall get" instead of two independent guesses. */
#define TOPDOWN_WALL_PX_MAX 20

/* REAL, NEW 2026-08-30, direct instruction ("i see it doing that but
 * its not the raymarching yet. lets keep pushing"): a genuine, real
 * per-pixel raymarch this time, not a 2D compositing trick - ported
 * near-verbatim from board-viewer's own bv_render_3d.c (its own real
 * DDA raymarcher's core primitives), which is itself already a proven,
 * real per-pixel DDA/AABB raymarcher. For a SINGLE object (one entity,
 * not a whole board of cells) the "march" collapses to one direct
 * ray-vs-one-box intersection per pixel - no grid traversal needed,
 * genuinely simpler than board-viewer's own multi-cell case while
 * using the EXACT same real ray-AABB math and face-UV convention, not
 * a simplified imitation of it. */

/* Ported near-verbatim from bv_render_3d.c's own ray_aabb_hit_3d() -
 * real slab-method ray/box intersection, returns the nearest real hit
 * distance and which of the 6 real faces it landed on (0/1=x, 2/3=y,
 * 4/5=z - see box_face_uv() below for what each face means). */
static int cursword_ray_aabb_hit(double ox, double oy, double oz, double dx, double dy, double dz,
                                  double bx0, double bx1, double by0, double by1, double bz0, double bz1,
                                  double *out_t, int *out_face) {
    double tmin = -1e18, tmax = 1e18;
    int face = -1;
    if (fabs(dx) < 1e-12) {
        if (ox < bx0 || ox > bx1) return 0;
    } else {
        double t0 = (bx0 - ox) / dx, t1 = (bx1 - ox) / dx;
        int f0 = 0;
        if (t0 > t1) { double t = t0; t0 = t1; t1 = t; f0 = 1; }
        if (t0 > tmin) { tmin = t0; face = f0; }
        if (t1 < tmax) tmax = t1;
        if (tmin > tmax) return 0;
    }
    if (fabs(dy) < 1e-12) {
        if (oy < by0 || oy > by1) return 0;
    } else {
        double t0 = (by0 - oy) / dy, t1 = (by1 - oy) / dy;
        int f0 = 2;
        if (t0 > t1) { double t = t0; t0 = t1; t1 = t; f0 = 3; }
        if (t0 > tmin) { tmin = t0; face = f0; }
        if (t1 < tmax) tmax = t1;
        if (tmin > tmax) return 0;
    }
    if (fabs(dz) < 1e-12) {
        if (oz < bz0 || oz > bz1) return 0;
    } else {
        double t0 = (bz0 - oz) / dz, t1 = (bz1 - oz) / dz;
        int f0 = 4;
        if (t0 > t1) { double t = t0; t0 = t1; t1 = t; f0 = 5; }
        if (t0 > tmin) { tmin = t0; face = f0; }
        if (t1 < tmax) tmax = t1;
        if (tmin > tmax) return 0;
    }
    if (tmax < 0.0) return 0;
    if (tmin < 0.0) { tmin = 0.0; face = -1; }
    *out_t = tmin;
    if (out_face) *out_face = face;
    return 1;
}

/* Ported near-verbatim from bv_render_3d.c's own box_face_uv() - real
 * hit-point -> texture UV, same single-texture-on-all-6-faces
 * convention that file already established (this house's own real
 * precedent for how a single sprite/emoji becomes a textured cube). */
static void cursword_box_face_uv(double wx, double wy, double wz,
                                  double bx0, double bx1, double by0, double by1, double bz0, double bz1,
                                  int face, double *u, double *v) {
    if (face == 2 || face == 3) {
        *u = (wx - bx0) / (bx1 - bx0);
        *v = (wz - bz0) / (bz1 - bz0);
    } else {
        *u = (face == 4 || face == 5) ? (wx - bx0) / (bx1 - bx0) : (wz - bz0) / (bz1 - bz0);
        *v = 1.0 - (wy - by0) / (by1 - by0);
    }
}

#define RAYMARCH_BLOCK_H 0.5   /* real box height in world units - a real, chosen "how tall is a desktop entity" constant */
#define RAYMARCH_CAM_DIST 2.2  /* real camera distance from the box's own center */
#define RAYMARCH_FOV_DEG 40.0  /* real vertical field of view */
/* Real, shared pinhole camera builder - factored out so both the
 * single-box raymarcher below AND the real per-voxel phymoji
 * raymarcher (further down) build the exact same real camera from
 * cam_tilt, never two independent (and possibly drifting) copies of
 * this math. Same real shape as bv_render_3d.c's own build_camera() -
 * forward = normalize(target - eye), real cross-product right/up -
 * just with a fixed look-at target (a point at world height cy)
 * instead of a walking hero's own anchor. */
typedef struct {
    double ex, ey, ez;             /* eye position */
    double fx, fy, fz;             /* forward */
    double rx, ry, rz;             /* right */
    double ux, uy, uz;             /* up */
    double tan_half_fov;
} RaymarchCam;

static void build_raymarch_cam(double cy, RaymarchCam *cam) {
    /* Real camera: pitch driven by cam_tilt (0 = looking straight
     * down/bird's-eye, 100 = a real oblique 3/4 angle). Real, new
     * 2026-08-30, direct correction ("lets keep the camera angles
     * reasonable... view from front for now") - yaw now comes from
     * the real emoji_sprite_view PDL toggle (g_emoji_sprite_view_top,
     * see desktop_load_click_two_step()'s own header comment): the
     * earlier fixed yaw=45 diagonal "corner" view is what read as
     * "melted"/unreasonable - "front" (the new default) is a straight
     * yaw=0 view instead, the classic real "topdown map, front-facing
     * sprite" convention. yaw isn't camera-KEY-controlled yet either
     * way, only pitch (tilt) and pan/zoom are - this toggle picks the
     * fixed default, not a third live-adjustable axis. */
    /* REAL FIX 2026-08-30, direct live report ("make sure there is no
     * tilt or angle, and show front facing view... it looks tilted") -
     * cam_tilt=0 now means a genuine, real pitch=0 EYE-LEVEL view
     * (dead-on front, zero angle) - the old formula started at
     * pitch=90 (straight DOWN) even at tilt=0, which is the opposite
     * of "front facing" despite the same numeric default. Tilt now
     * climbs UP from that real flat baseline toward a downward angle
     * as it increases, matching the real, literal meaning of "add
     * tilt" instead of starting pre-tilted. */
    double pitch_deg = (g_cam_tilt / 100.0) * 65.0; /* 0 (dead-on front, no angle) .. 65 (angled down) */
    /* Real, new 2026-08-31 - g_cam_yaw (q/e keys) added directly onto
     * the fixed front/top base yaw, real, live-adjustable rotation on
     * top of the PDL-picked default. */
    double yaw_deg = (g_emoji_sprite_view_top ? 45.0 : 0.0) + g_cam_yaw;
    double pitch = pitch_deg * M_PI_LOCAL / 180.0, yaw = yaw_deg * M_PI_LOCAL / 180.0;

    cam->ex = RAYMARCH_CAM_DIST * cos(pitch) * sin(yaw);
    cam->ey = cy + RAYMARCH_CAM_DIST * sin(pitch);
    cam->ez = RAYMARCH_CAM_DIST * cos(pitch) * cos(yaw);
    double fx = -cam->ex, fy = cy - cam->ey, fz = -cam->ez;
    double flen = sqrt(fx * fx + fy * fy + fz * fz);
    cam->fx = fx / flen; cam->fy = fy / flen; cam->fz = fz / flen;
    /* REAL FIX 2026-08-30, direct live report ("i see it but why is
     * it diagonal?" / "why does pressing not make it point straight
     * down like it does in 2d") - this cross product had its sign
     * backwards (real math bug, not a camera-parameter issue): world-
     * up = (0,1,0), right = forward x world-up should be
     * (fy*0-fz*1, fz*0-fx*0, fx*1-fy*0) = (-fz, 0, fx) - this used to
     * compute the literal NEGATIVE of that ((fz, 0, -fx)), a real
     * mirrored/rotated "right" vector that threw the whole
     * orientation off (the up vector derived from it, right x
     * forward below, inherited the same error) - not a pitch/tilt
     * problem at all, a real vector-math sign error, now corrected. */
    double rx = -cam->fz, ry = 0.0, rz = cam->fx;
    double rlen = sqrt(rx * rx + ry * ry + rz * rz);
    if (rlen < 1e-9) { rx = 1.0; ry = 0.0; rz = 0.0; rlen = 1.0; }
    cam->rx = rx / rlen; cam->ry = ry / rlen; cam->rz = rz / rlen;
    /* up = right x forward */
    double ux = cam->ry * cam->fz - cam->rz * cam->fy;
    double uy = cam->rz * cam->fx - cam->rx * cam->fz;
    double uz = cam->rx * cam->fy - cam->ry * cam->fx;
    double ulen = sqrt(ux * ux + uy * uy + uz * uz);
    cam->ux = ux / ulen; cam->uy = uy / ulen; cam->uz = uz / ulen;
    cam->tan_half_fov = tan((RAYMARCH_FOV_DEG / 2.0) * M_PI_LOCAL / 180.0);
}

static void draw_raymarch_block_rgb(Display *dpy, Drawable buf, GC gc, int bg_r, int bg_g, int bg_b) {
    if (!g_sprite_pixels || g_sprite_res <= 0) return;

    /* Real box bounds in world units - a unit-footprint cube, height
     * from RAYMARCH_BLOCK_H, centered on the origin. */
    double bx0 = -0.5, bx1 = 0.5, bz0 = -0.5, bz1 = 0.5;
    double by0 = 0.0, by1 = RAYMARCH_BLOCK_H;
    double cy = (by0 + by1) / 2.0;

    RaymarchCam cam;
    build_raymarch_cam(cy, &cam);
    double ex = cam.ex, ey = cam.ey, ez = cam.ez;
    double fx = cam.fx, fy = cam.fy, fz = cam.fz;
    double rx = cam.rx, ry = cam.ry, rz = cam.rz;
    double ux = cam.ux, uy = cam.uy, uz = cam.uz;
    double tan_half_fov = cam.tan_half_fov;

    /* Real per-pixel raymarch - one direct ray-vs-box test per pixel
     * (no grid/DDA stepping needed for a single object), genuinely
     * reads the sprite's own real texture per face via the SAME real
     * UV convention bv_render_3d.c already established. Anything the
     * ray misses leaves the existing flat base layer (drawn by the
     * caller before this) showing through unchanged. */
    for (int py = 0; py < WIN_PX; py++) {
        double ndc_y = (1.0 - 2.0 * (py + 0.5) / WIN_PX) * tan_half_fov;
        for (int px = 0; px < WIN_PX; px++) {
            double ndc_x = (2.0 * (px + 0.5) / WIN_PX - 1.0) * tan_half_fov;
            double dx = fx + rx * ndc_x + ux * ndc_y;
            double dy = fy + ry * ndc_x + uy * ndc_y;
            double dz = fz + rz * ndc_x + uz * ndc_y;
            double dlen = sqrt(dx * dx + dy * dy + dz * dz);
            dx /= dlen; dy /= dlen; dz /= dlen;

            double t; int face;
            if (!cursword_ray_aabb_hit(ex, ey, ez, dx, dy, dz, bx0, bx1, by0, by1, bz0, bz1, &t, &face)) continue;
            double wx = ex + dx * t, wy = ey + dy * t, wz = ez + dz * t;
            double u, v;
            cursword_box_face_uv(wx, wy, wz, bx0, bx1, by0, by1, bz0, bz1, face, &u, &v);
            if (u < 0.0) u = 0.0;
            if (u > 1.0) u = 1.0;
            if (v < 0.0) v = 0.0;
            if (v > 1.0) v = 1.0;
            int scol = (int)(u * g_sprite_res); if (scol >= g_sprite_res) scol = g_sprite_res - 1;
            int srow = (int)(v * g_sprite_res); if (srow >= g_sprite_res) srow = g_sprite_res - 1;
            unsigned char *sp = &g_sprite_pixels[(srow * g_sprite_res + scol) * 4];
            int a = sp[3];
            if (a <= 10) continue; /* real transparent texel - box "shows through" to the base layer */

            /* Real, simple per-face directional shading - top face
             * (2/3) full brightness, the two faces facing the camera's
             * own real diagonal (0/1 x-faces, 4/5 z-faces) shaded
             * differently so the box reads as a real 3D corner, not a
             * flat color. */
            int shade = (face == 2 || face == 3) ? 100 : (face == 0 || face == 1) ? 72 : 58;
            int r = (sp[0] * a + bg_r * (255 - a)) / 255 * shade / 100;
            int g = (sp[1] * a + bg_g * (255 - a)) / 255 * shade / 100;
            int b = (sp[2] * a + bg_b * (255 - a)) / 255 * shade / 100;
            XSetForeground(dpy, gc, 0xFF000000UL | ((unsigned long)r << 16) | ((unsigned long)g << 8) | (unsigned long)b);
            XDrawPoint(dpy, buf, gc, px, py);
        }
    }
}

/* REAL, NEW 2026-08-30, direct correction ("thats not true. phymoji
 * does it with chicken emoji. u need to dig deeper.") - the real,
 * ALREADY-BUILT phymoji system (bv_render_3d.c's own
 * load_phymoji_asset()/build_phymoji_columns()/test_phymoji_hit(),
 * generated by ops/pc_phymoji_gen.c into pieces/registry/
 * phymoji_assets/<entity_id>/voxels.csv - a real (x,y,z,r,g,b) sparse
 * voxel grid, genuinely built and working today, not planning-only -
 * confirmed live via chicken's own real 897-line voxels.csv). Ported
 * near-verbatim below (not reinvented), plus a real generated asset
 * for cursword itself (pc_phymoji_gen.+x run directly against
 * cursword's own 🗡️ emoji, 376 real voxels, copied to
 * <package_dir>/pieces/registry/phymoji_assets/cursword/voxels.csv -
 * entity_id == package_dir's own basename, so any future entity with
 * its own generated asset "just works" with zero extra per-entity
 * code). This SUPERSEDES draw_raymarch_block_rgb() above as the real
 * 3D render whenever a real voxel asset is present - that single-box
 * version stays as the real, honest fallback for any entity that
 * doesn't have one generated yet (g_phymoji_count == 0 below). */
#define MAX_PHYMOJI_VOXELS 8192
typedef struct { unsigned char lx, ly, lz, r, g, b; } CursPhymojiVoxel;
#define MAX_PHYMOJI_COLUMNS 2048
typedef struct { unsigned char lx, ly, exists_mask, cr[8], cg[8], cb[8]; } CursPhymojiColumn;

static CursPhymojiColumn g_phymoji_cols[MAX_PHYMOJI_COLUMNS];
static int g_phymoji_col_count = 0;
static int g_phymoji_max_lx = 0, g_phymoji_max_ly = 0, g_phymoji_max_lz = 0;
/* REAL, NEW 2026-08-30, direct instruction ("lets definately tune
 * proportions for 1:1 scaled camera") - the box world-size used to be
 * a fixed unit cube regardless of the asset's own real (lx,ly,lz)
 * extent, squashing/stretching every asset into the same shape
 * (confirmed live: the sword's real 14x13x8 crop looked "melted" -
 * flattened diagonal bands - forced into a 1x0.5x1 box). Real fix: ONE
 * shared world-units-per-voxel scale (PHYMOJI_VOXEL_UNIT) applied to
 * every axis identically - a genuinely proportional box matching the
 * asset's own real aspect ratio, not a separate guessed scale per
 * axis. Set once in load_cursword_phymoji(), read by draw_phymoji_rgb(). */
#define PHYMOJI_VOXEL_UNIT 0.09
static double g_phymoji_world_x = 1.0, g_phymoji_world_y = 0.5, g_phymoji_world_z = 1.0;

/* REAL, NEW 2026-08-30, direct instruction ("u should make a script
 * to do phymoji of all entities. save it locally in shared. and all
 * new entities will use it as well") - real, on-demand asset
 * generation, same real "ensure_X_generated, gated on existence,
 * generate once, cache forever" convention as chtpm_rgb_render.c's
 * own ensure_emoji_asset_generated() (see that function's own real
 * precedent). Shells out to sprite_phymoji_gen.+x (the real, shared
 * tool at &.widgits/_shared-lib/ops/sprite_phymoji_gen.c, copied
 * locally into this same ops_dir by build_khtpm_strip.sh) against
 * THIS entity's own real sprite.csv - not a re-rasterized emoji glyph
 * (see that tool's own header comment for why that distinction is a
 * real, previously-live bug, not a style preference). ops_dir comes
 * from main()'s own real self_exe_path() resolution, same real
 * pattern apply_asset_override() already uses. */
static void ensure_entity_phymoji_generated(const char *package_dir, const char *ops_dir) {
    char base_copy[TP_PATH_BUF];
    snprintf(base_copy, sizeof(base_copy), "%s", package_dir);
    char *entity_id = basename(base_copy);
    char csv_path[TP_PATH_BUF];
    snprintf(csv_path, sizeof(csv_path), "%s/pieces/registry/phymoji_assets/%s/voxels.csv", package_dir, entity_id);
    struct stat st;
    if (stat(csv_path, &st) == 0) return; /* already generated, real cache hit */
    char sprite_path[TP_PATH_BUF];
    snprintf(sprite_path, sizeof(sprite_path), "%s/sprite.csv", package_dir);
    if (access(sprite_path, F_OK) != 0) return; /* no sprite to generate from - real, honest no-op */
    char gen_bin[TP_PATH_BUF];
    snprintf(gen_bin, sizeof(gen_bin), "%s/sprite_phymoji_gen.+x", ops_dir);
    if (access(gen_bin, X_OK) != 0) return;
    char out_dir[TP_PATH_BUF];
    snprintf(out_dir, sizeof(out_dir), "%s/pieces/registry/phymoji_assets/%s", package_dir, entity_id);
    char cmd[TP_PATH_BUF * 3];
    snprintf(cmd, sizeof(cmd), "'%s' '%s' '%s' >/dev/null 2>&1", gen_bin, sprite_path, out_dir);
    int rc = system(cmd);
    (void)rc;
}

/* Ported near-verbatim from bv_render_3d.c's own load_phymoji_asset() +
 * build_phymoji_columns() - real CSV load, straight into real
 * (lx,ly)-merged columns (same real perf technique that file's own
 * header comment documents: one merged AABB test per column instead
 * of one per voxel). */
static void load_entity_phymoji(const char *package_dir, const char *ops_dir) {
    ensure_entity_phymoji_generated(package_dir, ops_dir);
    char base_copy[TP_PATH_BUF];
    snprintf(base_copy, sizeof(base_copy), "%s", package_dir);
    char *entity_id = basename(base_copy);
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/registry/phymoji_assets/%s/voxels.csv", package_dir, entity_id);
    FILE *f = fopen(path, "r");
    if (!f) return;
    CursPhymojiVoxel voxels[MAX_PHYMOJI_VOXELS];
    int n = 0, first = 1;
    char line[256];
    while (n < MAX_PHYMOJI_VOXELS && fgets(line, sizeof(line), f)) {
        if (first) { first = 0; if (strncmp(line, "x,y,z", 5) == 0) continue; }
        int x, y, z, r, g, b;
        if (sscanf(line, "%d,%d,%d,%d,%d,%d", &x, &y, &z, &r, &g, &b) == 6) {
            voxels[n].lx = (unsigned char)x; voxels[n].ly = (unsigned char)y; voxels[n].lz = (unsigned char)z;
            voxels[n].r = (unsigned char)r; voxels[n].g = (unsigned char)g; voxels[n].b = (unsigned char)b;
            if (x > g_phymoji_max_lx) g_phymoji_max_lx = x;
            if (y > g_phymoji_max_ly) g_phymoji_max_ly = y;
            if (z > g_phymoji_max_lz) g_phymoji_max_lz = z;
            n++;
        }
    }
    fclose(f);
    for (int i = 0; i < n; i++) {
        CursPhymojiVoxel *v = &voxels[i];
        int found = -1;
        for (int c = 0; c < g_phymoji_col_count; c++)
            if (g_phymoji_cols[c].lx == v->lx && g_phymoji_cols[c].ly == v->ly) { found = c; break; }
        if (found < 0) {
            if (g_phymoji_col_count >= MAX_PHYMOJI_COLUMNS) continue;
            found = g_phymoji_col_count++;
            g_phymoji_cols[found].lx = v->lx; g_phymoji_cols[found].ly = v->ly;
            g_phymoji_cols[found].exists_mask = 0;
        }
        int z = v->lz;
        if (z >= 0 && z < 8) {
            g_phymoji_cols[found].exists_mask = (unsigned char)(g_phymoji_cols[found].exists_mask | (1 << z));
            g_phymoji_cols[found].cr[z] = v->r; g_phymoji_cols[found].cg[z] = v->g; g_phymoji_cols[found].cb[z] = v->b;
        }
    }
    if (g_phymoji_col_count > 0) {
        /* Real, proportional world-size - see this file's own
         * g_phymoji_world_x/y/z declaration comment. */
        g_phymoji_world_x = (g_phymoji_max_lx + 1) * PHYMOJI_VOXEL_UNIT;
        g_phymoji_world_y = (g_phymoji_max_ly + 1) * PHYMOJI_VOXEL_UNIT;
        g_phymoji_world_z = (g_phymoji_max_lz + 1) * PHYMOJI_VOXEL_UNIT;
    }
    append_history(g_phymoji_col_count > 0 ? "ENTITY_PHYMOJI_LOADED" : "ENTITY_PHYMOJI_EMPTY");
}

/* Ported near-verbatim from bv_render_3d.c's own test_phymoji_hit() -
 * real world-ray -> local-voxel-grid transform (per-axis scale, so
 * local_t == world_t for any real hit, same real math note that
 * function's own header comment explains), then one merged-column
 * ray_aabb test per real column via cursword_ray_aabb_hit() (this
 * file's own already-ported primitive, same real function board-
 * viewer's own test_phymoji_hit() itself calls). */
static int cursword_phymoji_hit(double ox, double oy, double oz, double dx, double dy, double dz,
                                 double wx0, double wy0, double wz0,
                                 double world_size_x, double world_size_y, double world_size_z,
                                 double *out_t, int *out_face,
                                 unsigned char *out_r, unsigned char *out_g, unsigned char *out_b) {
    double scale_x = (double)(g_phymoji_max_lx + 1) / world_size_x;
    double scale_y = (double)(g_phymoji_max_ly + 1) / world_size_y;
    double scale_z = (double)(g_phymoji_max_lz + 1) / world_size_z;
    double lox = (ox - wx0) * scale_x, loy = (oy - wy0) * scale_y, loz = (oz - wz0) * scale_z;
    double ldx = dx * scale_x, ldy = dy * scale_y, ldz = dz * scale_z;

    double best_t = 1e18; int best_face = -1, best_col = -1;
    for (int c = 0; c < g_phymoji_col_count; c++) {
        if (!g_phymoji_cols[c].exists_mask) continue;
        int min_z = 0, max_z = 7;
        while (min_z < 8 && !(g_phymoji_cols[c].exists_mask & (1 << min_z))) min_z++;
        while (max_z > 0 && !(g_phymoji_cols[c].exists_mask & (1 << max_z))) max_z--;
        double t; int face;
        if (cursword_ray_aabb_hit(lox, loy, loz, ldx, ldy, ldz,
                                   (double)g_phymoji_cols[c].lx, (double)g_phymoji_cols[c].lx + 1.0,
                                   (double)g_phymoji_cols[c].ly, (double)g_phymoji_cols[c].ly + 1.0,
                                   (double)min_z, (double)max_z + 1.0, &t, &face)
            && t < best_t) {
            best_t = t; best_face = face; best_col = c;
        }
    }
    if (best_col < 0) return 0;
    double hit_loz = loz + ldz * best_t;
    int z = (int)hit_loz;
    if (z < 0) z = 0;
    if (z > 7) z = 7;
    if (!(g_phymoji_cols[best_col].exists_mask & (1 << z))) {
        int lo = z, hi = z;
        while (lo >= 0 || hi <= 7) {
            if (lo >= 0 && (g_phymoji_cols[best_col].exists_mask & (1 << lo))) { z = lo; break; }
            if (hi <= 7 && (g_phymoji_cols[best_col].exists_mask & (1 << hi))) { z = hi; break; }
            lo--; hi++;
        }
    }
    *out_t = best_t; *out_face = best_face;
    *out_r = g_phymoji_cols[best_col].cr[z]; *out_g = g_phymoji_cols[best_col].cg[z]; *out_b = g_phymoji_cols[best_col].cb[z];
    return 1;
}

/* Real, per-pixel raymarch through the actual voxel grid - same real
 * camera (build_raymarch_cam()) as the single-box fallback, but each
 * ray now tests real per-column voxel geometry instead of one flat
 * box, so the silhouette itself is genuinely volumetric (a sword's
 * real crossguard/blade shape, not a rectangular block skinned with a
 * sword texture). */
static void draw_phymoji_rgb(Display *dpy, Drawable buf, GC gc) {
    if (g_phymoji_col_count <= 0) return;
    /* Real, proportional box - see g_phymoji_world_x/y/z's own
     * declaration comment ("1:1 scaled camera" - the asset's own real
     * aspect ratio, not a forced unit cube). */
    double bx0 = -g_phymoji_world_x / 2.0, bx1 = g_phymoji_world_x / 2.0;
    double bz0 = -g_phymoji_world_z / 2.0, bz1 = g_phymoji_world_z / 2.0;
    double by0 = 0.0, by1 = g_phymoji_world_y;
    double cy = (by0 + by1) / 2.0;

    RaymarchCam cam;
    build_raymarch_cam(cy, &cam);

    for (int py = 0; py < WIN_PX; py++) {
        double ndc_y = (1.0 - 2.0 * (py + 0.5) / WIN_PX) * cam.tan_half_fov;
        for (int px = 0; px < WIN_PX; px++) {
            double ndc_x = (2.0 * (px + 0.5) / WIN_PX - 1.0) * cam.tan_half_fov;
            double dx = cam.fx + cam.rx * ndc_x + cam.ux * ndc_y;
            double dy = cam.fy + cam.ry * ndc_x + cam.uy * ndc_y;
            double dz = cam.fz + cam.rz * ndc_x + cam.uz * ndc_y;
            double dlen = sqrt(dx * dx + dy * dy + dz * dz);
            dx /= dlen; dy /= dlen; dz /= dlen;

            double t; int face; unsigned char vr, vg, vb;
            if (!cursword_phymoji_hit(cam.ex, cam.ey, cam.ez, dx, dy, dz, bx0, by0, bz0,
                                       bx1 - bx0, by1 - by0, bz1 - bz0, &t, &face, &vr, &vg, &vb))
                continue; /* real miss - base layer shows through */

            /* Real, simple per-face directional shading - same real
             * scheme draw_raymarch_block_rgb() itself uses (top
             * brightest, the two camera-facing diagonal faces shaded
             * differently), applied to the voxel's own REAL baked
             * color (pc_phymoji_gen.c already depth-attenuates colors
             * at generation time per real PyMoji Rules A/B/C - this is
             * an ADDITIONAL real per-face cue on top of that, not a
             * replacement for it). */
            int shade = (face == 2 || face == 3) ? 100 : (face == 0 || face == 1) ? 80 : 65;
            int r = vr * shade / 100, g = vg * shade / 100, b = vb * shade / 100;
            XSetForeground(dpy, gc, 0xFF000000UL | ((unsigned long)r << 16) | ((unsigned long)g << 8) | (unsigned long)b);
            XDrawPoint(dpy, buf, gc, px, py);
        }
    }
}

/* REAL REWRITE 2026-08-30, direct live correction ("i didn't see any
 * evidence of extrusion yet, like in piececraft; is that known/
 * intention?" -> answered honestly: the first version here was a flat
 * shading-strip CUE, not real extrusion -> "hopefully we do the
 * extrusion soon, cause thats the real kpi... to know we have made
 * the bulk progress"). REAL, NOW SUPERSEDED by draw_raymarch_block_rgb()
 * above (a genuine per-pixel raymarch) - kept here, unused by the
 * default dispatch, as a cheap fallback shape if the raymarcher's own
 * per-pixel cost ever needs a lighter-weight alternative. Textured,
 * tilt-driven extrusion, not a full per-pixel raymarch, but a real
 * two-face block:
 * a TOP face that visibly foreshortens (compresses vertically) as
 * cam_tilt increases - simulating a camera pitching down to reveal a
 * FRONT face below it, built by real texture sampling (stretching the
 * sprite's own bottom-edge texture row downward, progressively
 * darkened with depth for real shading), not a flat guessed color.
 * Both faces genuinely react to g_cam_tilt - 0 shows the plain flat
 * top only (matches the old "looking straight down" case exactly),
 * 100 shows a strongly compressed top and a tall, real-textured wall
 * beneath it. */
static void draw_topdown_block_rgb(Display *dpy, Drawable buf, GC gc, int bg_r, int bg_g, int bg_b) {
    if (!g_sprite_pixels || g_sprite_res <= 0) return;

    /* Real base layer - the plain flat sprite, unchanged. Guarantees
     * no gaps/holes: the compressed top face below only overdraws its
     * own real bbox footprint, everything else (padding/background)
     * still reads correctly from this base pass. */
    draw_sprite_rgb(dpy, buf, gc, bg_r, bg_g, bg_b);

    /* Real bbox crop - same real "the actual opaque silhouette, not
     * the whole padded canvas" technique bv_render_3d.c's own
     * compute_bbox_and_edge_color() already established as correct
     * for exactly this class of problem (real precedent, not
     * reinvented from scratch). */
    int u0 = g_sprite_res, v0 = g_sprite_res, u1 = -1, v1 = -1;
    for (int row = 0; row < g_sprite_res; row++) {
        for (int col = 0; col < g_sprite_res; col++) {
            if (g_sprite_pixels[(row * g_sprite_res + col) * 4 + 3] > 10) {
                if (col < u0) u0 = col;
                if (col > u1) u1 = col;
                if (row < v0) v0 = row;
                if (row > v1) v1 = row;
            }
        }
    }
    if (u1 < u0) { u0 = 0; v0 = 0; u1 = g_sprite_res - 1; v1 = g_sprite_res - 1; }

    double tilt = g_cam_tilt / 100.0; /* 0.0 (flat) .. 1.0 (max tilt) */
    int sx0 = (u0 * WIN_PX) / g_sprite_res;
    int sx1 = ((u1 + 1) * WIN_PX) / g_sprite_res;
    int sy0 = (v0 * WIN_PX) / g_sprite_res;
    int sy1 = ((v1 + 1) * WIN_PX) / g_sprite_res;
    int top_h_px = sy1 - sy0;
    if (top_h_px < 1) top_h_px = 1;

    /* REAL TOP FACE - vertically compressed by (1 - tilt*0.45): real
     * per-pixel resampling of the actual sprite texture (nearest-
     * neighbor on the source row), not a scaled copy of a pre-drawn
     * bitmap - genuinely re-samples g_sprite_pixels row-by-row, same
     * real alpha-blend formula draw_sprite_rgb() itself uses. */
    double top_scale = 1.0 - tilt * 0.45;
    int top_h_scaled = (int)(top_h_px * top_scale + 0.5);
    if (top_h_scaled < 1) top_h_scaled = 1;
    for (int y = 0; y < top_h_scaled; y++) {
        int dsty = sy0 + y;
        if (dsty < 0 || dsty >= WIN_PX) continue;
        int srow = v0 + (y * (v1 - v0 + 1)) / top_h_scaled;
        if (srow > v1) srow = v1;
        for (int x = 0; x < WIN_PX; x++) {
            int scol = (x * g_sprite_res) / WIN_PX;
            if (scol >= g_sprite_res) scol = g_sprite_res - 1;
            unsigned char *p = &g_sprite_pixels[(srow * g_sprite_res + scol) * 4];
            int a = p[3];
            if (a <= 0) continue;
            int r = (p[0] * a + bg_r * (255 - a)) / 255;
            int g = (p[1] * a + bg_g * (255 - a)) / 255;
            int b = (p[2] * a + bg_b * (255 - a)) / 255;
            XSetForeground(dpy, gc, 0xFF000000UL | ((unsigned long)r << 16) | ((unsigned long)g << 8) | (unsigned long)b);
            XDrawPoint(dpy, buf, gc, x, dsty);
        }
    }

    /* REAL FRONT/WALL FACE - a genuine texture-mapped strip, not a
     * flat averaged color: the sprite's own real bottom-edge texture
     * row (v1, its actual silhouette color there, column-mapped
     * across the wall's own real width) is stretched downward to fill
     * the wall's height, each row progressively darkened with depth
     * (a real, simple directional-shading cue, in-shadow the further
     * down/away from the top face it is). Height grows with BOTH the
     * top face's own compression gap (top_h_px - top_h_scaled) and
     * cam_tilt directly, so raising tilt genuinely makes more of this
     * real wall visible, not a fixed constant. */
    int wall_h = (top_h_px - top_h_scaled) + (int)(TOPDOWN_WALL_PX_MAX * tilt);
    int wall_top_y = sy0 + top_h_scaled;
    if (wall_top_y + wall_h > WIN_PX) wall_h = WIN_PX - wall_top_y;
    if (sx1 > sx0 && wall_h > 0) {
        for (int y = 0; y < wall_h; y++) {
            int dsty = wall_top_y + y;
            if (dsty < 0 || dsty >= WIN_PX) continue;
            int shade = 100 - (y * 45) / (wall_h > 1 ? wall_h : 1); /* 100%..55% down the wall */
            for (int x = sx0; x < sx1; x++) {
                int scol = u0 + ((x - sx0) * (u1 - u0 + 1)) / (sx1 - sx0);
                if (scol > u1) scol = u1;
                if (scol < u0) scol = u0;
                unsigned char *p = &g_sprite_pixels[(v1 * g_sprite_res + scol) * 4];
                if (p[3] <= 10) continue; /* real transparent edge pixel - leave the base layer showing through */
                int r = p[0] * shade / 100, g = p[1] * shade / 100, b = p[2] * shade / 100;
                XSetForeground(dpy, gc, 0xFF000000UL | ((unsigned long)r << 16) | ((unsigned long)g << 8) | (unsigned long)b);
                XDrawPoint(dpy, buf, gc, x, dsty);
            }
        }
    }
}

/* REAL, NEW 2026-08-04, direct instruction ("^ mode... wherever they
 * click... the phymoji will appear"): if tp_place_desktop.c already
 * wrote a desktop_pos.txt (a real click point resolved by
 * tp_arm_placer.c), spawn there instead of the fixed grid default -
 * "wherever they click" means the window's own life should start at
 * that point, not just be draggable to it afterward. */
static int read_initial_pos(const char *package_dir, int *out_x, int *out_y) {
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/desktop_pos.txt", package_dir);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[128];
    int x = -1, y = -1;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "x=", 2) == 0) x = atoi(line + 2);
        else if (strncmp(line, "y=", 2) == 0) y = atoi(line + 2);
    }
    fclose(f);
    if (x < 0 || y < 0) return 0;
    *out_x = x; *out_y = y;
    return 1;
}

/* REAL, NEW 2026-08-04, direct instruction ("add the context menus that
 * already exist from egg-pal to these by default"): a real, data-driven
 * popup context menu, modeled directly on egg_window.c's own
 * open_context_menu()/draw_context_menu()/close_context_menu() (same
 * override_redirect popup + XGrabPointer-on-open technique), but
 * reading its item list from the package's own meta.pdl METHOD rows
 * instead of egg_window's single hardcoded "Close" - see
 * TILE_PICKER_DESIGN.md §4.5. tp_place_desktop.c writes a default
 * METHOD row ("Close") into every new package, so the default behavior
 * matches egg_window's own exactly - this just makes the list
 * extensible (more methods appended to meta.pdl later, e.g. "Open Event
 * Editor" once event-editor exists, need no renderer changes here). */
/* REAL FIX 2026-08-05: asa/ava's methods.pdl was already AT this cap
 * (8: Chat/Events/Events(mock)/Play/Stop/Ledger/Close/Cancel) before
 * adding "Events (ez)" - load_methods() silently drops anything past
 * MAX_METHODS with no error, so a 9th row would have been invisible
 * with zero warning. Bumped with real headroom, not just +1. */
#define MAX_METHODS 12
#define POPUP_ROW_H 28
/* REAL FIX 2026-08-06, user: "menu screen is too thin i cant see everything"
 * — fixed 160px clipped RPG Menu rows (XP / qolq / Level lines with nav
 * prefixes). Width is now content-measured (see measure_context_popup_w /
 * g_popup_w); these are floor/ceiling only. */
#define POPUP_W_MIN 180
#define POPUP_W_MAX 640
#define POPUP_W POPUP_W_MIN /* legacy alias: text-popup floor, input defaults */
static int g_popup_w = POPUP_W_MIN;

static int load_methods(const char *package_dir, MethodItem *items, int max) {
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/meta.pdl", package_dir);
    FILE *f = pdl_open(path);
    if (!f) return 0;
    char line[TP_PATH_BUF];
    int n = 0;
    while (n < max && fgets(line, sizeof(line), f)) {
        if (strncmp(line, "METHOD", 6) != 0) continue;
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

/* REAL, 2026-08-05, direct instruction ("context menus should be much
 * more customizable, robust, even having user input, href, back, etc
 * just like chtpm" - see TILE_PICKER_DESIGN.md §11 for the wraith-alpha
 * research this is modeled on, scoped down to v1: real multi-PAGE
 * navigation + real text input, reusing MethodItem's own label/action
 * shape rather than inventing a parallel one - free positioning (x/y/w/h/z)
 * is deliberately deferred, not needed while every menu here is still a
 * single-column row list). Optional <package_dir>/objects.pdl:
 *   PAGE | <name>
 *   OBJECT | label=<text> | action=<value>
 * action is exactly methods.pdl's own real convention (a real command,
 * "CLOSE", "void") PLUS two new reserved forms: "GOTO:<page>" (push
 * current page, switch) and "BACK" (pop the page stack) for href/back
 * navigation, and "STATE:<key>" for a real input row (activates text
 * entry, committed value written to <package_dir>/<key>.txt on Escape -
 * same click-to-activate/Escape-to-commit shape this house's own cli_io
 * field convention already uses, not a new one). Absent entirely =
 * existing single-page methods.pdl behavior, completely unchanged. */
#define MAX_PAGES 8
typedef struct {
    char name[32];
    MethodItem items[MAX_METHODS];
    int n_items;
} ObjPage;

static int load_objects(const char *package_dir, ObjPage *pages, int max_pages) {
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/objects.pdl", package_dir);
    FILE *f = pdl_open(path);
    if (!f) return 0;
    char line[TP_PATH_BUF];
    int n_pages = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "PAGE", 4) == 0) {
            char *p = strchr(line, '|');
            if (!p) continue;
            p++;
            while (*p == ' ') p++;
            char *end = p + strcspn(p, "\r\n");
            while (end > p && end[-1] == ' ') end--;
            size_t nlen = (size_t)(end - p);
            if (nlen == 0 || n_pages >= max_pages) continue;
            if (nlen >= sizeof(pages[n_pages].name)) nlen = sizeof(pages[n_pages].name) - 1;
            memcpy(pages[n_pages].name, p, nlen);
            pages[n_pages].name[nlen] = '\0';
            pages[n_pages].n_items = 0;
            n_pages++;
        } else if (strncmp(line, "OBJECT", 6) == 0 && n_pages > 0) {
            ObjPage *pg = &pages[n_pages - 1];
            if (pg->n_items >= MAX_METHODS) continue;
            MethodItem *item = &pg->items[pg->n_items];
            item->label[0] = '\0';
            item->action[0] = '\0';
            char *tok = line;
            while ((tok = strchr(tok, '|')) != NULL) {
                tok++;
                while (*tok == ' ') tok++;
                char *tok_end = strchr(tok, '|');
                size_t tlen = tok_end ? (size_t)(tok_end - tok) : strcspn(tok, "\r\n");
                while (tlen > 0 && tok[tlen - 1] == ' ') tlen--;
                if (strncmp(tok, "label=", 6) == 0) {
                    size_t l = tlen - 6;
                    if (l >= sizeof(item->label)) l = sizeof(item->label) - 1;
                    memcpy(item->label, tok + 6, l);
                    item->label[l] = '\0';
                } else if (strncmp(tok, "action=", 7) == 0) {
                    size_t l = tlen - 7;
                    if (l >= sizeof(item->action)) l = sizeof(item->action) - 1;
                    memcpy(item->action, tok + 7, l);
                    item->action[l] = '\0';
                }
                tok = tok_end ? tok_end : tok + strlen(tok);
            }
            if (item->label[0]) pg->n_items++;
        }
    }
    fclose(f);
    /* REAL FIX 2026-08-05, direct correction ("it needs a cancel button
     * so context can clear, close removes entity" - caught mid-test
     * when the first objects.pdl page shipped with Close but no
     * Cancel): every page needs a real, discoverable no-op dismiss
     * distinct from Close (which really ends the process). Rather than
     * relying on every future objects.pdl author to remember this,
     * auto-append one when a page doesn't already declare its own. */
    for (int pi = 0; pi < n_pages; pi++) {
        int has_cancel = 0;
        for (int i = 0; i < pages[pi].n_items; i++) {
            if (strcmp(pages[pi].items[i].label, "Cancel") == 0) { has_cancel = 1; break; }
        }
        if (!has_cancel && pages[pi].n_items < MAX_METHODS) {
            snprintf(pages[pi].items[pages[pi].n_items].label, sizeof(pages[pi].items[0].label), "Cancel");
            snprintf(pages[pi].items[pages[pi].n_items].action, sizeof(pages[pi].items[0].action), "void");
            pages[pi].n_items++;
        }
    }
    return n_pages;
}

/* REAL, 2026-08-05, direct instruction ("book stack emoji... entirely
 * with khtpm / eventscript page, and pal"): a minimal, single-page
 * loader for a real Show Choices popup - a flat list of real
 * "OBJECT | label=.. | action=.." rows, no PAGE header needed (a choice
 * prompt is always exactly one page). Same real parse shape
 * load_objects() already uses for its own OBJECT rows, just without
 * the page-boundary bookkeeping. Used by the SHOW_PAGE relay command
 * below - the file is a real, externally-generated (by whatever op
 * asked for the choice) objects-style file, arbitrary full path, not
 * package_dir-relative. */
static int load_flat_objects(const char *full_path, MethodItem *items, int max) {
    FILE *f = pdl_open(full_path);
    if (!f) return 0;
    char line[TP_PATH_BUF];
    int n = 0;
    while (n < max && fgets(line, sizeof(line), f)) {
        if (strncmp(line, "OBJECT", 6) != 0) continue;
        items[n].label[0] = '\0';
        items[n].action[0] = '\0';
        char *tok = line;
        while ((tok = strchr(tok, '|')) != NULL) {
            tok++;
            while (*tok == ' ') tok++;
            char *tok_end = strchr(tok, '|');
            size_t tlen = tok_end ? (size_t)(tok_end - tok) : strcspn(tok, "\r\n");
            while (tlen > 0 && tok[tlen - 1] == ' ') tlen--;
            if (strncmp(tok, "label=", 6) == 0) {
                size_t l = tlen - 6;
                if (l >= sizeof(items[0].label)) l = sizeof(items[0].label) - 1;
                memcpy(items[n].label, tok + 6, l);
                items[n].label[l] = '\0';
            } else if (strncmp(tok, "action=", 7) == 0) {
                size_t l = tlen - 7;
                if (l >= sizeof(items[0].action)) l = sizeof(items[0].action) - 1;
                memcpy(items[n].action, tok + 7, l);
                items[n].action[l] = '\0';
            }
            tok = tok_end ? tok_end : tok + strlen(tok);
        }
        if (items[n].label[0]) n++;
    }
    fclose(f);
    return n;
}

/* REAL FIX 2026-08-05, direct instruction ("context window should have
 * name/id of entity, so its addressable by others" + "i dont see those
 * in context window?"): the entity's real id (piece_id-instance_id) was
 * only ever written into the invisible X11 window title - the actual
 * VISIBLE popup menu had no id at all. A non-clickable header row (row
 * 0) now shows it, so a human right-clicking an entity can read/copy
 * its real address directly, not just infer it from the glyph. */
static char g_full_id[96] = "";

/* Size the next context menu to its longest drawn row (header + labels,
 * including worst-case chtpm nav prefix "[>] 99. "). Result lives in
 * g_popup_w; open/draw/hit-test/submenu offset all share that. */
static int measure_context_popup_w(Display *dpy, MethodItem *items, int n) {
    const int pad = 28; /* left text x (12) + right margin + border */
    int maxw = popup_text_px(dpy, g_full_id);
    if (items) {
        for (int i = 0; i < n; i++) {
            char buf[192];
            snprintf(buf, sizeof(buf), "[>] 99. %s", items[i].label);
            int tw = popup_text_px(dpy, buf);
            if (tw > maxw) maxw = tw;
            tw = popup_text_px(dpy, items[i].label);
            if (tw > maxw) maxw = tw;
        }
    }
    int w = maxw + pad;
    if (w < POPUP_W_MIN) w = POPUP_W_MIN;
    if (w > POPUP_W_MAX) w = POPUP_W_MAX;
    return w;
}

/* REAL FIX 2026-08-05, direct-caught bug ("its been having issues"
 * investigation - a plain `kill <pid>` left a stale livedesk_open.txt/
 * nav_claims.txt entry behind forever, since this process had no
 * SIGTERM handler and this file's own real cleanup code (registry/
 * nav-claim removal, right before the real return at the end of
 * main()) only ever ran on a normal exit - a bare SIGTERM (the default
 * for plain `kill`, and what most process-manager/session shutdowns
 * send) skipped it entirely. Real fix: catch SIGTERM/SIGINT, set this
 * real sig_atomic_t flag (async-signal-safe per POSIX - nothing more
 * elaborate is safe to do inside a signal handler), the main loop's own
 * condition checks it every iteration so the SAME real cleanup path
 * always runs. */
static volatile sig_atomic_t g_shutdown_requested = 0;
static void handle_shutdown_signal(int sig) {
    (void)sig;
    g_shutdown_requested = 1;
}

/* Real Show Text popup content - see the SHOW_TEXT_FILE relay handler
 * in main() and its own Expose-draw branch below. */
static char g_text_popup_lines[64][256];
static int g_text_popup_n_lines = 0;

/* REAL FIX 2026-08-06, direct report ("book and monster are both having
 * focus control problems"): XGrabKeyboard/XGrabPointer are DISPLAY-WIDE
 * EXCLUSIVE resources - only one process can hold either at a time.
 * Every entity (book-stack, each monster, asa, ava...) is its own
 * separate OS process, each independently calling XGrabKeyboard the
 * moment ITS OWN popup opens, with zero coordination between them. If
 * two entities' popups happen to open around the same time, whichever
 * grabs last wins - the OTHER process's retry loop (open_context_menu's
 * own 5-attempt retry above) silently exhausts and returns a popup with
 * NO real keyboard grab at all, no error, no log - exactly this report.
 * Real fix: a real, house-wide, cross-process mutex via flock() on a
 * single shared lockfile (#.desktop/livedesk_popup.lock) - any process
 * about to open a popup blocks (real, not spin-polled - flock(LOCK_EX)
 * blocks in the kernel) until it's the only one holding it, so grab
 * contention structurally can't happen anymore. g_popup_lock_depth
 * makes this reentrant for a SINGLE process's own nested popups (e.g.
 * the main context menu plus its own input_popup_win at once) - only
 * the outermost open/close pair actually touches the lock. */
static int g_popup_lock_fd = -1;
static int g_popup_lock_depth = 0;
static char g_house_root_for_lock[TP_PATH_BUF] = "";

/* REAL, NEW 2026-08-29, direct instruction ("the tb has a
 * transparency. but that should propagate to 'all entities'... so
 * player can still see thru their desktop a bit") - real, working
 * opacity already exists (khtpm_strip_parser.c's set_window_opacity()/
 * tp_load_theme_opacity(), real _NET_WM_WINDOW_OPACITY + #.desktop/
 * livedesk_theme.pdl "COLOR|opacity|N") but every desktop entity
 * window (every pal/tile - what this file spawns) rendered at full
 * opacity, same real gap khtpm_core_render.c had for its own
 * windows. Ported the same way (adapted to THIS file's own TP_PATH_BUF
 * convention; house_root is a local in main() here, not a global, so
 * it's a real parameter instead). */

static double tp_load_theme_opacity(const char *house_root) {
    double opacity = 0.5;
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/livedesk_theme.pdl", house_root);
    FILE *f = fopen(path, "r");
    if (!f) return opacity;
    char line[TP_PATH_BUF];
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

/* REAL FIX/REVERT 2026-08-06, direct report ("not clicking buttons with
 * the mouse will run the button... serious focus issues that doing
 * khtpm introduced"): flock(LOCK_EX) BLOCKS THE ENTIRE PROCESS - this
 * whole event loop froze while waiting, and any real input event that
 * arrived at the X server during that freeze (a leftover click, a
 * queued Enter from earlier) sat buried in the queue and fired the
 * INSTANT the lock finally freed, indistinguishable from "it ran
 * itself." A real, un-clicked action. Real fix: bounded, non-blocking
 * wait (LOCK_NB in a short poll loop, ~1s total) instead of blocking
 * forever - worst case behaves like the pre-lock code (a grab might
 * still race), but the process is NEVER frozen long enough to build up
 * a dangerous input backlog. */
static void popup_lock_acquire(void) {
    if (!LIVEDESK_USE_POPUP_LOCK) return;
    if (g_popup_lock_depth++ > 0) return;
    if (!g_house_root_for_lock[0]) return;
    if (g_popup_lock_fd < 0) {
        char lock_path[TP_PATH_BUF];
        snprintf(lock_path, sizeof(lock_path), "%s/#.desktop/livedesk_popup.lock", g_house_root_for_lock);
        g_popup_lock_fd = open(lock_path, O_CREAT | O_RDWR, 0666);
    }
    if (g_popup_lock_fd >= 0) {
        for (int attempt = 0; attempt < 50; attempt++) {
            if (flock(g_popup_lock_fd, LOCK_EX | LOCK_NB) == 0) break;
            usleep(20000); /* 20ms * 50 = ~1s total bounded wait, never indefinite */
        }
    }
}

static void popup_lock_release(void) {
    if (!LIVEDESK_USE_POPUP_LOCK) return;
    if (g_popup_lock_depth <= 0) return;
    if (--g_popup_lock_depth > 0) return;
    if (g_popup_lock_fd >= 0) flock(g_popup_lock_fd, LOCK_UN);
}


/* Soft focus for a single popup window (option C). Only touches THIS
 * window — no XRaiseWindow on other entities, no global focus war. */
static void popup_soft_focus(Display *dpy, Window popup) {
    if (!LIVEDESK_POPUP_SOFT_FOCUS) return;
    if (!popup) return;
    XRaiseWindow(dpy, popup); /* raise the menu itself so it's not buried */
    XSetInputFocus(dpy, popup, RevertToParent, CurrentTime);
    XFlush(dpy);
}

static void clamp_popup_to_screen(Display *dpy, int *x, int *y, int w, int h) {
    /* REAL 2026-08-06, user: context windows near bottom/side should open
     * in empty space (stay fully on-screen above taskbar, not clipped). */
    int scr = DefaultScreen(dpy);
    int sw = DisplayWidth(dpy, scr);
    int sh = DisplayHeight(dpy, scr);
    const int margin = 4;
    const int taskbar_reserve = 40; /* livedesk bar ~32px + padding */
    int usable_h = sh - taskbar_reserve;
    if (usable_h < h + margin) usable_h = sh - margin;
    int px = *x, py = *y;
    if (px + w + margin > sw) px = sw - w - margin;
    if (px < margin) px = margin;
    if (py + h + margin > usable_h) py = usable_h - h - margin;
    if (py < margin) py = margin;
    /* Prefer flipping above-left of anchor if we still overflow badly */
    if (py + h > usable_h) py = margin;
    if (px + w > sw) px = margin;
    *x = px;
    *y = py;
}

static Window open_context_menu(Display *dpy, GC gc, int *root_x, int *root_y, int nitems, MethodItem *items) {
    /* REAL FIX 2026-08-29 (ENTITY-MENU-LEGACY-DEPRECATION-PLAN.md
     * Phase 3 / TP-DESKTOP-LEGACY-POPUP-REMOVAL-CHECKLIST.md) - same
     * real redirect this function already did at its own END (see the
     * dead block this replaces, right before the final `return popup;`
     * below) - g_use_khtpm_menu is checked ONCE per-process at the top
     * of main(), never per-call-site, so moving the check here doesn't
     * change WHICH calls redirect, only WHEN: this entity's own
     * XCreateWindow/popup_lock_acquire/XGrabPointer/XGrabKeyboard never
     * need to run at all now, instead of running fully then being
     * destroyed a moment later. Zero semantic change - every entity
     * behaves identically to before, just without the wasted work. */
#ifndef _WIN32
    if (g_use_khtpm_menu) {
        int px = root_x ? *root_x : 0;
        int py = root_y ? *root_y : 0;
        launch_khtpm_menu(px, py);
        return None;
    }
#endif
    popup_lock_acquire();
    /* +1 row for the id header (see g_full_id comment above). */
    int h = POPUP_ROW_H * ((nitems > 0 ? nitems : 1) + 1);
    /* Content-aware width: measure labels when caller passes items;
     * bare input/placeholder menus get a slightly roomier default. */
    if (items && nitems > 0)
        g_popup_w = measure_context_popup_w(dpy, items, nitems);
    else
        g_popup_w = 320; /* input / empty: wider than the old 160 floor */
    int px = root_x ? *root_x : 0;
    int py = root_y ? *root_y : 0;
    clamp_popup_to_screen(dpy, &px, &py, g_popup_w, h);
    if (root_x) *root_x = px;
    if (root_y) *root_y = py;
    XSetWindowAttributes swa;
    swa.override_redirect = True;
    swa.background_pixel = WhitePixel(dpy, DefaultScreen(dpy));
    /* REAL, 2026-08-05, direct instruction ("try some stuff with digit
     * jump / [] bracket nav"): KeyPressMask + a real keyboard grab
     * added alongside the existing pointer grab, so digit keys reach
     * this popup regardless of which window nominally has focus - same
     * reasoning as the existing XGrabPointer just above. */
    swa.event_mask = ExposureMask | ButtonPressMask | KeyPressMask;
    Window popup = XCreateWindow(dpy, RootWindow(dpy, DefaultScreen(dpy)),
                                  px, py, (unsigned)g_popup_w, h, 1,
                                  CopyFromParent, InputOutput, CopyFromParent,
                                  CWOverrideRedirect | CWBackPixel | CWEventMask, &swa);
    /* REAL FIX 2026-08-06, direct root cause found ("keystrokes went
     * INTO the terminal"): this whole session's "focus problems" were
     * never really about the grab retry logic - Mutter's Wayland
     * compositor (org.gnome.mutter.wayland xwayland-allow-grabs)
     * restricts XGrabKeyboard from XWayland clients by default, real,
     * intentional Wayland security policy, not a bug in this code.
     * xwayland-grab-access-rules allowlists by WM_CLASS - these windows
     * never set one, so they could never be allowlisted. Real fix: a
     * real WM_CLASS ("MuchiverseLivedesk"), matched by
     * $.crypts/enable_xwayland_grabs.sh (house-wide, re-runnable). */
    XClassHint *class_hint = XAllocClassHint();
    if (class_hint) {
        class_hint->res_name = (char *)"MuchiverseLivedesk";
        class_hint->res_class = (char *)"MuchiverseLivedesk";
        XSetClassHint(dpy, popup, class_hint);
        XFree(class_hint);
    }
    XMapRaised(dpy, popup);
    XSetForeground(dpy, gc, BlackPixel(dpy, DefaultScreen(dpy)));
    /* REAL FIX 2026-08-05, direct report ("it definately has a problem
     * getting focus"): neither grab's own return value was ever
     * checked. A real, well-known X11 race: closing one popup
     * (XUngrabKeyboard/XUngrabPointer) and immediately opening another
     * (exactly what a SHOW_PAGE Show-Choices transition does - close
     * the old popup, open the new one, same event-handling tick) can
     * race the X server's own internal grab-release processing, so the
     * new XGrabKeyboard/XGrabPointer call can genuinely fail
     * (AlreadyGrabbed) even though nothing else actually wants the
     * grab. Real fix: retry a few times with a short real wait,
     * confirmed via `man XGrabKeyboard`'s own documented AlreadyGrabbed
     * return - not a made-up mitigation. */
    /* REAL FIX 2026-08-07, direct report ("toolbar is not taking clicks",
     * "x didn't close them but closed toolbar"): when menu_stay_open is
     * on, the menu must NOT hold a modal pointer grab - a grab redirects
     * EVERY pointer event (toolbar clicks, other windows, everything) to
     * this popup, so with the menu kept open the whole desk becomes
     * unclickable. Stay-open menus are deliberately NON-modal by default:
     * no pointer grab, so clicks reach their real targets; the menu
     * simply stays open until the user clicks a row/Cancel or presses
     * Escape/Enter. Arrow nav still works - the keyboard grab below is
     * kept. menu_stay_open=0 restores the old modal grab + dismiss-on-
     * any-outside-click behavior (grab_pointer still toggles it there).
     *
     * REAL, 2026-08-11, direct instruction ("add a config to switch that
     * on and off w/o hardcoding... user can tweak it w/o changing code,
     * and find optimal solution"): the "no grab while stay-open" choice
     * above is a genuine, real tradeoff (some entities' row clicks may
     * not reliably reach a non-grabbed override-redirect popup under
     * this house's Wayland/XWayland setup — direct report: "it works
     * clicking enter, but not mouse clicking"), not a universally-correct
     * answer for every entity. grab_pointer_while_stay_open (meta.pdl,
     * default 0 = unchanged prior behavior) lets a human opt back INTO
     * grabbing per-entity to test whether that fixes click delivery for
     * THEM specifically, accepting the "rest of the desk goes unclickable
     * while this menu is open" tradeoff as a deliberate choice instead of
     * it being permanently unavailable. Don't grab pointer if menu stays
     * open — allows user to click other windows (e.g., browser) while
     * keeping the menu open for relay-based navigation. */
    if (g_grab_pointer && !g_menu_stay_open) {
        for (int attempt = 0; attempt < 5; attempt++) {
            int rc = XGrabPointer(dpy, popup, True, ButtonPressMask, GrabModeAsync, GrabModeAsync,
                                   None, None, CurrentTime);
            if (rc == GrabSuccess) break;
            XSync(dpy, False);
            usleep(5000);
        }
    }
    if (g_grab_keyboard) {
        for (int attempt = 0; attempt < 5; attempt++) {
            int rc = XGrabKeyboard(dpy, popup, True, GrabModeAsync, GrabModeAsync, CurrentTime);
            if (rc == GrabSuccess) break;
            XSync(dpy, False);
            usleep(5000);
        }
    }
    /* REAL FIX 2026-08-05, direct report ("clicking read just closes
     * context"): X11 can and does recycle a destroyed window's own
     * resource ID for the very next XCreateWindow call on the same
     * display (confirmed - this is exactly the close-old/open-new
     * sequence SHOW_PAGE does every time). Any ButtonPress/KeyPress
     * already sitting in the X server's queue, addressed to the OLD
     * popup's ID before it was destroyed, gets redelivered to this NEW
     * popup the instant it's mapped - read as an immediate phantom
     * click on row 0, dismissing the menu before the user ever saw it.
     * Real fix: drain any already-queued Button/KeyPress events
     * targeting this exact window id before returning, so only input
     * that arrives AFTER this popup genuinely existed can select a row. */
    XSync(dpy, False);
    XEvent stale_ev;
    while (XCheckWindowEvent(dpy, popup, ButtonPressMask | KeyPressMask, &stale_ev)) {
        /* discard - see comment above */
    }
    /* OPTION C 2026-08-06: grabs re-enabled (flags=1); locks stay off.
     * Soft focus on THIS popup only as fallback when grab fails or
     * XWayland ignores grab — not the old multi-entity focus war
     * (we never XSetInputFocus other processes' tile windows here). */
    popup_soft_focus(dpy, popup);

    /* REAL FIX 2026-08-29 - the real g_use_khtpm_menu redirect that
     * used to live here (create the legacy window fully, then destroy
     * it and launch_khtpm_menu() instead) moved to the TOP of this
     * function - see that real fix's own header comment. This point is
     * now only ever reached when g_use_khtpm_menu is 0 (or on Windows,
     * where khtpm_core_render.exe doesn't exist yet and the
     * legacy Xlib menu is still the real, correct behavior) - the
     * legacy popup created above is the REAL, visible result. */
    return popup;
}

/* REAL FIX 2026-08-05, direct correction ("im still seeing numbers in
 * brackets instead of [>] [] empty brackets like chtpm"): real chtpm
 * format (confirmed via direct example, a real live yahoo-broker frame)
 * is "[ ] N. Label" - an EMPTY bracket is a real focus-cursor marker
 * (becomes "[>]" for whichever row currently has focus), completely
 * separate from the plain "N." row number that follows it. The
 * previous "[N] Label" render put the shared live nav number INSIDE
 * the bracket - wrong shape entirely, not just a style nit. nav_base is
 * this popup's own claimed starting number (see nav_claim_rows()), so
 * row i's real number is nav_base+i; nav_base<=0 (the small
 * user_popup_win/input_popup_win submenus, which don't claim nav
 * numbers) suppresses both the bracket and the number rather than
 * showing a misleading unclaimed one.
 *
 * REAL FIX, same day, follow-up: real up/down focus-cursor tracking now
 * exists (focus_row, driven by real Up/Down KeyPress in main()'s own
 * popup_win branch) - whichever row equals focus_row shows "[>]", every
 * other row shows "[ ]", matching real chtpm's own single-row cursor
 * convention exactly (not a highlight/fill effect - just the bracket
 * glyph itself changes, same as the real captured yahoo-broker frame). */
static void draw_context_menu(Display *dpy, Window popup, GC gc, MethodItem *items, int n, int nav_base, int focus_row) {
    XClearWindow(dpy, popup);
    int h = POPUP_ROW_H * ((n > 0 ? n : 1) + 1);
    int pw = g_popup_w > 0 ? g_popup_w : POPUP_W_MIN;
    XDrawRectangle(dpy, popup, gc, 0, 0, pw - 1, h - 1);
    /* Row 0: non-clickable id header. */
    popup_draw_text(dpy, popup, gc, 12, POPUP_ROW_H / 2 + 4, g_full_id);
    XDrawLine(dpy, popup, gc, 0, POPUP_ROW_H, pw, POPUP_ROW_H);
    for (int i = 0; i < n; i++) {
        int row_y = (i + 1) * POPUP_ROW_H;
        if (i > 0) XDrawLine(dpy, popup, gc, 0, row_y, pw, row_y);
        char labeled[160];
        const char *cursor = (i == focus_row) ? "[>]" : "[ ]";
        if (nav_base > 0) snprintf(labeled, sizeof(labeled), "%s %d. %s", cursor, nav_base + i, items[i].label);
        else snprintf(labeled, sizeof(labeled), "%s", items[i].label);
        popup_draw_text(dpy, popup, gc, 12, row_y + POPUP_ROW_H / 2 + 4, labeled);
    }
}

/* REAL, 2026-08-05: the range-finder grid used to be drawn HERE as an
 * opaque popup (real, but covered the dog/nearby tiles - direct
 * correction: "it should just be a transparent outline like a png").
 * Moved to a real standalone binary, ops/tp_range_grid.c, using the
 * X11 Shape Extension (same real transparency technique this file's
 * own sprite rendering already uses) - launched via system() from the
 * "OPEN_RANGE_GRID" dispatch below instead of drawn inline. */

static void close_context_menu(Display *dpy, Window popup) {
    if (g_grab_pointer) XUngrabPointer(dpy, CurrentTime);
    if (g_grab_keyboard) XUngrabKeyboard(dpy, CurrentTime);
    XDestroyWindow(dpy, popup);
    popup_lock_release();
}

/* REAL, NEW 2026-08-04, direct instruction ("allow user editing of
 * asset... place an asset in asset folder of entity, and in .pal
 * specify emoji other than default, or path of asset (jpg/png), it will
 * use that instead of default emoji"). Real, simple convention:
 * <package_dir>/asset.pal, key=value lines:
 *   glyph=<emoji>          - regenerate sprite.csv from a DIFFERENT
 *                            emoji than glyph.txt's own default, via the
 *                            same real emoji_gen_atlas/emoji_xtract
 *                            pipeline tp_place_desktop.c already uses.
 *   asset_path=<path>      - use a real, arbitrary user image (PNG/JPG,
 *                            any size - NOT required to be pre-cropped
 *                            to 64x64) instead of any emoji at all, via
 *                            the NEW tp_asset_to_sprite.+x (see that
 *                            file's own header for why emoji_xtract.+x
 *                            alone isn't the right tool for an arbitrary
 *                            user image). Relative paths resolve against
 *                            package_dir/assets/ (the real "asset
 *                            folder" named in the instruction); absolute
 *                            paths are used as-is (e.g. pointing
 *                            directly at a file elsewhere, without
 *                            moving it).
 * Regenerates sprite.csv in place, once, at window startup - editing
 * asset.pal and relaunching the window picks up the change. */
static void apply_asset_override(const char *package_dir, const char *ops_dir) {
    char asset_pal[TP_PATH_BUF];
    snprintf(asset_pal, sizeof(asset_pal), "%s/asset.pal", package_dir);
    FILE *f = fopen(asset_pal, "r");
    if (!f) return;

    char glyph_override[64] = "", asset_path_raw[TP_PATH_BUF] = "";
    char line[TP_PATH_BUF];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strncmp(line, "glyph=", 6) == 0) snprintf(glyph_override, sizeof(glyph_override), "%s", line + 6);
        else if (strncmp(line, "asset_path=", 11) == 0) snprintf(asset_path_raw, sizeof(asset_path_raw), "%s", line + 11);
    }
    fclose(f);

    char sprite_path[TP_PATH_BUF];
    snprintf(sprite_path, sizeof(sprite_path), "%s/sprite.csv", package_dir);

    if (asset_path_raw[0]) {
        char resolved[TP_PATH_BUF];
        if (asset_path_raw[0] == '/') {
            snprintf(resolved, sizeof(resolved), "%s", asset_path_raw);
        } else {
            snprintf(resolved, sizeof(resolved), "%s/assets/%s", package_dir, asset_path_raw);
        }
#ifndef _WIN32
        char cmd[TP_PATH_BUF * 2];
        snprintf(cmd, sizeof(cmd), "'%s/tp_asset_to_sprite.+x' '%s' 64 '%s' >/dev/null 2>&1",
                 ops_dir, resolved, sprite_path);
        int rc = system(cmd);
        (void)rc;
#else
        (void)ops_dir;
        (void)resolved;
        /* Existing sprite.csv is used. Do not system() Linux .+x via cmd.exe
         * (0x800700E8 / pipe closed) — that was killing rgb at startup. */
#endif
    } else if (glyph_override[0]) {
#ifndef _WIN32
        char atlas_path[TP_PATH_BUF], cmd[TP_PATH_BUF * 2];
        snprintf(atlas_path, sizeof(atlas_path), "%s/atlas_override.png", package_dir);
        snprintf(cmd, sizeof(cmd),
                 "'%s/emoji_gen_atlas.+x' '%s' '%s' >/dev/null 2>&1 && "
                 "'%s/emoji_xtract.+x' '%s' 0 64 '%s' >/dev/null 2>&1",
                 ops_dir, glyph_override, atlas_path, ops_dir, atlas_path, sprite_path);
        int rc = system(cmd);
        (void)rc;
#else
        (void)ops_dir;
#endif
    }
}

/* REAL, NEW 2026-08-31, direct instruction ("do we have z layers
 * yet? ... the xelector/cursword moves up and down z levels but the
 * rest of the entities should remain on their own z level unless
 * some event is otherwise moving them") - a real, persistent per-
 * entity Z, same real file (desktop_pos.txt) every entity already
 * has, a real optional third `z=N` line (missing = 0, same real
 * backward-compatible fallback shape every other optional PDL/state
 * key in this house already uses). g_entity_z is this real, in-
 * memory value for THIS process's own entity - loaded once at
 * startup, changed only by cursword's own real c/v keys (this
 * entity's own z never changes on its own). */
static int g_entity_z = 0;

static int read_entity_z(const char *package_dir) {
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/desktop_pos.txt", package_dir);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[128];
    int z = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "z=", 2) == 0) z = atoi(line + 2);
    }
    fclose(f);
    return z;
}

static void write_pos(const char *package_dir, int x, int y) {
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/desktop_pos.txt", package_dir);
    FILE *f = fopen(path, "w");
    if (!f) return;
    /* Real, deliberate: write THIS process's own real g_entity_z, not
     * a re-read of whatever was on disk before - this file's own
     * g_entity_z is always kept in sync with disk on the only real
     * path that ever changes it (cursword's own c/v keys, which write
     * immediately), so this is never stale. Every other real caller
     * of write_pos() (drag/arrow-nudge/click-to-place) only ever
     * changes x/y, never z - preserving it here, with zero call-site
     * changes needed anywhere else in this file. */
    fprintf(f, "x=%d\ny=%d\nz=%d\n", x, y, g_entity_z);
    fclose(f);
}

/* Real, new 2026-08-31 - the shared, desktop-wide "which z level is
 * currently visible" file (same real "small state file under
 * #.desktop/" convention as desktop_camera_mode.txt). Cursword, the
 * real xelector/selector entity, is the only thing that ever WRITES
 * this (see cursword_handle_camera_key()'s own c/v branch) - every
 * entity's own window just reads it to decide whether to show or
 * hide itself (see the real map/unmap logic in the main render loop). */
static int g_active_z = 0;
static void load_active_z(const char *house_root) {
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/desktop_active_z.txt", house_root);
    FILE *f = fopen(path, "r");
    if (!f) { g_active_z = 0; return; }
    char line[16];
    g_active_z = fgets(line, sizeof(line), f) ? atoi(line) : 0;
    fclose(f);
}

static int tp_main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: tp_desktop_window.+x <package_dir>\n");
        return 1;
    }
    signal(SIGTERM, handle_shutdown_signal);
    signal(SIGINT, handle_shutdown_signal);
    char package_buf[TP_PATH_BUF];
    snprintf(package_buf, sizeof(package_buf), "%s", argv[1]);
#ifdef _WIN32
    win_package_rel(package_buf);
#endif
    const char *package_dir = package_buf;
    /* Real, one-time identity check - see g_is_cursword's own
     * declaration comment for why this is scoped to cursword only,
     * not every desktop entity. */
    {
        char pkgcopy[TP_PATH_BUF];
        snprintf(pkgcopy, sizeof(pkgcopy), "%s", package_dir);
        g_is_cursword = (strcmp(basename(pkgcopy), "cursword") == 0);
    }
    snprintf(g_history_path, sizeof(g_history_path), "%s/history.txt", package_dir);
    snprintf(g_relay_path, sizeof(g_relay_path), "%s/interact_relay.txt", package_dir);
    append_history("WINDOW_OPEN");
    char g_ops_dir[TP_PATH_BUF];
    resolve_livedesk_paths(g_ops_dir, sizeof(g_ops_dir), g_house_root, sizeof(g_house_root));
#ifdef _WIN32
    if (!g_house_root[0]) snprintf(g_house_root, sizeof(g_house_root), ".");
#endif
    snprintf(g_house_root_for_lock, sizeof(g_house_root_for_lock), "%s", g_house_root);
    if (g_house_root[0]) desktop_load_click_two_step(g_house_root);
    if (g_house_root[0]) load_override_redirect(g_house_root);
    if (g_house_root[0] && g_is_cursword) cursword_load_move_mode(g_house_root);
    /* REAL FIX 2026-08-27 (TILE-SYSTEM-DESIGN.md §0a) - read the real,
     * optional, house-wide grid cell size as early as possible (right
     * after g_house_root resolves, before anything below uses
     * GRID_CELL_PX). */
    GRID_CELL_PX = read_grid_cell_px(g_house_root);
    /* Stage 2c PROOF - see launch_khtpm_menu()'s own header comment. */
    {
        char menu_chtpm_path[TP_PATH_BUF];
        snprintf(menu_chtpm_path, sizeof(menu_chtpm_path), "%s/menu.chtpm", package_dir);
        if (access(menu_chtpm_path, F_OK) == 0) {
            g_use_khtpm_menu = 1;
            snprintf(g_khtpm_menu_pkg_dir, sizeof(g_khtpm_menu_pkg_dir), "%s", package_dir);
            snprintf(g_khtpm_menu_house_root, sizeof(g_khtpm_menu_house_root), "%s", g_house_root);
        }
    }
    int g_livedesk_index = 0;
    if (g_house_root[0]) {
        g_livedesk_index = ensure_livedesk_index(package_dir, g_house_root);
        livedesk_registry_add(g_house_root, package_dir, g_livedesk_index, getpid());
        ensure_taskbar_running(g_house_root);
        append_history("LIVEDESK_INDEX=%d", g_livedesk_index);
    }
    /* REAL FIX 2026-08-05 (MUCHI_RANCHER monsters), EXTENDED 2026-08-29
     * direct live report ("placing a tile isn't taking up the full
     * 80px tile square... all entities need this fix except muchi
     * mon"): the original fix only overrode the flat 64px WIN_PX when
     * footprint_tiles > 1, so every footprint=1 entity (every pet/asa/
     * ava/tile - everything except MUCHI_RANCHER monsters) rendered at
     * a fixed 64px regardless of the real, configurable GRID_CELL_PX
     * (80 by default) - a real, visible gap against the grid for
     * anything that's supposed to tile edge-to-edge (like an rmmv
     * floor tile). Real fix: always derive WIN_PX from GRID_CELL_PX *
     * footprint_tiles (footprint_tiles defaults to 1, so this is
     * exactly GRID_CELL_PX for every existing footprint=1 entity, and
     * unchanged footprint_tiles*GRID_CELL_PX for monsters). */
    {
        int footprint_tiles = read_footprint_tiles(package_dir);
        if (footprint_tiles < 1) footprint_tiles = 1;
        WIN_PX = footprint_tiles * GRID_CELL_PX;
    }
    read_menu_config(package_dir);
    /* REAL FIX 2026-08-04, direct instruction ("id like to see emojis
     * tho"): glyph.txt can now hold a real multi-byte UTF-8 emoji -
     * kept as a full string, not truncated to its first byte. Real
     * texture rendering of that emoji (via emoji_gen_atlas.+x, this
     * house's own FreeType+NotoColorEmoji pipeline, already used by
     * chtpm_rgb_render) is NOT done here yet - this window still only
     * shows a glyph-hashed color + the glyph as its window title, same
     * as before this fix. Flagged as still-open, tracked alongside the
     * tp_menu_input.c/tp_compose_frame.c glyph-widening this session
     * already did for the picker's own chtpm-rendered path (which DOES
     * show real emoji, via that same already-working renderer). */
    char glyph_line[64];
    read_glyph(package_dir, glyph_line, sizeof(glyph_line));
    char glyph = glyph_line[0] ? glyph_line[0] : '?';
    const char *glyph_str = glyph_line[0] ? glyph_line : "?";

    setlocale(LC_ALL, "");
    XSetLocaleModifiers("");
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "tp_desktop_window: cannot open display\n");
        return 1;
    }
    load_popup_fontset(dpy);

    int screen_num = DefaultScreen(dpy);
    Visual *vis = DefaultVisual(dpy, screen_num);
    int depth = DefaultDepth(dpy, screen_num);

    /* REAL, NEW 2026-08-30, direct instruction ("Do the real ARGB
     * transparency") - a real 32-bit ARGB TrueColor visual for
     * cursword's own window ONLY (every other entity keeps the plain
     * default visual/depth above, completely untouched). Real per-
     * pixel alpha this way is handled by the COMPOSITOR itself
     * (mutter/XWayland here) directly from this window's own backing
     * pixels - no XRenderComposite call needed on this side, the same
     * standard technique real transparent-window apps use. XMatchVisualInfo
     * with class=TrueColor, depth=32 is the common, simple way to find
     * it (virtually universal under a modern compositing X/XWayland
     * setup) - if it's ever unavailable, this falls back to the exact
     * same plain visual/depth every other entity already uses, so
     * cursword just loses real transparency (back to the flat-color
     * disc), never crashes/breaks. */
    XVisualInfo argb_vinfo;
    int have_argb_visual = 0;
    if (g_is_cursword)
        have_argb_visual = XMatchVisualInfo(dpy, screen_num, 32, TrueColor, &argb_vinfo);
    Visual *win_vis = have_argb_visual ? argb_vinfo.visual : vis;
    int win_depth = have_argb_visual ? argb_vinfo.depth : depth;

    XSetWindowAttributes swa;
    swa.colormap = XCreateColormap(dpy, RootWindow(dpy, screen_num), win_vis, AllocNone);
    /* Real, new 2026-08-30, direct instruction ("if cursword loses
     * focus... is there a way to make sure the halo goes away") -
     * FocusChangeMask added house-wide (every entity now gets real
     * FocusIn/FocusOut events, harmless no-op for every entity except
     * cursword, which is the only one that ever acts on them - see
     * the FocusOut handler in the main event loop below). */
    swa.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | ButtonMotionMask | KeyPressMask | FocusChangeMask;
    swa.override_redirect = g_override_redirect; /* real X11 requirement whenever a window's own depth differs from its parent's (root's) - harmless to set unconditionally */
    swa.background_pixel = 0;

    Window win = XCreateWindow(dpy, RootWindow(dpy, screen_num), 3 * GRID_CELL_PX, 3 * GRID_CELL_PX, WIN_PX, WIN_PX,
                                0, win_depth, InputOutput, win_vis,
                                CWColormap | CWEventMask | CWOverrideRedirect | CWBorderPixel | CWBackPixel, &swa);
    /* REAL, NEW 2026-09-01 - when the pdl turns override_redirect off
     * (WM-managed pieces, so the taskbar's @ toggle can control their
     * real z-order on Xwayland/Mutter), Mutter would put a titlebar/frame
     * + taskbar entry on them. Same real house fix open-hai/the taskbar
     * use: _MOTIF_WM_HINTS flags=MWM_HINTS_DECORATIONS(2),
     * decorations=0 - managed window, but zero WM-drawn chrome, keeping
     * the borderless tile look. No-op for the default override_redirect
     * path (a WM never manages those, the hint is simply ignored). */
    if (!g_override_redirect) {
        Atom motif_hints = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
        long hints[5] = { 2, 0, 0, 0, 0 }; /* flags=MWM_HINTS_DECORATIONS, decorations=0 */
        XChangeProperty(dpy, win, motif_hints, motif_hints, 32, PropModeReplace,
                        (const unsigned char *)hints, 5);
        XSetClassHint(dpy, win, &(XClassHint){(char *)"MuchiverseLivedesk", (char *)"MuchiverseLivedesk"});
    }
    XMapWindow(dpy, win);
    set_window_opacity(dpy, win, tp_load_theme_opacity(g_house_root));
    /* REAL FIX 2026-08-29, direct live report ("entities and tb dropdown
     * cell tabs aren't opaque yet" - i.e. still full opacity) - ported
     * from khtpm_strip_parser.c's own real "KISS opacity-on-reset fix"
     * (2026-08-11, opacity-bug-aug9.txt): Mutter/XWayland does not
     * reliably honor _NET_WM_WINDOW_OPACITY set at map-time, before the
     * window has been visible/painted for at least one real frame -
     * has to be re-applied after a short real delay. This was the
     * actual root cause for db-hq/events-hq/chat-hai/popup too (see
     * OPACITY-PIPELINE-INVESTIGATION-2026-08-29-part3.txt), just never
     * ported to this file, the ONE remaining opacity gap after that
     * fix landed. */
    {
        XFlush(dpy);
        usleep(200000);
        set_window_opacity(dpy, win, tp_load_theme_opacity(g_house_root));
        XFlush(dpy);
    }

    /* RGB compose buffer - all real drawing (background, glyph/sprite)
     * happens into this offscreen Pixmap, presented each frame via
     * XGetImage+XPutImage below, same as db-hq/taskbar. REAL, NEW
     * 2026-08-30: cursword's own buffer reserves CURSWORD_LOG_H extra
     * rows below WIN_PX for the real key-log debug strip (see
     * cursword_log_key()'s own header comment) - always allocated for
     * a cursword instance (cheap, ~80x20px), revealed only while armed
     * via the window resize + shape-mask union in
     * cursword_update_shape(). Every other entity is completely
     * unaffected (g_is_cursword false, buffer stays exactly WIN_PX x
     * WIN_PX as before). */
    Pixmap g_buf = XCreatePixmap(dpy, win, (unsigned)(g_is_cursword ? CURSWORD_LOG_W : WIN_PX),
                                  (unsigned)(WIN_PX + (g_is_cursword ? CURSWORD_LOG_H : 0)), (unsigned)win_depth);
    GC g_buf_gc = XCreateGC(dpy, g_buf, 0, NULL);
    /* REAL FIX 2026-08-05, direct instruction ("context window should
     * have name/id of entity, so its addressable by others"): the
     * window title used to be "tile:<glyph>" only - real piece_id
     * (basename of package_dir, the same id every methods.pdl/meta.pdl/
     * event_pkg already uses) was nowhere in it, so any OTHER process
     * that needs to find a SPECIFIC entity's own window (a future AI
     * tick loop targeting "chase target=cat", a test harness, etc) had
     * no reliable way to address it - glyph alone can collide (two
     * entities could share an emoji) and isn't the entity's own real
     * identity. basename() needs a mutable copy since POSIX basename()
     * may modify its argument. */
    char pkg_copy[TP_PATH_BUF];
    snprintf(pkg_copy, sizeof(pkg_copy), "%s", package_dir);
    const char *piece_id = basename(pkg_copy);
    char instance_id[32];
    read_instance_id(package_dir, instance_id, sizeof(instance_id));
    if (instance_id[0]) snprintf(g_full_id, sizeof(g_full_id), "%s-%s", piece_id, instance_id);
    else snprintf(g_full_id, sizeof(g_full_id), "%s", piece_id);
    char title[192];
    snprintf(title, sizeof(title), "tile:%s:%s", g_full_id, glyph_str);
    XStoreName(dpy, win, title);

    /* Alpha blending against the background is now done per-pixel in
     * draw_sprite_rgb() itself (no GL_BLEND state to set up). */
    float r, g, b;
    glyph_color(glyph, &r, &g, &b);
    g_font_loaded = load_glyph_font(dpy);

    /* Resolve ops_dir (same /proc/self/exe technique tp_place_desktop.c
     * already uses) so apply_asset_override() can find tp_asset_to_
     * sprite.+x/emoji_gen_atlas.+x/emoji_xtract.+x next to this binary -
     * kept around (not scoped to a throwaway block) since
     * load_entity_phymoji() below also needs it, to find
     * sprite_phymoji_gen.+x the same real way. */
    char resolved_ops_dir[TP_PATH_BUF] = "";
    {
        char self_path[TP_PATH_BUF];
        if (self_exe_path(self_path, sizeof(self_path))) {
            char *ops_dir = dirname(self_path);
            snprintf(resolved_ops_dir, sizeof(resolved_ops_dir), "%s", ops_dir);
            apply_asset_override(package_dir, ops_dir);
        }
    }

    char sprite_path[TP_PATH_BUF];
    snprintf(sprite_path, sizeof(sprite_path), "%s/sprite.csv", package_dir);
    g_has_sprite = load_sprite_csv(sprite_path);
    /* Real, new 2026-08-30 - real per-voxel phymoji asset, generated
     * on demand from this entity's own real sprite.csv if it doesn't
     * exist yet (see load_entity_phymoji()/ensure_entity_phymoji_
     * generated()'s own header comments) - loaded once here, cached
     * for the whole process lifetime same as the sprite itself. */
    load_entity_phymoji(package_dir, resolved_ops_dir);

    /* Real window shape from the sprite's own alpha, if we have one -
     * see build_shape_mask()'s own header comment for why GL_BLEND
     * alone wasn't enough. */
    if (g_has_sprite) {
#ifndef _WIN32
        Pixmap shape_mask = XCreatePixmap(dpy, win, WIN_PX, WIN_PX, 1);
        GC shape_gc = XCreateGC(dpy, shape_mask, 0, NULL);
        build_shape_mask(dpy, win, shape_gc, shape_mask);
        XFreeGC(dpy, shape_gc);
        XFreePixmap(dpy, shape_mask);
#endif
        /* Win: per-pixel alpha via UpdateLayeredWindow in XPutImage (XShape shim). */
    }

    /* REAL, NEW 2026-08-30, direct live report ("teh cursword is a
     * very thin png so since its hard to grab, could we make its grab
     * surface wider (like within the halo circle, even when halo
     * isn't visible?)") - X11's Shape extension has TWO independent
     * masks: ShapeBounding (what's drawn/visible - build_shape_mask()'s
     * own sprite-silhouette-only real behavior above, unchanged, so
     * cursword still LOOKS exactly as thin as its sprite) and
     * ShapeInput (what actually receives clicks/pointer events - can
     * be a completely different, WIDER shape with zero visual change).
     * Set once here, real full-circle radius matching the halo ring's
     * own geometry (cursword_update_shape()'s WIN_PX/2-5), so every
     * click anywhere inside that circle - not just on the thin visible
     * pixels - now hits cursword, whether armed or not. Cursword-only
     * (g_is_cursword), every other entity's own real click hit-testing
     * is completely unaffected.
     *
     * REAL FOLLOW-UP FIX 2026-08-30, direct live report ("im still
     * having to click right on the image") - this ShapeInput mask
     * turned out NOT to be honored by the real compositor for genuine
     * mouse clicks (real-world gap, confirmed live) - kept here as a
     * harmless, possibly-helpful-elsewhere redundancy, but
     * cursword_update_shape() below (called once, right after this
     * block) is the REAL fix now: it widens ShapeBOUNDING itself to
     * match, which every compositor DOES reliably honor for click
     * routing, at the cost of a real, always-visible dim backdrop
     * disc (see that function's own header comment for the exact
     * reasoning/color choice). */
    if (g_is_cursword) {
#ifndef _WIN32
        Pixmap input_mask = XCreatePixmap(dpy, win, (unsigned)WIN_PX, (unsigned)WIN_PX, 1);
        GC input_gc = XCreateGC(dpy, input_mask, 0, NULL);
        XSetForeground(dpy, input_gc, 0);
        XFillRectangle(dpy, input_mask, input_gc, 0, 0, WIN_PX, WIN_PX);
        XSetForeground(dpy, input_gc, 1);
        int icx = WIN_PX / 2, icy = WIN_PX / 2;
        int iradius = WIN_PX / 2 - 5;
        XFillArc(dpy, input_mask, input_gc, icx - iradius, icy - iradius,
                 (unsigned)(iradius * 2), (unsigned)(iradius * 2), 0, 360 * 64);
        XShapeCombineMask(dpy, win, ShapeInput, 0, 0, input_mask, ShapeSet);
        XFreeGC(dpy, input_gc);
        XFreePixmap(dpy, input_mask);
#endif
        /* Real fix - widen ShapeBOUNDING too, from the very start (not
         * just after the first arm/disarm), so the wider grab surface
         * is real from cursword's first frame on screen. */
        cursword_update_shape(dpy, win);
    }

    int screen_w = DisplayWidth(dpy, DefaultScreen(dpy));
    int screen_h = DisplayHeight(dpy, DefaultScreen(dpy));
    int max_col = (screen_w / GRID_CELL_PX) - 1;
    int max_row = (screen_h / GRID_CELL_PX) - 1;
    /* REAL, NEW 2026-08-31 ("map size" movement wall, see
     * read_map_size()'s own header comment) - a configured
     * desk_grid.pdl map_cols/map_rows overrides the screen-derived
     * bound above (real, deliberately smaller-or-equal "wall" so an
     * entity dragged/placed/nudged can never end up further out than
     * the configured map, not just the physical screen edge). Every
     * other real clamp site in this function (drag release, arrow-key
     * nudge, click-to-place) already reuses these same max_col/max_row
     * locals, so this one override site is the only real change
     * needed. */
    {
        int cfg_cols = 0, cfg_rows = 0;
        read_map_size(g_house_root, &cfg_cols, &cfg_rows);
        if (cfg_cols > 0) max_col = cfg_cols - 1;
        if (cfg_rows > 0) max_row = cfg_rows - 1;
    }
    if (max_col < 0) max_col = 0;
    if (max_row < 0) max_row = 0;

    int xfd = ConnectionNumber(dpy);
    /* Real, new 2026-08-31 - this entity's own persisted z, loaded
     * once at startup (see g_entity_z's own declaration comment). */
    g_entity_z = read_entity_z(package_dir);
    int win_x = 3 * GRID_CELL_PX, win_y = 3 * GRID_CELL_PX; /* grid-aligned spawn, matching egg_window.c's own default */
    {
        int ix, iy;
        if (read_initial_pos(package_dir, &ix, &iy)) {
            int gx = (ix + GRID_CELL_PX / 2) / GRID_CELL_PX;
            int gy = (iy + GRID_CELL_PX / 2) / GRID_CELL_PX;
            if (gx < 0) gx = 0; if (gx > max_col) gx = max_col;
            if (gy < 0) gy = 0; if (gy > max_row) gy = max_row;
            win_x = gx * GRID_CELL_PX;
            win_y = gy * GRID_CELL_PX;
        }
#ifdef _WIN32
        /* Linux pos can sit past this monitor. Keep on the primary work
         * area, below the strip. */
        {
            int pad_top = 40, g = 8;
            if (win_x < g) win_x = g;
            if (win_y < pad_top) win_y = pad_top;
            if (win_x + WIN_PX > screen_w - g) win_x = screen_w - WIN_PX - g;
            if (win_y + WIN_PX > screen_h - g) win_y = screen_h - WIN_PX - g;
            if (win_x < g) win_x = g;
            if (win_y < pad_top) win_y = pad_top;
        }
#endif
#ifdef __APPLE__
        /* macOS leg (2026-08-22): mirror of the _WIN32 work-area clamp.
         * Saved Linux grid positions can sit past this display's right
         * edge (live: tiles parked at x=1600 on a 1680px screen, mostly
         * invisible). XQuartz rootless maps y=0 to just under the macOS
         * menu bar, so pad_top only needs to clear the taskbar strip. */
        {
            int pad_top = 40, g = 8;
            if (win_x < g) win_x = g;
            if (win_y < pad_top) win_y = pad_top;
            if (win_x + WIN_PX > screen_w - g) win_x = screen_w - WIN_PX - g;
            if (win_y + WIN_PX > screen_h - g) win_y = screen_h - WIN_PX - g;
            if (win_x < g) win_x = g;
            if (win_y < pad_top) win_y = pad_top;
        }
#endif
        XMoveWindow(dpy, win, win_x, win_y);
        /* macOS leg (2026-08-22): persist the CLAMPED position - the
         * saved Linux grid value can sit past this display's edge, and
         * downstream consumers (khtpm_show_choices.c's picker spawn
         * reads this same file) must not inherit an off-screen x/y. */
        write_pos(package_dir, win_x, win_y);
    }
    MethodItem methods[MAX_METHODS];
    int n_methods = load_methods(package_dir, methods, MAX_METHODS);
    if (n_methods == 0) {
        snprintf(methods[0].label, sizeof(methods[0].label), "Close");
        snprintf(methods[0].action, sizeof(methods[0].action), "CLOSE");
        n_methods = 1;
    }
    /* REAL, 2026-08-05: optional multi-page objects.pdl overrides the
     * single-page methods.pdl list above - see load_objects()'s own
     * header comment. methods[]/n_methods become "whichever page is
     * currently open"'s own item list when objects.pdl exists, so every
     * existing render/dispatch/relay code below that already reads
     * methods[]/n_methods keeps working completely unchanged. */
    ObjPage obj_pages[MAX_PAGES];
    int n_obj_pages = load_objects(package_dir, obj_pages, MAX_PAGES);
    int using_objects = (n_obj_pages > 0);
    int cur_page = 0;
    int page_stack[MAX_PAGES];
    int page_stack_n = 0;
    if (using_objects) {
        n_methods = obj_pages[0].n_items;
        for (int i = 0; i < n_methods; i++) methods[i] = obj_pages[0].items[i];
    }
    int input_active = 0;
    char input_key[64] = "";
    char input_buffer[256] = "";
    Window input_popup_win = 0;
    Window popup_win = 0;
    int popup_nav_base = 0;
    int popup_focus_row = 0;
    int popup_digit_accum = 0; /* chtpm: digits move [>] before Enter */
    /* REAL, 2026-08-05, direct instruction ("book stack emoji... give
     * me option to generate a random verse... entirely with khtpm /
     * eventscript page, and pal"): real Show Choices support - a
     * SEPARATE process (e.g. a real event.pal's own exec'd op) can ask
     * THIS already-running window to present a choice popup via a new
     * SHOW_PAGE relay command, reusing the exact same popup_win/
     * methods[]/n_methods machinery the entity's own normal context
     * menu already uses. choice_mode gates the row-activation dispatch
     * sites (mouse click / RUN_METHOD / ACTIVATE_NAV / Enter) to write
     * the picked row's action to choice_result_path instead of running
     * the normal CLOSE/void/GOTO/etc dispatch. */
    int choice_mode = 0;
    char choice_result_path[TP_PATH_BUF] = "";
    Window text_popup_win = 0;
    GC popup_gc = XCreateGC(dpy, RootWindow(dpy, DefaultScreen(dpy)), 0, NULL);

    /* REAL, 2026-08-05, direct instruction ("lets just open an adjacent
     * contextmenu (with cancel button) beside the first one for now, so
     * we can test the range finder visual"): quick, real submenu using
     * the SAME already-working raw X11 popup mechanism (open_context_
     * menu/draw_context_menu), not a new CHTPM screen (see hikkikomorai/
     * x11-mouse-2do.txt for why the CHTPM route was rolled back -
     * gl_mirror.c has no mouse-click support and no configurable
     * window size). A second, fixed-content popup opens ADJACENT to
     * the first when "User" is clicked; "Move" inside it opens a third
     * small popup drawing a real range-finder grid. */
    Window user_popup_win = 0;
    int user_popup_x = 0, user_popup_y = 0;
    MethodItem user_methods[4];
    snprintf(user_methods[0].label, sizeof(user_methods[0].label), "Move");
    snprintf(user_methods[0].action, sizeof(user_methods[0].action), "OPEN_RANGE_GRID");
    snprintf(user_methods[1].label, sizeof(user_methods[1].label), "Inventory");
    snprintf(user_methods[1].action, sizeof(user_methods[1].action), "void");
    snprintf(user_methods[2].label, sizeof(user_methods[2].label), "Skill");
    snprintf(user_methods[2].action, sizeof(user_methods[2].action), "void");
    snprintf(user_methods[3].label, sizeof(user_methods[3].label), "Cancel");
    snprintf(user_methods[3].action, sizeof(user_methods[3].action), "void");

    int popup_x = 0, popup_y = 0; /* where the MAIN popup itself opened, for real submenu adjacency */
    int dragging = 0, drag_start_x = 0, drag_start_y = 0;
    /* Real click-vs-drag distinction, cursword only (see
     * g_is_cursword/CURSWORD_CLICK_MAX_PX/MS declaration comments) -
     * press_root_x/y are the RAW screen coords at ButtonPress (never
     * updated during a drag, unlike drag_start_x/y above which slides
     * forward every MotionNotify) so ButtonRelease can measure total
     * real distance traveled, and press_tv is the real press timestamp
     * for the elapsed-time half of the same check. */
    int press_root_x = 0, press_root_y = 0;
    struct timeval press_tv = {0, 0};
    int running = 1;
    struct timeval last_frame = { 0, 0 };

    while (running && !g_shutdown_requested) {
#ifdef _WIN32
        x11_wait(dpy, POLL_INTERVAL_USEC);
        (void)xfd;
#else
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(xfd, &fds);
        struct timeval tv = { 0, POLL_INTERVAL_USEC };
        select(xfd + 1, &fds, NULL, NULL, &tv);
#endif

        int need_redraw = 0;
        /* REAL FIX 2026-09-01 (live report: after the @ always-on-top
         * toggle respawned every entity at once, all but cursword sat
         * blank until an unrelated click elsewhere happened to trip a
         * dirty check - "should be immediate, not wait for next
         * click"). This "always paint the very first frame" seed only
         * existed on the _WIN32 branch below - on Linux, a freshly-
         * started process's need_redraw stayed 0 until SOME dirty
         * condition fired (an Expose event, theme/camera change,
         * etc.). A single normal launch usually gets away with it
         * (XMapWindow's own Expose arrives fast enough to be
         * unnoticeable), but respawning many entities at once creates
         * real X11-server/CPU contention that can visibly delay it -
         * pre-existing, not introduced by this session's consolidation
         * work (this file's own redraw logic is otherwise untouched).
         * Real fix: the same real seed, unconditional (not _WIN32-
         * only) - matches the already-real intent of last_frame's own
         * zero-init check. */
        if (last_frame.tv_sec == 0) need_redraw = 1;

        /* Real, cheap, event-driven opacity reapply - see
         * theme_changed_dirty()'s own declaration comment. */
        if (theme_changed_dirty(g_house_root)) {
            set_window_opacity(dpy, win, tp_load_theme_opacity(g_house_root));
        }

        /* Real, cheap, event-driven camera pan/tilt/mode reapply - see
         * camera_changed_dirty()'s own declaration comment (without
         * this, an idle entity nobody's touching never redraws even
         * when the shared camera state moves). */
        if (camera_changed_dirty(g_house_root)) {
            load_camera_mode(g_house_root);
            load_camera_state(g_house_root);
            load_active_z(g_house_root);
            need_redraw = 1;
        }

        /* REAL, 2026-08-05: poll interact_relay.txt for an injected
         * command - the "AI-injection power" half of this window's own
         * new CHTPM-parity work (see this file's header comment on
         * g_relay_path/append_history). A human, script, or AI agent
         * writes one command line into <package_dir>/interact_relay.txt;
         * this loop (already running every POLL_INTERVAL_USEC via the
         * select() above) picks it up, dispatches it exactly like a
         * real click would, logs it to history.txt, then truncates the
         * relay file so the same command isn't re-consumed next tick -
         * the same real "write once, consumed once" shape this house's
         * other relay files already use. */
        {
            struct stat relay_st;
            if (stat(g_relay_path, &relay_st) == 0 && relay_st.st_size > 0) {
                char line[TP_PATH_BUF];
                line[0] = '\0';
                FILE *rf = fopen(g_relay_path, "r");
                if (rf) {
                    if (!fgets(line, sizeof(line), rf)) line[0] = '\0';
                    fclose(rf);
                }
                FILE *tf = fopen(g_relay_path, "w");
                if (tf) fclose(tf);
                line[strcspn(line, "\n")] = '\0';
                if (line[0]) {
                    append_history("INJECTED: %s", line);
                    if (strcmp(line, "RAISE") == 0) {
                        /* Single-instance spawn support (2026-08-24,
                         * cursword HQ row): the taskbar writes RAISE into
                         * this relay instead of spawning a second
                         * tp_desktop_window_rgb when this entity is
                         * already open. Raise own main window to the top
                         * of the stack - stacking manipulation from the
                         * entity's own X connection, no cross-process
                         * window plumbing. Deliberately NO
                         * XSetInputFocus/XGrabKeyboard here: this is an
                         * override_redirect window, exempt from WM focus
                         * handling entirely (see db-hq's main() header
                         * comment for the full Mutter investigation) -
                         * real keyboard focus was never achievable for
                         * these windows; raise-to-top IS the whole
                         * observable "focus" behavior, and a human click
                         * still lands keyboard where Mutter allows it. */
                        XRaiseWindow(dpy, win);
                        /* REAL, NEW 2026-08-31, direct instruction ("when
                         * i click it from tb it should go back to a
                         * familiar location") - RAISE is ONLY ever sent
                         * for cursword's own single-instance re-click
                         * (khtpm_taskbar_manager.c's livedesk:spawn-
                         * cursword handler, the only real caller of this
                         * relay command house-wide), so this is real,
                         * safe, and cursword-only without an explicit
                         * g_is_cursword check. If it ever got dragged/
                         * nudged off into a weird spot (or left there by
                         * stale test/camera-pan state), a re-click now
                         * also snaps it straight back to its real pinned
                         * home (same CURSWORD_HOME_X/Y convention
                         * livedesk_ensure_cursword() already uses on
                         * respawn) - "always findable in the same spot"
                         * now also means "clicking it finds it," not
                         * just "it's always open." */
                        win_x = 0; win_y = 0;
                        XMoveWindow(dpy, win, win_x, win_y);
                        write_pos(package_dir, win_x, win_y);
                        XFlush(dpy);
                    } else if (strncmp(line, "RUN_METHOD:", 11) == 0) {
                        const char *label = line + 11;
                        for (int i = 0; i < n_methods; i++) {
                            if (strcmp(methods[i].label, label) != 0) continue;
                            /* Same GOTO:/BACK/STATE: reserved forms the
                             * real click path handles (see the
                             * ButtonPress branch below) - relay
                             * injection dispatches a page navigation or
                             * input-activation row exactly like a real
                             * click would, not just plain commands. */
                            if (using_objects && strncmp(methods[i].action, "GOTO:", 5) == 0) {
                                const char *target = methods[i].action + 5;
                                for (int pi = 0; pi < n_obj_pages; pi++) {
                                    if (strcmp(obj_pages[pi].name, target) == 0) {
                                        if (page_stack_n < MAX_PAGES) page_stack[page_stack_n++] = cur_page;
                                        cur_page = pi;
                                        n_methods = obj_pages[cur_page].n_items;
                                        for (int k = 0; k < n_methods; k++) methods[k] = obj_pages[cur_page].items[k];
                                        /* Same real fix as the mouse-click
                                         * path above: make the page switch
                                         * actually visible, don't just
                                         * silently update methods[]. */
                                        if (popup_win) close_context_menu(dpy, popup_win);
                                        else { popup_x = win_x; popup_y = win_y; }
                                        popup_win = open_context_menu(dpy, popup_gc, &popup_x, &popup_y, n_methods, methods);
                                        popup_nav_base = nav_claim_rows(g_house_root, getpid(), package_dir, methods, n_methods);
                                        popup_focus_row = 0; popup_digit_accum = 0;
                                        break;
                                    }
                                }
                            } else if (using_objects && strcmp(methods[i].action, "BACK") == 0) {
                                if (page_stack_n > 0) {
                                    cur_page = page_stack[--page_stack_n];
                                    n_methods = obj_pages[cur_page].n_items;
                                    for (int k = 0; k < n_methods; k++) methods[k] = obj_pages[cur_page].items[k];
                                    if (popup_win) close_context_menu(dpy, popup_win);
                                    else { popup_x = win_x; popup_y = win_y; }
                                    popup_win = open_context_menu(dpy, popup_gc, &popup_x, &popup_y, n_methods, methods);
                                    popup_nav_base = nav_claim_rows(g_house_root, getpid(), package_dir, methods, n_methods);
                                        popup_focus_row = 0; popup_digit_accum = 0;
                                }
                            } else if (using_objects && strncmp(methods[i].action, "STATE:", 6) == 0) {
                                snprintf(input_key, sizeof(input_key), "%s", methods[i].action + 6);
                                input_buffer[0] = '\0';
                                input_active = 1;
                                append_history("INPUT_ACTIVATE key=%s", input_key);
                                if (!input_popup_win) {
                                    input_popup_win = open_context_menu(dpy, popup_gc, (int[]){win_x}, (int[]){win_y + WIN_PX + 4}, 1, NULL) /* writeback discarded */;
                                }
                            } else {
                                dispatch_action(methods[i].action, package_dir, g_house_root, &running);
                            }
                            break;
                        }
                    } else if (strncmp(line, "ACTIVATE_NAV:", 13) == 0) {
                        /* REAL, 2026-08-05 (TILE_PICKER_DESIGN.md §13):
                         * the REMOTE half of the shared live nav pool -
                         * the taskbar's own terminal input writes this
                         * exact command into whichever window's
                         * interact_relay.txt actually owns the typed
                         * number (looked up from #.desktop/
                         * livedesk_nav_claims.txt), so pressing Enter on
                         * a number can activate a row inside a DIFFERENT
                         * window's currently-open menu, not just this
                         * process's own. Only meaningful while popup_win
                         * is open and N falls in this popup's own
                         * currently-claimed range - otherwise a stale/
                         * mistargeted command is silently ignored. */
                        int nav_n = atoi(line + 13);
                        if (popup_win && nav_n >= popup_nav_base && nav_n < popup_nav_base + n_methods) {
                            int row = nav_n - popup_nav_base;
                            close_context_menu(dpy, popup_win);
                            popup_win = 0;
                            nav_release_pid(g_house_root, getpid());
                            append_history("CLICK(nav) method=%s action=%s", methods[row].label, methods[row].action);
                            if (strcmp(methods[row].action, "CLOSE") == 0) {
                                running = 0;
                            } else if (strcmp(methods[row].action, "void") == 0) {
                                /* no-op */
                            } else if (using_objects && strncmp(methods[row].action, "GOTO:", 5) == 0) {
                                const char *target = methods[row].action + 5;
                                for (int pi = 0; pi < n_obj_pages; pi++) {
                                    if (strcmp(obj_pages[pi].name, target) == 0) {
                                        if (page_stack_n < MAX_PAGES) page_stack[page_stack_n++] = cur_page;
                                        cur_page = pi;
                                        n_methods = obj_pages[cur_page].n_items;
                                        for (int k = 0; k < n_methods; k++) methods[k] = obj_pages[cur_page].items[k];
                                        popup_win = open_context_menu(dpy, popup_gc, &popup_x, &popup_y, n_methods, methods);
                                        popup_nav_base = nav_claim_rows(g_house_root, getpid(), package_dir, methods, n_methods);
                                        popup_focus_row = 0; popup_digit_accum = 0;
                                        break;
                                    }
                                }
                            } else if (using_objects && strcmp(methods[row].action, "BACK") == 0) {
                                if (page_stack_n > 0) {
                                    cur_page = page_stack[--page_stack_n];
                                    n_methods = obj_pages[cur_page].n_items;
                                    for (int k = 0; k < n_methods; k++) methods[k] = obj_pages[cur_page].items[k];
                                    popup_win = open_context_menu(dpy, popup_gc, &popup_x, &popup_y, n_methods, methods);
                                    popup_nav_base = nav_claim_rows(g_house_root, getpid(), package_dir, methods, n_methods);
                                        popup_focus_row = 0; popup_digit_accum = 0;
                                }
                            } else if (using_objects && strncmp(methods[row].action, "STATE:", 6) == 0) {
                                snprintf(input_key, sizeof(input_key), "%s", methods[row].action + 6);
                                input_buffer[0] = '\0';
                                input_active = 1;
                                append_history("INPUT_ACTIVATE key=%s", input_key);
                                if (!input_popup_win) {
                                    input_popup_win = open_context_menu(dpy, popup_gc, (int[]){win_x}, (int[]){win_y + WIN_PX + 4}, 1, NULL) /* writeback discarded */;
                                }
                            } else {
                                dispatch_action(methods[row].action, package_dir, g_house_root, &running);
                            }
                        }
                    } else if (strncmp(line, "FOCUS_NAV:", 10) == 0) {
                        /* Move [>] only (chtpm digit-jump). No activate. */
                        int nav_n = atoi(line + 10);
                        if (popup_win && nav_n >= popup_nav_base && nav_n < popup_nav_base + n_methods) {
                            popup_focus_row = nav_n - popup_nav_base;
                            popup_digit_accum = nav_n;
                            draw_context_menu(dpy, popup_win, popup_gc, methods, n_methods, popup_nav_base, popup_focus_row);
                            append_history("FOCUS_NAV %d -> row %d", nav_n, popup_focus_row);
                        }
                    } else if (strncmp(line, "NAV_KEY:", 8) == 0) {
                        /* REAL, 2026-08-06, direct instruction ("what if
                         * we logged keybord input when toolbar is on,
                         * and it will be sent to our khtpm from
                         * master-ledger for desktop to our own .txt...
                         * focus giving us our own control of this"):
                         * the real fix for XWayland's grab restrictions
                         * turned out incomplete in practice - so instead
                         * of fighting the compositor for a real
                         * XGrabKeyboard, the taskbar's own input box
                         * (which DOES reliably hold real X focus, no
                         * grab needed - see tp_taskbar.c) becomes a
                         * remote keyboard for whichever entity currently
                         * has a popup open, relaying Up/Down/Enter/
                         * Escape here exactly like a real local KeyPress
                         * would - same dispatch logic as the popup_win
                         * KeyPress branch below, just triggered
                         * remotely instead of from this process's own
                         * X queue. Only meaningful while popup_win is
                         * actually open here - otherwise silently
                         * ignored (stale/mistargeted, same rule
                         * ACTIVATE_NAV already follows). */
                        char navkey[16];
                        snprintf(navkey, sizeof(navkey), "%s", line + 8);
                        navkey[strcspn(navkey, "\r\n")] = '\0';
                        if (popup_win) {
                            if (strcmp(navkey, "Escape") == 0) {
                                close_context_menu(dpy, popup_win);
                                popup_win = 0;
                                nav_release_pid(g_house_root, getpid());
                                need_redraw = 1;
                            } else if (strcmp(navkey, "Up") == 0) {
                                if (n_methods > 0) {
                                    popup_focus_row = (popup_focus_row - 1 + n_methods) % n_methods;
                                    popup_digit_accum = 0;
                                    draw_context_menu(dpy, popup_win, popup_gc, methods, n_methods, popup_nav_base, popup_focus_row);
                                }
                            } else if (strcmp(navkey, "Down") == 0) {
                                if (n_methods > 0) {
                                    popup_focus_row = (popup_focus_row + 1) % n_methods;
                                    popup_digit_accum = 0;
                                    draw_context_menu(dpy, popup_win, popup_gc, methods, n_methods, popup_nav_base, popup_focus_row);
                                }
                            } else if (strcmp(navkey, "Enter") == 0) {
                                int row = popup_focus_row;
                                close_context_menu(dpy, popup_win);
                                popup_win = 0;
                                nav_release_pid(g_house_root, getpid());
                                if (choice_mode) {
                                    FILE *rf5 = fopen(choice_result_path, "w");
                                    if (rf5) { fprintf(rf5, "%s\n", methods[row].action); fclose(rf5); }
                                    append_history("SHOW_PAGE_PICK(navkey) action=%s -> %s", methods[row].action, choice_result_path);
                                    choice_mode = 0;
                                    need_redraw = 1;
                                    goto skip_navkey_enter_dispatch;
                                }
                                append_history("CLICK(navkey) method=%s action=%s", methods[row].label, methods[row].action);
                                if (strcmp(methods[row].action, "CLOSE") == 0) {
                                    running = 0;
                                } else if (strcmp(methods[row].action, "void") == 0) {
                                    /* no-op */
                                } else if (strcmp(methods[row].action, "OPEN_USER") == 0) {
                                    user_popup_x = popup_x + g_popup_w + 4;
                                    user_popup_y = popup_y;
                                    user_popup_win = open_context_menu(dpy, popup_gc, &user_popup_x, &user_popup_y, 4, user_methods);
                                } else if (using_objects && strncmp(methods[row].action, "GOTO:", 5) == 0) {
                                    const char *target = methods[row].action + 5;
                                    for (int pi = 0; pi < n_obj_pages; pi++) {
                                        if (strcmp(obj_pages[pi].name, target) == 0) {
                                            if (page_stack_n < MAX_PAGES) page_stack[page_stack_n++] = cur_page;
                                            cur_page = pi;
                                            n_methods = obj_pages[cur_page].n_items;
                                            for (int k = 0; k < n_methods; k++) methods[k] = obj_pages[cur_page].items[k];
                                            popup_win = open_context_menu(dpy, popup_gc, &popup_x, &popup_y, n_methods, methods);
                                            popup_nav_base = nav_claim_rows(g_house_root, getpid(), package_dir, methods, n_methods);
                                            popup_focus_row = 0; popup_digit_accum = 0;
                                            break;
                                        }
                                    }
                                } else if (using_objects && strcmp(methods[row].action, "BACK") == 0) {
                                    if (page_stack_n > 0) {
                                        cur_page = page_stack[--page_stack_n];
                                        n_methods = obj_pages[cur_page].n_items;
                                        for (int k = 0; k < n_methods; k++) methods[k] = obj_pages[cur_page].items[k];
                                        popup_win = open_context_menu(dpy, popup_gc, &popup_x, &popup_y, n_methods, methods);
                                        popup_nav_base = nav_claim_rows(g_house_root, getpid(), package_dir, methods, n_methods);
                                        popup_focus_row = 0; popup_digit_accum = 0;
                                    }
                                } else if (using_objects && strncmp(methods[row].action, "STATE:", 6) == 0) {
                                    snprintf(input_key, sizeof(input_key), "%s", methods[row].action + 6);
                                    input_buffer[0] = '\0';
                                    input_active = 1;
                                    append_history("INPUT_ACTIVATE key=%s", input_key);
                                    if (!input_popup_win) {
                                        input_popup_win = open_context_menu(dpy, popup_gc, (int[]){win_x}, (int[]){win_y + WIN_PX + 4}, 1, NULL) /* writeback discarded */;
                                    }
                                } else {
                                    dispatch_action(methods[row].action, package_dir, g_house_root, &running);
                                }
                                need_redraw = 1;
                                skip_navkey_enter_dispatch: ;
                            }
                        }
                    } else if (strncmp(line, "SHOW_PAGE:", 10) == 0) {
                        /* REAL, 2026-08-05: real Show Choices, injected
                         * into THIS already-running window by a SEPARATE
                         * process (a real event.pal's own exec'd op) -
                         * "<objects_file>|<result_file>". Reuses the
                         * exact same popup_win/methods[]/n_methods
                         * machinery the entity's own normal context menu
                         * already uses - choice_mode gates every row-
                         * activation site below to write the picked
                         * row's action to choice_result_path instead of
                         * running the normal dispatch. */
                        char *sep = strchr(line + 10, '|');
                        if (sep) {
                            *sep = '\0';
                            char *objpath = line + 10;
                            char *respath = sep + 1;
                            respath[strcspn(respath, "\r\n")] = '\0';
                            n_methods = load_flat_objects(objpath, methods, MAX_METHODS);
                            if (n_methods > 0) {
                                snprintf(choice_result_path, sizeof(choice_result_path), "%s", respath);
                                choice_mode = 1;
                                if (popup_win) close_context_menu(dpy, popup_win);
                                popup_x = win_x;
                                popup_y = win_y + WIN_PX + 4;
                                popup_win = open_context_menu(dpy, popup_gc, &popup_x, &popup_y, n_methods, methods);
                                /* REAL FIX 2026-08-06, direct report ("they
                                 * hav nav in main but not book choices"):
                                 * SHOW_PAGE popups never claimed real
                                 * shared live nav numbers (nav_base was
                                 * hardcoded 0, which draw_context_menu
                                 * treats as "suppress the index entirely"
                                 * - meant to avoid showing an unclaimed
                                 * number, but it just reads as a missing
                                 * feature next to every other menu, which
                                 * DOES claim from nav_claim_rows()). Real
                                 * fix: claim rows here too, same as every
                                 * other popup_win open site in this file. */
                                popup_nav_base = nav_claim_rows(g_house_root, getpid(), package_dir, methods, n_methods);
                                popup_focus_row = 0; popup_digit_accum = 0;
                                append_history("SHOW_PAGE objects=%s result=%s", objpath, choice_result_path);
                            }
                        }
                    } else if (strncmp(line, "SHOW_TEXT_FILE:", 15) == 0) {
                        /* REAL, 2026-08-05: real Show Text - displays a
                         * real text file's own content (already real-
                         * line-wrapped by whichever op generated it) in
                         * a small popup, dismissed by click/Enter/Escape.
                         * Real, separate window from the choice/normal
                         * popups (text_popup_win), so a choice result and
                         * its own follow-up text display can coexist
                         * without fighting over popup_win/methods[]. */
                        char textpath[TP_PATH_BUF];
                        snprintf(textpath, sizeof(textpath), "%s", line + 15);
                        textpath[strcspn(textpath, "\r\n")] = '\0';
                        FILE *tf2 = fopen(textpath, "r");
                        if (tf2) {
                            char tline[256];
                            int n_lines = 0;
                            int max_px = 0;
                            char text_lines[64][256];
                            while (n_lines < 64 && fgets(tline, sizeof(tline), tf2)) {
                                tline[strcspn(tline, "\r\n")] = '\0';
                                snprintf(text_lines[n_lines], sizeof(text_lines[0]), "%s", tline);
                                int lw = popup_text_px(dpy, tline);
                                if (lw > max_px) max_px = lw;
                                n_lines++;
                            }
                            fclose(tf2);
                            if (text_popup_win) close_context_menu(dpy, text_popup_win);
                            /* REAL FIX 2026-08-07: width used to be
                             * max_w*7 - a hardcoded 7px/char guess -
                             * but the real popup face is the 18px fixed
                             * fontset (~9px/glyph; CJK/emoji wider), so
                             * long bible_text verses were silently
                             * clipped at the right edge. Sized now from
                             * the REAL Xutf8TextExtents of the widest
                             * line (popup_text_px), + padding. */
                            int pop_w = (max_px * 115) / 100 + 24;
                            if (pop_w < POPUP_W) pop_w = POPUP_W;
                            if (pop_w > 900) pop_w = 900;
                            int pop_h = (n_lines + 1) * POPUP_ROW_H;
                            int tpx = win_x, tpy = win_y + WIN_PX + 4;
                            clamp_popup_to_screen(dpy, &tpx, &tpy, pop_w, pop_h);
                            XSetWindowAttributes swa2;
                            swa2.override_redirect = True;
                            swa2.background_pixel = WhitePixel(dpy, DefaultScreen(dpy));
                            swa2.event_mask = ExposureMask | ButtonPressMask | KeyPressMask;
                            text_popup_win = XCreateWindow(dpy, RootWindow(dpy, DefaultScreen(dpy)),
                                                            tpx, tpy, pop_w, pop_h, 1,
                                                            CopyFromParent, InputOutput, CopyFromParent,
                                                            CWOverrideRedirect | CWBackPixel | CWEventMask, &swa2);
                            {
                                XClassHint *ch2 = XAllocClassHint();
                                if (ch2) {
                                    ch2->res_name = (char *)"MuchiverseLivedesk";
                                    ch2->res_class = (char *)"MuchiverseLivedesk";
                                    XSetClassHint(dpy, text_popup_win, ch2);
                                    XFree(ch2);
                                }
                            }
                            XMapRaised(dpy, text_popup_win);
                            /* REAL FIX 2026-08-24, direct user report ("i
                             * want text to stay on screen till clicked...
                             * it closes very aggressively, when i press a
                             * key when its open"): this popup used to copy
                             * the context-menu's input policy wholesale -
                             * XGrabPointer/XGrabKeyboard whenever the pal's
                             * own STATE rows say grab_pointer/grab_keyboard
                             * (cursword's do) plus popup_soft_focus().
                             * Consequences: every keystroke ANYWHERE landed
                             * in this process and the old dismiss-on-any-
                             * KeyPress branch ate it (user's screenshot
                             * shortcuts died while a verse was up), every
                             * click anywhere was both swallowed by the grab
                             * AND dismissed the popup, and focused apps lost
                             * their keys while it stayed open. A Show Text
                             * box is not a modal menu: NO grabs, NO input
                             * focus. It now receives exactly what falls on
                             * it - a click directly on the box (its own
                             * ButtonPressMask; override_redirect keeps it
                             * topmost under the cursor so the click lands
                             * here without any grab) - everything else
                             * passes through to whatever the user actually
                             * aimed at. Dismissal itself is tightened in
                             * the event loop: ButtonPress ON THIS WINDOW
                             * only, keys never dismiss. */
                            /* REAL FIX 2026-08-06, direct-caught bug (a
                             * NAV_KEY-opened SHOW_TEXT_FILE popup
                             * self-dismissed a few seconds after opening,
                             * with no real input): text_popup_win never
                             * got the SAME stale-queued-event drain
                             * open_context_menu() already has (see that
                             * function's own header comment - X11 window
                             * ID reuse can redeliver an old
                             * Button/KeyPress meant for a JUST-destroyed
                             * window to this brand new one). This is a
                             * separate window creation path (not routed
                             * through open_context_menu()), so it never
                             * got the fix. Same real fix here. */
                            XSync(dpy, False);
                            {
                                XEvent stale_ev2;
                                while (XCheckWindowEvent(dpy, text_popup_win, ButtonPressMask | KeyPressMask, &stale_ev2)) {
                                    /* discard - see comment above */
                                }
                            }
                            for (int li = 0; li < n_lines; li++) {
                                strncpy(g_text_popup_lines[li], text_lines[li], sizeof(g_text_popup_lines[0]) - 1);
                            }
                            g_text_popup_n_lines = n_lines;
                            append_history("SHOW_TEXT_FILE %s (%d lines)", textpath, n_lines);
                        }
                    } else if (strncmp(line, "OPEN_PAGE:", 10) == 0) {
                        /* REAL 2026-08-06: open a named objects.pdl page
                         * after reload (e.g. RPG Menu rewritten by
                         * open_rp_menu.sh with live Level/Gold/HP). */
                        char page_name[64];
                        snprintf(page_name, sizeof(page_name), "%s", line + 10);
                        page_name[strcspn(page_name, "\r\n")] = '\0';
                        if (popup_win) {
                            close_context_menu(dpy, popup_win);
                            popup_win = 0;
                            nav_release_pid(g_house_root, getpid());
                        }
                        n_obj_pages = load_objects(package_dir, obj_pages, MAX_PAGES);
                        using_objects = (n_obj_pages > 0);
                        cur_page = 0;
                        page_stack_n = 0;
                        if (using_objects) {
                            int found = -1;
                            for (int pi = 0; pi < n_obj_pages; pi++) {
                                if (strcmp(obj_pages[pi].name, page_name) == 0) {
                                    found = pi;
                                    break;
                                }
                            }
                            if (found >= 0) {
                                cur_page = found;
                                n_methods = obj_pages[cur_page].n_items;
                                for (int k = 0; k < n_methods; k++) methods[k] = obj_pages[cur_page].items[k];
                            } else {
                                cur_page = 0;
                                n_methods = obj_pages[0].n_items;
                                for (int k = 0; k < n_methods; k++) methods[k] = obj_pages[0].items[k];
                            }
                        } else {
                            n_methods = load_methods(package_dir, methods, MAX_METHODS);
                            if (n_methods == 0) {
                                snprintf(methods[0].label, sizeof(methods[0].label), "Close");
                                snprintf(methods[0].action, sizeof(methods[0].action), "CLOSE");
                                n_methods = 1;
                            }
                        }
                        popup_x = win_x;
                        popup_y = win_y + WIN_PX + 4;
                        popup_win = open_context_menu(dpy, popup_gc, &popup_x, &popup_y, n_methods, methods);
                        popup_nav_base = nav_claim_rows(g_house_root, getpid(), package_dir, methods, n_methods);
                        popup_focus_row = 0;
                        append_history("OPEN_PAGE:%s", page_name);
                    } else if (strcmp(line, "OPEN_CONTEXT") == 0) {
                        /* REAL 2026-08-06: taskbar nav Enter on a tab
                         * writes this to open the entity context menu
                         * (same reload+open path as right-click). */
                        if (popup_win) {
                            close_context_menu(dpy, popup_win);
                            popup_win = 0;
                            nav_release_pid(g_house_root, getpid());
                        }
                        n_obj_pages = load_objects(package_dir, obj_pages, MAX_PAGES);
                        using_objects = (n_obj_pages > 0);
                        if (using_objects) {
                            cur_page = 0;
                            page_stack_n = 0;
                            n_methods = obj_pages[cur_page].n_items;
                            for (int k = 0; k < n_methods; k++) methods[k] = obj_pages[cur_page].items[k];
                        } else {
                            n_methods = load_methods(package_dir, methods, MAX_METHODS);
                            if (n_methods == 0) {
                                snprintf(methods[0].label, sizeof(methods[0].label), "Close");
                                snprintf(methods[0].action, sizeof(methods[0].action), "CLOSE");
                                n_methods = 1;
                            }
                        }
                        popup_x = win_x;
                        popup_y = win_y + WIN_PX + 4;
                        popup_win = open_context_menu(dpy, popup_gc, &popup_x, &popup_y, n_methods, methods);
                        popup_nav_base = nav_claim_rows(g_house_root, getpid(), package_dir, methods, n_methods);
                        popup_focus_row = 0; popup_digit_accum = 0;
                        append_history("OPEN_CONTEXT");
                    } else if (strcmp(line, "CLOSE") == 0) {
                        running = 0;
                    }
                    need_redraw = 1;
                }
            }
        }

        while (XPending(dpy)) {
            XEvent xev;
            XNextEvent(dpy, &xev);
            if (popup_win && xev.type == Expose && xev.xany.window == popup_win) {
                draw_context_menu(dpy, popup_win, popup_gc, methods, n_methods, popup_nav_base, popup_focus_row);
            } else if (popup_win && xev.type == ButtonPress) {
                /* Pointer is grabbed to popup_win while it's open (see
                 * open_context_menu) - every button press anywhere
                 * arrives here regardless of which window it physically
                 * landed on, same real technique egg_window.c's own
                 * popup uses. */
                /* Row 0 is the non-clickable id header (see g_full_id
                 * comment) - a real click there must not dispatch
                 * methods[0]. */
                int raw_row = xev.xbutton.y / POPUP_ROW_H;
                int row = raw_row - 1;
                int inside = xev.xbutton.x >= 0 && xev.xbutton.x < g_popup_w &&
                             row >= 0 && row < n_methods;
                int header_click = xev.xbutton.x >= 0 && xev.xbutton.x < g_popup_w && raw_row == 0;
                /* REAL FIX 2026-08-05, direct report ("context nav
                 * arrows and index #'s should get autofocus while its
                 * open or esp if 'headerbar' is touch[ed]"): a click on
                 * the non-clickable id header used to unconditionally
                 * close the whole menu with zero effect, easy to mistake
                 * for "the window just closed"/lost focus. Now a no-op
                 * that keeps the menu open (real keyboard grab already
                 * held the whole time - see open_context_menu()'s own
                 * real grab-retry fix above) instead of silently
                 * dismissing. */
                if (header_click) {
                    /* OPTION C: header click re-asserts soft focus on this
                     * menu (was pure no-op when grab alone was assumed). */
                    popup_soft_focus(dpy, popup_win);
                    need_redraw = 1;
                    continue;
                }
                if (!inside) {
                    /* REAL FIX 2026-08-07, direct instruction: clicking
                     * the same anchor again / clicking elsewhere must NOT
                     * close the menu - it stays open until the user
                     * clicks a real row or Cancel. Configurable via the
                     * package's meta.pdl: STATE | menu_stay_open | 0
                     * restores the old dismiss-on-any-click behavior. */
                    if (g_menu_stay_open) {
                        popup_soft_focus(dpy, popup_win);
                        need_redraw = 1;
                        continue;
                    }
                    close_context_menu(dpy, popup_win);
                    popup_win = 0;
                    nav_release_pid(g_house_root, getpid());
                    need_redraw = 1;
                    continue;
                }
                /* REAL FIX 2026-08-30 - real house-wide click_two_step
                 * (see g_click_two_step's own declaration comment
                 * above): a click on an unfocused row only focuses it
                 * (same real semantics arrow-nav already uses via
                 * popup_focus_row) - a second click on the SAME,
                 * already-focused row is what actually activates it. */
                if (g_click_two_step && popup_focus_row != row) {
                    popup_focus_row = row;
                    draw_context_menu(dpy, popup_win, popup_gc, methods, n_methods, popup_nav_base, popup_focus_row);
                    popup_soft_focus(dpy, popup_win);
                    need_redraw = 1;
                    continue;
                }
                close_context_menu(dpy, popup_win);
                popup_win = 0;
                nav_release_pid(g_house_root, getpid());
                if (choice_mode) {
                    /* REAL, 2026-08-05: a real Show Choices pick - write
                     * the chosen row's real action (a branch id) to the
                     * result file the waiting op is polling, instead of
                     * running the normal CLOSE/void/GOTO/etc dispatch. */
                    FILE *rf3 = fopen(choice_result_path, "w");
                    if (rf3) { fprintf(rf3, "%s\n", methods[row].action); fclose(rf3); }
                    append_history("SHOW_PAGE_PICK action=%s -> %s", methods[row].action, choice_result_path);
                    choice_mode = 0;
                } else {
                    append_history("CLICK method=%s action=%s", methods[row].label, methods[row].action);
                    /* REAL FIX 2026-08-04, direct instruction: this house
                     * already has a real, canonical convention for
                     * method dispatch - #.haiku+/tpmos-re-dox/fo-menu-
                     * sys.md ("Fuzz-Op Dynamic Menu System"): a
                     * METHOD's VALUE is a real, directly-executable op
                     * path (e.g. "projects/fuzz-op/ops/+x/feed_op.+x
                     * xlector"), or the literal keyword "void" for an
                     * internal-only no-op - NOT an abstract action
                     * keyword string-matched in C (the previous CLOSE/
                     * CANCEL/PLAY/STOP scheme this replaces). "CLOSE"
                     * stays a single reserved, internal keyword (closing
                     * THIS renderer's own event loop is not something an
                     * external process can do on this process's behalf)
                     * - everything else, including "void", is real
                     * fo-menu-sys.md-style dispatch: void = do nothing,
                     * any other VALUE = a real command line, executed
                     * with this package's own dir as argv[1] (same
                     * "pass the piece_id" convention fo-menu-sys.md's own
                     * examples show). */
                    if (strcmp(methods[row].action, "CLOSE") == 0) {
                        running = 0;
                    } else if (strcmp(methods[row].action, "void") == 0) {
                        /* Intentional no-op - e.g. "Cancel" - discoverable
                         * way to dismiss the menu with zero effect. */
                    } else if (strcmp(methods[row].action, "OPEN_USER") == 0) {
                        /* Second reserved internal keyword, same class as
                         * CLOSE - see this file's own 2026-08-05
                         * comment on user_popup_win.
                         * REAL FIX 2026-08-05, direct instruction
                         * ("sub context menu is way offset, it could
                         * be right next to the other"): was positioned
                         * off the CLICK coordinate (wherever "User"
                         * happened to sit in the row list, often far
                         * down), not the main popup itself. Now
                         * adjacent to popup_win's own real position -
                         * right next to it, same top edge, regardless
                         * of which row was clicked. */
                        user_popup_x = popup_x + g_popup_w + 4;
                        user_popup_y = popup_y;
                        user_popup_win = open_context_menu(dpy, popup_gc, &user_popup_x, &user_popup_y, 4, user_methods);
                    } else if (using_objects && strncmp(methods[row].action, "GOTO:", 5) == 0) {
                        /* REAL, 2026-08-05: objects.pdl href navigation -
                         * see load_objects()'s own header comment.
                         * REAL FIX, same day, caught while wiring nav
                         * claims: this used to only update methods[]/
                         * n_methods without ever reopening popup_win -
                         * the page switch was invisible until the NEXT
                         * right-click. Now reopens at the same real
                         * position and re-claims nav numbers for the
                         * new page's own rows. */
                        const char *target = methods[row].action + 5;
                        for (int pi = 0; pi < n_obj_pages; pi++) {
                            if (strcmp(obj_pages[pi].name, target) == 0) {
                                if (page_stack_n < MAX_PAGES) page_stack[page_stack_n++] = cur_page;
                                cur_page = pi;
                                n_methods = obj_pages[cur_page].n_items;
                                for (int k = 0; k < n_methods; k++) methods[k] = obj_pages[cur_page].items[k];
                                popup_win = open_context_menu(dpy, popup_gc, &popup_x, &popup_y, n_methods, methods);
                                popup_nav_base = nav_claim_rows(g_house_root, getpid(), package_dir, methods, n_methods);
                                        popup_focus_row = 0; popup_digit_accum = 0;
                                break;
                            }
                        }
                    } else if (using_objects && strcmp(methods[row].action, "BACK") == 0) {
                        if (page_stack_n > 0) {
                            cur_page = page_stack[--page_stack_n];
                            n_methods = obj_pages[cur_page].n_items;
                            for (int k = 0; k < n_methods; k++) methods[k] = obj_pages[cur_page].items[k];
                            popup_win = open_context_menu(dpy, popup_gc, &popup_x, &popup_y, n_methods, methods);
                            popup_nav_base = nav_claim_rows(g_house_root, getpid(), package_dir, methods, n_methods);
                                        popup_focus_row = 0; popup_digit_accum = 0;
                        }
                    } else if (using_objects && strncmp(methods[row].action, "STATE:", 6) == 0) {
                        /* REAL, 2026-08-05: objects.pdl real text-input
                         * row - same click-to-activate/Escape-to-commit
                         * shape this house's own cli_io field convention
                         * already uses. A small floating popup shows the
                         * live buffer (input_popup_win, drawn below);
                         * committing writes <package_dir>/<key>.txt. */
                        snprintf(input_key, sizeof(input_key), "%s", methods[row].action + 6);
                        input_buffer[0] = '\0';
                        input_active = 1;
                        append_history("INPUT_ACTIVATE key=%s", input_key);
                        if (!input_popup_win) {
                            input_popup_win = open_context_menu(dpy, popup_gc, (int[]){win_x}, (int[]){win_y + WIN_PX + 4}, 1, NULL) /* writeback discarded */;
                        }
                    } else {
                        /* REAL FIX 2026-08-10, direct report ("bookstack no
                         * longer shows verse, event-ez button no longer
                         * opens event editor - path issue"): pals migration
                         * moved entities out of the dev-tree's fixed nesting
                         * depth (*.monads/*.widget/entities/<name>, always
                         * 4 levels under house_root) into
                         * xyzfs/users/<uuid>/home/livedesk/pals/<name>
                         * (a different depth entirely). METHOD/OBJECT
                         * actions that derived house_root by climbing a
                         * FIXED number of ".." from package_dir (argv[1])
                         * broke silently for any entity now living at pals'
                         * depth - not a bug in this dispatch call itself,
                         * but this is the one place that CAN fix it for
                         * every action at once: house_root is already
                         * known here (g_house_root), so pass it as a real
                         * second argument instead of making every
                         * downstream script/METHOD line re-derive it via
                         * fragile directory-climbing. Backward compatible -
                         * existing METHOD/OBJECT lines that only read $1
                         * (package_dir) are unaffected; only ones updated
                         * to also read $2 (sh scripts) / $1-after-$0 (sh -c
                         * lines, since $0 is already package_dir there)
                         * gain house_root. */
                        dispatch_action(methods[row].action, package_dir, g_house_root, &running);
                    }
                }
                need_redraw = 1;
            } else if (popup_win && xev.type == KeyPress) {
                /* REAL, 2026-08-05, direct correction (see this file's
                 * own nav_claim_rows()/TILE_PICKER_DESIGN.md §13): the
                 * earlier local-digit-selects-a-row idea was wrong -
                 * "[N]" is now a real, GLOBAL, shared live address (see
                 * nav_claim_rows()), not a local 1-9 shortcut, so a bare
                 * digit keypress here no longer means anything - real
                 * jump-by-number now only happens remotely, via the
                 * taskbar's own terminal input writing an
                 * "ACTIVATE_NAV:<N>" command into interact_relay.txt
                 * (see the relay-poll block above). Escape still just
                 * closes the menu locally, same as "Cancel". */
                /* REAL FIX, same day, follow-up ("just wanna make sure
                 * the fundamentals are in place"): real Up/Down moves a
                 * real focus cursor (popup_focus_row, wraps both ways)
                 * matching real chtpm's own "[>]" convention - see
                 * draw_context_menu()'s own comment. Enter activates
                 * whichever row currently holds the cursor - same real
                 * dispatch logic the mouse-click/ACTIVATE_NAV paths use. */
                char kbuf[8];
                KeySym ks;
                int klen = XLookupString(&xev.xkey, kbuf, sizeof(kbuf) - 1, &ks, NULL);
                if (klen < 0) klen = 0;
                if (klen >= (int)sizeof(kbuf)) klen = (int)sizeof(kbuf) - 1;
                kbuf[klen] = '\0';
                if (ks == XK_Escape) {
                    close_context_menu(dpy, popup_win);
                    popup_win = 0;
                    nav_release_pid(g_house_root, getpid());
                    popup_digit_accum = 0;
                    need_redraw = 1;
                } else if (ks == XK_Up) {
                    if (n_methods > 0) {
                        popup_focus_row = (popup_focus_row - 1 + n_methods) % n_methods;
                        popup_digit_accum = 0;
                        draw_context_menu(dpy, popup_win, popup_gc, methods, n_methods, popup_nav_base, popup_focus_row);
                    }
                } else if (ks == XK_Down) {
                    if (n_methods > 0) {
                        popup_focus_row = (popup_focus_row + 1) % n_methods;
                        popup_digit_accum = 0;
                        draw_context_menu(dpy, popup_win, popup_gc, methods, n_methods, popup_nav_base, popup_focus_row);
                    }
                } else if (klen > 0 && kbuf[0] >= '0' && kbuf[0] <= '9') {
                    /* chtpm digit_accum: jump [>] to global nav index in this menu */
                    int d = kbuf[0] - '0';
                    int lo = popup_nav_base;
                    int hi = popup_nav_base + n_methods - 1; /* inclusive */
                    if (n_methods <= 0) { /* no-op */ }
                    else {
                        int new_val = popup_digit_accum * 10 + d;
                        int jumped = 0;
                        if (new_val >= lo && new_val <= hi) {
                            popup_digit_accum = new_val;
                            popup_focus_row = new_val - popup_nav_base;
                            jumped = 1;
                        } else if (d >= lo && d <= hi) {
                            /* out of range as append — restart with d if valid address */
                            popup_digit_accum = d;
                            popup_focus_row = d - popup_nav_base;
                            jumped = 1;
                        } else if (d >= 1 && d <= n_methods && lo <= d && d <= hi) {
                            popup_digit_accum = d;
                            popup_focus_row = d - popup_nav_base;
                            jumped = 1;
                        } else if (d >= 1 && d <= n_methods) {
                            /* local 1..N when global range doesn't include small digits */
                            /* only if nav numbers are lo..hi; if lo>9, d alone won't match */
                            popup_digit_accum = 0;
                        } else {
                            popup_digit_accum = 0;
                        }
                        if (jumped)
                            draw_context_menu(dpy, popup_win, popup_gc, methods, n_methods, popup_nav_base, popup_focus_row);
                    }
                } else if (ks == XK_Return || ks == XK_KP_Enter) {
                    popup_digit_accum = 0;
                    int row = popup_focus_row;
                    close_context_menu(dpy, popup_win);
                    popup_win = 0;
                    nav_release_pid(g_house_root, getpid());
                    if (choice_mode) {
                        FILE *rf4 = fopen(choice_result_path, "w");
                        if (rf4) { fprintf(rf4, "%s\n", methods[row].action); fclose(rf4); }
                        append_history("SHOW_PAGE_PICK(cursor) action=%s -> %s", methods[row].action, choice_result_path);
                        choice_mode = 0;
                        need_redraw = 1;
                        goto skip_enter_dispatch;
                    }
                    append_history("CLICK(cursor) method=%s action=%s", methods[row].label, methods[row].action);
                    if (strcmp(methods[row].action, "CLOSE") == 0) {
                        running = 0;
                    } else if (strcmp(methods[row].action, "void") == 0) {
                        /* no-op */
                    } else if (strcmp(methods[row].action, "OPEN_USER") == 0) {
                        user_popup_x = popup_x + g_popup_w + 4;
                        user_popup_y = popup_y;
                        user_popup_win = open_context_menu(dpy, popup_gc, &user_popup_x, &user_popup_y, 4, user_methods);
                    } else if (using_objects && strncmp(methods[row].action, "GOTO:", 5) == 0) {
                        const char *target = methods[row].action + 5;
                        for (int pi = 0; pi < n_obj_pages; pi++) {
                            if (strcmp(obj_pages[pi].name, target) == 0) {
                                if (page_stack_n < MAX_PAGES) page_stack[page_stack_n++] = cur_page;
                                cur_page = pi;
                                n_methods = obj_pages[cur_page].n_items;
                                for (int k = 0; k < n_methods; k++) methods[k] = obj_pages[cur_page].items[k];
                                popup_win = open_context_menu(dpy, popup_gc, &popup_x, &popup_y, n_methods, methods);
                                popup_nav_base = nav_claim_rows(g_house_root, getpid(), package_dir, methods, n_methods);
                                popup_focus_row = 0; popup_digit_accum = 0;
                                break;
                            }
                        }
                    } else if (using_objects && strcmp(methods[row].action, "BACK") == 0) {
                        if (page_stack_n > 0) {
                            cur_page = page_stack[--page_stack_n];
                            n_methods = obj_pages[cur_page].n_items;
                            for (int k = 0; k < n_methods; k++) methods[k] = obj_pages[cur_page].items[k];
                            popup_win = open_context_menu(dpy, popup_gc, &popup_x, &popup_y, n_methods, methods);
                            popup_nav_base = nav_claim_rows(g_house_root, getpid(), package_dir, methods, n_methods);
                            popup_focus_row = 0; popup_digit_accum = 0;
                        }
                    } else if (using_objects && strncmp(methods[row].action, "STATE:", 6) == 0) {
                        snprintf(input_key, sizeof(input_key), "%s", methods[row].action + 6);
                        input_buffer[0] = '\0';
                        input_active = 1;
                        append_history("INPUT_ACTIVATE key=%s", input_key);
                        if (!input_popup_win) {
                            input_popup_win = open_context_menu(dpy, popup_gc, (int[]){win_x}, (int[]){win_y + WIN_PX + 4}, 1, NULL) /* writeback discarded */;
                        }
                    } else {
                        dispatch_action(methods[row].action, package_dir, g_house_root, &running);
                    }
                    need_redraw = 1;
                    skip_enter_dispatch: ;
                }
            } else if (user_popup_win && xev.type == Expose && xev.xany.window == user_popup_win) {
                draw_context_menu(dpy, user_popup_win, popup_gc, user_methods, 4, -1, -1);
            } else if (user_popup_win && xev.type == ButtonPress) {
                int raw_row = xev.xbutton.y / POPUP_ROW_H;
                int row = raw_row - 1;
                int inside = xev.xbutton.x >= 0 && xev.xbutton.x < g_popup_w &&
                             row >= 0 && row < 4;
                if (!inside) {
                    /* REAL FIX 2026-08-07: same "menus stay open until
                     * Cancel" rule as the main menu - an outside click
                     * must not dismiss this submenu either. Toggle with
                     * STATE | menu_stay_open | 0 in meta.pdl. */
                    if (g_menu_stay_open) {
                        popup_soft_focus(dpy, user_popup_win);
                        need_redraw = 1;
                        continue;
                    }
                    close_context_menu(dpy, user_popup_win);
                    user_popup_win = 0;
                    need_redraw = 1;
                    continue;
                }
                close_context_menu(dpy, user_popup_win);
                user_popup_win = 0;
                if (strcmp(user_methods[row].action, "OPEN_RANGE_GRID") == 0) {
                    /* REAL FIX 2026-08-05: this used to draw an OPAQUE
                     * popup right here (real, but covered up the dog
                     * and nearby tiles - direct correction: "it should
                     * just be a transparent outline like a png").
                     * Consolidated into tp_range_grid.+x, a real
                     * standalone binary using the X11 Shape Extension
                     * (same technique this file's own sprite
                     * transparency already uses) so only the outline
                     * strokes are opaque - launched here instead of
                     * duplicating that logic. Centered on THIS
                     * window's own real position (win_x/win_y, the
                     * same coords XMoveWindow already uses), not the
                     * submenu popup's location - direct correction
                     * ("the range finder should be around the dog
                     * tho, its off center"). */
                    int grid_size = 5 * GRID_CELL_PX;
                    int gx = win_x + WIN_PX / 2 - grid_size / 2;
                    int gy = win_y + WIN_PX / 2 - grid_size / 2;
                    char self_path[TP_PATH_BUF];
                    if (self_exe_path(self_path, sizeof(self_path))) {
                        char *ops_dir = dirname(self_path);
                        char cmd[TP_PATH_BUF * 2];
                        snprintf(cmd, sizeof(cmd), "'%s/tp_range_grid.+x' %d %d >/dev/null 2>&1 &",
                                 ops_dir, gx, gy);
                        int rc = system(cmd);
                        (void)rc;
                    }
                }
                need_redraw = 1;
            } else if (input_popup_win && xev.type == Expose && xev.xany.window == input_popup_win) {
                char disp[300];
                snprintf(disp, sizeof(disp), "%s: %s_", input_key, input_buffer);
                XClearWindow(dpy, input_popup_win);
                XDrawRectangle(dpy, input_popup_win, popup_gc, 0, 0, g_popup_w - 1, POPUP_ROW_H * 2 - 1);
                popup_draw_text(dpy, input_popup_win, popup_gc, 8, POPUP_ROW_H, disp);
            } else if (input_active && xev.type == KeyPress) {
                /* REAL, 2026-08-05: real text-input for an objects.pdl
                 * "STATE:<key>" row - see the ButtonPress branch above
                 * for how input_active/input_key get set. Escape commits
                 * (matching this house's own cli_io field convention:
                 * activate, type, Escape deactivates/persists - NOT
                 * Enter), Backspace edits, any other printable key
                 * appends. */
                char kbuf[32];
                KeySym ks;
                int klen = XLookupString(&xev.xkey, kbuf, sizeof(kbuf) - 1, &ks, NULL);
                if (ks == XK_Escape) {
                    char statepath[TP_PATH_BUF];
                    snprintf(statepath, sizeof(statepath), "%s/%s.txt", package_dir, input_key);
                    FILE *sf = fopen(statepath, "w");
                    if (sf) { fprintf(sf, "%s\n", input_buffer); fclose(sf); }
                    append_history("INPUT_COMMIT key=%s value=%s", input_key, input_buffer);
                    input_active = 0;
                    if (input_popup_win) { close_context_menu(dpy, input_popup_win); input_popup_win = 0; }
                } else if (ks == XK_BackSpace) {
                    size_t l = strlen(input_buffer);
                    if (l > 0) input_buffer[l - 1] = '\0';
                } else if (klen > 0) {
                    size_t l = strlen(input_buffer);
                    if (l + (size_t)klen < sizeof(input_buffer)) {
                        memcpy(input_buffer + l, kbuf, (size_t)klen);
                        input_buffer[l + klen] = '\0';
                    }
                }
                if (input_popup_win) {
                    char disp[300];
                    snprintf(disp, sizeof(disp), "%s: %s_", input_key, input_buffer);
                    XClearWindow(dpy, input_popup_win);
                    XDrawRectangle(dpy, input_popup_win, popup_gc, 0, 0, g_popup_w - 1, POPUP_ROW_H * 2 - 1);
                    XDrawString(dpy, input_popup_win, popup_gc, 8, POPUP_ROW_H,
                                disp, (int)strlen(disp));
                }
            } else if (text_popup_win && xev.type == Expose && xev.xany.window == text_popup_win) {
                XClearWindow(dpy, text_popup_win);
                int pop_w2 = 0, pop_h2 = 0;
                { Window root_r; int x_r, y_r; unsigned int w_r, h_r, bw_r, depth_r;
                  XGetGeometry(dpy, text_popup_win, &root_r, &x_r, &y_r, &w_r, &h_r, &bw_r, &depth_r);
                  pop_w2 = (int)w_r; pop_h2 = (int)h_r; }
                XDrawRectangle(dpy, text_popup_win, popup_gc, 0, 0, pop_w2 - 1, pop_h2 - 1);
                for (int li = 0; li < g_text_popup_n_lines; li++) {
                    popup_draw_text(dpy, text_popup_win, popup_gc, 8, (li + 1) * POPUP_ROW_H - 6, g_text_popup_lines[li]);
                }
            } else if (text_popup_win && xev.type == ButtonPress &&
                       xev.xany.window == text_popup_win) {
                /* REAL FIX 2026-08-24, direct user report ("i dont want it
                 * to close unless i click it exactly (not even if i click
                 * something else)"): this used to be
                 * `(ButtonPress || KeyPress)` with NO window check - any
                 * key anywhere or any click anywhere dismissed the box
                 * (and, while its input grabs were still in play, ate the
                 * event out from under whichever app it belonged to).
                 * Real RPG Maker "Show Text" waits for a confirm press,
                 * but this house's real use is screenshot-and-inspect:
                 * the box is now a passive overlay. ONLY a ButtonPress
                 * delivered ON the popup window itself closes it; keys of
                 * any kind never do; clicks on other windows are none of
                 * this process's business (no grab is taken at creation -
                 * see the creation-site fix above). */
                close_context_menu(dpy, text_popup_win);
                text_popup_win = 0;
                append_history("SHOW_TEXT_DISMISSED");
            } else if (xev.type == Expose) {
                need_redraw = 1;
            } else if (xev.type == ButtonPress && xev.xbutton.button == 1 && g_is_cursword && g_cursword_awaiting_place) {
                /* REAL, NEW 2026-08-30 - the real placement click,
                 * step 2 of the design doc (§10 click-to-place). The
                 * real XGrabPointer taken on arm (below) means ANY
                 * real click anywhere on the screen lands HERE
                 * regardless of which window it visually landed over -
                 * x_root/y_root are real screen coordinates, snapped to
                 * the same real desktop grid every entity already uses
                 * (matches the existing drag's own real grid-snap
                 * technique, just driven by the placement click's own
                 * position instead of the window's dragged position). */
                int gx = (xev.xbutton.x_root + GRID_CELL_PX / 2) / GRID_CELL_PX;
                int gy = (xev.xbutton.y_root + GRID_CELL_PX / 2) / GRID_CELL_PX;
                if (gx < 0) gx = 0;
                if (gx > max_col) gx = max_col;
                if (gy < 0) gy = 0;
                if (gy > max_row) gy = max_row;
                win_x = gx * GRID_CELL_PX;
                win_y = gy * GRID_CELL_PX;
                XMoveWindow(dpy, win, win_x, win_y);
                write_pos(package_dir, win_x, win_y);
                XUngrabPointer(dpy, CurrentTime);
                XUngrabKeyboard(dpy, CurrentTime);
                g_cursword_awaiting_place = 0;
                g_cursword_armed = 0;
                cursword_write_armed(g_house_root, 0);
                append_history("CURSWORD_PLACED");
                cursword_update_shape(dpy, win);
                need_redraw = 1;
            } else if (xev.type == ButtonPress && xev.xbutton.button == 1) {
                dragging = 1;
                drag_start_x = xev.xbutton.x_root;
                drag_start_y = xev.xbutton.y_root;
                press_root_x = xev.xbutton.x_root;
                press_root_y = xev.xbutton.y_root;
                gettimeofday(&press_tv, NULL);
            } else if (xev.type == ButtonRelease && xev.xbutton.button == 1) {
                dragging = 0;
                /* Real click-vs-drag distinction, cursword only - see
                 * g_is_cursword's own declaration comment
                 * (CURSWORD-DESKTOP-3D-AND-PIECECRAFT-INSCENE-DESKS-
                 * DESIGN.md §9/§10). A real click (small movement, real
                 * quick release) arms/disarms cursword instead of
                 * running the existing grid-snap-drag logic below -
                 * every OTHER entity, and any real drag on cursword
                 * itself, keeps the exact existing behavior unchanged. */
                int was_real_click = 0;
                if (g_is_cursword) {
                    int dx2 = xev.xbutton.x_root - press_root_x;
                    int dy2 = xev.xbutton.y_root - press_root_y;
                    int dist2 = dx2 * dx2 + dy2 * dy2;
                    struct timeval rel_tv; gettimeofday(&rel_tv, NULL);
                    long elapsed_ms = (rel_tv.tv_sec - press_tv.tv_sec) * 1000L +
                                       (rel_tv.tv_usec - press_tv.tv_usec) / 1000L;
                    if (dist2 <= CURSWORD_CLICK_MAX_PX * CURSWORD_CLICK_MAX_PX && elapsed_ms <= CURSWORD_CLICK_MAX_MS)
                        was_real_click = 1;
                }
                if (was_real_click) {
                    g_cursword_armed = !g_cursword_armed;
                    cursword_write_armed(g_house_root, g_cursword_armed);
                    append_history(g_cursword_armed ? "CURSWORD_ARMED" : "CURSWORD_DISARMED");
                    if (g_cursword_armed) g_cursword_log_n = 0; /* real, new 2026-08-30 - fresh key-log each new arm, see cursword_log_key()'s own header comment */
                    cursword_update_shape(dpy, win);
                    if (g_cursword_armed) {
                        /* REAL FIX 2026-08-30, direct report ("its not
                         * taking arrow keys yet? it should be very
                         * stingy with focus till esc is pressed"):
                         * arrow-key nudge was written assuming this
                         * window already held real X11 input focus,
                         * which nothing here ever guaranteed - normal
                         * click-to-focus WM behavior is not reliable
                         * enough for "stingy" key capture. A real
                         * display-wide XGrabKeyboard on arm (same
                         * retry-loop technique as the pre-existing
                         * XGrabPointer just below, and this file's own
                         * popup code ~line 2002) makes EVERY key press
                         * anywhere land on this window's own event
                         * queue regardless of focus, until the real
                         * Escape/disarm/placed paths ungrab it. */
                        /* REAL FIX 2026-08-30, direct live report ("its
                         * not holding focus. we need it to have special
                         * mode of focus when it has halo. where it has
                         * priority over all windows for key input") -
                         * this window is override_redirect (never WM-
                         * managed), the exact same real class of window
                         * open-hai's own code documents as unreliable
                         * for keyboard focus (HOUSE_STDS #21 - see
                         * khtpm_open_hai_render.c's own "Managed window +
                         * _MOTIF_WM_HINTS... NOT override_redirect" real
                         * fix). Converting cursword's whole window model
                         * to WM-managed would risk every other entity's
                         * own real positioning/desktop-icon behavior
                         * (same shared window-creation code, not cursword-
                         * specific) - real, scoped fix instead: raise the
                         * window to the very top AND explicitly set real
                         * input focus onto it, on top of the existing
                         * keyboard grab, every time it arms. A real
                         * grab alone SHOULD already route every key here
                         * per the X11 spec regardless of focus - adding
                         * both is the same "belt and suspenders" real
                         * mitigation for override-redirect focus
                         * flakiness under a real compositor (mutter/
                         * XWayland). */
                        int grab_rc = 0;
                        for (int attempt = 0; attempt < 5; attempt++) {
                            grab_rc = XGrabKeyboard(dpy, win, False, GrabModeAsync, GrabModeAsync, CurrentTime);
                            if (grab_rc == GrabSuccess) break;
                            XSync(dpy, False);
                            usleep(5000);
                        }
                        XRaiseWindow(dpy, win);
                        XSetInputFocus(dpy, win, RevertToParent, CurrentTime);
                        /* Real, visible diagnostic (see cursword_log_key()'s
                         * own header comment) - if the grab itself never
                         * actually succeeded, the debug strip says so
                         * immediately instead of silently doing nothing. */
                        cursword_log_key(grab_rc == GrabSuccess ? "GRAB-OK" : "GRABFAIL");
                    } else {
                        /* Disarmed via a real click (only reachable in
                         * arrow_only move_mode - click_place mode's own
                         * pointer grab means a self-click can never
                         * reach here, see the NOTE below). The keyboard
                         * grab taken on arm above must be released
                         * here too, same as the real Escape path. */
                        XUngrabKeyboard(dpy, CurrentTime);
                    }
                    if (g_cursword_armed && g_cursword_click_place) {
                        /* REAL, NEW 2026-08-30 - real click-to-place
                         * arm: grab the pointer display-wide (same real
                         * technique/retry-loop this file's own popup
                         * code already uses, ~line 1957) so the VERY
                         * NEXT real click anywhere is delivered to this
                         * window as the placement click, not whatever
                         * window it visually landed over. */
                        for (int attempt = 0; attempt < 5; attempt++) {
                            int rc = XGrabPointer(dpy, win, False, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
                            if (rc == GrabSuccess) { g_cursword_awaiting_place = 1; break; }
                            XSync(dpy, False);
                            usleep(5000);
                        }
                    }
                    /* NOTE: re-clicking cursword's own window while
                     * g_cursword_awaiting_place is true can NEVER reach
                     * this toggle branch at all - the real pointer grab
                     * above means EVERY real click, including one that
                     * visually lands back on cursword itself, is routed
                     * to the dedicated placement-click ButtonPress
                     * branch first (it runs unconditionally on ANY
                     * button-1 press while awaiting placement). Escape
                     * is therefore the only real way to cancel a
                     * pending placement - see that branch's own real
                     * XUngrabPointer call. */
                    need_redraw = 1;
                } else {
                /* REAL FIX 2026-08-04, direct instruction ("egg-pets
                 * snap to grid... do u see that logic"): same
                 * round-to-nearest-cell technique egg_window.c's own
                 * ButtonRelease handler uses, same GRID_CELL_PX (80),
                 * so a placed tile lands on the identical desktop grid
                 * egg-pals already use, not a separate/incompatible one. */
                int grid_x = (win_x + GRID_CELL_PX / 2) / GRID_CELL_PX;
                int grid_y = (win_y + GRID_CELL_PX / 2) / GRID_CELL_PX;
                if (grid_x < 0) grid_x = 0;
                if (grid_x > max_col) grid_x = max_col;
                if (grid_y < 0) grid_y = 0;
                if (grid_y > max_row) grid_y = max_row;
                win_x = grid_x * GRID_CELL_PX;
                win_y = grid_y * GRID_CELL_PX;
                XMoveWindow(dpy, win, win_x, win_y);
                write_pos(package_dir, win_x, win_y);
                need_redraw = 1;
                }
            } else if (xev.type == ButtonPress && xev.xbutton.button == 3) {
                /* REAL, NEW 2026-08-04: right-click now opens the real
                 * data-driven context menu (see load_methods() above)
                 * instead of closing immediately - matches the ask
                 * ("add the context menus that already exist from
                 * egg-pal to these by default"). */
                if (!popup_win) {
                    /* REAL FIX 2026-08-06: always reload base menu from DISK
                     * (objects.pdl if present, else meta.pdl). In-memory
                     * obj_pages went stale after SHOW_PAGE AND after user
                     * edited objects.pdl/meta.pdl while the process lived
                     * (Events (ez) missing until restart). */
                    read_menu_config(package_dir); /* re-read menu guard toggles too */
                    n_obj_pages = load_objects(package_dir, obj_pages, MAX_PAGES);
                    using_objects = (n_obj_pages > 0);
                    if (using_objects) {
                        cur_page = 0;
                        page_stack_n = 0;
                        n_methods = obj_pages[cur_page].n_items;
                        for (int k = 0; k < n_methods; k++) methods[k] = obj_pages[cur_page].items[k];
                    } else {
                        n_methods = load_methods(package_dir, methods, MAX_METHODS);
                        if (n_methods == 0) {
                            snprintf(methods[0].label, sizeof(methods[0].label), "Close");
                            snprintf(methods[0].action, sizeof(methods[0].action), "CLOSE");
                            n_methods = 1;
                        }
                    }
                    popup_x = xev.xbutton.x_root;
                    popup_y = xev.xbutton.y_root;
                    popup_win = open_context_menu(dpy, popup_gc, &popup_x, &popup_y, n_methods, methods);
                    popup_nav_base = nav_claim_rows(g_house_root, getpid(), package_dir, methods, n_methods);
                    popup_focus_row = 0; popup_digit_accum = 0;
                    if (popup_win)
                        draw_context_menu(dpy, popup_win, popup_gc, methods, n_methods, popup_nav_base, popup_focus_row);
                }
            } else if (xev.type == MotionNotify && dragging) {
                int dx = xev.xmotion.x_root - drag_start_x;
                int dy = xev.xmotion.y_root - drag_start_y;
                win_x += dx; win_y += dy;
                XMoveWindow(dpy, win, win_x, win_y);
                drag_start_x = xev.xmotion.x_root;
                drag_start_y = xev.xmotion.y_root;
                need_redraw = 1;
            } else if (xev.type == FocusOut && g_is_cursword && g_cursword_armed &&
                       xev.xfocus.mode == NotifyNormal) {
                /* REAL, NEW 2026-08-30, direct live report ("if
                 * cursword loses focus (like user clicks different
                 * window) is there a way to make sure the halo goes
                 * away, else it causes confusion when going back
                 * (unselects it)") - same real disarm sequence as the
                 * Escape branch just below (kept inline, not factored
                 * into a shared helper, since g_house_root etc. are
                 * main()'s own real locals, not accessible outside
                 * this function). A real XGrabKeyboard doesn't itself
                 * prevent the WM/compositor from moving real X input
                 * focus to a DIFFERENT window the user clicks - this
                 * is the real, missing "clicking away should un-arm
                 * it" signal.
                 * REAL BUG FOUND + FIXED LIVE (2026-08-30, direct live
                 * report of cursword self-closing on the very next
                 * keypress after arming) - X11 ALSO fires a real
                 * FocusOut on THIS SAME window the instant its own
                 * XGrabKeyboard call (just above, on arm) establishes
                 * the grab - a real, spurious, grab-related event, NOT
                 * a genuine "focus moved to another window." Its real
                 * xfocus.mode is NotifyGrab, not NotifyNormal - the
                 * mode check above is the real, correct way to tell
                 * them apart (confirmed via direct Xlib docs: a
                 * genuine focus change from a real user click is
                 * always NotifyNormal). Without this filter, arming
                 * cursword instantly self-disarmed itself one event
                 * later, so the VERY NEXT key hit this file's own
                 * "no menu open -> any key closes the tile" default
                 * fallback (since it looked unarmed again) - not a
                 * crash, a real, deliberate self-close, just
                 * triggered by a bug elsewhere. */
                if (g_cursword_awaiting_place) {
                    XUngrabPointer(dpy, CurrentTime);
                    g_cursword_awaiting_place = 0;
                }
                XUngrabKeyboard(dpy, CurrentTime);
                g_cursword_armed = 0;
                cursword_write_armed(g_house_root, 0);
                append_history("CURSWORD_DISARMED_FOCUS_LOST");
                cursword_update_shape(dpy, win);
                need_redraw = 1;
            } else if (xev.type == KeyPress) {
                if (g_is_cursword && g_cursword_armed) {
                    /* Real, house-standard dual-mode boundary - while
                     * armed, real key capture begins and continues
                     * until real Escape (CURSWORD-DESKTOP-3D-AND-
                     * PIECECRAFT-INSCENE-DESKS-DESIGN.md §3a, same
                     * real principle as board-viewer's own
                     * active_index==-1 model, !.HOUSE_STDS.md §A.9).
                     * Without this branch, this file's own real
                     * default ("no popup open -> any key closes the
                     * tile," right below) would close cursword outright
                     * the moment a key was pressed while armed -
                     * confirmed by direct read, not assumed. */
                    KeySym ks2 = XLookupKeysym(&xev.xkey, 0);
                    /* REAL FIX 2026-08-31 - the camera-mode keys moved
                     * from 1-4 to 5-8 (see cursword_handle_camera_key()'s
                     * own header comment: keys 1-4 are now reserved for
                     * a future "one map" perspective mode) - the old
                     * special-cased "1"/"2"/"3"/"4" label branch here is
                     * dropped since it's no longer needed: XKeysymToString()
                     * already returns the correct literal digit string
                     * ("5".."8") for these keysyms same as any other key. */
                    cursword_log_key(
                        ks2 == XK_Escape ? "ESC" :
                        ks2 == XK_Left ? "LEFT" : ks2 == XK_Right ? "RIGHT" :
                        ks2 == XK_Up ? "UP" : ks2 == XK_Down ? "DOWN" :
                        XKeysymToString(ks2) ? XKeysymToString(ks2) : "?");
                    need_redraw = 1; /* real, unconditional - the log line above must always repaint, even for a key none of the branches below handle */
                    if (ks2 == XK_Escape) {
                        if (g_cursword_awaiting_place) {
                            /* Real cancel of a pending click-to-place -
                             * the pointer grab from arm-time must be
                             * dropped here, this is the only real
                             * cancel path (see the placement
                             * ButtonPress branch's own comment for why
                             * re-clicking cursword itself can't reach
                             * this instead). */
                            XUngrabPointer(dpy, CurrentTime);
                            g_cursword_awaiting_place = 0;
                        }
                        XUngrabKeyboard(dpy, CurrentTime);
                        g_cursword_armed = 0;
                        cursword_write_armed(g_house_root, 0);
                        append_history("CURSWORD_DISARMED");
                        cursword_update_shape(dpy, win);
                        need_redraw = 1;
                    } else if (ks2 == XK_Left || ks2 == XK_Right || ks2 == XK_Up || ks2 == XK_Down) {
                        /* REAL, NEW 2026-08-30, step 2 - real arrow-key
                         * nudge (§3a: "Arrow keys move cursword itself
                         * ... likely the same 80px GRID_CELL_PX every
                         * entity already snaps to"). Real, always-on
                         * baseline movement in EITHER move_mode - same
                         * real grid-snap + write_pos + XMoveWindow
                         * technique the existing drag-release code uses,
                         * just stepping by one whole cell per press
                         * instead of snapping a dragged pixel position. */
                        int gx = win_x / GRID_CELL_PX, gy = win_y / GRID_CELL_PX;
                        if (ks2 == XK_Left) gx--;
                        else if (ks2 == XK_Right) gx++;
                        else if (ks2 == XK_Up) gy--;
                        else gy++;
                        if (gx < 0) gx = 0;
                        if (gx > max_col) gx = max_col;
                        if (gy < 0) gy = 0;
                        if (gy > max_row) gy = max_row;
                        win_x = gx * GRID_CELL_PX;
                        win_y = gy * GRID_CELL_PX;
                        /* REAL FIX 2026-08-31, direct live report ("now
                         * when moving cursword, in 3d mode it flickers
                         * back to old position after every arrow key
                         * move") - this used to move to the raw,
                         * unpanned win_x/win_y directly, then the
                         * per-frame camera-pan correction (further down
                         * this same loop, real header comment: "win_x/
                         * win_y themselves... stay completely
                         * untouched... only the actual X11 window
                         * position gets +cam_pan_x/+cam_pan_y") saw a
                         * real mismatch against its own cached last-
                         * applied position and immediately re-moved it
                         * to the correct panned spot - two real, back-
                         * to-back XMoveWindow calls to two different
                         * targets in the same tick, a real visible
                         * flicker. Real fix: apply the same real pan
                         * offset here too when in 3D mode, so this
                         * call already lands on the correct spot and
                         * the later correction becomes a genuine no-op
                         * (matches what it would have computed anyway,
                         * not a second real move). */
                        {
                            int in_3d = (g_camera_mode == 3 || g_camera_mode == 4);
                            int disp_x = in_3d ? win_x + g_cam_pan_x : win_x;
                            int disp_y = in_3d ? win_y + g_cam_pan_y : win_y;
                            XMoveWindow(dpy, win, disp_x, disp_y);
                        }
                        write_pos(package_dir, win_x, win_y);
                        need_redraw = 1;
                    } else if (cursword_handle_camera_key(g_house_root, package_dir, ks2)) {
                        /* Real, consolidated dispatch - see
                         * cursword_handle_camera_key()'s own header
                         * comment (1-4 mode, f reset, wasd/rt pan-
                         * tilt, and any future camera key all live
                         * there now, one real place). */
                        need_redraw = 1;
                    }
                } else if (popup_win || user_popup_win || input_popup_win || text_popup_win || input_active) {
                    /* REAL FIX 2026-08-07, direct instruction ("print
                     * screen closes the context"): with a menu/popup open,
                     * an unhandled key (Print Screen, media keys, etc.) is
                     * NOT a close signal. The old fallback here set
                     * running = 0 on ANY key, so pressing Print Screen
                     * while a bible-verse / context menu was up silently
                     * killed the whole tile window. Menus stay open until
                     * the user clicks Cancel or presses Escape/Enter. */
                } else if (g_is_cursword) {
                    /* REAL FIX 2026-08-31, direct live report (cursword
                     * silently vanishing - a real key hitting cursword
                     * right after it disarmed, e.g. from a rapid test
                     * sequence, fell straight into the generic
                     * "no popup open -> any key closes the tile" fallback
                     * below and killed the whole process - no crash, no
                     * signal, a real, deliberate, if surprising, exit).
                     * Direct instruction on the fix: "any key is meant
                     * to turn its halo focus off, not close it" - cursword
                     * is the real always-open assistant entity (see the
                     * "always-open first entity" work), not a closable
                     * popup tile, so it's now exempt from that generic
                     * close-on-any-key default. It's already unarmed by
                     * the time this branch can even run (the
                     * g_cursword_armed branch above handles every key
                     * while armed), so there's nothing further to do here
                     * - the halo/focus state already reflects "off." */
                } else {
#ifndef _WIN32
                    running = 0; /* no menu open: any key closes the tile, as before */
#endif
                }
            }
        }
        if (!running) break;
#ifndef _WIN32
        if (!package_still_exists(package_dir)) break; /* package gone -> stop pointing at nothing */
#endif

        /* REAL FIX 2026-08-04, direct instruction ("dont render more
         * than 30fps etc, cpu is getting hot"): explicit, measured frame
         * pacing (not just relying on select()'s 300ms timeout, which
         * doesn't bound how often this loop redraws once real X events
         * start arriving, e.g. during a drag). Skips the redraw entirely
         * if nothing changed AND less than one frame interval has
         * passed since the last real swap - a static, undragged tile
         * does zero GL work between polls. */
        struct timeval now;
        gettimeofday(&now, NULL);
        /* Win long is 32-bit: (tv_sec * 1000000) overflows, goes negative,
         * and this skip never presents a frame. Linux long is 64-bit. */
        long long elapsed_usec =
            ((long long)now.tv_sec - last_frame.tv_sec) * 1000000LL +
            ((long long)now.tv_usec - last_frame.tv_usec);
        if (!need_redraw) {
            continue;
        }
        if (last_frame.tv_sec != 0 && elapsed_usec < MIN_FRAME_USEC) {
            long rem = (long)(MIN_FRAME_USEC - elapsed_usec);
            if (rem > 0 && rem < 1000000L) usleep((useconds_t)rem);
            continue;
        }

        {
            int bg_r = (int)(r * 255.0f), bg_g = (int)(g * 255.0f), bg_b = (int)(b * 255.0f);
            /* Real, new 2026-08-30 - explicit full alpha, see
             * draw_sprite_rgb()'s own matching comment (harmless
             * no-op high byte for every non-cursword entity). */
            XSetForeground(dpy, g_buf_gc, 0xFF000000UL | ((unsigned long)bg_r << 16) | ((unsigned long)bg_g << 8) | (unsigned long)bg_b);
            /* Real, new 2026-08-30: cursword's own buffer reserves
             * CURSWORD_LOG_H extra rows (see the g_buf XCreatePixmap
             * comment above) - cleared here too every frame so stale
             * key-log text never lingers under a fresh background. */
            XFillRectangle(dpy, g_buf, g_buf_gc, 0, 0, (unsigned)(g_is_cursword ? CURSWORD_LOG_W : WIN_PX),
                            (unsigned)(WIN_PX + (g_is_cursword ? CURSWORD_LOG_H : 0)));
            /* REAL, NEW 2026-08-30, direct report ("im still having to
             * click right on the image") + direct instruction ("solid
             * disc but very low transparency") - draws the real, dim
             * backdrop disc that now permanently occupies cursword's
             * widened ShapeBounding (see cursword_update_shape()'s own
             * header comment for the full reasoning: this window has
             * no real per-pixel alpha, so a near-black fill is the
             * closest honest stand-in for "very low transparency").
             * Drawn BEFORE the sprite, always (not gated on armed), so
             * the sprite still renders crisp on top of it.
             * REAL FOLLOW-UP 2026-08-30 ("i wanted to change the
             * colors alpha... very transparent") - genuine per-pixel
             * alpha now, not a color trick: cursword's own window is a
             * real 32-bit ARGB visual (see the XMatchVisualInfo setup
             * near main()'s window creation) - the compositor blends
             * this disc against whatever's really behind it using the
             * alpha byte below, same gray (0x141414) the user already
             * confirmed was the right color. Trivially tunable -
             * raise/lower just the leading byte to taste.
             * REAL FOLLOW-UP 2026-08-30 ("set it to 1% alpha, even
             * lower?"): 0x20 (~12%) -> 0x03 (~1%, 3/255) - still a
             * real, nonzero alpha (the shape/click boundary is
             * unaffected either way, see the header comment above -
             * this is purely how visible it reads). */
            if (g_is_cursword) {
                /* Real, new 2026-08-30 ("make the grey translucent
                 * circle around cursword completely transparent 0%") -
                 * alpha 0x03 -> 0x00. Still a real, unioned
                 * ShapeBounding region (the wider click surface stays
                 * exactly as wide - alpha and clickability are fully
                 * independent, per this disc's own earlier header
                 * comment), just genuinely invisible now. */
                XSetForeground(dpy, g_buf_gc, 0x00141414UL);
                int dcx = WIN_PX / 2, dcy = WIN_PX / 2;
                int dradius = WIN_PX / 2 - 5;
                XFillArc(dpy, g_buf, g_buf_gc, dcx - dradius, dcy - dradius,
                          (unsigned)(dradius * 2), (unsigned)(dradius * 2), 0, 360 * 64);
            }
            /* Real, visible armed-state halo (§3a/§9 item 4: overlay/
             * ring, never replacing the sprite). STALE NOTE, corrected
             * 2026-08-30: originally drawn BEFORE the sprite here,
             * relying on draw_sprite_rgb()'s own per-pixel alpha to
             * "peek through" - moved AFTER the sprite instead (see the
             * real fix comment right below) once this sprite was
             * confirmed to have no real alpha transparency. Real color
             * changed from the design doc's original neon-blue spec to
             * a yellow glow, direct instruction. */
            /* Real, new 2026-08-30 - desktop-wide 3D switch (see
             * load_camera_mode()/draw_topdown_block_rgb()'s own header
             * comment): modes 3/4 render every sprite entity as a real
             * extruded block instead of the flat top-down blit. Cheap
             * enough to poll unconditionally every frame at this
             * file's own 30fps cap (MAX_FPS) - a tiny, single-line
             * file, no changed-marker optimization needed at this
             * scale. */
            load_camera_mode(g_house_root);
            load_camera_state(g_house_root);
            load_active_z(g_house_root);
            /* REAL, NEW 2026-08-31, direct instruction ("do we have z
             * layers yet?... it affects 2d also. in 2d u wont see the
             * entity") - real z-level VISIBILITY filter, applies
             * unconditionally in EVERY camera mode (not just 3D):
             * an entity whose own real g_entity_z doesn't match the
             * shared g_active_z (set only by cursword's own real c/v
             * keys, see cursword_handle_camera_key()'s own header
             * comment) is genuinely unmapped - not drawn, not
             * present, matching a real "which floor am I looking at"
             * convention, achievable within this file's own existing
             * one-window-per-entity architecture (no shared 3D scene
             * needed, direct instruction: "i hope we dont have to
             * switch to shared scene just yet"). */
            {
                static int z_was_mapped = 1; /* window starts real, mapped (XMapWindow already ran earlier in main()) */
                int z_should_show = (g_entity_z == g_active_z);
                if (z_should_show && !z_was_mapped) { XMapWindow(dpy, win); z_was_mapped = 1; }
                else if (!z_should_show && z_was_mapped) { XUnmapWindow(dpy, win); z_was_mapped = 0; }
                if (!z_should_show) goto skip_zfiltered_draw;
            }
            /* REAL, NEW 2026-08-31 - shared by both branches right
             * below (see update_entity_shape_from_3d()'s own header
             * comment for the full "red shadow" bug this is part of
             * fixing) - tracks whether THIS entity was in 3D mode last
             * frame, so returning to 2D restores its real shape
             * exactly once on the transition, not every 2D frame. */
            static int was_3d_last_frame = 0;
            if (g_has_sprite) {
                if (g_camera_mode == 3 || g_camera_mode == 4) {
                    /* REAL FIX 2026-08-30, direct live report ("its
                     * still showing 2d sprite behind the 3d. can u fix
                     * that?") - the flat sprite used to always draw
                     * first as a real "no gaps" base layer (matching
                     * draw_topdown_block_rgb()'s own older reasoning,
                     * from when the extrusion cue was a thin strip
                     * that genuinely needed something solid behind
                     * it). Both real raymarchers below now cover their
                     * own entire real footprint on a hit (a solid box,
                     * or the phymoji model's own real silhouette) - a
                     * missed ray is real, honest EMPTY space (the
                     * plain background already filled above), not the
                     * old flat sprite peeking through around/behind
                     * the 3D shape. Real per-voxel phymoji render
                     * (genuine volumetric silhouette) whenever this
                     * entity has a real generated voxel asset; the
                     * single-box-with-texture raymarch stays the real,
                     * honest fallback for any entity that doesn't. */
                    if (g_phymoji_col_count > 0)
                        draw_phymoji_rgb(dpy, g_buf, g_buf_gc);
                    else
                        draw_raymarch_block_rgb(dpy, g_buf, g_buf_gc, bg_r, bg_g, bg_b);
                    /* REAL FIX 2026-08-31, direct live report ("some
                     * entities... when rotated, leave a 'red shadow' of
                     * their 2d shape") - see update_entity_shape_from_3d()'s
                     * own header comment for the full root cause.
                     * Cursword exempt - see that same comment. */
                    if (!g_is_cursword)
                        update_entity_shape_from_3d(dpy, win, g_buf, bg_r, bg_g, bg_b);
                    was_3d_last_frame = 1;
                } else {
                    draw_sprite_rgb(dpy, g_buf, g_buf_gc, bg_r, bg_g, bg_b);
                    /* REAL, NEW 2026-08-31 - the real other half of the
                     * fix above: coming BACK to 2D from 3D must restore
                     * the window's real shape to the flat sprite's own
                     * silhouette (update_entity_shape_from_3d() left it
                     * pinned to whatever the last 3D frame's raymarch
                     * happened to cover), or the entity would stay stuck
                     * shaped like its last 3D pose forever - only runs
                     * on the actual mode transition, not every 2D
                     * frame. */
                    if (was_3d_last_frame && !g_is_cursword) {
                        Pixmap smask = XCreatePixmap(dpy, win, WIN_PX, WIN_PX, 1);
                        GC smask_gc = XCreateGC(dpy, smask, 0, NULL);
                        build_shape_mask(dpy, win, smask_gc, smask);
                        XFreeGC(dpy, smask_gc);
                        XFreePixmap(dpy, smask);
                    }
                    was_3d_last_frame = 0;
                }
            }
            else if (g_font_loaded) draw_glyph_rgb(dpy, g_buf, g_buf_gc, glyph);
            /* REAL, NEW 2026-08-30, direct instruction ("camera pan/
             * zoom moves the whole desktop") - a real, desktop-wide
             * screen-position offset while in 3D mode. win_x/win_y
             * themselves (the entity's own TRUE logical grid position,
             * used by drag/arrow-nudge/click-to-place/write_pos) are
             * deliberately left untouched - this only corrects the
             * real, DISPLAYED X11 position, an offset applied on top,
             * so panning can never corrupt an entity's own saved
             * position. Snaps back to the true win_x/win_y the instant
             * camera_mode leaves 3/4. Tracked with a static "last
             * applied" pair so this is a real no-op XMoveWindow-wise
             * on every frame pan/mode aren't actually changing (avoids
             * fighting a concurrent drag/arrow-nudge's own, separate
             * XMoveWindow calls more than strictly necessary). */
            {
                static int last_disp_x = -999999, last_disp_y = -999999;
                int in_3d = (g_camera_mode == 3 || g_camera_mode == 4);
                int disp_x = in_3d ? win_x + g_cam_pan_x : win_x;
                int disp_y = in_3d ? win_y + g_cam_pan_y : win_y;
                if (disp_x != last_disp_x || disp_y != last_disp_y) {
                    XMoveWindow(dpy, win, disp_x, disp_y);
                    last_disp_x = disp_x; last_disp_y = disp_y;
                }
            }
            /* REAL FIX 2026-08-30, found live: the halo used to draw
             * BEFORE the sprite, relying on draw_sprite_rgb()'s own
             * per-pixel alpha to let it "peek through" transparent
             * margins - confirmed live this specific sprite has no
             * real alpha transparency (fully opaque square), so the
             * halo was getting completely covered, invisible. Real
             * fix: draw the halo AFTER the sprite instead, right at
             * the window's own edge (inset only 1/4/7px) - a real,
             * always-visible border-glow regardless of any given
             * sprite's own opacity, still a real overlay/ring per §9
             * item 4 (never replaces the sprite, just frames it). */
            if (g_is_cursword && g_cursword_armed) {
                int cx = WIN_PX / 2, cy = WIN_PX / 2;
                /* REAL FIX 2026-08-30, found live: a 3-ring gradient
                 * with thin arcs left real gaps between rings, letting
                 * a few of the sprite's own semi-transparent edge
                 * pixels (alpha 1-127, below build_shape_mask()'s own
                 * >127 cutoff, normally never visible at all) show
                 * through as stray off-color specks once the mask
                 * newly included that span. Real fix: ONE solid ring,
                 * geometry byte-identical to cursword_update_shape()'s
                 * own mask ring (same radius, same line width) -
                 * guaranteed zero gaps since it's the exact same shape,
                 * not an approximation of it. */
                int halo_radius = WIN_PX / 2 - 5;
                if (halo_radius > 3) {
                    /* Real, direct instruction ("i actually want to
                     * change it to a 'yellow glowing look' instead of
                     * blue") - matches this house's own real "amber
                     * tint = armed" precedent (§3a) more closely than
                     * blue ever did. */
                    /* Real, new 2026-08-30: explicit full alpha (see
                     * draw_sprite_rgb()'s own matching comment) - the
                     * halo itself stays fully opaque, only the disc
                     * behind it (drawn above) is translucent. */
                    XSetForeground(dpy, g_buf_gc, 0xFFFFD400UL);
                    XSetLineAttributes(dpy, g_buf_gc, 9, LineSolid, CapButt, JoinMiter);
                    XDrawArc(dpy, g_buf, g_buf_gc, cx - halo_radius, cy - halo_radius, (unsigned)(halo_radius * 2), (unsigned)(halo_radius * 2), 0, 360 * 64);
                    XSetLineAttributes(dpy, g_buf_gc, 0, LineSolid, CapButt, JoinMiter);
                }

                /* REAL, NEW 2026-08-30, direct instruction ("i still
                 * dont have arrow control. would it help if we did a
                 * text display under cursword with pressed key
                 * history?") - a real, live-visible readout of the last
                 * few keys THIS window's own event loop actually
                 * received (cursword_log_key()'s own header comment has
                 * the full reasoning: tells "key never arrived" apart
                 * from "key arrived but didn't move it" at a glance,
                 * no file/log digging needed). */
                char logline[96] = "";
                size_t loff = 0;
                for (int li = 0; li < g_cursword_log_n; li++) {
                    int wrote = snprintf(logline + loff, sizeof(logline) - loff, "%s%s",
                                          li > 0 ? " " : "", g_cursword_log[li]);
                    if (wrote < 0 || (size_t)wrote >= sizeof(logline) - loff) break;
                    loff += (size_t)wrote;
                }
                /* Real, new 2026-08-30: explicit full alpha, same
                 * reasoning as the halo just above. */
                XSetForeground(dpy, g_buf_gc, 0xFFFFFFFFUL);
                popup_draw_text(dpy, g_buf, g_buf_gc, 2, WIN_PX + 15,
                                 logline[0] ? logline : "(no keys yet)");

                /* REAL, NEW 2026-08-30, direct instruction ("can we do
                 * another debug below sword, that shows camera
                 * angle?") - a second, real line: the exact same
                 * pitch_deg formula build_raymarch_cam() itself
                 * computes from g_cam_tilt (not a separate guess), so
                 * this is always genuinely what the camera is doing
                 * right now, not just the raw tilt number - direct
                 * live use case: "make sure there is no tilt or angle"
                 * is verifiable at a glance without guessing whether
                 * tilt=0 really means pitch=0. */
                char camline[48];
                double dbg_pitch = (g_cam_tilt / 100.0) * 65.0;
                snprintf(camline, sizeof(camline), "tilt=%d pitch=%.0f%s",
                         g_cam_tilt, dbg_pitch, g_emoji_sprite_view_top ? " top" : " front");
                popup_draw_text(dpy, g_buf, g_buf_gc, 2, WIN_PX + 33, camline);

                /* REAL, NEW 2026-08-31, direct instruction ("zx cy
                 * aren't changing z level in 2d or 3d mode yet as far
                 * as im concerned... add another debug row for
                 * cursword that show xyz position") - a real third
                 * line: THIS process's own real win_x/win_y (the true
                 * logical grid position, not the panned display
                 * position) and g_entity_z (this entity's own real,
                 * persisted z - changed only by c/v, see
                 * cursword_handle_camera_key()'s own header comment),
                 * plus the shared g_active_z right next to it so a
                 * z-vs-active-z mismatch (which is what actually
                 * drives real visibility - see the map/unmap logic
                 * earlier in this same loop) is visible at a glance,
                 * not just cursword's own z in isolation. */
                char posline[48];
                snprintf(posline, sizeof(posline), "x=%d y=%d z=%d az=%d",
                         win_x, win_y, g_entity_z, g_active_z);
                popup_draw_text(dpy, g_buf, g_buf_gc, 2, WIN_PX + 51, posline);
            }

            /* Present: same compose->present pattern as db-hq/taskbar -
             * one XGetImage capture off the buffer, XPutImage onto the
             * real window (proven pixel-identical in the original Phase
             * 0 test). Falls back to a direct XCopyArea if XGetImage
             * ever fails, matching the same safety fallback used there. */
            XSync(dpy, False);
            /* Real, new 2026-08-30: present the full reserved buffer
             * height while cursword is armed (the key-log strip),
             * exactly WIN_PX otherwise/for every other entity. */
            int present_h = WIN_PX + (g_is_cursword && g_cursword_armed ? CURSWORD_LOG_H : 0);
            int present_w = (g_is_cursword && g_cursword_armed) ? CURSWORD_LOG_W : WIN_PX;
            XImage *frame = XGetImage(dpy, g_buf, 0, 0, present_w, present_h, AllPlanes, ZPixmap);
            if (frame) {
                XPutImage(dpy, win, g_buf_gc, frame, 0, 0, 0, 0, present_w, present_h);
                XDestroyImage(frame);
            } else {
                XCopyArea(dpy, g_buf, win, g_buf_gc, 0, 0, (unsigned)present_w, (unsigned)present_h, 0, 0);
            }
        }
        skip_zfiltered_draw:
        last_frame = now;
    }

    if (popup_win) close_context_menu(dpy, popup_win); /* e.g. closed via keypress while menu was still open */
    if (user_popup_win) close_context_menu(dpy, user_popup_win);
    if (input_popup_win) close_context_menu(dpy, input_popup_win);
    /* Stage 2c PROOF - don't orphan the khtpm menu child if this entity's
     * own window closes while the menu is still up. */
    if (g_khtpm_menu_pid > 0) { kill(g_khtpm_menu_pid, SIGTERM); waitpid(g_khtpm_menu_pid, NULL, WNOHANG); }
    if (g_house_root[0]) {
        livedesk_registry_remove(g_house_root, getpid());
        nav_release_pid(g_house_root, getpid());
    }
    XFreeGC(dpy, popup_gc);
    free(g_sprite_pixels);
    XFreeGC(dpy, g_buf_gc);
    XFreePixmap(dpy, g_buf);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}
/* ============ end tile mode ============ */

int main(int argc, char **argv) {
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
        char desktop_marker[PATH_BUF];
        snprintf(desktop_marker, sizeof(desktop_marker), "%s/#.desktop", argv[1]);
        if (access(desktop_marker, F_OK) == 0) return strip_main(argc, argv);
        return tp_main(argc, argv);
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
    /* REAL, NEW 2026-09-01 - @ toggle: every real window this binary can
     * open (db-hq/events-hq/chat-hai/open-hai) obeys the shared pdl; the
     * popup/settings branches stay pinned below. Loaded before any
     * window creation - override_redirect is creation-time-only. */
    load_override_redirect(g_house_root);
    /* REAL, NEW 2026-08-29 - dbhq_load_font_scale() also reads the new
     * click_two_step key (see click_focus_then_activate()'s own
     * comment); that key applies to EVERY mode's clicks, not just
     * db-hq, so it's loaded here once, unconditionally, before any
     * mode-specific branch - the existing db-hq-only call further
     * down is now a harmless redundant reload, left in place rather
     * than restructured, since removing it isn't needed for this fix. */
    dbhq_load_font_scale();
    snprintf(g_chtpm_path, sizeof(g_chtpm_path), "%s", argv[2]);
    snprintf(g_package_dir, sizeof(g_package_dir), "%s", g_chtpm_path);
    { char *slash = strrchr(g_package_dir, '/'); if (slash) *slash = '\0'; }

    g_window = parse_chtpm(g_chtpm_path);
    if (!g_window) { fprintf(stderr, "khtpm_core_render: failed to parse %s\n", g_chtpm_path); return 1; }
    { struct stat gcst; if (stat(g_chtpm_path, &gcst) == 0) g_chtpm_mtime = gcst.st_mtim; }

    /* REAL Stage 5 §5d.3 step 6 (2026-08-16, khtpm-merge-how2.md §5d) -
     * real, data-driven mode detection - `<window class="swatch-
     * picker">` (matches wraith-alpha's own real "one binary, behavior
     * selected by loaded data" shape, not a new attribute/parser
     * change - class= was already fully generic). */
    for (int i = 0; i < g_window->n_classes; i++) {
        if (strcmp(g_window->classes[i], "swatch-picker") == 0) { g_is_swatch_picker = 1; break; }
        /* REAL Stage 5 §5d.10 (2026-08-16) - db-hq mode, real, data-
         * driven detection (`<window class="db-hq">`, same convention
         * as swatch-picker's own). */
        if (strcmp(g_window->classes[i], "db-hq") == 0) { g_is_db_hq = 1; break; }
        /* REAL §5d.11 (2026-08-16) - events-hq mode, same real
         * convention, matching its own real existing class attribute
         * (`<window class="events-hq-window">`, unchanged - no new
         * class token needed, this app's own class already existed). */
        if (strcmp(g_window->classes[i], "events-hq-window") == 0) { g_is_events_hq = 1; break; }
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
        if (strcmp(g_window->classes[i], "stats-hq") == 0) { g_is_stats_hq = 1; g_is_db_hq = 1; break; }
        /* REAL, NEW 2026-08-25 (Stage 2 palettes migration) - see
         * g_is_palettes's own declaration comment. */
        if (strcmp(g_window->classes[i], "palettes") == 0) { g_is_palettes = 1; g_is_db_hq = 1; break; }
        /* REAL, NEW 2026-08-25 (Stage 3 bookmarks migration off the
         * deprecated standalone khtpm_hq_render.c) - bm_menu.sh
         * composes <window class="database-window bookmarks">. */
        if (strcmp(g_window->classes[i], "bookmarks") == 0) { g_is_bookmarks = 1; g_is_db_hq = 1; break; }
    }

    /* REAL, NEW 2026-08-30 - piececraft-hq board-view mode, checked
     * separately from the chain above (not folded in) since it early-
     * returns before any of the shared X11/Elem/CSS setup below runs -
     * see g_is_pchq_board's own declaration comment. argv[3] (optional,
     * default "piececraft-hq") is the host project id whose live
     * board-viewer session to display - kept as a real argument rather
     * than hardcoded so this mode isn't accidentally piececraft-hq-only
     * at the C level, only at the .chtpm launch site. */
    for (int i = 0; i < g_window->n_classes; i++) {
        if (strcmp(g_window->classes[i], "pchq-board") == 0) { g_is_pchq_board = 1; break; }
    }
    if (g_is_pchq_board) {
        const char *host = (argc >= 4 && argv[3][0]) ? argv[3] : "piececraft-hq";
        return run_pchq_board_mode(g_house_root, host);
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
    if (g_is_events_hq) {
        if (argc < 5) { fprintf(stderr, "usage: %s <house_root> <chtpm_path> <event_pkg_dir> <entity_label>\n", argv[0]); return 1; }
        snprintf(g_evhq_pkg_dir, sizeof(g_evhq_pkg_dir), "%s", argv[3]);
        snprintf(g_evhq_entity_label, sizeof(g_evhq_entity_label), "%s", argv[4]);
    } else if (argc >= 5) {
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
        if (g_is_db_hq || g_is_events_hq) {
            snprintf(css_path, sizeof(css_path), "%s", g_chtpm_path);
            char *dot = strrchr(css_path, '.');
            if (dot) snprintf(dot, sizeof(css_path) - (size_t)(dot - css_path), ".css");
        } else {
            snprintf(css_path, sizeof(css_path), "%s/*.monads/*.livedesk-taskbar/ops/%s",
                     g_house_root, g_is_swatch_picker ? "taskbar_settings.css" : "entity_menu_default.css");
        }
        memset(&g_sheet, 0, sizeof(g_sheet));
        css_load(css_path, &g_sheet);
    }

    /* REAL Stage 5 §5d.10 (2026-08-16) - db-hq mode one-time init,
     * ported verbatim from khtpm_hq_render.c's own main(): real state
     * paths, font-scale/focus-grab/window-position .pdl read, common-
     * events load, real fork()+execl() module launch (the <module
     * src="..."/> tag), sidebar/panel content injection, signal
     * handlers for the manager-cleanup-on-TERM real fix. */
    if (g_is_db_hq) {
        signal(SIGTERM, dbhq_handle_term_signal);
        signal(SIGINT, dbhq_handle_term_signal);

        /* g_is_stats_hq: own state/action filenames so a real db-hq AND
         * a real stats-hq window can run at once without colliding
         * (2026-08-25, see the class-dispatch loop's own comment above
         * for the full rationale). */
        snprintf(g_dbhq_events_state_path, sizeof(g_dbhq_events_state_path),
                 g_is_stats_hq ? "%s/#.desktop/stats_hq_common_events.state.txt"
                               : "%s/#.desktop/db_hq_common_events.state.txt", g_house_root);
        snprintf(g_dbhq_action_path, sizeof(g_dbhq_action_path),
                 g_is_stats_hq ? "%s/#.desktop/stats_hq_action.txt"
                               : "%s/#.desktop/db_hq_action.txt", g_house_root);

        /* Real Terms tab wiring (2026-08-28, first tab to use dbhq_tab_
         * is_real()'s new generic path alongside Common Events) - a
         * second, independent real manager launched the SAME way
         * Common Events' own khtpm_hq_manager.+x is below (plain fork+
         * execl, not the <module src="..."/> single-slot mechanism,
         * since that XML tag only supports one manager per window).
         * Not gated to db-hq-only vs stats-hq since Terms is a real
         * game-database concept, not a per-session stat - launches
         * unconditionally alongside Common Events, same as it does. */
        if (!g_is_stats_hq && !g_is_palettes && !g_is_bookmarks) {
            snprintf(g_dbhq_terms_state_path, sizeof(g_dbhq_terms_state_path),
                     "%s/#.desktop/db_hq_terms.state.txt", g_house_root);
            char terms_bin[PATH_BUF];
            snprintf(terms_bin, sizeof(terms_bin), "%s/*.monads/*.livedesk-taskbar/ops/+x/terms_hq_manager.+x", g_house_root);
            char terms_pkgdir[PATH_BUF];
            snprintf(terms_pkgdir, sizeof(terms_pkgdir), "%s/#.desktop", g_house_root);
            pid_t terms_pid = fork();
            if (terms_pid == 0) {
                execl(terms_bin, terms_bin, g_house_root, terms_pkgdir, (char *)NULL);
                _exit(1);
            }
            snprintf(g_dbhq_actors_state_path, sizeof(g_dbhq_actors_state_path),
                     "%s/#.desktop/db_hq_actors.state.txt", g_house_root);
            char actors_bin[PATH_BUF];
            snprintf(actors_bin, sizeof(actors_bin), "%s/*.monads/*.livedesk-taskbar/ops/+x/actors_hq_manager.+x", g_house_root);
            pid_t actors_pid = fork();
            if (actors_pid == 0) {
                execl(actors_bin, actors_bin, g_house_root, terms_pkgdir, (char *)NULL);
                _exit(1);
            }
            dbhq_load_actors();
            {
                char pub_bin[PATH_BUF];
                snprintf(pub_bin, sizeof(pub_bin),
                         "%s/*.monads/*.livedesk-taskbar/ops/+x/dbhq_pdl_publish_manager.+x", g_house_root);
                for (int li = 0; li < DBHQ_N_LIST_TABS; li++) {
                    snprintf(g_dbhq_list_state_path[li], sizeof(g_dbhq_list_state_path[li]),
                             "%s/#.desktop/%s", g_house_root, g_dbhq_list_cfg[li].state_name);
                    char src_rel[PATH_BUF];
                    snprintf(src_rel, sizeof(src_rel), "&.widgits/db-hq/data/%s", g_dbhq_list_cfg[li].pdl_name);
                    pid_t pid = fork();
                    if (pid == 0) {
                        execl(pub_bin, pub_bin, g_house_root, terms_pkgdir, src_rel,
                              g_dbhq_list_cfg[li].state_name, (char *)NULL);
                        _exit(1);
                    }
                    dbhq_load_list_tab(li);
                }
            }
        }

        g_win_x = 100; g_win_y = 100; /* real db-hq default, distinct from the popup modes' 300,300 */
        dbhq_load_font_scale();
        g_dbhq_chrome_h = scaled(26);

        memset(g_dbhq_close_elem, 0, sizeof(*g_dbhq_close_elem));
        snprintf(g_dbhq_close_elem->tag, sizeof(g_dbhq_close_elem->tag), "closebtn");

        Elem *module_elem = find_by_tag(g_window, "module");
        if (module_elem && module_elem->label[0]) dbhq_launch_module(module_elem->label, module_elem->id);
        atexit(dbhq_cleanup_module);
        if (!g_is_stats_hq && !g_is_palettes && !g_is_bookmarks && g_dbhq_current_tab == DB_HQ_ACTORS_TAB)
            dbhq_show_actors();

        /* REAL, NEW 2026-08-25 (bookmarks manager port) - bookmarks is
         * per-pal (g_package_dir, the pal dir - not house-wide like db-hq/
         * stats-hq's own g_house_root-relative state paths above) and
         * has no sidebar/tabbar - it gets its own init branch instead of
         * being forced through the sidebar-shaped path below. */
        if (g_is_bookmarks) {
            snprintf(g_bm_state_path, sizeof(g_bm_state_path), "%s/bookmarks_state.txt", g_package_dir);
            Elem *panel = find_by_tag(g_window, "panel");
            if (panel) {
                g_bm_static_title = find_by_tag(panel, "title");
                g_bm_static_hint = find_by_id(panel, "bm-hint");
                g_bm_static_newplus = find_by_id(panel, "bm-newplus");
                g_bm_static_openfolder = find_by_id(panel, "bm-openfolder");
                dbhq_load_bookmark_state();
                dbhq_inject_bookmark_items(panel);
            }
        } else if (g_is_palettes && module_elem && module_elem->label[0]) {
            /* REAL, NEW 2026-08-25 (palettes manager port) - only real
             * picker categories (emojis/elements) declare a <module> tag;
             * stub categories (rmmv/cdda/...) are fully static (title +
             * hint + one doc-link button) and must NOT go through this
             * injection path - it only knows about title/hint, so it
             * would silently wipe the stub's own doc-link button. Category
             * is derived from the chtpm's own basename (palettes-<key>.
             * chtpm), same "safe derivation direction" convention
             * bm_menu.sh's own provision_bookmarks() comment documents -
             * matches module_elem->id (<module args="<key>"/>) exactly,
             * since palettes_menu.sh's launch_cat() names the file after
             * the same key it passes as args=. */
            const char *base = strrchr(g_chtpm_path, '/');
            base = base ? base + 1 : g_chtpm_path;
            char catbuf[64];
            snprintf(catbuf, sizeof(catbuf), "%s", base);
            char *dot = strrchr(catbuf, '.');
            if (dot) *dot = '\0';
            const char *prefix = "palettes-";
            const char *cat = (strncmp(catbuf, prefix, strlen(prefix)) == 0) ? catbuf + strlen(prefix) : catbuf;
            snprintf(g_pal_category, sizeof(g_pal_category), "%s", cat);
            snprintf(g_pal_state_path, sizeof(g_pal_state_path), "%s/palettes-%s_state.txt", g_package_dir, g_pal_category);
            /* REAL FIX 2026-08-27 - read the manager's own published
             * wide-layout flag (see g_pal_layout_wide's own header
             * comment) instead of hardcoding a category name here. */
            g_pal_layout_wide = 0;
            char layout_path[PATH_BUF];
            snprintf(layout_path, sizeof(layout_path), "%s/palettes-%s_layout.txt", g_package_dir, g_pal_category);
            FILE *lf = fopen(layout_path, "r");
            if (lf) {
                char lline[64];
                if (fgets(lline, sizeof(lline), lf) && strncmp(lline, "wide=", 5) == 0)
                    g_pal_layout_wide = atoi(lline + 5);
                fclose(lf);
            }

            Elem *panel = find_by_tag(g_window, "panel");
            if (panel) {
                g_pal_static_title = find_by_tag(panel, "title");
                g_pal_static_hint = find_by_tag(panel, "text");
                /* Capture the chtpm's own real default TITLE text ONCE,
                 * before anything ever overwrites it - see g_pal_default_
                 * hint's own header comment. REAL FIX, same testing pass:
                 * originally targeted g_pal_static_hint (.pal-hint), but
                 * live pixel-dump verification found that Elem never
                 * renders at all even for its own unmodified default text
                 * - a separate, pre-existing layout bug, not chased
                 * further here. g_pal_static_title (.block-title) is
                 * confirmed-rendering (it's the visible "palettes: RPG
                 * Maker Tiles" line), so the armed note goes there
                 * instead - variable name kept as "hint" throughout for
                 * a smaller diff, but it now drives the title Elem. */
                if (g_pal_static_title && !g_pal_default_hint[0]) {
                    snprintf(g_pal_default_hint, sizeof(g_pal_default_hint), "%s", g_pal_static_title->label);
                }
                if (strcmp(g_pal_category, "rmmv") == 0) {
                    snprintf(g_pal_armed_path, sizeof(g_pal_armed_path), "%s/state/rmmv_armed.txt", g_package_dir);
                    /* REAL BUG FIX 2026-08-29, direct live report
                     * ("window is opening auto armed, it shouldnt") -
                     * a fresh window's g_pal_armed_checksum starts at
                     * 0, but a stale rmmv_armed.txt left over from a
                     * PREVIOUS session already has real content - the
                     * first poll tick then sees checksum != 0, treats
                     * that as a fresh "just armed" change, and shows
                     * the old ARMED/Placed text even though this
                     * process holds no real grab at all. A truly fresh
                     * window session should never inherit armed state
                     * from a previous one - delete it. */
                    unlink(g_pal_armed_path);
                } else {
                    g_pal_armed_path[0] = '\0';
                }
                g_pal_armed_checksum = 0;
                snprintf(g_pal_options_path, sizeof(g_pal_options_path), "%s/rmmv_options.txt", g_package_dir);
                dbhq_load_palette_state();
                dbhq_load_palette_options();
                dbhq_inject_palette_tiles(panel);
            }
        } else {
            dbhq_load_common_events();
            if (g_dbhq_n_events > 0) g_dbhq_selected_event = 0;

            Elem *sidebar = find_by_tag(g_window, "sidebar");
            dbhq_inject_sidebar_items(sidebar);
            if (g_is_stats_hq) {
                stats_populate_panel(g_dbhq_selected_event);
            } else {
                Elem *panel_text = find_by_tag(g_window, "text");
                if (panel_text && g_dbhq_selected_event >= 0) snprintf(panel_text->label, sizeof(panel_text->label), "%s", g_dbhq_events[g_dbhq_selected_event]);
            }
        }
    }

    /* REAL §5d.11 (2026-08-16) - events-hq mode one-time init, ported
     * verbatim from khtpm_events_hq_render.c's own main(): manager
     * state paths, page-list load, entity sprite, real fork()+execl()
     * module launch (3 real args, not 1 - see evhq_launch_module()'s
     * own header comment), signal handlers, real initial page-data
     * refresh. */
    if (g_is_events_hq) {
        XSetErrorHandler(evhq_nonfatal_x_error);
        signal(SIGTERM, evhq_handle_term_signal);
        signal(SIGINT, evhq_handle_term_signal);

        memset(g_evhq_close_elem, 0, sizeof(*g_evhq_close_elem));
        snprintf(g_evhq_close_elem->tag, sizeof(g_evhq_close_elem->tag), "closebtn");

        if (access(g_evhq_pkg_dir, F_OK) != 0) mkdir(g_evhq_pkg_dir, 0755);
        evhq_init_manager_paths();
        evhq_load_pages();
        evhq_load_entity_sprite();

        g_win_x = 120; g_win_y = 120; /* real events-hq default, distinct from db-hq's 100,100 and the popup modes' 300,300 */

        Elem *evhq_module_elem = find_by_tag(g_window, "module");
        if (evhq_module_elem && evhq_module_elem->label[0]) evhq_launch_module(evhq_module_elem->label);
        atexit(evhq_cleanup_module);

        evhq_refresh_page_data(g_window); /* real, populates pagetabs/trigger/commands before the first layout pass */
    }

    if (g_is_swatch_picker) {
        static const char *hex[12] = { "#000000","#ffffff","#1a1a1a","#e5e5e5","#ef4444","#f97316","#eab308","#22c55e","#06b6d4","#3b82f6","#8b5cf6","#ec4899" };
        for (int i = 0; i < 12; i++) { g_palette_hex[i] = hex[i]; g_palette_name[i] = g_palette_name_buf[i]; }
        g_win_w = 420;
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
     * (dbhq_measure_text_px()), unlike the popup modes' fixed-height
     * rows which never needed dpy this early. Harmless reorder for
     * popup modes - dpy/screen/cmap weren't used before this point
     * either way. */
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
     * unhandled error. evhq_nonfatal_x_error() already existed for
     * exactly this class of "best-effort X call, never worth crashing
     * the whole app over" case, but was only ever installed for
     * events-hq mode - installing it globally here, at the earliest
     * possible point, so EVERY mode gets the same real safety net
     * (this fixes the crash without needing to chase every individual
     * best-effort X call across every mode one at a time). */
    XSetErrorHandler(evhq_nonfatal_x_error);
    screen = DefaultScreen(dpy);
    cmap = DefaultColormap(dpy, screen);
    font_ui = XftFontOpenName(dpy, screen, "DejaVu Sans:pixelsize=12");
    /* REAL, NEW 2026-08-25 (live report: bookmarks' own path labels
     * carry real emoji dir names, rendered as tofu boxes - "open-hai
     * has an implementation for this we can steal") - loads once, here,
     * for every mode (not just db-hq/bookmarks): any label text in any
     * consumer can legitimately contain emoji, this house's own
     * directory names prove that. */
    khtpm_load_emoji_tiles(g_house_root);

    assign_nav_and_layout();

    /* REAL Stage 5 §5d.10 (2026-08-16) - db-hq mode: real WM-managed
     * window creation + own event loop, genuinely different shape from
     * the popup modes below (chrome-bar drag, FocusIn/Out tracking,
     * _MOTIF_WM_HINTS decorations-off-but-managed, WM_CLASS grab
     * allowlist) - kept as its own real, separate branch rather than
     * interleaved into the popup path, so the 2 already-working popup
     * modes' code is untouched. Returns before reaching the popup
     * window-creation code below. */
    if (g_is_db_hq) {
        int ww = g_window->w, wh = g_window->h;

        XSetWindowAttributes swa;
        swa.background_pixel = alloc_pixel("#141414");
        swa.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | ButtonMotionMask | KeyPressMask | StructureNotifyMask | FocusChangeMask;
        /* REAL FIX 2026-08-29 (OPACITY-PIPELINE-INVESTIGATION-2026-08-29.txt
         * has the full research trail) - override_redirect was set on
         * the struct but never in the value-mask below, so X11 silently
         * ignored it and this was always a real WM-managed window, not
         * override_redirect like the taskbar's own windows (and this
         * file's own popup branch). That's why _NET_WM_WINDOW_OPACITY
         * had zero visible effect despite being set correctly - most
         * compositors, Mutter included, only reliably honor client-
         * requested opacity on unmanaged surfaces. Manual keyboard
         * focus (dbhq_grab_keyboard_retry()/dbhq_soft_focus() below,
         * already real, already used) still applies the same way an
         * override_redirect window gets focus - this doesn't remove or
         * change that logic, just makes the window type match what
         * this file already treats it as. */
        swa.override_redirect = (Bool)g_override_redirect; /* REAL, NEW 2026-09-01 - @ toggle: managed when the shared pdl says false */
        win = XCreateWindow(dpy, RootWindow(dpy, screen), g_win_x, g_win_y, (unsigned)ww, (unsigned)wh, 0,
                             CopyFromParent, InputOutput, CopyFromParent,
                             CWBackPixel | CWEventMask | CWOverrideRedirect, &swa);
        render_managed_wm_hints(dpy, win, !g_override_redirect); /* REAL, NEW 2026-09-01 - @ "normal" mode: undecorated + no shell chrome when managed */
        {
            /* REAL FIX 2026-08-29 (OPACITY-PIPELINE-INVESTIGATION-2026-08-29.txt
             * + part2.txt, root-caused by a delegated Haiku subagent) -
             * _MOTIF_WM_HINTS and WM_DELETE_WINDOW (via XSetWMProtocols)
             * REMOVED, in addition to the XSetWMHints/XSetWMNormalHints
             * already removed above. Live xprop diff against the
             * taskbar's own real, visibly-transparent windows
             * (khtpm_strip_parser.c, confirmed by direct user
             * observation) showed these were the ONLY remaining
             * property differences once override_redirect was added -
             * both are real ICCCM/WM-cooperation signals telling
             * Mutter "manage me," directly contradicting override_
             * redirect="I'm unmanaged," which made the compositor fall
             * back to its own WM-level opacity handling instead of
             * honoring the client's _NET_WM_WINDOW_OPACITY. Neither
             * property does anything useful on an undecorated
             * override_redirect window (no titlebar/close button exists
             * for the WM to route a close-request through anyway) -
             * hq_run_event_loop()'s own wm_delete_loop atom comparison
             * below is untouched and harmless if never matched. The
             * taskbar's own real windows never set either property. */
        }
        {
            XClassHint *ch = XAllocClassHint();
            if (ch) { ch->res_name = (char *)"MuchiverseLivedesk"; ch->res_class = (char *)"MuchiverseLivedesk"; XSetClassHint(dpy, win, ch); XFree(ch); }
        }
        XMapWindow(dpy, win);
        set_window_opacity(dpy, win, load_theme_opacity());
        XSync(dpy, False);
        { XWindowAttributes wa; if (XGetWindowAttributes(dpy, win, &wa)) { g_win_x = wa.x; g_win_y = wa.y; } }
        render_managed_sink_below(dpy, win); /* REAL, NEW 2026-09-01 - @ "normal" mode: drop this managed window below native apps */
        nav_tab_register(g_is_palettes ? "pal" : g_is_bookmarks ? "bm" : g_is_stats_hq ? "stats" : "dbhq",
                         g_is_palettes ? "palettes" : g_is_bookmarks ? "bookmarks" : g_is_stats_hq ? "stats-hq" : "db-hq");
        if (g_dbhq_focus_grab_enabled) { dbhq_grab_keyboard_retry(); dbhq_soft_focus(); }
        XSync(dpy, False);
        { XEvent stale_ev; while (XCheckWindowEvent(dpy, win, ButtonPressMask | KeyPressMask, &stale_ev)) { } }

        gc = XCreateGC(dpy, win, 0, NULL);
        buf = XCreatePixmap(dpy, win, (unsigned)ww, (unsigned)wh, (unsigned)DefaultDepth(dpy, screen));
        xftdraw_buf = XftDrawCreate(dpy, buf, DefaultVisual(dpy, screen), cmap);
        g_buf_w = ww; g_buf_h = wh;

        redraw();
        /* REAL FIX 2026-08-29 part 3 (OPACITY-PIPELINE-INVESTIGATION-2026-08-29-
         * part3.txt) - khtpm_strip_parser.c's own taskbar has a documented
         * "KISS opacity-on-reset fix" (its own comment near set_window_opacity()
         * relaunch calls) - Mutter/XWayland does not reliably honor
         * _NET_WM_WINDOW_OPACITY set at map-time on an override_redirect
         * window's FIRST paint; it must be re-applied after the window has
         * been visible/painted for at least one real frame. The taskbar
         * already does this (XFlush + usleep(200000) + re-set opacity after
         * its first real draw calls); this branch never did. Applying the
         * exact same pattern here. */
        XFlush(dpy);
        usleep(200000);
        set_window_opacity(dpy, win, load_theme_opacity());
        XFlush(dpy);
        if (dbhq_marker_pilot()) {
            /* snapshot so a leftover marker file does not force a second paint */
            (void)consume_frame_changed();
        }

        if (argc > 3 && strcmp(argv[3], "--dump-and-exit") == 0) { dump_frame_png(); g_quit = 1; }

        Atom wm_delete_loop = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
        hq_run_event_loop(wm_delete_loop, 0);

        nav_tab_unregister();
    history_unregister();
        XUngrabKeyboard(dpy, CurrentTime);
        XftDrawDestroy(xftdraw_buf);
        XFreePixmap(dpy, buf);
        XFreeGC(dpy, gc);
        XDestroyWindow(dpy, win);
        XCloseDisplay(dpy);

        /* REAL FIX (2026-08-17, live report: "chat hai... when i use [x]
         * to close, closes all desktop entures (bad)" - confirmed the
         * SAME real bug also affects db-hq's own [X], byte-identical
         * code). ktb_quit_and_save() is a real, TASKBAR-LEVEL quit
         * action - it calls livedesk_close_all() + livedesk_kill_stray_
         * entities() (real, desktop-wide entity teardown) and removes
         * the shared taskbar pidfile (ktb_unlink_pidfile()). NONE of
         * that is appropriate for a single sub-app window closing -
         * this block was ported from db-hq's own original standalone
         * code under a mistaken assumption it needed real "KtbState
         * persistence" on exit; it never did. Removed entirely, not
         * narrowed - `ktb` was only ever used for this one call. */
        return 0;
    }

    /* REAL §5d.11 (2026-08-16) - events-hq mode: real WM-managed window
     * creation + own event loop, kept as its own separate branch (not
     * interleaved into db-hq's or the popup modes' code) since its real
     * drag/focus/poll logic, while similar in shape to db-hq's, is a
     * genuinely separate real implementation (own globals, own close
     * elem, own picker-overlay-aware click gating) - same real
     * per-mode-exception precedent as everywhere else in this file. */
    if (g_is_events_hq) {
        int ww = g_window->w, wh = g_window->h;

        XSetWindowAttributes swa;
        swa.background_pixel = alloc_pixel("#141414");
        swa.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | ButtonMotionMask | KeyPressMask | StructureNotifyMask | FocusChangeMask;
        /* REAL FIX 2026-08-29 - see db-hq branch's own identical
         * comment above (OPACITY-PIPELINE-INVESTIGATION-2026-08-29.txt
         * has the full research trail) - same real bug, same fix. */
        swa.override_redirect = (Bool)g_override_redirect; /* REAL, NEW 2026-09-01 - @ toggle: managed when the shared pdl says false */
        win = XCreateWindow(dpy, RootWindow(dpy, screen), g_win_x, g_win_y, (unsigned)ww, (unsigned)wh, 0,
                             CopyFromParent, InputOutput, CopyFromParent,
                             CWBackPixel | CWEventMask | CWOverrideRedirect, &swa);
        render_managed_wm_hints(dpy, win, !g_override_redirect); /* REAL, NEW 2026-09-01 - @ "normal" mode: undecorated + no shell chrome when managed */
        {
            /* REAL FIX 2026-08-29 - see db-hq branch's own identical
             * comment above (OPACITY-PIPELINE-INVESTIGATION-2026-08-29.txt
             * + part2.txt) - _MOTIF_WM_HINTS/WM_DELETE_WINDOW removed
             * too, same reasoning: root-caused as the actual blocker by
             * a delegated Haiku subagent's own live xprop diff. */
        }
        {
            XClassHint *ch = XAllocClassHint();
            if (ch) { ch->res_name = (char *)"MuchiverseLivedesk"; ch->res_class = (char *)"MuchiverseLivedesk"; XSetClassHint(dpy, win, ch); XFree(ch); }
        }
        XMapWindow(dpy, win);
        set_window_opacity(dpy, win, load_theme_opacity());
        XSync(dpy, False);
        { XWindowAttributes wa; if (XGetWindowAttributes(dpy, win, &wa)) { g_win_x = wa.x; g_win_y = wa.y; } }
        render_managed_sink_below(dpy, win); /* REAL, NEW 2026-09-01 - @ "normal" mode: drop this managed window below native apps */
        nav_tab_register("evhq", "events-hq");

        gc = XCreateGC(dpy, win, 0, NULL);
        buf = XCreatePixmap(dpy, win, (unsigned)ww, (unsigned)wh, (unsigned)DefaultDepth(dpy, screen));
        xftdraw_buf = XftDrawCreate(dpy, buf, DefaultVisual(dpy, screen), cmap);
        g_buf_w = ww; g_buf_h = wh;

        redraw();
        /* REAL FIX 2026-08-29 part 3 - see db-hq branch's own identical
         * comment above (OPACITY-PIPELINE-INVESTIGATION-2026-08-29-part3.txt)
         * - same real "opacity-on-reset" quirk, same fix, ported from
         * khtpm_strip_parser.c's own already-documented pattern. */
        XFlush(dpy);
        usleep(200000);
        set_window_opacity(dpy, win, load_theme_opacity());
        XFlush(dpy);

        Atom wm_delete_loop = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
        hq_run_event_loop(wm_delete_loop, 0);

        nav_tab_unregister();
    history_unregister();
        XftDrawDestroy(xftdraw_buf);
        XFreePixmap(dpy, buf);
        XFreeGC(dpy, gc);
        XDestroyWindow(dpy, win);
        XCloseDisplay(dpy);
        return 0;
    }

    XSetWindowAttributes swa;
    swa.background_pixel = alloc_pixel("#1c1c1c"); /* real dark default - no white-flash bug, ai-cell's own proven pattern, not WhitePixel */
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
    swa.override_redirect = g_is_swatch_picker ? True : (Bool)g_override_redirect;
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
    render_managed_wm_hints(dpy, win, g_is_swatch_picker ? 0 : !g_override_redirect); /* REAL, NEW 2026-09-01 - managed branch: undecorated + no shell chrome (post-map sink-below lands after XMapRaised) */
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
         * never reads it. */
        Elem *module_elem = find_by_tag(g_window, "module");
        if (module_elem && module_elem->label[0]) {
            char parent_pid_path[PATH_BUF];
            snprintf(parent_pid_path, sizeof(parent_pid_path), "%s/module_parent.pid", g_package_dir);
            FILE *ppf = fopen(parent_pid_path, "w");
            if (ppf) { fprintf(ppf, "%d\n", (int)getpid()); fclose(ppf); }
            g_dbhq_module_pid = launch_module(module_elem->label, g_house_root, g_package_dir,
                                               module_elem->id[0] ? module_elem->id : NULL);
            atexit(dbhq_cleanup_module);
        }
    }

    gc = XCreateGC(dpy, win, 0, NULL);
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

    redraw();
    /* REAL FIX 2026-08-29 part 3 - see db-hq branch's own identical
     * comment above (OPACITY-PIPELINE-INVESTIGATION-2026-08-29-part3.txt)
     * - same real "opacity-on-reset" quirk, same fix, ported from
     * khtpm_strip_parser.c's own already-documented pattern. */
    XFlush(dpy);
    usleep(200000);
    set_window_opacity(dpy, win, load_theme_opacity());
    XFlush(dpy);

    if (g_is_swatch_picker) {
        /* Reset house action/state before the manager starts so a leftover
         * PICK: from the last session cannot count as the first pick. */
        {
            char ap[PATH_BUF], sp[PATH_BUF];
            snprintf(ap, sizeof(ap), "%s/#.desktop/taskbar_settings_action.txt", g_house_root);
            snprintf(sp, sizeof(sp), "%s/#.desktop/taskbar_settings_state.txt", g_house_root);
            FILE *af = fopen(ap, "w");
            if (af) { fputs("seq=0\n", af); fclose(af); }
            FILE *sf = fopen(sp, "w");
            if (sf) { fputs("phase=0\nbg=-1\nfg=-1\napply=0\n", sf); fclose(sf); }
            g_swatch_action_seq = 0;
            g_phase = 0;
            g_chosen_bg_idx = -1;
            g_chosen_fg_idx = -1;
        }
        char mb[PATH_BUF];
        snprintf(mb, sizeof(mb), "%s/*.monads/*.livedesk-taskbar/ops/+x/swatch_picker_manager.+x", g_house_root);
        g_swatch_mgr_pid = fork();
        if (g_swatch_mgr_pid == 0) {
            execl(mb, mb, g_house_root, (char *)NULL);
            _exit(1);
        }
    }
    hq_run_event_loop(wm_delete, 1);
    if (g_swatch_mgr_pid > 0) { kill(g_swatch_mgr_pid, SIGTERM); g_swatch_mgr_pid = -1; }

    XftDrawDestroy(xftdraw_buf);
    XFreeGC(dpy, gc);
    XFreePixmap(dpy, buf);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}
