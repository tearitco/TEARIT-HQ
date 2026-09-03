#!/bin/sh
# stats_action.sh - the only write path for stats-hq-pal. Called by the
# renderer from the static template's action= strings:
#
#   <item action="'.../stats_action.sh' 'sel' '<n>' '<pkg>'"/>
#        -> argv:  sel <n> <pkg>
#
# It maintains <pkg>/state/active.pdl (KEY | value lines); the PAL
# projector (pal/stats_projector.pal) reads that every tick and rebuilds
# the panel for session <n>. No business logic here - the session scan
# stays entirely in the unchanged stats_hq_manager.c.
set -u
VERB="${1:-}"

case "$VERB" in
    sel)
        N="${2:-0}"
        PKG="${3:-}"
        [ -n "$PKG" ] || { echo "stats_action: sel needs pkg" >&2; exit 1; }
        mkdir -p "$PKG/state"
        p="$PKG/state/active.pdl"
        {
            printf 'SECTION | KEY | VALUE\n'
            printf 'SEL     | sel | %s\n' "$N"
        } > "$p.tmp" && mv "$p.tmp" "$p"
        ;;
    *)
        echo "stats_action: unknown verb '$VERB'" >&2
        exit 1
        ;;
esac
