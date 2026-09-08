#!/bin/sh
# menu-widget.sh <house_root> <widget> <MODE> <start-token> <result-verb>
#
# The one launcher shim for a strip menu row whose _cmd is
#   widget:<widget> <MODE> <start-token>
# (dispatched by ktb_hq_activate()'s "widget:" branch, which appends the
# house_root and the row's result-verb). Opens the widget modally, waits
# for the pick, and drops it in #.desktop/livedesk_widget_result.txt as
#   verb=<result-verb>
#   value=<picked path, or empty on cancel>
# for ktb_poll_widget_result() (taskbar manager main loop) to consume.
#
# Replaces the per-feature pick-session.sh / save-as-session.sh.
#
# <widget>       : only "file-explorer" today (-> &.widgits/file-explorer/fe-pick.sh)
# <MODE>         : LOAD | SAVE (passed straight to fe-pick.sh)
# <start-token>  : @sessions | @package | @house | an absolute path
# <result-verb>  : open-session | save-as | load-map | ... (routing key)
set -u

HOUSE="${1:-}"
WIDGET="${2:-}"
MODE="${3:-LOAD}"
START_TOK="${4:-@house}"
VERB="${5:-}"

[ -n "$HOUSE" ] && [ -d "$HOUSE" ] || { echo "menu-widget.sh: bad house_root" >&2; exit 1; }
[ -n "$VERB" ] || { echo "menu-widget.sh: missing result-verb" >&2; exit 1; }

# resolve the start-token to a real directory
case "$START_TOK" in
    @sessions)
        START=""
        for d in "$HOUSE"/xyzfs/users/*/home/livedesk/sessions; do
            [ -d "$d" ] && { START="$d"; break; }
        done
        [ -n "$START" ] || START="$(find "$HOUSE/xyzfs" -type d -name sessions 2>/dev/null | head -n1)"
        ;;
    @package) START="$HOUSE" ;;   # package dir not meaningful for the strip itself
    @house)   START="$HOUSE" ;;
    /*)       START="$START_TOK" ;;
    *)        START="$HOUSE/$START_TOK" ;;
esac
[ -n "$START" ] && [ -d "$START" ] || START="$HOUSE"

PICK=""
case "$WIDGET" in
    file-explorer)
        PICK="$(sh "$HOUSE/&.widgits/file-explorer/fe-pick.sh" "$MODE" "$START")"
        ;;
    *)
        echo "menu-widget.sh: unknown widget '$WIDGET'" >&2
        exit 1
        ;;
esac

OUT="$HOUSE/#.desktop/livedesk_widget_result.txt"
TMP="$OUT.tmp.$$"
{
    printf 'verb=%s\n' "$VERB"
    printf 'value=%s\n' "$PICK"
} > "$TMP"
mv -f "$TMP" "$OUT"
