#!/bin/sh
# bm_menu.sh - Bookmarks manager for cursword (and any pal).
#
# 2026-08-25, real TPMOS-compliant rebuild (au11-hq/TPMOS-COMPLIANCE-
# DEBT.md's own standing rule, added the same day: don't patch around a
# missing manager with renderer-side workarounds - build the real thing,
# same shape as its own proven sibling). This script used to `printf`
# raw .chtpm XML directly (compose_bookmarks()) and regenerate the whole
# window file on every launch AND every New+ add - the exact anti-
# pattern that doc exists to stop. That's gone. The real split now:
#   - THIS script: file ops only (bookmarks.pdl read/write, one-time
#     per-pal provisioning of the static chtpm/css). Never touches XML.
#   - bookmarks_manager.c (real, compiled, independently-testable binary,
#     `<module src="..."/>` in the chtpm, launched by the renderer):
#     owns bookmarks.pdl -> bookmarks_state.txt publishing.
#   - khtpm_core_render.c: reads bookmarks_state.txt, injects
#     real <button> rows at runtime (dbhq_inject_bookmark_items()).
#
# Direct user goals this still serves (2026-08-24, unchanged):
#   1. A Bookmarks button whose entries live as real .pdl rows (same
#      pipe-delimited convention as every other registry in this house),
#      seeded with #.ref/menu.
#   2. A "New+" affordance where a path can be pasted and APPENDED to
#      that .pdl, via a NATIVE in-window input field (<cli-io> style:
#      Enter arms, type, Enter commits - zenity is GONE).
#   3. Entries are NAV-able and CLICKABLE rows in a db-hq-style window.
# Drag-drop of directories is deliberately OUT OF SCOPE (user choice).
#
# Canonical store: <pal>/bookmarks.pdl
#   SECTION      | KEY                | VALUE
#   ----------------------------------------
#   BOOKMARK     | events-commands    | <abs path>
# Window: <pal>/bookmarks.chtpm + bookmarks.css, provisioned ONCE per pal
#         (copied+token-substituted from bookmarks.template.chtpm/.css in
#         this dir) - never regenerated after that. Real bookmark ROWS
#         are injected at runtime by the renderer from bookmarks_manager.c's
#         own published state file, not baked into the chtpm.
# NO symlink mirror - rows open their REAL target dirs directly via
#         open:<path>, same as the entity menus' own Dir button.
#
# Usage:
#   bm_menu.sh <house_root> <pal_dir>            launch/refresh the window
#   bm_menu.sh add <pal> <name> <path>           upsert one BOOKMARK row
#   bm_menu.sh list <pal>                        print rows (name<TAB>path)
#   bm_menu.sh consumed-newplus <house_root> <pal_dir>
#                                                fired DETACHED by the
#                                                window's own input field
#                                                commit: reads + truncates
#                                                the typed line, upserts
#                                                into bookmarks.pdl. The
#                                                manager (already running,
#                                                launched by the window's
#                                                own <module> tag) notices
#                                                the .pdl's mtime change on
#                                                its own next poll tick and
#                                                republishes - no recompose,
#                                                no relaunch needed.
#
# No GUI toolkit dependency (zenity GONE); GDK_BACKEND=x11 like every
# other X11 window spawn on this Wayland+Xwayland desktop.

set -eu

log() {  # log <pal> <message...>
    _pal="$1"; shift
    mkdir -p "$_pal/audit"
    printf '%s %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$*" >> "$_pal/audit/bookmarks.log"
}

TAB="$(printf '\t')"

pdl_rows() {  # pdl_rows <pal> -> "name<TAB>path" lines, in file order
    [ -f "$1/bookmarks.pdl" ] || return 0
    grep '^BOOKMARK' "$1/bookmarks.pdl" | while IFS='|' read -r _ name path; do
        name=$(printf '%s' "$name" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
        path=$(printf '%s' "$path" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
        [ -n "$name" ] && [ -n "$path" ] && printf '%s\t%s\n' "$name" "$path"
    done
}

do_add() {  # do_add <pal> <name> <path> - upsert (replace existing name)
    _pal="$1"; _name="$2"; _path="$3"
    case "$_name$_path" in
        *"'"*) echo "single quotes not allowed in bookmark name/path (breaks onClick quoting): $_name$_path" >&2; return 1 ;;
    esac
    mkdir -p "$_pal"
    _real=$(realpath -m -- "$_path")
    if [ ! -e "$_real" ]; then
        echo "no such path: $_real" >&2
        return 1
    fi
    _tmp=$(mktemp "$_pal/bm.XXXXXX")
    if [ -f "$_pal/bookmarks.pdl" ]; then
        awk -F'|' -v n="$_name" '
            /^BOOKMARK/ {
                k = $2
                gsub(/^[ \t]+|[ \t]+$/, "", k)
                if (k == n) next
            }
            { print }
        ' "$_pal/bookmarks.pdl" > "$_tmp"
    else
        printf 'SECTION      | KEY                | VALUE\n' > "$_tmp"
        printf -- '----------------------------------------\n' >> "$_tmp"
    fi
    printf 'BOOKMARK     | %-18s | %s\n' "$_name" "$_real" >> "$_tmp"
    mv "$_tmp" "$_pal/bookmarks.pdl"
}

# provision_bookmarks <pal> <house> - copies the static chtpm/css
# templates into the pal ONLY if they don't already exist there (a
# one-time per-pal scaffold, not a per-launch regeneration - the real
# distinction TPMOS-COMPLIANCE-DEBT.md's own anti-pattern write-up draws).
# The only thing that ever varies between pals is baked in via a plain
# token substitution at provision time, not re-derived every launch.
provision_bookmarks() {
    _pal="$1"; _house="$2"
    _self_dir=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
    _self="$_self_dir/$(basename "$0")"
    if [ ! -f "$_pal/bookmarks.chtpm" ]; then
        # REAL FIX 2026-08-25 (found live: New+ silently did nothing) -
        # this house's own paths contain a literal '&' (&.widgits) and
        # '#' (#.desktop et al) - both are sed-special in a replacement
        # string ('&' re-inserts the matched text, '#' would end the
        # s### expression early if used unescaped as a delimiter). Every
        # substituted value needs backslash+& escaped before use.
        _pal_esc=$(printf '%s' "$_pal" | sed 's/[&\]/\\&/g')
        _house_esc=$(printf '%s' "$_house" | sed 's/[&\]/\\&/g')
        _self_esc=$(printf '%s' "$_self" | sed 's/[&\]/\\&/g')
        sed \
            -e "s#__PAL__#$_pal_esc#g" \
            -e "s#__HOUSE__#$_house_esc#g" \
            -e "s#__SELF__#$_self_esc#g" \
            "$_self_dir/bookmarks.template.chtpm" > "$_pal/bookmarks.chtpm"
    fi
    if [ ! -f "$_pal/bookmarks.css" ]; then
        cp "$_self_dir/bookmarks.template.css" "$_pal/bookmarks.css"
    fi
}

case "${1:-}" in
    add)     do_add "$2" "$3" "$4"; exit $? ;;
    list)    pdl_rows "$2"; exit 0 ;;
    consumed-newplus)
        # bm_menu.sh consumed-newplus <house> <pal>  (fired DETACHED by
        # the renderer after an input: commit appended the typed line to
        # .bm_newplus.txt). Read + truncate, upsert into bookmarks.pdl -
        # bookmarks_manager.c (already running for this pal's window)
        # notices the .pdl's mtime change on its own next poll tick and
        # republishes bookmarks_state.txt; the renderer picks that up on
        # its own next tick. No recompose, no relaunch, real live update.
        HOUSE="$2"; PAL="$3"
        F="$PAL/.bm_newplus.txt"
        [ -f "$F" ] || exit 0
        NEWPATH=$(tail -n 1 "$F")
        : > "$F"
        [ -z "$NEWPATH" ] && exit 0
        case "$NEWPATH" in
            "~"*) NEWPATH="$HOME${NEWPATH#~}" ;;
        esac
        BMNAME=$(basename "$(realpath -m -- "$NEWPATH")")
        if do_add "$PAL" "$BMNAME" "$NEWPATH"; then
            log "$PAL" "bookmark added via New+ input: $BMNAME -> $(realpath -m -- "$NEWPATH")"
        else
            log "$PAL" "New+ input REJECTED (bad path or name): $NEWPATH"
        fi
        exit 0
        ;;
esac

HOUSE="$1"; PAL="$2"
export GDK_BACKEND=x11

provision_bookmarks "$PAL" "$HOUSE"

# Single instance per pal: kill any previous renderer of THIS chtpm, then
# relaunch (same escalation-free TERM-first habit chat_button.sh uses;
# scoped match on the exact chtpm path so OTHER hq windows are safe).
SELF_CHTPM="$PAL/bookmarks.chtpm"
for p in /proc/[0-9]*; do
    _cl=$(cat "$p/cmdline" 2>/dev/null | tr '\0' ' ') || continue
    case "$_cl" in
        *"$SELF_CHTPM"*) kill "${p#/proc/}" 2>/dev/null || true ;;
    esac
done
sleep 0.2

BIN="$HOUSE/*.monads/*.livedesk-taskbar/ops/+x/khtpm_core_render.+x"
if [ ! -x "$BIN" ]; then
    (cd "$HOUSE/*.monads/*.livedesk-taskbar/ops" && sh build_core_render.sh) || true
fi
if [ ! -x "$BIN" ]; then
    echo "bm_menu: build failed, missing $BIN" >&2
    exit 1
fi
MGRBIN="$HOUSE/*.monads/*.livedesk-taskbar/ops/+x/bookmarks_manager.+x"
if [ ! -x "$MGRBIN" ]; then
    (cd "$HOUSE/*.monads/*.livedesk-taskbar/ops" && sh build_bookmarks_manager.sh) || true
fi

log "$PAL" "bookmarks window launched (real manager, TPMOS-compliant)"
exec "$BIN" "$HOUSE" "$SELF_CHTPM"
