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
#include "khtpm_taskbar_manager.h" /* REAL, db-hq mode only (§5d.10) - ktb_init()/ktb_quit_and_save() KtbState persistence, ported from khtpm_hq_render.c's own real usage */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h> /* REAL, chat-hai mode only - session-dir listing */
#include <unistd.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/wait.h> /* REAL, db-hq mode only - launch_module()/cleanup_module(), real fork()+execl() */
#include <errno.h>
#include <signal.h> /* REAL, db-hq mode only - handle_term_signal() */
#include <time.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h> /* 2026-08-24 - XA_WINDOW for the XdndAware property (XDND drop support) */
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>

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
static time_t g_chtpm_mtime = 0;

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
        snprintf(e->label, sizeof(e->label), "%s", val);
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
static int reparse_chtpm_if_changed(void) {
    if (!g_chtpm_path[0]) return 0;
    struct stat st;
    if (stat(g_chtpm_path, &st) != 0) return 0;
    if (st.st_mtime == g_chtpm_mtime) return 0;
    g_chtpm_mtime = st.st_mtime;
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
static int g_is_swatch_picker = 0;
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
static void nav_tab_register(const char *title);
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
static int g_is_chat_hai = 0; /* REAL Stage 5 - chat-hai mode, WM-managed family, 3rd/last app */
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

/* ============ REAL, chat-hai mode content (ported verbatim, chai_-prefixed) ============ */
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
static int chai_n_elems_static = 0;
/* REAL FIX 2026-08-15 (direct report: "typing... is really slow", then
 * "why were not just using the same layout renderer... merging both
 * features, for both cell and chat-hai?"): chai_layout_pass() used to
 * unconditionally rewind g_n_elems and call BOTH chai_inject_sessions() and
 * chai_inject_panel_feed() on EVERY chai_redraw - meaning every keystroke fully
 * re-wrapped every visible message from scratch, even though typing
 * only changes the composer. open-hai's own draw_transcript() already
 * caches its wrapped layout ("(re)builds the flat-line cache used for
 * scroll math" - only when content changes, not every frame); this is
 * the same fix ported into chat-hai's own renderer (a full shared-
 * renderer merge is real, larger future work - see
 * chat-hai-design.md's own "Renderer consolidation" section - this is
 * the scoped, chat-hai-only version of that fix, safe to do without
 * touching open-hai/db-hq/events-hq at all).
 *
 * chai_feed_dirty/chai_sessions_dirty: set true whenever the underlying DATA
 * actually changes (new ledger content, session switch/create/delete)
 * - chai_layout_pass() only re-runs the expensive injection+wrap work when
 * its own dirty flag is set, clearing it after. A keystroke-only
 * chai_redraw (both flags clean) touches NEITHER chai_inject_sessions() nor
 * chai_inject_panel_feed() at all - just repositions the already-built
 * Elems, which is cheap (no wrapping, no XftFontOpenName calls).
 *
 * chai_n_elems_after_sessions: the elem-pool checkpoint right after
 * sessions last rebuilt - lets chai_inject_panel_feed() rewind ONLY its own
 * portion of the pool when feed is dirty but sessions isn't, instead
 * of wiping sessions' already-valid Elems too. */
static int chai_n_elems_after_sessions = 0;
static int chai_feed_dirty = 1;
static int chai_sessions_dirty = 1;
static char g_house_root[PATH_BUF];

/* REAL module launch (Stage 2d, 2026-08-16) - ported verbatim from
 * khtpm_hq_render.c/khtpm_events_hq_render.c's own chai_launch_module()/
 * chai_cleanup_module()/chai_handle_term_signal(), itself ported from
 * wraith_parser_alpha.c's own chai_launch_module(). REAL BEHAVIOR CHANGE for
 * chat-hai specifically, confirmed with direct instruction: unlike
 * db-hq/events-hq (which never had an independent manager before this),
 * chat_hai_loop.sh used to deliberately OUTLIVE the shell (button.sh's
 * own "leave it running" guard) - tying its lifetime to the shell here
 * is a real, deliberate change to that behavior, not an accidental
 * side effect of adopting the real mechanism. */
static pid_t chai_module_pid = -1;

static void chai_cleanup_module(void) {
    if (chai_module_pid > 0) {
        kill(chai_module_pid, SIGTERM);
        waitpid(chai_module_pid, NULL, WNOHANG);
        chai_module_pid = -1;
    }
}

static void chai_handle_term_signal(int sig) {
    (void)sig;
    chai_cleanup_module();
    _exit(0);
}

static void chai_launch_module(const char *src) {
    if (!src || !src[0]) return;
    char full_path[PATH_BUF];
    if (src[0] == '/') snprintf(full_path, sizeof(full_path), "%s", src);
    else snprintf(full_path, sizeof(full_path), "%s/%s", g_house_root, src);

    /* REAL FIX 2026-08-29 (orphaned chat_hai_loop.sh bug, live report: loop
     * keeps running even after renderer crashes). Root cause: renderer forks
     * the loop as a child, but if renderer is killed (especially with -9),
     * the loop becomes orphaned with no way to know. Solution: write the
     * renderer's own PID to state/chat_hai_renderer.pid BEFORE launching the
     * loop. The loop checks this file periodically (every round) and exits
     * cleanly if the PID no longer exists - matching this house's own
     * "file-based state only" philosophy (see !.HOUSE_STDS.md §4 real PID
     * files). This is identical in spirit to how nav_tab registers processes
     * per-PID (see nav_tab_register()), just reused for renderer liveness. */
    char state_dir[PATH_BUF];
    snprintf(state_dir, sizeof(state_dir), "%s/&.hq-apps/chat-hai/state", g_house_root);
    mkdir(state_dir, 0777);
    char pid_file[PATH_BUF];
    snprintf(pid_file, sizeof(pid_file), "%s/chat_hai_renderer.pid", state_dir);
    FILE *pf = fopen(pid_file, "w");
    if (pf) {
        fprintf(pf, "%d\n", (int)getpid());
        fclose(pf);
    }

    chai_module_pid = fork();
    if (chai_module_pid == 0) {
        execl(full_path, full_path, g_house_root, (char *)NULL);
        _exit(1);
    } else if (chai_module_pid < 0) {
        fprintf(stderr, "chat-hai: chai_launch_module: fork failed for %s\n", full_path);
        chai_module_pid = -1;
    }
}

/* wraith-alpha-standard index nav state (see Elem.nav_index comment) -
 * g_nav/g_n_nav/g_focus_nav/g_quit are the SAME shared globals every
 * other mode already uses (declared once, near the top of this file) -
 * reused here, not redeclared. */
/* Real, visible bug found live (2026-08-12, direct report: "no > is on
 * screen when it opens"): nav 1 used to ALWAYS be the chrome close
 * button, whose "[>N]" badge is deliberately suppressed (too small a
 * box to fit one - see chai_draw_elem()'s own comment) in favor of just an
 * outline ring - so NO visible "[>N]" text existed anywhere on screen at
 * launch. Fixed properly in chai_assign_nav_indices() (close moved to the
 * LAST nav index instead, per direct instruction), so nav 1 defaulting
 * here now lands on the first real content tab and shows immediately,
 * matching the taskbar/context menus always showing an obvious ">" on a
 * real row the instant they open. */
static int chai_digit_accum = 0;
static char chai_last_key_label[32] = ""; /* see chai_draw_chrome_bar()'s debug status line */

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
static int chai_dragging = 0;
static int chai_drag_last_x = 0, chai_drag_last_y = 0;
/* Running window position, purely accumulated via deltas - matches
 * egg_window.c's own win_start_x/win_start_y exactly. Deliberately NOT
 * re-read from the server mid-drag (XGetWindowAttributes' x/y are
 * PARENT-relative, and a real WM-managed window may be reparented into
 * a frame even with decorations=0 - mixing that with root-relative
 * motion deltas would drift wrong). Initialized to the window's real
 * creation position in main(). */
static int chai_win_x = 100, chai_win_y = 100;

/* REAL, NEW 2026-08-29 - TASK 1: popup window (entity-menu, swatch-picker)
 * drag support. Same real pattern: ButtonPress on chrome (y < CHROME_H)
 * records x_root/y_root, MotionNotify computes delta and XMoveWindow's,
 * ButtonRelease clears the flag. Uses the shared g_win_x/g_win_y. */
static int g_popup_dragging = 0;
static int g_popup_drag_last_x = 0, g_popup_drag_last_y = 0;
/* chat-hai's own forced window size (screen-relative, real fix for the
 * "chai_apply_css() clobbers a one-time override every chai_redraw" bug - see
 * chai_layout_pass()'s own header comment where these are applied). 0 = not
 * yet computed (main() sets these once, from real screen dimensions,
 * before the first chai_redraw). */
static int chai_forced_win_w = 0, chai_forced_win_h = 0;



/* find_by_tag()/find_by_id() now come from khtpm_render_core.h (Stage 2a,
 * 2026-08-16). find_by_id() disambiguates elements sharing a tag (e.g.
 * <text id="status"> and <text id="composer-text"> both being tag
 * "text" - see the real bug this fixed 2026-08-15: chai_composer_sync() was
 * mutating "status"'s label via find_by_tag(window,"text") matching the
 * FIRST "text" element in document order, not the composer). Real
 * fixed-set of long-lived control elements (status/toggle-pause/
 * composer-text/send) should be looked up by id via that, not by tag. */

#define CHAI_MAX_EVENTS 128
/* REAL FIX 2026-08-16 (truncation report): was [256] which silently
 * truncated messages over 255 chars at load time via snprintf — most
 * AI replies in gemma-lab are 500-2000+ chars, losing their tails
 * before wrapping even sees them. Bumped to 1024 (128KB total, well
 * within stack/static budget). */
static char chai_events[CHAI_MAX_EVENTS][1024];
static char chai_speakers[CHAI_MAX_EVENTS][32];
static char chai_times[CHAI_MAX_EVENTS][8]; /* "HH:MM" - short timestamp, restored 2026-08-15 (direct report: "we got rid of the timestamps... not good") */
static int chai_n_events = 0;
static int chai_selected_event = -1;
static int chai_paused = 0;
/* "who's typing" (direct ask, 2026-08-15: "is it possible to show whos
 * 'thinking' (AKA TYPING) if waiting for a request?") - mirrors
 * chat_hai_loop.sh's own state/typing.txt (see that script's speak()
 * function): empty when nobody's mid-request, a persona name while
 * their qwen.sh call is in flight (the actually-slow part, ~20-40s per
 * this session's own logged timings). */
static char chai_typing_name[64] = "";

/* ---------- data: sessions (2026-08-15, direct instruction: "we should
 * beable to add / delete new sessions (that will start fresh, new
 * memories)") — one independent .ledger file per session under
 * state/sessions/, matching open-hai's own real disk-persisted
 * deletable-history convention (chat-hai-design.md's own reference).
 * state/sessions/active.txt names which one is currently live; both this
 * renderer AND chat_hai_loop.sh (the actual chat scheduler) read it, so
 * switching sessions here takes effect in the running loop within one
 * round, not just visually. ---------- */

#define CHAI_MAX_SESSIONS 32
static char chai_session_names[CHAI_MAX_SESSIONS][64]; /* basename, no .ledger ext */
static int chai_n_sessions = 0;
static char chai_active_session[64] = "main";

static void chai_sessions_dir_path(char *out, size_t outsz) {
    snprintf(out, outsz, "%s/&.hq-apps/chat-hai/state/sessions", g_house_root);
}

static void chai_session_ledger_path(char *out, size_t outsz, const char *name) {
    char dir[PATH_BUF];
    chai_sessions_dir_path(dir, sizeof(dir));
    snprintf(out, outsz, "%s/%s.ledger", dir, name);
}

static void chai_active_session_path(char *out, size_t outsz) {
    char dir[PATH_BUF];
    chai_sessions_dir_path(dir, sizeof(dir));
    snprintf(out, outsz, "%s/active.txt", dir);
}

/* One-time migration (first launch after this feature landed): if
 * state/sessions/ doesn't exist yet but the old single
 * state/transcript.ledger does, seed sessions/main.ledger from it so
 * existing conversation history isn't silently dropped. Safe to call
 * every startup - only acts once (checked via directory existence). */
static void chai_migrate_legacy_ledger_if_needed(void) {
    char dir[PATH_BUF];
    chai_sessions_dir_path(dir, sizeof(dir));
    struct stat st;
    if (stat(dir, &st) == 0) return; /* already migrated */
    mkdir(dir, 0755);
    char legacy[PATH_BUF], main_ledger[PATH_BUF];
    snprintf(legacy, sizeof(legacy), "%s/&.hq-apps/chat-hai/state/transcript.ledger", g_house_root);
    chai_session_ledger_path(main_ledger, sizeof(main_ledger), "main");
    FILE *src = fopen(legacy, "r");
    if (src) {
        FILE *dst = fopen(main_ledger, "w");
        if (dst) {
            char buf[4096]; size_t n;
            while ((n = fread(buf, 1, sizeof(buf), src)) > 0) fwrite(buf, 1, n, dst);
            fclose(dst);
        }
        fclose(src);
    } else {
        FILE *dst = fopen(main_ledger, "w");
        if (dst) fclose(dst);
    }
    char active[PATH_BUF];
    chai_active_session_path(active, sizeof(active));
    FILE *a = fopen(active, "w");
    if (a) { fprintf(a, "main\n"); fclose(a); }
}

/* Scans state/sessions/*.ledger into chai_session_names[], and reads
 * active.txt into chai_active_session. Cheap enough to call every frame
 * (small dir, small file) - matches this file's existing "reload from
 * disk every chai_redraw" convention (chai_load_ledger() itself). */
static void chai_load_sessions_list(void) {
    chai_n_sessions = 0;
    char dir[PATH_BUF];
    chai_sessions_dir_path(dir, sizeof(dir));
    DIR *d = opendir(dir);
    if (d) {
        struct dirent *de;
        while ((de = readdir(d)) != NULL && chai_n_sessions < CHAI_MAX_SESSIONS) {
            size_t len = strlen(de->d_name);
            if (len > 7 && strcmp(de->d_name + len - 7, ".ledger") == 0) {
                size_t namelen = len - 7;
                if (namelen >= sizeof(chai_session_names[0])) namelen = sizeof(chai_session_names[0]) - 1;
                strncpy(chai_session_names[chai_n_sessions], de->d_name, namelen);
                chai_session_names[chai_n_sessions][namelen] = '\0';
                chai_n_sessions++;
            }
        }
        closedir(d);
    }
    /* simple lexical sort so the list is stable across frames (readdir
     * order is filesystem-dependent, not otherwise deterministic) */
    for (int i = 0; i < chai_n_sessions - 1; i++)
        for (int j = i + 1; j < chai_n_sessions; j++)
            if (strcmp(chai_session_names[i], chai_session_names[j]) > 0) {
                char tmp[64]; strcpy(tmp, chai_session_names[i]);
                strcpy(chai_session_names[i], chai_session_names[j]);
                strcpy(chai_session_names[j], tmp);
            }
    char active[PATH_BUF];
    chai_active_session_path(active, sizeof(active));
    FILE *f = fopen(active, "r");
    if (f) {
        if (fgets(chai_active_session, sizeof(chai_active_session), f)) {
            char *nl = strchr(chai_active_session, '\n');
            if (nl) *nl = '\0';
        }
        fclose(f);
    }
}

static void chai_load_ledger(void); /* fwd decl, real def below - chai_switch_session() needs it */

/* Writes state/sessions/active.txt = name and reloads. chat_hai_loop.sh
 * re-reads the same file at the top of each round (see that script's own
 * current_session()), so this takes effect for the running chat within
 * one round, not just for this window's own display. */
static void chai_switch_session(const char *name) {
    snprintf(chai_active_session, sizeof(chai_active_session), "%s", name);
    char active[PATH_BUF];
    chai_active_session_path(active, sizeof(active));
    FILE *f = fopen(active, "w");
    if (f) { fprintf(f, "%s\n", name); fclose(f); }
    chai_selected_event = -1;
    chai_load_ledger();
    if (chai_n_events > 0) chai_selected_event = chai_n_events - 1;
    /* REAL FIX 2026-08-15 (see chai_feed_dirty's own header comment) - a
     * session switch changes BOTH which messages the feed shows (new
     * active session's ledger) AND which sidebar row is highlighted
     * "active" - both need a real rebuild on the next chai_layout_pass(). */
    chai_feed_dirty = 1;
    chai_sessions_dirty = 1;
}

/* Real, working "add session" (direct instruction, 2026-08-15): creates
 * a fresh, empty ledger (new = genuinely fresh memories, not a copy of
 * any prior session) named by timestamp, and switches to it immediately
 * so the effect is visible right away, matching open-hai's own
 * click-to-create-then-see-it feel. */
static void chai_create_new_session(void) {
    char name[64];
    time_t now = time(NULL);
    struct tm *tmv = localtime(&now);
    strftime(name, sizeof(name), "chat-%Y%m%d-%H%M%S", tmv);
    char path[PATH_BUF];
    chai_session_ledger_path(path, sizeof(path), name);
    FILE *f = fopen(path, "w");
    if (f) {
        char t[32];
        strftime(t, sizeof(t), "%Y-%m-%d %H:%M:%S", tmv);
        fprintf(f, "[%s] system: new session started | Trigger: chat-hai\n", t);
        fclose(f);
    }
    chai_load_sessions_list();
    chai_switch_session(name);
}

/* Real, working "delete session" — Backspace on a focused sidebar
 * session row (see chai_handle_key()'s own new branch), matching open-hai's
 * documented "Backspace on a sidebar row deletes it" convention
 * (chat-hai-design.md's own reference to that precedent). Refuses to
 * delete the LAST remaining session (a chat app with zero sessions is a
 * broken state, not an empty one) and always leaves the loop pointed at
 * a real, existing ledger afterward. */
static void chai_delete_session(const char *name) {
    if (chai_n_sessions <= 1) return;
    char path[PATH_BUF];
    chai_session_ledger_path(path, sizeof(path), name);
    remove(path);
    chai_load_sessions_list();
    if (strcmp(chai_active_session, name) == 0 && chai_n_sessions > 0) {
        chai_switch_session(chai_session_names[0]); /* also sets both dirty flags, see its own header comment */
    } else {
        chai_load_sessions_list();
        chai_sessions_dirty = 1; /* list changed even though active session didn't */
    }
}

/* Loads the ACTIVE session's ledger (<house>/&.hq-apps/chat-hai/state/
 * sessions/<chai_active_session>.ledger, see the sessions block above) into
 * chai_events[] — the scrolling feed. REAL FIX 2026-08-15: previously read
 * a single hardcoded state/transcript.ledger regardless of session.
 * Lines use the master-ledger formula:
 *   [YYYY-MM-DD HH:MM:SS] <speaker>: <message> | Trigger: chat-hai
 * Display format: SHORT "HH:MM" timestamp kept (see chai_times' own header
 * comment - a real regression this session accidentally dropped the
 * timestamp ENTIRELY instead of just shortening the on-screen display
 * of it, direct report: "not good i just wanted to make them smaller").
 * No more fixed-72-char content truncation either - chai_inject_panel_feed()
 * now real-wraps each message to however many lines it actually needs
 * (see chai_wrap_lines()), so keep the FULL message text here. */
/* REAL FIX 2026-08-16, direct live report ("u003e" literal text visible
 * in rendered messages instead of ">"): ledger lines can contain raw
 * JSON-style \uXXXX escapes (confirmed live in
 * state/sessions/*.ledger - some upstream LLM-response path writes the
 * JSON-encoded string straight into the ledger without decoding it
 * first). Real, minimal decode - only the 3 escapes that actually
 * appear in this house's own real ledger data (>/</& -
 * ">"/"<"/"&", the same 3 that always travel together from JSON/HTML
 * escaping of markup-like characters), same narrow-scope discipline as
 * this file's own decode_entities() above (only &quot;/&amp; because
 * that's literally all ITS one real call site needs). NOT a general
 * \uXXXX-to-UTF8 decoder - do not widen scope without a real, confirmed
 * need. Decodes in place.
 *
 * REAL, confirmed via hex dump of the actual live ledger data
 * (state/sessions/gemma-lab.ledger): the literal bytes are bare
 * "u003e" with NO leading backslash (`echo -n | xxd` showed
 * `75 30 30 33 65` right after a plain space, not `5c 75 ...`) - some
 * upstream JSON-decode step already consumed/stripped the backslash
 * without recognizing the escape it introduced, leaving the bare
 * "u003e" token behind. Matching WITHOUT a backslash, per the real
 * data, not the textbook JSON escape form. */
static void chai_decode_u_escapes(char *s) {
    char *r = s, *w = s;
    while (*r) {
        if (strncmp(r, "u003e", 5) == 0) { *w++ = '>'; r += 5; }
        else if (strncmp(r, "u003c", 5) == 0) { *w++ = '<'; r += 5; }
        else if (strncmp(r, "u0026", 5) == 0) { *w++ = '&'; r += 5; }
        else *w++ = *r++;
    }
    *w = '\0';
}

static void chai_load_ledger(void) {
    chai_n_events = 0;
    char ledger[PATH_BUF];
    chai_session_ledger_path(ledger, sizeof(ledger), chai_active_session);
    FILE *f = fopen(ledger, "r");
    if (!f) return;
    /* REAL FIX 2026-08-15 (direct report: "chat isn't updating" - the
     * actual root cause after everything else this session): this loop
     * used to read from the START of the file and stop the moment
     * chai_n_events hit CHAI_MAX_EVENTS (128) - so once a session's ledger grew
     * past 128 lines (trivially easy after a day of testing - main's
     * was already at 175), every subsequent chai_load_ledger() call re-read
     * the SAME first 128 (oldest) lines and NEVER reached anything
     * appended after that point. The mtime-poll fix earlier this
     * session (main loop's own stat()-and-reload) was firing correctly
     * on every new message, calling chai_load_ledger() right on schedule -
     * it just kept re-loading the same stale head of the file every
     * time, which is why the feed looked completely frozen despite the
     * chat loop visibly still running. Real fix: count total lines
     * first, then skip to (total - CHAI_MAX_EVENTS) before parsing, so this
     * always loads the TAIL (most recent), not the head. */
    /* REAL FIX 2026-08-16, direct live report ("resembling"/"would shift"
     * appearing as their own timestamp-less, speaker-less fake
     * "messages" - real forward/start truncation): this used to read
     * with fgets(line, 512, f) - real ledger lines run 1200-13700+
     * bytes (confirmed live, state/sessions/gemma-lab.ledger). fgets()
     * silently truncates at 511 bytes and returns; the NEXT fgets()
     * call then picks up mid-sentence (no leading '[timestamp]', no
     * speaker) and the loop treated that continuation fragment as its
     * OWN separate chai_events[] entry - exactly the symptom reported
     * (missing speaker, text starting mid-word). Real fix: getline()
     * with a NULL/0 buffer - it grows the buffer as needed, so one call
     * always returns one true logical (newline-terminated) line
     * regardless of length, matching every other real ledger-reading
     * convention in this house (no arbitrary fixed-size guess to
     * silently violate later, same real lesson as the old fixed-72-char
     * truncation fix above). */
    long total_lines = 0;
    {
        char *probe = NULL; size_t probe_cap = 0;
        while (getline(&probe, &probe_cap, f) != -1) total_lines++;
        free(probe);
    }
    rewind(f);
    long skip = total_lines > CHAI_MAX_EVENTS ? total_lines - CHAI_MAX_EVENTS : 0;
    char *line = NULL; size_t line_cap = 0;
    while (skip-- > 0 && getline(&line, &line_cap, f) != -1) { /* discard */ }
    while (getline(&line, &line_cap, f) != -1 && chai_n_events < CHAI_MAX_EVENTS) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        char *trig = strstr(line, " | Trigger: chat-hai");
        if (trig) *trig = '\0';
        /* pull "HH:MM" out of "[YYYY-MM-DD HH:MM:SS] ..." (chars 12-16
         * of a well-formed line) and advance content past the full
         * bracketed timestamp, same split point as before - only the
         * ON-SCREEN representation of the time got shorter, not gone. */
        char *content = line;
        chai_times[chai_n_events][0] = '\0';
        if (line[0] == '[') {
            char *bracket = strchr(line, ']');
            if (bracket && bracket[1] == ' ') {
                if (bracket - line >= 17) { /* "[YYYY-MM-DD HH:MM" is at least this long */
                    snprintf(chai_times[chai_n_events], sizeof(chai_times[0]), "%.5s", line + 12);
                }
                content = bracket + 2;
            }
        }
        if (content[0]) {
            /* extract speaker name (text before the first ": ") for color-coding */
            char *colon = strstr(content, ": ");
            if (colon) {
                int speaker_len = colon - content;
                if (speaker_len > 0 && speaker_len < (int)sizeof(chai_speakers[0]) - 1) {
                    strncpy(chai_speakers[chai_n_events], content, speaker_len);
                    chai_speakers[chai_n_events][speaker_len] = '\0';
                } else {
                    strcpy(chai_speakers[chai_n_events], "user");
                }
            } else {
                strcpy(chai_speakers[chai_n_events], "user");
            }
            /* REAL FIX 2026-08-15 (direct report: "explain why messages
             * cut off outside window instead of wrapping" - see
             * chai_wrap_lines()/chai_inject_panel_feed() for the actual fix): this
             * used to hard-truncate at a FIXED 72 characters regardless
             * of the panel's real pixel width, which is why long
             * messages visibly ran off the right edge of the window
             * instead of wrapping - a char-count guess has no relation
             * to actual rendered pixel width (font, per-character width,
             * and the panel's real width all vary). Full content is
             * kept here now; wrapping happens where real geometry is
             * known (chai_inject_panel_feed(), at panel width, not a guess). */
            snprintf(chai_events[chai_n_events], sizeof(chai_events[0]), "%s", content);
            chai_decode_u_escapes(chai_events[chai_n_events]);
        } else continue;
        chai_n_events++;
    }
    free(line);
    fclose(f);
}

/* REAL FIX 2026-08-15 (direct instruction: "sessions are on left ...
 * just like open-hai"): the sidebar (left column, per chat-hai.chtpm's
 * own <sidebar id="sessions">) is the SESSIONS list, not the chat feed
 * — a real, live layout bug had messages injected here instead, leaving
 * the actual message panel with no feed content at all (only its fixed
 * status/composer/send controls, stacked with no bounded scroll area,
 * which is what visually read as "input takes up the entire vertical
 * pane"). Real, working session list (2026-08-15 follow-up, direct
 * instruction: "we should beable to add / delete new sessions") — one
 * row per file in state/sessions/ (see chai_load_sessions_list()), plus a
 * trailing "+ New Session" row (tag "newsession", disambiguated from
 * real session rows by tag, not id, in chai_activate_elem()). Click a real
 * row to chai_switch_session(); Backspace while it's focused deletes it (see
 * chai_handle_key()'s own new branch) — matching open-hai's own
 * disk-persisted deletable-history convention this was built to mirror. */
static void chai_inject_sessions(Elem *sidebar) {
    if (!sidebar) return;
    sidebar->n_children = 0;
    chai_load_sessions_list();
    /* REAL FIX 2026-08-15 (direct report: "THE MAIN/NEW+ FONTS ARE
     * BLACK ON DARK BACKGROUND") - class renamed from "data-item" to
     * "session-item": the old CSS rule targeting these
     * (".sessions .data-item") used a descendant combinator this
     * parser doesn't support (see chat-hai.css's own header comment on
     * the ".session-item" rule this class now matches) and silently
     * never applied, leaving these black. */
    for (int i = 0; i < chai_n_sessions; i++) {
        Elem *item = elem_new("item");
        item->parent = sidebar;
        snprintf(item->classes[0], sizeof(item->classes[0]), "session-item");
        item->n_classes = 1;
        snprintf(item->label, sizeof(item->label), "%s", chai_session_names[i]);
        item->active = (strcmp(chai_session_names[i], chai_active_session) == 0);
        if (sidebar->n_children < MAX_CHILDREN) sidebar->children[sidebar->n_children++] = item;
    }
    Elem *newbtn = elem_new("newsession");
    newbtn->parent = sidebar;
    snprintf(newbtn->classes[0], sizeof(newbtn->classes[0]), "session-item");
    newbtn->n_classes = 1;
    snprintf(newbtn->label, sizeof(newbtn->label), "+ New Session");
    if (sidebar->n_children < MAX_CHILDREN) sidebar->children[sidebar->n_children++] = newbtn;
}

/* Cached pointers to the panel's own fixed-position controls, captured
 * once right after parse_chtpm() (see main()) — chai_inject_panel_feed()
 * rebuilds panel->children every chai_layout_pass() with a variable-length
 * run of feed items followed by these 4 known elements, so they must
 * survive across n_children resets rather than being re-found by a
 * tag/id walk that would itself be searching the very array being
 * rebuilt. */
static Elem *chai_status_elem = NULL;
static Elem *chai_toggle_elem = NULL;
static Elem *chai_speed_elem = NULL;
static Elem *chai_sound_elem = NULL;
static Elem *chai_composer_text_elem = NULL;
/* "^" activation state for the composer, see chai_require_cli_activation's
 * own header comment. Starts activated (1) - matches this app's
 * historical always-on-typing default when require_cli_activation=0. */
static int chai_composer_activated = 1;
/* g_send_elem removed 2026-08-15 (direct instruction: "we dont need a
 * send button" - Enter already sends, see chai_handle_key()'s Enter branch). */

/* Composes chai_status_elem's label from BOTH chai_paused and chai_typing_name -
 * called from the toggle-pause click handler AND the main loop's own
 * typing.txt poll, so either one changing updates the same real status
 * text instead of the two clobbering each other's writes. */
static void chai_update_status_label(void) {
    if (!chai_status_elem) return;
    if (chai_typing_name[0]) {
        snprintf(chai_status_elem->label, sizeof(chai_status_elem->label), "%s \xc2\xb7 %s typing\xe2\x80\xa6",
                 chai_paused ? "[stopped]" : "[running]", chai_typing_name);
    } else {
        snprintf(chai_status_elem->label, sizeof(chai_status_elem->label), "%s", chai_paused ? "[stopped]" : "[running]");
    }
}

/* Rebuilds panel->children as [ up to n_visible tail feed items (oldest
 * of the visible window first, newest last) ..., status, toggle-pause,
 * composer-text, send ]. n_visible is computed by the CALLER
 * (chai_layout_pass(), from the FIXED bottom-controls height reserved BEFORE
 * this runs — see that function's own comment) so there is no
 * chicken-and-egg between "how tall is the feed" and "how many items
 * are in it": the feed's pixel height is fixed by the bottom controls
 * alone, item count is derived FROM that height, never the reverse.
 * This is the same real fix class as open-hai's own transcript_geom()
 * (composer height computed first, feed gets what's left) — see
 * chat-hai-design.md's "Real layout spec" section, ported here rather
 * than reinvented. */
/* Forward decls - chai_apply_css()/chai_measure_text_px()/chai_wrap_lines() are defined
 * further down this file (they need dpy/screen/Xft, real definitions
 * near the rendering section), but chai_inject_panel_feed() here needs them
 * for real per-message text wrapping (see chai_wrap_lines()'s own header
 * comment). dpy/screen already have this exact "declared again earlier
 * than their real definition" pattern elsewhere in this file (both
 * appear a second time near the rendering section too) - same harmless
 * C tentative-declaration shape, not new precedent. */
static void chai_apply_css(Elem *e, int hover);
static int chai_scaled(int base_px);
static int chai_measure_text_px(const CssStyle *st, const char *text);
#define CHAI_WRAP_MAX_LINES 8
#define CHAI_WRAP_LINE_BUF 256
static int chai_wrap_lines(const CssStyle *st, const char *text, int max_w, char lines[][CHAI_WRAP_LINE_BUF]);

/* Receipt diagnostics — set each frame by chai_layout_pass() / chai_inject_panel_feed(),
 * read by chai_dump_frame_png() to write /tmp/chat-hai-frame.png.receipt.txt.
 * Lets agents verify layout math without needing to see the image. */
static int chai_panel_w = 0;
static int chai_wrap_w = 0;
static int chai_tabbar_h = 0;
static int chai_feed_font_css = 12;  /* CSS base font-size for feed items (before scaling) */

static void chai_inject_panel_feed(Elem *panel, int n_visible) {
    if (!panel) return;
    panel->n_children = 0;
    if (n_visible < 0) n_visible = 0;
    if (chai_n_events == 0) {
        Elem *item = elem_new("item");
        item->parent = panel;
        snprintf(item->label, sizeof(item->label), "(ledger empty — loop not running?)");
        if (panel->n_children < MAX_CHILDREN) panel->children[panel->n_children++] = item;
    } else {
        /* REAL FIX 2026-08-15 (direct report: "explain why messages cut
         * off outside window instead of wrapping" - see chai_wrap_lines()'s
         * own header comment): each message now becomes 1+ "item"
         * Elems (one per wrapped line), instead of always exactly one
         * possibly-overflowing Elem. n_visible counts LINES (unchanged
         * meaning from before - chai_layout_pass()'s own per-child stacking
         * loop already treats each panel "item" child as one line-height
         * row, so no change needed there, just how many Elems this
         * function produces per logical message). Timestamp restored
         * (direct report: "we got rid of the timestamps... not good") -
         * prefixed on each message's FIRST wrapped line only. */
        /* REAL FIX 2026-08-16 (truncation report: "text is sometimes
         * getting forward truncated"): chai_draw_elem() adds a default 4px left
         * pad to label_x (line ~1556: "int pad = ... 4; int label_x =
         * e->x + pad"), but wrap_w was computed as panel->w - chai_scaled(8)
         * — the same width given to the Elem. Wrapping therefore
         * overestimated the available drawing width by that 4px pad,
         * causing the last ~1 char of every marginal wrapped line to
         * overflow the element boundary. Subtract the pad here so
         * wrap_w matches the actual space chai_draw_elem() will use. */
        int wrap_w = panel->w - chai_scaled(8) - 4;
        if (wrap_w < 20) wrap_w = 20;
        chai_wrap_w = wrap_w;
        Elem tmp_style_elem;
        memset(&tmp_style_elem, 0, sizeof(tmp_style_elem));
        snprintf(tmp_style_elem.tag, sizeof(tmp_style_elem.tag), "item");
        snprintf(tmp_style_elem.classes[0], sizeof(tmp_style_elem.classes[0]), "data-item");
        tmp_style_elem.n_classes = 1;
        tmp_style_elem.parent = panel;  /* for descendant CSS: .content .data-item */
        chai_apply_css(&tmp_style_elem, 0);
        chai_feed_font_css = tmp_style_elem.style.has_font_size ? tmp_style_elem.style.font_size : 12;

        typedef struct { int event_idx; int n; char lines[CHAI_WRAP_MAX_LINES][CHAI_WRAP_LINE_BUF]; } WrappedMsg;
        static WrappedMsg wm[CHAI_MAX_EVENTS];
        int n_msgs = 0, total_lines = 0;
        for (int i = chai_n_events - 1; i >= 0 && n_msgs < CHAI_MAX_EVENTS; i--) {
            char full[400];
            if (chai_times[i][0]) snprintf(full, sizeof(full), "%s %s", chai_times[i], chai_events[i]);
            else snprintf(full, sizeof(full), "%s", chai_events[i]);
            int nl = chai_wrap_lines(&tmp_style_elem.style, full, wrap_w, wm[n_msgs].lines);
            if (total_lines + nl > n_visible && n_msgs > 0) break; /* always keep at least the newest message, even if it alone overflows */
            wm[n_msgs].event_idx = i;
            wm[n_msgs].n = nl;
            total_lines += nl;
            n_msgs++;
            if (total_lines >= n_visible) break;
        }
        for (int m = n_msgs - 1; m >= 0; m--) { /* emit oldest-first, matching the panel's own top-to-bottom stacking */
            int i = wm[m].event_idx;
            for (int l = 0; l < wm[m].n; l++) {
                Elem *item = elem_new("item");
                if (!item) break;
                item->parent = panel;
                snprintf(item->classes[0], sizeof(item->classes[0]), "data-item");
                snprintf(item->classes[1], sizeof(item->classes[1]), "%s", chai_speakers[i]);
                item->n_classes = 2;
                snprintf(item->label, sizeof(item->label), "%s", wm[m].lines[l]);
                item->active = (i == chai_selected_event);
                if (panel->n_children < MAX_CHILDREN) panel->children[panel->n_children++] = item;
            }
        }
    }
    /* Re-append the 4 cached fixed controls, in the SAME order
     * find_by_id()/find_by_tag() callers elsewhere in this file expect
     * (status first — chai_composer_sync()'s sibling logic and the
     * toggle-pause handler both rely on status being found before any
     * other "text"-tagged element, see find_by_id()'s own header
     * comment for the bug this replaced). */
    if (chai_status_elem && panel->n_children < MAX_CHILDREN) panel->children[panel->n_children++] = chai_status_elem;
    if (chai_toggle_elem && panel->n_children < MAX_CHILDREN) panel->children[panel->n_children++] = chai_toggle_elem;
    if (chai_speed_elem && panel->n_children < MAX_CHILDREN) panel->children[panel->n_children++] = chai_speed_elem;
    if (chai_sound_elem && panel->n_children < MAX_CHILDREN) panel->children[panel->n_children++] = chai_sound_elem;
    if (chai_composer_text_elem && panel->n_children < MAX_CHILDREN) panel->children[panel->n_children++] = chai_composer_text_elem;
}

/* ---------- layout: CSS overrides a small hand-rolled per-tag flow,
 * since v1 deliberately has no flex/grid engine (see plan) ---------- */

/* Order matches au11-hq/rpg-maker-database.html's own tab-bar exactly
 * (line 301-316) - real RPG Maker MV order, 15 tabs total. Direct
 * correction (2026-08-12): Common Events belongs right after Tilesets,
 * not last; "Terms" is its own 15th tab, separate from "Types" (both
 * exist in the mockup - not a typo/merge). */
static const char *CHAI_TAB_LABELS[] = {
    "Actors", "Classes", "Skills", "Items", "Weapons", "Armors",
    "Enemies", "Troops", "States", "Animations", "Tilesets",
    "Common Events", "System", "Types", "Terms"
};
#define CHAI_N_TABS 15
#define CHAI_COMMON_EVENTS_TAB 11
static int chai_current_tab = CHAI_COMMON_EVENTS_TAB; /* the only wired tab */

static const CssSheet *chai_sheet;

/* REAL 2026-08-16: ancestor callbacks for descendant-combinator CSS
 * support (e.g. ".messages-feed .data-item"). get_parent returns the
 * element's parent pointer (or NULL at root). get_info fills in an
 * element's tag/id/classes — used to inspect ancestors during matching. */
static void *chai_chat_hai_get_parent(const void *elem) {
    const Elem *e = (const Elem *)elem;
    return e->parent;
}

static void chai_chat_hai_get_info(const void *elem, const char **out_tag, const char **out_id,
                               char out_classes[][32], int *out_n_classes) {
    const Elem *e = (const Elem *)elem;
    *out_tag = e->tag;
    *out_id = e->id[0] ? e->id : NULL;
    *out_n_classes = e->n_classes;
    for (int i = 0; i < e->n_classes; i++)
        snprintf(out_classes[i], 32, "%s", e->classes[i]);
}

static void chai_apply_css(Elem *e, int hover) {
    css_compute_style_ex(chai_sheet, e->tag, e->id[0] ? e->id : NULL, e->classes, e->n_classes, hover, &e->style, e, chai_chat_hai_get_parent, chai_chat_hai_get_info);
}

/* X11/Xft globals - declared here (not down in the rendering section)
 * because layout now needs to MEASURE real font metrics, not guess a
 * fixed px-per-char width; that guess (7px/char) was the actual cause of
 * "big and jumbled" text - it didn't match whichever font XftFontOpenName
 * actually resolved, so boxes were sized wrong and labels overlapped. */

/* user-defined UI scale, direct request: "even if the window needed to
 * be bigger... or even reading this from a std user defined font size
 * .pdl so user can adjust scale for readability/access". Shared across
 * all -hq apps (not taskbar-specific), same key=value .pdl convention
 * already used by khtpm_strip_parser.c's load_theme_opacity() (reads
 * #.desktop/livedesk_taskbar.pdl the same way). Applies to BOTH font
 * sizes and layout box sizes (chrome height, row heights, default window
 * size) so a bigger font never gets clipped by boxes that didn't grow
 * with it - text metrics are measured AFTER scaling (chai_measure_text_px()
 * below), so nothing needs a second manual size fixup. */
static double chai_font_scale = 1.0;

/* focus_grab: KISS hail-mary, direct instruction 2026-08-12 ("all that
 * focus stuff is overkill... keep it in a separate config/.pdl, do the
 * same as a last hail mary"). Studied egg_window.c (a real "context"
 * entity window, ALSO launched fresh from a click, confirmed reliably
 * keyboard-usable) and found it does ZERO focus/grab calls for its main
 * window - no XSetInputFocus, no XGrabKeyboard, nothing beyond plain
 * override_redirect + XMapWindow. Default flips to that same bare-
 * minimum behavior; the whole chai_soft_focus()/XGrabKeyboard machinery
 * built earlier this session is kept but now OFF by default, toggleable
 * back on via this key without a rebuild if the simple path doesn't
 * actually fix it. */
static int chai_focus_grab_enabled = 0;

static void chai_load_font_scale(void) {
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
            if (v >= 0.5 && v <= 3.0) chai_font_scale = v; /* sane clamp - not a layout-breaking value */
        } else if (strcmp(line, "focus_grab") == 0) {
            chai_focus_grab_enabled = atoi(val) != 0;
        } else if (strcmp(line, "window_x") == 0) {
            chai_win_x = atoi(val);
        } else if (strcmp(line, "window_y") == 0) {
            chai_win_y = atoi(val);
        }
    }
    fclose(f);
}

/* REAL FIX 2026-08-15 (direct instruction: "all window dims can be read
 * from .pdl isntead of hardcoded" - the standing !.HOUSE_STDS.md §A.7
 * rule this file already violated three separate rounds in a row while
 * iterating on screen position/size by hand-editing C constants and
 * rebuilding each time). Reads chat_hai_config.pdl's own
 * window_width/window_bottom_margin/window_right_margin/window_top_offset
 * keys (unscaled base pixels - chai_font_scale still multiplies these the
 * same way it scales everything else, see that .pdl's own header
 * comment). Read ONCE at startup (unlike sleep_between, window geometry
 * doesn't need live mid-session reread) - called from main(), before
 * the screen-anchor block that consumes these values. */
static int chai_cfg_window_width = 280;
static int chai_cfg_bottom_margin = 50;
static int chai_cfg_right_margin = 10;
static int chai_cfg_top_offset = 100;

/* REAL FIX 2026-08-16 (direct report: "when i clicked 'input' the focus
 * nav '>' doesn't move into its bracket till i click elsewhere ... but it
 * still allows input"): legacy chtpm_parser.c required a cli-io input to
 * be explicitly "^" activated before it would accept keystrokes; this
 * app never had that gate (typing always went straight into the
 * composer regardless of focus/click, see chai_handle_key()'s printable-char
 * branch), and separately chai_activate_elem() had no case at all for the
 * composer-text element - a click moved g_focus_nav correctly but never
 * called chai_redraw(), so the badge only caught up whenever some OTHER event
 * happened to chai_redraw next. Direct follow-up ("in h-ai it move '>' and
 * auto sets it to '^' im fine with that, and make it an option to have
 * legacy from chtpm behavior"): default here is auto-activate-on-click/
 * focus (own real bug now fixed below); require_cli_activation=1 in this
 * .pdl switches to the legacy shape (click only focuses - '>' - typing
 * is ignored until Enter on the empty, focused composer sets it to '^').
 * khtpm_render_core.c candidates (db-hq/events-hq/open-hai/taskbar-
 * settings/taskbar) do not have any cli-io text-input element at all
 * currently (only buttons/tabs/nav rows), so this option has nothing to
 * port to yet there - tracked as a real research/doc TODO in
 * local-2do-15.txt rather than spec'd blind. */
static int chai_require_cli_activation = 0;

/* Incoming-message tone (2026-08-16): loaded from chat_hai_config.pdl's
 * sound_on key at startup (like require_cli_activation above), flipped
 * live by the Sound GUI button (chai_write_chat_hai_cfg()) and read fresh by
 * chat_hai_loop.sh on every posted message - see that script. */
static int chai_sound_on = 1;

static void chai_load_window_geometry_config(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/&.hq-apps/chat-hai/chat_hai_config.pdl", g_house_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#') continue;
        char key[64], val[64];
        if (sscanf(line, " SECTION | %63[^|] | %63s", key, val) != 2) continue;
        char *k = key; while (*k == ' ') k++;
        char *ke = k + strlen(k); while (ke > k && ke[-1] == ' ') *--ke = '\0';
        int n = atoi(val);
        if (strcmp(k, "window_width") == 0) chai_cfg_window_width = n;
        else if (strcmp(k, "window_bottom_margin") == 0) chai_cfg_bottom_margin = n;
        else if (strcmp(k, "window_right_margin") == 0) chai_cfg_right_margin = n;
        else if (strcmp(k, "window_top_offset") == 0) chai_cfg_top_offset = n;
        else if (strcmp(k, "require_cli_activation") == 0) chai_require_cli_activation = n;
        else if (strcmp(k, "sound_on") == 0) chai_sound_on = (n != 0);
    }
    fclose(f);
}

static int chai_scaled(int base_px) { return (int)(base_px * chai_font_scale + 0.5); }

/* REAL FIX 2026-08-15 (direct report: "typing in chat-hai... is really
 * slow... blocking render or something?"): every printable keystroke
 * calls chai_redraw() (chai_handle_key()'s own printable-char branch), which
 * re-wraps the ENTIRE visible feed via chai_wrap_lines() -> this function,
 * called once per word-boundary candidate for EVERY visible message,
 * every single keystroke. This function used to open a fresh XftFont
 * (a real, non-trivial font-lookup cost) and close it again on EVERY
 * call - with dozens of visible lines each needing several measure
 * calls to wrap, that's potentially hundreds of XftFontOpenName/
 * XftFontClose round-trips per keystroke. The font spec is almost
 * always IDENTICAL across all those calls in one chai_redraw (same feed
 * style) - real fix: cache the open font by its spec string, only
 * reopen when the spec actually changes. Not a 30fps/LAN-vs-local
 * issue (direct question) - this is synchronous CPU work on the main
 * X11 event thread, unrelated to model inference speed entirely. */
static int chai_measure_text_px(const CssStyle *st, const char *text) {
    char spec[128];
    const char *fam = st->has_font_family ? st->font_family : "DejaVu Sans";
    int size = chai_scaled(st->has_font_size ? st->font_size : 12);
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

#define CHAI_WRAP_MAX_LINES 8
#define CHAI_WRAP_LINE_BUF 256

/* REAL FIX 2026-08-15 (direct report: "explain why messages cut off
 * outside window instead of wrapping" - see chai_load_ledger()'s own header
 * comment for the removed fixed-72-char truncation this replaces):
 * real word-boundary wrapping at the ACTUAL pixel width available (via
 * chai_measure_text_px(), not a character-count guess). Greedy: grows a
 * candidate line one word at a time, backtracks to the last word
 * boundary the moment it no longer fits max_w, same algorithm shape as
 * open-hai's own wrap_text() (khtpm_open_hai_render.c) - not reinvented,
 * chat-hai just didn't have its own copy of this until now. Returns the
 * number of lines written (capped at CHAI_WRAP_MAX_LINES - a single message
 * that's still too long past that point gets a trailing "…" on the
 * last line rather than growing the feed unboundedly). */
static int chai_wrap_lines(const CssStyle *st, const char *text, int max_w, char lines[][CHAI_WRAP_LINE_BUF]) {
    int n = 0;
    const char *p = text;
    while (*p && n < CHAI_WRAP_MAX_LINES) {
        const char *line_start = p;
        const char *last_space = NULL;
        const char *scan = p;
        char candidate[CHAI_WRAP_LINE_BUF];
        candidate[0] = '\0';
        while (*scan) {
            const char *next = scan;
            while (*next && *next != ' ') next++;
            int seglen = (int)(next - line_start);
            if (seglen >= CHAI_WRAP_LINE_BUF) seglen = CHAI_WRAP_LINE_BUF - 1;
            char test[CHAI_WRAP_LINE_BUF];
            memcpy(test, line_start, (size_t)seglen);
            test[seglen] = '\0';
            if (chai_measure_text_px(st, test) > max_w && candidate[0]) break;
            snprintf(candidate, sizeof(candidate), "%s", test);
            if (*next == ' ') { last_space = next; scan = next + 1; } else { scan = next; break; }
        }
        (void)last_space;
        if (!candidate[0]) { /* single word wider than max_w - hard cut so we always make progress */
            size_t take = strlen(line_start);
            if (take > CHAI_WRAP_LINE_BUF - 1) take = CHAI_WRAP_LINE_BUF - 1;
            memcpy(candidate, line_start, take);
            candidate[take] = '\0';
        }
        snprintf(lines[n], CHAI_WRAP_LINE_BUF, "%s", candidate);
        n++;
        p = line_start + strlen(candidate);
        while (*p == ' ') p++;
    }
    if (*p && n == CHAI_WRAP_MAX_LINES) {
        /* still more text left after hitting the cap - mark truncation
         * on the last line rather than silently dropping the tail. */
        size_t l = strlen(lines[n - 1]);
        if (l < CHAI_WRAP_LINE_BUF - 4) snprintf(lines[n - 1] + l, CHAI_WRAP_LINE_BUF - l, "…");
    }
    return n;
}

/* Own drawn chrome bar (title + close), NOT a window-manager decoration -
 * same idea as wraith-alpha's own chrome row (ops/wraith_parser_alpha.c's
 * g_chrome_icons[]: nav 1 = title, icons after it, 'x' = CHROME_ACTION_
 * CLOSE), direct instruction: "we will create our own chrome bar and
 * title, ok? like in wraith-alpha". Kept to just title + close for this
 * app (no minimize/geom/context-menu - wraith-alpha's fuller icon set
 * isn't needed here). Window height grows by chai_chrome_h on top of the
 * CSS/default content height, so nothing below has to shrink to fit it.
 * chai_chrome_h (and every other layout constant in chai_layout_pass() below) is
 * chai_scaled by chai_font_scale, not just font sizes - a bigger font with
 * same-size boxes just clips, per direct instruction: "even if the
 * window needed to be bigger". */
static int chai_chrome_h = 26;
static Elem chai_close_elem_storage;
static Elem *chai_close_elem = &chai_close_elem_storage;
static int chai_close_x, chai_close_y, chai_close_w, chai_close_h;

/* Top Settings area (direct instruction 2026-08-16: "not in chrome bar.
 * there is room below it. above chat") - the Sound toggle left the
 * bottom control row and now lives behind a Settings affordance in the
 * strip below the chrome bar / above the chat feed. The badge is a
 * static, synthetic element (like chai_close_elem - not parsed from
 * dashboard.chtpm): right-aligned in the tabbar row; when open
 * (chai_settings_open), it drops a Sound row just under the tabbar and the
 * content area shifts down to make room (see chai_layout_pass()'s
 * settings_strip math). Ports open-hai's Settings panel shape
 * (nav_add'd + hit-tested before the tree walk). */
static Elem chai_settings_elem_storage;
static Elem *chai_settings_elem = &chai_settings_elem_storage;
static Elem chai_settings_sound_elem_storage;
static Elem *chai_settings_sound_elem = &chai_settings_sound_elem_storage;
static int chai_settings_open = 0;
static int chai_settings_w;

static void chai_layout_pass(Elem *window) {
    /* REAL FIX 2026-08-15 (see chai_feed_dirty's own header comment for the
     * full story): this used to unconditionally rewind g_n_elems here,
     * EVERY call - now the rewind only happens inside the sidebar/panel
     * blocks below, and ONLY when their own dirty flag says a real
     * rebuild is needed. A keystroke-only chai_redraw touches g_n_elems not
     * at all, reusing whatever was already built. */
    chai_apply_css(window, 0);
    /* REAL FIX 2026-08-15 (direct report: "its on right of screen but
     * still wide and stout instead of thin and long" - after the first
     * screen-anchor attempt): chai_apply_css() just above re-reads
     * chat-hai.css's own fixed 900x700 into window->style EVERY call -
     * chai_layout_pass() runs on every chai_redraw(), so a one-time override set
     * in main() before the FIRST chai_layout_pass() call got silently
     * clobbered back to the CSS default on the very next chai_redraw
     * (relay input, a message arriving, anything). Force it here
     * instead, unconditionally, every call - chai_forced_win_w/h are set
     * once in main() from real screen dimensions and never touched by
     * CSS again. */
    if (chai_forced_win_w > 0) { window->style.has_width = 1; window->style.width = chai_forced_win_w; }
    if (chai_forced_win_h > 0) { window->style.has_height = 1; window->style.height = chai_forced_win_h; }
    window->x = 0; window->y = 0;
    int default_w = chai_scaled(900);
    int content_total_h = window->style.has_height ? window->style.height : chai_scaled(600);

    Elem *tabbar = find_by_tag(window, "tabbar");
    Elem *sidebar = find_by_tag(window, "sidebar");
    Elem *panel = find_by_tag(window, "panel");

    int tabbar_h = chai_scaled(30);
    chai_tabbar_h = tabbar_h;
    int tab_widths[MAX_CHILDREN];
    int tabbar_natural_w = chai_scaled(4);
    if (tabbar) {
        chai_apply_css(tabbar, 0);
        for (int i = 0; i < tabbar->n_children; i++) {
            Elem *tab = tabbar->children[i];
            tab->active = (i == chai_current_tab);
            chai_apply_css(tab, 0);
            /* real measured width, not a guessed px/char - a mismatched
             * guess vs. the font XftFontOpenName actually resolved was
             * the root cause of overlapping/"jumbled" tab labels.
             * chai_measure_text_px() already applies chai_font_scale internally,
             * so this only needs to scale its own fixed padding/badge
             * allowance, not the measured part. Measured in this own
             * pre-pass (not while assigning x) so the window can grow to
             * fit ALL tabs first - 15 tabs (au11-hq/rpg-maker-database.
             * html's real count) don't fit the old fixed 900px default,
             * and this app has no flex-wrap engine to fall back on. */
            tab_widths[i] = chai_measure_text_px(&tab->style, tab->label) + chai_scaled(34); /* "[>NN]" badge + padding */
            tabbar_natural_w += tab_widths[i] + 1;
        }
    }
    window->w = window->style.has_width ? window->style.width : (tabbar_natural_w > default_w ? tabbar_natural_w : default_w);
    window->h = content_total_h + chai_chrome_h;

    chai_close_w = chai_scaled(56); chai_close_h = chai_chrome_h - chai_scaled(6); /* wide enough for "[>NN] x" - badge + label both now, see chai_draw_elem()'s own comment */
    chai_close_x = window->w - chai_close_w - chai_scaled(4);
    chai_close_y = chai_scaled(3);

    if (tabbar) {
        tabbar->x = 0; tabbar->y = chai_chrome_h; tabbar->w = window->w; tabbar->h = tabbar_h;
        int tx = chai_scaled(4);
        for (int i = 0; i < tabbar->n_children; i++) {
            Elem *tab = tabbar->children[i];
            tab->x = tx; tab->y = chai_chrome_h + chai_scaled(2); tab->w = tab_widths[i]; tab->h = tabbar_h - chai_scaled(4);
            tx += tab_widths[i] + 1;
        }
    }

    /* Settings affordance (see the static elems' own header comment):
     * badge right-aligned in the tabbar row (room there - the 15 tab
     * labels overflow the thin 280px column off the right edge, so the
     * strip's own right end is always clear), Sound row in a reserved
     * strip under the tabbar while open. The badge's style is set here
     * too so chai_measure_text_px() sees the same font the draw path uses. */
    css_style_init(&chai_settings_elem->style);
    chai_settings_elem->style.has_bg_color = 1;
    snprintf(chai_settings_elem->style.bg_color, sizeof(chai_settings_elem->style.bg_color), "#2f3238");
    chai_settings_elem->style.has_border_color = 1;
    chai_settings_elem->style.has_border_width = 1;
    chai_settings_elem->style.border_width = 1;
    chai_settings_elem->style.has_fg_color = 1;
    snprintf(chai_settings_elem->style.fg_color, sizeof(chai_settings_elem->style.fg_color), "#eeeeee");
    chai_settings_elem->style.has_font_family = 1;
    snprintf(chai_settings_elem->style.font_family, sizeof(chai_settings_elem->style.font_family), "DejaVu Sans");
    chai_settings_elem->style.has_font_size = 1;
    chai_settings_elem->style.font_size = 10;
    chai_settings_w = chai_measure_text_px(&chai_settings_elem->style, "Settings") + chai_scaled(30);
    chai_settings_elem->x = window->w - chai_settings_w - chai_scaled(4);
    chai_settings_elem->y = chai_chrome_h + chai_scaled(2);
    chai_settings_elem->w = chai_settings_w;
    chai_settings_elem->h = tabbar_h - chai_scaled(4);
    snprintf(chai_settings_elem->id, sizeof(chai_settings_elem->id), "settingsbtn");
    snprintf(chai_settings_elem->tag, sizeof(chai_settings_elem->tag), "settingsbtn");

    int settings_strip_reserved = 0;
    if (chai_settings_open) {
        int strip_h = chai_scaled(26);
        int strip_top = chai_chrome_h + tabbar_h + chai_scaled(2);
        settings_strip_reserved = strip_h + chai_scaled(2);
        css_style_init(&chai_settings_sound_elem->style);
        chai_settings_sound_elem->style.has_bg_color = 1;
        snprintf(chai_settings_sound_elem->style.bg_color, sizeof(chai_settings_sound_elem->style.bg_color), "#2f3238");
        chai_settings_sound_elem->style.has_border_color = 1;
        chai_settings_sound_elem->style.has_border_width = 1;
        chai_settings_sound_elem->style.border_width = 1;
        chai_settings_sound_elem->style.has_fg_color = 1;
        snprintf(chai_settings_sound_elem->style.fg_color, sizeof(chai_settings_sound_elem->style.fg_color), "#eeeeee");
        chai_settings_sound_elem->style.has_font_family = 1;
        snprintf(chai_settings_sound_elem->style.font_family, sizeof(chai_settings_sound_elem->style.font_family), "DejaVu Sans");
        chai_settings_sound_elem->style.has_font_size = 1;
        chai_settings_sound_elem->style.font_size = 10;
        chai_settings_sound_elem->x = chai_scaled(4);
        chai_settings_sound_elem->y = strip_top;
        chai_settings_sound_elem->w = window->w - chai_scaled(8);
        chai_settings_sound_elem->h = strip_h;
        snprintf(chai_settings_sound_elem->id, sizeof(chai_settings_sound_elem->id), "sound-toggle");
        snprintf(chai_settings_sound_elem->tag, sizeof(chai_settings_sound_elem->tag), "button");
    }

    int content_y = chai_chrome_h + tabbar_h + settings_strip_reserved;
    int content_h = content_total_h - tabbar_h - settings_strip_reserved;
    /* REAL FIX 2026-08-15 (direct instruction: "sessions select can be
     * very small thin on right side not left") - was 210px on the LEFT
     * (matching open-hai's own sidebar, per this file's earlier "copy
     * open-hai" convention); reversed per direct instruction. Feed is
     * now the main LEFT body, sessions a thin strip on the window's own
     * RIGHT edge (the window itself is already screen-right-anchored,
     * see main()'s own window_x default - this is the right edge of
     * THIS window, one level in from that). */
    int sidebar_w = chai_scaled(90);

    if (chai_current_tab != CHAI_COMMON_EVENTS_TAB) {
        /* placeholder tabs: no sidebar/panel geometry needed, drawn as
         * one centered message directly against the window in render_pass() */
        return;
    }

    if (sidebar) {
        /* REAL FIX 2026-08-15 (see chai_feed_dirty's own header comment) -
         * only rebuild when the session list/active session actually
         * changed (chai_switch_session()/chai_create_new_session()/chai_delete_session()
         * set chai_sessions_dirty=1). Must still run BEFORE the panel block
         * below on any frame where it DOES rebuild, so sessions claim
         * the pool slots first - chai_n_elems_after_sessions is the
         * checkpoint the panel block rewinds to for its own dirty case. */
        if (chai_sessions_dirty) {
            g_n_elems = chai_n_elems_static;
            chai_inject_sessions(sidebar);
            chai_n_elems_after_sessions = g_n_elems;
            chai_sessions_dirty = 0;
        }
        chai_apply_css(sidebar, 0);
        if (sidebar->style.has_width && !sidebar->style.width_is_pct) sidebar_w = sidebar->style.width;
        /* REAL Stage 3 port (2026-08-16) - same clean §5.1b pattern #1
         * as db-hq/events-hq's own sidebars: column stack, uniform
         * padding, zero gap, fixed-height rows. Container gets its real
         * full box (window's right edge, not a margin-shrunk one) so
         * chai_draw_elem()'s background fill isn't left with a sliver, per
         * the bug class caught twice in db-hq/events-hq's own tabbars. */
        int item_h = chai_scaled(22);
        sidebar->style.has_display = 1; sidebar->style.display_flex = 1;
        sidebar->style.has_flex_direction = 1; sidebar->style.flex_row = 0;
        sidebar->style.has_padding = 1; sidebar->style.padding = chai_scaled(4);
        sidebar->style.has_gap = 1; sidebar->style.gap = 0;
        for (int i = 0; i < sidebar->n_children; i++) {
            Elem *item = sidebar->children[i];
            chai_apply_css(item, 0);
            item->h = item_h;
        }
        css_layout_pass(sidebar, window->w - sidebar_w, content_y, sidebar_w, content_h);
        /* REAL, GENERALIZED 2026-08-28 (Phase C target #2) - chat-hai's
         * own session sidebar had zero scroll support before this; a
         * long enough session list ran off the bottom with no way to
         * reach it. row_class=NULL - every direct child (real sessions +
         * "+ New Session") is a real row, same shape db-hq's own sidebar
         * already uses. */
        generic_scroll_layout_pass(sidebar, NULL, sidebar->y, content_h);
    } else {
        /* REAL, NEW 2026-08-28 - other chat-hai tabs (placeholder tabs,
         * see the early `return` a few lines above this whole block)
         * never reach here at all, so this path is dead in that case;
         * kept for the case a future tab DOES have a real sidebar/panel
         * layout without a scrollable list, so stale grid state from a
         * PRIOR frame's sidebar never bleeds into it. */
        g_pal_has_grid = 0;
    }

    if (panel) {
        chai_apply_css(panel, 0);
        int margin = chai_scaled(8);
        panel->x = margin; /* LEFT edge of the window now - was sidebar_w + margin when sidebar lived on the left */
        panel->y = content_y + margin;
        panel->w = window->w - sidebar_w - margin * 2;
        chai_panel_w = panel->w;
        panel->h = content_h - margin * 2;

        /* REAL FIX 2026-08-15 (chat-hai-design.md "Real layout spec",
         * ported from open-hai's real transcript_geom()/draw_composer()
         * shape — khtpm_css_parser.c has no flex/box-model engine, see
         * that file's own "unrecognized properties, ignored" comment,
         * so chat-hai.css's flex-based bottom-pin was decorative
         * fiction and this must be pixel-computed here instead):
         * bottom-pinned control rows have a FIXED height computed
         * FIRST, independent of feed content; the feed then gets
         * exactly what's left above them. This is why the feed
         * correctly shrinks around fixed controls instead of a
         * control accidentally claiming the whole pane (the original,
         * real bug — a message-only sidebar list plus an unbounded
         * generic per-child stack in the panel with no fixed row
         * heights read as "input takes up the entire vertical pane"). */
        /* REAL FIX 2026-08-15 (direct report: "i didn't see 'thinking/
         * typing' in view yet... why isn't it rendered?"): status used
         * to share ONE row with both toggle-pause AND speed-toggle,
         * reserving a fixed chai_scaled(174) for the two buttons - that math
         * was written when the window was still ~900px wide (§ROUND 1
         * of the screen-anchor fix). Once the window was narrowed to
         * ~280px unscaled (per "thin and long"), panel->w shrank to
         * ~218px real pixels - almost EXACTLY equal to that same fixed
         * 174*1.25=218px reservation, leaving status_elem's box at
         * effectively ZERO width. The typing indicator text was being
         * composed into the label correctly (confirmed via frame-
         * history's own typing= field) - it just had no visible box
         * left to draw into. Real fix: status gets its OWN full-width
         * row; toggle-pause + speed-toggle move to a SEPARATE row below
         * it, side by side. */
        int status_row_h = chai_scaled(20);
        int button_row_h = chai_scaled(22);
        int composer_row_h = chai_scaled(30);
        int row_gap = chai_scaled(4);
        int bottom_h = status_row_h + row_gap + button_row_h + row_gap + composer_row_h;
        int feed_top = panel->y + chai_scaled(4);
        int feed_h = panel->h - chai_scaled(4) - bottom_h;
        if (feed_h < 0) feed_h = 0;
        int item_h = chai_scaled(18);
        int n_visible = item_h > 0 ? feed_h / item_h : 0;

        /* REAL FIX 2026-08-15 (direct report: "typing... is really
         * slow" -> "why were not just using the same layout renderer...
         * merging both features?" - see chai_feed_dirty's own header
         * comment for the full reasoning): only re-wrap/rebuild the
         * feed when the underlying ledger content actually changed
         * (chai_feed_dirty set by the main loop's ledger-mtime poll, and
         * by chai_switch_session()/chai_send_composer()/chai_send_cli_prompt() after
         * their own chai_load_ledger() calls) - not on every chai_redraw. A
         * keystroke-only chai_redraw skips this entirely, reusing the
         * already-wrapped Elems from the last real rebuild - this is
         * the actual fix for the slow-typing report, not just the
         * font-cache/pixel-unpack fixes from earlier this session
         * (real, confirmed contributors, but not the dominant cost). */
        if (chai_feed_dirty) {
            g_n_elems = chai_n_elems_after_sessions;
            chai_inject_panel_feed(panel, n_visible);
            chai_feed_dirty = 0;
        }

        int iy = feed_top;
        int bottom_y = panel->y + panel->h - bottom_h;
        int button_row_y = bottom_y + status_row_h + row_gap;
        /* two buttons in the control row (toggle-pause, speed-toggle) -
         * the Sound toggle left for the top Settings area 2026-08-16
         * (direct instruction: "not in chrome bar. there is room below
         * it. above chat"), see the static settings elems' own header
         * comment. Two equal cells, one gap between, margins both sides:
         * 4 + cell + 4 + cell + 4. */
        int cell_w = (panel->w - chai_scaled(12)) / 2;
        for (int i = 0; i < panel->n_children; i++) {
            Elem *c = panel->children[i];
            chai_apply_css(c, 0);
            if (strcmp(c->tag, "item") == 0) {
                /* feed message row - stacked top-down, fills the space
                 * ABOVE the fixed bottom controls (see bottom_h above),
                 * never the reverse - this is the actual scrolling-feed
                 * area chat-hai was missing entirely before this fix
                 * (messages used to live in the sidebar instead, see
                 * chai_inject_sessions()'s own header comment). */
                c->x = panel->x + chai_scaled(4);
                c->y = iy;
                c->w = panel->w - chai_scaled(8);
                c->h = item_h;
                iy += item_h;
                continue;
            }
            if (c == chai_status_elem) {
                /* own full-width row now - see this block's own header
                 * comment for why it was invisible before. */
                c->x = panel->x + chai_scaled(4); c->y = bottom_y;
                c->w = panel->w - chai_scaled(8); c->h = status_row_h;
            } else if (c == chai_toggle_elem) {
                c->x = panel->x + chai_scaled(4); c->y = button_row_y;
                c->w = cell_w; c->h = button_row_h;
            } else if (c == chai_speed_elem) {
                /* REAL, working GUI speed control (direct instruction,
                 * 2026-08-15: "can have an input in gui also" - for
                 * chat_hai_config.pdl's sleep_between setting). Cycles
                 * fixed presets (see chai_activate_elem()'s own "speed-
                 * toggle" branch), writes chat_hai_config.pdl, which
                 * chat_hai_loop.sh re-reads every round (see that
                 * script's own sleep_between() function). */
                c->x = panel->x + chai_scaled(4) + cell_w + chai_scaled(4); c->y = button_row_y;
                c->w = cell_w; c->h = button_row_h;
            } else if (c == chai_composer_text_elem) {
                /* the real cli-io composer row, pinned to the true
                 * bottom of the window - same "input anchored at the
                 * bottom, everything else flows above it" contract
                 * open-hai's own composer uses. Full row width
                 * since the send button (direct instruction: "we dont
                 * need a send button") is gone. */
                c->x = panel->x + chai_scaled(4); c->y = bottom_y + status_row_h + row_gap + button_row_h + row_gap;
                c->w = panel->w - chai_scaled(8); c->h = composer_row_h;
            }
        }
    }
}

/* wraith-alpha-standard index nav (ops/wraith_parser_alpha.c's own
 * digit_accum/do_jump/display_num convention, direct instruction: "wraith
 * alpha should be a huge inspiration for this"): every interactive
 * element gets a sequential 1-based number, assigned in the same order
 * they're laid out (tabs, then - if Common Events is open - sidebar
 * items, then panel buttons). Must run AFTER chai_layout_pass() so it walks
 * exactly what's currently visible (placeholder tabs have no sidebar/
 * panel children to number). */
static void chai_assign_nav_indices(Elem *window) {
    g_n_nav = 0;
    /* Chrome close control is now LAST, not nav 1 (direct instruction,
     * 2026-08-12: "u can give close button last nav index if that
     * helps") - its "[>N]" badge is deliberately suppressed (see
     * chai_draw_elem()'s own comment, too small a box to fit one), so
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
    if (chai_current_tab == CHAI_COMMON_EVENTS_TAB) {
        /* REAL, NEW 2026-08-28 (Phase C target #2) - scroll arrows
         * numbered BEFORE the sidebar rows they control, same order
         * dbhq_assign_nav_indices() already uses. */
        if (g_pal_has_grid) {
            if (g_pal_arrow_up->onclick[0] && g_n_nav < MAX_ELEMS) {
                g_pal_arrow_up->nav_index = ++g_n_nav; g_nav[g_n_nav - 1] = g_pal_arrow_up;
            }
            if (g_pal_arrow_down->onclick[0] && g_n_nav < MAX_ELEMS) {
                g_pal_arrow_down->nav_index = ++g_n_nav; g_nav[g_n_nav - 1] = g_pal_arrow_down;
            }
        }
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
            /* REAL FIX 2026-08-15 (direct report: "open chat user input
             * is supposed to have nav [] and number so its relay and
             * human index accessible. why did we deviate from these
             * stds?") — this loop used to skip anything not tag
             * "button", which silently dropped composer-text (tag
             * "text") from ever getting a nav_index/[>]N badge, unlike
             * open-hai's own composer which IS a numbered
             * cli-io target. The blanket "buttons only" rule was
             * copied from the events-hq/db-hq template this file
             * started as (see this file's own header comment) where
             * the panel really does only ever contain buttons - never
             * updated when chat-hai's composer-text landed. Explicit
             * allowlist now: buttons AND the composer field, nothing
             * else (status text and feed message rows still correctly
             * get no nav_index - they're not digit-jump targets). */
            for (int i = 0; i < panel->n_children && g_n_nav < MAX_ELEMS; i++) {
                Elem *c = panel->children[i];
                int navigable = (strcmp(c->tag, "button") == 0) || (c == chai_composer_text_elem);
                if (!navigable) { c->nav_index = 0; continue; }
                c->nav_index = ++g_n_nav;
                g_nav[g_n_nav - 1] = c;
            }
        }
    }
    /* Settings badge (+ its Sound row while the panel is open) come
     * before Close, matching open-hai's own Settings-before-Close nav
     * ordering - always reachable by digit even with the panel closed. */
    if (g_n_nav < MAX_ELEMS) {
        chai_settings_elem->nav_index = ++g_n_nav;
        g_nav[g_n_nav - 1] = chai_settings_elem;
    }
    if (chai_settings_open && g_n_nav < MAX_ELEMS) {
        chai_settings_sound_elem->nav_index = ++g_n_nav;
        g_nav[g_n_nav - 1] = chai_settings_sound_elem;
    }
    if (g_n_nav < MAX_ELEMS) {
        chai_close_elem->nav_index = ++g_n_nav;
        g_nav[g_n_nav - 1] = chai_close_elem;
    }
    if (g_focus_nav < 1) g_focus_nav = 1;
    if (g_focus_nav > g_n_nav) g_focus_nav = g_n_nav > 0 ? g_n_nav : 1;
}

/* ---------- rendering ---------- */


static unsigned long chai_alloc_pixel(const char *spec) {
    if (!spec || !spec[0]) return BlackPixel(dpy, screen);
    XColor c;
    if (spec[0] == '#') {
        if (XParseColor(dpy, cmap, spec, &c) && XAllocColor(dpy, cmap, &c)) return c.pixel;
    } else if (XAllocNamedColor(dpy, cmap, spec, &c, &c)) {
        return c.pixel;
    }
    return BlackPixel(dpy, screen);
}

static XftColor chai_xft_color(const char *spec) {
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

/* REAL FIX 2026-08-15 (direct report: "its not as fast as open-hai" -
 * checked open-hai's own font handling directly rather than guessing:
 * it opens each font ONCE at startup into persistent globals
 * (font_ui/font_small/font_mono) and reuses them for the app's entire
 * lifetime - only 5 total XftFontOpenName calls in that whole file.
 * chai_font_for() here used to open+close a font for EVERY drawn element,
 * EVERY chai_redraw - with 40-80+ visible feed lines, that's 40-80+ open/
 * close pairs per keystroke, on top of the wrap-path fix already
 * landed this session. Small cache, same "keyed by spec string" shape
 * as chai_measure_text_px()'s own fix - the caller no longer closes the
 * returned font (see chai_draw_elem()'s own call site, XftFontClose removed
 * there), matching open-hai's own "never close, just reuse" pattern. */
static XftFont *chai_font_for(const CssStyle *st) {
    char spec[128];
    const char *fam = st->has_font_family ? st->font_family : "DejaVu Sans";
    int size = chai_scaled(st->has_font_size ? st->font_size : 12);
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

static void chai_draw_elem(Elem *e, int hover_id_hash) {
    (void)hover_id_hash;
    if (e->style.has_bg_color) {
        XSetForeground(dpy, gc, chai_alloc_pixel(e->style.bg_color));
        XFillRectangle(dpy, buf, gc, e->x, e->y, e->w, e->h);
    }
    if (e->style.has_border_color) {
        XSetForeground(dpy, gc, chai_alloc_pixel(e->style.border_color));
        int bw = e->style.has_border_width ? e->style.border_width : 1;
        for (int i = 0; i < bw; i++)
            XDrawRectangle(dpy, buf, gc, e->x + i, e->y + i, e->w - 1 - 2 * i, e->h - 1 - 2 * i);
    }
    if (strcmp(e->tag, "tab") == 0 && e->active && !e->style.has_bg_color) {
        XSetForeground(dpy, gc, chai_alloc_pixel("#ffffff"));
        XFillRectangle(dpy, buf, gc, e->x, e->y, e->w, e->h);
    }
    /* REAL BUG FOUND + DISABLED 2026-08-15 (direct report: "why is last
     * chat highlighted? i cant read it. get rid of that for now") - the
     * newest ledger line auto-becomes chai_selected_event on every load
     * (see chai_load_ledger()'s own callers), which set item->active=1 on
     * that message's Elem(s), which painted this LIGHT BLUE (#cce5ff)
     * background behind it - but feed message text color comes from
     * per-speaker CSS classes (chat-hai.css's .data-item.<speaker>
     * rules), all light/pastel colors chosen to read against the app's
     * DARK background (#16181f/#1e2130). Light pastel text on a light
     * blue highlight = unreadable, every single time (always the newest
     * message, since that's what auto-selects). Disabled per direct
     * instruction rather than reworked - a real "selected message" UI
     * (if wanted later) needs a per-speaker-aware contrast choice, not
     * a single fixed highlight color; not in scope right now. */
    /* wraith-alpha-standard focus ring: the currently-focused navigable
     * element gets a highlighted outline, matching wraith_parser_alpha.c's
     * "[>]" focus prefix convention (adapted to a visible box here since
     * this is a graphical renderer, not the text-grid wraith-alpha draws
     * into). */
    if (e->nav_index > 0 && e->nav_index == g_focus_nav) {
        XSetForeground(dpy, gc, chai_alloc_pixel("#ff8c00"));
        XDrawRectangle(dpy, buf, gc, e->x - 1, e->y - 1, e->w + 1, e->h + 1);
    }
    int pad = e->style.has_padding ? e->style.padding : 4;
    int label_x = e->x + pad;
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
     * the right" fix). Real fix is a wider box (chai_close_w, see
     * chai_layout_pass()) and a shorter label ("x" not "[x]", since the
     * badge itself now supplies the brackets) instead of an exception -
     * every nav item gets a number, no special cases. */
    if (e->nav_index > 0) {
        char badge[16];
        int focused = (e->nav_index == g_focus_nav);
        /* REAL FIX 2026-08-12, direct correction ("db-hq and hai are
         * using nav index in not quite the std the std is [].<#> not
         * [<#>]"): verified against the actual real reference
         * (1.TPMOS_c_+rmmp.0103.0001/projects/wraith-alpha/ops/
         * wraith_parser_alpha.c ~line 2221-2224/2283) - the bracket
         * holds ONLY the state glyph (`[^]`/`[>]`/`[]`/`[ ]`), the
         * number is a SEPARATE suffix drawn after the closing bracket
         * with a trailing period (`pref + "%d." `, e.g. `[>]1.`), NOT
         * embedded inside the brackets as `[>1]`. This was wrong
         * everywhere in this house's own khtpm/-hq family until now -
         * see !.HOUSE_STDS.md #22's own correction for why this must
         * not drift back. */
        /* "^" activation glyph (2026-08-16, see chai_require_cli_activation's
         * own header comment): the composer specifically shows "^" once
         * activated, matching legacy chtpm_parser.c's cli-io convention
         * ("the selector will need to be '^' activated") - every other
         * navigable element keeps the plain "[>]" wraith-alpha focus
         * marker unchanged. */
        char state_ch = ' ';
        if (focused) state_ch = (e == chai_composer_text_elem && chai_composer_activated) ? '^' : '>';
        snprintf(badge, sizeof(badge), "[%c]%d.", state_ch, e->nav_index);
        char numspec[48];
        snprintf(numspec, sizeof(numspec), "DejaVu Sans Mono:pixelsize=%d", chai_scaled(9));
        XftFont *numfont = XftFontOpenName(dpy, screen, numspec);
        if (!numfont) { snprintf(numspec, sizeof(numspec), "DejaVu Sans:pixelsize=%d", chai_scaled(9)); numfont = XftFontOpenName(dpy, screen, numspec); }
        XftColor numcol = chai_xft_color(focused ? "#ff8c00" : "#9a9a9a");
        XGlyphInfo numext;
        XftTextExtentsUtf8(dpy, numfont, (const FcChar8 *)badge, (int)strlen(badge), &numext);
        int numy = e->y + (e->h + numfont->ascent - numfont->descent) / 2;
        XftDrawStringUtf8(xftdraw_buf, &numcol, numfont, label_x, numy, (const FcChar8 *)badge, (int)strlen(badge));
        XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &numcol);
        label_x += numext.width + 5;
        XftFontClose(dpy, numfont);
    }
    if (e->label[0]) {
        /* REAL FIX 2026-08-16 (truncation report: "text is sometimes
         * getting forward truncated, recolored"): clip Xft drawing to
         * the element's bounding box so overflow text (from any cause)
         * is hidden rather than bleeding across neighboring elements.
         * Without this, text that exceeds e->w draws unclipped across
         * the sidebar or past the window edge — appearing "recolored"
         * when it hits a different background. */
        Region label_clip = XCreateRegion();
        XRectangle label_rect = { (short)e->x, (short)e->y, (unsigned short)e->w, (unsigned short)e->h };
        XUnionRectWithRegion(&label_rect, label_clip, label_clip);
        XftDrawSetClip(xftdraw_buf, label_clip);
        XDestroyRegion(label_clip);
        /* chai_font_for() returns a CACHED, shared font now (see its own
         * header comment) - do not close it here, closing would
         * invalidate the cache for every other element drawn this same
         * frame, undoing the whole fix. */
        XftFont *font = chai_font_for(&e->style);
        XftColor col = chai_xft_color(e->style.has_fg_color ? e->style.fg_color : "#000000");
        XGlyphInfo extents;
        XftTextExtentsUtf8(dpy, font, (const FcChar8 *)e->label, (int)strlen(e->label), &extents);
        int ty = e->y + (e->h + font->ascent - font->descent) / 2;
        if (ty < e->y + font->ascent) ty = e->y + font->ascent + pad / 2;
        XftDrawStringUtf8(xftdraw_buf, &col, font, label_x, ty, (const FcChar8 *)e->label, (int)strlen(e->label));
        XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &col);
        XftDrawSetClip(xftdraw_buf, NULL); /* reset — other elements draw unclipped */
    }
}

/* absolute-positioned children (the floating block-title) are painted in
 * a later pass than their parent, per the design doc's own suggested
 * approach - this walk draws non-title children first, titles last. */
static void chai_render_tree(Elem *e, int depth) {
    if (depth == 0) chai_draw_elem(e, 0);
    for (int i = 0; i < e->n_children; i++) {
        Elem *c = e->children[i];
        if (strcmp(c->tag, "title") == 0) continue; /* deferred */
        if (strcmp(c->tag, "module") == 0) continue; /* pure config, never visual - see apply_attr()'s src= comment */
        chai_draw_elem(c, 0);
        chai_render_tree(c, depth + 1);
    }
    for (int i = 0; i < e->n_children; i++) {
        Elem *c = e->children[i];
        if (strcmp(c->tag, "title") == 0) chai_draw_elem(c, 0);
    }
}

static void chai_render_placeholder_tab(Elem *window) {
    char pspec[48];
    snprintf(pspec, sizeof(pspec), "DejaVu Sans:pixelsize=%d", chai_scaled(12));
    XftFont *font = XftFontOpenName(dpy, screen, pspec);
    XftColor col = chai_xft_color("#888888");
    char msg[64];
    snprintf(msg, sizeof(msg), "%s — (coming soon)", CHAI_TAB_LABELS[chai_current_tab]);
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
 * chai_handle_key()'s own logic being correct and already proven working
 * through the relay (which bypasses X input focus entirely, so it never
 * hit this). Fix is the SAME proven raise-then-focus-then-flush sequence
 * already used by khtpm_strip_parser.c's taskbar_soft_focus() - ported,
 * not reinvented, per that bug report's own explicit standard ("don't
 * invent a fresh focus mechanism without first checking whether an
 * already-proven pattern solves it").
 *
 * DIAGNOSTIC (also ported, khtpm_strip_parser.c's own chai_has_real_focus):
 * XSetInputFocus() is a REQUEST, not a guarantee - this tracks whether
 * the window ACTUALLY has focus right now via real FocusIn/FocusOut
 * events, the only authoritative source. If this never goes true despite
 * chai_soft_focus() being called, KeyPress events genuinely never reach this
 * process - a different, deeper problem than db-hq's own key-handling
 * logic (which is separately already proven correct via the relay). */
static int chai_has_real_focus = 0;

static void chai_soft_focus(void) {
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
static void chai_grab_keyboard_retry(void) {
    for (int attempt = 0; attempt < 5; attempt++) {
        int rc = XGrabKeyboard(dpy, win, True, GrabModeAsync, GrabModeAsync, CurrentTime);
        if (rc == GrabSuccess) break;
        XSync(dpy, False);
        usleep(5000);
    }
}

static Elem *g_window;

/* RGB compose→present refactor (2026-08-12, direct instruction: "we
 * should do db to rgb refactor. the need being auditability"). Proven
 * first on a throwaway test binary (!.khtpm-rgb-refactor.md's own
 * "Phase 0" - compose buffer vs. presented-window readback confirmed
 * BYTE-IDENTICAL two independent ways before trusting this pattern on
 * real code). `chai_redraw()` composes into `buf` (the offscreen Pixmap) via
 * Xft/Xlib, then presents via `XGetImage`+`XPutImage` (proven
 * pixel-identical to the old `XCopyArea` path in Phase 0).
 *
 * REAL FIX 2026-08-15 (direct report: "typing... is really slow" -
 * see chai_dump_frame_png()'s own header comment for the full story): this
 * used to ALSO keep a persistent `g_frame_rgb` byte-buffer copy,
 * rebuilt via a per-pixel `XGetPixel` loop on EVERY chai_redraw (every
 * keystroke) so `chai_dump_frame_png()` could write it out without its own
 * capture. That per-pixel unpacking is real, non-trivial cost paid on
 * the hot path for a feature (debug PNG dump) only used occasionally -
 * removed from here; `chai_dump_frame_png()` now does its own on-demand
 * capture+unpack instead, matching open-hai's own real, proven-fast
 * `chai_dump_frame_png()` shape (checked directly, not assumed). */

/* debug PNG dump - see the header comment above the stb_image_write.h
 * include. RGB refactor (2026-08-12): writes the single persistent
 * `g_frame_rgb` buffer chai_redraw() already derived for the real on-screen
 * present - no separate XGetImage capture of its own anymore. This IS
 * the auditability point of the refactor: what gets dumped is
 * byte-for-byte the same buffer that was actually presented, not a
 * fresh, possibly-different second capture. Bound to 'p' - not part of
 * the normal render loop, purely an on-demand debug aid. */
/* REAL FIX 2026-08-15 (direct report: "typing... is really slow", then
 * confirmed "still slower than expected" even after the font-cache fix
 * - direct question: "how does the openhai application achieve normal
 * input latency... since the 2 are very similar" - real answer found by
 * checking khtpm_open_hai_render.c directly, not assumed): chai_redraw() used
 * to unconditionally rebuild a full RGB byte buffer (g_frame_rgb) via
 * XGetPixel() in a per-pixel double loop, EVERY SINGLE REDRAW - every
 * keystroke calls chai_redraw() (chai_handle_key()'s printable-char branch).
 * XGetPixel is a real, non-trivial per-call cost (function-call
 * overhead, no batch access) - for a ~350x1500px window that's ~500k
 * XGetPixel calls on every keypress. open-hai's own real chai_redraw() does
 * NOT do this - it only does XGetImage->XPutImage->XDestroyImage (cheap,
 * no per-pixel unpacking) every frame, and reserves the expensive
 * per-pixel unpack for its OWN chai_dump_frame_png() - called on-demand
 * (the 'p' debug key), never on the hot path. Fixed to match: chai_dump_frame_png()
 * now does its own FRESH on-demand XGetImage + unpack here, instead of
 * reading a cache chai_redraw() no longer maintains. g_frame_rgb/g_frame_w/
 * g_frame_h are gone - this function is now self-contained, matching
 * open-hai's own chai_dump_frame_png() shape exactly. */
static void chai_redraw(void);
static void chai_dump_frame_png(void) {
    /* REAL FIX (2026-08-27) - same class of bug as dump_frame_png()'s
     * own header comment describes for db-hq/events-hq: `buf` only
     * holds whatever chai_redraw() drew on the PREVIOUS tick unless
     * forced fresh here first. */
    chai_redraw();
    int w = g_window->w, h = g_window->h;
    XImage *img = XGetImage(dpy, buf, 0, 0, (unsigned)w, (unsigned)h, AllPlanes, ZPixmap);
    if (!img) { fprintf(stderr, "chat-hai: chai_dump_frame_png: XGetImage failed\n"); return; }
    unsigned char *rgb = malloc((size_t)w * h * 3);
    if (!rgb) { XDestroyImage(img); fprintf(stderr, "chat-hai: chai_dump_frame_png: malloc failed\n"); return; }
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
    int ok = stbi_write_png("/tmp/chat-hai-frame.png", w, h, 3, rgb, w * 3);
    free(rgb);
    /* Receipt: plain key=value for agents that can't see images.
     * Format: same as ai-cell's receipt per testing guide. */
    {
        FILE *r = fopen("/tmp/chat-hai-frame.png.receipt.txt", "w");
        if (r) {
            time_t now = time(NULL);
            char ts[32]; strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&now));
            fprintf(r, "ok=%d\n", ok);
            fprintf(r, "w=%d\nh=%d\n", w, h);
            fprintf(r, "timestamp=%s\n", ts);
            fprintf(r, "font_scale=%.2f\n", chai_font_scale);
            fprintf(r, "feed_font_css=%d\n", chai_feed_font_css);
            fprintf(r, "feed_font_rendered=%d (=font_css * font_scale)\n", (int)(chai_feed_font_css * chai_font_scale));
            fprintf(r, "panel_w=%d\n", chai_panel_w);
            fprintf(r, "wrap_w=%d\n", chai_wrap_w);
            fprintf(r, "chrome_h=%d\n", chai_chrome_h);
            fprintf(r, "tabbar_h=%d (base=30)\n", chai_tabbar_h);
            fprintf(r, "settings_open=%d\n", chai_settings_open);
            fprintf(r, "sound_on=%d\n", chai_sound_on);
            fprintf(r, "n_events=%d\n", chai_n_events);
            fprintf(r, "n_sessions=%d\n", chai_n_sessions);
            fprintf(r, "active_session=%s\n", chai_active_session[0] ? chai_active_session : "-");
            fprintf(r, "n_nav=%d\n", g_n_nav);
            fprintf(r, "focus_nav=%d\n", g_focus_nav);
            fprintf(r, "paused=%d\n", chai_paused);
            fprintf(r, "win_x=%d\nwin_y=%d\n", chai_win_x, chai_win_y);
            /* First 3 feed items' geometry — lets agents verify per-element
             * positioning and clip math without needing to see the image. */
            Elem *panel_dbg = g_window ? find_by_tag(g_window, "panel") : NULL;
            if (panel_dbg) {
                int fcount = 0;
                for (int i = 0; i < panel_dbg->n_children && fcount < 3; i++) {
                    Elem *c = panel_dbg->children[i];
                    if (strcmp(c->tag, "item") != 0) continue;
                    int my_pad = c->style.has_padding ? c->style.padding : 4;
                    fprintf(r, "feed[%d]: x=%d w=%d h=%d pad=%d label_x=%d nav=%d label=%.40s\n",
                            fcount, c->x, c->w, c->h, my_pad, c->x + my_pad, c->nav_index, c->label);
                    fcount++;
                }
            }
            fclose(r);
        }
    }
    fprintf(stderr, ok ? "chat-hai: wrote /tmp/chat-hai-frame.png (%dx%d)\n" : "chat-hai: chai_dump_frame_png: write failed\n", w, h);
}

/* Own chrome bar (title + close) - see chai_layout_pass()'s CHROME_H comment
 * for the wraith-alpha precedent. Drawn unconditionally, every chai_redraw,
 * regardless of which tab is open - matches wraith-alpha's own chrome
 * row staying fixed while body content underneath changes. */
static void chai_draw_chrome_bar(void) {
    XSetForeground(dpy, gc, chai_alloc_pixel("#2b2b2b"));
    XFillRectangle(dpy, buf, gc, 0, 0, g_window->w, chai_chrome_h);

    char tspec[48];
    snprintf(tspec, sizeof(tspec), "DejaVu Sans:pixelsize=%d:bold", chai_scaled(10));
    XftFont *titlefont = XftFontOpenName(dpy, screen, tspec);
    if (!titlefont) { snprintf(tspec, sizeof(tspec), "DejaVu Sans:pixelsize=%d", chai_scaled(10)); titlefont = XftFontOpenName(dpy, screen, tspec); }
    XftColor titlecol = chai_xft_color("#eeeeee");
    /* legacy taskbar's own "^" convention (direct instruction 2026-08-12:
     * "legacy toolbar had a '^' indicator near digits, i noticed we lost
     * that but we could add it here" / "'^' indicating window had
     * focus") - real, ground-truth chai_has_real_focus (set only by an
     * actual FocusIn event, "the only authoritative source" per
     * khtpm_strip_parser.c's own F-19 diagnostic this was ported from),
     * not a guess or a request-was-sent flag. */
    char title[16];
    snprintf(title, sizeof(title), "chat-hai %s", chai_has_real_focus ? "^" : " ");
    int ty = (chai_chrome_h + titlefont->ascent - titlefont->descent) / 2;
    XftDrawStringUtf8(xftdraw_buf, &titlecol, titlefont, chai_scaled(8), ty, (const FcChar8 *)title, (int)strlen(title));
    XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &titlecol);
    XftFontClose(dpy, titlefont);

    chai_close_elem->x = chai_close_x; chai_close_elem->y = chai_close_y;
    chai_close_elem->w = chai_close_w; chai_close_elem->h = chai_close_h;
    snprintf(chai_close_elem->label, sizeof(chai_close_elem->label), "x");
    css_style_init(&chai_close_elem->style);
    chai_close_elem->style.has_border_color = 1;
    snprintf(chai_close_elem->style.border_color, sizeof(chai_close_elem->style.border_color), "%s",
             chai_close_elem->nav_index == g_focus_nav ? "#ff8c00" : "#888888");
    chai_close_elem->style.has_border_width = 1; chai_close_elem->style.border_width = 1;
    chai_close_elem->style.has_fg_color = 1;
    snprintf(chai_close_elem->style.fg_color, sizeof(chai_close_elem->style.fg_color), "#eeeeee");
    chai_draw_elem(chai_close_elem, 0);

    /* Debug status line, direct request 2026-08-12 ("we could show
     * digits in header like tb") - shows the last raw key this PROCESS
     * actually received and the current digit accumulator, live, so
     * it's visually obvious (not just in a log file) whether a real
     * keypress ever reaches this window at all vs. reaches it but
     * doesn't visibly move focus for some other reason - two very
     * different bugs that look identical from the outside otherwise. */
    char dbg[96];
    snprintf(dbg, sizeof(dbg), "Key:%s  Digits:%d  Focus:%d/%d  RealFocus:%s",
             chai_last_key_label[0] ? chai_last_key_label : "(none yet)",
             chai_digit_accum, g_focus_nav, g_n_nav, chai_has_real_focus ? "yes" : "no");
    char dspec[48];
    snprintf(dspec, sizeof(dspec), "DejaVu Sans:pixelsize=%d", chai_scaled(9));
    XftFont *dfont = XftFontOpenName(dpy, screen, dspec);
    if (dfont) {
        XftColor dcol = chai_xft_color("#88cc88");
        XGlyphInfo dext;
        XftTextExtentsUtf8(dpy, dfont, (const FcChar8 *)dbg, (int)strlen(dbg), &dext);
        int dx = g_window->w - chai_close_w - chai_scaled(12) - dext.width;
        int dy = (chai_chrome_h + dfont->ascent - dfont->descent) / 2;
        XftDrawStringUtf8(xftdraw_buf, &dcol, dfont, dx, dy, (const FcChar8 *)dbg, (int)strlen(dbg));
        XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &dcol);
        XftFontClose(dpy, dfont);
    }
}

/* Top Settings area (see the static elems' own header comment): the
 * Settings badge in the tabbar row, and - while open - the Sound row in
 * the reserved strip under the tabbar. Both are static/synthetic like
 * chai_close_elem, so they're drawn here AFTER chai_render_tree() (which only
 * walks the parsed window tree) and hit-tested in chai_handle_click() before
 * the tree walk. The focused/open border uses the same "#ff8c00" the
 * focus ring uses. */
static void chai_draw_settings_bar(void) {
    snprintf(chai_settings_elem->style.border_color, sizeof(chai_settings_elem->style.border_color), "%s",
             (chai_settings_open || chai_settings_elem->nav_index == g_focus_nav) ? "#ff8c00" : "#666666");
    snprintf(chai_settings_elem->label, sizeof(chai_settings_elem->label), "Settings");
    chai_draw_elem(chai_settings_elem, 0);
    if (chai_settings_open) {
        snprintf(chai_settings_sound_elem->style.border_color, sizeof(chai_settings_sound_elem->style.border_color), "%s",
                 chai_settings_sound_elem->nav_index == g_focus_nav ? "#ff8c00" : "#666666");
        snprintf(chai_settings_sound_elem->label, sizeof(chai_settings_sound_elem->label), "Sound: %s",
                 chai_sound_on ? "on" : "off");
        chai_draw_elem(chai_settings_sound_elem, 0);
    }
}

/* REAL FIX 2026-08-15 (direct instruction: "you should check it with
 * injection and framehistory.txt (we dont need a png dump to see if
 * frames are updating from chat)") — matches the exact convention
 * khtpm_strip_parser.c already uses for the taskbar
 * (#.desktop/khtpm_strip_frame_history.txt): one text line appended
 * per chai_redraw(), so an agent (or a human) can `tail -f` real state
 * (event count, active session, paused flag, last message) without
 * ever needing a screenshot. This is genuinely faster to verify against
 * than a PNG dump for anything that's really a DATA question ("is the
 * feed updating") rather than a real LAYOUT question ("does it look
 * right") - use this for the former, PNG dumps for the latter. */
static void chai_append_frame_history(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/chat_hai_frame_history.txt", g_house_root);
    FILE *f = fopen(path, "a");
    if (!f) return;
    const char *last = chai_n_events > 0 ? chai_events[chai_n_events - 1] : "";
    char last_short[80];
    snprintf(last_short, sizeof(last_short), "%.60s", last);
    fprintf(f, "session=%s n_events=%d paused=%d typing=%s focus_nav=%d/%d sound=%d settings=%d win_x=%d win_y=%d win_w=%d win_h=%d last=\"%s\"\n",
            chai_active_session, chai_n_events, chai_paused, chai_typing_name[0] ? chai_typing_name : "-", g_focus_nav, g_n_nav, chai_sound_on,
            chai_settings_open, chai_win_x, chai_win_y,
            g_window ? g_window->w : 0, g_window ? g_window->h : 0, last_short);
    fclose(f);
}

static void chai_redraw(void) {
    chai_layout_pass(g_window);
    chai_assign_nav_indices(g_window);
    XSetForeground(dpy, gc, chai_alloc_pixel(g_window->style.has_bg_color ? g_window->style.bg_color : "#ececec"));
    /* REAL FIX 2026-08-28 (live corruption found testing Phase 2's
     * frame-file paint) - clearing only g_window->w/h leaves stale
     * pixels visible whenever content SHRINKS between redraws (a
     * taller previous session's leftover rows) - the backing Pixmap
     * only ever GROWS (see g_buf_w/g_buf_h's own header comment),
     * it never shrinks back down, so clearing less than the real
     * allocated buffer leaves old content sitting below the new,
     * smaller content. Clear the FULL allocated buffer every time. */
    XFillRectangle(dpy, buf, gc, 0, 0, (unsigned)(g_buf_w > g_window->w ? g_buf_w : g_window->w), (unsigned)(g_buf_h > g_window->h ? g_buf_h : g_window->h));
    if (chai_current_tab != CHAI_COMMON_EVENTS_TAB) {
        Elem *tabbar = find_by_tag(g_window, "tabbar");
        if (tabbar) { chai_draw_elem(tabbar, 0); chai_render_tree(tabbar, 1); }
        chai_render_placeholder_tab(g_window);
    } else {
        chai_render_tree(g_window, 0);
    }
    /* REAL, NEW 2026-08-28 (Phase C target #2) - chat-hai has its OWN
     * redraw path (chai_render_tree()/chai_draw_elem()), entirely separate
     * from db-hq's dbhq_redraw_content() - replicated here, same
     * geometry generic_scroll_layout_pass() already computed. */
    if (g_pal_has_grid && g_pal_track_h > 0) {
        XSetForeground(dpy, gc, chai_alloc_pixel("#2a2a2a"));
        XFillRectangle(dpy, buf, gc, g_pal_track_x, g_pal_track_y, (unsigned)g_pal_track_w, (unsigned)g_pal_track_h);
        XSetForeground(dpy, gc, chai_alloc_pixel("#888888"));
        XFillRectangle(dpy, buf, gc, g_pal_track_x + chai_scaled(1), g_pal_thumb_y,
                       (unsigned)(g_pal_track_w - chai_scaled(2)), (unsigned)g_pal_thumb_h);
        int ax = g_pal_track_x, aw = g_pal_track_w;
        int up_y0 = g_pal_track_y - g_pal_arrow_h;
        int down_y0 = g_pal_track_y + g_pal_track_h;
        int up_enabled = !g_pal_arrow_up_disabled;
        int down_enabled = !g_pal_arrow_down_disabled;
        XSetForeground(dpy, gc, chai_alloc_pixel("#3a3a3a"));
        XFillRectangle(dpy, buf, gc, ax, up_y0, (unsigned)aw, (unsigned)g_pal_arrow_h);
        XFillRectangle(dpy, buf, gc, ax, down_y0, (unsigned)aw, (unsigned)g_pal_arrow_h);
        XSetForeground(dpy, gc, chai_alloc_pixel(up_enabled ? "#cccccc" : "#555555"));
        XPoint up_tri[3] = {
            { (short)(ax + aw / 2), (short)(up_y0 + chai_scaled(3)) },
            { (short)(ax + chai_scaled(2)), (short)(up_y0 + g_pal_arrow_h - chai_scaled(3)) },
            { (short)(ax + aw - chai_scaled(2)), (short)(up_y0 + g_pal_arrow_h - chai_scaled(3)) },
        };
        XFillPolygon(dpy, buf, gc, up_tri, 3, Convex, CoordModeOrigin);
        XSetForeground(dpy, gc, chai_alloc_pixel(down_enabled ? "#cccccc" : "#555555"));
        XPoint down_tri[3] = {
            { (short)(ax + aw / 2), (short)(down_y0 + g_pal_arrow_h - chai_scaled(3)) },
            { (short)(ax + chai_scaled(2)), (short)(down_y0 + chai_scaled(3)) },
            { (short)(ax + aw - chai_scaled(2)), (short)(down_y0 + chai_scaled(3)) },
        };
        XFillPolygon(dpy, buf, gc, down_tri, 3, Convex, CoordModeOrigin);
        chai_draw_elem(g_pal_arrow_up, 0);
        chai_draw_elem(g_pal_arrow_down, 0);
    }
    chai_draw_chrome_bar();
    chai_draw_settings_bar();

    /* REAL FIX 2026-08-15 (see chai_dump_frame_png()'s own header comment for
     * the full story - this used to also rebuild a full RGB byte buffer
     * here via a per-pixel XGetPixel loop, on EVERY chai_redraw/keystroke;
     * that unpacking now only happens on-demand inside chai_dump_frame_png()
     * itself, matching open-hai's real, proven-fast chai_redraw() shape:
     * present via XGetImage->XPutImage only, no per-pixel unpacking on
     * the hot path). */
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
    chai_append_frame_history();
}

/* ---------- hit testing / click dispatch ---------- */

/* hit_test() now comes from khtpm_render_core.h (Stage 2a, 2026-08-16). */

/* Composer: the user's jump-in line. Typed chars accumulate here (the
 * panel <text id="composer-text"> mirrors it); Enter appends the line to
 * the master-ledger formula and the next persona turn answers it. */
#define CHAI_COMPOSER_BUF 128
static char chai_composer[CHAI_COMPOSER_BUF] = "";
static int chai_composer_len = 0;

static void chai_composer_sync(void) {
    /* REAL FIX 2026-08-15: was find_by_tag(g_window, "text"), which
     * matches the FIRST "text"-tagged element in document order — that's
     * "status", not "composer-text" (both share tag "text"). This
     * silently mutated the status line instead of the composer every
     * keystroke. See find_by_id()'s own header comment. */
    if (chai_composer_text_elem) snprintf(chai_composer_text_elem->label, sizeof(chai_composer_text_elem->label), "> %s_", chai_composer);
}

static void chai_send_composer(void) {
    while (chai_composer_len > 0 && chai_composer[chai_composer_len - 1] == ' ') chai_composer_len--;
    chai_composer[chai_composer_len] = '\0';
    if (chai_composer_len == 0) { chai_composer_sync(); chai_redraw(); return; }
    char t[32];
    time_t now = time(NULL);
    struct tm *tmv = localtime(&now);
    strftime(t, sizeof(t), "%Y-%m-%d %H:%M:%S", tmv);
    char ledger[PATH_BUF];
    chai_session_ledger_path(ledger, sizeof(ledger), chai_active_session);
    FILE *f = fopen(ledger, "a");
    if (f) {
        fprintf(f, "[%s] user: %s | Trigger: chat-hai\n", t, chai_composer);
        fclose(f);
    }
    chai_composer_len = 0;
    chai_composer[0] = '\0';
    chai_load_ledger();
    if (chai_n_events > 0) chai_selected_event = chai_n_events - 1;
    /* Feed re-injection happens inside chai_layout_pass() (called by chai_redraw()
     * below), from the current chai_events/chai_n_events just reloaded above -
     * but ONLY when chai_feed_dirty is set (see that flag's own header
     * comment, real fix 2026-08-15 for the slow-typing report) - must
     * set it explicitly here since new content just really did arrive. */
    chai_feed_dirty = 1;
    chai_composer_sync();
    chai_redraw();
}

/* PRE-EXISTING BUG FOUND 2026-08-15: chai_send_cli_prompt() referenced an
 * undeclared chai_cli_prompts[] and is never called from anywhere in this
 * file - the binary running earlier this session was stale (built
 * before this dead code landed, never rebuilt since; see
 * chat-hai-design.md's own layout-fix section for the same "always
 * fully rebuild+restart" lesson). Declaring the missing array (all
 * unset for now) is the minimal fix to make this compile again; wiring
 * real cli-io quick-prompts (digits 1-9, like open-hai's own
 * numbered-shortcut composer) is unstarted, separate future work. */
static const char *chai_cli_prompts[10] = {0};

static void chai_send_cli_prompt(int digit) {
    if (digit < 1 || digit > 9 || !chai_cli_prompts[digit]) return;
    char t[32];
    time_t now = time(NULL);
    struct tm *tmv = localtime(&now);
    strftime(t, sizeof(t), "%Y-%m-%d %H:%M:%S", tmv);
    char ledger[PATH_BUF];
    chai_session_ledger_path(ledger, sizeof(ledger), chai_active_session);
    FILE *f = fopen(ledger, "a");
    if (f) {
        fprintf(f, "[%s] user: <%d> %s | Trigger: chat-hai\n", t, digit, chai_cli_prompts[digit]);
        fclose(f);
    }
    chai_load_ledger();
    if (chai_n_events > 0) chai_selected_event = chai_n_events - 1;
    chai_feed_dirty = 1; /* see that flag's own header comment */
    chai_redraw();
}

/* shared dispatch for both mouse clicks and keyboard index-activation
 * (Enter on the focused nav_index) - wraith-alpha's own convention is
 * that a numbered element behaves identically whichever input method
 * reaches it. */
/* ---------- chat_hai_config.pdl write (2026-08-16) ----------
 * ONE writer for the whole .pdl, shared by the speed-toggle and
 * sound-toggle GUI buttons - every click must preserve the other keys
 * (window geometry, require_cli_activation, sound_on, sleep_between).
 * chat_hai_loop.sh re-reads the file every round, so a click here goes
 * live without any restart (same contract as sleep_between()). */
static void chai_write_chat_hai_cfg(int sleep_secs, int sound) {
    char cfg_path[PATH_BUF];
    snprintf(cfg_path, sizeof(cfg_path), "%s/&.hq-apps/chat-hai/chat_hai_config.pdl", g_house_root);
    FILE *wf = fopen(cfg_path, "w");
    if (!wf) return;
    fprintf(wf,
        "# chat_hai_config.pdl - live-edited by the GUI buttons (chat_hai_hq_render.c)\n"
        "SECTION | sleep_between | %d\n"
        "SECTION | window_width | %d\n"
        "SECTION | window_bottom_margin | %d\n"
        "SECTION | window_right_margin | %d\n"
        "SECTION | window_top_offset | %d\n"
        "SECTION | require_cli_activation | %d\n"
        "SECTION | sound_on | %d\n",
        sleep_secs, chai_cfg_window_width, chai_cfg_bottom_margin, chai_cfg_right_margin,
        chai_cfg_top_offset, chai_require_cli_activation, sound);
    fclose(wf);
}

static void chai_activate_elem(Elem *hit) {
    if (!hit) return;
    /* REAL, NEW 2026-08-28 (Phase C target #2) - same generic scroll:up/
     * down dispatch dbhq_activate_elem()/evhq_activate_elem() already
     * use, checked first since the synthetic scroll-arrow Elems carry no
     * tag/id chai_activate_elem() otherwise dispatches on. */
    if (hit->onclick[0] && (strcmp(hit->onclick, "scroll:up") == 0 || strcmp(hit->onclick, "scroll:down") == 0)) {
        g_pal_scroll += (strcmp(hit->onclick, "scroll:down") == 0) ? 1 : -1;
        chai_redraw();
        return;
    }
    /* Activating anything that ISN'T the Settings affordance closes the
     * open panel first (same contract as open-hai's own Settings:
     * non-settings activation dismisses it). */
    if (chai_settings_open && hit != chai_settings_elem && hit != chai_settings_sound_elem) {
        chai_settings_open = 0;
        chai_redraw();
    }
    if (strcmp(hit->tag, "settingsbtn") == 0) {
        chai_settings_open = !chai_settings_open;
        chai_redraw();
        return;
    }
    if (hit == chai_composer_text_elem) {
        /* REAL FIX 2026-08-16 - see chai_require_cli_activation's own header
         * comment. This case used to not exist at all, so BOTH the
         * click path (chai_handle_click() always calls chai_activate_elem() on
         * whatever it hit) and the Enter-on-empty-focused-composer path
         * (chai_handle_key()'s XK_Return branch) silently did nothing here -
         * the focus badge only ever caught up on some UNRELATED chai_redraw.
         * Auto-activate default (require_cli_activation=0): always sets
         * activated=1, badge shows "^" immediately. Legacy mode
         * (require_cli_activation=1): still reached via Enter (the
         * wraith-alpha "Enter activates the focused element" path),
         * which is exactly the intended legacy activation gesture. */
        chai_composer_activated = 1;
        chai_redraw();
        return;
    }
    if (strcmp(hit->tag, "closebtn") == 0) {
        g_quit = 1;
        return;
    }
    if (strcmp(hit->tag, "tab") == 0) {
        for (int i = 0; i < CHAI_N_TABS; i++) if (strcmp(hit->label, CHAI_TAB_LABELS[i]) == 0) { chai_current_tab = i; break; }
        chai_redraw();
        return;
    }
    if (strcmp(hit->tag, "newsession") == 0) {
        chai_create_new_session();
        chai_redraw();
        return;
    }
    if (strcmp(hit->tag, "item") == 0) {
        /* Disambiguate by parent, NOT just tag - real sessions
         * (sidebar) and feed messages (panel) both use tag "item" (see
         * chai_inject_sessions()/chai_inject_panel_feed()'s own header comments).
         * A sidebar item click switches the active session (real
         * effect - chat_hai_loop.sh re-reads active.txt too, see
         * chai_switch_session()); a panel item click just selects which
         * message line is highlighted, as before. */
        Elem *sidebar = find_by_tag(g_window, "sidebar");
        if (hit->parent == sidebar) {
            chai_switch_session(hit->label);
            chai_redraw();
            return;
        }
        for (int i = 0; i < chai_n_events; i++) if (strcmp(chai_events[i], hit->label) == 0) { chai_selected_event = i; break; }
        chai_redraw();
        return;
    }
    if (strcmp(hit->id, "send") == 0) {
        chai_send_composer();
        return;
    }
    if (strcmp(hit->id, "toggle-pause") == 0) {
        /* REAL FIX 2026-08-15 (direct instruction: "the start stop of
         * chat should be of the logic of the ai lan call itself if need
         * be" - after "i still dont get results from start/stop" with
         * the PRIOR pkill -STOP/-CONT approach): SIGSTOP on
         * chat_hai_loop.sh's own bash PROCESS does not reliably freeze
         * a curl call already in flight inside a command-substitution
         * subshell - "stopped" chat kept producing replies mid-flight.
         * Real fix: a plain control FILE (state/paused.txt) that the
         * loop script checks in a wait-loop right before EVERY qwen.sh/
         * curl call (see chat_hai_loop.sh's own speak() function,
         * "REAL FIX 2026-08-15" comment there) - guarantees zero new
         * LAN calls while paused, no OS-signal timing races. */
        chai_paused = !chai_paused;
        char pause_path[PATH_BUF];
        snprintf(pause_path, sizeof(pause_path), "%s/&.hq-apps/chat-hai/state/paused.txt", g_house_root);
        FILE *pf = fopen(pause_path, "w");
        if (pf) { fprintf(pf, "%d\n", chai_paused ? 1 : 0); fclose(pf); }
        if (chai_toggle_elem) {
            snprintf(chai_toggle_elem->label, sizeof(chai_toggle_elem->label), "%s", chai_paused ? "Start" : "Stop");
        }
        chai_update_status_label();
        chai_redraw();
        return;
    }
    if (strcmp(hit->id, "speed-toggle") == 0) {
        /* REAL, working GUI speed control (direct instruction,
         * 2026-08-15: "can have an input in gui also" - for the same
         * sleep_between setting chat_hai_config.pdl now exposes as a
         * hand-editable file). Cycles a fixed preset list and writes
         * the .pdl chat_hai_loop.sh's own sleep_between() function
         * reads every round - no restart needed, matches that
         * function's own header comment. */
        static const int presets[] = { 2, 4, 6, 12, 20 };
        static const int n_presets = (int)(sizeof(presets) / sizeof(presets[0]));
        char cfg_path[PATH_BUF];
        snprintf(cfg_path, sizeof(cfg_path), "%s/&.hq-apps/chat-hai/chat_hai_config.pdl", g_house_root);
        int cur = 6, idx = 2; /* default matches chat_hai_config.pdl's own default */
        FILE *rf = fopen(cfg_path, "r");
        if (rf) {
            char line[256];
            while (fgets(line, sizeof(line), rf)) {
                if (strstr(line, "sleep_between")) {
                    char *bar2 = strrchr(line, '|');
                    if (bar2) cur = atoi(bar2 + 1);
                    break;
                }
            }
            fclose(rf);
        }
        for (int i = 0; i < n_presets; i++) if (presets[i] == cur) { idx = i; break; }
        int next_val = presets[(idx + 1) % n_presets];
        chai_write_chat_hai_cfg(next_val, chai_sound_on);
        if (chai_speed_elem) {
            snprintf(chai_speed_elem->label, sizeof(chai_speed_elem->label), "Speed: %ds", next_val);
        }
        chai_redraw();
        return;
    }
    if (strcmp(hit->id, "sound-toggle") == 0) {
        /* Sound on/off for the incoming-message tone (direct instruction,
         * 2026-08-16: "play a tone when a message is posted" - incoming
         * only, toggleable off). Flipped here, written to the same .pdl
         * the loop's own sound check reads fresh on every posted message
         * (see chai_write_chat_hai_cfg()), so it goes live immediately. */
        chai_sound_on = !chai_sound_on;
        char cfg_path[PATH_BUF];
        snprintf(cfg_path, sizeof(cfg_path), "%s/&.hq-apps/chat-hai/chat_hai_config.pdl", g_house_root);
        int cur = 6;
        FILE *rf = fopen(cfg_path, "r");
        if (rf) {
            char line[256];
            while (fgets(line, sizeof(line), rf)) {
                if (strstr(line, "sleep_between")) {
                    char *bar2 = strrchr(line, '|');
                    if (bar2) cur = atoi(bar2 + 1);
                    break;
                }
            }
            fclose(rf);
        }
        chai_write_chat_hai_cfg(cur, chai_sound_on);
        if (chai_sound_elem) {
            snprintf(chai_sound_elem->label, sizeof(chai_sound_elem->label), "Sound: %s", chai_sound_on ? "on" : "off");
        }
        chai_redraw();
        return;
    }
}

static void chai_handle_click(int px, int py) {
    /* close button lives in the chrome bar, outside window's own tag
     * tree (it's synthetic, not parsed from dashboard.chtpm) - check it
     * before the tree walk. */
    if (px >= chai_close_elem->x && px < chai_close_elem->x + chai_close_elem->w &&
        py >= chai_close_elem->y && py < chai_close_elem->y + chai_close_elem->h) {
        g_focus_nav = chai_close_elem->nav_index;
        chai_activate_elem(chai_close_elem);
        return;
    }
    /* Settings badge + its Sound row are static (synthetic, outside the
     * parsed window tree) - same close-button treatment, hit-test them
     * here before the tree walk. */
    if (px >= chai_settings_elem->x && px < chai_settings_elem->x + chai_settings_elem->w &&
        py >= chai_settings_elem->y && py < chai_settings_elem->y + chai_settings_elem->h) {
        g_focus_nav = chai_settings_elem->nav_index;
        chai_activate_elem(chai_settings_elem);
        return;
    }
    if (chai_settings_open &&
        px >= chai_settings_sound_elem->x && px < chai_settings_sound_elem->x + chai_settings_sound_elem->w &&
        py >= chai_settings_sound_elem->y && py < chai_settings_sound_elem->y + chai_settings_sound_elem->h) {
        g_focus_nav = chai_settings_sound_elem->nav_index;
        chai_activate_elem(chai_settings_sound_elem);
        return;
    }
    /* REAL, NEW 2026-08-28 (Phase C target #2) - same synthetic-elem
     * coordinate check dbhq_handle_click()/evhq_handle_click() already
     * use for the scroll arrows (drawn Elems, not part of g_window's
     * parsed tree - hit_test() below would never find them). */
    if (g_pal_has_grid) {
        if (px >= g_pal_arrow_up->x && px < g_pal_arrow_up->x + g_pal_arrow_up->w &&
            py >= g_pal_arrow_up->y && py < g_pal_arrow_up->y + g_pal_arrow_up->h) {
            if (g_pal_arrow_up->nav_index > 0) g_focus_nav = g_pal_arrow_up->nav_index;
            chai_activate_elem(g_pal_arrow_up);
            return;
        }
        if (px >= g_pal_arrow_down->x && px < g_pal_arrow_down->x + g_pal_arrow_down->w &&
            py >= g_pal_arrow_down->y && py < g_pal_arrow_down->y + g_pal_arrow_down->h) {
            if (g_pal_arrow_down->nav_index > 0) g_focus_nav = g_pal_arrow_down->nav_index;
            chai_activate_elem(g_pal_arrow_down);
            return;
        }
    }
    Elem *hit = hit_test(g_window, px, py);
    if (!hit) return;
    /* The composer text field is a real exception, not an oversight:
     * clicking a text field to focus it for typing is normal UX
     * everywhere, and is a genuinely different action from "select a
     * menu item" - it keeps its own real toggle (chai_require_cli_
     * activation) rather than the new two-step convention below.
     * Legacy mode: click only moves focus ("[>]"), Enter activates
     * ("[^]"). Auto mode (default): click both focuses and activates,
     * per direct instruction ("it move '>' and auto sets it to '^' im
     * fine with that") - unchanged, this predates and is orthogonal to
     * the newer click_focus_then_activate() convention below. */
    if (hit == chai_composer_text_elem) {
        if (hit->nav_index > 0) g_focus_nav = hit->nav_index;
        if (chai_require_cli_activation) { chai_redraw(); return; }
        chai_activate_elem(hit);
        return;
    }
    if (!click_focus_then_activate(hit)) { chai_redraw(); return; }
    chai_activate_elem(hit);
}

/* wraith-alpha-standard digit-accumulation key handling (ports
 * ops/wraith_parser_alpha.c's digit_accum/do_jump/Enter-activates
 * convention): digits move focus live as they're typed (do_jump), Enter
 * activates the focused element, any other key resets the accumulator. */
static void chai_handle_key(KeySym ks, char ch) {
    if (ch == 'p') { chai_dump_frame_png(); return; }
    if (ks == XK_Return || ks == XK_KP_Enter) {
        if (chai_digit_accum > 0 && chai_digit_accum <= g_n_nav) g_focus_nav = chai_digit_accum;
        chai_digit_accum = 0;
        if (chai_composer_len > 0) { chai_send_composer(); return; }
        if (g_focus_nav >= 1 && g_focus_nav <= g_n_nav) chai_activate_elem(g_nav[g_focus_nav - 1]);
        return;
    }
    if (ks == XK_Escape) {
        if (chai_digit_accum > 0) { chai_digit_accum = 0; return; }
        /* Open Settings panel closes on Escape first (same contract as
         * open-hai) - only a bare Escape with the panel closed quits. */
        if (chai_settings_open) { chai_settings_open = 0; chai_redraw(); return; }
        if (chai_composer_len > 0) { chai_composer_len = 0; chai_composer[0] = '\0'; chai_composer_sync(); chai_redraw(); return; }
        g_quit = 1; /* no WM chrome/close button (override_redirect) - Escape closes instead */
        return;
    }
    if (ks == XK_BackSpace) {
        /* Real, working "delete session" (direct instruction, 2026-08-15:
         * "we should beable to add / delete new sessions") - matches
         * open-hai's own documented "Backspace on a sidebar row
         * deletes it" convention (chat-hai-design.md's own reference).
         * Only fires when the CURRENTLY FOCUSED nav element is a real
         * sidebar session row (not the "+ New Session" button, not a
         * panel feed message) - falls through to composer-edit
         * otherwise, so this never eats a Backspace the user meant for
         * their in-progress message. */
        if (g_focus_nav >= 1 && g_focus_nav <= g_n_nav) {
            Elem *f = g_nav[g_focus_nav - 1];
            Elem *sidebar = find_by_tag(g_window, "sidebar");
            if (f && f->parent == sidebar && strcmp(f->tag, "item") == 0) {
                chai_delete_session(f->label);
                chai_redraw();
                return;
            }
        }
        if (chai_composer_len > 0) {
            chai_composer[--chai_composer_len] = '\0';
            chai_composer_sync();
            chai_redraw();
        }
        return;
    }
    if (ch >= '1' && ch <= '9') {
        /* Digits 1-9: send the composer text (cli-io shortcut, like open-hai) */
        chai_send_composer();
        return;
    }
    if (ch == '0') {
        chai_digit_accum = 0;
        return;
    }
    if (ks == XK_Up || ks == XK_Left) {
        if (g_focus_nav > 1) g_focus_nav--;
        chai_digit_accum = 0;
        chai_redraw();
        return;
    }
    if (ks == XK_Tab || ks == XK_ISO_Left_Tab) {
        if (chai_has_real_focus) nav_tab_cycle();
        chai_digit_accum = 0;
        return;
    }
    if (ks == XK_Down || ks == XK_Right) {
        if (g_focus_nav < g_n_nav) g_focus_nav++;
        chai_digit_accum = 0;
        chai_redraw();
        return;
    }
    /* REAL, NEW 2026-08-28 (Phase C target #2) - same real Page_Up/
     * Page_Down paging dbhq_handle_key() already uses for any
     * g_pal_has_grid mode; chat-hai's own session sidebar had no
     * keyboard scroll path at all before this. */
    if (ks == XK_Page_Up || ks == XK_Page_Down) {
        if (g_pal_has_grid) {
            int step = g_pal_visible_rows > 1 ? g_pal_visible_rows - 1 : 1;
            g_pal_scroll += (ks == XK_Page_Down) ? step : -step;
            chai_layout_pass(g_window);
            chai_assign_nav_indices(g_window);
        }
        chai_digit_accum = 0;
        chai_redraw();
        return;
    }
    /* printable chars (not digits, not 'p' receipt) go to the composer.
     * Legacy gate (require_cli_activation=1, see its own header comment):
     * only accepted once the composer is BOTH the focused element AND
     * "^" activated - checked together so a stale activated flag left
     * over from a previous focus never leaks input to the wrong place.
     * Default (0): always accepted, matches this app's original
     * always-on-typing behavior. */
    if (ch >= 32 && ch <= 126) {
        int composer_ready = !chai_require_cli_activation ||
            (chai_composer_activated && chai_composer_text_elem && g_focus_nav == chai_composer_text_elem->nav_index);
        if (composer_ready) {
            if (chai_composer_len < CHAI_COMPOSER_BUF - 1) {
                chai_composer[chai_composer_len++] = ch;
                chai_composer[chai_composer_len] = '\0';
            }
            chai_composer_sync();
        }
        chai_redraw();
        return;
    }
    chai_digit_accum = 0;
}

/* Agent relay (2026-08-26, direct instruction: "u should be able to
 * inject key to do testing + reading frame history... try that before
 * using xdo tool" - this was documented here as a plan but never
 * actually implemented, a real gap found while testing Task 6 live).
 * <house_root>/#.desktop/db_hq_history.txt, one decimal ASCII code per
 * line (48-57 digits, 13 Enter, 27 Escape, 8 Backspace, 32-126 other
 * printable). REAL CORRECTION (2026-08-26, same day): this was
 * ORIGINALLY written as a brand-new poller (poll_dbhq_agent_relay(),
 * its own cursor, its own file path) before discovering that a real,
 * working, GENERIC version of exactly this already existed in this
 * same file - history_path()/dispatch_relay_code()/poll_agent_history()
 * below, which ALREADY computes "db_hq_history.txt" for db-hq mode via
 * its own g_is_db_hq branch, and already dispatches through the shared
 * handle_key(). Running both at once meant every injected line got
 * dispatched TWICE (two independent pollers, two independent cursors,
 * same file) - the real cause of a whole session's worth of "nav focus
 * randomly drifts" confusion that was wrongly chased as an unrelated
 * bug. The duplicate poller has been deleted; only the real text-state
 * dump below survives, now wired into the EXISTING dispatch_relay_code()
 * via code 210 (200-205 are already reserved there for arrow/page keys -
 * see that function's own comment). Lesson for future agents: grep for
 * an existing mechanism by its ACTUAL behavior (mode-aware history
 * filename, generic dispatch) before assuming a dangling comment means
 * "not built yet" - it can also mean "built under a different name than
 * the comment used." */
/* Real, cheap TEXT state dump for db-hq debugging, sibling to
 * dump_frame_png() but readable directly (no image decode needed) and
 * far cheaper to generate - triggered via the real relay's code 210. */
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

static void assign_nav_and_layout(void) {
    /* REAL Stage 5 §5d.10 (2026-08-16) - db-hq mode branch, real WM-
     * managed window shape, own layout/nav functions (ported verbatim,
     * not forced into the popup modes' page/item shape below). */
    if (g_is_db_hq) { dbhq_layout_pass(g_window); dbhq_assign_nav_indices(g_window); return; }
    if (g_is_events_hq) { evhq_layout_pass(g_window); evhq_assign_nav_indices(g_window); return; }
    if (g_is_chat_hai) { chai_layout_pass(g_window); chai_assign_nav_indices(g_window); return; }
    g_n_nav = 0;
    Elem *page = find_page(g_current_page);
    if (!page) { g_win_h = CHROME_H + 8; return; }
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
    g_quit = 1; /* real menus close after a real action fires, matching tp_desktop_window_rgb.c's own UX (g_menu_stay_open aside - default behavior) */
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
    if (g_is_chat_hai) { chai_redraw(); return; }
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
        if (ff) { dbhq_serialize_frame_subtree(ff, page); fclose(ff); rename(tmpp, fpath); }
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
    if (g_is_chat_hai) { chai_dump_frame_png(); return; } /* REAL §5d.12 - self-contained, own /tmp/chat-hai-frame.png contract preserved, own real-redraw-before-capture fix below */
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
    if (g_is_chat_hai) { chai_handle_key(ks, ch); return; } /* REAL §5d.12 - own real 'p' handling inside, same key-order exception class as events-hq */
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
    if (ks == XK_Up) { if (g_focus_nav > 1) g_focus_nav--; return; }
    if (ks == XK_Down) { if (g_focus_nav < g_n_nav) g_focus_nav++; return; }
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
             g_is_chat_hai ? "chat_hai_history" :
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

static void nav_tab_register(const char *title) {
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
        fprintf(f, "%d %lx %s\n", g_nav_tab_ordinal, (unsigned long)win,
                title ? title : "hq");
        fclose(f);
    }
    snprintf(ledger, sizeof(ledger), "%s/#.desktop/nav_master_ledger.txt", g_house_root);
    FILE *lf = fopen(ledger, "a");
    if (lf) {
        fprintf(lf, "REG pid=%d tab=%d xid=%lx %s\n",
                (int)getpid(), g_nav_tab_ordinal, (unsigned long)win,
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
             g_is_chat_hai ? "chat_hai_frame_changed.txt" :
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
    else if (code == 210 && (g_is_db_hq || g_is_events_hq || g_is_chat_hai)) dbhq_dump_debug_state();
    else if (code >= 32 && code <= 126) handle_key(0, (char)code);
    else if (code > 255 && code != 200 && code != 201 && code != 202 &&
             code != 203 && code != 204 && code != 205 && code != 210)
        handle_key((KeySym)code, 0);
}
static int hq_window_has_x_focus(void) {
    if (g_is_chat_hai) return chai_has_real_focus;
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
                    else if (g_is_chat_hai) chai_handle_click(mx, my);
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

static time_t g_chai_last_ledger_mtime;

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
    if (g_is_chat_hai) {
        char chai_ledger_check[PATH_BUF];
        chai_session_ledger_path(chai_ledger_check, sizeof(chai_ledger_check), chai_active_session);
        struct stat chai_lst;
        if (stat(chai_ledger_check, &chai_lst) == 0 && chai_lst.st_mtime != g_chai_last_ledger_mtime) {
            g_chai_last_ledger_mtime = chai_lst.st_mtime;
            chai_load_ledger();
            if (chai_n_events > 0) chai_selected_event = chai_n_events - 1;
            chai_feed_dirty = 1;
            redraw();
        }
        {
            char chai_typing_path[PATH_BUF];
            snprintf(chai_typing_path, sizeof(chai_typing_path), "%s/&.hq-apps/chat-hai/state/typing.txt", g_house_root);
            char chai_cur_typing[64] = "";
            FILE *chai_tf = fopen(chai_typing_path, "r");
            if (chai_tf) {
                if (fgets(chai_cur_typing, sizeof(chai_cur_typing), chai_tf)) {
                    char *nl = strchr(chai_cur_typing, '\n');
                    if (nl) *nl = '\0';
                }
                fclose(chai_tf);
            }
            if (strcmp(chai_cur_typing, chai_typing_name) != 0) {
                snprintf(chai_typing_name, sizeof(chai_typing_name), "%s", chai_cur_typing);
                chai_update_status_label();
                redraw();
            }
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
    if (!g_is_db_hq && !g_is_events_hq && !g_is_chat_hai) {
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
    if (poll_agent_history() > 0 && !g_quit) hq_request_redraw();
    if (g_is_db_hq || g_is_events_hq || g_is_chat_hai) nav_tab_poll_active();
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
            if (ev->xbutton.button == 1 && ev->xbutton.y < CHROME_H &&
                !(ev->xbutton.x >= g_win_w - 60 && ev->xbutton.x < g_win_w)) {
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
        } else if (g_is_chat_hai) {
            if (ev->xbutton.button == 1 && ev->xbutton.y < 26) {
                chai_dragging = 1;
                chai_drag_last_x = ev->xbutton.x_root;
                chai_drag_last_y = ev->xbutton.y_root;
            }
            if (g_pal_has_grid && (ev->xbutton.button == 4 || ev->xbutton.button == 5)) {
                g_pal_scroll += (ev->xbutton.button == 5) ? 2 : -2;
                chai_layout_pass(g_window);
                chai_assign_nav_indices(g_window);
            } else if (ev->xbutton.button != 3) {
                chai_has_real_focus = 1;
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
        chai_dragging = 0;
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
        } else if (g_is_chat_hai && chai_dragging) {
            int dx = ev->xmotion.x_root - chai_drag_last_x;
            int dy = ev->xmotion.y_root - chai_drag_last_y;
            chai_win_x += dx; chai_win_y += dy;
            if (chai_win_y < WM_MANAGED_DRAG_MIN_Y) chai_win_y = WM_MANAGED_DRAG_MIN_Y;
            XMoveWindow(dpy, win, chai_win_x, chai_win_y);
            chai_drag_last_x = ev->xmotion.x_root;
            chai_drag_last_y = ev->xmotion.y_root;
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
        } else if (g_is_chat_hai) {
            snprintf(chai_last_key_label, sizeof(chai_last_key_label), "%s", kname ? kname : (buf8[0] ? buf8 : "?"));
            chai_has_real_focus = 1;
            dbhq_capture_key(ks, buf8[0]);
            poll_agent_history();
            redraw();
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
        } else if (g_is_chat_hai) {
            chai_has_real_focus = 1;
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
        } else if (g_is_chat_hai) {
            chai_has_real_focus = 0;
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

int main(int argc, char **argv) {
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
    { struct stat gcst; if (stat(g_chtpm_path, &gcst) == 0) g_chtpm_mtime = gcst.st_mtime; }

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
        /* REAL §5d.12 (2026-08-16) - chat-hai mode, last of the 5, same
         * real convention, matching its own real existing class
         * attribute (`<window class="chat-window">`, unchanged). */
        if (strcmp(g_window->classes[i], "chat-window") == 0) { g_is_chat_hai = 1; break; }
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
        if (g_is_db_hq || g_is_events_hq || g_is_chat_hai) {
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

    /* REAL §5d.12 (2026-08-16) - chat-hai mode one-time init, ported
     * verbatim from chat_hai_hq_render.c's own main(): signal handlers,
     * font-scale/window-geometry .pdl reads, session/ledger migration+
     * load, real fork()+execl() module launch, panel control-elem
     * caching (must happen right after parse, before layout_pass()
     * ever rebuilds panel->n_children - see chai_status_elem's own
     * header comment), composer sync. Forced window geometry (needs a
     * live X connection for DisplayWidth/Height) is set separately,
     * right after dpy opens below. */
    if (g_is_chat_hai) {
        signal(SIGTERM, chai_handle_term_signal);
        signal(SIGINT, chai_handle_term_signal);

        /* REAL FIX (found live via gdb backtrace - SIGSEGV in
         * css_compute_style_ex, chai_layout_pass -> assign_nav_and_
         * layout -> main): chai_sheet was declared but never pointed at
         * the shared g_sheet the generic CSS-load block above already
         * populated (chat-hai's own original main() used a locally-
         * scoped `static CssSheet sheet; g_sheet = &sheet;` that this
         * port's generic CSS-load branch made redundant, but the
         * pointer assignment itself was dropped in the process). */
        chai_sheet = &g_sheet;

        chai_load_font_scale();
        chai_load_window_geometry_config();
        if (chai_require_cli_activation) chai_composer_activated = 0;
        chai_chrome_h = scaled(26);

        memset(chai_close_elem, 0, sizeof(*chai_close_elem));
        snprintf(chai_close_elem->tag, sizeof(chai_close_elem->tag), "closebtn");

        chai_migrate_legacy_ledger_if_needed();
        chai_load_sessions_list();
        chai_load_ledger();
        if (chai_n_events > 0) chai_selected_event = chai_n_events - 1;

        Elem *chai_module_elem = find_by_tag(g_window, "module");
        if (chai_module_elem && chai_module_elem->label[0]) chai_launch_module(chai_module_elem->label);
        atexit(chai_cleanup_module);

        chai_n_elems_static = g_n_elems;

        Elem *chai_panel0 = find_by_tag(g_window, "panel");
        chai_status_elem = chai_panel0 ? find_by_id(chai_panel0, "status") : NULL;
        chai_toggle_elem = chai_panel0 ? find_by_id(chai_panel0, "toggle-pause") : NULL;
        chai_speed_elem = chai_panel0 ? find_by_id(chai_panel0, "speed-toggle") : NULL;
        chai_sound_elem = &chai_settings_sound_elem_storage;
        chai_composer_text_elem = chai_panel0 ? find_by_id(chai_panel0, "composer-text") : NULL;

        chai_composer_sync();
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

    /* REAL §5d.12 (2026-08-16) - chat-hai mode: forced window geometry
     * (chai_load_window_geometry_config()'s own .pdl-driven width/
     * top-offset/margins), ported verbatim from chat_hai_hq_render.c's
     * own main() - needs a live X connection for DisplayWidth/Height,
     * so it runs here, right after dpy opens, before the shared
     * assign_nav_and_layout() call below (chai_layout_pass() applies
     * chai_forced_win_w/h every call, same real "re-apply every frame,
     * not just once" fix its own header comment documents). */
    if (g_is_chat_hai) {
        int screen_w = DisplayWidth(dpy, screen);
        int screen_h = DisplayHeight(dpy, screen);
        chai_forced_win_w = scaled(chai_cfg_window_width);
        chai_forced_win_h = screen_h - scaled(chai_cfg_top_offset) - scaled(chai_cfg_bottom_margin);
        g_window->style.has_width = 1; g_window->style.width = chai_forced_win_w;
        g_window->style.has_height = 1; g_window->style.height = chai_forced_win_h;
        chai_win_x = screen_w - chai_forced_win_w - scaled(chai_cfg_right_margin);
        chai_win_y = scaled(chai_cfg_top_offset);
    }

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
        swa.override_redirect = True;
        win = XCreateWindow(dpy, RootWindow(dpy, screen), g_win_x, g_win_y, (unsigned)ww, (unsigned)wh, 0,
                             CopyFromParent, InputOutput, CopyFromParent,
                             CWBackPixel | CWEventMask | CWOverrideRedirect, &swa);
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
        nav_tab_register(g_is_palettes ? "palettes" : g_is_bookmarks ? "bookmarks" : g_is_stats_hq ? "stats-hq" : "db-hq");
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
        swa.override_redirect = True;
        win = XCreateWindow(dpy, RootWindow(dpy, screen), g_win_x, g_win_y, (unsigned)ww, (unsigned)wh, 0,
                             CopyFromParent, InputOutput, CopyFromParent,
                             CWBackPixel | CWEventMask | CWOverrideRedirect, &swa);
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
        nav_tab_register("events-hq");

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

    /* REAL §5d.12 (2026-08-16) - chat-hai mode: real WM-managed window
     * creation + own event loop, last of the 3 WM-managed apps merged.
     * Genuinely its own real branch (not interleaved into db-hq's or
     * events-hq's) - real ledger-mtime poll + typing poll on top of the
     * shared relay poll (chat-hai's own real "constantly scrolling
     * feed" requirement, ported verbatim), plus ktb_init()/
     * ktb_quit_and_save() on exit like db-hq. */
    if (g_is_chat_hai) {
        int ww = g_window->w, wh = g_window->h;

        XSetWindowAttributes swa;
        swa.background_pixel = alloc_pixel("#141414");
        swa.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | ButtonMotionMask | KeyPressMask | StructureNotifyMask | FocusChangeMask;
        /* REAL FIX 2026-08-29 - see db-hq branch's own identical
         * comment above (OPACITY-PIPELINE-INVESTIGATION-2026-08-29.txt
         * has the full research trail) - same real bug, same fix. */
        swa.override_redirect = True;
        win = XCreateWindow(dpy, RootWindow(dpy, screen), chai_win_x, chai_win_y, (unsigned)ww, (unsigned)wh, 0,
                             CopyFromParent, InputOutput, CopyFromParent,
                             CWBackPixel | CWEventMask | CWOverrideRedirect, &swa);
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
        /* open-hai / egg_window: XMapWindow, not XMapRaised — Mutter
         * activates MapRaised WM-managed windows and steals the human's
         * browser. File relay still drives this process. */
        XMapWindow(dpy, win);
        set_window_opacity(dpy, win, load_theme_opacity());
        XSync(dpy, False);
        { XWindowAttributes wa; if (XGetWindowAttributes(dpy, win, &wa)) { chai_win_x = wa.x; chai_win_y = wa.y; } }
        nav_tab_register("chat-hai");
        XSync(dpy, False);
        { XEvent stale_ev; while (XCheckWindowEvent(dpy, win, ButtonPressMask | KeyPressMask, &stale_ev)) { } }

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
         * to close, closes all desktop entures (bad)") - see db-hq's own
         * real, identical fix above for the full explanation:
         * ktb_quit_and_save() is a real, TASKBAR-LEVEL quit action
         * (desktop-wide entity teardown + shared pidfile removal), never
         * appropriate for a single sub-app window closing. Removed
         * entirely, not narrowed - `chai_ktb` was only ever used for
         * this one call. */
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
    swa.override_redirect = True;
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
