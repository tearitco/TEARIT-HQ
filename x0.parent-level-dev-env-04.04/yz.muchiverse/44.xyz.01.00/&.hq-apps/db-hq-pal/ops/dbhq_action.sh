#!/bin/sh
# dbhq_action.sh - the only write path for db-hq-pal. Called by the
# renderer from the static template's action= strings:
#
#   <item action="'.../dbhq_action.sh' 'tab' '<file>' '<tag>' '<title>'"/>
#        -> argv:  tab <file> <tag> <title> <pkg> <house>
#   <item action="'.../dbhq_action.sh' 'sel' '<n>'"/>
#        -> argv:  sel <n> <pkg> <house>
#   <item action="'.../dbhq_action.sh' 'open-ce'"/>
#        -> argv:  open-ce <pkg> <house>
#
# It maintains <pkg>/state/active.pdl (KEY | value lines); the PAL
# projector reads that every tick.
set -u
VERB="${1:-}"

active_pdl() { printf '%s/state/active.pdl' "$1"; }

write_active() {   # write_active <pkg> <file> <tag> <title> <sel>
    p="$(active_pdl "$1")"
    mkdir -p "$1/state"
    {
        printf 'SECTION | KEY   | VALUE\n'
        printf 'FILE    | file  | %s\n' "$2"
        printf 'TAG     | tag   | %s\n' "$3"
        printf 'TITLE   | title | %s\n' "$4"
        printf 'SEL     | sel   | %s\n' "$5"
    } > "$p.tmp" && mv "$p.tmp" "$p"
}

read_field() {   # read_field <pkg> <key>
    p="$(active_pdl "$1")"
    [ -f "$p" ] || return 0
    awk -F'|' -v k="$2" '{
        key=$2; gsub(/^[ \t]+|[ \t]+$/,"",key);
        if (key==k) { v=$3; gsub(/^[ \t]+|[ \t]+$/,"",v); print v; exit }
    }' "$p"
}

case "$VERB" in
    tab)
        FILE="${2:-}"; TAG="${3:-}"; TITLE="${4:-}"; PKG="${5:-}"
        [ -n "$PKG" ] || { echo "dbhq_action: tab needs pkg" >&2; exit 1; }
        write_active "$PKG" "$FILE" "$TAG" "$TITLE" 0
        ;;
    sel)
        N="${2:-0}"; PKG="${3:-}"; HOUSE="${4:-}"
        [ -n "$PKG" ] || { echo "dbhq_action: sel needs pkg" >&2; exit 1; }
        FILE="$(read_field "$PKG" file)"
        TAG="$(read_field "$PKG" tag)"
        TITLE="$(read_field "$PKG" title)"
        # Common Events: selection is enough — dbhq_ce_bridge starts the
        # events-hq manager+projector on common_events/<name>/event_pkg
        # inside this window (same editor as entity events-hq).
        write_active "$PKG" "$FILE" "$TAG" "$TITLE" "$N"
        ;;
    field)
        # argv: field <key> <pkg> <house> <typed_value>
        KEY="${2:-}"; PKG="${3:-}"; HOUSE="${4:-}"; VALUE="${5:-}"
        [ -n "$PKG" ] || { echo "dbhq_action: field needs pkg" >&2; exit 1; }
        FILE="$(read_field "$PKG" file)"
        TAG="$(read_field "$PKG" tag)"
        SEL="$(read_field "$PKG" sel)"
        [ -n "$FILE" ] && [ -n "$TAG" ] && [ -n "$KEY" ] || exit 0
        ST="$HOUSE/#.desktop/$FILE"
        [ -f "$ST" ] || exit 0
        python3 - "$ST" "$TAG" "$SEL" "$KEY" "$VALUE" <<'PY'
import sys
path, tag, sel_s, key, value = sys.argv[1:6]
sel = int(sel_s or "0")
lines = open(path).read().splitlines()
rec = -1
out = []
for line in lines:
    parts = [p.strip() for p in line.split("|")]
    if len(parts) >= 3 and parts[0] == tag and parts[1] == "id":
        rec += 1
    if rec == sel and len(parts) >= 3 and parts[0] == tag and parts[1] == key:
        pad = " " * max(1, 17 - len(key))
        line = "%-12s | %-18s | %s" % (tag, key, value)
    out.append(line)
open(path, "w").write("\n".join(out) + "\n")
PY
        ;;
    open-ce)
        HOUSE="${3:-}"
        [ -n "$HOUSE" ] && [ -d "$HOUSE" ] || { echo "dbhq_action: open-ce needs house" >&2; exit 1; }
        CE="$HOUSE/*.monads/*.muchi-pet/ops/open_db_hq.sh"
        [ -f "$CE" ] && setsid nohup sh "$CE" "$HOUSE" >/dev/null 2>&1 &
        ;;
    *)
        echo "dbhq_action: unknown verb '$VERB'" >&2
        exit 1
        ;;
esac
