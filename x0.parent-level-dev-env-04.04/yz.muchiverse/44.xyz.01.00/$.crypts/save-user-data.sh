#!/bin/bash
# save-user-data.sh - commit each desk user's data folder (xyzfs/users/<uuid>) to its own orphan data branch (Q007, USER-DATA-BRANCHES-DESIGN.md).
#
# Why: xyzfs/users is not tracked by the code branches any more, so this is the only git history of the owner's desk (entity histories, indexes, phones, wallets).
# How: plumbing only - a temporary index + --work-tree, so NOTHING is written into the working tree and nothing is checked out. A user's branch advances only
#      when its tree actually changed (idempotent: a second run commits nothing). Branch names: owner -> user/jb, user/<uuid8>, guests -> user/guest-<8>.
# Safety: never pushes. These branches hold wallet.txt and private chat history - keep them LOCAL. Never touches code branches or the current checkout.
# Usage: save-user-data.sh [-q]        (-q = quiet: only print when something was committed or on error)
# Run automatically by button.sh quit/reset (desktop already down, data quiet); `button.sh save-data` runs it by hand (live data may change mid-run: harmless, the next run catches up).
set -u
QUIET=0; [ "${1:-}" = "-q" ] && QUIET=1
HOUSE="$(cd "$(dirname "$0")/.." && pwd)"
REPO="$(git -C "$HOUSE" rev-parse --show-toplevel 2>/dev/null)" || { echo "save-user-data: not in a git repo" >&2; exit 1; }
GITDIR="$(cd "$HOUSE" && cd "$(git rev-parse --git-common-dir)" && pwd)"   # --git-common-dir is relative to the cwd, not the repo root; resolve it from there
[ -d "$GITDIR/objects" ] || { echo "save-user-data: cannot resolve the git dir ($GITDIR)" >&2; exit 1; }
U="$HOUSE/xyzfs/users"
OWNER_UUID="${SAVE_USER_DATA_OWNER:-0a9558a7-7c74-4358-833c-2d5b21edc421}"
[ -d "$U" ] || exit 0
g() { git --git-dir="$GITDIR" -c core.autocrlf=false "$@"; }
committed=0; failed=0; checked=0
for d in $(ls "$U"); do
    [ -d "$U/$d" ] || continue
    case "$d" in "$OWNER_UUID") br=user/jb ;; guest-*) br="user/guest-${d:6:8}" ;; *) br="user/${d:0:8}" ;; esac
    checked=$((checked+1))
    IDX="$(mktemp -u "${TMPDIR:-/tmp}/save-user-data.XXXXXX")"
    if ! ( cd "$U/$d" && GIT_INDEX_FILE="$IDX" g --work-tree="$U/$d" add -A -f . ) 2>/dev/null; then echo "save-user-data: add failed for $d" >&2; failed=$((failed+1)); rm -f "$IDX"; continue; fi
    tree="$(GIT_INDEX_FILE="$IDX" g write-tree 2>/dev/null)"; rm -f "$IDX"
    [ -n "$tree" ] || { echo "save-user-data: write-tree failed for $d" >&2; failed=$((failed+1)); continue; }
    tip="$(g rev-parse -q --verify "refs/heads/$br" 2>/dev/null || true)"
    if [ -n "$tip" ] && [ "$(g rev-parse "$tip^{tree}")" = "$tree" ]; then continue; fi   # unchanged
    parent=(); [ -n "$tip" ] && parent=(-p "$tip")
    msg="data: xyzfs/users/${d:0:8} $(date -u '+%Y-%m-%d %H:%M:%SZ')"
    c="$(printf '%s\n' "$msg" | g -c user.name=save-user-data -c user.email=save-user-data@localhost commit-tree "$tree" "${parent[@]}" 2>/dev/null)"
    [ -n "$c" ] || { echo "save-user-data: commit-tree failed for $d" >&2; failed=$((failed+1)); continue; }
    # compare-and-swap on the old tip: never clobbers a branch somebody else moved
    if g update-ref "refs/heads/$br" "$c" "${tip:-0000000000000000000000000000000000000000}" 2>/dev/null; then
        committed=$((committed+1)); [ "$QUIET" = 1 ] || echo "saved $br  $(g rev-parse --short "$c")"
    else echo "save-user-data: $br moved underneath us, skipped" >&2; failed=$((failed+1)); fi
done
[ "$QUIET" = 1 ] && [ "$committed" = 0 ] && [ "$failed" = 0 ] && exit 0
echo "save-user-data: $checked user folders, $committed branches advanced, $failed problems"
[ "$failed" = 0 ]
