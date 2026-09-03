#!/bin/sh
# evhq_action.sh - the ONLY write path from events-hq.xhtpm clicks.
# It never compiles IR: it just writes a request/selection file that
# khtpm_events_hq_manager.+x already polls. Compile stays in the manager.
#
# Invoked by the renderer's dispatch() as:
#   evhq_action.sh <verb> <arg> <event_pkg_dir> <xhtpm_pkg_dir> <house_root>
# (the template passes verb/arg/${ARG3}; the renderer appends
#  package_dir + house_root).
set -e
VERB="${1:-}"
ARG="${2:-}"
PKG_DIR="${3:-}"
[ -n "$PKG_DIR" ] && [ -d "$PKG_DIR" ] || { echo "evhq_action: bad event_pkg dir '$PKG_DIR'" >&2; exit 1; }
MGR_DIR="$PKG_DIR/.hq_manager"
mkdir -p "$MGR_DIR"

case "$VERB" in
  view)
    # 0=Scripting 1=Scratch 2=Blueprints. Projector reads this in phase 2.
    printf '%s\n' "$ARG" > "$MGR_DIR/view.txt"
    ;;
  page)
    # ARG = a page-dir name (page_1, ...). The manager reads selected_page.txt.
    printf '%s\n' "$ARG" > "$MGR_DIR/selected_page.txt"
    ;;
  edit|picker|play)
    # phase 2 - the picker overlay + Play wiring. Recorded, not acted on.
    printf '%s %s\n' "$VERB" "$ARG" > "$MGR_DIR/pending_ui_action.txt"
    ;;
  *)
    echo "evhq_action: unknown verb '$VERB'" >&2
    exit 1
    ;;
esac
