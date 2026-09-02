#!/bin/bash
# win-trip.sh - run BEFORE copying the house to Windows, and again AFTER
# copying it back. Fixes the one thing that physically breaks every round
# trip: NTFS cannot store names containing '*', but this house names its
# monads/dirs with a leading '*.' (e.g. *.monads, *.START_BUTTON). The
# Windows code aliases '*.' <-> '_.' at RESOLVE TIME (see crypt_autostart.c,
# khtpm_strip_x11_win.c) - so only the on-disk NAMES ever need converting,
# never file contents.
#
#   ./win-trip.sh to-win     # '*.' names -> '_.', writes WIN-TRIP-MANIFEST.txt
#   ./win-trip.sh to-linux   # renames them back using the manifest,
#                            # restores exec bits the transfer stripped
#   ./win-trip.sh status     # show which mode is needed right now
#
# WHY A MANIFEST: a few entries are LEGITIMATELY underscore-named (e.g.
# _.0.aigent-testing-k9.txt) and must NEVER be flipped back to '*.'. The
# manifest records exactly which paths were renamed at to-win time, so
# to-linux touches only those. Without it, to-linux refuses to guess.
#
# Run it from the HOUSE ROOT (or any subtree you're about to ship):
#   cd <house> && $.crypts/win-trip.sh to-win

set -u

SELF="$(cd "$(dirname "$0")" && pwd)"
ROOT="${2:-$(pwd)}"
MANIFEST="$ROOT/WIN-TRIP-MANIFEST.txt"

# find_star_paths <root> - deepest-first list of paths whose BASENAME starts
# with literal '*.' (depth-first order so children rename before parents).
find_star_paths() {
    find "$ROOT" -depth \( -path "*/pieces/sessions/*" \
        -o -path "*/harness-reports/*" -o -path "*/build-reports/*" \
        -o -name "*.backup-*" -o -path "*_BACKUP*" \) -prune -o \
        -type d -print0 -o -type f -print0 2>/dev/null |
    while IFS= read -r -d '' p; do
        case "$(basename "$p")" in
            '*'*) echo "$p" ;;
        esac
    done | awk -F/ '{ print NF-1, $0 }' | sort -rn | cut -d' ' -f2-
}

cmd="${1:-status}"
case "$cmd" in

to-win)
    mapfile -t paths < <(find_star_paths)
    if [ "${#paths[@]}" -eq 0 ]; then
        echo "to-win: nothing to rename (no '*.' names under $ROOT)"
        exit 0
    fi
    : > "$MANIFEST"
    n=0
    for p in "${paths[@]}"; do
        d=$(dirname "$p"); b=$(basename "$p"); nb="_${b#\*}"   # '*.foo' -> '_.foo' (star swapped for underscore)
        mv "$p" "$d/$nb" || { echo "FAILED: $p" >&2; continue; }
        printf '%s\n' "${p#$ROOT/}" >> "$MANIFEST"
        n=$((n+1))
    done
    echo "to-win: renamed $n entries -> '_.', manifest: ${MANIFEST#$ROOT/}"
    echo "  Copy the tree to Windows NOW. On return: ./win-trip.sh to-linux"
    ;;

to-linux)
    if [ ! -f "$MANIFEST" ]; then
        echo "to-linux: no WIN-TRIP-MANIFEST.txt found at $ROOT" >&2
        echo "  Refusing to guess - some '_.' names are legitimate." >&2
        echo "  Restore the manifest from the pre-trip copy, or rename manually." >&2
        exit 1
    fi
    n=0
    # SHALLOWEST first: by the time we process any entry, its parent dirs
    # are already back in '*.' form, so the entry's '_.' twin sits directly
    # inside the restored parent - no full-path guessing needed.
    mapfile -t rels < <(tac "$MANIFEST")
    for rel in "${rels[@]}"; do
        [ -z "$rel" ] && continue
        cur="$ROOT/$rel"
        tdir="$(dirname "$cur")"
        b="$(basename "$rel")"
        winp="$tdir/_${b#\*}"              # '_.' twin beside the restored parent
        if [ ! -e "$winp" ]; then
            [ -e "$cur" ] && continue       # already restored
            echo "MISSING: $rel"
            continue
        fi
        mv "$winp" "$cur" && n=$((n+1))
    done
    rm -f "$MANIFEST"
    echo "to-linux: restored $n entries to '*.' names."
    # Windows round trips come back LOCKED (perms stripped/mangled by the
    # transfer). Blanket-open the whole subtree - direct user instruction,
    # matches how the house lives on Linux anyway (dirs 777).
    chmod -R 777 "$ROOT" 2>/dev/null
    echo "to-linux: chmod -R 777 applied (transfer locks everything)."
    ;;

status)
    stars=$(find "$ROOT" -depth -name '*\**' 2>/dev/null \
        | grep -vE '/pieces/sessions/|harness-reports|build-reports|_BACKUP|\.backup-' \
        | wc -l)
    if [ -f "$MANIFEST" ]; then
        mlines=$(wc -l < "$MANIFEST")
        echo "STATUS: tree is in WINDOWS shape ('$_.') - manifest present ($mlines entries)."
        echo "  You are ABOUT TO GO BACK to Linux? run: $(basename "$0") to-linux"
    elif [ "$stars" -gt 0 ]; then
        echo "STATUS: tree is in LINUX shape ('*.') - $stars star-named entries."
        echo "  About to ship to Windows? run: $(basename "$0") to-win"
    else
        echo "STATUS: neither '*' names nor manifest found under $ROOT - nothing to do."
    fi
    ;;

*)
    echo "usage: $(basename "$0") {to-win|to-linux|status} [root-dir]"
    exit 1
    ;;
esac
