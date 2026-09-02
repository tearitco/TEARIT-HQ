#!/bin/bash
# symlink-swap.sh - house-wide mechanical `ln -s`/`ln -sf`/`ln -sfn` -> `cp -r`
# swap for button.sh session-dir setup blocks. See ../sim-smell-fix.md ("THE
# SOLUTION" section) for the full writeup of why this is the fix.
#
# WHAT THIS DOES (safe, mechanical, reversible):
#   - Rewrites every `ln -s`/`ln -sf`/`ln -sfn` call in a button.sh to the
#     equivalent `cp -r` call (cp -r works fine on both plain files and
#     directories, so one substitution covers every case in the house).
#   - Backs up the original button.sh to button.sh.pre-symlink-swap first.
#
# WHAT THIS DOES NOT DO (needs a human/agent pass afterward, per project):
#   - Decide which of the newly-copied files are REAL PERSISTENT STATE
#     (config.txt, data/, save files - written during a session, must
#     survive after the session dir is deleted) vs static/read-only code
#     that's safe to just re-copy every launch. Persistent files need a
#     `persist_session_state()` function added to the EXIT trap that copies
#     them back to $SCRIPT_DIR before `rm -rf "$SESSION_DIR"` runs - see
#     my-chara-txt's or piececraft-xyz's own button.sh for the working
#     pattern. Skipping this step means real save data silently vanishes
#     every time a session ends - the exact bug this whole doc is about.
#   - Fix any project-specific "live bidirectional channel" bugs (a file
#     read/written by ANOTHER process directly against the REAL, non-session
#     root - e.g. piececraft-xyz's widget_cmds/inbox.txt talking to
#     board-viewer). Those need a `resolve_real_root()`-style fix in the
#     project's own C code, not just a button.sh change - grep each
#     project's own comments for "REAL (non-session)" / "real_project_root"
#     to find candidates.
#   - Rebuild or test anything. Run each project's own build script and get
#     user signoff per the established protocol before trusting it.
#
# USAGE:
#   ./symlink-swap.sh <button.sh> [<button.sh> ...]
#   ./symlink-swap.sh --list-file /path/to/list.txt   # one path per line
#
# Dry run first: pass --dry-run before the file args to just show the diff
# without writing anything.

set -eu
(set -o pipefail) 2>/dev/null && set -o pipefail   # bash/ksh only; dash treats an unknown `set -o`
                                                    # arg as fatal even under `||`, so test in a
                                                    # subshell first rather than risk killing the script

# No bash arrays here on purpose (dash/POSIX sh has none) - both input
# forms (bare args, --list-file) get funneled into one plain newline-
# delimited temp file and processed with a single `while read` loop.

DRY_RUN=0
if [ "${1:-}" = "--dry-run" ]; then
    DRY_RUN=1
    shift
fi

FILELIST="$(mktemp)"
trap 'rm -f "$FILELIST"' EXIT INT TERM

if [ "${1:-}" = "--list-file" ]; then
    LISTFILE="$2"
    cp "$LISTFILE" "$FILELIST"
else
    for f in "$@"; do
        printf '%s\n' "$f" >> "$FILELIST"
    done
fi

if [ ! -s "$FILELIST" ]; then
    echo "usage: $0 [--dry-run] <button.sh> [<button.sh> ...]" >&2
    echo "       $0 [--dry-run] --list-file <path>" >&2
    exit 1
fi

while IFS= read -r f; do
    [ -n "$f" ] || continue
    if [ ! -f "$f" ]; then
        echo "SKIP (not found): $f" >&2
        continue
    fi
    before=$(grep -c '\bln -s' "$f" || true)
    if [ "$before" -eq 0 ]; then
        echo "clean already: $f"
        continue
    fi

    tmp="$(mktemp)"
    sed -E \
        -e 's/\bln -sfn /cp -r /g' \
        -e 's/\bln -sf /cp -r /g' \
        -e 's/\bln -s /cp -r /g' \
        "$f" > "$tmp"

    after=$(grep -c '\bln -s' "$tmp" || true)

    if [ "$DRY_RUN" -eq 1 ]; then
        echo "=== DRY RUN: $f ($before -> $after ln -s remaining) ==="
        diff -u "$f" "$tmp" || true
        rm -f "$tmp"
        continue
    fi

    cp "$f" "$f.pre-symlink-swap"
    mv "$tmp" "$f"
    echo "swapped: $f ($before ln-s -> cp-r, backup: $f.pre-symlink-swap, $after ln-s remaining)"
    if [ "$after" -gt 0 ]; then
        echo "  ! $after ln -s line(s) left unconverted in $f - inspect by hand (unusual pattern, didn't match the three known forms)"
    fi
done < "$FILELIST"
