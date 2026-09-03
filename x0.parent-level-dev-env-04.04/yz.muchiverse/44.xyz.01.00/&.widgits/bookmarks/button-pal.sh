#!/bin/sh
# button-pal.sh - PARALLEL launcher for the static-xhtpm bookmarks
# window (mirrors &.widgits/events-hq/button-pal.sh). The old bm_menu.sh
# + bookmarks.template.chtpm (class="bookmarks") stay untouched as
# rollback.
#
#   button-pal.sh <pal_dir> [house_root]
#
# Difference from bm_menu.sh:
#   - renders bookmarks-pal.xhtpm (class="bookmarks-pal", NOT "bookmarks")
#   - starts bookmarks_manager.+x itself (background) - the generic
#     <module> launcher can't pass it the pal dir as argv
#   - the xhtpm's one <module> is the projector; it reads KHTPM_ARG3
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
XHTPM="$HERE/bookmarks-pal.xhtpm"

PAL_DIR="${1:?usage: button-pal.sh <pal_dir> [house_root]}"
mkdir -p "$PAL_DIR"
PAL_DIR="$(cd "$PAL_DIR" && pwd)"
H="${2:-}"
if [ -z "$H" ] || [ ! -d "$H" ]; then H="$(cd "$HERE/../.." && pwd)"; fi

RENDER_OPS="$H/*.monads/*.livedesk-taskbar/ops"
BIN="$RENDER_OPS/+x/khtpm_core_render.+x"
PRISC="$H/&.widgits/_shared-lib/system/+x/prisc+x.+x"
MGR="$RENDER_OPS/+x/bookmarks_manager.+x"

[ -x "$BIN" ]   || (cd "$RENDER_OPS" && sh build_core_render.sh) || true
[ -x "$PRISC" ] || sh "$H/&.widgits/_shared-lib/ops/build_prisc.sh" || true
[ -x "$MGR" ]   || (cd "$RENDER_OPS" && sh build_bookmarks_manager.sh) || true
for f in "$BIN" "$PRISC" "$MGR" "$XHTPM"; do
    [ -e "$f" ] || { echo "bookmarks-pal: missing $f" >&2; exit 1; }
done
mkdir -p "$PAL_DIR/.hq_manager"

# Replace only an instance already open on THIS pal dir - byte-compare
# /proc/<pid>/cmdline (house paths carry emoji/parens; no regex).
same_instance_pids() {
    for pid in $(pgrep -f 'khtpm_core_render\.\+x|bookmarks_manager\.\+x|bookmarks_projector\.pal' 2>/dev/null || true); do
        if [ -r "/proc/$pid/cmdline" ]; then
            if tr '\0' '\n' < "/proc/$pid/cmdline" 2>/dev/null | grep -qxF "$PAL_DIR"; then
                echo "$pid"
            fi
        fi
    done
}

pids="$(same_instance_pids)"
if [ -n "$pids" ]; then
    echo "bookmarks-pal: replacing instance on $PAL_DIR: $(echo $pids | tr '\n' ' ')"
    echo "$pids" | xargs -r kill -TERM
    sleep 1
    pids="$(same_instance_pids)"
    [ -n "$pids" ] && { echo "$pids" | xargs -r kill -KILL; sleep 1; }
fi

# 1. the real manager (bookmarks.pdl -> bookmarks_state.txt), same 2
#    args bm_menu.sh's own launch uses.
setsid nohup "$MGR" "$H" "$PAL_DIR" \
    >/tmp/bookmarks-pal-mgr.log 2>&1 < /dev/null &
disown 2>/dev/null || true

# 2. the shared renderer on the static template. argv[3]=$PAL_DIR is a
#    directory -> the renderer's instance-dir hook.
setsid nohup "$BIN" "$H" "$XHTPM" "$PAL_DIR" \
    >/tmp/bookmarks-pal.log 2>&1 < /dev/null &
disown 2>/dev/null || true
sleep 1
echo "bookmarks-pal launched for $PAL_DIR (log=/tmp/bookmarks-pal.log)"
