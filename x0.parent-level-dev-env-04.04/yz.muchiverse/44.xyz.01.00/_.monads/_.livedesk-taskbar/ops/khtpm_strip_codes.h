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
#ifndef KHTPM_STRIP_CODES_H
#define KHTPM_STRIP_CODES_H

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
 * on dispatch order) - the manager driver's dispatch_code() replicates the
 * same precedence, so no separate "hq enter" / "hq escape" codes exist.
 * Printable ASCII (already reserved 0-127 per this header's own convention
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

#endif
