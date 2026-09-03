#!/bin/sh
# evhq_action.sh - the ONLY write path from events-hq.xhtpm clicks.
# It never compiles IR: it writes a selection/request file that
# khtpm_events_hq_manager.+x already polls (action.txt), or a small UI
# state file the projector reads. Compile stays in the manager.
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
    # 0=Scripting 1=Scratch 2=Blueprints (view-mode content swap is a follow-up)
    printf '%s\n' "$ARG" > "$MGR_DIR/view.txt"
    ;;
  page)
    # ARG = a page-dir name (page_1, ...). The manager reads selected_page.txt.
    printf '%s\n' "$ARG" > "$MGR_DIR/selected_page.txt"
    ;;
  picker)
    case "$ARG" in
      open)  printf '1\n' > "$MGR_DIR/picker.txt" ;;
      close) printf '0\n' > "$MGR_DIR/picker.txt" ;;
      *)     echo "evhq_action: picker needs open|close" >&2; exit 1 ;;
    esac
    ;;
  pick)
    # ARG = a registry command type. Append it with empty params; the
    # manager fills defaults + recompiles. Then close the picker.
    printf 'append:%s|\n' "$ARG" > "$MGR_DIR/action.txt"
    printf '0\n' > "$MGR_DIR/picker.txt"
    ;;
  play)
    # The manager owns play_event.sh invocation (it resolves the entity
    # dir = parent of event_pkg). Just hand it the request.
    printf 'play\n' > "$MGR_DIR/action.txt"
    ;;
  edit)
    # per-command field editor is a follow-up; record the intent.
    printf 'edit %s\n' "$ARG" > "$MGR_DIR/pending_ui_action.txt"
    ;;
  *)
    echo "evhq_action: unknown verb '$VERB'" >&2
    exit 1
    ;;
esac
