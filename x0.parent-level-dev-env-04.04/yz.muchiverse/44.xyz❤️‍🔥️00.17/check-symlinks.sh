#!/bin/bash
# check-symlinks.sh - audit any directory tree (house, a backup folder,
# whatever) for real symlinks. Two checks, since a project can look clean
# on one and still be dirty on the other (see sim-smell-fix.md's own
# "CORRECTION" writeup, 2026-08-20):
#
#   1. On-disk symlinks that exist RIGHT NOW (`find -type l`). Live session
#      dirs (pieces/sessions/*) are excluded by default since those are
#      ephemeral by design, recreated fresh every launch - not a
#      Windows-portability concern on their own. Pass --include-sessions to
#      see them anyway (useful for a backup folder, where "session" dirs
#      might be frozen leftovers, not live/regenerable).
#   2. button.sh files that still contain literal `ln -s`/`ln -sf`/`ln -sfn`
#      calls - these create real symlinks every time the project is RUN,
#      even if none happen to be sitting on disk at audit time.
#
# Lives at the house root so it can be run from there with no args to
# audit the whole house, or pointed at any other folder (e.g. a backup).
#
# USAGE:
#   ./check-symlinks.sh                         (default DIR = this script's own dir, i.e. house root)
#   ./check-symlinks.sh [DIR]
#   ./check-symlinks.sh [DIR] --include-sessions

set -u
(set -o pipefail) 2>/dev/null && set -o pipefail   # bash/ksh only; dash treats an unknown `set -o`
                                                    # arg as fatal even under `||`, so test in a
                                                    # subshell first rather than risk killing the script

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

TARGET="$SCRIPT_DIR"
INCLUDE_SESSIONS=0
for arg in "$@"; do
    if [ "$arg" = "--include-sessions" ]; then
        INCLUDE_SESSIONS=1
    else
        TARGET="$arg"
    fi
done

if [ ! -d "$TARGET" ]; then
    echo "not a directory: $TARGET" >&2
    exit 1
fi
TARGET="$(cd "$TARGET" && pwd)"

echo "=== Auditing: $TARGET ==="
echo ""
echo "--- 1. Real symlinks on disk right now ---"
if [ "$INCLUDE_SESSIONS" -eq 1 ]; then
    LINKS="$(find "$TARGET" -type l 2>/dev/null)"
else
    LINKS="$(find "$TARGET" -type l 2>/dev/null | grep -v '/pieces/sessions/')"
fi
if [ -z "$LINKS" ]; then
    echo "(none found)"
else
    count=0
    while IFS= read -r l; do
        [ -z "$l" ] && continue
        count=$((count + 1))
        printf '  %s\n    -> %s\n' "${l#$TARGET/}" "$(readlink "$l")"
    done << LINKSEOF
$LINKS
LINKSEOF
    echo ""
    echo "  TOTAL: $count real symlink(s) on disk"
fi
[ "$INCLUDE_SESSIONS" -eq 0 ] && echo "  (pieces/sessions/* excluded - pass --include-sessions to check those too)"

echo ""
echo "--- 2. button.sh files that still CREATE symlinks at runtime ---"
FOUND_ANY=0
while IFS= read -r f; do
    [ -z "$f" ] && continue
    n=$(grep -c '\bln -s' "$f" 2>/dev/null)
    n=${n:-0}
    if [ "$n" -gt 0 ]; then
        # distinguish real calls from mere comments mentioning ln -s
        real=$(grep -E '^\s*ln -s' "$f" | grep -v '^\s*#' | wc -l)
        if [ "$real" -gt 0 ]; then
            FOUND_ANY=1
            printf '  UNMIGRATED (%s real ln -s call(s)): %s\n' "$real" "${f#$TARGET/}"
        fi
    fi
done << BSHEOF
$(find "$TARGET" -iname "button.sh" 2>/dev/null | grep -v '/pieces/sessions/' | sort)
BSHEOF
if [ "$FOUND_ANY" -eq 0 ]; then
    echo "(none found - every button.sh in this tree is symlink-free)"
fi

echo ""
echo "=== done ==="
