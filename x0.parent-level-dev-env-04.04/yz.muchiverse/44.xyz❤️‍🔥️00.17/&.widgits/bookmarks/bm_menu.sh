#!/bin/sh
# bm_menu.sh - Bookmarks manager for cursword (and any pal).
# 2026-08-24, direct user goals:
#   1. A Bookmarks button whose entries live as real .pdl rows (same
#      pipe-delimited convention as every other registry in this house),
#      seeded with #.ref/menu (event.commands.remaining.txt's home).
#   2. A "New+" affordance where a path can be pasted and APPENDED to
#      that .pdl.
#   3. Entries are NAV-able and CLICKABLE rows in a db-hq-style window,
#      and New+ takes its path from a NATIVE in-window input field
#      (<cli-io> style: Enter arms, type, Enter commits - direct user
#      instruction "new+ should allow input from <cli-io>"; zenity is
#      GONE). Renderer side = khtpm_hq_render.c's generic input:
#      mechanism (same session): commit appends the line to
#      <pal>/.bm_newplus.txt then fires `consumed-newplus` detached;
#      this script reads/truncates that file and upserts.
#
# USER PREFERENCE CORRECTION (2026-08-24, after a live look at both):
# the window must stay the ORIGINAL db-hq-style khtpm_hq_render window
# ("the old window - it was fine"), NOT the small entity-menu context
# popup this briefly migrated to. Keyboard nav comes from
# kptm_hq_render.c's own GENERIC pass added same day: every element
# with an onClick= becomes a numbered [>]N. row (Up/Down, digit-jump,
# Enter) - same list UX as open-hai's chat-session rows. Drag-drop of
# directories is deliberately OUT OF SCOPE here per user choice; the
# renderer-side XDND drop_action= capability lives on in the MERGED
# entity-menu binary (!.HOUSE_STDS.md K.3-3) for any future consumer.
#
# Layout flow mirrors chat_button.sh / open_stats_hq.sh's own proven
# shape: meta.pdl METHOD row -> shared script under &.widgits/<tool>/ ->
# pal-local state dirs -> house-standard renderer binary.
#
# Canonical store: <pal>/bookmarks.pdl
#   SECTION      | KEY                | VALUE
#   ----------------------------------------
#   BOOKMARK     | events-commands    | <abs path>
# Window:       <pal>/bookmarks.chtpm regenerated fresh from the .pdl on
#               every launch AND every New+ add - khtpm_hq_render.c's own
#               mtime-gated live reload (same-day generic mechanism)
#               swaps the new list into the running window, no respawn.
# NO symlink mirror: an earlier revision mirrored rows as
#               <pal>/bookmarks/<safe_name> symlinks - REMOVED per direct
#               user rule "symlinks are disallowed for windows". Rows open
#               their REAL target dirs directly, exactly like the entity
#               menus' own Dir button (METHOD | Dir | xdg-open + $0).
#
# Usage:
#   bm_menu.sh <house_root> <pal_dir>            launch/refresh the window
#   bm_menu.sh add <pal> <name> <path>           upsert one BOOKMARK row
#   bm_menu.sh list <pal>                        print rows (name<TAB>path)
#   bm_menu.sh compose <pal> [<house>]           regenerate chtpm
#   bm_menu.sh consumed-newplus <house_root> <pal_dir>
#                                                fired DETACHED by the
#                                                window's own input field
#                                                commit (renderer generic
#                                                input: mechanism): reads
#                                                + truncates the typed
#                                                line, then recompose;
#                                                live reload shows it
#
# No GUI toolkit dependency anymore (zenity GONE per direct instruction
# "new+ should allow input from <cli-io>" - the window itself is the
# entry field); the launcher keeps GDK_BACKEND=x11 like every other X11
# window spawn on this Wayland+Xwayland desktop.
# Everything else is pure file ops, which is what tests exercise.

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

safe_name() {  # filesystem-safe name (kept for potential future non-link use)
    printf '%s' "$1" | tr ' /:' '___' | tr -cd 'A-Za-z0-9._+-'
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

# compose_bookmarks <pal> [<house_root>]
# Regenerates bookmarks.chtpm + bookmarks.css from the .pdl - the layout
# is always a fresh compiled artifact of the store, never hand-maintained
# (same visual-compiler rule event-ez follows for event.pal). Vocabulary
# = db-hq/stats-hq's own real HQML subset (database-window class); item
# and button elements carry onClick open:/exec:, which the renderer now
# numbers into its nav list automatically.
compose_bookmarks() {
    _pal="$1"; _house="${2:-}"
    _self_dir=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
    _self="$_self_dir/$(basename "$0")"
    # House root defaults to climbing from THIS script's own fixed
    # location (<house>/&.widgits/bookmarks/) - the safe derivation
    # direction per !.HOUSE_STDS.md #20; a caller-supplied house wins.
    [ -n "$_house" ] || _house=$(CDPATH= cd -- "$_self_dir/../../.." 2>/dev/null && pwd) || _house=""
    {
        printf '<window class="database-window">\n'
        printf '  <panel class="settings-block">\n'
        printf '    <title class="block-title" label="cursword bookmarks"/>\n'
        printf '    <text class="bm-hint" label="Click or Enter a bookmark to open it."/>\n'
        pdl_rows "$_pal" | while IFS="$TAB" read -r n p; do
            # fo-menu-sys.md pitfall #26 ("Button vs Text"): interactive
            # list entries are <button> elements - the renderer's own
            # natively laid-out/numbered/clickable vocabulary - NOT a
            # custom tag leaning on extra machinery. The manager (this
            # script) projects the PDL into that dialect. bm-bookmark =
            # the user-requested look: BLACK text on YELLOW highlight,
            # instantly readable as "this is a bookmark".
            printf '    <button class="bm-bookmark" label="%s  -  %s" onClick="open:%s"/>\n' "$n" "$p" "$p"
        done
        printf '    <button class="bm-btn" label="+ New+ (Enter, type a path, Enter)" onClick="input:%s/.bm_newplus.txt|%s consumed-newplus %s %s"/>\n' \
            "$_pal" "$_self" "$_house" "$_pal"
        printf '    <button class="bm-btn" label="Open Pal Folder" onClick="open:%s"/>\n' "$_pal"
        printf '  </panel>\n'
        printf '</window>\n'
    } > "$_pal/bookmarks.chtpm"

    # CSS cribbed from stats-hq/dashboard.css's load-bearing subset -
    # no new vocabulary invented (see that file's own header note).
    cat > "$_pal/bookmarks.css" << 'EOF'
/* bookmarks.css - generated by bm_menu.sh; reuses db-hq/stats-hq's own
   load-bearing subset exactly. */
window {
  background-color: #ececec;
  font-family: Ubuntu;
  font-size: 10px;
}
.settings-block {
  background-color: #fafafa;
  border: 1px solid #999999;
  border-width: 1;
}
.block-title {
  background-color: #ffffff;
  color: #444444;
  font-weight: bold;
  font-size: 9px;
  position: absolute;
  top: -8px;
  left: 10px;
}
.bm-hint {
  color: #888888;
  font-size: 9px;
}
.bm-btn {
  color: #222222;
  font-weight: bold;
}
.bm-bookmark {
  background-color: #ffd700;
  color: #000000;
  font-weight: bold;
}
EOF
}

case "${1:-}" in
    add)     do_add "$2" "$3" "$4"; exit $? ;;
    list)    pdl_rows "$2"; exit 0 ;;
    compose) compose_bookmarks "$2" "${3:-}"; exit 0 ;;
    consumed-newplus)
        # bm_menu.sh consumed-newplus <house> <pal>  (fired DETACHED by
        # the renderer after an input: commit appended the typed line to
        # .bm_newplus.txt - see the generic input: mechanism in
        # khtpm_hq_render.c). Read + truncate, upsert, recompose; live
        # reload shows it. Name auto-derives from the path's basename
        # (same habit as every other row source); bad paths are
        # rejected + logged by do_add as usual.
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
            compose_bookmarks "$PAL" "$HOUSE"
        else
            log "$PAL" "New+ input REJECTED (bad path or name): $NEWPATH"
        fi
        exit 0
        ;;
esac

HOUSE="$1"; PAL="$2"
export GDK_BACKEND=x11

compose_bookmarks "$PAL" "$HOUSE"

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

BIN="$HOUSE/*.monads/*.livedesk-taskbar/ops/+x/khtpm_hq_render.+x"
if [ ! -x "$BIN" ]; then
    (cd "$HOUSE/*.monads/*.livedesk-taskbar/ops" && sh build_db_hq.sh) || true
fi
if [ ! -x "$BIN" ]; then
    echo "bm_menu: build failed, missing $BIN" >&2
    exit 1
fi

log "$PAL" "bookmarks window launched (db-hq style)"
exec "$BIN" "$HOUSE" "$SELF_CHTPM"
