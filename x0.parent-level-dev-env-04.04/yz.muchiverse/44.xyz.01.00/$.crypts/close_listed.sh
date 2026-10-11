#!/bin/sh
# macOS has no setsid binary (same convention as run_khtpm_strip.sh): expand to nothing there, keep real setsid on Linux.
SETSID="setsid"
[ "$(uname)" = "Darwin" ] && SETSID=""
# close_listed.sh <house_root> [--dry-run] [--relaunch] - close every process listed in close_on_restart.pdl (see that file for why). Called by button.sh on restart/run/reset/quit.
# --dry-run prints what WOULD be closed and kills nothing. --relaunch: after closing, run the row's optional 4th-field command (from the house root, detached) for every row that had a running process. A process matches only if its command line holds the row's substring AND <house_root>.
# Safe against self-match: the process table is snapshotted to a file BEFORE the matcher runs, and this script's own pid and its parents are never signalled.
# Prints one line per process: "close_listed: TERM <pid> (<name>)". Exit 0 always.
HOUSE="${1:-}"; DRY=0; RELAUNCH=0
for a in "${2:-}" "${3:-}"; do case "$a" in --dry-run) DRY=1 ;; --relaunch) RELAUNCH=1 ;; esac; done
[ -n "$HOUSE" ] && [ -d "$HOUSE" ] || { echo "usage: close_listed.sh <house_root> [--dry-run] [--relaunch]" >&2; exit 0; }
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
# relaunch the rows that were running (not in a dry run)
if [ "$DRY" = 0 ] && [ "$RELAUNCH" = 1 ] && [ -s "$HITS" ]; then
    sleep 0.5
    while IFS= read -r line; do
        case "$line" in CLOSE*) ;; *) continue ;; esac
        NAME="$(echo "$line" | awk -F'|' '{gsub(/^[ \t]+|[ \t]+$/,"",$2); print $2}')"; CMD="$(echo "$line" | awk -F'|' '{gsub(/^[ \t]+|[ \t]+$/,"",$4); print $4}')"
        [ -n "$CMD" ] || continue
        awk -F'\t' -v n="$NAME" '$2 == n {f = 1} END {exit !f}' "$HITS" || continue
        echo "close_listed: RELAUNCH ($NAME): $CMD"
        (cd "$HOUSE" && $SETSID sh -c "$CMD" </dev/null >/dev/null 2>&1 &)
    done < "$LIST"
fi
exit 0
