/* khtpm_taskbar_manager_main.c — standalone "manager driver" binary.
 *
 * Per khtpm-strip-parser-design.md §5 decisions ("Binary split: manager
 * keeps KtbState + the file-relay loop only"): this is the CHTPM-manager
 * side of the new two-process strip architecture. It owns a KtbState
 * (reusing khtpm_taskbar_manager.c/.h completely unmodified — see that
 * file's header comment, this is done/live-verified logic, not touched
 * here), polls a bare-decimal-per-line strip_history.txt relay file the
 * new khtpm_strip_parser.c writes into, dispatches each code into the
 * existing ktb_* API, and on any state mutation writes strip_state.txt
 * (pipe-delimited, matching the house .pdl convention) plus appends a
 * single marker byte to strip_frame_changed.txt (mirroring CHTPM's own
 * frame_changed.txt touch-signal, see chtpm_parser.c's four `fopen(...,
 * "a")` appenders).
 *
 * This binary has ZERO platform/Xlib calls — pure logic + file relay,
 * exactly the shape the design's decisions section calls for.
 */
#ifndef _WIN32
#define _DEFAULT_SOURCE
#endif

#include "khtpm_taskbar_manager.h"

/* REAL FIX 2026-09-01, direct instruction ("we really dont make .h
 * files... u could move that into the .c with ease") - khtpm_strip_
 * codes.h inlined verbatim (was a real, genuine #include shared with
 * khtpm_core_render.c's own strip mode; that side already carries this
 * same content verbatim per its own "khtpm_strip_codes.h, verbatim"
 * comment, so the header itself was left with exactly one real
 * consumer - this file - and no reason left to exist as its own file).
 *
 * khtpm_strip_codes.h — shared decimal action-code protocol between
 * the strip's own real click/key resolution (khtpm_core_render.c's
 * strip_main(), writer) and this file (reader, polls
 * strip_history.txt and dispatches into the existing ktb_* manager
 * API).
 *
 * Per khtpm-strip-parser-design.md §2 ("Keys in"), the wire format on
 * strip_history.txt is CHTPM's own real convention: one bare decimal
 * integer per line ("%d\n"), no prefix. This is the single place both
 * sides agree on what each integer means.
 *
 * Judgment call (not specified field-by-field in the design doc):
 * CHTPM's own history.txt carries raw X keycodes/ASCII, so low values
 * (0-127) are reserved for literal ASCII (digit chars '0'-'9' =
 * 48-57, BackSpace = 8, Return = 13, Escape = 27 — the same codes
 * XLookupString/XLookupKeysym already normalize to). Values >= 1000
 * are strip-specific resolved actions (arrow-equivalents, tab clicks,
 * shortcut clicks, close) that have no natural single-byte ASCII code,
 * using the same "resolved action, not raw coordinates" shape. */
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

/* --- HQ popup menu (top-left window) + cli-io modal, added for the
 * khtpm_strip_parser port of tp_taskbar.c's second (HQ/user/file/desks)
 * window (2026-08-11). KSC_ENTER/KSC_ESCAPE/KSC_FOCUS_LEFT/KSC_FOCUS_RIGHT
 * above are REUSED here (not redefined) for hq-popup/cli-io navigation,
 * exactly like tp_taskbar.c's agent_relay_dispatch() reuses the same
 * (code==13)/(code==27) checks contextually depending on which modal is
 * active (cli-io > hq popup > armed nav, see that function's own comment
 * on dispatch order) - this file's own dispatch_code() replicates the
 * same precedence, so no separate "hq enter" / "hq escape" codes exist.
 * Printable ASCII (already reserved 0-127 per this comment's own convention
 * above) doubles as cli-io typed text when cliio_typing is active - again
 * mirrored from agent_relay_dispatch()'s cli-io branch, which filters the
 * same raw ASCII through cliio_key_allowed() rather than using a separate
 * "typed char" code range. */

/* Strip header cell click: 4000 + which, which = (cell index + 1) over the
 * full 15-cell strip (2026-08-11 pass 2, matches tp_taskbar.c's
 * load_strip_config()+cells[] order exactly): 1=HQ, 2=USER, 3=file,
 * 4=desks, 5=pals, 6=palettes, 7=edit, 8=player, 9=db, 10=plugins,
 * 11=menus, 12=store, 13=network, 14=ai, 15=date/time. See
 * khtpm_taskbar_manager.h's KtbState comment and
 * ktb_hq_open()/ktb_strip_user_activate() for what each which does. */
#define KSC_HQ_HEADER_BASE 4000
/* HQ/dyn popup row click (mouse only - keyboard uses digits + ENTER,
 * exactly like the bottom bar's own nav digit buffer):
 * 5000 + row_idx (idx in [0, KTB_LIVEDESK_DYN_MAX) ). */
#define KSC_HQ_ITEM_BASE   5000

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#ifndef _WIN32
#include <sys/types.h>
#include <unistd.h>
#endif

/* REAL BUG FIX 2026-08-18, direct user report ("its still staggering" /
 * "u need a much tighter investigation of parity of the entire tpmos
 * pipeline"): a flat 300ms poll here (this file) chained in series with
 * khtpm_strip_parser.c's OWN flat 300ms poll (the relay it reads from)
 * meant a single relay-injected action could take up to ~600ms to reach
 * strip_state.txt - confirmed live via a timed round-trip test, ~173ms
 * for one hop alone. TPMOS's real, canonical standard (read directly,
 * not guessed - !.TPMOS_ONBORD_BIBLE_10.md §3 "Active Pulse Throttling"):
 * "Active: usleep(16667) (~60 FPS) when input is detected or layout is in
 * focus. Idle: usleep(100000) (10 FPS) when not active." A flat 300ms
 * with no active/idle distinction at all was never the real standard -
 * this file (and its parser sibling) had silently drifted from it.
 * Fixed: dual-rate poll, matching TPMOS's own two constants exactly (not
 * approximated), switching to the active rate for a short hold window
 * after any real mutation - see ACTIVE_HOLD_TICKS below. */
#define POLL_INTERVAL_ACTIVE_USEC 16667
#define POLL_INTERVAL_IDLE_USEC   100000
/* How many idle-checked ticks to keep polling at the ACTIVE rate after the
 * last real mutation, before dropping back to IDLE - covers a burst of
 * several quick keystrokes (e.g. digit-accumulation) without re-triggering
 * per keystroke. 30 ticks * 16667us =~ 500ms of held-active polling after
 * the last real state change, a real, deliberate value (not TPMOS's own -
 * TPMOS's reference module loop doesn't need a hold window since it's
 * driven by a live input stream every tick; this poll loop only sees
 * DISCRETE mutation events, so a short hold avoids flapping instantly back
 * to idle between two keystrokes ~100-200ms apart). */
#define ACTIVE_HOLD_TICKS 30

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  define ktb_getpid() ((int)GetCurrentProcessId())
#  include <sys/stat.h>
#else
#  include <unistd.h>
#  include <sys/stat.h>
#  include <sys/time.h>
#  define ktb_getpid() ((int)getpid())
#endif

static volatile sig_atomic_t g_running = 1;

#ifndef _WIN32
static void on_sigterm(int sig) { (void)sig; g_running = 0; }
#endif

static void path_join2(char *out, size_t n, const char *root, const char *rel) {
    size_t rl = strlen(root);
    if (rl > 0 && (root[rl - 1] == '/' || root[rl - 1] == '\\'))
        snprintf(out, n, "%s%s", root, rel);
    else
        snprintf(out, n, "%s/%s", root, rel);
}

/* Pipe-delimited strip_state.txt serializer. Per the design's resolved
 * decision (§5 "strip_state.txt schema"): TAB/SHORTCUT rows plus KEY|value
 * scalar rows, matching the house's own .pdl style (see desk_01.pdl /
 * livedesk_theme.pdl) rather than CHTPM's key=value convention. */
static size_t serialize_state(const KtbState *s, char *buf, size_t buf_sz) {
    size_t off = 0;
    for (int i = 0; i < s->n_tabs && off < buf_sz; i++) {
        int n = snprintf(buf + off, buf_sz - off, "TAB | %d | %d | %s | %s\n",
                          s->tabs[i].pid, s->tabs[i].nav, s->tabs[i].entity, s->tabs[i].path);
        if (n < 0) break;
        off += (size_t)n;
    }
    for (int i = 0; i < s->n_shortcuts && off < buf_sz; i++) {
        int n = snprintf(buf + off, buf_sz - off, "SHORTCUT | %s | %s\n",
                          s->shortcuts[i].glyph, s->shortcuts[i].command);
        if (n < 0) break;
        off += (size_t)n;
    }
    /* HQITEM rows: the currently-open HQ menu's rows, per the design's
     * pipe-delimited schema convention. Empty (0 rows) whenever hq_open
     * is 0 - the parser draws only the header buttons in that case. */
    for (int i = 0; i < s->hq_n_menu && off < buf_sz; i++) {
        int n = snprintf(buf + off, buf_sz - off, "HQITEM | %s | %s | %d\n",
                          s->hq_menu[i].label, s->hq_menu[i].command, s->hq_menu[i].nav);
        if (n < 0) break;
        off += (size_t)n;
    }
    if (off < buf_sz) {
        int n = snprintf(buf + off, buf_sz - off,
                          "KEY | theme_bg | %s\n"
                          "KEY | theme_fg | %s\n"
                          "KEY | digit_buf | %s\n"
                          "KEY | tab_focus_idx | %d\n"
                          "KEY | nav_armed | %d\n"
                          "KEY | n_tabs | %d\n"
                          "KEY | n_shortcuts | %d\n"
                          "KEY | hq_open | %d\n"
                          "KEY | hq_n_menu | %d\n"
                          "KEY | hq_focus | %d\n"
                          "KEY | cliio_active | %d\n"
                          "KEY | cliio_typing | %d\n"
                          "KEY | cliio_op | %s\n"
                          "KEY | cliio_buffer | %s\n"
                          "KEY | cliio_focus | %d\n"
                          "KEY | strip_focus_cell | %d\n"
                          "KEY | cliio_label | %s\n",
                          s->theme_bg, s->theme_fg, s->digit_buf,
                          s->tab_focus_idx, s->nav_armed, s->n_tabs, s->n_shortcuts,
                          s->hq_open, s->hq_n_menu, s->hq_focus,
                          s->cliio_active, s->cliio_typing, s->cliio_op,
                          s->cliio_buffer, s->cliio_focus, s->strip_focus_cell,
                          /* cliio_label: the layout's <cli_io label="${cliio_label}"/>
                           * needs a display label, and khtpm_strip_layout.c has no
                           * per-tag conditional logic (out of the locked 5-tag
                           * vocabulary) to derive one from cliio_op itself — so the
                           * manager, which already knows the op, computes it here,
                           * same as it already computes every other markup-fragment
                           * VAR this pass adds. new-user-id/new-user-name added
                           * 2026-08-11 for the USER cell signup flow (see
                           * ktb_cliio_open_new_user()'s header comment). */
                          (strcmp(s->cliio_op, "save-as") == 0) ? "new session" :
                          (strcmp(s->cliio_op, "new-user-id") == 0) ? "new user - enter user id" :
                          (strcmp(s->cliio_op, "new-user-name") == 0) ? "new user - enter display name" :
                          "rename desk");
        if (n > 0) off += (size_t)n;
    }
    if (off >= buf_sz) off = buf_sz - 1;
    buf[off] = '\0';
    return off;
}

/* ---------------------------------------------------------------------
 * Markup-fragment VAR files — khtpm_strip_layout.c's ${var} substitution
 * (ported from chtpm_parser.c's own substitute_vars_naked()/set_var()
 * pattern, see khtpm-strip-parser-SCOPE.md's "manager pre-renders markup
 * fragments" section) needs strip_tabs/strip_shortcuts/strip_hq_items as
 * single string values. Multi-line-value decision (SCOPE.md's own
 * necessitation, left open there as "(a) escaped-newline row vs. (b) one
 * small file per VAR"): RESOLVED AS (b), one file per VAR
 * (strip_var_tabs.txt / strip_var_shortcuts.txt / strip_var_hqitems.txt),
 * following the scope doc's own stated lean ("simpler to implement
 * correctly and matches the house's general preference for one-concern-
 * per-file") — no concrete reason surfaced during implementation to prefer
 * (a) instead. These are ADDITIVE to strip_state.txt's existing TAB/
 * SHORTCUT/HQITEM rows (kept for compatibility/debugging, per SCOPE.md's
 * "can stay" option — nothing currently reads them besides the old
 * hardcoded parser this pass replaces, but removing them isn't necessary
 * either and keeps this file's diff smaller).
 * ------------------------------------------------------------------- */
static void write_small_file(const char *house_root, const char *rel_path, const char *buf) {
    char path[KTB_PATH_BUF], tmp[KTB_PATH_BUF];
    path_join2(path, sizeof(path), house_root, rel_path);
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *f = fopen(tmp, "w");
    if (!f) return;
    fputs(buf, f);
    fclose(f);
    remove(path);
    rename(tmp, path);
}

static void format_datetime(char *out, size_t out_sz, const char *lang) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    if (!tm_info) { snprintf(out, out_sz, "??:??"); return; }

    if (lang && strcmp(lang, "zh") == 0) {
        static const char *zh_wday[] = {"日", "一", "二", "三", "四", "五", "六"};
        int year = tm_info->tm_year + 1900;
        int mon = tm_info->tm_mon + 1;
        int mday = tm_info->tm_mday;
        snprintf(out, out_sz, "%04d年%02d月%02d日 周%s %02d:%02d",
                 year, mon, mday,
                 zh_wday[tm_info->tm_wday],
                 tm_info->tm_hour, tm_info->tm_min);
    } else {
        strftime(out, out_sz, "%Y-%m-%d %H:%M %a", tm_info);
    }
}

static void publish_var_fragments(const KtbState *s, const char *house_root) {
    char frag[48 * 1024];
    size_t off;

    /* strip_tabs: one <button label="N. entity" onClick="TAB:i"/> per tab,
     * matching khtpm_strip_header.chtpm/khtpm_strip_bottom.chtpm's onClick
     * dispatch convention (TAB:i -> KSC_TAB_BASE+i in khtpm_strip_parser.c).
     *
     * sprite="<entity dir>" (2026-08-11, "i want to put sprites back"):
     * carries s->tabs[i].path straight through as a new attribute — the
     * SAME path tp_taskbar.c's own tab_sprite(tabs[i].path) reads (that
     * function appends "/sprite.csv" itself, confirmed by reading it in
     * full; not duplicated here). khtpm_strip_layout.c's generic attribute
     * parser now recognizes "sprite" (see its own LAY_SPRITE_LEN comment)
     * and khtpm_strip_parser.c's draw_bottom() loads/blits it via its own
     * port of tab_sprite()/blit_tab_sprite(). Emitted unconditionally (even
     * when the entity has no sprite.csv) — the parser's tab_sprite() falls
     * back to text-only on a missing/unreadable file, matching legacy's own
     * "never a crash" contract, so no existence check is needed here. */
    off = 0;
    for (int i = 0; i < s->n_tabs && off < sizeof(frag); i++) {
        int n = snprintf(frag + off, sizeof(frag) - off,
                          "<button label=\"%d. %s\" onClick=\"TAB:%d\" sprite=\"%s\"/>",
                          s->tabs[i].nav, s->tabs[i].entity, i, s->tabs[i].path);
        if (n < 0) break;
        off += (size_t)n;
    }
    if (off >= sizeof(frag)) off = sizeof(frag) - 1;
    frag[off] = '\0';
    write_small_file(house_root, "#.desktop/strip_var_tabs.txt", frag);

    /* strip_shortcuts: one <button label="glyph" onClick="SHORTCUT:i"/> per
     * shortcut (SHORTCUT:i -> KSC_SHORTCUT_BASE+i). */
    off = 0;
    for (int i = 0; i < s->n_shortcuts && off < sizeof(frag); i++) {
        int n = snprintf(frag + off, sizeof(frag) - off,
                          "<button label=\"%s\" onClick=\"SHORTCUT:%d\"/>",
                          s->shortcuts[i].glyph, i);
        if (n < 0) break;
        off += (size_t)n;
    }
    if (off >= sizeof(frag)) off = sizeof(frag) - 1;
    frag[off] = '\0';
    write_small_file(house_root, "#.desktop/strip_var_shortcuts.txt", frag);

    /* strip_hq_items: one <button label=".." onClick="HQITEM:i"/> per row
     * of whichever cell's submenu is currently open (hq_menu[] is reused
     * across cells — see khtpm_taskbar_manager.h's KtbState comment).
     * Empty string when hq_open==0, matching the layout's own "only the
     * currently-ACTIVATEd cell's row renders" ACTIVATE-scope behavior —
     * the parser never shows this content unless some cell is actually
     * active, so an empty var here for the closed case is never visible
     * even if it briefly lags a state transition. */
    off = 0;
    if (s->hq_open) {
        /* REAL FIX 2026-08-30, direct live report ("pals entity in
         * 5.pals tb dropdown aren't actually drawing the pals pngs...
         * tb shows emojis and pngs already for user, and pid (clock)
         * see? why cant they use same?"). Direct hit: the real,
         * generic sprite mechanism (khtpm_strip_layout.c's
         * lay_get_sprite()/khtpm_strip_parser.c's tab_sprite()+
         * blit_tab_sprite()) already draws real sprite.csv images for
         * ANY button element that carries a real sprite="<dir>"
         * attribute - the strip_tabs fragment above already emits one
         * per tab (line ~234), and the header's own avatar cell does
         * too (via ${avatar_dir}). This HQITEM fragment (every HQ
         * popup's rows - session/desk/pals/etc., hq_menu[] is reused
         * across cells) never emitted one AT ALL, so every popup row
         * fell back to plain text-only, pals included - not a missing
         * drawing capability, just a missing attribute on this one
         * fragment. Fixed here, scoped to ONLY the pals cell
         * (s->hq_open==5, ktb_hq_open()'s own real "which" value,
         * confirmed via khtpm_taskbar_manager.h's own "hq_open is 0
         * (closed) or which" comment) - every other cell's rows
         * (session/desk/etc.) have no real per-row image and keep
         * their exact current text-only rendering, unaffected. */
        int is_pals = (s->hq_open == 5);
        char pals_root[KTB_PATH_BUF] = "";
        if (is_pals) livedesk_pals_root(s->house_root, pals_root, sizeof(pals_root));
        for (int i = 0; i < s->hq_n_menu && off < sizeof(frag); i++) {
            int n;
            /* command is "livedesk:pal:<name>" for every real pals row
             * (livedesk_build_pals_menu()'s own format, see that
             * function) - the trailing "Cancel" row's command is empty
             * and correctly falls through to the no-sprite branch. */
            if (is_pals && pals_root[0] && strncmp(s->hq_menu[i].command, "livedesk:pal:", 13) == 0) {
                n = snprintf(frag + off, sizeof(frag) - off,
                              "<button label=\"%s\" onClick=\"HQITEM:%d\" sprite=\"%s/%s\"/>",
                              s->hq_menu[i].label, i, pals_root, s->hq_menu[i].command + 13);
            } else if (strcmp(s->hq_menu[i].command, "livedesk:zorder-toggle") == 0) {
                /* REAL, NEW 2026-09-01 - the global always-on-top ("@")
                 * row. Emitted with a special onClick (not HQITEM:i) so
                 * the strip parser handles it locally with X (the parser
                 * owns g_dpy; this manager has NO Xlib access by design,
                 * see ktb_activate_tab()'s own comment chain). The label
                 * reflects the live mode, kept fresh in s->zorder_above
                 * by ktb_load_zorder_mode() on each reload. */
                if (s->zorder_above) {
                    n = snprintf(frag + off, sizeof(frag) - off,
                                  "<button label=\"@ always-on-top: ON\" onClick=\"ZORDER_TOGGLE\"/>");
                } else {
                    n = snprintf(frag + off, sizeof(frag) - off,
                                  "<button label=\"@ always-on-top: OFF\" onClick=\"ZORDER_TOGGLE\"/>");
                }
            } else {
                n = snprintf(frag + off, sizeof(frag) - off,
                              "<button label=\"%s\" onClick=\"HQITEM:%d\"/>",
                              s->hq_menu[i].label, i);
            }
            if (n < 0) break;
            off += (size_t)n;
        }
    }
    if (off >= sizeof(frag)) off = sizeof(frag) - 1;
    frag[off] = '\0';
    write_small_file(house_root, "#.desktop/strip_var_hqitems.txt", frag);

    /* Real gap fix (2026-08-11, direct request: "the button vars for
     * user/file/desk") — live username / file / desks labels, same
     * write_small_file() pattern as the three fragments above. Called
     * unconditionally each publish, matching tp_taskbar.c's own
     * once-a-second re-check-and-compare-before-redraw cadence in spirit
     * (this file's own publish cadence already gates on real state
     * changes via dispatch_code(), so no extra staleness guard needed
     * here — a cheap shell-out + string compare-free re-write each time
     * state actually changes is fine). */
    {
        /* KTB_PATH_BUF, not 256 — ktb_get_avatar_dir() builds a full
         * house_root-prefixed path, and this house's real paths are long
         * (deep emoji-segmented directories) — 256 would silently
         * truncate it, pointing tab_sprite() at a broken path. */
        char label[KTB_PATH_BUF];
        ktb_get_username(s, label, sizeof(label));
        write_small_file(house_root, "#.desktop/strip_var_username.txt", label);
        ktb_get_file_label(s, label, sizeof(label));
        write_small_file(house_root, "#.desktop/strip_var_file_label.txt", label);
        ktb_get_desks_label(s, label, sizeof(label));
        write_small_file(house_root, "#.desktop/strip_var_desks_label.txt", label);
        ktb_get_avatar_dir(s, label, sizeof(label));
        write_small_file(house_root, "#.desktop/strip_var_avatar_dir.txt", label);

        /* datetime — live-formatted date/time, updated every dispatch.
         * Read language from livedesk_taskbar.pdl's datetime_lang key
         * (default: zh for Chinese, en for English). */
        char pdl_path[KTB_PATH_BUF];
        snprintf(pdl_path, sizeof(pdl_path), "%s/#.desktop/livedesk_taskbar.pdl", house_root);
        char datetime_lang[16] = "zh";
        read_key_value(pdl_path, "datetime_lang", datetime_lang, sizeof(datetime_lang));
        char datetime_str[128];
        format_datetime(datetime_str, sizeof(datetime_str), datetime_lang);
        write_small_file(house_root, "#.desktop/strip_var_datetime.txt", datetime_str);
    }
}

static void write_strip_state(const char *house_root, const char *buf) {
    char path[KTB_PATH_BUF], tmp[KTB_PATH_BUF];
    path_join2(path, sizeof(path), house_root, "#.desktop/strip_state.txt");
    path_join2(tmp, sizeof(tmp), house_root, "#.desktop/strip_state.txt.tmp");
    FILE *f = fopen(tmp, "w");
    if (!f) return;
    fputs(buf, f);
    fclose(f);
    remove(path);
    rename(tmp, path);
}

/* Mirrors CHTPM's frame_changed.txt touch-signal (chtpm_parser.c's four
 * fopen(..., "a") appenders): a growing file is the whole signal, the byte
 * value itself is irrelevant. */
static void touch_frame_changed(const char *house_root) {
    char path[KTB_PATH_BUF];
    path_join2(path, sizeof(path), house_root, "#.desktop/strip_frame_changed.txt");
    FILE *f = fopen(path, "a");
    if (f) { fputc('x', f); fclose(f); }
}

static void xml_esc(const char *in, char *out, size_t out_sz) {
    size_t o = 0;
    for (; *in && o + 8 < out_sz; in++) {
        if (*in == '&') { memcpy(out + o, "&amp;", 5); o += 5; }
        else if (*in == '<') { memcpy(out + o, "&lt;", 4); o += 4; }
        else if (*in == '>') { memcpy(out + o, "&gt;", 4); o += 4; }
        else if (*in == '"') { memcpy(out + o, "&quot;", 6); o += 6; }
        else out[o++] = *in;
    }
    out[o] = '\0';
}

static void ui_put(char *buf, size_t *off, size_t cap, const char *key, const char *val) {
    char esc[2048];
    int n;
    xml_esc(val ? val : "", esc, sizeof(esc));
    if (*off >= cap) return;
    n = snprintf(buf + *off, cap - *off, "%s=%s\n", key, esc);
    if (n > 0) *off += (size_t)n;
}


static void publish_strip_ui(const KtbState *s, const char *house_root) {
    char body[96 * 1024], key[64], relay[KTB_PATH_BUF], act[KTB_PATH_BUF];
    char user_lab[KTB_PATH_BUF], file_lab[KTB_PATH_BUF], desks_lab[KTB_PATH_BUF];
    char avatar[KTB_PATH_BUF], dt[128], drop[64];
    size_t off = 0;
    int i;
    snprintf(relay, sizeof(relay),
             "%s/*.monads/*.livedesk-taskbar/ops/strip_relay.sh", house_root);
    ktb_get_username(s, user_lab, sizeof(user_lab));
    ktb_get_file_label(s, file_lab, sizeof(file_lab));
    ktb_get_desks_label(s, desks_lab, sizeof(desks_lab));
    ktb_get_avatar_dir(s, avatar, sizeof(avatar));
    {
        char pdl_path[KTB_PATH_BUF], datetime_lang[16] = "zh";
        snprintf(pdl_path, sizeof(pdl_path), "%s/#.desktop/livedesk_taskbar.pdl", house_root);
        read_key_value(pdl_path, "datetime_lang", datetime_lang, sizeof(datetime_lang));
        format_datetime(dt, sizeof(dt), datetime_lang);
    }
    drop[0] = '\0';
    if (s->hq_open >= 1 && s->hq_open <= 15)
        snprintf(drop, sizeof(drop), "strip-cell-%d", s->hq_open);

    ui_put(body, &off, sizeof(body), "username", user_lab);
    ui_put(body, &off, sizeof(body), "file_label", file_lab);
    ui_put(body, &off, sizeof(body), "desks_label", desks_lab);
    ui_put(body, &off, sizeof(body), "avatar_dir", avatar);
    ui_put(body, &off, sizeof(body), "datetime", dt);
    ui_put(body, &off, sizeof(body), "drop_target", drop);

    {
        char nbuf[16];
        snprintf(nbuf, sizeof(nbuf), "%d", s->hq_open ? s->hq_n_menu : 0);
        ui_put(body, &off, sizeof(body), "n_hqitems", nbuf);
    }
    if (s->hq_open) {
        int is_pals = (s->hq_open == 5);
        char pals_root[KTB_PATH_BUF] = "";
        if (is_pals) livedesk_pals_root(s->house_root, pals_root, sizeof(pals_root));
        for (i = 0; i < s->hq_n_menu; i++) {
            char spr[KTB_PATH_BUF] = "";
            const char *lab = s->hq_menu[i].label;
            char zlab[64];
            if (is_pals && pals_root[0] &&
                strncmp(s->hq_menu[i].command, "livedesk:pal:", 13) == 0)
                snprintf(spr, sizeof(spr), "%s/%s", pals_root, s->hq_menu[i].command + 13);
            if (strcmp(s->hq_menu[i].command, "livedesk:zorder-toggle") == 0) {
                snprintf(zlab, sizeof(zlab), "@ always-on-top: %s",
                         s->zorder_above ? "ON" : "OFF");
                lab = zlab;
                snprintf(key, sizeof(key), "hi_%d_label", i);
                ui_put(body, &off, sizeof(body), key, lab);
                snprintf(key, sizeof(key), "hi_%d_sprite", i);
                ui_put(body, &off, sizeof(body), key, "");
                snprintf(key, sizeof(key), "hi_%d_cmd", i);
                ui_put(body, &off, sizeof(body), key, "ZORDER_TOGGLE");
            } else {
                snprintf(key, sizeof(key), "hi_%d_label", i);
                ui_put(body, &off, sizeof(body), key, lab);
                snprintf(key, sizeof(key), "hi_%d_sprite", i);
                ui_put(body, &off, sizeof(body), key, spr);
                snprintf(act, sizeof(act), "'%s' %d", relay, 5000 + i);
                snprintf(key, sizeof(key), "hi_%d_cmd", i);
                ui_put(body, &off, sizeof(body), key, act);
            }
        }
    }

    {
        char nbuf[16];
        snprintf(nbuf, sizeof(nbuf), "%d", s->n_tabs);
        ui_put(body, &off, sizeof(body), "n_tabs", nbuf);
    }
    for (i = 0; i < s->n_tabs; i++) {
        char lab[KTB_PATH_BUF];
        snprintf(lab, sizeof(lab), "%d. %s", s->tabs[i].nav, s->tabs[i].entity);
        snprintf(key, sizeof(key), "tab_%d_label", i);
        ui_put(body, &off, sizeof(body), key, lab);
        snprintf(key, sizeof(key), "tab_%d_sprite", i);
        ui_put(body, &off, sizeof(body), key, s->tabs[i].path);
        snprintf(act, sizeof(act), "'%s' %d", relay, 2000 + i);
        snprintf(key, sizeof(key), "tab_%d_action", i);
        ui_put(body, &off, sizeof(body), key, act);
    }

    {
        char nbuf[16];
        snprintf(nbuf, sizeof(nbuf), "%d", s->n_shortcuts);
        ui_put(body, &off, sizeof(body), "n_sc", nbuf);
    }
    for (i = 0; i < s->n_shortcuts; i++) {
        snprintf(key, sizeof(key), "sc_%d_label", i);
        ui_put(body, &off, sizeof(body), key, s->shortcuts[i].glyph);
        snprintf(act, sizeof(act), "'%s' %d", relay, 3000 + i);
        snprintf(key, sizeof(key), "sc_%d_action", i);
        ui_put(body, &off, sizeof(body), key, act);
    }

    {
        char nbuf[16];
        snprintf(nbuf, sizeof(nbuf), "%d", s->n_hq_wins);
        ui_put(body, &off, sizeof(body), "n_hqwins", nbuf);
    }
    for (i = 0; i < s->n_hq_wins; i++) {
        char lab[768], oc[128];
        snprintf(lab, sizeof(lab), "🪟 %s", s->hq_wins[i].title);
        snprintf(key, sizeof(key), "hw_%d_label", i);
        ui_put(body, &off, sizeof(body), key, lab);
        snprintf(key, sizeof(key), "hw_%d_minclass", i);
        ui_put(body, &off, sizeof(body), key, s->hq_wins[i].minimized ? " hqwin-minimized" : "");
        snprintf(oc, sizeof(oc), "FOCUSWIN:0x%lx:%d",
                 s->hq_wins[i].win, s->hq_wins[i].pid);
        snprintf(key, sizeof(key), "hw_%d_onclick", i);
        ui_put(body, &off, sizeof(body), key, oc);
    }

    ui_put(body, &off, sizeof(body), "cliio_on", s->cliio_active ? "1" : "0");
    ui_put(body, &off, sizeof(body), "cliio_label",
           s->cliio_buffer[0] ? s->cliio_buffer : (s->cliio_op[0] ? s->cliio_op : "input"));
    snprintf(act, sizeof(act), "'%s' submit", relay);
    ui_put(body, &off, sizeof(body), "cliio_action", act);

    if (off >= sizeof(body)) off = sizeof(body) - 1;
    body[off] = '\0';
    write_small_file(house_root, "#.desktop/strip_ui.txt", body);
}

static void publish_state(const KtbState *s, const char *house_root) {
    char buf[64 * 1024];
    serialize_state(s, buf, sizeof(buf));
    write_strip_state(house_root, buf);
    publish_var_fragments(s, house_root);
    publish_strip_ui(s, house_root);
    touch_frame_changed(house_root);
}

/* Run a shortcut's command. The manager driver has no Xlib/platform code,
 * but system()-launching an external app is already something
 * khtpm_taskbar_manager.c itself does in several places (livedesk_copy_full,
 * livedesk_spawn_desk, etc. — all use system() directly), so this mirrors
 * that existing precedent rather than inventing a new "platform hook"
 * mechanism. Modeled on khtpm_taskbar_plat_x11.c's ktb_plat_run_command(),
 * duplicated here (not called directly) since that function lives in the
 * X11-only plat file and this binary must build without libX11. */
#ifndef _WIN32
/* macOS leg (2026-08-22): macOS ships no `setsid` binary and no
 * `xdg-open`. Every HQ-menu row (dir/cli/db/events/...) flows through
 * here, so both are handled at runtime: drop the setsid prefix (nohup+&
 * already detaches for this launcher shape — same thing the mac start
 * script does) and translate xdg-open → open. Linux output is
 * byte-identical; PDL stays canonical (no per-OS rewrites). */
# ifdef __APPLE__
#  define KTB_SETSID ""
static void ktb_portable_darwin(char *cmd, size_t sz) {
    if (strncmp(cmd, "xdg-open", 8) == 0 && (cmd[8] == ' ' || cmd[8] == '\0')) {
        char rest[KTB_PATH_BUF];
        snprintf(rest, sizeof(rest), "%s", cmd[8] ? cmd + 9 : "");
        snprintf(cmd, sz, "open %s", rest);
    }
}
# else
#  define KTB_SETSID "setsid "
static void ktb_portable_darwin(char *cmd, size_t sz) { (void)cmd; (void)sz; }
# endif
static void run_shortcut(const char *cmd) {
    if (!cmd || !cmd[0]) return;
    char portable[KTB_PATH_BUF];
    ktb_action_portable(cmd, portable, sizeof(portable));
    ktb_portable_darwin(portable, sizeof(portable));
    char sh[KTB_PATH_BUF * 2];
    snprintf(sh, sizeof(sh), KTB_SETSID "nohup sh -c '%s' >/dev/null 2>&1 &", portable);
    int rc = system(sh);
    (void)rc;
}
#else
static void run_shortcut(const char *cmd) { (void)cmd; }
#endif

/* Dispatch one decimal code (see khtpm_strip_codes.h) into the existing
 * ktb_* API surface. Returns 1 if state may have mutated (caller decides
 * whether to publish — kept simple: any recognized code is treated as
 * potentially-mutating, matching how cheap ktb_reload()/ktb_focus_delta()
 * etc. already are). */
static void dispatch_code(KtbState *s, int code) {
    /* Same precedence order as tp_taskbar.c's agent_relay_dispatch():
     * cli-io modal > hq popup > (else) the pre-existing bottom-bar
     * digit/focus/tab/shortcut dispatch below, unchanged. */
    if (s->cliio_active) {
        if (s->cliio_typing) {
            if (code == KSC_ESCAPE) ktb_cliio_stop_typing(s);
            else if (code == KSC_BACKSPACE) ktb_cliio_backspace(s);
            else if (code == KSC_ENTER) ktb_cliio_submit(s);
            else if (code >= 0x20 && code < 0x7f) ktb_cliio_type(s, (char)code);
        } else {
            if (code == KSC_ESCAPE) ktb_cliio_close(s);
            else if (code == KSC_ENTER) ktb_cliio_start_typing(s);
            /* Additive fix (2026-08-11): cli-io's 2 rows (field/cancel)
             * previously had no keyboard focus-toggle at all — see
             * ktb_cliio_focus_delta()'s own header comment in
             * khtpm_taskbar_manager.c for why, ported from tp_taskbar.c's
             * real cli-io nav-mode Up/Down handling. */
            else if (code == KSC_FOCUS_LEFT) ktb_cliio_focus_delta(s, -1);
            else if (code == KSC_FOCUS_RIGHT) ktb_cliio_focus_delta(s, 1);
        }
        return;
    }
    /* Bug 1 fix (2026-08-11): right-click "arm nav", ported from
     * tp_taskbar.c's button==3 handling — must be checked BEFORE the
     * hq_open branch below since legacy's close_popups() (called from that
     * same branch) is exactly what tears down an open hq popup on a
     * right-click. Gated out while cli-io is active, matching close_popups()
     * itself never touching g_cliio_active/g_cliio_win. */
    if (code == KSC_NAV_ARM) {
        ktb_nav_arm(s);
        return;
    }
    /* Real HQ menu's "X.quit" row (ktb_hq_open() which==5) can only
     * raise a flag from inside ktb_hq_activate() - see that field's own
     * header comment in khtpm_taskbar_manager.h. This check must be BEFORE
     * the hq_open block because ktb_hq_close() sets hq_open=0, so if the
     * check stayed inside the hq_open block, it would be unreachable.
     * This is the one place that actually stops the event loop, mirroring
     * tp_taskbar.c's own "quit" command branch in agent_relay_dispatch(). */
    if (s->hq_quit_requested) {
        s->hq_quit_requested = 0;
        ktb_quit_and_save(s);
        #ifndef _WIN32
        pid_t ppid = getppid();
        if (ppid > 1) kill(ppid, SIGTERM);
        #endif
        exit(0);
    }
    if (s->hq_open) {
        if (code == KSC_ESCAPE) {
            ktb_hq_close(s);
        } else if (code == KSC_ENTER) {
            ktb_hq_activate(s, s->hq_focus);
        } else if (code == KSC_FOCUS_LEFT) {
            ktb_hq_focus_delta(s, -1);
        } else if (code == KSC_FOCUS_RIGHT) {
            ktb_hq_focus_delta(s, 1);
        } else if (code >= '0' && code <= '9') {
            ktb_hq_digit(s, code - '0');
        } else if (code >= KSC_HQ_ITEM_BASE && code < KSC_HQ_ITEM_BASE + KTB_LIVEDESK_DYN_MAX) {
            ktb_hq_activate(s, code - KSC_HQ_ITEM_BASE);
        } else if (code >= KSC_HQ_HEADER_BASE && code < KSC_HQ_HEADER_BASE + KTB_STRIP_N_CELLS + 1) {
            /* Real bug fix (2026-08-12, direct live-test report: "click
             * another button from header while previous is open, it just
             * shows [the] open submenu under [the] new button"). Root
             * cause: this whole s->hq_open branch unconditionally
             * `return`s once it's entered, so a header-cell click code
             * (KSC_HQ_HEADER_BASE+n, sent the instant the CLIENT
             * optimistically switches its local popup scope to the
             * newly-clicked button - see khtpm_strip_parser.c's
             * dispatch_onclick() ACTIVATE branch) matched none of the
             * ESCAPE/ENTER/FOCUS/digit/HQITEM checks above and was
             * silently swallowed - the manager never rebuilt hq_menu[]
             * for the new cell, so the client kept showing the OLD
             * cell's stale ${strip_hq_items} content under the new
             * button forever (no self-correcting tick ever came).
             * Fixed: treat a header-cell click as "switch directly to
             * that cell's menu" even while another cell's menu is
             * already open, same as a fresh click from the closed state
             * below does. */
            ktb_hq_open(s, code - KSC_HQ_HEADER_BASE);
        }
        if (s->hq_quit_requested) {
            s->hq_quit_requested = 0;
            ktb_quit_and_save(s);
            #ifndef _WIN32
            pid_t ppid = getppid();
            if (ppid > 1) kill(ppid, SIGTERM);
            #endif
            exit(0);
        }
        return;
    }
    /* Full 12-cell strip header click (pass 2, 2026-08-11): which =
     * code - KSC_HQ_HEADER_BASE, matching KtbState's own 1=HQ..12=network
     * cell-order comment. which==2 (USER) used to have no submenu at all
     * (routed to ktb_strip_user_activate() instead of ktb_hq_open() so its
     * legacy strip_user_cmd action wasn't silently swallowed by the
     * "inert cell" branch) - changed 2026-08-11 to route through
     * ktb_hq_open() like every other cell now that USER has a real
     * submenu (New User/Switch/Logout, see livedesk_build_user_menu() in
     * khtpm_taskbar_manager.c + au11-hq/USER_CREATION.md). */
    if (code >= KSC_HQ_HEADER_BASE && code < KSC_HQ_HEADER_BASE + KTB_STRIP_N_CELLS + 1) {
        int which = code - KSC_HQ_HEADER_BASE;
        ktb_hq_open(s, which);
        return;
    }

    if (code >= 48 && code <= 57) {
        ktb_digit_push(s, (char)code);
    } else if (code == KSC_BACKSPACE) {
        ktb_digit_backspace(s);
    } else if (code == KSC_ENTER) {
        ktb_nav_enter(s);
    } else if (code == KSC_ESCAPE) {
        ktb_digit_clear(s);
    } else if (code == KSC_FOCUS_LEFT) {
        ktb_nav_focus_delta(s, -1);
    } else if (code == KSC_FOCUS_RIGHT) {
        ktb_nav_focus_delta(s, 1);
    } else if (code == KSC_CLOSE_QUIT) {
        ktb_quit_and_save(s);
        g_running = 0;
    } else if (code >= KSC_TAB_BASE && code < KSC_TAB_BASE + KTB_MAX_TABS) {
        ktb_activate_tab(s, code - KSC_TAB_BASE);
    } else if (code >= KSC_SHORTCUT_BASE && code < KSC_SHORTCUT_BASE + KTB_MAX_SHORTCUTS) {
        int idx = code - KSC_SHORTCUT_BASE;
        if (idx >= 0 && idx < s->n_shortcuts)
            run_shortcut(s->shortcuts[idx].command);
    }
    /* unrecognized codes are silently ignored, matching CHTPM's own
     * `if (code > 0)` best-effort dispatch in tp_taskbar.c's
     * agent_relay_dispatch() callers. */
}

/* Cheap size-check + cursor polling of strip_history.txt, mirroring
 * tp_taskbar.c's poll_agent_relay() (see that function, ~line 3368):
 * short-circuit on unchanged/absent file, resync (don't replay) on
 * truncation, never consume a partial trailing line, don't replay backlog
 * on startup (cursor seeded to current size on first sight of the file). */
static long g_relay_cursor = -1;

static int poll_strip_history(KtbState *s, const char *house_root) {
    char path[KTB_PATH_BUF];
    path_join2(path, sizeof(path), house_root, "#.desktop/strip_history.txt");
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    if (g_relay_cursor < 0) { g_relay_cursor = st.st_size; return 0; }
    if (st.st_size < g_relay_cursor) { g_relay_cursor = st.st_size; return 0; }
    if (st.st_size == g_relay_cursor) return 0;

    FILE *f = fopen(path, "r");
    if (!f) return 0;
    fseek(f, g_relay_cursor, SEEK_SET);
    char line[32];
    long consumed = g_relay_cursor;
    int mutated = 0;
    while (fgets(line, sizeof(line), f)) {
        char *nl = strchr(line, '\n');
        if (!nl) break; /* partial line - wait for the rest next poll */
        *nl = '\0';
        long here = ftell(f);
        int code = atoi(line);
        if (code > 0) {
            dispatch_code(s, code);
            mutated = 1;
        }
        consumed = here;
    }
    fclose(f);
    g_relay_cursor = consumed;
    return mutated;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: khtpm_taskbar_manager_main <house_root>\n");
        return 1;
    }
    const char *house_root = argv[1];

    /* REAL FIX 2026-08-19 (direct live report: taskbar HQ menu's "cli"
     * row - and any other row whose command is a relative/portable path,
     * e.g. ktb_action_portable()'s own rewritten &.widgits/$.crypts/
     * @.apps paths - silently did nothing). Root cause: this process
     * never chdir()'d to house_root, so its cwd was whatever directory
     * launched it (confirmed live: /proc/<pid>/cwd was $HOME, not house
     * root) - every system()/popen() call elsewhere in this file that
     * passes a relative path (the entire point of ktb_action_portable(),
     * plus raw relative commands like open_cli.sh's PDL row) has always
     * silently depended on cwd already being house_root, with nothing
     * enforcing it. One chdir() here, once, at startup, makes that
     * assumption actually true regardless of how/from-where this binary
     * gets launched. */
#ifdef _WIN32
    {
        wchar_t wh[KTB_PATH_BUF];
        if (MultiByteToWideChar(CP_UTF8, 0, house_root, -1, wh, KTB_PATH_BUF))
            SetCurrentDirectoryW(wh);
    }
#else
    { int chdir_rc = chdir(house_root); (void)chdir_rc; }
#endif

#ifndef _WIN32
    signal(SIGTERM, on_sigterm);
    signal(SIGINT, on_sigterm);
#endif

    KtbState st;
    ktb_init(&st, house_root);

    /* REAL FIX 2026-08-12, direct report ("i sawn u double render all
     * entities in toolbar on accident. we should have a guard so that
     * never happens"): livedesk_taskbar.pid (st.pid_path) already
     * existed but nothing ever CHECKED it before this process started
     * doing real work - it was purely a courtesy record for humans/
     * scripts to read, not an enforced singleton. Traced live: manually
     * launching this binary a second time (while strip_parser's own
     * ensure_manager_running()/launch_manager() already had a live one
     * running as its tracked child) produced TWO managers both polling
     * the same strip_history.txt/livedesk_open.txt/reset dispatch at
     * once - every relay command got handled twice, so "reset entities"
     * spawned every entity twice. This is now a REAL, enforced
     * singleton: if pid_path names a still-alive OTHER process, this
     * instance refuses to start at all (kill(pid,0) liveness probe -
     * same convention tp_desktop_window.c's own "already open -> just
     * add a tab" singleton check already uses elsewhere in this house).
     * A second manager launched by mistake (human or code) now exits
     * immediately instead of silently running alongside the real one. */
#ifndef _WIN32
    {
        FILE *pf = fopen(st.pid_path, "r");
        if (pf) {
            long existing = 0;
            if (fscanf(pf, "%ld", &existing) == 1) {
                fclose(pf);
                if (existing > 0 && existing != (long)ktb_getpid() &&
                    kill((pid_t)existing, 0) == 0) {
                    fprintf(stderr,
                        "khtpm_taskbar_manager_main: refusing to start - "
                        "pid %ld already running for this house_root "
                        "(%s)\n", existing, st.pid_path);
                    return 1;
                }
            } else {
                fclose(pf);
            }
        }
    }
#endif

    ktb_write_pidfile(&st, ktb_getpid());
    ktb_reload(&st);
    publish_state(&st, house_root); /* initial state so the parser has something to draw */

    /* Fixed-interval polling, matching poll_agent_relay()'s own precedent
     * in this taskbar (~400ms tick, same interval khtpm_taskbar_plat_x11.c
     * already uses for its select() timeout) — see design §5 "Poll
     * interval" decision. */
    int active_ticks = ACTIVE_HOLD_TICKS; /* start hot - matches TPMOS's own "layout in focus" active default right after launch */
    while (g_running) {
        int mutated = poll_strip_history(&st, house_root);
        /* also periodically reload so external tab/shortcut/theme file
         * changes (livedesk_open.txt, livedesk_shortcuts.pdl, etc.) are
         * picked up, matching ktb_plat_run()'s own per-tick ktb_reload(). */
        int prev_n_tabs = st.n_tabs, prev_n_sc = st.n_shortcuts;
        int prev_focus = st.tab_focus_idx;
        /* REAL, NEW 2026-09-01 - the global always-on-top toggle runs
         * LOCALLY in the strip parser (it owns the X Display) and writes
         * #.desktop/khtpm_zorder_mode.state.txt, never sending a code to
         * this manager. This manager's ktb_reload() mirrors that file into
         * s->zorder_above every tick, but without this comparison the
         * mirror change never counted as "reload_changed", so the strip
         * fragment never republished and the open HQ menu kept showing the
         * stale ON/OFF label. Include it so a toggle republishes at once. */
        int prev_zorder = st.zorder_above;
        int prev_n_hq = st.n_hq_wins;
        ktb_reload(&st);
        int reload_changed = (st.n_tabs != prev_n_tabs || st.n_shortcuts != prev_n_sc ||
                               st.tab_focus_idx != prev_focus || st.zorder_above != prev_zorder ||
                               st.n_hq_wins != prev_n_hq);
        {
            char pdl_path[KTB_PATH_BUF], datetime_lang[16] = "zh", dt[128];
            static char last_dt[128];
            snprintf(pdl_path, sizeof(pdl_path), "%s/#.desktop/livedesk_taskbar.pdl", house_root);
            read_key_value(pdl_path, "datetime_lang", datetime_lang, sizeof(datetime_lang));
            format_datetime(dt, sizeof(dt), datetime_lang);
            if (strcmp(dt, last_dt) != 0) {
                snprintf(last_dt, sizeof(last_dt), "%s", dt);
                reload_changed = 1;
            }
        }
        if (mutated || reload_changed)
            publish_state(&st, house_root);
        if (mutated || reload_changed) active_ticks = ACTIVE_HOLD_TICKS;
        else if (active_ticks > 0) active_ticks--;
        int interval = active_ticks > 0 ? POLL_INTERVAL_ACTIVE_USEC : POLL_INTERVAL_IDLE_USEC;

#ifndef _WIN32
        struct timeval tv = { 0, interval };
        select(0, NULL, NULL, NULL, &tv);
#else
        Sleep(interval / 1000);
#endif
    }

    /* Clean up any subwindows (h-ai, db-hq, etc.) and desktop entities on exit */
    ktb_quit_and_save(&st);

    ktb_unlink_pidfile(&st);
    return 0;
}
