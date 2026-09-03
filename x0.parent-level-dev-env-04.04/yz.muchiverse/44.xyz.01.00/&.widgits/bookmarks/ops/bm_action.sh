#!/bin/sh
# bm_action.sh - the only UI write path for bookmarks-pal.xhtpm.
#
# Called by the renderer's generic <cli_io> commit for the New+ row:
#   <cli_io onClick="'<pkg>/ops/bm_action.sh' 'newplus' '<pal_dir>'"/>
# default_cli_io_run_action() then appends  '<pkg>' '<house>' '<typed>'
# so argv is:  newplus <pal_dir> <pkg> <house> <typed_path>
#
# It upserts the BOOKMARK row via the EXISTING bm_menu.sh (file ops
# only, never touches XML); bookmarks_manager.c notices bookmarks.pdl's
# mtime change on its next poll tick and republishes bookmarks_state.txt,
# which pal/bookmarks_projector.pal then reprojects. No business logic
# is duplicated here.
set -u
VERB="${1:-}"

case "$VERB" in
    newplus)
        PAL="${2:-}"
        TYPED="${5:-}"
        [ -n "$PAL" ] || { echo "bm_action: newplus needs pal_dir" >&2; exit 1; }
        [ -n "$TYPED" ] || exit 0
        case "$TYPED" in
            "~"*) TYPED="$HOME${TYPED#~}" ;;
        esac
        BM_MENU="$(cd "$(dirname "$0")/.." && pwd)/bm_menu.sh"
        [ -f "$BM_MENU" ] || { echo "bm_action: missing $BM_MENU" >&2; exit 1; }
        NAME="$(basename "$(realpath -m -- "$TYPED")")"
        sh "$BM_MENU" add "$PAL" "$NAME" "$TYPED"
        ;;
    *)
        echo "bm_action: unknown verb '$VERB'" >&2
        exit 1
        ;;
esac
