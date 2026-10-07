#!/bin/sh
# close_listed.sh <house_root> [--dry-run] - close every process listed in close_on_restart.pdl (see that file for why). Called by button.sh on restart/run/reset/quit.
# --dry-run prints what WOULD be closed and kills nothing. A process matches only if its command line holds the row's substring AND <house_root>.
# Safe against self-match: the process table is snapshotted to a file BEFORE the matcher runs, and this script's own pid and its parents are never signalled.
# Prints one line per process: "close_listed: TERM <pid> (<name>)". Exit 0 always.
HOUSE="${1:-}"; DRY=0; [ "${2:-}" = "--dry-run" ] && DRY=1
[ -n "$HOUSE" ] && [ -d "$HOUSE" ] || { echo "usage: close_listed.sh <house_root> [--dry-run]" >&2; exit 0; }
HERE="$(cd "$(dirname "$0")" && pwd)"; LIST="${CLOSE_LIST:-$HERE/close_on_restart.pdl}"
[ -f "$LIST" ] || exit 0
SNAP="$(mktemp)"; HITS="$(mktemp)"; trap 'rm -f "$SNAP" "$HITS"' EXIT
ps -eo pid,args > "$SNAP"                                  # snapshot first: the matcher below is not in it
# my own pid and every ancestor (a restart is run from a shell that may itself be a descendant of a listed window)
PROT=" $$"; p=$$; while [ -n "$p" ] && [ "$p" -gt 1 ] 2>/dev/null; do p="$(ps -o ppid= -p "$p" 2>/dev/null | tr -d ' ')"; [ -n "$p" ] && PROT="$PROT $p"; done
while IFS= read -r line; do
    case "$line" in CLOSE*) ;; *) continue ;; esac
    NAME="$(echo "$line" | awk -F'|' '{gsub(/^[ \t]+|[ \t]+$/,"",$2); print $2}')"; SUB="$(echo "$line" | awk -F'|' '{gsub(/^[ \t]+|[ \t]+$/,"",$3); print $3}')"
    [ -n "$SUB" ] || continue
    awk -v sub_="$SUB" -v house="$HOUSE" -v name="$NAME" 'NR > 1 { pid = $1; $1 = ""; if (index($0, sub_) > 0 && index($0, house) > 0) print pid "\t" name }' "$SNAP" >> "$HITS"
done < "$LIST"
sort -u "$HITS" | while IFS="$(printf '\t')" read -r pid name; do
    case "$PROT" in *" $pid "*|*" $pid") continue ;; esac
    echo "close_listed: TERM $pid ($name)"
    [ "$DRY" = 1 ] || kill -TERM "$pid" 2>/dev/null
done
if [ "$DRY" = 0 ] && [ -s "$HITS" ]; then
    sleep 1
    sort -u "$HITS" | while IFS="$(printf '\t')" read -r pid name; do
        case "$PROT" in *" $pid "*|*" $pid") continue ;; esac
        kill -0 "$pid" 2>/dev/null && { echo "close_listed: KILL $pid ($name)"; kill -KILL "$pid" 2>/dev/null; }
    done
fi
exit 0
