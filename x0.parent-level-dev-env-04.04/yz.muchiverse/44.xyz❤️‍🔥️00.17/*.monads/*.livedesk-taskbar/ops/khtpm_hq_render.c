/* khtpm_hq_render.c — db-hq: first proof of the HQML CSS styling layer
 * (au11-hq/HQML-DESIGN+PLANS.md Phase 1), scoped to the Common Events
 * section per au11-hq/rpg-maker-database.html and au11-hq/todo-a12.txt.
 *
 * Standalone binary, deliberately NOT a modification of
 * khtpm_strip_parser.c/khtpm_strip_layout.c - the taskbar's own renderer
 * is untouched so nothing here can regress it. Own window, own event
 * loop, own tiny generic tag-tree parser (reads db-hq's own .chtpm tag
 * vocabulary: window/tabbar/tab/sidebar/item/panel/title/text/button),
 * styled via khtpm_css_parser.c against a matching .css file.
 *
 * Usage: khtpm_hq_render.+x <house_root> <chtpm_path>
 * common_events are read/written at <house_root>/common_events/<name>/,
 * the same GLOBAL (not session-scoped) location db-ez uses - see
 * livedesk_build_db_common_events_menu() in khtpm_taskbar_manager.c. */
#include "khtpm_css_parser.h"
#include "khtpm_render_core.c" /* real .c, not a header - see that file's own comment */
#include "khtpm_taskbar_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>
#include <sys/select.h>
#else
#include "khtpm_strip_x11_win.h"
#include <io.h>
#include <direct.h>
#ifndef S_ISDIR
#  define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif
#define mkdir(p, m) _mkdir(p)
#ifndef WNOHANG
#  define WNOHANG 1
#endif
static int hq_kill(pid_t p, int sig) {
    (void)sig;
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)p);
    if (!h) return -1;
    TerminateProcess(h, 1);
    CloseHandle(h);
    return 0;
}
#define kill hq_kill
static pid_t hq_waitpid(pid_t p, int *st, int fl) {
    (void)st; (void)fl;
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)p);
    if (!h) return p;
    DWORD code = 0;
    GetExitCodeProcess(h, &code);
    CloseHandle(h);
    return (code == STILL_ACTIVE) ? 0 : p;
}
#define waitpid hq_waitpid
static int hq_usleep(unsigned usec) {
    DWORD ms = usec / 1000;
    if (ms == 0) ms = 1;
    Sleep(ms);
    return 0;
}
#define usleep hq_usleep
#endif

/* debug PNG dump (press 'p') - Xlib/Xft equivalent of the house's own
 * chtpm-rgb-render + dump_rgb_png.c convention (which reads back a GL
 * frame via glReadPixels for GLUT/GLX windows, since those are otherwise
 * unviewable to an agent). db-hq has no GL context - it composes into an
 * offscreen X Pixmap (see `buf` below) and blits with XCopyArea, so the
 * equivalent readback here is XGetImage on that same Pixmap, not
 * glReadPixels. Same vendored stb_image_write.h the house already uses
 * elsewhere (ops/lib/, copied from 014.wsr-pal+2/ops/lib/ - public domain,
 * not re-fetched). */
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "lib/stb_image_write.h"

#define PATH_BUF 4096
#define MAX_ELEMS 512
/* Elem struct + MAX_CHILDREN now come from khtpm_render_core.h (Stage 2a
 * of the khtpm merge, 2026-08-16 - see that header's own comment). */

static Elem g_pool[MAX_ELEMS];
static int g_n_elems = 0;
static char g_house_root[PATH_BUF];
/* Declared here (not at its original 2026-08-16 spot further down) so
 * dump_frame_png()/write_hq_receipt() can read it for per-instance PNG/
 * receipt naming and the receipt's source_layout field - see the
 * 2026-08-24 receipt-port comment above write_hq_receipt(). */
static char g_chtpm_path[PATH_BUF];

/* REAL module launch (Stage 2d, 2026-08-16, direct correction: "explain
 * to me your plan and why its different from the tpmos/wraith
 * examples... they all get their own layouts but can share module,
 * right?"). Ported verbatim from wraith_parser_alpha.c's own
 * launch_module()/cleanup_module() (1.TPMOS_c_+rmmp.0103.0001/projects/
 * wraith-alpha/ops/wraith_parser_alpha.c ~line 1521) - plain fork()+
 * execv(), current_module_pid tracked, killed on exit. The <module
 * src="..."/> tag in dashboard.chtpm gets read after parse_chtpm()
 * returns (see main()); this is THIS shell's own code doing the launch,
 * not an external bash script - matches the real mechanism, not just
 * the file-IPC spirit of it. */
static pid_t g_module_pid = -1;

static void cleanup_module(void) {
    if (g_module_pid > 0) {
        kill(g_module_pid, SIGTERM);
        waitpid(g_module_pid, NULL, WNOHANG);
        g_module_pid = -1;
    }
}

/* REAL FIX 2026-08-16: atexit(cleanup_module) alone does NOT cover the
 * actual relaunch path - open_db_hq.sh kills the previous instance via
 * a raw `kill -TERM`, and SIGTERM's default disposition terminates the
 * process WITHOUT running atexit handlers. Without this, every db-hq
 * relaunch would orphan the previous manager process instead of really
 * replacing it. */
static void handle_term_signal(int sig) {
    (void)sig;
    cleanup_module();
    _exit(0);
}

static void launch_module(const char *src) {
    if (!src || !src[0]) return;
    char full_path[PATH_BUF];
    if (src[0] == '/') snprintf(full_path, sizeof(full_path), "%s", src);
    else snprintf(full_path, sizeof(full_path), "%s/%s", g_house_root, src);

#ifdef _WIN32
    (void)full_path; /* manager PE not required for first window paint */
#else
    g_module_pid = fork();
    if (g_module_pid == 0) {
        execl(full_path, full_path, g_house_root, (char *)NULL);
        _exit(1); /* execl only returns on failure */
    } else if (g_module_pid < 0) {
        fprintf(stderr, "db-hq: launch_module: fork failed for %s\n", full_path);
        g_module_pid = -1;
    }
#endif
}

/* wraith-alpha-standard index nav state (see Elem.nav_index comment) */
static Elem *g_nav[MAX_ELEMS];
static int g_n_nav = 0;
/* Real, visible bug found live (2026-08-12, direct report: "no > is on
 * screen when it opens"): nav 1 used to ALWAYS be the chrome close
 * button, whose "[>N]" badge is deliberately suppressed (too small a
 * box to fit one - see draw_elem()'s own comment) in favor of just an
 * outline ring - so NO visible "[>N]" text existed anywhere on screen at
 * launch. Fixed properly in assign_nav_indices() (close moved to the
 * LAST nav index instead, per direct instruction), so nav 1 defaulting
 * here now lands on the first real content tab and shows immediately,
 * matching the taskbar/context menus always showing an obvious ">" on a
 * real row the instant they open. */
static int g_focus_nav = 1;   /* 1-based, matches nav_index numbering */
static int g_digit_accum = 0;
static int g_quit = 0;
static char g_last_key_label[32] = ""; /* see draw_chrome_bar()'s debug status line */

/* Chrome-bar drag-to-move, direct request 2026-08-12 ("window should be
 * draggable from header tab by mouse"). Now WM-managed with
 * _MOTIF_WM_HINTS decorations=0 (see main()'s own header comment) - a
 * real WM would normally supply titlebar-drag itself, but with
 * decorations off there's no WM-drawn titlebar to drag, so this needs
 * hand-rolled ButtonPress/MotionNotify/ButtonRelease drag, exact same
 * proven shape as 01.muchi-pals-🥚️-13.01/system/egg_window.c's own X11
 * drag block (ButtonPress records x_root/y_root, MotionNotify computes
 * the delta and XMoveWindow's, ButtonRelease ends it) - ported, not
 * reinvented. Scoped to the chrome bar only (not the whole window, since
 * tabs/buttons elsewhere need normal single-click activation). */
static int g_dragging = 0;
static int g_drag_last_x = 0, g_drag_last_y = 0;
/* Running window position, purely accumulated via deltas - matches
 * egg_window.c's own win_start_x/win_start_y exactly. Deliberately NOT
 * re-read from the server mid-drag (XGetWindowAttributes' x/y are
 * PARENT-relative, and a real WM-managed window may be reparented into
 * a frame even with decorations=0 - mixing that with root-relative
 * motion deltas would drift wrong). Initialized to the window's real
 * creation position in main(). */
static int g_win_x = 100, g_win_y = 100;

static Elem *elem_new(const char *tag) {
    if (g_n_elems >= MAX_ELEMS) return NULL;
    Elem *e = &g_pool[g_n_elems++];
    memset(e, 0, sizeof(*e));
    snprintf(e->tag, sizeof(e->tag), "%s", tag);
    return e;
}

/* ---------- tiny generic tag-tree parser ---------- */

static void skip_ws(const char **p) { while (**p && isspace((unsigned char)**p)) (*p)++; }

/* HTML-style case-insensitive attribute-name compare. REAL BUG FIX
 * 2026-08-24 ("it still isn't opening the dir"): apply_attr()'s matches
 * were case-SENSITIVE ("onclick"), but every layout in the house writes
 * camelCase `onClick=` - so the attribute was silently DROPPED for the
 * first real consumer of the generic onClick mechanisms (bookmarks'
 * buttons had empty actions; clicks fell through to domain branches
 * that matched nothing = nothing happened, no error anywhere). The
 * legacy dashboards never hit this because they carry no onClick at
 * all - their interactivity is pure renderer-side state. Hand-rolled
 * to avoid a new include; ASCII-only is correct here (attr names are
 * ASCII in every house layout). */
static int attr_ci_eq(const char *a, const char *b) {
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return 0;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

static void parse_attr_value(const char **p, char *out, size_t outsz) {
    skip_ws(p);
    if (**p != '"') { out[0] = '\0'; return; }
    (*p)++;
    size_t n = 0;
    while (**p && **p != '"') {
        if (n + 1 < outsz) out[n++] = **p;
        (*p)++;
    }
    out[n] = '\0';
    if (**p == '"') (*p)++;
}

static void apply_attr(Elem *e, const char *name, const char *val) {
    if (attr_ci_eq(name, "id")) {
        snprintf(e->id, sizeof(e->id), "%s", val);
    } else if (attr_ci_eq(name, "class")) {
        char tmp[128]; snprintf(tmp, sizeof(tmp), "%s", val);
        char *tok = strtok(tmp, " ");
        while (tok && e->n_classes < CSS_MAX_CLASSES) {
            snprintf(e->classes[e->n_classes], sizeof(e->classes[0]), "%s", tok);
            e->n_classes++;
            tok = strtok(NULL, " ");
        }
    } else if (attr_ci_eq(name, "label")) {
        snprintf(e->label, sizeof(e->label), "%s", val);
    } else if (attr_ci_eq(name, "onclick")) {
        snprintf(e->onclick, sizeof(e->onclick), "%s", val);
    } else if (attr_ci_eq(name, "sprite")) {
        /* REAL, NEW 2026-08-24 - palettes emoji matrix (direct request:
         * "a matrix of png render emojis ... like clock in toolbar").
         * Same sprite= attribute convention the taskbar strip's own
         * layout uses (khtpm_strip_header.chtpm's sprite="${avatar_dir}"
         * user cell) - value is the sprite DIRECTORY whose sprite.csv
         * is the RGBA texture; drawn by draw_elem()'s hq_blit_sprite()
         * instead of the label text (label stays as text-only fallback,
         * matching the strip's own missing-csv behavior). */
        snprintf(e->sprite, sizeof(e->sprite), "%s", val);
    } else if (attr_ci_eq(name, "active")) {
        e->active = (strcmp(val, "true") == 0);
    } else if (attr_ci_eq(name, "src")) {
        /* <module src="..."/> (Stage 2d, 2026-08-16) - real wraith_parser_
         * alpha.c convention (its own module-tag handling supports src=
         * as the primary form, inner text as fallback). Reused e->label
         * to hold it - <module> elements are never drawn, so this is
         * safe and avoids adding a dedicated Elem field for one tag. */
        snprintf(e->label, sizeof(e->label), "%s", val);
    }
}

/* parses one element starting at '<' ; returns pointer just past this element
 * (including its closing tag, if any). */
static const char *parse_element(const char *p, Elem *parent) {
    skip_ws(&p);
    if (*p != '<') return p;
    p++;
    if (*p == '!') { /* comment <!-- ... --> */
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
        char val[256] = "";
        if (*p == '=') { p++; parse_attr_value(&p, val, sizeof(val)); }
        if (attr[0]) apply_attr(e, attr, val);
    }

    /* children, until matching close tag */
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
    size_t rd = fread(buf, 1, (size_t)sz, f);
    buf[rd] = '\0';
    fclose(f);
    const char *p = buf;
    Elem *root = elem_new("__root__");
    /* skip any number of leading top-level comments (e.g. this file's own
     * doc-comment header) before parsing the real root element - a single
     * parse_element() call treats a comment as the whole top-level
     * construct and returns immediately after it, so without this loop
     * the real <window> tag was silently never reached (root stayed
     * empty). */
    for (;;) {
        skip_ws(&p);
        if (!*p) break;
        if (p[0] == '<' && p[1] == '!') {
            const char *end = strstr(p, "-->");
            p = end ? end + 3 : p + strlen(p);
            continue;
        }
        break;
    }
    if (*p == '<') parse_element(p, root);
    if (root->n_children > 0) root = root->children[0];
    free(buf);
    return root;
}

/* find_by_tag() now comes from khtpm_render_core.h (Stage 2a, 2026-08-16). */

/* ---------- data: common_events listing (GLOBAL, house_root-wide) ---------- */

#define MAX_EVENTS 128
static char g_events[MAX_EVENTS][64];
static int g_n_events = 0;
static int g_selected_event = -1;

/* REAL FIX 2026-08-16 (Stage 2d shell/manager split, local-2do-15.txt's
 * "Stage 2d, REDONE correctly" entry): this used to scan common_events/
 * itself, every call. That directory scan is now khtpm_hq_manager.c's
 * job (a separate binary) - it publishes the sorted list to
 * #.desktop/db_hq_common_events.state.txt, and this function just reads
 * that file. The shell no longer touches the filesystem for business
 * data at all, only for its own state file. */
static char g_events_state_path[PATH_BUF];
static time_t g_events_state_mtime = 0;

/* Returns 1 if the list actually changed (caller should re-inject
 * sidebar items + redraw), 0 if unchanged (cheap no-op, safe to call
 * every main-loop tick). */
static int load_common_events(void) {
    struct stat st;
    if (stat(g_events_state_path, &st) != 0) return 0;
    if (st.st_mtime == g_events_state_mtime) return 0; /* unchanged since last read */
    g_events_state_mtime = st.st_mtime;

    g_n_events = 0;
    FILE *f = fopen(g_events_state_path, "r");
    if (!f) return 0;
    char line[128];
    while (g_n_events < MAX_EVENTS && fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = '\0';
        if (len == 0) continue;
        snprintf(g_events[g_n_events], sizeof(g_events[0]), "%s", line);
        g_n_events++;
    }
    fclose(f);
    return 1;
}

/* replaces the sidebar's single <placeholder id="common_events_rows"/>
 * child with one dynamically-built "item" element per common event
 * (mirrors ${common_events_rows} substitution from the design doc, done
 * structurally here instead of as a string template). */
static void inject_sidebar_items(Elem *sidebar) {
    if (!sidebar) return;
    sidebar->n_children = 0;
    for (int i = 0; i < g_n_events; i++) {
        Elem *item = elem_new("item");
        item->parent = sidebar;
        snprintf(item->classes[0], sizeof(item->classes[0]), "data-item");
        item->n_classes = 1;
        snprintf(item->label, sizeof(item->label), "%s", g_events[i]);
        item->active = (i == g_selected_event);
        if (sidebar->n_children < MAX_CHILDREN) sidebar->children[sidebar->n_children++] = item;
    }
    if (g_n_events == 0) {
        Elem *item = elem_new("item");
        item->parent = sidebar;
        snprintf(item->label, sizeof(item->label), "(none yet)");
        if (sidebar->n_children < MAX_CHILDREN) sidebar->children[sidebar->n_children++] = item;
    }
}

/* ---------- layout: CSS overrides a small hand-rolled per-tag flow,
 * since v1 deliberately has no flex/grid engine (see plan) ---------- */

/* Order matches au11-hq/rpg-maker-database.html's own tab-bar exactly
 * (line 301-316) - real RPG Maker MV order, 15 tabs total. Direct
 * correction (2026-08-12): Common Events belongs right after Tilesets,
 * not last; "Terms" is its own 15th tab, separate from "Types" (both
 * exist in the mockup - not a typo/merge). */
static const char *TAB_LABELS[] = {
    "Actors", "Classes", "Skills", "Items", "Weapons", "Armors",
    "Enemies", "Troops", "States", "Animations", "Tilesets",
    "Common Events", "System", "Types", "Terms"
};
#define N_TABS 15
#define COMMON_EVENTS_TAB 11
static int g_current_tab = COMMON_EVENTS_TAB; /* the only wired tab */

static const CssSheet *g_sheet;

static void apply_css(Elem *e, int hover) {
    css_compute_style(g_sheet, e->tag, e->id[0] ? e->id : NULL, e->classes, e->n_classes, hover, &e->style);
}

/* REAL, NEW 2026-08-24 (palettes emoji matrix): style an element and its
 * whole subtree - the panel loop used to style only direct children, so
 * nested <button class="pal-tile"> tiles never got their CSS at all. */
static void apply_css_deep(Elem *e, int hover) {
    if (!e) return;
    apply_css(e, hover);
    for (int i = 0; i < e->n_children; i++) apply_css_deep(e->children[i], hover);
}

/* ---------- palettes matrix scroll (REAL, NEW 2026-08-24,
 * pallette-design.txt VIEW SPECS, direct instruction "the palette view
 * should be a matrix. and there should be a scroll thumb"): the panel's
 * <row class="pal-grid-row"> children scroll as full rows, with a real
 * drawn track+thumb on the panel's right edge. Mechanics ported from the
 * house convention (open-hai/chat transcript ~1318-1340/1649-1655,
 * itself citing wraith-alpha's fs scroll_offset shape): one int offset,
 * clamped [0, total-visible] every layout. Wheel (Button4/5) +
 * PageUp/PageDown step it; nav indices are only assigned to VISIBLE
 * cells (zero-sized elems are skipped by assign_generic_onclick_nav), so
 * digit-jump can never target a scrolled-out tile. Offset resets on
 * chtpm reload (see reload_if_changed). ---------- */
static int g_pal_scroll = 0;       /* first visible grid row index */
static int g_pal_has_grid = 0;
static int g_pal_total_rows = 0, g_pal_visible_rows = 1;
static int g_pal_track_x = 0, g_pal_track_y = 0, g_pal_track_w = 0, g_pal_track_h = 0;
static int g_pal_thumb_y = 0, g_pal_thumb_h = 0;

static int elem_has_class(Elem *e, const char *cls) {
    for (int i = 0; i < e->n_classes; i++)
        if (strcmp(e->classes[i], cls) == 0) return 1;
    return 0;
}

/* X11/Xft globals - declared here (not down in the rendering section)
 * because layout now needs to MEASURE real font metrics, not guess a
 * fixed px-per-char width; that guess (7px/char) was the actual cause of
 * "big and jumbled" text - it didn't match whichever font XftFontOpenName
 * actually resolved, so boxes were sized wrong and labels overlapped. */
static Display *dpy;
static int screen;

/* user-defined UI scale, direct request: "even if the window needed to
 * be bigger... or even reading this from a std user defined font size
 * .pdl so user can adjust scale for readability/access". Shared across
 * all -hq apps (not taskbar-specific), same key=value .pdl convention
 * already used by khtpm_strip_parser.c's load_theme_opacity() (reads
 * #.desktop/livedesk_taskbar.pdl the same way). Applies to BOTH font
 * sizes and layout box sizes (chrome height, row heights, default window
 * size) so a bigger font never gets clipped by boxes that didn't grow
 * with it - text metrics are measured AFTER scaling (measure_text_px()
 * below), so nothing needs a second manual size fixup. */
static double g_font_scale = 1.0;

/* focus_grab: KISS hail-mary, direct instruction 2026-08-12 ("all that
 * focus stuff is overkill... keep it in a separate config/.pdl, do the
 * same as a last hail mary"). Studied egg_window.c (a real "context"
 * entity window, ALSO launched fresh from a click, confirmed reliably
 * keyboard-usable) and found it does ZERO focus/grab calls for its main
 * window - no XSetInputFocus, no XGrabKeyboard, nothing beyond plain
 * override_redirect + XMapWindow. Default flips to that same bare-
 * minimum behavior; the whole soft_focus()/XGrabKeyboard machinery
 * built earlier this session is kept but now OFF by default, toggleable
 * back on via this key without a rebuild if the simple path doesn't
 * actually fix it. */
static int g_focus_grab_enabled = 0;

static void load_font_scale(void) {
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
            if (v >= 0.5 && v <= 3.0) g_font_scale = v; /* sane clamp - not a layout-breaking value */
        } else if (strcmp(line, "focus_grab") == 0) {
            g_focus_grab_enabled = atoi(val) != 0;
        } else if (strcmp(line, "window_x") == 0) {
            g_win_x = atoi(val);
        } else if (strcmp(line, "window_y") == 0) {
            g_win_y = atoi(val);
        }
    }
    fclose(f);
}

static int scaled(int base_px) { return (int)(base_px * g_font_scale + 0.5); }

/* Stage 1 khtpm merge fix (khtpm-merge-how2.md §3.2), font-cache pattern
 * ported verbatim from chat_hai_hq_render.c's own measure_text_px() fix -
 * this was opening+closing a font on every call, called per-tab in
 * layout_pass()'s hot path. Single-slot cache keyed by spec string, only
 * reopens when the spec actually changes. */
static int measure_text_px(const CssStyle *st, const char *text) {
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

/* Own drawn chrome bar (title + close), NOT a window-manager decoration -
 * same idea as wraith-alpha's own chrome row (ops/wraith_parser_alpha.c's
 * g_chrome_icons[]: nav 1 = title, icons after it, 'x' = CHROME_ACTION_
 * CLOSE), direct instruction: "we will create our own chrome bar and
 * title, ok? like in wraith-alpha". Kept to just title + close for this
 * app (no minimize/geom/context-menu - wraith-alpha's fuller icon set
 * isn't needed here). Window height grows by g_chrome_h on top of the
 * CSS/default content height, so nothing below has to shrink to fit it.
 * g_chrome_h (and every other layout constant in layout_pass() below) is
 * scaled by g_font_scale, not just font sizes - a bigger font with
 * same-size boxes just clips, per direct instruction: "even if the
 * window needed to be bigger". */
static int g_chrome_h = 26;
static Elem g_close_elem_storage;
static Elem *g_close_elem = &g_close_elem_storage;
static int g_close_x, g_close_y, g_close_w, g_close_h;

static void layout_pass(Elem *window) {
    apply_css(window, 0);
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
        apply_css(tabbar, 0);
        for (int i = 0; i < tabbar->n_children; i++) {
            Elem *tab = tabbar->children[i];
            tab->active = (i == g_current_tab);
            apply_css(tab, 0);
            /* real measured width, not a guessed px/char - a mismatched
             * guess vs. the font XftFontOpenName actually resolved was
             * the root cause of overlapping/"jumbled" tab labels.
             * measure_text_px() already applies g_font_scale internally,
             * so this only needs to scale its own fixed padding/badge
             * allowance, not the measured part. Measured in this own
             * pre-pass (not while assigning x) so the window can grow to
             * fit ALL tabs first - 15 tabs (au11-hq/rpg-maker-database.
             * html's real count) don't fit the old fixed 900px default,
             * and this app has no flex-wrap engine to fall back on. */
            tab_widths[i] = measure_text_px(&tab->style, tab->label) + scaled(34); /* "[>NN]" badge + padding */
            /* REAL FIX 2026-08-16, Stage 3 port, caught live (first
             * dumped frame showed every tab overlapping at x=0 - tabs
             * had zero/stale width): css_layout_pass()'s own real
             * contract (khtpm_render_core.c's own header comment) is
             * that a child's NATURAL size comes from its own e->w, not
             * a caller-side local array - tab_widths[] alone was never
             * visible to the engine. Write it into the real Elem field
             * the engine actually reads. */
            tab->w = tab_widths[i];
            tabbar_natural_w += tab_widths[i] + 1;
        }
    }
    window->w = window->style.has_width ? window->style.width : (tabbar_natural_w > default_w ? tabbar_natural_w : default_w);
    window->h = content_total_h + g_chrome_h;

    g_close_w = scaled(56); g_close_h = g_chrome_h - scaled(6); /* wide enough for "[>NN] x" - badge + label both now, see draw_elem()'s own comment */
    /* Win: 15 scaled tabs make a ~1340px window; at window_x=100 on a
     * 1536-wide desk the [x] sits near the right edge and is easy to
     * miss / clip. Cap to the work area and pin x so chrome stays on
     * screen. Same helper idea for events-hq / chat-hai later. */
#ifdef _WIN32
    if (dpy) {
        int maxw = DisplayWidth(dpy, DefaultScreen(dpy)) - 16;
        int maxh = DisplayHeight(dpy, DefaultScreen(dpy)) - 16;
        if (maxw > 400 && window->w > maxw) window->w = maxw;
        if (maxh > 200 && window->h > maxh) window->h = maxh;
        g_win_x = 8;
        if (g_win_y < 8) g_win_y = 8;
        if (g_win_y + window->h > maxh) g_win_y = 8;
    }
#endif
    g_close_x = window->w - g_close_w - scaled(4);
    g_close_y = scaled(3);

    if (tabbar) {
        /* REAL PORT 2026-08-16, Stage 3 first live use of
         * css_layout_pass() (khtpm-merge-how2.md §5.3 step 4) - real
         * §5.1b pattern #2 (natural-width row, left-packed). tab->w is
         * pre-measured above (tab_widths[i], via measure_text_px()) per
         * the engine's own real contract (khtpm_render_core.c's own
         * css_layout_pass() header comment) - set BEFORE the engine
         * call, not after. tabbar itself needs display:flex/flex-
         * direction:row for the engine to treat it as a real flex
         * container - set programmatically here rather than editing
         * dashboard.css (real, deliberate choice for this first port:
         * proves the ENGINE live without also needing a real .chtpm/
         * .css authoring-convention decision in the same pass - a real
         * css-authored version is a legitimate future refinement, not
         * required for this port to be real/correct).
         * REAL, KNOWN GAP found while porting (not present in the
         * engine's own §5.1b design, a real limitation): db-hq's own
         * exact geometry insets tabs 2px down and shrinks them 4px tall
         * within the tabbar row (a real cross-axis PADDING) - the
         * engine has no padding/margin concept (§5.1b explicitly found
         * this unneeded by the 3 apps' MAIN patterns, but missed this
         * one cross-axis inset specifically). Real, safe fix: let the
         * engine handle x-positioning (its actual job, natural-width
         * packing) then apply the same real 2px/4px inset by hand
         * afterward - preserves EXACT original pixel geometry, doesn't
         * require extending the engine's own scope mid-port. */
        /* REAL, second gap found while porting: original tx starts at
         * scaled(4) (real left margin) and accumulates a real +1px gap
         * between tabs (tx += tab_widths[i] + 1) - the engine's own
         * pattern #2 packs with ZERO gap (§5.1b confirmed gap
         * unnecessary for the 3 apps' MAIN patterns, but this exact
         * +1px divider gap was missed the same way the cross-axis
         * inset was). Real, safe fix, same shape as the y/h fix above:
         * let the engine compute relative positions, offset the whole
         * row by the real left margin, then add back the real
         * per-index 1px cumulative gap by hand - preserves EXACT
         * original pixel geometry. */
        /* REAL FIX 2026-08-16, caught while porting events-hq's own
         * identical pattern: passing the real 4px left-margin as the
         * ENGINE CALL's own x/avail_w (as this code originally did)
         * shifts tabbar's OWN e->x/e->w too, not just its children -
         * draw_elem()'s real background fill (`XFillRectangle(...,
         * e->x, e->y, e->w, e->h)`) then leaves a real, if subtle
         * (masked by tab-bar bg #1a1a1a vs window bg #141414 being
         * close shades), unfilled 4px sliver on the left edge. Real
         * fix: give the engine the container's own REAL full box
         * (x=0, w=window->w), let it pack children starting at 0, then
         * add the real 4px margin to each child's x by hand alongside
         * the already-existing per-index gap adjustment - same real
         * technique, corrected target. */
        tabbar->style.has_display = 1; tabbar->style.display_flex = 1;
        tabbar->style.has_flex_direction = 1; tabbar->style.flex_row = 1;
        css_layout_pass(tabbar, 0, g_chrome_h, window->w, tabbar_h);
        for (int i = 0; i < tabbar->n_children; i++) {
            Elem *tab = tabbar->children[i];
            tab->x += scaled(4) + i; /* real 4px left margin + cumulative +1px-per-tab gap */
            tab->y = g_chrome_h + scaled(2); tab->h = tabbar_h - scaled(4); /* real cross-axis inset */
        }
    }

    int content_y = g_chrome_h + tabbar_h;
    int content_h = content_total_h - tabbar_h;
    int sidebar_w = scaled(210);

    if (g_current_tab != COMMON_EVENTS_TAB) {
        /* placeholder tabs: no sidebar/panel geometry needed, drawn as
         * one centered message directly against the window in render_pass() */
        return;
    }

    if (sidebar) {
        apply_css(sidebar, 0);
        if (sidebar->style.has_width && !sidebar->style.width_is_pct) sidebar_w = sidebar->style.width;
        /* REAL PORT 2026-08-16, Stage 3 (khtpm-merge-how2.md §5.3 step
         * 4, 2nd real live use of css_layout_pass()) - real §5.1b
         * pattern #1 (column stack of fixed-height rows), this time
         * using the real padding/gap support added right after the
         * tabbar port found it missing (khtpm_render_core.c's own
         * css_layout_pass() - see that file's header comment). Original
         * geometry was a REAL, uniform 4px inset on all 4 sides
         * (x+4/w-8 horizontally, y start +4 vertically) and ZERO real
         * gap between items (iy += item->h, no addition) - both map
         * cleanly onto the engine's own real padding/gap fields with
         * NO hand-adjustment needed this time (unlike the tabbar's own
         * asymmetric 4px-left-only/2px-both-cross values, which still
         * don't map to a single uniform padding number). */
        sidebar->style.has_display = 1; sidebar->style.display_flex = 1;
        sidebar->style.has_flex_direction = 1; sidebar->style.flex_row = 0;
        sidebar->style.has_padding = 1; sidebar->style.padding = scaled(4);
        int item_h = scaled(22);
        for (int i = 0; i < sidebar->n_children; i++) {
            Elem *item = sidebar->children[i];
            apply_css(item, 0);
            item->style.has_height = 1; item->style.height = item_h;
        }
        css_layout_pass(sidebar, 0, content_y, sidebar_w, content_h);
    }

    if (panel) {
        apply_css(panel, 0);
        int margin = scaled(8);
        panel->x = sidebar_w + margin;
        panel->y = content_y + margin;
        panel->w = window->w - sidebar_w - margin * 2;
        panel->h = content_h - margin * 2;
        /* REAL PORT 2026-08-16, Stage 3 (khtpm-merge-how2.md §5.3 step
         * 4, 3rd/last real live use of css_layout_pass() for db-hq -
         * completes step 4). Real §5.1b pattern #1 (column stack) +
         * pattern #5 (floating title, position:absolute) together in
         * one real container, first time both are exercised live at
         * once. The title's own real position:absolute comes from
         * dashboard.css's real `.block-title` rule (confirmed applied
         * via the real class="block-title" attribute in dashboard.
         * chtpm) - apply_css(c, 0) below populates it correctly, the
         * engine's own real position:absolute detection just works,
         * no special-casing needed (unlike the ORIGINAL code's own
         * `strcmp(c->tag, "title")` special case, now removed).
         * Real, live-tested finding (the open question flagged in this
         * doc's own khtpm-merge-how2.md §5.3 step 4 writeup): the
         * original's extra 16px top clearance (beyond the real 12px
         * uniform padding used everywhere else) turned out to be
         * UNNECESSARY - the title is genuinely out-of-flow via the
         * engine's own real position:absolute handling, so a normal
         * uniform 12px padding (matching the horizontal inset exactly)
         * is enough; confirmed via a real live PNG dump before
         * finalizing this, not assumed. */
        panel->style.has_display = 1; panel->style.display_flex = 1;
        panel->style.has_flex_direction = 1; panel->style.flex_row = 0;
        panel->style.has_padding = 1; panel->style.padding = scaled(12);
        panel->style.has_gap = 1; panel->style.gap = scaled(6);
        for (int i = 0; i < panel->n_children; i++) {
            Elem *c = panel->children[i];
            /* REAL FIX 2026-08-24 (palettes emoji matrix): style the whole
             * subtree, not just direct children - a <row>'s <button> tiles
             * carry their own class rules (.pal-tile width/height/bg);
             * without this they styled to zeros and drew as invisible
             * zero-width elements. Harmless for db-hq (deeper descendants
             * of panel children don't exist there). */
            apply_css_deep(c, 0);
            if (strcmp(c->tag, "title") == 0) {
                /* real pre-measured natural size, per the engine's own
                 * contract for position:absolute children (they use
                 * their own pre-set w/h, never CSS width/height). */
                c->w = measure_text_px(&c->style, c->label) + scaled(10);
                c->h = scaled(14);
                continue;
            }
            /* REAL FIX 2026-08-24: only default the height when CSS didn't
             * set one - .pal-grid-row's real 56px rule was being silently
             * overridden here (rows collapsed to 22px, sprites clipped). */
            if (!c->style.has_height) { c->style.has_height = 1; c->style.height = scaled(22); }
        }
        css_layout_pass(panel, panel->x, panel->y, panel->w, panel->h);

        /* palettes matrix scroll post-pass (see the globals' own block
         * comment): rows were laid out stacked by the engine; shift them
         * by the scroll offset, zero out any row not FULLY inside the
         * panel's padding box (full-row steps - no clipping engine yet,
         * so partially visible rows are simply not drawn rather than
         * bleeding over the hint/chrome), and compute the thumb. */
        g_pal_has_grid = 0;
        g_pal_total_rows = 0;
        Elem *grid_rows[MAX_CHILDREN];
        for (int i = 0; i < panel->n_children && i < MAX_CHILDREN; i++) {
            if (elem_has_class(panel->children[i], "pal-grid-row"))
                grid_rows[g_pal_total_rows++] = panel->children[i];
        }
        if (g_pal_total_rows > 0) {
            g_pal_has_grid = 1;
            int pad12 = scaled(12);
            int top = panel->y + pad12;
            int bot = panel->y + panel->h - pad12;
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
                r->y -= g_pal_scroll * pitch;
                if (r->y < top || r->y + r->h > bot) { r->w = 0; r->h = 0; }
            }
            /* real drawn thumb: proportional to scroll position */
            g_pal_track_w = scaled(8);
            g_pal_track_x = panel->x + panel->w - g_pal_track_w - scaled(2);
            g_pal_track_y = top;
            g_pal_track_h = bot - top;
            if (max_scroll == 0) {
                g_pal_thumb_y = g_pal_track_y; g_pal_thumb_h = g_pal_track_h;
            } else {
                int th = (g_pal_track_h * g_pal_visible_rows) / g_pal_total_rows;
                if (th < scaled(14)) th = scaled(14);
                int ty = g_pal_track_y +
                    ((g_pal_track_h - th) * g_pal_scroll) / max_scroll;
                g_pal_thumb_y = ty; g_pal_thumb_h = th;
            }
        }
    }
}

/* wraith-alpha-standard index nav (ops/wraith_parser_alpha.c's own
 * digit_accum/do_jump/display_num convention, direct instruction: "wraith
 * alpha should be a huge inspiration for this"): every interactive
 * element gets a sequential 1-based number, assigned in the same order
 * they're laid out (tabs, then - if Common Events is open - sidebar
 * items, then panel buttons). Must run AFTER layout_pass() so it walks
 * exactly what's currently visible (placeholder tabs have no sidebar/
 * panel children to number). */
static void clear_nav_indices(Elem *e) {
    if (!e) return;
    e->nav_index = 0;
    for (int i = 0; i < e->n_children; i++) clear_nav_indices(e->children[i]);
}

static void assign_generic_onclick_nav(Elem *e) {
    if (!e || g_n_nav >= MAX_ELEMS) return;
    /* nav_index == 0 guard is CORRECT again now that
     * clear_nav_indices() zeroes the whole tree at the top of every
     * assign_nav_indices() pass (that clearing IS the real bug fix -
     * before it, stale non-zero values made this guard skip rows
     * forever; without the guard, db-hq's natively-numbered <button>s
     * got double-numbered/duplicated into g_nav[]). */
    if (e->onclick[0] && e->nav_index == 0 && e != g_close_elem &&
        e->w > 0 && e->h > 0) {
        e->nav_index = ++g_n_nav;
        g_nav[g_n_nav - 1] = e;
    }
    for (int i = 0; i < e->n_children && g_n_nav < MAX_ELEMS; i++)
        assign_generic_onclick_nav(e->children[i]);
}

static void assign_nav_indices(Elem *window) {
    g_n_nav = 0;
    /* REAL BUG FIX 2026-08-24 ("event-commands still doesn't have
     * nav"): this pass runs EVERY redraw(), and g_n_nav restarts at 0
     * each time - but elements kept their PREVIOUS frame's nav_index,
     * so db-hq's own unconditional renumbering worked while anything
     * numbered ONLY by the generic pass below was silently skipped on
     * every frame after the first (stale index != 0), leaving it out
     * of g_nav[] entirely - arrows/Enter couldn't reach it and its
     * badge showed a dead number. Fix: zero the whole tree first, then
     * renumber from scratch every pass - same invariant wraith-alpha's
     * own text-grid relayout follows. */
    /* Chrome close control is now LAST, not nav 1 (direct instruction,
     * 2026-08-12: "u can give close button last nav index if that
     * helps") - its "[>N]" badge is deliberately suppressed (see
     * draw_elem()'s own comment, too small a box to fit one), so
     * defaulting focus there at launch left NO visible "[>N]" text
     * anywhere on screen. Content tabs now start at nav 1, matching the
     * taskbar/context menus always showing an obvious ">" on a real row
     * immediately. */
    Elem *tabbar = find_by_tag(window, "tabbar");
    if (tabbar) {
        for (int i = 0; i < tabbar->n_children && g_n_nav < MAX_ELEMS; i++) {
            Elem *tab = tabbar->children[i];
            tab->nav_index = ++g_n_nav;
            g_nav[g_n_nav - 1] = tab;
        }
    }
    if (g_current_tab == COMMON_EVENTS_TAB) {
        Elem *sidebar = find_by_tag(window, "sidebar");
        if (sidebar) {
            for (int i = 0; i < sidebar->n_children && g_n_nav < MAX_ELEMS; i++) {
                Elem *item = sidebar->children[i];
                item->nav_index = ++g_n_nav;
                g_nav[g_n_nav - 1] = item;
            }
        }
        Elem *panel = find_by_tag(window, "panel");
        if (panel) {
            for (int i = 0; i < panel->n_children && g_n_nav < MAX_ELEMS; i++) {
                Elem *c = panel->children[i];
                if (strcmp(c->tag, "button") != 0) { c->nav_index = 0; continue; }
                c->nav_index = ++g_n_nav;
                g_nav[g_n_nav - 1] = c;
            }
        }
    }
    /* 2026-08-24 - GENERIC nav pass: any element carrying its own
     * onClick= becomes a numbered, Up/Down + digit-jump + Enter-
     * activatable row (activate_elem() already dispatches onClick
     * first, and draw_elem()'s badge/focus-ring branches are already
     * generic on nav_index > 0). Same list-navigation UX as open-hai's
     * chat-session rows, zero domain code - first consumer: bookmarks'
     * directory entries. db-hq's own assignments above keep their
     * indices/precedence; these append after, and the chrome close
     * control deliberately stays LAST (its own direct-instruction rule
     * above). */
    assign_generic_onclick_nav(window);
    if (g_n_nav < MAX_ELEMS) {
        g_close_elem->nav_index = ++g_n_nav;
        g_nav[g_n_nav - 1] = g_close_elem;
    }
    if (g_focus_nav < 1) g_focus_nav = 1;
    if (g_focus_nav > g_n_nav) g_focus_nav = g_n_nav > 0 ? g_n_nav : 1;
}

/* ---------- rendering ---------- */

static Display *dpy;
static Window win;
static int screen;
static GC gc;
static Pixmap buf;
static XftDraw *xftdraw_buf;
static Colormap cmap;

static unsigned long alloc_pixel(const char *spec) {
    if (!spec || !spec[0]) return BlackPixel(dpy, screen);
    XColor c;
    if (spec[0] == '#') {
        if (XParseColor(dpy, cmap, spec, &c) && XAllocColor(dpy, cmap, &c)) return c.pixel;
    } else if (XAllocNamedColor(dpy, cmap, spec, &c, &c)) {
        return c.pixel;
    }
    return BlackPixel(dpy, screen);
}

static XftColor xft_color(const char *spec) {
    XftColor xc;
    XRenderColor rc = {0, 0, 0, 0xffff};
    if (spec && spec[0] == '#' && strlen(spec) >= 7) {
        unsigned int r, g, b;
        sscanf(spec + 1, "%02x%02x%02x", &r, &g, &b);
        rc.red = (unsigned short)(r * 257); rc.green = (unsigned short)(g * 257); rc.blue = (unsigned short)(b * 257);
    }
    XftColorAllocValue(dpy, DefaultVisual(dpy, screen), cmap, &rc, &xc);
    return xc;
}

/* Stage 1 khtpm merge fix (khtpm-merge-how2.md §3.2) - same cache pattern
 * as measure_text_px() above, ported from chat_hai_hq_render.c's own
 * font_for() fix. Caller must NOT XftFontClose() the returned font
 * anymore - it's a shared, cached handle now, not an owned one. */
static XftFont *font_for(const CssStyle *st) {
    char spec[128];
    const char *fam = st->has_font_family ? st->font_family : "DejaVu Sans";
    int size = scaled(st->has_font_size ? st->font_size : 12);
    snprintf(spec, sizeof(spec), "%s:pixelsize=%d%s", fam, size, (st->has_font_weight && st->font_weight_bold) ? ":bold" : "");

    static char cached_spec[128] = "";
    static XftFont *cached_font = NULL;
    if (cached_font && strcmp(cached_spec, spec) == 0) return cached_font;
    if (cached_font) XftFontClose(dpy, cached_font);
    XftFont *f = XftFontOpenName(dpy, screen, spec);
    if (!f) f = XftFontOpenName(dpy, screen, "DejaVu Sans:pixelsize=10");
    cached_font = f;
    snprintf(cached_spec, sizeof(cached_spec), "%s", spec);
    return f;
}

/* 2026-08-24 - GENERIC single-line text-input mechanism (direct
 * instruction: "new+ should allow input from <cli-io>"). Any element
 * whose onClick starts with "input:" is an input field instead of an
 * instant action: Enter/digit/click ARMS it ([^] armed state, same
 * glyph convention as every nav row), typed printable chars accumulate
 * in place, BackSpace edits, Enter COMMITS and Escape cancels. Commit
 * APPENDS the line to the file named after "input:" (everything up to
 * an optional '|'), then - if a '|' post-command is present - runs it
 * detached, exactly like exec: actions. Data-driven like every other
 * mechanism here: no domain code in the renderer; the layout file
 * declares both where the line lands and what fires afterwards.
 * First consumer: bookmarks' New+ path entry (replaces zenity).
 * Known v1 limits: ASCII typing only (no IME), no clipboard paste -
 * drag-a-dir or relay injection cover exotic paths for now. */
static Elem *g_input_elem = NULL;
static char g_input_buf[256];

static void input_disarm(void) {
    g_input_elem = NULL;
    g_input_buf[0] = '\0';
}

/* ---------- REAL sprite textures (2026-08-24, palettes emoji matrix) ----
 * Ported from khtpm_strip_parser.c's own tab_sprite()/blit_tab_sprite()
 * (itself ported VERBATIM from tp_taskbar.c ~line 915-1015) - the house's
 * one real emoji->image mechanism ("x11 renders emojis ... we have a
 * specific renderer for emojis"): a sprite.csv texture produced by
 *   emoji_gen_atlas.+x "<glyph>" "<atlas.png>"
 *   emoji_xtract.+x "<atlas.png>" 0 64 "<sprite.csv>"
 * read here as RGBA and blitted with per-pixel alpha over the element's
 * own bg. Missing/unreadable csv = text-only fallback, never a crash
 * (matches the strip's own explicit behavior). Cache is bigger than the
 * strip's KTB_MAX_TABS slots because one palettes window legitimately
 * shows 112+ tiles at once; mtime re-check per lookup kept so in-place
 * regeneration refreshes live, same as the strip's cursword-icon fix. */
#define HQ_SPRITE_PX_MAX 64
typedef struct {
    char path[512];
    unsigned char *rgba; /* res * res * 4 */
    int res;
    time_t mtime;
} HqSprite;
#define HQ_SPRITE_CACHE_N 128
static HqSprite g_hq_sprite_cache[HQ_SPRITE_CACHE_N];

static HqSprite *hq_sprite(const char *dir) {
    if (!dir || !dir[0]) return NULL;
    char pth[512];
    snprintf(pth, sizeof(pth), "%s", dir);
    size_t pl = strlen(pth);
    while (pl > 0 && (pth[pl - 1] == '\n' || pth[pl - 1] == '\r' || pth[pl - 1] == ' ' || pth[pl - 1] == '\t'))
        pth[--pl] = 0;
    if (!pth[0]) return NULL;
    char csv_path[520];
    snprintf(csv_path, sizeof(csv_path), "%s/sprite.csv", pth);
    struct stat st;
    time_t mt = 0;
    if (stat(csv_path, &st) == 0) mt = st.st_mtime;
    for (int i = 0; i < HQ_SPRITE_CACHE_N; i++) {
        if (g_hq_sprite_cache[i].rgba && strcmp(g_hq_sprite_cache[i].path, pth) == 0) {
            if (mt != g_hq_sprite_cache[i].mtime) {
                free(g_hq_sprite_cache[i].rgba);
                memset(&g_hq_sprite_cache[i], 0, sizeof(HqSprite));
                break;
            }
            return &g_hq_sprite_cache[i];
        }
    }
    FILE *f = fopen(csv_path, "r");
    if (!f) return NULL;
    char line[256];
    int res = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "# resolution=", 13) == 0) { res = atoi(line + 13); break; }
    }
    if (res <= 0 || res > 256) { fclose(f); return NULL; }
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
    for (int i = 0; i < HQ_SPRITE_CACHE_N; i++) {
        if (!g_hq_sprite_cache[i].rgba) {
            snprintf(g_hq_sprite_cache[i].path, sizeof(g_hq_sprite_cache[i].path), "%s", pth);
            g_hq_sprite_cache[i].rgba = pixels;
            g_hq_sprite_cache[i].res = res;
            g_hq_sprite_cache[i].mtime = mt;
            return &g_hq_sprite_cache[i];
        }
    }
    free(pixels);
    return NULL;
}

static void hq_blit_sprite(HqSprite *sp, int x0, int y0, int px, unsigned long bg_pixel) {
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
    unsigned char *bufpx = calloc((size_t)px * px, 4);
    if (!bufpx) return;
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
            bufpx[(y * px + x) * 4 + 0] = (unsigned char)(word & 0xff);
            bufpx[(y * px + x) * 4 + 1] = (unsigned char)((word >> 8) & 0xff);
            bufpx[(y * px + x) * 4 + 2] = (unsigned char)((word >> 16) & 0xff);
            bufpx[(y * px + x) * 4 + 3] = (unsigned char)((word >> 24) & 0xff);
        }
    }
    XImage *img = XCreateImage(dpy, vis, depth, ZPixmap, 0, (char *)bufpx, px, px, 32, 0);
    if (img) {
        img->byte_order = LSBFirst; /* buf[] is written LSB-first above */
        XPutImage(dpy, buf, gc, img, 0, 0, x0, y0, px, px);
        XDestroyImage(img);
    } else {
        free(bufpx);
    }
}

static void draw_elem(Elem *e, int hover_id_hash) {
    (void)hover_id_hash;
    if (e->style.has_bg_color) {
        XSetForeground(dpy, gc, alloc_pixel(e->style.bg_color));
        XFillRectangle(dpy, buf, gc, e->x, e->y, e->w, e->h);
    }
    if (e->style.has_border_color) {
        XSetForeground(dpy, gc, alloc_pixel(e->style.border_color));
        int bw = e->style.has_border_width ? e->style.border_width : 1;
        for (int i = 0; i < bw; i++)
            XDrawRectangle(dpy, buf, gc, e->x + i, e->y + i, e->w - 1 - 2 * i, e->h - 1 - 2 * i);
    }
    /* REAL FIX 2026-08-16, direct live report ("the black text in
     * header is no longer visble... fix highlight box as well"): these
     * 2 hardcoded fallback fills are the REAL active-state colors -
     * dashboard.css's own .tab.active/.data-item.active rules (see
     * that file) are DEAD, confirmed live - `active` is a real C
     * struct bool (Elem->active), never added to e->classes[] as an
     * actual matchable ".active" class string, so css_compute_style()
     * can never match those 2 selectors; the CSS-set has_bg_color stays
     * 0 for these elements regardless of what the .css file says, and
     * these 2 fallbacks ALWAYS fire. Was `#ffffff`/`#cce5ff` - real,
     * correct-looking values for the OLD light theme, now wrong for
     * the new dark one (2026-08-16, direct instruction to make db-hq
     * dark) - real fix: dark-theme values matching this file's own new
     * dashboard.css intent (`.tab.active`'s own now-effectively-
     * documentation-only `#2a2a2a`, `.data-item.active`'s own
     * `#2f5f8f`) so the (also-just-fixed) light-gray default text
     * color stays readable against both. Real, separate follow-up not
     * done here (out of scope for this pass): making `.tab.active`/
     * `.data-item.active` actually reachable via CSS again would need
     * pushing a real "active" string into e->classes[] whenever
     * Elem->active is set, or teaching css_compute_style() to check
     * the bool field directly - either is a real, larger change. */
    if (strcmp(e->tag, "tab") == 0 && e->active && !e->style.has_bg_color) {
        XSetForeground(dpy, gc, alloc_pixel("#2a2a2a"));
        XFillRectangle(dpy, buf, gc, e->x, e->y, e->w, e->h);
    }
    if (strcmp(e->tag, "item") == 0 && e->active && !e->style.has_bg_color) {
        XSetForeground(dpy, gc, alloc_pixel("#2f5f8f"));
        XFillRectangle(dpy, buf, gc, e->x, e->y, e->w, e->h);
    }
    /* wraith-alpha-standard focus ring: the currently-focused navigable
     * element gets a highlighted outline, matching wraith_parser_alpha.c's
     * "[>]" focus prefix convention (adapted to a visible box here since
     * this is a graphical renderer, not the text-grid wraith-alpha draws
     * into). */
    if (e->nav_index > 0 && e->nav_index == g_focus_nav) {
        XSetForeground(dpy, gc, alloc_pixel("#ff8c00"));
        XDrawRectangle(dpy, buf, gc, e->x - 1, e->y - 1, e->w + 1, e->h + 1);
    }
    int pad = e->style.has_padding ? e->style.padding : 4;
    int label_x = e->x + pad;
    /* nav-index badge geometry, computed here (BEFORE the sprite/label
     * blocks below) so the label text still reserves room for it, even
     * though the badge itself is now drawn LAST (see the actual draw
     * call + full rationale near the end of this function, REAL FIX
     * 2026-08-25). Sprite tiles ignore this reservation entirely (the
     * image centers in the whole box, not relative to label_x) - only
     * the label-text path below needs `badge_label_x`. */
    char nav_badge[16] = "";
    XftFont *nav_badge_font = NULL;
    XGlyphInfo nav_badge_ext = {0};
    int badge_label_x = label_x;
    if (e->nav_index > 0) {
        int focused = (e->nav_index == g_focus_nav);
        snprintf(nav_badge, sizeof(nav_badge), "[%c]%d.", focused ? '>' : ' ', e->nav_index);
        char numspec[48];
        snprintf(numspec, sizeof(numspec), "DejaVu Sans Mono:pixelsize=%d", scaled(9));
        nav_badge_font = XftFontOpenName(dpy, screen, numspec);
        if (!nav_badge_font) { snprintf(numspec, sizeof(numspec), "DejaVu Sans:pixelsize=%d", scaled(9)); nav_badge_font = XftFontOpenName(dpy, screen, numspec); }
        if (nav_badge_font) {
            XftTextExtentsUtf8(dpy, nav_badge_font, (const FcChar8 *)nav_badge, (int)strlen(nav_badge), &nav_badge_ext);
            badge_label_x = label_x + nav_badge_ext.width + 5;
        }
    }
    /* REAL, NEW 2026-08-24 - palettes emoji matrix: an element carrying a
     * real sprite= texture draws the image INSTEAD of its own label text
     * (same convention as the strip's header cells: "sprite at x+4, text
     * shifts" there becomes "image fills the tile, no text" here - these
     * tiles are square icon buttons, not wide cells). Missing/unreadable
     * csv falls through to the normal text path below, never blank.
     * Blit size: largest centered square that fits inside the element's
     * padded content box, capped at HQ_SPRITE_PX_MAX.
     *
     * REAL FIX 2026-08-25 (direct live report: "i dont see the nav on
     * the emojis"): this used to `return` right after the blit, but the
     * nav-index badge below used to draw BEFORE this block and get
     * immediately painted over by the sprite image occupying nearly the
     * same top-left corner - the badge was real and correct in the
     * receipt/nav data the whole time, just visually erased every frame.
     * Real fix: sprite draws first, badge draws LAST (see the badge
     * block's own new position below) so nothing can ever paint over it
     * again, for any element type - not just emoji tiles. */
    int drew_sprite = 0;
    if (e->sprite[0]) {
        HqSprite *sp = hq_sprite(e->sprite);
        if (sp) {
            int pad_s = e->style.has_padding ? e->style.padding : 4;
            int box_w = e->w - 2 * pad_s, box_h = e->h - 2 * pad_s;
            int px = box_w < box_h ? box_w : box_h;
            if (px > HQ_SPRITE_PX_MAX) px = HQ_SPRITE_PX_MAX;
            if (px > 0) {
                unsigned long bg_pixel = e->style.has_bg_color
                    ? alloc_pixel(e->style.bg_color)
                    : WhitePixel(dpy, screen);
                hq_blit_sprite(sp, e->x + (e->w - px) / 2, e->y + (e->h - px) / 2,
                               px, bg_pixel);
                drew_sprite = 1; /* image replaces text entirely - badge still comes after */
            }
        }
    }
    if (!drew_sprite && (e->label[0] || e == g_input_elem)) {
        /* Armed input field renders its live buffer (with caret) in
         * place of its own label - the visible "[^]-armed typing"
         * state, same convention as every nav row's glyph. */
        const char *draw_label = e->label;
        char ibuf[sizeof(g_input_buf) + 8];
        if (e == g_input_elem) {
            snprintf(ibuf, sizeof(ibuf), "> %s _", g_input_buf);
            draw_label = ibuf;
        }
        XftFont *font = font_for(&e->style);
        /* REAL FIX 2026-08-16, direct live report ("the black text in
         * header is no longer visble"): default text-color fallback
         * for any element with no real explicit CSS color: rule. This
         * CSS engine has no real cascade/inheritance (a documented,
         * deliberate minimal-subset scope, not a bug) - an element
         * without its OWN matching selector/color falls straight to
         * this hardcoded default, not to a parent's color. Was
         * `#000000` - correct for the OLD light theme, invisible
         * against the new dark one (2026-08-16, direct instruction to
         * make db-hq dark). Real fix: light gray, matching the same
         * default text color this house's other dark khtpm apps
         * already use (open-hai/chat-hai/entity-menu all read fine at
         * this exact value against their own #141414-family
         * backgrounds). */
        XftColor col = xft_color(e->style.has_fg_color ? e->style.fg_color : "#cccccc");
        XGlyphInfo extents;
        XftTextExtentsUtf8(dpy, font, (const FcChar8 *)draw_label, (int)strlen(draw_label), &extents);
        int ty = e->y + (e->h + font->ascent - font->descent) / 2;
        if (ty < e->y + font->ascent) ty = e->y + font->ascent + pad / 2;
        XftDrawStringUtf8(xftdraw_buf, &col, font, badge_label_x, ty, (const FcChar8 *)draw_label, (int)strlen(draw_label));
        XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &col);
        /* font_for() now returns a cached, shared handle - do not close it. */
    }
    /* nav-index badge: bracket-wrapped, moving ">" focus marker - matches
     * the taskbar/toolbar's own convention (khtpm_taskbar_manager.c's
     * hq_focus highlight; wraith_parser_alpha.c's "[>]"/"[ ]" prefix
     * this whole nav system was ported from). "[>3]" when focused,
     * "[ 3]" otherwise, in its own small muted font so it reads as a
     * toolbar index badge, not run into the label's own text. */
    /* Direct correction 2026-08-12 ("x close isn't getting a number...
     * everything gets a number") - the close button used to be
     * special-cased out of the badge (its box was too small and the
     * badge pushed the label off-screen, see the earlier "off screen to
     * the right" fix). Real fix is a wider box (g_close_w, see
     * layout_pass()) and a shorter label ("x" not "[x]", since the
     * badge itself now supplies the brackets) instead of an exception -
     * every nav item gets a number, no special cases.
     *
     * REAL FIX 2026-08-25 (direct live report: "i dont see the nav on
     * the emojis" - "maybe u should draw them last?"): the actual draw
     * moved here, to run AFTER both the sprite blit and the label text
     * instead of before them - it used to draw first and get silently
     * painted over by whichever of those two ran next (confirmed: nav
     * data itself was always correct, only the visible badge was being
     * erased). Drawing last means nothing painted by this function can
     * ever cover it again, for any element type. Geometry (nav_badge/
     * nav_badge_font/nav_badge_ext) was already computed further up
     * (before the sprite/label blocks) so the label-text path could
     * still reserve room for it via badge_label_x - only the actual
     * draw + cleanup happens here now, reusing those same values rather
     * than recomputing them a second time. */
    if (e->nav_index > 0 && nav_badge_font) {
        int focused = (e->nav_index == g_focus_nav);
        int numy = e->y + (e->h + nav_badge_font->ascent - nav_badge_font->descent) / 2;
        /* REAL FIX 2026-08-25, direct live follow-up ("the number are
         * still appearing over the emoji, instead of above the tile,
         * where there is whitespace"): CORRECTION to this comment's own
         * earlier claim (same day) that rows were packed with ~zero
         * gap - that was wrong, based on a bad reading; the real receipt
         * data shows consecutive palette rows are 64px apart while tiles
         * are only 48px tall, i.e. a genuine 16px gap exists above every
         * tile. Badge now draws INTO that real gap for sprite-bearing
         * elements specifically (baseline positioned so its full glyph
         * box, descent included, sits above e->y with a 2px margin) -
         * confirmed to fit (9px badge font is well under 16px). Solid
         * backing chip kept for contrast against the panel's own
         * background regardless of theme. */
        if (e->sprite[0]) {
            int chip_pad = 1;
            int gap_margin = 2;
            int numy_above = e->y - gap_margin - nav_badge_font->descent;
            int chip_x0 = e->x - chip_pad;
            int chip_y0 = numy_above - nav_badge_font->ascent - chip_pad;
            int chip_w = nav_badge_ext.width + 2 * chip_pad;
            int chip_h = nav_badge_font->ascent + nav_badge_font->descent + 2 * chip_pad;
            numy = numy_above;
            label_x = e->x;
            XSetForeground(dpy, gc, alloc_pixel("#141414"));
            XFillRectangle(dpy, buf, gc, chip_x0, chip_y0, (unsigned)chip_w, (unsigned)chip_h);
        }
        XftColor numcol = xft_color(focused ? "#ff8c00" : "#cccccc");
        XftDrawStringUtf8(xftdraw_buf, &numcol, nav_badge_font, label_x, numy, (const FcChar8 *)nav_badge, (int)strlen(nav_badge));
        XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &numcol);
        XftFontClose(dpy, nav_badge_font);
    }
}

/* absolute-positioned children (the floating block-title) are painted in
 * a later pass than their parent, per the design doc's own suggested
 * approach - this walk draws non-title children first, titles last. */
static void render_tree(Elem *e, int depth) {
    if (depth == 0) draw_elem(e, 0);
    for (int i = 0; i < e->n_children; i++) {
        Elem *c = e->children[i];
        if (strcmp(c->tag, "title") == 0) continue; /* deferred */
        /* <module> (Stage 2d, 2026-08-16) is pure config, never visual -
         * e->label holds its src path (repurposed field, see apply_attr()'s
         * own comment), not something to draw. */
        if (strcmp(c->tag, "module") == 0) continue;
        draw_elem(c, 0);
        render_tree(c, depth + 1);
    }
    for (int i = 0; i < e->n_children; i++) {
        Elem *c = e->children[i];
        if (strcmp(c->tag, "title") == 0) draw_elem(c, 0);
    }
}

static void render_placeholder_tab(Elem *window) {
    char pspec[48];
    snprintf(pspec, sizeof(pspec), "DejaVu Sans:pixelsize=%d", scaled(12));
    XftFont *font = XftFontOpenName(dpy, screen, pspec);
    XftColor col = xft_color("#888888");
    char msg[64];
    snprintf(msg, sizeof(msg), "%s — (coming soon)", TAB_LABELS[g_current_tab]);
    XGlyphInfo extents;
    XftTextExtentsUtf8(dpy, font, (const FcChar8 *)msg, (int)strlen(msg), &extents);
    int tx = (window->w - extents.width) / 2;
    int ty = window->h / 2;
    XftDrawStringUtf8(xftdraw_buf, &col, font, tx, ty, (const FcChar8 *)msg, (int)strlen(msg));
    XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &col);
    XftFontClose(dpy, font);
}

/* Real, documented bug class (!.HOUSE_STDS.md F-19): under this house's
 * Mutter/XWayland environment, a brand-new override_redirect window does
 * NOT reliably receive real keyboard input on bare mapping alone -
 * XGetInputFocus can report success while KeyPress events never arrive.
 * This is almost certainly why arrows/digit-jump looked broken (direct
 * report: "doesn't have > focus arrow move or digit jump yet") despite
 * handle_key()'s own logic being correct and already proven working
 * through the relay (which bypasses X input focus entirely, so it never
 * hit this). Fix is the SAME proven raise-then-focus-then-flush sequence
 * already used by khtpm_strip_parser.c's taskbar_soft_focus() - ported,
 * not reinvented, per that bug report's own explicit standard ("don't
 * invent a fresh focus mechanism without first checking whether an
 * already-proven pattern solves it").
 *
 * DIAGNOSTIC (also ported, khtpm_strip_parser.c's own g_has_real_focus):
 * XSetInputFocus() is a REQUEST, not a guarantee - this tracks whether
 * the window ACTUALLY has focus right now via real FocusIn/FocusOut
 * events, the only authoritative source. If this never goes true despite
 * soft_focus() being called, KeyPress events genuinely never reach this
 * process - a different, deeper problem than db-hq's own key-handling
 * logic (which is separately already proven correct via the relay). */
static int g_has_real_focus = 0;

static void soft_focus(void) {
    XRaiseWindow(dpy, win);
    XSetInputFocus(dpy, win, RevertToParent, CurrentTime);
    XFlush(dpy);
}

/* Real fix (found live, 2026-08-12): a grab taken ONCE at startup isn't
 * enough for a long-lived window - a fresh FocusIn immediately followed
 * by FocusOut appeared after a genuine physical click, meaning the grab
 * had already been lost/preempted sometime after launch with nothing to
 * recover it. tp_desktop_window.c's popups never hit this because
 * they're short-lived and re-created (thus re-grabbed) fresh every time
 * one opens - db-hq is one persistent window across its whole session,
 * so it must re-request the grab on every interaction instead, not just
 * once. Keyboard-only (see the call site's own note on why not
 * XGrabPointer too). */
static void grab_keyboard_retry(void) {
    for (int attempt = 0; attempt < 5; attempt++) {
        int rc = XGrabKeyboard(dpy, win, True, GrabModeAsync, GrabModeAsync, CurrentTime);
        if (rc == GrabSuccess) break;
        XSync(dpy, False);
        usleep(5000);
    }
}

static Elem *g_window;

/* RGB compose→present refactor (2026-08-12, direct instruction: "we
 * should do db to rgb refactor. the need being auditability"). `redraw()`
 * composes into `buf` (the offscreen Pixmap) via Xft/Xlib as before, then
 * presents via one real `XImage` (XGetImage->XPutImage) instead of the
 * old direct `XCopyArea` blit.
 *
 * Stage 1 khtpm merge fix (khtpm-merge-how2.md §3.1, 2026-08-15): the
 * persistent g_frame_rgb byte-buffer this comment used to describe is
 * gone - it forced an unconditional per-pixel unpack on every redraw()
 * just to keep a debug dump "always fresh," the same hot-path-does-
 * cold-path-work bug chat-hai had before this session's fix. Ported
 * open-hai's actual proven shape instead: dump_frame_png() does its own
 * on-demand XGetImage capture only when needed. */

/* RECEIPT PORT (2026-08-24, au11-hq/house-compaction.md Part 1 + the
 * approved plan at that time): dump_frame_png() used to write ONLY a
 * PNG - no receipt, no way to headlessly audit WHAT was rendered
 * (element positions/labels/nav/focus) without eyeballing pixels. Real
 * standard this ports from: 1.TPMOS_c_+rmmp.0103.0001/projects/
 * wraith-alpha/plugins/wraith_rgb_daemon.c's write_rgb_receipt() -
 * read in full before writing this. Deliberately SMALLER than wraith's
 * ~40-field-per-object receipt (no ancestor_chain/parent_id strings -
 * Elem has a real `parent` pointer, not a string id, and nothing here
 * consumes a chain string yet; no mouse-position fields - no live
 * mouse-tracking global exists in this file to source them from). Both
 * omissions are confirmed gaps vs the real reference, not oversights -
 * see house-compaction.md Part 2 for the "don't over-build past what
 * this house's testing needs" reasoning. Scoped to khtpm_hq_render.c
 * ONLY this pass (direct instruction, 2026-08-24) - events-hq/chat-hai
 * have the identical gap in their own dump_frame_png() copies, not
 * touched here.
 *
 * checksum_buffer() is a verbatim, unmodified port of wraith_rgb_
 * daemon.c's own function of the same name (plain FNV-1a 64, pure -
 * confirmed zero dependency on wraith's semantic-object machinery) -
 * kept LOCAL here, not promoted to khtpm_render_core.c, because
 * promoting it would imply events-hq/chat-hai are ready to consume it
 * too, which isn't true yet - move it when their own follow-up starts. */
static unsigned long long checksum_buffer(const unsigned char *buffer, size_t len) {
    unsigned long long hash = 1469598103934665603ULL;
    for (size_t i = 0; i < len; i++) {
        hash ^= (unsigned long long)buffer[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

/* Pre-order count, matching emit_hq_object()'s own traversal exactly -
 * gives write_hq_receipt() a real object_count to print in the header
 * BEFORE the OBJECT rows, without needing a two-buffer/rewind trick. */
static int count_hq_elems(Elem *e) {
    if (!e) return 0;
    int n = 1;
    for (int i = 0; i < e->n_children; i++) n += count_hq_elems(e->children[i]);
    return n;
}

/* Same recursion shape as clear_nav_indices()/assign_generic_onclick_nav()
 * above - deliberately reused, not reinvented. One line per Elem, field
 * set chosen from what's ALREADY live in the struct (see the big comment
 * above this block for what's real vs deferred). focused= matches how
 * draw_elem() itself determines focus (nav_index == g_focus_nav, see
 * that function's own check) - this is the real, same-process
 * equivalent of wraith's focused_object_id, not a guess. */
static void emit_hq_object(FILE *f, Elem *e, int *counter) {
    if (!e) return;
    (*counter)++;
    fprintf(f,
        "OBJECT | %04d | tag=%s label=%s action=%s nav=%d focused=%d x=%d y=%d w=%d h=%d fg=%s bg=%s border=%s sprite=%s active=%d\n",
        *counter,
        e->tag[0] ? e->tag : "-",
        e->label[0] ? e->label : "-",
        e->onclick[0] ? e->onclick : "-",
        e->nav_index,
        (e->nav_index > 0 && e->nav_index == g_focus_nav) ? 1 : 0,
        e->x, e->y, e->w, e->h,
        e->style.has_fg_color ? e->style.fg_color : "-",
        e->style.has_bg_color ? e->style.bg_color : "-",
        e->style.has_border_color ? e->style.border_color : "-",
        e->sprite[0] ? e->sprite : "-",
        e->active);
    for (int i = 0; i < e->n_children; i++) emit_hq_object(f, e->children[i], counter);
}

static void write_hq_receipt(const char *receipt_path, unsigned long long checksum, int w, int h) {
    FILE *f = fopen(receipt_path, "w");
    if (!f) { fprintf(stderr, "db-hq: write_hq_receipt: fopen failed %s\n", receipt_path); return; }
    time_t now = time(NULL);
    struct tm tm_utc;
    char iso_time[32] = "";
    if (gmtime_r(&now, &tm_utc) != NULL) {
        strftime(iso_time, sizeof(iso_time), "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
    }
    fprintf(f, "receipt_type=hq_render_audit\n");
    fprintf(f, "generated_by=khtpm_hq_render\n");
    fprintf(f, "generated_at_epoch=%ld\n", (long)now);
    fprintf(f, "generated_at_iso_utc=%s\n", iso_time[0] ? iso_time : "unknown");
    fprintf(f, "source_layout=%s\n", g_chtpm_path[0] ? g_chtpm_path : "unknown");
    fprintf(f, "viewport_width_px=%d\n", w);
    fprintf(f, "viewport_height_px=%d\n", h);
    fprintf(f, "render_checksum_fnv1a64=0x%016llX\n", checksum);
    fprintf(f, "object_count=%d\n", count_hq_elems(g_window));
    fprintf(f, "focused_nav_index=%d\n", g_focus_nav);
    fprintf(f, "SECTION | OBJECTS | HQ_ELEM_TREE\n");
    int counter = 0;
    emit_hq_object(f, g_window, &counter);
    fclose(f);
}

/* Derives a per-instance basename from g_chtpm_path (e.g. "palettes-
 * emojis" from ".../palettes-emojis.chtpm") - same "derive identity
 * from the loaded .chtpm name" convention this week's palettes work
 * already established for per-key CSS (palettes-$key.css). Fixes a
 * real, separate, pre-existing bug bundled into this same pass: the OLD
 * dump path was hardcoded to "db-hq-frame.png" for every app mode this
 * shared binary can run (db-hq/palettes/events-hq/chat-hai/entity-menu
 * all select via class=), so any two instances dumping at once clobbered
 * each other's PNG. */
static void hq_receipt_basename(char *out, size_t outsz) {
    const char *base = strrchr(g_chtpm_path, '/');
    base = base ? base + 1 : g_chtpm_path;
    if (!base[0]) base = "hq-render";
    snprintf(out, outsz, "%s", base);
    char *dot = strrchr(out, '.');
    if (dot && strcmp(dot, ".chtpm") == 0) *dot = '\0';
}

/* debug PNG dump - see the header comment above the stb_image_write.h
 * include. Does its own on-demand capture now (Stage 1 fix), not a
 * `g_frame_rgb` buffer redraw() already derived for the real on-screen
 * present - no separate XGetImage capture of its own anymore. This IS
 * the auditability point of the refactor: what gets dumped is
 * byte-for-byte the same buffer that was actually presented, not a
 * fresh, possibly-different second capture. Bound to 'p' - not part of
 * the normal render loop, purely an on-demand debug aid. */
/* Stage 1 khtpm merge fix (khtpm-merge-how2.md §3.1): the per-pixel
 * XGetPixel unpack used to run unconditionally, every redraw(), just to
 * keep g_frame_rgb "fresh" for this on-demand debug dump - same
 * hot-path-does-cold-path-work bug chat-hai had. Ported open-hai's actual
 * shape (khtpm_open_hai_render.c's dump_frame_png(), verified as the
 * ground truth 2026-08-15): the unpack now happens ONLY here, via its own
 * XGetImage capture of `buf`, only when 'p' is actually pressed. */
static void dump_frame_png(void) {
    if (!g_window) { fprintf(stderr, "db-hq: dump_frame_png: no frame composed yet\n"); return; }
    int w = g_window->w, h = g_window->h;
    XImage *img = XGetImage(dpy, buf, 0, 0, (unsigned)w, (unsigned)h, AllPlanes, ZPixmap);
    if (!img) { fprintf(stderr, "db-hq: dump_frame_png: XGetImage failed\n"); return; }
    unsigned char *rgb = malloc((size_t)w * h * 3);
    if (!rgb) { XDestroyImage(img); return; }
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            unsigned long px = XGetPixel(img, x, y);
            size_t o = ((size_t)y * w + x) * 3;
            rgb[o] = (unsigned char)((px >> 16) & 0xff);
            rgb[o + 1] = (unsigned char)((px >> 8) & 0xff);
            rgb[o + 2] = (unsigned char)(px & 0xff);
        }
    }
    XDestroyImage(img);
    char base[PATH_BUF];
    hq_receipt_basename(base, sizeof(base));
    char dump_path[PATH_BUF];
    char receipt_path[PATH_BUF];
#ifdef _WIN32
    snprintf(dump_path, sizeof(dump_path), "%s/#.desktop/%s-frame.png", g_house_root[0] ? g_house_root : ".", base);
    snprintf(receipt_path, sizeof(receipt_path), "%s/#.desktop/%s-frame.receipt.pdl", g_house_root[0] ? g_house_root : ".", base);
#else
    snprintf(dump_path, sizeof(dump_path), "/tmp/%s-frame.png", base);
    snprintf(receipt_path, sizeof(receipt_path), "/tmp/%s-frame.receipt.pdl", base);
#endif
    int ok = stbi_write_png(dump_path, w, h, 3, rgb, w * 3);
    fprintf(stderr, ok ? "db-hq: wrote %s (%dx%d)\n" : "db-hq: dump_frame_png: write failed %s\n", dump_path, w, h);
    unsigned long long checksum = checksum_buffer(rgb, (size_t)w * h * 3);
    write_hq_receipt(receipt_path, checksum, w, h);
    fprintf(stderr, "db-hq: wrote %s\n", receipt_path);
    free(rgb);
}

/* Own chrome bar (title + close) - see layout_pass()'s CHROME_H comment
 * for the wraith-alpha precedent. Drawn unconditionally, every redraw,
 * regardless of which tab is open - matches wraith-alpha's own chrome
 * row staying fixed while body content underneath changes. */
static void draw_chrome_bar(void) {
    XSetForeground(dpy, gc, alloc_pixel("#2b2b2b"));
    XFillRectangle(dpy, buf, gc, 0, 0, g_window->w, g_chrome_h);

    char tspec[48];
    snprintf(tspec, sizeof(tspec), "DejaVu Sans:pixelsize=%d:bold", scaled(10));
    XftFont *titlefont = XftFontOpenName(dpy, screen, tspec);
    if (!titlefont) { snprintf(tspec, sizeof(tspec), "DejaVu Sans:pixelsize=%d", scaled(10)); titlefont = XftFontOpenName(dpy, screen, tspec); }
    XftColor titlecol = xft_color("#eeeeee");
    /* legacy taskbar's own "^" convention (direct instruction 2026-08-12:
     * "legacy toolbar had a '^' indicator near digits, i noticed we lost
     * that but we could add it here" / "'^' indicating window had
     * focus") - real, ground-truth g_has_real_focus (set only by an
     * actual FocusIn event, "the only authoritative source" per
     * khtpm_strip_parser.c's own F-19 diagnostic this was ported from),
     * not a guess or a request-was-sent flag. */
    char title[16];
    snprintf(title, sizeof(title), "db-hq %s", g_has_real_focus ? "^" : " ");
    int ty = (g_chrome_h + titlefont->ascent - titlefont->descent) / 2;
    XftDrawStringUtf8(xftdraw_buf, &titlecol, titlefont, scaled(8), ty, (const FcChar8 *)title, (int)strlen(title));
    XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &titlecol);
    XftFontClose(dpy, titlefont);

    g_close_elem->x = g_close_x; g_close_elem->y = g_close_y;
    g_close_elem->w = g_close_w; g_close_elem->h = g_close_h;
    snprintf(g_close_elem->label, sizeof(g_close_elem->label), "x");
    css_style_init(&g_close_elem->style);
    g_close_elem->style.has_border_color = 1;
    snprintf(g_close_elem->style.border_color, sizeof(g_close_elem->style.border_color), "%s",
             g_close_elem->nav_index == g_focus_nav ? "#ff8c00" : "#888888");
    g_close_elem->style.has_border_width = 1; g_close_elem->style.border_width = 1;
    g_close_elem->style.has_fg_color = 1;
    snprintf(g_close_elem->style.fg_color, sizeof(g_close_elem->style.fg_color), "#eeeeee");
    draw_elem(g_close_elem, 0);

    /* Debug status line, direct request 2026-08-12 ("we could show
     * digits in header like tb") - shows the last raw key this PROCESS
     * actually received and the current digit accumulator, live, so
     * it's visually obvious (not just in a log file) whether a real
     * keypress ever reaches this window at all vs. reaches it but
     * doesn't visibly move focus for some other reason - two very
     * different bugs that look identical from the outside otherwise. */
    char dbg[96];
    snprintf(dbg, sizeof(dbg), "Key:%s  Digits:%d  Focus:%d/%d  RealFocus:%s",
             g_last_key_label[0] ? g_last_key_label : "(none yet)",
             g_digit_accum, g_focus_nav, g_n_nav, g_has_real_focus ? "yes" : "no");
    char dspec[48];
    snprintf(dspec, sizeof(dspec), "DejaVu Sans:pixelsize=%d", scaled(9));
    XftFont *dfont = XftFontOpenName(dpy, screen, dspec);
    if (dfont) {
        XftColor dcol = xft_color("#88cc88");
        XGlyphInfo dext;
        XftTextExtentsUtf8(dpy, dfont, (const FcChar8 *)dbg, (int)strlen(dbg), &dext);
        int dx = g_window->w - g_close_w - scaled(12) - dext.width;
        int dy = (g_chrome_h + dfont->ascent - dfont->descent) / 2;
        XftDrawStringUtf8(xftdraw_buf, &dcol, dfont, dx, dy, (const FcChar8 *)dbg, (int)strlen(dbg));
        XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &dcol);
        XftFontClose(dpy, dfont);
    }
}

static void redraw(void) {
    layout_pass(g_window);
    assign_nav_indices(g_window);
    XSetForeground(dpy, gc, alloc_pixel(g_window->style.has_bg_color ? g_window->style.bg_color : "#ececec"));
    XFillRectangle(dpy, buf, gc, 0, 0, g_window->w, g_window->h);
    if (g_current_tab != COMMON_EVENTS_TAB) {
        Elem *tabbar = find_by_tag(g_window, "tabbar");
        if (tabbar) { draw_elem(tabbar, 0); render_tree(tabbar, 1); }
        render_placeholder_tab(g_window);
    } else {
        render_tree(g_window, 0);
    }
    /* palettes matrix scroll thumb (pallette-design.txt VIEW SPECS) -
     * drawn only when this window's panel actually carries grid rows */
    if (g_pal_has_grid && g_pal_track_h > 0) {
        XSetForeground(dpy, gc, alloc_pixel("#cccccc"));
        XFillRectangle(dpy, buf, gc, g_pal_track_x, g_pal_track_y, (unsigned)g_pal_track_w, (unsigned)g_pal_track_h);
        XSetForeground(dpy, gc, alloc_pixel("#888888"));
        XFillRectangle(dpy, buf, gc, g_pal_track_x + scaled(1), g_pal_thumb_y,
                       (unsigned)(g_pal_track_w - scaled(2)), (unsigned)g_pal_thumb_h);
    }
    draw_chrome_bar();

    /* COMPOSE→PRESENT split, Stage 1-corrected (khtpm-merge-how2.md §3.1):
     * present via XGetImage->XPutImage (still pixel-identical to the old
     * XCopyArea path, per Phase 0). The per-pixel RGB unpack that used to
     * run here every frame is gone - dump_frame_png() now does its own
     * on-demand capture instead, matching open-hai's proven pattern. */
    XSync(dpy, False);
    int w = g_window->w, h = g_window->h;
    XImage *frame = XGetImage(dpy, buf, 0, 0, (unsigned)w, (unsigned)h, AllPlanes, ZPixmap);
    if (frame) {
        XPutImage(dpy, win, gc, frame, 0, 0, 0, 0, (unsigned)w, (unsigned)h);
        XDestroyImage(frame);
    } else {
        /* fall back to the old direct blit if XGetImage ever fails, so
         * a capture problem degrades to "no audit buffer this frame,"
         * never "no picture at all." */
        XCopyArea(dpy, buf, win, gc, 0, 0, (unsigned)w, (unsigned)h, 0, 0);
    }
    XFlush(dpy);
}

/* ---------- hit testing / click dispatch ---------- */

/* hit_test() now comes from khtpm_render_core.h (Stage 2a, 2026-08-16). */

/* REAL FIX 2026-08-16 (Stage 2d shell/manager split): this used to
 * spawn the events-hq process directly via system() - a real business
 * action, not rendering. The shell now only WRITES a request; the
 * manager binary (khtpm_hq_manager.c) polls #.desktop/db_hq_action.txt
 * and does the actual spawn. */
static char g_action_path[PATH_BUF];

static void open_in_editor(const char *name) {
    FILE *f = fopen(g_action_path, "w");
    if (!f) return;
    fprintf(f, "open:%s\n", name);
    fclose(f);
}

/* ---------- generic data-driven actions + live layout reload ----------
 * REAL, 2026-08-24 (bookmarks-hq, direct user rule: "shouldn't ever be
 * hardcoded" - layouts/behavior targets belong to the .chtpm, never to
 * this renderer's code). Two small generic mechanisms any HQML window
 * can now use, db-hq's own behavior untouched:
 *
 *   1. onClick="open:<path>" / onClick="exec:<shell command>" - honored
 *      for EVERY element carrying the attribute (the shared parser in
 *      khtpm_render_core.c already captured it into Elem.onclick;
 *      db-hq simply never read it before). Double-fork so a slow
 *      xdg-open/script can never stall the render loop; the orphaned
 *      grandchild is init-reaped, so no zombie bookkeeping here and
 *      launch_module()'s own child handling is unaffected.
 *
 *   2. Layout live reload - the .chtpm is mtime-gated each tick (same
 *      cheap pattern as load_common_events()); a regenerated layout
 *      swaps into the running window without a respawn. Deliberately
 *      does NOT relaunch the <module> child on reload. */
/* g_chtpm_path itself is now declared up near g_house_root (2026-08-24
 * receipt-port change) so dump_frame_png() can see it - not redeclared
 * here. */
static time_t g_chtpm_mtime = 0;

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

static void free_tree(Elem *e) {
    if (!e) return;
    for (int i = 0; i < e->n_children; i++) free_tree(e->children[i]);
    free(e);
}

static void reload_if_changed(void) {
    struct stat st;
    if (stat(g_chtpm_path, &st) != 0) return;
    if (st.st_mtime == g_chtpm_mtime) return;
    Elem *nw = parse_chtpm(g_chtpm_path);
    if (!nw) return; /* half-written file mid-regen: keep old tree */
    g_chtpm_mtime = st.st_mtime;

    /* Same post-parse domain glue main() runs, but guarded - generic
     * windows carry no sidebar/module and must not touch it. */
    Elem *sb = find_by_tag(nw, "sidebar");
    if (sb) {
        load_common_events();
        inject_sidebar_items(sb);
        Elem *panel_text = find_by_tag(nw, "text");
        if (panel_text && g_selected_event >= 0)
            snprintf(panel_text->label, sizeof(panel_text->label), "%s", g_events[g_selected_event]);
    }

    Elem *old = g_window;
    g_window = nw;
    free_tree(old);
    g_pal_scroll = 0; /* new content, new scroll - avoids a stale offset past the new max */
    /* The old tree is gone - an in-progress input field pointed into
     * it. Disarm rather than risk typing into freed memory (the user
     * just re-Enters the field; nothing else to do). */
    if (g_input_elem) { input_disarm(); }
    redraw();
}

/* shared dispatch for both mouse clicks and keyboard index-activation
 * (Enter on the focused nav_index) - wraith-alpha's own convention is
 * that a numbered element behaves identically whichever input method
 * reaches it. */
static void activate_elem(Elem *hit) {
    if (!hit) return;
    if (strcmp(hit->tag, "closebtn") == 0) {
        g_quit = 1;
        return;
    }
    /* Generic data-driven actions first (see the mechanisms block above):
     * an element with its own onClick handles itself straight from the
     * layout file; db-hq's domain branches below only run for elements
     * WITHOUT onclick. */
    if (hit->onclick[0]) {
        if (strncmp(hit->onclick, "input:", 6) == 0) {
            /* input: fields ARM instead of running anything now -
             * handle_key()'s armed branch owns every key until Enter/
             * Escape (see the g_input_elem block comment). */
            g_input_elem = hit;
            g_input_buf[0] = '\0';
            redraw();
            return;
        }
        if (strncmp(hit->onclick, "open:", 5) == 0)
            hq_run_detached(1, hit->onclick + 5);
        else if (strncmp(hit->onclick, "exec:", 5) == 0)
            hq_run_detached(0, hit->onclick + 5);
        redraw();
        return;
    }
    if (strcmp(hit->tag, "tab") == 0) {
        for (int i = 0; i < N_TABS; i++) if (strcmp(hit->label, TAB_LABELS[i]) == 0) { g_current_tab = i; break; }
        redraw();
        return;
    }
    if (strcmp(hit->tag, "item") == 0) {
        for (int i = 0; i < g_n_events; i++) if (strcmp(g_events[i], hit->label) == 0) { g_selected_event = i; break; }
        Elem *sidebar = find_by_tag(g_window, "sidebar");
        inject_sidebar_items(sidebar);
        Elem *panel_text = find_by_tag(g_window, "text");
        if (panel_text && g_selected_event >= 0) snprintf(panel_text->label, sizeof(panel_text->label), "%s", g_events[g_selected_event]);
        redraw();
        return;
    }
    if (strcmp(hit->id, "open-editor") == 0) {
        if (g_selected_event >= 0) open_in_editor(g_events[g_selected_event]);
        return;
    }
}

static void handle_click(int px, int py) {
    /* close button lives in the chrome bar, outside window's own tag
     * tree (it's synthetic, not parsed from dashboard.chtpm) - check it
     * before the tree walk. */
    if (px >= g_close_elem->x && px < g_close_elem->x + g_close_elem->w &&
        py >= g_close_elem->y && py < g_close_elem->y + g_close_elem->h) {
        g_focus_nav = g_close_elem->nav_index;
        activate_elem(g_close_elem);
        return;
    }
    Elem *hit = hit_test(g_window, px, py);
    if (!hit) return;
    if (hit->nav_index > 0) g_focus_nav = hit->nav_index;
    activate_elem(hit);
}

/* wraith-alpha-standard digit-accumulation key handling (ports
 * ops/wraith_parser_alpha.c's digit_accum/do_jump/Enter-activates
 * convention): digits move focus live as they're typed (do_jump), Enter
 * activates the focused element, any other key resets the accumulator. */
static void handle_key(KeySym ks, char ch) {
    /* Armed input field owns EVERY key first - digits must type, not
     * jump nav; 'p' must type, not dump a png (see g_input_elem block
     * comment). */
    if (g_input_elem) {
        if (ks == XK_Escape) {
            input_disarm();
            redraw();
            return;
        }
        if (ks == XK_Return || ks == XK_KP_Enter) {
            const char *spec = g_input_elem->onclick + 6;
            char target[PATH_BUF] = "";
            char post[1024] = "";
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
            g_digit_accum = 0;
            if (post[0]) hq_run_detached(0, post);
            redraw();
            return;
        }
        if (ks == XK_BackSpace) {
            size_t L = strlen(g_input_buf);
            if (L > 0) {
                L--;
                while (L > 0 && (g_input_buf[L] & 0xC0) == 0x80) L--; /* utf8-safe chop */
                g_input_buf[L] = '\0';
            }
            redraw();
            return;
        }
        if (ch >= 32 && ch <= 126 && strlen(g_input_buf) < sizeof(g_input_buf) - 2) {
            size_t L = strlen(g_input_buf);
            g_input_buf[L] = ch;
            g_input_buf[L + 1] = '\0';
            redraw();
            return;
        }
        return; /* swallow arrows/digits/etc while armed */
    }
    if (ch == 'p') { dump_frame_png(); return; }
    if (ks == XK_Return || ks == XK_KP_Enter) {
        if (g_digit_accum > 0 && g_digit_accum <= g_n_nav) g_focus_nav = g_digit_accum;
        g_digit_accum = 0;
        if (g_focus_nav >= 1 && g_focus_nav <= g_n_nav) activate_elem(g_nav[g_focus_nav - 1]);
        return;
    }
    if (ks == XK_Escape) {
        if (g_digit_accum > 0) { g_digit_accum = 0; return; }
        g_quit = 1; /* no WM chrome/close button (override_redirect) - Escape closes instead */
        return;
    }
    if (ch >= '0' && ch <= '9') {
        int d = ch - '0';
        int new_val = g_digit_accum * 10 + d;
        if (new_val > 0 && new_val <= g_n_nav) {
            g_digit_accum = new_val;
            g_focus_nav = new_val;
            redraw();
        } else if (d > 0 && d <= g_n_nav) {
            g_digit_accum = d;
            g_focus_nav = d;
            redraw();
        } else {
            g_digit_accum = 0;
        }
        return;
    }
    if (ks == XK_Page_Up || ks == XK_Page_Down) {
        /* palettes matrix paging (pallette-design.txt VIEW SPECS) - one
         * page = visible-1 rows so the top row stays for context, the
         * same "keep a line of context" idea as chat's scroll step */
        if (g_pal_has_grid) {
            int step = g_pal_visible_rows > 1 ? g_pal_visible_rows - 1 : 1;
            g_pal_scroll += (ks == XK_Page_Down) ? step : -step;
            layout_pass(g_window); /* clamp + re-hide happens in-layout */
            assign_nav_indices(g_window);
        }
        g_digit_accum = 0;
        redraw();
        return;
    }
    if (ks == XK_Up || ks == XK_Left) {
        if (g_focus_nav > 1) g_focus_nav--;
        g_digit_accum = 0;
        redraw();
        return;
    }
    if (ks == XK_Down || ks == XK_Right || ks == XK_Tab) {
        if (g_focus_nav < g_n_nav) g_focus_nav++;
        g_digit_accum = 0;
        redraw();
        return;
    }
    g_digit_accum = 0;
}

/* Agent relay (au11-hq/_.0.aigent-testing-k9.txt's documented "third
 * option" for raw-Xlib programs: "give the program its OWN file-relay
 * polling loop, additive alongside its existing XNextEvent() loop"):
 * <house_root>/#.desktop/db_hq_agent_relay.txt, one decimal ASCII code
 * per line (48-57 digits, 13 Enter, 27 Escape, 32-126 other printable) -
 * SAME contract as khtpm_strip_parser.c's poll_agent_relay() (never
 * replay backlog on first poll, resync-not-replay on truncation, leave a
 * partial trailing line for next time), ported line-for-line from that
 * function since it's already the proven, documented shape for this
 * exact problem. Dispatches through the SAME handle_key() the real
 * KeyPress handler uses (see dispatch_key_code()'s own header comment in
 * khtpm_strip_parser.c for why sharing beats duplicating). No XTest, no
 * shared input focus with a real human on the same display. */
static long g_relay_cursor = -1;

static void relay_path(char *out, size_t outsz) {
    snprintf(out, outsz, "%s/#.desktop/db_hq_agent_relay.txt", g_house_root);
}

static void dispatch_relay_code(int code) {
    if (code == 13) handle_key(XK_Return, 0);
    else if (code == 27) handle_key(XK_Escape, 0);
    else if (code == 8) handle_key(XK_BackSpace, 0);
    else if (code >= 32 && code <= 126) handle_key(0, (char)code);
}

static int poll_agent_relay(void) {
    char path[PATH_BUF];
    relay_path(path, sizeof(path));
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
        if (!nl) break; /* partial line at EOF - wait for the rest next poll */
        *nl = '\0';
        long here = ftell(f);
        int code = atoi(line);
        if (code > 0) { dispatch_relay_code(code); n_dispatched++; }
        consumed = here;
    }
    fclose(f);
    g_relay_cursor = consumed;
    return n_dispatched;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <house_root> <chtpm_path>\n", argv[0]);
        return 1;
    }
    snprintf(g_house_root, sizeof(g_house_root), "%s", argv[1]);
    snprintf(g_events_state_path, sizeof(g_events_state_path), "%s/#.desktop/db_hq_common_events.state.txt", g_house_root);
    snprintf(g_action_path, sizeof(g_action_path), "%s/#.desktop/db_hq_action.txt", g_house_root);
    snprintf(g_chtpm_path, sizeof(g_chtpm_path), "%s", argv[2]);
    {
        struct stat st;
        g_chtpm_mtime = (stat(g_chtpm_path, &st) == 0) ? st.st_mtime : 0;
    }
    signal(SIGTERM, handle_term_signal); /* see handle_term_signal()'s own header comment */
    signal(SIGINT, handle_term_signal);

    load_font_scale(); /* #.desktop/hq_ui.pdl's font_scale key - see load_font_scale()'s own header comment */
    g_chrome_h = scaled(26);

    memset(g_close_elem, 0, sizeof(*g_close_elem));
    snprintf(g_close_elem->tag, sizeof(g_close_elem->tag), "closebtn");

    load_common_events();
    if (g_n_events > 0) g_selected_event = 0;

    Elem *window = parse_chtpm(argv[2]);
    if (!window) {
        fprintf(stderr, "db-hq: failed to parse %s\n", argv[2]);
        return 1;
    }
    g_window = window;

    /* REAL module launch (Stage 2d, 2026-08-16) - read the <module
     * src="..."/> tag dashboard.chtpm now declares and fork()+execv()
     * it ourselves, matching wraith_parser_alpha.c's own launch_module()
     * call site exactly (right after the layout is parsed). Replaces
     * open_db_hq.sh's old manual dual-launch of the manager binary. */
    Elem *module_elem = find_by_tag(window, "module");
    if (module_elem && module_elem->label[0]) launch_module(module_elem->label);
    atexit(cleanup_module); /* covers every return path, not just g_quit's normal loop exit */

    Elem *sidebar = find_by_tag(window, "sidebar");
    inject_sidebar_items(sidebar);
    Elem *panel_text = find_by_tag(window, "text");
    if (panel_text && g_selected_event >= 0) snprintf(panel_text->label, sizeof(panel_text->label), "%s", g_events[g_selected_event]);

    char css_path[PATH_BUF];
    snprintf(css_path, sizeof(css_path), "%s", argv[2]);
    char *dot = strrchr(css_path, '.');
    if (dot) snprintf(dot, sizeof(css_path) - (size_t)(dot - css_path), ".css");
    static CssSheet sheet;
    memset(&sheet, 0, sizeof(sheet));
    css_load(css_path, &sheet);
    g_sheet = &sheet;

    dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "db-hq: cannot open display\n"); return 1; }
    screen = DefaultScreen(dpy);
    cmap = DefaultColormap(dpy, screen);

    layout_pass(window);
    int ww = window->w, wh = window->h;

    /* override_redirect, no WM decoration - same convention every khtpm
     * window uses (khtpm_strip_parser.c's win/hq_win/popup_win all set
     * CWOverrideRedirect the same way; see au11-hq/HQML-DESIGN+PLANS.md's
     * "Window Chrome Convention" note). No WM titlebar means no WM close
     * button either - Escape (with no digit pending) closes the window
     * instead, see handle_key(). */
    /* Real architecture fix (2026-08-12, direct instruction "yes do
     * that" after finding: real physical clicks reliably reach this
     * window - ButtonPress works fine - but FocusIn never fires no
     * matter what X11-level focus/grab calls are made). Root cause:
     * this system has org.gnome.mutter focus-change-on-pointer-rest =
     * true, an automatic Mutter WM focus policy - but override_redirect
     * windows are explicitly EXEMPT from window-manager focus handling
     * by X11 protocol definition, so Mutter never considers this window
     * for real focus transfer AT ALL, regardless of clicking or any
     * client-side XSetInputFocus/XGrabKeyboard call. The taskbar's own
     * override_redirect windows only "get away with it" because they
     * grab initial focus once, early in the session, and mostly never
     * need it back - not because override_redirect genuinely supports
     * reliable focus under this compositor.
     *
     * Fix: stop being override_redirect. Become a normally WM-MANAGED
     * window instead (so Mutter applies its real focus policy - the
     * same one that already works for every ordinary app on this
     * desktop), and suppress the visible title bar/border via the
     * standard _MOTIF_WM_HINTS "no decorations" hint below instead of
     * via override_redirect - a widely-supported way to get "managed
     * but borderless" rather than "borderless but unmanaged and
     * therefore focus-exempt". */
    XSetWindowAttributes swa;
    /* REAL FIX 2026-08-16 (deferred earlier this session per direct
     * priority call, "white flash is low priority" then "lets handle
     * the events flash and move on"): WhitePixel shown before the first
     * real (dark) redraw() paints was a real viewer-safety issue (white
     * flash before going black/dark). Same real dark-default fix
     * open-hai/khtpm_entity_menu_render.c already proved this session -
     * no white-flash bug, not a placeholder color. */
    swa.background_pixel = alloc_pixel("#141414");
    swa.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | ButtonMotionMask | KeyPressMask | StructureNotifyMask | FocusChangeMask;
    win = XCreateWindow(dpy, RootWindow(dpy, screen), g_win_x, g_win_y, (unsigned)ww, (unsigned)wh, 0,
                         CopyFromParent, InputOutput, CopyFromParent,
                         CWBackPixel | CWEventMask, &swa);
    {
        /* _MOTIF_WM_HINTS: flags=MWM_HINTS_DECORATIONS(2), decorations=0
         * - hides the title bar/border on any WM that honors Motif hints
         * (Mutter does), without exempting the window from WM focus
         * management the way override_redirect does. */
        Atom motif_hints = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
        long hints[5] = { 2, 0, 0, 0, 0 }; /* flags, functions, decorations, input_mode, status */
        XChangeProperty(dpy, win, motif_hints, motif_hints, 32, PropModeReplace, (unsigned char *)hints, 5);

        /* WM_HINTS input=True - ICCCM's own way for a client to declare
         * it expects/accepts keyboard input via the normal input-focus
         * model, checked by real window managers when deciding whether
         * to grant click-to-focus at all. */
        XWMHints *wmhints = XAllocWMHints();
        if (wmhints) {
            wmhints->flags = InputHint;
            wmhints->input = True;
            XSetWMHints(dpy, win, wmhints);
            XFree(wmhints);
        }

        /* Now a real managed window - register WM_DELETE_WINDOW so a WM
         * or Alt+F4 asks nicely instead of killing the process outright
         * (own [x]/Escape close paths still work regardless). */
        Atom wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
        XSetWMProtocols(dpy, win, &wm_delete, 1);

        /* PPosition: tell the WM the x/y passed to XCreateWindow are a
         * REAL placement request, not a default hint. Without this, Mutter
         * (org.gnome.mutter) treats them as unspecified and auto-places
         * the window (it was landing at arbitrary spots like 148,54 or
         * 198,104 instead of the hq_ui.pdl window_x/window_y - direct
         * report 2026-08-13 "stats and db-hq window opens too high on
         * desktop, underneath tb directly"). PPosition is the standard
         * way every WM honors an explicitly-requested screen position. */
        XSizeHints *shints = XAllocSizeHints();
        if (shints) {
            shints->flags = PPosition;
            shints->x = g_win_x;
            shints->y = g_win_y;
            XSetWMNormalHints(dpy, win, shints);
            XFree(shints);
        }
    }
    {
        /* MUST be "MuchiverseLivedesk", not a db-hq-specific class - real
         * root cause found (studied tp_desktop_window.c's open_context_
         * menu(), $.crypts/enable_xwayland_grabs.sh): Mutter's Wayland
         * compositor restricts XGrabKeyboard from XWayland clients by
         * default (org.gnome.mutter.wayland xwayland-allow-grabs=false,
         * a real security policy, not a bug), and xwayland-grab-access-
         * rules allowlists by WM_CLASS - this house's rule already
         * allowlists exactly "MuchiverseLivedesk" (confirmed:
         * `gsettings get org.gnome.mutter.wayland xwayland-grab-access-
         * rules` -> ['MuchiverseLivedesk']). A different class here would
         * make the grab below silently fail exactly like tp_desktop_
         * window.c's own original bug. */
        XClassHint *ch = XAllocClassHint();
        if (ch) {
            ch->res_name = (char *)"MuchiverseLivedesk";
            ch->res_class = (char *)"MuchiverseLivedesk";
            XSetClassHint(dpy, win, ch);
            XFree(ch);
        }
    }
    XMapRaised(dpy, win);
    /* sync g_win_x/g_win_y to wherever the WM actually placed it (real
     * position was 198,104 in testing, not the requested 100,100) - a
     * ONE-TIME read via XGetWindowAttributes right after mapping is
     * exactly the coordinate space XMoveWindow itself expects
     * (parent-relative), so this is safe here even though re-reading it
     * repeatedly DURING a drag would not be (see g_win_x's own header
     * comment on why dragging uses pure accumulated deltas instead). */
    XSync(dpy, False);
    { XWindowAttributes wa; if (XGetWindowAttributes(dpy, win, &wa)) { g_win_x = wa.x; g_win_y = wa.y; } }
    /* focus_grab=0 (default, see load_font_scale()'s own comment on
     * g_focus_grab_enabled): KISS hail-mary - egg_window.c's own entity
     * window does NONE of this (no XSetInputFocus, no XGrabKeyboard) and
     * reliably works despite ALSO launching fresh from a click, so try
     * matching that bare-minimum behavior exactly before assuming more
     * machinery is the answer. focus_grab=1 keeps the earlier grab+retry
     * approach (ported from tp_desktop_window.c's open_context_menu())
     * available as a fallback without needing a rebuild. */
    if (g_focus_grab_enabled) {
        grab_keyboard_retry();
        soft_focus();
    }
    /* drain any stale Button/KeyPress already queued for this window id
     * before it existed (X11 can recycle a just-destroyed window's ID for
     * the next XCreateWindow call - same real race tp_desktop_window.c's
     * own comment documents) so a leftover event can't phantom-activate
     * something the instant this window maps. */
    XSync(dpy, False);
    { XEvent stale_ev; while (XCheckWindowEvent(dpy, win, ButtonPressMask | KeyPressMask, &stale_ev)) { } }

    gc = XCreateGC(dpy, win, 0, NULL);
    buf = XCreatePixmap(dpy, win, (unsigned)ww, (unsigned)wh, (unsigned)DefaultDepth(dpy, screen));
    xftdraw_buf = XftDrawCreate(dpy, buf, DefaultVisual(dpy, screen), cmap);

    redraw();

    /* headless verification aid: argv[3]=="--dump-and-exit" dumps one
     * frame and quits immediately, for environments with no key-sender
     * tool (xdotool/xte) available to press 'p' interactively. */
    if (argc > 3 && strcmp(argv[3], "--dump-and-exit") == 0) {
        dump_frame_png();
        g_quit = 1;
    }

    while (!g_quit) {
        /* relay poll every loop tick, independent of X events - same
         * shape as khtpm_strip_parser.c's own main loop (poll_agent_
         * relay() call before the select()). */
        if (poll_agent_relay() > 0 && !g_quit) redraw();
        if (g_quit) break;
        /* Generic live layout reload (mtime-gated, cheap every tick) -
         * see the mechanisms block above. */
        reload_if_changed();
        /* Stage 2d shell/manager split: pick up khtpm_hq_manager.c's
         * latest common_events publish (mtime-gated, cheap every tick). */
        if (load_common_events()) {
            Elem *sidebar = find_by_tag(g_window, "sidebar");
            inject_sidebar_items(sidebar);
            redraw();
        }

#ifdef _WIN32
        Sleep(150);
#else
        fd_set fds;
        FD_ZERO(&fds);
        int xfd = ConnectionNumber(dpy);
        FD_SET(xfd, &fds);
        struct timeval tv = { 0, 150000 }; /* 150ms, matches this app's own scale (small window, infrequent redraws) */
        select(xfd + 1, &fds, NULL, NULL, &tv);
#endif

        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            if (ev.type == Expose) {
                redraw();
            } else if (ev.type == ButtonPress) {
                /* focus_grab=0 (default): egg_window.c's own entity
                 * window does nothing focus-related on click either -
                 * matching that bare-minimum KISS behavior. focus_grab=1
                 * keeps the grab+retry / right-click-force-focus
                 * machinery available without a rebuild if the simple
                 * path turns out not to be enough. */
                if (g_focus_grab_enabled) {
                    grab_keyboard_retry();
                    soft_focus();
                }
                /* chrome-bar drag start - see g_dragging's own header
                 * comment. Only when the press lands in the chrome bar
                 * itself and NOT on the close button (so [x] still just
                 * closes on click, doesn't start a drag first). */
                if (ev.xbutton.button == 1 && ev.xbutton.y < g_chrome_h &&
                    !(ev.xbutton.x >= g_close_elem->x && ev.xbutton.x < g_close_elem->x + g_close_elem->w &&
                      ev.xbutton.y >= g_close_elem->y && ev.xbutton.y < g_close_elem->y + g_close_elem->h)) {
                    g_dragging = 1;
                    g_drag_last_x = ev.xbutton.x_root;
                    g_drag_last_y = ev.xbutton.y_root;
                }
                if (ev.xbutton.button == 4 || ev.xbutton.button == 5) {
                    /* mouse wheel over a palettes matrix grid: scroll
                     * rows (pallette-design.txt VIEW SPECS). Clamp
                     * happens in layout_pass's own post-pass. */
                    if (g_pal_has_grid) {
                        g_pal_scroll += (ev.xbutton.button == 5) ? 2 : -2;
                        redraw();
                    }
                }
                if (ev.xbutton.button != 3 && ev.xbutton.button != 4 && ev.xbutton.button != 5)
                    handle_click(ev.xbutton.x, ev.xbutton.y);
            } else if (ev.type == ButtonRelease && ev.xbutton.button == 1) {
                g_dragging = 0;
            } else if (ev.type == MotionNotify) {
                if (g_dragging) {
                    int dx = ev.xmotion.x_root - g_drag_last_x;
                    int dy = ev.xmotion.y_root - g_drag_last_y;
                    g_win_x += dx; g_win_y += dy;
                    XMoveWindow(dpy, win, g_win_x, g_win_y);
                    g_drag_last_x = ev.xmotion.x_root;
                    g_drag_last_y = ev.xmotion.y_root;
                }
            } else if (ev.type == KeyPress) {
                char buf8[8]; KeySym ks;
                int n = XLookupString(&ev.xkey, buf8, sizeof(buf8) - 1, &ks, NULL);
                buf8[n > 0 ? n : 0] = '\0';
                /* ground-truth log: this fires the INSTANT a real X11
                 * KeyPress reaches this process, before any of handle_
                 * key()'s own nav logic runs - if this line never
                 * appears in the log despite real physical typing, the
                 * problem is 100% confirmed upstream of this app (X11/
                 * Xwayland/Mutter focus delivery), not this app's own
                 * key-handling code, which was already separately
                 * proven correct via the relay. */
                const char *kname = XKeysymToString(ks);
                snprintf(g_last_key_label, sizeof(g_last_key_label), "%s", kname ? kname : (buf8[0] ? buf8 : "?"));
                fprintf(stderr, "db-hq: REAL KeyPress received: keysym=%s char=%c\n", kname ? kname : "?", buf8[0] ? buf8[0] : '?');
                handle_key(ks, buf8[0]);
                redraw(); /* so the debug status line updates even if handle_key's own branch didn't already redraw */
            } else if (ev.type == FocusIn) {
                g_has_real_focus = 1;
                fprintf(stderr, "db-hq: FocusIn (real keyboard focus confirmed)\n");
                redraw(); /* live-update the "^" title indicator, not just on next keypress */
            } else if (ev.type == FocusOut) {
                g_has_real_focus = 0;
                fprintf(stderr, "db-hq: FocusOut (keyboard focus lost)\n");
                redraw();
            } else if (ev.type == ClientMessage) {
                /* WM_DELETE_WINDOW - now a real managed window (see the
                 * XSetWMProtocols() call in main()), so a WM/Alt+F4 can
                 * send this instead of just killing the process. */
                Atom wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
                if ((Atom)ev.xclient.data.l[0] == wm_delete) g_quit = 1;
            }
        }
    }

    XUngrabKeyboard(dpy, CurrentTime);
    XftDrawDestroy(xftdraw_buf);
    XFreePixmap(dpy, buf);
    XFreeGC(dpy, gc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);

    KtbState ktb;
    ktb_init(&ktb, g_house_root);
    ktb_quit_and_save(&ktb);

    return 0;
}
