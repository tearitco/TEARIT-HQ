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
#include "khtpm_strip_codes.h"

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
        for (int i = 0; i < s->hq_n_menu && off < sizeof(frag); i++) {
            int n = snprintf(frag + off, sizeof(frag) - off,
                              "<button label=\"%s\" onClick=\"HQITEM:%d\"/>",
                              s->hq_menu[i].label, i);
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

static void publish_state(const KtbState *s, const char *house_root) {
    char buf[64 * 1024];
    serialize_state(s, buf, sizeof(buf));
    write_strip_state(house_root, buf);
    publish_var_fragments(s, house_root);
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
        ktb_reload(&st);
        int reload_changed = (st.n_tabs != prev_n_tabs || st.n_shortcuts != prev_n_sc ||
                               st.tab_focus_idx != prev_focus);
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
